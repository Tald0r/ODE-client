//-----------------------------------------------------------------------------
// GameMain.h
//-----------------------------------------------------------------------------
// GameMain.cpp: the mode changes, sounds, music and weather, the socket's
// input and output, the mouse and input updates, zone loading and moving,
// and the disconnect screen. What it needs from the program around it goes
// through GameMainHost, which GameInit installs.
//-----------------------------------------------------------------------------

#ifndef	__GAMEMAIN_H__
#define	__GAMEMAIN_H__

#include "../basic/Platform.h"

struct GameMainHost
{
	BOOL (*InitDraw)();				// the display again (MODE_CHANGE_OPTION)
	BOOL (*InitSurface)();			// the surfaces again (MODE_CHANGE_OPTION)
	BOOL (*InitSound)();			// MODE_MAINMENU and MODE_WAIT_SETPOSITION
	void (*ReleaseSocket)();		// the main menu, a wrong log-in, a disconnect
	void (*StopLoadingThread)();	// quitting, a zone load, a disconnect
	void (*SetLoadingThreadPriority)(int priority);	// the loading thread's, around a zone load
};

// Every entry is required. GameInit installs it in InitGameObject, before
// anything can reach GameMain; returns the host it replaces.
const GameMainHost*	SetGameMainHost(const GameMainHost* pHost);

#endif
