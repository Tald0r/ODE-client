//--------------------------------------------------------------------------------
//
// Filename    : GCMorph1Handler.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCMorph1.h"
#include "DebugLog.h"
#include "MQuickSlot.h"
#include "PCOustersInfo2.h"
#include "PlayerStateHost.h"
#include "RaceType.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCMorph1Handler::execute ( GCMorph1 * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (!ZoneState::Exists()
		|| pPlayerCreature==NULL)
	{
		DEBUG_ADD_ERR("[Error] No zone or no player");

		return;
	}

	//-----------------------------------------------------------
	// A different ID.. the ID must change.
	//-----------------------------------------------------------
	int objectID;
	
	switch (pPacket->getPCInfo2()->getPCType())
	{
		//--------------------------------------------------
		// A slayer
		//--------------------------------------------------
		case PC_SLAYER :		
			objectID = ((PCSlayerInfo2*)pPacket->getPCInfo2())->getObjectID();
		break;

		//--------------------------------------------------
		// A vampire
		//--------------------------------------------------
		case PC_VAMPIRE :
			objectID = ((PCVampireInfo2*)pPacket->getPCInfo2())->getObjectID();			
		break;

		default :
			DEBUG_ADD_ERR("[Error] wrong PC Type");
		return;
	}

	//-----------------------------------------------------------
	// Play no sound, once the type is one the reply takes: a refused
	// one leaves the sound as it was (muted in the background, say).
	//-----------------------------------------------------------
	WorldEvents::MuteSound( true );

	if (PlayerState::GetID()!=static_cast<TYPE_OBJECTID>(objectID))
	{
		// Change the one in the zone too.
		ZoneState::RemoveCreature( PlayerState::GetID() );

		// the new ID
		CreatureState::SetID( pPlayerCreature, objectID );

		ZoneState::SetPlayer();
	}

	//--------------------------------------------------
	//
	//			 The player transforms
	//
	//--------------------------------------------------
	if (PlayerState::GetRace()==RACE_SLAYER)
	{
		PlayerState::ChangeToVampire();

		// for now
		PlayerState::SetGuildNumber( 2 );
	}
	else
	{
		PlayerState::ChangeToSlayer();

		// for now
		PlayerState::SetGuildNumber( -1 );
	}


	//--------------------------------------------------
	// set the special skill
	//--------------------------------------------------
	//g_pPlayer->SetSpecialActionInfo( MAGIC_LIGHT );

	//--------------------------------------------------
	// set the attack mode
	//--------------------------------------------------
//	g_pPlayer->SetAttackModeNormal();


	//--------------------------------------------------
	// Drop the quick slot. Needed? - -;
	//--------------------------------------------------	
	g_pQuickSlot = NULL;
	g_pArmsBand1 = NULL;
	g_pArmsBand2 = NULL;

			
	//--------------------------------------------------
	//		Reset the gear
	//--------------------------------------------------
	// probably not needed..
	//--------------------------------------------------
	DEBUG_ADD( "Remove All Addon" );
	
	//for (int i=0; i<ADDON_MAX; i++)
	//{
	//	g_pPlayer->RemoveAddon((enum ADDON)i);
	//}			

	PlayerState::SetBasicAttack( SKILL_ATTACK_MELEE, 1 );
		

	//--------------------------------------------------
	//
	//			 Set the player's details.
	//
	//--------------------------------------------------		
	switch (pPacket->getPCInfo2()->getPCType())
	{
		//--------------------------------------------------
		// A slayer
		//--------------------------------------------------
		case PC_SLAYER :
		{
			PlayerState::SetPCSlayerInfo( (PCSlayerInfo2*)pPacket->getPCInfo2() );
		}
		break;

		//--------------------------------------------------
		// A vampire
		//--------------------------------------------------
		case PC_VAMPIRE :
		{
			PlayerState::SetPCVampireInfo( (PCVampireInfo2*)pPacket->getPCInfo2() );
		}
		break;

		//--------------------------------------------------
		// An ouster
		//--------------------------------------------------
		case PC_OUSTERS :
		{
			PlayerState::SetPCOustersInfo( (PCOustersInfo2*)pPacket->getPCInfo2() );
		}
		break;
	}

	//--------------------------------------------------
	//	 Store the gear's details.
	//--------------------------------------------------
	PlayerState::SetGearInfo( pPacket->getGearInfo(), NULL );

	// Calculate again.
	PlayerState::CalculateStatus();

	//--------------------------------------------------
	//	Reset the inventory
	//--------------------------------------------------
	PlayerState::SetInventoryInfo( pPacket->getInventoryInfo() );

	//--------------------------------------------------
	// Other item details == the item held at the mouse
	//--------------------------------------------------
	PlayerState::SetExtraInfo( pPacket->getExtraInfo() );
	

	//--------------------------------------------------
	// Skill 
	//--------------------------------------------------


	//-----------------------------------------------------------
	// Let sound play again.
	//-----------------------------------------------------------
	WorldEvents::MuteSound( false );



	__END_CATCH
}
