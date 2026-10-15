//----------------------------------------------------------------------
// test_peer_self_disconnect.cpp
//----------------------------------------------------------------------
// A peer connected to this client (RequestServerPlayerManager) may send
// CRDisconnect, which asks the manager to drop the peer of the sender's
// own name. The request arrives while Update() is dispatching that very
// peer's packets, so the manager must not free the peer there: it drops
// it once the dispatch has returned. Real loopback connections carry the
// packet through the real factory, validator and handler.
//----------------------------------------------------------------------
#include "test_framework.h"
#include "packet_stream_access.h"
#include "WorldEventsHost.h"
#include "PacketDispatcher.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "RequestServerPlayer.h"
#include "RequestServerPlayerManager.h"
#include "RequestFunction.h"
#include "ServerSocket.h"
#include "Socket.h"
#include "SocketAPI.h"
#include "SocketOutputStream.h"
#include "Rpackets/CRDisconnect.h"

#ifdef _WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <string.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

uint BoundPort(SOCKET s)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
#ifdef _WIN32
	int len = (int)sizeof(addr);
#else
	socklen_t len = (socklen_t)sizeof(addr);
#endif
	if (getsockname(s, (struct sockaddr*)&addr, &len) != 0)
		return 0;
	return ntohs(addr.sin_port);
}

// The manager with its protected registration open to the test.
struct Manager : RequestServerPlayerManager
{
	using RequestServerPlayerManager::AddRequestServerPlayer;
	RequestServerPlayer* Find(const std::string& name)
	{
		for (RequestServerPlayer* pPlayer : m_listRequestServerPlayer)
			if (pPlayer->getName() == name) return pPlayer;
		return nullptr;
	}
};

std::vector<std::string> asked;

// Output queued on a peer whose socket is then closed: its flush is refused.
void Break(RequestServerPlayer* pPeer)
{
	CRDisconnect queued;
	pPeer->sendPacket(&queued);
	pPeer->getSocket()->close();
}

const WorldEvents::Host events{
	.RequestDisconnect = [](const char* pName) {
		asked.push_back(pName);
		// The executable's adapter is RequestDisconnect itself.
		::RequestDisconnect(pName);
	},
};

// The game binds handlers at start-up (PacketHandlerRegistry.cpp); a test
// that runs the registry clears the table around itself. This binds the
// real CRDisconnect handler whenever the table lacks it, since the table
// refuses a second registration. Whatever was bound must be this one.
static void DisconnectHandler(Packet* pPacket, Player* pPlayer)
{
	CRDisconnectHandler::execute(static_cast<CRDisconnect*>(pPacket), pPlayer);
}

void BindDisconnectHandler()
{
	if (PacketDispatcher::handlerFor(Packet::PACKET_CR_DISCONNECT) == nullptr)
		PacketDispatcher::registerHandler(Packet::PACKET_CR_DISCONNECT, &DisconnectHandler);
	CHECK(PacketDispatcher::handlerFor(Packet::PACKET_CR_DISCONNECT) == &DisconnectHandler);
}

struct World
{
	// Sockets are started before the listener below is built (Windows).
	bool sockets = (EnsureSocketsInitialised(), true);
	PacketFactoryManager factories;
	PacketValidator validator;
	PacketFactoryManager* oldFactories = g_pPacketFactoryManager;
	PacketValidator* oldValidator = g_pPacketValidator;
	RequestServerPlayerManager* oldManager = g_pRequestServerPlayerManager;
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	Manager manager;
	ServerSocket listener{0};
	std::vector<std::unique_ptr<Socket>> clients;
	World()
	{
		BindDisconnectHandler();
		factories.init();
		validator.init();
		g_pPacketFactoryManager = &factories;
		g_pPacketValidator = &validator;
		g_pRequestServerPlayerManager = &manager;
		listener.setNonBlocking(true);
		asked.clear();
	}
	~World()
	{
		// Whatever peers remain end their sessions before the manager frees them.
		for (const char* name : {"Nyx", "Vanda"})
			if (RequestServerPlayer* pPlayer = manager.Find(name))
				pPlayer->setPlayerStatus(CPS_END_SESSION);
		WorldEvents::SetHost(oldEvents);
		g_pRequestServerPlayerManager = oldManager;
		g_pPacketValidator = oldValidator;
		g_pPacketFactoryManager = oldFactories;
	}

