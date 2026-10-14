//////////////////////////////////////////////////////////////////////
//
// Filename    : GCShopBuyOKHandler.cpp
// Written By  : 김성민
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCShopBuyOK.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "ItemSkillIcons.h"
#include "MShop.h"
#include "MShopShelf.h"
#include "MInventory.h"
#include "TempInformation.h"
#include "MMoneyManager.h"
#include "MPriceManager.h"
#include "MItemFinder.h"

#define SAFE_DELETE(x)		{ if(x!=NULL) delete x; x=NULL; }

// (formerly declared here to save compile time)

void GCShopBuyOKHandler::execute ( GCShopBuyOK * pPacket , Player * pPlayer )
	 

{
	__BEGIN_TRY
	(void)pPlayer;
	

	//--------------------------------------------------------------
	// Is a purchase what we were waiting for?
	//--------------------------------------------------------------
	if (g_pTempInformation->GetMode() == TempInformation::MODE_SHOP_BUY)
	{
		MShop* pShop = (MShop*)g_pTempInformation->pValue;		
		int ShelfType	= static_cast<int>(g_pTempInformation->Value1);
		int index		= static_cast<int>(g_pTempInformation->Value2);
		int x			= static_cast<int>(g_pTempInformation->Value3);
		int y			= static_cast<int>(g_pTempInformation->Value4);

		DEBUG_ADD_FORMAT("[BuyOK] TempInfo. ShelfType=%d, index=%d, xy=(%d, %d)", ShelfType, index, x, y);
		
		
		MShopShelf* pShopShelf = pShop->GetShelf( ShelfType );

		//--------------------------------------------------------------
		// Take the item out of the shop.			
		//--------------------------------------------------------------
		if (pShopShelf!=NULL)
		{
			//------------------------------------------------------
			// Only a shelf other than the normal one deletes it.
			//------------------------------------------------------
			if (ShelfType==SHOP_RACK_SPECIAL)
			{
				pShopShelf->DeleteItem( index );
			}

			// The shop's version changes.
			pShopShelf->SetVersion( pPacket->getShopVersion() );
		}
		else
		{
			DEBUG_ADD_FORMAT_ERR("[Error] No Shelf.. type=%d", ShelfType);
		}

		//--------------------------------------------------------------
		// Make the item just bought.
		//--------------------------------------------------------------		
		MItem* pItem = MItem::NewItem( (ITEM_CLASS)pPacket->getItemClass() );

		if (pItem == NULL)
		{
			DEBUG_ADD_FORMAT_ERR("[Error] GCShopBuyOK: invalid item class %d", pPacket->getItemClass());
			return;
		}

		pItem->SetID( pPacket->getItemObjectID() );
		pItem->SetItemType( pPacket->getItemType() );
		pItem->SetItemOptionList( pPacket->getOptionType() );
		pItem->SetNumber( pPacket->getItemNum() );	// the number already stacked
		pItem->SetCurrentDurability( pPacket->getDurability() );
		pItem->SetSilver( pPacket->getSilver() );
		pItem->SetGrade( pPacket->getGrade() );
		pItem->SetEnchantLevel( pPacket->getEnchantLevel() );

		const MItem* pOldItem = g_pInventory->GetItem( x, y );

		//--------------------------------------------------------------		
		// Nothing in that cell --> just add it.
		//--------------------------------------------------------------				
		if (pOldItem==NULL)
		{				
		}
		//--------------------------------------------------------------		
		// Check whether it can stack.
		//--------------------------------------------------------------				
		else
		{
			if (pItem->IsInsertToItem( pOldItem ))
			{
				int total = //pOldItem->GetNumber() + 
							pItem->GetNumber();
				
				//------------------------------------------------
				// Too many.					
				//------------------------------------------------
				if ( static_cast<TYPE_ITEM_NUMBER>(total) > pItem->GetMaxNumber() )
				{
					DEBUG_ADD_FORMAT_ERR("[Error] Cannot Add. MaxNum exceed=%d", total);
				}
				//------------------------------------------------
				// The normal case: it stacks.					
				//------------------------------------------------
				else
				{
					//pItem->SetNumber( total );

					// Remove the item that was there.
					MItem* pRemovedItem = g_pInventory->RemoveItem( x, y );
					
					delete pRemovedItem; pRemovedItem = NULL;

//					if (pRemovedItem!=NULL)
//					{
//						delete pRemovedItem;
//					}
				}
			}
			else
			{
				DEBUG_ADD_ERR("[Error] Cannot Add. No match item");
			}
		}

		// A star shop's price, taken now: the item is freed below when it
		// does not fit, and the server has charged for it either way.
		STAR_ITEM_PRICE starPrice;
		g_pPriceManager->GetItemPrice( pItem, starPrice );

		//--------------------------------------------------------------
		// Add the item to the inventory.
		//--------------------------------------------------------------
		if (g_pInventory->AddItem( pItem, x, y ))
		{
			// Added --> play its sound.
			WorldEvents::PlaySound( pItem->GetTileSoundID() );

			// Check the skill icons.
			CheckItemForSkillIcon( pItem );
		}
		else
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Cannot Add to Inventory(%d,%d)", x,y);
			
			// Not added: free it.
			delete pItem;
		}
	
		//--------------------------------------------------------------
		// What it was bought with depends on the shop.
		//--------------------------------------------------------------
		switch (pShop->GetShopType())
		{
			//--------------------------------------------------------------
			// Money.
			//--------------------------------------------------------------
			case MShop::SHOP_NORMAL :
				//--------------------------------------------------------------
				// Change the money.
				//--------------------------------------------------------------
				if (!g_pMoneyManager->SetMoney( pPacket->getPrice() ))
				{
					DEBUG_ADD_FORMAT_ERR("[Error] Can't Set Money=%d, Price=%d", g_pMoneyManager->GetMoney(), pPacket->getPrice());
				}
			break;

			//--------------------------------------------------------------
			// Stars.
			//--------------------------------------------------------------
			case MShop::SHOP_EVENT_STAR :
				// Remove that many from the inventory.
				{
					int remainNum = starPrice.number;

					if (starPrice.type!=-1 && starPrice.number!=0)
					{
						// Find how many there are.
						MItemClassTypeFinder starFinder(ITEM_CLASS_EVENT_STAR, starPrice.type);

						// Remove that many from the inventory.
						while (remainNum > 0)
						{
							MItem* pStarItem = g_pInventory->FindItemGridOrder( starFinder );

							// No stars.
							if (pStarItem==NULL)
							{
								DEBUG_ADD_ERR("[Error] Not Enough Star -_-");
								break;
							}

							int itemNum = pStarItem->GetNumber();

							if (itemNum > remainNum)
							{
								pStarItem->SetNumber( itemNum - remainNum );
								remainNum = 0;
							}
							else
							{
								// The same or fewer.
								remainNum -= itemNum;

								MItem* pRemovedItem = g_pInventory->RemoveItem( 
																pStarItem->GetGridX(), 
																pStarItem->GetGridY() );

								if (pRemovedItem!=NULL)
								{
									delete pRemovedItem;
								}
								else
								{
									DEBUG_ADD_FORMAT_ERR("[Error] Can't remove star(%d, %d)", pStarItem->GetGridX(), pStarItem->GetGridY());
								}
							}
						}
					}
				}
			break;
		}

		
		// Clear the mode.
		g_pTempInformation->SetMode(TempInformation::MODE_NULL);

		// Trading is allowed again.
		DialogPresentation::UnlockItemTrade();

		//--------------------------------------------------------------
		// A motorcycle (key) was bought.
		//--------------------------------------------------------------
//		__BEGIN_HELP_EVENT
//			if (pItem->GetItemClass()==ITEM_CLASS_KEY 
//				&& pItem->GetItemType()==2)
//			{
//				// [Help] when a motorcycle is bought
////				ExecuteHelpEvent( HE_ITEM_BUY_MOTORCYCLE );
//			}
//		__END_HELP_EVENT
	}
	else
	{
		DEBUG_ADD_ERR("[Error] Buy? -_-; No Temp Information!");
	}


	__END_CATCH
}
