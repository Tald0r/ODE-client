#include "test_framework.h"
#include "MRippleZoneWideEffectGenerator.h"
#include "MFollowPathEffectGenerator.h"
#include "MEffect.h"

#include <array>
#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MFixedZoneEffectSprite sprite;
MWideRippleEffectBounds bounds;
bool spriteAvailable, boundsAvailable;
int requestedSprite, boundsCalls, submissions, acceptance;
std::vector<int> calls, slots, removedTargets;
std::vector<std::unique_ptr<MEffect>> effects;
struct Point { int x, y; bool operator==(const Point&) const = default; };
constexpr Point rows[8][5] = {
	{{4, 3}, {4, 4}, {4, 5}, {4, 6}, {4, 7}},
	{{2, 4}, {3, 5}, {4, 6}, {5, 7}, {6, 8}},
	{{3, 6}, {4, 6}, {5, 6}, {6, 6}, {7, 6}},
	{{4, 8}, {5, 7}, {6, 6}, {7, 5}, {8, 4}},
	{{6, 3}, {6, 4}, {6, 5}, {6, 6}, {6, 7}},
	{{4, 2}, {5, 3}, {6, 4}, {7, 5}, {8, 6}},
	{{3, 4}, {4, 4}, {5, 4}, {6, 4}, {7, 4}},
	{{2, 6}, {3, 5}, {4, 4}, {5, 3}, {6, 2}},
};
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(4); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE frame) { calls.push_back(3); return 7 + frame; },
};
const MEffectTargetHost targetHost{.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); }};
const MWideRippleEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& result) {
		calls.push_back(1); requestedSprite = type; result = sprite; return spriteAvailable;
	},
	.Bounds = [](MWideRippleEffectBounds& result) { calls.push_back(2); ++boundsCalls; result = bounds; return boundsAvailable; },
	.Queue = [](std::unique_ptr<MEffect> effect) {
		calls.push_back(5); ++submissions; CHECK_EQ(MEffect::EFFECT_SECTOR, effect->GetEffectType());
		CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo()); const int slot = boundsCalls - 1;
		if (acceptance >= 0 && (slot >= 31 || (acceptance & (1 << slot)) == 0)) return false;
		slots.push_back(slot); effects.push_back(std::move(effect)); return true;
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MWideRippleEffectHost* previousGenerator = MRippleZoneWideEffectGenerator::SetHost(&host);
	MRippleZoneWideEffectGenerator generator;
	World()
	{
		frameNow = 100; sprite = {BLT_EFFECT, 12, 3}; bounds = {20, 20}; spriteAvailable = boundsAvailable = true;
		requestedSprite = -1; boundsCalls = submissions = 0; acceptance = -1; effects.clear(); calls.clear(); slots.clear(); removedTargets.clear();
	}
	~World()
	{
		effects.clear(); MRippleZoneWideEffectGenerator::SetHost(previousGenerator);
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

std::unique_ptr<MEffectTarget> Target(BYTE id = 73)
{
	auto target = std::make_unique<MEffectTarget>(3); target->NextPhase(); target->m_EffectID = id;
	target->Set(777, 888, 999, 456); target->SetServerID(789); target->SetDelayFrame(31);
	target->SetResultTime(); target->SetResult(new MActionResult); return target;
}

void CheckTarget(const MEffectTarget& target)
{
	CHECK_EQ(777, target.GetX()); CHECK_EQ(888, target.GetY()); CHECK_EQ(999, target.GetZ()); CHECK_EQ(456, target.GetID());
	CHECK_EQ(789, target.GetServerID()); CHECK_EQ(1, target.GetCurrentPhase()); CHECK_EQ(3, target.GetMaxPhase()); CHECK_EQ(31, target.GetDelayFrame());
	CHECK(target.IsExistResult()); CHECK(target.IsResultTime());
}

void ClearEffects()
{
	effects.clear(); calls.clear(); slots.clear(); boundsCalls = submissions = 0;
}

std::vector<Point> Positions()
{
	std::vector<Point> result;
	for (const auto& effect : effects) result.push_back({effect->GetX(), effect->GetY()});
	return result;
}

} // namespace

