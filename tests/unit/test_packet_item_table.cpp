#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "PacketFunction.h"
#include "MCreatureWear.h"
#include "MFakeCreature.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MActionInfoTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "EffectSpriteTypeDef.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "MEventManager.h"
#include "MEventQueue.h"
#include "FrameClock.h"
#include "MEffect.h"
#include "MAttachEffect.h"
#include "MZone.h"
#include "MImageObject.h"
#include "ZoneFlags.h"
#include "Properties.h"
#include "GameInitInfoHost.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"
#include "AddonDef.h"
#include "SkillDef.h"
#include "VS_UI_title.h"
#include "PCSlayerInfo2.h"
#include "PCSlayerInfo3.h"
#include "PCOustersInfo2.h"
#include "PCOustersInfo3.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

// PacketFunction.cpp's, declared in no header.
extern MItem*	g_pPacketItemShoulder[SHOULDER_MAX];
int				UI_GetFaceStyle(bool bMale, int faceStyle);

// The packet handlers' helpers that dress a creature from a packet (the
// executable's until they moved into gameclient): the prototype items a
// packet's outlook indexes, the addons and colours they put on a
// creature, the face a hair style shows, the fake copies of a creature
// and the gore level.
namespace {

std::string N(long long value) { return std::to_string(value); }

//----------------------------------------------------------------------
// The nine prototype tables, cleared for a test and given back after.
//----------------------------------------------------------------------
template <std::size_t Size>
struct SavedTable
{
	MItem* (&table)[Size];
	MItem* saved[Size];
	explicit SavedTable(MItem* (&live)[Size]) : table(live)
	{
		std::copy(live, live + Size, saved);
		std::fill(live, live + Size, nullptr);
	}
	~SavedTable() { std::copy(saved, saved + Size, table); }
};

struct PacketTables
{
	SavedTable<HELMET_MAX> helm{g_pPacketItemHelm};
	SavedTable<JACKET_MAX> jacket{g_pPacketItemJacket};
	SavedTable<PANTS_MAX> pants{g_pPacketItemPants};
	SavedTable<WEAPON_MAX> weapon{g_pPacketItemWeapon};
	SavedTable<SHIELD_MAX> shield{g_pPacketItemShield};
	SavedTable<MOTORCYCLE_MAX> motorcycle{g_pPacketItemMotorcycle};
	SavedTable<SHOULDER_MAX> shoulder{g_pPacketItemShoulder};
	SavedTable<OUSTERS_COAT_MAX> oustersCoat{g_pPacketItemOustersCoat};
	SavedTable<OUSTERS_ARM_MAX> oustersArm{g_pPacketItemOustersArm};
	bool filled = false;

	// The start-up's tables, released when the test ends.
	void Fill() { InitPacketItemTable(); filled = true; }
	~PacketTables()
	{
		if (filled) ReleasePacketItemTable();
	}
};

// A table's entries as "class:type", "-" for none.
template <std::size_t Size>
std::vector<std::string> Entries(MItem* (&table)[Size])
{
	std::vector<std::string> entries;
	for (MItem* pItem : table)
		entries.push_back(pItem == nullptr ? "-" : N(pItem->GetItemClass()) + ":" + N(pItem->GetItemType()));
	return entries;
}

std::string E(int itemClass, int itemType) { return N(itemClass) + ":" + N(itemType); }

bool Same(const std::vector<std::string>& actual, const std::vector<std::string>& expected)
{
	if (actual == expected) return true;
	std::printf("      got:   ");
	for (const std::string& entry : actual) std::printf("[%s] ", entry.c_str());
	std::printf("\n      wanted:");
	for (const std::string& entry : expected) std::printf("[%s] ", entry.c_str());
	std::printf("\n");
	return false;
}

// An item that names itself in the list when it is deleted.
std::vector<std::string> freed;
struct Witness : MItem
{
	std::string name;
	explicit Witness(std::string itemName) : name(std::move(itemName)) {}
	~Witness() override { freed.push_back(name); }
};

template <std::size_t Size>
void FillWithWitnesses(MItem* (&table)[Size], const char* name)
{
	for (std::size_t i = 0; i < Size; ++i)
		table[i] = new Witness(std::string(name) + N(static_cast<long long>(i)));
}

//----------------------------------------------------------------------
// The creature scene: the tables and hosts a creature reads when it is
// made, typed, placed and freed.
//----------------------------------------------------------------------
const ZoneState::Host zoneHost = []() {
	ZoneState::Host host{};
	host.Exists = []() { return true; };
	host.GetFakeCreature = [](TYPE_OBJECTID) -> MCreature* { return nullptr; };
	return host;
}();
const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.Creature = []() -> MCreature* { return nullptr; };
	return host;
}();
const WorldEvents::Host worldHost = []() {
	WorldEvents::Host host{};
	host.ViewExists = []() { return true; };
	return host;
}();
const CreatureState::Host creatureHost = []() {
	CreatureState::Host host{};
	host.GetAddonItemClass = [](MCreature*, int, ITEM_CLASS* pItemClass) { *pItemClass = ITEM_CLASS_NULL; return false; };
	return host;
}();
const MEffectHost effectHost = []() {
	MEffectHost host{};
	host.CurrentFrame = []() -> DWORD { return g_CurrentFrame; };
	return host;
}();
const MAttachEffectHost attachHost = []() {
	MAttachEffectHost host{};
	host.LiveCreature = [](TYPE_OBJECTID, MAttachCreaturePosition&) { return true; };
	host.CreaturePosition = [](const MCreature*, MAttachCreaturePosition&) { return true; };
	return host;
}();

