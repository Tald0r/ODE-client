#include "test_framework.h"
#include "EffectTiming.h"

#include <initializer_list>
#include <limits>
#include <cstring>
#include <new>

namespace {
constexpr DWORD LastFrame = (std::numeric_limits<DWORD>::max)();
static_assert(sizeof(DWORD) == 4);
}

TEST(EffectTiming, DefaultLifetimeAndDrawDelayAreAlreadyExpired)
{
	EffectTiming timing;
	CHECK_EQ(0, timing.GetEndFrame());
	CHECK_EQ(0, timing.GetEndLinkFrame());
	for (DWORD now : {DWORD{0}, DWORD{1}, LastFrame})
	{
		CHECK(timing.IsEnd(now));
		CHECK(!timing.IsDelayFrame(now));
	}
}

TEST(EffectTiming, CountUsesTheLegacyMinusOneDeadlineAndInclusiveExpiry)
{
	EffectTiming timing;
	timing.SetCount(100, 10);
	CHECK_EQ(109, timing.GetEndFrame());
	CHECK_EQ(109, timing.GetEndLinkFrame());
	CHECK(!timing.IsEnd(108));
	CHECK(timing.IsEnd(109));
	CHECK(timing.IsEnd(110));
}

TEST(EffectTiming, ExplicitLinkCountsUseTheirOwnDeadline)
{
	EffectTiming timing;
	timing.SetCount(100, 10, 4);
	CHECK_EQ(109, timing.GetEndFrame());
	CHECK_EQ(103, timing.GetEndLinkFrame());
	timing.SetCount(100, 10, 20);
	CHECK_EQ(109, timing.GetEndFrame());
	CHECK_EQ(119, timing.GetEndLinkFrame());
}

TEST(EffectTiming, ZeroAndOneCountsKeepTheExistingDeadlineConvention)
{
	EffectTiming timing;
	timing.SetCount(100, 0, 0);
	CHECK_EQ(99, timing.GetEndFrame());
	CHECK_EQ(99, timing.GetEndLinkFrame());
	CHECK(timing.IsEnd(100));
	timing.SetCount(100, 1, 1);
	CHECK_EQ(100, timing.GetEndFrame());
	CHECK_EQ(100, timing.GetEndLinkFrame());
	CHECK(timing.IsEnd(100));
}

TEST(EffectTiming, TheLinkSentinelFollowsTheCurrentLifetime)
{
	EffectTiming timing;
	timing.SetCount(100, 10, MAX_LINKCOUNT);
	CHECK_EQ(109, timing.GetEndLinkFrame());
	timing.SetCount(300, 30, MAX_LINKCOUNT);
	CHECK_EQ(329, timing.GetEndLinkFrame());
}

TEST(EffectTiming, OrdinaryEffectsTreatTheAttachedSentinelAsAFiniteDuration)
{
	EffectTiming timing;
	timing.SetCount(100, 0xFFFF);
	CHECK_EQ(65634, timing.GetEndFrame());
	CHECK_EQ(65634, timing.GetEndLinkFrame());
}

TEST(EffectTiming, AttachedSentinelDurationUsesTheMaximumFrame)
{
	EffectTiming timing;
	timing.SetAttachedCount(100, 0xFFFF);
	CHECK_EQ(LastFrame, timing.GetEndFrame());
	CHECK_EQ(LastFrame, timing.GetEndLinkFrame());
	CHECK(!timing.IsEnd(LastFrame - 1));
	CHECK(timing.IsEnd(LastFrame));
}

TEST(EffectTiming, InfiniteAttachedEffectsMayHaveAnEarlierExplicitLink)
{
	EffectTiming timing;
	timing.SetAttachedCount(100, 0xFFFF, 4);
	CHECK_EQ(LastFrame, timing.GetEndFrame());
	CHECK_EQ(103, timing.GetEndLinkFrame());
}

