#include "test_framework.h"
#include "MHomingEffect.h"
#include "MathTable.h"
#include "SkillDef.h"

#include <algorithm>
#include <array>
#include <limits>
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
bool creatureExists;
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
MHomingEffect* observed;
int lightSectorX;
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(1); return frameNow; },
	.Light = [](BYTE blt, TYPE_FRAMEID id, BYTE direction, BYTE frame) {
		calls.push_back(3);
		lights.push_back({blt, id, direction, frame});
		if (observed) lightSectorX = observed->GetX();
		return lightValue;
	},
};
const MGuidanceEffectHost guidanceHost{
	.CreaturePosition = [](TYPE_OBJECTID id, int& x, int& y, int& z) {
		calls.push_back(2);
		lookups.push_back(id);
		if (!creatureExists || id != 42) return false;
		x = creatureX; y = creatureY; z = creatureZ;
		return true;
	},
};

struct World
{
	Tables tables;
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MGuidanceEffectHost* previousGuidance = MGuidanceEffect::SetHost(&guidanceHost);
	World()
	{
		frameNow = 100; creatureExists = true;
		creatureX = 192; creatureY = 0; creatureZ = 999; lightValue = 7;
		calls.clear(); lookups.clear(); lights.clear();
		observed = nullptr; lightSectorX = -1;
	}
	~World()
	{
		observed = nullptr;
		MGuidanceEffect::SetHost(previousGuidance);
		MEffect::SetHost(previousEffect);
	}
};

struct EffectProbe : MHomingEffect
{
	using MHomingEffect::MHomingEffect;
	using MHomingEffect::SetDirectionByAngle;
	int TargetX() const { return m_TargetX; }
	int TargetY() const { return m_TargetY; }
	int TargetZ() const { return m_TargetZ; }
	float HeightStep() const { return m_StepZ; }
	float Height() const { return m_PixelZ; }
	void SetPixels(float x, float y) { m_PixelX = x; m_PixelY = y; }
};

void Initialize(MHomingEffect& effect)
{
	effect.SetFrameID(12, 3);
	effect.SetPixelPosition(0, 0, 0);
	effect.SetDirection(DIRECTION_LEFTUP);
	effect.SetCount(100);
}

void ClearCalls()
{
	calls.clear(); lookups.clear(); lights.clear();
}

} // namespace

TEST(HomingEffect, DefaultEffectIsExpiredWithoutTracingOrAnimating)
{
	World world;
	EffectProbe effect(BLT_EFFECT, 0, 13);
	CHECK_EQ(MEffect::EFFECT_HOMING, effect.GetEffectType());
	CHECK_EQ(OBJECTID_NULL, effect.GetTraceCreatureID());
	CHECK_EQ(0, effect.GetStepPixel());
	CHECK_EQ(0, effect.HeightStep());
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetFrame());
	CHECK(lookups.empty());
	CHECK(lights.empty());
}

TEST(HomingEffect, TargetSetterKeepsHeightSpeedAndDisplayFacingSeparate)
{
	World world;
	EffectProbe effect(BLT_EFFECT, 0, 13);
	Initialize(effect);
	effect.SetTarget(192, 0, 32, 48);
	CHECK_EQ(192, effect.TargetX());
	CHECK_EQ(0, effect.TargetY());
	CHECK_EQ(32, effect.TargetZ());
	CHECK_EQ(2, effect.HeightStep());
	CHECK_EQ(48, effect.GetStepPixel());
	CHECK_EQ(DIRECTION_LEFTUP, effect.GetDirection());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(0, effect.GetPixelZ());
}

TEST(HomingEffect, CreatureTraceRefreshesOnlyHorizontalTargets)
{
	World world;
	EffectProbe effect(BLT_EFFECT, 0, 13);
	Initialize(effect);
	effect.SetTarget(400, 0, 32, 48);
	ClearCalls();
	effect.SetTraceCreatureID(42);
	CHECK_EQ(192, effect.TargetX());
	CHECK_EQ(0, effect.TargetY());
	CHECK_EQ(32, effect.TargetZ());
	CHECK_EQ(2, effect.HeightStep());
	CHECK(calls == std::vector<int>({2}));
	CHECK_EQ(0, effect.GetPixelX());
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(2, effect.GetPixelZ());
	CHECK_EQ(32, effect.TargetZ());
}

