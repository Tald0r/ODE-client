//////////////////////////////////////////////////////////////////////
//
// Filename    : GCThrowBombOK1Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCThrowBombOK1.h"
#include "AffectModifyInfo.h"
#include "DebugLog.h"
#include "ExplosiveActions.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCThrowBombOK1Handler::execute ( GCThrowBombOK1 * pPacket , Player * pPlayer )

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
		int skillID = GetBombActionInfo( pPacket->getItemType() );//pPacket->getSkillType();	

		if (skillID < 0)
		{
			DEBUG_ADD_FORMAT_ERR("[Error] no match skillType. itemType=%d", pPacket->getItemType());
			return;
		}

		int useSkillID = skillID;
		
		if ((*g_pActionInfoTable)[skillID].IsAffectCurrentWeaponAction())
		{
			useSkillID = CreatureState::GetBasicActionInfo(pPlayerCreature);

			if (static_cast<DWORD>(useSkillID) >= g_pActionInfoTable->GetMinResultActionInfo())
			{
				DEBUG_ADD_FORMAT_ERR("[Error] SkillType Error = %d", useSkillID);
				return;
			}
		}
			
		// (meaningless)
		int delayFrame = 16;//ConvertDurationToFrame( pPacket->getDuration() );

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

		pResult->Add( new MActionResultNodeActionInfo( 
											skillID,
											PlayerState::GetID(), 
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
		//if ((*g_pActionInfoTable)[skillID].IsOptionRangeToDirection())
		{
		//	g_pPlayer->SetDirection( pPacket->getDir() );	
		}		
	
		// Add it to the player's current skill as its result.		
		PlayerState::PacketAddActionResult( 0, pResult );

	}



	__END_CATCH
}
