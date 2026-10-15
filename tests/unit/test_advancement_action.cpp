#include "test_framework.h"
#include "AdvancementAction.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "MCreatureWear.h"
#include "MCreatureTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "EffectSpriteTypeDef.h"
#include "MCreatureSpriteTable.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"

#include <iterator>

// An advanced (second class) creature's actions (task 4.239, gamemodel's
// since then): each race's plain action converted to its advanced one.
// The expected rows repeat the tables, so a changed row is caught.
namespace {

const int OustersRows[] = {
	ACTION_STAND,
	ACTION_MOVE,
	ACTION_ADVANCEMENT_OUSTERS_ATTACK_NORMAL,
	ACTION_ADVANCEMENT_OUSTERS_MAGIC,
	ACTION_ADVANCEMENT_OUSTERS_DAMAGED,
	ACTION_ADVANCEMENT_OUSTERS_DRAINED,
	ACTION_ADVANCEMENT_OUSTERS_DIE,
	ACTION_STAND,
	ACTION_MOVE,
	ACTION_ADVANCEMENT_OUSTERS_SKILL_NORMAL,
	ACTION_ADVANCEMENT_OUSTERS_MAGIC_ATTACK,
	ACTION_ADVANCEMENT_OUSTERS_ABSORB_SOUL,
	ACTION_ADVANCEMENT_OUSTERS_FAST_MOVE_STOP,
	ACTION_ADVANCEMENT_OUSTERS_FAST_MOVE,
	ACTION_ADVANCEMENT_OUSTERS_ATTACK_SLOW,
	ACTION_ADVANCEMENT_OUSTERS_ATTACK_FAST,
	ACTION_ADVANCEMENT_OUSTERS_SKILL_SLOW,
	ACTION_ADVANCEMENT_OUSTERS_SKILL_FAST,
	-1,
};

const int SlayerRows[] = {
	ACTION_STAND,
	ACTION_MOVE,
	-1,
	ACTION_ADVANCEMENT_SLAYER_MAGIC,
	ACTION_DAMAGED,
	ACTION_ADVANCEMENT_SLAYER_DRAINED,
	ACTION_ADVANCEMENT_SLAYER_DIE,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_SR_GUN_NORMAL,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_AR_GUN_NORMAL,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_SWORD_NORMAL,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_BLADE_NORMAL,
	-1,
	ACTION_ADVANCEMENT_SLAYER_BIKE_MOVE,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_SR_GUN_NORMAL,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_AR_GUN_NORMAL,
	ACTION_ADVANCEMENT_SLAYER_SKILL_SWORD_NORMAL,
	ACTION_ADVANCEMENT_SLAYER_SKILL_BLADE_NORMAL,
	-1,
	ACTION_ADVANCEMENT_SLAYER_BIKE_STOP,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_SR_GUN_SLOW,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_SR_GUN_FAST,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_AR_GUN_SLOW,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_AR_GUN_FAST,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_SR_GUN_SLOW,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_SR_GUN_FAST,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_AR_GUN_SLOW,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_AR_GUN_FAST,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_SWORD_SLOW,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_SWORD_FAST,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_BLADE_SLOW,
	ACTION_ADVANCEMENT_SLAYER_ATTACK_BLADE_FAST,
	ACTION_ADVANCEMENT_SLAYER_SKILL_SWORD_SLOW,
	ACTION_ADVANCEMENT_SLAYER_SKILL_SWORD_FAST,
	ACTION_ADVANCEMENT_SLAYER_SKILL_BLADE_SLOW,
	ACTION_ADVANCEMENT_SLAYER_SKILL_BLADE_FAST,
};

const int VampireRows[] = {
	ACTION_ADVANCEMENT_STOP,
	ACTION_ADVANCEMENT_MOVE,
	ACTION_ADVANCEMENT_ATTACK_NORMAL,
	ACTION_ADVANCEMENT_MAGIC,
	ACTION_ADVANCEMENT_DAMAGED,
	ACTION_ADVANCEMENT_DRAINED,
	ACTION_ADVANCEMENT_DIE,
	ACTION_ADVANCEMENT_DRAIN,
	ACTION_ADVANCEMENT_ATTACK_SLOW,
	ACTION_ADVANCEMENT_ATTACK_FAST,
	ACTION_ADVANCEMENT_SKILL_SLOW,
	ACTION_ADVANCEMENT_SKILL_NORMAL,
	ACTION_ADVANCEMENT_SKILL_FAST,
	ACTION_ADVANCEMENT_MAGIC_ATTACK,
	ACTION_ADVANCEMENT_SPECIAL,
	ACTION_ADVANCEMENT_CREATE_WEAPON,
	ACTION_ADVANCEMENT_DESTROY_WEAPON,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
	-1,
};

} // namespace

