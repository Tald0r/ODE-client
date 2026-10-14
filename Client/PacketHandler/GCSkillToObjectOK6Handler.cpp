//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSkillToObjectOK6Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSkillToObjectOK6.h"
#include "AffectModifyInfo.h"
#include "ConvertDuration.h"
#include "DebugLog.h"
#include "DirectionSelection.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "MStatus.h"
#include "Packet/WireHost.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSkillToObjectOK6Handler::execute ( GCSkillToObjectOK6 * pPacket , Player * pPlayer )

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

	int delayFrame = ConvertDurationToFrame( pPacket->getDuration() );

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
		int skillID = pPacket->getSkillType();

		if( g_pActionInfoTable->GetSize() <= skillID )
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Exceed SkillType %d",skillID);
			SendBugReport("[ErrorGCSTOOK6H] Exceed SkillType %d", skillID );
			return;
		}


		if( (*g_pActionInfoTable)[skillID].IsUseActionStep() && pPacket->getGrade() > 0)
			skillID = (*g_pActionInfoTable)[skillID].GetActionStep( pPacket->getGrade() - 1);
	
		//------------------------------------------------------
		// With a user.
		//------------------------------------------------------
		// The player is hit by someone's skill.
		// [ TEST CODE ]
		MActionResult* pResult = new MActionResult;

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
			// Add the effect status, if any.
			//------------------------------------------------------
			EFFECTSTATUS es = (*g_pActionInfoTable)[skillID].GetEffectStatus();
			
			if (es!=EFFECTSTATUS_NULL)
			{

				pResult->Add( new MActionResultNodeAddEffectStatus( PlayerState::GetID(), 
																		es, 
																		delayFrame ) );
			}
		}

		pResult->Add( new MActionResultNodeActionInfo( 
								skillID, 
								OBJECTID_NULL,//pCreature->GetID(), 
								PlayerState::GetID(), 
								CreatureState::GetX(pPlayerCreature),
								CreatureState::GetY(pPlayerCreature),
								delayFrame ) );
		



		// [ TEST CODE ]
		//
		// Make the result and hand it over.
		//
		// Face the player.
		//pCreature->SetDirectionToPosition(g_pPlayer->GetX(), g_pPlayer->GetY());

		//Duration_t	m_Duration;
		//pCreature->PacketSpecialActionToOther(
		//					pPacket->getSkillType(), 
		//					pPacket->getObjectID(),
		//					pResult						// result
		//);
		int direction = SelectFacingDirection(
							pPacket->getX(), pPacket->getY(),
							CreatureState::GetX(pPlayerCreature), CreatureState::GetY(pPlayerCreature));

		WorldEvents::ExecuteActionInfoFromMainNode(
					static_cast<TYPE_ACTIONINFO>(pPacket->getSkillType() + (*g_pActionInfoTable).GetMinResultActionInfo()),										// the skill used
				
					pPacket->getX(), pPacket->getY(), 0, // the user (?) 
					direction,														// direction
					
					PlayerState::GetID(),												// the target
					CreatureState::GetX(pPlayerCreature), CreatureState::GetY(pPlayerCreature), 0,
					
					delayFrame,													// the skill's (remaining) duration		
					
					pResult,										// the result

					false);			// start the skill from its beginning
		
	}
		



	//------------------------------------------------------------------
	// Update what the UI shows.
	// (Setting it beats comparing first.)
	//------------------------------------------------------------------
	//UI_SetHP( g_pPlayer->GetHP(), g_pPlayer->GetMAX_HP() );
	//UI_SetMP( g_pPlayer->GetMP(), g_pPlayer->GetMAX_MP() );


	__END_CATCH
}
