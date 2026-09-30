//----------------------------------------------------------------------
// client_stream_reader.h
//----------------------------------------------------------------------
//
// The receive path the client's stream fuzz targets share: the bytes a
// server sends one ClientPlayer over TCP, read the way
// ClientPlayer::processCommand (Client/Packet/ClientPlayer.cpp) reads
// them, minus the handler dispatch, which is executable-side. The
// client talks to the login server and to a game server through the
// same ClientPlayer, so both targets read through this one loop and
// differ only in the player status the validator is asked about:
//
//   fuzz_client_stream.cpp        the game connection, CPS_NORMAL
//   fuzz_client_login_stream.cpp  the login connection, each of the
//                                 statuses a player holds there
//
// Input: [encrypt code byte][raw stream]. The code is what
// SocketEncryptInputStream::setEncryptCode holds; the stream is any
// number of frames, each a 7-byte header (id u16, body size u32,
// sequence u8, little-endian) and its body. At most 64 frames are read
// from one input, so a run of empty frames cannot turn one input into
// a slow one.
//
// The loop mirrors processCommand's gates in order, and stops where the
// production loop would throw (which disconnects the session) or break
// (which waits for more bytes):
//
//   ClientPlayer.cpp:136  peek the header; a partial one waits
//   ClientPlayer.cpp:173  id >= Packet::PACKET_MAX is a protocol error
//   ClientPlayer.cpp:193  the validator for the player's status: an id
//                         the status does not accept is a protocol
//                         error ("invalid packet ORDER"), and an
//                         IgnorePacketException (:221, thrown by a
//                         PIST_IGNORE_EXCEPT set) is caught and the
//                         packet still read. The read-without-executing
//                         branch for CPS_WAITING_FOR_GC_RECONNECT_LOGIN
//                         (:198) is not mirrored: neither target reads
//                         in that status.
//   ClientPlayer.cpp:234  a size above the factory's maximum
//   ClientPlayer.cpp:243  a body not yet fully buffered waits
//   ClientPlayer.cpp:253  createPacket
//   ClientPlayer.cpp:258  SocketInputStream::read(Packet*), the framed
//                         read that bounds the parser to the declared
//                         body and throws when it reads less or more
//
// Keep it in step with processCommand: a gate added there and not here
// makes these targets report inputs production refuses. No handler
// runs, so the status cannot change in the middle of a stream here,
// where in production a handler may change it between two frames.
//
// Each input is read twice, from a fresh stream each time: once with
// the stream at the start of the ring, and once with the ring's wrap
// point in the middle of the stream, so the reversed-order branches of
// SocketInputStream's peek, read and skip (the ones production takes
// once its 32 KB ring has wrapped) see the same bytes. The fuzzer
// moves the wrap point relative to the frames by changing the input's
// length. A stream shorter than two bytes gets only the first pass.
//
// Outcomes: a Throwable (the only type processCommand's caller,
// UpdateSocketInput in Client/GameMain.cpp, catches) is a rejected
// input. Anything else, a std::exception included, escapes, since in
// production it would escape the game loop and end the client through
// std::terminate: libFuzzer and the replay driver report it as a
// crash. A crash, a sanitizer report or an abort is a finding.
// PacketAssert.h's Assert throws AssertionError in builds without
// NDEBUG, which is a rejection too; set DE_FUZZ_ABORT_ON_ASSERT to make
// it abort instead, to see which Asserts hostile input reaches (a
// Release build compiles them out). A failed Assert appends to
// assertion_failed.log in the current directory, so run a fuzzer from
// a scratch directory.
//
// Header-only and free of the test framework, so each target stays
// one translation unit that links packetwire and nothing else.
//
//----------------------------------------------------------------------

#ifndef CLIENT_STREAM_READER_H
#define CLIENT_STREAM_READER_H

#include "packet_stream_preload.h"

