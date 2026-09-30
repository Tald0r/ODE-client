//----------------------------------------------------------------------
// fuzz_client_datagram.cpp
//----------------------------------------------------------------------
//
// Fuzz target: one UDP datagram arriving at the client's peer-to-peer
// socket, read the way ClientCommunicationManager::Update
// (Client/Packet/ClientCommunicationManager.cpp) reads it, minus the
// handler dispatch, which is executable-side.
//
// The socket is DatagramSocket(Wire::ClientCommunicationUDPPort()),
// bound on INADDR_ANY at start-up on every native build and read by
// Update() every 330 ms, and nothing authenticates a datagram: any
// sender that can reach the port chooses every byte of the input. The
// client accepts four ids over UDP, the RC packets that are
// DatagramPackets (the validator's CPS_CLIENT_COMMUNICATION_NORMAL set);
// Datagram::read refuses every other id before a packet exists.
//
// Input: the datagram's bytes, as recvfrom() returned them - a 6-byte
// header (id u16, body size u32, little-endian), the body, and the
// one-byte pad both peers count in the length. No encrypt code: UDP is
// not encrypted. An empty input is skipped: DatagramSocket::receive
// takes an empty datagram off the socket and makes no Datagram of it,
// so nothing reaches Datagram::read. (Until 2026-09-30 a Linux build
// never took it off - FIONREAD there is the next datagram's size - and
// every datagram behind it waited for good; see the review's row.) So
// is one longer than DATAGRAM_SOCKET_BUFFER_LEN, the most recvfrom()
// hands over.
//
// The path mirrors Update()'s, in order:
//
//   DatagramSocket::receive  new Datagram, setData(bytes), setAddress
//                            (a fixed sender here)
//   Datagram::read           header, PACKET_MAX, the validator set,
//                            the size cap, the datagram length,
//                            createPacket, the checked DatagramPacket
//                            conversion, the packet's read(Datagram&),
//                            setHost/setPort from the sender
//   __DEBUG_OUTPUT__ only    the packet's toString()
//   Update                   the validator check again, then the
//                            "[From] host(port)" log line's getters
//
// and stops before PacketDispatcher::dispatch. Update's catch formats
// every refusal with Throwable::toString() for the log, so this does
// too.
//
// Outcomes are the stream targets': a Throwable (all Update catches) is
// a rejected input, anything else escapes and is a crash, and
// DE_FUZZ_ABORT_ON_ASSERT turns a failed Assert into an abort. The
// tables come from client_stream_reader.h's Initialize(), the two
// GameInit.cpp builds before the socket opens; no wire host is
// installed, so the defaults answer (and a bug report, were one sent,
// would find no connection).
//
// Built twice by tests/fuzz/CMakeLists.txt: with replay_main.cpp as the
// replay ctest in every native test tree, and with libFuzzer's main as
// fuzz_client_datagram under BUILD_FUZZERS.
//
//----------------------------------------------------------------------

#include "client_stream_reader.h"

#include "Datagram.h"
#include "DatagramPacket.h"
#include "DatagramSocket.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

namespace {

// A fixed sender, 192.0.2.1:9858 (TEST-NET-1 and the default port), so
// the address the packet records is the same for every input.
void	SetSender(Datagram& datagram)
{
	SOCKADDR_IN	addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family		= AF_INET;
	addr.sin_addr.s_addr	= htonl(0xC0000201u);
	addr.sin_port		= htons(9858);
	datagram.setAddress(&addr);
}

void	ReadDatagram(const std::uint8_t* data, std::size_t size)
{
	Datagram datagram;
	datagram.setData(reinterpret_cast<char*>(const_cast<std::uint8_t*>(data)), (uint)size);
	SetSender(datagram);

	DatagramPacket* pRead = NULL;
	try {
		datagram.read(pRead);
		std::unique_ptr<DatagramPacket> pPacket(pRead);
		if (pPacket == nullptr)
			return;

#ifdef __DEBUG_OUTPUT__
		const std::string text = pPacket->toString();
		(void)text;
#endif

		if (!g_pPacketValidator->isValidPacketID(CPS_CLIENT_COMMUNICATION_NORMAL,
							  pPacket->getPacketID()))
			throw InvalidProtocolException("invalid packet ORDER");

		const std::string host = pPacket->getHost();
		const uint port = pPacket->getPort();
		(void)host;
		(void)port;
	} catch (AssertionError& t) {
		if (ClientStreamFuzz::g_bAbortOnAssert)
			std::abort();
		const std::string text = t.toString();
		(void)text;
	} catch (Throwable& t) {
		const std::string text = t.toString();
		(void)text;
	}
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int*, char***)
{
	ClientStreamFuzz::Initialize();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
	if (size == 0 || size > DATAGRAM_SOCKET_BUFFER_LEN)
		return 0;
	ReadDatagram(data, size);
	return 0;
}
