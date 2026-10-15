#include "test_framework.h"
#include "CreatureDrawExceptions.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"
#include "MEventManager.h"
#include "MStatus.h"
#include "MTypeDef.h"
#include "SoundDef.h"
#include <set>
#include <vector>

// How some creature types are drawn (task 4.229). The table maps each of
// those creature types to a procedure that may change the action, frame
// and direction about to be drawn, given the frame counter; it reaches
// the creature through CreatureState.
namespace {

struct Fake
{
	TYPE_OBJECTID id = 5;
	int type = 0;
	bool dead = false;
	std::set<int> effects;
	int actionCount = 0, actionCountMax = 8;
	// As in MCreature: SetDirection sets where it turns to, GetDirection
	// reads where it faces now (SetCurrentDirection sets that).
	int direction = 0;
	int currentDirection = 0;
	int shadowCount = -1;
	std::vector<int> currentDirections;
	MStatus status;
};
MCreature* C(Fake& f) { return reinterpret_cast<MCreature*>(&f); }
Fake& F(MCreature* c) { return *reinterpret_cast<Fake*>(c); }

std::vector<TYPE_SOUNDID> sounds;

const CreatureState::Host creatureHost{
	.Status = [](MCreature* c) -> MStatus* { return &F(c).status; },
	.GetID = [](MCreature* c) { return F(c).id; },
	.IsAlive = [](MCreature* c) { return !F(c).dead; },
	.GetDirection = [](MCreature* c) { return F(c).currentDirection; },
	.SetDirection = [](MCreature* c, int d) { F(c).direction = d; },
	.GetCreatureType = [](MCreature* c) { return F(c).type; },
	.SetCurrentDirection = [](MCreature* c, int d) { F(c).currentDirection = d; F(c).currentDirections.push_back(d); },
	.IsDead = [](MCreature* c) { return F(c).dead; },
	.HasEffectStatus = [](MCreature* c, EFFECTSTATUS s) { return F(c).effects.count(s) != 0; },
	.GetActionCount = [](MCreature* c) { return F(c).actionCount; },
	.GetActionCountMax = [](MCreature* c) { return F(c).actionCountMax; },
	.SetShadowCount = [](MCreature* c, int n) { F(c).shadowCount = n; },
};
const WorldEvents::Host worldHost{
	.PlaySound = [](TYPE_SOUNDID id) { sounds.push_back(id); },
};

struct Fixture
{
	std::map<DWORD, DrawCreatureExceptionProc> procs;
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creatureHost);
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&worldHost);
	MEventQueue events;
	MEventQueue* oldEvents = g_pEventManager;
	Fake creature;
	Fixture() { sounds.clear(); MapCreatureDrawExceptions(procs); g_pEventManager = &events; }
	~Fixture() { CreatureState::SetHost(oldCreature); WorldEvents::SetHost(oldWorld); g_pEventManager = oldEvents; }
	struct Drawn { int action, frame, direction; };
	// Draws the creature as its type's procedure says, from (action,
	// frame, direction) at the frame counter.
	Drawn Draw(int type, int action, int frame, int direction, DWORD currentFrame = 0)
	{
		creature.type = type;
		const auto entry = procs.find(DWORD(type));
		CHECK(entry != procs.end());
		if (entry != procs.end()) entry->second(C(creature), action, frame, direction, currentFrame);
		return {action, frame, direction};
	}
};

bool Is(const Fixture::Drawn& d, int action, int frame, int direction)
{
	return d.action == action && d.frame == frame && d.direction == direction;
}

} // namespace

TEST(CreatureDrawExceptions, TheTableHoldsItsTypesOnly)
{
	Fixture f;
	const int ranges[][2] = {{371, 376}, {433, 433}, {482, 482}, {526, 549}, {560, 563}, {636, 636},
		{650, 650}, {660, 670}, {672, 673}, {675, 675}, {698, 698}, {717, 717}, {721, 723},
		{726, 732}, {734, 734}, {738, 741}, {743, 748}, {753, 753}, {764, 765}, {792, 792}};
	size_t count = 0;
	for (const auto& r : ranges)
		for (int type = r[0]; type <= r[1]; ++type, ++count)
			CHECK(f.procs.count(DWORD(type)) == 1);
	CHECK_EQ(count, f.procs.size());
	for (int type : {0, 370, 377, 432, 434, 525, 550, 559, 564, 671, 674, 718, 720, 724, 725, 733, 735, 737, 742, 749, 763, 766, 791, 793})
		CHECK(f.procs.count(DWORD(type)) == 0);
}

