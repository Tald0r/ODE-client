#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "ClientPlayer.h"
#include "Exception.h"
#include "LoginPresentationHost.h"
#include "PacketDispatcher.h"
#include "ServerSocket.h"
#include "Socket.h"
#include "SocketAPI.h"
#include "SocketImpl.h"
#include "SocketOutputStream.h"
#include "UserInformation.h"
#include "Cpackets/CGConnectSetKey.h"
#include "Cpackets/CLReconnectLogin.h"
#include "Gpackets/GCReconnectLogin.h"
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
#include <memory>
#include <string>
#include <thread>
#include <vector>

// The game server sends the player back to the login server. The
// connection is closed and opened again to the packet's address, set
// non-blocking with no linger; a key packet goes out (flushed, a half
// second's wait, and handed to the dispatcher, whose handler is a no-op),
// then the reconnect login with the packet's key and whether the player
// logs in automatically. The player then waits for the character list.
// A login server that cannot be reached throws, and nothing follows.
namespace {

std::vector<std::string> calls;
// The connection a reply runs on, to see what a flush finds queued.
struct GamePlayer;
GamePlayer* current = nullptr;
std::size_t Queued();

std::string N(long long value) { return std::to_string(value); }

const LoginPresentation::Host login = []() {
	LoginPresentation::Host host{};
	host.FlushSocketOutput = []() { calls.push_back("flushed|" + N(static_cast<long long>(Queued()))); };
	host.Sleep = [](std::uint32_t ms) { calls.push_back("sleep|" + N(ms)); };
	host.EnterWaitPCListMode = []() { calls.push_back("wait pc list"); };
	return host;
}();

// The client registers the key packet's handler as a no-op; so does the test.
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
	GamePlayer() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
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

struct World : GameModelWorld
{
	const LoginPresentation::Host* oldLogin = LoginPresentation::SetHost(&login);
	World()
	{
		calls.clear();
		RegisterKeyHandler();
	}
	~World() { LoginPresentation::SetHost(oldLogin); }
};

void Receive(GamePlayer& player, uint port, DWORD key)
{
	GCReconnectLogin packet;
	packet.setLoginServerIP("127.0.0.1");
	packet.setLoginServerPort(port);
	packet.setKey(key);
	struct Running
	{
		explicit Running(GamePlayer* pPlayer) { current = pPlayer; }
		~Running() { current = nullptr; }
	} running(&player);
	GCReconnectLoginHandler::execute(&packet, &player);
}

} // namespace

TEST(ReconnectLoginReplies, TheLoginServerIsReachedAndAskedForTheCharacters)
{
	World world;
	EnsureSocketsInitialised();
	std::unique_ptr<ServerSocket> listener(new ServerSocket(0));
	const uint port = BoundPort(listener->getSOCKET());
	CHECK(port != 0);
	for (bool autoLogin : {true, false})
	{
		calls.clear();
		g_pUserInformation->IsAutoLogIn = autoLogin;
		GamePlayer player;
		Receive(player, port, 0x12345678);
		// The key packet is flushed alone, before the reconnect login.
		CGConnectSetKey keyAlone;
		keyAlone.setEncryptKey(0);
		keyAlone.setHashKey(0);
		CHECK(calls == (std::vector<std::string>{"flushed|" + N(static_cast<long long>(Encoded({&keyAlone}).size())), "sleep|500",
			"wait pc list"}));
#ifndef _WIN32
		// Non-blocking, or the main loop would stall on it (Windows cannot
		// say).
		CHECK(player.getSocket()->isNonBlocking());
#endif
		// No linger time (setLinger(0) is the fresh socket's own default).
		CHECK(player.getSocket()->getLinger() == 0);
		CHECK(player.getPlayerStatus() == CPS_AFTER_SENDING_CL_GET_PC_LIST);
		// The key packet, then the reconnect login with the packet's key.
		CLReconnectLogin expected;
		expected.setKey(0x12345678);
		expected.SetLoginMode(autoLogin);
		CGConnectSetKey key;
		key.setEncryptKey(0);
		key.setHashKey(0);
		const std::vector<unsigned char> keyOnly = Encoded({&key});
		const std::vector<unsigned char> both = Encoded({&key, &expected});
		const std::vector<unsigned char> sent = player.Sent();
		// The keys are random; the rest is the same bytes.
		CHECK(sent.size() == both.size());
		CHECK(sent.size() == both.size() && std::equal(both.begin() + static_cast<long>(keyOnly.size()), both.end(),
			sent.begin() + static_cast<long>(keyOnly.size())));
		CHECK(sent.size() >= 2 && std::equal(keyOnly.begin(), keyOnly.begin() + 2, sent.begin()));
		// The login server saw the connection.
		listener->setNonBlocking(true);
		Socket* pAccepted = NULL;
		for (int i = 0; i < 200 && pAccepted == NULL; i++)
		{
			pAccepted = listener->accept();
			if (pAccepted == NULL) std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		CHECK(pAccepted != NULL);
		delete pAccepted;
	}
}

TEST(ReconnectLoginReplies, AnUnreachableLoginServerThrowsAndNothingFollows)
{
	World world;
	EnsureSocketsInitialised();
	// A port no one listens on: bound, then closed.
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
}
