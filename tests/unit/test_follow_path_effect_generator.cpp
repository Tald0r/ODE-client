#include "test_framework.h"
#include "MFollowPathEffectGenerator.h"
#include "MSpreadOutEffectGenerator.h"
#include "MLinearEffect.h"
#include "SkillDef.h"

#include <array>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

// This is the existing production cache. Save and restore it so cold-cache
// and warm-cache behavior can be tested without depending on test order.
extern std::vector<POINT> FollowPath[8];

namespace {

DWORD frameNow;
MFixedZoneEffectSprite sprite;
bool spriteAvailable, acceptEffect;
int requestedSprite, submissions;
std::vector<int> calls, removedTargets;
std::vector<std::unique_ptr<MEffect>> effects;
struct Point { int x, y; };
constexpr Point routes[8][5] = {
	{{0, -1}, {-2, 0}, {0, 2}, {2, 0}, {-1, -1}},
	{{0, 2}, {-2, 0}, {0, -2}, {1, 0}, {0, 1}},
	{{-1, 0}, {0, 2}, {2, 0}, {0, -2}, {-1, 1}},
	{{2, 0}, {0, 2}, {-2, 0}, {0, -1}, {1, 0}},
	{{0, 1}, {2, 0}, {0, -2}, {-2, 0}, {1, 1}},
	{{0, -2}, {2, 0}, {0, 2}, {-1, 0}, {0, -1}},
	{{1, 0}, {0, -2}, {-2, 0}, {0, 2}, {1, -1}},
	{{-2, 0}, {0, -2}, {2, 0}, {0, 1}, {-1, 0}},
};
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
		calls.push_back(4); ++submissions; CHECK_EQ(MEffect::EFFECT_LINEAR, effect->GetEffectType());
		CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		if (!acceptEffect) return false;
		effects.push_back(std::move(effect)); return true;
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MFixedZoneEffectHost* previousGenerator = MFollowPathEffectGenerator::SetHost(&host);
	std::array<std::vector<POINT>, 8> savedPaths;
	MFollowPathEffectGenerator generator;
	World()
	{
		for (size_t i = 0; i < savedPaths.size(); ++i) { savedPaths[i] = std::move(FollowPath[i]); FollowPath[i].clear(); }
		frameNow = 100; sprite = {BLT_EFFECT, 12, 3}; spriteAvailable = acceptEffect = true;
		requestedSprite = -1; submissions = 0; effects.clear(); calls.clear(); removedTargets.clear();
	}
	~World()
	{
		effects.clear(); MFollowPathEffectGenerator::SetHost(previousGenerator);
		MEffectTarget::SetHost(previousTarget); MEffect::SetHost(previousEffect);
		for (size_t i = 0; i < savedPaths.size(); ++i) FollowPath[i] = std::move(savedPaths[i]);
	}
};

EFFECTGENERATOR_INFO Info()
{
	EFFECTGENERATOR_INFO info{}; info.nActionInfo = SKILL_WILD_TYPHOON; info.effectSpriteType = 17;
	info.x0 = 240; info.y0 = 120; info.z0 = 17; info.x1 = 900; info.y1 = 800; info.z1 = 99;
	info.direction = 0; info.step = 10; info.count = 30; info.linkCount = 5; info.power = 2; info.creatureID = 123;
	return info;
}

std::unique_ptr<MEffectTarget> Target(BYTE phase = 2, BYTE id = 73)
{
	auto target = std::make_unique<MEffectTarget>(255);
	for (int i = 0; i < phase; ++i) target->NextPhase();
	target->m_EffectID = id; target->Set(777, 888, 999, 456); target->SetServerID(789);
	target->SetDelayFrame(31); target->SetResultTime(); target->SetResult(new MActionResult); return target;
}

void ClearEffects()
{
	effects.clear(); calls.clear(); submissions = 0;
}

