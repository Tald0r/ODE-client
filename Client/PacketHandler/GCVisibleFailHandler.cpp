//////////////////////////////////////////////////////////////////////
//
// Filename    : GCVisibleErrorHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCVisibleFail.h"
#include "DebugLog.h"
#include "PlayerStateHost.h"


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCVisibleFailHandler::execute ( GCVisibleFail* pPacket , Player * pPlayer )
{
	__BEGIN_TRY
	(void)pPacket;
	(void)pPlayer;
		


	//------------------------------------------------------------------
	// Player가 기다리던 skill의 성공유무를 검증받았다.
	//------------------------------------------------------------------
	if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_SKILL_SUCCESS)
	{		
		PlayerState::SetWaitVerifyNULL();
	}
	else
	{
		DEBUG_ADD_ERR("[Error] Player is not WaitVerifySkillSuccess");
	}


	__END_CATCH
}
