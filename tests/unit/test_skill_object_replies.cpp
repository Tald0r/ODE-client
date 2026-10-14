#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "ConvertDuration.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "MSkillManager.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCSkillToObjectOK1.h"
#include "Gpackets/GCSkillToObjectOK2.h"
#include "Gpackets/GCSkillToObjectOK5.h"
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

// A skill landing on a creature: the player's (OK1), another creature's
// on the player (OK2), or one between two other creatures (OK5). The
// user faces the target's cell and casts the skill at it (the player's
// own queue rides on the skill it is casting); its
// queue hits the target with the skill's result action, then the node its
// result kind names and its effect status. A weapon skill acts as the
// user's basic action (none at all when that is a result action); a skill
// with steps acts as the step its grade names. Some skills also show a
// copy or a summoned creature: Shadow Dancing, Illusion Inversion and
// Illendue's fading copies, Blaze Walk's strikes, the plasma rocket, and
// the wild wolf, which runs at a target or at the creature in a corpse
// (the skill then goes on against that one). Set Afire marks the user,
// Bite of Death marks the target as drained by the user. Without a zone,
// the user or the target nothing happens; a skill past the action table
// is refused. The player's skill also answers the wait, and a skill on the
// player changes its status; one on the player carries the player's
// pending effect status in place of the skill's, holds the player still a
// second after a Typhoon, shows the skill rising from the player when the
// user is not seen, lets the user's soul absorbing go on, and is turned
// into Protection from Acid's result when the player has it against an
// acid.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool hasPlayer = true;
PlayerState::WAIT_VERIFY waitVerify = PlayerState::WAIT_VERIFY_NULL;
MStatus playerStatus;
std::set<int> playerEffects;

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	std::string name;
	TYPE_SECTORPOSITION x = 0;
	TYPE_SECTORPOSITION y = 0;
	TYPE_ACTIONINFO basic = 0;
	int direction = 0;
	int z = 0;
};
std::map<TYPE_OBJECTID, FakeCreature*> creatures;
std::map<TYPE_OBJECTID, FakeCreature*> corpses;
// The player, elsewhere: the result nodes run only with one there.
FakeCreature me;

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
// A handler that passes on a creature it never found fails here.
FakeCreature& Fake(MCreature* pCreature)
{
	static FakeCreature none;
	CHECK(pCreature != nullptr);
	return pCreature == nullptr ? none : *reinterpret_cast<FakeCreature*>(pCreature);
}
std::string Name(MCreature* p) { return p == nullptr ? std::string("nobody") : Fake(p).name; }
void Record(MCreature* p, const std::string& what) { calls.push_back(Fake(p).name + "." + what); }
void Land(MActionResult* pResult)
{
	if (pResult == nullptr) return;
	pResult->Execute();
	delete pResult;
}

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? Handle(me) : nullptr; };
	host.GetID = []() { return me.id; };
	host.Status = []() { return hasPlayer ? &playerStatus : nullptr; };
	host.GetWaitVerify = []() { return waitVerify; };
	host.SetWaitVerifyNULL = []() {
		calls.push_back("verified");
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
	};
	host.PacketAddActionResult = [](std::uint16_t effectID, MActionResult* pResult) {
		calls.push_back("Self.adds|" + N(effectID));
		Land(pResult);
		return true;
	};
	host.AddEffectStatus = [](EFFECTSTATUS status, std::uint32_t delayFrame) {
		calls.push_back("Self.has|" + N(status) + "|" + N(delayFrame));
		return true;
	};
	host.SetDelay = [](std::uint32_t milliseconds) { calls.push_back("Self.held|" + N(milliseconds)); };
	host.SetUndergroundCreature = []() { calls.push_back("Self.burrow"); };
	host.HasEffectStatus = [](EFFECTSTATUS status) { return playerEffects.count(status) != 0; };
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) -> MCreature* {
		CHECK(zoneExists);
		const auto found = creatures.find(id);
		return found == creatures.end() ? nullptr : Handle(*found->second);
	};
	host.GetCorpseCreature = [](TYPE_OBJECTID itemID) -> MCreature* {
		const auto found = corpses.find(itemID);
		return found == corpses.end() ? nullptr : Handle(*found->second);
	};
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.GetBasicActionInfo = [](MCreature* p) { return Fake(p).basic; };
	host.GetDirection = [](MCreature* p) { return Fake(p).direction; };
	host.GetZ = [](MCreature* p) { return Fake(p).z; };
	host.ClearStopBloodDrain = [](MCreature* p) { Record(p, "drains on"); };
	host.SetDirectionToPosition = [](MCreature* p, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) { Record(p, "face|" + N(sX) + "|" + N(sY)); };
	host.SetEffectDelayFrame = [](MCreature* p, TYPE_ACTIONINFO ai, std::uint32_t frames) { Record(p, "delay|" + N(ai) + "|" + N(frames)); };
	host.PacketSpecialActionResult = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t grade) {
		Record(p, "result|" + N(ai) + "|" + N(id) + "|" + N(sX) + "|" + N(sY) + "|" + N(grade));
	};
	host.AddEffectStatus = [](MCreature* p, EFFECTSTATUS status, std::uint32_t delayFrame) {
		Record(p, "effect|" + N(status) + "|" + N(delayFrame));
		return true;
	};
	host.SetUndergroundCreature = [](MCreature* p) { Record(p, "burrow"); };
	host.PacketSpecialActionToOther = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_OBJECTID id, MActionResult* pResult) {
		Record(p, "cast|" + N(ai) + "|" + N(id));
		Land(pResult);
	};
	host.SetDrainCreatureID = [](MCreature* p, TYPE_OBJECTID id) { Record(p, "drained by|" + N(id)); };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.SkillShadowDancing = [](MCreature* pUser, MCreature* pTarget, int skillID) {
		calls.push_back(Name(pUser) + ".shadow dancing|" + Name(pTarget) + "|" + N(skillID));
	};
	host.SkillIllendue = [](MCreature* pUser, MCreature* pTarget, int skillID) {
		calls.push_back(Name(pUser) + ".illendue|" + Name(pTarget) + "|" + N(skillID));
	};
	host.SkillBlazeWalk = [](MCreature* pUser, MCreature* pTarget, int skillID, std::uint8_t grade) {
		calls.push_back(Name(pUser) + ".blaze walk|" + Name(pTarget) + "|" + N(skillID) + "|" + N(grade));
	};
	host.AddRocketLauncher = [](MCreature* pUser, MCreature* pTarget) { calls.push_back(Name(pUser) + ".rocket|" + Name(pTarget)); };
	host.AddWildWolf = [](MCreature* pUser, MCreature* pTarget, bool bEatCorpse) {
		calls.push_back(Name(pUser) + ".wild wolf|" + Name(pTarget) + (bEatCorpse ? "|eat" : "|hunt"));
	};
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int startZ, int direction,
		TYPE_OBJECTID targetID, TYPE_SECTORPOSITION targetX, TYPE_SECTORPOSITION targetY, int targetZ, std::uint32_t delayFrame,
		MActionResult* pResult, bool bStartFromMainNode, int sX, int sY) {
		CHECK(sX == 0 && sY == 0);
		calls.push_back("cell|" + N(actionInfo) + "|" + N(startX) + "," + N(startY) + "," + N(startZ) + "|dir " + N(direction) + "|" + N(targetID)
			+ "@" + N(targetX) + "," + N(targetY) + "," + N(targetZ) + "|" + N(delayFrame) + (bStartFromMainNode ? "|main" : "|resume"));
		Land(pResult);
	};
	return host;
}();

