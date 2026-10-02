#include "test_framework.h"
#include "MRisingEffectGenerator.h"
#include "MLinearEffect.h"
#include "SkillDef.h"

#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MRisingEffectSprite sprite;
bool spriteAvailable;
int requestedSprite, nextLight, submissions;
unsigned acceptMask;
std::vector<int> calls, slots, removedTargets;
std::vector<std::unique_ptr<MEffect>> effects;
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(3); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE) { calls.push_back(2); return nextLight++; },
};
const MEffectTargetHost targetHost{
	.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); },
};
const MRisingEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MRisingEffectSprite& result) {
		calls.push_back(1); requestedSprite = type;
		result = sprite;
		return spriteAvailable;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		calls.push_back(4);
		CHECK(effect->GetEffectTarget() == nullptr);
		CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		const int slot = submissions++;
		if (!(acceptMask & (1u << slot))) return false;
		slots.push_back(slot);
		effects.push_back(std::move(effect));
		return true;
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MRisingEffectHost* previousRising = MRisingEffectGenerator::SetHost(&host);
	World()
	{
		frameNow = 100; nextLight = 7;
		sprite = {BLT_EFFECT, 12, 3}; spriteAvailable = true;
		requestedSprite = -1; submissions = 0; acceptMask = 0xFFFF;
		effects.clear(); calls.clear(); slots.clear(); removedTargets.clear();
	}
	~World()
	{
		effects.clear();
		MRisingEffectGenerator::SetHost(previousRising);
		MEffectTarget::SetHost(previousTarget);
		MEffect::SetHost(previousEffect);
	}
};

EFFECTGENERATOR_INFO Info(TYPE_ACTIONINFO action = 42)
{
	EFFECTGENERATOR_INFO info{};
	info.nActionInfo = action; info.effectSpriteType = 17;
	info.x0 = 96; info.y0 = 48; info.z0 = 12;
	info.x1 = 900; info.y1 = 800; info.z1 = 700;
	info.creatureID = 123;
	info.direction = DIRECTION_LEFTUP; info.step = 10;
	info.count = 30; info.linkCount = 5; info.power = 77;
	return info;
}

std::unique_ptr<MEffectTarget> Target()
{
	auto target = std::make_unique<MEffectTarget>(3);
	target->NextPhase(); target->m_EffectID = 73;
	target->Set(777, 888, 999, 456);
	target->SetServerID(789); target->SetDelayFrame(31);
	target->SetResultTime(); target->SetResult(new MActionResult);
	return target;
}

} // namespace

TEST(RisingEffectGenerator, OrdinaryShotUsesSourceCoordinatesAndConfiguredLifetime)
{
	World world;
	MRisingEffectGenerator generator;
	CHECK_EQ(EFFECTGENERATORID_RISING, generator.GetID());
	CHECK(generator.Generate(Info()));
	CHECK_EQ(1, effects.size());
	const auto& effect = *effects.front();
	CHECK_EQ(MEffect::EFFECT_LINEAR, effect.GetEffectType());
	CHECK_EQ(BLT_EFFECT, effect.GetBltType());
	CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame());
	CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight());
	CHECK_EQ(96, effect.GetPixelX()); CHECK_EQ(48, effect.GetPixelY());
	CHECK_EQ(12, effect.GetPixelZ());
	CHECK_EQ(2, effect.GetX()); CHECK_EQ(2, effect.GetY());
	CHECK_EQ(DIRECTION_DOWN, effect.GetDirection());
	CHECK_EQ(10, effect.GetStepPixel());
	CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame());
	CHECK_EQ(77, effect.GetPower()); CHECK_EQ(42, effect.GetActionInfo());
	CHECK_EQ(17, requestedSprite);
	CHECK(calls == std::vector<int>({1, 2, 3, 4}));
}

TEST(RisingEffectGenerator, OrdinaryShotRisesAndArrivesBeforeAnimatingTheLastStep)
{
	World world;
	MRisingEffectGenerator generator;
	CHECK(generator.Generate(Info()));
	for (int i = 1; i < 30; ++i)
	{
		CHECK(effects.front()->Update());
		CHECK_EQ(12 + 10 * i, effects.front()->GetPixelZ());
	}
	CHECK_EQ(2, effects.front()->GetFrame());
	const int light = effects.front()->GetLight();
	CHECK(!effects.front()->Update());
	CHECK_EQ(312, effects.front()->GetPixelZ());
	CHECK_EQ(2, effects.front()->GetFrame());
	CHECK_EQ(light, effects.front()->GetLight());
	CHECK_EQ(0, effects.front()->GetEndFrame());
	CHECK_EQ(104, effects.front()->GetEndLinkFrame());
}

