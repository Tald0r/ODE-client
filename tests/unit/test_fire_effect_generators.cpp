#include "test_framework.h"
#include "EffectSpriteTypeDef.h"
#include "MEffect.h"
#include "MEffectTarget.h"
#include "MEventQueue.h"
#include "MFirePiecingEffectGenerator.h"
#include "MPlayingWithFireEffectGenerator.h"
#include "MViewDef.h"
#include "WorldEventsHost.h"
#include <cstdlib>
#include <memory>
#include <vector>

// Fire piercing and playing with fire: fires laid on the tiles. Started
// (no previous fire), a fire stands half a tile from its point in its
// direction, its sprite the direction's fire (one of two sets at
// random); fire piercing adds its two neighbouring fires (3 in all),
// playing with fire all 16 round it and shakes the screen. Continued
// from a fire, one more stands beside it where that fire points:
// piercing's switches set at random, playing's turns to the next fire
// (and after 112 continuations switches set at random). The first sprite
// must be in the table. Each effect takes the steps, frames, direction
// and power and may overlap; the first the zone keeps follows the action
// and target, and later ones a copy of the target set at their place.
namespace {

constexpr int kTableSize = EFFECTSPRITETYPE_FIRE_32 + 1;
DWORD frameNow;
int acceptLimit, refuseFirst, offered, refusedType;
std::vector<std::unique_ptr<MEffect>> effects;
std::vector<MEvent> events;
std::vector<int> removedTargets;

const MEffectHost effectHost{
	.CurrentFrame = []() { return frameNow; },
	.Light = [](BYTE, TYPE_FRAMEID, BYTE, BYTE) { return 0; },
};
const MEffectTargetHost targetHost{.RemoveFromPlayer = [](BYTE id) { removedTargets.push_back(id); }};
// Each sprite's frame ID is its type, to tell the effects apart; its blit
// and frame count vary with it too.
BYTE BltOf(int type) { return static_cast<BYTE>(type % 2 ? BLT_EFFECT : BLT_NORMAL); }
int FramesOf(int type) { return type % 7 + 1; }
const MZoneAttackEffectHost host{
	.Sprite = [](TYPE_EFFECTSPRITETYPE type, MZoneAttackEffectSprite& sprite) {
		if (type >= kTableSize || type == refusedType) return false;
		sprite = {BltOf(type), static_cast<TYPE_FRAMEID>(type), FramesOf(type)};
		return true;
	},
	// Refuses the first few offered, and any past the limit.
	.Queue = [](std::unique_ptr<MEffect> effect) {
		if (++offered <= refuseFirst) return false;
		if (static_cast<int>(effects.size()) >= acceptLimit) return false;
		effects.push_back(std::move(effect));
		return true;
	},
};
const WorldEvents::Host worldEvents = []() {
	WorldEvents::Host h{};
	h.AddEvent = [](MEvent& event) { events.push_back(event); };
	return h;
}();

struct World
{
	const MEffectHost* oldEffect = MEffect::SetHost(&effectHost);
	const MEffectTargetHost* oldTarget = MEffectTarget::SetHost(&targetHost);
	const MZoneAttackEffectHost* oldPiercing = MFirePiecingEffectGenerator::SetHost(&host);
	const MZoneAttackEffectHost* oldPlaying = MPlayingWithFireEffectGenerator::SetHost(&host);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&worldEvents);
	MFirePiecingEffectGenerator piercing;
	MPlayingWithFireEffectGenerator playing;
	World()
	{
		frameNow = 100;
		acceptLimit = 99;
		refuseFirst = offered = 0;
		refusedType = -1;
		effects.clear();
		events.clear();
		removedTargets.clear();
	}
	~World()
	{
		effects.clear();
		WorldEvents::SetHost(oldEvents);
		MPlayingWithFireEffectGenerator::SetHost(oldPlaying);
		MFirePiecingEffectGenerator::SetHost(oldPiercing);
		MEffectTarget::SetHost(oldTarget);
		MEffect::SetHost(oldEffect);
	}
};

EFFECTGENERATOR_INFO Info(BYTE direction, MEffect* pPrevious = nullptr)
{
	EFFECTGENERATOR_INFO info{};
	info.nActionInfo = 42;
	info.effectSpriteType = EFFECTSPRITETYPE_FIRE_1;
	info.x0 = 480;
	info.y0 = 240;
	info.z0 = 7;
	info.creatureID = 123;
	info.direction = direction;
	info.step = 16;
	info.count = 30;
	info.linkCount = 5;
	info.power = 77;
	info.pPreviousEffect = pPrevious;
	return info;
}

