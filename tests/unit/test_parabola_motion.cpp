#include "test_framework.h"
#include "ParabolaEffectMotion.h"
#include "LinearEffectMotion.h"
#include "MathTable.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
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

struct Flight
{
	LinearEffectMotion path;
	ParabolaEffectMotion arc;
	float x = 0, y = 0, z = 0;
	void Target(int tx, int ty, int tz, WORD speed)
	{
		path.SetLinearTarget(x, y, z, tx, ty, tz, speed);
		arc.Reset(path, speed);
	}
	void Advance(WORD speed) { arc.Advance(path, x, y, z, speed); }
	bool Finish(WORD distance) { return arc.FinishStep(path, x, y, z, distance); }
};

void At(const Flight& flight, float x, float y, float z)
{
	CHECK(std::fabs(flight.x - x) < 0.0001f);
	CHECK(std::fabs(flight.y - y) < 0.0001f);
	CHECK(std::fabs(flight.z - z) < 0.0001f);
}
}

TEST(ParabolaMotion, DefaultStateHasNoLinearMovementOrArcProgression)
{
	Tables tables;
	Flight flight;
	flight.Advance(0);
	At(flight, 0, 0, 0);
	CHECK(!flight.Finish(1));
	flight.Advance(10);
	At(flight, 0, 0, 10);
	CHECK(!flight.Finish(10));
}

TEST(ParabolaMotion, AdvancingMovesLinearlyThenAddsTheNextArcHeight)
{
	Tables tables;
	Flight flight;
	flight.Target(40, 0, 0, 10);
	flight.Advance(10);
	At(flight, 10, 0, 7);
	CHECK(!flight.Finish(10));
	flight.Advance(10);
	At(flight, 20, 0, 7);
	CHECK(!flight.Finish(10));
}

TEST(ParabolaMotion, ThreeDimensionalVelocityContributesToHeight)
{
	Tables tables;
	Flight flight;
	flight.Target(30, 40, 120, 13);
	flight.Advance(13);
	At(flight, 3, 4, 24);
}

TEST(ParabolaMotion, SmokeCanObserveTheAdvancedPositionBeforeLandingSnapsIt)
{
	Tables tables;
	Flight flight;
	flight.Target(40, 0, 0, 10);
	flight.Advance(10);
	flight.Advance(10);
	flight.Advance(10);
	// The legacy negative cosine rounds down, reaching below the target
	// before the half-turn condition. The executable emits smoke here.
	At(flight, 30, 0, -1);
	CHECK(flight.Finish(10));
	At(flight, 40, 0, 0);
}

TEST(ParabolaMotion, HorizontalProximityDoesNotLandBeforeHalfATurn)
{
	Tables tables;
	Flight flight;
	flight.Target(40, 0, 0, 10);
	flight.x = 30; flight.z = 100;
	flight.Advance(10);
	At(flight, 40, 0, 107);
	CHECK(!flight.Finish(10));
	At(flight, 40, 0, 107);
}

TEST(ParabolaMotion, AfterHalfATurnArrivalIgnoresHeightProximity)
{
	Tables tables;
	Flight flight;
	flight.Target(40, 0, 0, 10);
	flight.z = 100;
	for (int move = 0; move < 3; ++move)
	{
		flight.Advance(10);
		CHECK(!flight.Finish(10));
	}
	flight.Advance(10);
	At(flight, 40, 0, 89);
	CHECK(flight.Finish(10));
	At(flight, 40, 0, 0);
}

TEST(ParabolaMotion, EveryHorizontalArrivalBoundaryIsStrict)
{
	Tables tables;
	for (int axis = 0; axis < 2; ++axis)
		for (int sign : {-1, 1})
		{
			Flight flight;
			flight.Target(0, 0, 0, 10);
			flight.z = 100;
			flight.Advance(0); // complete half a turn without a height offset
			flight.x = axis == 0 ? sign*10.0f : 0;
			flight.y = axis == 1 ? sign*10.0f : 0;
			CHECK(!flight.Finish(10));
			flight.x *= 0.99f; flight.y *= 0.99f;
			CHECK(flight.Finish(10));
			At(flight, 0, 0, 0);
		}
}

TEST(ParabolaMotion, FallingBelowTheTargetLandsBeforeHalfATurnAndWithoutXYProximity)
{
	Tables tables;
	Flight flight;
	flight.Target(40, 50, 0, 10);
	flight.z = -0.25f;
	CHECK(flight.Finish(0));
	At(flight, 40, 50, 0);
}

