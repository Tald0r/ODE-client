//////////////////////////////////////////////////////////////////////
//
// Filename    : GCHPRecoveryEndToSelfHandler.cpp
// Written By  : elca@ewestsoft.com
//
//////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
// include files
#include "Gpackets/GCHPRecoveryEndToSelf.h"
#include "ClientDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCHPRecoveryEndToSelfHandler::execute ( GCHPRecoveryEndToSelf * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		



	if (g_pPlayer!=NULL)
	{
		g_pPlayer->StopRecoveryHP();

		g_pPlayer->SetStatus( MODIFY_CURRENT_HP, pPacket->getCurrentHP() );
	}	


	__END_CATCH
}
