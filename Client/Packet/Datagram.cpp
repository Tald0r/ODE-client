//////////////////////////////////////////////////////////////////////
//
// Filename    : Datagram.cpp
// Written By  : reiot@ewestsoft.com
// Description : 
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Datagram.h"
#include "PacketAssert.h"
#include "PacketFactoryManager.h"
#include "DatagramPacket.h"
#include "Packet.h"
#include "PacketDiagnostics.h"
#include "PacketValidator.h"
#include "PlayerStatus.h"

#include <memory>

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
Datagram::Datagram () 
: m_Length(0), m_InputOffset(0), m_OutputOffset(0), m_Data(NULL) 
{
	__BEGIN_TRY

	memset( &m_SockAddr , 0 , sizeof(m_SockAddr) );
	m_SockAddr.sin_family = AF_INET;

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
Datagram::~Datagram () 
{ 

	if ( m_Data != NULL ) {
		delete [] m_Data; 
		m_Data = NULL;
	}

}


//////////////////////////////////////////////////////////////////////
// 내부 버퍼에 들어있는 내용을 외부 버퍼로 복사한다.
//////////////////////////////////////////////////////////////////////
void Datagram::read ( char * buf , uint len )
{
	// The bound is checked before the span exists: a span over [buf,
	// buf + len) with a len the buffer cannot hold is not a valid range.
	ensureReadable( len );
	read( std::span<char>( buf , len ) );
}

//////////////////////////////////////////////////////////////////////
// The two bounds, in the one form that is safe whatever the offsets
// hold: neither half can wrap, and an offset past the length - which
// nothing produces, but nothing enforces either - refuses rather than
// admits. The old `offset + len > length` wrapped for a len near
// UINT_MAX and passed (code-health review, Medium).
//////////////////////////////////////////////////////////////////////
void Datagram::ensureReadable ( uint len ) const
{
	if ( len > m_Length || m_InputOffset > m_Length - len )
		throw InsufficientDataException("Datagram read");
}

void Datagram::ensureWritable ( uint len ) const
{
	// A runtime check in every build, where an Assert vanished under
	// NDEBUG and let a body outgrow its buffer on the heap.
	if ( len > m_Length || m_OutputOffset > m_Length - len )
		throw Error("Datagram write past the end of the buffer");
}

//////////////////////////////////////////////////////////////////////
// read raw bytes into a bounded destination: the one read every other
// read() reaches, and the one place the bound is checked.
//////////////////////////////////////////////////////////////////////
void Datagram::read ( std::span<char> buf )
{
	__BEGIN_TRY

	const uint len = (uint)buf.size();

	ensureReadable( len );

	memcpy( buf.data() , &m_Data[m_InputOffset] , len );

	m_InputOffset += len;

	__END_CATCH
}

void Datagram::read ( std::span<std::byte> buf )
{
	read( std::span<char>( reinterpret_cast<char*>( buf.data() ) , buf.size() ) );
}


//////////////////////////////////////////////////////////////////////
// 내부 버퍼에 들어있는 내용을 외부 스트링으로 복사한다.
//////////////////////////////////////////////////////////////////////
void Datagram::read ( std::string & str , uint len )
{
	__BEGIN_TRY

	ensureReadable( len );

	str.reserve(len);
	str.assign( &m_Data[m_InputOffset] , len );

	m_InputOffset += len;

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// Rebuild the DatagramPacket this datagram carries.
//
// A datagram is one packet, whole: the header, the body its size field
// declares and the one-byte pad both peers count in the length. That
// holds only if (1) two packets from the same address come back from
// separate recvfrom() calls and (2) a packet arrives in one piece,
// which a large enough DatagramSocket buffer makes likely; a datagram
// of any other length is refused below.
//
// Only an id the client accepts over UDP is created: the validator's
// CPS_CLIENT_COMMUNICATION_NORMAL set, which is the four RC packets
// that are DatagramPackets. The factory registers every packet the
// client receives on any connection, and this path used to create
// whatever the id named and C-cast it to DatagramPacket*, so a GC, LC
// or CR id had read(Datagram&) called through an object that is not a
// DatagramPacket - past the end of its vtable. The conversion is
// checked as well, so a set that admits a non-datagram id refuses it.
//
// On any throw pPacket is left NULL: the packet is owned here until
// its read has succeeded, and only then handed to the caller.
//
//////////////////////////////////////////////////////////////////////
void Datagram::read ( DatagramPacket * & pPacket )
{
	__BEGIN_TRY

	Assert( pPacket == NULL );

	PacketID_t packetID;
	PacketSize_t packetSize;

	// initialize packet header
	readWire( packetID );
	readWire( packetSize );

	#ifdef __DEBUG_OUTPUT__
		cout << "DatagramPacket I  D : " << packetID;
	#endif

	// An id past the table.
	if ( packetID >= Packet::PACKET_MAX )
	{
		throw InvalidProtocolException("invalid packet id(datagram)");
	}

	// An id the client does not accept over UDP, refused before a packet
	// exists. With no validator installed nothing is accepted.
	if ( g_pPacketValidator == NULL
		|| !g_pPacketValidator->isValidPacketID( CPS_CLIENT_COMMUNICATION_NORMAL, packetID ) )
	{
		throw InvalidProtocolException("packet id not accepted over UDP");
	}

	#ifdef __DEBUG_OUTPUT__
		cout << " ( " << g_pPacketFactoryManager->getPacketName(packetID).c_str() << " ) " << endl;
	#endif

	#ifdef __DEBUG_OUTPUT__
		cout << "DatagramPacket Size : " << packetSize << endl;
	#endif

	// A size field over the packet's maximum.
	if ( packetSize > g_pPacketFactoryManager->getPacketMaxSize(packetID) )
	{
		PacketDiagnostics::reportBug("too large PacketSize ID)%d %d/%d", packetID, packetSize, g_pPacketFactoryManager->getPacketMaxSize( packetID ) );
		throw InvalidProtocolException("too large packet size(DataGram)");
	}

	// A datagram shorter than the packet it declares.
	if ( m_Length < szPacketHeader + packetSize )
		throw Error("datagram shorter than the packet it declares: not read whole");

	// A datagram longer than the packet it declares.
	if ( m_Length > szPacketHeader + packetSize )
		throw Error("datagram longer than the packet it declares: several read at once");

	// Create the packet, and hold it until it has read.
	std::unique_ptr<Packet> pCreated( g_pPacketFactoryManager->createPacket( packetID ) );

	Assert( pCreated != nullptr );

	DatagramPacket * pDatagramPacket = dynamic_cast<DatagramPacket*>( pCreated.get() );

	if ( pDatagramPacket == NULL )
		throw InvalidProtocolException("packet is not a datagram packet");

	// Read the body into it.
	pDatagramPacket->read( *this );

	// Record the sender's address and port on the packet.
	pDatagramPacket->setHost( getHost() );
	pDatagramPacket->setPort( getPort() );

	pCreated.release();
	pPacket = pDatagramPacket;

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
// 외부 버퍼에 들어있는 내용을 내부 버퍼로 복사한다.
//////////////////////////////////////////////////////////////////////
void Datagram::write ( const char * buf , uint len )
{
	// See read(char*, uint): the bound comes before the span.
	ensureWritable( len );
	write( std::span<const char>( buf , len ) );
}

//////////////////////////////////////////////////////////////////////
// write raw bytes from a bounded source: the one write every other
// write() reaches, and the one place the bound is checked.
//////////////////////////////////////////////////////////////////////
void Datagram::write ( std::span<const char> buf )
{
	__BEGIN_TRY

	const uint len = (uint)buf.size();

	ensureWritable( len );

	memcpy( &m_Data[m_OutputOffset] , buf.data() , len );

	m_OutputOffset += len;

	__END_CATCH
}

void Datagram::write ( std::span<const std::byte> buf )
{
	write( std::span<const char>( reinterpret_cast<const char*>( buf.data() ) , buf.size() ) );
}


//////////////////////////////////////////////////////////////////////
// 외부 스트링에 들어있는 내용을 내부 버퍼로 복사한다.
//
// *CAUTION*
//
// Every write() goes through write(std::span<const char>), which
// advances m_OutputOffset and checks the bound, so neither is done here.
//
//////////////////////////////////////////////////////////////////////
void Datagram::write ( const std::string & str )
{
	__BEGIN_TRY

	// write std::string body
	write( std::span<const char>( str.data() , str.size() ) );

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// write packet
//
// 패킷의 바이너리 이미지를 데이터그램으로 집어넣는다.
// 패킷을 전송하는 쪽에서 이 메쏘드를 호출하며, 이 상태에서 데이터그램의
// 내부 버퍼는 NULL 이어야 한다. 즉 이 메쏘드를 호출할 때 버퍼가 할당
// 되어야 한다.
//
//////////////////////////////////////////////////////////////////////
void Datagram::write ( const DatagramPacket * pPacket )
{
	__BEGIN_TRY

	Assert( pPacket != NULL );

	PacketID_t packetID = pPacket->getPacketID();
	PacketSize_t packetSize = pPacket->getPacketSize();

	// Size the buffer for the declared body and the pad behind it.
	setData( szPacketHeader + packetSize );

	// Write the packet header. The sequence slot that follows the size
	// on the stream is not written here; it is the zero pad setData left
	// behind the body.
	writeWire( packetID );
	writeWire( packetSize );

	// Write the packet body, and hold it to the size the header declared.
	// The buffer has one byte more than the body - the pad - so the
	// bound alone would let a body one byte over its declaration eat the
	// pad and go out looking honest; a body that writes less would send
	// zeros the peer parses as fields. Either is the packet class
	// disagreeing with itself, and neither goes on the wire.
	const uint bodyStart = m_OutputOffset;
	pPacket->write( *this );
	if ( m_OutputOffset - bodyStart != packetSize )
		throw Error("datagram body disagrees with the size its packet declares");

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// set data
//
// 데이터그램소켓에서 읽어들인 데이터를 내부버퍼에 복사한다.
//
//////////////////////////////////////////////////////////////////////
void Datagram::setData ( char * data , uint len )
{ 
	__BEGIN_TRY

	Assert( data != NULL && m_Data == NULL );

	m_Length = len; 
	m_Data = new char[m_Length]; 
	memcpy( m_Data , data , m_Length ); 

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void Datagram::setData ( uint len )
{
	__BEGIN_TRY

	Assert( m_Data == NULL );

	// Zero-filled: write(const DatagramPacket*) sizes the buffer at
	// szPacketHeader + body but writes one byte less, and that pad goes
	// on the wire. The server writes it as zero for the same reason.
	m_Length = len;
	m_Data = new char[ m_Length ]();

	__END_CATCH
}
	

//////////////////////////////////////////////////////////////////////
// set address
//////////////////////////////////////////////////////////////////////
void Datagram::setAddress ( SOCKADDR_IN * pSockAddr )
{ 
	__BEGIN_TRY

	Assert( pSockAddr != NULL );

	memcpy( &m_SockAddr , pSockAddr , szSOCKADDR_IN ); 

	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
// get debug std::string
//////////////////////////////////////////////////////////////////////
std::string Datagram::toString () const
{
	StringStream msg;
	msg << "Datagram("
		<< "Length:" << m_Length
		<< ",InputOffset:" << m_InputOffset
		<< ",OutputOffset:" << m_OutputOffset
		<< ")";
	return msg.toString();
}
