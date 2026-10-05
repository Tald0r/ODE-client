#include "test_framework.h"
#include "MAttachZoneSelectableEffectGenerator.h"
#include "MEffect.h"
#include "EffectSpriteTypeDef.h"
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
std::unique_ptr<MEffect> retained;
MFixedZoneEffectSprite metadata;
std::vector<int> spriteRequests;
bool metadataAvailable, accepted;
int submissions, destroyed;
DWORD now;
const MEffectHost effectHost{.CurrentFrame = [] { return now; }};
const MFixedZoneEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite) { spriteRequests.push_back(type); sprite = metadata; return metadataAvailable; },
	.Queue = [](std::unique_ptr<MEffect> effect) {
		++submissions; CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		CHECK(effect->IsSelectable()); if (!accepted) return false;
		retained = std::move(effect); return true;
	},
};
struct Target : MEffectTarget
{
	using MEffectTarget::operator=;
	Target() : MEffectTarget(3) { NextPhase(); Set(777,888,999,123); SetResult(new MActionResult); }
	~Target() override { ++destroyed; }
};
struct World
{
	const MFixedZoneEffectHost* old = MAttachZoneSelectableEffectGenerator::SetHost(&host);
	const MEffectHost* oldEffect = MEffect::SetHost(&effectHost);
	MAttachZoneSelectableEffectGenerator generator;
	World() { Reset(); metadata = {BLT_EFFECT,12,3}; metadataAvailable = true; now = 100; }
	void Reset() { retained.reset(); spriteRequests.clear(); accepted = true; submissions = destroyed = 0; }
	~World() { retained.reset(); MAttachZoneSelectableEffectGenerator::SetHost(old); MEffect::SetHost(oldEffect); }
};
EFFECTGENERATOR_INFO Info()
{
	EFFECTGENERATOR_INFO info{}; info.effectSpriteType = 17; info.nActionInfo = 42;
	info.x0 = 241; info.y0 = 123; info.z0 = 17; info.x1 = 999; info.y1 = 999; info.z1 = 999;
	info.direction = DIRECTION_LEFTUP; info.count = 30; info.linkCount = 5; info.step = 11; info.power = 7; return info;
}
bool Generate(World& world, EFFECTGENERATOR_INFO info, std::unique_ptr<MEffectTarget>& target)
{
	info.pEffectTarget = target.get(); const bool result = world.generator.Generate(info);
	if (retained && retained->GetEffectTarget() == target.get()) target.release();
	return result;
}
}

TEST(SelectableGroundEffectGenerator, RealSelectableEffectKeepsSourcePixelsAndOriginalTarget)
{
	World world; CHECK_EQ(EFFECTGENERATORID_ATTACH_ZONE_SELECTABLE, world.generator.GetID());
	std::unique_ptr<MEffectTarget> target = std::make_unique<Target>(); auto* original = target.get();
	std::srand(13); const int next = std::rand(); std::srand(13);
	CHECK(Generate(world, Info(), target)); CHECK_EQ(next, std::rand()); CHECK(!target); CHECK_EQ(1, submissions);
	CHECK(retained->GetEffectTarget() == original); CHECK_EQ(777, original->GetX()); CHECK(original->IsExistResult());
	CHECK(retained->IsSelectable()); CHECK_EQ(MEffect::EFFECT_SECTOR, retained->GetEffectType());
	CHECK_EQ(241, retained->GetPixelX()); CHECK_EQ(123, retained->GetPixelY()); CHECK_EQ(17, retained->GetPixelZ());
	CHECK_EQ(BLT_EFFECT, retained->GetBltType()); CHECK_EQ(12, retained->GetFrameID()); CHECK_EQ(3, retained->GetMaxFrame());
	CHECK_EQ(11, retained->GetStepPixel()); CHECK_EQ(7, retained->GetPower()); CHECK_EQ(DIRECTION_LEFTUP, retained->GetDirection());
	CHECK_EQ(129, retained->GetEndFrame()); CHECK_EQ(104, retained->GetEndLinkFrame()); CHECK_EQ(42, retained->GetActionInfo());
	retained.reset(); CHECK_EQ(1, destroyed);
}

