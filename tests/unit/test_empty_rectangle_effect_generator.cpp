#include "test_framework.h"
#include "MStopZoneEmptyRectEffectGenerator.h"
#include "MEffect.h"

#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MEmptyRectEffectSprite sprite;
MEmptyRectEffectBounds zoneBounds;
bool spriteAvailable, boundsAvailable, acceptQueue, validLargeRectangle;
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
const std::vector<Point> defaultPoints{{4, 4}, {5, 4}, {6, 4}, {4, 5}, {6, 5}, {4, 6}, {5, 6}, {6, 6}};
const Point linkedPoints[] = {{777, 888}, {900, 776}, {948, 776}, {852, 800}, {948, 800}, {852, 824}, {900, 824}, {948, 824}};
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(3); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE frame) { calls.push_back(2); return 7 + frame; },
};
const MEffectTargetHost targetHost{
	.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); },
};
const MEmptyRectEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MEmptyRectEffectSprite& result) {
		calls.push_back(1); requestedSprite = type; result = sprite; return spriteAvailable;
	},
	.Bounds = [](MEmptyRectEffectBounds& result) { calls.push_back(5); result = zoneBounds; return boundsAvailable; },
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
	const MEmptyRectEffectHost* previousRectangle = MStopZoneEmptyRectEffectGenerator::SetHost(&host);
	MStopZoneEmptyRectEffectGenerator generator;
	World()
	{
		frameNow = 100; sprite = {BLT_EFFECT, 12, 3}; zoneBounds = {11, 11};
		spriteAvailable = boundsAvailable = acceptQueue = validLargeRectangle = true;
		requestedSprite = -1; submissions = rejectBefore = 0;
		effects.clear(); calls.clear(); slots.clear(); removedTargets.clear(); acceptance.clear(); attempted.clear();
	}
	~World()
	{
		effects.clear(); MStopZoneEmptyRectEffectGenerator::SetHost(previousRectangle);
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
	info.power = 1; info.creatureID = 123;
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

TEST(EmptyRectangleEffectGenerator, ReadsBoundsBeforeConstructingRowMajorEffects)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_STOP_ZONE_EMPTY_RECT, world.generator.GetID());
	CHECK(world.generator.Generate(Info())); CHECK_EQ(8, effects.size()); CHECK(attempted == defaultPoints);
	CHECK_EQ(17, requestedSprite);
	std::vector<int> expectedCalls{1, 5}; for (int i = 0; i < 8; ++i) expectedCalls.insert(expectedCalls.end(), {2, 3, 4});
	CHECK(calls == expectedCalls);
	for (size_t i = 0; i < effects.size(); ++i)
	{
		auto& effect = *effects[i]; CHECK_EQ(MEffect::EFFECT_SECTOR, effect.GetEffectType());
		CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame());
		CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight()); CHECK_EQ(defaultPoints[i].x * 48, effect.GetPixelX());
		CHECK_EQ(defaultPoints[i].y * 24, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ());
		CHECK_EQ(9, effect.GetStepPixel()); CHECK_EQ(DIRECTION_RIGHTUP, effect.GetDirection()); CHECK_EQ(1, effect.GetPower());
		CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame()); CHECK_EQ(42, effect.GetActionInfo());
		CHECK(!effect.IsMulti()); CHECK(effect.GetEffectTarget() == nullptr);
	}
}

TEST(EmptyRectangleEffectGenerator, LargerSquaresKeepEveryInteriorTileExceptTheCenter)
{
	World world;
	auto info = Info(); info.power = 2;
	CHECK(world.generator.Generate(info)); CHECK_EQ(24, effects.size());
	CHECK(attempted == std::vector<Point>({{3, 3}, {4, 3}, {5, 3}, {6, 3}, {7, 3},
		{3, 4}, {4, 4}, {5, 4}, {6, 4}, {7, 4}, {3, 5}, {4, 5}, {6, 5}, {7, 5},
		{3, 6}, {4, 6}, {5, 6}, {6, 6}, {7, 6}, {3, 7}, {4, 7}, {5, 7}, {6, 7}, {7, 7}}));
	for (const auto& effect : effects) CHECK_EQ(2, effect->GetPower());
}

