#include "test_framework.h"
#include "MBloodyBreakerEffectGenerator.h"
#include "MRippleZoneWideEffectGenerator.h"
#include "MEffect.h"
#include "EffectSpriteTypeDef.h"

#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MBloodyBreakerEffectSprite sprite;
int maxFrames, submissions, acceptance;
bool spriteAvailable, framesAvailable;
std::vector<int> calls, spriteRequests, slots, removedTargets;
struct FrameRequest { BYTE blt; TYPE_FRAMEID id; bool operator==(const FrameRequest&) const = default; };
std::vector<FrameRequest> frameRequests;
std::vector<std::unique_ptr<MEffect>> effects;
struct Point { int x, y; bool operator==(const Point&) const = default; };
// Literal rows at source tile (5,5), in center / left / right / left / right order.
constexpr Point rows[8][5] = {
	{{4, 5}, {4, 6}, {4, 4}, {4, 7}, {4, 3}},
	{{4, 6}, {5, 6}, {4, 5}, {6, 6}, {4, 4}},
	{{5, 6}, {6, 6}, {4, 6}, {7, 6}, {3, 6}},
	{{6, 6}, {6, 5}, {5, 6}, {6, 4}, {4, 6}},
	{{6, 5}, {6, 4}, {6, 6}, {6, 3}, {6, 7}},
	{{6, 4}, {5, 4}, {6, 5}, {4, 4}, {6, 6}},
	{{5, 4}, {4, 4}, {6, 4}, {3, 4}, {7, 4}},
	{{4, 4}, {4, 5}, {5, 4}, {4, 6}, {6, 4}},
};
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(4); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE frame) { calls.push_back(3); return 7 + frame; },
};
const MEffectTargetHost targetHost{.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); }};
const MBloodyBreakerEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MBloodyBreakerEffectSprite& result) {
		calls.push_back(1); spriteRequests.push_back(type); result = sprite; return spriteAvailable;
	},
	.MaxFrames = [](BYTE blt, TYPE_FRAMEID id, int& count) {
		calls.push_back(2); frameRequests.push_back({blt, id}); count = maxFrames; return framesAvailable;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		calls.push_back(5); const int slot = submissions++;
		CHECK_EQ(MEffect::EFFECT_SECTOR, effect->GetEffectType());
		CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		if ((acceptance & (1 << slot)) == 0) return false;
		slots.push_back(slot); effects.push_back(std::move(effect)); return true;
	},
};

void ClearEffects()
{
	effects.clear(); calls.clear(); slots.clear(); spriteRequests.clear(); frameRequests.clear(); submissions = 0;
}

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MBloodyBreakerEffectHost* previousGenerator = MBloodyBreakerEffectGenerator::SetHost(&host);
	MBloodyBreakerEffectGenerator generator;
	World()
	{
		ClearEffects(); removedTargets.clear(); frameNow = 100; sprite = {BLT_EFFECT, 12}; maxFrames = 3;
		spriteAvailable = framesAvailable = true; acceptance = 31;
	}
	~World()
	{
		effects.clear(); MBloodyBreakerEffectGenerator::SetHost(previousGenerator);
		MEffectTarget::SetHost(previousTarget); MEffect::SetHost(previousEffect);
	}
};

EFFECTGENERATOR_INFO Info()
{
	EFFECTGENERATOR_INFO info{}; info.nActionInfo = 42; info.effectSpriteType = 17;
	info.x0 = 240; info.y0 = 120; info.z0 = 17; info.x1 = 900; info.y1 = 800; info.z1 = 700;
	info.direction = DIRECTION_LEFT; info.step = 10; info.count = 30; info.linkCount = 5; info.power = 3; info.creatureID = 123;
	return info;
}

std::unique_ptr<MEffectTarget> Target(BYTE phase = 4, BYTE id = 73)
{
	auto target = std::make_unique<MEffectTarget>(255);
	for (int i = 0; i < phase; ++i) target->NextPhase();
	target->m_EffectID = id; target->Set(777, 888, 999, 456); target->SetServerID(789);
	target->SetDelayFrame(31); target->SetResultTime(); target->SetResult(new MActionResult); return target;
}

