//////////////////////////////////////////////////////////////////////
//
// Filename    : GCHPRecoveryEndToOthersHandler.cpp
// Written By  : elca@ewestsoft.com
//
//////////////////////////////////////////////////////////////////////
#include "Client_PCH.h"
// include files
#include "Gpackets/GCHPRecoveryEndToOthers.h"
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

			CreatureState::SetStatus(pCreature, MODIFY_CURRENT_HP, pPacket->getCurrentHP() );
		}
	}



	__END_CATCH
}
