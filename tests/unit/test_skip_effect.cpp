#include "test_framework.h"
#include "MSkipEffect.h"

#include <cstdlib>
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
	bool skip;
	bool operator==(const LightRequest&) const = default;
};
std::vector<LightRequest> lights;
MSkipEffect* observed;
const MEffectHost host{
	.CurrentFrame = []() { calls.push_back(1); return frameNow; },
	.Light = [](BYTE blt, TYPE_FRAMEID id, BYTE direction, BYTE frame) {
		calls.push_back(2);
		lights.push_back({blt, id, direction, frame, observed && observed->IsSkipDraw()});
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
	World() { frameNow = 100; lightValue = 7; calls.clear(); lights.clear(); observed = nullptr; }
	~World() { observed = nullptr; MEffect::SetHost(previous); }
};

struct PositionProbe : MSkipEffect
{
	using MSkipEffect::MSkipEffect;
	void SetPixels(float x, float y, float z)
	{
		m_PixelX = x; m_PixelY = y; m_PixelZ = z;
	}
};

} // namespace

TEST(SkipEffect, DefaultStateUsesOneVisibleFrameInThree)
{
	World world;
	MSkipEffect effect(BLT_EFFECT);
	CHECK_EQ(3, effect.GetSkipValue());
	CHECK_EQ(MEffect::EFFECT_SECTOR, effect.GetEffectType());
	CHECK_EQ(MObject::TYPE_EFFECT, effect.GetObjectType());
	CHECK(!effect.IsSkipDraw());
	CHECK_EQ(0, effect.GetFrame());
	CHECK_EQ(0, effect.GetEndFrame());
	CHECK_EQ(ACTIONINFO_NULL, effect.GetActionInfo());
	CHECK(calls.empty());
}

TEST(SkipEffect, ValidSkipValuesAreRetained)
{
	World world;
	MSkipEffect effect(BLT_EFFECT);
	for (int divisor : {1, 2, 7, (std::numeric_limits<int>::max)()})
	{
		effect.SetSkipValue(divisor);
		CHECK_EQ(divisor, effect.GetSkipValue());
	}
}

TEST(SkipEffect, UpdatesStopFourFramesBeforeTheStoredEnd)
{
	World world;
	MSkipEffect effect(BLT_EFFECT);
	effect.SetFrameID(12, 3);
	effect.SetDirection(6);
	effect.SetCount(10);
	effect.SetSkipValue(1);
	CHECK_EQ(109, effect.GetEndFrame());
	frameNow = 104;
	calls.clear(); lights.clear();
	CHECK(effect.Update());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(7, effect.GetLight());
	CHECK(calls == std::vector<int>({1, 2}));
	CHECK(lights == std::vector<LightRequest>({{BLT_EFFECT, 12, 6, 1, false}}));
	frameNow = 105;
	lightValue = 19;
	calls.clear(); lights.clear();
	CHECK(!effect.Update());
	CHECK(!effect.IsEnd());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(7, effect.GetLight());
	CHECK(lights.empty());
}

TEST(SkipEffect, DefaultSkippingUsesTheSharedRandomStreamAndStillAnimates)
{
	World world;
	MSkipEffect effect(BLT_EFFECT);
	effect.SetFrameID(9, 4);
	effect.SetCount(100);
	std::srand(42);
	std::vector<bool> expected;
	for (int i = 0; i < 50; ++i) expected.push_back(std::rand() % 3 != 0);
	const int following = std::rand();
	std::srand(42);
	observed = &effect;
	lights.clear();
	bool sawVisible = false, sawSkipped = false;
	for (int i = 0; i < 50; ++i)
	{
		CHECK(effect.Update());
		CHECK_EQ(expected[i], effect.IsSkipDraw());
		CHECK_EQ((i + 1) % 4, effect.GetFrame());
		CHECK_EQ(expected[i], lights.back().skip);
		CHECK_EQ((i + 1) % 4, lights.back().frame);
		sawSkipped |= effect.IsSkipDraw();
		sawVisible |= !effect.IsSkipDraw();
	}
	CHECK(sawVisible && sawSkipped);
	CHECK_EQ(50, lights.size());
	CHECK_EQ(following, std::rand());
}

TEST(SkipEffect, AOneFrameDivisorClearsTheSkipFlagAndStillConsumesRandomness)
{
	World world;
	MSkipEffect effect(BLT_EFFECT);
	effect.SetCount(10);
	effect.SetSkipValue(1);
	effect.SetDrawSkip(true);
	std::srand(123);
	(void)std::rand();
	const int following = std::rand();
	std::srand(123);
	CHECK(effect.Update());
	CHECK(!effect.IsSkipDraw());
	CHECK_EQ(following, std::rand());
}

TEST(SkipEffect, OneFrameAnimationsWrapAtEveryUpdate)
{
	World world;
	MSkipEffect effect(BLT_EFFECT);
	effect.SetFrameID(1, 1);
	effect.SetCount(20);
	for (int i = 0; i < 5; ++i)
	{
		CHECK(effect.Update());
		CHECK_EQ(0, effect.GetFrame());
		CHECK_EQ(119, effect.GetEndFrame());
	}
}

