#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "ClientPlayer.h"
#include "DialogPresentationHost.h"
#include "MGameStringTable.h"
#include "MInventory.h"
#include "MMoneyManager.h"
#include "MTradeManager.h"
#include "PlayerStateHost.h"
#include "TempInformation.h"
#include "TradePresentationHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Cpackets/CGTradeAddItem.h"
#include "Cpackets/CGTradeFinish.h"
#include "Cpackets/CGTradeRemoveItem.h"
#include "Gpackets/GCTradeError.h"
#include "Gpackets/GCTradeVerify.h"
#include <algorithm>
#include <string>
#include <vector>

// A trade error ends a trade request the player was waiting on, says why
// in a message box (closing the "cancel?" box for a refusal), gives the
// player's trade money back, and closes the exchange window. A trade
// verification confirms what the player did in the trade box: an item
// added or taken out (marked for the trade or not), picked up, put down,
// or stacked onto another, money added or taken out, the trade refused or
// reconsidered. Each takes back both sides' acceptance; an item that is
// not part of the trade asks the other side to reconsider, and one that
// is tells the other side what changed.
namespace {

std::vector<std::string> calls;
bool hasPlayer = true;
PlayerState::WAIT_VERIFY waitVerify = PlayerState::WAIT_VERIFY_NULL;
MItem* mouseItem = nullptr;

struct FakeCreature
{
	bool slayer = true;
	bool ousters = false;
};
FakeCreature me;

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* pCreature) { return *reinterpret_cast<FakeCreature*>(pCreature); }
std::string Text(int id) { return (*g_pGameStringTable)[id].GetString(); }

struct Goods : MItem
{
	ITEM_CLASS m_Class;
	explicit Goods(ITEM_CLASS itemClass, TYPE_OBJECTID id, int number = 1) : m_Class(itemClass)
	{
		SetID(id);
		SetNumber(static_cast<TYPE_ITEM_NUMBER>(number));
	}
	ITEM_CLASS GetItemClass() const override { return m_Class; }
	bool IsPileItem() const override { return m_Class == ITEM_CLASS_POTION; }
	TYPE_ITEM_NUMBER GetMaxNumber() const override { return IsPileItem() ? 20 : 1; }
};

// A freed item's end is recorded.
const MItemHost items = []() {
	MItemHost host{};
	host.ItemDestroyed = [](MItem* pItem) noexcept { calls.push_back("freed|" + N(pItem->GetID())); };
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? Handle(me) : nullptr; };
	host.GetWaitVerify = []() { return waitVerify; };
	host.SetWaitVerifyNULL = []() { calls.push_back("verified"); };
	host.ClearItemCheckBuffer = []() { calls.push_back("buffer cleared"); };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.IsSlayer = [](MCreature* p) { return Fake(p).slayer; };
	host.IsOusters = [](MCreature* p) { return Fake(p).ousters; };
	return host;
}();

const TradePresentation::Host trade = []() {
	TradePresentation::Host host{};
	host.CloseExchange = []() { calls.push_back("window closed"); };
	host.CloseExchangeCancel = []() { calls.push_back("cancel box closed"); };
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.PopupFreeMessageDlg = [](const char* message) { calls.push_back(std::string("message|") + message); };
	host.PickUpItem = [](MItem* pItem) { calls.push_back("mouse takes|" + N(pItem->GetID())); };
	host.GetMouseItem = []() { return mouseItem; };
	host.DropItem = []() { calls.push_back("mouse drops"); };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.PlaySound = [](TYPE_SOUNDID soundID) { calls.push_back("sound|" + N(soundID)); };
	return host;
}();

// The game connection, its output kept unsent for inspection.
struct GamePlayer : ClientPlayer
{
	GamePlayer() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~GamePlayer() { setPlayerStatus(CPS_END_SESSION); }
	std::vector<unsigned char> Sent() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};

std::vector<unsigned char> Encoded(std::initializer_list<const Packet*> packets)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	for (const Packet* packet : packets) encoded.write(packet);
	return SocketOutputStreamTestAccess::Bytes(encoded);
}

