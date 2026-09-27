//#define __TEST_PACKET_RECEIVED_SIZE_PER_SECOND__

//////////////////////////////////////////////////////////////////////
// 
// SocketInputStream.cpp
// 
// by Reiot
// 
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////
// include files
//////////////////////////////////////////////////
#include "Client_PCH.h"
#include "SocketInputStream.h"
#include <cstdint>
#include "PacketAssert.h"
#include "Packet.h"
#include "MonotonicClock.h"
#include <cstdio>
#include <limits>

#ifdef __TEST_PACKET_RECEIVED_SIZE_PER_SECOND__

MonotonicClock::IntervalTimer	g_ReceivedSizeWindow(MonotonicClock::Millis(1000));
DWORD	g_dwReceiveSize=0;

#endif

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
SocketInputStream::SocketInputStream ( Socket * sock , uint BufferLen )
: m_pSocket(sock), m_Buffer(NULL), m_BufferLen(BufferLen), m_InitialBufferLen(BufferLen), m_Head(0), m_Tail(0),
	m_bFrameBounded(false), m_bFrameReadFailed(false), m_FrameRemaining(0)
{
	__BEGIN_TRY
		
	Assert( m_pSocket != NULL );
	if (m_BufferLen < 2 || m_BufferLen > MaxSocketInputBufferSize)
		throw IOException("invalid socket input buffer size");
	
	m_Buffer = new char[ m_BufferLen ];

	__END_CATCH
}
	

//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
SocketInputStream::~SocketInputStream () noexcept(false)
{
	__BEGIN_TRY
		
	if ( m_Buffer != NULL ) {
		delete [] m_Buffer;
		m_Buffer = NULL;
	}
		
	__END_CATCH
}

	
//////////////////////////////////////////////////////////////////////
//
// read data from input buffer
//
//////////////////////////////////////////////////////////////////////
uint SocketInputStream::read ( char * buf , uint len )
{
	if (m_bFrameBounded && buf == NULL)
		failRead("null packet read destination" );
	Assert( buf != NULL );
	return read(std::span<char>(buf, len));
}

