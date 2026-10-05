#include "test_framework.h"
#include "MAttackZoneRectEffectGenerator.h"
#include "MEffect.h"
#include <memory>

TEST(RectangleGroundEffectGenerator, RejectedSubmissionsDoNotClaimTargetTransfer)
{
	const MFixedZoneEffectHost host{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MFixedZoneEffectSprite& sprite) { sprite = {BLT_EFFECT, 12, 3}; return true; },
		.Queue = [](std::unique_ptr<MEffect>) { return false; },
	};
	const auto* previous = MAttackZoneRectEffectGenerator::SetHost(&host);
	for (int phase : {1, 2})
	{
		auto target = std::make_unique<MEffectTarget>(4);
		for (int i = 0; i < phase; ++i) target->NextPhase();
		EFFECTGENERATOR_INFO info{}; info.effectSpriteType = 17; info.pEffectTarget = target.get();
		MAttackZoneRectEffectGenerator generator; CHECK(!generator.Generate(info));
	}
	MAttackZoneRectEffectGenerator::SetHost(previous);
}

#include "MLinearEffect.h"
#include <stdexcept>
#include <vector>

namespace {
std::vector<std::unique_ptr<MEffect>> rectEffects;
std::vector<int> rectSlots;
int rectMask, rectSubmissions, rectDestroyed;
bool rectMetadata;
DWORD rectNow;
MFixedZoneEffectSprite rectSprite;
const MEffectHost rectEffectHost{.CurrentFrame = [] { return rectNow; }};
const MFixedZoneEffectHost rectHost{
	.Sprite = [](TYPE_EFFECTSPRITETYPE, MFixedZoneEffectSprite& sprite) { sprite = rectSprite; return rectMetadata; },
	.Queue = [](std::unique_ptr<MEffect> effect) {
		const int slot = rectSubmissions++; CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		CHECK_EQ(MEffect::EFFECT_LINEAR, effect->GetEffectType());
		if (!(rectMask & (1 << slot))) return false;
		rectSlots.push_back(slot); rectEffects.push_back(std::move(effect)); return true;
	},
};
struct RectWorld
{
	const MFixedZoneEffectHost* old = MAttackZoneRectEffectGenerator::SetHost(&rectHost);
	const MEffectHost* oldEffect = MEffect::SetHost(&rectEffectHost);
	MAttackZoneRectEffectGenerator generator;
	RectWorld() { Reset(); rectMetadata = true; rectSprite = {BLT_EFFECT,12,3}; rectNow = 100; }
	void Reset() { rectEffects.clear(); rectSlots.clear(); rectMask = 255; rectSubmissions = rectDestroyed = 0; }
	~RectWorld() { rectEffects.clear(); MAttackZoneRectEffectGenerator::SetHost(old); MEffect::SetHost(oldEffect); }
};
struct RectTarget : MEffectTarget
{
	using MEffectTarget::operator=;
	explicit RectTarget(int phase) : MEffectTarget(255) { for (int i = 0; i < phase; ++i) NextPhase(); Set(777,888,999,123); SetResult(new MActionResult); }
	~RectTarget() override { ++rectDestroyed; }
};
EFFECTGENERATOR_INFO RectInfo()
{
	EFFECTGENERATOR_INFO info{}; info.effectSpriteType = 17; info.nActionInfo = 42;
	info.x0 = 96; info.y0 = 48; info.z0 = 17; info.x1 = 240; info.y1 = 120; info.z1 = 99;
	info.direction = DIRECTION_LEFTUP; info.count = 30; info.linkCount = 5; info.step = 11; info.power = 7; return info;
}
bool RectGenerate(RectWorld& world, EFFECTGENERATOR_INFO info, std::unique_ptr<MEffectTarget>& target)
{
	info.pEffectTarget = target.get(); const bool accepted = world.generator.Generate(info);
	for (const auto& effect : rectEffects) if (effect->GetEffectTarget() == target.get()) { target.release(); break; }
	return accepted;
}
}

TEST(RectangleGroundEffectGenerator, EveryBurstMaskTransfersOriginalToTheFirstRetainedProjectile)
{
	RectWorld world;
	for (int mask = 0; mask < 256; ++mask)
	{
		world.Reset(); rectMask = mask; std::unique_ptr<MEffectTarget> target = std::make_unique<RectTarget>(2); auto* original = target.get();
		CHECK_EQ(mask != 0, RectGenerate(world, RectInfo(), target)); CHECK_EQ(8, rectSubmissions); CHECK_EQ(mask == 0, target != nullptr);
		CHECK_EQ(777, original->GetX()); CHECK(original->IsExistResult());
		for (size_t i = 0; i < rectEffects.size(); ++i)
		{
			const auto& effect = rectEffects[i]; CHECK_EQ(i == 0, effect->GetEffectTarget() == original);
			if (i) CHECK(effect->GetEffectTarget() == nullptr);
			CHECK_EQ(rectSlots[i], effect->GetDirection()); CHECK_EQ(240, effect->GetPixelX()); CHECK_EQ(120, effect->GetPixelY()); CHECK_EQ(0, effect->GetPixelZ());
			CHECK_EQ(12, effect->GetFrameID()); CHECK_EQ(3, effect->GetMaxFrame()); CHECK_EQ(11, effect->GetStepPixel()); CHECK_EQ(7, effect->GetPower());
			CHECK_EQ(129, effect->GetEndFrame()); CHECK_EQ(104, effect->GetEndLinkFrame()); CHECK_EQ(42, effect->GetActionInfo());
		}
		rectEffects.clear(); CHECK_EQ(mask != 0 ? 1 : 0, rectDestroyed); target.reset(); CHECK_EQ(1, rectDestroyed);
	}
}

