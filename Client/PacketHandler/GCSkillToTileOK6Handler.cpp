//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSkillToTileOK6Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSkillToTileOK6.h"
#include "AffectModifyInfo.h"
#include "ConvertDuration.h"
#include "DebugLog.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "MStatus.h"
#include "Packet/WireHost.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSkillToTileOK6Handler::execute ( GCSkillToTileOK6 * pPacket , Player * pPlayer )

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
	// Apply the status change.
	//------------------------------------------------------------------
	if (MStatus* pPlayerStatus = PlayerState::Status())
		AffectModifyInfo(pPlayerStatus, pPacket);

	int delayFrame = ConvertDurationToFrame( pPacket->getDuration() );

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
		//------------------------------------------------------
		// A packet for the player's own skill
		// is an error.
		//------------------------------------------------------
		
		//------------------------------------------------------
		// A skill without its user.
		//------------------------------------------------------
		int startX = pPacket->getOrgX();
		int startY = pPacket->getOrgY();
	
		int skillID = pPacket->getSkillType();

		if( g_pActionInfoTable->GetSize() <= skillID )
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Exceed SkillType %d",skillID);
			SendBugReport("[Error:GCSTTOK6H] Exceed SkillType %d", skillID );
			return;
		}
		// 2004, 9, 9, sobeit add start
		if(skillID == SKILL_BURNING_SOUL_LAUNCH)
		{
			int TempskillID = RESULT_CLIENT_BURNING_SOL_1+pPacket->getGrade();
			WorldEvents::ExecuteActionInfoFromMainNode(TempskillID,pPacket->getX(), pPacket->getY(), 0,0,0	,	
				pPacket->getX(), pPacket->getY(), 0, ConvertDurationToFrame( pPacket->getDuration() ), NULL, false);
		//	return;
		}
		// 2004, 9, 9, sobeit add end
		
		if( (*g_pActionInfoTable)[skillID].IsUseActionStep() &&pPacket->getGrade() > 0 )
			skillID = (*g_pActionInfoTable)[skillID].GetActionStep( pPacket->getGrade() - 1);

		//------------------------------------------------------
		// The result of the TileOK2. 
		//------------------------------------------------------				
		int size = pPacket->getCListNum();

		MActionResult* pResult = new MActionResult;

		//------------------------------------------------------------------
		// Apply the effect status.
		//------------------------------------------------------------------
		MStatus* pPlayerStatus = PlayerState::Status();
		if (pPlayerStatus != NULL && pPlayerStatus->GetEFFECT_STAT()!=EFFECTSTATUS_NULL)
		{
			//int esDelayFrame = ConvertDurationToFrame( g_pPlayer->GetDURATION() );
			pResult->Add( new MActionResultNodeAddEffectStatus( 
										PlayerState::GetID(), 
										(EFFECTSTATUS)pPlayerStatus->GetEFFECT_STAT(), 
										delayFrame ));

			pPlayerStatus->SetStatus( MODIFY_EFFECT_STAT, EFFECTSTATUS_NULL );
		}

		/*
		
		pResult->Add( new MActionResultNodeActionInfo( 
											skillID,
											pCreature->GetID(), 
											pCreature->GetID(), 
											pPacket->getX(),
											pPacket->getY(),
											delayFrame ) 
							);	
		*/

		if (size!=0)
		{
			// The result on each creature it lands on.
			for (int i=0; i<size; i++)
			{
				//MCreature* pTargetCreature = g_pZone->GetCreature( pPacket->getCListElement() );
					
				// The creature is hit.
				//if (pTargetCreature != NULL)
				//{
					// The player is hit by someone's skill.
					// [ TEST CODE ]
					
				//	pResult->Add( new MActionResultNodeActionInfo( 
				//								pCreature->GetID(), 
				//								pPacket->popCListElement(), 
				//								pPacket->getSkillType(),
				//								delayFrame ) 
				//				);
				//}
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
							if( (*g_pActionInfoTable)[skillID].IsUseActionGrade() )
							{
								pActionResultNode =  new MActionResultNodeActionInfo( 
									(*g_pActionInfoTable)[skillID].GetActionResultValue(),
									OBJECTID_NULL,//pCreature->GetID(), 
									targetID, 
									CreatureState::GetX(pTargetCreature),
									CreatureState::GetY(pTargetCreature),
									delayFrame, pPacket->getRange() );
							} else
							{
								pActionResultNode =  new MActionResultNodeActionInfo( 
									(*g_pActionInfoTable)[skillID].GetActionResultValue(),
									OBJECTID_NULL,//pCreature->GetID(), 
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
		
		}

		//------------------------------------------------------
		// The range is used as the direction.
		//------------------------------------------------------
		int direction = pPacket->getRange();

		if (direction >= 8) direction = 0;

		//------------------------------------------------------
		// Add the result of the TileOK2.
		//------------------------------------------------------
		//Duration_t	m_Duration;
		//pCreature->PacketSpecialActionToSector(
		//					pPacket->getSkillType(), 
		//					pPacket->getX(), pPacket->getY(),
		//					pResult						// result
		//);			
	

		//------------------------------------------------------
		// Start at once from some point in it.
		//------------------------------------------------------
		WorldEvents::ExecuteActionInfoFromMainNode(
				static_cast<TYPE_ACTIONINFO>(skillID + (*g_pActionInfoTable).GetMinResultActionInfo()),

				startX, startY, 0,
		
				direction,														// direction
		
				OBJECTID_NULL,												// the target
		
				pPacket->getX(), pPacket->getY(), 0, 
		
				delayFrame,													// the skill's (remaining) duration		
		
				pResult,
				
				false);			// start the skill from its beginning
		
	}


	//------------------------------------------------------------
	// The delay frame
	//------------------------------------------------------------
	//g_pPlayer->SetEffectDelayFrame( resultActionInfo, delayFrame );




	//------------------------------------------------------------------
	// Update what the UI shows.
	// (Setting it beats comparing first.)
	//------------------------------------------------------------------
	//UI_SetHP( g_pPlayer->GetHP(), g_pPlayer->GetMAX_HP() );
	//UI_SetMP( g_pPlayer->GetMP(), g_pPlayer->GetMAX_MP() );


	__END_CATCH
}
