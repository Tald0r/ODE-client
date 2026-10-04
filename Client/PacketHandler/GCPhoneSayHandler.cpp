//////////////////////////////////////////////////////////////////////
//
// Filename    : GCPhoneSayHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCPhoneSay.h"
#include "UserInformation.h"
#include "DebugLog.h"

//////////////////////////////////////////////////////////////////////
//
// Runs when the client receives this message from the server.
//
//////////////////////////////////////////////////////////////////////
void GCPhoneSayHandler::execute ( GCPhoneSay * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
	
	

	int slot = pPacket->getSlotID();

	// Unbounded on the wire; see the guard in GCPhoneConnectedHandler.
	// This one only reads the array, but it reads an MString from past
	// the end and calls GetString() on it, so the %s below prints from
	// whatever pointer was there.
	if (slot < 0 || slot >= MAX_PCS_SLOT)
	{
		DEBUG_ADD_FORMAT_WAR("[PacketError-GCPhoneSayHandler] slot out of range: %d", slot);
		return;
	}

	// A slot that never connected, or was hung up, has no name, and its
	// MString gives NULL, which %s must not be handed.
	const char* pName = g_pUserInformation->PCSUserName[ slot ].GetString();

	char message[128];
	snprintf(message, sizeof(message), "[%s] %s",
						pName == NULL ? "" : pName,
						pPacket->getMessage().c_str());

	//--------------------------------------------------
	// Remove curse words
	//--------------------------------------------------
	//g_pChatManager->RemoveCurse( message );

	//------------------------------------------------------
	// The message should be added to the slot's chat.
	//------------------------------------------------------
	//UI_AddChatToHistory( message );



	__END_CATCH
}
