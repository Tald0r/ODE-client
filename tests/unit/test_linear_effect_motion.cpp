#include "test_framework.h"
#include "LinearEffectMotion.h"

#include <array>
#include <cmath>
#include <limits>

namespace {
struct Position
{
	float x = 0, y = 0, z = 0;
	void Target(LinearEffectMotion& motion, int tx, int ty, int tz, WORD speed)
	{
		motion.SetLinearTarget(x, y, z, tx, ty, tz, speed);
	}
	bool Advance(LinearEffectMotion& motion, WORD arrivalDistance)
	{
		return motion.AdvanceLinear(x, y, z, arrivalDistance);
	}
};

void Near(float expected, float actual)
{
	CHECK(std::fabs(expected - actual) < 0.0001f);
}

void At(const Position& position, float x, float y, float z)
{
	Near(x, position.x); Near(y, position.y); Near(z, position.z);
}
}

TEST(LinearEffectMotion, DefaultMotionHasZeroLengthAndNoVelocity)
{
	LinearEffectMotion motion;
	Position origin;
	Near(0, motion.GetPathLength());
	CHECK(origin.Advance(motion, 1));
	At(origin, 0, 0, 0);
	Position displaced{3, 4, 5};
	CHECK(!displaced.Advance(motion, 1));
	At(displaced, 3, 4, 5);
}

TEST(LinearEffectMotion, EachAxisMovesInBothDirectionsAndStopsOnArrival)
{
	for (int axis = 0; axis < 3; ++axis)
		for (int sign : {-1, 1})
		{
			LinearEffectMotion motion;
			Position position;
			std::array<int, 3> target{};
			target[axis] = sign * 12;
			position.Target(motion, target[0], target[1], target[2], 2);
			Near(12, motion.GetPathLength());
			for (int move = 1; move <= 6; ++move)
			{
				CHECK_EQ(move == 6, position.Advance(motion, 2));
				At(position, axis == 0 ? sign*move*2.0f : 0,
					axis == 1 ? sign*move*2.0f : 0,
					axis == 2 ? sign*move*2.0f : 0);
			}
			CHECK(position.Advance(motion, 2));
			At(position, static_cast<float>(target[0]),
				static_cast<float>(target[1]), static_cast<float>(target[2]));
		}
}

TEST(LinearEffectMotion, VelocityUsesTheThreeDimensionalDistance)
{
	LinearEffectMotion motion;
	Position position;
	position.Target(motion, 30, 40, 120, 13);
	Near(130, motion.GetPathLength());
	for (int move = 1; move <= 8; ++move)
	{
		CHECK(!position.Advance(motion, 13));
		At(position, move*3.0f, move*4.0f, move*12.0f);
	}
	CHECK(position.Advance(motion, 13));
	At(position, 30, 40, 120);
	Near(130, motion.GetPathLength());
}

TEST(LinearEffectMotion, ArrivalUsesAnAxisAlignedCube)
{
	LinearEffectMotion motion;
	Position position{6, -6, 6};
	// No velocity: all three distances fit, although the Euclidean distance
	// to the default target exceeds seven pixels.
	CHECK(position.Advance(motion, 7));
	At(position, 0, 0, 0);
}

TEST(LinearEffectMotion, EveryArrivalBoundaryIsStrict)
{
	for (int axis = 0; axis < 3; ++axis)
		for (int sign : {-1, 1})
		{
			LinearEffectMotion motion;
			Position boundary{axis == 0 ? sign*1.0f : 0,
				axis == 1 ? sign*1.0f : 0, axis == 2 ? sign*1.0f : 0};
			CHECK(!boundary.Advance(motion, 1));
			Position inside{boundary.x*0.99f, boundary.y*0.99f, boundary.z*0.99f};
			CHECK(inside.Advance(motion, 1));
			At(inside, 0, 0, 0);
		}
}

TEST(LinearEffectMotion, OneDistantAxisPreventsArrival)
{
	for (Position position : {Position{1, 0, 0}, Position{0, 1, 0}, Position{0, 0, 1}})
	{
		LinearEffectMotion motion;
		const Position previous = position;
		CHECK(!position.Advance(motion, 1));
		At(position, previous.x, previous.y, previous.z);
	}
}

