#include "test_framework.h"
#include "MGuidanceEffect.h"
#include "MChaseEffect.h"
#include "SkillDef.h"

#include <limits>
#include <vector>

namespace {

DWORD frameNow;
bool creatureExists;
TYPE_OBJECTID creatureID;
int creatureX, creatureY, creatureZ, lightValue;
std::vector<int> calls;
std::vector<TYPE_OBJECTID> lookups;
struct LightRequest
{
	BYTE blt;
	TYPE_FRAMEID id;
	BYTE direction, frame;
	bool operator==(const LightRequest&) const = default;
};
std::vector<LightRequest> lights;
MEffect* observed;
int lightPixelX, lightSectorX;

const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(1); return frameNow; },
	.Light = [](BYTE blt, TYPE_FRAMEID id, BYTE direction, BYTE frame) {
		calls.push_back(3);
		lights.push_back({blt, id, direction, frame});
		if (observed)
		{
			lightPixelX = observed->GetPixelX();
			lightSectorX = observed->GetX();
		}
		return lightValue;
	},
};
const MGuidanceEffectHost guidanceHost{
	.CreaturePosition = [](TYPE_OBJECTID id, int& x, int& y, int& z) {
		calls.push_back(2);
		lookups.push_back(id);
		if (!creatureExists || id != creatureID) return false;
		x = creatureX; y = creatureY; z = creatureZ;
		return true;
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MGuidanceEffectHost* previousGuidance = MGuidanceEffect::SetHost(&guidanceHost);
	World()
	{
		frameNow = 100; creatureExists = true; creatureID = 42;
		creatureX = 192; creatureY = creatureZ = 0; lightValue = 7;
		calls.clear(); lookups.clear(); lights.clear();
		observed = nullptr; lightPixelX = lightSectorX = -1;
	}
	~World()
	{
		observed = nullptr;
		MGuidanceEffect::SetHost(previousGuidance);
		MEffect::SetHost(previousEffect);
	}
};

void Initialize(MGuidanceEffect& effect)
{
	effect.SetFrameID(12, 3);
	effect.SetPixelPosition(0, 0, 0);
	effect.SetCount(100);
	effect.SetStepPixel(48);
}

void ClearCalls()
{
	calls.clear(); lookups.clear(); lights.clear();
}

} // namespace

TEST(GuidanceChase, GuidanceDefaultsToAnExpiredEffectWithoutATarget)
{
	World world;
	MGuidanceEffect effect(BLT_EFFECT);
	CHECK_EQ(MEffect::EFFECT_GUIDANCE, effect.GetEffectType());
	CHECK_EQ(OBJECTID_NULL, effect.GetTraceCreatureID());
	CHECK_EQ(0, effect.GetStepPixel());
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetFrame());
	CHECK(lookups.empty());
	CHECK(lights.empty());
}

TEST(GuidanceChase, DefaultChaseAnimatesWithoutALifetimeOrClock)
{
	World world;
	MChaseEffect effect(BLT_EFFECT);
	effect.SetFrameID(12, 3);
	MEffect::SetHost(nullptr);
	ClearCalls();
	CHECK_EQ(MEffect::EFFECT_CHASE, effect.GetEffectType());
	CHECK_EQ(OBJECTID_NULL, effect.GetTraceCreatureID());
	CHECK(!effect.IsChaseOver());
	CHECK(effect.IsEnd());
	CHECK(effect.Update());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(0, effect.GetLight());
	CHECK(!effect.IsChaseOver());
	CHECK(calls.empty());
}

TEST(GuidanceChase, NonNullTraceSetterImmediatelyReadsAllThreeCoordinates)
{
	World world;
	MGuidanceEffect effect(BLT_EFFECT);
	Initialize(effect);
	creatureX = 3; creatureY = 4; creatureZ = 12;
	effect.SetStepPixel(13);
	ClearCalls();
	effect.SetTraceCreatureID(creatureID);
	CHECK(lookups == std::vector<TYPE_OBJECTID>({42}));
	CHECK(calls == std::vector<int>({2}));
	CHECK_EQ(DIRECTION_RIGHTDOWN, effect.GetDirection());
	CHECK_EQ(0, effect.GetPixelZ());
	CHECK_EQ(199, effect.GetEndFrame());
	CHECK(!effect.Update());
	CHECK_EQ(3, effect.GetPixelX());
	CHECK_EQ(4, effect.GetPixelY());
	CHECK_EQ(12, effect.GetPixelZ());
}

