////////////////////////////////////////////////////////////////////////////////
// Filename    : GCStashListHandler.cpp
// Written By  : 김성민
// Description :
////////////////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCStashList.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "MGameStringTable.h"
#include "MItem.h"
#include "MStorage.h"

void GCStashListHandler::execute ( GCStashList * pPacket , Player * pPlayer )
	 

{
	__BEGIN_TRY
	(void)pPlayer;
	

	int stashNum = pPacket->getStashNum();

	//------------------------------------------------------------
	// Is there a stash?
	//------------------------------------------------------------
	if (stashNum>0)
	{
		//------------------------------------------------------------
		// The old stash goes, and a new one is made.
		//------------------------------------------------------------
		if (g_pStorage!=NULL)
		{
			delete g_pStorage;
		}

		g_pStorage = new MStorage;

		
		g_pStorage->Init( stashNum );

		for (int rack=0; rack<stashNum; rack++)
		{
			//------------------------------------------------------------
			// The rack each item goes on.
			//------------------------------------------------------------
			g_pStorage->SetCurrent( rack );

			for (int index=0; index<STASH_INDEX_MAX; index++)
			{
				if (pPacket->isExist(rack, index))
				{
					const STASHITEM&	item = pPacket->getStashItem(rack, index);

					//------------------------------------------------------------
					// create the item.
					//------------------------------------------------------------
					MItem* pItem = MItem::NewItem( (ITEM_CLASS)item.itemClass );

					if (pItem == NULL)
					{
						DEBUG_ADD_FORMAT_ERR("[Error] GCStashList: invalid item class %d", (int)item.itemClass);
						continue;
					}

					pItem->SetID( item.objectID );

					pItem->SetItemType( item.itemType );
					pItem->SetItemOptionList( item.optionType );
					pItem->SetCurrentDurability( item.durability );

					//------------------------------------------
					// The count
					//------------------------------------------
					// A gun
					//------------------------------------------
					if (pItem->IsGunItem())
					{
						// A fresh magazine of the gun's type holding the stashed count;
						// without one the gun is stashed unloaded.
						if (!MItem::LoadGun( pItem, item.num ))
						{
							DEBUG_ADD_ERR("[Error] GCStashList: no magazine for the gun");
						}
					}		
					//------------------------------------------
					// Anything else
					//------------------------------------------
					else
					{
						pItem->SetNumber( item.num );
						pItem->SetSilver( item.silver );
					}

					pItem->SetGrade( item.grade );
					pItem->SetEnchantLevel( item.enchantLevel );

					//------------------------------------------------------------
					// Make the pockets' items.
					//------------------------------------------------------------
					int subNum = pPacket->getSubItemCount(rack, index);
					if (subNum!=0)
					{
						//------------------------------------------------------------
						// A belt
						//------------------------------------------------------------
						if (pItem->GetItemClass()==ITEM_CLASS_BELT)
						{
							MBelt* pBelt = (MBelt*)pItem;

							std::list<SubItemInfo*>& listSubItem = pPacket->getSubItems(rack, index);

							std::list<SubItemInfo*>::const_iterator iItem = listSubItem.begin();

							//------------------------------------------------------------
							// Set up each pocket's item.
							//------------------------------------------------------------
							while (iItem != listSubItem.end())
							{
								SubItemInfo* pItemInfo = *iItem;

								if (pItemInfo!=NULL)
								{
									//------------------------------------------------------------
									// create the sub item.
									//------------------------------------------------------------
									MItem* pSubItem = MItem::NewItem( (ITEM_CLASS)pItemInfo->getItemClass() );

									if (pSubItem == NULL)
									{
										DEBUG_ADD_FORMAT_ERR("[Error] GCStashList: invalid sub item class %d", pItemInfo->getItemClass());
										iItem++;
										continue;
									}

									pSubItem->SetID( pItemInfo->getObjectID() );

									pSubItem->SetItemType( pItemInfo->getItemType() );
									//pSubItem->SetItemOption( 0 );
									//pSubItem->SetItemDurability( item.durability );
									pSubItem->SetNumber( pItemInfo->getItemNum() );

									if (!pBelt->AddItem( pSubItem, pItemInfo->getSlotID() ))
									{
										delete pSubItem;

										// pItemInfo carries the same class; pSubItem is already freed here.
										DEBUG_ADD_FORMAT_ERR("[Error] Can't Add Item to Belt. rack=%d, slot=%d, class=%d, belt-slot=%d", rack, index, (int)pItemInfo->getItemClass(), (int)pItemInfo->getSlotID());
									}
								}

								iItem++;
							}
						} else if (pItem->GetItemClass()==ITEM_CLASS_OUSTERS_ARMSBAND)
						{
							MOustersArmsBand* pBelt = (MOustersArmsBand*)pItem;

							std::list<SubItemInfo*>& listSubItem = pPacket->getSubItems(rack, index);

							std::list<SubItemInfo*>::const_iterator iItem = listSubItem.begin();

							//------------------------------------------------------------
							// Set up each pocket's item.
							//------------------------------------------------------------
							while (iItem != listSubItem.end())
							{
								SubItemInfo* pItemInfo = *iItem;

								if (pItemInfo!=NULL)
								{
									//------------------------------------------------------------
									// create the sub item.
									//------------------------------------------------------------
									MItem* pSubItem = MItem::NewItem( (ITEM_CLASS)pItemInfo->getItemClass() );

									if (pSubItem == NULL)
									{
										DEBUG_ADD_FORMAT_ERR("[Error] GCStashList: invalid sub item class %d", pItemInfo->getItemClass());
										iItem++;
										continue;
									}

									pSubItem->SetID( pItemInfo->getObjectID() );

									pSubItem->SetItemType( pItemInfo->getItemType() );
									//pSubItem->SetItemOption( 0 );
									//pSubItem->SetItemDurability( item.durability );
									pSubItem->SetNumber( pItemInfo->getItemNum() );

									if (!pBelt->AddItem( pSubItem, pItemInfo->getSlotID() ))
									{
										delete pSubItem;

										// pItemInfo carries the same class; pSubItem is already freed here.
										DEBUG_ADD_FORMAT_ERR("[Error] Can't Add Item to Belt. rack=%d, slot=%d, class=%d, belt-slot=%d", rack, index, (int)pItemInfo->getItemClass(), (int)pItemInfo->getSlotID());
									}
								}

								iItem++;
							}
						}
						else
						{
							DEBUG_ADD_FORMAT_ERR("[Error] This item can't have subitems. itemClass=%d", (int)pItem->GetItemClass());
						}
					}

					//------------------------------------------------------------
					// Put the item in its slot
					//------------------------------------------------------------
					if (!g_pStorage->SetItem( index, pItem ))
					{
						// The slot refused it.
						delete pItem;

						DEBUG_ADD_FORMAT_ERR("[Error] Can't Add Item to Storage. rack=%d, slot=%d", rack, index);
					}
				}
			}
		}
		
		//------------------------------------------------------------
		// The stashed money
		//------------------------------------------------------------
		g_pStorage->GetMoneyManager()->SetMoney( pPacket->getStashGold() );

		//------------------------------------------------------------
		// Open the stash window.
		//------------------------------------------------------------
		DialogPresentation::RunStorage();
		DialogPresentation::SetStorage(g_pStorage);
	}
	//------------------------------------------------------------
	// No stash
	//------------------------------------------------------------
	else
	{
		DialogPresentation::PopupFreeMessageDlg((*g_pGameStringTable)[STRING_MESSAGE_NO_STORAGE].GetString());
	}
		

	__END_CATCH
}
