#include "test_framework.h"
#include "MScreenFade.h"

#include <cstring>
#include <initializer_list>
#include <limits>
#include <new>

// The dedicated targets compile both this fixture and the real implementation
// with the requested char mode. Ordinary unit_tests links gamemodel's object.
#ifdef SCREEN_FADE_EXPECT_UNSIGNED_CHAR
static_assert(std::numeric_limits<char>::is_signed == !SCREEN_FADE_EXPECT_UNSIGNED_CHAR);
#endif

TEST(ScreenFade, FreshStateIsIdleEvenOverNonzeroStorage)
{
	alignas(MScreenFade) unsigned char storage[sizeof(MScreenFade)];
	std::memset(storage, 0xcc, sizeof(storage));
	auto* fade = new (storage) MScreenFade;
	CHECK(!fade->IsActive());
	CHECK_EQ(0, fade->Value());
	fade->Advance(10, true);
	CHECK(!fade->IsActive());
	CHECK_EQ(0, fade->Value());
	// Advancing an idle object must not start the delayed fade's clock.
	fade->Start(31, -1, 1, 4);
	fade->Advance(100, true);
	CHECK_EQ(31, fade->Value());
	fade->~MScreenFade();
}

TEST(ScreenFade, AscendingFadeDrawsItsEndpointBeforeItStops)
{
	MScreenFade fade;
	fade.Start(1, 31, 10);
	for (int expected : {1, 11, 21, 31})
	{
		CHECK(fade.IsActive());
		CHECK_EQ(expected, fade.Value());
		fade.Advance(100, true);
	}
	CHECK(!fade.IsActive());
	CHECK_EQ(41, fade.Value());
	fade.Advance(200, true);
	CHECK_EQ(41, fade.Value());
}

TEST(ScreenFade, DescendingFadeDrawsItsEndpointBeforeItStops)
{
	MScreenFade fade;
	fade.Start(9, 5, 2);
	for (int expected : {9, 7, 5})
	{
		CHECK(fade.IsActive());
		CHECK_EQ(expected, fade.Value());
		fade.Advance(100, true);
	}
	CHECK(!fade.IsActive());
	CHECK_EQ(3, fade.Value());
}

TEST(ScreenFade, NegativeStepMagnitudeUsesTheDirectionOfTheEndpoints)
{
	MScreenFade fade;
	fade.Start(1, 31, -6);
	fade.Advance(100, true);
	CHECK_EQ(7, fade.Value());
	fade.Start(31, 1, -6);
	fade.Advance(101, true);
	CHECK_EQ(25, fade.Value());
}

TEST(ScreenFade, UndelayedFadeAdvancesOnlyOnLogicTicks)
{
	MScreenFade fade;
	fade.Start(25, 31, 2);
	fade.Advance(100, false);
	CHECK_EQ(25, fade.Value());
	fade.Advance(100, true);
	CHECK_EQ(27, fade.Value());
	for (int draw = 0; draw < 10; ++draw)
		fade.Advance(100, false);
	CHECK_EQ(27, fade.Value());
	fade.Advance(101, true);
	CHECK_EQ(29, fade.Value());
	fade.Advance(102, true);
	CHECK_EQ(31, fade.Value());
	CHECK(fade.IsActive());
	fade.Advance(103, true);
	CHECK(!fade.IsActive());
}

TEST(ScreenFade, DelayedFadeStartsItsClockOnTheFirstActiveDraw)
{
	MScreenFade fade;
	fade.Start(31, 1, 1, 4);
	CHECK_EQ(31, fade.Value());
	fade.Advance(1000, true);
	CHECK_EQ(31, fade.Value());
	fade.Advance(1003, true);
	CHECK_EQ(31, fade.Value());
	fade.Advance(1004, false);
	CHECK_EQ(30, fade.Value());
	fade.Advance(1004, true);
	CHECK_EQ(30, fade.Value());
	fade.Advance(1008, false);
	CHECK_EQ(29, fade.Value());
}

TEST(ScreenFade, DelayedFadeTakesOnlyOneStepAfterSkippedFrames)
{
	MScreenFade fade;
	fade.Start(1, 31, 2, 4);
	fade.Advance(100, true);
	fade.Advance(1000, true);
	CHECK_EQ(3, fade.Value());
	fade.Advance(1003, true);
	CHECK_EQ(3, fade.Value());
	fade.Advance(1004, false);
	CHECK_EQ(5, fade.Value());
}

TEST(ScreenFade, GDRFadeReachesOneThenHoldsForMoreThanEightyFrames)
{
	MScreenFade fade;
	fade.Start(31, -1, 1, 4);
	fade.Advance(100, false);
	for (std::uint32_t step = 1; step <= 30; ++step)
	{
		fade.Advance(100 + step * 4 - 1, true);
		CHECK_EQ(32 - step, fade.Value());
		fade.Advance(100 + step * 4, false);
		CHECK_EQ(31 - step, fade.Value());
		CHECK(fade.IsActive());
	}
	for (std::uint32_t held = 0; held <= 80; ++held)
	{
		fade.Advance(220 + held, true);
		CHECK_EQ(1, fade.Value());
		CHECK(fade.IsActive());
	}
	fade.Advance(301, false);
	CHECK(!fade.IsActive());
	CHECK_EQ(1, fade.Value());
}

