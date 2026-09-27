/*-----------------------------------------------------------------------------

	Platform.h

	Cross-platform abstraction layer for Dark Eden client.
	Provides unified API for Windows, Linux, and macOS.

	Original Windows API dependencies are abstracted here.

	2025.01.14

-----------------------------------------------------------------------------*/

#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include <stdint.h>
#include <stddef.h>
#include <string.h>   /* strcmp, for the MessageBox shim's video-driver test */
/* The real assert, on every platform. This header used to define
   assert(e) as ((void)(e)) off Windows, ahead of <assert.h>, which
   silently turned every assertion in the tree into an evaluated
   no-op there; an assertion that fails on Linux or macOS now aborts
   the way it does on Windows (port assessment, area B). */
#include <assert.h>

/* ============================================================================
 * Platform Detection
 * ============================================================================ */

/* Detect platform. This block is the one place in the tree that decides
   what the platform is: every other file reads these macros and none
   defines them, and the build passes no platform macro of its own on
   Linux or macOS - the compiler builtins are the truth, and a CMake
   define could only disagree with them (it did: every non-Windows
   target used to be told it was macOS). Ratchet R13 holds the older
   spellings - __LINUX__, _LINUX and PLATFORM_MACOS in a build file - at
   zero. */
#if defined(_WIN32) || defined(_WIN64)
	#ifndef PLATFORM_WINDOWS
		#define PLATFORM_WINDOWS
	#endif
#elif defined(__EMSCRIPTEN__)
	#define PLATFORM_WEB
#elif defined(__linux__)
	#ifndef PLATFORM_LINUX
		#define PLATFORM_LINUX
	#endif
#elif defined(__APPLE__)
	#include <TargetConditionals.h>
	#if TARGET_OS_MAC
		#ifndef PLATFORM_MACOS
			#define PLATFORM_MACOS
		#endif
	#endif
#else
	#define PLATFORM_UNKNOWN
#endif

/* PLATFORM_POSIX: the non-Windows platforms as one. Sockets, files,
   paths and threads are the same BSD/POSIX API on Linux and macOS, and
   most of the tree only ever needs "not Windows". Test this, not
   PLATFORM_MACOS, for anything that is not actually Darwin-specific. */
#if defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS) || defined(PLATFORM_WEB) || defined(__unix__)
	#ifndef PLATFORM_POSIX
		#define PLATFORM_POSIX
	#endif
#endif

/* ============================================================================
 * Platform Selection
 * ============================================================================ */

/* Force SDL backend on non-Windows platforms */
#ifndef PLATFORM_WINDOWS
	#ifndef PLATFORM_USE_SDL
		#define PLATFORM_USE_SDL
	#endif
#endif

/* Allow explicit SDL selection on Windows.

   <SDL.h>, never <SDL2/SDL.h>: the SDL2::SDL2 target every package
   source exports (vcpkg, Debian, Homebrew, a source install) puts the
   SDL2 header directory itself on the include path, and that is the
   only spelling all of them share. Homebrew's autotools-built config
   exports nothing above it, so <SDL2/SDL.h> resolves there only through
   a compiler default path - /usr/local/include on Intel, nothing on
   Apple Silicon. The tree used both spellings until the macOS step of
   the port (docs/linux-macos-port-assessment-2026-09-07.md, area G). */
#ifdef PLATFORM_USE_SDL
	#include <SDL.h>
#endif

/* Include windows.h as early as possible on Windows builds so the rest of
   this header can defer to the real Win32 types/structs/functions instead
   of redefining them (which fails to compile or silently diverges). */
#ifdef PLATFORM_WINDOWS
	#ifndef _WINDOWS_
		#define WIN32_LEAN_AND_MEAN
		#include <windows.h>
	#endif
#endif

/* ============================================================================
 * Calling Conventions
 * ============================================================================ */

/* Define calling conventions for non-Windows platforms */
#ifndef PLATFORM_WINDOWS
	#ifndef __cdecl
		#define __cdecl
	#endif
	#ifndef __stdcall
		#define __stdcall
	#endif
	#ifndef WINAPI
		#define WINAPI
	#endif
	#ifndef APIENTRY
		#define APIENTRY
	#endif
	#ifndef CALLBACK
		#define CALLBACK
	#endif
	#ifndef INLINE
		#define INLINE inline
	#endif
#endif

/* ============================================================================
 * Basic Type Definitions (from Typedef.h)
 * ============================================================================ */

#ifndef NULL
	#define NULL 0
#endif

#define NOT_SELECTED						-1

/* Type definitions (same as original Typedef.h)
   On Windows these come from <windows.h> instead, since it is included
   above and its types (DWORD, LONG, etc.) are not interchangeable with
   these fixed-width equivalents. */
#ifndef PLATFORM_WINDOWS
typedef uint8_t			BYTE;
typedef uint16_t		WORD;
typedef uint32_t		UINT;
typedef uint32_t		DWORD;
typedef uint32_t		ULONG;
typedef uint64_t		DWORD64;
typedef uint64_t		ULONGLONG;
typedef int64_t			LONGLONG;
typedef void*			PVOID;
typedef void*			ADDRESS_MODE;
typedef uintptr_t		ULONG_PTR;
typedef intptr_t		LONG_PTR;
typedef uintptr_t		DWORD_PTR;
typedef int32_t			LONG;
typedef int				BOOL;

	/* Define id_t for cross-platform compatibility (unsigned int on all platforms) */
	typedef unsigned int   id_t;
#endif /* !PLATFORM_WINDOWS */

/* QWORD is a project-specific type, not part of the real Win32 API, so
   <windows.h> never defines it - keep it available on every platform. */
#ifndef QWORD_DEFINED
#define QWORD_DEFINED
typedef uint64_t		QWORD;
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* WAVEFORMATEX structure for audio format */
#ifndef _WAVEFORMATEX_
#define _WAVEFORMATEX_
typedef struct _WAVEFORMATEX {
	WORD wFormatTag;
	WORD nChannels;
	DWORD nSamplesPerSec;
	DWORD nAvgBytesPerSec;
	WORD nBlockAlign;
	WORD wBitsPerSample;
	WORD cbSize;
} WAVEFORMATEX, *LPWAVEFORMATEX, *PWAVEFORMATEX;
#endif

/* WAVE format constants */
#define WAVE_FORMAT_PCM		1
#define WAVE_FORMAT_ADPCM	2

/* DirectSound buffer capabilities */
#ifndef PLATFORM_WINDOWS
#define DSBCAPS_PRIMARYBUFFER		0x00000001
#define DSBCAPS_STATIC			0x00000002
#define DSBCAPS_LOCHARDWARE		0x00000004
#define DSBCAPS_LOCSOFTWARE		0x00000008
#define DSBCAPS_CTRL3D			0x00000010
#define DSBCAPS_CTRLFREQUENCY		0x00000020
#define DSBCAPS_CTRLPAN			0x00000040
#define DSBCAPS_CTRLVOLUME		0x00000080
#define DSBCAPS_CTRLPOSITIONNOTIFY	0x00000100
#define DSBCAPS_CTRLFX			0x00000200
#define DSBCAPS_STICKYFOCUS		0x00004000
#define DSBCAPS_GLOBALFOCUS		0x00008000
#define DSBCAPS_GETCURRENTPOSITION2	0x00010000
#define DSBCAPS_MUTE3DATMAX		0x00020000
#define DSBCAPS_MIXIN			0x00040000
#define DSBCAPS_TRUEPLAYPOSITION	0x00080000
#endif

/* DirectSound types for non-Windows platforms */
#ifndef PLATFORM_WINDOWS
struct IDirectSound;
struct IDirectSoundBuffer;
struct IDirectSoundNotify;

#ifndef LPDIRECTSOUNDBUFFER
typedef struct IDirectSoundBuffer* LPDIRECTSOUNDBUFFER;
#endif
typedef struct IDirectSound* LPDIRECTSOUND;
typedef struct IDirectSoundNotify* LPDIRECTSOUNDNOTIFY;
#endif

/* CRITICAL_SECTION for thread synchronization */
#if !defined(_CRITICAL_SECTION_DEFINED) && !defined(PLATFORM_WINDOWS)
#define _CRITICAL_SECTION_DEFINED
#include <pthread.h>

typedef struct _CRITICAL_SECTION {
	pthread_mutex_t mutex;
	int initialized;
} CRITICAL_SECTION, *PCRITICAL_SECTION, *LPCRITICAL_SECTION;

/* Critical section functions - pthread-based implementations for macOS */
/* Note: Use recursive mutex to match Windows CRITICAL_SECTION behavior */
static inline void InitializeCriticalSection(CRITICAL_SECTION* cs) {
	if (cs != NULL) {
		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);  // recursive, as the Win32 CRITICAL_SECTION is
		pthread_mutex_init(&cs->mutex, &attr);
		pthread_mutexattr_destroy(&attr);
		cs->initialized = 1;
	}
}

static inline void EnterCriticalSection(CRITICAL_SECTION* cs) {
	if (cs != NULL && cs->initialized) {
		pthread_mutex_lock(&cs->mutex);
	}
}

static inline void LeaveCriticalSection(CRITICAL_SECTION* cs) {
	if (cs != NULL && cs->initialized) {
		pthread_mutex_unlock(&cs->mutex);
	}
}

