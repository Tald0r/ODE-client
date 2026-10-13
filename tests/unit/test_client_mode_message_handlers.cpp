#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "ClientMode.h"
#include "CMessageArray.h"
#include "CommunicationPresentationHost.h"
#include "GameMessages.h"
#include "MChatManager.h"
#include "MParty.h"
#include "MSkillManager.h"
#include "PlayerStateHost.h"
#include "RequestServerPlayer.h"
#include "RequestUserManager.h"
#include "SkillDef.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCLearnSkillReady.h"
#include "Gpackets/GCPartySay.h"
#include "Rpackets/CRConnect.h"
#include "Rpackets/RCCharacterInfo.h"
#include "Rpackets/RCConnectVerify.h"
#include "Rpackets/RCSay.h"
#include "Rpackets/RCStatusHP.h"
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

// The client mode and the message logs are gamemodel's, so the replies that
// read them run here: a skill domain ready to learn, a peer asking to
// connect, a peer's address and party status, and party and peer chat.
namespace {

const char* const kCurseFile = "client_mode_chat_words.txt";
std::vector<std::string> calls;
bool ghost = false;

std::string N(long long value) { return std::to_string(value); }
std::string Newest(CMessageArray& log) { return log[log.GetSize() - 1]; }

CommunicationPresentation::Host ChatHost()
{
	CommunicationPresentation::Host host{};
	host.AddChatToHistory = [](const char* message, const char* sender, int chatType, std::uint32_t color) {
		calls.push_back(std::string("chat|") + message + "|" + sender + "|" + N(chatType) + "|" + N(color));
	};
	return host;
}
const CommunicationPresentation::Host chat = ChatHost();

PlayerState::Host PlayerHost()
{
	PlayerState::Host host{};
	host.HasEffectStatus = [](EFFECTSTATUS status) { return status == EFFECTSTATUS_GHOST && ghost; };
	return host;
}
const PlayerState::Host player = PlayerHost();

struct World : GameModelWorld
{
	CLIENT_MODE oldMode = g_Mode;
	CLIENT_MODE oldNext = g_ModeNext;
	CMessageArray gameLog;
	CMessageArray* oldGame = g_pGameMessage;
	MSkillManager skills;
	MSkillManager* oldSkills = g_pSkillManager;
	MParty party;
	MParty* oldParty = g_pParty;
	RequestUserManager users;
	RequestUserManager* oldUsers = g_pRequestUserManager;
	MChatManager filter;
	MChatManager* oldFilter = g_pChatManager;
	const MChatHost* oldFilterHost = MChatManager::SetHost(nullptr);
	const CommunicationPresentation::Host* oldChat = CommunicationPresentation::SetHost(&chat);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	World()
	{
		InitGameStringTable();
		g_Mode = MODE_GAME;
		gameLog.Init(4, 128);
		g_pGameMessage = &gameLog;
		skills.CTypeTable<MSkillDomain>::Init(MAX_SKILLDOMAIN);
		g_pSkillManager = &skills;
		g_pParty = &party;
		g_pRequestUserManager = &users;
		{
			std::ofstream words(kCurseFile, std::ios::binary | std::ios::trunc);
			words << "darn\n";
		}
		filter.LoadFromFileCurse(kCurseFile);
		filter.SetAcceptMode();
		g_pChatManager = &filter;
		g_pUserInformation->CharacterID = "Lantern";
		g_pUserInformation->IsMaster = FALSE;
		calls.clear();
		ghost = false;
	}
	~World()
	{
		PlayerState::SetHost(oldPlayer);
		CommunicationPresentation::SetHost(oldChat);
		MChatManager::SetHost(oldFilterHost);
		g_pChatManager = oldFilter;
		g_pRequestUserManager = oldUsers;
		g_pParty = oldParty;
		g_pSkillManager = oldSkills;
		g_pGameMessage = oldGame;
		g_ModeNext = oldNext;
		g_Mode = oldMode;
		std::remove(kCurseFile);
	}
	PARTY_INFO* Member(const char* name, const char* ip)
	{
		PARTY_INFO* info = new PARTY_INFO;
		info->Name = name;
		info->IP = ip;
		info->HP = 1;
		info->MaxHP = 2;
		info->guildID = 3;
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

std::vector<unsigned char> Encoded(const Packet& packet)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	encoded.write(&packet);
	return SocketOutputStreamTestAccess::Bytes(encoded);
}

std::vector<unsigned char> Verify(RC_CONNECT_VERIFY_CODE code)
{
	RCConnectVerify verify;
	verify.setCode(code);
	return Encoded(verify);
}

// A connection from another client, closed when the test is done with it.
struct Peer : RequestServerPlayer
{
	Peer() : RequestServerPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~Peer() { setPlayerStatus(CPS_END_SESSION); }
	std::vector<unsigned char> Sent() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};

// Whether a reply threw DisconnectException.
template<class Body>
bool Disconnects(Body body)
{
	try { body(); }
	catch (DisconnectException&) { return true; }
	return false;
}

void ReceiveConnect(Player* pPlayer, const char* serverName, const char* clientName)
{
	CRConnect original;
	original.setRequestServerName(serverName);
	original.setRequestClientName(clientName);
	CRConnect packet;
	Decode(packet, original);
	CRConnectHandler::execute(&packet, pPlayer);
}

void ReceiveLearnSkillReady(SkillDomainType_t domain)
{
	GCLearnSkillReady original;
	original.setSkillDomainType(domain);
	GCLearnSkillReady packet;
	Decode(packet, original);
	GCLearnSkillReadyHandler::execute(&packet, nullptr);
}

void ReceivePartySay(const char* name, const char* message)
{
	GCPartySay original;
	original.setName(name);
	original.setMessage(message);
	original.setColor(0x00FF00);
	GCPartySay packet;
	Decode(packet, original);
	GCPartySayHandler::execute(&packet, nullptr);
}

void ReceivePeerSay(const char* name, const char* message)
{
	RCSay packet;
	packet.setName(name);
	packet.setMessage(message);
	packet.setColor(0x0000FF);
	RCSayHandler::execute(&packet);
}

} // namespace

TEST(ClientModeMessageHandlers, AReadySkillDomainIsMarkedAndAnnounced)
{
	World world;
	ReceiveLearnSkillReady(SKILLDOMAIN_SWORD);
	CHECK(world.skills.GetMutable(SKILLDOMAIN_SWORD)->HasNewSkill());
	CHECK(!world.skills.GetMutable(SKILLDOMAIN_BLADE)->HasNewSkill());
	CHECK(Newest(world.gameLog) == std::string("You can learn a new ")
		+ (*g_pGameStringTable)[SKILLDOMAIN_NAME[SKILLDOMAIN_SWORD]].GetString() + " skill.");

	// A domain past the table is refused before anything is touched.
	world.gameLog.Clear();
	ReceiveLearnSkillReady(MAX_SKILLDOMAIN);
	CHECK(Newest(world.gameLog).empty());
}

TEST(ClientModeMessageHandlers, APeerIsAcceptedOnlyInGameAndOnlyWhenItAsksForThisCharacter)
{
	World world;
	Peer accepted;
	ReceiveConnect(&accepted, "Lantern", "Nyx");
	CHECK(accepted.getName() == "Nyx");
	CHECK(accepted.getPlayerStatus() == CPS_REQUEST_SERVER_NORMAL);
	CHECK(accepted.Sent() == Verify(RC_CONNECT_OK));

	// Asking for another character, or outside the game, is refused and
	// ends the session.
	Peer wrongName;
	CHECK(Disconnects([&] { ReceiveConnect(&wrongName, "Someone", "Nyx"); }));
	CHECK(wrongName.Sent() == Verify(RC_CONNECT_REJECT));
	CHECK(wrongName.getPlayerStatus() == CPS_END_SESSION);
	CHECK(wrongName.getName().empty());
	const CLIENT_MODE notInGame[] = {MODE_MAINMENU, MODE_WAIT_UPDATEINFO, MODE_WAIT_SETPOSITION};
	for (CLIENT_MODE mode : notInGame)
	{
		g_Mode = mode;
		Peer early;
		CHECK(Disconnects([&] { ReceiveConnect(&early, "Lantern", "Nyx"); }));
		CHECK(early.Sent() == Verify(RC_CONNECT_REJECT));
		CHECK(early.getName().empty());
	}

	// Without a peer connection or a message log nothing is answered.
	g_Mode = MODE_GAME;
	CHECK(Disconnects([&] { ReceiveConnect(nullptr, "Lantern", "Nyx"); }));
	g_pGameMessage = nullptr;
	Peer unlogged;
	CHECK(Disconnects([&] { ReceiveConnect(&unlogged, "Lantern", "Nyx"); }));
	CHECK(unlogged.Sent().empty());
}

TEST(ClientModeMessageHandlers, APeersAddressAndGuildAreRecordedWhileInOrEnteringTheGame)
{
	World world;
	world.users.AddRequestUser("Nyx", "10.0.0.1");
	PARTY_INFO* nyx = world.Member("Nyx", "10.0.0.1");
	PARTY_INFO* vanda = world.Member("Vanda", "10.0.0.2");
	const CLIENT_MODE inGame[] = {MODE_GAME, MODE_WAIT_UPDATEINFO, MODE_WAIT_SETPOSITION};
	GuildID_t guild = 100;
	for (CLIENT_MODE mode : inGame)
	{
		g_Mode = mode;
		RCCharacterInfo info;
		info.setName("Nyx");
		info.setHost("10.0.0.9");
		info.setPort(9001);
		info.setGuildID(++guild);
		RCCharacterInfoHandler::execute(&info);
		CHECK(world.users.GetUserInfo("Nyx")->IP == "10.0.0.9");
		CHECK_EQ(9001, world.users.GetUserInfo("Nyx")->UDPPort);
		CHECK_EQ(static_cast<int>(guild), static_cast<int>(nyx->guildID));
	}

	// With no name the member is found by address.
	RCCharacterInfo byAddress;
	byAddress.setName("");
	byAddress.setHost("10.0.0.2");
	byAddress.setPort(9002);
	byAddress.setGuildID(7);
	RCCharacterInfoHandler::execute(&byAddress);
	CHECK_EQ(7, static_cast<int>(vanda->guildID));

	// At character selection, or without a party, nothing is recorded.
	g_Mode = MODE_WAIT_SELECTPC;
	RCCharacterInfo early;
	early.setName("Nyx");
	early.setHost("10.0.0.5");
	early.setPort(1);
	early.setGuildID(55);
	RCCharacterInfoHandler::execute(&early);
	CHECK(world.users.GetUserInfo("Nyx")->IP == "10.0.0.9");
	CHECK_EQ(static_cast<int>(guild), static_cast<int>(nyx->guildID));
	g_Mode = MODE_GAME;
	g_pParty = nullptr;
	RCCharacterInfoHandler::execute(&early);
	CHECK(world.users.GetUserInfo("Nyx")->IP == "10.0.0.9");
	g_pParty = &world.party;
	g_pGameMessage = nullptr;
	RCCharacterInfoHandler::execute(&early);
	CHECK(world.users.GetUserInfo("Nyx")->IP == "10.0.0.9");
	CHECK_EQ(static_cast<int>(guild), static_cast<int>(nyx->guildID));
	g_pGameMessage = &world.gameLog;
	g_pRequestUserManager = nullptr;
	RCCharacterInfoHandler::execute(&early);
	CHECK_EQ(static_cast<int>(guild), static_cast<int>(nyx->guildID));
}

TEST(ClientModeMessageHandlers, APeersHitPointsAreRecordedOnlyWhileItIsOutOfSight)
{
	World world;
	world.users.AddRequestUser("Nyx", "10.0.0.1");
	PARTY_INFO* nyx = world.Member("Nyx", "10.0.0.1");
	PARTY_INFO* vanda = world.Member("Vanda", "10.0.0.2");
	auto Status = [](const char* name, const char* host, HP_t hp, HP_t maxHP) {
		RCStatusHP status;
		status.setName(name);
		status.setHost(host);
		status.setPort(9003);
		status.setCurrentHP(hp);
		status.setMaxHP(maxHP);
		RCStatusHPHandler::execute(&status);
	};
	const CLIENT_MODE inGame[] = {MODE_GAME, MODE_WAIT_UPDATEINFO, MODE_WAIT_SETPOSITION};
	HP_t hp = 40;
	for (CLIENT_MODE mode : inGame)
	{
		g_Mode = mode;
		Status("Nyx", "10.0.0.7", ++hp, 90);
		CHECK_EQ(static_cast<int>(hp), static_cast<int>(nyx->HP));
		CHECK_EQ(90, static_cast<int>(nyx->MaxHP));
		CHECK(world.users.GetUserInfo("Nyx")->IP == "10.0.0.7");
	}

	// With no name the member is found by address.
	Status("", "10.0.0.2", 12, 34);
	CHECK_EQ(12, static_cast<int>(vanda->HP));
	CHECK_EQ(34, static_cast<int>(vanda->MaxHP));

	// In sight, the creature's own status is the truth: the record stays.
	nyx->bInSight = true;
	Status("Nyx", "10.0.0.7", 10, 90);
	CHECK_EQ(static_cast<int>(hp), static_cast<int>(nyx->HP));

	// Outside the game, or without the log or the peer records, nothing is
	// recorded, the address included.
	nyx->bInSight = false;
	g_Mode = MODE_LOGIN_WRONG;
	Status("Nyx", "10.0.0.8", 5, 90);
	g_Mode = MODE_GAME;
	g_pGameMessage = nullptr;
	Status("Nyx", "10.0.0.8", 5, 90);
	g_pGameMessage = &world.gameLog;
	g_pRequestUserManager = nullptr;
	Status("Nyx", "10.0.0.8", 5, 90);
	CHECK_EQ(static_cast<int>(hp), static_cast<int>(nyx->HP));
	CHECK(world.users.GetUserInfo("Nyx")->IP == "10.0.0.7");
}

// Both chat replies take the same rules, so each case runs through both.
using Say = void (*)(const char* name, const char* message);
const struct { const char* name; Say say; std::uint32_t color; } kSays[] = {
	{"party", ReceivePartySay, 0x00FF00}, {"peer", ReceivePeerSay, 0x0000FF},
};

TEST(ClientModeMessageHandlers, PartyAndPeerChatIsFilteredAndShownOnlyInGameFromAcceptedNames)
{
	for (const auto& say : kSays)
	{
		World world;
		const std::string color = N(say.color);
		const CLIENT_MODE inGame[] = {MODE_GAME, MODE_WAIT_UPDATEINFO, MODE_WAIT_SETPOSITION};
		for (CLIENT_MODE mode : inGame)
		{
			g_Mode = mode;
			say.say("Nyx", "darn it");
		}
		CHECK(calls == std::vector<std::string>(3, "chat|xxxx it|Nyx|3|" + color));

		// A ghost, a game master or the master's own words are not filtered.
		calls.clear();
		ghost = true;
		say.say("Nyx", "darn");
		ghost = false;
		g_pUserInformation->IsMaster = TRUE;
		say.say("Nyx", "darn");
		g_pUserInformation->IsMaster = FALSE;
		say.say("GM Rook", "darn");
		CHECK(calls == std::vector<std::string>({"chat|darn|Nyx|3|" + color, "chat|darn|Nyx|3|" + color,
			"chat|darn|GM Rook|3|" + color}));

		// A refused name is dropped unless it is the master's; outside the
		// game, or without a chat filter, nothing is shown.
		calls.clear();
		world.filter.AddID("Nyx");
		world.filter.AddID("GM Rook");
		say.say("Nyx", "hello");
		say.say("GM Rook", "hello");
		world.filter.RemoveID("Nyx");
		g_Mode = MODE_MAINMENU;
		say.say("Nyx", "hello");
		g_Mode = MODE_WAIT_SETPOSITION;
		g_pChatManager = nullptr;
		say.say("Nyx", "hello");
		CHECK(calls == std::vector<std::string>{"chat|hello|GM Rook|3|" + color});
	}
}

TEST(ClientModeMessageHandlers, MissingHostsAndEntriesSkipOnlyTheServices)
{
	World world;
	CommunicationPresentation::SetHost(nullptr);
	PlayerState::SetHost(nullptr);
	ReceivePartySay("Nyx", "darn");
	ReceivePeerSay("Nyx", "darn");
	CHECK(calls.empty());
	// Without the player service nobody is a ghost.
	CHECK(!PlayerState::HasEffectStatus(EFFECTSTATUS_GHOST));
	const PlayerState::Host empty{};
	PlayerState::SetHost(&empty);
	CHECK(!PlayerState::HasEffectStatus(EFFECTSTATUS_GHOST));
	CommunicationPresentation::SetHost(&chat);
	ReceivePartySay("Nyx", "darn");
	CHECK(calls == std::vector<std::string>{"chat|xxxx|Nyx|3|" + N(0x00FF00)});
}
