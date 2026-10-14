#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "ConvertDuration.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCSkillToTileOK1.h"
#include "Gpackets/GCSkillToTileOK2.h"
#include "Gpackets/GCSkillToTileOK5.h"
#include "Gpackets/GCSkillToTileOK6.h"
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

// A skill cast at a cell. Its queue hits the cell (the caster, or nobody
// when the skill's result is an action of its own), then for each creature
// it lands on adds the skill's effect status and the node its result kind
// names. A weapon skill acts as the caster's basic action (not Piercing);
// a skill with steps acts as the step its grade names; a graded skill's
// cell node carries the range as its grade. The player's skill answers the
// wait and goes to the player's current action (OK1). Another creature's
// is cast from the caster facing the cell (OK2 near the player, OK5
// elsewhere); without the caster near the player it starts at the cell,
// and without one at all from where the server says it came (OK6). The
// player's pending effect status rides on a skill near the player. The
// test hosts run every handed-over queue at once.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool hasPlayer = true;
PlayerState::WAIT_VERIFY waitVerify = PlayerState::WAIT_VERIFY_NULL;
MStatus playerStatus;

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	std::string name;
	TYPE_SECTORPOSITION x = 0;
	TYPE_SECTORPOSITION y = 0;
	TYPE_ACTIONINFO basic = 0;
	int direction = 0;
};
std::map<TYPE_OBJECTID, FakeCreature*> creatures;
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

void Land(MActionResult* pResult)
{
	if (pResult == nullptr) return;
	pResult->Execute();
	delete pResult;
}

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) -> MCreature* {
		CHECK(zoneExists);
		const auto found = creatures.find(id);
		return found == creatures.end() ? nullptr : Handle(*found->second);
	};
	return host;
}();

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
	host.SetTempSelectPos = [](int x, int y) { calls.push_back("temp|" + N(x) + "|" + N(y)); };
	host.PacketAddActionResult = [](std::uint16_t effectID, MActionResult* pResult) {
		calls.push_back("player.add|" + N(effectID));
		Land(pResult);
		return true;
	};
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.GetDirection = [](MCreature* p) { return Fake(p).direction; };
	host.GetBasicActionInfo = [](MCreature* p) { return Fake(p).basic; };
	host.SetDirectionToPosition = [](MCreature* p, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) { Record(p, "face|" + N(sX) + "|" + N(sY)); };
	host.SetDirection = [](MCreature* p, int direction) { Record(p, "dir|" + N(direction)); };
	host.SetEffectDelayFrame = [](MCreature* p, TYPE_ACTIONINFO ai, std::uint32_t frames) { Record(p, "delay|" + N(ai) + "|" + N(frames)); };
	host.SetBloodyZenith = [](MCreature* p, int delay) { Record(p, "zenith|" + N(delay)); };
	host.PacketSpecialActionResult = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t grade) {
		Record(p, "result|" + N(ai) + "|" + N(id) + "|" + N(sX) + "|" + N(sY) + "|" + N(grade));
	};
	host.AddEffectStatus = [](MCreature* p, EFFECTSTATUS status, std::uint32_t delayFrame) {
		Record(p, "effect|" + N(status) + "|" + N(delayFrame));
		return true;
	};
	host.SetUndergroundCreature = [](MCreature* p) { Record(p, "burrow"); };
	host.PacketSpecialActionToSector = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, MActionResult* pResult) {
		Record(p, "sector|" + N(ai) + "|" + N(sX) + "|" + N(sY));
		Land(pResult);
	};
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int startZ, int direction,
		TYPE_OBJECTID targetID, TYPE_SECTORPOSITION targetX, TYPE_SECTORPOSITION targetY, int targetZ, std::uint32_t delayFrame,
		MActionResult* pResult, bool bStartFromMainNode, int sX, int sY) {
		CHECK(startZ == 0 && targetZ == 0 && sX == 0 && sY == 0);
		calls.push_back("cell|" + N(actionInfo) + "|" + N(startX) + "," + N(startY) + "|dir " + N(direction) + "|" + N(targetID)
			+ "@" + N(targetX) + "," + N(targetY) + "|" + N(delayFrame) + (bStartFromMainNode ? "|main" : "|resume"));
		Land(pResult);
	};
	return host;
}();

