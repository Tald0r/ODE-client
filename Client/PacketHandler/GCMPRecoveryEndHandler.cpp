//////////////////////////////////////////////////////////////////////
//
// Filename    : GCMPRecoveryEndHandler.cpp
// Written By  : elca@ewestsoft.com
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCMPRecoveryEnd.h"
#include "MStatus.h"
#include "PlayerStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCMPRecoveryEndHandler::execute ( GCMPRecoveryEnd * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		



	PlayerState::StopRecoveryMP();

	PlayerState::Status()->SetStatus( MODIFY_CURRENT_MP, pPacket->getCurrentMP() );


	__END_CATCH
}
