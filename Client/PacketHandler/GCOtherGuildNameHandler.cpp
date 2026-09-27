//////////////////////////////////////////////////////////////////////////////
// Filename    : GCOtherGuildName.cc
// Written By  : elca@ewestsoft.com
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Client_PCH.h"
#include "Gpackets/GCOtherGuildName.h"
#include "ClientDef.h"
#include "UIFunction.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCOtherGuildNameHandler::execute(GCOtherGuildName* pPacket, Player* pPlayer)

{
	__BEGIN_TRY
	(void)pPlayer;
	// Other Info 검색후에-_- 파티 매니저도 검색해서 길드이름을 넣어준다.

	std::string guildName = pPacket->getGuildName();
	UI_SetGuildName(pPacket->getGuildID(), guildName);

	__END_CATCH
}
