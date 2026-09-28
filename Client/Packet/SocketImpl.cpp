////////////////////////////////////////////////////////////////////////
//
// SocketImpl.cpp
//
// by Reiot
//
////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////
// include files
//////////////////////////////////////////////////
#include "Client_PCH.h"
#include "SocketImpl.h"
#include "PacketFileAPI.h"
#include "SocketAPI.h"
#include "WebSocketTransport.h"

#if defined(PLATFORM_POSIX)
#include <sys/socket.h>
#include <arpa/inet.h>	// for inet_ntoa()
#elif __WINDOWS__
#endif

//////////////////////////////////////////////////
// forward declarations
//////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////
// 
// SocketImpl::SocketImpl ();
// 
// Parameters 
//     none
// 
// Exceptions
//     Error
// 
////////////////////////////////////////////////////////////////////////
SocketImpl::SocketImpl () 
: m_SocketID(INVALID_SOCKET), m_Host("Unknown"), m_Port(0)
{
}


//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
SocketImpl::SocketImpl ( uint port ) 
: m_SocketID(INVALID_SOCKET), m_Host("Unknown"), m_Port(port)
{
	m_key = 1;
}


//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
SocketImpl::SocketImpl ( const std::string & host , uint port ) 
: m_SocketID(INVALID_SOCKET), m_Host(host), m_Port(port)
{
	m_key = 1;
}


////////////////////////////////////////////////////////////////////////
//
// SocketImpl::SocketImpl ( const SocketImpl & impl );
//
// copy constructor 
//
// Parameters
//     impl - socket implementation object
//
// Exceptions
//     none
//
////////////////////////////////////////////////////////////////////////
SocketImpl::SocketImpl ( const SocketImpl & impl ) 
: m_SocketID(impl.m_SocketID), m_Host(impl.m_Host), m_Port(impl.m_Port)
{
	if (impl.m_WebMode) throw Error("Cannot copy a WebSocket connection");
	m_key = 1;
}

////////////////////////////////////////////////////////////////////////
//
// SocketImpl::~SocketImpl ();
//
// virtual destructor
//
// Parameters
//     none
//
// Exceptions
//     Error
//
////////////////////////////////////////////////////////////////////////
SocketImpl::~SocketImpl () noexcept(false)
{
	__BEGIN_TRY 

	close();
	
	__END_CATCH
}
	
