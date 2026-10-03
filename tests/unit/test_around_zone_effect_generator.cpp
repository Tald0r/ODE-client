#include "test_framework.h"
#include "MAroundZoneEffectGenerator.h"
#include "MEffect.h"
#include "EffectSpriteTypeDef.h"

#include <array>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MAroundZoneEffectSprite sprite;
int spriteMask, acceptance, submissions, metadataReads;
std::vector<int> calls, requestedSprites, slots, removedTargets, observedRandom;
std::vector<DWORD> attemptedDelays, acceptedDelays;
std::vector<std::unique_ptr<MEffect>> effects;
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(3); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE frame) { calls.push_back(2); return 7 + frame; },
};
const MEffectTargetHost targetHost{.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); }};
const MAroundZoneEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MAroundZoneEffectSprite& result) {
		calls.push_back(1); requestedSprites.push_back(type); result = sprite; return (spriteMask & (1 << metadataReads++)) != 0;
	},
	.Queue = [](std::unique_ptr<MEffect> effect, DWORD delay) {
		calls.push_back(4); attemptedDelays.push_back(delay); CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		const int slot = submissions++; if ((acceptance & (1 << slot)) == 0) return false;
		slots.push_back(slot); acceptedDelays.push_back(delay); effects.push_back(std::move(effect)); return true;
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MAroundZoneEffectHost* previousGenerator = MAroundZoneEffectGenerator::SetHost(&host);
	MAroundZoneEffectGenerator generator;
	World()
	{
		frameNow = 100; sprite = {BLT_EFFECT, 12, 4}; spriteMask = acceptance = 255; submissions = metadataReads = 0;
		effects.clear(); calls.clear(); requestedSprites.clear(); slots.clear(); removedTargets.clear(); observedRandom.clear(); attemptedDelays.clear(); acceptedDelays.clear();
	}
	~World()
	{
		effects.clear(); MAroundZoneEffectGenerator::SetHost(previousGenerator); MEffectTarget::SetHost(previousTarget); MEffect::SetHost(previousEffect);
	}
};

EFFECTGENERATOR_INFO Info(TYPE_EFFECTSPRITETYPE type = 17)
{
	EFFECTGENERATOR_INFO info{}; info.nActionInfo = 42; info.effectSpriteType = type;
	info.x0 = 240; info.y0 = 120; info.z0 = 17; info.x1 = 900; info.y1 = 800; info.z1 = 99;
	info.direction = DIRECTION_RIGHTUP; info.step = 9; info.count = 30; info.linkCount = 5; info.power = 2; info.creatureID = 123;
	return info;
}

std::unique_ptr<MEffectTarget> Target(BYTE id = 73)
{
	auto target = std::make_unique<MEffectTarget>(3); target->NextPhase(); target->m_EffectID = id; target->Set(777, 888, 999, 456);
	target->SetServerID(789); target->SetDelayFrame(31); target->SetResultTime(); target->SetResult(new MActionResult); return target;
}

void ClearEffects()
{
	effects.clear(); calls.clear(); requestedSprites.clear(); slots.clear(); observedRandom.clear(); attemptedDelays.clear(); acceptedDelays.clear(); submissions = metadataReads = 0;
}

std::vector<int> RandomDraws(unsigned seed, size_t count)
{
	std::srand(seed); std::vector<int> values; for (size_t i = 0; i < count; ++i) values.push_back(std::rand()); std::srand(seed); return values;
}

template<class Predicate> unsigned SeedWhere(Predicate predicate)
{
	for (unsigned seed = 1; seed < 10000; ++seed) if (predicate(RandomDraws(seed, 16))) return seed;
	CHECK(false); return 0;
}

} // namespace

