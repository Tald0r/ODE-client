#include "test_framework.h"
#include "MStopZoneXEffectGenerator.h"
#include "MStopZoneRhombusEffectGenerator.h"
#include "MEffect.h"

#include <array>
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
const std::vector<Point> xPoints{{5, 5}, {4, 6}, {6, 4}, {4, 4}, {6, 6}};
const std::vector<Point> rhombusPoints{{4, 5}, {6, 5}, {5, 4}, {5, 6}};

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

void SetHosts(const MFixedZoneEffectHost* service)
{
	MStopZoneXEffectGenerator::SetHost(service); MStopZoneRhombusEffectGenerator::SetHost(service);
}

struct Pattern
{
	MEffectGenerator* generator;
	int count;
	const std::vector<Point>& points;
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MFixedZoneEffectHost* previousX = MStopZoneXEffectGenerator::SetHost(&host);
	const MFixedZoneEffectHost* previousRhombus = MStopZoneRhombusEffectGenerator::SetHost(&host);
	MStopZoneXEffectGenerator x;
	MStopZoneRhombusEffectGenerator rhombus;
	std::array<Pattern, 2> patterns{{{&x, 5, xPoints}, {&rhombus, 4, rhombusPoints}}};
	World()
	{
		frameNow = 100; sprite = {BLT_EFFECT, 12, 3}; spriteAvailable = true;
		requestedSprite = -1; submissions = 0; acceptanceMask = 31;
		effects.clear(); calls.clear(); slots.clear(); removedTargets.clear(); attempted.clear();
	}
	~World()
	{
		effects.clear(); MStopZoneXEffectGenerator::SetHost(previousX); MStopZoneRhombusEffectGenerator::SetHost(previousRhombus);
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

TEST(FixedZoneEffectPatterns, ConfigureRealEffectsInTheOriginalPatternOrder)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_STOP_ZONE_X, world.x.GetID()); CHECK_EQ(EFFECTGENERATORID_STOP_ZONE_RHOMBUS, world.rhombus.GetID());
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); CHECK(pattern.generator->Generate(Info())); CHECK_EQ(pattern.count, effects.size());
		CHECK(attempted == pattern.points); CHECK_EQ(17, requestedSprite);
		std::vector<int> expectedCalls{1};
		for (int i = 0; i < pattern.count; ++i) expectedCalls.insert(expectedCalls.end(), {2, 3, 4});
		CHECK(calls == expectedCalls);
		for (size_t i = 0; i < effects.size(); ++i)
		{
			auto& effect = *effects[i]; CHECK_EQ(MEffect::EFFECT_SECTOR, effect.GetEffectType());
			CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame());
			CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight()); CHECK_EQ(pattern.points[i].x * 48, effect.GetPixelX());
			CHECK_EQ(pattern.points[i].y * 24, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ());
			CHECK_EQ(9, effect.GetStepPixel()); CHECK_EQ(DIRECTION_RIGHTUP, effect.GetDirection()); CHECK_EQ(2, effect.GetPower());
			CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame()); CHECK_EQ(42, effect.GetActionInfo());
			CHECK(!effect.IsMulti()); CHECK(effect.GetEffectTarget() == nullptr);
		}
	}
}

TEST(FixedZoneEffectPatterns, PowerDoesNotChangeEitherFixedPattern)
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

TEST(FixedZoneEffectPatterns, LowerBoundaryOffsetsWrapAsUnsignedSectors)
{
	World world;
	for (const int source : {0, -1})
	{
		auto info = Info(); info.x0 = info.y0 = source;
		ClearEffects(); CHECK(world.x.Generate(info));
		CHECK(attempted == std::vector<Point>({{0, 0}, {65535, 1}, {1, 65535}, {65535, 65535}, {1, 1}}));
		CHECK_EQ(3145680, effects[1]->GetPixelX()); CHECK_EQ(1572840, effects[2]->GetPixelY());
		ClearEffects(); CHECK(world.rhombus.Generate(info));
		CHECK(attempted == std::vector<Point>({{65535, 0}, {1, 0}, {0, 65535}, {0, 1}}));
	}
}

