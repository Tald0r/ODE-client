#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "ConvertDuration.h"
#include "ExplosiveActions.h"
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
#include "Gpackets/GCMineExplosionOK1.h"
#include "Gpackets/GCMineExplosionOK2.h"
#include "Gpackets/GCSkillToTileOK4.h"
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

// A skill's effect starting at a cell with nobody casting it, carrying a
// queue that adds, for each creature it lands on, the skill's effect
// status and an action node. A mine going off near the player (OK1, which
// also changes the player's status) or elsewhere (OK2) acts as itself on
// each creature, by that creature; an unseen caster's skill (OK4) acts as
// the node its result kind names, as its step, and starts as its result,
// facing down unless its range is its direction. The test hosts run every
// handed-over queue at once.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool hasPlayer = true;
MStatus playerStatus;

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	std::string name;
	TYPE_SECTORPOSITION x = 0;
	TYPE_SECTORPOSITION y = 0;
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
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.SetEffectDelayFrame = [](MCreature* p, TYPE_ACTIONINFO ai, std::uint32_t frames) { Record(p, "delay|" + N(ai) + "|" + N(frames)); };
	host.PacketSpecialActionResult = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t grade) {
		Record(p, "result|" + N(ai) + "|" + N(id) + "|" + N(sX) + "|" + N(sY) + "|" + N(grade));
	};
	host.AddEffectStatus = [](MCreature* p, EFFECTSTATUS status, std::uint32_t delayFrame) {
		Record(p, "effect|" + N(status) + "|" + N(delayFrame));
		return true;
	};
	host.SetUndergroundCreature = [](MCreature* p) { Record(p, "burrow"); };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.SkillToSector = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, int direction, std::uint32_t delayFrame, MActionResult* pResult) {
		calls.push_back("sector|" + N(actionInfo) + "|" + N(sX) + "," + N(sY) + "|dir " + N(direction) + "|" + N(delayFrame));
		CHECK(pResult != nullptr);
		if (pResult == nullptr) return;
		pResult->Execute();
		delete pResult;
	};
	return host;
}();

constexpr TYPE_ACTIONINFO kMin = MIN_RESULT_ACTIONINFO;
constexpr TYPE_ACTIONINFO kSkill = SKILL_DOUBLE_IMPACT;
constexpr TYPE_ACTIONINFO kStep = SKILL_TRIPLE_SLASHER;
constexpr int kPomz = 1;	// the second mine item type
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
		me = {7, "Self", 30, 31};
		ogre = {1001, "Ogre", 10, 11};
		vanda = {1002, "Vanda", 20, 21};
		creatures = {{ogre.id, &ogre}, {vanda.id, &vanda}, {me.id, &me}};
		zoneExists = true;
		hasPlayer = true;
		playerStatus = MStatus();
		playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
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

void ReceiveMineNearPlayer(int itemType, std::initializer_list<ObjectID_t> targets)
{
	GCMineExplosionOK1 original;
	original.setXYDir(40, 41, 2);
	original.setItemType(static_cast<ItemType_t>(itemType));
	for (ObjectID_t id : targets) original.addCListElement(id);
	original.addShortData(MODIFY_CURRENT_HP, 55);
	GCMineExplosionOK1 packet;
	Decode(packet, original);
	GCMineExplosionOK1Handler::execute(&packet, nullptr);
}

void ReceiveMine(int itemType, std::initializer_list<ObjectID_t> targets)
{
	GCMineExplosionOK2 original;
	original.setXYDir(40, 41, 2);
	original.setItemType(static_cast<ItemType_t>(itemType));
	for (ObjectID_t id : targets) original.addCListElement(id);
	GCMineExplosionOK2 packet;
	Decode(packet, original);
	GCMineExplosionOK2Handler::execute(&packet, nullptr);
}

void ReceiveUnseen(int skill, std::initializer_list<ObjectID_t> targets, int grade = 0)
{
	GCSkillToTileOK4 original;
	original.setSkillType(static_cast<SkillType_t>(skill));
	original.setX(40);
	original.setY(41);
	original.setRange(kRange);
	original.setDuration(kDuration);
	original.setGrade(static_cast<BYTE>(grade));
	for (ObjectID_t id : targets) original.addCListElement(id);
	GCSkillToTileOK4 packet;
	Decode(packet, original);
	GCSkillToTileOK4Handler::execute(&packet, nullptr);
}

// An action node's two records on its target, with `frames` of delay.
std::vector<std::string> Hit(const char* who, TYPE_ACTIONINFO action, TYPE_OBJECTID id, int x, int y, const std::string& frames, int grade = 0)
{
	std::vector<std::string> out = {std::string(who) + ".delay|" + N(action + kMin) + "|" + frames};
	out.push_back(std::string(who) + ".result|" + N(action + kMin) + "|" + N(id) + "|" + N(x) + "|" + N(y) + "|" + N(grade));
	return out;
}

std::vector<std::string> Join(std::initializer_list<std::vector<std::string>> parts)
{
	std::vector<std::string> out;
	for (const std::vector<std::string>& part : parts) out.insert(out.end(), part.begin(), part.end());
	return out;
}

std::string F() { return N(ConvertDurationToFrame(kDuration)); }

// A node that counts its own end, to see who frees a queue.
int freed = 0;
struct Counted : MActionResultNode
{
	~Counted() override { ++freed; }
	void Execute() override { calls.push_back("counted"); }
};

} // namespace

