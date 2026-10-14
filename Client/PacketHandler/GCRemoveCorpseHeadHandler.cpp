//--------------------------------------------------------------------------------
//
// Filename    : GCRemoveCorpseHeadHandler.cpp
// Written By  : Reiot
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCRemoveCorpseHead.h"
#include "DebugLog.h"
#include "ZoneStateHost.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCRemoveCorpseHeadHandler::execute ( GCRemoveCorpseHead * pPacket , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;


	int objectID = pPacket->getObjectID();

	//------------------------------------------------------
	// The zone is not made yet.
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
	}
	//------------------------------------------------------
	// The normal case.
	//------------------------------------------------------
	else
	{
		//----------------------------------------
		// A creature still standing loses its head; otherwise the
		// creature lying in the corpse with this ID does.
		//----------------------------------------
		MCreature* pCreature = ZoneState::GetCreature( objectID );

		if (pCreature==NULL)
		{
			pCreature = ZoneState::GetCorpseCreature( objectID );
		}

		if (pCreature==NULL)
		{
			DEBUG_ADD_FORMAT_ERR("[Error] No creature or corpse. id=%d", objectID);
		}
		else
		{
			CreatureState::RemoveHead(pCreature);
		}
	}

	__END_CATCH
}
