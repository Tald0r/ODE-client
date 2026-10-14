#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "ClientCommunicationManager.h"
#include "ClientPlayer.h"
#include "Datagram.h"
#include "DatagramSocket.h"
#include "Exception.h"
#include "LoginPresentationHost.h"
#include "PacketDispatcher.h"
#include "Properties.h"
#include "ServerInfoFileParser.h"
#include "ServerSocket.h"
#include "Socket.h"
#include "SocketAPI.h"
#include "SocketImpl.h"
#include "SocketOutputStream.h"
#include "UserInformation.h"
#include "WebSocketTransport.h"
#include "WireHost.h"
#include "Cpackets/CGConnect.h"
#include "Cpackets/CGConnectSetKey.h"
#include "Lpackets/LCReconnect.h"
#ifdef _WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// The login server sends the player on to the game server. The connection
// is closed and opened again to the packet's address; a key packet goes out
// (flushed, a half second's wait, handed to the dispatcher), then the
// connect with the packet's key, the PC's type and name and the machine's
// MAC address, flushed. Unless the transport is a web socket, the login
// server's check port is told of the player's character over UDP: the
// Korean config's address and port, or the foreign config's for the
// player's dimension. The player then waits for the game server's first
// update. A game server that cannot be reached throws, and nothing follows.
namespace {

std::vector<std::string> calls;
// The connection a reply runs on, to see what a flush finds queued.
struct GamePlayer;
GamePlayer* current = nullptr;
std::size_t Queued();
int dimension = 0;
bool hasMacAddress = true;
const std::uint8_t kMacAddress[6] = {1, 2, 3, 4, 5, 6};
uint s_UDPPort = 0;

std::string N(long long value) { return std::to_string(value); }

const LoginPresentation::Host login = []() {
	LoginPresentation::Host host{};
	host.FlushSocketOutput = []() { calls.push_back("flushed|" + N(static_cast<long long>(Queued()))); };
	host.Sleep = [](std::uint32_t ms) { calls.push_back("sleep|" + N(ms)); };
	host.EnterWaitUpdateInfoMode = []() { calls.push_back("wait update info"); };
	host.MacAddress = []() -> const std::uint8_t* { return hasMacAddress ? kMacAddress : nullptr; };
	host.Dimension = []() { return dimension; };
	return host;
}();

int HostMaxProcessPacket() { return 11; }
uint HostUDPPort() { return s_UDPPort; }
const WireHost s_Wire = {
	.MaxProcessPacket = HostMaxProcessPacket,
	.MaxRequestService = NULL,
	.ClientCommunicationUDPPort = HostUDPPort,
	.BugReportTarget = NULL,
	.EncryptZoneID = NULL,
	.EncryptServerID = NULL,
	.EncryptUsesEnglishSeed = NULL,
	.CurrentTime = NULL,
	.SendOtherRequest = NULL,
	.HasOtherRequest = NULL,
	.RemoveOtherRequest = NULL,
};

void NoOp(Packet*, Player*) {}
void RegisterKeyHandler()
{
	try {
		PacketDispatcher::registerHandler(CGConnectSetKey().getPacketID(), &NoOp);
	} catch (Error&) {
		// registered already, by another test
	}
}

uint BoundPort(SOCKET s)
{
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
#ifdef _WIN32
	int len = (int)sizeof(addr);
#else
	socklen_t len = (socklen_t)sizeof(addr);
#endif
	if (getsockname(s, (struct sockaddr*)&addr, &len) != 0) return 0;
	return ntohs(addr.sin_port);
}

// The game connection, its output kept unsent for inspection.
struct GamePlayer : ClientPlayer
{
	GamePlayer() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl())))
	{
		setPCType(PC_OUSTERS);
		setPCName("Nosferatu");
	}
	~GamePlayer() { setPlayerStatus(CPS_END_SESSION); }
	std::vector<unsigned char> Sent() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};
std::size_t Queued() { return current != nullptr ? current->Sent().size() : 0; }

