//--------------------------------------------------------------------------------
//
// Filename    : GCMoveOKHandler.cpp
// Written By  : elca, Reiot
// Description :
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCMoveOK.h"
#include "PlayerStateHost.h"


	#include "ClientPlayer.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCMoveOKHandler::execute ( GCMoveOK * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
		
	
	ClientPlayer * pClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);

	pClientPlayer->setX( pPacket->getX() );
	pClientPlayer->setY( pPacket->getY() );
	pClientPlayer->setDir( pPacket->getDir() );

	//cout << "Move to (" << (int)pPacket->getX() << "," << (int)pPacket->getY() << ")" << endl;
	

	//--------------------------------------------------
	// 검증된 Tile에 대한 이동
	//--------------------------------------------------
	PlayerState::PacketMoveOK(pPacket->getX(), pPacket->getY(), pPacket->getDir());


	__END_CATCH
}
