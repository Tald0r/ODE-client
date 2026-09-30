//----------------------------------------------------------------------
// test_datagram_frame.cpp
//----------------------------------------------------------------------
//
// The UDP framing contract of Datagram, the peer-to-peer and
// login-server-port-check transport: what Datagram::read(DatagramPacket*&)
// accepts and refuses, and what the two bounded primitives every
// datagram packet parses through do at the end of the buffer.
//
// The bytes Datagram::write puts down are pinned as goldens in
// test_packet_goldens.cpp (CGPortCheck.datagram, RCPositionInfo.datagram),
// not here. Here the frame is built by hand, so the read side is
// checked against the wire layout and not against the writer.
//
// The read path creates the packet through g_pPacketFactoryManager and
// asks g_pPacketValidator which ids the client accepts over UDP, so
// these tests install a real manager and validator for their duration,
// the way test_player_base.cpp does.
//
// Compiled with the packetwire defines (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "Cpackets/CGPortCheck.h"
#include "Datagram.h"
#include "DatagramPacket.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "PacketDiagnostics.h"
#include "PacketIDSet.h"
#include "PlayerStatus.h"
#include "Gpackets/GLIncomingConnectionError.h"
#include "Rpackets/RCCharacterInfo.h"
#include "Rpackets/RCPositionInfo.h"
#include "Rpackets/RCSay.h"
#include "Rpackets/RCStatusHP.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

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

class ValidatorScope
{
public:
	explicit ValidatorScope(PacketValidator* pValidator)
	: m_pPrevious(g_pPacketValidator)
	{
		g_pPacketValidator = pValidator;
	}

	~ValidatorScope()
	{
		g_pPacketValidator = m_pPrevious;
	}

private:
	PacketValidator* m_pPrevious;
};

// The two tables GameInit.cpp builds before the UDP socket opens, each
// installed as its global for one test.
struct WireTables
{
	WireTables()
	: factoriesScope(&factories), validatorScope(&validator)
	{
		factories.init();
		validator.init();
	}

	PacketFactoryManager	factories;
	PacketValidator		validator;
	FactoryManagerScope	factoriesScope;
	ValidatorScope		validatorScope;
};

// A datagram holding exactly `bytes`.
void	Load(Datagram& datagram, const std::vector<unsigned char>& bytes)
{
	std::vector<char> buffer(bytes.begin(), bytes.end());
	datagram.setData(buffer.empty() ? NULL : &buffer[0], (uint)buffer.size());
}

void	AppendLE(std::vector<unsigned char>& out, unsigned long long value, size_t width)
{
	for (size_t i = 0; i < width; i++)
		out.push_back((unsigned char)((value >> (8 * i)) & 0xFF));
}

// RCPositionInfo's body as a peer would put it on the wire: a length
// byte, the name, then ZoneID (2), X (1), Y (1).
const char*	kPeerName = "Nosferatu";

std::vector<unsigned char>	PositionInfoBody()
{
	std::vector<unsigned char> body;
	const size_t nameLength = std::strlen(kPeerName);
	body.push_back((unsigned char)nameLength);
	body.insert(body.end(), kPeerName, kPeerName + nameLength);
	AppendLE(body, 0x8A9B, szZoneID);
	body.push_back(0xC5);
	body.push_back(0xD6);
	return body;
}

// A whole datagram: id, size field, body, then the one-byte pad both
// peers count in the length. `sizeField` and `length` default to the
// honest values and can be pushed off them.
std::vector<unsigned char>	Frame(PacketID_t id, const std::vector<unsigned char>& body,
				      long sizeDrift = 0, long padBytes = 1)
{
	std::vector<unsigned char> frame;
	AppendLE(frame, id, szPacketID);
	AppendLE(frame, (unsigned long long)((long)body.size() + sizeDrift), szPacketSize);
	frame.insert(frame.end(), body.begin(), body.end());
	for (long i = 0; i < padBytes; i++)
		frame.push_back(0x00);
	return frame;
}

enum ReadOutcome
{
	READ_PACKET,
	READ_INVALID_PROTOCOL,
	READ_ERROR,
	READ_OTHER
};