// The packets as one stream sends them: each header carries the
// stream's running sequence number.
std::vector<unsigned char> Encoded(const std::vector<const Packet*>& packets)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	for (const Packet* pPacket : packets) encoded.write(pPacket);
	return SocketOutputStreamTestAccess::Bytes(encoded);
}

// What arrives at the probe within two seconds, as a string.
std::string Received(DatagramSocket& probe)
{
	for (int i = 0; i < 200; i++)
	{
		std::unique_ptr<Datagram> pDatagram(probe.receive());
		if (pDatagram) return std::string(pDatagram->getData(), pDatagram->getLength());
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return std::string();
}

struct TempServerInfo
{
	std::filesystem::path path;
	explicit TempServerInfo(const std::string& text)
	{
		path = std::filesystem::temp_directory_path()
			/ ("darkeden_reconnect_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".inf");
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		output << text;
	}
	~TempServerInfo()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
	}
};

struct World : GameModelWorld
{
	std::unique_ptr<ServerSocket> gameServer;
	uint gamePort = 0;
	// The login server's check ports: the Korean config's and dimension 0's
	// and 1's of the foreign one.
	std::unique_ptr<DatagramSocket> korean, foreign0, foreign1;
	std::unique_ptr<TempServerInfo> foreignFile;
	Properties koreanConfig;
	std::unique_ptr<ServerInfoFileParser> foreignConfig;
	std::unique_ptr<ClientCommunicationManager> communication;
	Properties* oldKorean = g_pConfigKorean;
	ServerInfoFileParser* oldForeign = g_pConfigForeign;
	ClientCommunicationManager* oldCommunication = g_pClientCommunicationManager;
	const LoginPresentation::Host* oldLogin = LoginPresentation::SetHost(&login);
	World()
	{
		calls.clear();
		dimension = 0;
		hasMacAddress = true;
		RegisterKeyHandler();
		EnsureSocketsInitialised();
		gameServer.reset(new ServerSocket(0));
		gamePort = BoundPort(gameServer->getSOCKET());
		korean.reset(new DatagramSocket(0));
		foreign0.reset(new DatagramSocket(0));
		foreign1.reset(new DatagramSocket(0));
		koreanConfig.setProperty("LoginServerAddress", "127.0.0.1");
		koreanConfig.setProperty("LoginServerCheckPort", N(BoundPort(korean->getSOCKET())));
		foreignFile.reset(new TempServerInfo(
			"@ zero\nLoginServerAddress:127.0.0.1\nLoginServerCheckPort:" + N(BoundPort(foreign0->getSOCKET())) + "\n@\n"
			"@ one\nLoginServerAddress:127.0.0.1\nLoginServerCheckPort:" + N(BoundPort(foreign1->getSOCKET())) + "\n@\n"));
		foreignConfig.reset(new ServerInfoFileParser(foreignFile->path.string()));
		g_pConfigKorean = &koreanConfig;
		g_pConfigForeign = foreignConfig.get();
		// The client's own UDP socket, on a port no one holds.
		{
			DatagramSocket free(0);
			s_UDPPort = BoundPort(free.getSOCKET());
		}
		Wire::SetHost(&s_Wire);
		communication.reset(new ClientCommunicationManager);
		g_pClientCommunicationManager = communication.get();
		g_pUserInformation->CharacterID = "Nosferatu";
	}
	~World()
	{
		g_pClientCommunicationManager = oldCommunication;
		communication.reset();
		Wire::SetHost(NULL);
		g_pConfigForeign = oldForeign;
		g_pConfigKorean = oldKorean;
		LoginPresentation::SetHost(oldLogin);
	}
};

void Receive(GamePlayer& player, uint port, DWORD key)
{
	LCReconnect packet;
	packet.setGameServerIP("127.0.0.1");
	packet.setGameServerPort(port);
	packet.setKey(key);
	current = &player;
	LCReconnectHandler::execute(&packet, &player);
	current = nullptr;
}

// The key packet and the connect, as the player's stream holds them.
void CheckConnect(const GamePlayer& player, DWORD key, const std::uint8_t* mac)
{
	CGConnectSetKey keyPacket;
	keyPacket.setEncryptKey(0);
	keyPacket.setHashKey(0);
	CGConnect connect;
	connect.setKey(key);
	connect.setPCType(PC_OUSTERS);
	connect.setPCName("Nosferatu");
	connect.setMacAddress(mac);
	const std::vector<unsigned char> keyOnly = Encoded({&keyPacket});
	const std::vector<unsigned char> both = Encoded({&keyPacket, &connect});
	const std::vector<unsigned char> sent = player.Sent();
	// The keys are random; the rest is the same bytes.
	CHECK(sent.size() == both.size() && std::equal(both.begin() + static_cast<long>(keyOnly.size()), both.end(),
		sent.begin() + static_cast<long>(keyOnly.size())));
	CHECK(sent.size() >= 2 && std::equal(keyOnly.begin(), keyOnly.begin() + 2, sent.begin()));
}

} // namespace

