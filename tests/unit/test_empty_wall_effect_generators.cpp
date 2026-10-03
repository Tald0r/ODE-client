#include "test_framework.h"
#include "MStopZoneEmptyHorizontalWallEffectGenerator.h"
#include "MStopZoneEmptyVerticalEffectGenerator.h"
#include "MEffect.h"
#include "SkillDef.h"

#include <array>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MEmptyWallEffectSprite sprite;
bool spriteAvailable, acceptQueue;
int requestedSprite, submissions, rejectBefore;
std::vector<bool> acceptance;
std::vector<int> calls, slots, removedTargets;
std::vector<std::unique_ptr<MEffect>> effects;
struct Point
{
	int x, y;
	bool operator==(const Point&) const = default;
};
std::vector<Point> attempted;
const std::vector<Point> horizontalPoints{{8, 8}, {9, 9}, {11, 11}, {12, 12}};
const std::vector<Point> verticalPoints{{12, 8}, {11, 9}, {9, 11}, {8, 12}};
const std::vector<Point> horizontalTargets{{777, 888}, {549, 274}, {645, 322}, {693, 346}};
const std::vector<Point> verticalTargets{{777, 888}, {453, 274}, {357, 322}, {309, 346}};

const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(3); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE frame) { calls.push_back(2); return 7 + frame; },
};
const MEffectTargetHost targetHost{
	.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); },
};
const MEmptyWallEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MEmptyWallEffectSprite& result) {
		calls.push_back(1); requestedSprite = type; result = sprite; return spriteAvailable;
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

void SetHosts(const MEmptyWallEffectHost* service)
{
	MStopZoneEmptyHorizontalWallEffectGenerator::SetHost(service); MStopZoneEmptyVerticalWallEffectGenerator::SetHost(service);
}

struct Pattern
{
	MEffectGenerator* generator;
	bool horizontal;
	const std::vector<Point>& points;
	const std::vector<Point>& targets;
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MEmptyWallEffectHost* previousHorizontal = MStopZoneEmptyHorizontalWallEffectGenerator::SetHost(&host);
	const MEmptyWallEffectHost* previousVertical = MStopZoneEmptyVerticalWallEffectGenerator::SetHost(&host);
	MStopZoneEmptyHorizontalWallEffectGenerator horizontal;
	MStopZoneEmptyVerticalWallEffectGenerator vertical;
	std::array<Pattern, 2> patterns{{{&horizontal, true, horizontalPoints, horizontalTargets},
		{&vertical, false, verticalPoints, verticalTargets}}};
	World()
	{
		frameNow = 100; sprite = {BLT_EFFECT, 12, 3}; spriteAvailable = acceptQueue = true;
		requestedSprite = -1; submissions = rejectBefore = 0;
		effects.clear(); calls.clear(); slots.clear(); removedTargets.clear(); attempted.clear(); acceptance.clear();
	}
	~World()
	{
		effects.clear(); MStopZoneEmptyHorizontalWallEffectGenerator::SetHost(previousHorizontal);
		MStopZoneEmptyVerticalWallEffectGenerator::SetHost(previousVertical);
		MEffectTarget::SetHost(previousTarget); MEffect::SetHost(previousEffect);
	}
};

EFFECTGENERATOR_INFO Info()
{
	EFFECTGENERATOR_INFO info{};
	info.nActionInfo = 42; info.effectSpriteType = 17;
	info.x0 = 261; info.y0 = 130; info.z0 = 17;
	info.x1 = 501; info.y1 = 250; info.z1 = 99;
	info.direction = DIRECTION_RIGHTUP; info.step = 5; info.count = 30; info.linkCount = 5;
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

TEST(EmptyWallEffectGenerators, ConfigureRealEffectsAroundTheDestinationWithoutTheMiddle)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_STOP_ZONE_EMPTY_HORIZONTAL_WALL, world.horizontal.GetID());
	CHECK_EQ(EFFECTGENERATORID_STOP_ZONE_EMPTY_VERTICAL_WALL, world.vertical.GetID());
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); CHECK(pattern.generator->Generate(Info())); CHECK_EQ(4, effects.size()); CHECK(attempted == pattern.points);
		CHECK_EQ(17, requestedSprite); CHECK(calls == std::vector<int>({1, 2, 3, 4, 2, 3, 4, 2, 3, 4, 2, 3, 4}));
		for (size_t i = 0; i < effects.size(); ++i)
		{
			auto& effect = *effects[i]; CHECK_EQ(MEffect::EFFECT_SECTOR, effect.GetEffectType());
			CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame());
			CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight()); CHECK_EQ(pattern.points[i].x * 48, effect.GetPixelX());
			CHECK_EQ(pattern.points[i].y * 24, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ());
			CHECK_EQ(5, effect.GetStepPixel()); CHECK_EQ(DIRECTION_RIGHTUP, effect.GetDirection()); CHECK_EQ(2, effect.GetPower());
			CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame()); CHECK_EQ(42, effect.GetActionInfo());
			CHECK(!effect.IsMulti()); CHECK(effect.GetEffectTarget() == nullptr);
		}
	}
}

