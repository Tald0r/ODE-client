//////////////////////////////////////////////////////////////////////
// 
// Filename    : RCCharacterInfo.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "RCCharacterInfo.h"


RCCharacterInfo::RCCharacterInfo()
{
	m_GuildID = 0;
}

//////////////////////////////////////////////////////////////////////
// Datagram 객체로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void RCCharacterInfo::read ( Datagram & iDatagram )
{
	__BEGIN_TRY

	// Name
	BYTE szName;

	iDatagram.read( szName );

	//if ( szName == 0 )
	//	throw InvalidProtocolException("szName == 0");

	if ( szName > 20 )
		throw InvalidProtocolException("too long Name length");

	if (szName > 0)
	{
		iDatagram.read( m_Name , szName );
	}

	// 
	iDatagram.read( m_GuildID );	

	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// Datagram 객체로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void RCCharacterInfo::write ( Datagram & oDatagram ) const
{
	__BEGIN_TRY

	// Name
	// Each cap runs on the std::string's own size, before the narrowing
	// to the BYTE that goes on the wire: 257 bytes narrow to 1, and a
	// check on the BYTE would pass them all behind that length byte.
	if ( m_Name.size() > 20 )
		throw InvalidProtocolException("too long Name length");

	const BYTE szName = static_cast<BYTE>(m_Name.size());

	//if ( szName == 0 )
	//	throw InvalidProtocolException("szName == 0");

	oDatagram.write( szName );

	if (szName > 0)
	{
		oDatagram.write( std::span<const char>( m_Name.data(), szName ) );
	}

	oDatagram.write( m_GuildID );

	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
	std::string RCCharacterInfo::toString () const
	{
		StringStream msg;
		
		msg << "RCCharacterInfo( "
			<< ",GuildID" << (int)m_GuildID
			<< ")";


		return msg.toString();
	}
#endif

