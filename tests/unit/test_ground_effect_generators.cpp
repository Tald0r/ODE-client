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
int acceptLimit;
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
	return info;
}

void CheckEffect(MEffect& effect, int type, int x, int y, int z)
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
	CHECK_EQ(DIRECTION_RIGHT, effect.GetDirection());
	CHECK_EQ(77, effect.GetPower());
}

std::unique_ptr<MEffectTarget> Target(int phases = 0)
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
	info.pEffectTarget = target.get();
	CHECK(world.attach.Generate(info));
	target.release();
	CHECK_EQ(1, queued.size());
	CHECK(queued[0].ground);
	CheckEffect(Effect(0), 500, 480, 240, 7);
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
		for (unsigned seed : {3u, 4u, 5u})
		{
			std::srand(seed);
			const int frame = std::rand() % c.frames;
			const int dx = std::rand() % TILE_X_HALF - (TILE_X_HALF >> 1);
			const int dy = std::rand() % 24 - 12;
			std::srand(seed);
			queued.clear();
			CHECK(world.attach.Generate(Info(c.type)));
			CHECK_EQ(1, queued.size());
			CheckEffect(Effect(0), c.type + frame, 480 + dx, 240 + dy, 7);
		}
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
			else CHECK(Effect(i).GetEffectTarget() != nullptr && Effect(i).GetEffectTarget() != info.pEffectTarget);
		}
	}
	// Refused ones are gone; the kept ones are linked, the first to a copy
	// when the first was refused. Always true.
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
		if (i == 0) CHECK(Effect(i).GetEffectTarget() == info.pEffectTarget);
		else CHECK(Effect(i).GetEffectTarget() != nullptr && Effect(i).GetEffectTarget() != info.pEffectTarget);
	}
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
	info.pEffectTarget = target.get();
	CHECK(world.ripple.Generate(info));
	target.release();
	CHECK_EQ(1, queued.size());
	CHECK(!queued[0].ground);
	CheckEffect(Effect(0), 500, 576, 288, 0);
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

TEST(GroundEffectGenerators, AnEarthquakeStepsOnTheGroundFasterAtTheFirstPhase)
{
	World world;
	for (unsigned seed : {3u, 4u, 5u})
	{
		std::srand(seed);
		const int frame = std::rand() % 3;
		std::srand(seed);
		queued.clear();
		CHECK(world.ripple.Generate(Info(EFFECTSPRITETYPE_EARTHQUAKE_1)));
		CHECK(queued[0].ground);
		CheckEffect(Effect(0), EFFECTSPRITETYPE_EARTHQUAKE_1 + frame, 493, 246, 0);
	}
	// At the target's first phase, three steps; the target goes twice as far.
	queued.clear();
	auto target = Target(1);
	EFFECTGENERATOR_INFO info = Info(EFFECTSPRITETYPE_EARTHQUAKE_1);
	info.pEffectTarget = target.get();
	CHECK(world.ripple.Generate(info));
	target.release();
	CHECK(Effect(0).GetPixelX() == 520 && Effect(0).GetPixelY() == 260);
	CHECK(info.pEffectTarget->GetX() == 560 && info.pEffectTarget->GetY() == 280);
	// Straight down: the step heads a tile on in the direction instead.
	queued.clear();
	EFFECTGENERATOR_INFO down = Info(EFFECTSPRITETYPE_EARTHQUAKE_1);
	down.x1 = 480;
	down.y1 = 300;
	CHECK(world.ripple.Generate(down));
	CHECK(Effect(0).GetPixelX() == 500 && Effect(0).GetPixelY() == 240);
}

TEST(GroundEffectGenerators, PowerOfLandsStonesKeepTheirLineAndTheTarget)
{
	World world;
	for (unsigned seed : {3u, 4u, 5u})
	{
		std::srand(seed);
		const int frame = std::rand() % 3;
		std::srand(seed);
		queued.clear();
		auto target = Target(1);
		EFFECTGENERATOR_INFO info = Info(EFFECTSPRITETYPE_POWER_OF_LAND_STONE_1);
		info.x1 = 480;
		info.y1 = 300;
		info.pEffectTarget = target.get();
		CHECK(world.ripple.Generate(info));
		target.release();
		CHECK(queued[0].ground);
		CheckEffect(Effect(0), EFFECTSPRITETYPE_POWER_OF_LAND_STONE_2 + frame, 480, 250, 0);
		// The target stays where it was.
		CHECK(info.pEffectTarget->GetX() == 1 && info.pEffectTarget->GetY() == 2);
	}
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
			std::srand(7);
			const int frame = std::rand() % 3;
			std::srand(7);
			queued.clear();
			EFFECTGENERATOR_INFO info = Info(type);
			info.x1 = 600;
			info.y1 = 200;
			CHECK(world.ripple.Generate(info));
			CHECK(queued[0].ground);
			CheckEffect(Effect(0), (gore ? EFFECTSPRITETYPE_TRANSFUSION_1 : EFFECTSPRITETYPE_GREEN_TRANSFUSION_1) + frame, 495, 235, 0);
		}
	}
	// Already at the creature: none.
	queued.clear();
	EFFECTGENERATOR_INFO there = Info(EFFECTSPRITETYPE_TRANSFUSION_1);
	there.x0 = 570;
	there.y0 = 185;
	CHECK(!world.ripple.Generate(there));
	CHECK(queued.empty());
	// No creature: the target's position is taken for its tile.
	creatures.clear();
	EFFECTGENERATOR_INFO missing = Info(EFFECTSPRITETYPE_TRANSFUSION_1);
	missing.x1 = 10;
	missing.y1 = 10;
	CHECK(!world.ripple.Generate(missing));
	missing.x1 = 20;
	CHECK(world.ripple.Generate(missing));
}
