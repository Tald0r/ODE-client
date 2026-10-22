//----------------------------------------------------------------------
// test_packetwire_parsers.cpp
//----------------------------------------------------------------------
//
// First tests against the packetwire library (docs/reviews/RESTRUCTURING.md,
// task 1.2). The wire info classes parse server-supplied counts in a
// loop, and their only defense against a hostile or corrupt count is
// the stream's own underflow check: SocketInputStream::read throws
// InsufficientDataException before touching bytes it does not have.
// These tests pin that contract from both sides - the stream's bounds
// behavior directly, and the parsers' reliance on it.
//
// Test seam: fill() is the only production writer of the stream's ring
// buffer and needs a connected peer, so SocketInputStreamTestAccess
// (befriended by the class) preloads the buffer directly. The Socket
// handed to the stream is never used - the constructor only asserts it
// is non-null - and closing a never-created socket is a no-op, so the
// Winsock start below only matters to tests that create a real one.
//
// This file compiles with the same defines as the packetwire library
// (__WIN32__/__WINDOWS__ on Windows - set in tests/CMakeLists.txt) so
// every shared class definition is identical to the library's.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "SocketInputStream.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "Packet.h"
#include "ModifyInfo.h"
#include "InventoryInfo.h"
#include "Gpackets/GCUpdateInfo.h"
#include "Gpackets/GCGlobalChat.h"
#include "Gpackets/GCPhoneConnected.h"
#include "UserInformation.h"
#include "Exception.h"

#include <string>
#include <string.h>
#include <stdexcept>
#include <vector>

namespace {

//----------------------------------------------------------------------
// Stream fixture: a small ring over a never-used socket.
//----------------------------------------------------------------------
struct StreamFixture
{
	Socket				m_Socket;
	SocketInputStream	m_Stream;

	explicit StreamFixture(uint bufferLen = 64)
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, bufferLen)
	{
	}

	void Preload(const unsigned char* data, unsigned int len, unsigned int head = 0)
	{
		SocketInputStreamTestAccess::Preload(m_Stream, data, len, head);
	}
};

//----------------------------------------------------------------------
// ModifyInfo is abstract (it inherits Packet's pure getPacketID()); the
// concrete subclasses are the GC packets. This minimal subclass exists
// only so read() can run against a real object.
//----------------------------------------------------------------------
class TestModifyInfo : public ModifyInfo
{
public:
	PacketID_t getPacketID() const  { return 0; }
};

//----------------------------------------------------------------------
// A deliberately simple packet for exercising the stream's frame boundary.
// Its parser reads a fixed number of body bytes regardless of the size in the
// wire header, making both over-consumption and under-consumption observable.
//----------------------------------------------------------------------
class FixedReadPacket : public Packet
{
public:
	explicit FixedReadPacket(unsigned int bytesToRead)
	: m_BytesToRead(bytesToRead), m_LastByte(0)
	{
	}

	void read(SocketInputStream& iStream)
	{
		for (unsigned int i = 0; i < m_BytesToRead; i++)
			iStream.read(m_LastByte);
	}

	void write(SocketOutputStream&) const {}
	PacketID_t getPacketID() const { return 1; }
	PacketSize_t getPacketSize() const { return m_BytesToRead; }
	unsigned char getLastByte() const { return (unsigned char)m_LastByte; }

private:
	unsigned int m_BytesToRead;
	char m_LastByte;
};

class ExplicitUnderflowPacket : public FixedReadPacket
{
public:
	ExplicitUnderflowPacket() : FixedReadPacket(0) {}
	void read(SocketInputStream&) { throw InsufficientDataException(); }
};

class StandardExceptionPacket : public FixedReadPacket
{
public:
	StandardExceptionPacket() : FixedReadPacket(0) {}
	void read(SocketInputStream&) { throw std::runtime_error("test parser failure"); }
};

