//--------------------------------------------------------------------------------
//
// Filename    : LCReconnectHandler.cpp
// Written By  : Reiot
// Description : 
//
//--------------------------------------------------------------------------------

// include files

#include "Client_PCH.h"
#include "WebSocketTransport.h"
#include "PacketDispatcher.h"
#include "Lpackets/LCReconnect.h"
#include "DebugLog.h"
#include "LoginPresentationHost.h"
#include "ServerInfoFileParser.h"

	#include "ClientPlayer.h"
	#include "Cpackets/CGConnect.h"
	#include "Cpackets/CGPortCheck.h"
	#include "UserInformation.h"
	#include "Properties.h"
	#include "ClientCommunicationManager.h"
	//add by viva
	#include "Cpackets/CGConnectSetKey.h"
	//end


// ACProtect include removed (SDL2) - Copy protection no longer needed

//--------------------------------------------------------------------------------
// As soon as the login server sends the game server's address, port and key,
// connect to the game server and send the CGConnect packet carrying the key.
//--------------------------------------------------------------------------------
void LCReconnectHandler::execute ( LCReconnect * pPacket , Player * pPlayer )

{
	__BEGIN_TRY


	ClientPlayer * pClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);

	// End the connection to the login server
	// Note that the login server ends the connection as it sends LCReconnect.
	
	pClientPlayer->disconnect();

	// Connect to the game server with what the LCReconnect packet holds.
	DEBUG_ADD_FORMAT("Reconnecting to %s:%d", 
										pPacket->getGameServerIP().c_str(), 
										pPacket->getGameServerPort());
	
	try {
		pClientPlayer->getSocket()->reconnect( pPacket->getGameServerIP() , pPacket->getGameServerPort() );
		// Reconnecting makes a new socket,
		// so its options must be set again.
		pClientPlayer->getSocket()->setNonBlocking();
		pClientPlayer->getSocket()->setLinger(0);

	} catch ( ConnectException & ce ) {
		throw Error(ce.toString());
	}

	// Once connected, send the CGConnect packet at once.
	// Keep the type and name of the PC selected before in the client player.
	DEBUG_ADD("Sending CGConnect");

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
	CGConnect cgConnect;
	cgConnect.setKey( pPacket->getKey() );
	cgConnect.setPCType( pClientPlayer->getPCType() );
	cgConnect.setPCName( pClientPlayer->getPCName() );
	// No host, no address: six zero bytes.
	static const std::uint8_t noMacAddress[6] = {};
	const std::uint8_t* pMacAddress = LoginPresentation::MacAddress();
	cgConnect.setMacAddress( pMacAddress != NULL ? pMacAddress : noMacAddress );
	
	pClientPlayer->sendPacket( &cgConnect );
	pClientPlayer->setPlayerStatus( CPS_AFTER_SENDING_CG_CONNECT );	

	// send it at once.
//	EMBEDDED_BEGIN;
	LoginPresentation::FlushSocketOutput();
//	EMBEDDED_END;
	
	
	// 2002.6.28 [UDP change]
	// to tell the server the UDP port
	if (!NetworkTransport::UsesWebSocket()) {
		CGPortCheck cgPortCheck;
		cgPortCheck.setPCName( g_pUserInformation->CharacterID.GetString() );

		std::string ServerAddress;
		uint ServerPort;

		if( g_pUserInformation->bKorean )
		{
			ServerAddress = g_pConfigKorean->getProperty("LoginServerAddress");
			ServerPort = g_pConfigKorean->getPropertyInt("LoginServerCheckPort");
			//add by sonic 2006.4.10 (check whether the address is local)
		}
		else
		{
			ServerAddress = g_pConfigForeign->getProperty( LoginPresentation::Dimension(), "LoginServerAddress" );
			ServerPort = g_pConfigForeign->getPropertyInt(LoginPresentation::Dimension(), "LoginServerCheckPort");
		}
	
			
		DEBUG_ADD("[ ClientPacket] Send CGPortCheck ");
		g_pClientCommunicationManager->sendPacket( ServerAddress,
			ServerPort,
			&cgPortCheck );

	}

	DEBUG_ADD("[ MODE ] START SETMODE MODE_WAIT_UPDATEINFO");
	LoginPresentation::EnterWaitUpdateInfoMode();
	DEBUG_ADD("[ MODE ] END SETMODE MODE_WAIT_UPDATEINFO");

	__END_CATCH
}
