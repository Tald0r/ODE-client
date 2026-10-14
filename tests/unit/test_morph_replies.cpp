#include "test_framework.h"
#include "gamemodel_world.h"
#include "ExtraInfo.h"
#include "GearInfo.h"
#include "InventoryInfo.h"
#include "MQuickSlot.h"
#include "PCOustersInfo2.h"
#include "PCSlayerInfo2.h"
#include "PCVampireInfo2.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCMorph1.h"
#include <string>
#include <vector>

// The player transforming. With sound muted, the player takes the ID of
// the packet's slayer or vampire (leaving the zone under its old one and
// joining it again under the new), turns from a slayer into a vampire
// (in guild 2) or from anything else into a slayer (in no guild, -1),
// loses its quick slot and arms bands, attacks with its bare hands at a
// reach of one cell, and takes its details, gear, inventory and the item
// at the mouse from the packet, its status calculated between the gear
// and the inventory; then sound plays again. An ouster's info, or any
// other the reply does not take, is refused with the sound left as it
// was. Without a zone or a player nothing happens.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool hasPlayer = true;
Race race = RACE_SLAYER;
TYPE_OBJECTID myID = 7;
int me;

// What the packet carries, to see the reply pass it on.
const void* gear;
const void* inventory;
const void* extra;

std::string N(long long value) { return std::to_string(value); }

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? reinterpret_cast<MCreature*>(&me) : nullptr; };
	host.GetID = []() { return myID; };
	host.GetRace = []() { return race; };
	host.ChangeToSlayer = []() { calls.push_back("to slayer"); };
	host.ChangeToVampire = []() { calls.push_back("to vampire"); };
	host.SetGuildNumber = [](int guild) { calls.push_back("guild|" + N(guild)); };
	host.SetBasicAttack = [](TYPE_ACTIONINFO actionInfo, int distance) {
		calls.push_back("basic attack|" + N(actionInfo) + "|" + N(distance));
	};
	host.SetPCSlayerInfo = [](PCSlayerInfo2* pInfo) { calls.push_back("slayer info|" + N(pInfo->getObjectID())); };
	host.SetPCVampireInfo = [](PCVampireInfo2* pInfo) { calls.push_back("vampire info|" + N(pInfo->getObjectID())); };
	host.SetPCOustersInfo = [](PCOustersInfo2* pInfo) { calls.push_back("ousters info|" + N(pInfo->getObjectID())); };
	host.SetGearInfo = [](GearInfo* pGearInfo, BloodBibleSignInfo* pBloodBibleInfo) {
		calls.push_back(std::string("gear|") + (pGearInfo == gear ? "packet's" : "other") + (pBloodBibleInfo ? "|bible" : "|no bible"));
	};
	host.CalculateStatus = []() { calls.push_back("calculated"); };
	host.SetInventoryInfo = [](InventoryInfo* pInfo) { calls.push_back(std::string("inventory|") + (pInfo == inventory ? "packet's" : "other")); };
	host.SetExtraInfo = [](ExtraInfo* pInfo) { calls.push_back(std::string("extra|") + (pInfo == extra ? "packet's" : "other")); };
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.RemoveCreature = [](TYPE_OBJECTID id) { calls.push_back("zone removes|" + N(id)); return true; };
	host.SetPlayer = []() { calls.push_back("zone takes the player"); };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.SetID = [](MCreature* p, TYPE_OBJECTID id) {
		CHECK(p == reinterpret_cast<MCreature*>(&me));
		calls.push_back("id|" + N(id));
		myID = id;
	};
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.MuteSound = [](bool bMute) { calls.push_back(bMute ? "muted" : "unmuted"); };
	return host;
}();

struct World : GameModelWorld
{
	// Never read: the reply only lets go of them.
	int belt = 0, band1 = 0, band2 = 0;
	MBelt* oldQuickSlot = g_pQuickSlot;
	MOustersArmsBand* oldBand1 = g_pArmsBand1;
	MOustersArmsBand* oldBand2 = g_pArmsBand2;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		calls.clear();
		zoneExists = true;
		hasPlayer = true;
		race = RACE_SLAYER;
		myID = 7;
		g_pQuickSlot = reinterpret_cast<MBelt*>(&belt);
		g_pArmsBand1 = reinterpret_cast<MOustersArmsBand*>(&band1);
		g_pArmsBand2 = reinterpret_cast<MOustersArmsBand*>(&band2);
	}
	~World()
	{
		WorldEvents::SetHost(oldEvents);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		PlayerState::SetHost(oldPlayer);
		g_pArmsBand2 = oldBand2;
		g_pArmsBand1 = oldBand1;
		g_pQuickSlot = oldQuickSlot;
	}
	bool Unequipped() const { return g_pQuickSlot == NULL && g_pArmsBand1 == NULL && g_pArmsBand2 == NULL; }
};

