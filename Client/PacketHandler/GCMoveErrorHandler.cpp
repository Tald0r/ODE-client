//////////////////////////////////////////////////////////////////////
//
// Filename    : GCMoveErrorHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCMoveError.h"
#include "ClientDef.h"

	#include "ClientPlayer.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCMoveErrorHandler::execute ( GCMoveError * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		
/*
	ClientPlayer * pClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);

	pClientPlayer->setX( pPacket->getX() );
	pClientPlayer->setY( pPacket->getY() );

	cout << "Move Back To (" << (int)pPacket->getX() << "," << (int)pPacket->getY() << ")" << endl;
	
#elif __WINDOWS__
*/

	// 서버 좌표 설정
	// 2001.10.8 제거
	g_pPlayer->SetServerPosition( pPacket->getX(), pPacket->getY() );

	// MoveOK message가 하나 안 날아온다는걸 알 수 있다.
	g_pPlayer->PacketMoveNO();
	


	__END_CATCH
}
