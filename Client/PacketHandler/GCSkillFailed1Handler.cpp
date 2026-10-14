//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSkillFailed1Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files

#include "Client_PCH.h"
#include "Gpackets/GCSkillFailed1.h"
#include "AffectModifyInfo.h"
#include "DebugLog.h"
#include "GameInfoPresentationHost.h"
#include "MActionInfoTable.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"
#include "MSkillManager.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSkillFailed1Handler::execute ( GCSkillFailed1 * pPacket , Player * pPlayer )

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
	// The server has answered whether the skill the player was waiting on
	// succeeded.
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
	// The skill failed, so its delay is cleared (below).
	//------------------------------------------------------------------
	int skillID = pPacket->getSkillType();
	
	if( skillID >= g_pActionInfoTable->GetSize() )
		return;

	//------------------------------------------------------------------
	// Release the item lock.
	//------------------------------------------------------------------
	if (PlayerState::GetItemCheckBufferStatus()==PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY)
	{
		PlayerState::ClearItemCheckBuffer();
	}
	else if(CreatureState::IsOusters(pPlayerCreature) && skillID == SKILL_ABSORB_SOUL)
	{
//		_MinTrace(" -_-a failed\n");
		CreatureState::SetStopAbsorbSoul(pPlayerCreature);
	} else if (CreatureState::IsSlayer(pPlayerCreature) && skillID == SKILL_ETERNITY )
	{
//		g_pSystemMessage->Add( (*g_pGameStringTable)[UI_STRING_MESSAGE_CANNOT_RESURRECT].GetString() );
		GameInfoPresentation::SetDelayEternity();
	}

		
	if (g_pSkillInfoTable!=NULL)
	{
		// Clear the delay only for a skill the action table does not mark
		// as keeping its delay on failure (IsIgnoreSkillFailDelay; this
		// list was once hard-coded below).
		if(false == (*g_pActionInfoTable)[skillID].IsIgnoreSkillFailDelay())
//		if (skillID != MAGIC_PARALYZE
//			&& skillID != MAGIC_CAUSE_CRITICAL_WOUNDS
//			&& skillID != SKILL_ENERGY_DROP
//			&& skillID != SKILL_VIGOR_DROP
//			&& skillID != SKILL_POISON_STORM
//			&& skillID != SKILL_ACID_STORM
//			&& skillID != SKILL_BLOODY_STORM
//			&& skillID != SKILL_MAGIC_ELUSION
//			&& skillID != SKILL_POISON_MESH
//			&& skillID != SKILL_ILLUSION_OF_AVENGE			
//			)
		{
			if (skillID < MIN_RESULT_ACTIONINFO)
			{
				if (auto* entry = g_pSkillInfoTable->GetMutable(skillID)) {
					entry->SetAvailableTime();
				}
			}
		}

		// Will of Life's reuse time depends on the vampire's level. The
		// server sets the skill's run time only when its WillOfLife
		// handler ran and the time check passed; this packet also answers
		// a cast the server refused before running the handler (a
		// complete safe zone, Paralyze, Cause Critical Wounds, Explosion
		// Water, Coma, the werewolf form, no skill slot, a skill it may
		// not use) and one that failed the time check, all of which leave
		// the run time alone. The client cannot tell them apart, so it
		// waits the full reuse time after every failure: longer than the
		// server may, never shorter.
		if( skillID == SKILL_WILL_OF_LIFE )
		{
			if (auto* entry = g_pSkillInfoTable->GetMutable(skillID)) {
				entry->SetAvailableTime( GetWillOfLifeDelay(GameInfoPresentation::GetCharInfoLevel()) );
			}
		}
	}

	//------------------------------------------------------------------
	// By the kind of skill
	//------------------------------------------------------------------
	switch (skillID)
	{
		case SKILL_BITE_OF_DEATH :
		case SKILL_BLOOD_DRAIN :
			CreatureState::SetStopBloodDrain(pPlayerCreature);
//			DEBUG_ADD("blood drain failed");
//			g_pPlayer->StopBloodDrain();
			break;
		case SKILL_SOUL_CHAIN :
			//gC_vs_ui.SetCannotTrace();
			PlayerState::SetCannotTrace();
			break;		
		case SKILL_BURNING_SOUL_CHARGING:
			CreatureState::RemoveEffectStatus(pPlayerCreature, EFFECTSTATUS_BURNING_SOL_CHARGE_1);
			break;
	}

	//------------------------------------------------------------------
	// Change the status values.
	//------------------------------------------------------------------
	if (MStatus* pPlayerStatus = PlayerState::Status())
		AffectModifyInfo(pPlayerStatus, pPacket);

	//------------------------------------------------------------------
	// Update what the UI shows (setting it unconditionally is
	// presumably cheaper than comparing first).
	//------------------------------------------------------------------
	//UI_SetHP( g_pPlayer->GetHP(), g_pPlayer->GetMAX_HP() );
	//UI_SetMP( g_pPlayer->GetMP(), g_pPlayer->GetMAX_MP() );


	__END_CATCH
}
