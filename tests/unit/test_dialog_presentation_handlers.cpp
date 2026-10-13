#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "DialogPresentationHost.h"
#include "MInventory.h"
#include "MStorage.h"
#include "MZoneTable.h"
#include "TempInformation.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCLearnSkillFailed.h"
#include "Gpackets/GCPetStashVerify.h"
#include "Gpackets/GCSearchMotorcycleFail.h"
#include "Gpackets/GCSearchMotorcycleOK.h"
#include "Gpackets/GCShopBuyFail.h"
#include "Gpackets/GCShopSellFail.h"
#include "Gpackets/GCTakeOutFail.h"
#include "Gpackets/GCTakeOutOK.h"
#include "Gpackets/GCUseBonusPointFail.h"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <set>
#include <string>
#include <vector>

// Replies that end in a message box or release a locked item present what
// they decide through DialogPresentation::Host: which message, and in what
// order with the item unlocks and the request register. The pet-stash
// reply also moves a pet item between the storage box and the inventory.
namespace {

std::vector<std::string> calls;
// The request register's mode when each call was made, beside it in calls.
std::vector<int> modes;
bool swapRunning = false;

std::string Text(const char* text) { return text ? text : "(null)"; }
std::string Message(int id) { return "popup|" + std::string((*g_pGameStringTable)[id].GetString()); }

void Record(const std::string& call)
{
	calls.push_back(call);
	modes.push_back(static_cast<int>(g_pTempInformation->GetMode()));
}

std::vector<int> Modes(std::initializer_list<TempInformation::TEMP_MODE> list)
{
	std::vector<int> out;
	for (TempInformation::TEMP_MODE mode : list) out.push_back(static_cast<int>(mode));
	return out;
}

const DialogPresentation::Host host{
	.PopupFreeMessageDlg = [](const char* message) { Record("popup|" + Text(message)); },
	.UnlockItem = []() { Record("unlock"); },
	.UnlockItemTrade = []() { Record("unlocktrade"); },
	.RemoveItemFromItemShop = [](std::uint32_t id) { Record("removeshopitem|" + std::to_string(id)); },
	.IsRunningSwapAdvancementItem = []() { Record("swap?"); return swapRunning; },
};

struct World : GameModelWorld
{
	TempInformation temp;
	TempInformation* oldTemp = g_pTempInformation;
	CZoneTable zones;
	CZoneTable* oldZones = g_pZoneTable;
	MInventory* oldInventory = g_pInventory;
	MStorage* oldStorage = g_pStorage;
	const DialogPresentation::Host* oldHost = DialogPresentation::SetHost(&host);
	World()
	{
		InitGameStringTable();
		g_pItemTable->InitClass(ITEM_CLASS_PET_ITEM, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_PET_ITEM, 0).SetGrid(1, 1);
		g_pTempInformation = &temp;
		ZONETABLE_INFO* zone = new ZONETABLE_INFO;
		zone->ID = 13;
		zone->Name = "Eslania";
		zones.Add(zone);
		g_pZoneTable = &zones;
		g_pInventory = new MInventory;
		g_pInventory->Init(4, 4);
		g_pStorage = new MStorage;
		g_pStorage->Init(1);
		calls.clear();
		modes.clear();
		swapRunning = false;
	}
	~World()
	{
		DialogPresentation::SetHost(oldHost);
		delete g_pStorage;	g_pStorage = oldStorage;
		delete g_pInventory;	g_pInventory = oldInventory;
		g_pZoneTable = oldZones;
		g_pTempInformation = oldTemp;
	}
};

template<class PacketType>
void Decode(PacketType& packet, const PacketType& original)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	packet.read(stream);
	CHECK(stream.isEmpty());
}

void ReceiveLearnSkillFailed(BYTE desc)
{
	GCLearnSkillFailed original;
	original.setSkillType(7);
	original.setDesc(desc);
	GCLearnSkillFailed packet;
	Decode(packet, original);
	GCLearnSkillFailedHandler::execute(&packet, nullptr);
}

