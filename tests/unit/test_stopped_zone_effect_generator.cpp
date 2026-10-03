#include "test_framework.h"
#include "MStopZoneEffectGenerator.h"
#include "MEffect.h"
#include "MEventQueue.h"
#include "EffectSpriteTypeDef.h"
#include "SkillDef.h"

#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MStopZoneEffectSprite sprite;
bool spriteAvailable, framesAvailable;
int maxFrames, requestedSprite, submissions, acceptanceMask;
std::vector<int> calls, slots, removedTargets, frameAtQueue;
std::vector<std::unique_ptr<MEffect>> effects;
std::vector<MEvent> events;
struct FrameRequest
{
	BYTE blt; TYPE_FRAMEID id;
	bool operator==(const FrameRequest&) const = default;
};
std::vector<FrameRequest> frameRequests;
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(5); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE frame) { calls.push_back(4); return 7 + frame; },
};
const MEffectTargetHost targetHost{
	.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); },
};
const MStopZoneEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MStopZoneEffectSprite& result) {
		calls.push_back(1); requestedSprite = type; result = sprite; return spriteAvailable;
	},
	.MaxFrames = [](BYTE blt, TYPE_FRAMEID frameID, int& count) {
		calls.push_back(3); frameRequests.push_back({blt, frameID}); count = maxFrames; return framesAvailable;
	},
	.AddEvent = [](MEvent& event) { calls.push_back(2); events.push_back(event); },
	.Queue = [](std::unique_ptr<MEffect> effect) {
		calls.push_back(6); frameAtQueue.push_back(effect->GetFrame());
		CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		const int slot = submissions++;
		if (!(acceptanceMask & (1 << slot))) return false;
		slots.push_back(slot); effects.push_back(std::move(effect)); return true;
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MStopZoneEffectHost* previousStop = MStopZoneEffectGenerator::SetHost(&host);
	MStopZoneEffectGenerator generator;
	World()
	{
		frameNow = 100; sprite = {BLT_EFFECT, 12, false}; spriteAvailable = framesAvailable = true;
		maxFrames = 3; requestedSprite = -1; submissions = 0; acceptanceMask = 31;
		effects.clear(); calls.clear(); slots.clear(); removedTargets.clear(); events.clear();
		frameAtQueue.clear(); frameRequests.clear(); std::srand(42);
	}
	~World()
	{
		effects.clear(); MStopZoneEffectGenerator::SetHost(previousStop);
		MEffectTarget::SetHost(previousTarget); MEffect::SetHost(previousEffect);
	}
};

EFFECTGENERATOR_INFO Info(TYPE_ACTIONINFO action = 42)
{
	EFFECTGENERATOR_INFO info{};
	info.nActionInfo = action; info.effectSpriteType = 17;
	info.x0 = 121; info.y0 = 73; info.z0 = 17;
	info.x1 = 260; info.y1 = 130; info.z1 = 99;
	info.direction = DIRECTION_RIGHTUP; info.step = 9; info.count = 30; info.linkCount = 5; info.power = 77;
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
	effects.clear(); slots.clear(); calls.clear(); frameAtQueue.clear(); submissions = 0;
}

} // namespace

TEST(StoppedZoneEffectGenerator, ConfiguresARealStationaryEffectAtTheSourceTileOrigin)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_STOP_ZONE, world.generator.GetID());
	CHECK(world.generator.Generate(Info())); CHECK_EQ(1, effects.size());
	auto& effect = *effects.front();
	CHECK_EQ(MEffect::EFFECT_SECTOR, effect.GetEffectType()); CHECK_EQ(BLT_EFFECT, effect.GetBltType());
	CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame()); CHECK_EQ(0, effect.GetFrame());
	CHECK_EQ(7, effect.GetLight()); CHECK_EQ(2, effect.GetX()); CHECK_EQ(3, effect.GetY());
	CHECK_EQ(96, effect.GetPixelX()); CHECK_EQ(72, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ());
	CHECK_EQ(9, effect.GetStepPixel()); CHECK_EQ(DIRECTION_RIGHTUP, effect.GetDirection());
	CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame());
	CHECK_EQ(77, effect.GetPower()); CHECK_EQ(42, effect.GetActionInfo()); CHECK(!effect.IsMulti());
	CHECK_EQ(17, requestedSprite); CHECK(frameRequests == std::vector<FrameRequest>({{BLT_EFFECT, 12}}));
	CHECK(calls == std::vector<int>({1, 3, 4, 5, 6}));
}

