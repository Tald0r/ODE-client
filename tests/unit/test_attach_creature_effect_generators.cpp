#include "test_framework.h"
#include "MAttachCreatureEffectGenerator.h"
#include "MAttachCreatureEffectGroundGenerator.h"
#include "MAttachCreatureOrbitEffectGenerator.h"
#include "EffectSpriteTypeDef.h"
#include "MAttachOrbitEffect.h"
#include "MEffectTarget.h"
#include "MathTable.h"
#include "OrbitEffectPolicy.h"
#include "SkillDef.h"
#include "WorldTileGeometry.h"
#include "ZoneStateHost.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

// The generators that attach an effect to a creature: the creature in the
// zone, else a fake one (else, for the plain generator, the creature in a
// corpse item). The ground generator lays it on the ground (Sword Wave 1
// turned one step back); the orbit generator keeps a summoned elemental's
// orbit step from the effect already on it (or picks one at random),
// clears the creature's effects first for an elemental's attack or heal,
// and stops those two from orbiting; the plain generator raises a water
// emission by its step, picks a golemer's, alcan's or bat's Jabbing Vein,
// a random bomb or stone skin frame, a Distance Blitz frame by the tiles
// covered, and turns Piercing round. Each effect takes the direction, the
// action and target it follows, and the power. None for no creature or no
// effect.
namespace {

struct Tables
{
	std::array<int, MathTable::MAX_ANGLE> sine, cosine;
	std::array<int, MathTable::MAX_ANGLE + 1> tangent;
	Tables()
	{
		std::copy_n(MathTable::FSinTab, sine.size(), sine.begin());
		std::copy_n(MathTable::FCosTab, cosine.size(), cosine.begin());
		std::copy_n(MathTable::FArcTanTab, tangent.size(), tangent.begin());
		if (MathTable::FCosTab[0] != 65536) MathTable::FCreateSines();
		MAttachOrbitEffect::InitOrbitPosition();
	}
	~Tables()
	{
		std::copy(sine.begin(), sine.end(), MathTable::FSinTab);
		std::copy(cosine.begin(), cosine.end(), MathTable::FCosTab);
		std::copy(tangent.begin(), tangent.end(), MathTable::FArcTanTab);
	}
};

int creatureA, creatureB, corpseCreature;
MCreature* const pA = reinterpret_cast<MCreature*>(&creatureA);
MCreature* const pB = reinterpret_cast<MCreature*>(&creatureB);
MCreature* const pInCorpse = reinterpret_cast<MCreature*>(&corpseCreature);
std::map<TYPE_OBJECTID, MCreature*> zoneCreatures, fakeCreatures, corpses;
std::map<TYPE_OBJECTID, MItem*> items;
std::map<MCreature*, int> creatureTypes;
std::map<MCreature*, std::vector<MAttachEffect*>> attached;
std::vector<std::unique_ptr<MAttachEffect>> created;
std::vector<std::string> calls;
bool refuse = false;
DWORD frameNow = 100;
int itemDummy;

std::string N(long long value) { return std::to_string(value); }
std::string Who(MCreature* p) { return p == pA ? "A" : p == pB ? "B" : p == pInCorpse ? "corpse's" : p == nullptr ? "none" : "other"; }

const MEffectHost effectHost{
	.CurrentFrame = []() { return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE) { return 0; },
};
const MAttachEffectHost attachHost{
	.Sprite = [](TYPE_EFFECTSPRITETYPE, MAttachEffectSprite& sprite) { sprite = {BLT_EFFECT, 12, 3}; return true; },
	.LiveCreature = [](TYPE_OBJECTID id, MAttachCreaturePosition& position) { position = {id, 96, 48, 12}; return true; },
};

std::vector<int> removedTargets;
const MEffectTargetHost targetHost{.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); }};

