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
#include "COrderedList.h"
#include "ClientMode.h"
#include "MTypeDef.h"

struct GameMainHost
{
	BOOL (*InitDraw)();				// the display again (MODE_CHANGE_OPTION)
	BOOL (*InitSurface)();			// the surfaces again (MODE_CHANGE_OPTION)
	BOOL (*InitSound)();			// MODE_MAINMENU and MODE_WAIT_SETPOSITION
	void (*ReleaseSocket)();		// the main menu, a wrong log-in, a disconnect
};

// Every entry is required. GameInit installs it in InitGameObject, before
// anything can reach GameMain; returns the host it replaces.
const GameMainHost*	SetGameMainHost(const GameMainHost* pHost);

// For tests: forgets what GameMain remembers between calls (the peer managers'
// next update, the one-time help message, the previous weather), as at
// start-up.
void					ResetGameMainState();

// GameMain.cpp's functions (Client.h, the executable's, declares them too).
// The modes, and the application's activation (Alt+Tab).
void		SetMode(enum CLIENT_MODE mode);
void		CheckActivate(BOOL bActiveGame);
bool		CheckInvalidProcess();

// The socket and the input, once a frame; false: the connection is gone.
bool		UpdateSocketInput();
bool		UpdateSocketOutput();
void		UpdateDisconnected();
void		UpdateInput();
void		UpdateMouse();

// Zones, the player and the game objects.
BOOL		LoadZone(int n);
BOOL		LoadZoneInfo(int n);
void		MoveZone(int n);
BOOL		LoadCreature(int spriteType);
void		ReleaseUselessCreatureSPKExcept(const COrderedList<int>& listUse);
void		ReleaseGameObject();
BOOL		InitPlayer(int x, int y, int dir);
void		Add_GDR_Ghost(int ZoneID);

// Sounds, music and weather.
void		PlaySound(TYPE_SOUNDID soundID, bool repeat, int x, int y);
void		PlaySoundAtPlayer(TYPE_SOUNDID soundID);
void		PlaySoundForce(TYPE_SOUNDID soundID);
void		StopSound(TYPE_SOUNDID soundID);
void		PlayMusicCurrentZone();
void		SetWeather(int weather, int level);
void		SetLightning(DWORD delay);

// The character window's last slot, and the screenshot.
void		SelectLastSelectedCharacter();
void		SaveLastSelectedCharacter(int slot);
void		MakeScreenShot();

// The file's globals.
#define	MAX_NPC				100		// g_MaxNPC's first value (Client.h's until task 4.262)
extern int			g_previousSoundID;
extern int			g_ScreenShotNumber;
extern int			g_MaxNPC;
extern bool			g_bHAL;
extern bool			g_bMusicSW;

#endif