TEST(HomingEffect, ActiveUpdateTracesMovesProjectsThenAnimatesAndLights)
{
	World world;
	MHomingEffect effect(BLT_EFFECT, 0, 13);
	Initialize(effect);
	effect.SetTraceCreatureID(42);
	effect.SetTarget(192, 0, 16, 48);
	observed = &effect;
	ClearCalls();
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(0, effect.GetPixelY());
	CHECK_EQ(1, effect.GetPixelZ());
	CHECK_EQ(1, effect.GetX());
	CHECK_EQ(1, lightSectorX);
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(7, effect.GetLight());
	CHECK(calls == std::vector<int>({1, 2, 3}));
	CHECK(lights == std::vector<LightRequest>({{BLT_EFFECT, 12, DIRECTION_LEFTUP, 1}}));
}

TEST(HomingEffect, SteeringTurnsBeforeMovementAndRetainsFixedPointRounding)
{
	World world;
	MHomingEffect effect(BLT_EFFECT, 0, 90);
	Initialize(effect);
	creatureX = 0; creatureY = -256;
	effect.SetTraceCreatureID(42);
	effect.SetTarget(0, -256, 0, 64);
	CHECK(effect.Update());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(-63, effect.GetPixelY());
	CHECK_EQ(DIRECTION_LEFTUP, effect.GetDirection());
	CHECK(effect.Update());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(-126, effect.GetPixelY());
}

TEST(HomingEffect, OrdinaryArrivalIsStrictInXYAndSnapsHeight)
{
	World world;
	EffectProbe effect(BLT_EFFECT, 0, 13);
	Initialize(effect);
	creatureX = 96;
	effect.SetTraceCreatureID(42);
	effect.SetTarget(96, 0, 1600, 48);
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(100, effect.GetPixelZ());
	CHECK_EQ(1, effect.GetX());
	ClearCalls();
	CHECK(!effect.Update());
	CHECK_EQ(96, effect.GetPixelX());
	CHECK_EQ(1600, effect.GetPixelZ());
	CHECK_EQ(0, effect.HeightStep());
	CHECK_EQ(1, effect.GetX());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(0, effect.GetEndFrame());
	CHECK_EQ(199, effect.GetEndLinkFrame());
	CHECK(calls == std::vector<int>({1, 2}));
	CHECK(lights.empty());
}

TEST(HomingEffect, ExpiredUpdateDoesNotTraceMoveOrAnimate)
{
	World world;
	MHomingEffect effect(BLT_EFFECT, 0, 13);
	Initialize(effect);
	effect.SetTraceCreatureID(42);
	effect.SetTarget(192, 0, 16, 48);
	frameNow = 199;
	ClearCalls();
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(0, effect.GetPixelZ());
	CHECK_EQ(0, effect.GetFrame());
	CHECK_EQ(42, effect.GetTraceCreatureID());
	CHECK(calls == std::vector<int>({1}));
}

TEST(HomingEffect, MissingCreatureClearsTraceAndLifetimeBeforeMovement)
{
	World world;
	MHomingEffect effect(BLT_EFFECT, 0, 13);
	Initialize(effect);
	effect.SetTraceCreatureID(42);
	effect.SetTarget(192, 0, 16, 48);
	creatureExists = false;
	ClearCalls();
	CHECK(!effect.Update());
	CHECK_EQ(OBJECTID_NULL, effect.GetTraceCreatureID());
	CHECK_EQ(0, effect.GetEndFrame());
	CHECK_EQ(199, effect.GetEndLinkFrame());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(0, effect.GetFrame());
	CHECK(calls == std::vector<int>({1, 2}));
	CHECK(lights.empty());
}

