#include "test_framework.h"
#include "MAttachZoneAroundEffectGenerator.h"
#include "MEffect.h"
#include <memory>
#include <vector>

TEST(AroundGroundEffectGenerator, AcceptedRingTransfersEachTargetToExactlyOneEffect)
{
	static std::vector<std::unique_ptr<MEffect>> effects;
	const MAroundGroundEffectHost host{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MFixedZoneEffectSprite& sprite) { sprite = {BLT_EFFECT, 12, 3}; return true; },
		.Queue = [](std::unique_ptr<MEffect> effect) { effects.push_back(std::move(effect)); return true; },
	};
	const auto* previous = MAttachZoneAroundEffectGenerator::SetHost(&host);
	MAttachZoneAroundEffectGenerator generator;
	auto target = std::make_unique<MEffectTarget>(3); target->NextPhase();
	EFFECTGENERATOR_INFO info{}; info.effectSpriteType = 17; info.pEffectTarget = target.get(); info.nActionInfo = 42;
	const bool accepted = generator.Generate(info);
	int originalOwners = 0;
	for (const auto& effect : effects) if (effect->GetEffectTarget() == target.get()) ++originalOwners;
	CHECK(accepted); CHECK_EQ(8, effects.size()); CHECK_EQ(1, originalOwners);
	// Detach duplicate pointers on the unfixed code, so this assertion reproduces
	// the ownership defect without letting cleanup double-delete the target.
	for (const auto& effect : effects) if (effect->GetEffectTarget() == target.get()) effect->SetEffectTargetNULL();
	effects.clear(); MAttachZoneAroundEffectGenerator::SetHost(previous);
}

#include "EffectSpriteTypeDef.h"

TEST(AroundGroundEffectGenerator, AxeDirectionsOutsideTheEightRowsRejectBeforeSubmission)
{
	static int submissions;
	const MAroundGroundEffectHost host{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MFixedZoneEffectSprite& sprite) { sprite = {BLT_EFFECT, 12, 3}; return true; },
		.Queue = [](std::unique_ptr<MEffect>) { ++submissions; return false; },
	};
	const auto* previous = MAttachZoneAroundEffectGenerator::SetHost(&host);
	EFFECTGENERATOR_INFO info{}; info.effectSpriteType = EFFECTSPRITETYPE_GREAT_RUFFIAN_1_AXE_GROUND; info.direction = 8;
	MAttachZoneAroundEffectGenerator generator; submissions = 0;
	CHECK(!generator.Generate(info)); CHECK_EQ(0, submissions);
	MAttachZoneAroundEffectGenerator::SetHost(previous);
}

#include "MEventQueue.h"
#include <cstdlib>
#include <stdexcept>