TEST(AdvancementAction, AdvancedActionsPassThrough)
{
	for (int action : {ADVANCEMENT_ACTION_START, ADVANCEMENT_ACTION_START + 7, 250})
	{
		CHECK_EQ(action, ConvAdvancementSlayerAction(action, ITEM_CLASS_SWORD));
		CHECK_EQ(action, ConvAdvancementOustersActionFromOusterAction(action, true));
		CHECK_EQ(action, GetAdvancementVampireActionFromVampireAction(action, nullptr));
	}
}

TEST(AdvancementAction, ASlayersRowsByAction)
{
	// Standing, moving and being hit go by the right hand; the rest by row.
	for (int action = 0; action < int(std::size(SlayerRows)); ++action)
	{
		if (action == ACTION_STAND || action == ACTION_MOVE || action == ACTION_DAMAGED) continue;
		CHECK_EQ(SlayerRows[action], ConvAdvancementSlayerAction(action, ITEM_CLASS_SWORD));
	}
	CHECK_EQ(ACTION_MAX_SLAYER, int(std::size(SlayerRows)));
}

TEST(AdvancementAction, ASlayerStandsMovesAndIsHitByTheRightHand)
{
	struct Hand { ITEM_CLASS itemClass; int stand, move, damaged; };
	const Hand hands[] = {
		{ITEM_CLASS_SWORD, ACTION_ADVANCEMENT_SLAYER_STOP_SWORD, ACTION_ADVANCEMENT_SLAYER_MOVE_SWORD, ACTION_ADVANCEMENT_SLAYER_DAMAGED_SWORD},
		{ITEM_CLASS_BLADE, ACTION_ADVANCEMENT_SLAYER_STOP_BLADE, ACTION_ADVANCEMENT_SLAYER_MOVE_BLADE, ACTION_ADVANCEMENT_SLAYER_DAMAGED_BLADE},
		{ITEM_CLASS_SG, ACTION_ADVANCEMENT_SLAYER_STOP_GUN, ACTION_ADVANCEMENT_SLAYER_MOVE_GUN, ACTION_ADVANCEMENT_SLAYER_DAMAGED_GUN},
		{ITEM_CLASS_SMG, ACTION_ADVANCEMENT_SLAYER_STOP_GUN, ACTION_ADVANCEMENT_SLAYER_MOVE_GUN, ACTION_ADVANCEMENT_SLAYER_DAMAGED_GUN},
		{ITEM_CLASS_AR, ACTION_ADVANCEMENT_SLAYER_STOP_GUN, ACTION_ADVANCEMENT_SLAYER_MOVE_GUN, ACTION_ADVANCEMENT_SLAYER_DAMAGED_GUN},
		{ITEM_CLASS_SR, ACTION_ADVANCEMENT_SLAYER_STOP_GUN, ACTION_ADVANCEMENT_SLAYER_MOVE_GUN, ACTION_ADVANCEMENT_SLAYER_DAMAGED_GUN},
		{ITEM_CLASS_MACE, ACTION_ADVANCEMENT_SLAYER_STOP_MACE_AND_CROSS, ACTION_ADVANCEMENT_SLAYER_MOVE_MACE_AND_CROSS, ACTION_ADVANCEMENT_SLAYER_DAMAGED_CROSS_MACE},
		{ITEM_CLASS_CROSS, ACTION_ADVANCEMENT_SLAYER_STOP_MACE_AND_CROSS, ACTION_ADVANCEMENT_SLAYER_MOVE_MACE_AND_CROSS, ACTION_ADVANCEMENT_SLAYER_DAMAGED_CROSS_MACE},
		{ITEM_CLASS_NULL, ACTION_ADVANCEMENT_SLAYER_STOP_MACE_AND_CROSS, ACTION_ADVANCEMENT_SLAYER_MOVE_MACE_AND_CROSS, ACTION_ADVANCEMENT_SLAYER_DAMAGED_CROSS_MACE},
	};
	for (const Hand& hand : hands)
	{
		CHECK_EQ(hand.stand, ConvAdvancementSlayerAction(ACTION_STAND, hand.itemClass));
		CHECK_EQ(hand.move, ConvAdvancementSlayerAction(ACTION_MOVE, hand.itemClass));
		CHECK_EQ(hand.damaged, ConvAdvancementSlayerAction(ACTION_DAMAGED, hand.itemClass));
	}
	// Any other action ignores the hand.
	CHECK_EQ(ConvAdvancementSlayerAction(3, ITEM_CLASS_SWORD), ConvAdvancementSlayerAction(3, ITEM_CLASS_SR));
}

