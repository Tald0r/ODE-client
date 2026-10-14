//////////////////////////////////////////////////////////////////////
//
// Filename    : GCThrowBombOK2Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCThrowBombOK2.h"
#include "AffectModifyInfo.h"
#include "DebugLog.h"
#include "ExplosiveActions.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "MEffectStatusDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCThrowBombOK2Handler::execute ( GCThrowBombOK2 * pPacket , Player * pPlayer )

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

	// (meaningless)
	int delayFrame = 16;//ConvertDurationToFrame( pPacket->getDuration() );

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
		if (pPacket->getObjectID()==PlayerState::GetID())
		{
			DEBUG_ADD_ERR("[Error] the User of GCSkillToBombOK2 is Player!");
		}
		else
		{
			//------------------------------------------------------
			// the user
			//------------------------------------------------------
			int creatureID = pPacket->getObjectID();
			MCreature* pCreature = ZoneState::GetCreature( creatureID );

			int skillID = GetBombActionInfo( pPacket->getItemType() );//pPacket->getSkillType();	

			if (skillID < 0)
			{
				DEBUG_ADD_FORMAT_ERR("[Error] no match skillType. itemType=%d", pPacket->getItemType());
				return;
			}

			int useSkillID = skillID;

			DEBUG_ADD("CurWA");

			if ((*g_pActionInfoTable)[skillID].IsAffectCurrentWeaponAction()
				&& pCreature!=NULL)
			{
				DEBUG_ADD("SetBA");

				useSkillID = CreatureState::GetBasicActionInfo(pCreature);

				if (static_cast<DWORD>(useSkillID) >= g_pActionInfoTable->GetMinResultActionInfo())
				{
					DEBUG_ADD_FORMAT_ERR("[Error] SkillType Error = %d", useSkillID);
					return;
				}
			}

			//------------------------------------------------------
			// The result of the bomb (OK2). 
			//------------------------------------------------------				
			int size = pPacket->getCListNum();

			MActionResult* pResult = new MActionResult;

			//------------------------------------------------------------------
			// Apply the effect status.
			//------------------------------------------------------------------
			MStatus* pPlayerStatus = PlayerState::Status();
			if (pPlayerStatus != NULL && pPlayerStatus->GetEFFECT_STAT()!=EFFECTSTATUS_NULL)
			{
				DEBUG_ADD("AddES");

				pResult->Add( new MActionResultNodeAddEffectStatus( 
											PlayerState::GetID(), 
											(EFFECTSTATUS)pPlayerStatus->GetEFFECT_STAT(), 
											delayFrame ));

				pPlayerStatus->SetStatus( MODIFY_EFFECT_STAT, EFFECTSTATUS_NULL );
			}

			DEBUG_ADD("AResultID?");

			int targetID = creatureID;
			if ((*g_pActionInfoTable)[skillID].GetActionResultID()==ACTIONRESULTNODE_ACTIONINFO)
			{
				targetID = OBJECTID_NULL;
			}

			DEBUG_ADD("AddAI");

			pResult->Add( new MActionResultNodeActionInfo( 
												skillID,
												creatureID, 
												targetID,
												pPacket->getX(),
												pPacket->getY(),
												delayFrame ) 
								);	
		
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

					DEBUG_ADD("getTC");

					MCreature* pTargetCreature = ZoneState::GetCreature( targetID );

					if (pTargetCreature!=NULL)
					{
						DEBUG_ADD("TCNotNULL");

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
								
								DEBUG_ADD("ANodeAI");

								pActionResultNode =  new MActionResultNodeActionInfo( 
																	useSkillID, //(*g_pActionInfoTable)[skillID].GetActionResultValue(),
																	creatureID, 
																	targetID, 
																	CreatureState::GetX(pTargetCreature),
																	CreatureState::GetY(pTargetCreature),
																	delayFrame);

								DEBUG_ADD("ANodeAI-ok");
							break;

							//------------------------------------------------------
							// Burrow and the like. 
							//------------------------------------------------------
							default :
								DEBUG_ADD("default");
								pActionResultNode = CreateActionResultNode(pTargetCreature, useSkillID);
						}

						//------------------------------------------------------
						// Apply it too, if there is one.
						//------------------------------------------------------
						if (pActionResultNode!=NULL)
						{
							DEBUG_ADD("add");
							pResult->Add( pActionResultNode );
							DEBUG_ADD("ok");
						}
					}
					else
					{
						DEBUG_ADD("TCNull");
					}
				}
			}


			DEBUG_ADD("pCreture?");

			//------------------------------------------------------
			// No user: show the result at once.
			//------------------------------------------------------
			if (pCreature==NULL)
			{
				DEBUG_ADD_FORMAT_ERR("[Error] The Creature is not Exist. id=%d", pPacket->getObjectID());
				
				int direction = pPacket->getDir();
				
				WorldEvents::ExecuteActionInfoFromMainNode(
							skillID,										// the skill used
						
							pPacket->getX(), pPacket->getY(), 0,
							direction,														// direction
							
							OBJECTID_NULL,		// changed 2002-03-06; it was pPacket->getObjectID()
							pPacket->getX(), pPacket->getY(), 0,
							
							delayFrame,													// the skill's (remaining) duration		
							
							pResult,
							
							false);			// start the skill from its beginning);
			}
			//------------------------------------------------------
			// The normal case.
			//------------------------------------------------------
			else
			{				
				DEBUG_ADD("C-OK");

				//------------------------------------------------------
				// Face the cell.
				//------------------------------------------------------
				CreatureState::SetDirectionToPosition(pCreature, pPacket->getX(), pPacket->getY());		

				//------------------------------------------------------
				// The range is used as the direction.
				//------------------------------------------------------
				CreatureState::SetDirection(pCreature, pPacket->getDir() );				

				//------------------------------------------------------
				// Add the bomb's (OK2) result.
				//------------------------------------------------------
				//Duration_t	m_Duration;
				DEBUG_ADD("AtoSector");

				CreatureState::PacketSpecialActionToSector(pCreature, skillID, 
									pPacket->getX(), pPacket->getY(),
									pResult						// result
				);

				DEBUG_ADD("AtoSectorOK");
			}
			
		}
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
	
	DEBUG_ADD("BombOK2End");


	__END_CATCH
}
