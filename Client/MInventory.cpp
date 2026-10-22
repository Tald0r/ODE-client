//----------------------------------------------------------------------
// MInventory.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MItem.h"
#include "MInventory.h"

// The player's affect check and the landing sound reach the executable
// through MItem's host (docs/reviews/RESTRUCTURING.md task 4.3).

//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
MInventory*		g_pInventory = NULL;

//----------------------------------------------------------------------
//
// constructor / destructor
//
//----------------------------------------------------------------------
MInventory::MInventory()
{
}

MInventory::~MInventory()
{
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Check AffectStatus
//----------------------------------------------------------------------
// 사용가능한지 체크
//----------------------------------------------------------------------
void			
MInventory::CheckAffectStatus(MItem* pItem)	// one item
{
	MItem::RefreshAffect( pItem );
}

//----------------------------------------------------------------------
// Add item ( pItem )
//----------------------------------------------------------------------
// Inventory에 추가될 수 있는 Item인지 보고,..
// 적절한 grid에 추가한다.
//----------------------------------------------------------------------
bool			
MInventory::AddItem(MItem* pItem)
{
	if (pItem->IsInventoryItem())
	{
		if (MGridItemManager::AddItem( pItem ))
		{
			// placed: play its inventory sound
			MItem::PlayItemSound( pItem->GetInventorySoundID() );
		
						
			return true;
		}

		return false;
	}

	return false;
}

//----------------------------------------------------------------------
// Add item ( pItem )
//----------------------------------------------------------------------
// Inventory에 추가될 수 있는 Item인지 보고,..
// grid(x,y)에 pItem을 추가한다.
//----------------------------------------------------------------------
bool			
MInventory::AddItem(MItem* pItem, BYTE x, BYTE y)
{
	if (pItem->IsInventoryItem())
	{
		if (MGridItemManager::AddItem( pItem, x, y ))
		{
			// placed: play its inventory sound
			MItem::PlayItemSound( pItem->GetInventorySoundID() );

			return true;
		}

		return false;
	}

	return false;
}

//----------------------------------------------------------------------
// Replace item ( pItem,  x,y,  pOldItem )
//----------------------------------------------------------------------
// Inventory에 추가될 수 있는 Item인지 보고,..
// 추가될 수 있으면 추가하는데
// 그 위치에 다른 item이 있다면 pOldItem에 담아서 넘겨준다.
//----------------------------------------------------------------------
bool			
MInventory::ReplaceItem(MItem* pItem, BYTE x, BYTE y, MItem*& pOldItem)
{
	if (pItem->IsInventoryItem())
	{
		if (MGridItemManager::ReplaceItem(pItem, x,y, pOldItem))
		{
			// placed: play its inventory sound
			MItem::PlayItemSound( pItem->GetInventorySoundID() );

			return true;
		}
		
		return false;
	}

	return false;
}

//----------------------------------------------------------------------
// Get Fit Position ( pItem, point )
//----------------------------------------------------------------------
// pItem이 들어갈 수 있는 적절한 grid위치를 구한다.
//
// 일단, 쌓여서 중복될 수 있는 Item인 경우를 체크해야한다.
// 개수가 한계치를 넘어서 완전 쌓이는게 불가능할 경우는 빈 공간을
// 찾으면 된다.
//----------------------------------------------------------------------
bool			
MInventory::GetFitPosition(MItem* pItem, POINT& point)
{
	//--------------------------------------------------------
	// 쌓이는 item인 경우만 체크한다.
	//--------------------------------------------------------
	if (pItem->IsPileItem())
	{
		// 모든 Item을 체크하면서 쌓일 수 있는지를 체크한다.
		// 찾는 순서는?? -_-;;
		// ID순.. 흠.. -_-;;;
		ITEM_MAP::iterator iItem = m_mapItem.begin();

		while (iItem != m_mapItem.end())
		{
			MItem* pInventoryItem = (*iItem).second;

			//--------------------------------------------
			// 완전하게 쌓일 수 있는 조건이 되면...
			// 기존의 item의 좌표를 넘겨준다.
			//--------------------------------------------
			if (pInventoryItem->GetItemClass()==pItem->GetItemClass()
				&& pInventoryItem->GetItemType()==pItem->GetItemType()
				&& pInventoryItem->GetNumber() + pItem->GetNumber() <= pItem->GetMaxNumber()
				&& !pInventoryItem->IsQuestItem())
			{
				point.x = pInventoryItem->GetGridX();
				point.y = pInventoryItem->GetGridY();

				return true;
			}

			iItem++;
		}
	}

	return MGridItemManager::GetFitPosition( pItem, point );
}

//----------------------------------------------------------------------
// Find Item  (class , type)
//----------------------------------------------------------------------
// inventory에서 적절한 itemClass와 itemType을 가진 item을 하나 찾는다.
// itemType은 지정안 할 수도 있다.
// 그리고, 하나만 찾으면 되므로... 가장 먼저 발견되는걸 넘겨주면 된다.
//----------------------------------------------------------------------
MItem*	
MInventory::FindItem( ITEM_CLASS itemClass, TYPE_ITEMTYPE itemType )
{
	ITEM_MAP::iterator iItem = m_mapItem.begin();

	//------------------------------------------------------
	// itemType은 지정하지 않은 경우
	//------------------------------------------------------
	if (itemType==ITEMTYPE_NULL)
	{
		while (iItem != m_mapItem.end())
		{
			MItem* pItem = (*iItem).second;

			// class만 비교
			if (pItem->GetItemClass()==itemClass)
			{
				return pItem;
			}

			iItem++;
		}
	}
	//------------------------------------------------------
	// class와 type 모두 비교
	//------------------------------------------------------
	else
	{
		while (iItem != m_mapItem.end())
		{
			MItem* pItem = (*iItem).second;

			// class와 type 모두 비교
			if (pItem->GetItemClass()==itemClass
				&& pItem->GetItemType()==itemType)
			{
				return pItem;
			}

			iItem++;
		}
	}

	return NULL;
}

