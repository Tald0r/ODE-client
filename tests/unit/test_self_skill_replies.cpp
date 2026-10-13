#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "ConvertDuration.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCSkillToSelfOK2.h"
#include "Gpackets/GCSkillToSelfOK3.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// A creature in the zone uses a skill on itself: it acts with a queue that
// hits it with the skill (its step when graded) for the skill's converted
// duration, adds the skill's effect status, and adds the node the skill's
// result kind names. A skill used at a cell starts the skill's result
// effect there from the effect generator's main node, for its duration.
// The test hosts run every handed-over queue at once.
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
FakeCreature me{7, "Self", 30, 31};

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

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return Handle(me); };
	host.GetID = []() { return me.id; };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.SetEffectDelayFrame = [](MCreature* p, TYPE_ACTIONINFO ai, std::uint32_t frames) { Record(p, "delay|" + N(ai) + "|" + N(frames)); };
	host.PacketSpecialActionResult = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t) {
		Record(p, "result|" + N(ai) + "|" + N(id) + "|" + N(sX) + "|" + N(sY));
	};
	host.AddEffectStatus = [](MCreature* p, EFFECTSTATUS status, std::uint32_t delayFrame) {
		Record(p, "effect|" + N(status) + "|" + N(delayFrame));
		return true;
	};
	host.SetUndergroundCreature = [](MCreature* p) { Record(p, "burrow"); };
	host.PacketSpecialActionToSelf = [](MCreature* p, TYPE_ACTIONINFO ai, MActionResult* pResult) {
		Record(p, "self|" + N(ai));
		Land(pResult);
	};
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int startZ, int direction,
		TYPE_OBJECTID targetID, TYPE_SECTORPOSITION targetX, TYPE_SECTORPOSITION targetY, int targetZ, std::uint32_t delayFrame,
		MActionResult* pResult, bool bStartFromMainNode, int sX, int sY) {
		calls.push_back("effect|" + N(actionInfo) + "|" + N(startX) + "," + N(startY) + "," + N(startZ) + "|dir " + N(direction)
			+ "|" + N(targetID) + "@" + N(targetX) + "," + N(targetY) + "," + N(targetZ) + "|" + N(delayFrame)
			+ (bStartFromMainNode ? "|main" : "|resume") + "|" + N(sX) + "," + N(sY));
		Land(pResult);
	};
	return host;
}();

constexpr TYPE_ACTIONINFO kMin = MIN_RESULT_ACTIONINFO;

struct World : GameModelWorld
{
	FakeCreature ogre;
	MActionInfoTable actions;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		ogre = {1001, "Ogre", 10, 11};
		creatures = {{ogre.id, &ogre}, {me.id, &me}};
		zoneExists = true;
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

void ReceiveOnSelf(ObjectID_t user, SkillType_t skill, Duration_t duration, BYTE grade = 0)
{
	GCSkillToSelfOK2 original;
	original.setObjectID(user);
	original.setSkillType(skill);
	original.setDuration(duration);
	original.setGrade(grade);
	GCSkillToSelfOK2 packet;
	Decode(packet, original);
	GCSkillToSelfOK2Handler::execute(&packet, nullptr);
}

void ReceiveAtCell(SkillType_t skill, Coord_t x, Coord_t y, Duration_t duration, BYTE grade = 0)
{
	GCSkillToSelfOK3 original;
	original.setXY(x, y);
	original.setSkillType(skill);
	original.setDuration(duration);
	original.setGrade(grade);
	GCSkillToSelfOK3 packet;
	Decode(packet, original);
	GCSkillToSelfOK3Handler::execute(&packet, nullptr);
}

} // namespace

TEST(SelfSkillReplies, ASkillOnItselfHitsTheCreatureForTheSkillsDuration)
{
	World world;
	const std::string frames = N(ConvertDurationToFrame(50));
	ReceiveOnSelf(1001, SKILL_DOUBLE_IMPACT, 50);
	CHECK(calls == std::vector<std::string>({"Ogre.self|" + N(SKILL_DOUBLE_IMPACT),
		"Ogre.delay|" + N(SKILL_DOUBLE_IMPACT + kMin) + "|" + frames, "Ogre.result|" + N(SKILL_DOUBLE_IMPACT + kMin) + "|1001|10|11"}));
}

