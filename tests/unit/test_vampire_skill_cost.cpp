//----------------------------------------------------------------------
// test_vampire_skill_cost.cpp
//----------------------------------------------------------------------
//
// What a vampire pays, in HP, to use a skill: MSkillInfoTable::
// GetVampireConsumeMP, which the skill bar's availability check
// (MSkillAvailable.cpp) and both skill descriptions
// (VS_UI_Description.cpp) read. The table row holds the skill's consume
// MP and level as the server's skill table has them (SkillInfo.inf); the
// vampire's own level is read only for Will of Life.
//
// The server charges decreaseConsumeMP (SkillUtil.cpp), which is
// decore::vampireSkillConsumeMP of those two and the vampire's current
// INT: the more the INT, less 20, exceeds the skill's level, the larger
// the discount. The checks below are rows of
// third_party/decore/domain/vectors/stats.tsv, which decore_tests
// asserts on every toolchain. Fifteen skills' server handlers do not
// charge it, and those keep the table cost. Will of Life's handler does
// not charge it either, but charges its own formula's Damage, from
// skill_output.tsv.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "type_table_access.h"

#include "MSkillManager.h"
#include "SkillDef.h"

namespace {

// A vampire level for the skills whose cost does not read it.
const int	kLevel = 50;

// A table whose row for `id` costs 200 at skill level 11, the inputs of
// stats.tsv's consume-level-11-* rows.
struct CostTable
{
	MSkillInfoTable	table;

	explicit CostTable(ACTIONINFO id)
	{
		testfw::MutableRow(table, id).SetMP(200);
		testfw::MutableRow(table, id).SetLearnLevel(11);
	}
};

} // namespace

// The row's level is its learn level (11), not the node's display
// level (1, which would give the 95% discount at INT 32).
TEST(VampireSkillCost, FallsAsTheIntPassesTheSkillLevel)
{
	CostTable cost(MAGIC_INVISIBILITY);
	const ACTIONINFO id = MAGIC_INVISIBILITY;

	CHECK_EQ(200, cost.table.GetVampireConsumeMP(id, 20, kLevel));
	CHECK_EQ(200, cost.table.GetVampireConsumeMP(id, 31, kLevel));		// consume-level-11-int-at-level
	CHECK_EQ(180, cost.table.GetVampireConsumeMP(id, 32, kLevel));		// consume-level-11-int-past-level
	CHECK_EQ(150, cost.table.GetVampireConsumeMP(id, 37, kLevel));		// consume-level-11-int-past-1.5x
	CHECK_EQ(100, cost.table.GetVampireConsumeMP(id, 43, kLevel));		// consume-level-11-int-past-2x
	CHECK_EQ(80, cost.table.GetVampireConsumeMP(id, 54, kLevel));		// consume-level-11-int-past-3x
	CHECK_EQ(50, cost.table.GetVampireConsumeMP(id, 65, kLevel));		// consume-level-11-int-past-4x
	CHECK_EQ(30, cost.table.GetVampireConsumeMP(id, 76, kLevel));		// consume-level-11-int-past-5x
	CHECK_EQ(20, cost.table.GetVampireConsumeMP(id, 87, kLevel));		// consume-level-11-int-past-6x
	CHECK_EQ(10, cost.table.GetVampireConsumeMP(id, 98, kLevel));		// consume-level-11-int-past-7x
	CHECK_EQ(10, cost.table.GetVampireConsumeMP(id, 520, kLevel));		// consume-level-11-int-far-past-7x
}

// consume-level-0-int-21-is-95pct: a level 0 skill is discounted by 95%
// from INT 21.
TEST(VampireSkillCost, ALevelZeroSkill)
{
	MSkillInfoTable table;
	testfw::MutableRow(table, MAGIC_HIDE).SetMP(100);
	testfw::MutableRow(table, MAGIC_HIDE).SetLearnLevel(0);

	CHECK_EQ(100, table.GetVampireConsumeMP(MAGIC_HIDE, 20, kLevel));	// consume-level-0-int-20
	CHECK_EQ(5, table.GetVampireConsumeMP(MAGIC_HIDE, 21, kLevel));
}

// Every vampire skill whose server handler (an execute(Vampire*) under
// src/server/gameserver/skill) never calls decreaseConsumeMP, except Will
// of Life (below). Extreme, Mephisto, PoisonMesh, StoneSkin, and
// ViolentPhantom and Deadly Claw (through SimpleTileMeleeSkill) charge
// the table cost undiscounted. Howl charges 10 and Transfusion 12% of the
// current HP. Blood Drain, Eat Corpse, Bloody Warp (which always fails),
// Open Casket, Unburrow, Uninvisibility and Untransform charge nothing.
// The client charges all fifteen the table cost: for the first six that
// is the server's charge, for the other nine what the client charged
// before the discount.
// The skill bar gates Bloody Warp by name (HasMagicBloodyWarp) and every
// learned vampire skill on this cost.
TEST(VampireSkillCost, SomeSkillsPayTheTableCost)
{
	const ACTIONINFO skills[] = {
		// charged the table cost undiscounted
		SKILL_EXTREME, SKILL_MEPHISTO, SKILL_POISON_MESH, SKILL_STONE_SKIN,
		SKILL_VIOLENT_PHANTOM, SKILL_VAMPIRE_INNATE_DEADLY_CLAW,
		// charged a cost of their own
		MAGIC_HOWL, SKILL_TRANSFUSION,
		// charged nothing
		SKILL_BLOOD_DRAIN, MAGIC_EAT_CORPSE, MAGIC_BLOODY_WARP,
		MAGIC_OPEN_CASKET, MAGIC_UN_BURROW, MAGIC_UN_INVISIBILITY,
		MAGIC_UN_TRANSFORM,
	};
	static_assert(sizeof(skills) / sizeof(skills[0]) == 15, "the fifteen skills");
	for (ACTIONINFO id : skills)
	{
		CostTable cost(id);
		CHECK_EQ(200, cost.table.GetVampireConsumeMP(id, 98, kLevel));
	}
}

