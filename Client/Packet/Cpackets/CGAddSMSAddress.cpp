//////////////////////////////////////////////////////////////////////////////
// Filename    : CGAddSMSAddress.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGAddSMSAddress.h"

CGAddSMSAddress::CGAddSMSAddress ()
{
	__BEGIN_TRY
	__END_CATCH
}

CGAddSMSAddress::~CGAddSMSAddress ()
{
}

void CGAddSMSAddress::read (SocketInputStream & iStream)
{
	__BEGIN_TRY

	BYTE szSTR;

	iStream.read( szSTR );
	iStream.read( m_CharacterName, szSTR );

	iStream.read( szSTR );
	iStream.read( m_CustomName, szSTR );

	iStream.read( szSTR );
	iStream.read( m_Number, szSTR );

	__END_CATCH
}

void CGAddSMSAddress::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY

	// Cap before narrowing; the length byte must describe every byte written.
	BYTE szSTR;

	if ( m_CharacterName.size() > 20 )
		throw InvalidProtocolException("too large character name length");

	szSTR = (BYTE)m_CharacterName.size();
	oStream.write( szSTR );
	oStream.write( std::span<const char>( m_CharacterName.data(), szSTR ) );

	if ( m_CustomName.size() > 40 )
		throw InvalidProtocolException("too large custom name length");

	szSTR = (BYTE)m_CustomName.size();
	oStream.write( szSTR );
	oStream.write( std::span<const char>( m_CustomName.data(), szSTR ) );

	if ( m_Number.size() > 11 )
		throw InvalidProtocolException("too large number length");

	szSTR = (BYTE)m_Number.size();
	oStream.write( szSTR );
	oStream.write( std::span<const char>( m_Number.data(), szSTR ) );

	__END_CATCH
}

string CGAddSMSAddress::toString () 
	const
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "CGAddSMSAddress("
		<< ")";
	return msg.toString();

	__END_CATCH
}
