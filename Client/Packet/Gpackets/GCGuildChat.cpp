//////////////////////////////////////////////////////////////////////
// 
// Filename    : GCGuildChat.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "GCGuildChat.h"

//////////////////////////////////////////////////////////////////////
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void GCGuildChat::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY

	iStream.read( m_Type );

	if ( m_Type != 0 )
	{
		// Guild names run up to 30 bytes (CGRegistGuild's cap, and the field
		// width GCOtherGuildName/GCShowGuildInfo declare), so 30 is the bound
		// that rejects garbage without dropping a legitimately-long name. The
		// handler formats it into a fixed buffer with snprintf regardless.
		BYTE szGName;
		iStream.read(szGName);

		if ( szGName > 30 )
			throw InvalidProtocolException("too long guild name length");

		if ( szGName != 0 ) iStream.read( m_SendGuildName, szGName );
	}

	BYTE szSender;

	iStream.read( szSender );

	if ( szSender == 0 )
		throw InvalidProtocolException("szSender == 0");
	if ( szSender > 10 )
		throw InvalidProtocolException("too long sender length");

	iStream.read( m_Sender, szSender );
	iStream.read( m_Color );

	BYTE szMessage;

	iStream.read( szMessage );

	if ( szMessage == 0 )
		throw InvalidProtocolException("szMessage == 0");

	if ( szMessage > 128 )
		throw InvalidProtocolException("too long message length");

	iStream.read( m_Message , szMessage );

	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void GCGuildChat::write ( SocketOutputStream & oStream ) const
{
	__BEGIN_TRY

	oStream.write( m_Type );
	if ( m_Type != 0 )
	{
		// Test the real length before it is narrowed to a BYTE, or a value
		// like 258 wraps to 2 and slips past the bound.
		if ( m_SendGuildName.size() > 30 )
			throw InvalidProtocolException("too long guild name length");

		BYTE szGName = static_cast<BYTE>(m_SendGuildName.size());
		oStream.write( szGName );
		oStream.write( m_SendGuildName );
	}

	BYTE szSender = static_cast<BYTE>(m_Sender.size());

	if ( szSender == 0 )
		throw InvalidProtocolException("szSener == 0");

	if ( szSender > 10 )
		throw InvalidProtocolException("too long sender length");

	oStream.write( szSender );
	oStream.write( m_Sender );
	oStream.write( m_Color );

	BYTE szMessage = static_cast<BYTE>(m_Message.size());

	if ( szMessage == 0 )
		throw InvalidProtocolException("szMessage == 0");

	if ( szMessage > 128 )
		throw InvalidProtocolException("too large message length");

	oStream.write( szMessage );
	oStream.write( m_Message );

	__END_CATCH
}


#ifdef __DEBUG_OUTPUT__
//////////////////////////////////////////////////////////////////////
// get packet's debug std::string
//////////////////////////////////////////////////////////////////////
std::string GCGuildChat::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "GCGuildChat("
		<< "Sener :" << m_Sender
		<< ",Color :" << m_Color
		<< ",Message:" << m_Message 
		<< ")" ;
	return msg.toString();
		
	__END_CATCH
}
#endif

