//----------------------------------------------------------------------
// test_packet_dispatcher.cpp
//----------------------------------------------------------------------
//
// PacketDispatcher (docs/reviews/RESTRUCTURING.md task 2.1): the id -> handler
// table that replaced the per-packet virtual execute(). These tests pin
// its contract:
//
//   - a registered handler runs with the packet and player it was given;
//   - dispatch throws InvalidProtocolException for an unregistered or
//     out-of-range id - a protocol violation, not a no-op (the receive
//     loops call dispatch unconditionally since task 2.4; the
//     transitional tryDispatch fallback and Packet::execute are gone);
//   - double registration is refused with a real runtime throw (not an
//     Assert that vanishes under NDEBUG), because two handlers silently
//     fighting over one id is how a mis-merge ships.
//
// The table is process-global and written only at startup, so each test
// registers a DISTINCT id and never re-registers one used by another
// test. Any valid id is safe HERE: the registry
// (Client/PacketHandlerRegistry.cpp, gamemodel's since task 4.218) fills
// the table only in test_packet_handler_registry.cpp, which clears it
// before and after - the ids are NOT "reserved" in the protocol (the id
// space is fully allocated up to PACKET_MAX).
//
// Compiles with the packetwire library's own defines - see
// tests/CMakeLists.txt.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "PacketDispatcher.h"
#include "Exception.h"

namespace {

//----------------------------------------------------------------------
// Minimal concrete packet carrying an arbitrary id.
//----------------------------------------------------------------------
class TestPacket : public Packet
{
public:
	explicit TestPacket(PacketID_t id) : m_ID(id) { }

	void read(SocketInputStream&) { }
	void write(SocketOutputStream&) const { }
	PacketID_t getPacketID() const  { return m_ID; }
	PacketSize_t getPacketSize() const  { return 0; }

private:
	PacketID_t m_ID;
};

// What the last handler invocation saw.
Packet* g_pSeenPacket = 0;
Player* g_pSeenPlayer = 0;
int     g_nCalls      = 0;

void recordingHandler(Packet* pPacket, Player* pPlayer)
{
	g_pSeenPacket = pPacket;
	g_pSeenPlayer = pPlayer;
	g_nCalls++;
}

// Distinct ids per test (see the file header for why any valid id is
// safe in this binary).
const PacketID_t ID_DISPATCH   = Packet::PACKET_MAX - 1;
const PacketID_t ID_DOUBLE     = Packet::PACKET_MAX - 3;
const PacketID_t ID_UNMIGRATED = Packet::PACKET_MAX - 4;	// never registered

} // namespace

TEST(PacketDispatcher, DispatchRunsTheRegisteredHandler)
{
	PacketDispatcher::registerHandler(ID_DISPATCH, &recordingHandler);

	TestPacket packet(ID_DISPATCH);

	// A distinct non-null sentinel, never dereferenced: comparing NULL
	// against NULL would pass even if the dispatcher dropped the player.
	int sentinel = 0;
	Player* pPlayer = reinterpret_cast<Player*>(&sentinel);

	int callsBefore = g_nCalls;
	g_pSeenPlayer = 0;
	PacketDispatcher::dispatch(&packet, pPlayer);

	CHECK_EQ(callsBefore + 1, g_nCalls);
	CHECK(g_pSeenPacket == &packet);
	CHECK(g_pSeenPlayer == pPlayer);
}

TEST(PacketDispatcher, DispatchThrowsForUnregisteredId)
{
	TestPacket packet(ID_UNMIGRATED);

	bool bThrew = false;
	try {
		PacketDispatcher::dispatch(&packet, 0);
	} catch (InvalidProtocolException&) {
		bThrew = true;
	}
	CHECK(bThrew);
}

TEST(PacketDispatcher, DoubleRegistrationIsRefused)
{
	PacketDispatcher::registerHandler(ID_DOUBLE, &recordingHandler);

	bool bThrew = false;
	try {
		PacketDispatcher::registerHandler(ID_DOUBLE, &recordingHandler);
	} catch (Error&) {
		bThrew = true;
	}
	CHECK(bThrew);
}

TEST(PacketDispatcher, OutOfRangeIdIsRejectedEverywhere)
{
	TestPacket packet(Packet::PACKET_MAX);

	bool bThrew = false;
	try {
		PacketDispatcher::dispatch(&packet, 0);
	} catch (InvalidProtocolException&) {
		bThrew = true;
	}
	CHECK(bThrew);

	bThrew = false;
	try {
		PacketDispatcher::registerHandler(Packet::PACKET_MAX, &recordingHandler);
	} catch (Error&) {
		bThrew = true;
	}
	CHECK(bThrew);
}
