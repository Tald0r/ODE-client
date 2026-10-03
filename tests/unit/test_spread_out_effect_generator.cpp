#include "test_framework.h"
#include "MSpreadOutEffectGenerator.h"
#include "MStopZoneRandomEffectGenerator.h"
#include "MLinearEffect.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MFixedZoneEffectSprite sprite;
bool spriteAvailable;
int requestedSprite, submissions, acceptance;
std::vector<int> calls, slots, removedTargets;
std::vector<std::unique_ptr<MEffect>> effects;
struct Point { int x, y; bool operator==(const Point&) const = default; };
const std::array<Point, 8> destinations{{{-60, 120}, {50, 215}, {240, 270}, {430, 215}, {540, 120}, {430, 25}, {240, -30}, {50, 25}}};
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(3); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE frame) { calls.push_back(2); return 7 + frame; },
};
const MEffectTargetHost targetHost{.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); }};
const MFixedZoneEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& result) {
		calls.push_back(1); requestedSprite = type; result = sprite; return spriteAvailable;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		calls.push_back(4); CHECK_EQ(MEffect::EFFECT_LINEAR, effect->GetEffectType()); CHECK(effect->GetEffectTarget() == nullptr);
		CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo()); const int slot = submissions++;
		if ((acceptance & (1 << slot)) == 0) return false;
		slots.push_back(slot); effects.push_back(std::move(effect)); return true;
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MFixedZoneEffectHost* previousGenerator = MSpreadOutEffectGenerator::SetHost(&host);
	MSpreadOutEffectGenerator generator;
	World()
	{
		frameNow = 100; sprite = {BLT_EFFECT, 12, 3}; spriteAvailable = true;
		requestedSprite = -1; submissions = 0; acceptance = 255;
		effects.clear(); calls.clear(); slots.clear(); removedTargets.clear();
	}
	~World()
	{
		effects.clear(); MSpreadOutEffectGenerator::SetHost(previousGenerator);
		MEffectTarget::SetHost(previousTarget); MEffect::SetHost(previousEffect);
	}
};

EFFECTGENERATOR_INFO Info()
{
	EFFECTGENERATOR_INFO info{}; info.nActionInfo = 42; info.effectSpriteType = 17;
	info.x0 = 240; info.y0 = 120; info.z0 = 17; info.x1 = 900; info.y1 = 800; info.z1 = 99;
	info.direction = 255; info.step = 10; info.count = 30; info.linkCount = 5; info.power = 2; info.creatureID = 123;
	return info;
}

std::unique_ptr<MEffectTarget> Target(BYTE id = 73)
{
	auto target = std::make_unique<MEffectTarget>(3); target->NextPhase(); target->m_EffectID = id; target->Set(777, 888, 999, 456);
	target->SetServerID(789); target->SetDelayFrame(31); target->SetResultTime(); target->SetResult(new MActionResult); return target;
}

void ClearEffects()
{
	effects.clear(); calls.clear(); slots.clear(); submissions = 0;
}

int NextRandom(unsigned seed)
{
	std::srand(seed); const int result = std::rand(); std::srand(seed); return result;
}

} // namespace

TEST(SpreadOutEffectGenerator, ConfiguresEightRealLinearEffectsInNumberedDirectionOrder)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_SPREAD_OUT, world.generator.GetID()); const int next = NextRandom(67);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(8, effects.size()); CHECK_EQ(17, requestedSprite); CHECK_EQ(next, std::rand());
	std::vector<int> expected{1}; for (int i = 0; i < 8; ++i) expected.insert(expected.end(), {2, 3, 4}); CHECK(calls == expected);
	for (size_t i = 0; i < effects.size(); ++i)
	{
		auto& effect = *effects[i]; CHECK_EQ(MEffect::EFFECT_LINEAR, effect.GetEffectType()); CHECK_EQ(BLT_EFFECT, effect.GetBltType());
		CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame()); CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight());
		CHECK_EQ(240, effect.GetPixelX()); CHECK_EQ(120, effect.GetPixelY()); CHECK_EQ(0, effect.GetPixelZ()); CHECK_EQ(5, effect.GetX()); CHECK_EQ(5, effect.GetY());
		CHECK_EQ(10, effect.GetStepPixel()); CHECK_EQ(i, effect.GetDirection()); CHECK_EQ(2, effect.GetPower());
		CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame()); CHECK_EQ(42, effect.GetActionInfo());
		CHECK(effect.GetEffectTarget() == nullptr); CHECK(!effect.IsMulti()); CHECK(!effect.IsDelayFrame());
	}
}