#include "CrtCompat.h"
#include "Exception.h"
#include "Packet.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "PlayerStatus.h"
#include "Socket.h"
#include "SocketEncryptInputStream.h"
#include "SocketImpl.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace ClientStreamFuzz {

// ClientPlayer's input ring (ClientPlayer.cpp:49).
inline const unsigned int	kInputBufferLen = 32768;

// More frames than any one input needs to reach a parser; bounds the
// time a stream of empty frames can take.
inline const int		kMaxFrames = 64;

// Set from DE_FUZZ_ABORT_ON_ASSERT by Initialize().
inline bool			g_bAbortOnAssert = false;

//----------------------------------------------------------------------
// Reads frames until processCommand, with the player in `status`,
// would stop. Throws what it throws.
//----------------------------------------------------------------------
inline void	ReadFrames(SocketInputStream& stream, PlayerStatus status)
{
	for (int frames = 0; frames < kMaxFrames; frames++)
	{
		char header[szPacketHeader];
		if (!stream.peek(header, szPacketHeader))
			return;

		PacketID_t	packetID = 0;
		PacketSize_t	packetSize = 0;
		std::memcpy(&packetID, &header[0], szPacketID);
		std::memcpy(&packetSize, &header[szPacketID], szPacketSize);

		if (packetID >= Packet::PACKET_MAX)
			return;

		try {
			if (!g_pPacketValidator->isValidPacketID(status, packetID))
				return;
		} catch (IgnorePacketException&) {
			// Read and not executed, as production does.
		}

		if (packetSize > g_pPacketFactoryManager->getPacketMaxSize(packetID))
			return;

		if (stream.length() < szPacketHeader + packetSize)
			return;

		std::unique_ptr<Packet> pPacket(g_pPacketFactoryManager->createPacket(packetID));
		stream.read(pPacket.get());
	}
}

//----------------------------------------------------------------------
// Loads the stream into a fresh ring starting at `head` and reads it.
// Catches only what production's loop catches; see the file header.
//----------------------------------------------------------------------
inline void	ReadAt(uchar code, const std::uint8_t* bytes, unsigned int len,
		       unsigned int head, PlayerStatus status)
{
	Socket				socket(new SocketImpl());
	SocketEncryptInputStream	stream(&socket, kInputBufferLen);
	stream.setEncryptCode(code);
	if (!SocketInputStreamTestAccess::Preload(stream, bytes, len, head))
		return;

	try {
		ReadFrames(stream, status);
	} catch (AssertionError&) {
		if (g_bAbortOnAssert)
			std::abort();
	} catch (Throwable&) {
	}
}

//----------------------------------------------------------------------
// Reads one fuzz input, [code][stream], with the player in `status`:
// from the start of the ring, then across its wrap point.
//----------------------------------------------------------------------
inline void	ReadInput(const std::uint8_t* data, std::size_t size, PlayerStatus status)
{
	if (size < 1)
		return;

	const uchar	code = data[0];
	const std::size_t	streamLen = size - 1;

	// The ring holds one byte less than its size.
	if (streamLen >= kInputBufferLen)
		return;

	ReadAt(code, data + 1, (unsigned int)streamLen, 0, status);

	// The wrap point streamLen / 2 bytes into the stream.
	const unsigned int	half = (unsigned int)(streamLen / 2);
	if (half > 0)
		ReadAt(code, data + 1, (unsigned int)streamLen, kInputBufferLen - half, status);
}

//----------------------------------------------------------------------
// The two tables GameInit.cpp builds before any connection exists.
//----------------------------------------------------------------------
inline void	Initialize()
{
	g_pPacketFactoryManager = new PacketFactoryManager();
	g_pPacketFactoryManager->init();
	g_pPacketValidator = new PacketValidator();
	g_pPacketValidator->init();

	g_bAbortOnAssert = Basic::GetEnvironment("DE_FUZZ_ABORT_ON_ASSERT").has_value();
}

} // namespace ClientStreamFuzz

#endif // CLIENT_STREAM_READER_H
