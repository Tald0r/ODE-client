#include "test_framework.h"
#include "MEventQueue.h"
#include "MEventManager.h"

#include <array>
#include <type_traits>
#include <vector>

// The screen manager uses these exact library members, so testing the queue
// exercises the calls made by CGameUpdate, MTopView and packet handlers.
static_assert(std::is_same_v<decltype(&MEventManager::AddEvent),
	void (MEventQueue::*)(MEvent&)>);
static_assert(std::is_same_v<decltype(&MEventManager::ProcessEvent),
	void (MEventQueue::*)()>);
static_assert(std::is_same_v<decltype(&MEventManager::GetEventByFlag),
	const MEvent* (MEventQueue::*)(DWORD, int)>);

namespace {
MonotonicClock::TimePoint now;
MonotonicClock::TimePoint Clock() { return now; }

MEvent Event(EVENT_ID id, DWORD flags = 0, int delay = -1,
	EVENT_TYPE type = EVENTTYPE_NULL)
{
	MEvent event;
	event.eventID = id;
	event.eventFlag = flags;
	event.eventDelay = delay;
	event.eventType = type;
	return event;
}

struct Observations
{
	std::vector<std::array<WORD, 3>> gamma;
	std::vector<DWORD> effectQueries;
	std::vector<std::array<int, 7>> fades;
	std::vector<int> actions; // 0: gamma, 1: effect query, 2: fade.
	bool effectActive = true;
	MEventQueue* queue = nullptr;
};
Observations observed;

void Gamma(WORD red, WORD green, WORD blue)
{
	observed.gamma.push_back({red, green, blue});
	observed.actions.push_back(0);
}

bool EffectActive(DWORD effect)
{
	observed.effectQueries.push_back(effect);
	observed.actions.push_back(1);
	return observed.effectActive;
}

void Fade(signed char start, signed char end, signed char step,
	BYTE red, BYTE green, BYTE blue, WORD delay)
{
	observed.fades.push_back({start, end, step, red, green, blue, delay});
	observed.actions.push_back(2);
	if (observed.queue)
		CHECK(!observed.queue->IsEvent(EVENTID_GDR_PRESENT));
}

const MEventHost host{
	.SetAddGammaRamp = Gamma,
	.HasEffectStatus = EffectActive,
	.SetFadeStart = Fade,
};

struct ClockScope
{
	MonotonicClock::ScopedTestSource source{Clock};

	explicit ClockScope(unsigned long long millis = 1000)
	{
		now = MonotonicClock::FromMillis(millis);
		observed = {};
	}
};
}

TEST(EventQueue, DefaultRecordsAreUntimedAndAlwaysVisible)
{
	ClockScope clock;
	MEvent event;
	CHECK_EQ(EVENTID_NULL, event.eventID);
	CHECK_EQ(EVENTTYPE_NULL, event.eventType);
	CHECK(event.eventStartTickCount == MonotonicClock::TimePoint());
	CHECK_EQ(-1, event.eventDelay);
	CHECK_EQ(-1, event.showTime);
	CHECK_EQ(-1, event.totalTime);
	CHECK_EQ(0, event.eventFlag);
	CHECK_EQ(0, event.parameter1);
	CHECK_EQ(0, event.parameter2);
	CHECK_EQ(0, event.parameter3);
	CHECK_EQ(0, event.parameter4);
	CHECK(event.m_StringsID.empty());
	CHECK(event.IsShowTime());
}

TEST(EventQueue, MissingQueriesAndRemovalsLeaveTheQueueEmpty)
{
	ClockScope clock;
	MEventQueue queue(&host);
	CHECK(queue.IsEmptyEvent());
	CHECK_EQ(0, queue.GetEventCount());
	CHECK(queue.GetEvent(EVENTID_LOGOUT) == nullptr);
	CHECK(!queue.IsEvent(EVENTID_LOGOUT));
	CHECK(queue.GetEventByFlag(EVENTFLAG_DENY_INPUT) == nullptr);
	CHECK_EQ(0, queue.GetEventCountByFlag(EVENTFLAG_DENY_INPUT));
	CHECK(queue.IsEmptyEventByFlag(EVENTFLAG_DENY_INPUT));
	queue.RemoveEvent(EVENTID_LOGOUT);
	queue.RemoveAllEventByType(EVENTTYPE_ZONE);
	queue.RemoveAllEvent();
	CHECK(queue.IsEmptyEvent());
	CHECK(observed.gamma.empty());
}

