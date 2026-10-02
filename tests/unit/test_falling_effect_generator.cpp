#include "test_framework.h"
#include "MFallingEffectGenerator.h"
#include "MLinearEffect.h"
#include "SkillDef.h"

#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MFallingEffectSprite sprite;
bool spriteAvailable, acceptQueue;
int requestedSprite, nextLight;
std::vector<int> calls, destroyedTargets;
std::vector<std::unique_ptr<MEffect>> effects;
struct LightRequest
{
	BYTE blt;
	TYPE_FRAMEID id;
	BYTE direction, frame;
	bool operator==(const LightRequest&) const = default;
};
std::vector<LightRequest> lights;
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(3); return frameNow; },
	.Light = [](BYTE blt, TYPE_FRAMEID id, BYTE direction, BYTE frame) {
		calls.push_back(2);
		lights.push_back({blt, id, direction, frame});
		return nextLight++;
	},
};
const MFallingEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFallingEffectSprite& result) {
		calls.push_back(1); requestedSprite = type;
		result = sprite;
		return spriteAvailable;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		calls.push_back(4);
		CHECK(effect->GetEffectTarget() == nullptr);
		CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		if (!acceptQueue) return false;
		effects.push_back(std::move(effect));
		return true;
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(nullptr);
	const MFallingEffectHost* previousFalling = MFallingEffectGenerator::SetHost(&host);
	World()
	{
		frameNow = 100; nextLight = 7;
		sprite = {BLT_EFFECT, 12, 3}; spriteAvailable = acceptQueue = true;
		requestedSprite = -1;
		effects.clear(); calls.clear(); lights.clear(); destroyedTargets.clear();
	}
	~World()
	{
		effects.clear();
		MFallingEffectGenerator::SetHost(previousFalling);
		MEffectTarget::SetHost(previousTarget);
		MEffect::SetHost(previousEffect);
	}
};

struct Target : MEffectTarget
{
	using MEffectTarget::operator=;
	int id;
	Target(int tag) : MEffectTarget(3), id(tag) { NextPhase(); }
	~Target() override { destroyedTargets.push_back(id); }
};

EFFECTGENERATOR_INFO Info()
{
	EFFECTGENERATOR_INFO info{};
	info.nActionInfo = 42; info.effectSpriteType = 17;
	info.x0 = 900; info.y0 = 800; info.z0 = 700;
	info.x1 = 96; info.y1 = 48; info.z1 = 12;
	info.direction = DIRECTION_LEFTUP; info.step = 100;
	info.count = 50; info.linkCount = 5; info.power = 77;
	return info;
}

} // namespace

TEST(FallingEffectGenerator, CreatesAConfiguredLinearEffectAboveItsDestination)
{
	World world;
	MFallingEffectGenerator generator;
	CHECK_EQ(EFFECTGENERATORID_FALLING, generator.GetID());
	CHECK(generator.Generate(Info()));
	CHECK_EQ(1, effects.size());
	const auto& effect = *effects.front();
	CHECK_EQ(MEffect::EFFECT_LINEAR, effect.GetEffectType());
	CHECK_EQ(BLT_EFFECT, effect.GetBltType());
	CHECK_EQ(12, effect.GetFrameID());
	CHECK_EQ(3, effect.GetMaxFrame());
	CHECK_EQ(0, effect.GetFrame());
	CHECK_EQ(7, effect.GetLight());
	CHECK_EQ(96, effect.GetPixelX());
	CHECK_EQ(48, effect.GetPixelY());
	CHECK_EQ(312, effect.GetPixelZ());
	CHECK_EQ(2, effect.GetX());
	CHECK_EQ(2, effect.GetY());
	CHECK_EQ(100, effect.GetStepPixel());
	CHECK_EQ(DIRECTION_DOWN, effect.GetDirection());
	CHECK_EQ(149, effect.GetEndFrame());
	CHECK_EQ(104, effect.GetEndLinkFrame());
	CHECK_EQ(77, effect.GetPower());
	CHECK_EQ(42, effect.GetActionInfo());
	CHECK_EQ(17, requestedSprite);
	CHECK(calls == std::vector<int>({1, 2, 3, 4}));
	CHECK(lights == std::vector<LightRequest>({{BLT_EFFECT, 12, DIRECTION_LEFT, 0}}));
}

TEST(FallingEffectGenerator, AcceptanceTransfersTheTargetAfterQueueSubmission)
{
	World world;
	MFallingEffectGenerator generator;
	auto target = std::make_unique<Target>(1);
	auto info = Info(); info.pEffectTarget = target.get();
	CHECK(generator.Generate(info));
	target.release();
	CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget);
	CHECK_EQ(1, effects.front()->GetLinkSize());
	CHECK(destroyedTargets.empty());
	effects.clear();
	CHECK(destroyedTargets == std::vector<int>({1}));
}

