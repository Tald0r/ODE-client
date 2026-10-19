#pragma once

#include "../basic/Platform.h"

// The stage the client is in, from the opening to the game. SetMode (the
// executable's) enters a mode with its side effects; g_ModeNext is the
// mode the main loop switches to next. The state is gamemodel's so that
// the replies that read it can be tested.
enum CLIENT_MODE
{
	MODE_NULL = 0,
	MODE_OPENING,
	MODE_MAINMENU,
	MODE_CHANGE_OPTION,	// 3D acceleration <-> 2D
	MODE_NEWUSER,
	MODE_CREATECHARACTER,
	MODE_WAIT_CREATEPCOK,
	MODE_WAIT_DELETEPCOK,
	MODE_SELECTCHARACTER,
	MODE_WAIT_LOGINOK,
	MODE_WAIT_WORLD_LIST,
	MODE_WAIT_SELECT_WORLD,
	MODE_WAIT_SERVER_LIST,
	MODE_WAIT_SELECT_SERVER,
	MODE_LOGIN_WRONG,
	MODE_WAIT_PCLIST,
	MODE_WAIT_SELECTPC,
	MODE_WAIT_RECONNECT,
	MODE_WAIT_UPDATEINFO,
	MODE_WAIT_SETPOSITION,		
	MODE_WAIT_REGISTERPLAYEROK,
	MODE_WAIT_RECONNECT_LOGIN,	// logged out, reconnecting automatically
	MODE_GAME,
	MODE_QUIT
};

extern enum CLIENT_MODE		g_Mode;
extern enum CLIENT_MODE		g_ModeNext;

// Whether the client is quitting to be updated (it is set with MODE_QUIT);
// login replies then show nothing. A BOOL.
extern int					g_bNeedUpdate;

// The account's premium days left, as the login server sends them; 0xFFFE
// is an account that is not a premium one, which the title screen says.
// The client sets 0xFFFF itself for a Netmarble account: nothing to show.
// The title screen and the event notices read it.
extern int					g_LeftPremiumDays;
// Whether the account is on the Family pricing plan.
extern bool					g_bFamily;
// Whether the game's window is the active one (the executable's window
// procedure sets it); the NPC talk box opens only then. A BOOL.
extern int					g_bActiveGame;

// Whether the game is in watch mode (the executable's key toggles it); no
// item is dropped or moved then. The executable's until task 4.258.
extern bool					g_bWatchMode;
// The login's sequence numbers, set when the player logs in (the
// executable's until task 4.258).
extern DWORD				g_dwSeqNumL;
extern DWORD				g_dwSeqNumG;
