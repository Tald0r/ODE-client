#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "MCreatureWear.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MActionInfoTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "MEffectStatusDef.h"
#include "EffectSpriteTypeDef.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "MItem.h"
#include "MItemOptionTable.h"
#include "AddonDef.h"
#include "SkillDef.h"
#include "MEffect.h"
#include "MAttachEffect.h"
#include "MEventManager.h"
#include "MEventQueue.h"
#include "FrameClock.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"
#include "../basic/ColorSetDef.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

// MItemUse.cpp's, reached through MItemHost.
MItem*	NewItemOfClass(ITEM_CLASS itemClass);

// The creature that wears gear (task 4.243, gamemodel's since then): what
// it wears by race and gender, what an item it puts on or takes off does,
// and what it asks of the world, through hosts that record each call.
namespace {

std::vector<std::string> calls;
std::string N(long long value) { return std::to_string(value); }
using Calls = std::vector<std::string>;

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

int maxEffectFrame = 7;

const WorldEvents::Host worldHost = []() {
	WorldEvents::Host host{};
	host.ViewExists = []() { return true; };
	host.GetMaxEffectFrame = [](int bltType, std::uint16_t frameID) { calls.push_back("max frame|" + N(bltType) + "|" + N(frameID)); return maxEffectFrame; };
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int startZ, int direction, TYPE_OBJECTID targetID,
		TYPE_SECTORPOSITION targetX, TYPE_SECTORPOSITION targetY, int targetZ, std::uint32_t delayFrame, MActionResult* pResult, bool bStartFromMainNode, int sX, int sY) {
		calls.push_back("skill|" + N(actionInfo) + "|" + N(startX) + "," + N(startY) + "," + N(startZ) + "|" + N(direction) + "|" + N(targetID)
			+ "|" + N(targetX) + "," + N(targetY) + "," + N(targetZ) + "|" + N(delayFrame) + "|" + (pResult != NULL ? "result" : "none")
			+ "|" + (bStartFromMainNode ? "main" : "first") + "|" + N(sX) + "," + N(sY));
	};
	return host;
}();
const ZoneState::Host zoneHost = []() {
	ZoneState::Host host{};
	host.Exists = []() { return true; };
	return host;
}();
const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.Creature = []() -> MCreature* { return nullptr; };
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
const MItemHost itemHost = []() {
	MItemHost host{};
	host.NewItem = NewItemOfClass;
	return host;
}();

// The player's own class: it puts gear on and off only while standing.
struct PlayerWear : MCreatureWear
{
	CLASS_TYPE GetClassType() const override { return CLASS_PLAYER; }
};

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
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&worldHost);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zoneHost);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&playerHost);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creatureHost);
	const MEffectHost* oldEffectHost = MEffect::SetHost(&effectHost);
	const MAttachEffectHost* oldAttachHost = MAttachEffect::SetHost(&attachHost);

	Scene()
	{
		calls.clear();
		maxEffectFrame = 7;
		MItem::SetHost(&itemHost);
		creatureTypes.Init(CREATURETYPE_INSTALL_TURRET + 1);
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
		// The option table's "no option" colour.
		g_pItemOptionTable->GetMutable(0)->ColorSet = 33;
	}
	~Scene()
	{
		MItem::SetHost(NULL);
		MAttachEffect::SetHost(oldAttachHost);
		MEffect::SetHost(oldEffectHost);
		CreatureState::SetHost(oldCreature);
		PlayerState::SetHost(oldPlayer);
		ZoneState::SetHost(oldZone);
		WorldEvents::SetHost(oldWorld);
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
	// A creature type of the tribe, with a count for each action and body
	// colour 40.
	void Tribe(int type, CREATURETRIBE tribe)
	{
		testfw::MutableRow(creatureTypes, type).SetCreatureTribe(tribe, ACTION_MAX_SLAYER);
		testfw::MutableRow(creatureTypes, type).ColorSet = 40;
		for (int action = 0; action < ACTION_MAX_SLAYER; ++action)
			testfw::MutableRow(creatureTypes, type).SetActionCount(action, 10 + action);
	}
	// An item of the class (type 1), its frames 11 (male) and 12 (female).
	std::unique_ptr<MItem> Item(ITEM_CLASS itemClass)
	{
		ITEMTYPE_TABLE& types = testfw::MutableRow(*g_pItemTable, itemClass);
		if (types.GetSize() < 2) types.Init(2);
		testfw::MutableRow(types, 1).AddonMaleFrameID = 11;
		testfw::MutableRow(types, 1).AddonFemaleFrameID = 12;
		std::unique_ptr<MItem> pItem(MItem::NewItem(itemClass));
		pItem->SetItemType(1);
		return pItem;
	}
};

