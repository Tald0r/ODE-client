#include "test_framework.h"
#include "MAttachOrbitEffect.h"
#include "MathTable.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {

DWORD frameNow;
std::vector<int> calls;
MAttachOrbitEffect* observed;
int lightStep, lightFrame, lightPixelX;
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(1); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE frame) {
		calls.push_back(2);
		if (observed)
		{
			lightStep = observed->GetOrbitStep();
			lightFrame = frame;
			lightPixelX = observed->GetPixelX();
		}
		return 7;
	},
};
const MAttachEffectHost attachHost{
	.Sprite = [](TYPE_EFFECTSPRITETYPE, MAttachEffectSprite& sprite) {
		sprite = {BLT_EFFECT, 12, 3};
		return true;
	},
	.LiveCreature = [](TYPE_OBJECTID id, MAttachCreaturePosition& position) {
		calls.push_back(3);
		position = {id, 96, 48, 12};
		return true;
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MAttachEffectHost* previousAttach = MAttachEffect::SetHost(&attachHost);
	std::array<int, MathTable::MAX_ANGLE> sine, cosine;
	std::array<int, MathTable::MAX_ANGLE + 1> tangent;
	World()
	{
		frameNow = 100; observed = nullptr;
		lightStep = lightFrame = lightPixelX = -1; calls.clear();
		std::copy_n(MathTable::FSinTab, sine.size(), sine.begin());
		std::copy_n(MathTable::FCosTab, cosine.size(), cosine.begin());
		std::copy_n(MathTable::FArcTanTab, tangent.size(), tangent.begin());
		if (MathTable::FCosTab[0] != 65536) MathTable::FCreateSines();
		MAttachOrbitEffect::InitOrbitPosition();
	}
	~World()
	{
		observed = nullptr;
		MAttachEffect::SetHost(previousAttach);
		MEffect::SetHost(previousEffect);
		std::copy(sine.begin(), sine.end(), MathTable::FSinTab);
		std::copy(cosine.begin(), cosine.end(), MathTable::FCosTab);
		std::copy(tangent.begin(), tangent.end(), MathTable::FArcTanTab);
	}
};

struct EffectProbe : MAttachOrbitEffect
{
	using MAttachOrbitEffect::MAttachOrbitEffect;
	void SetPixels(float x, float y) { m_PixelX = x; m_PixelY = y; }
};

} // namespace

TEST(AttachOrbitEffect, ConstructionCombinesAttachedLifetimeWithAnOrbit)
{
	World world;
	MAttachOrbitEffect effect(17, 10, 0, 0, 20);
	CHECK_EQ(MEffect::EFFECT_ATTACH_ORBIT, effect.GetEffectType());
	CHECK_EQ(17, effect.GetEffectSpriteType());
	CHECK_EQ(12, effect.GetFrameID());
	CHECK_EQ(3, effect.GetMaxFrame());
	CHECK_EQ(109, effect.GetEndFrame());
	CHECK_EQ(119, effect.GetEndLinkFrame());
	CHECK_EQ(0, effect.GetOrbitStep());
	CHECK_EQ(96, effect.GetPixelX());
	CHECK_EQ(0, effect.GetPixelY());
	CHECK_EQ(0, effect.GetPixelZ());
	CHECK_EQ(static_cast<TYPE_SECTORPOSITION>(-1), effect.GetX());
	CHECK_EQ(static_cast<TYPE_SECTORPOSITION>(-1), effect.GetY());
	CHECK_EQ(OBJECTID_NULL, effect.GetAttachCreatureID());
}

