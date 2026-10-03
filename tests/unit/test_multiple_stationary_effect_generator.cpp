#include "test_framework.h"
#include "MStopZoneMultipleEffectGenerator.h"
#include "MEffect.h"
#include "MEventQueue.h"
#include "SkillDef.h"

#include <array>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MStopMultipleEffectSprite sprite;
bool spriteAvailable, acceptQueue;
int requestedSprite, submissions, rejectBefore;
std::vector<int> calls, slots, removedTargets, observedRandom;
std::vector<std::unique_ptr<MEffect>> effects;
std::vector<MEvent> events;
MEventQueue* eventQueue;
struct Point
{
	int x, y;
	bool operator==(const Point&) const = default;
};
std::vector<Point> attempted;
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(4); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE frame) { calls.push_back(3); return 7 + frame; },
};
const MEffectTargetHost targetHost{.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); }};
const MStopMultipleEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MStopMultipleEffectSprite& result) {
		calls.push_back(1); requestedSprite = type; result = sprite; return spriteAvailable;
	},
	.AddEvent = [](MEvent& event) { calls.push_back(2); events.push_back(event); },
	.Queue = [](std::unique_ptr<MEffect> effect) {
		calls.push_back(5); attempted.push_back({effect->GetPixelX(), effect->GetPixelY()});
		CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		const int slot = submissions++;
		if (!acceptQueue || slot < rejectBefore) return false;
		slots.push_back(slot); effects.push_back(std::move(effect)); return true;
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MStopMultipleEffectHost* previousMultiple = MStopZoneMultipleEffectGenerator::SetHost(&host);
	MStopZoneMultipleEffectGenerator generator;
	World()
	{
		frameNow = 100; sprite = {BLT_EFFECT, 12, 3}; spriteAvailable = acceptQueue = true;
		requestedSprite = -1; submissions = rejectBefore = 0; eventQueue = nullptr;
		effects.clear(); calls.clear(); slots.clear(); removedTargets.clear(); attempted.clear(); events.clear(); observedRandom.clear();
	}
	~World()
	{
		effects.clear(); MStopZoneMultipleEffectGenerator::SetHost(previousMultiple);
		MEffectTarget::SetHost(previousTarget); MEffect::SetHost(previousEffect);
	}
};

EFFECTGENERATOR_INFO Info()
{
	EFFECTGENERATOR_INFO info{};
	info.nActionInfo = 42; info.effectSpriteType = 17;
	info.x0 = 501; info.y0 = 250; info.z0 = 17;
	info.x1 = 900; info.y1 = 800; info.z1 = 99;
	info.direction = DIRECTION_RIGHTUP; info.step = 9; info.count = 30; info.linkCount = 5;
	info.power = 2; info.creatureID = 123;
	return info;
}

std::unique_ptr<MEffectTarget> Target(BYTE id = 73)
{
	auto target = std::make_unique<MEffectTarget>(3);
	target->NextPhase(); target->m_EffectID = id; target->Set(777, 888, 999, 456);
	target->SetServerID(789); target->SetDelayFrame(31); target->SetResultTime();
	target->SetResult(new MActionResult); return target;
}

void ClearEffects()
{
	effects.clear(); calls.clear(); slots.clear(); attempted.clear(); events.clear(); observedRandom.clear(); submissions = 0;
}

std::vector<int> RandomDraws(unsigned seed, size_t count)
{
	std::srand(seed); std::vector<int> values;
	for (size_t i = 0; i < count; ++i) values.push_back(std::rand());
	std::srand(seed); return values;
}

} // namespace