const int Slayer = 5, Vampire = 6, Ousters = 7;

// A wearer of the type and gender (its table row's: the type sets the
// gender), placed.
void Dress(MCreatureWear& creature, int type, bool male = true)
{
	testfw::MutableRow(*g_pCreatureTable, type).bMale = male;
	creature.SetCreatureType(type);
	creature.SetID(900);
	creature.SetPosition(12, 34);
	creature.SetDirection(3);
	creature.SetCurrentDirection(5);
}

std::string Addon(const MCreatureWear& creature, int slot)
{
	const MCreatureWear::ADDON_INFO& addon = creature.GetAddonInfo(slot);
	return N(addon.bAddon) + "|" + N(addon.FrameID) + "|" + N(addon.ItemClass) + "|" + N(addon.ItemType)
		+ "|" + N(addon.ColorSet1) + "|" + N(addon.ColorSet2);
}

const int CoatColor = 377, TrouserColor = 377, SkinColor = 410;	// MCreatureWear.cpp's defaults

} // namespace

//----------------------------------------------------------------------
// Dressed by race and gender
//----------------------------------------------------------------------
TEST(CreatureWear, ANewWearerWearsNothing)
{
	Scene scene;
	MCreatureWear creature;
	for (int slot = 0; slot < ADDON_MAX; ++slot)
		CHECK(Addon(creature, slot) == "0|" + N(FRAMEID_NULL) + "|" + N(ITEM_CLASS_NULL) + "|" + N(ITEMTYPE_NULL) + "|0|0");
	CHECK_EQ(ADDONID_HAIR1_MALE, int(creature.GetHairFrameID()));
	CHECK(creature.IsWear());
	CHECK_EQ(int(MCreature::CLASS_CREATUREWEAR), int(creature.GetClassType()));
}

TEST(CreatureWear, ASlayerWearsItsGendersCoatAndTrousers)
{
	Scene scene;
	scene.Tribe(Slayer, CREATURETRIBE_SLAYER);
	for (bool male : {true, false})
	{
		MCreatureWear creature;
		Dress(creature, Slayer, male);
		CHECK(Addon(creature, ADDON_COAT) == "1|" + N(male ? ADDONID_COAT0_MALE : ADDONID_COAT0_FEMALE) + "|" + N(ITEM_CLASS_COAT) + "|0|40|" + N(CoatColor));
		CHECK(Addon(creature, ADDON_TROUSER) == "1|" + N(male ? ADDONID_TROUSER0_MALE : ADDONID_TROUSER0_FEMALE) + "|" + N(ITEM_CLASS_TROUSER) + "|0|40|" + N(TrouserColor));
		CHECK_EQ(0, int(creature.GetAddonInfo(ADDON_HELM).bAddon));
	}
}