void CheckFire(MEffect& effect, int est, int x, int y, BYTE direction)
{
	CHECK_EQ(est, effect.GetEst());
	CHECK_EQ(BltOf(est), effect.GetBltType());
	CHECK_EQ(est, effect.GetFrameID());
	CHECK_EQ(FramesOf(est), effect.GetMaxFrame());
	CHECK_EQ(x, effect.GetPixelX());
	CHECK_EQ(y, effect.GetPixelY());
	CHECK_EQ(7, effect.GetPixelZ());
	CHECK_EQ(16, effect.GetStepPixel());
	CHECK_EQ(frameNow + 30 - 1, effect.GetEndFrame());
	CHECK_EQ(frameNow + 5 - 1, effect.GetEndLinkFrame());
	CHECK_EQ(direction, effect.GetDirection());
	CHECK_EQ(77, effect.GetPower());
	CHECK(effect.IsMulti());
	CHECK_EQ(42, effect.GetActionInfo());
}

// A direction's start: the offset from the point and the first fire.
struct Start { BYTE direction; int dx, dy, fire; };
const Start kStarts[] = {
	{DIRECTION_LEFT, -TILE_X / 2, 0, EFFECTSPRITETYPE_FIRE_5},
	{DIRECTION_LEFTDOWN, -TILE_X / 2, TILE_Y / 2, EFFECTSPRITETYPE_FIRE_3},
	{DIRECTION_DOWN, 0, TILE_Y / 2, EFFECTSPRITETYPE_FIRE_1},
	{DIRECTION_RIGHTDOWN, TILE_X / 2, TILE_Y / 2, EFFECTSPRITETYPE_FIRE_15},
	{DIRECTION_RIGHT, TILE_X / 2, 0, EFFECTSPRITETYPE_FIRE_13},
	{DIRECTION_RIGHTUP, TILE_X / 2, -TILE_Y / 2, EFFECTSPRITETYPE_FIRE_11},
	{DIRECTION_UP, 0, -TILE_Y / 2, EFFECTSPRITETYPE_FIRE_9},
	{DIRECTION_LEFTUP, -TILE_X / 2, -TILE_Y / 2, EFFECTSPRITETYPE_FIRE_7}};

// Where a fire continues from one of each of the 16 fires (of either set).
const int kNext[16][2] = {
	{0, TILE_Y}, {-TILE_X / 2, TILE_Y}, {-TILE_X, TILE_Y}, {-TILE_X, TILE_Y / 2},
	{-TILE_X, 0}, {-TILE_X, -TILE_Y / 2}, {-TILE_X, -TILE_Y}, {-TILE_X / 2, -TILE_Y},
	{0, -TILE_Y}, {TILE_X / 2, -TILE_Y}, {TILE_X, -TILE_Y}, {TILE_X, -TILE_Y / 2},
	{TILE_X, 0}, {TILE_X, TILE_Y / 2}, {TILE_X, TILE_Y}, {TILE_X / 2, TILE_Y}};

// The fire `offset` steps round from `fire` (in its 16), in a set chosen
// by a draw.
int Round(int fire, int offset, int draw)
{
	return EFFECTSPRITETYPE_FIRE_1 + (fire - EFFECTSPRITETYPE_FIRE_1 + offset) % 16 + draw * 16;
}

std::unique_ptr<MEffect> Previous(int est, int x, int y)
{
	auto previous = std::make_unique<MEffect>(BLT_EFFECT);
	previous->SetEst(est);
	previous->SetPixelPosition(x, y, 0);
	return previous;
}

} // namespace