// Drive Datagram::read on `frame` and report how it ended. A packet
// that came back is handed to the caller through `ppPacket`. When the
// read throws, `pbLeftNull` says whether it left the caller's pointer
// NULL, as a refusal must: a packet handed back by a read that threw
// is one the caller has to know to delete.
ReadOutcome	ReadFrame(const std::vector<unsigned char>& frame, DatagramPacket** ppPacket = NULL,
			  bool* pbLeftNull = NULL)
{
	Datagram datagram;
	Load(datagram, frame);
	DatagramPacket* pPacket = NULL;
	ReadOutcome outcome = READ_PACKET;
	try {
		datagram.read(pPacket);
	} catch (InvalidProtocolException&) {
		outcome = READ_INVALID_PROTOCOL;
	} catch (Error&) {
		outcome = READ_ERROR;
	} catch (Throwable&) {
		outcome = READ_OTHER;
	}
	if (outcome != READ_PACKET)
	{
		if (pbLeftNull != NULL)
			*pbLeftNull = (pPacket == NULL);
		delete pPacket;
		return outcome;
	}
	if (ppPacket != NULL)
		*ppPacket = pPacket;
	else
		delete pPacket;
	return READ_PACKET;
}

} // namespace

//----------------------------------------------------------------------
// The read path rebuilds the packet the id names
//----------------------------------------------------------------------
TEST(Datagram, ReadRebuildsRCPositionInfoThroughTheFactory)
{
	WireTables tables;

	const std::vector<unsigned char> frame =
		Frame(Packet::PACKET_RC_POSITION_INFO, PositionInfoBody());
	CHECK_EQ(szPacketHeader + PositionInfoBody().size(), frame.size());

	Datagram datagram;
	Load(datagram, frame);
	datagram.setHost("127.0.0.1");
	datagram.setPort(0x9ABC);

	DatagramPacket* pPacket = NULL;
	datagram.read(pPacket);
	CHECK(pPacket != NULL);
	if (pPacket == NULL)
		return;

	RCPositionInfo* pInfo = dynamic_cast<RCPositionInfo*>(pPacket);
	CHECK(pInfo != NULL);
	if (pInfo != NULL)
	{
		CHECK_EQ((long long)Packet::PACKET_RC_POSITION_INFO, (long long)pInfo->getPacketID());
		CHECK(std::string(kPeerName) == pInfo->getName());
		CHECK_EQ(0x8A9B, pInfo->getZoneID());
		CHECK_EQ(0xC5, pInfo->getZoneX());
		CHECK_EQ(0xD6, pInfo->getZoneY());
		// The sender's address travels with the packet.
		CHECK(std::string("127.0.0.1") == pInfo->getHost());
		CHECK_EQ(0x9ABCu, pInfo->getPort());
	}
	delete pPacket;
}

//----------------------------------------------------------------------
// What the read path refuses, and with which exception
//----------------------------------------------------------------------
TEST(Datagram, ReadRefusesAnIdAtOrPastPacketMax)
{
	WireTables tables;

	const std::vector<unsigned char> body = PositionInfoBody();
	CHECK_EQ(READ_INVALID_PROTOCOL, ReadFrame(Frame((PacketID_t)Packet::PACKET_MAX, body)));
	CHECK_EQ(READ_INVALID_PROTOCOL, ReadFrame(Frame((PacketID_t)0xFFFF, body)));
}

TEST(Datagram, ReadRefusesASizeFieldOverTheFactoryMaximum)
{
	WireTables tables;

	const PacketSize_t maxSize = tables.factories.getPacketMaxSize(Packet::PACKET_RC_POSITION_INFO);
	std::vector<unsigned char> body(maxSize + 1, 0x00);
	body[0] = 0x00;	// an empty name, so the parser would not be the refuser
	CHECK_EQ(READ_INVALID_PROTOCOL, ReadFrame(Frame(Packet::PACKET_RC_POSITION_INFO, body)));
}

// The refusal is all there is: the size check used to send a bug
// report first, through PacketDiagnostics to SendBugReport, which posts
// a "*bug_report" chat line on the player's own server connection. A
// datagram comes from whoever can reach the UDP port, so that was one
// line to the server per hostile datagram, at the sender's pace.
namespace {

int	s_Reports = 0;

void	CountReport(const char*)
{
	s_Reports++;
}

struct ReportHookScope
{
	ReportHookScope() : m_Previous(PacketDiagnostics::getBugReportHook())
	{
		s_Reports = 0;
		PacketDiagnostics::setBugReportHook(&CountReport);
	}
	~ReportHookScope() { PacketDiagnostics::setBugReportHook(m_Previous); }