static inline void DeleteCriticalSection(CRITICAL_SECTION* cs) {
	if (cs != NULL && cs->initialized) {
		pthread_mutex_destroy(&cs->mutex);
		cs->initialized = 0;
	}
}

/* GDI object management functions - stub implementations */
static inline int DeleteObject(void* hObject) {
	(void)hObject;
	/* Stub - Windows GDI object deletion */
	return 1; /* Return TRUE */
}

/* Font weight constants */
#define FW_NORMAL 400
#define FW_BOLD 700
#define FW_THIN 100
#define FW_MEDIUM 500
#define FW_HEAVY 900
#define FW_EXTRABOLD 800
#define FW_LIGHT 300

/* LOGFONT structure for font creation */
#ifndef LOGFONT_DEFINED
#define LOGFONT_DEFINED
typedef struct tagLOGFONT {
	long lfHeight;
	long lfWidth;
	long lfEscapement;
	long lfOrientation;
	long lfWeight;
	unsigned char lfItalic;
	unsigned char lfUnderline;
	unsigned char lfStrikeOut;
	unsigned char lfCharSet;
	unsigned char lfOutPrecision;
	unsigned char lfClipPrecision;
	unsigned char lfQuality;
	unsigned char lfPitchAndFamily;
	char lfFaceName[32];
} LOGFONT, *PLOGFONT, *LPLOGFONT;
#endif

/* Character set constants */
#define ANSI_CHARSET 0
#define DEFAULT_CHARSET 1
#define SYMBOL_CHARSET 2
#define SHIFTJIS_CHARSET 128
#define HANGUL_CHARSET 129
#define GB2312_CHARSET 134
#define OEM_CHARSET 255

/* Output precision constants */
#define OUT_DEFAULT_PRECIS 0
#define OUT_STRING_PRECIS 1
#define OUT_CHARACTER_PRECIS 2
#define OUT_STROKE_PRECIS 3
#define OUT_TT_PRECIS 4
#define OUT_DEVICE_PRECIS 5
#define OUT_RASTER_PRECIS 6
#define OUT_TT_ONLY_PRECIS 7
#define OUT_OUTLINE_PRECIS 8
#define OUT_SCREEN_OUTLINE_PRECIS 9
#define OUT_PS_ONLY_PRECIS 10

/* Clip precision constants */
#define CLIP_DEFAULT_PRECIS 0
#define CLIP_CHARACTER_PRECIS 1
#define CLIP_STROKE_PRECIS 2
#define CLIP_MASK 0xf
#define CLIP_LH_ANGLES (1<<4)
#define CLIP_TT_ALWAYS (2<<4)
#define CLIP_EMBEDDED (8<<4)

/* Font quality constants */
#define DEFAULT_QUALITY 0
#define DRAFT_QUALITY 1
#define PROOF_QUALITY 2
#define NONANTIALIASED_QUALITY 3
#define ANTIALIASED_QUALITY 4
#define CLEARTYPE_QUALITY 5

/* Font pitch and family constants */
#define DEFAULT_PITCH 0
#define FIXED_PITCH 1
#define VARIABLE_PITCH 2
#define FF_DONTCARE 0
#define FF_ROMAN 1
#define FF_SWISS 2
#define FF_MODERN 3
#define FF_SCRIPT 4
#define FF_DECORATIVE 5

/* Background mode constants */
#define TRANSPARENT 1
#define OPAQUE 2

/* Text alignment constants */
#define TA_NOUPDATECP 0
#define TA_LEFT 0
#define TA_TOP 0
#define TA_UPDATECP 1
#define TA_RIGHT 2
#define TA_CENTER 6
#define TA_BASELINE 24

/* DirectDraw surface capabilities */
#define DDSCAPS_SYSTEMMEMORY 0x00000800L

/* CreateFontIndirect has no stub: it returned (void*)1 as a font handle,
   and nothing calls it any more - fonts are TextSystem's. A new caller
   fails to compile here rather than drawing with a fake handle. */
#endif

/* DirectDraw surface capabilities - project-specific constants, not part of
   real Win32 API, so <windows.h> never defines them - keep available on
   every platform (same reasoning as QWORD above). */
#ifndef DDSCAPS_SYSTEMMEMORY
#define DDSCAPS_SYSTEMMEMORY 0x00000800L
#endif

/* Windows path constants */
#ifndef _MAX_PATH
	#define _MAX_PATH	260
#endif

/* Define MAX_PATH without underscore for compatibility */
#ifndef MAX_PATH
	#define MAX_PATH _MAX_PATH
#endif

/* Color type definitions */
typedef DWORD			COLORREF;
#define RGB(r,g,b)		((COLORREF)(((BYTE)(r)|((WORD)((BYTE)(g))<<8))|(((DWORD)(BYTE)(b))<<16)))

/* id_t conflicts with POSIX on macOS/Linux, only define on Windows */
#ifdef PLATFORM_WINDOWS
typedef DWORD			id_t;
#endif

#ifdef __cplusplus
// A supported character type is required by std::basic_string. Recent libc++
// no longer supplies the non-standard char_traits<unsigned short> extension.
typedef char16_t		char_t;
#else
typedef WORD			char_t;
#endif

/* ============================================================================
 * Common Windows Type Definitions (for cross-platform compatibility)
 * ============================================================================ */