TEST(EmptyRectangleEffectGenerator, ClipsAtAllFourCornersBeforeOmittingTheCenter)
{
	World world;
	struct Case { int x, y; std::vector<Point> expected; };
	for (const Case& c : {Case{0, 0, {{1, 0}, {0, 1}, {1, 1}}}, {10, 0, {{9, 0}, {9, 1}, {10, 1}}},
		{0, 10, {{0, 9}, {1, 9}, {1, 10}}}, {10, 10, {{9, 9}, {10, 9}, {9, 10}}}})
	{
		ClearEffects(); auto info = Info(); info.x0 = c.x * 48; info.y0 = c.y * 24;
		CHECK(world.generator.Generate(info)); CHECK(attempted == c.expected);
	}
}

TEST(EmptyRectangleEffectGenerator, ZeroPowerReadsBoundsButKeepsTheCallerTarget)
{
	World world;
	auto target = Target(); auto info = Info(); info.power = 0; info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions);
	CHECK(calls == std::vector<int>({1, 5})); CHECK_EQ(777, target->GetX()); CHECK(removedTargets.empty());
}

TEST(EmptyRectangleEffectGenerator, EmptyAndSingleTileZonesConstructNoEffects)
{
	World world;
	for (const MEmptyRectEffectBounds size : {MEmptyRectEffectBounds{0, 0}, {0, 11}, {11, 0}, {1, 1}})
	{
		ClearEffects(); zoneBounds = size; auto info = Info(); info.x0 = info.y0 = 0;
		CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions); CHECK(calls == std::vector<int>({1, 5}));
	}
}

TEST(EmptyRectangleEffectGenerator, OutOfZoneCentersStillSubmitTheirClippedNeighbours)
{
	World world;
	zoneBounds = {5, 11}; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release(); CHECK(attempted == std::vector<Point>({{4, 4}, {4, 5}, {4, 6}}));
	CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget); CHECK_EQ(777, info.pEffectTarget->GetX());
}

TEST(EmptyRectangleEffectGenerator, SourceTruncationAndUnsignedNarrowingPrecedeClipping)
{
	World world;
	auto info = Info(); info.x0 = info.y0 = -1;
	CHECK(world.generator.Generate(info)); CHECK(attempted == std::vector<Point>({{1, 0}, {0, 1}, {1, 1}}));
	ClearEffects(); zoneBounds = {65535, 65535}; info.x0 = -48; info.y0 = -24;
	CHECK(world.generator.Generate(info)); CHECK(attempted == std::vector<Point>({{65534, 65534}}));
	CHECK_EQ(65534 * 48, effects.front()->GetPixelX()); CHECK_EQ(65534 * 24, effects.front()->GetPixelY());
	ClearEffects(); zoneBounds = {11, 11}; info.x0 = 3145728 + 261; info.y0 = 1572864 + 130;
	CHECK(world.generator.Generate(info)); CHECK(attempted == defaultPoints);
}

TEST(EmptyRectangleEffectGenerator, MaximumRadiusVisitsEveryNonCenterTileExactlyOnceInOrder)
{
	World world;
	// Stream rejected effects rather than retaining the full 511-by-511 area.
	MEffect::SetHost(nullptr); zoneBounds = {600, 600};
	const MEmptyRectEffectHost streaming{
		.Sprite = host.Sprite, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			const Point point{effect->GetX(), effect->GetY()};
			validLargeRectangle &= point.x >= 45 && point.x <= 555 && point.y >= 45 && point.y <= 555 && point != Point{300, 300};
			if (!attempted.empty())
			{
				const Point previous = attempted.back();
				validLargeRectangle &= point.y > previous.y || (point.y == previous.y && point.x > previous.x);
			}
			if (attempted.size() < 2) attempted.push_back(point); else attempted.back() = point;
			++submissions; return false;
		},
	};
	MStopZoneEmptyRectEffectGenerator::SetHost(&streaming);
	auto target = Target(); auto info = Info(); info.x0 = 300 * 48; info.y0 = 300 * 24; info.power = 255; info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK_EQ(261120, submissions); CHECK(validLargeRectangle);
	CHECK((attempted.front() == Point{45, 45})); CHECK((attempted.back() == Point{555, 555}));
	CHECK(effects.empty()); CHECK_EQ(777, target->GetX()); CHECK(removedTargets.empty()); CHECK(calls == std::vector<int>({1, 5}));
}

