#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "ConvertDuration.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCSkillToObjectOK5.h"
#include <map>
#include <string>
#include <vector>

// A creature's skill landing on another creature, neither the player
// (OK5). The user faces the target's cell and casts the skill at it; its
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
// is refused.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	std::string name;
	TYPE_SECTORPOSITION x = 0;
	TYPE_SECTORPOSITION y = 0;
	TYPE_ACTIONINFO basic = 0;
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
	host.Creature = []() { return Handle(me); };
	host.GetID = []() { return me.id; };
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
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		me = {7, "Self", 30, 31, SKILL_ATTACK_GUN_SR};
		ogre = {1001, "Ogre", 10, 11, SKILL_ATTACK_MELEE};
		vanda = {1002, "Vanda", 20, 21, SKILL_ATTACK_GUN_SG};
		// Lying in corpse item 2001, outside the zone's creatures.
		fallen = {1500, "Fallen", 25, 26, SKILL_ATTACK_MELEE};
		creatures = {{ogre.id, &ogre}, {vanda.id, &vanda}};
		corpses = {{2001, &fallen}};
		zoneExists = true;
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
	world.Row(kStep).SetActionResult(ACTIONRESULTNODE_NULL);
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