TEST(AroundZoneEffectGenerator, OrdinaryCallsConfigureTwoOrThreeRealEffectsAtTheDestination)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_AROUND_ZONE, world.generator.GetID());
	for (int count = 2; count <= 3; ++count)
	{
		ClearEffects(); const unsigned seed = SeedWhere([count](const auto& draws) { return draws[0] % 2 + 2 == count; }); const auto draws = RandomDraws(seed, 2);
		CHECK(world.generator.Generate(Info())); CHECK_EQ(count, effects.size()); CHECK_EQ(draws[1], std::rand()); CHECK(requestedSprites == std::vector<int>(count, 17));
		std::vector<int> expected; for (int i = 0; i < count; ++i) expected.insert(expected.end(), {1, 2, 3, 4}); CHECK(calls == expected);
		for (const auto& effect : effects)
		{
			CHECK_EQ(MEffect::EFFECT_SECTOR, effect->GetEffectType()); CHECK_EQ(BLT_EFFECT, effect->GetBltType()); CHECK_EQ(12, effect->GetFrameID()); CHECK_EQ(4, effect->GetMaxFrame());
			CHECK_EQ(0, effect->GetFrame()); CHECK_EQ(7, effect->GetLight()); CHECK_EQ(900, effect->GetPixelX()); CHECK_EQ(800, effect->GetPixelY()); CHECK_EQ(17, effect->GetPixelZ());
			CHECK_EQ(9, effect->GetStepPixel()); CHECK_EQ(DIRECTION_RIGHTUP, effect->GetDirection()); CHECK_EQ(2, effect->GetPower());
			CHECK_EQ(103, effect->GetEndFrame()); CHECK_EQ(104, effect->GetEndLinkFrame()); CHECK_EQ(42, effect->GetActionInfo());
			CHECK(!effect->IsMulti()); CHECK(!effect->IsDelayFrame()); CHECK(!effect->IsWaitFrame()); CHECK(effect->GetEffectTarget() == nullptr);
		}
		CHECK(attemptedDelays == std::vector<DWORD>(count, 0));
	}
}

TEST(AroundZoneEffectGenerator, FireUsesStepModuloFourForOneRandomizedQuadrant)
{
	World world;
	for (int step = 0; step < 8; ++step)
	{
		ClearEffects(); auto info = Info(EFFECTSPRITETYPE_POWER_OF_LAND_FIRE_2); info.step = static_cast<BYTE>(step); const auto draws = RandomDraws(67, 4);
		CHECK(world.generator.Generate(info)); CHECK_EQ(1, effects.size()); CHECK_EQ(EFFECTSPRITETYPE_POWER_OF_LAND_FIRE_2 + draws[0] % 2, requestedSprites.front());
		CHECK_EQ(900 + (step % 4 < 2 ? -1 : 1) * (draws[1] % 96 + 24), effects.front()->GetPixelX());
		CHECK_EQ(800 + (step % 2 == 0 ? -1 : 1) * (draws[2] % 48 + 24), effects.front()->GetPixelY());
		CHECK_EQ(step, effects.front()->GetStepPixel()); CHECK_EQ(draws[3], std::rand());
	}
}

TEST(AroundZoneEffectGenerator, GunDustKeepsItsVariantButResetsLaterPositions)
{
	World world;
	const unsigned seed = SeedWhere([](const auto& draws) { return draws[0] % 2 == 1 && draws[1] % 3 == 0 && draws[4] % 3 != 0; });
	const auto draws = RandomDraws(seed, 8); CHECK(world.generator.Generate(Info(EFFECTSPRITETYPE_GUN_DUST_1))); CHECK_EQ(3, effects.size());
	const std::array<int, 3> types{EFFECTSPRITETYPE_GUN_DUST_1, EFFECTSPRITETYPE_GUN_DUST_2, EFFECTSPRITETYPE_GUN_DUST_3};
	CHECK(requestedSprites == std::vector<int>({types[0], types[draws[4] % 3], types[draws[4] % 3]}));
	CHECK_EQ(900 + draws[2] % 96 - 48, effects[0]->GetPixelX()); CHECK_EQ(800 + draws[3] % 48 - 24, effects[0]->GetPixelY());
	CHECK_EQ(900 + draws[5] % 96 - 48, effects[1]->GetPixelX()); CHECK_EQ(800 + draws[6] % 48 - 24, effects[1]->GetPixelY());
	CHECK_EQ(900, effects[2]->GetPixelX()); CHECK_EQ(800, effects[2]->GetPixelY()); CHECK_EQ(draws[7], std::rand());
}

TEST(AroundZoneEffectGenerator, MoleShotRetainsTheSelectedVariantAcrossAttempts)
{
	World world;
	const unsigned seed = SeedWhere([](const auto& draws) { return draws[0] % 2 == 1 && draws[1] % 5 == 0 && draws[4] % 5 != 0; });
	const auto draws = RandomDraws(seed, 8); CHECK(world.generator.Generate(Info(EFFECTSPRITETYPE_MOLE_SHOT_1))); CHECK_EQ(3, effects.size());
	CHECK(requestedSprites == std::vector<int>({EFFECTSPRITETYPE_MOLE_SHOT_1, EFFECTSPRITETYPE_MOLE_SHOT_1 + draws[4] % 5, EFFECTSPRITETYPE_MOLE_SHOT_1 + draws[4] % 5}));
	CHECK_EQ(900 + draws[2] % 96 - 48, effects[0]->GetPixelX()); CHECK_EQ(800 + draws[3] % 48 - 24, effects[0]->GetPixelY());
	CHECK_EQ(900 + draws[5] % 96 - 48, effects[1]->GetPixelX()); CHECK_EQ(800 + draws[6] % 48 - 24, effects[1]->GetPixelY());
	CHECK_EQ(900, effects[2]->GetPixelX()); CHECK_EQ(800, effects[2]->GetPixelY()); CHECK_EQ(draws[7], std::rand());
}

