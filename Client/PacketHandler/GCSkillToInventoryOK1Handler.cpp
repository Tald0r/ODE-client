//////////////////////////////////////////////////////////////////////
//
// Filename    : GCSkillToInventoryOK1Handler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCSkillToInventoryOK1.h"
#include "AffectModifyInfo.h"
#include "BonusSkillHost.h"
#include "ConvertDuration.h"
#include "DebugLog.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "MCreatureTable.h"
#include "MInventory.h"
#include "MItem.h"
#include "MorphState.h"
#include "MSkillManager.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneFlags.h"
#include "ZoneStateHost.h"



MItem*	PacketSkillToMakeItem(MItem* pItem,
						  ITEM_CLASS itemClass, 
						  int itemType,
						  int itemID,
						  int x, int y, 
						  int targetX, int targetY);

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCSkillToInventoryOK1Handler::execute ( GCSkillToInventoryOK1 * pPacket, Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		DEBUG_ADD_ERR("[Error] GCSkillToInventoryOK1Handler: no player");
		return;
	}
		


	//------------------------------------------------------------------
	// The skill the player waited on is verified.
	//------------------------------------------------------------------	
	if (1)//PlayerState::GetWaitVerify()==PlayerState::WAIT_VERIFY_SKILL_SUCCESS)
	{		
		PlayerState::SetWaitVerifyNULL();

		//------------------------------------------------------------------	
		// Check the item check buffer.
		//------------------------------------------------------------------
		MItem* pItem = PlayerState::GetItemCheckBuffer();

		//----------------------------------------------------
		// An item in the check buffer
		//----------------------------------------------------
		if (pItem!=NULL)
		{
			PlayerState::ITEM_CHECK_BUFFER status =	PlayerState::GetItemCheckBufferStatus();

			//----------------------------------------------------
			// used on an inventory item
			//----------------------------------------------------
			if (status==PlayerState::ITEM_CHECK_BUFFER_SKILL_TO_INVENTORY)
			{
				//--------------------------------------------------
				// Clear the item check buffer.
				//--------------------------------------------------
				PlayerState::ClearItemCheckBuffer();
		
				// the duration converted
				DWORD delayFrame = 32;//ConvertDurationToFrame( pPacket->getDuration() );
					
				//pPacket->getX(),	
				//pPacket->getY(), 
				//pPacket->getCEffectID();				
				int				skillID = pPacket->getSkillType();
				
				if( skillID >= g_pActionInfoTable->GetSize() )
					return;
				TYPE_OBJECTID	itemID	= pItem->GetID();

				switch (skillID)
				{
					//----------------------------------------------------
					// making holy water/bombs/mines
					//----------------------------------------------------
					case MAGIC_CREATE_HOLY_WATER :
					case SKILL_MAKE_BOMB :
					case SKILL_MAKE_MINE :					
					case SKILL_CREATE_HOLY_POTION :
					case SKILL_ABSORB_SOUL:
					{
						// remove pItem and add the holy water.
						int x		= pItem->GetGridX();
						int y		= pItem->GetGridY();
						int targetX = pPacket->getX();
						int targetY = pPacket->getY();
						int itemType = pPacket->getItemType();
						int newItemID	= pPacket->getObjectID();
						int	ItemNum = pItem->GetNumber();
						
						ITEM_CLASS itemClass;
						
						if (skillID==MAGIC_CREATE_HOLY_WATER)	itemClass = ITEM_CLASS_HOLYWATER;
						if (skillID==SKILL_MAKE_BOMB)			itemClass = ITEM_CLASS_BOMB;
						if (skillID==SKILL_MAKE_MINE)			itemClass = ITEM_CLASS_MINE;
						if (skillID==SKILL_ABSORB_SOUL)			itemClass = ITEM_CLASS_PUPA;
						if (skillID==SKILL_CREATE_HOLY_POTION)
						{
							itemClass = ITEM_CLASS_POTION;
							itemType = (*g_pItemTable)[pItem->GetItemClass()][pItem->GetItemType()].Value3;
						}							

						MItem* pResultItem = PacketSkillToMakeItem(pItem,
																	itemClass, 
																	itemType,
																	newItemID,
																	x, y,
																	targetX, targetY);
						if (pResultItem!=NULL)
						{
							itemID = pResultItem->GetID();
							if( skillID == MAGIC_CREATE_HOLY_WATER )
								pResultItem->SetNumber( ItemNum );
						}

						// check the skills..
						//g_SkillAvailable.CheckMP();					
					}
					break;

					//----------------------------------------------------
					// Install Mine
					//----------------------------------------------------
					case SKILL_INSTALL_MINE :
						// make it used from the inventory.
						PlayerState::SetItemCheckBuffer( pItem, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);

						PlayerState::UseItemOK();

						// inventory effect
						skillID = SKILL_INSTALL_MINE;

						delayFrame = 14;

						BonusSkills::RefreshAvailableSkills();
					break;

					//----------------------------------------------------
					// Vampire Portal
					//----------------------------------------------------
					case MAGIC_BLOODY_TUNNEL :
						// make it used from the inventory.
						PlayerState::SetItemCheckBuffer( pItem, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);
						PlayerState::UseItemOK();

						// inventory effect
						skillID = MAGIC_BLOODY_MARK;

						delayFrame = 14;
					break;

					case MAGIC_BLOODY_MARK :
						//-------------------------------------------------
						// a vampire portal
						//-------------------------------------------------
						if (pItem->GetItemClass()==ITEM_CLASS_VAMPIRE_PORTAL_ITEM)
						{
							if (!MItem::IsPortalMarked( pItem ))
							{
								int zoneID = (g_bZonePlayerInLarge? g_nZoneLarge : g_nZoneSmall);
								int zoneX = CreatureState::GetServerX( pPlayerCreature );
								int zoneY = CreatureState::GetServerY( pPlayerCreature );
								MItem::MarkPortal( pItem, zoneID, zoneX, zoneY );
							}
						}

						// inventory effect
						skillID = MAGIC_BLOODY_MARK;

						delayFrame = 14;
					break;

					//----------------------------------------------------
					// turning into a wolf / bat
					//----------------------------------------------------
					case MAGIC_TRANSFORM_TO_WOLF :
					case MAGIC_TRANSFORM_TO_BAT :
					case SKILL_TRANSFORM_TO_WERWOLF :
					{
						//----------------------------------------------------
						// remove pItem.
						//----------------------------------------------------
						/*
						int x = pItem->GetGridX();
						int y = pItem->GetGridY();
						
						MItem* pRemovedItem = g_pInventory->RemoveItem( x, y );

						if (pRemovedItem!=NULL)
						{
							delete pRemovedItem;
						}
						else
						{
							DEBUG_ADD_FORMAT("[Error] No Removed Item from Inventory=(%d, %d)", x, y);
						}
						*/
						// make it used from the inventory.

						PlayerState::SetItemCheckBuffer( pItem, PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY);

						PlayerState::UseItemOK();

						//----------------------------------------------------
						// transform
						//----------------------------------------------------
						MActionResult* pResult = new MActionResult;

						const int wolfCreatureType = CREATURETYPE_WOLF;
						const int batCreatureType = CREATURETYPE_BAT;
						const int werwolfCreatureType = CREATURETYPE_WER_WOLF;

						int resultSkillID;

						int creatureType = 0;

						if (skillID==MAGIC_TRANSFORM_TO_WOLF)
						{
							pResult->Add( new MActionResultNodeChangeCreatureType( PlayerState::GetID(), wolfCreatureType ) );
							resultSkillID = RESULT_MAGIC_TRANSFORM_TO_WOLF;							

							creatureType = wolfCreatureType;
						}
						else if ( skillID == SKILL_TRANSFORM_TO_WERWOLF )
						{
							pResult->Add( new MActionResultNodeChangeCreatureType( PlayerState::GetID(), werwolfCreatureType ) );

							resultSkillID = RESULT_SKILL_TRANSFORM_TO_WERWOLF;
							creatureType = werwolfCreatureType;
						}
						else	// MAGIC_TRANSFORM_TO_BAT
						{
							pResult->Add( new MActionResultNodeChangeCreatureType( PlayerState::GetID(), batCreatureType ) );
							resultSkillID = RESULT_MAGIC_TRANSFORM_TO_BAT;

							creatureType = batCreatureType;
						}							

						//--------------------------------------------------
						// add the result to the transformation effect
						//--------------------------------------------------
						WorldEvents::ExecuteActionInfoFromMainNode(
							static_cast<TYPE_ACTIONINFO>(resultSkillID),		// the skill used
							CreatureState::GetX( pPlayerCreature ), CreatureState::GetY( pPlayerCreature ), 0,
							CreatureState::GetDirection( pPlayerCreature ),		// the direction used
							OBJECTID_NULL,										// the target
							CreatureState::GetX( pPlayerCreature ), CreatureState::GetY( pPlayerCreature ), 0,
							0,													// the skill's (remaining) duration
							pResult,
							false, 0, 0);		// the skill starts from the beginning.

						PlayerState::SetDelay( 1000 );

						//--------------------------------------------------
						// the transformation goes wrong
						// when moving to another zone, so
						// for now.. -_-;
						// MActionResultNodeChangeCreatureType or
						// GCUpdateInfoHandler clears this.
						//--------------------------------------------------
						g_MorphCreatureType = creatureType;


						//--------------------------------------------------
						// check the skills..
						//--------------------------------------------------
						BonusSkills::RefreshAvailableSkills();
							
					}
					break;
				}

				
				// the skill is used on the inventory item.
				WorldEvents::AddNewInventoryEffect( itemID,
										static_cast<TYPE_ACTIONINFO>(skillID + (*g_pActionInfoTable).GetMinResultActionInfo()),
										delayFrame,
										0
									);

			}
			//----------------------------------------------------
			// another status??
			//----------------------------------------------------
			else
			{
				DEBUG_ADD_FORMAT_ERR("[Error] ItemCheck Buffer is not Use Status: status=%d", (int)status);
			}
		}
		
	}
	else
	{
		DEBUG_ADD_ERR("[Error] Player is not WaitVerifySkillSuccess");
	}

	int skillID = pPacket->getSkillType();
	int resultActionInfo = skillID + (*g_pActionInfoTable).GetMinResultActionInfo();
	
	//------------------------------------------------------------
	// set the delay frame
	//------------------------------------------------------------
	DWORD delayFrame = ConvertDurationToFrame( pPacket->getDuration() );
	CreatureState::SetEffectDelayFrame( pPlayerCreature, resultActionInfo, delayFrame );

	//------------------------------------------------------------------
	// change the status values.
	//------------------------------------------------------------------
	MStatus* pPlayerStatus = PlayerState::Status();
	if (pPlayerStatus != NULL)
	{
		AffectModifyInfo(pPlayerStatus, pPacket);
	}

	//------------------------------------------------------------------
	// apply the effect status.
	//------------------------------------------------------------------
	if (pPlayerStatus != NULL && pPlayerStatus->GetEFFECT_STAT()!=EFFECTSTATUS_NULL)
	{
		//int esDelayFrame = ConvertDurationToFrame( g_pPlayer->GetDURATION() );

		// attach the effect.
		CreatureState::AddEffectStatus( pPlayerCreature, (EFFECTSTATUS)pPlayerStatus->GetEFFECT_STAT(), delayFrame );
		
		pPlayerStatus->SetStatus( MODIFY_EFFECT_STAT, EFFECTSTATUS_NULL );
	}
	else
	{
		//------------------------------------------------------
		// attach the EffectStatus, if any.
		//------------------------------------------------------
		EFFECTSTATUS es = (*g_pActionInfoTable)[skillID].GetEffectStatus();

		
		if (es!=EFFECTSTATUS_NULL)
		{
			CreatureState::AddEffectStatus( pPlayerCreature, es, delayFrame );
		}
	}


	__END_CATCH
}

