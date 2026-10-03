#include "test_framework.h"
#include "MAttackZoneEffectGenerator.h"
#include "MLinearEffect.h"
#include "EffectSpriteTypeDef.h"
#include "SkillDef.h"

#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MZoneAttackEffectSprite sprite;
bool spriteAvailable, acceptQueue;
int requestedSprite;
MEffectTarget* observedTarget;
std::vector<int> calls, removedTargets, targetAtLight, targetAtQueue;
std::vector<std::unique_ptr<MEffect>> effects;
struct LightRequest
{
	BYTE blt; TYPE_FRAMEID id; BYTE direction, frame;
	bool operator==(const LightRequest&) const = default;
};
std::vector<LightRequest> lights;
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(3); return frameNow; },
	.Light = [](BYTE blt, TYPE_FRAMEID id, BYTE direction, BYTE frame) {
		calls.push_back(2); lights.push_back({blt, id, direction, frame});
		if (observedTarget) targetAtLight = {observedTarget->GetX(), observedTarget->GetY(), observedTarget->GetZ()};
		return 7;
	},
};
const MEffectTargetHost targetHost{
	.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); },
};
const MZoneAttackEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MZoneAttackEffectSprite& result) {
		calls.push_back(1); requestedSprite = type; result = sprite;
		return spriteAvailable;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		calls.push_back(4);
		CHECK(effect->GetEffectTarget() == nullptr);
		CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		if (observedTarget) targetAtQueue = {observedTarget->GetX(), observedTarget->GetY(), observedTarget->GetZ()};
		if (!acceptQueue) return false;
		effects.push_back(std::move(effect)); return true;
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MZoneAttackEffectHost* previousZoneAttack = MAttackZoneEffectGenerator::SetHost(&host);
	World()
	{
		frameNow = 100; sprite = {BLT_EFFECT, 12, 3}; spriteAvailable = acceptQueue = true;
		requestedSprite = -1; observedTarget = nullptr;
		effects.clear(); calls.clear(); lights.clear(); removedTargets.clear();
		targetAtLight.clear(); targetAtQueue.clear();
	}
	~World()
	{
		observedTarget = nullptr; effects.clear();
		MAttackZoneEffectGenerator::SetHost(previousZoneAttack);
		MEffectTarget::SetHost(previousTarget); MEffect::SetHost(previousEffect);
	}
};

EFFECTGENERATOR_INFO Info()
{
	EFFECTGENERATOR_INFO info{};
	info.nActionInfo = 42; info.effectSpriteType = 17;
	info.x0 = 96; info.y0 = 48; info.z0 = 12;
	info.x1 = 396; info.y1 = 448; info.z1 = 12;
	info.direction = DIRECTION_LEFTUP; info.step = 50;
	info.count = 30; info.linkCount = 5; info.power = 77; info.creatureID = 123;
	return info;
}

std::unique_ptr<MEffectTarget> Target()
{
	auto target = std::make_unique<MEffectTarget>(3);
	target->NextPhase(); target->m_EffectID = 73; target->Set(777, 888, 999, 456);
	return target;
}

} // namespace

TEST(ZoneAttackEffectGenerator, ConfiguresARealProjectileWithTheSuppliedFacing)
{
	World world;
	MAttackZoneEffectGenerator generator;
	CHECK_EQ(EFFECTGENERATORID_ATTACK_ZONE, generator.GetID());
	CHECK(generator.Generate(Info())); CHECK_EQ(1, effects.size());
	auto& effect = *effects.front();
	CHECK_EQ(MEffect::EFFECT_LINEAR, effect.GetEffectType());
	CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID());
	CHECK_EQ(3, effect.GetMaxFrame()); CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight());
	CHECK_EQ(96, effect.GetPixelX()); CHECK_EQ(48, effect.GetPixelY()); CHECK_EQ(12, effect.GetPixelZ());
	CHECK_EQ(2, effect.GetX()); CHECK_EQ(2, effect.GetY()); CHECK(!effect.IsMulti());
	CHECK_EQ(DIRECTION_LEFTUP, effect.GetDirection()); CHECK_EQ(50, effect.GetStepPixel());
	CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame());
	CHECK_EQ(77, effect.GetPower()); CHECK_EQ(42, effect.GetActionInfo()); CHECK_EQ(17, requestedSprite);
	CHECK(calls == std::vector<int>({1, 2, 3, 4}));
	CHECK(lights == std::vector<LightRequest>({{BLT_EFFECT, 12, DIRECTION_LEFT, 0}}));
}

