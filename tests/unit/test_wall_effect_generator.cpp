#include "test_framework.h"
#include "MStopZoneWallEffectGenerator.h"
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
MStopWallEffectSprite sprite;
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
const std::vector<Point> defaultPoints{{8, 8}, {9, 9}, {10, 10}, {11, 11}, {12, 12}};
const Point linkedPoints[] = {{777, 888}, {549, 274}, {597, 298}, {645, 322}, {693, 346}};
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(3); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE frame) { calls.push_back(2); return 7 + frame; },
};
const MEffectTargetHost targetHost{.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); }};
const MStopWallEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MStopWallEffectSprite& result) {
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

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MStopWallEffectHost* previousWall = MStopZoneWallEffectGenerator::SetHost(&host);
	MStopZoneWallEffectGenerator generator;
	World()
	{
		frameNow = 100; sprite = {BLT_EFFECT, 12, 3}; spriteAvailable = acceptQueue = true;
		requestedSprite = -1; submissions = rejectBefore = 0;
		effects.clear(); calls.clear(); slots.clear(); removedTargets.clear(); acceptance.clear(); attempted.clear();
	}
	~World()
	{
		effects.clear(); MStopZoneWallEffectGenerator::SetHost(previousWall);
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

TEST(WallEffectGenerator, ConfiguresRealEffectsIncludingTheMiddleTile)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_STOP_ZONE_WALL, world.generator.GetID());
	CHECK(world.generator.Generate(Info())); CHECK_EQ(5, effects.size()); CHECK(attempted == defaultPoints); CHECK_EQ(17, requestedSprite);
	std::vector<int> expected{1}; for (int i = 0; i < 5; ++i) expected.insert(expected.end(), {2, 3, 4}); CHECK(calls == expected);
	for (size_t i = 0; i < effects.size(); ++i)
	{
		auto& effect = *effects[i]; CHECK_EQ(MEffect::EFFECT_SECTOR, effect.GetEffectType());
		CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame());
		CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight()); CHECK_EQ(defaultPoints[i].x * 48, effect.GetPixelX());
		CHECK_EQ(defaultPoints[i].y * 24, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ());
		CHECK_EQ(5, effect.GetStepPixel()); CHECK_EQ(DIRECTION_RIGHTUP, effect.GetDirection()); CHECK_EQ(2, effect.GetPower());
		CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame()); CHECK_EQ(42, effect.GetActionInfo());
		CHECK(effect.GetEffectTarget() == nullptr); CHECK(!effect.IsMulti());
	}
}

TEST(WallEffectGenerator, EveryDirectionKeepsItsCompleteTileSequence)
{
	World world;
	const std::array<std::vector<Point>, 8> expected{{
		{{10, 8}, {10, 9}, {10, 10}, {10, 11}, {10, 12}},
		{{8, 8}, {9, 9}, {10, 10}, {11, 11}, {12, 12}},
		{{8, 10}, {9, 10}, {10, 10}, {11, 10}, {12, 10}},
		{{12, 8}, {11, 9}, {10, 10}, {9, 11}, {8, 12}},
		{{10, 8}, {10, 9}, {10, 10}, {10, 11}, {10, 12}},
		{{8, 8}, {9, 9}, {10, 10}, {11, 11}, {12, 12}},
		{{8, 10}, {9, 10}, {10, 10}, {11, 10}, {12, 10}},
		{{8, 12}, {9, 11}, {10, 10}, {11, 9}, {12, 8}},
	}};
	for (BYTE direction = 0; direction < 8; ++direction)
	{
		ClearEffects(); auto info = Info(); info.direction = direction;
		CHECK(world.generator.Generate(info)); CHECK(attempted == expected[direction]);
		for (const auto& effect : effects) CHECK_EQ(direction, effect->GetDirection());
	}
}

TEST(WallEffectGenerator, EvenStepCountsKeepTheAsymmetricStartAndAllMiddleTiles)
{
	World world;
	auto target = Target(); auto info = Info(); info.step = 4; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(4, effects.size());
	CHECK(attempted == std::vector<Point>({{8, 8}, {9, 9}, {10, 10}, {11, 11}}));
	CHECK_EQ(645, effects.back()->GetEffectTarget()->GetX()); CHECK_EQ(322, effects.back()->GetEffectTarget()->GetY());
}

