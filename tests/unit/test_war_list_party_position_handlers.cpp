#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "CMessageArray.h"
#include "ClientMode.h"
#include "GameMessages.h"
#include "MEventQueue.h"
#include "MParty.h"
#include "MWarManager.h"
#include "PlayerStateHost.h"
#include "RequestUserManager.h"
#include "WorldEventsHost.h"
#include "ZoneFlags.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "GuildWarInfo.h"
#include "RaceWarInfo.h"
#include "Gpackets/GCPartyPosition.h"
#include "Gpackets/GCWarList.h"
#include "Rpackets/RCPositionInfo.h"
#include <initializer_list>
#include <string>
#include <vector>

// The current zone's flags are gamemodel's, so the replies that read them
// run here. The war list hands each war to the war manager, makes the zone
// unsafe when a race war is fought in it, plays the zone's music again and
// drops the war effect's event. A party member's position, from the server
// or from the member's own client, is stored with whether the member is in
// sight: in the zone the player is in (the large or the small one) and
// within one and a half times the player's sight.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool hasPlayer = true;
int playerHandle = 0;

std::string N(long long value) { return std::to_string(value); }
MCreature* PlayerCreature() { return reinterpret_cast<MCreature*>(&playerHandle); }

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetID = []() {
		CHECK(zoneExists);
		return 71;
	};
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? PlayerCreature() : nullptr; };
	host.GetSight = []() { return 13; };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetX = [](MCreature* p) {
		CHECK(p == PlayerCreature());
		return static_cast<TYPE_SECTORPOSITION>(100);
	};
	host.GetY = [](MCreature* p) {
		CHECK(p == PlayerCreature());
		return static_cast<TYPE_SECTORPOSITION>(90);
	};
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.PlayMusicCurrentZone = []() { calls.push_back("music"); };
	host.RemoveEvent = [](int eventID) { calls.push_back("remove|" + N(eventID)); };
	return host;
}();

struct World : GameModelWorld
{
	CLIENT_MODE oldMode = g_Mode;
	CMessageArray gameLog;
	CMessageArray* oldGame = g_pGameMessage;
	MParty party;
	MParty* oldParty = g_pParty;
	RequestUserManager users;
	RequestUserManager* oldUsers = g_pRequestUserManager;
	MWarManager wars;
	MWarManager* oldWars = g_pWarManager;
	const MWarHost* oldWarHost = MWarManager::SetHost(nullptr);
	bool oldSafe = g_bZoneSafe;
	int oldLarge = g_nZoneLarge;
	int oldSmall = g_nZoneSmall;
	bool oldInLarge = g_bZonePlayerInLarge;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		g_Mode = MODE_GAME;
		gameLog.Init(4, 128);
		g_pGameMessage = &gameLog;
		g_pParty = &party;
		g_pRequestUserManager = &users;
		g_pWarManager = &wars;
		g_bZoneSafe = true;
		g_nZoneLarge = 71;
		g_nZoneSmall = 72;
		g_bZonePlayerInLarge = true;
		calls.clear();
		zoneExists = true;
		hasPlayer = true;
	}
	~World()
	{
		WorldEvents::SetHost(oldEvents);
		CreatureState::SetHost(oldCreature);
		PlayerState::SetHost(oldPlayer);
		ZoneState::SetHost(oldZone);
		g_bZonePlayerInLarge = oldInLarge;
		g_nZoneSmall = oldSmall;
		g_nZoneLarge = oldLarge;
		g_bZoneSafe = oldSafe;
		MWarManager::SetHost(oldWarHost);
		g_pWarManager = oldWars;
		g_pRequestUserManager = oldUsers;
		g_pParty = oldParty;
		g_pGameMessage = oldGame;
		g_Mode = oldMode;
	}
	PARTY_INFO* Member(const char* name, const char* ip)
	{
		PARTY_INFO* info = new PARTY_INFO;
		info->Name = name;
		info->IP = ip;
		info->zoneID = 0;
		info->zoneX = 0;
		info->zoneY = 0;
		info->HP = 1;
		info->MaxHP = 2;
		info->bInSight = false;
		CHECK(party.AddMember(info));
		return info;
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

WarInfo* RaceWar(std::initializer_list<ZoneID_t> castles)
{
	auto* war = new RaceWarInfo;
	war->setStartTime(456);
	war->setRemainTime(120);
	for (ZoneID_t id : castles) war->getCastleIDs().addValue(id);
	return war;
}

WarInfo* GuildWar(ZoneID_t castle)
{
	auto* war = new GuildWarInfo;
	war->setStartTime(456);
	war->setRemainTime(120);
	war->setCastleID(castle);
	war->setAttackGuildName("Moon");
	war->setDefenseGuildName("Sun");
	return war;
}

void ReceiveWars(std::initializer_list<WarInfo*> wars)
{
	GCWarList original;
	for (WarInfo* war : wars) original.addWarInfo(war);
	GCWarList packet;
	Decode(packet, original);
	GCWarListHandler::execute(&packet, nullptr);
}

void ReceivePosition(const char* name, ZoneID_t zoneID, ZoneCoord_t x, ZoneCoord_t y)
{
	GCPartyPosition original;
	original.setName(name);
	original.setZoneID(zoneID);
	original.setXY(x, y);
	original.setHP(300, 250);
	GCPartyPosition packet;
	Decode(packet, original);
	GCPartyPositionHandler::execute(&packet, nullptr);
}

void ReceivePeerPosition(const char* name, const char* host, ZoneID_t zoneID, Coord_t x, Coord_t y)
{
	RCPositionInfo info;
	info.setName(name);
	info.setHost(host);
	info.setPort(9003);
	info.setZoneID(zoneID);
	info.setZoneX(x);
	info.setZoneY(y);
	RCPositionInfoHandler::execute(&info);
}

const std::vector<std::string> kAfterWars = {"music", "remove|" + N(EVENTID_WAR_EFFECT)};

} // namespace