#ifndef PLATFORM_WINDOWS
	/* Windows-compatible type definitions for non-Windows platforms */
	typedef int				BOOL;
	/* Define id_t for cross-platform compatibility (unsigned int on all platforms) */
	typedef unsigned int   id_t;
	#ifndef TRUE
		#define TRUE	1
	#endif
	#ifndef FALSE
		#define FALSE	0
	#endif

	typedef int32_t			HRESULT;
	typedef intptr_t		LRESULT;
	typedef uintptr_t		UINT_PTR;
	#ifndef S_OK
		#define S_OK		0
	#endif
	#ifndef S_FALSE
		#define S_FALSE		1
	#endif

	/* HRESULT macros */
	#ifndef SUCCEEDED
		#define SUCCEEDED(hr)	(((HRESULT)(hr)) >= 0)
	#endif
	#ifndef FAILED
		#define FAILED(hr)		(((HRESULT)(hr)) < 0)
	#endif

	/* VOID type */
	#ifndef VOID
		typedef void		VOID;
	#endif

	/* Additional Windows types */
	typedef int32_t			LONG;
	typedef void*			LPVOID;
	typedef void*			HWND;
	typedef void*			HDC;
	typedef void*			HFONT;
	typedef void*			HINSTANCE;
	typedef void*			HANDLE;
	typedef DWORD*			LPDWORD;
	typedef const char*		LPCSTR;
	typedef char*			LPSTR;
	typedef const char*		LPCTSTR;
	typedef char*			LPTSTR;
	typedef const wchar_t*	LPCWSTR;
	typedef wchar_t*		LPWSTR;
	typedef unsigned char*	LPBYTE;
	typedef intptr_t		LPARAM;
	typedef intptr_t		WPARAM;
	typedef uint32_t			UINT;

	/* MessageBox constants */
	#define MB_OK			0x00000000L
	#define MB_ICONERROR		0x00000010L

	/* Windows parameter annotation macros (for callback function signatures) */
	#ifndef IN
		#define IN
	#endif
	#ifndef OUT
		#define OUT
	#endif
	#ifndef OPTIONAL
		#define OPTIONAL
	#endif

	/* Character type macros */
	#ifndef _T
		#define _T(x)		x
	#endif
	#ifndef TEXT
		#define TEXT(x)	x
	#endif
	#ifndef _L
		#define _L(x)		x
	#endif

	/* TCHAR and related types */
	#ifndef UNICODE
		typedef char			TCHAR;
		#define _tcscat		strcat
		#define _tcscpy		strcpy
		#define _tcslen		strlen
		#define _tcschr		strchr
		#define _tcsrchr		strrchr
		#define _stprintf	sprintf
		#define _tprintf		printf
		#define _tmain		main
	#else
		typedef wchar_t		TCHAR;
		#define _tcscat		wcscat
		#define _tcscpy		wcscpy
		#define _tcslen		wcslen
		#define _tcschr		wcschr
		#define _tcsrchr		wcsrchr
		#define _stprintf	swprintf
		#define _tprintf		wprintf
		#define _tmain		wmain
	#endif

	typedef TCHAR*			LPTSTR;
	typedef const TCHAR*	LPCTSTR;

	/* _TCHAR alias for compatibility with older code */
	#ifndef _TCHAR
		#define _TCHAR	TCHAR
	#endif

	/* MessageBox: the stderr line it always wrote, plus a modal SDL dialog
	   (platform_show_error, PlatformSDL.cpp) when there is a real display
	   to show it on - SDL video up and not the dummy or offscreen driver -
	   so a message the game shows the player is not lost to a terminal
	   nobody is watching, while a headless run, CI, or a unit test that
	   reaches one of the library's MessageBox calls (CSpritePalBase.cpp's
	   SaveToFile, for one) never blocks on a dialog. Returns IDOK (1); the
	   game's MessageBox calls do not branch on the answer off Windows. */
	#ifdef __cplusplus
	extern "C"
	#endif
	void platform_show_error(const char* title, const char* message);
	static inline int MessageBox(void* hWnd, const char* lpText, const char* lpCaption, unsigned int uType) {
		(void)hWnd; (void)uType;
		fprintf(stderr, "[%s] %s\n", lpCaption ? lpCaption : "", lpText ? lpText : "");
		if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
			const char* driver = SDL_GetCurrentVideoDriver();
			if (driver != NULL && strcmp(driver, "dummy") != 0 && strcmp(driver, "offscreen") != 0) {
				platform_show_error(lpCaption ? lpCaption : "DarkEden", lpText ? lpText : "");
			}
		}
		return 1;
	}

	/* SystemParametersInfo constants */
	#define SPI_GETMOUSE			0x0003
	#define SPI_SETMOUSE			0x0004

	/* Stub for SystemParametersInfo */
	static inline BOOL SystemParametersInfo(UINT uiAction, UINT uiParam, void* pvParam, UINT fWinIni) {
		(void)uiAction; (void)uiParam; (void)pvParam; (void)fWinIni;
		// Non-Windows platforms don't have mouse acceleration settings in the same way
		return FALSE;
	}

	/* Note: GetCursorPos and ScreenToClient are defined as inline functions below,
	   but they require POINT to be fully defined first (included from Client_PCH.h) */

	/* Stub for GetCursorPos - gets mouse position in SDL */
	static inline BOOL GetCursorPos(void* lpPoint) {
		if (lpPoint) {
			typedef struct { LONG x; LONG y; } POINT;
			POINT* p = (POINT*)lpPoint;
			int x, y;
			SDL_GetMouseState(&x, &y);
			p->x = x;
			p->y = y;
			return TRUE;
		}
		return FALSE;
	}

	/* Stub for ScreenToClient - no-op for SDL (coordinates are already relative to window) */
	static inline BOOL ScreenToClient(void* hWnd, void* lpPoint) {
		(void)hWnd;
		// SDL already gives us window-relative coordinates
		return lpPoint ? TRUE : FALSE;
	}

	/* Case-insensitive string comparison - the MSVC CRT names over the POSIX
	   ones (<strings.h>, which <string.h> includes on glibc and Darwin) */
	#include <strings.h>
	#define stricmp strcasecmp
	#define _stricmp strcasecmp
	#define _strnicmp strncasecmp
	#define strnicmp strncasecmp

	/* Microsoft-specific string functions - use standard equivalents */
	#define _stscanf sscanf

	/* Process and thread stubs for tlhelp32.h functions */
	#define INVALID_HANDLE_VALUE ((HANDLE)-1)
	#define TH32CS_SNAPPROCESS 0x00000002
	#define CloseHandle(handle) /* No-op on non-Windows */

	typedef struct {
		DWORD dwSize;              // Length of structure in bytes
		DWORD cntUsage;
		DWORD th32ProcessID;
		ULONG_PTR th32DefaultHeapID;
		DWORD th32ModuleID;
		DWORD cntThreads;
		DWORD th32ParentProcessID;
		LONG pcPriClassBase;
		DWORD dwFlags;
		char szExeFile[MAX_PATH];
	} PROCESSENTRY32;

	/* CreateToolhelp32Snapshot stub - returns invalid handle on non-Windows */
	static inline void* CreateToolhelp32Snapshot(DWORD dwFlags, DWORD th32ProcessID) {
		(void)dwFlags;
		(void)th32ProcessID;
		return INVALID_HANDLE_VALUE;
	}

	/* Process32First stub - always fails on non-Windows */
	static inline BOOL Process32First(void* hSnapshot, PROCESSENTRY32* lppe) {
		(void)hSnapshot;
		(void)lppe;
		return FALSE;
	}

	/* Process32Next stub - always fails on non-Windows */
	static inline BOOL Process32Next(void* hSnapshot, PROCESSENTRY32* lppe) {
		(void)hSnapshot;
		(void)lppe;
		return FALSE;
	}

	/* FindWindow stub - not implemented on non-Windows */
	static inline void* FindWindow(const char* lpClassName, const char* lpWindowName) {
		(void)lpClassName;
		(void)lpWindowName;
		return NULL; // No window finding on non-Windows platforms
	}

	/* ShowCursor over SDL, with the Win32 contract: a display count that
	   starts at 0 (shown), goes up on TRUE and down on FALSE, is returned
	   after the change, and the cursor is visible while it is >= 0.
	   UI_ShowWindowCursor / UI_HiddenWindowCursor (GameUI.cpp) loop on
	   that count - a fixed return value inverted them. The game draws its
	   own cursor and hides the system one at start-up (InitApp). */
	static inline int ShowCursor(BOOL bShow) {
		static int s_nDisplayCount = 0;
		s_nDisplayCount += bShow ? 1 : -1;
		SDL_ShowCursor(s_nDisplayCount >= 0 ? SDL_ENABLE : SDL_DISABLE);
		return s_nDisplayCount;
	}

	/* InitCommonControls stub - no-op on non-Windows */
	#define InitCommonControls()

	/* GetSystemMetrics constants */
	#define SM_CXSCREEN 0
	#define SM_CYSCREEN 1
	#define SM_CYVSCROLL 20
	#define SM_CYSIZEFRAME 33
	#define SM_CYMENU 15
	#define SM_CXSIZEFRAME 32

	/* GetSystemMetrics stub - returns default values on non-Windows */
	static inline int GetSystemMetrics(int nIndex) {
		switch(nIndex) {
			case SM_CXSCREEN: return 1024;
			case SM_CYSCREEN: return 768;
			case SM_CYVSCROLL: return 16;
			default: return 0;
		}
	}

	/* Window style constants */
	#define WS_EX_TOPMOST 0x00000008
	#define WS_EX_APPWINDOW 0x00040000
	#define WS_VISIBLE 0x10000000
	#define WS_POPUP 0x80000000L
	#define WS_OVERLAPPED 0x00000000L
	#define WS_CLIPCHILDREN 0x02000000L
	#define WS_THICKFRAME 0x00040000L
	#define WS_MINIMIZEBOX 0x00020000L
	#define WS_SYSMENU 0x00080000L
	#define SW_HIDE 0

	/* Progress bar constants */
	#define PROGRESS_CLASS "PROGRESS_CLASS"
	#define PBS_SMOOTH 0x01
	#define PBM_SETRANGE (WM_USER+1)
	#define PBM_SETSTEP (WM_USER+4)
	#define PBM_SETPOS (WM_USER+2)
	#define PBM_STEPIT (WM_USER+5)

	/* Window class styles */
	#define CS_HREDRAW 0x0001
	#define CS_VREDRAW 0x0002
	#define CS_DBLCLKS 0x0008

	/* WNDCLASS structure (Windows window class registration) */
	typedef struct tagWNDCLASS {
		UINT style;
		void* lpfnWndProc;
		int cbClsExtra;
		int cbWndExtra;
		void* hInstance;
		void* hIcon;
		void* hCursor;
		void* hbrBackground;
		const char* lpszMenuName;
		const char* lpszClassName;
	} WNDCLASS, *PWNDCLASS, *LPWNDCLASS;

	/* Message macros */
	#define MAKELPARAM(l, h) ((LPARAM)(((DWORD_PTR)(l) & 0xFFFF) | ((DWORD_PTR)(h) << 16)))
	#define WM_USER 0x0400
	#define WM_TIMER 0x0113
	#define WM_CHAR 0x0102
	#define WM_KEYUP 0x0101
	#define WM_IME_COMPOSITION 0x010F
	#define WM_IME_STARTCOMPOSITION 0x010D
	#define WM_IME_ENDCOMPOSITION 0x010E

	/* SDL text input messages */
	#define WM_TEXTINPUT 0x0111  /* SDL text input event (committed text) */
	#define WM_TEXTEDITING 0x0110  /* SDL text editing event (IME composition) */

	/* Window messages */
	#define WM_DESTROY 0x0002
	#define WM_CLOSE 0x0010
	#define WM_QUIT 0x0012
	#define WM_SYSCOMMAND 0x0112
	#define WM_MOVE 0x0003
	#define WM_KEYDOWN 0x0100
	#define WM_GETMINMAXINFO 0x0024
	#define WM_ACTIVATEAPP 0x001C

	/* Virtual key codes */
	#define VK_SPACE 0x20
	#define VK_RETURN 0x0D
	#define VK_ESCAPE 0x1B
	#define VK_SCROLL 0x91

	/* Code page constants */
	#define CP_ACP 0
	#define CP_OEMCP 1
	#define CP_UTF8 65001
	#define WC_COMPOSITECHECK 0x00000200

	/* System command values */
	#define SC_HOTKEY 0xF150
	#define SC_KEYMENU 0xF100
	#define SC_TASKLIST 0xF140
	#define SC_PREVWINDOW 0xF050
	#define SC_NEXTWINDOW 0xF040
	#define SC_CLOSE 0xF060
	#define SC_MOVE 0xF010
	#define SC_SIZE 0xF000
	#define SC_SCREENSAVE 0xF140  // Note: Same value as SC_TASKLIST
	#define SC_MONITORPOWER 0xF170
	#define SC_MAXIMIZE 0xF030

	/* MCI messages - still needed by some code */
	#define MM_MCINOTIFY 0x3D9
	#define MCI_NOTIFY_SUCCESSFUL 0x0001
	#define MCI_NOTIFY_SUPERCEDED 0x0002
	#define MCI_NOTIFY_ABORTED 0x0004
	#define MCI_NOTIFY_FAILURE 0x0008

	/* SendMessage stub - no-op on non-Windows */
	static inline LRESULT SendMessage(void* hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
		(void)hWnd; (void)Msg; (void)wParam; (void)lParam;
		return 0;
	}

	/* Reads one code point out of a wide string, advancing *pIndex past the
	 * units it consumed.
	 *
	 * wchar_t is 16 bits on Windows and 32 on the platforms this block is
	 * compiled for, so the source is UTF-16 in one case and UTF-32 in the
	 * other, and one walk deliberately serves both: a surrogate pair is
	 * combined wherever it appears, without testing the unit width.
	 *
	 * That is a real if harmless divergence from strict UTF-32, and it is
	 * stated rather than hidden because an earlier version of this comment
	 * asserted the opposite. Given the units D83D DE00, a strict UTF-32
	 * decoder yields two U+FFFD and this yields the single code point they
	 * spell in UTF-16. Well formed input of either width is unaffected, and
	 * ill formed UTF-32 has no right answer worth branching for.
	 *
	 * What does matter is that anything not a well formed code point becomes
	 * U+FFFD identically in both passes of the conversion below - the two
	 * passes share this function precisely so their byte counts cannot
	 * diverge, and a divergence of one byte would be a buffer overflow. */
	static inline unsigned int Platform_WideNextCodePoint(LPCWSTR pSrc, int nCount, int* pIndex) {
		unsigned int cp = (unsigned int)pSrc[(*pIndex)++];

		if (cp >= 0xD800 && cp <= 0xDBFF) {
			if (*pIndex < nCount) {
				unsigned int lo = (unsigned int)pSrc[*pIndex];
				if (lo >= 0xDC00 && lo <= 0xDFFF) {
					++(*pIndex);
					return 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
				}
			}
			return 0xFFFD;		/* high surrogate with no pair */
		}
		if (cp >= 0xDC00 && cp <= 0xDFFF)
			return 0xFFFD;		/* stray low surrogate */
		if (cp > 0x10FFFF)
			return 0xFFFD;		/* out of range, or a negative wchar_t */

		return cp;
	}

	/* Encodes one code point as UTF-8. Writes only when pOut is non-NULL and
	 * nRoom bytes are free, but returns the length either way, so the
	 * measuring pass and the writing pass below cannot disagree. */
	static inline int Platform_Utf8Encode(unsigned int cp, LPSTR pOut, int nRoom) {
		int n;

		if (cp < 0x80)			n = 1;
		else if (cp < 0x800)	n = 2;
		else if (cp < 0x10000)	n = 3;
		else					n = 4;

		if (pOut == NULL || nRoom < n)
			return n;

		switch (n) {
		case 1:
			pOut[0] = (char)cp;
			break;
		case 2:
			pOut[0] = (char)(0xC0 | (cp >> 6));
			pOut[1] = (char)(0x80 | (cp & 0x3F));
			break;
		case 3:
			pOut[0] = (char)(0xE0 | (cp >> 12));
			pOut[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
			pOut[2] = (char)(0x80 | (cp & 0x3F));
			break;
		default:
			pOut[0] = (char)(0xF0 | (cp >> 18));
			pOut[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
			pOut[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
			pOut[3] = (char)(0x80 | (cp & 0x3F));
			break;
		}
		return n;
	}

	/* WideCharToMultiByte - UTF-8 conversion for non-Windows.
	 *
	 * Callers branch on the return value, so the Win32 contract is what this
	 * has to reproduce, not just the transcoding:
	 *
	 *   - cbMultiByte == 0 is a size query. Nothing is written and the byte
	 *     count the conversion needs is returned.
	 *   - A destination too small for the whole conversion returns 0
	 *     (ERROR_INSUFFICIENT_BUFFER on Win32), never a length. A caller that
	 *     indexes with the return value therefore stays inside its buffer.
	 *     One deliberate divergence, measured against the real API: Win32
	 *     fills the destination with as much as fits before failing, and this
	 *     writes nothing. Every caller here zeroes its length before touching
	 *     the buffer, so the difference is unobservable in this tree, and
	 *     writing nothing is the safer of the two.
	 *   - cchWideChar == -1 means the source is NUL terminated; the
	 *     terminator is converted and counted. A non-negative cchWideChar
	 *     converts exactly that many units and appends no terminator.
	 *
	 * CodePage is honoured only as far as it matters here: every caller in
	 * this tree asks for CP_UTF8 and UTF-8 is what this produces. */
	static inline int WideCharToMultiByte(UINT CodePage, DWORD dwFlags,
		LPCWSTR lpWideCharStr, int cchWideChar,
		LPSTR lpMultiByteStr, int cbMultiByte,
		LPCSTR lpDefaultChar, BOOL* lpUsedDefaultChar) {
		int nNeeded = 0;
		int nWritten = 0;
		int i = 0;

		(void)CodePage; (void)dwFlags; (void)lpDefaultChar;

		if (lpUsedDefaultChar != NULL)
			*lpUsedDefaultChar = FALSE;

		if (lpWideCharStr == NULL || cchWideChar < -1 || cbMultiByte < 0)
			return 0;

		if (cchWideChar == -1) {
			int len = 0;
			while (lpWideCharStr[len]) len++;
			cchWideChar = len + 1;	/* the terminator converts too */
		}

		while (i < cchWideChar) {
			unsigned int cp = Platform_WideNextCodePoint(lpWideCharStr, cchWideChar, &i);
			nNeeded += Platform_Utf8Encode(cp, NULL, 0);
		}

		if (cbMultiByte == 0)
			return nNeeded;

		if (lpMultiByteStr == NULL || nNeeded > cbMultiByte)
			return 0;

		i = 0;
		while (i < cchWideChar) {
			unsigned int cp = Platform_WideNextCodePoint(lpWideCharStr, cchWideChar, &i);
			nWritten += Platform_Utf8Encode(cp, lpMultiByteStr + nWritten, cbMultiByte - nWritten);
		}

		return nWritten;
	}

	/* SetWindowText stub - no-op on non-Windows */
	static inline BOOL SetWindowText(void* hWnd, const char* lpText) {
		(void)hWnd; (void)lpText;
		return TRUE;
	}

	/* ShowWindow stub - no-op on non-Windows */
	static inline BOOL ShowWindow(void* hWnd, int nCmdShow) {
		(void)hWnd; (void)nCmdShow;
		return TRUE;
	}

	/* CreateWindowEx stub - returns NULL on non-Windows (no window creation) */
	static inline void* CreateWindowEx(DWORD dwExStyle, const char* lpClassName,
	                                    const char* lpWindowName, DWORD dwStyle,
	                                    int X, int Y, int nWidth, int nHeight,
	                                    void* hWndParent, void* hMenu, void* hInstance, void* lpParam) {
		(void)dwExStyle; (void)lpClassName; (void)lpWindowName; (void)dwStyle;
		(void)X; (void)Y; (void)nWidth; (void)nHeight;
		(void)hWndParent; (void)hMenu; (void)hInstance; (void)lpParam;
		return NULL; // No window creation on non-Windows platforms
	}

	/* RegisterClass stub - returns 0 (atom) on non-Windows */
	static inline unsigned short RegisterClass(const WNDCLASS* lpWndClass) {
		(void)lpWndClass;
		return 0;
	}

	/* Stock object constants */
	#define BLACK_BRUSH 4
	#define WHITE_BRUSH 0
	#define DC_BRUSH 18

	/* GetStockObject stub - returns NULL on non-Windows */
	static inline void* GetStockObject(int nIndex) {
		(void)nIndex;
		return NULL;
	}

	/* LoadIcon stub - returns NULL on non-Windows */
	static inline void* LoadIcon(void* hInstance, const char* lpIconName) {
		(void)hInstance; (void)lpIconName;
		return NULL;
	}

	/* LoadCursor stub - returns NULL on non-Windows */
	static inline void* LoadCursor(void* hInstance, const char* lpCursorName) {
		(void)hInstance; (void)lpCursorName;
		return NULL;
	}

	/* SetCursor stub - returns NULL on non-Windows */
	static inline void* SetCursor(void* hCursor) {
		(void)hCursor;
		return NULL;
	}

	/* UpdateWindow stub - does nothing on non-Windows */
	static inline BOOL UpdateWindow(void* hWnd) {
		(void)hWnd;
		return TRUE;
	}

	/* SetFocus stub - returns NULL on non-Windows */
	static inline void* SetFocus(void* hWnd) {
		(void)hWnd;
		return NULL;
	}

	/* DefWindowProc stub - default window procedure */
	static inline LRESULT DefWindowProc(void* hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
		(void)hWnd; (void)Msg; (void)wParam; (void)lParam;
		return 0;
	}

	/* PostQuitMessage stub - no-op on non-Windows */
	static inline void PostQuitMessage(int nExitCode) {
		(void)nExitCode;
	}

	/* GetDoubleClickTime stub - returns default 500ms on non-Windows */
	static inline int GetDoubleClickTime() {
		return 500; // Default double-click time in milliseconds
	}

	/* HMENU typedef */
	typedef void* HMENU;
	typedef void* HBRUSH;
	typedef void* HICON;
	typedef void* HCURSOR;

	/* Resource management macros */
	#define MAKEINTRESOURCE(i) (LPCTSTR)((DWORD_PTR)((WORD)(i)))

	/* Standard cursor */
	#define IDC_ARROW ((LPCTSTR)"MAKEINTRESOURCE(32512)")

	/* FAR PASCAL macros for Windows callback conventions */
	#ifndef FAR
		#define FAR
	#endif

	#ifndef PASCAL
		#define PASCAL
	#endif

	/* FARPROC - pointer to a function (Windows callback type) */
	typedef int (*FARPROC)();

	/* Callback function type */
	typedef long (__cdecl *WNDPROC)(void*, unsigned int, unsigned long, long long);

	/* IWebBrowser2 stub - COM interface for web browser control */
	#ifndef IWebBrowser2_DEFINED
	#define IWebBrowser2_DEFINED
	typedef void* IWebBrowser2;
	#endif

	/* Windows timing functions */
	#define GetTickCount()		platform_get_ticks()
	#define timeGetTime()		platform_get_ticks()
#endif

#ifdef PLATFORM_WINDOWS
	/* timeGetTime() lives in <mmsystem.h> (winmm), which this project no
	   longer includes since it also drags in HMMIO/MMCKINFO/... that
	   conflict with basic/AudioTypes.h's SDL stand-ins. Route it through
	   the same platform_get_ticks() used on every other platform instead
	   of pulling mmsystem.h back in just for this one function. */
	#ifndef timeGetTime
	#define timeGetTime()		platform_get_ticks()
	#endif
#endif

/* ============================================================================
 * Platform-Specific Types
 * ============================================================================ */

#ifdef PLATFORM_WINDOWS
	/* Windows types */
	#ifndef _WINDOWS_
		#define WIN32_LEAN_AND_MEAN
		#include <windows.h>
	#endif

	typedef HANDLE	platform_thread_t;
	typedef HANDLE	platform_mutex_t;
	typedef HANDLE	platform_event_t;
	typedef HMODULE	platform_lib_t;

	#define PLATFORM_INVALID_THREAD	NULL
	#define PLATFORM_INVALID_MUTEX	NULL
	#define PLATFORM_INVALID_EVENT	NULL
	#define PLATFORM_INVALID_LIB		NULL

#else
	/* SDL/POSIX types */
	typedef SDL_Thread*	platform_thread_t;
	typedef SDL_mutex*	platform_mutex_t;

	/* Forward declaration for event structure */
	typedef struct platform_event_s* platform_event_t;

	typedef void*	platform_lib_t;

	#define PLATFORM_INVALID_THREAD	NULL
	#define PLATFORM_INVALID_MUTEX	NULL
	#define PLATFORM_INVALID_EVENT	NULL
	#define PLATFORM_INVALID_LIB		NULL

#endif /* PLATFORM_WINDOWS */

/* ============================================================================
 * Exported C API
 * ============================================================================ */

/* Dependencies above and compatibility helpers below keep their own language
   linkage. Only the API implemented by PlatformSDL.cpp is exported as C. */
#ifdef __cplusplus
extern "C" {
#endif

/* Ordered font fallback paths; static storage, terminated by NULL. */
const char* const* platform_get_font_paths(void);

/* ============================================================================
 * Time Functions
 * ============================================================================ */

/**
 * Get current time in milliseconds (like timeGetTime/GetTickCount)
 * @return Time in milliseconds
 */
DWORD platform_get_ticks(void);

/**
 * Get high-performance counter value (like QueryPerformanceCounter)
 * @return Counter value
 */
uint64_t platform_get_performance_counter(void);

/**
 * Get high-performance counter frequency (like QueryPerformanceFrequency)
 * @return Counter frequency (counts per second)
 */
uint64_t platform_get_performance_frequency(void);

/**
 * Sleep for specified milliseconds (like Sleep)
 * @param ms Milliseconds to sleep
 */
void platform_sleep(DWORD ms);

/* ============================================================================
 * Thread Functions
 * ============================================================================ */

/**
 * Thread entry point type
 */
typedef DWORD (*platform_thread_func_t)(void* param);

/**
 * Create a new thread (like CreateThread)
 * @param func Thread function
 * @param param Parameter to pass to thread function
 * @return Thread handle or PLATFORM_INVALID_THREAD on failure
 */
platform_thread_t platform_thread_create(platform_thread_func_t func, void* param);

/**
 * Wait for thread to finish (like WaitForSingleObject on thread)
 * @param thread Thread handle
 * @return Wait result (0 = success, non-zero = failure)
 */
int platform_thread_wait(platform_thread_t thread);

/**
 * Close thread handle (like CloseHandle on thread)
 * @param thread Thread handle
 */
void platform_thread_close(platform_thread_t thread);

/* ============================================================================
 * Mutex Functions
 * ============================================================================ */

/**
 * Create a mutex (like CreateMutex)
 * @param initial_locked Whether mutex starts locked
 * @return Mutex handle or PLATFORM_INVALID_MUTEX on failure
 */
platform_mutex_t platform_mutex_create(int initial_locked);

/**
 * Lock a mutex (like WaitForSingleObject on mutex)
 * @param mutex Mutex handle
 * @return Lock result (0 = success, non-zero = failure)
 */
int platform_mutex_lock(platform_mutex_t mutex);

/**
 * Unlock a mutex (like ReleaseMutex)
 * @param mutex Mutex handle
 * @return Unlock result (0 = success, non-zero = failure)
 */
int platform_mutex_unlock(platform_mutex_t mutex);

/**
 * Close mutex handle (like CloseHandle on mutex)
 * @param mutex Mutex handle
 */
void platform_mutex_close(platform_mutex_t mutex);

/* ============================================================================
 * Event Functions
 * ============================================================================ */

/**
 * Create an event object (like CreateEvent)
 * @param manual_reset Whether event requires manual reset
 * @param initial_state Initial state (TRUE = signaled, FALSE = non-signaled)
 * @return Event handle or PLATFORM_INVALID_EVENT on failure
 */
platform_event_t platform_event_create(int manual_reset, int initial_state);

/**
 * Wait for event to be signaled (like WaitForSingleObject on event)
 * @param event Event handle
 * @param timeout Timeout in milliseconds (or PLATFORM_INFINITE for infinite wait)
 * @return Wait result (0 = success, non-zero = timeout/failure)
 */
int platform_event_wait(platform_event_t event, DWORD timeout);

/**
 * Signal an event (like SetEvent)
 * @param event Event handle
 * @return Signal result (0 = success, non-zero = failure)
 */
int platform_event_signal(platform_event_t event);

/**
 * Reset an event to non-signaled (like ResetEvent)
 * @param event Event handle
 * @return Reset result (0 = success, non-zero = failure)
 */
int platform_event_reset(platform_event_t event);

/**
 * Close event handle (like CloseHandle on event)
 * @param event Event handle
 */
void platform_event_close(platform_event_t event);

#define PLATFORM_INFINITE	((DWORD)-1)

/* The Win32 thread-wait names the game uses, over the event API above.
   These lived in Client/MWorkThread.h, a game header, so GameMain.cpp,
   CGameUpdate.cpp and MWorkThread.cpp reached them only through that
   include, and it defined them on Windows as well, beside the real
   declarations in <windows.h>. Off Windows only, here, like the rest
   of the shim. */
#ifndef PLATFORM_WINDOWS
	typedef DWORD (*LPTHREAD_START_ROUTINE)(void* lpParameter);

	#define THREAD_PRIORITY_NORMAL          0
	#define THREAD_PRIORITY_ABOVE_NORMAL    1
	#define THREAD_PRIORITY_BELOW_NORMAL   -1
	#define THREAD_PRIORITY_HIGHEST          2
	#define THREAD_PRIORITY_LOWEST          -2

	#define WAIT_OBJECT_0                   0
	#define WAIT_TIMEOUT                    258

	/* Waits on an event handle only: the game's one caller waits on the
	   file-loading event. A thread or mutex handle here is a misuse. */
	static inline DWORD WaitForSingleObject(HANDLE event, DWORD timeout) {
		platform_event_t evt = (platform_event_t)event;
		if (platform_event_wait(evt, timeout) == 0) {
			return WAIT_OBJECT_0;
		}
		return WAIT_TIMEOUT;
	}

	/* No-op: SDL threads are not re-prioritised after creation here. */
	static inline BOOL SetThreadPriority(HANDLE thread, int priority) {
		(void)thread; (void)priority;
		return TRUE;
	}

	/* The request-service managers in Client/Packet used to carry their
	   own copies of the next four, written over pthread_t, while the
	   thread they applied them to came from platform_thread_create - an
	   SDL_Thread*. pthread_cancel on an SDL_Thread* is not a thread
	   cancel. These are honest instead: SDL has no cancel and no
	   non-blocking exit-code query, so both report failure. The one
	   caller left, RequestServerPlayerManager, calls TerminateThread
	   only inside its PLATFORM_WINDOWS branch; the outbound manager
	   that discarded the result and looped on GetExitCodeThread is
	   deleted (docs/RESTRUCTURING.md task 5.2, eighth slice). The
	   request service is created on Windows only (GameInit.cpp), and
	   the manager exists off Windows because it is a packetwire
	   member. A real cancel needs a stop flag the thread function
	   polls. */
	#define STILL_ACTIVE 259
	static inline BOOL TerminateThread(HANDLE thread, DWORD exitCode) {
		(void)thread; (void)exitCode;
		return FALSE;
	}
	static inline BOOL GetExitCodeThread(HANDLE thread, LPDWORD lpExitCode) {
		(void)thread;
		if (lpExitCode) *lpExitCode = STILL_ACTIVE;
		return FALSE;
	}
	static inline HANDLE GetCurrentThread(void) {
		return (HANDLE)(size_t)SDL_ThreadID();
	}
#endif

/* ============================================================================
 * Dynamic Library Functions
 * ============================================================================ */

/**
 * Load a dynamic library (like LoadLibrary)
 * @param filename Library file name/path
 * @return Library handle or PLATFORM_INVALID_LIB on failure
 */
platform_lib_t platform_lib_load(const char* filename);

/**
 * Get function address from library (like GetProcAddress)
 * @param lib Library handle
 * @param symbol Function symbol name
 * @return Function pointer or NULL on failure
 */
void* platform_lib_get_symbol(platform_lib_t lib, const char* symbol);

/**
 * Unload a dynamic library (like FreeLibrary)
 * @param lib Library handle
 */
void platform_lib_free(platform_lib_t lib);

/* ============================================================================
 * File/Path Functions
 * ============================================================================ */

/**
 * Get path separator for current platform
 * @return Path separator character ('\\' on Windows, '/' on POSIX)
 */
char platform_get_path_separator(void);

/**
 * Check if file exists
 * @param filename File path to check
 * @return 1 if exists, 0 otherwise
 */
int platform_file_exists(const char* filename);

/**
 * Get current executable directory
 * @param buffer Buffer to store path
 * @param size Buffer size
 * @return 0 on success, non-zero on failure
 */
int platform_get_executable_dir(char* buffer, size_t size);

/**
 * Create directory if it doesn't exist
 * @param path Directory path
 * @return 0 on success, non-zero on failure
 */
int platform_create_directory(const char* path);

/* ============================================================================
 * Keyboard Functions (from PlatformUtil.h)
 * ============================================================================ */

/**
 * Check if Control key is currently pressed
 * @return 1 if pressed, 0 otherwise
 */
int platform_is_ctrl_pressed(void);

/**
 * Get keyboard scan code from lParam (Windows message parameter)
 * @param lParam LPARAM from keyboard message
 * @return Scan code
 */
BYTE platform_get_scan_code(DWORD lParam);

/* ============================================================================
 * Registry/Configuration Functions (Windows-only abstraction)
 * ============================================================================ */

/**
 * Get string value from configuration (replaces RegQueryValueEx)
 * @param key Configuration key name (e.g., "SOFTWARE\\Netmarble\\NetmarbleDarkEden")
 * @param value Value name
 * @param buffer Buffer to store value
 * @param size Buffer size (in/out)
 * @return 0 on success, non-zero on failure
 */
int platform_config_get_string(const char* key, const char* value,
                               char* buffer, DWORD* size);

/**
 * Set string value in configuration (replaces RegSetValueEx)
 * @param key Configuration key name
 * @param value Value name
 * @param data String data to set
 * @return 0 on success, non-zero on failure
 */
int platform_config_set_string(const char* key, const char* value,
                               const char* data);

/* ============================================================================
 * Error Reporting
 * ============================================================================ */

/**
 * Show error message box (like MessageBox)
 * @param title Message box title
 * @param message Error message
 */
void platform_show_error(const char* title, const char* message);

/* ============================================================================
 * Initialization
 * ============================================================================ */

/**
 * Initialize platform abstraction layer
 * Call this at program startup
 * @return 0 on success, non-zero on failure
 */
int platform_init(void);

/**
 * Cleanup platform abstraction layer
 * Call this at program shutdown
 */
void platform_shutdown(void);

#ifdef __cplusplus
}
#endif

/* ============================================================================
 * Windows Compatibility Macros
 * ============================================================================ */

// Define DLLIFC as empty on non-Windows platforms (for Immersion library compatibility)
#ifndef PLATFORM_WINDOWS
#ifndef DLLIFC
#define DLLIFC
#endif
#endif

/* The remaining Windows API shims below (types, structs, and stub functions)
   are only needed when compiling without <windows.h>; on real Windows builds
   <windows.h> (included above) already provides all of them. */
#ifndef PLATFORM_WINDOWS

// Windows constants that may be needed
#ifndef MAXLONG
#define MAXLONG 2147483647L  // 0x7FFFFFFF
#endif

#ifndef MAXDWORD
#define MAXDWORD 0xFFFFFFFF
#endif

/* ============================================================================
 * Rectangle Structure (Windows RECT equivalent)
 * ============================================================================ */

#ifndef RECT_DEFINED
#define RECT_DEFINED

/**
 * Point structure (equivalent to Windows POINT)
 * Used for defining 2D coordinates
 */
#ifndef POINT_DEFINED
#define POINT_DEFINED
typedef struct tagPOINT {
    LONG x;
    LONG y;
} POINT, *PPOINT, *LPPOINT;
#endif

/**
 * Size structure (equivalent to Windows SIZE). Used to live in
 * Client/Client_PCH.h alone, so a translation unit that reached this
 * header another way had RECT and POINT but no SIZE.
 */
#ifndef SIZE_DEFINED
#define SIZE_DEFINED
typedef struct tagSIZE {
    LONG cx;
    LONG cy;
} SIZE, *PSIZE, *LPSIZE;
#endif

/**
 * Rectangle structure (equivalent to Windows RECT)
 * Used for defining rectangular areas
 */
typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *PRECT, *LPRECT;

/**
 * MINMAXINFO structure (used in WM_GETMINMAXINFO)
 * Contains information about a window's maximized size and position
 */
typedef struct tagMINMAXINFO {
    POINT ptReserved;
    POINT ptMaxSize;
    POINT ptMaxPosition;
    POINT ptMinTrackSize;
    POINT ptMaxTrackSize;
} MINMAXINFO, *PMINMAXINFO, *LPMINMAXINFO;

/* SYSTEMTIME structure (date and time) */
typedef struct _SYSTEMTIME {
    WORD wYear;
    WORD wMonth;
    WORD wDayOfWeek;
    WORD wDay;
    WORD wHour;
    WORD wMinute;
    WORD wSecond;
    WORD wMilliseconds;
} SYSTEMTIME, *PSYSTEMTIME, *LPSYSTEMTIME;

/* DEVMODE structure (display mode settings) */
#define ENUM_CURRENT_SETTINGS ((DWORD)-1)
#define DM_BITSPERPEL 0x00040000
#define DM_PELSWIDTH 0x00080000
#define DM_PELSHEIGHT 0x00100000
#define DM_DISPLAYFREQUENCY 0x00400000

typedef struct _devicemode {
    char   dmDeviceName[32];
    WORD   dmSpecVersion;
    WORD   dmDriverVersion;
    WORD   dmSize;
    WORD   dmDriverExtra;
    DWORD  dmFields;
    union {
        struct {
            short dmOrientation;
            short dmPaperSize;
            short dmPaperLength;
            short dmPaperWidth;
            short dmScale;
            short dmCopies;
            short dmDefaultSource;
            short dmPrintQuality;
        };
        POINT dmPosition;
    };
    short  dmColor;
    short  dmDuplex;
    short  dmYResolution;
    short  dmTTOption;
    short  dmCollate;
    char   dmFormName[32];
    WORD   dmLogPixels;
    DWORD  dmBitsPerPel;
    DWORD  dmPelsWidth;
    DWORD  dmPelsHeight;
    DWORD  dmDisplayFlags;
    DWORD  dmDisplayFrequency;
} DEVMODE, *PDEVMODE, *LPDEVMODE;

#endif /* RECT_DEFINED */

/* ============================================================================
 * Windows File and Process API Stubs
 * ============================================================================ */

/* File time structure */
#ifndef FILETIME_DEFINED
#define FILETIME_DEFINED
typedef struct _FILETIME {
	DWORD dwLowDateTime;
	DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;
#endif

/* Security attributes structure */
#ifndef SECURITY_ATTRIBUTES_DEFINED
#define SECURITY_ATTRIBUTES_DEFINED
typedef struct _SECURITY_ATTRIBUTES {
	DWORD nLength;
	LPVOID lpSecurityDescriptor;
	BOOL bInheritHandle;
} SECURITY_ATTRIBUTES, *PSECURITY_ATTRIBUTES, *LPSECURITY_ATTRIBUTES;
#endif

/* Process access rights */
#ifndef PROCESS_ALL_ACCESS
	#define PROCESS_ALL_ACCESS (0xFFFF)
#endif

/* Display settings constants */
#ifndef CDS_RESET
	#define CDS_RESET 0x40000000
#endif
#ifndef CDS_UPDATEREGISTRY
	#define CDS_UPDATEREGISTRY 0x00000001
#endif
#ifndef DISP_CHANGE_SUCCESSFUL
	#define DISP_CHANGE_SUCCESSFUL 0
#endif
#ifndef DISP_CHANGE_RESTART
	#define DISP_CHANGE_RESTART 1
#endif
#ifndef DISP_CHANGE_FAILED
	#define DISP_CHANGE_FAILED -1
#endif

/* Opaque handle types */
typedef void* HANDLE;
typedef void* HWND;
typedef void* HINSTANCE;
typedef void* HMODULE;
typedef void* HKEY;

/* Registry constants */
#ifndef HKEY_LOCAL_MACHINE
	#define HKEY_LOCAL_MACHINE ((HKEY)0x80000002)
#endif

#ifndef KEY_ALL_ACCESS
	#define KEY_ALL_ACCESS (0xF003F)
#endif

#ifndef REG_SZ
	#define REG_SZ 1
#endif

#ifndef ERROR_SUCCESS
	#define ERROR_SUCCESS 0
#endif

/* Stub implementations for file and process operations */
#ifndef DeleteFile
static inline BOOL DeleteFileA(LPCSTR lpFileName) {
	(void)lpFileName;
	return FALSE;
}
#define DeleteFile DeleteFileA
#endif

#ifndef CopyFile
static inline BOOL CopyFileA(LPCSTR lpExistingFileName, LPCSTR lpNewFileName, BOOL bFailIfExists) {
	(void)lpExistingFileName; (void)lpNewFileName; (void)bFailIfExists;
	return FALSE;
}
#define CopyFile CopyFileA
#endif

#ifndef SetCurrentDirectory
static inline BOOL SetCurrentDirectoryA(LPCSTR lpPathName) {
	(void)lpPathName;
	return FALSE;
}
#define SetCurrentDirectory SetCurrentDirectoryA
#endif

#ifndef GetModuleFileName
static inline DWORD GetModuleFileNameA(HMODULE hModule, LPSTR lpFilename, DWORD nSize) {
	(void)hModule;
	if (lpFilename && nSize > 0) {
		lpFilename[0] = '\0';
		return 0;
	}
	return 0;
}
#define GetModuleFileName GetModuleFileNameA
#endif

#ifndef GetWindowThreadProcessId
static inline DWORD GetWindowThreadProcessId(HWND hWnd, LPDWORD lpdwProcessId) {
	(void)hWnd;
	if (lpdwProcessId) *lpdwProcessId = 0;
	return 0;
}
#endif

#ifndef TerminateProcess
static inline BOOL TerminateProcess(HANDLE hProcess, UINT uExitCode) {
	(void)hProcess; (void)uExitCode;
	return FALSE;
}
#endif

#ifndef INVALID_HANDLE_VALUE
	#define INVALID_HANDLE_VALUE ((HANDLE)(-1))
#endif

#ifndef CreateMutexA
static inline HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName) {
	(void)lpMutexAttributes; (void)bInitialOwner; (void)lpName;
	return (HANDLE)NULL;
}
#define CreateMutex CreateMutexA
#endif

#ifndef ReleaseMutex
static inline BOOL ReleaseMutex(HANDLE hMutex) {
	(void)hMutex;
	return FALSE;
}
#endif

#ifndef OpenProcess
static inline HANDLE OpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId) {
	(void)dwDesiredAccess; (void)bInheritHandle; (void)dwProcessId;
	return (HANDLE)NULL;
}
#endif

#ifndef GetLastError
static inline DWORD GetLastError() {
	return 0;
}
#endif

#ifndef _chdir
	#define _chdir chdir
#endif

/* Unix-style function name mappings for Windows compatibility */
#ifndef _access
	#define _access access
#endif

#ifndef _getcwd
	#define _getcwd getcwd
#endif

#ifndef _rmdir
	#define _rmdir rmdir
#endif

/* _P_OVERLAY for spawn function */
#ifndef _P_OVERLAY
	#define _P_OVERLAY 2
#endif

/* Registry types */
#ifndef REGSAM
typedef DWORD REGSAM;
#endif

#ifndef PHKEY
typedef HKEY* PHKEY;
#endif

/* No registry stubs. RegOpenKeyEx/RegQueryValueEx/RegSetValueEx/RegCloseKey
   used to return ERROR_SUCCESS here with empty data, so a caller could
   not tell a missing key from a present one; the tree has no live
   caller left (Client.cpp's Netmarble read is commented out), and the
   replacement for a new one is platform_config_get_string /
   platform_config_set_string below. */

/* Registry access mask type */
#ifndef REGSAM_DEFINED
	#define REGSAM_DEFINED
#endif

/* No EnumDisplaySettings / ChangeDisplaySettings stubs: the former
   reported a fixed 1024x768 whatever the display, the latter always
   failed, and the only callers are commented out in Client.cpp. The
   SDL display path is basic/DisplaySettings. */

#ifndef Sleep
	/* Sleep for specified milliseconds */
	#ifdef PLATFORM_WINDOWS
		/* Use Windows Sleep */
	#else
		/* Unix: use usleep */
		#include <unistd.h>
		static inline void Sleep(DWORD dwMilliseconds) {
			usleep(dwMilliseconds * 1000);
		}
	#endif
#endif

/* Spawn functions */
#ifndef _spawnl
static inline intptr_t _spawnl(int mode, const char* cmdname, const char* arg0, ...) {
	(void)mode; (void)cmdname; (void)arg0;
	return -1;
}
#endif

/* MSG structure for Windows messages */
#ifndef tagMSG_DEFINED
#define tagMSG_DEFINED
typedef struct tagMSG {
	HWND hwnd;
	UINT message;
	WPARAM wParam;
	LPARAM lParam;
	DWORD time;
	POINT pt;
} MSG, *PMSG, *LPMSG;
#endif

/* Message processing functions */
#ifndef GetMessage
static inline BOOL GetMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax) {
	(void)hWnd; (void)wMsgFilterMin; (void)wMsgFilterMax;
	if (lpMsg) {
		lpMsg->message = WM_QUIT;
		return FALSE;
	}
	return FALSE;
}
#endif

#ifndef TranslateMessage
static inline BOOL TranslateMessage(const MSG* lpMsg) {
	(void)lpMsg;
	return FALSE;
}
#endif

#ifndef DispatchMessage
static inline LRESULT DispatchMessage(const MSG* lpMsg) {
	(void)lpMsg;
	return 0;
}
#endif

#ifndef WaitMessage
/* WaitMessage blocks until a window message arrives; the frame loop calls
   it when the application is inactive. The SDL events are pumped by the
   loop itself, so the honest equivalent is a short sleep rather than a
   busy spin. */
static inline BOOL WaitMessage(void) {
	SDL_Delay(10);
	return TRUE;
}
#endif

/* PeekMessage flags */
#ifndef PM_NOREMOVE
	#define PM_NOREMOVE 0x0000
#endif

/* Console constants */
#ifndef MIN_CLRSCR
	#define MIN_CLRSCR 0
#endif

#ifndef MIN_SHOWWND
	#define MIN_SHOWWND 1
#endif

#ifndef MIN_HIDEWND
	#define MIN_HIDEWND 2
#endif

/* PeekMessage function */
#ifndef PeekMessageA
static inline BOOL PeekMessageA(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg) {
	(void)lpMsg; (void)hWnd; (void)wMsgFilterMin; (void)wMsgFilterMax; (void)wRemoveMsg;
	return FALSE;
}
#define PeekMessage PeekMessageA
#endif

/* System parameters info */
#ifndef SPI_SETSCREENSAVERRUNNING
	#define SPI_SETSCREENSAVERRUNNING 17
#endif

/* OS version info */
#ifndef OSVERSIONINFO_DEFINED
#define OSVERSIONINFO_DEFINED
typedef struct _OSVERSIONINFOA {
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwMinorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
	char szCSDVersion[128];
} OSVERSIONINFOA, *POSVERSIONINFOA, *LPOSVERSIONINFOA;
#define OSVERSIONINFO OSVERSIONINFOA
#endif

#ifndef VER_PLATFORM_WIN32_WINDOWS
	#define VER_PLATFORM_WIN32_WINDOWS 1
#endif

#ifndef VER_PLATFORM_WIN32_NT
	#define VER_PLATFORM_WIN32_NT 2
#endif

/* DirectInput key codes */
#ifndef DIK_NUMPADENTER
	#define DIK_NUMPADENTER 0x9C
#endif

/* Version checking */
#ifndef GetVersionExA
static inline BOOL GetVersionExA(LPOSVERSIONINFOA lpVersionInformation) {
	if (lpVersionInformation) {
		lpVersionInformation->dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
		lpVersionInformation->dwMajorVersion = 5;
		lpVersionInformation->dwMinorVersion = 1;
		lpVersionInformation->dwBuildNumber = 2600;
		lpVersionInformation->dwPlatformId = VER_PLATFORM_WIN32_NT;
		lpVersionInformation->szCSDVersion[0] = '\0';
	}
	return TRUE;
}
#define GetVersionEx GetVersionExA
#endif

/* Exception handling */
/* Exception filter type (must be defined before EXCEPTION_POINTERS) */
#ifndef LPTOP_LEVEL_EXCEPTION_FILTER
struct _EXCEPTION_POINTERS;
typedef LONG (*LPTOP_LEVEL_EXCEPTION_FILTER)(struct _EXCEPTION_POINTERS*);
#endif

#ifndef EXCEPTION_POINTERS_DEFINED
#define EXCEPTION_POINTERS_DEFINED
typedef struct _EXCEPTION_POINTERS {
	DWORD ExceptionCode;
	DWORD ExceptionFlags;
	void* ExceptionRecord;
	void* ExceptionAddress;
	DWORD NumberParameters;
	void* ExceptionInformation[15];
} EXCEPTION_POINTERS, *PEXCEPTION_POINTERS;
#endif

#ifndef SetUnhandledExceptionFilter
static inline LPTOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter) {
	(void)lpTopLevelExceptionFilter;
	return NULL;
}
#endif

