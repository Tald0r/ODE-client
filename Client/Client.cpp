#include "Client_PCH.h"
#include "DisplaySettings.h"
#include "CrtCompat.h"
// EXECryptor include removed (SDL2) - Copy protection no longer needed
// APICheck (legacy WPE anti-cheat probe) removed - see code health review C28



//-----------------------------------------------------------------------------
// Client.cpp
//-----------------------------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500 /* version 5.0 */
#endif /* !_WIN32_WINNT */


#include "DebugInfo.h"
#ifndef _DEBUG
#include "CrashReport.h"
#include "PacketDispatcher.h"
#endif

//
//-----------------------------------------------------------------------------
// Include files
//-----------------------------------------------------------------------------
#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include "../basic/Platform.h"
#endif
#include <stdio.h>
#include <stdarg.h>
#ifdef PLATFORM_WINDOWS
#include <io.h>
#include <direct.h>
#include <crtdbg.h>
#include <process.h>
#else
#include <unistd.h>
#include <limits.h>		// PATH_MAX, for the data-root search in ClientMain
#endif
#include <time.h>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <filesystem>
#include "ClientMain.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "UserOption.h"
#endif
#include "DataPath.h"			// Basic::NormalizeDataPath; Basic::FindDataRoot, the working directory off Windows
#ifndef PLATFORM_WINDOWS
#include "DXLib/DXLibBackend.h"	// dxlib_input_update(), the SDL event pump
extern bool g_bRunning;			// cleared by the pump on SDL_QUIT (DXLibBackendSDL.cpp)
#endif
#include <system_error>
// std::filesystem directory enumeration, in place of the _findfirst /
// _findnext walk CheckLogFile() used to run
// (docs/cpp17-cpp20-compatibility-assessment-2026-09-04.md, priority 6).
#include "DirectoryListing.h"
#include "Client.h"
#include "GameInitInfoHost.h"
#include "PacketFunction.h"
#include "GameObject.h"
#include "VS_UI.h"
#include "MMusic.h"
#include "UIDialog.h"
#include "Updater/UpdateManager.h"
#ifdef PLATFORM_WINDOWS
#include <Commctrl.h>
#endif
#include "AppendPatchInfo.h"
#include "Netmarble.h"
#include "EffectSpriteTypeDef.h"
#include "Packet/Properties.h"
#include "ServerInfo.h"
#include "MTestDef.h"
#include "resource.h"
#include "UserInformation.h"
#include "MFileDef.h"
#include "SoundSetting.h"


// REMOVED: nProtect anti-cheat code (SDL migration - no longer needed)

//yckou
#include "DebugKit.h"

#define DIRECTORY_UPDATE		"Update"

//-----------------------------------------------------------------------------
// Global
//-----------------------------------------------------------------------------

// REMOVED: nProtect anti-cheat code (SDL migration - no longer needed)
// g_hWnd is gamemodel's (ClientMode.cpp) since task 4.253, and g_hInstance
// since task 4.264.
// g_x and g_y are InputState.cpp's (gamemodel, task 4.247).
char				g_CWD[_MAX_PATH];

// FPS (g_FrameNow, the frame clock, is FrameClock.cpp's)
// g_CurrentFrame, the frame count, is FrameClock.cpp's.

// g_FrameCount is ClientMode.cpp's (gamemodel) since task 4.255.
int					g_StartFrameCount	= 0;
MonotonicClock::TimePoint	g_StartTime		= MonotonicClock::Now();
// g_FrameRate and g_bGoodFPS are FrameClock.cpp's (gamemodel) since tasks
// 4.261 and 4.262.
const int			g_FrameGood			= 15;

// StampFrameClock is FrameClock.cpp's (gamemodel) since task 4.264.

// Execute Program --> bActiveApp
// minimize | anotherWnd click--> !ActiveGame
// g_bActiveApp, whether the application is active, is ClientMode.cpp's
// (gamemodel) since task 4.255.
// g_bActiveGame, whether the game's window is the active one, is ClientMode.cpp's.
// g_bNeedUpdate, whether the client is quitting to be updated, is ClientMode.cpp's.

// g_double_click_time is ClientMode.cpp's (gamemodel) since task 4.255.

// g_LeftPremiumDays and g_bFamily, the account's premium days and family
// flag from the login, are ClientMode.cpp's too.

// g_dwSeqNumL and g_dwSeqNumG are ClientMode.cpp's (gamemodel) since task 4.258.

// g_pWebBrowser is Vs_ui.cpp's (VS_UI) since task 4.256.
// g_FutecIP and g_FutecPort, the launcher's login server, are ClientMode.cpp's
// (gamemodel) since task 4.264.
// The advanced actions' frame counts are AdvancementAction.cpp's (gamemodel).

enum DARKEDEN_LANGUAGE
{
	DARKEDEN_KOREAN = 0,
	DARKEDEN_CHINESE,
	DARKEDEN_JAPANESE,
	DARKEDEN_ENGLISH,
	DARKEDEN_TAIWAN,
	
	DARKEDEN_LANGUAGE_MAX
};

// 2004, 7, 14, sobeit add start
struct REALSERVER_INFO
{
	REALSERVER_INFO() { bMode = false;WorldID = 0;}
	bool bMode;		// 0: 수동접속, 1:정상접속
	int WorldID;	// 0: 브란, 1: 펠레스
	MString ID;		// id
	MString Key;	// Key
};
// 2004, 7, 14, sobeit add end

// g_macAddress is gamemodel's (ClientMode.cpp) since task 4.253.
extern void CheckMacScreenMode();
// g_MyFull and g_GameRect are gamemodel's (ClientMode.cpp) since task 4.252.

// Integer factor by which the OS window is larger than the game frame in
// windowed mode. The game still renders at g_GameRect size; the renderer
// letterbox-scales the frame up (spritectl_present_surface) and mouse input
// maps back down (spritectl_window_to_game_coords). Chosen in InitApp as the
// largest factor that fits the desktop work area.
static int g_WindowScale = 1;
// The g_SECTOR_* view sizes are ViewSectors.cpp's (gamemodel, task 4.245), and
// so are the g_TILESURFACE_* tile-surface sizes (task 4.260).
//-----------------------------------------------------------------------------
// define function
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------
// Get Futec Address [Futec수정]
//-----------------------------------------------------------------------
// DarkEden.exe Futec(IP:Port)
//              01234567890123
//-----------------------------------------------------------------------
bool
GetFutecAddress(const char* pStr)
{
	const char * pFutecStr = strstr(pStr, "Futec");
	const char * pLeftParen = strchr(pStr, '(');
	const char * pSeperator = strchr(pStr, ':');
	const char * pRightParen = strchr(pStr, ')');

	if (pFutecStr==NULL
		|| pLeftParen==NULL
		|| pSeperator==NULL
		|| pRightParen==NULL
		|| !(pFutecStr < pLeftParen 
			&& pLeftParen < pSeperator
			&& pSeperator < pRightParen))
		return false;

	char str[20];

	Basic::CopyBounded( g_FutecIP, pLeftParen+1, pSeperator-(pLeftParen+1) );
	Basic::CopyBounded( str, pSeperator+1, pRightParen-(pSeperator+1) );
	g_FutecPort = atoi(str);

	return true;
}

