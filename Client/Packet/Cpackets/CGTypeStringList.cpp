//////////////////////////////////////////////////////////////////////////////
// Filename    : CGTypeStringList.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGTypeStringList.h"

CGTypeStringList::CGTypeStringList ()
{
	__BEGIN_TRY
	__END_CATCH
}

CGTypeStringList::~CGTypeStringList ()
{
}

void CGTypeStringList::read (SocketInputStream & iStream)
{
	__BEGIN_TRY

	BYTE num;
		
	iStream.read(m_StringType);
	iStream.read(num);

	for ( BYTE i=0; i<num; i++ )
	{
		std::string temp;
		BYTE szString;
		iStream.read(szString);

		if ( szString == 0 ) throw InvalidProtocolException("String 길이가 0입니다.");
		if ( szString > MAX_STRING_LENGTH ) throw InvalidProtocolException("String 길이가 너무 깁니다.");

		iStream.read(temp, szString);

		addString( temp );
	}
	iStream.read(m_Param);

	__END_CATCH
}

void CGTypeStringList::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY

	oStream.write(m_StringType);

	// Cap before narrowing; the count byte must describe every entry written.
	if ( m_StringList.size() > MAX_STRING_NUM )
		throw InvalidProtocolException("too large string list size");

	const BYTE szList = (BYTE)m_StringList.size();

	oStream.write( szList );

	std::list<std::string>::const_iterator itr = m_StringList.begin();

	for( ; itr != m_StringList.end() ; ++itr )
	{
		// Cap before narrowing; read() above is the contract.
		if ( (*itr).size() > MAX_STRING_LENGTH )
			throw InvalidProtocolException("too large string length");

		const BYTE szString = (BYTE)(*itr).size();

		oStream.write( szString );
		oStream.write( std::span<const char>( (*itr).data(), szString ) );
	}

	oStream.write( m_Param );

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
std::string CGTypeStringList::toString () 
	const
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "CGTypeStringList("
		<< ")";
	return msg.toString();

	__END_CATCH
}
#endif