#include "test_framework.h"
#include "MStopZoneRectEffectGenerator.h"
#include "MEffect.h"
#include "EffectSpriteTypeDef.h"
#include "SkillDef.h"

#include <cstdlib>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

DWORD frameNow;
MRectZoneEffectSprite sprite;
MRectZoneEffectBounds zoneBounds;
int maxFrames, requestedSprite, requestedBlt, requestedFrame, submissions, rejectBefore;
bool spriteAvailable, framesAvailable, boundsAvailable, acceptQueue, validLargeRectangle;
std::vector<bool> acceptance;
std::vector<int> calls, slots, removedTargets, submittedFrames;
std::vector<DWORD> delays;
std::vector<std::unique_ptr<MEffect>> effects;
struct Point
{
	int x, y;
	bool operator==(const Point&) const = default;
};
std::vector<Point> attempted;
const std::vector<Point> defaultPoints{{5, 5}, {4, 4}, {5, 4}, {6, 4}, {4, 5}, {6, 5}, {4, 6}, {5, 6}, {6, 6}};
const Point linkedPoints[] = {{777, 888}, {852, 776}, {900, 776}, {948, 776}, {852, 800}, {948, 800}, {852, 824}, {900, 824}, {948, 824}};
const MEffectHost effectHost{
	.CurrentFrame = []() { calls.push_back(4); return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE frame) { calls.push_back(3); return 7 + frame; },
};
const MEffectTargetHost targetHost{.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); }};
const MRectZoneEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MRectZoneEffectSprite& result) {
		calls.push_back(1); requestedSprite = type; result = sprite; return spriteAvailable;
	},
	.MaxFrames = [](BYTE blt, TYPE_FRAMEID frame, int& count) {
		calls.push_back(2); requestedBlt = blt; requestedFrame = frame; count = maxFrames; return framesAvailable;
	},
	.Bounds = [](MRectZoneEffectBounds& result) { calls.push_back(6); result = zoneBounds; return boundsAvailable; },
	.Queue = [](std::unique_ptr<MEffect> effect, DWORD delay) {
		calls.push_back(5); attempted.push_back({effect->GetX(), effect->GetY()});
		submittedFrames.push_back(effect->GetFrameID()); delays.push_back(delay);
		CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(ACTIONINFO_NULL, effect->GetActionInfo());
		const int slot = submissions++;
		if (!acceptQueue || slot < rejectBefore || (!acceptance.empty() &&
			(static_cast<size_t>(slot) >= acceptance.size() || !acceptance[slot]))) return false;
		slots.push_back(slot); effects.push_back(std::move(effect)); return true;
	},
};

struct World
{
	const MEffectHost* previousEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* previousTarget = MEffectTarget::SetHost(&targetHost);
	const MRectZoneEffectHost* previousRectangle = MStopZoneRectEffectGenerator::SetHost(&host);
	MStopZoneRectEffectGenerator generator;
	World()
	{
		frameNow = 100; sprite = {BLT_EFFECT, 12}; maxFrames = 3; zoneBounds = {11, 11};
		spriteAvailable = framesAvailable = boundsAvailable = acceptQueue = validLargeRectangle = true;
		requestedSprite = requestedBlt = requestedFrame = -1; submissions = rejectBefore = 0;
		effects.clear(); calls.clear(); slots.clear(); removedTargets.clear(); acceptance.clear();
		attempted.clear(); submittedFrames.clear(); delays.clear();
	}
	~World()
	{
		effects.clear(); MStopZoneRectEffectGenerator::SetHost(previousRectangle);
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
	info.power = 1; info.creatureID = 123;
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
	effects.clear(); calls.clear(); slots.clear(); attempted.clear(); submittedFrames.clear(); delays.clear(); submissions = 0;
}

} // namespace

TEST(RectangleEffectGenerator, SubmitsTheCenterBeforeBoundsAndThenAllSurroundingTilesInRowOrder)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_STOP_ZONE_RECT, world.generator.GetID());
	CHECK(world.generator.Generate(Info())); CHECK_EQ(9, effects.size()); CHECK(attempted == defaultPoints);
	CHECK_EQ(17, requestedSprite); CHECK_EQ(BLT_EFFECT, requestedBlt); CHECK_EQ(12, requestedFrame);
	std::vector<int> expected{1, 2, 3, 4, 5, 6}; for (int i = 0; i < 8; ++i) expected.insert(expected.end(), {3, 4, 5});
	CHECK(calls == expected); CHECK(delays == std::vector<DWORD>(9, 0));
	for (size_t i = 0; i < effects.size(); ++i)
	{
		auto& effect = *effects[i]; CHECK_EQ(MEffect::EFFECT_SECTOR, effect.GetEffectType());
		CHECK_EQ(BLT_EFFECT, effect.GetBltType()); CHECK_EQ(12, effect.GetFrameID()); CHECK_EQ(3, effect.GetMaxFrame());
		CHECK_EQ(0, effect.GetFrame()); CHECK_EQ(7, effect.GetLight()); CHECK_EQ(defaultPoints[i].x * 48, effect.GetPixelX());
		CHECK_EQ(defaultPoints[i].y * 24, effect.GetPixelY()); CHECK_EQ(17, effect.GetPixelZ());
		CHECK_EQ(9, effect.GetStepPixel()); CHECK_EQ(DIRECTION_RIGHTUP, effect.GetDirection()); CHECK_EQ(1, effect.GetPower());
		CHECK_EQ(129, effect.GetEndFrame()); CHECK_EQ(104, effect.GetEndLinkFrame()); CHECK_EQ(42, effect.GetActionInfo());
		CHECK(effect.GetEffectTarget() == nullptr); CHECK(!effect.IsMulti()); CHECK(!effect.IsWaitFrame());
	}
}