TEST(FixedZoneEffectPatterns, MaximumAndNegativeTileOriginsWrapTheirNeighbours)
{
	World world;
	for (const Point source : {Point{3145680, 1572840}, {-48, -24}})
	{
		auto info = Info(); info.x0 = source.x; info.y0 = source.y;
		ClearEffects(); CHECK(world.x.Generate(info));
		CHECK(attempted == std::vector<Point>({{65535, 65535}, {65534, 0}, {0, 65534}, {65534, 65534}, {0, 0}}));
		ClearEffects(); CHECK(world.rhombus.Generate(info));
		CHECK(attempted == std::vector<Point>({{65534, 65535}, {0, 65535}, {65535, 65534}, {65535, 0}}));
	}
}

TEST(FixedZoneEffectPatterns, SourceTilesNarrowBeforeApplyingPatternOffsets)
{
	World world;
	auto info = Info(); info.x0 = 3145728 + 261; info.y0 = 1572864 + 130;
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); CHECK(pattern.generator->Generate(info)); CHECK(attempted == pattern.points);
	}
}

TEST(FixedZoneEffectPatterns, LaterTargetsKeepCoordinatesAndPhaseButDropResultOwnership)
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
			CHECK_EQ(777, linked->GetX()); CHECK_EQ(888, linked->GetY()); CHECK_EQ(999, linked->GetZ()); CHECK_EQ(456, linked->GetID());
			for (size_t j = 0; j < i; ++j) CHECK(linked != effects[j]->GetEffectTarget());
		}
	}
}

TEST(FixedZoneEffectPatterns, EveryAcceptanceMaskReportsOnlyOriginalTargetTransfer)
{
	World world;
	for (const auto& pattern : world.patterns)
	{
		for (int mask = 0; mask < (1 << pattern.count); ++mask)
		{
			ClearEffects(); acceptanceMask = mask; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
			CHECK_EQ((mask & 1) != 0, pattern.generator->Generate(info)); CHECK_EQ(pattern.count, submissions);
			bool originalOwned = false;
			for (size_t i = 0; i < effects.size(); ++i)
			{
				const auto* linked = effects[i]->GetEffectTarget(); CHECK(linked != nullptr); if (!linked) continue;
				originalOwned |= linked == target.get(); CHECK_EQ(slots[i] == 0, linked == target.get());
				CHECK_EQ(777, linked->GetX()); CHECK_EQ(888, linked->GetY());
			}
			CHECK_EQ((mask & 1) != 0, originalOwned); if (originalOwned) target.release();
		}
	}
}

TEST(FixedZoneEffectPatterns, TargetlessAcceptanceStillReportsSlotZeroAndLinksEveryAcceptedAction)
{
	World world;
	for (const auto& pattern : world.patterns)
	{
		for (int mask = 0; mask < (1 << pattern.count); ++mask)
		{
			ClearEffects(); acceptanceMask = mask;
			CHECK_EQ((mask & 1) != 0, pattern.generator->Generate(Info())); CHECK_EQ(pattern.count, submissions);
			for (const auto& effect : effects) { CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(42, effect->GetActionInfo()); }
		}
	}
}

TEST(FixedZoneEffectPatterns, LaterCopiesSurviveCallerCleanupWhenSlotZeroIsRejected)
{
	World world;
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); acceptanceMask = 30; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK(!pattern.generator->Generate(info)); CHECK_EQ(pattern.count - 1, effects.size()); target.reset();
		for (const auto& effect : effects)
		{
			CHECK_EQ(777, effect->GetEffectTarget()->GetX()); CHECK_EQ(31, effect->GetEffectTarget()->GetDelayFrame());
			CHECK(!effect->GetEffectTarget()->IsExistResult());
		}
	}
}

