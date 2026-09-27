//////////////////////////////////////////////////////////////////////////////
// Filename    : CGRequestUnionInfo.cpp 
// Written By  : 
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGRequestUnionInfo.h"


void CGRequestUnionInfo::read (SocketInputStream & iStream)
{
	__BEGIN_TRY
	(void)iStream;
		
	__END_CATCH
}

void CGRequestUnionInfo::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY
	(void)oStream;
		
	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
string CGRequestUnionInfo::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
    msg << "CGRequestUnionInfo("
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
#endif