TEST(CreatureWear, AVampireWearsItsCoatAndAnOustersItsRobe)
{
	Scene scene;
	scene.Tribe(Vampire, CREATURETRIBE_VAMPIRE);
	scene.Tribe(Ousters, CREATURETRIBE_OUSTERS);
	MCreatureWear vampire;
	Dress(vampire, Vampire);
	CHECK(Addon(vampire, ADDON_COAT) == "1|" + N(FRAMEID_NULL) + "|" + N(ITEM_CLASS_VAMPIRE_COAT) + "|0|40|" + N(CoatColor));
	CHECK_EQ(0, int(vampire.GetAddonInfo(ADDON_TROUSER).bAddon));
	MCreatureWear ousters;
	Dress(ousters, Ousters);
	CHECK(Addon(ousters, ADDON_COAT) == "1|1|" + N(ITEM_CLASS_OUSTERS_COAT) + "|0|40|" + N(CoatColor));
	CHECK_EQ(0, int(ousters.GetAddonInfo(ADDON_TROUSER).bAddon));
}

TEST(CreatureWear, ANewTypeTakesOffWhatItWore)
{
	Scene scene;
	scene.Tribe(Slayer, CREATURETRIBE_SLAYER);
	MCreatureWear creature;
	creature.SetAddonHair(99, 5);
	creature.SetAddonColorSet1(ADDON_HELM, 7);
	creature.SetCreatureType(Slayer);
	CHECK_EQ(0, int(creature.GetAddonInfo(ADDON_HAIR).bAddon));
	CHECK_EQ(0, int(creature.GetAddonInfo(ADDON_HELM).ColorSet1));
}

//----------------------------------------------------------------------
// Colours and hair
//----------------------------------------------------------------------
TEST(CreatureWear, AColourSetMustBeOneOfTheSetsOrAnItemsMark)
{
	Scene scene;
	MCreatureWear creature;
	for (WORD set : {WORD(0), WORD(MAX_COLORSET - 1), WORD(UNIQUE_ITEM_COLOR), WORD(QUEST_ITEM_COLOR)})
	{
		creature.SetAddonColorSet1(ADDON_HELM, set);
		creature.SetAddonColorSet2(ADDON_HELM, set);
		CHECK_EQ(int(set), int(creature.GetAddonInfo(ADDON_HELM).ColorSet1));
		CHECK_EQ(int(set), int(creature.GetAddonInfo(ADDON_HELM).ColorSet2));
	}
	// Past the sets: kept as it was.
	creature.SetAddonColorSet1(ADDON_HELM, 3);
	creature.SetAddonColorSet2(ADDON_HELM, 4);
	creature.SetAddonColorSet1(ADDON_HELM, MAX_COLORSET);
	creature.SetAddonColorSet2(ADDON_HELM, MAX_COLORSET);
	CHECK_EQ(3, int(creature.GetAddonInfo(ADDON_HELM).ColorSet1));
	CHECK_EQ(4, int(creature.GetAddonInfo(ADDON_HELM).ColorSet2));
	// A colour puts nothing on, except trousers.
	CHECK_EQ(0, int(creature.GetAddonInfo(ADDON_HELM).bAddon));
	creature.SetAddonColorSet1(ADDON_TROUSER, 3);
	CHECK_EQ(1, int(creature.GetAddonInfo(ADDON_TROUSER).bAddon));
	MCreatureWear other;
	other.SetAddonColorSet2(ADDON_TROUSER, 3);
	CHECK_EQ(1, int(other.GetAddonInfo(ADDON_TROUSER).bAddon));
	// No such slot: nothing (the shoulder, kept after the slots, stays).
	creature.SetAddonColorSet1(ADDON_MAX, 3);
	creature.SetAddonColorSet2(ADDON_MAX, 3);
	CHECK_EQ(0, int(creature.GetACAddonInfo(AC_ADDON_SHOULDER).ColorSet1));
	CHECK_EQ(0, int(creature.GetACAddonInfo(AC_ADDON_SHOULDER).ColorSet2));
}

