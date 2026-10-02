#include "test_framework.h"
#include "MMovingEffect.h"
#include "MScreenEffect.h"
#include "MViewDef.h"

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
MEffect* observedEffect;
int lightSector;
const MEffectHost host{
	.CurrentFrame = []() { calls.push_back(1); return frameNow; },
	.Light = [](BYTE blt, TYPE_FRAMEID id, BYTE direction, BYTE frame) {
		calls.push_back(2);
		lights.push_back({blt, id, direction, frame});
		if (observedEffect) lightSector = observedEffect->GetX();
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
	int basisX = MScreenEffect::m_ScreenBasisX;
	int basisY = MScreenEffect::m_ScreenBasisY;
	World()
	{
		frameNow = 100; lightValue = 7; observedEffect = nullptr; lightSector = -1;
		calls.clear(); lights.clear(); MScreenEffect::SetScreenBasis(0, 0);
	}
	~World()
	{
		observedEffect = nullptr;
		MEffect::SetHost(previous);
		MScreenEffect::SetScreenBasis(basisX, basisY);
	}
};

template<class Base>
struct PositionProbe : Base
{
	using Base::Base;
	void SetPixels(float x, float y, float z)
	{
		this->m_PixelX = x; this->m_PixelY = y; this->m_PixelZ = z;
	}
};

template<class Effect>
void CheckLifecycle()
{
	Effect effect(BLT_EFFECT);
	effect.SetFrameID(42, 3);
	effect.SetDirection(6);
	effect.SetCount(3);
	calls.clear(); lights.clear();
	CHECK(effect.Update());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(7, effect.GetLight());
	CHECK(calls == std::vector<int>({1, 2}));
	CHECK(lights == std::vector<LightRequest>({{BLT_EFFECT, 42, 6, 1}}));
	frameNow = 101;
	lightValue = 9;
	CHECK(effect.Update());
	CHECK_EQ(2, effect.GetFrame());
	CHECK_EQ(9, effect.GetLight());
	frameNow = 102;
	calls.clear(); lights.clear();
	CHECK(!effect.Update());
	CHECK_EQ(2, effect.GetFrame());
	CHECK_EQ(9, effect.GetLight());
	CHECK(calls == std::vector<int>({1}));
	CHECK(lights.empty());
}

template<class Effect>
void CheckNonAlphaLighting()
{
	for (const auto blt : {BLT_NORMAL, BLT_SHADOW, BLT_SCREEN})
	{
		Effect effect(static_cast<BYTE>(blt));
		effect.SetFrameID(1, 2);
		effect.SetCount(10);
		effect.SetLight(19);
		calls.clear(); lights.clear();
		CHECK(effect.Update());
		CHECK_EQ(1, effect.GetFrame());
		CHECK_EQ(19, effect.GetLight());
		CHECK(calls == std::vector<int>({1}));
		CHECK(lights.empty());
	}
}

template<class Effect>
void CheckMissingHost()
{
	Effect effect(BLT_EFFECT);
	effect.SetFrameID(2, 3);
	effect.SetCount(50);
	effect.SetLight(21);
	MEffect::SetHost(nullptr);
	calls.clear();
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetFrame());
	CHECK_EQ(21, effect.GetLight());
	CHECK(calls.empty());
	const MEffectHost empty{};
	MEffect::SetHost(&empty);
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetFrame());
	const MEffectHost clockOnly{.CurrentFrame = host.CurrentFrame};
	MEffect::SetHost(&clockOnly);
	CHECK(effect.Update());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(0, effect.GetLight());
	MEffect::SetHost(&host);
}

} // namespace

TEST(MovingScreenEffects, TypesAndDefaultBaseStateRemainAvailable)
{
	World world;
	MMovingEffect moving(BLT_EFFECT);
	MScreenEffect screen(BLT_SCREEN);
	CHECK_EQ(MEffect::EFFECT_MOVING, moving.GetEffectType());
	CHECK_EQ(MEffect::EFFECT_SCREEN, screen.GetEffectType());
	CHECK_EQ(MObject::TYPE_EFFECT, moving.GetObjectType());
	CHECK_EQ(MObject::TYPE_EFFECT, screen.GetObjectType());
	CHECK_EQ(0, screen.GetScreenX());
	CHECK_EQ(0, screen.GetScreenY());
	CHECK_EQ(SECTORPOSITION_NULL, moving.GetX());
	CHECK_EQ(SECTORPOSITION_NULL, screen.GetX());
	CHECK(!moving.Update());
	CHECK(!screen.Update());
	CHECK(lights.empty());
}