TEST(SpreadOutEffectGenerator, DestinationsKeepIsometricSpeedScalingAndRetargetTheOriginal)
{
	World world;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); CHECK(world.generator.Generate(info)); target.release();
	for (size_t i = 0; i < effects.size(); ++i)
	{
		const auto* linked = effects[i]->GetEffectTarget(); CHECK_EQ(i == 0, linked == info.pEffectTarget);
		CHECK_EQ(destinations[i].x, linked->GetX()); CHECK_EQ(destinations[i].y, linked->GetY()); CHECK_EQ(0, linked->GetZ()); CHECK_EQ(123, linked->GetID());
	}
}

TEST(SpreadOutEffectGenerator, AllAcceptanceMasksReserveTheOriginalForDirectionZero)
{
	World world;
	for (int mask = 0; mask < 256; ++mask)
	{
		ClearEffects(); acceptance = mask; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK_EQ((mask & 1) != 0, world.generator.Generate(info)); CHECK_EQ(8, submissions); bool originalOwned = false;
		for (size_t i = 0; i < effects.size(); ++i)
		{
			const auto* linked = effects[i]->GetEffectTarget(); CHECK(linked != nullptr); if (!linked) continue;
			originalOwned |= linked == target.get(); CHECK_EQ(slots[i] == 0, linked == target.get());
			CHECK_EQ(destinations[slots[i]].x, linked->GetX()); CHECK_EQ(destinations[slots[i]].y, linked->GetY()); CHECK_EQ(0, linked->GetZ()); CHECK_EQ(123, linked->GetID());
		}
		CHECK_EQ((mask & 1) != 0, originalOwned);
		if (originalOwned) target.release();
		else { CHECK_EQ(777, target->GetX()); CHECK_EQ(888, target->GetY()); CHECK_EQ(999, target->GetZ()); CHECK_EQ(456, target->GetID()); }
	}
}

TEST(SpreadOutEffectGenerator, TargetlessAcceptanceMasksStillReportOnlyDirectionZero)
{
	World world;
	for (int mask = 0; mask < 256; ++mask)
	{
		ClearEffects(); acceptance = mask; CHECK_EQ((mask & 1) != 0, world.generator.Generate(Info())); CHECK_EQ(8, submissions);
		for (const auto& effect : effects) { CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(42, effect->GetActionInfo()); }
	}
}

TEST(SpreadOutEffectGenerator, CopiesKeepPhaseMetadataButOnlyOriginalKeepsResultAndServerID)
{
	World world;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); CHECK(world.generator.Generate(info)); target.release();
	for (size_t i = 0; i < effects.size(); ++i)
	{
		const auto* linked = effects[i]->GetEffectTarget(); CHECK_EQ(1, linked->GetCurrentPhase()); CHECK_EQ(3, linked->GetMaxPhase());
		CHECK_EQ(31, linked->GetDelayFrame()); CHECK_EQ(73, linked->GetEffectID()); CHECK_EQ(i == 0, linked->IsExistResult()); CHECK_EQ(i == 0, linked->IsResultTime());
		CHECK_EQ(i == 0 ? 789 : OBJECTID_NULL, linked->GetServerID()); for (size_t j = 0; j < i; ++j) CHECK(linked != effects[j]->GetEffectTarget());
	}
}

TEST(SpreadOutEffectGenerator, RejectedDirectionZeroLeavesIndependentCopiesAfterCallerDestruction)
{
	World world;
	acceptance = 254; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); CHECK(!world.generator.Generate(info));
	CHECK_EQ(7, effects.size()); CHECK(removedTargets.empty()); target.reset(); CHECK(removedTargets == std::vector<int>{73});
	for (size_t i = 0; i < effects.size(); ++i) { CHECK_EQ(destinations[i + 1].x, effects[i]->GetEffectTarget()->GetX()); CHECK(!effects[i]->GetEffectTarget()->IsExistResult()); }
	ClearEffects(); CHECK(removedTargets == std::vector<int>(8, 73));
}