namespace {
MFixedZoneEffectSprite aroundSprite;
std::vector<std::unique_ptr<MEffect>> aroundEffects;
std::vector<int> aroundSlots, aroundCalls;
std::vector<MEvent> aroundEvents;
int aroundMask, aroundSubmissions, aroundDestroyed;
bool aroundMetadata;
DWORD aroundNow;
const MEffectHost aroundEffectHost{.CurrentFrame = [] { return aroundNow; }};
const MAroundGroundEffectHost aroundHost{
	.Sprite = [](TYPE_EFFECTSPRITETYPE, MFixedZoneEffectSprite& sprite) { aroundCalls.push_back(1); sprite = aroundSprite; return aroundMetadata; },
	.Queue = [](std::unique_ptr<MEffect> effect) {
		aroundCalls.push_back(2); const int slot = aroundSubmissions++;
		CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		if (!(aroundMask & (1 << slot))) return false;
		aroundSlots.push_back(slot); aroundEffects.push_back(std::move(effect)); return true;
	},
	.AddEvent = [](MEvent& event) { aroundCalls.push_back(3); aroundEvents.push_back(event); },
};
struct AroundWorld
{
	const MAroundGroundEffectHost* old = MAttachZoneAroundEffectGenerator::SetHost(&aroundHost);
	const MEffectHost* oldEffect = MEffect::SetHost(&aroundEffectHost);
	MAttachZoneAroundEffectGenerator generator;
	AroundWorld() { Reset(); aroundSprite = {BLT_EFFECT, 12, 3}; aroundMetadata = true; aroundNow = 100; }
	void Reset() { aroundEffects.clear(); aroundSlots.clear(); aroundCalls.clear(); aroundEvents.clear(); aroundMask = 255; aroundSubmissions = aroundDestroyed = 0; }
	~AroundWorld() { aroundEffects.clear(); MAttachZoneAroundEffectGenerator::SetHost(old); MEffect::SetHost(oldEffect); }
};
struct AroundTarget : MEffectTarget
{
	using MEffectTarget::operator=;
	AroundTarget() : MEffectTarget(4) { NextPhase(); Set(777,888,999,123); SetServerID(321); SetResult(new MActionResult); }
	~AroundTarget() override { ++aroundDestroyed; }
};
EFFECTGENERATOR_INFO AroundInfo(bool axe = false)
{
	EFFECTGENERATOR_INFO info{}; info.effectSpriteType = axe ? EFFECTSPRITETYPE_GREAT_RUFFIAN_1_AXE_GROUND : 17;
	info.x0 = 999; info.y0 = 999; info.z0 = 77; info.x1 = 480; info.y1 = 240; info.z1 = 88;
	info.nActionInfo = 42; info.direction = DIRECTION_LEFTUP; info.step = 11; info.power = 7; info.count = 30; info.linkCount = 5;
	return info;
}
bool AroundGenerate(AroundWorld& world, EFFECTGENERATOR_INFO info, std::unique_ptr<MEffectTarget>& target)
{
	info.pEffectTarget = target.get(); const bool result = world.generator.Generate(info);
	for (const auto& effect : aroundEffects) if (effect->GetEffectTarget() == target.get()) { target.release(); break; }
	return result;
}
void CheckAroundOriginal(const MEffectTarget& target)
{
	CHECK_EQ(777, target.GetX()); CHECK_EQ(888, target.GetY()); CHECK_EQ(999, target.GetZ()); CHECK_EQ(123, target.GetID());
	CHECK_EQ(321, target.GetServerID()); CHECK(target.IsExistResult()); CHECK_EQ(1, target.GetCurrentPhase());
}
}

TEST(AroundGroundEffectGenerator, EveryRingAcceptanceMaskUsesOneOriginalAndIndependentCopies)
{
	AroundWorld world;
	const int positions[8][3] = {{9,9,7},{10,9,6},{11,9,5},{9,10,0},{11,10,4},{9,11,1},{10,11,2},{11,11,3}};
	for (int mask = 0; mask < 256; ++mask)
	{
		world.Reset(); aroundMask = mask; std::unique_ptr<MEffectTarget> target = std::make_unique<AroundTarget>(); auto* original = target.get();
		CHECK_EQ(mask != 0, AroundGenerate(world, AroundInfo(), target)); CHECK_EQ(8, aroundSubmissions); CHECK(aroundEvents.empty());
		CHECK_EQ(mask == 0, target != nullptr); CheckAroundOriginal(*original);
		for (size_t i = 0; i < aroundEffects.size(); ++i)
		{
			auto& effect = *aroundEffects[i]; const int slot = aroundSlots[i];
			CHECK_EQ(positions[slot][0], effect.GetX()); CHECK_EQ(positions[slot][1], effect.GetY()); CHECK_EQ(positions[slot][2], effect.GetDirection());
			CHECK_EQ(0, effect.GetPixelZ()); CHECK_EQ(11, effect.GetStepPixel()); CHECK_EQ(7, effect.GetPower()); CHECK_EQ(42, effect.GetActionInfo());
			CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame()); CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame());
			CHECK_EQ(i == 0, effect.GetEffectTarget() == original);
			if (i != 0)
			{
				const auto& copy = *effect.GetEffectTarget(); CHECK_EQ(777, copy.GetX()); CHECK_EQ(1, copy.GetCurrentPhase());
				CHECK_EQ(OBJECTID_NULL, copy.GetServerID()); CHECK(!copy.IsExistResult());
				for (size_t j = 0; j < i; ++j) CHECK(effect.GetEffectTarget() != aroundEffects[j]->GetEffectTarget());
			}
		}
		aroundEffects.clear(); CHECK_EQ(mask != 0 ? 1 : 0, aroundDestroyed); target.reset(); CHECK_EQ(1, aroundDestroyed);
	}
}