TEST(CreatureWear, ItsArmourCodeSlotsAreItsGearSlots)
{
	Scene scene;
	scene.Tribe(Slayer, CREATURETRIBE_SLAYER);
	MCreatureWear creature;
	Dress(creature, Slayer);
	const std::pair<int, int> slots[] = {
		{AC_ADDON_BODY, ADDON_COAT}, {AC_ADDON_HELM, ADDON_HELM}, {AC_ADDON_LEFTHAND, ADDON_LEFTHAND},
		{AC_ADDON_RIGHTHAND, ADDON_RIGHTHAND}, {AC_ADDON_MOTOR, ADDON_MOTOR},
	};
	for (const auto& slot : slots)
	{
		creature.SetAddonColorSet1(slot.second, WORD(100 + slot.second));
		CHECK_EQ(100 + slot.second, int(creature.GetACAddonInfo(slot.first).ColorSet1));
	}
	// The shoulder (and any other): kept apart, and nothing wears it.
	CHECK_EQ(0, int(creature.GetACAddonInfo(AC_ADDON_SHOULDER).bAddon));
	CHECK_EQ(0, int(creature.GetACAddonInfo(AC_ADDON_SHOULDER).ColorSet1));
}

TEST(CreatureWear, AnOperatorsCoatColoursItsBody)
{
	Scene scene;
	scene.Tribe(CREATURETYPE_SLAYER_OPERATOR, CREATURETRIBE_SLAYER);
	scene.Tribe(Slayer, CREATURETRIBE_SLAYER);
	MCreatureWear op;
	Dress(op, CREATURETYPE_SLAYER_OPERATOR);
	op.SetAddonColorSet2(ADDON_COAT, 21);
	CHECK_EQ(21, int(op.GetBodyColor1()));
	CHECK_EQ(21, int(op.GetBodyColor2()));
	// Not its coat, or not an operator: the body keeps its colours.
	op.SetAddonColorSet2(ADDON_TROUSER, 22);
	CHECK_EQ(21, int(op.GetBodyColor1()));
	MCreatureWear plain;
	Dress(plain, Slayer);
	plain.SetBodyColor1(40);
	plain.SetBodyColor2(41);
	plain.SetAddonColorSet2(ADDON_COAT, 21);
	CHECK_EQ(40, int(plain.GetBodyColor1()));
	CHECK_EQ(41, int(plain.GetBodyColor2()));
}

TEST(CreatureWear, HairIsWornAndCopiedWithTheRest)
{
	Scene scene;
	scene.Tribe(Slayer, CREATURETRIBE_SLAYER);
	MCreatureWear creature;
	Dress(creature, Slayer);
	creature.SetAddonHair(ADDONID_HAIR2_MALE, 9);
	CHECK(Addon(creature, ADDON_HAIR) == "1|" + N(ADDONID_HAIR2_MALE) + "|" + N(ITEM_CLASS_NULL) + "|" + N(ITEMTYPE_NULL) + "|9|0");
	CHECK_EQ(ADDONID_HAIR2_MALE, int(creature.GetHairFrameID()));
	MCreatureWear twin;
	twin.SetSameWear(&creature);
	for (int slot = 0; slot < ADDON_MAX; ++slot)
		CHECK(Addon(twin, slot) == Addon(creature, slot));
	CHECK_EQ(ADDONID_HAIR2_MALE, int(twin.GetHairFrameID()));
}

//----------------------------------------------------------------------
// Putting gear on
//----------------------------------------------------------------------
TEST(CreatureWear, ACoatIsWornByGenderInItsOptionsColour)
{
	Scene scene;
	scene.Tribe(Slayer, CREATURETRIBE_SLAYER);
	for (bool male : {true, false})
	{
		MCreatureWear creature;
		Dress(creature, Slayer, male);
		std::unique_ptr<MItem> coat = scene.Item(ITEM_CLASS_COAT);
		CHECK(creature.SetAddonItem(coat.get()));
		// Skin underneath, the option's colour (33) on top.
		CHECK(Addon(creature, ADDON_COAT) == "1|" + N(male ? 11 : 12) + "|" + N(ITEM_CLASS_COAT) + "|1|40|33");
		CHECK_EQ(33, int(creature.GetBodyColor2()));
	}
}

