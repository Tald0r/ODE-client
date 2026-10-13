//////////////////////////////////////////////////////////////////////
//
// Filename    : GCBloodDrainOK1Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
// include files
#include "Gpackets/GCBloodDrainOK1.h"
#include "AffectModifyInfo.h"
#include "DebugLog.h"
#include "MActionInfoTable.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCBloodDrainOK1Handler::execute ( GCBloodDrainOK1 * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	// The player; without one there is nothing to do (the game crashed here).
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
		MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );

		// The creature is hit.
		if (pCreature != NULL)
		{	
			// Tell the creature the player drains its blood.
			CreatureState::SetDrainCreatureID(pCreature, PlayerState::GetID() );

			// Show the result at once.
			CreatureState::PacketSpecialActionResult(pCreature, static_cast<TYPE_ACTIONINFO>(SKILL_BLOOD_DRAIN + (*g_pActionInfoTable).GetMinResultActionInfo()),
					CreatureState::GetID(pCreature),
					CreatureState::GetX(pCreature),
					CreatureState::GetY(pCreature)
			);	

			int delayFrame = 691200;	// 12 hours of 16 frames a second: 12*60*60*16
			CreatureState::AddEffectStatus(pCreature, EFFECTSTATUS_BLOOD_DRAIN, delayFrame );	
			
//			// 2004-06-09: a test, acting after the drain-success message
//			if(g_pPlayer->IsVampire())
//			{
//				g_pPlayer->SetDirectionToPosition(pCreature->GetX(), pCreature->GetY());
//				g_pPlayer->SetAction(ACTION_VAMPIRE_DRAIN);
//				
//			}
//			// 2004, 6, 9 sobeit add end
		}
	}
	
	CreatureState::ClearStopBloodDrain(pPlayerCreature);


	__END_CATCH
}