TEST(WallEffectGenerator, ZeroStepsKeepCallerOwnershipButOneStepTransfersIt)
{
	World world;
	auto target = Target(); auto info = Info(); info.step = 0; info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK_EQ(0, submissions); CHECK(calls == std::vector<int>({1})); CHECK_EQ(777, target->GetX());
	ClearEffects(); info.step = 1;
	CHECK(world.generator.Generate(info)); CHECK_EQ(1, effects.size()); CHECK(effects.front()->GetEffectTarget() == target.get()); target.release();
	CHECK(attempted == std::vector<Point>({{10, 10}})); CHECK_EQ(777, info.pEffectTarget->GetX()); CHECK_EQ(888, info.pEffectTarget->GetY());
}

TEST(WallEffectGenerator, MaximumLengthIncludesEveryTileAndFullTargetDisplacement)
{
	World world;
	auto target = Target(); auto info = Info(); info.step = 255; info.x1 = 19221; info.y1 = 9610; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(255, effects.size());
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(273 + i, effects[i]->GetX()); CHECK_EQ(273 + i, effects[i]->GetY()); CHECK_EQ(255, effects[i]->GetStepPixel());
	}
	CHECK_EQ(31413, effects.back()->GetEffectTarget()->GetX()); CHECK_EQ(15706, effects.back()->GetEffectTarget()->GetY());
}

TEST(WallEffectGenerator, MineActionsStillPlaceWallsAtTheDestination)
{
	World world;
	for (int action = MINE_ANKLE_KILLER - 1; action <= MINE_COBRA + 1; ++action)
	{
		ClearEffects(); auto info = Info(); info.nActionInfo = static_cast<TYPE_ACTIONINFO>(action);
		CHECK(world.generator.Generate(info)); CHECK(attempted == defaultPoints);
		for (const auto& effect : effects) CHECK_EQ(action, effect->GetActionInfo());
	}
}

TEST(WallEffectGenerator, PowerChangesDoNotChangeTheWallLength)
{
	World world;
	for (const BYTE power : {static_cast<BYTE>(0), static_cast<BYTE>(255)})
	{
		ClearEffects(); auto info = Info(); info.power = power;
		CHECK(world.generator.Generate(info)); CHECK(attempted == defaultPoints);
		for (const auto& effect : effects) CHECK_EQ(power, effect->GetPower());
	}
}

TEST(WallEffectGenerator, NegativeDestinationTruncationPrecedesUnsignedTileStorage)
{
	World world;
	auto info = Info(); info.x1 = info.y1 = -1;
	CHECK(world.generator.Generate(info)); CHECK(attempted == std::vector<Point>({{65534, 65534}, {65535, 65535}, {0, 0}, {1, 1}, {2, 2}}));
	ClearEffects(); info.x1 = -48; info.y1 = -24;
	CHECK(world.generator.Generate(info)); CHECK(attempted == std::vector<Point>({{65533, 65533}, {65534, 65534}, {65535, 65535}, {0, 0}, {1, 1}}));
}

TEST(WallEffectGenerator, WrappedTilesDoNotWrapCopiedPixelCoordinates)
{
	World world;
	auto target = Target(); auto info = Info(); info.x1 += 3145728; info.y1 += 1572864; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release(); CHECK(attempted == defaultPoints);
	CHECK_EQ(3146277, effects[1]->GetEffectTarget()->GetX()); CHECK_EQ(1573138, effects[1]->GetEffectTarget()->GetY());
	CHECK_EQ(3146421, effects.back()->GetEffectTarget()->GetX()); CHECK_EQ(1573210, effects.back()->GetEffectTarget()->GetY());
}

TEST(WallEffectGenerator, CopiesPreservePhaseAndUsePixelRemaindersFromTheDestination)
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

TEST(WallEffectGenerator, EveryDirectionAdvancesCopiedTargetsFromDestinationPixels)
{
	World world;
	const Point expectedLast[] = {{501, 346}, {693, 346}, {693, 250}, {309, 346}, {501, 346}, {693, 346}, {693, 250}, {693, 154}};
	for (BYTE direction = 0; direction < 8; ++direction)
	{
		ClearEffects(); auto target = Target(); auto info = Info(); info.direction = direction; info.pEffectTarget = target.get();
		CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(5, effects.size());
		CHECK_EQ(expectedLast[direction].x, effects.back()->GetEffectTarget()->GetX()); CHECK_EQ(expectedLast[direction].y, effects.back()->GetEffectTarget()->GetY());
	}
}

