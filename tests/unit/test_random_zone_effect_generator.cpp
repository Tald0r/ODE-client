#include "test_framework.h"
#include "MStopZoneRandomEffectGenerator.h"
#include "MStopZoneXEffectGenerator.h"
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
int requestedSprite, submissions, acceptance;
std::vector<int> calls, slots, removedTargets, observedRandom;
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
const MEffectTargetHost targetHost{.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); }};
const MFixedZoneEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& result) {
		calls.push_back(1); requestedSprite = type; result = sprite; return spriteAvailable;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		calls.push_back(4); attempted.push_back({effect->GetX(), effect->GetY()});
		CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		const int slot = submissions++;
		if ((acceptance & (1 << slot)) == 0) return false;
		slots.push_back(slot); effects.push_back(std::move(effect)); return true;
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MFixedZoneEffectHost* previousRandom = MStopZoneRandomEffectGenerator::SetHost(&host);
	MStopZoneRandomEffectGenerator generator;
	World()
	{
		frameNow = 100; sprite = {BLT_EFFECT, 12, 3}; spriteAvailable = true;
		requestedSprite = -1; submissions = 0; acceptance = 15;
		effects.clear(); calls.clear(); slots.clear(); removedTargets.clear(); attempted.clear(); observedRandom.clear();
	}
	~World()
	{
		effects.clear(); MStopZoneRandomEffectGenerator::SetHost(previousRandom);
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
	effects.clear(); calls.clear(); slots.clear(); attempted.clear(); observedRandom.clear(); submissions = 0;
}

std::vector<int> RandomDraws(unsigned seed, size_t count)
{
	std::srand(seed); std::vector<int> values;
	for (size_t i = 0; i < count; ++i) values.push_back(std::rand());
	std::srand(seed); return values;
}

} // namespace

TEST(RandomZoneEffectGenerator, ConfiguresFourRealEffectsInQuadrantOrder)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_STOP_ZONE_RANDOM, world.generator.GetID());
	const auto draws = RandomDraws(67, 9);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(4, effects.size()); CHECK_EQ(17, requestedSprite); CHECK_EQ(draws[8], std::rand());
	CHECK(attempted == std::vector<Point>({{4 - draws[0] % 3, 4 - draws[1] % 3}, {6 + draws[2] % 3, 4 - draws[3] % 3},
		{6 + draws[4] % 3, 6 + draws[5] % 3}, {4 - draws[6] % 3, 6 + draws[7] % 3}}));
	std::vector<int> expected{1}; for (int i = 0; i < 4; ++i) expected.insert(expected.end(), {2, 3, 4}); CHECK(calls == expected);
	for (size_t i = 0; i < effects.size(); ++i)
	{
		auto& effect = *effects[i]; CHECK_EQ(MEffect::EFFECT_SECTOR, effect.GetEffectType());
		CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame());
		CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight()); CHECK_EQ(attempted[i].x * 48, effect.GetPixelX());
		CHECK_EQ(attempted[i].y * 24, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ());
		CHECK_EQ(9, effect.GetStepPixel()); CHECK_EQ(DIRECTION_RIGHTUP, effect.GetDirection()); CHECK_EQ(2, effect.GetPower());
		CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame()); CHECK_EQ(42, effect.GetActionInfo());
		CHECK(effect.GetEffectTarget() == nullptr); CHECK(!effect.IsMulti()); CHECK(!effect.IsDelayFrame());
	}
}

TEST(RandomZoneEffectGenerator, EachQuadrantDrawsAfterThePreviousSubmission)
{
	World world;
	const MFixedZoneEffectHost consumingRandom{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { observedRandom.push_back(std::rand()); return host.Queue(std::move(effect)); },
	};
	MStopZoneRandomEffectGenerator::SetHost(&consumingRandom); const auto draws = RandomDraws(97, 13);
	CHECK(world.generator.Generate(Info())); CHECK(observedRandom == std::vector<int>({draws[2], draws[5], draws[8], draws[11]}));
	CHECK(attempted == std::vector<Point>({{4 - draws[0] % 3, 4 - draws[1] % 3}, {6 + draws[3] % 3, 4 - draws[4] % 3},
		{6 + draws[6] % 3, 6 + draws[7] % 3}, {4 - draws[9] % 3, 6 + draws[10] % 3}})); CHECK_EQ(draws[12], std::rand());
}

