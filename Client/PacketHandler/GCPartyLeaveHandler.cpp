//////////////////////////////////////////////////////////////////////////////
// Filename    : GCPartyLeaveHandler.cpp
// Written By  : 김성민
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Client_PCH.h"
#include "Gpackets/GCPartyLeave.h"
#include "MGameStringTable.h"
#include "SafeFormat.h"
#include "MParty.h"
#include "UserInformation.h"
#include "CMessageArray.h"
#include "CommunicationPresentationHost.h"
#include "DebugLog.h"
#include "GameMessages.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"

extern CMessageArray*		g_pGameMessage;

void GCPartyLeaveHandler::execute (GCPartyLeave * pPacket , Player * pPlayer)
	 

{
	__BEGIN_TRY
	(void)pPlayer;
	

	if (!ZoneState::Exists()
		|| PlayerState::Creature()==NULL
		|| g_pUserInformation==NULL
		|| g_pParty==NULL
		|| g_pGameMessage==NULL)
	{
		DEBUG_ADD_WAR("GCPartyLeaveHandler Failed");
		return;
	}

	
	// getExpeller()/getExpellee() return std::string by value, so the temporary
	// died at the end of its own full-expression and the pointers below dangled
	// for the rest of the function. Bind the strings to locals that outlive
	// every use.
	const std::string expeller = pPacket->getExpeller();	// the one who expelled
	const std::string expellee = pPacket->getExpellee();	// the one expelled

	const char* pExpeller = expeller.c_str();
	const char* pExpellee = expellee.c_str();

	char str[256];
			
	bool bRemovePartyCheck = false;
	
	//-------------------------------------------------------------------
	// The leave this client asked for is answered
	//-------------------------------------------------------------------
	if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_PARTY_LEAVE)
	{
		PlayerState::SetWaitVerifyNULL();
	}

	//-------------------------------------------------------------------
	// Someone left on their own
	//-------------------------------------------------------------------
	if (pExpeller==NULL || pExpeller[0]=='\0')
	{
		//----------------------------------------------------------
		// I left
		//----------------------------------------------------------
		if (g_pUserInformation->CharacterID==pExpellee)
		{
			g_pParty->UnSetPlayerParty();
			g_pParty->Release();	// my party is gone			

			g_pGameMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_REMOVE_PARTY_MYSELF].GetString() );
			//g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_REMOVE_PARTY_MYSELF].GetString() );

			CommunicationPresentation::CloseParty();
		}
		//----------------------------------------------------------
		// someone else left
		//----------------------------------------------------------
		else if (g_pParty->RemoveMember( pExpellee ))
		{
			SafeFormat::Format(str, GetGameString(STRING_MESSAGE_REMOVE_PARTY_HIMSELF), 
					pExpellee);		
			
			//g_pUIDialog->PopupFreeMessageDlg( str );
			g_pGameMessage->Add( str );

			bRemovePartyCheck = true;
		}
	}
	//-------------------------------------------------------------------
	// Someone expelled someone
	//-------------------------------------------------------------------
	else
	{
		//-------------------------------------------------------------------
		// I expelled pExpellee
		//-------------------------------------------------------------------
		if (g_pUserInformation->CharacterID==pExpeller)
		{		
			if (g_pParty->RemoveMember( pExpellee ))
			{
				SafeFormat::Format(str, GetGameString(STRING_MESSAGE_KICK_PARTY_MEMBER_OK), 
						pExpellee);

				//g_pUIDialog->PopupFreeMessageDlg( str );
				g_pGameMessage->Add( str );
			}

			bRemovePartyCheck = true;
		}
		//-------------------------------------------------------------------
		// pExpeller expelled me
		//-------------------------------------------------------------------
		else if (g_pUserInformation->CharacterID==pExpellee)
		{
			g_pParty->UnSetPlayerParty();
			g_pParty->Release();	// my party is gone			

			SafeFormat::Format(str, GetGameString(STRING_MESSAGE_KICKED_FROM_PARTY), 
						pExpeller);		
			
			//g_pUIDialog->PopupFreeMessageDlg( str );
			g_pGameMessage->Add( str );

			bRemovePartyCheck = true;
		}
		//-------------------------------------------------------------------
		// pExpeller expelled pExpellee
		//-------------------------------------------------------------------
		else if (g_pParty->RemoveMember( pExpellee ))
		{
			SafeFormat::Format(str, GetGameString(STRING_MESSAGE_KICK_PARTY_MEMBER), 
					pExpeller, pExpellee);

			//g_pUIDialog->PopupFreeMessageDlg( str );
			g_pGameMessage->Add( str );
		}
	}

	
	//--------------------------------------------------
	// Is the party over?
	//--------------------------------------------------
	if (bRemovePartyCheck && g_pParty->GetSize()==0)
	{		
		CommunicationPresentation::CloseParty();

		// the message is enough
		//g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_REMOVE_PARTY].GetString() );		
		g_pGameMessage->Add( (*g_pGameStringTable)[STRING_MESSAGE_REMOVE_PARTY].GetString() );
	}


	__END_CATCH
}
