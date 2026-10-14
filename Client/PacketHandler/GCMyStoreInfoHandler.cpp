//////////////////////////////////////////////////////////////////////
//
// Filename    : GCMyStoreInfoHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCMyStoreInfo.h"
#include "MStorage.h"
#include "PCItemInfo.h"
#include "CommunicationPresentationHost.h"
#include "DebugLog.h"
#include "MCreatureTable.h"
#include "MGameStringTable.h"
#include "MInventory.h"
#include "MItem.h"
#include "PlayerStateHost.h"
#include "VsUiPresentationHost.h"
#include "ZoneStateHost.h"
//////////////////////////////////////////////////////////////////////
//
// The method run when the client receives the message from the server.
//
//////////////////////////////////////////////////////////////////////
// MCreature::IsNPC: whether the creature table's row for its type is an
// NPC's.
static bool IsNPCType(MCreature* pCreature)
{
	return (*g_pCreatureTable)[CreatureState::GetCreatureType(pCreature)].IsNPC();
}

void GCMyStoreInfoHandler::execute ( GCMyStoreInfo * pPacket , Player * pPlayer )

{
	
		__BEGIN_TRY 
		(void)pPlayer;

		MCreature* const pPlayerCreature = PlayerState::Creature();
		if (pPlayerCreature == NULL)
		{
			return;
		}

		if(PlayerState::IsTraceCreature())
			PlayerState::TraceNULL();

		if(IsNPCType(pPlayerCreature) == false)
			CreatureState::SetPersnalShop(pPlayerCreature, pPacket->getStoreInfo()->isOpen());

		if(pPacket->getStoreInfo()->isOpen()) // only when the shop is open 
		{
			if(!pPacket->getStoreInfo()->getSign().empty()) 
				CreatureState::SetPersnalString(pPlayerCreature, pPacket->getStoreInfo()->getSign().c_str(), CommunicationPresentation::GetChattingColor());
			else
				CreatureState::SetPersnalString(pPlayerCreature, (*g_pGameStringTable)[UI_STRING_MESSAGE_PERSNAL_DEFAULT_MESSGE].GetString(), CommunicationPresentation::GetChattingColor());
		}
 
		if(VsUiPresentation::GetPersnalShoptoUI() == 3) // closing the shop
		{
			CreatureState::ClearChatString(pPlayerCreature);
			VsUiPresentation::SetPersnalShoptoUI(0);
			VsUiPresentation::ClosePersnalShop(); 

			if (g_pStorage2!=NULL)
			{
				delete g_pStorage2;
				g_pStorage2 = NULL;
			}
			VsUiPresentation::SetPersnalShop( NULL );
			
			return;
		}
		if(VsUiPresentation::GetPersnalShoptoUI() == 2) // return when opening the shop.
		{ 
			VsUiPresentation::SetPersnalShoptoUI(0);
			VsUiPresentation::ClosePersnalShop(); 
			return;
		}

 		if(g_pStorage2!=NULL) 
		{
			delete g_pStorage2;
			g_pStorage2 = NULL;
		}
	
		g_pStorage2 = new MStorage;
		g_pStorage2->Init( 1 ); //STASH_RACK_MAX );	// hm.. 3 of them?? 
		g_pStorage2->SetCurrent( 0 );

		if(PlayerState::GetID() != 0)
			g_pStorage2->SetCuropenid(PlayerState::GetID());


			
		MItem * p_olditem = NULL;
		MItem * p_Newitem = NULL;
		  
		if(strlen(pPacket->getStoreInfo()->getSign().c_str())>0)
			CreatureState::SetPersnalShopMessage(pPlayerCreature, pPacket->getStoreInfo()->getSign().c_str());
//			set->SetPersnalShopMessage(pPacket->getStoreInfo()->getSign().c_str());
		for (int rack=0; rack<1; rack++)
		{	 
			//------------------------------------------------------------
			// Set the storage being accessed.
			//------------------------------------------------------------
			//int numitem = pPacket->getStoreInfo().getItems().size();
			int numitem = static_cast<int>(pPacket->getStoreInfo()->getItems().size());
			for (int index=0; index<numitem; index++)
			{
// 
				StoreItemInfo&	item = pPacket->getStoreInfo()->getStoreItemInfo(index);
				//if(item.isItemExist() == 45)
				if(item.isItemExist() != 0)
				{
					 
					//------------------------------------------------------------
					// create the item.
					//------------------------------------------------------------
					MItem* pItem = MItem::NewItem( (ITEM_CLASS)item.getItemClass() );

					if (pItem == NULL)
					{
						DEBUG_ADD_FORMAT_ERR("[Error] GCMyStoreInfo: invalid item class %d", item.getItemClass());
						continue;
					}

					pItem->SetID( item.getObjectID() );

					pItem->SetItemType( item.getItemType() );
					pItem->SetItemOptionList( item.getOptionType() );
					pItem->SetCurrentDurability( item.getDurability() );
					pItem->SetPersnalPrice(item.getPrice());
					pItem->SetNumber(pPacket->getStoreInfo()->getStoreItemInfo(index).getItemNum());
					
					g_pInventory->SetBegin();
					while (g_pInventory->IsNotEnd())
					{
						MItem * p_item = g_pInventory->Get();
						
						if(p_item != NULL)
						{
							if(item.getObjectID() == p_item->GetID())
							{
								p_olditem = g_pInventory->GetItem(p_item->GetGridX(),p_item->GetGridY());
								p_Newitem = p_olditem;
								p_Newitem->SetPersnal(true);
								g_pInventory->ReplaceItem(p_Newitem,p_item->GetGridX(),p_item->GetGridY(),p_olditem);
								break;
							}
						}
						g_pInventory->Next();
					}
					//------------------------------------------
					// the number
					//------------------------------------------
					// a gun
					//------------------------------------------
					if (pItem->IsGunItem())
					{
						// An empty magazine of the type the gun takes, with the one
						// round a new pile holds: the loop upstream ran here, which
						// the executable's LoadGun runs.
						MItem::LoadGun( pItem, 1 );
					}		
					//------------------------------------------
					// not a gun
					//------------------------------------------
					else
					{
						pItem->SetSilver( item.getSilver() );
					}

					pItem->SetGrade( item.getGrade() );
					pItem->SetEnchantLevel( item.getEnchantLevel() );

					//------------------------------------------------------------
					// Make the sub items, if any.
					//------------------------------------------------------------

					int subNum =item.getListNum();
	
					if (subNum!=0)
					{
						//------------------------------------------------------------
						// a belt
						//------------------------------------------------------------
						if (pItem->GetItemClass()==ITEM_CLASS_BELT)
						{
							MBelt* pBelt = (MBelt*)pItem;
							std::list<SubItemInfo*>& listSubItem = item.getSubItems();
							std::list<SubItemInfo*>::const_iterator iItem = listSubItem.begin();

							//------------------------------------------------------------
							// Set each sub item.
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
										DEBUG_ADD_FORMAT_ERR("[Error] GCMyStoreInfo: invalid sub item class %d", pItemInfo->getItemClass());
										iItem++;
										continue;
									}

									pSubItem->SetID( pItemInfo->getObjectID() );

									pSubItem->SetItemType( pItemInfo->getItemType() );
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
						else if (pItem->GetItemClass()==ITEM_CLASS_OUSTERS_ARMSBAND)
						{
							MOustersArmsBand* pBelt = (MOustersArmsBand*)pItem;

							std::list<SubItemInfo*>& listSubItem = item.getSubItems();

							std::list<SubItemInfo*>::const_iterator iItem = listSubItem.begin();

							//------------------------------------------------------------
							// Set each sub item.
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
										DEBUG_ADD_FORMAT_ERR("[Error] GCMyStoreInfo: invalid sub item class %d", pItemInfo->getItemClass());
										iItem++;
										continue;
									}

									pSubItem->SetID( pItemInfo->getObjectID() );

									pSubItem->SetItemType( pItemInfo->getItemType() );
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
					// Put the item in the storage
					//------------------------------------------------------------
					if (!g_pStorage2->SetItem( index, pItem ))
					{
						// what's this.. 
						delete pItem;
						
						DEBUG_ADD_FORMAT_ERR("[Error] Can't Add Item to Storage. rack=%d, slot=%d", rack, index);
					}

			}
			}
		}
		 
		if(VsUiPresentation::GetPersnalShoptoUI() == 1)
		{
			VsUiPresentation::RunPersnalShop();
			VsUiPresentation::SetPersnalShop( g_pStorage2 ); 		
			return; 
		}
		if(VsUiPresentation::IsRunningPersnalShop()== false && CreatureState::CurPernalShop(pPlayerCreature) == false && pPacket->getOpenUI()!=0)
		{
			VsUiPresentation::RunPersnalShop();
			VsUiPresentation::SetPersnalShop( g_pStorage2 ); 		
		}
	__END_CATCH
}

  