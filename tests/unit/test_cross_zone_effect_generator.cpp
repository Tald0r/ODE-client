#include "test_framework.h"
#include "MStopZoneCrossEffectGenerator.h"
#include "MEffect.h"
#include "SkillDef.h"

#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MCrossZoneEffectSprite sprite;
MCrossZoneEffectBounds zoneBounds;
bool spriteAvailable, boundsAvailable, acceptQueue;
int requestedSprite, submissions, rejectBefore;
MEffectTarget* targetAtBounds;
std::vector<bool> acceptance;
std::vector<int> calls, slots, removedTargets;
std::vector<std::unique_ptr<MEffect>> effects;
struct Point
{
	int x, y;
	bool operator==(const Point&) const = default;
};
std::vector<Point> attempted;
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(3); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE frame) { calls.push_back(2); return 7 + frame; },
};
const MEffectTargetHost targetHost{
	.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); },
};
const MCrossZoneEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MCrossZoneEffectSprite& result) {
		calls.push_back(1); requestedSprite = type; result = sprite; return spriteAvailable;
	},
	.Bounds = [](MCrossZoneEffectBounds& result) {
		calls.push_back(5); result = zoneBounds;
		targetAtBounds = effects.empty() ? nullptr : effects.front()->GetEffectTarget();
		return boundsAvailable;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		calls.push_back(4); attempted.push_back({effect->GetX(), effect->GetY()});
		CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		const int slot = submissions++;
		if (!acceptQueue || slot < rejectBefore || (!acceptance.empty() &&
			(static_cast<size_t>(slot) >= acceptance.size() || !acceptance[slot]))) return false;
		slots.push_back(slot); effects.push_back(std::move(effect)); return true;
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MCrossZoneEffectHost* previousCross = MStopZoneCrossEffectGenerator::SetHost(&host);
	MStopZoneCrossEffectGenerator generator;
	World()
	{
		frameNow = 100; sprite = {BLT_EFFECT, 12, 3}; zoneBounds = {11, 11};
		spriteAvailable = boundsAvailable = acceptQueue = true;
		requestedSprite = -1; submissions = rejectBefore = 0; targetAtBounds = nullptr;
		effects.clear(); calls.clear(); slots.clear(); removedTargets.clear(); acceptance.clear(); attempted.clear();
	}
	~World()
	{
		effects.clear(); MStopZoneCrossEffectGenerator::SetHost(previousCross);
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
	effects.clear(); calls.clear(); slots.clear(); attempted.clear(); submissions = 0; targetAtBounds = nullptr;
}

const std::vector<Point> defaultPoints{{5, 5}, {5, 3}, {5, 4}, {3, 5}, {4, 5}, {6, 5}, {7, 5}, {5, 6}, {5, 7}};
const Point copiedTargets[] = {{0, 0}, {948, 776}, {948, 800}, {852, 824}, {900, 824},
	{996, 824}, {1044, 824}, {948, 848}, {948, 872}};

} // namespace

TEST(CrossZoneEffectGenerator, SubmitsTheCenterThenRowMajorCrossArms)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_STOP_ZONE_CROSS, world.generator.GetID());
	CHECK(world.generator.Generate(Info())); CHECK_EQ(9, effects.size()); CHECK(attempted == defaultPoints);
	CHECK_EQ(17, requestedSprite);
	CHECK(std::vector<int>(calls.begin(), calls.begin() + 5) == std::vector<int>({1, 2, 3, 4, 5}));
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

TEST(CrossZoneEffectGenerator, SandCrossUsesThreeForArmsButKeepsCenterPower)
{
	World world;
	for (const BYTE power : {static_cast<BYTE>(0), static_cast<BYTE>(1), static_cast<BYTE>(9)})
	{
		ClearEffects(); auto info = Info(); info.nActionInfo = SAND_CROSS; info.power = power;
		CHECK(world.generator.Generate(info)); CHECK_EQ(13, effects.size()); CHECK_EQ(power, effects.front()->GetPower());
		CHECK(attempted == std::vector<Point>({{5, 5}, {5, 2}, {5, 3}, {5, 4}, {2, 5}, {3, 5}, {4, 5},
			{6, 5}, {7, 5}, {8, 5}, {5, 6}, {5, 7}, {5, 8}}));
		for (size_t i = 1; i < effects.size(); ++i) CHECK_EQ(3, effects[i]->GetPower());
	}
}

