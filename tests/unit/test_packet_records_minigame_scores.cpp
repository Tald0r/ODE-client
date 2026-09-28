//----------------------------------------------------------------------
// test_packet_records_minigame_scores.cpp
//----------------------------------------------------------------------
//
// GCMiniGameScores::write() and getPacketSize() against each other and
// against read(): a score table of at most 10 entries, each a
// BYTE-length name of at most 20 bytes and a WORD score. The same
// bounds as the server's copy and as the factory's max size
// (3 + (2 + 1 + 20) * 10 = 233).
//
// Compiled with the packetwire defines (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "Exception.h"
#include "Packet.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketEncryptInputStream.h"
#include "SocketEncryptOutputStream.h"

#include "Gpackets/GCMiniGameScores.h"

#include <string>
#include <vector>

namespace {

//----------------------------------------------------------------------
// Streams over a never-used socket (see tests/support/packet_stream_access.h),
// at encryption code 0 so the bytes in the ring are the plain body.
//----------------------------------------------------------------------
struct ScoresOutFixture
{
	Socket				m_Socket;
	SocketEncryptOutputStream	m_Stream;

	ScoresOutFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket)
	{
		m_Stream.setEncryptCode(0);
	}
};

struct ScoresInFixture
{
	Socket				m_Socket;
	SocketEncryptInputStream	m_Stream;

	ScoresInFixture()
	: m_Socket((EnsureSocketsInitialised(), new SocketImpl())),
	  m_Stream(&m_Socket, 4096)
	{
		m_Stream.setEncryptCode(0);
	}
};

} // namespace

//----------------------------------------------------------------------
// Names of different lengths: getPacketSize() counts each entry's own
// name, not the first one's.
//----------------------------------------------------------------------
TEST(PacketRecordsMiniGameScores, PacketSizeCountsEveryNameLength)
{
	GCMiniGameScores	packet;
	packet.setGameType(GAME_NEMO);
	packet.setLevel(2);
	packet.addScore("a", 1);
	packet.addScore("bbbbb", 2);

	CHECK_EQ(3 + (1 + 1 + 2) + (1 + 5 + 2), (long long)packet.getPacketSize());

	ScoresOutFixture	out;
	packet.write(out.m_Stream);

	CHECK_EQ((long long)packet.getPacketSize(), (long long)out.m_Stream.size());
}

//----------------------------------------------------------------------
// More than 10 scores go out as the first 10. 256 is where a count
// narrowed to a byte before the clamp wraps to 0.
//----------------------------------------------------------------------
TEST(PacketRecordsMiniGameScores, OverfullTableWritesTheFirstTen)
{
	GCMiniGameScores	src;
	src.setGameType(GAME_NEMO);
	src.setLevel(2);
	for (int i = 0; i < 256; i++)
		src.addScore("x", (WORD)i);

	ScoresOutFixture	out;
	src.write(out.m_Stream);

	const std::vector<unsigned char> body =
		SocketOutputStreamTestAccess::Bytes(out.m_Stream);

	CHECK_EQ(3 + 10 * (1 + 1 + 2), (long long)body.size());
	CHECK_EQ((long long)src.getPacketSize(), (long long)body.size());
	CHECK(body.size() > 2 && body[2] == 10);

	GCMiniGameScores	dst;
	ScoresInFixture		in;

	SocketInputStreamTestAccess::Preload(in.m_Stream,
		body.empty() ? NULL : &body[0], (unsigned int)body.size());

	try {
		dst.read(in.m_Stream);
	} catch (...) {
		CHECK(false);
	}

	CHECK_EQ(10, (long long)dst.getSize());
	CHECK_EQ(0, (long long)in.m_Stream.length());
}

//----------------------------------------------------------------------
// A name of exactly 20 bytes goes out; 21 is refused once GameType,
// Level and the count are in the ring, as the server's writeString
// refuses it.
//----------------------------------------------------------------------
TEST(PacketRecordsMiniGameScores, NameLengthIsCappedAtTwenty)
{
	{
		GCMiniGameScores	packet;
		packet.setGameType(GAME_NEMO);
		packet.setLevel(2);
		packet.addScore(std::string(20, 'n'), 7);

		ScoresOutFixture	out;
		packet.write(out.m_Stream);

		CHECK_EQ((long long)packet.getPacketSize(), (long long)out.m_Stream.size());
	}

	const size_t	lengths[] = { 21, 255, 256, 257 };

	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++)
	{
		GCMiniGameScores	packet;
		packet.setGameType(GAME_NEMO);
		packet.setLevel(2);
		packet.addScore(std::string(lengths[i], 'n'), 7);

		ScoresOutFixture	out;
		bool			bThrew = false;

		try {
			packet.write(out.m_Stream);
		} catch (InvalidProtocolException&) {
			bThrew = true;
		}

		CHECK(bThrew);
		CHECK_EQ(3, (long long)out.m_Stream.size());
	}
}