constexpr TYPE_ACTIONINFO kMin = MIN_RESULT_ACTIONINFO;
constexpr TYPE_ACTIONINFO kSkill = SKILL_DOUBLE_IMPACT;
constexpr TYPE_ACTIONINFO kStep = SKILL_TRIPLE_SLASHER;
constexpr int kDuration = 30;
constexpr int kRange = 3;

struct World : GameModelWorld
{
	FakeCreature ogre, vanda;
	MActionInfoTable actions;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		me = {7, "Self", 30, 31, SKILL_ATTACK_GUN_SR, 6};
		ogre = {1001, "Ogre", 10, 11, SKILL_ATTACK_MELEE, 3};
		vanda = {1002, "Vanda", 20, 21, SKILL_ATTACK_GUN_SG, 5};
		creatures = {{ogre.id, &ogre}, {vanda.id, &vanda}, {me.id, &me}};
		zoneExists = true;
		hasPlayer = true;
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
		playerStatus = MStatus();
		playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
		playerStatus.SetStatus(MODIFY_EFFECT_STAT, EFFECTSTATUS_NULL);
		calls.clear();
		actions.Init(MAX_ACTIONINFO);
		actions.SetMinResultActionInfo(kMin);
		g_pActionInfoTable = &actions;
	}
	~World()
	{
		WorldEvents::SetHost(oldEvents);
		PlayerState::SetHost(oldPlayer);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		g_pActionInfoTable = oldActions;
		creatures.clear();
	}
	MActionInfo& Row(TYPE_ACTIONINFO action) { return testfw::MutableRow(actions, action); }
	void Steps(TYPE_ACTIONINFO action)
	{
		Row(action).SetUseActionStep();
		Row(action).SetActionStep(1, kStep);
	}
	static long long HP() { return static_cast<long long>(playerStatus.GetStatus(MODIFY_CURRENT_HP)); }
};

std::string F() { return N(ConvertDurationToFrame(kDuration)); }

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

template<class PacketType>
void Fill(PacketType& packet, int skill, std::initializer_list<ObjectID_t> targets, int grade, int range)
{
	packet.setSkillType(static_cast<SkillType_t>(skill));
	packet.setX(40);
	packet.setY(41);
	packet.setRange(static_cast<Range_t>(range));
	packet.setDuration(kDuration);
	packet.setGrade(static_cast<BYTE>(grade));
	for (ObjectID_t id : targets) packet.addCListElement(id);
}

void ReceivePlayers(int skill, std::initializer_list<ObjectID_t> targets, int grade = 0, int range = kRange)
{
	GCSkillToTileOK1 original;
	Fill(original, skill, targets, grade, range);
	original.setCEffectID(9);
	original.addShortData(MODIFY_CURRENT_HP, 55);
	GCSkillToTileOK1 packet;
	Decode(packet, original);
	GCSkillToTileOK1Handler::execute(&packet, nullptr);
}

void ReceiveNearPlayer(ObjectID_t user, int skill, std::initializer_list<ObjectID_t> targets, int grade = 0, int range = kRange)
{
	GCSkillToTileOK2 original;
	Fill(original, skill, targets, grade, range);
	original.setObjectID(user);
	original.addShortData(MODIFY_CURRENT_HP, 44);
	GCSkillToTileOK2 packet;
	Decode(packet, original);
	GCSkillToTileOK2Handler::execute(&packet, nullptr);
}

void ReceiveElsewhere(ObjectID_t user, int skill, std::initializer_list<ObjectID_t> targets, int grade = 0, int range = kRange)
{
	GCSkillToTileOK5 original;
	Fill(original, skill, targets, grade, range);
	original.setObjectID(user);
	GCSkillToTileOK5 packet;
	Decode(packet, original);
	GCSkillToTileOK5Handler::execute(&packet, nullptr);
}

void ReceiveUnseen(int skill, std::initializer_list<ObjectID_t> targets, int grade = 0, int range = kRange)
{
	GCSkillToTileOK6 original;
	Fill(original, skill, targets, grade, range);
	original.setOrgXY(50, 51);
	original.addShortData(MODIFY_CURRENT_HP, 33);
	GCSkillToTileOK6 packet;
	Decode(packet, original);
	GCSkillToTileOK6Handler::execute(&packet, nullptr);
}