TEST(AroundZoneEffectGenerator, TurretScrapAlwaysAttemptsFiveWithPositiveOffsetsOnlyAtItsBaseVariant)
{
	World world;
	const unsigned seed = SeedWhere([](const auto& draws) { return draws[0] % 5 == 0 && draws[3] % 5 != 0; }); const auto draws = RandomDraws(seed, 7);
	CHECK(world.generator.Generate(Info(EFFECTSPRITETYPE_INSTALL_TURRET_SCRAP1))); CHECK_EQ(5, effects.size());
	CHECK_EQ(EFFECTSPRITETYPE_INSTALL_TURRET_SCRAP1, requestedSprites[0]);
	CHECK_EQ(900 + draws[1] % 96, effects[0]->GetPixelX()); CHECK_EQ(800 + draws[2] % 48, effects[0]->GetPixelY());
	CHECK_EQ(900 + draws[4] % 96, effects[1]->GetPixelX()); CHECK_EQ(800 + draws[5] % 48, effects[1]->GetPixelY());
	for (int i = 1; i < 5; ++i) CHECK_EQ(EFFECTSPRITETYPE_INSTALL_TURRET_SCRAP1 + draws[3] % 5, requestedSprites[i]);
	for (int i = 2; i < 5; ++i) { CHECK_EQ(900, effects[i]->GetPixelX()); CHECK_EQ(800, effects[i]->GetPixelY()); }
	CHECK_EQ(draws[6], std::rand());
}

TEST(AroundZoneEffectGenerator, NonBaseVariantsUseTheOrdinaryAttemptCount)
{
	World world;
	for (const int type : {EFFECTSPRITETYPE_POWER_OF_LAND_FIRE_2 + 1, EFFECTSPRITETYPE_INSTALL_TURRET_SCRAP1 + 1, static_cast<int>(EFFECTSPRITETYPE_GUN_DUST_2), EFFECTSPRITETYPE_MOLE_SHOT_1 + 1})
	{
		ClearEffects(); const auto draws = RandomDraws(97, 2); CHECK(world.generator.Generate(Info(static_cast<TYPE_EFFECTSPRITETYPE>(type))));
		CHECK_EQ(draws[0] % 2 + 2, effects.size()); CHECK(requestedSprites == std::vector<int>(effects.size(), type)); CHECK_EQ(draws[1], std::rand());
		for (const auto& effect : effects) { CHECK_EQ(900, effect->GetPixelX()); CHECK_EQ(800, effect->GetPixelY()); }
	}
}

TEST(AroundZoneEffectGenerator, StreamDirectionsEmitSixSourceOffsetsWithPerAttemptWaiting)
{
	World world;
	struct Delta { int x, y; };
	const std::array<Delta, 8> directions{{{-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}}};
	for (int direction = 0; direction < 256; ++direction)
	{
		ClearEffects(); auto info = Info(EFFECTSPRITETYPE_SPIT_STREAM); info.direction = static_cast<BYTE>(direction); const auto draws = RandomDraws(101, 1);
		CHECK(world.generator.Generate(info)); CHECK_EQ(6, effects.size()); CHECK_EQ(draws[0], std::rand()); CHECK(attemptedDelays == std::vector<DWORD>({0, 1, 2, 3, 4, 5}));
		const Delta delta = direction < 8 ? directions[direction] : Delta{0, 0};
		std::vector<int> expected{1, 2, 3, 4}; for (int i = 1; i < 6; ++i) expected.insert(expected.end(), {1, 2, 3, 3, 3, 4}); CHECK(calls == expected);
		for (int i = 0; i < 6; ++i)
		{
			CHECK_EQ(240 + (i + 1) * 24 * delta.x, effects[i]->GetPixelX()); CHECK_EQ(120 + (i + 1) * 24 * delta.y, effects[i]->GetPixelY());
			CHECK_EQ(17, effects[i]->GetPixelZ()); CHECK_EQ(direction, effects[i]->GetDirection()); CHECK_EQ(i != 0, effects[i]->IsMulti());
			CHECK_EQ(103 + i, effects[i]->GetEndFrame()); CHECK_EQ(104, effects[i]->GetEndLinkFrame()); CHECK_EQ(i != 0, effects[i]->IsWaitFrame()); CHECK(!effects[i]->IsDelayFrame());
		}
	}
}

