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
// Moves money from one wallet to another, all or nothing. The server
// refuses a move its source cannot cover and trims one its destination
// cannot hold (decideMoneyIncrease and decideMoneyDecrease in its
// trade/TradeTableDecision.cpp), so it never moves part of one. When this
// client's wallets cannot make the move the server reports, they disagree
// with the server's; moving one side only would create or lose money on
// screen, so both are left as they were.
//----------------------------------------------------------------------------
void MoveMoney( MMoneyManager* pFrom, MMoneyManager* pTo, int money )
{
	if (!pFrom->CanUseMoney( money ) || !pTo->CanAddMoney( money ))
	{
		DEBUG_ADD_FORMAT( "[Error] GCTradeMoney: the wallets cannot move %d (%d -> %d)", money, pFrom->GetMoney(), pTo->GetMoney() );
		return;
	}

	pFrom->UseMoney( money );
	pTo->AddMoney( money );
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
			MoveMoney( g_pMoneyManager, g_pTradeManager->GetMyMoneyManager(), money );
			
			bRefuseAccept = true;
		break;

		//---------------------------------------------------------------
		// [Result] The server moved this much from the trade box back
		// into the wallet.
		//---------------------------------------------------------------
		case GC_TRADE_MONEY_DECREASE_RESULT :			
			MoveMoney( g_pTradeManager->GetMyMoneyManager(), g_pMoneyManager, money );

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
