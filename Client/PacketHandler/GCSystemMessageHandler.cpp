//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSystemMessageHandler.cc
// Written By  : elca
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSystemMessage.h"
#include "ClientDef.h"
#include "UIFunction.h"
#include "MGameStringTable.h"
#include "SafeFormat.h"
#include "Client.h"
//////////////////////////////////////////////////////////////////////
//
// 클라이언트에서 서버로부터 메시지를 받았을때 실행되는 메쏘드이다.
//
//////////////////////////////////////////////////////////////////////
void GCSystemMessageHandler::execute ( GCSystemMessage * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
	
	// The message can be up to 255 bytes; a fixed char[128] here overflowed.
	static std::string previous1;
	switch(pPacket->getType())
	{ 
		case SYSTEM_MESSAGE_HOLY_LAND :		// 아담의 성지 관련
			if(g_pUserOption->DoNotShowHolyLandMsg)
				return;
			break;

		case SYSTEM_MESSAGE_NORMAL:
			break;

		case SYSTEM_MESSAGE_OPERATOR:	// 운영자 말씀
			break;
	
		case SYSTEM_MESSAGE_MASTER_LAIR:	// 마스터 레어 관련
			if(g_pUserOption->DoNotShowLairMsg)
				return;
			break;

		case SYSTEM_MESSAGE_COMBAT:		// 전쟁 관련
			if(g_pUserOption->DoNotShowWarMsg)
				return;
			break;
	
		case SYSTEM_MESSAGE_INFO: 		// 특정한 정보 관련
			break;
			
		case SYSTEM_MESSAGE_RANGER_CHAT:
			{
				// getMessage() returns by value; keep the string alive past
				// the call instead of holding a dangling c_str() pointer.
				std::string rangerMessage = pPacket->getMessage();
				if (!rangerMessage.empty())
				{
					UI_SetRangerChatString((char*)rangerMessage.c_str());
				}
			}
			return;

		case SYSTEM_MESSAGE_PLAYER:	    // add by Coffee 2007-8-2
			{
				// Same by-value hazard as above; one local copy serves the
				// Add and the dedup record.
				std::string playerMessage = pPacket->getMessage();

				if (!playerMessage.empty())
				{
// 				if (strcmp(previous1, message)==0)
// 				{
// 					BOOL bExist = FALSE;
// 
// 					//--------------------------------------------------------------------
// 					// 이미 있는 메세지인지 검사한다.
// 					//--------------------------------------------------------------------
// 					for (int i=0; i<g_pPlayerMessage->GetSize(); i++)
// 					{
// 						if (strcmp((*g_pPlayerMessage)[i], message)==0)
// 						{
// 							bExist = TRUE;
// 						}
// 					}
// 
// 					//--------------------------------------------------------------------
// 					// 없는거면 추가한다.		
// 					//--------------------------------------------------------------------
// 					if (!bExist)
// 					{
// 						g_pPlayerMessage->Add( message );
// 					}
// 				}
// 				//--------------------------------------------------------------------
// 				// 새로운 메세지이면 추가한다.
// 				//--------------------------------------------------------------------
// 				else
// 				{
					g_pPlayerMessage->Add( playerMessage.c_str() );

					previous1 = playerMessage;
//				}
				}
			}
			return;

		default:
			break;
			
	}

	// Same overflow as previous1 above: the message outgrows a char[128].
	static std::string previous;

	// Fix: Store string in local variable to avoid use-after-free
	// The temporary string returned by getMessage() is destroyed at end of statement
	std::string messageStr = pPacket->getMessage();
	const char* message = messageStr.c_str();

	// add by Coffee 2007-8-2 藤속溝固斤口혐깎
		char *pMsg = NULL;
		if (message!=NULL && pPacket->getType() != SYSTEM_MESSAGE_PLAYER )
		{
			// The old "+20" assumed the table format never adds more than 20
			// characters around the message. Size the buffer from the format
			// string itself instead, which is a safe upper bound for a single
			// %s substitution.
			//
			// The format is a String.inf entry, so it goes through SafeFormat
			// rather than snprintf: bounding the write was never the whole
			// problem here, because a second %s in that entry would read a
			// stack word as a char* and print it inside the bound. This site
			// hoisted the entry into a local first, which is why ratchet R7 -
			// which looks for the lookup at the format argument itself - read
			// zero while it was still live. Both adversarial reviewers of
			// task 5.4's fourth slice found it independently.
			size_t msgSize = (*g_pGameStringTable)[UI_STRING_MESSAGE_SYSTEM].GetLength()+messageStr.size()+1;
			pMsg = new char[msgSize];
			SafeFormat::Format(pMsg, msgSize, GetGameString(UI_STRING_MESSAGE_SYSTEM), message);
			pPacket->setMessage(pMsg);
			SAFE_DELETE_ARRAY( pMsg );
		}
		// Update message pointer after potential setMessage()
		messageStr = pPacket->getMessage();
		message = messageStr.c_str();
	// add end by Coffee 2007-8-2
	//--------------------------------------------------------------------
	// print to the system message area
	//--------------------------------------------------------------------
	if (previous == messageStr)
	{
		BOOL bExist = FALSE;

		//--------------------------------------------------------------------
		// 이미 있는 메세지인지 검사한다.
		//--------------------------------------------------------------------
		for (int i=0; i<g_pSystemMessage->GetSize(); i++)
		{
			if (strcmp((*g_pSystemMessage)[i], message)==0)
			{
				bExist = TRUE;
			}
		}

		//--------------------------------------------------------------------
		// 없는거면 추가한다.		
		//--------------------------------------------------------------------
		if (!bExist)
		{
			g_pSystemMessage->Add( message );
		}
	}
	//--------------------------------------------------------------------
	// 새로운 메세지이면 추가한다.
	//--------------------------------------------------------------------
	else
	{
		g_pSystemMessage->Add( message );

		previous = messageStr;
	}


	__END_CATCH
}
