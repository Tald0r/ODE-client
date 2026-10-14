//////////////////////////////////////////////////////////////////////////////
// Filename    : GCEnterVampirePortalHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
#include "Gpackets/GCEnterVampirePortal.h"
#include "DebugLog.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void GCEnterVampirePortalHandler::execute ( GCEnterVampirePortal * pPacket , Player * pPlayer )

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
		int creatureID = pPacket->getObjectID();

		MCreature* pCreature = ZoneState::GetCreature( creatureID );

		//--------------------------------------------------
		// It vanishes into the portal
		//--------------------------------------------------
		if (pCreature!=NULL)
		{
			int x = pPacket->getX();
			int y = pPacket->getY();

			CreatureState::SetStop( pCreature );

			CreatureState::MovePosition( pCreature, x, y );
			CreatureState::SetServerPosition( pCreature, x, y );

			WorldEvents::MoveIntoPortal( pCreature );

			ZoneState::RemoveCreature( creatureID );
		}
		else
		{
			DEBUG_ADD_FORMAT_ERR("[Error] There is no such Creature. id=%d", creatureID);
		}
	}


	__END_CATCH
}
