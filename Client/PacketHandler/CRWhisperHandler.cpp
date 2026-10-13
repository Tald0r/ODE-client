//////////////////////////////////////////////////////////////////////
//
// Filename    : CRWhisperHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Rpackets/CRWhisper.h"
#include "CMessageArray.h"
#include "DebugLog.h"
#include "ClientMode.h"
#include "CommunicationPresentationHost.h"
#include "GameMessages.h"
#include "MChatManager.h"
#include "MGameStringTable.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "RequestServerPlayer.h"
#include "RequestUserManager.h"
#include "Rpackets/RCRequestVerify.h"
#include "UserInformation.h"


//////////////////////////////////////////////////////////////////////
//
// Runs when another client whispers to this one over a peer connection.
//
//////////////////////////////////////////////////////////////////////
void CRWhisperHandler::execute ( CRWhisper * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	
	
	RequestServerPlayer* pRequestServerPlayer = dynamic_cast<RequestServerPlayer*>( pPlayer );
		
	//-------------------------------------------------------------------
	// Is it really for this character?
	//-------------------------------------------------------------------
	if ((g_Mode!=MODE_GAME						// not in the game,
		&& g_Mode!=MODE_WAIT_UPDATEINFO			// not loading,
		&& g_Mode!=MODE_WAIT_SETPOSITION)		// not waiting for the position
		|| PlayerState::Creature()==NULL
		|| g_pChatManager==NULL
		|| g_pUserInformation==NULL 
		|| g_pUserInformation->CharacterID.GetString()==NULL
		|| pPacket->getTargetName() != g_pUserInformation->CharacterID.GetString()
		|| pPacket->getWorldID() != g_pUserInformation->WorldID)
	{		
		// If not, say the whisper failed
		RCRequestVerify _RCRequestVerify;
		_RCRequestVerify.setCode( REQUEST_VERIFY_WHISPER_FAILED );
		pRequestServerPlayer->sendPacket( &_RCRequestVerify );

		// and send that.
		try {
			pRequestServerPlayer->processOutput();
		} catch (Throwable& t)	{
			DEBUG_ADD_ERR(t.toString().c_str());
		}	
	}
	else
	{
		//-------------------------------------------------------------------
		// Remember the sender's address.
		//-------------------------------------------------------------------

		if (!g_pRequestUserManager->HasRequestUser(pPacket->getName().c_str()))
		{
			// getHost() returns std::string by value, so its c_str() dies at
			// the end of this statement; the copy has to outlive the call.
			const std::string strIP = pPlayer->getSocket()->getHost();

			g_pRequestUserManager->AddRequestUser( pPacket->getName().c_str(), strIP.c_str() );
		}

		// Refresh the IP and port.
		//RequestUserInfo* pUserInfo = g_pRequestUserManager->GetUserInfo( pPacket->getName().c_str() );

		//if (pUserInfo!=NULL)
		{
		//	Socket* pSocket = pPlayer->getSocket();
		//	pUserInfo->IP = pSocket->getHost().c_str();
			//pUserInfo->TCPPort = pSocket->getPort();
		}


		int numMessage = pPacket->getMessageSize();

		// CRWhisper::read admits a name of exactly 10 bytes and a message of
		// exactly 128 (CRWhisper.cpp:87,105), so each buffer needs room for
		// that many characters plus the terminator -- the old char[128] was
		// one byte short for the message. Same shape as GCWhisperHandler and
		// RCSayHandler; the sender here is a peer client over UDP.
		char str[128 + 1];
		char strName[10 + 1];
		snprintf(strName, sizeof(strName), "%s", pPacket->getName().c_str());

		//bool bMasterWords = (strstr(strName, "GM")!=NULL);
		bool bMasterWords = strncmp( strName, (*g_pGameStringTable)[UI_STRING_MESSAGE_MASTER_NAME].GetString(), (*g_pGameStringTable)[UI_STRING_MESSAGE_MASTER_NAME].GetLength() ) == 0;
		
		if (bMasterWords 
			|| g_pChatManager->IsAcceptID( strName ))
		{
			//--------------------------------------------------
			// There can be several messages.
			//--------------------------------------------------
			for (int i=0; i<numMessage; i++)
			{
				WHISPER_MESSAGE* pString = pPacket->popMessage();

				if (pString==NULL)
				{
					break;
				}

				snprintf(str, sizeof(str), "%s", pString->msg.c_str());

				//--------------------------------------------------
				// Curses are filtered unless an operator said it or the
				// player is a master: operators' words show whole, and
				// masters see everything. A ghost sees everything too.
				// (Unlike GCWhisperHandler, an operator player is not
				// exempt.)
				//--------------------------------------------------
				if (!bMasterWords && !g_pUserInformation->IsMaster 
					&& !PlayerState::HasEffectStatus( EFFECTSTATUS_GHOST ))
				{
					g_pChatManager->RemoveCurse( str, true );

					#ifndef _DEBUG
						//--------------------------------------------------
						// Another race's words are partly masked; the more
						// INT the player has, the more can be read.
						//--------------------------------------------------
						Race race = (Race)pPacket->getRace();
						if (PlayerState::GetRace() != race)
						{
							// INT goes up to 150, and any INT from 300 reads at the
							// cap, so a larger one is clamped there before the int
							// arithmetic, as GCWhisperHandler does.
							MStatus* pStatus = PlayerState::Status();
							const int playerINT = pStatus != NULL ? static_cast<int>(min(pStatus->GetINT(), static_cast<DWORD>(300))) : 0;
							int percent = min(75, 25+playerINT*100/(min(2, PlayerState::GetRace()+1)*150));
							g_pChatManager->AddMask(str, percent);
						}
					#endif
				}

				//sprintf(str, "<%s> %s", pPacket->getName().c_str(), pPacket->getMessage().c_str());
				// whisper = 2
				CommunicationPresentation::AddChatToHistory( str, strName, 2, pString->color );
				
				if ( bMasterWords )
				{
					// An operator's words also go to the notice log.
					g_pNoticeMessage->AddSafeFormat("%s>%s",strName,str);
				}

				// The sender becomes the whisper target: ID+' '
				char strWhisperID[128];
				snprintf(strWhisperID, sizeof(strWhisperID), "%s ", pPacket->getName().c_str());
				g_pUserInformation->WhisperID = strWhisperID;



				delete pString;
			}
		}
	}

	//-------------------------------------------------------------------
	// Close the peer connection.
	//-------------------------------------------------------------------
	throw DisconnectException("Disconnect after Whisper");
	/*
	try {

		pRequestServerPlayer->disconnect( UNDISCONNECTED );

	} catch (Throwable& t)	{
		DEBUG_ADD(t.toString().c_str());
	}
	*/


	__END_CATCH
}