TEST(AttachOrbitEffect, EveryOrbitProjectsItsCardinalOffsetsAroundTheStoredPosition)
{
	World world;
	struct Path { int type, quarter, width, height; };
	for (const auto path : {Path{0, 8, 96, 48}, Path{1, 8, 48, 24}, Path{2, 16, 24, 12}})
	{
		MAttachOrbitEffect effect(17, 10, path.type, 0);
		effect.SetPixelPosition(240, 120, 30);
		CHECK_EQ(240 + path.width, effect.GetPixelX());
		CHECK_EQ(120, effect.GetPixelY());
		effect.SetOrbitStep(path.quarter);
		CHECK_EQ(240, effect.GetPixelX());
		CHECK_EQ(120 + path.height - 1, effect.GetPixelY());
		effect.SetOrbitStep(path.quarter * 2);
		CHECK_EQ(240 - path.width, effect.GetPixelX());
		CHECK_EQ(120, effect.GetPixelY());
		effect.SetOrbitStep(path.quarter * 3);
		CHECK_EQ(240, effect.GetPixelX());
		CHECK_EQ(120 - path.height, effect.GetPixelY());
		CHECK_EQ(30, effect.GetPixelZ());
		CHECK_EQ(5, effect.GetX());
		CHECK_EQ(5, effect.GetY());
	}
}

TEST(AttachOrbitEffect, OrdinaryProjectionRetainsFixedPointRounding)
{
	World world;
	MAttachOrbitEffect effect(17, 10, 0, 4);
	CHECK_EQ(67, effect.GetPixelX());
	CHECK_EQ(33, effect.GetPixelY());
	CHECK_EQ(67, effect.GetOrbitPosition().x);
	CHECK_EQ(33, effect.GetOrbitPosition().y);
	effect.SetOrbitStep(20);
	CHECK_EQ(-68, effect.GetPixelX());
	CHECK_EQ(-34, effect.GetPixelY());
}

TEST(AttachOrbitEffect, ExplicitStartingAndAssignedStepsNormalizeAcrossTheIntegerRange)
{
	World world;
	struct Case { int step, expected; };
	for (const auto value : {Case{-2, 62}, Case{-64, 0}, Case{65, 1},
		Case{(std::numeric_limits<int>::min)(), 0}, Case{(std::numeric_limits<int>::max)(), 63}})
	{
		MAttachOrbitEffect effect(17, 10, 0, value.step);
		CHECK_EQ(value.expected, effect.GetOrbitStep());
		effect.SetOrbitStep(value.step);
		CHECK_EQ(value.expected, effect.GetOrbitStep());
	}
}

TEST(AttachOrbitEffect, DefaultConstructionConsumesExactlyOneRandomDraw)
{
	World world;
	std::srand(4817);
	const int expectedStep = std::rand() % 64;
	const int expectedNext = std::rand();
	std::srand(4817);
	MAttachOrbitEffect effect(17, 10, 0);
	CHECK_EQ(expectedStep, effect.GetOrbitStep());
	CHECK_EQ(expectedNext, std::rand());
}

TEST(AttachOrbitEffect, ExplicitConstructionAndStepAssignmentDoNotDrawRandomness)
{
	World world;
	std::srand(4817);
	const int expectedNext = std::rand();
	std::srand(4817);
	MAttachOrbitEffect effect(17, 10, 0, 0);
	effect.SetOrbitStep(-1);
	CHECK_EQ(63, effect.GetOrbitStep());
	CHECK_EQ(expectedNext, std::rand());
}

TEST(AttachOrbitEffect, UpdateAnimatesAndLightsBeforeAdvancingTheOrbit)
{
	World world;
	MAttachOrbitEffect effect(17, 10, 0, 0);
	effect.SetPixelPosition(96, 48, 12);
	observed = &effect;
	calls.clear();
	CHECK(effect.Update());
	CHECK(calls == std::vector<int>({1, 2}));
	CHECK_EQ(0, lightStep);
	CHECK_EQ(1, lightFrame);
	CHECK_EQ(192, lightPixelX);
	CHECK_EQ(1, effect.GetOrbitStep());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(7, effect.GetLight());
	CHECK_EQ(96, effect.MEffect::GetPixelX());
	CHECK_EQ(48, effect.MEffect::GetPixelY());
	CHECK_EQ(2, effect.GetX());
	CHECK_EQ(2, effect.GetY());
}