TEST(HomingEffect, MissingCreatureInTheSetterUsesVirtualHomingTrace)
{
	World world;
	EffectProbe effect(BLT_EFFECT, 0, 13);
	Initialize(effect);
	effect.SetTarget(192, 0, 16, 48);
	creatureExists = false;
	effect.SetTraceCreatureID(42);
	CHECK_EQ(OBJECTID_NULL, effect.GetTraceCreatureID());
	CHECK_EQ(0, effect.GetEndFrame());
	CHECK_EQ(192, effect.TargetX());
	CHECK_EQ(16, effect.TargetZ());
	CHECK_EQ(1, effect.HeightStep());
}

TEST(HomingEffect, NullTraceSetterSkipsLookupButOrdinaryUpdateRequiresIt)
{
	World world;
	MHomingEffect effect(BLT_EFFECT, 0, 13);
	Initialize(effect);
	effect.SetTarget(192, 0, 0, 48);
	ClearCalls();
	effect.SetTraceCreatureID(OBJECTID_NULL);
	CHECK(lookups.empty());
	CHECK(!effect.Update());
	CHECK(lookups == std::vector<TYPE_OBJECTID>({OBJECTID_NULL}));
	CHECK_EQ(0, effect.GetPixelX());
}

TEST(HomingEffect, HaloAttackDoesNotTraceOrStopAtItsHorizontalTarget)
{
	World world;
	MHomingEffect effect(BLT_EFFECT, 0, 13);
	Initialize(effect);
	effect.SetTarget(48, 0, 16, 48);
	effect.SetLink(SKILL_CLIENT_HALO_ATTACK, nullptr);
	MGuidanceEffect::SetHost(nullptr);
	ClearCalls();
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(1, effect.GetPixelZ());
	CHECK_EQ(1, effect.GetX());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(199, effect.GetEndFrame());
	CHECK(calls == std::vector<int>({1, 3}));
	CHECK(effect.Update());
	CHECK_EQ(96, effect.GetPixelX());
	CHECK(lookups.empty());
}

TEST(HomingEffect, HaloAttackRetainsItsTurnInsteadOfReaimingEachFrame)
{
	World world;
	MHomingEffect effect(BLT_EFFECT, 0, 90);
	Initialize(effect);
	effect.SetTarget(0, -256, 0, 64);
	effect.SetLink(SKILL_CLIENT_HALO_ATTACK, nullptr);
	CHECK(effect.Update());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(-63, effect.GetPixelY());
	CHECK(effect.Update());
	CHECK_EQ(-64, effect.GetPixelX());
	CHECK_EQ(-63, effect.GetPixelY());
	CHECK_EQ(DIRECTION_LEFTUP, effect.GetDirection());
	CHECK(lookups.empty());
}

TEST(HomingEffect, PlainHaloUsesOrdinaryTrackingAndArrival)
{
	World world;
	MHomingEffect effect(BLT_EFFECT, 0, 13);
	Initialize(effect);
	creatureX = 48;
	effect.SetTraceCreatureID(42);
	effect.SetTarget(48, 0, 16, 48);
	effect.SetLink(SKILL_HALO, nullptr);
	ClearCalls();
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetEndFrame());
	CHECK_EQ(16, effect.GetPixelZ());
	CHECK_EQ(0, effect.GetFrame());
	CHECK(calls == std::vector<int>({1, 2}));
}

TEST(HomingEffect, AscendingHeightStopsAtTheTargetWhileHorizontalFlightContinues)
{
	World world;
	EffectProbe effect(BLT_EFFECT, 0, 0);
	Initialize(effect);
	effect.SetTarget(1000, 0, 16, 1);
	effect.SetLink(SKILL_CLIENT_HALO_ATTACK, nullptr);
	for (int step = 1; step <= 16; ++step)
	{
		CHECK(effect.Update());
		CHECK_EQ(step, effect.GetPixelZ());
	}
	CHECK_EQ(0, effect.HeightStep());
	CHECK(effect.Update());
	CHECK_EQ(16, effect.GetPixelZ());
	CHECK_EQ(17, effect.GetPixelX());
}