TEST(CreatureDrawExceptions, SomeOnlyFaceSouth)
{
	Fixture f;
	for (int type : {371, 376, 482, 526, 549, 560, 563, 650, 660, 670, 673})
		CHECK(Is(f.Draw(type, ACTION_MOVE, 3, 6), ACTION_MOVE, 3, 2));
}

TEST(CreatureDrawExceptions, StillStandingOnes)
{
	Fixture f;
	for (int type : {738, 739, 741})
		CHECK(Is(f.Draw(type, ACTION_MOVE, 3, 6), ACTION_STAND, 0, 2));
	CHECK(Is(f.Draw(740, ACTION_MOVE, 3, 6), ACTION_STAND, 0, 1));		// a claymore
	CHECK(Is(f.Draw(675, ACTION_MOVE, 3, 6), ACTION_MAGIC, 0, 3));		// an ousters NPC
	CHECK(Is(f.Draw(636, ACTION_MOVE, 3, 6), 6, 14, 6));					// the fish shop's landlord lies down
	CHECK(Is(f.Draw(792, ACTION_MOVE, 3, 6), ACTION_STAND, 3, 3));		// the Seal of Reason
	CHECK(Is(f.Draw(792, ACTION_DIE, 3, 6), ACTION_DIE, 3, 3));
}

TEST(CreatureDrawExceptions, TepezAndTheHornStandUntilComaOrDeath)
{
	Fixture f;
	CHECK(Is(f.Draw(433, ACTION_MOVE, 3, 6), ACTION_STAND, 3, 1));
	CHECK(Is(f.Draw(698, ACTION_MOVE, 3, 6), ACTION_STAND, 3, 2));
	f.creature.effects = {EFFECTSTATUS_COMA};
	CHECK(Is(f.Draw(433, ACTION_MOVE, 3, 6), ACTION_DIE, 3, 1));
	CHECK(Is(f.Draw(698, ACTION_MOVE, 3, 6), ACTION_DIE, 3, 2));
	f.creature.effects.clear();
	f.creature.dead = true;
	CHECK(Is(f.Draw(433, ACTION_MOVE, 3, 6), ACTION_DIE, 3, 1));
	CHECK(Is(f.Draw(698, ACTION_MOVE, 3, 6), ACTION_DIE, 3, 2));
}

TEST(CreatureDrawExceptions, TheSweeperKeeperWalksInPlaceAndRings)
{
	Fixture f;
	f.creature.id = 5;
	CHECK(Is(f.Draw(672, ACTION_MOVE, 0, 6, 20), ACTION_MOVE, (20 + 5) % 11, 2));
	CHECK(sounds.empty());
	CHECK(Is(f.Draw(672, ACTION_MOVE, 0, 6, 28), ACTION_MOVE, (28 + 5) % 11, 2));	// every 14th frame
	CHECK((sounds == std::vector<TYPE_SOUNDID>{SOUND_SWEEPER_REPEAT}));
	CHECK(Is(f.Draw(672, ACTION_STAND, 4, 6, 28), ACTION_STAND, 4, 2));			// only while moving
	CHECK_EQ(1u, sounds.size());
	f.Draw(672, ACTION_MOVE, 0, 6, 21);											// not a 14th frame
	CHECK_EQ(1u, sounds.size());
	f.Draw(672, ACTION_MOVE, 0, 6, 14);
	CHECK_EQ(2u, sounds.size());
}

TEST(CreatureDrawExceptions, GildreFloatsDrainingAndKneelsDead)
{
	Fixture f;
	for (int type : {717, 721, 723})
		CHECK(Is(f.Draw(type, ACTION_MOVE, 3, 6), ACTION_MOVE, 3, 6));
	f.creature.effects = {EFFECTSTATUS_GDR_FLOATING};
	CHECK(Is(f.Draw(717, ACTION_MOVE, 13, 6), ACTION_VAMPIRE_DRAIN, 6 + (13 & 7), 6));
	CHECK(Is(f.Draw(717, ACTION_MOVE, 14, 6), ACTION_VAMPIRE_DRAIN, 12, 6));
	CHECK(Is(f.Draw(721, ACTION_MOVE, 14, 6), ACTION_VAMPIRE_DRAIN, 12, 6));
	CHECK(Is(f.Draw(723, ACTION_MOVE, 14, 6), ACTION_VAMPIRE_DRAIN, 12, 6));
	f.creature.effects.clear();
	// Dead, at the end of its action: 723 kneels on frame 46.
	f.creature.dead = true;
	f.creature.actionCount = 8;
	CHECK(Is(f.Draw(723, ACTION_DIE, 3, 6), ACTION_DIE, 46, 6));
	CHECK(Is(f.Draw(717, ACTION_DIE, 3, 6), ACTION_DIE, 3, 6));
	f.creature.actionCount = 9;											// past the end
	CHECK(Is(f.Draw(723, ACTION_DIE, 3, 6), ACTION_DIE, 46, 6));
	f.creature.actionCount = 7;
	CHECK(Is(f.Draw(723, ACTION_DIE, 3, 6), ACTION_DIE, 3, 6));
	f.creature.actionCount = 8;
	f.creature.dead = false;
	CHECK(Is(f.Draw(723, ACTION_DIE, 3, 6), ACTION_DIE, 3, 6));
}