TEST(FallingEffectGenerator, RealProjectileFallsAndStopsBeforeTheArrivalFrameAndLightAdvance)
{
	World world;
	MFallingEffectGenerator generator;
	CHECK(generator.Generate(Info()));
	auto& effect = *effects.front();
	CHECK(effect.Update());
	CHECK_EQ(212, effect.GetPixelZ());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(8, effect.GetLight());
	CHECK(effect.Update());
	CHECK_EQ(112, effect.GetPixelZ());
	CHECK_EQ(2, effect.GetFrame());
	calls.clear(); lights.clear();
	CHECK(!effect.Update());
	CHECK_EQ(12, effect.GetPixelZ());
	CHECK_EQ(0, effect.GetEndFrame());
	CHECK_EQ(104, effect.GetEndLinkFrame());
	CHECK_EQ(2, effect.GetFrame());
	CHECK_EQ(9, effect.GetLight());
	CHECK(calls == std::vector<int>({3}));
	CHECK(lights.empty());
}

TEST(FallingEffectGenerator, NegativeDestinationHeightKeepsTheSameFall)
{
	World world;
	MFallingEffectGenerator generator;
	auto info = Info(); info.z1 = -350;
	CHECK(generator.Generate(info));
	auto& effect = *effects.front();
	CHECK_EQ(-50, effect.GetPixelZ());
	CHECK(effect.Update()); CHECK_EQ(-150, effect.GetPixelZ());
	CHECK(effect.Update()); CHECK_EQ(-250, effect.GetPixelZ());
	CHECK(!effect.Update()); CHECK_EQ(-350, effect.GetPixelZ());
}

TEST(FallingEffectGenerator, LargeStepsRetainTheLinearArrivalTolerance)
{
	World world;
	MFallingEffectGenerator generator;
	auto info = Info(); info.step = 255;
	CHECK(generator.Generate(info));
	CHECK(!effects.front()->Update());
	CHECK_EQ(12, effects.front()->GetPixelZ());
	CHECK_EQ(0, effects.front()->GetFrame());
}

TEST(FallingEffectGenerator, ZeroSpeedAnimatesWithoutFallingOrArriving)
{
	World world;
	MFallingEffectGenerator generator;
	auto info = Info(); info.step = 0;
	CHECK(generator.Generate(info));
	for (int i = 0; i < 3; ++i) CHECK(effects.front()->Update());
	CHECK_EQ(312, effects.front()->GetPixelZ());
	CHECK_EQ(149, effects.front()->GetEndFrame());
	CHECK_EQ(0, effects.front()->GetFrame());
}

TEST(FallingEffectGenerator, ExpiredCountStopsBeforeAnyMovement)
{
	World world;
	MFallingEffectGenerator generator;
	auto info = Info(); info.count = 1;
	CHECK(generator.Generate(info));
	CHECK(!effects.front()->Update());
	CHECK_EQ(312, effects.front()->GetPixelZ());
	CHECK_EQ(0, effects.front()->GetFrame());
}

TEST(FallingEffectGenerator, HaloActionRetainsItsArrivalLifetimeCap)
{
	World world;
	MFallingEffectGenerator generator;
	auto info = Info(); info.nActionInfo = SKILL_HALO;
	CHECK(generator.Generate(info));
	CHECK(effects.front()->Update());
	CHECK(effects.front()->Update());
	CHECK(effects.front()->Update());
	CHECK_EQ(12, effects.front()->GetPixelZ());
	CHECK_EQ(108, effects.front()->GetEndFrame());
	CHECK_EQ(104, effects.front()->GetEndLinkFrame());
	CHECK_EQ(0, effects.front()->GetFrame());
	frameNow = 108;
	CHECK(!effects.front()->Update());
}

TEST(FallingEffectGenerator, MetadataRefreshesOnEveryCallAndFrameCountsNarrow)
{
	World world;
	MFallingEffectGenerator generator;
	CHECK(generator.Generate(Info()));
	sprite = {BLT_NORMAL, 23, 258};
	CHECK(generator.Generate(Info()));
	CHECK_EQ(12, effects[0]->GetFrameID());
	CHECK_EQ(23, effects[1]->GetFrameID());
	CHECK_EQ(2, effects[1]->GetMaxFrame());
	CHECK_EQ(BLT_NORMAL, effects[1]->GetBltType());
	const int light = effects[1]->GetLight();
	CHECK(effects[1]->Update());
	CHECK_EQ(light, effects[1]->GetLight());
}

