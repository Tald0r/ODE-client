//////////////////////////////////////////////////////////////////////////////
// Filename    : GCPartyInviteHandler.cpp
// Written By  : 김성민
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Client_PCH.h"
#include "Gpackets/GCPartyInvite.h"
#include "Cpackets/CGPartyInvite.h"
#include "ClientPlayer.h"
#include "MGameStringTable.h"
#include "CommunicationPresentationHost.h"
#include "DialogPresentationHost.h"
#include "GameInfoPresentationHost.h"
#include "GameMessages.h"
#include "MCreatureTable.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "TempInformation.h"
#include "MParty.h"
#include "CMessageArray.h"
#include "DebugLog.h"
#include "MHelpDef.h"

void GCPartyInviteHandler::execute (GCPartyInvite * pPacket , Player * pPlayer)
	 

{
	__BEGIN_TRY
	

	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (!ZoneState::Exists()
		|| g_pTempInformation==NULL
		|| pPlayerCreature==NULL
		|| g_pParty==NULL
		|| g_pGameMessage==NULL)
	{
		DEBUG_ADD_WAR("GCPartyInviteHandler Failed");
		return;
	}

	int targetID = pPacket->getTargetObjectID();

	ClientPlayer * pClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);
			
	
	bool bCancelAll = false;
	bool bMessage = false;
	int cancelMessage = 0;
	bool bJoinedSomeone = false;

	switch (pPacket->getCode())
	{
		//--------------------------------------------------------------------
		//
		// Someone invites me.
		//
		//--------------------------------------------------------------------
		case GC_PARTY_INVITE_REQUEST :
			//----------------------------------------------------------------------
			// I already invited someone (upstream said 'trade')
			//----------------------------------------------------------------------
			if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_PARTY
				&& g_pTempInformation->GetMode()==TempInformation::MODE_PARTY_REQUEST)
			{
				int code;

				//----------------------------------------------------------------------
				// the one I invited invites me too --> OK
				//----------------------------------------------------------------------
				if (g_pTempInformation->Value1==targetID)
				{
					code = CG_PARTY_INVITE_ACCEPT;

					g_pTempInformation->SetMode(TempInformation::MODE_NULL);

					//-------------------------------------------
					// The party is formed
					//-------------------------------------------
					// UI_RunParty();
					// Wait for the party-success packet.
					CommunicationPresentation::ClosePartyCancel();

					bJoinedSomeone = true;
					
					//2004, 5, 6 sobeit add start
					GameInfoPresentation::ExecuteHelpEvent(HELP_EVENT_PARTY);
					//2004, 5, 6 sobeit add end
				}
				//----------------------------------------------------------------------
				// Otherwise
				//----------------------------------------------------------------------
				else
				{
					code = CG_PARTY_INVITE_BUSY;
				}

				CGPartyInvite _CGPartyInvite;
				_CGPartyInvite.setTargetObjectID( targetID );
				_CGPartyInvite.setCode( code );				

				pClientPlayer->sendPacket( &_CGPartyInvite );

			}
			//----------------------------------------------------------------------
			// Party? Y/N
			//----------------------------------------------------------------------
			else
			{
				//-------------------------------------------
				// Join the party?
				//-------------------------------------------
				if ((PlayerState::GetWaitVerify()!=PlayerState::WAIT_VERIFY_NULL && PlayerState::GetWaitVerify()!=PlayerState::WAIT_VERIFY_PARTY)			// waiting on another verification
					//|| g_pPlayer->IsRepeatAction()		// repeating an action
					|| DialogPresentation::IsLockInput()		// talking to an NPC..
					|| CreatureState::GetCreatureType(pPlayerCreature)==CREATURETYPE_BAT	// a bat
					|| CreatureState::GetCreatureType(pPlayerCreature)==CREATURETYPE_WOLF	// a wolf
					|| !g_pParty->IsAcceptInvite()
					)
				{
					// Refuse the party
					CGPartyInvite _CGPartyInvite;
					_CGPartyInvite.setTargetObjectID( targetID );
					_CGPartyInvite.setCode( CG_PARTY_INVITE_REJECT );

					pClientPlayer->sendPacket( &_CGPartyInvite );
				}
				else
				{
					CommunicationPresentation::RunPartyRequest( targetID );
				}
			}
		break;

		//--------------------------------------------------------------------
		//
		// The one inviting me cancelled.
		//
		//--------------------------------------------------------------------
		case GC_PARTY_INVITE_CANCEL :
			if (g_pTempInformation->GetMode() == TempInformation::MODE_PARTY_REQUEST)
			{
				g_pTempInformation->SetMode(TempInformation::MODE_NULL);
			}		

			CommunicationPresentation::ClosePartyRequest();
			CommunicationPresentation::ClosePartyAsk();			
		break;

		//--------------------------------------------------------------------
		//
		// The one I invited joined.
		//
		//--------------------------------------------------------------------
		case GC_PARTY_INVITE_ACCEPT :
			//----------------------------------------------------------------------
			// I already invited someone else
			//----------------------------------------------------------------------
			if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_PARTY
				&& g_pTempInformation->GetMode()==TempInformation::MODE_PARTY_REQUEST)
			{
				g_pTempInformation->SetMode(TempInformation::MODE_NULL);
			}
			
			bJoinedSomeone = true;

			CommunicationPresentation::ClosePartyCancel();			

			//2004, 5, 6 sobeit add start
			GameInfoPresentation::ExecuteHelpEvent(HELP_EVENT_PARTY);
			//2004, 5, 6 sobeit add end	
		break;

		//--------------------------------------------------------------------
		//
		// The one I invited refused.
		//
		//--------------------------------------------------------------------
		case GC_PARTY_INVITE_REJECT :
			bCancelAll = true;
			bMessage = true;
			cancelMessage = STRING_MESSAGE_PARTY_REJECTED;			
		break;

		//--------------------------------------------------------------------
		//
		// The one I invited is already answering another invitation.
		//
		//--------------------------------------------------------------------
		case GC_PARTY_INVITE_BUSY :
			bCancelAll = true;
			bMessage = true;
			cancelMessage = STRING_MESSAGE_PARTY_BUSY;
		break;

		//--------------------------------------------------------------------
		//
		// The one I invited is already in another party.
		//
		//--------------------------------------------------------------------
		case GC_PARTY_INVITE_ANOTHER_PARTY :
		{
			MCreature* pCreature = ZoneState::GetCreature( pPacket->getTargetObjectID() );
			
			if ( pCreature!=NULL )
			{
				// Say so only when it is not the player's party.
				if (CreatureState::IsPlayerParty(pCreature))
				{
					bCancelAll = true;
					bMessage = false;
				}
				else
				{
					bCancelAll = true;
					bMessage = true;
					cancelMessage = STRING_MESSAGE_IN_ANOTHER_PARTY;					
				}
			}
		}
		break;

		//--------------------------------------------------------------------
		//
		// It is full.
		// 
		//--------------------------------------------------------------------
		case GC_PARTY_INVITE_MEMBER_FULL :
			bCancelAll = true;
			bMessage = true;
			cancelMessage = STRING_MESSAGE_PARTY_FULL;
		break;
	}

	//-------------------------------------------
	// Joined: say so
	//-------------------------------------------
	if (bJoinedSomeone)
	{
		MCreature* pCreature = ZoneState::GetCreature( g_pTempInformation->PartyInviter );

		if (pCreature!=NULL)
		{
			g_pGameMessage->AddSafeFormat(GetGameString(STRING_MESSAGE_SOMEONE_JOINED_PARTY), CreatureState::GetName(pCreature));
		}
	}

	//--------------------------------------------------------------------
	// Close everything, and say why
	//--------------------------------------------------------------------
	if (bCancelAll)
	{
		if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_PARTY
			&& g_pTempInformation->GetMode()==TempInformation::MODE_PARTY_REQUEST)
		{			
			// Drop the verification data
			PlayerState::SetWaitVerifyNULL();
			g_pTempInformation->SetMode(TempInformation::MODE_NULL);
		}

		// Close "cancel the party?".
		CommunicationPresentation::ClosePartyCancel();

		CommunicationPresentation::ClosePartyAsk();
		CommunicationPresentation::ClosePartyRequest();

		if (bMessage)
		{
			DialogPresentation::PopupFreeMessageDlg( GetGameString(cancelMessage) );		
		}
	}


	__END_CATCH
}