// Will of Life costs what its server handler charges: the Damage of its
// formula (decore::skillformula::WillOfLife, 5 + level / 7) at the
// vampire's level, whatever the table row or the INT. Before, the client
// charged it the table cost (Mana 50 in the server's seed), so the skill
// bar greyed it out below 50 HP, where the server lets a vampire cast it
// with any HP above the formula's cost (26 at level 150). The skill
// description showed the formula, the skill tree's description the table
// cost.
TEST(VampireSkillCost, WillOfLifeCostsItsFormula)
{
	static_assert(SKILL_WILL_OF_LIFE == 207, "the server's skill type (src/Core/types/SkillTypes.h there)");
	CostTable cost(SKILL_WILL_OF_LIFE);
	const ACTIONINFO id = SKILL_WILL_OF_LIFE;

	CHECK_EQ(5, cost.table.GetVampireConsumeMP(id, 98, 0));		// wol-level-0
	CHECK_EQ(5, cost.table.GetVampireConsumeMP(id, 98, 6));		// wol-level-6
	CHECK_EQ(6, cost.table.GetVampireConsumeMP(id, 98, 7));		// wol-level-7
	CHECK_EQ(7, cost.table.GetVampireConsumeMP(id, 98, 14));		// wol-level-14
	CHECK_EQ(19, cost.table.GetVampireConsumeMP(id, 98, 100));	// wol-level-100
	CHECK_EQ(26, cost.table.GetVampireConsumeMP(id, 98, 150));	// wol-level-150-vampire-max

	// wol-grid-level-50, at an INT below any discount and one past 7x.
	CHECK_EQ(12, cost.table.GetVampireConsumeMP(id, 20, 50));
	CHECK_EQ(12, cost.table.GetVampireConsumeMP(id, 520, 50));
}

// The inverse: skills whose server handlers do charge decreaseConsumeMP
// (GroundAttack.cpp, BloodySnake.cpp, BiteOfDeath.cpp, RapidGliding.cpp,
// the three TransformTo*.cpp, Hide.cpp and Invisibility.cpp; each target
// overload passes on to the tile one that charges it). The skill bar
// adds the first four by name or by form and the others from the
// learned skills, and reads this cost for all of them. One added to the
// table-cost list by mistake would cost 200 here where the server
// charges 10.
TEST(VampireSkillCost, TheOtherGatedSkillsAreDiscounted)
{
	static_assert(MAGIC_HIDE == 98 && MAGIC_INVISIBILITY == 100
		&& MAGIC_TRANSFORM_TO_WOLF == 101 && MAGIC_TRANSFORM_TO_BAT == 102
		&& MAGIC_GROUND_ATTACK == 179 && MAGIC_BLOODY_SNAKE == 184
		&& MAGIC_RAPID_GLIDING == 203 && SKILL_TRANSFORM_TO_WERWOLF == 273
		&& SKILL_BITE_OF_DEATH == 278,
		"the server's skill types (src/Core/types/SkillTypes.h there)");
	const ACTIONINFO skills[] = {
		MAGIC_GROUND_ATTACK, MAGIC_BLOODY_SNAKE, SKILL_BITE_OF_DEATH,
		MAGIC_RAPID_GLIDING, MAGIC_TRANSFORM_TO_WOLF, MAGIC_TRANSFORM_TO_BAT,
		SKILL_TRANSFORM_TO_WERWOLF, MAGIC_HIDE, MAGIC_INVISIBILITY,
	};
	for (ACTIONINFO id : skills)
	{
		CostTable cost(id);
		CHECK_EQ(200, cost.table.GetVampireConsumeMP(id, 31, kLevel));	// consume-level-11-int-at-level
		CHECK_EQ(10, cost.table.GetVampireConsumeMP(id, 98, kLevel));	// consume-level-11-int-past-7x
		// The vampire's level does not change their cost.
		CHECK_EQ(10, cost.table.GetVampireConsumeMP(id, 98, 1));
		CHECK_EQ(10, cost.table.GetVampireConsumeMP(id, 98, 150));
	}
}

// A skill the table does not hold costs nothing.
TEST(VampireSkillCost, AnUnknownSkillCostsNothing)
{
	MSkillInfoTable table;
	CHECK_EQ(0, table.GetVampireConsumeMP(-1, 98, kLevel));
	CHECK_EQ(0, table.GetVampireConsumeMP(table.GetSize(), 98, kLevel));
}
