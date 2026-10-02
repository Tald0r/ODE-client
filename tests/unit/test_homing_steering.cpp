#include "test_framework.h"
#include "HomingEffectSteering.h"
#include "MathTable.h"

#include <algorithm>
#include <array>

namespace {
// The legacy initializer scales atan entries in place. Initialize only when
// needed, then restore all shared tables so another test can own startup.
struct Tables
{
	std::array<int, MathTable::MAX_ANGLE> sine, cosine;
	std::array<int, MathTable::MAX_ANGLE + 1> tangent;
	Tables()
	{
		std::copy_n(MathTable::FSinTab, sine.size(), sine.begin());
		std::copy_n(MathTable::FCosTab, cosine.size(), cosine.begin());
		std::copy_n(MathTable::FArcTanTab, tangent.size(), tangent.begin());
		if (MathTable::FCosTab[0] != 65536) MathTable::FCreateSines();
	}
	~Tables()
	{
		std::copy(sine.begin(), sine.end(), MathTable::FSinTab);
		std::copy(cosine.begin(), cosine.end(), MathTable::FCosTab);
		std::copy(tangent.begin(), tangent.end(), MathTable::FArcTanTab);
	}
};

void Step(const POINT& actual, int x, int y)
{
	CHECK_EQ(x, actual.x); CHECK_EQ(y, actual.y);
}
}

TEST(HomingSteering, ConstructorConvertsDegreesWithIntegerTruncation)
{
	struct Example { int degrees, angle; };
	for (const auto& e : {Example{0, 0}, {13, 73}, {30, 170}, {45, 256},
		{90, 512}, {180, 1024}, {315, 1792}, {360, 2048}, {-30, -170}})
	{
		HomingEffectSteering steering(e.degrees, e.degrees);
		CHECK_EQ(e.angle, steering.GetAngle());
		CHECK_EQ(e.angle, steering.GetTurnStep());
	}
}

TEST(HomingSteering, CardinalDisplacementsKeepLegacyFixedPointRounding)
{
	Tables tables;
	struct Example { int degrees, x, y; };
	for (const auto& e : {Example{0, 64, 0}, {90, 0, -63}, {180, -64, 0}, {270, 0, 64}})
	{
		HomingEffectSteering steering(e.degrees, 0);
		Step(steering.Advance(64), e.x, e.y);
	}
}

TEST(HomingSteering, DiagonalDisplacementsUseScreenYAndFloorNegativeValues)
{
	Tables tables;
	struct Example { int degrees, x, y; };
	for (const auto& e : {Example{45, 45, -45}, {135, -46, -45},
		{225, -46, 46}, {315, 45, 46}})
	{
		HomingEffectSteering steering(e.degrees, 0);
		Step(steering.Advance(64), e.x, e.y);
	}
}

TEST(HomingSteering, AdvanceTurnsBeforeCalculatingDisplacement)
{
	Tables tables;
	HomingEffectSteering steering(0, 90);
	Step(steering.Advance(64), 0, -63);
	CHECK_EQ(512, steering.GetAngle());
	Step(steering.Advance(64), -64, 0);
	CHECK_EQ(1024, steering.GetAngle());
}

TEST(HomingSteering, PositiveAndNegativeTurnsWrapAroundTheCycle)
{
	Tables tables;
	HomingEffectSteering positive(315, 90);
	Step(positive.Advance(64), 45, -45);
	CHECK_EQ(256, positive.GetAngle());
	HomingEffectSteering negative(0, -45);
	Step(negative.Advance(64), 45, 46);
	CHECK_EQ(1792, negative.GetAngle());
}

TEST(HomingSteering, CompleteAndNegativeInitialTurnsNormalizeWhenAdvanced)
{
	Tables tables;
	HomingEffectSteering complete(360, 0);
	Step(complete.Advance(64), 64, 0);
	CHECK_EQ(0, complete.GetAngle());
	HomingEffectSteering negative(-90, 0);
	Step(negative.Advance(64), 0, 64);
	CHECK_EQ(1536, negative.GetAngle());
}

TEST(HomingSteering, TrackingChoosesTheShorterTurnAcrossTheCycleBoundary)
{
	Tables tables;
	HomingEffectSteering up(315, 13);
	up.TurnToward(0, 0, 10, 0);
	CHECK_EQ(73, up.GetTurnStep());
	HomingEffectSteering down(45, 13);
	down.TurnToward(0, 0, 10, 0);
	CHECK_EQ(-73, down.GetTurnStep());
}

