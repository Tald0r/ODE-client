#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "packet_stream_access.h"
#include "MItem.h"
#include "MInventory.h"
#include "MSlayerGear.h"
#include "MQuickSlot.h"
#include "MSkillManager.h"
#include "MActionInfoTable.h"
#include "MEffectGeneratorTable.h"
#include "MEffectGenerator.h"
#include "MEffectTarget.h"
#include "MZoneTable.h"
#include "MCreatureTable.h"
#include "MGameStringTable.h"
#include "ClientFunction.h"
#include "SkillDef.h"
#include "GameSocket.h"
#include "GameMessages.h"
#include "ZoneFlags.h"
#include "../basic/CMessageArray.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "DialogPresentationHost.h"
#include "NoticePresentationHost.h"
#include "VsUiPresentationHost.h"
#include "WorldEventsHost.h"
#include "ClientPlayer.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "Cpackets/CGAddInventoryToMouse.h"
#include "Cpackets/CGReloadFromInventory.h"
#include "Cpackets/CGReloadFromQuickSlot.h"
#include "Cpackets/CGSkillToInventory.h"
#include "Cpackets/CGUseItemFromGear.h"
#include "Cpackets/CGUseItemFromInventory.h"
#include "Cpackets/CGUsePotionFromInventory.h"
#include "Cpackets/CGUsePotionFromQuickSlot.h"

#include <cstdio>
#include <memory>
#include <set>
#include <string>
#include <vector>

// MItemUse.cpp's, reached through MItemHost.
void	UsePotionFromInventory(MItem* pPotion);
void	UsePetFromInventory(MItem* pPet);
MItem*	NewItemOfClass(ITEM_CLASS itemClass);

// Using items (task 4.237, gamemodel's since then): each use body checks
// the player, the zone and the skills, then sends a packet and arms the
// player's check buffer, or opens a window. The socket, the player, the
// zone and the windows are fakes that record each call in order.
namespace {

std::vector<std::string> calls;
std::string N(long long value) { return std::to_string(value); }

// The last skill packet's target cell (set only by the bodies that aim).
Coord_t targetX = 0, targetY = 0;

std::string Render(Packet& packet)
{
	switch (packet.getPacketID())
	{
	case Packet::PACKET_CG_SKILL_TO_INVENTORY: {
		CGSkillToInventory& p = static_cast<CGSkillToInventory&>(packet);
		targetX = p.getTargetX();
		targetY = p.getTargetY();
		return "skill|" + N(p.getObjectID()) + "|" + N(p.getX()) + "|" + N(p.getY()) + "|" + N(p.getSkillType()) + "|" + N(p.getInventoryItemObjectID());
	}
	case Packet::PACKET_CG_USE_ITEM_FROM_INVENTORY: {
		CGUseItemFromInventory& p = static_cast<CGUseItemFromInventory&>(packet);
		return "use|" + N(p.getObjectID()) + "|" + N(p.getX()) + "|" + N(p.getY()) + "|" + N(p.getInventoryItemObjectID());
	}
	case Packet::PACKET_CG_ADD_INVENTORY_TO_MOUSE: {
		CGAddInventoryToMouse& p = static_cast<CGAddInventoryToMouse&>(packet);
		return "lift|" + N(p.getObjectID()) + "|" + N(p.getX()) + "|" + N(p.getY()) + "|" + N(p.getInventoryItemObjectID());
	}
	case Packet::PACKET_CG_USE_POTION_FROM_INVENTORY: {
		CGUsePotionFromInventory& p = static_cast<CGUsePotionFromInventory&>(packet);
		return "potion|" + N(p.getObjectID()) + "|" + N(p.getX()) + "|" + N(p.getY());
	}
	case Packet::PACKET_CG_USE_POTION_FROM_QUICKSLOT: {
		CGUsePotionFromQuickSlot& p = static_cast<CGUsePotionFromQuickSlot&>(packet);
		return "quick potion|" + N(p.getObjectID()) + "|" + N(p.getSlotID());
	}
	case Packet::PACKET_CG_RELOAD_FROM_INVENTORY: {
		CGReloadFromInventory& p = static_cast<CGReloadFromInventory&>(packet);
		return "reload|" + N(p.getObjectID()) + "|" + N(p.getX()) + "|" + N(p.getY());
	}
	case Packet::PACKET_CG_RELOAD_FROM_QUICKSLOT: {
		CGReloadFromQuickSlot& p = static_cast<CGReloadFromQuickSlot&>(packet);
		return "quick reload|" + N(p.getObjectID()) + "|" + N(p.getSlotID());
	}
	case Packet::PACKET_CG_USE_ITEM_FROM_GEAR: {
		CGUseItemFromGear& p = static_cast<CGUseItemFromGear&>(packet);
		return "gear|" + N(p.getObjectID()) + "|" + N(p.getPart());
	}
	default:
		return "packet|" + N(packet.getPacketID());
	}
}

// The game server's connection: it keeps what is sent.
struct Wire : ClientPlayer
{
	Wire() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~Wire() { setPlayerStatus(CPS_END_SESSION); }
	void sendPacket(Packet* pPacket) override { calls.push_back(Render(*pPacket)); }
};

TYPE_OBJECTID playerID = 77;
Race race = RACE_SLAYER;
bool ousters = false;
bool noPlayer = false;
PlayerState::WAIT_VERIFY waiting = PlayerState::WAIT_VERIFY_NULL;
MItem* checkBuffer = NULL;
std::set<int> effects;
int safeSector = 0;
int creatureType = CREATURETYPE_SLAYER_MALE;
TYPE_OBJECTID petID = OBJECTID_NULL;
CreatureState::MOVE_DEVICE device = CreatureState::MOVE_DEVICE_WALK;
bool standing = true;
int direction = 5;
bool zoneExists = true;
int zoneID = 60;
bool helicopter = false;
bool flagAround = false;
MObject playerObject;
MCreature* PlayerCreature() { return reinterpret_cast<MCreature*>(&playerObject); }
// The player the creature calls are about: none without one.
bool IsPlayer(MCreature* pCreature) { return pCreature == (noPlayer ? NULL : PlayerCreature()); }

const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.GetID = []() { return playerID; };
	host.GetRace = []() { return race; };
	host.Creature = []() { return noPlayer ? static_cast<MCreature*>(NULL) : PlayerCreature(); };
	host.GetWaitVerify = []() { return waiting; };
	host.GetItemCheckBuffer = []() { return checkBuffer; };
	host.HasEffectStatus = [](EFFECTSTATUS status) { return effects.count(status) > 0; };
	host.IsInSafeSector = []() { return safeSector; };
	host.SetItemCheckBuffer = [](MItem* pItem, PlayerState::ITEM_CHECK_BUFFER status) { calls.push_back("buffer|" + N(pItem->GetID()) + "|" + N(status)); };
	host.SetWaitVerify = [](PlayerState::WAIT_VERIFY verify) { calls.push_back("verify|" + N(verify)); };
	return host;
}();
const CreatureState::Host creatureHost = []() {
	CreatureState::Host host{};
	host.IsOusters = [](MCreature* pCreature) { CHECK(IsPlayer(pCreature)); return ousters; };
	host.GetCreatureType = [](MCreature* pCreature) { CHECK(IsPlayer(pCreature)); return creatureType; };
	host.GetPetID = [](MCreature* pCreature) { CHECK(IsPlayer(pCreature)); return petID; };
	host.GetMoveDevice = [](MCreature* pCreature) { CHECK(IsPlayer(pCreature)); return device; };
	host.IsStop = [](MCreature* pCreature) { CHECK(IsPlayer(pCreature)); return standing; };
	host.GetDirection = [](MCreature* pCreature) { CHECK(IsPlayer(pCreature)); return direction; };
	return host;
}();
const ZoneState::Host zoneHost = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetID = []() { return zoneID; };
	host.HasHelicopter = [](TYPE_OBJECTID id) { CHECK_EQ(playerID, id); return helicopter; };
	host.IsExistCorpseFromPlayer = [](MCreature* pCreature, int type) {
		CHECK(IsPlayer(pCreature));
		calls.push_back("look for|" + N(type));
		return flagAround;
	};
	host.DeleteCreature = [](MCreature* pCreature) { calls.push_back("delete creature|" + N(reinterpret_cast<MObject*>(pCreature)->GetID())); };
	return host;
}();
const DialogPresentation::Host dialogHost = []() {
	DialogPresentation::Host host{};
	host.PopupFreeMessageDlg = [](const char* message) { calls.push_back(std::string("dialog|") + (message != NULL ? message : "(null)")); };
	return host;
}();
const NoticePresentation::Host noticeHost = []() {
	NoticePresentation::Host host{};
	host.PopupMessage = [](int stringID) { calls.push_back("popup|" + N(stringID)); };
	return host;
}();
const VsUiPresentation::Host uiHost = []() {
	VsUiPresentation::Host host{};
	host.LockGear = []() { calls.push_back("lock gear"); };
	host.RunMixingForge = [](int forgeClass, int forgeType) { calls.push_back("forge|" + N(forgeClass) + "|" + N(forgeType)); };
	host.RunBulletinBoardWindow = [](const MItem* pItem) { calls.push_back("board|" + N(pItem->GetID())); };
	host.RunXmasCardWindow = [](const MItem* pItem) { calls.push_back("xmas card|" + N(pItem->GetID())); };
	host.RunQuestInventory = [](const MItem* pItem) { calls.push_back("quest inventory|" + N(pItem->GetID())); };
	return host;
}();
const WorldEvents::Host worldHost = []() {
	WorldEvents::Host host{};
	host.PlaySound = [](TYPE_SOUNDID soundID) { calls.push_back("sound|" + N(soundID)); };
	return host;
}();