TEST(CrossZoneEffectGenerator, ZeroPowerStillSubmitsTheCenter)
{
	World world;
	auto info = Info(); info.power = 0;
	CHECK(world.generator.Generate(info)); CHECK_EQ(1, effects.size()); CHECK_EQ(0, effects.front()->GetPower());
	CHECK(calls == std::vector<int>({1, 2, 3, 4, 5}));
}

TEST(CrossZoneEffectGenerator, ClipsArmsAtAllFourZoneEdges)
{
	World world;
	struct Case { int x, y; std::vector<Point> points; };
	for (const Case& c : {Case{0, 0, {{0, 0}, {1, 0}, {2, 0}, {0, 1}, {0, 2}}},
		{10, 10, {{10, 10}, {10, 8}, {10, 9}, {8, 10}, {9, 10}}},
		{0, 5, {{0, 5}, {0, 3}, {0, 4}, {1, 5}, {2, 5}, {0, 6}, {0, 7}}},
		{5, 0, {{5, 0}, {3, 0}, {4, 0}, {6, 0}, {7, 0}, {5, 1}, {5, 2}}}})
	{
		ClearEffects(); auto info = Info(); info.x0 = c.x * 48; info.y0 = c.y * 24;
		CHECK(world.generator.Generate(info)); CHECK(attempted == c.points); CHECK_EQ(c.points.size(), effects.size());
	}
}

TEST(CrossZoneEffectGenerator, CenterSubmissionPrecedesClippingEvenOutsideTheZone)
{
	World world;
	zoneBounds = {5, 5}; rejectBefore = 1;
	auto info = Info(); info.x0 = 5 * 48; info.y0 = 2 * 24; info.power = 1;
	auto target = Target(); info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release();
	CHECK(attempted == std::vector<Point>({{5, 2}, {4, 2}})); CHECK_EQ(1, effects.size());
	CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget); CHECK_EQ(777, info.pEffectTarget->GetX());
	CHECK(targetAtBounds == nullptr);
}

TEST(CrossZoneEffectGenerator, EmptyDimensionsKeepOnlyTheCenterAttempt)
{
	World world;
	for (const MCrossZoneEffectBounds size : {MCrossZoneEffectBounds{0, 0}, {0, 11}, {11, 0}, {1, 1}})
	{
		ClearEffects(); zoneBounds = size;
		CHECK(world.generator.Generate(Info())); CHECK(attempted == std::vector<Point>({{5, 5}}));
	}
}

TEST(CrossZoneEffectGenerator, SourceCoordinatesRetainTruncationAndSectorNarrowing)
{
	World world;
	struct Case { int x, y, sx, sy; };
	for (const Case c : {Case{-1, -1, 0, 0}, {-48, -24, 65535, 65535}, {3145728, 1572864, 0, 0}})
	{
		ClearEffects(); auto info = Info(); info.x0 = c.x; info.y0 = c.y; info.power = 0;
		CHECK(world.generator.Generate(info)); CHECK_EQ(c.sx, effects.front()->GetX()); CHECK_EQ(c.sy, effects.front()->GetY());
		CHECK_EQ(c.sx * 48, effects.front()->GetPixelX()); CHECK_EQ(c.sy * 24, effects.front()->GetPixelY());
	}
}

TEST(CrossZoneEffectGenerator, MaximumSectorBoundaryStillClipsToTheLastValidTile)
{
	World world;
	zoneBounds = {65535, 11}; rejectBefore = 1;
	auto info = Info(); info.x0 = 3145680; info.y0 = 120; info.power = 1;
	CHECK(world.generator.Generate(info)); CHECK(attempted == std::vector<Point>({{65535, 5}, {65534, 5}}));
	CHECK_EQ(1, effects.size()); CHECK_EQ(65534, effects.front()->GetX());
}

