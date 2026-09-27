
//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAuthKeyHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAuthKey.h"
	#include "Client.h"
//////////////////////////////////////////////////////////////////////
//
// 클라이언트에서 서버로부터 메시지를 받았을때 실행되는 메쏘드이다.
//
//////////////////////////////////////////////////////////////////////

void GCAuthKeyHandler::execute ( GCAuthKey * pPacket , Player * pPlayer )


{
	__BEGIN_TRY
	(void)pPacket;
	(void)pPlayer;
//		__BEGIN_DEBUG_EX


#if !defined(OUTPUT_DEBUG) && !defined(_DEBUG) && !defined(__DEBUG_OUTPUT__) && defined(__NPROTECT__) //add by zdj
	Set_Auth(pPacket->getKey());
//	MessageBox(g_hWnd, "GCAuthKeyHandler받음1", "Warning", MB_OK);
#endif
//	MessageBox(g_hWnd, "GCAuthKeyHandler받음2", "Warning", MB_OK);
// REMOVED: nProtect anti-cheat code (SDL migration - no longer needed)

//	__END_DEBUG_EX
	__END_CATCH
}