TEST(EmptyRectangleEffectGenerator, CopiesKeepPhaseAndUseTheClippedLowerBoundAnchor)
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

TEST(EmptyRectangleEffectGenerator, ClippedCopiesRetainTheirOffsetFromTheNewLowerBounds)
{
	World world;
	auto target = Target(); auto info = Info(); info.x0 = info.y0 = 0; info.power = 2; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(8, effects.size());
	const Point expected[] = {{777, 888}, {948, 776}, {852, 800}, {900, 800}, {948, 800}, {852, 824}, {900, 824}, {948, 824}};
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(expected[i].x, effects[i]->GetEffectTarget()->GetX()); CHECK_EQ(expected[i].y, effects[i]->GetEffectTarget()->GetY());
	}
}

TEST(EmptyRectangleEffectGenerator, EveryAcceptanceMaskTransfersTheOriginalToTheFirstAcceptedEffect)
{
	World world;
	for (int mask = 0; mask < 256; ++mask)
	{
		ClearEffects(); acceptance.assign(8, false);
		for (int slot = 0; slot < 8; ++slot) acceptance[slot] = (mask & (1 << slot)) != 0;
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK_EQ(mask != 0, world.generator.Generate(info)); CHECK_EQ(8, submissions);
		bool originalOwned = false;
		for (size_t i = 0; i < effects.size(); ++i)
		{
			const auto* linked = effects[i]->GetEffectTarget(); CHECK(linked != nullptr); if (!linked) continue;
			originalOwned |= linked == target.get(); CHECK_EQ(i == 0, linked == target.get());
			CHECK_EQ(i == 0 ? 777 : linkedPoints[slots[i]].x, linked->GetX());
			CHECK_EQ(i == 0 ? 888 : linkedPoints[slots[i]].y, linked->GetY());
		}
		CHECK_EQ(mask != 0, originalOwned); if (originalOwned) target.release();
	}
}

TEST(EmptyRectangleEffectGenerator, TargetlessMasksReportAnyAcceptanceAndLinkEveryAction)
{
	World world;
	for (int mask = 0; mask < 256; ++mask)
	{
		ClearEffects(); acceptance.assign(8, false);
		for (int slot = 0; slot < 8; ++slot) acceptance[slot] = (mask & (1 << slot)) != 0;
		CHECK_EQ(mask != 0, world.generator.Generate(Info())); CHECK_EQ(8, submissions);
		for (const auto& effect : effects) { CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(42, effect->GetActionInfo()); }
	}
}

TEST(EmptyRectangleEffectGenerator, MissingMetadataRejectsBeforeReadingBounds)
{
	World world;
	const MEmptyRectEffectHost empty{};
	for (const auto* service : {static_cast<const MEmptyRectEffectHost*>(nullptr), &empty, &host})
	{
		ClearEffects(); spriteAvailable = false; MStopZoneEmptyRectEffectGenerator::SetHost(service);
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions);
		CHECK(calls == (service == &host ? std::vector<int>({1}) : std::vector<int>{})); CHECK_EQ(777, target->GetX());
	}
}

TEST(EmptyRectangleEffectGenerator, MissingBoundsRejectBeforeConstructingEffects)
{
	World world;
	const MEmptyRectEffectHost noBounds{.Sprite = host.Sprite, .Queue = host.Queue};
	for (const auto* service : {&noBounds, &host})
	{
		ClearEffects(); boundsAvailable = false; MStopZoneEmptyRectEffectGenerator::SetHost(service);
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions); CHECK_EQ(777, target->GetX());
		CHECK(calls == (service == &host ? std::vector<int>({1, 5}) : std::vector<int>({1})));
	}
}