bool Generate(World& world, EFFECTGENERATOR_INFO info, std::unique_ptr<MEffectTarget>& target)
{
	info.pEffectTarget = target.get(); const bool result = world.generator.Generate(info);
	if (result) target.release();
	return result;
}

void CheckUnchanged(const MEffectTarget& target)
{
	CHECK_EQ(777, target.GetX()); CHECK_EQ(888, target.GetY()); CHECK_EQ(999, target.GetZ()); CHECK_EQ(456, target.GetID());
	CHECK_EQ(789, target.GetServerID()); CHECK(target.IsExistResult()); CHECK(target.IsResultTime());
}

void CheckPaths()
{
	for (size_t direction = 0; direction < 8; ++direction)
	{
		CHECK_EQ(5, FollowPath[direction].size());
		for (size_t phase = 0; phase < FollowPath[direction].size() && phase < 5; ++phase)
		{
			CHECK_EQ(routes[direction][phase].x, FollowPath[direction][phase].x);
			CHECK_EQ(routes[direction][phase].y, FollowPath[direction][phase].y);
		}
	}
}

int NextRandom(unsigned seed)
{
	std::srand(seed); const int result = std::rand(); std::srand(seed); return result;
}

} // namespace

TEST(FollowPathEffectGenerator, ConfiguresARealLinearEffectWithoutDrawingRandomNumbers)
{
	World world; CHECK_EQ(EFFECTGENERATORID_FOLLOW_PATH, world.generator.GetID());
	auto target = Target(); const int next = NextRandom(61); CHECK(Generate(world, Info(), target));
	CHECK_EQ(next, std::rand()); CHECK_EQ(17, requestedSprite); CHECK_EQ(1, submissions); CHECK_EQ(1, effects.size());
	CHECK(calls == std::vector<int>({1, 2, 3, 4}));
	auto& effect = *effects.front(); CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID());
	CHECK_EQ(3, effect.GetMaxFrame()); CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight());
	CHECK_EQ(240, effect.GetPixelX()); CHECK_EQ(120, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ());
	CHECK_EQ(5, effect.GetX()); CHECK_EQ(5, effect.GetY()); CHECK_EQ(10, effect.GetStepPixel()); CHECK_EQ(2, effect.GetPower());
	CHECK_EQ(0, effect.GetDirection()); CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame());
	CHECK_EQ(SKILL_WILD_TYPHOON, effect.GetActionInfo()); CHECK(!effect.IsMulti()); CHECK(!effect.IsDelayFrame());
}

TEST(FollowPathEffectGenerator, AllFortyRouteStepsAreRelativeToTheSourceTile)
{
	World world;
	for (const Point source : {Point{240, 120}, Point{261, 130}})
	{
		for (int direction = 0; direction < 8; ++direction)
		{
			for (int phase = 2; phase <= 6; ++phase)
			{
				ClearEffects(); auto target = Target(static_cast<BYTE>(phase)); auto* original = target.get(); auto info = Info();
				info.x0 = source.x; info.y0 = source.y; info.direction = static_cast<BYTE>(direction);
				CHECK(Generate(world, info, target)); auto& effect = *effects.front(); CHECK(effect.GetEffectTarget() == original);
				CHECK_EQ(240 + 48 * routes[direction][phase - 2].x, original->GetX());
				CHECK_EQ(120 + 24 * routes[direction][phase - 2].y, original->GetY());
				CHECK_EQ(99, original->GetZ()); CHECK_EQ(123, original->GetID()); CHECK_EQ(direction, effect.GetDirection());
				CHECK_EQ(source.x, effect.GetPixelX()); CHECK_EQ(source.y, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ());
			}
		}
	}
}

