//////////////////////////////////////////////////////////////////////
//
// Filename    : GCPhoneDisconnectedHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCPhoneDisconnected.h"
#include "UserInformation.h"
#include "ClientDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCPhoneDisconnectedHandler::execute ( GCPhoneDisconnected * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;


	//------------------------------------------------------------------
	// 접속이 끊긴 slot 체크한다.
	//------------------------------------------------------------------
	int slot = pPacket->getSlotID();

	// Unbounded on the wire; see the guard in GCPhoneConnectedHandler
	// for what an out-of-range slot does to these two arrays. Release()
	// is the worse half here: it delete[]s m_pString read from past the
	// end of the array.
	if (slot < 0 || slot >= MAX_PCS_SLOT)
	{
		DEBUG_ADD_FORMAT("[PacketError-GCPhoneDisconnectedHandler] slot out of range: %d", slot);
		return;
	}

	g_pUserInformation->OtherPCSNumber[ slot ] = 0;
	g_pUserInformation->PCSUserName[ slot ].Release();

	//------------------------------------------------------------------
	// PCS 접속 해제
	//------------------------------------------------------------------
	//UI_AcquireQuitPCSOnlineModeMessage();
//	UI_DisconnectPCS( slot );

	__END_CATCH
}