TEST(EmptyRectangleEffectGenerator, MissingQueueDestroysUnlinkedEffectsAndLeavesTheCallerTarget)
{
	World world;
	const MEmptyRectEffectHost metadataOnly{.Sprite = host.Sprite, .Bounds = host.Bounds};
	MStopZoneEmptyRectEffectGenerator::SetHost(&metadataOnly);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK_EQ(0, submissions); CHECK(effects.empty()); CHECK(removedTargets.empty());
	std::vector<int> expected{1, 5}; for (int i = 0; i < 8; ++i) expected.insert(expected.end(), {2, 3});
	CHECK(calls == expected); CHECK_EQ(777, target->GetX());
}

TEST(EmptyRectangleEffectGenerator, MetadataAndBoundsRefreshBetweenCalls)
{
	World world;
	CHECK(world.generator.Generate(Info())); CHECK_EQ(8, effects.size()); CHECK_EQ(12, effects.front()->GetFrameID());
	ClearEffects(); sprite = {BLT_NORMAL, 23, 258}; zoneBounds = {6, 6};
	CHECK(world.generator.Generate(Info())); CHECK_EQ(3, effects.size()); CHECK(attempted == std::vector<Point>({{4, 4}, {5, 4}, {4, 5}}));
	for (const auto& effect : effects)
	{
		CHECK_EQ(BLT_NORMAL, effect->GetBltType()); CHECK_EQ(23, effect->GetFrameID()); CHECK_EQ(2, effect->GetMaxFrame());
		CHECK(effect->Update()); CHECK_EQ(1, effect->GetFrame()); CHECK_EQ(8, effect->GetLight());
	}
}

TEST(EmptyRectangleEffectGenerator, AnimationCountsRetainByteNarrowing)
{
	World world;
	struct Case { int supplied, expected; };
	for (const Case c : {Case{-1, 255}, {0, 0}, {256, 0}, {258, 2}})
	{
		ClearEffects(); sprite.maxFrames = c.supplied; CHECK(world.generator.Generate(Info()));
		for (const auto& effect : effects) { CHECK_EQ(c.expected, effect->GetMaxFrame()); CHECK_EQ(0, effect->GetFrame()); }
	}
}

TEST(EmptyRectangleEffectGenerator, SpriteCallbackCanReplaceBoundsAndQueueServices)
{
	World world;
	zoneBounds = {6, 6};
	const MEmptyRectEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MEmptyRectEffectSprite& result) {
			result = {BLT_NORMAL, 20, 4}; MStopZoneEmptyRectEffectGenerator::SetHost(&host); return true;
		},
	};
	CHECK(MStopZoneEmptyRectEffectGenerator::SetHost(&changing) == &host);
	CHECK(world.generator.Generate(Info())); CHECK(attempted == std::vector<Point>({{4, 4}, {5, 4}, {4, 5}}));
	CHECK_EQ(20, effects.front()->GetFrameID()); CHECK_EQ(BLT_NORMAL, effects.front()->GetBltType()); CHECK_EQ(4, effects.back()->GetMaxFrame());
	CHECK_EQ(5, calls.front()); CHECK(MStopZoneEmptyRectEffectGenerator::SetHost(nullptr) == &host);
}

TEST(EmptyRectangleEffectGenerator, SpriteCallbackCanRemoveBoundsBeforeAnyConstruction)
{
	World world;
	const MEmptyRectEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MEmptyRectEffectSprite& result) {
			result = sprite; MStopZoneEmptyRectEffectGenerator::SetHost(nullptr); return true;
		},
		.Bounds = host.Bounds, .Queue = host.Queue,
	};
	MStopZoneEmptyRectEffectGenerator::SetHost(&changing);
	CHECK(!world.generator.Generate(Info())); CHECK(effects.empty()); CHECK(calls.empty()); CHECK_EQ(0, submissions);
}

