//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSkillToSelfOK1Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSkillToSelfOK1.h"
#include "ClientDef.h"
#include "PacketFunction2.h"
#include "SkillDef.h"
#include "MSkillManager.h"
#include "UIMessageManager.h"
#include "MEventManager.h"
#include "UIFunction.h"
#include "MGameStringTable.h"


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSkillToSelfOK1Handler::execute ( GCSkillToSelfOK1 * pPacket , Player * pPlayer )

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

	int skillID = pPacket->getSkillType();

	if( g_pActionInfoTable->GetSize() <= skillID )
	{
		DEBUG_ADD_FORMAT("[Error] Exceed SkillType %d",skillID);
		SendBugReport("[ErrorGCSTSOK1H] Exceed SkillType %d", skillID );
		return;
	}

	if( (*g_pActionInfoTable)[skillID].IsUseActionStep() && pPacket->getGrade() > 0)
		skillID = (*g_pActionInfoTable)[skillID].GetActionStep( pPacket->getGrade() - 1);

	int resultActionInfo = skillID + (*g_pActionInfoTable).GetMinResultActionInfo();
	
	if( skillID == MAGIC_UN_TRANSFORM)
	{
		// An ousters sends untransform when it gets off a summoned sylph,
		// and this arrives as the answer to that. It is handled here as
		// an exception rather than fixed.
		if(g_pPlayer->IsOusters())
		{
			g_pPlayer->SetWaitVerifyNULL();
			return;
		}
		else if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
		{
			return;
			//resultActionInfo = RESULT_SKILL_INSTALL_TURRET;
		}
	}
	
	if( skillID == SKILL_ETERNITY && g_pPlayer->IsSlayer() )
	{	
		MEvent event;
		event.eventID = EVENTID_RESURRECT;
		event.eventDelay = 5000;
		event.eventFlag = EVENTFLAG_SHOW_DELAY_STRING;
		event.eventType = EVENTTYPE_ZONE;
		event.m_StringsID.push_back(STRING_MESSAGE_RESURRECT_AFTER_SECONDS);
		
		g_pEventManager->AddEvent(event);
		UI_CloseRequestResurrectWindow();
	}

	//------------------------------------------------------------
	// Set the delay frame
	//------------------------------------------------------------
	DWORD delayFrame = ConvertDurationToFrame( pPacket->getDuration() );
	if(resultActionInfo == RESULT_SKILL_CONCEALMENT)				// round the frames to fit the effect's animation
	{
		int FrameSize = (*g_pActionInfoTable)[resultActionInfo][1].Count;
		int RemainFrame = delayFrame % FrameSize;
		if(RemainFrame < FrameSize / 2 && delayFrame > static_cast<DWORD>(FrameSize))
		{
			delayFrame -= RemainFrame;
		} else
		{
			delayFrame += FrameSize - RemainFrame;
		}
	}

	g_pPlayer->SetEffectDelayFrame(resultActionInfo, delayFrame );

	// Soul Chain's delay starts when it is confirmed, not when it is used.
	if(skillID == SKILL_SOUL_CHAIN)
	{
		if (skillID < MIN_RESULT_ACTIONINFO)
		{
			if (auto* entry = g_pSkillInfoTable->GetMutable(skillID)) {
				entry->SetNextAvailableTime();
			}
		}
//		g_pUIMessageManager->Execute(UI_CLOSE_TRACE_WINDOW, 0, 0, NULL);
	}

	//------------------------------------------------------
	// How the player looks when the skill succeeds
	//------------------------------------------------------
	g_pPlayer->PacketSpecialActionResult( 
					resultActionInfo,
					g_pPlayer->GetID(),
					g_pPlayer->GetX(),
					g_pPlayer->GetY()
	);

	//------------------------------------------------------------------
	// This packet arrives when the player's skill succeeded, so its
	// result is applied.
	//------------------------------------------------------------------
	// Change the status values.
	//------------------------------------------------------------------
	AffectModifyInfo(g_pPlayer, pPacket);

	//------------------------------------------------------------------
	// Apply the effect status.
	//------------------------------------------------------------------
	if (g_pPlayer->GetEFFECT_STAT()!=EFFECTSTATUS_NULL)
	{
		//int esDelayFrame = ConvertDurationToFrame( g_pPlayer->GetDURATION() );

		// Attach the effect.
		g_pPlayer->AddEffectStatus((EFFECTSTATUS)g_pPlayer->GetEFFECT_STAT(), delayFrame);	
		
		g_pPlayer->SetStatus( MODIFY_EFFECT_STAT, EFFECTSTATUS_NULL );
	}
	else
	{
		//------------------------------------------------------
		// Attach the skill's effect status, if it has one.
		//------------------------------------------------------
		EFFECTSTATUS es = (*g_pActionInfoTable)[skillID].GetEffectStatus();

		
		if (es!=EFFECTSTATUS_NULL)
		{
			g_pPlayer->AddEffectStatus( es, delayFrame );
		}
	}

	// Will of Life's reuse time depends on the vampire's level.
	if(skillID == SKILL_WILL_OF_LIFE )
	{
		g_pPlayer->CheckRegen();
		if (auto* entry = g_pSkillInfoTable->GetMutable(skillID)) {
			entry->SetAvailableTime( GetWillOfLifeDelay(UI_GetCharInfoLevel()) );
		}
	}

	//------------------------------------------------------------------
	// Update what the UI shows (setting it unconditionally is
	// presumably cheaper than comparing first).
	//------------------------------------------------------------------
	//UI_SetHP( g_pPlayer->GetHP(), g_pPlayer->GetMAX_HP() );
	//UI_SetMP( g_pPlayer->GetMP(), g_pPlayer->GetMAX_MP() );


	
	//------------------------------------------------------
	//
	// Apply the skill's result, if it has one.
	//
	//------------------------------------------------------
	MActionResultNode* pActionResultNode = CreateActionResultNode(g_pPlayer, skillID);

	//------------------------------------------------------
	// Run it if there is one
	//------------------------------------------------------
	if (pActionResultNode!=NULL)
	{
		pActionResultNode->Execute();

		delete pActionResultNode;
	}


	__END_CATCH
}
