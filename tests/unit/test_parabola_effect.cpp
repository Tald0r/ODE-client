#include "test_framework.h"
#include "MParabolaEffect.h"
#include "MathTable.h"
#include "SkillDef.h"

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace {

struct Tables
{
	std::array<int, MathTable::MAX_ANGLE> sine, cosine;
	std::array<int, MathTable::MAX_ANGLE + 1> tangent;
	Tables()
	{
		std::copy_n(MathTable::FSinTab, sine.size(), sine.begin());
		std::copy_n(MathTable::FCosTab, cosine.size(), cosine.begin());
		std::copy_n(MathTable::FArcTanTab, tangent.size(), tangent.begin());
		if (MathTable::FCosTab[0] != 65536) MathTable::FCreateSines();
	}
	~Tables()
	{
		std::copy(sine.begin(), sine.end(), MathTable::FSinTab);
		std::copy(cosine.begin(), cosine.end(), MathTable::FCosTab);
		std::copy(tangent.begin(), tangent.end(), MathTable::FArcTanTab);
	}
};

DWORD frameNow;
bool spriteAvailable;
MParabolaSmokeSprite smokeSprite;
std::vector<int> calls;
struct LightRequest
{
	BYTE blt;
	TYPE_FRAMEID id;
	BYTE direction, frame;
	bool operator==(const LightRequest&) const = default;
};
std::vector<LightRequest> lights;
struct QueuedSmoke
{
	std::unique_ptr<MEffect> effect;
	DWORD wait;
};
std::vector<QueuedSmoke> queued;
struct Impact
{
	TYPE_SECTORPOSITION x, y;
	bool operator==(const Impact&) const = default;
};
std::vector<Impact> impacts;
MParabolaEffect* observed;
int queueParentX, queueParentZ, queueSectorX, impactPixelX, impactSectorX;
DWORD impactEndFrame;
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(1); return frameNow; },
	.Light = [](BYTE blt, TYPE_FRAMEID id, BYTE direction, BYTE frame) {
		calls.push_back(3);
		lights.push_back({blt, id, direction, frame});
		return id == 90 ? 11 : 7;
	},
};
const MParabolaEffectHost parabolaHost{
	.SmokeSprite = [](MParabolaSmokeSprite& sprite) {
		calls.push_back(2);
		if (!spriteAvailable) return false;
		sprite = smokeSprite;
		return true;
	},
	.QueueSmoke = [](std::unique_ptr<MEffect> smoke, DWORD wait) {
		calls.push_back(4);
		queued.push_back({std::move(smoke), wait});
		if (observed)
		{
			queueParentX = observed->GetPixelX();
			queueParentZ = observed->GetPixelZ();
			queueSectorX = observed->GetX();
		}
	},
	.CannonadeImpact = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back(5);
		impacts.push_back({x, y});
		if (observed)
		{
			impactPixelX = observed->GetPixelX();
			impactSectorX = observed->GetX();
			impactEndFrame = observed->GetEndFrame();
		}
	},
};

struct World
{
	Tables tables;
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MParabolaEffectHost* previousParabola = MParabolaEffect::SetHost(&parabolaHost);
	World()
	{
		frameNow = 100; spriteAvailable = true;
		smokeSprite = {BLT_EFFECT, 90, 5};
		calls.clear(); lights.clear(); queued.clear(); impacts.clear();
		observed = nullptr;
		queueParentX = queueParentZ = queueSectorX = impactPixelX = impactSectorX = -1;
		impactEndFrame = 999;
	}
	~World()
	{
		queued.clear(); observed = nullptr;
		MParabolaEffect::SetHost(previousParabola);
		MEffect::SetHost(previousEffect);
	}
};

struct EffectProbe : MParabolaEffect
{
	using MParabolaEffect::MParabolaEffect;
	int TargetTileX() const { return m_TargetTileX; }
	int TargetTileY() const { return m_TargetTileY; }
	float Length() const { return GetPathLength(); }
	void SetPixels(float x, float y, float z) { m_PixelX = x; m_PixelY = y; m_PixelZ = z; }
};