TEST(AroundZoneEffectGenerator, AxeThrowUsesThreeSourceEffectsAndNeighboringDirections)
{
	World world;
	for (int direction = 0; direction < 256; ++direction)
	{
		ClearEffects(); auto info = Info(EFFECTSPRITETYPE_GREAT_RUFFIAN_2_AXE_THROW); info.direction = static_cast<BYTE>(direction); const auto draws = RandomDraws(103, 1);
		CHECK(world.generator.Generate(info)); CHECK_EQ(3, effects.size()); CHECK_EQ(draws[0], std::rand());
		for (int i = 0; i < 3; ++i)
		{
			CHECK_EQ(240, effects[i]->GetPixelX()); CHECK_EQ(120, effects[i]->GetPixelY()); CHECK_EQ(17, effects[i]->GetPixelZ()); CHECK(effects[i]->IsMulti());
			CHECK_EQ((direction + i + 7) % 8, effects[i]->GetDirection()); CHECK_EQ(0, acceptedDelays[i]); CHECK_EQ(103, effects[i]->GetEndFrame());
		}
	}
}

TEST(AroundZoneEffectGenerator, EveryAcceptancePatternGivesTheFirstAcceptedEffectTheOriginal)
{
	World world;
	struct Pattern { TYPE_EFFECTSPRITETYPE type; int count; };
	const auto draws = RandomDraws(107, 1);
	for (const Pattern pattern : {Pattern{17, draws[0] % 2 + 2}, {EFFECTSPRITETYPE_POWER_OF_LAND_FIRE_2, 1}, {EFFECTSPRITETYPE_INSTALL_TURRET_SCRAP1, 5},
		{EFFECTSPRITETYPE_SPIT_STREAM, 6}, {EFFECTSPRITETYPE_GREAT_RUFFIAN_2_AXE_THROW, 3}})
	{
		for (int mask = 0; mask < (1 << pattern.count); ++mask)
		{
			ClearEffects(); acceptance = mask; std::srand(107); auto target = Target(); auto info = Info(pattern.type); info.pEffectTarget = target.get();
			CHECK_EQ(mask != 0, world.generator.Generate(info)); CHECK_EQ(pattern.count, submissions); bool originalOwned = false;
			for (size_t i = 0; i < effects.size(); ++i)
			{
				const auto* linked = effects[i]->GetEffectTarget(); CHECK(linked != nullptr); if (!linked) continue;
				originalOwned |= linked == target.get(); CHECK_EQ(i == 0, linked == target.get());
				CHECK_EQ(i == 0 ? 777 : 900, linked->GetX()); CHECK_EQ(i == 0 ? 888 : 800, linked->GetY()); CHECK_EQ(i == 0 ? 999 : 99, linked->GetZ()); CHECK_EQ(i == 0 ? 456 : 123, linked->GetID());
			}
			CHECK_EQ(mask != 0, originalOwned); if (originalOwned) target.release();
		}
	}
}

TEST(AroundZoneEffectGenerator, TargetlessStreamsReportAnyAcceptedEffect)
{
	World world;
	for (int mask = 0; mask < 64; ++mask)
	{
		ClearEffects(); acceptance = mask; CHECK_EQ(mask != 0, world.generator.Generate(Info(EFFECTSPRITETYPE_SPIT_STREAM))); CHECK_EQ(6, submissions);
		for (const auto& effect : effects) { CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(42, effect->GetActionInfo()); }
	}
}

TEST(AroundZoneEffectGenerator, CopiesPreservePhaseMetadataWhileTargetingDestinationHeight)
{
	World world;
	auto target = Target(); auto info = Info(EFFECTSPRITETYPE_GREAT_RUFFIAN_2_AXE_THROW); info.pEffectTarget = target.get(); CHECK(world.generator.Generate(info)); target.release();
	for (size_t i = 0; i < effects.size(); ++i)
	{
		const auto* linked = effects[i]->GetEffectTarget(); CHECK_EQ(1, linked->GetCurrentPhase()); CHECK_EQ(3, linked->GetMaxPhase()); CHECK_EQ(31, linked->GetDelayFrame()); CHECK_EQ(73, linked->GetEffectID());
		CHECK_EQ(i == 0, linked->IsExistResult()); CHECK_EQ(i == 0, linked->IsResultTime()); CHECK_EQ(i == 0 ? 789 : OBJECTID_NULL, linked->GetServerID());
		CHECK_EQ(i == 0 ? 777 : 900, linked->GetX()); CHECK_EQ(i == 0 ? 888 : 800, linked->GetY()); CHECK_EQ(i == 0 ? 999 : 99, linked->GetZ());
		for (size_t j = 0; j < i; ++j) CHECK(linked != effects[j]->GetEffectTarget());
	}
	ClearEffects(); CHECK(removedTargets == std::vector<int>(3, 73));
}

