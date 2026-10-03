#include "test_framework.h"
#include "MStopZoneSelectableEffectGenerator.h"
#include "MEffect.h"
#include "EffectSpriteTypeDef.h"

#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MSelectableZoneEffectSprite sprite;
int maxFrames, requestedSprite, requestedFrame, requestedBlt, submissions, lightDirection, queueRandom;
bool spriteAvailable, framesAvailable, acceptEffect;
std::vector<int> calls, removedTargets;
std::vector<std::unique_ptr<MEffect>> effects;
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(4); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE direction, BYTE frame) { calls.push_back(3); lightDirection = direction; return 7 + frame; },
};
const MEffectTargetHost targetHost{.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); }};
const MSelectableZoneEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MSelectableZoneEffectSprite& result) {
		calls.push_back(1); requestedSprite = type; result = sprite; return spriteAvailable;
	},
	.MaxFrames = [](BYTE blt, TYPE_FRAMEID frame, int& result) {
		calls.push_back(2); requestedBlt = blt; requestedFrame = frame; result = maxFrames; return framesAvailable;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		calls.push_back(5); ++submissions; CHECK(effect->IsSelectable()); CHECK_EQ(0, effect->GetFrame());
		CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		if (!acceptEffect) return false;
		effects.push_back(std::move(effect)); return true;
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MSelectableZoneEffectHost* previousGenerator = MStopZoneSelectableEffectGenerator::SetHost(&host);
	MStopZoneSelectableEffectGenerator generator;
	World()
	{
		frameNow = 100; sprite = {BLT_EFFECT, 12, false}; maxFrames = 3;
		spriteAvailable = framesAvailable = acceptEffect = true;
		requestedSprite = requestedFrame = requestedBlt = lightDirection = queueRandom = -1; submissions = 0;
		effects.clear(); calls.clear(); removedTargets.clear();
	}
	~World()
	{
		effects.clear(); MStopZoneSelectableEffectGenerator::SetHost(previousGenerator);
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
	effects.clear(); calls.clear(); submissions = 0;
	requestedSprite = requestedFrame = requestedBlt = lightDirection = queueRandom = -1;
}

std::vector<int> RandomDraws(unsigned seed, size_t count)
{
	std::srand(seed); std::vector<int> values;
	for (size_t i = 0; i < count; ++i) values.push_back(std::rand());
	std::srand(seed); return values;
}

} // namespace

TEST(SelectableStationaryEffectGenerator, ConfiguresOneRealSelectableEffect)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_STOP_ZONE_SELECTABLE, world.generator.GetID()); const auto draws = RandomDraws(67, 1);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(1, effects.size()); CHECK_EQ(draws[0], std::rand());
	CHECK(calls == std::vector<int>({1, 2, 3, 4, 5})); CHECK_EQ(17, requestedSprite); CHECK_EQ(12, requestedFrame); CHECK_EQ(BLT_EFFECT, requestedBlt);
	auto& effect = *effects.front(); CHECK(effect.IsSelectable()); CHECK_EQ(MEffect::EFFECT_SECTOR, effect.GetEffectType());
	CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame());
	CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight()); CHECK_EQ(0, lightDirection);
	CHECK_EQ(5, effect.GetX()); CHECK_EQ(5, effect.GetY()); CHECK_EQ(240, effect.GetPixelX()); CHECK_EQ(120, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ());
	CHECK_EQ(9, effect.GetStepPixel()); CHECK_EQ(DIRECTION_RIGHTUP, effect.GetDirection()); CHECK_EQ(2, effect.GetPower());
	CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame()); CHECK_EQ(42, effect.GetActionInfo());
	CHECK(effect.GetEffectTarget() == nullptr); CHECK(!effect.IsMulti()); CHECK(!effect.IsDelayFrame());
}

