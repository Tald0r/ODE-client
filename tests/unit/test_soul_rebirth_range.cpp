#include "test_framework.h"
#include "MSkillManager.h"
#include "domain/SkillOutputFormulas.h"

TEST(SoulRebirthRange, ActiveSkillAddsOneTileAtEachTenLevelBoundary)
{
	struct Boundary { int level, range; };
	for (const auto point : {Boundary{0, 2}, {1, 2}, {9, 2}, {10, 3},
		{19, 3}, {20, 4}, {29, 4}, {30, 5}})
		CHECK_EQ(point.range, GetSoulRebirthRange(point.level, 0));
}

TEST(SoulRebirthRange, MasteryUsesItsOwnLevelBoundary)
{
	CHECK_EQ(2, GetSoulRebirthRange(9, 9));
	CHECK_EQ(3, GetSoulRebirthRange(9, 10));
	CHECK_EQ(3, GetSoulRebirthRange(10, 9));
	CHECK_EQ(4, GetSoulRebirthRange(10, 10));
	CHECK_EQ(4, GetSoulRebirthRange(19, 19));
	CHECK_EQ(5, GetSoulRebirthRange(19, 20));
	CHECK_EQ(6, GetSoulRebirthRange(20, 20));
	CHECK_EQ(6, GetSoulRebirthRange(29, 29));
	CHECK_EQ(7, GetSoulRebirthRange(29, 30));
	CHECK_EQ(8, GetSoulRebirthRange(30, 30));
}

TEST(SoulRebirthRange, MatchesServerFormulaAndMasteryForEverySupportedLevel)
{
	// SoulRebirth::execute uses the active slot's formula Range, then adds
	// the passive slot's level / 10 before checking the target distance.
	// Only SkillLevel affects this formula's Range. No character stats or
	// advancement level contribute, and a missing passive slot adds zero.
	int mismatches = 0;
	for (int active = 0; active <= 30; ++active)
	{
		decore::skillformula::SkillInput input{};
		input.SkillLevel = active;
		decore::skillformula::SkillOutput output;
		decore::skillformula::SoulRebirth(input, output);
		for (int mastery = 0; mastery <= 30; ++mastery)
			if (GetSoulRebirthRange(active, mastery) != output.Range + mastery / 10)
				++mismatches;
	}
	CHECK_EQ(0, mismatches);
}
