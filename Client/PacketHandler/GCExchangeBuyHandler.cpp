//////////////////////////////////////////////////////////////////////////////
// Filename    : GCExchangeBuyHandler.cpp
// Description : Client-side handler for GCExchangeBuy, the server's answer to
//               a Point Exchange purchase. The packet class compiles into the
//               packetwire library; the handler stays with the executable
//               like every other one.
//////////////////////////////////////////////////////////////////////////////

#include "Client_PCH.h"
#include "Gpackets/GCExchangeBuy.h"

#include "Player.h"
#include "CommunicationPresentationHost.h"
#include "DialogPresentationHost.h"
#include "VsUiPresentationHost.h"
#include "MGameStringTable.h"

void GCExchangeBuyHandler::execute(GCExchangeBuy* pPacket, Player* pPlayer)
{
	__BEGIN_TRY
	(void)pPlayer;

	// The message is the server's own text: "Success" on a purchase, the
	// reason on a refusal. Its BYTE length prefix bounds it at 255 bytes,
	// which the free message dialog's 256-byte rows hold whole.
	const std::string& message = pPacket->getMessage();
	if (!message.empty())
		DialogPresentation::PopupFreeMessageDlg(message.c_str());
	else if (!pPacket->isSuccess())
		CommunicationPresentation::PopupMessage(STRING_ERROR_ETC_ERROR);

	// A purchase takes the listing off the market, so the open window asks
	// for its page again rather than keep showing what was just bought.
	if (pPacket->isSuccess())
		VsUiPresentation::RefreshPointExchange();

	__END_CATCH
}