TEST(EmptyWallEffectGenerators, AllEightDirectionsKeepTheirDistinctTileAndTargetOrders)
{
	World world;
	const std::vector<Point> horizontal[] = {
		{{10, 8}, {10, 9}, {10, 11}, {10, 12}}, {{8, 8}, {9, 9}, {11, 11}, {12, 12}},
		{{8, 10}, {9, 10}, {11, 10}, {12, 10}}, {{12, 8}, {11, 9}, {9, 11}, {8, 12}},
		{{10, 8}, {10, 9}, {10, 11}, {10, 12}}, {{8, 8}, {9, 9}, {11, 11}, {12, 12}},
		{{8, 10}, {9, 10}, {11, 10}, {12, 10}}, {{8, 12}, {9, 11}, {11, 9}, {12, 8}},
	};
	const std::vector<Point> vertical[] = {
		{{8, 10}, {9, 10}, {11, 10}, {12, 10}}, {{8, 12}, {9, 11}, {11, 9}, {12, 8}},
		{{10, 8}, {10, 9}, {10, 11}, {10, 12}}, {{8, 8}, {9, 9}, {11, 11}, {12, 12}},
		{{8, 10}, {9, 10}, {11, 10}, {12, 10}}, {{12, 8}, {11, 9}, {9, 11}, {8, 12}},
		{{10, 8}, {10, 9}, {10, 11}, {10, 12}}, {{8, 8}, {9, 9}, {11, 11}, {12, 12}},
	};
	const Point horizontalEnds[] = {{501, 346}, {693, 346}, {693, 250}, {309, 346},
		{501, 346}, {693, 346}, {693, 250}, {693, 154}};
	const Point verticalEnds[] = {{693, 250}, {693, 154}, {501, 346}, {693, 346},
		{693, 250}, {309, 346}, {501, 346}, {693, 346}};
	for (const auto& pattern : world.patterns)
	{
		for (BYTE direction = 0; direction < 8; ++direction)
		{
			ClearEffects(); auto target = Target(); auto info = Info(); info.direction = direction; info.pEffectTarget = target.get();
			CHECK(pattern.generator->Generate(info)); target.release(); CHECK(attempted == (pattern.horizontal ? horizontal[direction] : vertical[direction]));
			const Point end = pattern.horizontal ? horizontalEnds[direction] : verticalEnds[direction];
			CHECK_EQ(end.x, effects.back()->GetEffectTarget()->GetX()); CHECK_EQ(end.y, effects.back()->GetEffectTarget()->GetY());
			for (const auto& effect : effects) CHECK_EQ(direction, effect->GetDirection());
		}
	}
}