void CheckTarget(const MEffectTarget& target, BYTE phase = 4)
{
	CHECK_EQ(777, target.GetX()); CHECK_EQ(888, target.GetY()); CHECK_EQ(999, target.GetZ()); CHECK_EQ(456, target.GetID());
	CHECK_EQ(789, target.GetServerID()); CHECK_EQ(phase, target.GetCurrentPhase()); CHECK_EQ(255, target.GetMaxPhase()); CHECK_EQ(31, target.GetDelayFrame());
	CHECK(target.IsExistResult()); CHECK(target.IsResultTime());
}

// Follow actual ownership so a failed result assertion cannot create a double delete.
bool Generate(World& world, EFFECTGENERATOR_INFO info, std::unique_ptr<MEffectTarget>& target)
{
	info.pEffectTarget = target.get();
	const bool accepted = world.generator.Generate(info);
	for (const auto& effect : effects) if (target && effect->GetEffectTarget() == target.get()) { target.release(); break; }
	return accepted;
}

std::vector<Point> Positions()
{
	std::vector<Point> points;
	for (const auto& effect : effects) points.push_back({effect->GetX(), effect->GetY()});
	return points;
}

} // namespace

TEST(BloodyBreakerEffectGenerator, ConfiguresRealEffectsWithMetadataRefreshEvenAfterTheFinalAttempt)
{
	World world; CHECK_EQ(EFFECTGENERATORID_BLOODY_BREAKER, world.generator.GetID()); auto target = Target(); auto* original = target.get();
	std::srand(61); const int next = std::rand(); std::srand(61); CHECK(Generate(world, Info(), target)); CHECK_EQ(next, std::rand());
	CHECK_EQ(5, effects.size()); CHECK_EQ(5, submissions); CHECK(spriteRequests == std::vector<int>(6, 17));
	CHECK(frameRequests == std::vector<FrameRequest>(6, {BLT_EFFECT, 12}));
	std::vector<int> expected{1, 2}; for (int i = 0; i < 5; ++i) expected.insert(expected.end(), {3, 4, 5, 1, 2}); CHECK(calls == expected);
	for (size_t i = 0; i < effects.size(); ++i)
	{
		auto& effect = *effects[i]; CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame());
		CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight()); CHECK_EQ(192, effect.GetPixelX()); CHECK_EQ(rows[0][i].y * 24, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ());
		CHECK_EQ(0, effect.GetDirection()); CHECK_EQ(10, effect.GetStepPixel()); CHECK_EQ(3, effect.GetPower()); CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame());
		CHECK_EQ(42, effect.GetActionInfo()); CHECK(effect.IsMulti()); CHECK(!effect.IsDelayFrame()); CHECK_EQ(i == 0, effect.GetEffectTarget() == original);
		if (i != 0) CHECK(effect.GetEffectTarget() == nullptr);
	}
}

TEST(BloodyBreakerEffectGenerator, AllEightDirectionsAndSixPhasesMatchTheOrderedRows)
{
	World world; constexpr int lengths[6] = {1, 3, 3, 5, 5, 5};
	for (int direction = 0; direction < 8; ++direction) for (BYTE phase = 1; phase <= 6; ++phase)
	{
		ClearEffects(); auto target = Target(phase); auto* original = target.get(); auto info = Info(); info.direction = static_cast<BYTE>(direction);
		CHECK(Generate(world, info, target)); CHECK_EQ(lengths[phase - 1], effects.size()); CheckTarget(*original, phase);
		for (size_t i = 0; i < effects.size(); ++i)
		{
			CHECK_EQ(rows[direction][i].x, effects[i]->GetX()); CHECK_EQ(rows[direction][i].y, effects[i]->GetY());
			CHECK_EQ(direction, effects[i]->GetDirection()); CHECK_EQ(i == 0, effects[i]->GetEffectTarget() == original);
		}
	}
}

TEST(BloodyBreakerEffectGenerator, EveryOtherPhaseAndMissingTargetRejectAfterInitialMetadata)
{
	World world;
	for (int direction = 0; direction < 8; ++direction) for (int phase = -1; phase < 256; ++phase)
	{
		if (phase >= 1 && phase <= 6) continue;
		ClearEffects(); auto target = phase < 0 ? std::unique_ptr<MEffectTarget>{} : Target(static_cast<BYTE>(phase));
		auto info = Info(); info.direction = static_cast<BYTE>(direction); CHECK(!Generate(world, info, target));
		CHECK_EQ(0, submissions); CHECK(effects.empty()); CHECK(calls == std::vector<int>({1, 2}));
		if (target) CheckTarget(*target, static_cast<BYTE>(phase));
	}
}

