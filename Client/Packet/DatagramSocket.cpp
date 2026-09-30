//////////////////////////////////////////////////////////////////////
//
// Filename   : DatagramSocket.cpp
// Written By : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "DatagramSocket.h"
#include "PacketAssert.h"
#include "PacketFileAPI.h"
#include "DebugLog.h"
// MTestDef.h is gone: its one struct sits behind __METROTECH_TEST__,
// which nothing defines (the OUTPUT_DEBUG block that would have is
// commented out), so all it carried here was DebugInfo.h - and with
// it MinTr.h, which the wire layer may not reach.

//////////////////////////////////////////////////////////////////////
//
// constructor for UDP Client Socket
//
// UDP 클라이언트 소켓은 단지 nonamed 소켓만 생성해 두면 된다.
// 왜냐하면, 서버로 send할 때마다 Datagram의 주소를 지정해두면
// 되기 때문이다.
//
//////////////////////////////////////////////////////////////////////
DatagramSocket::DatagramSocket ()
: m_SocketID(INVALID_SOCKET)
{
	__BEGIN_TRY 

	__BEGIN_DEBUG
	m_SocketID = SocketAPI::socket_ex( AF_INET , SOCK_DGRAM , 0 );
	__END_DEBUG

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// constructor for UDP Server Socket
//
// UDP 서버 소켓은 소켓을 생성하고, port 를 바인딩시키면 준비가 완료된다.
//
//////////////////////////////////////////////////////////////////////
DatagramSocket::DatagramSocket ( uint port )
: m_SocketID(INVALID_SOCKET)
{
	__BEGIN_TRY 

	m_SocketID = SocketAPI::socket_ex( AF_INET , SOCK_DGRAM , 0 );

	// clear memory
	memset( &m_SockAddr , 0 , szSOCKADDR_IN );
	m_SockAddr.sin_family      = AF_INET;
	m_SockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	m_SockAddr.sin_port        = htons(port);

	int opt = 1;
	SocketAPI::setsockopt_ex( m_SocketID, SOL_SOCKET , SO_REUSEADDR , &opt , sizeof(opt) );

	// bind address to socket
	SocketAPI::bind_ex( m_SocketID , (SOCKADDR*)&m_SockAddr , szSOCKADDR_IN );

	// set host
	inet_ntoa( m_SockAddr.sin_addr );

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
DatagramSocket::~DatagramSocket () noexcept(false)
{
	__BEGIN_TRY
	
	if ( m_SocketID != INVALID_SOCKET )
	{
		try {
			SocketAPI::closesocket_ex( m_SocketID );
		} catch (Throwable& t) {
			DEBUG_ADD( t.toString().c_str() );
		}
	}

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// send datagram to peer
//////////////////////////////////////////////////////////////////////
uint DatagramSocket::send ( Datagram * pDatagram )
{
	__BEGIN_TRY 

	Assert( pDatagram != NULL );

	int nSent = SocketAPI::sendto_ex( m_SocketID , pDatagram->getData() , pDatagram->getLength() , 0 , pDatagram->getAddress() , szSOCKADDR_IN );

	return (uint)nSent;

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// receive datagram from peer
//
// Takes the next datagram off the socket, or returns NULL only when
// none is waiting (or, on Windows, recvfrom() failed). It must never
// block, since it runs on the game thread every Update(), and the
// socket itself is a blocking one.
//
// A datagram with no bytes is returned as an empty Datagram, not as
// NULL. Update() ends its tick on NULL, so an empty datagram returned
// as NULL cost a whole 330 ms tick where any other refused datagram
// costs one of the tick's MaxProcessPacket reads; an empty Datagram is
// refused by Datagram::read like any other short one.
//
// POSIX asks recvfrom() itself, with MSG_DONTWAIT, rather than asking
// FIONREAD first. On Linux FIONREAD on a UDP socket is the size of the
// next datagram, so an empty datagram at the head of the queue read as
// "nothing waiting" forever and hid every datagram behind it. Windows
// still asks FIONREAD first, untested here.
//
//////////////////////////////////////////////////////////////////////
Datagram * DatagramSocket::receive ()
{
	__BEGIN_TRY 

	Datagram * pDatagram = NULL;

	SOCKADDR_IN SockAddr;
	uint _szSOCKADDR_IN = szSOCKADDR_IN;

	int nReceived = -1;

#if defined(PLATFORM_POSIX)
	try
	{
		nReceived = SocketAPI::recvfrom_ex( m_SocketID , m_Buffer , DATAGRAM_SOCKET_BUFFER_LEN , MSG_DONTWAIT , (SOCKADDR*)&SockAddr , &_szSOCKADDR_IN );
	}
	catch ( NonBlockingIOException & )
	{
		// Nothing waiting.
		return NULL;
	}

	#ifdef __METROTECH_TEST__
		g_UDPTest.UDPPacketAvailable ++;
	#endif
#else
	// Is there anything to read?
	ulong available = SocketAPI::availablesocket_ex( m_SocketID );		
	
	if (available > 0)
	{
		#ifdef __METROTECH_TEST__
			g_UDPTest.UDPPacketAvailable ++;
		#endif

		DEBUG_ADD_FORMAT("[DatagramSocket] available=%d", available);

		// Copy it into the socket's own buffer.
		nReceived = SocketAPI::recvfrom_ex( m_SocketID , m_Buffer , DATAGRAM_SOCKET_BUFFER_LEN , 0 , (SOCKADDR*)&SockAddr , &_szSOCKADDR_IN );
	}
#endif

	// An empty datagram (nReceived == 0) is still one taken off the
	// socket; only a failed recvfrom() (-1, Windows) makes none.
	if ( nReceived >= 0 ) 
	{
		#ifdef __METROTECH_TEST__
			g_UDPTest.UDPPacketReceive ++;
		#endif

		DEBUG_ADD_FORMAT("[DatagramSocket] received=%d", nReceived);

		pDatagram = new Datagram();
		pDatagram->setData( m_Buffer , nReceived );
		pDatagram->setAddress( &SockAddr );
	}

	return pDatagram;

	__END_CATCH
}
