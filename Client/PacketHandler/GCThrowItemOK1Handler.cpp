//////////////////////////////////////////////////////////////////////
//
// Filename    : GCThrowItemOK1Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCThrowItemOK1.h"
#include "ConvertDuration.h"
#include "DebugLog.h"
#include "MActionResult.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCThrowItemOK1Handler::execute ( GCThrowItemOK1 * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	// The player; without one there is nothing to do.
	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		return;
	}
		


	TYPE_ACTIONINFO skillType = MAGIC_THROW_HOLY_WATER;	// not the action the player waited on

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

		// [ TEST CODE ]
		// An effect in progress takes it as a result.
		// Make the result.
		// g_pPlayer->>AddActionResult( ... )

		// The creature is hit.
		if (pCreature != NULL)
		{			
			MActionResult* pResult = new MActionResult;

			// The creature hit faces the player.
			//pCreature->SetDirectionToPosition(g_pPlayer->GetX(), g_pPlayer->GetY());

			//int delayFrame = ConvertDurationToFrame( pPacket->getDuration() );

			// Show the result.
			//pCreature->PacketSpecialActionResult( pPacket->getSkillType() + g_ActionInfoTable.GetMinResultActionInfo());
			pResult->Add( new MActionResultNodeActionInfo( 
											skillType,
											PlayerState::GetID(), 
											CreatureState::GetID(pCreature), 
											CreatureState::GetX(pCreature),
											CreatureState::GetY(pCreature))
											//delayFrame ) 
							);	

			
			// [ TEST CODE ] 
			// Show the damage of the hit.			

			// Add it to the player's current skill as its result.
			//g_pPlayer->PacketAddActionResult( pPacket->getCEffectID(), pResult );		
			PlayerState::PacketAddActionResult(0, pResult );		
		}
		
	}
		


	__END_CATCH
}