TEST(BloodyBreakerEffectGenerator, AcceptedCenterKeepsTheOriginalResultAndAllTargetMetadata)
{
	World world; auto target = Target(); auto* original = target.get(); auto* result = target->GetResult();
	CHECK(Generate(world, Info(), target)); CHECK(effects.front()->GetEffectTarget() == original); CheckTarget(*original);
	CHECK(original->GetResult() == result); CHECK(removedTargets.empty()); ClearEffects(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(BloodyBreakerEffectGenerator, EveryAcceptanceMaskKeepsSidesIndependentAndOnlyTheCenterOwnsTheTarget)
{
	World world;
	for (int mask = 0; mask < 32; ++mask)
	{
		ClearEffects(); acceptance = mask; auto target = Target(); auto* original = target.get();
		const bool result = Generate(world, Info(), target); if (mask & 1) CHECK(result);
		CHECK_EQ(5, submissions); CheckTarget(*original); CHECK_EQ((mask & 1) == 0, target != nullptr);
		for (size_t i = 0; i < effects.size(); ++i)
		{
			CHECK_EQ(slots[i] == 0, effects[i]->GetEffectTarget() == original);
			if (slots[i] != 0) CHECK(effects[i]->GetEffectTarget() == nullptr);
			CHECK_EQ(rows[0][slots[i]].y, effects[i]->GetY()); CHECK_EQ(42, effects[i]->GetActionInfo());
		}
	}
}

TEST(BloodyBreakerEffectGenerator, BloodyWallVariantsCycleAfterRejectionsAndKeepTheInitialBlitType)
{
	World world;
	const MBloodyBreakerEffectHost variants{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MBloodyBreakerEffectSprite& result) {
			const bool available = host.Sprite(type, result); const int variant = type - EFFECTSPRITETYPE_BLOODY_WALL_1;
			result = {static_cast<BYTE>(BLT_EFFECT + variant), static_cast<TYPE_FRAMEID>(20 + variant)}; return available;
		},
		.MaxFrames = host.MaxFrames, .Queue = host.Queue,
	};
	MBloodyBreakerEffectGenerator::SetHost(&variants);
	for (int first = 0; first < 3; ++first)
	{
		ClearEffects(); acceptance = 21; auto target = Target(); auto info = Info(); info.effectSpriteType = static_cast<TYPE_EFFECTSPRITETYPE>(EFFECTSPRITETYPE_BLOODY_WALL_1 + first);
		CHECK(Generate(world, info, target)); CHECK_EQ(6, spriteRequests.size()); CHECK_EQ(6, frameRequests.size());
		for (size_t i = 0; i < spriteRequests.size(); ++i)
		{
			CHECK_EQ(EFFECTSPRITETYPE_BLOODY_WALL_1 + (first + i) % 3, spriteRequests[i]);
			CHECK_EQ(BLT_EFFECT + first, frameRequests[i].blt); CHECK_EQ(20 + (first + i) % 3, frameRequests[i].id);
		}
		for (size_t i = 0; i < effects.size(); ++i) { CHECK_EQ(BLT_EFFECT + first, effects[i]->GetBltType()); CHECK_EQ(20 + (first + slots[i]) % 3, effects[i]->GetFrameID()); }
	}
}

TEST(BloodyBreakerEffectGenerator, SourceConversionAndFinalPositionsKeepUnsignedTileWrapping)
{
	World world; struct Source { int x, y, expectedX, expectedY; };
	for (const Source source : {Source{261, 130, 6, 5}, {-1, -1, 1, 0}, {-48, -24, 0, 65535}, {3145728, 1572864, 1, 0}})
	{
		ClearEffects(); auto target = Target(1); auto info = Info(); info.x0 = source.x; info.y0 = source.y; info.direction = DIRECTION_RIGHT;
		CHECK(Generate(world, info, target)); CHECK_EQ(source.expectedX, effects.front()->GetX()); CHECK_EQ(source.expectedY, effects.front()->GetY());
	}
	ClearEffects(); auto target = Target(); auto info = Info(); info.x0 = info.y0 = 0; CHECK(Generate(world, info, target));
	CHECK(Positions() == std::vector<Point>({{65535, 0}, {65535, 1}, {65535, 65535}, {65535, 2}, {65535, 65534}}));
}

TEST(BloodyBreakerEffectGenerator, ExtremeSourceCoordinatesRetainNarrowingAndFloatHeightStorage)
{
	World world; const int low = (std::numeric_limits<int>::min)(), high = (std::numeric_limits<int>::max)();
	for (const int source : {low, high})
	{
		ClearEffects(); auto target = Target(1); auto info = Info(); info.x0 = info.y0 = info.z0 = source; info.direction = DIRECTION_RIGHT;
		CHECK(Generate(world, info, target)); CHECK_EQ(source == low ? 21847 : 43691, effects.front()->GetX());
		CHECK_EQ(source == low ? 43691 : 21845, effects.front()->GetY()); CHECK_EQ(source, effects.front()->GetPixelZ());
	}
}

TEST(BloodyBreakerEffectGenerator, PowerDoesNotChangeThePhasePatternAndKeepsEveryByte)
{
	World world;
	for (int power = 0; power < 256; ++power)
	{
		ClearEffects(); auto target = Target(2); auto info = Info(); info.power = static_cast<BYTE>(power); CHECK(Generate(world, info, target)); CHECK_EQ(3, effects.size());
		for (const auto& effect : effects) { CHECK_EQ(power, effect->GetPower()); CHECK_EQ(4, effect->GetX()); }
	}
}

TEST(BloodyBreakerEffectGenerator, MissingMetadataRejectsBeforeConstructionOrTargetTransfer)
{
	World world; const MBloodyBreakerEffectHost empty{}, noFrames{.Sprite = host.Sprite, .Queue = host.Queue};
	for (const auto* service : {static_cast<const MBloodyBreakerEffectHost*>(nullptr), &empty, &noFrames, &host})
	{
		ClearEffects(); MBloodyBreakerEffectGenerator::SetHost(service); framesAvailable = false; auto target = Target();
		CHECK(!Generate(world, Info(), target)); CHECK_EQ(0, submissions); CHECK(effects.empty()); CheckTarget(*target);
		CHECK(calls == (service == &host ? std::vector<int>{1, 2} : service == &noFrames ? std::vector<int>{1} : std::vector<int>{}));
	}
	ClearEffects(); spriteAvailable = false; auto target = Target(); CHECK(!Generate(world, Info(), target)); CHECK(calls == std::vector<int>{1});
}

TEST(BloodyBreakerEffectGenerator, MissingQueueDiscardsEffectsAndKeepsTheOriginalWithItsCaller)
{
	World world; const MBloodyBreakerEffectHost noQueue{.Sprite = host.Sprite, .MaxFrames = host.MaxFrames}; MBloodyBreakerEffectGenerator::SetHost(&noQueue);
	auto target = Target(); Generate(world, Info(), target); CHECK(target != nullptr); CHECK(effects.empty()); CHECK_EQ(0, submissions); CheckTarget(*target);
	std::vector<int> expected{1, 2}; for (int i = 0; i < 5; ++i) expected.insert(expected.end(), {3, 4, 1, 2}); CHECK(calls == expected);
}

TEST(BloodyBreakerEffectGenerator, SpriteAndFrameCallbacksCanInstallTheFollowingServices)
{
	World world;
	const MBloodyBreakerEffectHost fromSprite{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MBloodyBreakerEffectSprite& result) { MBloodyBreakerEffectGenerator::SetHost(&host); return host.Sprite(type, result); },
	};
	const MBloodyBreakerEffectHost fromFrames{
		.Sprite = host.Sprite,
		.MaxFrames = [](BYTE blt, TYPE_FRAMEID id, int& count) { MBloodyBreakerEffectGenerator::SetHost(&host); return host.MaxFrames(blt, id, count); },
	};
	for (const auto* service : {&fromSprite, &fromFrames})
	{
		ClearEffects(); MBloodyBreakerEffectGenerator::SetHost(service); auto target = Target(); CHECK(Generate(world, Info(), target)); CHECK_EQ(5, effects.size());
	}
}

