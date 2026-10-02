#include "test_framework.h"
#include "MLinearEffect.h"
#include "SkillDef.h"

#include <limits>
#include <vector>

namespace {

DWORD frameNow;
int lightValue;
std::vector<int> calls;
struct LightRequest
{
	BYTE blt;
	TYPE_FRAMEID id;
	BYTE direction, frame;
	bool operator==(const LightRequest&) const = default;
};
std::vector<LightRequest> lights;
MLinearEffect* observed;
int lightSector;
const MEffectHost host{
	.CurrentFrame = []() { calls.push_back(1); return frameNow; },
	.Light = [](BYTE blt, TYPE_FRAMEID id, BYTE direction, BYTE frame) {
		calls.push_back(2);
		lights.push_back({blt, id, direction, frame});
		if (observed) lightSector = observed->GetX();
		return lightValue;
	},
};
const MEffectHost expiredHost{
	.CurrentFrame = []() -> DWORD { return 10000; },
	.Light = host.Light,
};

struct World
{
	const MEffectHost* previous = MEffect::SetHost(&host);
	World()
	{
		frameNow = 100; lightValue = 7; calls.clear(); lights.clear();
		observed = nullptr; lightSector = -1;
	}
	~World() { observed = nullptr; MEffect::SetHost(previous); }
};

struct EffectProbe : MLinearEffect
{
	using MLinearEffect::MLinearEffect;
	using MEffect::LimitRemainingFrames;
	float Length() const { return GetPathLength(); }
	void SetPixels(float x, float y, float z)
	{
		m_PixelX = x; m_PixelY = y; m_PixelZ = z;
	}
};

void Initialize(MLinearEffect& effect)
{
	effect.SetFrameID(12, 3);
	effect.SetPixelPosition(0, 0, 0);
	effect.SetCount(100);
}

} // namespace

TEST(LinearEffect, DefaultStateIsAnExpiredStationaryLinearEffect)
{
	World world;
	EffectProbe effect(BLT_EFFECT);
	CHECK_EQ(MEffect::EFFECT_LINEAR, effect.GetEffectType());
	CHECK_EQ(MObject::TYPE_EFFECT, effect.GetObjectType());
	CHECK_EQ(0, effect.GetStepPixel());
	CHECK_EQ(0, effect.Length());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(ACTIONINFO_NULL, effect.GetActionInfo());
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetFrame());
	CHECK(lights.empty());
}

TEST(LinearEffect, TargetSelectionCalculatesThreeDimensionalSpeedAndFacing)
{
	World world;
	EffectProbe effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTarget(3, 4, 12, 13);
	CHECK_EQ(13, effect.GetStepPixel());
	CHECK_EQ(13, effect.Length());
	CHECK_EQ(DIRECTION_RIGHTDOWN, effect.GetDirection());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(0, effect.GetPixelY());
	CHECK_EQ(0, effect.GetPixelZ());
	CHECK(!effect.Update());
	CHECK_EQ(3, effect.GetPixelX());
	CHECK_EQ(4, effect.GetPixelY());
	CHECK_EQ(12, effect.GetPixelZ());
}

TEST(LinearEffect, TargetSelectionTruncatesCurrentSubpixelsForFacing)
{
	World world;
	EffectProbe effect(BLT_EFFECT);
	effect.SetPixels(-0.75f, 0.75f, 0);
	effect.SetTarget(0, 0, 0, 1);
	CHECK_EQ(DIRECTION_DOWN, effect.GetDirection());
	effect.SetTarget(-1, -1, 0, 1);
	CHECK_EQ(DIRECTION_LEFTUP, effect.GetDirection());
}

TEST(LinearEffect, ActiveMovementProjectsPositionThenAnimatesAndReadsLight)
{
	World world;
	MLinearEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTarget(192, 0, 0, 48);
	observed = &effect;
	calls.clear(); lights.clear();
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(1, effect.GetX());
	CHECK_EQ(1, lightSector);
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(7, effect.GetLight());
	CHECK(calls == std::vector<int>({1, 2}));
	CHECK(lights == std::vector<LightRequest>({{BLT_EFFECT, 12, DIRECTION_RIGHT, 1}}));
}

TEST(LinearEffect, ArrivalIsStrictAndStopsBeforeProjectionAnimationAndLighting)
{
	World world;
	MLinearEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTarget(96, 0, 0, 48);
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(1, effect.GetX());
	CHECK_EQ(1, effect.GetFrame());
	calls.clear(); lights.clear();
	CHECK(!effect.Update());
	CHECK_EQ(96, effect.GetPixelX());
	CHECK_EQ(1, effect.GetX());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(0, effect.GetEndFrame());
	CHECK_EQ(199, effect.GetEndLinkFrame());
	CHECK(calls == std::vector<int>({1}));
	CHECK(lights.empty());
	CHECK(!effect.Update());
}

