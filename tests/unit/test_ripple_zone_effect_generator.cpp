#include "test_framework.h"
#include "MRippleZoneEffectGenerator.h"
#include "MEffect.h"
#include "EffectSpriteTypeDef.h"

#include <memory>
#include <stdexcept>
#include <vector>

namespace {
MFixedZoneEffectSprite sprite;
TYPE_SECTORPOSITION width, height;
bool accepted, metadataAvailable, ground;
int submissions;
DWORD now;
std::vector<int> calls;
std::unique_ptr<MEffect> retained;
const MRippleZoneEffectHost host{
	.Bounds = [](TYPE_SECTORPOSITION& w, TYPE_SECTORPOSITION& h) { calls.push_back(1); w = width; h = height; return true; },
	.Sprite = [](TYPE_EFFECTSPRITETYPE, MFixedZoneEffectSprite& result) { calls.push_back(2); result = sprite; return metadataAvailable; },
	.Queue = [](std::unique_ptr<MEffect> effect, bool isGround) {
		calls.push_back(3); ++submissions; ground = isGround;
		CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		if (!accepted) return false;
		retained = std::move(effect); return true;
	},
};
const MEffectHost effectHost{.CurrentFrame = [] { return now; }};
struct World
{
	const MRippleZoneEffectHost* old = MRippleZoneEffectGenerator::SetHost(&host);
	const MEffectHost* oldEffect = MEffect::SetHost(&effectHost);
	MRippleZoneEffectGenerator generator;
	World() { Reset(); width = height = 20; sprite = {BLT_EFFECT, 12, 3}; metadataAvailable = true; now = 100; }
	void Reset() { retained.reset(); calls.clear(); submissions = 0; accepted = true; ground = false; }
	~World() { retained.reset(); MRippleZoneEffectGenerator::SetHost(old); MEffect::SetHost(oldEffect); }
};
EFFECTGENERATOR_INFO Info()
{
	EFFECTGENERATOR_INFO info{}; info.effectSpriteType = 17; info.nActionInfo = 42;
	info.x0 = 240; info.y0 = 120; info.z0 = 17; info.x1 = 999; info.y1 = 999; info.z1 = 999;
	info.direction = DIRECTION_LEFT; info.step = 11; info.power = 7; info.count = 30; info.linkCount = 5;
	return info;
}
bool Generate(World& world, EFFECTGENERATOR_INFO info, std::unique_ptr<MEffectTarget>& target)
{
	info.pEffectTarget = target.get(); const bool result = world.generator.Generate(info);
	if (retained && retained->GetEffectTarget() == target.get()) target.release();
	return result;
}
}

TEST(RippleZoneEffectGenerator, EveryDirectionBytePreservesTheOriginalStepAndConfiguresARealEffect)
{
	World world; CHECK_EQ(EFFECTGENERATORID_RIPPLE_ZONE, world.generator.GetID());
	const int positions[8][2] = {{4,5},{4,6},{5,6},{6,6},{6,5},{6,4},{5,4},{4,4}};
	for (int direction = 0; direction < 256; ++direction)
	{
		world.Reset(); auto target = std::make_unique<MEffectTarget>(3); auto* original = target.get();
		auto info = Info(); info.direction = static_cast<BYTE>(direction); CHECK(Generate(world, info, target)); CHECK(!ground);
		CHECK(calls == std::vector<int>({1,2,3})); CHECK(retained->GetEffectTarget() == original);
		CHECK_EQ(direction < 8 ? positions[direction][0] : 5, retained->GetX()); CHECK_EQ(direction < 8 ? positions[direction][1] : 5, retained->GetY());
		CHECK_EQ(17, retained->GetPixelZ()); CHECK_EQ(direction, retained->GetDirection()); CHECK_EQ(11, retained->GetStepPixel());
		CHECK_EQ(7, retained->GetPower()); CHECK_EQ(BLT_EFFECT, retained->GetBltType()); CHECK_EQ(12, retained->GetFrameID());
		CHECK_EQ(3, retained->GetMaxFrame()); CHECK_EQ(129, retained->GetEndFrame()); CHECK_EQ(104, retained->GetEndLinkFrame()); CHECK_EQ(42, retained->GetActionInfo());
	}
}