TEST(RectangleEffectGenerator, RadiusTwoFillsTheSquareAndDoesNotDuplicateTheCenter)
{
	World world;
	auto info = Info(); info.power = 2; CHECK(world.generator.Generate(info)); CHECK_EQ(25, effects.size());
	CHECK(attempted == std::vector<Point>({{5, 5}, {3, 3}, {4, 3}, {5, 3}, {6, 3}, {7, 3},
		{3, 4}, {4, 4}, {5, 4}, {6, 4}, {7, 4}, {3, 5}, {4, 5}, {6, 5}, {7, 5},
		{3, 6}, {4, 6}, {5, 6}, {6, 6}, {7, 6}, {3, 7}, {4, 7}, {5, 7}, {6, 7}, {7, 7}}));
}

TEST(RectangleEffectGenerator, AllFourCornersClipOnlyTheSurroundingTiles)
{
	World world;
	struct Case { int x, y; std::vector<Point> expected; };
	for (const Case& c : {Case{0, 0, {{0, 0}, {1, 0}, {0, 1}, {1, 1}}}, {10, 0, {{10, 0}, {9, 0}, {9, 1}, {10, 1}}},
		{0, 10, {{0, 10}, {0, 9}, {1, 9}, {1, 10}}}, {10, 10, {{10, 10}, {9, 9}, {10, 9}, {9, 10}}}})
	{
		ClearEffects(); auto info = Info(); info.x0 = c.x * 48; info.y0 = c.y * 24;
		CHECK(world.generator.Generate(info)); CHECK(attempted == c.expected);
	}
}

TEST(RectangleEffectGenerator, ZeroPowerAndEmptyBoundsStillSubmitTheCenter)
{
	World world;
	auto info = Info(); info.power = 0; CHECK(world.generator.Generate(info)); CHECK_EQ(1, submissions);
	for (const MRectZoneEffectBounds bounds : {MRectZoneEffectBounds{0, 0}, {0, 11}, {11, 0}, {1, 1}})
	{
		ClearEffects(); zoneBounds = bounds; info = Info();
		CHECK(world.generator.Generate(info)); CHECK_EQ(1, effects.size()); CHECK((attempted.front() == Point{5, 5}));
		CHECK(calls == std::vector<int>({1, 2, 3, 4, 5, 6}));
	}
}

TEST(RectangleEffectGenerator, SourceTruncationAndUnsignedNarrowingPrecedeCenterSubmission)
{
	World world;
	auto info = Info(); info.x0 = info.y0 = -1;
	CHECK(world.generator.Generate(info)); CHECK(attempted == std::vector<Point>({{0, 0}, {1, 0}, {0, 1}, {1, 1}}));
	ClearEffects(); zoneBounds = {65535, 65535}; info.x0 = -48; info.y0 = -24;
	CHECK(world.generator.Generate(info)); CHECK(attempted == std::vector<Point>({{65535, 65535}, {65534, 65534}}));
	ClearEffects(); zoneBounds = {11, 11}; info.x0 = 3145728 + 261; info.y0 = 1572864 + 130;
	CHECK(world.generator.Generate(info)); CHECK(attempted == defaultPoints);
}

TEST(RectangleEffectGenerator, MaximumRadiusVisitsEveryTileExactlyOnceWithTheCenterFirst)
{
	World world;
	MEffect::SetHost(nullptr); zoneBounds = {600, 600};
	const MRectZoneEffectHost streaming{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect, DWORD delay) {
			const Point point{effect->GetX(), effect->GetY()};
			if (submissions == 0) validLargeRectangle &= point == Point{300, 300};
			else
			{
				validLargeRectangle &= point.x >= 45 && point.x <= 555 && point.y >= 45 && point.y <= 555 && point != Point{300, 300};
				if (!attempted.empty())
				{
					const Point previous = attempted.back();
					validLargeRectangle &= point.y > previous.y || (point.y == previous.y && point.x > previous.x);
				}
				if (attempted.size() < 2) attempted.push_back(point); else attempted.back() = point;
			}
			validLargeRectangle &= delay == 0; ++submissions; return false;
		},
	};
	MStopZoneRectEffectGenerator::SetHost(&streaming);
	auto target = Target(); auto info = Info(); info.x0 = 300 * 48; info.y0 = 300 * 24; info.power = 255; info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK_EQ(261121, submissions); CHECK(validLargeRectangle);
	CHECK((attempted.front() == Point{45, 45})); CHECK((attempted.back() == Point{555, 555}));
	CHECK(effects.empty()); CHECK_EQ(777, target->GetX()); CHECK(removedTargets.empty()); CHECK(calls == std::vector<int>({1, 2, 6}));
}