TEST(MovingScreenEffects, MovingStopsBeforeAnimationAndLightingAtExpiry)
{
	World world;
	CheckLifecycle<MMovingEffect>();
}

TEST(MovingScreenEffects, ScreenStopsBeforeAnimationAndLightingAtExpiry)
{
	World world;
	CheckLifecycle<MScreenEffect>();
}

TEST(MovingScreenEffects, MovingPreservesNonAlphaLighting)
{
	World world;
	CheckNonAlphaLighting<MMovingEffect>();
}

TEST(MovingScreenEffects, ScreenPreservesNonAlphaLighting)
{
	World world;
	CheckNonAlphaLighting<MScreenEffect>();
}

TEST(MovingScreenEffects, MovingProjectsPixelsAfterItsLightLookup)
{
	World world;
	PositionProbe<MMovingEffect> effect(BLT_EFFECT);
	effect.SetFrameID(1, 3);
	effect.SetCount(10);
	effect.SetPosition(13, 14);
	effect.SetPixels(49.875f, 50.25f, 9.5f);
	observedEffect = &effect;
	CHECK(effect.Update());
	CHECK_EQ(13, lightSector);
	CHECK_EQ(1, effect.GetX());
	CHECK_EQ(2, effect.GetY());
	CHECK_EQ(49, effect.GetPixelX());
	CHECK_EQ(50, effect.GetPixelY());
	CHECK_EQ(9, effect.GetPixelZ());
}

TEST(MovingScreenEffects, ExpiredMovingEffectDoesNotProjectPixels)
{
	World world;
	PositionProbe<MMovingEffect> effect(BLT_EFFECT);
	effect.SetCount(1);
	effect.SetPosition(13, 14);
	effect.SetPixels(49.875f, 50.25f, 9.5f);
	CHECK(!effect.Update());
	CHECK_EQ(13, effect.GetX());
	CHECK_EQ(14, effect.GetY());
	CHECK_EQ(0, effect.GetFrame());
}

TEST(MovingScreenEffects, MovingProjectionRetainsSignedDivisionAndSectorWrap)
{
	World world;
	PositionProbe<MMovingEffect> effect(BLT_NORMAL);
	effect.SetCount(10);
	effect.SetPixels(-47.875f, -48.75f, 0);
	CHECK(effect.Update());
	CHECK_EQ(0, effect.GetX());
	CHECK_EQ(static_cast<TYPE_SECTORPOSITION>(-2), effect.GetY());
}

TEST(MovingScreenEffects, MovingWithoutAHostStopsAndMissingLightingIsZero)
{
	World world;
	CheckMissingHost<MMovingEffect>();
}

TEST(MovingScreenEffects, ScreenWithoutAHostStopsAndMissingLightingIsZero)
{
	World world;
	CheckMissingHost<MScreenEffect>();
}

TEST(MovingScreenEffects, ExistingObjectsObserveHostReplacement)
{
	World world;
	MMovingEffect moving(BLT_EFFECT);
	MScreenEffect screen(BLT_EFFECT);
	moving.SetFrameID(1, 3); screen.SetFrameID(2, 3);
	moving.SetCount(10); screen.SetCount(10);
	MEffect::SetHost(&expiredHost);
	CHECK(!moving.Update()); CHECK(!screen.Update());
	CHECK_EQ(0, moving.GetFrame()); CHECK_EQ(0, screen.GetFrame());
	MEffect::SetHost(&host);
	CHECK(moving.Update()); CHECK(screen.Update());
	CHECK_EQ(1, moving.GetFrame()); CHECK_EQ(1, screen.GetFrame());
}