enum BoundaryOperation
{
	BOUNDARY_STRING_READ,
	BOUNDARY_PEEK,
	BOUNDARY_SKIP
};

class BoundaryOperationPacket : public FixedReadPacket
{
public:
	explicit BoundaryOperationPacket(BoundaryOperation operation)
	: FixedReadPacket(0), m_Operation(operation)
	{
	}

	void read(SocketInputStream& iStream)
	{
		char bytes[2];
		switch (m_Operation)
		{
		case BOUNDARY_STRING_READ:
		{
			std::string text;
			iStream.read(text, 2);
			break;
		}
		case BOUNDARY_PEEK:
			iStream.peek(bytes, 2);
			break;
		case BOUNDARY_SKIP:
			iStream.skip(2);
			break;
		}
	}

private:
	BoundaryOperation m_Operation;
};

enum SwallowedOperation
{
	SWALLOWED_SCALAR,
	SWALLOWED_SPAN,
	SWALLOWED_STRING,
	SWALLOWED_PEEK,
	SWALLOWED_SKIP,
	SWALLOWED_DESTINATION_LENGTH,
	SWALLOWED_EMPTY_SPAN,
	SWALLOWED_EMPTY_STRING,
	SWALLOWED_EMPTY_PEEK,
	SWALLOWED_EMPTY_SKIP,
	SWALLOWED_NESTED_FRAME,
	SWALLOWED_OPERATION_COUNT
};

class SwallowedFailurePacket : public FixedReadPacket
{
public:
	SwallowedFailurePacket(SwallowedOperation operation, bool consumeFirst)
	: FixedReadPacket(1), m_Operation(operation), m_ConsumeFirst(consumeFirst), m_Caught(false) {}

	void read(SocketInputStream& stream)
	{
		if (m_ConsumeFirst)
			FixedReadPacket::read(stream);
		try {
			char bytes[2] = {};
			std::string text;
			uint16_t scalar = 0;
			switch (m_Operation) {
			case SWALLOWED_SCALAR: stream.readWire(scalar); break;
			case SWALLOWED_SPAN: stream.read(std::as_writable_bytes(std::span(bytes))); break;
			case SWALLOWED_STRING: stream.read(text, 2); break;
			case SWALLOWED_PEEK: stream.peek(bytes, 2); break;
			case SWALLOWED_SKIP: stream.skip(2); break;
			case SWALLOWED_DESTINATION_LENGTH: stream.read(std::span<char>(bytes, 1), 2); break;
			case SWALLOWED_EMPTY_SPAN: stream.read(std::span<char>()); break;
			case SWALLOWED_EMPTY_STRING: stream.read(text, 0); break;
			case SWALLOWED_EMPTY_PEEK: stream.peek(bytes, 0); break;
			case SWALLOWED_EMPTY_SKIP: stream.skip(0); break;
			case SWALLOWED_NESTED_FRAME: stream.read(static_cast<Packet*>(this)); break;
			default: CHECK(false); break;
			}
		} catch (InvalidProtocolException&) {
			m_Caught = true;
		}
		if (!m_ConsumeFirst)
			FixedReadPacket::read(stream);
	}

	bool caughtFailure() const { return m_Caught; }

private:
	SwallowedOperation m_Operation;
	bool m_ConsumeFirst;
	bool m_Caught;
};

void AppendFrame(std::vector<unsigned char>& wire, PacketID_t packetID,
	PacketSize_t bodySize, const std::vector<unsigned char>& body)
{
	wire.push_back((unsigned char)(packetID & 0xFF));
	wire.push_back((unsigned char)((packetID >> 8) & 0xFF));
	for (unsigned int i = 0; i < szPacketSize; i++)
		wire.push_back((unsigned char)((bodySize >> (i * 8)) & 0xFF));
	wire.push_back(0); // sequence
	wire.insert(wire.end(), body.begin(), body.end());
}

} // namespace

//----------------------------------------------------------------------
// SocketInputStream contract
//----------------------------------------------------------------------