TEST(SelectableGroundEffectGenerator, BloodVariantsDrawSpriteThenBothOffsetsBeforeSubmission)
{
	World world;
	for (const auto type : {EFFECTSPRITETYPE_BLOOD_GROUND_2_1, EFFECTSPRITETYPE_BLOOD_GROUND_1_1})
		for (const int seed : {1,7,91}) for (const bool accept : {false,true})
	{
		world.Reset(); accepted = accept; auto info = Info(); info.effectSpriteType = type;
		std::srand(seed); const int selected = type + std::rand() % (type == EFFECTSPRITETYPE_BLOOD_GROUND_2_1 ? 4 : 5);
		const int x = 241 + std::rand() % 24 - 12, y = 123 + std::rand() % 24 - 12, next = std::rand();
		std::srand(seed); CHECK_EQ(accept, world.generator.Generate(info)); CHECK_EQ(next, std::rand());
		CHECK(spriteRequests == std::vector<int>{selected}); CHECK_EQ(1, submissions);
		if (retained) { CHECK_EQ(x, retained->GetPixelX()); CHECK_EQ(y, retained->GetPixelY()); CHECK_EQ(17, retained->GetPixelZ()); }
	}
}

TEST(SelectableGroundEffectGenerator, MetadataFailurePreservesVariantRandomDrawsAndCallerOwnership)
{
	World world; metadataAvailable = false; auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_BLOOD_GROUND_1_1;
	std::srand(17); std::rand(); std::rand(); std::rand(); const int next = std::rand(); std::srand(17);
	std::unique_ptr<MEffectTarget> target = std::make_unique<Target>(); CHECK(!Generate(world, info, target)); CHECK_EQ(next, std::rand());
	CHECK(target != nullptr); CHECK_EQ(0, submissions); CHECK_EQ(0, destroyed);
}

TEST(SelectableGroundEffectGenerator, MissingServicesAndRejectionDoNotTakeTheOriginal)
{
	World world; const MFixedZoneEffectHost empty{}, noQueue{.Sprite = host.Sprite};
	for (const auto* service : {static_cast<const MFixedZoneEffectHost*>(nullptr), &empty, &noQueue, &host})
	{
		world.Reset(); MAttachZoneSelectableEffectGenerator::SetHost(service); accepted = false;
		std::unique_ptr<MEffectTarget> target = std::make_unique<Target>(); CHECK(!Generate(world, Info(), target)); CHECK(target != nullptr); CHECK(!retained); CHECK_EQ(0, destroyed);
	}
}

TEST(SelectableGroundEffectGenerator, QueueExceptionDestroysTheEffectWithoutTakingTheOriginal)
{
	World world; const MFixedZoneEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool { effect->SetLink(43, new Target); throw std::runtime_error("queue"); },
	};
	MAttachZoneSelectableEffectGenerator::SetHost(&throwing); std::unique_ptr<MEffectTarget> target = std::make_unique<Target>();
	auto info = Info(); info.pEffectTarget = target.get(); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, destroyed); CHECK(target->IsExistResult()); CHECK_EQ(777, target->GetX());
	target.reset(); CHECK_EQ(2, destroyed);
}

TEST(SelectableGroundEffectGenerator, SpriteCallbackCanReplaceTheFollowingQueue)
{
	World world; const MFixedZoneEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite) { MAttachZoneSelectableEffectGenerator::SetHost(&host); return host.Sprite(type,sprite); },
	};
	MAttachZoneSelectableEffectGenerator::SetHost(&changing); CHECK(world.generator.Generate(Info())); CHECK_EQ(1, submissions);
}

TEST(SelectableGroundEffectGenerator, RealEffectsAnimateWithoutMovingAndExpireAtTheDeadline)
{
	World world;
	for (const int frames : {0,3,256,258})
	{
		world.Reset(); metadata.maxFrames = frames; CHECK(world.generator.Generate(Info())); CHECK_EQ(static_cast<BYTE>(frames), retained->GetMaxFrame());
		CHECK(retained->Update()); CHECK_EQ(241, retained->GetPixelX()); CHECK_EQ(123, retained->GetPixelY());
		now = 129; CHECK(!retained->Update()); now = 100;
	}
}

TEST(SelectableGroundEffectGenerator, BloodJitterSaturatesAtIntegerPixelLimits)
{
	World world; auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_BLOOD_GROUND_2_1;
	info.x0 = (std::numeric_limits<int>::max)(); info.y0 = (std::numeric_limits<int>::min)();
	// Find an overflowing positive X and negative Y draw on this runtime's RNG.
	unsigned seed = 0;
	for (; seed < 1000; ++seed)
	{
		std::srand(seed); std::rand(); const int x = std::rand() % 24 - 12, y = std::rand() % 24 - 12;
		if (x > 0 && y < 0) break;
	}
	CHECK(seed < 1000); std::srand(seed); CHECK(world.generator.Generate(info));
	CHECK_EQ((std::numeric_limits<int>::max)(), retained->GetPixelX()); CHECK_EQ((std::numeric_limits<int>::min)(), retained->GetPixelY());
}
