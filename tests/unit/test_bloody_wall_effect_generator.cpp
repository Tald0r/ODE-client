#include "test_framework.h"
#include "MBloodyWallEffectGenerator.h"
#include "MBloodyBreakerEffectGenerator.h"
#include "MEffect.h"
#include "EffectSpriteTypeDef.h"

#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MBloodyWallEffectSprite sprite;
int maxFrames, submissions, acceptance;
bool spriteAvailable, framesAvailable;
std::vector<int> calls, spriteRequests, slots, removedTargets;
struct FrameRequest { BYTE blt; TYPE_FRAMEID id; bool operator==(const FrameRequest&) const = default; };
std::vector<FrameRequest> frameRequests;
std::vector<std::unique_ptr<MEffect>> effects;
struct Point { int x, y; bool operator==(const Point&) const = default; };
// Literal row order at source tile (5,5).
constexpr Point rows[8][5] = {
	{{5, 3}, {5, 4}, {5, 5}, {5, 6}, {5, 7}},
	{{4, 4}, {4, 5}, {5, 5}, {5, 6}, {6, 6}},
	{{3, 5}, {4, 5}, {5, 5}, {6, 5}, {7, 5}},
	{{6, 4}, {6, 5}, {5, 5}, {5, 6}, {4, 6}},
	{{5, 3}, {5, 4}, {5, 5}, {5, 6}, {5, 7}},
	{{6, 6}, {6, 5}, {5, 5}, {5, 4}, {4, 4}},
	{{3, 5}, {4, 5}, {5, 5}, {6, 5}, {7, 5}},
	{{6, 4}, {5, 4}, {5, 5}, {4, 5}, {4, 6}},
};
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(4); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE frame) { calls.push_back(3); return 7 + frame; },
};
const MEffectTargetHost targetHost{.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); }};
const MBloodyWallEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MBloodyWallEffectSprite& result) {
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
	const MBloodyWallEffectHost* previousGenerator = MBloodyWallEffectGenerator::SetHost(&host);
	MBloodyWallEffectGenerator generator;
	World()
	{
		ClearEffects(); removedTargets.clear(); frameNow = 100; sprite = {BLT_EFFECT, 12, false}; maxFrames = 3;
		spriteAvailable = framesAvailable = true; acceptance = 31;
	}
	~World()
	{
		effects.clear(); MBloodyWallEffectGenerator::SetHost(previousGenerator);
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

void CheckCopy(const MEffectTarget& target, int x, int y, int z = 17)
{
	CHECK_EQ(x, target.GetX()); CHECK_EQ(y, target.GetY()); CHECK_EQ(z, target.GetZ()); CHECK_EQ(123, target.GetID());
	CHECK_EQ(73, target.GetEffectID()); CHECK_EQ(4, target.GetCurrentPhase()); CHECK_EQ(255, target.GetMaxPhase()); CHECK_EQ(31, target.GetDelayFrame());
	CHECK_EQ(OBJECTID_NULL, target.GetServerID()); CHECK(!target.IsExistResult()); CHECK(!target.IsResultTime());
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
void ClearEffectsCheckingCopyDestruction(const MEffectTarget* original, int expectedCopies)
{
	int destroyed = 0;
	for (const auto& effect : effects)
	{
		auto* target = effect->GetEffectTarget();
		if (target == original) continue;
		CHECK(target != nullptr); CHECK(!target->IsExistResult());
		auto result = std::make_unique<MActionResult>(); result->Add(new CopyResultMarker(destroyed)); target->SetResult(result.release());
	}
	effects.clear(); CHECK_EQ(expectedCopies, destroyed);
}
}

TEST(BloodyWallEffectGenerator, ConfiguresFiveRealEffectsAndRefreshesMetadataAfterTheFinalAttempt)
{
	World world; CHECK_EQ(EFFECTGENERATORID_BLOODY_WALL, world.generator.GetID()); auto target = Target(); auto* original = target.get();
	std::srand(61); const int next = std::rand(); std::srand(61); CHECK(Generate(world, Info(), target)); CHECK_EQ(next, std::rand());
	CHECK_EQ(5, effects.size()); CHECK_EQ(5, submissions); CHECK(spriteRequests == std::vector<int>(6, 17));
	CHECK(frameRequests == std::vector<FrameRequest>(6, {BLT_EFFECT, 12}));
	std::vector<int> expected{1, 2}; for (int i = 0; i < 5; ++i) expected.insert(expected.end(), {3, 4, 5, 1, 2}); CHECK(calls == expected);
	for (size_t i = 0; i < effects.size(); ++i)
	{
		auto& effect = *effects[i]; CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame());
		CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight()); CHECK_EQ(240, effect.GetPixelX()); CHECK_EQ(rows[0][i].y * 24, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ());
		CHECK_EQ(0, effect.GetDirection()); CHECK_EQ(10, effect.GetStepPixel()); CHECK_EQ(3, effect.GetPower()); CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame());
		CHECK_EQ(42, effect.GetActionInfo()); CHECK(!effect.IsMulti()); CHECK(!effect.IsDelayFrame()); CHECK_EQ(i == 0, effect.GetEffectTarget() == original);
		if (i != 0) CheckCopy(*effect.GetEffectTarget(), 900, 800 + (static_cast<int>(i) - 2) * 24);
	}
	CheckTarget(*original); ClearEffectsCheckingCopyDestruction(original, 4); CHECK(removedTargets == std::vector<int>{73});
}