TEST(SelfSkillReplies, TheSkillsEffectStatusAndResultKindAreAddedToTheQueue)
{
	World world;
	auto& row = testfw::MutableRow(world.actions, SKILL_DOUBLE_IMPACT);
	row.SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	row.SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	const std::string frames = N(ConvertDurationToFrame(50));
	ReceiveOnSelf(1001, SKILL_DOUBLE_IMPACT, 50);
	CHECK(calls == std::vector<std::string>({"Ogre.self|" + N(SKILL_DOUBLE_IMPACT),
		"Ogre.delay|" + N(SKILL_DOUBLE_IMPACT + kMin) + "|" + frames, "Ogre.result|" + N(SKILL_DOUBLE_IMPACT + kMin) + "|1001|10|11",
		"Ogre.effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|" + frames, "Ogre.burrow"}));
}

TEST(SelfSkillReplies, AGradedSkillOnItselfHitsAsItsStepButActsAsTheSkill)
{
	World world;
	auto& row = testfw::MutableRow(world.actions, SKILL_DOUBLE_IMPACT);
	row.SetUseActionStep();
	row.SetActionStep(1, SKILL_SINGLE_BLOW);
	ReceiveOnSelf(1001, SKILL_DOUBLE_IMPACT, 0, 2);
	CHECK(calls == std::vector<std::string>({"Ogre.self|" + N(SKILL_DOUBLE_IMPACT),
		"Ogre.delay|" + N(SKILL_SINGLE_BLOW + kMin) + "|0", "Ogre.result|" + N(SKILL_SINGLE_BLOW + kMin) + "|1001|10|11"}));
}

TEST(SelfSkillReplies, ASkillOnItselfNeedsAZoneTheCreatureAndASkillInTheTable)
{
	World world;
	ReceiveOnSelf(4242, SKILL_DOUBLE_IMPACT, 0);
	ReceiveOnSelf(1001, MAX_ACTIONINFO, 0);
	zoneExists = false;
	ReceiveOnSelf(1001, SKILL_DOUBLE_IMPACT, 0);
	CHECK(calls.empty());
}

TEST(SelfSkillReplies, ASkillAtACellStartsItsResultEffectThere)
{
	World world;
	ReceiveAtCell(SKILL_DOUBLE_IMPACT, 40, 41, 50);
	CHECK(calls == std::vector<std::string>({"effect|" + N(SKILL_DOUBLE_IMPACT + kMin) + "|40,41,0|dir 0|" + N(OBJECTID_NULL)
		+ "@40,41,0|" + N(ConvertDurationToFrame(50)) + "|resume|0,0", "(no result)"}));
	// A graded skill starts its step's.
	calls.clear();
	auto& row = testfw::MutableRow(world.actions, SKILL_DOUBLE_IMPACT);
	row.SetUseActionStep();
	row.SetActionStep(0, SKILL_SINGLE_BLOW);
	ReceiveAtCell(SKILL_DOUBLE_IMPACT, 40, 41, 0, 1);
	CHECK(calls.size() == 2 && calls[0].rfind("effect|" + N(SKILL_SINGLE_BLOW + kMin) + "|", 0) == 0);
}

TEST(SelfSkillReplies, ASkillAtACellNeedsAZoneAndASkillInTheTable)
{
	World world;
	ReceiveAtCell(MAX_ACTIONINFO, 40, 41, 0);
	zoneExists = false;
	ReceiveAtCell(SKILL_DOUBLE_IMPACT, 40, 41, 0);
	CHECK(calls.empty());
}

namespace {

int freed = 0;
struct CountedNode : MActionResultNode
{
	~CountedNode() override { freed++; }
	void Execute() override { calls.push_back("ran"); }
};

} // namespace

TEST(SelfSkillReplies, WithoutTheEffectEntryTheQueueIsFreedUnrun)
{
	World world;
	freed = 0;
	const WorldEvents::Host empty{};
	for (const WorldEvents::Host* host : {static_cast<const WorldEvents::Host*>(nullptr), &empty})
	{
		WorldEvents::SetHost(host);
		MActionResult* pResult = new MActionResult;
		pResult->Add(new CountedNode);
		WorldEvents::ExecuteActionInfoFromMainNode(SKILL_DOUBLE_IMPACT, 1, 2, 0, 0, OBJECTID_NULL, 1, 2, 0, 0, pResult);
	}
	CHECK_EQ(2, freed);
	CHECK(calls.empty());
}
