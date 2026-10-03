#include "test_framework.h"
#include "MStopZoneEmptyCrossEffectGenerator.h"
#include "MEffect.h"

#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MFixedZoneEffectSprite sprite;
bool spriteAvailable;
int requestedSprite, submissions, acceptanceMask;
std::vector<int> calls, slots, removedTargets;
std::vector<std::unique_ptr<MEffect>> effects;
struct Point
{
	int x, y;
	bool operator==(const Point&) const = default;
};
std::vector<Point> attempted;
const std::vector<Point> defaultPoints{{4, 5}, {6, 5}, {5, 4}, {5, 6}};
const Point linkedPoints[] = {{777, 888}, {309, 130}, {261, 106}, {261, 154}};
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(3); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE frame) { calls.push_back(2); return 7 + frame; },
};
const MEffectTargetHost targetHost{
	.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); },
};
const MFixedZoneEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& result) {
		calls.push_back(1); requestedSprite = type; result = sprite; return spriteAvailable;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		calls.push_back(4); attempted.push_back({effect->GetX(), effect->GetY()});
		CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		const int slot = submissions++;
		if ((acceptanceMask & (1 << slot)) == 0) return false;
		slots.push_back(slot); effects.push_back(std::move(effect)); return true;
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MFixedZoneEffectHost* previousCross = MStopZoneEmptyCrossEffectGenerator::SetHost(&host);
	MStopZoneEmptyCrossEffectGenerator generator;
	World()
	{
		frameNow = 100; sprite = {BLT_EFFECT, 12, 3}; spriteAvailable = true;
		requestedSprite = -1; submissions = 0; acceptanceMask = 15;
		effects.clear(); calls.clear(); slots.clear(); removedTargets.clear(); attempted.clear();
	}
	~World()
	{
		effects.clear(); MStopZoneEmptyCrossEffectGenerator::SetHost(previousCross);
		MEffectTarget::SetHost(previousTarget); MEffect::SetHost(previousEffect);
	}
};

EFFECTGENERATOR_INFO Info()
{
	EFFECTGENERATOR_INFO info{};
	info.nActionInfo = 42; info.effectSpriteType = 17;
	info.x0 = 261; info.y0 = 130; info.z0 = 17;
	info.x1 = 900; info.y1 = 800; info.z1 = 99;
	info.direction = DIRECTION_RIGHTUP; info.step = 9; info.count = 30; info.linkCount = 5;
	info.power = 2; info.creatureID = 123;
	return info;
}

std::unique_ptr<MEffectTarget> Target(BYTE id = 73)
{
	auto target = std::make_unique<MEffectTarget>(3);
	target->NextPhase(); target->m_EffectID = id; target->Set(777, 888, 999, 456);
	target->SetServerID(789); target->SetDelayFrame(31); target->SetResultTime();
	target->SetResult(new MActionResult); return target;
}

void ClearEffects()
{
	effects.clear(); calls.clear(); slots.clear(); attempted.clear(); submissions = 0;
}

} // namespace

TEST(EmptyCrossEffectGenerator, ConfiguresFourRealArmsWithoutACenter)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_STOP_ZONE_EMPTY_CROSS, world.generator.GetID());
	CHECK(world.generator.Generate(Info())); CHECK_EQ(4, effects.size()); CHECK(attempted == defaultPoints);
	CHECK_EQ(17, requestedSprite); CHECK(calls == std::vector<int>({1, 2, 3, 4, 2, 3, 4, 2, 3, 4, 2, 3, 4}));
	for (size_t i = 0; i < effects.size(); ++i)
	{
		auto& effect = *effects[i]; CHECK_EQ(MEffect::EFFECT_SECTOR, effect.GetEffectType());
		CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame());
		CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight()); CHECK_EQ(defaultPoints[i].x * 48, effect.GetPixelX());
		CHECK_EQ(defaultPoints[i].y * 24, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ());
		CHECK_EQ(9, effect.GetStepPixel()); CHECK_EQ(DIRECTION_RIGHTUP, effect.GetDirection()); CHECK_EQ(2, effect.GetPower());
		CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame()); CHECK_EQ(42, effect.GetActionInfo());
		CHECK(!effect.IsMulti()); CHECK(effect.GetEffectTarget() == nullptr);
	}
}