TEST(AroundGroundEffectGenerator, AxeDirectionsAndAcceptanceMasksPreserveOrderAndShakeAfterSubmission)
{
	AroundWorld world;
	const int xs[8][2] = {{10,10},{10,17},{3,17},{3,10},{10,10},{3,10},{3,17},{10,17}};
	const int ys[8][2] = {{3,17},{3,10},{10,10},{10,3},{3,17},{10,17},{10,10},{17,10}};
	for (int direction = 0; direction < 8; ++direction) for (int mask = 0; mask < 8; ++mask)
	{
		world.Reset(); aroundMask = mask; auto info = AroundInfo(true); info.direction = static_cast<BYTE>(direction);
		std::unique_ptr<MEffectTarget> target = std::make_unique<AroundTarget>(); auto* original = target.get();
		CHECK_EQ(mask != 0, AroundGenerate(world, info, target)); CHECK_EQ(3, aroundSubmissions); CheckAroundOriginal(*original);
		CHECK(aroundCalls == std::vector<int>({1,2,2,2,3})); CHECK_EQ(1, aroundEvents.size());
		const auto& event = aroundEvents.front(); CHECK_EQ(EVENTID_METEOR_SHAKE, event.eventID); CHECK_EQ(EVENTTYPE_ZONE, event.eventType);
		CHECK_EQ(500, event.eventDelay); CHECK_EQ(EVENTFLAG_SHAKE_SCREEN, event.eventFlag); CHECK_EQ(3, event.parameter3);
		for (size_t i = 0; i < aroundEffects.size(); ++i)
		{
			const int slot = aroundSlots[i]; auto& effect = *aroundEffects[i];
			CHECK_EQ(slot == 0 ? 10 : xs[direction][slot - 1], effect.GetX()); CHECK_EQ(slot == 0 ? 10 : ys[direction][slot - 1], effect.GetY());
			CHECK_EQ(direction, effect.GetDirection()); CHECK_EQ(i == 0, effect.GetEffectTarget() == original); CHECK_EQ(42, effect.GetActionInfo());
			if (i) CHECK(!effect.GetEffectTarget()->IsExistResult());
		}
	}
}

TEST(AroundGroundEffectGenerator, TargetlessPatternsKeepTheirExistingActionLinkPolicy)
{
	AroundWorld world;
	for (const bool axe : {false, true}) for (const int mask : {0,2,5,255})
	{
		world.Reset(); aroundMask = mask; CHECK_EQ(mask != 0, world.generator.Generate(AroundInfo(axe)));
		for (size_t i = 0; i < aroundEffects.size(); ++i)
		{
			CHECK(aroundEffects[i]->GetEffectTarget() == nullptr);
			CHECK_EQ(axe && i != 0 ? ACTIONINFO_NULL : 42, aroundEffects[i]->GetActionInfo());
		}
	}
}

TEST(AroundGroundEffectGenerator, EveryInvalidAxeDirectionRejectsWithoutEffectsOrEvents)
{
	AroundWorld world;
	for (int direction = 8; direction < 256; ++direction)
	{
		world.Reset(); auto info = AroundInfo(true); info.direction = static_cast<BYTE>(direction);
		std::unique_ptr<MEffectTarget> target = std::make_unique<AroundTarget>(); CHECK(!AroundGenerate(world, info, target));
		CHECK_EQ(0, aroundSubmissions); CHECK(aroundEvents.empty()); CHECK(target != nullptr);
	}
}

TEST(AroundGroundEffectGenerator, MissingServicesKeepOwnershipAndMetadataPrecedesConstruction)
{
	AroundWorld world; const MAroundGroundEffectHost empty{}, noQueue{.Sprite = aroundHost.Sprite};
	for (const auto* services : {static_cast<const MAroundGroundEffectHost*>(nullptr), &empty, &noQueue})
	{
		world.Reset(); MAttachZoneAroundEffectGenerator::SetHost(services); std::unique_ptr<MEffectTarget> target = std::make_unique<AroundTarget>();
		CHECK(!AroundGenerate(world, AroundInfo(), target)); CHECK(target != nullptr); CHECK_EQ(0, aroundSubmissions);
	}
	world.Reset(); MAttachZoneAroundEffectGenerator::SetHost(&aroundHost); aroundMetadata = false;
	CHECK(!world.generator.Generate(AroundInfo(true))); CHECK(aroundCalls == std::vector<int>{1});
}

