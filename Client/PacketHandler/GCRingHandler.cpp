//////////////////////////////////////////////////////////////////////
//
// Filename    : GCRingHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCRing.h"
#include "UserInformation.h"
#include "ClientDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCRingHandler::execute ( GCRing * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;


	int slot = pPacket->getSlotID();
	char pName[128];

	snprintf(pName, sizeof(pName), "%s", pPacket->getName().c_str());

	// 
//	UI_OnLinePCS(pName, pcsNumber);

	// PCS에 전화가 왔다고 알린다.
//	UI_PleaseConnectPCS(pName, pcsNumber, slot);

	// 번호 기억
	// Unbounded on the wire; see the guard in GCPhoneConnectedHandler.
	if (slot < 0 || slot >= MAX_PCS_SLOT)
	{
		DEBUG_ADD_FORMAT("[PacketError-GCRingHandler] slot out of range: %d", slot);
		return;
	}

	g_pUserInformation->OtherPCSNumber[ slot ] = pPacket->getPhoneNumber();
	g_pUserInformation->PCSUserName[ slot ] = pPacket->getName().c_str();

	__END_CATCH
}