TEST(StoppedZoneEffectGenerator, UpdatesAnimationAndLightWithoutMoving)
{
	World world;
	CHECK(world.generator.Generate(Info())); auto& effect = *effects.front();
	CHECK(effect.Update()); CHECK_EQ(1, effect.GetFrame()); CHECK_EQ(8, effect.GetLight());
	CHECK_EQ(96, effect.GetPixelX()); CHECK_EQ(72, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ());
	CHECK_EQ(9, effect.GetStepPixel()); frameNow = 129;
	CHECK(!effect.Update()); CHECK_EQ(2, effect.GetFrame()); CHECK_EQ(9, effect.GetLight());
}

TEST(StoppedZoneEffectGenerator, SectorPlacementRetainsTruncationAndUnsignedNarrowing)
{
	World world;
	struct Case { int x, y, sectorX, sectorY, pixelX, pixelY; };
	for (const Case c : {Case{-1, -1, 0, 0, 0, 0}, {-48, -24, 65535, 65535, 3145680, 1572840},
		{3145728, 1572864, 0, 0, 0, 0}, {95, 47, 1, 1, 48, 24}})
	{
		ClearEffects(); auto info = Info(); info.x0 = c.x; info.y0 = c.y;
		CHECK(world.generator.Generate(info)); CHECK_EQ(c.sectorX, effects.front()->GetX());
		CHECK_EQ(c.sectorY, effects.front()->GetY()); CHECK_EQ(c.pixelX, effects.front()->GetPixelX());
		CHECK_EQ(c.pixelY, effects.front()->GetPixelY());
	}
}

TEST(StoppedZoneEffectGenerator, FireworksAndPetAttacksKeepExactPixelPlacement)
{
	World world;
	std::vector<int> types{EFFECTSPRITETYPE_FIRE_CRACKER_1, EFFECTSPRITETYPE_FIRE_CRACKER_2,
		EFFECTSPRITETYPE_FIRE_CRACKER_3, EFFECTSPRITETYPE_PET_SLAYER_3TH_ATTACK_1, EFFECTSPRITETYPE_PET_SLAYER_3TH_ATTACK_2};
	for (int type = EFFECTSPRITETYPE_DRAGON_FIRE_CRACKER; type <= EFFECTSPRITETYPE_FIRE_CRACKER_4; ++type) types.push_back(type);
	for (const int type : types)
	{
		ClearEffects(); auto info = Info(); info.effectSpriteType = static_cast<TYPE_EFFECTSPRITETYPE>(type);
		CHECK(world.generator.Generate(info)); CHECK_EQ(121, effects.front()->GetPixelX());
		CHECK_EQ(73, effects.front()->GetPixelY()); CHECK_EQ(17, effects.front()->GetPixelZ());
		const bool pet = type == EFFECTSPRITETYPE_PET_SLAYER_3TH_ATTACK_1 || type == EFFECTSPRITETYPE_PET_SLAYER_3TH_ATTACK_2;
		CHECK_EQ(!pet, effects.front()->IsMulti());
	}
}