/* DirectDraw caps */
#ifndef DDSCAPS_VIDEOMEMORY
	#define DDSCAPS_VIDEOMEMORY 0x00000040
#endif

typedef struct _DDCAPS {
	DWORD dwSize;
	DWORD dwCaps;
	DWORD dwCaps2;
	DWORD dwCKeyCaps;
	DWORD dwFXCaps;
	DWORD dwFXAlphaCaps;
	DWORD dwPalCaps;
	DWORD dwSVCaps;
	DWORD dwAlphaCaps;
	DWORD dwVideoPortCaps;
	DWORD dwVideoPortCaps2;
	DWORD dwVidMemTotal;
	DWORD dwVidMemFree;
	DWORD dwMaxVisibleOverhead;
} DDCAPS;

/* SetRect function */
static inline void SetRect(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom) {
    if (lprc) {
        lprc->left = xLeft;
        lprc->top = yTop;
        lprc->right = xRight;
        lprc->bottom = yBottom;
    }
}

#endif /* !PLATFORM_WINDOWS (Windows API shims started above) */

/* SDL text input relay messages (not real Win32 message IDs; used to carry
   SDL_TEXTINPUT/SDL_TEXTEDITING events through this codebase's message
   dispatch). Defined above for non-Windows; USE_SDL_BACKEND is mandatory on
   all platforms now (including Windows), so also define them here when
   <windows.h> didn't (real Win32 has no WM_TEXTINPUT/WM_TEXTEDITING). */