TEST(FixedZoneEffectPatterns, DestinationAndPreviousEffectDoNotAlterPlacementOrTargets)
{
	World world;
	MEffect previous(BLT_NORMAL); previous.SetFrameID(77, 3);
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); info.pPreviousEffect = &previous;
		info.x1 = (std::numeric_limits<int>::max)(); info.y1 = (std::numeric_limits<int>::min)(); info.z1 = info.x1;
		CHECK(pattern.generator->Generate(info)); target.release(); CHECK(attempted == pattern.points);
		for (const auto& effect : effects) { CHECK_EQ(12, effect->GetFrameID()); CHECK_EQ(777, effect->GetEffectTarget()->GetX()); }
	}
}

TEST(FixedZoneEffectPatterns, MissingMetadataRejectsBeforeConstruction)
{
	World world;
	const MFixedZoneEffectHost empty{};
	for (const auto* service : {static_cast<const MFixedZoneEffectHost*>(nullptr), &empty, &host})
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

TEST(FixedZoneEffectPatterns, MissingQueuesReleaseAllUnlinkedEffects)
{
	World world;
	const MFixedZoneEffectHost metadataOnly{.Sprite = host.Sprite}; SetHosts(&metadataOnly);
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK(!pattern.generator->Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions); CHECK_EQ(777, target->GetX());
		CHECK_EQ(1 + 2 * pattern.count, calls.size()); CHECK_EQ(1, calls.front());
	}
}

TEST(FixedZoneEffectPatterns, MetadataRefreshesAndFrameCountsRetainByteNarrowing)
{
	World world;
	struct Case { int supplied, expected; };
	for (const auto& pattern : world.patterns)
	{
		for (const Case c : {Case{-1, 255}, {0, 0}, {256, 0}, {258, 2}})
		{
			ClearEffects(); sprite = {BLT_NORMAL, 23, c.supplied};
			CHECK(pattern.generator->Generate(Info()));
			for (const auto& effect : effects)
			{
				CHECK_EQ(BLT_NORMAL, effect->GetBltType()); CHECK_EQ(23, effect->GetFrameID());
				CHECK_EQ(c.expected, effect->GetMaxFrame()); CHECK_EQ(0, effect->GetFrame());
			}
		}
	}
}

TEST(FixedZoneEffectPatterns, HostInstallersRestoreIndependently)
{
	World world;
	CHECK(MStopZoneXEffectGenerator::SetHost(nullptr) == &host);
	CHECK(!world.x.Generate(Info())); CHECK(world.rhombus.Generate(Info())); CHECK_EQ(4, effects.size());
	ClearEffects(); CHECK(MStopZoneXEffectGenerator::SetHost(&host) == nullptr);
	CHECK(MStopZoneRhombusEffectGenerator::SetHost(nullptr) == &host);
	CHECK(!world.rhombus.Generate(Info())); CHECK(world.x.Generate(Info())); CHECK_EQ(5, effects.size());
}

TEST(FixedZoneEffectPatterns, SpriteCallbacksCanReplaceTheSubmissionService)
{
	World world;
	const MFixedZoneEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MFixedZoneEffectSprite& result) {
			result = {BLT_NORMAL, 20, 4}; SetHosts(&host); return true;
		},
	};
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); SetHosts(&changing); CHECK(pattern.generator->Generate(Info())); CHECK_EQ(pattern.count, effects.size());
		CHECK_EQ(2, calls.front()); CHECK_EQ(20, effects.back()->GetFrameID()); CHECK_EQ(4, effects.back()->GetMaxFrame());
	}
}

TEST(FixedZoneEffectPatterns, SpriteCallbacksCanRemoveQueuesBeforeConstruction)
{
	World world;
	const MFixedZoneEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MFixedZoneEffectSprite& result) { result = sprite; SetHosts(nullptr); return true; },
		.Queue = host.Queue,
	};
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); SetHosts(&changing); CHECK(!pattern.generator->Generate(Info())); CHECK(effects.empty());
		CHECK_EQ(2 * pattern.count, calls.size()); CHECK_EQ(2, calls.front()); CHECK_EQ(0, submissions);
	}
}

