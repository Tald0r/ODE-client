#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCBloodDrainOK3.h"
#include "Gpackets/GCKnocksTargetBackOK5.h"
#include "Gpackets/GCSkillToTileOK3.h"
#include "Gpackets/GCThrowItemOK3.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Skill replies about two creatures in the zone, neither of them this
// player: the user faces its target or cell and acts, handing the action
// its result queue, which the creature runs when the action lands. The
// test host runs the queue at once, so its nodes show up in the record:
// a knock-back's hit and move, holy water's and a blood drain's hit. A
// blood drain also tells the target who drains it and marks it drained;
// a knock-back sets the target's server cell at once.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	std::string name;
	TYPE_SECTORPOSITION x = 0;
	TYPE_SECTORPOSITION y = 0;
};
std::map<TYPE_OBJECTID, FakeCreature*> creatures;

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* pCreature)
{
	static FakeCreature none;
	CHECK(pCreature != nullptr);
	return pCreature == nullptr ? none : *reinterpret_cast<FakeCreature*>(pCreature);
}
void Record(MCreature* p, const std::string& what) { calls.push_back(Fake(p).name + "." + what); }

// Runs a handed-over result queue, as the creature does when its action
// lands, and frees it.
void Land(MActionResult* pResult)
{
	if (pResult == nullptr)
	{
		calls.push_back("(no result)");
		return;
	}
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

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.SetDirectionToPosition = [](MCreature* p, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) { Record(p, "face|" + N(sX) + "|" + N(sY)); };
	host.SetServerPosition = [](MCreature* p, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) { Record(p, "server|" + N(sX) + "|" + N(sY)); };
	host.MovePosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		Record(p, "move|" + N(x) + "|" + N(y));
		return true;
	};
	host.ReleaseMoveBuffer = [](MCreature* p) { Record(p, "release"); };
	host.SetEffectDelayFrame = [](MCreature* p, TYPE_ACTIONINFO ai, std::uint32_t frames) { Record(p, "delay|" + N(ai) + "|" + N(frames)); };
	host.PacketSpecialActionResult = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t) {
		Record(p, "result|" + N(ai) + "|" + N(id) + "|" + N(sX) + "|" + N(sY));
	};
	host.AddEffectStatus = [](MCreature* p, EFFECTSTATUS status, std::uint32_t delayFrame) {
		Record(p, "effect|" + N(status) + "|" + N(delayFrame));
		return true;
	};
	host.PacketSpecialActionToOther = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_OBJECTID id, MActionResult* pResult) {
		Record(p, "other|" + N(ai) + "|" + N(id));
		Land(pResult);
	};
	host.PacketSpecialActionToSector = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, MActionResult* pResult) {
		Record(p, "sector|" + N(ai) + "|" + N(sX) + "|" + N(sY));
		Land(pResult);
	};
	host.SetDrainCreatureID = [](MCreature* p, TYPE_OBJECTID id) { Record(p, "drainer|" + N(id)); };
	host.ClearStopBloodDrain = [](MCreature* p) { Record(p, "drainon"); };
	return host;
}();

// The player stands apart; the results ask for it before moving anyone.
FakeCreature self{7, "Self", 0, 0};

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return Handle(self); };
	host.GetID = []() { return self.id; };
	return host;
}();

constexpr TYPE_ACTIONINFO kMin = MIN_RESULT_ACTIONINFO;