TEST(WarListHandler, ARaceWarInTheCurrentZoneMakesItUnsafe)
{
	World world;
	ReceiveWars({RaceWar({72, 71}), GuildWar(73)});
	CHECK(world.wars.IsExist(71) && world.wars.IsExist(72) && world.wars.IsExist(73));
	CHECK(!g_bZoneSafe);
	CHECK(calls == kAfterWars);
}

TEST(WarListHandler, AnotherWarOrOneElsewhereKeepsTheZoneSafe)
{
	World world;
	ReceiveWars({GuildWar(71), RaceWar({72})});
	CHECK(world.wars.IsExist(71) && world.wars.IsExist(72));
	CHECK(g_bZoneSafe);
	CHECK(calls == kAfterWars);
	// An empty list still plays the music again and drops the effect.
	calls.clear();
	ReceiveWars({});
	CHECK(calls == kAfterWars);
}

TEST(WarListHandler, NothingHappensWithoutAZoneOrAWarManager)
{
	World world;
	zoneExists = false;
	ReceiveWars({RaceWar({71})});
	zoneExists = true;
	g_pWarManager = nullptr;
	ReceiveWars({RaceWar({71})});
	g_pWarManager = &world.wars;
	CHECK_EQ(0, world.wars.getSize());
	CHECK(g_bZoneSafe);
	CHECK(calls.empty());
	// Missing services still store the wars and flag the zone.
	WorldEvents::SetHost(nullptr);
	ReceiveWars({RaceWar({71})});
	CHECK(world.wars.IsExist(71));
	CHECK(!g_bZoneSafe);
	CHECK(calls.empty());
}

TEST(PartyPositionHandler, AMembersPositionIsStoredWithWhetherItIsInSight)
{
	World world;
	PARTY_INFO* nyx = world.Member("Nyx", "10.0.0.1");
	// The player is at (100, 90) in zone 71, seeing 13 cells: 19 away is in sight.
	ReceivePosition("Nyx", 71, 110, 99);
	CHECK_EQ(71, static_cast<int>(nyx->zoneID));
	CHECK_EQ(110, static_cast<int>(nyx->zoneX));
	CHECK_EQ(99, static_cast<int>(nyx->zoneY));
	CHECK_EQ(250, static_cast<int>(nyx->HP));
	CHECK_EQ(300, static_cast<int>(nyx->MaxHP));
	CHECK(nyx->bInSight);
	// 20 away is not.
	ReceivePosition("Nyx", 71, 110, 100);
	CHECK(!nyx->bInSight);
	// The same cell in the small zone is not, until the player is in it.
	ReceivePosition("Nyx", 72, 110, 99);
	CHECK(!nyx->bInSight);
	g_bZonePlayerInLarge = false;
	ReceivePosition("Nyx", 72, 110, 99);
	CHECK(nyx->bInSight);
}

