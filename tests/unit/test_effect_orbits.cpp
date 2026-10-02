#include "test_framework.h"
#include "EffectOrbit.h"
#include "MathTable.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace {
// FCreateSines also scales its atan table in place. Preserve all shared
// tables so these fixtures do not initialize another test's globals twice.
struct Paths
{
	std::array<int, MathTable::MAX_ANGLE> sine, cosine;
	std::array<int, MathTable::MAX_ANGLE + 1> tangent;
	Paths()
	{
		std::copy_n(MathTable::FSinTab, sine.size(), sine.begin());
		std::copy_n(MathTable::FCosTab, cosine.size(), cosine.begin());
		std::copy_n(MathTable::FArcTanTab, tangent.size(), tangent.begin());
		MathTable::FCreateSines();
		EffectOrbit::InitializePositions();
	}
	~Paths()
	{
		std::copy(sine.begin(), sine.end(), MathTable::FSinTab);
		std::copy(cosine.begin(), cosine.end(), MathTable::FCosTab);
		std::copy(tangent.begin(), tangent.end(), MathTable::FArcTanTab);
	}
};

void Position(const EffectOrbit& orbit, int x, int y)
{
	CHECK_EQ(x, orbit.GetPosition().x);
	CHECK_EQ(y, orbit.GetPosition().y);
}
}

TEST(EffectOrbits, EachPathKeepsItsCardinalPixelOffsets)
{
	Paths paths;
	struct Path { int type; int quarter; int x; int y; };
	const Path expected[]{{0, 8, 96, 48}, {1, 8, 48, 24}, {2, 16, 24, 12}};
	for (const auto& path : expected)
	{
		Position(EffectOrbit(path.type, 0), path.x, 0);
		// The legacy float angle at pi/2 produces 65535, not 65536.
		Position(EffectOrbit(path.type, path.quarter), 0, path.y - 1);
		Position(EffectOrbit(path.type, path.quarter * 2), -path.x, 0);
		Position(EffectOrbit(path.type, path.quarter * 3), 0, -path.y);
	}
}

TEST(EffectOrbits, FixedPointRoundingRetainsNegativePixelAsymmetry)
{
	Paths paths;
	Position(EffectOrbit(0, 4), 67, 33);
	Position(EffectOrbit(0, 12), -68, 33);
	Position(EffectOrbit(0, 20), -68, -34);
	Position(EffectOrbit(0, 28), 67, -34);
	Position(EffectOrbit(1, 4), 33, 16);
	Position(EffectOrbit(1, 12), -34, 16);
	Position(EffectOrbit(2, 8), 16, 8);
	Position(EffectOrbit(2, 24), -17, 8);
}

TEST(EffectOrbits, LargeAndMediumPathsRepeatAfterThirtyTwoSteps)
{
	Paths paths;
	for (int type : {0, 1})
		for (int step = 0; step < 32; ++step)
		{
			EffectOrbit first(type, step), second(type, step + 32);
			Position(second, first.GetPosition().x, first.GetPosition().y);
		}
	Position(EffectOrbit(2, 32), -24, 0);
}

TEST(EffectOrbits, EveryCachedStepStaysInsideItsEllipseBounds)
{
	Paths paths;
	const int widths[]{96, 48, 24};
	const int heights[]{48, 24, 12};
	for (int type = 0; type < 3; ++type)
		for (int step = 0; step < 64; ++step)
		{
			const EffectOrbit orbit(type, step);
			const auto& position = orbit.GetPosition();
			CHECK(position.x >= -widths[type] && position.x <= widths[type]);
			CHECK(position.y >= -heights[type] && position.y <= heights[type]);
		}
}

TEST(EffectOrbits, NonnegativeStartingStepsWrapWithoutDrawingRandomness)
{
	unsigned draws = 0;
	for (int step : {0, 1, 63, 64, 65, 129, (std::numeric_limits<int>::max)()})
	{
		EffectOrbit orbit(0, step, [&] { ++draws; return 5u; });
		CHECK_EQ(step % 64, orbit.GetStep());
		CHECK(orbit.IsRunning());
	}
	CHECK_EQ(0, draws);
}

