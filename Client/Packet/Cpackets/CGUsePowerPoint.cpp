//////////////////////////////////////////////////////////////////////////////
// Filename    : CGUsePowerPoint.cpp 
// Written By  :
// Description :
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGUsePowerPoint.h"

//////////////////////////////////////////////////////////////////////////////
// class CGUsePowerPoint member methods
//////////////////////////////////////////////////////////////////////////////

CGUsePowerPoint::CGUsePowerPoint ()
{
	__BEGIN_TRY
	__END_CATCH
}
	
CGUsePowerPoint::~CGUsePowerPoint ()
{
}

void CGUsePowerPoint::read ( SocketInputStream & iStream )
{
	__BEGIN_TRY
	(void)iStream;
	__END_CATCH
}
		    
void CGUsePowerPoint::write ( SocketOutputStream & oStream ) 
     const
{
	__BEGIN_TRY
	(void)oStream;
	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
string CGUsePowerPoint::toString () 
	const
{
	__BEGIN_TRY

	StringStream msg;
	msg << "CGUsePowerPoint()";
	return msg.toString();

	__END_CATCH
}
#endif