TEST(BloodyWallEffectGenerator, AllEightDirectionsKeepTheirLiteralOrderAndDestinationRelativeCopies)
{
	World world;
	for (int direction = 0; direction < 8; ++direction)
	{
		ClearEffects(); auto target = Target(); auto* original = target.get(); auto info = Info(); info.direction = static_cast<BYTE>(direction);
		CHECK(Generate(world, info, target)); CHECK_EQ(5, effects.size()); CheckTarget(*original);
		for (size_t i = 0; i < effects.size(); ++i)
		{
			CHECK_EQ(rows[direction][i].x, effects[i]->GetX()); CHECK_EQ(rows[direction][i].y, effects[i]->GetY()); CHECK_EQ(direction, effects[i]->GetDirection());
			if (i) CheckCopy(*effects[i]->GetEffectTarget(), 900 + (rows[direction][i].x - 5) * 48, 800 + (rows[direction][i].y - 5) * 24);
		}
	}
}

TEST(BloodyWallEffectGenerator, EveryAcceptanceMaskGivesTheFirstAcceptedEffectTheOriginal)
{
	World world;
	for (int mask = 0; mask < 32; ++mask)
	{
		ClearEffects(); acceptance = mask; auto target = Target(); auto* original = target.get();
		CHECK_EQ(mask != 0, Generate(world, Info(), target)); CHECK_EQ(5, submissions); CheckTarget(*original); CHECK_EQ(mask == 0, target != nullptr);
		for (size_t i = 0; i < effects.size(); ++i)
		{
			CHECK_EQ(i == 0, effects[i]->GetEffectTarget() == original); CHECK_EQ(rows[0][slots[i]].y, effects[i]->GetY());
			if (i) CheckCopy(*effects[i]->GetEffectTarget(), 900, 800 + (slots[i] - 2) * 24);
		}
	}
}

TEST(BloodyWallEffectGenerator, TargetlessRowsReportAnyAcceptanceAndLinkOnlyTheAction)
{
	World world;
	for (int mask = 0; mask < 32; ++mask)
	{
		ClearEffects(); acceptance = mask; CHECK_EQ(mask != 0, world.generator.Generate(Info())); CHECK_EQ(5, submissions);
		for (const auto& effect : effects) { CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(42, effect->GetActionInfo()); }
	}
}

TEST(BloodyWallEffectGenerator, PhaseAndPowerBytesDoNotChangeTheFivePositions)
{
	World world; acceptance = 1;
	for (int value = 0; value < 256; ++value)
	{
		ClearEffects(); auto target = Target(static_cast<BYTE>(value)); auto* original = target.get(); auto info = Info(); info.power = static_cast<BYTE>(value);
		CHECK(Generate(world, info, target)); CHECK_EQ(5, submissions); CHECK_EQ(1, effects.size()); CheckTarget(*original, static_cast<BYTE>(value));
		CHECK_EQ(value, effects.front()->GetPower()); CHECK_EQ(5, effects.front()->GetX()); CHECK_EQ(3, effects.front()->GetY());
	}
}