TEST(CreatureWear, ASlayersItemWithoutAFrameIsNotWorn)
{
	Scene scene;
	scene.Tribe(Slayer, CREATURETRIBE_SLAYER);
	MCreatureWear creature;
	Dress(creature, Slayer);
	std::unique_ptr<MItem> helm = scene.Item(ITEM_CLASS_HELM);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_HELM, 1).AddonMaleFrameID = FRAMEID_NULL;
	CHECK(creature.SetAddonItem(helm.get()));
	CHECK_EQ(0, int(creature.GetAddonInfo(ADDON_HELM).bAddon));
}

TEST(CreatureWear, AHelmTakesOffTheDefaultHairOnly)
{
	Scene scene;
	scene.Tribe(Slayer, CREATURETRIBE_SLAYER);
	for (int hair : {int(ADDONID_HAIR1_MALE), int(ADDONID_HAIR2_MALE), int(ADDONID_HAIR3_MALE), int(ADDONID_HAIR1_FEMALE), 99})
	{
		MCreatureWear creature;
		Dress(creature, Slayer);
		creature.SetAddonHair(TYPE_FRAMEID(hair), 9);
		std::unique_ptr<MItem> helm = scene.Item(ITEM_CLASS_HELM);
		CHECK(creature.SetAddonItem(helm.get()));
		CHECK_EQ(hair == 99 ? 1 : 0, int(creature.GetAddonInfo(ADDON_HAIR).bAddon));
		// Off again, the default hair comes back.
		CHECK(creature.RemoveAddonItem(helm.get()));
		CHECK_EQ(0, int(creature.GetAddonInfo(ADDON_HELM).bAddon));
		CHECK_EQ(1, int(creature.GetAddonInfo(ADDON_HAIR).bAddon));
	}
}

TEST(CreatureWear, ATwoHandedWeaponAndAShieldTakeEachOtherOff)
{
	Scene scene;
	scene.Tribe(Slayer, CREATURETRIBE_SLAYER);
	MCreatureWear creature;
	Dress(creature, Slayer);
	std::unique_ptr<MItem> shield = scene.Item(ITEM_CLASS_SHIELD);
	std::unique_ptr<MItem> blade = scene.Item(ITEM_CLASS_BLADE);
	std::unique_ptr<MItem> sword = scene.Item(ITEM_CLASS_SWORD);
	CHECK(creature.SetAddonItem(shield.get()));
	CHECK_EQ(1, int(creature.GetAddonInfo(ADDON_LEFTHAND).bAddon));
	// A one-handed sword leaves the shield, and a shield the sword.
	CHECK(creature.SetAddonItem(sword.get()));
	CHECK_EQ(1, int(creature.GetAddonInfo(ADDON_LEFTHAND).bAddon));
	CHECK(creature.SetAddonItem(shield.get()));
	CHECK_EQ(int(ITEM_CLASS_SWORD), int(creature.GetAddonInfo(ADDON_RIGHTHAND).ItemClass));
	CHECK_EQ(1, int(creature.GetAddonInfo(ADDON_RIGHTHAND).bAddon));
	// A two-handed blade takes it off.
	CHECK(creature.SetAddonItem(blade.get()));
	CHECK_EQ(0, int(creature.GetAddonInfo(ADDON_LEFTHAND).bAddon));
	CHECK_EQ(int(ITEM_CLASS_BLADE), int(creature.GetAddonInfo(ADDON_RIGHTHAND).ItemClass));
	// A shield takes the two-handed blade off.
	CHECK(creature.SetAddonItem(shield.get()));
	CHECK_EQ(0, int(creature.GetAddonInfo(ADDON_RIGHTHAND).bAddon));
	CHECK_EQ(1, int(creature.GetAddonInfo(ADDON_LEFTHAND).bAddon));
}