// A target an effect will own once it is linked.
MEffectTarget* NewTarget()
{
	auto* pTarget = new MEffectTarget(3);
	pTarget->Set(777, 888, 999, 456);
	return pTarget;
}

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.GetCreature = [](TYPE_OBJECTID id) { auto it = zoneCreatures.find(id); return it == zoneCreatures.end() ? nullptr : it->second; };
	host.GetFakeCreature = [](TYPE_OBJECTID id) { auto it = fakeCreatures.find(id); return it == fakeCreatures.end() ? nullptr : it->second; };
	host.GetItem = [](TYPE_OBJECTID id) { auto it = items.find(id); return it == items.end() ? nullptr : it->second; };
	host.GetCorpseCreature = [](TYPE_OBJECTID id) { auto it = corpses.find(id); return it == corpses.end() ? nullptr : it->second; };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetCreatureType = [](MCreature* p) { return creatureTypes[p]; };
	host.CreateAttachEffect = [](MCreature* p, int type, std::uint32_t delay, std::uint32_t link, bool ground, int effectType) -> MAttachEffect* {
		calls.push_back("create|" + Who(p) + "|" + N(type) + "|" + N(delay) + "|" + N(link) + "|" + (ground ? "ground" : "body") + "|" + N(effectType));
		if (refuse) return nullptr;
		MAttachEffect* pEffect = effectType == MEffect::EFFECT_ATTACH_ORBIT
			? new MAttachOrbitEffect(static_cast<TYPE_EFFECTSPRITETYPE>(type), delay, 0, 0, link)
			: new MAttachEffect(static_cast<TYPE_EFFECTSPRITETYPE>(type), delay, link);
		created.emplace_back(pEffect);
		attached[p].push_back(pEffect);
		return pEffect;
	};
	host.IsExistAttachEffect = [](MCreature* p) { return !attached[p].empty(); };
	// Asked only of a creature that has an effect; none otherwise.
	host.FirstAttachEffect = [](MCreature* p) -> MAttachEffect* {
		calls.push_back("first|" + Who(p));
		return attached[p].empty() ? nullptr : attached[p].front();
	};
	host.ClearAttachEffect = [](MCreature* p) { calls.push_back("clear|" + Who(p)); attached[p].clear(); };
	return host;
}();

struct World
{
	Tables tables;
	const MEffectHost* oldEffect = MEffect::SetHost(&effectHost);
	const MAttachEffectHost* oldAttach = MAttachEffect::SetHost(&attachHost);
	const MEffectTargetHost* oldTargetHost = MEffectTarget::SetHost(&targetHost);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	MAttachCreatureEffectGroundGenerator ground;
	MAttachCreatureOrbitEffectGenerator orbit;
	MAttachCreatureEffectGenerator plain;
	World()
	{
		zoneCreatures = {{1001, pA}};
		fakeCreatures = {{2001, pB}};
		items.clear();
		corpses.clear();
		creatureTypes.clear();
		attached.clear();
		created.clear();
		calls.clear();
		refuse = false;
		frameNow = 100;
	}
	~World()
	{
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		attached.clear();
		created.clear();
		MEffectTarget::SetHost(oldTargetHost);
		MAttachEffect::SetHost(oldAttach);
		MEffect::SetHost(oldEffect);
	}
};

EFFECTGENERATOR_INFO Info(TYPE_OBJECTID id = 1001, int spriteType = 17)
{
	EFFECTGENERATOR_INFO info{};
	info.nActionInfo = 42;
	info.pEffectTarget = nullptr;
	info.effectSpriteType = static_cast<TYPE_EFFECTSPRITETYPE>(spriteType);
	info.creatureID = id;
	info.direction = 3;
	info.step = 9;
	info.count = 30;
	info.linkCount = 5;
	info.power = 77;
	return info;
}

std::string Created(MCreature* p, int type, bool ground, int effectType)
{
	return "create|" + Who(p) + "|" + N(type) + "|30|5|" + (ground ? "ground" : "body") + "|" + N(effectType);
}

// A seed whose first draw picks an orbit step that is neither 0 nor the
// one given: some C libraries' first draws for small seeds are small.
unsigned SeedFor(int avoid)
{
	for (unsigned seed = 1;; seed++)
	{
		std::srand(seed);
		const int step = RandomIndex(std::rand(), MAX_EFFECT_ORBIT_STEP);
		if (step != 0 && step != avoid) return seed;
	}
}