TEST(HomingEffect, ZeroHorizontalSpeedStillAdvancesHeightAndAnimation)
{
	World world;
	MHomingEffect effect(BLT_EFFECT, 0, 13);
	Initialize(effect);
	creatureX = 0;
	effect.SetTraceCreatureID(42);
	effect.SetTarget(0, 0, 16, 0);
	CHECK(effect.Update());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(1, effect.GetPixelZ());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(199, effect.GetEndFrame());
}

TEST(HomingEffect, MissingClockStopsBothOrdinaryAndHaloFlight)
{
	World world;
	for (const auto action : {static_cast<TYPE_ACTIONINFO>(ACTIONINFO_NULL),
		static_cast<TYPE_ACTIONINFO>(SKILL_CLIENT_HALO_ATTACK)})
	{
		MHomingEffect effect(BLT_EFFECT, 0, 13);
		MEffect::SetHost(&effectHost);
		Initialize(effect);
		effect.SetTraceCreatureID(42);
		effect.SetTarget(192, 0, 16, 48);
		effect.SetLink(action, nullptr);
		MEffect::SetHost(nullptr);
		ClearCalls();
		CHECK(!effect.Update());
		CHECK_EQ(0, effect.GetPixelX());
		CHECK_EQ(0, effect.GetFrame());
		CHECK(calls.empty());
	}
}

TEST(HomingEffect, MissingLookupAndEmptyHostStopOrdinaryFlight)
{
	World world;
	const MGuidanceEffectHost empty{};
	for (const auto* host : {static_cast<const MGuidanceEffectHost*>(nullptr), &empty})
	{
		MHomingEffect effect(BLT_EFFECT, 0, 13);
		Initialize(effect);
		MGuidanceEffect::SetHost(&guidanceHost);
		effect.SetTraceCreatureID(42);
		effect.SetTarget(192, 0, 16, 48);
		MGuidanceEffect::SetHost(host);
		CHECK(!effect.Update());
		CHECK_EQ(OBJECTID_NULL, effect.GetTraceCreatureID());
		CHECK_EQ(0, effect.GetEndFrame());
		CHECK_EQ(0, effect.GetPixelX());
	}
}

TEST(HomingEffect, ReplacementLookupChangesExistingHorizontalTargetsOnly)
{
	World world;
	EffectProbe effect(BLT_EFFECT, 0, 90);
	Initialize(effect);
	effect.SetTraceCreatureID(42);
	// Keep a nonzero turn by initially aiming above the effect.
	effect.SetTarget(0, -256, 16, 64);
	const MGuidanceEffectHost replacement{
		.CreaturePosition = [](TYPE_OBJECTID id, int& x, int& y, int& z) {
			CHECK_EQ(42, id);
			x = 0; y = 256; z = -999;
			return true;
		},
	};
	MGuidanceEffect::SetHost(&replacement);
	CHECK(effect.Update());
	CHECK_EQ(0, effect.TargetX());
	CHECK_EQ(256, effect.TargetY());
	CHECK_EQ(16, effect.TargetZ());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(64, effect.GetPixelY());
	CHECK_EQ(1, effect.GetPixelZ());
}

TEST(HomingEffect, NonAlphaUpdatesPreserveTheirExistingLight)
{
	World world;
	for (const auto blt : {BLT_NORMAL, BLT_SHADOW, BLT_SCREEN})
	{
		MHomingEffect effect(static_cast<BYTE>(blt), 0, 13);
		Initialize(effect);
		effect.SetTraceCreatureID(42);
		effect.SetTarget(192, 0, 16, 48);
		effect.SetLight(19);
		ClearCalls();
		CHECK(effect.Update());
		CHECK_EQ(19, effect.GetLight());
		CHECK_EQ(1, effect.GetFrame());
		CHECK(lights.empty());
	}
}

TEST(HomingEffect, MissingLightEntryReturnsZeroDuringActiveFlight)
{
	World world;
	const MEffectHost clockOnly{.CurrentFrame = effectHost.CurrentFrame};
	MEffect::SetHost(&clockOnly);
	MHomingEffect effect(BLT_EFFECT, 0, 13);
	Initialize(effect);
	effect.SetTraceCreatureID(42);
	effect.SetTarget(192, 0, 16, 48);
	effect.SetLight(19);
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(0, effect.GetLight());
}

