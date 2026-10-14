//////////////////////////////////////////////////////////////////////
//
// Filename    : GCShopMarketConditionHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCShopMarketCondition.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "ZoneStateHost.h"
#include "MShopShelf.h"
#include "MShop.h"
#include "MPriceManager.h"

void GCShopMarketConditionHandler::execute ( GCShopMarketCondition * pPacket , Player * pPlayer )
	 

{
	__BEGIN_TRY
	(void)pPlayer;
	

	//------------------------------------------------------
	// The zone is not created yet
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
	}
	//------------------------------------------------------
	// Normal..
	//------------------------------------------------------
	else
	{
		MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );

		//------------------------------------------------------
		// No such creature
		//------------------------------------------------------
		if (pCreature==NULL)
		{
			DEBUG_ADD_FORMAT_ERR("[Error] There is no such Creature id=%d", pPacket->getObjectID());
		}
		//------------------------------------------------------
		// An NPC
		//------------------------------------------------------
		else if (CreatureState::IsNPC(pCreature))
		{

			//------------------------------------------------------
			// Read its shop.
			//------------------------------------------------------
			MShop* pShop = CreatureState::GetShop(pCreature);

			if (pShop==NULL)
			{
				// Without one, create it.
				pShop = new MShop;
				pShop->Init( MShopShelf::MAX_SHELF );

				// Give it to the NPC
				if (!CreatureState::SetShop( pCreature, pShop ))
				{
					delete pShop;
				}

				// Create the normal item shelf.
				//pNPC->CreateFixedShelf();
			}

			//------------------------------------------------------
			// The shelves' price rates
			//------------------------------------------------------
			g_pPriceManager->SetMarketCondBuy( pPacket->getMarketCondBuy() );
			g_pPriceManager->SetMarketCondSell( pPacket->getMarketCondSell() );
			
			//------------------------------------------------------
			// Open the inventory to sell.
			//------------------------------------------------------
			DialogPresentation::OpenInventoryToSell();

		}
		//------------------------------------------------------
		// Not an NPC
		//------------------------------------------------------
		else
		{
			DEBUG_ADD_FORMAT_ERR("[Error] The Creature is Not NPC. id=%d", pPacket->getObjectID());
		}
	}

	__END_CATCH
}
