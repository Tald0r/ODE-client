//////////////////////////////////////////////////////////////////////
//
// Filename    : GCTakeOffHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCTakeOff.h"
#include "DebugLog.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCTakeOffHandler::execute ( GCTakeOff * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

	//------------------------------------------------------
	//
	//  The creature takes the addon off.
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
			DEBUG_ADD_FORMAT_ERR("[Error] Not Exist Creature. ID=%d", pPacket->getObjectID());
		}
		//--------------------------------------------------
		// The creature is here;
		// a wearing creature takes the addon off.
		//--------------------------------------------------
		else if (CreatureState::IsWear(pCreature))
		{
			// Stop the creature.
			CreatureState::SetStop(pCreature);

			#ifdef	OUTPUT_DEBUG				
				if (CreatureState::RemoveAddon( pCreature, pPacket->getSlotID() ))
				{
					DEBUG_ADD_FORMAT("[OK] RemoveAddon. ID=%d, Slot=%d", pPacket->getObjectID(), (int)pPacket->getSlotID());
				}
				else
				{
					DEBUG_ADD_FORMAT_ERR("[Error] RemoveAddon. ID=%d, Slot=%d", pPacket->getObjectID(), (int)pPacket->getSlotID());
				}				
			#else
				CreatureState::RemoveAddon( pCreature, pPacket->getSlotID() );
			#endif
		}
			
		//--------------------------------------------------
		// A creature that cannot wear clothes.
		//--------------------------------------------------
		else 
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Creature is Not CreatureWear. ID=%d", pPacket->getObjectID());
		}
	}
	


	__END_CATCH
}