TEST(EmptyCrossEffectGenerator, PowerDoesNotChangeTheFixedPattern)
{
	World world;
	for (const BYTE power : {static_cast<BYTE>(0), static_cast<BYTE>(255)})
	{
		ClearEffects(); auto info = Info(); info.power = power;
		CHECK(world.generator.Generate(info)); CHECK(attempted == defaultPoints);
		for (const auto& effect : effects) CHECK_EQ(power, effect->GetPower());
	}
}

TEST(EmptyCrossEffectGenerator, LowerSectorBoundaryWrapsWhileCopiesKeepNegativePixelRemainders)
{
	World world;
	auto target = Target(); auto info = Info(); info.x0 = info.y0 = -1; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release();
	CHECK(attempted == std::vector<Point>({{65535, 0}, {1, 0}, {0, 65535}, {0, 1}}));
	CHECK_EQ(3145680, effects[0]->GetPixelX()); CHECK_EQ(1572840, effects[2]->GetPixelY());
	const Point expected[] = {{777, 888}, {47, -1}, {-1, -25}, {-1, 23}};
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(expected[i].x, effects[i]->GetEffectTarget()->GetX()); CHECK_EQ(expected[i].y, effects[i]->GetEffectTarget()->GetY());
	}
}

TEST(EmptyCrossEffectGenerator, MaximumAndNegativeTileOriginsWrapTheirNeighbours)
{
	World world;
	for (const Point source : {Point{3145680, 1572840}, {-48, -24}})
	{
		ClearEffects(); auto info = Info(); info.x0 = source.x; info.y0 = source.y;
		CHECK(world.generator.Generate(info));
		CHECK(attempted == std::vector<Point>({{65534, 65535}, {0, 65535}, {65535, 65534}, {65535, 0}}));
	}
}

TEST(EmptyCrossEffectGenerator, SourceTilesNarrowBeforeApplyingArmOffsets)
{
	World world;
	auto target = Target(); auto info = Info(); info.x0 = 3145728 + 261; info.y0 = 1572864 + 130; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release(); CHECK(attempted == defaultPoints);
	CHECK_EQ(3146037, effects[1]->GetEffectTarget()->GetX()); CHECK_EQ(1572994, effects[1]->GetEffectTarget()->GetY());
}

TEST(EmptyCrossEffectGenerator, LaterTargetsUseSourcePixelsAndKeepPhaseWithoutResultOwnership)
{
	World world;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release();
	for (size_t i = 0; i < effects.size(); ++i)
	{
		const auto* linked = effects[i]->GetEffectTarget(); CHECK(linked != nullptr); if (!linked) continue;
		CHECK_EQ(i == 0, linked == info.pEffectTarget); CHECK_EQ(1, linked->GetCurrentPhase()); CHECK_EQ(3, linked->GetMaxPhase());
		CHECK_EQ(31, linked->GetDelayFrame()); CHECK_EQ(73, linked->GetEffectID());
		CHECK_EQ(i == 0, linked->IsExistResult()); CHECK_EQ(i == 0, linked->IsResultTime());
		CHECK_EQ(i == 0 ? 789 : OBJECTID_NULL, linked->GetServerID());
		CHECK_EQ(linkedPoints[i].x, linked->GetX()); CHECK_EQ(linkedPoints[i].y, linked->GetY());
		CHECK_EQ(i == 0 ? 999 : 17, linked->GetZ()); CHECK_EQ(i == 0 ? 456 : 123, linked->GetID());
		for (size_t j = 0; j < i; ++j) CHECK(linked != effects[j]->GetEffectTarget());
	}
}

