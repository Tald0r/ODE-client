//////////////////////////////////////////////////////////////////////
//
// Filename    : GCStatusCurrentHPHandler.cpp
// Written By  : elca@ewestsoft.com
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCStatusCurrentHP.h"
#include "DebugLog.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCStatusCurrentHPHandler::execute ( GCStatusCurrentHP * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		


	//------------------------------------------------------
	// Zone이 아직 생성되지 않은 경우
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
	
	}	
	//------------------------------------------------------
	// 정상.. 
	//------------------------------------------------------
	else
	{
		MCreature* pCreature = ZoneState::GetCreature(pPacket->getObjectID());

		//--------------------------------------------------
		// 없으면.. - -;
		//--------------------------------------------------
		if (pCreature==NULL)
		{	
			DEBUG_ADD_FORMAT_ERR("[Error] SetHP:There is no such Creature. id=%d", pPacket->getObjectID());
		}
		else
		{		
			CreatureState::SetStatus(pCreature, MODIFY_CURRENT_HP, pPacket->getCurrentHP() );
		}
	}


	__END_CATCH
}