MonotonicClock::TimePoint now = MonotonicClock::TimePoint() + MonotonicClock::Millis(100000);
const MItemHost itemHost = []() {
	MItemHost host{};
	host.pCurrentTime = &now;
	host.NewItem = NewItemOfClass;
	return host;
}();

// An item action's effect: it records the action and its length (the
// delay given, its phase being a delay node), and declines, so nothing
// is left behind.
struct FakeGenerator : MEffectGenerator
{
	TYPE_EFFECTGENERATORID GetID() override { return 7; }
	bool Generate(const EFFECTGENERATOR_INFO& info) override
	{
		calls.push_back("effect|" + N(info.nActionInfo) + "|" + N(info.count));
		return false;
	}
};

struct Generators : MEffectGeneratorTable
{
	void Add(MEffectGenerator* generator) { AddEffectGenerator(generator); }
};

std::string S(int stringID)
{
	const char* text = (*g_pGameStringTable)[stringID].GetString();
	return text != NULL ? text : "(null)";
}

std::vector<std::string> Lines(CMessageArray& log)
{
	std::vector<std::string> lines;
	for (int i = 0; i < log.GetSize(); ++i)
		if (log[i][0] != '\0') lines.push_back(log[i]);
	return lines;
}

struct Scene
{
	GameModelWorld world;
	Wire wire;
	CMessageArray systemLog;
	MSkillInfoTable info;
	MSkillSet available;
	MActionInfoTable actions;
	Generators generators;
	MInventory inventory;
	MSlayerGear slayerGear;
	CZoneTable zones;
	ClientPlayer* oldSocket = g_pSocket;
	CMessageArray* oldSystem = g_pSystemMessage;
	MSkillInfoTable* oldInfo = g_pSkillInfoTable;
	MSkillSet* oldAvailable = g_pSkillAvailable;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	MEffectGeneratorTable* oldGenerators = g_pEffectGeneratorTable;
	MInventory* oldInventory = g_pInventory;
	MSlayerGear* oldSlayerGear = g_pSlayerGear;
	MOustersArmsBand* oldArmsBand = g_pArmsBand1;
	CZoneTable* oldZones = g_pZoneTable;
	bool oldHolyLand = g_bHolyLand;
	bool oldZoneSafe = g_bZoneSafe;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&playerHost);
	const CreatureState::Host* oldCreatures = CreatureState::SetHost(&creatureHost);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zoneHost);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialogHost);
	const NoticePresentation::Host* oldNotice = NoticePresentation::SetHost(&noticeHost);
	const VsUiPresentation::Host* oldUi = VsUiPresentation::SetHost(&uiHost);
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&worldHost);
	const MEffectTargetHost* oldTarget = MEffectTarget::SetHost(&targetHost);

	static inline const MEffectTargetHost targetHost{};

	Scene()
	{
		InitGameStringTable();
		calls.clear();
		targetX = targetY = 0;
		playerID = 77;
		race = RACE_SLAYER;
		ousters = false;
		noPlayer = false;
		waiting = PlayerState::WAIT_VERIFY_NULL;
		checkBuffer = NULL;
		effects.clear();
		safeSector = 0;
		creatureType = CREATURETYPE_SLAYER_MALE;
		petID = OBJECTID_NULL;
		device = CreatureState::MOVE_DEVICE_WALK;
		standing = true;
		direction = 5;
		zoneExists = true;
		zoneID = 60;
		helicopter = false;
		flagAround = false;
		g_pSocket = &wire;
		systemLog.Init(8, 256);
		g_pSystemMessage = &systemLog;
		info.CTypeTable<SKILLINFO_NODE>::Init(MAX_ACTIONINFO);
		g_pSkillInfoTable = &info;
		g_pSkillAvailable = &available;
		actions.Init(MAX_ACTIONINFO);
		g_pActionInfoTable = &actions;
		generators.Add(new FakeGenerator);
		g_pEffectGeneratorTable = &generators;
		inventory.Init(6, 4);
		g_pInventory = &inventory;
		slayerGear.Init();
		g_pSlayerGear = &slayerGear;
		g_pArmsBand1 = NULL;
		ZONETABLE_INFO* pZone = new ZONETABLE_INFO;
		pZone->ID = 60;
		pZone->CannotUseSpecialItem = false;
		zones.Add(pZone);
		g_pZoneTable = &zones;
		g_bHolyLand = false;
		g_bZoneSafe = false;
		g_pClientConfig->FPS = 10;
		MItem::SetHost(&itemHost);
	}
	~Scene()
	{
		MItem::SetHost(NULL);
		MEffectTarget::SetHost(oldTarget);
		WorldEvents::SetHost(oldWorld);
		VsUiPresentation::SetHost(oldUi);
		NoticePresentation::SetHost(oldNotice);
		DialogPresentation::SetHost(oldDialog);
		ZoneState::SetHost(oldZone);
		CreatureState::SetHost(oldCreatures);
		PlayerState::SetHost(oldPlayer);
		g_bZoneSafe = oldZoneSafe;
		g_bHolyLand = oldHolyLand;
		g_pZoneTable = oldZones;
		g_pArmsBand1 = oldArmsBand;
		g_pSlayerGear = oldSlayerGear;
		g_pInventory = oldInventory;
		g_pEffectGeneratorTable = oldGenerators;
		g_pActionInfoTable = oldActions;
		g_pSkillAvailable = oldAvailable;
		g_pSkillInfoTable = oldInfo;
		g_pSystemMessage = oldSystem;
		g_pSocket = oldSocket;
		inventory.Release();
		slayerGear.Release();
	}
	// An item class whose rows are each one cell.
	void Rows(ITEM_CLASS itemClass, int types)
	{
		g_pItemTable->InitClass(itemClass, types);
		for (int type = 0; type < types; ++type)
			testfw::MutableRow(*g_pItemTable, itemClass, type).SetGrid(1, 1);
	}
	// The item in the inventory at (x, y), which owns it from then on.
	template <class T>
	T* Put(TYPE_OBJECTID id, int x, int y, TYPE_ITEMTYPE type = 0, int number = 1)
	{
		T* pItem = new T;
		pItem->SetID(id);
		pItem->SetItemType(type);
		pItem->SetNumber(number);
		if ((*g_pItemTable)[pItem->GetItemClass()].GetSize() <= int(type))
			Rows(pItem->GetItemClass(), type + 1 > 10 ? type + 1 : 10);
		CHECK(inventory.AddItem(pItem, x, y));
		return pItem;
	}
	// A skill the player has, usable now, with a delay once used (one
	// under 1800 milliseconds counts as none).
	void Learn(ACTIONINFO skill, DWORD delay = 5000)
	{
		available.AddSkill(skill);
		info.GetMutable(skill)->SetDelayTime(delay);
	}
	bool Ready(ACTIONINFO skill) const { return info[skill].IsAvailableTime(); }
	// An item action with one phase, by the fake generator.
	void Effect(TYPE_ACTIONINFO action)
	{
		MActionInfo* pInfo = actions.GetMutable(action);
		pInfo->Set("action", 0, 0, 1, FLAG_ACTIONINFO_TARGET_ITEM);
		pInfo->Init(1);
		pInfo->GetMutable(0)->EffectGeneratorID = 7;
		pInfo->GetMutable(0)->bDelayNode = true;
	}
};

using Calls = std::vector<std::string>;

// Whether the calls are these, printing both when they are not.
bool Same(const Calls& expected)
{
	if (calls == expected) return true;
	std::printf("      calls: ");
	for (const std::string& call : calls) std::printf("[%s] ", call.c_str());
	std::printf("\n      wanted:");
	for (const std::string& call : expected) std::printf("[%s] ", call.c_str());
	std::printf("\n");
	return false;
}

