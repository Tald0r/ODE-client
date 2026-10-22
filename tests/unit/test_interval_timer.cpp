//----------------------------------------------------------------------
// test_interval_timer.cpp
//----------------------------------------------------------------------
//
// MonotonicClock::IntervalTimer, the periodic gate the VS_UI widgets'
// animation, scroll and cursor timers run on (the fourth clocks slice
// of docs/history/cpp17-cpp20-compatibility-assessment-2026-09-04.md, priority
// 5). The gate it replaces was "prev + interval <= GetTickCount()"
// over DWORDs, so beside the firing rule these pin the two things that
// shape lost: the 32-bit wrap, across which it fired early or stalled,
// and the second clock read that let the interval drift.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "MonotonicClock.h"

namespace {

unsigned long long	g_ull_fake_millisec = 0;

// Milliseconds the fake clock moves on EVERY read. Zero for most tests;
// the single-read test sets it, so an implementation that read the
// clock twice would mark its firing later than the read that opened
// the gate, and the test can see the difference.
unsigned long long	g_ull_step_per_read = 0;

MonotonicClock::TimePoint
FakeNow()
{
	const MonotonicClock::TimePoint tp = MonotonicClock::FromMillis(g_ull_fake_millisec);
	g_ull_fake_millisec += g_ull_step_per_read;
	return tp;
}

void
SetNow(unsigned long long ull_millisec)
{
	g_ull_fake_millisec = ull_millisec;
}

const unsigned long long	LEGACY_WRAP = 0x100000000ull;

// The gate the widgets used, over DWORDs, for the comparisons beside
// the assertions.
bool
OldGate(DWORD dw_prev, DWORD dw_millisec, DWORD dw_now)
{
	return (DWORD)(dw_prev + dw_millisec) <= dw_now;
}

} // anonymous namespace

//----------------------------------------------------------------------
// The firing rule
//----------------------------------------------------------------------
TEST(IntervalTimer, FiresOnceTheIntervalHasPassedAndThenRestarts)
{
	MonotonicClock::ScopedTestSource clock(FakeNow);
	SetNow(1000);
	MonotonicClock::IntervalTimer timer;
	timer.SetIntervalMillis(100);
	CHECK_EQ(100, (int)timer.GetInterval().count());

	SetNow(1099);
	CHECK(!timer.Fire());
	CHECK_EQ(99, (int)timer.Elapsed().count());

	// Exactly the interval: open, as "prev + interval <= now" was.
	SetNow(1100);
	CHECK(timer.Fire());
	CHECK_EQ(0, (int)timer.Elapsed().count());

	// The firing restarted the interval from the moment of the firing,
	// not from when the interval had nominally ended.
	SetNow(1150);
	CHECK(!timer.Fire());
	SetNow(1200);
	CHECK(timer.Fire());

	// Late by a lot: one firing, not one per missed interval.
	SetNow(5000);
	CHECK(timer.Fire());
	CHECK(!timer.Fire());
}

TEST(IntervalTimer, RestartMarksNowWithoutFiring)
{
	MonotonicClock::ScopedTestSource clock(FakeNow);
	SetNow(0);
	MonotonicClock::IntervalTimer timer(MonotonicClock::Millis(300));

	SetNow(250);
	timer.Restart();
	SetNow(500);
	CHECK(!timer.Fire());	// 250 since the restart
	SetNow(550);
	CHECK(timer.Fire());
}

// A fresh timer has a zero interval and fires at once, which is what a
// widget whose DWORD members were still zero did.
TEST(IntervalTimer, AFreshTimerFiresAtOnce)
{
	MonotonicClock::ScopedTestSource clock(FakeNow);
	SetNow(7777);
	MonotonicClock::IntervalTimer timer;
	CHECK_EQ(0, (int)timer.GetInterval().count());
	CHECK(timer.Fire());
	CHECK(timer.Fire());
}

// SetInterval mid-flight does not move the last firing.
TEST(IntervalTimer, ChangingTheIntervalKeepsTheLastFiring)
{
	MonotonicClock::ScopedTestSource clock(FakeNow);
	SetNow(0);
	MonotonicClock::IntervalTimer timer(MonotonicClock::Millis(100));
	SetNow(80);
	timer.SetInterval(MonotonicClock::Millis(50));
	CHECK(timer.Fire());	// 80 since the start, over the new 50
}

//----------------------------------------------------------------------
// What the DWORD gate lost
//----------------------------------------------------------------------