TEST(PartyPositionHandler, NothingChangesForNoNameAStrangerOrOutOfTheGame)
{
	World world;
	PARTY_INFO* nyx = world.Member("Nyx", "10.0.0.1");
	{
		// read() refuses an empty name, so this packet is not decoded.
		GCPartyPosition unnamed;
		unnamed.setZoneID(71);
		unnamed.setXY(110, 99);
		unnamed.setHP(300, 250);
		GCPartyPositionHandler::execute(&unnamed, nullptr);
	}
	ReceivePosition("Vanda", 71, 110, 99);
	g_Mode = MODE_WAIT_SELECTPC;
	ReceivePosition("Nyx", 71, 110, 99);
	g_Mode = MODE_GAME;
	hasPlayer = false;
	ReceivePosition("Nyx", 71, 110, 99);
	hasPlayer = true;
	g_pGameMessage = nullptr;
	ReceivePosition("Nyx", 71, 110, 99);
	g_pGameMessage = &world.gameLog;
	g_pRequestUserManager = nullptr;
	ReceivePosition("Nyx", 71, 110, 99);
	g_pRequestUserManager = &world.users;
	g_pParty = nullptr;
	ReceivePosition("Nyx", 71, 110, 99);
	g_pParty = &world.party;
	CHECK_EQ(0, static_cast<int>(nyx->zoneID));
	CHECK_EQ(1, static_cast<int>(nyx->HP));
	CHECK(!nyx->bInSight);
	// While loading or waiting for the position, it is stored.
	for (CLIENT_MODE mode : {MODE_WAIT_UPDATEINFO, MODE_WAIT_SETPOSITION})
	{
		g_Mode = mode;
		nyx->zoneID = 0;
		ReceivePosition("Nyx", 71, 110, 99);
		CHECK_EQ(71, static_cast<int>(nyx->zoneID));
	}
}

TEST(PeerPositionHandler, APeersAddressAndPositionAreStoredByNameOrAddress)
{
	World world;
	PARTY_INFO* nyx = world.Member("Nyx", "10.0.0.1");
	PARTY_INFO* vanda = world.Member("Vanda", "10.0.0.2");
	world.users.AddRequestUser("Nyx", "10.0.0.1", 9001);
	ReceivePeerPosition("Nyx", "10.0.0.7", 71, 110, 99);
	CHECK(world.users.GetUserInfo("Nyx")->IP == "10.0.0.7");
	CHECK_EQ(9003, static_cast<int>(world.users.GetUserInfo("Nyx")->UDPPort));
	CHECK_EQ(71, static_cast<int>(nyx->zoneID));
	CHECK_EQ(110, static_cast<int>(nyx->zoneX));
	CHECK_EQ(99, static_cast<int>(nyx->zoneY));
	CHECK(nyx->bInSight);
	// With no name the member is found by address.
	ReceivePeerPosition("", "10.0.0.2", 72, 110, 99);
	CHECK_EQ(72, static_cast<int>(vanda->zoneID));
	CHECK(!vanda->bInSight);
	g_bZonePlayerInLarge = false;
	ReceivePeerPosition("", "10.0.0.2", 72, 101, 91);
	CHECK(vanda->bInSight);
}

TEST(PeerPositionHandler, NothingChangesOutOfTheGameOrWithoutAPlayer)
{
	World world;
	PARTY_INFO* nyx = world.Member("Nyx", "10.0.0.1");
	g_Mode = MODE_WAIT_SELECTPC;
	ReceivePeerPosition("Nyx", "10.0.0.7", 71, 110, 99);
	g_Mode = MODE_GAME;
	hasPlayer = false;
	ReceivePeerPosition("Nyx", "10.0.0.7", 71, 110, 99);
	hasPlayer = true;
	ReceivePeerPosition("Vanda", "10.0.0.7", 71, 110, 99);
	CHECK_EQ(0, static_cast<int>(nyx->zoneID));
	CHECK(!nyx->bInSight);
}