TEST(AroundZoneEffectGenerator, RejectedStreamAttemptsStillAdvanceWaitingAndLifetime)
{
	World world;
	acceptance = 40; auto target = Target(); auto info = Info(EFFECTSPRITETYPE_SPIT_STREAM); info.pEffectTarget = target.get(); CHECK(world.generator.Generate(info)); target.release();
	CHECK(slots == std::vector<int>({3, 5})); CHECK(acceptedDelays == std::vector<DWORD>({3, 5})); CHECK_EQ(106, effects[0]->GetEndFrame()); CHECK_EQ(108, effects[1]->GetEndFrame());
	CHECK(effects[0]->GetEffectTarget() == info.pEffectTarget); CHECK_EQ(999, effects[0]->GetEffectTarget()->GetZ()); CHECK_EQ(99, effects[1]->GetEffectTarget()->GetZ());
	frameNow = 103; CHECK(!effects[0]->IsWaitFrame()); CHECK(effects[1]->IsWaitFrame()); frameNow = 105; CHECK(!effects[1]->IsWaitFrame());
}

TEST(AroundZoneEffectGenerator, FireRandomizationPrecedesMetadataAndQueueCallbacks)
{
	World world;
	const MAroundZoneEffectHost consumingRandom{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MAroundZoneEffectSprite& result) { observedRandom.push_back(std::rand()); return host.Sprite(type, result); },
		.Queue = [](std::unique_ptr<MEffect> effect, DWORD delay) { observedRandom.push_back(std::rand()); return host.Queue(std::move(effect), delay); },
	};
	MAroundZoneEffectGenerator::SetHost(&consumingRandom); const auto draws = RandomDraws(109, 6); auto info = Info(EFFECTSPRITETYPE_POWER_OF_LAND_FIRE_2); info.step = 3;
	CHECK(world.generator.Generate(info)); CHECK_EQ(EFFECTSPRITETYPE_POWER_OF_LAND_FIRE_2 + draws[0] % 2, requestedSprites.front());
	CHECK_EQ(924 + draws[1] % 96, effects.front()->GetPixelX()); CHECK_EQ(824 + draws[2] % 48, effects.front()->GetPixelY());
	CHECK(observedRandom == std::vector<int>({draws[3], draws[4]})); CHECK_EQ(draws[5], std::rand());
}

TEST(AroundZoneEffectGenerator, QueueRandomnessInterleavesWithTheNextVariantSelection)
{
	World world;
	const MAroundZoneEffectHost consumingRandom{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect, DWORD delay) { observedRandom.push_back(std::rand()); return host.Queue(std::move(effect), delay); },
	};
	MAroundZoneEffectGenerator::SetHost(&consumingRandom);
	const unsigned seed = SeedWhere([](const auto& draws) { return draws[0] % 5 == 0 && draws[4] % 5 != 0; }); const auto draws = RandomDraws(seed, 12);
	CHECK(world.generator.Generate(Info(EFFECTSPRITETYPE_INSTALL_TURRET_SCRAP1)));
	CHECK_EQ(EFFECTSPRITETYPE_INSTALL_TURRET_SCRAP1 + draws[4] % 5, requestedSprites[1]); CHECK_EQ(900 + draws[5] % 96, effects[1]->GetPixelX());
	CHECK_EQ(800 + draws[6] % 48, effects[1]->GetPixelY()); CHECK(observedRandom == std::vector<int>({draws[3], draws[7], draws[8], draws[9], draws[10]})); CHECK_EQ(draws[11], std::rand());
}

TEST(AroundZoneEffectGenerator, MissingMetadataStillConsumesSelectionDrawsButConstructsNothing)
{
	World world;
	const MAroundZoneEffectHost empty{};
	for (const auto* service : {static_cast<const MAroundZoneEffectHost*>(nullptr), &empty, &host})
	{
		ClearEffects(); spriteMask = 0; MAroundZoneEffectGenerator::SetHost(service);
		const auto draws = RandomDraws(127, 2); CHECK(!world.generator.Generate(Info())); CHECK_EQ(draws[1], std::rand()); CHECK(effects.empty()); CHECK_EQ(0, submissions);
		CHECK(calls == (service == &host ? std::vector<int>(draws[0] % 2 + 2, 1) : std::vector<int>{}));
	}
	ClearEffects(); MAroundZoneEffectGenerator::SetHost(&host); const auto draws = RandomDraws(131, 4);
	CHECK(!world.generator.Generate(Info(EFFECTSPRITETYPE_POWER_OF_LAND_FIRE_2))); CHECK_EQ(draws[3], std::rand()); CHECK_EQ(1, metadataReads); CHECK(effects.empty());
}

