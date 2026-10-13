//////////////////////////////////////////////////////////////////////
//
// Filename    : GCHPRecoveryStartToOthersHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCHPRecoveryStartToOthers.h"
#include "ConvertDuration.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCHPRecoveryStartToOthersHandler::execute ( GCHPRecoveryStartToOthers * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;


	if (ZoneState::Exists())
	{
		MCreature* pCreature = ZoneState::GetCreature( pPacket->getObjectID() );

		if (pCreature!=NULL)
		{
			DWORD delay = ConvertDurationToMillisecond( pPacket->getDelay() );

			CreatureState::SetRecoveryHP(pCreature, pPacket->getQuantity(),
										pPacket->getPeriod(),
										delay );
		}
	}

	__END_CATCH
}
