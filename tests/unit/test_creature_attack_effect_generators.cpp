#include "test_framework.h"
#include "MAttackCreatureEffectGenerator.h"
#include "MAttackCreatureHomingEffectGenerator.h"
#include "MEffectTarget.h"
#include "MGuidanceEffect.h"
#include "MHomingEffect.h"
#include "MViewDef.h"
#include "MathTable.h"
#include "SkillDef.h"
#include "WorldTileGeometry.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <vector>

// The guided and homing attacks on a creature. Both read the sprite, the
// creature and the frames, and none is made without them. The guided one
// (MAttackCreatureEffectGenerator) traces the creature from the start, as
// does a second and third arrow, bolt or tooth at their skill's steps 2
// and 3, set 12 pixels to either side (across for the directions 2 and 6,
// up and down otherwise), each with its own copy of the target. The
// homing one (MAttackCreatureHomingEffectGenerator) starts a tile above
// its point and heads for a tile above the creature, starting at the
// angle of its direction and turning 13 degrees a step; a halo attack
// turns 30 (45 across, lasting 8 frames) and starts two directions to
// one side or the other at random. Each effect takes the direction, the
// steps and frames, the power and, once the zone keeps it, the action and
// target it follows.
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
	}
	~Tables()
	{
		std::copy(sine.begin(), sine.end(), MathTable::FSinTab);
		std::copy(cosine.begin(), cosine.end(), MathTable::FCosTab);
		std::copy(tangent.begin(), tangent.end(), MathTable::FArcTanTab);
	}
};

DWORD frameNow;
MCreatureParabolaEffectSprite sprite;
MCreatureParabolaPosition creature;
bool spriteAvailable, creatureAvailable, framesAvailable;
int maxFrames, acceptLimit;
std::vector<int> calls;
std::vector<std::unique_ptr<MEffect>> effects;
std::vector<int> removedTargets;

const MEffectHost effectHost{
	.CurrentFrame = []() { return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE) { return 0; },
};
const MEffectTargetHost targetHost{.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); }};
// The creature, for the effects' own tracing.
const MGuidanceEffectHost guidanceHost{
	.CreaturePosition = [](TYPE_OBJECTID, int& x, int& y, int& z) {
		x = WorldTileGeometry::TileToPixelX(creature.x);
		y = WorldTileGeometry::TileToPixelY(creature.y);
		z = creature.z;
		return true;
	},
};
const MCreatureParabolaEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MCreatureParabolaEffectSprite& result) {
		calls.push_back(1);
		CHECK_EQ(17, type);
		result = sprite;
		return spriteAvailable;
	},
	.Creature = [](TYPE_OBJECTID id, MCreatureParabolaPosition& position) {
		calls.push_back(2);
		CHECK_EQ(123, id);
		position = creature;
		return creatureAvailable;
	},
	.MaxFrames = [](BYTE blt, TYPE_FRAMEID frameID, int& count) {
		calls.push_back(3);
		CHECK_EQ(sprite.bltType, blt);
		CHECK_EQ(sprite.frameID, frameID);
		count = maxFrames;
		return framesAvailable;
	},
	.Queue = [](std::unique_ptr<MEffect> effect) {
		calls.push_back(4);
		// The action and target come only after the zone keeps the effect.
		CHECK(effect->GetEffectTarget() == nullptr);
		if (static_cast<int>(effects.size()) >= acceptLimit) return false;
		effects.push_back(std::move(effect));
		return true;
	},
};

struct World
{
	Tables tables;
	const MEffectHost* oldEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* oldTarget = MEffectTarget::SetHost(&targetHost);
	const MGuidanceEffectHost* oldGuidance = MGuidanceEffect::SetHost(&guidanceHost);
	const MCreatureParabolaEffectHost* oldGuided = MAttackCreatureEffectGenerator::SetHost(&host);
	const MCreatureParabolaEffectHost* oldHoming = MAttackCreatureHomingEffectGenerator::SetHost(&host);
	MAttackCreatureEffectGenerator guided;
	MAttackCreatureHomingEffectGenerator homing;
	World()
	{
		frameNow = 100;
		sprite = {BLT_EFFECT, 12};
		creature = {10, 20, 6};
		spriteAvailable = creatureAvailable = framesAvailable = true;
		maxFrames = 3;
		acceptLimit = 99;
		calls.clear();
		effects.clear();
		removedTargets.clear();
	}
	~World()
	{
		effects.clear();
		MAttackCreatureHomingEffectGenerator::SetHost(oldHoming);
		MAttackCreatureEffectGenerator::SetHost(oldGuided);
		MGuidanceEffect::SetHost(oldGuidance);
		MEffectTarget::SetHost(oldTarget);
		MEffect::SetHost(oldEffect);
	}
};

