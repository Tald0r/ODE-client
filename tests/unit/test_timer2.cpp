//----------------------------------------------------------------------
// test_timer2.cpp
//----------------------------------------------------------------------
//
// C_TIMER2 (basic/timer2.h) - the callback timer VS_UI runs its cursor
// blink, its interface blink and its title-screen character update on.
// Its insides moved from two DWORD tick counts to a
// MonotonicClock::TimePoint and a MonotonicClock::Duration
// (docs/history/cpp17-cpp20-compatibility-assessment-2026-09-04.md,
// modernization backlog priority 5); its API did not move at all.
//
// These tests exist because that kind of change is exactly the kind that
// looks equivalent and is not. The firing rule has three parts that a
// rewrite can get wrong independently, and each is pinned separately
// below:
//
//   - a timer fires when the elapsed time REACHES the interval, and not
//     one millisecond before it;
//   - after firing, the reference time becomes the current time of that
//     Execute() call, not previous + interval - so a frame that arrives
//     late costs one fire rather than banking a debt of them;
//   - Add() starts a timer paused, and Continue(), Refresh() and
//     ResetSpeed() each restart the interval from now.
//
// The last two tests are the point of the migration rather than a
// regression guard for it: they drive the clock across the wrap of the
// 32-bit millisecond tick the old implementation used, where the timer
// now keeps its interval and the arithmetic it used to do could not.
//
// The clock is injected (MonotonicClock::ScopedTestSource), so none of
// this sleeps and none of it is timing-dependent.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "MonotonicClock.h"
#include "timer2.h"
#include <limits>

struct Timer2TestAccess {
	static size_t RetainedEntries(const C_TIMER2& timer) {
		return timer.m_timer_queue ? timer.m_timer_queue->size() : 0;
	}
	static bool HasStorage(const C_TIMER2& timer) { return timer.m_timer_queue.has_value(); }
	static void SetNextId(C_TIMER2& timer, timer_id_t next) { timer.m_next_tid = next; }
};

namespace {

//----------------------------------------------------------------------
// The injected clock.
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
// C_TIMER2 callbacks are plain function pointers, so the observation is
// a pair of file-scope counters.
//----------------------------------------------------------------------
int		g_n_fires_a = 0;
int		g_n_fires_b = 0;

void	FireA()		{ g_n_fires_a++; }
void	FireB()		{ g_n_fires_b++; }

void
ResetFires()
{
	g_n_fires_a = 0;
	g_n_fires_b = 0;
}

//----------------------------------------------------------------------
// The wrap point of the 32-bit millisecond tick C_TIMER2 used to run on.
//----------------------------------------------------------------------
const unsigned long long	LEGACY_WRAP = 0x100000000ull;

} // anonymous namespace


//----------------------------------------------------------------------
// The firing boundary
//----------------------------------------------------------------------

TEST(Timer2, FiresWhenTheIntervalIsReachedAndNotOneMillisecondEarlier)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	ResetFires();

	SetNow(1000);

	C_TIMER2 timer;
	timer_id_t tid = timer.Add(100, &FireA);
	CHECK(tid != INVALID_TID);

	timer.Continue(tid);				// reference time is 1000

	SetNow(1099);
	timer.Execute();
	CHECK_EQ(0, g_n_fires_a);

	SetNow(1100);
	timer.Execute();
	CHECK_EQ(1, g_n_fires_a);
}

TEST(Timer2, AddStartsPausedSoNothingFiresBeforeContinue)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	ResetFires();

	SetNow(0);

	C_TIMER2 timer;
	timer_id_t tid = timer.Add(10, &FireA);

	// A whole second of elapsed time against a 10 ms interval.
	SetNow(1000);
	timer.Execute();
	CHECK_EQ(0, g_n_fires_a);

	// Continue() restarts the interval from now, so 1000 is not "990 ms
	// overdue" - it is the new reference time.
	timer.Continue(tid);

	SetNow(1009);
	timer.Execute();
	CHECK_EQ(0, g_n_fires_a);

	SetNow(1010);
	timer.Execute();
	CHECK_EQ(1, g_n_fires_a);
}

