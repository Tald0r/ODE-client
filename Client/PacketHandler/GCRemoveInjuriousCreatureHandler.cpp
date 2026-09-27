//////////////////////////////////////////////////////////////////////////////
// Filename    : GCRemoveInjuriousCreatureHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Client_PCH.h"
#include "Gpackets/GCRemoveInjuriousCreature.h"
#include "MJusticeAttackManager.h"
#include "DebugInfo.h"

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void GCRemoveInjuriousCreatureHandler::execute (GCRemoveInjuriousCreature* pPacket , Player* pPlayer)

{
	__BEGIN_TRY
	(void)pPlayer;

	if (g_pJusticeAttackManager!=NULL)
	{
		if (!g_pJusticeAttackManager->RemoveCreature( pPacket->getName().c_str() ))
		{
			DEBUG_ADD_FORMAT("[Error] Can't remove InjuriousCreature : %s", pPacket->getName().c_str() );
		}
	}

	__END_CATCH
}