struct Scene
{
	GameModelWorld world;
	CREATURE_TABLE creatureTypes;
	CREATURESPRITE_TABLE sprites;
	MActionInfoTable actions;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	RankBonusTable rankBonuses;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	MEventQueue events;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	MEventQueue* oldEvents = g_pEventManager;
	MZone* oldZone = g_pZone;
	const ZoneState::Host* oldZoneHost = ZoneState::SetHost(&zoneHost);
	const PlayerState::Host* oldPlayerHost = PlayerState::SetHost(&playerHost);
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&worldHost);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creatureHost);
	const MEffectHost* oldEffectHost = MEffect::SetHost(&effectHost);
	const MAttachEffectHost* oldAttachHost = MAttachEffect::SetHost(&attachHost);

	Scene()
	{
		creatureTypes.Init(800);
		g_pCreatureTable = &creatureTypes;
		sprites.Init(4);
		g_pCreatureSpriteTable = &sprites;
		actions.Init(MAX_ACTIONINFO);
		// The game loads every row; a row here stands still unless a test
		// sets it (the constructor leaves what Set fills).
		for (int info = 0; info < MAX_ACTIONINFO; ++info)
			actions.GetMutable(info)->Set("", ACTION_STAND, EFFECTSPRITETYPE_NULL, 0, 0);
		actions.SetMinResultActionInfo(MIN_RESULT_ACTIONINFO);
		g_pActionInfoTable = &actions;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		g_pRankBonusTable = &rankBonuses;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		g_pEventManager = &events;
	}
	~Scene()
	{
		MAttachEffect::SetHost(oldAttachHost);
		MEffect::SetHost(oldEffectHost);
		CreatureState::SetHost(oldCreature);
		WorldEvents::SetHost(oldWorld);
		PlayerState::SetHost(oldPlayerHost);
		ZoneState::SetHost(oldZoneHost);
		g_pZone = oldZone;
		g_pEventManager = oldEvents;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pRankBonusTable = oldRankBonuses;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pActionInfoTable = oldActions;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatureTypes;
	}
	// A creature type of the tribe and gender, with a count for each action.
	void Tribe(int type, CREATURETRIBE tribe, bool male = true)
	{
		testfw::MutableRow(creatureTypes, type).SetCreatureTribe(tribe, ACTION_MAX_SLAYER);
		testfw::MutableRow(creatureTypes, type).bMale = male;
		for (int action = 0; action < ACTION_MAX_SLAYER; ++action)
			testfw::MutableRow(creatureTypes, type).SetActionCount(action, 10 + action);
	}
};

// A wearer that records each item it is handed and wears none, so its
// addons show only what the colour and hair setters put there.
struct Wearer : MCreatureWear
{
	std::vector<const MItem*> handed;
	bool SetAddonItem(MItem* pItem) override
	{
		handed.push_back(pItem);
		return true;
	}
};

std::string Addon(const MCreatureWear& creature, int slot)
{
	const MCreatureWear::ADDON_INFO& addon = creature.GetAddonInfo(slot);
	return N(addon.bAddon) + "|" + N(addon.FrameID) + "|" + N(addon.ItemClass) + "|" + N(addon.ItemType)
		+ "|" + N(addon.ColorSet1) + "|" + N(addon.ColorSet2);
}

// An addon no item was put on: worn or not, and its two colours.
std::string Colours(int worn, int colorSet1, int colorSet2)
{
	return N(worn) + "|" + N(FRAMEID_NULL) + "|" + N(ITEM_CLASS_NULL) + "|" + N(ITEMTYPE_NULL) + "|" + N(colorSet1) + "|" + N(colorSet2);
}

std::string Hair(int frameID, int colorSet)
{
	return "1|" + N(frameID) + "|" + N(ITEM_CLASS_NULL) + "|" + N(ITEMTYPE_NULL) + "|" + N(colorSet) + "|0";
}

// The zone view's slayer: every outlook field and colour its own value,
// each sub colour 100 past its main one so a read of the wrong one shows.
PCSlayerInfo3 Slayer3(Sex sex, HairStyle hair, HelmetType helmet, JacketType jacket, PantsType pants,
	WeaponType weapon, ShieldType shield, MotorcycleType motorcycle, BYTE shoulder)
{
	PCSlayerInfo3 info;
	info.setSex(sex);
	info.setHairStyle(hair);
	info.setHelmetType(helmet);
	info.setJacketType(jacket);
	info.setPantsType(pants);
	info.setWeaponType(weapon);
	info.setShieldType(shield);
	info.setMotorcycleType(motorcycle);
	info.setShoulderType(shoulder);
	info.setHairColor(11);
	info.setSkinColor(12);
	info.setHelmetColor(13, MAIN_COLOR);
	info.setHelmetColor(113, SUB_COLOR);
	info.setJacketColor(14, MAIN_COLOR);
	info.setJacketColor(114, SUB_COLOR);
	info.setPantsColor(15, MAIN_COLOR);
	info.setPantsColor(115, SUB_COLOR);
	info.setWeaponColor(16, MAIN_COLOR);
	info.setWeaponColor(116, SUB_COLOR);
	info.setShieldColor(17, MAIN_COLOR);
	info.setShieldColor(117, SUB_COLOR);
	info.setMotorcycleColor(18, MAIN_COLOR);
	info.setMotorcycleColor(118, SUB_COLOR);
	info.setShoulderColor(19);
	info.setAttackSpeed(70);
	info.setAdvancementLevel(0);
	return info;
}

PCOustersInfo3 Ousters3(OustersCoatType coat, OustersArmType arm)
{
	PCOustersInfo3 info;
	info.setSex(FEMALE);
	info.setCoatType(coat);
	info.setArmType(arm);
	info.setSylphType(OUSTERS_SYLPH_OFF);
	info.setHairColor(21);
	info.setCoatColor(22);
	info.setArmColor(23);
	info.setBootsColor(24);
	info.setAttackSpeed(10);
	return info;
}

// What a creature's protected zone pointer holds.
struct ZonePeek : MCreature
{
	static MZone* Of(const MCreature& creature) { return creature.*(&ZonePeek::m_pZone); }
};

} // namespace