TEST(AdvancementAction, AnOustersRowsByActionAndTheChakram)
{
	for (int action = 0; action <= ACTION_OUSTERS_CHAKRAM_FAST; ++action)
	{
		if (action == ACTION_STAND || action == ACTION_MOVE) continue;
		CHECK_EQ(OustersRows[action], ConvAdvancementOustersActionFromOusterAction(action, true));
		CHECK_EQ(OustersRows[action], ConvAdvancementOustersActionFromOusterAction(action, false));
	}
	CHECK_EQ(ACTION_ADVANCEMENT_OUSTERS_CHAKRAM_STOP, ConvAdvancementOustersActionFromOusterAction(ACTION_STAND, true));
	CHECK_EQ(ACTION_ADVANCEMENT_OUSTERS_WRISTLET_STOP, ConvAdvancementOustersActionFromOusterAction(ACTION_STAND, false));
	CHECK_EQ(ACTION_ADVANCEMENT_OUSTERS_CHAKRAM_MOVE, ConvAdvancementOustersActionFromOusterAction(ACTION_MOVE, true));
	CHECK_EQ(ACTION_ADVANCEMENT_OUSTERS_WRISTLET_MOVE, ConvAdvancementOustersActionFromOusterAction(ACTION_MOVE, false));
	// Past the ousters' actions there is none.
	CHECK_EQ(-1, ConvAdvancementOustersActionFromOusterAction(ACTION_OUSTERS_CHAKRAM_FAST + 1, true));
	CHECK_EQ(-1, ConvAdvancementOustersActionFromOusterAction(ADVANCEMENT_ACTION_START - 1, false));
}

TEST(AdvancementAction, AVampiresRowsByAction)
{
	for (int action = 0; action < int(std::size(VampireRows)); ++action)
		CHECK_EQ(VampireRows[action], GetAdvancementVampireActionFromVampireAction(action, nullptr));
}

TEST(AdvancementAction, TheFrameCountsHoldEveryAdvancedAction)
{
	CHECK_EQ(size_t(ACTION_ADVANCEMENT_SLAYER_MAX - ADVANCEMENT_ACTION_START), sizeof(g_AdvanceSlayerActionMaxCount));
	CHECK_EQ(size_t(ACTION_ADVANCEMENT_MAX - ADVANCEMENT_ACTION_START), sizeof(g_AdvanceVampireActionMaxCount));
	CHECK_EQ(size_t(ACTION_ADVANCEMENT_OUSTERS_MAX - ADVANCEMENT_ACTION_START), sizeof(g_AdvanceOustersActionMaxCount));
}

//----------------------------------------------------------------------
// The parts and actions the creature drawing picks (task 4.248)
//----------------------------------------------------------------------
namespace {

// A wearer whose right hand holds what a test puts there.
struct Wearer : MCreatureWear
{
	void Hold(ITEM_CLASS itemClass)
	{
		m_Addon[ADDON_RIGHTHAND].bAddon = TRUE;
		m_Addon[ADDON_RIGHTHAND].ItemClass = itemClass;
	}
};

struct Wardrobe
{
	GameModelWorld world;
	CREATURE_TABLE creatureTypes;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	CREATURESPRITE_TABLE sprites;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	RankBonusTable rankBonuses;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	Wardrobe()
	{
		creatureTypes.Init(4);
		testfw::MutableRow(creatureTypes, 0).SetCreatureTribe(CREATURETRIBE_SLAYER, ACTION_MAX_SLAYER);
		g_pCreatureTable = &creatureTypes;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		sprites.Init(1);
		g_pCreatureSpriteTable = &sprites;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		g_pRankBonusTable = &rankBonuses;
	}
	~Wardrobe()
	{
		g_pRankBonusTable = oldRankBonuses;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pCreatureSpriteTable = oldSprites;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pCreatureTable = oldCreatureTypes;
	}
};

} // namespace

TEST(AdvancementAction, ANewVampireActionReplacesTwoActions)
{
	CHECK_EQ(16, GetNewVampireActionFromVampireAction(13));
	CHECK_EQ(7, GetNewVampireActionFromVampireAction(12));
	for (int action : {0, 1, 7, 11, 14, 16})
		CHECK_EQ(action, GetNewVampireActionFromVampireAction(action));
}

