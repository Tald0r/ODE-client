//////////////////////////////////////////////////////////////////////
//
// Filename    : GCGetOffMotorCycleOKHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCGetOffMotorCycleOK.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "SoundDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCGetOffMotorCycleOKHandler::execute ( GCGetOffMotorCycleOK * pPacket, Player * pPlayer )

{
	__BEGIN_TRY

	(void)pPacket;
	(void)pPlayer;

	// The player; without one there is nothing to do.
	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		return;
	}

	//------------------------------------------
	// The motorcycle is no longer drawn.
	//------------------------------------------
	CreatureState::RemoveAddon(pPlayerCreature, ADDON_MOTOR );

	//------------------------------------------
	// The player walks now.
	//------------------------------------------
	CreatureState::SetMoveDevice(pPlayerCreature, CreatureState::MOVE_DEVICE_WALK );

	//------------------------------------------
	// Either way, take it as verified.
	//------------------------------------------
	PlayerState::SetWaitVerifyNULL();

	//------------------------------------------
	// The sound of getting off.
	//------------------------------------------
	WorldEvents::PlaySound( SOUND_WORLD_BIKE_STOP );


	__END_CATCH
}