void CheckEffect(const MAttachEffect& effect, int type, int direction)
{
	CHECK_EQ(type, effect.GetEffectSpriteType());
	CHECK_EQ(direction, effect.GetDirection());
	CHECK_EQ(42, effect.GetActionInfo());
	CHECK_EQ(77, effect.GetPower());
}

} // namespace

TEST(AttachCreatureEffectGenerators, TheGroundOneLaysTheEffectUnderTheZonesOrAFakeCreature)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_ATTACH_CREATURE_GROUND, world.ground.GetID());
	EFFECTGENERATOR_INFO info = Info();
	info.pEffectTarget = NewTarget();
	CHECK(world.ground.Generate(info));
	CHECK(calls == std::vector<std::string>{Created(pA, 17, true, MEffect::EFFECT_ATTACH)});
	CheckEffect(*created.back(), 17, 3);
	CHECK(created.back()->GetEffectTarget() == info.pEffectTarget);
	calls.clear();
	CHECK(world.ground.Generate(Info(2001)));
	CHECK(calls == std::vector<std::string>{Created(pB, 17, true, MEffect::EFFECT_ATTACH)});
	// Sword Wave 1 turns one step back, round past 0.
	EFFECTGENERATOR_INFO wave = Info(1001, EFFECTSPRITETYPE_SWORD_WAVE_1);
	CHECK(world.ground.Generate(wave));
	CheckEffect(*created.back(), EFFECTSPRITETYPE_SWORD_WAVE_1, 2);
	wave.direction = 0;
	CHECK(world.ground.Generate(wave));
	CHECK_EQ(7, created.back()->GetDirection());
	// No creature, or no effect: none.
	calls.clear();
	CHECK(!world.ground.Generate(Info(3001)));
	CHECK(calls.empty());
	refuse = true;
	CHECK(!world.ground.Generate(Info()));
}

TEST(AttachCreatureEffectGenerators, TheOrbitOnePicksItsStepAtRandomOrFromTheElementalsLast)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_ATTACH_CREATURE_ORBIT, world.orbit.GetID());
	const unsigned seed = SeedFor(-1);
	std::srand(seed);
	const int step = RandomIndex(std::rand(), MAX_EFFECT_ORBIT_STEP);
	std::srand(seed);
	EFFECTGENERATOR_INFO info = Info();
	info.pEffectTarget = NewTarget();
	CHECK(world.orbit.Generate(info));
	CHECK(calls == std::vector<std::string>{Created(pA, 17, false, MEffect::EFFECT_ATTACH_ORBIT)});
	MAttachOrbitEffect& first = static_cast<MAttachOrbitEffect&>(*created.back());
	CheckEffect(first, 17, 3);
	CHECK(first.GetEffectTarget() == info.pEffectTarget);
	CHECK_EQ(step, first.GetOrbitStep());
	// A summoned elemental follows the orbit effect already on the creature.
	first.SetOrbitStep(5);
	for (int type : {EFFECTSPRITETYPE_SUMMON_FIRE_ELEMENTAL, EFFECTSPRITETYPE_SUMMON_WATER_ELEMENTAL})
	{
		CHECK(world.orbit.Generate(Info(1001, type)));
		CHECK_EQ(5, static_cast<MAttachOrbitEffect&>(*created.back()).GetOrbitStep());
	}
	// The first effect not an orbit: at random.
	attached[pB].push_back(new MAttachEffect(17, 30, 5));
	created.emplace_back(attached[pB].back());
	const unsigned otherSeed = SeedFor(5);
	std::srand(otherSeed);
	const int other = RandomIndex(std::rand(), MAX_EFFECT_ORBIT_STEP);
	std::srand(otherSeed);
	CHECK(world.orbit.Generate(Info(2001, EFFECTSPRITETYPE_SUMMON_FIRE_ELEMENTAL)));
	CHECK_EQ(other, static_cast<MAttachOrbitEffect&>(*created.back()).GetOrbitStep());
	// An elemental on a creature with nothing attached: at random, with
	// nothing asked of its effects.
	attached[pB].clear();
	calls.clear();
	const unsigned bareSeed = SeedFor(-1);
	std::srand(bareSeed);
	const int bare = RandomIndex(std::rand(), MAX_EFFECT_ORBIT_STEP);
	std::srand(bareSeed);
	CHECK(world.orbit.Generate(Info(2001, EFFECTSPRITETYPE_SUMMON_WATER_ELEMENTAL)));
	CHECK_EQ(bare, static_cast<MAttachOrbitEffect&>(*created.back()).GetOrbitStep());
	CHECK(calls == std::vector<std::string>{Created(pB, EFFECTSPRITETYPE_SUMMON_WATER_ELEMENTAL, false, MEffect::EFFECT_ATTACH_ORBIT)});
	// Another sprite type ignores the step on the creature.
	const unsigned thirdSeed = SeedFor(5);
	std::srand(thirdSeed);
	const int third = RandomIndex(std::rand(), MAX_EFFECT_ORBIT_STEP);
	std::srand(thirdSeed);
	CHECK(world.orbit.Generate(Info(1001, 17)));
	CHECK_EQ(third, static_cast<MAttachOrbitEffect&>(*created.back()).GetOrbitStep());
}

