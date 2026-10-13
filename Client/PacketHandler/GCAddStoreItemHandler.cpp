//////////////////////////////////////////////////////////////////////
//
// Filename    : GCAddStoreItemHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCAddStoreItem.h"
#include "MStorage.h"
#include "PCItemInfo.h"
#include "DebugLog.h"


//////////////////////////////////////////////////////////////////////
//
// 클라이언트에서 서버로부터 메시지를 받았을때 실행되는 메쏘드이다.
//
//////////////////////////////////////////////////////////////////////
void GCAddStoreItemHandler::execute ( GCAddStoreItem * pPacket , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;
		//__BEGIN_DEBUG_EX
		
		
		if(g_pStorage2 != NULL && g_pStorage2->GetCuropenid() !=0)
		{
			if( pPacket->getOwnerObjectID() == g_pStorage2->GetCuropenid())
			{
				g_pStorage2->SetCurrent( 0 );
				
				int current_storage = g_pStorage2->GetCurrent();
				const int storage_size = g_pStorage2->GetSize();
				const int storage_max = STORAGE_SLOT;
				
				for(int j = 0; j < storage_size; j++)
				{
					g_pStorage2->SetCurrent(0);
					
					for(int i = 0; i < storage_max; i++)
					{
						const MItem *p_slot_item = g_pStorage2->GetItem(i);
						
						// 슬랏이 비었으면 걍~ 넣는다
						if(p_slot_item == NULL)
						{
							//------------------------------------------------------------
							// create the item.
							//------------------------------------------------------------
							MItem* pItem = MItem::NewItem( (ITEM_CLASS)pPacket->getItem().getItemClass() );

							if (pItem == NULL)
							{
								DEBUG_ADD_FORMAT_ERR("[Error] GCAddStoreItem: invalid item class %d", pPacket->getItem().getItemClass());
								return;
							}

							pItem->SetID( pPacket->getItem().getObjectID() );
							
							pItem->SetItemType( pPacket->getItem().getItemType() );
							pItem->SetItemOptionList( pPacket->getItem().getOptionType() );
							pItem->SetCurrentDurability( pPacket->getItem().getDurability() );
							pItem->SetPersnalPrice(pPacket->getItem().getPrice());
							
							//------------------------------------------
							// 개수
							//------------------------------------------
							// 총인 경우
							//------------------------------------------
							if (pItem->IsGunItem())
							{
								// A fresh magazine of the gun's type with one round, as a new
								// magazine starts; without one the gun is stored unloaded.
								if (!MItem::LoadGun( pItem, 1 ))
								{
									DEBUG_ADD_ERR("[Error] GCAddStoreItem: no magazine for the gun");
								}
							}		
							else
							{
								pItem->SetSilver( pPacket->getItem().getSilver() );
							}
							
							pItem->SetGrade( pPacket->getItem().getGrade() );
							pItem->SetEnchantLevel( pPacket->getItem().getEnchantLevel() );
							
							//------------------------------------------------------------
							// Sub Item이 있으면 생성한다.
							//------------------------------------------------------------
							
							int subNum =static_cast<int>(pPacket->getItem().getSubItems().size());
							
							if (subNum!=0)
							{
								//------------------------------------------------------------
								// Belt인 경우
								//------------------------------------------------------------
								if (pItem->GetItemClass()==ITEM_CLASS_BELT)
								{
									MBelt* pBelt = (MBelt*)pItem;
									std::list<SubItemInfo*>& listSubItem = pPacket->getItem().getSubItems();
									std::list<SubItemInfo*>::const_iterator iItem = listSubItem.begin();
									
									//------------------------------------------------------------
									// 각각의 sub item을 설정한다.
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
												DEBUG_ADD_FORMAT_ERR("[Error] GCAddStoreItem: invalid sub item class %d", pItemInfo->getItemClass());
												iItem++;
												continue;
											}

											pSubItem->SetID( pItemInfo->getObjectID() );
											
											pSubItem->SetItemType( pItemInfo->getItemType() );
											pSubItem->SetNumber( pItemInfo->getItemNum() );
											
											if (!pBelt->AddItem( pSubItem, pItemInfo->getSlotID() ))
											{
												delete pSubItem;
												
												//										DEBUG_ADD_FORMAT("[Error] Can't Add Item to Belt. rack=%d, slot=%d, class=%d, belt-slot=%d", rack, index, (int)pSubItem->GetItemClass(), (int)pItemInfo->getSlotID());
											}
										}
										
										iItem++;
									}
								}
								else if (pItem->GetItemClass()==ITEM_CLASS_OUSTERS_ARMSBAND)
								{
									MOustersArmsBand* pBelt = (MOustersArmsBand*)pItem;
									
									std::list<SubItemInfo*>& listSubItem = pPacket->getItem().getSubItems();
									
									std::list<SubItemInfo*>::const_iterator iItem = listSubItem.begin();
									
									//------------------------------------------------------------
									// 각각의 sub item을 설정한다.
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
												DEBUG_ADD_FORMAT_ERR("[Error] GCAddStoreItem: invalid sub item class %d", pItemInfo->getItemClass());
												iItem++;
												continue;
											}

											pSubItem->SetID( pItemInfo->getObjectID() );
											
											pSubItem->SetItemType( pItemInfo->getItemType() );
											pSubItem->SetNumber( pItemInfo->getItemNum() );
											
											if (!pBelt->AddItem( pSubItem, pItemInfo->getSlotID() ))
											{
												delete pSubItem;
												
												//										DEBUG_ADD_FORMAT("[Error] Can't Add Item to Belt. rack=%d, slot=%d, class=%d, belt-slot=%d", rack, index, (int)pSubItem->GetItemClass(), (int)pItemInfo->getSlotID());
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
							
							if (!g_pStorage2->SetItem( i, pItem ))
							{
								delete pItem;
								
								//DEBUG_ADD_FORMAT("[Error] Can't Add Item to Storage. rack=%d, slot=%d", rack, index);
							}
							break;
			}
		}
		current_storage ++;
		if(current_storage >= storage_size)
			current_storage = 0;
	}
	
		}
		
		
}	


//__END_DEBUG_EX 
__END_CATCH
}
