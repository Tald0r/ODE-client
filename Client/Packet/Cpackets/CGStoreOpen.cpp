//////////////////////////////////////////////////////////////////////////////
// Filename    : CGStoreOpen.cpp 
// Written By  : 
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGStoreOpen.h"

void CGStoreOpen::read (SocketInputStream & iStream)
{
	__BEGIN_TRY
	(void)iStream;

	__END_CATCH
}

void CGStoreOpen::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY
	(void)oStream;

	__END_CATCH
}

string CGStoreOpen::toString () const
{
	__BEGIN_TRY
		
	StringStream msg;
    msg << "CGStoreOpen("
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
