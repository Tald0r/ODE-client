//////////////////////////////////////////////////////////////////////
//
// Filename    : GCMakeItemOKHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCMakeItemOK.h"
#include "AffectModifyInfo.h"
#include "DebugLog.h"
#include "DialogPresentationHost.h"
#include "MInventory.h"
#include "MItem.h"
#include "PlayerStateHost.h"
#include "TempInformation.h"

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCMakeItemOKHandler::execute ( GCMakeItemOK * pPacket , Player * pPlayer )

{
	__BEGIN_TRY
	(void)pPlayer;
		


	//------------------------------------------------------------------
	// Check the temp mode
	//------------------------------------------------------------------
	if (g_pTempInformation->GetMode() == TempInformation::MODE_SKILL_MAKE_ITEM)
	{
		g_pTempInformation->SetMode(TempInformation::MODE_NULL);
		//g_pTempInformation->Value1 = itemClass;
		//g_pTempInformation->Value2 = itemType;

		// Let go of the items locked for the skill
		DialogPresentation::UnlockItem();
	}
	else
	{
		DEBUG_ADD_ERR( "[Error] Temp Mode is Not MODE_SKILL_MAKE_ITEM" );
		
		// The packet is applied anyway.
	}
		

	//------------------------------------------------------------------
	//
	// Change Item Num
	//
	//------------------------------------------------------------------
	int size = pPacket->getChangedItemListNum();

	for (int i=0; i<size; i++)
	{
		int itemID = pPacket->popFrontChangedItemListElement();
		int itemNum = pPacket->popFrontChangedItemNumListElement();

		MItem* pItem = ((MItemManager*)g_pInventory)->GetItem( itemID );

		if (pItem!=NULL)
		{
			pItem->SetNumber( itemNum );
		}
	}

	//------------------------------------------------------------------
	//
	// Add Item To Inventory
	//
	//------------------------------------------------------------------	
	MItem* pItem = MItem::NewItem( (ITEM_CLASS)pPacket->getItemClass() );

	if (pItem == NULL)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] GCMakeItemOK: invalid item class %d", pPacket->getItemClass());
		return;
	}

	pItem->SetID( pPacket->getObjectID() );
	pItem->SetItemType( pPacket->getItemType() );
	pItem->SetItemOptionList( pPacket->getOptionType() );
	pItem->SetCurrentDurability( pPacket->getDurability() );
	pItem->SetNumber( pPacket->getItemNum() );

	int invenX = pPacket->getX();
	int invenY = pPacket->getY();	

	if (!g_pInventory->AddItem( pItem, invenX, invenY ))
	{
		// -_-;
		delete pItem;
	}	


	//------------------------------------------------------------------
	//
	// ModifyInfo
	//
	//------------------------------------------------------------------
	// Change the player's status.
	//------------------------------------------------------------------
	MStatus* pStatus = PlayerState::Status();
	if (pStatus != NULL)
	{
		AffectModifyInfo(pStatus, pPacket);
	}


	__END_CATCH
}
