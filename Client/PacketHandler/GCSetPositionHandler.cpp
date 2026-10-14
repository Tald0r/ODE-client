//----------------------------------------------------------------------
//
// Filename    : GCSetPositionHandler.cc
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSetPosition.h"
#include "DebugLog.h"
#include "LoginPresentationHost.h"

	#include "ClientPlayer.h"

//----------------------------------------------------------------------
// 
// GCSetPositionHander::execute
//
//----------------------------------------------------------------------
void GCSetPositionHandler::execute ( GCSetPosition * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
		

	ClientPlayer * pClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);

	DEBUG_ADD_FORMAT("Set Position To ( %d, %d ) to Dir(%d)", (int)pPacket->getX(), (int)pPacket->getY(), (int)pPacket->getDir());		
	
	pClientPlayer->setXY( pPacket->getX() , pPacket->getY() );

	pClientPlayer->setPlayerStatus( CPS_NORMAL ); 


	//--------------------------------------------------------
	// Place the player
	//--------------------------------------------------------
	LoginPresentation::InitPlayer(	pPacket->getX(), 
				pPacket->getY(),
				pPacket->getDir());

	//--------------------------------------------------------
	// Start the game..
	//--------------------------------------------------------
	LoginPresentation::EnterGameMode();

	
	__END_CATCH
}