TEST(ZoneAttackEffectGenerator, RealMotionFollowsTheTargetWhileFacingStaysSupplied)
{
	World world;
	MAttackZoneEffectGenerator generator;
	CHECK(generator.Generate(Info()));
	auto& effect = *effects.front();
	CHECK(effect.Update()); CHECK_EQ(126, effect.GetPixelX()); CHECK_EQ(88, effect.GetPixelY());
	CHECK_EQ(DIRECTION_LEFTUP, effect.GetDirection());
	for (int i = 1; i < 8; ++i) CHECK(effect.Update());
	CHECK_EQ(2, effect.GetFrame());
	CHECK(!effect.Update()); CHECK_EQ(396, effect.GetPixelX()); CHECK_EQ(448, effect.GetPixelY());
	CHECK_EQ(2, effect.GetFrame()); CHECK_EQ(0, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame());
}

TEST(ZoneAttackEffectGenerator, OrdinaryAcceptanceTransfersAnUnmodifiedTarget)
{
	World world;
	MAttackZoneEffectGenerator generator;
	auto target = Target(); auto info = Info(); info.pEffectTarget = observedTarget = target.get();
	CHECK(generator.Generate(info)); target.release();
	CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget); CHECK_EQ(1, effects.front()->GetLinkSize());
	CHECK(targetAtQueue == std::vector<int>({777, 888, 999}));
	CHECK(targetAtLight == targetAtQueue);
	effects.clear(); CHECK(removedTargets == std::vector<int>({73}));
}

TEST(ZoneAttackEffectGenerator, HaloExtendsThreeTilesInAllEightDirections)
{
	World world;
	MAttackZoneEffectGenerator generator;
	struct Offset { int x, y; };
	for (const Offset offset : {Offset{-1, 1}, {1, -1}, {-1, -1}, {1, 1}, {-1, 0}, {0, 1}, {0, -1}, {1, 0}})
	{
		auto info = Info(); info.nActionInfo = SKILL_HALO; info.step = 255;
		info.x0 = 480; info.y0 = 240; info.x1 = 480 + 96 * offset.x; info.y1 = 240 + 48 * offset.y;
		CHECK(generator.Generate(info));
		auto& effect = *effects.back();
		CHECK(effect.IsMulti()); CHECK_EQ(info.direction, effect.GetDirection());
		CHECK(effect.Update());
		CHECK_EQ(480 + 240 * offset.x, effect.GetPixelX()); CHECK_EQ(240 + 120 * offset.y, effect.GetPixelY());
		CHECK_EQ(108, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame());
		CHECK_EQ(1, effect.GetFrame());
	}
}

TEST(ZoneAttackEffectGenerator, HaloUsesTileOriginsAndCoincidentTilesFaceDown)
{
	World world;
	MAttackZoneEffectGenerator generator;
	struct Case { int sx, sy, tx, ty, x, y; };
	for (const Case c : {Case{480, 240, 480, 240, 480, 312}, {503, 251, 527, 263, 480, 312},
		{-1, -1, -47, -23, 0, 72}})
	{
		auto info = Info(); info.nActionInfo = SKILL_HALO; info.step = 255;
		info.x0 = c.sx; info.y0 = c.sy; info.x1 = c.tx; info.y1 = c.ty;
		CHECK(generator.Generate(info)); CHECK(effects.back()->Update());
		CHECK_EQ(c.x, effects.back()->GetPixelX()); CHECK_EQ(c.y, effects.back()->GetPixelY());
	}
}