TEST(ParabolaMotion, EqualTargetHeightIsNotBelowIt)
{
	Tables tables;
	Flight flight;
	flight.Target(40, 0, 0, 10);
	CHECK(!flight.Finish(10));
	At(flight, 0, 0, 0);
}

TEST(ParabolaMotion, ATargetAboveTheCurrentHeightRetainsTheImmediateLandingRule)
{
	Tables tables;
	Flight flight;
	flight.Target(30, 40, 120, 13);
	flight.Advance(13);
	CHECK(flight.Finish(13));
	At(flight, 30, 40, 120);
}

TEST(ParabolaMotion, LandingClearsEveryComponentOfTheLinearVelocity)
{
	Tables tables;
	Flight flight;
	flight.Target(30, 40, 120, 13);
	flight.Advance(13);
	CHECK(flight.Finish(13));
	flight.x = flight.y = flight.z = 1000;
	CHECK(!flight.path.AdvanceLinear(flight.x, flight.y, flight.z, 1));
	At(flight, 1000, 1000, 1000);
}

TEST(ParabolaMotion, ShortAndZeroLengthPathsCompleteTheirArcInOneStep)
{
	Tables tables;
	for (int distance : {0, 5})
	{
		Flight flight;
		flight.Target(distance, 0, 0, 10);
		flight.Advance(10);
		At(flight, distance == 0 ? 0.0f : 10.0f, 0, -10);
		CHECK(flight.Finish(10));
		At(flight, static_cast<float>(distance), 0, 0);
	}
}

TEST(ParabolaMotion, FractionalLengthsRetainWholeStepTruncation)
{
	Tables tables;
	Flight flight;
	flight.x = 0.5f;
	flight.Target(20, 0, 0, 10);
	flight.Advance(10);
	At(flight, 10.5f, 0, -10);
	CHECK(flight.Finish(10));
	At(flight, 20, 0, 0);
}

TEST(ParabolaMotion, ZeroSpeedRemainsStationaryAndDoesNotArriveAtLevelHeight)
{
	Tables tables;
	Flight flight;
	flight.Target(100, 0, 0, 0);
	for (int move = 0; move < 5; ++move)
	{
		flight.Advance(0);
		CHECK(!flight.Finish(0));
		At(flight, 0, 0, 0);
	}
}

TEST(ParabolaMotion, PathsBeyondTheAngleResolutionRetainZeroArcStep)
{
	Tables tables;
	Flight flight;
	flight.Target(2048, 0, 0, 1);
	for (int move = 1; move <= 5; ++move)
	{
		flight.Advance(1);
		At(flight, static_cast<float>(move), 0, static_cast<float>(move));
		CHECK(!flight.Finish(1));
	}
}

TEST(ParabolaMotion, FullTurnsKeepTheArrivalGateOpen)
{
	Tables tables;
	Flight flight;
	flight.Target(40, 0, 0, 10);
	flight.z = 100;
	for (int move = 0; move < 8; ++move) flight.Advance(10);
	At(flight, 80, 0, 98);
	flight.x = 40;
	CHECK(flight.Finish(10));
	At(flight, 40, 0, 0);
}

TEST(ParabolaMotion, ChangingSpeedDoesNotRecomputeTheLinearVelocityOrArcStep)
{
	Tables tables;
	Flight flight;
	flight.Target(40, 0, 0, 10);
	flight.Advance(20);
	At(flight, 10, 0, 14);
	flight.Advance(30);
	At(flight, 20, 0, 14);
}

TEST(ParabolaMotion, ResetRestartsTheArcAfterItsArrivalGateWasOpened)
{
	Tables tables;
	Flight flight;
	flight.Target(40, 0, 0, 10);
	for (int move = 0; move < 4; ++move) flight.Advance(0);
	flight.arc.Reset(flight.path, 10);
	flight.x = 30; flight.z = 100;
	flight.Advance(10);
	At(flight, 40, 0, 107);
	CHECK(!flight.Finish(10));
}

TEST(ParabolaMotion, CopiesKeepIndependentArcAndTrajectoryState)
{
	Tables tables;
	Flight first;
	first.Target(40, 0, 0, 10);
	first.Advance(10);
	Flight second = first;
	first.arc.Reset(first.path, 10);
	first.Advance(10);
	second.Advance(10);
	At(first, 20, 0, 14);
	At(second, 20, 0, 7);
}