TEST(SocketInputStream, ZeroLengthReadIsRejected)
{
	StreamFixture f;
	const unsigned char bytes[] = { 0x01, 0x02 };
	f.Preload(bytes, sizeof(bytes));

	bool bThrew = false;
	char out[4];
	try {
		f.m_Stream.read(out, 0);
	} catch (InvalidProtocolException&) {
		bThrew = true;
	}
	CHECK(bThrew);
}

TEST(SocketInputStream, ReadBeyondAvailableThrowsAndConsumesNothing)
{
	StreamFixture f;
	const unsigned char bytes[] = { 0xAA, 0xBB, 0xCC };
	f.Preload(bytes, sizeof(bytes));

	bool bThrew = false;
	char out[8];
	try {
		f.m_Stream.read(out, 4);
	} catch (InsufficientDataException&) {
		bThrew = true;
	}
	CHECK(bThrew);

	// The guard fires before the copy, so the 3 bytes are still there
	// and still readable.
	CHECK_EQ(3, f.m_Stream.length());
	f.m_Stream.read(out, 3);
	CHECK_EQ(0xAA, (unsigned char)out[0]);
	CHECK_EQ(0xBB, (unsigned char)out[1]);
	CHECK_EQ(0xCC, (unsigned char)out[2]);
}

TEST(SocketInputStream, WrappedReadReassemblesAcrossBufferEnd)
{
	StreamFixture f(16);
	const unsigned char bytes[] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };

	// Head at 12 of 16: bytes 12..15 sit at the end, 16..17 wrap to 0..1.
	f.Preload(bytes, sizeof(bytes), 12);
	CHECK_EQ(6, f.m_Stream.length());

	char out[6];
	f.m_Stream.read(out, 6);
	CHECK_EQ(0, memcmp(out, bytes, 6));
	CHECK(f.m_Stream.isEmpty());
}

TEST(SocketInputStream, PacketReadDoesNotCrossDeclaredBodyIntoNextFrame)
{
	StreamFixture f(32);
	std::vector<unsigned char> wire;
	AppendFrame(wire, 1, 1, std::vector<unsigned char>{ 0xA1 });
	const size_t secondFrameOffset = wire.size();
	AppendFrame(wire, 1, 1, std::vector<unsigned char>{ 0xB2 });

	// Start close enough to the end that the malformed body and the following
	// header straddle the ring boundary.
	f.Preload(&wire[0], (unsigned int)wire.size(), 29);

	FixedReadPacket malformed(2);
	bool bThrew = false;
	try {
		f.m_Stream.read(&malformed);
	} catch (InvalidProtocolException&) {
		bThrew = true;
	}
	CHECK(bThrew);

	// The parser consumed its one declared byte, but was refused before it
	// could steal the first byte of the next packet's header.
	CHECK_EQ(wire.size() - secondFrameOffset, f.m_Stream.length());
	char nextHeader[szPacketHeader];
	CHECK(f.m_Stream.peek(nextHeader, sizeof(nextHeader)));
	CHECK_EQ(0, memcmp(nextHeader, &wire[secondFrameOffset], sizeof(nextHeader)));

	FixedReadPacket valid(1);
	f.m_Stream.read(&valid);
	CHECK_EQ(0xB2, valid.getLastByte());
	CHECK(f.m_Stream.isEmpty());
}