void ReceiveMotorcycleFound(ZoneID_t zone)
{
	GCSearchMotorcycleOK original;
	original.setZoneID(zone);
	original.setX(81);
	original.setY(42);
	GCSearchMotorcycleOK packet;
	Decode(packet, original);
	GCSearchMotorcycleOKHandler::execute(&packet, nullptr);
}

void ReceivePetStash(BYTE code)
{
	GCPetStashVerify original;
	original.setCode(code);
	GCPetStashVerify packet;
	Decode(packet, original);
	GCPetStashVerifyHandler::execute(&packet, nullptr);
}

void ReceiveShopBuyFail(BYTE code)
{
	GCShopBuyFail original;
	original.setObjectID(1234);
	original.setCode(code);
	original.setAmount(5);
	GCShopBuyFail packet;
	Decode(packet, original);
	GCShopBuyFailHandler::execute(&packet, nullptr);
}

void ReceiveTakeOutOK(ObjectID_t id)
{
	GCTakeOutOK original;
	original.setObjectID(id);
	GCTakeOutOK packet;
	Decode(packet, original);
	GCTakeOutOKHandler::execute(&packet, nullptr);
}

// Every host entry is asked for at least once: seven calls with a host.
void RunEveryHandler(World& world)
{
	GCTakeOutFail takeOutFail;
	GCTakeOutFailHandler::execute(&takeOutFail, nullptr);
	ReceiveTakeOutOK(77);
	ReceivePetStash(GCPetStashVerify::PET_STASH_RACK_IS_EMPTY + 1);
	GCShopSellFail sellFail;
	world.temp.SetMode(TempInformation::MODE_SHOP_SELL);
	GCShopSellFailHandler::execute(&sellFail, nullptr);
}

} // namespace

TEST(DialogPresentationHandlers, ASkillLearnFailureClearsTheRequestAndNamesItsReason)
{
	World world;
	const struct { BYTE desc; int message; } cases[] = {
		{10, STRING_MESSAGE_SKILL_NOT_SUPPORT}, {0, STRING_MESSAGE_SKILL_DIFFER_DOMAIN},
		{1, STRING_MESSAGE_SKILL_EXCEED_LEVEL}, {2, STRING_MESSAGE_SKILL_CANNOT_LEARN},
		{255, STRING_MESSAGE_SKILL_CANNOT_LEARN},
	};
	for (const auto& c : cases)
	{
		calls.clear();
		modes.clear();
		world.temp.SetMode(TempInformation::MODE_SKILL_LEARN);
		ReceiveLearnSkillFailed(c.desc);
		CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
		CHECK_EQ(1u, calls.size());
		CHECK(!calls.empty() && calls[0] == Message(c.message));
		// The request is cleared before the reason is shown.
		CHECK(modes == Modes({TempInformation::MODE_NULL}));
	}
}

TEST(DialogPresentationHandlers, AFailedMotorcycleSearchPicksOneOfTwoExcuses)
{
	World world;
	std::set<std::string> seen;
	for (unsigned int seed = 1; seed <= 64; ++seed)
	{
		std::srand(seed);
		GCSearchMotorcycleFail packet;
		GCSearchMotorcycleFailHandler::execute(&packet, nullptr);
	}
	CHECK_EQ(64u, calls.size());
	for (const std::string& call : calls) seen.insert(call);
	CHECK_EQ(2u, seen.size());
	CHECK(seen.count(Message(STRING_MESSAGE_FIND_MOTOR_NO_WHERE)) == 1);
	CHECK(seen.count(Message(STRING_MESSAGE_FIND_MOTOR_NO_KEY)) == 1);
}