TEST(EmptyWallEffectGenerators, EveryMineActionUsesSourceTilesButKeepsDestinationPixelTargets)
{
	World world;
	for (const auto& pattern : world.patterns)
	{
		for (int action = MINE_ANKLE_KILLER - 1; action <= MINE_COBRA + 1; ++action)
		{
			ClearEffects(); auto target = Target(); auto info = Info(); info.nActionInfo = static_cast<TYPE_ACTIONINFO>(action); info.pEffectTarget = target.get();
			CHECK(pattern.generator->Generate(info)); target.release();
			if (action >= MINE_ANKLE_KILLER && action <= MINE_COBRA)
				CHECK(attempted == (pattern.horizontal ? std::vector<Point>({{3, 3}, {4, 4}, {6, 6}, {7, 7}}) :
					std::vector<Point>({{7, 3}, {6, 4}, {4, 6}, {3, 7}})));
			else CHECK(attempted == pattern.points);
			for (size_t i = 0; i < effects.size(); ++i)
			{
				CHECK_EQ(action, effects[i]->GetActionInfo()); CHECK_EQ(pattern.targets[i].x, effects[i]->GetEffectTarget()->GetX());
				CHECK_EQ(pattern.targets[i].y, effects[i]->GetEffectTarget()->GetY());
			}
		}
	}
}

TEST(EmptyWallEffectGenerators, ZeroAndSingleStepsConstructNoEffectsAndKeepCallerOwnership)
{
	World world;
	for (const auto& pattern : world.patterns)
	{
		for (const BYTE step : {static_cast<BYTE>(0), static_cast<BYTE>(1)})
		{
			ClearEffects(); auto target = Target(); auto info = Info(); info.step = step; info.pEffectTarget = target.get();
			CHECK(!pattern.generator->Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions);
			CHECK(calls == std::vector<int>({1})); CHECK_EQ(777, target->GetX());
		}
	}
}

TEST(EmptyWallEffectGenerators, EvenStepCountsSkipTheUpperMiddleIteration)
{
	World world;
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); auto info = Info(); info.step = 4;
		CHECK(pattern.generator->Generate(info)); CHECK_EQ(3, effects.size());
		CHECK(attempted == (pattern.horizontal ? std::vector<Point>({{8, 8}, {9, 9}, {11, 11}}) :
			std::vector<Point>({{12, 8}, {11, 9}, {9, 11}})));
		ClearEffects(); info.step = 2;
		CHECK(pattern.generator->Generate(info)); CHECK_EQ(1, effects.size());
		CHECK(attempted == (pattern.horizontal ? std::vector<Point>({{9, 9}}) : std::vector<Point>({{11, 9}})));
	}
}

TEST(EmptyWallEffectGenerators, MaximumStepCountKeepsTheGapAndFullTargetDisplacement)
{
	World world;
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); auto target = Target(); auto info = Info(); info.step = 255; info.pEffectTarget = target.get();
		CHECK(pattern.generator->Generate(info)); target.release(); CHECK_EQ(254, effects.size());
		CHECK((attempted.front() == (pattern.horizontal ? Point{65419, 65419} : Point{137, 65419})));
		CHECK((attempted.back() == (pattern.horizontal ? Point{137, 137} : Point{65419, 137})));
		for (const auto point : attempted) CHECK((point != Point{10, 10}));
		CHECK_EQ(pattern.horizontal ? 12693 : -11691, effects.back()->GetEffectTarget()->GetX());
		CHECK_EQ(6346, effects.back()->GetEffectTarget()->GetY()); CHECK_EQ(255, effects.back()->GetStepPixel());
	}
}

TEST(EmptyWallEffectGenerators, NegativeMineOriginsTruncateAndWrapAtSubmission)
{
	World world;
	auto info = Info(); info.nActionInfo = MINE_ANKLE_KILLER; info.x0 = info.y0 = -1;
	CHECK(world.horizontal.Generate(info));
	CHECK(attempted == std::vector<Point>({{65534, 65534}, {65535, 65535}, {1, 1}, {2, 2}}));
	ClearEffects(); CHECK(world.vertical.Generate(info));
	CHECK(attempted == std::vector<Point>({{2, 65534}, {1, 65535}, {65535, 1}, {65534, 2}}));
}

