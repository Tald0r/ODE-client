#include "ViewSectors.h"

// 800x600's values; Client.cpp sets those of the chosen resolution.
LONG	g_SECTOR_WIDTH           =16 ;
LONG	g_SECTOR_HEIGHT          =25 ;
LONG	g_SECTOR_WIDTH_HALF      =9 ;
LONG	g_SECTOR_HEIGHT_HALF     =13 ;
LONG	g_SECTOR_SKIP_PLAYER_LEFT=-8 ;
LONG	g_SECTOR_SKIP_PLAYER_UP  =-12 ;

// The tile surface the view draws the zone's tiles into, in sectors and
// pixels; 800x600's values too. Client.cpp's until task 4.260.
LONG	g_TILESURFACE_SECTOR_WIDTH         =22;
LONG	g_TILESURFACE_SECTOR_HEIGHT        =31;
LONG	g_TILESURFACE_SECTOR_OUTLINE_RIGHT =19;
LONG	g_TILESURFACE_SECTOR_OUTLINE_DOWN  =28;
LONG	g_TILESURFACE_WIDTH                =1056;
LONG	g_TILESURFACE_HEIGHT               =744;
LONG	g_TILESURFACE_OUTLINE_RIGHT        =912;
LONG	g_TILESURFACE_OUTLINE_DOWN         =672;
