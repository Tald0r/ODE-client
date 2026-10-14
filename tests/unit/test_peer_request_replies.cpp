#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "ClientPlayer.h"
#include "CommunicationPresentationHost.h"
#include "RequestFileManager.h"
#include "RequestServerPlayer.h"
#include "RequestUserManager.h"
#include "UserInformation.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Rpackets/CRRequest.h"
#include "Rpackets/RCRequestVerify.h"
#include <string>
#include <vector>

// Another client asks over its own connection for one of the player's
// files. Asked for the player's profile, the connection queues the
// profile's index file and the profile to send, unless that peer has a
// request queued already; with no profile, or asked for someone else's,
// it answers why not and the session ends. Asked for the guild mark, the
// connection switches to sending it. A peer with no name goes by its
// address.
namespace {

const char* profile = "Profile/Lantern.bmp";
bool hasProfiles = true;
std::vector<std::string> asked;

const CommunicationPresentation::Host communication = []() {
	CommunicationPresentation::Host host{};
	host.HasProfiles = []() { return hasProfiles; };
	host.GetProfileFilename = [](const char* name) -> const char* {
		asked.push_back(name);
		return profile;
	};
	return host;
}();

// A connection from another client, closed when the test is done with it.
struct Peer : RequestServerPlayer
{
	// A host given is only recorded: nothing is opened.
	explicit Peer(const std::string& host = "")
		: RequestServerPlayer(new Socket((EnsureSocketsInitialised(), host.empty() ? new SocketImpl() : new SocketImpl(host, 9999)))) {}
	~Peer() { setPlayerStatus(CPS_END_SESSION); }
	std::vector<unsigned char> Sent() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};

// The game server's connection, which reads any packet id it is sent.
struct GameConnection : ClientPlayer
{
	GameConnection() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~GameConnection() { setPlayerStatus(CPS_END_SESSION); }
	std::vector<unsigned char> Sent() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};

struct Files : RequestFileManager
{
	RequestSendInfo* Of(const std::string& name)
	{
		REQUEST_SEND_MAP::iterator i = m_OtherRequests.find(name);
		return i == m_OtherRequests.end() ? nullptr : i->second;
	}
};

struct World : GameModelWorld
{
	UserInformation user;
	Files files;
	RequestUserManager users;
	UserInformation* oldUser = g_pUserInformation;
	RequestFileManager* oldFiles = g_pRequestFileManager;
	RequestUserManager* oldUsers = g_pRequestUserManager;
	const CommunicationPresentation::Host* oldCommunication = CommunicationPresentation::SetHost(&communication);
	World()
	{
		user.CharacterID = "Lantern";
		g_pUserInformation = &user;
		g_pRequestFileManager = &files;
		g_pRequestUserManager = &users;
		profile = "Profile/Lantern.bmp";
		hasProfiles = true;
		asked.clear();
	}
	~World()
	{
		CommunicationPresentation::SetHost(oldCommunication);
		g_pRequestUserManager = oldUsers;
		g_pRequestFileManager = oldFiles;
		g_pUserInformation = oldUser;
	}
};

// Whether the reply ended the connection (the server player's manager
// drops it).
bool Receive(Peer& peer, CR_REQUEST_CODE code, const char* name)
{
	CRRequest packet;
	packet.setCode(code);
	packet.setRequestName(name);
	try
	{
		CRRequestHandler::execute(&packet, &peer);
	}
	catch (DisconnectException&)
	{
		return true;
	}
	return false;
}

std::vector<unsigned char> Verify(REQUEST_VERIFY code)
{
	RCRequestVerify verify;
	verify.setCode(code);
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	encoded.write(&verify);
	return SocketOutputStreamTestAccess::Bytes(encoded);
}

// A request read off the wire with a code byte no request has.
void ReceiveCode(Peer& peer, BYTE code)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	encoded.write(code);
	encoded.write(static_cast<BYTE>(0));
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	CRRequest packet;
	packet.read(stream);
	CHECK(stream.isEmpty());
	CRRequestHandler::execute(&packet, &peer);
}

} // namespace

