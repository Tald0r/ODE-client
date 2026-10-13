//////////////////////////////////////////////////////////////////////////////
// Filename    : GCPartyErrorHandler.cpp
// Written By  : 김성민
// Description :
//////////////////////////////////////////////////////////////////////////////

#include "Client_PCH.h"
#include "Gpackets/GCPartyError.h"
#include "CommunicationPresentationHost.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "MGameStringTable.h"
#include "PlayerStateHost.h"
#include "TempInformation.h"

void GCPartyErrorHandler::execute (GCPartyError * pPacket , Player * pPlayer)
{
	__BEGIN_TRY
	(void)pPlayer;

	// Without a player or the temp-mode register there is no request to end.
	// (A missing dialog skips the messages; the executable checked for one.)
	if (PlayerState::Creature()==NULL
		|| g_pTempInformation==NULL)
	{
		DEBUG_ADD_WAR("GCPartyErrorHandler Failed");
		return;
	}

	int code = pPacket->getCode();

	//----------------------------------------------------------------------
	// A party request that cannot go ahead ends the one this player made.
	//----------------------------------------------------------------------
	if (code==GC_PARTY_ERROR_TARGET_NOT_EXIST
		|| code==GC_PARTY_ERROR_RACE_DIFFER
		|| code==GC_PARTY_ERROR_NOT_SAFE)
	{	
		if (g_pTempInformation->GetMode() == TempInformation::MODE_PARTY_REQUEST)
		{
			// Close the "cancel the request?" box.
			CommunicationPresentation::ClosePartyCancel();

			// The request is answered.
			if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_PARTY)
			{
				PlayerState::SetWaitVerifyNULL();
			}

			g_pTempInformation->SetMode(TempInformation::MODE_NULL);
		}
	}

	switch (code)
	{
		// The invited player is not there.
		case GC_PARTY_ERROR_TARGET_NOT_EXIST :
			CommunicationPresentation::ClosePartyCancel();
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_PARTY_NOBODY].GetString());
		break;

		// The invited player is of another race.
		case GC_PARTY_ERROR_RACE_DIFFER :
			CommunicationPresentation::ClosePartyCancel();
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_RACE_DIFFER].GetString());
		break;

		// A party is made only in a safety zone.
		case GC_PARTY_ERROR_NOT_SAFE :
			CommunicationPresentation::ClosePartyCancel();
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_PARTY_SAFETY_ZONE_ONLY].GetString());
		break;

		// Someone is transformed.
		case GC_PARTY_ERROR_NOT_NORMAL_FORM :
			CommunicationPresentation::ClosePartyCancel();
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_NOT_NORMAL_FORM].GetString());
		break;

		// Already in a party, and inviting again.
		case GC_PARTY_ERROR_ALREADY_INVITING :
			CommunicationPresentation::ClosePartyCancel();
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_PARTY_BUSY].GetString());
		break;

		// A party packet while not in a party.
		case GC_PARTY_ERROR_NOT_INVITING :
			#ifdef OUTPUT_DEBUG
				DEBUG_ADD_ERR("[Error] Server is Not Party Mode");
			#endif
		break;

		// No right to expel a member.
		case GC_PARTY_ERROR_NO_AUTHORITY :
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_NO_AUTHORITY].GetString());
			#ifdef OUTPUT_DEBUG
				DEBUG_ADD("Party request refused: no authority");
			#endif
		break;

		// Anything else.
		case GC_PARTY_ERROR_UNKNOWN :
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_ERROR_PARTY].GetString());
			#ifdef OUTPUT_DEBUG
				DEBUG_ADD_ERR("[Error] GC_PARTY_ERROR_UNKNOWN");
			#endif
		break;
	}

	__END_CATCH
}
