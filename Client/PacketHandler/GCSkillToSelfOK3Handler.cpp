//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSkillToSelfOK3Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSkillToSelfOK3.h"
#include "ConvertDuration.h"
#include "DebugLog.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "Packet/WireHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSkillToSelfOK3Handler::execute ( GCSkillToSelfOK3 * pPacket , Player * pPlayer )

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
		//MCreature* pCreature = g_pZone->GetCreature( pPacket->getObjectID() );


		//------------------------------------------------------
		//
		// The skill is used at a cell.
		//
		//------------------------------------------------------			

		int skillID = pPacket->getSkillType();
		
		if( g_pActionInfoTable->GetSize() <= skillID )
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Exceed SkillType %d",skillID);
			SendBugReport("[Error:GCSTSOK3H] Exceed SkillType %d", skillID );
			return;
		}


		if( (*g_pActionInfoTable)[skillID].IsUseActionStep() && pPacket->getGrade() > 0)
			skillID = (*g_pActionInfoTable)[skillID].GetActionStep( pPacket->getGrade() - 1);
		
		DWORD delayFrame = ConvertDurationToFrame( pPacket->getDuration() );

		/*
		
		MActionResult* pResult = NULL;

		//------------------------------------------------------
		// Add the effect status, if any.
		//------------------------------------------------------
		EFFECTSTATUS es = (*g_pActionInfoTable)[skillID].GetEffectStatus();
		
		if (es!=EFFECTSTATUS_NULL)
		{
			pResult = new MActionResult;
			pResult->Add( new MActionResultNodeAddEffectStatus( g_Player->GetID(), 
																	es, 
																	delayFrame ) );
		}
	
		
		pResult->Add( new MActionResultNodeActionInfo( 
								skillID, 
								OBJECTID_NULL,//pCreature->GetID(), 
								OBJECTID_NULL,//pCreature->GetID(), 
								pPacket->getX(),//pCreature->GetX(),
								pPacket->getY(),//pCreature->GetY(),
								delayFrame ) );
		*/

		//------------------------------------------------------
		//
		// Apply the skill's own result too, if it has one.
		//
		//------------------------------------------------------
		/*
		MActionResultNode* pActionResultNode = NULL;
		switch ((*g_pActionInfoTable)[skillID].GetActionResultID())
		{
			//------------------------------------------------------
			// Burrow
			//------------------------------------------------------
			case ACTIONRESULTNODE_CREATURE_BURROW :
				pActionResultNode = new MActionResultNodeCreatureBurrow(pCreature->GetID());
			break;
		}
		*/

		//------------------------------------------------------
		// Apply it too, if there is one.
		//------------------------------------------------------
		/*
		if (pActionResultNode!=NULL)
		{
			pResult->Add( pActionResultNode );
		}
		*/

		//Duration_t	m_Duration;
		//pCreature->PacketSpecialActionToSelf( 
		//						pPacket->getSkillType(),
		//						pResult	// result
		//	);

		//------------------------------------------------------
		// Only the cell is known.
		//------------------------------------------------------
		WorldEvents::ExecuteActionInfoFromMainNode(
			static_cast<TYPE_ACTIONINFO>(skillID + (*g_pActionInfoTable).GetMinResultActionInfo()),
		
			pPacket->getX(), pPacket->getY(), 0, // the user (?) 
			0,														// direction
			
			OBJECTID_NULL,												// the target
			pPacket->getX(), pPacket->getY(), 0, 
			
			delayFrame,													// the skill's (remaining) duration		
			
			NULL,

			false);			// start the skill from its beginning
	}


	__END_CATCH
}
