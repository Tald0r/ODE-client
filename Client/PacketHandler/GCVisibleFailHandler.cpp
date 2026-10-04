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
#include "ClientDef.h"


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
	if (g_pPlayer->GetWaitVerify()==MPlayer::WAIT_VERIFY_SKILL_SUCCESS)
	{		
		g_pPlayer->SetWaitVerifyNULL();
	}
	else
	{
		DEBUG_ADD_ERR("[Error] Player is not WaitVerifySkillSuccess");
	}


	__END_CATCH
}
