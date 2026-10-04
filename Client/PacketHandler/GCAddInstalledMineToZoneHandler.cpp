//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAddInstalledMineToZoneHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
// include files
#include "Gpackets/GCAddInstalledMineToZone.h"
#include "ClientDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAddInstalledMineToZoneHandler::execute ( GCAddInstalledMineToZone * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

	/*
		DEBUG_ADD_FORMAT("[Add NewItem] class=%d, type=%d, opt=%d, id=%d, xy=(%d,%d)",
										(int)pPacket->getItemClass(), 
										(int)pPacket->getItemType(), 
										(int)pPacket->getOptionType(), 
										(int)pPacket->getObjectID(), 
										(int)pPacket->getX(), 
										(int)pPacket->getY());
	*/
	
	// 
	MItem* pItem = AddItemToZone( (GCAddItemToZone*) pPacket, FALSE );	

	if (pItem==NULL)
	{
		// 머지
		DEBUG_ADD_ERR("[Error] Can't Installed Mine");
	}
	else if (pItem->GetItemClass()==ITEM_CLASS_MINE)
	{
		((MMine*)pItem)->SetInstalled();
	}
	else
	{
		DEBUG_ADD_ERR("[Error] This is not Mine(-_-;)");
	}


	__END_CATCH
}
