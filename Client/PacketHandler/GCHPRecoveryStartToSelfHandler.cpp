//////////////////////////////////////////////////////////////////////
//
// Filename    : GCHPRecoveryToOhtersHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCHPRecoveryStartToSelf.h"
#include "ConvertDuration.h"
#include "PlayerStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCHPRecoveryStartToSelfHandler::execute ( GCHPRecoveryStartToSelf * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		

	// item 사용 검증
	//UseItemOK();

	if (PlayerState::Status()!=NULL)
	{
		DWORD delay = ConvertDurationToMillisecond( pPacket->getDelay() );
	
		PlayerState::SetRecoveryHP( pPacket->getQuantity(),
									pPacket->getPeriod(),
									delay );
	}	


	__END_CATCH
}