	PacketDiagnostics::BugReportFn m_Previous;
};

} // namespace

TEST(Datagram, ASizeFieldOverTheMaximumIsRefusedWithoutAReport)
{
	WireTables tables;
	ReportHookScope hook;

	const PacketSize_t maxSize = tables.factories.getPacketMaxSize(Packet::PACKET_RC_POSITION_INFO);
	std::vector<unsigned char> body(maxSize + 1, 0x00);
	CHECK_EQ(READ_INVALID_PROTOCOL, ReadFrame(Frame(Packet::PACKET_RC_POSITION_INFO, body)));
	CHECK_EQ(0, s_Reports);
}

// Both peers count the datagram as szPacketHeader + size field: one
// byte more than the id, the size and the body. A datagram shorter
// than that is "not read whole", a longer one "several at once", and
// both are refused before any packet is created.
TEST(Datagram, ReadRefusesALengthThatDisagreesWithTheSizeField)
{
	WireTables tables;

	const std::vector<unsigned char> body = PositionInfoBody();
	CHECK_EQ(READ_PACKET, ReadFrame(Frame(Packet::PACKET_RC_POSITION_INFO, body, 0, 1)));
	CHECK_EQ(READ_ERROR, ReadFrame(Frame(Packet::PACKET_RC_POSITION_INFO, body, 0, 0)));
	CHECK_EQ(READ_ERROR, ReadFrame(Frame(Packet::PACKET_RC_POSITION_INFO, body, 0, 2)));
	CHECK_EQ(READ_ERROR, ReadFrame(Frame(Packet::PACKET_RC_POSITION_INFO, body, 1, 1)));
	CHECK_EQ(READ_ERROR, ReadFrame(Frame(Packet::PACKET_RC_POSITION_INFO, body, -1, 1)));
}

//----------------------------------------------------------------------
// Only a datagram packet the client accepts over UDP is created
//----------------------------------------------------------------------
//
// The factory registers every packet the client receives, whatever the
// connection: GC and LC over TCP from the servers, CR and three of the
// RC packets over the TCP request-server connection between clients,
// and the four RC packets that are DatagramPackets over UDP. The read
// path used to check only that the id was below PACKET_MAX, then
// C-cast whatever the factory built to DatagramPacket* and call
// read(Datagram&) through it: for any id but those four that is a
// virtual call through an object that is not a DatagramPacket, which
// runs whatever sits in that vtable slot. The UDP socket is bound on
// INADDR_ANY from start-up, so anyone who can send the client a
// datagram chose the id (code-health review, Found by reading).
//----------------------------------------------------------------------
namespace {

// A frame for `id` whose zero-filled body the size and length gates
// accept, so what refuses it is the id check or, were that missing, the
// conversion check (ARefusedIdCreatesNoPacket tells the two apart).
std::vector<unsigned char>	ZeroFrame(PacketFactoryManager& factories, PacketID_t id)
{
	const PacketSize_t maxSize = factories.getPacketMaxSize(id);
	return Frame(id, std::vector<unsigned char>(maxSize < 16 ? maxSize : 16, 0x00));
}

// Registered packet classes that are not DatagramPackets, from the
// three connections they arrive on: one GC id (the game connection),
// one LC id (the login connection), and every CR and RC class of the
// request-server connection between clients.
const PacketID_t	kNotDatagramIds[] = {
	Packet::PACKET_GC_SAY,			// game server, TCP
	Packet::PACKET_LC_LOGIN_OK,		// login server, TCP
	Packet::PACKET_CR_CONNECT,		// request-server connection, TCP
	Packet::PACKET_CR_DISCONNECT,
	Packet::PACKET_CR_REQUEST,
	Packet::PACKET_CR_WHISPER,
	Packet::PACKET_RC_CONNECT_VERIFY,
	Packet::PACKET_RC_REQUEST_VERIFY,
	Packet::PACKET_RC_REQUESTED_FILE,
};

} // namespace

TEST(Datagram, ReadRefusesAnIdThatIsNotADatagramPacket)
{
	WireTables tables;

	for (PacketID_t id : kNotDatagramIds)
	{
		bool bLeftNull = false;
		CHECK_EQ(READ_INVALID_PROTOCOL, ReadFrame(ZeroFrame(tables.factories, id), NULL, &bLeftNull));
		CHECK(bLeftNull);
	}
}