TEST(EmptyWallEffectGenerators, PowerDoesNotChangePatternLengthOrPlacement)
{
	World world;
	for (const auto& pattern : world.patterns)
	{
		for (const BYTE power : {static_cast<BYTE>(0), static_cast<BYTE>(255)})
		{
			ClearEffects(); auto info = Info(); info.power = power;
			CHECK(pattern.generator->Generate(info)); CHECK(attempted == pattern.points);
			for (const auto& effect : effects) CHECK_EQ(power, effect->GetPower());
		}
	}
}

TEST(EmptyWallEffectGenerators, LaterTargetsKeepPhaseAndAdvanceAcrossTheMissingMiddle)
{
	World world;
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK(pattern.generator->Generate(info)); target.release();
		for (size_t i = 0; i < effects.size(); ++i)
		{
			const auto* linked = effects[i]->GetEffectTarget(); CHECK(linked != nullptr); if (!linked) continue;
			CHECK_EQ(i == 0, linked == info.pEffectTarget); CHECK_EQ(1, linked->GetCurrentPhase()); CHECK_EQ(3, linked->GetMaxPhase());
			CHECK_EQ(31, linked->GetDelayFrame()); CHECK_EQ(73, linked->GetEffectID());
			CHECK_EQ(i == 0, linked->IsExistResult()); CHECK_EQ(i == 0, linked->IsResultTime());
			CHECK_EQ(i == 0 ? 789 : OBJECTID_NULL, linked->GetServerID());
			CHECK_EQ(pattern.targets[i].x, linked->GetX()); CHECK_EQ(pattern.targets[i].y, linked->GetY());
			CHECK_EQ(i == 0 ? 999 : 17, linked->GetZ()); CHECK_EQ(i == 0 ? 456 : 123, linked->GetID());
			for (size_t j = 0; j < i; ++j) CHECK(linked != effects[j]->GetEffectTarget());
		}
	}
}

TEST(EmptyWallEffectGenerators, EveryAcceptanceMaskTransfersTheOriginalToTheFirstAcceptedEffect)
{
	World world;
	for (const auto& pattern : world.patterns)
	{
		for (int mask = 0; mask < 16; ++mask)
		{
			ClearEffects(); acceptance.assign(4, false);
			for (int slot = 0; slot < 4; ++slot) acceptance[slot] = (mask & (1 << slot)) != 0;
			auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
			CHECK_EQ(mask != 0, pattern.generator->Generate(info)); CHECK_EQ(4, submissions);
			bool originalOwned = false;
			for (size_t i = 0; i < effects.size(); ++i)
			{
				const auto* linked = effects[i]->GetEffectTarget(); CHECK(linked != nullptr); if (!linked) continue;
				originalOwned |= linked == target.get(); CHECK_EQ(i == 0, linked == target.get());
				CHECK_EQ(i == 0 ? 777 : pattern.targets[slots[i]].x, linked->GetX());
				CHECK_EQ(i == 0 ? 888 : pattern.targets[slots[i]].y, linked->GetY());
			}
			CHECK_EQ(mask != 0, originalOwned); if (originalOwned) target.release();
		}
	}
}

TEST(EmptyWallEffectGenerators, TargetlessMasksReportAnyAcceptedEffectAndLinkEveryAction)
{
	World world;
	for (const auto& pattern : world.patterns)
	{
		for (int mask = 0; mask < 16; ++mask)
		{
			ClearEffects(); acceptance.assign(4, false);
			for (int slot = 0; slot < 4; ++slot) acceptance[slot] = (mask & (1 << slot)) != 0;
			CHECK_EQ(mask != 0, pattern.generator->Generate(Info())); CHECK_EQ(4, submissions);
			for (const auto& effect : effects) { CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(42, effect->GetActionInfo()); }
		}
	}
}

TEST(EmptyWallEffectGenerators, MissingMetadataRejectsBeforeConstruction)
{
	World world;
	const MEmptyWallEffectHost empty{};
	for (const auto* service : {static_cast<const MEmptyWallEffectHost*>(nullptr), &empty, &host})
	{
		SetHosts(service); spriteAvailable = false;
		for (const auto& pattern : world.patterns)
		{
			ClearEffects(); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
			CHECK(!pattern.generator->Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions);
			CHECK(calls == (service == &host ? std::vector<int>({1}) : std::vector<int>{})); CHECK_EQ(777, target->GetX());
		}
	}
}