TEST(SocketInputStream, PacketReadRejectsAndDiscardsAnUnderConsumedBody)
{
	StreamFixture f(32);
	std::vector<unsigned char> wire;
	AppendFrame(wire, 1, 2, std::vector<unsigned char>{ 0xA1, 0xA2 });
	const size_t secondFrameOffset = wire.size();
	AppendFrame(wire, 1, 1, std::vector<unsigned char>{ 0xB2 });
	f.Preload(&wire[0], (unsigned int)wire.size());

	FixedReadPacket malformed(1);
	bool bThrew = false;
	try {
		f.m_Stream.read(&malformed);
	} catch (InvalidProtocolException&) {
		bThrew = true;
	}
	CHECK(bThrew);

	// Exact consumption is part of accepting a frame. The unread byte belongs
	// to the malformed frame and is discarded; the next header stays aligned.
	CHECK_EQ(wire.size() - secondFrameOffset, f.m_Stream.length());
	FixedReadPacket valid(1);
	f.m_Stream.read(&valid);
	CHECK_EQ(0xB2, valid.getLastByte());
	CHECK(f.m_Stream.isEmpty());
}

TEST(SocketInputStream, SwallowedStreamErrorsStillRejectTheFrame)
{
	for (int operation = 0; operation < SWALLOWED_OPERATION_COUNT; ++operation)
	{
		for (bool consumeFirst : { false, true })
		{
			StreamFixture input(32);
			std::vector<unsigned char> wire;
			AppendFrame(wire, 1, 1, { 0xA1 });
			AppendFrame(wire, 1, 1, { 0xB2 });
			input.Preload(wire.data(), (uint)wire.size(), 29);
			SwallowedFailurePacket malformed((SwallowedOperation)operation, consumeFirst);
			bool rejected = false;
			try { input.m_Stream.read(&malformed); }
			catch (InvalidProtocolException&) { rejected = true; }
			CHECK(malformed.caughtFailure());
			CHECK(rejected);
			CHECK_EQ(szPacketHeader + 1, input.m_Stream.length());
			FixedReadPacket valid(1);
			input.m_Stream.read(&valid);
			CHECK_EQ(0xB2, valid.getLastByte());
			CHECK(input.m_Stream.isEmpty());
		}
	}
}

TEST(SocketInputStream, FragmentedPacketReadConsumesNothingUntilBodyIsComplete)
{
	StreamFixture f(32);
	std::vector<unsigned char> wire;
	AppendFrame(wire, 1, 2, std::vector<unsigned char>{ 0xA1, 0xA2 });

	// A complete header and only the first body byte. The receive loop normally
	// performs this check too, but the packet-reading API must be safe on its own.
	f.Preload(&wire[0], szPacketHeader + 1, 30);
	FixedReadPacket packet(2);
	bool bThrew = false;
	try {
		f.m_Stream.read(&packet);
	} catch (InsufficientDataException&) {
		bThrew = true;
	}
	CHECK(bThrew);
	CHECK_EQ(szPacketHeader + 1, f.m_Stream.length());

	char stillBuffered[szPacketHeader + 1];
	f.m_Stream.read(stillBuffered, (uint)sizeof(stillBuffered));
	CHECK_EQ(0, memcmp(stillBuffered, &wire[0], sizeof(stillBuffered)));
}

TEST(SocketInputStream, FragmentedPacketHeaderConsumesNothing)
{
	StreamFixture f(16);
	std::vector<unsigned char> wire;
	AppendFrame(wire, 1, 1, std::vector<unsigned char>{ 0xA1 });

	// Leave the last header byte for a later fill and wrap the fragment around
	// the end of the ring. A direct packet read must preserve every byte.
	const uint fragmentSize = szPacketHeader - 1;
	f.Preload(&wire[0], fragmentSize, 14);
	FixedReadPacket packet(1);
	bool bThrew = false;
	try {
		f.m_Stream.read(&packet);
	} catch (InsufficientDataException&) {
		bThrew = true;
	}
	CHECK(bThrew);
	CHECK_EQ(fragmentSize, f.m_Stream.length());

	char stillBuffered[szPacketHeader - 1];
	f.m_Stream.read(stillBuffered, (uint)sizeof(stillBuffered));
	CHECK_EQ(0, memcmp(stillBuffered, &wire[0], sizeof(stillBuffered)));
}