TEST(RectangleEffectGenerator, CopiesPreservePhaseAndMetadataButUseDestinationPixelOffsets)
{
	World world;
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release();
	for (size_t i = 0; i < effects.size(); ++i)
	{
		const auto* linked = effects[i]->GetEffectTarget(); CHECK(linked != nullptr); if (!linked) continue;
		CHECK_EQ(i == 0, linked == info.pEffectTarget); CHECK_EQ(1, linked->GetCurrentPhase()); CHECK_EQ(3, linked->GetMaxPhase());
		CHECK_EQ(31, linked->GetDelayFrame()); CHECK_EQ(73, linked->GetEffectID());
		CHECK_EQ(i == 0, linked->IsExistResult()); CHECK_EQ(i == 0, linked->IsResultTime());
		CHECK_EQ(i == 0 ? 789 : OBJECTID_NULL, linked->GetServerID());
		CHECK_EQ(linkedPoints[i].x, linked->GetX()); CHECK_EQ(linkedPoints[i].y, linked->GetY());
		CHECK_EQ(i == 0 ? 999 : 17, linked->GetZ()); CHECK_EQ(i == 0 ? 456 : 123, linked->GetID());
		for (size_t j = 0; j < i; ++j) CHECK(linked != effects[j]->GetEffectTarget());
	}
}

TEST(RectangleEffectGenerator, ClippingMovesTheCopyAnchorToTheNewLowerBounds)
{
	World world;
	auto target = Target(); auto info = Info(); info.x0 = info.y0 = 0; info.power = 2; info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); target.release(); CHECK_EQ(9, effects.size());
	const Point expected[] = {{777, 888}, {900, 776}, {948, 776}, {852, 800}, {900, 800}, {948, 800}, {852, 824}, {900, 824}, {948, 824}};
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(expected[i].x, effects[i]->GetEffectTarget()->GetX()); CHECK_EQ(expected[i].y, effects[i]->GetEffectTarget()->GetY());
	}
}

TEST(RectangleEffectGenerator, AllAcceptanceMasksGiveTheOriginalToTheFirstAcceptedEffect)
{
	World world;
	for (int mask = 0; mask < 512; ++mask)
	{
		ClearEffects(); acceptance.assign(9, false);
		for (int slot = 0; slot < 9; ++slot) acceptance[slot] = (mask & (1 << slot)) != 0;
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		CHECK_EQ(mask != 0, world.generator.Generate(info)); CHECK_EQ(9, submissions);
		bool originalOwned = false;
		for (size_t i = 0; i < effects.size(); ++i)
		{
			const auto* linked = effects[i]->GetEffectTarget(); CHECK(linked != nullptr); if (!linked) continue;
			originalOwned |= linked == target.get(); CHECK_EQ(i == 0, linked == target.get());
			CHECK_EQ(i == 0 ? 777 : linkedPoints[slots[i]].x, linked->GetX());
			CHECK_EQ(i == 0 ? 888 : linkedPoints[slots[i]].y, linked->GetY());
		}
		CHECK_EQ(mask != 0, originalOwned); if (originalOwned) target.release();
	}
}

TEST(RectangleEffectGenerator, TargetlessMasksReportAnyAcceptanceAndLinkEveryAction)
{
	World world;
	for (int mask = 0; mask < 512; ++mask)
	{
		ClearEffects(); acceptance.assign(9, false);
		for (int slot = 0; slot < 9; ++slot) acceptance[slot] = (mask & (1 << slot)) != 0;
		CHECK_EQ(mask != 0, world.generator.Generate(Info())); CHECK_EQ(9, submissions);
		for (const auto& effect : effects) { CHECK(effect->GetEffectTarget() == nullptr); CHECK_EQ(42, effect->GetActionInfo()); }
	}
}

