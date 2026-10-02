#include "test_framework.h"
#include "ShowTimeChecker.h"

#include <limits>
#include <stdexcept>

namespace {
auto Time(unsigned long long milliseconds) { return MonotonicClock::FromMillis(milliseconds); }
auto Deadline(const ShowTimeChecker& schedule) { return schedule.NextPlayTime.time_since_epoch().count(); }
}

TEST(ShowTime, DefaultsRetainTheAllDayWindowAndImmediateFirstDeadline)
{
	ShowTimeChecker schedule;
	CHECK(!schedule.Loop);
	CHECK_EQ(60000, schedule.MinDelay);
	CHECK_EQ(60000, schedule.MaxDelay);
	CHECK_EQ(0, schedule.StartHour);
	CHECK_EQ(24, schedule.EndHour);
	CHECK_EQ(0, Deadline(schedule));
	for (BYTE hour = 0; hour <= 24; ++hour)
	{
		CHECK(schedule.IsShowHour(hour));
		CHECK(schedule.IsShowTime(Time(0), hour));
	}
	CHECK(!schedule.IsShowHour(25));
	CHECK(!schedule.IsShowHour(255));
}

TEST(ShowTime, DaytimeWindowsIncludeBothEndpoints)
{
	ShowTimeChecker schedule;
	schedule.StartHour = 6;
	schedule.EndHour = 18;
	for (BYTE hour : {0, 5, 19, 23, 24, 255}) CHECK(!schedule.IsShowHour(hour));
	for (BYTE hour : {6, 7, 12, 17, 18}) CHECK(schedule.IsShowHour(hour));
}

TEST(ShowTime, CrossMidnightWindowsIncludeTheEarlyAndLateHours)
{
	ShowTimeChecker schedule;
	schedule.StartHour = 22;
	schedule.EndHour = 3;
	for (BYTE hour : {0, 1, 2, 3, 22, 23, 24, 255}) CHECK(schedule.IsShowHour(hour));
	for (BYTE hour : {4, 5, 12, 20, 21}) CHECK(!schedule.IsShowHour(hour));
}

TEST(ShowTime, EqualHourBoundsAllowOnlyThatHour)
{
	ShowTimeChecker schedule;
	for (BYTE selected : {0, 12, 24, 255})
	{
		schedule.StartHour = schedule.EndHour = selected;
		for (int hour = 0; hour <= 255; ++hour)
			CHECK_EQ(hour == selected, schedule.IsShowHour(static_cast<BYTE>(hour)));
	}
}

TEST(ShowTime, StoredHourBytesAreComparedWithoutModuloTwentyFour)
{
	ShowTimeChecker schedule;
	schedule.StartHour = 250;
	schedule.EndHour = 255;
	for (BYTE hour : {0, 10, 15, 24, 249}) CHECK(!schedule.IsShowHour(hour));
	for (BYTE hour : {250, 251, 254, 255}) CHECK(schedule.IsShowHour(hour));
	schedule.StartHour = 255;
	schedule.EndHour = 0;
	CHECK(schedule.IsShowHour(0));
	CHECK(schedule.IsShowHour(255));
	for (BYTE hour : {1, 23, 24, 254}) CHECK(!schedule.IsShowHour(hour));
}

TEST(ShowTime, MissingGameHourSuppressesPlaybackEvenForLoops)
{
	ShowTimeChecker schedule;
	for (bool loop : {false, true})
	{
		schedule.Loop = loop;
		CHECK(!schedule.IsShowHour(std::nullopt));
		CHECK(!schedule.IsShowTime(Time(100000), std::nullopt));
	}
}

TEST(ShowTime, NonLoopingPlaybackIsDueAtTheExactDeadline)
{
	ShowTimeChecker schedule;
	schedule.NextPlayTime = Time(1000);
	CHECK(!schedule.IsShowTime(Time(999), 12));
	CHECK(schedule.IsShowTime(Time(1000), 12));
	CHECK(schedule.IsShowTime(Time(1001), 12));
	CHECK_EQ(1000, Deadline(schedule));
	CHECK(!schedule.IsShowTime(Time(100), 12));
	CHECK_EQ(1000, Deadline(schedule));
}

TEST(ShowTime, LoopingPlaybackBypassesTheDeadlineButStillRequiresAnAllowedHour)
{
	ShowTimeChecker schedule;
	schedule.Loop = true;
	schedule.StartHour = 6;
	schedule.EndHour = 18;
	schedule.NextPlayTime = Time(100000);
	CHECK(schedule.IsShowTime(Time(0), 6));
	CHECK(schedule.IsShowTime(Time(0), 18));
	CHECK(!schedule.IsShowTime(Time(100001), 5));
	CHECK(!schedule.IsShowTime(Time(100001), 19));
	CHECK_EQ(100000, Deadline(schedule));
}

TEST(ShowTime, AnOverdueNonLoopStillWaitsForItsHourWindow)
{
	ShowTimeChecker schedule;
	schedule.StartHour = 22;
	schedule.EndHour = 3;
	schedule.NextPlayTime = Time(1000);
	CHECK(!schedule.IsShowTime(Time(2000), 21));
	CHECK(schedule.IsShowTime(Time(2001), 22));
	CHECK_EQ(1000, Deadline(schedule));
}