TEST(SpreadOutEffectGenerator, ZeroStepOrCountStillEmitsEightEffectsTargetingTheSource)
{
	World world;
	for (int kind = 0; kind < 2; ++kind)
	{
		ClearEffects(); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); if (kind) info.count = 0; else info.step = 0;
		CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(8, effects.size());
		for (const auto& effect : effects) { CHECK_EQ(240, effect->GetEffectTarget()->GetX()); CHECK_EQ(120, effect->GetEffectTarget()->GetY()); CHECK_EQ(0, effect->GetEffectTarget()->GetZ()); }
	}
}

TEST(SpreadOutEffectGenerator, InputDirectionAndPowerDoNotChangeTheEightTrajectoryDestinations)
{
	World world;
	for (const BYTE value : {static_cast<BYTE>(0), static_cast<BYTE>(255)})
	{
		ClearEffects(); auto target = Target(); auto info = Info(); info.direction = info.power = value; info.pEffectTarget = target.get();
		CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(8, effects.size());
		for (size_t i = 0; i < effects.size(); ++i) { CHECK_EQ(value, effects[i]->GetPower()); CHECK_EQ(i, effects[i]->GetDirection()); CHECK_EQ(destinations[i].x, effects[i]->GetEffectTarget()->GetX()); }
	}
}

TEST(SpreadOutEffectGenerator, RealLinearUpdatesFollowTheirComputedTargetsAtTheRequestedSpeed)
{
	World world;
	CHECK(world.generator.Generate(Info()));
	CHECK(effects[0]->Update()); CHECK_EQ(230, effects[0]->GetPixelX()); CHECK_EQ(120, effects[0]->GetPixelY()); CHECK_EQ(0, effects[0]->GetPixelZ());
	CHECK(effects[DIRECTION_LEFTDOWN]->Update()); CHECK_EQ(231, effects[DIRECTION_LEFTDOWN]->GetPixelX()); CHECK_EQ(124, effects[DIRECTION_LEFTDOWN]->GetPixelY());
	CHECK(effects[DIRECTION_DOWN]->Update()); CHECK_EQ(240, effects[DIRECTION_DOWN]->GetPixelX()); CHECK_EQ(130, effects[DIRECTION_DOWN]->GetPixelY());
	CHECK(effects[DIRECTION_RIGHT]->Update()); CHECK_EQ(250, effects[DIRECTION_RIGHT]->GetPixelX()); CHECK_EQ(120, effects[DIRECTION_RIGHT]->GetPixelY());
	CHECK_EQ(1, effects[0]->GetFrame()); CHECK_EQ(8, effects[0]->GetLight()); CHECK_EQ(0, effects[0]->GetDirection());
	frameNow = 129; CHECK(!effects[DIRECTION_RIGHT]->Update()); CHECK_EQ(250, effects[DIRECTION_RIGHT]->GetPixelX()); CHECK_EQ(1, effects[DIRECTION_RIGHT]->GetFrame());
}

TEST(SpreadOutEffectGenerator, RealLinearEffectArrivesAtTheRetargetedDestination)
{
	World world;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); CHECK(world.generator.Generate(info)); target.release();
	int updates = 0; while (updates < 40 && effects.front()->Update()) ++updates;
	CHECK(updates < 40); CHECK_EQ(-60, effects.front()->GetPixelX()); CHECK_EQ(120, effects.front()->GetPixelY()); CHECK_EQ(0, effects.front()->GetPixelZ());
	CHECK(effects.front()->IsEnd()); CHECK_EQ(effects.front()->GetEffectTarget()->GetX(), effects.front()->GetPixelX());
}

TEST(SpreadOutEffectGenerator, MissingMetadataRejectsBeforeConstructingAnyEffect)
{
	World world;
	const MFixedZoneEffectHost empty{};
	for (const auto* service : {static_cast<const MFixedZoneEffectHost*>(nullptr), &empty, &host})
	{
		ClearEffects(); MSpreadOutEffectGenerator::SetHost(service); spriteAvailable = false; const int next = NextRandom(97);
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); CHECK(!world.generator.Generate(info));
		CHECK_EQ(0, submissions); CHECK(effects.empty()); CHECK_EQ(next, std::rand()); CHECK(target->IsExistResult());
		CHECK(calls == (service == &host ? std::vector<int>{1} : std::vector<int>{}));
	}
}

