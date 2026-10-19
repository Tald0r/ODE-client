//----------------------------------------------------------------------
// ClientMain.h
//
// The game's entry point, shared by the two platform entry points in
// SDLMain.cpp: WinMain on Windows and main elsewhere.
// It is WinMain's old body - the command line, the working directory,
// the file definitions, the language and character input, InitApp,
// the frame loop and the shutdown - with the Win32-only steps (the
// single-instance mutex, the patcher hand-off, the DLL whitelist, the
// message pump) behind PLATFORM_WINDOWS and their SDL equivalents in
// the other branch (docs/linux-macos-port-assessment-2026-09-07.md,
// area D). SDLMain.cpp used to carry a hand-copied partial of that
// body instead, with about twenty globals re-declared at the wrong
// types.
//
// lpCmdLine is the launch argument as WinMain received it (mutable:
// the body rewrites its own pointer to the display settings' launch
// command); nCmdShow is passed through to InitApp. Returns the process
// exit code.
//
// What ClientMain needs of the process and the OS - ending the process,
// handing it to the updater, a message box, the game's window, the
// message pump and the DLLs beside the executable - goes through
// ClientHost below, which the entry points install; the decisions taken
// on what those answer are plain functions declared after it.
//----------------------------------------------------------------------
#ifndef __CLIENT_MAIN_H__
#define __CLIENT_MAIN_H__

#include "../basic/Platform.h"
#include "MonotonicClock.h"
#include "MString.h"
#include <string>
#include <vector>

int ClientMain(char* lpCmdLine, int nCmdShow);
#ifdef __EMSCRIPTEN__
bool ClientHasBrowserLoop();
#endif

//----------------------------------------------------------------------
// ClientHost: the process and the OS, as ClientMain and its steps use
// them. SystemHost() is the real one, which the entry points install
// first; tests install their own. Without a host every entry does
// nothing and answers NULL, NoMessage or false.
//----------------------------------------------------------------------
namespace ClientHost {

// What one turn of the message pump found.
enum class Pumped
{
	NoMessage,		// none waiting
	Dispatched,		// one, translated and dispatched to its window
	Quit,			// the quit message (GetMessage answered FALSE)
};

struct Host
{
	// Ends the process with this exit code (a refused Netmarble launch).
	void (*Exit)(int code) = nullptr;
	// Replaces the process with the updater, Updater.exe, given these
	// arguments (_spawnl with _P_OVERLAY; off Windows the shim's no-op).
	void (*RunUpdater)(const char* arguments) = nullptr;
	// A message box with an OK button, which the player dismisses.
	void (*ShowMessageBox)(const char* text, const char* caption) = nullptr;
	// Windows: registers the game's window class (WindowProc) and creates
	// the game's window at (0, 0) with these styles and outer size; NULL
	// when it cannot.
	HWND (*CreateGameWindow)(DWORD exStyle, DWORD style, int width, int height) = nullptr;
	// Windows: destroys a window (the game's, when its start-up fails).
	void (*DestroyGameWindow)(HWND hWnd) = nullptr;
	// One turn of the OS message pump: PeekMessage, then GetMessage,
	// TranslateMessage and DispatchMessage when a message is waiting.
	Pumped (*PumpMessage)() = nullptr;
	// Waits for an OS message (the inactive application's turn).
	void (*WaitForMessage)() = nullptr;
	// Windows: appends the names of the "*.dll" entries in the working
	// directory, files and directories alike, in listing order; false
	// when the directory cannot be listed.
	bool (*ListDllNames)(std::vector<std::string>& names) = nullptr;
};

// Borrowed until replaced; returns the host it replaces. Every call
// reads the current host again.
const Host*	SetHost(const Host* host);
// The real host: the process, the Win32 calls (the shim's off Windows)
// and the working directory.
const Host*	SystemHost();

void	Exit(int code);
void	RunUpdater(const char* arguments);
void	ShowMessageBox(const char* text, const char* caption);
HWND	CreateGameWindow(DWORD exStyle, DWORD style, int width, int height);
void	DestroyGameWindow(HWND hWnd);
Pumped	PumpMessage();
void	WaitForMessage();
bool	ListDllNames(std::vector<std::string>& names);

}

//----------------------------------------------------------------------
// The launch command (lpCmdLine, after the display settings' launch
// command and the cut of its last character).
//----------------------------------------------------------------------

