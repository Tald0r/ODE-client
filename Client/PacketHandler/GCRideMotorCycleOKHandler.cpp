//////////////////////////////////////////////////////////////////////
//
// Filename    : GCRideMotorCycleOKHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCRideMotorCycleOK.h"
#include "DebugLog.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "MItem.h"
#include "SoundDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCRideMotorCycleOKHandler::execute ( GCRideMotorCycleOK * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	// The player; without one there is nothing to do.
	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		return;
	}
		


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
		MItem* pItem = ZoneState::GetItem( pPacket->getObjectID() );

		//------------------------------------------					
		// pItem must be a motorcycle.
		// The player puts it on.
		//------------------------------------------					
		if (pItem != NULL)
		{
			if (pItem->GetItemClass()==ITEM_CLASS_MOTORCYCLE)
			{
				//------------------------------------------					
				// the motorcycle addon goes on.
				//------------------------------------------					
				CreatureState::SetAddonItem(pPlayerCreature, pItem );
				
				//------------------------------------------
				// It rides now.
				//------------------------------------------					
				//g_pPlayer->SetMoveDevice( MCreature::MOVE_DEVICE_RIDE );

				//------------------------------------------
				// When not connected to a server,
				// the item goes straight onto the mouse.
				//------------------------------------------
				//PlaySound( pItem->GetTileSoundID(),
				//			false,
				//			g_pPlayer->GetX(), g_pPlayer->GetY());

				//gC_vs_ui.PickUpItem(pItem);
				ZoneState::RemoveItem( pItem->GetID() );

				WorldEvents::PlaySound( SOUND_WORLD_BIKE_GO );
			}
			else
			{
				DEBUG_ADD_FORMAT_ERR("[Error] id=%d is Not Motorcycle", pPacket->getObjectID());
			}
		}
	}
	
	//------------------------------------------
	// Either way, take it as verified.
	//------------------------------------------
	PlayerState::SetWaitVerifyNULL();

	// [Help] on riding a motorcycle
//	__BEGIN_HELP_EVENT
////		ExecuteHelpEvent( HE_ITEM_RIDE_MOTORCYCLE );
//	__END_HELP_EVENT


	__END_CATCH
}