TEST(PeerRequestReplies, TheProfileIsQueuedWithItsIndex)
{
	World world;
	Peer peer;
	peer.setName("Nyx");
	CHECK(!Receive(peer, CR_REQUEST_FILE_PROFILE, "Lantern"));
	CHECK(asked == std::vector<std::string>{"Lantern"});
	CHECK(peer.getRequestMode() == REQUEST_CLIENT_MODE_PROFILE);
	CHECK(peer.getPlayerStatus() == CPS_REQUEST_SERVER_NORMAL);
	CHECK(peer.Sent().empty());
	RequestSendInfo* pInfo = world.files.Of("Nyx");
	CHECK(pInfo != nullptr);
	if (pInfo == nullptr) return;
	CHECK_EQ(2u, pInfo->GetSize());
	CHECK(pInfo->GetFront()->GetFilename() == "Profile/Lantern.bmpi");
	CHECK(pInfo->GetFront()->GetFileType() == REQUEST_FILE_PROFILE_INDEX);
	pInfo->DeleteFront();
	CHECK(pInfo->GetFront()->GetFilename() == "Profile/Lantern.bmp");
	CHECK(pInfo->GetFront()->GetFileType() == REQUEST_FILE_PROFILE);
}

TEST(PeerRequestReplies, APeerWithARequestQueuedGetsNothingMore)
{
	World world;
	world.files.AddOtherRequest(new RequestSendInfo("Nyx"));
	Peer peer;
	peer.setName("Nyx");
	const PlayerStatus before = peer.getPlayerStatus();
	CHECK(!Receive(peer, CR_REQUEST_FILE_PROFILE, "Lantern"));
	CHECK_EQ(0u, world.files.Of("Nyx")->GetSize());
	CHECK(peer.getPlayerStatus() == before);
	CHECK(peer.Sent().empty());
}

TEST(PeerRequestReplies, NoProfileOrSomeoneElsesIsAnsweredAndEnds)
{
	World world;
	Peer peer;
	peer.setName("Nyx");
	profile = nullptr;
	CHECK(Receive(peer, CR_REQUEST_FILE_PROFILE, "Lantern"));
	CHECK(peer.Sent() == Verify(REQUEST_VERIFY_PROFILE_NOT_EXIST));
	CHECK(peer.getPlayerStatus() == CPS_END_SESSION);
	CHECK(world.files.Of("Nyx") == nullptr);
	Peer other;
	other.setName("Nyx");
	profile = "Profile/Lantern.bmp";
	asked.clear();
	CHECK(Receive(other, CR_REQUEST_FILE_PROFILE, "Ruth"));
	CHECK(other.Sent() == Verify(REQUEST_VERIFY_PROFILE_WRONG_USER));
	CHECK(other.getPlayerStatus() == CPS_END_SESSION);
	CHECK(asked.empty());
	CHECK(world.files.Of("Nyx") == nullptr);
}

TEST(PeerRequestReplies, TheGuildMarkSwitchesTheConnectionToSendIt)
{
	World world;
	Peer peer;
	peer.setName("Nyx");
	CHECK(!Receive(peer, CR_REQUEST_FILE_GUILD_MARK, ""));
	CHECK(peer.getRequestMode() == REQUEST_CLIENT_MODE_GUILDMARK);
	CHECK(peer.getPlayerStatus() == CPS_REQUEST_SERVER_NORMAL);
	CHECK(peer.Sent().empty());
	// No request at all: nothing.
	Peer idle;
	idle.setName("Nyx");
	const PlayerStatus before = idle.getPlayerStatus();
	const REQUEST_CLIENT_MODE mode = idle.getRequestMode();
	CHECK(!Receive(idle, CR_REQUEST_NULL, "Lantern"));
	CHECK(idle.getPlayerStatus() == before && idle.getRequestMode() == mode);
	CHECK(asked.empty());
}

