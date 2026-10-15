//----------------------------------------------------------------------
// test_player_base.cpp
//----------------------------------------------------------------------
//
// The two files task 5.1 moved into packetwire: Player, the base under
// all three player classes - the send/receive plumbing over a socket,
// with no game state of its own - and DatagramSocket. Both became
// linkable into a test binary when the logging header moved into
// basic/ and the wire layer stopped reaching for the executable's
// debug facilities, so this file is first of all the proof that they
// link.
//
// The fixtures cover constructors, IDs and socket accessors, then drive
// processCommand through buffered frames without connecting to a peer.
//----------------------------------------------------------------------

#include "test_framework.h"

#include "packet_stream_access.h"		// EnsureSocketsInitialised
#include "DatagramSocket.h"
#include "Packet.h"
#include "PacketDispatcher.h"
#include "PacketFactory.h"
#include "PacketFactoryManager.h"
#include "Player.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"

#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

const PacketID_t RECEIVE_LOOP_PACKET_ID = Packet::PACKET_MAX - 8;

int g_nReceiveLoopPacketsCreated = 0;
int g_nReceiveLoopPacketsDestroyed = 0;
int g_nReceiveLoopDispatches = 0;
int g_nReceiveLoopThrowMode = 0;

class ReceiveLoopPacket : public Packet
{
public:
	ReceiveLoopPacket()
	{
		g_nReceiveLoopPacketsCreated++;
	}

	~ReceiveLoopPacket()
	{
		g_nReceiveLoopPacketsDestroyed++;
	}

	void read(SocketInputStream& iStream)
	{
		if (g_nReceiveLoopThrowMode == 1)
			throw std::runtime_error("receive loop parser failure");
		if (g_nReceiveLoopThrowMode == 2)
			throw 17;
		if (g_nReceiveLoopThrowMode == 3)
		{
			// Model a legacy nested decoder that consumes the available field,
			// logs/ignores the missing next field, and returns to its caller.
			char byte;
			iStream.read(byte);
			try { iStream.read(byte); }
			catch (Throwable&) {}
			return;
		}

		char body[2];
		iStream.read(body, (uint)sizeof(body));
	}

	void write(SocketOutputStream&) const {}
	PacketID_t getPacketID() const { return RECEIVE_LOOP_PACKET_ID; }
	PacketSize_t getPacketSize() const { return 2; }
	std::string getPacketName() const { return "ReceiveLoopPacket"; }
	std::string toString() const { return getPacketName(); }
};

class ReceiveLoopPacketFactory : public PacketFactory
{
public:
	Packet* createPacket() { return new ReceiveLoopPacket(); }
#ifdef __DEBUG_OUTPUT__
	std::string getPacketName() const { return "ReceiveLoopPacket"; }
#endif
	PacketID_t getPacketID() const noexcept { return RECEIVE_LOOP_PACKET_ID; }
	PacketSize_t getPacketMaxSize() const noexcept { return 2; }
};

class ReceiveLoopPlayer : public Player
{
public:
	ReceiveLoopPlayer()
	: Player(new Socket((EnsureSocketsInitialised(), new SocketImpl())))
	{
	}

	SocketInputStream& inputStream()
	{
		return *m_pInputStream;
	}
};

class FactoryManagerScope
{
public:
	explicit FactoryManagerScope(PacketFactoryManager* pManager)
	: m_pPrevious(g_pPacketFactoryManager)
	{
		g_pPacketFactoryManager = pManager;
	}

	~FactoryManagerScope()
	{
		g_pPacketFactoryManager = m_pPrevious;
	}

private:
	PacketFactoryManager* m_pPrevious;
};

void ReceiveLoopHandler(Packet*, Player*)
{
	g_nReceiveLoopDispatches++;
}

void EnsureReceiveLoopHandlerRegistered()
{
	// The dispatch table rejects duplicate registrations, and another test
	// may clear it: bind the handler whenever the table lacks it. Both
	// fixtures use this same handler, in either test order, and whatever
	// was bound must be it.
	if (PacketDispatcher::handlerFor(RECEIVE_LOOP_PACKET_ID) == nullptr)
		PacketDispatcher::registerHandler(RECEIVE_LOOP_PACKET_ID, &ReceiveLoopHandler);
	CHECK(PacketDispatcher::handlerFor(RECEIVE_LOOP_PACKET_ID) == &ReceiveLoopHandler);
}

