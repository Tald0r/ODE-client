//////////////////////////////////////////////////////////////////////
//
// Filename    : GCOtherStoreInfoHandler.cc
// Written By  : reiot@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCOtherStoreInfo.h"
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

void GCOtherStoreInfoHandler::execute ( GCOtherStoreInfo * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
	//__BEGIN_DEBUG_EX
	



		MCreature* pCreature = ZoneState::GetCreature(pPacket->getObjectID());
		if(pCreature == NULL)
			return; 
		if(IsNPCType(pCreature) == false)
			CreatureState::SetPersnalShop(pCreature, pPacket->getStoreInfo()->isOpen());
		
		if(CreatureState::CurPernalShop(pCreature) == 1 && IsNPCType(pCreature) == false)
		{
			if(!pPacket->getStoreInfo()->getSign().empty())
				CreatureState::SetPersnalString(pCreature, pPacket->getStoreInfo()->getSign().c_str(), CommunicationPresentation::GetChattingColor());
			else
				CreatureState::SetPersnalString(pCreature, (*g_pGameStringTable)[UI_STRING_MESSAGE_PERSNAL_DEFAULT_MESSGE].GetString(), CommunicationPresentation::GetChattingColor());
		}

		if(g_pStorage2 != NULL && g_pStorage2->GetCuropenid() !=0)
		{
			if(false == pPacket->getStoreInfo()->isOpen() && g_pStorage2->GetCuropenid() == pPacket->getObjectID())
			{
				if(VsUiPresentation::IsRunningPersnalShop())
					VsUiPresentation::ClosePersnalShop();
				return;
			}
		}

		if(strlen(pPacket->getStoreInfo()->getSign().c_str())>0 && IsNPCType(pCreature) == false)
			CreatureState::SetPersnalShopMessage(pCreature, pPacket->getStoreInfo()->getSign().c_str());

		if(false == pPacket->isRequested()) // not the player opening the shop by choice, only news that it opened
			return;


		// the player chose to open the personal shop: handled below
		if (g_pStorage2!=NULL) 
			delete g_pStorage2;

		g_pStorage2 = new MStorage;
		g_pStorage2->Init( 1 ); //STASH_RACK_MAX );	// hm.. 3 of them?? 
		g_pStorage2->SetCurrent( 0 );
		pPacket->getStoreInfo()->getSign();
		g_pStorage2->SetCuropenid(pPacket->getObjectID());

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
						DEBUG_ADD_FORMAT_ERR("[Error] GCOtherStoreInfo: invalid item class %d", item.getItemClass());
						continue;
					}

					pItem->SetID( item.getObjectID() );

					pItem->SetItemType( item.getItemType() );
					pItem->SetItemOptionList( item.getOptionType() );
					pItem->SetCurrentDurability( item.getDurability() );
					pItem->SetPersnalPrice(item.getPrice());
					pItem->SetNumber(pPacket->getStoreInfo()->getStoreItemInfo(index).getItemNum());
					
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
										DEBUG_ADD_FORMAT_ERR("[Error] GCOtherStoreInfo: invalid sub item class %d", pItemInfo->getItemClass());
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
										DEBUG_ADD_FORMAT_ERR("[Error] GCOtherStoreInfo: invalid sub item class %d", pItemInfo->getItemClass());
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
			//		}
				}
			}
		}
		
		VsUiPresentation::SetInventoryMode(2);
		VsUiPresentation::RunPersnalShop();
		VsUiPresentation::SetInventoryMode(2);
		VsUiPresentation::SetPersnalShop( g_pStorage2 ); 		
			 


	//__END_DEBUG_EX 
	__END_CATCH
}