TEST(GuidanceChase, EachGuidanceUpdateRetracesBeforeMovingAndLighting)
{
	World world;
	MGuidanceEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTraceCreatureID(creatureID);
	observed = &effect;
	ClearCalls();
	CHECK(effect.Update());
	CHECK(calls == std::vector<int>({1, 2, 3}));
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(48, lightPixelX);
	CHECK_EQ(1, lightSectorX);
	CHECK_EQ(7, effect.GetLight());
	CHECK(lights == std::vector<LightRequest>({{BLT_EFFECT, 12, DIRECTION_RIGHT, 1}}));
	creatureX = 48; creatureY = 192;
	ClearCalls();
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(48, effect.GetPixelY());
	CHECK_EQ(DIRECTION_DOWN, effect.GetDirection());
	CHECK_EQ(2, effect.GetFrame());
	CHECK(calls == std::vector<int>({1, 2, 3}));
}

TEST(GuidanceChase, SpeedSetAfterTheCreatureIDTakesEffectOnTheNextTrace)
{
	World world;
	MGuidanceEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetStepPixel(0);
	effect.SetTraceCreatureID(creatureID);
	effect.SetStepPixel(48);
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(48, effect.GetStepPixel());
}

TEST(GuidanceChase, GuidanceArrivalStopsBeforeProjectionAnimationAndLight)
{
	World world;
	MGuidanceEffect effect(BLT_EFFECT);
	Initialize(effect);
	creatureX = 96;
	effect.SetTraceCreatureID(creatureID);
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(1, effect.GetX());
	CHECK_EQ(1, effect.GetFrame());
	ClearCalls();
	CHECK(!effect.Update());
	CHECK_EQ(96, effect.GetPixelX());
	CHECK_EQ(1, effect.GetX());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(0, effect.GetEndFrame());
	CHECK_EQ(199, effect.GetEndLinkFrame());
	CHECK_EQ(42, effect.GetTraceCreatureID());
	CHECK(calls == std::vector<int>({1, 2}));
	CHECK(lights.empty());
}

TEST(GuidanceChase, ExpiryPreventsCreatureLookupMovementAndAnimation)
{
	World world;
	MGuidanceEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTraceCreatureID(creatureID);
	effect.SetLight(19);
	frameNow = 199;
	ClearCalls();
	CHECK(!effect.Update());
	CHECK_EQ(42, effect.GetTraceCreatureID());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(0, effect.GetFrame());
	CHECK_EQ(19, effect.GetLight());
	CHECK(calls == std::vector<int>({1}));
	CHECK(lookups.empty());
}

TEST(GuidanceChase, MissingCreatureInTheSetterClearsOnlyTraceAndEndDeadline)
{
	World world;
	MGuidanceEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTarget(192, 0, 0, 48);
	creatureExists = false;
	ClearCalls();
	effect.SetTraceCreatureID(creatureID);
	CHECK_EQ(OBJECTID_NULL, effect.GetTraceCreatureID());
	CHECK_EQ(0, effect.GetEndFrame());
	CHECK_EQ(199, effect.GetEndLinkFrame());
	CHECK_EQ(DIRECTION_RIGHT, effect.GetDirection());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK(calls == std::vector<int>({2}));
}

TEST(GuidanceChase, LosingACreatureDuringUpdateStopsBeforeChangingTheFrame)
{
	World world;
	for (bool chase : {false, true})
	{
		MGuidanceEffect guidance(BLT_EFFECT);
		MChaseEffect persistent(BLT_EFFECT);
		MGuidanceEffect& effect = chase ? static_cast<MGuidanceEffect&>(persistent) : guidance;
		Initialize(effect);
		creatureExists = true;
		effect.SetTraceCreatureID(creatureID);
		CHECK(effect.Update());
		creatureExists = false;
		ClearCalls();
		CHECK(!effect.Update());
		CHECK_EQ(OBJECTID_NULL, effect.GetTraceCreatureID());
		CHECK_EQ(0, effect.GetEndFrame());
		CHECK_EQ(199, effect.GetEndLinkFrame());
		CHECK_EQ(48, effect.GetPixelX());
		CHECK_EQ(1, effect.GetFrame());
		CHECK_EQ(7, effect.GetLight());
		CHECK(lookups == std::vector<TYPE_OBJECTID>({42}));
		CHECK(lights.empty());
	}
}

