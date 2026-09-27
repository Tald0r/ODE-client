//--------------------------------------------------------------------------------
// ClientCommunicationManager.h
//--------------------------------------------------------------------------------
// 다른 클라이언트로부터의 정보를 받아들어 
//--------------------------------------------------------------------------------
#include "Client_PCH.h"
#include "ClientCommunicationManager.h"
#include "Datagram.h"
#include "DatagramPacket.h"
#include "PacketDispatcher.h"
#include "WireHost.h"
#include "WebSocketTransport.h"
#include "PacketValidator.h"
#include "DebugLog.h"
// MTestDef.h is gone: its one struct sits behind __METROTECH_TEST__,
// which nothing defines (the OUTPUT_DEBUG block that would have is
// commented out), so all it carried here was DebugInfo.h - and with
// it MinTr.h, which the wire layer may not reach.

//--------------------------------------------------------------------------------
// Global
//--------------------------------------------------------------------------------
ClientCommunicationManager*	g_pClientCommunicationManager = NULL;


//--------------------------------------------------------------------------------
// constructor
//--------------------------------------------------------------------------------
ClientCommunicationManager::ClientCommunicationManager ()
: m_pDatagramSocket(NULL)
{
	// The gateway carries only the TCP stream and a browser cannot open UDP.
	// Leave the socket NULL, as a failed bind does: peer-to-peer sends and
	// Update() are then no-ops.
	if (NetworkTransport::UsesWebSocket()) return;
    // Note: __BEGIN_TRY/__END_CATCH are not used here because __BEGIN_TRY
    // expands to a no-op in Release builds (NDEBUG), which would leave the
    // catch(...) below orphaned. Use explicit try/catch instead.
    try {
        try {
            // create datagram server socket
            m_pDatagramSocket = new DatagramSocket( Wire::ClientCommunicationUDPPort() );

            SocketAPI::setsocketnonblocking_ex( m_pDatagramSocket->getSOCKET(), true );

//		m_pDatagramSocket->
        } catch (Throwable& t)	{
            DEBUG_ADD_FORMAT_ERR("[Error] CCM-%s", t.toString().c_str());
            // Note: Socket creation may fail if port is in use, continue without P2P communication
            m_pDatagramSocket = NULL;
        }
    }
    catch (...) {
        // Catch-all to prevent constructor from propagating exceptions
    }
}

//--------------------------------------------------------------------------------
// destructor
//--------------------------------------------------------------------------------
ClientCommunicationManager::~ClientCommunicationManager ()
{

    if ( m_pDatagramSocket != NULL ) {
        delete m_pDatagramSocket;
        m_pDatagramSocket = NULL;
    }

}


//--------------------------------------------------------------------------------
// send datagram to datagram-socket
//--------------------------------------------------------------------------------
void ClientCommunicationManager::sendDatagram ( Datagram * pDatagram )
{
    __BEGIN_TRY

	// The constructor leaves the socket NULL when the UDP bind fails (port
	// already taken by a second client instance, for example) and the client is
	// meant to run on without peer-to-peer. Every user of the socket therefore
	// has to tolerate its absence.
	if (m_pDatagramSocket == NULL)
	{
		DEBUG_ADD("[Error] ClientCommunicationManager-sendDatagram-no UDP socket");
		return;
	}

    try
    {
        m_pDatagramSocket->send( pDatagram );
    }
    catch ( ConnectException & )
    {
		throw ConnectException( "ClientCommunicationManager::sendDatagram 상위로 던진다");
    }

    __END_CATCH
}


