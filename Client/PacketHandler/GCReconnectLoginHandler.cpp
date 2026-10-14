//--------------------------------------------------------------------------------
//
// Filename    : GCReconnectLoginHandler.cpp
// Written By  : Reiot
// Description : 
//
//--------------------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "PacketDispatcher.h"
#include "Gpackets/GCReconnectLogin.h"
#include "DebugLog.h"
#include "LoginPresentationHost.h"

	#include "ClientPlayer.h"
	#include "Cpackets/CLReconnectLogin.h"
	#include "UserInformation.h"
	//add by viva
	#include "Cpackets/CGConnectSetKey.h"
	//end
//--------------------------------------------------------------------------------
// The game server sends the player back to the login server: reconnect to
// it and send the CLReconnectLogin packet carrying the key. (Upstream's
// comment, copied from LCReconnect's, spoke of the game server.)
//--------------------------------------------------------------------------------
void GCReconnectLoginHandler::execute ( GCReconnectLogin * pPacket , Player * pPlayer )

{
	__BEGIN_TRY


 
	ClientPlayer * pClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);

	// End the connection to the login server
	// Note that the login server ends the connection as it sends LCReconnect.
	
	pClientPlayer->disconnect();

	// Connect to the login server with what the GCReconnectLogin packet holds.
	DEBUG_ADD_FORMAT("Reconnecting to %s:%d", 
									pPacket->getLoginServerIP().c_str(), 
									pPacket->getLoginServerPort());
	
	try {

		pClientPlayer->getSocket()->reconnect( pPacket->getLoginServerIP() , pPacket->getLoginServerPort() );

		// Reconnecting makes a new socket,
		// so its options must be set again.
		pClientPlayer->getSocket()->setNonBlocking();
		pClientPlayer->getSocket()->setLinger(0);

	} catch ( ConnectException & ce ) {
		throw Error(ce.toString());
	}

	// Once connected, send the CLReconnectLogin packet at once.
	// Keep the type and name of the PC selected before in the client player.
	DEBUG_ADD("Sending CLReconnectLogin");
		
	//add by viva	
	CGConnectSetKey cgConnectSetKey;
	cgConnectSetKey.setEncryptKey(rand());
	cgConnectSetKey.setHashKey(rand());
	pClientPlayer->sendPacket(&cgConnectSetKey);
	LoginPresentation::FlushSocketOutput();
	LoginPresentation::Sleep(500);
	// Registered as an explicit no-op (see PacketHandlerRegistry.cpp) - the
	// linked handler was always the empty stub.
	PacketDispatcher::dispatch(&cgConnectSetKey, pClientPlayer);
	//end

	// reconnecting..
	CLReconnectLogin _CLReconnectLogin;
	_CLReconnectLogin.setKey( pPacket->getKey() );
	_CLReconnectLogin.SetLoginMode(g_pUserInformation->IsAutoLogIn);
	
	pClientPlayer->sendPacket( &_CLReconnectLogin );
	pClientPlayer->setPlayerStatus( CPS_AFTER_SENDING_CL_GET_PC_LIST );	

	// wait for the pc list.
	LoginPresentation::EnterWaitPCListMode();

	__END_CATCH
}