TEST(SpreadOutEffectGenerator, MissingQueueConstructsAndDiscardsAllEightUnlinkedEffects)
{
	World world;
	const MFixedZoneEffectHost noQueue{.Sprite = host.Sprite}; MSpreadOutEffectGenerator::SetHost(&noQueue);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK(removedTargets.empty());
	std::vector<int> expected{1}; for (int i = 0; i < 8; ++i) expected.insert(expected.end(), {2, 3}); CHECK(calls == expected); CHECK_EQ(777, target->GetX());
}

TEST(SpreadOutEffectGenerator, SpriteCallbackCanReplaceTheQueue)
{
	World world;
	const MFixedZoneEffectHost replacing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& result) { MSpreadOutEffectGenerator::SetHost(&host); return host.Sprite(type, result); },
	};
	MSpreadOutEffectGenerator::SetHost(&replacing); CHECK(world.generator.Generate(Info())); CHECK_EQ(8, effects.size());
}

TEST(SpreadOutEffectGenerator, QueueReplacementKeepsSpriteMetadataUntilTheNextGeneration)
{
	World world;
	const MFixedZoneEffectHost replacing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { sprite = {BLT_SHADOW, 21, 5}; MSpreadOutEffectGenerator::SetHost(&host); return host.Queue(std::move(effect)); },
	};
	MSpreadOutEffectGenerator::SetHost(&replacing); CHECK(world.generator.Generate(Info()));
	for (const auto& effect : effects) { CHECK_EQ(BLT_EFFECT, effect->GetBltType()); CHECK_EQ(12, effect->GetFrameID()); CHECK_EQ(3, effect->GetMaxFrame()); }
	ClearEffects(); CHECK(world.generator.Generate(Info()));
	for (const auto& effect : effects) { CHECK_EQ(BLT_SHADOW, effect->GetBltType()); CHECK_EQ(21, effect->GetFrameID()); CHECK_EQ(5, effect->GetMaxFrame()); }
}

TEST(SpreadOutEffectGenerator, QueueRemovalKeepsTheRetargetedOriginal)
{
	World world;
	const MFixedZoneEffectHost removing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { MSpreadOutEffectGenerator::SetHost(nullptr); return host.Queue(std::move(effect)); },
	};
	MSpreadOutEffectGenerator::SetHost(&removing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(1, effects.size()); CHECK_EQ(-60, info.pEffectTarget->GetX()); CHECK_EQ(120, info.pEffectTarget->GetY());
	ClearEffects(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(SpreadOutEffectGenerator, InstallerIsIndependentOfRandomZoneGeneration)
{
	World world;
	const MFixedZoneEffectHost sentinel{}; const auto* previousRandom = MStopZoneRandomEffectGenerator::SetHost(&sentinel);
	CHECK(MSpreadOutEffectGenerator::SetHost(nullptr) == &host); CHECK(MStopZoneRandomEffectGenerator::SetHost(previousRandom) == &sentinel);
	CHECK(!world.generator.Generate(Info())); CHECK(calls.empty());
}

TEST(SpreadOutEffectGenerator, RejectingQueueDestroysItsMarkersAndPreservesCallerCoordinates)
{
	World world;
	const MFixedZoneEffectHost rejecting{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { ++submissions; effect->SetLink(42, Target(94).release()); return false; },
	};
	MSpreadOutEffectGenerator::SetHost(&rejecting); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK_EQ(8, submissions); CHECK(removedTargets == std::vector<int>(8, 94)); CHECK_EQ(777, target->GetX()); CHECK(target->IsExistResult());
}

TEST(SpreadOutEffectGenerator, FirstQueueExceptionDestroysItsEffectWithoutTakingTheTarget)
{
	World world;
	const MFixedZoneEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool { ++submissions; effect->SetLink(42, Target(94).release()); throw std::runtime_error("queue"); },
	};
	MSpreadOutEffectGenerator::SetHost(&throwing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, submissions); CHECK(removedTargets == std::vector<int>{94}); CHECK_EQ(777, target->GetX()); CHECK(target->IsExistResult());
}

TEST(SpreadOutEffectGenerator, LaterQueueExceptionKeepsTheAlreadyRetargetedOriginal)
{
	World world;
	const MFixedZoneEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			if (submissions == 1) { effect->SetLink(42, Target(94).release()); throw std::runtime_error("queue"); }
			return host.Queue(std::move(effect));
		},
	};
	MSpreadOutEffectGenerator::SetHost(&throwing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, effects.size());
	if (!effects.empty()) { CHECK(effects.front()->GetEffectTarget() == target.get()); if (effects.front()->GetEffectTarget() == target.get()) target.release(); }
	CHECK_EQ(-60, info.pEffectTarget->GetX()); CHECK_EQ(120, info.pEffectTarget->GetY()); CHECK(removedTargets == std::vector<int>{94});
	ClearEffects(); CHECK(removedTargets == std::vector<int>({94, 73}));
}