TEST(MultipleStationaryEffectGenerator, SchedulesTheShakeBeforeConfiguringSixteenRealEffects)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_STOP_ZONE_MULTIPLE, world.generator.GetID());
	CHECK(world.generator.Generate(Info())); CHECK_EQ(16, effects.size()); CHECK_EQ(17, requestedSprite); CHECK_EQ(1, events.size());
	const auto& event = events.front(); CHECK_EQ(EVENTID_METEOR_SHAKE, event.eventID); CHECK_EQ(EVENTTYPE_ZONE, event.eventType);
	CHECK_EQ(2300, event.eventDelay); CHECK_EQ(EVENTFLAG_SHAKE_SCREEN, event.eventFlag); CHECK_EQ(1, event.parameter3);
	CHECK_EQ(-1, event.showTime); CHECK_EQ(-1, event.totalTime); CHECK_EQ(0, event.parameter1); CHECK_EQ(0, event.parameter2); CHECK_EQ(0, event.parameter4); CHECK(event.m_StringsID.empty());
	std::vector<int> expected{1, 2}; for (int i = 0; i < 16; ++i) expected.insert(expected.end(), {4, 3, 4, 5}); CHECK(calls == expected);
	for (size_t i = 0; i < effects.size(); ++i)
	{
		auto& effect = *effects[i]; CHECK_EQ(MEffect::EFFECT_SECTOR, effect.GetEffectType());
		CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame());
		CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight()); CHECK_EQ(17, effect.GetPixelZ());
		CHECK_EQ(0, effect.GetStepPixel()); CHECK_EQ(DIRECTION_RIGHTUP, effect.GetDirection()); CHECK_EQ(2, effect.GetPower());
		CHECK_EQ(129 + 2 * i, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame()); CHECK_EQ(42, effect.GetActionInfo());
		CHECK(effect.GetEffectTarget() == nullptr); CHECK(effect.IsMulti()); CHECK(!effect.IsWaitFrame());
		frameNow = 100; CHECK_EQ(i != 0, effect.IsDelayFrame()); frameNow = 100 + static_cast<DWORD>(2 * i); CHECK(!effect.IsDelayFrame());
	}
}

TEST(MultipleStationaryEffectGenerator, OrdinaryPhasesKeepTheirFourQuadrantsAndDrawOrder)
{
	World world;
	const auto draws = RandomDraws(83, 33);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(16, attempted.size()); CHECK_EQ(draws[32], std::rand());
	const Point first[] = {{477 - draws[0] % 48, 250 - draws[1] % 50}, {477 - draws[2] % 48, 250 + draws[3] % 50},
		{525 + draws[4] % 48, 250 - draws[5] % 50}, {525 + draws[6] % 48, 250 + draws[7] % 50}};
	const Point last[] = {{477 - draws[24] % 48, 250 - draws[25] % 50}, {477 - draws[26] % 48, 250 + draws[27] % 50},
		{525 + draws[28] % 48, 250 - draws[29] % 50}, {525 + draws[30] % 48, 250 + draws[31] % 50}};
	for (size_t i = 0; i < 4; ++i) { CHECK(attempted[i] == first[i]); CHECK(attempted[12 + i] == last[i]); }
	for (size_t i = 0; i < attempted.size(); ++i)
	{
		const auto p = attempted[i];
		CHECK(i % 4 < 2 ? p.x >= 430 && p.x <= 477 : p.x >= 525 && p.x <= 572);
		CHECK(i % 2 == 0 ? p.y >= 201 && p.y <= 250 : p.y >= 250 && p.y <= 299);
	}
}

TEST(MultipleStationaryEffectGenerator, WideStormsEmitSixPhasesWithWiderOffsets)
{
	World world;
	for (const TYPE_ACTIONINFO action : {static_cast<TYPE_ACTIONINFO>(SKILL_ACID_STORM_WIDE), static_cast<TYPE_ACTIONINFO>(SKILL_POISON_STORM_WIDE)})
	{
		ClearEffects(); auto info = Info(); info.nActionInfo = action; const auto draws = RandomDraws(109, 49);
		CHECK(world.generator.Generate(info)); CHECK_EQ(24, effects.size()); CHECK_EQ(draws[48], std::rand()); CHECK_EQ(1, events.size());
		CHECK((attempted.front() == Point{477 - draws[0] % 96, 250 - draws[1] % 100}));
		CHECK((attempted.back() == Point{525 + draws[46] % 96, 250 + draws[47] % 100}));
		for (size_t i = 0; i < attempted.size(); ++i)
		{
			const auto p = attempted[i]; CHECK(i % 4 < 2 ? p.x >= 382 && p.x <= 477 : p.x >= 525 && p.x <= 620);
			CHECK(i % 2 == 0 ? p.y >= 151 && p.y <= 250 : p.y >= 250 && p.y <= 349);
			CHECK_EQ(129 + 2 * i, effects[i]->GetEndFrame()); CHECK_EQ(104, effects[i]->GetEndLinkFrame()); CHECK_EQ(action, effects[i]->GetActionInfo());
		}
	}
}

TEST(MultipleStationaryEffectGenerator, NearbyActionsAndPowerDoNotSelectWideStorms)
{
	World world;
	for (const TYPE_ACTIONINFO action : {static_cast<TYPE_ACTIONINFO>(SKILL_POISON_STORM_WIDE - 1), static_cast<TYPE_ACTIONINFO>(SKILL_ACID_STORM_WIDE + 1)})
	{
		for (const BYTE power : {static_cast<BYTE>(0), static_cast<BYTE>(255)})
		{
			ClearEffects(); auto info = Info(); info.nActionInfo = action; info.power = power; info.step = 0;
			CHECK(world.generator.Generate(info)); CHECK_EQ(16, effects.size());
			for (const auto& effect : effects) { CHECK_EQ(power, effect->GetPower()); CHECK_EQ(0, effect->GetStepPixel()); }
		}
	}
}

