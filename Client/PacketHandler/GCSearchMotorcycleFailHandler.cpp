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
#include "MGameStringTable.h"
#include "DialogPresentationHost.h"

void GCSearchMotorcycleFailHandler::execute ( GCSearchMotorcycleFail * pPacket , Player * pPlayer )
	 

{
	__BEGIN_TRY
	(void)pPacket;
	(void)pPlayer;
	


	if (rand()%2)
	{
		DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_FIND_MOTOR_NO_WHERE].GetString());
	}
	else
	{
		DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_FIND_MOTOR_NO_KEY].GetString());
	}
	

	__END_CATCH
}