	// A peer connected over loopback and registered under its name; the
	// returned socket is the peer's own end.
	Socket* Connect(const char* name)
	{
		clients.emplace_back(new Socket("127.0.0.1", BoundPort(listener.getSOCKET())));
		Socket* client = clients.back().get();
		client->connect();
		Socket* accepted = NULL;
		for (int i = 0; i < 200 && accepted == NULL; i++)
		{
			accepted = listener.accept();
			if (accepted == NULL)
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		CHECK(accepted != NULL);
		if (accepted == NULL) return client;
		accepted->setNonBlocking(true);
		RequestServerPlayer* peer = new RequestServerPlayer(accepted);
		peer->setName(name);
		peer->setPlayerStatus(CPS_REQUEST_SERVER_NORMAL);
		CHECK(manager.AddRequestServerPlayer(peer));
		return client;
	}
};

void Send(Socket* socket, const Packet& packet)
{
	SocketOutputStream out(socket);
	out.write(&packet);
	out.flush();
}

} // namespace

TEST(PeerSelfDisconnect, APeerAskingToDisconnectItselfIsDroppedAfterItsDispatch)
{
	World world;
	Socket* nyx = world.Connect("Nyx");
	world.Connect("Vanda");
	CHECK_EQ(2, world.manager.GetSize());

	CRDisconnect disconnect;
	Send(nyx, disconnect);
	for (int i = 0; i < 200 && world.manager.GetSize() == 2; i++)
	{
		world.manager.Update();
		if (world.manager.GetSize() == 2)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	// The handler asked by the sender's name; only that peer is gone.
	CHECK(asked == std::vector<std::string>{"Nyx"});
	CHECK_EQ(1, world.manager.GetSize());
	CHECK(world.manager.Find("Nyx") == nullptr);
	CHECK(world.manager.Find("Vanda") != nullptr);

	// Later updates go on with the remaining peer.
	world.manager.Update();
	CHECK_EQ(1, world.manager.GetSize());
}

TEST(PeerSelfDisconnect, RequestDisconnectDropsTheNamedPeerOfTheInstalledManager)
{
	World world;
	world.Connect("Nyx");
	world.Connect("Vanda");
	::RequestDisconnect("Vanda");
	CHECK(world.manager.Find("Vanda") == nullptr);
	CHECK(world.manager.Find("Nyx") != nullptr);
	// Without a manager there is nobody to drop.
	g_pRequestServerPlayerManager = NULL;
	::RequestDisconnect("Nyx");
	CHECK(world.manager.Find("Nyx") != nullptr);
}

TEST(PeerSelfDisconnect, ADisconnectOutsideAnUpdateDropsThePeerAtOnce)
{
	World world;
	world.Connect("Nyx");
	world.Connect("Vanda");
	world.manager.Disconnect("Vanda");
	CHECK_EQ(1, world.manager.GetSize());
	CHECK(world.manager.Find("Vanda") == nullptr);
	world.manager.Disconnect("Nobody");
	CHECK_EQ(1, world.manager.GetSize());
}

TEST(PeerSelfDisconnect, APeerWhoseConnectionBrokeWithOutputQueuedIsDroppedAndTheRestServed)
{
	World world;
	world.Connect("Vanda");
	Socket* nyx = world.Connect("Nyx");
	Break(world.manager.Find("Vanda"));

	// Vanda heads the list. Dropping her must not abort the loop, or Nyx,
	// behind her, is never served again.
	CRDisconnect disconnect;
	Send(nyx, disconnect);
	for (int i = 0; i < 200 && world.manager.GetSize() != 0; i++)
	{
		world.manager.Update();
		if (world.manager.GetSize() != 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	CHECK(world.manager.Find("Vanda") == nullptr);
	CHECK(asked == std::vector<std::string>{"Nyx"});
	CHECK_EQ(0, world.manager.GetSize());
}

TEST(PeerSelfDisconnect, ADroppedPeerWhoseFlushIsRefusedIsStillFreed)
{
	World world;
	world.Connect("Nyx");
	world.Connect("Vanda");
	Break(world.manager.Find("Vanda"));
	world.manager.Disconnect("Vanda");
	CHECK(world.manager.Find("Vanda") == nullptr);
	CHECK(world.manager.Find("Nyx") != nullptr);
}
