//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSkillToTileOK3Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSkillToTileOK3.h"
#include "DebugLog.h"
#include "MActionInfoTable.h"
#include "Packet/WireHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSkillToTileOK3Handler::execute ( GCSkillToTileOK3 * pPacket , Player * pPlayer )

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
		MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );


		// The creature uses something on a tile.
		if (pCreature != NULL)
		{	
			int skillID = pPacket->getSkillType();
			
			if( g_pActionInfoTable->GetSize() <= skillID )
			{
				SendBugReport("[GCSTTOK3H] exceed skilltype %d",skillID);
				return;
			}
			// 2004, 9, 9, sobeit add start
//			if(skillID == SKILL_BURNING_SOUL_LAUNCH)
//			{
//				skillID = CLIENT_BURNING_SOL_1+pPacket->getGrade();
//				ExecuteActionInfoFromMainNode(skillID,pPacket->getX(), pPacket->getY(), 0,0,0	,	
//					pPacket->getX(), pPacket->getY(), 0, 0, NULL, false);
//				return;
//			}
			// 2004, 9, 9, sobeit add end

			if( (*g_pActionInfoTable)[skillID].IsUseActionStep() && pPacket->getGrade() > 0 )
				skillID = (*g_pActionInfoTable)[skillID].GetActionStep( pPacket->getGrade() - 1) == ACTIONINFO_NULL ? 
									skillID : (*g_pActionInfoTable)[skillID].GetActionStep( pPacket->getGrade() - 1);

			//------------------------------------------------------
			// The creature acts.
			//------------------------------------------------------
			//Duration_t	m_Duration;
			CreatureState::PacketSpecialActionToSector(pCreature, skillID, 
								pPacket->getX(), 
								pPacket->getY(),
								NULL		// result
			);		

			// Face the cell.
			CreatureState::SetDirectionToPosition(pCreature, pPacket->getX(), pPacket->getY());		
		}
		else
		{
			DEBUG_ADD_FORMAT_ERR("[Error] There is no such Creature id=%d", pPacket->getObjectID());
		}
	}


	__END_CATCH
}
