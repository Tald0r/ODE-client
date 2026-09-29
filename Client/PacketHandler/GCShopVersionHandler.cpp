//////////////////////////////////////////////////////////////////////
//
// Filename    : GCShopVersionHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCShopVersion.h"
#include "ClientDef.h"
#include "MNPC.h"
#include "MShopShelf.h"
#include "ServerInfo.h"
#include "UIFunction.h"
#include "MPriceManager.h"
void GCShopVersionHandler::execute ( GCShopVersion * pPacket , Player * pPlayer )

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
			// The NPC's shop
			//------------------------------------------------------
			MShop* pShop = pNPC->GetShop();

			if (pShop==NULL)
			{
				// It has none yet: create it.
				pShop = new MShop;
				pShop->Init( MShopShelf::MAX_SHELF );
				
				// and give it to the NPC
				pNPC->SetShop( pShop );				
			}

			//------------------------------------------------------
			// The normal rack is the one shown first.
			//------------------------------------------------------
			pShop->SetCurrent( 0 );

			//------------------------------------------------------
			// Build the normal rack (and the mysterious one) from the shop templates.
			//------------------------------------------------------
			pNPC->CreateFixedShelf();
			pNPC->CreateFixedShelf(true);	// mysterious -_-;


			//------------------------------------------------------
			// Compare each rack's version with the server's.
			//------------------------------------------------------
			BOOL bSameAll = TRUE;
			for (ShopRackType_t i=0; i<SHOP_RACK_TYPE_MAX; i++)
			{
				//------------------------------------------------------
				// Only the special rack is checked: the client builds
				// the others itself from its own data.
				//------------------------------------------------------
				if (i!=SHOP_RACK_SPECIAL)
				{
					continue;
				}

				MShopShelf* pShopShelf = pShop->GetShelf( i );

				//------------------------------------------------------
				// No rack yet: create it
				//------------------------------------------------------
				if (pShopShelf==NULL)
				{
					// i is SHOP_RACK_SPECIAL here, which is SHELF_SPECIAL
					// (both 1): inside the factory table, never NULL.
					pShopShelf = MShopShelf::NewShelf( i );

					pShop->SetShelf( (MShopShelf::SHELF_TYPE)i, pShopShelf );
				}

				unsigned int serverVersion = pPacket->getVersion( i );
				unsigned int clientVersion = pShopShelf->GetVersion();

				//------------------------------------------------------
				// A different version: ask for the rack's items.
				//------------------------------------------------------
				if (serverVersion!=clientVersion)
				{
					// Some rack is out of date
					bSameAll = FALSE;

						// The request for the rack's items
						CGShopRequestList	_CGShopRequestList;
						_CGShopRequestList.setObjectID( pNPC->GetID() );
						_CGShopRequestList.setRackType( i );

						g_pSocket->sendPacket( &_CGShopRequestList );						
				}
			}

			// The castle's tax ratio for this player, or the NPC's market
			// condition when that ratio is 100 (MPriceManager::SetShopTaxRatio).
			g_pPriceManager->SetShopTaxRatio( pPacket->getMarketCondSell() );
			
			//------------------------------------------------------
			// Every rack is up to date:
			// open the shop now.
			//------------------------------------------------------
			if (bSameAll)
			{
				//------------------------------------------------------
				// Everything is in place:
				// open the shop.
				//------------------------------------------------------
				UI_RunShop();
				UI_SetShop( pShop );		// the shop to show				
			}
			
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