TEST(WallEffectGenerator, EveryAcceptanceMaskTransfersTheOriginalToTheFirstAcceptedEffect)
{
	World world;
	for (int mask = 0; mask < 32; ++mask)
	{
		ClearEffects(); acceptance.assign(5, false);
		for (int slot = 0; slot < 5; ++slot) acceptance[slot] = (mask & (1 << slot)) != 0;
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK_EQ(mask != 0, world.generator.Generate(info)); CHECK_EQ(5, submissions);
		bool originalOwned = false;
		for (size_t i = 0; i < effects.size(); ++i)
		{
			const auto* linked = effects[i]->GetEffectTarget(); CHECK(linked != nullptr); if (!linked) continue;
			originalOwned |= linked == target.get(); CHECK_EQ(i == 0, linked == target.get());
			CHECK_EQ(i == 0 ? 777 : linkedPoints[slots[i]].x, linked->GetX()); CHECK_EQ(i == 0 ? 888 : linkedPoints[slots[i]].y, linked->GetY());
		}
		CHECK_EQ(mask != 0, originalOwned); if (originalOwned) target.release();
	}
}

TEST(WallEffectGenerator, TargetlessAcceptanceMasksLinkEveryAcceptedAction)
{
	World world;
	for (int mask = 0; mask < 32; ++mask)
	{
		ClearEffects(); acceptance.assign(5, false);
		for (int slot = 0; slot < 5; ++slot) acceptance[slot] = (mask & (1 << slot)) != 0;
		CHECK_EQ(mask != 0, world.generator.Generate(Info())); CHECK_EQ(5, submissions);
		for (const auto& effect : effects) { CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(42, effect->GetActionInfo()); }
	}
}

TEST(WallEffectGenerator, MissingMetadataRejectsBeforeConstruction)
{
	World world;
	const MStopWallEffectHost empty{};
	for (const auto* service : {static_cast<const MStopWallEffectHost*>(nullptr), &empty, &host})
	{
		ClearEffects(); spriteAvailable = false; MStopZoneWallEffectGenerator::SetHost(service);
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions);
		CHECK(calls == (service == &host ? std::vector<int>({1}) : std::vector<int>{})); CHECK_EQ(777, target->GetX());
	}
}

TEST(WallEffectGenerator, MissingQueueReleasesUnlinkedEffectsAndKeepsCallerOwnership)
{
	World world;
	const MStopWallEffectHost noQueue{.Sprite = host.Sprite}; MStopZoneWallEffectGenerator::SetHost(&noQueue);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions); CHECK_EQ(777, target->GetX()); CHECK(removedTargets.empty());
	std::vector<int> expected{1}; for (int i = 0; i < 5; ++i) expected.insert(expected.end(), {2, 3}); CHECK(calls == expected);
}

TEST(WallEffectGenerator, SpriteCallbackCanReplaceTheQueueAndReturnDifferentMetadata)
{
	World world;
	const MStopWallEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MStopWallEffectSprite& result) {
			result = {BLT_NORMAL, 23, 4}; MStopZoneWallEffectGenerator::SetHost(&host); return true;
		},
	};
	CHECK(MStopZoneWallEffectGenerator::SetHost(&changing) == &host);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(5, effects.size()); CHECK(attempted == defaultPoints);
	for (const auto& effect : effects) { CHECK_EQ(BLT_NORMAL, effect->GetBltType()); CHECK_EQ(23, effect->GetFrameID()); CHECK_EQ(4, effect->GetMaxFrame()); }
	CHECK_EQ(2, calls.front()); CHECK(MStopZoneWallEffectGenerator::SetHost(nullptr) == &host);
}

TEST(WallEffectGenerator, SpriteCallbackCanRemoveTheQueueBeforeConstruction)
{
	World world;
	const MStopWallEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MStopWallEffectSprite& result) { result = sprite; MStopZoneWallEffectGenerator::SetHost(nullptr); return true; },
		.Queue = host.Queue,
	};
	MStopZoneWallEffectGenerator::SetHost(&changing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions); CHECK_EQ(10, calls.size()); CHECK_EQ(777, target->GetX());
}

TEST(WallEffectGenerator, QueueReplacementKeepsCurrentMetadataAndRefreshesTheNextCall)
{
	World world;
	const MStopWallEffectHost changing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { sprite = {BLT_NORMAL, 23, 8}; MStopZoneWallEffectGenerator::SetHost(&host); return host.Queue(std::move(effect)); },
	};
	MStopZoneWallEffectGenerator::SetHost(&changing);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(5, effects.size());
	for (const auto& effect : effects) { CHECK_EQ(12, effect->GetFrameID()); CHECK_EQ(3, effect->GetMaxFrame()); }
	ClearEffects(); CHECK(world.generator.Generate(Info()));
	for (const auto& effect : effects) { CHECK_EQ(23, effect->GetFrameID()); CHECK_EQ(8, effect->GetMaxFrame()); }
}