TEST(MultipleStationaryEffectGenerator, EachPhaseDrawsAllPositionsBeforeItsFirstQueueCallback)
{
	World world;
	const MStopMultipleEffectHost consumingRandom{
		.Sprite = host.Sprite, .AddEvent = host.AddEvent,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			if (submissions == 0) observedRandom.push_back(std::rand());
			return host.Queue(std::move(effect));
		},
	};
	MStopZoneMultipleEffectGenerator::SetHost(&consumingRandom);
	const auto draws = RandomDraws(137, 34);
	CHECK(world.generator.Generate(Info())); CHECK(observedRandom == std::vector<int>({draws[8]}));
	CHECK((attempted[1] == Point{477 - draws[2] % 48, 250 + draws[3] % 50}));
	CHECK((attempted[4] == Point{477 - draws[9] % 48, 250 - draws[10] % 50})); CHECK_EQ(draws[33], std::rand());
}

TEST(MultipleStationaryEffectGenerator, RejectionStillSchedulesTheEventAndConsumesEveryPositionDraw)
{
	World world;
	acceptQueue = false; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	const auto draws = RandomDraws(163, 33);
	CHECK(!world.generator.Generate(info)); CHECK_EQ(16, submissions); CHECK(effects.empty()); CHECK_EQ(1, events.size());
	CHECK_EQ(draws[32], std::rand()); CHECK_EQ(777, target->GetX()); CHECK_EQ(888, target->GetY()); CHECK(removedTargets.empty());
}

TEST(MultipleStationaryEffectGenerator, CopiesKeepPhaseAndMetadataButUseGeneratedSourcePixels)
{
	World world;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release();
	for (size_t i = 0; i < effects.size(); ++i)
	{
		const auto* linked = effects[i]->GetEffectTarget(); CHECK(linked != nullptr); if (!linked) continue;
		CHECK_EQ(i == 0, linked == info.pEffectTarget); CHECK_EQ(1, linked->GetCurrentPhase()); CHECK_EQ(3, linked->GetMaxPhase());
		CHECK_EQ(31, linked->GetDelayFrame()); CHECK_EQ(73, linked->GetEffectID());
		CHECK_EQ(i == 0, linked->IsExistResult()); CHECK_EQ(i == 0, linked->IsResultTime()); CHECK_EQ(i == 0 ? 789 : OBJECTID_NULL, linked->GetServerID());
		CHECK_EQ(i == 0 ? 777 : attempted[i].x, linked->GetX()); CHECK_EQ(i == 0 ? 888 : attempted[i].y, linked->GetY());
		CHECK_EQ(i == 0 ? 999 : 17, linked->GetZ()); CHECK_EQ(i == 0 ? 456 : 123, linked->GetID());
		for (size_t j = 0; j < i; ++j) CHECK(linked != effects[j]->GetEffectTarget());
	}
}

TEST(MultipleStationaryEffectGenerator, EveryFirstAcceptedSlotReceivesTheOriginalAndKeepsItsDelay)
{
	World world;
	struct Case { TYPE_ACTIONINFO action; int count; };
	for (const Case c : {Case{42, 16}, {SKILL_ACID_STORM_WIDE, 24}, {SKILL_POISON_STORM_WIDE, 24}})
	{
		for (int first = 0; first <= c.count; ++first)
		{
			ClearEffects(); frameNow = 100; rejectBefore = first;
			auto target = Target(); auto info = Info(); info.nActionInfo = c.action; info.pEffectTarget = target.get();
			CHECK_EQ(first < c.count, world.generator.Generate(info)); CHECK_EQ(c.count, submissions); CHECK_EQ(c.count - first, effects.size());
			bool originalOwned = false;
			for (size_t i = 0; i < effects.size(); ++i)
			{
				const auto* linked = effects[i]->GetEffectTarget(); CHECK(linked != nullptr); if (!linked) continue;
				originalOwned |= linked == target.get(); CHECK_EQ(i == 0, linked == target.get());
				CHECK_EQ(i == 0 ? 777 : attempted[slots[i]].x, linked->GetX()); CHECK_EQ(i == 0 ? 888 : attempted[slots[i]].y, linked->GetY());
				CHECK_EQ(129 + 2 * slots[i], effects[i]->GetEndFrame()); CHECK_EQ(104, effects[i]->GetEndLinkFrame());
			}
			CHECK_EQ(first < c.count, originalOwned); if (originalOwned) target.release();
		}
	}
}

