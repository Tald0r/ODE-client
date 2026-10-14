//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSkillToTileOK4Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSkillToTileOK4.h"
#include "ConvertDuration.h"
#include "DebugLog.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "Packet/WireHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSkillToTileOK4Handler::execute ( GCSkillToTileOK4 * pPacket , Player * pPlayer )

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
		int skillID = pPacket->getSkillType();
		if( g_pActionInfoTable->GetSize() <= skillID )
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Exceed SkillType %d",skillID);
			SendBugReport("[Error:GCSTTOK4H] Exceed SkillType %d", skillID);
			return;
		}
//		// 2004, 9, 9, sobeit add start
//		if(skillID == SKILL_BURNING_SOUL_LAUNCH)
//		{
//			skillID = CLIENT_BURNING_SOL_1+pPacket->getGrade();
//			ExecuteActionInfoFromMainNode(skillID,pPacket->getX(), pPacket->getY(), 0,0,0	,	
//				pPacket->getX(), pPacket->getY(), 0, ConvertDurationToFrame( pPacket->getDuration() ), NULL, false);
//			return;
//		}
//		// 2004, 9, 9, sobeit add end
		if( (*g_pActionInfoTable)[skillID].IsUseActionStep() && pPacket->getGrade() > 0)
			skillID = (*g_pActionInfoTable)[skillID].GetActionStep( pPacket->getGrade() - 1);

			//------------------------------------------------------
		// Make the result.
		//------------------------------------------------------
		MActionResult* pResult = new MActionResult;
	
		DWORD delayFrame = ConvertDurationToFrame( pPacket->getDuration() );

		int size = pPacket->getCListNum();

		for (int i=0; i<size; i++)
		{
			//pResult->Add( new MActionResultNodeActionInfo( 
			//					OBJECTID_NULL,		// the user; may it be missing?
			//					pPacket->popCListElement(), 
			//					skillType,
			//					delayFrame ) );
			//------------------------------------------------------
			//
			// Apply the skill's own result, if it has one.
			//
			//------------------------------------------------------
			int targetID = pPacket->popCListElement();

			MCreature* pTargetCreature = ZoneState::GetCreature( targetID );

			if (pTargetCreature!=NULL)
			{
				//------------------------------------------------------
				// Add the effect status, if any.
				//------------------------------------------------------
				EFFECTSTATUS es = (*g_pActionInfoTable)[skillID].GetEffectStatus();
							
				if (es!=EFFECTSTATUS_NULL)
				{
					pResult->Add( new MActionResultNodeAddEffectStatus( targetID, es, delayFrame ) );
				}

				MActionResultNode* pActionResultNode = NULL;

				switch ((*g_pActionInfoTable)[skillID].GetActionResultID())
				{
					//------------------------------------------------------
					// Run another action.
					//------------------------------------------------------
					case ACTIONRESULTNODE_ACTIONINFO :
						if( (*g_pActionInfoTable)[skillID].IsUseActionGrade() )
						{
							pActionResultNode =  new MActionResultNodeActionInfo( 
								(*g_pActionInfoTable)[skillID].GetActionResultValue(),
								OBJECTID_NULL, // no user
								targetID, 
								CreatureState::GetX(pTargetCreature),
								CreatureState::GetY(pTargetCreature),
								delayFrame, pPacket->getRange());
						} else
						{
							pActionResultNode =  new MActionResultNodeActionInfo( 
								(*g_pActionInfoTable)[skillID].GetActionResultValue(),
								OBJECTID_NULL, // no user
								targetID, 
								CreatureState::GetX(pTargetCreature),
								CreatureState::GetY(pTargetCreature),
								delayFrame);
						}
					break;

					//------------------------------------------------------
					// Burrow and the like.
					//------------------------------------------------------
					default :
						pActionResultNode = CreateActionResultNode(pTargetCreature, skillID);					
				}

				//------------------------------------------------------
				// Apply it too, if there is one.
				//------------------------------------------------------
				if (pActionResultNode!=NULL)
				{
					pResult->Add( pActionResultNode );
				}
			}
		}

		//------------------------------------------------------
		// Apply it straight to the sector.
		//------------------------------------------------------
		int resultActionInfo = skillID + (*g_pActionInfoTable).GetMinResultActionInfo();


		int direction;

		//------------------------------------------------------
		// The range is used as the direction.
		//------------------------------------------------------
		if ((*g_pActionInfoTable)[skillID].IsOptionRangeToDirection())
		{
			direction = pPacket->getRange();
		}
		else
		{
			direction = DIRECTION_DOWN;
		}

		WorldEvents::SkillToSector(resultActionInfo, 
						pPacket->getX(),
						pPacket->getY(),
						direction, 
						delayFrame,
						pResult);
	}


	__END_CATCH
}
