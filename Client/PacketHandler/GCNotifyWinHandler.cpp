
//////////////////////////////////////////////////////////////////////
//
// Filename    : GCNotifyWinHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCNotifyWin.h"
#include "ClientDef.h"
#include "UIFunction.h"

//////////////////////////////////////////////////////////////////////
//
// 클라이언트에서 서버로부터 메시지를 받았을때 실행되는 메쏘드이다.
//
//////////////////////////////////////////////////////////////////////
void GCNotifyWinHandler::execute ( GCNotifyWin * pPacket , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;
	
	//cout << pPacket->toString() << endl;
	UI_RunImageNotice( pPacket->getName().c_str(), pPacket->getGiftID() );

	__END_CATCH
}