TEST(RisingEffectGenerator, OrdinaryTargetTransfersWithoutBeingRetargeted)
{
	World world;
	MRisingEffectGenerator generator;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(generator.Generate(info)); target.release();
	CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget);
	CHECK_EQ(777, info.pEffectTarget->GetX());
	CHECK_EQ(888, info.pEffectTarget->GetY());
	CHECK_EQ(999, info.pEffectTarget->GetZ());
	CHECK_EQ(456, info.pEffectTarget->GetID());
	effects.clear();
	CHECK(removedTargets == std::vector<int>({73}));
}

TEST(RisingEffectGenerator, VolleyCreatesOrderedSideShotsAndACentralOriginalTarget)
{
	World world;
	MRisingEffectGenerator generator;
	auto target = Target(); auto info = Info(SKILL_FIRE_CRACKER_VOLLEY_1);
	info.pEffectTarget = target.get(); auto* result = target->GetResult();
	CHECK(generator.Generate(info)); target.release();
	CHECK_EQ(3, effects.size());
	CHECK(slots == std::vector<int>({0, 1, 2}));
	const int x[] = {188, 96, 4}, z[] = {297, 312, 297};
	const int direction[] = {DIRECTION_RIGHT, DIRECTION_DOWN, DIRECTION_LEFT};
	for (int i = 0; i < 3; ++i)
	{
		auto* linked = effects[i]->GetEffectTarget();
		CHECK_EQ(x[i], linked->GetX()); CHECK_EQ(48, linked->GetY());
		CHECK_EQ(z[i], linked->GetZ()); CHECK_EQ(123, linked->GetID());
		CHECK_EQ(73, linked->GetEffectID()); CHECK_EQ(1, linked->GetCurrentPhase());
		CHECK_EQ(3, linked->GetMaxPhase()); CHECK_EQ(31, linked->GetDelayFrame());
		CHECK_EQ(direction[i], effects[i]->GetDirection());
		CHECK_EQ(i == 1 ? 10 : 9, effects[i]->GetStepPixel());
		CHECK_EQ(129, effects[i]->GetEndFrame()); CHECK_EQ(104, effects[i]->GetEndLinkFrame());
		CHECK_EQ(77, effects[i]->GetPower());
		CHECK_EQ(info.nActionInfo, effects[i]->GetActionInfo());
		CHECK_EQ(i == 1 ? 789 : OBJECTID_NULL, linked->GetServerID());
		CHECK_EQ(i == 1, linked->IsResultTime());
		CHECK(linked->GetResult() == (i == 1 ? result : nullptr));
		CHECK((linked == info.pEffectTarget) == (i == 1));
	}
	CHECK(effects[0]->GetEffectTarget() != effects[2]->GetEffectTarget());
	CHECK(calls == std::vector<int>({1, 2, 3, 4, 2, 3, 4, 2, 3, 4}));
	effects.clear();
	CHECK(removedTargets == std::vector<int>({73, 73, 73}));
}

TEST(RisingEffectGenerator, VolleySideProjectilesTravelTowardTheirOwnTargets)
{
	World world;
	MRisingEffectGenerator generator;
	CHECK(generator.Generate(Info(SKILL_FIRE_CRACKER_VOLLEY_1)));
	for (auto& effect : effects) CHECK(effect->Update());
	CHECK_EQ(98, effects[0]->GetPixelX()); CHECK_EQ(20, effects[0]->GetPixelZ());
	CHECK_EQ(96, effects[1]->GetPixelX()); CHECK_EQ(22, effects[1]->GetPixelZ());
	CHECK_EQ(93, effects[2]->GetPixelX()); CHECK_EQ(20, effects[2]->GetPixelZ());
	CHECK_EQ(1, effects[0]->GetFrame()); CHECK_EQ(1, effects[2]->GetFrame());
}

TEST(RisingEffectGenerator, StormRetainsTenAndThirtyDegreeOrderAndUsesIndexOneForOriginal)
{
	World world;
	MRisingEffectGenerator generator;
	auto target = Target(); auto info = Info(SKILL_FIRE_CRACKER_STORM);
	info.step = 9; info.count = 31; info.pEffectTarget = target.get();
	CHECK(generator.Generate(info)); target.release();
	CHECK_EQ(4, effects.size());
	const int x[] = {144, 235, -43, 48}, z[] = {286, 253, 253, 286};
	for (int i = 0; i < 4; ++i)
	{
		auto* linked = effects[i]->GetEffectTarget();
		CHECK_EQ(x[i], linked->GetX()); CHECK_EQ(z[i], linked->GetZ());
		CHECK_EQ(48, linked->GetY()); CHECK_EQ(123, linked->GetID());
		CHECK((linked == info.pEffectTarget) == (i == 1));
		CHECK_EQ(8, effects[i]->GetStepPixel());
		CHECK_EQ(i < 2 ? DIRECTION_RIGHT : DIRECTION_LEFT, effects[i]->GetDirection());
		CHECK_EQ(info.nActionInfo, effects[i]->GetActionInfo());
	}
	effects.clear();
	CHECK(removedTargets == std::vector<int>({73, 73, 73, 73}));
}

