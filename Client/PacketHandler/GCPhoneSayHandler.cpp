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
// 클라이언트에서 서버로부터 메시지를 받았을때 실행되는 메쏘드이다.
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
		DEBUG_ADD_FORMAT("[PacketError-GCPhoneSayHandler] slot out of range: %d", slot);
		return;
	}

	char message[128];
	snprintf(message, sizeof(message), "[%s] %s",
						g_pUserInformation->PCSUserName[ slot ].GetString(), 
						pPacket->getMessage().c_str());

	//--------------------------------------------------
	// 욕 제거
	//--------------------------------------------------
	//g_pChatManager->RemoveCurse( message );

	//------------------------------------------------------
	// 특정 slot에 message를 추가시켜야 한다.
	//------------------------------------------------------
	//UI_AddChatToHistory( message );



	__END_CATCH
}
