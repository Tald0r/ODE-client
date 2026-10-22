//----------------------------------------------------------------------
// test_monotonic_clock.cpp
//----------------------------------------------------------------------
//
// MonotonicClock (basic/MonotonicClock.h) - the one clock the timing
// code is being moved onto, and the seam that makes timing testable at
// all (docs/history/cpp17-cpp20-compatibility-assessment-2026-09-04.md,
// modernization backlog priority 5).
//
// Three contracts are pinned here.
//
// The first is the seam itself: with no test source installed the
// adapter is steady_clock and platform_get_ticks(), and with one
// installed it is whatever the test says it is - including after a
// failing check, which is why ScopedTestSource exists and is what these
// tests use.
//
// The second is the epoch decision, and it is the one that would be
// expensive to get wrong. LegacyTicks() is not derived from Now(): on
// the real clock it is platform_get_ticks(), the same value
// timeGetTime() would have returned, so a class with some sites
// converted and some not keeps comparing like with like. The check
// below is what stops a later slice from "simplifying" that into a
// truncation of Now() and silently shifting every unconverted call
// site's epoch. Note that it is compared against platform_get_ticks()
// and not against GetTickCount(): on PLATFORM_WINDOWS those are two
// different counters (kernel32 versus SDL_GetTicks), which is exactly
// the confusion MonotonicClock.h documents.
//
// The third is the reason for the whole exercise: the legacy tick is 32
// bits of milliseconds and wraps every 49.7 days, and the shape the
// client writes its timers in - previous + delay <= now - is true on
// every frame once that sum carries past 2^32. That is demonstrated
// here as arithmetic, on the same numbers the timer test then drives a
// real C_TIMER2 across.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "MonotonicClock.h"

namespace {

//----------------------------------------------------------------------
// The injected clock: one absolute millisecond count the tests move
// wherever they need it, including past 2^32.
//----------------------------------------------------------------------
unsigned long long	g_ull_fake_millisec = 0;

MonotonicClock::TimePoint
FakeNow()
{
	return MonotonicClock::FromMillis(g_ull_fake_millisec);
}

void
SetNow(unsigned long long ull_millisec)
{
	g_ull_fake_millisec = ull_millisec;
}

//----------------------------------------------------------------------
// The wrap point of the legacy 32-bit millisecond tick.
//----------------------------------------------------------------------
const unsigned long long	LEGACY_WRAP = 0x100000000ull;

} // anonymous namespace


//----------------------------------------------------------------------
// The real clock
//----------------------------------------------------------------------

TEST(MonotonicClock, RealClockIsInstalledByDefaultAndDoesNotGoBackwards)
{
	CHECK(MonotonicClock::GetTestSource() == NULL);

	const MonotonicClock::TimePoint tp_first = MonotonicClock::Now();
	const MonotonicClock::TimePoint tp_second = MonotonicClock::Now();

	CHECK(tp_second >= tp_first);
}

TEST(MonotonicClock, LegacyTicksIsTheSameCounterPlatformGetTicksReturns)
{
	// The epoch decision, pinned. LegacyTicks() must stay
	// platform_get_ticks() and not become a truncation of Now(), or
	// every call site not yet converted starts comparing against a
	// different epoch than the ones that are.
	CHECK(MonotonicClock::GetTestSource() == NULL);

	const DWORD dw_adapter = MonotonicClock::LegacyTicks();
	const DWORD dw_platform = platform_get_ticks();

	// Two reads of the same counter a few instructions apart. The bound
	// is deliberately loose: it separates "the same counter" from "a
	// different epoch" (a truncation of steady_clock would differ by
	// hours or more), and a scheduling stall under ASan on a loaded
	// machine must not fail the suite.
	CHECK((DWORD)(dw_platform - dw_adapter) < 60000u);
}


//----------------------------------------------------------------------
// The test seam
//----------------------------------------------------------------------

TEST(MonotonicClock, InjectedSourceReplacesTheClockExactly)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);

	SetNow(1234);
	CHECK_EQ(1234, MonotonicClock::Now().time_since_epoch().count());

	SetNow(1234 + 7);
	CHECK_EQ(1241, MonotonicClock::Now().time_since_epoch().count());
}

TEST(MonotonicClock, ScopedTestSourceRestoresWhatItFound)
{
	CHECK(MonotonicClock::GetTestSource() == NULL);

	{
		MonotonicClock::ScopedTestSource outer(&FakeNow);
		CHECK(MonotonicClock::GetTestSource() == &FakeNow);

		{
			// Restoring "the real clock" has to mean restoring the
			// previous source, not unconditionally clearing it.
			MonotonicClock::ScopedTestSource inner(NULL);
			CHECK(MonotonicClock::GetTestSource() == NULL);
		}

		CHECK(MonotonicClock::GetTestSource() == &FakeNow);
	}

	CHECK(MonotonicClock::GetTestSource() == NULL);
}


