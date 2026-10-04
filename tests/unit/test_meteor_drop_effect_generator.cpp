#include "test_framework.h"
#include "MMeteorDropEffectGenerator.h"
#include "MFollowPathEffectGenerator.h"
#include "MLinearEffect.h"
#include "MEventQueue.h"

#include <array>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MonotonicClock::TimePoint timeNow;
MFixedZoneEffectSprite sprite;
bool spriteAvailable, acceptEffect;
int requestedSprite, submissions;
std::vector<int> calls, removedTargets;
std::vector<std::array<WORD, 3>> gammaRamps;
std::vector<std::unique_ptr<MEffect>> effects;
std::vector<MEvent> events;
std::unique_ptr<MEventQueue> eventQueue;
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(3); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE frame) { calls.push_back(2); return 7 + frame; },
};
const MEffectTargetHost targetHost{.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); }};
const MEventHost eventHost{
	.SetAddGammaRamp = [](WORD red, WORD green, WORD blue) { calls.push_back(6); gammaRamps.push_back({red, green, blue}); },
};
const MMeteorDropEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& result) {
		calls.push_back(1); requestedSprite = type; result = sprite; return spriteAvailable;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		calls.push_back(4); ++submissions; CHECK_EQ(MEffect::EFFECT_LINEAR, effect->GetEffectType());
		CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		if (!acceptEffect) return false;
		effects.push_back(std::move(effect)); return true;
	},
	.AddEvent = [](MEvent& event) {
		calls.push_back(5); CHECK(!effects.empty()); CHECK_EQ(42, effects.back()->GetActionInfo());
		eventQueue->AddEvent(event); events.push_back(event);
	},
};

struct World
{
	MonotonicClock::ScopedTestSource clock{[]() { return timeNow; }};
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MMeteorDropEffectHost* previousGenerator = MMeteorDropEffectGenerator::SetHost(&host);
	MMeteorDropEffectGenerator generator;
	World()
	{
		frameNow = 100; timeNow = MonotonicClock::FromMillis(1500); sprite = {BLT_EFFECT, 12, 3}; spriteAvailable = acceptEffect = true;
		requestedSprite = -1; submissions = 0; eventQueue = std::make_unique<MEventQueue>(&eventHost);
		effects.clear(); events.clear(); calls.clear(); removedTargets.clear(); gammaRamps.clear();
	}
	~World()
	{
		effects.clear(); eventQueue.reset(); MMeteorDropEffectGenerator::SetHost(previousGenerator);
		MEffectTarget::SetHost(previousTarget); MEffect::SetHost(previousEffect);
	}
};

EFFECTGENERATOR_INFO Info()
{
	EFFECTGENERATOR_INFO info{}; info.nActionInfo = 42; info.effectSpriteType = 17;
	info.x0 = 900; info.y0 = 800; info.z0 = 700; info.x1 = 96; info.y1 = 48; info.z1 = 12;
	info.direction = DIRECTION_LEFTUP; info.step = 100; info.count = 50; info.linkCount = 5; info.power = 77; info.creatureID = 123;
	return info;
}

std::unique_ptr<MEffectTarget> Target(BYTE id = 73)
{
	auto target = std::make_unique<MEffectTarget>(3); target->NextPhase(); target->m_EffectID = id;
	target->Set(777, 888, 999, 456); target->SetServerID(789); target->SetDelayFrame(31);
	target->SetResultTime(); target->SetResult(new MActionResult); return target;
}

void CheckTarget(const MEffectTarget& target)
{
	CHECK_EQ(777, target.GetX()); CHECK_EQ(888, target.GetY()); CHECK_EQ(999, target.GetZ()); CHECK_EQ(456, target.GetID());
	CHECK_EQ(789, target.GetServerID()); CHECK_EQ(1, target.GetCurrentPhase()); CHECK_EQ(3, target.GetMaxPhase()); CHECK_EQ(31, target.GetDelayFrame());
	CHECK(target.IsExistResult()); CHECK(target.IsResultTime());
}

void ClearEffects()
{
	effects.clear(); eventQueue->RemoveAllEvent(); calls.clear(); events.clear(); gammaRamps.clear(); submissions = 0;
}

int NextRandom(unsigned seed)
{
	std::srand(seed); const int next = std::rand(); std::srand(seed); return next;
}

} // namespace