TEST(Timer2, ReferenceTimeBecomesTheFiringTimeNotPreviousPlusInterval)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	ResetFires();

	SetNow(0);

	C_TIMER2 timer;
	timer_id_t tid = timer.Add(100, &FireA);
	timer.Continue(tid);				// reference time is 0

	// 250 ms in one step: two and a half intervals. One fire, not two,
	// and the reference time is now 250 rather than 100 or 200.
	SetNow(250);
	timer.Execute();
	CHECK_EQ(1, g_n_fires_a);

	SetNow(349);
	timer.Execute();
	CHECK_EQ(1, g_n_fires_a);

	SetNow(350);
	timer.Execute();
	CHECK_EQ(2, g_n_fires_a);
}


//----------------------------------------------------------------------
// Pause, Continue, Refresh, ResetSpeed
//----------------------------------------------------------------------

TEST(Timer2, PauseStopsTheTimerAndContinueRestartsTheInterval)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	ResetFires();

	SetNow(0);

	C_TIMER2 timer;
	timer_id_t tid = timer.Add(100, &FireA);
	timer.Continue(tid);

	SetNow(50);
	timer.Pause(tid);

	SetNow(1000);
	timer.Execute();
	CHECK_EQ(0, g_n_fires_a);

	timer.Continue(tid);				// reference time is 1000

	SetNow(1099);
	timer.Execute();
	CHECK_EQ(0, g_n_fires_a);

	SetNow(1100);
	timer.Execute();
	CHECK_EQ(1, g_n_fires_a);
}

TEST(Timer2, RefreshRestartsTheIntervalWithoutUnpausing)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	ResetFires();

	SetNow(0);

	C_TIMER2 timer;
	timer_id_t tid = timer.Add(100, &FireA);
	timer.Continue(tid);

	SetNow(99);
	timer.Refresh(tid);					// reference time is 99

	SetNow(198);
	timer.Execute();
	CHECK_EQ(0, g_n_fires_a);

	SetNow(199);
	timer.Execute();
	CHECK_EQ(1, g_n_fires_a);

	// Refresh() on a paused timer leaves it paused.
	timer.Pause(tid);
	timer.Refresh(tid);
	SetNow(1000);
	timer.Execute();
	CHECK_EQ(1, g_n_fires_a);
}

TEST(Timer2, ResetSpeedChangesTheIntervalAndRestartsIt)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	ResetFires();

	SetNow(0);

	C_TIMER2 timer;
	timer_id_t tid = timer.Add(100, &FireA);
	timer.Continue(tid);

	SetNow(60);
	timer.ResetSpeed(tid, 20);			// reference time is 60, interval 20

	SetNow(79);
	timer.Execute();
	CHECK_EQ(0, g_n_fires_a);

	SetNow(80);
	timer.Execute();
	CHECK_EQ(1, g_n_fires_a);
}


//----------------------------------------------------------------------
// Ids
//----------------------------------------------------------------------

TEST(Timer2, DeleteInvalidatesTheIdAndStopsTheTimer)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	ResetFires();

	SetNow(0);

	C_TIMER2 timer;
	timer_id_t tid = timer.Add(10, &FireA);
	timer.Continue(tid);

	const timer_id_t slot = tid;

	CHECK_EQ(true, timer.Delete(tid));
	CHECK_EQ(INVALID_TID, tid);

	SetNow(1000);
	timer.Execute();
	CHECK_EQ(0, g_n_fires_a);

	// The slot stays deleted: a second Delete of the same id is refused
	// and leaves the caller's id alone.
	timer_id_t again = slot;
	CHECK_EQ(false, timer.Delete(again));
	CHECK_EQ(slot, again);
}