TEST(MovingScreenEffects, OneFrameAnimationsWrapWithoutAffectingLifetime)
{
	World world;
	MMovingEffect moving(BLT_EFFECT);
	MScreenEffect screen(BLT_EFFECT);
	moving.SetFrameID(1, 1); screen.SetFrameID(2, 1);
	moving.SetCount(10); screen.SetCount(10);
	for (int i = 0; i < 3; ++i)
	{
		CHECK(moving.Update()); CHECK(screen.Update());
		CHECK_EQ(0, moving.GetFrame()); CHECK_EQ(0, screen.GetFrame());
		CHECK_EQ(109, moving.GetEndFrame()); CHECK_EQ(109, screen.GetEndFrame());
	}
}

TEST(MovingScreenEffects, WrappedDeadlinesRetainAbsoluteUnsignedComparison)
{
	World world;
	MMovingEffect moving(BLT_EFFECT);
	MScreenEffect screen(BLT_EFFECT);
	frameNow = (std::numeric_limits<DWORD>::max)() - 1;
	moving.SetCount(4); screen.SetCount(4);
	CHECK(!moving.Update()); CHECK(!screen.Update());
	frameNow = 0;
	CHECK(moving.Update()); CHECK(screen.Update());
	frameNow = 1;
	CHECK(!moving.Update()); CHECK(!screen.Update());
}

TEST(MovingScreenEffects, DrawDelaysAndWaitsDoNotChangeUpdatePolicy)
{
	World world;
	MMovingEffect moving(BLT_EFFECT);
	MScreenEffect screen(BLT_EFFECT);
	moving.SetCount(10); screen.SetCount(10);
	moving.SetDelayFrame(100); screen.SetWaitFrame(100);
	moving.SetDrawSkip(true); screen.SetDrawSkip(true);
	CHECK(moving.Update()); CHECK(screen.Update());
	CHECK(moving.IsDelayFrame()); CHECK(screen.IsWaitFrame());
	CHECK(moving.IsSkipDraw()); CHECK(screen.IsSkipDraw());
}

TEST(MovingScreenEffects, ScreenPositionStoresAnOffsetAndPreservesHeightAndSector)
{
	World world;
	MScreenEffect effect(BLT_EFFECT);
	effect.SetPosition(11, 22);
	effect.SetZ(33);
	MScreenEffect::SetScreenBasis(100, -200);
	effect.SetScreenPosition(125, -240);
	CHECK_EQ(25, effect.GetPixelX());
	CHECK_EQ(-40, effect.GetPixelY());
	CHECK_EQ(33, effect.GetPixelZ());
	CHECK_EQ(125, effect.GetScreenX());
	CHECK_EQ(-240, effect.GetScreenY());
	CHECK_EQ(11, effect.GetX());
	CHECK_EQ(22, effect.GetY());
	effect.SetCount(10);
	CHECK(effect.Update());
	CHECK_EQ(11, effect.GetX());
	CHECK_EQ(22, effect.GetY());
}

TEST(MovingScreenEffects, BasisChangesMoveAllExistingScreenEffects)
{
	World world;
	MScreenEffect first(BLT_EFFECT), second(BLT_EFFECT);
	MScreenEffect::SetScreenBasis(100, 200);
	first.SetScreenPosition(110, 220);
	second.SetScreenPosition(85, 175);
	MScreenEffect::SetScreenBasis(-300, 500);
	CHECK_EQ(-290, first.GetScreenX());
	CHECK_EQ(520, first.GetScreenY());
	CHECK_EQ(-315, second.GetScreenX());
	CHECK_EQ(475, second.GetScreenY());
	CHECK_EQ(10, first.GetPixelX());
	CHECK_EQ(-15, second.GetPixelX());
}

TEST(MovingScreenEffects, ResettingScreenPositionUsesTheCurrentBasis)
{
	World world;
	MScreenEffect effect(BLT_EFFECT);
	MScreenEffect::SetScreenBasis(100, 200);
	effect.SetScreenPosition(110, 220);
	MScreenEffect::SetScreenBasis(-300, 500);
	effect.SetScreenPosition(12, -34);
	CHECK_EQ(312, effect.GetPixelX());
	CHECK_EQ(-534, effect.GetPixelY());
	CHECK_EQ(12, effect.GetScreenX());
	CHECK_EQ(-34, effect.GetScreenY());
}