TEST(EmptyCrossEffectGenerator, EveryAcceptanceMaskReportsOnlyOriginalTargetTransfer)
{
	World world;
	for (int mask = 0; mask < 16; ++mask)
	{
		ClearEffects(); acceptanceMask = mask; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK_EQ((mask & 1) != 0, world.generator.Generate(info)); CHECK_EQ(4, submissions);
		bool originalOwned = false;
		for (size_t i = 0; i < effects.size(); ++i)
		{
			const auto* linked = effects[i]->GetEffectTarget(); CHECK(linked != nullptr); if (!linked) continue;
			originalOwned |= linked == target.get(); CHECK_EQ(slots[i] == 0, linked == target.get());
			CHECK_EQ(linkedPoints[slots[i]].x, linked->GetX()); CHECK_EQ(linkedPoints[slots[i]].y, linked->GetY());
		}
		CHECK_EQ((mask & 1) != 0, originalOwned); if (originalOwned) target.release();
	}
}

TEST(EmptyCrossEffectGenerator, TargetlessAcceptanceStillReportsSlotZeroAndLinksEachAction)
{
	World world;
	for (int mask = 0; mask < 16; ++mask)
	{
		ClearEffects(); acceptanceMask = mask;
		CHECK_EQ((mask & 1) != 0, world.generator.Generate(Info())); CHECK_EQ(4, submissions);
		for (const auto& effect : effects) { CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(42, effect->GetActionInfo()); }
	}
}

TEST(EmptyCrossEffectGenerator, LaterCopiesSurviveCallerCleanupAfterFirstArmRejection)
{
	World world;
	acceptanceMask = 14; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK_EQ(3, effects.size()); target.reset();
	for (size_t i = 0; i < effects.size(); ++i)
	{
		const auto* linked = effects[i]->GetEffectTarget();
		CHECK_EQ(linkedPoints[i + 1].x, linked->GetX()); CHECK_EQ(linkedPoints[i + 1].y, linked->GetY());
		CHECK_EQ(31, linked->GetDelayFrame()); CHECK(!linked->IsExistResult());
	}
}

TEST(EmptyCrossEffectGenerator, DestinationAndPreviousEffectDoNotAlterSourceTargets)
{
	World world;
	MEffect previous(BLT_NORMAL); previous.SetFrameID(77, 3);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); info.pPreviousEffect = &previous;
	info.x1 = (std::numeric_limits<int>::max)(); info.y1 = (std::numeric_limits<int>::min)(); info.z1 = info.x1;
	CHECK(world.generator.Generate(info)); target.release(); CHECK(attempted == defaultPoints);
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(12, effects[i]->GetFrameID()); CHECK_EQ(linkedPoints[i].x, effects[i]->GetEffectTarget()->GetX());
		CHECK_EQ(linkedPoints[i].y, effects[i]->GetEffectTarget()->GetY());
	}
}

TEST(EmptyCrossEffectGenerator, MissingMetadataRejectsBeforeConstruction)
{
	World world;
	const MFixedZoneEffectHost empty{};
	for (const auto* service : {static_cast<const MFixedZoneEffectHost*>(nullptr), &empty, &host})
	{
		ClearEffects(); MStopZoneEmptyCrossEffectGenerator::SetHost(service); spriteAvailable = false;
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions);
		CHECK(calls == (service == &host ? std::vector<int>({1}) : std::vector<int>{})); CHECK_EQ(777, target->GetX());
	}
}

TEST(EmptyCrossEffectGenerator, MissingQueueReleasesAllUnlinkedEffects)
{
	World world;
	const MFixedZoneEffectHost metadataOnly{.Sprite = host.Sprite}; MStopZoneEmptyCrossEffectGenerator::SetHost(&metadataOnly);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions); CHECK_EQ(777, target->GetX());
	CHECK(calls == std::vector<int>({1, 2, 3, 2, 3, 2, 3, 2, 3})); CHECK(removedTargets.empty());
}