TEST(Timer2, OutOfRangeIdsAreIgnoredByEveryEntryPoint)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	ResetFires();

	SetNow(0);

	C_TIMER2 timer;
	timer_id_t tid = timer.Add(100, &FireA);
	timer.Continue(tid);

	// Below the first slot, and past the last one.
	timer_id_t negative = -7;
	timer_id_t past_end = tid + 100;

	CHECK_EQ(false, timer.Delete(negative));
	CHECK_EQ(false, timer.Delete(past_end));
	CHECK_EQ(-7, negative);

	timer.Pause(negative);
	timer.Pause(past_end);
	timer.Continue(negative);
	timer.Continue(past_end);
	timer.Refresh(negative);
	timer.Refresh(past_end);
	timer.ResetSpeed(negative, 5);
	timer.ResetSpeed(past_end, 5);

	// The live timer was not disturbed by any of that.
	SetNow(100);
	timer.Execute();
	CHECK_EQ(1, g_n_fires_a);
}

TEST(Timer2, DeletedTimersDoNotAccumulateBehindALiveTimer)
{
	C_TIMER2 timer;
	CHECK(!Timer2TestAccess::HasStorage(timer));
	timer_id_t survivor = timer.Add(100, &FireA);
	CHECK(survivor != INVALID_TID);
	for (int i = 0; i < 2048; ++i) {
		timer_id_t temporary = timer.Add(100, &FireB);
		CHECK(temporary != INVALID_TID);
		CHECK(timer.Delete(temporary));
	}
	CHECK_EQ(1, Timer2TestAccess::RetainedEntries(timer));
	CHECK(timer.Delete(survivor));
	CHECK_EQ(0, Timer2TestAccess::RetainedEntries(timer));
	CHECK(!Timer2TestAccess::HasStorage(timer));
}

TEST(Timer2, ADeletedIdCannotChangeATimerAddedLater)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	ResetFires();
	SetNow(0);
	C_TIMER2 timer;
	timer_id_t first = timer.Add(100, &FireA);
	const timer_id_t stale = first;
	CHECK(timer.Delete(first));
	timer_id_t second = timer.Add(100, &FireB);
	CHECK(second != stale);
	timer.Continue(second);
	SetNow(99);
	timer_id_t staleCopy = stale;
	CHECK(!timer.Delete(staleCopy));
	timer.Pause(stale); timer.ResetSpeed(stale, 1000);
	timer.Continue(stale); timer.Refresh(stale);
	SetNow(100);
	timer.Execute();
	CHECK_EQ(0, g_n_fires_a);
	CHECK_EQ(1, g_n_fires_b);
}

TEST(Timer2, IdExhaustionFailsWithoutWrappingOrReusingDeletedIds)
{
	C_TIMER2 timer;
	const timer_id_t maximum = (std::numeric_limits<timer_id_t>::max)();
	Timer2TestAccess::SetNextId(timer, maximum - 1);
	timer_id_t penultimate = timer.Add(100, &FireA);
	timer_id_t last = timer.Add(100, &FireB);
	CHECK_EQ(maximum - 1, penultimate); CHECK_EQ(maximum, last);
	CHECK_EQ(INVALID_TID, timer.Add(100, &FireB));
	CHECK(timer.Delete(last)); CHECK(timer.Delete(penultimate));
	CHECK_EQ(INVALID_TID, timer.Add(100, &FireB));
	CHECK_EQ(0, Timer2TestAccess::RetainedEntries(timer));
}

TEST(Timer2, TimersAreIndependentOfEachOther)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	ResetFires();

	SetNow(0);

	C_TIMER2 timer;
	timer_id_t tid_a = timer.Add(100, &FireA);
	timer_id_t tid_b = timer.Add(250, &FireB);

	CHECK(tid_a != tid_b);

	timer.Continue(tid_a);
	timer.Continue(tid_b);

	SetNow(100);
	timer.Execute();
	CHECK_EQ(1, g_n_fires_a);
	CHECK_EQ(0, g_n_fires_b);

	SetNow(250);
	timer.Execute();
	CHECK_EQ(2, g_n_fires_a);
	CHECK_EQ(1, g_n_fires_b);
}

TEST(Timer2, ATimerWithNoCallbackStillKeepsItsInterval)
{
	// fp_proc == NULL was always accepted and the reference time was
	// still advanced; nothing in the tree relies on it, but a rewrite
	// that started dereferencing it would crash rather than fail.
	MonotonicClock::ScopedTestSource guard(&FakeNow);

	SetNow(0);

	C_TIMER2 timer;
	timer_id_t tid = timer.Add(100, NULL);
	CHECK(tid != INVALID_TID);

	timer.Continue(tid);

	SetNow(1000);
	timer.Execute();

	// Still addressable and still live afterwards.
	timer.Refresh(tid);
	CHECK_EQ(true, timer.Delete(tid));
}


