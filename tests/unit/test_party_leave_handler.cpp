#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "CMessageArray.h"
#include "CommunicationPresentationHost.h"
#include "GameMessages.h"
#include "MGameStringTable.h"
#include "MParty.h"
#include "PlayerStateHost.h"
#include "UserInformation.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCPartyLeave.h"
#include <string>
#include <vector>

// A member leaving or being expelled changes the party and says so in the
// game log. When this player leaves, or is expelled, the whole party goes;
// when a leave or this player's own expulsion empties the party, the party
// window closes and the log says the party was disbanded. A leave this
// player asked for is answered. Nothing happens without a zone, a player,
// the user information, the party or the game log.
namespace {

std::vector<std::string> calls;
// The members the party let go of: their creatures leave it in the zone.
std::vector<std::string> leaves;
bool zoneExists = true;
bool hasPlayer = true;
PlayerState::WAIT_VERIFY waitVerify = PlayerState::WAIT_VERIFY_NULL;
int playerHandle = 0;

const MPartyHost partyHost = []() {
	MPartyHost host{};
	host.LeaveByName = [](const char* name) { leaves.push_back(name); };
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? reinterpret_cast<MCreature*>(&playerHandle) : nullptr; };
	host.GetWaitVerify = []() { return waitVerify; };
	host.SetWaitVerifyNULL = []() { calls.push_back("verified"); waitVerify = PlayerState::WAIT_VERIFY_NULL; };
	return host;
}();

const CommunicationPresentation::Host communication = []() {
	CommunicationPresentation::Host host{};
	host.CloseParty = []() { calls.push_back("close"); };
	return host;
}();

struct World : GameModelWorld
{
	MParty party;
	UserInformation user;
	CMessageArray log;
	MParty* oldParty = g_pParty;
	UserInformation* oldUser = g_pUserInformation;
	CMessageArray* oldLog = g_pGameMessage;
	const MPartyHost* oldPartyHost = MParty::SetHost(&partyHost);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const CommunicationPresentation::Host* oldCommunication = CommunicationPresentation::SetHost(&communication);
	World()
	{
		InitGameStringTable();
		log.Init(6, 256);
		user.CharacterID = "Me";
		for (const char* name : {"Vanda", "Nyx"})
		{
			PARTY_INFO* info = new PARTY_INFO;
			info->Name = name;
			CHECK(party.AddMember(info));
		}
		g_pParty = &party;
		g_pUserInformation = &user;
		g_pGameMessage = &log;
		calls.clear();
		leaves.clear();
		zoneExists = true;
		hasPlayer = true;
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
	}
	~World()
	{
		CommunicationPresentation::SetHost(oldCommunication);
		PlayerState::SetHost(oldPlayer);
		ZoneState::SetHost(oldZone);
		MParty::SetHost(oldPartyHost);
		g_pGameMessage = oldLog;
		g_pUserInformation = oldUser;
		g_pParty = oldParty;
	}
	std::vector<std::string> Log()
	{
		std::vector<std::string> lines;
		for (int i = 0; i < log.GetSize(); i++)
			if (log[i] != nullptr && log[i][0] != '\0') lines.push_back(log[i]);
		return lines;
	}
};

void Receive(const std::string& expeller, const std::string& expellee)
{
	GCPartyLeave original;
	original.setExpeller(expeller);
	original.setExpellee(expellee);
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	GCPartyLeave packet;
	packet.read(stream);
	CHECK(stream.isEmpty());
	GCPartyLeaveHandler::execute(&packet, nullptr);
}

} // namespace

TEST(PartyLeaveHandler, MembersLeavingEmptyThePartyThenItIsDisbanded)
{
	World world;
	Receive("", "Vanda");
	CHECK_EQ(1, world.party.GetSize());
	CHECK(calls.empty());
	Receive("", "Nyx");
	CHECK_EQ(0, world.party.GetSize());
	CHECK(world.Log() == std::vector<std::string>({"Vanda left the party.", "Nyx left the party.", "The party was disbanded."}));
	CHECK(calls == std::vector<std::string>{"close"});
	CHECK(leaves == std::vector<std::string>({"Vanda", "Nyx"}));
}