//----------------------------------------------------------------------
// The prototype tables
//----------------------------------------------------------------------
TEST(PacketItemTable, InitFillsEachOutlookWithItsPrototype)
{
	PacketTables tables;
	tables.Fill();
	const std::string none = "-";
	CHECK(Same(Entries(g_pPacketItemHelm), {none, E(ITEM_CLASS_HELM, 0), E(ITEM_CLASS_HELM, 3),
		E(ITEM_CLASS_HELM, 15), E(ITEM_CLASS_HELM, 16), E(ITEM_CLASS_HELM, 17)}));
	// The basic jacket and trousers are the body's own: no item.
	CHECK(Same(Entries(g_pPacketItemJacket), {none, E(ITEM_CLASS_COAT, 0), E(ITEM_CLASS_COAT, 4), E(ITEM_CLASS_COAT, 8),
		E(ITEM_CLASS_COAT, 28), E(ITEM_CLASS_COAT, 30), E(ITEM_CLASS_COAT, 32)}));
	// PANTS4 has no prototype.
	CHECK(Same(Entries(g_pPacketItemPants), {none, E(ITEM_CLASS_TROUSER, 0), E(ITEM_CLASS_TROUSER, 4), E(ITEM_CLASS_TROUSER, 8), none}));
	CHECK(Same(Entries(g_pPacketItemWeapon), {none,
		E(ITEM_CLASS_SWORD, 0), E(ITEM_CLASS_SWORD, 16), E(ITEM_CLASS_BLADE, 0), E(ITEM_CLASS_BLADE, 16),
		E(ITEM_CLASS_SR, 0), E(ITEM_CLASS_SR, 14), E(ITEM_CLASS_SR, 15), E(ITEM_CLASS_SR, 16),
		E(ITEM_CLASS_AR, 0), E(ITEM_CLASS_AR, 14), E(ITEM_CLASS_AR, 15), E(ITEM_CLASS_AR, 16),
		E(ITEM_CLASS_SG, 0), E(ITEM_CLASS_SMG, 0), E(ITEM_CLASS_CROSS, 0), E(ITEM_CLASS_CROSS, 14),
		E(ITEM_CLASS_MACE, 0), E(ITEM_CLASS_MACE, 14)}));
	CHECK(Same(Entries(g_pPacketItemShield), {none, E(ITEM_CLASS_SHIELD, 0), E(ITEM_CLASS_SHIELD, 5),
		E(ITEM_CLASS_SHIELD, 15), E(ITEM_CLASS_SHIELD, 17)}));
	CHECK(Same(Entries(g_pPacketItemMotorcycle), {none, E(ITEM_CLASS_MOTORCYCLE, 0), E(ITEM_CLASS_MOTORCYCLE, 6),
		E(ITEM_CLASS_MOTORCYCLE, 7)}));
	CHECK(Same(Entries(g_pPacketItemShoulder), {none, E(ITEM_CLASS_SHOULDER_ARMOR, 0)}));
	// An ouster's basic robe is an item; COAT1 is the same robe.
	CHECK(Same(Entries(g_pPacketItemOustersCoat), {E(ITEM_CLASS_OUSTERS_COAT, 0), E(ITEM_CLASS_OUSTERS_COAT, 0),
		E(ITEM_CLASS_OUSTERS_COAT, 3), E(ITEM_CLASS_OUSTERS_COAT, 6), E(ITEM_CLASS_OUSTERS_COAT, 6)}));
	CHECK(Same(Entries(g_pPacketItemOustersArm), {E(ITEM_CLASS_OUSTERS_WRISTLET, 0), E(ITEM_CLASS_OUSTERS_CHAKRAM, 0)}));
	// Each entry its own item: the release deletes every one.
	CHECK(g_pPacketItemOustersCoat[OUSTERS_COAT_BASIC] != g_pPacketItemOustersCoat[OUSTERS_COAT1]);
	CHECK(g_pPacketItemOustersCoat[OUSTERS_COAT3] != g_pPacketItemOustersCoat[OUSTERS_COAT4]);
}

TEST(PacketItemTable, ReleaseDeletesEachEntryButTheNoneSlots)
{
	PacketTables tables;
	freed.clear();
	FillWithWitnesses(g_pPacketItemHelm, "helm");
	FillWithWitnesses(g_pPacketItemJacket, "jacket");
	FillWithWitnesses(g_pPacketItemPants, "pants");
	FillWithWitnesses(g_pPacketItemWeapon, "weapon");
	FillWithWitnesses(g_pPacketItemShield, "shield");
	FillWithWitnesses(g_pPacketItemMotorcycle, "motorcycle");
	FillWithWitnesses(g_pPacketItemShoulder, "shoulder");
	FillWithWitnesses(g_pPacketItemOustersCoat, "coat");
	FillWithWitnesses(g_pPacketItemOustersArm, "arm");
	MItem* const keptHelm = g_pPacketItemHelm[HELMET_NONE];
	MItem* const keptJacket = g_pPacketItemJacket[JACKET_BASIC];
	MItem* const keptPants = g_pPacketItemPants[PANTS_BASIC];
	MItem* const keptWeapon = g_pPacketItemWeapon[WEAPON_NONE];
	MItem* const keptMotorcycle = g_pPacketItemMotorcycle[MOTORCYCLE_NONE];
	MItem* const keptShoulder = g_pPacketItemShoulder[SHOULDER_NONE];

	ReleasePacketItemTable();

	// In the release's order. Slot 0 of the helmet, jacket, pants, weapon,
	// motorcycle and shoulder tables is skipped (Init leaves it empty);
	// the shield's and the ousters' slot 0 are deleted.
	std::vector<std::string> expected = {"coat0", "coat1", "coat2", "coat3", "coat4", "arm0", "arm1",
		"helm1", "helm2", "helm3", "helm4", "helm5",
		"jacket1", "jacket2", "jacket3", "jacket4", "jacket5", "jacket6",
		"pants1", "pants2", "pants3", "pants4"};
	for (int i = 1; i < WEAPON_MAX; ++i) expected.push_back("weapon" + N(i));
	for (const char* name : {"shield0", "shield1", "shield2", "shield3", "shield4", "motorcycle1", "motorcycle2", "motorcycle3", "shoulder1"})
		expected.push_back(name);
	CHECK(Same(freed, expected));

	// Every entry it deleted is emptied, so a second release frees nothing
	// and a packet read after it dresses no one in freed items.
	for (MItem* pItem : g_pPacketItemOustersCoat) CHECK(pItem == nullptr);
	for (MItem* pItem : g_pPacketItemOustersArm) CHECK(pItem == nullptr);
	for (int i = 1; i < HELMET_MAX; ++i) CHECK(g_pPacketItemHelm[i] == nullptr);
	for (int i = 1; i < JACKET_MAX; ++i) CHECK(g_pPacketItemJacket[i] == nullptr);
	for (int i = 1; i < PANTS_MAX; ++i) CHECK(g_pPacketItemPants[i] == nullptr);
	for (int i = 1; i < WEAPON_MAX; ++i) CHECK(g_pPacketItemWeapon[i] == nullptr);
	for (MItem* pItem : g_pPacketItemShield) CHECK(pItem == nullptr);
	for (int i = 1; i < MOTORCYCLE_MAX; ++i) CHECK(g_pPacketItemMotorcycle[i] == nullptr);
	for (int i = 1; i < SHOULDER_MAX; ++i) CHECK(g_pPacketItemShoulder[i] == nullptr);
	freed.clear();
	ReleasePacketItemTable();
	CHECK(freed.empty());

	// The skipped slots still hold their items.
	CHECK(g_pPacketItemHelm[HELMET_NONE] == keptHelm);
	CHECK(g_pPacketItemJacket[JACKET_BASIC] == keptJacket);
	CHECK(g_pPacketItemPants[PANTS_BASIC] == keptPants);
	CHECK(g_pPacketItemWeapon[WEAPON_NONE] == keptWeapon);
	CHECK(g_pPacketItemMotorcycle[MOTORCYCLE_NONE] == keptMotorcycle);
	CHECK(g_pPacketItemShoulder[SHOULDER_NONE] == keptShoulder);
	for (MItem* pItem : {keptHelm, keptJacket, keptPants, keptWeapon, keptMotorcycle, keptShoulder}) delete pItem;
	freed.clear();
}

