//----------------------------------------------------------------------
// test_pet_item_countdown.cpp
//----------------------------------------------------------------------
//
// MPetItem's life countdown (the third clocks slice of
// docs/history/cpp17-cpp20-compatibility-assessment-2026-09-04.md, priority 5).
// A pet's durability is the number of minutes it has left, counted
// down from the moment the durability was set on MonotonicClock's
// clock; the affect check (MCreature::CheckAffectStatus) and the
// description panel (VS_UI_Description.cpp) used to work the countdown
// out for themselves from a DWORD tick, and now ask the item.
//
// The clock is injected, so nothing here sleeps and the 32-bit wrap
// the old arithmetic lived on is one assignment away.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "gamemodel_world.h"
#include "MItem.h"
#include "MonotonicClock.h"

namespace {

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

const unsigned long long	MINUTE = 60 * 1000ull;
const unsigned long long	LEGACY_WRAP = 0x100000000ull;

struct PetWorld : GameModelWorld
{
	MonotonicClock::ScopedTestSource	m_Clock;

	PetWorld()
	: GameModelWorld(1), m_Clock(FakeNow)
	{
		g_pItemTable->InitClass(ITEM_CLASS_PET_ITEM, 1);
		SetNow(0);
	}
};

} // namespace

//----------------------------------------------------------------------
// The countdown
//----------------------------------------------------------------------
TEST(MPetItem, DurabilityCountsDownInWholeMinutesFromWhenItWasSet)
{
	PetWorld w;
	SetNow(5 * MINUTE);
	MPetItem pet;
	pet.SetCurrentDurability(100);

	CHECK_EQ(0, (int)pet.MinutesSinceUpdate().count());
	CHECK_EQ(100u, pet.GetRemainingDurability());

	// One millisecond short of a minute is still no minute.
	SetNow(5 * MINUTE + MINUTE - 1);
	CHECK_EQ(0, (int)pet.MinutesSinceUpdate().count());
	CHECK_EQ(100u, pet.GetRemainingDurability());

	SetNow(5 * MINUTE + MINUTE);
	CHECK_EQ(1, (int)pet.MinutesSinceUpdate().count());
	CHECK_EQ(99u, pet.GetRemainingDurability());

	SetNow(5 * MINUTE + 99 * MINUTE + 59 * 1000);
	CHECK_EQ(1u, pet.GetRemainingDurability());

	// Exactly the durability elapsed: dead, which is what the affect
	// check tests for (the old `durability <= gap`).
	SetNow(5 * MINUTE + 100 * MINUTE);
	CHECK_EQ(0u, pet.GetRemainingDurability());

	// Past it: still zero, never negative or wrapped.
	SetNow(5 * MINUTE + 1000 * MINUTE);
	CHECK_EQ(0u, pet.GetRemainingDurability());
	CHECK_EQ(1000, (int)pet.MinutesSinceUpdate().count());
}

TEST(MPetItem, SettingTheDurabilityRestartsTheCountdown)
{
	PetWorld w;
	MPetItem pet;
	pet.SetCurrentDurability(10);
	SetNow(8 * MINUTE);
	CHECK_EQ(2u, pet.GetRemainingDurability());

	// The server sends a fresh durability: the count starts again from
	// now, not from the first update.
	pet.SetCurrentDurability(10);
	CHECK_EQ(10u, pet.GetRemainingDurability());
	SetNow(8 * MINUTE + 3 * MINUTE);
	CHECK_EQ(7u, pet.GetRemainingDurability());

	// Through the base-class pointer too: SetCurrentDurability is
	// virtual on MItem, and the packet handlers hold an MItem*.
	MItem* pItem = &pet;
	pItem->SetCurrentDurability(4);
	SetNow(8 * MINUTE + 3 * MINUTE + 1 * MINUTE);
	CHECK_EQ(3u, pet.GetRemainingDurability());
}

// A pet whose durability is never set counts from its construction,
// which is what the old constructor's timeGetTime() meant.
TEST(MPetItem, AFreshPetCountsFromItsConstruction)
{
	PetWorld w;
	SetNow(30 * MINUTE);
	MPetItem pet;
	SetNow(30 * MINUTE + 2 * MINUTE + 500);
	CHECK_EQ(2, (int)pet.MinutesSinceUpdate().count());
	// MItem's constructor leaves the durability at whatever the packet
	// will set; with nothing set it is 0 and the pet reads as dead.
	CHECK_EQ(0u, pet.GetRemainingDurability());
}

//----------------------------------------------------------------------
// The clock's width
//----------------------------------------------------------------------

// The old countdown was (timeGetTime() - m_UpdateTime) / 1000 / 60 on
// DWORDs. Across the 32-bit tick's wrap the unsigned subtraction still
// gives the right gap, but once more than 49.7 days have passed it
// wraps and the gap reads small again: a pet set with a long durability
// and left for 50 days read as 0.3 days old. Worked out beside the
// assertion on the same numbers.
TEST(MPetItem, TheCountdownDoesNotWrapAtTheLegacyTick)
{
	PetWorld w;
	const unsigned long long setAt = LEGACY_WRAP - 10 * MINUTE;
	SetNow(setAt);
	MPetItem pet;
	pet.SetCurrentDurability(100000);	// 69.4 days

	// Ten minutes later the tick has wrapped; the old arithmetic coped,
	// because both operands were DWORDs and the subtraction went round.
	SetNow(LEGACY_WRAP + 5 * MINUTE);
	CHECK_EQ(15, (int)pet.MinutesSinceUpdate().count());
	CHECK_EQ(100000u - 15u, pet.GetRemainingDurability());
	{
		const DWORD oldTick = (DWORD)(LEGACY_WRAP + 5 * MINUTE);	// 300,000: past the wrap
		const DWORD oldGap = (DWORD)(oldTick - (DWORD)setAt) / 1000 / 60;
		CHECK_EQ(15u, oldGap);
	}

	// Fifty days later it had not.
	const unsigned long long fiftyDays = 50ull * 24 * 60 * MINUTE;
	SetNow(setAt + fiftyDays);
	CHECK_EQ(50 * 24 * 60, (int)pet.MinutesSinceUpdate().count());
	CHECK_EQ(100000u - 50u * 24 * 60, pet.GetRemainingDurability());
	{
		const DWORD oldTick = (DWORD)(setAt + fiftyDays);
		const DWORD oldGap = (DWORD)(oldTick - (DWORD)setAt) / 1000 / 60;
		CHECK(oldGap < 24 * 60);	// under a day, on the unfixed arithmetic
	}

	// And an elapsed count past the durability's own 32-bit range is
	// simply dead, not wrapped back to life: 2^32 + 100 minutes, which is
	// past INT_MAX minutes too (std::chrono::minutes counts in an int on
	// MSVC, which is why the item counts in its own 64-bit Minutes).
	const unsigned long long farMinutes = 0x100000000ull + 100;
	SetNow(setAt + farMinutes * MINUTE);
	CHECK_EQ((long long)farMinutes, pet.MinutesSinceUpdate().count());
	CHECK_EQ(0u, pet.GetRemainingDurability());
}
