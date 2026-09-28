//////////////////////////////////////////////////////////////////////
// 
// Filename    : LGIncomingConnection.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "LGIncomingConnection.h"


//////////////////////////////////////////////////////////////////////
// Datagram 객체로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void LGIncomingConnection::read ( Datagram & iDatagram )
{
	__BEGIN_TRY

	//--------------------------------------------------
	// read player id 
	//--------------------------------------------------
	BYTE szPlayerID;

	iDatagram.read( szPlayerID );

	if ( szPlayerID == 0 )
		throw InvalidProtocolException("szPlayerID == 0");

	if ( szPlayerID > 20 )
		throw InvalidProtocolException("too long name length");

	iDatagram.read( m_PlayerID , szPlayerID );

	//--------------------------------------------------
	// read creature's name
	//--------------------------------------------------
	BYTE szPCName;

	iDatagram.read( szPCName );

	if ( szPCName == 0 )
		throw InvalidProtocolException("szPCName == 0");

	if ( szPCName > 20 )
		throw InvalidProtocolException("too long name length");

	iDatagram.read( m_PCName , szPCName );

	//--------------------------------------------------
	// read client IP
	//--------------------------------------------------
	BYTE szClientIP;

	iDatagram.read( szClientIP );

	if ( szClientIP == 0 )
		throw InvalidProtocolException("szClientIP == 0");

	if ( szClientIP > 15 )
		throw InvalidProtocolException("too long IP length");

	iDatagram.read( m_ClientIP , szClientIP );

	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// Datagram 객체로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void LGIncomingConnection::write ( Datagram & oDatagram ) const
{
	__BEGIN_TRY

	//--------------------------------------------------
	// write player id
	//--------------------------------------------------
	// Each cap runs on the std::string's own size, before the narrowing
	// to the BYTE that goes on the wire: 257 bytes narrow to 1, and a
	// check on the BYTE would pass them all behind that length byte.
	if ( m_PlayerID.size() > 20 )
		throw InvalidProtocolException("too long name length");

	const BYTE szPlayerID = static_cast<BYTE>(m_PlayerID.size());

	if ( szPlayerID == 0 )
		throw InvalidProtocolException("szPlayerID == 0");

	oDatagram.write( szPlayerID );

	oDatagram.write( std::span<const char>( m_PlayerID.data(), szPlayerID ) );

	//--------------------------------------------------
	// write PC name
	//--------------------------------------------------
	if ( m_PCName.size() > 20 )
		throw InvalidProtocolException("too long name length");

	const BYTE szPCName = static_cast<BYTE>(m_PCName.size());

	if ( szPCName == 0 )
		throw InvalidProtocolException("szPCName == 0");

	oDatagram.write( szPCName );

	oDatagram.write( std::span<const char>( m_PCName.data(), szPCName ) );

	//--------------------------------------------------
	// write client IP
	//--------------------------------------------------
	if ( m_ClientIP.size() > 15 )
		throw InvalidProtocolException("too long IP length");

	const BYTE szClientIP = static_cast<BYTE>(m_ClientIP.size());

	if ( szClientIP == 0 )
		throw InvalidProtocolException("szClientIP == 0");

	oDatagram.write( szClientIP );

	oDatagram.write( std::span<const char>( m_ClientIP.data(), szClientIP ) );

	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
	std::string LGIncomingConnection::toString () const
	{
		StringStream msg;

		msg << "LGIncomingConnection("
			<< "PlayerID:" << m_PlayerID
			<< ",PCName:" << m_PCName 
			<< ",ClientIP:" << m_ClientIP 
			<< ")";

		return msg.toString();
	}
#endif

