//////////////////////////////////////////////////////////////////////
//
// Filename    : GCGetOffMotorCycleHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCGetOffMotorCycle.h"
#include "DebugLog.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "SoundDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCGetOffMotorCycleHandler::execute ( GCGetOffMotorCycle * pPacket , Player * pPlayer )

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
	// The creature gets off its motorcycle.
	//------------------------------------------------------
	else
	{
		MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );

		if (pCreature!=NULL)
		{
			// A creature that can wear gear.
			if (CreatureState::IsWear(pCreature))
			{
				//------------------------------------------
				// The motorcycle is no longer drawn.
				//------------------------------------------
				CreatureState::RemoveAddon( pCreature, ADDON_MOTOR );

				//------------------------------------------
				// It walks now.
				//------------------------------------------
				CreatureState::SetMoveDevice( pCreature, CreatureState::MOVE_DEVICE_WALK );

				//------------------------------------------
				// The sound of getting off, heard where the
				// player stands.
				//------------------------------------------
				if (MCreature* const pPlayerCreature = PlayerState::Creature())
				{
					WorldEvents::PlaySoundAt( SOUND_WORLD_BIKE_STOP,
								false,
								CreatureState::GetX(pPlayerCreature),
								CreatureState::GetY(pPlayerCreature));
				}
			}
			else
			{
				DEBUG_ADD_FORMAT_ERR("[Error] Creature is not WearCreature id=%d", pPacket->getObjectID());
			}
		}
		else
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Not Exist Creature id=%d", pPacket->getObjectID());
		}
	}


	__END_CATCH
}