TEST(EventQueue, InsertionStampsAndOwnsACopyWhileReplacementRestartsTime)
{
	ClockScope clock;
	MEventQueue queue;
	auto event = Event(EVENTID_LOGOUT, EVENTFLAG_SHOW_STRING, 500);
	event.m_StringsID = {11, 22};
	event.parameter1 = 12;
	event.parameter2 = 34;
	event.parameter3 = 56;
	event.parameter4 = 78;
	queue.AddEvent(event);
	CHECK(event.eventStartTickCount == now);
	const auto* saved = queue.GetEvent(EVENTID_LOGOUT);
	CHECK(saved != nullptr);
	if (!saved) return;
	CHECK(saved != &event);
	CHECK(saved->eventStartTickCount == now);
	CHECK_EQ(500, saved->eventDelay);
	CHECK_EQ(12, saved->parameter1);
	CHECK_EQ(34, saved->parameter2);
	CHECK_EQ(56, saved->parameter3);
	CHECK_EQ(78, saved->parameter4);
	event.m_StringsID[0] = 99;
	event.parameter1 = 100;
	CHECK_EQ(11, saved->m_StringsID[0]);
	CHECK_EQ(12, saved->parameter1);
	now += MonotonicClock::Millis(200);
	event.eventDelay = 50;
	queue.AddEvent(event);
	CHECK_EQ(1, queue.GetEventCount());
	CHECK(saved == queue.GetEvent(EVENTID_LOGOUT));
	CHECK_EQ(99, saved->m_StringsID[0]);
	CHECK_EQ(100, saved->parameter1);
	CHECK_EQ(50, saved->eventDelay);
	CHECK_EQ(0, saved->ElapsedMillis());
	CHECK(saved->eventStartTickCount == now);
}

TEST(EventQueue, FlagQueriesMatchAnyBitAndEnumerateInIDOrder)
{
	ClockScope clock;
	MEventQueue queue;
	auto keyboard = Event(EVENTID_LOGOUT, EVENTFLAG_DENY_INPUT_KEYBOARD);
	auto unrelated = Event(EVENTID_PREMIUM_HALF, EVENTFLAG_SHOW_STRING);
	auto mouse = Event(EVENTID_METEOR, EVENTFLAG_DENY_INPUT_MOUSE);
	auto both = Event(EVENTID_WAR_EFFECT, EVENTFLAG_DENY_INPUT);
	for (auto* event : {&keyboard, &unrelated, &mouse, &both}) queue.AddEvent(*event);
	CHECK_EQ(4, queue.GetEventCount());
	CHECK_EQ(3, queue.GetEventCountByFlag(EVENTFLAG_DENY_INPUT));
	CHECK_EQ(2, queue.GetEventCountByFlag(EVENTFLAG_DENY_INPUT_MOUSE));
	CHECK(!queue.IsEmptyEventByFlag(EVENTFLAG_DENY_INPUT));
	CHECK(queue.IsEmptyEventByFlag(EVENTFLAG_FADE_SCREEN));
	CHECK(queue.GetEventByFlag(EVENTFLAG_DENY_INPUT) == queue.GetEvent(EVENTID_METEOR));
	CHECK(queue.GetEventByFlag(EVENTFLAG_DENY_INPUT, 1) == queue.GetEvent(EVENTID_LOGOUT));
	CHECK(queue.GetEventByFlag(EVENTFLAG_DENY_INPUT, 2) == queue.GetEvent(EVENTID_WAR_EFFECT));
	CHECK(queue.GetEventByFlag(EVENTFLAG_DENY_INPUT, 3) == nullptr);
	CHECK(queue.GetEventByFlag(EVENTFLAG_DENY_INPUT, -1) == nullptr);
	CHECK(queue.GetEventByFlag(0) == nullptr);
	CHECK_EQ(0, queue.GetEventCountByFlag(0));
	CHECK(queue.IsEmptyEventByFlag(0));
}

TEST(EventQueue, ZoneRemovalAndClearHandleAdjacentMatchesAndCanRepeat)
{
	ClockScope clock;
	MEventQueue queue;
	auto first = Event(EVENTID_HALLUCINATION, 0, -1, EVENTTYPE_ZONE);
	auto second = Event(EVENTID_KICK_OUT_FROM_ZONE, 0, -1, EVENTTYPE_ZONE);
	auto effect = Event(EVENTID_COMBAT_MASTER, 0, -1, EVENTTYPE_EFFECT);
	auto global = Event(EVENTID_LOGOUT);
	for (auto* event : {&first, &effect, &global, &second}) queue.AddEvent(*event);
	queue.RemoveAllEventByType(EVENTTYPE_ZONE);
	CHECK_EQ(2, queue.GetEventCount());
	CHECK(!queue.IsEvent(first.eventID));
	CHECK(!queue.IsEvent(second.eventID));
	CHECK(queue.IsEvent(effect.eventID));
	CHECK(queue.IsEvent(global.eventID));
	queue.RemoveAllEventByType(EVENTTYPE_ZONE);
	CHECK_EQ(2, queue.GetEventCount());
	queue.RemoveAllEvent();
	queue.RemoveAllEvent();
	CHECK(queue.IsEmptyEvent());
	queue.AddEvent(first);
	CHECK_EQ(1, queue.GetEventCount());
}