TEST(EmptyRectangleEffectGenerator, BoundsCallbackCanReplaceTheQueueWithoutChangingSpriteMetadata)
{
	World world;
	const MEmptyRectEffectHost changing{
		.Sprite = host.Sprite,
		.Bounds = [](MEmptyRectEffectBounds& result) {
			calls.push_back(5); result = {6, 6}; sprite = {BLT_NORMAL, 23, 8};
			MStopZoneEmptyRectEffectGenerator::SetHost(&host); return true;
		},
	};
	MStopZoneEmptyRectEffectGenerator::SetHost(&changing);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(3, effects.size());
	CHECK(attempted == std::vector<Point>({{4, 4}, {5, 4}, {4, 5}}));
	for (const auto& effect : effects) { CHECK_EQ(12, effect->GetFrameID()); CHECK_EQ(3, effect->GetMaxFrame()); }
}

TEST(EmptyRectangleEffectGenerator, BoundsCallbackCanRemoveTheQueueBeforeSubmission)
{
	World world;
	const MEmptyRectEffectHost changing{
		.Sprite = host.Sprite,
		.Bounds = [](MEmptyRectEffectBounds& result) {
			calls.push_back(5); result = zoneBounds; MStopZoneEmptyRectEffectGenerator::SetHost(nullptr); return true;
		},
		.Queue = host.Queue,
	};
	MStopZoneEmptyRectEffectGenerator::SetHost(&changing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK_EQ(0, submissions); CHECK(effects.empty()); CHECK_EQ(777, target->GetX()); CHECK_EQ(18, calls.size());
}

TEST(EmptyRectangleEffectGenerator, QueueChangesAffectTheNextCallButKeepCurrentBoundsAndFrames)
{
	World world;
	const MEmptyRectEffectHost changing{
		.Sprite = host.Sprite, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			zoneBounds = {2, 2}; sprite = {BLT_NORMAL, 23, 8}; MStopZoneEmptyRectEffectGenerator::SetHost(&host);
			return host.Queue(std::move(effect));
		},
	};
	MStopZoneEmptyRectEffectGenerator::SetHost(&changing);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(8, effects.size()); CHECK(attempted == defaultPoints);
	for (const auto& effect : effects) { CHECK_EQ(12, effect->GetFrameID()); CHECK_EQ(3, effect->GetMaxFrame()); }
	ClearEffects(); CHECK(!world.generator.Generate(Info())); CHECK(calls == std::vector<int>({1, 5})); CHECK(effects.empty());
}

TEST(EmptyRectangleEffectGenerator, QueueRemovalAfterAcceptanceKeepsTheOriginalTarget)
{
	World world;
	const MEmptyRectEffectHost changing{
		.Sprite = host.Sprite, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect) { MStopZoneEmptyRectEffectGenerator::SetHost(nullptr); return host.Queue(std::move(effect)); },
	};
	MStopZoneEmptyRectEffectGenerator::SetHost(&changing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); CHECK_EQ(1, effects.size()); CHECK_EQ(1, submissions);
	CHECK(effects.front()->GetEffectTarget() == target.get()); target.release(); CHECK_EQ(777, info.pEffectTarget->GetX());
}

TEST(EmptyRectangleEffectGenerator, RejectingQueuesDestroyTheirOwnMarkers)
{
	World world;
	const MEmptyRectEffectHost rejecting{
		.Sprite = host.Sprite, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect) { effect->SetLink(43, Target(74).release()); return false; },
	};
	MStopZoneEmptyRectEffectGenerator::SetHost(&rejecting);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(removedTargets == std::vector<int>(8, 74));
	target.reset(); CHECK_EQ(9, removedTargets.size()); CHECK_EQ(73, removedTargets.back());
}