TEST(LinearEffect, ArrivalClearsVelocityBeforeALaterLifetimeRestart)
{
	World world;
	MLinearEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTarget(3, 0, 0, 3);
	CHECK(!effect.Update());
	effect.SetCount(100);
	effect.SetLink(SKILL_HALO, nullptr);
	CHECK(effect.Update());
	CHECK_EQ(3, effect.GetPixelX());
	CHECK_EQ(108, effect.GetEndFrame());
}

TEST(LinearEffect, ExpiredEffectsDoNotMoveOrRefreshTheirFrame)
{
	World world;
	MLinearEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTarget(192, 0, 0, 48);
	frameNow = 199;
	calls.clear(); lights.clear();
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(0, effect.GetX());
	CHECK_EQ(0, effect.GetFrame());
	CHECK(calls == std::vector<int>({1}));
	CHECK(lights.empty());
}

TEST(LinearEffect, ZeroSpeedNeverSatisfiesTheStrictArrivalRadius)
{
	World world;
	MLinearEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTarget(0, 0, 0, 0);
	CHECK(effect.Update());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(199, effect.GetEndFrame());
	effect.SetTarget(100, 100, 100, 0);
	CHECK(effect.Update());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(0, effect.GetPixelY());
	CHECK_EQ(0, effect.GetPixelZ());
}

TEST(LinearEffect, CoincidentTargetWithPositiveSpeedArrivesImmediately)
{
	World world;
	MLinearEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTarget(0, 0, 0, 1);
	CHECK_EQ(DIRECTION_DOWN, effect.GetDirection());
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetEndFrame());
	CHECK_EQ(0, effect.GetFrame());
}

TEST(LinearEffect, RetargetingUsesTheCurrentPositionAndReplacesSpeedAndDirection)
{
	World world;
	MLinearEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTarget(192, 0, 0, 48);
	CHECK(effect.Update());
	effect.SetTarget(0, 0, 0, 12);
	CHECK_EQ(12, effect.GetStepPixel());
	CHECK_EQ(DIRECTION_LEFT, effect.GetDirection());
	CHECK(effect.Update());
	CHECK_EQ(36, effect.GetPixelX());
	CHECK_EQ(0, effect.GetX());
}

TEST(LinearEffect, ChangingStepPixelChangesArrivalRadiusWithoutRecomputingVelocity)
{
	World world;
	MLinearEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTarget(96, 0, 0, 24);
	effect.SetStepPixel(1);
	CHECK(effect.Update());
	CHECK_EQ(24, effect.GetPixelX());
	effect.SetStepPixel(100);
	CHECK(!effect.Update());
	CHECK_EQ(96, effect.GetPixelX());
}

TEST(LinearEffect, NonAlphaUpdatesPreserveTheirExistingLight)
{
	World world;
	for (const auto blt : {BLT_NORMAL, BLT_SHADOW, BLT_SCREEN})
	{
		MLinearEffect effect(static_cast<BYTE>(blt));
		Initialize(effect);
		effect.SetTarget(192, 0, 0, 48);
		effect.SetLight(19);
		calls.clear(); lights.clear();
		CHECK(effect.Update());
		CHECK_EQ(19, effect.GetLight());
		CHECK_EQ(1, effect.GetFrame());
		CHECK(calls == std::vector<int>({1}));
		CHECK(lights.empty());
	}
}

TEST(LinearEffect, HaloArrivalContinuesAnimationAndCapsLifetimeAtNowPlusEight)
{
	World world;
	MLinearEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetLink(SKILL_HALO, nullptr);
	effect.SetTarget(48, 0, 0, 48);
	calls.clear(); lights.clear();
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(1, effect.GetX());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(108, effect.GetEndFrame());
	CHECK_EQ(199, effect.GetEndLinkFrame());
	CHECK(calls == std::vector<int>({1, 1, 2}));
	for (frameNow = 101; frameNow < 108; ++frameNow)
	{
		CHECK(effect.Update());
		CHECK_EQ(108, effect.GetEndFrame());
		CHECK_EQ(48, effect.GetPixelX());
	}
	CHECK(!effect.Update());
}

TEST(LinearEffect, HaloArrivalDoesNotExtendAnEarlierDeadline)
{
	World world;
	for (DWORD count : {DWORD{2}, DWORD{5}, DWORD{9}})
	{
		MLinearEffect effect(BLT_EFFECT);
		Initialize(effect);
		effect.SetCount(count);
		effect.SetLink(SKILL_HALO, nullptr);
		effect.SetTarget(0, 0, 0, 1);
		CHECK(effect.Update());
		CHECK_EQ(100 + count - 1, effect.GetEndFrame());
		CHECK_EQ(100 + count - 1, effect.GetEndLinkFrame());
	}
}