TEST(StoppedZoneEffectGenerator, MultiEffectExceptionsKeepSectorPlacement)
{
	World world;
	for (const auto type : {EFFECTSPRITETYPE_GREAT_RUFFIAN_2_AXE_THROW, EFFECTSPRITETYPE_GREAT_RUFFIAN_2_AXE_THROW_SHADOW,
		EFFECTSPRITETYPE_GREAT_RUFFIAN_1_AXE_GROUND, EFFECTSPRITETYPE_GREAT_RUFFIAN_1_AXE_WAVE,
		EFFECTSPRITETYPE_NEW_PLASMA_ROCKET_LAUNCHER_BLOW})
	{
		ClearEffects(); auto info = Info(); info.effectSpriteType = type;
		CHECK(world.generator.Generate(info)); CHECK(effects.front()->IsMulti());
		CHECK_EQ(96, effects.front()->GetPixelX()); CHECK_EQ(72, effects.front()->GetPixelY());
	}
}

TEST(StoppedZoneEffectGenerator, SwordWavesAdjustFacingWithModuloEight)
{
	World world;
	struct Case { TYPE_EFFECTSPRITETYPE type; int offset; };
	for (const Case c : {Case{EFFECTSPRITETYPE_SWORD_WAVE_1, 1}, {EFFECTSPRITETYPE_SWORD_WAVE_2, 6}, {EFFECTSPRITETYPE_SWORD_WAVE_3, 1}})
		for (const int direction : {0, 1, 2, 3, 4, 5, 6, 7, 255})
		{
			ClearEffects(); auto info = Info(); info.effectSpriteType = c.type; info.direction = static_cast<BYTE>(direction);
			CHECK(world.generator.Generate(info)); CHECK_EQ((direction + c.offset) % 8, effects.front()->GetDirection());
		}
	ClearEffects(); auto info = Info(); info.direction = 255;
	CHECK(world.generator.Generate(info)); CHECK_EQ(255, effects.front()->GetDirection());
}

TEST(StoppedZoneEffectGenerator, DarknessVariantsRetainAllSixFamiliesAndTheirRandomConsumption)
{
	World world;
	struct Family { int input, output; };
	for (const Family family : {Family{EFFECTSPRITETYPE_DARKNESS_1_1, EFFECTSPRITETYPE_DARKNESS_1_1},
		{EFFECTSPRITETYPE_DARKNESS_2_1, EFFECTSPRITETYPE_DARKNESS_2_1}, {EFFECTSPRITETYPE_DARKNESS_3_1, EFFECTSPRITETYPE_DARKNESS_3_1},
		{EFFECTSPRITETYPE_GRAY_DARKNESS_1_1, EFFECTSPRITETYPE_GRAY_DARKNESS_1_1},
		{EFFECTSPRITETYPE_GRAY_DARKNESS_2_1, EFFECTSPRITETYPE_GRAY_DARKNESS_2_1},
		// The existing third gray family selects from ordinary darkness 3.
		{EFFECTSPRITETYPE_GRAY_DARKNESS_3_1, EFFECTSPRITETYPE_DARKNESS_3_1}})
		for (int variant = 0; variant < 5; ++variant)
		{
			ClearEffects(); const unsigned seed = 400 + variant; std::srand(seed);
			const int expected = family.output + std::rand() % 5; const int next = std::rand();
			std::srand(seed); auto info = Info(); info.effectSpriteType = static_cast<TYPE_EFFECTSPRITETYPE>(family.input + variant);
			CHECK(world.generator.Generate(info)); CHECK_EQ(expected, requestedSprite); CHECK_EQ(next, std::rand());
		}
}

TEST(StoppedZoneEffectGenerator, PreviousDarknessAddsFiveFramesWithoutResamplingTheSprite)
{
	World world;
	MEffect previous(BLT_NORMAL); previous.SetFrameID(90, 3);
	for (const auto type : {EFFECTSPRITETYPE_DARKNESS_1_1, EFFECTSPRITETYPE_DARKNESS_2_1, EFFECTSPRITETYPE_DARKNESS_3_1,
		EFFECTSPRITETYPE_GRAY_DARKNESS_1_1, EFFECTSPRITETYPE_GRAY_DARKNESS_2_1, EFFECTSPRITETYPE_GRAY_DARKNESS_3_1})
	{
		ClearEffects(); std::srand(7); const int next = std::rand(); std::srand(7);
		auto info = Info(); info.effectSpriteType = type; info.pPreviousEffect = &previous;
		CHECK(world.generator.Generate(info)); CHECK_EQ(type, requestedSprite); CHECK_EQ(95, effects.front()->GetFrameID());
		CHECK_EQ(95, frameRequests.back().id); CHECK_EQ(next, std::rand());
	}
}