TEST(SelectableStationaryEffectGenerator, AcceptanceTransfersTheOriginalWithoutChangingItsMetadata)
{
	World world;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); auto* result = target->GetResult();
	CHECK(world.generator.Generate(info)); target.release(); auto* linked = effects.front()->GetEffectTarget(); CHECK(linked == info.pEffectTarget);
	CHECK_EQ(777, linked->GetX()); CHECK_EQ(888, linked->GetY()); CHECK_EQ(999, linked->GetZ()); CHECK_EQ(456, linked->GetID());
	CHECK_EQ(1, linked->GetCurrentPhase()); CHECK_EQ(3, linked->GetMaxPhase()); CHECK_EQ(31, linked->GetDelayFrame());
	CHECK_EQ(73, linked->GetEffectID()); CHECK_EQ(789, linked->GetServerID()); CHECK(linked->IsResultTime()); CHECK(linked->GetResult() == result);
	ClearEffects(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(SelectableStationaryEffectGenerator, EveryDarknessPhaseUsesTheResolvedFrameFamily)
{
	World world;
	for (const int first : {EFFECTSPRITETYPE_DARKNESS_1_1, EFFECTSPRITETYPE_DARKNESS_2_1, EFFECTSPRITETYPE_DARKNESS_3_1,
		EFFECTSPRITETYPE_GRAY_DARKNESS_1_1, EFFECTSPRITETYPE_GRAY_DARKNESS_2_1, EFFECTSPRITETYPE_GRAY_DARKNESS_3_1})
	{
		for (int offset = 0; offset < 5; ++offset)
		{
			ClearEffects(); sprite.frameID = static_cast<TYPE_FRAMEID>(first + offset); const auto draws = RandomDraws(97, 2);
			CHECK(world.generator.Generate(Info())); CHECK_EQ(17, requestedSprite); CHECK_EQ(first + draws[0] % 5, requestedFrame);
			CHECK_EQ(requestedFrame, effects.front()->GetFrameID()); CHECK_EQ(draws[1], std::rand());
		}
	}
}

TEST(SelectableStationaryEffectGenerator, RequestedDarknessTypeDoesNotOverrideAnOrdinaryResolvedFrame)
{
	World world;
	auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_GRAY_DARKNESS_3_1; const auto draws = RandomDraws(101, 1);
	CHECK(world.generator.Generate(info)); CHECK_EQ(EFFECTSPRITETYPE_GRAY_DARKNESS_3_1, requestedSprite); CHECK_EQ(12, requestedFrame); CHECK_EQ(draws[0], std::rand());
}

TEST(SelectableStationaryEffectGenerator, PreviousDarknessFrameAdvancesFiveWithoutRandomization)
{
	World world;
	for (const TYPE_FRAMEID previousFrame : {static_cast<TYPE_FRAMEID>(EFFECTSPRITETYPE_DARKNESS_1_1 + 3), static_cast<TYPE_FRAMEID>(65534)})
	{
		ClearEffects(); MEffect previous(BLT_NORMAL); previous.SetFrameID(previousFrame, 7); calls.clear();
		auto info = Info(); info.pPreviousEffect = &previous; sprite.frameID = EFFECTSPRITETYPE_GRAY_DARKNESS_2_1;
		const auto draws = RandomDraws(103, 1); CHECK(world.generator.Generate(info));
		CHECK_EQ(static_cast<TYPE_FRAMEID>(previousFrame + 5), requestedFrame); CHECK_EQ(requestedFrame, effects.front()->GetFrameID()); CHECK_EQ(draws[0], std::rand());
		CHECK_EQ(previousFrame, previous.GetFrameID()); CHECK_EQ(7, previous.GetMaxFrame());
	}
}

TEST(SelectableStationaryEffectGenerator, OrdinaryFramesIgnorePreviousEffectsAndThunderboltSpecialCases)
{
	World world;
	MEffect previous(BLT_NORMAL); previous.SetFrameID(EFFECTSPRITETYPE_DARKNESS_1_1, 7);
	for (const int frame : {12, static_cast<int>(EFFECTSPRITETYPE_INFINITY_THUNDERBOLT_CENTER)})
	{
		ClearEffects(); sprite.frameID = static_cast<TYPE_FRAMEID>(frame); auto info = Info(); info.pPreviousEffect = &previous;
		const auto draws = RandomDraws(107, 1); CHECK(world.generator.Generate(info)); CHECK_EQ(frame, requestedFrame); CHECK_EQ(draws[0], std::rand());
	}
}

TEST(SelectableStationaryEffectGenerator, SwordWaveDirectionsUseFinalFramesForEveryDirectionByte)
{
	World world;
	for (const int frame : {EFFECTSPRITETYPE_SWORD_WAVE_1, EFFECTSPRITETYPE_SWORD_WAVE_2, EFFECTSPRITETYPE_SWORD_WAVE_3})
	{
		for (int direction = 0; direction < 256; ++direction)
		{
			ClearEffects(); sprite.frameID = static_cast<TYPE_FRAMEID>(frame); auto info = Info(); info.direction = static_cast<BYTE>(direction);
			CHECK(world.generator.Generate(info)); CHECK_EQ((direction + (frame == EFFECTSPRITETYPE_SWORD_WAVE_2 ? 6 : 1)) % 8, effects.front()->GetDirection());
		}
	}
}

TEST(SelectableStationaryEffectGenerator, PreviousFrameCanSelectASwordWaveBeforeDirectionAdjustment)
{
	World world;
	MEffect previous(BLT_NORMAL); previous.SetFrameID(EFFECTSPRITETYPE_SWORD_WAVE_2 - 5, 3);
	sprite.frameID = EFFECTSPRITETYPE_DARKNESS_1_1; auto info = Info(); info.pPreviousEffect = &previous; info.direction = 7;
	const auto draws = RandomDraws(109, 1); CHECK(world.generator.Generate(info)); CHECK_EQ(EFFECTSPRITETYPE_SWORD_WAVE_2, requestedFrame);
	CHECK_EQ(5, effects.front()->GetDirection()); CHECK_EQ(draws[0], std::rand());
}

TEST(SelectableStationaryEffectGenerator, OrdinaryFramesKeepZeroAndMaximumPowerStepAndDirection)
{
	World world;
	for (const BYTE value : {static_cast<BYTE>(0), static_cast<BYTE>(255)})
	{
		ClearEffects(); auto info = Info(); info.step = info.power = info.direction = value; CHECK(world.generator.Generate(info)); CHECK_EQ(1, submissions);
		CHECK_EQ(value, effects.front()->GetStepPixel()); CHECK_EQ(value, effects.front()->GetPower()); CHECK_EQ(value, effects.front()->GetDirection());
	}
}

TEST(SelectableStationaryEffectGenerator, SourceConversionTruncatesAndNarrowsWithoutUsingDestination)
{
	World world;
	struct Position { int x, y, expectedX, expectedY; };
	for (const Position p : {Position{-1, -1, 0, 0}, {-48, -24, 65535, 65535}, {3145989, 1572994, 5, 5},
		{(std::numeric_limits<int>::max)(), (std::numeric_limits<int>::min)(), 43690, 43691},
		{(std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)(), 21846, 21845}})
	{
		ClearEffects(); auto info = Info(); info.x0 = p.x; info.y0 = p.y; CHECK(world.generator.Generate(info));
		CHECK_EQ(p.expectedX, effects.front()->GetX()); CHECK_EQ(p.expectedY, effects.front()->GetY());
	}
}

TEST(SelectableStationaryEffectGenerator, RepeatStartFollowsAcceptanceWithoutRefreshingLight)
{
	World world;
	sprite.repeatFrame = true; maxFrames = 7; const auto draws = RandomDraws(127, 2);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(draws[0] % 7, effects.front()->GetFrame()); CHECK_EQ(draws[1], std::rand());
	CHECK_EQ(7, effects.front()->GetLight()); CHECK(calls == std::vector<int>({1, 2, 3, 4, 5}));
	CHECK(effects.front()->Update()); CHECK_EQ((draws[0] % 7 + 1) % 7, effects.front()->GetFrame()); CHECK_EQ(7 + effects.front()->GetFrame(), effects.front()->GetLight());
	CHECK_EQ(DIRECTION_RIGHTUP, lightDirection);
}

TEST(SelectableStationaryEffectGenerator, QueueRandomnessPrecedesTheRepeatStartDraw)
{
	World world;
	const MSelectableZoneEffectHost consumingRandom{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) { queueRandom = std::rand(); return host.Queue(std::move(effect)); },
	};
	MStopZoneSelectableEffectGenerator::SetHost(&consumingRandom); sprite.repeatFrame = true; maxFrames = 7; const auto draws = RandomDraws(131, 3);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(draws[0], queueRandom); CHECK_EQ(draws[1] % 7, effects.front()->GetFrame()); CHECK_EQ(draws[2], std::rand());
}