//-------------------------------------------------------------------------------
// Make Item To Inventory
//-------------------------------------------------------------------------------
// Handles another item appearing in the inventory when a skill is used on pItem.
//
// On success the new item
// (or, piled on an item already there, that item) is returned.
//-------------------------------------------------------------------------------
MItem*
PacketSkillToMakeItem(MItem* pItem,
						  ITEM_CLASS itemClass,
						  int itemType,
						  int itemID,
						  int x, int y, 
						  int targetX, int targetY)
{
	if (pItem==NULL || g_pInventory==NULL)
	{
		return NULL;
	}

	//----------------------------------------------------
	// one left, or
	// the same place.. remove it completely.
	//----------------------------------------------------
	if (pItem->GetNumber()==1 
		|| (x==targetX && y==targetY) )
	{
		MItem* pRemovedItem = g_pInventory->RemoveItem( x, y );

		if (pRemovedItem!=NULL)
		{
			delete pRemovedItem;
		}
		else
		{
			DEBUG_ADD_FORMAT_ERR("[Error] No Removed Item from Inventory=(%d, %d)", x, y);
		}
	}
	//----------------------------------------------------
	// several: only one fewer.
	//----------------------------------------------------
	else
	{
		//if (pItem->GetNumber() > 1)								
		pItem->SetNumber( pItem->GetNumber() - 1 );
	}

	MItem* pTargetItem = g_pInventory->GetItem( targetX, targetY );

	//----------------------------------------------------
	// no item where it piles
	//----------------------------------------------------
	if (pTargetItem==NULL)
	{							
		MItem* pResultItem = MItem::NewItem( itemClass );

		if (pResultItem == NULL)
		{
			DEBUG_ADD_FORMAT_ERR("[Error] PacketSkillToMakeItem: invalid item class %d", itemClass);
			return NULL;
		}

		pResultItem->SetID( itemID );
		pResultItem->SetItemType( itemType );
		//pHolyWater->SetItemOption(
		pResultItem->SetGridXY( targetX, targetY );

		if (!g_pInventory->AddItem( pResultItem, targetX, targetY ))
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Cannot Add pResultItem to Inventory=(%d, %d)", targetX, targetY);
			
			delete pResultItem;
		}
		else
		{
			return pResultItem;
		}
	}
	//----------------------------------------------------
	// an item where it piles
	//----------------------------------------------------	
	else
	{
		// check.. probably not needed..
		if (pTargetItem->GetItemClass()==itemClass
			&& pTargetItem->GetItemType()==itemType
			&& pTargetItem->GetID()==static_cast<TYPE_OBJECTID>(itemID))
		{
			// the max check is skipped..
			pTargetItem->SetNumber( pTargetItem->GetNumber() + 1 );

			return pTargetItem;
		}
		else
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Wrong Item. tcls=%d, ttype=%d, tid=%d, type=%d, id=%d",
											pTargetItem->GetItemClass(), 
											pTargetItem->GetItemType(), 
											pTargetItem->GetID(),
											itemType,
											itemID);
		}
	}

	return NULL;
}