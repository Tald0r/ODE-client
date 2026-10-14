#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "BonusSkillHost.h"
#include "ClientPlayer.h"
#include "DialogPresentationHost.h"
#include "GameInfoPresentationHost.h"
#include "MHelpDef.h"
#include "MInventory.h"
#include "MItem.h"
#include "MItemTable.h"
#include "MMoneyManager.h"
#include "MQuickSlot.h"
#include "MSkillManager.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketOutputStream.h"
#include "TempInformation.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Cpackets/CGAddMouseToQuickSlot.h"
#include "Gpackets/GCDeleteandPickUpOK.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

// An item picked up, as the server confirms it. The item waiting in the
// player's check buffer is taken out of it; when its ID is the packet's:
// - an event skull (types 12-16) sounds and leaves the zone;
// - money is added (the durability its low word, the silver its high, up
//   to what the player may still carry), sounds and leaves the zone;
// - into the inventory, it goes to its cell (and is picked off the
//   ground), or piles on the same item there (up to the picked one's
//   most) and leaves the zone;
// - onto the mouse, it goes there and is picked off the ground;
// - into the quick slot (when the trade register waits on that), it goes
//   on the mouse, off the ground, and into the slayer's belt or the
//   ouster's arms band (slots 3-5 the second) when the slot is empty, or
//   onto the same item there when they fit; either way the server is
//   told.
// An item the player now has lets its statuses react, and gives the
// skills it enables: holy water, a slayer portal, bomb material, bombs
// and mines for a slayer; a portal item, a wolf's or a bat's item for a
// vampire (only skills the vampire has). A help event follows: a belt's
// or any item's.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
Race race = RACE_SLAYER;
// The player's creature an ouster: its race says ousters for any tribe
// but a slayer's or a vampire's, an NPC's too.
bool ousters = false;
int me;
MItem* bufferItem = nullptr;
PlayerState::ITEM_CHECK_BUFFER bufferStatus = PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY;
// What lies on the ground, the zone's; the mouse only borrows its item.
std::map<TYPE_OBJECTID, std::unique_ptr<MItem>> ground;
MItem* mouse = nullptr;

std::string N(long long value) { return std::to_string(value); }

struct Ware : MItem
{
	ITEM_CLASS m_Class;
	bool m_Pile;
	TYPE_ITEM_NUMBER m_Max;
	bool marked = false;		// a portal item's mark
	Ware(ITEM_CLASS itemClass, TYPE_OBJECTID id, TYPE_ITEMTYPE type = 0, bool pile = false, TYPE_ITEM_NUMBER max = 1)
		: m_Class(itemClass), m_Pile(pile), m_Max(max)
	{
		SetID(id);
		SetItemType(type);
	}
	ITEM_CLASS GetItemClass() const override { return m_Class; }
	bool IsPileItem() const override { return m_Pile; }
	bool IsQuickItem() const override { return m_Pile; }
	TYPE_ITEM_NUMBER GetMaxNumber() const override { return m_Max; }
};

// Lays the item on the ground and puts it in the check buffer.
MItem* Picking(MItem* pItem, PlayerState::ITEM_CHECK_BUFFER status, BYTE x = 1, BYTE y = 2)
{
	pItem->SetGridXY(x, y);
	ground[pItem->GetID()].reset(pItem);
	bufferItem = pItem;
	bufferStatus = status;
	return pItem;
}

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.GetItemCheckBuffer = []() { return bufferItem; };
	host.GetItemCheckBufferStatus = []() { return bufferStatus; };
	host.ClearItemCheckBuffer = []() { calls.push_back("cleared"); bufferItem = nullptr; };
	host.GetRace = []() { return race; };
	host.Creature = []() { return reinterpret_cast<MCreature*>(&me); };
	host.CheckAffectStatus = [](MItem* pItem) { calls.push_back("affect|" + N(pItem->GetID())); };
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.PickupItem = [](TYPE_OBJECTID id) {
		calls.push_back("zone picks up|" + N(id));
		auto it = ground.find(id);
		if (it != ground.end())
		{
			it->second.release();
			ground.erase(it);
		}
	};
	host.RemoveItem = [](TYPE_OBJECTID id) {
		calls.push_back("zone removes|" + N(id));
		return ground.erase(id) == 1;
	};
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.IsOusters = [](MCreature* p) { return p == reinterpret_cast<MCreature*>(&me) && ousters; };
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.PickUpItem = [](MItem* pItem) { calls.push_back("mouse takes|" + N(pItem->GetID())); mouse = pItem; };
	host.DropItem = []() { calls.push_back("mouse drops"); mouse = nullptr; };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.PlaySound = [](TYPE_SOUNDID soundID) { calls.push_back("sound|" + N(soundID)); };
	return host;
}();

