//----------------------------------------------------------------------
//
// Filename    : LCVersionCheckErrorHandler.cpp
// Written By  : Reiot
// Description : 
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Lpackets/LCVersionCheckError.h"
#include "LoginPresentationHost.h"
#include "MGameStringTable.h"

//----------------------------------------------------------------------
// 
//----------------------------------------------------------------------
void LCVersionCheckErrorHandler::execute ( LCVersionCheckError * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPacket;
	(void)pPlayer;


	
	// client를 update해야한다.
	//g_bNeedUpdate = TRUE;
	// edit by Coffee
	//g_ModeNext = MODE_QUIT;
	LoginPresentation::QueueLoginWrongMode();
	//PopupErrorMessage((ErrorID)21); // 쇱꿎경굶댄轎
	LoginPresentation::PopupExitMessageDlg((*g_pGameStringTable)[UI_STRING_MESSAGE_CHECK_VERSION_ERROR].GetString());
	// end

		
	__END_CATCH
}