std::string Buffer(TYPE_OBJECTID id, PlayerState::ITEM_CHECK_BUFFER status) { return "buffer|" + N(id) + "|" + N(status); }
const std::string SkillVerify = "verify|" + N(PlayerState::WAIT_VERIFY_SKILL_SUCCESS);

} // namespace

//----------------------------------------------------------------------
// The factory and the plain values
//----------------------------------------------------------------------
TEST(ItemUse, TheFactoryMakesEachClass)
{
	Scene scene;
	for (ITEM_CLASS itemClass : {ITEM_CLASS_MOTORCYCLE, ITEM_CLASS_MAGAZINE, ITEM_CLASS_CORPSE, ITEM_CLASS_MOON_CARD, ITEM_CLASS_TRAP_ITEM})
	{
		std::unique_ptr<MItem> pItem(NewItemOfClass(itemClass));
		CHECK(pItem != nullptr);
		if (pItem) CHECK_EQ(itemClass, pItem->GetItemClass());
	}
	CHECK(NewItemOfClass(ITEM_CLASS(-1)) == NULL);
	CHECK(NewItemOfClass(ITEM_CLASS(MAX_ITEM_CLASS)) == NULL);
}

TEST(ItemUse, ACorpseFreesItsCreatureThroughTheZone)
{
	Scene scene;
	MObject creature;
	creature.SetID(31);
	{
		MCorpse corpse;
		corpse.SetCreature(reinterpret_cast<MCreature*>(&creature));
	}
	CHECK(Same(Calls{"delete creature|31"}));
	{
		MCorpse empty;
	}
	CHECK_EQ(1u, calls.size());
}

TEST(ItemUse, AMagazineFitsItsGunOrItsOwnKind)
{
	Scene scene;
	scene.Rows(ITEM_CLASS_MAGAZINE, 2);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MAGAZINE, 1).Value1 = ITEM_CLASS_SG;
	MMagazine magazine;
	magazine.SetItemType(1);
	MGunSG shotgun;
	MGunAR rifle;
	CHECK(magazine.IsInsertToItem(&shotgun));
	CHECK(!magazine.IsInsertToItem(&rifle));
	CHECK(!magazine.IsInsertToItem(NULL));
	MMagazine same, other;
	same.SetItemType(1);
	other.SetItemType(0);
	// Anything else as any pile: the same kind with room left.
	CHECK(magazine.IsInsertToItem(&same));
	CHECK(!magazine.IsInsertToItem(&other));
}

TEST(ItemUse, AVampirePortalHoldsItsMarkInItsValues)
{
	Scene scene;
	MVampirePortalItem portal;
	CHECK(!portal.IsMarked());
	portal.SetSilver(1003);
	portal.SetCurrentDurability((40 << 8) | 200);
	portal.SetEnchantLevel(4);
	CHECK(portal.IsMarked());
	CHECK_EQ(1003, portal.GetZoneID());
	CHECK_EQ(40, portal.GetZoneX());
	CHECK_EQ(200, portal.GetZoneY());
	CHECK_EQ(4, portal.GetNumber());
	portal.SetZone(0, 1, 2);
	CHECK(!portal.IsMarked());
	portal.SetZone(OBJECTID_NULL, 1, 2);
	CHECK(!portal.IsMarked());
	MSlayerPortalItem slayerPortal;
	slayerPortal.SetEnchantLevel(9);
	CHECK_EQ(9, slayerPortal.GetNumber());
}

//----------------------------------------------------------------------
// Making things from an item
//----------------------------------------------------------------------
TEST(ItemUse, TheMadeItemGoesWhereItFitsOrInThePlaceOfTheLast)
{
	Scene scene;
	scene.Rows(ITEM_CLASS_POTION, 4);
	MWater* pWater = scene.Put<MWater>(50, 3, 2, 3, 2);
	POINT fit = {9, 9};
	// Room: the first free cell.
	CHECK(GetMakeItemFitPosition(pWater, ITEM_CLASS_POTION, 1, fit));
	CHECK_EQ(0, fit.x);
	CHECK_EQ(0, fit.y);
	// The last one of the stack: where it is.
	pWater->SetNumber(1);
	CHECK(GetMakeItemFitPosition(pWater, ITEM_CLASS_POTION, 1, fit));
	CHECK_EQ(3, fit.x);
	CHECK_EQ(2, fit.y);
	// Unless a pile of the same takes it.
	MPotion* pPile = scene.Put<MPotion>(70, 1, 0, 1, 1);
	CHECK(GetMakeItemFitPosition(pWater, ITEM_CLASS_POTION, 1, fit));
	CHECK_EQ(1, fit.x);
	CHECK_EQ(0, fit.y);
	CHECK(scene.inventory.RemoveItem(70) == pPile);
	delete pPile;
	// No room: only the last one changes where it is.
	for (int y = 0; y < 4; ++y)
		for (int x = 0; x < 6; ++x)
			if (x != 3 || y != 2) scene.Put<MWater>(100 + y * 6 + x, x, y);
	fit = {9, 9};
	CHECK(GetMakeItemFitPosition(pWater, ITEM_CLASS_POTION, 1, fit));
	CHECK_EQ(3, fit.x);
	CHECK_EQ(2, fit.y);
	pWater->SetNumber(2);
	fit = {9, 9};
	CHECK(!GetMakeItemFitPosition(pWater, ITEM_CLASS_POTION, 1, fit));
	// A class with no item: nowhere.
	CHECK(!GetMakeItemFitPosition(pWater, ITEM_CLASS(MAX_ITEM_CLASS), 0, fit));
}

TEST(ItemUse, WaterMakesHolyWaterWhereItIs)
{
	Scene scene;
	scene.Learn(MAGIC_CREATE_HOLY_WATER);
	scene.Effect(MAGIC_CREATE_HOLY_WATER);
	MWater* pWater = scene.Put<MWater>(50, 3, 2, 2, 5);
	pWater->UseInventory();
	CHECK(Same(Calls{"skill|50|3|2|" + N(MAGIC_CREATE_HOLY_WATER) + "|0", SkillVerify,
		Buffer(50, PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY), "effect|" + N(MAGIC_CREATE_HOLY_WATER) + "|30"}));
	CHECK_EQ(3, targetX);
	CHECK_EQ(2, targetY);
	CHECK(!scene.Ready(MAGIC_CREATE_HOLY_WATER));
}

TEST(ItemUse, WaterMakesAHolyPotionWhereItFits)
{
	Scene scene;
	scene.Learn(SKILL_CREATE_HOLY_POTION);
	scene.Effect(SKILL_CREATE_HOLY_POTION);
	// The water's row names the potion it makes: type 2, one cell. Type 0
	// takes four.
	scene.Rows(ITEM_CLASS_POTION, 4);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_POTION, 0).SetGrid(2, 2);
	MWater* pWater = scene.Put<MWater>(50, 3, 2, 6, 5);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_WATER, 6).Value3 = 2;
	// Only (5, 3) is free.
	for (int y = 0; y < 4; ++y)
		for (int x = 0; x < 6; ++x)
			if ((x != 3 || y != 2) && (x != 5 || y != 3)) scene.Put<MWater>(100 + y * 6 + x, x, y);
	pWater->UseInventory();
	CHECK(Same(Calls{"skill|50|3|2|" + N(SKILL_CREATE_HOLY_POTION) + "|0", SkillVerify,
		Buffer(50, PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY), "effect|" + N(SKILL_CREATE_HOLY_POTION) + "|30"}));
	CHECK_EQ(5, targetX);
	CHECK_EQ(3, targetY);
	CHECK(!scene.Ready(SKILL_CREATE_HOLY_POTION));
	// No room for the potion: nothing.
	calls.clear();
	scene.info.GetMutable(SKILL_CREATE_HOLY_POTION)->SetAvailableTime();
	scene.Put<MWater>(99, 5, 3);
	pWater->UseInventory();
	CHECK(Same({}));
}