TEST(FallingEffectGenerator, IndependentLinkCountAndPermanentLookingDurationRemainFinite)
{
	World world;
	MFallingEffectGenerator generator;
	auto info = Info(); info.count = 0xFFFF; info.linkCount = MAX_LINKCOUNT;
	CHECK(generator.Generate(info));
	CHECK_EQ(65634, effects.back()->GetEndFrame());
	CHECK_EQ(65634, effects.back()->GetEndLinkFrame());
	info.count = 10; info.linkCount = 20;
	CHECK(generator.Generate(info));
	CHECK_EQ(109, effects.back()->GetEndFrame());
	CHECK_EQ(119, effects.back()->GetEndLinkFrame());
}

TEST(FallingEffectGenerator, RejectionLeavesTheCallerOwningTheTarget)
{
	World world;
	MFallingEffectGenerator generator;
	auto target = std::make_unique<Target>(1);
	auto info = Info(); info.pEffectTarget = target.get();
	acceptQueue = false;
	CHECK(!generator.Generate(info));
	CHECK(effects.empty());
	CHECK(destroyedTargets.empty());
	CHECK(calls == std::vector<int>({1, 2, 3, 4}));
	target.reset();
	CHECK(destroyedTargets == std::vector<int>({1}));
}

TEST(FallingEffectGenerator, RejectingQueueDestroysTheSubmittedEffectAndItsOwnAttachedMarker)
{
	World world;
	MFallingEffectGenerator generator;
	const MFallingEffectHost rejecting{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			effect->SetLink(43, new Target(2));
			return false;
		},
	};
	MFallingEffectGenerator::SetHost(&rejecting);
	auto target = std::make_unique<Target>(1);
	auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!generator.Generate(info));
	CHECK(destroyedTargets == std::vector<int>({2}));
	target.reset();
	CHECK(destroyedTargets == std::vector<int>({2, 1}));
}

TEST(FallingEffectGenerator, QueueExceptionReleasesTheSubmittedEffectWithoutTakingTheCallerTarget)
{
	World world;
	MFallingEffectGenerator generator;
	const MFallingEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool {
			effect->SetLink(43, new Target(2));
			throw std::runtime_error("Queue failure");
		},
	};
	MFallingEffectGenerator::SetHost(&throwing);
	auto target = std::make_unique<Target>(1);
	auto info = Info(); info.pEffectTarget = target.get();
	bool threw = false;
	try { generator.Generate(info); }
	catch (const std::runtime_error&) { threw = true; }
	CHECK(threw);
	CHECK(destroyedTargets == std::vector<int>({2}));
	target.reset();
	CHECK(destroyedTargets == std::vector<int>({2, 1}));
}

TEST(FallingEffectGenerator, MissingMetadataRejectsBeforeConstructionOrSubmission)
{
	World world;
	MFallingEffectGenerator generator;
	spriteAvailable = false;
	CHECK(!generator.Generate(Info()));
	CHECK(calls == std::vector<int>({1}));
	CHECK(effects.empty());
	CHECK(lights.empty());
}

TEST(FallingEffectGenerator, MissingQueueReleasesTheUnlinkedEffectAndKeepsTheCallerTarget)
{
	World world;
	MFallingEffectGenerator generator;
	const MFallingEffectHost spriteOnly{.Sprite = host.Sprite};
	MFallingEffectGenerator::SetHost(&spriteOnly);
	auto target = std::make_unique<Target>(1);
	auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!generator.Generate(info));
	CHECK(calls == std::vector<int>({1, 2, 3}));
	CHECK(effects.empty());
	CHECK(destroyedTargets.empty());
}

TEST(FallingEffectGenerator, AbsentOrPartialMetadataHostsLeaveTheCallerTargetUntouched)
{
	World world;
	MFallingEffectGenerator generator;
	const MFallingEffectHost empty{};
	const MFallingEffectHost queueOnly{.Queue = host.Queue};
	for (const auto* service : {static_cast<const MFallingEffectHost*>(nullptr), &empty, &queueOnly})
	{
		MFallingEffectGenerator::SetHost(service);
		auto target = std::make_unique<Target>(1);
		auto info = Info(); info.pEffectTarget = target.get();
		const auto destroyed = destroyedTargets.size();
		CHECK(!generator.Generate(info));
		CHECK_EQ(destroyed, destroyedTargets.size());
		CHECK(effects.empty());
	}
	CHECK(calls.empty());
}

TEST(FallingEffectGenerator, SpriteCallbackCanReplaceTheSubmissionService)
{
	World world;
	MFallingEffectGenerator generator;
	const MFallingEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MFallingEffectSprite& result) {
			result = {BLT_NORMAL, 20, 2};
			MFallingEffectGenerator::SetHost(&host);
			return true;
		},
	};
	CHECK(MFallingEffectGenerator::SetHost(&changing) == &host);
	CHECK(generator.Generate(Info()));
	CHECK_EQ(BLT_NORMAL, effects.front()->GetBltType());
	CHECK_EQ(20, effects.front()->GetFrameID());
	CHECK_EQ(2, effects.front()->GetMaxFrame());
	CHECK(calls == std::vector<int>({2, 3, 4}));
	CHECK(MFallingEffectGenerator::SetHost(&changing) == &host);
}