// The refusal comes before any packet exists, not only before its
// read: Datagram::read's conversion check refuses these ids too, after
// createPacket, so the test above alone would pass with the id check
// removed. Here each id's factory is one that counts what it creates,
// wrapped around the real one, and a refused id must have created
// nothing. RCPositionInfo, which is accepted, is the positive control:
// its factory counts one. With the id check deleted from Datagram.cpp
// this test counted 9 packets created for the 9 refused ids.
namespace {

int	s_Created = 0;

class CountingFactory : public PacketFactory
{
public:
	CountingFactory(PacketFactoryManager& real, PacketID_t id)
	: m_Real(real), m_ID(id) {}

	Packet*		createPacket()
	{
		s_Created++;
		return m_Real.createPacket(m_ID);
	}

#ifdef __DEBUG_OUTPUT__
	std::string	getPacketName() const { return m_Real.getPacketName(m_ID); }
#endif

	PacketID_t	getPacketID() const { return m_ID; }

	PacketSize_t	getPacketMaxSize() const { return m_Real.getPacketMaxSize(m_ID); }

private:
	PacketFactoryManager&	m_Real;
	PacketID_t		m_ID;
};

} // namespace

TEST(Datagram, ARefusedIdCreatesNoPacket)
{
	PacketFactoryManager real;
	real.init();

	// Only the ids read below are registered; every other slot is empty.
	PacketFactoryManager counting;
	for (PacketID_t id : kNotDatagramIds)
		counting.addFactory(new CountingFactory(real, id));
	counting.addFactory(new CountingFactory(real, Packet::PACKET_RC_POSITION_INFO));
	FactoryManagerScope factoriesScope(&counting);

	PacketValidator validator;
	validator.init();
	ValidatorScope validatorScope(&validator);

	s_Created = 0;
	for (PacketID_t id : kNotDatagramIds)
	{
		bool bLeftNull = false;
		CHECK_EQ(READ_INVALID_PROTOCOL, ReadFrame(ZeroFrame(counting, id), NULL, &bLeftNull));
		CHECK(bLeftNull);
	}
	CHECK_EQ(0, s_Created);

	CHECK_EQ(READ_PACKET, ReadFrame(Frame(Packet::PACKET_RC_POSITION_INFO, PositionInfoBody())));
	CHECK_EQ(1, s_Created);
}

// The four the client does accept still read: each written by its own
// class, as the peer that sent it would, and read back.
namespace {

template <class PacketT>
ReadOutcome	RoundTrip(const PacketT& packet)
{
	Datagram written;
	written.write(&packet);
	const unsigned char* data = (const unsigned char*)written.getData();
	DatagramPacket* pPacket = NULL;
	const ReadOutcome outcome =
		ReadFrame(std::vector<unsigned char>(data, data + written.getLength()), &pPacket);
	if (outcome == READ_PACKET)
	{
		CHECK(pPacket != NULL);
		CHECK(dynamic_cast<PacketT*>(pPacket) != NULL);
		CHECK_EQ((long long)packet.getPacketID(), (long long)pPacket->getPacketID());
	}
	delete pPacket;
	return outcome;
}

} // namespace

TEST(Datagram, ReadStillAcceptsTheFourPeerDatagramPackets)
{
	WireTables tables;

	RCCharacterInfo characterInfo;
	characterInfo.setName("Nosferatu");
	characterInfo.setGuildID(0x1234);
	CHECK_EQ(READ_PACKET, RoundTrip(characterInfo));

	RCPositionInfo positionInfo;
	positionInfo.setName("Nosferatu");
	positionInfo.setZoneID(0x8A9B);
	positionInfo.setZoneX(0xC5);
	positionInfo.setZoneY(0xD6);
	CHECK_EQ(READ_PACKET, RoundTrip(positionInfo));

	RCSay say;
	say.setName("Nosferatu");
	say.setMessage("evening");
	say.setColor(0x00FF8040);
	CHECK_EQ(READ_PACKET, RoundTrip(say));

	RCStatusHP statusHP;
	statusHP.setName("Nosferatu");
	statusHP.setMaxHP(0x0123);
	statusHP.setCurrentHP(0x0045);
	CHECK_EQ(READ_PACKET, RoundTrip(statusHP));
}

