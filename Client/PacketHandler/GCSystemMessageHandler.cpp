//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSystemMessageHandler.cc
// Written By  : elca
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSystemMessage.h"
#include "CMessageArray.h"
#include "CommunicationPresentationHost.h"
#include "GameMessages.h"
#include "MGameStringTable.h"
#include "SafeFormat.h"
//////////////////////////////////////////////////////////////////////
//
// Runs when the client receives this message from the server.
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
		case SYSTEM_MESSAGE_HOLY_LAND :		// about Adam's holy land
			if(CommunicationPresentation::HideHolyLandMessages())
				return;
			break;

		case SYSTEM_MESSAGE_NORMAL:
			break;

		case SYSTEM_MESSAGE_OPERATOR:	// an operator's words
			break;
	
		case SYSTEM_MESSAGE_MASTER_LAIR:	// about a master's lair
			if(CommunicationPresentation::HideLairMessages())
				return;
			break;

		case SYSTEM_MESSAGE_COMBAT:		// about a war
			if(CommunicationPresentation::HideWarMessages())
				return;
			break;
	
		case SYSTEM_MESSAGE_INFO: 		// some particular information
			break;
			
		case SYSTEM_MESSAGE_RANGER_CHAT:
			{
				// getMessage() returns by value; keep the string alive past
				// the call instead of holding a dangling c_str() pointer.
				std::string rangerMessage = pPacket->getMessage();
				if (!rangerMessage.empty())
				{
					CommunicationPresentation::SetRangerChatString(rangerMessage.c_str());
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
					g_pPlayerMessage->Add( playerMessage.c_str() );

					previous1 = playerMessage;
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

	// add by Coffee 2007-8-2: mark a system message as one
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
			delete [] pMsg;
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
		// Is the message already shown?
		//--------------------------------------------------------------------
		for (int i=0; i<g_pSystemMessage->GetSize(); i++)
		{
			if (strcmp((*g_pSystemMessage)[i], message)==0)
			{
				bExist = TRUE;
			}
		}

		//--------------------------------------------------------------------
		// If not, add it.
		//--------------------------------------------------------------------
		if (!bExist)
		{
			g_pSystemMessage->Add( message );
		}
	}
	//--------------------------------------------------------------------
	// A new message is added.
	//--------------------------------------------------------------------
	else
	{
		g_pSystemMessage->Add( message );

		previous = messageStr;
	}


	__END_CATCH
}