TEST(FollowPathEffectGenerator, EveryPhaseByteAcceptsOnlyTheFiveAvailableSteps)
{
	World world;
	for (int direction = 0; direction < 8; ++direction)
	{
		for (int phase = 0; phase < 256; ++phase)
		{
			ClearEffects(); auto target = Target(static_cast<BYTE>(phase)); auto info = Info(); info.direction = static_cast<BYTE>(direction);
			const bool expected = phase >= 2 && phase <= 6; CHECK_EQ(expected, Generate(world, info, target)); CHECK_EQ(expected, submissions);
			CHECK_EQ(expected, effects.size()); CHECK(calls == (expected ? std::vector<int>{1, 2, 3, 4} : std::vector<int>{1}));
			if (target) { CheckUnchanged(*target); CHECK_EQ(phase, target->GetCurrentPhase()); }
		}
	}
}

TEST(FollowPathEffectGenerator, TargetlessTyphoonBuildsTheCacheWithoutCreatingAnEffect)
{
	World world; CHECK(!world.generator.Generate(Info())); CHECK_EQ(0, submissions); CHECK(effects.empty());
	CHECK(calls == std::vector<int>{1}); CheckPaths();
}

TEST(FollowPathEffectGenerator, OtherActionsUseOnlyAnAlreadyBuiltCache)
{
	World world; auto target = Target(); auto info = Info(); info.nActionInfo = 42;
	CHECK(!Generate(world, info, target)); CHECK(calls == std::vector<int>{1}); CheckUnchanged(*target);
	for (const auto& path : FollowPath) CHECK(path.empty());
	CHECK(!world.generator.Generate(Info())); ClearEffects();
	CHECK(Generate(world, info, target)); CHECK_EQ(42, effects.front()->GetActionInfo());
	CHECK_EQ(240, effects.front()->GetEffectTarget()->GetX()); CHECK_EQ(96, effects.front()->GetEffectTarget()->GetY());
}

TEST(FollowPathEffectGenerator, TyphoonRebuildsAllDirectionsEvenWhenPhaseIsRejected)
{
	World world;
	for (int pass = 0; pass < 3; ++pass)
	{
		for (auto& path : FollowPath) path.assign(9, POINT{91, 92});
		auto target = Target(255); CHECK(!Generate(world, Info(), target)); CheckPaths(); CHECK_EQ(0, submissions);
	}
}

TEST(FollowPathEffectGenerator, NonTyphoonKeepsTheExistingCacheAcrossGenerations)
{
	World world; auto target = Target(); CHECK(Generate(world, Info(), target)); ClearEffects();
	for (int direction = 0; direction < 8; ++direction)
	{
		for (int phase = 2; phase <= 6; ++phase)
		{
			ClearEffects(); auto next = Target(static_cast<BYTE>(phase)); auto info = Info(); info.nActionInfo = 42; info.direction = static_cast<BYTE>(direction);
			CHECK(Generate(world, info, next)); const auto* linked = effects.front()->GetEffectTarget();
			CHECK_EQ(240 + 48 * routes[direction][phase - 2].x, linked->GetX()); CHECK_EQ(120 + 24 * routes[direction][phase - 2].y, linked->GetY());
		}
	}
	CheckPaths();
}

TEST(FollowPathEffectGenerator, SourceTilesNarrowBeforeSignedOffsetsWithoutWrappingAgain)
{
	World world;
	struct Source { int x, y, baseX, baseY; };
	for (const Source source : {Source{0, 0, 0, 0}, Source{-1, -1, 0, 0}, Source{-48, -24, 3145680, 1572840}, Source{3145728, 1572864, 0, 0}})
	{
		for (int direction = 0; direction < 8; ++direction)
		{
			for (int phase = 2; phase <= 6; ++phase)
			{
				ClearEffects(); auto target = Target(static_cast<BYTE>(phase)); auto info = Info(); info.x0 = source.x; info.y0 = source.y;
				info.direction = static_cast<BYTE>(direction); CHECK(Generate(world, info, target)); const auto* linked = effects.front()->GetEffectTarget();
				CHECK_EQ(source.baseX + 48 * routes[direction][phase - 2].x, linked->GetX());
				CHECK_EQ(source.baseY + 24 * routes[direction][phase - 2].y, linked->GetY());
			}
		}
	}
}