// The accepted set is the validator's CPS_CLIENT_COMMUNICATION_NORMAL
// set, the one Update() checks. The other side of it is which classes
// are DatagramPackets; they must agree, or the set admits an id whose
// packet cannot be read from a datagram, or refuses one that can.
TEST(Datagram, TheAcceptedIdsAreExactlyTheRegisteredDatagramPackets)
{
	WireTables tables;

	int datagramPackets = 0;
	for (uint id = 0; id < (uint)Packet::PACKET_MAX; id++)
	{
		Packet* pPacket = NULL;
		try {
			pPacket = tables.factories.createPacket((PacketID_t)id);
		} catch (Throwable&) {
			pPacket = NULL;		// no factory: nothing to create
		}
		const bool bDatagram = dynamic_cast<DatagramPacket*>(pPacket) != NULL;
		delete pPacket;

		const bool bAccepted =
			tables.validator.isValidPacketID(CPS_CLIENT_COMMUNICATION_NORMAL, (PacketID_t)id);
		if (bDatagram != bAccepted)
			std::fprintf(stderr, "  packet id %u: DatagramPacket %d, accepted over UDP %d\n",
				id, (int)bDatagram, (int)bAccepted);
		CHECK_EQ(bDatagram, bAccepted);
		if (bDatagram)
			datagramPackets++;
	}
	CHECK_EQ(4, datagramPackets);
}

// The conversion is checked too, so a set that some day admits an id
// whose class is not a DatagramPacket gets a refusal and not the
// confusion back. Here the set is built to admit GCSay.
TEST(Datagram, ASetThatAdmitsANonDatagramIdStillGetsNoPacket)
{
	PacketFactoryManager factories;
	factories.init();
	FactoryManagerScope factoriesScope(&factories);

	PacketValidator validator;
	PacketIDSet* pSet = new PacketIDSet(CPS_CLIENT_COMMUNICATION_NORMAL);
	pSet->addPacketID(Packet::PACKET_GC_SAY);
	validator.addPacketIDSet(CPS_CLIENT_COMMUNICATION_NORMAL, pSet);
	ValidatorScope validatorScope(&validator);

	bool bLeftNull = false;
	CHECK_EQ(READ_INVALID_PROTOCOL, ReadFrame(ZeroFrame(factories, Packet::PACKET_GC_SAY), NULL, &bLeftNull));
	CHECK(bLeftNull);
}

// With no validator installed nothing is accepted: the check fails
// closed rather than reading the id unchecked.
TEST(Datagram, ReadWithNoValidatorRefusesEveryId)
{
	PacketFactoryManager factories;
	factories.init();
	FactoryManagerScope factoriesScope(&factories);
	ValidatorScope validatorScope(NULL);

	bool bLeftNull = false;
	CHECK_EQ(READ_INVALID_PROTOCOL,
		ReadFrame(Frame(Packet::PACKET_RC_POSITION_INFO, PositionInfoBody()), NULL, &bLeftNull));
	CHECK(bLeftNull);
}

// A packet whose own read throws is not handed back half-read: the
// caller's pointer stays NULL and the packet is freed inside read().
// Here the name's length byte claims nine bytes and the body holds one.
TEST(Datagram, APacketWhoseReadThrowsIsNotHandedBack)
{
	WireTables tables;

	std::vector<unsigned char> body;
	body.push_back(9);
	body.push_back('N');
	bool bLeftNull = false;
	CHECK_EQ(READ_OTHER, ReadFrame(Frame(Packet::PACKET_RC_POSITION_INFO, body), NULL, &bLeftNull));
	CHECK(bLeftNull);
}

//----------------------------------------------------------------------
// The two bounded primitives every datagram packet parses through
//----------------------------------------------------------------------
TEST(Datagram, ReadingPastTheEndUnderflows)
{
	std::vector<unsigned char> bytes;
	bytes.push_back(0xA1);
	bytes.push_back(0xB2);
	bytes.push_back(0xC3);

	{
		Datagram datagram;
		Load(datagram, bytes);
		char buffer[4] = { 0, 0, 0, 0 };
		datagram.read(buffer, 3);
		CHECK_EQ(0, std::memcmp(buffer, "\xA1\xB2\xC3", 3));
		bool bThrew = false;
		try {
			datagram.read(buffer, 1);
		} catch (InsufficientDataException&) {
			bThrew = true;
		}
		CHECK(bThrew);
	}
	{
		Datagram datagram;
		Load(datagram, bytes);
		std::string text;
		datagram.read(text, 2);
		CHECK(std::string("\xA1\xB2") == text);
		bool bThrew = false;
		try {
			datagram.read(text, 2);
		} catch (InsufficientDataException&) {
			bThrew = true;
		}
		CHECK(bThrew);
		// A refused read leaves the string as it was.
		CHECK(std::string("\xA1\xB2") == text);
	}
}

