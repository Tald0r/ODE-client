//////////////////////////////////////////////////////////////////////
// 
// Filename    : GCSystemMessage.cpp 
// Written By  : reiot@ewestsoft.com
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "GCSystemMessage.h"


//////////////////////////////////////////////////////////////////////
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void GCSystemMessage::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY
		
	BYTE szMessage;

	iStream.read( szMessage );

	if ( szMessage == 0 )
		throw InvalidProtocolException("szMessage == 0");

	// szMessage is a BYTE, so the old "> 256" guard here could never fire.
	// A message of up to 255 bytes is legal; the handler copies it with
	// bounded operations rather than trusting a length cap here.

	iStream.read( m_Message , szMessage );

	iStream.read( m_Color );

	BYTE t;
	iStream.read( t );
	m_Type = (SystemMessageType)t;

	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void GCSystemMessage::write ( SocketOutputStream & oStream ) const
{
	__BEGIN_TRY
		
	BYTE szMessage = m_Message.size();

	oStream.write( szMessage );

	if ( szMessage == 0 )
		throw InvalidProtocolException("szMessage == 0");

	oStream.write( m_Message );

	oStream.write( m_Color );

	BYTE t = (BYTE)m_Type;
	oStream.write( t );

	__END_CATCH
}

//////////////////////////////////////////////////////////////////////
// get packet's debug string
//////////////////////////////////////////////////////////////////////
std::string GCSystemMessage::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
	
	msg << "GCSystemMessage("
		<< "Type:" << (int)m_Type 
		<< ",Color:" << m_Color 
		<< ",Message:" << m_Message 
		<< ")" ;
	
	return msg.toString();
		
	__END_CATCH
}


