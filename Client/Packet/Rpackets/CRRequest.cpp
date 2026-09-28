//////////////////////////////////////////////////////////////////////
// 
// Filename    : CRRequest.cpp 
// Written By  : elca@ewestsoft.com
// Description : 다른 클라이언트에게 뭔가를 요청하는거다.
// 
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// include files
//////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CRRequest.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"

//////////////////////////////////////////////////////////////////////
// constructor
//////////////////////////////////////////////////////////////////////
CRRequest::CRRequest ()
{
	__BEGIN_TRY

	m_Code = CR_REQUEST_NULL;

	__END_CATCH
}

	
//////////////////////////////////////////////////////////////////////
// destructor
//////////////////////////////////////////////////////////////////////
CRRequest::~CRRequest ()
{
}


//////////////////////////////////////////////////////////////////////
// 입력스트림(버퍼)으로부터 데이타를 읽어서 패킷을 초기화한다.
//////////////////////////////////////////////////////////////////////
void CRRequest::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY

	BYTE code;
	iStream.read( code);
	m_Code = (CR_REQUEST_CODE)code;

	BYTE num;
	iStream.read( num );

	if (num > 20)
		throw InvalidProtocolException("szRequestName>20");
	
	if (num > 0)
	{
		iStream.read( m_RequestName, num );
	}
		
	__END_CATCH
}

		    
//////////////////////////////////////////////////////////////////////
// 출력스트림(버퍼)으로 패킷의 바이너리 이미지를 보낸다.
//////////////////////////////////////////////////////////////////////
void CRRequest::write ( SocketOutputStream & oStream ) 
     const
{
	__BEGIN_TRY
		
	BYTE code = (BYTE)m_Code;
	oStream.write( code);

	
	// Each cap runs on the std::string's own size, before the narrowing
	// to the BYTE that goes on the wire: 257 bytes narrow to 1, and a
	// check on the BYTE would pass them all behind that length byte.
	if (m_RequestName.size() > 20)
		throw InvalidProtocolException("szRequestName>20");

	const BYTE num = static_cast<BYTE>(m_RequestName.size());

	oStream.write( num );

	if (num > 0)
	{
		oStream.write( std::span<const char>( m_RequestName.data(), num ) );
	}

	
	__END_CATCH
}


//////////////////////////////////////////////////////////////////////
//
// get packet's debug std::string
//
//////////////////////////////////////////////////////////////////////
#ifdef __DEBUG_OUTPUT__
	std::string CRRequest::toString () 
		const
	{
		__BEGIN_TRY

		StringStream msg;

		msg << "CRRequest( "
			<< "code: " << (int)m_Code
			<< "RequestName: " << m_RequestName
			<< ")";

		return msg.toString();

		__END_CATCH
	}

#endif