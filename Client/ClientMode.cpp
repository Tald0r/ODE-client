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