//--------------------------------------------------------------------------------
// send datagram-packet to datagram-socket
//--------------------------------------------------------------------------------
void ClientCommunicationManager::sendPacket ( const std::string& host , uint port , DatagramPacket * pPacket )
{
    __BEGIN_TRY
    __BEGIN_DEBUG

	if (host.size()==0)
	{
		DEBUG_ADD("[Error] ClientCommunicationManager-sendPacket-host NULL");
		return;
	}

	// See sendDatagram: the socket is optional, so peer-to-peer sends are
	// dropped rather than dereferencing NULL.
	if (m_pDatagramSocket == NULL)
	{
		DEBUG_ADD("[Error] ClientCommunicationManager-sendPacket-no UDP socket");
		return;
	}

    try {

        // 데이터그램 객체를 하나 두고, 전송할 peer 의 호스트와 포트를 지정한다.
        Datagram datagram;

		datagram.setHost(host);
        datagram.setPort(port);

        // 데이터그램 패킷을 데이터그램에 집어넣는다.
        datagram.write(pPacket);

        // 데이터그램 소켓을 통해서 데이터그램을 전송한다.
        m_pDatagramSocket->send( &datagram );

		#ifdef __METROTECH_TEST__
			g_UDPTest.UDPPacketSend ++;
		#endif

		#ifdef __DEBUG_OUTPUT__
			DEBUG_ADD_FORMAT("[To] %s(%d)", host.c_str(), port);
			DEBUG_ADD_FORMAT("[Send] %s", pPacket->toString().c_str());
		#endif

    } catch ( Throwable & t ) {
		// -_- it drops the connection anyway, so report it as a string.
		// The exception text can embed packet derived data, so it is passed as
		// an argument and never as the format string.
		if( strstr( t.toString().c_str(), "InvalidProtocolException") != NULL )
			if( strstr( t.toString().c_str(), "(datagram)" ) != NULL )
				SendBugReport( "%s", t.toString().c_str() );

        DEBUG_ADD( t.toString().c_str() );
    }

    __END_DEBUG
    __END_CATCH
}

//--------------------------------------------------------------------------------
// Update
//--------------------------------------------------------------------------------
void
ClientCommunicationManager::Update()
{
	// See sendDatagram: the socket is optional. This runs once per frame from
	// the main loop, so it stays silent -- a failed bind would otherwise write a
	// log line every frame for the lifetime of the process.
	if (m_pDatagramSocket == NULL)
		return;

	const int maxPacket = Wire::MaxProcessPacket();

	for (int i=0; i<maxPacket; i++)
	{
		//DEBUG_ADD_FORMAT("[CC-Update] %d", i);

		Datagram*       pDatagram       = NULL;
		DatagramPacket* pDatagramPacket = NULL;
	
		try
		{
			// 데이터그램 객체를 끄집어낸다.
			pDatagram = m_pDatagramSocket->receive();

			if (pDatagram==NULL)
				break;

			DEBUG_ADD("[CCM-Update] something");
			
			// 데이터그램 패킷 객체를 끄집어낸다.
			pDatagram->read( pDatagramPacket );

			#ifdef __METROTECH_TEST__
				g_UDPTest.UDPPacketRead ++;
			#endif

			if (pDatagramPacket!=NULL)
			{
				#ifdef __DEBUG_OUTPUT__
					DEBUG_ADD_FORMAT("[RECEIVE] %s", pDatagramPacket->toString().c_str());
				#endif

				// 걍 한번 체크..
				if ( !g_pPacketValidator->isValidPacketID( CPS_CLIENT_COMMUNICATION_NORMAL, pDatagramPacket->getPacketID() ))
				{
					throw InvalidProtocolException("invalid packet ORDER");
				}			

				// 끄집어낸 데이터그램 패킷 객체를 실행한다.
				DEBUG_ADD_FORMAT("[From] %s(%d)", pDatagramPacket->getHost().c_str(),
													pDatagramPacket->getPort());

				// Dispatch table (RESTRUCTURING.md tasks 2.1-2.4); an
				// unregistered id throws. The datagram direction has no
				// player behind it.
				PacketDispatcher::dispatch( pDatagramPacket , NULL );

				#ifdef __METROTECH_TEST__
					g_UDPTest.UDPPacketExecute ++;
				#endif

				// 데이터그램 패킷 객체를 삭제한다.
				delete pDatagramPacket;
			}

			// 데이터그램 객체를 삭제한다.
			delete pDatagram;
			
		}
		catch ( Throwable & t )
		{
			// -_- it drops the connection anyway, so report it as a string.
			// The exception text can embed packet derived data, so it is passed
			// as an argument and never as the format string.
			if( strstr( t.toString().c_str(), "InvalidProtocolException") != NULL )
				if( strstr( t.toString().c_str(), "(datagram)" ) != NULL )
					SendBugReport( "%s", t.toString().c_str() );

			DEBUG_ADD( t.toString().c_str() );

			if (pDatagramPacket!=NULL)
				delete pDatagramPacket;

			if (pDatagram!=NULL)
				delete pDatagram;			
		}
	}

}
