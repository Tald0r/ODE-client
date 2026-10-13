//////////////////////////////////////////////////////////////////////
//
// Filename    : GCModifyGuildMemberInfoHandler.cc
// Written By  : elca
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCModifyGuildMemberInfo.h"
#include "UserInformation.h"
#include "GuildPresentationHost.h"
#include "PlayerStateHost.h"

//////////////////////////////////////////////////////////////////////
//
// 클라이언트에서 서버로부터 메시지를 받았을때 실행되는 메쏘드이다.
//
//////////////////////////////////////////////////////////////////////
void GCModifyGuildMemberInfoHandler::execute ( GCModifyGuildMemberInfo * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
	
	g_pUserInformation->GuildName = pPacket->getGuildName().c_str();
	g_pUserInformation->GuildGrade = pPacket->getGuildMemberRank();
	PlayerState::SetGuildNumber(pPacket->getGuildID());

	GuildPresentation::SetGuild(pPacket->getGuildID(), pPacket->getGuildMemberRank(), pPacket->getGuildName());

	__END_CATCH
}