TEST(ReconnectGameReplies, TheGameServerIsReachedAndTheLoginServerToldOverUDP)
{
	World world;
	g_pUserInformation->bKorean = true;
	GamePlayer player;
	Receive(player, world.gamePort, 0x0BADF00D);
	// The key packet is flushed alone, then the connect behind it.
	CGConnectSetKey keyPacket;
	keyPacket.setEncryptKey(0);
	keyPacket.setHashKey(0);
	const std::size_t keyBytes = Encoded({&keyPacket}).size();
	const std::size_t allBytes = player.Sent().size();
	CHECK(calls == (std::vector<std::string>{"flushed|" + N(static_cast<long long>(keyBytes)), "sleep|500",
		"flushed|" + N(static_cast<long long>(allBytes)), "wait update info"}));
	CHECK(player.getPlayerStatus() == CPS_AFTER_SENDING_CG_CONNECT);
	CheckConnect(player, 0x0BADF00D, kMacAddress);
#ifndef _WIN32
	// Non-blocking, or the main loop would stall on it (Windows cannot say).
	CHECK(player.getSocket()->isNonBlocking());
#endif
	if (!NetworkTransport::UsesWebSocket())
	{
		// The Korean config's check port hears of the character.
		CHECK(Received(*world.korean).find("Nosferatu") != std::string::npos);
		CHECK(Received(*world.foreign0).empty() && Received(*world.foreign1).empty());
	}
}

TEST(ReconnectGameReplies, AForeignPlayerTellsItsDimensionsCheckPort)
{
	if (NetworkTransport::UsesWebSocket()) return;
	World world;
	g_pUserInformation->bKorean = false;
	dimension = 1;
	GamePlayer player;
	Receive(player, world.gamePort, 7);
	CHECK(Received(*world.foreign1).find("Nosferatu") != std::string::npos);
	CHECK(Received(*world.foreign0).empty() && Received(*world.korean).empty());
	dimension = 0;
	GamePlayer again;
	Receive(again, world.gamePort, 8);
	CHECK(Received(*world.foreign0).find("Nosferatu") != std::string::npos);
	CHECK(Received(*world.foreign1).empty());
}

TEST(ReconnectGameReplies, WithoutAMacAddressTheConnectCarriesZeros)
{
	World world;
	g_pUserInformation->bKorean = true;
	hasMacAddress = false;
	GamePlayer player;
	Receive(player, world.gamePort, 9);
	const std::uint8_t zeros[6] = {};
	CheckConnect(player, 9, zeros);
}

TEST(ReconnectGameReplies, AnUnreachableGameServerThrowsAndNothingFollows)
{
	World world;
	uint port = 0;
	{
		ServerSocket closed(0);
		port = BoundPort(closed.getSOCKET());
	}
	GamePlayer player;
	bool bThrew = false;
	try {
		Receive(player, port, 1);
	} catch (Error&) {
		bThrew = true;
	}
	// A refused connect throws Error, after the old connection closed.
	CHECK(bThrew);
	CHECK(player.getPlayerStatus() == CPS_END_SESSION);
	CHECK(calls.empty());
	CHECK(player.Sent().empty());
	if (!NetworkTransport::UsesWebSocket()) CHECK(Received(*world.korean).empty());
}
