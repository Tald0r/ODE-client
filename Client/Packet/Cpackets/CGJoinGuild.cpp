//////////////////////////////////////////////////////////////////////////////
// Filename    : CGJoinGuild.cpp 
// Written By  : 
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGJoinGuild.h"


void CGJoinGuild::read (SocketInputStream & iStream)
{
	__BEGIN_TRY
		
	BYTE szGuildMemberIntro;

	iStream.read( m_GuildID );
	iStream.read( m_GuildMemberRank );
	iStream.read( szGuildMemberIntro );

	if ( szGuildMemberIntro != 0 )
		iStream.read( m_GuildMemberIntro, szGuildMemberIntro );
	else
		m_GuildMemberIntro = "";

	__END_CATCH
}

void CGJoinGuild::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY

	// Cap the std::string's own size, before narrowing to the length byte.
	// 255 is the most a length byte can express.
	if ( m_GuildMemberIntro.size() > 255 )
		throw InvalidProtocolException( "szGuildMemberIntro > 256" );

	const BYTE szGuildMemberIntro = (BYTE)m_GuildMemberIntro.size();

	oStream.write( m_GuildID );
	oStream.write( m_GuildMemberRank );
	oStream.write( szGuildMemberIntro );

	if ( szGuildMemberIntro != 0 )
		oStream.write( std::span<const char>(m_GuildMemberIntro.data(), szGuildMemberIntro) );

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
std::string CGJoinGuild::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
    msg << "CGJoinGuild("
		<< "GuildMemberRank:" << m_GuildMemberRank
		<< "GuildMemberIntro:" << m_GuildMemberIntro
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
#endif