const GameInfoPresentation::Host info = []() {
	GameInfoPresentation::Host host{};
	host.ExecuteHelpEvent = [](int helpEvent) { calls.push_back("help|" + N(helpEvent)); };
	return host;
}();

const MItemHost items = []() {
	MItemHost host{};
	host.IsPortalMarked = [](const MItem* pPortal) { return static_cast<const Ware*>(pPortal)->marked; };
	return host;
}();

const BonusSkills::Host skills = []() {
	BonusSkills::Host host{};
	host.RefreshAvailableSkills = []() { calls.push_back("skills refreshed"); };
	return host;
}();

// The game connection, its output kept unsent for inspection.
struct GamePlayer : ClientPlayer
{
	GamePlayer() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~GamePlayer() { setPlayerStatus(CPS_END_SESSION); }
	std::vector<unsigned char> Sent() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};

std::vector<unsigned char> Told(TYPE_OBJECTID id, int slot)
{
	CGAddMouseToQuickSlot told;
	told.setObjectID(id);
	told.setSlotID(static_cast<SlotID_t>(slot));
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	encoded.write(&told);
	return SocketOutputStreamTestAccess::Bytes(encoded);
}

const ITEM_CLASS kClasses[] = {ITEM_CLASS_SKULL, ITEM_CLASS_MONEY, ITEM_CLASS_POTION, ITEM_CLASS_HOLYWATER, ITEM_CLASS_SLAYER_PORTAL_ITEM,
	ITEM_CLASS_BOMB_MATERIAL, ITEM_CLASS_BOMB, ITEM_CLASS_MINE, ITEM_CLASS_VAMPIRE_PORTAL_ITEM, ITEM_CLASS_VAMPIRE_ETC, ITEM_CLASS_BELT};
TYPE_SOUNDID TileSound(ITEM_CLASS itemClass) { return static_cast<TYPE_SOUNDID>(100 + itemClass); }
TYPE_SOUNDID InventorySound(ITEM_CLASS itemClass) { return static_cast<TYPE_SOUNDID>(200 + itemClass); }
ACTIONINFO UseAction(ITEM_CLASS itemClass) { return static_cast<ACTIONINFO>(MAGIC_THROW_HOLY_WATER + 50 + itemClass); }