TEST(FireEffectGenerators, FirePiercingStartsThreeFiresInItsDirection)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_FIRE_PIECING, world.piercing.GetID());
	bool seen[3][2] = {};
	for (const Start& s : kStarts)
	{
		for (unsigned seed = 1; seed <= 6; seed++)
		{
			std::srand(seed);
			const int d0 = std::rand() % 2, d1 = std::rand() % 2, d2 = std::rand() % 2;
			seen[0][d0] = seen[1][d1] = seen[2][d2] = true;
			std::srand(seed);
			effects.clear();
			auto target = std::make_unique<MEffectTarget>(3);
			target->Set(1, 2, 3, 456);
			EFFECTGENERATOR_INFO info = Info(s.direction);
			info.pEffectTarget = target.get();
			CHECK(world.piercing.Generate(info));
			target.release();
			CHECK_EQ(3, effects.size());
			if (effects.size() != 3) continue;
			const int x = 480 - TILE_X / 2 + s.dx, y = 240 + TILE_Y / 2 + s.dy;
			const int first = s.fire + d0 * 16;
			CheckFire(*effects[0], first, x, y, s.direction);
			CheckFire(*effects[1], Round(first, 15, d1), x, y, s.direction);
			CheckFire(*effects[2], Round(first, 1, d2), x, y, s.direction);
			// The first follows the target; the others a copy set at their place.
			CHECK(effects[0]->GetEffectTarget() == info.pEffectTarget);
			for (int i = 1; i < 3; i++)
			{
				MEffectTarget* pCopy = effects[i]->GetEffectTarget();
				CHECK(pCopy != nullptr && pCopy != info.pEffectTarget);
				CHECK(pCopy != nullptr && pCopy->GetX() == x && pCopy->GetY() == y && pCopy->GetZ() == 7 && pCopy->GetID() == 123);
			}
		}
	}
	// The seeds drew each set at each draw.
	for (const auto& draw : seen) CHECK(draw[0] && draw[1]);
	// No target: none follow one, though all follow the action.
	effects.clear();
	CHECK(world.piercing.Generate(Info(DIRECTION_DOWN)));
	for (const auto& effect : effects)
	{
		CHECK(effect->GetEffectTarget() == nullptr);
		CHECK_EQ(42, effect->GetActionInfo());
	}
	CHECK(events.empty());
}

TEST(FireEffectGenerators, FirePiercingContinuesBesideTheLastFireInEitherSet)
{
	World world;
	for (int k = 0; k < 16; k++)
	{
		for (int set : {0, 16})
		{
			const int last = EFFECTSPRITETYPE_FIRE_1 + k + set;
			for (unsigned seed : {3u, 4u})
			{
				std::srand(seed);
				const int draw = std::rand() % 2;
				std::srand(seed);
				effects.clear();
				auto previous = Previous(last, 600, 300);
				CHECK(world.piercing.Generate(Info(DIRECTION_LEFT, previous.get())));
				CHECK_EQ(1, effects.size());
				// The other set, or the same, at random.
				const int expected = last > EFFECTSPRITETYPE_FIRE_16 ? last - draw * 16 : last + draw * 16;
				CheckFire(*effects[0], expected, 600 + kNext[k][0], 300 + kNext[k][1], DIRECTION_LEFT);
			}
		}
	}
}

TEST(FireEffectGenerators, AFireOutsideTheTableMakesNone)
{
	World world;
	// Piercing, continued from a fire past the table: none.
	auto previous = Previous(kTableSize + 20, 600, 300);
	CHECK(!world.piercing.Generate(Info(DIRECTION_LEFT, previous.get())));
	CHECK(effects.empty());
	// Started in no known direction, both keep the sprite they were given;
	// past the table, none, though playing with fire has already shaken
	// the screen. (Continued, playing with fire wraps past the last fire
	// to the first, so it is never refused that way.)
	EFFECTGENERATOR_INFO unknown = Info(8);
	unknown.effectSpriteType = kTableSize + 1;
	CHECK(!world.piercing.Generate(unknown));
	CHECK(!world.playing.Generate(unknown));
	CHECK(effects.empty());
	CHECK_EQ(1, events.size());
	// Without a host: none.
	MFirePiecingEffectGenerator::SetHost(nullptr);
	MPlayingWithFireEffectGenerator::SetHost(nullptr);
	CHECK(!world.piercing.Generate(Info(DIRECTION_LEFT)));
	CHECK(!world.playing.Generate(Info(DIRECTION_LEFT)));
	CHECK(effects.empty());
}