TEST(SelectableStationaryEffectGenerator, DarknessSelectionAndAcceptedRepeatConsumeTwoDistinctDraws)
{
	World world;
	sprite.frameID = EFFECTSPRITETYPE_GRAY_DARKNESS_3_1; sprite.repeatFrame = true; maxFrames = 7; const auto draws = RandomDraws(137, 3);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(EFFECTSPRITETYPE_GRAY_DARKNESS_3_1 + draws[0] % 5, requestedFrame);
	CHECK_EQ(draws[1] % 7, effects.front()->GetFrame()); CHECK_EQ(draws[2], std::rand());
}

TEST(SelectableStationaryEffectGenerator, RejectionKeepsTheCallerTargetAndSkipsRepeatRandomness)
{
	World world;
	acceptEffect = false; sprite.repeatFrame = true; sprite.frameID = EFFECTSPRITETYPE_DARKNESS_2_1;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); const auto draws = RandomDraws(139, 2);
	CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK_EQ(1, submissions); CHECK(removedTargets.empty()); CHECK(target->IsExistResult());
	CHECK_EQ(EFFECTSPRITETYPE_DARKNESS_2_1 + draws[0] % 5, requestedFrame); CHECK_EQ(draws[1], std::rand());
	ClearEffects(); const auto next = RandomDraws(149, 2); CHECK(!world.generator.Generate(Info())); CHECK_EQ(next[1], std::rand());
}