TEST(MultipleStationaryEffectGenerator, TargetlessGenerationReportsAnyAcceptedEffect)
{
	World world;
	for (const int first : {0, 1, 3, 4, 7, 12, 15, 16})
	{
		ClearEffects(); rejectBefore = first;
		CHECK_EQ(first < 16, world.generator.Generate(Info())); CHECK_EQ(16, submissions); CHECK_EQ(16 - first, effects.size());
		for (const auto& effect : effects) { CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(42, effect->GetActionInfo()); }
	}
}

TEST(MultipleStationaryEffectGenerator, NegativeSourceCoordinatesRemainPixelCoordinates)
{
	World world;
	auto target = Target(); auto info = Info(); info.x0 = -501; info.y0 = -250; info.pEffectTarget = target.get();
	const auto draws = RandomDraws(193, 33);
	CHECK(world.generator.Generate(info)); target.release();
	CHECK((attempted.front() == Point{-525 - draws[0] % 48, -250 - draws[1] % 50}));
	CHECK((attempted.back() == Point{-477 + draws[30] % 48, -250 + draws[31] % 50}));
	CHECK_EQ(attempted.back().x, effects.back()->GetEffectTarget()->GetX()); CHECK_EQ(attempted.back().y, effects.back()->GetEffectTarget()->GetY());
}

TEST(MultipleStationaryEffectGenerator, MissingMetadataRejectsBeforeEventsRandomnessOrConstruction)
{
	World world;
	const MStopMultipleEffectHost empty{};
	for (const auto* service : {static_cast<const MStopMultipleEffectHost*>(nullptr), &empty, &host})
	{
		ClearEffects(); spriteAvailable = false; MStopZoneMultipleEffectGenerator::SetHost(service);
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); const auto draws = RandomDraws(223, 1);
		CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK(events.empty()); CHECK_EQ(0, submissions); CHECK_EQ(draws[0], std::rand());
		CHECK(calls == (service == &host ? std::vector<int>({1}) : std::vector<int>{})); CHECK_EQ(777, target->GetX());
	}
}

TEST(MultipleStationaryEffectGenerator, MissingEventCallbackKeepsAllEffectsAndPositionDraws)
{
	World world;
	const MStopMultipleEffectHost noEvent{.Sprite = host.Sprite, .Queue = host.Queue};
	MStopZoneMultipleEffectGenerator::SetHost(&noEvent); const auto draws = RandomDraws(251, 33);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(16, effects.size()); CHECK(events.empty()); CHECK_EQ(draws[32], std::rand());
	CHECK_EQ(1, calls.front()); CHECK_EQ(4, calls[1]);
}

TEST(MultipleStationaryEffectGenerator, MissingQueueDestroysUnlinkedEffectsButStillSchedulesTheShake)
{
	World world;
	const MStopMultipleEffectHost noQueue{.Sprite = host.Sprite, .AddEvent = host.AddEvent};
	MStopZoneMultipleEffectGenerator::SetHost(&noQueue);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); const auto draws = RandomDraws(277, 33);
	CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions); CHECK_EQ(1, events.size());
	CHECK_EQ(777, target->GetX()); CHECK(removedTargets.empty()); CHECK_EQ(draws[32], std::rand());
	std::vector<int> expected{1, 2}; for (int i = 0; i < 16; ++i) expected.insert(expected.end(), {4, 3, 4}); CHECK(calls == expected);
}

TEST(MultipleStationaryEffectGenerator, SpriteCallbackCanReplaceEventAndQueueServices)
{
	World world;
	const MStopMultipleEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MStopMultipleEffectSprite& result) {
			result = {BLT_NORMAL, 23, 258}; MStopZoneMultipleEffectGenerator::SetHost(&host); return true;
		},
	};
	CHECK(MStopZoneMultipleEffectGenerator::SetHost(&changing) == &host);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(16, effects.size()); CHECK_EQ(1, events.size()); CHECK_EQ(2, calls.front());
	for (const auto& effect : effects) { CHECK_EQ(BLT_NORMAL, effect->GetBltType()); CHECK_EQ(23, effect->GetFrameID()); CHECK_EQ(2, effect->GetMaxFrame()); }
	CHECK(MStopZoneMultipleEffectGenerator::SetHost(nullptr) == &host);
}