// The old gate compared two DWORDs that wrap independently. When the
// sum prev + interval carried past 2^32 while the tick had not, the
// gate was open on every frame until the tick wrapped too - an early
// firing, for up to one interval. When the tick wrapped while the sum
// had not, the gate stayed shut until the tick came round to the sum
// again: 49.7 days, for a widget that was not polled in the window
// between the two. The timer keeps its interval through both.
TEST(IntervalTimer, KeepsItsIntervalAcrossTheLegacyWrap)
{
	MonotonicClock::ScopedTestSource clock(FakeNow);

	// The early firing: prev 10 ms before the wrap, a 100 ms interval.
	const unsigned long long nearWrap = LEGACY_WRAP - 10;
	SetNow(nearWrap);
	MonotonicClock::IntervalTimer timer(MonotonicClock::Millis(100));
	SetNow(nearWrap + 5);
	CHECK(!timer.Fire());
	CHECK(OldGate((DWORD)nearWrap, 100, (DWORD)(nearWrap + 5)));	// 90 <= 0xFFFFFFFB: open at 5 ms
	SetNow(LEGACY_WRAP + 60);	// 70 ms in
	CHECK(!timer.Fire());
	SetNow(LEGACY_WRAP + 90);	// 100 ms in
	CHECK(timer.Fire());

	// The stall: prev 150 ms before the wrap, the sum 50 ms short of it.
	const unsigned long long beforeWrap = LEGACY_WRAP - 150;
	SetNow(beforeWrap);
	timer.Restart();
	SetNow(LEGACY_WRAP + 10);	// 160 ms in
	CHECK(timer.Fire());
	CHECK(!OldGate((DWORD)beforeWrap, 100, (DWORD)(LEGACY_WRAP + 10)));	// 0xFFFFFFCE <= 10: shut
	// ... and shut for the next 49.7 days, until the tick comes round.
	CHECK(!OldGate((DWORD)beforeWrap, 100, (DWORD)(LEGACY_WRAP + 24ull * 60 * 60 * 1000)));
	CHECK(OldGate((DWORD)beforeWrap, 100, (DWORD)(LEGACY_WRAP + LEGACY_WRAP - 50)));
}

// The old shape read the clock twice - once for the gate, once to mark
// the firing - so the work between the reads was added to every
// interval. One read: the firing is marked at the time that opened the
// gate.
TEST(IntervalTimer, MarksTheFiringAtTheTimeThatOpenedTheGate)
{
	MonotonicClock::ScopedTestSource clock(FakeNow);
	SetNow(0);
	MonotonicClock::IntervalTimer timer(MonotonicClock::Millis(100));

	// From here every read of the clock moves it 3 ms, standing in for
	// the work between two reads. Fire() reads once: the firing is marked
	// at 100, the read that opened the gate. A two-read implementation
	// would mark it at 103 and fire next at 203.
	SetNow(100);
	g_ull_step_per_read = 3;
	CHECK(timer.Fire());
	g_ull_step_per_read = 0;
	SetNow(200);
	CHECK(timer.Fire());
	SetNow(300);
	CHECK(timer.Fire());
	CHECK_EQ(0, (int)timer.Elapsed().count());
}

// A backwards test source with a zero interval: Fire() clamps the
// elapsed time as Elapsed() does, so the fresh timer's promise to fire
// at once holds even then.
TEST(IntervalTimer, FireClampsABackwardsClockLikeElapsed)
{
	MonotonicClock::ScopedTestSource clock(FakeNow);
	SetNow(500);
	MonotonicClock::IntervalTimer timer;
	SetNow(400);
	CHECK(timer.Fire());
}

// Expire marks the last firing as the interval ago, so a gate that is
// polled next opens; ExpireBy takes any distance. The chat window's
// help and hide timers and the mine board's double-click guard used
// "prev = GetTickCount() - X" for this.
TEST(IntervalTimer, ExpireOpensTheGateAtTheNextPoll)
{
	MonotonicClock::ScopedTestSource clock(FakeNow);
	SetNow(10000);
	MonotonicClock::IntervalTimer timer(MonotonicClock::Millis(60000));
	CHECK(!timer.Fire());
	timer.Expire();
	CHECK_EQ(60000, (int)timer.Elapsed().count());
	CHECK(timer.Fire());		// and that firing restarts it
	CHECK(!timer.Fire());

	// A strict gate (prev + X < now) over an expired timer opens one
	// millisecond later, exactly as it did over the DWORD idiom.
	timer.Expire();
	CHECK(!(timer.Elapsed() > timer.GetInterval()));
	SetNow(10001);
	CHECK(timer.Elapsed() > timer.GetInterval());

	// ExpireBy with a distance of its own, past the interval.
	timer.ExpireBy(MonotonicClock::Millis(5000));
	CHECK_EQ(5000, (int)timer.Elapsed().count());
	CHECK(!(timer.Elapsed() < MonotonicClock::Millis(500)));
}

// A test source that runs backwards is the one way Elapsed() could go
// negative; it reads as zero instead.
TEST(IntervalTimer, ElapsedIsNeverNegative)
{
	MonotonicClock::ScopedTestSource clock(FakeNow);
	SetNow(500);
	MonotonicClock::IntervalTimer timer(MonotonicClock::Millis(10));
	SetNow(400);
	CHECK_EQ(0, (int)timer.Elapsed().count());
	CHECK(!timer.Fire());
}
