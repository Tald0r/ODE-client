//////////////////////////////////////////////////////////////////////
//
// Filename    : GCWaitGuildListHandler.cpp
// Written By  : 
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCWaitGuildList.h"
#include "ClientDef.h"
#include "UIFunction.h"
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
	if ( g_pPlayer->GetWaitVerify()==MPlayer::WAIT_VERIFY_NPC_ASK )
	{
		g_pPlayer->SetWaitVerifyNULL();

		DEBUG_ADD("[Verified] NPC Ask Answer OK");
	}

	UI_ShowWaitGuildList(pPacket);

	__END_CATCH
}
