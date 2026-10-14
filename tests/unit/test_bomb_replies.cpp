#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
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
#include "Gpackets/GCThrowBombOK1.h"
#include "Gpackets/GCThrowBombOK2.h"
#include "Gpackets/GCThrowBombOK3.h"
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

// A bomb item's type names its action. A bomb lands at a cell: the result
// queue hits the cell (the thrower, or nobody when the action's result is
// an action of its own), then for each creature it lands on adds the
// action's effect status and the node its result kind names (its own action
// on the creature, or the factory's node). The player's bomb answers the
// wait and goes to the player's current action; another creature's bomb
// is thrown from its user facing the cell, or, without the user, starts at
// the cell; the player's pending effect status rides along on any bomb
// near the player. The test hosts run every handed-over queue at once.
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
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.GetBasicActionInfo = [](MCreature* p) { return Fake(p).basic; };
	host.SetDirectionToPosition = [](MCreature* p, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) { Record(p, "face|" + N(sX) + "|" + N(sY)); };
	host.SetDirection = [](MCreature* p, int direction) { Record(p, "dir|" + N(direction)); };
	host.SetEffectDelayFrame = [](MCreature* p, TYPE_ACTIONINFO ai, std::uint32_t frames) { Record(p, "delay|" + N(ai) + "|" + N(frames)); };
	host.PacketSpecialActionResult = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t) {
		Record(p, "result|" + N(ai) + "|" + N(id) + "|" + N(sX) + "|" + N(sY));
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
constexpr int kAcer = 1;	// the second bomb item type

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
		ogre = {1001, "Ogre", 10, 11, SKILL_ATTACK_MELEE};
		vanda = {1002, "Vanda", 20, 21, SKILL_ATTACK_GUN_SG};
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

void ReceivePlayersBomb(int itemType, std::initializer_list<ObjectID_t> targets)
{
	GCThrowBombOK1 original;
	original.setXYDir(40, 41, 2);
	original.setItemType(static_cast<ItemType_t>(itemType));
	for (ObjectID_t id : targets) original.addCListElement(id);
	original.addShortData(MODIFY_CURRENT_HP, 55);
	GCThrowBombOK1 packet;
	Decode(packet, original);
	GCThrowBombOK1Handler::execute(&packet, nullptr);
}

void ReceiveBombNearPlayer(ObjectID_t user, int itemType, std::initializer_list<ObjectID_t> targets)
{
	GCThrowBombOK2 original;
	original.setObjectID(user);
	original.setXYDir(40, 41, 2);
	original.setItemType(static_cast<ItemType_t>(itemType));
	for (ObjectID_t id : targets) original.addCListElement(id);
	original.addShortData(MODIFY_CURRENT_HP, 44);
	GCThrowBombOK2 packet;
	Decode(packet, original);
	GCThrowBombOK2Handler::execute(&packet, nullptr);
}

void ReceiveBomb(ObjectID_t user, int itemType, std::initializer_list<ObjectID_t> targets)
{
	GCThrowBombOK3 original;
	original.setObjectID(user);
	original.setXYDir(40, 41, 2);
	original.setItemType(static_cast<ItemType_t>(itemType));
	for (ObjectID_t id : targets) original.addCListElement(id);
	GCThrowBombOK3 packet;
	Decode(packet, original);
	GCThrowBombOK3Handler::execute(&packet, nullptr);
}

std::string Hit(const char* who, TYPE_ACTIONINFO action, TYPE_OBJECTID id, int x, int y)
{
	return std::string(who) + ".delay|" + N(action + kMin) + "|16" + "&" + std::string(who) + ".result|" + N(action + kMin) + "|" + N(id) + "|" + N(x) + "|" + N(y);
}

// Splits Hit's two records.
std::vector<std::string> Then(std::vector<std::string> first, const std::vector<std::string>& rest)
{
	std::vector<std::string> out;
	for (const std::vector<std::string>* part : {static_cast<const std::vector<std::string>*>(&first), &rest})
		for (const std::string& s : *part)
		{
			const size_t amp = s.find('&');
			if (amp == std::string::npos) { out.push_back(s); continue; }
			out.push_back(s.substr(0, amp));
			out.push_back(s.substr(amp + 1));
		}
	return out;
}

} // namespace

