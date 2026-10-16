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

// The tile surface the view (MTopView) draws the zone's tiles into: its
// size in sectors and pixels, and where its outline starts. Client.cpp
// sets them with the g_SECTOR_* ones. Defined in gamemodel (task 4.260).
extern	LONG g_TILESURFACE_SECTOR_WIDTH;
extern	LONG g_TILESURFACE_SECTOR_HEIGHT;
extern	LONG g_TILESURFACE_SECTOR_OUTLINE_RIGHT;
extern	LONG g_TILESURFACE_SECTOR_OUTLINE_DOWN;
extern	LONG g_TILESURFACE_WIDTH;
extern	LONG g_TILESURFACE_HEIGHT;
extern	LONG g_TILESURFACE_OUTLINE_RIGHT;
extern	LONG g_TILESURFACE_OUTLINE_DOWN;
