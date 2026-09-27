//////////////////////////////////////////////////////////////////////
//
// Filename    : GCRequestFailedHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCRequestFailed.h"

void GCRequestFailedHandler::execute ( GCRequestFailed * pPacket , Player * pPlayer )
{
	__BEGIN_TRY
	(void)pPacket;
	(void)pPlayer;

	// Nothing to do. This answered a CGRequestIP the server could not
	// satisfy by forgetting the pending request (and, once, dropping a
	// queued whisper and printing a chat line); every request of that
	// kind was made by the outbound peer side, which upstream had
	// compiled out and docs/RESTRUCTURING.md task 5.2's eighth slice
	// deleted. Registered so the packet is consumed, not fatal.

	__END_CATCH
}
