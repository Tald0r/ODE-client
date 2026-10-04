//----------------------------------------------------------------------
// test_skill_range.cpp
//----------------------------------------------------------------------
//
// GetSkillRangeAtLevel (MSkillManager.h): how far away, in tiles, the
// player can use a skill from, by its race and the skill's proficiency
// level, from the skill table's minimum and maximum range.
// MPlayer::GetActionInfoRange asks it for every skill it does not
// range some other way (Head Shot, the skills that take the weapon's
// range, and the three with a formula of their own), so how close the
// character walks before using a skill follows it.
//
// A slayer's is the server's rule, decore::skillRange, which the
// server's four sliding and walking skills check their target's distance
// against: the range grows from the minimum at level 0 to the maximum at
// level 100, in double arithmetic with one truncation, each input and
// the result read modulo 256. The inputs are rows of
// third_party/decore/domain/vectors/skill_range.tsv, which decore_tests
// asserts on every toolchain; each check names its row.
//
// A vampire's and an ousters' keep the client's integer step, to level
// 100 and to level 30. The server ranges no vampire skill by its level
// (Rapid Gliding's, Bloody Zenith's and Set Afire's ranges are its
// stats'). It ranges five ousters skills by the level of the skill
// slot, each by the Range of its own formula in
// third_party/decore/domain/SkillOutputFormulas.cpp: Blunting and
// Tendril 1 + level / 10, Prominence 2 + level / 10, Teleport and
// Charging Attack 3 + level / 10, Teleport's at most 6. Each has a span
// of 3 in the server's seed and in the SkillInfo.inf the upstream tree
// carried, and a span of 3 stepped to level 30 is the minimum plus
// level / 10, so the client's step gives the server's range; the checks
// below call those formulas. A span other than 3 for one of them would
// not. Soul Rebirth is a sixth, ranged by its slot level on the server
// (2 + level / 10, plus the passive skill's level / 10). Its separate
// GetSoulRebirthRange helper is covered by test_soul_rebirth_range.cpp.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "MSkillManager.h"

#include "domain/SkillOutputFormulas.h"

// The server's four sliding and walking skills at their SkillBalance
// ranges (Flash Sliding and Blaze Walk 2 to 7, Shadow Walk and Blitz
// Sliding 2 to 6), where the client's integer step gave the same.
TEST(SkillRange, ASlayerSkillGrowsToItsMaximumAtLevel100)
{
	CHECK_EQ(2, GetSkillRangeAtLevel(RACE_SLAYER, 2, 7, 0));		// seed-flash-sliding-level-0
	CHECK_EQ(2, GetSkillRangeAtLevel(RACE_SLAYER, 2, 7, 1));		// seed-flash-sliding-level-1
	CHECK_EQ(3, GetSkillRangeAtLevel(RACE_SLAYER, 2, 7, 20));		// seed-flash-sliding-level-20
	CHECK_EQ(4, GetSkillRangeAtLevel(RACE_SLAYER, 2, 7, 50));		// seed-flash-sliding-level-50
	CHECK_EQ(6, GetSkillRangeAtLevel(RACE_SLAYER, 2, 7, 99));		// seed-flash-sliding-level-99
	CHECK_EQ(7, GetSkillRangeAtLevel(RACE_SLAYER, 2, 7, 100));		// seed-flash-sliding-level-100
	CHECK_EQ(2, GetSkillRangeAtLevel(RACE_SLAYER, 2, 6, 20));		// seed-shadow-walk-level-20
	CHECK_EQ(3, GetSkillRangeAtLevel(RACE_SLAYER, 2, 6, 25));		// seed-shadow-walk-level-25
	CHECK_EQ(5, GetSkillRangeAtLevel(RACE_SLAYER, 2, 6, 99));		// seed-shadow-walk-level-99
	CHECK_EQ(6, GetSkillRangeAtLevel(RACE_SLAYER, 2, 6, 100));		// seed-shadow-walk-level-100
	CHECK_EQ(7, GetSkillRangeAtLevel(RACE_SLAYER, 7, 7, 63));		// zero-span
	CHECK_EQ(1, GetSkillRangeAtLevel(RACE_SLAYER, 5, 0, 80));		// no-fma-5-0-level-80
}

// Where the server's rule differs from the client's integer step, one
// lower each time: 0.29 and 0.58 round low in binary, so a span of 50
// or more truncates one below the integer answer at levels 29 and 58;
// and with the maximum below the minimum the server truncates the whole
// range, not the step. Neither the server's seed nor the client's
// in-code overrides give a slayer skill such a range.
TEST(SkillRange, ASlayerSkillTakesTheServersRule)
{
	CHECK_EQ(28, GetSkillRangeAtLevel(RACE_SLAYER, 0, 100, 29));	// level-29-span-100
	CHECK_EQ(28, GetSkillRangeAtLevel(RACE_SLAYER, 0, 50, 58));		// level-58-span-50
	CHECK_EQ(127, GetSkillRangeAtLevel(RACE_SLAYER, 12, 212, 58));	// level-58-span-200-from-12
	CHECK_EQ(4, GetSkillRangeAtLevel(RACE_SLAYER, 6, 2, 30));		// max-below-min-6-2-level-30
	CHECK_EQ(0, GetSkillRangeAtLevel(RACE_SLAYER, 1, 0, 50));		// max-below-min-1-0-level-50
}