TEST(FollowPathEffectGenerator, ExtremeSourcesKeepUnsignedTileNarrowingAndSignedOffsets)
{
	World world;
	const int low = (std::numeric_limits<int>::min)(), high = (std::numeric_limits<int>::max)();
	for (const int x : {low, high}) for (const int y : {low, high})
	{
		for (int direction = 0; direction < 8; ++direction) for (int phase = 2; phase <= 6; ++phase)
		{
			ClearEffects(); auto target = Target(static_cast<BYTE>(phase)); auto info = Info(); info.x0 = x; info.y0 = y;
			info.direction = static_cast<BYTE>(direction); CHECK(Generate(world, info, target)); const auto* linked = effects.front()->GetEffectTarget();
			CHECK_EQ(((x == low ? 21846 : 43690) + routes[direction][phase - 2].x) * 48, linked->GetX());
			CHECK_EQ(((y == low ? 43691 : 21845) + routes[direction][phase - 2].y) * 24, linked->GetY());
		}
	}
}

TEST(FollowPathEffectGenerator, AcceptanceRetargetsTheOriginalWithoutLosingItsMetadata)
{
	World world; auto target = Target(); auto* original = target.get(); auto* result = target->GetResult();
	CHECK(Generate(world, Info(), target)); CHECK(effects.front()->GetEffectTarget() == original);
	CHECK_EQ(240, original->GetX()); CHECK_EQ(96, original->GetY()); CHECK_EQ(99, original->GetZ()); CHECK_EQ(123, original->GetID());
	CHECK_EQ(2, original->GetCurrentPhase()); CHECK_EQ(255, original->GetMaxPhase()); CHECK_EQ(31, original->GetDelayFrame());
	CHECK_EQ(73, original->GetEffectID()); CHECK_EQ(789, original->GetServerID()); CHECK(original->GetResult() == result); CHECK(original->IsResultTime());
	CHECK(removedTargets.empty()); ClearEffects(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(FollowPathEffectGenerator, QueueSeesTheOriginalUnchangedUntilItAccepts)
{
	World world; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	static const MEffectTarget* observedTarget; observedTarget = target.get();
	const MFixedZoneEffectHost inspecting{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { CheckUnchanged(*observedTarget); return host.Queue(std::move(effect)); },
	};
	MFollowPathEffectGenerator::SetHost(&inspecting); CHECK(Generate(world, info, target));
	CHECK_EQ(240, observedTarget->GetX()); CHECK_EQ(96, observedTarget->GetY());
}

TEST(FollowPathEffectGenerator, RejectionLeavesOwnershipAndCoordinatesWithTheCaller)
{
	World world; acceptEffect = false; auto target = Target(); CHECK(!Generate(world, Info(), target));
	CHECK_EQ(1, submissions); CHECK(effects.empty()); CHECK(removedTargets.empty()); CheckUnchanged(*target);
	CHECK_EQ(2, target->GetCurrentPhase()); target.reset(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(FollowPathEffectGenerator, MissingMetadataStopsBeforeBuildingOrReplacingPaths)
{
	World world; const MFixedZoneEffectHost empty{};
	for (const auto* service : {static_cast<const MFixedZoneEffectHost*>(nullptr), &empty, &host})
	{
		for (auto& path : FollowPath) path.assign(1, POINT{91, 92});
		ClearEffects(); MFollowPathEffectGenerator::SetHost(service); spriteAvailable = false; auto target = Target();
		CHECK(!Generate(world, Info(), target)); CHECK_EQ(0, submissions); CheckUnchanged(*target);
		CHECK(calls == (service == &host ? std::vector<int>{1} : std::vector<int>{}));
		for (const auto& path : FollowPath) { CHECK_EQ(1, path.size()); CHECK_EQ(91, path.front().x); CHECK_EQ(92, path.front().y); }
	}
}

TEST(FollowPathEffectGenerator, MissingQueueDiscardsTheUnlinkedEffectAfterBuildingPaths)
{
	World world; const MFixedZoneEffectHost noQueue{.Sprite = host.Sprite}; MFollowPathEffectGenerator::SetHost(&noQueue);
	auto target = Target(); CHECK(!Generate(world, Info(), target)); CHECK_EQ(0, submissions); CHECK(effects.empty());
	CHECK(removedTargets.empty()); CheckUnchanged(*target); CHECK(calls == std::vector<int>({1, 2, 3})); CheckPaths();
}

TEST(FollowPathEffectGenerator, SpriteCallbackCanReplaceTheQueue)
{
	World world; const MFixedZoneEffectHost replacing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& result) { MFollowPathEffectGenerator::SetHost(&host); return host.Sprite(type, result); },
	};
	MFollowPathEffectGenerator::SetHost(&replacing); auto target = Target(); CHECK(Generate(world, Info(), target)); CHECK_EQ(1, submissions);
}

TEST(FollowPathEffectGenerator, SpriteCallbackCanRemoveTheQueueAfterReturningMetadata)
{
	World world; const MFixedZoneEffectHost removing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& result) { MFollowPathEffectGenerator::SetHost(nullptr); return host.Sprite(type, result); },
		.Queue = host.Queue,
	};
	MFollowPathEffectGenerator::SetHost(&removing); auto target = Target(); CHECK(!Generate(world, Info(), target)); CheckUnchanged(*target);
	CHECK(calls == std::vector<int>({1, 2, 3})); CHECK(effects.empty()); CheckPaths();
}

