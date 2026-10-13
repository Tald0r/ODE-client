//----------------------------------------------------------------------
//
// Filename    : RCSayHandler.cpp
// Written By  : Reiot
// Description :
//
//----------------------------------------------------------------------

// include files
#include "Client_PCH.h"
#include "Rpackets/RCSay.h"
#include "ClientMode.h"
#include "CommunicationPresentationHost.h"
#include "PlayerStateHost.h"
#include "UserInformation.h"
#include "MChatManager.h"
#include "MGameStringTable.h"

//----------------------------------------------------------------------
// 
// RCSayHander::execute()
// 
//----------------------------------------------------------------------
void RCSayHandler::execute ( RCSay * pPacket )

{
	__BEGIN_TRY

	if ((g_Mode==MODE_GAME
			|| g_Mode==MODE_WAIT_UPDATEINFO			// 로딩 중이 아니거나..
			|| g_Mode==MODE_WAIT_SETPOSITION		// 좌표 기다리는 경우
			)
		&& g_pUserInformation!=NULL
		&& g_pChatManager!=NULL)
	{
		//g_pClientCommunicationManager->sendPacket( pPacket->getHost() , pPacket->getPort() , &glIncomingConnectionOK );
		// RCSay::read admits a name of exactly 20 bytes and a message of exactly
		// 128 (RCSay.cpp:30,43), and RCSayFactory::getPacketMaxSize budgets the
		// same two numbers -- so the buffers need room for that many characters
		// plus the terminator, and the old char[128] was one byte short for the
		// message. The sender here is a peer client over UDP, so the length is
		// not even nominally trustworthy.
		char str[128 + 1];
		char strName[20 + 1];
		snprintf(str, sizeof(str), "%s", pPacket->getMessage().c_str());
		snprintf(strName, sizeof(strName), "%s", pPacket->getName().c_str());

		//bool bMasterWords = (strstr(strName, "GM")!=NULL);
		bool bMasterWords = strncmp( strName, (*g_pGameStringTable)[UI_STRING_MESSAGE_MASTER_NAME].GetString(), (*g_pGameStringTable)[UI_STRING_MESSAGE_MASTER_NAME].GetLength() ) == 0;

		if (bMasterWords 
			|| g_pChatManager->IsAcceptID( strName ))
		{
			//--------------------------------------------------
			// 욕 제거
			// 운영자가 한 말도 아니고 나도 운영자가 아니면 filter한다.
			// --> 운영자의 말은 다 보이고 운영자는 다 본다.
			//--------------------------------------------------
			if (!bMasterWords && !g_pUserInformation->IsMaster 
				&& !PlayerState::HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
				&& !g_bLight
#endif
				)
			{
				g_pChatManager->RemoveCurse( str );

			}

			//sprintf(str, "<%s> %s", pPacket->getName().c_str(), pPacket->getMessage().c_str());
			// party = 3
			CommunicationPresentation::AddChatToHistory( str, strName, 3, pPacket->getColor() );

			// 귓속말 대상 설정 ID+' '
			//char strWhisperID[128];
			//sprintf(strWhisperID, "%s ", pPacket->getName().c_str());
			//g_pUserInformation->WhisperID = strWhisperID;

			// [도움말] 귓속말 받을 때
//			__BEGIN_HELP_EVENT
//
//				//ExecuteHelpEvent( HE_CHAT_WHISPERED );	
//			__END_HELP_EVENT
		}
	}
		
	__END_CATCH
}