void AppendReceiveLoopFrame(std::vector<unsigned char>& wire,
	PacketSize_t declaredSize, const std::vector<unsigned char>& body)
{
	const size_t oldSize = wire.size();
	wire.resize(oldSize + szPacketHeader);
	const SequenceSize_t sequence = 0;
	std::memcpy(&wire[oldSize], &RECEIVE_LOOP_PACKET_ID, szPacketID);
	std::memcpy(&wire[oldSize + szPacketID], &declaredSize, szPacketSize);
	std::memcpy(&wire[oldSize + szPacketID + szPacketSize],
		&sequence, szSequenceSize);
	wire.insert(wire.end(), body.begin(), body.end());
}

} // namespace

//----------------------------------------------------------------------
// The socket-free constructor
//----------------------------------------------------------------------
TEST(PlayerBase, ADefaultPlayerHoldsNoSocket)
{
	Player player;

	// Nothing to send or receive through yet.
	CHECK(player.getSocket() == NULL);

	// The id is the one piece of state that needs no socket.
	CHECK(player.getID().empty());
	player.setID("tester");
	CHECK_EQ(0, std::strcmp("tester", player.getID().c_str()));
}

TEST(PlayerBase, SettingASocketKeepsIt)
{
	Player player;

	// A socket the player is only ever asked to hand back, never to
	// read or write: setSocket replaces the streams only if they are
	// there, and the socket-free constructor made none, so nothing
	// dereferences it.
	Socket*	pFake = (Socket*)0x1;

	player.setSocket(pFake);
	CHECK(pFake == player.getSocket());

	// Put it back before the player is destroyed - the destructor
	// closes and deletes whatever socket it holds.
	player.setSocket(NULL);
	CHECK(player.getSocket() == NULL);

}

TEST(PlayerReceiveLoop, FragmentationAndMalformedBodiesNeverDispatchOrLeak)
{
	PacketFactoryManager manager;
	manager.addFactory(new ReceiveLoopPacketFactory());
	FactoryManagerScope managerScope(&manager);
	EnsureReceiveLoopHandlerRegistered();

	g_nReceiveLoopPacketsCreated = 0;
	g_nReceiveLoopPacketsDestroyed = 0;
	g_nReceiveLoopDispatches = 0;
	g_nReceiveLoopThrowMode = 0;

	std::vector<unsigned char> validWire;
	AppendReceiveLoopFrame(validWire, 2,
		std::vector<unsigned char>{ 0xA1, 0xA2 });

	// A partial header is left byte-for-byte in the ring. No packet exists yet,
	// so there is neither a dispatch nor an allocation to hand off.
	{
		ReceiveLoopPlayer player;
		const uint fragmentSize = szPacketHeader - 1;
		SocketInputStreamTestAccess::Preload(player.inputStream(),
			&validWire[0], fragmentSize, player.inputStream().capacity() - 2);
		player.processCommand();
		CHECK_EQ(fragmentSize, player.inputStream().length());
		CHECK_EQ(0, g_nReceiveLoopPacketsCreated);
		CHECK_EQ(0, g_nReceiveLoopPacketsDestroyed);
		CHECK_EQ(0, g_nReceiveLoopDispatches);
	}

	// A complete header with a partial body has the same transport semantics.
	{
		ReceiveLoopPlayer player;
		const uint fragmentSize = szPacketHeader + 1;
		SocketInputStreamTestAccess::Preload(player.inputStream(),
			&validWire[0], fragmentSize);
		player.processCommand();
		CHECK_EQ(fragmentSize, player.inputStream().length());
		CHECK_EQ(0, g_nReceiveLoopPacketsCreated);
		CHECK_EQ(0, g_nReceiveLoopPacketsDestroyed);
		CHECK_EQ(0, g_nReceiveLoopDispatches);
	}

	// The first complete frame lies about its one-byte body. The factory-owned
	// parser is destroyed during exception unwinding, its handler is not called,
	// and the following valid frame remains available to the receive loop.
	{
		ReceiveLoopPlayer player;
		std::vector<unsigned char> wire;
		AppendReceiveLoopFrame(wire, 1,
			std::vector<unsigned char>{ 0xB1 });
		const size_t secondFrameOffset = wire.size();
		AppendReceiveLoopFrame(wire, 2,
			std::vector<unsigned char>{ 0xC1, 0xC2 });
		SocketInputStreamTestAccess::Preload(player.inputStream(),
			&wire[0], (uint)wire.size());

		bool bProtocolError = false;
		try {
			player.processCommand();
		} catch (InvalidProtocolException&) {
			bProtocolError = true;
		}
		CHECK(bProtocolError);
		CHECK_EQ(1, g_nReceiveLoopPacketsCreated);
		CHECK_EQ(1, g_nReceiveLoopPacketsDestroyed);
		CHECK_EQ(0, g_nReceiveLoopDispatches);
		CHECK_EQ(wire.size() - secondFrameOffset,
			player.inputStream().length());

		player.processCommand();
		CHECK_EQ(2, g_nReceiveLoopPacketsCreated);
		CHECK_EQ(2, g_nReceiveLoopPacketsDestroyed);
		CHECK_EQ(1, g_nReceiveLoopDispatches);
		CHECK(player.inputStream().isEmpty());
	}

	// Ownership also survives exceptions outside the project's Throwable tree.
	// Both the standard-library and primitive cases unwind the receive loop
	// before dispatch and delete the factory-created packet exactly once.
	for (int throwMode = 1; throwMode <= 2; throwMode++)
	{
		ReceiveLoopPlayer player;
		SocketInputStreamTestAccess::Preload(player.inputStream(),
			&validWire[0], (uint)validWire.size());
		g_nReceiveLoopThrowMode = throwMode;
		bool bCaughtExpected = false;
		try {
			player.processCommand();
		} catch (const std::runtime_error&) {
			bCaughtExpected = throwMode == 1;
		} catch (int value) {
			bCaughtExpected = throwMode == 2 && value == 17;
		}
		CHECK(bCaughtExpected);
		CHECK_EQ(2 + throwMode, g_nReceiveLoopPacketsCreated);
		CHECK_EQ(2 + throwMode, g_nReceiveLoopPacketsDestroyed);
		CHECK_EQ(1, g_nReceiveLoopDispatches);
		CHECK(player.inputStream().isEmpty());
	}
	g_nReceiveLoopThrowMode = 0;
}