#ifndef WM_TEXTINPUT
#define WM_TEXTINPUT 0x0111
#define WM_TEXTEDITING 0x0110
#endif

/* MCI notification constants (normally <mmsystem.h>/<digitalv.h>, which this
   project no longer includes - see CWinUpdate.h). Used by Client.cpp's
   WindowProc to detect AVI intro playback completion. Values are stable,
   decades-old Win32 SDK constants. */
#ifndef MM_MCINOTIFY
#define MM_MCINOTIFY 0x03B9
#define MCI_NOTIFY_SUCCESSFUL 0x0001
#endif

/* min and max: see the function templates after the extern "C" block
   at the end of this header. They used to be macros here, and a
   function-like macro named min breaks every `min(` in the standard
   library headers included after this one - std::numeric_limits<T>::min()
   for one - which is how 1,155 of 1,243 translation units failed the
   first Linux compile (port assessment, step 0). */
#ifndef PLATFORM_WINDOWS
/* __int64 Windows type - use long long on POSIX */
typedef long long __int64;
/* _atoi64 Windows function - use atoll on POSIX */
#define _atoi64(x) atoll(x)
#endif

#ifndef PLATFORM_WINDOWS
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
/* Windows file attribute bits, the Win32 values */
#define FILE_ATTRIBUTE_DIRECTORY (0x00000010)
#define FILE_ATTRIBUTE_NORMAL    (0x00000080)