TEST(SocketInputStream, CompleteBodyUnderflowIsAProtocolErrorAndNotFragmentation)
{
	StreamFixture f(32);
	std::vector<unsigned char> wire;
	AppendFrame(wire, 1, 1, std::vector<unsigned char>{ 0xA1 });
	AppendFrame(wire, 1, 1, std::vector<unsigned char>{ 0xB2 });
	f.Preload(&wire[0], (unsigned int)wire.size());

	ExplicitUnderflowPacket malformed;
	bool bProtocolError = false;
	try {
		f.m_Stream.read(&malformed);
	} catch (InvalidProtocolException&) {
		bProtocolError = true;
	}
	CHECK(bProtocolError);

	// The explicit underflow cannot be mistaken for a partial transport frame.
	FixedReadPacket valid(1);
	f.m_Stream.read(&valid);
	CHECK_EQ(0xB2, valid.getLastByte());
	CHECK(f.m_Stream.isEmpty());
}

TEST(SocketInputStream, StandardParserExceptionRestoresTheFrameBoundary)
{
	StreamFixture f(32);
	std::vector<unsigned char> wire;
	AppendFrame(wire, 1, 1, std::vector<unsigned char>{ 0xA1 });
	AppendFrame(wire, 1, 1, std::vector<unsigned char>{ 0xB2 });
	f.Preload(&wire[0], (unsigned int)wire.size());

	StandardExceptionPacket malformed;
	bool bThrew = false;
	try {
		f.m_Stream.read(&malformed);
	} catch (const std::runtime_error&) {
		bThrew = true;
	}
	CHECK(bThrew);

	FixedReadPacket valid(1);
	f.m_Stream.read(&valid);
	CHECK_EQ(0xB2, valid.getLastByte());
	CHECK(f.m_Stream.isEmpty());
}

TEST(SocketInputStream, EveryBodyOperationStopsAtTheFrameBoundary)
{
	const BoundaryOperation operations[] = {
		BOUNDARY_STRING_READ,
		BOUNDARY_PEEK,
		BOUNDARY_SKIP
	};

	for (size_t i = 0; i < sizeof(operations) / sizeof(operations[0]); i++)
	{
		StreamFixture f(32);
		std::vector<unsigned char> wire;
		AppendFrame(wire, 1, 1, std::vector<unsigned char>{ 0xA1 });
		const size_t secondFrameOffset = wire.size();
		AppendFrame(wire, 1, 1, std::vector<unsigned char>{ 0xB2 });
		f.Preload(&wire[0], (unsigned int)wire.size(), 29);

		BoundaryOperationPacket malformed(operations[i]);
		bool bThrew = false;
		try {
			f.m_Stream.read(&malformed);
		} catch (InvalidProtocolException&) {
			bThrew = true;
		}
		CHECK(bThrew);
		CHECK_EQ(wire.size() - secondFrameOffset, f.m_Stream.length());

		FixedReadPacket valid(1);
		f.m_Stream.read(&valid);
		CHECK_EQ(0xB2, valid.getLastByte());
		CHECK(f.m_Stream.isEmpty());
	}
}

//----------------------------------------------------------------------
// read(std::string&, len) - the overload the Gpackets chat/guild/name
// parsers feed server-supplied lengths into.
//----------------------------------------------------------------------

TEST(SocketInputStream, StringReadZeroLengthIsRejected)
{
	StreamFixture f;
	const unsigned char bytes[] = { 0x41 };
	f.Preload(bytes, sizeof(bytes));

	bool bThrew = false;
	std::string str;
	try {
		f.m_Stream.read(str, 0);
	} catch (InvalidProtocolException&) {
		bThrew = true;
	}
	CHECK(bThrew);
}

