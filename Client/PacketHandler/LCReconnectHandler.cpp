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
#include "ClientDef.h"
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

extern int g_Dimension;
extern bool		UpdateSocketOutput();
extern BYTE g_macAddress[6];
//--------------------------------------------------------------------------------
// 로그인서버로부터 게임 서버의 주소와 포트, 그리고 인증키를 받은 즉시
// 게임 서버로 연결한 후, 인증키를 담은 CGConnect 패킷을 전송한다.
//--------------------------------------------------------------------------------
void LCReconnectHandler::execute ( LCReconnect * pPacket , Player * pPlayer )

{
	__BEGIN_TRY


	ClientPlayer * pClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);

	// 로그인 서버와의 연결을 종료한다
	// 이때 로그인 서버는 LCReconnect 패킷을 보내면서 연결을 종료한다는 사실에 유의하라.
	
	pClientPlayer->disconnect();

	// LCReconnect 패킷에 들어있는 정보를 사용해서, 게임 서버로 연결한다.
	DEBUG_ADD_FORMAT("Reconnecting to %s:%d", 
										pPacket->getGameServerIP().c_str(), 
										pPacket->getGameServerPort());
	
	try {
		pClientPlayer->getSocket()->reconnect( pPacket->getGameServerIP() , pPacket->getGameServerPort() );
		// reconnect하게 되면 소켓이 새로 만들어지게 된다.
		// 따라서, 이 소켓 역시 옵션을 새로 지정해줘야 한다.
		pClientPlayer->getSocket()->setNonBlocking();
		pClientPlayer->getSocket()->setLinger(0);

	} catch ( ConnectException & ce ) {
		throw Error(ce.toString());
	}

	// 연결이 이루어지면, 바로 CGConnect 패킷을 전송한다.
	// 이전에 Select 한 PC의 타입과 이름을 클라이언트 플레이어 객체에 저장해둔다.
	DEBUG_ADD("Sending CGConnect");

	//add by viva	
	CGConnectSetKey cgConnectSetKey;
	cgConnectSetKey.setEncryptKey(rand());
	cgConnectSetKey.setHashKey(rand());
	pClientPlayer->sendPacket(&cgConnectSetKey);
	UpdateSocketOutput();
	Sleep(500);
	// Registered as an explicit no-op (see PacketHandlerRegistry.cpp) - the
	// linked handler was always the empty stub.
	PacketDispatcher::dispatch(&cgConnectSetKey, pClientPlayer);
	//end

	// 재접속..
	CGConnect cgConnect;
	cgConnect.setKey( pPacket->getKey() );
	cgConnect.setPCType( pClientPlayer->getPCType() );
	cgConnect.setPCName( pClientPlayer->getPCName() );
	cgConnect.setMacAddress( g_macAddress );
	
	pClientPlayer->sendPacket( &cgConnect );
	pClientPlayer->setPlayerStatus( CPS_AFTER_SENDING_CG_CONNECT );	

	// 바로 보낸다.
//	EMBEDDED_BEGIN;
	UpdateSocketOutput();
//	EMBEDDED_END;
	
	
	// 2002.6.28 [UDP수정]
	// 서버에 UDP port를 알려주기 위해서
	if (!NetworkTransport::UsesWebSocket()) {
		CGPortCheck cgPortCheck;
		cgPortCheck.setPCName( g_pUserInformation->CharacterID.GetString() );

		std::string ServerAddress;
		uint ServerPort;

		if( g_pUserInformation->bKorean )
		{
			ServerAddress = g_pConfigKorean->getProperty("LoginServerAddress");
			ServerPort = g_pConfigKorean->getPropertyInt("LoginServerCheckPort");
			//add by sonic 2006.4.10 쇱꿎角뤠槨굶뒈뒈囹
		}
		else
		{
			ServerAddress = g_pConfigForeign->getProperty( g_Dimension, "LoginServerAddress" );
			ServerPort = g_pConfigForeign->getPropertyInt(g_Dimension, "LoginServerCheckPort");
		}
	
			
		DEBUG_ADD("[ ClientPacket] Send CGPortCheck ");
		g_pClientCommunicationManager->sendPacket( ServerAddress,
			ServerPort,
			&cgPortCheck );

	}

	DEBUG_ADD("[ MODE ] START SETMODE MODE_WAIT_UPDATEINFO");
	SetMode( MODE_WAIT_UPDATEINFO );
	DEBUG_ADD("[ MODE ] END SETMODE MODE_WAIT_UPDATEINFO");

	__END_CATCH
}
