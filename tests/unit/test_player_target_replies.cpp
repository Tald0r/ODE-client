#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "ConvertDuration.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCBloodDrainOK1.h"
#include "Gpackets/GCBloodDrainOK2.h"
#include "Gpackets/GCKnocksTargetBackOK2.h"
#include "Gpackets/GCThrowItemOK2.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Skill replies between the player and a creature in the zone, each also
// carrying the player's changed status. The player drains a creature: the
// wait for the skill is answered, the creature learns who drains it and is
// hit and marked drained, and the player's drain goes on. A creature drains
// the player, knocks it back, or hits it with holy water: the creature faces
// the player and acts, and the player is hit; a drained player takes the
// effect the status names. Without the creature the player is still hit
// (drain, knock-back) or only takes the status (holy water). Without a
// player nothing happens. The test host runs each handed-over result queue
// at once.
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
	host.Creature = []() { return hasPlayer ? Handle(me) : nullptr; };
	host.GetID = []() { return me.id; };
	host.Status = []() { return &playerStatus; };
	host.GetWaitVerify = []() { return waitVerify; };
	host.SetWaitVerifyNULL = []() {
		calls.push_back("verified");
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
	};
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.SetDirectionToPosition = [](MCreature* p, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) { Record(p, "face|" + N(sX) + "|" + N(sY)); };
	host.SetServerPosition = [](MCreature* p, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) { Record(p, "server|" + N(sX) + "|" + N(sY)); };
	host.MovePosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		Record(p, "move|" + N(x) + "|" + N(y));
		return true;
	};
	host.SetStop = [](MCreature* p) { Record(p, "stop"); };
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
	host.SetDrainCreatureID = [](MCreature* p, TYPE_OBJECTID id) { Record(p, "drainer|" + N(id)); };
	host.ClearStopBloodDrain = [](MCreature* p) { Record(p, "drainon"); };
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
	World()
	{
		ogre = {1001, "Ogre", 10, 11};
		creatures = {{ogre.id, &ogre}, {me.id, &me}};
		zoneExists = true;
		hasPlayer = true;
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
		playerStatus = MStatus();
		playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
		// A player under no effect, as the game keeps it.
		playerStatus.SetStatus(MODIFY_EFFECT_STAT, EFFECTSTATUS_NULL);
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

void ReceiveDrainOK1(ObjectID_t target)
{
	GCBloodDrainOK1 original;
	original.setObjectID(target);
	original.addShortData(MODIFY_CURRENT_HP, 55);
	GCBloodDrainOK1 packet;
	Decode(packet, original);
	GCBloodDrainOK1Handler::execute(&packet, nullptr);
}

void ReceiveDrainOK2(ObjectID_t drainer, EFFECTSTATUS effect = EFFECTSTATUS_NULL, ulong duration = 0)
{
	GCBloodDrainOK2 original;
	original.setObjectID(drainer);
	original.addShortData(MODIFY_CURRENT_HP, 44);
	if (effect != EFFECTSTATUS_NULL)
	{
		original.addLongData(MODIFY_EFFECT_STAT, static_cast<ulong>(effect));
		original.addLongData(MODIFY_DURATION, duration);
	}
	GCBloodDrainOK2 packet;
	Decode(packet, original);
	GCBloodDrainOK2Handler::execute(&packet, nullptr);
}

void ReceiveKnockBack(ObjectID_t attacker, SkillType_t skill, Coord_t x, Coord_t y)
{
	GCKnocksTargetBackOK2 original;
	original.setObjectID(attacker);
	original.setSkillType(skill);
	original.setXYDir(x, y, 3);
	original.addShortData(MODIFY_CURRENT_HP, 33);
	GCKnocksTargetBackOK2 packet;
	Decode(packet, original);
	GCKnocksTargetBackOK2Handler::execute(&packet, nullptr);
}

void ReceiveThrowOK2(ObjectID_t thrower)
{
	GCThrowItemOK2 original;
	original.setObjectID(thrower);
	original.addShortData(MODIFY_CURRENT_HP, 22);
	GCThrowItemOK2 packet;
	Decode(packet, original);
	GCThrowItemOK2Handler::execute(&packet, nullptr);
}

// The hit an action-info node lands on the player, at its cell.
std::vector<std::string> PlayerHit(TYPE_ACTIONINFO skill)
{
	return {"Self.delay|" + N(skill + kMin) + "|0", "Self.result|" + N(skill + kMin) + "|7|30|31"};
}

std::vector<std::string> Then(std::vector<std::string> first, const std::vector<std::string>& rest)
{
	first.insert(first.end(), rest.begin(), rest.end());
	return first;
}

} // namespace

TEST(PlayerTargetReplies, ThePlayersDrainHitsAndMarksTheCreatureAndGoesOn)
{
	World world;
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	ReceiveDrainOK1(1001);
	CHECK(calls == std::vector<std::string>({"verified", "Ogre.drainer|7",
		"Ogre.result|" + N(SKILL_BLOOD_DRAIN + kMin) + "|1001|10|11",
		"Ogre.effect|" + N(EFFECTSTATUS_BLOOD_DRAIN) + "|691200", "Self.drainon"}));
	CHECK_EQ(55, World::HP());
}