TEST(PacketItemTable, ReleaseAfterInitFreesEveryPrototypeOnce)
{
	// ASan stops a double free or an entry shared between two slots.
	PacketTables tables;
	InitPacketItemTable();
	ReleasePacketItemTable();
	for (MItem* pItem : g_pPacketItemOustersCoat) CHECK(pItem == nullptr);
	for (MItem* pItem : g_pPacketItemOustersArm) CHECK(pItem == nullptr);
	// Not a second release: the seven tables it leaves holding freed items
	// (above) make it a use after free at the helmets.
}

//----------------------------------------------------------------------
// A slayer NPC's clothes, from its creature type's row
//----------------------------------------------------------------------
TEST(PacketWear, ANpcWearsItsRowsClothesInItsColours)
{
	Scene scene;
	PacketTables tables;
	tables.Fill();
	const int npc = 600;
	ITEM_WEARINFO* pWear = new ITEM_WEARINFO;
	testfw::MutableRow(scene.creatureTypes, npc).pItemWearInfo = pWear;
	pWear->hair = 1;
	pWear->jacket = BYTE(JACKET2);
	pWear->pants = BYTE(PANTS3);
	pWear->helmet = BYTE(HELMET1);
	pWear->weapon = BYTE(WEAPON_BLADE);
	pWear->shield = BYTE(SHIELD2);
	pWear->motorcycle = BYTE(MOTORCYCLE1);
	pWear->skinColor = 31;
	pWear->hairColor = 32;
	pWear->jacketColor = 33;
	pWear->pantsColor = 34;
	pWear->helmetColor = 35;
	pWear->weaponColor = 36;
	pWear->shieldColor = 37;
	pWear->motorcycleColor = 38;
	for (bool male : {true, false})
	{
		testfw::MutableRow(scene.creatureTypes, npc).bMale = male;
		Wearer creature;
		SetAddonToSlayer(&creature, npc);
		CHECK(creature.handed == std::vector<const MItem*>({g_pPacketItemJacket[JACKET2], g_pPacketItemPants[PANTS3],
			g_pPacketItemHelm[HELMET1], g_pPacketItemWeapon[WEAPON_BLADE], g_pPacketItemShield[SHIELD2],
			g_pPacketItemMotorcycle[MOTORCYCLE1]}));
		CHECK(Addon(creature, ADDON_HAIR) == Hair(male ? ADDONID_HAIR2_MALE : ADDONID_HAIR2_FEMALE, 32));
		CHECK_EQ(male ? int(ADDONID_HAIR2_MALE) : int(ADDONID_HAIR2_FEMALE), int(creature.GetHairFrameID()));
		// Skin under the coat and trousers, each item's colour on top; the
		// helmet, weapon, shield and motorcycle take their first set only.
		CHECK(Addon(creature, ADDON_COAT) == Colours(0, 31, 33));
		CHECK(Addon(creature, ADDON_TROUSER) == Colours(1, 31, 34));
		CHECK(Addon(creature, ADDON_HELM) == Colours(0, 35, 0));
		CHECK(Addon(creature, ADDON_RIGHTHAND) == Colours(0, 36, 0));
		CHECK(Addon(creature, ADDON_LEFTHAND) == Colours(0, 37, 0));
		CHECK(Addon(creature, ADDON_MOTOR) == Colours(0, 38, 0));
	}
}

TEST(PacketWear, AnNpcRowPastTheTablesWearsNothingThere)
{
	// The creature data's bytes past a table's end are nothing worn, and a
	// hair past the three takes the first.
	Scene scene;
	PacketTables tables;
	tables.Fill();
	const int npc = 600;
	ITEM_WEARINFO* pWear = new ITEM_WEARINFO;
	testfw::MutableRow(scene.creatureTypes, npc).pItemWearInfo = pWear;
	testfw::MutableRow(scene.creatureTypes, npc).bMale = true;
	pWear->hair = 3;
	pWear->jacket = 255;
	pWear->pants = 255;
	pWear->helmet = 255;
	pWear->weapon = 255;
	pWear->shield = 255;
	pWear->motorcycle = 255;
	Wearer creature;
	SetAddonToSlayer(&creature, npc);
	CHECK(creature.handed == std::vector<const MItem*>(6, nullptr));
	CHECK_EQ(int(ADDONID_HAIR1_MALE), int(creature.GetHairFrameID()));
}

TEST(PacketWear, ANpcWithoutClothesKeepsItsBodyColours)
{
	Scene scene;
	PacketTables tables;
	tables.Fill();
	const int npc = 601;
	ITEM_WEARINFO* pWear = new ITEM_WEARINFO;
	testfw::MutableRow(scene.creatureTypes, npc).pItemWearInfo = pWear;
	testfw::MutableRow(scene.creatureTypes, npc).bMale = true;
	pWear->hair = 2;
	pWear->skinColor = 31;
	pWear->hairColor = 32;
	pWear->jacketColor = 33;
	pWear->pantsColor = 34;
	pWear->helmetColor = 35;
	pWear->weaponColor = 36;
	pWear->shieldColor = 37;
	pWear->motorcycleColor = 38;
	Wearer creature;
	SetAddonToSlayer(&creature, npc);
	// Every outlook 0: the tables' empty slots.
	CHECK(creature.handed == std::vector<const MItem*>(6, nullptr));
	CHECK(Addon(creature, ADDON_HAIR) == Hair(ADDONID_HAIR3_MALE, 32));
	// No item, no item colour: only the skin.
	CHECK(Addon(creature, ADDON_COAT) == Colours(0, 31, 0));
	CHECK(Addon(creature, ADDON_TROUSER) == Colours(1, 31, 0));
	for (int slot : {ADDON_HELM, ADDON_RIGHTHAND, ADDON_LEFTHAND, ADDON_MOTOR})
		CHECK(Addon(creature, slot) == Colours(0, 0, 0));
}

TEST(PacketWear, ATypeWithoutClothesOrPastTheTableIsLeftAlone)
{
	Scene scene;
	PacketTables tables;
	tables.Fill();
	// A row without clothes, one past the last row and one below the first.
	for (int npc : {602, scene.creatureTypes.GetSize(), -1})
	{
		Wearer creature;
		SetAddonToSlayer(&creature, npc);
		CHECK(creature.handed.empty());
		CHECK(Addon(creature, ADDON_HAIR) == Colours(0, 0, 0));
		CHECK(Addon(creature, ADDON_TROUSER) == Colours(0, 0, 0));
	}
}

