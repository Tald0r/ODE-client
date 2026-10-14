//////////////////////////////////////////////////////////////////////
//
// Filename    : GCCrossCounterOK2Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCCrossCounterOK2.h"
#include "AffectModifyInfo.h"
#include "DebugLog.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCCrossCounterOK2Handler::execute ( GCCrossCounterOK2 * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	// The player; without one there is nothing to do.
	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		return;
	}
		


	// The player is hit by objectID's creature.
	//------------------------------------------------------------------
	// Apply the status change.
	//------------------------------------------------------------------
	if (MStatus* pPlayerStatus = PlayerState::Status())
		AffectModifyInfo(pPlayerStatus, pPacket);


	//int delayFrame = ConvertDurationToFrame( pPacket->getDuration() );

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
		MCreature* pUserCreature = ZoneState::GetCreature( pPacket->getObjectID() );

		WorldEvents::SkillCrossCounter( pUserCreature, pPlayerCreature, pPacket->getSkillType() );
		
	}	


	__END_CATCH
}