TEST(CrossZoneEffectGenerator, MaximumPowerProducesTheFullBoundedCross)
{
	World world;
	zoneBounds = {600, 600}; auto info = Info(); info.x0 = 300 * 48; info.y0 = 300 * 24; info.power = 255;
	CHECK(world.generator.Generate(info)); CHECK_EQ(1021, effects.size());
	CHECK((attempted.front() == Point{300, 300})); CHECK((attempted[1] == Point{300, 45}));
	CHECK((attempted.back() == Point{300, 555}));
	for (const auto point : attempted)
	{
		CHECK(point.x == 300 || point.y == 300); CHECK(point.x >= 45 && point.x <= 555); CHECK(point.y >= 45 && point.y <= 555);
	}
}

TEST(CrossZoneEffectGenerator, CopiesRetainPhaseButUseClippedBoundsForTheirTargets)
{
	World world;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release(); CHECK(targetAtBounds == info.pEffectTarget);
	for (size_t i = 0; i < effects.size(); ++i)
	{
		const auto* linked = effects[i]->GetEffectTarget(); CHECK(linked != nullptr);
		CHECK_EQ(i == 0, linked == info.pEffectTarget); CHECK_EQ(1, linked->GetCurrentPhase()); CHECK_EQ(3, linked->GetMaxPhase());
		CHECK_EQ(31, linked->GetDelayFrame()); CHECK_EQ(73, linked->GetEffectID());
		CHECK_EQ(i == 0, linked->IsExistResult()); CHECK_EQ(i == 0, linked->IsResultTime());
		CHECK_EQ(i == 0 ? 789 : OBJECTID_NULL, linked->GetServerID());
		CHECK_EQ(i == 0 ? 777 : copiedTargets[i].x, linked->GetX()); CHECK_EQ(i == 0 ? 888 : copiedTargets[i].y, linked->GetY());
		CHECK_EQ(i == 0 ? 999 : 17, linked->GetZ()); CHECK_EQ(i == 0 ? 456 : 123, linked->GetID());
		for (size_t j = 0; j < i; ++j) CHECK(linked != effects[j]->GetEffectTarget());
	}
}

TEST(CrossZoneEffectGenerator, ClippedCopiesKeepTheExistingLowerBoundOffset)
{
	World world;
	auto target = Target(); auto info = Info(); info.x0 = info.y0 = 0; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release();
	const Point expected[] = {{777, 888}, {900, 776}, {948, 776}, {852, 800}, {852, 824}};
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(expected[i].x, effects[i]->GetEffectTarget()->GetX()); CHECK_EQ(expected[i].y, effects[i]->GetEffectTarget()->GetY());
	}
}

TEST(CrossZoneEffectGenerator, EveryAcceptanceMaskTransfersTheOriginalToTheFirstAcceptedEffect)
{
	World world;
	for (unsigned mask = 0; mask < 512; ++mask)
	{
		ClearEffects(); acceptance.assign(9, false);
		for (unsigned slot = 0; slot < 9; ++slot) acceptance[slot] = (mask & (1u << slot)) != 0;
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK_EQ(mask != 0, world.generator.Generate(info)); CHECK_EQ(9, submissions);
		bool originalOwned = false;
		for (size_t i = 0; i < effects.size(); ++i)
		{
			const auto* linked = effects[i]->GetEffectTarget(); CHECK(linked != nullptr); if (!linked) continue;
			originalOwned |= linked == target.get(); CHECK_EQ(i == 0, linked == target.get());
			CHECK_EQ(i == 0 ? 777 : copiedTargets[slots[i]].x, linked->GetX());
			CHECK_EQ(i == 0 ? 888 : copiedTargets[slots[i]].y, linked->GetY());
		}
		CHECK_EQ(mask != 0, originalOwned); if (originalOwned) target.release();
	}
}

TEST(CrossZoneEffectGenerator, TargetlessAcceptanceStillLinksTheActionOnEveryEffect)
{
	World world;
	rejectBefore = 3;
	CHECK(world.generator.Generate(Info())); CHECK_EQ(6, effects.size());
	for (const auto& effect : effects) { CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(42, effect->GetActionInfo()); }
}

TEST(CrossZoneEffectGenerator, RejectionLeavesTheCallerOwningTheUnmodifiedTarget)
{
	World world;
	acceptQueue = false; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK_EQ(9, submissions); CHECK(effects.empty()); CHECK(removedTargets.empty());
	CHECK_EQ(777, target->GetX()); CHECK_EQ(888, target->GetY()); target.reset(); CHECK(removedTargets == std::vector<int>({73}));
}

