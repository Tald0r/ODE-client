#include "test_framework.h"
#include "MathTable.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <numbers>

namespace {

// GameInit initializes these shared tables once before creating effects.
// The legacy initializer scales the atan table in place, so calling it for
// every test would change its units. Keep the production startup contract.
void InitTables()
{
	static const bool initialized = [] {
		MathTable::FCreateSines();
		return true;
	}();
	(void)initialized;
}

double TargetAngle(int dx, int dy)
{
	double angle = std::atan2(-static_cast<double>(dy), static_cast<double>(dx))
		* MathTable::MAX_ANGLE / (2.0 * std::numbers::pi);
	if (angle < 0)
		angle += MathTable::MAX_ANGLE;
	return angle;
}

double AngleDifference(double first, double second)
{
	const double distance = std::abs(first - second);
	return (std::min)(distance, MathTable::MAX_ANGLE - distance);
}

} // namespace

TEST(MathTable, DegreeConversionPreservesIntegerTruncationAndFullTurns)
{
	struct Conversion { int degrees; int angle; };
	const Conversion cases[] = {
		{0, 0}, {1, 5}, {30, 170}, {45, 256}, {60, 341}, {90, 512},
		{180, 1024}, {270, 1536}, {359, 2042}, {360, 2048}, {720, 4096},
		{-1, -5}, {-30, -170}, {-90, -512}, {-360, -2048}
	};
	for (const auto& c : cases)
		CHECK_EQ(c.angle, MathTable::GetAngle360(c.degrees));
}

TEST(MathTable, SineAndCosineUseSixteenFractionalBitsAcrossTheWholeCycle)
{
	InitTables();
	for (int angle = 0; angle < MathTable::MAX_ANGLE; ++angle)
	{
		const double radians = angle * (2.0 * std::numbers::pi) / MathTable::MAX_ANGLE;
		const int sine = MathTable::FSin(angle);
		const int cosine = MathTable::FCos(angle);
		// Retain the float-generated table's rounding; a fixed-point unit is
		// 1/65536, and each result is truncated toward zero during generation.
		CHECK(std::abs(sine - std::sin(radians) * 65536.0) < 2.0);
		CHECK(std::abs(cosine - std::cos(radians) * 65536.0) < 2.0);
		CHECK(sine >= -65536 && sine <= 65536);
		CHECK(cosine >= -65536 && cosine <= 65536);
	}
}

TEST(MathTable, PeriodicLookupsAcceptNegativeAndLargeSignedAngles)
{
	InitTables();
	for (int angle : {(std::numeric_limits<int>::min)(), -8193, -4096, -2049,
		-2048, -1, 0, 2047, 2048, 4095, 8193, (std::numeric_limits<int>::max)()})
	{
		const int normalized = (angle % MathTable::MAX_ANGLE + MathTable::MAX_ANGLE)
			% MathTable::MAX_ANGLE;
		CHECK_EQ(MathTable::FSin(normalized), MathTable::FSin(angle));
		CHECK_EQ(MathTable::FCos(normalized), MathTable::FCos(angle));
	}
}

TEST(MathTable, CardinalTargetsUseScreenCoordinatesAndCoincidentTargetsFaceRight)
{
	InitTables();
	for (int origin : {-100000, 0, 100000})
	{
		CHECK_EQ(MathTable::ANGLE_0, MathTable::GetAngleToTarget(origin, origin, origin, origin));
		for (int distance : {1, 17, 1000})
		{
			CHECK_EQ(MathTable::ANGLE_0,
				MathTable::GetAngleToTarget(origin, origin, origin + distance, origin));
			CHECK_EQ(MathTable::ANGLE_90,
				MathTable::GetAngleToTarget(origin, origin, origin, origin - distance));
			CHECK_EQ(MathTable::ANGLE_180,
				MathTable::GetAngleToTarget(origin, origin, origin - distance, origin));
			CHECK_EQ(MathTable::ANGLE_270,
				MathTable::GetAngleToTarget(origin, origin, origin, origin + distance));
		}
	}
}

TEST(MathTable, TargetAnglesFollowEveryScreenOctant)
{
	InitTables();
	for (int dx = -24; dx <= 24; ++dx)
	{
		for (int dy = -24; dy <= 24; ++dy)
		{
			if (dx == 0 && dy == 0)
				continue;
			const int angle = MathTable::GetAngleToTarget(100, -200, 100 + dx, -200 + dy);
			CHECK(angle >= 0 && angle < MathTable::MAX_ANGLE);
			// The integer ratio and quantized atan table approximate atan2.
			CHECK(AngleDifference(angle, TargetAngle(dx, dy)) <= 3.0);
			CHECK_EQ(angle, MathTable::GetAngleToTarget(-1000, 2000,
				-1000 + 7 * dx, 2000 + 7 * dy));
		}
	}
}

TEST(MathTable, TurnDirectionUsesTheShorterArcAndRetainsHalfTurnTies)
{
	struct Turn { int current; int target; int direction; };
	const Turn cases[] = {
		{0, 0, 0}, {2047, 2047, 0}, {0, 1, 1}, {1, 0, -1},
		{0, 2047, -1}, {2047, 0, 1}, {0, 1023, 1}, {1023, 0, -1},
		{0, 1024, -1}, {1024, 0, 1}, {0, 1025, -1}, {1025, 0, 1},
		{512, 1536, -1}, {1536, 512, 1}, {1700, 100, 1}, {100, 1700, -1}
	};
	for (const auto& c : cases)
		CHECK_EQ(c.direction, MathTable::GetAngleDir(c.current, c.target));
}

