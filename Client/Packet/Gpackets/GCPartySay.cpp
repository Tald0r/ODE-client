//////////////////////////////////////////////////////////////////////
// 
// Filename    : GCPartySay.cpp 
// Written By  : reiot@ewestsoft.com
// Description : 
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "GCPartySay.h"


//////////////////////////////////////////////////////////////////////
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void GCPartySay::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY

	// Both lengths arrive as raw BYTEs and must be bounded here: the declared
	// packet max size (20-byte name, 128-byte message) does not constrain the
	// sub-reads, and the handler copies both strings into fixed buffers.
	// The bounds mirror GCSay and GCWhisper.
	BYTE szName;
	iStream.read(szName);

	if (szName > 20)
		throw InvalidProtocolException("too long name length");

	iStream.read(m_Name,szName);
	iStream.read(m_Color);

	BYTE szMessage;
	iStream.read(szMessage);

	if (szMessage > 128)
		throw InvalidProtocolException("too long message length");

	iStream.read(m_Message,szMessage);
		
	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void GCPartySay::write ( SocketOutputStream & oStream ) const
{
	__BEGIN_TRY

	BYTE szName = static_cast<BYTE>(m_Name.size());
	oStream.write(szName);
	oStream.write(m_Name);
	szName = static_cast<BYTE>(m_Message.size());
	oStream.write(m_Color);
	oStream.write(szName);
	oStream.write(m_Message);
		
	__END_CATCH
}


#ifdef __DEBUG_OUTPUT__
//////////////////////////////////////////////////////////////////////
//
// get packet's debug string
//
//////////////////////////////////////////////////////////////////////
string GCPartySay::toString () const
{
	__BEGIN_TRY

	StringStream msg;
	msg << "GCPartySay("
		<< "Name : " << m_Name
		<< ", Message : " << m_Message
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
#endif

