//--------------------------------------------------------------------------------
//
// Filename    : GCUntransformOKHandler.cpp
// Written By  : elca, Reiot
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCUntransformOK.h"
#include "DebugLog.h"
#include "MActionResult.h"
#include "MCreatureTable.h"
#include "MItem.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "SkillDef.h"
#include "MVampireGear.h"
#include "UserInformation.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCUntransformOKHandler::execute ( GCUntransformOK * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	// The player; without one there is nothing to do.
	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		return;
	}
		

	bool bWerWolf = CreatureState::GetCreatureType(pPlayerCreature) == CREATURETYPE_WER_WOLF;


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

	int x = pPacket->getX();
	int y = pPacket->getY();
	int dir = pPacket->getDir();

	int creatureType = CreatureState::IsMale(pPlayerCreature)?CREATURETYPE_VAMPIRE_MALE1:CREATURETYPE_VAMPIRE_FEMALE1;
	MItem *pItem = g_pVampireGear->GetItem(MVampireGear::GEAR_VAMPIRE_COAT);
//	_MinTrace("Adasd2:%d %d\n",creatureType,g_pPlayer->IsMale());

	if(pItem != NULL)
	{
		if(CreatureState::IsMale(pPlayerCreature))
		{
			creatureType = pItem->GetAddonMaleFrameID();
		}
		else
		{
			creatureType = pItem->GetAddonFemaleFrameID();
		}
//		_MinTrace("Adasd:%d %d\n",creatureType,g_pPlayer->IsMale());
	}

	if (CreatureState::GetCompetence(pPlayerCreature)==0)
	{
		creatureType = CREATURETYPE_VAMPIRE_OPERATOR;
	}
	
	MActionResult* pResult = new MActionResult;

	pResult->Add( new MActionResultNodeChangeCreatureType( PlayerState::GetID(), creatureType ) );

	//--------------------------------------------------
	// Back to a vampire.
	//--------------------------------------------------								
	WorldEvents::ExecuteActionInfoFromMainNode(
		RESULT_MAGIC_UN_TRANSFORM,										// the skill used
	
		x, y, 0,
		dir,
		
		OBJECTID_NULL,												// the target
		x, y, 0, 
		
		0,													// the skill's (remaining) duration		
		
		pResult, //NULL,
		
		false);			// start the skill from its beginning

	PlayerState::SetDelay( 1000 );


	CreatureState::SetServerPosition(pPlayerCreature, x, y );
	CreatureState::MovePosition(pPlayerCreature, x, y );
	CreatureState::SetDirection(pPlayerCreature, dir );
	CreatureState::SetCurrentDirection(pPlayerCreature, dir );
	
	if( bWerWolf )
	{
		CreatureState::SetBodyColor1(pPlayerCreature, g_pUserInformation->SkinColor );
		if( pItem != NULL )
			CreatureState::SetBodyColor2(pPlayerCreature, pItem->GetItemOptionColorSet() );
		else
			CreatureState::SetBodyColor2(pPlayerCreature, 377 );
	}
	


	__END_CATCH
}
