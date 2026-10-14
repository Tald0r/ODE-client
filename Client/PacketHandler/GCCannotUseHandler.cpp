//////////////////////////////////////////////////////////////////////
//
// Filename    : GCCannotUseHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCCannotUse.h"
#include "CommunicationPresentationHost.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "CMessageArray.h"
#include "GameMessages.h"
#include "MGameStringTable.h"
#include "MItemFinder.h"
#include "MItemManager.h"
#include "PlayerStateHost.h"
#include "TempInformation.h"
#include "ZoneStateHost.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCCannotUseHandler::execute ( GCCannotUse * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;

	// The player; without one there is nothing to do.
	MCreature* const pPlayerCreature = PlayerState::Creature();
	if (pPlayerCreature == NULL)
	{
		return;
	}
		

	
	if(PlayerState::GetWaitVerify() == PlayerState::WAIT_VERIFY_LOVE_CHAIN)
	{
		TYPE_OBJECTID packetObjectID = pPacket->getObjectID();
		ITEM_CLASS		ItemClass;

		switch(PlayerState::GetRace() )
		{
		case RACE_SLAYER :
			ItemClass = ITEM_CLASS_COUPLE_RING;
			break;
		case RACE_VAMPIRE :
			ItemClass = ITEM_CLASS_VAMPIRE_COUPLE_RING;
			break;
		default :
			ItemClass = ITEM_CLASS_COUPLE_RING;
			break;
		}

		MItemManager* pGear = PlayerState::GetGear();
		MItemClassFinder itemFinder( ItemClass );

//		const MItem *pItem = pGear->GetItem(packetObjectID);
		MItem *pItem = pGear->FindItem( itemFinder );
		if(pItem != NULL && pItem->GetID() == packetObjectID)
		{
			PlayerState::ClearItemCheckBuffer();			
			PlayerState::SetWaitVerifyNULL();
			CreatureState::RemoveEffectStatus( pPlayerCreature, EFFECTSTATUS_LOVE_CHAIN );

//			g_pSystemMessage->AddFormat((*g_pGameStringTable)[UI_STRING_MESSAGE_CANNOT_TRACE].GetString(), (*g_pGameStringTable)[STRING_MESSAGE_COUPLE].GetString());
			g_pSystemMessage->Add((*g_pGameStringTable)[STRING_MESSAGE_COUPLE_CAN_NOT_FIND].GetString());
		}
		return;
	}
	//----------------------------------------------------
	// Read the item awaiting verification.
	//----------------------------------------------------
	MItem* pItem = PlayerState::GetItemCheckBuffer();

	if (pItem!=NULL)
	{
		PlayerState::ITEM_CHECK_BUFFER status =	PlayerState::GetItemCheckBufferStatus();

		//----------------------------------------------------
		// An item use awaiting verification..
		//----------------------------------------------------
		if( status == PlayerState::ITEM_CHECK_BUFFER_ITEM_TO_ITEM )
		{			
			PlayerState::ClearItemCheckBuffer();
			g_pTempInformation->SetMode( TempInformation::MODE_NULL );
			
			if( pItem->GetItemClass() != ITEM_CLASS_CODE_SHEET )
				CommunicationPresentation::PopupMessage( STRING_MESSAGE_FAILED_REMOVE_OPTION );
		} else
		if (status==PlayerState::ITEM_CHECK_BUFFER_USE_FROM_INVENTORY)
		{
			MItem *pItem = PlayerState::GetItemCheckBuffer();
			if(pItem != NULL && pItem->GetItemClass() == ITEM_CLASS_EVENT_TREE &&
				pItem->GetItemType() >= 26 && pItem->GetItemType() <= 28)
				CommunicationPresentation::PopupMessage( STRING_MESSAGE_USE_GUILD_MEMBER_ONLY );

			if(pItem->GetItemClass() == ITEM_CLASS_MIXING_ITEM)
			{
				DialogPresentation::CloseMixingForge();
				CommunicationPresentation::PopupMessage(UI_STRING_MESSAGE_FAIL_MERGE_ITEM);				
			}

			if(pItem->GetItemClass() == ITEM_CLASS_DYE_POTION && pItem->GetItemType() == 48)
			{
				// 1: wearing something, 2: not while in a couple, 3: an odd error
				switch( pPacket->getObjectID() )
				{
				case 1 :
					CommunicationPresentation::PopupMessage(STRING_MESSAGE_CANNOT_CHANGE_SEX_BY_WEAR);
					break;
				case 2 :
					CommunicationPresentation::PopupMessage(STRING_MESSAGE_CANNOT_CHANGE_SEX_BY_COUPLE);
					break;
				default :
					CommunicationPresentation::PopupMessage(STRING_ERROR_ETC_ERROR);
					break;
				}
			}

			// Only the item check buffer is cleared.
			PlayerState::ClearItemCheckBuffer();			
		}
		else if (status==PlayerState::ITEM_CHECK_BUFFER_USE_FROM_QUICKSLOT)
		{
			// Only the item check buffer is cleared.
			PlayerState::ClearItemCheckBuffer();

			//----------------------------------------------------
			// Undo the lock that kept the belt from being taken off.
			//----------------------------------------------------
			DialogPresentation::UnlockGear();
		}
		//----------------------------------------------------
		// Another status?
		//----------------------------------------------------
		else if(status == PlayerState::ITEM_CHECK_BUFFER_USE_FROM_GEAR)
		{
			PlayerState::ClearItemCheckBuffer();			

			// UNDONE: a WAIT after an OK never gets here: the OK has already
			// emptied the CHECK_BUFFER, so the pItem != NULL above fails.
			// That case checks WAIT_VERIFY_LOVE_CHAIN, compares the ObjectID
			// and then does what follows.
			if( pItem->GetItemClass() == ITEM_CLASS_COUPLE_RING ||
				pItem->GetItemClass() == ITEM_CLASS_VAMPIRE_COUPLE_RING)
			{
				g_pSystemMessage->Add((*g_pGameStringTable)[STRING_MESSAGE_COUPLE_CAN_NOT_FIND].GetString());
//				PlayerState::SetWaitVerifyNULL();
//				CreatureState::RemoveEffectStatus( pPlayerCreature, EFFECTSTATUS_LOVE_CHAIN );
			}
		}
		// 2004, 9, 13, sobeit add start - using a quest inventory item failed
		else if(status == PlayerState::ITEM_CHECK_BUFFER_USE_FROM_GQUEST_INVENTORY)
		{
			PlayerState::ClearItemCheckBuffer();
		}
		// 2004, 9, 13, sobeit add end - using a quest inventory item failed
		// 2004, 12, 13, sobeit add start - a failed pet lure with a full inventory used to leave the lock on...-_-
		else if(status == PlayerState::ITEM_CHECK_BUFFER_DROP_TO_CREATURE)
		{
			PlayerState::ClearItemCheckBuffer();
		}
		// 2004, 12, 13, sobeit add end
		else
		{
			DEBUG_ADD_FORMAT_ERR("[Error] ItemCheck Buffer is not Use Status: status=%d", (int)status);
		}

	}
	//----------------------------------------------------
	// No item.. - -;;
	//----------------------------------------------------
	else
	{
		DEBUG_ADD_ERR("[Error] No Item in CheckBuffer");
	}
	

	__END_CATCH
}