TEST(WideRippleEffectGenerator, ConfiguresTheWholeRowFromOneSpriteLookupWithoutRandomness)
{
	World world; CHECK_EQ(EFFECTGENERATORID_RIPPLE_ZONE_WIDE, world.generator.GetID());
	std::srand(61); const int next = std::rand(); std::srand(61); CHECK(world.generator.Generate(Info())); CHECK_EQ(next, std::rand());
	CHECK_EQ(17, requestedSprite); CHECK_EQ(5, submissions); CHECK_EQ(5, effects.size());
	std::vector<int> expected{1}; for (int i = 0; i < 5; ++i) expected.insert(expected.end(), {2, 3, 4, 5}); CHECK(calls == expected);
	for (size_t i = 0; i < effects.size(); ++i)
	{
		auto& effect = *effects[i]; CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame());
		CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight()); CHECK_EQ(192, effect.GetPixelX()); CHECK_EQ((3 + i) * 24, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ());
		CHECK_EQ(0, effect.GetDirection()); CHECK_EQ(10, effect.GetStepPixel()); CHECK_EQ(4, effect.GetPower()); CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame());
		CHECK_EQ(42, effect.GetActionInfo()); CHECK(effect.GetEffectTarget() == nullptr); CHECK(!effect.IsMulti()); CHECK(!effect.IsDelayFrame());
	}
}

TEST(WideRippleEffectGenerator, AllEightDirectionsKeepTheirNumberedRowOrder)
{
	World world;
	for (int direction = 0; direction < 8; ++direction)
	{
		ClearEffects(); auto info = Info(); info.direction = static_cast<BYTE>(direction); CHECK(world.generator.Generate(info)); CHECK_EQ(5, effects.size());
		for (size_t i = 0; i < effects.size(); ++i) { CHECK_EQ(rows[direction][i].x, effects[i]->GetX()); CHECK_EQ(rows[direction][i].y, effects[i]->GetY()); CHECK_EQ(direction, effects[i]->GetDirection()); }
	}
}

TEST(WideRippleEffectGenerator, EveryPowerByteControlsRowLengthCenterAndWrappedNextPower)
{
	World world; bounds = {65535, 65535};
	for (int power = 0; power < 256; ++power)
	{
		ClearEffects(); auto target = Target(); auto info = Info(); info.x0 = 24000; info.y0 = 12000; info.power = static_cast<BYTE>(power); info.pEffectTarget = target.get();
		CHECK_EQ(power != 0, world.generator.Generate(info)); CHECK_EQ(power == 0 ? 0 : 2 * power - 1, effects.size());
		if (power) target.release();
		for (size_t i = 0; i < effects.size(); ++i)
		{
			CHECK_EQ(499, effects[i]->GetX()); CHECK_EQ(501 - power + i, effects[i]->GetY()); CHECK_EQ(static_cast<BYTE>(power + 1), effects[i]->GetPower());
			CHECK_EQ(i == static_cast<size_t>(power - 1), effects[i]->GetEffectTarget() == info.pEffectTarget);
			if (i != static_cast<size_t>(power - 1)) CHECK(effects[i]->GetEffectTarget() == nullptr);
		}
		if (power == 0) { CHECK(calls == std::vector<int>{1}); CheckTarget(*target); }
	}
}

TEST(WideRippleEffectGenerator, AllAcceptanceMasksReserveTheOriginalForTheCenter)
{
	World world;
	for (int mask = 0; mask < 32; ++mask)
	{
		ClearEffects(); acceptance = mask; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK_EQ((mask & 4) != 0, world.generator.Generate(info)); CHECK_EQ(5, submissions);
		if (mask & 4) target.release();
		for (size_t i = 0; i < effects.size(); ++i)
		{
			CHECK_EQ(slots[i] == 2, effects[i]->GetEffectTarget() == info.pEffectTarget);
			if (slots[i] != 2) CHECK(effects[i]->GetEffectTarget() == nullptr);
			CHECK_EQ(rows[0][slots[i]].x, effects[i]->GetX()); CHECK_EQ(rows[0][slots[i]].y, effects[i]->GetY());
		}
		CheckTarget(*info.pEffectTarget);
	}
}

