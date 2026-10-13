//--------------------------------------------------------------------------------
//
// Filename    : GCMoveHandler.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCMove.h"
#include "Player.h"
#include "DebugLog.h"
#include "ZoneStateHost.h"

	#include "Cpackets/CGMove.h"
	#include "ClientPlayer.h"


//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCMoveHandler::execute ( GCMove * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

		// Move the creature whose ID is getObjectID(). One that is not in the
		// zone is only reported.

		//------------------------------------------------------
		// The zone is not created yet.
		//------------------------------------------------------

		if (!ZoneState::Exists())
		{
			// message
			DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
		}
		//------------------------------------------------------
		// Normal case.
		//------------------------------------------------------
		else
		{
			MCreature* pCreature = ZoneState::GetCreature(pPacket->getObjectID());

			//--------------------------------------------------
			// Not in the zone.
			//--------------------------------------------------
			if (pCreature==NULL)
			{
				// message
				DEBUG_ADD_ERR("[Error] Not Exist");
				
//				_MinTrace("afsaf");
//				_MinTrace("[GCMove] Not Exist Creature. (MyX:%d,MyY:%d,CreatureID:%d,MoveX:%d,MoveY:%d,Direction:%d)\n",
//					g_pPlayer->GetX(), g_pPlayer->GetY(), pPacket->getObjectID(), pPacket->getX(), pPacket->getY(),
//					pPacket->getDir() );
			}
			//--------------------------------------------------
			// In the zone: move it.
			//--------------------------------------------------
			else
			{
				CreatureState::PacketMove(pCreature, pPacket->getX(), pPacket->getY(), pPacket->getDir());		
			}
		}
	

//	#endif


	__END_CATCH
}
