//--------------------------------------------------------------------------------
//
// Filename    : GCDeleteInventoryItemHandler.cpp
// Written By  : Reiot
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCDeleteInventoryItem.h"
#include "ClientDef.h"
#include "MInventory.h"
#include "UIFunction.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCDeleteInventoryItemHandler::execute ( GCDeleteInventoryItem * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;


	MItem* pItem = UI_GetMouseItem();

	// mouse에 있는 것도 함 체크해준다. by sigi. 2002.7.8
	if (pItem!=NULL
		&& pItem->GetID()==pPacket->getObjectID())
	{
		UI_DropItem();		
	}
	else
	{
		pItem = g_pInventory->RemoveItem( pPacket->getObjectID() );

		if (pItem==NULL)
		{
			DEBUG_ADD_FORMAT_ERR( "[Error] Can't remove Item. id=%d", pPacket->getObjectID());
		}		
	}

	if (pItem!=NULL)
	{

		if (g_pPlayer->IsItemCheckBufferDropToRelicTable() || g_pPlayer->IsItemCheckBufferDropToCreature())
		{
			//---------------------------------------------
			// the item being dropped
			//---------------------------------------------
			MItem* pCheckItem = g_pPlayer->GetItemCheckBuffer();

			if(pCheckItem->GetID() == pItem->GetID())
				UI_DropItem();
			g_pPlayer->ClearItemCheckBuffer();
		}

		// The tooltip descriptor may still hold this item's raw pointer and
		// dereferences it every frame, so it has to be cleared before the
		// delete — the same pattern the shop and gear handlers use.
		UI_RemoveDescriptor( (void*)pItem );

		delete pItem;
	}


	__END_CATCH
}
