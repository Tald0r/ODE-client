//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAddNewItemToZoneHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddNewItemToZone.h"
#include "ClientDef.h"
#include "MItem.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAddNewItemToZoneHandler::execute ( GCAddNewItemToZone * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

	#ifdef __DEBUG_OUTPUT__
		DEBUG_ADD_FORMAT("[Add NewItem] class=%d, type=%d, opt=%d, id=%d, xy=(%d,%d)",
										(int)pPacket->getItemClass(), 
										(int)pPacket->getItemType(), 
										(int)pPacket->getOptionTypeSize(), 
										(int)pPacket->getObjectID(), 
										(int)pPacket->getX(), 
										(int)pPacket->getY());
	#endif
	
	// dropping = FALSE
	AddItemToZone( (GCAddItemToZone*) pPacket, FALSE );	
	

	__END_CATCH
}