struct World : GameModelWorld
{
	MInventory inventory;
	MMoneyManager money;
	TempInformation temp;
	MSkillSet available;
	MBelt belt;
	MOustersArmsBand band1, band2;
	GamePlayer connection;
	MInventory* oldInventory = g_pInventory;
	MMoneyManager* oldMoney = g_pMoneyManager;
	TempInformation* oldTemp = g_pTempInformation;
	MSkillSet* oldAvailable = g_pSkillAvailable;
	MBelt* oldQuickSlot = g_pQuickSlot;
	MOustersArmsBand* oldBand1 = g_pArmsBand1;
	MOustersArmsBand* oldBand2 = g_pArmsBand2;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	const GameInfoPresentation::Host* oldInfo = GameInfoPresentation::SetHost(&info);
	const BonusSkills::Host* oldSkills = BonusSkills::SetHost(&skills);
	World()
	{
		calls.clear();
		zoneExists = true;
		race = RACE_SLAYER;
		ousters = false;
		bufferItem = nullptr;
		mouse = nullptr;
		ground.clear();
		for (ITEM_CLASS itemClass : kClasses)
		{
			g_pItemTable->InitClass(itemClass, 20);
			for (int type = 0; type < 20; type++)
			{
				ITEMTABLE_INFO& row = testfw::MutableRow(*g_pItemTable, itemClass, type);
				row.SetGrid(1, 1);
				row.TileSoundID = TileSound(itemClass);
				row.InventorySoundID = InventorySound(itemClass);
				row.UseActionInfo = UseAction(itemClass);
			}
		}
		inventory.Init(4, 4);
		static_cast<MSlotItemManager&>(belt).Init(4);
		static_cast<MSlotItemManager&>(band1).Init(3);
		static_cast<MSlotItemManager&>(band2).Init(3);
		g_pInventory = &inventory;
		g_pMoneyManager = &money;
		g_pTempInformation = &temp;
		g_pSkillAvailable = &available;
		g_pQuickSlot = &belt;
		g_pArmsBand1 = nullptr;
		g_pArmsBand2 = nullptr;
		MItem::SetHost(&items);
	}
	~World()
	{
		BonusSkills::SetHost(oldSkills);
		GameInfoPresentation::SetHost(oldInfo);
		WorldEvents::SetHost(oldEvents);
		DialogPresentation::SetHost(oldDialog);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		PlayerState::SetHost(oldPlayer);
		g_pArmsBand2 = oldBand2;
		g_pArmsBand1 = oldBand1;
		g_pQuickSlot = oldQuickSlot;
		g_pSkillAvailable = oldAvailable;
		g_pTempInformation = oldTemp;
		g_pMoneyManager = oldMoney;
		g_pInventory = oldInventory;
		inventory.Release();
		delete mouse;
		mouse = nullptr;
		bufferItem = nullptr;
		ground.clear();
	}
	void Receive(TYPE_OBJECTID id)
	{
		GCDeleteandPickUpOK packet;
		packet.setObjectID(id);
		GCDeleteandPickUpOKHandler::execute(&packet, &connection);
	}
	void WaitingForTheQuickSlot(int slot)
	{
		temp.SetMode(TempInformation::MODE_TRADE_VERIFY_PICKUP_TO_QUICKSLOT);
		temp.Value1 = slot;
	}
};

const std::string kGetItem = "help|" + N(HELP_EVENT_GET_ITEM);

std::vector<std::string> Then(std::vector<std::string> first, const std::vector<std::string>& rest)
{
	first.insert(first.end(), rest.begin(), rest.end());
	return first;
}

} // namespace

TEST(PickupReplies, WithNothingInTheBufferNothingFollows)
{
	World world;
	world.Receive(2001);
	CHECK(calls == std::vector<std::string>{"cleared"});
}

TEST(PickupReplies, AnotherItemsIDOnlyHelps)
{
	World world;
	Picking(new Ware(ITEM_CLASS_POTION, 2001), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE);
	world.Receive(2002);
	CHECK(calls == (std::vector<std::string>{"cleared", kGetItem}));
	calls.clear();
	Picking(new Ware(ITEM_CLASS_BELT, 2003), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE);
	world.Receive(2004);
	CHECK(calls == (std::vector<std::string>{"cleared", "help|" + N(HELP_EVENT_GET_BELT)}));
	CHECK(ground.size() == 2);
}

TEST(PickupReplies, EventSkullsLeaveTheZoneAtOnce)
{
	World world;
	for (TYPE_ITEMTYPE type : {12, 16})
	{
		calls.clear();
		Picking(new Ware(ITEM_CLASS_SKULL, 2001 + type, type), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY);
		world.Receive(2001 + type);
		CHECK(calls == (std::vector<std::string>{"cleared", "sound|" + N(InventorySound(ITEM_CLASS_SKULL)), "zone removes|" + N(2001 + type), kGetItem}));
	}
	// Another class's type 12 is no event item.
	calls.clear();
	Picking(new Ware(ITEM_CLASS_POTION, 2020, 12), PlayerState::ITEM_CHECK_BUFFER_PICKUP_FROM_INVENTORY);
	world.Receive(2020);
	CHECK(calls == (std::vector<std::string>{"cleared", kGetItem}));
	// Another skull is picked up as the buffer says.
	for (TYPE_ITEMTYPE type : {11, 17})
	{
		calls.clear();
		Picking(new Ware(ITEM_CLASS_SKULL, 2001 + type, type), PlayerState::ITEM_CHECK_BUFFER_PICKUP_FROM_INVENTORY);
		world.Receive(2001 + type);
		CHECK(calls == (std::vector<std::string>{"cleared", kGetItem}));
	}
	CHECK(world.inventory.GetItemNum() == 0);
}