TEST(RectangleEffectGenerator, IceAndMineActionsOverrideSurroundingPowerAfterTheCenter)
{
	World world;
	struct Case { TYPE_ACTIONINFO action; int power, count; };
	for (const Case c : {Case{SKILL_WIDE_ICE_FIELD, 2, 25}, {SKILL_LAND_MINE_EXPLOSION, 3, 49}})
	{
		ClearEffects(); auto info = Info(); info.nActionInfo = c.action;
		CHECK(world.generator.Generate(info)); CHECK_EQ(c.count, effects.size()); CHECK_EQ(1, effects.front()->GetPower());
		CHECK_EQ(5 - c.power, attempted[1].x); CHECK_EQ(5 - c.power, attempted[1].y);
		CHECK_EQ(5 + c.power, attempted.back().x); CHECK_EQ(5 + c.power, attempted.back().y);
		for (size_t i = 1; i < effects.size(); ++i) CHECK_EQ(c.power, effects[i]->GetPower());
	}
}

TEST(RectangleEffectGenerator, FirstDarknessPhasesRandomizeCenterThenSurroundingVariants)
{
	World world;
	for (const int base : {EFFECTSPRITETYPE_DARKNESS_1_1, EFFECTSPRITETYPE_GRAY_DARKNESS_1_1})
	{
		ClearEffects(); sprite.frameID = static_cast<TYPE_FRAMEID>(base + 4);
		std::srand(137); std::vector<int> expected; for (int i = 0; i < 9; ++i) expected.push_back(base + std::rand() % 5);
		const int next = std::rand(); std::srand(137);
		CHECK(world.generator.Generate(Info())); CHECK(submittedFrames == expected); CHECK_EQ(expected.front(), requestedFrame);
		CHECK_EQ(next, std::rand()); CHECK(delays == std::vector<DWORD>(9, 0));
	}
}

TEST(RectangleEffectGenerator, LaterDarknessPhasesKeepTheirCenterButRestartSurroundingVariants)
{
	World world;
	struct Case { int frame, base; };
	for (const Case c : {Case{EFFECTSPRITETYPE_DARKNESS_2_1, EFFECTSPRITETYPE_DARKNESS_1_1},
		{EFFECTSPRITETYPE_DARKNESS_3_5, EFFECTSPRITETYPE_DARKNESS_1_1},
		{EFFECTSPRITETYPE_GRAY_DARKNESS_2_1, EFFECTSPRITETYPE_GRAY_DARKNESS_1_1},
		{EFFECTSPRITETYPE_GRAY_DARKNESS_3_5, EFFECTSPRITETYPE_GRAY_DARKNESS_1_1}})
	{
		ClearEffects(); sprite.frameID = static_cast<TYPE_FRAMEID>(c.frame);
		std::srand(271); std::vector<int> expected{c.frame}; for (int i = 0; i < 8; ++i) expected.push_back(c.base + std::rand() % 5);
		const int next = std::rand(); std::srand(271);
		CHECK(world.generator.Generate(Info())); CHECK(submittedFrames == expected); CHECK_EQ(c.frame, requestedFrame); CHECK_EQ(next, std::rand());
	}
}

TEST(RectangleEffectGenerator, PreviousDarknessFramesAdvanceBeforeFrameLookupAndClassification)
{
	World world;
	struct Case { int previous, selected, surrounding; };
	for (const Case c : {Case{EFFECTSPRITETYPE_DARKNESS_1_1, EFFECTSPRITETYPE_DARKNESS_2_1, EFFECTSPRITETYPE_DARKNESS_1_1},
		{EFFECTSPRITETYPE_GRAY_DARKNESS_1_1, EFFECTSPRITETYPE_GRAY_DARKNESS_2_1, EFFECTSPRITETYPE_GRAY_DARKNESS_1_1},
		{65534, 3, EFFECTSPRITETYPE_GRAY_DARKNESS_1_1}})
	{
		ClearEffects(); sprite.frameID = EFFECTSPRITETYPE_DARKNESS_1_1;
		MEffect previous(BLT_NORMAL); previous.SetFrameID(static_cast<TYPE_FRAMEID>(c.previous), 7);
		auto info = Info(); info.pPreviousEffect = &previous;
		std::srand(391); std::vector<int> expected{c.selected}; for (int i = 0; i < 8; ++i) expected.push_back(c.surrounding + std::rand() % 5);
		const int next = std::rand(); std::srand(391);
		CHECK(world.generator.Generate(info)); CHECK(submittedFrames == expected); CHECK_EQ(c.selected, requestedFrame); CHECK_EQ(next, std::rand());
		CHECK_EQ(c.previous, previous.GetFrameID());
	}
}