TEST(EmptyWallEffectGenerators, MissingQueuesReleaseAllUnlinkedEffects)
{
	World world;
	const MEmptyWallEffectHost metadataOnly{.Sprite = host.Sprite}; SetHosts(&metadataOnly);
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK(!pattern.generator->Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions); CHECK_EQ(777, target->GetX());
		CHECK(calls == std::vector<int>({1, 2, 3, 2, 3, 2, 3, 2, 3}));
	}
}

TEST(EmptyWallEffectGenerators, MetadataRefreshesAndFrameCountsRetainByteNarrowing)
{
	World world;
	struct Case { int supplied, expected; };
	for (const auto& pattern : world.patterns)
	{
		for (const Case c : {Case{-1, 255}, {0, 0}, {256, 0}, {258, 2}})
		{
			ClearEffects(); sprite = {BLT_NORMAL, 23, c.supplied}; CHECK(pattern.generator->Generate(Info()));
			for (const auto& effect : effects)
			{
				CHECK_EQ(BLT_NORMAL, effect->GetBltType()); CHECK_EQ(23, effect->GetFrameID());
				CHECK_EQ(c.expected, effect->GetMaxFrame()); CHECK_EQ(0, effect->GetFrame());
			}
		}
	}
}

TEST(EmptyWallEffectGenerators, HostInstallersRestoreIndependently)
{
	World world;
	CHECK(MStopZoneEmptyHorizontalWallEffectGenerator::SetHost(nullptr) == &host);
	CHECK(!world.horizontal.Generate(Info())); CHECK(world.vertical.Generate(Info())); CHECK_EQ(4, effects.size());
	ClearEffects(); CHECK(MStopZoneEmptyHorizontalWallEffectGenerator::SetHost(&host) == nullptr);
	CHECK(MStopZoneEmptyVerticalWallEffectGenerator::SetHost(nullptr) == &host);
	CHECK(!world.vertical.Generate(Info())); CHECK(world.horizontal.Generate(Info())); CHECK_EQ(4, effects.size());
}

TEST(EmptyWallEffectGenerators, SpriteCallbacksCanReplaceTheSubmissionService)
{
	World world;
	const MEmptyWallEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MEmptyWallEffectSprite& result) {
			result = {BLT_NORMAL, 20, 4}; SetHosts(&host); return true;
		},
	};
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); SetHosts(&changing); CHECK(pattern.generator->Generate(Info())); CHECK_EQ(4, effects.size());
		CHECK_EQ(2, calls.front()); CHECK_EQ(20, effects.back()->GetFrameID()); CHECK_EQ(4, effects.back()->GetMaxFrame());
	}
}

TEST(EmptyWallEffectGenerators, SpriteCallbacksCanRemoveQueuesBeforeConstruction)
{
	World world;
	const MEmptyWallEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MEmptyWallEffectSprite& result) { result = sprite; SetHosts(nullptr); return true; },
		.Queue = host.Queue,
	};
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); SetHosts(&changing); CHECK(!pattern.generator->Generate(Info())); CHECK(effects.empty());
		CHECK(calls == std::vector<int>({2, 3, 2, 3, 2, 3, 2, 3})); CHECK_EQ(0, submissions);
	}
}

TEST(EmptyWallEffectGenerators, QueueReplacementRetainsTheInitialSpriteSnapshot)
{
	World world;
	const MEmptyWallEffectHost changing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { sprite = {BLT_NORMAL, 23, 8}; SetHosts(&host); return host.Queue(std::move(effect)); },
	};
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); sprite = {BLT_EFFECT, 12, 3}; SetHosts(&changing); CHECK(pattern.generator->Generate(Info())); CHECK_EQ(4, effects.size());
		for (const auto& effect : effects) { CHECK_EQ(12, effect->GetFrameID()); CHECK_EQ(3, effect->GetMaxFrame()); }
	}
}