TEST(PlayerTargetReplies, ThePlayersDrainWithoutTheCreatureOnlyGoesOn)
{
	World world;
	// Another pending wait stays.
	waitVerify = PlayerState::WAIT_VERIFY_TRADE;
	ReceiveDrainOK1(4242);
	zoneExists = false;
	ReceiveDrainOK1(1001);
	CHECK(calls == std::vector<std::string>({"Self.drainon", "Self.drainon"}));
	CHECK_EQ(static_cast<int>(PlayerState::WAIT_VERIFY_TRADE), static_cast<int>(waitVerify));
	CHECK_EQ(55, World::HP());
}

TEST(PlayerTargetReplies, ACreatureDrainingThePlayerFacesItActsAndHitsIt)
{
	World world;
	ReceiveDrainOK2(1001);
	CHECK(calls == Then({"Self.drainer|1001", "Ogre.drainon", "Ogre.face|30|31", "Ogre.other|" + N(SKILL_BLOOD_DRAIN) + "|7"},
		PlayerHit(SKILL_BLOOD_DRAIN)));
	CHECK_EQ(44, World::HP());
	// Without the creature, the player is still shown drained.
	calls.clear();
	ReceiveDrainOK2(4242);
	CHECK(calls == std::vector<std::string>({"Self.drainer|4242", "Self.result|" + N(RESULT_SKILL_BLOOD_DRAIN) + "|7|30|31"}));
	// Without a zone only the status changes.
	calls.clear();
	zoneExists = false;
	ReceiveDrainOK2(1001);
	CHECK(calls.empty());
}

TEST(PlayerTargetReplies, ADrainedPlayerTakesTheEffectItsStatusNames)
{
	World world;
	zoneExists = false;
	ReceiveDrainOK2(1001, EFFECTSTATUS_BLOOD_DRAIN, 30);
	CHECK(calls == std::vector<std::string>{"Self.effect|" + N(EFFECTSTATUS_BLOOD_DRAIN) + "|" + N(ConvertDurationToFrame(30))});
	CHECK_EQ(static_cast<long long>(EFFECTSTATUS_NULL), static_cast<long long>(playerStatus.GetStatus(MODIFY_EFFECT_STAT)));
}

TEST(PlayerTargetReplies, AKnockBackMovesStopsAndHitsThePlayer)
{
	World world;
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	ReceiveKnockBack(1001, SKILL_DOUBLE_IMPACT, 14, 15);
	const std::string hit = N(SKILL_DOUBLE_IMPACT + kMin);
	CHECK(calls == std::vector<std::string>({"verified", "Ogre.face|30|31", "Ogre.other|" + N(SKILL_DOUBLE_IMPACT) + "|7", "(no result)",
		"Self.server|14|15", "Self.move|14|15", "Self.stop", "Self.result|" + hit + "|1001|10|11"}));
	CHECK_EQ(33, World::HP());
	// Without the attacker the player is still knocked back.
	calls.clear();
	ReceiveKnockBack(4242, SKILL_DOUBLE_IMPACT, 16, 17);
	CHECK(calls == std::vector<std::string>({"Self.server|16|17", "Self.move|16|17", "Self.stop",
		"Self.result|" + hit + "|" + N(OBJECTID_NULL) + "|16|17"}));
}

TEST(PlayerTargetReplies, AKnockBackWithoutAZoneOrSkillOnlyAnswersTheWait)
{
	World world;
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	ReceiveKnockBack(1001, MAX_ACTIONINFO, 14, 15);
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	zoneExists = false;
	ReceiveKnockBack(1001, SKILL_DOUBLE_IMPACT, 14, 15);
	CHECK(calls == std::vector<std::string>({"verified", "verified"}));
	// Both return before the status is applied.
	CHECK_EQ(10, World::HP());
}

TEST(PlayerTargetReplies, HolyWaterThrownAtThePlayerHitsIt)
{
	World world;
	ReceiveThrowOK2(1001);
	CHECK(calls == Then({"Ogre.face|30|31", "Ogre.other|" + N(MAGIC_THROW_HOLY_WATER) + "|7"}, PlayerHit(MAGIC_THROW_HOLY_WATER)));
	CHECK_EQ(22, World::HP());
	calls.clear();
	ReceiveThrowOK2(4242);
	zoneExists = false;
	ReceiveThrowOK2(1001);
	CHECK(calls.empty());
}

TEST(PlayerTargetReplies, WithoutAPlayerNothingHappens)
{
	World world;
	hasPlayer = false;
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	ReceiveDrainOK1(1001);
	ReceiveDrainOK2(1001);
	ReceiveKnockBack(1001, SKILL_DOUBLE_IMPACT, 1, 1);
	ReceiveThrowOK2(1001);
	CHECK(calls.empty());
	CHECK_EQ(10, World::HP());
	CHECK_EQ(static_cast<int>(PlayerState::WAIT_VERIFY_SKILL_SUCCESS), static_cast<int>(waitVerify));
}