TEST(EffectOrbits, TheMinusOneSentinelDrawsExactlyOneStartingStep)
{
	unsigned draws = 0;
	for (unsigned sample : {0u, 63u, 64u, 129u, (std::numeric_limits<unsigned>::max)()})
	{
		EffectOrbit orbit(1, -1, [&] { ++draws; return sample; });
		CHECK_EQ(sample % 64, orbit.GetStep());
	}
	CHECK_EQ(5, draws);
}

TEST(EffectOrbits, DefaultRandomnessStartsWithinTheCycle)
{
	EffectOrbit orbit(0);
	CHECK(orbit.GetStep() >= 0 && orbit.GetStep() < 64);
	CHECK(orbit.IsRunning());
}

TEST(EffectOrbits, NextStepWrapsAtSixtyFour)
{
	EffectOrbit orbit(0, 0);
	for (int step = 1; step <= 256; ++step)
	{
		orbit.NextStep();
		CHECK_EQ(step % 64, orbit.GetStep());
	}
}

TEST(EffectOrbits, OnlyActiveRunningEffectsAdvanceDuringUpdate)
{
	for (bool active : {false, true})
		for (bool running : {false, true})
		{
			EffectOrbit orbit(0, 63);
			orbit.SetRunning(running);
			orbit.Update(active);
			CHECK_EQ(active && running ? 0 : 63, orbit.GetStep());
			CHECK_EQ(running, orbit.IsRunning());
		}
}

TEST(EffectOrbits, PausedEffectsResumeFromTheSameStep)
{
	EffectOrbit orbit(0, 10);
	orbit.SetRunning(false);
	for (int i = 0; i < 10; ++i) orbit.Update(true);
	CHECK_EQ(10, orbit.GetStep());
	orbit.SetRunning(true);
	orbit.Update(false);
	CHECK_EQ(10, orbit.GetStep());
	orbit.Update(true);
	CHECK_EQ(11, orbit.GetStep());
}

TEST(EffectOrbits, AnExplicitStepAdvanceWorksWhilePaused)
{
	EffectOrbit orbit(0, 63);
	orbit.SetRunning(false);
	orbit.NextStep();
	CHECK_EQ(0, orbit.GetStep());
	CHECK(!orbit.IsRunning());
}

TEST(EffectOrbits, InheritedStepsReplaceTheRandomStartWithoutChangingPauseState)
{
	Paths paths;
	EffectOrbit orbit(2, -1, [] { return 50u; });
	orbit.SetRunning(false);
	orbit.SetStep(16);
	CHECK_EQ(16, orbit.GetStep());
	CHECK(!orbit.IsRunning());
	Position(orbit, 0, 11);
}

TEST(EffectOrbits, PositionReferencesSurviveAnOrbitAdvancingAndBeingDestroyed)
{
	Paths paths;
	const POINT* position;
	{
		EffectOrbit orbit(0, 0);
		position = &orbit.GetPosition();
		orbit.NextStep();
		CHECK_EQ(96, position->x); CHECK_EQ(0, position->y);
	}
	CHECK_EQ(96, position->x); CHECK_EQ(0, position->y);
}

TEST(EffectOrbits, CopiesHaveIndependentStepAndRunningState)
{
	EffectOrbit original(0, 5);
	EffectOrbit copy = original;
	original.SetRunning(false);
	original.SetStep(10);
	copy.Update(true);
	CHECK_EQ(10, original.GetStep());
	CHECK(!original.IsRunning());
	CHECK_EQ(6, copy.GetStep());
	CHECK(copy.IsRunning());
}

TEST(EffectOrbits, AThrowingRandomSourcePropagatesItsFailure)
{
	unsigned draws = 0;
	bool threw = false;
	try
	{
		EffectOrbit orbit(0, -1, [&]() -> unsigned {
			++draws;
			throw std::runtime_error("Random source failed");
		});
	}
	catch (const std::runtime_error&) { threw = true; }
	CHECK(threw);
	CHECK_EQ(1, draws);
}