TEST(BloodyWallEffectGenerator, RandomStartVariantCyclesAfterEveryAttemptWithTheInitialBlitType)
{
	World world;
	const MBloodyWallEffectHost variants{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MBloodyWallEffectSprite& result) {
			const bool available = host.Sprite(type, result); const int variant = type - EFFECTSPRITETYPE_BLOODY_WALL_1;
			result = {static_cast<BYTE>(BLT_EFFECT + variant), static_cast<TYPE_FRAMEID>(20 + variant), false}; return available;
		},
		.MaxFrames = host.MaxFrames, .Queue = host.Queue,
	};
	MBloodyWallEffectGenerator::SetHost(&variants);
	for (int inputVariant = 0; inputVariant < 3; ++inputVariant) for (const int mask : {0, 10, 31})
	{
		ClearEffects(); acceptance = mask; auto target = Target(); auto info = Info(); info.effectSpriteType = static_cast<TYPE_EFFECTSPRITETYPE>(EFFECTSPRITETYPE_BLOODY_WALL_1 + inputVariant);
		std::srand(71); const int first = std::rand() % 3; const int next = std::rand(); std::srand(71);
		CHECK_EQ(mask != 0, Generate(world, info, target)); CHECK_EQ(next, std::rand()); CHECK_EQ(6, spriteRequests.size()); CHECK_EQ(6, frameRequests.size());
		for (size_t i = 0; i < spriteRequests.size(); ++i)
		{
			CHECK_EQ(EFFECTSPRITETYPE_BLOODY_WALL_1 + (first + i) % 3, spriteRequests[i]);
			CHECK_EQ(BLT_EFFECT + first, frameRequests[i].blt); CHECK_EQ(20 + (first + i) % 3, frameRequests[i].id);
		}
		for (size_t i = 0; i < effects.size(); ++i) { CHECK_EQ(BLT_EFFECT + first, effects[i]->GetBltType()); CHECK_EQ(20 + (first + slots[i]) % 3, effects[i]->GetFrameID()); }
	}
}

TEST(BloodyWallEffectGenerator, RepeatingFramesConsumeRandomnessOnlyForAcceptedEffects)
{
	World world; sprite.repeatFrame = true; maxFrames = 7;
	for (int mask = 0; mask < 32; ++mask)
	{
		ClearEffects(); acceptance = mask; std::vector<int> frames; std::srand(83);
		for (int i = 0; i < 5; ++i) if (mask & (1 << i)) frames.push_back(std::rand() % 7);
		const int next = std::rand(); std::srand(83); CHECK_EQ(mask != 0, world.generator.Generate(Info())); CHECK_EQ(next, std::rand());
		CHECK_EQ(frames.size(), effects.size());
		for (size_t i = 0; i < effects.size(); ++i) { CHECK_EQ(frames[i], effects[i]->GetFrame()); CHECK_EQ(7, effects[i]->GetLight()); }
	}
}

TEST(BloodyWallEffectGenerator, VariantSelectionPrecedesPerAcceptedEffectFrameRandomness)
{
	World world; sprite.repeatFrame = true; maxFrames = 5; acceptance = 10; auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_BLOODY_WALL_3;
	std::srand(37); const int variant = std::rand() % 3; const int first = std::rand() % 5, second = std::rand() % 5, next = std::rand(); std::srand(37);
	CHECK(world.generator.Generate(info)); CHECK_EQ(next, std::rand()); CHECK_EQ(EFFECTSPRITETYPE_BLOODY_WALL_1 + variant, spriteRequests.front());
	CHECK_EQ(first, effects[0]->GetFrame()); CHECK_EQ(second, effects[1]->GetFrame()); CHECK(slots == std::vector<int>({1, 3}));
}

TEST(BloodyWallEffectGenerator, VariantRandomnessIsConsumedEvenWhenInitialMetadataIsMissing)
{
	World world; MBloodyWallEffectGenerator::SetHost(nullptr); auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_BLOODY_WALL_2;
	std::srand(97); std::rand(); const int next = std::rand(); std::srand(97);
	CHECK(!world.generator.Generate(info)); CHECK_EQ(next, std::rand()); CHECK(calls.empty()); CHECK(effects.empty());
}

TEST(BloodyWallEffectGenerator, InitialRepeatPolicyIsRetainedAsLaterMetadataChanges)
{
	World world; const MBloodyWallEffectHost changing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) { sprite.repeatFrame = !sprite.repeatFrame; return host.Queue(std::move(effect)); },
	};
	MBloodyWallEffectGenerator::SetHost(&changing);
	for (const bool repeats : {false, true})
	{
		ClearEffects(); sprite.repeatFrame = repeats; std::vector<int> frames; std::srand(41);
		for (int i = 0; i < 5; ++i) frames.push_back(repeats ? std::rand() % 3 : 0);
		const int next = std::rand(); std::srand(41); CHECK(world.generator.Generate(Info())); CHECK_EQ(next, std::rand());
		for (size_t i = 0; i < effects.size(); ++i) CHECK_EQ(frames[i], effects[i]->GetFrame());
	}
}

