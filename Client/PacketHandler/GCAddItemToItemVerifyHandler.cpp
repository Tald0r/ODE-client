//--------------------------------------------------------------------------------
//
// Filename    : GCAddItemToItemVerifyHandler.cpp
// Written By  : elca, Reiot
// Description :
//
//--------------------------------------------------------------------------------
#include "Client_PCH.h"
// include files
#include "Gpackets/GCAddItemToItemVerify.h"
#include "CommunicationPresentationHost.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "MGameStringTable.h"
#include "MInventory.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "TempInformation.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
void GCAddItemToItemVerifyHandler::execute ( GCAddItemToItemVerify * pPacket , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;
	__BEGIN_DEBUG
		
	MItem *pInventoryItem = NULL;

	MItem *pItem[2] = { NULL, NULL };

	if(g_pTempInformation->GetMode() == TempInformation::MODE_ITEM_TO_ITEM)
	{
		pInventoryItem = g_pInventory->GetItem(static_cast<BYTE>(g_pTempInformation->Value1), static_cast<BYTE>(g_pTempInformation->Value2));
		g_pTempInformation->SetMode(TempInformation::MODE_NULL);
	} else if( g_pTempInformation->GetMode() == TempInformation::MODE_ITEM_MIXING )
	{
		pItem[0] = g_pInventory->GetItem( static_cast<BYTE>(g_pTempInformation->Value1), static_cast<BYTE>(g_pTempInformation->Value2) );
		pItem[1] = g_pInventory->GetItem( static_cast<BYTE>(g_pTempInformation->Value3), static_cast<BYTE>(g_pTempInformation->Value4) );

		g_pTempInformation->SetMode( TempInformation::MODE_NULL);
	}		
	else
		DEBUG_ADD_ERR("[ERROR] TempInformationMode");


	switch(pPacket->getCode())
	{
		case ADD_ITEM_TO_ITEM_VERIFY_ERROR:
			// ui error dialog
			CommunicationPresentation::PopupMessage( STRING_ERROR_ETC_ERROR );
			DialogPresentation::UnlockItem();
			PlayerState::ClearItemCheckBuffer();
			break;

		case ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_IMPOSSIBLE:
			// ui error dialog
			CommunicationPresentation::PopupMessage( STRING_MESSAGE_ITEM_TO_ITEM_IMPOSIBLE );
			DialogPresentation::UnlockItem();
			PlayerState::ClearItemCheckBuffer();
			break;

		case ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_FAIL_CRASH:
			// The item may be gone (no request remembered, or its cell empty).
			if( pInventoryItem == NULL )
				return;
			//----------------------------------------------------
			// the skill's attempt
			//----------------------------------------------------
			WorldEvents::AddNewInventoryEffect( pInventoryItem->GetID(),
				MAGIC_ENCHANT_REMOVE_ITEM,
				0, 0
				);
			// a sound??
			WorldEvents::PlaySound(SOUND_XMAS_STAR);
			DialogPresentation::DropItem();
			break;

		case ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_FAIL_DECREASE:
			// The item may be gone (no request remembered, or its cell empty).
			if( pInventoryItem == NULL )
				return;
			{
				DWORD OptionType = pPacket->getParameter();
				
				//----------------------------------------------------
				// the skill's attempt
				//----------------------------------------------------
				WorldEvents::AddNewInventoryEffect( pInventoryItem->GetID(),
					MAGIC_ENCHANT_OPTION_NULL,
					0, OptionType
					);
				
				// a sound??
				WorldEvents::PlaySound(SOUND_XMAS_STAR);
				DialogPresentation::DropItem();
			}
			break;

		case ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_OK:
			// The item may be gone (no request remembered, or its cell empty).
			if( pInventoryItem == NULL )
				return;
			{
				DWORD OptionType = pPacket->getParameter();
				//----------------------------------------------------
				// the skill's attempt
				//----------------------------------------------------
				WorldEvents::AddNewInventoryEffect( pInventoryItem->GetID(),
					MAGIC_ENCHANT_OPTION_PLUS,
					0, OptionType
					);


//				pInventoryItem->SetItemOption(OptionType);
				DialogPresentation::DropItem();

			}
			break;
			// add  by sonic 2006.11.3  三属性
		case ADD_ITEM_TO_ITEM_VERIFY_THREE_ENCHANT_OK:
			// The item may be gone (no request remembered, or its cell empty).
			if( pInventoryItem == NULL )
				return;
			{
				DWORD OptionType = pPacket->getParameter();
				DWORD OptionType2= pPacket->getParameter2();
				//----------------------------------------------------
				// the skill's attempt
				//----------------------------------------------------
				TYPE_ITEM_OPTION Option1;
				TYPE_ITEM_OPTION Option2;
				TYPE_ITEM_OPTION Option3;
				TYPE_ITEM_OPTION oldOption1;
				TYPE_ITEM_OPTION oldOption2;
				TYPE_ITEM_OPTION oldOption3;
				BYTE bTmp[4];
				memcpy(&bTmp[0],&OptionType,4);
				memcpy(&Option1,&bTmp[0],1);
				memcpy(&oldOption1,&bTmp[1],1);
				memcpy(&Option2,&bTmp[2],1);
				memcpy(&oldOption2,&bTmp[3],1);
				memcpy(&bTmp[0],&OptionType2,4);
				memcpy(&Option3,&bTmp[0],1);
				memcpy(&oldOption3,&bTmp[1],1);
				if (Option1!=0)
					pInventoryItem->ChangeItemOption(oldOption1,Option1);
				if (Option2!=0)
					pInventoryItem->ChangeItemOption(oldOption2,Option2);
				if (Option3!=0)
					pInventoryItem->ChangeItemOption(oldOption3,Option3);
				WorldEvents::AddNewInventoryEffect( pInventoryItem->GetID(),
					MAGIC_ENCHANT_OPTION_PLUS,
					0, OptionType
					);


//				pInventoryItem->SetItemOption(OptionType);
				DialogPresentation::DropItem();

			}
			break;
		case ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_IMPOSSIBLE_NO_PREMIUM:
			if(PlayerState::Creature() != NULL && CreatureState::IsSlayer(PlayerState::Creature()))
				CommunicationPresentation::PopupMessage( STRING_MESSAGE_ITEM_TO_ITEM_FAIL_NO_PREMIUM_SLAYER );
			else
				CommunicationPresentation::PopupMessage( STRING_MESSAGE_ITEM_TO_ITEM_FAIL_NO_PREMIUM_VAMPIRE );
			DialogPresentation::UnlockItem();
			PlayerState::ClearItemCheckBuffer();
			break;
		case ADD_ITEM_TO_ITEM_VERIFY_TRANS_OK :				// the sex change worked
			// The item may be gone (no request remembered, or its cell empty).
			if( pInventoryItem == NULL )
				return;
			{
				WorldEvents::AddNewInventoryEffect( pInventoryItem->GetID() ,
					MAGIC_TRANS_ITEM_OK,
					0, 0 );
				DialogPresentation::DropItem();
			}
			break;
		case ADD_ITEM_TO_ITEM_VERIFY_TRANS_IMPOSSIBLE :		// the sex change failed
			{
			}
			break;
		case ADD_ITEM_TO_ITEM_VERIFY_MIXING_OK :
			{
				if( pItem[0] == NULL || pItem[1] == NULL )
					return;
								
				DialogPresentation::OkMixingForge( pPacket->getParameter(), pItem[0], pItem[1] );
				
				g_pTempInformation->SetMode( TempInformation::MODE_NULL );
				g_pTempInformation->Value1 = 0;
				g_pTempInformation->Value2 = 0;
				g_pTempInformation->Value3 = 0;
				g_pTempInformation->Value4 = 0;
				g_pTempInformation->pValue = NULL;
			}
			break;
		case ADD_ITEM_TO_ITEM_VERIFY_DETACHING_OK :	
			if( pInventoryItem == NULL )
				return;

			DialogPresentation::OkRemoveOption( pPacket->getParameter(), pInventoryItem );
			DialogPresentation::DropItem();
			PlayerState::ClearItemCheckBuffer();
			break;
		case ADD_ITEM_TO_ITEM_VERIFY_MIXING_FAILED_SAME_OPTION_GROUP :
			CommunicationPresentation::PopupMessage( STRING_MESSAGE_MIXING_FORGE_FAILED_SAME_OPTION_GROUP );
			break;
			
		case ADD_ITEM_TO_ITEM_VERIFY_ENCHANT_FAIL:
			// The item may be gone (no request remembered, or its cell empty).
			if( pInventoryItem == NULL )
				return;
			{
				WorldEvents::AddNewInventoryEffect( pInventoryItem->GetID() ,
					MAGIC_UNKOWN1,
					0, 0 );
				DialogPresentation::DropItem();
				DialogPresentation::UnlockItem();
				PlayerState::ClearItemCheckBuffer();
			}
			break;

		case ADD_ITEM_TO_ITEM_VERIFY_REVIVAL_OK:
			{
				if( pInventoryItem == NULL )
				return;

				DWORD OptionType = pPacket->getParameter();
				WorldEvents::AddNewInventoryEffect( pInventoryItem->GetID(),
					MAGIC_ENCHANT_OPTION_PLUS,
					0, OptionType
					);
				
	//			pInventoryItem->SetCurrentDurability(pPacket->getParameter());
				DialogPresentation::DropItem();
	//			PlayerState::ClearItemCheckBuffer();
			}
			break;
		case ADD_ITEM_TO_ITEM_VERIFY_CLEAR_OPTION_OK:
			// The item may be gone (no request remembered, or its cell empty).
			if( pInventoryItem == NULL )
				return;
			{
				DWORD OptionType = pPacket->getParameter();
				WorldEvents::AddNewInventoryEffect( pInventoryItem->GetID(),
				MAGIC_ENCHANT_OPTION_NULL,
				0, OptionType
				);
				WorldEvents::PlaySound(SOUND_XMAS_STAR);
//				pInventoryItem->RemoveItemOption(g_pTempInformation->Value3);

				DialogPresentation::DropItem();
				CommunicationPresentation::PopupMessage( STRING_MESSAGE_SUCCESS_CHANGE );
			}
			break;

		// Item grade upgrade succeeded. The server sends the item's new
		// grade as the one parameter (CGAddItemToItemHandler sets it from
		// pItem->getGrade()) and nothing else for this code; the second
		// parameter this case used to read as a new item type was never
		// on the wire (task 2.5 triage, docs/RESTRUCTURING.md).
		case ADD_ITEM_TO_ITEM_VERIFY_UP_GRADE_OK:
			// The item may be gone (no request remembered, or its cell empty).
			if( pInventoryItem == NULL )
				return;
			{
				DWORD grade = pPacket->getParameter();
				pInventoryItem->SetGrade( grade );

				WorldEvents::AddNewInventoryEffect( pInventoryItem->GetID(),
				MAGIC_ENCHANT_OPTION_PLUS,
				0, 0
				);
				WorldEvents::PlaySound(SOUND_XMAS_STAR);
//				pInventoryItem->RemoveItemOption(g_pTempInformation->Value3);

				DialogPresentation::DropItem();
				CommunicationPresentation::PopupMessage( STRING_MESSAGE_SUCCESS_CHANGE );
			}
			break;

		// add by svi 2009-07-15 增加删除属性确认
		
		case ADD_ITEM_TO_ITEM_REMOVE_OPTION_OK:
			// The item may be gone (no request remembered, or its cell empty).
			if( pInventoryItem == NULL )
				return;
			{
				DWORD OptionType = pPacket->getParameter();

				pInventoryItem->RemoveItemOption( static_cast<TYPE_ITEM_OPTION>(OptionType) );
				
				WorldEvents::AddNewInventoryEffect( pInventoryItem->GetID(),
				MAGIC_ENCHANT_OPTION_PLUS,
				0, 0
				);
				WorldEvents::PlaySound(SOUND_XMAS_STAR);
				DialogPresentation::DropItem();
				CommunicationPresentation::PopupMessage( STRING_MESSAGE_SUCCESS_CHANGE );
			}
			break;

	}


	__END_DEBUG
	__END_CATCH
}