TEST(ItemUse, WaterNeedsItsSkillTimeAndNoPendingCheck)
{
	Scene scene;
	scene.Effect(MAGIC_CREATE_HOLY_WATER);
	MWater* pWater = scene.Put<MWater>(50, 3, 2, 0, 5);
	// Not learned.
	pWater->UseInventory();
	CHECK(Same({}));
	// Learned, but not yet usable again.
	scene.Learn(MAGIC_CREATE_HOLY_WATER);
	scene.info.GetMutable(MAGIC_CREATE_HOLY_WATER)->SetNextAvailableTime();
	pWater->UseInventory();
	CHECK(Same({}));
	scene.info.GetMutable(MAGIC_CREATE_HOLY_WATER)->SetAvailableTime();
	// Waiting on the server, or an item in the check buffer.
	waiting = PlayerState::WAIT_VERIFY_ITEM_USE;
	pWater->UseInventory();
	waiting = PlayerState::WAIT_VERIFY_NULL;
	checkBuffer = pWater;
	pWater->UseInventory();
	CHECK(Same({}));
	checkBuffer = NULL;
	// A water of type 7 or past makes nothing.
	pWater->SetItemType(7);
	scene.Learn(SKILL_CREATE_HOLY_POTION);
	pWater->UseInventory();
	CHECK(Same({}));
	// Types 0 to 2 are holy water, 3 to 6 holy potions.
	for (TYPE_ITEMTYPE type : {0, 1, 2})
	{
		calls.clear();
		scene.info.GetMutable(MAGIC_CREATE_HOLY_WATER)->SetAvailableTime();
		pWater->SetItemType(type);
		pWater->UseInventory();
		CHECK(!calls.empty() && calls[0] == "skill|50|3|2|" + N(MAGIC_CREATE_HOLY_WATER) + "|0");
	}
	scene.Rows(ITEM_CLASS_POTION, 4);
	for (TYPE_ITEMTYPE type : {3, 6})
	{
		calls.clear();
		scene.info.GetMutable(SKILL_CREATE_HOLY_POTION)->SetAvailableTime();
		pWater->SetItemType(type);
		pWater->UseInventory();
		CHECK(!calls.empty() && calls[0] == "skill|50|3|2|" + N(SKILL_CREATE_HOLY_POTION) + "|0");
	}
}

TEST(ItemUse, BombMaterialMakesABombOrAMine)
{
	Scene scene;
	scene.Learn(SKILL_MAKE_BOMB);
	scene.Learn(SKILL_MAKE_MINE);
	scene.Effect(SKILL_MAKE_BOMB);
	scene.Effect(SKILL_MAKE_MINE);
	scene.Rows(ITEM_CLASS_BOMB, 5);
	scene.Rows(ITEM_CLASS_MINE, 5);
	// Types 0 to 4 make bombs, 5 and past mines (type - 5).
	MBombMaterial* pMaterial = scene.Put<MBombMaterial>(50, 3, 2, 4, 1);
	pMaterial->UseInventory();
	CHECK(Same(Calls{"skill|50|3|2|" + N(SKILL_MAKE_BOMB) + "|0", SkillVerify,
		Buffer(50, PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY), "effect|" + N(SKILL_MAKE_BOMB) + "|30"}));
	// The last one: made where it is.
	CHECK_EQ(3, targetX);
	CHECK_EQ(2, targetY);
	// The skill's next time is left to the server.
	CHECK(scene.Ready(SKILL_MAKE_BOMB));
	calls.clear();
	pMaterial->SetItemType(5);
	pMaterial->SetNumber(3);
	pMaterial->UseInventory();
	CHECK(Same(Calls{"skill|50|3|2|" + N(SKILL_MAKE_MINE) + "|0", SkillVerify,
		Buffer(50, PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY), "effect|" + N(SKILL_MAKE_MINE) + "|30"}));
	CHECK_EQ(0, targetX);
	CHECK_EQ(0, targetY);
}

TEST(ItemUse, BombMaterialNeedsItsSkillRoomAndNoPendingCheck)
{
	Scene scene;
	scene.Rows(ITEM_CLASS_MINE, 5);
	MBombMaterial* pMaterial = scene.Put<MBombMaterial>(50, 3, 2, 9, 2);
	pMaterial->UseInventory();
	CHECK(Same({}));
	scene.Learn(SKILL_MAKE_MINE);
	scene.info.GetMutable(SKILL_MAKE_MINE)->SetNextAvailableTime();
	pMaterial->UseInventory();
	CHECK(Same({}));
	scene.info.GetMutable(SKILL_MAKE_MINE)->SetAvailableTime();
	waiting = PlayerState::WAIT_VERIFY_ITEM_USE;
	pMaterial->UseInventory();
	waiting = PlayerState::WAIT_VERIFY_NULL;
	checkBuffer = pMaterial;
	pMaterial->UseInventory();
	checkBuffer = NULL;
	CHECK(Same({}));
	// Type 9 makes mine 4.
	pMaterial->UseInventory();
	CHECK(!calls.empty() && calls[0] == "skill|50|3|2|" + N(SKILL_MAKE_MINE) + "|0");
	// No room, and more than one left: nothing.
	calls.clear();
	for (int y = 0; y < 4; ++y)
		for (int x = 0; x < 6; ++x)
			if (x != 3 || y != 2) scene.Put<MWater>(100 + y * 6 + x, x, y);
	pMaterial->UseInventory();
	CHECK(Same({}));
}

//----------------------------------------------------------------------
// Guns
//----------------------------------------------------------------------
TEST(ItemUse, AMagazineReloadsTheGunInHand)
{
	Scene scene;
	scene.Rows(ITEM_CLASS_MAGAZINE, 3);
	scene.Rows(ITEM_CLASS_SG, 1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SG, 0).Race = FLAG_RACE_SLAYER;
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MAGAZINE, 1).Value1 = ITEM_CLASS_SG;
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MAGAZINE, 2).Value1 = ITEM_CLASS_AR;
	MMagazine* pMagazine = scene.Put<MMagazine>(50, 3, 2, 1, 4);
	// No gun, or a sword: nothing.
	pMagazine->UseInventory();
	CHECK(Same({}));
	scene.Rows(ITEM_CLASS_SWORD, 1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).Race = FLAG_RACE_SLAYER;
	MSword* pSword = new MSword;
	pSword->SetID(61);
	CHECK(scene.slayerGear.AddItem(pSword, MSlayerGear::GEAR_SLAYER_RIGHTHAND));
	pMagazine->UseInventory();
	CHECK(Same({}));
	CHECK(scene.slayerGear.RemoveItem(MSlayerGear::GEAR_SLAYER_RIGHTHAND) == pSword);
	delete pSword;
	MGunSG* pGun = new MGunSG;
	pGun->SetID(60);
	CHECK(scene.slayerGear.AddItem(pGun, MSlayerGear::GEAR_SLAYER_RIGHTHAND));
	pMagazine->UseInventory();
	CHECK(Same(Calls{"reload|50|3|2", Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY)}));
	// Not this gun's magazine: nothing.
	calls.clear();
	pMagazine->SetItemType(2);
	pMagazine->UseInventory();
	CHECK(Same({}));
}

TEST(ItemUse, AVividMagazineNeedsItsSkill)
{
	Scene scene;
	scene.Rows(ITEM_CLASS_MAGAZINE, 2);
	scene.Rows(ITEM_CLASS_SG, 1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SG, 0).Race = FLAG_RACE_SLAYER;
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MAGAZINE, 1).Value1 = ITEM_CLASS_SG;
	MGunSG* pGun = new MGunSG;
	pGun->SetID(60);
	CHECK(scene.slayerGear.AddItem(pGun, MSlayerGear::GEAR_SLAYER_RIGHTHAND));
	MMagazine* pMagazine = scene.Put<MMagazine>(50, 3, 2, 1, 4);
	// Value3 names the skill the magazine needs.
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MAGAZINE, 1).Value3 = SKILL_VIVID_MAGAZINE;
	pMagazine->UseInventory();
	CHECK(Same({}));
	CHECK((Lines(scene.systemLog) == std::vector<std::string>{S(UI_STRING_MESSAGE_CANNOT_RELOAD_BY_VIVID_MAGAZINE)}));
	scene.Learn(SKILL_VIVID_MAGAZINE);
	pMagazine->UseInventory();
	CHECK(Same(Calls{"reload|50|3|2", Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY)}));
	// A value past the action table never is.
	calls.clear();
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MAGAZINE, 1).Value3 = MAX_ACTIONINFO;
	pMagazine->UseInventory();
	CHECK(Same({}));
	CHECK_EQ(2u, Lines(scene.systemLog).size());
}

TEST(ItemUse, AQuickSlotMagazineReloadsAndLocksTheBelt)
{
	Scene scene;
	scene.Rows(ITEM_CLASS_MAGAZINE, 3);
	scene.Rows(ITEM_CLASS_SG, 1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SG, 0).Race = FLAG_RACE_SLAYER;
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MAGAZINE, 1).Value1 = ITEM_CLASS_SG;
	MMagazine magazine;
	magazine.SetID(50);
	magazine.SetItemType(1);
	magazine.SetItemSlot(2);
	// No gun, or a sword: nothing.
	magazine.UseQuickItem();
	CHECK(Same({}));
	scene.Rows(ITEM_CLASS_SWORD, 1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).Race = FLAG_RACE_SLAYER;
	MSword* pSword = new MSword;
	pSword->SetID(61);
	CHECK(scene.slayerGear.AddItem(pSword, MSlayerGear::GEAR_SLAYER_RIGHTHAND));
	magazine.UseQuickItem();
	CHECK(Same({}));
	CHECK(scene.slayerGear.RemoveItem(MSlayerGear::GEAR_SLAYER_RIGHTHAND) == pSword);
	delete pSword;
	MGunSG* pGun = new MGunSG;
	pGun->SetID(60);
	CHECK(scene.slayerGear.AddItem(pGun, MSlayerGear::GEAR_SLAYER_RIGHTHAND));
	magazine.UseQuickItem();
	CHECK(Same(Calls{"quick reload|50|2", Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT), "lock gear"}));
	// The wrong magazine: the empty click.
	calls.clear();
	magazine.SetItemType(2);
	magazine.UseQuickItem();
	CHECK(Same(Calls{"sound|" + N(SOUND_ITEM_NO_MAGAZINE)}));
}