/* Windows Virtual Key Codes for keyboard input */
#ifndef VK_UP
#define VK_UP    0x26
#endif
#ifndef VK_DOWN
#define VK_DOWN  0x28
#endif
#ifndef VK_LEFT
#define VK_LEFT  0x25
#endif
#ifndef VK_RIGHT
#define VK_RIGHT 0x27
#endif
#ifndef VK_RETURN
#define VK_RETURN 0x0D
#endif
#ifndef VK_ESCAPE
#define VK_ESCAPE 0x1B
#endif
#ifndef VK_TAB
#define VK_TAB 0x09
#endif
#ifndef VK_BACK
#define VK_BACK 0x08
#endif
#ifndef VK_SPACE
#define VK_SPACE  0x20
#endif
#ifndef VK_SHIFT
#define VK_SHIFT  0x10
#endif
#ifndef VK_CONTROL
#define VK_CONTROL 0x11
#endif
#ifndef VK_HOME
#define VK_HOME 0x24
#endif
#ifndef VK_END
#define VK_END 0x23
#endif
#ifndef VK_DELETE
#define VK_DELETE 0x2E
#endif
#ifndef VK_INSERT
#define VK_INSERT 0x2D
#endif

/* DirectInput Key Codes */
#ifndef DIK_LCONTROL
#define DIK_LCONTROL 0x1D
#endif
#ifndef DIK_RCONTROL
#define DIK_RCONTROL 0x9D
#endif
#ifndef DIK_LSHIFT
#define DIK_LSHIFT 0x2A
#endif
#ifndef DIK_RSHIFT
#define DIK_RSHIFT 0x36
#endif

