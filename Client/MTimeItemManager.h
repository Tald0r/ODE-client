//-----------------------------------------------------------------------------
// MTimeItemManager.H									- by sonee
//-----------------------------------------------------------------------------
// MTimeItemManager
//  - records the information for items that carry a time limit.
//  - 2003.04.04
//-----------------------------------------------------------------------------

#ifndef __TIME_ITEM_MANAGER_HEADER__
#define __TIME_ITEM_MANAGER_HEADER__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include "MonotonicClock.h"
#include "MTypeDef.h"
#include <map>

// A deadline: an absolute point on MonotonicClock's clock, at the whole
// second this register counts in.
typedef MonotonicClock::SecondPoint				TIMEITEM_DEADLINE;

class MTimeItemManager : public std::map<TYPE_OBJECTID, TIMEITEM_DEADLINE>
{
public :
	typedef std::map<TYPE_OBJECTID, TIMEITEM_DEADLINE>	TIMEITEM_MAP;

	MTimeItemManager();
	~MTimeItemManager();

	bool	IsExist( TYPE_OBJECTID objectID );
	bool	IsExpired( TYPE_OBJECTID objectID );

	bool	AddTimeItem( TYPE_OBJECTID objectID, DWORD time );			// time is in seconds
	bool	RemoveTimeItem( TYPE_OBJECTID objectID );


	int		GetDay( TYPE_OBJECTID objectID );
	int		GetHour( TYPE_OBJECTID objectID );
	int		GetMinute( TYPE_OBJECTID objectID );
	int		GetSecond( TYPE_OBJECTID objectID );

private :
	// Seconds left on an item, or zero once its deadline has been reached.
	std::chrono::seconds	GetRemainingSeconds( TYPE_OBJECTID objectID );
};

extern MTimeItemManager		*g_pTimeItemManager;

#endif