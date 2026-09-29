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
#include "Gpackets/GCModifyNickname.h"

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

// The fuzzer's input (tests/fuzz/regressions/client_stream/
// GCModifyNickname-type-0xff.hex) without its encrypt code byte: one
// GCModifyNickname frame of 30 bytes whose nickname type is 0xff.
TEST(PacketFuzzFindings, ModifyNicknameFrameWithTypeFfIsRefused)
{
	std::vector<unsigned char> frame = {
		0x15, 0x01,			// id 0x0115, GCModifyNickname
		0x1e, 0x00, 0x00, 0x00,		// body size 30
		0x00,				// sequence
		0x00, 0x00, 0x00, 0xff,		// object id
		0xff, 0xff,			// nickname id
		0xff,				// nickname type
		0xff, 0xff,			// unread: the type ended the parse
	};
	frame.resize(7 + 30, 0x00);

	GCModifyNickname packet;
	CHECK(FramedReadIsRefused(packet, frame));
}