std::vector<unsigned char> Reconsider()
{
	CGTradeFinish finish;
	finish.setTargetObjectID(1003);
	finish.setCode(CG_TRADE_FINISH_RECONSIDER);
	return Encoded({&finish});
}

struct World : GameModelWorld
{
	MInventory inventory;
	MMoneyManager money;
	MTradeManager tradeManager;
	TempInformation temp;
	GamePlayer connection;
	MInventory* oldInventory = g_pInventory;
	MMoneyManager* oldMoney = g_pMoneyManager;
	MTradeManager* oldTrade = g_pTradeManager;
	TempInformation* oldTemp = g_pTempInformation;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const TradePresentation::Host* oldTradeHost = TradePresentation::SetHost(&trade);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		InitGameStringTable();
		calls.clear();
		hasPlayer = true;
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
		mouseItem = nullptr;
		me = FakeCreature();
		for (ITEM_CLASS itemClass : {ITEM_CLASS_SWORD, ITEM_CLASS_POTION})
		{
			g_pItemTable->InitClass(itemClass, 2);
			for (int type = 0; type < 2; type++)
			{
				testfw::MutableRow(*g_pItemTable, itemClass, type).SetGrid(1, 1);
				testfw::MutableRow(*g_pItemTable, itemClass, type).InventorySoundID = static_cast<TYPE_SOUNDID>(200 + itemClass);
			}
		}
		inventory.Init(6, 4);
		money.SetMoney(1000);
		g_pInventory = &inventory;
		g_pMoneyManager = &money;
		tradeManager.Init();
		tradeManager.SetOtherID(1003);
		g_pTradeManager = &tradeManager;
		g_pTempInformation = &temp;
		MItem::SetHost(&items);
	}
	~World()
	{
		MItem::SetHost(nullptr);
		WorldEvents::SetHost(oldEvents);
		DialogPresentation::SetHost(oldDialog);
		TradePresentation::SetHost(oldTradeHost);
		CreatureState::SetHost(oldCreature);
		PlayerState::SetHost(oldPlayer);
		g_pTempInformation = oldTemp;
		g_pTradeManager = oldTrade;
		tradeManager.Release();
		inventory.Release();
		g_pMoneyManager = oldMoney;
		g_pInventory = oldInventory;
	}
	void Accepted()
	{
		tradeManager.AcceptMyTrade();
		tradeManager.AcceptOtherTrade();
	}
	bool Refused() const { return !tradeManager.IsAcceptMyTrade() && !tradeManager.IsAcceptOtherTrade(); }
	void Waiting(TempInformation::TEMP_MODE mode, MItem* pItem = nullptr, int x = 0, int y = 0)
	{
		temp.SetMode(mode);
		temp.pValue = pItem;
		temp.Value2 = x;
		temp.Value3 = y;
	}
};

void ReceiveError(int code)
{
	GCTradeError packet;
	packet.setTargetObjectID(1003);
	packet.setCode(static_cast<BYTE>(code));
	GCTradeErrorHandler::execute(&packet, nullptr);
}

void ReceiveVerify(int code, Player* pConnection)
{
	GCTradeVerify packet;
	packet.setCode(static_cast<BYTE>(code));
	GCTradeVerifyHandler::execute(&packet, pConnection);
}

} // namespace