TEST(AroundZoneEffectGenerator, MissingMetadataSkipsOnlyThatAttemptAndPreservesEarlierAcceptance)
{
	World world;
	for (int mask : {1, 62})
	{
		ClearEffects(); spriteMask = mask; auto target = Target(); auto info = Info(EFFECTSPRITETYPE_SPIT_STREAM); info.pEffectTarget = target.get();
		CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(6, metadataReads); CHECK_EQ(mask == 1 ? 1 : 5, submissions);
		CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget); CHECK_EQ(mask == 1 ? 0 : 1, acceptedDelays.front());
		CHECK_EQ(777, effects.front()->GetEffectTarget()->GetX());
	}
}

TEST(AroundZoneEffectGenerator, RejectedAndUnavailableAttemptsKeepVariantStateAndRandomOrder)
{
	World world;
	const unsigned seed = SeedWhere([](const auto& draws) { return draws[0] % 5 != 0; }); const auto draws = RandomDraws(seed, 4);
	spriteMask = 30; acceptance = 0; auto target = Target(); auto info = Info(EFFECTSPRITETYPE_INSTALL_TURRET_SCRAP1); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK_EQ(5, metadataReads); CHECK_EQ(4, submissions); CHECK_EQ(draws[3], std::rand());
	CHECK(requestedSprites == std::vector<int>(5, EFFECTSPRITETYPE_INSTALL_TURRET_SCRAP1 + draws[0] % 5)); CHECK_EQ(777, target->GetX()); CHECK(target->IsExistResult());
}

TEST(AroundZoneEffectGenerator, MissingQueueConstructsAndDiscardsEveryUnlinkedStreamEffect)
{
	World world;
	const MAroundZoneEffectHost noQueue{.Sprite = host.Sprite}; MAroundZoneEffectGenerator::SetHost(&noQueue);
	auto target = Target(); auto info = Info(EFFECTSPRITETYPE_SPIT_STREAM); info.pEffectTarget = target.get(); CHECK(!world.generator.Generate(info));
	CHECK_EQ(6, metadataReads); CHECK(effects.empty()); CHECK(removedTargets.empty()); CHECK(target->IsExistResult());
	std::vector<int> expected{1, 2, 3}; for (int i = 1; i < 6; ++i) expected.insert(expected.end(), {1, 2, 3, 3, 3}); CHECK(calls == expected);
}

TEST(AroundZoneEffectGenerator, SpriteCallbackCanReplaceSubmissionAndFutureMetadata)
{
	World world;
	const MAroundZoneEffectHost replacing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MAroundZoneEffectSprite& result) { MAroundZoneEffectGenerator::SetHost(&host); return host.Sprite(type, result); },
	};
	MAroundZoneEffectGenerator::SetHost(&replacing); CHECK(world.generator.Generate(Info(EFFECTSPRITETYPE_SPIT_STREAM))); CHECK_EQ(6, metadataReads); CHECK_EQ(6, effects.size());
}

TEST(AroundZoneEffectGenerator, QueueReplacementRefreshesMetadataForTheNextAttempt)
{
	World world;
	const MAroundZoneEffectHost replacing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect, DWORD delay) { sprite = {BLT_SHADOW, 21, 7}; MAroundZoneEffectGenerator::SetHost(&host); return host.Queue(std::move(effect), delay); },
	};
	MAroundZoneEffectGenerator::SetHost(&replacing); CHECK(world.generator.Generate(Info(EFFECTSPRITETYPE_SPIT_STREAM))); CHECK_EQ(6, effects.size());
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(i == 0 ? BLT_EFFECT : BLT_SHADOW, effects[i]->GetBltType()); CHECK_EQ(i == 0 ? 12 : 21, effects[i]->GetFrameID());
		CHECK_EQ(i == 0 ? 4 : 7, effects[i]->GetMaxFrame()); CHECK_EQ(i == 0 ? 103 : 106 + i, effects[i]->GetEndFrame());
	}
}