TEST(FallingEffectGenerator, ClockCallbackCanRemoveTheQueueBeforeSubmission)
{
	World world;
	MFallingEffectGenerator generator;
	const MEffectHost changing{
		.CurrentFrame = []() { MFallingEffectGenerator::SetHost(nullptr); return frameNow; },
		.Light = effectHost.Light,
	};
	MEffect::SetHost(&changing);
	CHECK(!generator.Generate(Info()));
	CHECK(effects.empty());
	CHECK(calls == std::vector<int>({1, 2}));
}

TEST(FallingEffectGenerator, MissingBaseServicesKeepTheirStoredCountAndInactiveFallback)
{
	World world;
	MFallingEffectGenerator generator;
	MEffect::SetHost(nullptr);
	CHECK(generator.Generate(Info()));
	CHECK_EQ(49, effects.front()->GetEndFrame());
	CHECK_EQ(4, effects.front()->GetEndLinkFrame());
	CHECK_EQ(0, effects.front()->GetLight());
	CHECK(!effects.front()->Update());
	CHECK_EQ(312, effects.front()->GetPixelZ());
}

TEST(FallingEffectGenerator, FiniteCountsRetainTheirAbsoluteClockWrap)
{
	World world;
	MFallingEffectGenerator generator;
	frameNow = (std::numeric_limits<DWORD>::max)() - 1;
	auto info = Info(); info.count = 4;
	CHECK(generator.Generate(info));
	CHECK_EQ(1, effects.front()->GetEndFrame());
	CHECK(!effects.front()->Update());
	frameNow = 0;
	CHECK(effects.front()->Update());
	CHECK_EQ(212, effects.front()->GetPixelZ());
}

TEST(FallingEffectGenerator, HeightAboveTheIntegerRangeClampsBeforeFloatStorage)
{
	World world;
	MFallingEffectGenerator generator;
	const int top = (std::numeric_limits<int>::max)();
	for (int destination : {top, top - 1, top - 299})
	{
		auto info = Info(); info.z1 = destination;
		CHECK(generator.Generate(info));
		CHECK_EQ(top, effects.back()->GetPixelZ());
		CHECK_EQ(96, effects.back()->GetPixelX());
		CHECK_EQ(48, effects.back()->GetPixelY());
	}
}

TEST(FallingEffectGenerator, SaturatedStartStillReachesANearbyRepresentableDestination)
{
	World world;
	MFallingEffectGenerator generator;
	const int top = (std::numeric_limits<int>::max)();
	auto info = Info(); info.z1 = top - 127; info.step = 255;
	CHECK(generator.Generate(info));
	CHECK_EQ(top, effects.front()->GetPixelZ());
	CHECK(!effects.front()->Update());
	CHECK_EQ(top - 127, effects.front()->GetPixelZ());
	CHECK_EQ(0, effects.front()->GetEndFrame());
	CHECK_EQ(0, effects.front()->GetFrame());
}

TEST(FallingEffectGenerator, RepresentableHeightAdditionKeepsExistingFloatRounding)
{
	World world;
	MFallingEffectGenerator generator;
	const int top = (std::numeric_limits<int>::max)();
	const int bottom = (std::numeric_limits<int>::min)();
	struct Height { int destination, expected; };
	for (const Height height : {Height{top - 300, top}, {top - 811, top - 511},
		{bottom, bottom + 256}, {-300, 0}, {-1, 299}, {0, 300}, {1, 301}})
	{
		auto info = Info(); info.z1 = height.destination;
		CHECK(generator.Generate(info));
		CHECK_EQ(height.expected, effects.back()->GetPixelZ());
	}
}

TEST(FallingEffectGenerator, SaturatedStartPreservesTargetOwnershipAndLinkTiming)
{
	World world;
	MFallingEffectGenerator generator;
	auto target = std::make_unique<Target>(1);
	auto info = Info(); info.z1 = (std::numeric_limits<int>::max)();
	info.pEffectTarget = target.get();
	CHECK(generator.Generate(info));
	target.release();
	CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget);
	CHECK_EQ(149, effects.front()->GetEndFrame());
	CHECK_EQ(104, effects.front()->GetEndLinkFrame());
	CHECK(!effects.front()->Update());
	CHECK_EQ(info.z1, effects.front()->GetPixelZ());
	CHECK_EQ(104, effects.front()->GetEndLinkFrame());
	effects.clear();
	CHECK(destroyedTargets == std::vector<int>({1}));
}