TEST(PickupReplies, MoneyIsAddedUpToTheLimit)
{
	World world;
	MItem* pMoney = Picking(new Ware(ITEM_CLASS_MONEY, 2001), PlayerState::ITEM_CHECK_BUFFER_PICKUP_MONEY);
	pMoney->SetCurrentDurability(5);
	pMoney->SetSilver(1);
	world.money.SetMoney(100);
	world.Receive(2001);
	CHECK(calls == (std::vector<std::string>{"cleared", "sound|" + N(TileSound(ITEM_CLASS_MONEY)), "zone removes|2001", kGetItem}));
	CHECK_EQ(100 + (1 << 16) + 5, world.money.GetMoney());
	// Past the limit only what fits is added.
	pMoney = Picking(new Ware(ITEM_CLASS_MONEY, 2002), PlayerState::ITEM_CHECK_BUFFER_PICKUP_MONEY);
	pMoney->SetCurrentDurability(500);
	world.money.SetMoneyLimit(world.money.GetMoney() + 70);
	world.Receive(2002);
	CHECK_EQ(world.money.GetMoney(), world.money.GetMoneyLimit());
}

TEST(PickupReplies, AnItemGoesToItsCellInTheInventory)
{
	World world;
	MItem* pItem = Picking(new Ware(ITEM_CLASS_HOLYWATER, 2001), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY, 1, 2);
	world.Receive(2001);
	CHECK(calls == (std::vector<std::string>{"cleared", "zone picks up|2001", "affect|2001", kGetItem}));
	CHECK(world.inventory.GetItem(1, 2) == pItem);
	CHECK(world.available.IsEnableSkill(MAGIC_THROW_HOLY_WATER));
	// Without a zone it is still put away, and not picked off the ground.
	calls.clear();
	zoneExists = false;
	pItem = Picking(new Ware(ITEM_CLASS_POTION, 2002), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY, 3, 3);
	world.Receive(2002);
	CHECK(calls == (std::vector<std::string>{"cleared", "affect|2002", kGetItem}));
	CHECK(world.inventory.GetItem(3, 3) == pItem);
	ground[2002].release();
	ground.erase(2002);
	// A cell outside the inventory takes nothing.
	calls.clear();
	zoneExists = true;
	Picking(new Ware(ITEM_CLASS_POTION, 2003), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY, 9, 0);
	world.Receive(2003);
	CHECK(calls == (std::vector<std::string>{"cleared", kGetItem}));
	CHECK(ground.count(2003) == 1);
}

TEST(PickupReplies, AnItemPilesOnTheSameItemInItsCell)
{
	World world;
	MItem* pOld = new Ware(ITEM_CLASS_POTION, 2001, 3, true, 10);
	pOld->SetNumber(3);
	CHECK(world.inventory.AddItem(pOld, 0, 0));
	MItem* pItem = Picking(new Ware(ITEM_CLASS_POTION, 2002, 3, true, 9), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY, 0, 0);
	pItem->SetNumber(4);
	world.Receive(2002);
	CHECK(calls == (std::vector<std::string>{"cleared", "affect|2002", "zone removes|2002", kGetItem}));
	CHECK(world.inventory.GetItem(0, 0) == pOld);
	CHECK_EQ(7, static_cast<int>(pOld->GetNumber()));
	// Up to the old item's most the pile holds the sum, past the picked
	// one's most.
	pItem = Picking(new Ware(ITEM_CLASS_POTION, 2010, 3, true, 9), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY, 0, 0);
	pItem->SetNumber(3);
	world.Receive(2010);
	CHECK_EQ(10, static_cast<int>(pOld->GetNumber()));
	// Past the old item's most, the pile holds the picked one's most.
	pItem = Picking(new Ware(ITEM_CLASS_POTION, 2003, 3, true, 9), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY, 0, 0);
	pItem->SetNumber(4);
	world.Receive(2003);
	CHECK_EQ(9, static_cast<int>(pOld->GetNumber()));
	// An old item that does not pile takes nothing.
	MItem* pSingle = new Ware(ITEM_CLASS_POTION, 2030, 3, false, 9);
	CHECK(world.inventory.AddItem(pSingle, 2, 0));
	calls.clear();
	Picking(new Ware(ITEM_CLASS_POTION, 2031, 3, true, 9), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY, 2, 0);
	world.Receive(2031);
	CHECK(calls == (std::vector<std::string>{"cleared", kGetItem}));
	CHECK_EQ(1, static_cast<int>(pSingle->GetNumber()));
	// Another type, another class or an item that does not pile: nothing.
	calls.clear();
	const std::vector<Ware*> others = {new Ware(ITEM_CLASS_POTION, 2004, 4, true, 9), new Ware(ITEM_CLASS_HOLYWATER, 2005, 3, true, 9),
		new Ware(ITEM_CLASS_POTION, 2006, 3, false, 9)};
	for (Ware* pOther : others)
	{
		Picking(pOther, PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY, 0, 0);
		world.Receive(pOther->GetID());
	}
	CHECK(calls == (std::vector<std::string>{"cleared", kGetItem, "cleared", kGetItem, "cleared", kGetItem}));
	CHECK_EQ(9, static_cast<int>(pOld->GetNumber()));
	CHECK(ground.size() == 4);
}