TEST(CrossZoneEffectGenerator, MissingMetadataRejectsBeforeConstructingEffects)
{
	World world;
	const MCrossZoneEffectHost empty{};
	for (const auto* service : {static_cast<const MCrossZoneEffectHost*>(nullptr), &empty, &host})
	{
		ClearEffects(); spriteAvailable = false; MStopZoneCrossEffectGenerator::SetHost(service);
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions);
		CHECK(calls == (service == &host ? std::vector<int>({1}) : std::vector<int>{})); CHECK_EQ(777, target->GetX());
	}
}

TEST(CrossZoneEffectGenerator, MissingQueueDestroysUnlinkedEffectsAndLeavesTheCallerTarget)
{
	World world;
	const MCrossZoneEffectHost metadataOnly{.Sprite = host.Sprite, .Bounds = host.Bounds};
	MStopZoneCrossEffectGenerator::SetHost(&metadataOnly);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK_EQ(0, submissions); CHECK(effects.empty()); CHECK(removedTargets.empty());
	CHECK_EQ(20, calls.size()); CHECK_EQ(5, calls[3]); CHECK_EQ(777, target->GetX());
}

TEST(CrossZoneEffectGenerator, MissingBoundsKeepsTheCenterResultAndItsOwnership)
{
	World world;
	const MCrossZoneEffectHost noBounds{.Sprite = host.Sprite, .Queue = host.Queue};
	for (const auto* service : {&noBounds, &host})
	{
		for (const bool accepted : {false, true})
		{
			ClearEffects(); boundsAvailable = false; acceptQueue = accepted; MStopZoneCrossEffectGenerator::SetHost(service);
			auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
			CHECK_EQ(accepted, world.generator.Generate(info)); CHECK_EQ(1, submissions);
			CHECK_EQ(accepted ? 1 : 0, effects.size());
			if (accepted) { CHECK(effects.front()->GetEffectTarget() == target.get()); target.release(); }
			else CHECK_EQ(777, target->GetX());
		}
	}
}

TEST(CrossZoneEffectGenerator, MetadataAndBoundsRefreshBetweenCalls)
{
	World world;
	CHECK(world.generator.Generate(Info())); CHECK_EQ(9, effects.size());
	ClearEffects(); sprite = {BLT_NORMAL, 23, 258}; zoneBounds = {6, 6};
	CHECK(world.generator.Generate(Info())); CHECK_EQ(5, effects.size());
	CHECK(attempted == std::vector<Point>({{5, 5}, {5, 3}, {5, 4}, {3, 5}, {4, 5}}));
	for (const auto& effect : effects)
	{
		CHECK_EQ(BLT_NORMAL, effect->GetBltType()); CHECK_EQ(23, effect->GetFrameID()); CHECK_EQ(2, effect->GetMaxFrame());
		CHECK(effect->Update()); CHECK_EQ(1, effect->GetFrame()); CHECK_EQ(8, effect->GetLight());
	}
}

TEST(CrossZoneEffectGenerator, AnimationCountsRetainByteNarrowing)
{
	World world;
	struct Case { int supplied, expected; };
	for (const Case c : {Case{-1, 255}, {0, 0}, {256, 0}, {258, 2}})
	{
		ClearEffects(); sprite.maxFrames = c.supplied;
		CHECK(world.generator.Generate(Info()));
		for (const auto& effect : effects) { CHECK_EQ(c.expected, effect->GetMaxFrame()); CHECK_EQ(0, effect->GetFrame()); }
	}
}

TEST(CrossZoneEffectGenerator, SpriteCallbackCanReplaceLaterServices)
{
	World world;
	const MCrossZoneEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MCrossZoneEffectSprite& result) {
			result = {BLT_NORMAL, 20, 4}; MStopZoneCrossEffectGenerator::SetHost(&host); return true;
		},
	};
	CHECK(MStopZoneCrossEffectGenerator::SetHost(&changing) == &host);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(9, effects.size()); CHECK_EQ(20, effects.front()->GetFrameID());
	CHECK_EQ(BLT_NORMAL, effects.front()->GetBltType()); CHECK_EQ(4, effects.back()->GetMaxFrame());
	CHECK_EQ(2, calls.front()); CHECK(MStopZoneCrossEffectGenerator::SetHost(nullptr) == &host);
}