TEST(FollowPathEffectGenerator, AcceptedQueueRemovalStillLinksAndRetargetsTheOriginal)
{
	World world; const MFixedZoneEffectHost removing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { MFollowPathEffectGenerator::SetHost(nullptr); return host.Queue(std::move(effect)); },
	};
	MFollowPathEffectGenerator::SetHost(&removing); auto target = Target(); auto* original = target.get(); CHECK(Generate(world, Info(), target));
	CHECK(effects.front()->GetEffectTarget() == original); CHECK_EQ(240, original->GetX()); CHECK_EQ(96, original->GetY());
	ClearEffects(); auto next = Target(); CHECK(!Generate(world, Info(), next)); CHECK(calls.empty()); CheckUnchanged(*next);
}

TEST(FollowPathEffectGenerator, EachGenerationReadsFreshSpriteMetadata)
{
	World world; auto target = Target(); CHECK(Generate(world, Info(), target)); ClearEffects();
	sprite = {BLT_SHADOW, 21, 258}; auto next = Target(); CHECK(Generate(world, Info(), next));
	CHECK_EQ(BLT_SHADOW, effects.front()->GetBltType()); CHECK_EQ(21, effects.front()->GetFrameID()); CHECK_EQ(2, effects.front()->GetMaxFrame());
}

TEST(FollowPathEffectGenerator, InstallerIsIndependentOfSpreadOutGeneration)
{
	World world; const MFixedZoneEffectHost sentinel{}; const auto* previous = MSpreadOutEffectGenerator::SetHost(&sentinel);
	CHECK(MFollowPathEffectGenerator::SetHost(nullptr) == &host); CHECK(MSpreadOutEffectGenerator::SetHost(previous) == &sentinel);
	auto target = Target(); CHECK(!Generate(world, Info(), target)); CHECK(calls.empty());
}

TEST(FollowPathEffectGenerator, RejectingQueueDestroysItsMarkerWithoutTakingTheCallerTarget)
{
	World world; const MFixedZoneEffectHost rejecting{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) { ++submissions; effect->SetLink(42, Target(2, 94).release()); return false; },
	};
	MFollowPathEffectGenerator::SetHost(&rejecting); auto target = Target(); CHECK(!Generate(world, Info(), target));
	CHECK_EQ(1, submissions); CHECK(removedTargets == std::vector<int>{94}); CheckUnchanged(*target);
}

