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
// The range grows from the minimum at level 0 to the maximum at the top
// level, 100 for a slayer or a vampire and 30 for an ousters, in integer
// arithmetic.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "MSkillManager.h"

// The server's four sliding and walking skills at their SkillBalance
// ranges (Flash Sliding 2 to 7, Shadow Walk 2 to 6).
TEST(SkillRange, ASlayerSkillGrowsToItsMaximumAtLevel100)
{
	CHECK_EQ(2, GetSkillRangeAtLevel(RACE_SLAYER, 2, 7, 0));
	CHECK_EQ(2, GetSkillRangeAtLevel(RACE_SLAYER, 2, 7, 1));
	CHECK_EQ(3, GetSkillRangeAtLevel(RACE_SLAYER, 2, 7, 20));
	CHECK_EQ(4, GetSkillRangeAtLevel(RACE_SLAYER, 2, 7, 50));
	CHECK_EQ(6, GetSkillRangeAtLevel(RACE_SLAYER, 2, 7, 99));
	CHECK_EQ(7, GetSkillRangeAtLevel(RACE_SLAYER, 2, 7, 100));
	CHECK_EQ(2, GetSkillRangeAtLevel(RACE_SLAYER, 2, 6, 20));
	CHECK_EQ(5, GetSkillRangeAtLevel(RACE_SLAYER, 2, 6, 99));
	CHECK_EQ(7, GetSkillRangeAtLevel(RACE_SLAYER, 7, 7, 63));
}

// The client's integer step at a span of 50 or more and with the
// maximum below the minimum.
TEST(SkillRange, ASlayerSkillTakesTheIntegerStep)
{
	CHECK_EQ(29, GetSkillRangeAtLevel(RACE_SLAYER, 0, 100, 29));
	CHECK_EQ(29, GetSkillRangeAtLevel(RACE_SLAYER, 0, 50, 58));
	CHECK_EQ(5, GetSkillRangeAtLevel(RACE_SLAYER, 6, 2, 30));
	CHECK_EQ(1, GetSkillRangeAtLevel(RACE_SLAYER, 1, 0, 50));
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
}

// An ousters raises a skill to level 30.
TEST(SkillRange, AnOustersSkillGrowsToItsMaximumAtLevel30)
{
	CHECK_EQ(2, GetSkillRangeAtLevel(RACE_OUSTERS, 2, 7, 0));
	CHECK_EQ(3, GetSkillRangeAtLevel(RACE_OUSTERS, 2, 7, 10));
	CHECK_EQ(4, GetSkillRangeAtLevel(RACE_OUSTERS, 2, 7, 15));
	CHECK_EQ(7, GetSkillRangeAtLevel(RACE_OUSTERS, 2, 7, 30));
}