TEST(FixedZoneEffectPatterns, QueueReplacementRetainsTheInitialSpriteSnapshot)
{
	World world;
	const MFixedZoneEffectHost changing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { sprite = {BLT_NORMAL, 23, 8}; SetHosts(&host); return host.Queue(std::move(effect)); },
	};
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); sprite = {BLT_EFFECT, 12, 3}; SetHosts(&changing);
		CHECK(pattern.generator->Generate(Info())); CHECK_EQ(pattern.count, effects.size());
		for (const auto& effect : effects) { CHECK_EQ(12, effect->GetFrameID()); CHECK_EQ(3, effect->GetMaxFrame()); }
	}
}

TEST(FixedZoneEffectPatterns, QueueRemovalAfterSlotZeroKeepsItsOriginalTarget)
{
	World world;
	const MFixedZoneEffectHost changing{
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

TEST(FixedZoneEffectPatterns, RejectingQueuesDestroyTheirAttachedMarkers)
{
	World world;
	const MFixedZoneEffectHost rejecting{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { effect->SetLink(43, Target(74).release()); return false; },
	};
	SetHosts(&rejecting);
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); removedTargets.clear(); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK(!pattern.generator->Generate(info)); CHECK(removedTargets == std::vector<int>(pattern.count, 74));
		target.reset(); CHECK_EQ(pattern.count + 1, removedTargets.size()); CHECK_EQ(73, removedTargets.back());
	}
}

TEST(FixedZoneEffectPatterns, FirstQueueExceptionsLeaveTheCallerTargetUntouched)
{
	World world;
	const MFixedZoneEffectHost throwing{
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

TEST(FixedZoneEffectPatterns, LaterQueueExceptionsPreserveTransferredOwnership)
{
	World world;
	const MFixedZoneEffectHost throwing{
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

TEST(FixedZoneEffectPatterns, NeitherPatternConsumesRandomness)
{
	World world;
	for (const auto& pattern : world.patterns)
	{
		for (const int mask : {0, 31})
		{
			ClearEffects(); acceptanceMask = mask;
			std::srand(719); const int next = std::rand(); std::srand(719);
			CHECK_EQ(mask != 0, pattern.generator->Generate(Info())); CHECK_EQ(next, std::rand());
			for (const auto& effect : effects) CHECK_EQ(0, effect->GetFrame());
		}
	}
}

TEST(FixedZoneEffectPatterns, RealEffectsAnimateAndRefreshLightWithoutMoving)
{
	World world;
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); frameNow = 100; sprite.bltType = BLT_NORMAL;
		CHECK(pattern.generator->Generate(Info()));
		for (const auto& effect : effects)
		{
			const int x = effect->GetPixelX(), y = effect->GetPixelY();
			frameNow = 100; CHECK(effect->Update()); CHECK_EQ(1, effect->GetFrame()); CHECK_EQ(8, effect->GetLight());
			CHECK_EQ(x, effect->GetPixelX()); CHECK_EQ(y, effect->GetPixelY()); CHECK_EQ(17, effect->GetPixelZ());
			frameNow = 130; CHECK(!effect->Update()); CHECK_EQ(2, effect->GetFrame()); CHECK_EQ(9, effect->GetLight());
		}
	}
}

TEST(FixedZoneEffectPatterns, CountsRemainFiniteAndLinkTimingIsIndependent)
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

TEST(FixedZoneEffectPatterns, MissingBaseServicesRetainCountsAndInactiveFallback)
{
	World world;
	MEffect::SetHost(nullptr);
	for (const auto& pattern : world.patterns)
	{
		ClearEffects(); CHECK(pattern.generator->Generate(Info())); CHECK_EQ(29, effects.front()->GetEndFrame());
		CHECK_EQ(4, effects.front()->GetEndLinkFrame()); CHECK_EQ(0, effects.front()->GetLight()); CHECK(!effects.front()->Update());
	}
}