TEST(MultipleStationaryEffectGenerator, EventCallbackCanReplaceTheQueueWithoutRefreshingMetadata)
{
	World world;
	const MStopMultipleEffectHost changing{
		.Sprite = host.Sprite,
		.AddEvent = [](MEvent& event) { host.AddEvent(event); sprite = {BLT_NORMAL, 23, 8}; MStopZoneMultipleEffectGenerator::SetHost(&host); },
	};
	MStopZoneMultipleEffectGenerator::SetHost(&changing);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(16, effects.size()); CHECK_EQ(1, events.size());
	for (const auto& effect : effects) { CHECK_EQ(12, effect->GetFrameID()); CHECK_EQ(3, effect->GetMaxFrame()); }
	ClearEffects(); CHECK(world.generator.Generate(Info()));
	for (const auto& effect : effects) { CHECK_EQ(23, effect->GetFrameID()); CHECK_EQ(8, effect->GetMaxFrame()); }
}

TEST(MultipleStationaryEffectGenerator, EventCallbackCanRemoveTheQueueBeforeRandomization)
{
	World world;
	const MStopMultipleEffectHost changing{
		.Sprite = host.Sprite,
		.AddEvent = [](MEvent& event) { host.AddEvent(event); MStopZoneMultipleEffectGenerator::SetHost(nullptr); },
		.Queue = host.Queue,
	};
	MStopZoneMultipleEffectGenerator::SetHost(&changing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); const auto draws = RandomDraws(307, 33);
	CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions); CHECK_EQ(1, events.size());
	CHECK_EQ(50, calls.size()); CHECK_EQ(draws[32], std::rand()); CHECK_EQ(777, target->GetX());
}

TEST(MultipleStationaryEffectGenerator, EventCallbackRandomnessPrecedesEveryPositionDraw)
{
	World world;
	const MStopMultipleEffectHost consumingRandom{
		.Sprite = host.Sprite,
		.AddEvent = [](MEvent& event) { observedRandom.push_back(std::rand()); host.AddEvent(event); },
		.Queue = host.Queue,
	};
	MStopZoneMultipleEffectGenerator::SetHost(&consumingRandom); const auto draws = RandomDraws(331, 34);
	CHECK(world.generator.Generate(Info())); CHECK(observedRandom == std::vector<int>({draws[0]}));
	CHECK((attempted.front() == Point{477 - draws[1] % 48, 250 - draws[2] % 50})); CHECK_EQ(draws[33], std::rand());
}

TEST(MultipleStationaryEffectGenerator, QueueRemovalKeepsTheAlreadyTransferredOriginal)
{
	World world;
	const MStopMultipleEffectHost changing{
		.Sprite = host.Sprite, .AddEvent = host.AddEvent,
		.Queue = [](std::unique_ptr<MEffect> effect) { MStopZoneMultipleEffectGenerator::SetHost(nullptr); return host.Queue(std::move(effect)); },
	};
	MStopZoneMultipleEffectGenerator::SetHost(&changing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); const auto draws = RandomDraws(359, 33);
	CHECK(world.generator.Generate(info)); CHECK_EQ(1, effects.size()); CHECK_EQ(1, submissions);
	CHECK(effects.front()->GetEffectTarget() == target.get()); target.release(); CHECK_EQ(777, info.pEffectTarget->GetX()); CHECK_EQ(draws[32], std::rand());
}

TEST(MultipleStationaryEffectGenerator, ThrowingEventKeepsCallerOwnershipBeforeAnyPositionDraw)
{
	World world;
	const MStopMultipleEffectHost throwing{
		.Sprite = host.Sprite,
		.AddEvent = [](MEvent&) { throw std::runtime_error("Event failure"); },
		.Queue = host.Queue,
	};
	MStopZoneMultipleEffectGenerator::SetHost(&throwing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); const auto draws = RandomDraws(383, 1);
	bool threw = false; try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK(effects.empty()); CHECK_EQ(0, submissions); CHECK_EQ(draws[0], std::rand()); CHECK_EQ(777, target->GetX()); CHECK(removedTargets.empty());
}

TEST(MultipleStationaryEffectGenerator, RejectingQueuesDestroyTheirOwnMarkers)
{
	World world;
	const MStopMultipleEffectHost rejecting{
		.Sprite = host.Sprite, .AddEvent = host.AddEvent,
		.Queue = [](std::unique_ptr<MEffect> effect) { effect->SetLink(43, Target(74).release()); return false; },
	};
	MStopZoneMultipleEffectGenerator::SetHost(&rejecting);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(removedTargets == std::vector<int>(16, 74));
	target.reset(); CHECK_EQ(17, removedTargets.size()); CHECK_EQ(73, removedTargets.back());
}

