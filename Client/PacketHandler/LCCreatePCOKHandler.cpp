//----------------------------------------------------------------------
//
// Filename    : LCCreatePCOKHandler.cpp
// Written By  : Reiot
// Description : 
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Lpackets/LCCreatePCOK.h"
#include "LoginPresentationHost.h"

	#include "ClientPlayer.h"
	#include "Cpackets/CLGetPCList.h"

//----------------------------------------------------------------------
// 
//----------------------------------------------------------------------
void LCCreatePCOKHandler::execute ( LCCreatePCOK * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPacket;


	ClientPlayer * pClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);

	LoginPresentation::NewCharacterCreateOk();

	// 다시 PC LIST 를 받아와야 한다.
	CLGetPCList clGetPCList;
	pClientPlayer->sendPacket( &clGetPCList );

	pClientPlayer->setPlayerStatus( CPS_AFTER_SENDING_CL_GET_PC_LIST );
	

	__END_CATCH
}