TEST(HomingEffect, ExplicitAngleFacingKeepsTheLegacySectorBoundaries)
{
	World world;
	struct Example { int degrees; BYTE facing; };
	for (const auto& e : {Example{0, DIRECTION_RIGHT}, {29, DIRECTION_RIGHT},
		{30, DIRECTION_RIGHTUP}, {59, DIRECTION_RIGHTUP}, {60, DIRECTION_UP},
		{119, DIRECTION_UP}, {120, DIRECTION_LEFTUP}, {149, DIRECTION_LEFTUP},
		{150, DIRECTION_LEFT}, {209, DIRECTION_LEFT}, {210, DIRECTION_LEFTDOWN},
		{239, DIRECTION_LEFTDOWN}, {240, DIRECTION_DOWN}, {299, DIRECTION_DOWN},
		{300, DIRECTION_RIGHTDOWN}, {329, DIRECTION_RIGHTDOWN},
		{330, DIRECTION_RIGHT}, {359, DIRECTION_RIGHT}})
	{
		EffectProbe effect(BLT_EFFECT, e.degrees, 13);
		effect.SetDirectionByAngle();
		CHECK_EQ(e.facing, effect.GetDirection());
	}
}

TEST(HomingEffect, OrdinaryArrivalClearsTurningBeforeLifetimeReuse)
{
	World world;
	MHomingEffect effect(BLT_EFFECT, 0, 90);
	Initialize(effect);
	creatureX = 0; creatureY = -63;
	effect.SetTraceCreatureID(42);
	effect.SetTarget(0, -63, 0, 64);
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(-63, effect.GetPixelY());
	effect.SetCount(100);
	effect.SetTarget(-192, -63, 0, 64);
	effect.SetLink(SKILL_CLIENT_HALO_ATTACK, nullptr);
	CHECK(effect.Update());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(-126, effect.GetPixelY());
}