TEST(PickupReplies, AnItemGoesOnTheMouse)
{
	World world;
	MItem* pItem = Picking(new Ware(ITEM_CLASS_SLAYER_PORTAL_ITEM, 2001), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE);
	world.Receive(2001);
	CHECK(calls == (std::vector<std::string>{"cleared", "mouse takes|2001", "zone picks up|2001", "affect|2001", kGetItem}));
	CHECK(mouse == pItem);
	CHECK(world.available.IsEnableSkill(SUMMON_HELICOPTER));
	// Without a zone it is on the mouse, still on the ground too.
	calls.clear();
	delete mouse;
	mouse = nullptr;
	zoneExists = false;
	Picking(new Ware(ITEM_CLASS_POTION, 2002), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE);
	world.Receive(2002);
	CHECK(calls == (std::vector<std::string>{"cleared", "mouse takes|2002", "affect|2002", kGetItem}));
	mouse = nullptr;
}

TEST(PickupReplies, AnItemGoesIntoTheSlayersBeltAndTheServerIsTold)
{
	World world;
	world.WaitingForTheQuickSlot(2);
	MItem* pItem = Picking(new Ware(ITEM_CLASS_POTION, 2001, 3, true, 10), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_QUICKSLOT);
	world.Receive(2001);
	CHECK(calls == (std::vector<std::string>{"cleared", "mouse takes|2001", "zone picks up|2001", "mouse drops", "affect|2001", kGetItem}));
	CHECK(world.belt.GetItem(2) == pItem);
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	CHECK(world.connection.Sent() == Told(2001, 2));
	// The same item there takes the picked one when they fit.
	pItem->SetNumber(3);
	world.WaitingForTheQuickSlot(2);
	MItem* pMore = Picking(new Ware(ITEM_CLASS_POTION, 2002, 3, true, 10), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_QUICKSLOT);
	pMore->SetNumber(7);
	calls.clear();
	GamePlayer told;
	GCDeleteandPickUpOK packet;
	packet.setObjectID(2002);
	GCDeleteandPickUpOKHandler::execute(&packet, &told);
	CHECK(calls == (std::vector<std::string>{"cleared", "mouse takes|2002", "zone picks up|2002", "mouse drops", "affect|2001", kGetItem}));
	CHECK_EQ(10, static_cast<int>(pItem->GetNumber()));
	CHECK(told.Sent() == Told(2002, 2));
	CHECK(mouse == nullptr);
	// Past its most, or another type that would fit: it stays on the
	// mouse, untold.
	for (TYPE_ITEMTYPE type : {3, 4})
	{
		pItem->SetNumber(type == 3 ? 10 : 2);
		world.WaitingForTheQuickSlot(2);
		MItem* pTooMany = Picking(new Ware(ITEM_CLASS_POTION, 2003 + type, type, true, 10), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_QUICKSLOT);
		calls.clear();
		GamePlayer untold;
		GCDeleteandPickUpOK more;
		more.setObjectID(2003 + type);
		GCDeleteandPickUpOKHandler::execute(&more, &untold);
		CHECK(calls == (std::vector<std::string>{"cleared", "mouse takes|" + N(2003 + type), "zone picks up|" + N(2003 + type),
			"affect|" + N(2003 + type), kGetItem}));
		CHECK(mouse == pTooMany && untold.Sent().empty());
		delete mouse;
		mouse = nullptr;
	}
	CHECK_EQ(2, static_cast<int>(pItem->GetNumber()));
}