//----------------------------------------------------------------------
// Splitting a stack
//----------------------------------------------------------------------
TEST(ItemUse, AStackOfTwoOrMoreSplits)
{
	Scene scene;
	const std::string lift = "lift|0|3|2|0";
	const std::string some = Buffer(50, PlayerState::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY);
	{
		MMoney* pItem = scene.Put<MMoney>(50, 3, 2, 0, 1);
		pItem->UseInventory();
		CHECK(Same({}));
		pItem->SetNumber(2);
		pItem->UseInventory();
		CHECK(Same(Calls{lift, some}));
		scene.inventory.RemoveItem(50);
		delete pItem;
	}
	{
		calls.clear();
		MEventStarItem* pItem = scene.Put<MEventStarItem>(50, 3, 2, 0, 1);
		pItem->UseInventory();
		CHECK(Same({}));
		pItem->SetNumber(2);
		pItem->UseInventory();
		CHECK(Same(Calls{lift, some}));
		scene.inventory.RemoveItem(50);
		delete pItem;
	}
	{
		calls.clear();
		MPetEnchantItem* pItem = scene.Put<MPetEnchantItem>(50, 3, 2, 0, 1);
		pItem->UseInventory();
		CHECK(Same({}));
		pItem->SetNumber(2);
		pItem->UseInventory();
		CHECK(Same(Calls{lift, some}));
		scene.inventory.RemoveItem(50);
		delete pItem;
	}
}

TEST(ItemUse, AMoonCardSplitsOnlyAsCloversBelowAFullStack)
{
	Scene scene;
	MMoonCardItem* pCard = scene.Put<MMoonCardItem>(50, 3, 2, 2, 2);
	const int full = pCard->GetMaxNumber();
	CHECK(full > 2);
	pCard->UseInventory();
	CHECK(Same(Calls{"lift|0|3|2|0", Buffer(50, PlayerState::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY)}));
	calls.clear();
	pCard->SetItemType(3);
	pCard->UseInventory();
	CHECK_EQ(2u, calls.size());
	calls.clear();
	for (TYPE_ITEMTYPE type : {1, 4})
	{
		pCard->SetItemType(type);
		pCard->UseInventory();
	}
	pCard->SetItemType(2);
	pCard->SetNumber(1);
	pCard->UseInventory();
	pCard->SetNumber(full);
	pCard->UseInventory();
	CHECK(Same({}));
	pCard->SetNumber(full - 1);
	pCard->UseInventory();
	CHECK_EQ(2u, calls.size());
}

TEST(ItemUse, MixingItemsSplitOrOpenTheForge)
{
	Scene scene;
	MMixingItem* pItem = scene.Put<MMixingItem>(50, 3, 2, 0, 2);
	const std::string lift = "lift|0|3|2|0";
	const std::string some = Buffer(50, PlayerState::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY);
	const std::string mixing = Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_MIXING_ITEM);
	// Types 0 to 8: a stack splits, one opens the forge for its class and
	// type.
	pItem->UseInventory();
	CHECK(Same(Calls{lift, some}));
	pItem->SetNumber(1);
	const int expected[9][2] = {{0, 0}, {0, 1}, {0, 2}, {1, 0}, {1, 1}, {1, 2}, {2, 0}, {2, 1}, {2, 2}};
	for (TYPE_ITEMTYPE type = 0; type <= 8; ++type)
	{
		calls.clear();
		pItem->SetItemType(type);
		pItem->UseInventory();
		CHECK(Same(Calls{"forge|" + N(expected[type][0]) + "|" + N(expected[type][1]), mixing}));
	}
	// Types 9 to 18 (Puritas) always split, even one.
	for (TYPE_ITEMTYPE type : {9, 18})
	{
		calls.clear();
		pItem->SetItemType(type);
		pItem->UseInventory();
		CHECK(Same(Calls{lift, some}));
	}
	calls.clear();
	pItem->SetItemType(19);
	pItem->UseInventory();
	CHECK(Same({}));
}

//----------------------------------------------------------------------
// Skills from items
//----------------------------------------------------------------------
TEST(ItemUse, AMineIsLaidTheWayThePlayerFaces)
{
	Scene scene;
	MMine* pMine = scene.Put<MMine>(50, 3, 2, 0, 3);
	pMine->UseInventory();
	CHECK(Same({}));
	scene.Learn(SKILL_INSTALL_MINE);
	direction = 6;
	pMine->UseInventory();
	CHECK(Same(Calls{"skill|50|3|2|" + N(SKILL_INSTALL_MINE) + "|0",
		Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY), SkillVerify,
		Buffer(50, PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY)}));
	CHECK_EQ(6, targetX);
	CHECK(!scene.Ready(SKILL_INSTALL_MINE));
	// Not again until the delay, and never on a motorcycle.
	calls.clear();
	pMine->UseInventory();
	scene.info.GetMutable(SKILL_INSTALL_MINE)->SetAvailableTime();
	device = CreatureState::MOVE_DEVICE_RIDE;
	pMine->UseInventory();
	CHECK(Same({}));
	device = CreatureState::MOVE_DEVICE_SUMMON_SYLPH;
	pMine->UseInventory();
	CHECK_EQ(4u, calls.size());
}

TEST(ItemUse, ASlayerPortalIsUsedOutsideTheHolyLand)
{
	Scene scene;
	MSlayerPortalItem* pPortal = scene.Put<MSlayerPortalItem>(50, 3, 2, 0, 2);
	pPortal->SetAffectStatus();
	const Calls used = {"use|50|3|2|0", Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY)};
	pPortal->UseInventory();
	CHECK(Same(used));
	// Each condition alone refuses.
	auto refused = [&](auto set, auto reset) {
		calls.clear();
		set();
		pPortal->UseInventory();
		CHECK(Same({}));
		reset();
		pPortal->UseInventory();
		CHECK(Same(used));
	};
	refused([] { g_bHolyLand = true; }, [] { g_bHolyLand = false; });
	refused([&] { scene.zones.Get(60)->CannotUseSpecialItem = true; }, [&] { scene.zones.Get(60)->CannotUseSpecialItem = false; });
	refused([&] { pPortal->UnSetAffectStatus(); }, [&] { pPortal->SetAffectStatus(); });
	refused([&] { pPortal->SetNumber(0); }, [&] { pPortal->SetNumber(1); });
	refused([] { helicopter = true; }, [] { helicopter = false; });
	refused([] { effects.insert(EFFECTSTATUS_INSTALL_TURRET); }, [] { effects.clear(); });
}

TEST(ItemUse, AVampirePortalMarksThenOpensATunnel)
{
	Scene scene;
	scene.Learn(MAGIC_BLOODY_MARK);
	scene.Learn(MAGIC_BLOODY_TUNNEL);
	scene.Effect(MAGIC_BLOODY_MARK);
	scene.Effect(MAGIC_BLOODY_TUNNEL);
	MVampirePortalItem* pPortal = scene.Put<MVampirePortalItem>(50, 3, 2, 0, 2);
	pPortal->SetAffectStatus();
	// Unmarked: the mark, with its motion.
	pPortal->UseInventory();
	CHECK(Same(Calls{"skill|50|3|2|" + N(MAGIC_BLOODY_MARK) + "|0", SkillVerify,
		Buffer(50, PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY), "effect|" + N(MAGIC_BLOODY_MARK) + "|14"}));
	CHECK(!scene.Ready(MAGIC_BLOODY_MARK));
	// Marked: the tunnel, without one.
	calls.clear();
	pPortal->SetZone(1003, 40, 70);
	pPortal->UseInventory();
	CHECK(Same(Calls{"skill|50|3|2|" + N(MAGIC_BLOODY_TUNNEL) + "|0", SkillVerify,
		Buffer(50, PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY)}));
	CHECK(!scene.Ready(MAGIC_BLOODY_TUNNEL));
	// Not again until the delay.
	calls.clear();
	pPortal->UseInventory();
	CHECK(Same({}));
	scene.info.GetMutable(MAGIC_BLOODY_TUNNEL)->SetAvailableTime();
	// Each condition alone refuses.
	for (int condition = 0; condition < 4; ++condition)
	{
		g_bHolyLand = condition == 0;
		scene.zones.Get(60)->CannotUseSpecialItem = condition == 1;
		if (condition == 2) pPortal->UnSetAffectStatus(); else pPortal->SetAffectStatus();
		pPortal->SetNumber(condition == 3 ? 0 : 1);
		pPortal->UseInventory();
	}
	CHECK(Same({}));
	// Without the skill: nothing.
	scene.available.clear();
	pPortal->SetNumber(1);
	g_bHolyLand = false;
	scene.zones.Get(60)->CannotUseSpecialItem = false;
	pPortal->SetAffectStatus();
	pPortal->UseInventory();
	CHECK(Same({}));
}

