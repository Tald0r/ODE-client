#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "GameInfoPresentationHost.h"
#include "MActionInfoTable.h"
#include "MSkillManager.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCSkillFailed1.h"
#include "Gpackets/GCSkillFailed2.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// The player's skill failed: the wait is answered, a skill to the
// inventory leaves the item check buffer, an ousters' soul absorption or
// a blood drain stops, a slayer's Eternity waits out its delay, Soul Chain
// cannot trace, Burning Soul loses its charge, and the skill is ready
// again at once (Will of Life after its level's delay) unless its row
// ignores a failure; the status changes last. Another creature's skill
// failed: it shows the skill (its weapon's for a plain attack, its step's
// for a grade) failing on its target, on nobody at its own cell, on
// itself, or at its cell; an ousters absorbing a soul finds the soul in a
// corpse and stops absorbing; a blood drain stops; a grenade shows
// nothing.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool hasPlayer = true;
PlayerState::WAIT_VERIFY waitVerify = PlayerState::WAIT_VERIFY_NULL;
PlayerState::ITEM_CHECK_BUFFER checkStatus = PlayerState::ITEM_CHECK_BUFFER_NULL;
MStatus playerStatus;
MonotonicClock::TimePoint now{MonotonicClock::Duration(1000)};

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	std::string name;
	TYPE_SECTORPOSITION x = 0;
	TYPE_SECTORPOSITION y = 0;
	TYPE_ACTIONINFO basic = 0;
	bool ousters = false;
	bool slayer = false;
};
std::map<TYPE_OBJECTID, FakeCreature*> creatures;
// Corpse item ID -> the creature it holds; item 9002 is no corpse.
std::map<TYPE_OBJECTID, FakeCreature*> corpses;
FakeCreature me;

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* pCreature)
{
	static FakeCreature none;
	CHECK(pCreature != nullptr);
	return pCreature == nullptr ? none : *reinterpret_cast<FakeCreature*>(pCreature);
}
void Record(MCreature* p, const std::string& what) { calls.push_back(Fake(p).name + "." + what); }
MCreature* Find(std::map<TYPE_OBJECTID, FakeCreature*>& from, TYPE_OBJECTID id)
{
	const auto found = from.find(id);
	return found == from.end() ? nullptr : Handle(*found->second);
}

const MItemHost items = []() {
	MItemHost host{};
	host.pCurrentTime = &now;
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) -> MCreature* {
		CHECK(zoneExists);
		return Find(creatures, id);
	};
	host.GetCorpseCreature = [](TYPE_OBJECTID id) -> MCreature* { return Find(corpses, id); };
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? Handle(me) : nullptr; };
	host.Status = []() { return hasPlayer ? &playerStatus : nullptr; };
	host.GetWaitVerify = []() { return waitVerify; };
	host.SetWaitVerifyNULL = []() {
		calls.push_back("verified");
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
	};
	host.GetItemCheckBufferStatus = []() { return checkStatus; };
	host.ClearItemCheckBuffer = []() { calls.push_back("buffer cleared"); };
	host.SetCannotTrace = []() { calls.push_back("cannot trace"); };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.GetBasicActionInfo = [](MCreature* p) { return Fake(p).basic; };
	host.IsOusters = [](MCreature* p) { return Fake(p).ousters; };
	host.IsSlayer = [](MCreature* p) { return Fake(p).slayer; };
	host.SetStopBloodDrain = [](MCreature* p) { Record(p, "stop drain"); };
	host.SetStopAbsorbSoul = [](MCreature* p) { Record(p, "stop absorb"); };
	host.RemoveEffectStatus = [](MCreature* p, EFFECTSTATUS status) {
		Record(p, "remove|" + N(status));
		return true;
	};
	host.SetDirectionToPosition = [](MCreature* p, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) { Record(p, "face|" + N(sX) + "|" + N(sY)); };
	host.PacketSpecialActionToNobody = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		Record(p, "nobody|" + N(ai) + "|" + N(x) + "|" + N(y));
	};
	host.PacketSpecialActionToSelf = [](MCreature* p, TYPE_ACTIONINFO ai, MActionResult* pResult) {
		CHECK(pResult == nullptr);
		Record(p, "self|" + N(ai));
	};
	host.PacketSpecialActionToOther = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_OBJECTID id, MActionResult* pResult) {
		CHECK(pResult == nullptr);
		Record(p, "other|" + N(ai) + "|" + N(id));
	};
	host.PacketSpecialActionToSector = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, MActionResult* pResult) {
		CHECK(pResult == nullptr);
		Record(p, "sector|" + N(ai) + "|" + N(sX) + "|" + N(sY));
	};
	return host;
}();