TEST(RandomZoneEffectGenerator, AllAcceptancePatternsKeepTheOriginalExclusiveToSlotZero)
{
	World world;
	for (int mask = 0; mask < 16; ++mask)
	{
		ClearEffects(); acceptance = mask; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK_EQ((mask & 1) != 0, world.generator.Generate(info)); CHECK_EQ(4, submissions);
		bool originalOwned = false;
		for (size_t i = 0; i < effects.size(); ++i)
		{
			const auto* linked = effects[i]->GetEffectTarget(); CHECK(linked != nullptr); if (!linked) continue;
			originalOwned |= linked == target.get(); CHECK_EQ(slots[i] == 0, linked == target.get());
			CHECK_EQ(777, linked->GetX()); CHECK_EQ(888, linked->GetY()); CHECK_EQ(999, linked->GetZ()); CHECK_EQ(456, linked->GetID());
		}
		CHECK_EQ((mask & 1) != 0, originalOwned); if (originalOwned) target.release();
	}
}

TEST(RandomZoneEffectGenerator, TargetlessAcceptancePatternsStillReportOnlySlotZero)
{
	World world;
	for (int mask = 0; mask < 16; ++mask)
	{
		ClearEffects(); acceptance = mask; CHECK_EQ((mask & 1) != 0, world.generator.Generate(Info())); CHECK_EQ(4, submissions);
		for (const auto& effect : effects) { CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(42, effect->GetActionInfo()); }
	}
}

TEST(RandomZoneEffectGenerator, LaterCopiesSurviveAfterARejectedFirstSlotLeavesTheCallerTarget)
{
	World world;
	acceptance = 14; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK_EQ(3, effects.size()); CHECK(removedTargets.empty()); target.reset();
	CHECK(removedTargets == std::vector<int>({73}));
	for (const auto& effect : effects)
	{
		CHECK_EQ(777, effect->GetEffectTarget()->GetX()); CHECK_EQ(888, effect->GetEffectTarget()->GetY()); CHECK(!effect->GetEffectTarget()->IsExistResult());
	}
	ClearEffects(); CHECK(removedTargets == std::vector<int>(4, 73));
}

TEST(RandomZoneEffectGenerator, CopiesKeepPhaseAndMetadataWithoutRetargeting)
{
	World world;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); CHECK(world.generator.Generate(info)); target.release();
	for (size_t i = 0; i < effects.size(); ++i)
	{
		const auto* linked = effects[i]->GetEffectTarget(); CHECK(linked != nullptr); if (!linked) continue;
		CHECK_EQ(i == 0, linked == info.pEffectTarget); CHECK_EQ(1, linked->GetCurrentPhase()); CHECK_EQ(3, linked->GetMaxPhase());
		CHECK_EQ(31, linked->GetDelayFrame()); CHECK_EQ(73, linked->GetEffectID());
		CHECK_EQ(i == 0, linked->IsExistResult()); CHECK_EQ(i == 0, linked->IsResultTime()); CHECK_EQ(i == 0 ? 789 : OBJECTID_NULL, linked->GetServerID());
		CHECK_EQ(777, linked->GetX()); CHECK_EQ(888, linked->GetY()); CHECK_EQ(999, linked->GetZ()); CHECK_EQ(456, linked->GetID());
		for (size_t j = 0; j < i; ++j) CHECK(linked != effects[j]->GetEffectTarget());
	}
}

TEST(RandomZoneEffectGenerator, ZeroAndMaximumPowerAndStepKeepFourRandomQuadrants)
{
	World world;
	for (const BYTE value : {static_cast<BYTE>(0), static_cast<BYTE>(255)})
	{
		ClearEffects(); auto info = Info(); info.power = info.step = value; info.direction = value; const auto draws = RandomDraws(127, 9);
		CHECK(world.generator.Generate(info)); CHECK_EQ(4, effects.size()); CHECK_EQ(draws[8], std::rand());
		for (const auto& effect : effects) { CHECK_EQ(value, effect->GetPower()); CHECK_EQ(value, effect->GetStepPixel()); CHECK_EQ(value, effect->GetDirection()); }
	}
}

TEST(RandomZoneEffectGenerator, NegativeFractionalTilesTruncateBeforeQuadrantOffsetsWrap)
{
	World world;
	auto info = Info(); info.x0 = info.y0 = -1; const auto draws = RandomDraws(151, 9);
	CHECK(world.generator.Generate(info));
	CHECK(attempted == std::vector<Point>({{65535 - draws[0] % 3, 65535 - draws[1] % 3}, {1 + draws[2] % 3, 65535 - draws[3] % 3},
		{1 + draws[4] % 3, 1 + draws[5] % 3}, {65535 - draws[6] % 3, 1 + draws[7] % 3}}));
}