TEST(WallEffectGenerator, QueueRemovalKeepsTheAlreadyTransferredOriginal)
{
	World world;
	const MStopWallEffectHost changing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { MStopZoneWallEffectGenerator::SetHost(nullptr); return host.Queue(std::move(effect)); },
	};
	MStopZoneWallEffectGenerator::SetHost(&changing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); CHECK_EQ(1, effects.size()); CHECK_EQ(1, submissions);
	CHECK(effects.front()->GetEffectTarget() == target.get()); target.release(); CHECK_EQ(777, info.pEffectTarget->GetX());
}

TEST(WallEffectGenerator, RejectingQueuesDestroyTheirOwnMarkers)
{
	World world;
	const MStopWallEffectHost rejecting{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { effect->SetLink(43, Target(74).release()); return false; },
	};
	MStopZoneWallEffectGenerator::SetHost(&rejecting);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(removedTargets == std::vector<int>(5, 74));
	target.reset(); CHECK_EQ(6, removedTargets.size()); CHECK_EQ(73, removedTargets.back());
}

TEST(WallEffectGenerator, FirstQueueExceptionReleasesItsEffectAndKeepsCallerOwnership)
{
	World world;
	const MStopWallEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool { effect->SetLink(43, Target(74).release()); throw std::runtime_error("Queue failure"); },
	};
	MStopZoneWallEffectGenerator::SetHost(&throwing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	bool threw = false; try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK(effects.empty()); CHECK(removedTargets == std::vector<int>({74})); CHECK_EQ(777, target->GetX());
	target.reset(); CHECK(removedTargets == std::vector<int>({74, 73}));
}

TEST(WallEffectGenerator, LaterQueueExceptionKeepsTheTransferredOriginal)
{
	World world;
	const MStopWallEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool {
			if (effects.empty()) return host.Queue(std::move(effect));
			effect->SetLink(43, Target(74).release()); throw std::runtime_error("Later queue failure");
		},
	};
	MStopZoneWallEffectGenerator::SetHost(&throwing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	bool threw = false; try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, effects.size()); CHECK(effects.front()->GetEffectTarget() == target.get()); target.release();
	CHECK(removedTargets == std::vector<int>({74})); CHECK_EQ(777, info.pEffectTarget->GetX());
	ClearEffects(); CHECK(removedTargets == std::vector<int>({74, 73}));
}

TEST(WallEffectGenerator, AcceptanceAndRejectionDoNotConsumeRandomness)
{
	World world;
	for (const bool accepted : {false, true})
	{
		ClearEffects(); acceptQueue = accepted;
		std::srand(947); const int next = std::rand(); std::srand(947);
		CHECK_EQ(accepted, world.generator.Generate(Info())); CHECK_EQ(next, std::rand());
		for (const auto& effect : effects) CHECK_EQ(0, effect->GetFrame());
	}
}

TEST(WallEffectGenerator, FrameCountsKeepByteNarrowingWithoutRandomAnimationStarts)
{
	World world;
	struct Case { int supplied, expected; };
	for (const Case c : {Case{-1, 255}, {0, 0}, {256, 0}, {258, 2}})
	{
		ClearEffects(); sprite.maxFrames = c.supplied; CHECK(world.generator.Generate(Info()));
		for (const auto& effect : effects) { CHECK_EQ(c.expected, effect->GetMaxFrame()); CHECK_EQ(0, effect->GetFrame()); }
	}
}

TEST(WallEffectGenerator, RealEffectsAnimateAndRefreshLightBeforeTheirStationaryExpiry)
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

TEST(WallEffectGenerator, CountsRemainFiniteAndLinkTimingIsIndependent)
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

TEST(WallEffectGenerator, MissingBaseServicesKeepFiniteCountsAndInactiveEffects)
{
	World world;
	MEffect::SetHost(nullptr);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(29, effects.front()->GetEndFrame());
	CHECK_EQ(4, effects.front()->GetEndLinkFrame()); CHECK_EQ(0, effects.front()->GetLight()); CHECK(!effects.front()->Update());
}