void Initialize(MParabolaEffect& effect)
{
	effect.SetFrameID(12, 3);
	effect.SetPixelPosition(0, 0, 0);
	effect.SetCount(100);
	effect.SetTarget(192, 0, 0, 48);
}

void ClearCalls() { calls.clear(); lights.clear(); }

} // namespace

TEST(ParabolaEffect, DefaultStateIsExpiredWithZeroTargetTiles)
{
	World world;
	EffectProbe effect(BLT_EFFECT);
	CHECK_EQ(MEffect::EFFECT_PARABOLA, effect.GetEffectType());
	CHECK_EQ(0, effect.TargetTileX());
	CHECK_EQ(0, effect.TargetTileY());
	CHECK_EQ(0, effect.Length());
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetFrame());
	CHECK(queued.empty());
	CHECK(impacts.empty());
	CHECK(lights.empty());
}

TEST(ParabolaEffect, ActiveFlightUsesArcHeightThenProjectsAnimatesAndLights)
{
	World world;
	MParabolaEffect effect(BLT_EFFECT);
	Initialize(effect);
	ClearCalls();
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(0, effect.GetPixelY());
	CHECK_EQ(33, effect.GetPixelZ());
	CHECK_EQ(1, effect.GetX());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(7, effect.GetLight());
	CHECK(calls == std::vector<int>({1, 3}));
	CHECK(lights == std::vector<LightRequest>({{BLT_EFFECT, 12, DIRECTION_RIGHT, 1}}));
	CHECK(queued.empty());
	CHECK(impacts.empty());
}

TEST(ParabolaEffect, FallingBelowTargetLandsBeforeProjectionAnimationAndLight)
{
	World world;
	MParabolaEffect effect(BLT_EFFECT);
	Initialize(effect);
	CHECK(effect.Update());
	CHECK(effect.Update());
	CHECK_EQ(96, effect.GetPixelX());
	CHECK_EQ(33, effect.GetPixelZ());
	ClearCalls();
	CHECK(!effect.Update());
	CHECK_EQ(192, effect.GetPixelX());
	CHECK_EQ(0, effect.GetPixelZ());
	CHECK_EQ(2, effect.GetX());
	CHECK_EQ(2, effect.GetFrame());
	CHECK_EQ(0, effect.GetEndFrame());
	CHECK_EQ(199, effect.GetEndLinkFrame());
	CHECK(calls == std::vector<int>({1}));
	CHECK(lights.empty());
	CHECK(impacts.empty());
}

TEST(ParabolaEffect, CannonadeEmitsRealSmokeAtTheAdvancedPositionBeforeLanding)
{
	World world;
	MParabolaEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetLink(SKILL_CANNONADE, nullptr);
	effect.SetTargetTile(9, 13);
	observed = &effect;
	CHECK(effect.Update());
	CHECK(effect.Update());
	ClearCalls();
	CHECK(!effect.Update());
	CHECK_EQ(3, queued.size());
	CHECK_EQ(144, queued.back().effect->GetPixelX());
	CHECK_EQ(-1, queued.back().effect->GetPixelZ());
	CHECK_EQ(144, queueParentX);
	CHECK_EQ(-1, queueParentZ);
	CHECK_EQ(2, queueSectorX);
	CHECK_EQ(192, impactPixelX);
	CHECK_EQ(2, impactSectorX);
	CHECK_EQ(0, impactEndFrame);
	CHECK(impacts == std::vector<Impact>({{9, 13}}));
	CHECK(calls == std::vector<int>({1, 2, 3, 1, 4, 5}));
	CHECK(lights == std::vector<LightRequest>({{BLT_EFFECT, 90, DIRECTION_LEFT, 0}}));
	CHECK(!effect.Update());
	CHECK_EQ(3, queued.size());
	CHECK_EQ(1, impacts.size());
}