TEST(RectangleEffectGenerator, DarknessUsesResolvedFramesAndWideActionsOverrideCenterPower)
{
	World world;
	for (const int frame : {EFFECTSPRITETYPE_DARKNESS_1_1, EFFECTSPRITETYPE_GRAY_DARKNESS_1_1})
	{
		for (const TYPE_ACTIONINFO action : {static_cast<TYPE_ACTIONINFO>(RESULT_MAGIC_DARKNESS_WIDE), static_cast<TYPE_ACTIONINFO>(RESULT_SKILL_WIDE_GRAY_DARKNESS)})
		{
			ClearEffects(); sprite.frameID = static_cast<TYPE_FRAMEID>(frame); auto info = Info(); info.nActionInfo = action;
			CHECK(world.generator.Generate(info)); CHECK_EQ(25, effects.size());
			for (const auto& effect : effects) CHECK_EQ(2, effect->GetPower());
		}
	}
	ClearEffects(); sprite.frameID = 12; auto info = Info(); info.effectSpriteType = EFFECTSPRITETYPE_DARKNESS_1_1;
	info.nActionInfo = RESULT_MAGIC_DARKNESS_WIDE;
	std::srand(411); const int next = std::rand(); std::srand(411);
	CHECK(world.generator.Generate(info)); CHECK_EQ(9, effects.size()); CHECK_EQ(1, effects.front()->GetPower()); CHECK_EQ(next, std::rand());
}

TEST(RectangleEffectGenerator, SharpHailDrawsEachSurroundingVariantBeforeItsDelay)
{
	World world;
	sprite.frameID = EFFECTSPRITETYPE_SHARP_HAIL_DROP_1;
	std::srand(571); std::vector<int> expectedFrames{EFFECTSPRITETYPE_SHARP_HAIL_DROP_1}; std::vector<DWORD> expectedDelays{0};
	for (int i = 0; i < 24; ++i) { expectedFrames.push_back(EFFECTSPRITETYPE_SHARP_HAIL_DROP_1 + std::rand() % 3); expectedDelays.push_back(std::rand() % 16); }
	const int next = std::rand(); std::srand(571);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(25, effects.size()); CHECK(submittedFrames == expectedFrames); CHECK(delays == expectedDelays);
	CHECK_EQ(EFFECTSPRITETYPE_SHARP_HAIL_DROP_1, requestedFrame); CHECK_EQ(next, std::rand());
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(2, effects[i]->GetPower()); CHECK_EQ(3, effects[i]->GetMaxFrame()); CHECK_EQ(0, effects[i]->GetFrame());
		CHECK_EQ(129 + expectedDelays[i], effects[i]->GetEndFrame()); CHECK_EQ(104, effects[i]->GetEndLinkFrame());
		CHECK_EQ(expectedDelays[i] != 0, effects[i]->IsWaitFrame());
	}
}

TEST(RectangleEffectGenerator, MineDelayExtendsLifetimeWithoutWordNarrowingOrChangingLinkTime)
{
	World world;
	auto info = Info(); info.nActionInfo = SKILL_LAND_MINE_EXPLOSION; info.count = 65535;
	std::srand(617); std::vector<DWORD> expected{0}; for (int i = 0; i < 48; ++i) expected.push_back(std::rand() % 16);
	const int next = std::rand(); std::srand(617);
	CHECK(world.generator.Generate(info)); CHECK_EQ(49, effects.size()); CHECK(delays == expected); CHECK_EQ(next, std::rand());
	for (size_t i = 0; i < effects.size(); ++i)
	{
		CHECK_EQ(65634 + expected[i], effects[i]->GetEndFrame()); CHECK_EQ(104, effects[i]->GetEndLinkFrame()); CHECK_EQ(12, effects[i]->GetFrameID());
		frameNow = 100; CHECK_EQ(expected[i] != 0, effects[i]->IsWaitFrame());
		frameNow = 100 + expected[i]; CHECK(!effects[i]->IsWaitFrame());
	}
}

TEST(RectangleEffectGenerator, MineActionKeepsHailVariantsButExpandsOnlyTheSurroundingArea)
{
	World world;
	sprite.frameID = EFFECTSPRITETYPE_SHARP_HAIL_DROP_1;
	auto info = Info(); info.nActionInfo = SKILL_LAND_MINE_EXPLOSION;
	std::srand(673); std::vector<int> expectedFrames{EFFECTSPRITETYPE_SHARP_HAIL_DROP_1}; std::vector<DWORD> expectedDelays{0};
	for (int i = 0; i < 48; ++i) { expectedFrames.push_back(EFFECTSPRITETYPE_SHARP_HAIL_DROP_1 + std::rand() % 3); expectedDelays.push_back(std::rand() % 16); }
	const int next = std::rand(); std::srand(673);
	CHECK(world.generator.Generate(info)); CHECK_EQ(49, effects.size()); CHECK(submittedFrames == expectedFrames); CHECK(delays == expectedDelays);
	CHECK_EQ(2, effects.front()->GetPower()); for (size_t i = 1; i < effects.size(); ++i) CHECK_EQ(3, effects[i]->GetPower());
	CHECK_EQ(next, std::rand());
}

