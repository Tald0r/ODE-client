#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
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
#include "Gpackets/GCAttackArmsOK1.h"
#include "Gpackets/GCAttackArmsOK2.h"
#include "Gpackets/GCAttackMeleeOK1.h"
#include "Gpackets/GCAttackMeleeOK2.h"
#include "Gpackets/GCKnocksTargetBackOK1.h"
#include "Gpackets/GCThrowItemOK1.h"
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

// Attacks between the player and a creature in the zone. The player's own
// attack (a gun, a melee blow, a knock-back, holy water) answers the wait
// for it and hands the player's current action its result: the creature
// hit with the skill or the player's basic action, or moved. A gun's
// magazine takes the bullets left. A creature's attack on the player (a
// gun, a melee blow) faces the player and acts, the player hit with the
// creature's skill or basic action; without the creature the player is
// still shown hit. Each but holy water also carries the player's status.
// The test hosts run every handed-over result queue at once.
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
};
std::map<TYPE_OBJECTID, FakeCreature*> creatures;
// The player's and the ogre's basic actions differ from every constant the
// replies name.
FakeCreature me{7, "Self", 30, 31, SKILL_ATTACK_GUN_SR};

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
	host.PacketAddActionResult = [](std::uint16_t effectID, MActionResult* pResult) {
		calls.push_back("player.add|" + N(effectID));
		Land(pResult);
		return true;
	};
	host.SetCurrentMagazineNumber = [](int number) { calls.push_back("magazine|" + N(number)); };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.GetBasicActionInfo = [](MCreature* p) { return Fake(p).basic; };
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
	host.PacketSpecialActionToOther = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_OBJECTID id, MActionResult* pResult) {
		Record(p, "other|" + N(ai) + "|" + N(id));
		Land(pResult);
	};
	return host;
}();

constexpr TYPE_ACTIONINFO kMin = MIN_RESULT_ACTIONINFO;
const std::initializer_list<TYPE_ACTIONINFO> kOwnGunSkills = {SKILL_JABBING_VEIN, SKILL_MOLE_SHOT, SKILL_TRIDENT,
	SKILL_QUICK_FIRE, SKILL_ULTIMATE_BLOW, SKILL_HARPOON_BOMB};

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
		ogre = {1001, "Ogre", 10, 11, SKILL_ATTACK_GUN_SG};
		creatures = {{ogre.id, &ogre}, {me.id, &me}};
		zoneExists = true;
		hasPlayer = true;
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
		playerStatus = MStatus();
		playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
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

void ReceiveShot(ObjectID_t target, SkillType_t skill, bool success = true)
{
	GCAttackArmsOK1 original;
	original.setObjectID(target);
	original.setSkillType(skill);
	original.setSkillSuccess(success);
	original.setBulletNum(17);
	original.addShortData(MODIFY_CURRENT_HP, 55);
	GCAttackArmsOK1 packet;
	Decode(packet, original);
	GCAttackArmsOK1Handler::execute(&packet, nullptr);
}

void ReceiveBlow(ObjectID_t target)
{
	GCAttackMeleeOK1 original;
	original.setObjectID(target);
	original.addShortData(MODIFY_CURRENT_HP, 44);
	GCAttackMeleeOK1 packet;
	Decode(packet, original);
	GCAttackMeleeOK1Handler::execute(&packet, nullptr);
}

void ReceiveKnockBack(ObjectID_t target, Coord_t x, Coord_t y, bool success = true)
{
	GCKnocksTargetBackOK1 original;
	original.setObjectID(target);
	original.setSkillType(SKILL_DOUBLE_IMPACT);
	original.setSkillSuccess(success);
	original.setBulletNum(9);
	original.setXYDir(x, y, 2);
	original.addShortData(MODIFY_CURRENT_HP, 33);
	GCKnocksTargetBackOK1 packet;
	Decode(packet, original);
	GCKnocksTargetBackOK1Handler::execute(&packet, nullptr);
}

void ReceiveThrow(ObjectID_t target)
{
	GCThrowItemOK1 original;
	original.setObjectID(target);
	GCThrowItemOK1 packet;
	Decode(packet, original);
	GCThrowItemOK1Handler::execute(&packet, nullptr);
}