TEST(ZoneAttackEffectGenerator, WindDividerRangeAndUnnormalizedLinkTargetRemainDistinct)
{
	World world;
	MAttackZoneEffectGenerator generator;
	for (const auto type : {EFFECTSPRITETYPE_WIND_DIVIDER_1, EFFECTSPRITETYPE_WIND_DIVIDER_2, EFFECTSPRITETYPE_WIND_DIVIDER_3})
	{
		auto target = Target(); auto info = Info(); info.effectSpriteType = type;
		info.pEffectTarget = observedTarget = target.get();
		CHECK(generator.Generate(info)); target.release();
		CHECK(targetAtLight == std::vector<int>({450096, 600048, 12})); CHECK(targetAtQueue == targetAtLight);
		CHECK_EQ(456, info.pEffectTarget->GetID());
		auto& effect = *effects.back();
		for (int i = 0; i < 28; ++i) CHECK(effect.Update());
		CHECK(!effect.Update()); CHECK_EQ(996, effect.GetPixelX()); CHECK_EQ(1248, effect.GetPixelY());
		CHECK_EQ(DIRECTION_LEFTUP, effect.GetDirection());
		effects.clear(); observedTarget = nullptr;
	}
}

TEST(ZoneAttackEffectGenerator, WindDividerRetargetsBeforeARejectedSubmission)
{
	World world;
	MAttackZoneEffectGenerator generator;
	acceptQueue = false;
	auto target = Target(); auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_WIND_DIVIDER_1;
	info.pEffectTarget = observedTarget = target.get();
	CHECK(!generator.Generate(info)); CHECK(effects.empty()); CHECK(removedTargets.empty());
	CHECK_EQ(450096, target->GetX()); CHECK_EQ(600048, target->GetY()); CHECK_EQ(12, target->GetZ());
	CHECK_EQ(456, target->GetID()); CHECK(targetAtLight == targetAtQueue);
}

TEST(ZoneAttackEffectGenerator, WindFallbackStepsOneSectorInEverySuppliedDirection)
{
	World world;
	MAttackZoneEffectGenerator generator;
	struct Case { BYTE direction; int linkedX, linkedY, x, y; };
	for (const Case c : {Case{DIRECTION_LEFTDOWN, -2304, 1248, 51, 70},
		{DIRECTION_RIGHTUP, 2496, -1152, 141, 26}, {DIRECTION_LEFTUP, -2304, -1152, 51, 26},
		{DIRECTION_RIGHTDOWN, 2496, 1248, 141, 70}, {DIRECTION_LEFT, -2304, 48, 46, 48},
		{DIRECTION_DOWN, 96, 1248, 96, 98}, {DIRECTION_UP, 96, -1152, 96, -2},
		{DIRECTION_RIGHT, 2496, 48, 146, 48}})
	{
		auto target = Target(); auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_WIND_DIVIDER_1;
		info.x1 = info.x0; info.direction = c.direction; info.step = 10; info.count = 5;
		info.pEffectTarget = target.get();
		CHECK(generator.Generate(info)); target.release();
		CHECK_EQ(c.linkedX, info.pEffectTarget->GetX()); CHECK_EQ(c.linkedY, info.pEffectTarget->GetY());
		bool arrived = false;
		for (int i = 0; i < 8; ++i) if (!effects.back()->Update()) { arrived = true; break; }
		CHECK(arrived); CHECK_EQ(c.x, effects.back()->GetPixelX()); CHECK_EQ(c.y, effects.back()->GetPixelY());
		CHECK_EQ(c.direction, effects.back()->GetDirection()); effects.clear();
	}
}

TEST(ZoneAttackEffectGenerator, EitherAlignedAxisTriggersTheWindFallback)
{
	World world;
	MAttackZoneEffectGenerator generator;
	struct Point { int x, y; };
	for (const Point point : {Point{96, 500}, {500, 48}, {96, 48}})
	{
		auto target = Target(); auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_WIND_DIVIDER_1;
		info.x1 = point.x; info.y1 = point.y; info.direction = DIRECTION_RIGHTUP;
		info.step = 10; info.count = 5; info.pEffectTarget = target.get();
		CHECK(generator.Generate(info)); target.release();
		CHECK_EQ(2496, info.pEffectTarget->GetX()); CHECK_EQ(-1152, info.pEffectTarget->GetY());
		effects.clear();
	}
}

