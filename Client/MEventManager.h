//----------------------------------------------------------------------
// MEventManager.h
//----------------------------------------------------------------------

#ifndef __MEVENT_MANAGER_H__
#define __MEVENT_MANAGER_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include "CTypeTable.h"
#include "MEventQueue.h"
/* Explicit path: plain "CDirectDrawSurface.h" resolves to this same
   directory's stale, unmigrated copy (missing m_ddsd) instead of the
   maintained Client/DXLib/CDirectDrawSurface.h. */
#include "DXLib/CDirectDrawSurface.h"
enum EVENTBACKGROUND_ID
{
	EVENTBACKGROUNDID_COSMOS,
	EVENTBACKGROUNDID_OUSTERS_SLAYER,
	EVENTBACKGROUNDID_OUSTERS_VAMPIRE,
	EVENTBACKGROUNDID_QUEST_2,
	EVENTBACKGROUNDID_CLOUD,
	
	EVENTBACKGROUNDID_MAX,
};

class MEventManager : public MEventQueue
{
	public:
		MEventManager();
		~MEventManager();

		bool					AssertEventBackground(EVENTBACKGROUND_ID id);
		CDirectDrawSurface*		GetEventBackground(EVENTBACKGROUND_ID id) {
			return AssertEventBackground(id) ? m_EventBackGround.GetMutable(id) : nullptr;
		}
		
	protected :
		CTypeTable<CDirectDrawSurface>	m_EventBackGround;	// Owned event background images.
		
};

//----------------------------------------------------------------------
// global
//----------------------------------------------------------------------
extern MEventManager* g_pEventManager;

#endif