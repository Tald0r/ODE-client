//////////////////////////////////////////////////////////////////////
//
// Filename    : LCSelectPCErrorHandler.cpp
// Written By  : Reiot
// Description : 
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Lpackets/LCSelectPCError.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "LoginPresentationHost.h"
#include "MGameStringTable.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void LCSelectPCErrorHandler::execute ( LCSelectPCError * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
	DEBUG_ADD("[Packet] LCSelectPCErrorHandler");


	//cout << "Access Denied... (" << pPacket->toString() << ")" << endl;
	//throw ProtocolException("SelectPC Failed!");

//	g_pUIDialog->PopupFreeMessageDlg( pPacket->getMessage().c_str() );
	switch(pPacket->getCode())
	{
		case SELECT_PC_CANNOT_PLAY:
			LoginPresentation::RunAskGoBilingPage(0);
//			g_pUIDialog->PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_SELECT_PC_CANNOT_PLAY].GetString());
			LoginPresentation::Sleep(1000);
			break;

		case SELECT_PC_NOT_BILLING_CHECK:
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_SELECT_PC_NOT_BILLING_CHECK].GetString());
			LoginPresentation::Sleep(1000);
			break;

		case SELECT_PC_CANNOT_PLAY_BY_ATTR:
//			g_pUIDialog->PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_SELECT_PC_CANNOT_PLAY_BY_ATTR].GetString());
			DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_LOGIN_ERROR_NONPK].GetString());
			//UI_RunAskGoBilingPage(1);
			LoginPresentation::Sleep(1000);
			break;
	}
	LoginPresentation::EnterSelectPCMode();

		
	__END_CATCH
}