TEST(BloodyBreakerEffectGenerator, MissingMetadataAfterSubmissionPreservesEarlierCenterOwnership)
{
	World world; static int removeAt;
	const MBloodyBreakerEffectHost removing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) { const bool accepted = host.Queue(std::move(effect)); if (submissions == removeAt) MBloodyBreakerEffectGenerator::SetHost(nullptr); return accepted; },
	};
	for (const int mask : {30, 31}) for (int stop = 1; stop <= 5; ++stop)
	{
		ClearEffects(); acceptance = mask; removeAt = stop; MBloodyBreakerEffectGenerator::SetHost(&removing); auto target = Target(); auto* original = target.get();
		CHECK_EQ(mask == 31, Generate(world, Info(), target)); CHECK_EQ(stop, submissions); CHECK_EQ(stop, spriteRequests.size()); CheckTarget(*original);
	}
}

TEST(BloodyBreakerEffectGenerator, UnavailableFramesAfterSubmissionStopWithoutLosingTheAcceptedCenter)
{
	World world;
	const MBloodyBreakerEffectHost removing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) { framesAvailable = false; return host.Queue(std::move(effect)); },
	};
	MBloodyBreakerEffectGenerator::SetHost(&removing);
	for (const int mask : {30, 31})
	{
		ClearEffects(); framesAvailable = true; acceptance = mask; auto target = Target(); auto* original = target.get();
		CHECK_EQ(mask == 31, Generate(world, Info(), target)); CHECK_EQ(1, submissions); CHECK_EQ(2, frameRequests.size()); CheckTarget(*original);
	}
}