TEST(AroundZoneEffectGenerator, ServiceRemovalAfterAcceptanceKeepsTransferredOwnershipAndSuccess)
{
	World world;
	const MAroundZoneEffectHost removing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect, DWORD delay) { MAroundZoneEffectGenerator::SetHost(nullptr); return host.Queue(std::move(effect), delay); },
	};
	MAroundZoneEffectGenerator::SetHost(&removing); auto target = Target(); auto info = Info(EFFECTSPRITETYPE_SPIT_STREAM); info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(1, metadataReads); CHECK_EQ(1, effects.size()); CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget);
	CHECK(calls == std::vector<int>({1, 2, 3, 4})); ClearEffects(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(AroundZoneEffectGenerator, SpriteRemovalBeforeSubmissionDiscardsTheUnlinkedEffect)
{
	World world;
	const MAroundZoneEffectHost removing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MAroundZoneEffectSprite& result) { MAroundZoneEffectGenerator::SetHost(nullptr); return host.Sprite(type, result); },
		.Queue = host.Queue,
	};
	MAroundZoneEffectGenerator::SetHost(&removing); auto target = Target(); auto info = Info(EFFECTSPRITETYPE_SPIT_STREAM); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK(removedTargets.empty()); CHECK(target->IsExistResult()); CHECK(calls == std::vector<int>({1, 2, 3}));
}

TEST(AroundZoneEffectGenerator, RejectingQueuesDestroyTheirMarkersAndLeaveTheOriginal)
{
	World world;
	const MAroundZoneEffectHost rejecting{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect, DWORD delay) { attemptedDelays.push_back(delay); effect->SetLink(42, Target(94).release()); return false; },
	};
	MAroundZoneEffectGenerator::SetHost(&rejecting); auto target = Target(); auto info = Info(EFFECTSPRITETYPE_SPIT_STREAM); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(attemptedDelays == std::vector<DWORD>({0, 1, 2, 3, 4, 5})); CHECK(removedTargets == std::vector<int>(6, 94)); CHECK_EQ(777, target->GetX()); CHECK(target->IsExistResult());
}

TEST(AroundZoneEffectGenerator, FirstQueueExceptionDestroysItsEffectAndLeavesCallerOwnership)
{
	World world;
	const MAroundZoneEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect, DWORD) -> bool { effect->SetLink(42, Target(94).release()); throw std::runtime_error("queue"); },
	};
	MAroundZoneEffectGenerator::SetHost(&throwing); auto target = Target(); auto info = Info(EFFECTSPRITETYPE_SPIT_STREAM); info.pEffectTarget = target.get(); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, metadataReads); CHECK(removedTargets == std::vector<int>{94}); CHECK(target->IsExistResult());
}

TEST(AroundZoneEffectGenerator, LaterQueueExceptionPreservesTheTransferredOriginal)
{
	World world;
	const MAroundZoneEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect, DWORD delay) {
			if (delay == 1) { effect->SetLink(42, Target(94).release()); throw std::runtime_error("queue"); }
			return host.Queue(std::move(effect), delay);
		},
	};
	MAroundZoneEffectGenerator::SetHost(&throwing); auto target = Target(); auto info = Info(EFFECTSPRITETYPE_SPIT_STREAM); info.pEffectTarget = target.get(); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(2, metadataReads); CHECK_EQ(1, effects.size());
	if (!effects.empty()) { CHECK(effects.front()->GetEffectTarget() == target.get()); if (effects.front()->GetEffectTarget() == target.get()) target.release(); }
	CHECK(removedTargets == std::vector<int>{94}); ClearEffects(); CHECK(removedTargets == std::vector<int>({94, 73}));
}