TEST(HomingSteering, ExactHalfTurnsRetainTheExistingTieBreak)
{
	Tables tables;
	HomingEffectSteering right(0, 13);
	right.TurnToward(0, 0, -10, 0);
	CHECK_EQ(-73, right.GetTurnStep());
	HomingEffectSteering left(180, 13);
	left.TurnToward(0, 0, 10, 0);
	CHECK_EQ(73, left.GetTurnStep());
}

TEST(HomingSteering, ANewTargetCanReverseTheCurrentTurn)
{
	Tables tables;
	HomingEffectSteering steering(0, 13);
	steering.TurnToward(10, 20, 10, 10);
	CHECK_EQ(73, steering.GetTurnStep());
	steering.TurnToward(10, 20, 10, 30);
	CHECK_EQ(-73, steering.GetTurnStep());
	steering.TurnToward(10, 20, 10, 10);
	CHECK_EQ(73, steering.GetTurnStep());
}

TEST(HomingSteering, DiagonalTargetsUseTheSharedAngleTable)
{
	Tables tables;
	HomingEffectSteering above(30, 13);
	above.TurnToward(5, 7, 25, -13);
	CHECK_EQ(73, above.GetTurnStep());
	HomingEffectSteering below(60, 13);
	below.TurnToward(5, 7, 25, -13);
	CHECK_EQ(-73, below.GetTurnStep());
}

TEST(HomingSteering, TrackingPreservesTurnMagnitudeRegardlessOfItsInitialSign)
{
	Tables tables;
	HomingEffectSteering steering(0, -30);
	steering.TurnToward(0, 0, 0, -20);
	CHECK_EQ(170, steering.GetTurnStep());
	steering.TurnToward(0, 0, 0, 20);
	CHECK_EQ(-170, steering.GetTurnStep());
}

TEST(HomingSteering, AlignmentRetainsTheLegacyLossOfTurnMagnitude)
{
	Tables tables;
	HomingEffectSteering steering(0, 13);
	steering.TurnToward(0, 0, 100, 0);
	CHECK_EQ(0, steering.GetTurnStep());
	steering.TurnToward(0, 0, 0, -100);
	CHECK_EQ(0, steering.GetTurnStep());
	Step(steering.Advance(64), 64, 0);
}

TEST(HomingSteering, CoincidentTargetsUseTheExistingRightFacingAngle)
{
	Tables tables;
	HomingEffectSteering steering(90, 13);
	steering.TurnToward(5, 7, 5, 7);
	CHECK_EQ(-73, steering.GetTurnStep());
}

TEST(HomingSteering, StoppingTheTurnKeepsTheCurrentHeading)
{
	Tables tables;
	HomingEffectSteering steering(45, 30);
	steering.StopTurning();
	CHECK_EQ(0, steering.GetTurnStep());
	CHECK_EQ(256, steering.GetAngle());
	Step(steering.Advance(64), 45, -45);
	steering.TurnToward(0, 0, -20, 0);
	CHECK_EQ(0, steering.GetTurnStep());
}

TEST(HomingSteering, ZeroSpeedStillAdvancesTheAngle)
{
	Tables tables;
	HomingEffectSteering steering(0, 30);
	Step(steering.Advance(0), 0, 0);
	CHECK_EQ(170, steering.GetAngle());
	CHECK_EQ(170, steering.GetTurnStep());
}

TEST(HomingSteering, FixedTurnsContinueWithoutTargetTracking)
{
	Tables tables;
	HomingEffectSteering steering(0, 45);
	const POINT expected[]{{45, -45}, {0, -63}, {-46, -45}, {-64, 0},
		{-46, 46}, {0, 64}, {45, 46}, {64, 0}};
	for (int turn = 0; turn < 2; ++turn)
		for (const auto& point : expected)
			Step(steering.Advance(64), point.x, point.y);
	CHECK_EQ(0, steering.GetAngle());
}

TEST(HomingSteering, CopiesKeepIndependentAnglesAndTurnState)
{
	Tables tables;
	HomingEffectSteering first(0, 90);
	HomingEffectSteering second = first;
	first.StopTurning();
	Step(first.Advance(64), 64, 0);
	Step(second.Advance(64), 0, -63);
	CHECK_EQ(0, first.GetAngle());
	CHECK_EQ(512, second.GetAngle());
}
