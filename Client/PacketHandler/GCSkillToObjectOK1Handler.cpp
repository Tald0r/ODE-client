//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSkillToObjectOK1Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSkillToObjectOK1.h"
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
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSkillToObjectOK1Handler::execute ( GCSkillToObjectOK1 * pPacket , Player * pPlayer )

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
		MCreature* pCreature = ZoneState::GetCreature( pPacket->getTargetObjectID() );

		// [ TEST CODE ]
		// An effect in progress must be added as a result.
		// Make the result..
		// g_pPlayer->>AddActionResult( ... )

		// 2005, 1, 3, sobeit add start
		if(pPacket->getSkillType() == SKILL_WILD_WOLF)
		{
			if(pCreature != NULL)
				WorldEvents::AddWildWolf(pPlayerCreature, pCreature, pPacket->getGrade()?true:false);
			else
			{
				// The target may lie in a corpse; the skill then goes on
				// against the corpse's creature.
				pCreature = ZoneState::GetCorpseCreature(pPacket->getTargetObjectID());
				if(pCreature != NULL)
				{
					WorldEvents::AddWildWolf(pPlayerCreature, pCreature, pPacket->getGrade()?true:false);
				}
			}
		}
		// 2005, 1, 3, sobeit add end
		// Damage the creature
		if (pCreature != NULL)
		{			
			int skillID = pPacket->getSkillType();
			
			if( g_pActionInfoTable->GetSize() <= skillID )
			{
				DEBUG_ADD_FORMAT_ERR("[Error] Exceed SkillType %d",skillID);
				
				SendBugReport("[Error:GCSTOOK1H] Exceed SkillType %d", skillID );
				return;
			}

			// 2004, 11, 13, sobeit add start 
			if(skillID == SKILL_PLASMA_ROCKET_LAUNCHER)
			{
				WorldEvents::AddRocketLauncher(pPlayerCreature, pCreature);
			}
			// 2004, 11, 13, sobeit add end
//			// 2005, 1, 3, sobeit add start
			else if(skillID == SKILL_HEAL_PASS)
			{
				WorldEvents::ExecuteActionInfoFromMainNode(SKILL_HEAL_PASS,CreatureState::GetX( pPlayerCreature ), CreatureState::GetY( pPlayerCreature ), 0,CreatureState::GetDirection( pPlayerCreature ),CreatureState::GetID( pCreature )	,	
					CreatureState::GetX( pCreature ), CreatureState::GetY( pCreature ), 0, 20 , NULL, false, 0, 0);
				
			}
//			// 2005, 1, 3, sobeit add end
			// A skill that takes the current weapon's action..
			if ((*g_pActionInfoTable)[skillID].IsAffectCurrentWeaponAction())
			{
				skillID = CreatureState::GetBasicActionInfo( pPlayerCreature );	

				if (static_cast<DWORD>(skillID) >= g_pActionInfoTable->GetMinResultActionInfo())
				{
					DEBUG_ADD_FORMAT_ERR("[Error] SkillType Error = %d", skillID);
					return;
				}
			}

			if( (*g_pActionInfoTable)[skillID].IsUseActionStep() && pPacket->getGrade() > 0)
				skillID = (*g_pActionInfoTable)[skillID].GetActionStep( pPacket->getGrade() - 1);
	
				
			// Exceptions by skill.
			switch( skillID )
			{
			case SKILL_SHADOW_DANCING :
				WorldEvents::SkillShadowDancing(pPlayerCreature, pCreature, SKILL_SINGLE_BLOW);
				break;
			// add by Coffee 2007-4-15
			case SKILL_ILLUSION_INVERSION:
				WorldEvents::SkillIllendue( pPlayerCreature, pCreature, TEMP_SKILL_ILLUSION_INVERSION);
				break;
			// end by Coffee
			case SKILL_ILLENDUE :
				// 2004, 12, 15, sobeit modify start - a healer using the enchant light ball was a bug.
				//SkillIllendue( g_pPlayer, pCreature, SKILL_LIGHT_BALL);
				WorldEvents::SkillIllendue( pPlayerCreature, pCreature, MAGIC_CAUSE_SERIOUS_WOUNDS);
				// 2004, 12, 15, sobeit modify end
				break;
				
			case SKILL_BLAZE_WALK :
				WorldEvents::SkillBlazeWalk( pPlayerCreature, pCreature, SKILL_BLAZE_WALK_ATTACK , pPacket->getGrade() );
				break;

			case SKILL_BITE_OF_DEATH :
				CreatureState::SetDrainCreatureID( pCreature, PlayerState::GetID() );
				break;
			case SKILL_SET_AFIRE:
				PlayerState::AddEffectStatus(EFFECTSTATUS_SET_AFIRE_ATTACK, 20);
				break;
			}
			
			MActionResult* pResult = new MActionResult;

			// The creature hit faces the player who hit it
			//pCreature->SetDirectionToPosition(g_pPlayer->GetX(), g_pPlayer->GetY());

			int delayFrame = ConvertDurationToFrame( pPacket->getDuration() );

			// Show the result
			//pCreature->PacketSpecialActionResult( pPacket->getSkillType() + g_ActionInfoTable.GetMinResultActionInfo());
			pResult->Add( new MActionResultNodeActionInfo( 
											skillID,
											PlayerState::GetID(), 
											CreatureState::GetID( pCreature ), 
											CreatureState::GetX( pCreature ),
											CreatureState::GetY( pCreature ),
											delayFrame ) 
							);	

			//------------------------------------------------------
			//
			// If the skill has a result, apply it.
			//
			//------------------------------------------------------
			MActionResultNode* pActionResultNode = CreateActionResultNode(pCreature, skillID, pPacket->getGrade() );

			//------------------------------------------------------
			// Run it if not NULL
			//------------------------------------------------------
			if (pActionResultNode!=NULL)
			{
				pResult->Add( pActionResultNode );
			}


			//------------------------------------------------------
			// Attach the effect status, if there is one.
			//------------------------------------------------------
			EFFECTSTATUS es = (*g_pActionInfoTable)[skillID].GetEffectStatus();
			
			if (es!=EFFECTSTATUS_NULL)
			{
				pResult->Add( new MActionResultNodeAddEffectStatus( CreatureState::GetID( pCreature ), 
																			es, 
																			delayFrame ) );
			}
			
			// Add it as the result of the player's skill in progress.
			PlayerState::PacketAddActionResult( pPacket->getCEffectID(), pResult );		
		}
		
	}
	


	__END_CATCH
}
