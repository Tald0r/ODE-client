//////////////////////////////////////////////////////////////////////
// 
// Filename    : GCAddNewItemToZone.cc 
// Written By  : elca
// Description : 
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "GCAddNewItemToZone.h"
#include "PacketAssert.h"

//--------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------
GCAddNewItemToZone::GCAddNewItemToZone()
{
	__BEGIN_TRY
	__END_CATCH
}

	
//--------------------------------------------------------------------
// Destructor
//--------------------------------------------------------------------
GCAddNewItemToZone::~GCAddNewItemToZone()
{
}


#ifdef __DEBUG_OUTPUT__	
//////////////////////////////////////////////////////////////////////
//
// get packet's debug std::string
//
//////////////////////////////////////////////////////////////////////
std::string GCAddNewItemToZone::toString () const
{
	__BEGIN_TRY

	StringStream msg;

	msg << "GCAddNewItemToZone("
		<< "ObjectID:" << m_ObjectID
		<< ",X:" << (int)m_X 
		<< ",Y:" << (int)m_Y 
		<< ",ItemClass: " << (int)m_ItemClass
		<< ",ItemType: " << (int)m_ItemType
		<< ",OptionTypeSize: " << (int)m_OptionType.size()
		<< ",Durability: " << (int)m_Durability
		<< ",ItemNum: " << (int)m_ItemNum
		<< ")" ;

	return msg.toString();

	__END_CATCH
}
#endif