TEST(FireEffectGenerators, TheTargetGoesToTheFirstFireTheZoneKeeps)
{
	World world;
	// The zone refuses them all: still true.
	acceptLimit = 0;
	CHECK(world.piercing.Generate(Info(DIRECTION_DOWN)));
	CHECK(effects.empty());
	// It keeps two: the first follows the target, the second a copy.
	acceptLimit = 2;
	auto target = std::make_unique<MEffectTarget>(3);
	EFFECTGENERATOR_INFO info = Info(DIRECTION_DOWN);
	info.pEffectTarget = target.get();
	CHECK(world.piercing.Generate(info));
	target.release();
	CHECK_EQ(2, effects.size());
	if (effects.size() != 2) return;
	CHECK(effects[0]->GetEffectTarget() == info.pEffectTarget);
	CHECK(effects[1]->GetEffectTarget() != nullptr && effects[1]->GetEffectTarget() != info.pEffectTarget);
	// The first refused: the second, the first kept, follows the target.
	effects.clear();
	acceptLimit = 99;
	refuseFirst = 1;
	offered = 0;
	auto second = std::make_unique<MEffectTarget>(3);
	EFFECTGENERATOR_INFO later = Info(DIRECTION_DOWN);
	later.pEffectTarget = second.get();
	CHECK(world.piercing.Generate(later));
	second.release();
	CHECK_EQ(2, effects.size());
	if (effects.size() != 2) return;
	CHECK(effects[0]->GetEffectTarget() == later.pEffectTarget);
	CHECK(effects[1]->GetEffectTarget() != later.pEffectTarget);
	// Playing with fire likewise: the first refused, the next follows the
	// target and the rest copies.
	effects.clear();
	refuseFirst = 1;
	offered = 0;
	auto played = std::make_unique<MEffectTarget>(3);
	EFFECTGENERATOR_INFO playedInfo = Info(DIRECTION_DOWN);
	playedInfo.pEffectTarget = played.get();
	CHECK(world.playing.Generate(playedInfo));
	played.release();
	CHECK_EQ(15, effects.size());
	if (effects.size() != 15) return;
	CHECK(effects[0]->GetEffectTarget() == playedInfo.pEffectTarget);
	CHECK(effects[1]->GetEffectTarget() != nullptr && effects[1]->GetEffectTarget() != playedInfo.pEffectTarget);
	// Playing with fire, all refused: still true (the caller keeps no
	// target; with one, it leaks, as before).
	effects.clear();
	refuseFirst = 99;
	offered = 0;
	CHECK(world.playing.Generate(Info(DIRECTION_DOWN)));
	CHECK(effects.empty());
}

TEST(FireEffectGenerators, AFireWhoseSpriteIsRefusedIsSkipped)
{
	World world;
	// Started down: the first fire, then the one before it, then the one
	// after; the one before is refused.
	std::srand(5);
	const int d0 = std::rand() % 2, d1 = std::rand() % 2, d2 = std::rand() % 2;
	std::srand(5);
	const int first = EFFECTSPRITETYPE_FIRE_1 + d0 * 16;
	refusedType = Round(first, 15, d1);
	CHECK(world.piercing.Generate(Info(DIRECTION_DOWN)));
	CHECK_EQ(2, effects.size());
	if (effects.size() != 2) return;
	CHECK_EQ(first, effects[0]->GetEst());
	CHECK_EQ(Round(first, 1, d2), effects[1]->GetEst());
}

TEST(FireEffectGenerators, PlayingWithFireStartsSixteenFiresRoundAndShakesTheScreen)
{
	World world;
	CHECK_EQ(EFFECTGENERATORID_PLAYING_WITH_FIRE, world.playing.GetID());
	bool seen[16][2] = {};
	auto allSeen = [&seen]() {
		for (const auto& draw : seen) if (!draw[0] || !draw[1]) return false;
		return true;
	};
	// Every direction at least twice, and on until each draw has given
	// both sets (some libraries' first draws barely vary between seeds).
	for (unsigned seed = 1; seed <= 16 || (!allSeen() && seed <= 400); seed++)
	{
		const Start& s = kStarts[seed % 8];
		std::srand(seed);
		int draws[16];
		for (int i = 0; i < 16; i++)
		{
			draws[i] = std::rand() % 2;
			seen[i][draws[i]] = true;
		}
		std::srand(seed);
		effects.clear();
		events.clear();
		CHECK(world.playing.Generate(Info(s.direction)));
		CHECK_EQ(16, effects.size());
		if (effects.size() != 16) continue;
		const int x = 480 - TILE_X / 2 + s.dx, y = 240 + TILE_Y / 2 + s.dy;
		const int first = s.fire + draws[0] * 16;
		CheckFire(*effects[0], first, x, y, s.direction);
		CheckFire(*effects[1], Round(first, 15, draws[1]), x, y, s.direction);
		for (int i = 2; i < 16; i++) CheckFire(*effects[i], Round(first, i - 1, draws[i]), x, y, s.direction);
		// No target: all follow the action alone.
		for (const auto& effect : effects)
		{
			CHECK(effect->GetEffectTarget() == nullptr);
			CHECK_EQ(42, effect->GetActionInfo());
		}
		CHECK_EQ(1, events.size());
		const MEvent& event = events.front();
		CHECK(event.eventID == EVENTID_METEOR_SHAKE && event.eventType == EVENTTYPE_ZONE);
		CHECK(event.eventDelay == 2000 && event.eventFlag == EVENTFLAG_SHAKE_SCREEN && event.parameter3 == 3);
	}
	for (const auto& draw : seen) CHECK(draw[0] && draw[1]);
}