TEST(SectorSkillReplies, AMineNearThePlayerGoesOffAtItsCellAndActsOnEachCreatureByItself)
{
	World world;
	const TYPE_ACTIONINFO pomz = static_cast<TYPE_ACTIONINFO>(GetMineActionInfo(kPomz));
	world.Row(pomz).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	ReceiveMineNearPlayer(kPomz, {1001, 4242, 7});
	CHECK(calls == Join({{"sector|" + N(pomz) + "|40,41|dir 2|16", "Ogre.effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|16"},
		Hit("Ogre", pomz, 1001, 10, 11, "16"),
		{"Self.effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|16"}, Hit("Self", pomz, 7, 30, 31, "16")}));
	CHECK_EQ(55, World::HP());
}

TEST(SectorSkillReplies, AMineNearThePlayerGuards)
{
	World world;
	// A type that is no mine shows nothing; the status still changes.
	ReceiveMineNearPlayer(9, {1001});
	CHECK(calls.empty());
	CHECK_EQ(55, World::HP());
	// Without a zone, nothing at all.
	playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
	zoneExists = false;
	ReceiveMineNearPlayer(kPomz, {1001});
	CHECK(calls.empty());
	CHECK_EQ(10, World::HP());
	// Without a player the mine still goes off.
	zoneExists = true;
	hasPlayer = false;
	ReceiveMineNearPlayer(kPomz, {1001});
	const TYPE_ACTIONINFO pomz = static_cast<TYPE_ACTIONINFO>(GetMineActionInfo(kPomz));
	CHECK(calls == Join({{"sector|" + N(pomz) + "|40,41|dir 2|16"}, Hit("Ogre", pomz, 1001, 10, 11, "16")}));
	CHECK_EQ(10, World::HP());
}

TEST(SectorSkillReplies, AMineElsewhereGoesOffTheSameWay)
{
	World world;
	const TYPE_ACTIONINFO pomz = static_cast<TYPE_ACTIONINFO>(GetMineActionInfo(kPomz));
	world.Row(pomz).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	ReceiveMine(kPomz, {1001, 4242});
	CHECK(calls == Join({{"sector|" + N(pomz) + "|40,41|dir 2|16", "Ogre.effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|16"},
		Hit("Ogre", pomz, 1001, 10, 11, "16")}));
	CHECK_EQ(10, World::HP());
	calls.clear();
	ReceiveMine(9, {1001});
	zoneExists = false;
	ReceiveMine(kPomz, {1001});
	CHECK(calls.empty());
}

TEST(SectorSkillReplies, AnUnseenCastersSkillStartsAtTheCellAsItsResultFacingDown)
{
	World world;
	world.Row(kSkill).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	world.Row(kSkill).SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	ReceiveUnseen(kSkill, {1001, 4242});
	CHECK(calls == std::vector<std::string>({"sector|" + N(kSkill + kMin) + "|40,41|dir " + N(DIRECTION_DOWN) + "|" + F(),
		"Ogre.effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|" + F(), "Ogre.burrow"}));
	// A skill whose range is its direction faces that way.
	calls.clear();
	world.Row(kSkill).SetOptionRangeToDirection();
	ReceiveUnseen(kSkill, {});
	CHECK(calls == std::vector<std::string>{"sector|" + N(kSkill + kMin) + "|40,41|dir " + N(kRange) + "|" + F()});
}

TEST(SectorSkillReplies, AnUnseenCastersActionSkillActsAsItsResultValueAndItsStep)
{
	World world;
	world.Row(kSkill).SetActionResult(ACTIONRESULTNODE_ACTIONINFO, SKILL_SINGLE_BLOW);
	ReceiveUnseen(kSkill, {1001});
	CHECK(calls == Join({{"sector|" + N(kSkill + kMin) + "|40,41|dir " + N(DIRECTION_DOWN) + "|" + F()},
		Hit("Ogre", SKILL_SINGLE_BLOW, 1001, 10, 11, F())}));
	calls.clear();
	world.Row(kSkill).SetUseActionGrade();
	world.Row(SKILL_SINGLE_BLOW).SetUseActionGrade();
	ReceiveUnseen(kSkill, {1001});
	CHECK(calls == Join({{"sector|" + N(kSkill + kMin) + "|40,41|dir " + N(DIRECTION_DOWN) + "|" + F()},
		Hit("Ogre", SKILL_SINGLE_BLOW, 1001, 10, 11, F(), kRange)}));
	// By its step, and a node of another kind from the step.
	calls.clear();
	world.Row(kSkill).SetUseActionStep();
	world.Row(kSkill).SetActionStep(1, kStep);
	world.Row(kStep).SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	ReceiveUnseen(kSkill, {1001}, 2);
	CHECK(calls == std::vector<std::string>({"sector|" + N(kStep + kMin) + "|40,41|dir " + N(DIRECTION_DOWN) + "|" + F(), "Ogre.burrow"}));
}

TEST(SectorSkillReplies, AnUnseenCastersSkillGuards)
{
	World world;
	ReceiveUnseen(static_cast<int>(world.actions.GetSize()), {1001});
	zoneExists = false;
	ReceiveUnseen(kSkill, {1001});
	CHECK(calls.empty());
}

TEST(SectorSkillReplies, WithoutAHostTheSectorQueueIsFreedUnrun)
{
	World world;
	freed = 0;
	MActionResult* pResult = new MActionResult;
	pResult->Add(new Counted);
	WorldEvents::SkillToSector(kSkill, 40, 41, 2, 16, pResult);
	CHECK(calls == Join({{"sector|" + N(kSkill) + "|40,41|dir 2|16", "counted"}}));
	CHECK_EQ(1, freed);
	calls.clear();
	WorldEvents::SetHost(nullptr);
	pResult = new MActionResult;
	pResult->Add(new Counted);
	WorldEvents::SkillToSector(kSkill, 40, 41, 2, 16, pResult);
	CHECK(calls.empty());
	CHECK_EQ(2, freed);
	ReceiveMine(kPomz, {1001});
	CHECK(calls.empty());
}