TEST(ScreenFade, MinusOneEndpointWithoutDelayDoesNotHold)
{
	MScreenFade fade;
	fade.Start(31, -1, 1);
	for (int value = 31; value >= 1; --value)
	{
		CHECK(fade.IsActive());
		CHECK_EQ(value, fade.Value());
		fade.Advance(100, true);
	}
	CHECK(!fade.IsActive());
	CHECK_EQ(0, fade.Value());
}

TEST(ScreenFade, NormalEndpointsAndSkippedShadeOneDoNotEnterTheGDRHold)
{
	MScreenFade ordinary;
	ordinary.Start(2, 1, 1, 4);
	ordinary.Advance(100, true);
	ordinary.Advance(104, true);
	CHECK(ordinary.IsActive());
	CHECK_EQ(1, ordinary.Value());
	ordinary.Advance(108, true);
	CHECK(!ordinary.IsActive());

	MScreenFade skipped;
	skipped.Start(2, -1, 3, 4);
	skipped.Advance(100, true);
	skipped.Advance(104, true);
	CHECK(!skipped.IsActive());
	CHECK_EQ(-1, skipped.Value());
}

TEST(ScreenFade, DelayAndGDRHoldUseWrappingFrameDifferences)
{
	MScreenFade fade;
	fade.Start(2, -1, 1, 4);
	fade.Advance(0xfffffffbU, true);
	fade.Advance(0xfffffffeU, true);
	CHECK_EQ(2, fade.Value());
	fade.Advance(0xffffffffU, true);
	CHECK_EQ(1, fade.Value());
	fade.Advance(79, true); // Eighty frames after 0xffffffff.
	CHECK(fade.IsActive());
	fade.Advance(80, false);
	CHECK(!fade.IsActive());

	MScreenFade step;
	step.Start(10, 1, 1, 4);
	step.Advance(0xfffffffeU, true);
	step.Advance(1, true);
	CHECK_EQ(10, step.Value());
	step.Advance(2, false);
	CHECK_EQ(9, step.Value());
}

TEST(ScreenFade, StopAndRestartRetainThePreviousDelayPhase)
{
	MScreenFade fade;
	fade.Start(31, 1, 1, 4);
	fade.Advance(100, true);
	fade.Advance(104, true);
	CHECK_EQ(30, fade.Value());
	fade.Stop();
	CHECK(!fade.IsActive());
	fade.Advance(200, true);
	CHECK_EQ(30, fade.Value());
	fade.Start(10, 1, 1, 4);
	fade.Advance(201, false);
	CHECK_EQ(9, fade.Value()); // The old phase permits an immediate first step.
	fade.Start(20, 1, 1, 4);
	fade.Advance(202, true);
	CHECK_EQ(20, fade.Value());
	fade.Advance(205, false);
	CHECK_EQ(19, fade.Value());
}

TEST(ScreenFade, SeparateViewsHaveIndependentDelayClocks)
{
	MScreenFade first;
	MScreenFade second;
	first.Start(31, 1, 1, 4);
	first.Advance(100, true);
	second.Start(31, 1, 1, 4);
	second.Advance(103, true);
	first.Advance(104, false);
	second.Advance(104, false);
	CHECK_EQ(30, first.Value());
	CHECK_EQ(31, second.Value());
	second.Advance(107, false);
	CHECK_EQ(30, second.Value());
}

TEST(ScreenFade, ZeroStepKeepsTheDeathShadeUntilStopped)
{
	MScreenFade fade;
	fade.Start(20, 0, 0);
	for (std::uint32_t frame = 0; frame < 100; ++frame)
		fade.Advance(frame, true);
	CHECK(fade.IsActive());
	CHECK_EQ(20, fade.Value());
	fade.Stop();
	CHECK(!fade.IsActive());
}

TEST(ScreenFade, StopsAtTheRendererBoundsEvenWhenTheEndpointIsBeyondThem)
{
	MScreenFade fade;
	fade.Start(30, 40, 2);
	fade.Advance(100, true);
	CHECK(!fade.IsActive());
	CHECK_EQ(32, fade.Value());
	fade.Start(2, -10, 2);
	fade.Advance(101, true);
	CHECK(!fade.IsActive());
	CHECK_EQ(0, fade.Value());
	fade.Start(10, 10, 1);
	fade.Advance(102, false);
	CHECK(fade.IsActive());
	fade.Advance(103, true);
	CHECK(!fade.IsActive());
	CHECK_EQ(9, fade.Value());
}
