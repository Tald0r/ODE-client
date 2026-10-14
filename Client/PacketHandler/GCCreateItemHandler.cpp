//--------------------------------------------------------------------------------
//
// Filename    : GCCreateItemHandler.cpp
// Written By  : elca
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCCreateItem.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "MInventory.h"
#include "MItem.h"
#include "PlayerStateHost.h"
#include "VsUiPresentationHost.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCCreateItemHandler::execute ( GCCreateItem * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	if (PlayerState::Creature() == NULL)
	{
		return;
	}
		

	MItem* pItem;

	
	//---------------------------------------------------------------------
	// picking up some of an inventory item..
	//---------------------------------------------------------------------
	if (PlayerState::GetItemCheckBufferStatus() == PlayerState::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY)
	{
		MItem* pItem = PlayerState::GetItemCheckBuffer();
		PlayerState::ClearItemCheckBuffer();		

		if (pItem!=NULL)
		{
			// the number to pick up is
			// ( the number there was - the number left behind ).
			int numPickup = pItem->GetNumber() - pPacket->getItemNum();

			// Take it out of the inventory, if it is where the inventory has it
			// (another item may be in its cell: that one stays).
			if (g_pInventory->GetItem( pItem->GetGridX(), pItem->GetGridY() )==pItem
				&& g_pInventory->RemoveItem( pItem->GetGridX(), pItem->GetGridY() )==pItem)
			{
				// Set its number again.
				pItem->SetNumber( numPickup );

				// Onto the mouse.
				DialogPresentation::PickUpItem( pItem );
			}
			else
			{
				DEBUG_ADD_ERR("[Error] PickupPart item Wrong");
			}
		}
		else
		{
			DEBUG_ADD_ERR("[Error] PickupPart item NULL");
		}
	}

	//---------------------------------------------------------------------
	// create the item.
	//---------------------------------------------------------------------
	pItem = MItem::NewItem( (enum ITEM_CLASS)pPacket->getItemClass() );

	if (pItem == NULL)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] GCCreateItem: invalid item class %d", pPacket->getItemClass());
		return;
	}

	pItem->SetItemType( pPacket->getItemType() );
	pItem->SetItemOptionList( pPacket->getOptionType() );

	pItem->SetID( pPacket->getObjectID() );		

	pItem->SetCurrentDurability( pPacket->getDurability() );

	//pItem->SetNumber( pPacket->getItemNum() );

	pItem->SetGrade( pPacket->getGrade() );
	pItem->SetEnchantLevel( pPacket->getEnchantLevel() );

	//------------------------------------------
	// the number
	//------------------------------------------
	// a gun
	//------------------------------------------
	if (pItem->IsGunItem())
	{
		// An empty magazine of the type the gun takes, holding the item's
		// number of rounds: the loop upstream ran here, which the
		// executable's LoadGun runs.
		MItem::LoadGun( pItem, pPacket->getItemNum() );
	}
	//------------------------------------------
	// not a gun
	//------------------------------------------
	else
	{
		pItem->SetNumber( pPacket->getItemNum() );
		//pItem->SetSilver( item.silver );
	}

	// the Christmas tree put together
	if(pItem->GetItemClass() == ITEM_CLASS_EVENT_TREE && (pItem->GetItemType() == 12
		|| pItem->GetItemType() == 25 || pItem->GetItemType() == 41))
	{
		if(PlayerState::GetItemCheckBufferStatus() == PlayerState::ITEM_CHECK_BUFFER_TREE_MERGE)
		{
			for(int y = 0; y < 4; y++)
			{
				for(int x = 0; x < 3; x++)
				{
					MItem *pPartOfTree = g_pInventory->GetItem(x+pPacket->getInvenX(), y+pPacket->getInvenY());
					if(pPartOfTree != NULL && 
						pPartOfTree->GetItemClass() == ITEM_CLASS_EVENT_TREE &&
						( pPartOfTree->GetItemType() == y*3+x ||
						pPartOfTree->GetItemType() == y*3+x + 13 ||
						pPartOfTree->GetItemType() == y*3+x + 29))
					{
						g_pInventory->RemoveItem(x+pPacket->getInvenX(), y+pPacket->getInvenY());
						delete pPartOfTree;
					}
				}
			}
			PlayerState::ClearItemCheckBuffer();
		}
		else
		{
			DEBUG_ADD("[GCCreateItemHandler] ItemCheckBuffer != ITEM_CHECK_BUFFER_TREE_MERGE");
		}
	}

	//---------------------------------------------------------------------
	// Add it to the inventory.
	//---------------------------------------------------------------------
	if (g_pInventory->AddItem( pItem, pPacket->getInvenX(), pPacket->getInvenY() ))
	{
		if( VsUiPresentation::IsCreatingToGear() )
		{
			VsUiPresentation::MoveCreatedToGear( pPacket->getInvenX(), pPacket->getInvenY() );
		}
		// Added properly..
	}
	else
	{
		DEBUG_ADD_FORMAT_ERR("[Error] Cannot Create Item to inventory: id=%d, tp=%d, xy=(%d,%d)", (int)pItem->GetID(), (int)pItem->GetItemType(), (int)pPacket->getInvenX(), (int)pPacket->getInvenY());
		
		delete pItem;
	}



	__END_CATCH
}