TEST(AdvancementAction, AnAdvancedSlayerDrawsAPartForEachGearClass)
{
	const struct { ITEM_CLASS itemClass; int part; } parts[] = {
		{ ITEM_CLASS_SWORD, AC_SWORD }, { ITEM_CLASS_BLADE, AC_BLADE }, { ITEM_CLASS_AR, AC_GUN_AR },
		{ ITEM_CLASS_SG, AC_GUN_SR }, { ITEM_CLASS_SMG, AC_GUN_SR }, { ITEM_CLASS_SR, AC_GUN_SR },
		{ ITEM_CLASS_MACE, AC_MACE }, { ITEM_CLASS_CROSS, AC_CROSS }, { ITEM_CLASS_SHIELD, AC_SHIELD },
		{ ITEM_CLASS_HELM, AC_HELMET }, { ITEM_CLASS_COAT, AC_BODY }, { ITEM_CLASS_TROUSER, AC_BODY },
		{ ITEM_CLASS_SHOULDER_ARMOR, AC_SHOULDER },
		{ ITEM_CLASS_RING, -1 }, { ITEM_CLASS_GLOVE, -1 }, { ITEM_CLASS_POTION, -1 },
	};
	for (const auto& p : parts)
		CHECK_EQ(p.part, GetAdvancementPartFromItemClass(p.itemClass, 0));
	// A motorcycle by its frame: the second bike's own, else the first.
	CHECK_EQ(int(AC_BIKE_2), GetAdvancementPartFromItemClass(ITEM_CLASS_MOTORCYCLE, AC_BIKE_2));
	CHECK_EQ(int(AC_BIKE_1), GetAdvancementPartFromItemClass(ITEM_CLASS_MOTORCYCLE, AC_BIKE_1));
	CHECK_EQ(int(AC_BIKE_1), GetAdvancementPartFromItemClass(ITEM_CLASS_MOTORCYCLE, 0));
}

TEST(AdvancementAction, AnAdvancedSlayersGunAttackGoesByItsSpeed)
{
	Wardrobe wardrobe;
	MCreature creature;
	for (int speed : {int(MCreature::WEAPON_SPEED_SLOW), int(MCreature::WEAPON_SPEED_NORMAL), int(MCreature::WEAPON_SPEED_FAST)})
	{
		creature.SetWeaponSpeed(MCreature::WEAPON_SPEED(speed));
		CHECK_EQ(int(ACTION_ADVANCEMENT_SLAYER_ATTACK_AR_GUN_SLOW) + speed, GetAdvancementSlayerAttackActionFromItemClass(ITEM_CLASS_AR, &creature));
		for (ITEM_CLASS itemClass : {ITEM_CLASS_SG, ITEM_CLASS_SMG, ITEM_CLASS_SR})
			CHECK_EQ(int(ACTION_ADVANCEMENT_SLAYER_ATTACK_SR_GUN_SLOW) + speed, GetAdvancementSlayerAttackActionFromItemClass(itemClass, &creature));
		CHECK_EQ(-1, GetAdvancementSlayerAttackActionFromItemClass(ITEM_CLASS_SWORD, &creature));
	}
}