TEST(AttachCreatureEffectGenerators, AnElementalsAttackOrHealClearsTheCreatureAndStandsStill)
{
	World world;
	CHECK(world.orbit.Generate(Info()));
	static_cast<MAttachOrbitEffect&>(*created.back()).SetOrbitStep(6);
	for (int type : {EFFECTSPRITETYPE_SUMMON_FIRE_ELEMENTAL_ATTACK, EFFECTSPRITETYPE_SUMMON_WATER_ELEMENTAL_HEAL})
	{
		calls.clear();
		attached[pA] = {created.front().get()};
		CHECK(world.orbit.Generate(Info(1001, type)));
		CHECK(calls == (std::vector<std::string>{"first|A", "clear|A", Created(pA, type, false, MEffect::EFFECT_ATTACH_ORBIT)}));
		MAttachOrbitEffect& effect = static_cast<MAttachOrbitEffect&>(*created.back());
		// The step is the last one's, and the orbit holds still.
		CHECK_EQ(6, effect.GetOrbitStep());
		effect.Update();
		CHECK_EQ(6, effect.GetOrbitStep());
	}
	// Others keep orbiting.
	CHECK(world.orbit.Generate(Info(1001, EFFECTSPRITETYPE_SUMMON_FIRE_ELEMENTAL)));
	MAttachOrbitEffect& moving = static_cast<MAttachOrbitEffect&>(*created.back());
	const int before = moving.GetOrbitStep();
	moving.Update();
	CHECK_EQ((before + 1) % MAX_EFFECT_ORBIT_STEP, moving.GetOrbitStep());
	// No creature, or no effect: none.
	calls.clear();
	CHECK(!world.orbit.Generate(Info(3001)));
	CHECK(calls.empty());
	refuse = true;
	CHECK(!world.orbit.Generate(Info()));
}

TEST(AttachCreatureEffectGenerators, ThePlainOneFindsTheCreatureInTheZoneAFakeOrACorpse)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_ATTACH_CREATURE, world.plain.GetID());
	EFFECTGENERATOR_INFO info = Info();
	info.pEffectTarget = NewTarget();
	CHECK(world.plain.Generate(info));
	CHECK(created.back()->GetEffectTarget() == info.pEffectTarget);
	CHECK(world.plain.Generate(Info(2001)));
	items = {{3001, reinterpret_cast<MItem*>(&itemDummy)}};
	corpses = {{3001, pInCorpse}};
	CHECK(world.plain.Generate(Info(3001)));
	CHECK(calls == (std::vector<std::string>{Created(pA, 17, false, MEffect::EFFECT_ATTACH), Created(pB, 17, false, MEffect::EFFECT_ATTACH),
		Created(pInCorpse, 17, false, MEffect::EFFECT_ATTACH)}));
	CheckEffect(*created.back(), 17, 3);
	CHECK_EQ(0, created.back()->GetPixelZ());
	// No creature and no item: none.
	calls.clear();
	CHECK(!world.plain.Generate(Info(4001)));
	CHECK(calls.empty());
	refuse = true;
	CHECK(!world.plain.Generate(Info()));
}