TEST(RectangleEffectGenerator, RejectedDarknessMinesStillConsumeAllVariantsAndDelays)
{
	World world;
	sprite.frameID = EFFECTSPRITETYPE_DARKNESS_1_1; acceptQueue = false;
	auto target = Target(); auto info = Info(); info.nActionInfo = SKILL_LAND_MINE_EXPLOSION; info.pEffectTarget = target.get();
	std::srand(733); std::vector<int> expectedFrames{EFFECTSPRITETYPE_DARKNESS_1_1 + std::rand() % 5}; std::vector<DWORD> expectedDelays{0};
	for (int i = 0; i < 48; ++i) { expectedFrames.push_back(EFFECTSPRITETYPE_DARKNESS_1_1 + std::rand() % 5); expectedDelays.push_back(std::rand() % 16); }
	const int next = std::rand(); std::srand(733);
	CHECK(!world.generator.Generate(info)); CHECK_EQ(49, submissions); CHECK(effects.empty());
	CHECK(submittedFrames == expectedFrames); CHECK(delays == expectedDelays); CHECK_EQ(next, std::rand());
	CHECK_EQ(777, target->GetX()); CHECK(removedTargets.empty());
}

TEST(RectangleEffectGenerator, HailNeighboursAndOrdinaryFramesDoNotRandomizeOrOverridePower)
{
	World world;
	for (const int frame : {12, static_cast<int>(EFFECTSPRITETYPE_DARKNESS_1_1) - 1,
		static_cast<int>(EFFECTSPRITETYPE_DARKNESS_3_5) + 1, static_cast<int>(EFFECTSPRITETYPE_GRAY_DARKNESS_1_1) - 1,
		static_cast<int>(EFFECTSPRITETYPE_GRAY_DARKNESS_3_5) + 1,
		static_cast<int>(EFFECTSPRITETYPE_SHARP_HAIL_DROP_2), static_cast<int>(EFFECTSPRITETYPE_SHARP_HAIL_DROP_3)})
	{
		ClearEffects(); sprite.frameID = static_cast<TYPE_FRAMEID>(frame);
		std::srand(787); const int next = std::rand(); std::srand(787);
		CHECK(world.generator.Generate(Info())); CHECK_EQ(9, effects.size()); CHECK_EQ(next, std::rand());
		CHECK(submittedFrames == std::vector<int>(9, frame)); CHECK(delays == std::vector<DWORD>(9, 0));
		for (const auto& effect : effects) CHECK_EQ(1, effect->GetPower());
	}
}

TEST(RectangleEffectGenerator, MissingSpriteServicesRejectBeforeSelectionOrConstruction)
{
	World world;
	const MRectZoneEffectHost empty{};
	for (const auto* service : {static_cast<const MRectZoneEffectHost*>(nullptr), &empty, &host})
	{
		ClearEffects(); spriteAvailable = false; sprite.frameID = EFFECTSPRITETYPE_DARKNESS_1_1; MStopZoneRectEffectGenerator::SetHost(service);
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		std::srand(811); const int next = std::rand(); std::srand(811);
		CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions); CHECK_EQ(next, std::rand());
		CHECK(calls == (service == &host ? std::vector<int>({1}) : std::vector<int>{})); CHECK_EQ(777, target->GetX());
	}
}

TEST(RectangleEffectGenerator, MissingFramesRejectAfterCenterVariantSelection)
{
	World world;
	const MRectZoneEffectHost noFrames{.Sprite = host.Sprite, .Bounds = host.Bounds, .Queue = host.Queue};
	for (const auto* service : {&noFrames, &host})
	{
		ClearEffects(); framesAvailable = false; sprite.frameID = EFFECTSPRITETYPE_DARKNESS_1_1; MStopZoneRectEffectGenerator::SetHost(service);
		auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
		std::srand(839); (void)std::rand(); const int next = std::rand(); std::srand(839);
		CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions); CHECK_EQ(next, std::rand());
		CHECK(calls == (service == &host ? std::vector<int>({1, 2}) : std::vector<int>({1}))); CHECK_EQ(777, target->GetX());
	}
}

TEST(RectangleEffectGenerator, MissingBoundsPreserveTheCenterSubmissionAndItsOwnershipResult)
{
	World world;
	const MRectZoneEffectHost noBounds{.Sprite = host.Sprite, .MaxFrames = host.MaxFrames, .Queue = host.Queue};
	for (const auto* service : {&noBounds, &host})
	{
		for (const bool accepted : {false, true})
		{
			ClearEffects(); boundsAvailable = false; acceptQueue = accepted; MStopZoneRectEffectGenerator::SetHost(service);
			auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
			CHECK_EQ(accepted, world.generator.Generate(info)); CHECK_EQ(1, submissions); CHECK_EQ(accepted ? 1 : 0, effects.size());
			CHECK_EQ(777, target->GetX()); if (accepted) { CHECK(effects.front()->GetEffectTarget() == target.get()); target.release(); }
			CHECK(calls == (service == &host ? std::vector<int>({1, 2, 3, 4, 5, 6}) : std::vector<int>({1, 2, 3, 4, 5})));
		}
	}
}