constexpr TYPE_ACTIONINFO kMin = MIN_RESULT_ACTIONINFO;
constexpr TYPE_ACTIONINFO kSkill = SKILL_DOUBLE_IMPACT;
constexpr TYPE_ACTIONINFO kStep = SKILL_TRIPLE_SLASHER;
constexpr int kDuration = 30;

struct World : GameModelWorld
{
	FakeCreature ogre, vanda, fallen;
	MActionInfoTable actions;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	// One row per skill, below the result actions, as the game's.
	MSkillInfoTable skills;
	MSkillInfoTable* oldSkills = g_pSkillInfoTable;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		me = {7, "Self", 30, 31, SKILL_ATTACK_GUN_SR, 6, 0};
		ogre = {1001, "Ogre", 10, 11, SKILL_ATTACK_MELEE, 3, 0};
		vanda = {1002, "Vanda", 20, 21, SKILL_ATTACK_GUN_SG, 5, 0};
		// Lying in corpse item 2001, outside the zone's creatures.
		fallen = {1500, "Fallen", 25, 26, SKILL_ATTACK_MELEE, 0, 0};
		creatures = {{ogre.id, &ogre}, {vanda.id, &vanda}, {me.id, &me}};
		corpses = {{2001, &fallen}};
		zoneExists = true;
		hasPlayer = true;
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
		playerStatus = MStatus();
		playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
		playerStatus.SetStatus(MODIFY_EFFECT_STAT, EFFECTSTATUS_NULL);
		playerEffects.clear();
		g_pSkillInfoTable = &skills;
		calls.clear();
		actions.Init(MAX_ACTIONINFO);
		actions.SetMinResultActionInfo(kMin);
		g_pActionInfoTable = &actions;
	}
	~World()
	{
		WorldEvents::SetHost(oldEvents);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		PlayerState::SetHost(oldPlayer);
		g_pActionInfoTable = oldActions;
		g_pSkillInfoTable = oldSkills;
		creatures.clear();
		corpses.clear();
	}
	MActionInfo& Row(TYPE_ACTIONINFO action) { return testfw::MutableRow(actions, action); }
};

std::string F() { return N(ConvertDurationToFrame(kDuration)); }

void Receive(ObjectID_t user, ObjectID_t target, int skill, int grade = 0)
{
	GCSkillToObjectOK5 packet;
	packet.setObjectID(user);
	packet.setTargetObjectID(target);
	packet.setSkillType(static_cast<SkillType_t>(skill));
	packet.setDuration(kDuration);
	packet.setGrade(static_cast<BYTE>(grade));
	GCSkillToObjectOK5Handler::execute(&packet, nullptr);
}