TEST(LinearEffect, OtherActionsUseOrdinaryArrivalIncludingHaloAttack)
{
	World world;
	for (const auto action : {SKILL_CLIENT_HALO_ATTACK, SKILL_CANNONADE})
	{
		MLinearEffect effect(BLT_EFFECT);
		Initialize(effect);
		effect.SetLink(static_cast<TYPE_ACTIONINFO>(action), nullptr);
		effect.SetTarget(0, 0, 0, 1);
		CHECK(!effect.Update());
		CHECK_EQ(0, effect.GetEndFrame());
		CHECK_EQ(0, effect.GetFrame());
	}
}

TEST(LinearEffect, HaloDeadlineCapRetainsUnsignedWrap)
{
	World world;
	MLinearEffect effect(BLT_EFFECT);
	Initialize(effect);
	frameNow = (std::numeric_limits<DWORD>::max)() - 4;
	effect.SetCount(5);
	effect.SetLink(SKILL_HALO, nullptr);
	effect.SetTarget(0, 0, 0, 1);
	CHECK(effect.Update());
	CHECK_EQ(3, effect.GetEndFrame());
	CHECK_EQ((std::numeric_limits<DWORD>::max)(), effect.GetEndLinkFrame());
	CHECK(effect.IsEnd());
	CHECK(!effect.Update());
	frameNow = 0;
	CHECK(effect.Update());
	CHECK_EQ(3, effect.GetEndFrame());
	frameNow = 3;
	CHECK(!effect.Update());
}

TEST(LinearEffect, MissingClockStopsMovementAndPartialHostsGuardLighting)
{
	World world;
	MLinearEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTarget(192, 0, 0, 48);
	MEffect::SetHost(nullptr);
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(0, effect.GetFrame());
	const MEffectHost lightOnly{.CurrentFrame = nullptr, .Light = host.Light};
	MEffect::SetHost(&lightOnly);
	CHECK(!effect.Update());
	const MEffectHost clockOnly{.CurrentFrame = host.CurrentFrame};
	MEffect::SetHost(&clockOnly);
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(0, effect.GetLight());
}

TEST(LinearEffect, ExistingEffectsObserveHostReplacement)
{
	World world;
	MLinearEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTarget(192, 0, 0, 48);
	MEffect::SetHost(&expiredHost);
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetPixelX());
	MEffect::SetHost(&host);
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
}

TEST(LinearEffect, LifetimeCapHandlesMissingClocksWithoutADeferredDeadline)
{
	World world;
	EffectProbe effect(BLT_EFFECT);
	Initialize(effect);
	MEffect::SetHost(nullptr);
	effect.LimitRemainingFrames(8);
	CHECK_EQ(0, effect.GetEndFrame());
	CHECK_EQ(199, effect.GetEndLinkFrame());
	MEffect::SetHost(&host);
	CHECK(!effect.Update());
}

TEST(LinearEffect, LinkDelayAndWaitDeadlinesDoNotControlMovement)
{
	World world;
	MLinearEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetCount(20, 1);
	effect.SetDelayFrame(100);
	effect.SetWaitFrame(100);
	effect.SetDrawSkip(true);
	effect.SetTarget(192, 0, 0, 48);
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK(effect.IsDelayFrame());
	CHECK(effect.IsWaitFrame());
	CHECK(effect.IsSkipDraw());
	CHECK_EQ(100, effect.GetEndLinkFrame());
}

TEST(LinearEffect, TargetFacingUsesBoundedStoredPixels)
{
	World world;
	EffectProbe effect(BLT_EFFECT);
	effect.SetPixels(std::numeric_limits<float>::infinity(),
		-std::numeric_limits<float>::infinity(), 0);
	effect.SetTarget(0, 0, 0, 1);
	CHECK_EQ(DIRECTION_LEFTDOWN, effect.GetDirection());
	effect.SetPixels((std::numeric_limits<float>::max)(), 0, 0);
	effect.SetTarget(0, 0, 0, 1);
	CHECK_EQ(DIRECTION_LEFT, effect.GetDirection());
	const float nan = std::numeric_limits<float>::quiet_NaN();
	effect.SetPixels(nan, nan, 0);
	effect.SetTarget(100, 0, 0, 1);
	CHECK_EQ(DIRECTION_RIGHT, effect.GetDirection());
}

TEST(LinearEffect, DisplayOffsetsDoNotChangeStoredPositionProjectionOrFacing)
{
	World world;
	struct DisplayEffect : MLinearEffect
	{
		DisplayEffect() : MLinearEffect(BLT_EFFECT) {}
		int GetPixelX() const override { return 1000; }
		int GetPixelY() const override { return 1000; }
	} effect;
	effect.SetPixelPosition(0, 0, 0);
	CHECK_EQ(1000, effect.GetPixelX());
	CHECK_EQ(1000, effect.GetPixelY());
	CHECK_EQ(0, effect.GetX());
	CHECK_EQ(0, effect.GetY());
	effect.SetTarget(0, 100, 0, 1);
	CHECK_EQ(DIRECTION_DOWN, effect.GetDirection());
}