// An action node's two records on its target; `user`, when given, is
// Bloody Zenith's record on its user between them.
std::vector<std::string> Hit(const char* who, TYPE_ACTIONINFO action, TYPE_OBJECTID id, int x, int y, int grade = 0, const char* user = nullptr)
{
	std::vector<std::string> out = {std::string(who) + ".delay|" + N(action + kMin) + "|" + F()};
	if (user != nullptr) out.push_back(std::string(user) + ".zenith|8");
	out.push_back(std::string(who) + ".result|" + N(action + kMin) + "|" + N(id) + "|" + N(x) + "|" + N(y) + "|" + N(grade));
	return out;
}

std::vector<std::string> Join(std::initializer_list<std::vector<std::string>> parts)
{
	std::vector<std::string> out;
	for (const std::vector<std::string>& part : parts) out.insert(out.end(), part.begin(), part.end());
	return out;
}

std::string Effect(const char* who, EFFECTSTATUS status) { return std::string(who) + ".effect|" + N(status) + "|" + F(); }

std::vector<std::string> CastBy(const char* who, TYPE_ACTIONINFO skill, int direction = -1)
{
	std::vector<std::string> out = {std::string(who) + ".face|40|41"};
	if (direction >= 0) out.push_back(std::string(who) + ".dir|" + N(direction));
	out.push_back(std::string(who) + ".sector|" + N(skill) + "|40|41");
	return out;
}

std::string AtCell(TYPE_ACTIONINFO action, int startX, int startY, int direction, TYPE_OBJECTID target)
{
	return "cell|" + N(action) + "|" + N(startX) + "," + N(startY) + "|dir " + N(direction) + "|" + N(target) + "@40,41|" + F() + "|resume";
}

} // namespace

TEST(SkillTileReplies, ThePlayersSkillHitsItsCellAndEachCreatureItLandsOn)
{
	World world;
	world.Row(kSkill).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	world.Row(kSkill).SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	ReceivePlayers(kSkill, {1001, 4242});
	CHECK(calls == Join({{"verified", "player.add|9"}, Hit("Self", kSkill, 7, 40, 41),
		{Effect("Ogre", EFFECTSTATUS_CURSE_PARALYSIS), "Ogre.burrow"}}));
	CHECK_EQ(55, World::HP());
	// A skill whose range is its direction turns the player.
	calls.clear();
	world.Row(kSkill).SetOptionRangeToDirection();
	ReceivePlayers(kSkill, {});
	CHECK(calls == Join({{"Self.dir|3", "player.add|9"}, Hit("Self", kSkill, 7, 40, 41)}));
}

TEST(SkillTileReplies, ThePlayersActionSkillHitsNobodyAtTheCellAndActsOnEachCreature)
{
	World world;
	world.Row(kSkill).SetActionResult(ACTIONRESULTNODE_ACTIONINFO);
	ReceivePlayers(kSkill, {1001});
	CHECK(calls == Join({{"player.add|9"}, Hit("Ogre", kSkill, 1001, 10, 11)}));
	// A weapon skill acts as the player's basic action; Piercing does not.
	calls.clear();
	world.Row(kSkill).SetAffectCurrentWeaponAction();
	ReceivePlayers(kSkill, {1001});
	CHECK(calls == Join({{"player.add|9"}, Hit("Ogre", SKILL_ATTACK_GUN_SR, 1001, 10, 11)}));
	calls.clear();
	world.Row(SKILL_PIERCING).SetAffectCurrentWeaponAction();
	world.Row(SKILL_PIERCING).SetActionResult(ACTIONRESULTNODE_ACTIONINFO);
	ReceivePlayers(SKILL_PIERCING, {1001});
	CHECK(calls == Join({{"player.add|9"}, Hit("Ogre", SKILL_PIERCING, 1001, 10, 11)}));
	// A basic action that is a result shows nothing; the status still changes.
	calls.clear();
	me.basic = kMin;
	playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
	ReceivePlayers(kSkill, {1001});
	CHECK(calls.empty());
	CHECK_EQ(55, World::HP());
}