TEST(AroundGroundEffectGenerator, QueueAndEventExceptionsRespectOriginalOwnership)
{
	AroundWorld world; static int throwAt;
	const MAroundGroundEffectHost throwing{
		.Sprite = aroundHost.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { if (aroundSubmissions == throwAt) throw std::runtime_error("queue"); return aroundHost.Queue(std::move(effect)); },
		.AddEvent = [](MEvent&) { throw std::runtime_error("event"); },
	};
	for (const int slot : {0,1,3})
	{
		world.Reset(); throwAt = slot; MAttachZoneAroundEffectGenerator::SetHost(&throwing); auto* original = new AroundTarget;
		bool threw = false;
		try { MEffectTargetOwner owner(original); auto info = AroundInfo(true); info.pEffectTarget = original; world.generator.Generate(info); }
		catch (const std::runtime_error&) { threw = true; }
		CHECK(threw); CHECK_EQ(slot, aroundSubmissions); CHECK_EQ(slot == 0 ? 1 : 0, aroundDestroyed);
		if (slot != 0) { CHECK(aroundEffects.front()->GetEffectTarget() == original); CheckAroundOriginal(*original); }
		aroundEffects.clear(); CHECK_EQ(1, aroundDestroyed);
	}
}

TEST(AroundGroundEffectGenerator, MetadataAndQueuesReReadTheInstalledHost)
{
	AroundWorld world;
	const MAroundGroundEffectHost fromSprite{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite) { MAttachZoneAroundEffectGenerator::SetHost(&aroundHost); return aroundHost.Sprite(type,sprite); },
	};
	MAttachZoneAroundEffectGenerator::SetHost(&fromSprite); CHECK(world.generator.Generate(AroundInfo())); CHECK_EQ(8, aroundEffects.size());
	world.Reset(); const MAroundGroundEffectHost removing{
		.Sprite = aroundHost.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { const bool result = aroundHost.Queue(std::move(effect)); MAttachZoneAroundEffectGenerator::SetHost(nullptr); return result; },
		.AddEvent = aroundHost.AddEvent,
	};
	MAttachZoneAroundEffectGenerator::SetHost(&removing); std::unique_ptr<MEffectTarget> target = std::make_unique<AroundTarget>();
	CHECK(AroundGenerate(world, AroundInfo(true), target)); CHECK_EQ(1, aroundSubmissions); CHECK(aroundEvents.empty()); CHECK(!target);
}

TEST(AroundGroundEffectGenerator, RingWrapsSectorEdgesAndRealEffectsAnimateInPlace)
{
	AroundWorld world; auto info = AroundInfo(); info.x1 = info.y1 = 0;
	std::srand(51); const int next = std::rand(); std::srand(51);
	CHECK(world.generator.Generate(info)); CHECK_EQ(next, std::rand()); CHECK_EQ(65535, aroundEffects.front()->GetX()); CHECK_EQ(65535, aroundEffects.front()->GetY());
	CHECK(aroundEffects.front()->Update()); CHECK_EQ(1, aroundEffects.front()->GetFrame()); CHECK_EQ(65535, aroundEffects.front()->GetX());
	aroundNow = 129; CHECK(!aroundEffects.front()->Update());
}

TEST(AroundGroundEffectGenerator, SecondaryRingAndAxeTargetsNeverUnregisterTheOriginal)
{
	AroundWorld world;
	static std::vector<int> removed;
	const MEffectTargetHost host{.RemoveFromPlayer = [](BYTE id) { removed.push_back(id); }};
	const auto* previous = MEffectTarget::SetHost(&host);
	for (const bool axe : {false,true})
	{
		world.Reset(); removed.clear(); std::unique_ptr<MEffectTarget> target = std::make_unique<AroundTarget>();
		target->m_EffectID = 73; CHECK(AroundGenerate(world, AroundInfo(axe), target));
		while (aroundEffects.size() > 1) aroundEffects.pop_back();
		CHECK(removed.empty()); CHECK_EQ(0, aroundDestroyed); CheckAroundOriginal(*aroundEffects.front()->GetEffectTarget());
		aroundEffects.clear(); CHECK_EQ(1, aroundDestroyed); CHECK(removed == std::vector<int>{73});
	}
	MEffectTarget::SetHost(previous);
}
