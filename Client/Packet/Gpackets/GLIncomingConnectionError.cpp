//////////////////////////////////////////////////////////////////////
// 
// Filename    : GLIncomingConnectionError.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "GLIncomingConnectionError.h"


//////////////////////////////////////////////////////////////////////
// Datagram 객체로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void GLIncomingConnectionError::read ( Datagram & iDatagram )
{
	__BEGIN_TRY

	//--------------------------------------------------
	//--------------------------------------------------
	BYTE szMessage;

	iDatagram.read( szMessage );

	if ( szMessage == 0 ) 
		throw InvalidProtocolException("szMessage == 0");

	if ( szMessage >= 80 )
		throw InvalidProtocolException("too large message length");

	iDatagram.read( m_Message , szMessage );


	//--------------------------------------------------
	//--------------------------------------------------
	BYTE szPlayerID;

	iDatagram.read( szPlayerID );

	if ( szPlayerID == 0 ) 
		throw InvalidProtocolException("szPlayerID == 0");

	if ( szPlayerID >= 80 )
		throw InvalidProtocolException("too large playerID length");

	iDatagram.read( m_PlayerID , szPlayerID );


	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// Datagram 객체로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void GLIncomingConnectionError::write ( Datagram & oDatagram ) const
{
	__BEGIN_TRY

	//--------------------------------------------------
	//--------------------------------------------------
	// Each cap runs on the std::string's own size, before the narrowing
	// to the BYTE that goes on the wire: 257 bytes narrow to 1, and a
	// check on the BYTE would pass them all behind that length byte.
	if ( m_Message.size() >= 80 )
		throw InvalidProtocolException("too large message length");

	const BYTE szMessage = static_cast<BYTE>(m_Message.size());

	if ( szMessage == 0 ) 
		throw InvalidProtocolException("szMessage == 0");

	oDatagram.write( szMessage );

	oDatagram.write( std::span<const char>( m_Message.data(), szMessage ) );


	//--------------------------------------------------
	//--------------------------------------------------
	if ( m_PlayerID.size() >= 80 )
		throw InvalidProtocolException("too large playerID length");

	const BYTE szPlayerID = static_cast<BYTE>(m_PlayerID.size());

	if ( szPlayerID == 0 ) 
		throw InvalidProtocolException("szPlayerID == 0");

	oDatagram.write( szPlayerID );

	oDatagram.write( std::span<const char>( m_PlayerID.data(), szPlayerID ) );

	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
	std::string GLIncomingConnectionError::toString () const
	{
		__BEGIN_TRY
			
		StringStream msg;
		msg << "GLIncomingConnectionError("
			<< "Message:" << m_Message 
			<< "PlayerID:" << m_PlayerID
			<< ")";
		return msg.toString();

		__END_CATCH
	}
#endif