TEST(ItemUse, AVampireItemTransforms)
{
	Scene scene;
	scene.Effect(MAGIC_TRANSFORM_TO_WOLF);
	scene.Effect(MAGIC_TRANSFORM_TO_BAT);
	MVampireETC* pItem = scene.Put<MVampireETC>(50, 3, 2, 0, 2);
	// Type 0 the wolf, 1 the bat, with the skill.
	pItem->UseInventory();
	CHECK(Same({}));
	scene.Learn(MAGIC_TRANSFORM_TO_WOLF);
	scene.Learn(MAGIC_TRANSFORM_TO_BAT);
	pItem->UseInventory();
	CHECK(Same(Calls{"skill|50|3|2|" + N(MAGIC_TRANSFORM_TO_WOLF) + "|0", SkillVerify,
		Buffer(50, PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY), "effect|" + N(MAGIC_TRANSFORM_TO_WOLF) + "|30"}));
	CHECK(!scene.Ready(MAGIC_TRANSFORM_TO_WOLF));
	calls.clear();
	pItem->SetItemType(1);
	pItem->UseInventory();
	CHECK(!calls.empty() && calls[0] == "skill|50|3|2|" + N(MAGIC_TRANSFORM_TO_BAT) + "|0");
	// Type 2 is used as an item; past it nothing.
	calls.clear();
	pItem->SetItemType(2);
	pItem->UseInventory();
	CHECK(Same(Calls{"use|50|3|2|0", Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY)}));
	calls.clear();
	pItem->SetItemType(3);
	pItem->UseInventory();
	CHECK(Same({}));
	// Not in a zone that bars special items, while waiting or with an item
	// in the check buffer.
	scene.info.GetMutable(MAGIC_TRANSFORM_TO_BAT)->SetAvailableTime();
	pItem->SetItemType(1);
	scene.zones.Get(60)->CannotUseSpecialItem = true;
	pItem->UseInventory();
	pItem->SetItemType(2);
	pItem->UseInventory();
	scene.zones.Get(60)->CannotUseSpecialItem = false;
	pItem->SetItemType(1);
	waiting = PlayerState::WAIT_VERIFY_ITEM_USE;
	pItem->UseInventory();
	waiting = PlayerState::WAIT_VERIFY_NULL;
	checkBuffer = pItem;
	pItem->UseInventory();
	CHECK(Same({}));
}

TEST(ItemUse, AVampireItemOfTheSkullClassMakesAWerewolf)
{
	// No item class makes an MVampireETC a skull; a derived one does.
	struct SkullLike : MVampireETC
	{
		ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_SKULL; }
	};
	Scene scene;
	scene.Rows(ITEM_CLASS_SKULL, 10);
	scene.Learn(SKILL_TRANSFORM_TO_WERWOLF);
	SkullLike* pItem = scene.Put<SkullLike>(50, 3, 2, 5, 1);
	pItem->UseInventory();
	CHECK(!calls.empty() && calls[0] == "skill|50|3|2|" + N(SKILL_TRANSFORM_TO_WERWOLF) + "|0");
}

TEST(ItemUse, ASkullTransformsOnlyAsTheWolfsHead)
{
	Scene scene;
	scene.Learn(SKILL_TRANSFORM_TO_WERWOLF);
	scene.Learn(MAGIC_TRANSFORM_TO_WOLF);
	scene.Learn(MAGIC_TRANSFORM_TO_BAT);
	scene.Effect(SKILL_TRANSFORM_TO_WERWOLF);
	MSkull* pSkull = scene.Put<MSkull>(50, 3, 2, 39, 1);
	pSkull->UseInventory();
	CHECK(Same(Calls{"skill|50|3|2|" + N(SKILL_TRANSFORM_TO_WERWOLF) + "|0", SkillVerify,
		Buffer(50, PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY), "effect|" + N(SKILL_TRANSFORM_TO_WERWOLF) + "|30"}));
	CHECK(!scene.Ready(SKILL_TRANSFORM_TO_WERWOLF));
	// Skulls 0 and 1 are the wolf and the bat, 2 is used, others nothing.
	const std::pair<TYPE_ITEMTYPE, std::string> expected[] = {
		{0, "skill|50|3|2|" + N(MAGIC_TRANSFORM_TO_WOLF) + "|0"},
		{1, "skill|50|3|2|" + N(MAGIC_TRANSFORM_TO_BAT) + "|0"},
		{2, "use|50|3|2|0"},
	};
	for (const auto& row : expected)
	{
		calls.clear();
		pSkull->SetItemType(row.first);
		pSkull->UseInventory();
		CHECK(!calls.empty() && calls[0] == row.second);
	}
	calls.clear();
	pSkull->SetItemType(38);
	pSkull->UseInventory();
	CHECK(Same({}));
	// Not where special items are barred, nor while waiting or holding a
	// checked item.
	scene.info.GetMutable(SKILL_TRANSFORM_TO_WERWOLF)->SetAvailableTime();
	pSkull->SetItemType(39);
	scene.zones.Get(60)->CannotUseSpecialItem = true;
	pSkull->UseInventory();
	pSkull->SetItemType(2);
	pSkull->UseInventory();
	scene.zones.Get(60)->CannotUseSpecialItem = false;
	pSkull->SetItemType(39);
	waiting = PlayerState::WAIT_VERIFY_ITEM_USE;
	pSkull->UseInventory();
	waiting = PlayerState::WAIT_VERIFY_NULL;
	checkBuffer = pSkull;
	pSkull->UseInventory();
	checkBuffer = NULL;
	CHECK(Same({}));
	// Without the skill, or before its time: nothing.
	scene.info.GetMutable(SKILL_TRANSFORM_TO_WERWOLF)->SetNextAvailableTime();
	pSkull->UseInventory();
	scene.available.clear();
	scene.info.GetMutable(SKILL_TRANSFORM_TO_WERWOLF)->SetAvailableTime();
	pSkull->UseInventory();
	CHECK(Same({}));
}

TEST(ItemUse, ASummonGemNeedsAnUnsafeStillPlayerWithoutAFlag)
{
	Scene scene;
	MOustersSummonGem* pGem = scene.Put<MOustersSummonGem>(50, 3, 2, 0, 2);
	pGem->SetAffectStatus();
	pGem->UseInventory();
	CHECK(Same(Calls{"use|50|3|2|0", Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY)}));
	// The sylph is summoned four seconds from now.
	CHECK(!scene.Ready(SKILL_SUMMON_SYLPH));
	now += MonotonicClock::Millis(3999);
	CHECK(!scene.Ready(SKILL_SUMMON_SYLPH));
	now += MonotonicClock::Millis(1);
	CHECK(scene.Ready(SKILL_SUMMON_SYLPH));
	auto refused = [&](auto set, auto reset) {
		calls.clear();
		set();
		pGem->UseInventory();
		CHECK(Same({}));
		reset();
		pGem->UseInventory();
		CHECK_EQ(2u, calls.size());
	};
	refused([&] { pGem->SetNumber(0); }, [&] { pGem->SetNumber(1); });
	refused([&] { pGem->UnSetAffectStatus(); }, [&] { pGem->SetAffectStatus(); });
	refused([] { g_bZoneSafe = true; }, [] { g_bZoneSafe = false; });
	refused([] { safeSector = 1; }, [] { safeSector = 0; });
	refused([] { safeSector = 2; }, [] { safeSector = 0; });
	refused([] { standing = false; }, [] { standing = true; });
	refused([] { effects.insert(EFFECTSTATUS_HAS_FLAG); }, [] { effects.clear(); });
}

