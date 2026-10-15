//----------------------------------------------------------------------
// MEventManager.h - the game's event queue and the background IDs events
// carry.
//----------------------------------------------------------------------
// g_pEventManager is an MEventQueue with the host GameInitInfo installs.
// MEventManager, which added background images, went with task 4.224:
// this port never drew them (see MTopView.cpp).
//----------------------------------------------------------------------
#ifndef __MEVENT_MANAGER_H__
#define __MEVENT_MANAGER_H__

#include "MEventQueue.h"

// An event's background image (MEvent::parameter4).
enum EVENTBACKGROUND_ID
{
	EVENTBACKGROUNDID_COSMOS,
	EVENTBACKGROUNDID_OUSTERS_SLAYER,
	EVENTBACKGROUNDID_OUSTERS_VAMPIRE,
	EVENTBACKGROUNDID_QUEST_2,
	EVENTBACKGROUNDID_CLOUD,
	EVENTBACKGROUNDID_MAX,
};

extern MEventQueue* g_pEventManager;

#endif