TEST(SelectableStationaryEffectGenerator, FullFrameCountControlsTheDrawBeforeByteAnimationWrapping)
{
	World world;
	sprite.repeatFrame = true;
	for (const int count : {-1, -7, 0, 1, 256, 258, 511})
	{
		ClearEffects(); maxFrames = count; const auto draws = RandomDraws(151, 2); CHECK(world.generator.Generate(Info()));
		const int steps = count == 0 ? 0 : draws[0] % count; const BYTE narrowed = static_cast<BYTE>(count);
		CHECK_EQ(narrowed, effects.front()->GetMaxFrame()); CHECK_EQ(steps % (narrowed ? narrowed : 256), effects.front()->GetFrame());
		CHECK_EQ(draws[count == 0 ? 0 : 1], std::rand()); CHECK_EQ(7, effects.front()->GetLight());
	}
}

TEST(SelectableStationaryEffectGenerator, MissingSpriteMetadataRejectsBeforeAnyRandomDraw)
{
	World world;
	const MSelectableZoneEffectHost empty{};
	for (const auto* service : {static_cast<const MSelectableZoneEffectHost*>(nullptr), &empty, &host})
	{
		ClearEffects(); MStopZoneSelectableEffectGenerator::SetHost(service); spriteAvailable = false; sprite.frameID = EFFECTSPRITETYPE_DARKNESS_1_1;
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); const auto draws = RandomDraws(157, 1);
		CHECK(!world.generator.Generate(info)); CHECK_EQ(draws[0], std::rand()); CHECK(effects.empty()); CHECK_EQ(0, submissions); CHECK(target->IsExistResult());
		CHECK(calls == (service == &host ? std::vector<int>{1} : std::vector<int>{}));
	}
}

TEST(SelectableStationaryEffectGenerator, MissingFrameCountRejectsAfterDarknessSelection)
{
	World world;
	const MSelectableZoneEffectHost noFrames{.Sprite = host.Sprite, .Queue = host.Queue};
	for (const auto* service : {&noFrames, &host})
	{
		ClearEffects(); MStopZoneSelectableEffectGenerator::SetHost(service); framesAvailable = false;
		sprite.frameID = EFFECTSPRITETYPE_GRAY_DARKNESS_2_1; sprite.repeatFrame = true; const auto draws = RandomDraws(163, 2);
		CHECK(!world.generator.Generate(Info())); CHECK_EQ(draws[1], std::rand()); CHECK(effects.empty()); CHECK_EQ(0, submissions);
		CHECK(calls == (service == &host ? std::vector<int>({1, 2}) : std::vector<int>{1}));
		if (service == &host) CHECK_EQ(EFFECTSPRITETYPE_GRAY_DARKNESS_2_1 + draws[0] % 5, requestedFrame);
	}
}

TEST(SelectableStationaryEffectGenerator, MissingQueueDestroysTheUnlinkedEffectWithoutRepeatRandomness)
{
	World world;
	const MSelectableZoneEffectHost noQueue{.Sprite = host.Sprite, .MaxFrames = host.MaxFrames};
	MStopZoneSelectableEffectGenerator::SetHost(&noQueue); sprite.repeatFrame = true;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); const auto draws = RandomDraws(167, 1);
	CHECK(!world.generator.Generate(info)); CHECK_EQ(draws[0], std::rand()); CHECK(effects.empty()); CHECK(removedTargets.empty()); CHECK(target->IsExistResult());
	CHECK(calls == std::vector<int>({1, 2, 3, 4}));
}

