//////////////////////////////////////////////////////////////////////
//
// Filename    : GCGetOffMotorCycleFailedHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCGetOffMotorCycleFailed.h"
#include "PlayerStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCGetOffMotorCycleFailedHandler::execute ( GCGetOffMotorCycleFailed * pPacket, Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPacket;
	(void)pPlayer;
		


	//------------------------------------------
	// 어쨋든 간에.. 검증이 되었다고 본다.
	//------------------------------------------
	PlayerState::SetWaitVerifyNULL();


	__END_CATCH
}
