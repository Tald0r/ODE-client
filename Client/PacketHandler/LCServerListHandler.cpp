//----------------------------------------------------------------------
// LCServerListHandler.cpp
// Written By: Reiot
//----------------------------------------------------------------------

#include "Client_PCH.h"
#include "Lpackets/LCServerList.h"
#include "ApplyServerList.h"
#include "CServerInformation.h"
#include "ClientDef.h"
#include "UIFunction.h"

void LCServerListHandler::execute(LCServerList* pPacket, Player* pPlayer)
{
	__BEGIN_TRY
	(void)pPlayer;

	if (g_pServerInformation == NULL)
	{
		DEBUG_ADD("[Error] g_pServerInformation is NULL");
		return;
	}

	if (ApplyServerList(*g_pServerInformation, *pPacket))
	{
		UI_SetServerList();
		SetMode(MODE_WAIT_SELECT_SERVER);
	}

	__END_CATCH
}
