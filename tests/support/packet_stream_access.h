//----------------------------------------------------------------------
// packet_stream_access.h
//----------------------------------------------------------------------
//
// Test-only seams into the packetwire socket streams, shared by every
// test that pushes bytes through them (test_packetwire_parsers.cpp,
// test_packet_goldens.cpp).
//
// In production the streams sit on a connected socket: fill() is the
// only writer of the input ring and flush() the only reader of the
// output ring. A unit test has no peer, so the two access classes -
// befriended by the streams with a one-line, unconditional friend
// declaration that changes neither layout nor behavior - preload the
// input ring and copy the output ring out. The Socket handed to a
// stream is never used (the constructors only assert it is non-null),
// and closing a never-created socket is a no-op; still call
// EnsureSocketsInitialised() first so a test that does create one has
// Winsock started.
//
// Include this from a TU compiled with the packetwire defines
// (tests/CMakeLists.txt, PACKETWIRE_TEST_DEFINITIONS): the stream and
// Packet class definitions must be identical to the library's.
//
//----------------------------------------------------------------------

#ifndef PACKET_STREAM_ACCESS_H
#define PACKET_STREAM_ACCESS_H

#include "test_framework.h"

#include "packet_stream_preload.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"

#include <vector>

#ifdef _WIN32
#include <winsock.h>
#endif

//----------------------------------------------------------------------
// SocketInputStreamTestAccess is defined in packet_stream_preload.h,
// shared with the fuzz harnesses, which cannot use CHECK. In unit_tests
// packet_stream_access.cpp installs its preload hook, which CHECKs that
// the bytes fit, so a test that preloads more than the ring holds fails
// instead of reading an empty stream. A new test binary that includes
// this header compiles that file too.
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Befriended by SocketOutputStream: copies out what write() has put in
// the ring since the last flush, in order, across the wrap point.
//----------------------------------------------------------------------
class SocketOutputStreamTestAccess
{
public:
	static std::vector<unsigned char> Bytes(const SocketOutputStream& stream)
	{
		std::vector<unsigned char> out;
		for (unsigned int i = stream.m_Head; i != stream.m_Tail;
		     i = (i + 1) % stream.m_BufferLen)
			out.push_back((unsigned char)stream.m_Buffer[i]);
		return out;
	}

	// Drops the first `count` live bytes, as flush() does after a
	// successful send of that many.
	static void Consume(SocketOutputStream& stream, unsigned int count)
	{
		CHECK(count <= stream.length());
		stream.m_Head = (stream.m_Head + count) % stream.m_BufferLen;
	}

	// The ring indices, for the one thing Bytes() cannot say: where in
	// the buffer the live run sits.
	static unsigned int Head(const SocketOutputStream& stream)
	{
		return stream.m_Head;
	}

	static unsigned int Tail(const SocketOutputStream& stream)
	{
		return stream.m_Tail;
	}
};

//----------------------------------------------------------------------
// One-time Winsock init (see the file header). MAKEWORD(2,2) matches
// what any modern Windows provides; on other platforms this is a no-op.
//----------------------------------------------------------------------
inline void EnsureSocketsInitialised()
{
#ifdef _WIN32
	static bool bDone = false;
	if (!bDone)
	{
		WSADATA data;
		// Fail visibly here rather than as an Error thrown out of
		// ~SocketImpl during fixture teardown (which would terminate the
		// process with no pointer at the cause). CHECK only records into
		// the framework's counters, so it is safe during fixture setup.
		CHECK(WSAStartup(MAKEWORD(2, 2), &data) == 0);
		bDone = true;
	}
#endif
}

#endif // PACKET_STREAM_ACCESS_H
