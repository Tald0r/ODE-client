#include "test_framework.h"
#include "packet_stream_access.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "Rpackets/RCConnectVerify.h"
#include "Rpackets/RCRequestVerify.h"
#include <vector>

// The two answers a peer's server side writes. The client has no reply for
// either, but its game connection reads any packet id in the normal
// status before it looks for a reply, so the code byte the server sends
// lands in the packet: every byte has to be a value of its enum.
namespace {

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
