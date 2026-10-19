//-----------------------------------------------------------------------------
// Client.h
//-----------------------------------------------------------------------------

#ifndef	__CLIENT_H__
#define	__CLIENT_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif
//-----------------------------------------------------------------------------
// include
//-----------------------------------------------------------------------------
#include "MonotonicClock.h"
#include "SafeFormat.h"
#ifdef PLATFORM_WINDOWS
#ifndef _WINDOWS_
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif
#else
#include "../basic/Platform.h"
#include <fstream>
#endif
#include "DebugInfo.h"

#include "ClientDef.h"

//#include "MInput.h"
//#include "MMusic.h"
//#include "MySound.h"

//#include "CInputManager.h"

#include "DXLib/DXLib.h"
#include "SpriteLib/SP.h"
#include "FR.h"
// WinLib.h is CWinUpdate, the frame-update base, which is platform-neutral
// (CWinUpdate.cpp compiles everywhere). The non-Windows branch used to
// forward-declare the class instead, and ClientMain's loop, which calls
// Update() through it, then had an incomplete type.
#include "WinLib.h"


#include "MZone.h"
#include "MTopView.h"
#include "MPlayer.h"

#include "CSoundPartManager.h"

#include "MScreenEffectManager.h"

#include "SpriteLib/MLoadingSPKWorkNode.h"
#include "MItem.h"
#include "SpriteLib/CStorageSurface.h"

class CMessageArray;
class Player;


//-----------------------------------------------------------------------------
// Definitions
//-----------------------------------------------------------------------------
#ifdef __YHDK2__
#define PROGRAM_NAME                "YHDK2"
#define PROGRAM_TITLE               "YHDK2"
#else
#define PROGRAM_NAME                "TTDK2"
#define PROGRAM_TITLE               "TTDK2"
#endif

#define	PROGRAM_FILENAME			"fengshen.exe"
#define	UPDATER_FILENAME			"Updater.exe"
#define	UPDATER_NEW_FILENAME		"Updater2.exe"

// MAX_NPC is GameMain.h's (gameclient) since task 4.262.
#define	MAX_IMAGEOBJECT				0
#define	MAX_ANIMATIONOBJECT			0
#define	MAX_INTERACTIONOBJECT		0
#define	MAX_ITEM					0


// DebugMessage
#define	MAX_DEBUGMESSAGE	25
#define	MAX_SYSTEMMESSAGE	5
#define MAX_PLAYERMESSAGE	5
#define	MAX_GAMEMESSAGE		5
#define	MAX_HELPMESSAGE		5


//------------------------------------------------------------
// Update delay 
//------------------------------------------------------------
// 20 --> 50 Frame
// 33 --> 30 Frame
// 50 --> 20 Frame
// 62 --> 16 Frame
// 66 --> 15 Frame
// 80 --> 12 Frame
// 100 --> 10 Frame
// 111 --> 9 Frame
#define	LIMIT_FPS_FOR_CURSOR	24		// 부드러운(?) 커서 출력을 위한 최소FPS
// DELAY_UPDATE_LOGIN and DELAY_UPDATE_WAITING are ClientMode.h's (gamemodel)
// since task 4.262.
#define	DELAY_UPDATE_GAME		62//62  렝岺속醵홍애긴좆

#define SAFE_DELETE(x)		{ if(x!=NULL) delete x; x=NULL; }
#define SAFE_DELETE_ARRAY(x){ if(x!=NULL) delete []x; x=NULL;}


// SOUND_MIN, SOUND_RANGE and SOUND_DEGREE are AudioTypes.h's (basic) since
// task 4.258.

//-----------------------------------------------------------------------------
// Global data
//-----------------------------------------------------------------------------
extern HWND				g_hWnd;
extern HINSTANCE		g_hInstance;

// mouse cursor
extern int					g_x;
extern int					g_y;

// mp3
//CMySound*					g_pMP3 = NULL;

// Sound PartManager
extern CSoundPartManager*	g_pSoundManager;

extern POINT				g_SelectSector;
extern BOOL					g_bLButtonDown;
extern BOOL					g_bRButtonDown;
extern BOOL					g_bCButtonDown;
extern BOOL					g_bUIInput;

extern int					g_UpdateDelay;

// Zone
extern int					g_nZoneLarge;
extern int					g_nZoneSmall;
extern bool					g_bZoneLargeLoadImage;
extern bool					g_bZoneSmallLoadImage;
extern MZone*				g_pZoneLarge;
extern MZone*				g_pZoneSmall;
extern bool					g_bZonePlayerInLarge;
// g_ZoneAmbientSounds: AmbientSoundState.h (gamemodel's since task 4.246).

// EffectManager
extern MScreenEffectManager*	g_pInventoryEffectManager;

// DirectDraw
extern CSpriteSurface*		g_pBack;
extern CSpriteSurface*		g_pLast;
extern int					g_ScreenShotNumber;