TEST(PeerRequestReplies, APeerWithNoNameGoesByItsAddress)
{
	World world;
	Peer peer("10.0.0.7");
	CHECK(!Receive(peer, CR_REQUEST_FILE_PROFILE, "Lantern"));
	CHECK(peer.getName() == "10.0.0.7");
	CHECK(world.files.Of("10.0.0.7") != nullptr);
	// A named peer keeps its name.
	Peer named("10.0.0.8");
	named.setName("Nyx");
	CHECK(!Receive(named, CR_REQUEST_FILE_PROFILE, "Lantern"));
	CHECK(named.getName() == "Nyx");
	CHECK(world.files.Of("Nyx") != nullptr);
}

TEST(PeerRequestReplies, WithoutTheGameStateNothingIsDone)
{
	World world;
	Peer peer;
	peer.setName("Nyx");
	const PlayerStatus before = peer.getPlayerStatus();
	hasProfiles = false;
	CHECK(!Receive(peer, CR_REQUEST_FILE_GUILD_MARK, ""));
	hasProfiles = true;
	g_pUserInformation = nullptr;
	CHECK(!Receive(peer, CR_REQUEST_FILE_GUILD_MARK, ""));
	g_pUserInformation = &world.user;
	g_pRequestFileManager = nullptr;
	CHECK(!Receive(peer, CR_REQUEST_FILE_GUILD_MARK, ""));
	g_pRequestFileManager = &world.files;
	g_pRequestUserManager = nullptr;
	CHECK(!Receive(peer, CR_REQUEST_FILE_GUILD_MARK, ""));
	g_pRequestUserManager = &world.users;
	CHECK(peer.getPlayerStatus() == before);
	// Without the host there are no profiles.
	CommunicationPresentation::SetHost(nullptr);
	CHECK(!CommunicationPresentation::HasProfiles());
	CHECK(CommunicationPresentation::GetProfileFilename("Lantern") == nullptr);
	CHECK(!Receive(peer, CR_REQUEST_FILE_GUILD_MARK, ""));
	CHECK(peer.getPlayerStatus() == before);
}

TEST(PeerRequestReplies, ACodeNoRequestHasDoesNothing)
{
	World world;
	Peer peer;
	peer.setName("Nyx");
	const PlayerStatus before = peer.getPlayerStatus();
	const REQUEST_CLIENT_MODE mode = peer.getRequestMode();
	// The code is one byte on the wire; three codes are known.
	ReceiveCode(peer, 7);
	ReceiveCode(peer, 255);
	CHECK(peer.getPlayerStatus() == before && peer.getRequestMode() == mode);
	CHECK(peer.Sent().empty());
	CHECK(asked.empty());
}

TEST(PeerRequestReplies, TheGameServerSendingARequestIsIgnored)
{
	World world;
	GameConnection server;
	const PlayerStatus before = server.getPlayerStatus();
	CRRequest packet;
	packet.setCode(CR_REQUEST_FILE_PROFILE);
	packet.setRequestName("Lantern");
	CRRequestHandler::execute(&packet, &server);
	CHECK(server.getPlayerStatus() == before);
	CHECK(server.Sent().empty());
	CHECK(asked.empty());
}

TEST(PeerRequestReplies, BeforeACharacterIsChosenNoProfileIsTheirs)
{
	World world;
	world.user.CharacterID = "";
	Peer peer;
	peer.setName("Nyx");
	CHECK(Receive(peer, CR_REQUEST_FILE_PROFILE, ""));
	CHECK(peer.Sent() == Verify(REQUEST_VERIFY_PROFILE_WRONG_USER));
	CHECK(peer.getPlayerStatus() == CPS_END_SESSION);
	CHECK(asked.empty());
	CHECK(world.files.Of("Nyx") == nullptr);
}