TEST(BloodyBreakerEffectGenerator, ChangedMetadataAffectsTheNextAttemptButNotTheOriginalBlitType)
{
	World world;
	const MBloodyBreakerEffectHost changing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) { sprite = {BLT_SHADOW, 21}; maxFrames = 258; return host.Queue(std::move(effect)); },
	};
	MBloodyBreakerEffectGenerator::SetHost(&changing); auto target = Target(); CHECK(Generate(world, Info(), target));
	for (size_t i = 0; i < effects.size(); ++i) { CHECK_EQ(BLT_EFFECT, effects[i]->GetBltType()); CHECK_EQ(i == 0 ? 12 : 21, effects[i]->GetFrameID()); CHECK_EQ(i == 0 ? 3 : 2, effects[i]->GetMaxFrame()); }
	for (const auto& request : frameRequests) CHECK_EQ(BLT_EFFECT, request.blt);
	ClearEffects(); target = Target(); CHECK(Generate(world, Info(), target));
	for (const auto& effect : effects) CHECK_EQ(BLT_SHADOW, effect->GetBltType());
}

TEST(BloodyBreakerEffectGenerator, PhaseIsSampledOnceBeforeQueueCallbacksAdvanceTheTarget)
{
	World world; static MEffectTarget* original;
	const MBloodyBreakerEffectHost advancing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) { original->NextPhase(); return host.Queue(std::move(effect)); },
	};
	MBloodyBreakerEffectGenerator::SetHost(&advancing); auto target = Target(2); original = target.get(); CHECK(Generate(world, Info(), target));
	CHECK(Positions() == std::vector<Point>({{4, 5}, {4, 6}, {4, 4}})); CHECK_EQ(5, original->GetCurrentPhase());
}

TEST(BloodyBreakerEffectGenerator, InstallerIsIndependentOfOtherGenerators)
{
	World world; const MWideRippleEffectHost sentinel{}; const auto* previous = MRippleZoneWideEffectGenerator::SetHost(&sentinel);
	CHECK(MBloodyBreakerEffectGenerator::SetHost(nullptr) == &host); CHECK(MRippleZoneWideEffectGenerator::SetHost(previous) == &sentinel);
	auto target = Target(); CHECK(!Generate(world, Info(), target)); CHECK(calls.empty());
}

TEST(BloodyBreakerEffectGenerator, RejectedSubmissionConsumesItsMarkersWithoutTakingTheOriginal)
{
	World world; const MBloodyBreakerEffectHost rejecting{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) { effect->SetLink(42, Target(4, 94).release()); return false; },
	};
	MBloodyBreakerEffectGenerator::SetHost(&rejecting); auto target = Target(); Generate(world, Info(), target);
	CHECK(removedTargets == std::vector<int>(5, 94)); CheckTarget(*target); CHECK_EQ(6, frameRequests.size());
}