TEST(PickupReplies, AnItemGoesIntoTheOustersArmsBand)
{
	World world;
	race = RACE_OUSTERS;
	ousters = true;
	g_pQuickSlot = nullptr;
	g_pArmsBand1 = &world.band1;
	g_pArmsBand2 = &world.band2;
	world.WaitingForTheQuickSlot(4);
	MItem* pItem = Picking(new Ware(ITEM_CLASS_POTION, 2001, 3, true, 10), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_QUICKSLOT);
	world.Receive(2001);
	CHECK(world.band2.GetItem(1) == pItem);
	CHECK(world.connection.Sent() == Told(2001, 4));
	world.WaitingForTheQuickSlot(1);
	pItem = Picking(new Ware(ITEM_CLASS_POTION, 2002, 3, true, 10), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_QUICKSLOT);
	world.Receive(2002);
	CHECK(world.band1.GetItem(1) == pItem);
	world.WaitingForTheQuickSlot(3);
	pItem = Picking(new Ware(ITEM_CLASS_POTION, 2009, 3, true, 10), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_QUICKSLOT);
	world.Receive(2009);
	CHECK(world.band2.GetItem(0) == pItem);
	// A slayer without a belt does not use an ouster's bands.
	race = RACE_SLAYER;
	ousters = false;
	world.WaitingForTheQuickSlot(1);
	MItem* pSlayers = Picking(new Ware(ITEM_CLASS_POTION, 2010, 3, true, 10), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_QUICKSLOT);
	world.Receive(2010);
	CHECK(mouse == pSlayers && world.band1.GetItem(2) == nullptr);
	delete mouse;
	mouse = nullptr;
	// A player whose race reads ousters but whose creature is no ouster
	// (an NPC's tribe) does not use the bands either.
	race = RACE_OUSTERS;
	ousters = false;
	world.WaitingForTheQuickSlot(1);
	MItem* pNot = Picking(new Ware(ITEM_CLASS_POTION, 2011, 3, true, 10), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_QUICKSLOT);
	world.Receive(2011);
	CHECK(mouse == pNot && world.band1.GetItem(1) != pNot);
	delete mouse;
	mouse = nullptr;
	ousters = true;
	// Without bands, or a slayer without a belt: on the mouse, untold.
	for (Race who : {RACE_OUSTERS, RACE_SLAYER})
	{
		race = who;
		ousters = who == RACE_OUSTERS;
		g_pArmsBand1 = nullptr;
		g_pArmsBand2 = nullptr;
		world.WaitingForTheQuickSlot(1);
		MItem* pHeld = Picking(new Ware(ITEM_CLASS_POTION, 2003 + who, 3, true, 10), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_QUICKSLOT);
		GamePlayer untold;
		GCDeleteandPickUpOK packet;
		packet.setObjectID(2003 + who);
		GCDeleteandPickUpOKHandler::execute(&packet, &untold);
		CHECK(mouse == pHeld && untold.Sent().empty());
		delete mouse;
		mouse = nullptr;
	}
}

TEST(PickupReplies, TheQuickSlotWaitsOnTheTradeRegister)
{
	World world;
	Picking(new Ware(ITEM_CLASS_POTION, 2001, 3, true, 10), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_QUICKSLOT);
	world.Receive(2001);
	CHECK(calls == (std::vector<std::string>{"cleared", kGetItem}));
	CHECK(ground.count(2001) == 1 && world.connection.Sent().empty());
	// Without a zone the wait ends and the item is had, on the ground.
	calls.clear();
	zoneExists = false;
	world.WaitingForTheQuickSlot(2);
	Picking(new Ware(ITEM_CLASS_POTION, 2002, 3, true, 10), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_QUICKSLOT);
	world.Receive(2002);
	CHECK(calls == (std::vector<std::string>{"cleared", "affect|2002", kGetItem}));
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	CHECK(world.belt.GetItem(2) == nullptr && world.connection.Sent().empty());
}

