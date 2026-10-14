#include "test_framework.h"
#include "gamemodel_world.h"
#include "EffectSpriteTypeDef.h"
#include "MAttachZoneEffectGenerator.h"
#include "MEffect.h"
#include "MEffectTarget.h"
#include "MEventQueue.h"
#include "MRippleZonePixelEffectGenerator.h"
#include "MViewDef.h"
#include "SkillDef.h"
#include "UserInformation.h"
#include "WorldEventsHost.h"
#include "WorldTileGeometry.h"
#include "ZoneStateHost.h"
#include <cstdlib>
#include <map>
#include <memory>
#include <utility>
#include <vector>

// The attach-zone generator lays a ground effect at a point: blood at a
// random spot near it in one of its frames, a quest's magic zone up and
// to the left, Great Ruffian's axe shaking the screen; by the action's
// grade, on a row, a cross or a square of tiles round it (the first
// following the target, the others copies); Mercy Ground's nine parts at
// the point. The pixel-ripple generator lays an effect a step from its
// start toward the target (Power of Land's stones and earthquakes on the
// ground, a few pixels on, faster at the target's first phase; blood
// transfusion toward a creature, unless it is already there, green when
// gore is off), or at the target itself, and points the target past it.
// A sprite past the table makes none; only an effect the zone keeps is
// linked.
namespace {

constexpr int kTableSize = MAX_EFFECTSPRITETYPE;
DWORD frameNow;
int acceptLimit, refuseFirst, offered;
struct Queued
{
	std::unique_ptr<MEffect> effect;
	bool ground;
};
std::vector<Queued> queued;
std::vector<MEvent> events;
std::vector<int> removedTargets;
int creatureAtTile;
std::map<TYPE_OBJECTID, std::pair<int, int>> creatures;

MEffect& Effect(size_t i) { return *queued.at(i).effect; }

const MEffectHost effectHost{
	.CurrentFrame = []() { return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE) { return 0; },
};
const MEffectTargetHost targetHost{.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); }};
int refusedType = -1;
bool Sprite(TYPE_EFFECTSPRITETYPE type, MFixedZoneEffectSprite& sprite)
{
	if (type >= kTableSize || type == refusedType) return false;
	sprite = {BLT_EFFECT, static_cast<TYPE_FRAMEID>(type), 3};
	return true;
}
bool Keep(std::unique_ptr<MEffect> effect, bool ground)
{
	// Refuses the first few offered, and any past the limit.
	if (++offered <= refuseFirst) return false;
	if (static_cast<int>(queued.size()) >= acceptLimit) return false;
	queued.push_back({std::move(effect), ground});
	return true;
}
const MFixedZoneEffectHost groundHost{
	.Sprite = Sprite,
	.Queue = [](std::unique_ptr<MEffect> effect) { return Keep(std::move(effect), true); },
};
const MRippleZoneEffectHost rippleHost{
	.Bounds = nullptr,
	.Sprite = Sprite,
	.Queue = Keep,
};
const WorldEvents::Host worldEvents = []() {
	WorldEvents::Host h{};
	h.AddEvent = [](MEvent& event) { events.push_back(event); };
	return h;
}();
const ZoneState::Host zone = []() {
	ZoneState::Host h{};
	h.GetCreature = [](TYPE_OBJECTID id) {
		return creatures.count(id) ? reinterpret_cast<MCreature*>(&creatureAtTile) : nullptr;
	};
	return h;
}();
const CreatureState::Host creature = []() {
	CreatureState::Host h{};
	h.GetX = [](MCreature*) { return static_cast<TYPE_SECTORPOSITION>(creatures.begin()->second.first); };
	h.GetY = [](MCreature*) { return static_cast<TYPE_SECTORPOSITION>(creatures.begin()->second.second); };
	return h;
}();

