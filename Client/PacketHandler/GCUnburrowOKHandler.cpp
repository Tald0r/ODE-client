//--------------------------------------------------------------------------------
//
// Filename    : GCUnburrowOKHandler.cpp
// Written By  : elca, Reiot
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCUnburrowOK.h"
#include "DebugLog.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCUnburrowOKHandler::execute ( GCUnburrowOK * pPacket , Player * pPlayer )

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

	//-----------------------------------------------------------------
	// The position.
	//-----------------------------------------------------------------
	CreatureState::SetServerPosition(pPlayerCreature, pPacket->getX(), pPacket->getY() );	
	CreatureState::MovePosition(pPlayerCreature, pPacket->getX(), pPacket->getY() );
	CreatureState::SetDirection(pPlayerCreature, pPacket->getDir() );

	//-----------------------------------------------------------------
	// Come up above ground.
	//-----------------------------------------------------------------
	PlayerState::SetGroundCreature();

	//--------------------------------------------------
	// The effect of coming out of a burrow is the burrowing effect.
	//--------------------------------------------------		
	WorldEvents::ExecuteActionInfoFromMainNode(
		RESULT_MAGIC_HIDE,										// the skill used
	
		CreatureState::GetX(pPlayerCreature), CreatureState::GetY(pPlayerCreature), 0,
		CreatureState::GetDirection(pPlayerCreature),														// direction
		
		OBJECTID_NULL,												// the target
		CreatureState::GetX(pPlayerCreature), CreatureState::GetY(pPlayerCreature), 0, 
		
		0,													// the skill's (remaining) duration		
		
		NULL,
		
		false);			// start the skill from its beginning


	__END_CATCH
}