TEST(EventQueue, ExpiryIsStrictAndRemovesAllDueEventsAcrossLegacyTickWrap)
{
	ClockScope clock(0xfffffff0ULL);
	MEventQueue queue;
	auto first = Event(EVENTID_HALLUCINATION, 0, 32);
	auto second = Event(EVENTID_KICK_OUT_FROM_ZONE, 0, 32);
	auto later = Event(EVENTID_LOGOUT, 0, 40);
	auto forever = Event(EVENTID_PREMIUM_HALF);
	for (auto* event : {&first, &second, &later, &forever}) queue.AddEvent(*event);
	now += MonotonicClock::Millis(32);
	CHECK_EQ(32, first.ElapsedMillis());
	queue.ProcessEvent();
	CHECK_EQ(4, queue.GetEventCount());
	now += MonotonicClock::Millis(1);
	queue.ProcessEvent();
	CHECK_EQ(2, queue.GetEventCount());
	CHECK(!queue.IsEvent(first.eventID));
	CHECK(!queue.IsEvent(second.eventID));
	CHECK(queue.IsEvent(later.eventID));
	CHECK(queue.IsEvent(forever.eventID));
	now += MonotonicClock::Millis(8);
	queue.ProcessEvent();
	CHECK_EQ(1, queue.GetEventCount());
	CHECK(queue.IsEvent(forever.eventID));
}

TEST(EventQueue, ZeroDelaySurvivesItsStartInstant)
{
	ClockScope clock;
	MEventQueue queue;
	auto event = Event(EVENTID_LOGOUT, 0, 0);
	queue.AddEvent(event);
	queue.ProcessEvent();
	CHECK(queue.IsEvent(event.eventID));
	now += MonotonicClock::Millis(1);
	queue.ProcessEvent();
	CHECK(queue.IsEmptyEvent());
}

TEST(EventQueue, VisibilityUsesTheShowWindowOfEachPeriod)
{
	ClockScope clock(0xfffffff0ULL);
	MEventQueue queue;
	auto event = Event(EVENTID_WAR_EFFECT);
	event.showTime = 2000;
	event.totalTime = 30000;
	queue.AddEvent(event);
	CHECK(event.IsShowTime());
	now += MonotonicClock::Millis(1999);
	CHECK(event.IsShowTime());
	now += MonotonicClock::Millis(1);
	CHECK(!event.IsShowTime());
	now = event.eventStartTickCount + MonotonicClock::Millis(29999);
	CHECK(!event.IsShowTime());
	now += MonotonicClock::Millis(1);
	CHECK(event.IsShowTime());
	now += MonotonicClock::Millis(2000);
	CHECK(!event.IsShowTime());
	event.showTime = 0;
	CHECK(!event.IsShowTime());
	event.showTime = -1;
	CHECK(event.IsShowTime());
}

TEST(EventQueue, EffectQueriesUseCurrentPlayerStateAndOnlyEffectEvents)
{
	ClockScope clock;
	MEventQueue queue(&host);
	auto first = Event(EVENTID_HALLUCINATION, 0, -1, EVENTTYPE_EFFECT);
	auto second = Event(EVENTID_COMBAT_MASTER, 0, -1, EVENTTYPE_EFFECT);
	auto zone = Event(EVENTID_LOGOUT, 0, -1, EVENTTYPE_ZONE);
	first.parameter1 = 17;
	second.parameter1 = 23;
	for (auto* event : {&first, &second, &zone}) queue.AddEvent(*event);
	queue.ProcessEvent();
	CHECK_EQ(3, queue.GetEventCount());
	CHECK(observed.effectQueries == std::vector<DWORD>({17, 23}));
	observed.effectQueries.clear();
	observed.effectActive = false;
	queue.ProcessEvent();
	CHECK(observed.effectQueries == std::vector<DWORD>({17, 23}));
	CHECK_EQ(1, queue.GetEventCount());
	CHECK(queue.IsEvent(zone.eventID));
}

TEST(EventQueue, MissingHostOrEffectCallbackKeepsEffectsUntilTheirExpiry)
{
	ClockScope clock;
	const MEventHost empty{};
	const MEventHost gammaOnly{.SetAddGammaRamp = Gamma};
	for (const MEventHost* services : {static_cast<const MEventHost*>(nullptr), &empty, &gammaOnly})
	{
		MEventQueue queue(services);
		auto timed = Event(EVENTID_HALLUCINATION, 0, 20, EVENTTYPE_EFFECT);
		auto forever = Event(EVENTID_LOGOUT, 0, -1, EVENTTYPE_EFFECT);
		queue.AddEvent(timed);
		queue.AddEvent(forever);
		queue.ProcessEvent();
		CHECK_EQ(2, queue.GetEventCount());
		now += MonotonicClock::Millis(21);
		queue.ProcessEvent();
		CHECK_EQ(1, queue.GetEventCount());
		CHECK(queue.IsEvent(forever.eventID));
	}
	CHECK(observed.effectQueries.empty());
}

