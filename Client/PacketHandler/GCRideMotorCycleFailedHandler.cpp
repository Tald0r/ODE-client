//////////////////////////////////////////////////////////////////////
//
// Filename    : GCRideMotorCycleFailedHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCRideMotorCycleFailed.h"
#include "ClientDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCRideMotorCycleFailedHandler::execute ( GCRideMotorCycleFailed * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPacket;
	(void)pPlayer;
		

	
	//------------------------------------------
	// 어쨋든 간에.. 검증이 되었다고 본다.
	//------------------------------------------
	g_pPlayer->SetWaitVerifyNULL();


	__END_CATCH
}
