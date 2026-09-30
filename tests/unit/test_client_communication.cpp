//----------------------------------------------------------------------
// test_client_communication.cpp
//----------------------------------------------------------------------
//
// ClientCommunicationManager::Update over loopback: the peer-to-peer
// UDP receive loop, fed real datagrams through a real socket.
//
// The socket binds INADDR_ANY on the configured port at start-up and
// nothing authenticates a datagram, so whoever can reach the port
// chooses what Update() reads. What it must not do with a hostile
// datagram is send anything anywhere: a bug report is a "*bug_report"
// chat line on the player's own connection to the server, and a sender
// who could make the client post one per datagram could flood the
// server from the victim's session at the attacker's pace.
//
// The manager takes its port from the wire host, so the test asks the
// system for a free one, lets it go and hands it over; a report is
// observed through the host's BugReportTarget entry, which
// SendBugReport asks only once it has decided to send. A handler for
// RCPositionInfo, registered once per process, is the positive
// control: the last datagram sent is a valid one, so when it has been
// dispatched every datagram before it has been read.
//
// The same loop must also survive a datagram with no bytes, which on
// Linux used to stop it for good (AnEmptyDatagramDoesNotStopTheReceiveLoop).
//
// Compiled with the packetwire defines (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "ClientCommunicationManager.h"
#include "Datagram.h"
#include "DatagramSocket.h"
#include "Packet.h"
#include "PacketDispatcher.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "WebSocketTransport.h"
#include "WireHost.h"

#ifdef _WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <chrono>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

namespace {

uint	s_Port		= 0;
int	s_Asked		= 0;
int	s_Dispatched	= 0;

int	HostMaxProcessPacket()		{ return 11; }
uint	HostUDPPort()			{ return s_Port; }
Player*	HostBugReportTarget()		{ s_Asked++; return NULL; }

const WireHost	s_Host = {
	.MaxProcessPacket		= HostMaxProcessPacket,
	.ClientCommunicationUDPPort	= HostUDPPort,
	.BugReportTarget		= HostBugReportTarget,
};

struct HostScope
{
	HostScope()	{ Wire::SetHost(&s_Host); }
	~HostScope()	{ Wire::SetHost(NULL); }
};

struct TablesScope
{
	TablesScope()
	: m_pFactories(g_pPacketFactoryManager), m_pValidator(g_pPacketValidator)
	{
		factories.init();
		validator.init();
		g_pPacketFactoryManager = &factories;
		g_pPacketValidator = &validator;
	}

	~TablesScope()
	{
		g_pPacketFactoryManager = m_pFactories;
		g_pPacketValidator = m_pValidator;
	}

