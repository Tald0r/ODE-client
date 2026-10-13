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
#include "ConvertDuration.h"
#include "PlayerStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCMPRecoveryStartHandler::execute ( GCMPRecoveryStart * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;


	// item 사용 검증
	//UseItemOK();

	if (PlayerState::Status()!=NULL)
	{
		DWORD delay = ConvertDurationToMillisecond( pPacket->getDelay() );
	
		PlayerState::SetRecoveryMP( pPacket->getQuantity(),
									pPacket->getPeriod(),
									delay );
	}	


	__END_CATCH
}