TEST(SelectableStationaryEffectGenerator, SpriteCallbackCanReplaceTheRemainingServices)
{
	World world;
	const MSelectableZoneEffectHost replacing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MSelectableZoneEffectSprite& result) {
			MStopZoneSelectableEffectGenerator::SetHost(&host); return host.Sprite(type, result);
		},
	};
	MStopZoneSelectableEffectGenerator::SetHost(&replacing); CHECK(world.generator.Generate(Info())); CHECK_EQ(1, effects.size());
	CHECK(calls == std::vector<int>({1, 2, 3, 4, 5}));
}

TEST(SelectableStationaryEffectGenerator, SpriteRemovalStillSelectsTheVariantBeforeMissingFramesReject)
{
	World world;
	const MSelectableZoneEffectHost removing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MSelectableZoneEffectSprite& result) {
			MStopZoneSelectableEffectGenerator::SetHost(nullptr); return host.Sprite(type, result);
		},
		.MaxFrames = host.MaxFrames, .Queue = host.Queue,
	};
	MStopZoneSelectableEffectGenerator::SetHost(&removing); sprite.frameID = EFFECTSPRITETYPE_DARKNESS_3_1; const auto draws = RandomDraws(173, 2);
	CHECK(!world.generator.Generate(Info())); CHECK_EQ(draws[1], std::rand()); CHECK(calls == std::vector<int>{1}); CHECK(effects.empty());
}

TEST(SelectableStationaryEffectGenerator, FrameCallbackCanReplaceTheQueueWithoutRefreshingSpriteMetadata)
{
	World world;
	const MSelectableZoneEffectHost replacing{
		.Sprite = host.Sprite,
		.MaxFrames = [](BYTE blt, TYPE_FRAMEID frame, int& result) {
			sprite = {BLT_SHADOW, 21, true}; MStopZoneSelectableEffectGenerator::SetHost(&host); return host.MaxFrames(blt, frame, result);
		},
	};
	MStopZoneSelectableEffectGenerator::SetHost(&replacing); const auto draws = RandomDraws(179, 1); CHECK(world.generator.Generate(Info()));
	CHECK_EQ(BLT_EFFECT, effects.front()->GetBltType()); CHECK_EQ(12, effects.front()->GetFrameID()); CHECK_EQ(draws[0], std::rand());
	ClearEffects(); CHECK(world.generator.Generate(Info())); CHECK_EQ(BLT_SHADOW, effects.front()->GetBltType()); CHECK_EQ(21, effects.front()->GetFrameID());
}

TEST(SelectableStationaryEffectGenerator, FrameCallbackRemovalStillConstructsAndDiscardsTheEffect)
{
	World world;
	const MSelectableZoneEffectHost removing{
		.Sprite = host.Sprite,
		.MaxFrames = [](BYTE blt, TYPE_FRAMEID frame, int& result) {
			MStopZoneSelectableEffectGenerator::SetHost(nullptr); return host.MaxFrames(blt, frame, result);
		},
		.Queue = host.Queue,
	};
	MStopZoneSelectableEffectGenerator::SetHost(&removing); sprite.repeatFrame = true; const auto draws = RandomDraws(181, 1);
	CHECK(!world.generator.Generate(Info())); CHECK_EQ(draws[0], std::rand()); CHECK(calls == std::vector<int>({1, 2, 3, 4})); CHECK(effects.empty());
}

TEST(SelectableStationaryEffectGenerator, QueueRemovalStillLinksAndRandomizesAnAcceptedEffect)
{
	World world;
	const MSelectableZoneEffectHost removing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) { MStopZoneSelectableEffectGenerator::SetHost(nullptr); return host.Queue(std::move(effect)); },
	};
	MStopZoneSelectableEffectGenerator::SetHost(&removing); sprite.repeatFrame = true; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	const auto draws = RandomDraws(191, 2); CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(1, effects.size());
	CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget); CHECK_EQ(draws[0] % 3, effects.front()->GetFrame()); CHECK_EQ(draws[1], std::rand());
	CHECK(!world.generator.Generate(Info())); ClearEffects(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(SelectableStationaryEffectGenerator, RejectingQueueDestroysItsOwnMarkerAndLeavesTheCallerTarget)
{
	World world;
	const MSelectableZoneEffectHost rejecting{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) { ++submissions; effect->SetLink(42, Target(94).release()); return false; },
	};
	MStopZoneSelectableEffectGenerator::SetHost(&rejecting); sprite.repeatFrame = true; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	const auto draws = RandomDraws(193, 1); CHECK(!world.generator.Generate(info)); CHECK_EQ(1, submissions); CHECK_EQ(draws[0], std::rand());
	CHECK(removedTargets == std::vector<int>{94}); CHECK(target->IsExistResult()); target.reset(); CHECK(removedTargets == std::vector<int>({94, 73}));
}