TEST(DialogPresentationHandlers, AFoundMotorcycleNamesItsZoneAndPositionWhenTheZoneIsKnown)
{
	World world;
	ReceiveMotorcycleFound(13);
	ReceiveMotorcycleFound(14);
	CHECK_EQ(2u, calls.size());
	CHECK(calls.size() == 2 && calls[0] == "popup|It is in Eslania at (81, 42).");
	CHECK(calls.size() == 2 && calls[1] == Message(STRING_MESSAGE_FIND_MOTOR_NO_WHERE));
}

TEST(DialogPresentationHandlers, TakeOutRepliesConfirmThenDropTheShopItem)
{
	World world;
	GCTakeOutFail fail;
	GCTakeOutFailHandler::execute(&fail, nullptr);
	ReceiveTakeOutOK(9001);
	CHECK_EQ(3u, calls.size());
	CHECK(calls.size() == 3 && calls[0] == Message(UI_STRING_MESSAGE_TAKE_OUT_FAIL));
	CHECK(calls.size() == 3 && calls[1] == Message(UI_STRING_MESSAGE_TAKE_OUT_OK));
	CHECK(calls.size() == 3 && calls[2] == "removeshopitem|9001");
}

TEST(DialogPresentationHandlers, BonusPointAndShopFailuresClearTheRequestAndUnlockFirst)
{
	World world;
	world.temp.SetMode(TempInformation::MODE_SHOP_BUY);
	GCUseBonusPointFail bonus;
	GCUseBonusPointFailHandler::execute(&bonus, nullptr);
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);

	world.temp.SetMode(TempInformation::MODE_SHOP_BUY);
	ReceiveShopBuyFail(GC_SHOP_BUY_FAIL_NOT_ENOUGH_BLACK_STAR);
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	ReceiveShopBuyFail(200);

	world.temp.SetMode(TempInformation::MODE_SHOP_SELL);
	GCShopSellFail sell;
	GCShopSellFailHandler::execute(&sell, nullptr);
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	swapRunning = true;
	world.temp.SetMode(TempInformation::MODE_SHOP_SELL);
	GCShopSellFailHandler::execute(&sell, nullptr);

	const std::vector<std::string> expected = {
		Message(STRING_MESSAGE_CANNOT_USE_BONUS_POINT),
		"unlocktrade", Message(STRING_MESSAGE_CANNOT_BUY_NO_ITEM),
		"unlocktrade", Message(STRING_MESSAGE_CANNOT_BUY_NO_ITEM),
		"unlocktrade", "swap?", Message(STRING_MESSAGE_CANNOT_SELL),
		"unlocktrade", "swap?", Message(UI_STRING_MESSAGE_SWAP_ADVANCEMENT_ITEM_ERROR),
	};
	CHECK(calls == expected);
	// Each request is cleared before anything is unlocked or shown.
	CHECK(modes == std::vector<int>(expected.size(), static_cast<int>(TempInformation::MODE_NULL)));
}

TEST(DialogPresentationHandlers, PetStashFailuresNameTheirReasonThenClearAndUnlock)
{
	World world;
	const struct { BYTE code; int message; } cases[] = {
		{GCPetStashVerify::PET_STASH_NOT_ENOUGH_MONEY, STRING_MESSAGE_WAR_NOT_ENOUGH_MONEY},
		{GCPetStashVerify::PET_STASH_NO_INVENTORY_SPACE, UI_STRING_MESSAGE_INVENTORY_FULL_MONSTER_KILL_QUEST},
		{GCPetStashVerify::PET_STASH_RACK_IS_NOT_EMPTY, STRING_MESSAGE_EXIST_ITEM_ALREADY},
		{GCPetStashVerify::PET_STASH_RACK_IS_EMPTY, STRING_MESSAGE_CANNOT_BUY_NO_ITEM},
	};
	for (const auto& c : cases)
	{
		calls.clear();
		modes.clear();
		world.temp.SetMode(TempInformation::MODE_PETITEM_MOVETO_INVENTORY);
		ReceivePetStash(c.code);
		CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
		CHECK_EQ(2u, calls.size());
		CHECK(calls.size() == 2 && calls[0] == Message(c.message) && calls[1] == "unlock");
		// The reason is shown while the request is pending; it is cleared before the unlock.
		CHECK(modes == Modes({TempInformation::MODE_PETITEM_MOVETO_INVENTORY, TempInformation::MODE_NULL}));
	}
	calls.clear();
	modes.clear();
	world.temp.SetMode(TempInformation::MODE_PETITEM_MOVETO_INVENTORY);
	ReceivePetStash(GCPetStashVerify::PET_STASH_RACK_IS_EMPTY + 1);
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	CHECK(calls == std::vector<std::string>{"unlock"});
	CHECK(modes == Modes({TempInformation::MODE_NULL}));
}

