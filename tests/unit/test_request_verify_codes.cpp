#include "test_framework.h"
#include "packet_stream_access.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "Rpackets/RCConnectVerify.h"
#include "Rpackets/CRRequest.h"
#include "Rpackets/RCRequestVerify.h"
#include <type_traits>
#include <vector>

// The two answers a peer's server side writes, and the request a peer
// sends. The client has no reply for the answers, but its game connection
// reads any packet id in the normal status before it looks for a reply,
// so the code byte the server sends lands in the packet: every byte has
// to be a value of its enum.
namespace {

// The fixed types are what makes every byte a value; a plain enum's values
// end at its largest enumerator's bit width, and only UBSan sees a load
// past that.
static_assert(std::is_same_v<std::underlying_type_t<RC_CONNECT_VERIFY_CODE>, BYTE>);
static_assert(std::is_same_v<std::underlying_type_t<REQUEST_VERIFY>, BYTE>);
static_assert(std::is_same_v<std::underlying_type_t<CR_REQUEST_CODE>, BYTE>);

template<class PacketType>
void ReadCode(PacketType& packet, BYTE code)
{
	const std::vector<unsigned char> bytes{code};
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	packet.read(stream);
	CHECK(stream.isEmpty());
}

} // namespace

TEST(RequestVerifyCodes, AnyCodeByteReadsBackAsSent)
{
	for (int code : {0, 3, 4, 8, 200, 255})
	{
		RCConnectVerify connect;
		ReadCode(connect, static_cast<BYTE>(code));
		CHECK_EQ(code, static_cast<int>(connect.getCode()));
		RCRequestVerify request;
		ReadCode(request, static_cast<BYTE>(code));
		CHECK_EQ(code, static_cast<int>(request.getCode()));
	}
}
