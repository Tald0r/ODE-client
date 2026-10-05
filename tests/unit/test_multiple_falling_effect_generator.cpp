#include "test_framework.h"
#include "MMultipleFallingEffectGenerator.h"
#include "MLinearEffect.h"
#include "SkillDef.h"

#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MMultipleFallingEffectSprite sprite;
bool spriteAvailable, acceptQueue;
int requestedSprite, submissions, rejectBefore, acceptOnly, observedRandom;
std::vector<int> calls, slots, removedTargets;
std::vector<std::unique_ptr<MEffect>> effects;
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(3); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE) { calls.push_back(2); return 7; },
};
const MEffectTargetHost targetHost{
	.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); },
};
const MMultipleFallingEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MMultipleFallingEffectSprite& result) {
		calls.push_back(1); requestedSprite = type;
		result = sprite;
		return spriteAvailable;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		calls.push_back(4);
		CHECK(effect->GetEffectTarget() == nullptr);
		CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		const int slot = submissions++;
		if (!acceptQueue || slot < rejectBefore || (acceptOnly >= 0 && slot != acceptOnly)) return false;
		slots.push_back(slot); effects.push_back(std::move(effect));
		return true;
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MMultipleFallingEffectHost* previousMultiple = MMultipleFallingEffectGenerator::SetHost(&host);
	World()
	{
		frameNow = 100; sprite = {BLT_EFFECT, 12, 3}; spriteAvailable = acceptQueue = true;
		requestedSprite = -1; submissions = rejectBefore = 0; acceptOnly = -1; observedRandom = -1;
		effects.clear(); calls.clear(); slots.clear(); removedTargets.clear();
		std::srand(42);
	}
	~World()
	{
		effects.clear();
		MMultipleFallingEffectGenerator::SetHost(previousMultiple);
		MEffectTarget::SetHost(previousTarget);
		MEffect::SetHost(previousEffect);
	}
};

EFFECTGENERATOR_INFO Info(TYPE_ACTIONINFO action = 42)
{
	EFFECTGENERATOR_INFO info{};
	info.nActionInfo = action; info.effectSpriteType = 17;
	info.x0 = 96; info.y0 = 48; info.z0 = 12;
	info.x1 = 900; info.y1 = 800; info.z1 = 100;
	info.creatureID = 123;
	info.direction = DIRECTION_LEFTUP; info.step = 50;
	info.count = 30; info.linkCount = 5; info.power = 77;
	return info;
}

std::unique_ptr<MEffectTarget> Target()
{
	auto target = std::make_unique<MEffectTarget>(3);
	target->NextPhase(); target->m_EffectID = 73;
	target->Set(777, 888, 999, 456);
	target->SetServerID(789); target->SetDelayFrame(31);
	target->SetResultTime(); target->SetResult(new MActionResult);
	return target;
}

void CheckSpread(const MEffect& effect, int slot, int width, int height)
{
	if (slot % 4 < 2)
		CHECK(effect.GetPixelX() <= 72 && effect.GetPixelX() > 72 - width);
	else CHECK(effect.GetPixelX() >= 120 && effect.GetPixelX() < 120 + width);
	if (slot % 2 == 0)
		CHECK(effect.GetPixelY() <= 48 && effect.GetPixelY() > 48 - height);
	else CHECK(effect.GetPixelY() >= 48 && effect.GetPixelY() < 48 + height);
	CHECK(effect.GetPixelZ() >= 362 + 150 * (slot / 4));
	CHECK(effect.GetPixelZ() < 412 + 150 * (slot / 4));
}

} // namespace

namespace {

struct CopyResultMarker : MActionResultNode
{
	int& destroyed;
	explicit CopyResultMarker(int& count) : destroyed(count) {}
	~CopyResultMarker() override { ++destroyed; }
	void Execute() override {}
};
void ClearEffectsCheckingCopyDestruction(size_t firstCopy = 1)
{
	int destroyed = 0;
	const auto expected = effects.size() - firstCopy;
	for (size_t i = firstCopy; i < effects.size(); ++i)
	{
		auto* target = effects[i]->GetEffectTarget(); CHECK(target != nullptr); CHECK(!target->IsExistResult());
		auto result = std::make_unique<MActionResult>(); result->Add(new CopyResultMarker(destroyed)); target->SetResult(result.release());
	}
	effects.clear(); CHECK_EQ(expected, static_cast<size_t>(destroyed));
}
}