template <class Info>
void Receive(TYPE_OBJECTID id)
{
	Info* pInfo = new Info;
	pInfo->setObjectID(id);
	GCMorph1 packet;
	packet.setPCInfo2(pInfo);
	packet.setGearInfo(new GearInfo);
	packet.setInventoryInfo(new InventoryInfo);
	packet.setExtraInfo(new ExtraInfo);
	gear = packet.getGearInfo();
	inventory = packet.getInventoryInfo();
	extra = packet.getExtraInfo();
	GCMorph1Handler::execute(&packet, nullptr);
}

std::vector<std::string> Then(std::vector<std::string> first, const std::vector<std::string>& rest)
{
	first.insert(first.end(), rest.begin(), rest.end());
	return first;
}

// What follows the turning, for the details the packet gives.
std::vector<std::string> Equipped(const std::string& details)
{
	return {"basic attack|" + N(SKILL_ATTACK_MELEE) + "|1", details, "gear|packet's|no bible", "calculated", "inventory|packet's",
		"extra|packet's", "unmuted"};
}

} // namespace

TEST(MorphReplies, ASlayerBecomesAVampire)
{
	World world;
	Receive<PCVampireInfo2>(7);
	CHECK(calls == Then({"muted", "to vampire", "guild|2"}, Equipped("vampire info|7")));
	CHECK(world.Unequipped());
}

TEST(MorphReplies, AnythingElseBecomesASlayer)
{
	World world;
	race = RACE_VAMPIRE;
	Receive<PCSlayerInfo2>(7);
	CHECK(calls == Then({"muted", "to slayer", "guild|-1"}, Equipped("slayer info|7")));
	CHECK(world.Unequipped());
	calls.clear();
	race = RACE_OUSTERS;
	Receive<PCSlayerInfo2>(7);
	CHECK(calls == Then({"muted", "to slayer", "guild|-1"}, Equipped("slayer info|7")));
}

TEST(MorphReplies, ANewIDMovesThePlayerInTheZone)
{
	World world;
	Receive<PCVampireInfo2>(8);
	CHECK(calls == Then({"muted", "zone removes|7", "id|8", "zone takes the player", "to vampire", "guild|2"}, Equipped("vampire info|8")));
	CHECK_EQ(8u, myID);
	calls.clear();
	race = RACE_VAMPIRE;
	Receive<PCSlayerInfo2>(9);
	CHECK(calls == Then({"muted", "zone removes|8", "id|9", "zone takes the player", "to slayer", "guild|-1"}, Equipped("slayer info|9")));
}

TEST(MorphReplies, AnOustersInfoIsRefusedLeavingTheSoundAlone)
{
	World world;
	race = RACE_OUSTERS;
	Receive<PCOustersInfo2>(8);
	CHECK(calls.empty());
	CHECK(!world.Unequipped());
	CHECK_EQ(7u, myID);
}

TEST(MorphReplies, WithoutAZoneOrAPlayerNothingHappens)
{
	World world;
	zoneExists = false;
	Receive<PCVampireInfo2>(8);
	zoneExists = true;
	hasPlayer = false;
	Receive<PCVampireInfo2>(8);
	CHECK(calls.empty());
	CHECK(!world.Unequipped());
	// Without the hosts the reply still lets go of the quick slot.
	hasPlayer = true;
	PlayerState::Host bare{};
	bare.Creature = player.Creature;
	PlayerState::SetHost(&bare);
	WorldEvents::SetHost(nullptr);
	Receive<PCSlayerInfo2>(8);
	CHECK(calls == std::vector<std::string>({"zone removes|0", "id|8", "zone takes the player"}));
	CHECK(world.Unequipped());
}