// The user facing the target and casting the skill, which lands on the
// target as its result action.
std::vector<std::string> Cast(TYPE_ACTIONINFO action, TYPE_ACTIONINFO result = 0)
{
	if (result == 0) result = action + kMin;
	return {"Ogre.face|20|21", "Ogre.cast|" + N(action) + "|1002", "Vanda.delay|" + N(result) + "|" + F(),
		"Vanda.result|" + N(result) + "|1002|20|21|0"};
}

std::vector<std::string> Then(std::vector<std::string> first, const std::vector<std::string>& rest)
{
	first.insert(first.end(), rest.begin(), rest.end());
	return first;
}

// The player's skill on a creature, its queue riding on effect 9.
void ReceivePlayers(ObjectID_t target, int skill, int grade = 0)
{
	GCSkillToObjectOK1 packet;
	packet.setSkillType(static_cast<SkillType_t>(skill));
	packet.setCEffectID(9);
	packet.setTargetObjectID(target);
	packet.setDuration(kDuration);
	packet.setGrade(static_cast<BYTE>(grade));
	packet.addShortData(MODIFY_CURRENT_HP, 55);
	GCSkillToObjectOK1Handler::execute(&packet, nullptr);
}

// A creature's skill on the player.
void ReceiveOnPlayer(ObjectID_t user, int skill, int grade = 0)
{
	GCSkillToObjectOK2 packet;
	packet.setObjectID(user);
	packet.setSkillType(static_cast<SkillType_t>(skill));
	packet.setDuration(kDuration);
	packet.setGrade(static_cast<BYTE>(grade));
	packet.addShortData(MODIFY_CURRENT_HP, 44);
	GCSkillToObjectOK2Handler::execute(&packet, nullptr);
}

// The player's queue landing on the target as the skill's result action.
std::vector<std::string> Adds(TYPE_ACTIONINFO action, TYPE_ACTIONINFO result = 0)
{
	if (result == 0) result = action + kMin;
	return {"Self.adds|9", "Vanda.delay|" + N(result) + "|" + F(), "Vanda.result|" + N(result) + "|1002|20|21|0"};
}

// Ogre facing the player and casting at it; the queue lands on the player.
std::vector<std::string> OnPlayer(TYPE_ACTIONINFO action, TYPE_ACTIONINFO result = 0)
{
	if (result == 0) result = action + kMin;
	return {"Ogre.face|30|31", "Ogre.cast|" + N(action) + "|7", "Self.delay|" + N(result) + "|" + F(), "Self.result|" + N(result) + "|7|30|31|0"};
}

// The calls after the first n.
std::vector<std::string> Drop(std::vector<std::string> all, std::size_t n)
{
	all.erase(all.begin(), all.begin() + static_cast<std::ptrdiff_t>(n));
	return all;
}

long long PlayerHP() { return static_cast<long long>(playerStatus.GetStatus(MODIFY_CURRENT_HP)); }

} // namespace

TEST(SkillObjectReplies, ASkillLandsOnTheTargetWithItsResultAndEffect)
{
	World world;
	Receive(1001, 1002, kSkill);
	CHECK(calls == Cast(kSkill));
	// Its result kind's node, then its effect status, follow the hit.
	calls.clear();
	world.Row(kSkill).SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	world.Row(kSkill).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	Receive(1001, 1002, kSkill);
	CHECK(calls == Then(Cast(kSkill), {"Vanda.burrow", "Vanda.effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|" + F()}));
}

TEST(SkillObjectReplies, AWeaponSkillActsAsTheUsersBasicActionAndAStepAsItsGrade)
{
	World world;
	world.Row(kSkill).SetAffectCurrentWeaponAction();
	Receive(1001, 1002, kSkill);
	CHECK(calls == Cast(SKILL_ATTACK_MELEE));
	// A basic action that is itself a result action: nothing.
	calls.clear();
	world.ogre.basic = kMin;
	Receive(1001, 1002, kSkill);
	CHECK(calls.empty());
	// A skill with steps acts as the step its grade names; grade 0 as itself.
	world.Row(SKILL_SINGLE_BLOW).SetUseActionStep();
	world.Row(SKILL_SINGLE_BLOW).SetActionStep(1, kStep);
	Receive(1001, 1002, SKILL_SINGLE_BLOW, 2);
	CHECK(calls == Cast(kStep));
	calls.clear();
	Receive(1001, 1002, SKILL_SINGLE_BLOW, 0);
	CHECK(calls == Cast(SKILL_SINGLE_BLOW));
}

