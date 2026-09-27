//--------------------------------------------------------------------------------
//
// Filename    : CURequestLoginModeHandler.cpp
// Written By  : Reiot
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Upackets/CURequestLoginMode.h"


//--------------------------------------------------------------------------------
//
// 로그인 모드를 확인해서 알려준다
//
//--------------------------------------------------------------------------------
void CURequestLoginModeHandler::execute ( CURequestLoginMode * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPacket;
	(void)pPlayer;
		
	__END_CATCH
}