TEST(MultipleStationaryEffectGenerator, FirstQueueExceptionReleasesItsEffectAfterOnePhaseOfDraws)
{
	World world;
	const MStopMultipleEffectHost throwing{
		.Sprite = host.Sprite, .AddEvent = host.AddEvent,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool { effect->SetLink(43, Target(74).release()); throw std::runtime_error("Queue failure"); },
	};
	MStopZoneMultipleEffectGenerator::SetHost(&throwing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); const auto draws = RandomDraws(409, 9);
	bool threw = false; try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK(effects.empty()); CHECK_EQ(1, events.size()); CHECK_EQ(draws[8], std::rand());
	CHECK(removedTargets == std::vector<int>({74})); CHECK_EQ(777, target->GetX());
	target.reset(); CHECK(removedTargets == std::vector<int>({74, 73}));
}

TEST(MultipleStationaryEffectGenerator, LaterQueueExceptionKeepsTheTransferredOriginal)
{
	World world;
	const MStopMultipleEffectHost throwing{
		.Sprite = host.Sprite, .AddEvent = host.AddEvent,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool {
			if (effects.empty()) return host.Queue(std::move(effect));
			effect->SetLink(43, Target(74).release()); throw std::runtime_error("Later queue failure");
		},
	};
	MStopZoneMultipleEffectGenerator::SetHost(&throwing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	bool threw = false; try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, effects.size()); CHECK(effects.front()->GetEffectTarget() == target.get()); target.release();
	CHECK(removedTargets == std::vector<int>({74})); CHECK_EQ(777, info.pEffectTarget->GetX());
	ClearEffects(); CHECK(removedTargets == std::vector<int>({74, 73}));
}

TEST(MultipleStationaryEffectGenerator, ShakeRecordsCanBeQueuedByTheRealEventQueue)
{
	World world;
	MonotonicClock::ScopedTestSource clock([]() { return MonotonicClock::TimePoint(MonotonicClock::Duration(5000)); });
	MEventQueue queue; eventQueue = &queue;
	const MStopMultipleEffectHost withQueue{
		.Sprite = host.Sprite,
		.AddEvent = [](MEvent& event) { eventQueue->AddEvent(event); },
		.Queue = host.Queue,
	};
	MStopZoneMultipleEffectGenerator::SetHost(&withQueue);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(1, queue.GetEventCount());
	const auto* event = queue.GetEvent(EVENTID_METEOR_SHAKE); CHECK(event != nullptr); if (!event) return;
	CHECK_EQ(5000, event->eventStartTickCount.time_since_epoch().count()); CHECK_EQ(2300, event->eventDelay);
	CHECK_EQ(EVENTFLAG_SHAKE_SCREEN, event->eventFlag); CHECK_EQ(1, event->parameter3);
	ClearEffects(); CHECK(world.generator.Generate(Info())); CHECK_EQ(1, queue.GetEventCount());
	queue.RemoveAllEventByType(EVENTTYPE_ZONE); CHECK(queue.IsEmptyEvent());
}

TEST(MultipleStationaryEffectGenerator, FrameCountsNarrowWithoutAddingRandomAnimationStarts)
{
	World world;
	struct Case { int supplied, expected; };
	for (const Case c : {Case{-1, 255}, {0, 0}, {256, 0}, {258, 2}})
	{
		ClearEffects(); sprite.maxFrames = c.supplied; const auto draws = RandomDraws(431, 33);
		CHECK(world.generator.Generate(Info())); CHECK_EQ(draws[32], std::rand());
		for (const auto& effect : effects) { CHECK_EQ(c.expected, effect->GetMaxFrame()); CHECK_EQ(0, effect->GetFrame()); }
	}
}

TEST(MultipleStationaryEffectGenerator, StaggeredCountsStayWideAndLinkSentinelsFollowTheirOwnEffect)
{
	World world;
	auto info = Info(); info.nActionInfo = SKILL_ACID_STORM_WIDE; info.count = 65535; info.linkCount = MAX_LINKCOUNT;
	CHECK(world.generator.Generate(info)); CHECK_EQ(24, effects.size());
	CHECK_EQ(65634, effects.front()->GetEndFrame()); CHECK_EQ(65680, effects.back()->GetEndFrame()); CHECK_EQ(65680, effects.back()->GetEndLinkFrame());
	ClearEffects(); info.count = 10; info.linkCount = 20;
	CHECK(world.generator.Generate(info)); CHECK_EQ(109, effects.front()->GetEndFrame()); CHECK_EQ(155, effects.back()->GetEndFrame()); CHECK_EQ(119, effects.back()->GetEndLinkFrame());
}