TEST(ParabolaEffect, SmokeOwnsItsFramePositionLifetimeDirectionAndMultiFlag)
{
	World world;
	MParabolaEffect effect(BLT_EFFECT);
	effect.SetPixelPosition(72, 36, -5);
	effect.SetDirection(DIRECTION_RIGHTUP);
	ClearCalls();
	effect.MakeCannonadeSmoke();
	CHECK_EQ(1, queued.size());
	MEffect& smoke = *queued.front().effect;
	CHECK_EQ(MEffect::EFFECT_SECTOR, smoke.GetEffectType());
	CHECK_EQ(BLT_EFFECT, smoke.GetBltType());
	CHECK_EQ(90, smoke.GetFrameID());
	CHECK_EQ(5, smoke.GetMaxFrame());
	CHECK_EQ(0, smoke.GetFrame());
	CHECK_EQ(72, smoke.GetPixelX());
	CHECK_EQ(36, smoke.GetPixelY());
	CHECK_EQ(-5, smoke.GetPixelZ());
	CHECK_EQ(1, smoke.GetX());
	CHECK_EQ(1, smoke.GetY());
	CHECK_EQ(DIRECTION_RIGHTUP, smoke.GetDirection());
	CHECK(smoke.IsMulti());
	CHECK_EQ(108, smoke.GetEndFrame());
	CHECK_EQ(108, smoke.GetEndLinkFrame());
	CHECK_EQ(10, queued.front().wait);
	CHECK(calls == std::vector<int>({2, 3, 1, 4}));
	// The wait argument selects the zone's wait list; it does not set a deadline.
	CHECK(!smoke.IsWaitFrame());
	CHECK(!smoke.IsDelayFrame());
	CHECK_EQ(11, smoke.GetLight());
}

TEST(ParabolaEffect, ThreeDimensionalTargetingEmitsSmokeBeforeTheHeightSnap)
{
	World world;
	EffectProbe effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTarget(30, 40, 120, 13);
	effect.SetLink(SKILL_CANNONADE, nullptr);
	CHECK_EQ(130, effect.Length());
	CHECK_EQ(13, effect.GetStepPixel());
	CHECK_EQ(DIRECTION_RIGHTDOWN, effect.GetDirection());
	CHECK(!effect.Update());
	CHECK_EQ(1, queued.size());
	CHECK_EQ(3, queued[0].effect->GetPixelX());
	CHECK_EQ(4, queued[0].effect->GetPixelY());
	CHECK_EQ(24, queued[0].effect->GetPixelZ());
	CHECK_EQ(30, effect.GetPixelX());
	CHECK_EQ(40, effect.GetPixelY());
	CHECK_EQ(120, effect.GetPixelZ());
	CHECK(impacts == std::vector<Impact>({{0, 0}}));
}

TEST(ParabolaEffect, ExpiryPreventsMovementSmokeImpactAndAnimation)
{
	World world;
	MParabolaEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetLink(SKILL_CANNONADE, nullptr);
	frameNow = 199;
	ClearCalls();
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(0, effect.GetPixelZ());
	CHECK_EQ(0, effect.GetFrame());
	CHECK(queued.empty());
	CHECK(impacts.empty());
	CHECK(calls == std::vector<int>({1}));
}

TEST(ParabolaEffect, OtherActionsDoNotGenerateCannonadeOutputs)
{
	World world;
	for (const auto action : {static_cast<TYPE_ACTIONINFO>(SKILL_HALO),
		static_cast<TYPE_ACTIONINFO>(SKILL_CLIENT_HALO_ATTACK)})
	{
		MParabolaEffect effect(BLT_EFFECT);
		Initialize(effect);
		effect.SetLink(action, nullptr);
		CHECK(effect.Update());
		CHECK(effect.Update());
		CHECK(!effect.Update());
		CHECK_EQ(0, effect.GetEndFrame());
		CHECK(queued.empty());
		CHECK(impacts.empty());
	}
}

TEST(ParabolaEffect, MissingMetadataSkipsSmokeWithoutPreventingImpact)
{
	World world;
	MParabolaEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetLink(SKILL_CANNONADE, nullptr);
	spriteAvailable = false;
	CHECK(effect.Update());
	CHECK(effect.Update());
	ClearCalls();
	CHECK(!effect.Update());
	CHECK(queued.empty());
	CHECK(impacts == std::vector<Impact>({{0, 0}}));
	CHECK(calls == std::vector<int>({1, 2, 5}));
}

