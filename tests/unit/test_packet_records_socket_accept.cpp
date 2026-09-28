//----------------------------------------------------------------------
// test_packet_records_socket_accept.cpp
//----------------------------------------------------------------------
//
// ServerSocket::accept() over loopback: the accepted Socket carries the
// handle accept() returned, so bytes the peer sends arrive through it.
// SocketImpl::accept() keeps that handle as a SOCKET from accept_ex()
// to the new SocketImpl; a 64-bit Windows handle cannot be forced here,
// so this guards the path rather than reproducing a truncation.
//
// Compiled with the packetwire defines (tests/CMakeLists.txt).
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "packet_stream_access.h"

#include "Exception.h"
#include "ServerSocket.h"
#include "Socket.h"
#include "SocketAPI.h"

#ifdef _WIN32
#include <winsock.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <stdio.h>
#include <string.h>

#include <chrono>
#include <thread>

namespace {

//----------------------------------------------------------------------
// The port the system bound a listener on port 0 to; 0 on failure.
//----------------------------------------------------------------------
uint	BoundPort ( SOCKET s )
{
	struct sockaddr_in	addr;
	memset(&addr, 0, sizeof(addr));

#ifdef _WIN32
	int		len = (int)sizeof(addr);
#else
	socklen_t	len = (socklen_t)sizeof(addr);
#endif

	if (getsockname(s, (struct sockaddr *)&addr, &len) != 0)
		return 0;

	return ntohs(addr.sin_port);
}

} // namespace

TEST(PacketRecordsSocketAccept, AcceptedSocketReceivesWhatThePeerSent)
{
	EnsureSocketsInitialised();

	ServerSocket*	pListener = NULL;
	Socket*		pClient = NULL;
	Socket*		pAccepted = NULL;
	char		received = 0;

	try {
		pListener = new ServerSocket(0);

		const uint port = BoundPort(pListener->getSOCKET());
		CHECK(port != 0);

		pClient = new Socket("127.0.0.1", port);
		pClient->connect();

		// Non-blocking, so a connection that never arrives fails the
		// check after a bounded wait instead of hanging the suite.
		pListener->setNonBlocking(true);
		for (int i = 0; i < 200 && pAccepted == NULL; i++)
		{
			pAccepted = pListener->accept();
			if (pAccepted == NULL)
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		CHECK(pAccepted != NULL);

		if (pAccepted != NULL)
		{
			CHECK(pAccepted->getSOCKET() != INVALID_SOCKET);

			const char sent = 'Z';
			CHECK_EQ(1, (long long)pClient->send(&sent, 1));

			// Whether an accepted socket inherits the listener's
			// non-blocking mode differs by platform; set it, and wait
			// for the byte the same bounded way.
			pAccepted->setNonBlocking(true);
			uint nReceived = 0;
			for (int i = 0; i < 200 && nReceived == 0; i++)
			{
				try {
					nReceived = pAccepted->receive(&received, 1);
				} catch (NonBlockingIOException&) {
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}
			}
			CHECK_EQ(1, (long long)nReceived);
		}
	} catch (Throwable& t) {
		fprintf(stderr, "loopback accept threw: %s\n", t.toString().c_str());
		CHECK(false);
	} catch (...) {
		CHECK(false);
	}

	CHECK_EQ('Z', (long long)received);

	delete pAccepted;
	delete pClient;
	delete pListener;
}