EFFECTGENERATOR_INFO Info(TYPE_ACTIONINFO action = 42, BYTE direction = DIRECTION_LEFTUP)
{
	EFFECTGENERATOR_INFO info{};
	info.nActionInfo = action;
	info.effectSpriteType = 17;
	info.x0 = 300;
	info.y0 = 400;
	info.z0 = 50;
	info.creatureID = 123;
	info.direction = direction;
	info.step = 16;
	info.count = 30;
	info.linkCount = 5;
	info.power = 77;
	return info;
}

std::unique_ptr<MEffectTarget> Target(BYTE id = 73)
{
	auto target = std::make_unique<MEffectTarget>(3);
	target->NextPhase();
	target->m_EffectID = id;
	target->Set(777, 888, 999, 456);
	target->SetServerID(789);
	return target;
}

// The direction is checked apart: a guided effect turns to its creature.
void CheckCommon(MEffect& effect, int x, int y, int z, DWORD count)
{
	CHECK_EQ(BLT_EFFECT, effect.GetBltType());
	CHECK_EQ(12, effect.GetFrameID());
	CHECK_EQ(3, effect.GetMaxFrame());
	CHECK_EQ(x, effect.GetPixelX());
	CHECK_EQ(y, effect.GetPixelY());
	CHECK_EQ(z, effect.GetPixelZ());
	CHECK_EQ(16, effect.GetStepPixel());
	// SetCount ends a frame short of now + count.
	CHECK_EQ(frameNow + count - 1, effect.GetEndFrame());
	CHECK_EQ(frameNow + 5 - 1, effect.GetEndLinkFrame());
	CHECK_EQ(77, effect.GetPower());
}

// Two effects fly alike for eight updates (long enough for a start a
// degree off to show).
void CheckFlight(MEffect& effect, MEffect& twin)
{
	CHECK_EQ(twin.GetDirection(), effect.GetDirection());
	for (int i = 0; i < 8; i++)
	{
		const bool a = effect.Update();
		const bool b = twin.Update();
		CHECK(a == b);
		CHECK_EQ(twin.GetPixelX(), effect.GetPixelX());
		CHECK_EQ(twin.GetPixelY(), effect.GetPixelY());
		CHECK_EQ(twin.GetPixelZ(), effect.GetPixelZ());
		CHECK_EQ(twin.GetDirection(), effect.GetDirection());
	}
}

// A guided effect set as the generator should set it.
std::unique_ptr<MGuidanceEffect> GuidedTwin(const EFFECTGENERATOR_INFO& info, int x, int y, int z)
{
	auto twin = std::make_unique<MGuidanceEffect>(BLT_EFFECT);
	twin->SetFrameID(12, 3);
	twin->SetPixelPosition(x, y, z);
	twin->SetDirection(info.direction);
	twin->SetTraceCreatureID(info.creatureID);
	twin->SetStepPixel(info.step);
	twin->SetCount(info.count, info.linkCount);
	twin->SetPower(info.power);
	return twin;
}

} // namespace

TEST(CreatureAttackEffectGenerators, AGuidedAttackTracesTheCreature)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_ATTACK_CREATURE, world.guided.GetID());
	auto target = Target();
	EFFECTGENERATOR_INFO info = Info();
	info.pEffectTarget = target.get();
	CHECK(world.guided.Generate(info));
	target.release();
	CHECK(calls == (std::vector<int>{1, 2, 3, 4}));
	CHECK_EQ(1, effects.size());
	MGuidanceEffect& effect = static_cast<MGuidanceEffect&>(*effects.front());
	CHECK_EQ(MEffect::EFFECT_GUIDANCE, effect.GetEffectType());
	CheckCommon(effect, 300, 400, 50, 30);
	CHECK_EQ(123, effect.GetTraceCreatureID());
	CHECK_EQ(42, effect.GetActionInfo());
	CHECK(effect.GetEffectTarget() == info.pEffectTarget);
	CHECK(!effect.IsMulti());
	auto twin = GuidedTwin(info, 300, 400, 50);
	CheckFlight(effect, *twin);
}

