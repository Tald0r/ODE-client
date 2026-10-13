//////////////////////////////////////////////////////////////////////
//
// Filename    : GCHPRecoveryEndToOthersHandler.cpp
// Written By  : elca@ewestsoft.com
//
//////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
// include files
#include "Gpackets/GCHPRecoveryEndToOthers.h"
#include "MStatus.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCHPRecoveryEndToOthersHandler::execute ( GCHPRecoveryEndToOthers * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		


	if (ZoneState::Exists())
	{
		MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );

		if (pCreature!=NULL)
		{
			CreatureState::StopRecoveryHP(pCreature);

			CreatureState::Status(pCreature)->SetStatus( MODIFY_CURRENT_HP, pPacket->getCurrentHP() );
		}
	}



	__END_CATCH
}
