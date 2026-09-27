//////////////////////////////////////////////////////////////////////
// 
// Filename    : GCAddInstalledMineToZone.cc 
// Written By  : elca
// Description : 
// 
//////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
// include files
#include "GCAddInstalledMineToZone.h"
#include "PacketAssert.h"

//--------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------
GCAddInstalledMineToZone::GCAddInstalledMineToZone()
{
	__BEGIN_TRY
	__END_CATCH
}

	
//--------------------------------------------------------------------
// Destructor
//--------------------------------------------------------------------
GCAddInstalledMineToZone::~GCAddInstalledMineToZone()
{
}


#ifdef __DEBUG_OUTPUT__	
//////////////////////////////////////////////////////////////////////
//
// get packet's debug std::string
//
//////////////////////////////////////////////////////////////////////
std::string GCAddInstalledMineToZone::toString () const
{
	__BEGIN_TRY

	StringStream msg;
	msg << "GCAddInstalledMineToZone("
		<< "ObjectID:" << m_ObjectID
		<< ",X:" << (int)m_X 
		<< ",Y:" << (int)m_Y 
		<< ",ItemClass:" << (int)m_ItemClass
		<< ",ItemType:" << (int)m_ItemType
		<< ",OptionTypeSize:" << (int)m_OptionType.size()
		<< ",Durability:" << (int)m_Durability
		<< ",ItemNum:" << (int)m_ItemNum
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
#endif