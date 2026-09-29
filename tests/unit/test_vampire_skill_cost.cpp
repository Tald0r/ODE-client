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

TEST(VampireSkillCost, IsTheTableCost)
{
	CostTable cost(MAGIC_INVISIBILITY);

	CHECK_EQ(200, cost.table.GetVampireConsumeMP(MAGIC_INVISIBILITY, 20));
	CHECK_EQ(200, cost.table.GetVampireConsumeMP(MAGIC_INVISIBILITY, 31));
	CHECK_EQ(200, cost.table.GetVampireConsumeMP(MAGIC_INVISIBILITY, 32));
	CHECK_EQ(200, cost.table.GetVampireConsumeMP(MAGIC_INVISIBILITY, 43));
	CHECK_EQ(200, cost.table.GetVampireConsumeMP(MAGIC_INVISIBILITY, 98));
}

// Skills whose cost the server does not discount by INT.
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
