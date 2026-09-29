//----------------------------------------------------------------------
// test_vampire_skill_cost.cpp
//----------------------------------------------------------------------
//
// What a vampire pays, in HP, to use a skill: MSkillInfoTable::
// GetVampireConsumeMP, which the skill bar's availability check
// (MSkillAvailable.cpp) and both skill descriptions
// (VS_UI_Description.cpp) read. The table row holds the skill's consume
// MP and level as the server's skill table has them (SkillInfo.inf).
//
// The server charges decreaseConsumeMP (SkillUtil.cpp), which is
// decore::vampireSkillConsumeMP of those two and the vampire's current
// INT: the more the INT, less 20, exceeds the skill's level, the larger
// the discount. The checks below are rows of
// third_party/decore/domain/vectors/stats.tsv, which decore_tests
// asserts on every toolchain. Eleven skills' handlers do not charge it,
// and those keep the table cost.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "type_table_access.h"

#include "MSkillManager.h"
#include "SkillDef.h"

namespace {

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

	CHECK_EQ(200, cost.table.GetVampireConsumeMP(id, 20));
	CHECK_EQ(200, cost.table.GetVampireConsumeMP(id, 31));		// consume-level-11-int-at-level
	CHECK_EQ(180, cost.table.GetVampireConsumeMP(id, 32));		// consume-level-11-int-past-level
	CHECK_EQ(150, cost.table.GetVampireConsumeMP(id, 37));		// consume-level-11-int-past-1.5x
	CHECK_EQ(100, cost.table.GetVampireConsumeMP(id, 43));		// consume-level-11-int-past-2x
	CHECK_EQ(80, cost.table.GetVampireConsumeMP(id, 54));		// consume-level-11-int-past-3x
	CHECK_EQ(50, cost.table.GetVampireConsumeMP(id, 65));		// consume-level-11-int-past-4x
	CHECK_EQ(30, cost.table.GetVampireConsumeMP(id, 76));		// consume-level-11-int-past-5x
	CHECK_EQ(20, cost.table.GetVampireConsumeMP(id, 87));		// consume-level-11-int-past-6x
	CHECK_EQ(10, cost.table.GetVampireConsumeMP(id, 98));		// consume-level-11-int-past-7x
	CHECK_EQ(10, cost.table.GetVampireConsumeMP(id, 520));		// consume-level-11-int-far-past-7x
}

// consume-level-0-int-21-is-95pct: a level 0 skill is discounted by 95%
// from INT 21.
TEST(VampireSkillCost, ALevelZeroSkill)
{
	MSkillInfoTable table;
	testfw::MutableRow(table, MAGIC_HIDE).SetMP(100);
	testfw::MutableRow(table, MAGIC_HIDE).SetLearnLevel(0);

	CHECK_EQ(100, table.GetVampireConsumeMP(MAGIC_HIDE, 20));	// consume-level-0-int-20
	CHECK_EQ(5, table.GetVampireConsumeMP(MAGIC_HIDE, 21));
}

// Skills whose handler does not charge decreaseConsumeMP. Extreme,
// Mephisto, PoisonMesh, StoneSkin, and ViolentPhantom and Deadly Claw
// (through SimpleTileMeleeSkill) charge the table cost undiscounted.
// Howl charges 10, Transfusion 12% of the current HP and Will of Life its
// own output; Blood Drain and Eat Corpse no mana at all. The client shows
// those five the table cost, as it did.
TEST(VampireSkillCost, SomeSkillsPayTheTableCost)
{
	const ACTIONINFO skills[] = {
		SKILL_EXTREME, SKILL_MEPHISTO, SKILL_POISON_MESH, SKILL_STONE_SKIN,
		SKILL_VIOLENT_PHANTOM, SKILL_VAMPIRE_INNATE_DEADLY_CLAW,
		MAGIC_HOWL, SKILL_TRANSFUSION, SKILL_WILL_OF_LIFE, SKILL_BLOOD_DRAIN,
		MAGIC_EAT_CORPSE,
	};
	for (ACTIONINFO id : skills)
	{
		CostTable cost(id);
		CHECK_EQ(200, cost.table.GetVampireConsumeMP(id, 98));
	}
}

// A skill the table does not hold costs nothing.
TEST(VampireSkillCost, AnUnknownSkillCostsNothing)
{
	MSkillInfoTable table;
	CHECK_EQ(0, table.GetVampireConsumeMP(-1, 98));
	CHECK_EQ(0, table.GetVampireConsumeMP(table.GetSize(), 98));
}
