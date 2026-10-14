//////////////////////////////////////////////////////////////////////
//
// Filename    : GCRideMotorCycleHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCRideMotorCycle.h"
#include "DebugLog.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "MItem.h"
#include "SoundDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCRideMotorCycleHandler::execute ( GCRideMotorCycle * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

	

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
	//
	// The creature puts pItem (the motorcycle) on.
	//------------------------------------------------------
	else
	{
		MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );

		//-------------------------------------------------------------
		// The character is here.
		//-------------------------------------------------------------
		if (pCreature!=NULL)
		{
			// A creature that can wear gear.
			if (CreatureState::IsWear(pCreature))
			{
				MItem* pItem = ZoneState::GetItem( pPacket->getTargetObjectID() );

				//------------------------------------------					
				// pItem must be a motorcycle.
				// The creature puts it on.
				//------------------------------------------					
				if (pItem != NULL)
				{
					if (pItem->GetItemClass()==ITEM_CLASS_MOTORCYCLE)
					{
						//------------------------------------------					
						// By sex, the motorcycle addon goes on.
						//------------------------------------------
						CreatureState::SetAddonItem( pCreature, pItem );						
						
						//------------------------------------------
						// It rides now.
						//------------------------------------------					
						//pCreatureWear->SetMoveDevice( MCreature::MOVE_DEVICE_RIDE );

						//------------------------------------------
						// When not connected to a server,
						// the item goes straight onto the mouse.
						//------------------------------------------
						//PlaySound( pItem->GetTileSoundID(),
						//			false,
						//			pCreatureWear->GetX(), pCreatureWear->GetY());

						//gC_vs_ui.PickUpItem(pItem);
						ZoneState::RemoveItem( pItem->GetID() );

						// The sound of riding off, heard where the player
						// stands.
						if (MCreature* const pPlayerCreature = PlayerState::Creature())
						{
							WorldEvents::PlaySoundAt( SOUND_WORLD_BIKE_GO,
										false,
										CreatureState::GetX(pPlayerCreature),
										CreatureState::GetY(pPlayerCreature));
						}
					}
					else
					{
						DEBUG_ADD_FORMAT_ERR("[Error] id=%d is Not Motorcycle", pPacket->getTargetObjectID());
					}
				}
			}
		}
		//-------------------------------------------------------------
		// No character.
		//-------------------------------------------------------------
		else
		{
			MItem* pItem = ZoneState::GetItem( pPacket->getTargetObjectID() );

			//------------------------------------------					
			// pItem must be a motorcycle.
			// Just remove it.
			//------------------------------------------					
			if (pItem != NULL)
			{
				if (pItem->GetItemClass()==ITEM_CLASS_MOTORCYCLE)
				{
					ZoneState::RemoveItem( pItem->GetID() );
				}
			}
		}
	}
	

	__END_CATCH
}