//----------------------------------------------------------------------
// The wrap, which is what the migration was for
//----------------------------------------------------------------------

TEST(Timer2, KeepsItsIntervalAcrossTheWrapOfTheLegacyThirtyTwoBitTick)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	ResetFires();

	// 256 ms before the 32-bit millisecond tick goes round, with a 512
	// ms timer: the fire is on the far side of the wrap.
	const unsigned long long ull_start = LEGACY_WRAP - 0x100;

	SetNow(ull_start);
	CHECK_EQ(0xFFFFFF00u, MonotonicClock::LegacyTicks());

	C_TIMER2 timer;
	timer_id_t tid = timer.Add(0x200, &FireA);
	timer.Continue(tid);

	// 16 ms in. This is where the old "previous + interval <= now"
	// arithmetic went wrong: 0xFFFFFF00 + 0x200 wraps to 0x100, which is
	// behind the current tick, so the timer would have fired here - 496
	// ms early - and then on every frame afterwards.
	const DWORD dw_legacy_prev = 0xFFFFFF00u;
	const DWORD dw_legacy_interval = 0x200u;

	SetNow(ull_start + 0x10);
	CHECK(dw_legacy_prev + dw_legacy_interval <= MonotonicClock::LegacyTicks());
	timer.Execute();
	CHECK_EQ(0, g_n_fires_a);

	// One millisecond short of the interval, with the legacy tick now
	// past its wrap and reading a very small number.
	SetNow(ull_start + 0x1FF);
	CHECK(MonotonicClock::LegacyTicks() < 0x200u);
	timer.Execute();
	CHECK_EQ(0, g_n_fires_a);

	// And exactly on the interval.
	SetNow(ull_start + 0x200);
	timer.Execute();
	CHECK_EQ(1, g_n_fires_a);
}

TEST(Timer2, DoesNotLoseAnIntervalLongerThanTheLegacyTickCanCount)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	ResetFires();

	SetNow(1000);

	C_TIMER2 timer;
	timer_id_t tid = timer.Add(100, &FireA);
	timer.Continue(tid);				// reference time is 1000

	// 49.7 days and 50 ms later. In 32-bit tick arithmetic the elapsed
	// time reads as 50 ms - the whole 2^32 is gone - so the old
	// subtraction form would have reported "not yet" for a timer that is
	// four billion milliseconds overdue.
	const unsigned long long ull_late = 1000 + LEGACY_WRAP + 50;

	CHECK_EQ(50u, (DWORD)((DWORD)ull_late - (DWORD)1000u));

	SetNow(ull_late);
	timer.Execute();
	CHECK_EQ(1, g_n_fires_a);

	// One fire, and the reference time is the time of that fire, so the
	// next one is a full interval away and not immediate.
	SetNow(ull_late + 99);
	timer.Execute();
	CHECK_EQ(1, g_n_fires_a);

	SetNow(ull_late + 100);
	timer.Execute();
	CHECK_EQ(2, g_n_fires_a);
}


//----------------------------------------------------------------------
// Re-entrancy
//----------------------------------------------------------------------
//
// The one thing the vector rewrite changed underneath the API: a
// callback that calls back into the manager. Execute() re-reads the
// queue size each iteration and re-addresses the firing unit by index
// after the callback, because an Add() inside the callback can move
// the whole queue. No shipped callback does this today; these pin the
// contract so that one can.
//----------------------------------------------------------------------