TEST(BombReplies, AMineOrBombItemTypeNamesItsAction)
{
	const TYPE_ACTIONINFO bombs[] = {BOMB_SPLINTER, BOMB_ACER, BOMB_BULLS, BOMB_STUN, BOMB_CROSSBOW};
	const TYPE_ACTIONINFO mines[] = {MINE_ANKLE_KILLER, MINE_POMZ, MINE_AP_C1, MINE_DIAMONDBACK, MINE_SWIFT_EX};
	for (int type = 0; type < 5; type++)
	{
		CHECK_EQ(static_cast<int>(bombs[type]), GetBombActionInfo(type));
		CHECK_EQ(static_cast<int>(mines[type]), GetMineActionInfo(type));
	}
	for (int type : {-1, 5, 255})
	{
		CHECK_EQ(-1, GetBombActionInfo(type));
		CHECK_EQ(-1, GetMineActionInfo(type));
	}
}

TEST(BombReplies, ThePlayersBombHitsItsCellAndEachCreatureItLandsOn)
{
	World world;
	world.Row(BOMB_ACER).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	world.Row(BOMB_ACER).SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	ReceivePlayersBomb(kAcer, {1001, 4242});
	CHECK(calls == Then({"verified", "player.add|0", Hit("Self", BOMB_ACER, 7, 40, 41)},
		{"Ogre.effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|16", "Ogre.burrow"}));
	CHECK_EQ(55, World::HP());
}

TEST(BombReplies, ABombWhoseResultIsAnActionHitsNobodyAtTheCellAndActsOnEachCreature)
{
	World world;
	world.Row(BOMB_ACER).SetActionResult(ACTIONRESULTNODE_ACTIONINFO);
	ReceivePlayersBomb(kAcer, {1001});
	CHECK(calls == Then({"player.add|0"}, {Hit("Ogre", BOMB_ACER, 1001, 10, 11)}));
	// A weapon bomb acts as the player's basic action on each creature.
	calls.clear();
	world.Row(BOMB_ACER).SetAffectCurrentWeaponAction();
	ReceivePlayersBomb(kAcer, {1001});
	CHECK(calls == Then({"player.add|0"}, {Hit("Ogre", SKILL_ATTACK_GUN_SR, 1001, 10, 11)}));
	// Unless that is a result.
	calls.clear();
	me.basic = kMin;
	ReceivePlayersBomb(kAcer, {1001});
	me.basic = SKILL_ATTACK_GUN_SR;
	CHECK(calls.empty());
}

TEST(BombReplies, ThePlayersBombOfAnUnknownTypeOrWithoutAZoneOnlyChangesTheStatus)
{
	World world;
	ReceivePlayersBomb(9, {1001});
	CHECK_EQ(55, World::HP());
	playerStatus.SetStatus(MODIFY_CURRENT_HP, 10);
	zoneExists = false;
	ReceivePlayersBomb(kAcer, {1001});
	CHECK_EQ(55, World::HP());
	CHECK(calls.empty());
}

TEST(BombReplies, ACreaturesBombIsThrownTowardsItsCell)
{
	World world;
	world.Row(BOMB_ACER).SetEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS);
	ReceiveBombNearPlayer(1002, kAcer, {1001});
	CHECK(calls == Then({"Vanda.face|40|41", "Vanda.dir|2", "Vanda.sector|" + N(BOMB_ACER) + "|40|41", Hit("Vanda", BOMB_ACER, 1002, 40, 41)},
		{"Ogre.effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|16"}));
	CHECK_EQ(44, World::HP());
	// The same between two creatures, without the player.
	calls.clear();
	hasPlayer = false;
	ReceiveBomb(1002, kAcer, {1001});
	CHECK(calls == Then({"Vanda.face|40|41", "Vanda.dir|2", "Vanda.sector|" + N(BOMB_ACER) + "|40|41", Hit("Vanda", BOMB_ACER, 1002, 40, 41)},
		{"Ogre.effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|16"}));
}

TEST(BombReplies, WithoutItsUserACreaturesBombStartsAtTheCell)
{
	World world;
	world.Row(BOMB_ACER).SetActionResult(ACTIONRESULTNODE_ACTIONINFO);
	ReceiveBombNearPlayer(4242, kAcer, {1001});
	CHECK(calls == Then({"cell|" + N(BOMB_ACER) + "|40,41|dir 2|" + N(OBJECTID_NULL) + "@40,41|16|resume"},
		{Hit("Ogre", BOMB_ACER, 1001, 10, 11)}));
	// Between creatures, nothing is shown without the user.
	calls.clear();
	ReceiveBomb(4242, kAcer, {1001});
	CHECK(calls.empty());
}