TEST(BloodyWallEffectGenerator, SourceAndFinalTileNarrowingPreserveWrappedPositions)
{
	World world; struct Source { int x, y, expectedX, expectedY; };
	for (const Source source : {Source{261, 130, 5, 3}, {-1, -1, 0, 65534}, {-48, -24, 65535, 65533}, {3145728, 1572864, 0, 65534}})
	{
		ClearEffects(); auto info = Info(); info.x0 = source.x; info.y0 = source.y; CHECK(world.generator.Generate(info));
		CHECK_EQ(source.expectedX, effects.front()->GetX()); CHECK_EQ(source.expectedY, effects.front()->GetY());
	}
	ClearEffects(); auto info = Info(); info.x0 = info.y0 = 0; CHECK(world.generator.Generate(info));
	CHECK(Positions() == std::vector<Point>({{0, 65534}, {0, 65535}, {0, 0}, {0, 1}, {0, 2}}));
}

TEST(BloodyWallEffectGenerator, ExtremeSourcePixelsAndHeightsPreserveNarrowingAndTargetCopies)
{
	World world; const int low = (std::numeric_limits<int>::min)(), high = (std::numeric_limits<int>::max)();
	for (const int source : {low, high})
	{
		ClearEffects(); auto target = Target(); auto info = Info(); info.x0 = info.y0 = info.z0 = source; CHECK(Generate(world, info, target));
		CHECK_EQ(source == low ? 21846 : 43690, effects.front()->GetX()); CHECK_EQ(source == low ? 43689 : 21843, effects.front()->GetY());
		CHECK_EQ(source, effects.front()->GetPixelZ()); CheckCopy(*effects[1]->GetEffectTarget(), 900, 776, source);
	}
}

TEST(BloodyWallEffectGenerator, MissingMetadataRejectsBeforeConstructionOrTargetTransfer)
{
	World world; const MBloodyWallEffectHost empty{}, noFrames{.Sprite = host.Sprite, .Queue = host.Queue};
	for (const auto* service : {static_cast<const MBloodyWallEffectHost*>(nullptr), &empty, &noFrames, &host})
	{
		ClearEffects(); MBloodyWallEffectGenerator::SetHost(service); framesAvailable = false; auto target = Target();
		CHECK(!Generate(world, Info(), target)); CHECK_EQ(0, submissions); CHECK(effects.empty()); CheckTarget(*target);
		CHECK(calls == (service == &host ? std::vector<int>{1, 2} : service == &noFrames ? std::vector<int>{1} : std::vector<int>{}));
	}
	ClearEffects(); spriteAvailable = false; auto target = Target(); CHECK(!Generate(world, Info(), target)); CHECK(calls == std::vector<int>{1});
}

TEST(BloodyWallEffectGenerator, MissingQueueDiscardsEffectsAndKeepsTheOriginalWithItsCaller)
{
	World world; const MBloodyWallEffectHost noQueue{.Sprite = host.Sprite, .MaxFrames = host.MaxFrames}; MBloodyWallEffectGenerator::SetHost(&noQueue);
	auto target = Target(); CHECK(!Generate(world, Info(), target)); CHECK(target != nullptr); CHECK(effects.empty()); CHECK_EQ(0, submissions); CheckTarget(*target);
	std::vector<int> expected{1, 2}; for (int i = 0; i < 5; ++i) expected.insert(expected.end(), {3, 4, 1, 2}); CHECK(calls == expected);
}

TEST(BloodyWallEffectGenerator, SpriteAndFrameCallbacksCanInstallTheFollowingServices)
{
	World world;
	const MBloodyWallEffectHost fromSprite{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MBloodyWallEffectSprite& result) { MBloodyWallEffectGenerator::SetHost(&host); return host.Sprite(type, result); },
	};
	const MBloodyWallEffectHost fromFrames{
		.Sprite = host.Sprite,
		.MaxFrames = [](BYTE blt, TYPE_FRAMEID id, int& count) { MBloodyWallEffectGenerator::SetHost(&host); return host.MaxFrames(blt, id, count); },
	};
	for (const auto* service : {&fromSprite, &fromFrames})
	{
		ClearEffects(); MBloodyWallEffectGenerator::SetHost(service); auto target = Target(); CHECK(Generate(world, Info(), target)); CHECK_EQ(5, effects.size());
	}
}

TEST(BloodyWallEffectGenerator, MissingMetadataAfterSubmissionPreservesEarlierTargetOwnership)
{
	World world; static int removeAt;
	const MBloodyWallEffectHost removing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) { const bool accepted = host.Queue(std::move(effect)); if (submissions == removeAt) MBloodyWallEffectGenerator::SetHost(nullptr); return accepted; },
	};
	for (const int mask : {30, 31}) for (int stop = 1; stop <= 5; ++stop)
	{
		ClearEffects(); acceptance = mask; removeAt = stop; MBloodyWallEffectGenerator::SetHost(&removing); auto target = Target(); auto* original = target.get();
		CHECK_EQ(mask == 31 || stop > 1, Generate(world, Info(), target)); CHECK_EQ(stop, submissions); CHECK_EQ(stop, spriteRequests.size()); CheckTarget(*original);
	}
}