TEST(EffectTiming, OtherAttachedDurationsUseOrdinaryCountArithmetic)
{
	for (DWORD duration : {DWORD{0}, DWORD{1}, DWORD{10}, DWORD{65534}, DWORD{65536}, LastFrame})
	{
		EffectTiming attached, ordinary;
		attached.SetAttachedCount(100, duration, 5);
		ordinary.SetCount(100, duration, 5);
		CHECK_EQ(ordinary.GetEndFrame(), attached.GetEndFrame());
		CHECK_EQ(ordinary.GetEndLinkFrame(), attached.GetEndLinkFrame());
	}
}

TEST(EffectTiming, ZeroAtStartupRetainsUnsignedUnderflow)
{
	EffectTiming timing;
	timing.SetCount(0, 0, 0);
	CHECK_EQ(LastFrame, timing.GetEndFrame());
	CHECK_EQ(LastFrame, timing.GetEndLinkFrame());
	CHECK(!timing.IsEnd(0));
	CHECK(timing.IsEnd(LastFrame));
}

TEST(EffectTiming, CountWrapKeepsAbsoluteUnsignedComparisons)
{
	EffectTiming timing;
	timing.SetCount(LastFrame - 2, 10, 3);
	CHECK_EQ(6, timing.GetEndFrame());
	CHECK_EQ(LastFrame, timing.GetEndLinkFrame());
	CHECK(timing.IsEnd(LastFrame - 2));
	CHECK(!timing.IsEnd(5));
	CHECK(timing.IsEnd(6));
}

TEST(EffectTiming, DrawDelayEndsExactlyAtTheDeadlineWithoutSubtractingOne)
{
	EffectTiming timing;
	timing.SetDelayFrame(100, 5);
	CHECK(timing.IsDelayFrame(100));
	CHECK(timing.IsDelayFrame(104));
	CHECK(!timing.IsDelayFrame(105));
	CHECK(!timing.IsDelayFrame(106));
	timing.SetDelayFrame(100, 0);
	CHECK(!timing.IsDelayFrame(100));
	CHECK(timing.IsDelayFrame(99));
}

TEST(EffectTiming, WaitingEndsExactlyAtTheDeadlineWithoutSubtractingOne)
{
	EffectTiming timing;
	timing.SetWaitFrame(100, 5);
	CHECK(timing.IsWaitFrame(100));
	CHECK(timing.IsWaitFrame(104));
	CHECK(!timing.IsWaitFrame(105));
	CHECK(!timing.IsWaitFrame(106));
	timing.SetWaitFrame(100, 0);
	CHECK(!timing.IsWaitFrame(100));
	CHECK(timing.IsWaitFrame(99));
}

TEST(EffectTiming, DelayAndWaitPreserveTheirOwnWrapSemantics)
{
	EffectTiming timing;
	timing.SetDelayFrame(LastFrame - 1, 5);
	timing.SetWaitFrame(LastFrame - 2, 4);
	CHECK(!timing.IsDelayFrame(LastFrame - 1));
	CHECK(timing.IsDelayFrame(2));
	CHECK(!timing.IsDelayFrame(3));
	CHECK(!timing.IsWaitFrame(LastFrame - 2));
	CHECK(timing.IsWaitFrame(0));
	CHECK(!timing.IsWaitFrame(1));
}

TEST(EffectTiming, UpdatingCountsLeavesWaitAndDrawDelayAlone)
{
	EffectTiming timing;
	timing.SetDelayFrame(100, 5);
	timing.SetWaitFrame(100, 10);
	timing.SetCount(200, 20, 4);
	CHECK_EQ(219, timing.GetEndFrame());
	CHECK(timing.IsDelayFrame(104));
	CHECK(!timing.IsDelayFrame(105));
	CHECK(timing.IsWaitFrame(109));
	CHECK(!timing.IsWaitFrame(110));
	timing.SetAttachedCount(300, 0xFFFF);
	CHECK(timing.IsDelayFrame(104));
	CHECK(!timing.IsWaitFrame(110));
}