TEST(EmptyWallEffectGenerators, QueueRemovalAfterTheFirstAcceptanceKeepsItsOriginalTarget)
{
	World world;
	const MEmptyWallEffectHost changing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { SetHosts(nullptr); return host.Queue(std::move(effect)); },
	};
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); SetHosts(&changing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK(pattern.generator->Generate(info)); CHECK_EQ(1, effects.size()); CHECK_EQ(1, submissions);
		CHECK(effects.front()->GetEffectTarget() == target.get()); target.release(); CHECK_EQ(777, info.pEffectTarget->GetX());
	}
}

TEST(EmptyWallEffectGenerators, RejectingQueuesDestroyTheirAttachedMarkers)
{
	World world;
	const MEmptyWallEffectHost rejecting{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { effect->SetLink(43, Target(74).release()); return false; },
	};
	SetHosts(&rejecting);
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); removedTargets.clear(); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK(!pattern.generator->Generate(info)); CHECK(removedTargets == std::vector<int>(4, 74));
		target.reset(); CHECK_EQ(5, removedTargets.size()); CHECK_EQ(73, removedTargets.back());
	}
}

TEST(EmptyWallEffectGenerators, FirstQueueExceptionsLeaveTheCallerTargetUntouched)
{
	World world;
	const MEmptyWallEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool {
			effect->SetLink(43, Target(74).release()); throw std::runtime_error("Queue failure");
		},
	};
	SetHosts(&throwing);
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); removedTargets.clear(); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		bool threw = false;
		try { pattern.generator->Generate(info); } catch (const std::runtime_error&) { threw = true; }
		CHECK(threw); CHECK(removedTargets == std::vector<int>({74})); CHECK_EQ(777, target->GetX());
		target.reset(); CHECK(removedTargets == std::vector<int>({74, 73}));
	}
}

TEST(EmptyWallEffectGenerators, LaterQueueExceptionsPreserveTransferredOwnership)
{
	World world;
	const MEmptyWallEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool {
			if (effects.empty()) return host.Queue(std::move(effect));
			effect->SetLink(43, Target(74).release()); throw std::runtime_error("Later queue failure");
		},
	};
	SetHosts(&throwing);
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); removedTargets.clear(); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		bool threw = false;
		try { pattern.generator->Generate(info); } catch (const std::runtime_error&) { threw = true; }
		CHECK(threw); CHECK_EQ(1, effects.size()); CHECK(effects.front()->GetEffectTarget() == target.get());
		target.release(); CHECK(removedTargets == std::vector<int>({74})); CHECK_EQ(777, info.pEffectTarget->GetX());
		ClearEffects(); CHECK(removedTargets == std::vector<int>({74, 73}));
	}
}

TEST(EmptyWallEffectGenerators, NeitherPatternConsumesRandomness)
{
	World world;
	for (const auto& pattern : world.patterns)
	{
		for (const bool accepted : {false, true})
		{
			ClearEffects(); acceptQueue = accepted;
			std::srand(719); const int next = std::rand(); std::srand(719);
			CHECK_EQ(accepted, pattern.generator->Generate(Info())); CHECK_EQ(next, std::rand());
			for (const auto& effect : effects) CHECK_EQ(0, effect->GetFrame());
		}
	}
}

TEST(EmptyWallEffectGenerators, RealEffectsAnimateAndRefreshLightWithoutMoving)
{
	World world;
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); frameNow = 100; sprite.bltType = BLT_NORMAL; CHECK(pattern.generator->Generate(Info()));
		for (const auto& effect : effects)
		{
			const int x = effect->GetPixelX(), y = effect->GetPixelY();
			frameNow = 100; CHECK(effect->Update()); CHECK_EQ(1, effect->GetFrame()); CHECK_EQ(8, effect->GetLight());
			CHECK_EQ(x, effect->GetPixelX()); CHECK_EQ(y, effect->GetPixelY()); CHECK_EQ(17, effect->GetPixelZ());
			frameNow = 130; CHECK(!effect->Update()); CHECK_EQ(2, effect->GetFrame()); CHECK_EQ(9, effect->GetLight());
		}
	}
}