/* Windows macros for word manipulation */
#ifndef LOWORD
#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#endif
#ifndef HIWORD
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#endif
#ifndef LOBYTE
#define LOBYTE(w) ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#endif
#ifndef HIBYTE
#define HIBYTE(w) ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#endif

static inline DWORD GetLogicalDrives() {
    /* macOS stub - return no drives */
    return 0;
}

static inline DWORD GetCurrentDirectory(DWORD nBufferLength, LPSTR lpBuffer) {
    /* macOS stub - get current working directory */
    if (getcwd(lpBuffer, nBufferLength) != NULL) {
        return (DWORD)strlen(lpBuffer);
    }
    return 0;
}

/* GetLocalTime - fill SYSTEMTIME structure with current local time */
static inline void GetLocalTime(LPSYSTEMTIME lpSystemTime) {
    /* macOS stub - get current local time */
    if (lpSystemTime) {
        struct tm* now;
        time_t aclock;
        time(&aclock);
        now = localtime(&aclock);

        lpSystemTime->wYear = now->tm_year + 1900;
        lpSystemTime->wMonth = now->tm_mon + 1;
        lpSystemTime->wDayOfWeek = now->tm_wday;
        lpSystemTime->wDay = now->tm_mday;
        lpSystemTime->wHour = now->tm_hour;
        lpSystemTime->wMinute = now->tm_min;
        lpSystemTime->wSecond = now->tm_sec;
        lpSystemTime->wMilliseconds = 0;
    }
}
#endif