TEST(BloodyWallEffectGenerator, UnavailableFramesAfterSubmissionStopWithoutLosingTheFirstAcceptedEffect)
{
	World world;
	const MBloodyWallEffectHost removing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) { framesAvailable = false; return host.Queue(std::move(effect)); },
	};
	MBloodyWallEffectGenerator::SetHost(&removing);
	for (const int mask : {30, 31})
	{
		ClearEffects(); framesAvailable = true; acceptance = mask; auto target = Target(); auto* original = target.get();
		CHECK_EQ(mask == 31, Generate(world, Info(), target)); CHECK_EQ(1, submissions); CHECK_EQ(2, frameRequests.size()); CheckTarget(*original);
	}
}

TEST(BloodyWallEffectGenerator, ChangedMetadataAffectsTheNextAttemptButNotTheOriginalBlitType)
{
	World world;
	const MBloodyWallEffectHost changing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) { sprite = {BLT_SHADOW, 21}; maxFrames = 258; return host.Queue(std::move(effect)); },
	};
	MBloodyWallEffectGenerator::SetHost(&changing); auto target = Target(); CHECK(Generate(world, Info(), target));
	for (size_t i = 0; i < effects.size(); ++i) { CHECK_EQ(BLT_EFFECT, effects[i]->GetBltType()); CHECK_EQ(i == 0 ? 12 : 21, effects[i]->GetFrameID()); CHECK_EQ(i == 0 ? 3 : 2, effects[i]->GetMaxFrame()); }
	for (const auto& request : frameRequests) CHECK_EQ(BLT_EFFECT, request.blt);
	ClearEffects(); target = Target(); CHECK(Generate(world, Info(), target));
	for (const auto& effect : effects) CHECK_EQ(BLT_SHADOW, effect->GetBltType());
}

TEST(BloodyWallEffectGenerator, InstallerIsIndependentOfOtherGenerators)
{
	World world; const MBloodyBreakerEffectHost sentinel{}; const auto* previous = MBloodyBreakerEffectGenerator::SetHost(&sentinel);
	CHECK(MBloodyWallEffectGenerator::SetHost(nullptr) == &host); CHECK(MBloodyBreakerEffectGenerator::SetHost(previous) == &sentinel);
	auto target = Target(); CHECK(!Generate(world, Info(), target)); CHECK(calls.empty());
}

TEST(BloodyWallEffectGenerator, RejectedSubmissionConsumesItsMarkersWithoutTakingTheOriginal)
{
	World world; const MBloodyWallEffectHost rejecting{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) { effect->SetLink(42, Target(4, 94).release()); return false; },
	};
	MBloodyWallEffectGenerator::SetHost(&rejecting); auto target = Target(); CHECK(!Generate(world, Info(), target));
	CHECK(removedTargets == std::vector<int>(5, 94)); CheckTarget(*target); CHECK_EQ(6, frameRequests.size());
}

TEST(BloodyWallEffectGenerator, EarlyQueueExceptionConsumesTheEffectAndLeavesTheOriginalWithItsCaller)
{
	World world; const MBloodyWallEffectHost throwing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool { effect->SetLink(42, Target(4, 94).release()); throw std::runtime_error("queue"); },
	};
	MBloodyWallEffectGenerator::SetHost(&throwing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK(removedTargets == std::vector<int>{94}); CheckTarget(*target); CHECK_EQ(1, frameRequests.size());
}

TEST(BloodyWallEffectGenerator, LateQueueExceptionKeepsTheFirstAcceptedEffectOwningTheOriginal)
{
	World world; const MBloodyWallEffectHost throwing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			if (submissions == 1) { effect->SetLink(42, Target(4, 94).release()); throw std::runtime_error("queue"); }
			return host.Queue(std::move(effect));
		},
	};
	MBloodyWallEffectGenerator::SetHost(&throwing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, effects.size()); CHECK(effects.front()->GetEffectTarget() == target.get());
	if (effects.front()->GetEffectTarget() == target.get()) target.release();
	CheckTarget(*info.pEffectTarget); CHECK(removedTargets == std::vector<int>{94}); ClearEffects(); CHECK(removedTargets == std::vector<int>({94, 73}));
}