const GameInfoPresentation::Host info = []() {
	GameInfoPresentation::Host host{};
	host.GetCharInfoLevel = []() { return 40; };
	host.SetDelayEternity = []() { calls.push_back("eternity waits"); };
	return host;
}();

constexpr TYPE_ACTIONINFO kMin = MIN_RESULT_ACTIONINFO;
constexpr ACTIONINFO kSkill = SKILL_DOUBLE_IMPACT;
constexpr TYPE_ACTIONINFO kStep = SKILL_TRIPLE_SLASHER;

struct World : GameModelWorld
{
	FakeCreature ogre, elf, lying;
	MActionInfoTable actions;
	MSkillInfoTable skills;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	MSkillInfoTable* oldSkills = g_pSkillInfoTable;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const GameInfoPresentation::Host* oldInfo = GameInfoPresentation::SetHost(&info);
	World()
	{
		me = {7, "Self", 30, 31, SKILL_ATTACK_GUN_SR};
		ogre = {1001, "Ogre", 10, 11, SKILL_ATTACK_GUN_SG};
		elf = {1002, "Elf", 20, 21, SKILL_ATTACK_MELEE, true};
		lying = {5001, "Lying", 40, 41};
		creatures = {{ogre.id, &ogre}, {elf.id, &elf}, {lying.id, &lying}, {me.id, &me}};
		corpses = {{9001, &lying}};
		zoneExists = true;
		hasPlayer = true;
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
		checkStatus = PlayerState::ITEM_CHECK_BUFFER_NULL;
		playerStatus = MStatus();
		playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
		calls.clear();
		MItem::SetHost(&items);
		actions.Init(MAX_ACTIONINFO);
		actions.SetMinResultActionInfo(kMin);
		g_pActionInfoTable = &actions;
		g_pSkillInfoTable = &skills;
	}
	~World()
	{
		GameInfoPresentation::SetHost(oldInfo);
		PlayerState::SetHost(oldPlayer);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		MItem::SetHost(nullptr);
		g_pSkillInfoTable = oldSkills;
		g_pActionInfoTable = oldActions;
		creatures.clear();
		corpses.clear();
	}
	MActionInfo& Row(TYPE_ACTIONINFO action) { return testfw::MutableRow(actions, action); }
	void Target(TYPE_ACTIONINFO action, BYTE flags) { Row(action).Set("skill", 0, 0, 0, flags); }
	SKILLINFO_NODE& Skill(ACTIONINFO skill) { return *skills.GetMutable(skill); }
	static long long HP() { return static_cast<long long>(playerStatus.GetStatus(MODIFY_CURRENT_HP)); }
};

template<class PacketType>
void Decode(PacketType& packet, const PacketType& original)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	packet.read(stream);
	CHECK(stream.isEmpty());
}

void ReceivePlayersFailure(int skill)
{
	GCSkillFailed1 original;
	original.setSkillType(static_cast<SkillType_t>(skill));
	original.addShortData(MODIFY_CURRENT_HP, 55);
	GCSkillFailed1 packet;
	Decode(packet, original);
	GCSkillFailed1Handler::execute(&packet, nullptr);
}

void ReceiveFailure(ObjectID_t user, ObjectID_t target, int skill, int grade = 0)
{
	GCSkillFailed2 original;
	original.setObjectID(user);
	original.setTargetObjectID(target);
	original.setSkillType(static_cast<SkillType_t>(skill));
	original.setGrade(static_cast<BYTE>(grade));
	GCSkillFailed2 packet;
	Decode(packet, original);
	GCSkillFailed2Handler::execute(&packet, nullptr);
}

} // namespace

TEST(SkillFailedReplies, ThePlayersFailedSkillIsReadyAgainAndTheStatusChanges)
{
	World world;
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	world.Skill(kSkill).SetDelayTime(5000);
	world.Skill(kSkill).SetNextAvailableTime();
	CHECK(!world.Skill(kSkill).IsAvailableTime());
	ReceivePlayersFailure(kSkill);
	CHECK(calls == std::vector<std::string>{"verified"});
	CHECK(world.Skill(kSkill).IsAvailableTime());
	CHECK_EQ(55, World::HP());
	// A row that ignores a failure keeps the skill waiting.
	world.Skill(kSkill).SetNextAvailableTime();
	world.Row(kSkill).SetSkillFailDelay();
	ReceivePlayersFailure(kSkill);
	CHECK(!world.Skill(kSkill).IsAvailableTime());
}