//----------------------------------------------------------------------
// The pad byte goes on the wire, so it must be a value and not what
// the allocator left behind
//----------------------------------------------------------------------

// Every byte the datagram sends is written: the header, the body, and
// the one-byte pad in the sequence slot, which both peers count in the
// length and neither reads. The server writes it as zero (its
// Datagram zero-fills the buffer for exactly this reason); a datagram
// buffer that came from a bare new char[] carries a heap byte there
// instead - 0xCD under the MSVC debug heap, anything in Release.
TEST(Datagram, ThePadByteBehindTheBodyIsZero)
{
	CGPortCheck packet;
	packet.setPCName("WirePin");

	// Several datagrams. Under the MSVC debug heap and under ASan a bare
	// new char[] hands back a fill pattern, so the unfixed code fails
	// every iteration (0xCD); an allocator that returns zeroed pages
	// would let it pass, and the loop cannot buy more than that.
	for (int i = 0; i < 8; i++)
	{
		Datagram datagram;
		datagram.write(&packet);
		const unsigned char* data = (const unsigned char*)datagram.getData();
		CHECK_EQ(szPacketHeader + packet.getPacketSize(), datagram.getLength());
		CHECK_EQ(0, data[datagram.getLength() - 1]);
	}
}

// A datagram sized for a body but never fully written by it must not
// leak either: setData(len) hands back zeroed bytes.
TEST(Datagram, ABufferSizedForWritingStartsZeroed)
{
	for (int i = 0; i < 8; i++)
	{
		Datagram datagram;
		datagram.setData(64);
		const unsigned char* data = (const unsigned char*)datagram.getData();
		int nonZero = 0;
		for (uint j = 0; j < datagram.getLength(); j++)
			if (data[j] != 0)
				nonZero++;
		CHECK_EQ(0, nonZero);
	}
}

//----------------------------------------------------------------------
// The typed scalar layer: every Datagram overload writes its declared
// width and reads it back, through the same WireScalar rule the
// socket streams use
//----------------------------------------------------------------------
namespace {

enum class PeerFlag : std::uint8_t { NONE = 0, BUSY = 0x9C };

} // namespace

TEST(Datagram, TypedOverloadsWriteTheirDeclaredWidthsAndReadThemBack)
{
	Datagram out;
	out.setData(1 + 1 + 2 + 2 + 4 + 4 + 4 + 4 + 2 + 1);
	out.write((char)-1);
	out.write((uchar)0x81);
	out.write((short)-2);
	out.write((ushort)0x8283);
	out.write((int)-3);
	out.write((uint)0x84858687u);
	out.write((long)-4);
	out.write((ulong)0x88898A8Bu);
	out.writeWire((std::uint16_t)0x8C8D);
	out.writeWire(PeerFlag::BUSY);

	const unsigned char expected[] = {
		0xFF,
		0x81,
		0xFE, 0xFF,
		0x83, 0x82,
		0xFD, 0xFF, 0xFF, 0xFF,
		0x87, 0x86, 0x85, 0x84,
		0xFC, 0xFF, 0xFF, 0xFF,
		0x8B, 0x8A, 0x89, 0x88,
		0x8D, 0x8C,
		0x9C
	};
	CHECK_EQ(sizeof(expected), out.getLength());
	CHECK_EQ(0, std::memcmp(out.getData(), expected, sizeof(expected)));

	// The same bytes, arriving: setData(data, len) is the receive path.
	Datagram in;
	in.setData(out.getData(), out.getLength());
	char c = 0; uchar uc = 0; short s = 0; ushort us = 0; int i = 0; uint ui = 0;
	long l = 0; ulong ul = 0; std::uint16_t u16 = 0; PeerFlag flag = PeerFlag::NONE;
	in.read(c);
	in.read(uc);
	in.read(s);
	in.read(us);
	in.read(i);
	in.read(ui);
	in.read(l);
	in.read(ul);
	in.readWire(u16);
	in.readWire(flag);
	CHECK_EQ((char)-1, c);
	CHECK_EQ((uchar)0x81, uc);
	CHECK_EQ((short)-2, s);
	CHECK_EQ((ushort)0x8283, us);
	CHECK_EQ(-3, i);
	CHECK_EQ(0x84858687u, ui);
	CHECK_EQ(-4L, l);
	CHECK_EQ((ulong)0x88898A8Bu, ul);
	CHECK_EQ((std::uint16_t)0x8C8D, u16);
	CHECK(PeerFlag::BUSY == flag);

	// Nothing left: the reads consumed exactly the widths the writes put down.
	bool bThrew = false;
	try {
		char extra = 0;
		in.read(extra);
	} catch (InsufficientDataException&) {
		bThrew = true;
	}
	CHECK(bThrew);
}