TEST(MovingScreenEffects, ScreenCoordinatesTruncateOffsetsBeforeAddingTheBasis)
{
	World world;
	PositionProbe<MScreenEffect> effect(BLT_EFFECT);
	MScreenEffect::SetScreenBasis(100, -100);
	effect.SetPixels(-0.75f, 0.75f, 0);
	CHECK_EQ(100, effect.GetScreenX());
	CHECK_EQ(-100, effect.GetScreenY());
	effect.SetPixels(-2.75f, 2.75f, 0);
	CHECK_EQ(98, effect.GetScreenX());
	CHECK_EQ(-98, effect.GetScreenY());
}

TEST(MovingScreenEffects, ScreenPositionDifferenceCanSpanTheFullIntegerRange)
{
	World world;
	const int low = (std::numeric_limits<int>::min)();
	const int high = (std::numeric_limits<int>::max)();
	MScreenEffect effect(BLT_EFFECT);
	MScreenEffect::SetScreenBasis(low, high);
	effect.SetScreenPosition(high, low);
	MScreenEffect::SetScreenBasis(0, 0);
	CHECK_EQ(high, effect.GetScreenX());
	CHECK_EQ(low, effect.GetScreenY());
}

TEST(MovingScreenEffects, ScreenCoordinatesSaturateWhenTheBasisPushesThemOutOfRange)
{
	World world;
	const int low = (std::numeric_limits<int>::min)();
	const int high = (std::numeric_limits<int>::max)();
	MScreenEffect effect(BLT_EFFECT);
	effect.SetScreenPosition(100, -100);
	MScreenEffect::SetScreenBasis(high, low);
	CHECK_EQ(high, effect.GetScreenX());
	CHECK_EQ(low, effect.GetScreenY());
}

TEST(MovingScreenEffects, MaximumIntegerPositionDoesNotConvertARoundedFloatOutOfRange)
{
	World world;
	const int low = (std::numeric_limits<int>::min)();
	const int high = (std::numeric_limits<int>::max)();
	MScreenEffect effect(BLT_EFFECT);
	effect.SetScreenPosition(high, low);
	CHECK_EQ(high, effect.GetScreenX());
	CHECK_EQ(low, effect.GetScreenY());
}

TEST(MovingScreenEffects, WideScreenOffsetsCanCancelAgainstTheBasis)
{
	World world;
	PositionProbe<MScreenEffect> effect(BLT_EFFECT);
	MScreenEffect::SetScreenBasis((std::numeric_limits<int>::min)(),
		(std::numeric_limits<int>::max)());
	effect.SetPixels(2147483648.0f, -2147483648.0f, 0);
	CHECK_EQ(0, effect.GetScreenX());
	CHECK_EQ(-1, effect.GetScreenY());
}

TEST(MovingScreenEffects, ScreenProjectionPreservesExactIntegerEndpoints)
{
	World world;
	PositionProbe<MScreenEffect> effect(BLT_EFFECT);
	effect.SetPixels(2147483520.0f, -2147483520.0f, 0);
	MScreenEffect::SetScreenBasis(127, -128);
	CHECK_EQ((std::numeric_limits<int>::max)(), effect.GetScreenX());
	CHECK_EQ((std::numeric_limits<int>::min)(), effect.GetScreenY());
}

TEST(MovingScreenEffects, NonfiniteScreenOffsetsHaveBoundedResults)
{
	World world;
	PositionProbe<MScreenEffect> effect(BLT_EFFECT);
	MScreenEffect::SetScreenBasis(17, -23);
	const float infinity = std::numeric_limits<float>::infinity();
	const float nan = std::numeric_limits<float>::quiet_NaN();
	effect.SetPixels(infinity, -infinity, 0);
	CHECK_EQ((std::numeric_limits<int>::max)(), effect.GetScreenX());
	CHECK_EQ((std::numeric_limits<int>::min)(), effect.GetScreenY());
	effect.SetPixels(nan, nan, 0);
	CHECK_EQ(17, effect.GetScreenX());
	CHECK_EQ(-23, effect.GetScreenY());
}
