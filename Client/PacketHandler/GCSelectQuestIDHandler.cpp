
//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSelectQuestIDHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSelectQuestID.h"
#include "ClientDef.h"
#include "TempInformation.h"
#include "UIFunction.h"

//////////////////////////////////////////////////////////////////////
//
// 클라이언트에서 서버로부터 메시지를 받았을때 실행되는 메쏘드이다.
//
//////////////////////////////////////////////////////////////////////
void GCSelectQuestIDHandler::execute ( GCSelectQuestID * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
	
	
	DEBUG_ADD("[GCSelectQuestIDHandler] Execute");
	UI_RunQuestList( pPacket );
	
	g_pTempInformation->SetMode( TempInformation::MODE_SELECT_QUEST );	
	

	__END_CATCH
}