TEST(ParabolaEffect, MissingHostOrAllEmptyEntriesDoNotPreventFlight)
{
	World world;
	const MParabolaEffectHost empty{};
	for (const auto* host : {static_cast<const MParabolaEffectHost*>(nullptr), &empty})
	{
		MParabolaEffect::SetHost(host);
		MParabolaEffect effect(BLT_EFFECT);
		Initialize(effect);
		effect.SetLink(SKILL_CANNONADE, nullptr);
		CHECK(effect.Update());
		CHECK(effect.Update());
		CHECK(!effect.Update());
		CHECK_EQ(192, effect.GetPixelX());
		CHECK(queued.empty());
		CHECK(impacts.empty());
	}
}

TEST(ParabolaEffect, MetadataAndImpactEntriesWorkWithoutAQueue)
{
	World world;
	const MParabolaEffectHost noQueue{
		.SmokeSprite = parabolaHost.SmokeSprite,
		.CannonadeImpact = parabolaHost.CannonadeImpact,
	};
	MParabolaEffect::SetHost(&noQueue);
	MParabolaEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetLink(SKILL_CANNONADE, nullptr);
	CHECK(effect.Update());
	CHECK(effect.Update());
	ClearCalls();
	CHECK(!effect.Update());
	CHECK(queued.empty());
	CHECK_EQ(1, impacts.size());
	CHECK(calls == std::vector<int>({1, 2, 3, 1, 5}));
}

TEST(ParabolaEffect, QueueAndImpactEntriesDoNotInventMissingMetadata)
{
	World world;
	const MParabolaEffectHost noMetadata{
		.QueueSmoke = parabolaHost.QueueSmoke,
		.CannonadeImpact = parabolaHost.CannonadeImpact,
	};
	MParabolaEffect::SetHost(&noMetadata);
	MParabolaEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetLink(SKILL_CANNONADE, nullptr);
	CHECK(effect.Update());
	CHECK(effect.Update());
	CHECK(!effect.Update());
	CHECK(queued.empty());
	CHECK_EQ(1, impacts.size());
}

TEST(ParabolaEffect, MissingImpactEntryStillAllowsAllSmokeToBeQueued)
{
	World world;
	const MParabolaEffectHost noImpact{
		.SmokeSprite = parabolaHost.SmokeSprite,
		.QueueSmoke = parabolaHost.QueueSmoke,
	};
	MParabolaEffect::SetHost(&noImpact);
	MParabolaEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetLink(SKILL_CANNONADE, nullptr);
	CHECK(effect.Update());
	CHECK(effect.Update());
	CHECK(!effect.Update());
	CHECK_EQ(3, queued.size());
	CHECK(impacts.empty());
}

TEST(ParabolaEffect, HostIsReadAgainWhenMetadataReplacesItsQueue)
{
	World world;
	const MParabolaEffectHost replacing{
		.SmokeSprite = [](MParabolaSmokeSprite& sprite) {
			sprite = smokeSprite;
			MParabolaEffect::SetHost(&parabolaHost);
			return true;
		},
	};
	CHECK(MParabolaEffect::SetHost(&replacing) == &parabolaHost);
	MParabolaEffect effect(BLT_EFFECT);
	effect.MakeCannonadeSmoke();
	CHECK_EQ(1, queued.size());
	CHECK_EQ(90, queued[0].effect->GetFrameID());
	CHECK(MParabolaEffect::SetHost(&replacing) == &parabolaHost);
}

TEST(ParabolaEffect, RemovingTheHostDuringMetadataDiscardsSmoke)
{
	World world;
	const MParabolaEffectHost removing{
		.SmokeSprite = [](MParabolaSmokeSprite& sprite) {
			sprite = smokeSprite;
			MParabolaEffect::SetHost(nullptr);
			return true;
		},
		.QueueSmoke = parabolaHost.QueueSmoke,
	};
	MParabolaEffect::SetHost(&removing);
	MParabolaEffect effect(BLT_EFFECT);
	effect.MakeCannonadeSmoke();
	CHECK(queued.empty());
	CHECK_EQ(1, lights.size());
	CHECK(MParabolaEffect::SetHost(&parabolaHost) == nullptr);
}