TEST(StoppedZoneEffectGenerator, PreviousFrameNarrowingRetainsTheNullSentinelFallback)
{
	World world;
	struct Case { TYPE_FRAMEID previous, expected; };
	for (const Case c : {Case{65530, 12}, {65531, 0}, {65535, 4}})
	{
		ClearEffects(); MEffect previous(BLT_NORMAL); previous.SetFrameID(c.previous, 3);
		auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_DARKNESS_1_1; info.pPreviousEffect = &previous;
		CHECK(world.generator.Generate(info)); CHECK_EQ(c.expected, effects.front()->GetFrameID());
		CHECK_EQ(c.expected, frameRequests.back().id);
	}
}

TEST(StoppedZoneEffectGenerator, ThunderboltCenterSamplesThreeVariantsButNeighboursDoNot)
{
	World world;
	for (unsigned seed = 1; seed <= 20; ++seed)
	{
		ClearEffects(); std::srand(seed); const int expected = EFFECTSPRITETYPE_INFINITY_THUNDERBOLT_CENTER + std::rand() % 3;
		const int next = std::rand(); std::srand(seed);
		auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_INFINITY_THUNDERBOLT_CENTER;
		CHECK(world.generator.Generate(info)); CHECK_EQ(expected, requestedSprite); CHECK_EQ(next, std::rand());
	}
	for (int offset : {1, 2})
	{
		ClearEffects(); std::srand(8); const int next = std::rand(); std::srand(8);
		auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_INFINITY_THUNDERBOLT_CENTER + offset;
		CHECK(world.generator.Generate(info)); CHECK_EQ(info.effectSpriteType, requestedSprite); CHECK_EQ(next, std::rand());
	}
}

TEST(StoppedZoneEffectGenerator, MeteorEnqueuesItsShakeBeforeFramesAndSubmissionEvenOnRejection)
{
	World world;
	auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_METEOR_ROCK; acceptanceMask = 0;
	CHECK(!world.generator.Generate(info)); CHECK_EQ(1, events.size());
	const auto& event = events.front();
	CHECK_EQ(EVENTID_METEOR_SHAKE, event.eventID); CHECK_EQ(EVENTTYPE_ZONE, event.eventType);
	CHECK_EQ(63, event.eventDelay); CHECK_EQ(EVENTFLAG_SHAKE_SCREEN, event.eventFlag); CHECK_EQ(3, event.parameter3);
	CHECK_EQ(0, event.parameter1); CHECK_EQ(0, event.parameter2); CHECK_EQ(0, event.parameter4);
	CHECK_EQ(-1, event.showTime); CHECK_EQ(-1, event.totalTime); CHECK(event.m_StringsID.empty());
	CHECK(calls == std::vector<int>({1, 2, 3, 4, 5, 6})); CHECK(effects.empty());
}

TEST(StoppedZoneEffectGenerator, MissingFramesRetainAnAlreadySubmittedMeteorEvent)
{
	World world;
	auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_METEOR_ROCK; framesAvailable = false;
	CHECK(!world.generator.Generate(info)); CHECK_EQ(1, events.size()); CHECK(effects.empty());
	CHECK(calls == std::vector<int>({1, 2, 3}));
}