TEST(SkillTileReplies, ThePlayersSkillActsAsItsStepAndCarriesItsGrade)
{
	World world;
	world.Steps(kSkill);
	world.Row(kStep).SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	ReceivePlayers(kSkill, {1001}, 2);
	CHECK(calls == Join({{"player.add|9"}, Hit("Self", kStep, 7, 40, 41), {"Ogre.burrow"}}));
	// Grade 0 is the skill itself.
	calls.clear();
	ReceivePlayers(kSkill, {1001}, 0);
	CHECK(calls == Join({{"player.add|9"}, Hit("Self", kSkill, 7, 40, 41)}));
	calls.clear();
	world.Row(kSkill).SetUseActionGrade();
	ReceivePlayers(kSkill, {1001}, 0);
	CHECK(calls == Join({{"player.add|9"}, Hit("Self", kSkill, 7, 40, 41, kRange)}));
}

TEST(SkillTileReplies, ThePlayersBurningSoulMarksTheCellAndARottenAppleFallsThere)
{
	World world;
	ReceivePlayers(SKILL_BURNING_SOUL_CHARGING, {});
	CHECK(calls == Join({{"temp|40|41", "player.add|9"}, Hit("Self", SKILL_BURNING_SOUL_CHARGING, 7, 40, 41)}));
	calls.clear();
	ReceivePlayers(SKILL_ROTTEN_APPLE, {});
	CHECK(calls == Join({{AtCell(SKILL_CLIENT_ROTTEN_APPLE, 40, 41, 0, 0), "player.add|9"}, Hit("Self", SKILL_ROTTEN_APPLE, 7, 40, 41)}));
}

TEST(SkillTileReplies, ThePlayersSkillPastTheTableOrWithoutAZoneOnlyChangesTheStatus)
{
	World world;
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	ReceivePlayers(static_cast<int>(world.actions.GetSize()), {1001});
	CHECK(calls == std::vector<std::string>{"verified"});
	CHECK_EQ(55, World::HP());
	calls.clear();
	playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
	zoneExists = false;
	ReceivePlayers(kSkill, {1001});
	CHECK(calls.empty());
	CHECK_EQ(55, World::HP());
	// Without a player, nothing at all.
	playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
	zoneExists = true;
	hasPlayer = false;
	ReceivePlayers(kSkill, {1001});
	CHECK(calls.empty());
	CHECK_EQ(10, World::HP());
}

TEST(SkillTileReplies, ACreaturesSkillNearThePlayerIsCastTowardsItsCell)
{
	World world;
	world.Row(kSkill).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	world.Row(kSkill).SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	ReceiveNearPlayer(1002, kSkill, {1001, 4242});
	CHECK(calls == Join({CastBy("Vanda", kSkill), Hit("Vanda", kSkill, 1002, 40, 41),
		{Effect("Ogre", EFFECTSTATUS_CURSE_PARALYSIS), "Ogre.burrow"}}));
	CHECK_EQ(44, World::HP());
	// The player's pending effect status rides along, once.
	calls.clear();
	world.Row(kSkill).SetOptionRangeToDirection();
	playerStatus.SetStatus(MODIFY_EFFECT_STAT, EFFECTSTATUS_CURSE_PARALYSIS);
	ReceiveNearPlayer(1002, kSkill, {});
	CHECK(calls == Join({CastBy("Vanda", kSkill, kRange), {Effect("Self", EFFECTSTATUS_CURSE_PARALYSIS)}, Hit("Vanda", kSkill, 1002, 40, 41)}));
	CHECK_EQ(static_cast<long long>(EFFECTSTATUS_NULL), static_cast<long long>(playerStatus.GetStatus(MODIFY_EFFECT_STAT)));
}

TEST(SkillTileReplies, WithoutItsCasterNearThePlayerASkillStartsAtTheCell)
{
	World world;
	world.Row(kSkill).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	world.Row(kSkill).SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	// A weapon skill without its caster acts as itself.
	world.Row(kSkill).SetAffectCurrentWeaponAction();
	ReceiveNearPlayer(4242, kSkill, {1001});
	CHECK(calls == std::vector<std::string>({AtCell(kSkill, 40, 41, 0, OBJECTID_NULL), Effect("Ogre", EFFECTSTATUS_CURSE_PARALYSIS), "Ogre.burrow"}));
	// The range is its direction when the skill says so.
	calls.clear();
	world.Row(kSkill).SetOptionRangeToDirection();
	world.Row(kSkill).SetActionResult(ACTIONRESULTNODE_ACTIONINFO);
	ReceiveNearPlayer(4242, kSkill, {1001});
	CHECK(calls == Join({{AtCell(kSkill, 40, 41, kRange, OBJECTID_NULL), Effect("Ogre", EFFECTSTATUS_CURSE_PARALYSIS)}, Hit("Ogre", kSkill, 1001, 10, 11)}));
}

