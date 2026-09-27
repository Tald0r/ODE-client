//////////////////////////////////////////////////////////////////////////////
// Filename    : CGRequestPowerPoint.cpp 
// Written By  :
// Description :
// 서버에 원하는 사람의 IP 요청
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGRequestPowerPoint.h"

//////////////////////////////////////////////////////////////////////////////
// class CGRequestPowerPoint member methods
//////////////////////////////////////////////////////////////////////////////

CGRequestPowerPoint::CGRequestPowerPoint ()
{
	__BEGIN_TRY
	__END_CATCH
}
	
CGRequestPowerPoint::~CGRequestPowerPoint ()
{
}

void CGRequestPowerPoint::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY

	BYTE szCellNum;
	iStream.read( szCellNum );

	if ( szCellNum == 0 )
		throw InvalidProtocolException( "szCellNum == 0" );

	if ( szCellNum > 12 )
		throw InvalidProtocolException( "szCellNum > 12" );

	iStream.read( m_CellNum, szCellNum );

	__END_CATCH
}
		    
void CGRequestPowerPoint::write ( SocketOutputStream & oStream ) 
     const
{
	__BEGIN_TRY
		
	// Cap the std::string's own size, before narrowing to the length byte.
	if ( m_CellNum.size() > 12 )
		throw InvalidProtocolException( "szCellNum > 12" );

	const BYTE szCellNum = (BYTE)m_CellNum.size();

	if ( szCellNum == 0 )
		throw InvalidProtocolException( "szCellNum == 0" );

	oStream.write( szCellNum );
	oStream.write( std::span<const char>(m_CellNum.data(), szCellNum) );

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
string CGRequestPowerPoint::toString () 
	const
{
	__BEGIN_TRY

	StringStream msg;
	msg << "CGRequestPowerPoint( "
		<< ",CellNum: " << m_CellNum
		<< ")";
	return msg.toString();

	__END_CATCH
}
#endif