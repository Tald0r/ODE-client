//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSearchMotorcycleFailHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSearchMotorcycleFail.h"
#include "ClientDef.h"
#include "MGameStringTable.h"
#include "UIDialog.h"

void GCSearchMotorcycleFailHandler::execute ( GCSearchMotorcycleFail * pPacket , Player * pPlayer )
	 

{
	__BEGIN_TRY
	(void)pPacket;
	(void)pPlayer;
	


	if (rand()%2)
	{
		g_pUIDialog->PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_FIND_MOTOR_NO_WHERE].GetString());
	}
	else
	{
		g_pUIDialog->PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_FIND_MOTOR_NO_KEY].GetString());
	}
	

	__END_CATCH
}