TEST(SkillFailedReplies, WillOfLifeWaitsItsLevelsDelay)
{
	World world;
	ReceivePlayersFailure(SKILL_WILL_OF_LIFE);
	const int delay = GetWillOfLifeDelay(40);
	CHECK(delay > 0);
	CHECK_EQ(static_cast<long long>(delay), static_cast<long long>(world.Skill(SKILL_WILL_OF_LIFE).GetAvailableTimeLeft()));
}

TEST(SkillFailedReplies, ThePlayersFailedSkillUndoesWhatItStarted)
{
	World world;
	checkStatus = PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY;
	ReceivePlayersFailure(kSkill);
	checkStatus = PlayerState::ITEM_CHECK_BUFFER_NULL;
	me.ousters = true;
	ReceivePlayersFailure(SKILL_ABSORB_SOUL);
	me.ousters = false;
	ReceivePlayersFailure(SKILL_ABSORB_SOUL);
	me.slayer = true;
	ReceivePlayersFailure(SKILL_ETERNITY);
	me.slayer = false;
	ReceivePlayersFailure(SKILL_ETERNITY);
	ReceivePlayersFailure(SKILL_BLOOD_DRAIN);
	ReceivePlayersFailure(SKILL_BITE_OF_DEATH);
	ReceivePlayersFailure(SKILL_SOUL_CHAIN);
	ReceivePlayersFailure(SKILL_BURNING_SOUL_CHARGING);
	CHECK(calls == std::vector<std::string>({"buffer cleared", "Self.stop absorb", "eternity waits", "Self.stop drain", "Self.stop drain",
		"cannot trace", "Self.remove|" + N(EFFECTSTATUS_BURNING_SOL_CHARGE_1)}));
	// The buffer comes first: a soul absorption with a skill to the
	// inventory pending only clears the buffer.
	calls.clear();
	me.ousters = true;
	checkStatus = PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY;
	ReceivePlayersFailure(SKILL_ABSORB_SOUL);
	CHECK(calls == std::vector<std::string>{"buffer cleared"});
}

TEST(SkillFailedReplies, ThePlayersFailureGuards)
{
	World world;
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	// Past the table, only the wait is answered.
	ReceivePlayersFailure(static_cast<int>(world.actions.GetSize()));
	CHECK(calls == std::vector<std::string>{"verified"});
	CHECK_EQ(10, World::HP());
	calls.clear();
	hasPlayer = false;
	ReceivePlayersFailure(SKILL_SOUL_CHAIN);
	CHECK(calls.empty());
	CHECK_EQ(10, World::HP());
}

TEST(SkillFailedReplies, ACreaturesFailedSkillShowsOnItsTarget)
{
	World world;
	world.Target(kSkill, FLAG_ACTIONINFO_TARGET_OTHER);
	ReceiveFailure(1001, 1002, kSkill);
	CHECK(calls == std::vector<std::string>({"Ogre.face|20|21", "Ogre.other|" + N(kSkill) + "|1002"}));
	// Nobody there: on nobody at its own cell; itself: on itself.
	calls.clear();
	ReceiveFailure(1001, 4242, kSkill);
	ReceiveFailure(1001, 1001, kSkill);
	CHECK(calls == std::vector<std::string>({"Ogre.nobody|" + N(kSkill) + "|10|11", "Ogre.self|" + N(kSkill)}));
	// A skill on itself, or at its cell.
	calls.clear();
	world.Target(kSkill, FLAG_ACTIONINFO_TARGET_SELF);
	ReceiveFailure(1001, 1002, kSkill);
	world.Target(kSkill, FLAG_ACTIONINFO_TARGET_ZONE);
	ReceiveFailure(1001, 1002, kSkill);
	CHECK(calls == std::vector<std::string>({"Ogre.self|" + N(kSkill), "Ogre.sector|" + N(kSkill) + "|10|11"}));
}

TEST(SkillFailedReplies, ACreaturesPlainAttackIsItsWeaponsAndAGradedOneItsStep)
{
	World world;
	world.Target(SKILL_ATTACK_GUN_SG, FLAG_ACTIONINFO_TARGET_SELF);
	ReceiveFailure(1001, 1002, SKILL_ATTACK_MELEE);
	ReceiveFailure(1001, 1002, SKILL_ATTACK_ARMS);
	CHECK(calls == std::vector<std::string>(2, "Ogre.self|" + N(SKILL_ATTACK_GUN_SG)));
	// A weapon whose action is a result shows nothing, though the result's
	// row has a target.
	calls.clear();
	world.Target(kMin, FLAG_ACTIONINFO_TARGET_SELF);
	world.ogre.basic = kMin;
	ReceiveFailure(1001, 1002, SKILL_ATTACK_MELEE);
	CHECK(calls.empty());
	// SetActionStep keeps a step only once the row uses steps.
	world.Target(kSkill, FLAG_ACTIONINFO_TARGET_ZONE);
	world.Target(kStep, FLAG_ACTIONINFO_TARGET_SELF);
	world.Row(kSkill).SetUseActionStep();
	world.Row(kSkill).SetActionStep(1, kStep);
	ReceiveFailure(1001, 1002, kSkill, 2);
	// Grade 0 is the skill itself.
	ReceiveFailure(1001, 1002, kSkill, 0);
	CHECK(calls == std::vector<std::string>({"Ogre.self|" + N(kStep), "Ogre.sector|" + N(kSkill) + "|10|11"}));
}