TEST(BombReplies, ThePlayersPendingEffectRidesOnABombNearThePlayer)
{
	World world;
	playerStatus.SetStatus(MODIFY_EFFECT_STAT, EFFECTSTATUS_CURSE_PARALYSIS);
	ReceiveBombNearPlayer(1002, kAcer, {});
	CHECK(calls == Then({"Vanda.face|40|41", "Vanda.dir|2", "Vanda.sector|" + N(BOMB_ACER) + "|40|41",
		"Self.effect|" + N(EFFECTSTATUS_CURSE_PARALYSIS) + "|16", Hit("Vanda", BOMB_ACER, 1002, 40, 41)}, {}));
	CHECK_EQ(static_cast<long long>(EFFECTSTATUS_NULL), static_cast<long long>(playerStatus.GetStatus(MODIFY_EFFECT_STAT)));
}

TEST(BombReplies, ACreaturesBombWhoseResultIsAnActionHitsNobodyAtTheCellAndActsOnEachCreature)
{
	World world;
	world.Row(BOMB_ACER).SetActionResult(ACTIONRESULTNODE_ACTIONINFO);
	const std::vector<std::string> thrown = {"Vanda.face|40|41", "Vanda.dir|2", "Vanda.sector|" + N(BOMB_ACER) + "|40|41"};
	ReceiveBombNearPlayer(1002, kAcer, {1001});
	CHECK(calls == Then(thrown, {Hit("Ogre", BOMB_ACER, 1001, 10, 11)}));
	calls.clear();
	ReceiveBomb(1002, kAcer, {1001});
	CHECK(calls == Then(thrown, {Hit("Ogre", BOMB_ACER, 1001, 10, 11)}));
}

TEST(BombReplies, AWeaponBombActsAsItsThrowersBasicAction)
{
	World world;
	world.Row(BOMB_ACER).SetAffectCurrentWeaponAction();
	world.Row(BOMB_ACER).SetActionResult(ACTIONRESULTNODE_ACTIONINFO);
	const std::vector<std::string> thrown = {"Vanda.face|40|41", "Vanda.dir|2", "Vanda.sector|" + N(BOMB_ACER) + "|40|41"};
	ReceiveBombNearPlayer(1002, kAcer, {1001});
	CHECK(calls == Then(thrown, {Hit("Ogre", SKILL_ATTACK_GUN_SG, 1001, 10, 11)}));
	calls.clear();
	ReceiveBomb(1002, kAcer, {1001});
	CHECK(calls == Then(thrown, {Hit("Ogre", SKILL_ATTACK_GUN_SG, 1001, 10, 11)}));
	// Without its user, the bomb acts as itself.
	calls.clear();
	ReceiveBombNearPlayer(4242, kAcer, {1001});
	CHECK(calls == Then({"cell|" + N(BOMB_ACER) + "|40,41|dir 2|" + N(OBJECTID_NULL) + "@40,41|16|resume"},
		{Hit("Ogre", BOMB_ACER, 1001, 10, 11)}));
	// A thrower whose basic action is a result shows nothing.
	calls.clear();
	world.vanda.basic = kMin;
	ReceiveBombNearPlayer(1002, kAcer, {1001});
	ReceiveBomb(1002, kAcer, {1001});
	CHECK(calls.empty());
}

TEST(BombReplies, AWeaponBombHitsItsCellAsItselfAndEachCreatureAsTheWeapon)
{
	World world;
	world.Row(BOMB_ACER).SetAffectCurrentWeaponAction();
	world.Row(SKILL_ATTACK_GUN_SR).SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	world.Row(SKILL_ATTACK_GUN_SG).SetActionResult(ACTIONRESULTNODE_CREATURE_BURROW);
	ReceivePlayersBomb(kAcer, {1001});
	CHECK(calls == Then({"player.add|0", Hit("Self", BOMB_ACER, 7, 40, 41)}, {"Ogre.burrow"}));
	calls.clear();
	ReceiveBomb(1002, kAcer, {1001});
	CHECK(calls == Then({"Vanda.face|40|41", "Vanda.dir|2", "Vanda.sector|" + N(BOMB_ACER) + "|40|41", Hit("Vanda", BOMB_ACER, 1002, 40, 41)},
		{"Ogre.burrow"}));
}

TEST(BombReplies, TheGuards)
{
	World world;
	// A bomb the packet says the player threw is not shown twice.
	ReceiveBombNearPlayer(7, kAcer, {1001});
	ReceiveBombNearPlayer(1002, 9, {1001});
	ReceiveBomb(1002, 9, {1001});
	zoneExists = false;
	ReceiveBombNearPlayer(1002, kAcer, {1001});
	ReceiveBomb(1002, kAcer, {1001});
	CHECK(calls.empty());
	hasPlayer = false;
	zoneExists = true;
	ReceivePlayersBomb(kAcer, {1001});
	ReceiveBombNearPlayer(1002, kAcer, {1001});
	CHECK(calls.empty());
}