//----------------------------------------------------------------------
// Plain uses
//----------------------------------------------------------------------
TEST(ItemUse, PlainItemsAreUsedFromTheInventory)
{
	Scene scene;
	const auto used = [](TYPE_OBJECTID id) { return Calls{"use|" + N(id) + "|3|2|0", Buffer(id, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY)}; };
	scene.Put<MTrapItem>(50, 3, 2)->UseInventory();
	CHECK(Same(used(50)));
	scene.inventory.RemoveItem(50);
	// A gift box from type 6.
	calls.clear();
	MEventGiftBoxItem* pBox = scene.Put<MEventGiftBoxItem>(51, 3, 2, 5);
	pBox->UseInventory();
	CHECK(Same({}));
	pBox->SetItemType(6);
	pBox->UseInventory();
	CHECK(Same(used(51)));
	scene.inventory.RemoveItem(51);
	delete pBox;
	// The resurrection scroll is type 1.
	calls.clear();
	MItemETC* pScroll = scene.Put<MItemETC>(52, 3, 2, 0);
	pScroll->UseInventory();
	pScroll->SetItemType(2);
	pScroll->UseInventory();
	CHECK(Same({}));
	pScroll->SetItemType(1);
	pScroll->UseInventory();
	CHECK(Same(used(52)));
	scene.inventory.RemoveItem(52);
	delete pScroll;
	// A text message item, with a player.
	calls.clear();
	MSms_item* pSms = scene.Put<MSms_item>(53, 3, 2);
	noPlayer = true;
	pSms->UseInventory();
	CHECK(Same({}));
	noPlayer = false;
	pSms->UseInventory();
	CHECK(Same(used(53)));
}

TEST(ItemUse, EffectItemsOpenTheBoardOrAreUsed)
{
	Scene scene;
	MEffectItem* pItem = scene.Put<MEffectItem>(50, 3, 2, 9);
	pItem->UseInventory();
	CHECK(Same(Calls{"use|50|3|2|0", Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY)}));
	for (TYPE_ITEMTYPE type : {10, 12})
	{
		calls.clear();
		pItem->SetItemType(type);
		pItem->UseInventory();
		CHECK(Same(Calls{"board|50"}));
	}
	calls.clear();
	pItem->SetItemType(13);
	pItem->UseInventory();
	CHECK_EQ(2u, calls.size());
}

TEST(ItemUse, AKeyRidesOnlyWithoutAFlagAround)
{
	Scene scene;
	MKey* pKey = scene.Put<MKey>(50, 3, 2);
	pKey->UseInventory();
	CHECK(Same(Calls{"look for|670", "use|50|3|2|0", Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY)}));
	calls.clear();
	flagAround = true;
	pKey->UseInventory();
	CHECK(Same(Calls{"look for|670"}));
	CHECK((Lines(scene.systemLog) == std::vector<std::string>{S(UI_STRING_MESSAGE_CANNOT_ACTION_MOTORCYCLE_FLAG)}));
}

TEST(ItemUse, AQuickSlotKeyLocksTheBeltUnlessATurretIsUp)
{
	Scene scene;
	MKey key;
	key.SetID(50);
	key.SetItemSlot(4);
	key.UseQuickItem();
	const std::string quick = Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
	CHECK(Same(Calls{"quick potion|50|4", "lock gear", quick, quick, "lock gear"}));
	calls.clear();
	effects.insert(EFFECTSTATUS_INSTALL_TURRET);
	key.UseQuickItem();
	CHECK(Same({}));
}

TEST(ItemUse, QuickSlotPotionsLockTheBelt)
{
	Scene scene;
	MPotion potion;
	potion.SetID(50);
	potion.SetItemSlot(3);
	potion.UseQuickItem();
	CHECK(Same(Calls{"quick potion|50|3", Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT), "lock gear"}));
}

TEST(ItemUse, AnOustersArmsBandSlotCountsFromTheFirstBand)
{
	Scene scene;
	scene.Rows(ITEM_CLASS_OUSTERS_ARMSBAND, 1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_OUSTERS_ARMSBAND, 0).Value3 = 3;
	scene.Rows(ITEM_CLASS_PUPA, 1);
	scene.Rows(ITEM_CLASS_COMPOS_MEI, 1);
	MOustersArmsBand band;
	band.SetItemType(0);
	MOustersPupa* pPupa = new MOustersPupa;
	pPupa->SetID(50);
	CHECK(band.AddItem(pPupa, 1));
	MOustersComposMei mei;
	mei.SetID(51);
	mei.SetItemSlot(1);
	const std::string quick50 = Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
	const std::string quick51 = Buffer(51, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
	// In the first band: its slot. Elsewhere, or without a first band:
	// the slot plus 3.
	g_pArmsBand1 = &band;
	pPupa->UseQuickItem();
	mei.UseQuickItem();
	g_pArmsBand1 = NULL;
	pPupa->UseQuickItem();
	mei.UseQuickItem();
	CHECK(Same(Calls{"quick potion|50|1", quick50, "lock gear", "quick potion|51|4", quick51, "lock gear",
		"quick potion|50|4", quick50, "lock gear", "quick potion|51|4", quick51, "lock gear"}));
	band.MSlotItemManager::Release();
}

TEST(ItemUse, EventItemsAreCandyOrUsedOutsideSafeZones)
{
	Scene scene;
	MEventEtcItem* pItem = scene.Put<MEventEtcItem>(50, 3, 2, 13);
	const std::string used = Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
	pItem->UseInventory();
	CHECK(Same(Calls{"use|50|3|2|0", used}));
	// Candy and rice cakes (14 to 17) are eaten, even in a safe zone.
	for (TYPE_ITEMTYPE type : {14, 15, 16, 17})
	{
		calls.clear();
		pItem->SetItemType(type);
		safeSector = 2;
		pItem->UseInventory();
		CHECK(Same(Calls{"potion|50|3|2", used}));
	}
	// Anything else is refused where it is safe for everyone.
	calls.clear();
	pItem->SetItemType(18);
	pItem->UseInventory();
	CHECK(Same(Calls{"dialog|" + S(STRING_MESSAGE_NOT_USE_SAFETY_ZONE)}));
	calls.clear();
	safeSector = 1;
	pItem->UseInventory();
	CHECK(Same(Calls{"use|50|3|2|0", used}));
}

TEST(ItemUse, QuickSlotCandyFollowsTheArmsBandForOusters)
{
	Scene scene;
	MEventEtcItem candy;
	candy.SetID(50);
	candy.SetItemType(14);
	candy.SetItemSlot(2);
	const std::string quick = Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT);
	candy.UseQuickItem();
	CHECK(Same(Calls{"quick potion|50|2", quick, "lock gear"}));
	calls.clear();
	ousters = true;
	candy.UseQuickItem();
	CHECK(Same(Calls{"quick potion|50|5", quick, "lock gear"}));
	scene.Rows(ITEM_CLASS_OUSTERS_ARMSBAND, 1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_OUSTERS_ARMSBAND, 0).Value3 = 3;
	MOustersArmsBand band;
	band.SetItemType(0);
	g_pArmsBand1 = &band;
	calls.clear();
	candy.UseQuickItem();
	CHECK(Same(Calls{"quick potion|50|5", quick, "lock gear"}));
	MEventEtcItem* pHeld = new MEventEtcItem;
	pHeld->SetID(51);
	pHeld->SetItemType(15);
	if (band.AddItem(pHeld, 2))
	{
		calls.clear();
		pHeld->UseQuickItem();
		CHECK(Same(Calls{"quick potion|51|2", Buffer(51, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT), "lock gear"}));
		band.MSlotItemManager::Release();
	}
	else
	{
		delete pHeld;
	}
	// Anything but candy and rice cakes: nothing.
	calls.clear();
	candy.SetItemType(13);
	candy.UseQuickItem();
	candy.SetItemType(18);
	candy.UseQuickItem();
	CHECK(Same({}));
}

TEST(ItemUse, OustersCannotChangeTheirGender)
{
	Scene scene;
	MDyePotionItem* pPotion = scene.Put<MDyePotionItem>(50, 3, 2, 48);
	pPotion->UseInventory();
	CHECK(Same(Calls{"use|50|3|2|0", Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY)}));
	calls.clear();
	ousters = true;
	pPotion->UseInventory();
	CHECK(Same(Calls{"popup|" + N(STRING_MESSAGE_CANNOT_USE_OUSTERS)}));
	calls.clear();
	pPotion->SetItemType(47);
	pPotion->UseInventory();
	CHECK_EQ(2u, calls.size());
}

TEST(ItemUse, ACodeSheetOpensTheQuestInventoryWhenFull)
{
	Scene scene;
	MCodeSheetItem* pSheet = scene.Put<MCodeSheetItem>(50, 3, 2);
	for (int i = 0; i < 29; ++i) pSheet->AddItemOption(1);
	pSheet->UseInventory();
	CHECK(Same({}));
	pSheet->AddItemOption(1);
	pSheet->UseInventory();
	CHECK(Same(Calls{"quest inventory|50"}));
	calls.clear();
	pSheet->AddItemOption(1);
	pSheet->UseInventory();
	pSheet->SetItemType(1);
	pSheet->ClearItemOption();
	for (int i = 0; i < 30; ++i) pSheet->AddItemOption(1);
	pSheet->UseInventory();
	CHECK(Same({}));
}