TEST(HomingEffect, ExpiryKeepsAbsoluteUnsignedClockComparisons)
{
	World world;
	MHomingEffect effect(BLT_EFFECT, 0, 13);
	Initialize(effect);
	effect.SetTraceCreatureID(42);
	effect.SetTarget(192, 0, 0, 48);
	frameNow = (std::numeric_limits<DWORD>::max)() - 1;
	effect.SetCount(4);
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

TEST(HomingEffect, LinkDelayAndWaitDeadlinesDoNotControlFlight)
{
	World world;
	MHomingEffect effect(BLT_EFFECT, 0, 13);
	Initialize(effect);
	effect.SetTraceCreatureID(42);
	effect.SetTarget(192, 0, 16, 48);
	effect.SetCount(100, 1);
	effect.SetDelayFrame(100);
	effect.SetWaitFrame(100);
	effect.SetDrawSkip(true);
	CHECK(effect.Update());
	CHECK_EQ(48, effect.GetPixelX());
	CHECK_EQ(100, effect.GetEndLinkFrame());
	CHECK(effect.IsDelayFrame());
	CHECK(effect.IsWaitFrame());
	CHECK(effect.IsSkipDraw());
}

TEST(HomingEffect, DescendingHeightStopsAtItsTargetDuringContinuedFlight)
{
	World world;
	EffectProbe effect(BLT_EFFECT, 0, 0);
	Initialize(effect);
	effect.SetPixelPosition(0, 0, 32);
	effect.SetTarget(1000, 0, 16, 1);
	effect.SetLink(SKILL_CLIENT_HALO_ATTACK, nullptr);
	for (int step = 1; step <= 16; ++step)
	{
		CHECK(effect.Update());
		CHECK_EQ(32 - step, effect.GetPixelZ());
	}
	CHECK(effect.HeightStep() == 0.0f);
	CHECK(effect.Update());
	CHECK_EQ(16, effect.GetPixelZ());
	CHECK(effect.Update());
	CHECK_EQ(16, effect.GetPixelZ());
	CHECK_EQ(18, effect.GetPixelX());
}

TEST(HomingEffect, FractionalDescentStopsAtThePlaneWithoutOvershooting)
{
	World world;
	EffectProbe effect(BLT_EFFECT, 0, 0);
	Initialize(effect);
	effect.SetPixelPosition(0, 0, 1);
	effect.SetTarget(1000, 0, 0, 1);
	effect.SetLink(SKILL_CLIENT_HALO_ATTACK, nullptr);
	for (int step = 0; step < 16; ++step) CHECK(effect.Update());
	CHECK(effect.Height() == 0.0f);
	CHECK(effect.HeightStep() == 0.0f);
	CHECK(effect.Update());
	CHECK(effect.Height() == 0.0f);
}

TEST(HomingEffect, SteeringSafelyConvertsRoundedIntegerMaximumPixels)
{
	World world;
	const int high = (std::numeric_limits<int>::max)();
	for (bool vertical : {false, true})
	{
		EffectProbe effect(BLT_EFFECT, 0, 13);
		Initialize(effect);
		effect.SetPixelPosition(vertical ? 0 : high, vertical ? high : 0, 0);
		effect.SetTarget(0, 0, 0, 1);
		CHECK_EQ(0, effect.TargetX());
		CHECK_EQ(0, effect.TargetY());
		CHECK_EQ(high, vertical ? effect.GetPixelY() : effect.GetPixelX());
	}
}

TEST(HomingEffect, RetargetingFromAscentToDescentSettlesAtTheNewHeight)
{
	World world;
	EffectProbe effect(BLT_EFFECT, 0, 0);
	Initialize(effect);
	effect.SetTarget(1000, 0, 16, 1);
	effect.SetLink(SKILL_CLIENT_HALO_ATTACK, nullptr);
	for (int step = 0; step < 4; ++step) CHECK(effect.Update());
	CHECK_EQ(4, effect.GetPixelZ());
	effect.SetTarget(1000, 0, -12, 1);
	for (int step = 0; step < 16; ++step) CHECK(effect.Update());
	CHECK_EQ(-12, effect.GetPixelZ());
	CHECK(effect.HeightStep() == 0.0f);
	CHECK(effect.Update());
	CHECK_EQ(-12, effect.GetPixelZ());
}

TEST(HomingEffect, NonfiniteStoredPixelsUseTheBaseProjectionWhenTargeting)
{
	World world;
	const int high = (std::numeric_limits<int>::max)();
	const int low = (std::numeric_limits<int>::min)();
	const float infinity = std::numeric_limits<float>::infinity();
	const float nan = std::numeric_limits<float>::quiet_NaN();
	struct Example { float x, y; int targetX, targetY; };
	for (const auto& e : {Example{infinity, -infinity, high, low},
		{-infinity, infinity, low, high}, {nan, nan, 0, 0},
		{(std::numeric_limits<float>::max)(), 0, high, 0}})
	{
		EffectProbe effect(BLT_EFFECT, 0, 13);
		effect.SetPixels(e.x, e.y);
		effect.SetTarget(e.targetX, e.targetY, 0, 1);
		CHECK_EQ(e.targetX, effect.TargetX());
		CHECK_EQ(e.targetY, effect.TargetY());
		CHECK_EQ(e.targetX, effect.GetPixelX());
		CHECK_EQ(e.targetY, effect.GetPixelY());
	}
}

TEST(HomingEffect, DisplayOffsetsDoNotAlterStoredPositionSteering)
{
	World world;
	struct DisplayEffect : MHomingEffect
	{
		DisplayEffect() : MHomingEffect(BLT_EFFECT, 90, 90) {}
		int GetPixelX() const override { return 1000; }
		int GetPixelY() const override { return 1000; }
	} effect;
	Initialize(effect);
	effect.SetTarget(192, 0, 0, 64);
	effect.SetLink(SKILL_CLIENT_HALO_ATTACK, nullptr);
	CHECK(effect.Update());
	CHECK_EQ(64, effect.MEffect::GetPixelX());
	CHECK_EQ(0, effect.MEffect::GetPixelY());
	CHECK_EQ(1000, effect.GetPixelX());
	CHECK_EQ(1000, effect.GetPixelY());
}