TEST(MultipleFallingEffectGenerator, DefaultPatternConfiguresFourPhasesOfRealLinearEffects)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	CHECK_EQ(EFFECTGENERATORID_MULTIPLE_FALLING, generator.GetID());
	CHECK(generator.Generate(Info()));
	CHECK_EQ(16, effects.size()); CHECK_EQ(16, submissions); CHECK_EQ(17, requestedSprite);
	for (int i = 0; i < 16; ++i)
	{
		const auto& effect = *effects[i];
		CHECK_EQ(i, slots[i]); CheckSpread(effect, i, 48, 50);
		CHECK_EQ(MEffect::EFFECT_LINEAR, effect.GetEffectType());
		CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID());
		CHECK_EQ(3, effect.GetMaxFrame()); CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight());
		CHECK_EQ(DIRECTION_DOWN, effect.GetDirection()); CHECK_EQ(50, effect.GetStepPixel());
		CHECK_EQ(132 + 3 * (i / 4), effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame());
		CHECK_EQ(77, effect.GetPower()); CHECK_EQ(42, effect.GetActionInfo());
	}
	CHECK_EQ(49, calls.size()); CHECK_EQ(1, calls.front());
	for (std::size_t i = 1; i < calls.size(); i += 3)
	{
		CHECK_EQ(2, calls[i]); CHECK_EQ(3, calls[i + 1]); CHECK_EQ(4, calls[i + 2]);
	}
}

TEST(MultipleFallingEffectGenerator, StormAndHailActionsSelectTheirPhaseCountAndSpread)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	struct Pattern { TYPE_ACTIONINFO action; int phases, width, height; };
	for (const Pattern pattern : {Pattern{SKILL_ACID_STORM_WIDE, 6, 96, 100},
		{SKILL_POISON_STORM_WIDE, 6, 96, 100}, {SKILL_ICE_HAIL, 13, 96, 48},
		{SKILL_WIDE_ICE_HAIL, 25, 192, 96}})
	{
		submissions = 0; slots.clear();
		CHECK(generator.Generate(Info(pattern.action)));
		CHECK_EQ(pattern.phases * 4, effects.size()); CHECK_EQ(pattern.phases * 4, submissions);
		for (int i = 0; i < pattern.phases * 4; ++i)
		{
			CheckSpread(*effects[i], i, pattern.width, pattern.height);
			CHECK_EQ(132 + 3 * (i / 4), effects[i]->GetEndFrame());
			CHECK_EQ(pattern.action, effects[i]->GetActionInfo());
		}
		effects.clear();
	}
}

TEST(MultipleFallingEffectGenerator, FirstAcceptedTargetStaysUnchangedAndLaterTargetsAreCopies)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	auto* result = target->GetResult();
	CHECK(generator.Generate(info)); target.release();
	CHECK(effects[0]->GetEffectTarget() == info.pEffectTarget);
	CHECK_EQ(777, info.pEffectTarget->GetX()); CHECK_EQ(888, info.pEffectTarget->GetY());
	CHECK_EQ(999, info.pEffectTarget->GetZ()); CHECK_EQ(456, info.pEffectTarget->GetID());
	CHECK(info.pEffectTarget->GetResult() == result);
	for (std::size_t i = 1; i < effects.size(); ++i)
	{
		auto* linked = effects[i]->GetEffectTarget();
		CHECK(linked != info.pEffectTarget);
		CHECK_EQ(effects[i]->GetPixelX(), linked->GetX()); CHECK_EQ(effects[i]->GetPixelY(), linked->GetY());
		CHECK_EQ(76, linked->GetZ()); CHECK_EQ(123, linked->GetID());
		CHECK_EQ(73, linked->GetEffectID()); CHECK_EQ(1, linked->GetCurrentPhase());
		CHECK_EQ(3, linked->GetMaxPhase()); CHECK_EQ(31, linked->GetDelayFrame());
		CHECK_EQ(OBJECTID_NULL, linked->GetServerID()); CHECK(!linked->IsResultTime());
		CHECK(linked->GetResult() == nullptr);
		for (std::size_t j = 0; j < i; ++j) CHECK(linked != effects[j]->GetEffectTarget());
	}
	ClearEffectsCheckingCopyDestruction();
	CHECK(removedTargets == std::vector<int>{73});
}