TEST(EmptyWallEffectGenerators, CountsRemainFiniteAndLinkTimingIsIndependent)
{
	World world;
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); frameNow = 100; auto info = Info(); info.count = 65535; info.linkCount = MAX_LINKCOUNT;
		CHECK(pattern.generator->Generate(info)); CHECK_EQ(65634, effects.back()->GetEndFrame()); CHECK_EQ(65634, effects.back()->GetEndLinkFrame());
		ClearEffects(); info.count = 10; info.linkCount = 20;
		CHECK(pattern.generator->Generate(info)); CHECK_EQ(109, effects.back()->GetEndFrame()); CHECK_EQ(119, effects.back()->GetEndLinkFrame());
		ClearEffects(); frameNow = (std::numeric_limits<DWORD>::max)() - 1; info.count = 4;
		CHECK(pattern.generator->Generate(info)); CHECK_EQ(1, effects.back()->GetEndFrame()); CHECK(!effects.back()->Update());
		frameNow = 0; CHECK(effects.back()->Update()); CHECK_EQ(2, effects.back()->GetFrame());
	}
}

TEST(EmptyWallEffectGenerators, EachEffectReadsTheCurrentClockAfterEarlierSubmissions)
{
	World world;
	const MEmptyWallEffectHost advancing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { const bool accepted = host.Queue(std::move(effect)); ++frameNow; return accepted; },
	};
	SetHosts(&advancing);
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); frameNow = 100; CHECK(pattern.generator->Generate(Info()));
		for (size_t i = 0; i < effects.size(); ++i)
		{
			CHECK_EQ(129 + i, effects[i]->GetEndFrame()); CHECK_EQ(104 + i, effects[i]->GetEndLinkFrame());
		}
	}
}

TEST(EmptyWallEffectGenerators, MissingBaseServicesRetainCountsAndInactiveFallback)
{
	World world;
	MEffect::SetHost(nullptr);
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); CHECK(pattern.generator->Generate(Info())); CHECK_EQ(29, effects.front()->GetEndFrame());
		CHECK_EQ(4, effects.front()->GetEndLinkFrame()); CHECK_EQ(0, effects.front()->GetLight()); CHECK(!effects.front()->Update());
	}
}

TEST(EmptyWallEffectGenerators, HorizontalRejectsTheFirstDirectionPastItsTable)
{
	World world;
	acceptQueue = false; auto target = Target(); auto info = Info(); info.direction = 8; info.step = 2; info.pEffectTarget = target.get();
	CHECK(!world.horizontal.Generate(info)); CHECK_EQ(0, submissions); CHECK(calls == std::vector<int>({1}));
	CHECK_EQ(777, target->GetX()); CHECK_EQ(888, target->GetY()); CHECK(effects.empty());
}

TEST(EmptyWallEffectGenerators, VerticalRejectsTheFirstDirectionPastItsTable)
{
	World world;
	acceptQueue = false; auto target = Target(); auto info = Info(); info.direction = 8; info.step = 2; info.pEffectTarget = target.get();
	CHECK(!world.vertical.Generate(info)); CHECK_EQ(0, submissions); CHECK(calls == std::vector<int>({1}));
	CHECK_EQ(777, target->GetX()); CHECK_EQ(888, target->GetY()); CHECK(effects.empty());
}

TEST(EmptyWallEffectGenerators, EveryInvalidDirectionRejectsAtAllPatternLengthBoundaries)
{
	World world;
	for (const auto& pattern : world.patterns)
	{
		for (int direction = 8; direction <= 255; ++direction)
		{
			for (const BYTE step : {static_cast<BYTE>(0), static_cast<BYTE>(1), static_cast<BYTE>(2), static_cast<BYTE>(255)})
			{
				ClearEffects(); auto target = Target(); auto info = Info(); info.direction = static_cast<BYTE>(direction);
				info.step = step; info.pEffectTarget = target.get();
				CHECK(!pattern.generator->Generate(info)); CHECK_EQ(0, submissions); CHECK(effects.empty());
				CHECK(calls == std::vector<int>({1})); CHECK_EQ(777, target->GetX()); CHECK_EQ(888, target->GetY());
			}
		}
	}
}