TEST(CreatureAttackEffectGenerators, AGuidedAttackNeedsTheSpriteTheCreatureTheFramesAndTheZone)
{
	World world;
	for (int missing = 0; missing < 4; missing++)
	{
		calls.clear();
		effects.clear();
		spriteAvailable = missing != 0;
		creatureAvailable = missing != 1;
		framesAvailable = missing != 2;
		acceptLimit = missing != 3 ? 99 : 0;
		auto target = Target();
		EFFECTGENERATOR_INFO info = Info();
		info.pEffectTarget = target.get();
		CHECK(!world.guided.Generate(info));
		// The caller keeps the target.
		CHECK(effects.empty());
		CHECK_EQ(missing + 1, static_cast<int>(calls.size()));
	}
	// Without a host: none.
	MAttackCreatureEffectGenerator::SetHost(nullptr);
	CHECK(!world.guided.Generate(Info()));
}

TEST(CreatureAttackEffectGenerators, LaterStepsOfArrowsBoltsAndTeethShootBesideTheFirst)
{
	World world;
	const struct { TYPE_ACTIONINFO action; int extra; } steps[] = {
		{STEP_SKILL_KASAS_ARROW_2, 1}, {STEP_SKILL_BLAZE_BOLT_2, 1}, {STEP_SKILL_EARTHS_TEETH_2, 1},
		{STEP_SKILL_KASAS_ARROW_3, 2}, {STEP_SKILL_BLAZE_BOLT_3, 2}, {STEP_SKILL_EARTHS_TEETH_3, 2}};
	for (const auto& s : steps)
	{
		for (BYTE direction : {BYTE(2), BYTE(6), BYTE(DIRECTION_LEFTUP)})
		{
			effects.clear();
			auto target = Target();
			EFFECTGENERATOR_INFO info = Info(s.action, direction);
			info.pEffectTarget = target.get();
			CHECK(world.guided.Generate(info));
			target.release();
			CHECK_EQ(1 + s.extra, static_cast<int>(effects.size()));
			CheckCommon(*effects[0], 300, 400, 50, 30);
			CHECK(effects[0]->GetEffectTarget() == info.pEffectTarget);
			for (int i = 1; i <= s.extra; i++)
			{
				const int z = -12 + 24 * (i - 1);
				MGuidanceEffect& extra = static_cast<MGuidanceEffect&>(*effects[i]);
				const int x = direction == 2 || direction == 6 ? 300 + z : 300;
				const int height = direction == 2 || direction == 6 ? 50 : 50 - z;
				CheckCommon(extra, x, 400, height, 30);
				auto twin = GuidedTwin(info, x, 400, height);
				CHECK_EQ(twin->GetDirection(), extra.GetDirection());
				CHECK_EQ(123, extra.GetTraceCreatureID());
				CHECK_EQ(s.action, extra.GetActionInfo());
				// Its own copy of the target.
				MEffectTarget* pCopy = extra.GetEffectTarget();
				CHECK(pCopy != nullptr && pCopy != info.pEffectTarget);
				// A copy keeps the place and effect ID, and drops the server's ID.
				CHECK(pCopy != nullptr && pCopy->GetX() == 777 && pCopy->GetY() == 888 && pCopy->GetID() == 456
					&& pCopy->m_EffectID == 73 && pCopy->GetServerID() == OBJECTID_NULL);
			}
		}
	}
	// No target: the extra ones follow none, nor any action.
	effects.clear();
	CHECK(world.guided.Generate(Info(STEP_SKILL_BLAZE_BOLT_3)));
	CHECK_EQ(3, effects.size());
	CHECK(effects[1]->GetEffectTarget() == nullptr && effects[2]->GetEffectTarget() == nullptr);
	CHECK_EQ(ACTIONINFO_NULL, effects[2]->GetActionInfo());
	// The zone refusing a later one: true, the earlier ones kept and the
	// first still owning the target. (A false would have the caller
	// delete the target the first effect owns.)
	effects.clear();
	acceptLimit = 2;
	auto target = Target();
	EFFECTGENERATOR_INFO refused = Info(STEP_SKILL_BLAZE_BOLT_3);
	refused.pEffectTarget = target.get();
	CHECK(world.guided.Generate(refused));
	target.release();
	CHECK_EQ(2, effects.size());
	CHECK(effects[0]->GetEffectTarget() == refused.pEffectTarget);
}