TEST(EmptyRectangleEffectGenerator, FirstQueueExceptionReleasesTheEffectAndLeavesTheCallerTarget)
{
	World world;
	const MEmptyRectEffectHost throwing{
		.Sprite = host.Sprite, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool {
			effect->SetLink(43, Target(74).release()); throw std::runtime_error("Queue failure");
		},
	};
	MStopZoneEmptyRectEffectGenerator::SetHost(&throwing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK(removedTargets == std::vector<int>({74})); CHECK_EQ(777, target->GetX());
	target.reset(); CHECK(removedTargets == std::vector<int>({74, 73}));
}

TEST(EmptyRectangleEffectGenerator, LaterQueueExceptionPreservesTheTransferredOriginal)
{
	World world;
	const MEmptyRectEffectHost throwing{
		.Sprite = host.Sprite, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool {
			if (effects.empty()) return host.Queue(std::move(effect));
			effect->SetLink(43, Target(74).release()); throw std::runtime_error("Later queue failure");
		},
	};
	MStopZoneEmptyRectEffectGenerator::SetHost(&throwing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, effects.size()); CHECK(effects.front()->GetEffectTarget() == target.get());
	target.release(); CHECK(removedTargets == std::vector<int>({74})); CHECK_EQ(777, info.pEffectTarget->GetX());
	ClearEffects(); CHECK(removedTargets == std::vector<int>({74, 73}));
}

TEST(EmptyRectangleEffectGenerator, AcceptanceAndRejectionDoNotConsumeRandomness)
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

TEST(EmptyRectangleEffectGenerator, RealEffectsAnimateAndRefreshLightWithoutMoving)
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

TEST(EmptyRectangleEffectGenerator, CountsRemainFiniteAndLinkTimingIsIndependent)
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

TEST(EmptyRectangleEffectGenerator, MissingBaseServicesRetainCountsAndInactiveFallback)
{
	World world;
	MEffect::SetHost(nullptr);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(29, effects.front()->GetEndFrame());
	CHECK_EQ(4, effects.front()->GetEndLinkFrame()); CHECK_EQ(0, effects.front()->GetLight()); CHECK(!effects.front()->Update());
}

TEST(EmptyRectangleEffectGenerator, CopiedTargetsSaturatePositivePixelOverflow)
{
	World world;
	const int high = (std::numeric_limits<int>::max)();
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); info.x1 = high - 7; info.y1 = high - 3;
	CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(8, effects.size());
	const Point expected[] = {{777, 888}, {high - 7, high - 27}, {high, high - 27}, {high - 55, high - 3},
		{high, high - 3}, {high - 55, high}, {high - 7, high}, {high, high}};
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(expected[i].x, effects[i]->GetEffectTarget()->GetX()); CHECK_EQ(expected[i].y, effects[i]->GetEffectTarget()->GetY());
	}
}

TEST(EmptyRectangleEffectGenerator, CopiedTargetsSaturateNegativePixelOverflow)
{
	World world;
	const int low = (std::numeric_limits<int>::min)();
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); info.x1 = low + 7; info.y1 = low + 3;
	CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(8, effects.size());
	const Point expected[] = {{777, 888}, {low + 7, low}, {low + 55, low}, {low, low + 3},
		{low + 55, low + 3}, {low, low + 27}, {low + 7, low + 27}, {low + 55, low + 27}};
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(expected[i].x, effects[i]->GetEffectTarget()->GetX()); CHECK_EQ(expected[i].y, effects[i]->GetEffectTarget()->GetY());
	}
}

TEST(EmptyRectangleEffectGenerator, ClippedCopiedTargetsSaturateWithoutMovingTheirAnchor)
{
	World world;
	const int low = (std::numeric_limits<int>::min)(), high = (std::numeric_limits<int>::max)();
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	info.x0 = info.y0 = 0; info.power = 2; info.x1 = high; info.y1 = low;
	CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(8, effects.size());
	const Point expected[] = {{777, 888}, {high, low}, {high - 48, low}, {high, low},
		{high, low}, {high - 48, low + 24}, {high, low + 24}, {high, low + 24}};
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(expected[i].x, effects[i]->GetEffectTarget()->GetX()); CHECK_EQ(expected[i].y, effects[i]->GetEffectTarget()->GetY());
	}
}

