//----------------------------------------------------------------------
//
// Filename    : TextInfo.cpp
// Writen By   : beowulf
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "TextInfo.h"

//----------------------------------------------------------------------
// read data from socket input stream
//----------------------------------------------------------------------
void TextInfo::read ( SocketInputStream & iStream ) 
{
	__BEGIN_TRY

	iStream.read( m_ID );

	BYTE szWriter;
	
	iStream.read( szWriter );
	
	if ( szWriter == 0 )
		throw InvalidProtocolException("szWriter == 0" );
	if ( szWriter > 20 )
		throw InvalidProtocolException("too large Writer lenth");
		
	iStream.read( m_Writer , szWriter );

	BYTE szTopic;

	iStream.read ( szTopic );

 	if ( szTopic == 0 )
		throw InvalidProtocolException("szTopic == 0" );
		
	iStream.read( m_Topic , szTopic );

	iStream.read( m_Hit );

	__END_CATCH
}


//----------------------------------------------------------------------
// write data to socket output stream
//----------------------------------------------------------------------
void TextInfo::write ( SocketOutputStream & oStream ) const 
{
	__BEGIN_TRY

	oStream.write( m_ID );

	if ( m_Writer.empty() )
		throw InvalidProtocolException("empty BBS_ID");
	if ( m_Writer.size() > 20 )
		throw InvalidProtocolException("too large Writer length");

	BYTE szWriter = static_cast<BYTE>(m_Writer.size());

	oStream.write( szWriter );
	
	oStream.write( m_Writer );

	if ( m_Topic.empty() )
		throw InvalidProtocolException ("empty BBS_Topic");
	if ( m_Topic.size() > 255 )
		throw InvalidProtocolException ("too large Topic length");

	BYTE szTopic = static_cast<BYTE>(m_Topic.size());

	oStream.write( szTopic );
	
	oStream.write( m_Topic );
	
	oStream.write( m_Hit );

	__END_CATCH
}


//----------------------------------------------------------------------
// get debug string
//----------------------------------------------------------------------
std::string TextInfo::toString () const 
{
	StringStream msg;
	
	msg << "TextInfo("
		<< "ID:" << m_ID 
		<< ",Writer:"<< m_Writer
		<< ",Topic:"<< m_Topic
		<< ",Hit:"<< m_Hit
		<< ")";

	return msg.toString();
}
	
