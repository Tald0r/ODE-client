//----------------------------------------------------------------------
// LCServerListHandler.cpp
// Written By: Reiot
//----------------------------------------------------------------------

#include "Client_PCH.h"
#include "Lpackets/LCServerList.h"
#include "ApplyServerList.h"
#include "CServerInformation.h"
#include "LoginListHost.h"
#include "DebugLog.h"

void LCServerListHandler::execute(LCServerList* pPacket, Player* pPlayer)
{
	__BEGIN_TRY
	(void)pPlayer;

	if (g_pServerInformation == NULL)
	{
		DEBUG_ADD_ERR("[Error] g_pServerInformation is NULL");
		return;
	}

	if (ApplyServerList(*g_pServerInformation, *pPacket))
	{
		LoginLists::ServerListApplied();
	}

	__END_CATCH
}
