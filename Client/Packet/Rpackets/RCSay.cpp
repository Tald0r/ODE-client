//////////////////////////////////////////////////////////////////////
// 
// Filename    : RCSay.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "RCSay.h"


//////////////////////////////////////////////////////////////////////
// Datagram 객체로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void RCSay::read ( Datagram & iDatagram )
{
	__BEGIN_TRY

	// Name
	BYTE szName;

	iDatagram.read( szName );

	if ( szName == 0 )
		throw InvalidProtocolException("szName == 0");

	if ( szName > 20 )
		throw InvalidProtocolException("too long Name length");

	iDatagram.read( m_Name , szName );

	// message
	BYTE szMessage;

	iDatagram.read( szMessage );

	if ( szMessage == 0 )
		throw InvalidProtocolException("szMessage == 0");

	if ( szMessage > 128 )
		throw InvalidProtocolException("too long message length");

	iDatagram.read( m_Message , szMessage );

	// color
	iDatagram.read( m_Color );

	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// Datagram 객체로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void RCSay::write ( Datagram & oDatagram ) const
{
	__BEGIN_TRY

	// Name
	// Each cap runs on the std::string's own size, before the narrowing
	// to the BYTE that goes on the wire: 257 bytes narrow to 1, and a
	// check on the BYTE would pass them all behind that length byte.
	if ( m_Name.size() > 20 )
		throw InvalidProtocolException("too long Name length");

	const BYTE szName = static_cast<BYTE>(m_Name.size());

	if ( szName == 0 )
		throw InvalidProtocolException("szName == 0");

	oDatagram.write( szName );
	oDatagram.write( std::span<const char>( m_Name.data(), szName ) );

	// message
	if ( m_Message.size() > 128 )
		throw InvalidProtocolException("too long message length");

	const BYTE szMessage = static_cast<BYTE>(m_Message.size());

	if ( szMessage == 0 )
		throw InvalidProtocolException("szMessage == 0");

	oDatagram.write( szMessage );
	oDatagram.write( std::span<const char>( m_Message.data(), szMessage ) );

	// color
	oDatagram.write( m_Color );

	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
	std::string RCSay::toString () const
	{
		StringStream msg;

		msg << "RCSay("
			<< "Name:" << m_Name
			<< "Message:" << m_Message
			<< "Color:" << m_Color
			<< ")";

		return msg.toString();
	}
#endif