TEST(ShowTime, EqualDelayBoundsUseTheCurrentFrameWithoutDrawingRandomness)
{
	ShowTimeChecker schedule;
	unsigned draws = 0;
	for (DWORD delay : {0u, 1u, 60000u, 0xffffffffu})
	{
		schedule.MinDelay = schedule.MaxDelay = delay;
		schedule.NextPlayTime = Time(100000);
		schedule.SetNextShowTime(Time(5000), [&] { ++draws; return 1u; });
		CHECK_EQ(5000, Deadline(schedule));
		CHECK_EQ(0, draws);
	}
}

TEST(ShowTime, DelaySelectionIncludesTheMinimumAndExcludesTheMaximum)
{
	ShowTimeChecker schedule;
	schedule.MinDelay = 100;
	schedule.MaxDelay = 104;
	const unsigned samples[]{0, 1, 2, 3, 4, 7, 8, 0xffffffffu};
	const unsigned expected[]{100, 101, 102, 103, 100, 103, 100, 103};
	unsigned draws = 0;
	for (unsigned i = 0; i < 8; ++i)
	{
		schedule.SetNextShowTime(Time(5000), [&] { ++draws; return samples[i]; });
		CHECK_EQ(5000 + expected[i], Deadline(schedule));
		CHECK_EQ(i + 1, draws);
	}
}

TEST(ShowTime, AOneMillisecondGapStillDrawsButAlwaysUsesTheMinimum)
{
	ShowTimeChecker schedule;
	schedule.MinDelay = 100;
	schedule.MaxDelay = 101;
	unsigned draws = 0;
	schedule.SetNextShowTime(Time(5000), [&] { ++draws; return 0xffffffffu; });
	CHECK_EQ(5100, Deadline(schedule));
	CHECK_EQ(1, draws);
}

TEST(ShowTime, FullDwordDelayArithmeticCrossesTheOldTickWrapWithoutLosingTheFrame)
{
	ShowTimeChecker schedule;
	schedule.MinDelay = 0;
	schedule.MaxDelay = (std::numeric_limits<DWORD>::max)();
	schedule.SetNextShowTime(Time(0xfffffff0u), [] { return 0xfffffffeu; });
	CHECK_EQ(0x1ffffffeeuLL, Deadline(schedule));
	schedule.MinDelay = 0xfffffffeu;
	schedule.SetNextShowTime(Time(0xfffffff0u), [] { return 123u; });
	CHECK_EQ(0x1ffffffeeuLL, Deadline(schedule));
}

TEST(ShowTime, ReversedDelayBoundsRetainTheLegacyDwordWrap)
{
	// Malformed assets were not normalized by the scheduler. Keep this rule
	// visible while moving it; any change to that policy needs its own fix.
	ShowTimeChecker schedule;
	schedule.MinDelay = 0xffffffffu;
	schedule.MaxDelay = 1;
	schedule.SetNextShowTime(Time(1000), [] { return 0u; });
	CHECK_EQ(0x1000003e7uLL, Deadline(schedule));
	schedule.SetNextShowTime(Time(1000), [] { return 1u; });
	CHECK_EQ(1000, Deadline(schedule));
}

TEST(ShowTime, SchedulingChangesOnlyTheDeadline)
{
	ShowTimeChecker schedule;
	schedule.Loop = true;
	schedule.MinDelay = 100;
	schedule.MaxDelay = 200;
	schedule.StartHour = 22;
	schedule.EndHour = 3;
	schedule.SetNextShowTime(Time(5000), [] { return 99u; });
	CHECK_EQ(5199, Deadline(schedule));
	CHECK(schedule.Loop);
	CHECK_EQ(100, schedule.MinDelay);
	CHECK_EQ(200, schedule.MaxDelay);
	CHECK_EQ(22, schedule.StartHour);
	CHECK_EQ(3, schedule.EndHour);
}

TEST(ShowTime, ThrowingRandomnessPreservesTheExistingDeadline)
{
	ShowTimeChecker schedule;
	schedule.MinDelay = 100;
	schedule.MaxDelay = 200;
	schedule.NextPlayTime = Time(5000);
	bool threw = false;
	try
	{
		schedule.SetNextShowTime(Time(9000), []() -> unsigned { throw std::runtime_error("Random source failed"); });
	}
	catch (const std::runtime_error&) { threw = true; }
	CHECK(threw);
	CHECK_EQ(5000, Deadline(schedule));
	schedule.SetNextShowTime(Time(9000), [] { return 0u; });
	CHECK_EQ(9100, Deadline(schedule));
}

TEST(ShowTime, DefaultRandomnessProducesADeadlineWithinTheConfiguredInterval)
{
	ShowTimeChecker schedule;
	schedule.MinDelay = 100;
	schedule.MaxDelay = 200;
	schedule.SetNextShowTime(Time(5000));
	CHECK(Deadline(schedule) >= 5100);
	CHECK(Deadline(schedule) < 5200);
}

TEST(ShowTime, CopiesKeepIndependentSchedules)
{
	ShowTimeChecker original;
	original.StartHour = 22;
	original.EndHour = 3;
	original.NextPlayTime = Time(5000);
	ShowTimeChecker copy = original;
	original.StartHour = original.EndHour = 12;
	original.NextPlayTime = Time(10000);
	CHECK(copy.IsShowTime(Time(5000), 23));
	CHECK(!original.IsShowTime(Time(5000), 23));
	CHECK_EQ(5000, Deadline(copy));
}