TEST(RectangleGroundEffectGenerator, RealBurstProjectilesArriveAtTheEightLiteralTileOrigins)
{
	RectWorld world; std::unique_ptr<MEffectTarget> target = std::make_unique<RectTarget>(2); CHECK(RectGenerate(world, RectInfo(), target));
	const int endpoints[8][2] = {{192,120},{192,144},{240,144},{288,144},{288,120},{288,96},{240,96},{192,96}};
	for (size_t i = 0; i < rectEffects.size(); ++i)
	{
		int updates = 0; while (rectEffects[i]->Update() && ++updates < 100) {}
		CHECK(updates < 100); CHECK_EQ(endpoints[i][0], rectEffects[i]->GetPixelX()); CHECK_EQ(endpoints[i][1], rectEffects[i]->GetPixelY()); CHECK_EQ(0, rectEffects[i]->GetPixelZ());
	}
}

TEST(RectangleGroundEffectGenerator, OnlyPhaseTwoBurstsAndOtherPhasesUseSourceHeightAsTargetHeight)
{
	RectWorld world;
	for (int phase = 0; phase < 256; ++phase)
	{
		world.Reset(); std::unique_ptr<MEffectTarget> target = std::make_unique<RectTarget>(phase); CHECK(RectGenerate(world, RectInfo(), target));
		CHECK_EQ(phase == 2 ? 8 : 1, rectSubmissions);
		if (phase == 2) continue;
		auto& effect = *rectEffects.front(); CHECK_EQ(96, effect.GetPixelX()); CHECK_EQ(48, effect.GetPixelY()); CHECK_EQ(0, effect.GetPixelZ());
		CHECK_EQ(DIRECTION_LEFTUP, effect.GetDirection());
		int updates = 0; while (effect.Update() && ++updates < 100) {}
		CHECK(updates < 100); CHECK_EQ(240, effect.GetPixelX()); CHECK_EQ(120, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ());
	}
}

TEST(RectangleGroundEffectGenerator, TargetlessCallsUseASingleProjectileAndPreserveEveryDirectionByte)
{
	RectWorld world;
	for (int direction = 0; direction < 256; ++direction)
	{
		world.Reset(); auto info = RectInfo(); info.direction = static_cast<BYTE>(direction); CHECK(world.generator.Generate(info));
		CHECK_EQ(1, rectSubmissions); CHECK(rectEffects.front()->GetEffectTarget() == nullptr); CHECK_EQ(direction, rectEffects.front()->GetDirection()); CHECK_EQ(42, rectEffects.front()->GetActionInfo());
	}
}

TEST(RectangleGroundEffectGenerator, MissingMetadataOrQueueDoesNotTakeTheTarget)
{
	RectWorld world; const MFixedZoneEffectHost empty{}, noQueue{.Sprite = rectHost.Sprite};
	for (const auto* service : {static_cast<const MFixedZoneEffectHost*>(nullptr), &empty, &noQueue})
	{
		world.Reset(); MAttackZoneRectEffectGenerator::SetHost(service); std::unique_ptr<MEffectTarget> target = std::make_unique<RectTarget>(2);
		CHECK(!RectGenerate(world, RectInfo(), target)); CHECK(target != nullptr); CHECK_EQ(0, rectSubmissions);
	}
	world.Reset(); MAttackZoneRectEffectGenerator::SetHost(&rectHost); rectMetadata = false;
	CHECK(!world.generator.Generate(RectInfo())); CHECK_EQ(0, rectSubmissions);
}

TEST(RectangleGroundEffectGenerator, QueueExceptionsBeforeAndAfterAcceptanceKeepExactlyOneOwner)
{
	RectWorld world; static int throwAt;
	const MFixedZoneEffectHost throwing{
		.Sprite = rectHost.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { if (rectSubmissions == throwAt) throw std::runtime_error("queue"); return rectHost.Queue(std::move(effect)); },
	};
	for (int slot : {0,1,7})
	{
		world.Reset(); throwAt = slot; MAttackZoneRectEffectGenerator::SetHost(&throwing); auto* target = new RectTarget(2); bool threw = false;
		try { MEffectTargetOwner owner(target); auto info = RectInfo(); info.pEffectTarget = target; world.generator.Generate(info); }
		catch (const std::runtime_error&) { threw = true; }
		CHECK(threw); CHECK_EQ(slot, rectSubmissions); CHECK_EQ(slot == 0 ? 1 : 0, rectDestroyed);
		if (slot) CHECK(rectEffects.front()->GetEffectTarget() == target);
		rectEffects.clear(); CHECK_EQ(1, rectDestroyed);
	}
}

TEST(RectangleGroundEffectGenerator, HostReplacementAfterSpriteAndRemovalAfterAcceptanceAreObserved)
{
	RectWorld world; const MFixedZoneEffectHost fromSprite{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite) { MAttackZoneRectEffectGenerator::SetHost(&rectHost); return rectHost.Sprite(type,sprite); },
	};
	MAttackZoneRectEffectGenerator::SetHost(&fromSprite); CHECK(world.generator.Generate(RectInfo())); CHECK_EQ(1, rectSubmissions);
	world.Reset(); const MFixedZoneEffectHost removing{
		.Sprite = rectHost.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { const bool accepted = rectHost.Queue(std::move(effect)); MAttackZoneRectEffectGenerator::SetHost(nullptr); return accepted; },
	};
	MAttackZoneRectEffectGenerator::SetHost(&removing); std::unique_ptr<MEffectTarget> target = std::make_unique<RectTarget>(2);
	CHECK(RectGenerate(world, RectInfo(), target)); CHECK_EQ(1, rectSubmissions); CHECK(!target);
}