TEST(EffectTiming, ChangingOneDelayDoesNotChangeAnyOtherDeadline)
{
	EffectTiming timing;
	timing.SetCount(100, 20, 3);
	timing.SetDelayFrame(100, 5);
	timing.SetWaitFrame(100, 10);
	timing.SetDelayFrame(200, 50);
	CHECK_EQ(119, timing.GetEndFrame());
	CHECK_EQ(102, timing.GetEndLinkFrame());
	CHECK(timing.IsWaitFrame(109));
	CHECK(!timing.IsWaitFrame(110));
	timing.SetWaitFrame(300, 60);
	CHECK(timing.IsDelayFrame(249));
	CHECK(!timing.IsDelayFrame(250));
}

TEST(EffectTiming, EveryQueryUsesTheFrameSuppliedByTheCaller)
{
	EffectTiming timing;
	timing.SetCount(100, 10);
	timing.SetDelayFrame(100, 10);
	timing.SetWaitFrame(100, 10);
	CHECK(timing.IsEnd(200));
	CHECK(!timing.IsDelayFrame(200));
	CHECK(!timing.IsWaitFrame(200));
	CHECK(!timing.IsEnd(105));
	CHECK(timing.IsDelayFrame(105));
	CHECK(timing.IsWaitFrame(105));
}

TEST(EffectTiming, CopiesHaveIndependentDeadlines)
{
	EffectTiming first;
	first.SetCount(100, 10);
	first.SetDelayFrame(100, 10);
	first.SetWaitFrame(100, 10);
	EffectTiming second = first;
	first.SetCount(200, 10);
	first.SetDelayFrame(200, 10);
	first.SetWaitFrame(200, 10);
	CHECK_EQ(109, second.GetEndFrame());
	CHECK_EQ(109, second.GetEndLinkFrame());
	CHECK(second.IsEnd(200));
	CHECK(!second.IsDelayFrame(200));
	CHECK(!second.IsWaitFrame(200));
	CHECK(!first.IsEnd(200));
	CHECK(first.IsDelayFrame(200));
	CHECK(first.IsWaitFrame(200));
}

TEST(EffectTiming, FreshObjectsDoNotWaitOnPoisonedStorage)
{
	for (int pattern : {0x01, 0x55, 0xAA, 0xCC, 0xCD, 0xFF})
	{
		alignas(EffectTiming) unsigned char storage[sizeof(EffectTiming)];
		std::memset(storage, pattern, sizeof(storage));
		auto* timing = new (storage) EffectTiming;
		CHECK(!timing->IsWaitFrame(0));
		CHECK(!timing->IsWaitFrame(1));
		timing->~EffectTiming();
	}
}

TEST(EffectTiming, ReusedStorageDoesNotKeepAPreviousWaitDeadline)
{
	alignas(EffectTiming) unsigned char storage[sizeof(EffectTiming)];
	auto* previous = new (storage) EffectTiming;
	previous->SetWaitFrame(100, 100);
	previous->~EffectTiming();
	auto* fresh = new (storage) EffectTiming;
	CHECK(!fresh->IsWaitFrame(0));
	CHECK(!fresh->IsWaitFrame(100));
	fresh->~EffectTiming();
}

TEST(EffectTiming, CountsAndDrawDelayDoNotActivateAnUnscheduledWait)
{
	EffectTiming timing;
	timing.SetCount(100, 10);
	timing.SetDelayFrame(100, 30);
	CHECK(!timing.IsWaitFrame(0));
	CHECK(!timing.IsWaitFrame(100));
	timing.SetAttachedCount(200, 0xFFFF);
	CHECK(!timing.IsWaitFrame(0));
	CHECK(!timing.IsWaitFrame(200));
	CHECK(!timing.IsWaitFrame(LastFrame));
}

TEST(EffectTiming, CopiesOfFreshTimingAcquireTheirWaitsIndependently)
{
	EffectTiming first;
	EffectTiming second = first;
	CHECK(!first.IsWaitFrame(0));
	CHECK(!second.IsWaitFrame(0));
	second.SetWaitFrame(100, 10);
	CHECK(!first.IsWaitFrame(100));
	CHECK(second.IsWaitFrame(100));
	CHECK(!second.IsWaitFrame(110));
}
