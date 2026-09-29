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

// The reuse time is the formula's Delay, in tenths of a second: the
// server's handler sets the skill slot's run time to it
// (RaceSkillSlot::setRunTime takes tenths), so the client waits
// Delay * 100 ms = 6000 + 200 * level. Before, it waited
// (3 + level / 10) * 2 seconds, the same only at a multiple of ten
// levels and up to 1.8 s short otherwise, so the skill came back before
// the server's run time and the cast failed (GCSkillFailed1).
TEST(WillOfLife, WaitsTheServersReuseTime)
{
	CHECK_EQ(6000, GetWillOfLifeDelay(0));		// wol-level-0: Delay 60
	CHECK_EQ(6200, GetWillOfLifeDelay(1));		// wol-level-1: Delay 62
	CHECK_EQ(7800, GetWillOfLifeDelay(9));		// wol-grid-level-9: Delay 78
	CHECK_EQ(8000, GetWillOfLifeDelay(10));		// wol-grid-level-10: Delay 80
	CHECK_EQ(9800, GetWillOfLifeDelay(19));		// wol-grid-level-19: Delay 98
	CHECK_EQ(10000, GetWillOfLifeDelay(20));	// wol-grid-level-20: Delay 100
	CHECK_EQ(11600, GetWillOfLifeDelay(28));	// wol-grid-level-28: Delay 116
	CHECK_EQ(26000, GetWillOfLifeDelay(100));	// wol-level-100: Delay 260
	CHECK_EQ(36000, GetWillOfLifeDelay(150));	// wol-level-150-vampire-max: Delay 360
}