//----------------------------------------------------------------------
// The player's own slayer (PCSlayerInfo2): hair and body colours
//----------------------------------------------------------------------
TEST(PacketWear, APlayersSlayerTakesItsHairAndSkin)
{
	Scene scene;
	const int male[] = {ADDONID_HAIR1_MALE, ADDONID_HAIR2_MALE, ADDONID_HAIR3_MALE};
	const int female[] = {ADDONID_HAIR1_FEMALE, ADDONID_HAIR2_FEMALE, ADDONID_HAIR3_FEMALE};
	for (Sex sex : {MALE, FEMALE})
		for (int style = HAIR_STYLE1; style <= HAIR_STYLE3; ++style)
		{
			PCSlayerInfo2 info;
			info.setSex(sex);
			info.setHairStyle(HairStyle(style));
			info.setHairColor(41);
			info.setSkinColor(42);
			info.setAdvancementLevel(0);
			Wearer creature;
			SetAddonToSlayer(&creature, &info);
			const int frame = sex == MALE ? male[style] : female[style];
			CHECK(Addon(creature, ADDON_HAIR) == Hair(frame, 41));
			CHECK_EQ(frame, int(creature.GetHairFrameID()));
			CHECK(Addon(creature, ADDON_COAT) == Colours(0, 42, 0));
			CHECK(Addon(creature, ADDON_TROUSER) == Colours(1, 42, 0));
			CHECK(creature.handed.empty());
		}
	// A style past the three the tables hold, as the wire's byte can be,
	// takes the first, as PCSlayerInfo3's does.
	for (Sex sex : {MALE, FEMALE})
		for (int style : {3, 255})
		{
			PCSlayerInfo2 info;
			info.setSex(sex);
			info.setHairStyle(HairStyle(style));
			info.setHairColor(41);
			Wearer creature;
			SetAddonToSlayer(&creature, &info);
			CHECK_EQ(sex == MALE ? int(ADDONID_HAIR1_MALE) : int(ADDONID_HAIR1_FEMALE), int(creature.GetHairFrameID()));
		}
}

TEST(PacketWear, AnAdvancedSlayersCoatTakesItsHairColour)
{
	Scene scene;
	for (int level : {1, 255})
	{
		PCSlayerInfo2 info;
		info.setSex(MALE);
		info.setHairStyle(HAIR_STYLE1);
		info.setHairColor(41);
		info.setSkinColor(42);
		info.setAdvancementLevel(Level_t(level));
		Wearer creature;
		SetAddonToSlayer(&creature, &info);
		CHECK(Addon(creature, ADDON_COAT) == Colours(0, 41, 0));
		CHECK(Addon(creature, ADDON_TROUSER) == Colours(1, 42, 0));
	}
}

//----------------------------------------------------------------------
// The zone view's slayer (PCSlayerInfo3): its whole outlook
//----------------------------------------------------------------------
TEST(PacketWear, AViewedSlayerWearsEachOutlookInItsMainColour)
{
	Scene scene;
	PacketTables tables;
	tables.Fill();
	for (Sex sex : {MALE, FEMALE})
	{
		const PCSlayerInfo3 info = Slayer3(sex, HAIR_STYLE2, HELMET3, JACKET4, PANTS2, WEAPON_AR2, SHIELD4, MOTORCYCLE3, SHOULDER1);
		Wearer creature;
		SetAddonToSlayer(&creature, &info);
		// In this order: a shield after the weapon takes off a two-handed one.
		CHECK(creature.handed == std::vector<const MItem*>({g_pPacketItemJacket[JACKET4], g_pPacketItemPants[PANTS2],
			g_pPacketItemHelm[HELMET3], g_pPacketItemWeapon[WEAPON_AR2], g_pPacketItemShield[SHIELD4],
			g_pPacketItemMotorcycle[MOTORCYCLE3], g_pPacketItemShoulder[SHOULDER1]}));
		CHECK(Addon(creature, ADDON_HAIR) == Hair(sex == MALE ? ADDONID_HAIR2_MALE : ADDONID_HAIR2_FEMALE, 11));
		CHECK(Addon(creature, ADDON_COAT) == Colours(0, 12, 14));
		CHECK(Addon(creature, ADDON_TROUSER) == Colours(1, 12, 15));
		CHECK(Addon(creature, ADDON_HELM) == Colours(0, 13, 13));
		CHECK(Addon(creature, ADDON_RIGHTHAND) == Colours(0, 16, 16));
		CHECK(Addon(creature, ADDON_LEFTHAND) == Colours(0, 17, 17));
		CHECK(Addon(creature, ADDON_MOTOR) == Colours(0, 18, 18));
		// Attack speed 70: fast (66 and under is normal).
		CHECK_EQ(int(MCreature::WEAPON_SPEED_FAST), int(creature.GetWeaponSpeed()));
	}
}

TEST(PacketWear, AViewedSlayerWithoutItemsShowsItsSkin)
{
	Scene scene;
	PacketTables tables;
	tables.Fill();
	PCSlayerInfo3 info = Slayer3(MALE, HAIR_STYLE3, HELMET_NONE, JACKET_BASIC, PANTS_BASIC, WEAPON_NONE, SHIELD_NONE, MOTORCYCLE_NONE, SHOULDER_NONE);
	info.setAttackSpeed(33);
	Wearer creature;
	SetAddonToSlayer(&creature, &info);
	CHECK(creature.handed == std::vector<const MItem*>(7, nullptr));
	CHECK(Addon(creature, ADDON_HAIR) == Hair(ADDONID_HAIR3_MALE, 11));
	CHECK(Addon(creature, ADDON_COAT) == Colours(0, 12, 0));
	CHECK(Addon(creature, ADDON_TROUSER) == Colours(1, 12, 0));
	for (int slot : {ADDON_HELM, ADDON_RIGHTHAND, ADDON_LEFTHAND, ADDON_MOTOR})
		CHECK(Addon(creature, slot) == Colours(0, 0, 0));
	CHECK_EQ(int(MCreature::WEAPON_SPEED_SLOW), int(creature.GetWeaponSpeed()));
}

TEST(PacketWear, AViewedAdvancedSlayersCoatTakesItsHairColour)
{
	Scene scene;
	PacketTables tables;
	tables.Fill();
	PCSlayerInfo3 info = Slayer3(FEMALE, HAIR_STYLE1, HELMET_NONE, JACKET1, PANTS_BASIC, WEAPON_NONE, SHIELD_NONE, MOTORCYCLE_NONE, SHOULDER_NONE);
	info.setAdvancementLevel(1);
	Wearer creature;
	SetAddonToSlayer(&creature, &info);
	CHECK(Addon(creature, ADDON_COAT) == Colours(0, 11, 14));
	CHECK(Addon(creature, ADDON_TROUSER) == Colours(1, 12, 0));
}

