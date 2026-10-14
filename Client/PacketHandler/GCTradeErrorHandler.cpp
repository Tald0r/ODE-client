//////////////////////////////////////////////////////////////////////
//
// Filename    : GCTradeErrorHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCTradeError.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "PlayerStateHost.h"
#include "TradePresentationHost.h"
#include "ZoneStateHost.h"
#include "MGameStringTable.h"
#include "TempInformation.h"
#include "MTradeManager.h"

void GCTradeErrorHandler::execute ( GCTradeError * pPacket , Player * pPlayer )
	 

{
	__BEGIN_TRY
	(void)pPlayer;

	// The player; without one there is nothing to do.
	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		return;
	}

	if (g_pTempInformation==NULL
		|| g_pGameStringTable==NULL)
	{
		return;
	}


	int code = pPacket->getCode();

	//----------------------------------------------------------------------
	//
	//	What every cancelled exchange does
	//
	//----------------------------------------------------------------------
	if (code==GC_TRADE_ERROR_CODE_TARGET_NOT_EXIST
		|| code==GC_TRADE_ERROR_CODE_RACE_DIFFER
		|| code==GC_TRADE_ERROR_CODE_NOT_SAFE)
	{	
		if (g_pTempInformation->GetMode() == TempInformation::MODE_TRADE_REQUEST)
		{
			// Close the "cancel the exchange?" box.
			TradePresentation::CloseExchangeCancel();

			// No longer waiting.
			if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_TRADE)
			{
				PlayerState::SetWaitVerifyNULL();
			}

			g_pTempInformation->SetMode(TempInformation::MODE_NULL);
		}
	}

	switch (code)
	{
		//----------------------------------------------------------------------
		// The one asked to trade is not there.
		//----------------------------------------------------------------------
		case GC_TRADE_ERROR_CODE_TARGET_NOT_EXIST :						
			// Nobody by that name.
			TradePresentation::CloseExchangeCancel();

			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TRADE_NOBODY].GetString());
		break;
		
		
		//----------------------------------------------------------------------
		// The one asked to trade is another race.
		//----------------------------------------------------------------------
		case GC_TRADE_ERROR_CODE_RACE_DIFFER :
			// Cannot trade.
			TradePresentation::CloseExchangeCancel();

			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_TRADE].GetString());
		break;

		//----------------------------------------------------------------------
		// Trading outside a safety zone.
		//----------------------------------------------------------------------
		case GC_TRADE_ERROR_CODE_NOT_SAFE :
			// Cannot trade.
			TradePresentation::CloseExchangeCancel();

			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TRADE_SAFETY_ZONE_ONLY].GetString());			
		break;

		//----------------------------------------------------------------------
		// Trading while riding a motorcycle.
		//----------------------------------------------------------------------
		case GC_TRADE_ERROR_CODE_MOTORCYCLE :
			// Cannot trade.
			TradePresentation::CloseExchangeCancel();

			if( CreatureState::IsSlayer(pPlayerCreature) )
				DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TRADE_CANNOT_ON_MOTORCYCLE].GetString());
			else if ( CreatureState::IsOusters(pPlayerCreature) )
				DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_TRADE_SUMMON_SYLPH].GetString() );
		break;
		

		//----------------------------------------------------------------------
		// Asked to trade while already trading.
		//----------------------------------------------------------------------
		case GC_TRADE_ERROR_CODE_ALREADY_TRADING :
			
			TradePresentation::CloseExchangeCancel();

			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_TRADE_ALREADY_TRADING].GetString());
		break;

		//----------------------------------------------------------------------
		// A trade packet while not trading.
		//----------------------------------------------------------------------
		case GC_TRADE_ERROR_CODE_NOT_TRADING :
			// - -;
			#ifdef OUTPUT_DEBUG
				{
					DEBUG_ADD_ERR("[Error] Server is Not Trade Mode");
				}
			#endif
		break;

		//----------------------------------------------------------------------
		// The item to add to the trade is not there.
		//----------------------------------------------------------------------
		case GC_TRADE_ERROR_CODE_ADD_ITEM :
			#ifdef OUTPUT_DEBUG
				{
					DEBUG_ADD_ERR("[Error] There is no such item to Add");
				}
			#endif
		break;

		//----------------------------------------------------------------------
		// The item to take out of the trade is not there.
		//----------------------------------------------------------------------
		case GC_TRADE_ERROR_CODE_REMOVE_ITEM :
			#ifdef OUTPUT_DEBUG
				{
					DEBUG_ADD_ERR("[Error] There is no such item to Remove");
				}
			#endif
		break;

		//----------------------------------------------------------------------
		// Not enough money to add to the trade.
		//----------------------------------------------------------------------
		case GC_TRADE_ERROR_CODE_INCREASE_MONEY :
			#ifdef OUTPUT_DEBUG
				{
					DEBUG_ADD_ERR("[Error] GC_TRADE_ERROR_CODE_INCREASE_MONEY");
				}
			#endif
		break;

		//----------------------------------------------------------------------
		// Not enough money to take out of the trade.
		//----------------------------------------------------------------------
		case GC_TRADE_ERROR_CODE_DECREASE_MONEY :
			#ifdef OUTPUT_DEBUG
				{
					DEBUG_ADD_ERR("[Error] GC_TRADE_ERROR_CODE_DECREASE_MONEY");
				}
			#endif
		break;

		//----------------------------------------------------------------------
		// The trade failed: not enough room.
		//----------------------------------------------------------------------
		case GC_TRADE_ERROR_CODE_NOT_ENOUGH_SPACE :
			
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_TRADE_NO_SPACE].GetString());

			#ifdef OUTPUT_DEBUG			
				{
					DEBUG_ADD("Trade refused: insufficient inventory space");
				}
			#endif
		break;

		//----------------------------------------------------------------------
		// An unknown error.
		//----------------------------------------------------------------------
		case GC_TRADE_ERROR_CODE_UNKNOWN :
			
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_TRADE].GetString());

			#ifdef OUTPUT_DEBUG			
				{
					DEBUG_ADD_ERR("[Error] GC_TRADE_ERROR_CODE_UNKNOWN");
				}
			#endif
		break;
		
		case GC_TRADE_ERROR_CODE_EVENT_GIFT_BOX:
			DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_CANNOT_EVENT_GIFT_BOX].GetString());
			break;
	}

	//----------------------------------------------------------------------
	// A trade error always closes the exchange window.
	//----------------------------------------------------------------------
	if (g_pTradeManager!=NULL)
	{
		g_pTradeManager->CancelTrade();
	}

	TradePresentation::CloseExchange();


	__END_CATCH
}
