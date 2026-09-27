//--------------------------------------------------------------------------------
//
// Filename    : GCLightningHandler.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCLightning.h"

	#include "ClientPlayer.h"

#include "ClientDef.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCLightningHandler::execute ( GCLightning * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		


	// server : 10 = 1초
	// client : 1000 = 1초
	// 그래서.. *100.. 음하하..
	SetLightning( pPacket->getDelay()*100 );


	__END_CATCH
}