TEST(StoppedZoneEffectGenerator, StoneAugerCrossKeepsOrderAndCopiesOnlyLaterTargets)
{
	World world;
	struct Point { int x, y; };
	const Point expected[] = {{5, 6}, {6, 5}, {5, 5}, {5, 4}, {4, 5}};
	for (const auto action : {RESULT_SKILL_STONE_AUGER, RESULT_STEP_SKILL_STONE_AUGER_2, RESULT_STEP_SKILL_STONE_AUGER_3})
	{
		ClearEffects(); auto target = Target(); auto info = Info(action); info.pEffectTarget = target.get();
		CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(5, effects.size());
		for (int i = 0; i < 5; ++i)
		{
			auto& effect = *effects[i]; const auto* linked = effect.GetEffectTarget(); CHECK(linked != nullptr);
			CHECK_EQ(expected[i].x, effect.GetX()); CHECK_EQ(expected[i].y, effect.GetY()); CHECK_EQ(17, effect.GetPixelZ());
			CHECK(effect.IsMulti()); CHECK_EQ(action, effect.GetActionInfo()); CHECK_EQ(DIRECTION_RIGHTUP, effect.GetDirection());
			CHECK_EQ(777, linked->GetX()); CHECK_EQ(888, linked->GetY()); CHECK_EQ(999, linked->GetZ()); CHECK_EQ(456, linked->GetID());
			CHECK_EQ(1, linked->GetCurrentPhase()); CHECK_EQ(3, linked->GetMaxPhase()); CHECK_EQ(31, linked->GetDelayFrame());
			CHECK_EQ(73, linked->GetEffectID()); CHECK_EQ(i == 0, linked == info.pEffectTarget);
			CHECK_EQ(i == 0, linked->IsExistResult()); CHECK_EQ(i == 0, linked->IsResultTime());
			CHECK_EQ(i == 0 ? 789 : OBJECTID_NULL, linked->GetServerID());
			for (int j = 0; j < i; ++j) CHECK(linked != effects[j]->GetEffectTarget());
		}
	}
}

TEST(StoppedZoneEffectGenerator, TargetlessStoneAugerLeavesTheActionUnlinked)
{
	World world;
	CHECK(world.generator.Generate(Info(RESULT_SKILL_STONE_AUGER))); CHECK_EQ(5, effects.size());
	for (const auto& effect : effects)
	{
		CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
	}
}

TEST(StoppedZoneEffectGenerator, StoneAugerSectorOffsetsRetainBoundaryWrap)
{
	World world;
	auto info = Info(RESULT_SKILL_STONE_AUGER); info.x1 = info.y1 = 0;
	CHECK(world.generator.Generate(info));
	struct Point { int x, y; };
	const Point expected[] = {{0, 1}, {1, 0}, {0, 0}, {0, 65535}, {65535, 0}};
	for (int i = 0; i < 5; ++i)
	{
		CHECK_EQ(expected[i].x, effects[i]->GetX()); CHECK_EQ(expected[i].y, effects[i]->GetY());
	}
}

TEST(StoppedZoneEffectGenerator, RepeatingAnimationRandomizesOnlyAfterAcceptedSubmission)
{
	World world;
	sprite.repeatFrame = true; maxFrames = 7;
	std::srand(92); const int frame = std::rand() % 7; const int next = std::rand(); std::srand(92);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release();
	CHECK_EQ(0, frameAtQueue.front()); CHECK_EQ(frame, effects.front()->GetFrame()); CHECK_EQ(next, std::rand());
	CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget); CHECK_EQ(7, effects.front()->GetLight());
	CHECK(effects.front()->Update()); CHECK_EQ((frame + 1) % 7, effects.front()->GetFrame());
	CHECK_EQ(7 + (frame + 1) % 7, effects.front()->GetLight());
}

TEST(StoppedZoneEffectGenerator, RejectedEffectsDoNotConsumeAnimationRandomnessOrTargets)
{
	World world;
	sprite.repeatFrame = true; acceptanceMask = 0;
	std::srand(92); const int next = std::rand(); std::srand(92);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK_EQ(next, std::rand()); CHECK(effects.empty()); CHECK(removedTargets.empty());
	CHECK_EQ(777, target->GetX()); target.reset(); CHECK(removedTargets == std::vector<int>({73}));
}

