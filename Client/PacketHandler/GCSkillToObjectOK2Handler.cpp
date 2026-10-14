//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSkillToObjectOK2Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSkillToObjectOK2.h"
#include "AffectModifyInfo.h"
#include "ConvertDuration.h"
#include "DebugLog.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "MSkillManager.h"
#include "Packet/WireHost.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSkillToObjectOK2Handler::execute ( GCSkillToObjectOK2 * pPacket , Player * pPlayer )

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


	int skillID = pPacket->getSkillType();
	
	if( g_pActionInfoTable->GetSize() <= skillID )
	{
		DEBUG_ADD_FORMAT_ERR("[Error] Exceed SkillType %d",skillID);
		SendBugReport("[Error:GCSTOOK2H] Exceed SkillType %d", skillID );
		return;
	}


	if( (*g_pActionInfoTable)[skillID].IsUseActionStep() && pPacket->getGrade() > 0 )
		skillID = (*g_pActionInfoTable)[skillID].GetActionStep( pPacket->getGrade() - 1);

	int delayFrame = ConvertDurationToFrame( pPacket->getDuration() );

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
		MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );


		// When I (the player) am hit by someone's skill..
		// [ TEST CODE ]
		MActionResult* pResult = new MActionResult;

		// 2005, 1, 3, sobeit add start
		if(pPacket->getSkillType() == SKILL_WILD_WOLF)
		{
			if(pCreature != NULL)
				WorldEvents::AddWildWolf(pCreature, pPlayerCreature, pPacket->getGrade()?true:false);
			else
			{
				// The user may lie in a corpse; the skill then goes on
				// from the corpse's creature.
				pCreature = ZoneState::GetCorpseCreature(pPacket->getObjectID());
				if(pCreature != NULL)
				{
					WorldEvents::AddWildWolf(pCreature, pPlayerCreature, pPacket->getGrade()?true:false);
				}
			}
		}
		// 2005, 1, 3, sobeit add end
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
		else
		{
			//------------------------------------------------------
			// Attach the effect status, if there is one.
			//------------------------------------------------------
			EFFECTSTATUS es = (*g_pActionInfoTable)[skillID].GetEffectStatus();
			
			if (es!=EFFECTSTATUS_NULL)
			{
				pResult->Add( new MActionResultNodeAddEffectStatus( PlayerState::GetID(),
																		es, 
																		delayFrame ) );
			}
		}

		//------------------------------------------------------
		// [new skill]
		// One hit by Typhoon cannot move for a while,
		// for now (-_-;) a second.
		// Only the player needs stopping.
		//------------------------------------------------------
		if (skillID==SKILL_TYPHOON || skillID == SKILL_WILD_TYPHOON)
		{
			if( pPacket->getGrade() == 1 || skillID == SKILL_WILD_TYPHOON )
				PlayerState::SetDelay( 1000 );

		}
		// 2004, 11, 13, sobeit add start 
		if(skillID == SKILL_PLASMA_ROCKET_LAUNCHER)
		{
			WorldEvents::AddRocketLauncher(pCreature, pPlayerCreature);
		}
		// 2004, 11, 13, sobeit add end
	
		// A skill that takes the current weapon's action..
		if ((*g_pActionInfoTable)[skillID].IsAffectCurrentWeaponAction()
			&& pCreature!=NULL)
		{
			skillID = CreatureState::GetBasicActionInfo( pCreature );

			if (static_cast<DWORD>(skillID) >= g_pActionInfoTable->GetMinResultActionInfo())
			{
				DEBUG_ADD_FORMAT_ERR("[Error] SkillType Error = %d", skillID);
				return;
			}
		}

		pResult->Add( new MActionResultNodeActionInfo( 
								skillID, 
								pPacket->getObjectID(), 
								PlayerState::GetID(), 
								CreatureState::GetX( pPlayerCreature ),
								CreatureState::GetY( pPlayerCreature ),
								delayFrame ) );	

		//------------------------------------------------------
		//
		// If the skill has a result, apply it.
		//
		//------------------------------------------------------
		MActionResultNode* pActionResultNode = CreateActionResultNode(pPlayerCreature, skillID, pPacket->getGrade() );

		//------------------------------------------------------
		// Run it if not NULL
		//------------------------------------------------------
		if (pActionResultNode!=NULL)
		{
			pResult->Add( pActionResultNode );
		}

		//------------------------------------------------------
		// Without the user
		//------------------------------------------------------
		if (pCreature == NULL)
		{
			WorldEvents::ExecuteActionInfoFromMainNode(
						static_cast<TYPE_ACTIONINFO>(skillID + (*g_pActionInfoTable).GetMinResultActionInfo()),										// the skill used
					
						CreatureState::GetX( pPlayerCreature ), CreatureState::GetY( pPlayerCreature ), CreatureState::GetZ( pPlayerCreature ),
						CreatureState::GetDirection( pPlayerCreature ),														// the direction used
						
						PlayerState::GetID(),												// the target
						CreatureState::GetX( pPlayerCreature ), CreatureState::GetY( pPlayerCreature ), CreatureState::GetZ( pPlayerCreature ),
						
						delayFrame,													// the skill's (remaining) duration		
						
						pResult,
						
						false, 0, 0);			// start the skill from its beginning.
		}
		//------------------------------------------------------
		// With the user
		//------------------------------------------------------
		else
		{
			if(skillID == SKILL_ABSORB_SOUL)
				CreatureState::ClearStopBloodDrain( pCreature );

			// [new skill 2]
			// Check for Shadow Dancing, and use Double Impact for now (-_-;).

			switch( skillID )
			{
			case SKILL_SHADOW_DANCING :
				WorldEvents::SkillShadowDancing(pCreature, pPlayerCreature, SKILL_SINGLE_BLOW);
				break;
			// add by Coffee 2007-4-15
			case SKILL_ILLUSION_INVERSION:
				WorldEvents::SkillIllendue( pCreature, pPlayerCreature, TEMP_SKILL_ILLUSION_INVERSION);
				break;
			// end by Coffee	
			case SKILL_ILLENDUE :
				// 2004, 12, 15, sobeit modify start - a healer using the enchant light ball was a bug.
				//SkillIllendue( pCreature, g_pPlayer, SKILL_LIGHT_BALL );
				WorldEvents::SkillIllendue( pCreature, pPlayerCreature, MAGIC_CAUSE_SERIOUS_WOUNDS );
				// 2004, 12, 15, sobeit modify end
				break;
				
			case SKILL_BLAZE_WALK :
				WorldEvents::SkillBlazeWalk( pCreature , pPlayerCreature, SKILL_BLAZE_WALK_ATTACK, pPacket->getGrade()  );
				break;

			case SKILL_BITE_OF_DEATH :
				CreatureState::SetDrainCreatureID( pPlayerCreature, pPacket->getObjectID() );
				break;
			case SKILL_SET_AFIRE:
				CreatureState::AddEffectStatus( pCreature, EFFECTSTATUS_SET_AFIRE_ATTACK, 20 );
				break;
			}
			
			//------------------------------------------------------
			// When the player is protected
			//------------------------------------------------------
			if (PlayerState::HasEffectStatus(EFFECTSTATUS_PROTECTION_FROM_ACID))
			{
				//------------------------------------------------------
				// PROTECTION_FROM_ACID
				//------------------------------------------------------
				if ((*g_pSkillInfoTable)[skillID].GetSkillStep()==SKILL_STEP_VAMPIRE_ACID)
				{
					skillID = PROTECTION_FROM_ACID;					
				}				
			}			


			// [ TEST CODE ]
			//
			// The result must be made, stored and sent.
			//
			// Face the direction
			CreatureState::SetDirectionToPosition( pCreature, CreatureState::GetX( pPlayerCreature ), CreatureState::GetY( pPlayerCreature ));

			//Duration_t	m_Duration;
			CreatureState::PacketSpecialActionToOther( pCreature,
								skillID, 
								PlayerState::GetID(),//pPacket->getObjectID(),
								pResult						// the result
			);
		}
	}		
	


	//------------------------------------------------------------------
	// Update what the UI shows.
	// (Setting it beats comparing first.)
	//------------------------------------------------------------------
	//UI_SetHP( g_pPlayer->GetHP(), g_pPlayer->GetMAX_HP() );
	//UI_SetMP( g_pPlayer->GetMP(), g_pPlayer->GetMAX_MP() );


	__END_CATCH
}