TEST(WideRippleEffectGenerator, TargetlessAcceptanceStillReportsOnlyTheCenter)
{
	World world;
	for (int mask = 0; mask < 32; ++mask)
	{
		ClearEffects(); acceptance = mask; CHECK_EQ((mask & 4) != 0, world.generator.Generate(Info())); CHECK_EQ(5, submissions);
		for (const auto& effect : effects) { CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(42, effect->GetActionInfo()); }
	}
}

TEST(WideRippleEffectGenerator, AcceptedCenterKeepsAllOriginalTargetMetadataAndCoordinates)
{
	World world; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); auto* result = target->GetResult();
	CHECK(world.generator.Generate(info)); target.release(); CHECK(effects[2]->GetEffectTarget() == info.pEffectTarget); CheckTarget(*info.pEffectTarget);
	CHECK(info.pEffectTarget->GetResult() == result); CHECK(removedTargets.empty()); ClearEffects(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(WideRippleEffectGenerator, RejectedCenterLeavesIndependentTargetlessSides)
{
	World world; acceptance = 27; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK_EQ(4, effects.size()); target.reset(); CHECK(removedTargets == std::vector<int>{73});
	for (const auto& effect : effects) CHECK(effect->GetEffectTarget() == nullptr);
	ClearEffects(); CHECK(removedTargets == std::vector<int>{73});
}

TEST(WideRippleEffectGenerator, SourceConversionTruncatesBeforeUnsignedTileStepping)
{
	World world; struct Source { int x, y, expectedX, expectedY; };
	for (const Source source : {Source{261, 130, 6, 5}, {-1, -1, 1, 0}, {-48, 0, 0, 0}, {3145728, 1572864, 1, 0}})
	{
		ClearEffects(); auto info = Info(); info.x0 = source.x; info.y0 = source.y; info.direction = DIRECTION_RIGHT; info.power = 1;
		CHECK(world.generator.Generate(info)); CHECK_EQ(source.expectedX, effects.front()->GetX()); CHECK_EQ(source.expectedY, effects.front()->GetY());
	}
}

TEST(WideRippleEffectGenerator, ExtremeSourcesRetainSectorNarrowingAndHeightStorage)
{
	World world; bounds = {65535, 65535}; const int low = (std::numeric_limits<int>::min)(), high = (std::numeric_limits<int>::max)();
	for (const int source : {low, high})
	{
		ClearEffects(); auto info = Info(); info.x0 = info.y0 = info.z0 = source; info.direction = DIRECTION_RIGHT; info.power = 1;
		CHECK(world.generator.Generate(info)); CHECK_EQ(source == low ? 21847 : 43691, effects.front()->GetX());
		CHECK_EQ(source == low ? 43691 : 21845, effects.front()->GetY()); CHECK_EQ(source, effects.front()->GetPixelZ());
	}
}

TEST(WideRippleEffectGenerator, OutgoingClippingKeepsOnlyTheInBoundsPrefix)
{
	World world; bounds.height = 5; CHECK(!world.generator.Generate(Info()));
	CHECK(Positions() == std::vector<Point>({{4, 3}, {4, 4}})); CHECK_EQ(5, boundsCalls); CHECK_EQ(2, submissions);
	ClearEffects(); bounds.height = 6; CHECK(world.generator.Generate(Info())); CHECK_EQ(3, submissions); CHECK_EQ(5, boundsCalls);
}

TEST(WideRippleEffectGenerator, BoundsAreRefreshedAfterEachSubmission)
{
	World world; const MWideRippleEffectHost shrinking{
		.Sprite = host.Sprite, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect) { bounds.width = 0; return host.Queue(std::move(effect)); },
	};
	MRippleZoneWideEffectGenerator::SetHost(&shrinking); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK_EQ(1, submissions); CHECK_EQ(5, boundsCalls); CheckTarget(*target);
}

