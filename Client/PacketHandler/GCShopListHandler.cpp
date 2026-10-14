//////////////////////////////////////////////////////////////////////
//
// Filename    : GCShopListHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCShopList.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "GameInfoPresentationHost.h"
#include "MHelpDef.h"
#include "MShop.h"
#include "ZoneStateHost.h"
#include "MShopShelf.h"
#include "MPriceManager.h"

void GCShopListHandler::execute ( GCShopList * pPacket , Player * pPlayer )
	 

{
	__BEGIN_TRY
	(void)pPlayer;

	DEBUG_ADD("[GCShopListHandler::execute] run in execute function OK [0].");	
		

	//------------------------------------------------------
	// The zone is not created yet
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
	}
	//------------------------------------------------------
	// The zone exists
	//------------------------------------------------------
	else
	{
		MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );

		//------------------------------------------------------
		// No creature has that id
		//------------------------------------------------------
		if (pCreature==NULL)
		{
			DEBUG_ADD("[Error] OK 111");
		}
		//------------------------------------------------------
		// The creature is an NPC
		//------------------------------------------------------
		else if (CreatureState::IsNPC(pCreature))
		{

			DEBUG_ADD("[GCShopListHandler::execute] OK [1]\n");



			//------------------------------------------------------
			// A new rack for the items
			//------------------------------------------------------

			ShopRackType_t shopType = pPacket->getShopType();
			DEBUG_ADD_FORMAT("[GCShopListHandler::execute] OK [1.0]  %d\n", shopType);
			if (shopType >= MShopShelf::MAX_SHELF) {
				DEBUG_ADD_WAR("[GCShopListHandler::execute] SHELF_TYPE Wrong!");
				return;
			}

			MShopShelf* pShelf = MShopShelf::NewShelf( shopType );
			if (pShelf == NULL)
			{
				return;
			}


			DEBUG_ADD("[GCShopListHandler::execute] OK [1.1]\n");

			pShelf->SetVersion( pPacket->getShopVersion() );
			pShelf->SetEnable();

			DEBUG_ADD("[GCShopListHandler::execute] OK [2]\n");
			//------------------------------------------------------
			// Add the items
			//------------------------------------------------------
			for (int i=0; i<SHOP_RACK_INDEX_MAX; i++)
			{

				DEBUG_ADD_FORMAT("[GCShopListHandler::execute] shop item %d befre", i);

				const SHOPLISTITEM& item = pPacket->getShopItem( i );

				DEBUG_ADD_FORMAT("[GCShopListHandler::execute] shop item %d after", i);

				if (item.bExist)
				{
					// create the item
					MItem* pItem = MItem::NewItem( (ITEM_CLASS)item.itemClass );

					if (pItem == NULL)
					{
						DEBUG_ADD_FORMAT_ERR("[Error] GCShopList: invalid item class %d", (int)item.itemClass);
						continue;
					}

					pItem->SetID( item.objectID );
					pItem->SetItemType( item.itemType );
					pItem->SetItemOptionList( item.optionType );
					pItem->SetCurrentDurability( item.durability );
					pItem->SetSilver( item.silver );
					pItem->SetGrade( item.grade );
					pItem->SetEnchantLevel( item.enchantLevel );

					// Put the item on the rack
					pShelf->SetItem( i, pItem );
				}
			}

			DEBUG_ADD("[GCShopListHandler::execute] OK [3]");

			//------------------------------------------------------
			//
			// Put the rack in the NPC's shop
			//
			//------------------------------------------------------
			MShop* pShop = CreatureState::GetShop(pCreature);

			if (pShop==NULL)
			{
				// The NPC has no shop yet: create it
				pShop = new MShop;
				pShop->Init( MShopShelf::MAX_SHELF );

				// and give it to the NPC
				if (!CreatureState::SetShop( pCreature, pShop ))
				{
					// Only without the entry: nothing takes the shop or the rack.
					delete pShop;
					delete pShelf;
					return;
				}

				// Build the normal rack (and the mysterious one) from the shop templates.
				CreatureState::CreateFixedShelf( pCreature, false );
				CreatureState::CreateFixedShelf( pCreature, true );	// mysterious -_-;
			}

			//------------------------------------------------------
			// The kind of shop
			//------------------------------------------------------
			pShop->SetShopType( (MShop::SHOP_TYPE)pPacket->getNPCShopType() );

			//------------------------------------------------------
			// The normal rack is the one shown first.
			//------------------------------------------------------
			if (pShop->GetShopType()==MShop::SHOP_EVENT_STAR)
			{
				pShop->SetCurrent( 1 );
			}
			else
			{			
				pShop->SetCurrent( 0 );
			}

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
			DialogPresentation::SetShop( pShop );		// the shop to show
			DialogPresentation::RunShop();
			DialogPresentation::SetShop( pShop );		// the shop to show
		}
		//------------------------------------------------------
		// The creature is not an NPC
		//------------------------------------------------------
		else
		{
			DEBUG_ADD_FORMAT_ERR("[Error] The Creature is Not NPC. id=%d", pPacket->getObjectID());
		}
	}
//	__BEGIN_HELP_EVENT
		GameInfoPresentation::ExecuteHelpEvent( HELP_EVENT_USE_SHOP );
//	__END_HELP_EVENT

	__END_CATCH
}
