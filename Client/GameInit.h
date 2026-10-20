//-----------------------------------------------------------------------------
// GameInit.h
//-----------------------------------------------------------------------------
// GameInit.cpp: the start-up and the teardown. InitGame brings up the
// logging, the file definitions, the string tables and the message logs,
// then the input, the display, the game objects (with the hosts the
// libraries ask the game through), the surfaces and the update loops;
// ReleaseAllObjects undoes it. The title picture shown while it loads,
// the sound, the connection to the login server and the skill tree are
// here too.
//-----------------------------------------------------------------------------

#ifndef	__GAMEINIT_H__
#define	__GAMEINIT_H__

#include "../basic/Platform.h"

class CSprite;

// GameInit.cpp's functions (Client.h, the executable's, declares some of
// them too). The whole start-up, and the whole teardown.
BOOL		InitGame();
void		ReleaseAllObjects();

// The steps of InitGame, in the order it takes them after the string
// tables (InitClientConfig, InitInfomation and InitUserOption are
// GameInitInfo.h's).
BOOL		InitVolume();
BOOL		InitInput();
BOOL		InitDraw();
BOOL		InitGameObject();
BOOL		InitSurface();
BOOL		InitGameUpdate();

// The sound (GameMain's mode changes start it), the connection to the
// login server (the UI's messages open it), the skill tree (a zone's
// loading builds it), and what ReleaseAllObjects undoes them with.
BOOL		InitSound();
void		UnInitSound();
BOOL		InitSocket();
void		ReleaseSocket();
void		InitSkillTree();
void		ReleaseGameUpdate();

// The lines each message log InitGame makes keeps (Client.h's until task
// 4.264).
#define	MAX_SYSTEMMESSAGE	5
#define MAX_PLAYERMESSAGE	5
#define	MAX_GAMEMESSAGE		5
#define	MAX_HELPMESSAGE		5

// The title picture shown while the game loads.
void		StartTitleLoading();
void		DrawTitleLoading();
void		EndTitleLoading(bool SendLogin = false);
extern bool			g_bTitleLoading;
extern CSprite*		g_pTitleLoadingSprite;

// The add-on sprites' loading in parts: both answer true at once (their
// bodies were never reached).
bool		PrepareLoadingAddonSPK();
bool		LoadingAddonSPK(bool bLoadingAll);

#endif