TEST(PartyLeaveHandler, SomeoneNotInThePartyLeavingChangesNothing)
{
	World world;
	Receive("", "Stranger");
	CHECK_EQ(2, world.party.GetSize());
	CHECK(world.Log().empty());
}

TEST(PartyLeaveHandler, LeavingMyselfEndsThePartyAndClosesItsWindow)
{
	World world;
	Receive("", "Me");
	CHECK_EQ(0, world.party.GetSize());
	CHECK(world.Log() == std::vector<std::string>{"You left the party."});
	CHECK(calls == std::vector<std::string>{"close"});
	// Every member's creature leaves the party.
	CHECK(leaves == std::vector<std::string>({"Vanda", "Nyx"}));
}

TEST(PartyLeaveHandler, ExpellingIsWordedForWhoExpelledWhom)
{
	World world;
	// I expel Vanda: the party goes on with Nyx.
	Receive("Me", "Vanda");
	CHECK_EQ(1, world.party.GetSize());
	// Nyx expels someone else: only the log says so, and an emptied party
	// is not disbanded by it.
	Receive("Nyx", "Stranger");
	Receive("Nyx", "Nyx");
	CHECK_EQ(0, world.party.GetSize());
	CHECK(world.Log() == std::vector<std::string>({"Vanda was kicked out of the party.", "Nyx kicked Nyx out of the party."}));
	CHECK(calls.empty());
}

TEST(PartyLeaveHandler, MyExpellingTheLastMemberDisbandsTheParty)
{
	World world;
	Receive("Me", "Vanda");
	Receive("Me", "Nyx");
	CHECK(world.Log() == std::vector<std::string>({"Vanda was kicked out of the party.", "Nyx was kicked out of the party.",
		"The party was disbanded."}));
	CHECK(calls == std::vector<std::string>{"close"});
}

TEST(PartyLeaveHandler, BeingExpelledEndsThePartyAndDisbandsIt)
{
	World world;
	Receive("Vanda", "Me");
	CHECK_EQ(0, world.party.GetSize());
	CHECK(world.Log() == std::vector<std::string>({"You were kicked out by Vanda.", "The party was disbanded."}));
	CHECK(calls == std::vector<std::string>{"close"});
	CHECK(leaves == std::vector<std::string>({"Vanda", "Nyx"}));
}

TEST(PartyLeaveHandler, ALeaveThisPlayerAskedForIsAnswered)
{
	World world;
	waitVerify = PlayerState::WAIT_VERIFY_PARTY_LEAVE;
	Receive("", "Vanda");
	CHECK(calls == std::vector<std::string>{"verified"});
	// Another pending request stays.
	waitVerify = PlayerState::WAIT_VERIFY_TRADE;
	Receive("", "Stranger");
	CHECK_EQ(static_cast<int>(PlayerState::WAIT_VERIFY_TRADE), static_cast<int>(waitVerify));
}

TEST(PartyLeaveHandler, NothingHappensWithoutAZonePlayerUserPartyOrLog)
{
	World world;
	zoneExists = false;
	Receive("", "Vanda");
	zoneExists = true;
	hasPlayer = false;
	Receive("", "Vanda");
	hasPlayer = true;
	g_pUserInformation = nullptr;
	Receive("", "Vanda");
	g_pUserInformation = &world.user;
	g_pGameMessage = nullptr;
	Receive("", "Vanda");
	g_pGameMessage = &world.log;
	g_pParty = nullptr;
	Receive("", "Vanda");
	g_pParty = &world.party;
	CHECK_EQ(2, world.party.GetSize());
	CHECK(world.Log().empty());
	CHECK(calls.empty());
	// Without the communication service the party still goes, unshown.
	CommunicationPresentation::SetHost(nullptr);
	Receive("", "Me");
	CHECK_EQ(0, world.party.GetSize());
	CHECK(calls.empty());
}