TEST(MathTable, ClipAngleCorrectsAnOvershootOfOneTurn)
{
	struct Clip { int input; int expected; };
	const Clip cases[] = {
		{-2048, 0}, {-2047, 1}, {-1, 2047}, {0, 0}, {1, 1},
		{2047, 2047}, {2048, 0}, {2049, 1}, {4095, 2047}
	};
	for (const auto& c : cases)
		CHECK_EQ(c.expected, MathTable::ClipAngle(c.input));
}

TEST(MathTable, ArcTanSamplesApproximateRatiosFromMinusOneToOne)
{
	InitTables();
	for (int index = 0; index <= MathTable::MAX_ANGLE; ++index)
	{
		const double ratio = (index - MathTable::MAX_ANGLE_HALF)
			/ static_cast<double>(MathTable::MAX_ANGLE_HALF);
		const double expected = std::atan(ratio) * MathTable::MAX_ANGLE / (2.0 * std::numbers::pi);
		CHECK(std::abs(MathTable::FArcTan(index) - expected) <= 2.0);
	}
}

TEST(MathTable, ArcTanEndpointsUseTheSameAngleScale)
{
	InitTables();
	CHECK_EQ(-MathTable::ANGLE_45, MathTable::FArcTan(0));
	CHECK_EQ(0, MathTable::FArcTan(MathTable::MAX_ANGLE_HALF));
	CHECK_EQ(MathTable::ANGLE_45, MathTable::FArcTan(MathTable::MAX_ANGLE));
}

TEST(MathTable, ExactDiagonalTargetsUseFortyFiveDegreeAngles)
{
	InitTables();
	for (int origin : {-100000, 0, 100000})
	{
		for (int distance : {1, 7, 128, 4096})
		{
			CHECK_EQ(MathTable::ANGLE_45,
				MathTable::GetAngleToTarget(origin, origin, origin + distance, origin - distance));
			CHECK_EQ(MathTable::ANGLE_135,
				MathTable::GetAngleToTarget(origin, origin, origin - distance, origin - distance));
			CHECK_EQ(MathTable::ANGLE_225,
				MathTable::GetAngleToTarget(origin, origin, origin - distance, origin + distance));
			CHECK_EQ(MathTable::ANGLE_315,
				MathTable::GetAngleToTarget(origin, origin, origin + distance, origin + distance));
		}
	}
}

TEST(MathTable, CardinalTargetsPreserveDirectionAcrossTheFullIntegerSpan)
{
	InitTables();
	const int low = (std::numeric_limits<int>::min)();
	const int high = (std::numeric_limits<int>::max)();
	CHECK_EQ(MathTable::ANGLE_0, MathTable::GetAngleToTarget(low, 0, high, 0));
	CHECK_EQ(MathTable::ANGLE_180, MathTable::GetAngleToTarget(high, 0, low, 0));
	CHECK_EQ(MathTable::ANGLE_270, MathTable::GetAngleToTarget(0, low, 0, high));
	CHECK_EQ(MathTable::ANGLE_90, MathTable::GetAngleToTarget(0, high, 0, low));
}

TEST(MathTable, DiagonalTargetsPreserveQuadrantsAcrossTheFullIntegerSpan)
{
	InitTables();
	const int low = (std::numeric_limits<int>::min)();
	const int high = (std::numeric_limits<int>::max)();
	CHECK_EQ(MathTable::ANGLE_45, MathTable::GetAngleToTarget(low, high, high, low));
	CHECK_EQ(MathTable::ANGLE_135, MathTable::GetAngleToTarget(high, high, low, low));
	CHECK_EQ(MathTable::ANGLE_225, MathTable::GetAngleToTarget(high, low, low, high));
	CHECK_EQ(MathTable::ANGLE_315, MathTable::GetAngleToTarget(low, low, high, high));
}

TEST(MathTable, LargeTargetVectorsKeepTheirAnglesWhenScaled)
{
	InitTables();
	struct Offset { int x, y; };
	for (const auto& v : {Offset{6, 5}, {5, 6}, {-6, 5}, {-5, 6},
		{6, -5}, {5, -6}, {-6, -5}, {-5, -6},
		{6, 6}, {6, -6}, {-6, 6}, {-6, -6}})
	{
		CHECK_EQ(MathTable::GetAngleToTarget(0, 0, v.x, v.y),
			MathTable::GetAngleToTarget(0, 0, v.x * 500000, v.y * 500000));
	}
}

TEST(MathTable, BoundaryTargetsFollowEveryScreenOctant)
{
	InitTables();
	const int low = (std::numeric_limits<int>::min)();
	const int high = (std::numeric_limits<int>::max)();
	const int coordinates[] = {low, low + 1, -3000000, -1, 0, 1, 3000000, high - 1, high};
	for (int x : coordinates)
	{
		for (int y : coordinates)
		{
			if (x == 0 && y == 0) continue;
			const int angle = MathTable::GetAngleToTarget(0, 0, x, y);
			CHECK(angle >= 0 && angle < MathTable::MAX_ANGLE);
			CHECK(AngleDifference(angle, TargetAngle(x, y)) <= 3.0);
		}
	}
}

TEST(MathTable, TranslatingSmallVectorsNearIntegerLimitsKeepsQuantization)
{
	InitTables();
	const int low = (std::numeric_limits<int>::min)();
	const int high = (std::numeric_limits<int>::max)();
	struct Offset { int x, y; };
	for (int origin : {low + 64, high - 64})
	{
		for (const auto& v : {Offset{6, 5}, {5, 6}, {-6, 5}, {-5, 6},
			{6, -5}, {-5, -6}, {0, 0}, {-1, 0}})
		{
			CHECK_EQ(MathTable::GetAngleToTarget(0, 0, v.x, v.y),
				MathTable::GetAngleToTarget(origin, origin, origin + v.x, origin + v.y));
		}
	}
}