void ReceiveShotAtPlayer(ObjectID_t attacker, SkillType_t skill)
{
	GCAttackArmsOK2 original;
	original.setObjectID(attacker);
	original.setSkillType(skill);
	original.addShortData(MODIFY_CURRENT_HP, 66);
	GCAttackArmsOK2 packet;
	Decode(packet, original);
	GCAttackArmsOK2Handler::execute(&packet, nullptr);
}

void ReceiveBlowAtPlayer(ObjectID_t attacker)
{
	GCAttackMeleeOK2 original;
	original.setObjectID(attacker);
	original.addShortData(MODIFY_CURRENT_HP, 77);
	GCAttackMeleeOK2 packet;
	Decode(packet, original);
	GCAttackMeleeOK2Handler::execute(&packet, nullptr);
}

std::vector<std::string> OgreHit(TYPE_ACTIONINFO action)
{
	return {"Ogre.delay|" + N(action + kMin) + "|0", "Ogre.result|" + N(action + kMin) + "|1001|10|11"};
}

std::vector<std::string> PlayerHit(TYPE_ACTIONINFO action)
{
	return {"Self.delay|" + N(action + kMin) + "|0", "Self.result|" + N(action + kMin) + "|7|30|31"};
}

std::vector<std::string> Then(std::vector<std::string> first, const std::vector<std::string>& rest)
{
	first.insert(first.end(), rest.begin(), rest.end());
	return first;
}

} // namespace

TEST(PlayerAttackReplies, ThePlayersShotHitsWithItsSkillOrBasicActionAndSpendsBullets)
{
	World world;
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	ReceiveShot(1001, SKILL_DOUBLE_IMPACT);
	CHECK(calls == Then(Then({"verified", "player.add|0"}, OgreHit(SKILL_ATTACK_GUN_SR)), {"magazine|17"}));
	CHECK_EQ(55, World::HP());
	for (TYPE_ACTIONINFO skill : kOwnGunSkills)
	{
		calls.clear();
		ReceiveShot(1001, skill);
		CHECK(calls == Then(Then({"player.add|0"}, OgreHit(skill)), {"magazine|17"}));
	}
}

TEST(PlayerAttackReplies, AMissedShotOrNoTargetOnlySpendsBullets)
{
	World world;
	ReceiveShot(1001, SKILL_DOUBLE_IMPACT, false);
	ReceiveShot(4242, SKILL_DOUBLE_IMPACT);
	CHECK(calls == std::vector<std::string>({"magazine|17", "magazine|17"}));
	// Without a zone the status still changes, and nothing else.
	calls.clear();
	zoneExists = false;
	playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
	ReceiveShot(1001, SKILL_DOUBLE_IMPACT);
	CHECK(calls.empty());
	CHECK_EQ(55, World::HP());
}

TEST(PlayerAttackReplies, ThePlayersBlowHitsWithItsBasicAction)
{
	World world;
	ReceiveBlow(1001);
	CHECK(calls == Then({"player.add|0"}, OgreHit(SKILL_ATTACK_GUN_SR)));
	CHECK_EQ(44, World::HP());
	calls.clear();
	ReceiveBlow(4242);
	zoneExists = false;
	ReceiveBlow(1001);
	CHECK(calls.empty());
}

TEST(PlayerAttackReplies, ThePlayersKnockBackMovesTheCreature)
{
	World world;
	ReceiveKnockBack(1001, 14, 15);
	CHECK(calls == std::vector<std::string>({"player.add|0", "Ogre.release", "Ogre.move|14|15", "Ogre.server|14|15",
		"Ogre.server|14|15", "magazine|9"}));
	CHECK_EQ(33, World::HP());
	calls.clear();
	ReceiveKnockBack(1001, 14, 15, false);
	ReceiveKnockBack(4242, 14, 15);
	CHECK(calls == std::vector<std::string>({"magazine|9", "magazine|9"}));
}

TEST(PlayerAttackReplies, ThePlayersHolyWaterHits)
{
	World world;
	ReceiveThrow(1001);
	CHECK(calls == Then({"player.add|0"}, OgreHit(MAGIC_THROW_HOLY_WATER)));
	CHECK_EQ(10, World::HP());
	calls.clear();
	ReceiveThrow(4242);
	CHECK(calls.empty());
}