//////////////////////////////////////////////////////////////////////
//
// read data into a bounded destination
//
//////////////////////////////////////////////////////////////////////
uint SocketInputStream::read ( std::span<char> buf )
{
	__BEGIN_TRY
	
	if ( buf.empty() )
		failRead("len==0");
	if ( buf.size() > (std::numeric_limits<uint>::max)() )
		failRead("span is too large");

	const uint len = static_cast<uint>(buf.size());

	if ( m_bFrameBounded && len > m_FrameRemaining )
		failRead("packet parser read past declared body");
	
	// 요청한 만큼의 데이타가 버퍼내에 존재하지 않을 경우 예외를 던진다.
	// 만약 모든 read 가 peek() 로 체크한 후 호출된다면, 아래 if-throw 는 
	// 중복된 감이 있다. 따라서, 코멘트로 처리해도 무방하다.
	// 단 아래 코드를 코멘트처리하면, 바로 아래의 if-else 를 'if'-'else if'-'else'
	// 로 수정해줘야 한다.
	if ( len > length() )
		failUnderflow( len - length() );
	
	if ( m_Head < m_Tail ) {	// normal order

		//
        //    H   T
        // 0123456789
        // ...abcd...
        //

		memcpy( buf.data() , &m_Buffer[m_Head] , len );

	} else {					// reversed order ( m_Head > m_Tail )
		
        //
        //     T  H
        // 0123456789
        // abcd...efg
        //
	 
		uint rightLen = m_BufferLen - m_Head;
		if ( len <= rightLen ) {
			memcpy( buf.data() , &m_Buffer[m_Head] , len );
		} else {
			memcpy( buf.data() , &m_Buffer[m_Head] , rightLen );
			memcpy( &buf[rightLen] , m_Buffer , len - rightLen );
		}

	}

	m_Head = ( m_Head + len ) % m_BufferLen;
	if ( m_bFrameBounded )
		m_FrameRemaining -= len;
/*
	#ifdef __DEBUG_OUTPUT__
		if (len > 0) {
			FILE* fp = fopen("read.log", "a");
			for (int i=0; i< len ; i++) {
				fprintf(fp, " %02x", (unsigned char)buf[i]);
			}
			fclose(fp);
		}
	#endif
*/ 	
	return len;
		
	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
// read a prefix into a bounded destination
//////////////////////////////////////////////////////////////////////
uint SocketInputStream::read ( std::span<char> buf , std::size_t len )
{
	if ( len > buf.size() )
		failRead("read length exceeds destination span");
	if ( len > (std::numeric_limits<uint>::max)() )
		failRead("read length exceeds stream limit");

	return read(buf.first(len));
}

//////////////////////////////////////////////////////////////////////
// read raw bytes into a bounded destination
//////////////////////////////////////////////////////////////////////
uint SocketInputStream::read ( std::span<std::byte> buf )
{
	return read(std::span<char>(reinterpret_cast<char*>(buf.data()), buf.size()));
}

//////////////////////////////////////////////////////////////////////
// read data from input buffer
//////////////////////////////////////////////////////////////////////
uint SocketInputStream::read ( std::string & str , uint len ) 
{
	__BEGIN_TRY
		
	if ( len == 0 )
		failRead("len==0");
	if ( m_bFrameBounded && len > m_FrameRemaining )
		failRead("packet parser read past declared body");
	
	// 요청한 만큼의 데이타가 버퍼내에 존재하지 않을 경우 예외를 던진다.
	// 만약 모든 read 가 peek() 로 체크한 후 호출된다면, 아래 if-throw 는 
	// 중복된 감이 있다. 따라서, 코멘트로 처리해도 무방하다.
	// 단 아래 코드를 코멘트처리하면, 바로 아래의 if-else 를 if-else if-else
	// 로 수정해줘야 한다.
	if ( len > length() )
		failUnderflow( len - length() );
	
	// 스트링에다가 len 만큼 공간을 미리 할당한다.
	str.reserve( len );

	if ( m_Head < m_Tail ) {	// normal order

		//
        //    H   T
        // 0123456789
        // ...abcd...
        //

		str.assign( &m_Buffer[m_Head] , len );

	} else { 					// reversed order ( m_Head > m_Tail )

        //
        //     T  H
        // 0123456789
        // abcd...efg
        //

		uint rightLen = m_BufferLen - m_Head;
		if ( len <= rightLen ) {
			str.assign( &m_Buffer[m_Head] , len );
		} else {
			str.assign( &m_Buffer[m_Head] , rightLen );
			str.append( m_Buffer , len - rightLen );
		}
	}

	// Fix: Ensure string is properly null-terminated by resizing to actual content length
	// This prevents strlen() from reading past the end when c_str() is used
	// Find first null terminator or use entire length
	size_t nullPos = str.find('\0');
	if (nullPos != std::string::npos) {
		str.resize(nullPos);
	}

	m_Head = ( m_Head + len ) % m_BufferLen;
	if ( m_bFrameBounded )
		m_FrameRemaining -= len;

	/*
	#ifdef __DEBUG_OUTPUT__
		if (len > 0) {
			FILE* fp = fopen("read.log", "a");
			const char *buf = str.c_str();
			for (int i=0; i< len ; i++) {
				fprintf(fp, " %02x", (unsigned char)buf[i]);
			}
			fclose(fp);
		}
	#endif
		*/

	return len;
		
	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
// read packet from input buffer
//////////////////////////////////////////////////////////////////////
// Preserve a failed stream operation even if the packet's decoder catches it.
void SocketInputStream::failRead(const char* message)
{
	if (m_bFrameBounded)
		m_bFrameReadFailed = true;
	throw InvalidProtocolException(message);
}

void SocketInputStream::failUnderflow(uint missing)
{
	if (m_bFrameBounded)
		m_bFrameReadFailed = true;
	throw InsufficientDataException(missing);
}

void SocketInputStream::finishFrame() noexcept
{
	// The complete body was preflighted. Discard only its unread bytes, then
	// clear both the bound and the failure state for the following frame.
	m_Head = (m_Head + m_FrameRemaining) % m_BufferLen;
	m_FrameRemaining = 0;
	m_bFrameReadFailed = false;
	m_bFrameBounded = false;
}

void SocketInputStream::read ( Packet * pPacket ) 
{
	__BEGIN_TRY
		
	// The caller selected a packet type from the header. This method owns the
	// complete framed read: it waits for the declared body, consumes the header,
	// bounds the parser to that body, and requires exact body consumption.
	if (m_bFrameBounded && pPacket == NULL)
		failRead("null nested packet" );
	Assert( pPacket != NULL );
	if ( m_bFrameBounded )
		failRead("nested packet read");

	// Refuse a fragmented frame before consuming its header. The connection
	// loops already perform this check, but keeping it here makes the framing
	// operation safe for every caller and preserves the bytes for the next fill.
	char header[szPacketHeader];
	if ( !peek(header, szPacketHeader) )
		throw InsufficientDataException(szPacketHeader - length());

	PacketSize_t packetSize = 0;
	memcpy(&packetSize, &header[szPacketID], szPacketSize);
	const uint buffered = length();
	const uint bufferedBody = buffered - szPacketHeader;
	if ( packetSize > bufferedBody )
		throw InsufficientDataException(packetSize - bufferedBody);

	// The header is outside the body bound. Once it is consumed, reads made by
	// the packet parser may consume exactly packetSize bytes and no more.
	skip( szPacketHeader );
	m_bFrameBounded = true;
	m_bFrameReadFailed = false;
	m_FrameRemaining = packetSize;

	try {
		pPacket->read( *this );

		if (m_bFrameReadFailed)
			failRead("packet parser suppressed a stream failure");
		if (m_FrameRemaining != 0)
			failRead("packet parser did not consume declared body");

		finishFrame();
	} catch (InsufficientDataException&) {
		// The complete body was buffered before parsing. This is a malformed
		// body, not transport fragmentation that a receive loop may retry.
		finishFrame();
		throw InvalidProtocolException("packet parser underflowed declared body");
	} catch (...) {
		finishFrame();
		throw;
	}

//    printf("%s:%d:%s read packet: %d size: %d\n", pPacket->getPacketID(), pPacket->getPacketSize());
	
	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// peek data from buffer
//////////////////////////////////////////////////////////////////////
bool SocketInputStream::peek ( char * buf , uint len )
{
//	__BEGIN_TRY
			
	if (m_bFrameBounded && buf == NULL)
		failRead("null packet read destination" );
	Assert( buf != NULL );	

	if ( len == 0 )
		failRead("len==0");
	if ( m_bFrameBounded && len > m_FrameRemaining )
		failRead("packet parser peeked past declared body");
	
	// 요청한 크기보다 버퍼의 데이타가 적은 경우, 예외를 던진다.
	if ( len > length() ) {
		if (m_bFrameBounded)
			failUnderflow(len - length());
		return false;
	}

	// buf 에 복사는 하되, m_Head 는 변화시키지 않는다.
	if ( m_Head < m_Tail ) {	// normal order

		//
        //    H   T
        // 0123456789
        // ...abcd...
        //

		memcpy( buf , &m_Buffer[m_Head] , len );

	} else { 					// reversed order ( m_Head > m_Tail )
		
        //
        //     T  H
        // 0123456789
        // abcd...efg
        //
	 
		uint rightLen = m_BufferLen - m_Head;
		if ( len <= rightLen ) {
			memcpy( &buf[0]        , &m_Buffer[m_Head] , len );
		} else {
			memcpy( &buf[0]        , &m_Buffer[m_Head] , rightLen );
			memcpy( &buf[rightLen] , &m_Buffer[0]      , len - rightLen );
		}
	}

/*
#ifdef __DEBUG_OUTPUT__
	if (len == szPacketHeader) {
				FILE* fp = fopen("peek.log", "a");
				fprintf(fp, "(pos=%d) ", m_Head);
				for (int i=0; i<len; i++) {
					fprintf(fp, " %02x", (unsigned char)(buf[i]));
				}
				fprintf(fp, "\r\n");
				fclose(fp);
	}
#endif
*/
	
	return true;
//	__END_CATCH
}

	
//////////////////////////////////////////////////////////////////////
//
// skip data from buffer
//
// read(N) == peek(N) + skip(N)
//
//////////////////////////////////////////////////////////////////////
void SocketInputStream::skip ( uint len ) 
{
	__BEGIN_TRY
		
	if ( len == 0 )
		failRead("len==0");
	if ( m_bFrameBounded && len > m_FrameRemaining )
		failRead("packet parser skipped past declared body");
	
	if ( len > length() )
		failUnderflow( len - length() );
	
	// m_Head 를 증가시킨다.

	[[maybe_unused]] uint pos = m_Head;
	m_Head = ( m_Head + len ) % m_BufferLen;
	if ( m_bFrameBounded )
		m_FrameRemaining -= len;

/*
	#ifdef __DEBUG_OUTPUT__
		if (len > 0) {
			FILE* fp = fopen("read.log", "a");
			fprintf(fp, "\r\n pos (%d) ", pos);
			for (uint i=0; i< len; i++) {
				pos = (pos + i) % m_BufferLen;
				fprintf(fp, " %02x", (unsigned char)(m_Buffer[pos]));
			}
			fclose(fp);
		}
	#endif
*/

	__END_CATCH
}
	

//////////////////////////////////////////////////////////////////////
// Fill contiguous portions of the ring. Query the backlog before growing a full
// ring; every receive has space, and would-block preserves this call's progress.
//////////////////////////////////////////////////////////////////////
uint SocketInputStream::fill ()
{
	__BEGIN_TRY

	if (isEmpty() && m_BufferLen > m_InitialBufferLen)
		resize(static_cast<int>(m_InitialBufferLen) - static_cast<int>(m_BufferLen));

#ifdef __TEST_PACKET_RECEIVED_SIZE_PER_SECOND__
	if (g_ReceivedSizeWindow.Fire())
		g_dwReceiveSize = 0;
#endif

	uint filled = 0;
	for (;;)
	{
		if (length() == m_BufferLen - 1)
		{
			const uint available = m_pSocket->available();
			if (available == 0)
				return filled;
			if (available > MaxSocketInputBufferSize - m_BufferLen)
				throw InvalidProtocolException("socket input buffer limit exceeded");
			resize(static_cast<int>(available));
		}

		const uint free = m_Head > m_Tail
			? m_Head - m_Tail - 1
			: m_BufferLen - m_Tail - (m_Head == 0 ? 1 : 0);
		uint received = 0;
		try
		{
			received = m_pSocket->receive(m_Buffer + m_Tail, free);
		}
		catch (const NonBlockingIOException&)
		{
			return filled;
		}
		m_Tail = (m_Tail + received) % m_BufferLen;
		filled += received;
#ifdef __TEST_PACKET_RECEIVED_SIZE_PER_SECOND__
		g_dwReceiveSize += received;
#endif
		if (received < free)
			return filled;
	}

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// resize buffer
//////////////////////////////////////////////////////////////////////
void SocketInputStream::resize ( int size )
{
	__BEGIN_TRY
		
	if (size == 0)
		return;
	const std::int64_t requested = static_cast<std::int64_t>(m_BufferLen) + size;
	const uint len = length();
	// Keep the sentinel slot, reject signed underflow and bound allocation.
	if (requested < 2 || requested <= len || requested > MaxSocketInputBufferSize)
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
	m_Tail = len;	// m_Tail 은 들어있는 데이타의 길이와 같다.

	#ifdef __DEBUG_OUTPUT__
		ofstream ofile("buffer_resized.log",ios::app);
		ofile << "SocketInputStream resized " << size << " bytes!" << endl;
		ofile.close();
	#endif

	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
//
// get data's size in buffer
//
// NOTES
//
//       H   T           T  H
//    0123456789     0123456789
//    ...abcd...     abcd...efg
//
//    7 - 3 = 4      10 - ( 7 - 4 ) = 7
//
// CAUTION
//
//    m_Tail 이 빈 칸을 가리키고 있다는 것에 유의하라. 
//    버퍼의 크기가 m_BufferLen 라면 실제 이 큐에 들어갈 
//    수 있는 데이타는 ( m_BufferLen - 1 ) 이 된다.
//
//////////////////////////////////////////////////////////////////////
uint SocketInputStream::length () const
{
	__BEGIN_TRY

	uint buffered = 0;
	if ( m_Head < m_Tail )
		buffered = m_Tail - m_Head;
	else if ( m_Head > m_Tail )
		buffered = m_BufferLen - m_Head + m_Tail;

	if ( m_bFrameBounded && buffered > m_FrameRemaining )
		return m_FrameRemaining;
	return buffered;

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// get debug string
//////////////////////////////////////////////////////////////////////
std::string SocketInputStream::toString () const
{
	StringStream msg;

	msg << "SocketInputStream("
		<< "BufferLen:" << m_BufferLen
		<< ",Head:" << m_Head
		<< ",Tail:" << m_Tail
		<< ")";

	return msg.toString();
}
