#include "test_framework.h"
#include "DirectionSelection.h"

#include <initializer_list>

TEST(DirectionSelection, CoincidentPositionsFaceDown)
{
	for (int x : {-10000, -1, 0, 1, 10000})
		for (int y : {-10000, -1, 0, 1, 10000})
			CHECK_EQ(DIRECTION_DOWN, SelectFacingDirection(x, y, x, y));
}

TEST(DirectionSelection, AxisAlignedPositionsFaceTheFourCardinalDirections)
{
	for (int distance : {1, 2, 100, 10000})
	{
		CHECK_EQ(DIRECTION_RIGHT, SelectFacingDirection(0, 0, distance, 0));
		CHECK_EQ(DIRECTION_LEFT, SelectFacingDirection(0, 0, -distance, 0));
		CHECK_EQ(DIRECTION_DOWN, SelectFacingDirection(0, 0, 0, distance));
		CHECK_EQ(DIRECTION_UP, SelectFacingDirection(0, 0, 0, -distance));
	}
}

TEST(DirectionSelection, EqualDisplacementsFaceTheFourDiagonals)
{
	for (int distance : {1, 2, 100, 10000})
	{
		CHECK_EQ(DIRECTION_RIGHTDOWN, SelectFacingDirection(0, 0, distance, distance));
		CHECK_EQ(DIRECTION_RIGHTUP, SelectFacingDirection(0, 0, distance, -distance));
		CHECK_EQ(DIRECTION_LEFTDOWN, SelectFacingDirection(0, 0, -distance, distance));
		CHECK_EQ(DIRECTION_LEFTUP, SelectFacingDirection(0, 0, -distance, -distance));
	}
}

TEST(DirectionSelection, LowSlopeCutoffRetainsFloatRoundingAtPointThreeFive)
{
	// 35/100 rounds below the double-valued 0.35 cutoff in the original
	// calculation. That exact rational slope therefore remains horizontal.
	for (int y : {34, 35})
	{
		CHECK_EQ(DIRECTION_RIGHT, SelectFacingDirection(0, 0, 100, y));
		CHECK_EQ(DIRECTION_RIGHT, SelectFacingDirection(0, 0, 100, -y));
		CHECK_EQ(DIRECTION_LEFT, SelectFacingDirection(0, 0, -100, y));
		CHECK_EQ(DIRECTION_LEFT, SelectFacingDirection(0, 0, -100, -y));
	}
	CHECK_EQ(DIRECTION_RIGHTDOWN, SelectFacingDirection(0, 0, 100, 36));
	CHECK_EQ(DIRECTION_RIGHTUP, SelectFacingDirection(0, 0, 100, -36));
	CHECK_EQ(DIRECTION_LEFTDOWN, SelectFacingDirection(0, 0, -100, 36));
	CHECK_EQ(DIRECTION_LEFTUP, SelectFacingDirection(0, 0, -100, -36));
}

TEST(DirectionSelection, HighSlopeCutoffIncludesTheExactRatioInTheDiagonal)
{
	for (int y : {299, 300})
	{
		CHECK_EQ(DIRECTION_RIGHTDOWN, SelectFacingDirection(0, 0, 100, y));
		CHECK_EQ(DIRECTION_RIGHTUP, SelectFacingDirection(0, 0, 100, -y));
		CHECK_EQ(DIRECTION_LEFTDOWN, SelectFacingDirection(0, 0, -100, y));
		CHECK_EQ(DIRECTION_LEFTUP, SelectFacingDirection(0, 0, -100, -y));
	}
	CHECK_EQ(DIRECTION_DOWN, SelectFacingDirection(0, 0, 100, 301));
	CHECK_EQ(DIRECTION_UP, SelectFacingDirection(0, 0, 100, -301));
	CHECK_EQ(DIRECTION_DOWN, SelectFacingDirection(0, 0, -100, 301));
	CHECK_EQ(DIRECTION_UP, SelectFacingDirection(0, 0, -100, -301));
}

TEST(DirectionSelection, ShallowAndSteepSlopesIgnoreTheMinorAxisSign)
{
	for (int sign : {-1, 1})
	{
		CHECK_EQ(DIRECTION_RIGHT, SelectFacingDirection(0, 0, 10000, sign));
		CHECK_EQ(DIRECTION_LEFT, SelectFacingDirection(0, 0, -10000, sign));
		CHECK_EQ(DIRECTION_DOWN, SelectFacingDirection(0, 0, sign, 10000));
		CHECK_EQ(DIRECTION_UP, SelectFacingDirection(0, 0, sign, -10000));
	}
}

TEST(DirectionSelection, FacingDependsOnDisplacementRatherThanAbsolutePosition)
{
	for (int x = -40; x <= 40; ++x)
		for (int y = -40; y <= 40; ++y)
		{
			const auto expected = SelectFacingDirection(0, 0, x, y);
			CHECK_EQ(expected, SelectFacingDirection(10000, -5000, 10000 + x, -5000 + y));
			CHECK_EQ(expected, SelectFacingDirection(-10000, 5000, -10000 + x, 5000 + y));
		}
}

TEST(DirectionSelection, ReversingNonzeroDisplacementSelectsTheOppositeDirection)
{
	for (int x = -40; x <= 40; ++x)
		for (int y = -40; y <= 40; ++y)
		{
			if (x == 0 && y == 0) continue;
			const int forward = SelectFacingDirection(0, 0, x, y);
			CHECK_EQ((forward + 4) % 8, SelectFacingDirection(0, 0, -x, -y));
		}
}

TEST(DirectionSelection, ScalingExactSmallDisplacementsPreservesFacing)
{
	for (int x = -40; x <= 40; ++x)
		for (int y = -40; y <= 40; ++y)
		{
			const auto expected = SelectFacingDirection(0, 0, x, y);
			CHECK_EQ(expected, SelectFacingDirection(0, 0, x * 32, y * 32));
		}
}

TEST(DirectionSelection, LargeCoordinatesKeepTheExistingFloatThresholdQuantization)
{
	CHECK_EQ(DIRECTION_RIGHT, SelectFacingDirection(0, 0, 100000000, 35000001));
	CHECK_EQ(DIRECTION_RIGHTDOWN, SelectFacingDirection(0, 0, 100000000, 35000003));
	CHECK_EQ(DIRECTION_RIGHTDOWN, SelectFacingDirection(0, 0, 100000000, 300000001));
	CHECK_EQ(DIRECTION_DOWN, SelectFacingDirection(0, 0, 100000000, 300000017));
	CHECK_EQ(DIRECTION_LEFTUP, SelectFacingDirection(0, 0, -100000000, -300000001));
	CHECK_EQ(DIRECTION_UP, SelectFacingDirection(0, 0, -100000000, -300000017));
}
