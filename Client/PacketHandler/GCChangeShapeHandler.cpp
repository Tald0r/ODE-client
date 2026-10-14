//////////////////////////////////////////////////////////////////////
//
// Filename    : GCChangeShapeHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCChangeShape.h"
#include "DebugLog.h"
#include "ZoneStateHost.h"
#include "MItem.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCChangeShapeHandler::execute ( GCChangeShape * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		


	//--------------------------------------------------------
	//
	// A temporary item for the shape.
	//
	//--------------------------------------------------------
	MItem* pItem = NULL;

	pItem = MItem::NewItem( (enum ITEM_CLASS)pPacket->getItemClass() );

	//--------------------------------------------------------
	// No such item.
	//--------------------------------------------------------
	if (pItem==NULL)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] Can't new Item ID=%d, Type=%d", pPacket->getItemClass(), pPacket->getItemType());
		
		return;
	}

	pItem->SetItemType( pPacket->getItemType() );
	pItem->AddItemOption( pPacket->getOptionType() );
	pItem->SetQuestFlag( (pPacket->getFlag() & SHAPE_FLAG_QUEST) ? true : false );


	//------------------------------------------------------
	//
	//  The creature puts the addon on.
	//
	//------------------------------------------------------
	//------------------------------------------------------
	// The zone is not made yet.
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
	}
	//------------------------------------------------------
	// The normal case. 
	//------------------------------------------------------
	else
	{
		MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );

		//--------------------------------------------------
		// No creature.
		//--------------------------------------------------
		if (pCreature==NULL)
		{
			// message
			DEBUG_ADD_FORMAT_ERR("[Error] Not Exist Creature. ID=%d", pPacket->getObjectID());
		}
		//--------------------------------------------------
		// The creature is here;
		// a wearing creature puts the addon on.
		//--------------------------------------------------
		else if (CreatureState::IsWear(pCreature))
		{
			// Stop the creature.
			CreatureState::SetStop(pCreature);
			
			#ifdef	OUTPUT_DEBUG				
				if (CreatureState::SetAddonItem( pCreature, pItem ))
				{
					DEBUG_ADD_FORMAT("[OK] Addon Item. ID=%d, Class=%d, Type=%d", pPacket->getObjectID(), pItem->GetItemClass(), pItem->GetItemType());
				}
				else
				{
					DEBUG_ADD_FORMAT_ERR("[Error] Can't Addon Item. ID=%d, Class=%d, Type=%d", pPacket->getObjectID(), pItem->GetItemClass(), pItem->GetItemType());
				}				
			#else
				CreatureState::SetAddonItem( pCreature, pItem );
			#endif

			CreatureState::SetWeaponSpeed( pCreature, pPacket->getAttackSpeed() );
		}
			
		//--------------------------------------------------
		// A creature that cannot wear clothes.
		//--------------------------------------------------
		else 
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Creature is Not CreatureWear. ID=%d", pPacket->getObjectID());
		}
	}	

	delete pItem;


	__END_CATCH
}