TEST(PacketWear, AnOutlookPastItsTableIsNothingWorn)
{
	Scene scene;
	PacketTables tables;
	tables.Fill();
	// Each table's last slot is worn, the value one past it (and the
	// field's largest) is not; the hair style past HAIR_STYLE3 is HAIR1.
	struct Case { HelmetType helmet; JacketType jacket; WeaponType weapon; ShieldType shield; BYTE shoulder; bool worn; };
	const Case cases[] = {
		{HELMET5, JACKET6, WEAPON_MACE1, SHIELD4, SHOULDER1, true},
		{HELMET_MAX, JACKET_MAX, WEAPON_MAX, SHIELD_MAX, SHOULDER_MAX, false},
		{HelmetType(7), JacketType(7), WeaponType(31), ShieldType(7), 3, false},
	};
	for (const Case& test : cases)
		for (PantsType pants : {PANTS4, PANTS_MAX, PantsType(7)})
			for (Sex sex : {MALE, FEMALE})
			{
				const PCSlayerInfo3 info = Slayer3(sex, HairStyle(3), test.helmet, test.jacket, pants, test.weapon, test.shield, MOTORCYCLE_NONE, test.shoulder);
				Wearer creature;
				SetAddonToSlayer(&creature, &info);
				const std::vector<const MItem*> worn = test.worn
					? std::vector<const MItem*>({g_pPacketItemJacket[JACKET6], nullptr, g_pPacketItemHelm[HELMET5],
						g_pPacketItemWeapon[WEAPON_MACE1], g_pPacketItemShield[SHIELD4], nullptr, g_pPacketItemShoulder[SHOULDER1]})
					: std::vector<const MItem*>(7, nullptr);
				CHECK(creature.handed == worn);
				CHECK(Addon(creature, ADDON_HAIR) == Hair(sex == MALE ? ADDONID_HAIR1_MALE : ADDONID_HAIR1_FEMALE, 11));
				CHECK(Addon(creature, ADDON_COAT) == Colours(0, 12, test.worn ? 14 : 0));
				// PANTS4 has no prototype, so the trousers never take a colour.
				CHECK(Addon(creature, ADDON_TROUSER) == Colours(1, 12, 0));
				CHECK(Addon(creature, ADDON_HELM) == (test.worn ? Colours(0, 13, 13) : Colours(0, 0, 0)));
				CHECK(Addon(creature, ADDON_RIGHTHAND) == (test.worn ? Colours(0, 16, 16) : Colours(0, 0, 0)));
				CHECK(Addon(creature, ADDON_LEFTHAND) == (test.worn ? Colours(0, 17, 17) : Colours(0, 0, 0)));
			}
}

//----------------------------------------------------------------------
// An ouster
//----------------------------------------------------------------------
TEST(PacketWear, AnOusterWearsItsRobeAndArmInItsColours)
{
	Scene scene;
	PacketTables tables;
	tables.Fill();
	const std::pair<OustersCoatType, OustersArmType> outlooks[] = {
		{OUSTERS_COAT_BASIC, OUSTERS_ARM_GAUNTLET}, {OUSTERS_COAT3, OUSTERS_ARM_CHAKRAM}, {OUSTERS_COAT4, OUSTERS_ARM_GAUNTLET},
	};
	for (const auto& outlook : outlooks)
	{
		const PCOustersInfo3 info = Ousters3(outlook.first, outlook.second);
		Wearer creature;
		SetAddonToOusters(&creature, &info);
		CHECK(creature.handed == std::vector<const MItem*>({g_pPacketItemOustersCoat[outlook.first], g_pPacketItemOustersArm[outlook.second]}));
		// Hair colour under the robe, the robe's colour on top; the boots and
		// the arm take theirs in both sets.
		CHECK(Addon(creature, ADDON_COAT) == Colours(0, 21, 22));
		CHECK(Addon(creature, ADDON_TROUSER) == Colours(1, 24, 24));
		CHECK(Addon(creature, ADDON_RIGHTHAND) == Colours(0, 23, 23));
		CHECK(Addon(creature, ADDON_HAIR) == Colours(0, 0, 0));
		CHECK_EQ(int(MCreature::WEAPON_SPEED_SLOW), int(creature.GetWeaponSpeed()));
	}
}

TEST(PacketWear, AnOusterIsLeftAloneWithoutItsRobe)
{
	Scene scene;
	PacketTables tables;
	// The tables not filled, then the robe past the table (the field holds
	// 0 to 7): nothing is put on, nothing coloured.
	for (int coat : {int(OUSTERS_COAT1), int(OUSTERS_COAT_MAX), 7})
	{
		if (coat == OUSTERS_COAT_MAX) tables.Fill();
		const PCOustersInfo3 info = Ousters3(OustersCoatType(coat), OUSTERS_ARM_CHAKRAM);
		Wearer creature;
		SetAddonToOusters(&creature, &info);
		CHECK(creature.handed.empty());
		CHECK(Addon(creature, ADDON_COAT) == Colours(0, 0, 0));
		CHECK(Addon(creature, ADDON_TROUSER) == Colours(0, 0, 0));
		CHECK_EQ(int(MCreature::WEAPON_SPEED_NORMAL), int(creature.GetWeaponSpeed()));
	}
	// No creature or no packet: nothing to do.
	const PCOustersInfo3 info = Ousters3(OUSTERS_COAT1, OUSTERS_ARM_CHAKRAM);
	SetAddonToOusters(NULL, &info);
	Wearer creature;
	SetAddonToOusters(&creature, static_cast<const PCOustersInfo3*>(NULL));
	CHECK(creature.handed.empty());
}

TEST(PacketWear, ThePlayersOwnOusterPacketDressesNothing)
{
	Scene scene;
	PacketTables tables;
	tables.Fill();
	PCOustersInfo2 info;
	info.setSex(FEMALE);
	info.setHairColor(21);
	Wearer creature;
	SetAddonToOusters(&creature, &info);
	CHECK(creature.handed.empty());
	for (int slot = 0; slot < ADDON_MAX; ++slot)
		CHECK(Addon(creature, slot) == Colours(0, 0, 0));
}