TEST(RectangleEffectGenerator, MissingQueueReleasesEveryUnlinkedEffect)
{
	World world;
	const MRectZoneEffectHost noQueue{.Sprite = host.Sprite, .MaxFrames = host.MaxFrames, .Bounds = host.Bounds};
	MStopZoneRectEffectGenerator::SetHost(&noQueue);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions); CHECK_EQ(777, target->GetX()); CHECK(removedTargets.empty());
	std::vector<int> expected{1, 2, 3, 4, 6}; for (int i = 0; i < 8; ++i) expected.insert(expected.end(), {3, 4});
	CHECK(calls == expected);
}

TEST(RectangleEffectGenerator, SpriteCallbackCanReplaceFrameBoundsAndQueueServices)
{
	World world;
	const MRectZoneEffectHost changing{
		.Sprite = [](TYPE_EFFECTSPRITETYPE, MRectZoneEffectSprite& result) {
			result = {BLT_NORMAL, 23}; maxFrames = 258; zoneBounds = {6, 6}; MStopZoneRectEffectGenerator::SetHost(&host); return true;
		},
	};
	CHECK(MStopZoneRectEffectGenerator::SetHost(&changing) == &host);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(4, effects.size()); CHECK_EQ(BLT_NORMAL, requestedBlt); CHECK_EQ(23, requestedFrame);
	CHECK(attempted == std::vector<Point>({{5, 5}, {4, 4}, {5, 4}, {4, 5}}));
	for (const auto& effect : effects) { CHECK_EQ(23, effect->GetFrameID()); CHECK_EQ(2, effect->GetMaxFrame()); CHECK_EQ(BLT_NORMAL, effect->GetBltType()); }
	CHECK_EQ(2, calls.front()); CHECK(MStopZoneRectEffectGenerator::SetHost(nullptr) == &host);
}

TEST(RectangleEffectGenerator, FrameCallbackCanRemoveTheQueueAndBoundsBeforeConstruction)
{
	World world;
	const MRectZoneEffectHost changing{
		.Sprite = host.Sprite,
		.MaxFrames = [](BYTE, TYPE_FRAMEID, int& count) { calls.push_back(2); count = 3; MStopZoneRectEffectGenerator::SetHost(nullptr); return true; },
		.Bounds = host.Bounds, .Queue = host.Queue,
	};
	MStopZoneRectEffectGenerator::SetHost(&changing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(effects.empty()); CHECK_EQ(0, submissions); CHECK_EQ(777, target->GetX());
	CHECK(calls == std::vector<int>({1, 2, 3, 4}));
}

TEST(RectangleEffectGenerator, CenterQueueReplacementChangesTheBoundsButKeepsResolvedFrames)
{
	World world;
	const MRectZoneEffectHost changing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Queue = [](std::unique_ptr<MEffect> effect, DWORD delay) {
			zoneBounds = {6, 6}; sprite = {BLT_NORMAL, 23}; maxFrames = 8; MStopZoneRectEffectGenerator::SetHost(&host);
			return host.Queue(std::move(effect), delay);
		},
	};
	MStopZoneRectEffectGenerator::SetHost(&changing);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(4, effects.size());
	for (const auto& effect : effects) { CHECK_EQ(12, effect->GetFrameID()); CHECK_EQ(3, effect->GetMaxFrame()); }
	ClearEffects(); CHECK(world.generator.Generate(Info())); CHECK_EQ(4, effects.size());
	for (const auto& effect : effects) { CHECK_EQ(23, effect->GetFrameID()); CHECK_EQ(8, effect->GetMaxFrame()); }
}

TEST(RectangleEffectGenerator, BoundsCallbackCanRemoveTheQueueAfterCenterOwnershipTransfers)
{
	World world;
	const MRectZoneEffectHost changing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames,
		.Bounds = [](MRectZoneEffectBounds& result) { calls.push_back(6); result = zoneBounds; MStopZoneRectEffectGenerator::SetHost(nullptr); return true; },
		.Queue = host.Queue,
	};
	MStopZoneRectEffectGenerator::SetHost(&changing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(world.generator.Generate(info)); CHECK_EQ(1, effects.size()); CHECK_EQ(1, submissions); CHECK_EQ(22, calls.size());
	CHECK(effects.front()->GetEffectTarget() == target.get()); target.release(); CHECK_EQ(777, info.pEffectTarget->GetX());
}

TEST(RectangleEffectGenerator, RejectingQueuesDestroyTheirOwnMarkersAndKeepTheCallerTarget)
{
	World world;
	const MRectZoneEffectHost rejecting{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect, DWORD) { effect->SetLink(43, Target(74).release()); return false; },
	};
	MStopZoneRectEffectGenerator::SetHost(&rejecting);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	CHECK(!world.generator.Generate(info)); CHECK(removedTargets == std::vector<int>(9, 74));
	target.reset(); CHECK_EQ(10, removedTargets.size()); CHECK_EQ(73, removedTargets.back());
}

