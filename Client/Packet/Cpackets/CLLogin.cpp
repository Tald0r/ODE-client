//////////////////////////////////////////////////////////////////////
// 
// Filename    : CLLogin.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "CLLogin.h"

#include <span>


//////////////////////////////////////////////////////////////////////
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void CLLogin::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY

	BYTE szID;

	iStream.read( szID );

	if ( szID == 0 )
		throw InvalidProtocolException("szID == 0");

	if ( szID > 30 )
		throw InvalidProtocolException("too large ID length");

	iStream.read( m_ID , szID );

	BYTE szPassword;

	iStream.read( szPassword );

	if ( szPassword == 0 )
		throw InvalidProtocolException("szPassword == 0");

	if ( szPassword > 20 )
		throw InvalidProtocolException("too large password length");

	iStream.read( m_Password , szPassword );

	// The whole array, its extent from its declaration.
	iStream.read( std::as_writable_bytes( std::span( m_MacAddress ) ) );

	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void CLLogin::write ( SocketOutputStream & oStream ) const
{
	__BEGIN_TRY

	if( !m_bNetmarble )
	{
		// Cap both std::strings' own sizes, before narrowing to the
		// length bytes.
		if ( m_ID.size() > 30 )
			throw InvalidProtocolException("too large ID length");

		const BYTE szID = (BYTE)m_ID.size();

		if ( szID == 0 )
			throw InvalidProtocolException("empty ID");

		oStream.write( szID );

		oStream.write( std::span<const char>(m_ID.data(), szID) );

		if ( m_Password.size() > 20 )
			throw InvalidProtocolException("too large password length");

		const BYTE szPassword = (BYTE)m_Password.size();

		if ( szPassword == 0 )
			throw InvalidProtocolException("szPassword == 0");

		oStream.write( szPassword );

		oStream.write( std::span<const char>(m_Password.data(), szPassword) );

		oStream.write( std::as_bytes( std::span( m_MacAddress ) ) );
		
		BYTE loginMode = 0;
		// add by Coffee 2006.11.7 藤속貢籃되쩍친駕
		oStream.write( loginMode );
		// end by Coffee
		//oStream.write( m_LoginMode );
	}
	else
	{
		int	szID = static_cast<int>(m_ID.size());

		if ( szID == 0 )
			throw InvalidProtocolException("empty ID");
		if ( szID > 2048 )
			throw InvalidProtocolException("too large ID length");

		oStream.write( szID );
		oStream.write( m_ID );
	}

	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
// get debug std::string
//////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
	std::string CLLogin::toString () const
	{
		char szTemp[10];

		StringStream msg;

		msg << "CLLogin("
			<< "ID:" << m_ID 
			<< ",Password:" << m_Password 
			<< ",MacAddress:"
			<< itoa( m_MacAddress[0], szTemp, 16 )
			<< itoa( m_MacAddress[1], szTemp, 16 )
			<< itoa( m_MacAddress[2], szTemp, 16 )
			<< itoa( m_MacAddress[3], szTemp, 16 )
			<< itoa( m_MacAddress[4], szTemp, 16 )
			<< itoa( m_MacAddress[5], szTemp, 16 )
			<< ")";

		return msg.toString();
	}

#endif

PacketSize_t CLLogin::getPacketSize () const
{
	if( !m_bNetmarble )
	{
		return static_cast<PacketSize_t>(szBYTE + m_ID.size() + szBYTE + m_Password.size() + 6 + szBYTE);
	}
	
	return static_cast<PacketSize_t>(szint + m_ID.size());
}