TEST(GuidanceChase, NullTraceSetterSkipsLookupButActiveGuidanceStillLooksItUp)
{
	World world;
	MGuidanceEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTraceCreatureID(creatureID);
	ClearCalls();
	effect.SetTraceCreatureID(OBJECTID_NULL);
	CHECK(calls.empty());
	CHECK_EQ(199, effect.GetEndFrame());
	CHECK(!effect.Update());
	CHECK(lookups == std::vector<TYPE_OBJECTID>({OBJECTID_NULL}));
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(0, effect.GetEndFrame());
}

TEST(GuidanceChase, MissingClockStopsGuidanceBeforeReadingTheCreature)
{
	World world;
	MGuidanceEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTraceCreatureID(creatureID);
	MEffect::SetHost(nullptr);
	ClearCalls();
	CHECK(!effect.Update());
	CHECK_EQ(42, effect.GetTraceCreatureID());
	CHECK_EQ(199, effect.GetEndFrame());
	CHECK(calls.empty());
	const MEffectHost lightOnly{.Light = effectHost.Light};
	MEffect::SetHost(&lightOnly);
	CHECK(!effect.Update());
	CHECK(calls.empty());
}

TEST(GuidanceChase, MissingLookupHostOrEntryBehavesLikeAMissingCreature)
{
	World world;
	const MGuidanceEffectHost empty{};
	for (const auto* host : {static_cast<const MGuidanceEffectHost*>(nullptr), &empty})
	{
		MGuidanceEffect effect(BLT_EFFECT);
		Initialize(effect);
		MGuidanceEffect::SetHost(&guidanceHost);
		effect.SetTraceCreatureID(creatureID);
		MGuidanceEffect::SetHost(host);
		ClearCalls();
		CHECK(!effect.Update());
		CHECK_EQ(OBJECTID_NULL, effect.GetTraceCreatureID());
		CHECK_EQ(0, effect.GetEndFrame());
		CHECK_EQ(0, effect.GetPixelX());
		CHECK(lookups.empty());
		CHECK(lights.empty());
	}
}

TEST(GuidanceChase, ExistingEffectsReadAReplacementLookupHost)
{
	World world;
	MGuidanceEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTraceCreatureID(creatureID);
	const MGuidanceEffectHost opposite{
		.CreaturePosition = [](TYPE_OBJECTID id, int& x, int& y, int& z) {
			CHECK_EQ(42, id);
			x = -192; y = z = 0;
			return true;
		},
	};
	CHECK(MGuidanceEffect::SetHost(&opposite) == &guidanceHost);
	CHECK(effect.Update());
	CHECK_EQ(-48, effect.GetPixelX());
	CHECK_EQ(DIRECTION_LEFT, effect.GetDirection());
	CHECK(MGuidanceEffect::SetHost(&guidanceHost) == &opposite);
	CHECK(effect.Update());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(DIRECTION_RIGHT, effect.GetDirection());
}

TEST(GuidanceChase, FailedLookupDoesNotApplyItsPartialCoordinates)
{
	World world;
	MGuidanceEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTraceCreatureID(creatureID);
	const MGuidanceEffectHost failing{
		.CreaturePosition = [](TYPE_OBJECTID, int& x, int& y, int& z) {
			x = -100; y = 200; z = 300;
			return false;
		},
	};
	MGuidanceEffect::SetHost(&failing);
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(0, effect.GetPixelY());
	CHECK_EQ(0, effect.GetPixelZ());
	CHECK_EQ(DIRECTION_RIGHT, effect.GetDirection());
}

TEST(GuidanceChase, TraceUsesTheLinearSetterInsteadOfADerivedDisplayPolicy)
{
	World world;
	struct Effect : MGuidanceEffect
	{
		Effect() : MGuidanceEffect(BLT_EFFECT) {}
		bool called = false;
		void SetTarget(int, int, int, WORD) override { called = true; }
	} effect;
	Initialize(effect);
	effect.SetTraceCreatureID(creatureID);
	CHECK(!effect.called);
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK(!effect.called);
}