//bool ParsingRealServer(const char* pCommandLine,  int Dimention, REALSERVER_INFO &info);

bool
ParsingRealServer(const char* pCommandLine, int Dimention, REALSERVER_INFO &info)
{
	(void)Dimention;
	if( pCommandLine == NULL )
		return false;
	
	char szTemp[4096];
	if(strlen(pCommandLine) < 16)
		return false;

	const char *pString = strchr(pCommandLine, ' ');
	if(pString == NULL)
	{
		return false;
	}
	
	snprintf(szTemp, sizeof(szTemp), "%s", pString+1);
	
	int argcnt = 0;
	char* token = NULL;
	char* context = NULL;
	char arg2[4][32];
	argcnt = 0;

	token = Basic::Tokenize(szTemp, "|", &context);
	if(NULL == token)
		return false;
	while(token && argcnt < 4)
	{
		snprintf(arg2[argcnt], sizeof(arg2[argcnt]), "%s", token);
		argcnt++;
		token = Basic::Tokenize(NULL, "|", &context);
	}
	// mode|demention|ID|key
	info.bMode = atoi(arg2[0])?1:0;
	info.WorldID = atoi(arg2[1]);
	info.ID = arg2[2];
	info.Key = arg2[3];
	
	return true;
} 
//-----------------------------------------------------------------------------
// Name: InitFail()
// Desc: This function is called if an initialization function fails
//-----------------------------------------------------------------------------
HRESULT InitFailMessage(const char* message)
{
	// 프로그램 중단..
	g_bActiveApp = FALSE;

	ShowCursor( TRUE );
	ShowWindow(g_hWnd, SW_HIDE);

    //ReleaseAllObjects();
	DEBUG_ADD( message );
//add by sonic 2006.4.11
#ifdef PLATFORM_WINDOWS
    DestroyWindow(g_hWnd);
#endif
//end 

    return 0;
}

//-----------------------------------------------------------------------------
// ProgressBar
//-----------------------------------------------------------------------------
bool g_bUseProgressBar = true;
HWND g_hWndProgress = NULL;
const int progressBarWidth = 300;
const int progressBarHeight = 40;
const int g_numAppendFiles = 16;		// AppendPatch.inf의 file들 개수

void
RemoveProgressBar()
{
	if (g_hWndProgress!=NULL)
	{
#ifdef PLATFORM_WINDOWS
		DestroyWindow(g_hWndProgress);
#endif
		g_hWndProgress = NULL;
	}
}

DARKEDEN_LANGUAGE CheckDarkEdenLanguage()
{
	if( g_pFileDef == NULL )
		return DARKEDEN_KOREAN;

	int num = 0;
	// Held, not borrowed: getProperty() returns a temporary, and the
	// pointer taken from it used to dangle by the time fopen() read it.
	// FileDef.inf names the file bare, which getProperty() leaves as
	// spelled, so it is resolved for the disk here (basic/DataPath.h).
	const std::string fileName = Basic::NormalizeDataPath(g_pFileDef->getProperty("FILE_LANGUAGE_INFO"));
	FILE *f = Basic::OpenFile(fileName.c_str(), "r");
	if (f == NULL) goto exit;

	char szLine[512];

	while( fgets( szLine, 512, f) != NULL)
	{
		if( szLine[0] == ';' || strlen( szLine ) <= 0 )
			continue;

		if( strncmp( szLine, "LANGUAGE",8 ) == 0 )
		{
			Basic::ScanString(szLine+8,"%d",&num);
			break;
		}
	}

	fclose(f);

exit:
	return (DARKEDEN_LANGUAGE)(DARKEDEN_KOREAN + num );
}

int			
GetPersnalShopUpdateTime()
{
	CRarFile rarfile;

	rarfile.SetRAR(g_pFileDef->getProperty("FILE_INFO_DATA").c_str(),"darkeden");
	rarfile.Open(g_pFileDef->getProperty("FILE_UPDATE_PERSNALSHOPTIME").c_str() );

	if( !rarfile.IsSet() )
		return 60000;

	char szLine[512];
	int num = 0;

	while( rarfile.GetString( szLine, 512 ) )
	{
		if( strncmp( szLine, "UPDATETIME",10 ) == 0 )
		{
			Basic::ScanString(szLine+10,"%d",&num);
			break;
		}
	}

	rarfile.Release();

	return num;
}
void
CreateProgressBar(const char* str)
{
	if (g_bUseProgressBar)
	{
		RemoveProgressBar();

		InitCommonControls();

		g_hWndProgress = CreateWindowEx(WS_EX_TOPMOST, PROGRESS_CLASS, str, 
										WS_VISIBLE | PBS_SMOOTH, 
										GetSystemMetrics(SM_CXSCREEN)/2 - progressBarWidth/2, 
										GetSystemMetrics(SM_CYSCREEN)/2 - progressBarHeight/2, 
										progressBarWidth, progressBarHeight,
										NULL, (HMENU)0, g_hInstance, NULL); 
	}
}


void
SetProgressBarCount(int count)
{	
	if (!g_bUseProgressBar || g_hWndProgress==NULL)
	{
		return;
	}

	SendMessage(g_hWndProgress, PBM_SETRANGE, 0, MAKELPARAM(0, count)); 
    SendMessage(g_hWndProgress, PBM_SETSTEP, (WPARAM) 1, 0); 
	SendMessage(g_hWndProgress, PBM_SETPOS, (WPARAM) 0, 0); 	
}

void
SetProgressBarText(const char* str)
{
	if (g_bUseProgressBar)
	{
		if (g_hWndProgress==NULL)
		{
			CreateProgressBar(str);
			SetProgressBarCount( g_numAppendFiles );
		}
		else
		{
			SetWindowText( g_hWndProgress, str	);
		}
	}
}



void
UpdateProgressBar()
{
	if (!g_bUseProgressBar || g_hWndProgress==NULL)
	{
		return;
	}

	SendMessage(g_hWndProgress, PBM_STEPIT, 0, 0); 
}


