/*-----------------------------------------------------------------------------

	Client_PCH.h

	The common header for the game's translation units: the executable,
	VS_UI, the packet layer and the game model. CMake precompiles it
	privately for DarkEden and VS_UI, using each target's own options;
	the other libraries continue to include it normally. It replaces
	the original VC6 precompiled header, and since 2026-09-08 it also
	replaces the second copy that lived in VS_UI/ (that file is a
	wrapper around this one now, keeping only VS_UI's warning pragmas).
	The two had drifted: only this one defined __GAME_CLIENT__, which
	changed Packet's virtual set at the time, so the two VS_UI
	translation units that include packet classes saw a different
	vtable layout from the library that defines them. Which copy a
	translation unit got was decided by include-path order. The macro
	itself is gone since task 5.2's tenth slice (2026-09-13): every
	conditional on it had one value in every translation unit, and
	ratchet R15 keeps the tree at zero live tokens of it and of the
	four server macros the shared sources once switched on (comments
	such as this one are not counted).

	2025.01.14, merged 2026-09-08

-----------------------------------------------------------------------------*/

#ifndef __CLIENT_PCH_H__
#define __CLIENT_PCH_H__

/* Standard C and C++ library, ahead of Platform.h. On Windows Platform.h
   brings <windows.h>, whose min and max macros are then live while any
   later standard header is parsed; MSVC's STL survives that (it spells
   them (min)()), but the old VS_UI PCH parsed the STL first and the
   merged one should not narrow that. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdarg>
#include <cassert>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <deque>
#include <bitset>
#include <algorithm>
#include <fstream>
#include <iostream>

/* Platform types. Platform.h detects the platform from the compiler,
   defines PLATFORM_WINDOWS / PLATFORM_LINUX / PLATFORM_MACOS /
   PLATFORM_POSIX, and on Windows includes <windows.h> with
   WIN32_LEAN_AND_MEAN, which keeps <mmsystem.h> and the DirectX-adjacent
   headers out - basic/AudioTypes.h and DXLib/CDirectDraw.h define
   SDL-backed stand-ins for those type names. */
#include "../basic/Platform.h"

#ifdef PLATFORM_WINDOWS
	#include <io.h>
	#include <fcntl.h>
#else
	#include <unistd.h>
#endif

/* Keep the legacy shorthand without importing every C++20 library name. */
using std::cout;
using std::endl;
using std::ifstream;
using std::ios;
using std::ofstream;
using std::string;
using std::vector;
using std::map;
using std::list;
using std::deque;
using std::bitset;

/* The game's window and sector geometry, and the server's dimension and
   3D acceleration, read by VS_UI and defined in gamemodel (ClientMode.cpp,
   ViewSectors.cpp), and the frame count, the tick delay, whether the
   application is active, the double-click interval, the title's alpha
   and the back and last surfaces, which the waiting screens' update
   reads (ClientMode.cpp, task 4.255). Declared here so VS_UI's
   translation units, which have no header of their own for them, see
   one declaration. */
extern BOOL g_MyFull;
extern RECT g_GameRect;
extern int g_Dimension;
extern BOOL g_bEnable3DHAL;
extern HWND g_hWnd;
extern bool g_bFullScreen;
extern BYTE g_macAddress[6];
class CSpriteSurface;
extern int g_FrameCount;
extern int g_UpdateDelay;
extern BOOL g_bActiveApp;
extern DWORD g_double_click_time;
extern int g_TitleSpriteAlpha;
extern CSpriteSurface* g_pBack;
extern CSpriteSurface* g_pLast;
extern	LONG g_SECTOR_WIDTH;
extern	LONG g_SECTOR_HEIGHT;
extern	LONG g_SECTOR_WIDTH_HALF;
extern	LONG g_SECTOR_HEIGHT_HALF;
extern	LONG g_SECTOR_SKIP_PLAYER_LEFT;
extern	LONG g_SECTOR_SKIP_PLAYER_UP;

extern	LONG g_TILESURFACE_SECTOR_WIDTH;
extern	LONG g_TILESURFACE_SECTOR_HEIGHT;
extern	LONG g_TILESURFACE_SECTOR_OUTLINE_RIGHT;
extern	LONG g_TILESURFACE_SECTOR_OUTLINE_DOWN;
extern	LONG g_TILESURFACE_WIDTH;
extern	LONG g_TILESURFACE_HEIGHT;
extern	LONG g_TILESURFACE_OUTLINE_RIGHT;
extern	LONG g_TILESURFACE_OUTLINE_DOWN;

/* Exception handling for Packet system */
#include "Packet/Exception.h"

#endif /* __CLIENT_PCH_H__ */