TEST(SocketInputStream, StringReadBeyondAvailableThrowsAndConsumesNothing)
{
	StreamFixture f;
	const unsigned char bytes[] = { 'a', 'b', 'c' };
	f.Preload(bytes, sizeof(bytes));

	bool bThrew = false;
	std::string str;
	try {
		f.m_Stream.read(str, 4);
	} catch (InsufficientDataException&) {
		bThrew = true;
	}
	CHECK(bThrew);

	// Guard fires before the copy: the 3 bytes are still readable.
	CHECK_EQ(3, f.m_Stream.length());
	f.m_Stream.read(str, 3);
	CHECK(str == std::string("abc"));
}

TEST(SocketInputStream, StringReadTruncatesAtEmbeddedNulButConsumesFullLength)
{
	// Contract worth pinning because returned size != consumed size: the
	// string is cut at the first embedded NUL, but m_Head still advances
	// by the full requested length. A parser that frames on str.size()
	// instead of the length it asked for desyncs here.
	StreamFixture f;
	const unsigned char bytes[] = { 'a', 'b', 0x00, 'c', 'd' };
	f.Preload(bytes, sizeof(bytes));

	std::string str;
	f.m_Stream.read(str, 5);
	CHECK_EQ(2, str.size());
	CHECK(str == std::string("ab"));
	CHECK(f.m_Stream.isEmpty());
}

//----------------------------------------------------------------------
// ModifyInfo::read - [shortCount:1][(type:1,value:2)...]
//                    [longCount:1][(type:1,value:4)...]
//----------------------------------------------------------------------

TEST(ModifyInfo, ReadParsesShortAndLongLists)
{
	StreamFixture f;
	const unsigned char bytes[] = {
		0x02,				// shortCount = 2
		0x03, 0x34, 0x12,		// type 3,  value 0x1234 (LE)
		0x17, 0xFF, 0xFF,		// type 23, value 0xFFFF
		0x01,				// longCount = 1
		0x2F, 0xEF, 0xBE, 0xAD, 0xDE,	// type 47, value 0xDEADBEEF (LE)
	};
	f.Preload(bytes, sizeof(bytes));

	TestModifyInfo info;
	info.read(f.m_Stream);

	CHECK_EQ(2, info.getShortCount());
	CHECK_EQ(1, info.getLongCount());

	SHORTDATA shortData;
	info.popShortData(shortData);
	CHECK_EQ(3, shortData.type);
	CHECK_EQ(0x1234, shortData.value);
	info.popShortData(shortData);
	CHECK_EQ(23, shortData.type);
	CHECK_EQ(0xFFFF, shortData.value);

	LONGDATA longData;
	info.popLongData(longData);
	CHECK_EQ(47, longData.type);
	CHECK_EQ(0xDEADBEEFLL, (long long)longData.value);

	// Everything consumed - exactly getPacketSize() bytes.
	CHECK(f.m_Stream.isEmpty());
}

TEST(ModifyInfo, OversizedShortCountHitsTheUnderflowGuard)
{
	StreamFixture f;

	// A hostile count with no data behind it: the first entry's read
	// must throw instead of parsing garbage.
	//
	// Contract note: a thrown read leaves the object INCONSISTENT -
	// m_ShortCount is already 200 over an empty list, and popShortData
	// on it would call front() on that empty list (undefined behavior).
	// A packet whose read threw must be discarded, never popped; every
	// current caller does exactly that.
	const unsigned char bytes[] = { 0xC8 };	// shortCount = 200
	f.Preload(bytes, sizeof(bytes));

	TestModifyInfo info;
	bool bThrew = false;
	try {
		info.read(f.m_Stream);
	} catch (InsufficientDataException&) {
		bThrew = true;
	}
	CHECK(bThrew);
}

//----------------------------------------------------------------------
// GCUpdateInfo lifetime - the packet a truncated login reply leaves
// behind.
//
// GCUpdateInfo::read allocates its sub-objects as it parses, and the
// client-side destructor deletes every one of them. The receive loop
// deletes a packet whose read threw, so every pointer the destructor
// frees must be null from construction, not only after a complete
// read. m_pBloodBibleSign was the one that was not (found when task
// 2.5's wire-layout test first constructed and destroyed every packet):
// a body cut short before the blood-bible section freed whatever the
// heap held.
//----------------------------------------------------------------------

