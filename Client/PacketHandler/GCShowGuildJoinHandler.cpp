//////////////////////////////////////////////////////////////////////
//
// Filename    : GCShowGuildJoinHandler.cpp
// Written By  : 
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCShowGuildJoin.h"
#include "GuildPresentationHost.h"

//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
void GCShowGuildJoinHandler::execute ( GCShowGuildJoin * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
	
		GuildPresentation::ShowGuildJoin(pPacket->getJoinFee(), pPacket->getGuildMemberRank(), NULL, pPacket->getGuildName().c_str(), pPacket->getGuildID());

	__END_CATCH
}