TEST(CreatureWear, AWeaponSetsItsBasicAttack)
{
	Scene scene;
	scene.Tribe(Slayer, CREATURETRIBE_SLAYER);
	MCreatureWear creature;
	Dress(creature, Slayer);
	std::unique_ptr<MItem> sword = scene.Item(ITEM_CLASS_SWORD);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 1).UseActionInfo = SKILL_DOUBLE_IMPACT;
	CHECK(creature.SetAddonItem(sword.get()));
	CHECK_EQ(int(SKILL_DOUBLE_IMPACT), int(creature.GetBasicActionInfo()));
	// None of its own: a plain melee attack; and taken off, too.
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 1).UseActionInfo = ACTIONINFO_NULL;
	CHECK(creature.SetAddonItem(sword.get()));
	CHECK_EQ(int(SKILL_ATTACK_MELEE), int(creature.GetBasicActionInfo()));
	creature.SetBasicActionInfo(SKILL_DOUBLE_IMPACT);
	CHECK(creature.RemoveAddonItem(sword.get()));
	CHECK_EQ(int(SKILL_ATTACK_MELEE), int(creature.GetBasicActionInfo()));
}

TEST(CreatureWear, AMotorcycleIsRiddenAndLeftOnFoot)
{
	Scene scene;
	scene.Tribe(Slayer, CREATURETRIBE_SLAYER);
	MCreatureWear creature;
	Dress(creature, Slayer);
	std::unique_ptr<MItem> bike = scene.Item(ITEM_CLASS_MOTORCYCLE);
	CHECK(creature.SetAddonItem(bike.get()));
	CHECK_EQ(int(MCreature::MOVE_DEVICE_RIDE), int(creature.GetMoveDevice()));
	CHECK(creature.RemoveAddonItem(bike.get()));
	CHECK_EQ(int(MCreature::MOVE_DEVICE_WALK), int(creature.GetMoveDevice()));
}

TEST(CreatureWear, ThePlayerChangesOnlyWhileStanding)
{
	Scene scene;
	scene.Tribe(Slayer, CREATURETRIBE_SLAYER);
	PlayerWear player;
	Dress(player, Slayer);
	std::unique_ptr<MItem> helm = scene.Item(ITEM_CLASS_HELM);
	player.SetAction(ACTION_ATTACK);
	CHECK(!player.SetAddonItem(helm.get()));
	CHECK(!player.RemoveAddonItem(helm.get()));
	CHECK_EQ(0, int(player.GetAddonInfo(ADDON_HELM).bAddon));
	// Anyone else is stood up first.
	MCreatureWear other;
	Dress(other, Slayer);
	other.SetAction(ACTION_ATTACK);
	CHECK(other.SetAddonItem(helm.get()));
	CHECK_EQ(1, int(other.GetAddonInfo(ADDON_HELM).bAddon));
	CHECK(!other.SetAddonItem(NULL));
	CHECK(!other.RemoveAddonItem(NULL));
}