TEST(SkipEffect, NonAlphaUpdatesPreserveTheirExistingLight)
{
	World world;
	for (const auto blt : {BLT_NORMAL, BLT_SHADOW, BLT_SCREEN})
	{
		MSkipEffect effect(static_cast<BYTE>(blt));
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

TEST(SkipEffect, ExpiredUpdatesPreserveStateAndDoNotConsumeRandomness)
{
	World world;
	MSkipEffect effect(BLT_EFFECT);
	effect.SetFrameID(1, 3);
	effect.SetCount(5);
	effect.SetDrawSkip(true);
	effect.SetLight(11);
	std::srand(456);
	const int next = std::rand();
	std::srand(456);
	calls.clear(); lights.clear();
	CHECK(!effect.Update());
	CHECK_EQ(next, std::rand());
	CHECK(effect.IsSkipDraw());
	CHECK_EQ(0, effect.GetFrame());
	CHECK_EQ(11, effect.GetLight());
	CHECK(calls == std::vector<int>({1}));
	CHECK(lights.empty());
}

TEST(SkipEffect, MissingClockStopsUpdatesWithoutConsumingRandomness)
{
	World world;
	MSkipEffect effect(BLT_EFFECT);
	effect.SetCount(100);
	effect.SetLight(21);
	effect.SetDrawSkip(true);
	MEffect::SetHost(nullptr);
	std::srand(789);
	const int next = std::rand();
	std::srand(789);
	calls.clear();
	CHECK(!effect.Update());
	CHECK_EQ(next, std::rand());
	CHECK_EQ(0, effect.GetFrame());
	CHECK_EQ(21, effect.GetLight());
	CHECK(effect.IsSkipDraw());
	CHECK(calls.empty());
}

TEST(SkipEffect, PartialHostsGuardTheClockAndLightIndependently)
{
	World world;
	MSkipEffect effect(BLT_EFFECT);
	effect.SetCount(20);
	effect.SetLight(17);
	const MEffectHost empty{};
	MEffect::SetHost(&empty);
	CHECK(!effect.Update());
	CHECK_EQ(17, effect.GetLight());
	const MEffectHost lightOnly{.CurrentFrame = nullptr, .Light = host.Light};
	MEffect::SetHost(&lightOnly);
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetFrame());
	const MEffectHost clockOnly{.CurrentFrame = host.CurrentFrame};
	MEffect::SetHost(&clockOnly);
	CHECK(effect.Update());
	CHECK_EQ(1, effect.GetFrame());
	CHECK_EQ(0, effect.GetLight());
}

TEST(SkipEffect, ExistingEffectsObserveHostReplacement)
{
	World world;
	MSkipEffect effect(BLT_EFFECT);
	effect.SetCount(20);
	MEffect::SetHost(&expiredHost);
	CHECK(!effect.Update());
	CHECK_EQ(0, effect.GetFrame());
	MEffect::SetHost(&host);
	CHECK(effect.Update());
	CHECK_EQ(1, effect.GetFrame());
}

TEST(SkipEffect, UpdatesDoNotProjectOrMoveThePosition)
{
	World world;
	PositionProbe effect(BLT_EFFECT);
	effect.SetCount(20);
	effect.SetPosition(13, 14);
	effect.SetPixels(49.875f, -24.875f, 9.5f);
	CHECK(effect.Update());
	CHECK_EQ(13, effect.GetX());
	CHECK_EQ(14, effect.GetY());
	CHECK_EQ(49, effect.GetPixelX());
	CHECK_EQ(-24, effect.GetPixelY());
	CHECK_EQ(9, effect.GetPixelZ());
}

TEST(SkipEffect, LinkDelayAndWaitDeadlinesDoNotControlUpdates)
{
	World world;
	MSkipEffect effect(BLT_EFFECT);
	effect.SetCount(10, 2);
	effect.SetDelayFrame(100);
	effect.SetWaitFrame(100);
	frameNow = 104;
	CHECK_EQ(101, effect.GetEndLinkFrame());
	CHECK(effect.IsDelayFrame());
	CHECK(effect.IsWaitFrame());
	CHECK(effect.Update());
	CHECK_EQ(1, effect.GetFrame());
}

TEST(SkipEffect, WrappedEndFramesRetainAbsoluteUnsignedComparison)
{
	World world;
	MSkipEffect effect(BLT_EFFECT);
	frameNow = (std::numeric_limits<DWORD>::max)() - 3;
	effect.SetCount(10);
	CHECK_EQ(5, effect.GetEndFrame());
	CHECK(!effect.Update());
	frameNow = 0;
	CHECK(effect.Update());
	frameNow = 1;
	CHECK(!effect.Update());
	CHECK_EQ(1, effect.GetFrame());
}
