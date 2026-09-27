//----------------------------------------------------------------------
//
// Filename    : LCVersionCheckOKHandler.cpp
// Written By  : Reiot
// Description : 
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Lpackets/LCVersionCheckOK.h"
#include "ClientDef.h"


//----------------------------------------------------------------------
// 
//----------------------------------------------------------------------
void LCVersionCheckOKHandler::execute ( LCVersionCheckOK * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPacket;
	(void)pPlayer;



	// 정상적인 경우.. 아무것도 할게 없다.

		
	__END_CATCH
}