TEST(CrossZoneEffectGenerator, CenterCallbackCanReplaceBoundsAndLaterQueues)
{
	World world;
	const MCrossZoneEffectHost changing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			zoneBounds = {6, 6}; sprite = {BLT_NORMAL, 23, 8}; MStopZoneCrossEffectGenerator::SetHost(&host);
			return host.Queue(std::move(effect));
		},
	};
	MStopZoneCrossEffectGenerator::SetHost(&changing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release(); CHECK(targetAtBounds == info.pEffectTarget);
	CHECK(attempted == std::vector<Point>({{5, 5}, {5, 3}, {5, 4}, {3, 5}, {4, 5}}));
	for (const auto& effect : effects) { CHECK_EQ(12, effect->GetFrameID()); CHECK_EQ(3, effect->GetMaxFrame()); }
}

TEST(CrossZoneEffectGenerator, BoundsCallbackCanRemoveTheQueueAfterTheCenterTakesOwnership)
{
	World world;
	const MCrossZoneEffectHost changing{
		.Sprite = host.Sprite,
		.Bounds = [](MCrossZoneEffectBounds& result) {
			result = zoneBounds; MStopZoneCrossEffectGenerator::SetHost(nullptr); return true;
		},
		.Queue = host.Queue,
	};
	MStopZoneCrossEffectGenerator::SetHost(&changing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); CHECK_EQ(1, submissions); CHECK_EQ(1, effects.size());
	CHECK(effects.front()->GetEffectTarget() == target.get()); target.release(); CHECK_EQ(777, info.pEffectTarget->GetX());
}

TEST(CrossZoneEffectGenerator, SpriteCallbackCanRemoveTheQueueAndBoundsBeforeConstruction)
{
	World world;
	const MCrossZoneEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MCrossZoneEffectSprite& result) {
			result = sprite; MStopZoneCrossEffectGenerator::SetHost(nullptr); return true;
		},
		.Bounds = host.Bounds, .Queue = host.Queue,
	};
	MStopZoneCrossEffectGenerator::SetHost(&changing);
	CHECK(!world.generator.Generate(Info())); CHECK(effects.empty()); CHECK(calls == std::vector<int>({2, 3}));
}

TEST(CrossZoneEffectGenerator, RejectingQueuesDestroyTheirOwnMarkers)
{
	World world;
	const MCrossZoneEffectHost rejecting{
		.Sprite = host.Sprite, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect) { effect->SetLink(43, Target(74).release()); return false; },
	};
	MStopZoneCrossEffectGenerator::SetHost(&rejecting);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(removedTargets == std::vector<int>(9, 74));
	target.reset(); CHECK_EQ(10, removedTargets.size()); CHECK_EQ(73, removedTargets.back());
}