TEST(SpreadOutEffectGenerator, FrameCountsNarrowWithoutChangingDirectionCountOrRandomState)
{
	World world;
	for (const int frames : {-1, 0, 256, 258})
	{
		ClearEffects(); sprite.maxFrames = frames; const int next = NextRandom(127); CHECK(world.generator.Generate(Info())); CHECK_EQ(8, effects.size()); CHECK_EQ(next, std::rand());
		for (const auto& effect : effects) { CHECK_EQ(static_cast<BYTE>(frames), effect->GetMaxFrame()); CHECK_EQ(0, effect->GetFrame()); }
	}
}

TEST(SpreadOutEffectGenerator, CountAndLinkSentinelsRemainFiniteAndIndependent)
{
	World world;
	auto info = Info(); info.count = 65535; info.linkCount = MAX_LINKCOUNT; CHECK(world.generator.Generate(info));
	for (const auto& effect : effects) { CHECK_EQ(65634, effect->GetEndFrame()); CHECK_EQ(65634, effect->GetEndLinkFrame()); }
	ClearEffects(); info.count = 1; info.linkCount = 65534; CHECK(world.generator.Generate(info));
	for (const auto& effect : effects) { CHECK_EQ(100, effect->GetEndFrame()); CHECK_EQ(65633, effect->GetEndLinkFrame()); }
}

TEST(SpreadOutEffectGenerator, WrappedClockAndMissingBaseServicesKeepTheirFallbacks)
{
	World world;
	frameNow = 0xFFFFFFFEu; CHECK(world.generator.Generate(Info()));
	for (const auto& effect : effects) { CHECK_EQ(27, effect->GetEndFrame()); CHECK_EQ(2, effect->GetEndLinkFrame()); CHECK(effect->IsEnd()); }
	ClearEffects(); MEffect::SetHost(nullptr); CHECK(world.generator.Generate(Info()));
	for (const auto& effect : effects) { CHECK_EQ(29, effect->GetEndFrame()); CHECK_EQ(4, effect->GetEndLinkFrame()); CHECK_EQ(0, effect->GetLight()); CHECK(!effect->Update()); CHECK_EQ(0, effect->GetFrame()); }
}

TEST(SpreadOutEffectGenerator, ZeroSourceDestinationsFollowWrappedNeighborTilesWithoutOverflow)
{
	World world;
	const std::array<Point, 8> expected{{{300, 0}, {270, 0}, {0, 150}, {190, 95}, {300, 0}, {0, 150}, {0, 150}, {187, 93}}};
	auto target = Target(); auto info = Info(); info.x0 = info.y0 = 0; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(8, effects.size());
	for (size_t i = 0; i < effects.size(); ++i)
	{
		const auto* linked = effects[i]->GetEffectTarget(); CHECK_EQ(expected[i].x, linked->GetX()); CHECK_EQ(expected[i].y, linked->GetY());
		CHECK_EQ(0, linked->GetZ()); CHECK_EQ(123, linked->GetID());
	}
}

TEST(SpreadOutEffectGenerator, NegativeFractionalSourcesKeepTruncationAndWrappedDestinations)
{
	World world;
	const std::array<Point, 8> expected{{{269, -1}, {269, -1}, {5, 149}, {186, 94}, {269, 4}, {-1, 149}, {-1, 149}, {186, 92}}};
	auto target = Target(); auto info = Info(); info.x0 = info.y0 = -1; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(8, effects.size());
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(expected[i].x, effects[i]->GetEffectTarget()->GetX()); CHECK_EQ(expected[i].y, effects[i]->GetEffectTarget()->GetY());
	}
}

