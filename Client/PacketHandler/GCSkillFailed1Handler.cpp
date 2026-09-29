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
#include "ClientDef.h"
#include "SkillDef.h"
#include "MSkillManager.h"
#include "MinTr.h"
#include "UIFunction.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSkillFailed1Handler::execute ( GCSkillFailed1 * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

	//------------------------------------------------------------------
	// The server has answered whether the skill the player was waiting on
	// succeeded.
	//------------------------------------------------------------------	
	if (g_pPlayer->GetWaitVerify()==MPlayer::WAIT_VERIFY_SKILL_SUCCESS)
	{		
		g_pPlayer->SetWaitVerifyNULL();
	}
	else
	{
		DEBUG_ADD("[Error] Player is not WaitVerifySkillSuccess");
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
	if (g_pPlayer->GetItemCheckBufferStatus()==MPlayer::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY)
	{
		g_pPlayer->ClearItemCheckBuffer();
	}
	else if(g_pPlayer->IsOusters() && skillID == SKILL_ABSORB_SOUL)
	{
//		_MinTrace(" -_-a failed\n");
		g_pPlayer->SetStopAbsorbSoul();
	} else if (g_pPlayer->IsSlayer() && skillID == SKILL_ETERNITY )
	{
//		g_pSystemMessage->Add( (*g_pGameStringTable)[UI_STRING_MESSAGE_CANNOT_RESURRECT].GetString() );
		UI_SetDelayEternity();
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

		// Will of Life's reuse time depends on the vampire's level.
		if( skillID == SKILL_WILL_OF_LIFE )
		{
			if (auto* entry = g_pSkillInfoTable->GetMutable(skillID)) {
				entry->SetAvailableTime( GetWillOfLifeDelay(UI_GetCharInfoLevel()) );
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
			g_pPlayer->SetStopBloodDrain();
//			DEBUG_ADD("blood drain failed");
//			g_pPlayer->StopBloodDrain();
			break;
		case SKILL_SOUL_CHAIN :
			//gC_vs_ui.SetCannotTrace();
			g_pPlayer->SetCannotTrace();
			break;		
		case SKILL_BURNING_SOUL_CHARGING:
			g_pPlayer->RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_1);
			break;
	}

	//------------------------------------------------------------------
	// Change the status values.
	//------------------------------------------------------------------
	AffectModifyInfo(g_pPlayer, pPacket);

	//------------------------------------------------------------------
	// Update what the UI shows (setting it unconditionally is
	// presumably cheaper than comparing first).
	//------------------------------------------------------------------
	//UI_SetHP( g_pPlayer->GetHP(), g_pPlayer->GetMAX_HP() );
	//UI_SetMP( g_pPlayer->GetMP(), g_pPlayer->GetMAX_MP() );


	__END_CATCH
}