TEST(TradeReplies, ARefusedRequestEndsTheWaitAndSaysWhy)
{
	World world;
	const struct { int code; int text; } refusals[] = {
		{GC_TRADE_ERROR_CODE_TARGET_NOT_EXIST, STRING_MESSAGE_TRADE_NOBODY},
		{GC_TRADE_ERROR_CODE_RACE_DIFFER, STRING_MESSAGE_CANNOT_TRADE},
		{GC_TRADE_ERROR_CODE_NOT_SAFE, STRING_MESSAGE_TRADE_SAFETY_ZONE_ONLY}};
	for (const auto& r : refusals)
	{
		calls.clear();
		world.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
		waitVerify = PlayerState::WAIT_VERIFY_TRADE;
		ReceiveError(r.code);
		CHECK(calls == std::vector<std::string>({"cancel box closed", "verified", "cancel box closed",
			"message|" + Text(r.text), "window closed"}));
		CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	}
	// Not waiting on a request: only the box, the message and the window.
	calls.clear();
	world.temp.SetMode(TempInformation::MODE_TRADE_VERIFY_ADD_ITEM);
	ReceiveError(GC_TRADE_ERROR_CODE_TARGET_NOT_EXIST);
	CHECK(calls == std::vector<std::string>({"cancel box closed", "message|" + Text(STRING_MESSAGE_TRADE_NOBODY), "window closed"}));
	CHECK(world.temp.GetMode() == TempInformation::MODE_TRADE_VERIFY_ADD_ITEM);
	// Waiting on a request but not on the trade answer: the wait stays.
	calls.clear();
	world.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	waitVerify = PlayerState::WAIT_VERIFY_NULL;
	ReceiveError(GC_TRADE_ERROR_CODE_RACE_DIFFER);
	CHECK(calls == std::vector<std::string>({"cancel box closed", "cancel box closed", "message|" + Text(STRING_MESSAGE_CANNOT_TRADE), "window closed"}));
}

TEST(TradeReplies, EachOtherErrorSaysWhatItSaysAndClosesTheWindow)
{
	World world;
	ReceiveError(GC_TRADE_ERROR_CODE_MOTORCYCLE);
	me = {false, true};
	ReceiveError(GC_TRADE_ERROR_CODE_MOTORCYCLE);
	me = {false, false};
	ReceiveError(GC_TRADE_ERROR_CODE_MOTORCYCLE);
	CHECK(calls == std::vector<std::string>({"cancel box closed", "message|" + Text(STRING_MESSAGE_TRADE_CANNOT_ON_MOTORCYCLE), "window closed",
		"cancel box closed", "message|" + Text(STRING_MESSAGE_CANNOT_TRADE_SUMMON_SYLPH), "window closed",
		"cancel box closed", "window closed"}));
	calls.clear();
	ReceiveError(GC_TRADE_ERROR_CODE_ALREADY_TRADING);
	ReceiveError(GC_TRADE_ERROR_CODE_NOT_ENOUGH_SPACE);
	ReceiveError(GC_TRADE_ERROR_CODE_UNKNOWN);
	ReceiveError(GC_TRADE_ERROR_CODE_EVENT_GIFT_BOX);
	CHECK(calls == std::vector<std::string>({"cancel box closed", "message|" + Text(STRING_MESSAGE_CANNOT_TRADE_ALREADY_TRADING), "window closed",
		"message|" + Text(STRING_MESSAGE_CANNOT_TRADE_NO_SPACE), "window closed",
		"message|" + Text(STRING_MESSAGE_CANNOT_TRADE), "window closed",
		"message|" + Text(STRING_MESSAGE_CANNOT_EVENT_GIFT_BOX), "window closed"}));
	// The server's own bookkeeping errors only close the window.
	calls.clear();
	for (int code : {GC_TRADE_ERROR_CODE_NOT_TRADING, GC_TRADE_ERROR_CODE_ADD_ITEM, GC_TRADE_ERROR_CODE_REMOVE_ITEM,
		GC_TRADE_ERROR_CODE_INCREASE_MONEY, GC_TRADE_ERROR_CODE_DECREASE_MONEY, GC_TRADE_ERROR_CODE_BAT_OR_WOLF})
		ReceiveError(code);
	CHECK(calls == std::vector<std::string>(6, "window closed"));
}