TEST(BloodyBreakerEffectGenerator, EarlyQueueExceptionConsumesTheEffectAndLeavesTheOriginalWithItsCaller)
{
	World world; const MBloodyBreakerEffectHost throwing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool { effect->SetLink(42, Target(4, 94).release()); throw std::runtime_error("queue"); },
	};
	MBloodyBreakerEffectGenerator::SetHost(&throwing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK(removedTargets == std::vector<int>{94}); CheckTarget(*target); CHECK_EQ(1, frameRequests.size());
}

TEST(BloodyBreakerEffectGenerator, LateQueueExceptionKeepsTheAcceptedCenterOwningTheOriginal)
{
	World world; const MBloodyBreakerEffectHost throwing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			if (submissions == 1) { effect->SetLink(42, Target(4, 94).release()); throw std::runtime_error("queue"); }
			return host.Queue(std::move(effect));
		},
	};
	MBloodyBreakerEffectGenerator::SetHost(&throwing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, effects.size()); CHECK(effects.front()->GetEffectTarget() == target.get());
	if (effects.front()->GetEffectTarget() == target.get()) target.release();
	CheckTarget(*info.pEffectTarget); CHECK(removedTargets == std::vector<int>{94}); ClearEffects(); CHECK(removedTargets == std::vector<int>({94, 73}));
}

TEST(BloodyBreakerEffectGenerator, FinalMetadataExceptionKeepsTheAcceptedCenterOwningTheOriginal)
{
	World world; const MBloodyBreakerEffectHost throwing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MBloodyBreakerEffectSprite& result) {
			if (submissions == 1) throw std::runtime_error("metadata");
			return host.Sprite(type, result);
		},
		.MaxFrames = host.MaxFrames, .Queue = host.Queue,
	};
	MBloodyBreakerEffectGenerator::SetHost(&throwing); auto target = Target(1); auto info = Info(); info.pEffectTarget = target.get(); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, effects.size()); CHECK(effects.front()->GetEffectTarget() == target.get());
	if (effects.front()->GetEffectTarget() == target.get()) target.release();
	CheckTarget(*info.pEffectTarget, 1); CHECK(removedTargets.empty()); ClearEffects(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(BloodyBreakerEffectGenerator, AnimationLengthsNarrowWithoutChangingPatternOrDuration)
{
	World world;
	for (const int frames : {-1, 0, 256, 258})
	{
		ClearEffects(); maxFrames = frames; auto target = Target(); CHECK(Generate(world, Info(), target)); CHECK_EQ(5, effects.size());
		for (const auto& effect : effects) { CHECK_EQ(static_cast<BYTE>(frames), effect->GetMaxFrame()); CHECK_EQ(129, effect->GetEndFrame()); }
	}
}

TEST(BloodyBreakerEffectGenerator, RealUpdatesAnimateWithoutMovingAndStopAtTheDeadline)
{
	World world; auto target = Target(); CHECK(Generate(world, Info(), target)); auto& effect = *effects.front();
	CHECK(effect.Update()); CHECK_EQ(192, effect.GetPixelX()); CHECK_EQ(120, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ()); CHECK_EQ(1, effect.GetFrame()); CHECK_EQ(8, effect.GetLight());
	frameNow = 129; CHECK(!effect.Update()); CHECK_EQ(2, effect.GetFrame()); CHECK_EQ(9, effect.GetLight()); CHECK_EQ(192, effect.GetPixelX());
}

TEST(BloodyBreakerEffectGenerator, DeadlinesRetainFiniteSentinelsClockWrapAndMissingClockFallback)
{
	World world; auto target = Target(); auto info = Info(); info.count = 65535; info.linkCount = MAX_LINKCOUNT; CHECK(Generate(world, info, target));
	for (const auto& effect : effects) { CHECK_EQ(65634, effect->GetEndFrame()); CHECK_EQ(65634, effect->GetEndLinkFrame()); }
	ClearEffects(); target = Target(); frameNow = 0xFFFFFFFEu; CHECK(Generate(world, Info(), target));
	for (const auto& effect : effects) { CHECK_EQ(27, effect->GetEndFrame()); CHECK_EQ(2, effect->GetEndLinkFrame()); }
	ClearEffects(); target = Target(); MEffect::SetHost(nullptr); CHECK(Generate(world, Info(), target));
	for (const auto& effect : effects) { CHECK_EQ(29, effect->GetEndFrame()); CHECK_EQ(4, effect->GetEndLinkFrame()); CHECK_EQ(0, effect->GetLight()); CHECK(!effect->Update()); }
}
