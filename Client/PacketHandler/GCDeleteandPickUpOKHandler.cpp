//////////////////////////////////////////////////////////////////////
//
// Filename    : GCDeleteandPickUpOKHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCDeleteandPickUpOK.h"
#include "BonusSkillHost.h"
#include "ClientPlayer.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "GameInfoPresentationHost.h"
#include "MHelpDef.h"
#include "MInventory.h"
#include "MItem.h"
#include "MMoneyManager.h"
#include "MQuickSlot.h"
#include "MSkillManager.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "TempInformation.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Packet/Cpackets/CGAddMouseToQuickSlot.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCDeleteandPickUpOKHandler::execute ( GCDeleteandPickUpOK * pPacket, Player * pPlayer )

{
	__BEGIN_TRY
		


	//---------------------------------------------
	// remember the ItemCheckBuffer's item
	//---------------------------------------------
	MItem* pItem = PlayerState::GetItemCheckBuffer();

	//---------------------------------------------
	// the buffer's status
	//---------------------------------------------
	PlayerState::ITEM_CHECK_BUFFER	status = PlayerState::GetItemCheckBufferStatus();

	//---------------------------------------------
	// clear the item check buffer.
	//---------------------------------------------
	PlayerState::ClearItemCheckBuffer();

	//---------------------------------------------
	// no item to pick up? what..
	//---------------------------------------------
	if (pItem==NULL)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] Pickup Item : no item! [ID=%d]", pPacket->getObjectID());
		
		return;
	}

	ITEM_CLASS itemClass = pItem->GetItemClass();

	BOOL bRemoveZoneItem = FALSE;

	//------------------------------------------------------------------------
	// the same id..
	//------------------------------------------------------------------------
	if (pItem->GetID() == pPacket->getObjectID())
	{		
		BOOL bSkillCheck = FALSE;

		// our side's relic: remove it at once
//		if (pItem->GetItemClass()==ITEM_CLASS_RELIC// && 
//				(
//					(pItem->GetItemType() == 0 || pItem->GetItemType() == 1 ) && g_pPlayer->IsSlayer() ||
//					(pItem->GetItemType() == 2 || pItem->GetItemType() == 3 ) && g_pPlayer->IsVampire()
//				)
//			)
//		{
//			// sound
//			PlaySound( pItem->GetTileSoundID() );
//
//			// remove it from the zone
//			bRemoveZoneItem = TRUE;
//			//g_pZone->RemoveItem( pItem->GetID() );
//			
//		}

		//------------------------------------------------------------------------
		// an event item... remove it.
		//------------------------------------------------------------------------
//		else 
		if (pItem->GetItemClass()==ITEM_CLASS_SKULL
			&& pItem->GetItemType()>=12 && pItem->GetItemType()<=16)
		{
			// sound
			WorldEvents::PlaySound( pItem->GetInventorySoundID() );

			// remove it from the zone
			bRemoveZoneItem = TRUE;
			//g_pZone->RemoveItem( pItem->GetID() );
		}
		//------------------------------------------------------------------------
		// picking up money
		//------------------------------------------------------------------------
		else if (status == PlayerState::ITEM_CHECK_BUFFER_PICKUP_MONEY)
		{
			int lowWord = pItem->GetCurrentDurability();
			int highWord = pItem->GetSilver();
			int money = (highWord << 16) | lowWord;

			// not past the money limit
			money = min(money, g_pMoneyManager->GetMaxAddMoney());
			
			// add the money
			g_pMoneyManager->AddMoney( money );

			// sound
			WorldEvents::PlaySound( pItem->GetTileSoundID() );

			// remove it from the zone
			bRemoveZoneItem = TRUE;
			//g_pZone->RemoveItem( pItem->GetID() );
		}		
		//------------------------------------------------------------------------
		// putting the item in the inventory
		//------------------------------------------------------------------------
		else if (status == PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY)
		{
			const MItem* pOldItem = g_pInventory->GetItem( pItem->GetGridX(), pItem->GetGridY() );

			//------------------------------------------------------------------------
			// added to an empty place
			//------------------------------------------------------------------------
			if (pOldItem==NULL)
			{
				//---------------------------------------------
				// put the item in the inventory.
				//---------------------------------------------
				if (g_pInventory->AddItem( pItem, pItem->GetGridX(), pItem->GetGridY() ))
				{		
					bSkillCheck = TRUE;

					// remove it from the zone only once it is in the inventory.
					if (!ZoneState::Exists())
					{					
						DEBUG_ADD_ERR("[Error] Zone is not Init!");
					}
					else
					{
						// play the pick-up sound.
						//PlaySound( pItem->GetTileSoundID(),
						//			false,
						//			g_pPlayer->GetX(), g_pPlayer->GetY());

						ZoneState::PickupItem( pItem->GetID() );
					}
				}
				//---------------------------------------------
				// otherwise what?? item
				//---------------------------------------------
				else
				{
					// there was room, and now there is not..
					DEBUG_ADD_FORMAT_ERR("[Error] Pickup Item ID no fit position! ID=%d, xy=(%d, %d)", pItem->GetID(), pItem->GetGridX(), pItem->GetGridY());
				}
			}
			//------------------------------------------------------------------------
			// piled on the item already there
			//------------------------------------------------------------------------
			else
			{
				//--------------------------------------------------------
				// check once more that the items pile.
				//--------------------------------------------------------
				if (pOldItem->IsPileItem() && pItem->IsPileItem()
					&& pOldItem->GetItemClass()==itemClass
					&& pOldItem->GetItemType()==pItem->GetItemType())
				{
					bSkillCheck = TRUE;

					//----------------------------------------------------
					// pile pItem on pOldItem.
					//----------------------------------------------------
					int total = pOldItem->GetNumber() + pItem->GetNumber();
					if ( static_cast<TYPE_ITEM_NUMBER>(total) > pOldItem->GetMaxNumber() )
					{
						DEBUG_ADD_FORMAT_ERR("[Error] Exceed Item Pile Limit : %d/%d", total, pOldItem->GetMaxNumber());
						
						// assume only up to the max is added.
						total = pItem->GetMaxNumber();
					}
					
					//---------------------------------------------
					// change OldItem's number and add it again.
					//---------------------------------------------
					MItem* pNewItem = g_pInventory->RemoveItem( pOldItem->GetID() );
					pNewItem->SetNumber( total );
					g_pInventory->AddItem( pNewItem, pItem->GetGridX(), pItem->GetGridY() );

					// remove the item completely.
					bRemoveZoneItem = TRUE;
					//g_pZone->RemoveItem( pItem->GetID() );
				}
				//--------------------------------------------------------
				// items that do not pile
				//--------------------------------------------------------
				else
				{
					DEBUG_ADD_FORMAT_ERR("[Error] Cannot Pile Items : inv_item_id=%d, zone_item_id=%d", pItem->GetID(), pOldItem->GetID());
				}
			}
		}
		//---------------------------------------------
		// putting the item on the mouse
		//---------------------------------------------
		else if (status == PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE)
		{
			bSkillCheck = TRUE;

			DialogPresentation::PickUpItem( pItem );

			if (!ZoneState::Exists())
			{
				DEBUG_ADD_ERR("[Error] Zone is not Init!");
			}
			else
			{				
				// play the pick-up sound.
				//PlaySound( pItem->GetTileSoundID(),
				//			false,
				//			g_pPlayer->GetX(), g_pPlayer->GetY());


				ZoneState::PickupItem( pItem->GetID() );
			}
		}
		//------------------------------------------------------------------------
		// putting the item in the quick slot
		//------------------------------------------------------------------------
		else if (status == PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_QUICKSLOT)
		{
			if (g_pTempInformation->GetMode()==TempInformation::MODE_TRADE_VERIFY_PICKUP_TO_QUICKSLOT)
			{
				g_pTempInformation->SetMode(TempInformation::MODE_NULL);
				int slot = static_cast<int>(g_pTempInformation->Value1);

				int itemID = pItem->GetID();

				// yum..
				bSkillCheck = TRUE;	
				
				if (!ZoneState::Exists())		// just for show - -;
				{
					DEBUG_ADD_ERR("[Error] Zone is not Init!");	// in pieces~
				}
				else 
				{	
					DialogPresentation::PickUpItem( pItem );

					// pick it up from the zone.
					ZoneState::PickupItem( pItem->GetID() );

					//------------------------------------------
					// put it in the quick slot again.
					//------------------------------------------
					if ((g_pQuickSlot!=NULL&&PlayerState::GetRace()==RACE_SLAYER) || (PlayerState::Creature()!=NULL && CreatureState::IsOusters( PlayerState::Creature() ) &&(
						g_pArmsBand1 != NULL || g_pArmsBand2 != NULL )))
					{
						MItem* pQuickItem = NULL ;//= g_pQuickSlot->GetItem( slot );
						MOustersArmsBand* pQuickSlot = NULL;
						
						if( slot >= 3 )
							pQuickSlot = g_pArmsBand2;
						else
							pQuickSlot = g_pArmsBand1;
						
						if( PlayerState::GetRace()==RACE_SLAYER )
							pQuickItem = g_pQuickSlot->GetItem( slot );
						else
							pQuickItem = pQuickSlot->GetItem( slot % 3 );


						bool bSendPacket = false;

						//--------------------------------------------------------
						// nothing there: just put it in.
						//--------------------------------------------------------
						if (pQuickItem==NULL)
						{
							// mouse --> quickslot
							DialogPresentation::DropItem();
							
							if( pQuickSlot!= NULL)
							{
								if( !pQuickSlot->AddItem( pItem, slot %3) )
								{
									DEBUG_ADD_FORMAT_ERR("[Error] can't add to quickslot. ousters id=%d, slot=%d", pItem->GetID(), slot);
								}
							}else
							if (!g_pQuickSlot->AddItem( pItem, slot ))
							{
								DEBUG_ADD_FORMAT_ERR("[Error] can't add to quickslot. id=%d, slot=%d", pItem->GetID(), slot);
							}

							bSendPacket = true;
						}
						//--------------------------------------------------------
						// something there must be something it piles on.
						// check once more.
						//--------------------------------------------------------
						else
						{
							if (pQuickItem->GetItemClass()==pItem->GetItemClass()
								&& pQuickItem->GetItemType()==pItem->GetItemType())
							{
								//----------------------------------------------------
								// the sum must not pass the max.
								//----------------------------------------------------
								int addTotal = pQuickItem->GetNumber() + pItem->GetNumber();
								if ( static_cast<TYPE_ITEM_NUMBER>(addTotal) <= pQuickItem->GetMaxNumber() )
								{
									DialogPresentation::DropItem();

									delete pItem;	// merged, so delete it.

									pItem = pQuickItem;	// pItem is read below..
									
									pQuickItem->SetNumber( addTotal );

									bSendPacket = true;
								}
							}
						}

						if (bSendPacket)
						{
							//------------------------------------------
							// this needs no verification.
							//------------------------------------------
							CGAddMouseToQuickSlot _CGAddMouseToQuickSlot;
							_CGAddMouseToQuickSlot.setObjectID( itemID );
							_CGAddMouseToQuickSlot.setSlotID( slot );
							
							ClientPlayer * pClientPlayer = dynamic_cast<ClientPlayer*>(pPlayer);

							pClientPlayer->sendPacket( &_CGAddMouseToQuickSlot );
						}
					}
				}
			}		 
		}
		//---------------------------------------------
		// another case?
		//---------------------------------------------		
		else
		{
			// what??
			DEBUG_ADD_FORMAT_ERR("[Error] ItemCheckBuffer is not Pickup Status [ID=%d]", pItem->GetID());
		}

		//------------------------------------------------------------------
		// what follows a proper pick-up..
		//------------------------------------------------------------------
		if (bSkillCheck)
		{
			PlayerState::CheckAffectStatus(pItem);

			//------------------------------------------------------------------------
			//
			//							A slayer
			//
			//------------------------------------------------------------------------
			if (PlayerState::GetRace()==RACE_SLAYER)
			{
				switch (itemClass)
				{
					//----------------------------------------------------------
					// holy water
					//----------------------------------------------------------
					case ITEM_CLASS_HOLYWATER :
						g_pSkillAvailable->AddSkill( MAGIC_THROW_HOLY_WATER );
					break;

					//----------------------------------------------------------
					// a slayer portal
					//----------------------------------------------------------
					case ITEM_CLASS_SLAYER_PORTAL_ITEM :
						g_pSkillAvailable->AddSkill( SUMMON_HELICOPTER );
					break;

					//----------------------------------------------------------
					// bomb/mine material
					//----------------------------------------------------------
					case ITEM_CLASS_BOMB_MATERIAL :
					{
						// 0~4 are bombs
						// 5~9 are mines -_-;
						// SKILL_INSTALL_MINE must be checked. -_-;
						// the available-skill set had better check each skill.
						BonusSkills::RefreshAvailableSkills();
					}
					break;

					//----------------------------------------------------------
					// bombs / mines
					//----------------------------------------------------------
					case ITEM_CLASS_BOMB :
						BonusSkills::RefreshAvailableSkills();
						g_pSkillAvailable->AddSkill( (ACTIONINFO)pItem->GetUseActionInfo(), 
													g_pSkillAvailable->IsEnableSkill(SKILL_THROW_BOMB));
					break;

					case ITEM_CLASS_MINE :
						BonusSkills::RefreshAvailableSkills();
						g_pSkillAvailable->AddSkill( (ACTIONINFO)pItem->GetUseActionInfo(),
													g_pSkillAvailable->IsEnableSkill(SKILL_INSTALL_MINE));
					break;

					default:
					break;
				}
			}
			//----------------------------------------------------------
			//
			//					A vampire
			//
			//----------------------------------------------------------
			else if (PlayerState::GetRace()==RACE_VAMPIRE)
			{
				switch (itemClass)
				{
					//----------------------------------------------------------
					// Vampire Portal Item
					//----------------------------------------------------------
					case ITEM_CLASS_VAMPIRE_PORTAL_ITEM :
						if (MItem::IsPortalMarked( pItem ))
						{
							if (g_pSkillAvailable->find( MAGIC_BLOODY_TUNNEL ) != g_pSkillAvailable->end())
							{
								g_pSkillAvailable->AddSkill( MAGIC_BLOODY_TUNNEL );
							}
						}
						else
						{
							if (g_pSkillAvailable->find( MAGIC_BLOODY_MARK ) != g_pSkillAvailable->end())
							{
								g_pSkillAvailable->AddSkill( MAGIC_BLOODY_MARK );
							}
						}
					break;

					//----------------------------------------------------------
					// Vampire ETC (transformation items)
					//----------------------------------------------------------
					case ITEM_CLASS_VAMPIRE_ETC :
						if (pItem->GetItemType()==0)
						{
							if (g_pSkillAvailable->find( MAGIC_TRANSFORM_TO_WOLF ) != g_pSkillAvailable->end())
							{
								g_pSkillAvailable->AddSkill( MAGIC_TRANSFORM_TO_WOLF );
							}
						} 
						else if (pItem->GetItemType()==1)
						{
							if (g_pSkillAvailable->find( MAGIC_TRANSFORM_TO_BAT ) != g_pSkillAvailable->end())
							{
								g_pSkillAvailable->AddSkill( MAGIC_TRANSFORM_TO_BAT );
							}
						}
					break;

					default:
					break;
				}
			}
		}

		//---------------------------------------------
		// the item must leave the zone
		//---------------------------------------------
		if (bRemoveZoneItem)
		{
			ZoneState::RemoveItem( pItem->GetID() );
		}
	}
	//---------------------------------------------
	// a different id.. what?? - -;;
	//---------------------------------------------
	else
	{
		DEBUG_ADD_FORMAT_ERR("[Error] Pickup Item ID conflict (%d != %d)", pItem->GetID(), pPacket->getObjectID());
	}

//	__BEGIN_HELP_EVENT
		if(itemClass == ITEM_CLASS_BELT)
			GameInfoPresentation::ExecuteHelpEvent( HELP_EVENT_GET_BELT );
		else
			GameInfoPresentation::ExecuteHelpEvent( HELP_EVENT_GET_ITEM );
//	__END_HELP_EVENT

//	__BEGIN_HELP_EVENT
//		if (status == PlayerState::ITEM_CHECK_BUFFER_PICKUP_MONEY)
//		{
//			// [Help] picking up an item
//			ExecuteHelpEvent( HE_ITEM_PICKUP_MONEY );	
//		}
//		else
//		{
//			// [Help] picking up an item
//			ExecuteHelpEvent( HE_ITEM_PICKUP );	
//		}
//	__END_HELP_EVENT



	__END_CATCH
}