struct World : GameModelWorld
{
	FakeCreature ogre, vanda;
	MActionInfoTable actions;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	World()
	{
		ogre = {1001, "Ogre", 10, 11};
		vanda = {1002, "Vanda", 20, 21};
		creatures = {{ogre.id, &ogre}, {vanda.id, &vanda}};
		zoneExists = true;
		calls.clear();
		actions.Init(MAX_ACTIONINFO);
		actions.SetMinResultActionInfo(kMin);
		g_pActionInfoTable = &actions;
	}
	~World()
	{
		PlayerState::SetHost(oldPlayer);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		g_pActionInfoTable = oldActions;
		creatures.clear();
	}
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

void ReceiveKnockBack(ObjectID_t user, ObjectID_t target, SkillType_t skill, Coord_t x, Coord_t y)
{
	GCKnocksTargetBackOK5 original;
	original.setObjectID(user);
	original.setTargetObjectID(target);
	original.setSkillType(skill);
	original.setXYDir(x, y, 2);
	original.setSkillSuccess(true);
	GCKnocksTargetBackOK5 packet;
	Decode(packet, original);
	GCKnocksTargetBackOK5Handler::execute(&packet, nullptr);
}

void ReceiveTileSkill(ObjectID_t user, SkillType_t skill, Coord_t x, Coord_t y, BYTE grade = 0)
{
	GCSkillToTileOK3 original;
	original.setObjectID(user);
	original.setSkillType(skill);
	original.setX(x);
	original.setY(y);
	original.setGrade(grade);
	GCSkillToTileOK3 packet;
	Decode(packet, original);
	GCSkillToTileOK3Handler::execute(&packet, nullptr);
}

void ReceiveThrow(ObjectID_t user, ObjectID_t target)
{
	GCThrowItemOK3 original;
	original.setObjectID(user);
	original.setTargetObjectID(target);
	GCThrowItemOK3 packet;
	Decode(packet, original);
	GCThrowItemOK3Handler::execute(&packet, nullptr);
}

void ReceiveDrain(ObjectID_t user, ObjectID_t target)
{
	GCBloodDrainOK3 original;
	original.setObjectID(user);
	original.setTargetObjectID(target);
	GCBloodDrainOK3 packet;
	Decode(packet, original);
	GCBloodDrainOK3Handler::execute(&packet, nullptr);
}

// The hit an action-info node lands on the ogre, at its cell.
std::vector<std::string> Hit(TYPE_ACTIONINFO skill)
{
	return {"Ogre.delay|" + N(skill + kMin) + "|0", "Ogre.result|" + N(skill + kMin) + "|1001|10|11"};
}

std::vector<std::string> Then(std::vector<std::string> first, const std::vector<std::string>& rest)
{
	first.insert(first.end(), rest.begin(), rest.end());
	return first;
}

} // namespace

TEST(SpecialActionReplies, AKnockBackHitsMovesAndSetsTheTargetsServerCell)
{
	World world;
	ReceiveKnockBack(1002, 1001, SKILL_DOUBLE_IMPACT, 14, 15);
	CHECK(calls == Then(Then({"Vanda.face|10|11", "Vanda.other|" + N(SKILL_DOUBLE_IMPACT) + "|1001"}, Hit(SKILL_DOUBLE_IMPACT)),
		{"Ogre.release", "Ogre.move|14|15", "Ogre.server|14|15", "Ogre.server|14|15"}));
}

TEST(SpecialActionReplies, AKnockBackNeedsAZoneBothCreaturesAndASkillInTheTable)
{
	World world;
	ReceiveKnockBack(4242, 1001, SKILL_DOUBLE_IMPACT, 1, 1);
	ReceiveKnockBack(1002, 4242, SKILL_DOUBLE_IMPACT, 1, 1);
	ReceiveKnockBack(1002, 1001, MAX_ACTIONINFO, 1, 1);
	zoneExists = false;
	ReceiveKnockBack(1002, 1001, SKILL_DOUBLE_IMPACT, 1, 1);
	CHECK(calls.empty());
}

TEST(SpecialActionReplies, ATileSkillActsOnItsCellAndFacesIt)
{
	World world;
	ReceiveTileSkill(1002, SKILL_DOUBLE_IMPACT, 30, 31);
	CHECK(calls == std::vector<std::string>({"Vanda.sector|" + N(SKILL_DOUBLE_IMPACT) + "|30|31", "(no result)", "Vanda.face|30|31"}));
}

TEST(SpecialActionReplies, AGradedTileSkillActsAsItsStep)
{
	World world;
	auto& row = testfw::MutableRow(world.actions, SKILL_DOUBLE_IMPACT);
	row.SetUseActionStep();
	row.SetActionStep(0, SKILL_SINGLE_BLOW);
	row.SetActionStep(1, ACTIONINFO_NULL);
	// Grade 1 is step 0; a step with no action keeps the skill; grade 0
	// takes no step.
	ReceiveTileSkill(1002, SKILL_DOUBLE_IMPACT, 1, 2, 1);
	ReceiveTileSkill(1002, SKILL_DOUBLE_IMPACT, 1, 2, 2);
	ReceiveTileSkill(1002, SKILL_DOUBLE_IMPACT, 1, 2, 0);
	const std::string impact = "Vanda.sector|" + N(SKILL_DOUBLE_IMPACT) + "|1|2";
	CHECK(calls == std::vector<std::string>({"Vanda.sector|" + N(SKILL_SINGLE_BLOW) + "|1|2", "(no result)", "Vanda.face|1|2",
		impact, "(no result)", "Vanda.face|1|2", impact, "(no result)", "Vanda.face|1|2"}));
}