//----------------------------------------------------------------------
// Taking gear off
//----------------------------------------------------------------------
TEST(CreatureWear, TakenOffArmourLeavesTheDefaults)
{
	Scene scene;
	scene.Tribe(Slayer, CREATURETRIBE_SLAYER);
	scene.Tribe(Ousters, CREATURETRIBE_OUSTERS);
	MCreatureWear creature;
	Dress(creature, Slayer, false);
	std::unique_ptr<MItem> coat = scene.Item(ITEM_CLASS_COAT);
	std::unique_ptr<MItem> trousers = scene.Item(ITEM_CLASS_TROUSER);
	CHECK(creature.SetAddonItem(coat.get()));
	CHECK(creature.SetAddonItem(trousers.get()));
	CHECK(creature.RemoveAddonItem(coat.get()));
	CHECK(creature.RemoveAddonItem(trousers.get()));
	CHECK(Addon(creature, ADDON_COAT) == "1|" + N(ADDONID_COAT0_FEMALE) + "|" + N(ITEM_CLASS_COAT) + "|0|40|" + N(CoatColor));
	CHECK(Addon(creature, ADDON_TROUSER) == "1|" + N(ADDONID_TROUSER0_FEMALE) + "|" + N(ITEM_CLASS_TROUSER) + "|0|40|" + N(TrouserColor));
	CHECK_EQ(CoatColor, int(creature.GetBodyColor2()));
	// An ousters' robe comes back as frame 1 in the coat colour.
	MCreatureWear ousters;
	Dress(ousters, Ousters);
	CHECK(ousters.RemoveAddonItem(coat.get()));
	CHECK(Addon(ousters, ADDON_COAT) == "1|1|" + N(ITEM_CLASS_COAT) + "|0|" + N(CoatColor) + "|" + N(CoatColor));
	// An operator's body goes back to the skin colour.
	scene.Tribe(CREATURETYPE_SLAYER_OPERATOR, CREATURETRIBE_SLAYER);
	MCreatureWear op;
	Dress(op, CREATURETYPE_SLAYER_OPERATOR);
	CHECK(op.RemoveAddonItem(coat.get()));
	CHECK_EQ(SkinColor, int(op.GetBodyColor1()));
	CHECK_EQ(SkinColor, int(op.GetBodyColor2()));
}

TEST(CreatureWear, RemovingASlotTakesOffItsItem)
{
	Scene scene;
	scene.Tribe(Slayer, CREATURETRIBE_SLAYER);
	MCreatureWear creature;
	Dress(creature, Slayer);
	std::unique_ptr<MItem> helm = scene.Item(ITEM_CLASS_HELM);
	CHECK(creature.SetAddonItem(helm.get()));
	CHECK(creature.RemoveAddon(ADDON_HELM));
	CHECK_EQ(0, int(creature.GetAddonInfo(ADDON_HELM).bAddon));
	// Nothing there, or no such slot: nothing to remove.
	CHECK(!creature.RemoveAddon(ADDON_HELM));
	CHECK(!creature.RemoveAddon(ADDON_MAX));
	// Worn without an item (hair): cleared.
	creature.SetAddonHair(99, 9);
	CHECK(creature.RemoveAddon(ADDON_HAIR));
	CHECK_EQ(0, int(creature.GetAddonInfo(ADDON_HAIR).bAddon));
}

//----------------------------------------------------------------------
// What it asks of the world
//----------------------------------------------------------------------
TEST(CreatureWear, ItsPoolMakesAndFreesIt)
{
	Scene scene;
	std::unique_ptr<MCreatureWear> pCreature(new MCreatureWear);
	CHECK(pCreature->IsWear());
	pCreature.reset();
	MCreatureWear* pAgain = new MCreatureWear;
	delete pAgain;
}

TEST(CreatureWear, AnAdvancedSylphEndsWithItsSkill)
{
	Scene scene;
	scene.Tribe(Ousters, CREATURETRIBE_OUSTERS);
	MCreatureWear plain;
	Dress(plain, Ousters);
	plain.AddEffectStatus(EFFECTSTATUS_SUMMON_SYLPH, 40);
	MCreatureWear advanced;
	Dress(advanced, Ousters);
	advanced.SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
	advanced.AddEffectStatus(EFFECTSTATUS_SUMMON_SYLPH, 40);
	MCreatureWear coloured;
	Dress(coloured, Ousters);
	coloured.SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
	coloured.AddEffectStatus(EFFECTSTATUS_SUMMON_SYLPH_RED, 40);
	calls.clear();
	advanced.RemoveEffectStatus(EFFECTSTATUS_SUMMON_SYLPH);
	CHECK(!calls.empty() && calls.back() == "skill|" + N(SKILL_CLIENT_ADVANCEMENT_SUMMON_SYLPH_END) + "|12,34,0|5|900|12,34,0|8|none|first|0,0");
	calls.clear();
	plain.RemoveEffectStatus(EFFECTSTATUS_SUMMON_SYLPH);
	coloured.RemoveEffectStatus(EFFECTSTATUS_SUMMON_SYLPH_RED);
	for (const std::string& call : calls)
		CHECK(call.rfind("skill|", 0) != 0);
	CHECK_EQ(int(MCreature::MOVE_DEVICE_WALK), int(advanced.GetMoveDevice()));
}