TEST(RandomZoneEffectGenerator, NegativeWholeTilesNarrowBeforeAddingOffsets)
{
	World world;
	auto info = Info(); info.x0 = -48; info.y0 = -24; const auto draws = RandomDraws(179, 9);
	CHECK(world.generator.Generate(info));
	CHECK(attempted == std::vector<Point>({{65534 - draws[0] % 3, 65534 - draws[1] % 3}, {draws[2] % 3, 65534 - draws[3] % 3},
		{draws[4] % 3, draws[5] % 3}, {65534 - draws[6] % 3, draws[7] % 3}}));
}

TEST(RandomZoneEffectGenerator, WholeSectorPeriodsKeepPlacementAndTargetsUnchanged)
{
	World world;
	const auto draws = RandomDraws(199, 9); CHECK(world.generator.Generate(Info())); const auto expected = attempted;
	ClearEffects(); auto target = Target(); auto info = Info(); info.x0 += 3145728; info.y0 += 1572864; info.pEffectTarget = target.get();
	std::srand(199); CHECK(world.generator.Generate(info)); target.release(); CHECK(attempted == expected); CHECK_EQ(draws[8], std::rand());
	CHECK_EQ(777, effects.back()->GetEffectTarget()->GetX()); CHECK_EQ(888, effects.back()->GetEffectTarget()->GetY());
}

TEST(RandomZoneEffectGenerator, IntegerLimitSourcesNarrowSafelyBeforeOffsets)
{
	World world;
	for (int side = 0; side < 2; ++side)
	{
		ClearEffects(); auto info = Info(); info.x0 = side ? (std::numeric_limits<int>::min)() : (std::numeric_limits<int>::max)();
		info.y0 = side ? (std::numeric_limits<int>::max)() : (std::numeric_limits<int>::min)();
		const int x = side ? 21846 : 43690, y = side ? 21845 : 43691; const auto draws = RandomDraws(211, 9);
		CHECK(world.generator.Generate(info)); CHECK_EQ(draws[8], std::rand());
		CHECK(attempted == std::vector<Point>({{x - 1 - draws[0] % 3, y - 1 - draws[1] % 3}, {x + 1 + draws[2] % 3, y - 1 - draws[3] % 3},
			{x + 1 + draws[4] % 3, y + 1 + draws[5] % 3}, {x - 1 - draws[6] % 3, y + 1 + draws[7] % 3}}));
	}
}

TEST(RandomZoneEffectGenerator, MissingMetadataRejectsBeforeRandomnessOrConstruction)
{
	World world;
	const MFixedZoneEffectHost empty{};
	for (const auto* service : {static_cast<const MFixedZoneEffectHost*>(nullptr), &empty, &host})
	{
		ClearEffects(); MStopZoneRandomEffectGenerator::SetHost(service); spriteAvailable = false;
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); const auto draws = RandomDraws(223, 1);
		CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions); CHECK_EQ(draws[0], std::rand());
		CHECK(calls == (service == &host ? std::vector<int>{1} : std::vector<int>{})); CHECK(target->IsExistResult());
	}
}

TEST(RandomZoneEffectGenerator, MissingQueueStillConstructsFourEffectsAndDrawsEightOffsets)
{
	World world;
	const MFixedZoneEffectHost metadataOnly{.Sprite = host.Sprite}; MStopZoneRandomEffectGenerator::SetHost(&metadataOnly);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); const auto draws = RandomDraws(227, 9);
	CHECK(!world.generator.Generate(info)); CHECK_EQ(draws[8], std::rand()); CHECK(effects.empty()); CHECK(removedTargets.empty());
	CHECK(calls == std::vector<int>({1, 2, 3, 2, 3, 2, 3, 2, 3})); CHECK(target->IsExistResult());
}

TEST(RandomZoneEffectGenerator, SpriteCallbackCanReplaceTheQueue)
{
	World world;
	const MFixedZoneEffectHost replacing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& result) {
			MStopZoneRandomEffectGenerator::SetHost(&host); return host.Sprite(type, result);
		},
	};
	MStopZoneRandomEffectGenerator::SetHost(&replacing); CHECK(world.generator.Generate(Info())); CHECK_EQ(4, effects.size());
}

TEST(RandomZoneEffectGenerator, SpriteCallbackCanRemoveTheQueueBeforeDrawingOffsets)
{
	World world;
	const MFixedZoneEffectHost removing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& result) {
			MStopZoneRandomEffectGenerator::SetHost(nullptr); return host.Sprite(type, result);
		},
		.Queue = host.Queue,
	};
	MStopZoneRandomEffectGenerator::SetHost(&removing); const auto draws = RandomDraws(229, 9);
	CHECK(!world.generator.Generate(Info())); CHECK_EQ(draws[8], std::rand()); CHECK(effects.empty());
	CHECK(calls == std::vector<int>({1, 2, 3, 2, 3, 2, 3, 2, 3}));
}

