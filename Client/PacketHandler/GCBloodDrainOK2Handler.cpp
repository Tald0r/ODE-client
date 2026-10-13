//////////////////////////////////////////////////////////////////////
//
// Filename    : GCBloodDrainOK2Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
// include files
#include "Gpackets/GCBloodDrainOK2.h"
#include "AffectModifyInfo.h"
#include "ConvertDuration.h"
#include "DebugLog.h"
#include "MActionResult.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCBloodDrainOK2Handler::execute ( GCBloodDrainOK2 * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	// The player; without one there is nothing to do.
	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		return;
	}
		


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

		// The player learns who drains it.
		CreatureState::SetDrainCreatureID(pPlayerCreature, pPacket->getObjectID() );
		
		// The creature is hit.
		if (pCreature == NULL)
		{
			// The player is hit at once.
			CreatureState::PacketSpecialActionResult(pPlayerCreature, RESULT_SKILL_BLOOD_DRAIN,
										PlayerState::GetID(),	
										CreatureState::GetX(pPlayerCreature),
										CreatureState::GetY(pPlayerCreature)
			 );
		}
		else
		{
			CreatureState::ClearStopBloodDrain(pCreature);

			// The player is hit by someone's skill.
			// [ TEST CODE ]
			MActionResult* pResult = new MActionResult;
			pResult->Add( new MActionResultNodeActionInfo( 
										SKILL_BLOOD_DRAIN,
										CreatureState::GetID(pCreature), 
										PlayerState::GetID(), 
										CreatureState::GetX(pPlayerCreature),
										CreatureState::GetY(pPlayerCreature)
										 ) );		

			// [ TEST CODE ]
			//
			// Make the result and hand it over.
			//
			// Face the player.
			CreatureState::SetDirectionToPosition(pCreature, CreatureState::GetX(pPlayerCreature), CreatureState::GetY(pPlayerCreature));

			//Duration_t	m_Duration;
			CreatureState::PacketSpecialActionToOther(pCreature, SKILL_BLOOD_DRAIN,
								PlayerState::GetID(),
								pResult						// result
			);
		}
	}	
	
	//------------------------------------------------------
	// The player being hit.
	//------------------------------------------------------
	//------------------------------------------------------------
	// The delay frame
	//------------------------------------------------------------
	//g_pPlayer->SetEffectDelayFrame( resultActionInfo, delayFrame );

	//------------------------------------------------------------------
	// Apply the status change.
	//------------------------------------------------------------------
	MStatus* pPlayerStatus = PlayerState::Status();
	if (pPlayerStatus != NULL)
	{
		AffectModifyInfo(pPlayerStatus, pPacket);

		//------------------------------------------------------------------
		// Bitten.
		//------------------------------------------------------------------
		if (pPlayerStatus->GetEFFECT_STAT()!=EFFECTSTATUS_NULL)
		{
			int delayFrame = ConvertDurationToFrame( pPlayerStatus->GetDURATION() );

			CreatureState::AddEffectStatus(pPlayerCreature, (EFFECTSTATUS)pPlayerStatus->GetEFFECT_STAT(), delayFrame);	
			
			pPlayerStatus->SetStatus( MODIFY_EFFECT_STAT, EFFECTSTATUS_NULL );
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