TEST(ZoneAttackEffectGenerator, UnknownWindDirectionAtAnAlignedSourceKeepsTheTargetUntouched)
{
	World world;
	MAttackZoneEffectGenerator generator;
	auto target = Target(); auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_WIND_DIVIDER_1;
	info.x1 = info.x0; info.direction = 255; info.pEffectTarget = target.get();
	CHECK(generator.Generate(info)); target.release();
	CHECK_EQ(777, info.pEffectTarget->GetX()); CHECK_EQ(888, info.pEffectTarget->GetY());
	CHECK_EQ(999, info.pEffectTarget->GetZ()); CHECK_EQ(255, effects.front()->GetDirection());
	CHECK(!effects.front()->Update()); CHECK_EQ(96, effects.front()->GetPixelX()); CHECK_EQ(48, effects.front()->GetPixelY());
}

TEST(ZoneAttackEffectGenerator, HaloExtensionPrecedesWindRangeAndLinkTargetAdjustment)
{
	World world;
	MAttackZoneEffectGenerator generator;
	auto target = Target(); auto info = Info(); info.nActionInfo = SKILL_HALO;
	info.effectSpriteType = EFFECTSPRITETYPE_WIND_DIVIDER_1; info.pEffectTarget = target.get();
	CHECK(generator.Generate(info)); target.release();
	// Tile displacement (6, 16) selects right-down, extending both axes.
	CHECK(effects.front()->IsMulti()); CHECK_EQ(648096, info.pEffectTarget->GetX());
	CHECK_EQ(684048, info.pEffectTarget->GetY()); CHECK_EQ(456, info.pEffectTarget->GetID());
	CHECK_EQ(info.direction, effects.front()->GetDirection());
}

TEST(ZoneAttackEffectGenerator, ZeroSpeedKeepsWindEffectsStationaryAndRetargetsToTheSource)
{
	World world;
	MAttackZoneEffectGenerator generator;
	auto target = Target(); auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_WIND_DIVIDER_1;
	info.step = 0; info.pEffectTarget = target.get();
	CHECK(generator.Generate(info)); target.release();
	CHECK_EQ(96, info.pEffectTarget->GetX()); CHECK_EQ(48, info.pEffectTarget->GetY());
	CHECK(effects.front()->Update()); CHECK_EQ(96, effects.front()->GetPixelX());
	CHECK_EQ(48, effects.front()->GetPixelY()); CHECK_EQ(1, effects.front()->GetFrame());
}

TEST(ZoneAttackEffectGenerator, ZeroCountRetargetsWindButExpiresBeforeMovement)
{
	World world;
	MAttackZoneEffectGenerator generator;
	auto target = Target(); auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_WIND_DIVIDER_1;
	info.count = 0; info.pEffectTarget = target.get();
	CHECK(generator.Generate(info)); target.release();
	CHECK_EQ(96, info.pEffectTarget->GetX()); CHECK_EQ(48, info.pEffectTarget->GetY());
	CHECK_EQ(99, effects.front()->GetEndFrame()); CHECK(!effects.front()->Update());
	CHECK_EQ(0, effects.front()->GetFrame());
}

TEST(ZoneAttackEffectGenerator, MetadataRefreshesAndFrameCountsRetainByteNarrowing)
{
	World world;
	MAttackZoneEffectGenerator generator;
	CHECK(generator.Generate(Info()));
	sprite = {BLT_NORMAL, 23, 258};
	CHECK(generator.Generate(Info()));
	CHECK_EQ(12, effects[0]->GetFrameID()); CHECK_EQ(23, effects[1]->GetFrameID());
	CHECK_EQ(BLT_NORMAL, effects[1]->GetBltType()); CHECK_EQ(2, effects[1]->GetMaxFrame());
	const auto previousLights = lights.size();
	CHECK(effects[1]->Update()); CHECK_EQ(previousLights, lights.size());
}