TEST(MeteorDropEffectGenerator, ConfiguresARealLinearMeteorAndSchedulesTheFadeAfterAcceptance)
{
	World world; CHECK_EQ(EFFECTGENERATORID_METEOR_DROP, world.generator.GetID()); const int next = NextRandom(67);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(next, std::rand()); CHECK_EQ(17, requestedSprite); CHECK_EQ(1, submissions); CHECK_EQ(1, effects.size());
	CHECK(calls == std::vector<int>({1, 2, 3, 4, 5, 6}));
	auto& effect = *effects.front(); CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame());
	CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight()); CHECK_EQ(196, effect.GetPixelX()); CHECK_EQ(48, effect.GetPixelY()); CHECK_EQ(412, effect.GetPixelZ());
	CHECK_EQ(4, effect.GetX()); CHECK_EQ(2, effect.GetY()); CHECK_EQ(DIRECTION_LEFT, effect.GetDirection()); CHECK_EQ(100, effect.GetStepPixel());
	CHECK_EQ(77, effect.GetPower()); CHECK_EQ(149, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame()); CHECK_EQ(42, effect.GetActionInfo());
	CHECK(effect.GetEffectTarget() == nullptr); CHECK(!effect.IsMulti()); CHECK(!effect.IsDelayFrame());
	CHECK(gammaRamps == (std::vector<std::array<WORD, 3>>{{30, 0, 0}}));
}

TEST(MeteorDropEffectGenerator, EventHasOneSecondLifetimeAndOnlyTheRedFadeFlag)
{
	World world; CHECK(world.generator.Generate(Info())); CHECK_EQ(1, events.size());
	const auto& event = events.front(); CHECK_EQ(EVENTID_METEOR, event.eventID); CHECK_EQ(EVENTTYPE_ZONE, event.eventType);
	CHECK_EQ(1000, event.eventDelay); CHECK_EQ(EVENTFLAG_FADE_SCREEN, event.eventFlag); CHECK_EQ(30 << 16, event.parameter2);
	CHECK_EQ(0, event.parameter1); CHECK_EQ(0, event.parameter3); CHECK_EQ(0, event.parameter4); CHECK(event.m_StringsID.empty());
	CHECK_EQ(-1, event.showTime); CHECK_EQ(-1, event.totalTime); CHECK(event.eventStartTickCount == timeNow);
	timeNow = MonotonicClock::FromMillis(2500); eventQueue->ProcessEvent(); CHECK(eventQueue->IsEvent(EVENTID_METEOR));
	timeNow = MonotonicClock::FromMillis(2501); eventQueue->ProcessEvent(); CHECK(eventQueue->IsEmptyEvent());
	CHECK(gammaRamps == (std::vector<std::array<WORD, 3>>{{30, 0, 0}, {0, 0, 0}})); CHECK_EQ(1, effects.size());
}

TEST(MeteorDropEffectGenerator, RepeatedMeteorsReplaceTheFadeAndRestartItsClock)
{
	World world; CHECK(world.generator.Generate(Info())); timeNow = MonotonicClock::FromMillis(2200);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(2, effects.size()); CHECK_EQ(2, events.size()); CHECK_EQ(1, eventQueue->GetEventCount());
	CHECK(eventQueue->GetEvent(EVENTID_METEOR)->eventStartTickCount == timeNow);
	timeNow = MonotonicClock::FromMillis(2501); eventQueue->ProcessEvent(); CHECK(eventQueue->IsEvent(EVENTID_METEOR));
	timeNow = MonotonicClock::FromMillis(3201); eventQueue->ProcessEvent(); CHECK(eventQueue->IsEmptyEvent());
}

TEST(MeteorDropEffectGenerator, RealMotionFallsDiagonallyAndStopsBeforeArrivalAnimation)
{
	World world; CHECK(world.generator.Generate(Info())); auto& effect = *effects.front();
	CHECK(effect.Update()); CHECK_EQ(171, effect.GetPixelX()); CHECK_EQ(48, effect.GetPixelY()); CHECK_EQ(314, effect.GetPixelZ()); CHECK_EQ(1, effect.GetFrame()); CHECK_EQ(8, effect.GetLight());
	CHECK(effect.Update()); CHECK_EQ(147, effect.GetPixelX()); CHECK_EQ(217, effect.GetPixelZ());
	CHECK(effect.Update()); CHECK_EQ(123, effect.GetPixelX()); CHECK_EQ(120, effect.GetPixelZ()); CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight());
	CHECK(!effect.Update()); CHECK_EQ(96, effect.GetPixelX()); CHECK_EQ(48, effect.GetPixelY()); CHECK_EQ(12, effect.GetPixelZ());
	CHECK_EQ(0, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame()); CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight());
	CHECK(eventQueue->IsEvent(EVENTID_METEOR)); CHECK_EQ(1, events.size());
}

