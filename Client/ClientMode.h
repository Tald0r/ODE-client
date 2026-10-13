#pragma once

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
