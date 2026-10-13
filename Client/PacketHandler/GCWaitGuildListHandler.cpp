//////////////////////////////////////////////////////////////////////
//
// Filename    : GCWaitGuildListHandler.cpp
// Written By  : 
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCWaitGuildList.h"
#include "DebugLog.h"
#include "GuildPresentationHost.h"
#include "PlayerStateHost.h"
//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
void GCWaitGuildListHandler::execute ( GCWaitGuildList * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
	
	//------------------------------------------------------
	// 검증
	//------------------------------------------------------
	if ( PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_NPC_ASK )
	{
		PlayerState::SetWaitVerifyNULL();

		DEBUG_ADD("[Verified] NPC Ask Answer OK");
	}

	GuildPresentation::ShowWaitGuildList(pPacket);

	__END_CATCH
}