// Execute Program --> bActiveApp
// minimize | anotherWnd click--> !ActiveGame
extern BOOL					g_bActiveApp; // Is application active?
extern BOOL					g_bActiveGame; // Is Game Active?
extern BOOL					g_bNeedUpdate; // update해야되나?


// FPS
extern MonotonicClock::TimePoint	g_FrameNow;	// the frame clock (FrameClock.h says when it is stamped)
void						StampFrameClock();
extern DWORD				g_CurrentFrame;

extern int					g_FrameCount;
extern int					g_StartFrameCount;
extern MonotonicClock::TimePoint	g_StartTime;
extern int					g_FrameRate;
extern bool				g_bGoodFPS;

// Chat string
#ifdef	OUTPUT_DEBUG
	extern CMessageArray*		g_pDebugMessage;
#endif

extern bool					g_bNetStatusGood;	// 네트웍 상태가 좋은가?

extern CMessageArray*		g_pSystemMessage;

extern CMessageArray*		g_pPlayerMessage;  //鯤소斤口잚

extern CMessageArray*		g_pGameMessage;
extern CMessageArray*		g_pHelpMessage;
extern CMessageArray*		g_pNoticeMessage;

// Server IP
//extern char				g_ServerIP[80];
extern bool				g_bFullScreen;
extern int				g_MaxNPC;
extern bool				g_bHAL;
extern bool				g_bMusicSW;

extern CWinUpdate*				g_pUpdate;
class CWaitPacketUpdate;
extern CWaitPacketUpdate*		g_pCWaitPacketUpdate;	// CWaitPacketUpdate.cpp's



//-----------------------------------------------------------------------------
// Global Function Prototypes
//-----------------------------------------------------------------------------

HRESULT InitFailMessage(const char* message);
template <typename... Args>
HRESULT InitFail(const char* format, Args... args)
{
	char message[1024];
	SafeFormat::Format(message, format, args...);
	return InitFailMessage(message);
}
//bool		CheckDXVersion();
void		ReleaseAllObjects();
void		ReleaseSocket();
void		ReleaseGameUpdate();
void		ReleaseGameObject();
void		UnInitSound();

BOOL		InitGame();

// ALT + TAB 처리
void		CheckActivate(BOOL bActiveGame);

// Init DX..
BOOL		InitInput();
BOOL		InitSound();
BOOL		InitDraw();

// Init Other
BOOL		InitClientConfig();
BOOL		InitUserOption();
BOOL		InitVolume();
BOOL		InitSurface();
BOOL		InitTimer();
BOOL		InitInfomation();
BOOL		InitGameObject();
BOOL		InitGameUpdate();
BOOL		InitPlayer(int x, int y, int dir);
void		InitSkillTree();		// the skill tree, and the domain experience the server ships
BOOL		InitSocket();

// Utility Func
BOOL		LoadZone(int n);
BOOL		LoadZoneInfo(int n);
void		MoveZone(int n);
BOOL		LoadCreature(int spriteType);
void		PlaySound(TYPE_SOUNDID soundID, bool repeat, int x, int y);
void		PlaySound(TYPE_SOUNDID soundID);
// PlaySound's body, the host's (WorldEventsHost.h).
void		PlaySoundAtPlayer(TYPE_SOUNDID soundID);
// Centred at full volume, whatever the per-second limit.
void		PlaySoundForce(TYPE_SOUNDID soundID);
void		StopSound(TYPE_SOUNDID soundID);
void		PlayMusicCurrentZone();

void		SetMode(enum CLIENT_MODE mode);
bool		UpdateSocketInput();
bool		UpdateSocketOutput();
void		UpdateDisconnected();

void		UpdateInput();
void		UpdateMouse();

void		SelectLastSelectedCharacter();
void		SaveLastSelectedCharacter( int slot );

// GetMakeItemFitPosition: ClientFunction.h

// Etc
void		MakeScreenShot();

// UI_ResultReceiver: UIFunction.h (GameUI.cpp's).

// 날씨
void		SetWeather(int weather, int level);
void		SetLightning(DWORD delay);

// file열기
bool		FileOpenBinary(const char* filename, std::ifstream& file);

// 2004, 3, 29 sobeit add start
void		Add_GDR_Ghost(int ZoneID);
// 2004, 3, 29 sobeit add end

// by csm 2004/12/14
int			GetPersnalShopUpdateTime();
// Pre-make table of advancement class Creature's frame. by sonee 2004.12.30
extern BYTE g_AdvanceVampireActionMaxCount[ ACTION_ADVANCEMENT_MAX - ACTION_ADVANCEMENT_STOP ];
extern BYTE g_AdvanceSlayerActionMaxCount[ ACTION_ADVANCEMENT_SLAYER_MAX - ACTION_ADVANCEMENT_SLAYER_STOP_SWORD ];
extern BYTE g_AdvanceOustersActionMaxCount[ ACTION_ADVANCEMENT_OUSTERS_MAX - ACTION_ADVANCEMENT_OUSTERS_CHAKRAM_STOP ];

//Add by sonic 2006.7.29
BOOL		GetSystem();
#endif
