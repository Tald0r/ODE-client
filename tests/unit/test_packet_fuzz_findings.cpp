//----------------------------------------------------------------------
// test_packet_fuzz_findings.cpp
//----------------------------------------------------------------------
//
// Parsers the client packet-read fuzz target (tests/fuzz/
// fuzz_client_stream.cpp) crashed, each pinned with the bytes it
// crashed on. The same inputs, as the fuzzer wrote them, replay in the
// fuzz_replay_client_stream ctest from tests/fuzz/regressions/
// client_stream; this file states the contract the fix established:
// the hostile value is refused with InvalidProtocolException, the
// exception every receive loop treats as a protocol violation.
//
// Compiled with the packetwire defines (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "Exception.h"
#include "NicknameInfo.h"
#include "Packet.h"
#include "Socket.h"
#include "SocketEncryptInputStream.h"
#include "SocketImpl.h"
#include "StoreInfo.h"
#include "Gpackets/GCModifyNickname.h"
#include "Gpackets/GCMyStoreInfo.h"
#include "Gpackets/GCOtherStoreInfo.h"

#include <vector>

namespace {

//----------------------------------------------------------------------
// An input stream over a never-used socket (packet_stream_access.h), at
// encrypt code 0, as the fuzz inputs below were found.
//----------------------------------------------------------------------
struct FindingInFixture
{
	Socket				m_Socket;
	SocketEncryptInputStream	m_Stream;

	explicit FindingInFixture(const std::vector<unsigned char>& bytes)
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, 4096)
	{
		m_Stream.setEncryptCode(0);
		SocketInputStreamTestAccess::Preload(m_Stream,
			bytes.empty() ? NULL : &bytes[0], (unsigned int)bytes.size());
	}
};

// Reads one framed packet; true when read() refused it as a protocol
// violation.
bool	FramedReadIsRefused(Packet& packet, const std::vector<unsigned char>& frame)
{
	FindingInFixture f(frame);
	bool bRefused = false;
	try {
		f.m_Stream.read(&packet);
	} catch (InvalidProtocolException&) {
		bRefused = true;
	}
	// The refused frame is discarded whole, as a completed one is.
	CHECK_EQ(0u, f.m_Stream.length());
	return bRefused;
}

// NicknameInfo's body: id (WORD, little-endian) and type, then the
// type's own fields.
bool	NicknameReadIsRefused(const std::vector<unsigned char>& body)
{
	FindingInFixture f(body);
	NicknameInfo info;
	try {
		info.read(f.m_Stream);
	} catch (InvalidProtocolException&) {
		return true;
	}
	return false;
}

} // namespace

//----------------------------------------------------------------------
// NicknameInfo::read ended its type switch in a libc assert(false),
// which aborts the client in every build without NDEBUG, on a type byte
// any GCModifyNickname, GCAddSlayer, GCAddVampire, GCAddOusters,
// GCAddNickname, GCNicknameList or GCUpdateInfo carries.
//----------------------------------------------------------------------
TEST(PacketFuzzFindings, NicknameTypePastTheLastIsRefused)
{
	CHECK(NicknameReadIsRefused({ 0x34, 0x12, 6 }));
	CHECK(NicknameReadIsRefused({ 0x34, 0x12, 0x7f }));
	CHECK(NicknameReadIsRefused({ 0x34, 0x12, 0xff }));
}

TEST(PacketFuzzFindings, EveryNicknameTypeInRangeStillReads)
{
	// NICK_NONE: nothing follows.
	CHECK(!NicknameReadIsRefused({ 0x34, 0x12, NicknameInfo::NICK_NONE }));
	// The three index types: a WORD follows.
	CHECK(!NicknameReadIsRefused({ 0x34, 0x12, NicknameInfo::NICK_BUILT_IN, 7, 0 }));
	CHECK(!NicknameReadIsRefused({ 0x34, 0x12, NicknameInfo::NICK_QUEST, 7, 0 }));
	CHECK(!NicknameReadIsRefused({ 0x34, 0x12, NicknameInfo::NICK_FORCED, 7, 0 }));
	// The two string types: a length byte, then the string.
	CHECK(!NicknameReadIsRefused({ 0x34, 0x12, NicknameInfo::NICK_CUSTOM_FORCED, 2, 'h', 'i' }));
	CHECK(!NicknameReadIsRefused({ 0x34, 0x12, NicknameInfo::NICK_CUSTOM, 0 }));
}

