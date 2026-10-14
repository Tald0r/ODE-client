//----------------------------------------------------------------------
//
// Filename    : GCRequestPowerPointResultHandler.cpp
// Written By  :
// Description : 
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCRequestPowerPointResult.h"

#include "CommunicationPresentationHost.h"
#include "DialogPresentationHost.h"
#include "VsUiPresentationHost.h"
#include "MGameStringTable.h"
#include "SafeFormat.h"

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void GCRequestPowerPointResultHandler::execute ( GCRequestPowerPointResult * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;


	switch(pPacket->getErrorCode())
	{
	case GCRequestPowerPointResult::_NO_ERROR: 
		{
			VsUiPresentation::SetPowerjjangPoint( pPacket->getSumPowerPoint() );
			char TempBuffer[128];
			SafeFormat::Format(TempBuffer, GetGameString(UI_STRING_POWER_JJANG_REQUEST_OK), pPacket->getRequestPowerPoint());
			DialogPresentation::PopupFreeMessageDlg(TempBuffer);
		}
		break;
	case GCRequestPowerPointResult::SERVER_ERROR:
		CommunicationPresentation::PopupMessage( UI_STRING_POWER_JJANG_ERROR_SERVER_ERROR );
		break;
	case GCRequestPowerPointResult::PROCESS_ERROR:
		CommunicationPresentation::PopupMessage( UI_STRING_POWER_JJANG_ERROR_PROCESS_ERROR );
		break;
	case GCRequestPowerPointResult::NO_MEMBER:
		CommunicationPresentation::PopupMessage( UI_STRING_POWER_JJANG_ERROR_NO_MEMBER );
		break;
	case GCRequestPowerPointResult::NO_POINT:
		CommunicationPresentation::PopupMessage( UI_STRING_POWER_JJANG_ERROR_NO_POINT );
		break;
	case GCRequestPowerPointResult::NO_MATCHING:
		CommunicationPresentation::PopupMessage( UI_STRING_POWER_JJANG_ERROR_NO_MATCHING );
		break;
	case GCRequestPowerPointResult::CONNECT_ERROR:
		CommunicationPresentation::PopupMessage( UI_STRING_POWER_JJANG_ERROR_CONNECT );
		break;
	default:
		CommunicationPresentation::PopupMessage( STRING_ERROR_ETC_ERROR );
		break;
	}
		

	__END_CATCH
}