TEST(FireEffectGenerators, PlayingWithFireGivesItsLaterFiresCopiesOfTheTarget)
{
	World world;
	auto target = std::make_unique<MEffectTarget>(3);
	target->Set(1, 2, 3, 456);
	EFFECTGENERATOR_INFO info = Info(DIRECTION_UP);
	info.pEffectTarget = target.get();
	CHECK(world.playing.Generate(info));
	target.release();
	CHECK_EQ(16, effects.size());
	CHECK(effects[0]->GetEffectTarget() == info.pEffectTarget);
	const int x = 480 - TILE_X / 2, y = 240 + TILE_Y / 2 - TILE_Y / 2;
	for (int i = 1; i < 16; i++)
	{
		MEffectTarget* pCopy = effects[i]->GetEffectTarget();
		CHECK(pCopy != nullptr && pCopy != info.pEffectTarget);
		CHECK(pCopy != nullptr && pCopy->GetX() == x && pCopy->GetY() == y && pCopy->GetZ() == 7 && pCopy->GetID() == 123);
	}
}

TEST(FireEffectGenerators, PlayingWithFireTurnsToTheNextFireThenAtRandom)
{
	World world;
	// Started, then continued: the next fire round, wrapping from the last.
	CHECK(world.playing.Generate(Info(DIRECTION_LEFT)));
	for (int k = 0; k < 16; k++)
	{
		for (int set : {0, 16})
		{
			const int last = EFFECTSPRITETYPE_FIRE_1 + k + set;
			effects.clear();
			events.clear();
			auto previous = Previous(last, 600, 300);
			CHECK(world.playing.Generate(Info(DIRECTION_RIGHT, previous.get())));
			CHECK_EQ(1, effects.size());
			const int next = last == EFFECTSPRITETYPE_FIRE_32 ? EFFECTSPRITETYPE_FIRE_1 : last + 1;
			CheckFire(*effects[0], next, 600 + kNext[k][0], 300 + kNext[k][1], DIRECTION_RIGHT);
			CHECK(events.empty());
		}
	}
	// 32 continued; up to the 112th it still turns.
	for (int i = 33; i <= 112; i++)
	{
		effects.clear();
		auto previous = Previous(EFFECTSPRITETYPE_FIRE_3, 600, 300);
		CHECK(world.playing.Generate(Info(DIRECTION_RIGHT, previous.get())));
		CHECK_EQ(EFFECTSPRITETYPE_FIRE_4, effects[0]->GetEst());
	}
	// From the 113th, it switches sets at random: from the second set back,
	// from the first (the 16th included) forward.
	bool seen[2] = {};
	for (int last : {EFFECTSPRITETYPE_FIRE_20, EFFECTSPRITETYPE_FIRE_17, EFFECTSPRITETYPE_FIRE_3, EFFECTSPRITETYPE_FIRE_16})
	{
		for (unsigned seed = 1; seed <= 6; seed++)
		{
			std::srand(seed);
			const int draw = std::rand() % 2;
			seen[draw] = true;
			std::srand(seed);
			effects.clear();
			auto previous = Previous(last, 600, 300);
			CHECK(world.playing.Generate(Info(DIRECTION_RIGHT, previous.get())));
			CHECK_EQ(1, effects.size());
			if (effects.empty()) continue;
			CHECK_EQ(last > EFFECTSPRITETYPE_FIRE_16 ? last - draw * 16 : last + draw * 16, effects[0]->GetEst());
		}
	}
	CHECK(seen[0] && seen[1]);
	// Started again: counting from none.
	CHECK(world.playing.Generate(Info(DIRECTION_LEFT)));
	effects.clear();
	auto previous = Previous(EFFECTSPRITETYPE_FIRE_3, 600, 300);
	CHECK(world.playing.Generate(Info(DIRECTION_RIGHT, previous.get())));
	CHECK_EQ(EFFECTSPRITETYPE_FIRE_4, effects[0]->GetEst());
}