TEST(DialogPresentationHandlers, AConfirmedPetStashMovesThePetTheRequestAsked)
{
	World world;
	// Storage box slot 3 -> inventory, clearing the days it was kept.
	MPetItem* stored = new MPetItem;
	stored->SetID(501);
	stored->SetPetKeepedDay(6);
	CHECK(g_pStorage->SetItem(3, stored));
	world.temp.SetMode(TempInformation::MODE_PETITEM_MOVETO_INVENTORY);
	world.temp.Value1 = 3;
	world.temp.pValue = stored;
	ReceivePetStash(GCPetStashVerify::PET_STASH_OK);
	CHECK(g_pStorage->GetItem(3) == nullptr);
	CHECK(g_pInventory->GetItem(static_cast<TYPE_OBJECTID>(501)) == stored);
	CHECK_EQ(0, static_cast<long long>(stored->GetPetKeepedDay()));
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);

	// Inventory grid (x, y) -> storage box slot 5.
	MPetItem* carried = new MPetItem;
	carried->SetID(502);
	CHECK(g_pInventory->AddItem(carried, 2, 1));
	world.temp.SetMode(TempInformation::MODE_PETITEM_MOVETO_PETSTORAGE);
	world.temp.Value1 = 5;
	world.temp.Value2 = 2;
	world.temp.Value3 = 1;
	world.temp.pValue = carried;
	ReceivePetStash(GCPetStashVerify::PET_STASH_OK);
	CHECK(g_pInventory->GetItem(2, 1) == nullptr);
	CHECK(g_pStorage->GetItem(5) == carried);

	// Any other request moves nothing, from either container.
	MPetItem* blocker = new MPetItem;
	blocker->SetID(503);
	CHECK(g_pInventory->AddItem(blocker, 2, 1));
	world.temp.SetMode(TempInformation::MODE_SHOP_BUY);
	world.temp.Value1 = 5;
	world.temp.pValue = carried;
	ReceivePetStash(GCPetStashVerify::PET_STASH_OK);
	CHECK(g_pStorage->GetItem(5) == carried);
	CHECK(g_pInventory->GetItem(2, 1) == blocker);
	CHECK(g_pInventory->GetItem(static_cast<TYPE_OBJECTID>(501)) == stored);
	CHECK(calls == std::vector<std::string>({"unlock", "unlock", "unlock"}));
	CHECK(modes == std::vector<int>(3, static_cast<int>(TempInformation::MODE_NULL)));
}

TEST(DialogPresentationHandlers, MissingHostsAndEntriesSkipPresentationOnly)
{
	World world;
	DialogPresentation::SetHost(nullptr);
	RunEveryHandler(world);
	CHECK(calls.empty());
	CHECK(!DialogPresentation::IsRunningSwapAdvancementItem());
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	const DialogPresentation::Host empty{};
	CHECK(DialogPresentation::SetHost(&empty) == nullptr);
	RunEveryHandler(world);
	CHECK(calls.empty());
	CHECK(!DialogPresentation::IsRunningSwapAdvancementItem());
	CHECK(DialogPresentation::SetHost(&host) == &empty);
	RunEveryHandler(world);
	CHECK_EQ(7u, calls.size());
}
