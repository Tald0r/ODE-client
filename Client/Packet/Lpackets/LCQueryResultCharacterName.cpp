//--------------------------------------------------------------------------------
// 
// Filename    : LCQueryResultCharacterName.cpp 
// Written By  : Reiot
// Description : 
// 
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "LCQueryResultCharacterName.h"


//--------------------------------------------------------------------------------
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//--------------------------------------------------------------------------------
void LCQueryResultCharacterName::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY

	//--------------------------------------------------
	// read player id
	//--------------------------------------------------
	BYTE szCharacterName;

	iStream.read( szCharacterName );

	if ( szCharacterName == 0 )
		throw InvalidProtocolException("szCharacterName == 0");

	if ( szCharacterName > 20 )
		throw InvalidProtocolException("too large CharacterName length");

	iStream.read( m_CharacterName , szCharacterName );

	//--------------------------------------------------
	// read id existence
	//--------------------------------------------------
	iStream.read( m_bExist );

	__END_CATCH
}

		    
//--------------------------------------------------------------------------------
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//--------------------------------------------------------------------------------
void LCQueryResultCharacterName::write ( SocketOutputStream & oStream ) const
{
	__BEGIN_TRY

	//--------------------------------------------------
	// write player id
	//--------------------------------------------------
	// Each cap runs on the std::string's own size, before the narrowing
	// to the BYTE that goes on the wire: 257 bytes narrow to 1, and a
	// check on the BYTE would pass them all behind that length byte.
	if ( m_CharacterName.size() > 20 )
		throw InvalidProtocolException("too large CharacterName length");

	const BYTE szCharacterName = static_cast<BYTE>(m_CharacterName.size());

	if ( szCharacterName == 0 )
		throw InvalidProtocolException("empty CharacterName");

	oStream.write( szCharacterName );

	oStream.write( std::span<const char>( m_CharacterName.data(), szCharacterName ) );

	//--------------------------------------------------
	// write id existence
	//--------------------------------------------------
	oStream.write( m_bExist );

	__END_CATCH
}

//--------------------------------------------------------------------------------
// get debug string
//--------------------------------------------------------------------------------
#ifdef __DEBUG_OUTPUT__
	std::string LCQueryResultCharacterName::toString () const
	{
		__BEGIN_TRY
			
		StringStream msg;
		msg << "LCQueryResultCharacterName("
			<< "CharacterName:" << m_CharacterName 
			<< ",Exist:" << m_bExist 
			<< ")";
		return msg.toString();
			
		__END_CATCH
	}
#endif