TEST(ZoneAttackEffectGenerator, RejectedSubmissionKeepsTheOrdinaryCallerTarget)
{
	World world;
	MAttackZoneEffectGenerator generator;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	acceptQueue = false;
	CHECK(!generator.Generate(info)); CHECK(effects.empty()); CHECK(removedTargets.empty());
	CHECK_EQ(777, target->GetX()); CHECK_EQ(888, target->GetY()); CHECK_EQ(999, target->GetZ());
	CHECK(calls == std::vector<int>({1, 2, 3, 4}));
	target.reset(); CHECK(removedTargets == std::vector<int>({73}));
}

TEST(ZoneAttackEffectGenerator, RejectingQueueDestroysItsEffectAndAttachedMarker)
{
	World world;
	MAttackZoneEffectGenerator generator;
	const MZoneAttackEffectHost rejecting{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			auto marker = Target(); marker->m_EffectID = 74;
			effect->SetLink(43, marker.release()); return false;
		},
	};
	MAttackZoneEffectGenerator::SetHost(&rejecting);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!generator.Generate(info)); CHECK(removedTargets == std::vector<int>({74}));
	target.reset(); CHECK(removedTargets == std::vector<int>({74, 73}));
}

TEST(ZoneAttackEffectGenerator, QueueExceptionCleansUpTheEffectButRetainsTheMutatedWindTarget)
{
	World world;
	MAttackZoneEffectGenerator generator;
	const MZoneAttackEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool {
			auto marker = Target(); marker->m_EffectID = 74;
			effect->SetLink(43, marker.release()); throw std::runtime_error("Queue failure");
		},
	};
	MAttackZoneEffectGenerator::SetHost(&throwing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	info.effectSpriteType = EFFECTSPRITETYPE_WIND_DIVIDER_1;
	bool threw = false;
	try { generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK(removedTargets == std::vector<int>({74}));
	CHECK_EQ(450096, target->GetX()); CHECK_EQ(600048, target->GetY()); CHECK_EQ(456, target->GetID());
	target.reset(); CHECK(removedTargets == std::vector<int>({74, 73}));
}

TEST(ZoneAttackEffectGenerator, MissingMetadataRejectsBeforeLightLookupOrTargetMutation)
{
	World world;
	MAttackZoneEffectGenerator generator;
	spriteAvailable = false;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	info.effectSpriteType = EFFECTSPRITETYPE_WIND_DIVIDER_1;
	CHECK(!generator.Generate(info)); CHECK(calls == std::vector<int>({1}));
	CHECK(effects.empty()); CHECK(lights.empty()); CHECK(removedTargets.empty());
	CHECK_EQ(777, target->GetX()); CHECK_EQ(888, target->GetY()); CHECK_EQ(999, target->GetZ());
}

TEST(ZoneAttackEffectGenerator, MissingMetadataHostsLeaveTheCallerTargetUntouched)
{
	World world;
	MAttackZoneEffectGenerator generator;
	const MZoneAttackEffectHost empty{}, queueOnly{.Queue = host.Queue};
	for (const auto* service : {static_cast<const MZoneAttackEffectHost*>(nullptr), &empty, &queueOnly})
	{
		MAttackZoneEffectGenerator::SetHost(service);
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		info.effectSpriteType = EFFECTSPRITETYPE_WIND_DIVIDER_1;
		const auto removed = removedTargets.size();
		CHECK(!generator.Generate(info)); CHECK_EQ(removed, removedTargets.size());
		CHECK_EQ(777, target->GetX()); CHECK_EQ(888, target->GetY()); CHECK_EQ(999, target->GetZ());
	}
	CHECK(calls.empty()); CHECK(effects.empty());
}

TEST(ZoneAttackEffectGenerator, MissingQueueKeepsOwnershipAfterWindTargetMutation)
{
	World world;
	MAttackZoneEffectGenerator generator;
	const MZoneAttackEffectHost spriteOnly{.Sprite = host.Sprite};
	MAttackZoneEffectGenerator::SetHost(&spriteOnly);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	info.effectSpriteType = EFFECTSPRITETYPE_WIND_DIVIDER_1;
	CHECK(!generator.Generate(info)); CHECK(effects.empty()); CHECK(removedTargets.empty());
	CHECK_EQ(450096, target->GetX()); CHECK_EQ(600048, target->GetY());
	CHECK(calls == std::vector<int>({1, 2, 3}));
}

TEST(ZoneAttackEffectGenerator, SpriteCallbackCanReplaceTheSubmissionService)
{
	World world;
	MAttackZoneEffectGenerator generator;
	const MZoneAttackEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MZoneAttackEffectSprite& result) {
			result = {BLT_NORMAL, 20, 2};
			MAttackZoneEffectGenerator::SetHost(&host); return true;
		},
	};
	CHECK(MAttackZoneEffectGenerator::SetHost(&changing) == &host);
	CHECK(generator.Generate(Info()));
	CHECK_EQ(BLT_NORMAL, effects.front()->GetBltType()); CHECK_EQ(20, effects.front()->GetFrameID());
	CHECK_EQ(2, effects.front()->GetMaxFrame()); CHECK(calls == std::vector<int>({2, 3, 4}));
	CHECK(MAttackZoneEffectGenerator::SetHost(&changing) == &host);
}