TEST(MultipleStationaryEffectGenerator, RealEffectsRemainStationaryWhileAnimationAndLightAdvance)
{
	World world;
	sprite.bltType = BLT_NORMAL; CHECK(world.generator.Generate(Info()));
	for (const auto& effect : effects)
	{
		const int x = effect->GetPixelX(), y = effect->GetPixelY();
		frameNow = 100; CHECK(effect->Update()); CHECK_EQ(1, effect->GetFrame()); CHECK_EQ(8, effect->GetLight());
		CHECK_EQ(x, effect->GetPixelX()); CHECK_EQ(y, effect->GetPixelY()); CHECK_EQ(17, effect->GetPixelZ());
		frameNow = effect->GetEndFrame() + 1; CHECK(!effect->Update()); CHECK_EQ(2, effect->GetFrame()); CHECK_EQ(9, effect->GetLight());
	}
}

TEST(MultipleStationaryEffectGenerator, WrappedClocksKeepDelayAndLifetimeSemantics)
{
	World world;
	frameNow = (std::numeric_limits<DWORD>::max)() - 1; auto info = Info(); info.count = 4;
	CHECK(world.generator.Generate(info)); CHECK_EQ(1, effects.front()->GetEndFrame()); CHECK_EQ(31, effects.back()->GetEndFrame());
	CHECK(!effects.front()->Update()); CHECK(!effects.back()->IsDelayFrame());
	frameNow = 0; CHECK(effects.front()->Update()); CHECK(effects.back()->IsDelayFrame()); frameNow = 28; CHECK(!effects.back()->IsDelayFrame());
}

TEST(MultipleStationaryEffectGenerator, MissingBaseServicesKeepStaggeredCountsAndInactiveFallback)
{
	World world;
	MEffect::SetHost(nullptr);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(29, effects.front()->GetEndFrame()); CHECK_EQ(59, effects.back()->GetEndFrame());
	CHECK_EQ(4, effects.back()->GetEndLinkFrame()); CHECK_EQ(0, effects.front()->GetLight());
	CHECK(!effects.front()->Update()); CHECK(!effects.back()->IsDelayFrame());
}

TEST(MultipleStationaryEffectGenerator, PositiveRandomizedCoordinatesSaturateBeforeTargetCopies)
{
	World world;
	const int high = (std::numeric_limits<int>::max)();
	struct Case { TYPE_ACTIONINFO action; int count, rangeX, rangeY; };
	for (const Case c : {Case{42, 16, 48, 50}, {SKILL_ACID_STORM_WIDE, 24, 96, 100}, {SKILL_POISON_STORM_WIDE, 24, 96, 100}})
	{
		ClearEffects(); auto target = Target(); auto info = Info(); info.nActionInfo = c.action; info.x0 = info.y0 = high; info.pEffectTarget = target.get();
		const auto draws = RandomDraws(487, static_cast<size_t>(2 * c.count + 1));
		CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(c.count, effects.size()); CHECK_EQ(draws.back(), std::rand());
		CHECK_EQ(777, info.pEffectTarget->GetX()); CHECK_EQ(888, info.pEffectTarget->GetY());
		for (size_t i = 1; i < effects.size(); ++i)
		{
			const int x = i % 4 < 2 ? high - draws[2 * i] % c.rangeX - 24 : high;
			const int y = i % 2 == 0 ? high - draws[2 * i + 1] % c.rangeY : high;
			CHECK_EQ(x, effects[i]->GetEffectTarget()->GetX()); CHECK_EQ(y, effects[i]->GetEffectTarget()->GetY());
		}
	}
}

TEST(MultipleStationaryEffectGenerator, NegativeRandomizedCoordinatesSaturateBeforeTargetCopies)
{
	World world;
	const int low = (std::numeric_limits<int>::min)();
	struct Case { TYPE_ACTIONINFO action; int count, rangeX, rangeY; };
	for (const Case c : {Case{42, 16, 48, 50}, {SKILL_ACID_STORM_WIDE, 24, 96, 100}, {SKILL_POISON_STORM_WIDE, 24, 96, 100}})
	{
		ClearEffects(); auto target = Target(); auto info = Info(); info.nActionInfo = c.action; info.x0 = info.y0 = low; info.pEffectTarget = target.get();
		const auto draws = RandomDraws(509, static_cast<size_t>(2 * c.count + 1));
		CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(c.count, effects.size()); CHECK_EQ(draws.back(), std::rand());
		CHECK_EQ(777, info.pEffectTarget->GetX()); CHECK_EQ(888, info.pEffectTarget->GetY());
		for (size_t i = 1; i < effects.size(); ++i)
		{
			const int x = i % 4 < 2 ? low : low + draws[2 * i] % c.rangeX + 24;
			const int y = i % 2 == 0 ? low : low + draws[2 * i + 1] % c.rangeY;
			CHECK_EQ(x, effects[i]->GetEffectTarget()->GetX()); CHECK_EQ(y, effects[i]->GetEffectTarget()->GetY());
		}
	}
}

