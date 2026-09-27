//////////////////////////////////////////////////////////////////////
//
// Filename    : GCHPRecoveryEndToOthersHandler.cpp
// Written By  : elca@ewestsoft.com
//
//////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
// include files
#include "Gpackets/GCHPRecoveryEndToOthers.h"
#include "ClientDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCHPRecoveryEndToOthersHandler::execute ( GCHPRecoveryEndToOthers * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		


	if (g_pZone!=NULL)
	{
		MCreature* pCreature = g_pZone->GetCreature( pPacket->getObjectID() );

		if (pCreature!=NULL)
		{
			pCreature->StopRecoveryHP();

			pCreature->SetStatus( MODIFY_CURRENT_HP, pPacket->getCurrentHP() );
		}
	}



	__END_CATCH
}
