#include "test_framework.h"
#include "packet_stream_access.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Datagram.h"
#include "Gpackets/GCCastingSkill.h"
#include "Gpackets/GCDisconnect.h"
#include "Gpackets/GCPhoneConnectionFailed.h"
#include "Gpackets/GCReconnect.h"
#include "Gpackets/GCRequestFailed.h"
#include "Gpackets/GCRequestedIP.h"
#include "Gpackets/GCShowMessageBox.h"
#include "Gpackets/GLIncomingConnectionError.h"
#include "Gpackets/GLIncomingConnectionOK.h"
#include "Lpackets/LCVersionCheckOK.h"
#include "Lpackets/LGIncomingConnection.h"
#include "Upackets/CURequestLoginMode.h"
#include "Upackets/UCRequestLoginMode.h"
#include <vector>

// These handlers accept their packet and act on nothing in any shipped build.
// Each check decodes a packet, runs the gamemodel handler on it and requires
// the packet to encode to the same bytes afterwards. Calling every handler
// directly is also the proof that its object links from gamemodel.
namespace {

template<class PacketType>
std::vector<unsigned char> Encode(const PacketType& packet)
{
	Socket socket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream stream(&socket);
	packet.write(stream);
	return SocketOutputStreamTestAccess::Bytes(stream);
}

template<class PacketType, class Handle>
void CheckIgnored(const PacketType& original, Handle handle)
{
	const std::vector<unsigned char> bytes = Encode(original);
	Socket socket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&socket, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	PacketType packet;
	packet.read(stream);
	CHECK(stream.isEmpty());
	handle(&packet);
	CHECK(Encode(packet) == bytes);
}

// The login and game servers' datagram packets travel as whole frames.
template<class PacketType>
std::vector<unsigned char> EncodeFrame(const PacketType& packet)
{
	Datagram datagram;
	datagram.write(&packet);
	const char* data = datagram.getData();
	return std::vector<unsigned char>(data, data + datagram.getLength());
}

template<class PacketType, class Handle>
void CheckIgnoredDatagram(const PacketType& original, Handle handle)
{
	std::vector<unsigned char> bytes = EncodeFrame(original);
	Datagram datagram;
	datagram.setData(reinterpret_cast<char*>(bytes.data()), static_cast<uint>(bytes.size()));
	PacketID_t id = 0;
	PacketSize_t size = 0;
	datagram.read(id);
	datagram.read(size);
	CHECK_EQ(original.getPacketID(), id);
	CHECK_EQ(original.getPacketSize(), size);
	PacketType packet;
	packet.read(datagram);
	handle(&packet);
	CHECK(EncodeFrame(packet) == bytes);
}

} // namespace

TEST(IgnoredPacketHandlers, GameServerRepliesLeaveTheirPacketUnchanged)
{
	GCCastingSkill casting;
	casting.setSkillType(7);
	CheckIgnored(casting, [](GCCastingSkill* p) { GCCastingSkillHandler::execute(p, nullptr); });

	GCDisconnect disconnect;
	disconnect.setMessage("server closing");
	CheckIgnored(disconnect, [](GCDisconnect* p) { GCDisconnectHandler::execute(p, nullptr); });

	CheckIgnored(GCPhoneConnectionFailed(),
		[](GCPhoneConnectionFailed* p) { GCPhoneConnectionFailedHandler::execute(p, nullptr); });

	GCReconnect reconnect;
	reconnect.setName("slayer");
	reconnect.setServerIP("10.0.0.2");
	reconnect.setKey(0x12345678);
	CheckIgnored(reconnect, [](GCReconnect* p) { GCReconnectHandler::execute(p, nullptr); });

	GCRequestFailed failed;
	failed.setCode(3);
	failed.setName("vampire");
	CheckIgnored(failed, [](GCRequestFailed* p) { GCRequestFailedHandler::execute(p, nullptr); });

	GCRequestedIP requested;
	requested.setName("ousters");
	requested.setIP(0x0100007f);
	requested.setPort(9858);
	CheckIgnored(requested, [](GCRequestedIP* p) { GCRequestedIPHandler::execute(p, nullptr); });

	// GCShowMessageBox's read and write throw for every message length, so
	// only the handler's effect on a constructed packet can be checked.
	GCShowMessageBox box;
	box.setMessage("notice");
	GCShowMessageBoxHandler::execute(&box, nullptr);
	CHECK(box.getMessage() == "notice");
}

TEST(IgnoredPacketHandlers, LoginAndUpdateRepliesLeaveTheirPacketUnchanged)
{
	CheckIgnored(LCVersionCheckOK(), [](LCVersionCheckOK* p) { LCVersionCheckOKHandler::execute(p, nullptr); });
	CheckIgnored(CURequestLoginMode(),
		[](CURequestLoginMode* p) { CURequestLoginModeHandler::execute(p, nullptr); });

	// The update server's reply travels on a raw socket, not a packet stream.
	UCRequestLoginMode mode;
	mode.setLoginMode(1);
	UCRequestLoginModeHandler::execute(&mode, nullptr);
	CHECK_EQ(1, mode.getLoginMode());
}

TEST(IgnoredPacketHandlers, ServerToServerConnectionPacketsLeaveTheirPacketUnchanged)
{
	GLIncomingConnectionError error;
	error.setMessage("refused");
	error.setPlayerID("account");
	CheckIgnoredDatagram(error, [](GLIncomingConnectionError* p) { GLIncomingConnectionErrorHandler::execute(p); });

	GLIncomingConnectionOK ok;
	ok.setPlayerID("account");
	ok.setTCPPort(9999);
	ok.setKey(0xCAFEBABE);
	CheckIgnoredDatagram(ok, [](GLIncomingConnectionOK* p) { GLIncomingConnectionOKHandler::execute(p); });

	LGIncomingConnection incoming;
	incoming.setPlayerID("account");
	incoming.setPCName("slayer");
	incoming.setClientIP("10.0.0.3");
	CheckIgnoredDatagram(incoming, [](LGIncomingConnection* p) { LGIncomingConnectionHandler::execute(p); });
}