TEST(StoppedZoneEffectGenerator, StoneAugerRandomizesAcceptedSlotsInSubmissionOrder)
{
	World world;
	sprite.repeatFrame = true;
	for (int mask = 1; mask < 32; mask += 2)
	{
		ClearEffects(); acceptanceMask = mask;
		std::srand(27); std::vector<int> frames, accepted;
		for (int slot = 0; slot < 5; ++slot) if (mask & (1 << slot)) { frames.push_back(std::rand() % 3); accepted.push_back(slot); }
		const int next = std::rand(); std::srand(27);
		auto target = Target(); auto info = Info(RESULT_SKILL_STONE_AUGER); info.pEffectTarget = target.get();
		CHECK(world.generator.Generate(info)); target.release();
		CHECK_EQ(5, submissions); CHECK(slots == accepted); CHECK_EQ(frames.size(), effects.size());
		for (size_t i = 0; i < effects.size(); ++i) CHECK_EQ(frames[i], effects[i]->GetFrame());
		CHECK_EQ(next, std::rand()); CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget);
		for (int frame : frameAtQueue) CHECK_EQ(0, frame);
	}
}

TEST(StoppedZoneEffectGenerator, VariantSamplingPrecedesMetadataRejection)
{
	World world;
	spriteAvailable = false;
	std::srand(62); const int variant = std::rand() % 5; const int next = std::rand(); std::srand(62);
	auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_DARKNESS_1_1;
	CHECK(!world.generator.Generate(info)); CHECK_EQ(EFFECTSPRITETYPE_DARKNESS_1_1 + variant, requestedSprite);
	CHECK_EQ(next, std::rand()); CHECK(calls == std::vector<int>({1})); CHECK(effects.empty());
}

TEST(StoppedZoneEffectGenerator, MissingMetadataHostsLeaveTheCallerTargetUntouched)
{
	World world;
	const MStopZoneEffectHost empty{}, queueOnly{.Queue = host.Queue};
	for (const auto* service : {static_cast<const MStopZoneEffectHost*>(nullptr), &empty, &queueOnly})
	{
		MStopZoneEffectGenerator::SetHost(service); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		const auto removed = removedTargets.size();
		CHECK(!world.generator.Generate(info)); CHECK_EQ(removed, removedTargets.size()); CHECK_EQ(777, target->GetX());
	}
	CHECK(calls.empty()); CHECK(effects.empty());
}

TEST(StoppedZoneEffectGenerator, MissingFrameLookupRejectsBeforeConstruction)
{
	World world;
	const MStopZoneEffectHost spriteOnly{.Sprite = host.Sprite, .Queue = host.Queue};
	MStopZoneEffectGenerator::SetHost(&spriteOnly);
	CHECK(!world.generator.Generate(Info())); CHECK(calls == std::vector<int>({1})); CHECK(effects.empty());
}

TEST(StoppedZoneEffectGenerator, MissingQueueReleasesTheUnlinkedOrdinaryEffect)
{
	World world;
	const MStopZoneEffectHost metadataOnly{.Sprite = host.Sprite, .MaxFrames = host.MaxFrames};
	MStopZoneEffectGenerator::SetHost(&metadataOnly);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK(removedTargets.empty());
	CHECK(calls == std::vector<int>({1, 3, 4, 5}));
}

TEST(StoppedZoneEffectGenerator, MissingEventOutputDoesNotRejectTheMeteor)
{
	World world;
	const MStopZoneEffectHost noEvent{.Sprite = host.Sprite, .MaxFrames = host.MaxFrames, .Queue = host.Queue};
	MStopZoneEffectGenerator::SetHost(&noEvent);
	auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_METEOR_ROCK;
	CHECK(world.generator.Generate(info)); CHECK(events.empty()); CHECK_EQ(1, effects.size());
	CHECK(calls == std::vector<int>({1, 3, 4, 5, 6}));
}