TEST(GuidanceChase, CreatureSetterAndUpdatesKeepVirtualTraceDispatch)
{
	World world;
	struct Effect : MGuidanceEffect
	{
		Effect() : MGuidanceEffect(BLT_EFFECT) {}
		int traces = 0;
		bool TraceCreature() override { ++traces; return false; }
	} effect;
	Initialize(effect);
	effect.SetTraceCreatureID(creatureID);
	CHECK_EQ(1, effect.traces);
	CHECK(!effect.Update());
	CHECK_EQ(2, effect.traces);
	CHECK_EQ(42, effect.GetTraceCreatureID());
	CHECK_EQ(199, effect.GetEndFrame());
	CHECK(lookups.empty());
}

TEST(GuidanceChase, GuidanceDoesNotUseTheLinearHaloArrivalExtension)
{
	World world;
	MGuidanceEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetLink(SKILL_HALO, nullptr);
	creatureX = 0;
	effect.SetTraceCreatureID(creatureID);
	ClearCalls();
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetEndFrame());
	CHECK_EQ(0, effect.GetFrame());
	CHECK(lights.empty());
}

TEST(GuidanceChase, ChaseIgnoresExpiredLifetimeAndMovesBeforeLighting)
{
	World world;
	MChaseEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTraceCreatureID(creatureID);
	frameNow = 10000;
	observed = &effect;
	ClearCalls();
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(48, lightPixelX);
	CHECK_EQ(1, lightSectorX);
	CHECK(!effect.IsChaseOver());
	CHECK_EQ(199, effect.GetEndFrame());
	CHECK(calls == std::vector<int>({2, 3}));
	CHECK(lights == std::vector<LightRequest>({{BLT_EFFECT, 12, DIRECTION_RIGHT, 1}}));
}

TEST(GuidanceChase, ChaseArrivalAnimatesButSkipsProjectionAndLight)
{
	World world;
	MChaseEffect effect(BLT_EFFECT);
	Initialize(effect);
	creatureX = 96;
	effect.SetTraceCreatureID(creatureID);
	CHECK(effect.Update());
	CHECK(!effect.IsChaseOver());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(1, effect.GetX());
	ClearCalls();
	CHECK(effect.Update());
	CHECK(effect.IsChaseOver());
	CHECK_EQ(96, effect.GetPixelX());
	CHECK_EQ(1, effect.GetX());
	CHECK_EQ(2, effect.GetFrame());
	CHECK_EQ(199, effect.GetEndFrame());
	CHECK(calls == std::vector<int>({2}));
	CHECK(lights.empty());
	CHECK(effect.Update());
	CHECK(effect.IsChaseOver());
	CHECK_EQ(0, effect.GetFrame());
	CHECK_EQ(1, effect.GetX());
}

TEST(GuidanceChase, ChaseResumesWhenAnArrivedCreatureMovesAway)
{
	World world;
	MChaseEffect effect(BLT_EFFECT);
	Initialize(effect);
	creatureX = 48;
	effect.SetTraceCreatureID(creatureID);
	CHECK(effect.Update());
	CHECK(effect.IsChaseOver());
	creatureX = 240;
	CHECK(effect.Update());
	CHECK(!effect.IsChaseOver());
	CHECK_EQ(96, effect.GetPixelX());
	CHECK_EQ(2, effect.GetX());
	CHECK_EQ(2, effect.GetFrame());
	CHECK_EQ(7, effect.GetLight());
}

TEST(GuidanceChase, ClearingChaseCreatureRetainsItsLastTrajectory)
{
	World world;
	MChaseEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTraceCreatureID(creatureID);
	CHECK(effect.Update());
	effect.SetTraceCreatureID(OBJECTID_NULL);
	ClearCalls();
	CHECK(effect.Update());
	CHECK_EQ(96, effect.GetPixelX());
	CHECK_EQ(2, effect.GetX());
	CHECK(!effect.IsChaseOver());
	CHECK(calls == std::vector<int>({3}));
	CHECK(lookups.empty());
}