// A typed read at the end of the buffer is refused whole: no partial
// scalar, and the value the caller passed is untouched.
TEST(Datagram, ATypedReadShortOfItsWidthUnderflowsAndLeavesTheValue)
{
	std::vector<unsigned char> bytes;
	bytes.push_back(0xA1);
	bytes.push_back(0xB2);
	bytes.push_back(0xC3);
	Datagram datagram;
	Load(datagram, bytes);

	uint value = 0x11223344u;
	bool bThrew = false;
	try {
		datagram.read(value);
	} catch (InsufficientDataException&) {
		bThrew = true;
	}
	CHECK(bThrew);
	CHECK_EQ(0x11223344u, value);

	// The three bytes are still there for a read that fits.
	ushort first = 0;
	datagram.read(first);
	CHECK_EQ((ushort)0xB2A1, first);
}

//----------------------------------------------------------------------
// The bounds checks survive a length that wraps and a Release build
// (code-health review, Medium: "Datagram bounds checks compute
// m_InputOffset + len in unsigned arithmetic that can wrap")
//----------------------------------------------------------------------

// A length that pushes m_InputOffset + len past UINT_MAX used to pass
// the check and hand memcpy a read of most of the address space. Both
// primitives refuse it, and refuse it before touching memory - under
// the unfixed code this test does not fail, it takes the process down.
// Of the four lengths only the first wraps at offset 1 (1 + 0xFFFFFFFF
// is 0); the other three are merely too long, and the old check refused
// them too. They are here so the wrapping one is not the only path.
TEST(Datagram, AReadLengthThatWrapsTheOffsetIsRefused)
{
	std::vector<unsigned char> bytes;
	bytes.push_back(0xA1);
	bytes.push_back(0xB2);
	bytes.push_back(0xC3);

	const uint lengths[] = { 0xFFFFFFFFu, 0xFFFFFFFEu, 0xFFFFFFFDu, 0x80000000u };

	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++)
	{
		Datagram datagram;
		Load(datagram, bytes);
		char first = 0;
		datagram.read(first);	// offset 1, so 1 + 0xFFFFFFFF wraps to 0
		CHECK_EQ((char)0xA1, first);

		char sink[4] = { 0, 0, 0, 0 };
		bool bThrew = false;
		try {
			datagram.read(sink, lengths[i]);
		} catch (InsufficientDataException&) {
			bThrew = true;
		}
		CHECK(bThrew);

		std::string text("untouched");
		bThrew = false;
		try {
			datagram.read(text, lengths[i]);
		} catch (InsufficientDataException&) {
			bThrew = true;
		}
		CHECK(bThrew);
		CHECK(std::string("untouched") == text);

		// The datagram is still readable from where it was.
		ushort rest = 0;
		datagram.read(rest);
		CHECK_EQ((ushort)0xC3B2, rest);
	}
}

