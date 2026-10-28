//----------------------------------------------------------------------
// DrawItemNameNode.h
//----------------------------------------------------------------------
// Information for drawing a String on the screen.
//----------------------------------------------------------------------

#ifndef	__DRAWITEMNAMENODE_H__
#define	__DRAWITEMNAMENODE_H__

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include "../basic/Platform.h"
#endif
#include "MItem.h"

class DRAWITEMNAME_NODE {
	public :
		DRAWITEMNAME_NODE(DWORD dist, MItem* pItem)
		{
			m_Distance	= dist;
			m_ID		= pItem->GetID();
			m_pItem		= pItem;
			// No point until DrawItemNameList places the name, which it can skip.
			m_Rect		= RECT{0, 0, 0, 0};
		}

		//-------------------------------------------------------
		// Get
		//-------------------------------------------------------
		int				GetDistance() const		{ return m_Distance; }		
		TYPE_OBJECTID	GetID() const			{ return m_ID; }
		MItem*			GetItem() const			{ return m_pItem; }		

		//-------------------------------------------------------
		// Rect
		//-------------------------------------------------------
		void			SetRect(const RECT& rect)	{ m_Rect = rect; }
		BOOL			IsPointInRect(int x, int y) const
		{
			if (x >= m_Rect.left && x < m_Rect.right
				&& y >= m_Rect.top && y < m_Rect.bottom)
			{
				return TRUE;
			}

			return FALSE;
		}

	protected :
		DWORD			m_Distance;		// distance from the player
		TYPE_OBJECTID	m_ID;			// Item ID
		MItem*			m_pItem;		// item
		RECT			m_Rect;
};

#endif