TEST(PlayerReceiveLoop, SwallowedReadFailureNeverReachesTheHandler)
{
	PacketFactoryManager manager;
	manager.addFactory(new ReceiveLoopPacketFactory());
	FactoryManagerScope managerScope(&manager);
	EnsureReceiveLoopHandlerRegistered();
	g_nReceiveLoopPacketsCreated = 0;
	g_nReceiveLoopPacketsDestroyed = 0;
	g_nReceiveLoopDispatches = 0;
	g_nReceiveLoopThrowMode = 3;
	struct ResetMode { ~ResetMode() { g_nReceiveLoopThrowMode = 0; } } resetMode;

	ReceiveLoopPlayer player;
	std::vector<unsigned char> wire;
	AppendReceiveLoopFrame(wire, 1, { 0xA1 });
	AppendReceiveLoopFrame(wire, 2, { 0xB1, 0xB2 });
	SocketInputStreamTestAccess::Preload(player.inputStream(), wire.data(),
		(uint)wire.size(), player.inputStream().capacity() - 3);
	bool rejected = false;
	try { player.processCommand(); }
	catch (InvalidProtocolException&) { rejected = true; }
	CHECK(rejected);
	CHECK_EQ(1, g_nReceiveLoopPacketsCreated);
	CHECK_EQ(1, g_nReceiveLoopPacketsDestroyed);
	CHECK_EQ(0, g_nReceiveLoopDispatches);
	CHECK_EQ(szPacketHeader + 2, player.inputStream().length());

	g_nReceiveLoopThrowMode = 0;
	player.processCommand();
	CHECK_EQ(2, g_nReceiveLoopPacketsCreated);
	CHECK_EQ(2, g_nReceiveLoopPacketsDestroyed);
	CHECK_EQ(1, g_nReceiveLoopDispatches);
	CHECK(player.inputStream().isEmpty());
}

//----------------------------------------------------------------------
// The other file this slice moved
//----------------------------------------------------------------------
TEST(DatagramSocketLink, ItsObjectIsInTheLibrary)
{
	//------------------------------------------------------------------
	// A static library hands the linker only the objects that resolve
	// something, so a member nothing references is never pulled in and
	// a build says nothing about whether it could have been. Taking
	// the address of each entry point is what forces it: the whole
	// object has to come in, and with it every symbol it references -
	// which is how the wire layer's last seam to the executable,
	// Player::processCommand's SendBugReport, showed up - the slice
	// after this one moved that function into the library too.
	//
	// Nothing here opens a socket. Both constructors do, and the
	// server one binds a port, which is not a thing a unit test should
	// need to be free.
	//------------------------------------------------------------------
	uint (DatagramSocket::*pSend)(Datagram*) = &DatagramSocket::send;
	Datagram* (DatagramSocket::*pReceive)() = &DatagramSocket::receive;
	SOCKET (DatagramSocket::*pGet)() const = &DatagramSocket::getSOCKET;

	CHECK(pSend != NULL);
	CHECK(pReceive != NULL);
	CHECK(pGet != NULL);

	// Its buffer is sized for the largest datagram it can be handed.
	CHECK(sizeof(DatagramSocket) > DATAGRAM_SOCKET_BUFFER_LEN);
}