TEST(GCUpdateInfo, FreshPacketHoldsNoSubObjects)
{
	GCUpdateInfo packet;
	CHECK(packet.getPCInfo() == NULL);
	CHECK(packet.getInventoryInfo() == NULL);
	CHECK(packet.getGearInfo() == NULL);
	CHECK(packet.getExtraInfo() == NULL);
	CHECK(packet.getEffectInfo() == NULL);
	CHECK(packet.getRideMotorcycleInfo() == NULL);
	CHECK(packet.getNicknameInfo() == NULL);
	CHECK(packet.getBloodBibleSignInfo() == NULL);
}

TEST(GCUpdateInfo, PacketWhoseReadThrewIsSafeToDestroy)
{
	StreamFixture f;

	// An empty body: the very first byte (the PC type) underflows, so
	// nothing past the constructor has run when the packet is deleted.
	GCUpdateInfo* pPacket = new GCUpdateInfo();
	bool bThrew = false;
	try {
		pPacket->read(f.m_Stream);
	} catch (InsufficientDataException&) {
		bThrew = true;
	}
	CHECK(bThrew);
	delete pPacket;	// the receive loop's path; ASan aborts on a bad free
}

//----------------------------------------------------------------------
// InventoryInfo::read - [listNum:1][InventorySlotInfo...]
//----------------------------------------------------------------------

TEST(InventoryInfo, ZeroCountParsesToEmptyList)
{
	StreamFixture f;
	const unsigned char bytes[] = { 0x00 };
	f.Preload(bytes, sizeof(bytes));

	InventoryInfo info;
	info.read(f.m_Stream);

	CHECK_EQ(0, info.getListNum());
	CHECK(f.m_Stream.isEmpty());
}

TEST(InventoryInfo, OversizedCountHitsTheUnderflowGuard)
{
	StreamFixture f;

	// 60 is the count getMaxSize() budgets for; with no slot data the
	// nested InventorySlotInfo read must hit the underflow guard.
	const unsigned char bytes[] = { 0x3C };
	f.Preload(bytes, sizeof(bytes));

	InventoryInfo info;
	bool bThrew = false;
	try {
		info.read(f.m_Stream);
	} catch (InsufficientDataException&) {
		bThrew = true;
	}
	CHECK(bThrew);
}

//----------------------------------------------------------------------
// The chat length guards the handlers' buffers rest on
//----------------------------------------------------------------------
//
// Every one of these packets carries a BYTE length and then that many
// bytes of server-supplied text, and each read() decides for itself how
// long is too long. The handlers copy the result into a fixed buffer,
// so these numbers and those buffer sizes have to be read together -
// and until the R3 pass they were not. GCNPCSayDynamic accepts 2048
// where its handler copied into char[256], which is a 1792-byte stack
// overflow a server could send at will; the copy is bounded now
// (docs/reviews/code-health-review-2026-08-29.md, the packet-tree copy pass).
//
// Pinned here because the packets are library code and the handlers are
// not: if someone widens one of these limits, this is the only place
// that can notice.
//----------------------------------------------------------------------
namespace {

// A GCGlobalChat body, in the order read() takes it: a uint colour,
// a BYTE length, that many bytes of text, then a Race_t.
void	PreloadGlobalChat(StreamFixture& f, unsigned int declaredLength, unsigned int textBytes)
{
	std::vector<unsigned char> body;

	for (unsigned int i = 0; i < sizeof(uint); ++i)
		body.push_back(0);			// colour

	body.push_back((unsigned char)declaredLength);

	for (unsigned int i = 0; i < textBytes; ++i)
		body.push_back('A');

	for (unsigned int i = 0; i < sizeof(Race_t); ++i)
		body.push_back(0);			// race

	f.Preload(&body[0], (unsigned int)body.size());
}

bool	GlobalChatReadThrows(unsigned int declaredLength, unsigned int textBytes)
{
	StreamFixture f(4096);
	PreloadGlobalChat(f, declaredLength, textBytes);

	GCGlobalChat packet;

	try {
		packet.read(f.m_Stream);
	} catch (ProtocolException&) {
		return true;
	} catch (Error&) {
		return true;
	}

	return false;
}

} // namespace