TEST(MultipleFallingEffectGenerator, EverySlotCanBecomeTheFirstAcceptedOwner)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	for (int first = 0; first < 16; ++first)
	{
		submissions = 0; rejectBefore = acceptOnly = first;
		slots.clear(); removedTargets.clear();
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK(generator.Generate(info)); target.release();
		CHECK_EQ(16, submissions); CHECK_EQ(1, effects.size());
		CHECK(slots == std::vector<int>({first}));
		CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget);
		CHECK_EQ(999, info.pEffectTarget->GetZ());
		CHECK_EQ(132 + 3 * (first / 4), effects.front()->GetEndFrame());
		effects.clear();
		CHECK(removedTargets == std::vector<int>({73}));
	}
}

TEST(MultipleFallingEffectGenerator, CompleteRejectionLeavesTheCallerTargetUntouched)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	acceptQueue = false;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!generator.Generate(info)); CHECK_EQ(16, submissions);
	CHECK(effects.empty()); CHECK(removedTargets.empty()); CHECK_EQ(999, target->GetZ());
}

TEST(MultipleFallingEffectGenerator, TargetlessPatternsKeepTheirActionWithoutCreatingTargets)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	CHECK(generator.Generate(Info()));
	for (const auto& effect : effects)
	{
		CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(42, effect->GetActionInfo());
	}
	CHECK(removedTargets.empty());
}

TEST(MultipleFallingEffectGenerator, RealProjectilesFallToTheSharedHeightAndStopBeforeAnimation)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	CHECK(generator.Generate(Info()));
	auto& effect = *effects.front();
	const int start = effect.GetPixelZ();
	CHECK(effect.Update()); CHECK_EQ(start - 50, effect.GetPixelZ()); CHECK_EQ(1, effect.GetFrame());
	bool arrived = false;
	for (int i = 0; i < 10; ++i)
	{
		const int previousFrame = effect.GetFrame();
		if (!effect.Update())
		{
			CHECK_EQ(previousFrame, effect.GetFrame()); arrived = true; break;
		}
	}
	CHECK(arrived); CHECK_EQ(76, effect.GetPixelZ());
	CHECK_EQ(0, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame());
}

TEST(MultipleFallingEffectGenerator, PhaseLifetimesExpireIndependentlyBeforeMovement)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	CHECK(generator.Generate(Info()));
	const int firstZ = effects[0]->GetPixelZ(), nextZ = effects[4]->GetPixelZ();
	frameNow = 132;
	CHECK(!effects[0]->Update()); CHECK_EQ(firstZ, effects[0]->GetPixelZ());
	CHECK(effects[4]->Update()); CHECK_EQ(nextZ - 50, effects[4]->GetPixelZ());
}

TEST(MultipleFallingEffectGenerator, PhaseIncrementsApplyEvenWhenTheInputCountIsZero)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	auto info = Info(); info.count = 0;
	CHECK(generator.Generate(info));
	CHECK_EQ(102, effects[0]->GetEndFrame()); CHECK_EQ(111, effects[15]->GetEndFrame());
	CHECK(effects[0]->Update());
}

TEST(MultipleFallingEffectGenerator, StepsAboveThePhaseHeightKeepOneDurationForEveryPhase)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	auto info = Info(); info.step = 255;
	CHECK(generator.Generate(info));
	for (const auto& effect : effects) CHECK_EQ(129, effect->GetEndFrame());
}