TEST(FollowPathEffectGenerator, QueueExceptionConsumesItsEffectWithoutTakingTheCallerTarget)
{
	World world; const MFixedZoneEffectHost throwing{
		.Sprite = host.Sprite,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool { ++submissions; effect->SetLink(42, Target(2, 94).release()); throw std::runtime_error("queue"); },
	};
	MFollowPathEffectGenerator::SetHost(&throwing); auto target = Target(); bool threw = false;
	try { Generate(world, Info(), target); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, submissions); CHECK(removedTargets == std::vector<int>{94}); CheckUnchanged(*target); CheckPaths();
}

TEST(FollowPathEffectGenerator, MetadataExceptionLeavesTheCacheAndCallerUntouched)
{
	World world; const MFixedZoneEffectHost throwing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MFixedZoneEffectSprite&) -> bool { throw std::runtime_error("sprite"); }, .Queue = host.Queue,
	};
	MFollowPathEffectGenerator::SetHost(&throwing); auto target = Target(); bool threw = false;
	try { Generate(world, Info(), target); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(0, submissions); CHECK(calls.empty()); CheckUnchanged(*target); for (const auto& path : FollowPath) CHECK(path.empty());
}

TEST(FollowPathEffectGenerator, AnimationCountsNarrowWithoutChangingDurationOrRandomState)
{
	World world;
	for (const int frames : {-1, 0, 256, 258})
	{
		ClearEffects(); sprite.maxFrames = frames; auto target = Target(); const int next = NextRandom(89);
		CHECK(Generate(world, Info(), target)); CHECK_EQ(next, std::rand()); CHECK_EQ(static_cast<BYTE>(frames), effects.front()->GetMaxFrame());
		CHECK_EQ(0, effects.front()->GetFrame()); CHECK_EQ(129, effects.front()->GetEndFrame()); CHECK_EQ(104, effects.front()->GetEndLinkFrame());
	}
}

TEST(FollowPathEffectGenerator, ZeroAndMaximumPowerAndStepDoNotChangeTheRoute)
{
	World world;
	for (const BYTE value : {static_cast<BYTE>(0), static_cast<BYTE>(255)})
	{
		ClearEffects(); auto info = Info(); info.step = info.power = value; auto target = Target(); CHECK(Generate(world, info, target));
		CHECK_EQ(value, effects.front()->GetPower()); CHECK_EQ(value, effects.front()->GetStepPixel());
		CHECK_EQ(240, effects.front()->GetEffectTarget()->GetX()); CHECK_EQ(96, effects.front()->GetEffectTarget()->GetY());
	}
}

TEST(FollowPathEffectGenerator, CountAndLinkDeadlinesRemainFiniteAndIndependent)
{
	World world;
	struct Timing { WORD count, link; DWORD end, endLink; };
	for (const Timing time : {Timing{0, 5, 99, 104}, Timing{65535, MAX_LINKCOUNT, 65634, 65634}, Timing{1, 65534, 100, 65633}})
	{
		ClearEffects(); auto info = Info(); info.count = time.count; info.linkCount = time.link; auto target = Target(); CHECK(Generate(world, info, target));
		CHECK_EQ(time.end, effects.front()->GetEndFrame()); CHECK_EQ(time.endLink, effects.front()->GetEndLinkFrame());
	}
}

TEST(FollowPathEffectGenerator, WrappedClockAndMissingBaseServicesKeepTheirFallbacks)
{
	World world; frameNow = 0xFFFFFFFEu; auto target = Target(); CHECK(Generate(world, Info(), target));
	CHECK_EQ(27, effects.front()->GetEndFrame()); CHECK_EQ(2, effects.front()->GetEndLinkFrame()); CHECK(effects.front()->IsEnd());
	ClearEffects(); MEffect::SetHost(nullptr); auto next = Target(); CHECK(Generate(world, Info(), next));
	CHECK_EQ(29, effects.front()->GetEndFrame()); CHECK_EQ(4, effects.front()->GetEndLinkFrame()); CHECK_EQ(0, effects.front()->GetLight());
	CHECK(!effects.front()->Update()); CHECK_EQ(0, effects.front()->GetFrame());
}