TEST(SkillTileReplies, ACreaturesActionSkillNearThePlayerActsAsItselfAndItsNodesAsTheWeapon)
{
	World world;
	world.Row(kSkill).SetAffectCurrentWeaponAction();
	world.Row(kSkill).SetActionResult(ACTIONRESULTNODE_ACTIONINFO);
	ReceiveNearPlayer(1002, kSkill, {1001});
	CHECK(calls == Join({CastBy("Vanda", kSkill), Hit("Ogre", kSkill, 1001, 10, 11)}));
	// A node of another kind comes from the caster's basic action.
	calls.clear();
	world.Row(kSkill).SetActionResult(ACTIONRESULTNODE_NULL);
	world.Row(SKILL_ATTACK_GUN_SG).SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	ReceiveNearPlayer(1002, kSkill, {1001});
	CHECK(calls == Join({CastBy("Vanda", kSkill), Hit("Vanda", kSkill, 1002, 40, 41), {"Ogre.burrow"}}));
	// Not for Piercing.
	calls.clear();
	world.Row(SKILL_PIERCING).SetAffectCurrentWeaponAction();
	world.Row(SKILL_PIERCING).SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	world.Row(SKILL_ATTACK_GUN_SG).SetActionResult(ACTIONRESULTNODE_NULL);
	ReceiveNearPlayer(1002, SKILL_PIERCING, {1001});
	CHECK(calls == Join({CastBy("Vanda", SKILL_PIERCING), Hit("Vanda", SKILL_PIERCING, 1002, 40, 41), {"Ogre.burrow"}}));
	// A caster whose basic action is a result shows nothing.
	calls.clear();
	world.vanda.basic = kMin;
	ReceiveNearPlayer(1002, kSkill, {1001});
	CHECK(calls.empty());
	CHECK_EQ(44, World::HP());
}

TEST(SkillTileReplies, ACreaturesSkillNearThePlayerActsAsItsStepAndAGradedOneCreditsThePlayer)
{
	World world;
	world.Steps(kSkill);
	world.Row(kStep).SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	ReceiveNearPlayer(1002, kSkill, {1001}, 2);
	CHECK(calls == Join({CastBy("Vanda", kStep), Hit("Vanda", kStep, 1002, 40, 41), {"Ogre.burrow"}}));
	// The graded cell node is the player's, with the range as its grade.
	calls.clear();
	world.Row(MAGIC_BLOODY_ZENITH).SetUseActionGrade();
	ReceiveNearPlayer(1002, MAGIC_BLOODY_ZENITH, {});
	CHECK(calls == Join({CastBy("Vanda", MAGIC_BLOODY_ZENITH), Hit("Vanda", MAGIC_BLOODY_ZENITH, 1002, 40, 41, kRange, "Self")}));
	calls.clear();
	world.Row(MAGIC_BLOODY_ZENITH) = MActionInfo();
	ReceiveNearPlayer(1002, MAGIC_BLOODY_ZENITH, {});
	CHECK(calls == Join({CastBy("Vanda", MAGIC_BLOODY_ZENITH), Hit("Vanda", MAGIC_BLOODY_ZENITH, 1002, 40, 41, 0, "Vanda")}));
}

TEST(SkillTileReplies, ACreaturesBurningSoulLaunchesFromItAndARottenAppleFallsAtTheCell)
{
	World world;
	ReceiveNearPlayer(1002, SKILL_BURNING_SOUL_LAUNCH, {}, 2);
	CHECK(calls == Join({{AtCell(SKILL_CLIENT_BURNING_SOL_1 + 2, 20, 21, 5, 1002)}, CastBy("Vanda", SKILL_BURNING_SOUL_LAUNCH),
		Hit("Vanda", SKILL_BURNING_SOUL_LAUNCH, 1002, 40, 41)}));
	calls.clear();
	ReceiveElsewhere(1002, SKILL_BURNING_SOUL_LAUNCH, {}, 1);
	CHECK(calls == Join({{AtCell(SKILL_CLIENT_BURNING_SOL_1 + 1, 20, 21, 5, 1002)}, CastBy("Vanda", SKILL_BURNING_SOUL_LAUNCH),
		Hit("Vanda", SKILL_BURNING_SOUL_LAUNCH, 1002, 40, 41)}));
	for (int packet = 0; packet < 2; packet++)
	{
		calls.clear();
		if (packet == 0) ReceiveNearPlayer(1002, SKILL_ROTTEN_APPLE, {});
		else ReceiveElsewhere(1002, SKILL_ROTTEN_APPLE, {});
		CHECK(calls == Join({{AtCell(SKILL_CLIENT_ROTTEN_APPLE, 40, 41, 0, 0)}, CastBy("Vanda", SKILL_ROTTEN_APPLE),
			Hit("Vanda", SKILL_ROTTEN_APPLE, 1002, 40, 41)}));
	}
}