TEST(AdvancementAction, ANewSlayerActionFollowsItsGunAndItsAttack)
{
	Wardrobe wardrobe;
	Wearer slayer;
	slayer.Hold(ITEM_CLASS_SWORD);
	CHECK_EQ(ACTION_STAND, ConvNewSlayerActionFromSlayerAction(ACTION_STAND, &slayer));
	CHECK_EQ(ACTION_MOVE, ConvNewSlayerActionFromSlayerAction(ACTION_MOVE, &slayer));
	for (ITEM_CLASS gun : {ITEM_CLASS_SG, ITEM_CLASS_SMG, ITEM_CLASS_AR, ITEM_CLASS_SR})
	{
		slayer.Hold(gun);
		CHECK_EQ(int(ACTION_NEW_SLAYER_STOP_GUN), ConvNewSlayerActionFromSlayerAction(ACTION_STAND, &slayer));
		CHECK_EQ(int(ACTION_NEW_SLAYER_MOVE_GUN), ConvNewSlayerActionFromSlayerAction(ACTION_MOVE, &slayer));
	}
	const struct { int from; int to; } rows[] = {
		{ ACTION_SLAYER_GUN_SG, ACTION_NEW_SLAYER_GUN_SG }, { ACTION_SLAYER_GUN_SMG, ACTION_NEW_SLAYER_GUN_SMG },
		{ ACTION_SLAYER_SWORD, ACTION_NEW_SLAYER_SWORD }, { ACTION_SLAYER_BLADE, ACTION_NEW_SLAYER_BLADE },
		{ ACTION_SLAYER_SWORD_2, ACTION_NEW_SLAYER_SWORD_2 }, { ACTION_SLAYER_BLADE_2, ACTION_NEW_SLAYER_BLADE_2 },
		{ ACTION_SLAYER_MOTOR_MOVE, ACTION_NEW_SLAYER_MOTOR_MOVE }, { ACTION_SLAYER_MOTOR_STAND, ACTION_NEW_SLAYER_MOTOR_STAND },
		{ ACTION_SLAYER_GUN_SR_SLOW, ACTION_NEW_SLAYER_GUN_SR_SLOW }, { ACTION_SLAYER_GUN_SR_FAST, ACTION_NEW_SLAYER_GUN_SR_FAST },
		{ ACTION_SLAYER_GUN_AR_SLOW, ACTION_NEW_SLAYER_GUN_AR_SLOW }, { ACTION_SLAYER_GUN_AR_FAST, ACTION_NEW_SLAYER_GUN_AR_FAST },
		{ ACTION_SLAYER_GUN_SG_SLOW, ACTION_NEW_SLAYER_GUN_SG_SLOW }, { ACTION_SLAYER_GUN_SG_FAST, ACTION_NEW_SLAYER_GUN_SG_FAST },
		{ ACTION_SLAYER_GUN_SMG_SLOW, ACTION_NEW_SLAYER_GUN_SMG_SLOW }, { ACTION_SLAYER_GUN_SMG_FAST, ACTION_NEW_SLAYER_GUN_SMG_FAST },
		{ ACTION_SLAYER_SWORD_SLOW, ACTION_NEW_SLAYER_SWORD_SLOW }, { ACTION_SLAYER_SWORD_FAST, ACTION_NEW_SLAYER_SWORD_FAST },
		{ ACTION_SLAYER_BLADE_SLOW, ACTION_NEW_SLAYER_BLADE_SLOW }, { ACTION_SLAYER_BLADE_FAST, ACTION_NEW_SLAYER_BLADE_FAST },
		{ ACTION_SLAYER_SWORD_2_SLOW, ACTION_NEW_SLAYER_SWORD_2_SLOW }, { ACTION_SLAYER_SWORD_2_FAST, ACTION_NEW_SLAYER_SWORD_2_FAST },
		{ ACTION_SLAYER_BLADE_2_SLOW, ACTION_NEW_SLAYER_BLADE_2_SLOW }, { ACTION_SLAYER_BLADE_2_FAST, ACTION_NEW_SLAYER_BLADE_2_FAST },
		{ 130, 37 },
	};
	for (const auto& r : rows)
		CHECK_EQ(r.to, ConvNewSlayerActionFromSlayerAction(r.from, &slayer));
	// The rest pass through.
	for (int action : {int(ACTION_ATTACK), int(ACTION_MAGIC), int(ACTION_DAMAGED), int(ACTION_DIE), 129, 131})
		CHECK_EQ(action, ConvNewSlayerActionFromSlayerAction(action, &slayer));
}

TEST(AdvancementAction, TheWearersAdvancedSlayerActionGoesByItsRightHand)
{
	Wardrobe wardrobe;
	Wearer slayer;
	for (ITEM_CLASS hand : {ITEM_CLASS_SWORD, ITEM_CLASS_BLADE, ITEM_CLASS_AR, ITEM_CLASS_SR, ITEM_CLASS_CROSS})
	{
		slayer.Hold(hand);
		for (int action = 0; action < ACTION_MAX_SLAYER; ++action)
			CHECK_EQ(ConvAdvancementSlayerAction(action, hand), ConvAdvancementSlayerActionFromSlayerAction(action, &slayer));
	}
	// A different hand gives a different answer somewhere.
	slayer.Hold(ITEM_CLASS_SWORD);
	const int sword = ConvAdvancementSlayerActionFromSlayerAction(ACTION_STAND, &slayer);
	slayer.Hold(ITEM_CLASS_AR);
	CHECK(sword != ConvAdvancementSlayerActionFromSlayerAction(ACTION_STAND, &slayer));
}