// The display mode a launch command's last character names: '1' 800x600
// in a window, '2' 800x600 full screen, '3' 1024x768 in a window, '4'
// 1024x768 full screen. Sets g_MyFull, and for 1024x768 the frame
// (g_GameRect), the cursor (g_x, g_y) and the view's sizes (ViewSectors.h).
// Answers whether the game runs full screen: `fullscreen` (the display
// settings' choice) for any other character, which changes nothing.
bool	ApplyDisplayMode(char mode, bool fullscreen);

// What ClientMain does with the launch command.
enum LAUNCH_COMMAND
{
	LAUNCH_UPDATER,		// shorter than eight characters (empty too): run the updater
	LAUNCH_REFUSED,		// its first eight characters are not "00000000": quit
	LAUNCH_GAME,		// run the game
};
LAUNCH_COMMAND	CheckLaunchCommand(const char* lpCmdLine);

// The world (g_Dimension) a game launch command names, its ninth
// character, when that is a digit; otherwise -1.
int		LaunchDimension(const char* lpCmdLine);

// The launcher's login server, "Futec(ip:port)" anywhere in the command,
// into g_FutecIP and g_FutecPort; false (and nothing set) without one.
bool	GetFutecAddress(const char* pStr);

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

// The automatic login a web launch carries after the command's first
// space: "mode|world|ID|key". false for a command under 16 characters,
// without a space, or with nothing after it.
bool	ParsingRealServer(const char* pCommandLine, int Dimention, REALSERVER_INFO &info);

//----------------------------------------------------------------------
// The installation
//----------------------------------------------------------------------

// The first name in the list that is not a DLL the game ships with or
// tolerates (compared without regard to case), or NULL when every one
// is. ClientMain returns -1 on one, silently (CLAUDE.md, Traps).
const std::string*	FindUnlistedDll(const std::vector<std::string>& names);

// The working directory ClientMain settled on (the executable's on
// Windows, the data tree's elsewhere), where the updater is run from.
extern char		g_CWD[];		// _MAX_PATH bytes

enum DARKEDEN_LANGUAGE
{
	DARKEDEN_KOREAN = 0,
	DARKEDEN_CHINESE,
	DARKEDEN_JAPANESE,
	DARKEDEN_ENGLISH,
	DARKEDEN_TAIWAN,

	DARKEDEN_LANGUAGE_MAX
};
// The language FILE_LANGUAGE_INFO names ("LANGUAGE n"); Korean without
// the file definitions or the file.
DARKEDEN_LANGUAGE	CheckDarkEdenLanguage();

// How often the personal shops' signs refresh (FILE_UPDATE_PERSNALSHOPTIME).
int			GetPersnalShopUpdateTime();

// The append patches (FILE_INFO_APPENDPATCH); false when that file is
// missing, and ClientMain then quits.
bool		CheckTerriblePatch();

// The progress window the append patches report to (Windows; the shim
// creates none elsewhere).
extern bool	g_bUseProgressBar;
extern HWND	g_hWndProgress;
void		CreateProgressBar(const char* str);
void		SetProgressBarCount(int count);
void		SetProgressBarText(const char* str);
void		UpdateProgressBar();
void		RemoveProgressBar();

//----------------------------------------------------------------------
// The window and the frame loop
//----------------------------------------------------------------------

// The game's window procedure.
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

// The largest whole factor, at least 1, by which a game frame of
// gameWidth x gameHeight plus the window's frame still fits a work area
// of workWidth x workHeight: the windowed mode's crisp upscale.
int			ChooseWindowScale(int gameWidth, int gameHeight, int frameWidth, int frameHeight,
				int workWidth, int workHeight);
// The factor InitApp chose (1 in full screen); WM_GETMINMAXINFO pins
// the window to it.
extern int	g_WindowScale;

// The window (Windows), the cursor, then InitGame.
BOOL		InitApp(int nCmdShow);
// A failed start-up: the application stops, the window goes.
HRESULT		InitFailMessage(const char* message);

// One frame: the SDL events (off Windows), one OS message or, when none
// waits, the game's update while the application is active (and the
// frame rate once a second) or a wait for a message. false: quit.
bool		RunClientFrame();
// The frame rate's window (RunClientFrame): when it opened, at what
// frame count.
extern int	g_StartFrameCount;
extern MonotonicClock::TimePoint	g_StartTime;

// The shutdown: the objects, the character input, the configuration, the
// single-instance mutex, then the updater when g_bNeedUpdate asks for it.
void		FinishClient(HANDLE hMutex, const char* lpCmdLine);

#endif // __CLIENT_MAIN_H__