TEST(CrossZoneEffectGenerator, FirstQueueExceptionDestroysTheEffectAndLeavesTheCallerTarget)
{
	World world;
	const MCrossZoneEffectHost throwing{
		.Sprite = host.Sprite, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool {
			effect->SetLink(43, Target(74).release()); throw std::runtime_error("Queue failure");
		},
	};
	MStopZoneCrossEffectGenerator::SetHost(&throwing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK(removedTargets == std::vector<int>({74})); CHECK_EQ(777, target->GetX());
	target.reset(); CHECK(removedTargets == std::vector<int>({74, 73}));
}

TEST(CrossZoneEffectGenerator, LaterQueueExceptionPreservesTheAlreadyTransferredOriginal)
{
	World world;
	const MCrossZoneEffectHost throwing{
		.Sprite = host.Sprite, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool {
			if (effects.empty()) return host.Queue(std::move(effect));
			effect->SetLink(43, Target(74).release()); throw std::runtime_error("Arm queue failure");
		},
	};
	MStopZoneCrossEffectGenerator::SetHost(&throwing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, effects.size()); CHECK(effects.front()->GetEffectTarget() == target.get());
	target.release(); CHECK(removedTargets == std::vector<int>({74})); CHECK_EQ(777, info.pEffectTarget->GetX());
	ClearEffects(); CHECK(removedTargets == std::vector<int>({74, 73}));
}

TEST(CrossZoneEffectGenerator, AcceptanceAndRejectionDoNotConsumeRandomness)
{
	World world;
	for (const bool accepted : {false, true})
	{
		ClearEffects(); acceptQueue = accepted;
		std::srand(719); const int next = std::rand(); std::srand(719);
		CHECK_EQ(accepted, world.generator.Generate(Info())); CHECK_EQ(next, std::rand());
		for (const auto& effect : effects) CHECK_EQ(0, effect->GetFrame());
	}
}

TEST(CrossZoneEffectGenerator, UpdatesRealAnimationAndLightWithoutMoving)
{
	World world;
	CHECK(world.generator.Generate(Info()));
	for (const auto& effect : effects)
	{
		const int x = effect->GetPixelX(), y = effect->GetPixelY();
		frameNow = 100; CHECK(effect->Update()); CHECK_EQ(1, effect->GetFrame()); CHECK_EQ(8, effect->GetLight());
		CHECK_EQ(x, effect->GetPixelX()); CHECK_EQ(y, effect->GetPixelY()); CHECK_EQ(17, effect->GetPixelZ());
		frameNow = 130; CHECK(!effect->Update()); CHECK_EQ(2, effect->GetFrame()); CHECK_EQ(9, effect->GetLight());
	}
}

TEST(CrossZoneEffectGenerator, CountsRemainFiniteAndLinkTimingIsIndependent)
{
	World world;
	auto info = Info(); info.count = 65535; info.linkCount = MAX_LINKCOUNT;
	CHECK(world.generator.Generate(info)); CHECK_EQ(65634, effects.back()->GetEndFrame()); CHECK_EQ(65634, effects.back()->GetEndLinkFrame());
	info.count = 10; info.linkCount = 20;
	CHECK(world.generator.Generate(info)); CHECK_EQ(109, effects.back()->GetEndFrame()); CHECK_EQ(119, effects.back()->GetEndLinkFrame());
	frameNow = (std::numeric_limits<DWORD>::max)() - 1; info.count = 4;
	CHECK(world.generator.Generate(info)); CHECK_EQ(1, effects.back()->GetEndFrame()); CHECK(!effects.back()->Update());
	frameNow = 0; CHECK(effects.back()->Update()); CHECK_EQ(2, effects.back()->GetFrame());
}

TEST(CrossZoneEffectGenerator, MissingBaseServicesRetainCountsAndInactiveFallback)
{
	World world;
	MEffect::SetHost(nullptr);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(29, effects.front()->GetEndFrame());
	CHECK_EQ(4, effects.front()->GetEndLinkFrame()); CHECK_EQ(0, effects.front()->GetLight()); CHECK(!effects.front()->Update());
}

TEST(CrossZoneEffectGenerator, CopiedTargetOffsetsSaturateAtTheUpperCoordinateLimit)
{
	World world;
	const int high = (std::numeric_limits<int>::max)();
	auto target = Target(); auto info = Info(); info.x1 = info.y1 = high; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release();
	const Point expected[] = {{777, 888}, {high, high - 24}, {high, high}, {high - 48, high}, {high, high},
		{high, high}, {high, high}, {high, high}, {high, high}};
	CHECK_EQ(9, effects.size());
	for (size_t i = 0; i < effects.size(); ++i)
	{
		const auto* linked = effects[i]->GetEffectTarget();
		CHECK_EQ(expected[i].x, linked->GetX()); CHECK_EQ(expected[i].y, linked->GetY());
		CHECK_EQ(i == 0 ? 999 : 17, linked->GetZ()); CHECK_EQ(i == 0 ? 456 : 123, linked->GetID());
	}
}

TEST(CrossZoneEffectGenerator, CopiedTargetOffsetsSaturateAtTheLowerCoordinateLimit)
{
	World world;
	const int low = (std::numeric_limits<int>::min)();
	auto target = Target(); auto info = Info(); info.x1 = info.y1 = low; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release();
	const Point expected[] = {{777, 888}, {low + 48, low}, {low + 48, low}, {low, low + 24}, {low, low + 24},
		{low + 96, low + 24}, {low + 144, low + 24}, {low + 48, low + 48}, {low + 48, low + 72}};
	CHECK_EQ(9, effects.size());
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(expected[i].x, effects[i]->GetEffectTarget()->GetX()); CHECK_EQ(expected[i].y, effects[i]->GetEffectTarget()->GetY());
	}
}