TEST(ZoneAttackEffectGenerator, ClockCallbackCanRemoveTheQueueBeforeSubmission)
{
	World world;
	MAttackZoneEffectGenerator generator;
	const MEffectHost changing{
		.CurrentFrame = []() { MAttackZoneEffectGenerator::SetHost(nullptr); return frameNow; },
		.Light = effectHost.Light,
	};
	MEffect::SetHost(&changing);
	CHECK(!generator.Generate(Info())); CHECK(effects.empty());
	CHECK(calls == std::vector<int>({1, 2}));
}

TEST(ZoneAttackEffectGenerator, MissingBaseServicesKeepTheStoredLifetimeAndInactiveFallback)
{
	World world;
	MAttackZoneEffectGenerator generator;
	MEffect::SetHost(nullptr);
	CHECK(generator.Generate(Info()));
	CHECK_EQ(29, effects.front()->GetEndFrame()); CHECK_EQ(4, effects.front()->GetEndLinkFrame());
	CHECK_EQ(0, effects.front()->GetLight()); CHECK(!effects.front()->Update());
	CHECK_EQ(96, effects.front()->GetPixelX()); CHECK_EQ(48, effects.front()->GetPixelY());
}

TEST(ZoneAttackEffectGenerator, CountsStayFiniteAndLinkDeadlinesRemainIndependent)
{
	World world;
	MAttackZoneEffectGenerator generator;
	auto info = Info(); info.count = 0xFFFF; info.linkCount = MAX_LINKCOUNT;
	CHECK(generator.Generate(info));
	CHECK_EQ(65634, effects.back()->GetEndFrame()); CHECK_EQ(65634, effects.back()->GetEndLinkFrame());
	info.count = 10; info.linkCount = 20;
	CHECK(generator.Generate(info));
	CHECK_EQ(109, effects.back()->GetEndFrame()); CHECK_EQ(119, effects.back()->GetEndLinkFrame());
}

TEST(ZoneAttackEffectGenerator, CountsRetainTheirAbsoluteClockWrap)
{
	World world;
	MAttackZoneEffectGenerator generator;
	frameNow = (std::numeric_limits<DWORD>::max)() - 1;
	auto info = Info(); info.count = 4;
	CHECK(generator.Generate(info)); CHECK_EQ(1, effects.front()->GetEndFrame());
	CHECK(!effects.front()->Update());
	frameNow = 0;
	CHECK(effects.front()->Update()); CHECK_EQ(126, effects.front()->GetPixelX()); CHECK_EQ(88, effects.front()->GetPixelY());
}
