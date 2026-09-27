//////////////////////////////////////////////////////////////////////
//
// Filename    : GCActiveGuildListHandler.cpp
// Written By  : 
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCActiveGuildList.h"
#include "ClientDef.h"
#include "UIFunction.h"

//////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////
void GCActiveGuildListHandler::execute ( GCActiveGuildList * pPacket , Player * pPlayer )
{
	__BEGIN_TRY
	(void)pPlayer;
	
	//cout << pPacket->toString() << endl; 
	//------------------------------------------------------
	// 검증
	//------------------------------------------------------
	if ( g_pPlayer->GetWaitVerify()==MPlayer::WAIT_VERIFY_NPC_ASK )
	{
		g_pPlayer->SetWaitVerifyNULL();

		DEBUG_ADD("[Verified] NPC Ask Answer OK");
	}


	UI_ShowActiveGuildList(pPacket);

	__END_CATCH
}
