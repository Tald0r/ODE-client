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
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "MInventory.h"
#include "PlayerStateHost.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCDeleteInventoryItemHandler::execute ( GCDeleteInventoryItem * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;


	MItem* pItem = DialogPresentation::GetMouseItem();

	// The item on the mouse is checked too. by sigi. 2002.7.8
	if (pItem!=NULL
		&& pItem->GetID()==pPacket->getObjectID())
	{
		DialogPresentation::DropItem();
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
		const PlayerState::ITEM_CHECK_BUFFER status = PlayerState::GetItemCheckBufferStatus();

		if (status==PlayerState::ITEM_CHECK_BUFFER_DROP_TO_RELICTABLE || status==PlayerState::ITEM_CHECK_BUFFER_DROP_TO_CREATURE)
		{
			//---------------------------------------------
			// the item being dropped
			//---------------------------------------------
			MItem* pCheckItem = PlayerState::GetItemCheckBuffer();

			if(pCheckItem->GetID() == pItem->GetID())
				DialogPresentation::DropItem();
			PlayerState::ClearItemCheckBuffer();
		}

		// The item's destroy hook clears the tooltip descriptor that may
		// still hold it.
		delete pItem;
	}


	__END_CATCH
}
