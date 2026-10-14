//////////////////////////////////////////////////////////////////////
//
// Filename    : GCShopSellOKHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCShopSellOK.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "ItemSkillIcons.h"
#include "MInventory.h"
#include "TempInformation.h"
#include "MMoneyManager.h"
#include "MStorage.h"
// (formerly declared here to save compile time)

void GCShopSellOKHandler::execute ( GCShopSellOK * pPacket , Player * pPlayer )
	 

{
	__BEGIN_TRY
	(void)pPlayer;
	

	//--------------------------------------------------------------
	//
	// Is a sale what we were waiting for?
	//
	//--------------------------------------------------------------

	if(pPacket->getShopVersion() == -1 && g_pStorage2!=NULL)
	{
		int current_storage = g_pStorage2->GetCurrent();
		const int storage_size = g_pStorage2->GetSize();
		const int storage_max = STORAGE_SLOT;
		
		for(int j = 0; j < storage_size; j++)
		{
			g_pStorage2->SetCurrent(0);
			 
			for(int i = 0; i < storage_max; i++)
			{
				const MItem *p_slot_item = g_pStorage2->GetItem(i);
				
				// An empty slot just takes it.
				if(NULL != p_slot_item)
				{
					if(p_slot_item->GetID() == pPacket->getObjectID())
					{ 
						MItem* pRemovedItem = g_pStorage2->RemoveItem( i );
						delete pRemovedItem; pRemovedItem = NULL;
						break;
					}
				}
			}
			current_storage ++;
			if(current_storage >= storage_size)
				current_storage = 0;
		}

		// Remove it from the inventory.
		MItem *TempItem = g_pInventory->RemoveItem( (TYPE_OBJECTID)pPacket->getItemObjectID() );
		// Free it.
		delete TempItem; TempItem = NULL;

		g_pMoneyManager->AddMoney(pPacket->getPrice());
		g_pTempInformation->SetMode(TempInformation::MODE_NULL);
		DialogPresentation::UnlockItemTrade();
		return;
	
	}
	if(pPacket->getShopVersion() == -1 && g_pStorage2==NULL)
	{
		MItem *TempItem = g_pInventory->RemoveItem( (TYPE_OBJECTID)pPacket->getItemObjectID() );
		// Free it.
		delete TempItem; TempItem = NULL;

		g_pMoneyManager->AddMoney(pPacket->getPrice());
		g_pTempInformation->SetMode(TempInformation::MODE_NULL);
		DialogPresentation::UnlockItemTrade();
		return;

	}

	if (g_pTempInformation->GetMode() == TempInformation::MODE_SHOP_SELL)
	{
		MItem* pCheckItem = (MItem*)g_pTempInformation->pValue;		
		
		const MItem* pItem = g_pInventory->GetItem( pCheckItem->GetGridX(), pCheckItem->GetGridY() );

		// Check the ID.
		if (pCheckItem->GetID()==pItem->GetID())
		{
			// ShopVersion_t getShopVersion(); -_-;;

			MItem* pRemovedItem = g_pInventory->RemoveItem( (TYPE_OBJECTID)pItem->GetID() );

			if (pRemovedItem!=NULL)
			{
				WorldEvents::PlaySound( pRemovedItem->GetInventorySoundID() );

				CheckItemForSkillIcon( pRemovedItem );


				delete pRemovedItem;
			}
			else
			{
				DEBUG_ADD_ERR("[Error] Item is NULL");
			}

			//--------------------------------------------------------------
			// Change the money.
			//--------------------------------------------------------------
			if (!g_pMoneyManager->SetMoney( pPacket->getPrice() ))
			{
				DEBUG_ADD_FORMAT_ERR("[Error] Can't Set Money=%d, Price=%d", g_pMoneyManager->GetMoney(), pPacket->getPrice());
			}
		}
		else
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Different ID. Packet(%d)!=ClientTemp(%d)", pPacket->getItemObjectID(), pItem->GetID());
		}

		// Clear the mode.
		g_pTempInformation->SetMode(TempInformation::MODE_NULL);
		
		// Trading is allowed again.
		DialogPresentation::UnlockItemTrade();
	}
	//--------------------------------------------------------------
	//
	// Selling every skull.
	//
	//--------------------------------------------------------------
	else if (g_pTempInformation->GetMode() == TempInformation::MODE_SHOP_SELL_ALL_SKULL)
	{
		MItem* pSkull = g_pInventory->FindItem( ITEM_CLASS_SKULL );

		//--------------------------------------------------------------
		// Play the sound once.
		//--------------------------------------------------------------
		if (pSkull!=NULL)
		{
			WorldEvents::PlaySound( pSkull->GetInventorySoundID() );
		}				

		//--------------------------------------------------------------
		// Remove every skull.
		//--------------------------------------------------------------
		while (pSkull!=NULL)
		{
			MItem* pRemovedItem = g_pInventory->RemoveItem( (TYPE_OBJECTID)pSkull->GetID() );

			if (pRemovedItem!=NULL)
			{

				delete pRemovedItem;
			}

			//--------------------------------------------------------------
			// Is there another skull?
			//--------------------------------------------------------------
			pSkull = g_pInventory->FindItem( ITEM_CLASS_SKULL );
		}

		//--------------------------------------------------------------
		// Change the money.
		//--------------------------------------------------------------
		if (!g_pMoneyManager->SetMoney( pPacket->getPrice() ))
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Can't Set Money=%d, Price=%d", g_pMoneyManager->GetMoney(), pPacket->getPrice());
		}

		// Clear the mode.
		g_pTempInformation->SetMode(TempInformation::MODE_NULL);

		// Trading is allowed again.
		DialogPresentation::UnlockItemTrade();
	}	
	//--------------------------------------------------------------
	//
	// Something went wrong.
	//
	//--------------------------------------------------------------
	else
	{
		DEBUG_ADD_ERR("[Error] Sell? -_-; No Temp Information!");
	}

	__END_CATCH
}