TEST(MultipleFallingEffectGenerator, RejectionBeforeTheFirstOwnerDoesNotChangeLaterCopyCoordinates)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	rejectBefore = 5;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(generator.Generate(info)); target.release();
	CHECK_EQ(11, effects.size()); CHECK_EQ(5, slots.front());
	CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget);
	CHECK_EQ(999, info.pEffectTarget->GetZ());
	for (std::size_t i = 1; i < effects.size(); ++i)
	{
		auto* linked = effects[i]->GetEffectTarget();
		CHECK(linked != info.pEffectTarget);
		CHECK_EQ(effects[i]->GetPixelX(), linked->GetX());
		CHECK_EQ(effects[i]->GetPixelY(), linked->GetY());
		CHECK_EQ(76, linked->GetZ()); CHECK_EQ(123, linked->GetID());
	}
	ClearEffectsCheckingCopyDestruction(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(MultipleFallingEffectGenerator, SamplingConsumesTwelveRandomValuesForEveryPhaseEvenOnRejection)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	struct Pattern { TYPE_ACTIONINFO action; int phases; };
	for (const Pattern pattern : {Pattern{42, 4}, {SKILL_ACID_STORM_WIDE, 6},
		{SKILL_POISON_STORM_WIDE, 6}, {SKILL_ICE_HAIL, 13}, {SKILL_WIDE_ICE_HAIL, 25}})
	{
		for (bool accepted : {true, false})
		{
			std::srand(123);
			for (int i = 0; i < 12 * pattern.phases; ++i) (void)std::rand();
			const int following = std::rand();
			std::srand(123); submissions = 0; acceptQueue = accepted;
			CHECK_EQ(accepted, generator.Generate(Info(pattern.action)));
			CHECK_EQ(following, std::rand()); CHECK_EQ(4 * pattern.phases, submissions);
			effects.clear();
		}
	}
}

TEST(MultipleFallingEffectGenerator, SeededFirstShotKeepsItsHorizontalAndHeightJitter)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	std::srand(456);
	const int x = 72 - std::rand() % 48;
	const int y = 48 - std::rand() % 50;
	const int z = 362 + std::rand() % 50;
	std::srand(456);
	CHECK(generator.Generate(Info()));
	CHECK_EQ(x, effects.front()->GetPixelX()); CHECK_EQ(y, effects.front()->GetPixelY());
	CHECK_EQ(z, effects.front()->GetPixelZ());
}

TEST(MultipleFallingEffectGenerator, TheWholePhaseIsSampledBeforeItsFirstSubmission)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	std::srand(789);
	for (int i = 0; i < 12; ++i) (void)std::rand();
	const int following = std::rand();
	std::srand(789);
	const MMultipleFallingEffectHost observing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			if (submissions == 0) observedRandom = std::rand();
			return host.Queue(std::move(effect));
		},
	};
	MMultipleFallingEffectGenerator::SetHost(&observing);
	CHECK(generator.Generate(Info())); CHECK_EQ(following, observedRandom);
}

TEST(MultipleFallingEffectGenerator, OneMetadataSnapshotCoversTheEntirePattern)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	const MMultipleFallingEffectHost changing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			sprite = {BLT_NORMAL, 23, 258};
			return host.Queue(std::move(effect));
		},
	};
	MMultipleFallingEffectGenerator::SetHost(&changing);
	CHECK(generator.Generate(Info()));
	for (const auto& effect : effects)
	{
		CHECK_EQ(12, effect->GetFrameID()); CHECK_EQ(3, effect->GetMaxFrame());
		CHECK_EQ(BLT_EFFECT, effect->GetBltType());
	}
	effects.clear(); submissions = 0;
	CHECK(generator.Generate(Info()));
	for (auto& effect : effects)
	{
		CHECK_EQ(23, effect->GetFrameID()); CHECK_EQ(2, effect->GetMaxFrame());
		CHECK_EQ(BLT_NORMAL, effect->GetBltType());
		CHECK(effect->Update()); CHECK_EQ(7, effect->GetLight());
	}
}