TEST(ParabolaEffect, NewSmokeReadsCurrentMetadataAndRetainsByteFrameConversion)
{
	World world;
	MParabolaEffect effect(BLT_EFFECT);
	effect.MakeCannonadeSmoke();
	smokeSprite = {BLT_NORMAL, 91, 258};
	effect.MakeCannonadeSmoke();
	CHECK_EQ(2, queued.size());
	CHECK_EQ(90, queued[0].effect->GetFrameID());
	CHECK_EQ(5, queued[0].effect->GetMaxFrame());
	CHECK_EQ(BLT_NORMAL, queued[1].effect->GetBltType());
	CHECK_EQ(91, queued[1].effect->GetFrameID());
	CHECK_EQ(2, queued[1].effect->GetMaxFrame());
}

TEST(ParabolaEffect, NonAlphaFlightPreservesItsExistingLight)
{
	World world;
	for (const auto blt : {BLT_NORMAL, BLT_SHADOW, BLT_SCREEN})
	{
		MParabolaEffect effect(static_cast<BYTE>(blt));
		Initialize(effect);
		effect.SetLight(19);
		ClearCalls();
		CHECK(effect.Update());
		CHECK_EQ(19, effect.GetLight());
		CHECK_EQ(1, effect.GetFrame());
		CHECK(lights.empty());
	}
}

TEST(ParabolaEffect, MissingClockPreventsAllUpdateOutputs)
{
	World world;
	MParabolaEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetLink(SKILL_CANNONADE, nullptr);
	MEffect::SetHost(nullptr);
	ClearCalls();
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(0, effect.GetPixelZ());
	CHECK_EQ(0, effect.GetFrame());
	CHECK(calls.empty());
	CHECK(queued.empty());
	CHECK(impacts.empty());
}

TEST(ParabolaEffect, MissingLightEntryDoesNotPreventSmokeOrMovement)
{
	World world;
	const MEffectHost clockOnly{.CurrentFrame = effectHost.CurrentFrame};
	MEffect::SetHost(&clockOnly);
	MParabolaEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetLink(SKILL_CANNONADE, nullptr);
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(0, effect.GetLight());
	CHECK_EQ(1, queued.size());
	CHECK_EQ(0, queued[0].effect->GetLight());
}

TEST(ParabolaEffect, ZeroSpeedDoesNotArriveButStillAnimatesAndEmitsSmoke)
{
	World world;
	MParabolaEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetTarget(0, 0, 0, 0);
	effect.SetLink(SKILL_CANNONADE, nullptr);
	CHECK(effect.Update());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(0, effect.GetPixelZ());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(1, queued.size());
	CHECK(impacts.empty());
}

TEST(ParabolaEffect, RetargetingUsesCurrentPositionAndRestartsTheArc)
{
	World world;
	MParabolaEffect effect(BLT_EFFECT);
	Initialize(effect);
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(33, effect.GetPixelZ());
	effect.SetTarget(240, 0, 33, 48);
	CHECK(effect.Update());
	CHECK_EQ(96, effect.GetPixelX());
	CHECK_EQ(66, effect.GetPixelZ());
	CHECK_EQ(2, effect.GetFrame());
}

TEST(ParabolaEffect, CannonadeTargetTilesKeepTheirUnsignedStorageConversion)
{
	World world;
	MParabolaEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetLink(SKILL_CANNONADE, nullptr);
	effect.SetTargetTile(-1, 65538);
	CHECK(effect.Update());
	CHECK(effect.Update());
	CHECK(!effect.Update());
	CHECK(impacts == std::vector<Impact>({{65535, 2}}));
}

TEST(ParabolaEffect, LinkDelayAndWaitDeadlinesDoNotControlFlightOrSmoke)
{
	World world;
	MParabolaEffect effect(BLT_EFFECT);
	Initialize(effect);
	effect.SetCount(100, 1);
	effect.SetDelayFrame(100);
	effect.SetWaitFrame(100);
	effect.SetDrawSkip(true);
	effect.SetLink(SKILL_CANNONADE, nullptr);
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(100, effect.GetEndLinkFrame());
	CHECK(effect.IsDelayFrame());
	CHECK(effect.IsWaitFrame());
	CHECK(effect.IsSkipDraw());
	CHECK_EQ(108, queued[0].effect->GetEndLinkFrame());
}

