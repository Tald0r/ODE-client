//----------------------------------------------------------------------
// LCWorldListHandler.cpp
// Written By: Reiot
//----------------------------------------------------------------------

#include "Client_PCH.h"
#include "Lpackets/LCWorldList.h"
#include "ApplyServerList.h"
#include "CServerInformation.h"
#include "LoginListHost.h"

namespace LoginLists {
namespace {
const Host* s_Host = nullptr;
}

const Host* SetHost(const Host* host)
{
	const auto* previous = s_Host;
	s_Host = host;
	return previous;
}

void WorldListApplied()
{
	if (s_Host && s_Host->PublishWorlds) s_Host->PublishWorlds();
	if (s_Host && s_Host->SelectWorldMode) s_Host->SelectWorldMode();
}

void ServerListApplied()
{
	if (s_Host && s_Host->PublishServers) s_Host->PublishServers();
	if (s_Host && s_Host->SelectServerMode) s_Host->SelectServerMode();
}
}

void LCWorldListHandler::execute(LCWorldList* pPacket, Player* pPlayer)
{
	__BEGIN_TRY
	(void)pPlayer;

	if (g_pServerInformation == NULL)
		g_pServerInformation = new CServerInformation;

	ApplyWorldList(*g_pServerInformation, *pPacket);

	LoginLists::WorldListApplied();

	__END_CATCH
}
