//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSkillToObjectOK5Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSkillToObjectOK5.h"
#include "ConvertDuration.h"
#include "DebugLog.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Packet/WireHost.h"

// [new skill 2]
#include "SkillDef.h"	
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSkillToObjectOK5Handler::execute ( GCSkillToObjectOK5 * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		


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
		int skillID = pPacket->getSkillType();

		if( g_pActionInfoTable->GetSize() <= skillID )
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Exceed SkillType %d",skillID);
			SendBugReport("[Error:GCSTOOK5H] Exceed SkillType %d", skillID );
			return;
		}


		MCreature* pUserCreature = ZoneState::GetCreature( pPacket->getObjectID() );
		MCreature* pTargetCreature = ZoneState::GetCreature( pPacket->getTargetObjectID() );		
//		if(pTargetCreature == NULL && skillID == SKILL_ABSORB_SOUL && pUserCreature != NULL && pUserCreature->IsOusters())
//		{
//			MItem *pItem = g_pZone->GetItem(pPacket->getTargetObjectID());
//			if(pItem != NULL)
//			{
//				pTargetCreature = ((MCorpse *)pItem)->GetCreature();
//			}
//		}
		// 2005, 1, 3, sobeit add start
		if(pPacket->getSkillType() == SKILL_WILD_WOLF && pUserCreature != NULL)
		{
			if(pTargetCreature != NULL)
				WorldEvents::AddWildWolf(pUserCreature, pTargetCreature, pPacket->getGrade()?true:false);
			else
			{
				// The target may lie in a corpse; the skill then goes on
				// against the corpse's creature.
				pTargetCreature = ZoneState::GetCorpseCreature(pPacket->getTargetObjectID());
				if(pTargetCreature != NULL)
				{
					WorldEvents::AddWildWolf(pUserCreature, pTargetCreature, pPacket->getGrade()?true:false);
				}
			}
		}
		// 2005, 1, 3, sobeit add end
	
		// Damage the creature
		if (pUserCreature != NULL && pTargetCreature != NULL)
		{
			// 2004, 11, 13, sobeit add start 
			if(skillID == SKILL_PLASMA_ROCKET_LAUNCHER)
			{
				WorldEvents::AddRocketLauncher(pUserCreature, pTargetCreature);
			}
			// 2004, 11, 13, sobeit add end
		

			if ((*g_pActionInfoTable)[skillID].IsAffectCurrentWeaponAction())
			{
				skillID = CreatureState::GetBasicActionInfo( pUserCreature );

				if (static_cast<DWORD>(skillID) >= g_pActionInfoTable->GetMinResultActionInfo())
				{
					DEBUG_ADD_FORMAT_ERR("[Error] SkillType Error = %d", skillID);
					return;
				}
			}
			if( (*g_pActionInfoTable)[skillID].IsUseActionStep() && pPacket->getGrade() > 0)
				skillID = (*g_pActionInfoTable)[skillID].GetActionStep(pPacket->getGrade() - 1);

			// [new skill 2]
			// Check for Shadow Dancing, and use Double Impact for now (-_-;).
			switch( skillID )
			{
			case SKILL_SHADOW_DANCING :
				WorldEvents::SkillShadowDancing(pUserCreature, pTargetCreature, SKILL_SINGLE_BLOW);
				break;
			// add by Coffee 2007-4-15
			case SKILL_ILLUSION_INVERSION:
				WorldEvents::SkillIllendue( pUserCreature, pTargetCreature, TEMP_SKILL_ILLUSION_INVERSION);
				break;
			// end by Coffee
			case SKILL_ILLENDUE :
				// 2004, 12, 15, sobeit modify start - a healer using the enchant light ball was a bug.
				//SkillIllendue( pUserCreature, pTargetCreature, SKILL_LIGHT_BALL );
				WorldEvents::SkillIllendue( pUserCreature, pTargetCreature, MAGIC_CAUSE_SERIOUS_WOUNDS );
				// 2004, 12, 15, sobeit modify end
				break;

			case SKILL_BLAZE_WALK :
				WorldEvents::SkillBlazeWalk( pUserCreature, pTargetCreature, SKILL_BLAZE_WALK_ATTACK, pPacket->getGrade()  );
				break;
			case SKILL_SET_AFIRE:
				CreatureState::AddEffectStatus( pUserCreature, EFFECTSTATUS_SET_AFIRE_ATTACK, 20 );
				break;
			}

			// [ TEST CODE ]
			//
			// The result must be made, stored and sent.
			//
			// When someone (the target) is hit by someone's skill..
			// [ TEST CODE ]
			MActionResult* pResult = new MActionResult;

			DWORD delayFrame = ConvertDurationToFrame( pPacket->getDuration() );

			pResult->Add( new MActionResultNodeActionInfo( 
										skillID, 
										pPacket->getObjectID(), 
										pPacket->getTargetObjectID(), 
										CreatureState::GetX( pTargetCreature ),
										CreatureState::GetY( pTargetCreature ),
										delayFrame ) );

			//------------------------------------------------------
			//
			// If the skill has a result, apply it.
			//
			//------------------------------------------------------
			MActionResultNode* pActionResultNode = CreateActionResultNode(pTargetCreature, skillID, pPacket->getGrade() );

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
				pResult->Add( new MActionResultNodeAddEffectStatus( CreatureState::GetID( pTargetCreature ),
																		es, 
																		delayFrame ) );
			}


			// Face each other
			CreatureState::SetDirectionToPosition( pUserCreature, CreatureState::GetX( pTargetCreature ), CreatureState::GetY( pTargetCreature ));
			//pTargetCreature->SetDirectionToPosition(pUserCreature->GetX(), pUserCreature->GetY());

			//Duration_t	m_Duration;
			CreatureState::PacketSpecialActionToOther( pUserCreature,
								skillID, /*pPacket->getSkillType(), */
								pPacket->getTargetObjectID(),
								pResult			// the result
			);		

			if( skillID == SKILL_BITE_OF_DEATH )
				CreatureState::SetDrainCreatureID( pTargetCreature, pPacket->getObjectID() );
		}
	}	


	__END_CATCH
}