TEST(MeteorDropEffectGenerator, ZeroSpeedAnimatesUntilTheDeadlineWithoutMoving)
{
	World world; auto info = Info(); info.step = 0; CHECK(world.generator.Generate(info)); auto& effect = *effects.front();
	CHECK(effect.Update()); CHECK_EQ(196, effect.GetPixelX()); CHECK_EQ(412, effect.GetPixelZ()); CHECK_EQ(1, effect.GetFrame());
	frameNow = 149; CHECK(!effect.Update()); CHECK_EQ(196, effect.GetPixelX()); CHECK_EQ(412, effect.GetPixelZ()); CHECK_EQ(1, effect.GetFrame());
}

TEST(MeteorDropEffectGenerator, LargeStepRetainsTheLinearArrivalTolerance)
{
	World world; auto info = Info(); info.step = 255; CHECK(world.generator.Generate(info)); auto& effect = *effects.front();
	CHECK(!effect.Update()); CHECK_EQ(96, effect.GetPixelX()); CHECK_EQ(12, effect.GetPixelZ()); CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(0, effect.GetEndFrame());
}

TEST(MeteorDropEffectGenerator, EveryInputDirectionIsOverriddenByTheTrajectoryFacing)
{
	World world;
	for (int direction = 0; direction < 256; ++direction)
	{
		ClearEffects(); auto info = Info(); info.direction = static_cast<BYTE>(direction); CHECK(world.generator.Generate(info));
		CHECK_EQ(DIRECTION_LEFT, effects.front()->GetDirection()); CHECK_EQ(196, effects.front()->GetPixelX()); CHECK_EQ(412, effects.front()->GetPixelZ());
	}
}

TEST(MeteorDropEffectGenerator, SourceCoordinatesAreIgnoredAndNegativeDestinationsKeepOffsets)
{
	World world; auto info = Info(); info.x0 = info.y0 = info.z0 = (std::numeric_limits<int>::min)();
	info.x1 = -400; info.y1 = -123; info.z1 = -500; CHECK(world.generator.Generate(info));
	CHECK_EQ(-300, effects.front()->GetPixelX()); CHECK_EQ(-123, effects.front()->GetPixelY()); CHECK_EQ(-100, effects.front()->GetPixelZ());
	ClearEffects(); info.x0 = info.y0 = info.z0 = (std::numeric_limits<int>::max)(); CHECK(world.generator.Generate(info));
	CHECK_EQ(-300, effects.front()->GetPixelX()); CHECK_EQ(-123, effects.front()->GetPixelY()); CHECK_EQ(-100, effects.front()->GetPixelZ());
}

TEST(MeteorDropEffectGenerator, OriginalTargetTransfersWithAllCoordinatesAndMetadataUnchanged)
{
	World world; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); auto* result = target->GetResult();
	CHECK(world.generator.Generate(info)); target.release(); CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget); CheckTarget(*info.pEffectTarget);
	CHECK(info.pEffectTarget->GetResult() == result); CHECK(removedTargets.empty()); ClearEffects(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(MeteorDropEffectGenerator, EventCallbackObservesTheAlreadyLinkedOriginalTarget)
{
	World world; static MEffectTarget* expectedTarget; auto target = Target(); expectedTarget = target.get();
	const MMeteorDropEffectHost inspecting{
		.Sprite = host.Sprite, .Queue = host.Queue,
		.AddEvent = [](MEvent& event) { CHECK(effects.back()->GetEffectTarget() == expectedTarget); CheckTarget(*expectedTarget); host.AddEvent(event); },
	};
	MMeteorDropEffectGenerator::SetHost(&inspecting); auto info = Info(); info.pEffectTarget = target.get(); CHECK(world.generator.Generate(info)); target.release();
	CHECK_EQ(1, events.size());
}

TEST(MeteorDropEffectGenerator, RejectedSubmissionLeavesTheCallerTargetAndSchedulesNoEvent)
{
	World world; acceptEffect = false; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK_EQ(1, submissions); CHECK(effects.empty()); CHECK(events.empty()); CHECK(gammaRamps.empty()); CheckTarget(*target);
	CHECK(removedTargets.empty()); CHECK(calls == std::vector<int>({1, 2, 3, 4}));
}

TEST(MeteorDropEffectGenerator, MissingMetadataRejectsBeforeConstructingOrScheduling)
{
	World world; const MMeteorDropEffectHost empty{};
	for (const auto* service : {static_cast<const MMeteorDropEffectHost*>(nullptr), &empty, &host})
	{
		ClearEffects(); MMeteorDropEffectGenerator::SetHost(service); spriteAvailable = false; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK(!world.generator.Generate(info)); CHECK_EQ(0, submissions); CHECK(events.empty()); CheckTarget(*target);
		CHECK(calls == (service == &host ? std::vector<int>{1} : std::vector<int>{}));
	}
}

TEST(MeteorDropEffectGenerator, MissingQueueDiscardsTheUnlinkedEffectAndSkipsTheEvent)
{
	World world; const MMeteorDropEffectHost noQueue{.Sprite = host.Sprite, .AddEvent = host.AddEvent}; MMeteorDropEffectGenerator::SetHost(&noQueue);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); CHECK(!world.generator.Generate(info));
	CHECK(calls == std::vector<int>({1, 2, 3})); CHECK(effects.empty()); CHECK(events.empty()); CHECK(removedTargets.empty()); CheckTarget(*target);
}