TEST(SkillObjectReplies, SomeSkillsShowACopyOrASummon)
{
	World world;
	Receive(1001, 1002, SKILL_SHADOW_DANCING);
	CHECK(calls == Then({"Ogre.shadow dancing|Vanda|" + N(SKILL_SINGLE_BLOW)}, Cast(SKILL_SHADOW_DANCING, SKILL_SINGLE_BLOW + kMin)));
	calls.clear();
	Receive(1001, 1002, SKILL_ILLUSION_INVERSION);
	CHECK(calls == Then({"Ogre.illendue|Vanda|" + N(TEMP_SKILL_ILLUSION_INVERSION)}, Cast(SKILL_ILLUSION_INVERSION)));
	calls.clear();
	Receive(1001, 1002, SKILL_ILLENDUE);
	CHECK(calls == Then({"Ogre.illendue|Vanda|" + N(MAGIC_CAUSE_SERIOUS_WOUNDS)}, Cast(SKILL_ILLENDUE, SKILL_LIGHT_BALL + kMin)));
	calls.clear();
	Receive(1001, 1002, SKILL_BLAZE_WALK, 3);
	CHECK(calls == Then({"Ogre.blaze walk|Vanda|" + N(SKILL_BLAZE_WALK_ATTACK) + "|3"}, Cast(SKILL_BLAZE_WALK, SKILL_BLAZE_WALK_ATTACK + kMin)));
	// The rocket flies before the skill is cast.
	calls.clear();
	Receive(1001, 1002, SKILL_PLASMA_ROCKET_LAUNCHER);
	CHECK(calls == Then({"Ogre.rocket|Vanda"}, Cast(SKILL_PLASMA_ROCKET_LAUNCHER)));
}

TEST(SkillObjectReplies, SetAfireMarksTheUserAndBiteOfDeathTheTarget)
{
	World world;
	Receive(1001, 1002, SKILL_SET_AFIRE);
	CHECK(calls == Then({"Ogre.effect|" + N(EFFECTSTATUS_SET_AFIRE_ATTACK) + "|20"}, Cast(SKILL_SET_AFIRE)));
	calls.clear();
	Receive(1001, 1002, SKILL_BITE_OF_DEATH);
	CHECK(calls == Then(Cast(SKILL_BITE_OF_DEATH), {"Vanda.drained by|1001"}));
}

TEST(SkillObjectReplies, TheWildWolfRunsAtTheTargetOrTheCreatureInACorpse)
{
	World world;
	Receive(1001, 1002, SKILL_WILD_WOLF, 1);
	CHECK(calls == Then({"Ogre.wild wolf|Vanda|eat"}, Cast(SKILL_WILD_WOLF)));
	calls.clear();
	Receive(1001, 1002, SKILL_WILD_WOLF, 0);
	CHECK(calls.size() == 5 && calls[0] == "Ogre.wild wolf|Vanda|hunt");
	// At a corpse, the skill goes on against its creature; the queue names
	// the corpse item, which no zone creature answers to.
	calls.clear();
	Receive(1001, 2001, SKILL_WILD_WOLF, 1);
	CHECK(calls == std::vector<std::string>({"Ogre.wild wolf|Fallen|eat", "Ogre.face|25|26", "Ogre.cast|" + N(SKILL_WILD_WOLF) + "|2001"}));
	calls.clear();
	Receive(1001, 2001, SKILL_WILD_WOLF, 0);
	CHECK(calls.size() == 3 && calls[0] == "Ogre.wild wolf|Fallen|hunt");
	// Another skill at a corpse, or the wolf at nothing or without its user:
	// nothing.
	calls.clear();
	Receive(1001, 2001, kSkill);
	Receive(1001, 2002, SKILL_WILD_WOLF, 1);
	Receive(1009, 2001, SKILL_WILD_WOLF, 1);
	Receive(1009, 1002, SKILL_WILD_WOLF, 1);
	CHECK(calls.empty());
}

TEST(SkillObjectReplies, WithoutAZoneTheUserOrTheTargetOrPastTheTableNothingHappens)
{
	World world;
	Receive(1009, 1002, kSkill);
	Receive(1001, 1009, kSkill);
	Receive(1001, 1002, MAX_ACTIONINFO);
	Receive(1001, 1002, MAX_ACTIONINFO + 100);
	// The special skills too.
	for (int skill : {SKILL_SHADOW_DANCING, SKILL_PLASMA_ROCKET_LAUNCHER, SKILL_SET_AFIRE, SKILL_BITE_OF_DEATH})
		Receive(1001, 1009, skill);
	zoneExists = false;
	Receive(1001, 1002, kSkill);
	CHECK(calls.empty());
	// Without the hosts nothing is reached either.
	zoneExists = true;
	ZoneState::SetHost(nullptr);
	CreatureState::SetHost(nullptr);
	WorldEvents::SetHost(nullptr);
	WorldEvents::SkillShadowDancing(Handle(world.ogre), Handle(world.vanda), 1);
	WorldEvents::SkillIllendue(Handle(world.ogre), Handle(world.vanda), 1);
	WorldEvents::SkillBlazeWalk(Handle(world.ogre), Handle(world.vanda), 1, 1);
	WorldEvents::AddRocketLauncher(Handle(world.ogre), Handle(world.vanda));
	WorldEvents::AddWildWolf(Handle(world.ogre), Handle(world.vanda), true);
	Receive(1001, 1002, kSkill);
	CHECK(calls.empty());
}