TEST(PlayerAttackReplies, ACreaturesShotFacesThePlayerAndHitsIt)
{
	World world;
	ReceiveShotAtPlayer(1001, SKILL_DOUBLE_IMPACT);
	CHECK(calls == Then({"Ogre.face|30|31", "Ogre.other|" + N(SKILL_ATTACK_GUN_SG) + "|7"}, PlayerHit(SKILL_ATTACK_GUN_SG)));
	CHECK_EQ(66, World::HP());
	for (TYPE_ACTIONINFO skill : kOwnGunSkills)
	{
		calls.clear();
		ReceiveShotAtPlayer(1001, skill);
		CHECK(calls == Then({"Ogre.face|30|31", "Ogre.other|" + N(skill) + "|7"}, PlayerHit(skill)));
		// Without the attacker the player is hit by the skill's own result.
		calls.clear();
		ReceiveShotAtPlayer(4242, skill);
		CHECK(calls == std::vector<std::string>{"Self.result|" + N(skill + kMin) + "|7|30|31"});
	}
	calls.clear();
	ReceiveShotAtPlayer(4242, SKILL_DOUBLE_IMPACT);
	CHECK(calls == std::vector<std::string>{"Self.result|" + N(RESULT_SKILL_ATTACK_GUN_AR) + "|7|30|31"});
}

TEST(PlayerAttackReplies, ACreaturesBlowFacesThePlayerAndHitsIt)
{
	World world;
	ReceiveBlowAtPlayer(1001);
	CHECK(calls == Then({"Ogre.face|30|31", "Ogre.other|" + N(SKILL_ATTACK_GUN_SG) + "|7"}, PlayerHit(SKILL_ATTACK_GUN_SG)));
	CHECK_EQ(77, World::HP());
	calls.clear();
	ReceiveBlowAtPlayer(4242);
	CHECK(calls == std::vector<std::string>{"Self.result|" + N(RESULT_SKILL_ATTACK_MELEE) + "|7|30|31"});
}

TEST(PlayerAttackReplies, ACreaturesAttackWithoutAZoneChangesNothing)
{
	World world;
	zoneExists = false;
	ReceiveShotAtPlayer(1001, SKILL_DOUBLE_IMPACT);
	ReceiveBlowAtPlayer(1001);
	CHECK(calls.empty());
	// Both return before the status.
	CHECK_EQ(10, World::HP());
}

TEST(PlayerAttackReplies, WithoutAPlayerNothingHappens)
{
	World world;
	hasPlayer = false;
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	ReceiveShot(1001, SKILL_DOUBLE_IMPACT);
	ReceiveBlow(1001);
	ReceiveKnockBack(1001, 1, 1);
	ReceiveThrow(1001);
	ReceiveShotAtPlayer(1001, SKILL_DOUBLE_IMPACT);
	ReceiveBlowAtPlayer(1001);
	CHECK(calls.empty());
	CHECK_EQ(10, World::HP());
}

namespace {

// A node that counts its own destruction, to see a queue freed.
int freed = 0;
struct CountedNode : MActionResultNode
{
	~CountedNode() override { freed++; }
	void Execute() override { calls.push_back("ran"); }
};

} // namespace

TEST(PlayerAttackReplies, WithoutTheEntriesTheQueueIsFreedUnrunAndNoBulletsAreSet)
{
	World world;
	PlayerState::Host partial = player;
	partial.PacketAddActionResult = nullptr;
	partial.SetCurrentMagazineNumber = nullptr;
	PlayerState::SetHost(&partial);
	ReceiveShot(1001, SKILL_DOUBLE_IMPACT);
	CHECK(calls.empty());
	CHECK_EQ(55, World::HP());
	freed = 0;
	MActionResult* pResult = new MActionResult;
	pResult->Add(new CountedNode);
	CHECK(!PlayerState::PacketAddActionResult(0, pResult));
	CHECK_EQ(1, freed);
	PlayerState::SetHost(nullptr);
	pResult = new MActionResult;
	pResult->Add(new CountedNode);
	CHECK(!PlayerState::PacketAddActionResult(0, pResult));
	CHECK_EQ(2, freed);
	CHECK(calls.empty());
}
