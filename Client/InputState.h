#pragma once

#include "../basic/Platform.h"

// The mouse and the network state the game loop keeps and the player
// reads (gamemodel's since task 4.247; Client.cpp defined the cursor,
// GameMain.cpp the rest): the cursor on the screen, the mouse buttons
// held, the sector last picked under the cursor, and whether the network
// keeps up.
extern int		g_x;
extern int		g_y;
extern BOOL		g_bLButtonDown;
extern BOOL		g_bRButtonDown;
extern POINT	g_SelectSector;
extern bool		g_bNetStatusGood;