TEST(RippleZoneEffectGenerator, SixGroundVariantsTransferTheirTargetsAndActionLinks)
{
	World world;
	for (const auto type : {EFFECTSPRITETYPE_EARTHQUAKE_1, EFFECTSPRITETYPE_EARTHQUAKE_2, EFFECTSPRITETYPE_EARTHQUAKE_3,
		EFFECTSPRITETYPE_POWER_OF_LAND_STONE_1, EFFECTSPRITETYPE_POWER_OF_LAND_STONE_2, EFFECTSPRITETYPE_POWER_OF_LAND_STONE_3})
	{
		world.Reset(); auto target = std::make_unique<MEffectTarget>(3); target->Set(77, 88, 99, 123); auto* original = target.get();
		auto info = Info(); info.effectSpriteType = type; CHECK(Generate(world, info, target)); CHECK(ground);
		CHECK(!target); CHECK_EQ(77, original->GetX()); CHECK(retained->GetEffectTarget() == original); CHECK_EQ(42, retained->GetActionInfo());
	}
}

TEST(RippleZoneEffectGenerator, RejectionRetainsCallerOwnershipOnBothQueues)
{
	World world;
	for (const auto type : {static_cast<TYPE_EFFECTSPRITETYPE>(17), static_cast<TYPE_EFFECTSPRITETYPE>(EFFECTSPRITETYPE_EARTHQUAKE_1)})
	{
		world.Reset(); accepted = false; auto target = std::make_unique<MEffectTarget>(3); auto info = Info(); info.effectSpriteType = type;
		CHECK(!Generate(world, info, target)); CHECK(target != nullptr); CHECK(!retained); CHECK_EQ(1, submissions);
	}
}

TEST(RippleZoneEffectGenerator, BoundsRejectUnsignedUnderflowAndUpperEdgesBeforeMetadata)
{
	World world;
	for (int edge = 0; edge < 4; ++edge)
	{
		world.Reset(); auto info = Info();
		if (edge == 0) { info.x0 = 0; info.direction = DIRECTION_LEFT; }
		if (edge == 1) { info.y0 = 0; info.direction = DIRECTION_UP; }
		if (edge == 2) { info.x0 = 19 * 48; info.direction = DIRECTION_RIGHT; }
		if (edge == 3) { info.y0 = 19 * 24; info.direction = DIRECTION_DOWN; }
		CHECK(!world.generator.Generate(info)); CHECK(calls == std::vector<int>{1}); CHECK_EQ(0, submissions);
	}
}

TEST(RippleZoneEffectGenerator, PixelDivisionTruncatesAndUnsignedStepCanWrapBackIntoTheZone)
{
	World world; auto info = Info(); info.x0 = -1; info.direction = DIRECTION_RIGHT;
	CHECK(world.generator.Generate(info)); CHECK_EQ(1, retained->GetX());
	world.Reset(); info.x0 = -48; CHECK(world.generator.Generate(info)); CHECK_EQ(0, retained->GetX());
}

TEST(RippleZoneEffectGenerator, MissingServicesSkipWithoutTakingTheTarget)
{
	World world; const MRippleZoneEffectHost empty{}, noSprite{.Bounds = host.Bounds}, noQueue{.Bounds = host.Bounds, .Sprite = host.Sprite};
	for (const auto* service : {static_cast<const MRippleZoneEffectHost*>(nullptr), &empty, &noSprite, &noQueue})
	{
		world.Reset(); MRippleZoneEffectGenerator::SetHost(service); auto target = std::make_unique<MEffectTarget>(3);
		CHECK(!Generate(world, Info(), target)); CHECK(target != nullptr); CHECK(!retained); CHECK_EQ(0, submissions);
	}
	world.Reset(); MRippleZoneEffectGenerator::SetHost(&host); metadataAvailable = false;
	CHECK(!world.generator.Generate(Info())); CHECK(calls == std::vector<int>({1,2})); CHECK_EQ(0, submissions);
}

