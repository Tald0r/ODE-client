//////////////////////////////////////////////////////////////////////
// 
// Filename    : GCDropItemToZone.cc 
// Written By  : elca
// Description : 
// 
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "GCDropItemToZone.h"
#include "PacketAssert.h"

//--------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------
GCDropItemToZone::GCDropItemToZone()
{
	__BEGIN_TRY

	m_DropPetOID=0;

	__END_CATCH
}

	
//--------------------------------------------------------------------
// Destructor
//--------------------------------------------------------------------
GCDropItemToZone::~GCDropItemToZone()
{
}

void GCDropItemToZone::read ( SocketInputStream & iStream )
{
	GCAddItemToZone::read(iStream);
	iStream.read( m_DropPetOID );
}

void GCDropItemToZone::write ( SocketOutputStream & oStream ) const
{
	GCAddItemToZone::write(oStream);
	oStream.write( m_DropPetOID );
}

#ifdef __DEBUG_OUTPUT__
//////////////////////////////////////////////////////////////////////
// toString
//////////////////////////////////////////////////////////////////////
std::string GCDropItemToZone::toString () const
{
	__BEGIN_TRY

	StringStream msg;
	msg << "GCDropItemToZone("
		<< "ObjectID:"     << m_ObjectID
		<< ",X:"           << (int)m_X 
		<< ",Y:"           << (int)m_Y 
		<< ",ItemClass:"   << (int)m_ItemClass
		<< ",ItemType:"    << (int)m_ItemType
		<< ",OptionTypeSize:"  << (int)m_OptionType.size()
		<< ",Durability:"  << (int)m_Durability
		<< ",ItemNum:"     << (int)m_ItemNum
		<< ")" ;
	return msg.toString();

	__END_CATCH
}
#endif