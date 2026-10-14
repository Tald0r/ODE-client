//////////////////////////////////////////////////////////////////////
//
// Filename    : GCTradePrepareHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCTradePrepare.h"
#include "Cpackets/CGTradePrepare.h"
#include "ClientPlayer.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "MCreatureTable.h"
#include "MGameStringTable.h"
#include "PlayerStateHost.h"
#include "TempInformation.h"
#include "TradePresentationHost.h"
#include "ZoneStateHost.h"



void GCTradePrepareHandler::execute ( GCTradePrepare * pPacket , Player * pPlayer )
	 


{
	__BEGIN_TRY
	

	ObjectID_t targetID = pPacket->getTargetObjectID();
	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature==NULL
		|| g_pTempInformation==NULL)
	{
		DEBUG_ADD_WAR("GCTradePrepareHandler Failed");
		return;
	}

	ClientPlayer * pClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);
		
	//----------------------------------------------------------------------
	//
	//			Someone asks me to trade.
	//
	//----------------------------------------------------------------------
	// Already trading... etc.. busy with something.. --> refuse
	//----------------------------------------------------------------------
	if ((PlayerState::GetWaitVerify()!=PlayerState::WAIT_VERIFY_NULL && PlayerState::GetWaitVerify()!=PlayerState::WAIT_VERIFY_TRADE)			// waiting on a verification
		|| PlayerState::IsRepeatAction()		// repeating an action
		|| DialogPresentation::IsLockInput()		// talking to an NPC..
		|| CreatureState::GetCreatureType( pPlayerCreature )==CREATURETYPE_BAT	// a bat
		|| CreatureState::GetCreatureType( pPlayerCreature )==CREATURETYPE_WOLF	// a wolf
		)
	{
		// refuse the exchange
		CGTradePrepare _CGTradePrepare;
		_CGTradePrepare.setTargetObjectID( targetID );
		_CGTradePrepare.setCode( CG_TRADE_PREPARE_CODE_REJECT );

		pClientPlayer->sendPacket( &_CGTradePrepare );

		return;
	}


	// someone else already asked me to trade --> refuse [the server handles it]
	switch (pPacket->getCode())
	{
		//----------------------------------------------------------------------
		//
		// when someone else asks me to exchange
		//
		//----------------------------------------------------------------------
		case GC_TRADE_PREPARE_CODE_REQUEST :
		{
			bool bLookMe = false;

			//----------------------------------------------------------------------
			// I already asked someone to trade
			//----------------------------------------------------------------------
			if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_TRADE
				&& g_pTempInformation->GetMode()==TempInformation::MODE_TRADE_REQUEST)
			{
				int code;

				//----------------------------------------------------------------------
				// that one asks me to trade --> OK
				//----------------------------------------------------------------------
				if (g_pTempInformation->Value1==targetID)
				{
					code = CG_TRADE_PREPARE_CODE_ACCEPT;

					g_pTempInformation->SetMode(TempInformation::MODE_NULL);

					//-------------------------------------------
					// open the exchange window.
					//-------------------------------------------
					TradePresentation::RunExchange( targetID );

					bLookMe = true;
				}
				//----------------------------------------------------------------------
				// otherwise
				//----------------------------------------------------------------------
				else
				{
					code = CG_TRADE_PREPARE_CODE_BUSY;
				}

				CGTradePrepare _CGTradePrepare;
				_CGTradePrepare.setTargetObjectID( targetID );
				_CGTradePrepare.setCode( code );				

				pClientPlayer->sendPacket( &_CGTradePrepare );

			}
			//----------------------------------------------------------------------
			// Exchange? Y/N
			//----------------------------------------------------------------------
			else
			{
				//-------------------------------------------
				// Exchange?
				//-------------------------------------------
				TradePresentation::RunExchangeAsk( targetID );
				
				bLookMe = true;
			}

			//----------------------------------------------------------------------
			// the other character faces me
			//----------------------------------------------------------------------
			if (bLookMe)
			{
				MCreature* pCreature = ZoneState::Exists()? ZoneState::GetCreature( targetID ) : NULL;
				
				if (pCreature!=NULL)
				{
					CreatureState::SetDirectionToPosition( pCreature, CreatureState::GetX( pPlayerCreature ), CreatureState::GetY( pPlayerCreature ) );
				}
				else
				{
					DEBUG_ADD_FORMAT_ERR("[Error] No Such Creature. id=%d", targetID);
				}
			}
		}
		break;

		//----------------------------------------------------------------------
		//
		// the one who asked to exchange cancels it
		//
		//----------------------------------------------------------------------
		case GC_TRADE_PREPARE_CODE_CANCEL :

			if (g_pTempInformation->GetMode() == TempInformation::MODE_TRADE_REQUEST)
			{
				g_pTempInformation->SetMode(TempInformation::MODE_NULL);
			}		

			// I already pressed OK and the exchange window is open
			if (TradePresentation::IsRunningExchange())
			{
				TradePresentation::CloseExchange();
			}
			else
			{
				TradePresentation::CloseExchangeAsk();
			}
		break;

		//----------------------------------------------------------------------
		//
		// when my exchange request is accepted
		//
		//----------------------------------------------------------------------
		case GC_TRADE_PREPARE_CODE_ACCEPT :
			//----------------------------------------------------------------------
			// I already asked someone to trade
			//----------------------------------------------------------------------
			if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_TRADE
				&& g_pTempInformation->GetMode()==TempInformation::MODE_TRADE_REQUEST)
			{
				g_pTempInformation->SetMode(TempInformation::MODE_NULL);

				//-------------------------------------------
				// open the exchange window.
				//-------------------------------------------
				TradePresentation::RunExchange( targetID );
			}

		break;


		//----------------------------------------------------------------------
		//
		// when my exchange request is refused
		//
		//----------------------------------------------------------------------
		case GC_TRADE_PREPARE_CODE_REJECT :
		{
			if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_TRADE
				&& g_pTempInformation->GetMode()==TempInformation::MODE_TRADE_REQUEST)
			{			
				// drop the verification data
				PlayerState::SetWaitVerifyNULL();
				g_pTempInformation->SetMode(TempInformation::MODE_NULL);

				// close 'cancel the exchange?'.
				TradePresentation::CloseExchangeCancel();

				// the trade-refused dialog
				DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TRADE_REJECTED].GetString());
			}
		}
		break;

		//----------------------------------------------------------------------
		//
		// the one I asked is exchanging now (or doing something else..)
		//
		//----------------------------------------------------------------------
		case GC_TRADE_PREPARE_CODE_BUSY :
			if (PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_TRADE
				&& g_pTempInformation->GetMode()==TempInformation::MODE_TRADE_REQUEST)
			{			
				// drop the verification data
				PlayerState::SetWaitVerifyNULL();

				g_pTempInformation->SetMode(TempInformation::MODE_NULL);				

				TradePresentation::CloseExchangeAsk();
				TradePresentation::CloseExchangeCancel();

				// busy, so no exchange
				DialogPresentation::PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_TRADE_BUSY].GetString());
			}
		break;

	}


	__END_CATCH
}