TEST(MultipleFallingEffectGenerator, AThrowingQueueDestroysItsEffectWithoutTakingTheCallerTarget)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	const MMultipleFallingEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool {
			auto marker = Target(); marker->m_EffectID = 74;
			effect->SetLink(43, marker.release());
			throw std::runtime_error("Queue failure");
		},
	};
	MMultipleFallingEffectGenerator::SetHost(&throwing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	bool threw = false;
	try { generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK(removedTargets == std::vector<int>({74})); CHECK_EQ(999, target->GetZ());
}

TEST(MultipleFallingEffectGenerator, UnavailableMetadataRejectsBeforeRandomnessOrConstruction)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	spriteAvailable = false;
	std::srand(123); const int next = std::rand(); std::srand(123);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!generator.Generate(info)); CHECK_EQ(next, std::rand());
	CHECK(calls == std::vector<int>({1})); CHECK(effects.empty()); CHECK(removedTargets.empty());
}

TEST(MultipleFallingEffectGenerator, AbsentAndPartialServicesKeepTargetsWithTheCaller)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	const MMultipleFallingEffectHost empty{}, queueOnly{.Queue = host.Queue}, spriteOnly{.Sprite = host.Sprite};
	for (const auto* service : {static_cast<const MMultipleFallingEffectHost*>(nullptr), &empty, &queueOnly})
	{
		MMultipleFallingEffectGenerator::SetHost(service);
		CHECK(!generator.Generate(Info()));
	}
	CHECK(calls.empty());
	MMultipleFallingEffectGenerator::SetHost(&spriteOnly);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!generator.Generate(info)); CHECK_EQ(33, calls.size());
	CHECK_EQ(0, submissions); CHECK(effects.empty()); CHECK(removedTargets.empty());
}

TEST(MultipleFallingEffectGenerator, QueueRemovalAfterAcceptanceKeepsTheCompletedTransfer)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	const MMultipleFallingEffectHost disappearing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			const bool accepted = host.Queue(std::move(effect));
			MMultipleFallingEffectGenerator::SetHost(nullptr);
			return accepted;
		},
	};
	CHECK(MMultipleFallingEffectGenerator::SetHost(&disappearing) == &host);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(generator.Generate(info)); target.release();
	CHECK_EQ(1, effects.size()); CHECK_EQ(1, submissions);
	CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget);
	CHECK(MMultipleFallingEffectGenerator::SetHost(&host) == nullptr);
	effects.clear(); CHECK(removedTargets == std::vector<int>({73}));
}

TEST(MultipleFallingEffectGenerator, ClockRemovalOfTheQueueBeforeSubmissionKeepsTheTarget)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	const MEffectHost changing{
		.CurrentFrame = []() { MMultipleFallingEffectGenerator::SetHost(nullptr); return frameNow; },
		.Light = effectHost.Light,
	};
	MEffect::SetHost(&changing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!generator.Generate(info)); CHECK_EQ(0, submissions);
	CHECK(effects.empty()); CHECK(removedTargets.empty());
}

TEST(MultipleFallingEffectGenerator, PhaseDurationsCanExceedTheInputWordWithoutNarrowing)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	auto info = Info(); info.count = (std::numeric_limits<WORD>::max)(); info.linkCount = MAX_LINKCOUNT;
	CHECK(generator.Generate(info));
	CHECK_EQ(65637, effects[0]->GetEndFrame()); CHECK_EQ(65646, effects[15]->GetEndFrame());
	CHECK_EQ(effects[15]->GetEndFrame(), effects[15]->GetEndLinkFrame());
}

TEST(MultipleFallingEffectGenerator, AbsoluteFrameWrapAndMissingClockKeepBaseEffectSemantics)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	frameNow = (std::numeric_limits<DWORD>::max)() - 1;
	CHECK(generator.Generate(Info())); CHECK_EQ(30, effects.front()->GetEndFrame());
	CHECK(!effects.front()->Update()); frameNow = 0;
	CHECK(effects.front()->Update());
	effects.clear(); submissions = 0;
	MEffect::SetHost(nullptr);
	CHECK(generator.Generate(Info())); CHECK_EQ(32, effects.front()->GetEndFrame());
	CHECK_EQ(4, effects.front()->GetEndLinkFrame()); CHECK_EQ(0, effects.front()->GetLight());
	const int start = effects.front()->GetPixelZ();
	CHECK(!effects.front()->Update()); CHECK_EQ(start, effects.front()->GetPixelZ());
}

