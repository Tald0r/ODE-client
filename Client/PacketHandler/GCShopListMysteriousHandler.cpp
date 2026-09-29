//////////////////////////////////////////////////////////////////////
//
// Filename    : GCShopListMysteriousHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCShopListMysterious.h"
#include "ClientDef.h"
#include "MNPC.h"
#include "MShopShelf.h"
#include "MPriceManager.h"
#include "UIFunction.h"

void GCShopListMysteriousHandler::execute ( GCShopListMysterious * pPacket , Player * pPlayer )
	 

{
	__BEGIN_TRY
	(void)pPlayer;
	

	//------------------------------------------------------
	// The zone is not created yet
	//------------------------------------------------------
	if (g_pZone==NULL)
	{
		// message
		DEBUG_ADD("[Error] Zone is Not Init.. yet.");			
	}
	//------------------------------------------------------
	// The zone exists
	//------------------------------------------------------
	else
	{
		MCreature* pCreature = g_pZone->GetCreature( pPacket->getObjectID() );

		//------------------------------------------------------
		// No creature has that id
		//------------------------------------------------------
		if (pCreature==NULL)
		{
			DEBUG_ADD_FORMAT("[Error] There is no such Creature id=%d", pPacket->getObjectID());
		}
		//------------------------------------------------------
		// The creature is an NPC
		//------------------------------------------------------
		else if (pCreature->GetClassType()==MCreature::CLASS_NPC)
		{
			MNPC* pNPC = (MNPC*)pCreature;

			//------------------------------------------------------
			// A new rack for the items
			//------------------------------------------------------
			MShopShelf* pShelf = MShopShelf::NewShelf( pPacket->getShopType() );
			if (pShelf == NULL)
			{
				DEBUG_ADD_FORMAT("[Error] GCShopListMysterious: invalid shelf type %d", (int)pPacket->getShopType());
				return;
			}
			
			pShelf->SetVersion( pPacket->getShopVersion() );

			//------------------------------------------------------
			// Add the items
			//------------------------------------------------------
			for (int i=0; i<SHOP_RACK_INDEX_MAX; i++)
			{
				const SHOPLISTITEM_MYSTERIOUS& item = pPacket->getShopItem( i );

				if (item.bExist)
				{
					// create the item
					MItem* pItem = MItem::NewItem( (ITEM_CLASS)item.itemClass );

					if (pItem == NULL)
					{
						DEBUG_ADD_FORMAT("[Error] GCShopListMysterious: invalid item class %d", (int)item.itemClass);
						continue;
					}

					//pItem->SetID( item.objectID );
					//pItem->SetItemType( item.itemType );
					//pItem->SetItemOption( item.optionType );
					//pItem->SetCurrentDurability( item.durability );

					pItem->UnSetIdentified();

					// Put the item on the rack
					pShelf->SetItem( i, pItem );
				}
			}

			//------------------------------------------------------
			//
			// Put the rack in the NPC's shop
			//
			//------------------------------------------------------
			MShop* pShop = pNPC->GetShop();

			if (pShop==NULL)
			{
				// The NPC has no shop yet: create it
				pShop = new MShop;
				pShop->Init( MShopShelf::MAX_SHELF );

				// and give it to the NPC
				pNPC->SetShop( pShop );

				// Build the normal rack (and the mysterious one) from the shop templates.
				pNPC->CreateFixedShelf();
				pNPC->CreateFixedShelf(true);	// mystrious
			}

			//------------------------------------------------------
			// The normal rack is the one shown first.
			//------------------------------------------------------
			pShop->SetCurrent( 0 );

			//------------------------------------------------------
			// The NPC's buying rate, and the castle's tax ratio for this
			// player (MPriceManager::SetShopTaxRatio)
			//------------------------------------------------------
			g_pPriceManager->SetMarketCondBuy( pPacket->getMarketCondBuy() );
			g_pPriceManager->SetShopTaxRatio( pPacket->getMarketCondSell() );
			
			//------------------------------------------------------
			// Put the rack in the shop
			//------------------------------------------------------
			pShop->SetShelf( pShelf->GetShelfType(), pShelf );

			//------------------------------------------------------
			// Everything is in place:
			// open the shop.
			//------------------------------------------------------
			UI_SetShop( pShop );		// the shop to show
			UI_RunShop();
			UI_SetShop( pShop );		// the shop to show
		}
		//------------------------------------------------------
		// The creature is not an NPC
		//------------------------------------------------------
		else
		{
			DEBUG_ADD_FORMAT("[Error] The Creature is Not NPC. id=%d", pPacket->getObjectID());
		}
	}



	__END_CATCH
}