// The first frame of the fuzzer's input (tests/fuzz/regressions/
// client_stream/GCModifyNickname-type-0xff.hex), byte for byte, without
// the encrypt code byte before it and the 8 zero bytes after it: one
// GCModifyNickname frame of 30 bytes whose nickname type is 0xff.
TEST(PacketFuzzFindings, ModifyNicknameFrameWithTypeFfIsRefused)
{
	std::vector<unsigned char> frame = {
		0x15, 0x01,			// id 0x0115, GCModifyNickname
		0x1e, 0x00, 0x00, 0x00,		// body size 30
		0x00,				// sequence
		0x00, 0x00, 0xff, 0xff,		// object id
		0xff, 0xff,			// nickname id
		0xff,				// nickname type
		0xff, 0xff,			// unread: the type ended the parse
	};
	frame.resize(7 + 30, 0x00);

	GCModifyNickname packet;
	CHECK(FramedReadIsRefused(packet, frame));
}

//----------------------------------------------------------------------
// StoreInfo::read took an item count from a wire byte (up to 255) and
// read that many items into m_Items, which the constructor sizes to
// MAX_ITEM_NUM (20): a count of 21 or more wrote StoreItemInfo fields
// past the end of the vector's heap block. GCMyStoreInfo and
// GCOtherStoreInfo carry a StoreInfo.
//----------------------------------------------------------------------
namespace {

// A StoreInfo body as read(iStream, toOther) takes it: open, an empty
// sign, the count, then `present` items that are all absent (one zero
// byte each).
std::vector<unsigned char>	StoreBody(unsigned char count, unsigned int present)
{
	std::vector<unsigned char> body = { 1, 0, count };
	body.resize(body.size() + present, 0x00);
	return body;
}

// read()s `body` into a fresh StoreInfo. True when refused; `unread` is
// what the stream still held afterwards.
bool	StoreReadIsRefused(const std::vector<unsigned char>& body, bool toOther,
			   unsigned int& unread)
{
	FindingInFixture f(body);
	StoreInfo info;
	bool bRefused = false;
	try {
		info.read(f.m_Stream, toOther);
	} catch (InvalidProtocolException&) {
		bRefused = true;
	}
	unread = f.m_Stream.length();
	return bRefused;
}

} // namespace

TEST(PacketFuzzFindings, StoreItemCountPastTheVectorIsRefusedBeforeAnyItem)
{
	const unsigned char counts[] = { (unsigned char)(MAX_ITEM_NUM + 1), 0x80, 0xff };
	for (unsigned char count : counts)
	{
		for (bool toOther : { false, true })
		{
			// Enough absent items that the stream never runs out first.
			unsigned int unread = 0;
			CHECK(StoreReadIsRefused(StoreBody(count, count), toOther, unread));
			// Refused at the count byte: not one item was read.
			CHECK_EQ((unsigned int)count, unread);
		}
	}
}

TEST(PacketFuzzFindings, StoreItemCountUpToTheVectorStillReads)
{
	const unsigned char counts[] = { 0, 1, (unsigned char)MAX_ITEM_NUM };
	for (unsigned char count : counts)
	{
		unsigned int unread = 1;
		CHECK(!StoreReadIsRefused(StoreBody(count, count), false, unread));
		CHECK_EQ(0u, unread);
	}
}

// The fuzzer's input (tests/fuzz/regressions/client_stream/
// GCMyStoreInfo-item-count-0xa9.hex) without its encrypt code byte:
// one GCMyStoreInfo frame of 64 bytes, count 169, one item present.
TEST(PacketFuzzFindings, MyStoreInfoFrameWithCountA9IsRefused)
{
	std::vector<unsigned char> frame = {
		0x1f, 0x01,			// id 0x011f, GCMyStoreInfo
		0x40, 0x00, 0x00, 0x00,		// body size 64
		0x00,				// sequence
		0x00,				// open UI
		0x00,				// open
		0x00,				// sign length
		0xa9,				// item count 169
		0x5a, 0x53, 0x6b, 0x01,		// item 0 present, then its fields
	};
	frame.resize(7 + 64, 0x00);

	GCMyStoreInfo packet;
	CHECK(FramedReadIsRefused(packet, frame));
}

// The same count through GCOtherStoreInfo, the other packet that
// carries a StoreInfo: an open store with 21 absent items.
TEST(PacketFuzzFindings, OtherStoreInfoFrameWithCount21IsRefused)
{
	std::vector<unsigned char> body = {
		0x04, 0x03, 0x02, 0x01,		// object id
		0x01,				// requested
	};
	const std::vector<unsigned char> store =
		StoreBody((unsigned char)(MAX_ITEM_NUM + 1), MAX_ITEM_NUM + 1);
	body.insert(body.end(), store.begin(), store.end());

	std::vector<unsigned char> frame = {
		0x2d, 0x01,			// id 0x012d, GCOtherStoreInfo
		(unsigned char)body.size(), 0x00, 0x00, 0x00,
		0x00,				// sequence
	};
	frame.insert(frame.end(), body.begin(), body.end());

	GCOtherStoreInfo packet;
	CHECK(FramedReadIsRefused(packet, frame));
}