TEST(EffectOrbits, NegativeStartingStepsOtherThanTheSentinelWrapIntoTheCycle)
{
	struct Case { int step; int expected; };
	const Case cases[]{{-2, 62}, {-63, 1}, {-64, 0}, {-65, 63},
		{(std::numeric_limits<int>::min)(), 0}, {(std::numeric_limits<int>::min)() + 1, 1}};
	unsigned draws = 0;
	for (const auto& example : cases)
	{
		EffectOrbit orbit(0, example.step, [&] { ++draws; return 3u; });
		CHECK_EQ(example.expected, orbit.GetStep());
	}
	CHECK_EQ(0, draws);
}

TEST(EffectOrbits, AssignedStepsNormalizeSignedAndOversizedValues)
{
	struct Case { int step; int expected; };
	const Case cases[]{{-1, 63}, {-2, 62}, {-64, 0}, {-65, 63}, {64, 0}, {65, 1},
		{(std::numeric_limits<int>::max)(), 63}, {(std::numeric_limits<int>::min)(), 0}};
	EffectOrbit orbit(0, 0);
	orbit.SetRunning(false);
	for (const auto& example : cases)
	{
		orbit.SetStep(example.step);
		CHECK_EQ(example.expected, orbit.GetStep());
		CHECK(!orbit.IsRunning());
	}
}

TEST(EffectOrbits, TheLargestAssignedStepCanAdvanceWithoutSignedOverflow)
{
	EffectOrbit orbit(0, 0);
	orbit.SetStep((std::numeric_limits<int>::max)());
	orbit.NextStep();
	CHECK_EQ(0, orbit.GetStep());
}

TEST(EffectOrbits, UnknownTypesProduceNoPositionOffset)
{
	Paths paths;
	for (int type : {-1, 3})
	{
		EffectOrbit orbit(type, 0);
		Position(orbit, 0, 0);
		orbit.Update(true);
		CHECK_EQ(1, orbit.GetStep());
		Position(orbit, 0, 0);
	}
}

TEST(EffectOrbits, ExtremeUnknownTypesKeepTheZeroOffsetAcrossTheCycle)
{
	Paths paths;
	for (int type : {(std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)(), 65536})
	{
		EffectOrbit orbit(type, 0);
		for (int step = 0; step < 64; ++step)
		{
			Position(orbit, 0, 0);
			orbit.NextStep();
		}
	}
}

TEST(EffectOrbits, NormalizedNegativeStepsUseTheCorrespondingPositivePosition)
{
	Paths paths;
	for (int type = 0; type < 3; ++type)
	{
		EffectOrbit orbit(type, -2), expected(type, 62);
		Position(orbit, expected.GetPosition().x, expected.GetPosition().y);
		orbit.SetStep(-1);
		expected.SetStep(63);
		Position(orbit, expected.GetPosition().x, expected.GetPosition().y);
		orbit.NextStep();
		expected.SetStep(0);
		Position(orbit, expected.GetPosition().x, expected.GetPosition().y);
	}
}

TEST(EffectOrbits, SettingTheSentinelDoesNotDrawRandomnessAgain)
{
	unsigned draws = 0;
	EffectOrbit orbit(0, -1, [&] { ++draws; return 7u; });
	orbit.SetStep(-1);
	CHECK_EQ(63, orbit.GetStep());
	CHECK_EQ(1, draws);
	orbit.Update(true);
	CHECK_EQ(0, orbit.GetStep());
	CHECK_EQ(1, draws);
}

TEST(EffectOrbits, ReinitializingPathsKeepsCachedOffsetsAndReferencesStable)
{
	Paths paths;
	std::array<POINT, 3 * 64> previous;
	for (int type = 0; type < 3; ++type)
		for (int step = 0; step < 64; ++step)
			previous[type * 64 + step] = EffectOrbit(type, step).GetPosition();
	EffectOrbit orbit(0, 4);
	const POINT* address = &orbit.GetPosition();
	EffectOrbit::InitializePositions();
	CHECK(address == &orbit.GetPosition());
	for (int type = 0; type < 3; ++type)
		for (int step = 0; step < 64; ++step)
			Position(EffectOrbit(type, step), previous[type * 64 + step].x, previous[type * 64 + step].y);
}