TEST(GCGlobalChat, RejectsAZeroLengthAndAnOverLongMessage)
{
	// Zero is rejected outright.
	CHECK_EQ(true, GlobalChatReadThrows(0, 0));

	// 128 is the limit the handler's char[256] is sized against, so it
	// has to be accepted...
	CHECK_EQ(false, GlobalChatReadThrows(128, 128));

	// ...and 129 refused. If this ever stops throwing, the buffer in
	// GCGlobalChatHandler is the thing to look at.
	CHECK_EQ(true, GlobalChatReadThrows(129, 129));
	CHECK_EQ(true, GlobalChatReadThrows(255, 255));
}

TEST(GCGlobalChat, KeepsTheWholeMessageItAccepted)
{
	StreamFixture f(4096);
	PreloadGlobalChat(f, 128, 128);

	GCGlobalChat packet;
	packet.read(f.m_Stream);

	// Truncating here rather than at the copy would hide the defect the
	// R3 pass fixed, so the packet has to hand over all 128 bytes.
	CHECK_EQ(128, (int)packet.getMessage().size());
}

//----------------------------------------------------------------------
// The slot id the PCS handlers rest a guard on
//----------------------------------------------------------------------
//
// GCPhoneConnected, GCPhoneDisconnected, GCPhoneSay and GCRing all carry
// a SlotID_t - a BYTE - and index UserInformation's PCSUserName and
// OtherPCSNumber with it. Those arrays hold MAX_PCS_SLOT (3) entries,
// and read() bounds the NAME but not the SLOT, so until the index pass
// a server could assign an MString hundreds of bytes past the end of
// g_pUserInformation: MString::operator= reads m_pString out of
// whatever is there and delete[]s it.
//
// The four handlers guard it now. This pins the fact that makes those
// guards load-bearing rather than belt-and-braces: the wire accepts any
// slot a BYTE can hold. If someone later adds a limit to read(), this
// test is what tells them the handler guards changed meaning.
//----------------------------------------------------------------------
namespace {

bool	PhoneConnectedAcceptsSlot(unsigned int slot)
{
	StreamFixture f(256);

	std::vector<unsigned char> body;

	for (unsigned int i = 0; i < sizeof(PhoneNumber_t); ++i)
		body.push_back(0);			// phone number

	body.push_back((unsigned char)slot);		// SlotID_t

	body.push_back(4);				// name length
	for (int i = 0; i < 4; ++i)
		body.push_back('n');

	f.Preload(&body[0], (unsigned int)body.size());

	GCPhoneConnected packet;

	try {
		packet.read(f.m_Stream);
	} catch (ProtocolException&) {
		return false;
	} catch (Error&) {
		return false;
	}

	return (unsigned int)packet.getSlotID() == slot;
}

} // namespace

TEST(GCPhoneConnected, AcceptsAnySlotIdAByteCanHold)
{
	// The three that address a real array...
	CHECK_EQ(true, PhoneConnectedAcceptsSlot(0));
	CHECK_EQ(true, PhoneConnectedAcceptsSlot(MAX_PCS_SLOT - 1));

	// ...and the ones that do not. Every one of these reaches the
	// handler, which is why the handler is where the guard lives.
	CHECK_EQ(true, PhoneConnectedAcceptsSlot(MAX_PCS_SLOT));
	CHECK_EQ(true, PhoneConnectedAcceptsSlot(200));
	CHECK_EQ(true, PhoneConnectedAcceptsSlot(255));
}