TEST(TradeReplies, AnErrorGivesThePlayersTradeMoneyBack)
{
	World world;
	world.tradeManager.GetMyMoneyManager()->SetMoney(250);
	ReceiveError(GC_TRADE_ERROR_CODE_UNKNOWN);
	CHECK_EQ(1250, world.money.GetMoney());
	CHECK_EQ(0, world.tradeManager.GetMyMoneyManager()->GetMoney());
	// Without a trade the window still closes; without a player or the
	// register, nothing happens.
	calls.clear();
	g_pTradeManager = nullptr;
	ReceiveError(GC_TRADE_ERROR_CODE_NOT_TRADING);
	CHECK(calls == std::vector<std::string>{"window closed"});
	calls.clear();
	g_pTradeManager = &world.tradeManager;
	hasPlayer = false;
	ReceiveError(GC_TRADE_ERROR_CODE_UNKNOWN);
	hasPlayer = true;
	g_pTempInformation = nullptr;
	ReceiveError(GC_TRADE_ERROR_CODE_UNKNOWN);
	g_pTempInformation = &world.temp;
	CHECK(calls.empty());
}

TEST(TradeReplies, AnItemAddedToTheTradeIsMarkedAndBothSidesReconsider)
{
	World world;
	Goods sword(ITEM_CLASS_SWORD, 41);
	for (int code : {GC_TRADE_VERIFY_CODE_ADD_ITEM_OK, GC_TRADE_VERIFY_CODE_ADD_ITEM_WHEN_ACCEPT})
	{
		calls.clear();
		sword.UnSetTrade();
		world.Accepted();
		world.Waiting(TempInformation::MODE_TRADE_VERIFY_ADD_ITEM, &sword);
		ReceiveVerify(code, &world.connection);
		CHECK(calls == std::vector<std::string>{"buffer cleared"});
		CHECK(sword.IsTrade());
		CHECK(world.Refused());
		CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	}
	// A failed addition only ends the wait.
	calls.clear();
	sword.UnSetTrade();
	world.Accepted();
	world.Waiting(TempInformation::MODE_TRADE_VERIFY_ADD_ITEM, &sword);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_ADD_ITEM_FAIL, &world.connection);
	CHECK(calls == std::vector<std::string>{"buffer cleared"});
	CHECK(!sword.IsTrade());
	CHECK(world.tradeManager.IsAcceptMyTrade());
	// Not waiting on an addition: nothing.
	calls.clear();
	world.Waiting(TempInformation::MODE_TRADE_VERIFY_REMOVE_ITEM, &sword);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_ADD_ITEM_OK, &world.connection);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_ADD_ITEM_FAIL, &world.connection);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_ADD_ITEM_WHEN_ACCEPT, &world.connection);
	CHECK(calls.empty());
	CHECK(!sword.IsTrade());
	CHECK(world.connection.Sent().empty());
}

TEST(TradeReplies, AnItemTakenOutOfTheTradeIsUnmarked)
{
	World world;
	Goods sword(ITEM_CLASS_SWORD, 41);
	sword.SetTrade();
	world.Accepted();
	world.Waiting(TempInformation::MODE_TRADE_VERIFY_REMOVE_ITEM, &sword);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_REMOVE_ITEM, &world.connection);
	CHECK(calls == std::vector<std::string>{"buffer cleared"});
	CHECK(!sword.IsTrade());
	CHECK(world.Refused());
}

TEST(TradeReplies, AnItemPickedUpFromTheTradeBoxGoesOnTheMouse)
{
	World world;
	Goods* sword = new Goods(ITEM_CLASS_SWORD, 41);
	CHECK(world.inventory.AddItem(sword, 2, 1));
	world.Accepted();
	world.Waiting(TempInformation::MODE_TRADE_VERIFY_PICKUP_FROM_INVENTORY, sword, 2, 1);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_INVENTORY_TO_MOUSE_OK, &world.connection);
	CHECK(calls == std::vector<std::string>({"buffer cleared", "mouse takes|41"}));
	CHECK(world.inventory.GetItem(2, 1) == nullptr);
	CHECK(world.Refused());
	// Not part of the trade: the other side is asked to reconsider.
	CHECK(world.connection.Sent() == Reconsider());
	// Part of the trade: nothing is sent.
	GamePlayer quiet;
	sword->SetTrade();
	CHECK(world.inventory.AddItem(sword, 2, 1));
	world.Waiting(TempInformation::MODE_TRADE_VERIFY_PICKUP_FROM_INVENTORY, sword, 2, 1);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_INVENTORY_TO_MOUSE_OK, &quiet);
	CHECK(quiet.Sent().empty());
	delete sword;
}