TEST(GuidanceChase, ChaseCanFlyToAnExplicitDestinationWithoutACreature)
{
	World world;
	MChaseEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTraceCreatureID(creatureID);
	effect.SetTraceCreatureID(OBJECTID_NULL);
	effect.SetTarget(-96, 0, 0, 48);
	ClearCalls();
	CHECK(effect.Update());
	CHECK_EQ(-48, effect.GetPixelX());
	CHECK_EQ(DIRECTION_LEFT, effect.GetDirection());
	CHECK(!effect.IsChaseOver());
	CHECK(effect.Update());
	CHECK_EQ(-96, effect.GetPixelX());
	CHECK(effect.IsChaseOver());
	CHECK(lookups.empty());
}

TEST(GuidanceChase, ZeroSpeedKeepsBothEffectsActiveEvenAtTheirTarget)
{
	World world;
	MGuidanceEffect guidance(BLT_EFFECT);
	MChaseEffect chase(BLT_EFFECT);
	creatureX = 0;
	for (MGuidanceEffect* effect : {&guidance, static_cast<MGuidanceEffect*>(&chase)})
	{
		Initialize(*effect);
		effect->SetStepPixel(0);
		effect->SetTraceCreatureID(creatureID);
		CHECK(effect->Update());
		CHECK_EQ(0, effect->GetPixelX());
		CHECK_EQ(1, effect->GetFrame());
		CHECK_EQ(199, effect->GetEndFrame());
	}
	CHECK(!chase.IsChaseOver());
}

TEST(GuidanceChase, NonAlphaEffectsKeepTheirExistingLight)
{
	World world;
	for (const auto blt : {BLT_NORMAL, BLT_SHADOW, BLT_SCREEN})
	{
		MGuidanceEffect guidance(static_cast<BYTE>(blt));
		MChaseEffect chase(static_cast<BYTE>(blt));
		for (MGuidanceEffect* effect : {&guidance, static_cast<MGuidanceEffect*>(&chase)})
		{
			Initialize(*effect);
			effect->SetLight(19);
			effect->SetTraceCreatureID(creatureID);
			ClearCalls();
			CHECK(effect->Update());
			CHECK_EQ(19, effect->GetLight());
			CHECK_EQ(1, effect->GetFrame());
			CHECK(lights.empty());
		}
	}
}

TEST(GuidanceChase, MissingLightEntryReturnsZeroWhileTrackingStillWorks)
{
	World world;
	const MEffectHost clockOnly{.CurrentFrame = effectHost.CurrentFrame};
	MEffect::SetHost(&clockOnly);
	MGuidanceEffect guidance(BLT_EFFECT);
	MChaseEffect chase(BLT_EFFECT);
	for (MGuidanceEffect* effect : {&guidance, static_cast<MGuidanceEffect*>(&chase)})
	{
		Initialize(*effect);
		effect->SetLight(19);
		effect->SetTraceCreatureID(creatureID);
		CHECK(effect->Update());
		CHECK_EQ(48, effect->GetPixelX());
		CHECK_EQ(0, effect->GetLight());
	}
}

TEST(GuidanceChase, GuidanceKeepsAbsoluteUnsignedClockComparisons)
{
	World world;
	MGuidanceEffect effect(BLT_EFFECT);
	Initialize(effect);
	frameNow = (std::numeric_limits<DWORD>::max)() - 1;
	effect.SetCount(4);
	effect.SetTraceCreatureID(creatureID);
	ClearCalls();
	CHECK_EQ(1, effect.GetEndFrame());
	CHECK(!effect.Update());
	CHECK(lookups.empty());
	frameNow = 0;
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	frameNow = 1;
	CHECK(!effect.Update());
}

TEST(GuidanceChase, DelayWaitAndLinkDeadlinesDoNotGateTracking)
{
	World world;
	MGuidanceEffect guidance(BLT_EFFECT);
	MChaseEffect chase(BLT_EFFECT);
	for (MGuidanceEffect* effect : {&guidance, static_cast<MGuidanceEffect*>(&chase)})
	{
		Initialize(*effect);
		effect->SetCount(100, 1);
		effect->SetDelayFrame(100);
		effect->SetWaitFrame(100);
		effect->SetDrawSkip(true);
		effect->SetTraceCreatureID(creatureID);
		CHECK(effect->Update());
		CHECK_EQ(48, effect->GetPixelX());
		CHECK_EQ(100, effect->GetEndLinkFrame());
		CHECK(effect->IsDelayFrame());
		CHECK(effect->IsWaitFrame());
		CHECK(effect->IsSkipDraw());
	}
}