//-----------------------------------------------------------------------------
// Name: WindowProc()
// Desc: The Main Window Procedure
//-----------------------------------------------------------------------------
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
// REMOVED: nProtect anti-cheat code (SDL migration - no longer needed)

		//---------------------------------------------------------------
		//
		//						MM_MCINOTIFY
		//
		//---------------------------------------------------------------
		case MM_MCINOTIFY :
			// A MIDI or video finished playing. Nothing plays one through MCI:
			// the opening video went with task 4.210, and the music replay
			// that used to follow here was already commented out.
		return 0L;

		//---------------------------------------------------------------
		//
		//	WM_SYSCOMMAND
		// 
		//---------------------------------------------------------------
		case WM_SYSCOMMAND:
            // Prevent moving/sizing and power loss in fullscreen mode
            switch( wParam )
            {
				case SC_HOTKEY:
				case SC_KEYMENU:
				case SC_TASKLIST:
				case SC_PREVWINDOW:
				case SC_NEXTWINDOW:
					return 1;

				// SC_CLOSE deliberately NOT blocked: Alt+F4 must always work
				// as the emergency exit - upstream swallowed it in fullscreen,
				// which together with the old topmost window style could trap
				// the user on a wedged fullscreen client with no way out.
				case SC_MOVE:
                case SC_SIZE:
                case SC_MAXIMIZE:
                case SC_MONITORPOWER:
                    if( CSDLGraphics::IsFullscreen() )
                        return 1;
                    break;
            }
        break;//return 0L;



		//---------------------------------------------------------------
		//
		//							WM_ACTIVATEAPP
		//
		//---------------------------------------------------------------
		case WM_ACTIVATEAPP :
		{

            // Pause if minimized or not the top window

			g_double_click_time = GetDoubleClickTime();

			BOOL bActiveGame = (BOOL)wParam;

			{
				CheckActivate(bActiveGame);

				g_bActiveGame = bActiveGame;
			}
		}
        break;//return 0L;

		//---------------------------------------------------------------
		//
		//							WM_DESTROY
		//
		//---------------------------------------------------------------
        case WM_DESTROY:
            // Clean up and close the app            
            PostQuitMessage(0);
        return 0L;

		case WM_GETMINMAXINFO:
		{
            // Fix the size of the window to 640x480 (client size)
            MINMAXINFO* pMinMax = (MINMAXINFO *)lParam;
				// Pinned to the scaled window size (see g_WindowScale). Live
				// resizing stays disabled: the SDL renderer wraps a foreign
				// Win32 window and does not reliably track swapchain resizes.
				pMinMax->ptMinTrackSize.x = g_GameRect.right*g_WindowScale+GetSystemMetrics(SM_CXSIZEFRAME)*2;
				pMinMax->ptMinTrackSize.y = g_GameRect.bottom*g_WindowScale+GetSystemMetrics(SM_CYSIZEFRAME)*2
											   +GetSystemMetrics(SM_CYMENU);
            pMinMax->ptMaxTrackSize.x = pMinMax->ptMinTrackSize.x;
            pMinMax->ptMaxTrackSize.y = pMinMax->ptMinTrackSize.y;
		}
        break;
		
		//---------------------------------------------------------------
		//
		//							WM_MOVE
		//
		//---------------------------------------------------------------
		case WM_MOVE :			
			if (g_bActiveApp && !CSDLGraphics::IsFullscreen())
				CSDLGraphics::OnMove();
		break;

		//---------------------------------------------------------------
		//
		//							WM_KEYDOWN
		//
		//---------------------------------------------------------------
        case WM_KEYDOWN:

			gC_vs_ui.KeyboardControl(message, static_cast<UINT>(wParam), static_cast<long>(lParam));
            // Handle any non-accelerated key commands

			switch (wParam)
			{
				//-----------------------------------------------
				// Screen Shot
				//-----------------------------------------------
				case VK_SCROLL :	
						MakeScreenShot();
					
				return 0L;

			}		
			
		return 0L; 
		case WM_TIMER :			
			if( g_SDLAudio.IsInit() && g_pOGG != NULL )
				g_pOGG->streamUpdate (g_pOGG);
			break;

		//---------------------------------------------------------------
		//
		//							WM_CHAR
		//
		//---------------------------------------------------------------
		case WM_CHAR :
		{
			gC_vs_ui.KeyboardControl(message, static_cast<UINT>(wParam), static_cast<long>(lParam));

		}
		return 0L;	
		// Keep the IME indicator from showing during Korean input.
		//

		//---------------------------------------------------------------
		//
		//				WM_KEYUP
		//				WM_IME_COMPOSITION
		//				WM_IME_STARTCOMPOSITION
		//				WM_IME_ENDCOMPOSITION
		//
		//---------------------------------------------------------------
		case WM_KEYUP:
		 case WM_IME_COMPOSITION:
		 case WM_IME_STARTCOMPOSITION:
		 case WM_IME_ENDCOMPOSITION:
			gC_vs_ui.KeyboardControl(message, static_cast<UINT>(wParam), static_cast<long>(lParam));
			if (message == WM_IME_STARTCOMPOSITION)
				return 0;

			break; // ! break!!!!

    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}


//-----------------------------------------------------------------------------
// Name: InitApp()
// Desc: Do work required for every instance of the application:
//          Create the window, initialize data
//-----------------------------------------------------------------------------
BOOL
InitApp(int nCmdShow)
{
#ifdef PLATFORM_WINDOWS
	WNDCLASS                    wc;
    // Set up and register window class
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = g_hInstance;
    wc.hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_DARKEDEN));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH )GetStockObject(BLACK_BRUSH);
    wc.lpszMenuName = NULL;//NAME;
	wc.lpszClassName = PROGRAM_NAME;
    RegisterClass(&wc);
	
	// Calculate the proper size for the window given a client of 640x480
    int cx,cy;	
	DWORD style = 0;
	DWORD exStyle = 0;

// 2004, 8, 27, sobeit add start - mac address 체크 및, mac screen mode 체크
	memset( g_macAddress, 0, 6*sizeof(BYTE) );
	if(GetMacAddressFromNetBIOS(g_macAddress))
	{
#ifndef _DEBUG
		CheckMacScreenMode();
#endif
	}