TEST(TradeReplies, AnItemPutDownInTheTradeBoxLeavesTheMouse)
{
	World world;
	Goods* sword = new Goods(ITEM_CLASS_SWORD, 41);
	mouseItem = sword;
	world.Accepted();
	world.Waiting(TempInformation::MODE_TRADE_VERIFY_DROP_TO_INVENTORY, nullptr, 3, 2);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_MOUSE_TO_INVENTORY_OK, &world.connection);
	CHECK(calls == std::vector<std::string>({"buffer cleared", "mouse drops"}));
	CHECK(world.inventory.GetItem(3, 2) == sword);
	CHECK(world.Refused());
	CHECK(world.connection.Sent() == Reconsider());
	// Put down on another item, that item goes on the mouse; an item of
	// the trade tells the other side it was added.
	calls.clear();
	GamePlayer told;
	Goods* axe = new Goods(ITEM_CLASS_SWORD, 42);
	axe->SetTrade();
	mouseItem = axe;
	world.Waiting(TempInformation::MODE_TRADE_VERIFY_DROP_TO_INVENTORY, nullptr, 3, 2);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_MOUSE_TO_INVENTORY_OK, &told);
	CHECK(calls == std::vector<std::string>({"buffer cleared", "mouse takes|41"}));
	CGTradeAddItem added;
	added.setTargetObjectID(1003);
	added.setItemObjectID(42);
	CHECK(told.Sent() == Encoded({&added}));
	// Nothing on the mouse: only both sides reconsider.
	calls.clear();
	GamePlayer empty;
	mouseItem = nullptr;
	world.Accepted();
	world.Waiting(TempInformation::MODE_TRADE_VERIFY_DROP_TO_INVENTORY, nullptr, 0, 0);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_MOUSE_TO_INVENTORY_OK, &empty);
	CHECK(calls == std::vector<std::string>{"buffer cleared"});
	CHECK(world.Refused());
	CHECK(empty.Sent().empty());
	delete sword;
}

TEST(TradeReplies, AStackPutOntoAnotherJoinsItUpToItsLimit)
{
	World world;
	Goods* stack = new Goods(ITEM_CLASS_POTION, 41, 15);
	CHECK(world.inventory.AddItem(stack, 1, 1));
	Goods* held = new Goods(ITEM_CLASS_POTION, 42, 8);
	mouseItem = held;
	world.Waiting(TempInformation::MODE_TRADE_VERIFY_INSERT_TO_INVENTORY, stack);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_MOUSE_TO_INVENTORY_OK, &world.connection);
	// Past the limit the rest stays on the mouse.
	CHECK_EQ(20, static_cast<int>(stack->GetNumber()));
	CHECK_EQ(3, static_cast<int>(held->GetNumber()));
	CHECK(calls == std::vector<std::string>({"buffer cleared", "sound|" + N(200 + ITEM_CLASS_POTION)}));
	CHECK(world.connection.Sent() == Reconsider());
	// Within the limit it all joins and the held stack goes; an item of
	// the trade is taken out and added again.
	calls.clear();
	GamePlayer told;
	stack->SetNumber(5);
	stack->SetTrade();
	world.Waiting(TempInformation::MODE_TRADE_VERIFY_INSERT_TO_INVENTORY, stack);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_MOUSE_TO_INVENTORY_OK, &told);
	CHECK_EQ(8, static_cast<int>(stack->GetNumber()));
	CHECK(calls == std::vector<std::string>({"buffer cleared", "mouse drops", "freed|42", "sound|" + N(200 + ITEM_CLASS_POTION)}));
	CGTradeRemoveItem removed;
	removed.setTargetObjectID(1003);
	removed.setItemObjectID(41);
	CGTradeAddItem added;
	added.setTargetObjectID(1003);
	added.setItemObjectID(41);
	CHECK(told.Sent() == Encoded({&removed, &added}));
}