TEST(CrossZoneEffectGenerator, ClippedTargetsSaturateWhileRetainingTheLowerBoundAnchor)
{
	World world;
	const int low = (std::numeric_limits<int>::min)();
	auto target = Target(); auto info = Info(); info.x0 = info.y0 = 0; info.x1 = info.y1 = low; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release();
	const Point expected[] = {{777, 888}, {low, low}, {low + 48, low}, {low, low}, {low, low + 24}};
	CHECK_EQ(5, effects.size());
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(expected[i].x, effects[i]->GetEffectTarget()->GetX()); CHECK_EQ(expected[i].y, effects[i]->GetEffectTarget()->GetY());
	}
}

TEST(CrossZoneEffectGenerator, MaximumRadiusOffsetsAreBoundedWithoutChangingTheCross)
{
	World world;
	const int low = (std::numeric_limits<int>::min)(), high = (std::numeric_limits<int>::max)();
	zoneBounds = {600, 600}; auto target = Target(); auto info = Info(); info.power = 255;
	info.x0 = 300 * 48; info.y0 = 300 * 24; info.x1 = high - 100; info.y1 = low + 10; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(1021, effects.size());
	CHECK_EQ(777, effects.front()->GetEffectTarget()->GetX()); CHECK_EQ(888, effects.front()->GetEffectTarget()->GetY());
	CHECK_EQ(high, effects[1]->GetEffectTarget()->GetX()); CHECK_EQ(low, effects[1]->GetEffectTarget()->GetY());
	CHECK_EQ(high, effects.back()->GetEffectTarget()->GetX()); CHECK_EQ(low + 12226, effects.back()->GetEffectTarget()->GetY());
	CHECK((attempted.front() == Point{300, 300})); CHECK((attempted.back() == Point{300, 555}));
}

TEST(CrossZoneEffectGenerator, RepresentableBoundaryOffsetsRemainExact)
{
	World world;
	const int low = (std::numeric_limits<int>::min)(), high = (std::numeric_limits<int>::max)();
	auto target = Target(); auto info = Info(); info.x1 = high - 144; info.y1 = low + 24; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release();
	const Point expected[] = {{777, 888}, {high - 96, low}, {high - 96, low + 24}, {high - 192, low + 48},
		{high - 144, low + 48}, {high - 48, low + 48}, {high, low + 48}, {high - 96, low + 72}, {high - 96, low + 96}};
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(expected[i].x, effects[i]->GetEffectTarget()->GetX()); CHECK_EQ(expected[i].y, effects[i]->GetEffectTarget()->GetY());
	}
}

TEST(CrossZoneEffectGenerator, FirstAcceptedArmKeepsItsOriginalTargetAtExtremeDestinations)
{
	World world;
	rejectBefore = 8; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	info.x1 = (std::numeric_limits<int>::max)(); info.y1 = (std::numeric_limits<int>::min)();
	CHECK(world.generator.Generate(info)); CHECK_EQ(1, effects.size()); CHECK(effects.front()->GetEffectTarget() == target.get());
	target.release(); CHECK_EQ(777, info.pEffectTarget->GetX()); CHECK_EQ(888, info.pEffectTarget->GetY());
	CHECK_EQ(999, info.pEffectTarget->GetZ()); CHECK_EQ(456, info.pEffectTarget->GetID());
}

TEST(CrossZoneEffectGenerator, TargetlessEffectsIgnoreExtremeDestinations)
{
	World world;
	auto info = Info(); info.x1 = (std::numeric_limits<int>::max)(); info.y1 = (std::numeric_limits<int>::min)();
	CHECK(world.generator.Generate(info)); CHECK(attempted == defaultPoints);
	for (const auto& effect : effects) { CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(42, effect->GetActionInfo()); }
}