struct World : GameModelWorld
{
	const MEffectHost* oldEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* oldTarget = MEffectTarget::SetHost(&targetHost);
	const MFixedZoneEffectHost* oldGround = MAttachZoneEffectGenerator::SetHost(&groundHost);
	const MRippleZoneEffectHost* oldRipple = MRippleZonePixelEffectGenerator::SetHost(&rippleHost);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&worldEvents);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	MAttachZoneEffectGenerator attach;
	MRippleZonePixelEffectGenerator ripple;
	World()
	{
		frameNow = 100;
		acceptLimit = 99;
		refuseFirst = offered = 0;
		queued.clear();
		events.clear();
		removedTargets.clear();
		creatures.clear();
		refusedType = -1;
		g_pUserInformation->GoreLevel = true;
	}
	~World()
	{
		queued.clear();
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		WorldEvents::SetHost(oldEvents);
		MRippleZonePixelEffectGenerator::SetHost(oldRipple);
		MAttachZoneEffectGenerator::SetHost(oldGround);
		MEffectTarget::SetHost(oldTarget);
		MEffect::SetHost(oldEffect);
	}
};

EFFECTGENERATOR_INFO Info(int spriteType, BYTE grade = 0)
{
	EFFECTGENERATOR_INFO info{};
	info.nActionInfo = 42;
	info.effectSpriteType = static_cast<TYPE_EFFECTSPRITETYPE>(spriteType);
	info.x0 = 480;
	info.y0 = 240;
	info.z0 = 7;
	info.x1 = 576;
	info.y1 = 288;
	info.creatureID = 123;
	info.direction = DIRECTION_RIGHT;
	info.step = 20;
	info.count = 30;
	info.linkCount = 5;
	info.power = 77;
	info.temp1 = grade;
	info.z1 = 9;	// never used: a ripple lies at height 0
	return info;
}

void CheckEffect(MEffect& effect, int type, int x, int y, int z, BYTE direction = DIRECTION_RIGHT)
{
	CHECK_EQ(BLT_EFFECT, effect.GetBltType());
	CHECK_EQ(type, effect.GetFrameID());
	CHECK_EQ(3, effect.GetMaxFrame());
	CHECK_EQ(x, effect.GetPixelX());
	CHECK_EQ(y, effect.GetPixelY());
	CHECK_EQ(z, effect.GetPixelZ());
	CHECK_EQ(20, effect.GetStepPixel());
	CHECK_EQ(frameNow + 30 - 1, effect.GetEndFrame());
	CHECK_EQ(frameNow + 5 - 1, effect.GetEndLinkFrame());
	CHECK_EQ(direction, effect.GetDirection());
	CHECK_EQ(77, effect.GetPower());
}

std::unique_ptr<MEffectTarget> Target(int phases = 0);
bool GroundOnce(int spriteType);

std::unique_ptr<MEffectTarget> Target(int phases)
{
	auto target = std::make_unique<MEffectTarget>(3);
	for (int i = 0; i < phases; i++) target->NextPhase();
	target->Set(1, 2, 3, 456);
	return target;
}

} // namespace

TEST(GroundEffectGenerators, AnAttachZoneEffectLiesAtItsPoint)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_ATTACH_ZONE, world.attach.GetID());
	auto target = Target();
	EFFECTGENERATOR_INFO info = Info(500);
	info.direction = DIRECTION_LEFTUP;
	info.pEffectTarget = target.get();
	CHECK(world.attach.Generate(info));
	target.release();
	CHECK_EQ(1, queued.size());
	CHECK(queued.at(0).ground);
	CheckEffect(Effect(0), 500, 480, 240, 7, DIRECTION_LEFTUP);
	CHECK(Effect(0).GetEffectTarget() == info.pEffectTarget);
	CHECK_EQ(42, Effect(0).GetActionInfo());
	CHECK(events.empty());
	// The zone refusing it: false, and the caller keeps the target.
	queued.clear();
	acceptLimit = 0;
	CHECK(!world.attach.Generate(Info(500)));
	// A sprite past the table, or no host: none.
	acceptLimit = 99;
	CHECK(!world.attach.Generate(Info(kTableSize)));
	MAttachZoneEffectGenerator::SetHost(nullptr);
	CHECK(!world.attach.Generate(Info(500)));
	CHECK(queued.empty());
}