TEST(AttachOrbitEffect, PausingTheOrbitStillAdvancesAttachmentAnimation)
{
	World world;
	MAttachOrbitEffect effect(17, 10, 0, 10);
	effect.SetOrbitRunning(false);
	CHECK(effect.Update());
	CHECK_EQ(10, effect.GetOrbitStep());
	CHECK_EQ(1, effect.GetFrame());
	effect.SetOrbitRunning(true);
	CHECK(effect.Update());
	CHECK_EQ(11, effect.GetOrbitStep());
	CHECK_EQ(2, effect.GetFrame());
}

TEST(AttachOrbitEffect, ExpiryStopsBothAnimationAndOrbitWithoutLosingPosition)
{
	World world;
	MAttachOrbitEffect effect(17, 10, 0, 63);
	frameNow = 109;
	const int x = effect.GetPixelX(), y = effect.GetPixelY();
	calls.clear();
	CHECK(!effect.Update());
	CHECK(calls == std::vector<int>({1}));
	CHECK_EQ(63, effect.GetOrbitStep());
	CHECK_EQ(0, effect.GetFrame());
	CHECK_EQ(x, effect.GetPixelX());
	CHECK_EQ(y, effect.GetPixelY());
}

TEST(AttachOrbitEffect, ExplicitAdvanceWrapsEvenWhenPausedAndExpired)
{
	World world;
	MAttachOrbitEffect effect(17, 1, 0, 63);
	effect.SetOrbitRunning(false);
	CHECK(!effect.Update());
	effect.NextOrbitStep();
	CHECK_EQ(0, effect.GetOrbitStep());
	CHECK_EQ(96, effect.GetPixelX());
	CHECK_EQ(0, effect.GetFrame());
}

TEST(AttachOrbitEffect, AttachmentProjectsTheBasePositionWithoutIncludingItsOrbit)
{
	World world;
	MAttachOrbitEffect effect(17, 10, 0, 0);
	effect.SetAttachCreatureID(7);
	CHECK_EQ(7, effect.GetAttachCreatureID());
	CHECK_EQ(192, effect.GetPixelX());
	CHECK_EQ(48, effect.GetPixelY());
	CHECK_EQ(12, effect.GetPixelZ());
	CHECK_EQ(2, effect.GetX());
	CHECK_EQ(2, effect.GetY());
	effect.SetOrbitStep(16);
	CHECK_EQ(0, effect.GetPixelX());
	effect.SetAttachCreatureID(8);
	CHECK_EQ(8, effect.GetAttachCreatureID());
	CHECK_EQ(0, effect.GetPixelX());
	CHECK_EQ(2, effect.GetX());
	CHECK_EQ(16, effect.GetOrbitStep());
}

TEST(AttachOrbitEffect, PositionChangesReplaceTheBaseWithoutApplyingAnOffsetTwice)
{
	World world;
	MAttachOrbitEffect effect(17, 10, 0, 0);
	effect.SetPixelPosition(96, 48, 12);
	effect.SetPixelPosition(144, 72, 24);
	CHECK_EQ(240, effect.GetPixelX());
	CHECK_EQ(72, effect.GetPixelY());
	CHECK_EQ(24, effect.GetPixelZ());
	CHECK_EQ(3, effect.GetX());
	CHECK_EQ(3, effect.GetY());
}

TEST(AttachOrbitEffect, MissingTargetStopsFutureOrbitUpdates)
{
	World world;
	MAttachOrbitEffect effect(17, 10, 0, 10);
	effect.SetAttachCreatureID(7);
	MAttachEffect::SetHost(nullptr);
	effect.SetAttachCreatureID(8);
	CHECK_EQ(0, effect.GetEndFrame());
	CHECK_EQ(7, effect.GetAttachCreatureID());
	CHECK(!effect.Update());
	CHECK_EQ(10, effect.GetOrbitStep());
	CHECK_EQ(0, effect.GetFrame());
}