namespace {

// A homing effect set as the generator should set it, to compare their
// flight.
std::unique_ptr<MHomingEffect> Twin(const EFFECTGENERATOR_INFO& info, int angle, int turn, DWORD count)
{
	auto twin = std::make_unique<MHomingEffect>(BLT_EFFECT, angle, turn);
	twin->SetFrameID(12, 3);
	twin->SetPixelPosition(info.x0, info.y0, info.z0 + TILE_Y);
	twin->SetDirection(info.direction);
	twin->SetTraceCreatureID(info.creatureID);
	twin->SetTarget(WorldTileGeometry::TileToPixelX(creature.x), WorldTileGeometry::TileToPixelY(creature.y), creature.z + TILE_Y, info.step);
	twin->SetCount(count, info.linkCount);
	twin->SetPower(info.power);
	twin->SetMulti(true);
	twin->SetLink(info.nActionInfo, nullptr);
	return twin;
}

constexpr int kAngles[8] = {180, 225, 270, 315, 0, 45, 90, 135};

} // namespace

TEST(CreatureAttackEffectGenerators, AHomingAttackStartsAtItsDirectionsAngleAndTurnsTowardTheCreature)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_ATTACK_CREATURE_HOMING, world.homing.GetID());
	for (BYTE direction = 0; direction < 8; direction++)
	{
		effects.clear();
		calls.clear();
		auto target = Target();
		EFFECTGENERATOR_INFO info = Info(42, direction);
		info.pEffectTarget = target.get();
		CHECK(world.homing.Generate(info));
		target.release();
		CHECK(calls == (std::vector<int>{1, 2, 3, 4}));
		CHECK_EQ(1, effects.size());
		MHomingEffect& effect = static_cast<MHomingEffect&>(*effects.front());
		CHECK_EQ(MEffect::EFFECT_HOMING, effect.GetEffectType());
		CheckCommon(effect, 300, 400, 50 + TILE_Y, 30);
		CHECK_EQ(direction, effect.GetDirection());
		CHECK_EQ(123, effect.GetTraceCreatureID());
		CHECK(effect.IsMulti());
		CHECK_EQ(42, effect.GetActionInfo());
		CHECK(effect.GetEffectTarget() == info.pEffectTarget);
		auto twin = Twin(info, kAngles[direction], 13, 30);
		CheckFlight(effect, *twin);
	}
}

TEST(CreatureAttackEffectGenerators, AHaloTurnsFasterFromTwoDirectionsAside)
{
	World world;
	bool sides[2] = {false, false};
	// Two creatures whose pixel x and y differ, so a mixed-up target shows
	// in the halo's flight (which does not trace the creature).
	for (const MCreatureParabolaPosition& at : {MCreatureParabolaPosition{1, 7, 6}, MCreatureParabolaPosition{9, 12, 6}})
	for (BYTE direction : {BYTE(0), BYTE(2), BYTE(3), BYTE(6)})
	{
		creature = at;
		for (unsigned seed : {1u, 2u, 3u, 4u})
		{
			std::srand(seed);
			const int side = std::rand() % 2 ? 2 : 6;
			sides[side == 2] = true;
			std::srand(seed);
			effects.clear();
			EFFECTGENERATOR_INFO info = Info(SKILL_CLIENT_HALO_ATTACK, direction);
			CHECK(world.homing.Generate(info));
			CHECK_EQ(1, effects.size());
			const bool across = direction == 2 || direction == 6;
			// Across, it lasts 8 frames; otherwise its count.
			CheckCommon(*effects.front(), 300, 400, 50 + TILE_Y, across ? 8 : 30);
			CHECK_EQ(direction, effects.front()->GetDirection());
			auto twin = Twin(info, kAngles[(direction + side) % 8], across ? 45 : 30, across ? 8 : 30);
			CheckFlight(*effects.front(), *twin);
		}
	}
	// The seeds drew both sides.
	CHECK(sides[0] && sides[1]);
}

TEST(CreatureAttackEffectGenerators, AHomingAttackNeedsTheSpriteTheCreatureTheFramesAndTheZone)
{
	World world;
	for (int missing = 0; missing < 4; missing++)
	{
		calls.clear();
		effects.clear();
		spriteAvailable = missing != 0;
		creatureAvailable = missing != 1;
		framesAvailable = missing != 2;
		acceptLimit = missing != 3 ? 99 : 0;
		auto target = Target();
		EFFECTGENERATOR_INFO info = Info();
		info.pEffectTarget = target.get();
		CHECK(!world.homing.Generate(info));
		CHECK(effects.empty());
		CHECK_EQ(missing + 1, static_cast<int>(calls.size()));
	}
	MAttackCreatureHomingEffectGenerator::SetHost(nullptr);
	CHECK(!world.homing.Generate(Info()));
}