TEST(AttachCreatureEffectGenerators, AWaterEmissionRisesByItsStep)
{
	World world;
	for (int action : {SKILL_EMISSION_WATER, STEP_SKILL_EMISSION_WATER_2, STEP_SKILL_EMISSION_WATER_3})
	{
		EFFECTGENERATOR_INFO info = Info();
		info.nActionInfo = static_cast<TYPE_ACTIONINFO>(action);
		CHECK(world.plain.Generate(info));
		CHECK_EQ(9, created.back()->GetPixelZ());
	}
	CHECK(world.plain.Generate(Info()));
	CHECK_EQ(0, created.back()->GetPixelZ());
}

TEST(AttachCreatureEffectGenerators, JabbingVeinFitsGolemersAlcansAndBats)
{
	World world;
	const struct { int creatureType; int front; int rear; } cases[] = {
		{104, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_GOLEMER, EFFECTSPRITETYPE_JABBING_VEIN_REAR_GOLEMER},
		{111, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_GOLEMER, EFFECTSPRITETYPE_JABBING_VEIN_REAR_GOLEMER},
		{119, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_GOLEMER, EFFECTSPRITETYPE_JABBING_VEIN_REAR_GOLEMER},
		{358, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_GOLEMER, EFFECTSPRITETYPE_JABBING_VEIN_REAR_GOLEMER},
		{359, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_GOLEMER, EFFECTSPRITETYPE_JABBING_VEIN_REAR_GOLEMER},
		{467, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_GOLEMER, EFFECTSPRITETYPE_JABBING_VEIN_REAR_GOLEMER},
		{582, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_GOLEMER, EFFECTSPRITETYPE_JABBING_VEIN_REAR_GOLEMER},
		{106, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_ALCAN, EFFECTSPRITETYPE_JABBING_VEIN_REAR_ALCAN},
		{129, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_ALCAN, EFFECTSPRITETYPE_JABBING_VEIN_REAR_ALCAN},
		{137, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_ALCAN, EFFECTSPRITETYPE_JABBING_VEIN_REAR_ALCAN},
		{103, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_3, EFFECTSPRITETYPE_JABBING_VEIN_REAR_3},
		{105, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_3, EFFECTSPRITETYPE_JABBING_VEIN_REAR_3},
		{107, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_3, EFFECTSPRITETYPE_JABBING_VEIN_REAR_3},
		{466, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_3, EFFECTSPRITETYPE_JABBING_VEIN_REAR_3},
		{468, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_3, EFFECTSPRITETYPE_JABBING_VEIN_REAR_3},
		{357, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_3, EFFECTSPRITETYPE_JABBING_VEIN_REAR_3},
		{360, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_3, EFFECTSPRITETYPE_JABBING_VEIN_REAR_3},
		{581, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_3, EFFECTSPRITETYPE_JABBING_VEIN_REAR_3},
		{583, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_3, EFFECTSPRITETYPE_JABBING_VEIN_REAR_3},
		{110, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_3, EFFECTSPRITETYPE_JABBING_VEIN_REAR_3},
		{120, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_3, EFFECTSPRITETYPE_JABBING_VEIN_REAR_3},
		{128, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_3, EFFECTSPRITETYPE_JABBING_VEIN_REAR_3},
		{138, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_3, EFFECTSPRITETYPE_JABBING_VEIN_REAR_3},
		{185, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_BAT, EFFECTSPRITETYPE_JABBING_VEIN_REAR_BAT}};
	for (const auto& c : cases)
	{
		creatureTypes[pA] = c.creatureType;
		CHECK(world.plain.Generate(Info(1001, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_3)));
		CHECK_EQ(c.front, created.back()->GetEffectSpriteType());
		CHECK(world.plain.Generate(Info(1001, EFFECTSPRITETYPE_JABBING_VEIN_REAR_3)));
		CHECK_EQ(c.rear, created.back()->GetEffectSpriteType());
	}
	// A bat's every Jabbing Vein frame is the bat's; another type is let be.
	creatureTypes[pA] = 185;
	for (int frame : {EFFECTSPRITETYPE_JABBING_VEIN_FRONT_1, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_BAT})
	{
		CHECK(world.plain.Generate(Info(1001, frame)));
		CHECK_EQ(EFFECTSPRITETYPE_JABBING_VEIN_FRONT_BAT, created.back()->GetEffectSpriteType());
	}
	for (int frame : {EFFECTSPRITETYPE_JABBING_VEIN_REAR_1, EFFECTSPRITETYPE_JABBING_VEIN_REAR_BAT})
	{
		CHECK(world.plain.Generate(Info(1001, frame)));
		CHECK_EQ(EFFECTSPRITETYPE_JABBING_VEIN_REAR_BAT, created.back()->GetEffectSpriteType());
	}
	CHECK(world.plain.Generate(Info(1001, 17)));
	CHECK_EQ(17, created.back()->GetEffectSpriteType());
	creatureTypes[pA] = 184;
	CHECK(world.plain.Generate(Info(1001, EFFECTSPRITETYPE_JABBING_VEIN_FRONT_1)));
	CHECK_EQ(EFFECTSPRITETYPE_JABBING_VEIN_FRONT_1, created.back()->GetEffectSpriteType());
}