TEST(SkillTileReplies, ACreaturesSkillNearThePlayerGuards)
{
	World world;
	// The player's own skill is not shown twice.
	ReceiveNearPlayer(7, kSkill, {1001});
	CHECK_EQ(44, World::HP());
	ReceiveNearPlayer(1002, static_cast<int>(world.actions.GetSize()), {1001});
	zoneExists = false;
	ReceiveNearPlayer(1002, kSkill, {1001});
	CHECK(calls.empty());
	playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
	zoneExists = true;
	hasPlayer = false;
	ReceiveNearPlayer(1002, kSkill, {1001});
	CHECK(calls.empty());
	CHECK_EQ(10, World::HP());
}

TEST(SkillTileReplies, ACreaturesSkillElsewhereIsCastTowardsItsCell)
{
	World world;
	world.Row(kSkill).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	world.Row(kSkill).SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	ReceiveElsewhere(1002, kSkill, {1001, 4242});
	CHECK(calls == Join({CastBy("Vanda", kSkill), Hit("Vanda", kSkill, 1002, 40, 41),
		{Effect("Ogre", EFFECTSTATUS_CURSE_PARALYSIS), "Ogre.burrow"}}));
	calls.clear();
	world.Row(kSkill).SetOptionRangeToDirection();
	ReceiveElsewhere(1002, kSkill, {});
	CHECK(calls == Join({CastBy("Vanda", kSkill, kRange), Hit("Vanda", kSkill, 1002, 40, 41)}));
}

TEST(SkillTileReplies, ACreaturesActionSkillElsewhereActsAsTheWeaponAndItsStep)
{
	World world;
	world.Row(kSkill).SetAffectCurrentWeaponAction();
	world.Row(kSkill).SetActionResult(ACTIONRESULTNODE_ACTIONINFO);
	ReceiveElsewhere(1002, kSkill, {1001});
	CHECK(calls == Join({CastBy("Vanda", kSkill), Hit("Ogre", SKILL_ATTACK_GUN_SG, 1001, 10, 11)}));
	calls.clear();
	world.Row(SKILL_PIERCING).SetAffectCurrentWeaponAction();
	world.Row(SKILL_PIERCING).SetActionResult(ACTIONRESULTNODE_ACTIONINFO);
	ReceiveElsewhere(1002, SKILL_PIERCING, {1001});
	CHECK(calls == Join({CastBy("Vanda", SKILL_PIERCING), Hit("Ogre", SKILL_PIERCING, 1001, 10, 11)}));
	// The step replaces the weapon.
	calls.clear();
	world.Steps(kSkill);
	world.Row(kStep).SetActionResult(ACTIONRESULTNODE_ACTIONINFO);
	ReceiveElsewhere(1002, kSkill, {1001}, 2);
	CHECK(calls == Join({CastBy("Vanda", kStep), Hit("Ogre", kStep, 1001, 10, 11)}));
	calls.clear();
	world.vanda.basic = kMin;
	ReceiveElsewhere(1002, kSkill, {1001});
	CHECK(calls.empty());
}

TEST(SkillTileReplies, AGradedSkillElsewhereCreditsThePlayer)
{
	World world;
	world.Row(MAGIC_BLOODY_ZENITH).SetUseActionGrade();
	ReceiveElsewhere(1002, MAGIC_BLOODY_ZENITH, {});
	CHECK(calls == Join({CastBy("Vanda", MAGIC_BLOODY_ZENITH), Hit("Vanda", MAGIC_BLOODY_ZENITH, 1002, 40, 41, kRange, "Self")}));
	calls.clear();
	world.Row(MAGIC_BLOODY_ZENITH) = MActionInfo();
	ReceiveElsewhere(1002, MAGIC_BLOODY_ZENITH, {});
	CHECK(calls == Join({CastBy("Vanda", MAGIC_BLOODY_ZENITH), Hit("Vanda", MAGIC_BLOODY_ZENITH, 1002, 40, 41, 0, "Vanda")}));
}