TEST(GroundEffectGenerators, BloodLandsNearItsPointInARandomFrame)
{
	World world;
	const struct { int type, frames; } cases[] = {
		{EFFECTSPRITETYPE_GREEN_BLOOD_GROUND_2_1, 4}, {EFFECTSPRITETYPE_GREEN_BLOOD_GROUND_1_1, 5},
		{EFFECTSPRITETYPE_BLOOD_GROUND_2_1, 4}, {EFFECTSPRITETYPE_BLOOD_GROUND_1_1, 5}};
	for (const auto& c : cases)
	{
		// Seeds until every frame has been drawn.
		std::vector<bool> seen(c.frames, false);
		auto all = [&seen]() { for (bool s : seen) if (!s) return false; return true; };
		for (unsigned seed = 1; seed <= 400 && !all(); seed++)
		{
			std::srand(seed);
			const int frame = std::rand() % c.frames;
			const int dx = std::rand() % TILE_X_HALF - (TILE_X_HALF >> 1);
			const int dy = std::rand() % 24 - 12;
			seen[frame] = true;
			std::srand(seed);
			queued.clear();
			CHECK(world.attach.Generate(Info(c.type)));
			CHECK_EQ(1, queued.size());
			if (queued.empty()) continue;
			CheckEffect(Effect(0), c.type + frame, 480 + dx, 240 + dy, 7);
		}
		CHECK(all());
	}
}

TEST(GroundEffectGenerators, AnAxeShakesTheScreenAndAMagicZoneSitsUpAndLeft)
{
	World world;
	CHECK(world.attach.Generate(Info(EFFECTSPRITETYPE_GREAT_RUFFIAN_1_AXE_GROUND)));
	CHECK_EQ(1, events.size());
	CHECK(events[0].eventID == EVENTID_METEOR_SHAKE && events[0].eventType == EVENTTYPE_ZONE);
	CHECK(events[0].eventDelay == 500 && events[0].eventFlag == EVENTFLAG_SHAKE_SCREEN && events[0].parameter3 == 3);
	CheckEffect(Effect(0), EFFECTSPRITETYPE_GREAT_RUFFIAN_1_AXE_GROUND, 480, 240, 7);
	queued.clear();
	CHECK(world.attach.Generate(Info(EFFECTSPRITETYPE_QUEST_MAGIC_ZONE)));
	CheckEffect(Effect(0), EFFECTSPRITETYPE_QUEST_MAGIC_ZONE, 480 - 26, 240 - 14, 7);
	CHECK_EQ(1, events.size());
}

TEST(GroundEffectGenerators, TheActionsGradeLaysARowACrossOrASquare)
{
	World world;
	// The point is tile (10, 10).
	const std::vector<std::pair<int, int>> row = {{9, 10}, {10, 10}, {11, 10}};
	const std::vector<std::pair<int, int>> cross = {{9, 10}, {10, 9}, {11, 10}, {10, 11}, {10, 10}};
	std::vector<std::pair<int, int>> square;
	for (int i = -1; i <= 1; i++)
		for (int j = -1; j <= 1; j++) square.push_back({10 + i, 10 + j});
	const std::pair<BYTE, std::vector<std::pair<int, int>>> grades[] = {{1, row}, {2, cross}, {3, square}};
	for (const auto& [grade, tiles] : grades)
	{
		queued.clear();
		auto target = Target();
		EFFECTGENERATOR_INFO info = Info(500, grade);
		info.pEffectTarget = target.get();
		CHECK(world.attach.Generate(info));
		target.release();
		CHECK_EQ(tiles.size(), queued.size());
		for (size_t i = 0; i < tiles.size(); i++)
		{
			CHECK(queued[i].ground);
			CheckEffect(Effect(i), 500, WorldTileGeometry::TileToPixelX(tiles[i].first), WorldTileGeometry::TileToPixelY(tiles[i].second), 7);
			CHECK_EQ(42, Effect(i).GetActionInfo());
			if (i == 0) CHECK(Effect(i).GetEffectTarget() == info.pEffectTarget);
			else
			{
				MEffectTarget* pCopy = Effect(i).GetEffectTarget();
				CHECK(pCopy != nullptr && pCopy != info.pEffectTarget);
				CHECK(pCopy != nullptr && pCopy->GetX() == 1 && pCopy->GetY() == 2 && pCopy->GetID() == 456);
			}
		}
	}
	// The first refused: the kept ones follow copies, and the target the
	// caller passed is followed by none (it leaks; still true).
	queued.clear();
	refuseFirst = 1;
	offered = 0;
	auto unlinked = Target();
	EFFECTGENERATOR_INFO firstRefused = Info(500, 1);
	firstRefused.pEffectTarget = unlinked.get();
	CHECK(world.attach.Generate(firstRefused));
	CHECK_EQ(2, queued.size());
	CHECK(queued.size() == 2 && Effect(0).GetEffectTarget() != nullptr && Effect(0).GetEffectTarget() != firstRefused.pEffectTarget);
	refuseFirst = 0;
	// A magic zone's grade lays round its shifted point: (454, 226) is
	// tile (9, 9).
	queued.clear();
	CHECK(world.attach.Generate(Info(EFFECTSPRITETYPE_QUEST_MAGIC_ZONE, 1)));
	CHECK_EQ(3, queued.size());
	for (size_t i = 0; i < queued.size(); i++)
	{
		CHECK_EQ(WorldTileGeometry::TileToPixelX(8 + static_cast<int>(i)), Effect(i).GetPixelX());
		CHECK_EQ(WorldTileGeometry::TileToPixelY(9), Effect(i).GetPixelY());
	}
	// A grade's tiles face the action's direction.
	queued.clear();
	EFFECTGENERATOR_INFO facing = Info(500, 1);
	facing.direction = DIRECTION_DOWN;
	CHECK(world.attach.Generate(facing));
	for (size_t i = 0; i < queued.size(); i++) CHECK_EQ(DIRECTION_DOWN, Effect(i).GetDirection());
	// An unknown grade lays nothing, and is still true.
	queued.clear();
	CHECK(world.attach.Generate(Info(500, 4)));
	CHECK(queued.empty());
	// Refused ones are gone; only the kept ones are linked. Always true.
	queued.clear();
	acceptLimit = 1;
	auto target = Target();
	EFFECTGENERATOR_INFO info = Info(500, 1);
	info.pEffectTarget = target.get();
	CHECK(world.attach.Generate(info));
	target.release();
	CHECK_EQ(1, queued.size());
	CHECK(Effect(0).GetEffectTarget() == info.pEffectTarget);
	// No target: the others follow the action alone.
	queued.clear();
	acceptLimit = 99;
	CHECK(world.attach.Generate(Info(500, 3)));
	CHECK_EQ(9, queued.size());
	CHECK(Effect(0).GetEffectTarget() == nullptr && Effect(8).GetEffectTarget() == nullptr);
	CHECK_EQ(42, Effect(0).GetActionInfo());
	CHECK_EQ(ACTIONINFO_NULL, Effect(8).GetActionInfo());
}