TEST(EmptyCrossEffectGenerator, MetadataRefreshesAndFrameCountsRetainByteNarrowing)
{
	World world;
	struct Case { int supplied, expected; };
	for (const Case c : {Case{-1, 255}, {0, 0}, {256, 0}, {258, 2}})
	{
		ClearEffects(); sprite = {BLT_NORMAL, 23, c.supplied};
		CHECK(world.generator.Generate(Info()));
		for (const auto& effect : effects)
		{
			CHECK_EQ(BLT_NORMAL, effect->GetBltType()); CHECK_EQ(23, effect->GetFrameID());
			CHECK_EQ(c.expected, effect->GetMaxFrame()); CHECK_EQ(0, effect->GetFrame());
		}
	}
}

TEST(EmptyCrossEffectGenerator, SpriteCallbackCanReplaceTheSubmissionService)
{
	World world;
	const MFixedZoneEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MFixedZoneEffectSprite& result) {
			result = {BLT_NORMAL, 20, 4}; MStopZoneEmptyCrossEffectGenerator::SetHost(&host); return true;
		},
	};
	CHECK(MStopZoneEmptyCrossEffectGenerator::SetHost(&changing) == &host);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(4, effects.size());
	CHECK_EQ(2, calls.front()); CHECK_EQ(20, effects.back()->GetFrameID()); CHECK_EQ(4, effects.back()->GetMaxFrame());
	CHECK(MStopZoneEmptyCrossEffectGenerator::SetHost(nullptr) == &host);
}

TEST(EmptyCrossEffectGenerator, SpriteCallbackCanRemoveQueuesBeforeConstruction)
{
	World world;
	const MFixedZoneEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MFixedZoneEffectSprite& result) {
			result = sprite; MStopZoneEmptyCrossEffectGenerator::SetHost(nullptr); return true;
		},
		.Queue = host.Queue,
	};
	MStopZoneEmptyCrossEffectGenerator::SetHost(&changing); CHECK(!world.generator.Generate(Info())); CHECK(effects.empty());
	CHECK(calls == std::vector<int>({2, 3, 2, 3, 2, 3, 2, 3})); CHECK_EQ(0, submissions);
}

TEST(EmptyCrossEffectGenerator, QueueReplacementRetainsTheInitialSpriteSnapshot)
{
	World world;
	const MFixedZoneEffectHost changing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			sprite = {BLT_NORMAL, 23, 8}; MStopZoneEmptyCrossEffectGenerator::SetHost(&host); return host.Queue(std::move(effect));
		},
	};
	MStopZoneEmptyCrossEffectGenerator::SetHost(&changing);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(4, effects.size());
	for (const auto& effect : effects) { CHECK_EQ(12, effect->GetFrameID()); CHECK_EQ(3, effect->GetMaxFrame()); }
}

TEST(EmptyCrossEffectGenerator, QueueRemovalAfterTheFirstArmKeepsItsOriginalTarget)
{
	World world;
	const MFixedZoneEffectHost changing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			MStopZoneEmptyCrossEffectGenerator::SetHost(nullptr); return host.Queue(std::move(effect));
		},
	};
	MStopZoneEmptyCrossEffectGenerator::SetHost(&changing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); CHECK_EQ(1, effects.size()); CHECK_EQ(1, submissions);
	CHECK(effects.front()->GetEffectTarget() == target.get()); target.release(); CHECK_EQ(777, info.pEffectTarget->GetX());
}

TEST(EmptyCrossEffectGenerator, RejectingQueuesDestroyTheirAttachedMarkers)
{
	World world;
	const MFixedZoneEffectHost rejecting{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { effect->SetLink(43, Target(74).release()); return false; },
	};
	MStopZoneEmptyCrossEffectGenerator::SetHost(&rejecting);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(removedTargets == std::vector<int>(4, 74));
	target.reset(); CHECK_EQ(5, removedTargets.size()); CHECK_EQ(73, removedTargets.back());
}