TEST(SpecialActionReplies, ATileSkillNeedsAZoneItsCreatureAndASkillInTheTable)
{
	World world;
	ReceiveTileSkill(4242, SKILL_DOUBLE_IMPACT, 1, 1);
	ReceiveTileSkill(1002, MAX_ACTIONINFO, 1, 1);
	zoneExists = false;
	ReceiveTileSkill(1002, SKILL_DOUBLE_IMPACT, 1, 1);
	CHECK(calls.empty());
}

TEST(SpecialActionReplies, HolyWaterIsThrownAtTheTarget)
{
	World world;
	ReceiveThrow(1002, 1001);
	CHECK(calls == Then({"Vanda.face|10|11", "Vanda.other|" + N(MAGIC_THROW_HOLY_WATER) + "|1001"}, Hit(MAGIC_THROW_HOLY_WATER)));
}

TEST(SpecialActionReplies, ABloodDrainTellsTheTargetHitsItAndMarksItDrained)
{
	World world;
	ReceiveDrain(1002, 1001);
	CHECK(calls == Then(Then({"Ogre.drainer|1002", "Vanda.drainon", "Vanda.face|10|11", "Vanda.other|" + N(SKILL_BLOOD_DRAIN) + "|1001"},
		Hit(SKILL_BLOOD_DRAIN)), {"Ogre.effect|" + N(EFFECTSTATUS_BLOOD_DRAIN) + "|691200"}));
}

TEST(SpecialActionReplies, AThrowOrDrainNeedsAZoneAndBothCreatures)
{
	World world;
	ReceiveThrow(4242, 1001);
	ReceiveThrow(1002, 4242);
	ReceiveDrain(4242, 1001);
	ReceiveDrain(1002, 4242);
	zoneExists = false;
	ReceiveThrow(1002, 1001);
	ReceiveDrain(1002, 1001);
	CHECK(calls.empty());
}

namespace {

// A node that counts its own destruction, to see a queue freed.
int freed = 0;
struct CountedNode : MActionResultNode
{
	~CountedNode() override { freed++; }
	void Execute() override {}
};

MActionResult* CountedQueue()
{
	MActionResult* pResult = new MActionResult;
	pResult->Add(new CountedNode);
	return pResult;
}

} // namespace

TEST(SpecialActionReplies, AQueueNoEntryTakesIsFreed)
{
	World world;
	FakeCreature& ogre = world.ogre;
	freed = 0;
	// Without a host, and with a host that lacks the entries.
	const CreatureState::Host empty{};
	for (const CreatureState::Host* host : {static_cast<const CreatureState::Host*>(nullptr), &empty})
	{
		CreatureState::SetHost(host);
		CreatureState::PacketSpecialActionToSector(Handle(ogre), SKILL_DOUBLE_IMPACT, 1, 2, CountedQueue());
		CreatureState::PacketSpecialActionToOther(Handle(ogre), SKILL_DOUBLE_IMPACT, 1002, CountedQueue());
		CreatureState::PacketSpecialActionToSelf(Handle(ogre), SKILL_DOUBLE_IMPACT, CountedQueue());
		CreatureState::PacketSpecialActionToNobody(Handle(ogre), SKILL_DOUBLE_IMPACT, 1, 2);
		// No queue at all is fine too.
		CreatureState::PacketSpecialActionToOther(Handle(ogre), SKILL_DOUBLE_IMPACT, 1002, nullptr);
	}
	CHECK_EQ(6, freed);
	CHECK(calls.empty());
}

TEST(SpecialActionReplies, WithoutTheCreatureServiceNothingIsDone)
{
	World world;
	CreatureState::SetHost(nullptr);
	ReceiveKnockBack(1002, 1001, SKILL_DOUBLE_IMPACT, 1, 1);
	ReceiveThrow(1002, 1001);
	ReceiveDrain(1002, 1001);
	ReceiveTileSkill(1002, SKILL_DOUBLE_IMPACT, 1, 1);
	CHECK(calls.empty());
}