	PacketFactoryManager	factories;
	PacketValidator		validator;

private:
	PacketFactoryManager*	m_pFactories;
	PacketValidator*	m_pValidator;
};

void	PositionInfoHandler(Packet*, Player*)
{
	s_Dispatched++;
}

// The dispatch table is filled once per process and refuses a second
// registration.
void	EnsurePositionInfoHandlerRegistered()
{
	static const bool registered = [] {
		PacketDispatcher::registerHandler(Packet::PACKET_RC_POSITION_INFO, &PositionInfoHandler);
		return true;
	}();
	(void)registered;
}

// A port no socket holds right now: bound on 0, read back, released.
uint	FreeUDPPort()
{
	std::unique_ptr<DatagramSocket> pProbe(new DatagramSocket(0));

	struct sockaddr_in	addr;
	std::memset(&addr, 0, sizeof(addr));
#ifdef _WIN32
	int		len = (int)sizeof(addr);
#else
	socklen_t	len = (socklen_t)sizeof(addr);
#endif
	if (getsockname(pProbe->getSOCKET(), (struct sockaddr *)&addr, &len) != 0)
		return 0;
	return ntohs(addr.sin_port);
}

void	AppendLE(std::vector<char>& out, unsigned long long value, size_t width)
{
	for (size_t i = 0; i < width; i++)
		out.push_back((char)((value >> (8 * i)) & 0xFF));
}

// id, size field, body, then the pad byte both peers count.
std::vector<char>	Frame(PacketID_t id, PacketSize_t sizeField, const std::vector<char>& body)
{
	std::vector<char> frame;
	AppendLE(frame, id, szPacketID);
	AppendLE(frame, sizeField, szPacketSize);
	frame.insert(frame.end(), body.begin(), body.end());
	frame.push_back(0);
	return frame;
}

void	Send(DatagramSocket& sender, uint port, std::vector<char> bytes)
{
	Datagram datagram;
	datagram.setData(&bytes[0], (uint)bytes.size());
	datagram.setHost("127.0.0.1");
	datagram.setPort(port);
	sender.send(&datagram);
}

// A datagram with no bytes at all. Datagram and DatagramSocket::send
// have no way to say that, so it goes straight to sendto().
void	SendEmpty(DatagramSocket& sender, uint port)
{
	struct sockaddr_in	to;
	std::memset(&to, 0, sizeof(to));
	to.sin_family		= AF_INET;
	to.sin_addr.s_addr	= htonl(INADDR_LOOPBACK);
	to.sin_port		= htons((unsigned short)port);
#ifdef _WIN32
	const int	len = (int)sizeof(to);
#else
	const socklen_t	len = (socklen_t)sizeof(to);
#endif
	sendto(sender.getSOCKET(), "", 0, 0, (struct sockaddr *)&to, len);
}

// A valid RCPositionInfo: a name of nine bytes, the zone id, x and y.
std::vector<char>	PositionInfoFrame()
{
	std::vector<char> body;
	body.push_back(9);
	const char* name = "Nosferatu";
	body.insert(body.end(), name, name + 9);
	AppendLE(body, 0x8A9B, szZoneID);
	body.push_back((char)0xC5);
	body.push_back((char)0xD6);
	return Frame(Packet::PACKET_RC_POSITION_INFO, (PacketSize_t)body.size(), body);
}

// Calls Update() until one packet has been dispatched, or for two
// seconds: loopback delivers at once, but a slow machine gets time
// before the datagram is called lost.
void	UpdateUntilDispatched(ClientCommunicationManager& manager)
{
	for (int i = 0; i < 200 && s_Dispatched == 0; i++)
	{
		manager.Update();
		if (s_Dispatched == 0)
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

} // namespace

// Three kinds of hostile datagram, each refused, and none of them may
// make the client send a report: an id past PACKET_MAX (whose
// exception text carries "(datagram)", which Update's catch matched to
// send one), a size field over RCPositionInfo's maximum (which
// Datagram::read reported through PacketDiagnostics before throwing),
// and a GC id. Then one valid RCPositionInfo, which must still arrive.
TEST(ClientCommunicationManager, AHostileDatagramSendsNoBugReport)
{
	// A browser build opens no UDP socket; Update() is a no-op there.
	if (NetworkTransport::UsesWebSocket())
		return;

	EnsureSocketsInitialised();
	EnsurePositionInfoHandlerRegistered();

	TablesScope	tables;
	HostScope	host;

	s_Port = FreeUDPPort();
	CHECK(s_Port != 0);
	s_Asked = 0;
	s_Dispatched = 0;

	ClientCommunicationManager	manager;
	std::unique_ptr<DatagramSocket>	pSender(new DatagramSocket());

	const PacketSize_t overMax =
		tables.factories.getPacketMaxSize(Packet::PACKET_RC_POSITION_INFO) + 1;

	for (int i = 0; i < 4; i++)
		Send(*pSender, s_Port, Frame((PacketID_t)0xFFFF, 0, std::vector<char>()));
	for (int i = 0; i < 3; i++)
		Send(*pSender, s_Port, Frame(Packet::PACKET_RC_POSITION_INFO, overMax, std::vector<char>()));
	Send(*pSender, s_Port, Frame(Packet::PACKET_GC_SAY, 4, std::vector<char>(4, 0)));

	Send(*pSender, s_Port, PositionInfoFrame());

	UpdateUntilDispatched(manager);

	CHECK_EQ(1, s_Dispatched);
	CHECK_EQ(0, s_Asked);
}

// An empty datagram, then a valid RCPositionInfo, which must arrive.
// DatagramSocket::receive used to call recvfrom() only when FIONREAD
// reported bytes waiting. On Linux FIONREAD on a UDP socket is the size
// of the next datagram, so an empty one at the head of the queue read
// as "nothing waiting" on every tick, was never taken off, and hid
// every datagram behind it until the process restarted: one datagram
// from anyone stopped the client's UDP receive for good. On macOS
// FIONREAD counts every queued byte, so there this passed before the
// fix too.
TEST(ClientCommunicationManager, AnEmptyDatagramDoesNotStopTheReceiveLoop)
{
	if (NetworkTransport::UsesWebSocket())
		return;

	EnsureSocketsInitialised();
	EnsurePositionInfoHandlerRegistered();

	TablesScope	tables;
	HostScope	host;

	s_Port = FreeUDPPort();
	CHECK(s_Port != 0);
	s_Asked = 0;
	s_Dispatched = 0;

	ClientCommunicationManager	manager;
	std::unique_ptr<DatagramSocket>	pSender(new DatagramSocket());

	SendEmpty(*pSender, s_Port);
	Send(*pSender, s_Port, PositionInfoFrame());

	UpdateUntilDispatched(manager);

	CHECK_EQ(1, s_Dispatched);
	CHECK_EQ(0, s_Asked);
}