TEST(RisingEffectGenerator, DragonAndEveryVolleyActionSelectThreeShots)
{
	World world;
	MRisingEffectGenerator generator;
	for (int action = SKILL_FIRE_CRACKER_VOLLEY_1; action <= SKILL_FIRE_CRACKER_WIDE_VOLLEY_4; ++action)
	{
		submissions = 0; effects.clear();
		CHECK(generator.Generate(Info(static_cast<TYPE_ACTIONINFO>(action))));
		CHECK_EQ(3, effects.size());
	}
	submissions = 0; effects.clear();
	CHECK(generator.Generate(Info(SKILL_DRAGON_FIRE_CRACKER)));
	CHECK_EQ(3, effects.size());
}

TEST(RisingEffectGenerator, ActionsOutsideTheFireworkPatternsUseOneShot)
{
	World world;
	MRisingEffectGenerator generator;
	for (int action : {SKILL_FIRE_CRACKER_4, SAND_OF_SOUL_STONE})
	{
		submissions = 0; effects.clear();
		CHECK(generator.Generate(Info(static_cast<TYPE_ACTIONINFO>(action))));
		CHECK_EQ(1, effects.size());
	}
}

TEST(RisingEffectGenerator, PatternsWithoutTargetsStillCarryTheAction)
{
	World world;
	MRisingEffectGenerator generator;
	for (const auto action : {SKILL_FIRE_CRACKER_VOLLEY_1, SKILL_FIRE_CRACKER_STORM})
	{
		submissions = 0; effects.clear();
		CHECK(generator.Generate(Info(action)));
		for (const auto& effect : effects)
		{
			CHECK(effect->GetEffectTarget() == nullptr);
			CHECK_EQ(action, effect->GetActionInfo());
		}
	}
	CHECK(removedTargets.empty());
}

TEST(RisingEffectGenerator, RejectedSideShotsDoNotPreventOriginalTargetTransfer)
{
	World world;
	MRisingEffectGenerator generator;
	acceptMask = 2;
	auto target = Target(); auto info = Info(SKILL_FIRE_CRACKER_STORM);
	info.pEffectTarget = target.get();
	CHECK(generator.Generate(info)); target.release();
	CHECK_EQ(4, submissions); CHECK_EQ(1, effects.size());
	CHECK(slots == std::vector<int>({1}));
	CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget);
	CHECK(removedTargets.empty());
	effects.clear();
	CHECK(removedTargets == std::vector<int>({73}));
}

TEST(RisingEffectGenerator, OrdinaryRejectionLeavesTheTargetAndCoordinatesWithTheCaller)
{
	World world;
	MRisingEffectGenerator generator;
	acceptMask = 0;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!generator.Generate(info));
	CHECK(effects.empty()); CHECK(removedTargets.empty());
	CHECK_EQ(999, target->GetZ());
}

