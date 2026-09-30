//////////////////////////////////////////////////////////////////////
//
// Filename    : GCTradeMoneyHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCTradeMoney.h"
#include "DebugLog.h"
#include "MTradeManager.h"

#include <climits>

namespace {

//----------------------------------------------------------------------------
// Applies a result the server has already committed. The server rejects a
// move before it changes anything (decideMoneyIncrease and
// decideMoneyDecrease in its trade/TradeTableDecision.cpp); otherwise it
// sets both its wallet and its stake, and only then reports the amount. So
// each side here follows the server on its own. A side that cannot follow
// (its balance would leave 0..limit) disagrees with the server's already;
// it keeps its value and the refusal is logged, and the other side still
// follows, since holding it back would only make that side wrong too.
//----------------------------------------------------------------------------
void ApplyResult( MMoneyManager* pFrom, MMoneyManager* pTo, int money )
{
	if (!pFrom->UseMoney( money ))
	{
		DEBUG_ADD_FORMAT( "[Error] GCTradeMoney: the source cannot give %d (has %d)", money, pFrom->GetMoney() );
	}

	if (!pTo->AddMoney( money ))
	{
		DEBUG_ADD_FORMAT( "[Error] GCTradeMoney: the destination cannot take %d (has %d)", money, pTo->GetMoney() );
	}
}

} // namespace

void GCTradeMoneyHandler::execute ( GCTradeMoney * pPacket , Player * pPlayer )
	 

{
	__BEGIN_TRY
	(void)pPlayer;
	
	
	//------------------------------------------------------------------------
	// No trade is open: nothing to change.
	//------------------------------------------------------------------------
	if (g_pTradeManager==NULL)
	{
		DEBUG_ADD( "[Error] TradeManager is NULL");
		
		return;
	}

	//------------------------------------------------------------------------
	// Gold_t is unsigned and no wallet holds more than two billion (the
	// server's MAX_MONEY), so an amount past INT_MAX is not one the server
	// moved. Narrowed to int it would turn negative and run every move
	// below backwards; such a packet changes nothing.
	//------------------------------------------------------------------------
	if (pPacket->getAmount() > (Gold_t)INT_MAX)
	{
		DEBUG_ADD_FORMAT( "[PacketError-GCTradeMoneyHandler] amount out of range: %u", (unsigned int)pPacket->getAmount() );
		return;
	}

	const int money = (int)pPacket->getAmount();

	bool bRefuseAccept = false;
	bool bNextAcceptTime = false;
	
	switch (pPacket->getCode())
	{
		//---------------------------------------------------------------
		// The other side added money to its offer.
		//---------------------------------------------------------------
		case GC_TRADE_MONEY_INCREASE :
			g_pTradeManager->GetOtherMoneyManager()->AddMoney( money );

			bRefuseAccept = true;
		break;

		//---------------------------------------------------------------
		// The other side took money back from its offer.
		//---------------------------------------------------------------
		case GC_TRADE_MONEY_DECREASE :
			g_pTradeManager->GetOtherMoneyManager()->UseMoney( money );

			bRefuseAccept = true;
			bNextAcceptTime = true;
		break;
		
		//---------------------------------------------------------------
		// [Result] The server moved this much of the player's money from
		// the wallet into the trade box.
		//---------------------------------------------------------------
		case GC_TRADE_MONEY_INCREASE_RESULT :			
			ApplyResult( g_pMoneyManager, g_pTradeManager->GetMyMoneyManager(), money );
			
			bRefuseAccept = true;
		break;

		//---------------------------------------------------------------
		// [Result] The server moved this much from the trade box back
		// into the wallet.
		//---------------------------------------------------------------
		case GC_TRADE_MONEY_DECREASE_RESULT :			
			ApplyResult( g_pTradeManager->GetMyMoneyManager(), g_pMoneyManager, money );

			bRefuseAccept = true;
		break;
	}

	//-----------------------------------------------------------
	// The trade changed: both OKs are cancelled.
	//-----------------------------------------------------------
	if (bRefuseAccept)
	{
		g_pTradeManager->RefuseOtherTrade();
		g_pTradeManager->RefuseMyTrade();

		if(bNextAcceptTime)
			g_pTradeManager->SetNextAcceptTime();
	}
	


	__END_CATCH
}
