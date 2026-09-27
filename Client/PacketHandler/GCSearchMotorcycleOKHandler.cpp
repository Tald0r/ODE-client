//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSearchMotorcycleOKHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSearchMotorcycleOK.h"
#include "ClientDef.h"
#include "UIDialog.h"
#include "MZoneTable.h"
#include "MGameStringTable.h"
#include "SafeFormat.h"

void GCSearchMotorcycleOKHandler::execute ( GCSearchMotorcycleOK * pPacket , Player * pPlayer )
	 

{
	__BEGIN_TRY
	(void)pPlayer;
	
	

	ZONETABLE_INFO* pZoneInfo = g_pZoneTable->Get( pPacket->getZoneID() );

	if (pZoneInfo==NULL)
	{
		g_pUIDialog->PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_FIND_MOTOR_NO_WHERE].GetString());
	}
	else
	{
		char str[128];

		SafeFormat::Format(str, GetGameString(STRING_MESSAGE_FIND_MOTOR_OK), 
						pZoneInfo->Name.GetString(),
						pPacket->getX(),
						pPacket->getY());

		g_pUIDialog->PopupFreeMessageDlg( str );
	}


	__END_CATCH
}
