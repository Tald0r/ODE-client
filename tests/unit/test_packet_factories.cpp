//----------------------------------------------------------------------
// test_packet_factories.cpp
//----------------------------------------------------------------------
//
// The packet classes, the factory manager and the validator compile
// into packetwire since docs/reviews/RESTRUCTURING.md task 2.4, and this test
// is the link-level proof for the directions the client RECEIVES:
// PacketFactoryManager::init() news every factory the client registers
// (GC, LC, RC, UC - not CG/CL, which the client only writes and never
// registers), each factory's vtable references its packet's
// constructor and vtable, so constructing one manager pulls every
// received-direction packet object in the archive into this test
// binary. A received-direction packet source that still reached into
// the executable (a game global, a handler body, a debug facility)
// would fail this binary's link, not just a grep. The written
// directions get the same proof from test_wire_layout.cpp, which
// constructs every factory under Client/Packet directly (task 2.5).
//
// The behavioral checks are deliberately thin: the wire layout is
// pinned elsewhere (test_wire_layout.cpp, test_packet_goldens.cpp).
//
// Compiled with the packetwire defines (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "Packet.h"
#include "PacketFactoryManager.h"
#include "Exception.h"
#include <memory>

#include "Gpackets/GCSay.h"
#include "Lpackets/LCLoginOK.h"
#include "Upackets/UCRequestLoginMode.h"

namespace {

template <class PacketT>
bool	CreatesA(PacketFactoryManager& manager, PacketID_t id)
{
	Packet* pPacket = manager.createPacket(id);
	const bool bOk = pPacket != NULL
		&& dynamic_cast<PacketT*>(pPacket) != NULL
		&& pPacket->getPacketID() == id;
	delete pPacket;
	return bOk;
}

bool	Rejects(PacketFactoryManager& manager, PacketID_t id)
{
	try {
		Packet* pPacket = manager.createPacket(id);
		delete pPacket;
	} catch (InvalidProtocolException&) {
		return true;
	}
	return false;
}

} // namespace

//----------------------------------------------------------------------
// One manager, every registered factory constructed: the link proof.
// A packet from each direction the client receives comes back as the
// right concrete type under its own id.
//----------------------------------------------------------------------
TEST(PacketFactoryManager, InitRegistersTheReceivedDirectionsAndLinksWithoutTheGame)
{
	PacketFactoryManager manager;
	manager.init();

	CHECK(CreatesA<GCSay>(manager, Packet::PACKET_GC_SAY));
	CHECK(CreatesA<LCLoginOK>(manager, Packet::PACKET_LC_LOGIN_OK));
	CHECK(CreatesA<UCRequestLoginMode>(manager, Packet::PACKET_UC_REQUEST_LOGIN_MODE));
}

//----------------------------------------------------------------------
// The manager's max-size table is what PacketValidator sizes the read
// against; it must be the factory's own answer, not a copy that can
// drift.
//----------------------------------------------------------------------
TEST(PacketFactoryManager, MaxSizeComesFromTheFactory)
{
	PacketFactoryManager manager;
	manager.init();

	CHECK_EQ(GCSayFactory().getPacketMaxSize(),
		 manager.getPacketMaxSize(Packet::PACKET_GC_SAY));
	CHECK_EQ(LCLoginOKFactory().getPacketMaxSize(),
		 manager.getPacketMaxSize(Packet::PACKET_LC_LOGIN_OK));
}

//----------------------------------------------------------------------
// The client registers only what it receives. CG packets are what it
// SENDS - it constructs them directly, never through the manager - so
// their factories stay unregistered here, and an incoming CG id (which
// only a misbehaving peer would send) is refused with the protocol
// exception rather than served. An id past the table is refused the
// same way, not indexed.
//----------------------------------------------------------------------
TEST(PacketFactoryManager, UnregisteredAndOutOfRangeIdsAreRefused)
{
	PacketFactoryManager manager;
	manager.init();

	CHECK(Rejects(manager, Packet::PACKET_CG_MOVE));
	CHECK(Rejects(manager, Packet::PACKET_MAX));
	CHECK(Rejects(manager, (PacketID_t)0xFFFF));
}

namespace {
class InvalidIdFactory : public GCSayFactory
{
public:
	explicit InvalidIdFactory(PacketID_t id) : m_Id(id) {}
	PacketID_t getPacketID() const noexcept override { return m_Id; }
private:
	PacketID_t m_Id;
};
}

TEST(PacketFactoryManager, RegistrationRejectsInvalidIdsBeforeIndexing)
{
	PacketFactoryManager manager;
	manager.addFactory(new GCSayFactory);
	for (PacketID_t id : {static_cast<PacketID_t>(Packet::PACKET_MAX), static_cast<PacketID_t>(0xFFFF)})
	{
		auto candidate = std::make_unique<InvalidIdFactory>(id);
		bool refused = false;
		try { manager.addFactory(candidate.get()); candidate.release(); }
		catch (const InvalidProtocolException&) { refused = true; }
		catch (const Throwable&) {}
		CHECK(refused);
		CHECK(CreatesA<GCSay>(manager, Packet::PACKET_GC_SAY));
	}
}

TEST(PacketFactoryManager, RegistrationRejectsNull)
{
	PacketFactoryManager manager;
	bool refused = false;
	try { manager.addFactory(nullptr); }
	catch (const InvalidProtocolException&) { refused = true; }
	CHECK(refused);
}

TEST(PacketFactoryManager, DuplicateRegistrationLeavesTheOriginalFactory)
{
	PacketFactoryManager manager;
	manager.addFactory(new GCSayFactory);
	auto candidate = std::make_unique<GCSayFactory>();
	bool refused = false;
	try { manager.addFactory(candidate.get()); candidate.release(); }
	catch (const Error&) { refused = true; }
	CHECK(refused);
	CHECK(CreatesA<GCSay>(manager, Packet::PACKET_GC_SAY));
	CHECK_EQ(GCSayFactory().getPacketMaxSize(), manager.getPacketMaxSize(Packet::PACKET_GC_SAY));
}