TEST(SpreadOutEffectGenerator, ExtremeSourcesKeepDestinationsWithinTheirRequestedTravel)
{
	World world;
	for (const int x : {(std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()})
	{
		for (const int y : {(std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()})
		{
			ClearEffects(); auto target = Target(); auto info = Info(); info.x0 = x; info.y0 = y; info.step = 255; info.count = 65535; info.pEffectTarget = target.get();
			CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(8, effects.size());
			for (const auto& effect : effects)
			{
				const auto* linked = effect->GetEffectTarget();
				const auto dx = (static_cast<std::int64_t>(linked->GetX()) - x) * (x < 0 ? 1 : -1);
				const auto dy = (static_cast<std::int64_t>(linked->GetY()) - y) * (y < 0 ? 1 : -1);
				// The wrapped neighbors are near zero. Every direction travels inward,
				// about 7.6 million pixels per axis at this maximum count and speed.
				CHECK(dx >= 7000000 && dx <= 8000000); CHECK(dy >= 7000000 && dy <= 8000000);
				CHECK_EQ(0, linked->GetZ()); CHECK_EQ(123, linked->GetID());
			}
		}
	}
}

TEST(SpreadOutEffectGenerator, RepresentableFractionalSourceKeepsTheOriginalIntegerScaling)
{
	World world;
	const std::array<Point, 8> expected{{{-9, 91}, {-5, 184}, {85, 247}, {450, 228}, {492, 45}, {374, -12}, {181, 0}, {71, 37}}};
	auto target = Target(); auto info = Info(); info.x0 = 261; info.y0 = 130; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release();
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(expected[i].x, effects[i]->GetEffectTarget()->GetX()); CHECK_EQ(expected[i].y, effects[i]->GetEffectTarget()->GetY());
	}
}

TEST(SpreadOutEffectGenerator, SourcesPastSectorPeriodsKeepTheirOriginalPixelsForTrajectoryScaling)
{
	World world;
	auto target = Target(); auto info = Info(); info.x0 = 3145968; info.y0 = 1572984; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release();
	for (const auto& effect : effects)
	{
		CHECK_EQ(3145968, effect->GetPixelX()); CHECK_EQ(1572984, effect->GetPixelY());
		CHECK_EQ(3145781, effect->GetEffectTarget()->GetX()); CHECK_EQ(1572891, effect->GetEffectTarget()->GetY());
	}
}

TEST(SpreadOutEffectGenerator, WrappedDestinationsAlsoDriveTargetlessLinearMotion)
{
	World world;
	auto info = Info(); info.x0 = info.y0 = 0; CHECK(world.generator.Generate(info));
	CHECK(effects[DIRECTION_LEFT]->Update()); CHECK_EQ(10, effects[DIRECTION_LEFT]->GetPixelX()); CHECK_EQ(0, effects[DIRECTION_LEFT]->GetPixelY());
	CHECK(effects[DIRECTION_UP]->Update()); CHECK_EQ(0, effects[DIRECTION_UP]->GetPixelX()); CHECK_EQ(10, effects[DIRECTION_UP]->GetPixelY());
	CHECK(effects[DIRECTION_LEFTUP]->Update()); CHECK_EQ(8, effects[DIRECTION_LEFTUP]->GetPixelX()); CHECK_EQ(4, effects[DIRECTION_LEFTUP]->GetPixelY());
	for (const auto& effect : effects) CHECK(effect->GetEffectTarget() == nullptr);
}

TEST(SpreadOutEffectGenerator, ExtremeRejectionKeepsCallerCoordinatesWhileCopiesUseComputedDestinations)
{
	World world;
	acceptance = 254; auto target = Target(); auto info = Info(); info.x0 = (std::numeric_limits<int>::min)(); info.y0 = (std::numeric_limits<int>::max)();
	info.step = 255; info.count = 65535; info.pEffectTarget = target.get(); CHECK(!world.generator.Generate(info)); CHECK_EQ(7, effects.size());
	CHECK_EQ(777, target->GetX()); CHECK_EQ(888, target->GetY()); CHECK_EQ(999, target->GetZ()); CHECK_EQ(456, target->GetID()); CHECK(target->IsExistResult());
	for (const auto& effect : effects)
	{
		CHECK(effect->GetEffectTarget() != target.get()); CHECK(effect->GetEffectTarget()->GetX() < -2139000000);
		CHECK(effect->GetEffectTarget()->GetY() > 2139000000); CHECK_EQ(0, effect->GetEffectTarget()->GetZ());
	}
}