TEST(SelectableStationaryEffectGenerator, ThrowingFrameLookupKeepsCallerOwnershipAfterVariantSelection)
{
	World world;
	const MSelectableZoneEffectHost throwing{
		.Sprite = host.Sprite,
		.MaxFrames = [](BYTE, TYPE_FRAMEID, int&) -> bool { throw std::runtime_error("frames"); },
		.Queue = host.Queue,
	};
	MStopZoneSelectableEffectGenerator::SetHost(&throwing); sprite.frameID = EFFECTSPRITETYPE_DARKNESS_1_1;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); const auto draws = RandomDraws(197, 2); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(draws[1], std::rand()); CHECK(effects.empty()); CHECK(removedTargets.empty()); CHECK(target->IsExistResult());
	CHECK(calls == std::vector<int>{1});
}

TEST(SelectableStationaryEffectGenerator, ThrowingQueueDestroysItsEffectBeforeLinkingOrRepeating)
{
	World world;
	const MSelectableZoneEffectHost throwing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool { effect->SetLink(42, Target(94).release()); throw std::runtime_error("queue"); },
	};
	MStopZoneSelectableEffectGenerator::SetHost(&throwing); sprite.repeatFrame = true;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); const auto draws = RandomDraws(199, 1); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(draws[0], std::rand()); CHECK(effects.empty()); CHECK(removedTargets == std::vector<int>{94}); CHECK(target->IsExistResult());
}

TEST(SelectableStationaryEffectGenerator, RealSelectableEffectRemainsStationaryUntilItsFiniteDeadline)
{
	World world;
	CHECK(world.generator.Generate(Info()));
	for (int update = 1; update <= 4; ++update)
	{
		CHECK(effects.front()->Update()); CHECK_EQ(update % 3, effects.front()->GetFrame()); CHECK_EQ(7 + update % 3, effects.front()->GetLight());
		CHECK_EQ(240, effects.front()->GetPixelX()); CHECK_EQ(120, effects.front()->GetPixelY()); CHECK_EQ(17, effects.front()->GetPixelZ()); CHECK(effects.front()->IsSelectable());
	}
	frameNow = 129; CHECK(!effects.front()->Update()); CHECK_EQ(2, effects.front()->GetFrame()); CHECK_EQ(9, effects.front()->GetLight());
}

TEST(SelectableStationaryEffectGenerator, FiniteCountsAndLinkSentinelsKeepTheirDeadlines)
{
	World world;
	for (const WORD duration : {static_cast<WORD>(0), static_cast<WORD>(1), static_cast<WORD>(65535)})
	{
		ClearEffects(); auto info = Info(); info.count = duration; info.linkCount = MAX_LINKCOUNT; CHECK(world.generator.Generate(info));
		CHECK_EQ(99u + duration, effects.front()->GetEndFrame()); CHECK_EQ(99u + duration, effects.front()->GetEndLinkFrame());
	}
	ClearEffects(); auto info = Info(); info.count = 1; info.linkCount = 65534; CHECK(world.generator.Generate(info));
	CHECK_EQ(100, effects.front()->GetEndFrame()); CHECK_EQ(65633, effects.front()->GetEndLinkFrame());
}

TEST(SelectableStationaryEffectGenerator, WrappedClockAndMissingBaseServicesKeepTheirFallbacks)
{
	World world;
	frameNow = 0xFFFFFFFEu; CHECK(world.generator.Generate(Info())); CHECK_EQ(27, effects.front()->GetEndFrame()); CHECK_EQ(2, effects.front()->GetEndLinkFrame());
	CHECK(effects.front()->IsEnd()); frameNow = 0; CHECK(!effects.front()->IsEnd());
	ClearEffects(); MEffect::SetHost(nullptr); CHECK(world.generator.Generate(Info()));
	CHECK_EQ(29, effects.front()->GetEndFrame()); CHECK_EQ(4, effects.front()->GetEndLinkFrame()); CHECK_EQ(0, effects.front()->GetLight());
	CHECK(effects.front()->IsEnd()); CHECK(!effects.front()->IsDelayFrame()); CHECK(!effects.front()->Update()); CHECK_EQ(1, effects.front()->GetFrame());
}
