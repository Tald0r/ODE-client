//////////////////////////////////////////////////////////////////////
// 
// SocketOutputStream.cpp
// 
// by Reiot
// 
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////
// include files
//////////////////////////////////////////////////
#include "Client_PCH.h"
#include "SocketOutputStream.h"
#include "PacketAssert.h"
#include "Packet.h"

#include <algorithm>
#include <cstdint>

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
SocketOutputStream::SocketOutputStream ( Socket * sock , uint BufferLen )
: m_Socket(sock), m_Buffer(NULL), m_BufferLen(BufferLen), m_Head(0), m_Tail(0) ,m_Sequence(0)
{
	__BEGIN_TRY

	Assert( m_Socket != NULL );
	if (m_BufferLen < 2 || m_BufferLen > MaxSocketOutputBufferSize)
		throw IOException("invalid socket output buffer size");
	
	m_Buffer = new char[ m_BufferLen ];

	__END_CATCH
}
	

//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
SocketOutputStream::~SocketOutputStream () noexcept(false)
{
	__BEGIN_TRY

	if ( m_Buffer != NULL ) {
		// 연결이 끊겨서 ConnectException 을 받아 종료된 상태에서
		// flush를 할 경우 SIGPIPE 가 난다. 따라서, 무시하자~
		// flush();
		delete [] m_Buffer;
		m_Buffer = NULL;
	}
	
	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// write data to stream (output buffer)
//
// *Notes*
//
// ( ( m_Head = m_Tail + 1 ) ||  
//   ( ( m_Head == 0 ) && ( m_Tail == m_BufferLen - 1 ) )
//
// 일 때 버퍼 full 로 간주한다는 것을 잊지 말라. 따라서, 버퍼의 빈
// 공간의 크기는 항상 1 을 빼줘야 한다는 사실!
//
//////////////////////////////////////////////////////////////////////
uint SocketOutputStream::write ( const char * buf , uint len )
{
	if ( len == 0 )
		return 0;
	Assert( buf != NULL );
	return write(std::span<const char>(buf, len));
}

//////////////////////////////////////////////////////////////////////
// write data from a bounded source
//////////////////////////////////////////////////////////////////////
uint SocketOutputStream::write ( std::span<const char> buf )
{
	__BEGIN_TRY

	if ( buf.empty() )
		return 0;
	const uint queued = length();
	if (buf.size() > MaxSocketOutputBufferSize - queued - 1)
		throw InvalidProtocolException("socket output queue exceeds its byte budget");

	const uint len = static_cast<uint>(buf.size());

	// 현재 버퍼의 빈 영역을 계산한다.
	uint nFree = ( ( m_Head <= m_Tail ) ?  m_BufferLen - m_Tail + m_Head - 1 : m_Head - m_Tail - 1 );
	//m_Tail - m_Head - 1 );

	// 쓸려고 하는 데이타의 크기가 빈 영역의 크기를 초과할 경우 버퍼를 증가시킨다.
	if (len > nFree) {
		const uint required = queued + len + 1;
		const uint grown = (std::min)(MaxSocketOutputBufferSize, m_BufferLen * 2);
		const uint capacity = (std::max)(required, grown);
		resize(static_cast<int>(capacity - m_BufferLen));
	}
		
	if ( m_Head <= m_Tail ) {		// normal order

		//
		//    H   T
		// 0123456789
		// ...abcd...
		//
		
		if ( m_Head == 0 ) {
			
			nFree = m_BufferLen - m_Tail - 1;
			memcpy( &m_Buffer[m_Tail] , buf.data() , len );

		} else {

			nFree = m_BufferLen - m_Tail;
			if ( len <= nFree )
				memcpy( &m_Buffer[m_Tail] , buf.data() , len );
			else {
				memcpy( &m_Buffer[m_Tail] , buf.data() , nFree );
				memcpy( m_Buffer , &buf[nFree] , len - nFree );
			}

		}

	} else {						// reversed order

		//
		//     T  H
		// 0123456789
		// abcd...efg
		//
		
		memcpy( &m_Buffer[m_Tail] , buf.data() , len );

	}
	
	// advance m_Tail
	m_Tail = ( m_Tail + len ) % m_BufferLen;
		
	return len;
	
	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
// write raw bytes from a bounded source
//////////////////////////////////////////////////////////////////////
uint SocketOutputStream::write ( std::span<const std::byte> buf )
{
	return write(std::span<const char>(
		reinterpret_cast<const char*>(buf.data()), buf.size()));
}
	

//////////////////////////////////////////////////////////////////////
// write packet to stream (output buffer)
//////////////////////////////////////////////////////////////////////
void SocketOutputStream::write ( const Packet * pPacket )
{
	__BEGIN_TRY

	// The framing header goes into the ring before the body, and the body
	// write can throw; roll both back so the ring never keeps a header
	// announcing a body that never followed. The saved LENGTH, not the
	// saved tail: a resize() during the body write moves the retained
	// bytes, and the length is what survives that.
	const uint savedLength = length();
	const BYTE savedSequence = m_Sequence;

	try {

		// First write the packet id and the packet size to the output buffer.
		const PacketID_t packetID = pPacket->getPacketID();
		writeWire( packetID );

		const PacketSize_t packetSize = pPacket->getPacketSize();
		writeWire( packetSize );

		// then the sequence number
		writeWire( m_Sequence );
		m_Sequence++;

		#ifdef __DEBUG_OUTPUT__
			printf("%s:%d SocketOutputStream::write packetID: %d, packetSZ: %d sequence %d\n",
				__FILE__, __LINE__,
				packetID, packetSize, m_Sequence-1);
		#endif

		// Now write the packet body to the output buffer.
		pPacket->write( *this );

	} catch ( ... ) {

		m_Tail = ( m_Head + savedLength ) % m_BufferLen;
		m_Sequence = savedSequence;
		throw;

	}

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// flush stream (output buffer) to socket
//////////////////////////////////////////////////////////////////////
uint SocketOutputStream::flush () 
{
	__BEGIN_TRY

	uint nFlushed = 0;
	uint nSent;
	uint nLeft;
	
	try {

		if ( m_Head < m_Tail ) {
		
			//
			//    H   T
			// 0123456789
			// ...abcd...
			//
	
			nLeft = m_Tail - m_Head;

			while ( nLeft > 0 ) {
				nSent = m_Socket->send( &m_Buffer[m_Head] , nLeft , 0 );
				nFlushed += nSent;
				nLeft -= nSent;
				m_Head += nSent;
			}
	
		} else if ( m_Head > m_Tail ) {
	
			//
			//     T  H
			// 0123456789
			// abcd...efg
			//
			
			nLeft = m_BufferLen - m_Head;

			while ( nLeft > 0 ) {
				nSent = m_Socket->send( &m_Buffer[m_Head] , nLeft , 0 );
				nFlushed += nSent;
				nLeft -= nSent;
				m_Head += nSent;
			}
			
			Assert( m_Head == m_BufferLen );
			
			m_Head = 0;
			
			nLeft = m_Tail;

			while ( nLeft > 0 ) {
				nSent = m_Socket->send( &m_Buffer[m_Head] , nLeft , 0 );
				nFlushed += nSent;
				nLeft -= nSent;
				m_Head += nSent;
			}
	
		}
	
		Assert( m_Head == m_Tail );
//		if ( m_Head != m_Tail ) 
//			g_pLogManager->log1( "m_Head = %d , m_Tail = %d\n", m_Head, m_Tail );
		//m_Head = m_Tail = 0;
		
	} catch ( NonBlockingIOException & ) {

		// The socket took what it could and refused the rest. m_Head names
		// the first byte the peer has not received, so the ring is left
		// holding exactly the remainder for the next flush; dropping it
		// would cut the peer's frame mid-packet.

	}

	// Only an emptied ring is normalised back to offset zero.
	if ( m_Head == m_Tail )
		m_Head = m_Tail = 0;

	return nFlushed;

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// resize buffer
//////////////////////////////////////////////////////////////////////
void SocketOutputStream::resize ( int size )
{
	__BEGIN_TRY
		
	if (size == 0)
		return;
	const std::int64_t requested = static_cast<std::int64_t>(m_BufferLen) + size;
	const uint len = length();
	// Keep the sentinel slot, reject signed underflow and bound allocation.
	if (requested < 2 || requested <= len || requested > MaxSocketOutputBufferSize)
		throw IOException("invalid socket buffer size");
	const uint newBufferLen = static_cast<uint>(requested);

	char * newBuffer = new char[ newBufferLen ];
		
	// 원래 버퍼의 내용을 복사한다.
	if ( m_Head < m_Tail ) {

		//
		//    H   T
		// 0123456789
		// ...abcd...
		//

		memcpy( newBuffer , &m_Buffer[m_Head] , m_Tail - m_Head );

	} else if ( m_Head > m_Tail ) {

		//
		//     T  H
		// 0123456789
		// abcd...efg
		//
		 
		memcpy( newBuffer , &m_Buffer[m_Head] , m_BufferLen - m_Head );
		memcpy( &newBuffer[ m_BufferLen - m_Head ] , m_Buffer , m_Tail );

	}
		
	// 원래 버퍼를 삭제한다.
	delete [] m_Buffer;
		
	// 버퍼 및 버퍼 크기를 재설정한다.
	m_Buffer = newBuffer;
	m_BufferLen = newBufferLen;
	m_Head = 0;
	m_Tail = len;	

	#ifdef __DEBUG_OUTPUT__
		ofstream ofile("buffer_resized.log",ios::app);
		ofile << "SocketOutputStream resized " << size << " bytes!" << endl;
		ofile.close();
	#endif

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get data's size in buffer
//////////////////////////////////////////////////////////////////////
uint SocketOutputStream::length () const
     noexcept
{
    if ( m_Head < m_Tail )
        return m_Tail - m_Head;
	 
    else if ( m_Head > m_Tail )
        return m_BufferLen - m_Head + m_Tail;
			 
    return 0;
}