TEST(WideRippleEffectGenerator, MissingMetadataRejectsBeforeBoundsOrConstruction)
{
	World world; const MWideRippleEffectHost empty{};
	for (const auto* service : {static_cast<const MWideRippleEffectHost*>(nullptr), &empty, &host})
	{
		ClearEffects(); MRippleZoneWideEffectGenerator::SetHost(service); spriteAvailable = false; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK(!world.generator.Generate(info)); CHECK_EQ(0, boundsCalls); CHECK_EQ(0, submissions); CheckTarget(*target);
		CHECK(calls == (service == &host ? std::vector<int>{1} : std::vector<int>{}));
	}
}

TEST(WideRippleEffectGenerator, MissingBoundsRejectBeforeConstruction)
{
	World world; const MWideRippleEffectHost noBounds{.Sprite = host.Sprite, .Queue = host.Queue};
	for (const auto* service : {&noBounds, &host})
	{
		ClearEffects(); MRippleZoneWideEffectGenerator::SetHost(service); boundsAvailable = false; auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK(!world.generator.Generate(info)); CHECK_EQ(0, submissions); CHECK(effects.empty()); CheckTarget(*target);
		CHECK(calls == (service == &host ? std::vector<int>{1, 2} : std::vector<int>{1}));
	}
}

TEST(WideRippleEffectGenerator, ZeroPowerNeedsNoBoundsOrSubmissionService)
{
	World world; const MWideRippleEffectHost onlySprite{.Sprite = host.Sprite}; MRippleZoneWideEffectGenerator::SetHost(&onlySprite);
	auto info = Info(); info.power = 0; CHECK(!world.generator.Generate(info)); CHECK(calls == std::vector<int>{1});
}

TEST(WideRippleEffectGenerator, MissingQueueDiscardsEveryConfiguredEffectWithoutTakingTheTarget)
{
	World world; const MWideRippleEffectHost noQueue{.Sprite = host.Sprite, .Bounds = host.Bounds}; MRippleZoneWideEffectGenerator::SetHost(&noQueue);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK(removedTargets.empty()); CheckTarget(*target);
	std::vector<int> expected{1}; for (int i = 0; i < 5; ++i) expected.insert(expected.end(), {2, 3, 4}); CHECK(calls == expected);
}

TEST(WideRippleEffectGenerator, SpriteCallbackCanInstallBoundsAndSubmission)
{
	World world; const MWideRippleEffectHost replacing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& result) { MRippleZoneWideEffectGenerator::SetHost(&host); return host.Sprite(type, result); },
	};
	MRippleZoneWideEffectGenerator::SetHost(&replacing); CHECK(world.generator.Generate(Info())); CHECK_EQ(5, effects.size());
}

TEST(WideRippleEffectGenerator, BoundsCallbackCanInstallSubmission)
{
	World world; const MWideRippleEffectHost replacing{
		.Sprite = host.Sprite,
		.Bounds = [](MWideRippleEffectBounds& result) { MRippleZoneWideEffectGenerator::SetHost(&host); return host.Bounds(result); },
	};
	MRippleZoneWideEffectGenerator::SetHost(&replacing); CHECK(world.generator.Generate(Info())); CHECK_EQ(5, effects.size());
}

TEST(WideRippleEffectGenerator, HostRemovalPreservesOnlyAnAlreadyAcceptedCenter)
{
	World world; static int removeAt;
	const MWideRippleEffectHost removing{
		.Sprite = host.Sprite, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect) { if (boundsCalls - 1 == removeAt) MRippleZoneWideEffectGenerator::SetHost(nullptr); return host.Queue(std::move(effect)); },
	};
	for (int stop = 0; stop < 5; ++stop)
	{
		ClearEffects(); removeAt = stop; MRippleZoneWideEffectGenerator::SetHost(&removing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK_EQ(stop >= 2, world.generator.Generate(info)); CHECK_EQ(stop + 1, effects.size());
		if (stop >= 2) target.release();
		CheckTarget(*info.pEffectTarget);
	}
}

