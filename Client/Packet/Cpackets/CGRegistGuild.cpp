//////////////////////////////////////////////////////////////////////////////
// Filename    : CGRegistGuild.cpp 
// Written By  : 
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGRegistGuild.h"


void CGRegistGuild::read (SocketInputStream & iStream)
{
	__BEGIN_TRY
		
	BYTE szGuildName, szGuildIntro;

	iStream.read( szGuildName );

	if ( szGuildName == 0 )
		throw InvalidProtocolException( "szGuildName == 0 " );
	if ( szGuildName > 30 ) 
		throw InvalidProtocolException( "szGuildName > 30" );

	iStream.read( m_GuildName, szGuildName );

	iStream.read( szGuildIntro );

	if ( szGuildIntro != 0 )
		iStream.read( m_GuildIntro, szGuildIntro );
	else
		m_GuildIntro = "";

	__END_CATCH
}

void CGRegistGuild::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY
		
	// Cap both std::strings' own sizes, before narrowing to the length
	// bytes. 255 is the most a length byte can express.
	if ( m_GuildName.size() > 30 )
		throw InvalidProtocolException( "szGuildName > 30" );

	if ( m_GuildIntro.size() > 255 )
		throw InvalidProtocolException( "szGuildIntro > 256" );

	const BYTE szGuildName = (BYTE)m_GuildName.size();
	const BYTE szGuildIntro = (BYTE)m_GuildIntro.size();

	if ( szGuildName == 0 )
		throw InvalidProtocolException( "szGuildName == 0 " );

	oStream.write( szGuildName );
	oStream.write( std::span<const char>(m_GuildName.data(), szGuildName) );
	oStream.write( szGuildIntro );

	if ( szGuildIntro != 0 )
		oStream.write( std::span<const char>(m_GuildIntro.data(), szGuildIntro) );

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
std::string CGRegistGuild::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
    msg << "CGRegistGuild("
		<< "GuildName:" << m_GuildName
		<< "GuildIntro:" << m_GuildIntro
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
#endif