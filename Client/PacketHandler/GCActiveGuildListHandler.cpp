//////////////////////////////////////////////////////////////////////
//
// Filename    : GCActiveGuildListHandler.cpp
// Written By  : 
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCActiveGuildList.h"
#include "DebugLog.h"
#include "GuildPresentationHost.h"
#include "PlayerStateHost.h"

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
	if ( PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_NPC_ASK )
	{
		PlayerState::SetWaitVerifyNULL();

		DEBUG_ADD("[Verified] NPC Ask Answer OK");
	}


	GuildPresentation::ShowActiveGuildList(pPacket);

	__END_CATCH
}
