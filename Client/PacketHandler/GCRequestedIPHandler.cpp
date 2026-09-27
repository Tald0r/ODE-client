//////////////////////////////////////////////////////////////////////
//
// Filename    : GCRequestedIPHandler.cc
// Written By  : crazydog
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCRequestedIP.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void GCRequestedIPHandler::execute ( GCRequestedIP * pPacket , Player * pPlayer )
{
	__BEGIN_TRY
	(void)pPacket;
	(void)pPlayer;

	// Nothing to do. The reply to a CGRequestIP - a peer's address, and
	// the connection this client would then have dialled to it for a
	// whisper or a profile - served the outbound peer side, which
	// upstream had already compiled out (this handler began with
	// `if (bKorean == false || 1) return;`) and which
	// docs/RESTRUCTURING.md task 5.2's eighth slice deleted. The handler
	// stays registered so that a server which still sends the packet
	// gets it consumed rather than the game connection dropped for a
	// packet with no handler.

	__END_CATCH
}