//----------------------------------------------------------------------
// The face a hair style shows
//----------------------------------------------------------------------
TEST(PacketFaceStyle, EachHairStyleHasItsGendersFace)
{
	CHECK_EQ(int(M_FACE1), UI_GetFaceStyle(true, HAIR_STYLE1));
	CHECK_EQ(int(M_FACE2), UI_GetFaceStyle(true, HAIR_STYLE2));
	CHECK_EQ(int(M_FACE3), UI_GetFaceStyle(true, HAIR_STYLE3));
	CHECK_EQ(int(W_FACE1), UI_GetFaceStyle(false, HAIR_STYLE1));
	CHECK_EQ(int(W_FACE2), UI_GetFaceStyle(false, HAIR_STYLE2));
	CHECK_EQ(int(W_FACE3), UI_GetFaceStyle(false, HAIR_STYLE3));
	// Any other style: 0 (M_OLD_HELMET, W_OLD_HELMET).
	for (bool male : {true, false})
		for (int style : {-1, 3, 255})
			CHECK_EQ(0, UI_GetFaceStyle(male, style));
}

//----------------------------------------------------------------------
// Fake creatures
//----------------------------------------------------------------------
TEST(PacketFakeCreature, ANewFakeStandsWhereItIsMadeInThePlayersZone)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	int zoneMarker = 0;
	g_pZone = reinterpret_cast<MZone*>(&zoneMarker);
	MFakeCreature* pFake = NewFakeCreature(5, 17, 23, 6);
	CHECK(pFake != nullptr);
	CHECK(ZonePeek::Of(*pFake) == g_pZone);
	CHECK_EQ(5, int(pFake->GetCreatureType()));
	CHECK_EQ(17, int(pFake->GetX()));
	CHECK_EQ(23, int(pFake->GetY()));
	// A fake keeps no server position: SetServerPosition skips CLASS_FAKE.
	CHECK_EQ(int(SECTORPOSITION_NULL), int(pFake->GetServerX()));
	CHECK_EQ(int(SECTORPOSITION_NULL), int(pFake->GetServerY()));
	CHECK_EQ(6, int(pFake->GetServerDirection()));
	CHECK_EQ(6, int(pFake->GetDirection()));
	// Standing: the stand's frames counted out (a type alone counts none).
	CHECK_EQ(int(ACTION_STAND), int(pFake->GetAction()));
	CHECK_EQ(10 + ACTION_STAND, int(pFake->GetActionCountMax()));
	delete pFake;
}

TEST(PacketFakeCreature, ACopyTakesTheBodyClothesAndActionsWhereItIsMade)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	int zoneMarker = 0;
	g_pZone = reinterpret_cast<MZone*>(&zoneMarker);
	MCreatureWear original;
	original.SetCreatureType(5);
	original.SetDirection(2);
	original.SetCurrentDirection(5);
	original.SetBodyColor1(51);
	original.SetBodyColor2(52);
	original.SetAddonHair(ADDONID_HAIR3_MALE, 53);
	original.SetAddonColorSet1(ADDON_HELM, 54);
	original.SetWeaponSpeed(MCreature::WEAPON_SPEED_FAST);
	original.SetBasicActionInfo(SKILL_TRIPLE_SLASHER);
	original.SetSpecialActionInfo(SKILL_DOUBLE_IMPACT);
	MFakeCreature* pFake = NewFakeCreature(&original, 17, 23);
	CHECK(ZonePeek::Of(*pFake) == g_pZone);
	CHECK_EQ(5, int(pFake->GetCreatureType()));
	CHECK_EQ(51, int(pFake->GetBodyColor1()));
	CHECK_EQ(52, int(pFake->GetBodyColor2()));
	for (int slot = 0; slot < ADDON_MAX; ++slot)
		CHECK(Addon(*pFake, slot) == Addon(original, slot));
	CHECK_EQ(int(ADDONID_HAIR3_MALE), int(pFake->GetHairFrameID()));
	CHECK_EQ(17, int(pFake->GetX()));
	CHECK_EQ(23, int(pFake->GetY()));
	// A fake keeps no server position: SetServerPosition skips CLASS_FAKE.
	CHECK_EQ(int(SECTORPOSITION_NULL), int(pFake->GetServerX()));
	CHECK_EQ(int(SECTORPOSITION_NULL), int(pFake->GetServerY()));
	// It faces where the original is turned, not where it was told to.
	CHECK_EQ(5, int(pFake->GetServerDirection()));
	CHECK_EQ(5, int(pFake->GetDirection()));
	CHECK_EQ(int(ACTION_STAND), int(pFake->GetAction()));
	CHECK_EQ(10 + ACTION_STAND, int(pFake->GetActionCountMax()));
	CHECK_EQ(int(MCreature::WEAPON_SPEED_FAST), int(pFake->GetWeaponSpeed()));
	CHECK_EQ(int(SKILL_TRIPLE_SLASHER), int(pFake->GetBasicActionInfo()));
	// Taken after the stand, which clears it.
	CHECK_EQ(int(SKILL_DOUBLE_IMPACT), int(pFake->GetSpecialActionInfo()));
	delete pFake;
}

TEST(PacketFakeCreature, ACopyOfACreatureWithoutClothesWearsItsTypesOwn)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	g_pZone = nullptr;
	MCreature original;
	original.SetCreatureType(5);
	original.SetCurrentDirection(1);
	MFakeCreature* pFake = NewFakeCreature(&original, 3, 4);
	CHECK(ZonePeek::Of(*pFake) == nullptr);
	// The slayer type's own coat and trousers, nothing copied.
	MCreatureWear plain;
	plain.SetCreatureType(5);
	for (int slot = 0; slot < ADDON_MAX; ++slot)
		CHECK(Addon(*pFake, slot) == Addon(plain, slot));
	CHECK_EQ(1, int(pFake->GetDirection()));
	delete pFake;
}

//----------------------------------------------------------------------
// The gore level
//----------------------------------------------------------------------
namespace {

const char* kActionFile = "packet_item_table_action.inf";

std::vector<std::string> initFailures;

// A zone that names itself in the list when it is freed, through an
// image object it owns.
std::vector<std::string> zonesFreed;
struct ZoneWitness : MImageObject
{
	std::string name;
	explicit ZoneWitness(std::string zoneName) : MImageObject(1, 0, 0, 0, 0, 0, 0), name(std::move(zoneName)) {}
	~ZoneWitness() override { zonesFreed.push_back(name); }
};

MZone* WitnessedZone(const char* name)
{
	MZone* pZone = new MZone(1, 1);
	pZone->AddImageObject(new ZoneWitness(name));
	return pZone;
}

// The action table, the user's settings, both zones and the file
// definitions the gore level reads, every one given back after.
struct GoreScene
{
	GameModelWorld world;
	MActionInfoTable actions;
	Properties definitions;
	GameInitInfoHost::Host host;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	Properties* oldDefinitions = g_pFileDef;
	const GameInitInfoHost::Host* oldHost = nullptr;
	MZone* oldZone = g_pZone;
	MZone* oldLarge = g_pZoneLarge;
	MZone* oldSmall = g_pZoneSmall;
	int oldLargeID = g_nZoneLarge, oldSmallID = g_nZoneSmall;
	bool oldLargeImage = g_bZoneLargeLoadImage, oldSmallImage = g_bZoneSmallLoadImage;
	int zoneMarker = 0;