TEST(SkillFailedReplies, AnOustersAbsorbingASoulFindsItInACorpseAndStops)
{
	World world;
	world.Target(SKILL_ABSORB_SOUL, FLAG_ACTIONINFO_TARGET_OTHER);
	ReceiveFailure(1002, 9001, SKILL_ABSORB_SOUL);
	CHECK(calls == std::vector<std::string>({"Elf.stop absorb", "Elf.face|40|41", "Elf.other|" + N(SKILL_ABSORB_SOUL) + "|9001"}));
	// Not an ousters: no corpse is looked for, nothing stops.
	calls.clear();
	ReceiveFailure(1001, 9001, SKILL_ABSORB_SOUL);
	CHECK(calls == std::vector<std::string>{"Ogre.nobody|" + N(SKILL_ABSORB_SOUL) + "|10|11"});
}

TEST(SkillFailedReplies, OnlyAnOustersAbsorbingASoulWithNoLivingTargetLooksForACorpse)
{
	World world;
	world.Target(SKILL_ABSORB_SOUL, FLAG_ACTIONINFO_TARGET_OTHER);
	world.Target(kSkill, FLAG_ACTIONINFO_TARGET_OTHER);
	// A living target is absorbed from as it stands.
	ReceiveFailure(1002, 1001, SKILL_ABSORB_SOUL);
	CHECK(calls == std::vector<std::string>({"Elf.stop absorb", "Elf.face|10|11", "Elf.other|" + N(SKILL_ABSORB_SOUL) + "|1001"}));
	// Another skill of an ousters at a corpse finds nobody.
	calls.clear();
	ReceiveFailure(1002, 9001, kSkill);
	CHECK(calls == std::vector<std::string>{"Elf.nobody|" + N(kSkill) + "|20|21"});
	// With no caster, no corpse is looked for.
	calls.clear();
	ReceiveFailure(4242, 9001, SKILL_ABSORB_SOUL);
	CHECK(calls.empty());
}

TEST(SkillFailedReplies, AnItemThatIsNoCorpseHoldsNoSoul)
{
	World world;
	world.Target(SKILL_ABSORB_SOUL, FLAG_ACTIONINFO_TARGET_OTHER);
	// Item 9002 is no corpse; the reply read it as one before.
	ReceiveFailure(1002, 9002, SKILL_ABSORB_SOUL);
	CHECK(calls == std::vector<std::string>({"Elf.stop absorb", "Elf.nobody|" + N(SKILL_ABSORB_SOUL) + "|20|21"}));
}

TEST(SkillFailedReplies, ACreaturesBloodDrainStopsAndAGrenadeShowsNothing)
{
	World world;
	world.Target(SKILL_BLOOD_DRAIN, FLAG_ACTIONINFO_TARGET_OTHER);
	world.Target(SKILL_BITE_OF_DEATH, FLAG_ACTIONINFO_TARGET_OTHER);
	ReceiveFailure(1001, 1002, SKILL_BLOOD_DRAIN);
	ReceiveFailure(1001, 1002, SKILL_BITE_OF_DEATH);
	CHECK(calls == std::vector<std::string>({"Ogre.stop drain", "Ogre.face|20|21", "Ogre.other|" + N(SKILL_BLOOD_DRAIN) + "|1002",
		"Ogre.stop drain", "Ogre.face|20|21", "Ogre.other|" + N(SKILL_BITE_OF_DEATH) + "|1002"}));
	calls.clear();
	world.Target(SKILL_GRENADE_ATTACK, FLAG_ACTIONINFO_TARGET_OTHER);
	ReceiveFailure(1001, 1002, SKILL_GRENADE_ATTACK);
	CHECK(calls.empty());
}

TEST(SkillFailedReplies, ACreaturesFailureGuards)
{
	World world;
	world.Target(kSkill, FLAG_ACTIONINFO_TARGET_SELF);
	ReceiveFailure(4242, 1002, kSkill);
	ReceiveFailure(1001, 1002, static_cast<int>(world.actions.GetSize()));
	zoneExists = false;
	ReceiveFailure(1001, 1002, kSkill);
	CHECK(calls.empty());
}
