//----------------------------------------------------------------------
//
// Filename    : GCUsePowerPointResultHandler.cpp
// Written By  :
// Description : 
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCUsePowerPointResult.h"

#include "CommunicationPresentationHost.h"
#include "VsUiPresentationHost.h"
#include "MGameStringTable.h"

//----------------------------------------------------------------------
//----------------------------------------------------------------------
void GCUsePowerPointResultHandler::execute ( GCUsePowerPointResult * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;


	switch(pPacket->getErrorCode())
	{
	case GCUsePowerPointResult::_NO_ERROR:
		{
			VsUiPresentation::SetPowerjjangPoint( static_cast<int>(pPacket->getPowerPoint()) );
			VsUiPresentation::PowerjjangGambleResult( pPacket->getItemCode() );
		}
		break;
	case GCUsePowerPointResult::NOT_ENOUGH_INVENTORY_SPACE:
		CommunicationPresentation::PopupMessage( UI_STRING_MESSAGE_INVENTORY_FULL_MONSTER_KILL_QUEST );
		break;
	case GCUsePowerPointResult::NOT_ENOUGH_POWER_POINT:
		CommunicationPresentation::PopupMessage( UI_STRING_POWER_JJANG_ERROR_NO_POINT );
		break;
	default:
		CommunicationPresentation::PopupMessage( STRING_ERROR_ETC_ERROR );
		break;
	}

	__END_CATCH
}