	GoreScene()
	{
		initFailures.clear();
		zonesFreed.clear();
		actions.Init(MAX_ACTIONINFO);
		for (int info = 0; info < MAX_ACTIONINFO; ++info)
			actions.GetMutable(info)->Set("", ACTION_STAND, EFFECTSPRITETYPE_NULL, 0, 0);
		g_pActionInfoTable = &actions;
		// Made before FileDef names anything: a zone reads the horn file
		// FileDef names.
		g_pFileDef = nullptr;
		g_pZoneSmall = WitnessedZone("small");
		g_pZoneLarge = WitnessedZone("large");
		g_nZoneSmall = 13;
		g_nZoneLarge = 14;
		g_bZoneSmallLoadImage = true;
		g_bZoneLargeLoadImage = true;
		g_pZone = reinterpret_cast<MZone*>(&zoneMarker);
		definitions.setProperty("FILE_INFO_ACTION", kActionFile);
		g_pFileDef = &definitions;
		host.InitFailMessage = [](const char* message) { initFailures.push_back(message); };
		oldHost = GameInitInfoHost::SetHost(&host);
	}
	~GoreScene()
	{
		GameInitInfoHost::SetHost(oldHost);
		std::remove(kActionFile);
		delete g_pZoneSmall;
		delete g_pZoneLarge;
		g_pZone = oldZone;
		g_pZoneLarge = oldLarge;
		g_pZoneSmall = oldSmall;
		g_nZoneLarge = oldLargeID;
		g_nZoneSmall = oldSmallID;
		g_bZoneLargeLoadImage = oldLargeImage;
		g_bZoneSmallLoadImage = oldSmallImage;
		g_pFileDef = oldDefinitions;
		g_pActionInfoTable = oldActions;
	}
	// Both zones gone and unnamed, the player's zone forgotten.
	bool ZonesReleased() const
	{
		return zonesFreed == std::vector<std::string>({"small", "large"})
			&& g_pZoneSmall == nullptr && g_pZoneLarge == nullptr
			&& g_nZoneSmall == ZONEID_NULL && g_nZoneLarge == ZONEID_NULL
			&& !g_bZoneSmallLoadImage && !g_bZoneLargeLoadImage
			&& g_pZone == nullptr;
	}
};

// An action table file: its result range, then no actions.
void SaveActionFile(DWORD minResult, DWORD maxResult)
{
	MActionInfoTable table;
	table.SetMinResultActionInfo(minResult);
	table.SetMaxResultActionInfo(maxResult);
	std::ofstream out(kActionFile, std::ios::binary | std::ios::trunc);
	table.SaveToFile(out);
}

} // namespace

TEST(PacketGoreLevel, TurningItOffBleedsGreenAndDropsTheZones)
{
	GoreScene scene;
	g_pUserInformation->GoreLevel = true;
	scene.actions.GetMutable(SKILL_ATTACK_MELEE)->Set("", ACTION_ATTACK, EFFECTSPRITETYPE_BLOOD_GUN_1_1, 0, 0);
	SetGoreLevel(false);
	// The child mode: green blood, and a vampire's death starts at frame 8.
	CHECK_EQ(int(EFFECTSPRITETYPE_GREEN_BLOOD_GUN_1_1), int(scene.actions[SKILL_ATTACK_MELEE].GetActionEffectSpriteType()));
	for (int speed = 0; speed < 3; ++speed)
	{
		CHECK_EQ(8, scene.actions[RESULT_VAMPIRE_DIE].GetStartFrame(speed));
		CHECK_EQ(8, scene.actions[RESULT_VAMPIRE_DIE].GetCastingStartFrame(speed));
	}
	CHECK(scene.ZonesReleased());
	CHECK_EQ(false, g_pUserInformation->GoreLevel);
	CHECK(initFailures.empty());
}

TEST(PacketGoreLevel, TurningItOnReloadsTheActionsAndDropsTheZones)
{
	GoreScene scene;
	g_pUserInformation->GoreLevel = false;
	SaveActionFile(123, 456);
	SetGoreLevel(true);
	// The file's table replaced the live one.
	CHECK_EQ(0, scene.actions.GetSize());
	CHECK_EQ(123, int(scene.actions.GetMinResultActionInfo()));
	CHECK_EQ(456, int(scene.actions.GetMaxResultActionInfo()));
	CHECK(scene.ZonesReleased());
	CHECK_EQ(true, g_pUserInformation->GoreLevel);
	CHECK(initFailures.empty());
}

TEST(PacketGoreLevel, WithoutTheActionFileNothingChanges)
{
	GoreScene scene;
	g_pUserInformation->GoreLevel = false;
	std::remove(kActionFile);
	SetGoreLevel(true);
	CHECK(initFailures == std::vector<std::string>({std::string("[Error] File Open Error : [") + kActionFile + "]"}));
	CHECK_EQ(int(MAX_ACTIONINFO), scene.actions.GetSize());
	CHECK(zonesFreed.empty());
	CHECK(g_pZoneSmall != nullptr);
	CHECK(g_pZoneLarge != nullptr);
	CHECK_EQ(13, g_nZoneSmall);
	CHECK_EQ(14, g_nZoneLarge);
	CHECK(g_bZoneSmallLoadImage);
	CHECK(g_bZoneLargeLoadImage);
	CHECK(g_pZone == reinterpret_cast<MZone*>(&scene.zoneMarker));
	CHECK_EQ(false, g_pUserInformation->GoreLevel);
}

TEST(PacketGoreLevel, WithoutZonesOnlyThePlayersZoneIsForgotten)
{
	GoreScene scene;
	delete g_pZoneSmall;
	delete g_pZoneLarge;
	g_pZoneSmall = nullptr;
	g_pZoneLarge = nullptr;
	zonesFreed.clear();
	g_pUserInformation->GoreLevel = true;
	SetGoreLevel(false);
	// The names and image flags belong to a zone that is not there: kept.
	CHECK(zonesFreed.empty());
	CHECK_EQ(13, g_nZoneSmall);
	CHECK_EQ(14, g_nZoneLarge);
	CHECK(g_bZoneSmallLoadImage);
	CHECK(g_bZoneLargeLoadImage);
	CHECK(g_pZone == nullptr);
	CHECK_EQ(false, g_pUserInformation->GoreLevel);
}