TEST(CreatureWear, AVampireComingToRisesByItsBlood)
{
	Scene scene;
	scene.Tribe(Vampire, CREATURETRIBE_VAMPIRE);
	scene.Tribe(Slayer, CREATURETRIBE_SLAYER);
	MCreatureWear vampire;
	Dress(vampire, Vampire);
	vampire.AddEffectStatus(EFFECTSTATUS_COMA, 40);
	calls.clear();
	vampire.RemoveEffectStatus(EFFECTSTATUS_COMA);
	CHECK(Same({"skill|" + N(BLOOD_RESURRECT) + "|12,34,0|3|900|12,34,0|32|none|first|0,0"}));
	CHECK(vampire.IsAlive());
	MCreatureWear slayer;
	Dress(slayer, Slayer);
	slayer.AddEffectStatus(EFFECTSTATUS_COMA, 40);
	calls.clear();
	slayer.RemoveEffectStatus(EFFECTSTATUS_COMA);
	CHECK(Same({}));
	CHECK(slayer.IsAlive());
}

TEST(CreatureWear, AnOustersFastMoveSpriteFollowsItsAction)
{
	Scene scene;
	scene.Tribe(Ousters, CREATURETRIBE_OUSTERS);
	scene.effectSprites.GetMutable(EFFECTSPRITETYPE_FAST_MOVE_STOP)->BltType = BLT_SCREEN;
	scene.effectSprites.GetMutable(EFFECTSPRITETYPE_FAST_MOVE_STOP)->FrameID = 41;
	scene.effectSprites.GetMutable(EFFECTSPRITETYPE_FAST_MOVE_FLY)->FrameID = 42;
	// Standing, the flying sprite becomes the hovering one, at its frames.
	MCreatureWear stander;
	Dress(stander, Ousters);
	stander.SetMoveDevice(MCreature::MOVE_DEVICE_SUMMON_SYLPH);
	MAttachEffect* pFly = stander.CreateAttachEffect(EFFECTSPRITETYPE_FAST_MOVE_FLY, 50, 50);
	stander.SetAction(ACTION_STAND);
	CHECK_EQ(ACTION_OUSTERS_FAST_MOVE_STAND, int(stander.GetAction()));
	calls.clear();
	stander.UpdateAttachEffect();
	CHECK(Same({"max frame|" + N(BLT_SCREEN) + "|41"}));
	CHECK_EQ(int(EFFECTSPRITETYPE_FAST_MOVE_STOP), int(pFly->GetEffectSpriteType()));
	// Moving under a sylph on foot, the hovering one flies (blitted as the
	// hovering one is).
	MCreatureWear mover;
	Dress(mover, Ousters);
	mover.AddEffectStatus(EFFECTSTATUS_SUMMON_SYLPH_RED, 50);
	mover.SetMoveDevice(MCreature::MOVE_DEVICE_WALK);
	mover.SetAction(ACTION_MOVE);
	CHECK_EQ(ACTION_OUSTERS_FAST_MOVE_MOVE, int(mover.GetAction()));
	MAttachEffect* pHover = mover.CreateAttachEffect(EFFECTSPRITETYPE_FAST_MOVE_STOP, 50, 50);
	calls.clear();
	mover.UpdateAttachEffect();
	CHECK(Same({"max frame|" + N(BLT_SCREEN) + "|42"}));
	CHECK_EQ(int(EFFECTSPRITETYPE_FAST_MOVE_FLY), int(pHover->GetEffectSpriteType()));
}
