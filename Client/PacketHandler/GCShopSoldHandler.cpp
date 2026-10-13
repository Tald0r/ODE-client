//////////////////////////////////////////////////////////////////////
//
// Filename    : GCShopSoldHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCShopSold.h"
#include "DebugLog.h"
#include "MItem.h"
#include "MShop.h"
#include "MShopShelf.h"
#include "ZoneStateHost.h"


void GCShopSoldHandler::execute ( GCShopSold * pPacket , Player * pPlayer )
	 


{
	__BEGIN_TRY
	(void)pPlayer;
	

	//------------------------------------------------------
	// The zone is not made yet
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
	}
	//------------------------------------------------------
	// Otherwise
	//------------------------------------------------------
	else
	{
		MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );

		if (pCreature==NULL)
		{
			DEBUG_ADD_FORMAT_ERR("[Error] There is no such Creature. id=%d", pPacket->getObjectID());
		}
		else if (CreatureState::IsNPC(pCreature))
		{

			MShop* pShop = CreatureState::GetShop(pCreature);

			if (pShop==NULL)
			{
				DEBUG_ADD_FORMAT_ERR("[Error] the Creature has NO Shop. creatureID=%d", pPacket->getObjectID());
			}
			else
			{
				MShopShelf* pShopShelf = pShop->GetShelf( (MShopShelf::SHELF_TYPE)pPacket->getShopType() );

				if (pShopShelf==NULL)
				{
					DEBUG_ADD_FORMAT_ERR("[Error] There is no such Shelf. type=%d", (int)pPacket->getShopType());
				}
				else
				{
					//------------------------------------------------------
					// Only the special shelf loses the sold item.
					//------------------------------------------------------
					if (pPacket->getShopType()==SHOP_RACK_SPECIAL)
					{
						MItem* pRemovedItem = pShopShelf->RemoveItem( pPacket->getShopIndex() );

						if (pRemovedItem!=NULL)
						{
							// Remove the sold item. Deleting it also drops its UI
							// descriptor, through MItemHost::ItemDestroyed.
							delete pRemovedItem;
						}
					}

					// the shelf's new version
					pShopShelf->SetVersion( pPacket->getShopVersion() );									
				}
			}
		}
		else
		{
			DEBUG_ADD_FORMAT_ERR("[Error] the Creature is not NPC. id=%d", pPacket->getObjectID());
		}
	}


	__END_CATCH
}