TEST(MeteorDropEffectGenerator, MissingEventServiceKeepsAcceptedOwnershipAndReturnsSuccess)
{
	World world; const MMeteorDropEffectHost noEvent{.Sprite = host.Sprite, .Queue = host.Queue}; MMeteorDropEffectGenerator::SetHost(&noEvent);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); CHECK(world.generator.Generate(info)); target.release();
	CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget); CHECK(events.empty()); CHECK(calls == std::vector<int>({1, 2, 3, 4})); CheckTarget(*info.pEffectTarget);
}

TEST(MeteorDropEffectGenerator, SpriteCallbackCanReplaceQueueAndEventServices)
{
	World world; const MMeteorDropEffectHost replacing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& result) { MMeteorDropEffectGenerator::SetHost(&host); return host.Sprite(type, result); },
	};
	MMeteorDropEffectGenerator::SetHost(&replacing); CHECK(world.generator.Generate(Info())); CHECK_EQ(1, effects.size()); CHECK_EQ(1, events.size());
}

TEST(MeteorDropEffectGenerator, SpriteCallbackCanRemoveTheRemainingServices)
{
	World world; const MMeteorDropEffectHost removing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& result) { MMeteorDropEffectGenerator::SetHost(nullptr); return host.Sprite(type, result); },
		.Queue = host.Queue, .AddEvent = host.AddEvent,
	};
	MMeteorDropEffectGenerator::SetHost(&removing); CHECK(!world.generator.Generate(Info())); CHECK(effects.empty()); CHECK(events.empty());
	CHECK(calls == std::vector<int>({1, 2, 3}));
}

TEST(MeteorDropEffectGenerator, QueueCallbackCanInstallTheEventService)
{
	World world; const MMeteorDropEffectHost replacing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { MMeteorDropEffectGenerator::SetHost(&host); return host.Queue(std::move(effect)); },
	};
	MMeteorDropEffectGenerator::SetHost(&replacing); CHECK(world.generator.Generate(Info())); CHECK_EQ(1, events.size());
}

TEST(MeteorDropEffectGenerator, AcceptedQueueRemovalSkipsTheEventButStillLinksTheTarget)
{
	World world; const MMeteorDropEffectHost removing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { MMeteorDropEffectGenerator::SetHost(nullptr); return host.Queue(std::move(effect)); },
		.AddEvent = host.AddEvent,
	};
	MMeteorDropEffectGenerator::SetHost(&removing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release(); CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget); CHECK(events.empty());
	CHECK(!world.generator.Generate(Info())); CHECK_EQ(1, effects.size());
}

TEST(MeteorDropEffectGenerator, EventCallbackCanRemoveTheGeneratorHost)
{
	World world; const MMeteorDropEffectHost removing{
		.Sprite = host.Sprite, .Queue = host.Queue,
		.AddEvent = [](MEvent& event) { MMeteorDropEffectGenerator::SetHost(nullptr); host.AddEvent(event); },
	};
	MMeteorDropEffectGenerator::SetHost(&removing); CHECK(world.generator.Generate(Info())); CHECK_EQ(1, events.size());
	CHECK(!world.generator.Generate(Info())); CHECK_EQ(1, effects.size());
}