TEST(StoppedZoneEffectGenerator, MetadataRefreshesAndAnimationCountsRetainByteNarrowing)
{
	World world;
	CHECK(world.generator.Generate(Info())); sprite = {BLT_NORMAL, 23, false}; maxFrames = 258;
	CHECK(world.generator.Generate(Info())); CHECK_EQ(12, effects[0]->GetFrameID());
	CHECK_EQ(23, effects[1]->GetFrameID()); CHECK_EQ(BLT_NORMAL, effects[1]->GetBltType()); CHECK_EQ(2, effects[1]->GetMaxFrame());
	CHECK(frameRequests == std::vector<FrameRequest>({{BLT_EFFECT, 12}, {BLT_NORMAL, 23}}));
	CHECK(effects[1]->Update()); CHECK_EQ(1, effects[1]->GetFrame()); CHECK_EQ(8, effects[1]->GetLight());
}

TEST(StoppedZoneEffectGenerator, SpriteCallbackCanReplaceLaterServices)
{
	World world;
	const MStopZoneEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MStopZoneEffectSprite& result) {
			result = {BLT_NORMAL, 20, false}; MStopZoneEffectGenerator::SetHost(&host); return true;
		},
	};
	CHECK(MStopZoneEffectGenerator::SetHost(&changing) == &host);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(20, effects.front()->GetFrameID());
	CHECK_EQ(BLT_NORMAL, effects.front()->GetBltType()); CHECK(calls == std::vector<int>({3, 4, 5, 6}));
	CHECK(MStopZoneEffectGenerator::SetHost(nullptr) == &host);
}

TEST(StoppedZoneEffectGenerator, FrameCallbackCanRemoveTheQueueBeforeSubmission)
{
	World world;
	const MStopZoneEffectHost changing{
		.Sprite = host.Sprite,
		.MaxFrames = [](BYTE, TYPE_FRAMEID, int& count) { count = 3; MStopZoneEffectGenerator::SetHost(nullptr); return true; },
		.Queue = host.Queue,
	};
	MStopZoneEffectGenerator::SetHost(&changing);
	CHECK(!world.generator.Generate(Info())); CHECK(effects.empty()); CHECK(calls == std::vector<int>({1, 4, 5}));
}

TEST(StoppedZoneEffectGenerator, EventCallbackCanRemoveTheFrameServiceBeforeConstruction)
{
	World world;
	const MStopZoneEffectHost changing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.AddEvent = [](MEvent& event) { events.push_back(event); MStopZoneEffectGenerator::SetHost(nullptr); },
		.Queue = host.Queue,
	};
	MStopZoneEffectGenerator::SetHost(&changing);
	auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_METEOR_ROCK;
	CHECK(!world.generator.Generate(info)); CHECK_EQ(1, events.size()); CHECK(effects.empty());
	CHECK(calls == std::vector<int>({1}));
}

TEST(StoppedZoneEffectGenerator, RejectingQueuesDestroyTheirOwnAttachedMarker)
{
	World world;
	const MStopZoneEffectHost rejecting{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) { effect->SetLink(43, Target(74).release()); return false; },
	};
	MStopZoneEffectGenerator::SetHost(&rejecting);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(removedTargets == std::vector<int>({74}));
	target.reset(); CHECK(removedTargets == std::vector<int>({74, 73}));
}

TEST(StoppedZoneEffectGenerator, QueueExceptionsReleaseEffectsWithoutTakingTheOriginalTarget)
{
	World world;
	const MStopZoneEffectHost throwing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool {
			effect->SetLink(43, Target(74).release()); throw std::runtime_error("Queue failure");
		},
	};
	MStopZoneEffectGenerator::SetHost(&throwing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK(removedTargets == std::vector<int>({74}));
	target.reset(); CHECK(removedTargets == std::vector<int>({74, 73}));
}