TEST(TradeReplies, MoneyAndTheTradesEndAreVerified)
{
	World world;
	world.Accepted();
	world.temp.SetMode(TempInformation::MODE_TRADE_VERIFY_ADD_MONEY);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_MONEY_INCREASE, &world.connection);
	CHECK(world.Refused());
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	world.Accepted();
	world.temp.SetMode(TempInformation::MODE_TRADE_VERIFY_REMOVE_MONEY);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_MONEY_DECREASE, &world.connection);
	CHECK(world.Refused());
	// The wrong mode: the acceptance stands.
	world.Accepted();
	world.temp.SetMode(TempInformation::MODE_TRADE_VERIFY_ADD_MONEY);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_MONEY_DECREASE, &world.connection);
	world.temp.SetMode(TempInformation::MODE_TRADE_VERIFY_REMOVE_MONEY);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_MONEY_INCREASE, &world.connection);
	CHECK(world.tradeManager.IsAcceptMyTrade() && world.tradeManager.IsAcceptOtherTrade());
	CHECK(calls.empty());
	// A refused trade gives the money back and closes the window.
	world.tradeManager.GetMyMoneyManager()->SetMoney(100);
	world.temp.SetMode(TempInformation::MODE_TRADE_VERIFY_CLOSE);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_FINISH_REJECT, &world.connection);
	CHECK(calls == std::vector<std::string>{"window closed"});
	CHECK_EQ(1100, world.money.GetMoney());
	// Reconsidering takes back only the player's acceptance.
	world.temp.SetMode(TempInformation::MODE_TRADE_VERIFY_CANCEL);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_FINISH_RECONSIDER, &world.connection);
	CHECK(!world.tradeManager.IsAcceptMyTrade());
	CHECK(world.tradeManager.IsAcceptOtherTrade());
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	// An accepted trade needs nothing.
	calls.clear();
	ReceiveVerify(GC_TRADE_VERIFY_CODE_FINISH_ACCEPT, &world.connection);
	CHECK(calls.empty());
	CHECK(world.connection.Sent().empty());
}

TEST(TradeReplies, AFailedMoveEndsTheWait)
{
	World world;
	world.temp.SetMode(TempInformation::MODE_TRADE_VERIFY_DROP_TO_INVENTORY);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_MOUSE_TO_INVENTORY_FAIL, &world.connection);
	world.temp.SetMode(TempInformation::MODE_TRADE_VERIFY_PICKUP_FROM_INVENTORY);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_INVENTORY_TO_MOUSE_FAIL, &world.connection);
	CHECK(calls == std::vector<std::string>(2, "buffer cleared"));
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	// The wrong mode: nothing.
	calls.clear();
	world.temp.SetMode(TempInformation::MODE_TRADE_VERIFY_PICKUP_FROM_INVENTORY);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_MOUSE_TO_INVENTORY_FAIL, &world.connection);
	world.temp.SetMode(TempInformation::MODE_TRADE_VERIFY_DROP_TO_INVENTORY);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_INVENTORY_TO_MOUSE_FAIL, &world.connection);
	CHECK(calls.empty());
}

TEST(TradeReplies, AVerifyWithoutAPlayerOrATradeDoesNothing)
{
	World world;
	world.Accepted();
	world.temp.SetMode(TempInformation::MODE_TRADE_VERIFY_ADD_MONEY);
	hasPlayer = false;
	ReceiveVerify(GC_TRADE_VERIFY_CODE_MONEY_INCREASE, &world.connection);
	hasPlayer = true;
	g_pTradeManager = nullptr;
	ReceiveVerify(GC_TRADE_VERIFY_CODE_MONEY_INCREASE, &world.connection);
	g_pTradeManager = &world.tradeManager;
	CHECK(world.tradeManager.IsAcceptMyTrade());
	CHECK(world.temp.GetMode() == TempInformation::MODE_TRADE_VERIFY_ADD_MONEY);
}

