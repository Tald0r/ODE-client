//----------------------------------------------------------------------
// MItemFinder.h
//----------------------------------------------------------------------
// Used to find a particular item.
// Derive from MItemFinder for what is needed.
//
// (Made only once it was needed;
// older code keeps its own searches.)
//----------------------------------------------------------------------

#ifndef __MITEMFINDER_H__
#define __MITEMFINDER_H__

#include "MItem.h"

//----------------------------------------------------------------------
// MItemFinder
//----------------------------------------------------------------------
class MItemFinder {
	public :
		virtual bool operator () ( const MItem* pItem ) = 0;
};

//----------------------------------------------------------------------
// MItemClassFinder
//----------------------------------------------------------------------
// Finds an item of the item class.
//----------------------------------------------------------------------
class MItemClassFinder : public MItemFinder {
	public :
		MItemClassFinder(ITEM_CLASS itemClass)		{ m_ItemClass = itemClass; }
		
		void	SetItemClass(ITEM_CLASS itemClass)	{ m_ItemClass = itemClass; }

		bool	operator () ( const MItem* pItem )
		{
			return pItem->GetItemClass()==m_ItemClass;
		}

	protected :
		ITEM_CLASS	m_ItemClass;	// the item class looked for
};

//----------------------------------------------------------------------
// MItemClassTypeFinder
//----------------------------------------------------------------------
// Finds an item of the item class and type.
//----------------------------------------------------------------------
class MItemClassTypeFinder : public MItemClassFinder {
	public :
		MItemClassTypeFinder(ITEM_CLASS itemClass, TYPE_ITEMTYPE itemType) : MItemClassFinder(itemClass)
		{ 
			m_ItemType = itemType;
		}
		
		void	SetItemType(TYPE_ITEMTYPE itemType)	{ m_ItemType = itemType; }

		bool	operator () ( const MItem* pItem )
		{
			return pItem->GetItemClass()==m_ItemClass
					&& pItem->GetItemType()==m_ItemType;
		}

	protected :
		TYPE_ITEMTYPE	m_ItemType;	// the item type looked for
};

//----------------------------------------------------------------------
// MVampirePortalItemFinder
//----------------------------------------------------------------------
// Finds a vampire portal item, marked or not.
//----------------------------------------------------------------------
class MVampirePortalItemFinder : public MItemFinder {
	public :
		MVampirePortalItemFinder(bool bMarked)
		{
			m_bMarked = bMarked;
		}

		bool	operator () ( const MItem* pItem )
		{
			// The portal item's vtable is executable-side: its mark comes
			// through MItemHost.
			return pItem->GetItemClass()==ITEM_CLASS_VAMPIRE_PORTAL_ITEM
					&& MItem::IsPortalMarked(pItem)==m_bMarked;
		}

	protected :
		bool	m_bMarked;
};

//----------------------------------------------------------------------
// MSlayerPortalItemFinder
//----------------------------------------------------------------------
// Finds one that can be used.
//----------------------------------------------------------------------
class MSlayerPortalItemFinder : public MItemFinder {
	public :
		MSlayerPortalItemFinder() {}

		bool	operator () ( const MItem* pItem )
		{
			return pItem->GetItemClass()==ITEM_CLASS_SLAYER_PORTAL_ITEM
					&& pItem->GetNumber() > 0;
		}
};

//----------------------------------------------------------------------
// MEventStartItemFinder
//----------------------------------------------------------------------
// How many items of the class and type there are..
class MItemClassTypeNumberFinder : public MItemClassTypeFinder {
	public :
		MItemClassTypeNumberFinder(ITEM_CLASS itemClass, TYPE_ITEMTYPE itemType) 
			: MItemClassTypeFinder(itemClass, itemType)	
		{
			m_TotalNumber = 0;
		}		
	
		bool	operator () ( const MItem* pItem )
		{
			// The same class and type: add its number.
			if (pItem->GetItemClass()==m_ItemClass
					&& pItem->GetItemType()==m_ItemType)
			{
				m_TotalNumber += pItem->GetNumber();
			}

			// (So the search goes on.)
			return false;
		}

		void	Clear()						{ m_TotalNumber = 0; }
		int		GetTotalNumber()	const	{ return m_TotalNumber; }


	protected :
		int				m_TotalNumber;
};

//----------------------------------------------------------------------
// MItemClassRangeTypeFinder
//----------------------------------------------------------------------
// Finds an item of the item class and a type from min to max.
//----------------------------------------------------------------------
class MItemClassRangeTypeFinder : public MItemClassFinder {
	public :
		MItemClassRangeTypeFinder(ITEM_CLASS itemClass, TYPE_ITEMTYPE minItemType, TYPE_ITEMTYPE maxItemType) : MItemClassFinder(itemClass)
		{ 
			m_MinItemType = minItemType;
			m_MaxItemType = maxItemType;
		}
		
		void	SetItemType(TYPE_ITEMTYPE minItemType, TYPE_ITEMTYPE maxItemType)	{ m_MinItemType = minItemType;	m_MaxItemType = maxItemType; }

		bool	operator () ( const MItem* pItem )
		{
			return pItem->GetItemClass()==m_ItemClass
					&& pItem->GetItemType()>=m_MinItemType
					&& pItem->GetItemType()<=m_MaxItemType;
		}

	protected :
		TYPE_ITEMTYPE	m_MinItemType;	// the lowest item type looked for
		TYPE_ITEMTYPE	m_MaxItemType;	// the highest item type looked for
};

//----------------------------------------------------------------------
// MOustersSummonGemItemFinder 
//----------------------------------------------------------------------
//----------------------------------------------------------------------
class MOustersSummonGemItemFinder : public MItemFinder {
	public :
		MOustersSummonGemItemFinder() {}

		bool	operator () ( const MItem* pItem )
		{
			return pItem->GetItemClass()==ITEM_CLASS_OUSTERS_SUMMON_ITEM
					&& pItem->GetNumber() > 0;
		}
};

//----------------------------------------------------------------------
// MEventGiftBoxItemFinder 
//----------------------------------------------------------------------
//----------------------------------------------------------------------
class MEventGiftBoxItemFinder : public MItemFinder {
	public :
		MEventGiftBoxItemFinder() {}

		bool	operator () ( const MItem* pItem )
		{
			return pItem->GetItemClass()==ITEM_CLASS_EVENT_GIFT_BOX
					&& pItem->GetItemType() > 1 && pItem->IsTrade();
		}
};


#endif