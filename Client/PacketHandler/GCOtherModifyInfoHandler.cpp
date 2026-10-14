//////////////////////////////////////////////////////////////////////////////
// Filename    : GCOtherModifyInfoHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Client_PCH.h"
#include "Gpackets/GCOtherModifyInfo.h"
#include "AffectModifyInfo.h"
#include "DebugLog.h"
#include "GameInfoPresentationHost.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include <cstring>

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCOtherModifyInfoHandler::execute ( GCOtherModifyInfo * pPacket , Player * pPlayer )

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
		MCreature* pCreature = ZoneState::GetCreature(pPacket->getObjectID());

		if (pCreature==NULL)
		{
			DEBUG_ADD_FORMAT_ERR("[Error] the Creature is Not Exist. id=%d", pPacket->getObjectID());
		}
		else
		{
			MStatus* pStatus = CreatureState::Status( pCreature );
			if (pStatus != NULL)
			{
				AffectModifyInfo( pStatus, pPacket );
			}

			const char* pName = CreatureState::GetName( pCreature );
			const char* pOtherInfoName = GameInfoPresentation::GetOtherInfoName();
			if(pName != NULL && pOtherInfoName != NULL && strcmp(pName, pOtherInfoName) == 0)
			{
				// The other-info window shows only the player's own race.
				if( PlayerState::Creature() != NULL
					&& CreatureState::GetRace(pCreature) == PlayerState::GetRace())
					GameInfoPresentation::RunOtherInfo(pCreature);
			}
		}
	}


	__END_CATCH
}
