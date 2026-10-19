#include "Client_PCH.h"
#include "ClientMode.h"

enum CLIENT_MODE	g_Mode = MODE_NULL;
enum CLIENT_MODE	g_ModeNext = MODE_NULL;
int					g_bNeedUpdate = 0;
int					g_LeftPremiumDays = 0;
bool				g_bFamily = false;
int					g_bActiveGame = 0;

// The window's and the server's state the UI reads, the executable's
// until task 4.252: whether the client runs full screen and the game's
// rectangle in the window (Sonic's 2006 1024x768 build added both), the
// dimension of the server the player is on, and whether 3D acceleration
// is on.
BOOL				g_MyFull = TRUE;
RECT				g_GameRect = {799, 599, 800, 600};
int					g_Dimension = 0;
BOOL				g_bEnable3DHAL = FALSE;

// The game's window, whether it fills the screen, and the machine's MAC
// address (the executable's until task 4.253, which the packet helpers'
// Windows screen-mode check reads).
HWND				g_hWnd = NULL;
bool				g_bFullScreen = false;
BYTE				g_macAddress[6] = { 0, };

// The loop's and the screen's state the waiting screens' update
// (CWaitUIUpdate.cpp) reads and writes, the executable's until task 4.255:
// the frames drawn, how many milliseconds a logic tick waits, whether the
// application is active, the double-click interval, the title picture's
// fading alpha (32 until the fade starts), and the back buffer and the
// last frame.
int					g_FrameCount = 0;
int					g_UpdateDelay = 0;
BOOL				g_bActiveApp = FALSE;
DWORD				g_double_click_time = 0;
int					g_TitleSpriteAlpha = 32;
CSpriteSurface*		g_pBack = NULL;
CSpriteSurface*		g_pLast = NULL;

// What the UI's messages (UIMessageManager.cpp) read and write, the
// executable's until task 4.258: whether the game is in watch mode, and
// the login's sequence numbers.
bool				g_bWatchMode = false;
DWORD				g_dwSeqNumL = 0;
DWORD				g_dwSeqNumG = 0;
