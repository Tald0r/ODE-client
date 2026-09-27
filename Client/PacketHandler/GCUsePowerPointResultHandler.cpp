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

#include "VS_UI_GameCommon.h"
#include "UIFunction.h"
#include "MGameStringTable.h"
#include "VS_UI.h"

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
			g_char_slot_ingame.m_Powerjjang_Point = pPacket->getPowerPoint();
			gC_vs_ui.PowerjjangGambleResult(pPacket->getItemCode());
		}
		break;
	case GCUsePowerPointResult::NOT_ENOUGH_INVENTORY_SPACE:
		UI_PopupMessage( UI_STRING_MESSAGE_INVENTORY_FULL_MONSTER_KILL_QUEST );
		break;
	case GCUsePowerPointResult::NOT_ENOUGH_POWER_POINT:
		UI_PopupMessage( UI_STRING_POWER_JJANG_ERROR_NO_POINT );
		break;
	default:
		UI_PopupMessage( STRING_ERROR_ETC_ERROR );
		break;
	}

	__END_CATCH
}

