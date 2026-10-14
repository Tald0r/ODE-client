//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSkillToTileOK5Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSkillToTileOK5.h"
#include "ConvertDuration.h"
#include "DebugLog.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "Packet/WireHost.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSkillToTileOK5Handler::execute ( GCSkillToTileOK5 * pPacket , Player * pPlayer )

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
				DEBUG_ADD_FORMAT_ERR("[Error] Exceed SkillType %d",skillID);
				SendBugReport("[Error:GCSTTOK5H] Exceed SkillType %d", skillID );
				return;
			}
			// 2004, 9, 9, sobeit add start
			if(skillID == SKILL_BURNING_SOUL_LAUNCH)
			{
				int TempskillID = SKILL_CLIENT_BURNING_SOL_1+pPacket->getGrade();
				WorldEvents::ExecuteActionInfoFromMainNode(TempskillID,CreatureState::GetX(pCreature), CreatureState::GetY(pCreature), 0,CreatureState::GetDirection(pCreature),CreatureState::GetID(pCreature)	,	
					pPacket->getX(), pPacket->getY(), 0, ConvertDurationToFrame( pPacket->getDuration() ), NULL, false);
			}
			else if(skillID == SKILL_ROTTEN_APPLE)
			{
				WorldEvents::ExecuteActionInfoFromMainNode(SKILL_CLIENT_ROTTEN_APPLE,pPacket->getX(), pPacket->getY(), 0,0,0	,	
					pPacket->getX(), pPacket->getY(), 0, ConvertDurationToFrame( pPacket->getDuration() ), NULL, false);
			}
			// 2004, 9, 9, sobeit add end

			int useSkillID = skillID;

			if ((*g_pActionInfoTable)[skillID].IsAffectCurrentWeaponAction()&& skillID != SKILL_PIERCING )
			{
				useSkillID = CreatureState::GetBasicActionInfo(pCreature);

				if (static_cast<DWORD>(useSkillID) >= g_pActionInfoTable->GetMinResultActionInfo())
				{
					DEBUG_ADD_FORMAT_ERR("[Error] SkillType Error = %d", useSkillID);
					return;
				}
			}
			
			DEBUG_ADD_FORMAT("GRADE : %d %d",skillID, pPacket->getGrade());
			if( (*g_pActionInfoTable)[skillID].IsUseActionStep() && pPacket->getGrade() > 0)
				useSkillID = skillID = (*g_pActionInfoTable)[skillID].GetActionStep( pPacket->getGrade() - 1);

			
			// Make the result.
			MActionResult* pResult = new MActionResult;
		
			DWORD delayFrame = ConvertDurationToFrame( pPacket->getDuration() );

			int targetID = CreatureState::GetID(pCreature);
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
					CreatureState::GetID(pCreature), 
					targetID,
					pPacket->getX(),
					pPacket->getY(),
					delayFrame ) 
					);
			}
//
//			pResult->Add( new MActionResultNodeActionInfo( 
//											skillID,
//											pCreature->GetID(), 
//											targetID,	//pCreature->GetID(), 
//											pPacket->getX(),
//											pPacket->getY(),
//											delayFrame ) 
//							);


			int size = pPacket->getCListNum();
	
			for (int i=0; i<size; i++)
			{
				//pResult->Add( new MActionResultNodeActionInfo( 
				//					pCreature->GetID(), 
				//					pPacket->popCListElement(), 
				//					pPacket->getSkillType(),
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
							pActionResultNode =  new MActionResultNodeActionInfo( 
																useSkillID,	//(*g_pActionInfoTable)[skillID].GetActionResultValue(),
																CreatureState::GetID(pCreature), 
																targetID, 
																CreatureState::GetX(pTargetCreature),
																CreatureState::GetY(pTargetCreature),
																delayFrame);
						break;

						//------------------------------------------------------
						// Burrow
						//------------------------------------------------------
						default :
							pActionResultNode = CreateActionResultNode(pTargetCreature, useSkillID);
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
			// Face the cell.
			//------------------------------------------------------
			CreatureState::SetDirectionToPosition(pCreature, pPacket->getX(), pPacket->getY());
			
			//------------------------------------------------------
			// The range is used as the direction.
			//------------------------------------------------------
			if ((*g_pActionInfoTable)[pPacket->getSkillType()].IsOptionRangeToDirection())
			{
				CreatureState::SetDirection(pCreature, pPacket->getRange() );
			}

			//------------------------------------------------------
			// Set how it acts.
			//------------------------------------------------------
			//Duration_t	m_Duration;
			CreatureState::PacketSpecialActionToSector(pCreature, skillID, 
								pPacket->getX(), pPacket->getY(),
								pResult		// result
			);		

//			_MinTrace("Incomming GCSkillToTileOK5Handler(SkillType:%d X:%d Y:%d Dir:%d",
//				pPacket->getSkillType(), pPacket->getX(), pPacket->getY(), pCreature->GetDirection() );

		
		}
	}


	__END_CATCH
}