TEST(CreatureDrawExceptions, FrozenGildreTurnsEveryOtherFrame)
{
	// It is told to turn one step from the drawn direction, and drawn
	// facing where it faces now.
	Fixture f;
	f.creature.direction = 7;
	f.creature.currentDirection = 3;
	CHECK(Is(f.Draw(722, ACTION_STAND, 3, 7, 2), ACTION_STAND, 3, 7));
	CHECK_EQ(7, f.creature.direction);
	CHECK(Is(f.Draw(722, ACTION_STAND, 3, 7, 3), ACTION_STAND, 3, 3));
	CHECK_EQ(0, f.creature.direction);									// (7 + 1) % 8
}

TEST(CreatureDrawExceptions, WaypointsTowersObstaclesAndTheGoreGrand)
{
	Fixture f;
	for (int type : {730, 731, 732})
		CHECK(Is(f.Draw(type, ACTION_MOVE, 3, 6), ACTION_STAND, 0, 2));
	CHECK(Is(f.Draw(734, ACTION_MOVE, 3, 6), ACTION_STAND, 0, 1));		// a guild tower
	for (int type = 743; type <= 748; ++type)
		CHECK(Is(f.Draw(type, ACTION_MOVE, 3, 6), ACTION_STAND, 0, 2));
	CHECK(Is(f.Draw(753, ACTION_MOVE, 3, 6), ACTION_MOVE, 3, 2));
	f.creature.dead = true;
	for (int type : {730, 731, 732})
		CHECK(Is(f.Draw(type, ACTION_DIE, 3, 6), ACTION_DIE, 3, 6));		// a dead waypoint: as drawn
	CHECK(Is(f.Draw(734, ACTION_DIE, 3, 6), ACTION_DIE, 3, 1));
	for (int type = 743; type <= 748; ++type)
		CHECK(Is(f.Draw(type, ACTION_MOVE, 3, 6), ACTION_DIE, 3, 2));
	CHECK(Is(f.Draw(753, ACTION_MOVE, 3, 6), ACTION_DIE, 3, 2));
	// Standing still even dead.
	for (int type : {738, 739, 741})
		CHECK(Is(f.Draw(type, ACTION_DIE, 3, 6), ACTION_STAND, 0, 2));
}

TEST(CreatureDrawExceptions, TheGuildWarGateShowsItsDamage)
{
	Fixture f;
	f.creature.status.SetStatus(MODIFY_MAX_HP, 100);
	const int expected[][2] = {{100, 0}, {80, 0}, {79, 1}, {60, 1}, {40, 2}, {20, 3}, {19, 4}, {0, 4}};
	for (const auto& e : expected)
	{
		f.creature.status.SetStatus(MODIFY_CURRENT_HP, DWORD(e[0]));
		for (int type : {726, 727, 728, 729})
			CHECK(Is(f.Draw(type, ACTION_MOVE, 9, 6), ACTION_STAND, 4 - e[0] * 5 / 100 < 0 ? 0 : 4 - e[0] * 5 / 100, 1));
	}
	CHECK(Is(f.Draw(726, ACTION_MOVE, 9, 6), ACTION_STAND, 4, 1));
	f.creature.status.SetStatus(MODIFY_CURRENT_HP, 100);
	CHECK(Is(f.Draw(726, ACTION_MOVE, 9, 6), ACTION_STAND, 0, 1));
	f.creature.status.SetStatus(MODIFY_CURRENT_HP, 120);					// past its maximum: no less than 0
	CHECK(Is(f.Draw(726, ACTION_MOVE, 9, 6), ACTION_STAND, 0, 1));
	f.creature.dead = true;
	CHECK(Is(f.Draw(726, ACTION_MOVE, 0, 6), ACTION_STAND, 4, 1));
}