namespace {

C_TIMER2*	g_p_reentrant_timer = NULL;
timer_id_t	g_tid_other = INVALID_TID;
int		g_n_added = 0;

// Adds enough timers to force the queue to reallocate under the caller.
void
AddManyDuringFire()
{
	g_n_fires_a++;

	for (int i = 0; i < 64; i++)
	{
		if (g_p_reentrant_timer->Add(100, &FireB) != INVALID_TID)
		{
			g_n_added++;
		}
	}
}

void
DeleteOtherDuringFire()
{
	g_n_fires_a++;
	g_p_reentrant_timer->Delete(g_tid_other);
}

timer_id_t g_tid_self = INVALID_TID;
void DeleteSelfAndReplaceDuringFire()
{
	g_n_fires_a++;
	CHECK(g_p_reentrant_timer->Delete(g_tid_self));
	g_tid_other = g_p_reentrant_timer->Add(100, &FireB);
	g_p_reentrant_timer->Continue(g_tid_other);
}

} // anonymous namespace

TEST(Timer2, ACallbackMayAddTimersAndTheyStartPausedInThatSamePass)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	ResetFires();
	g_n_added = 0;

	SetNow(1000);

	C_TIMER2 timer;
	g_p_reentrant_timer = &timer;

	timer_id_t tid = timer.Add(100, &AddManyDuringFire);
	timer.Continue(tid);				// reference time is 1000

	SetNow(1100);
	timer.Execute();

	CHECK_EQ(1, g_n_fires_a);
	CHECK_EQ(64, g_n_added);
	CHECK_EQ(0, g_n_fires_b);			// added paused, reached, skipped

	// The firing timer's reference time was written after the queue
	// moved: it is the time of that Execute(), not stale memory.
	SetNow(1199);
	timer.Execute();
	CHECK_EQ(1, g_n_fires_a);

	// The ids handed out inside the callback are live.
	for (timer_id_t tid_added = 1; tid_added <= 64; tid_added++)
	{
		timer.Continue(tid_added);		// reference time is 1199
	}

	SetNow(1299);
	timer.Execute();
	CHECK_EQ(2, g_n_fires_a);			// 1100 + 100 + 99: fired at 1200
	CHECK_EQ(64, g_n_fires_b);			// 1199 + 100 reached at 1299

	g_p_reentrant_timer = NULL;
}

TEST(Timer2, ACallbackMayDeleteAnotherTimerBeforeItFiresInThatPass)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	ResetFires();

	SetNow(1000);

	C_TIMER2 timer;
	g_p_reentrant_timer = &timer;

	timer_id_t tid_first = timer.Add(100, &DeleteOtherDuringFire);
	g_tid_other = timer.Add(100, &FireB);
	timer.Continue(tid_first);
	timer.Continue(g_tid_other);

	SetNow(1100);
	timer.Execute();

	// The first timer fired and deleted the second before the pass
	// reached it, so the second never fired and its id is gone.
	CHECK_EQ(1, g_n_fires_a);
	CHECK_EQ(0, g_n_fires_b);
	CHECK_EQ(INVALID_TID, g_tid_other);

	timer_id_t tid_stale = 1;
	CHECK_EQ(false, timer.Delete(tid_stale));

	// The survivor keeps its interval.
	SetNow(1200);
	timer.Execute();
	CHECK_EQ(2, g_n_fires_a);
	CHECK_EQ(0, g_n_fires_b);

	g_p_reentrant_timer = NULL;
}

TEST(Timer2, ACallbackMayDeleteItselfAndAddAReplacement)
{
	MonotonicClock::ScopedTestSource guard(&FakeNow);
	ResetFires();
	SetNow(0);
	C_TIMER2 timer;
	g_p_reentrant_timer = &timer;
	g_tid_self = timer.Add(100, &DeleteSelfAndReplaceDuringFire);
	timer.Continue(g_tid_self);
	SetNow(100);
	timer.Execute();
	CHECK_EQ(INVALID_TID, g_tid_self);
	CHECK_EQ(1, g_n_fires_a); CHECK_EQ(0, g_n_fires_b);
	CHECK_EQ(1, Timer2TestAccess::RetainedEntries(timer));
	SetNow(199);
	timer.Execute();
	CHECK_EQ(0, g_n_fires_b);
	SetNow(200);
	timer.Execute();
	CHECK_EQ(1, g_n_fires_a); CHECK_EQ(1, g_n_fires_b);
	g_p_reentrant_timer = NULL;
}