//////////////////////////////////////////////////////////////////////
// create connection
//////////////////////////////////////////////////////////////////////
void SocketImpl::create ()
{
	__BEGIN_TRY

	m_WebMode = NetworkTransport::UsesWebSocket();
	if (m_WebMode) return;
		
	// create socket
	m_SocketID = SocketAPI::socket_ex( AF_INET , SOCK_STREAM , 0 );

	// clear socket address structure
    memset( &m_SockAddr , 0 , sizeof(m_SockAddr) );

	// set socket address's family to "AF_INET"
	m_SockAddr.sin_family      = AF_INET;
	m_key = 1;
	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
// close connection
//////////////////////////////////////////////////////////////////////
void SocketImpl::close ()
{
	__BEGIN_TRY

	// Closing is idempotent. Player closes its socket and then deletes it,
	// and the destructor closes again; releasing the descriptor twice would
	// close whatever socket or file had reused its number in between.
	if (m_WebMode) {
		m_WebSocket.reset();
		m_WebMode = false;
		return;
	}
	if ( m_SocketID == INVALID_SOCKET )
		return;

	// Forget the number before the call: a close that fails with EINTR or
	// EIO has still released the descriptor, and a retry would close
	// whatever reused it. Linger is 0 everywhere, so WSAEWOULDBLOCK cannot
	// leave the handle open.
	const SOCKET socket = m_SocketID;
	m_SocketID = INVALID_SOCKET;
	try {
		SocketAPI::closesocket_ex( socket );
	} catch ( FileNotOpenedException & ) {
		// if already closed, ignore...
	}

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// bind address to socket
//////////////////////////////////////////////////////////////////////
void SocketImpl::bind ()
{
	__BEGIN_TRY
	if (m_WebMode) throw ConnectException("WebSockets do not support peer listeners");

	m_SockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_SockAddr.sin_port        = htons(m_Port);

	SocketAPI::bind_ex( m_SocketID , (const struct sockaddr *)&m_SockAddr , sizeof(m_SockAddr) );

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// bind address to socket
//////////////////////////////////////////////////////////////////////
void SocketImpl::bind ( uint port )
{
	__BEGIN_TRY
	if (m_WebMode) throw ConnectException("WebSockets do not support peer listeners");

	// set server's port
	m_Port = port;

	m_SockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_SockAddr.sin_port        = htons(m_Port);

	SocketAPI::bind_ex( m_SocketID , (const struct sockaddr *)&m_SockAddr , sizeof(m_SockAddr) );
	
	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
// bind address to socket
//////////////////////////////////////////////////////////////////////
void SocketImpl::listen ( uint backlog )
{
	__BEGIN_TRY
	if (m_WebMode) throw ConnectException("WebSockets do not support peer listeners");
		
	SocketAPI::listen_ex( m_SocketID , backlog );

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// connect to remote host
//////////////////////////////////////////////////////////////////////
void SocketImpl::connect ( const std::string & host , uint port )
{
	__BEGIN_TRY 
		
	m_Host = host;
	m_Port = port;
	if (m_WebMode) {
		// Release a previous connection before its replacement starts.
		m_WebSocket.reset();
		m_WebSocket = std::make_unique<NetworkTransport::WebSocketTransport>(
			NetworkTransport::WebSocketURL(NetworkTransport::WebSocketGateway(), host, port));
		return;
	}

	// set sockaddr's host
	m_SockAddr.sin_addr.s_addr = inet_addr( m_Host.c_str() );
	
	// set sockaddr's port
	m_SockAddr.sin_port = htons(m_Port);
	
	// try to connect to peer host
	SocketAPI::connect_ex( m_SocketID , (const struct sockaddr *)&m_SockAddr , sizeof(m_SockAddr) );
	
	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
void SocketImpl::connect ()
{
	__BEGIN_TRY
	if (m_WebMode) { connect(m_Host, m_Port); return; }

	// set sockaddr's host
	m_SockAddr.sin_addr.s_addr = inet_addr( m_Host.c_str() );
	
	// set sockaddr's port
	m_SockAddr.sin_port = htons(m_Port);
	
	// try to connect to peer host
	SocketAPI::connect_ex( m_SocketID , (const struct sockaddr *)&m_SockAddr , sizeof(m_SockAddr) );
	
	__END_CATCH
}
	

//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
SocketImpl * SocketImpl::accept ()
{
	__BEGIN_TRY
	if (m_WebMode) throw ConnectException("WebSockets do not support peer listeners");

    // make client socket address structure ready
    SOCKADDR_IN ClientAddr;
    memset( &ClientAddr , 0 , sizeof(ClientAddr) );
    uint len = sizeof(ClientAddr);
 
    // get client socket descriptor
    uint ClientID = static_cast<uint>(SocketAPI::accept_ex( m_SocketID , (struct sockaddr *)&ClientAddr , &len ));
 
    // create MSocketImpl with socket descriptor
    SocketImpl * client = new SocketImpl();
 
	// initialize client socket implementation object
	client->m_SocketID = ClientID;
	memcpy( &(client->m_SockAddr) , &ClientAddr , sizeof(SOCKADDR_IN) );
	client->m_Host = client->_getHost();
	client->m_Port = client->_getPort();
 
    // return SocketImpl
    return client;

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// send data to peer
//////////////////////////////////////////////////////////////////////
uint SocketImpl::send ( const void * buf , uint len , uint flags )
{
	__BEGIN_TRY
	if (m_WebMode) {
		if (!m_WebSocket) throw ConnectException("WebSocket has not connected");
		return m_WebSocket->send(buf, len);
	}
#ifdef __USE_ENCRY__  //使用加密协议

	DWORD enlen =len+5;
	char* enbuf = new char[enlen];
	
	//设置封包Key
	enbuf[4]=m_key;
	// 设置封包长度
	memcpy(enbuf,&enlen,4);

	memcpy(&enbuf[5],buf,len);
	EnData(&enbuf[4],len+1);
	uint ulen=SocketAPI::send_ex( m_SocketID , enbuf , enlen , flags );
	ulen--;
	delete[] enbuf;
	enbuf =NULL;
	return len;
	//return SocketAPI::send_ex( m_SocketID , buf , len , flags );
#else //不使用加密 
	return SocketAPI::send_ex( m_SocketID , buf , len , flags );
#endif
	__END_CATCH
}
	
//////////////////////////////////////////////////////////////////////
// receive data from peer
//////////////////////////////////////////////////////////////////////
uint SocketImpl::receive ( void * buf , uint len , uint flags )
{
	__BEGIN_TRY
	if (m_WebMode) {
		if (!m_WebSocket) throw ConnectException("WebSocket has not connected");
		return m_WebSocket->receive(buf, len);
	}

	return SocketAPI::recv_ex( m_SocketID , buf , len , flags );
	
	__END_CATCH
}
	
//////////////////////////////////////////////////////////////////////
// how much available?
//////////////////////////////////////////////////////////////////////
uint SocketImpl::available () const
{
	__BEGIN_TRY
	if (m_WebMode) return m_WebSocket ? m_WebSocket->available() : 0;
		
	return SocketAPI::availablesocket_ex( m_SocketID );

	__END_CATCH
}
	
//////////////////////////////////////////////////////////////////////
// get socket's linger status
//////////////////////////////////////////////////////////////////////
uint SocketImpl::getLinger () const
{
	__BEGIN_TRY
	if (m_WebMode) return 0;

	struct linger ling;
	uint len = sizeof(ling);
	
	SocketAPI::getsockopt_ex( m_SocketID , SOL_SOCKET , SO_LINGER , &ling , &len );
	
	return ling.l_linger;
	
	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// set socket's linger status
//////////////////////////////////////////////////////////////////////
void SocketImpl::setLinger ( uint lingertime )
{
	__BEGIN_TRY
	if (m_WebMode) return;

	struct linger ling;
	
	ling.l_onoff = lingertime > 0 ? 1 : 0;
	ling.l_linger = lingertime;
	
	SocketAPI::setsockopt_ex( m_SocketID , SOL_SOCKET , SO_LINGER , &ling , sizeof(ling) );
	
	__END_CATCH
}

	
//////////////////////////////////////////////////////////////////////
// get socket's reuse address status
//////////////////////////////////////////////////////////////////////
bool SocketImpl::isReuseAddr () const
{
	__BEGIN_TRY
	if (m_WebMode) return false;
		
	int reuse;
	uint len = sizeof(reuse);
	
	SocketAPI::getsockopt_ex( m_SocketID , SOL_SOCKET , SO_REUSEADDR , &reuse , &len );
	
	return reuse == 1;
	
	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// set socket's reuse address status
//////////////////////////////////////////////////////////////////////
void SocketImpl::setReuseAddr ( bool on )
{
	__BEGIN_TRY
	if (m_WebMode) return;

	int opt = on == true ? 1 : 0;
	
	SocketAPI::setsockopt_ex( m_SocketID , SOL_SOCKET , SO_REUSEADDR , &opt , sizeof(opt) );
	
	__END_CATCH
}
	

//////////////////////////////////////////////////////////////////////
// get socket's nonblocking status
//////////////////////////////////////////////////////////////////////
bool SocketImpl::isNonBlocking () const
{
	__BEGIN_TRY
	if (m_WebMode) return true;

	return SocketAPI::getsocketnonblocking_ex( m_SocketID );
	
	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// set socket's nonblocking status
//////////////////////////////////////////////////////////////////////
void SocketImpl::setNonBlocking ( bool on )
{
	__BEGIN_TRY
	if (m_WebMode) return;

	SocketAPI::setsocketnonblocking_ex( m_SocketID , on );
	
	__END_CATCH
}
	

//////////////////////////////////////////////////////////////////////
// get receive buffer size
//////////////////////////////////////////////////////////////////////
uint SocketImpl::getReceiveBufferSize () const
{
	__BEGIN_TRY
	if (m_WebMode) return static_cast<uint>(NetworkTransport::WebSocketTransport::MaxBufferedBytes);

	uint ReceiveBufferSize;
	uint size = sizeof(ReceiveBufferSize);

	SocketAPI::getsockopt_ex( m_SocketID , SOL_SOCKET , SO_SNDBUF , &ReceiveBufferSize, &size );
	
	return ReceiveBufferSize;
	
	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// set receive buffer size
//////////////////////////////////////////////////////////////////////
void SocketImpl::setReceiveBufferSize ( uint ReceiveBufferSize )
{
	__BEGIN_TRY
	if (m_WebMode) return;

	SocketAPI::setsockopt_ex( m_SocketID , SOL_SOCKET , SO_SNDBUF , &ReceiveBufferSize, sizeof(ReceiveBufferSize) );
	
	__END_CATCH
}
	
//////////////////////////////////////////////////////////////////////
// get send buffer size
//////////////////////////////////////////////////////////////////////
uint SocketImpl::getSendBufferSize () const
{
	__BEGIN_TRY
	if (m_WebMode) return static_cast<uint>(NetworkTransport::WebSocketTransport::MaxBufferedBytes);
		
	uint SendBufferSize;
	uint size = sizeof(SendBufferSize);

	SocketAPI::getsockopt_ex( m_SocketID , SOL_SOCKET , SO_SNDBUF , &SendBufferSize, &size );
	
	return SendBufferSize;
	
	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// void SocketImpl::setSendBufferSize ( uint SendBufferSize )
//
// set send buffer size
//
// Parameters
//
// Return
//
// Exceptions
//     Error from setsockopt_ex()
//
//////////////////////////////////////////////////////////////////////
void SocketImpl::setSendBufferSize ( uint SendBufferSize )
{
	__BEGIN_TRY
	if (m_WebMode) return;
		
	SocketAPI::setsockopt_ex( m_SocketID , SOL_SOCKET , SO_SNDBUF , &SendBufferSize, sizeof(SendBufferSize) );
	
	__END_CATCH
}
	

//////////////////////////////////////////////////////////////////////
//
// string SocketImpl::_getHost () const;
//
// get host address of this socket
//
// Parameters
//     none
//
// Return
//     host string
//
// Exceptions
//     none
//
//////////////////////////////////////////////////////////////////////
std::string SocketImpl::_getHost () const 
{
	return std::string( inet_ntoa( m_SockAddr.sin_addr ) );
}


//////////////////////////////////////////////////////////////////////
//
// void SocketImpl::_setHost ( const std::string & host )
//
// set host address of this socket
//
// Parameters
//     host - peer host's address
//
// Return
//     none
//
// Exceptions
//     none
//
//////////////////////////////////////////////////////////////////////
void SocketImpl::_setHost ( const std::string & host ) 
     noexcept
{
	m_SockAddr.sin_addr.s_addr = inet_addr( host.c_str() );
}
			    

//////////////////////////////////////////////////////////////////////
//
// uint SocketImpl::_getPort () const
//
// get port of this socket
//
// Parameters
//     none
//
// Return
//     port
//
// Exceptions
//     none
//
//////////////////////////////////////////////////////////////////////
uint SocketImpl::_getPort () const 
     noexcept
{
	return ntohs( m_SockAddr.sin_port );
}


//////////////////////////////////////////////////////////////////////
//
// void SocketImpl::_setPort ( uint port )
//
// set host address of this socket
//
// Parameters
//     port - peer host's port
//
// Return
//     none
//
// Exceptions
//     none
//
//////////////////////////////////////////////////////////////////////
void SocketImpl::_setPort ( uint port ) 
	 noexcept
{
	m_SockAddr.sin_port = htons(port);
}
void SocketImpl::EnData(char* pData,uint len) 
	noexcept
{
	for (uint i=0;i<len;i++)
	{
		pData[i]=(pData[i] ^ m_key);
		pData[i]=((pData[i] >> 6) & 3) ^ (pData[i] << 2);
	}
	m_key++;
	if (m_key >= 100)
	{
		m_key = 1;
	}
}
