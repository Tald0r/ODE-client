//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSkillToTileOK1Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSkillToTileOK1.h"
#include "AffectModifyInfo.h"
#include "ConvertDuration.h"
#include "DebugLog.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "Packet/WireHost.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"
//#include "mintr.h"
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSkillToTileOK1Handler::execute ( GCSkillToTileOK1 * pPacket, Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	// The player; without one there is nothing to do.
	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		return;
	}

	//------------------------------------------------------------------
	// The skill the player waited on was confirmed.
	//------------------------------------------------------------------
	if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_SKILL_SUCCESS)
	{		
		PlayerState::SetWaitVerifyNULL();
	}
	else
	{
		DEBUG_ADD_ERR("[Error] Player is not WaitVerifySkillSuccess");
	}


	//------------------------------------------------------------------
	// The player's skill succeeded, so
	// its result is applied.
	//------------------------------------------------------------------
	// Apply the status change.
	//------------------------------------------------------------------
	if (MStatus* pPlayerStatus = PlayerState::Status())
		AffectModifyInfo(pPlayerStatus, pPacket);
	
	//------------------------------------------------------------------
	// Update what the UI shows.
	// (Setting it beats comparing first.)
	//------------------------------------------------------------------
	//UI_SetHP( g_pPlayer->GetHP(), g_pPlayer->GetMAX_HP() );
	//UI_SetMP( g_pPlayer->GetMP(), g_pPlayer->GetMAX_MP() );

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
			SendBugReportAt(DiagnosticSite(), "%d", skillID);
			return;
		}
//		// 2004, 9, 9, sobeit add start
//		if(skillID == SKILL_BURNING_SOUL_LAUNCH)
//		{
//			skillID = RESULT_CLIENT_BURNING_SOL_1+pPacket->getGrade();
//			
//			ExecuteActionInfoFromMainNode(skillID,pPacket->getX(), pPacket->getY(), 0,0,0	,	
//				pPacket->getX(), pPacket->getY(), 0, ConvertDurationToFrame( pPacket->getDuration() ), NULL, false);
//			return;
//		}
		if(skillID == SKILL_BURNING_SOUL_CHARGING)
		{
			PlayerState::SetTempSelectPos(pPacket->getX(), pPacket->getY());
		}
	
		else if(skillID == SKILL_ROTTEN_APPLE)
		{
			WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_ROTTEN_APPLE,pPacket->getX(), pPacket->getY(), 0,0,0	,	
				pPacket->getX(), pPacket->getY(), 0, ConvertDurationToFrame( pPacket->getDuration() ), NULL, false);
		}
//		// 2004, 9, 9, sobeit add end
		int useSkillID = skillID;
		
		if ((*g_pActionInfoTable)[skillID].IsAffectCurrentWeaponAction() && skillID != SKILL_PIERCING )
		{
			useSkillID = CreatureState::GetBasicActionInfo(pPlayerCreature);

			if (static_cast<DWORD>(useSkillID) >= g_pActionInfoTable->GetMinResultActionInfo())
			{
				SendBugReportAt(DiagnosticSite(), "%d", skillID);
				return;
			}
		}

		if( (*g_pActionInfoTable)[skillID].IsUseActionStep() && pPacket->getGrade() > 0 )
			useSkillID = skillID = (*g_pActionInfoTable)[skillID].GetActionStep( pPacket->getGrade() -1 );
				
		int delayFrame = ConvertDurationToFrame( pPacket->getDuration() );

		int size = pPacket->getCListNum();

		MActionResult* pResult = new MActionResult;

		//------------------------------------------------------
		// The player's skill succeeded.
		//------------------------------------------------------
		//int resultActionInfo = skillID + (*g_pActionInfoTable).GetMinResultActionInfo();
		//g_pPlayer->PacketSpecialActionResult( resultActionInfo );

		int targetID = PlayerState::GetID();
		if ((*g_pActionInfoTable)[skillID].GetActionResultID()==ACTIONRESULTNODE_ACTIONINFO)
		{
			targetID = OBJECTID_NULL;
		}
		
		if((*g_pActionInfoTable)[skillID].IsUseActionGrade() )
		{
			
			pResult->Add( new MActionResultNodeActionInfo(
				skillID,
				PlayerState::GetID(), 
				targetID, 
				pPacket->getX(),
				pPacket->getY(),
				delayFrame, pPacket->getRange() ) 
				);	
		} else
		{
			pResult->Add( new MActionResultNodeActionInfo( 
				skillID,
				PlayerState::GetID(), 
				targetID, 
				pPacket->getX(),
				pPacket->getY(),
				delayFrame ) 
				);	
		}
		
//		_MinTrace("GCSkilltoTileOK1Handler. PlayerID : %d(%d,%d) %d\n", g_pPlayer->GetID(), pPacket->getX(), pPacket->getY(), skillID );
		if (size!=0)
		{
			// The result on each creature it lands on.
			for (int i=0; i<size; i++)
			{
				int targetID = pPacket->popCListElement();
				MCreature* pCreature = ZoneState::GetCreature( targetID );

				// The creature is hit.
				if (pCreature != NULL)
				{
					//------------------------------------------------------
					// Add the effect status, if any.
					//------------------------------------------------------
				
					EFFECTSTATUS es = (*g_pActionInfoTable)[skillID].GetEffectStatus();
					
					if (es!=EFFECTSTATUS_NULL)
					{
						pResult->Add( new MActionResultNodeAddEffectStatus( targetID, es, delayFrame ) );
					}

					// Show the result at once.
					//pCreature->PacketSpecialActionResult( pPacket->getSkillType() + (*g_pActionInfoTable).GetMinResultActionInfo());
			
					// The node that shows the skill's result.
					//pResult->Add( new MActionResultNodeActionInfo( 
					//							g_pPlayer->GetID(), 
					//							pCreature->GetID(), 
					//							pPacket->getSkillType(),
					//							delayFrame ) 
					//			);			
					
					//------------------------------------------------------
					//
					// Apply the skill's own result, if it has one.
					//
					//------------------------------------------------------
					MActionResultNode* pActionResultNode = NULL;

					switch ((*g_pActionInfoTable)[skillID].GetActionResultID())
					{
						//------------------------------------------------------
						// Run another action.
						//------------------------------------------------------
						case ACTIONRESULTNODE_ACTIONINFO :
							pActionResultNode =  new MActionResultNodeActionInfo( 
																useSkillID,//(*g_pActionInfoTable)[skillID].GetActionResultValue(),
																PlayerState::GetID(), 
																CreatureState::GetID(pCreature), 
																CreatureState::GetX(pCreature), 
																CreatureState::GetY(pCreature), 
																delayFrame);
						break;

						//------------------------------------------------------
						// Burrow and the like.
						//------------------------------------------------------
						default :
							pActionResultNode = CreateActionResultNode(pCreature, useSkillID);
						
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
		}			
		
		//------------------------------------------------------
		// The range is used as the direction.
		//------------------------------------------------------
		if ((*g_pActionInfoTable)[skillID].IsOptionRangeToDirection())
		{
			CreatureState::SetDirection(pPlayerCreature, pPacket->getRange() );
		}
	
		// Add it to the player's current skill as its result.		
		PlayerState::PacketAddActionResult( pPacket->getCEffectID(), pResult );

	}


	__END_CATCH
}
