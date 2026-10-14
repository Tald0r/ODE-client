//////////////////////////////////////////////////////////////////////
//
// Filename    : GCTradeVerifyHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCTradeVerify.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "PlayerStateHost.h"
#include "TradePresentationHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "TempInformation.h"
#include "MTradeManager.h"
#include "MMoneyManager.h"


	#include "ClientPlayer.h"
	#include "Cpackets/CGTradeAddItem.h"
	#include "Cpackets/CGTradeFinish.h"
	#include "Cpackets/CGTradeRemoveItem.h"


void GCTradeVerifyHandler::execute ( GCTradeVerify * pPacket , Player * pPlayer )
	 

{
	__BEGIN_TRY

	// The player; without one there is nothing to do.
	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		return;
	}
	

	//------------------------------------------------------------------------
	// No trade manager.
	//------------------------------------------------------------------------
	if (g_pTradeManager==NULL)
	{
		DEBUG_ADD_ERR( "[Error] TradeManager is NULL");
		
		return;
	}

	ClientPlayer * pClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);

	switch (pPacket->getCode())
	{
		//----------------------------------------------------------------------
		//
		// A GCTradeAddItem waiting to be verified: OK.
		//
		//----------------------------------------------------------------------
		case GC_TRADE_VERIFY_CODE_ADD_ITEM_OK :
			if (g_pTempInformation->GetMode()==TempInformation::MODE_TRADE_VERIFY_ADD_ITEM)
			{
				// The player's item check buffer should be verified too; skipped.
				PlayerState::ClearItemCheckBuffer();

				g_pTempInformation->SetMode(TempInformation::MODE_NULL);

				MItem* pItem	= (MItem*)g_pTempInformation->pValue;
				
				pItem->SetTrade();

				//-----------------------------------------------------------
				// Something changed: the OKs are cancelled.
				//-----------------------------------------------------------
				g_pTradeManager->RefuseOtherTrade();
				g_pTradeManager->RefuseMyTrade();
			}
		break;

		//----------------------------------------------------------------------
		//
		// A GCTradeAddItem waiting to be verified: failed.
		//
		//----------------------------------------------------------------------
		case GC_TRADE_VERIFY_CODE_ADD_ITEM_FAIL :
			if (g_pTempInformation->GetMode()==TempInformation::MODE_TRADE_VERIFY_ADD_ITEM)
			{
				PlayerState::ClearItemCheckBuffer();

				g_pTempInformation->SetMode(TempInformation::MODE_NULL);
			}
		break;

		//----------------------------------------------------------------------
		//
		//		Verifies an item added to the trade
		//
		//----------------------------------------------------------------------
		case GC_TRADE_VERIFY_CODE_ADD_ITEM_WHEN_ACCEPT :
			
			if (g_pTempInformation->GetMode()==TempInformation::MODE_TRADE_VERIFY_ADD_ITEM)
			{
				// The player's item check buffer should be verified too; skipped.
				PlayerState::ClearItemCheckBuffer();

				g_pTempInformation->SetMode(TempInformation::MODE_NULL);

				MItem* pItem	= (MItem*)g_pTempInformation->pValue;
				
				//-----------------------------------------------------------
				// The addition is confirmed.
				//-----------------------------------------------------------
				pItem->SetTrade();

				//-----------------------------------------------------------
				// Something changed: the OKs are cancelled.
				//-----------------------------------------------------------
				g_pTradeManager->RefuseOtherTrade();
				g_pTradeManager->RefuseMyTrade();				
			}
			else
			{
				DEBUG_ADD_ERR( "[Error] TempMode is Wrong..");
			}
		break;

		//----------------------------------------------------------------------
		//
		//			Verifies an item taken out of the trade
		//
		//----------------------------------------------------------------------
		case GC_TRADE_VERIFY_CODE_REMOVE_ITEM :
				
		//----------------------------------------------------------------------
		//
		//					mouse --> inventory OK
		//
		//----------------------------------------------------------------------
		case GC_TRADE_VERIFY_CODE_MOUSE_TO_INVENTORY_OK :	
		
		//----------------------------------------------------------------------
		//
		//					inventory --> mouse OK
		//
		//----------------------------------------------------------------------
		case GC_TRADE_VERIFY_CODE_INVENTORY_TO_MOUSE_OK :
		
			//----------------------------------------------------------------------
			// remove item from trade
			//----------------------------------------------------------------------
			if (g_pTempInformation->GetMode()==TempInformation::MODE_TRADE_VERIFY_REMOVE_ITEM)
			{
				// The player's item check buffer should be verified too; skipped.
				PlayerState::ClearItemCheckBuffer();

				g_pTempInformation->SetMode(TempInformation::MODE_NULL);

				MItem* pItem	= (MItem*)g_pTempInformation->pValue;
				
				//-----------------------------------------------------------
				// The item that was chosen --> cancelled.
				//-----------------------------------------------------------
				pItem->UnSetTrade();

				//-----------------------------------------------------------
				// Something changed: the OKs are cancelled.
				//-----------------------------------------------------------
				g_pTradeManager->RefuseOtherTrade();
				g_pTradeManager->RefuseMyTrade();				
			}
			//----------------------------------------------------------------------
			// Verifying inventory --> mouse.
			//----------------------------------------------------------------------
			else if (g_pTempInformation->GetMode()==TempInformation::MODE_TRADE_VERIFY_PICKUP_FROM_INVENTORY)
			{
				// The player's item check buffer should be verified too; skipped.
				PlayerState::ClearItemCheckBuffer();

				g_pTempInformation->SetMode(TempInformation::MODE_NULL);

				MItem* pItem	= (MItem*)g_pTempInformation->pValue;
				int gridX		= static_cast<int>(g_pTempInformation->Value2);
				int gridY		= static_cast<int>(g_pTempInformation->Value3);
			
				DialogPresentation::PickUpItem( pItem );
				g_pTradeManager->GetMyInventory()->RemoveItem( gridX, gridY );

				//----------------------------------------------------------------
				// Cancel the OKs.
				//----------------------------------------------------------------				
				g_pTradeManager->RefuseMyTrade();
				g_pTradeManager->RefuseOtherTrade();

				//----------------------------------------------------------------
				// An item chosen for the trade cancels the OK by itself,
				// any other item must send a packet cancelling the OK.
				//----------------------------------------------------------------
				if (!pItem->IsTrade())
				{
					CGTradeFinish _CGTradeFinish;
					_CGTradeFinish.setTargetObjectID( g_pTradeManager->GetOtherID() );
					_CGTradeFinish.setCode( CG_TRADE_FINISH_RECONSIDER );

					pClientPlayer->sendPacket( &_CGTradeFinish );

				}			
			}
			//----------------------------------------------------------------
			// Just putting an item down.
			//----------------------------------------------------------------
			else if (g_pTempInformation->GetMode()==TempInformation::MODE_TRADE_VERIFY_DROP_TO_INVENTORY)
			{
				// The player's item check buffer should be verified too; skipped.
				PlayerState::ClearItemCheckBuffer();

				g_pTempInformation->SetMode(TempInformation::MODE_NULL);

				// pItem is the item that was held.
				int gridX		= static_cast<int>(g_pTempInformation->Value2);
				int gridY		= static_cast<int>(g_pTempInformation->Value3);
				MItem* pMouseItem = DialogPresentation::GetMouseItem();

				// pItem == pMouseItem.
				//----------------------------------------------------------------
				// With an item on the mouse.
				//----------------------------------------------------------------
				if (pMouseItem!=NULL)
				{			
					MItem* pOldItem = NULL;

					//----------------------------------------------------------------
					// Add it to the inventory.
					//----------------------------------------------------------------
					if (g_pTradeManager->GetMyInventory()->ReplaceItem(
															pMouseItem,		// the item to add
															gridX, gridY,	// where to add it 
															pOldItem))								// the item that was there
					{
						if (pOldItem != NULL) // replaced?
						{
							DialogPresentation::PickUpItem( pOldItem );						
						}
						else
						{
							DialogPresentation::DropItem();
						}
					}

					//---------------------------------------------------
					// The item put down was chosen for the trade.
					//---------------------------------------------------
					if (pMouseItem->IsTrade())
					{
						CGTradeAddItem _CGTradeAddItem;
						_CGTradeAddItem.setTargetObjectID( g_pTradeManager->GetOtherID() );
						_CGTradeAddItem.setItemObjectID( pMouseItem->GetID() );

						pClientPlayer->sendPacket( &_CGTradeAddItem );

					}
				}
				else
				{
					// (a mess)
				}

				//----------------------------------------------------------------
				// Cancel the OKs.
				//----------------------------------------------------------------
				g_pTradeManager->RefuseMyTrade();
				g_pTradeManager->RefuseOtherTrade();

				//----------------------------------------------------------------
				// An item chosen for the trade cancels the OK by itself,
				// any other item must send a packet cancelling the OK.
				//----------------------------------------------------------------
				if (pMouseItem!=NULL && !pMouseItem->IsTrade())
				{
					CGTradeFinish _CGTradeFinish;
					_CGTradeFinish.setTargetObjectID( g_pTradeManager->GetOtherID() );
					_CGTradeFinish.setCode( CG_TRADE_FINISH_RECONSIDER );

					pClientPlayer->sendPacket( &_CGTradeFinish );

				}
			}
			//----------------------------------------------------------------
			// Added onto an item.
			//----------------------------------------------------------------
			else if (g_pTempInformation->GetMode()==TempInformation::MODE_TRADE_VERIFY_INSERT_TO_INVENTORY)				
			{
				// The player's item check buffer should be verified too; skipped.
				PlayerState::ClearItemCheckBuffer();

				g_pTempInformation->SetMode(TempInformation::MODE_NULL);

				MItem* pItem	= (MItem*)g_pTempInformation->pValue;
				MItem* pMouseItem = DialogPresentation::GetMouseItem();

				//----------------------------------------------------
				// Add pMouseItem onto pItem.
				//----------------------------------------------------
				int total = pMouseItem->GetNumber() + pItem->GetNumber();
				if ( static_cast<TYPE_ITEM_NUMBER>(total) > pItem->GetMaxNumber() )
				{
					// Past the stack's limit.
					pMouseItem->SetNumber( total - pItem->GetMaxNumber() );
					pItem->SetNumber( pItem->GetMaxNumber() );
				}
				else
				{
					// All of it fits on pItem.
					pItem->SetNumber( total );
					DialogPresentation::DropItem();

					delete pMouseItem;
				}

				//----------------------------------------------------
				// An item chosen for the trade.
				//----------------------------------------------------
				// Remove and Add - -;
				if (pItem->IsTrade())
				{
						//----------------------------------------------------
						// Its number changed, so remove it...
						//----------------------------------------------------
						CGTradeRemoveItem _CGTradeRemoveItem;
						_CGTradeRemoveItem.setTargetObjectID( g_pTradeManager->GetOtherID() );
						_CGTradeRemoveItem.setItemObjectID( pItem->GetID() );

						pClientPlayer->sendPacket( &_CGTradeRemoveItem );

						#ifdef __DEBUG_OUTPUT__
							DEBUG_ADD_FORMAT( "[Send] %s", _CGTradeRemoveItem.toString().c_str() );							
						#endif
						
						//----------------------------------------------------
						// ...and add it again.
						//----------------------------------------------------
						CGTradeAddItem _CGTradeAddItem;
						_CGTradeAddItem.setTargetObjectID( g_pTradeManager->GetOtherID() );
						_CGTradeAddItem.setItemObjectID( pItem->GetID() );

						pClientPlayer->sendPacket( &_CGTradeAddItem );
						
				}
				else
				{
					//----------------------------------------------------------------
					// An item chosen for the trade cancels the OK by itself,
					// any other item must send a packet cancelling the OK.
					//----------------------------------------------------------------					
					CGTradeFinish _CGTradeFinish;
					_CGTradeFinish.setTargetObjectID( g_pTradeManager->GetOtherID() );
					_CGTradeFinish.setCode( CG_TRADE_FINISH_RECONSIDER );

					pClientPlayer->sendPacket( &_CGTradeFinish );

				}

				WorldEvents::PlaySound( pItem->GetInventorySoundID() );
			}
			//----------------------------------------------------------------
			// The wrong mode.
			//----------------------------------------------------------------
			else
			{
				DEBUG_ADD_ERR( "[Error] TempMode is Wrong..");
			}
		break;


		//----------------------------------------------------------------------
		//
		//			Verifies money added to the trade
		//
		//----------------------------------------------------------------------
		case GC_TRADE_VERIFY_CODE_MONEY_INCREASE :

			if (g_pTempInformation->GetMode()==TempInformation::MODE_TRADE_VERIFY_ADD_MONEY)
			{
				g_pTempInformation->SetMode(TempInformation::MODE_NULL);

			
				//if (!g_pMoneyManager->GetOtherMoneyManager->CanAddMoney(money))
				//{
				//	money = g_pMoneyManager->GetOtherMoneyManager->GetMaxAddMoney();
				//}

				//g_pMoneyManager->UseMoney( money );
				//g_pTradeManager->GetMyMoneyManager()->AddMoney( money );

				//-----------------------------------------------------------
				// Something changed: the OKs are cancelled.
				//-----------------------------------------------------------
				g_pTradeManager->RefuseOtherTrade();
				g_pTradeManager->RefuseMyTrade();
			}
			else
			{
				DEBUG_ADD_ERR( "[Error] TempMode is Wrong..");
			}
		break;

		//----------------------------------------------------------------------
		//
		//			Verifies money taken out of the trade
		//
		//----------------------------------------------------------------------
		case GC_TRADE_VERIFY_CODE_MONEY_DECREASE:

			if (g_pTempInformation->GetMode()==TempInformation::MODE_TRADE_VERIFY_REMOVE_MONEY)
			{
				g_pTempInformation->SetMode(TempInformation::MODE_NULL);

				
				//g_pTradeManager->GetMyMoneyManager()->UseMoney( money );
				//g_pMoneyManager->AddMoney( money );	

				//-----------------------------------------------------------
				// Something changed: the OKs are cancelled.
				//-----------------------------------------------------------
				g_pTradeManager->RefuseOtherTrade();
				g_pTradeManager->RefuseMyTrade();				
			}
			else
			{
				DEBUG_ADD_ERR( "[Error] TempMode is Wrong..");
			}
		break;

		//----------------------------------------------------------------------
		// Verifies the trade being agreed.
		//----------------------------------------------------------------------
		case GC_TRADE_VERIFY_CODE_FINISH_ACCEPT:
			// Needs no verification.
		break;

		//----------------------------------------------------------------------
		//
		//				Verifies the trade being cancelled
		//
		//----------------------------------------------------------------------
		case GC_TRADE_VERIFY_CODE_FINISH_REJECT:

			if (g_pTempInformation->GetMode()==TempInformation::MODE_TRADE_VERIFY_CLOSE)
			{
				g_pTempInformation->SetMode(TempInformation::MODE_NULL);

				//----------------------------------------------------------------
				// Close the exchange window.
				//----------------------------------------------------------------			
				if (g_pTradeManager!=NULL)
				{
					g_pTradeManager->CancelTrade();
				}

				TradePresentation::CloseExchange();
			}
		break;

		//----------------------------------------------------------------------
		//
		//				Verifies the trade being reconsidered
		//
		//----------------------------------------------------------------------
		case GC_TRADE_VERIFY_CODE_FINISH_RECONSIDER:

			if (g_pTempInformation->GetMode()==TempInformation::MODE_TRADE_VERIFY_CANCEL)
			{
				g_pTempInformation->SetMode(TempInformation::MODE_NULL);

				//----------------------------------------------------------------
				// Cancel the OKs.
				//----------------------------------------------------------------			
				g_pTradeManager->RefuseMyTrade();
			}
		break;

		//----------------------------------------------------------------------
		//
		//					mouse --> inventory Fail
		//
		//----------------------------------------------------------------------
		case GC_TRADE_VERIFY_CODE_MOUSE_TO_INVENTORY_FAIL:
			if (g_pTempInformation->GetMode()==TempInformation::MODE_TRADE_VERIFY_DROP_TO_INVENTORY)
			{
				// The player's item check buffer should be verified too; skipped.
				PlayerState::ClearItemCheckBuffer();

				g_pTempInformation->SetMode(TempInformation::MODE_NULL);
			}
			else
			{
				DEBUG_ADD_ERR( "[Error] TempMode is Not MODE_TRADE_VERIFY_DROP_TO_INVENTORY");
			}
		break;


		//----------------------------------------------------------------------
		//
		//					inventory --> mouse Fail
		//
		//----------------------------------------------------------------------
		case GC_TRADE_VERIFY_CODE_INVENTORY_TO_MOUSE_FAIL:
			if (g_pTempInformation->GetMode()==TempInformation::MODE_TRADE_VERIFY_PICKUP_FROM_INVENTORY)
			{
				// The player's item check buffer should be verified too; skipped.
				PlayerState::ClearItemCheckBuffer();

				g_pTempInformation->SetMode(TempInformation::MODE_NULL);
			}
			else
			{
				DEBUG_ADD_ERR( "[Error] TempMode is Not MODE_TRADE_VERIFY_PICKUP_FROM_INVENTORY");
			}
		break;
	}



	__END_CATCH
}