TEST(GroundEffectGenerators, MercyGroundLaysItsNineParts)
{
	World world;
	auto target = Target();
	EFFECTGENERATOR_INFO info = Info(EFFECTSPRITETYPE_MERCY_GROUND_1);
	info.pEffectTarget = target.get();
	CHECK(world.attach.Generate(info));
	target.release();
	CHECK_EQ(9, queued.size());
	for (size_t i = 0; i < 9; i++)
	{
		CHECK(queued[i].ground);
		CheckEffect(Effect(i), EFFECTSPRITETYPE_MERCY_GROUND_1 + static_cast<int>(i), 480, 240, 7);
		CHECK_EQ(42, Effect(i).GetActionInfo());
		if (i == 0) CHECK(Effect(i).GetEffectTarget() == info.pEffectTarget);
		else CHECK(Effect(i).GetEffectTarget() != nullptr && Effect(i).GetEffectTarget() != info.pEffectTarget);
	}
	// Mercy Ground's parts face the action's direction.
	queued.clear();
	EFFECTGENERATOR_INFO facing = Info(EFFECTSPRITETYPE_MERCY_GROUND_1);
	facing.direction = DIRECTION_UP;
	CHECK(world.attach.Generate(facing));
	CHECK_EQ(9, queued.size());
	for (size_t i = 0; i < queued.size(); i++) CHECK_EQ(DIRECTION_UP, Effect(i).GetDirection());
	// The zone keeping four: those four are linked, the rest are gone.
	queued.clear();
	acceptLimit = 4;
	auto four = Target();
	EFFECTGENERATOR_INFO fourInfo = Info(EFFECTSPRITETYPE_MERCY_GROUND_1);
	fourInfo.pEffectTarget = four.get();
	CHECK(world.attach.Generate(fourInfo));
	four.release();
	CHECK_EQ(4, queued.size());
	CHECK(queued.size() == 4 && Effect(0).GetEffectTarget() == fourInfo.pEffectTarget && Effect(3).GetEffectTarget() != nullptr);
	acceptLimit = 99;
	// A part whose sprite is refused is skipped, the rest laid in their own
	// frames; always true.
	queued.clear();
	refusedType = EFFECTSPRITETYPE_MERCY_GROUND_1 + 4;
	CHECK(world.attach.Generate(Info(EFFECTSPRITETYPE_MERCY_GROUND_1)));
	CHECK_EQ(8, queued.size());
	CHECK_EQ(EFFECTSPRITETYPE_MERCY_GROUND_1 + 3, Effect(3).GetFrameID());
	CHECK_EQ(EFFECTSPRITETYPE_MERCY_GROUND_1 + 5, Effect(4).GetFrameID());
	CHECK_EQ(EFFECTSPRITETYPE_MERCY_GROUND_1 + 8, Effect(7).GetFrameID());
}