TEST(SkillObjectReplies, ThePlayersSkillAnswersTheWaitAndRidesOnItsEffect)
{
	World world;
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	ReceivePlayers(1002, kSkill);
	CHECK(calls == Then({"verified"}, Adds(kSkill)));
	CHECK_EQ(55LL, PlayerHP());
	// Not waiting, it lands all the same; with its node and effect after
	// the hit.
	calls.clear();
	world.Row(kSkill).SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	world.Row(kSkill).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	ReceivePlayers(1002, kSkill);
	CHECK(calls == Then(Adds(kSkill), {"Vanda.burrow", "Vanda.effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|" + F()}));
}

TEST(SkillObjectReplies, ThePlayersWeaponSkillStepAndSpecialSkills)
{
	World world;
	world.Row(kSkill).SetAffectCurrentWeaponAction();
	ReceivePlayers(1002, kSkill);
	CHECK(calls == Adds(SKILL_ATTACK_GUN_SR));
	calls.clear();
	me.basic = kMin;
	ReceivePlayers(1002, kSkill);
	CHECK(calls.empty());
	world.Row(SKILL_SINGLE_BLOW).SetUseActionStep();
	world.Row(SKILL_SINGLE_BLOW).SetActionStep(1, kStep);
	ReceivePlayers(1002, SKILL_SINGLE_BLOW, 2);
	CHECK(calls == Adds(kStep));
	calls.clear();
	ReceivePlayers(1002, SKILL_SHADOW_DANCING);
	ReceivePlayers(1002, SKILL_ILLUSION_INVERSION);
	ReceivePlayers(1002, SKILL_ILLENDUE);
	ReceivePlayers(1002, SKILL_BLAZE_WALK, 3);
	ReceivePlayers(1002, SKILL_PLASMA_ROCKET_LAUNCHER);
	CHECK(calls == Then(Then(Then(Then(
		Then({"Self.shadow dancing|Vanda|" + N(SKILL_SINGLE_BLOW)}, Adds(SKILL_SHADOW_DANCING, SKILL_SINGLE_BLOW + kMin)),
		Then({"Self.illendue|Vanda|" + N(TEMP_SKILL_ILLUSION_INVERSION)}, Adds(SKILL_ILLUSION_INVERSION))),
		Then({"Self.illendue|Vanda|" + N(MAGIC_CAUSE_SERIOUS_WOUNDS)}, Adds(SKILL_ILLENDUE, SKILL_LIGHT_BALL + kMin))),
		Then({"Self.blaze walk|Vanda|" + N(SKILL_BLAZE_WALK_ATTACK) + "|3"}, Adds(SKILL_BLAZE_WALK, SKILL_BLAZE_WALK_ATTACK + kMin))),
		Then({"Self.rocket|Vanda"}, Adds(SKILL_PLASMA_ROCKET_LAUNCHER))));
	// Heal Pass also passes from the player to the target, 20 frames on.
	calls.clear();
	ReceivePlayers(1002, SKILL_HEAL_PASS);
	CHECK(calls == Then({"cell|" + N(SKILL_HEAL_PASS) + "|30,31,0|dir 6|1002@20,21,0|20|resume"}, Adds(SKILL_HEAL_PASS)));
	// Bite of Death marks the target before the hit, Set Afire the player.
	calls.clear();
	ReceivePlayers(1002, SKILL_BITE_OF_DEATH);
	ReceivePlayers(1002, SKILL_SET_AFIRE);
	CHECK(calls == Then(Then({"Vanda.drained by|7"}, Adds(SKILL_BITE_OF_DEATH)),
		Then({"Self.has|" + N(EFFECTSTATUS_SET_AFIRE_ATTACK) + "|20"}, Adds(SKILL_SET_AFIRE))));
}

TEST(SkillObjectReplies, ThePlayersWildWolfAndMissingPieces)
{
	World world;
	ReceivePlayers(1002, SKILL_WILD_WOLF, 1);
	CHECK(calls == Then({"Self.wild wolf|Vanda|eat"}, Adds(SKILL_WILD_WOLF)));
	calls.clear();
	ReceivePlayers(1002, SKILL_WILD_WOLF, 0);
	CHECK(calls == Then({"Self.wild wolf|Vanda|hunt"}, Adds(SKILL_WILD_WOLF)));
	// At a corpse it goes on against the corpse's creature, whose ID the
	// zone does not answer to.
	calls.clear();
	ReceivePlayers(2001, SKILL_WILD_WOLF, 0);
	CHECK(calls == std::vector<std::string>({"Self.wild wolf|Fallen|hunt", "Self.adds|9"}));
	// Another skill at a corpse, a missing target, or past the table: only
	// the status changes.
	calls.clear();
	ReceivePlayers(2001, kSkill);
	ReceivePlayers(1009, kSkill);
	ReceivePlayers(1002, MAX_ACTIONINFO);
	CHECK(calls.empty());
	CHECK_EQ(55LL, PlayerHP());
	// Without a zone the wait is still answered and the status changed.
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
	zoneExists = false;
	ReceivePlayers(1002, kSkill);
	CHECK(calls == std::vector<std::string>({"verified"}));
	CHECK_EQ(55LL, PlayerHP());
	// Without a player nothing happens.
	calls.clear();
	zoneExists = true;
	hasPlayer = false;
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
	ReceivePlayers(1002, kSkill);
	CHECK(calls.empty());
	CHECK_EQ(10LL, PlayerHP());
}

