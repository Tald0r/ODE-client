//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAttackMeleeOK2Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAttackMeleeOK2.h"
#include "AffectModifyInfo.h"
#include "DebugLog.h"
#include "MActionResult.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "MActionInfoTable.h"
#include "SkillDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCAttackMeleeOK2Handler::execute ( GCAttackMeleeOK2 * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	// The player; without one there is nothing to do.
	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		return;
	}
		

	// message

	//------------------------------------------------------
	// The zone is not made yet.
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
		
		return;
	}	

	//------------------------------------------------------
	// The creatures involved.
	//------------------------------------------------------
	MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );

	if (pCreature==NULL)
	{
		// No such creature.
		DEBUG_ADD_FORMAT("There's no such creature : ID=%d", pPacket->getObjectID());				
		
		//------------------------------------------------------------------
		// Hit at once.
		//------------------------------------------------------------------
		CreatureState::PacketSpecialActionResult(pPlayerCreature, RESULT_SKILL_ATTACK_MELEE,//pCreature->GetBasicActionInfo(),
								PlayerState::GetID(), 
								CreatureState::GetX(pPlayerCreature),
								CreatureState::GetY(pPlayerCreature)
					);
	}
	else
	{
		//------------------------------------------------------
		// The acting creature faces its target.
		//------------------------------------------------------
		CreatureState::SetDirectionToPosition(pCreature, CreatureState::GetX(pPlayerCreature), CreatureState::GetY(pPlayerCreature) );

		//------------------------------------------------------
		// The creature attacks the player.
		//------------------------------------------------------
		//g_pPlayer->PacketSpecialActionResult( SKILL_ATTACK_MELEE + g_ActionInfoTable.GetMinResultActionInfo() );
		MActionResult* pResult = new MActionResult;
		pResult->Add( new MActionResultNodeActionInfo( 
									CreatureState::GetBasicActionInfo(pCreature),
									pPacket->getObjectID(), 
									PlayerState::GetID(), 
									CreatureState::GetX(pPlayerCreature),
									CreatureState::GetY(pPlayerCreature)
						)	 
					);

		//------------------------------------------------------
		// The creature acts.
		//------------------------------------------------------
		CreatureState::PacketSpecialActionToOther(pCreature, CreatureState::GetBasicActionInfo(pCreature), 
						PlayerState::GetID(), 
						pResult
		);
	}

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


	__END_CATCH
}
