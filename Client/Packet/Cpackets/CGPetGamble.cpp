//////////////////////////////////////////////////////////////////////////////
// Filename    : CGPetGamble.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGPetGamble.h"
#include "PacketAssert.h"


CGPetGamble::CGPetGamble ()
{
	__BEGIN_TRY
	__END_CATCH
}

CGPetGamble::~CGPetGamble ()
{
}

void CGPetGamble::read (SocketInputStream & iStream)
{
	__BEGIN_TRY
	(void)iStream;
		
	__END_CATCH
}

void CGPetGamble::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY
	(void)oStream;

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
std::string CGPetGamble::toString () 
	const
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "CGPetGamble()";
	return msg.toString();

	__END_CATCH
}
#endif