// 2004, 8, 27, sobeit add end - mac address 체크 및, mac screen mode 체크
	if (g_bFullScreen)
	{
		// Borderless fullscreen, deliberately NOT topmost: a WS_EX_TOPMOST
		// popup keeps covering the whole screen even when the process hangs
		// or the debugger breaks, leaving alt-tab useless and forcing a
		// reboot. A screen-sized popup covers the desktop just as well while
		// focused, and yields normally on alt-tab and debugger breaks.
		exStyle = WS_EX_APPWINDOW;
		style = WS_POPUP|WS_CLIPCHILDREN;
		cx = GetSystemMetrics(SM_CXSCREEN);
		cy = GetSystemMetrics(SM_CYSCREEN);
	}
	else
	{
		exStyle = WS_EX_APPWINDOW;				//by viva : add WS_SYSMENU
		style = WS_CLIPCHILDREN | WS_OVERLAPPED | WS_THICKFRAME | WS_MINIMIZEBOX | WS_SYSMENU;//WS_OVERLAPPEDWINDOW;
			// Largest integer multiple of the game size that fits the desktop
			// work area - crisp upscale, no gameplay or UI change. Fullscreen
			// (the branch above) gets the fractional sharp-bilinear scale.
			{
				RECT workArea = { 0, 0, 0, 0 };
				SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);

				int frameW = GetSystemMetrics(SM_CXSIZEFRAME)*2;
				int frameH = GetSystemMetrics(SM_CYSIZEFRAME)*2 + GetSystemMetrics(SM_CYMENU);
				int workW = workArea.right - workArea.left;
				int workH = workArea.bottom - workArea.top;

				g_WindowScale = 1;
				while ((g_WindowScale + 1) * g_GameRect.right + frameW <= workW
					&& (g_WindowScale + 1) * g_GameRect.bottom + frameH <= workH)
				{
					g_WindowScale++;
				}
			}

			cx = g_GameRect.right * g_WindowScale + GetSystemMetrics(SM_CXSIZEFRAME)*2;
			cy = g_GameRect.bottom * g_WindowScale + GetSystemMetrics(SM_CYSIZEFRAME)*2+GetSystemMetrics(SM_CYMENU);
	}
    g_hWnd = CreateWindowEx(exStyle,//0,
                          PROGRAM_NAME,
                          PROGRAM_TITLE,
						  style,
                          0,
                          0,
						  cx,
						  cy,
                          NULL,
                          NULL,
                          g_hInstance,
                          NULL);

    if (!g_hWnd)
	{
        return FALSE;
	}
#else
	// No native window off Windows: CSDLGraphics::Init creates the SDL
	// window when InitGame() sets the display mode, and g_hWnd stays NULL.
	// Of the Win32 calls below, ShowCursor drives SDL's cursor through the
	// shim; SetCursor, ShowWindow, UpdateWindow and SetFocus are its no-ops.
#endif

// REMOVED: nProtect anti-cheat code (SDL migration - no longer needed)

	// cursor를 없애준다.
	SetCursor(NULL);
	ShowCursor( FALSE );

	// window를 보여준다.
	ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    SetFocus(g_hWnd);	



	//---------------------------------------------------
	// Init Game
	//---------------------------------------------------
	return InitGame();
}

//-----------------------------------------------------------------------------
// Check TerriblePatch
//-----------------------------------------------------------------------------
bool
CheckTerriblePatch()
{
	//-----------------------------------------------------------------------------
	// ImageObject Patch
	//-----------------------------------------------------------------------------
	AppendPatchTable apt;

	//-----------------------------------------------------------------------------
	// Load
	//-----------------------------------------------------------------------------
	std::ifstream fileAppendInfo;
	if (!FileOpenBinary(g_pFileDef->getProperty("FILE_INFO_APPENDPATCH").c_str(), fileAppendInfo))
		return false;
	apt.LoadFromFile( fileAppendInfo );
	fileAppendInfo.close();

	//-----------------------------------------------------------------------------
	// Append 패치를 한다.
	//-----------------------------------------------------------------------------
	for (int i=0; i<apt.GetSize(); i++)
	{
		if (!apt[i].ExecutePatch())
		{
			break;
		}

		if (!apt[i].CheckFinalInfo())
		{
				break;
		}
		
	}

	return true;
}

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: The Windows entry point. Records the instance handle and hands
//       the rest to ClientMain, which is this function's old body.
//-----------------------------------------------------------------------------
#ifdef PLATFORM_WINDOWS
int PASCAL
WinMain(HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR lpCmdLine,
        int nCmdShow)
{
	(void)hPrevInstance;
	g_hInstance = hInstance;
	return ClientMain(lpCmdLine, nCmdShow);
}
#endif

//-----------------------------------------------------------------------------
// Name: ClientMain()
// Desc: Initialization, frame loop, shutdown - on every platform. This
//       was WinMain's body (ClientMain.h says how it is shared); the
//       Win32-only steps are behind PLATFORM_WINDOWS, with the SDL
//       equivalent in the other branch where one is needed.
//-----------------------------------------------------------------------------
static void FinishClient(HANDLE hMutex, const char* lpCmdLine)
{
	(void)hMutex;
	ReleaseAllObjects();
	delete gC_ci;

	DEBUG_CMD(MIN_HIDEWND, "------------------------------");
	if (g_pClientConfig!=NULL)
	{
		delete g_pClientConfig;
		g_pClientConfig = NULL;
	}

// REMOVED: nProtect anti-cheat code (SDL migration - no longer needed)
	ReleaseMutex( hMutex );

	//----------------------------------------------------------------
	// Updater를 실행시킨다.
	//----------------------------------------------------------------
	if (g_bNeedUpdate)
	{
		char szTemp[512];
		snprintf(szTemp, sizeof(szTemp), "UPDATE %s", lpCmdLine);
		_chdir(g_CWD);
		_spawnl(_P_OVERLAY, UPDATER_FILENAME, UPDATER_FILENAME, szTemp, NULL);
	}
}

static bool RunClientFrame()
{
	MSG msg{};
#ifndef PLATFORM_WINDOWS
	// The SDL event pump: dxlib_input_update() drains the queue into the
	// input state and clears g_bRunning on SDL_QUIT. PeekMessage below
	// is the shim's stub and always says no, so the frame runs.
	dxlib_input_update();
	if (!g_bRunning)
		return false;
#endif
	if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		if (!GetMessage(&msg, NULL, 0, 0))
			return false;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	else if (g_bActiveApp)
	{
		{
			StampFrameClock();

			{
				if (g_pUpdate!=NULL)
				{
					// 노파심.. 으흠.. --;;
					CWinUpdate*	pCurrentUpdate = g_pUpdate;

					pCurrentUpdate->Update();
				}
			}
// REMOVED: nProtect anti-cheat code (SDL migration - no longer needed)

				DWORD timeGap = (DWORD)(g_FrameNow - g_StartTime).count();

				if (timeGap > 1000)
				{
					g_FrameRate = (g_FrameCount - g_StartFrameCount) * 1000 / timeGap;

					// 15 fps 이상
					g_bGoodFPS = (g_FrameRate >= g_FrameGood);

					g_StartTime = g_FrameNow;
					g_StartFrameCount = g_FrameCount;
				}
		}
	}
	else
	{
		// Make sure we go to sleep if we have nothing else to do
		WaitMessage();
	}

	return true;
}

#ifdef __EMSCRIPTEN__
namespace {
bool browserLoopRunning = false;
struct BrowserSession { HANDLE mutex; std::string command; };
void BrowserFrame(void* data)
{
	auto* session = static_cast<BrowserSession*>(data);
	if (RunClientFrame()) return;
	emscripten_cancel_main_loop();
	browserLoopRunning = false;
	if (g_pUserOption && g_pFileDef)
		g_pUserOption->SaveToFile(g_pFileDef->getProperty("FILE_INFO_USEROPTION").c_str());
	FinishClient(session->mutex, session->command.c_str());
	delete session;
	SDL_Quit();
	emscripten_force_exit(0);
}
}
bool ClientHasBrowserLoop() { return browserLoopRunning; }
#endif

