//////////////////////////////////////////////////////////////////////
//
// Filename    : GCHPRecoveryEndToSelfHandler.cpp
// Written By  : elca@ewestsoft.com
//
//////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
// include files
#include "Gpackets/GCHPRecoveryEndToSelf.h"
#include "MStatus.h"
#include "PlayerStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCHPRecoveryEndToSelfHandler::execute ( GCHPRecoveryEndToSelf * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		



	if (PlayerState::Status()!=NULL)
	{
		PlayerState::StopRecoveryHP();

		PlayerState::Status()->SetStatus( MODIFY_CURRENT_HP, pPacket->getCurrentHP() );
	}	


	__END_CATCH
}