TEST(EventQueue, GammaUsesTheFirstFadeAndRefreshesAfterNonFadeChanges)
{
	ClockScope clock;
	MEventQueue queue(&host);
	auto later = Event(EVENTID_METEOR, EVENTFLAG_FADE_SCREEN);
	later.parameter2 = 0xff123456;
	auto first = Event(EVENTID_HALLUCINATION, EVENTFLAG_FADE_SCREEN);
	first.parameter2 = 0xaaabcdef;
	auto plain = Event(EVENTID_LOGOUT);
	queue.AddEvent(plain);
	CHECK(observed.gamma.empty());
	queue.AddEvent(later);
	queue.AddEvent(first);
	queue.AddEvent(plain);
	queue.RemoveEvent(plain.eventID);
	queue.RemoveEvent(first.eventID);
	queue.RemoveEvent(later.eventID);
	queue.RemoveEvent(later.eventID);
	const std::vector<std::array<WORD, 3>> expected{
		{0x12, 0x34, 0x56}, {0xab, 0xcd, 0xef}, {0xab, 0xcd, 0xef},
		{0xab, 0xcd, 0xef}, {0x12, 0x34, 0x56}, {0, 0, 0}
	};
	CHECK(observed.gamma == expected);
}

TEST(EventQueue, TimedGDRExpiryRemovesThenResetsGammaThenStartsSignedFade)
{
	ClockScope clock;
	MEventQueue queue(&host);
	observed.queue = &queue;
	auto event = Event(EVENTID_GDR_PRESENT, EVENTFLAG_FADE_SCREEN, 5000, EVENTTYPE_EFFECT);
	event.parameter1 = 17;
	event.parameter2 = 0x010203;
	queue.AddEvent(event);
	now += MonotonicClock::Millis(5000);
	queue.ProcessEvent();
	CHECK(queue.IsEvent(event.eventID));
	CHECK(observed.fades.empty());
	observed.actions.clear();
	observed.effectQueries.clear();
	now += MonotonicClock::Millis(1);
	queue.ProcessEvent();
	CHECK(queue.IsEmptyEvent());
	CHECK(observed.effectQueries.empty()); // Timeout takes priority over effect removal.
	CHECK(observed.actions == std::vector<int>({0, 2}));
	const std::vector<std::array<int, 7>> expected{{31, -1, 1, 0, 0, 0, 4}};
	CHECK(observed.fades == expected);
	queue.ProcessEvent();
	CHECK_EQ(1, observed.fades.size());
}

TEST(EventQueue, ManualAndEffectRemovalDoNotStartTheGDRFade)
{
	ClockScope clock;
	MEventQueue queue(&host);
	auto event = Event(EVENTID_GDR_PRESENT, 0, 5000, EVENTTYPE_EFFECT);
	queue.AddEvent(event);
	queue.RemoveEvent(event.eventID);
	queue.AddEvent(event);
	queue.RemoveAllEvent();
	queue.AddEvent(event);
	observed.effectActive = false;
	queue.ProcessEvent();
	CHECK(queue.IsEmptyEvent());
	CHECK(observed.fades.empty());
}

TEST(EventQueue, DestructionResetsGammaWithoutTriggeringAnExpiryFade)
{
	ClockScope clock;
	{
		MEventQueue queue(&host);
		auto event = Event(EVENTID_GDR_PRESENT, EVENTFLAG_FADE_SCREEN, 0);
		event.parameter2 = 0x123456;
		queue.AddEvent(event);
		now += MonotonicClock::Millis(1);
	}
	const std::vector<std::array<WORD, 3>> expected{{0x12, 0x34, 0x56}, {0, 0, 0}};
	CHECK(observed.gamma == expected);
	CHECK(observed.fades.empty());
}

TEST(EventQueue, CallbackSetsBelongToEachQueueAndAreReadOnEveryCall)
{
	ClockScope clock;
	MEventHost changing{.SetAddGammaRamp = Gamma};
	MEventQueue withHost(&changing);
	MEventQueue withoutHost;
	auto event = Event(EVENTID_METEOR, EVENTFLAG_FADE_SCREEN);
	withHost.AddEvent(event);
	CHECK_EQ(1, observed.gamma.size());
	withoutHost.AddEvent(event);
	withoutHost.RemoveEvent(event.eventID);
	CHECK_EQ(1, observed.gamma.size());
	changing.SetAddGammaRamp = nullptr;
	withHost.RemoveEvent(event.eventID);
	CHECK_EQ(1, observed.gamma.size());
}
