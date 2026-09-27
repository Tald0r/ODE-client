//////////////////////////////////////////////////////////////////////
//
// Filename    : GCPhoneConnectedHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCPhoneConnected.h"
#include "UserInformation.h"
#include "ClientDef.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCPhoneConnectedHandler::execute ( GCPhoneConnected * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;


	int pcsNumber = pPacket->getPhoneNumber();
	int slot = pPacket->getSlotID();

	// The slot is a SlotID_t - a BYTE - read straight off the wire, and
	// GCPhoneConnected::read() does not bound it. The arrays below hold
	// MAX_PCS_SLOT entries, so an unchecked slot writes an int and then
	// assigns an MString hundreds of bytes past the end of
	// g_pUserInformation: MString::operator= reads m_pString out of
	// whatever is there and delete[]s it, so this was an arbitrary free
	// and an arbitrary write at a server's discretion. The same guard is
	// in GCPhoneDisconnected, GCPhoneSay and GCRing, which index the
	// same two arrays from the same unbounded field.
	if (slot < 0 || slot >= MAX_PCS_SLOT)
	{
		DEBUG_ADD_FORMAT("[PacketError-GCPhoneConnectedHandler] slot out of range: %d", slot);
		return;
	}

	// 번호를 저장해 둔다.
	g_pUserInformation->OtherPCSNumber[ slot ] = pcsNumber;
	g_pUserInformation->PCSUserName[ slot ] = pPacket->getName().c_str();

	// PCS를 띄운다.
	char pName[128];
	snprintf(pName, sizeof(pName), "%s", pPacket->getName().c_str());

//	UI_OnLinePCS(pName, pcsNumber);

	__END_CATCH
}