TEST(BloodyWallEffectGenerator, MetadataExceptionAfterAcceptanceKeepsTheFirstAcceptedEffectOwningTheOriginal)
{
	World world; const MBloodyWallEffectHost throwing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MBloodyWallEffectSprite& result) {
			if (submissions == 1) throw std::runtime_error("metadata");
			return host.Sprite(type, result);
		},
		.MaxFrames = host.MaxFrames, .Queue = host.Queue,
	};
	MBloodyWallEffectGenerator::SetHost(&throwing); auto target = Target(1); auto info = Info(); info.pEffectTarget = target.get(); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, effects.size()); CHECK(effects.front()->GetEffectTarget() == target.get());
	if (effects.front()->GetEffectTarget() == target.get()) target.release();
	CheckTarget(*info.pEffectTarget, 1); CHECK(removedTargets.empty()); ClearEffects(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(BloodyWallEffectGenerator, AnimationLengthsNarrowWithoutChangingPatternOrDuration)
{
	World world;
	for (const int frames : {-1, 0, 256, 258})
	{
		ClearEffects(); maxFrames = frames; auto target = Target(); CHECK(Generate(world, Info(), target)); CHECK_EQ(5, effects.size());
		for (const auto& effect : effects) { CHECK_EQ(static_cast<BYTE>(frames), effect->GetMaxFrame()); CHECK_EQ(129, effect->GetEndFrame()); }
	}
}

TEST(BloodyWallEffectGenerator, RealUpdatesAnimateWithoutMovingAndStopAtTheDeadline)
{
	World world; auto target = Target(); CHECK(Generate(world, Info(), target)); auto& effect = *effects.front();
	CHECK(effect.Update()); CHECK_EQ(240, effect.GetPixelX()); CHECK_EQ(72, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ()); CHECK_EQ(1, effect.GetFrame()); CHECK_EQ(8, effect.GetLight());
	frameNow = 129; CHECK(!effect.Update()); CHECK_EQ(2, effect.GetFrame()); CHECK_EQ(9, effect.GetLight()); CHECK_EQ(240, effect.GetPixelX());
}

TEST(BloodyWallEffectGenerator, DeadlinesRetainFiniteSentinelsClockWrapAndMissingClockFallback)
{
	World world; auto target = Target(); auto info = Info(); info.count = 65535; info.linkCount = MAX_LINKCOUNT; CHECK(Generate(world, info, target));
	for (const auto& effect : effects) { CHECK_EQ(65634, effect->GetEndFrame()); CHECK_EQ(65634, effect->GetEndLinkFrame()); }
	ClearEffects(); target = Target(); frameNow = 0xFFFFFFFEu; CHECK(Generate(world, Info(), target));
	for (const auto& effect : effects) { CHECK_EQ(27, effect->GetEndFrame()); CHECK_EQ(2, effect->GetEndLinkFrame()); }
	ClearEffects(); target = Target(); MEffect::SetHost(nullptr); CHECK(Generate(world, Info(), target));
	for (const auto& effect : effects) { CHECK_EQ(29, effect->GetEndFrame()); CHECK_EQ(4, effect->GetEndLinkFrame()); CHECK_EQ(0, effect->GetLight()); CHECK(!effect->Update()); }
}

TEST(BloodyWallEffectGenerator, DirectionPastTheEightRowsRejectsBeforeConstruction)
{
	World world; auto target = Target(); auto* original = target.get(); auto info = Info(); info.direction = 8;
	CHECK(!Generate(world, info, target)); CHECK_EQ(0, submissions); CHECK(effects.empty());
	CHECK(calls == std::vector<int>({1, 2})); CheckTarget(*original);
}

TEST(BloodyWallEffectGenerator, EveryInvalidDirectionRejectsWithAndWithoutATarget)
{
	World world;
	for (int direction = 8; direction < 256; ++direction) for (const bool linked : {false, true})
	{
		ClearEffects(); auto target = linked ? Target() : std::unique_ptr<MEffectTarget>{}; auto info = Info(); info.direction = static_cast<BYTE>(direction);
		CHECK(!Generate(world, info, target)); CHECK_EQ(0, submissions); CHECK(effects.empty()); CHECK(calls == std::vector<int>({1, 2}));
		if (target) CheckTarget(*target);
	}
}

TEST(BloodyWallEffectGenerator, InvalidDirectionsKeepInitialVariantRandomnessAndMetadataOrdering)
{
	World world; sprite.repeatFrame = true; auto info = Info(); info.direction = 255; info.effectSpriteType = EFFECTSPRITETYPE_BLOODY_WALL_3;
	for (const bool available : {false, true})
	{
		ClearEffects(); spriteAvailable = available; std::srand(11); const int variant = std::rand() % 3, next = std::rand(); std::srand(11);
		CHECK(!world.generator.Generate(info)); CHECK_EQ(next, std::rand()); CHECK_EQ(EFFECTSPRITETYPE_BLOODY_WALL_1 + variant, spriteRequests.front());
		CHECK(calls == (available ? std::vector<int>{1, 2} : std::vector<int>{1})); CHECK_EQ(0, submissions);
	}
}

TEST(BloodyWallEffectGenerator, EmptyRepeatingAnimationKeepsFrameZeroWithoutDrawingRandomness)
{
	World world; sprite.repeatFrame = true; maxFrames = 0; acceptance = 1; auto target = Target(); auto* original = target.get();
	std::srand(1); const int next = std::rand(); std::srand(1);
	CHECK(Generate(world, Info(), target)); CHECK_EQ(next, std::rand()); CHECK_EQ(5, submissions); CHECK_EQ(1, effects.size());
	CHECK_EQ(0, effects.front()->GetFrame()); CHECK_EQ(0, effects.front()->GetMaxFrame()); CHECK_EQ(7, effects.front()->GetLight());
	CHECK(effects.front()->GetEffectTarget() == original); CheckTarget(*original);
}

TEST(BloodyWallEffectGenerator, NonpositiveFrameCountsKeepAcceptanceCopiesAndByteNarrowing)
{
	World world; sprite.repeatFrame = true;
	for (const int frames : {0, -1, -3, (std::numeric_limits<int>::min)()}) for (const int mask : {0, 10, 31})
	{
		ClearEffects(); maxFrames = frames; acceptance = mask; auto target = Target(); auto* original = target.get();
		std::srand(7); const int next = std::rand(); std::srand(7); CHECK_EQ(mask != 0, Generate(world, Info(), target)); CHECK_EQ(next, std::rand());
		CHECK_EQ(5, submissions); CHECK_EQ(6, frameRequests.size()); CheckTarget(*original);
		for (size_t i = 0; i < effects.size(); ++i)
		{
			CHECK_EQ(0, effects[i]->GetFrame()); CHECK_EQ(static_cast<BYTE>(frames), effects[i]->GetMaxFrame());
			CHECK_EQ(i == 0, effects[i]->GetEffectTarget() == original);
			if (i) CheckCopy(*effects[i]->GetEffectTarget(), 900, 800 + (slots[i] - 2) * 24);
		}
	}
}

TEST(BloodyWallEffectGenerator, RefreshedFrameLengthsControlOnlyTheirOwnRandomSelections)
{
	World world; sprite.repeatFrame = true;
	const MBloodyWallEffectHost changing{
		.Sprite = host.Sprite,
		.MaxFrames = [](BYTE blt, TYPE_FRAMEID id, int& count) {
			const bool available = host.MaxFrames(blt, id, count); count = submissions % 2 == 0 ? 0 : 7; return available;
		},
		.Queue = host.Queue,
	};
	MBloodyWallEffectGenerator::SetHost(&changing); std::srand(23); const int first = std::rand() % 7, second = std::rand() % 7, next = std::rand(); std::srand(23);
	auto target = Target(); CHECK(Generate(world, Info(), target)); CHECK_EQ(next, std::rand());
	CHECK_EQ(0, effects[0]->GetFrame()); CHECK_EQ(first, effects[1]->GetFrame()); CHECK_EQ(0, effects[2]->GetFrame()); CHECK_EQ(second, effects[3]->GetFrame()); CHECK_EQ(0, effects[4]->GetFrame());
	CHECK_EQ(6, frameRequests.size());
}

TEST(BloodyWallEffectGenerator, PositiveFrameLengthsRetainRandomDrawsAndByteAnimationWrapping)
{
	World world; sprite.repeatFrame = true;
	for (const int frames : {1, 3, 255, 256, 258})
	{
		ClearEffects(); maxFrames = frames; std::vector<int> expected; std::srand(47); const int cycle = static_cast<BYTE>(frames) == 0 ? 256 : static_cast<BYTE>(frames);
		for (int i = 0; i < 5; ++i) expected.push_back((std::rand() % frames) % cycle);
		const int next = std::rand(); std::srand(47); CHECK(world.generator.Generate(Info())); CHECK_EQ(next, std::rand());
		for (size_t i = 0; i < effects.size(); ++i) { CHECK_EQ(expected[i], effects[i]->GetFrame()); CHECK_EQ(static_cast<BYTE>(frames), effects[i]->GetMaxFrame()); CHECK_EQ(7, effects[i]->GetLight()); }
	}
}

TEST(BloodyWallEffectGenerator, PositiveDestinationOffsetsSaturateCopiedTargetsAtTheIntegerMaximum)
{
	World world; const int high = (std::numeric_limits<int>::max)();
	const int xs[4] = {high - 48, high, high, high}, ys[4] = {high - 24, high, high, high};
	for (const BYTE direction : {static_cast<BYTE>(DIRECTION_LEFT), static_cast<BYTE>(DIRECTION_DOWN)})
	{
		ClearEffects(); auto target = Target(); auto* original = target.get(); auto info = Info(); info.direction = direction; info.x1 = info.y1 = high;
		CHECK(Generate(world, info, target)); CheckTarget(*original); CHECK_EQ(5, effects.size());
		for (size_t i = 1; i < effects.size(); ++i) CheckCopy(*effects[i]->GetEffectTarget(), direction == DIRECTION_DOWN ? xs[i - 1] : high, direction == DIRECTION_LEFT ? ys[i - 1] : high);
	}
}

TEST(BloodyWallEffectGenerator, NegativeDestinationOffsetsSaturateCopiedTargetsAtTheIntegerMinimum)
{
	World world; const int low = (std::numeric_limits<int>::min)();
	const int xs[4] = {low, low, low + 48, low + 96}, ys[4] = {low, low, low + 24, low + 48};
	for (const BYTE direction : {static_cast<BYTE>(DIRECTION_LEFT), static_cast<BYTE>(DIRECTION_DOWN)})
	{
		ClearEffects(); auto target = Target(); auto* original = target.get(); auto info = Info(); info.direction = direction; info.x1 = info.y1 = low;
		CHECK(Generate(world, info, target)); CheckTarget(*original); CHECK_EQ(5, effects.size());
		for (size_t i = 1; i < effects.size(); ++i) CheckCopy(*effects[i]->GetEffectTarget(), direction == DIRECTION_DOWN ? xs[i - 1] : low, direction == DIRECTION_LEFT ? ys[i - 1] : low);
	}
}

TEST(BloodyWallEffectGenerator, RepresentableDestinationOffsetsKeepExactIntegerCoordinates)
{
	World world; const int x = (std::numeric_limits<int>::max)() - 96, y = (std::numeric_limits<int>::min)() + 48;
	for (int direction = 0; direction < 8; ++direction)
	{
		ClearEffects(); auto target = Target(); auto info = Info(); info.direction = static_cast<BYTE>(direction); info.x1 = x; info.y1 = y;
		CHECK(Generate(world, info, target));
		for (size_t i = 1; i < effects.size(); ++i) CheckCopy(*effects[i]->GetEffectTarget(), x + (rows[direction][i].x - 5) * 48, y + (rows[direction][i].y - 5) * 24);
	}
}

TEST(BloodyWallEffectGenerator, ExtremeDestinationsAreSafeEvenWhenNoTargetCopyIsNeeded)
{
	World world;
	for (const int destination : {(std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()})
		for (int direction = 0; direction < 8; ++direction) for (const int mask : {0, 1, 31}) for (const bool linked : {false, true})
	{
		if (linked && mask == 31) continue;
		ClearEffects(); acceptance = mask; auto target = linked ? Target() : std::unique_ptr<MEffectTarget>{}; auto* original = target.get();
		auto info = Info(); info.x1 = info.y1 = destination; info.direction = static_cast<BYTE>(direction);
		CHECK_EQ(mask != 0, Generate(world, info, target)); CHECK_EQ(5, submissions);
		if (original) CheckTarget(*original);
		for (size_t i = 0; i < effects.size(); ++i)
		{
			CHECK_EQ(rows[direction][slots[i]].x, effects[i]->GetX()); CHECK_EQ(rows[direction][slots[i]].y, effects[i]->GetY());
			CHECK(effects[i]->GetEffectTarget() == original);
		}
	}
}

TEST(BloodyWallEffectGenerator, DeferredFirstAcceptanceKeepsTheOriginalAndSaturatesLaterCopies)
{
	World world; const int high = (std::numeric_limits<int>::max)(), low = (std::numeric_limits<int>::min)();
	for (const int mask : {2, 10, 24, 26})
	{
		ClearEffects(); acceptance = mask; auto target = Target(); auto* original = target.get(); auto info = Info(); info.direction = DIRECTION_LEFTDOWN; info.x1 = high; info.y1 = low;
		CHECK(Generate(world, info, target)); CHECK(effects.front()->GetEffectTarget() == original); CheckTarget(*original);
		for (size_t i = 1; i < effects.size(); ++i) CheckCopy(*effects[i]->GetEffectTarget(), high, low + 24);
	}
}