//----------------------------------------------------------------------
// The wrap that started all this
//----------------------------------------------------------------------

TEST(MonotonicClock, LegacyTicksFollowsTheInjectedSourceAndWrapsAtThirtyTwoBits)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);

	SetNow(LEGACY_WRAP - 0x100);
	CHECK_EQ(0xFFFFFF00u, MonotonicClock::LegacyTicks());

	// 0x200 ms later the counter has gone round.
	SetNow(LEGACY_WRAP + 0x100);
	CHECK_EQ(0x100u, MonotonicClock::LegacyTicks());
}

TEST(MonotonicClock, TimePointsKeepCountingWhereTheLegacyTickWrapped)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);

	SetNow(LEGACY_WRAP + 500);

	// The tick lost the top bits; the time point did not.
	CHECK_EQ(500u, MonotonicClock::LegacyTicks());
	CHECK_EQ(4294967796LL, MonotonicClock::Now().time_since_epoch().count());
}

TEST(MonotonicClock, TheLegacyAdditionPatternMisfiresAtTheWrapAndTypedDurationsDoNot)
{
	// 256 ms before the wrap, a 512 ms timer, and 16 ms of elapsed time:
	// nothing should fire.
	const DWORD dw_prev  = 0xFFFFFF00u;
	const DWORD dw_delay = 0x200u;
	const DWORD dw_now   = 0xFFFFFF10u;

	// What the client writes today. dw_prev + dw_delay carries past 2^32
	// and lands on 0x100, which is behind dw_now, so the timer fires 496
	// ms early - and keeps firing on every frame from here on.
	CHECK(dw_prev + dw_delay <= dw_now);

	// The wrap-safe spelling of the same test in the same 32-bit
	// arithmetic. Correct, but indistinguishable at a glance from the
	// one above, which is the maintenance problem.
	CHECK(!((DWORD)(dw_now - dw_prev) >= dw_delay));

	// Typed, there is no addition to carry: the difference of two time
	// points is a duration, and it is compared against a duration.
	const MonotonicClock::TimePoint tp_prev =
		MonotonicClock::FromMillis(LEGACY_WRAP - 0x100);
	const MonotonicClock::TimePoint tp_now =
		MonotonicClock::FromMillis(LEGACY_WRAP - 0x100 + 0x10);

	CHECK(!(tp_now - tp_prev >= MonotonicClock::Millis(dw_delay)));
	CHECK_EQ(0x10, (tp_now - tp_prev).count());
}

TEST(MonotonicClock, DurationsSurviveMoreThanTheFortyNineDaysTheTickCovers)
{
	// 2^32 ms is 49.7 days. A DWORD subtraction across that point
	// returns the remainder and reports the elapsed time as 50 ms; the
	// typed difference reports all of it.
	const unsigned long long ull_start = 1000;
	const unsigned long long ull_end   = 1000 + LEGACY_WRAP + 50;

	CHECK_EQ(50u, (DWORD)((DWORD)ull_end - (DWORD)ull_start));

	const MonotonicClock::Duration d_elapsed =
		MonotonicClock::FromMillis(ull_end) - MonotonicClock::FromMillis(ull_start);

	CHECK_EQ(4294967346LL, d_elapsed.count());
}

TEST(MonotonicClock, NowInSecondsFloorsToTheClockSecond)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);

	// The whole-second clock the second-counted deadlines read: 1999 ms
	// is still second 1, 2000 ms is second 2, and a deadline set as now
	// plus 10 s reads 10 s left however far into the second it was set.
	SetNow(1999);
	CHECK_EQ(1LL, MonotonicClock::NowInSeconds().time_since_epoch().count());
	const MonotonicClock::SecondPoint tp_deadline = MonotonicClock::NowInSeconds() + std::chrono::seconds(10);

	SetNow(2000);
	CHECK_EQ(2LL, MonotonicClock::NowInSeconds().time_since_epoch().count());
	CHECK_EQ(9LL, (tp_deadline - MonotonicClock::NowInSeconds()).count());

	SetNow(11999);
	CHECK_EQ(0LL, (tp_deadline - MonotonicClock::NowInSeconds()).count());

	// Past the 32-bit tick's wrap the second count keeps climbing.
	SetNow(LEGACY_WRAP + 1500);
	CHECK_EQ(4294968LL, MonotonicClock::NowInSeconds().time_since_epoch().count());
}