TEST(AttachOrbitEffect, MissingClockStopsEvenAPermanentOrbit)
{
	World world;
	MAttachOrbitEffect effect(17, 0xFFFF, 0, 10);
	MEffect::SetHost(nullptr);
	CHECK(!effect.Update());
	CHECK_EQ(10, effect.GetOrbitStep());
	MEffect::SetHost(&effectHost);
	CHECK(effect.Update());
	CHECK_EQ(11, effect.GetOrbitStep());
}

TEST(AttachOrbitEffect, WrappedFiniteLifetimeUsesTheAttachedAbsoluteDeadline)
{
	World world;
	frameNow = (std::numeric_limits<DWORD>::max)() - 1;
	MAttachOrbitEffect effect(17, 4, 0, 63);
	CHECK_EQ(1, effect.GetEndFrame());
	CHECK(!effect.Update());
	CHECK_EQ(63, effect.GetOrbitStep());
	frameNow = 0;
	CHECK(effect.Update());
	CHECK_EQ(0, effect.GetOrbitStep());
	frameNow = 1;
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetOrbitStep());
}

TEST(AttachOrbitEffect, LinkDelayWaitAndDrawSkipDoNotPauseAnActiveOrbit)
{
	World world;
	MAttachOrbitEffect effect(17, 10, 0, 0, 1);
	effect.SetDelayFrame(100);
	effect.SetWaitFrame(100);
	effect.SetDrawSkip(true);
	CHECK(effect.Update());
	CHECK_EQ(1, effect.GetOrbitStep());
	CHECK_EQ(100, effect.GetEndLinkFrame());
	CHECK(effect.IsDelayFrame());
	CHECK(effect.IsWaitFrame());
	CHECK(effect.IsSkipDraw());
}

TEST(AttachOrbitEffect, DifferentInstancesKeepIndependentOrbitState)
{
	World world;
	MAttachOrbitEffect first(17, 10, 0, 5), second(17, 10, 1, 7);
	first.SetOrbitRunning(false);
	CHECK(first.Update());
	CHECK(second.Update());
	CHECK_EQ(5, first.GetOrbitStep());
	CHECK_EQ(8, second.GetOrbitStep());
	second.SetOrbitStep(20);
	CHECK_EQ(5, first.GetOrbitStep());
}

TEST(AttachOrbitEffect, CachedPositionReferencesOutliveTheEffect)
{
	World world;
	const POINT* position;
	{
		MAttachOrbitEffect effect(17, 10, 0, 0);
		position = &effect.GetOrbitPosition();
		effect.NextOrbitStep();
	}
	CHECK_EQ(96, position->x);
	CHECK_EQ(0, position->y);
}

