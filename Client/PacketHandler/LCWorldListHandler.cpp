//----------------------------------------------------------------------
// LCWorldListHandler.cpp
// Written By: Reiot
//----------------------------------------------------------------------

#include "Client_PCH.h"
#include "Lpackets/LCWorldList.h"
#include "ApplyServerList.h"
#include "CServerInformation.h"
#include "ClientDef.h"
#include "UIFunction.h"

void LCWorldListHandler::execute(LCWorldList* pPacket, Player* pPlayer)
{
	__BEGIN_TRY
	(void)pPlayer;

	if (g_pServerInformation == NULL)
		g_pServerInformation = new CServerInformation;

	ApplyWorldList(*g_pServerInformation, *pPacket);

	UI_SetWorldList();
	SetMode(MODE_WAIT_SELECT_WORLD);

	__END_CATCH
}