TEST(WideRippleEffectGenerator, MetadataIsRetainedForTheCurrentRowAndRefreshedForTheNext)
{
	World world; const MWideRippleEffectHost changing{
		.Sprite = host.Sprite, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect) { sprite = {BLT_SHADOW, 21, 258}; return host.Queue(std::move(effect)); },
	};
	MRippleZoneWideEffectGenerator::SetHost(&changing); CHECK(world.generator.Generate(Info()));
	for (const auto& effect : effects) { CHECK_EQ(BLT_EFFECT, effect->GetBltType()); CHECK_EQ(12, effect->GetFrameID()); CHECK_EQ(3, effect->GetMaxFrame()); }
	ClearEffects(); CHECK(world.generator.Generate(Info()));
	for (const auto& effect : effects) { CHECK_EQ(BLT_SHADOW, effect->GetBltType()); CHECK_EQ(21, effect->GetFrameID()); CHECK_EQ(2, effect->GetMaxFrame()); }
}

TEST(WideRippleEffectGenerator, InstallerIsIndependentOfFixedPatternGenerators)
{
	World world; const MFixedZoneEffectHost sentinel{}; const auto* previous = MFollowPathEffectGenerator::SetHost(&sentinel);
	CHECK(MRippleZoneWideEffectGenerator::SetHost(nullptr) == &host); CHECK(MFollowPathEffectGenerator::SetHost(previous) == &sentinel);
	CHECK(!world.generator.Generate(Info())); CHECK(calls.empty());
}

TEST(WideRippleEffectGenerator, RejectingQueueConsumesItsMarkersWithoutTakingTheOriginal)
{
	World world; const MWideRippleEffectHost rejecting{
		.Sprite = host.Sprite, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect) { effect->SetLink(42, Target(94).release()); return false; },
	};
	MRippleZoneWideEffectGenerator::SetHost(&rejecting); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(removedTargets == std::vector<int>(5, 94)); CheckTarget(*target);
}

TEST(WideRippleEffectGenerator, EarlyQueueExceptionLeavesTheOriginalWithItsCaller)
{
	World world; const MWideRippleEffectHost throwing{
		.Sprite = host.Sprite, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect) -> bool { effect->SetLink(42, Target(94).release()); throw std::runtime_error("queue"); },
	};
	MRippleZoneWideEffectGenerator::SetHost(&throwing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK(removedTargets == std::vector<int>{94}); CheckTarget(*target);
}

TEST(WideRippleEffectGenerator, LateQueueExceptionKeepsTheAcceptedCenterOwningTheOriginal)
{
	World world; const MWideRippleEffectHost throwing{
		.Sprite = host.Sprite, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect) {
			if (boundsCalls == 4) { effect->SetLink(42, Target(94).release()); throw std::runtime_error("queue"); }
			return host.Queue(std::move(effect));
		},
	};
	MRippleZoneWideEffectGenerator::SetHost(&throwing); auto target = Target(); auto info = Info(); info.pEffectTarget = target.get(); bool threw = false;
	try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(3, effects.size()); CHECK(effects[2]->GetEffectTarget() == target.get());
	if (effects[2]->GetEffectTarget() == target.get()) target.release();
	CheckTarget(*info.pEffectTarget); CHECK(removedTargets == std::vector<int>{94}); ClearEffects(); CHECK(removedTargets == std::vector<int>({94, 73}));
}

TEST(WideRippleEffectGenerator, AnimationCountsNarrowWithoutChangingRowLengthOrDuration)
{
	World world;
	for (const int frames : {-1, 0, 256, 258})
	{
		ClearEffects(); sprite.maxFrames = frames; CHECK(world.generator.Generate(Info())); CHECK_EQ(5, effects.size());
		for (const auto& effect : effects) { CHECK_EQ(static_cast<BYTE>(frames), effect->GetMaxFrame()); CHECK_EQ(129, effect->GetEndFrame()); }
	}
}