TEST(ParabolaEffect, ExpiryAndSmokeLifetimeKeepUnsignedClockWrap)
{
	World world;
	MParabolaEffect effect(BLT_EFFECT);
	Initialize(effect);
	frameNow = (std::numeric_limits<DWORD>::max)() - 1;
	effect.SetCount(4);
	effect.SetLink(SKILL_CANNONADE, nullptr);
	CHECK_EQ(1, effect.GetEndFrame());
	CHECK(!effect.Update());
	CHECK(queued.empty());
	frameNow = 0;
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(1, queued.size());
	CHECK_EQ(8, queued[0].effect->GetEndFrame());
	frameNow = 1;
	CHECK(!effect.Update());
}

TEST(ParabolaEffect, SmokePreservesMaximumIntegerPixelsAfterFloatStorage)
{
	World world;
	const int high = (std::numeric_limits<int>::max)();
	MParabolaEffect effect(BLT_EFFECT);
	effect.SetPixelPosition(high, high, high);
	effect.MakeCannonadeSmoke();
	CHECK_EQ(1, queued.size());
	CHECK_EQ(high, queued[0].effect->GetPixelX());
	CHECK_EQ(high, queued[0].effect->GetPixelY());
	CHECK_EQ(high, queued[0].effect->GetPixelZ());
}

TEST(ParabolaEffect, SmokeBoundsNonfiniteCoordinatesBeforeCopyingThem)
{
	World world;
	EffectProbe effect(BLT_EFFECT);
	effect.SetPixels(std::numeric_limits<float>::infinity(),
		-std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN());
	effect.MakeCannonadeSmoke();
	CHECK_EQ(1, queued.size());
	CHECK_EQ((std::numeric_limits<int>::max)(), queued[0].effect->GetPixelX());
	CHECK_EQ((std::numeric_limits<int>::min)(), queued[0].effect->GetPixelY());
	CHECK_EQ(0, queued[0].effect->GetPixelZ());
}

TEST(ParabolaEffect, SmokePreservesOrdinaryFractionalTruncationAndMinimumPixels)
{
	World world;
	EffectProbe effect(BLT_EFFECT);
	effect.SetPixels(-12.75f, 31.75f, -0.75f);
	effect.MakeCannonadeSmoke();
	CHECK_EQ(-12, queued[0].effect->GetPixelX());
	CHECK_EQ(31, queued[0].effect->GetPixelY());
	CHECK_EQ(0, queued[0].effect->GetPixelZ());
	const int low = (std::numeric_limits<int>::min)();
	effect.SetPixelPosition(low, low, low);
	effect.MakeCannonadeSmoke();
	CHECK_EQ(low, queued[1].effect->GetPixelX());
	CHECK_EQ(low, queued[1].effect->GetPixelY());
	CHECK_EQ(low, queued[1].effect->GetPixelZ());
}

TEST(ParabolaEffect, SmokeUsesStoredCoordinatesWhenDisplayGettersAreOverridden)
{
	World world;
	struct DisplayEffect : MParabolaEffect
	{
		DisplayEffect() : MParabolaEffect(BLT_EFFECT) {}
		int GetPixelX() const override { return 1000; }
		int GetPixelY() const override { return 2000; }
		int GetPixelZ() const override { return 3000; }
	} effect;
	effect.SetPixelPosition(12, 34, 56);
	effect.MakeCannonadeSmoke();
	CHECK_EQ(1000, effect.GetPixelX());
	CHECK_EQ(2000, effect.GetPixelY());
	CHECK_EQ(3000, effect.GetPixelZ());
	CHECK_EQ(12, queued[0].effect->GetPixelX());
	CHECK_EQ(34, queued[0].effect->GetPixelY());
	CHECK_EQ(56, queued[0].effect->GetPixelZ());
}