TEST(TradeReplies, OnlyARefusalEndsTheRequestWait)
{
	World world;
	for (int code : {GC_TRADE_ERROR_CODE_ALREADY_TRADING, GC_TRADE_ERROR_CODE_UNKNOWN, GC_TRADE_ERROR_CODE_NOT_TRADING})
	{
		world.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
		waitVerify = PlayerState::WAIT_VERIFY_TRADE;
		ReceiveError(code);
		CHECK(world.temp.GetMode() == TempInformation::MODE_TRADE_REQUEST);
	}
	CHECK(std::find(calls.begin(), calls.end(), "verified") == calls.end());
	// Without the game strings, nothing happens.
	calls.clear();
	MStringArray* strings = g_pGameStringTable;
	g_pGameStringTable = nullptr;
	ReceiveError(GC_TRADE_ERROR_CODE_TARGET_NOT_EXIST);
	g_pGameStringTable = strings;
	CHECK(calls.empty());
	CHECK(world.temp.GetMode() == TempInformation::MODE_TRADE_REQUEST);
}

TEST(TradeReplies, AStackFillingItsLimitExactlyJoinsWhole)
{
	World world;
	Goods* stack = new Goods(ITEM_CLASS_POTION, 41, 12);
	CHECK(world.inventory.AddItem(stack, 1, 1));
	mouseItem = new Goods(ITEM_CLASS_POTION, 42, 8);
	world.Waiting(TempInformation::MODE_TRADE_VERIFY_INSERT_TO_INVENTORY, stack);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_MOUSE_TO_INVENTORY_OK, &world.connection);
	CHECK_EQ(20, static_cast<int>(stack->GetNumber()));
	CHECK(calls == std::vector<std::string>({"buffer cleared", "mouse drops", "freed|42", "sound|" + N(200 + ITEM_CLASS_POTION)}));
}

TEST(TradeReplies, AnItemThatCannotBePutDownStaysOnTheMouse)
{
	World world;
	Goods* sword = new Goods(ITEM_CLASS_SWORD, 41);
	mouseItem = sword;
	world.Accepted();
	// Past the trade box's edge.
	world.Waiting(TempInformation::MODE_TRADE_VERIFY_DROP_TO_INVENTORY, nullptr, 6, 0);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_MOUSE_TO_INVENTORY_OK, &world.connection);
	CHECK(calls == std::vector<std::string>{"buffer cleared"});
	CHECK(world.Refused());
	CHECK(world.connection.Sent() == Reconsider());
	delete sword;
}

TEST(TradeReplies, AVerifyInTheWrongModeChangesNothing)
{
	World world;
	Goods sword(ITEM_CLASS_SWORD, 41);
	mouseItem = &sword;
	world.Accepted();
	ReceiveVerify(GC_TRADE_VERIFY_CODE_MOUSE_TO_INVENTORY_OK, &world.connection);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_INVENTORY_TO_MOUSE_OK, &world.connection);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_REMOVE_ITEM, &world.connection);
	CHECK(world.tradeManager.IsAcceptMyTrade() && world.tradeManager.IsAcceptOtherTrade());
	CHECK(world.connection.Sent().empty());
	// The trade's end, each in another's mode.
	world.tradeManager.GetMyMoneyManager()->SetMoney(100);
	world.temp.SetMode(TempInformation::MODE_TRADE_VERIFY_CANCEL);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_FINISH_REJECT, &world.connection);
	CHECK(world.temp.GetMode() == TempInformation::MODE_TRADE_VERIFY_CANCEL);
	world.temp.SetMode(TempInformation::MODE_TRADE_VERIFY_CLOSE);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_FINISH_RECONSIDER, &world.connection);
	ReceiveVerify(GC_TRADE_VERIFY_CODE_FINISH_ACCEPT, &world.connection);
	CHECK(world.temp.GetMode() == TempInformation::MODE_TRADE_VERIFY_CLOSE);
	CHECK(world.tradeManager.IsAcceptMyTrade() && world.tradeManager.IsAcceptOtherTrade());
	CHECK_EQ(1000, world.money.GetMoney());
	CHECK(calls.empty());
	mouseItem = nullptr;
}
