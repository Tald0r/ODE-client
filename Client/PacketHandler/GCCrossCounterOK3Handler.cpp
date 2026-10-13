//////////////////////////////////////////////////////////////////////
//
// Filename    : GCCrossCounterOK3Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCCrossCounterOK3.h"
#include "DebugLog.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCCrossCounterOK3Handler::execute ( GCCrossCounterOK3 * pPacket , Player * pPlayer )

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
	//------------------------------------------------------
	else
	{
		MCreature* pUserCreature = ZoneState::GetCreature( pPacket->getObjectID() );
		MCreature* pTargetCreature = ZoneState::GetCreature( pPacket->getTargetObjectID() );

		WorldEvents::SkillCrossCounter( pUserCreature, pTargetCreature, pPacket->getSkillType() );
	}	


	__END_CATCH
}
