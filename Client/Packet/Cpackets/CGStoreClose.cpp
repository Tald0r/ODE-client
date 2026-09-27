//////////////////////////////////////////////////////////////////////////////
// Filename    : CGStoreClose.cpp 
// Written By  : 
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGStoreClose.h"

void CGStoreClose::read (SocketInputStream & iStream)
{
	__BEGIN_TRY
	(void)iStream;

	__END_CATCH
}

void CGStoreClose::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY
	(void)oStream;

	__END_CATCH
}

string CGStoreClose::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
    msg << "CGStoreClose("
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
