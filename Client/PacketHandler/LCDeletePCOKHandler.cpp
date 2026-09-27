//--------------------------------------------------------------------------------
//
// Filename    : LCDeletePCOKHandler.cpp
// Written By  : Reiot
// Description : 
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Lpackets/LCDeletePCOK.h"

	#include "ClientPlayer.h"
	#include "Cpackets/CLGetPCList.h"

#include "ClientDef.h"
#include "UIFunction.h"

//--------------------------------------------------------------------------------
//
// The PC was deleted successfully. Upstream kept a second branch here
// for its Linux console test client (a cout banner, no UI); this is
// the game client's branch on every platform.
//
//--------------------------------------------------------------------------------
void LCDeletePCOKHandler::execute ( LCDeletePCOK * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPacket;


	ClientPlayer * pClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);

	// The delete succeeded.
	UI_DeleteCharacterOK();

	// Ask for the PC list again.
	CLGetPCList clGetPCList;
	pClientPlayer->sendPacket( &clGetPCList );

	pClientPlayer->setPlayerStatus( CPS_AFTER_SENDING_CL_GET_PC_LIST );

	// Wait for the PC list.
	g_ModeNext = MODE_WAIT_PCLIST;


	__END_CATCH
}
