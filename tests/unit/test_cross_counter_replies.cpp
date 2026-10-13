#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCCrossCounterOK1.h"
#include "Gpackets/GCCrossCounterOK2.h"
#include "Gpackets/GCCrossCounterOK3.h"
#include <map>
#include <string>
#include <vector>

// A cross counter is shown as a fading copy of the user striking back at the
// target. The player's own counters it at a creature (answering the wait
// for the skill); a creature's counters the player; or one creature's
// counters another. The player's two also carry its status. A missing
// creature is passed on as none, and the world shows nothing for it.
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
};
std::map<TYPE_OBJECTID, FakeCreature*> creatures;
FakeCreature me{7, "Self"};

MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
std::string Name(MCreature* p) { return p == nullptr ? "none" : reinterpret_cast<FakeCreature*>(p)->name; }

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
	host.Status = []() { return &playerStatus; };
	host.GetWaitVerify = []() { return waitVerify; };
	host.SetWaitVerifyNULL = []() {
		calls.push_back("verified");
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
	};
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.SkillCrossCounter = [](MCreature* pUser, MCreature* pTarget, int skillID) {
		calls.push_back("counter|" + Name(pUser) + ">" + Name(pTarget) + "|" + std::to_string(skillID));
	};
	return host;
}();

struct World : GameModelWorld
{
	FakeCreature ogre, vanda;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		ogre = {1001, "Ogre"};
		vanda = {1002, "Vanda"};
		creatures = {{ogre.id, &ogre}, {vanda.id, &vanda}, {me.id, &me}};
		zoneExists = true;
		hasPlayer = true;
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
		playerStatus = MStatus();
		playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
		calls.clear();
	}
	~World()
	{
		WorldEvents::SetHost(oldEvents);
		PlayerState::SetHost(oldPlayer);
		ZoneState::SetHost(oldZone);
		creatures.clear();
	}
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

void ReceivePlayersCounter(ObjectID_t target, SkillType_t skill)
{
	GCCrossCounterOK1 original;
	original.setObjectID(target);
	original.setSkillType(skill);
	original.addShortData(MODIFY_CURRENT_HP, 55);
	GCCrossCounterOK1 packet;
	Decode(packet, original);
	GCCrossCounterOK1Handler::execute(&packet, nullptr);
}

void ReceiveCounterAtPlayer(ObjectID_t user, SkillType_t skill)
{
	GCCrossCounterOK2 original;
	original.setObjectID(user);
	original.setSkillType(skill);
	original.addShortData(MODIFY_CURRENT_HP, 44);
	GCCrossCounterOK2 packet;
	Decode(packet, original);
	GCCrossCounterOK2Handler::execute(&packet, nullptr);
}

void ReceiveCounter(ObjectID_t user, ObjectID_t target, SkillType_t skill)
{
	GCCrossCounterOK3 original;
	original.setObjectID(user);
	original.setTargetObjectID(target);
	original.setSkillType(skill);
	GCCrossCounterOK3 packet;
	Decode(packet, original);
	GCCrossCounterOK3Handler::execute(&packet, nullptr);
}

const std::string kSkill = std::to_string(SKILL_CROSS_COUNTER);

} // namespace

TEST(CrossCounterReplies, ThePlayersCounterStrikesTheCreatureAndAnswersTheWait)
{
	World world;
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	ReceivePlayersCounter(1001, SKILL_CROSS_COUNTER);
	CHECK(calls == std::vector<std::string>({"verified", "counter|Self>Ogre|" + kSkill}));
	CHECK_EQ(55, World::HP());
	// A creature not in the zone is passed on as none; without a zone
	// nothing is shown, and the status still changes.
	calls.clear();
	ReceivePlayersCounter(4242, SKILL_CROSS_COUNTER);
	playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
	zoneExists = false;
	ReceivePlayersCounter(1001, SKILL_CROSS_COUNTER);
	CHECK(calls == std::vector<std::string>{"counter|Self>none|" + kSkill});
	CHECK_EQ(55, World::HP());
}

TEST(CrossCounterReplies, ACreaturesCounterStrikesThePlayer)
{
	World world;
	ReceiveCounterAtPlayer(1002, SKILL_CROSS_COUNTER);
	CHECK(calls == std::vector<std::string>{"counter|Vanda>Self|" + kSkill});
	CHECK_EQ(44, World::HP());
	calls.clear();
	playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
	zoneExists = false;
	ReceiveCounterAtPlayer(1002, SKILL_CROSS_COUNTER);
	CHECK(calls.empty());
	CHECK_EQ(44, World::HP());
}

TEST(CrossCounterReplies, OneCreaturesCounterStrikesAnother)
{
	World world;
	ReceiveCounter(1002, 1001, SKILL_ATTACK_MELEE);
	ReceiveCounter(4242, 1001, SKILL_ATTACK_MELEE);
	zoneExists = false;
	ReceiveCounter(1002, 1001, SKILL_ATTACK_MELEE);
	const std::string melee = std::to_string(SKILL_ATTACK_MELEE);
	CHECK(calls == std::vector<std::string>({"counter|Vanda>Ogre|" + melee, "counter|none>Ogre|" + melee}));
	CHECK_EQ(10, World::HP());
}

TEST(CrossCounterReplies, WithoutAPlayerThePlayersCountersDoNothing)
{
	World world;
	hasPlayer = false;
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	ReceivePlayersCounter(1001, SKILL_CROSS_COUNTER);
	ReceiveCounterAtPlayer(1002, SKILL_CROSS_COUNTER);
	CHECK(calls.empty());
	CHECK_EQ(10, World::HP());
	// A counter between two creatures does not need one.
	ReceiveCounter(1002, 1001, SKILL_CROSS_COUNTER);
	CHECK_EQ(1u, calls.size());
}
