//////////////////////////////////////////////////////////////////////////////
// Filename    : CGTakeOutGood.cpp 
// Written By  : elca@ewestsoft.com
// Description : 
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "CGTakeOutGood.h"
#include "PacketAssert.h"


CGTakeOutGood::CGTakeOutGood ()
{
	__BEGIN_TRY
	__END_CATCH
}

CGTakeOutGood::~CGTakeOutGood ()
{
}

void CGTakeOutGood::read (SocketInputStream & iStream)
{
	__BEGIN_TRY
		
	iStream.read(m_ObjectID);

	__END_CATCH
}

void CGTakeOutGood::write (SocketOutputStream & oStream) const
{
	__BEGIN_TRY

	oStream.write(m_ObjectID);

	__END_CATCH
}

#ifdef __DEBUG_OUTPUT__
std::string CGTakeOutGood::toString () 
	const
{
	__BEGIN_TRY
		
	StringStream msg;
	msg << "CGTakeOutGood(ObjectID : " << (int)m_ObjectID << ")";
	return msg.toString();

	__END_CATCH
}
#endif