TEST(RandomZoneEffectGenerator, QueueReplacementKeepsMetadataUntilTheNextGeneration)
{
	World world;
	const MFixedZoneEffectHost replacing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			sprite = {BLT_EFFECT, 21, 5}; MStopZoneRandomEffectGenerator::SetHost(&host); return host.Queue(std::move(effect));
		},
	};
	MStopZoneRandomEffectGenerator::SetHost(&replacing); CHECK(world.generator.Generate(Info())); CHECK_EQ(4, effects.size());
	for (const auto& effect : effects) { CHECK_EQ(12, effect->GetFrameID()); CHECK_EQ(3, effect->GetMaxFrame()); }
	ClearEffects(); CHECK(world.generator.Generate(Info())); CHECK_EQ(4, effects.size());
	for (const auto& effect : effects) { CHECK_EQ(21, effect->GetFrameID()); CHECK_EQ(5, effect->GetMaxFrame()); }
}

TEST(RandomZoneEffectGenerator, QueueRemovalKeepsTheOriginalAndConsumesRemainingDraws)
{
	World world;
	const MFixedZoneEffectHost removing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			MStopZoneRandomEffectGenerator::SetHost(nullptr); return host.Queue(std::move(effect));
		},
	};
	MStopZoneRandomEffectGenerator::SetHost(&removing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	const auto draws = RandomDraws(233, 9); CHECK(world.generator.Generate(info)); target.release();
	CHECK_EQ(1, effects.size()); CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget); CHECK_EQ(draws[8], std::rand());
	CHECK(calls == std::vector<int>({1, 2, 3, 4, 2, 3, 2, 3, 2, 3})); ClearEffects(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(RandomZoneEffectGenerator, InstallerIsIndependentOfTheFixedXPattern)
{
	World world;
	const MFixedZoneEffectHost sentinel{}; const auto* previousX = MStopZoneXEffectGenerator::SetHost(&sentinel);
	CHECK(MStopZoneRandomEffectGenerator::SetHost(nullptr) == &host);
	CHECK(MStopZoneXEffectGenerator::SetHost(previousX) == &sentinel);
	CHECK(!world.generator.Generate(Info())); CHECK(calls.empty());
}

TEST(RandomZoneEffectGenerator, RejectingQueuesDestroyTheirMarkersAndKeepTheCallerTarget)
{
	World world;
	const MFixedZoneEffectHost rejecting{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { ++submissions; effect->SetLink(42, Target(94).release()); return false; },
	};
	MStopZoneRandomEffectGenerator::SetHost(&rejecting); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	const auto draws = RandomDraws(239, 9); CHECK(!world.generator.Generate(info)); CHECK_EQ(4, submissions); CHECK_EQ(draws[8], std::rand());
	CHECK(removedTargets == std::vector<int>(4, 94)); CHECK(target->IsExistResult()); target.reset(); CHECK_EQ(73, removedTargets.back());
}

TEST(RandomZoneEffectGenerator, FirstQueueExceptionDestroysItsEffectAfterOnlyTwoDraws)
{
	World world;
	const MFixedZoneEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool {
			++submissions; effect->SetLink(42, Target(94).release()); throw std::runtime_error("queue");
		},
	};
	MStopZoneRandomEffectGenerator::SetHost(&throwing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	const auto draws = RandomDraws(241, 3); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, submissions); CHECK_EQ(draws[2], std::rand()); CHECK(removedTargets == std::vector<int>{94}); CHECK(target->IsExistResult());
}

TEST(RandomZoneEffectGenerator, LaterQueueExceptionPreservesTheTransferredOriginal)
{
	World world;
	const MFixedZoneEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			if (submissions == 1) { effect->SetLink(42, Target(94).release()); throw std::runtime_error("queue"); }
			return host.Queue(std::move(effect));
		},
	};
	MStopZoneRandomEffectGenerator::SetHost(&throwing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	const auto draws = RandomDraws(251, 5); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, effects.size()); CHECK_EQ(draws[4], std::rand());
	if (!effects.empty()) { CHECK(effects.front()->GetEffectTarget() == target.get()); if (effects.front()->GetEffectTarget() == target.get()) target.release(); }
	CHECK(removedTargets == std::vector<int>{94}); ClearEffects(); CHECK(removedTargets == std::vector<int>({94, 73}));
}