TEST(SkillObjectReplies, ASkillOnThePlayerLandsWithItsEffectOrThePendingOne)
{
	World world;
	ReceiveOnPlayer(1001, kSkill);
	CHECK(calls == OnPlayer(kSkill));
	CHECK_EQ(44LL, PlayerHP());
	// The skill's effect status lands before the hit; a pending one on the
	// player takes its place and is cleared.
	calls.clear();
	world.Row(kSkill).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	ReceiveOnPlayer(1001, kSkill);
	CHECK(calls == Then({"Ogre.face|30|31", "Ogre.cast|" + N(kSkill) + "|7", "Self.effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|" + F()},
		Drop(OnPlayer(kSkill), 2)));
	calls.clear();
	playerStatus.SetStatus(MODIFY_EFFECT_STAT, EFFECTSTATUS_BLOOD_DRAIN);
	ReceiveOnPlayer(1001, kSkill);
	CHECK(calls.size() == 5 && calls[2] == "Self.effect|" + N(EFFECTSTATUS_BLOOD_DRAIN) + "|" + F());
	CHECK_EQ(static_cast<long long>(EFFECTSTATUS_NULL), static_cast<long long>(playerStatus.GetEFFECT_STAT()));
	// Its result kind's node lands on the player.
	calls.clear();
	world.Row(kSkill).SetEffectStatus(EFFECTSTATUS_NULL);
	world.Row(kSkill).SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	ReceiveOnPlayer(1001, kSkill);
	CHECK(calls == Then(OnPlayer(kSkill), {"Self.burrow"}));
}

TEST(SkillObjectReplies, ASkillOnThePlayerByUserWeaponStepAndSpecials)
{
	World world;
	world.Row(kSkill).SetAffectCurrentWeaponAction();
	ReceiveOnPlayer(1001, kSkill);
	CHECK(calls == OnPlayer(SKILL_ATTACK_MELEE));
	calls.clear();
	world.ogre.basic = kMin;
	ReceiveOnPlayer(1001, kSkill);
	CHECK(calls.empty());
	world.Row(SKILL_SINGLE_BLOW).SetUseActionStep();
	world.Row(SKILL_SINGLE_BLOW).SetActionStep(1, kStep);
	ReceiveOnPlayer(1001, SKILL_SINGLE_BLOW, 2);
	CHECK(calls == OnPlayer(kStep));
	calls.clear();
	ReceiveOnPlayer(1001, SKILL_SHADOW_DANCING);
	CHECK(calls == Then({"Ogre.shadow dancing|Self|" + N(SKILL_SINGLE_BLOW)}, OnPlayer(SKILL_SHADOW_DANCING, SKILL_SINGLE_BLOW + kMin)));
	calls.clear();
	ReceiveOnPlayer(1001, SKILL_ILLUSION_INVERSION);
	ReceiveOnPlayer(1001, SKILL_ILLENDUE);
	ReceiveOnPlayer(1001, SKILL_BLAZE_WALK, 3);
	CHECK(calls == Then(Then(Then({"Ogre.illendue|Self|" + N(TEMP_SKILL_ILLUSION_INVERSION)}, OnPlayer(SKILL_ILLUSION_INVERSION)),
		Then({"Ogre.illendue|Self|" + N(MAGIC_CAUSE_SERIOUS_WOUNDS)}, OnPlayer(SKILL_ILLENDUE, SKILL_LIGHT_BALL + kMin))),
		Then({"Ogre.blaze walk|Self|" + N(SKILL_BLAZE_WALK_ATTACK) + "|3"}, OnPlayer(SKILL_BLAZE_WALK, SKILL_BLAZE_WALK_ATTACK + kMin))));
	// The rocket flies before anything else; Bite of Death marks the player,
	// Set Afire the user; Absorb Soul lets the user's draining go on.
	calls.clear();
	ReceiveOnPlayer(1001, SKILL_PLASMA_ROCKET_LAUNCHER);
	ReceiveOnPlayer(1001, SKILL_BITE_OF_DEATH);
	ReceiveOnPlayer(1001, SKILL_SET_AFIRE);
	ReceiveOnPlayer(1001, SKILL_ABSORB_SOUL);
	CHECK(calls == Then(Then(Then(Then({"Ogre.rocket|Self"}, OnPlayer(SKILL_PLASMA_ROCKET_LAUNCHER)),
		Then({"Self.drained by|1001"}, OnPlayer(SKILL_BITE_OF_DEATH))),
		Then({"Ogre.effect|" + N(EFFECTSTATUS_SET_AFIRE_ATTACK) + "|20"}, OnPlayer(SKILL_SET_AFIRE))),
		Then({"Ogre.drains on"}, OnPlayer(SKILL_ABSORB_SOUL))));
}