TEST(MeteorDropEffectGenerator, EachGenerationReadsFreshSpriteMetadata)
{
	World world; CHECK(world.generator.Generate(Info())); ClearEffects(); sprite = {BLT_SHADOW, 21, 258};
	CHECK(world.generator.Generate(Info())); CHECK_EQ(BLT_SHADOW, effects.front()->GetBltType()); CHECK_EQ(21, effects.front()->GetFrameID()); CHECK_EQ(2, effects.front()->GetMaxFrame());
}

TEST(MeteorDropEffectGenerator, InstallerIsIndependentOfFixedPatternGenerators)
{
	World world; const MFixedZoneEffectHost sentinel{}; const auto* previous = MFollowPathEffectGenerator::SetHost(&sentinel);
	CHECK(MMeteorDropEffectGenerator::SetHost(nullptr) == &host); CHECK(MFollowPathEffectGenerator::SetHost(previous) == &sentinel);
	CHECK(!world.generator.Generate(Info())); CHECK(calls.empty());
}

TEST(MeteorDropEffectGenerator, RejectingQueueConsumesItsMarkerWithoutTakingTheCallerTarget)
{
	World world; const MMeteorDropEffectHost rejecting{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { ++submissions; effect->SetLink(42, Target(94).release()); return false; },
		.AddEvent = host.AddEvent,
	};
	MMeteorDropEffectGenerator::SetHost(&rejecting); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(removedTargets == std::vector<int>{94}); CHECK(events.empty()); CheckTarget(*target);
}

TEST(MeteorDropEffectGenerator, QueueExceptionDestroysItsEffectBeforeAnyTargetTransferOrEvent)
{
	World world; const MMeteorDropEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool { effect->SetLink(42, Target(94).release()); throw std::runtime_error("queue"); },
		.AddEvent = host.AddEvent,
	};
	MMeteorDropEffectGenerator::SetHost(&throwing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK(removedTargets == std::vector<int>{94}); CHECK(events.empty()); CheckTarget(*target);
}