TEST(AttachOrbitEffect, UnknownOrbitTypesKeepTheirBasePositionWhileStepsAdvance)
{
	World world;
	for (int type : {-1, 3, (std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()})
	{
		MAttachOrbitEffect effect(17, 10, type, 63);
		effect.SetPixelPosition(96, 48, 12);
		CHECK_EQ(96, effect.GetPixelX());
		CHECK_EQ(48, effect.GetPixelY());
		CHECK(effect.Update());
		CHECK_EQ(0, effect.GetOrbitStep());
		CHECK_EQ(96, effect.GetPixelX());
		CHECK_EQ(48, effect.GetPixelY());
	}
}

TEST(AttachOrbitEffect, FractionalBaseCoordinatesTruncateBeforeAddingTheOrbit)
{
	World world;
	EffectProbe effect(17, 10, 0, 4);
	effect.SetPixels(-0.75f, -0.75f);
	CHECK_EQ(67, effect.GetPixelX());
	CHECK_EQ(33, effect.GetPixelY());
	effect.SetOrbitStep(20);
	effect.SetPixels(0.75f, 0.75f);
	CHECK_EQ(-68, effect.GetPixelX());
	CHECK_EQ(-34, effect.GetPixelY());
}

TEST(AttachOrbitEffect, DisplayCoordinatesSaturateAfterAddingTheOrbit)
{
	World world;
	const int low = (std::numeric_limits<int>::min)();
	const int high = (std::numeric_limits<int>::max)();
	EffectProbe effect(17, 10, 0, 20);
	effect.SetPixels(static_cast<float>(low), static_cast<float>(low));
	CHECK_EQ(low, effect.GetPixelX());
	CHECK_EQ(low, effect.GetPixelY());
	effect.SetOrbitStep(4);
	effect.SetPixels(static_cast<float>(high), static_cast<float>(high));
	CHECK_EQ(high, effect.GetPixelX());
	CHECK_EQ(high, effect.GetPixelY());
}

TEST(AttachOrbitEffect, OppositeOffsetsCanBringARoundedBoundaryBackIntoRange)
{
	World world;
	const int high = (std::numeric_limits<int>::max)();
	EffectProbe effect(17, 10, 0, 16);
	// INT_MAX rounds to 2^31 in the float position; subtract before clamping.
	effect.SetPixels(static_cast<float>(high), static_cast<float>(high));
	CHECK_EQ(high - 95, effect.GetPixelX());
	effect.SetOrbitStep(24);
	CHECK_EQ(high - 47, effect.GetPixelY());
}

TEST(AttachOrbitEffect, NonfiniteBaseCoordinatesHaveBoundedDisplayResults)
{
	World world;
	const float inf = std::numeric_limits<float>::infinity();
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EffectProbe effect(17, 10, 0, 4);
	effect.SetPixels(inf, -inf);
	CHECK_EQ((std::numeric_limits<int>::max)(), effect.GetPixelX());
	CHECK_EQ((std::numeric_limits<int>::min)(), effect.GetPixelY());
	effect.SetPixels(nan, nan);
	CHECK_EQ(67, effect.GetPixelX());
	CHECK_EQ(33, effect.GetPixelY());
}

TEST(AttachOrbitEffect, PublicPositionInputUsesTheSameBoundedDisplayProjection)
{
	World world;
	const int high = (std::numeric_limits<int>::max)();
	const int low = (std::numeric_limits<int>::min)();
	MAttachOrbitEffect effect(17, 10, 0, 4);
	effect.SetPixelPosition(high, low, 12);
	CHECK_EQ(high, effect.GetPixelX());
	CHECK_EQ(low + 33, effect.GetPixelY());
	CHECK_EQ(high, effect.MEffect::GetPixelX());
	CHECK_EQ(low, effect.MEffect::GetPixelY());
	CHECK_EQ(12, effect.GetPixelZ());
	effect.SetOrbitStep(20);
	CHECK_EQ(high - 67, effect.GetPixelX());
	CHECK_EQ(low, effect.GetPixelY());
}

TEST(AttachOrbitEffect, RepresentableNearLimitSumsKeepTheirExactOffsets)
{
	World world;
	const int low = (std::numeric_limits<int>::min)();
	EffectProbe effect(17, 10, 0, 0);
	// The preceding float below 2^31 leaves room for the entire orbit width.
	effect.SetPixels(2147483520.0f, static_cast<float>(low));
	CHECK_EQ(2147483616, effect.GetPixelX());
	effect.SetOrbitStep(8);
	CHECK_EQ(low + 47, effect.GetPixelY());
	effect.SetPixels(static_cast<float>(low), 2147483520.0f);
	effect.SetOrbitStep(0);
	CHECK_EQ(low + 96, effect.GetPixelX());
	effect.SetOrbitStep(8);
	CHECK_EQ(2147483567, effect.GetPixelY());
}
