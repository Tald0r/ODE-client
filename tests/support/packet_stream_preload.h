//----------------------------------------------------------------------
// packet_stream_preload.h
//----------------------------------------------------------------------
//
// The one definition of SocketInputStreamTestAccess, the class
// SocketInputStream befriends (SocketInputStream.h) so a program with
// no connected peer can put bytes in the input ring. fill() is the
// only production writer of that ring and it needs a live socket.
//
// Two kinds of program use it, which is why it lives apart from the
// unit tests' packet_stream_access.h:
//
//   - the unit tests, through packet_stream_access.h, whose
//     packet_stream_access.cpp installs a hook that CHECKs every
//     preload fits;
//   - the fuzz harnesses under tests/fuzz, which link neither the test
//     framework nor anything that names it, and read Preload's result.
//
// The class must be defined exactly once per program and identically
// in every translation unit, so nothing here depends on the test
// framework or on a macro a consumer might set.
//
// Include this from a translation unit compiled with the packetwire
// definitions (they arrive through packetwire's link interface): the
// stream class definition must be identical to the library's.
//
//----------------------------------------------------------------------

#ifndef PACKET_STREAM_PRELOAD_H
#define PACKET_STREAM_PRELOAD_H

#include "SocketInputStream.h"

class SocketInputStreamTestAccess
{
public:
	// Called by every Preload with the requested length and the ring's
	// size, before the length is tested. Null by default;
	// packet_stream_access.cpp points it at the test framework, whose
	// CHECK(len < bufferLen) fails the test on an oversized preload
	// instead of leaving an empty ring behind a passing one.
	using FitHook = void (*)(unsigned int len, unsigned int bufferLen);
	static inline FitHook s_pOnPreload = nullptr;

	//------------------------------------------------------------------
	// Writes `len` bytes into the private ring buffer, starting at
	// `head` so a read can be made to reassemble across the wrap point.
	// Returns false and loads nothing when the bytes do not fit.
	//------------------------------------------------------------------
	static bool Preload(SocketInputStream& stream, const unsigned char* data,
			    unsigned int len, unsigned int head = 0)
	{
		if (s_pOnPreload != nullptr)
			s_pOnPreload(len, stream.m_BufferLen);

		// The ring keeps one slot empty to distinguish full from empty.
		// An oversized preload would wrap onto its own head and read
		// as an EMPTY ring, so it is refused here, not just recorded.
		if (len >= stream.m_BufferLen)
			return false;

		for (unsigned int i = 0; i < len; i++)
			stream.m_Buffer[(head + i) % stream.m_BufferLen] = (char)data[i];

		stream.m_Head = head;
		stream.m_Tail = (head + len) % stream.m_BufferLen;
		return true;
	}
};

#endif // PACKET_STREAM_PRELOAD_H