TEST(WideRippleEffectGenerator, RealUpdatesAnimateWithoutMovingAndStopAtTheDeadline)
{
	World world; CHECK(world.generator.Generate(Info())); auto& effect = *effects.front();
	CHECK(effect.Update()); CHECK_EQ(192, effect.GetPixelX()); CHECK_EQ(72, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ()); CHECK_EQ(1, effect.GetFrame()); CHECK_EQ(8, effect.GetLight());
	frameNow = 129; CHECK(!effect.Update()); CHECK_EQ(2, effect.GetFrame()); CHECK_EQ(9, effect.GetLight()); CHECK_EQ(192, effect.GetPixelX());
}

TEST(WideRippleEffectGenerator, DeadlinesRetainFiniteSentinelsClockWrapAndMissingClockFallback)
{
	World world; auto info = Info(); info.count = 65535; info.linkCount = MAX_LINKCOUNT; CHECK(world.generator.Generate(info));
	for (const auto& effect : effects) { CHECK_EQ(65634, effect->GetEndFrame()); CHECK_EQ(65634, effect->GetEndLinkFrame()); }
	ClearEffects(); frameNow = 0xFFFFFFFEu; CHECK(world.generator.Generate(Info()));
	for (const auto& effect : effects) { CHECK_EQ(27, effect->GetEndFrame()); CHECK_EQ(2, effect->GetEndLinkFrame()); }
	ClearEffects(); MEffect::SetHost(nullptr); CHECK(world.generator.Generate(Info()));
	for (const auto& effect : effects) { CHECK_EQ(29, effect->GetEndFrame()); CHECK_EQ(4, effect->GetEndLinkFrame()); CHECK_EQ(0, effect->GetLight()); CHECK(!effect->Update()); }
}

TEST(WideRippleEffectGenerator, DirectionOutsideTheEightRowsRejectsBeforeBoundsOrConstruction)
{
	World world; auto target = Target(); auto info = Info(); info.direction = 8; info.pEffectTarget = target.get();
	const bool accepted = world.generator.Generate(info); CHECK(!accepted); if (accepted) target.release();
	CHECK_EQ(0, boundsCalls); CHECK_EQ(0, submissions); CHECK(effects.empty()); CHECK(calls == std::vector<int>{1});
	CheckTarget(*info.pEffectTarget);
}

TEST(WideRippleEffectGenerator, EveryInvalidDirectionRejectsForEmptyAndFullRowsWithEitherTargetState)
{
	World world;
	for (int direction = 8; direction < 256; ++direction) for (const BYTE power : {static_cast<BYTE>(0), static_cast<BYTE>(1), static_cast<BYTE>(255)})
	{
		for (const bool linked : {false, true})
		{
			ClearEffects(); auto target = linked ? Target() : std::unique_ptr<MEffectTarget>{}; auto info = Info();
			info.direction = static_cast<BYTE>(direction); info.power = power; info.pEffectTarget = target.get();
			CHECK(!world.generator.Generate(info)); CHECK_EQ(0, boundsCalls); CHECK_EQ(0, submissions); CHECK(effects.empty()); CHECK(calls == std::vector<int>{1});
			if (target) CheckTarget(*target);
		}
	}
}

TEST(WideRippleEffectGenerator, InvalidDirectionStillLooksUpSpriteMetadataFirst)
{
	World world; auto info = Info(); info.direction = 255; spriteAvailable = false;
	CHECK(!world.generator.Generate(info)); CHECK_EQ(17, requestedSprite); CHECK(calls == std::vector<int>{1}); CHECK_EQ(0, boundsCalls);
	ClearEffects(); spriteAvailable = true; CHECK(!world.generator.Generate(info)); CHECK(calls == std::vector<int>{1}); CHECK_EQ(0, boundsCalls);
}
