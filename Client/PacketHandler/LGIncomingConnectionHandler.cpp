//----------------------------------------------------------------------
//
// Filename    : LGIncomingConnectionHandler.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Lpackets/LGIncomingConnection.h"
#include "Properties.h"

//----------------------------------------------------------------------
// 
// LGIncomingConnectionHander::execute()
// 
// 게임 서버가 로그인 서버로부터 LGIncomingConnection 패킷을 받게 되면,
// ConnectionInfo를 새로 추가하게 된다.
// 
//----------------------------------------------------------------------
void LGIncomingConnectionHandler::execute ( LGIncomingConnection * pPacket )

{
	__BEGIN_TRY
	(void)pPacket;

	__END_CATCH
}
