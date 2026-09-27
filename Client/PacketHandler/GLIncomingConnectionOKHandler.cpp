//--------------------------------------------------------------------------------
//
// Filename    : GLIncomingConnectionOKHandler.cpp
// Written By  : Reiot
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GLIncomingConnectionOK.h"

//--------------------------------------------------------------------------------
// 
// GLIncomingConnectionOKHander::execute()
// 
// 게임 서버로부터 GLIncomingConnectionOK 패킷이 날아오면, 로그인 서버는 이 허가가 
// 어느 플레이어에 대한 허가인지 찾아내야 한다. 그 후, 이 플레이어에게 LCReconnect
// 패킷을 던져줘야 한다.
// 
//--------------------------------------------------------------------------------
void GLIncomingConnectionOKHandler::execute ( GLIncomingConnectionOK * pPacket )

{
	__BEGIN_TRY
	(void)pPacket;

	__END_CATCH
}