/* Copy the SDL surface descriptor used by CSpriteSurface on every platform. */
#include "2d.h"
static inline void SetSurfaceInfo(S_SURFACEINFO* dest, const S_SURFACEINFO* src) {
    if (dest && src) {
        dest->p_surface = src->p_surface;
        dest->width = src->width;
        dest->height = src->height;
        dest->pitch = src->pitch;
    }
}

/* DirectInput key codes for non-Windows platforms */
#ifndef PLATFORM_WINDOWS
/* DIK_LMENU and DIK_RMENU are the DirectInput names for Left/Right ALT */
#define DIK_LMENU           0x38
#define DIK_RMENU           0xB8
/* Alternate names for ALT keys */
#define DIK_LALT            DIK_LMENU
#define DIK_RALT            DIK_RMENU

/* Windows macros for creating LONG/LPARAM from values */
#ifndef MAKELONG
#define MAKELONG(a, b) ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD_PTR)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#endif
#ifndef MAKEWPARAM
#define MAKEWPARAM(l, h) ((WPARAM)(DWORD)MAKELONG(l, h))
#endif
#ifndef MAKELPARAM
#define MAKELPARAM(l, h) ((LPARAM)(DWORD)MAKELONG(l, h))
#endif
#ifndef MAKELRESULT
#define MAKELRESULT(l, h) ((LRESULT)(DWORD)MAKELONG(l, h))
#endif
#endif

/* min and max for the Windows code, off Windows.
 *
 * On Windows <windows.h> defines min and max as macros and the tree
 * calls them unqualified, about 700 lines in 64 files, mixing argument
 * types freely (max(1, someShort + 3)). std::min and std::max refuse
 * mixed types, so a plain `using std::min` would not carry that code.
 * These templates take two independent types and return their common
 * type, which is what the macro's conditional expression yielded.
 *
 * They coexist with `using namespace std;`: for a same-type call both
 * std::min<T>(const T&, const T&) and this template are viable, and
 * the standard one is the more specialized, so overload resolution
 * picks it and there is no ambiguity; for a mixed-type call deduction
 * fails for std::min and only this one remains. Not macros, so
 * `std::numeric_limits<T>::min()` and every other `min(` inside the
 * standard headers are left alone. C++ only: the C translation units
 * (deflate.c and friends) never used the Windows macros.
 *
 * Three ways this is not the macro, all compile-visible or benign:
 * a same-type call in a `using namespace std` file returns std::min's
 * const T& (so `const int& r = min(f(), g());` would dangle there and
 * not elsewhere - no site does that); a call on two unsigned chars or
 * shorts yields that type where the macro's conditional yielded int;
 * and a class type needs operator< rather than operator> for max. */
#if defined(__cplusplus) && !defined(PLATFORM_WINDOWS)
#include <type_traits>
template <typename A, typename B>
constexpr typename std::common_type<A, B>::type min(const A& a, const B& b)
{
	return (b < a) ? b : a;
}
template <typename A, typename B>
constexpr typename std::common_type<A, B>::type max(const A& a, const B& b)
{
	return (a < b) ? b : a;
}
#endif

#endif /* __PLATFORM_H__ */