TEST(FollowPathEffectGenerator, RealLinearUpdatesReachTheRoutePointWithTheRequestedFacing)
{
	World world; auto target = Target(); auto info = Info(); info.z0 = info.z1 = 0; CHECK(Generate(world, info, target));
	auto& effect = *effects.front(); CHECK(effect.Update()); CHECK_EQ(240, effect.GetPixelX()); CHECK_EQ(110, effect.GetPixelY()); CHECK_EQ(0, effect.GetPixelZ());
	CHECK_EQ(0, effect.GetDirection()); CHECK_EQ(1, effect.GetFrame()); CHECK_EQ(8, effect.GetLight());
	CHECK(!effect.Update()); CHECK_EQ(240, effect.GetPixelX()); CHECK_EQ(96, effect.GetPixelY()); CHECK_EQ(0, effect.GetEndFrame());
	CHECK_EQ(1, effect.GetFrame()); CHECK_EQ(8, effect.GetLight()); CHECK_EQ(effect.GetEffectTarget()->GetY(), effect.GetPixelY());
}

TEST(FollowPathEffectGenerator, ZeroStepKeepsTheEffectAtItsOriginUntilItsDeadline)
{
	World world; auto target = Target(); auto info = Info(); info.step = 0; CHECK(Generate(world, info, target));
	auto& effect = *effects.front(); CHECK(effect.Update()); CHECK_EQ(240, effect.GetPixelX()); CHECK_EQ(120, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ());
	frameNow = 129; CHECK(!effect.Update()); CHECK_EQ(1, effect.GetFrame()); CHECK_EQ(120, effect.GetPixelY());
}

TEST(FollowPathEffectGenerator, DirectionPastThePathArrayRejectsWithoutTakingTheTarget)
{
	World world; auto target = Target(); auto info = Info(); info.direction = 8;
	CHECK(!Generate(world, info, target)); CHECK_EQ(0, submissions); CHECK(effects.empty());
	if (target) CheckUnchanged(*target);
	CHECK(calls == std::vector<int>{1}); CheckPaths();
}

TEST(FollowPathEffectGenerator, EveryInvalidDirectionRejectsForAllTargetStates)
{
	World world;
	for (int direction = 8; direction < 256; ++direction)
	{
		for (const int phase : {-1, 0, 2, 255})
		{
			ClearEffects(); auto target = phase < 0 ? std::unique_ptr<MEffectTarget>{} : Target(static_cast<BYTE>(phase));
			auto info = Info(); info.direction = static_cast<BYTE>(direction); CHECK(!Generate(world, info, target));
			CHECK_EQ(0, submissions); CHECK(effects.empty()); CHECK(calls == std::vector<int>{1});
			if (target) { CheckUnchanged(*target); CHECK_EQ(phase, target->GetCurrentPhase()); }
		}
	}
}

TEST(FollowPathEffectGenerator, InvalidDirectionKeepsMetadataAndCacheOrdering)
{
	World world; auto info = Info(); info.direction = 255; auto target = Target();
	spriteAvailable = false; CHECK(!Generate(world, info, target)); for (const auto& path : FollowPath) CHECK(path.empty());
	spriteAvailable = true; CHECK(!Generate(world, info, target)); CheckPaths();
	for (auto& path : FollowPath) path.assign(1, POINT{91, 92});
	info.nActionInfo = 42; CHECK(!Generate(world, info, target));
	for (const auto& path : FollowPath) { CHECK_EQ(1, path.size()); CHECK_EQ(91, path.front().x); CHECK_EQ(92, path.front().y); }
	CHECK_EQ(0, submissions); CheckUnchanged(*target);
}
