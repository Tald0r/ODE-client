//////////////////////////////////////////////////////////////////////
// 
// Filename    : CRConnect.cpp 
// Written By  : elca@ewestsoft.com
// Description : 다른 client에 접속 요청을 한다.
// 
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// include files
//////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CRConnect.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
CRConnect::CRConnect ()
{
	__BEGIN_TRY
	__END_CATCH
}

	
//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
CRConnect::~CRConnect ()
{
}


//////////////////////////////////////////////////////////////////////
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void CRConnect::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY

	BYTE num;
	iStream.read( num );

	if (num == 0)
		throw InvalidProtocolException("szRequestServerName==0");
	
	iStream.read( m_RequestServerName, num );
	

	iStream.read( num );

	if (num == 0)
		throw InvalidProtocolException("szRequestClientName==0");
	
	iStream.read( m_RequestClientName, num );
	

	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void CRConnect::write ( SocketOutputStream & oStream ) 
     const
{
	__BEGIN_TRY
		
	// servername: 1..10 bytes, what getPacketMaxSize() budgets.
	// Each cap runs on the std::string's own size, before the narrowing
	// to the BYTE that goes on the wire: 257 bytes narrow to 1, and a
	// check on the BYTE would pass them all behind that length byte.
	if (m_RequestServerName.size() > 10)
		throw InvalidProtocolException("szRequestServerName>10");

	BYTE num = static_cast<BYTE>(m_RequestServerName.size());
	
	if (num == 0)
		throw InvalidProtocolException("szRequestServerName==0");

	oStream.write( num );
	oStream.write( std::span<const char>( m_RequestServerName.data(), num ) );


	// clientname: 1..10 bytes, likewise.
	if (m_RequestClientName.size() > 10)
		throw InvalidProtocolException("szRequestClientName>10");

	num = static_cast<BYTE>(m_RequestClientName.size());
	
	if (num == 0)
		throw InvalidProtocolException("szRequestClientName==0");

	oStream.write( num );
	oStream.write( std::span<const char>( m_RequestClientName.data(), num ) );

	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// get packet's debug std::string
//
//////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
	std::string CRConnect::toString () 
		const
	{
		__BEGIN_TRY

		StringStream msg;

		msg << "CRConnect( "
			<< "RequestServerName: " << m_RequestServerName 
			<< ",RequestClientName: " << m_RequestClientName
			<< ")";

		return msg.toString();

		__END_CATCH
	}

#endif