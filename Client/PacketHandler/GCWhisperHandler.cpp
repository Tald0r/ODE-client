//////////////////////////////////////////////////////////////////////
//
// Filename    : GCWhisperHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCWhisper.h"
#include "CMessageArray.h"
#include "CommunicationPresentationHost.h"
#include "GameMessages.h"
#include "MChatManager.h"
#include "MGameStringTable.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "UserInformation.h"

extern CMessageArray*		g_pNoticeMessage;

//////////////////////////////////////////////////////////////////////
//
// Runs when the client receives this message from the server.
//
//////////////////////////////////////////////////////////////////////
void GCWhisperHandler::execute ( GCWhisper * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
	


	
	// GCWhisper::read admits a name of exactly 10 bytes and a message of exactly
	// 128 (GCWhisper.cpp:29,43), and GCWhisperFactory::getPacketMaxSize budgets
	// the same two numbers -- so the buffers need room for that many characters
	// plus the terminator, and the old char[128] was one byte short for the
	// message. The bound cannot be moved to the packet instead: a 128-byte
	// message is what the wire layout declares legal and what GCWhisper::write
	// on the server side happily produces.
	char str[128 + 1];
	char strName[10 + 1];
	snprintf(str, sizeof(str), "%s", pPacket->getMessage().c_str());
	snprintf(strName, sizeof(strName), "%s", pPacket->getName().c_str());

//	bool bMasterWords = (strstr(strName, "GM")!=NULL);
	bool bMasterWords = strncmp( strName, (*g_pGameStringTable)[UI_STRING_MESSAGE_MASTER_NAME].GetString(), (*g_pGameStringTable)[UI_STRING_MESSAGE_MASTER_NAME].GetLength() ) == 0;

	if (bMasterWords 
		|| g_pChatManager->IsAcceptID( strName ))
	{
		//--------------------------------------------------
		// Curses are filtered unless an operator said it or the
		// player is one: operators' words show whole, and operators
		// see everything. A ghost sees everything too.
		//--------------------------------------------------
		if (!bMasterWords && !g_pUserInformation->IsMaster &&
			!PlayerState::IsOperator()
			&& !PlayerState::HasEffectStatus( EFFECTSTATUS_GHOST ))
		{
			g_pChatManager->RemoveCurse( str, true );

			#ifndef _DEBUG
				//--------------------------------------------------
				// Another race's words are partly masked; the more INT
				// the player has, the more can be read.
				//--------------------------------------------------
				Race race = (Race)pPacket->getRace();
				if (PlayerState::GetRace() != race)
				{
					// INT goes up to 150.
					MStatus* pStatus = PlayerState::Status();
					const int playerINT = pStatus != NULL ? static_cast<int>(pStatus->GetINT()) : 0;
					int percent = min(75, 25+playerINT*100/(min(2, PlayerState::GetRace()+1)*150));
					
					g_pChatManager->AddMask(str, percent);
				}
			#endif
		}

		//sprintf(str, "<%s> %s", pPacket->getName().c_str(), pPacket->getMessage().c_str());
		// whisper = 2
		CommunicationPresentation::AddChatToHistory( str, strName, 2, pPacket->getColor() );
		
		if ( bMasterWords )
		{
			// An operator's words also go to the notice log.
			g_pNoticeMessage->AddSafeFormat("%s>%s",strName,str);
		}

		// The sender becomes the whisper target: ID+' '
		char strWhisperID[128];
		snprintf(strWhisperID, sizeof(strWhisperID), "%s ", pPacket->getName().c_str());
		g_pUserInformation->WhisperID = strWhisperID;

	}



	__END_CATCH
}