TEST(GroundEffectGenerators, APlainRippleLiesAtTheTargetAndAimsTheTargetThere)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_RIPPLE_ZONE_PIXEL, world.ripple.GetID());
	auto target = Target();
	EFFECTGENERATOR_INFO info = Info(500);
	info.direction = DIRECTION_UP;
	info.pEffectTarget = target.get();
	CHECK(world.ripple.Generate(info));
	target.release();
	CHECK_EQ(1, queued.size());
	CHECK(!queued.at(0).ground);
	CheckEffect(Effect(0), 500, 576, 288, 0, DIRECTION_UP);
	MEffectTarget* pTarget = Effect(0).GetEffectTarget();
	CHECK(pTarget == info.pEffectTarget);
	CHECK(pTarget->GetX() == 576 && pTarget->GetY() == 288 && pTarget->GetZ() == 0 && pTarget->GetID() == 456);
	// Refused: false; past the table or no host: none.
	queued.clear();
	acceptLimit = 0;
	CHECK(!world.ripple.Generate(Info(500)));
	acceptLimit = 99;
	CHECK(!world.ripple.Generate(Info(kTableSize)));
	MRippleZonePixelEffectGenerator::SetHost(nullptr);
	CHECK(!world.ripple.Generate(Info(500)));
	CHECK(queued.empty());
}

namespace {

// Seeds from 1 on until a draw of rand() % 3 has given each value (some
// libraries' first draws barely vary between small seeds); each seed's
// frame is checked by `check`.
template <typename Check>
void ForEachFrame(Check check)
{
	bool seen[3] = {};
	for (unsigned seed = 1; seed <= 400 && !(seen[0] && seen[1] && seen[2]); seed++)
	{
		std::srand(seed);
		const int frame = std::rand() % 3;
		seen[frame] = true;
		std::srand(seed);
		check(frame);
	}
	CHECK(seen[0] && seen[1] && seen[2]);
}

} // namespace

TEST(GroundEffectGenerators, AnEarthquakeStepsOnTheGroundFasterAtTheFirstPhase)
{
	World world;
	ForEachFrame([](int frame) {
		queued.clear();
		CHECK(GroundOnce(EFFECTSPRITETYPE_EARTHQUAKE_1));
		CHECK_EQ(1, queued.size());
		if (queued.empty()) return;
		CHECK(queued[0].ground);
		CheckEffect(Effect(0), EFFECTSPRITETYPE_EARTHQUAKE_1 + frame, 493, 246, 0);
	});
	// Before the target's first phase, one step; the target twice as far.
	queued.clear();
	auto early = Target(0);
	EFFECTGENERATOR_INFO before = Info(EFFECTSPRITETYPE_EARTHQUAKE_1);
	before.pEffectTarget = early.get();
	CHECK(world.ripple.Generate(before));
	early.release();
	CHECK(queued.size() == 1 && Effect(0).GetPixelX() == 493 && Effect(0).GetPixelY() == 246);
	CHECK(before.pEffectTarget->GetX() == 506 && before.pEffectTarget->GetY() == 253 && before.pEffectTarget->GetZ() == 0);
	// At its first phase, three steps.
	queued.clear();
	auto target = Target(1);
	EFFECTGENERATOR_INFO info = Info(EFFECTSPRITETYPE_EARTHQUAKE_1);
	info.pEffectTarget = target.get();
	CHECK(world.ripple.Generate(info));
	target.release();
	CHECK(queued.size() == 1 && Effect(0).GetPixelX() == 520 && Effect(0).GetPixelY() == 260);
	CHECK(info.pEffectTarget->GetX() == 560 && info.pEffectTarget->GetY() == 280);
	// Straight down or straight across: the step heads a tile on from the
	// start in the direction instead.
	for (const auto& [x1, y1] : {std::pair<int, int>{480, 300}, std::pair<int, int>{360, 240}})
	{
		queued.clear();
		EFFECTGENERATOR_INFO straight = Info(EFFECTSPRITETYPE_EARTHQUAKE_1);
		straight.x1 = x1;
		straight.y1 = y1;
		CHECK(world.ripple.Generate(straight));
		CHECK(queued.size() == 1 && Effect(0).GetPixelX() == 500 && Effect(0).GetPixelY() == 240);
	}
}