TEST(PickupReplies, ASlayersItemsGiveItsSkills)
{
	World world;
	world.available.AddSkill(SKILL_THROW_BOMB, 0);
	world.available.AddSkill(SKILL_INSTALL_MINE, 0);
	const std::pair<ITEM_CLASS, std::vector<std::string>> refreshes[] = {
		{ITEM_CLASS_BOMB_MATERIAL, {"skills refreshed"}}, {ITEM_CLASS_BOMB, {"skills refreshed"}}, {ITEM_CLASS_MINE, {"skills refreshed"}}};
	TYPE_OBJECTID id = 2001;
	for (const auto& [itemClass, extra] : refreshes)
	{
		calls.clear();
		Picking(new Ware(itemClass, ++id), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE);
		world.Receive(id);
		CHECK(calls == Then(Then({"cleared", "mouse takes|" + N(id), "zone picks up|" + N(id), "affect|" + N(id)}, extra), {kGetItem}));
		delete mouse;
		mouse = nullptr;
	}
	// A bomb's use is as enabled as throwing bombs; a mine's as installing.
	CHECK(world.available.find(UseAction(ITEM_CLASS_BOMB)) != world.available.end());
	CHECK(!world.available.IsEnableSkill(UseAction(ITEM_CLASS_BOMB)));
	CHECK(world.available.find(UseAction(ITEM_CLASS_MINE)) != world.available.end());
	CHECK(!world.available.IsEnableSkill(UseAction(ITEM_CLASS_MINE)));
	world.available.AddSkill(SKILL_THROW_BOMB);
	world.available.AddSkill(SKILL_INSTALL_MINE);
	for (ITEM_CLASS itemClass : {ITEM_CLASS_BOMB, ITEM_CLASS_MINE})
	{
		Picking(new Ware(itemClass, ++id), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE);
		world.Receive(id);
		delete mouse;
		mouse = nullptr;
	}
	CHECK(world.available.IsEnableSkill(UseAction(ITEM_CLASS_BOMB)) && world.available.IsEnableSkill(UseAction(ITEM_CLASS_MINE)));
	CHECK(world.available.find(UseAction(ITEM_CLASS_BOMB_MATERIAL)) == world.available.end());
	// Not a slayer: no slayer skills.
	race = RACE_OUSTERS;
	Picking(new Ware(ITEM_CLASS_HOLYWATER, ++id), PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE);
	world.Receive(id);
	CHECK(world.available.find(MAGIC_THROW_HOLY_WATER) == world.available.end());
}

TEST(PickupReplies, AVampiresItemsGiveOnlySkillsItHas)
{
	World world;
	race = RACE_VAMPIRE;
	for (ACTIONINFO skill : {MAGIC_BLOODY_TUNNEL, MAGIC_BLOODY_MARK, MAGIC_TRANSFORM_TO_WOLF, MAGIC_TRANSFORM_TO_BAT})
		world.available.AddSkill(skill, 0);
	auto pick = [&](MItem* pItem) {
		Picking(pItem, PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE);
		world.Receive(pItem->GetID());
		delete mouse;
		mouse = nullptr;
	};
	Ware* pMarked = new Ware(ITEM_CLASS_VAMPIRE_PORTAL_ITEM, 2001);
	pMarked->marked = true;
	pick(pMarked);
	CHECK(world.available.IsEnableSkill(MAGIC_BLOODY_TUNNEL) && !world.available.IsEnableSkill(MAGIC_BLOODY_MARK));
	pick(new Ware(ITEM_CLASS_VAMPIRE_PORTAL_ITEM, 2002));
	CHECK(world.available.IsEnableSkill(MAGIC_BLOODY_MARK));
	pick(new Ware(ITEM_CLASS_VAMPIRE_ETC, 2003, 0));
	CHECK(world.available.IsEnableSkill(MAGIC_TRANSFORM_TO_WOLF) && !world.available.IsEnableSkill(MAGIC_TRANSFORM_TO_BAT));
	pick(new Ware(ITEM_CLASS_VAMPIRE_ETC, 2004, 1));
	CHECK(world.available.IsEnableSkill(MAGIC_TRANSFORM_TO_BAT));
	// Skills the vampire does not have are not given.
	world.available.clear();
	pick(new Ware(ITEM_CLASS_VAMPIRE_ETC, 2005, 0));
	pick(new Ware(ITEM_CLASS_VAMPIRE_ETC, 2006, 1));
	pick(new Ware(ITEM_CLASS_VAMPIRE_PORTAL_ITEM, 2007));
	Ware* pAgain = new Ware(ITEM_CLASS_VAMPIRE_PORTAL_ITEM, 2008);
	pAgain->marked = true;
	pick(pAgain);
	CHECK(world.available.empty());
}