TEST(MultipleStationaryEffectGenerator, FixedHorizontalOffsetsCannotOverflowAfterRepresentableRandomSums)
{
	World world;
	const int low = (std::numeric_limits<int>::min)(), high = (std::numeric_limits<int>::max)();
	for (const bool upper : {false, true})
	{
		ClearEffects(); auto target = Target(); auto info = Info(); info.x0 = upper ? high - 47 : low + 47; info.pEffectTarget = target.get();
		const auto draws = RandomDraws(547, 33); CHECK(world.generator.Generate(info)); target.release();
		for (size_t i = 1; i < effects.size(); ++i)
		{
			const int amount = draws[2 * i] % 48; const bool left = i % 4 < 2;
			const int expected = upper ? (left ? high - 71 - amount : (amount > 23 ? high : high - 23 + amount)) :
				(left ? (amount > 23 ? low : low + 23 - amount) : low + 71 + amount);
			CHECK_EQ(expected, effects[i]->GetEffectTarget()->GetX());
		}
	}
}

TEST(MultipleStationaryEffectGenerator, TargetlessExtremePositionsStayNearTheirSource)
{
	World world;
	const int low = (std::numeric_limits<int>::min)(), high = (std::numeric_limits<int>::max)();
	for (const bool upper : {false, true})
	{
		ClearEffects(); auto info = Info(); info.x0 = info.y0 = upper ? high : low; info.nActionInfo = SKILL_ACID_STORM_WIDE;
		const auto draws = RandomDraws(571, 49); CHECK(world.generator.Generate(info)); CHECK_EQ(24, effects.size()); CHECK_EQ(draws[48], std::rand());
		for (const auto& effect : effects)
		{
			// Float storage may round by one spacing unit near the integer limit.
			CHECK(upper ? effect->GetPixelX() >= high - 256 : effect->GetPixelX() <= low + 256);
			CHECK(upper ? effect->GetPixelY() >= high - 256 : effect->GetPixelY() <= low + 256);
			CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(SKILL_ACID_STORM_WIDE, effect->GetActionInfo());
		}
	}
}

TEST(MultipleStationaryEffectGenerator, RepresentableRandomizedCoordinatesRemainExactNearLimits)
{
	World world;
	const int low = (std::numeric_limits<int>::min)(), high = (std::numeric_limits<int>::max)();
	auto target = Target(); auto info = Info(); info.x0 = high - 120; info.y0 = low + 100; info.nActionInfo = SKILL_ACID_STORM_WIDE; info.pEffectTarget = target.get();
	const auto draws = RandomDraws(599, 49); CHECK(world.generator.Generate(info)); target.release();
	for (size_t i = 1; i < effects.size(); ++i)
	{
		const int x = i % 4 < 2 ? high - 144 - draws[2 * i] % 96 : high - 96 + draws[2 * i] % 96;
		const int y = i % 2 == 0 ? low + 100 - draws[2 * i + 1] % 100 : low + 100 + draws[2 * i + 1] % 100;
		CHECK_EQ(x, effects[i]->GetEffectTarget()->GetX()); CHECK_EQ(y, effects[i]->GetEffectTarget()->GetY());
	}
}

TEST(MultipleStationaryEffectGenerator, ExtremePositionsKeepRejectedAndLateOriginalTargetsUntouched)
{
	World world;
	for (const bool accepted : {false, true})
	{
		ClearEffects(); rejectBefore = 15; acceptQueue = accepted;
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		info.x0 = (std::numeric_limits<int>::max)(); info.y0 = (std::numeric_limits<int>::min)();
		CHECK_EQ(accepted, world.generator.Generate(info)); CHECK_EQ(16, submissions); CHECK_EQ(accepted ? 1 : 0, effects.size());
		CHECK_EQ(777, target->GetX()); CHECK_EQ(888, target->GetY()); CHECK_EQ(999, target->GetZ()); CHECK_EQ(456, target->GetID());
		if (accepted) { CHECK(effects.front()->GetEffectTarget() == target.get()); target.release(); }
	}
}