TEST(GroundEffectGenerators, PowerOfLandsStonesKeepTheirLineAndTheTarget)
{
	World world;
	ForEachFrame([&world](int frame) {
		queued.clear();
		auto target = Target(1);
		EFFECTGENERATOR_INFO info = Info(EFFECTSPRITETYPE_POWER_OF_LAND_STONE_1);
		info.x1 = 480;
		info.y1 = 300;
		info.pEffectTarget = target.get();
		CHECK(world.ripple.Generate(info));
		target.release();
		CHECK_EQ(1, queued.size());
		if (queued.empty()) return;
		CHECK(queued[0].ground);
		CheckEffect(Effect(0), EFFECTSPRITETYPE_POWER_OF_LAND_STONE_2 + frame, 480, 250, 0);
		// The target stays where it was.
		CHECK(info.pEffectTarget->GetX() == 1 && info.pEffectTarget->GetY() == 2);
	});
}

TEST(GroundEffectGenerators, ATransfusionFlowsTowardTheCreatureUnlessItIsThere)
{
	World world;
	// The creature at tile (12, 8): pixel (576, 192).
	creatures[123] = {12, 8};
	for (bool gore : {true, false})
	{
		g_pUserInformation->GoreLevel = gore;
		for (int type : {EFFECTSPRITETYPE_TRANSFUSION_1, EFFECTSPRITETYPE_GREEN_TRANSFUSION_1})
		{
			ForEachFrame([&world, gore, type](int frame) {
				queued.clear();
				// At the target's first phase, still one step.
				auto target = Target(1);
				EFFECTGENERATOR_INFO info = Info(type);
				info.x1 = 600;
				info.y1 = 200;
				info.pEffectTarget = target.get();
				CHECK(world.ripple.Generate(info));
				target.release();
				CHECK_EQ(1, queued.size());
				if (queued.empty()) return;
				CHECK(queued[0].ground);
				CheckEffect(Effect(0), (gore ? EFFECTSPRITETYPE_TRANSFUSION_1 : EFFECTSPRITETYPE_GREEN_TRANSFUSION_1) + frame, 495, 235, 0);
			});
		}
	}
	// Already at the creature (their 24-pixel squares touch or overlap,
	// the effect's taken from half a square in): none.
	const struct { int x0, y0; bool at; } edges[] = {
		{552, 192, true}, {551, 192, false},	// the creature's left at the effect's right
		{588, 192, true}, {589, 192, false},	// its right at the effect's left
		{576, 168, true}, {576, 167, false},	// its top at the effect's bottom
		{576, 204, true}, {576, 205, false}};	// its bottom at the effect's top
	for (const auto& e : edges)
	{
		queued.clear();
		EFFECTGENERATOR_INFO touching = Info(EFFECTSPRITETYPE_TRANSFUSION_1);
		touching.x0 = e.x0;
		touching.y0 = e.y0;
		CHECK(world.ripple.Generate(touching) == !e.at);
		CHECK_EQ(e.at ? 0u : 1u, queued.size());
	}
	// No creature: the target's position is taken for its tile.
	creatures.clear();
	EFFECTGENERATOR_INFO missing = Info(EFFECTSPRITETYPE_TRANSFUSION_1);
	missing.x1 = 10;
	missing.y1 = 10;
	CHECK(!world.ripple.Generate(missing));
	missing.x1 = 20;
	CHECK(world.ripple.Generate(missing));
}

namespace {
bool GroundOnce(int spriteType)
{
	MRippleZonePixelEffectGenerator ripple;
	return ripple.Generate(Info(spriteType));
}
} // namespace