// Each input and the result wrap at 256, as the server's 8-bit types
// do; a slayer's proficiency level stops at 100.
TEST(SkillRange, ASlayerSkillWrapsAt256)
{
	CHECK_EQ(7, GetSkillRangeAtLevel(RACE_SLAYER, 2, 7, 101));		// wrap-level-101
	CHECK_EQ(2, GetSkillRangeAtLevel(RACE_SLAYER, 2, 7, 256));		// wrap-level-256-is-0
	CHECK_EQ(3, GetSkillRangeAtLevel(RACE_SLAYER, 256, 7, 50));		// wrap-min-256-is-0
	CHECK_EQ(84, GetSkillRangeAtLevel(RACE_SLAYER, 200, 255, 255));	// wrap-result-above-255
}

// The server ranges no vampire skill by a proficiency level; the client
// scales a vampire's to 100, as a slayer's. Raising Dead and Summon
// Servant have minimum 1 and maximum 0 in the server's seed.
TEST(SkillRange, AVampireSkillTakesTheIntegerStepTo100)
{
	CHECK_EQ(1, GetSkillRangeAtLevel(RACE_VAMPIRE, 1, 0, 0));
	CHECK_EQ(1, GetSkillRangeAtLevel(RACE_VAMPIRE, 1, 0, 50));
	CHECK_EQ(1, GetSkillRangeAtLevel(RACE_VAMPIRE, 1, 0, 99));
	CHECK_EQ(0, GetSkillRangeAtLevel(RACE_VAMPIRE, 1, 0, 100));
	CHECK_EQ(4, GetSkillRangeAtLevel(RACE_VAMPIRE, 2, 7, 50));
	CHECK_EQ(29, GetSkillRangeAtLevel(RACE_VAMPIRE, 0, 100, 29));
	CHECK_EQ(5, GetSkillRangeAtLevel(RACE_VAMPIRE, 6, 2, 30));
}

// An ousters raises a skill to level 30.
TEST(SkillRange, AnOustersSkillGrowsToItsMaximumAtLevel30)
{
	CHECK_EQ(2, GetSkillRangeAtLevel(RACE_OUSTERS, 2, 7, 0));
	CHECK_EQ(3, GetSkillRangeAtLevel(RACE_OUSTERS, 2, 7, 10));
	CHECK_EQ(4, GetSkillRangeAtLevel(RACE_OUSTERS, 2, 7, 15));
	CHECK_EQ(7, GetSkillRangeAtLevel(RACE_OUSTERS, 2, 7, 30));
}

namespace {

// The Range of the server's formula for an ousters skill at `level`.
// The input is filled as the server's SkillInput(Ousters*, slot)
// (skill/SkillHandler.cpp) fills it, SkillLevel being the slot's level;
// the five formulas' Range reads nothing else, so the stats, the
// weapon and the advancement class level are left 0 and GunClass::Other.
int OustersFormulaRange(void (*formula)(const decore::skillformula::SkillInput&,
										 decore::skillformula::SkillOutput&),
						int level)
{
	decore::skillformula::SkillInput in = {};
	in.SkillLevel = level;
	in.DomainLevel = 0;
	in.DomainGrade = -1;
	in.TargetType = decore::skillformula::SkillInput::TARGET_MAX;
	in.Gun = decore::skillformula::GunClass::Other;
	in.PartySize = 1;

	decore::skillformula::SkillOutput out;
	formula(in, out);
	return out.Range;
}

} // namespace

// The five ousters skills the server ranges by their level, at their
// seed ranges, at every level 0 to 30.
TEST(SkillRange, AnOustersSkillStepsAsTheServersFormula)
{
	for (int level = 0; level <= 30; level++)
	{
		CHECK_EQ(OustersFormulaRange(decore::skillformula::Blunting, level),
				 GetSkillRangeAtLevel(RACE_OUSTERS, 1, 4, level));
		CHECK_EQ(OustersFormulaRange(decore::skillformula::Tendril, level),
				 GetSkillRangeAtLevel(RACE_OUSTERS, 1, 4, level));
		CHECK_EQ(OustersFormulaRange(decore::skillformula::Prominence, level),
				 GetSkillRangeAtLevel(RACE_OUSTERS, 2, 5, level));
		CHECK_EQ(OustersFormulaRange(decore::skillformula::Teleport, level),
				 GetSkillRangeAtLevel(RACE_OUSTERS, 3, 6, level));
		CHECK_EQ(OustersFormulaRange(decore::skillformula::ChargingAttack, level),
				 GetSkillRangeAtLevel(RACE_OUSTERS, 3, 6, level));
	}
}