TEST(MeteorDropEffectGenerator, EventExceptionLeavesTheAcceptedEffectOwningTheOriginal)
{
	World world; const MMeteorDropEffectHost throwing{
		.Sprite = host.Sprite, .Queue = host.Queue,
		.AddEvent = [](MEvent&) { throw std::runtime_error("event"); },
	};
	MMeteorDropEffectGenerator::SetHost(&throwing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, effects.size()); CHECK(effects.front()->GetEffectTarget() == target.get());
	if (effects.front()->GetEffectTarget() == target.get()) target.release();
	CheckTarget(*info.pEffectTarget); CHECK(removedTargets.empty()); ClearEffects(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(MeteorDropEffectGenerator, AnimationCountNarrowingAndPowerDoNotChangeTheFade)
{
	World world;
	for (const int frames : {-1, 0, 256, 258}) for (const BYTE power : {static_cast<BYTE>(0), static_cast<BYTE>(255)})
	{
		ClearEffects(); sprite.maxFrames = frames; auto info = Info(); info.power = power; CHECK(world.generator.Generate(info));
		CHECK_EQ(static_cast<BYTE>(frames), effects.front()->GetMaxFrame()); CHECK_EQ(power, effects.front()->GetPower());
		CHECK_EQ(149, effects.front()->GetEndFrame()); CHECK_EQ(1000, events.front().eventDelay); CHECK_EQ(30 << 16, events.front().parameter2);
	}
}

TEST(MeteorDropEffectGenerator, CountsAndLinkSentinelsKeepIndependentFiniteDeadlines)
{
	World world; struct Timing { WORD count, link; DWORD end, endLink; };
	for (const Timing time : {Timing{0, 5, 99, 104}, Timing{65535, MAX_LINKCOUNT, 65634, 65634}, Timing{1, 65534, 100, 65633}})
	{
		ClearEffects(); auto info = Info(); info.count = time.count; info.linkCount = time.link; CHECK(world.generator.Generate(info));
		CHECK_EQ(time.end, effects.front()->GetEndFrame()); CHECK_EQ(time.endLink, effects.front()->GetEndLinkFrame()); CHECK_EQ(1, events.size());
	}
}

TEST(MeteorDropEffectGenerator, FrameClockWrapAndMissingBaseServicesDoNotPreventTheEvent)
{
	World world; frameNow = 0xFFFFFFFEu; CHECK(world.generator.Generate(Info()));
	CHECK_EQ(47, effects.front()->GetEndFrame()); CHECK_EQ(2, effects.front()->GetEndLinkFrame()); CHECK(effects.front()->IsEnd()); CHECK_EQ(1, events.size());
	ClearEffects(); MEffect::SetHost(nullptr); CHECK(world.generator.Generate(Info())); CHECK_EQ(49, effects.front()->GetEndFrame());
	CHECK_EQ(4, effects.front()->GetEndLinkFrame()); CHECK_EQ(0, effects.front()->GetLight()); CHECK(!effects.front()->Update()); CHECK_EQ(1, events.size());
}

TEST(MeteorDropEffectGenerator, EventLifetimeCrossesTheLegacyMillisecondWrap)
{
	World world; timeNow = MonotonicClock::FromMillis(0xFFFFFFFEull); CHECK(world.generator.Generate(Info()));
	timeNow = MonotonicClock::FromMillis(0x1000003E6ull); eventQueue->ProcessEvent(); CHECK(eventQueue->IsEvent(EVENTID_METEOR));
	timeNow = MonotonicClock::FromMillis(0x1000003E7ull); eventQueue->ProcessEvent(); CHECK(eventQueue->IsEmptyEvent());
}

TEST(MeteorDropEffectGenerator, HorizontalLaunchOffsetSaturatesAtTheIntegerLimit)
{
	World world; const int top = (std::numeric_limits<int>::max)();
	for (const int destination : {top, top - 1, top - 99})
	{
		ClearEffects(); auto info = Info(); info.x1 = destination; CHECK(world.generator.Generate(info));
		CHECK_EQ(top, effects.front()->GetPixelX()); CHECK_EQ(48, effects.front()->GetPixelY()); CHECK_EQ(412, effects.front()->GetPixelZ());
	}
}

TEST(MeteorDropEffectGenerator, VerticalLaunchOffsetSaturatesAtTheIntegerLimit)
{
	World world; const int top = (std::numeric_limits<int>::max)();
	for (const int destination : {top, top - 1, top - 399})
	{
		ClearEffects(); auto info = Info(); info.z1 = destination; CHECK(world.generator.Generate(info));
		CHECK_EQ(top, effects.front()->GetPixelZ()); CHECK_EQ(196, effects.front()->GetPixelX()); CHECK_EQ(48, effects.front()->GetPixelY());
	}
}

TEST(MeteorDropEffectGenerator, RepresentableOffsetsKeepTheirExistingFloatRounding)
{
	World world; const int top = (std::numeric_limits<int>::max)(), bottom = (std::numeric_limits<int>::min)();
	struct Position { int x, z, expectedX, expectedZ; };
	for (const Position point : {Position{top - 100, top - 400, top, top}, {top - 611, top - 911, top - 511, top - 511},
		{bottom, bottom, bottom + 128, bottom + 384}, {-100, -400, 0, 0}, {-1, -1, 99, 399}, {0, 0, 100, 400}})
	{
		ClearEffects(); auto info = Info(); info.x1 = point.x; info.z1 = point.z; CHECK(world.generator.Generate(info));
		CHECK_EQ(point.expectedX, effects.front()->GetPixelX()); CHECK_EQ(point.expectedZ, effects.front()->GetPixelZ());
	}
}

TEST(MeteorDropEffectGenerator, SaturatedLaunchStillArrivesAndRetainsTheOriginalTargetAndFade)
{
	World world; const int top = (std::numeric_limits<int>::max)(); auto target = Target(); auto info = Info();
	info.x1 = top; info.z1 = top - 127; info.step = 255; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release(); auto& effect = *effects.front(); CHECK(effect.GetEffectTarget() == info.pEffectTarget);
	CHECK_EQ(top, effect.GetPixelX()); CHECK_EQ(top, effect.GetPixelZ()); CheckTarget(*info.pEffectTarget);
	CHECK(!effect.Update()); CHECK_EQ(top, effect.GetPixelX()); CHECK_EQ(top - 127, effect.GetPixelZ()); CHECK_EQ(0, effect.GetFrame());
	CHECK_EQ(0, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame()); CHECK_EQ(1, events.size()); CHECK(eventQueue->IsEvent(EVENTID_METEOR));
	ClearEffects(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(MeteorDropEffectGenerator, RejectedExtremeLaunchKeepsTheCallerTargetAndSkipsTheFade)
{
	World world; acceptEffect = false; auto target = Target(); auto info = Info();
	info.x1 = info.z1 = (std::numeric_limits<int>::max)(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK_EQ(1, submissions); CHECK(effects.empty()); CHECK(events.empty()); CheckTarget(*target);
	CHECK(removedTargets.empty());
}