TEST(LinearEffectMotion, ZeroSpeedStaysStillAndDoesNotReportArrival)
{
	LinearEffectMotion motion;
	Position position{4, 5, 6};
	position.Target(motion, 14, 15, 16, 0);
	for (int i = 0; i < 5; ++i)
	{
		CHECK(!position.Advance(motion, 0));
		At(position, 4, 5, 6);
	}
	position.Target(motion, 4, 5, 6, 0);
	CHECK(!position.Advance(motion, 0));
	At(position, 4, 5, 6);
}

TEST(LinearEffectMotion, RetargetingAtTheCurrentPositionClearsOldVelocity)
{
	LinearEffectMotion motion;
	Position position;
	position.Target(motion, 30, 40, 120, 13);
	CHECK(!position.Advance(motion, 13));
	position.Target(motion, 3, 4, 12, 13);
	Near(0, motion.GetPathLength());
	CHECK(position.Advance(motion, 13));
	At(position, 3, 4, 12);
}

TEST(LinearEffectMotion, RetargetingChangesDirectionFromTheLatestPosition)
{
	LinearEffectMotion motion;
	Position position;
	position.Target(motion, 100, 0, 0, 10);
	CHECK(!position.Advance(motion, 10));
	At(position, 10, 0, 0);
	position.Target(motion, -20, 40, 0, 5);
	Near(50, motion.GetPathLength());
	CHECK(!position.Advance(motion, 5));
	At(position, 7, 4, 0);
}

TEST(LinearEffectMotion, FractionalOriginsKeepFractionalVelocity)
{
	LinearEffectMotion motion;
	Position position{0.5f, 0, 0};
	position.Target(motion, 20, 26, 0, 5);
	Near(32.5f, motion.GetPathLength());
	CHECK(!position.Advance(motion, 5));
	At(position, 3.5f, 4, 0);
}

TEST(LinearEffectMotion, OvershootingAShortPathSnapsBackToItsTarget)
{
	for (int sign : {-1, 1})
	{
		LinearEffectMotion motion;
		Position position;
		position.Target(motion, sign*3, sign*4, 0, 10);
		CHECK(position.Advance(motion, 10));
		At(position, sign*3.0f, sign*4.0f, 0);
		CHECK(position.Advance(motion, 10));
		At(position, sign*3.0f, sign*4.0f, 0);
	}
}

TEST(LinearEffectMotion, TheFullWordSpeedMovesWithoutTruncation)
{
	const WORD speed = (std::numeric_limits<WORD>::max)();
	LinearEffectMotion motion;
	Position position;
	position.Target(motion, speed*3, 0, 0, speed);
	CHECK(!position.Advance(motion, speed));
	At(position, speed, 0, 0);
	CHECK(!position.Advance(motion, speed));
	At(position, speed*2.0f, 0, 0);
	CHECK(position.Advance(motion, speed));
	At(position, speed*3.0f, 0, 0);
}

TEST(LinearEffectMotion, ArrivalDistanceCanChangeWithoutRecomputingVelocity)
{
	LinearEffectMotion motion;
	Position position;
	position.Target(motion, 100, 0, 0, 10);
	CHECK(!position.Advance(motion, 50));
	At(position, 10, 0, 0);
	CHECK(position.Advance(motion, 81));
	At(position, 100, 0, 0);
}

TEST(LinearEffectMotion, CallerPositionChangesDoNotRecomputeVelocity)
{
	LinearEffectMotion motion;
	Position position;
	position.Target(motion, 100, 0, 0, 10);
	position = {20, 30, 40};
	CHECK(!position.Advance(motion, 10));
	At(position, 30, 30, 40);
	Near(100, motion.GetPathLength());
}

TEST(LinearEffectMotion, CopiedTrajectoriesRetargetIndependently)
{
	LinearEffectMotion first;
	Position a;
	a.Target(first, 100, 0, 0, 10);
	LinearEffectMotion second = first;
	Position b;
	a.Target(first, 0, -200, 0, 20);
	CHECK(!a.Advance(first, 20));
	CHECK(!b.Advance(second, 10));
	At(a, 0, -20, 0);
	At(b, 10, 0, 0);
	Near(200, first.GetPathLength());
	Near(100, second.GetPathLength());
}