TEST(AroundZoneEffectGenerator, MetadataExceptionAfterAcceptanceLeavesTheOriginalInItsEffect)
{
	World world;
	const MAroundZoneEffectHost throwing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MAroundZoneEffectSprite& result) { if (metadataReads == 1) throw std::runtime_error("sprite"); return host.Sprite(type, result); },
		.Queue = host.Queue,
	};
	MAroundZoneEffectGenerator::SetHost(&throwing); auto target = Target(); auto info = Info(EFFECTSPRITETYPE_SPIT_STREAM); info.pEffectTarget = target.get(); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, effects.size());
	if (!effects.empty()) { CHECK(effects.front()->GetEffectTarget() == target.get()); if (effects.front()->GetEffectTarget() == target.get()) target.release(); }
	CHECK(removedTargets.empty()); ClearEffects(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(AroundZoneEffectGenerator, FrameCountControlsLifetimeWhileAnimationNarrowsToAByte)
{
	World world;
	for (const int count : {-1, 0, 1, 256, 258, 65535})
	{
		ClearEffects(); sprite.maxFrames = count; auto info = Info(EFFECTSPRITETYPE_SPIT_STREAM); info.count = 0; info.linkCount = MAX_LINKCOUNT;
		const auto draws = RandomDraws(137, 1); CHECK(world.generator.Generate(info)); CHECK_EQ(draws[0], std::rand());
		for (size_t i = 0; i < effects.size(); ++i)
		{
			CHECK_EQ(static_cast<BYTE>(count), effects[i]->GetMaxFrame()); CHECK_EQ(0, effects[i]->GetFrame());
			CHECK_EQ(static_cast<DWORD>(99 + count + static_cast<int>(i)), effects[i]->GetEndFrame()); CHECK_EQ(effects[i]->GetEndFrame(), effects[i]->GetEndLinkFrame());
		}
	}
}

TEST(AroundZoneEffectGenerator, PowerStepAndInputCountDoNotChangeStreamPlacementOrLifetime)
{
	World world;
	for (const BYTE value : {static_cast<BYTE>(0), static_cast<BYTE>(255)})
	{
		ClearEffects(); auto info = Info(EFFECTSPRITETYPE_SPIT_STREAM); info.step = info.power = value; info.count = value ? 65535 : 0;
		CHECK(world.generator.Generate(info)); CHECK_EQ(6, effects.size());
		for (size_t i = 0; i < effects.size(); ++i) { CHECK_EQ(value, effects[i]->GetStepPixel()); CHECK_EQ(value, effects[i]->GetPower()); CHECK_EQ(264 + i * 24, effects[i]->GetPixelX()); CHECK_EQ(103 + i, effects[i]->GetEndFrame()); }
	}
}

TEST(AroundZoneEffectGenerator, RealEffectsStayStationaryWhileAnimationAndLightAdvance)
{
	World world;
	CHECK(world.generator.Generate(Info(EFFECTSPRITETYPE_GREAT_RUFFIAN_2_AXE_THROW)));
	for (int update = 1; update <= 4; ++update)
	{
		CHECK(effects.front()->Update()); CHECK_EQ(update % 4, effects.front()->GetFrame()); CHECK_EQ(7 + update % 4, effects.front()->GetLight());
		CHECK_EQ(240, effects.front()->GetPixelX()); CHECK_EQ(120, effects.front()->GetPixelY()); CHECK_EQ(17, effects.front()->GetPixelZ());
	}
	frameNow = 103; CHECK(!effects.front()->Update()); CHECK_EQ(1, effects.front()->GetFrame()); CHECK_EQ(8, effects.front()->GetLight());
}

TEST(AroundZoneEffectGenerator, NegativePositionsRemainPixelsAndCopiedTargetsKeepFullDestinationValues)
{
	World world;
	auto target = Target(); auto info = Info(); info.x1 = -49; info.y1 = -25; info.z1 = -3; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release();
	for (const auto& effect : effects) { CHECK_EQ(-49, effect->GetPixelX()); CHECK_EQ(-25, effect->GetPixelY()); CHECK_EQ(17, effect->GetPixelZ()); CHECK_EQ(65535, effect->GetX()); CHECK_EQ(65535, effect->GetY()); }
	CHECK_EQ(-49, effects.back()->GetEffectTarget()->GetX()); CHECK_EQ(-25, effects.back()->GetEffectTarget()->GetY()); CHECK_EQ(-3, effects.back()->GetEffectTarget()->GetZ());
}

TEST(AroundZoneEffectGenerator, WrappedClocksKeepUnsignedWaitAndLifetimeDeadlines)
{
	World world;
	frameNow = 0xFFFFFFFEu; CHECK(world.generator.Generate(Info(EFFECTSPRITETYPE_SPIT_STREAM)));
	for (size_t i = 0; i < effects.size(); ++i) { CHECK_EQ(1 + i, effects[i]->GetEndFrame()); CHECK_EQ(2, effects[i]->GetEndLinkFrame()); CHECK(effects[i]->IsEnd()); CHECK_EQ(i == 1, effects[i]->IsWaitFrame()); }
	frameNow = 0; CHECK(!effects.back()->IsEnd()); CHECK(effects.back()->IsWaitFrame());
}

TEST(AroundZoneEffectGenerator, MissingBaseServicesKeepZeroBasedDeadlinesAndInactiveEffects)
{
	World world;
	MEffect::SetHost(nullptr); CHECK(world.generator.Generate(Info(EFFECTSPRITETYPE_SPIT_STREAM)));
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(3 + i, effects[i]->GetEndFrame()); CHECK_EQ(4, effects[i]->GetEndLinkFrame()); CHECK_EQ(0, effects[i]->GetLight());
		CHECK(!effects[i]->IsWaitFrame()); CHECK(!effects[i]->IsDelayFrame()); CHECK(!effects[i]->Update()); CHECK_EQ(1, effects[i]->GetFrame());
	}
}
