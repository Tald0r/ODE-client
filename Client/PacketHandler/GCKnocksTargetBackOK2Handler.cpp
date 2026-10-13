//////////////////////////////////////////////////////////////////////
//
// Filename    : GCKnocksTargetBackOK2Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCKnocksTargetBackOK2.h"
#include "AffectModifyInfo.h"
#include "DebugLog.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCKnocksTargetBackOK2Handler::execute ( GCKnocksTargetBackOK2 * pPacket , Player * pPlayer )

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

	//------------------------------------------------------
	// The zone is not made yet.
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
		
		return;
	}	


	int skillType = pPacket->getSkillType();	//pCreature->GetBasicActionInfo(),	// the basic attack, a gun? 	
	
	if( skillType >= g_pActionInfoTable->GetSize() )
	{
		DEBUG_ADD_FORMAT_WAR("[GCKnocksTargetBackOK2] SkillType Error %d",skillType );
		return;
	}
	
	//------------------------------------------------------
	// The direction it is knocked back in.
	//------------------------------------------------------
	unsigned short x = pPacket->getX();
	unsigned short y = pPacket->getY();
		

	//------------------------------------------------------
	// The creatures involved.
	//------------------------------------------------------
	MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );
	
	if (pCreature==NULL)
	{
		// No such creature.
		DEBUG_ADD_FORMAT("There's no such creature : ID=%d, Skill=%d", pPacket->getObjectID(), pPacket->getSkillType());
		
		// Apply it at once.
		CreatureState::SetServerPosition(pPlayerCreature, x, y );		
		CreatureState::MovePosition(pPlayerCreature, x, y );
		CreatureState::SetStop(pPlayerCreature);

		// Show the result at once.
		CreatureState::PacketSpecialActionResult(pPlayerCreature, static_cast<TYPE_ACTIONINFO>(pPacket->getSkillType() + (*g_pActionInfoTable).GetMinResultActionInfo()),
							OBJECTID_NULL,
							x,
							y
		);
	}
	else
	{	
		// The packet carries the cell after the move.
		//MCreature::GetPositionToDirection(x, y, pPacket->getDir());


		//------------------------------------------------------
		// The acting creature faces the player.
		//------------------------------------------------------
		CreatureState::SetDirectionToPosition(pCreature, CreatureState::GetX(pPlayerCreature), CreatureState::GetY(pPlayerCreature) );

		//------------------------------------------------------
		// The result
		//------------------------------------------------------
		// The creature attacks the player.
		//g_pPlayer->PacketSpecialActionResult( SKILL_ATTACK_MELEE + (*g_pActionInfoTable).GetMinResultActionInfo() );


		//------------------------------------------------------
		// The creature acts.
		//------------------------------------------------------
		CreatureState::PacketSpecialActionToOther(pCreature, skillType,
						PlayerState::GetID(), 
						NULL	//pResult
		);

		//------------------------------------------------------
		// To stay in sync,
		// Show the result at once.
		//------------------------------------------------------
		// Apply it at once.
		CreatureState::SetServerPosition(pPlayerCreature, x, y );		
		CreatureState::MovePosition(pPlayerCreature, x, y );
		CreatureState::SetStop(pPlayerCreature);

		// Show the result at once.
		CreatureState::PacketSpecialActionResult(pPlayerCreature, static_cast<TYPE_ACTIONINFO>(pPacket->getSkillType() + (*g_pActionInfoTable).GetMinResultActionInfo()),
							CreatureState::GetID(pCreature),
							CreatureState::GetX(pCreature),
							CreatureState::GetY(pCreature)
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
