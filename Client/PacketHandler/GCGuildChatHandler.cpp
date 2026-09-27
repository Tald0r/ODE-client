//////////////////////////////////////////////////////////////////////
//
// Filename    : GCGuildChatHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCGuildChat.h"
#include "ClientDef.h"
#include "MChatManager.h"
#include "UserInformation.h"
#include "UIFunction.h"
//////////////////////////////////////////////////////////////////////
//
// 클라이언트에서 서버로부터 메시지를 받았을때 실행되는 메쏘드이다.
//
//////////////////////////////////////////////////////////////////////
void GCGuildChatHandler::execute ( GCGuildChat * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
	
	//cout << pPacket->toString() << endl;
		char str[256];

		//---------------------------------------------------------------
		// 완성형 --> 조합형
		//---------------------------------------------------------------
		//UI_WansungToJohap( pPacket->getMessage().c_str(), str );
	
		
		snprintf( str, sizeof(str), "%s", pPacket->getMessage().c_str() );
		
		if (str[0] != '\0')
		{
			//--------------------------------------------------
			// 나에게 보이는 글인가?
			//--------------------------------------------------
			if (g_pChatManager->IsAcceptID( pPacket->getSender().c_str() ))
			{
				//--------------------------------------------------
				// 욕 제거
				//--------------------------------------------------
				g_pChatManager->RemoveCurse( str );
					
				// GUILDCHAT = 4
				// 2004, 11, 11, sobeit modify start
				if(0 == pPacket->getType()) // 길드챗 
					UI_AddChatToHistory( str, (char *)pPacket->getSender().c_str(), 4, pPacket->getColor() );
				else // union chat
				{
					char szName[128];
					snprintf(szName, sizeof(szName), "[%s]%s", pPacket->getSendGuildName().c_str(), pPacket->getSender().c_str());
					UI_AddChatToHistory( str, szName, 5, pPacket->getColor() );
				}
				//UI_AddChatToHistory( str, (char *)pPacket->getSender().c_str(), 4, pPacket->getColor() );
				// 2004, 11, 11, sobeit modify start
			}
		}

	__END_CATCH
}
