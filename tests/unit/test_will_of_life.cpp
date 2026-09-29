//----------------------------------------------------------------------
// test_will_of_life.cpp
//----------------------------------------------------------------------
//
// Will of Life, a vampire skill whose numbers depend on the vampire's
// level, through the two functions the client asks (MSkillManager.h):
// GetWillOfLifeHP, which the skill description shows as the cast's HP
// cost and MCreature::SetRegen adds to each regeneration while the
// effect lasts, and GetWillOfLifeDelay, the reuse time the skill bar's
// cooldown and the two skill packet handlers use.
//
// The server's WillOfLife handler charges the formula's Damage, gives
// the effect the same Damage as its bonus, and sets the reuse time from
// its Delay (decore::skillformula::WillOfLife). The inputs are rows of
// third_party/decore/domain/vectors/skill_output.tsv, which decore_tests
// asserts on every toolchain; each check names its row.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "MSkillManager.h"

// Damage is 5 + level / 7: one more HP every seventh level.
TEST(WillOfLife, CostsTheFormulasDamage)
{
	CHECK_EQ(5, GetWillOfLifeHP(0));		// wol-level-0
	CHECK_EQ(5, GetWillOfLifeHP(1));		// wol-level-1
	CHECK_EQ(5, GetWillOfLifeHP(6));		// wol-level-6
	CHECK_EQ(6, GetWillOfLifeHP(7));		// wol-level-7
	CHECK_EQ(6, GetWillOfLifeHP(13));		// wol-level-13
	CHECK_EQ(7, GetWillOfLifeHP(14));		// wol-level-14
	CHECK_EQ(7, GetWillOfLifeHP(19));		// wol-grid-level-19
	CHECK_EQ(19, GetWillOfLifeHP(100));		// wol-level-100
	CHECK_EQ(26, GetWillOfLifeHP(150));		// wol-level-150-vampire-max
}

// The client's own reuse time: (3 + level / 10) * 2 seconds.
TEST(WillOfLife, WaitsTheClientsReuseTime)
{
	CHECK_EQ(6000, GetWillOfLifeDelay(0));
	CHECK_EQ(6000, GetWillOfLifeDelay(9));
	CHECK_EQ(8000, GetWillOfLifeDelay(10));
	CHECK_EQ(8000, GetWillOfLifeDelay(19));
	CHECK_EQ(26000, GetWillOfLifeDelay(100));
	CHECK_EQ(36000, GetWillOfLifeDelay(150));
}