TEST(AttachCreatureEffectGenerators, BombsAndStoneSkinPickARandomFrame)
{
	World world;
	const struct { int type; int frames; } cases[] = {
		{EFFECTSPRITETYPE_TRIDENT_BOMB_1, 10}, {EFFECTSPRITETYPE_QUICK_FIRE_BOMB_1, 5}, {EFFECTSPRITETYPE_STONE_SKIN_DAMAGED_1, 3}};
	for (const auto& c : cases)
	{
		for (unsigned seed : {5u, 6u, 7u})
		{
			std::srand(seed);
			const int expected = c.type + std::rand() % c.frames;
			std::srand(seed);
			CHECK(world.plain.Generate(Info(1001, c.type)));
			CHECK_EQ(expected, created.back()->GetEffectSpriteType());
		}
	}
}

TEST(AttachCreatureEffectGenerators, PiercingTurnsRound)
{
	World world;
	for (BYTE direction : {BYTE(0), BYTE(3), BYTE(5)})
	{
		EFFECTGENERATOR_INFO info = Info(1001, EFFECTSPRITETYPE_PIERCING_FRONT);
		info.direction = direction;
		CHECK(world.plain.Generate(info));
		CHECK_EQ((direction + 4) % 8, created.back()->GetDirection());
	}
}

TEST(AttachCreatureEffectGenerators, DistanceBlitzCountsTheTilesCovered)
{
	World world;
	// From 0 to 6 tiles, along X or Y: 2 tiles or fewer is the first
	// frame, 6 or more the fifth.
	const struct { int dx, dy, frame; } cases[] = {{0, 0, 0}, {2, 0, 0}, {3, 0, 1}, {0, 4, 2}, {0, -4, 2}, {5, 1, 3}, {6, 0, 4}, {9, 9, 4}};
	for (const auto& c : cases)
	{
		EFFECTGENERATOR_INFO info = Info(1001, EFFECTSPRITETYPE_DISTANCE_BLITZ_12);
		info.x0 = WorldTileGeometry::TileToPixelX(10);
		info.y0 = WorldTileGeometry::TileToPixelY(20);
		info.x1 = WorldTileGeometry::TileToPixelX(10 + c.dx);
		info.y1 = WorldTileGeometry::TileToPixelY(20 + c.dy);
		CHECK(world.plain.Generate(info));
		CHECK_EQ(EFFECTSPRITETYPE_DISTANCE_BLITZ_12 + c.frame, created.back()->GetEffectSpriteType());
	}
	// Backwards counts the same.
	EFFECTGENERATOR_INFO back = Info(1001, EFFECTSPRITETYPE_DISTANCE_BLITZ_12);
	back.x0 = WorldTileGeometry::TileToPixelX(10);
	back.y0 = WorldTileGeometry::TileToPixelY(20);
	back.x1 = WorldTileGeometry::TileToPixelX(6);
	back.y1 = WorldTileGeometry::TileToPixelY(20);
	CHECK(world.plain.Generate(back));
	CHECK_EQ(EFFECTSPRITETYPE_DISTANCE_BLITZ_12 + 2, created.back()->GetEffectSpriteType());
}
