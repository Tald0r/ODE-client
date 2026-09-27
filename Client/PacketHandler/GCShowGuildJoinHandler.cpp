//////////////////////////////////////////////////////////////////////
//
// Filename    : GCShowGuildJoinHandler.cpp
// Written By  : 
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCShowGuildJoin.h"
#include "ClientDef.h"
#include "UIFunction.h"

//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
void GCShowGuildJoinHandler::execute ( GCShowGuildJoin * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
	
		UI_ShowGuildJoin(pPacket->getJoinFee(), pPacket->getGuildMemberRank(), NULL, pPacket->getGuildName().c_str(), pPacket->getGuildID());

	__END_CATCH
}