TEST(SkillObjectReplies, ATyphoonHoldsThePlayerAndProtectionTurnsAnAcid)
{
	World world;
	ReceiveOnPlayer(1001, SKILL_TYPHOON, 1);
	ReceiveOnPlayer(1001, SKILL_WILD_TYPHOON, 0);
	CHECK(calls == Then(Then({"Self.held|1000"}, OnPlayer(SKILL_TYPHOON)), Then({"Self.held|1000"}, OnPlayer(SKILL_WILD_TYPHOON))));
	calls.clear();
	ReceiveOnPlayer(1001, SKILL_TYPHOON, 2);
	CHECK(calls == OnPlayer(SKILL_TYPHOON));
	// An acid on a player protected from it is cast as the protection; its
	// queue keeps the acid.
	calls.clear();
	testfw::MutableRow(world.skills, MAGIC_ACID_TOUCH).SetSkillStep(SKILL_STEP_VAMPIRE_ACID);
	playerEffects = {EFFECTSTATUS_PROTECTION_FROM_ACID};
	ReceiveOnPlayer(1001, MAGIC_ACID_TOUCH);
	std::vector<std::string> acid = OnPlayer(MAGIC_ACID_TOUCH);
	acid[1] = "Ogre.cast|" + N(PROTECTION_FROM_ACID) + "|7";
	CHECK(calls == acid);
	// Not protected, or not an acid: as cast.
	calls.clear();
	playerEffects.clear();
	ReceiveOnPlayer(1001, MAGIC_ACID_TOUCH);
	playerEffects = {EFFECTSTATUS_PROTECTION_FROM_ACID};
	ReceiveOnPlayer(1001, kSkill);
	CHECK(calls == Then(OnPlayer(MAGIC_ACID_TOUCH), OnPlayer(kSkill)));
}

TEST(SkillObjectReplies, AnUnseenUsersSkillRisesFromThePlayer)
{
	World world;
	me.z = 4;
	world.Row(kSkill).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	ReceiveOnPlayer(1009, kSkill);
	CHECK(calls == std::vector<std::string>({"cell|" + N(kSkill + kMin) + "|30,31,4|dir 6|7@30,31,4|" + F() + "|resume",
		"Self.effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|" + F(), "Self.delay|" + N(kSkill + kMin) + "|" + F(),
		"Self.result|" + N(kSkill + kMin) + "|7|30|31|0"}));
	// The wolf from a corpse's creature runs at the player, then the skill
	// goes on from that creature, which faces the player and casts.
	calls.clear();
	world.Row(kSkill).SetEffectStatus(EFFECTSTATUS_NULL);
	ReceiveOnPlayer(2001, SKILL_WILD_WOLF, 1);
	CHECK(calls == Then({"Fallen.wild wolf|Self|eat", "Fallen.face|30|31", "Fallen.cast|" + N(SKILL_WILD_WOLF) + "|7"},
		Drop(OnPlayer(SKILL_WILD_WOLF), 2)));
}

TEST(SkillObjectReplies, ASkillOnThePlayerWithoutAZoneOrPastTheTable)
{
	World world;
	ReceiveOnPlayer(1001, MAX_ACTIONINFO);
	CHECK(calls.empty());
	CHECK_EQ(44LL, PlayerHP());
	zoneExists = false;
	playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
	ReceiveOnPlayer(1001, kSkill);
	CHECK(calls.empty());
	CHECK_EQ(44LL, PlayerHP());
	// Without a player nothing happens.
	zoneExists = true;
	hasPlayer = false;
	playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
	ReceiveOnPlayer(1001, kSkill);
	CHECK(calls.empty());
	CHECK_EQ(10LL, PlayerHP());
}

TEST(SkillObjectReplies, AResultActionOnAProtectedPlayerIsNoAcid)
{
	World world;
	// The skill table holds a row per skill, below the result actions; a
	// result action cast at a player protected from acid reads its default
	// row past the end, no acid, and is cast as itself.
	playerEffects = {EFFECTSTATUS_PROTECTION_FROM_ACID};
	ReceiveOnPlayer(1001, kMin + 1);
	CHECK(calls.size() >= 2 && calls[1] == "Ogre.cast|" + N(kMin + 1) + "|7");
}

