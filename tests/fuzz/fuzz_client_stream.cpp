//----------------------------------------------------------------------
// fuzz_client_stream.cpp
//----------------------------------------------------------------------
//
// Fuzz target: the bytes a game server sends the client over TCP, read
// the way ClientPlayer::processCommand (Client/Packet/ClientPlayer.cpp)
// reads them, minus the handler dispatch, which is executable-side.
//
// Input: [encrypt code byte][raw stream]. The code is what the session
// hands SocketEncryptInputStream::setEncryptCode after login; the
// stream is any number of frames, each a 7-byte header (id u16, body
// size u32, sequence u8, little-endian) and its body. At most 64 frames
// are read from one input, so a run of empty frames cannot turn one
// input into a slow one.
//
// The loop mirrors processCommand's gates in order, and stops where the
// production loop would throw (which disconnects the session) or break
// (which waits for more bytes):
//
//   ClientPlayer.cpp:136  peek the header; a partial one waits
//   ClientPlayer.cpp:173  id >= Packet::PACKET_MAX is a protocol error
//   ClientPlayer.cpp:193  the validator; CPS_NORMAL is PIST_ANY, so no
//                         id is refused there, but an IgnorePacketException
//                         (:221) is still caught and the packet still read
//   ClientPlayer.cpp:234  a size above the factory's maximum
//   ClientPlayer.cpp:243  a body not yet fully buffered waits
//   ClientPlayer.cpp:253  createPacket
//   ClientPlayer.cpp:258  SocketInputStream::read(Packet*), the framed
//                         read that bounds the parser to the declared
//                         body and throws when it reads less or more
//
// Keep it in step with processCommand: a gate added there and not here
// makes this target report inputs production refuses.
//
// Outcomes: every exception the production loop can see (Throwable,
// std::exception) is a rejected input and returns 0. A crash, a
// sanitizer report or an abort is a finding. PacketAssert.h's Assert
// throws AssertionError in builds without NDEBUG, which is a rejection
// too; set DE_FUZZ_ABORT_ON_ASSERT to make it abort instead, to see
// which Asserts hostile input reaches (a Release build compiles them
// out). A failed Assert appends to assertion_failed.log in the current
// directory, so run a fuzzer from a scratch directory.
//
// Built twice by tests/fuzz/CMakeLists.txt: with replay_main.cpp as the
// replay ctest in every native test tree, and with libFuzzer's main as
// fuzz_client_stream under BUILD_FUZZERS.
//
//----------------------------------------------------------------------

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
#include <exception>
#include <memory>

namespace {

// ClientPlayer's input ring (ClientPlayer.cpp:49).
const unsigned int	kInputBufferLen = 32768;

// More frames than any one input needs to reach a parser; bounds the
// time a stream of empty frames can take.
const int		kMaxFrames = 64;

bool	g_bAbortOnAssert = false;

//----------------------------------------------------------------------
// Reads frames until processCommand would stop. Throws what it throws.
//----------------------------------------------------------------------
void	ReadFrames(SocketInputStream& stream)
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
			if (!g_pPacketValidator->isValidPacketID(CPS_NORMAL, packetID))
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

} // namespace

//----------------------------------------------------------------------
// The two tables GameInit.cpp builds before any connection exists.
//----------------------------------------------------------------------
extern "C" int LLVMFuzzerInitialize(int*, char***)
{
	g_pPacketFactoryManager = new PacketFactoryManager();
	g_pPacketFactoryManager->init();
	g_pPacketValidator = new PacketValidator();
	g_pPacketValidator->init();

	g_bAbortOnAssert = Basic::GetEnvironment("DE_FUZZ_ABORT_ON_ASSERT").has_value();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
	if (size < 1)
		return 0;

	const uchar	code = data[0];
	const std::size_t	streamLen = size - 1;

	// The ring holds one byte less than its size.
	if (streamLen >= kInputBufferLen)
		return 0;

	Socket				socket(new SocketImpl());
	SocketEncryptInputStream	stream(&socket, kInputBufferLen);
	stream.setEncryptCode(code);
	if (!SocketInputStreamTestAccess::Preload(stream, data + 1, (unsigned int)streamLen))
		return 0;

	try {
		ReadFrames(stream);
	} catch (AssertionError&) {
		if (g_bAbortOnAssert)
			std::abort();
	} catch (Throwable&) {
	} catch (std::exception&) {
	}

	return 0;
}
