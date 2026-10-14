//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAddEffectHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddEffect.h"
#include "ConvertDuration.h"
#include "DebugLog.h"
#include "MEffectStatusTable.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "MEffectStatusDef.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAddEffectHandler::execute ( GCAddEffect * pPacket , Player * pPlayer )
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
		int CreatureID = pPacket->getObjectID();
		
		MCreature* pCreature = ZoneState::GetCreature(CreatureID);

		// Not standing: the creature lying in the corpse with this ID (the
		// relic shelf is one).
		if (pCreature == NULL)
		{
			pCreature = ZoneState::GetCorpseCreature(CreatureID);
		}

		if (pCreature==NULL)
		{
			DEBUG_ADD_FORMAT_ERR("[Error] the Creature is Not Exist. id=%d", pPacket->getObjectID());
		}
		else
		{
			// Checked before it becomes an EFFECTSTATUS: the wire's 16 bits
			// run past the enum's range.
			if(pPacket->getEffectID() >= EFFECTSTATUS_MAX)
				return;

			EFFECTSTATUS status = (EFFECTSTATUS)pPacket->getEffectID();
//			if( status == EFFECTSTATUS_VIEW_HP )
//			{
//				int a = 0;
//			}
		
			
			if( (*g_pEffectStatusTable)[ status ].SoundID != SOUNDID_NULL )
			{
				WorldEvents::PlaySound( (*g_pEffectStatusTable)[ status ].SoundID );
			}
			
			// For now...
			// ignore doom,
			// and extreme.
			if (status == EFFECTSTATUS_BACK_STAB_3 || status == EFFECTSTATUS_ICE_OF_SOUL_STONE ||
				status == EFFECTSTATUS_TRAP_TRIGGERED || status == EFFECTSTATUS_TRAPPED ||
				status == EFFECTSTATUS_TRYING || status == EFFECTSTATUS_GROUND_ELEMENTAL_CENTER)
			{
				// The third stage has its own action info.
				TYPE_ACTIONINFO		ActionInfo = (*g_pEffectStatusTable)[ status ].ActionInfo;
				if( ActionInfo != ACTIONINFO_NULL )
				{
					DWORD delayFrame = 18;
					
					if( status == EFFECTSTATUS_ICE_OF_SOUL_STONE )
						delayFrame = 19;

					if( status == EFFECTSTATUS_TRYING || status == EFFECTSTATUS_GROUND_ELEMENTAL_CENTER)
						delayFrame = 0xFFFF;

					if( status == EFFECTSTATUS_TRAP_TRIGGERED || status == EFFECTSTATUS_TRAPPED )
						delayFrame = ConvertDurationToFrame( pPacket->getDuration() );
					
					WorldEvents::ExecuteActionInfoFromMainNode(
						ActionInfo,						
						CreatureState::GetX(pCreature), CreatureState::GetY(pCreature), 0,
						CreatureState::GetDirection(pCreature),						
						CreatureState::GetID(pCreature),
						CreatureState::GetX(pCreature), CreatureState::GetY(pCreature), 0,						
						delayFrame,						
						NULL, status == EFFECTSTATUS_TRAP_TRIGGERED
						);
				}
			}
			// 2004, 9, 3, sobeit add start
			else
			if(status == EFFECTSTATUS_TURRET_LASER && PlayerState::Creature() != NULL) // a siege trap's beam the attackers do not see
			{
				if(PlayerState::HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_1) || 
					PlayerState::HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_2) || 
					PlayerState::HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_3) || 
					PlayerState::HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_4) || 
					PlayerState::HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_5) )
					return;
			}
			// 2004, 9, 3, sobeit add end
			if (status!=EFFECTSTATUS_CURSE_DOOM
				&& status!=EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS
				&& status!=EFFECTSTATUS_BACK_STAB_3
				//&& status!=EFFECTSTATUS_EXTREME
				)
			{
				// The delay
				DWORD delayFrame = ConvertDurationToFrame( pPacket->getDuration() );

				// Add the effect status.
				CreatureState::AddEffectStatus(pCreature, status, delayFrame);
			}
			
			switch( status )
			{
			case EFFECTSTATUS_SLAYER_TRYING_1:                   // 320
			case EFFECTSTATUS_SLAYER_TRYING_2:                   // 321
			case EFFECTSTATUS_SLAYER_TRYING_3:                   // 322
			case EFFECTSTATUS_VAMPIRE_TRYING_1:                  // 323
			case EFFECTSTATUS_VAMPIRE_TRYING_2:                  // 324
			case EFFECTSTATUS_VAMPIRE_TRYING_3:                  // 325
			case EFFECTSTATUS_OUSTERS_TRYING_1:                  // 326
			case EFFECTSTATUS_OUSTERS_TRYING_2:                  // 327
			case EFFECTSTATUS_OUSTERS_TRYING_3:                  // 328
				{
					WorldEvents::ExecuteActionInfoFromMainNode(
						REGEN_TOWER_FLASH_BEAD,
						CreatureState::GetX(pCreature), CreatureState::GetY(pCreature), 0,
						DIRECTION_DOWN,
						CreatureState::GetID(pCreature),
						CreatureState::GetX(pCreature), CreatureState::GetY(pCreature), 0,
						7,
						NULL );
						
				}
				break;

			default:
				break;
			}
		}
	}


	__END_CATCH
}