TEST(EmptyRectangleEffectGenerator, MaximumRadiusSaturatesOnlyTheCopiedTargetCoordinates)
{
	World world;
	MEffect::SetHost(nullptr); zoneBounds = {600, 600};
	const MEmptyRectEffectHost endpoints{
		.Sprite = host.Sprite, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			const int slot = submissions++;
			if (slot != 0 && slot != 261119) return false;
			effects.push_back(std::move(effect)); return true;
		},
	};
	MStopZoneEmptyRectEffectGenerator::SetHost(&endpoints);
	const int high = (std::numeric_limits<int>::max)();
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	info.x0 = 300 * 48; info.y0 = 300 * 24; info.power = 255; info.x1 = high - 20000; info.y1 = high - 12000;
	CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(261120, submissions); CHECK_EQ(2, effects.size());
	CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget);
	CHECK_EQ(777, info.pEffectTarget->GetX()); CHECK_EQ(888, info.pEffectTarget->GetY());
	CHECK_EQ(555, effects.back()->GetX()); CHECK_EQ(555, effects.back()->GetY());
	CHECK_EQ(high, effects.back()->GetEffectTarget()->GetX()); CHECK_EQ(high, effects.back()->GetEffectTarget()->GetY());
	CHECK_EQ(17, effects.back()->GetEffectTarget()->GetZ()); CHECK_EQ(123, effects.back()->GetEffectTarget()->GetID());
}

TEST(EmptyRectangleEffectGenerator, RepresentableOffsetsStillReachBothIntegerEndpoints)
{
	World world;
	const int low = (std::numeric_limits<int>::min)(), high = (std::numeric_limits<int>::max)();
	for (const Point base : {Point{low + 48, low + 24}, {high - 48, high - 24}})
	{
		ClearEffects(); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); info.x1 = base.x; info.y1 = base.y;
		CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(8, effects.size());
		const Point expected[] = {{777, 888}, {base.x, base.y - 24}, {base.x + 48, base.y - 24}, {base.x - 48, base.y},
			{base.x + 48, base.y}, {base.x - 48, base.y + 24}, {base.x, base.y + 24}, {base.x + 48, base.y + 24}};
		for (size_t i = 0; i < effects.size(); ++i)
		{
			CHECK_EQ(expected[i].x, effects[i]->GetEffectTarget()->GetX()); CHECK_EQ(expected[i].y, effects[i]->GetEffectTarget()->GetY());
		}
	}
}

TEST(EmptyRectangleEffectGenerator, ExtremeDestinationsLeaveALateOriginalAndRejectedTargetsUntouched)
{
	World world;
	const int low = (std::numeric_limits<int>::min)(), high = (std::numeric_limits<int>::max)();
	for (const bool accepted : {false, true})
	{
		ClearEffects(); rejectBefore = 7; acceptQueue = accepted;
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); info.x1 = high; info.y1 = low;
		CHECK_EQ(accepted, world.generator.Generate(info)); CHECK_EQ(8, submissions); CHECK_EQ(accepted ? 1 : 0, effects.size());
		CHECK_EQ(777, target->GetX()); CHECK_EQ(888, target->GetY()); CHECK_EQ(999, target->GetZ()); CHECK_EQ(456, target->GetID());
		if (accepted) { CHECK(effects.front()->GetEffectTarget() == target.get()); target.release(); }
	}
}

TEST(EmptyRectangleEffectGenerator, TargetlessEffectsDoNotApplyExtremeDestinationOffsets)
{
	World world;
	auto info = Info(); info.x1 = (std::numeric_limits<int>::max)(); info.y1 = (std::numeric_limits<int>::min)();
	CHECK(world.generator.Generate(info)); CHECK_EQ(8, effects.size());
	for (const auto& effect : effects) { CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(42, effect->GetActionInfo()); }
}