TEST(SkillObjectReplies, TheRewrittenSkillDecidesTheStepAndTheSpecialsTheCastOneTheRocket)
{
	World world;
	// A weapon skill whose basic action has steps steps as the basic one;
	// on the player the step is taken first, from the skill cast, so the
	// basic action is used as it is.
	world.Row(kSkill).SetAffectCurrentWeaponAction();
	world.Row(SKILL_SINGLE_BLOW).SetUseActionStep();
	world.Row(SKILL_SINGLE_BLOW).SetActionStep(1, kStep);
	world.ogre.basic = SKILL_SINGLE_BLOW;
	me.basic = SKILL_SINGLE_BLOW;
	Receive(1001, 1002, kSkill, 2);
	ReceivePlayers(1002, kSkill, 2);
	ReceiveOnPlayer(1001, kSkill, 2);
	CHECK(calls == Then(Then(Cast(kStep), Adds(kStep)), OnPlayer(SKILL_SINGLE_BLOW)));
	// A step onto Bite of Death or Shadow Dancing is that skill.
	calls.clear();
	world.Row(kStep).SetUseActionStep();
	world.Row(kStep).SetActionStep(1, SKILL_BITE_OF_DEATH);
	world.Row(kStep).SetActionStep(2, SKILL_SHADOW_DANCING);
	Receive(1001, 1002, kStep, 2);
	ReceivePlayers(1002, kStep, 2);
	ReceiveOnPlayer(1001, kStep, 2);
	CHECK(calls == Then(Then(Then(Cast(SKILL_BITE_OF_DEATH), {"Vanda.drained by|1001"}),
		Then({"Vanda.drained by|7"}, Adds(SKILL_BITE_OF_DEATH))),
		Then({"Self.drained by|1001"}, OnPlayer(SKILL_BITE_OF_DEATH))));
	calls.clear();
	Receive(1001, 1002, kStep, 3);
	ReceivePlayers(1002, kStep, 3);
	ReceiveOnPlayer(1001, kStep, 3);
	CHECK(calls == Then(Then(
		Then({"Ogre.shadow dancing|Vanda|" + N(SKILL_SINGLE_BLOW)}, Cast(SKILL_SHADOW_DANCING, SKILL_SINGLE_BLOW + kMin)),
		Then({"Self.shadow dancing|Vanda|" + N(SKILL_SINGLE_BLOW)}, Adds(SKILL_SHADOW_DANCING, SKILL_SINGLE_BLOW + kMin))),
		Then({"Ogre.shadow dancing|Self|" + N(SKILL_SINGLE_BLOW)}, OnPlayer(SKILL_SHADOW_DANCING, SKILL_SINGLE_BLOW + kMin))));
	// The rocket flies for the skill cast, even one that acts as the
	// weapon's.
	calls.clear();
	world.Row(SKILL_PLASMA_ROCKET_LAUNCHER).SetAffectCurrentWeaponAction();
	world.ogre.basic = SKILL_ATTACK_MELEE;
	me.basic = SKILL_ATTACK_MELEE;
	Receive(1001, 1002, SKILL_PLASMA_ROCKET_LAUNCHER);
	ReceivePlayers(1002, SKILL_PLASMA_ROCKET_LAUNCHER);
	ReceiveOnPlayer(1001, SKILL_PLASMA_ROCKET_LAUNCHER);
	CHECK(calls == Then(Then(Then({"Ogre.rocket|Vanda"}, Cast(SKILL_ATTACK_MELEE)), Then({"Self.rocket|Vanda"}, Adds(SKILL_ATTACK_MELEE))),
		Then({"Ogre.rocket|Self"}, OnPlayer(SKILL_ATTACK_MELEE))));
}

TEST(SkillObjectReplies, TheWildWolfOnThePlayerAndEachCorpseAtEitherGrade)
{
	World world;
	// From a live user, eating or hunting by grade.
	ReceiveOnPlayer(1001, SKILL_WILD_WOLF, 1);
	ReceiveOnPlayer(1001, SKILL_WILD_WOLF, 0);
	CHECK(calls == Then(Then({"Ogre.wild wolf|Self|eat"}, OnPlayer(SKILL_WILD_WOLF)), Then({"Ogre.wild wolf|Self|hunt"}, OnPlayer(SKILL_WILD_WOLF))));
	// From or at a corpse, at the grade not covered elsewhere.
	calls.clear();
	ReceiveOnPlayer(2001, SKILL_WILD_WOLF, 0);
	ReceivePlayers(2001, SKILL_WILD_WOLF, 1);
	CHECK(calls.size() == 7 && calls[0] == "Fallen.wild wolf|Self|hunt" && calls[5] == "Self.wild wolf|Fallen|eat");
}

TEST(SkillObjectReplies, AnUnseenUsersWeaponSkillIsCastAsSent)
{
	World world;
	world.Row(kSkill).SetAffectCurrentWeaponAction();
	ReceiveOnPlayer(1009, kSkill);
	CHECK(calls.size() == 3 && calls[0] == "cell|" + N(kSkill + kMin) + "|30,31,0|dir 6|7@30,31,0|" + F() + "|resume");
}

TEST(SkillObjectReplies, BlitzSlidingLandsOnItsUser)
{
	World world;
	// Its result shows on the user the queue names: the player for its
	// own skill, the caster otherwise.
	const TYPE_ACTIONINFO result = SKILL_BLITZ_SLIDING_ATTACK + kMin;
	Receive(1001, 1002, SKILL_BLITZ_SLIDING_ATTACK);
	ReceivePlayers(1002, SKILL_BLITZ_SLIDING_ATTACK);
	ReceiveOnPlayer(1001, SKILL_BLITZ_SLIDING_ATTACK);
	CHECK(calls == std::vector<std::string>({"Ogre.face|20|21", "Ogre.cast|" + N(SKILL_BLITZ_SLIDING_ATTACK) + "|1002",
		"Vanda.delay|" + N(result) + "|" + F(), "Ogre.result|" + N(result) + "|1002|20|21|0",
		"Self.adds|9", "Vanda.delay|" + N(result) + "|" + F(), "Self.result|" + N(result) + "|1002|20|21|0",
		"Ogre.face|30|31", "Ogre.cast|" + N(SKILL_BLITZ_SLIDING_ATTACK) + "|7",
		"Self.delay|" + N(result) + "|" + F(), "Ogre.result|" + N(result) + "|7|30|31|0"}));
}
