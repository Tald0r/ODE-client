//////////////////////////////////////////////////////////////////////
//
// Filename    : GCReloadOKHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCReloadOK.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "MInventory.h"
#include "MItem.h"
#include "MQuickSlot.h"
#include "MSlayerGear.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCReloadOKHandler::execute ( GCReloadOK * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		
	// message

	//------------------------------------------------------------------
	// A packet for a slayer only.
	//------------------------------------------------------------------
	if (PlayerState::GetRace()==RACE_VAMPIRE)
	{
		DEBUG_ADD_ERR("[Error] Player is Vampire. No Effect!");

		return;
	}

	// hmm.. a cut & paste masterpiece.. - -;;

	//------------------------------------------------------------------
	//
	//				Check the item check buffer
	//
	//------------------------------------------------------------------	
	MItem* pItem = PlayerState::GetItemCheckBuffer();

	//----------------------------------------------------
	// An item in the check buffer
	//----------------------------------------------------
	if (pItem!=NULL)
	{
		WorldEvents::PlaySound( pItem->GetUseSoundID() );

		PlayerState::ITEM_CHECK_BUFFER status =	PlayerState::GetItemCheckBufferStatus();

		//----------------------------------------------------
		// used from the inventory
		//----------------------------------------------------
		if (status==PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY)
		{
			// Clear the item check buffer.
			PlayerState::ClearItemCheckBuffer();

			//----------------------------------------------------
			// MAGAZINE
			//----------------------------------------------------
			if (pItem->GetItemClass()==ITEM_CLASS_MAGAZINE)
			{
				MItem* pMagazineItem;

				//-------------------------------------------------
				// the number must be checked.
				//-------------------------------------------------
				if (pItem->GetNumber()>1)
				{
					// more of them left,
					// one fewer.
					pItem->SetNumber( pItem->GetNumber() - 1 );

					//-------------------------------------------------
					// make a new magazine for the gun.
					//-------------------------------------------------
					pMagazineItem = MItem::NewItem( ITEM_CLASS_MAGAZINE );

					pMagazineItem->SetID( 0 );	// any ID.. probably unused..
					pMagazineItem->SetItemType( pItem->GetItemType() );
					pMagazineItem->SetItemOptionList( pItem->GetItemOptionList() );
				}
				//-------------------------------------------------
				// all used --> remove it.
				//-------------------------------------------------
				else
				{
					// remove it from the inventory
					g_pInventory->RemoveItem( pItem->GetID() );

					// not freed --> it goes in the gun..
					//delete pItem;	
					
					pMagazineItem = pItem;
				}

				
				//----------------------------------------------------
				// set the number of bullets again.
				//----------------------------------------------------
				// Magazine.GetNumber()
				//
				// as a magazine, the number of magazines piled
				// in a gun, Number is the number of bullets.
				//----------------------------------------------------
				pMagazineItem->SetNumber( pPacket->getBulletNum() );

				// a slayer only.
				const MItem* pRightHandItem = g_pSlayerGear->GetItem( (MSlayerGear::GEAR_SLAYER)MSlayerGear::GEAR_SLAYER_RIGHTHAND );

				//----------------------------------------------------
				// a gun in the right hand
				//----------------------------------------------------
				if (pRightHandItem!=NULL && pRightHandItem->IsGunItem())
				{
					// load the magazine as the current one, freeing the one it held
					MItem::ReloadGun( const_cast<MItem*>(pRightHandItem), pMagazineItem );
				}
				//----------------------------------------------------
				// no gun in the right hand --> an error.. the magazine
				// is spent all the same: the last one has left its
				// container, and of a stack only the new one goes.
				//----------------------------------------------------
				else
				{
					DEBUG_ADD_ERR("[Error] Player not Use GUN");

					if (pMagazineItem == pItem)
					{
						// forget the item's description - any use? - -;
						DialogPresentation::RemoveDescriptor( (void*)pItem );
					}

					delete pMagazineItem;
				}
			}
			//----------------------------------------------------
			// not a magazine is an error.
			//----------------------------------------------------
			else
			{
				DEBUG_ADD_FORMAT_ERR("[Error] Item is Not Magazine: the ItemClass=%d", (int)pItem->GetItemClass());
			}
		}
		//----------------------------------------------------
		// used from the quick slot
		//----------------------------------------------------
		else if (status==PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT)
		{
			// Clear the item check buffer.
			PlayerState::ClearItemCheckBuffer();

			//----------------------------------------------------
			// MAGAZINE
			//----------------------------------------------------
			if (pItem->GetItemClass()==ITEM_CLASS_MAGAZINE)
			{
				MItem* pMagazineItem;

				//-------------------------------------------------
				// the number must be checked.
				//-------------------------------------------------
				if (pItem->GetNumber()>1)
				{
					// more of them left,
					// one fewer.
					pItem->SetNumber( pItem->GetNumber() - 1 );

					//-------------------------------------------------
					// make a new magazine for the gun.
					//-------------------------------------------------
					pMagazineItem = MItem::NewItem( ITEM_CLASS_MAGAZINE );

					pMagazineItem->SetID( 0 );	// any ID.. probably unused..
					pMagazineItem->SetItemType( pItem->GetItemType() );
					pMagazineItem->SetItemOptionList( pItem->GetItemOptionList() );					
				}
				//-------------------------------------------------
				// all used --> remove it.
				//-------------------------------------------------
				else
				{
					// remove it from the quick slot
					g_pQuickSlot->RemoveItem( pItem->GetID() );

					// not freed --> it goes in the gun..
					//delete pItem;	
					
					pMagazineItem = pItem;
				}

				//----------------------------------------------------
				// set the number of bullets again.
				//----------------------------------------------------
				pMagazineItem->SetNumber( pPacket->getBulletNum() );

				// a slayer only.
				const MItem* pRightHandItem = g_pSlayerGear->GetItem( (MSlayerGear::GEAR_SLAYER)MSlayerGear::GEAR_SLAYER_RIGHTHAND );

				//----------------------------------------------------
				// a gun in the right hand
				//----------------------------------------------------
				if (pRightHandItem!=NULL && pRightHandItem->IsGunItem())
				{
					// load the magazine as the current one, freeing the one it held
					MItem::ReloadGun( const_cast<MItem*>(pRightHandItem), pMagazineItem );
				}
				//----------------------------------------------------
				// no gun in the right hand --> an error.. the magazine
				// is spent all the same: the last one has left its
				// container, and of a stack only the new one goes.
				//----------------------------------------------------
				else
				{
					DEBUG_ADD_ERR("[Error] Player not Use GUN");

					if (pMagazineItem == pItem)
					{
						// forget the item's description - any use? - -;
						DialogPresentation::RemoveDescriptor( (void*)pItem );
					}

					delete pMagazineItem;
				}
			}
			//----------------------------------------------------
			// not a magazine is an error.
			//----------------------------------------------------
			else
			{
				DEBUG_ADD_FORMAT_ERR("[Error] Item is Not Magazine: the ItemClass=%d", (int)pItem->GetItemClass());
			}
		}
		//----------------------------------------------------
		// another status??
		//----------------------------------------------------
		else
		{
			DEBUG_ADD_FORMAT_ERR("[Error] ItemCheck Buffer is not Use Status: status=%d", (int)status);
			
			return;
		}

		// for silver bullets... damage and so on.. calculate again..
		PlayerState::CalculateStatus();

	}
	//----------------------------------------------------
	// no item.. - -;;
	//----------------------------------------------------
	else
	{
		DEBUG_ADD_ERR("[Error] No Item in CheckBuffer");
		
		return;
	}



	__END_CATCH
}
