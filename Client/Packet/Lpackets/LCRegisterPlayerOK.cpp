//////////////////////////////////////////////////////////////////////
// 
// Filename    : LCRegisterPlayerOK.cpp
// Written By  : Reiot
// Description : 
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "LCRegisterPlayerOK.h"

//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
void LCRegisterPlayerOK::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY

	BYTE szGroupName;
	iStream.read( szGroupName );
	iStream.read( m_GroupName, szGroupName );
	iStream.read( m_isAdult );

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
void LCRegisterPlayerOK::write ( SocketOutputStream & oStream ) const
{
	__BEGIN_TRY

	// GroupName: 1..20 bytes, what getPacketMaxSize() budgets and the
	// server's writer enforces. The cap runs on the std::string's own
	// size, before the narrowing to the BYTE that goes on the wire.
	if ( m_GroupName.size() > 20 )
		throw InvalidProtocolException("too large GroupName length");

	const BYTE szGroupName = static_cast<BYTE>(m_GroupName.size());

	if ( szGroupName == 0 )
		throw InvalidProtocolException("szGroupName == 0");

	oStream.write( szGroupName );
	oStream.write( std::span<const char>( m_GroupName.data(), szGroupName ) );
	oStream.write( m_isAdult );

	__END_CATCH
}