TEST(SkillTileReplies, ACreaturesSkillElsewhereGuards)
{
	World world;
	ReceiveElsewhere(4242, kSkill, {1001});
	ReceiveElsewhere(1002, static_cast<int>(world.actions.GetSize()), {1001});
	zoneExists = false;
	ReceiveElsewhere(1002, kSkill, {1001});
	CHECK(calls.empty());
}

TEST(SkillTileReplies, AnUnseenCastersSkillStartsWhereTheServerSaysItCameFrom)
{
	World world;
	world.Row(kSkill).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	world.Row(kSkill).SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	playerStatus.SetStatus(MODIFY_EFFECT_STAT, EFFECTSTATUS_CURSE_PARALYSIS);
	ReceiveUnseen(kSkill, {1001, 4242});
	CHECK(calls == std::vector<std::string>({AtCell(kSkill + kMin, 50, 51, kRange, OBJECTID_NULL),
		Effect("Self", EFFECTSTATUS_CURSE_PARALYSIS), Effect("Ogre", EFFECTSTATUS_CURSE_PARALYSIS), "Ogre.burrow"}));
	CHECK_EQ(33, World::HP());
	CHECK_EQ(static_cast<long long>(EFFECTSTATUS_NULL), static_cast<long long>(playerStatus.GetStatus(MODIFY_EFFECT_STAT)));
	// A range past the eight directions faces the first.
	for (int range : {7, 8, 9})
	{
		calls.clear();
		ReceiveUnseen(kSkill, {}, 0, range);
		CHECK(calls == std::vector<std::string>{AtCell(kSkill + kMin, 50, 51, range < 8 ? range : 0, OBJECTID_NULL)});
	}
}

TEST(SkillTileReplies, AnUnseenCastersActionSkillActsAsItsResultValueOnEachCreature)
{
	World world;
	world.Row(kSkill).SetActionResult(ACTIONRESULTNODE_ACTIONINFO, SKILL_SINGLE_BLOW);
	ReceiveUnseen(kSkill, {1001});
	CHECK(calls == Join({{AtCell(kSkill + kMin, 50, 51, kRange, OBJECTID_NULL)}, Hit("Ogre", SKILL_SINGLE_BLOW, 1001, 10, 11)}));
	calls.clear();
	world.Row(kSkill).SetUseActionGrade();
	world.Row(SKILL_SINGLE_BLOW).SetUseActionGrade();
	ReceiveUnseen(kSkill, {1001});
	CHECK(calls == Join({{AtCell(kSkill + kMin, 50, 51, kRange, OBJECTID_NULL)}, Hit("Ogre", SKILL_SINGLE_BLOW, 1001, 10, 11, kRange)}));
	// By its step, and a node of another kind from the step.
	calls.clear();
	world.Steps(kSkill);
	world.Row(kStep).SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	ReceiveUnseen(kSkill, {1001}, 2);
	CHECK(calls == std::vector<std::string>({AtCell(kStep + kMin, 50, 51, kRange, OBJECTID_NULL), "Ogre.burrow"}));
}

TEST(SkillTileReplies, AnUnseenCastersBurningSoulLandsAtTheCellAndTheGuards)
{
	World world;
	ReceiveUnseen(SKILL_BURNING_SOUL_LAUNCH, {}, 2);
	CHECK(calls == std::vector<std::string>({AtCell(RESULT_CLIENT_BURNING_SOL_1 + 2, 40, 41, 0, 0),
		AtCell(SKILL_BURNING_SOUL_LAUNCH + kMin, 50, 51, kRange, OBJECTID_NULL)}));
	calls.clear();
	ReceiveUnseen(static_cast<int>(world.actions.GetSize()), {1001});
	zoneExists = false;
	ReceiveUnseen(kSkill, {1001});
	CHECK(calls.empty());
	CHECK_EQ(33, World::HP());
	playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
	zoneExists = true;
	hasPlayer = false;
	ReceiveUnseen(kSkill, {1001});
	CHECK(calls.empty());
	CHECK_EQ(10, World::HP());
}