TEST(StoppedZoneEffectGenerator, MissingBaseServicesRetainCountsAndInactiveFallback)
{
	World world;
	MEffect::SetHost(nullptr);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(29, effects.front()->GetEndFrame());
	CHECK_EQ(4, effects.front()->GetEndLinkFrame()); CHECK_EQ(0, effects.front()->GetLight()); CHECK(!effects.front()->Update());
}

TEST(StoppedZoneEffectGenerator, CountsRemainFiniteAndLinkTimingIsIndependent)
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

TEST(StoppedZoneEffectGenerator, StoneAugerReturnReportsOriginalOwnershipForEveryAcceptanceMask)
{
	World world;
	for (int mask = 0; mask < 32; ++mask)
	{
		ClearEffects(); acceptanceMask = mask;
		auto target = Target(); auto info = Info(RESULT_SKILL_STONE_AUGER); info.pEffectTarget = target.get();
		CHECK_EQ((mask & 1) != 0, world.generator.Generate(info)); CHECK_EQ(5, submissions);
		bool ownsOriginal = false;
		for (const auto& effect : effects) ownsOriginal |= effect->GetEffectTarget() == target.get();
		CHECK_EQ((mask & 1) != 0, ownsOriginal);
		// Red-run cleanup follows actual ownership rather than the reported result.
		if (ownsOriginal) target.release();
		else CHECK_EQ(777, target->GetX());
	}
}

TEST(StoppedZoneEffectGenerator, TargetlessStoneAugerReportsWhetherAnyEffectWasAccepted)
{
	World world;
	for (int mask = 0; mask < 32; ++mask)
	{
		ClearEffects(); acceptanceMask = mask;
		CHECK_EQ(mask != 0, world.generator.Generate(Info(RESULT_SKILL_STONE_AUGER)));
		CHECK_EQ(5, submissions); CHECK_EQ(mask == 0, effects.empty());
	}
}

TEST(StoppedZoneEffectGenerator, MissingQueueCannotClaimStoneAugerTargetOwnership)
{
	World world;
	const MStopZoneEffectHost metadataOnly{.Sprite = host.Sprite, .MaxFrames = host.MaxFrames};
	MStopZoneEffectGenerator::SetHost(&metadataOnly);
	auto target = Target(); auto info = Info(RESULT_SKILL_STONE_AUGER); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK(removedTargets.empty()); CHECK_EQ(0, submissions);
	CHECK_EQ(777, target->GetX());
}

TEST(StoppedZoneEffectGenerator, QueueRemovalBeforeOriginalAcceptanceRetainsCallerOwnership)
{
	World world;
	const MStopZoneEffectHost changing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect>) {
			++submissions; MStopZoneEffectGenerator::SetHost(nullptr); return false;
		},
	};
	MStopZoneEffectGenerator::SetHost(&changing);
	auto target = Target(); auto info = Info(RESULT_SKILL_STONE_AUGER); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK_EQ(1, submissions); CHECK(effects.empty()); CHECK(removedTargets.empty());
}

TEST(StoppedZoneEffectGenerator, QueueRemovalAfterOriginalAcceptanceKeepsTheReportedTransfer)
{
	World world;
	const MStopZoneEffectHost changing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			const bool accepted = host.Queue(std::move(effect)); MStopZoneEffectGenerator::SetHost(nullptr); return accepted;
		},
	};
	MStopZoneEffectGenerator::SetHost(&changing);
	auto target = Target(); auto info = Info(RESULT_SKILL_STONE_AUGER); info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); CHECK_EQ(1, effects.size());
	CHECK(effects.front()->GetEffectTarget() == target.get());
	if (effects.front()->GetEffectTarget() == target.get()) target.release();
	CHECK_EQ(1, submissions); effects.clear(); CHECK(removedTargets == std::vector<int>({73}));
}