TEST(CreatureDrawExceptions, Types764And765ShakeTheGroundAndTurnWhileDrained)
{
	Fixture f;
	// Not casting: no after-images.
	CHECK(Is(f.Draw(764, ACTION_STAND, 2, 6), ACTION_STAND, 2, 6));
	CHECK_EQ(0, f.creature.shadowCount);
	f.creature.shadowCount = 3;
	CHECK(Is(f.Draw(764, ACTION_MAGIC, 2, 6), ACTION_MAGIC, 2, 6));
	CHECK_EQ(3, f.creature.shadowCount);
	// A step on frames 1 and 8 shakes the screen.
	f.Draw(764, ACTION_MOVE, 2, 6);
	CHECK(!f.events.IsEvent(EVENTID_METEOR_SHAKE));
	f.Draw(765, ACTION_MOVE, 8, 6);
	const MEvent* pShake = f.events.GetEvent(EVENTID_METEOR_SHAKE);
	CHECK(pShake != nullptr);
	if (pShake != nullptr)
	{
		CHECK_EQ(EVENTTYPE_ZONE, pShake->eventType);
		CHECK_EQ(300, pShake->eventDelay);
		CHECK_EQ(EVENTFLAG_SHAKE_SCREEN, pShake->eventFlag);
		CHECK_EQ(1, pShake->parameter3);
	}
	f.events.RemoveAllEvent();
	f.Draw(764, ACTION_MOVE, 1, 6);
	CHECK(f.events.IsEvent(EVENTID_METEOR_SHAKE));
	// Dying, 765 faces south; 764 as drawn, and 765 too when not dying.
	CHECK(Is(f.Draw(765, ACTION_DIE, 2, 6), ACTION_DIE, 2, 2));
	CHECK(Is(f.Draw(764, ACTION_DIE, 2, 6), ACTION_DIE, 2, 6));
	CHECK(Is(f.Draw(765, ACTION_STAND, 2, 6), ACTION_STAND, 2, 6));
	// Drained: drawn attacking, held on frame 4, turning back every other
	// frame counter.
	CHECK(Is(f.Draw(764, ACTION_DRAINED, 3, 6, 1), ACTION_ATTACK, 3, 6));
	CHECK(Is(f.Draw(764, ACTION_DRAINED, 6, 6, 2), ACTION_ATTACK, 4, 6));
	CHECK(Is(f.Draw(764, ACTION_DRAINED, 4, 6, 3), ACTION_ATTACK, 4, 6));	// up to 4: not held, no turn
	CHECK(f.creature.currentDirections.empty());
	CHECK(Is(f.Draw(764, ACTION_DRAINED, 6, 0, 3), ACTION_ATTACK, 4, 7));
	CHECK((f.creature.currentDirections == std::vector<int>{7}));
	CHECK(Is(f.Draw(764, ACTION_DRAINED, 5, 6, 3), ACTION_ATTACK, 4, 5));	// held from 5, turned from 6
	CHECK((f.creature.currentDirections == std::vector<int>{7, 5}));
}

TEST(CreatureDrawExceptions, TheNewCreatureEntriesForwardOrDefault)
{
	Fake creature;
	const CreatureState::Host* old = CreatureState::SetHost(nullptr);
	CHECK(!CreatureState::HasEffectStatus(C(creature), EFFECTSTATUS_COMA));
	CHECK_EQ(0, CreatureState::GetActionCount(C(creature)));
	CHECK_EQ(0, CreatureState::GetActionCountMax(C(creature)));
	CreatureState::SetShadowCount(C(creature), 3);
	CHECK_EQ(-1, creature.shadowCount);
	const CreatureState::Host none{};
	CreatureState::SetHost(&none);
	CHECK(!CreatureState::HasEffectStatus(C(creature), EFFECTSTATUS_COMA));
	CHECK_EQ(0, CreatureState::GetActionCount(C(creature)));
	CHECK_EQ(0, CreatureState::GetActionCountMax(C(creature)));
	CreatureState::SetShadowCount(C(creature), 3);
	CHECK_EQ(-1, creature.shadowCount);
	CreatureState::SetHost(&creatureHost);
	creature.effects = {EFFECTSTATUS_COMA};
	creature.actionCount = 2;
	creature.actionCountMax = 6;
	CHECK(CreatureState::HasEffectStatus(C(creature), EFFECTSTATUS_COMA));
	CHECK_EQ(2, CreatureState::GetActionCount(C(creature)));
	CHECK_EQ(6, CreatureState::GetActionCountMax(C(creature)));
	CreatureState::SetShadowCount(C(creature), 3);
	CHECK_EQ(3, creature.shadowCount);
	CreatureState::SetHost(old);
}