int ClientMain(char* lpCmdLine, int nCmdShow)
{
	// Help events' mail (task 4.222).
	static const MHelpHost helpHost = {
		.AddHelpMail = [](HELP_EVENT he) { gC_vs_ui.AddHelpMail(he); },
	};
	SetHelpHost(&helpHost);

	// A refused Netmarble launch (task 4.220).
	static const NetmarbleHost netmarbleHost = {
		.RefuseLaunch = [] {
			MessageBox( NULL, "넷마블 홈페이지를 통해서 다시 실행해주세요.","NetmarbleDarkEden",MB_OK);
			exit(0);
		},
	};
	SetNetmarbleHost(&netmarbleHost);

	// The append patch's progress bar (task 4.212).
	static const AppendPatchHost appendPatchHost = {
		.UpdateProgress = UpdateProgressBar,
		.SetProgressText = SetProgressBarText,
	};
	SetAppendPatchHost(&appendPatchHost);

	// The loading thread's sprite-pack jobs open their files so (task 4.217).
	static const MLoadingSPKHost loadingSPKHost = { .OpenBinary = FileOpenBinary };
	SetLoadingSPKHost(&loadingSPKHost);

	// What the start-up's loading (gameclient's, task 4.252) asks of the
	// executable, installed before FileOpenBinary's first caller
	// (CheckTerriblePatch, then InitGame's string tables).
	static const GameInitInfoHost::Host gameInitInfoHost{
		.SetFadeStart = ::SetFadeStart,
		.InitPacketItemTable = ::InitPacketItemTable,
		.PersonalShopUpdateTime = GetPersnalShopUpdateTime,
		.InitFailMessage = [](const char* message) { InitFailMessage(message); },
		.ShowError = [](const char* message) { MessageBox(NULL, message, "Error", MB_OK); },
	};
	GameInitInfoHost::SetHost(&gameInitInfoHost);

	// Flip presents g_pBack, once it exists (task 4.211).
	CSDLGraphics::SetBackBufferSource([]() -> spritectl_surface_t {
		return g_pBack != NULL ? g_pBack->GetBackendSurface() : SPRITECTL_INVALID_SURFACE;
	});

#ifdef PLATFORM_WINDOWS
	GetModuleFileName(NULL, g_CWD, _MAX_PATH);
	char *tempCut = strrchr(g_CWD, '\\');
	if(tempCut == NULL)
		return FALSE;

	*tempCut = '\0';
	SetCurrentDirectory(g_CWD);
#else
	// The game's data is found relative to the working directory
	// (GameInit.cpp), which on Windows is the executable's own. Off
	// Windows the launcher decides it - a shell gives its own directory,
	// Finder gives "/" - so the data is looked for under the working
	// directory first, then the executable's directory, then, for a
	// macOS bundle (Contents/MacOS/DarkEden), the directory the bundle
	// sits in; the first one that has the file definitions becomes the
	// working directory. Not the bundle's own Resources directory: the
	// game writes beside its data (UserSet/, Log/, the profile
	// directory), which inside a bundle under /Applications fails or
	// breaks the signature. None of them having it is reported and not
	// fatal here: the game goes on to fail at its first open, as it
	// always did, with the working directory it was given. g_CWD records
	// the result for the _chdir(g_CWD) calls below.
	{
		std::vector<std::string> vCandidates;
		vCandidates.push_back(".");

		// PATH_MAX-sized, not _MAX_PATH (260): platform_get_executable_dir
		// refuses a directory its buffer cannot hold rather than
		// truncating, and a deep install would otherwise lose every
		// candidate but "." without a word.
		char szBase[PATH_MAX];
		if (platform_get_executable_dir(szBase, sizeof(szBase)) == 0)
		{
			vCandidates.push_back(szBase);
			vCandidates.push_back(std::string(szBase) + "../../../");
		}
		else
		{
			fprintf(stderr, "ClientMain: cannot determine the executable's directory; looking for the data under the working directory only\n");
		}

		const std::string sRoot = Basic::FindDataRoot(vCandidates, FILE_INFO_FILEDEF);
		if (sRoot.empty())
		{
			// The marker as it is looked for: MFileDef.h spells it with
			// backslashes, which FindDataRoot folds.
			std::string sMarker = FILE_INFO_FILEDEF;
			std::replace(sMarker.begin(), sMarker.end(), '\\', '/');
			fprintf(stderr, "ClientMain: no data tree (%s) under any of:\n", sMarker.c_str());
			for (size_t i = 0; i < vCandidates.size(); i++)
				fprintf(stderr, "    %s\n", vCandidates[i].c_str());
		}
		else if (chdir(sRoot.c_str()) != 0)
		{
			fprintf(stderr, "ClientMain: cannot change to the data directory %s\n", sRoot.c_str());
		}
	}

	if (getcwd(g_CWD, _MAX_PATH) == NULL)
	{
		g_CWD[0] = '.';
		g_CWD[1] = '\0';
	}
#endif

	auto& displaySettings = GetDisplaySettings();
	displaySettings.Load();
	std::string launchCommand = displaySettings.LaunchCommand(lpCmdLine);
	lpCmdLine = launchCommand.data();

	//add by zdj
	bool cmpFullScreen = displaySettings.fullscreen;
	if (lpCmdLine[strlen(lpCmdLine)-1] == '1')
	{
		g_MyFull=false;
		cmpFullScreen = false;
	}
	else if (lpCmdLine[strlen(lpCmdLine)-1] == '2')
	{
		g_MyFull=false;
		cmpFullScreen=true;
	}
	// add by Sonic 2006.9.26
	else if (lpCmdLine[strlen(lpCmdLine)-1] == '3')
	{
		g_MyFull=true;
		cmpFullScreen=false;
		g_GameRect.left=1023;
		g_GameRect.top=767;
		g_GameRect.right=1024;
		g_GameRect.bottom=768;
		g_x=512;
		g_y=384;
		g_SECTOR_WIDTH           = 21;
		g_SECTOR_HEIGHT          = 32;
		g_SECTOR_WIDTH_HALF      = 11;
		g_SECTOR_HEIGHT_HALF     = 17;
		g_SECTOR_SKIP_PLAYER_LEFT= -10;
		g_SECTOR_SKIP_PLAYER_UP  = -16;

		g_TILESURFACE_SECTOR_WIDTH         =27;
		g_TILESURFACE_SECTOR_HEIGHT        =37;
		g_TILESURFACE_SECTOR_OUTLINE_RIGHT =24;
		g_TILESURFACE_SECTOR_OUTLINE_DOWN  =34;
		g_TILESURFACE_WIDTH                =1296;
		g_TILESURFACE_HEIGHT               =888;
		g_TILESURFACE_OUTLINE_RIGHT        =1152;
		g_TILESURFACE_OUTLINE_DOWN         =816;
	}
	else if (lpCmdLine[strlen(lpCmdLine)-1] == '4')
	{
		g_MyFull=true;
		cmpFullScreen=true;
		g_GameRect.left=1023;
		g_GameRect.top=767;
		g_GameRect.right=1024;
		g_GameRect.bottom=768;
		g_x=512;
		g_y=384;
		g_SECTOR_WIDTH           = 21;
		g_SECTOR_HEIGHT          = 32;
		g_SECTOR_WIDTH_HALF      = 11;
		g_SECTOR_HEIGHT_HALF     = 17;
		g_SECTOR_SKIP_PLAYER_LEFT= -10;
		g_SECTOR_SKIP_PLAYER_UP  = -16;

		g_TILESURFACE_SECTOR_WIDTH         =27;
		g_TILESURFACE_SECTOR_HEIGHT        =37;
		g_TILESURFACE_SECTOR_OUTLINE_RIGHT =24;
		g_TILESURFACE_SECTOR_OUTLINE_DOWN  =34;
		g_TILESURFACE_WIDTH                =1296;
		g_TILESURFACE_HEIGHT               =888;
		g_TILESURFACE_OUTLINE_RIGHT        =1152;
		g_TILESURFACE_OUTLINE_DOWN         =816;
	}
	lpCmdLine[strlen(lpCmdLine)-1]=0x0;
	//----------------------------------------------------------
	// Run a single instance of the application (Windows only: the
	// named mutex and the FindWindow checks are Win32; off Windows
	// there is no lock, and the ReleaseMutex calls further down hand
	// the shim's no-op a NULL).
	//----------------------------------------------------------
#ifndef PLATFORM_WINDOWS
	HANDLE hMutex = NULL;
#else
	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = FALSE;
	// 2006.11.07 edit Coffee: allow the program to run twice
	HANDLE hMutex = CreateMutex(&sa, FALSE, "<<<DarkEden>>>");
	// end Coffee 2006.11.07  
	if( FindWindow( "PROCEXPL", "" ) != NULL )
	{
		return -1;
	}
#endif // PLATFORM_WINDOWS
#ifdef PLATFORM_WINDOWS
	// Windows only, all of it: closing a running launcher window,
	// replacing Updater.exe, and the startup DLL whitelist. The
	// whitelist judges *.dll names beside the executable; there are
	// none off Windows and the shared libraries there are the loader's
	// business, not the game's.
//yckou:update the update.exe program
	DWORD pid;
	HANDLE hProcess=NULL,hProcessToken=NULL;
	HWND hwndUpdate = FindWindow("#32770", "FsDarkedenLaunch");
		if(hwndUpdate != NULL)
		{
				pid =0;
				GetWindowThreadProcessId(hwndUpdate, &pid);
				hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
				TerminateProcess(hProcess, 0);
				int a = static_cast<int>(SendMessage(hwndUpdate, WM_CLOSE, 0 , 0));
		}
		Sleep(1000);
	// Existence test only; the error_code overload reports false rather
	// than throwing on an unreadable directory.
	std::error_code ecUpdater2;
	if(std::filesystem::exists("Updater2.exe", ecUpdater2))
	{
		DeleteFile("Updater.exe");
		CopyFile("Updater2.exe","Updater.exe",FALSE);
		DeleteFile("Updater2.exe");
	}
//yckou end
//add by sonic Check *.dll have Bug. 2006.4.13
//yckou begin: check invalid *.dll
	// The startup DLL whitelist: a *.dll beside the executable whose name is
	// not below makes WinMain() return -1 here, silently, before logging is
	// up. Directories are listed too, because the body judges the name alone.
	std::string InvalidDll;
	std::vector<Basic::SDirectoryEntry> vDllEntries;

	// A directory that cannot be enumerated skips the check.
	if (Basic::ListDirectory(".", "*.dll", vDllEntries, Basic::LIST_FILES_AND_DIRECTORIES))
	{
		for (size_t iDll=0; iDll<vDllEntries.size(); iDll++)
		{
			InvalidDll = vDllEntries[iDll].sName;

			size_t iLen = InvalidDll.size();
			for (size_t j=0;j<iLen;j++)
			{
				if(isupper((unsigned char)InvalidDll[j]) != 0)
					InvalidDll[j] = (char)tolower((unsigned char)InvalidDll[j]);
			}

			if(InvalidDll != "timer.dll" &&
				InvalidDll != "msvcrtd.dll" &&
				InvalidDll != "msvcrt.dll" &&
				InvalidDll != "msvcp60.dll" &&
				InvalidDll != "msvcirt.dll" &&
				InvalidDll != "ifc22.dll" &&
				InvalidDll != "gl.dll" &&
				InvalidDll != "dsetup32.dll" &&
				InvalidDll != "dsetup.dll" &&
				InvalidDll != "deutil.dll" &&
				InvalidDll != "basics.dll" &&
				InvalidDll != "npx.dll" &&
				InvalidDll != "npupdate0.dll" &&
				InvalidDll != "nppsk.dll" &&
				InvalidDll != "npgmup.dll" &&
				InvalidDll != "npfgm9x.dll" &&
				InvalidDll != "npcipher.dll" &&
				InvalidDll != "mother.dll" &&
				InvalidDll != "fl.dll" &&
				InvalidDll != "dbghelp.dll" &&
				InvalidDll != "darkedenweb.dll" &&
				InvalidDll != "npchk.dll" &&
				InvalidDll != "xerces-c_2_4_0.dll" &&
				// SDL2/vcpkg runtime DLLs (SDL migration replaced the DirectX/
				// Immersion/GL_import libraries this whitelist used to name -
				// ifc22.dll/gl.dll above are their now-dead entries). Debug and
				// Release configs use differently-suffixed DLL names, so both
				// are listed.
				InvalidDll != "sdl2d.dll" &&
				InvalidDll != "libssl-3-x64.dll" &&
				InvalidDll != "libcrypto-3-x64.dll" &&
				InvalidDll != "sdl2.dll" &&
				InvalidDll != "sdl2_ttfd.dll" &&
				InvalidDll != "sdl2_ttf.dll" &&
				InvalidDll != "sdl2_imaged.dll" &&
				InvalidDll != "sdl2_image.dll" &&
				InvalidDll != "freetyped.dll" &&
				InvalidDll != "freetype.dll" &&
				InvalidDll != "libpng16d.dll" &&
				InvalidDll != "libpng16.dll" &&
				InvalidDll != "zlibd1.dll" &&
				InvalidDll != "zlib1.dll" &&
				// vcpkg ships zlib as z.dll/zd.dll rather than the zlib1.dll
				// names above, and it arrives here as a transitive dependency
				// of freetype and libpng.
				InvalidDll != "zd.dll" &&
				InvalidDll != "z.dll" &&
				InvalidDll != "jpeg62.dll" &&
				InvalidDll != "brotlicommon.dll" &&
				InvalidDll != "brotlidec.dll" &&
				InvalidDll != "bz2d.dll" &&
				InvalidDll != "bz2.dll" &&
				// vcpkg's libiconv, deployed beside the executable because
				// DarkEden links Iconv::Iconv. TextService::NormalizeText needs
				// a real iconv to transcode the shipped CP949 data, since SDL's
				// built-in converter supports no legacy CJK code page. The name
				// carries no debug suffix, so one entry covers both configs.
				InvalidDll != "iconv-2.dll" &&
				// vcpkg's SDL2_mixer and the codec libraries it was built
				// against (ogg/vorbis for .ogg music, wavpack). They are only
				// deployed once DXLibBackendSDL.cpp actually references the
				// mixer, so they were absent from the output directory until
				// sound was wired up. The codec names carry no debug suffix.
				InvalidDll != "sdl2_mixerd.dll" &&
				InvalidDll != "sdl2_mixer.dll" &&
				InvalidDll != "ogg.dll" &&
				InvalidDll != "vorbis.dll" &&
				InvalidDll != "vorbisenc.dll" &&
				InvalidDll != "vorbisfile.dll" &&
				InvalidDll != "wavpackdll.dll")
				return -1;
		}
	}
//yckou end
//end sonic

#endif
	
	//-----------------------------------------------------------------------
	// Client 시작..
	//-----------------------------------------------------------------------
	g_bNeedUpdate = FALSE;
	// Upstream configured the CRT debug heap here (#ifdef _DEBUG):
	//
	//   tmpDbgFlag  = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	//   tmpDbgFlag |= _CRTDBG_DELAY_FREE_MEM_DF | _CRTDBG_LEAK_CHECK_DF;
	//   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | tmpDbgFlag);
	//
	// MSVC defines _DEBUG automatically under /MDd, so this ran in every Debug
	// build. _CRTDBG_DELAY_FREE_MEM_DF makes the debug CRT keep every freed
	// block, dead-filled, instead of releasing it - with in-game churn of
	// ~2,000 small alloc/free cycles per frame that retained ~190 KB of
	// corpses per frame (~200 MB/min) until the process ran out of memory.
	// Removed outright: the leak-check exit dump it paired with is useless at
	// this allocation volume, and the default _CRTDBG_ALLOC_MEM_DF already
	// provides the debug-heap guard fills.
	

	// g_hInstance is set by WinMain before this runs; it stays NULL off Windows.
	// random
	srand(static_cast<unsigned int>(time(NULL)));

	g_pFileDef = new Properties;
	g_pFileDef->load(FILE_INFO_FILEDEF);

	// 넷마블용
	bool bNetmarble = false;
	Properties NetmarbleConfig;

	NetmarbleConfig.load(g_pFileDef->getProperty("FILE_INFO_NETMARBLE").c_str());
	bNetmarble = NetmarbleConfig.getPropertyInt("Netmarble") != 0;

	//------------------------------------------------------
	// Server IP가 설정되어 있지 않은 경우 --> Launcher실행
	//------------------------------------------------------
	if (strlen(lpCmdLine)==0)
	{
			if (!ReleaseMutex( hMutex ))
			{
				DEBUG_ADD_FORMAT_ERR("Failed to release mutex : %d", GetLastError());
			}

			ReleaseAllObjects();			
			_chdir( g_CWD );
			_spawnl(_P_OVERLAY, UPDATER_FILENAME, UPDATER_FILENAME, lpCmdLine, NULL);
			return FALSE;
	}
	//------------------------------------------------------
	// Server IP가 설정되어 있는 경우
	//------------------------------------------------------
	else
	{
		char* str = lpCmdLine;
		if (strlen(str) < 8)
		{
			ReleaseMutex( hMutex );
			ReleaseAllObjects();
			_chdir( g_CWD );
			_spawnl(_P_OVERLAY, UPDATER_FILENAME, UPDATER_FILENAME, lpCmdLine, NULL);

			return FALSE;
		}

		//add by sonic 2006.4.9  设置加密函数
		DWORD GetCommand;
		// EXECryptor_GetHardwareID() removed (SDL2) - Copy protection no longer needed
		GetCommand = 0x00000000;  //用于调试
		char checkStr[9];
		memset(checkStr,0,9);
		snprintf(checkStr, sizeof(checkStr), "%X", GetCommand);
		char T_checkStr[9];
		memset(T_checkStr,0,9);
		if (8 -strlen(checkStr) !=0)
		{
			memcpy(T_checkStr,checkStr,strlen(checkStr));
			int n = static_cast<int>(8-strlen(checkStr));
			char* p=checkStr;
			for (int i =0;i<n;i++)
			{
				memcpy(p,"0",1);
				p++;
			}
			memcpy(p,T_checkStr,strlen(T_checkStr));
		}
		memcpy(checkStr,"00000000",8);
		memcpy(T_checkStr,str,8);

		////////////////////Add by Sonic 2006-6-21////////////////////////////////
		if (memcmp(checkStr,T_checkStr,strlen(checkStr)))
		{
			return false;
		}
		//////////////////////////////////////////////////////////////////////////
		if(!bNetmarble && lpCmdLine[8] >= '0' && lpCmdLine[8] <= '9')
		{
			char szDimen[10];
			memset( szDimen, 0, 10 );
			memcpy( szDimen, lpCmdLine+8, 1 );
			g_Dimension = atoi( szDimen );
		}
		g_bFullScreen	= true;
		g_MaxNPC		= 0;

		g_bHAL			= true;
		g_bMusicSW		= true;

	}
	// [Futec수정]
	GetFutecAddress(lpCmdLine);
//add by sonic 2006.4.10 判断程序启动模式
	if ( cmpFullScreen )
		{
			g_bFullScreen = true;
		}
		else
			g_bFullScreen = false;
	// Upstream forced windowed mode here under #ifdef _DEBUG - which is live
	// in every Debug build (MSVC defines _DEBUG under /MDd, see CLAUDE.md).
	// Removed so the fullscreen command-line flag ('2'/'4' as the last
	// character) works in Debug builds too; fullscreen is a borderless
	// desktop-size window, so there is nothing debugger-hostile about it.
	if (!CheckTerriblePatch())
	{
		ReleaseAllObjects();
		ReleaseMutex( hMutex );
		return 0;
	}

	//----------------------------------------------------------------
	// Updater.exe를 update 시킨다.
	//----------------------------------------------------------------
	// 새로운 updater실행화일이 존재하면..
	if (!_access(UPDATER_NEW_FILENAME, 0))//updaterNewFile)
	{	
		// 예전 updater를 지우고..
		if (remove( UPDATER_FILENAME )==0)
		{
			// 새것을 예전걸로 rename
			if (rename( UPDATER_NEW_FILENAME, UPDATER_FILENAME )==0)
			{
				DEBUG_ADD("Update Updater.exe OK"); 
			}
		}
	}	

	//---------------------------------------------------
	// Update 디렉토리를 지워준다.
	//---------------------------------------------------
	// updater에서 지워야 하는데..빼먹는 바람에.. T_T;;
	//---------------------------------------------------
	char CWD[_MAX_PATH];

	// 현재 path읽기
	if (_getcwd( CWD, _MAX_PATH )!=NULL)
	{	
		// Empty unless built below. The directory is the Windows updater's
		// leftover and the backslash is Windows' separator, so off Windows
		// this names nothing and the cleanup is a no-op, as intended - but
		// _getcwd() can answer "/" there (the browser build's root), which
		// the length test skips, and _rmdir() then read an uninitialized
		// buffer.
		char UpdateDir[_MAX_PATH + sizeof(DIRECTORY_UPDATE)] = { 0, };

		int lenCWD = static_cast<int>(strlen(CWD));
		if (lenCWD > 1)
		{
			if (CWD[lenCWD-1]=='\\')
			{
				snprintf(UpdateDir, sizeof(UpdateDir), "%s%s", CWD, DIRECTORY_UPDATE);
			}
			else
			{
				snprintf(UpdateDir, sizeof(UpdateDir), "%s\\%s", CWD, DIRECTORY_UPDATE);
			}
		}
		
		if (_rmdir( UpdateDir )!=0)
		{			
			// PATH가 잘못된 경우
			if (errno==ENOENT)
			{				
			}
			// Not removed because something is inside.
			else //if (errno==ENOTEMPTY)
			{
				// The current directory is already remembered in CWD.
				if (_chdir( UpdateDir ) == 0)
				{
					//---------------------------------------------------
					// Delete the files one by one. T_T;
					//
					// The _chdir dance above is kept, so remove() still
					// takes a bare name relative to the Update directory
					// and the working directory is restored below exactly
					// as it was.
					//
					// Files only, where the legacy "*.*" also matched
					// subdirectories: remove() cannot delete a directory
					// on Windows, so the entries no longer listed are
					// exactly the ones this loop could never have acted
					// on. The pattern is "*" and not "*.*" because a '.'
					// is a literal to Basic::ListDirectory while Win32
					// read "*.*" as "everything"; asking for "*.*" here
					// would silently drop every dotless name.
					//---------------------------------------------------
					std::vector<Basic::SDirectoryEntry>	vUpdateFiles;

					// Read every file.
					if ( Basic::ListDirectory( ".", "*", vUpdateFiles ) )
					{
						for (size_t iFile=0; iFile<vUpdateFiles.size(); iFile++)
						{
							const std::string&	sFilename = vUpdateFiles[iFile].sName;

							// No need to delete names starting with a dot.
							// "." and ".." are never listed, so the test
							// now only skips dotfiles - and every listed
							// entry is processed, where the legacy
							// while/_findnext loop dropped the entry
							// _findfirst itself had returned. On NTFS
							// "*.*" returned "." first, so the skip only
							// ever lost ".".
							if (!sFilename.empty() && sFilename[0] != '.')
							{
								remove( sFilename.c_str() );
							}
						}
					}

					_chdir( CWD );

					if (_rmdir( UpdateDir )==0)
					{
						// 잘 지워졌다.
					}
				}				
			}
		}
	}

	RemoveProgressBar();
	REALSERVER_INFO RealServerInfo;
	if(bNetmarble)
	{
		if(ParsingNetmarble(lpCmdLine) == false)
		{
			if(NetmarbleConfig.getPropertyInt("Netmarble") == 1)
			{

				return -1;
			}

			bNetmarble = false;
		}
	} else
	{
		if(ParsingRealServer(lpCmdLine, g_Dimension, RealServerInfo) == false)
		{
		}
	}

	DARKEDEN_LANGUAGE Language = CheckDarkEdenLanguage();
	
	switch( Language )
	{
	case DARKEDEN_CHINESE :
		gC_ci = new CI_CHINESE;
		break;
	default :
		gC_ci = new CI_KOREAN;
		break;
	}
	
#if !defined(_DEBUG) && defined(PLATFORM_WINDOWS)
	// The crash reporter is a Win32 structured-exception handler over dbghelp.
	if( gC_ci->IsKorean() == true )
		InitCrashReport();
#endif
	if (InitApp(nCmdShow))
	{
		// For Netmarble: a Netmarble launch never gets this far with
		// bNetmarble set. ParsingNetmarble refuses it or answers false,
		// and on false ClientMain returns or clears bNetmarble above (task
		// 4.220 retired the copy of its never filled NETMARBLE_INFO that
		// stood here).
		{
			// mode|demention|ID|key
			g_pUserInformation->IsAutoLogIn			= RealServerInfo.bMode;
			if(true == g_pUserInformation->IsAutoLogIn)
			{
				g_pUserInformation->UserID				= RealServerInfo.ID;
				g_pUserInformation->AutoLogInKeyValue	= RealServerInfo.Key;
			
				g_Dimension = RealServerInfo.WorldID;

				DEBUG_ADD("Auto-login configured");
			}			
			
		
		}

		if( gC_ci->IsKorean() )
			g_pUserInformation->SetKorean();
		else if (gC_ci->IsChinese() )
			g_pUserInformation->SetChinese();
		else if (gC_ci->IsJapanese() )
			g_pUserInformation->SetJapanese();

		//------------------------------
		// 초기화 되지 않았으면 초기화한다.
		//------------------------------
		if (!g_pTopView->IsInit() && !g_pTopView->Init())
		{
			LOG_ERROR("Game view initialization failed; stopping before the frame loop");
			goto release_objects;
		}

		g_bActiveApp = TRUE;
		DEBUG_CMD(MIN_CLRSCR, "시작");
		DEBUG_CMD(MIN_SHOWWND, "------------------------------");
#ifdef __EMSCRIPTEN__
		auto* browser = new BrowserSession{nullptr, std::string(lpCmdLine)};
		browser->mutex = hMutex;
		browserLoopRunning = true;
		emscripten_set_main_loop_arg(BrowserFrame, browser, 0, false);
		return 0;
#else
		while (RunClientFrame()) {}
#endif

		SystemParametersInfo(SPI_SETSCREENSAVERRUNNING, FALSE, NULL, 0); 
	}
	//add by Soargon
	// Borderless presentation leaves the desktop display mode unchanged.
	//end
	//-----------------------------------------------------------------------------
	// 모든 object들 제거
	//-----------------------------------------------------------------------------	
release_objects:
	FinishClient(hMutex, lpCmdLine);

	return 0;
}