TEST(RisingEffectGenerator, QueueExceptionsReleaseTheSubmittedEffectWithoutTakingTheTarget)
{
	World world;
	MRisingEffectGenerator generator;
	const MRisingEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool {
			auto marker = Target(); marker->m_EffectID = 74;
			effect->SetLink(43, marker.release());
			throw std::runtime_error("Queue failure");
		},
	};
	MRisingEffectGenerator::SetHost(&throwing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	bool threw = false;
	try { generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK(removedTargets == std::vector<int>({74}));
	CHECK_EQ(999, target->GetZ());
}

TEST(RisingEffectGenerator, MetadataRefreshesPerGenerationAndNarrowsFrameCounts)
{
	World world;
	MRisingEffectGenerator generator;
	CHECK(generator.Generate(Info()));
	sprite = {BLT_NORMAL, 23, 258};
	CHECK(generator.Generate(Info()));
	CHECK_EQ(12, effects[0]->GetFrameID()); CHECK_EQ(23, effects[1]->GetFrameID());
	CHECK_EQ(2, effects[1]->GetMaxFrame()); CHECK_EQ(BLT_NORMAL, effects[1]->GetBltType());
	const int light = effects[1]->GetLight();
	CHECK(effects[1]->Update()); CHECK_EQ(light, effects[1]->GetLight());
}

TEST(RisingEffectGenerator, MissingMetadataRejectsEveryPatternBeforeConstruction)
{
	World world;
	MRisingEffectGenerator generator;
	spriteAvailable = false;
	for (const auto action : {static_cast<TYPE_ACTIONINFO>(42), static_cast<TYPE_ACTIONINFO>(SKILL_FIRE_CRACKER_VOLLEY_1), static_cast<TYPE_ACTIONINFO>(SKILL_FIRE_CRACKER_STORM)})
	{
		auto target = Target(); auto info = Info(action); info.pEffectTarget = target.get();
		const auto destroyed = removedTargets.size();
		CHECK(!generator.Generate(info)); CHECK_EQ(destroyed, removedTargets.size());
		CHECK(effects.empty());
	}
	CHECK(calls == std::vector<int>({1, 1, 1}));
}

TEST(RisingEffectGenerator, MissingOrPartialHostsSkipUnavailableOperations)
{
	World world;
	MRisingEffectGenerator generator;
	const MRisingEffectHost empty{}, queueOnly{.Queue = host.Queue};
	for (const auto* service : {static_cast<const MRisingEffectHost*>(nullptr), &empty, &queueOnly})
	{
		MRisingEffectGenerator::SetHost(service);
		CHECK(!generator.Generate(Info()));
	}
	CHECK(calls.empty());
	const MRisingEffectHost spriteOnly{.Sprite = host.Sprite};
	MRisingEffectGenerator::SetHost(&spriteOnly);
	CHECK(!generator.Generate(Info()));
	CHECK(calls == std::vector<int>({1, 2, 3}));
	CHECK(effects.empty());
}

TEST(RisingEffectGenerator, MetadataCallbackCanReplaceTheQueueService)
{
	World world;
	MRisingEffectGenerator generator;
	const MRisingEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MRisingEffectSprite& result) {
			result = {BLT_NORMAL, 23, 2}; MRisingEffectGenerator::SetHost(&host); return true;
		},
	};
	CHECK(MRisingEffectGenerator::SetHost(&changing) == &host);
	CHECK(generator.Generate(Info()));
	CHECK_EQ(23, effects.front()->GetFrameID());
	CHECK(calls == std::vector<int>({2, 3, 4}));
	CHECK(MRisingEffectGenerator::SetHost(&changing) == &host);
}

TEST(RisingEffectGenerator, MissingBaseServicesKeepStoredCountsAndInactiveUpdates)
{
	World world;
	MRisingEffectGenerator generator;
	MEffect::SetHost(nullptr);
	CHECK(generator.Generate(Info()));
	CHECK_EQ(29, effects.front()->GetEndFrame()); CHECK_EQ(4, effects.front()->GetEndLinkFrame());
	CHECK_EQ(0, effects.front()->GetLight());
	CHECK(!effects.front()->Update()); CHECK_EQ(12, effects.front()->GetPixelZ());
}

TEST(RisingEffectGenerator, ZeroSpeedKeepsEveryPatternStationary)
{
	World world;
	MRisingEffectGenerator generator;
	for (const auto action : {static_cast<TYPE_ACTIONINFO>(42), static_cast<TYPE_ACTIONINFO>(SKILL_FIRE_CRACKER_VOLLEY_1), static_cast<TYPE_ACTIONINFO>(SKILL_FIRE_CRACKER_STORM)})
	{
		submissions = 0; effects.clear();
		auto info = Info(action); info.step = 0;
		CHECK(generator.Generate(info));
		for (auto& effect : effects)
		{
			CHECK(effect->Update()); CHECK_EQ(12, effect->GetPixelZ());
			CHECK_EQ(96, effect->GetPixelX()); CHECK_EQ(0, effect->GetStepPixel());
			CHECK_EQ(1, effect->GetFrame());
		}
	}
}

TEST(RisingEffectGenerator, LifetimeExpiresBeforeMovementAndLinkCountIsIndependent)
{
	World world;
	MRisingEffectGenerator generator;
	auto info = Info(); info.count = 1; info.linkCount = 20;
	CHECK(generator.Generate(info));
	CHECK(!effects.front()->Update()); CHECK_EQ(12, effects.front()->GetPixelZ());
	CHECK_EQ(119, effects.front()->GetEndLinkFrame());
}

TEST(RisingEffectGenerator, FiniteCountsRetainTheirAbsoluteClockWrap)
{
	World world;
	MRisingEffectGenerator generator;
	frameNow = (std::numeric_limits<DWORD>::max)() - 1;
	auto info = Info(); info.count = 4;
	CHECK(generator.Generate(info)); CHECK_EQ(1, effects.front()->GetEndFrame());
	CHECK(!effects.front()->Update());
	frameNow = 0;
	CHECK(effects.front()->Update()); CHECK_EQ(22, effects.front()->GetPixelZ());
}