TEST(EmptyCrossEffectGenerator, FirstQueueExceptionLeavesTheCallerTargetUntouched)
{
	World world;
	const MFixedZoneEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool {
			effect->SetLink(43, Target(74).release()); throw std::runtime_error("Queue failure");
		},
	};
	MStopZoneEmptyCrossEffectGenerator::SetHost(&throwing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK(removedTargets == std::vector<int>({74})); CHECK_EQ(777, target->GetX());
	target.reset(); CHECK(removedTargets == std::vector<int>({74, 73}));
}

TEST(EmptyCrossEffectGenerator, LaterQueueExceptionPreservesTransferredOwnership)
{
	World world;
	const MFixedZoneEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool {
			if (effects.empty()) return host.Queue(std::move(effect));
			effect->SetLink(43, Target(74).release()); throw std::runtime_error("Later queue failure");
		},
	};
	MStopZoneEmptyCrossEffectGenerator::SetHost(&throwing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, effects.size()); CHECK(effects.front()->GetEffectTarget() == target.get());
	target.release(); CHECK(removedTargets == std::vector<int>({74})); CHECK_EQ(777, info.pEffectTarget->GetX());
	ClearEffects(); CHECK(removedTargets == std::vector<int>({74, 73}));
}

TEST(EmptyCrossEffectGenerator, AcceptanceAndRejectionDoNotConsumeRandomness)
{
	World world;
	for (const int mask : {0, 15})
	{
		ClearEffects(); acceptanceMask = mask;
		std::srand(719); const int next = std::rand(); std::srand(719);
		CHECK_EQ(mask != 0, world.generator.Generate(Info())); CHECK_EQ(next, std::rand());
		for (const auto& effect : effects) CHECK_EQ(0, effect->GetFrame());
	}
}

TEST(EmptyCrossEffectGenerator, RealEffectsAnimateAndRefreshLightWithoutMoving)
{
	World world;
	sprite.bltType = BLT_NORMAL; CHECK(world.generator.Generate(Info()));
	for (const auto& effect : effects)
	{
		const int x = effect->GetPixelX(), y = effect->GetPixelY();
		frameNow = 100; CHECK(effect->Update()); CHECK_EQ(1, effect->GetFrame()); CHECK_EQ(8, effect->GetLight());
		CHECK_EQ(x, effect->GetPixelX()); CHECK_EQ(y, effect->GetPixelY()); CHECK_EQ(17, effect->GetPixelZ());
		frameNow = 130; CHECK(!effect->Update()); CHECK_EQ(2, effect->GetFrame()); CHECK_EQ(9, effect->GetLight());
	}
}

TEST(EmptyCrossEffectGenerator, CountsRemainFiniteAndLinkTimingIsIndependent)
{
	World world;
	auto info = Info(); info.count = 65535; info.linkCount = MAX_LINKCOUNT;
	CHECK(world.generator.Generate(info)); CHECK_EQ(65634, effects.back()->GetEndFrame()); CHECK_EQ(65634, effects.back()->GetEndLinkFrame());
	ClearEffects(); info.count = 10; info.linkCount = 20;
	CHECK(world.generator.Generate(info)); CHECK_EQ(109, effects.back()->GetEndFrame()); CHECK_EQ(119, effects.back()->GetEndLinkFrame());
	ClearEffects(); frameNow = (std::numeric_limits<DWORD>::max)() - 1; info.count = 4;
	CHECK(world.generator.Generate(info)); CHECK_EQ(1, effects.back()->GetEndFrame()); CHECK(!effects.back()->Update());
	frameNow = 0; CHECK(effects.back()->Update()); CHECK_EQ(2, effects.back()->GetFrame());
}

TEST(EmptyCrossEffectGenerator, MissingBaseServicesRetainCountsAndInactiveFallback)
{
	World world;
	MEffect::SetHost(nullptr);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(29, effects.front()->GetEndFrame());
	CHECK_EQ(4, effects.front()->GetEndLinkFrame()); CHECK_EQ(0, effects.front()->GetLight()); CHECK(!effects.front()->Update());
}