TEST(RippleZoneEffectGenerator, CallbacksCanInstallTheFollowingServices)
{
	World world; const MRippleZoneEffectHost fromBounds{
		.Bounds = [](TYPE_SECTORPOSITION& w, TYPE_SECTORPOSITION& h) { MRippleZoneEffectGenerator::SetHost(&host); return host.Bounds(w, h); },
	};
	const MRippleZoneEffectHost fromSprite{
		.Bounds = host.Bounds,
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& result) { MRippleZoneEffectGenerator::SetHost(&host); return host.Sprite(type, result); },
	};
	for (const auto* service : {&fromBounds, &fromSprite})
	{
		world.Reset(); MRippleZoneEffectGenerator::SetHost(service); CHECK(world.generator.Generate(Info())); CHECK_EQ(1, submissions);
	}
}

TEST(RippleZoneEffectGenerator, RealEffectAnimatesInPlaceAndExpiresAtItsDeadline)
{
	World world; CHECK(world.generator.Generate(Info())); CHECK(retained->Update()); CHECK_EQ(1, retained->GetFrame());
	CHECK_EQ(192, retained->GetPixelX()); CHECK_EQ(120, retained->GetPixelY()); CHECK_EQ(17, retained->GetPixelZ());
	now = 129; CHECK(!retained->Update()); CHECK_EQ(2, retained->GetFrame());
}

namespace {
int destroyedRippleMarkers;
struct RippleMarker : MEffectTarget
{
	RippleMarker() : MEffectTarget(1) {}
	~RippleMarker() override { ++destroyedRippleMarkers; }
};
}

TEST(RippleZoneEffectGenerator, BothQueueExceptionsDestroyTheSubmittedEffectAndRetainTheOriginal)
{
	World world;
	const MRippleZoneEffectHost throwing{
		.Bounds = host.Bounds, .Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect, bool isGround) -> bool {
			ground = isGround; effect->SetLink(42, new RippleMarker); throw std::runtime_error("queue");
		},
	};
	MRippleZoneEffectGenerator::SetHost(&throwing);
	for (const auto type : {static_cast<TYPE_EFFECTSPRITETYPE>(17), static_cast<TYPE_EFFECTSPRITETYPE>(EFFECTSPRITETYPE_EARTHQUAKE_1)})
	{
		world.Reset(); destroyedRippleMarkers = 0; auto target = std::make_unique<MEffectTarget>(3); target->Set(77, 88, 99, 123);
		auto info = Info(); info.effectSpriteType = type; info.pEffectTarget = target.get(); bool threw = false;
		try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
		CHECK(threw); CHECK_EQ(1, destroyedRippleMarkers); CHECK(!retained); CHECK_EQ(77, target->GetX());
		CHECK_EQ(type == EFFECTSPRITETYPE_EARTHQUAKE_1, ground);
	}
}

TEST(RippleZoneEffectGenerator, AcceptedGroundRippleKeepsNonterminalTargetReachableForTheNextPhase)
{
	World world;
	for (const auto type : {EFFECTSPRITETYPE_EARTHQUAKE_1, EFFECTSPRITETYPE_EARTHQUAKE_2, EFFECTSPRITETYPE_EARTHQUAKE_3,
		EFFECTSPRITETYPE_POWER_OF_LAND_STONE_1, EFFECTSPRITETYPE_POWER_OF_LAND_STONE_2, EFFECTSPRITETYPE_POWER_OF_LAND_STONE_3})
	{
		world.Reset(); destroyedRippleMarkers = 0;
		std::unique_ptr<MEffectTarget> target = std::make_unique<RippleMarker>();
		target->m_MaxPhase = 3;
		// Generate and GenerateNext advance the target before invoking the generator.
		target->NextPhase();
		target->SetResult(new MActionResult);
		auto* original = target.get(); auto* result = original->GetResult();
		auto info = Info(); info.effectSpriteType = type;
		CHECK(Generate(world, info, target)); CHECK(ground);
		// A successful generator must give the ground updater a continuation.
		CHECK(retained->GetEffectTarget() == original);
		CHECK_EQ(42, retained->GetActionInfo());
		CHECK_EQ(1, retained->GetLinkSize());
		CHECK(!target);
		CHECK(original->GetResult() == result); CHECK(!original->IsResultTime());
		now = 104;
		CHECK(retained->Update()); CHECK_EQ(104, retained->GetEndLinkFrame());
		CHECK_EQ(1, retained->GetLinkSize());
		retained.reset();
		CHECK_EQ(1, destroyedRippleMarkers);
		// On unfixed code the test's caller still owns the target and cleans it up.
		target.reset(); now = 100;
	}
}
