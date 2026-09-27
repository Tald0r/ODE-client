//////////////////////////////////////////////////////////////////////
//
// Filename    : GCMPRecoveryToOhtersHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCMPRecoveryStart.h"
#include "ClientDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCMPRecoveryStartHandler::execute ( GCMPRecoveryStart * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;


	// item 사용 검증
	//UseItemOK();

	if (g_pPlayer!=NULL)
	{
		DWORD delay = ConvertDurationToMillisecond( pPacket->getDelay() );
	
		g_pPlayer->SetRecoveryMP( pPacket->getQuantity(),
									pPacket->getPeriod(),
									delay );
	}	


	__END_CATCH
}
