#pragma once

#include "Platform.h"

// The view's size in sectors, and where the player stands in it. Client.cpp
// sets them for the screen resolution at start-up; the fake creatures (and
// the view) read them. Defined in gamemodel (task 4.245); they were
// Client.cpp's.
extern	LONG g_SECTOR_WIDTH;
extern	LONG g_SECTOR_HEIGHT;
extern	LONG g_SECTOR_WIDTH_HALF;
extern	LONG g_SECTOR_HEIGHT_HALF;
extern	LONG g_SECTOR_SKIP_PLAYER_LEFT;
extern	LONG g_SECTOR_SKIP_PLAYER_UP;