TEST(RectangleEffectGenerator, FirstQueueExceptionReleasesItsEffectAndKeepsCallerOwnership)
{
	World world;
	const MRectZoneEffectHost throwing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect, DWORD) -> bool { effect->SetLink(43, Target(74).release()); throw std::runtime_error("Queue failure"); },
	};
	MStopZoneRectEffectGenerator::SetHost(&throwing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	bool threw = false; try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK(effects.empty()); CHECK(removedTargets == std::vector<int>({74})); CHECK_EQ(777, target->GetX());
	target.reset(); CHECK(removedTargets == std::vector<int>({74, 73}));
}

TEST(RectangleEffectGenerator, LaterQueueExceptionKeepsTheAlreadyTransferredOriginal)
{
	World world;
	const MRectZoneEffectHost throwing{
		.Sprite = host.Sprite, .MaxFrames = host.MaxFrames, .Bounds = host.Bounds,
		.Queue = [](std::unique_ptr<MEffect> effect, DWORD delay) -> bool {
			if (effects.empty()) return host.Queue(std::move(effect), delay);
			effect->SetLink(43, Target(74).release()); throw std::runtime_error("Later queue failure");
		},
	};
	MStopZoneRectEffectGenerator::SetHost(&throwing);
	auto target = Target(); auto info = Info(); info.pEffectTarget = target.get();
	bool threw = false; try { world.generator.Generate(info); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw); CHECK_EQ(1, effects.size()); CHECK(effects.front()->GetEffectTarget() == target.get()); target.release();
	CHECK(removedTargets == std::vector<int>({74})); CHECK_EQ(777, info.pEffectTarget->GetX());
	ClearEffects(); CHECK(removedTargets == std::vector<int>({74, 73}));
}

TEST(RectangleEffectGenerator, FrameCountsNarrowOnceWithoutRandomAnimationStarts)
{
	World world;
	struct Case { int supplied, expected; };
	for (const Case c : {Case{-1, 255}, {0, 0}, {256, 0}, {258, 2}})
	{
		ClearEffects(); maxFrames = c.supplied;
		std::srand(881); const int next = std::rand(); std::srand(881);
		CHECK(world.generator.Generate(Info())); CHECK_EQ(next, std::rand());
		for (const auto& effect : effects) { CHECK_EQ(c.expected, effect->GetMaxFrame()); CHECK_EQ(0, effect->GetFrame()); }
	}
}

TEST(RectangleEffectGenerator, RealEffectsAnimateAndRefreshLightBeforeTheirStationaryExpiry)
{
	World world;
	sprite.bltType = BLT_NORMAL; CHECK(world.generator.Generate(Info()));
	for (const auto& effect : effects)
	{
		const int x = effect->GetPixelX(), y = effect->GetPixelY();
		frameNow = 100; CHECK(effect->Update()); CHECK_EQ(1, effect->GetFrame()); CHECK_EQ(8, effect->GetLight());
		CHECK_EQ(x, effect->GetPixelX()); CHECK_EQ(y, effect->GetPixelY()); CHECK_EQ(17, effect->GetPixelZ());
		frameNow = 130; CHECK(!effect->Update()); CHECK_EQ(2, effect->GetFrame()); CHECK_EQ(9, effect->GetLight());
	}
}

TEST(RectangleEffectGenerator, LinkCountsAndWrappedClocksKeepTheirExistingDeadlineSemantics)
{
	World world;
	auto info = Info(); info.count = 65535; info.linkCount = MAX_LINKCOUNT;
	CHECK(world.generator.Generate(info)); CHECK_EQ(65634, effects.back()->GetEndFrame()); CHECK_EQ(65634, effects.back()->GetEndLinkFrame());
	ClearEffects(); info.count = 10; info.linkCount = 20;
	CHECK(world.generator.Generate(info)); CHECK_EQ(109, effects.back()->GetEndFrame()); CHECK_EQ(119, effects.back()->GetEndLinkFrame());
	ClearEffects(); frameNow = (std::numeric_limits<DWORD>::max)() - 1; info.count = 4;
	CHECK(world.generator.Generate(info)); CHECK_EQ(1, effects.back()->GetEndFrame()); CHECK(!effects.back()->Update());
	frameNow = 0; CHECK(effects.back()->Update()); CHECK_EQ(2, effects.back()->GetFrame());
}

TEST(RectangleEffectGenerator, MissingBaseServicesKeepFiniteCountsAndInactiveEffects)
{
	World world;
	MEffect::SetHost(nullptr);
	CHECK(world.generator.Generate(Info())); CHECK_EQ(29, effects.front()->GetEndFrame());
	CHECK_EQ(4, effects.front()->GetEndLinkFrame()); CHECK_EQ(0, effects.front()->GetLight()); CHECK(!effects.front()->Update());
}
