////////////////////////////////////////////////////////////////////////////////
// Filename    : GCGoodsListHandler.cpp
// Written By  : 김성민
// Description :
////////////////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCGoodsList.h"
#include "TradePresentationHost.h"

void GCGoodsListHandler::execute ( GCGoodsList * pPacket , Player * pPlayer )
	 

{
	__BEGIN_TRY
	(void)pPlayer;

	TradePresentation::RunItemShop( pPacket );			

	__END_CATCH
}