// The write bound was an Assert, which NDEBUG compiles away: a body
// that outgrows the buffer its declared size bought wrote past the
// heap block in Release. It is a runtime check now, in every build,
// and refuses a wrapping length the same way. Two of the four lengths
// wrap (2 + 0xFFFFFFFF is 1, 2 + 0xFFFFFFFE is 0), and those passed
// the Assert as well, so on the unfixed code this test is a second
// reproduction: it does not fail, it takes the process down. What this
// test cannot show is the Release half - the suite is a Debug build,
// where the Assert threw too.
TEST(Datagram, AWritePastTheBufferIsRefusedInEveryBuild)
{
	Datagram datagram;
	datagram.setData(4);
	datagram.write((ushort)0x8182);

	const uint lengths[] = { 3, 5, 0xFFFFFFFFu, 0xFFFFFFFEu };
	const char source[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++)
	{
		bool bThrew = false;
		try {
			datagram.write(source, lengths[i]);
		} catch (Error&) {
			bThrew = true;
		}
		CHECK(bThrew);
	}

	// What fits still goes in, and nothing before it moved.
	datagram.write((ushort)0x8384);
	const unsigned char expected[] = { 0x82, 0x81, 0x84, 0x83 };
	CHECK_EQ(0, std::memcmp(datagram.getData(), expected, sizeof(expected)));

	// Full: one more byte is refused too.
	bool bThrew = false;
	try {
		datagram.write((char)1);
	} catch (Error&) {
		bThrew = true;
	}
	CHECK(bThrew);
}

//----------------------------------------------------------------------
// A packet body is held to the size its header declares
//----------------------------------------------------------------------
namespace {

// A datagram packet whose write() emits `bodySize` bytes while its
// getPacketSize() declares `bodySize + drift` - the datagram twin of
// the stream tests' drifting packet, and the server's
// DriftingDatagramPacket.
class DriftingDatagramPacket : public DatagramPacket
{
public:
	DriftingDatagramPacket(uint bodySize, int drift)
	: m_BodySize(bodySize), m_Drift(drift)
	{
	}

	void read(Datagram&) { throw UnsupportedError(); }
	void write(Datagram& oDatagram) const
	{
		for (uint i = 0; i < m_BodySize; i++)
			oDatagram.write((uchar)(0xA0 + i));
	}
	PacketID_t getPacketID() const noexcept { return 0x4321; }
	PacketSize_t getPacketSize() const { return (PacketSize_t)((int)m_BodySize + m_Drift); }
#ifdef __DEBUG_OUTPUT__
	std::string getPacketName() const { return "DriftingDatagramPacket"; }
	std::string toString() const { return "DriftingDatagramPacket"; }
#endif

private:
	uint m_BodySize;
	int m_Drift;
};

bool	WriteIsRefused(const DatagramPacket& packet)
{
	Datagram datagram;
	try {
		datagram.write(&packet);
	} catch (Error&) {
		return true;
	}
	return false;
}

} // namespace

// The buffer holds one byte more than the body - the pad - so the bound
// alone would let a body one byte over its declaration eat the pad and
// go out looking honest, with the peer dropping the last field. A body
// under its declaration would send zeros the peer parses as fields.
// Both are refused; an honest packet frames as before.
TEST(Datagram, ABodyThatDisagreesWithItsDeclaredSizeIsRefused)
{
	CHECK(WriteIsRefused(DriftingDatagramPacket(6, +1)));
	CHECK(WriteIsRefused(DriftingDatagramPacket(6, -1)));
	CHECK(WriteIsRefused(DriftingDatagramPacket(6, +5)));
	CHECK(WriteIsRefused(DriftingDatagramPacket(6, -6)));

	DriftingDatagramPacket honest(6, 0);
	Datagram datagram;
	datagram.write(&honest);
	const unsigned char expected[] = {
		0x21, 0x43,
		0x06, 0x00, 0x00, 0x00,
		0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5,
		0x00
	};
	CHECK_EQ(sizeof(expected), datagram.getLength());
	CHECK_EQ(0, std::memcmp(datagram.getData(), expected, sizeof(expected)));
}

// GLIncomingConnectionError declared only its first string while its
// write() emits two; the server's copy declares both. Under the old
// Assert-only bound that was a silent heap overrun in Release, and
// under the declared-size check it would be a refusal. The client
// never sends or receives this packet (its factory sat in the
// server-side registration list, which task 5.2's ninth slice deleted),
// so this pins the cross-repo agreement.
TEST(GLIncomingConnectionError, DeclaresBothStrings)
{
	GLIncomingConnectionError packet;
	packet.setMessage("no room");
	packet.setPlayerID("Reiot");
	CHECK_EQ((PacketSize_t)(1 + 7 + 1 + 5), packet.getPacketSize());

	Datagram datagram;
	datagram.write(&packet);
	CHECK_EQ(szPacketHeader + packet.getPacketSize(), datagram.getLength());
}