TEST(ItemUse, CoupleRingsAreUsedFromTheGear)
{
	Scene scene;
	MCoupleRing ring;
	ring.SetID(50);
	ring.SetItemSlot(MSlayerGear::GEAR_SLAYER_RING2);
	ring.UseGear();
	MVampireCoupleRing vampireRing;
	vampireRing.SetID(51);
	vampireRing.SetItemSlot(3);
	vampireRing.UseGear();
	CHECK(Same(Calls{"gear|50|" + N(MSlayerGear::GEAR_SLAYER_RING2), Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_GEAR),
		"gear|51|3", Buffer(51, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_GEAR)}));
}

TEST(ItemUse, PotionsAndPetsAreUsedThroughTheHostsBodies)
{
	Scene scene;
	MItem* pPotion = scene.Put<MPotion>(50, 3, 2);
	UsePotionFromInventory(pPotion);
	CHECK(Same(Calls{"potion|50|3|2", Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY)}));
	// A pet with life left is called.
	calls.clear();
	scene.Rows(ITEM_CLASS_PET_ITEM, 1);
	MPetItem* pPet = scene.Put<MPetItem>(51, 4, 2);
	pPet->SetCurrentDurability(0);
	UsePetFromInventory(pPet);
	CHECK(Same({}));
	pPet->SetCurrentDurability(1);
	UsePetFromInventory(pPet);
	CHECK(Same(Calls{"use|51|4|2|0", Buffer(51, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY)}));
}

TEST(ItemUse, PetFoodFeedsAPetOutOrSplits)
{
	Scene scene;
	MPetFood* pFood = scene.Put<MPetFood>(50, 3, 2, 0, 1);
	pFood->UseInventory();
	CHECK(Same({}));
	pFood->SetNumber(2);
	pFood->UseInventory();
	CHECK(Same(Calls{"lift|0|3|2|0", Buffer(50, PlayerState::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY)}));
	calls.clear();
	petID = 900;
	pFood->UseInventory();
	CHECK(Same(Calls{"use|50|3|2|0", Buffer(50, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY)}));
	// Without a player: no pet, whatever the creature host answers.
	calls.clear();
	noPlayer = true;
	pFood->UseInventory();
	CHECK(Same(Calls{"lift|0|3|2|0", Buffer(50, PlayerState::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY)}));
}

//----------------------------------------------------------------------
// The event tree and the bulletin board
//----------------------------------------------------------------------
TEST(ItemUse, AnEventTreeOpensItsWindow)
{
	Scene scene;
	MEventTreeItem* pTree = scene.Put<MEventTreeItem>(50, 3, 2, 12);
	pTree->UseInventory();
	CHECK(Same(Calls{"xmas card|50"}));
	for (TYPE_ITEMTYPE type : {26, 28})
	{
		calls.clear();
		pTree->SetItemType(type);
		pTree->UseInventory();
		CHECK(Same(Calls{"board|50"}));
	}
	calls.clear();
	for (TYPE_ITEMTYPE type : {11, 13, 25, 29})
	{
		pTree->SetItemType(type);
		pTree->UseInventory();
	}
	CHECK(Same({}));
}

TEST(ItemUse, AnEventTreeIsRefusedWhereItIsSafe)
{
	Scene scene;
	MEventTreeItem* pTree = scene.Put<MEventTreeItem>(50, 3, 2, 12);
	const Calls refused = {"dialog|" + S(STRING_MESSAGE_NOT_USE_SAFETY_POSITION)};
	for (int safe : {1, 2})
	{
		calls.clear();
		safeSector = safe;
		pTree->UseInventory();
		CHECK(Same(refused));
	}
	safeSector = 0;
	// The master lairs count as safe.
	for (int lair : {1104, 1106, 1114, 1115})
	{
		calls.clear();
		zoneID = lair;
		pTree->UseInventory();
		CHECK(Same(refused));
	}
	calls.clear();
	zoneID = 1105;
	pTree->UseInventory();
	CHECK(Same(Calls{"xmas card|50"}));
	// Without a zone a lair cannot be told.
	calls.clear();
	zoneID = 1104;
	zoneExists = false;
	pTree->UseInventory();
	CHECK(Same(Calls{"xmas card|50"}));
	zoneExists = true;
	// In a castle, or for an operator, it is not refused.
	safeSector = 2;
	for (int castle : {1201, 1206})
	{
		calls.clear();
		zoneID = castle;
		pTree->UseInventory();
		CHECK(Same(Calls{"xmas card|50"}));
	}
	calls.clear();
	zoneID = 1207;
	pTree->UseInventory();
	zoneID = 1200;
	pTree->UseInventory();
	CHECK(Same(Calls{refused[0], refused[0]}));
	for (int operatorType : {CREATURETYPE_SLAYER_OPERATOR, CREATURETYPE_VAMPIRE_OPERATOR, CREATURETYPE_OUSTERS_OPERATOR})
	{
		calls.clear();
		creatureType = operatorType;
		pTree->UseInventory();
		CHECK(Same(Calls{"xmas card|50"}));
	}
}

TEST(ItemUse, AnEventTreeIsRefusedOnAMount)
{
	Scene scene;
	MEventTreeItem* pTree = scene.Put<MEventTreeItem>(50, 3, 2, 27);
	device = CreatureState::MOVE_DEVICE_RIDE;
	pTree->UseInventory();
	CHECK(Same(Calls{"dialog|" + S(STRING_MESSAGE_CANNOT_USE_RIDE_MOTORCYCLE)}));
	calls.clear();
	race = RACE_OUSTERS;
	ousters = true;
	pTree->UseInventory();
	CHECK(Same(Calls{"dialog|" + S(STRING_MESSAGE_CANNOT_USE_SUMMON_SYLPH)}));
	// A vampire is refused without a word.
	calls.clear();
	race = RACE_VAMPIRE;
	ousters = false;
	pTree->UseInventory();
	CHECK(Same({}));
	// A summoned sylph is no mount here.
	device = CreatureState::MOVE_DEVICE_SUMMON_SYLPH;
	pTree->UseInventory();
	CHECK(Same(Calls{"board|50"}));
}

//----------------------------------------------------------------------
// The new host entries
//----------------------------------------------------------------------
TEST(ItemUse, WithoutHostsTheNewEntriesAnswerNothing)
{
	const ZoneState::Host* oldZone = ZoneState::SetHost(nullptr);
	const CreatureState::Host* oldCreatures = CreatureState::SetHost(nullptr);
	const VsUiPresentation::Host* oldUi = VsUiPresentation::SetHost(nullptr);
	calls.clear();
	CHECK(!ZoneState::HasHelicopter(77));
	CHECK(!ZoneState::IsExistCorpseFromPlayer(PlayerCreature(), 670));
	CHECK_EQ(CreatureState::MOVE_DEVICE_WALK, CreatureState::GetMoveDevice(PlayerCreature()));
	CHECK(!CreatureState::IsStop(PlayerCreature()));
	MKey key;
	VsUiPresentation::LockGear();
	VsUiPresentation::RunMixingForge(1, 2);
	VsUiPresentation::RunBulletinBoardWindow(&key);
	VsUiPresentation::RunXmasCardWindow(&key);
	VsUiPresentation::RunQuestInventory(&key);
	CHECK(Same({}));
	// With them, each is asked.
	ZoneState::SetHost(&zoneHost);
	CreatureState::SetHost(&creatureHost);
	VsUiPresentation::SetHost(&uiHost);
	noPlayer = false;
	playerID = 77;
	helicopter = true;
	flagAround = true;
	device = CreatureState::MOVE_DEVICE_RIDE;
	standing = true;
	key.SetID(5);
	CHECK(ZoneState::HasHelicopter(77));
	CHECK(ZoneState::IsExistCorpseFromPlayer(PlayerCreature(), 670));
	CHECK_EQ(CreatureState::MOVE_DEVICE_RIDE, CreatureState::GetMoveDevice(PlayerCreature()));
	CHECK(CreatureState::IsStop(PlayerCreature()));
	VsUiPresentation::LockGear();
	VsUiPresentation::RunMixingForge(1, 2);
	VsUiPresentation::RunBulletinBoardWindow(&key);
	VsUiPresentation::RunXmasCardWindow(&key);
	VsUiPresentation::RunQuestInventory(&key);
	CHECK(Same({"look for|670", "lock gear", "forge|1|2", "board|5", "xmas card|5", "quest inventory|5"}));
	helicopter = flagAround = false;
	device = CreatureState::MOVE_DEVICE_WALK;
	VsUiPresentation::SetHost(oldUi);
	CreatureState::SetHost(oldCreatures);
	ZoneState::SetHost(oldZone);
}
