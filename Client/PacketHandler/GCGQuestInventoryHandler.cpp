//////////////////////////////////////////////////////////////////////
//
// Filename    : GCGQuestInventoryHandler.cc
// Written By  : elca@ewestsoft.com
// Description :
//
//////////////////////////////////////////////////////////////////////

// include files
#include "Client_PCH.h"
#include "Gpackets/GCGQuestInventory.h"
#include "DebugLog.h"
#include "MItem.h"
#include "VsUiPresentationHost.h"
#include "ZoneStateHost.h"
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
void GCGQuestInventoryHandler::execute ( GCGQuestInventory * pGCGQuestInventory , Player * pPlayer )

{
	__BEGIN_TRY 
	(void)pPlayer;
//		__BEGIN_DEBUG_EX
		

	//------------------------------------------------------
	// The zone is not made yet
	//------------------------------------------------------
	if (!ZoneState::Exists())
	{
		// message
		DEBUG_ADD_ERR("[Error] Zone is Not Init.. yet.");
	}
	//------------------------------------------------------
	// Otherwise
	//------------------------------------------------------
	else
	{
		std::vector<MItem*> QuestItemList;

		std::vector<ItemType_t>	TempItemList = pGCGQuestInventory->getItemList();

		for(int i = 0; static_cast<size_t>(i)<TempItemList.size(); i++)
		{
			ItemType_t TempType = TempItemList[i];

			// The game's factory always makes a quest item; without one (a
			// test binary) the type is skipped.
			MItem* pItem = MItem::NewItem( ITEM_CLASS_GQUEST_ITEM );
			if (pItem == NULL)
			{
				DEBUG_ADD_ERR("[Error] GCGQuestInventory: no quest item");
				continue;
			}
			pItem->SetItemType( TempType );
			pItem->SetCurrentDurability( 1 );	
			QuestItemList.push_back(pItem);
		}
		// The quest window keeps the items; with no one to take them they
		// are freed here.
		if (!VsUiPresentation::UpdateQuestItemInfo(QuestItemList))
		{
			for (MItem* pItem : QuestItemList)
			{
				delete pItem;
			}
		}



	}


//	__END_DEBUG_EX
	__END_CATCH
}