TEST(MultipleFallingEffectGenerator, DestinationBelowTheIntegerRangeClampsForEveryCopy)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	const int bottom = (std::numeric_limits<int>::min)();
	auto target = Target(); auto info = Info(); info.z1 = bottom; info.pEffectTarget = target.get();
	CHECK(generator.Generate(info)); target.release();
	CHECK_EQ(999, info.pEffectTarget->GetZ());
	for (std::size_t i = 1; i < effects.size(); ++i)
		CHECK_EQ(bottom, effects[i]->GetEffectTarget()->GetZ());
}

TEST(MultipleFallingEffectGenerator, ExtremeSourceCoordinatesAndHeightsStayBounded)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	const int top = (std::numeric_limits<int>::max)();
	const int bottom = (std::numeric_limits<int>::min)();
	for (int source : {top, bottom})
	{
		submissions = 0;
		auto info = Info(); info.x0 = info.y0 = source; info.z0 = top;
		CHECK(generator.Generate(info));
		for (std::size_t i = 0; i < effects.size(); ++i)
		{
			CHECK_EQ(top, effects[i]->GetPixelZ());
			if ((source == top && i % 4 >= 2) || (source == bottom && i % 4 < 2))
				CHECK_EQ(source, effects[i]->GetPixelX());
			if ((source == top && i % 2 == 1) || (source == bottom && i % 2 == 0))
				CHECK_EQ(source, effects[i]->GetPixelY());
		}
		effects.clear();
	}
}

TEST(MultipleFallingEffectGenerator, ZeroSpeedKeepsConfiguredLifetimeAndStationaryAnimation)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	auto target = Target(); auto info = Info(); info.step = 0; info.pEffectTarget = target.get();
	CHECK(generator.Generate(info)); target.release();
	CHECK_EQ(16, effects.size()); CHECK(effects.front()->GetEffectTarget() == info.pEffectTarget);
	for (auto& effect : effects)
	{
		const int start = effect->GetPixelZ();
		CHECK_EQ(0, effect->GetStepPixel()); CHECK_EQ(129, effect->GetEndFrame());
		CHECK_EQ(104, effect->GetEndLinkFrame());
		CHECK(effect->Update()); CHECK_EQ(start, effect->GetPixelZ()); CHECK_EQ(1, effect->GetFrame());
	}
	ClearEffectsCheckingCopyDestruction(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(MultipleFallingEffectGenerator, DestinationSubtractionPreservesRepresentableBoundaryValues)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	const int bottom = (std::numeric_limits<int>::min)();
	const int top = (std::numeric_limits<int>::max)();
	struct Height { int supplied, expected; };
	for (const Height height : {Height{bottom + 24, bottom}, {bottom + 25, bottom + 1},
		{-1, -25}, {0, -24}, {top, top - 24}})
	{
		submissions = 0;
		auto target = Target(); auto info = Info(); info.z1 = height.supplied; info.pEffectTarget = target.get();
		CHECK(generator.Generate(info)); target.release();
		CHECK_EQ(height.expected, effects[1]->GetEffectTarget()->GetZ());
		effects.clear();
	}
}

TEST(MultipleFallingEffectGenerator, ZeroSpeedAndDurationKeepEveryActionPatternExpired)
{
	World world;
	MMultipleFallingEffectGenerator generator;
	struct Pattern { TYPE_ACTIONINFO action; int count; };
	for (const Pattern pattern : {Pattern{42, 16}, {SKILL_ACID_STORM_WIDE, 24},
		{SKILL_POISON_STORM_WIDE, 24}, {SKILL_ICE_HAIL, 52}, {SKILL_WIDE_ICE_HAIL, 100}})
	{
		submissions = 0;
		auto info = Info(pattern.action); info.step = 0; info.count = 0;
		CHECK(generator.Generate(info)); CHECK_EQ(pattern.count, effects.size());
		for (auto& effect : effects)
		{
			CHECK_EQ(99, effect->GetEndFrame()); CHECK_EQ(104, effect->GetEndLinkFrame());
			CHECK_EQ(0, effect->GetStepPixel()); CHECK(!effect->Update());
		}
		effects.clear();
	}
}