TEST(RandomZoneEffectGenerator, ExceptionAfterARejectedFirstSlotKeepsCallerAndCopiedTargetsSeparate)
{
	World world;
	const MFixedZoneEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			if (submissions == 2) { effect->SetLink(42, Target(94).release()); throw std::runtime_error("queue"); }
			return host.Queue(std::move(effect));
		},
	};
	MStopZoneRandomEffectGenerator::SetHost(&throwing); acceptance = 2; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	const auto draws = RandomDraws(257, 7); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, effects.size()); CHECK_EQ(draws[6], std::rand()); CHECK(target->IsExistResult());
	if (!effects.empty()) { CHECK(effects.front()->GetEffectTarget() != target.get()); CHECK_EQ(777, effects.front()->GetEffectTarget()->GetX()); }
	CHECK(removedTargets == std::vector<int>{94}); ClearEffects(); CHECK(removedTargets == std::vector<int>({94, 73}));
	target.reset(); CHECK(removedTargets == std::vector<int>({94, 73, 73}));
}

TEST(RandomZoneEffectGenerator, FrameCountsNarrowWithoutRandomAnimationStarts)
{
	World world;
	for (int frames : {-1, 0, 256, 258})
	{
		ClearEffects(); sprite.maxFrames = frames; const auto draws = RandomDraws(263, 9);
		CHECK(world.generator.Generate(Info())); CHECK_EQ(4, effects.size()); CHECK_EQ(draws[8], std::rand());
		for (const auto& effect : effects) { CHECK_EQ(static_cast<BYTE>(frames), effect->GetMaxFrame()); CHECK_EQ(0, effect->GetFrame()); }
	}
}

TEST(RandomZoneEffectGenerator, RealEffectsStayStationaryWhileAnimationAndLightAdvance)
{
	World world;
	CHECK(world.generator.Generate(Info())); const int x = effects.front()->GetPixelX(), y = effects.front()->GetPixelY();
	for (int update = 1; update <= 4; ++update)
	{
		CHECK(effects.front()->Update()); CHECK_EQ(update % 3, effects.front()->GetFrame()); CHECK_EQ(7 + update % 3, effects.front()->GetLight());
		CHECK_EQ(x, effects.front()->GetPixelX()); CHECK_EQ(y, effects.front()->GetPixelY()); CHECK_EQ(17, effects.front()->GetPixelZ());
	}
	frameNow = 129; CHECK(!effects.front()->Update()); CHECK_EQ(2, effects.front()->GetFrame()); CHECK_EQ(9, effects.front()->GetLight());
}

TEST(RandomZoneEffectGenerator, FiniteLifetimesAndIndependentLinkCountsKeepTheirDeadlines)
{
	World world;
	for (const WORD duration : {static_cast<WORD>(0), static_cast<WORD>(1), static_cast<WORD>(65535)})
	{
		ClearEffects(); auto info = Info(); info.count = duration; info.linkCount = MAX_LINKCOUNT;
		CHECK(world.generator.Generate(info));
		for (const auto& effect : effects) { CHECK_EQ(99u + duration, effect->GetEndFrame()); CHECK_EQ(99u + duration, effect->GetEndLinkFrame()); }
	}
	ClearEffects(); auto info = Info(); info.count = 1; info.linkCount = 65534; CHECK(world.generator.Generate(info));
	for (const auto& effect : effects) { CHECK_EQ(100, effect->GetEndFrame()); CHECK_EQ(65633, effect->GetEndLinkFrame()); }
}

TEST(RandomZoneEffectGenerator, WrappedClockKeepsUnsignedDeadlineBehavior)
{
	World world;
	frameNow = 0xFFFFFFFEu; CHECK(world.generator.Generate(Info()));
	for (const auto& effect : effects) { CHECK_EQ(27, effect->GetEndFrame()); CHECK_EQ(2, effect->GetEndLinkFrame()); CHECK(effect->IsEnd()); CHECK(!effect->IsDelayFrame()); }
	frameNow = 0; for (const auto& effect : effects) CHECK(!effect->IsEnd());
}

TEST(RandomZoneEffectGenerator, MissingBaseServicesKeepFiniteCountsAndInactiveEffects)
{
	World world;
	MEffect::SetHost(nullptr); CHECK(world.generator.Generate(Info()));
	for (const auto& effect : effects)
	{
		CHECK_EQ(29, effect->GetEndFrame()); CHECK_EQ(4, effect->GetEndLinkFrame()); CHECK_EQ(0, effect->GetLight());
		CHECK(effect->IsEnd()); CHECK(!effect->IsDelayFrame()); CHECK(!effect->Update()); CHECK_EQ(1, effect->GetFrame());
	}
}
