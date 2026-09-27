//////////////////////////////////////////////////////////////////////
//
// Filename    : GCUseBonusPointErrorHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCUseBonusPointFail.h"
#include "ClientDef.h"
#include "UIDialog.h"
#include "TempInformation.h"
#include "MGameStringTable.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCUseBonusPointFailHandler::execute ( GCUseBonusPointFail* pPacket , Player * pPlayer )
{
	__BEGIN_TRY
	(void)pPacket;
	(void)pPlayer;
		


	//------------------------------------------------------------
	// mode 제거
	//------------------------------------------------------------
	g_pTempInformation->SetMode(TempInformation::MODE_NULL);

	g_pUIDialog->PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_CANNOT_USE_BONUS_POINT].GetString());


	__END_CATCH
}
