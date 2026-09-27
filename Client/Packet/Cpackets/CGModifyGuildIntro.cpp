//////////////////////////////////////////////////////////////////////////////
// Filename    : CGModifyGuildIntro.cpp 
// Written By  : 
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGModifyGuildIntro.h"


void CGModifyGuildIntro::read (SocketInputStream & iStream)
{
	__BEGIN_TRY

	BYTE szGuildIntro;

	iStream.read( m_GuildID );
	iStream.read( szGuildIntro );

	if ( szGuildIntro > 0 )
		iStream.read( m_GuildIntro, szGuildIntro );
	else
		m_GuildIntro = "";

	__END_CATCH
}

void CGModifyGuildIntro::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY

	// Cap the std::string's own size, before narrowing to the length byte.
	if ( m_GuildIntro.size() > 255 )
		throw InvalidProtocolException( "too long szGuildIntro length" );

	const BYTE szGuildIntro = (BYTE)m_GuildIntro.size();

	oStream.write( m_GuildID );
	oStream.write( szGuildIntro );

	if ( szGuildIntro > 0 )
		oStream.write( std::span<const char>(m_GuildIntro.data(), szGuildIntro) );

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
std::string CGModifyGuildIntro::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
    msg << "CGModifyGuildIntro("
		<< "GuildID:" << (int)m_GuildID
		<< "GuildIntro:" << m_GuildIntro
		<< ")";
	return msg.toString();

	__END_CATCH
}
#endif