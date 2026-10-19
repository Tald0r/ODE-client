//-----------------------------------------------------------------------------
// GameMain.cpp
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Include files
//-----------------------------------------------------------------------------
#include "Client_PCH.h"
#include "CrtCompat.h"
#include "DataPath.h"

#ifdef PLATFORM_WINDOWS
// <MMSystem.h> not included: this file only uses timeGetTime()/GetTickCount(),
// which basic/Platform.h already routes through platform_get_ticks(); including
// the real header here conflicts with that macro (see basic/Platform.h).
#include <process.h>
#include <io.h>
#include <share.h>
#include <direct.h>
#include <fcntl.h>	// _O_RDONLY (used with _open() below)

// SpriteLib backend: maps letterboxed window coordinates back to game
// coordinates (see UpdateMouse below).
extern "C" void spritectl_window_to_game_coords(int* x, int* y);
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif // PLATFORM_WINDOWS

#include "TextSystem/TextService.h"

#include "Packet/WireHost.h"
#include "FrameClock.h"
#include "InputState.h"
#include "GameMessages.h"
#include "../basic/CMessageArray.h"
#include "GameSocket.h"
#include "ClientConfig.h"
#include "GameInitInfo.h"
#include "PacketFunction.h"
#include "MPlayer.h"
#include "MTopView.h"
#include "MZoneTable.h"
#include "MGameTime.h"
#include "MWeather.h"
#include "MSoundTable.h"
#include "MMusicTable.h"
#include "DXLib/COGGSTREAM.h"
#include "UIFunction.h"
#include "Properties.h"
#include "DebugLog.h"
#include "PacketDef.h"
#include "SoundNode.h"
#include "UIDialog.h"
#include "MusicDef.h"
#include "MusicSelection.h"
#include "AmbientSoundState.h"
#include "ZoneInfoData.h"
#include "CServerInformation.h"
#include "UserInformation.h"
#include "TempInformation.h"

#include "CWaitUIUpdate.h"
#include "CWaitPacketUpdate.h"
#include "CGameUpdate.h"
#include "COrderedList.h"
#include "SpriteIDDef.h"
#include "MMusic.h"
#include "MChatManager.h"
#include "MGameStringTable.h"
#include "MHelpManager.h"
#include "DXLib/CSoundPartManager.h"
#include "MParty.h"
#include "MZoneSound.h"
#include "MZoneSoundManager.h"
#include "SoundDef.h"
#include "RequestServerPlayerManager.h"
#include "ClientCommunicationManager.h"
#include "MJusticeAttackManager.h"
#include "PCConfigTable.h"
#include "Profiler.h"
#include "MEventManager.h"
#include "MNPC.h"


#include "MWarManager.h"
#include "MTimeItemManager.h"

#include "VS_UI.h"

#include "MGuildType.h"
#include "MFakeCreature.h"

#include "SXml.h"
#include "ZoneFlags.h"
#include "ZoneLoadingHost.h"
#include "GameMain.h"

#include <optional>

//add by zdj 2005.5.17
extern BOOL g_MyFull;
extern RECT g_GameRect;

bool  g_bCheckHack = true;
#define MAX_INVALID_PROCESS 20

std::string g_strBadProcessList[MAX_INVALID_PROCESS] =
{
	"变速",
	"加速",
	"fpe",
	"金山游侠",
	"game master",
	"gameice",
	"gamehack",
	"游戏修改大师",
	"东方不败",
	"accelerat",
	"wpe",
	"winsock expert",
	"a speeder",
	"彩云",
	"零点",
	"蓝月",
	"时空游侠",
	"时空游侠标准版",
	"蓝※月",
	"flyodbg",
};

extern BOOL g_bSetHotKey;
extern CSoundPartManager*	g_pSoundManager;

//-----------------------------------------------------------------------------
// Global
//-----------------------------------------------------------------------------

// The executable's services (GameInit installs them; see GameMain.h).
static const GameMainHost*	s_pGameMainHost = NULL;

const GameMainHost*
SetGameMainHost(const GameMainHost* pHost)
{
	const GameMainHost* pPrevious = s_pGameMainHost;
	s_pGameMainHost = pHost;
	return pPrevious;
}

// What GameMain remembers from one call to the next. Each was a function-local
// static; an empty optional is one the first call has not set yet, from the
// frame clock or its argument as the static was. UpdateSocketInput: when the
// peer managers update next. SetMode(MODE_WAIT_SETPOSITION): whether the help
// key message is still to be shown. SetWeather: the weather before this one.
static std::optional<MonotonicClock::TimePoint>	s_NextManagerUpdate;
static int										s_FirstSetPosition = 1;
static std::optional<int>						s_PreviousWeather;

void
ResetGameMainState()
{
	s_NextManagerUpdate.reset();
	s_FirstSetPosition = 1;
	s_PreviousWeather.reset();
}

int g_previousSoundID = SOUNDID_NULL;

	
// Sound PartManager
// g_pSoundManager is GameInitInfo.cpp's (gameclient's) since task 4.252.

// mouse cursor
// g_SelectSector, g_bLButtonDown, g_bRButtonDown and g_bNetStatusGood are
// InputState.cpp's (gamemodel, task 4.247), g_bCButtonDown since task 4.261.
// g_bUIInput is InputState.cpp's (gamemodel) since task 4.258.

// g_UpdateDelay is ClientMode.cpp's (gamemodel) since task 4.255.

// Zone
// g_ZoneCreatureColorSet is ZoneFlags.cpp's.
// g_bZoneSafe, g_nZoneLarge, g_nZoneSmall and g_bZonePlayerInLarge are
// ZoneFlags.cpp's, and so are the large and small zones and whether their
// images are loaded (task 4.253).
// g_ZoneAmbientSounds is AmbientSoundState.cpp's (gamemodel, task 4.246).
// g_bWatchMode is ClientMode.cpp's (gamemodel) since task 4.258.


// g_pInventoryEffectManager is MScreenEffectManager.cpp's (task 4.253).

// DirectDraw
// g_pBack and g_pLast are ClientMode.cpp's (gamemodel) since task 4.255.
int						g_ScreenShotNumber = 0;

// g_SoundPerSecond is AmbientSoundState.cpp's (gamemodel) since task 4.261.

// socket: GameSocket.cpp (gamemodel)

// g_pLoadingThread is GameInit.cpp's since task 4.262.

// g_bFullScreen is gamemodel's (ClientMode.cpp) since task 4.253.
int					g_MaxNPC		= MAX_NPC;
bool				g_bHAL			= true;
bool				g_bMusicSW		= true;
extern int			g_MorphCreatureType;

// g_Mode and g_ModeNext are gamemodel's (ClientMode.cpp); the message
// logs' pointers too (GameMessages.cpp).

// g_pUpdate is CWinUpdate.cpp's (gamemodel) since task 4.255.

extern void Add_GDR_Effect(int nEffect, bool bAppearBossMonster);
extern void Add_GDR_Potal_Effect(int nMapID);
//---------------------------------------------------------------------------
// Update Socket Input
//---------------------------------------------------------------------------
bool
UpdateSocketInput()
{

	
	try {
		
		if (g_pSocket != NULL)
		{
			g_pSocket->processInput();
			g_pSocket->processCommand();
		}

	} catch (Throwable &t) 	{

		if( strstr( t.toString().c_str(), "InvalidProtocolException") != NULL )
		{
			// The exception text can embed packet derived data, so it is passed
			// as an argument and never as the format string.
			if( strstr( t.toString().c_str(), "(datagram)" ) == NULL )
				SendBugReport( "%s", t.toString().c_str() );
		}

		LOG_ERROR("%s", t.toString().c_str());
		LOG_ERROR("[Error] UpdateSocketInput");			
		LOG_ERROR("%s", t.toString().c_str());
		
		SetMode( MODE_MAINMENU );
		UpdateDisconnected();


		return false;
	}


	//----------------------------------------------------------------
	// RequestServerPlayerManager도 처리한다.
	//----------------------------------------------------------------
	if (!s_NextManagerUpdate) s_NextManagerUpdate = g_FrameNow;
	MonotonicClock::TimePoint& nextTime = *s_NextManagerUpdate;

	if (nextTime <= g_FrameNow)
	{	

		if (g_pRequestServerPlayerManager!=NULL)
		{

			g_pRequestServerPlayerManager->Update();

		}

		if (g_pClientCommunicationManager!=NULL)
		{

			g_pClientCommunicationManager->Update();

		}


		// three updates a second
		nextTime = g_FrameNow + MonotonicClock::Millis(330);
	}

	

	return true;
}

//---------------------------------------------------------------------------
// Update Socket Output
//---------------------------------------------------------------------------
bool
UpdateSocketOutput()
{

	
	try {
		
		if (g_pSocket != NULL)
		{
			g_pSocket->processOutput();
		}

	} catch (Throwable &t) 	{

		// The exception text can embed packet derived data, so it is passed as
		// an argument and never as the format string.
		if( strstr( t.toString().c_str(), "InvalidProtocolException") != NULL )
		{
			if( strstr( t.toString().c_str(), "(datagram)" ) == NULL )
				SendBugReport( "%s", t.toString().c_str() );
		}

		DEBUG_ADD_ERR("[Error] UpdateSocketInput");
		DEBUG_ADD_ERR(t.toString().c_str());
		
		SetMode( MODE_MAINMENU );
		UpdateDisconnected();


		return false;
	}

	
 
	return true;
}


bool ContainsIgnoreCase(const char* haystack, const char* needle) {
	if (!haystack || !needle) return false;
	size_t needleLen = strlen(needle);
	if (needleLen == 0) return true;

	for (; *haystack != '\0'; ++haystack) {
		if (_strnicmp(haystack, needle, needleLen) == 0) {
			return true;
		}
	}
	return false;
}

// Detects cheat tools by their window titles (yckou).
bool CheckInvalidProcess()
{
#ifdef PLATFORM_WINDOWS
	// Windows-specific anti-cheat check.
	if (g_bCheckHack)
	{
		HWND hCurrentWindow;
		char szText[255];
		hCurrentWindow = GetWindow(g_hWnd,GW_HWNDFIRST);
		while (hCurrentWindow != NULL)
		{
			// No outside cheat tool owns the game's own windows, and for those
			// GetWindowText sends WM_GETTEXT instead of reading the stored title.
			DWORD processID = 0;
			GetWindowThreadProcessId(hCurrentWindow, &processID);
			if (processID != GetCurrentProcessId()
				&& GetWindowText(hCurrentWindow, szText, 255)>0)
			{
				int iLen = static_cast<int>(strlen(szText));
				for (int j=0;j<iLen;j++)
				{
					if(isupper((unsigned char)szText[j]) != 0)
						szText[j] = (char)tolower((unsigned char)szText[j]);
				}


				for (int i=0;i<MAX_INVALID_PROCESS;i++)
				{
					if (ContainsIgnoreCase(szText, g_strBadProcessList[i].c_str()) &&
						!ContainsIgnoreCase(szText, "microsoft internet explorer") &&
						!ContainsIgnoreCase(szText, "myie") &&
						!ContainsIgnoreCase(szText, "dudu") &&
						!ContainsIgnoreCase(szText, "苟潼"))
					{
						g_bCheckHack = false;
						// The client has to be updated.
						g_bNeedUpdate = TRUE;
						SetMode(MODE_QUIT);
						g_ModeNext = MODE_QUIT;
						g_bCheckHack = true;
						return false;
					}
				}
			}
			hCurrentWindow = GetWindow(hCurrentWindow, GW_HWNDNEXT);
		}
	}
#else
	// Anti-cheat process checking is Windows-specific
	// On other platforms, simply return true (no invalid processes detected)
#endif
	return true;
}

//-----------------------------------------------------------------------------
// Client의 실행 Mode를 설정한다.
//-----------------------------------------------------------------------------
// game 상태 변경과 
// UI의 mode변경을 처리한다.
//
// (!)다른 곳~에서는 game상태변경이나 UI mode변경을 처리하지 않도록 해야한다.
//-----------------------------------------------------------------------------
void		
SetMode(enum CLIENT_MODE mode)
{
	g_Mode = mode;

	// 입력을 초기화한다.
	if (g_pSDLInput!=NULL)
	{
		g_pSDLInput->UpdateInput();
		g_pSDLInput->Clear();
	
		// acquire
		g_pSDLInput->SetAcquire(true);			
		
		// 입력을 초기화한다.
		g_pSDLInput->Clear();
	}

	g_bLButtonDown = FALSE;
	g_bRButtonDown = FALSE;
	g_bCButtonDown = FALSE;


	switch (g_Mode)
	{
		//------------------------------------------------------
		// 뭔가 option을 바꾼다. - -;
		//------------------------------------------------------
		case MODE_CHANGE_OPTION :
			{
			}
			//--------------------------------------------------
			// 음악 멈춘다.
			//--------------------------------------------------
			if (g_pUserOption->PlayWaveMusic)
			{
					if( g_SDLAudio.IsInit() )
						g_pOGG->streamClose();
			}
			else
			{
				g_Music.Stop();
			}

			CSDLGraphics::RestoreGammaRamp();
			CSDLGraphics::ReleaseAll();

			s_pGameMainHost->InitDraw();
			s_pGameMainHost->InitSurface();

			//-----------------------------------------------------------------
			// 잠시 기다려 달라고 출력..
			//-----------------------------------------------------------------
			g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_WAIT].GetString(), -1, -1, 0 );

			gC_vs_ui.Process();
			gC_vs_ui.Show();

			//-----------------------------------------------------------------
			// Last를 Back으로 copy - 3D HAL이 아닌 경우만..
			//-----------------------------------------------------------------
			{
				POINT point = { 0, 0 };
				RECT rect = { 0, 0, g_GameRect.right, g_GameRect.bottom };

				g_pBack->BltNoColorkey( &point, g_pLast, &rect );
			}

			CSDLGraphics::Flip();

			//-----------------------------------------------------------------
			// g_pTopView : 2D <--> 3D 바뀔때..
			//-----------------------------------------------------------------
			if (g_pTopView!=NULL && !g_pTopView->InitChanges())
			{
				LOG_ERROR("Game view restoration failed; stopping before another frame");
				g_pUIDialog->CloseMessageDlg();
				SetMode(MODE_QUIT);
				return;
			}

			g_pUIDialog->CloseMessageDlg();

			// 바로 다음에 MAINMENU로.. - -;
			SetMode( MODE_MAINMENU );
		break;

		//------------------------------------------------------
		// Login 상태 설정..
		//------------------------------------------------------
		case MODE_MAINMENU :
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MAINMENU");
			
			// acquire
			g_pSDLInput->SetAcquire(g_bActiveGame==TRUE);


			//------------------------------------------------------
			// Socket 해제..
			//------------------------------------------------------
			if(NULL == g_pUserInformation->pLogInClientPlayer &&
				true == g_pUserInformation->IsAutoLogIn)// 웹 로긴이고 한번도 로그인 하지 않았을경우
				s_pGameMainHost->ReleaseSocket();


			//------------------------------------------------------
			// [ TEST CODE ]
			// 여기서 sound를 초기화해도 되남??
			//------------------------------------------------------
			s_pGameMainHost->InitSound();

			g_CurrentFrame		= 0;

			g_bSetHotKey	= FALSE;

			//------------------------------------------------------
			// ui dialog
			//------------------------------------------------------
			g_pUIDialog->HidePCTalkDlg();


			//------------------------------------------------------
			// 모두 대화 허용
			//------------------------------------------------------
			g_pChatManager->ClearID();
			g_pChatManager->SetAcceptMode();

			// Title화면 UI시작
			gC_vs_ui.StartTitle();	
		
			gC_vs_ui.DropItem();

			g_bUIInput = FALSE;			

			// 귓속말 안 받을려고... 냠 -- ;
			if (g_pUserInformation!=NULL)
			{
				// 귓속말 대상을 지워준다.
				g_pUserInformation->WhisperID.Release();

				g_pUserInformation->CharacterID.Release();
			}

			//----------------------------------------------
			// message 제거
			//----------------------------------------------
			g_pSystemMessage->Clear();
			g_pPlayerMessage->Clear();
			g_pGameMessage->Clear();
			g_pNoticeMessage->Clear();


			//------------------------------------------------------
			// 음악 시작
			//------------------------------------------------------
			if (g_pUserOption->PlayWaveMusic)
			// Music playback - use SDL_mixer (cross-platform)
			g_Music.Stop();

			if (g_pUserOption->PlayMusic)
			{
				int musicID = g_pClientConfig->MUSIC_THEME;

				if (musicID!=MUSICID_NULL)
				{
					g_Music.Play( (*g_pMusicTable)[ musicID ].Filename );
				}
			}

			//----------------------------------------------
			// update 함수 설정
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_LOGIN;	
			g_pUpdate = g_pCWaitUIUpdate;
			g_pCWaitUIUpdate->Init();
		break;

		//------------------------------------------------------
		// PC를 선택하기 위한 정보를 기다린다.
		//------------------------------------------------------
		case MODE_NEWUSER :
		{
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_NEWUSER");
			
			//----------------------------------------------
			// update 함수 설정
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_LOGIN;	
			g_pUpdate = g_pCWaitUIUpdate;
			g_pCWaitUIUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// Login이 성공되기를 기다린다.
		//------------------------------------------------------
		// (ID, Password)를 보내고
		// Login이 성공 되기를 기다린다.
		//------------------------------------------------------
		case MODE_WAIT_LOGINOK :
		{
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_LOGINOK");
			
			//----------------------------------------------
			// update 함수 설정
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_WAITING;			
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );			
			g_pCWaitPacketUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// MODE_WAIT_WORLD_LIST
		//------------------------------------------------------
		case MODE_WAIT_WORLD_LIST :
		{
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_WORLD_LIST");

			//----------------------------------------------
			// update 함수 설정
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_WAITING;			
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );			
			g_pCWaitPacketUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// MODE_WAIT_SELECT_WORLD
		//------------------------------------------------------
		case MODE_WAIT_SELECT_WORLD :
		{
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_SELECT_WORLD");

			g_pUserInformation->KeepConnection = FALSE;			

			//----------------------------------------------
			// update 함수 설정
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_LOGIN;	
			g_pUpdate = g_pCWaitUIUpdate;
			g_pCWaitUIUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// MODE_WAIT_SERVER_LIST
		//------------------------------------------------------
		case MODE_WAIT_SERVER_LIST :
		{
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_SERVER_LIST");
			
			//----------------------------------------------
			// update 함수 설정
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_WAITING;			
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );			
			g_pCWaitPacketUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// MODE_WAIT_SELECT_SERVER
		//------------------------------------------------------
		case MODE_WAIT_SELECT_SERVER :
		{
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_SELECT_SERVER");

			g_pUserInformation->KeepConnection = FALSE;			

			//----------------------------------------------
			// update 함수 설정
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_LOGIN;	
			g_pUpdate = g_pCWaitUIUpdate;
			g_pCWaitUIUpdate->Init();
		}
		break;

		
		//------------------------------------------------------
		// 잘못 입력한 경우이다.
		//------------------------------------------------------
		case MODE_LOGIN_WRONG :
		{
			// 다시 mainmenu로..

			g_Mode = MODE_MAINMENU;

			//-----------------------
			// Socket 해제..
			//-----------------------
			s_pGameMainHost->ReleaseSocket();

			//----------------------------------------------
			// update 함수 설정
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_LOGIN;	
			g_pUpdate = g_pCWaitUIUpdate;
			g_pCWaitUIUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// Register되기를 기다린다.
		//------------------------------------------------------		
		case MODE_WAIT_REGISTERPLAYEROK :
		{
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_REGISTERPLAYEROK");
			
			//----------------------------------------------
			// update 함수 설정
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_WAITING;			
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );
			g_pCWaitPacketUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// PC를 선택하기 위한 정보를 기다린다.
		//------------------------------------------------------
		case MODE_WAIT_PCLIST :
		{			
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_PCLIST");				
			
			//------------------------------------------------------------
			// Characer 선택 창을 띄워야 한다.
			//------------------------------------------------------------
			// LCPCListHandler에서 하게 했다.

			g_ZoneAmbientSounds.SetNextSoundTime(g_FrameNow);

			//----------------------------------------------
			// update 함수 설정
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_WAITING;		
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );
			g_pCWaitPacketUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// PC를 선택하기 위한 정보를 기다린다.
		//------------------------------------------------------
		case MODE_WAIT_SELECTPC :
		{
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_SELECTPC");
			
			//----------------------------------------------
			// Player의 정보 초기화
			//----------------------------------------------
			g_pUserInformation->KeepConnection = FALSE;
			

			SelectLastSelectedCharacter();

			//----------------------------------------------
			// update 함수 설정
			//----------------------------------------------
			g_MorphCreatureType = 0;
			g_UpdateDelay = DELAY_UPDATE_LOGIN;	
			g_pUpdate = g_pCWaitUIUpdate;
			g_pCWaitUIUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// PC가 제대로 생성되었나?
		//------------------------------------------------------
		case MODE_WAIT_CREATEPCOK :
		{
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_CREATEPCOK");
			
			//----------------------------------------------
			// update 함수 설정
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_WAITING;		
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );
			g_pCWaitPacketUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// PC가 제대로 생성되었나?
		//------------------------------------------------------
		case MODE_WAIT_DELETEPCOK :
		{
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_DELETEPCOK");
			
			//----------------------------------------------
			// update 함수 설정
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_WAITING;		
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );
			g_pCWaitPacketUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// GameServer로 접속하기 위해서...
		//------------------------------------------------------
		case MODE_WAIT_RECONNECT :
		{				
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_RECONNECT");
			
			//----------------------------------------------
			// update 함수 설정
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_WAITING;		
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );
			g_pCWaitPacketUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// GameServer에서 LoginServer로 재접속하기 위해서...
		//------------------------------------------------------
		case MODE_WAIT_RECONNECT_LOGIN :
		{				
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_RECONNECT_LOGIN");

			// 귓속말 안 받을려고... 냠 -- ;
			if (g_pUserInformation!=NULL)
			{
				g_pUserInformation->CharacterID.Release();
			}
			
			//----------------------------------------------
			// update 함수 설정
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_WAITING;		
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );
			g_pCWaitPacketUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// UpdateInfo를 받는다.
		//------------------------------------------------------
		case MODE_WAIT_UPDATEINFO :
		{	
			//--------------------------------------------------
			// Player 초기화
			//--------------------------------------------------
			if (g_pPlayer!=NULL)
			{
				if (g_pZone!=NULL)
				{
					g_pZone->RemoveCreature( g_pPlayer->GetID() );
				}
				
				delete g_pPlayer;
				g_pPlayer = NULL;
			}

			//--------------------------------------------------
			// 정당방위 초기화
			//--------------------------------------------------
			g_pJusticeAttackManager->Release();

			//--------------------------------------------------
			//
			// Skill Tree 초기화 - 임시로.. - -;;
			//
			//--------------------------------------------------
			ZoneLoading::InitSkillTree();

			//------------------------------
			// 게임 UI 를 시작한다.
			//------------------------------
			UI_SetCharInfoName(g_pUserInformation->CharacterID);
			DEBUG_ADD("[ SetMode ] SET CHARINFO");
			UI_StartGame();

			DEBUG_ADD("[ SetMode ] UI FINISH");

			g_ZoneAmbientSounds.SetNextSoundTime(g_FrameNow);

			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_UPDATEINFO");
			
			//----------------------------------------------
			// update 함수 설정
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_WAITING;		
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );
			g_pCWaitPacketUpdate->Init();			
		}
		break;

		//------------------------------------------------------
		// Player의 좌표를 받기 위해서 기다린다.
		//------------------------------------------------------
		case MODE_WAIT_SETPOSITION :
		{
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_SETPOSITION");
			
			//--------------------------------------------------
			// Option 저장
			//--------------------------------------------------
			if (g_pUserOption!=NULL)
			{
				g_pUserOption->SaveToFile( g_pFileDef->getProperty("FILE_INFO_USEROPTION").c_str());
			}
			
			// 정지..
			g_pPlayer->SetStop();
			g_pPlayer->SetAction( ACTION_STAND );

			// 키 입력 제거
			g_pSDLInput->Clear();

			g_bLButtonDown = FALSE;
			g_bRButtonDown = FALSE;
			g_bCButtonDown = FALSE;

			// [ TEST CODE ]
			s_pGameMainHost->InitSound();

			int& first = s_FirstSetPosition;
			if (first)
			{
				g_pSystemMessage->Add((*g_pGameStringTable)[STRING_MESSAGE_HELP_KEY].GetString());
				first = 0;
			}

			//--------------------------------------------------
			// 게임 서버로 CGReady 패킷을 보낸다.
			//--------------------------------------------------
				CGReady cgReady;
				g_pSocket->sendPacket( &cgReady );
				g_pSocket->setPlayerStatus( CPS_WAITING_FOR_GC_SET_POSITION );

				// 바로 보낸다.
				UpdateSocketOutput();

			g_SelectSector.x = 0;
			g_SelectSector.y = 0;


			if (g_pTopView!=NULL)
			{
				g_pTopView->ClearShadowManager();
						
				g_pTopView->SetSelectedNULL();
			}
		
			// update 함수
			g_UpdateDelay = DELAY_UPDATE_WAITING;			
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );
			g_pCWaitPacketUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// 게임을 시작한다.
		//------------------------------------------------------
		case MODE_GAME :
			DEBUG_ADD("---------- Start Game ---------- ");

			DEBUG_ADD("CSDLGraphics::RestoreAllSurfaces()");
			CSDLGraphics::RestoreAllSurfaces();

			DEBUG_ADD("CDirect3D::Restore() - removed (SDL2)");

			DEBUG_ADD("TempInformation");

			//-----------------------------------------------------------
			// temp information제거
			//-----------------------------------------------------------
			g_pTempInformation->SetMode(TempInformation::MODE_NULL);

			DEBUG_ADD("UserInformation");
			//-----------------------------------------------------------
			// cancel the logout
			//-----------------------------------------------------------
			g_pUserInformation->CancelLogout();
		
			//-----------------------------------------------------------
			// hot key를 저장한다.
			//-----------------------------------------------------------
			DEBUG_ADD("SaveHotKey");
			UI_SaveHotKeyToServer();			

			//-----------------------------------------
			// Button 초기화
			//-----------------------------------------
			g_bLButtonDown = FALSE;
			g_bRButtonDown = FALSE;
			g_bCButtonDown = FALSE;

			//-----------------------------------------
			// UI Dialog에서 입력 막은거 풀어줌..
			//-----------------------------------------
			DEBUG_ADD("UIDialog");
			UIDialog::UnSetLockInputPCTalk();
			UIDialog::UnSetLockInputMessage();

			// 부활 버튼 없애기
			// 이거는 GCUpdateInfoHandler에서 해야하는데.. 일단. - -;
			DEBUG_ADD("FinishRequestDie");
			gC_vs_ui.FinishRequestDie();
			gC_vs_ui.FinishRequestResurrect();
			gC_vs_ui.CloseRequestShrineMinimap();


//Edit by sonic 2006.7.27  修改加速地址
			g_UpdateDelay = 100 ^ 90;
			
			DEBUG_ADD("SetUpdate");
g_pUpdate = g_pCGameUpdate;
		g_pCGameUpdate->Init();

		// Ensure input is enabled after start game
		CheckActivate(TRUE);

		ExecuteHelpEvent( HELP_EVENT_INTERFACE );

		break;

		
		//------------------------------------------------------
		// Program을 끝낸다.
		//------------------------------------------------------
		case MODE_QUIT :
			// Logout을 보낸다.
				if (g_pSocket!=NULL)
				{
					//--------------------------------------------------
					// update할려고 종료하는 경우는 CLLogout을 보낸다.
					//--------------------------------------------------
					if (g_bNeedUpdate)
					{
						//--------------------------------------------------
						// Login 서버로 CLLogout 패킷을 보낸다.
						//--------------------------------------------------
							CLLogout clLogout;
						
							g_pSocket->sendPacket( &clLogout );

					}
					//--------------------------------------------------
					// game중에 종료하는 경우.. CG Logout
					//--------------------------------------------------
					else
					{
						CGLogout cgLogout;
					
						g_pSocket->sendPacket( &cgLogout );

					}
				}

			//--------------------------------------------------
			// Thread Loading 종료..
			//--------------------------------------------------
			s_pGameMainHost->StopLoadingThread();

			// Update함수 설정
			g_pUpdate = NULL;

			g_bUIInput = FALSE;

			// window 닫기
			g_bActiveApp = FALSE;
#ifdef PLATFORM_WINDOWS
			PostMessage(g_hWnd, WM_CLOSE, 0, 0);
#else
			// On non-Windows platforms, set running flag to false to exit game loop
			extern bool g_bRunning;
			g_bRunning = false;
#endif // PLATFORM_WINDOWS
		break;

		default:
			break;
	}

	// 입력을 초기화한다.
	g_pSDLInput->Clear();
}

//-----------------------------------------------------------------------------
// Check Activate
//-----------------------------------------------------------------------------
// 프로그램이 실행중인지 아닌지에 따라서 처리..
// ALT + TAB 과 관련이 깊다. - -;
//-----------------------------------------------------------------------------
void
CheckActivate(BOOL bActiveGame)
{

	if (g_pSDLInput!=NULL)
	{
		// acquire
		g_pSDLInput->SetAcquire(bActiveGame==TRUE);			
		
		// 입력을 초기화한다.
		g_pSDLInput->Clear();
	}
	
	//----------------------------------------------------
	// 프로그램 진행..
	//----------------------------------------------------
	g_bActiveGame = FALSE;

	if (g_bActiveApp)
	{
		CSDLGraphics::RestoreGammaRamp();

		if (bActiveGame)
		{
			CSDLGraphics::RestoreAllSurfaces();

			DEBUG_ADD("WM_ACTIVATEAPP : Restore Surfaces");


			//----------------------------------------------------
			// global setting
			//----------------------------------------------------
			g_bActiveGame = TRUE;

			DEBUG_ADD("IsHAL : Before graphics release");
			// CDirect3D::Release() removed (SDL2)

			DEBUG_ADD("IsHAL : Before graphics init");
			// CDirect3D::Init() removed (SDL2) - SDL2 always uses hardware acceleration

			DEBUG_ADD("IsHAL : Before graphics restore");
			// CDirect3D::Restore() removed (SDL2)

			if (g_bFullScreen)
			{
				DEBUG_ADD("FullScreen : Before DD::SetDisplayMode()");
				if(g_MyFull)
				{
					CSDLGraphics::SetDisplayMode(1024, 768, 16, 0, 0);
				}
				else
				{
					CSDLGraphics::SetDisplayMode(800, 600, 16, 0, 0);
				}
			}

			DEBUG_ADD("Before Restore All Surfaces");

			CSDLGraphics::RestoreAllSurfaces();

			DEBUG_ADD("Before Restore");

			DEBUG_ADD("if g_pTopView");

			if (g_pTopView!=NULL && g_pTopView->IsInit())
			{
				DEBUG_ADD("Before g_pTopView->RestoreSurface");

				g_pTopView->RestoreSurface();

				DEBUG_ADD("After g_pTopView->RestoreSurface");

				g_pTopView->SetFirstDraw();
			}

			//--------------------------------------------------------
			// 감마값 재 설정
			//--------------------------------------------------------
			if (g_pUserOption!=NULL
				&& g_pClientConfig!=NULL)				
			{
				if (g_pUserOption->UseGammaControl
					&& g_pUserOption->GammaValue!=100)
				{
					CSDLGraphics::SetGammaRamp( g_pUserOption->GammaValue );
				}
				
				//------------------------------------
				// 연주중이면.. 중단..
				//------------------------------------
				// Music playback - use SDL_mixer (cross-platform)
				if (g_pUserOption->PlayMusic)
				{
					if (g_Music.IsPlay())
					{
						g_Music.Resume();
					}
					else if (g_Mode==MODE_GAME)
					{													
						PlayMusicCurrentZone();
					}
					else
					{
						int musicID = g_pClientConfig->MUSIC_THEME;

						if (musicID!=MUSICID_NULL
							&& g_pMusicTable!=NULL)
						{
							g_Music.Play( (*g_pMusicTable)[ musicID ].Filename );
						}
					}
				}
				//------------------------------------
				// 연주중이 아닌 경우
				//------------------------------------
				else
				{
					// nothing - -;
				}
			}

			if (g_Mode!=MODE_WAIT_UPDATEINFO)
			{
				g_SDLAudio.UnSetMute();
			}

			//--------------------------------
			// mouse 버튼 눌린 상태 제거
			//--------------------------------
			g_bLButtonDown = FALSE;
			g_bRButtonDown = FALSE;
			g_bCButtonDown = FALSE;

			if (g_pPlayer!=NULL)
			{
				g_pPlayer->UnSetLockMode();
			}

			
			// UI에서 alt+tab처리
			DEBUG_ADD("UI_RestoreWhenActivateGame");

			gC_vs_ui.RestoreWhenActivateGame();

			DEBUG_ADD("UI_Restore_ok");
		}
		//----------------------------------------------------
		// 멈춤..
		//----------------------------------------------------
		else
		{
			// SDL handles display mode restoration automatically
			if (g_bFullScreen)
			{
				CSDLGraphics::RestoreDisplayMode();
			}


			// Music playback - use SDL_mixer (cross-platform)
			g_Music.Stop();
			
			if (g_Mode!=MODE_WAIT_UPDATEINFO)
			{
				g_SDLAudio.SetMute();			
			}
			
			// 반복 동작 중지
			if (g_pPlayer!=NULL)
			{
				g_pPlayer->UnSetRepeatAction();
			}
		}
	}

}

//-----------------------------------------------------------------------------
// Load Creature
//-----------------------------------------------------------------------------
// n번째 Creature Type ID의 Sprite를 load한다.
//-----------------------------------------------------------------------------
BOOL
LoadCreature(int spriteType)
{
	// Debug Message
	DEBUG_ADD_FORMAT("LoadCreature : %d", spriteType);
	
	g_pTopView->LoadFromFileCreatureSPK( spriteType );

	return TRUE;
}

//-----------------------------------------------------------------------------
// ReleaseUselessCreatureSPKExcept
//-----------------------------------------------------------------------------
void		
ReleaseUselessCreatureSPKExcept(const COrderedList<int>& listUse)
{
	DEBUG_ADD("ReleaseUselessCreatureSPKExcept");
	
	g_pTopView->ReleaseUselessCreatureSPKExcept( listUse );
}

//-----------------------------------------------------------------------------
// Release GameObject
//-----------------------------------------------------------------------------
// game에 관련된 object들을 제거시킨다.
//-----------------------------------------------------------------------------
void
ReleaseGameObject()
{
	//------------------------------------------------------
	// 날씨 제거
	//------------------------------------------------------
	g_pWeather->Release();

	g_pParty->Release();

	g_pEventManager->RemoveAllEvent();
	g_pWarManager->ClearWar();
	g_pTimeItemManager->clear();

	//------------------------------------------------------
	// Zone에서 Player제거 / Zone 제거
	//------------------------------------------------------
	if (g_pZone!=NULL)
	{
		DEBUG_ADD("[ SetZone NULL ] Remove Player & Set Zone to NULL");
		g_pZone->RemovePlayer();				
		g_pZone->ReleaseObject();
		g_pZone = NULL;

		if (g_pZoneSoundTable)
		{
			delete g_pZoneSoundTable;
			g_pZoneSoundTable = NULL;
		}

		if (g_pZoneSoundManager)
		{
			delete g_pZoneSoundManager;
			g_pZoneSoundManager = NULL;
		}
	}

	//------------------------------------------------------
	// player제거
	//------------------------------------------------------
	if (g_pPlayer!=NULL)
	{
		DEBUG_ADD("[ delete Player ]");
		
		delete g_pPlayer;
		g_pPlayer = NULL;
	}

	if (g_pTradeManager!=NULL)
	{
		delete g_pTradeManager;
		g_pTradeManager = NULL;
	}

	if (g_pStorage!=NULL)
	{
		delete g_pStorage;
		g_pStorage = NULL;
	}
	
	if (g_pStorage2!=NULL)
	{
		delete g_pStorage2;
		g_pStorage2 = NULL;
	}
			

	DEBUG_ADD("[ delete GameObject OK ]");
}

//-----------------------------------------------------------------------------
// Get Near Sprite Sets
//-----------------------------------------------------------------------------
// The tile and image-object sprites near the player, from the zone (whose
// sets are std::sets since task 4.246) into the view's set managers.
//-----------------------------------------------------------------------------
static void
GetNearSpriteSets(CSpriteSetManager& TileSSM, CSpriteSetManager& ImageObjectSSM)
{
	std::set<TYPE_SPRITEID> tileIDs, imageObjectIDs;
	g_pZone->GetNearSpriteSet(tileIDs, imageObjectIDs, g_pPlayer->GetX(), g_pPlayer->GetY());
	for (TYPE_SPRITEID id : tileIDs) TileSSM.Add(id);
	for (TYPE_SPRITEID id : imageObjectIDs) ImageObjectSSM.Add(id);
}

//-----------------------------------------------------------------------------
// Load Zone
//-----------------------------------------------------------------------------
// n번째 zone을 load한다.
//-----------------------------------------------------------------------------
BOOL
LoadZone(int n)
{	
	//------------------------------------------------
	// Zone의 종류(size)에 따른 처	리
	//------------------------------------------------
	// 
	// 같은 종류(size)에 따른 Zone에 Load를 해야한다.
	//
	//------------------------------------------------
	ZONETABLE_INFO* pZoneInfo = g_pZoneTable->Get( n );

	if (pZoneInfo==NULL)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] Wrong Zone ID=%d", n);
		
		return FALSE;
	}

	//------------------------------------------------
	// 모든 사운드를 정지해버린다.
	//------------------------------------------------
	if (g_pSoundManager!=NULL)
	{
		g_pSoundManager->Stop();		
		g_ZoneAmbientSounds.OnSoundsStopped();
	}
	g_SDLAudio.ReleaseDuplicateBuffer();


	// Debug Message
	DEBUG_ADD_FORMAT("LoadZone : ID=%d, Filename=%s", pZoneInfo->ID, pZoneInfo->Filename.GetString());		
	
	//------------------------------------------------
	// Music 정지...
	//------------------------------------------------
	if (g_pUserOption->PlayWaveMusic)
	{
		if( g_pOGG != NULL )
		{
			if( g_SDLAudio.IsInit() )
				g_pOGG->streamClose();
		}
	}
	else
	{
		g_Music.Stop();
	}

	//-------------------------------------------------------------
	// Loading 중이던것 모두 제거
	//-------------------------------------------------------------
	s_pGameMainHost->StopLoadingThread();

	// priority를 최대한 낮춘다.
	// Thread priority is platform-specific - currently implemented for Windows only
#ifdef PLATFORM_WINDOWS
	s_pGameMainHost->SetLoadingThreadPriority( THREAD_PRIORITY_IDLE );
#endif // PLATFORM_WINDOWS

	//----------------------------------------------------------------------
	// 현재 Zone의 object들 제거
	//----------------------------------------------------------------------
	if (g_pZone!=NULL)
	{
		g_pZone->ReleaseObject();
	}

	//----------------------------------------------------------------------
	//
	//					Small Zone
	//
	//----------------------------------------------------------------------
	if (pZoneInfo->Property & FLAG_ZONESIZE_SMALL)
	{	
		//-------------------------------------------
		// 이전에 LargeZone에 있었다면.. 
		// 그림자 다시 생성
		//-------------------------------------------
		if (g_bZonePlayerInLarge)
		{
			g_pTopView->ClearShadowManager();
		}
		
		//-------------------------------------------
		// 다시 load할 필요가 없는 경우
		//-------------------------------------------
		if (g_nZoneSmall==n && g_pZoneSmall!=NULL)
		{
			DEBUG_ADD("ReleaseObject in SmallZone");
			
			// object들 제거
			g_pZoneSmall->ReleaseObject();

			DEBUG_ADD("OK");
			

			// SmallZone의 pointer를 대입
			g_pZone	= g_pZoneSmall;

			UI_DrawProgress( 95 );
		}
		//-------------------------------------------
		// 다시 Load해야될 경우
		//-------------------------------------------
		else
		{
			// Load할 Zone의 종류(size)를 기억
			g_nZoneSmall = n;	

			DEBUG_ADD("[Load Zone] Release Old Tile&ImageObject SPK");
			
			// 이전 Zone에 있던 Sprite들을 지워준다.
			g_pTopView->ReleaseTileSPKSmallZone();		
			g_pTopView->ReleaseImageObjectSPKSmallZone();	
			
			// SmallZone을 초기화한다.
			if (g_pZoneSmall != NULL)
			{
				DEBUG_ADD( "Delete old ZoneSmall");
				
				delete g_pZoneSmall;
			}				

			DEBUG_ADD( "New ZoneSmall");
			
			g_pZoneSmall = new MZone;

			// SmallZone의 pointer를 대입
			g_pZone	= g_pZoneSmall;

			// 틴버전인경우 틴버전용 맵파일 로딩
			MString filename = pZoneInfo->Filename;
			if(g_pUserInformation->GoreLevel == false)
			{
				if(pZoneInfo->TeenFilename.GetLength() > 0)
					filename = pZoneInfo->TeenFilename;
			}

			std::ifstream file;
			if (!FileOpenBinary(filename, file))
			{
				// priority를 정상으로
				s_pGameMainHost->SetLoadingThreadPriority( THREAD_PRIORITY_LOWEST );

				return FALSE; 
			}

			DEBUG_ADD("[Load Zone] Load Zone Data");
			
			UI_DrawProgress( 80 );

			// Zone loading 실패?
			if (!g_pZone->LoadFromFile( file ))
			{
				SetMode( MODE_QUIT );

				// priority를 정상으로
				s_pGameMainHost->SetLoadingThreadPriority( THREAD_PRIORITY_LOWEST );
				return FALSE;
			}

			g_pZone->SetCurrentZoneID( n );
		
			file.close();			
			
			
			
			UI_DrawProgress( 90 );

			//-------------------------------------------------------
			// Player의 좌표 근처의 Sprite들을 Load한다.
			//-------------------------------------------------------
			DEBUG_ADD("[Load Zone] Find Tile&ImageObject ID");
			
			CSpriteSetManager TileSSM;
			CSpriteSetManager ImageObjectSSM;
			GetNearSpriteSets(TileSSM, ImageObjectSSM);
			if (!g_pTopView->LoadFromFileTileAndImageObjectSet(TileSSM, ImageObjectSSM)) {
				LOG_ERROR("Rejected zone sprite preload: zone=%d", n);
				SetMode(MODE_QUIT);
				s_pGameMainHost->SetLoadingThreadPriority(THREAD_PRIORITY_LOWEST);
				return FALSE;
			}

			DEBUG_ADD("[Load Zone] Find Tile&ImageObject ID.. OK");
			
			UI_DrawProgress( 95 );
		}
		
		// 틴버전인경우 틴버전용 맵파일 로딩
		MString filename = (*g_pZoneTable).Get(g_nZoneSmall)->Filename;
		if(g_pUserInformation->GoreLevel == false)
		{
			if((*g_pZoneTable).Get(g_nZoneSmall)->TeenFilename.GetLength() > 0)
				filename = (*g_pZoneTable).Get(g_nZoneSmall)->TeenFilename;
		}

		std::ifstream file;
		// Zone.inf spells it Data\Map\<zone>.map: resolved for the disk as
		// FileOpenBinary() resolves the zone load (basic/DataPath.h).
		file.open(Basic::NormalizeDataPath(filename.GetString()).c_str(), ios::binary);

		file.seekg(g_pZone->GetTileFilePosition(), ios::beg);
		g_pTopView->LoadFromFileTileSPKSmallZone( file );

		file.seekg(g_pZone->GetImageObjectFilePosition(), ios::beg);				
		g_pTopView->LoadFromFileImageObjectSPKSmallZone( file );

		file.close();		
		
		DEBUG_ADD("[Load Zone] Before Music Check");
		
		DEBUG_ADD("[Load Zone] Before Music Play");
		
		g_bZonePlayerInLarge = false;
	}

	//----------------------------------------------------------------------
	//
	//					Large Zone
	//
	//----------------------------------------------------------------------
	else
	{
		//-------------------------------------------
		// 이전에 SmallZone에 있었다면.. 
		// 그림자 다시 생성
		//-------------------------------------------
		if (!g_bZonePlayerInLarge)
		{
			g_pTopView->ClearShadowManager();
		}

		//-----------------------------------------------
		// 다시 Load해야할 필요가 없는 경우
		//-----------------------------------------------
		if (g_nZoneLarge==n && g_pZoneLarge!=NULL)
		{
			DEBUG_ADD("ReleaseObject in LargeZone");
			
			// object들 제거
			g_pZoneLarge->ReleaseObject();
			
			DEBUG_ADD("OK");
			

			// LargeZone의 pointer를 대입
			g_pZone	= g_pZoneLarge;

			UI_DrawProgress( 95 );
		}
		//-----------------------------------------------
		// 다시 Load해야될 경우
		//-----------------------------------------------
		else
		{
			// Load할 Zone의 종류(size)를 기억
			g_nZoneLarge = n;	

			DEBUG_ADD("[Load Zone] Release Old Tile&ImageObject SPK");
			
			// 이전 Zone에 있던 Sprite들을 지워준다.
			g_pTopView->ReleaseTileSPKLargeZone();		
			g_pTopView->ReleaseImageObjectSPKLargeZone();	
			
			// LargeZone을 초기화한다.
			if (g_pZoneLarge != NULL)
			{
				DEBUG_ADD( "Delete old ZoneLarge");
				
				delete g_pZoneLarge;
			}			
			
			DEBUG_ADD( "New ZoneLarge");
			
			g_pZoneLarge = new MZone;

			// LargeZone의 pointer를 대입
			g_pZone	= g_pZoneLarge;			
			
			// 틴버전인경우 틴버전용 맵파일 로딩
			MString filename = pZoneInfo->Filename;
			if(g_pUserInformation->GoreLevel == false)
			{
				if(pZoneInfo->TeenFilename.GetLength() > 0)
					filename = pZoneInfo->TeenFilename;
			}
			
			std::ifstream file;
			if (!FileOpenBinary(filename, file))
			{
				// priority를 정상으로
				s_pGameMainHost->SetLoadingThreadPriority( THREAD_PRIORITY_LOWEST );

				return FALSE; 
			}

			DEBUG_ADD("[Load Zone] Load Zone Data");
			
			UI_DrawProgress( 80 );

			// Zone loading 실패?
			if (!g_pZone->LoadFromFile( file ))
			{
				SetMode( MODE_QUIT );

				// priority를 정상으로
				s_pGameMainHost->SetLoadingThreadPriority( THREAD_PRIORITY_LOWEST );

				return FALSE;
			}

			file.close();

			UI_DrawProgress( 90 );
			
			//-------------------------------------------------------
			// Player의 좌표 근처의 Sprite들을 Load한다.
			//-------------------------------------------------------
			DEBUG_ADD("[Load Zone] Find Tile&ImageObject ID");
			
			CSpriteSetManager TileSSM;
			CSpriteSetManager ImageObjectSSM;
			GetNearSpriteSets(TileSSM, ImageObjectSSM);
			if (!g_pTopView->LoadFromFileTileAndImageObjectSet(TileSSM, ImageObjectSSM)) {
				LOG_ERROR("Rejected zone sprite preload: zone=%d", n);
				SetMode(MODE_QUIT);
				s_pGameMainHost->SetLoadingThreadPriority(THREAD_PRIORITY_LOWEST);
				return FALSE;
			}
			
			DEBUG_ADD("[Load Zone] Find Tile&ImageObject ID.. OK");
				
			UI_DrawProgress( 95 );
		}

		MString filename = (*g_pZoneTable).Get(g_nZoneLarge)->Filename;
		if(g_pUserInformation->GoreLevel == false)
		{
			if((*g_pZoneTable).Get(g_nZoneLarge)->TeenFilename.GetLength() > 0)
				filename = (*g_pZoneTable).Get(g_nZoneLarge)->TeenFilename;
		}

		std::ifstream file;
		// Zone.inf spells it Data\Map\<zone>.map: resolved for the disk as
		// FileOpenBinary() resolves the zone load (basic/DataPath.h).
		file.open(Basic::NormalizeDataPath(filename.GetString()).c_str(), ios::binary);

		file.seekg(g_pZone->GetTileFilePosition(), ios::beg);
		g_pTopView->LoadFromFileTileSPKLargeZone( file );

		file.seekg(g_pZone->GetImageObjectFilePosition(), ios::beg);				
		g_pTopView->LoadFromFileImageObjectSPKLargeZone( file );

		file.close();		

		
		
		DEBUG_ADD("[Load Zone] Before Music Play");
		
		g_bZonePlayerInLarge = true;
	}

	DEBUG_ADD("[Load Zone] Before Minimap Loading");

	//------------------------------------------------
	// UI에 zone설정..
	//------------------------------------------------
	SIZE zoneSize = { g_pZone->GetWidth(), g_pZone->GetHeight() };

	gC_vs_ui.SetSize( zoneSize );
	gC_vs_ui.SetZone( n );


	LoadZoneInfo( n );
	g_pZone->SetCurrentZoneID( n );

	// 2004, 6, 22 sobeit add start - 질드레 코어 관련
	Add_GDR_Ghost(n);
	Add_GDR_Effect(1,false);
	Add_GDR_Effect(2,false);
	Add_GDR_Potal_Effect(n);
	// 2005, 1, 18, sobeit add start-퀘스트 땜에 - bDisableTileImage==false가 대부분이므로..
	if(n == 4001)
	{
		for (int i=0; i<g_pZone->GetHeight(); i++)
		{
			for (int j=0; j<g_pZone->GetWidth(); j++)
			{
				const_cast<MSector &>(g_pZone->GetSector(i, j)).SetDisableTileImage();
			}
		}
	}
	// 2005, 1, 18, sobeit add end
	// 2004, 9, 21, sobeit add start - 아담 북 구름
	else if(n == 74)
	{
		if(g_pPlayer->GetY()<60)
			g_pPlayer->SetShowAdamCloud(true);
		else
			g_pPlayer->SetShowAdamCloud(false);
	}
	// 2004, 9, 21, sobeit add end
	// 2004, 6, 22 sobeit add end
	//------------------------------------------------
	// Random SoundID 시간 설정
	//------------------------------------------------
	g_ZoneAmbientSounds.DelayAfterZoneLoad(g_FrameNow, static_cast<unsigned>(rand()));
	
	//------------------------------------------------
	// View에게 출력해야할 Zone을 지정한다.
	//------------------------------------------------
	// 이전에 출력된 creature들을 없앤다.
	g_pTopView->SetZone(g_pZone);	

	// Debug Message
	DEBUG_ADD_FORMAT("LoadZone OK : size=(%d, %d)", g_pZone->GetWidth(), g_pZone->GetHeight());			
		

	// priority를 정상으로
	s_pGameMainHost->SetLoadingThreadPriority( THREAD_PRIORITY_LOWEST );

	if (pZoneInfo->CreatureColorSet < MAX_COLORSET)
	{
		g_ZoneCreatureColorSet = pZoneInfo->CreatureColorSet;
	}
	else
	{
		g_ZoneCreatureColorSet = 0xFFFF;
	}	

	g_bZoneSafe = pZoneInfo->Safety;
	g_bHolyLand = pZoneInfo->HolyLand;

	g_pWarManager->ClearWar();
	g_pTimeItemManager->clear();

	g_pUserInformation->WorldName = g_pServerInformation->GetServerGroupName();

	if(gpC_base != NULL && g_pUserInformation->bCompetence == true && g_pUserInformation->bCompetenceShape != true &&
		g_pZoneTable->Get( g_pZone->GetID() )->CompetenceZone == false
		)
	{
		gpC_base->SendTextMessage(UI_CHAT_RETURN, CLD_NORMAL, 0, "*command ghost on");
	}
	
	return TRUE;
}

//-----------------------------------------------------------------------------
// Move Zone 
//-----------------------------------------------------------------------------
// Zone을 이동한다.
//-----------------------------------------------------------------------------
void
MoveZone(int n)
{
	//------------------------------------------------
	// Zone에서 Player를 빼낸다.
	//------------------------------------------------
	if (g_pZone != NULL)
	{
		g_pPlayer->SetStop();
		g_pPlayer->SetAction( ACTION_STAND );
		g_pZone->RemovePlayer();
	}
	
	// n Zone을 load한다.
	LoadZone( n );

	g_pPlayer->SetZone(g_pZone);
	g_pZone->SetPlayer();
	g_pPlayer->SetTimeLightSight(g_pPlayer->GetTimeLightSight());
}

//-----------------------------------------------------------------------------
// LoadZoneInfo
//-----------------------------------------------------------------------------
BOOL
LoadZoneInfo(int n)
{
	//------------------------------------------------
	// ZoneInfo
	//------------------------------------------------
	ZONETABLE_INFO* pZoneInfo = g_pZoneTable->Get( n );

	if (pZoneInfo==NULL)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] Wrong Zone ID=%d", n);
		
		return FALSE;
	}

	//------------------------------------------------
	//
	// ZoneInfoFile Loading
	//
	//------------------------------------------------
	DEBUG_ADD("[Load Zone] Before Open ZoneInfoFile");
	
	//------------------------------------------------
	// ZoneInfo
	//------------------------------------------------
	std::ifstream zoneInfoFile(Basic::NormalizeDataPath(pZoneInfo->InfoFilename.GetString()), ios::binary);

	//------------------------------------------------
	// File이 있는 경우만 loading한다.
	//------------------------------------------------
	if (zoneInfoFile.is_open())
	{
		DEBUG_ADD("[Load Zone] Load MinimapInfo");
		
		ZoneInfoData zoneInfo;

		//------------------------------------------------
		// Parse the complete file before publishing anything to the zone or UI.
		//------------------------------------------------
		if (zoneInfo.LoadFromFile(zoneInfoFile, g_pZone->GetWidth(), g_pZone->GetHeight()))
		{		
			DEBUG_ADD("[Load Zone] Load PortalInfo");
			
			//------------------------------------------------
			// 포탈 정보를 loading한다.
			// 내부에서 해주는게 좋지만... 일단..
			//------------------------------------------------
			const int numPortal = static_cast<int>(zoneInfo.portals.size());

			DEBUG_ADD_FORMAT("numPortal = %d", numPortal);

			for (int i=0; i<numPortal; i++)
			{
				const MPortal& portal = zoneInfo.portals[i];
				const RECT rect = {portal.GetLeft(), portal.GetTop(), portal.GetRight(), portal.GetBottom()};

				const std::vector<WORD>& zoneID = portal.GetZoneID();

				int numZoneID = static_cast<int>(zoneID.size());

				DEBUG_ADD_FORMAT("[%d] numZoneID = %d", i, numZoneID);

				for (int i=0; i<numZoneID; i++)
				{
					DEBUG_ADD_FORMAT("AddPortal. type=%d, zoneID=%d", portal.GetType(), zoneID[i]);
					
					g_pZone->AddPortal( portal.GetType(), zoneID[i], rect );
				}

				//------------------------------------------------
				// UI에 설정한다.
				//------------------------------------------------
				if (numZoneID==1)
				{					
					DEBUG_ADD("UI SetPortal");

					gC_vs_ui.SetPortal( rect, zoneID[0] );
				}
				else
				{
					DEBUG_ADD("UI SetPortal - 60001");

					// 멀티 포탈인 경우.. ㅋㅋ.. _-_;
					gC_vs_ui.SetPortal( rect, 60001 );
				}
			}

			DEBUG_ADD("PortalInfo OK");

			//------------------------------------------------
			// 안전지대 정보
			//------------------------------------------------
			const int numSafe = static_cast<int>(zoneInfo.safetyZones.size());

			DEBUG_ADD_FORMAT("numSafe = %d", numSafe);

			DEBUG_ADD_FORMAT("[g_pPlayer] %p", static_cast<const void*>(g_pPlayer));
			Race MyRace = g_pPlayer->GetRace();
			DEBUG_ADD("bSlayer OK");

			for (int i=0; i<numSafe; i++)
			{			
				const B_RECT& rect2 = zoneInfo.safetyZones[i];

				RECT safeRect = 
				{ 
					rect2.left,
					rect2.top,
					rect2.right,
					rect2.bottom
				};
				
				DEBUG_ADD_FORMAT("UI SetSafety. f=%d, (%d, %d, %d, %d)", rect2.flag, safeRect.left, safeRect.top, safeRect.right, safeRect.bottom);

				if (rect2.flag != 0)
				{
					BYTE fSafe = 0;

					if (rect2.flag & FLAG_MIP_SAFE_COMMON)
					{
						fSafe = FLAG_SECTOR_SAFE_COMMON;
					}
					
					if (rect2.flag & FLAG_MIP_SAFE_SLAYER)
					{
						fSafe |= FLAG_SECTOR_SAFE_SLAYER;
					}

					if (rect2.flag & FLAG_MIP_SAFE_VAMPIRE)
					{
						fSafe |= FLAG_SECTOR_SAFE_VAMPIRE;
					}
					if (rect2.flag & FLAG_MIP_SAFE_OUSTERS)
					{
						fSafe |= FLAG_SECTOR_SAFE_OUSTERS;
					}
					
					g_pZone->SetSafeSector( safeRect, fSafe );

					// 내가 안전한 위치인가?

					if((MyRace == RACE_OUSTERS && ( rect2.flag & FLAG_MIP_SAFE_OUSTERS )) ||
						(MyRace == RACE_VAMPIRE && ( rect2.flag & FLAG_MIP_SAFE_VAMPIRE)) ||
						(MyRace == RACE_SLAYER && (rect2.flag & FLAG_MIP_SAFE_SLAYER)) ||
						(rect2.flag & FLAG_MIP_SAFE_COMMON) )
					{
						gC_vs_ui.SetSafetyZone( safeRect, true );						
					}
					// 다른 종족이 안전한 위치인가?
					else
					{
						gC_vs_ui.SetSafetyZone( safeRect, false );
					}
				}
			}
			DEBUG_ADD("NUMSAFE OK");

			DEBUG_ADD("zoneinfoclose");
			zoneInfoFile.close();
			DEBUG_ADD("zoneinfoclose OK");
			
			DEBUG_ADD("Horn Setting Start");
			// ousters horn을 맵에 심는다
			UI_PORTAL_LIST portalList;

			for(int i = 0; static_cast<size_t>(i) < g_pZone->GetHorn().size(); i++)
			{
				portalList = g_pZone->GetHorn()[i];
				
				UI_PORTAL_LIST::iterator itr = portalList.begin();
				UI_PORTAL_LIST::iterator endItr = portalList.end();
				
				while(itr != endItr)
				{
					if(itr->zone_id == g_pZone->GetID())
					{
						DEBUG_ADD_FORMAT("zoneID : %d, x : %d, y : %d, portalX : %d, portalY : %d", 
							itr->zone_id, itr->x, itr->y, itr->portal_x, itr->portal_y);

						int npcID = 659;
						
						MNPC* pNPC = new MNPC;
						
						pNPC->SetZone( g_pZone );
						
						pNPC->SetGuildNumber( GUILDID_OUSTERS );
						
						pNPC->SetCreatureType( npcID );
						pNPC->SetGroundCreature();
						pNPC->SetID(MFakeCreature::GetFakeID());
						// Edit By Sonic 2006.10.1  修正魔灵配柱子位置
						//原位置 62  x=39 y=30  修正位置 x=7 y=11
						if(itr->zone_id==62)
						{
							pNPC->SetPosition( 7, 11 );
						}else
						{
							pNPC->SetPosition( itr->portal_x, itr->portal_y );
						}
						// End
						pNPC->SetServerPosition( itr->portal_x, itr->portal_y );
						pNPC->SetDirection( 1 );
						pNPC->SetCurrentDirection( 1 );
						pNPC->SetAction( ACTION_STAND );
						
						pNPC->SetName( (*g_pCreatureTable)[npcID].Name.GetString() );
						
						// NPC ID
						pNPC->SetNPCID( npcID );
						
						pNPC->SetStatus( MODIFY_MAX_HP, 100 );
						pNPC->SetStatus( MODIFY_CURRENT_HP, 100 );
						
						if (!g_pZone->AddCreature( pNPC ))
						{
							delete pNPC;
							pNPC = NULL;
						}
						else if(g_pPlayer->IsOusters())
						{
							const char* pName = pNPC->GetName();
							int x = pNPC->GetX();;
							int y = pNPC->GetY();
							
							UI_SetNPCInfo( pName, npcID, x, y );
						}
						DEBUG_ADD("AddNPC OK");
					}
					itr++;
				}
			}
			DEBUG_ADD("Horn Setting End");

			if(g_pZone->GetID() == 1311)
			{
				SetWeather(WEATHER_SPOT, 2);
			}
		}
		else
		{			
			DEBUG_ADD_FORMAT_ERR("[Error] Invalid ZoneInfo for zone ID=%d", n);
			return FALSE;
		}

		
		
		DEBUG_ADD("[Load Zone] Load ZoneInfo OK");
	}
	else
	{		
		DEBUG_ADD_ERR("[Load Zone] ZoneInfo File OpenError");
	}

	DEBUG_ADD("[Load Zone] OK");

	return TRUE;
}

//-----------------------------------------------------------------------------
// Init Player
//-----------------------------------------------------------------------------
BOOL
InitPlayer(int x, int y, int dir)
{
	// Debug Message
	DEBUG_ADD_FORMAT("InitPlayer : (%d,%d) Dir=%d", x, y, dir);			
	
	g_pPlayer->ClearChatString();

	//------------------------------------------------
	// 살아있게 한다. 확인용...
	//------------------------------------------------
	g_pTopView->SetFadeEnd();

	//------------------------------------------------
	// 위치 설정
	//------------------------------------------------
	g_pPlayer->SetStop();
	g_pPlayer->SetX( x );
	g_pPlayer->SetY( y );
	g_pPlayer->SetServerPosition(x, y);
	g_pPlayer->SetDirection(dir);
	g_pPlayer->SetCurrentDirection(dir);

	//------------------------------------------------
	//  Zone <--> Player
	//------------------------------------------------
	g_pZone->SetPlayer();
	g_pPlayer->SetZone(g_pZone);

	// 정지 동작
	if (g_pPlayer->IsAlive())
	{
		g_pPlayer->SetAction( ACTION_STAND );
	}
	else
	{
		g_pPlayer->SetAction( ACTION_DIE );
	}

	//-------------------------------------------------------
	// Server로 보낸 move packet 개수를 지우고
	// 처음부터~~ 다시 move하게 한다.
	//-------------------------------------------------------
	g_pPlayer->ResetSendMove();

	//-------------------------------------------------------
	// 기술 검증 상태..
	//-------------------------------------------------------
	g_pPlayer->SetWaitVerifyNULL();

	//-------------------------------------------------------
	//
	//						Item 관련
	//
	//-------------------------------------------------------
	//------------------------------------------------
	// ItemCheckBuffer상태
	//------------------------------------------------
	g_pPlayer->ClearItemCheckBuffer();
	
	//------------------------------------------------
	// 정보창에 보여주는 정보..
	//------------------------------------------------
	g_char_slot_ingame.Race = g_pPlayer->GetRace();
	g_char_slot_ingame.bl_female = !g_pPlayer->IsMale();

	return TRUE;
}

//-----------------------------------------------------------------------------
// Make ScreenShot
//-----------------------------------------------------------------------------
// 현재 화면의 ScreenShot을 저장한다.
//-----------------------------------------------------------------------------
void	
MakeScreenShot()
{
	char str[256];

	int maxScreenShot = g_pClientConfig->MAX_SCREENSHOT;


#ifdef PLATFORM_WINDOWS
	_mkdir("ScreenShot");
#else
	mkdir("ScreenShot", 0755);
#endif // PLATFORM_WINDOWS
	// MAX_SCREENSHOT개의 ScreenCapture만 가능하다.
	for (; g_ScreenShotNumber<maxScreenShot; g_ScreenShotNumber++)
	{
		snprintf(str, sizeof(str), "%s%03d.jpg", g_pFileDef->getProperty("PATH_SCREENSHOT").c_str(), g_ScreenShotNumber);

#ifdef PLATFORM_WINDOWS
		int fd = -1;
		_sopen_s( &fd, str, _O_RDONLY, _SH_DENYNO, 0 );
#else
		int fd = open( str, O_RDONLY );
#endif // PLATFORM_WINDOWS

		// file이 없는 경우에 save하기 위해서..
		if( fd == -1 )
		{
			// Save

			// SaveSurfaceToImage() (UtilityFunction.cpp, retired in task
			// 4.231) took a CDirectDrawSurface&, but with
			// SPRITELIB_BACKEND_SDL (the only backend this project builds
			// now, Windows included) CSpriteSurface is a standalone class
			// that no longer inherits from CDirectDrawSurface. g_pBack is a
			// CSpriteSurface*, so this call never type-checked under that
			// backend.
			// TODO: Implement screenshot saving against CSpriteSurface (SDL
			// surface -> BMP/JPEG) or via stb_image_write; see 참고자료/작업필요stub.md.
			printf("Screenshot functionality not yet implemented on this platform\n");

			// The open failed, so there is no descriptor to close. On Windows,
			// _close(-1) invokes the CRT invalid-parameter handler and aborts.
			g_ScreenShotNumber++;
			return;
		}
		else
		{
#ifdef PLATFORM_WINDOWS
			_close(fd);
#else
			close(fd);
#endif // PLATFORM_WINDOWS
		}
	}


}

//-----------------------------------------------------------------------------
// PlaySound
//-----------------------------------------------------------------------------
// (*g_pSoundTable)에서의 soundID와 관련되는 Filename을 Play하면 된다.
// (*g_pSoundManager)에 있으면 바로 play하면 되고.. 
// 없으면 Wav를 Load해서 play하면 된다.
//-----------------------------------------------------------------------------
void	
PlaySound(TYPE_SOUNDID soundID, bool repeat, int x, int y)
{
	if(g_pEventManager->GetEventByFlag(EVENTFLAG_NOT_PLAY_SOUND))
		return;
	
	__BEGIN_PROFILE("PlaySound1")


	//-----------------------------------------------------------
	// 정의되지 않는 sound ID일 경우..
	//-----------------------------------------------------------
	if (!g_SDLAudio.IsInit() || soundID >= (*g_pSoundTable).GetSize()
		|| !g_pUserOption->PlaySound)
	{
		__END_PROFILE("PlaySound1")
		return;
	}

	//-----------------------------------------------------------
	// 초당 소리낼 수 있는 개수 제한
	//-----------------------------------------------------------
	if (!repeat 
		&& g_SoundPerSecond > g_pClientConfig->MAX_SOUND_PER_SECOND)
	{
		__END_PROFILE("PlaySound1")
		return;
	}

	int gapX = x - g_pPlayer->GetX();
	int gapY = y - g_pPlayer->GetY();
			
	int dist = max(abs(gapX), abs(gapY));				


	// 거리가 어느 정도 이상이면 소리를 출력하지 않아야 한다.
	if (dist < 25)
	{			
		//-----------------------------------------------------------
		// 없으면 --> Load & Play
		//-----------------------------------------------------------
		if ((*g_pSoundManager).IsDataNULL(soundID))
		{
			const char* pFilename = (*g_pSoundTable)[soundID].Filename;

			if (pFilename==NULL)
			{
				__END_PROFILE("PlaySound1")
				return;
			}					

			// Sound.inf is data, not code: a filename longer than the buffer
			// is truncated so the load fails and gets logged, not a smashed stack.
			char strFilename[256];
			Basic::CopyBounded(strFilename, pFilename, sizeof(strFilename)-1);
			strFilename[sizeof(strFilename)-1] = '\0';

			LPDIRECTSOUNDBUFFER pBuffer = g_SDLAudio.LoadWav( strFilename );

			//-----------------------------------------------------------
			// Loading 실패
			//-----------------------------------------------------------
			if (pBuffer==NULL)
			{
				DEBUG_ADD_FORMAT_ERR("[Error] Failed to Load WAV. id=%d, fn=%s", soundID, strFilename );
			}
			else
			//-----------------------------------------------------------
			// Load에 성공 했으면...
			//-----------------------------------------------------------
			{
				LPDIRECTSOUNDBUFFER pOld;
				if ((*g_pSoundManager).SetData( soundID, pBuffer, pOld )!=0xFFFF)
				{
					g_SDLAudio.Release(pOld);
				}

				int gapX = x - g_pPlayer->GetX();
				int gapY = y - g_pPlayer->GetY();

				int dist = max(abs(gapX), abs(gapY));				
			
				if (gapX > 3)
				{
					g_SDLAudio.CenterToRightPan( pBuffer, (gapX-3) << 7 );
				}
				else if (gapX < -3)
				{
					g_SDLAudio.CenterToLeftPan( pBuffer, (abs(gapX+3)) << 7 );
				}
				else
				{
					g_SDLAudio.CenterPan( pBuffer );
				}			

				
				// 오토바이 소리 좀 줄이기 하드코딩. - -;
				if (soundID==SOUND_WORLD_BIKE_GO
					|| soundID==SOUND_WORLD_BIKE_STOP)
				{
					dist += 5;
				}

				
				int sub = (dist << 2) * g_pUserOption->VolumeSound;

				g_SDLAudio.SubVolumeFromMax(pBuffer, sub);			

				// Play
				g_SDLAudio.Play( pBuffer, repeat, g_bGoodFPS);
				g_SoundPerSecond++;

				// Force Feel
				//-------------------------------------------------
				{
				}
			}
		}
		//-----------------------------------------------------------
		// 있는 경우 --> Play
		//-----------------------------------------------------------
		else
		{
			LPDIRECTSOUNDBUFFER pBuffer;
			if ((*g_pSoundManager).GetData(soundID, pBuffer))
			{			
				
				if (gapX > 3)
				{
					g_SDLAudio.CenterToRightPan( pBuffer, (gapX-3) << 7 );
				}
				else if (gapX < -3)
				{
					g_SDLAudio.CenterToLeftPan( pBuffer, (abs(gapX+3)) << 7 );
				}
				else
				{
					g_SDLAudio.CenterPan( pBuffer );
				}

				// 오토바이 소리 좀 줄이기 하드코딩. - -;
				if (soundID==SOUND_WORLD_BIKE_GO
					|| soundID==SOUND_WORLD_BIKE_STOP)
				{
					dist += 5;
				}

				int sub = (dist << 2) * g_pUserOption->VolumeSound;
				
				g_SDLAudio.SubVolumeFromMax(pBuffer, sub);

				g_SDLAudio.Play( pBuffer, repeat, g_bGoodFPS );
				g_SoundPerSecond++;

			}
		}
	}

	__END_PROFILE("PlaySound1")

}

//-----------------------------------------------------------------------------
// PlaySound
//-----------------------------------------------------------------------------
// 소리나는 위치가 늘 player의 위치..
// CenterPan & MaxVolume
//-----------------------------------------------------------------------------
void	
PlaySoundAtPlayer(TYPE_SOUNDID soundID)
{
	if(g_pEventManager->GetEventByFlag(EVENTFLAG_NOT_PLAY_SOUND))
		return;

	__BEGIN_PROFILE("PlaySound2")

	//-----------------------------------------------------------
	// 정의되지 않는 sound ID일 경우..
	//-----------------------------------------------------------
	if (!g_SDLAudio.IsInit() || soundID >= (*g_pSoundTable).GetSize()
		|| !g_pUserOption->PlaySound)
	{
		__END_PROFILE("PlaySound2")
		return;
	}

	//-----------------------------------------------------------
	// 초당 소리낼 수 있는 개수 제한
	//-----------------------------------------------------------
	// player의 소리는 제한할 필요가 없다.

	//-----------------------------------------------------------
	// 없으면 --> Load & Play
	//-----------------------------------------------------------
	if ((*g_pSoundManager).IsDataNULL(soundID))
	{
		const char* pFilename = (*g_pSoundTable)[soundID].Filename;

		if (pFilename==NULL)
		{
			__END_PROFILE("PlaySound2")
			return;
		}

		// Sound.inf is data, not code: a filename longer than the buffer
		// is truncated so the load fails and gets logged, not a smashed stack.
		char strFilename[256];
		Basic::CopyBounded(strFilename, pFilename, sizeof(strFilename)-1);
		strFilename[sizeof(strFilename)-1] = '\0';
 		LPDIRECTSOUNDBUFFER pBuffer = g_SDLAudio.LoadWav( strFilename );

		//-----------------------------------------------------------
		// Loading 실패
		//-----------------------------------------------------------
		if (pBuffer==NULL)
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Failed to Load WAV. id=%d, fn=%s", soundID, strFilename );
		}
		//-----------------------------------------------------------
		// Load에 성공 했으면...
		//-----------------------------------------------------------
		else
		{
			// Replace됐으면 원래것을 메모리에서 지운다.
			LPDIRECTSOUNDBUFFER pOld;
			if ((*g_pSoundManager).SetData( soundID, pBuffer, pOld )!=0xFFFF)
			{
				g_SDLAudio.Release(pOld);
			}

			g_SDLAudio.CenterPan( pBuffer );						
		
			g_SDLAudio.SetMaxVolume(pBuffer);

			// Play
			g_SDLAudio.Play( pBuffer, false, g_bGoodFPS );	
			
			g_SoundPerSecond++;

			//-------------------------------------------------
			// Force Feel
			//-------------------------------------------------
			{
			}
		}
	}
	//-----------------------------------------------------------
	// 있는 경우 --> Play
	//-----------------------------------------------------------
	else
	{
		LPDIRECTSOUNDBUFFER pBuffer;
		if ((*g_pSoundManager).GetData(soundID, pBuffer))
		{			
			g_SDLAudio.CenterPan( pBuffer );
		
			g_SDLAudio.SetMaxVolume(pBuffer);

			g_SDLAudio.Play( pBuffer, false, g_bGoodFPS );
			g_SoundPerSecond++;
		}

	}	

	__END_PROFILE("PlaySound2")
}

void PlaySoundForce(TYPE_SOUNDID soundID)
{
	__BEGIN_PROFILE("PlaySound2")

	//-----------------------------------------------------------
	// 정의되지 않는 sound ID일 경우..
	//-----------------------------------------------------------
	if (!g_SDLAudio.IsInit() || soundID >= (*g_pSoundTable).GetSize()
		)
	{
		__END_PROFILE("PlaySound2")
		return;
	}

	//-----------------------------------------------------------
	// 초당 소리낼 수 있는 개수 제한
	//-----------------------------------------------------------
	// player의 소리는 제한할 필요가 없다.

	//-----------------------------------------------------------
	// 없으면 --> Load & Play
	//-----------------------------------------------------------
	if ((*g_pSoundManager).IsDataNULL(soundID))
	{
		const char* pFilename = (*g_pSoundTable)[soundID].Filename;

		if (pFilename==NULL)
		{
			__END_PROFILE("PlaySound2")
			return;
		}

		// Sound.inf is data, not code: a filename longer than the buffer
		// is truncated so the load fails and gets logged, not a smashed stack.
		char strFilename[256];
		Basic::CopyBounded(strFilename, pFilename, sizeof(strFilename)-1);
		strFilename[sizeof(strFilename)-1] = '\0';
 		LPDIRECTSOUNDBUFFER pBuffer = g_SDLAudio.LoadWav( strFilename );

		//-----------------------------------------------------------
		// Loading 실패
		//-----------------------------------------------------------
		if (pBuffer==NULL)
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Failed to Load WAV. id=%d, fn=%s", soundID, strFilename );
		}
		//-----------------------------------------------------------
		// Load에 성공 했으면...
		//-----------------------------------------------------------
		else
		{
			// Replace됐으면 원래것을 메모리에서 지운다.
			LPDIRECTSOUNDBUFFER pOld;
			if ((*g_pSoundManager).SetData( soundID, pBuffer, pOld )!=0xFFFF)
			{
				g_SDLAudio.Release(pOld);
			}

			g_SDLAudio.CenterPan( pBuffer );						
		
			g_SDLAudio.SetMaxVolume(pBuffer);

			// Play
			g_SDLAudio.Play( pBuffer, false, g_bGoodFPS );	
			
			g_SoundPerSecond++;

			//-------------------------------------------------
			// Force Feel
			//-------------------------------------------------
			{
			}
		}
	}
	//-----------------------------------------------------------
	// 있는 경우 --> Play
	//-----------------------------------------------------------
	else
	{
		LPDIRECTSOUNDBUFFER pBuffer;
		if ((*g_pSoundManager).GetData(soundID, pBuffer))
		{			
			g_SDLAudio.CenterPan( pBuffer );
		
			g_SDLAudio.SetMaxVolume(pBuffer);

			g_SDLAudio.Play( pBuffer, false, g_bGoodFPS );
			g_SoundPerSecond++;
		}

	}	

	__END_PROFILE("PlaySound2")
}

//-----------------------------------------------------------------------------
// Stop Sound
//-----------------------------------------------------------------------------
void	
StopSound(TYPE_SOUNDID soundID)
{
	//-----------------------------------------------------------
	// 정의되지 않는 sound ID일 경우..
	//-----------------------------------------------------------
	if (!g_SDLAudio.IsInit() || soundID == SOUNDID_NULL)
		return;

	//-----------------------------------------------------------
	// 정의되지 않는 sound ID일 경우..
	//-----------------------------------------------------------
	if (soundID == SOUNDID_NULL)
		return;

	//-----------------------------------------------------------
	// 없으면 --> 그냥 return
	//-----------------------------------------------------------
	if (soundID < g_pSoundManager->GetMaxIndex())
	{
		if ((*g_pSoundManager).IsDataNULL(soundID))
		{
			return;
		}
		//-----------------------------------------------------------
		// 있는 경우 --> Stop
		//-----------------------------------------------------------
		else
		{
			LPDIRECTSOUNDBUFFER pBuffer;
			if ((*g_pSoundManager).GetData(soundID, pBuffer))
			{	
				g_SDLAudio.Stop( pBuffer );
			}	
		}
	}
}

//---------------------------------------------------------------------------
// Play Music Current Zone
//---------------------------------------------------------------------------
// 날씨를 바꾼다.
//---------------------------------------------------------------------------
void		
PlayMusicCurrentZone()
{
	// 시간에 따라 play
	if(g_pEventManager->GetEventByFlag(EVENTFLAG_NOT_PLAY_SOUND))
		return;
	
	const int zoneID = g_bZonePlayerInLarge ? g_nZoneLarge : g_nZoneSmall;
	const ZONETABLE_INFO* zoneInfo = g_pZoneTable->Get(zoneID);
	const bool holyLand = zoneInfo != nullptr && zoneInfo->HolyLand;
	const bool waveMusic = g_pUserOption->PlayWaveMusic;
	const BYTE hour = g_pGameTime != nullptr ? g_pGameTime->GetHour() : 0;
	const bool war = holyLand && waveMusic && g_pWarManager != nullptr
		&& g_pWarManager->IsExist(zoneID);
	const int newMusicID = SelectZoneMusic(zoneID, hour, holyLand, waveMusic, war);

	if (g_Mode==MODE_GAME && g_pZone!=NULL && g_pUserOption->PlayMusic)
	{
		{
			{		
			}
			{
			}

			int musicID = newMusicID;

			if (musicID!=MUSICID_NULL)
			{
				if (g_pUserOption->PlayWaveMusic)
				{
					if( g_oggfile != NULL )
						fclose(g_oggfile);

					g_oggfile = NULL;

					if( g_SDLAudio.IsInit() )
					{
						g_pOGG->streamClose();
						g_oggfile = Basic::OpenFile( Basic::NormalizeDataPath( (*g_pMusicTable)[ musicID ].FilenameWav.GetString() ).c_str(), "rb");
						if( g_oggfile != NULL )
						{
							// 2004, 11, 8, sobeit add start - 그냥 에러 체크
							if(SOUND_ERR_OK == g_pOGG->streamLoad( g_oggfile, NULL ))
							{
								if(SOUND_ERR_OK == g_pOGG->streamPlay( SOUND_PLAY_REPEAT ))
								{
									int volume = (g_pUserOption->VolumeMusic - 15) * 250;
									g_pOGG->streamVolume( max( -10000, min( -1, volume ) ) );
								}
							}
							// 2004, 11, 8, sobeit add end - 그냥 에러 체크
						}
					}
				}
				else
				{
					g_Music.Play( (*g_pMusicTable)[ musicID ].Filename );
				}
			}
		}
	}
}

//---------------------------------------------------------------------------
// Set Weather
//---------------------------------------------------------------------------
// 날씨를 바꾼다.
//---------------------------------------------------------------------------
void		
SetWeather(int weather, int level)
{
	if (!s_PreviousWeather) s_PreviousWeather = weather;
	int& previousWeather = *s_PreviousWeather;

	//---------------------------------------------
	// 이전의 날씨와 관련된 Sound를 없앤다.
	//---------------------------------------------
	StopSound( g_previousSoundID );
	

	DEBUG_ADD_FORMAT("[Set Weather] %d, %d", weather, level);
	
	//---------------------------------------------
	// 날씨 종류에 따라서...
	//---------------------------------------------
	if(previousWeather == WEATHER_SPOT)
		g_pWeather->Release();
	
	switch (weather)
	{
		//------------------------------
		// 맑은 날씨
		//------------------------------
		case WEATHER_CLEAR :	
			// 이전에 비가 오던 날씨였으면...
			if (previousWeather==WEATHER_RAINY)
			{
				if (g_pTopView!=NULL && g_pTopView->IsInit())
				{
					g_pTopView->ClearShadowManager();
				}
			}
			
			g_pWeather->Stop();
			g_previousSoundID = SOUNDID_NULL;
		break;

		//------------------------------
		// 비
		//------------------------------
		case WEATHER_RAINY :
			// 이전에 맑은 날씨였다면...
			if (previousWeather!=WEATHER_RAINY)
			{
				if (g_pTopView!=NULL && g_pTopView->IsInit())
				{
					g_pTopView->ClearShadowManager();
				}
			}

			g_pWeather->SetRain( level<<3 );

			// 2는 비가 더 많이 올때의 소리
			g_previousSoundID = (level>=15)? SOUND_WORLD_WEATHER_RAIN_2 : SOUND_WORLD_WEATHER_RAIN_1; 
			
			// 반복해서 비 소리 내기...
			PlaySound( g_previousSoundID , 
						true, 
						g_pPlayer->GetX(), g_pPlayer->GetY() );
		break;

		//------------------------------
		// 눈
		//------------------------------
		case WEATHER_SNOWY  :
			// 이전에 비가 오던 날씨였으면...
			if (previousWeather==WEATHER_RAINY)
			{
				if (g_pTopView!=NULL && g_pTopView->IsInit())
				{
					g_pTopView->ClearShadowManager();
				}
			}

			g_pWeather->SetSnow( level<<3 );
			g_previousSoundID = SOUNDID_NULL;
		break;

		//------------------------------
		// Spot
		//------------------------------
		case WEATHER_SPOT  :
			// 이전에 비가 오던 날씨였으면...
			if (previousWeather==WEATHER_RAINY)
			{
				if (g_pTopView!=NULL && g_pTopView->IsInit())
				{
					g_pTopView->ClearShadowManager();
				}
			}
			
			g_pWeather->SetSpot( level<<3 );
			g_previousSoundID = SOUNDID_NULL;
			break;
	}

	previousWeather = weather;

	DEBUG_ADD("[Set Weather] OK");
}

//---------------------------------------------------------------------------
// Set Lightning
//---------------------------------------------------------------------------
void
SetLightning(DWORD delay)
{
	DEBUG_ADD("[Set Lightning]");
	
	//---------------------------------------------
	// 번개 표현..
	//---------------------------------------------
	if (g_pTopView!=NULL && g_pTopView->IsInit())
	{
		g_pTopView->SetFadeStart(1, 31, 10, 31,31,31);
	}

	// Queue the thunder at this frame's player position and timestamp.
	g_pZone->AddSound(MakeThunderSound(delay, g_pPlayer->GetX(), g_pPlayer->GetY(), g_FrameNow));
}

//---------------------------------------------------------------------------
// Update Input
//---------------------------------------------------------------------------
void
UpdateInput()
{
	// Input Event
	if (g_pSDLInput!=NULL)
	{
		g_pSDLInput->UpdateInput();
	}
}

//---------------------------------------------------------------------------
// Update Mouse
//---------------------------------------------------------------------------
void
UpdateMouse()
{
	// Cursor의 position을 얻어낸다.
	POINT point;
	GetCursorPos(&point);
	// add by svi
	ScreenToClient(g_hWnd, &point);
	// end

	// The game frame is letterbox-scaled into the window, so window-client
	// coordinates must map back to game coordinates (SpriteLib backend).
	{
		int mouseX = (int)point.x;
		int mouseY = (int)point.y;
		spritectl_window_to_game_coords(&mouseX, &mouseY);
		point.x = mouseX;
		point.y = mouseY;
	}

	g_x = point.x;
	g_y = point.y;


	//-----------------------------------------------
	// Mouse Cursor가 밖으로 나가지 않도록 한다.
	//-----------------------------------------------
	if (g_x<0) g_x=0;
	else if (g_x>=g_GameRect.right) g_x=g_GameRect.right-1;
	if (g_y<0) g_y=0;
	else if (g_y>=g_GameRect.bottom) g_y=g_GameRect.bottom-1;

}

//---------------------------------------------------------------------------
// Update Disconnected
//---------------------------------------------------------------------------
void
UpdateDisconnected()
{
	DEBUG_ADD("UpdateDisconnected : Start Process");
	
	//--------------------------------------------------
	// socket제거
	//--------------------------------------------------
	s_pGameMainHost->ReleaseSocket();

	//--------------------------------------------------
	// 소리 중지
	//--------------------------------------------------
	StopSound( g_previousSoundID );
	StopSound( SOUND_WORLD_PROPELLER );

	//--------------------------------------------------
	// 음악 멈춘다.
	//--------------------------------------------------
	if (g_pUserOption->PlayWaveMusic)
	{
		DEBUG_ADD("MP3 STOP5");
		if( g_SDLAudio.IsInit() )
			g_pOGG->streamClose();
		DEBUG_ADD("MP3 STOP5 OK");
	}
	else
	{
		g_Music.Stop();
	}

	DEBUG_ADD("UpdateDisconnected : Stop Music OK");
	
	//--------------------------------------------------
	// Thread Loading 종료..
	//--------------------------------------------------
	s_pGameMainHost->StopLoadingThread();

	DEBUG_ADD("UpdateDisconnected : Stop Thread OK");
	
	//--------------------------------------------------
	// game object들 제거
	//--------------------------------------------------
	ReleaseGameObject();

	DEBUG_ADD("UpdateDisconnected : Release GameObject OK");
	
	//------------------------------------------------------
	// dialog들 다 닫기.
	//------------------------------------------------------
	gC_vs_ui.CloseOption();
	g_pUIDialog->CloseMessageDlg();
	g_pUIDialog->ClosePCTalkDlg();

	DEBUG_ADD("UpdateDisconnected : Close All Dialog OK");

#ifdef __EMSCRIPTEN__
	// The caller has restored the title screen. Let its ordinary frame loop
	// draw and dismiss this dialog: a synchronous input wait prevents the
	// browser from delivering the keyboard/mouse events needed to close it.
	g_pUIDialog->PopupFreeMessageDlg(
		(*g_pGameStringTable)[UI_STRING_MESSAGE_CANNOT_CONNECT_SERVER].GetString());
	return;
#endif
	

	//--------------------------------------------------
	// 접속 끊겼다는 dialog
	//--------------------------------------------------
	// 필살~ 임시 코드..
	CSpritePack		SPK;
	CFileIndexTable	FIT;
	SPK.Init(3);
	CSprite* pSpriteDisconected = &SPK[0];
	CSprite* pSpriteDisconectedCloseFocused = &SPK[1];
	CSprite* pSpriteDisconectedClosePushed = &SPK[2];

	const std::string indexPath = g_pFileDef->getProperty("FILE_SPRITEINDEX_UI");
	std::ifstream indexFile(indexPath, ios::binary);
	if (!FIT.LoadFromFile(indexFile)) {
		LOG_ERROR("Rejected disconnect sprite index: source=%s", indexPath.c_str());
	} else {
		const std::string path = g_pFileDef->getProperty("FILE_SPRITE_UI");
		std::ifstream spkFile(path, ios::binary);
		const unsigned ids[] = { SPRITEID_DISCONNECTED, SPRITEID_DISCONNECTED_CLOSE_FOCUSED,
			SPRITEID_DISCONNECTED_CLOSE_PUSHED };
		for (WORD i = 0; i < 3; ++i) {
			long offset = 0;
			if (!FIT.TryGetOffset(ids[i], offset)) {
				LOG_ERROR("Missing disconnect sprite index: source=%s id=%u", indexPath.c_str(), ids[i]);
				continue;
			}
			spkFile.clear();
			spkFile.seekg(offset);
			CTypePackDetail::LoadElement(SPK[i], spkFile, path.c_str(), ids[i]);
		}
	}

	DEBUG_ADD("UpdateDisconnected : Load Disconnected Dialog OK");
	
	//--------------------------------------------------
	// 접속 끊겼다는 걸 먼저 한번 띄워준다.
	//--------------------------------------------------
	{
		POINT point;

		//-----------------------------------------------------------------
		// Disconnected dialog
		//-----------------------------------------------------------------
		g_pLast->Lock();
		
		point.x = (g_GameRect.right / 2)  - (pSpriteDisconected->GetWidth()>>1);
		point.y = (g_GameRect.bottom / 2) - (pSpriteDisconected->GetHeight()>>1);

		g_pLast->BltSprite( &point, pSpriteDisconected );

		g_pLast->Unlock();

		//-----------------------------------------------------------------
		// Last를 Back으로 copy - 3D HAL이 아닌 경우만..
		//-----------------------------------------------------------------
		point.x = 0;
		point.y = 0;
		RECT rect = { 0, 0, g_GameRect.right, g_GameRect.bottom };

		g_pBack->BltNoColorkey( &point, g_pLast, &rect );	
	}	

	CSDLGraphics::Flip();	

	DEBUG_ADD("UpdateDisconnected : Draw Disconnected Dialog Once OK");
	
	
	DEBUG_ADD("UpdateDisconnected : UI Start Title OK");
	
	//------------------------------------------------------
	// ALT + TAB 상태가 아닐 때
	//------------------------------------------------------
	if (g_bActiveGame)
	{
		DEBUG_ADD("UpdateDisconnected : Wait For Press Key");
		
		const int closeX	= 111;
		const int closeY	= 69;
		const int closeWidth = 198;
		const int closeHeight = 20;

		BOOL bClickClose = FALSE, bClickFocused = FALSE;
		
		MSG                         msg;

		while (1)
		{
			// UI로의 입력을 막아야 되는데...
			if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
			{	
				if (!GetMessage(&msg, NULL, 0, 0))
					break;

				if (msg.message!=WM_KEYDOWN 
					&& msg.message!=WM_CHAR 
					&& msg.message!=WM_KEYUP
					&& msg.message!=WM_IME_COMPOSITION
					&& msg.message!=WM_IME_STARTCOMPOSITION
					&& msg.message!=WM_IME_ENDCOMPOSITION)
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}
		
			UpdateInput();
			UpdateMouse();

			POINT point;
			
			RECT rectClose;
			rectClose.left = g_GameRect.right /2 - (pSpriteDisconected->GetWidth()>>1) + closeX;
			rectClose.top = g_GameRect.bottom /2 - (pSpriteDisconected->GetHeight()>>1) + closeY;
			rectClose.right = rectClose.left + closeWidth;
			rectClose.bottom = rectClose.top + closeHeight;

			if (g_pSDLInput->KeyDown(DIK_ESCAPE)
				|| g_pSDLInput->KeyDown(DIK_NUMPADENTER)
				|| g_pSDLInput->KeyDown(DIK_RETURN)
				|| g_pSDLInput->KeyDown(DIK_LMENU)
				|| g_pSDLInput->KeyDown(DIK_RMENU)
				|| g_pSDLInput->KeyDown(DIK_TAB)
				|| g_pSDLInput->KeyDown(DIK_SPACE))
			{
				break;
			}

			if (g_x > rectClose.left && g_x < rectClose.right
				&& g_y > rectClose.top && g_y < rectClose.bottom)
			{
				bClickFocused = TRUE;
			}
			else bClickFocused = FALSE;

			if (g_pSDLInput->m_lb_down)
			{		
				if (g_x > rectClose.left && g_x < rectClose.right
					&& g_y > rectClose.top && g_y < rectClose.bottom)
				{
					bClickClose = TRUE;
				}
			}

			if (g_pSDLInput->m_lb_up)
			{
				if (bClickClose)
				{
					if (g_x > rectClose.left && g_x < rectClose.right
						&& g_y > rectClose.top && g_y < rectClose.bottom)
					{
						break;
					}
				}

				bClickClose = FALSE;
			}
		

			{
				gC_vs_ui.Show();

				//-----------------------------------------------------------------
				// Disconnected dialog
				//-----------------------------------------------------------------
				g_pLast->Lock();
				
				point.x = g_GameRect.right /2 - (pSpriteDisconected->GetWidth()>>1);
				point.y = g_GameRect.bottom/2 - (pSpriteDisconected->GetHeight()>>1);

				g_pLast->BltSprite( &point, pSpriteDisconected );

				if (bClickFocused)
				{
					if (g_x > rectClose.left && g_x < rectClose.right
						&& g_y > rectClose.top && g_y < rectClose.bottom)
					{
						point.x = g_GameRect.right /2 - (pSpriteDisconected->GetWidth()>>1) + closeX;
						point.y = g_GameRect.bottom/2 - (pSpriteDisconected->GetHeight()>>1) + closeY;

						g_pBack->BltSprite( &point, pSpriteDisconectedCloseFocused );
					}
				}

				if (bClickClose)
				{
					if (g_x > rectClose.left && g_x < rectClose.right
						&& g_y > rectClose.top && g_y < rectClose.bottom)
					{
						point.x = g_GameRect.right /2 - (pSpriteDisconected->GetWidth()>>1) + closeX;
						point.y = g_GameRect.bottom/2 - (pSpriteDisconected->GetHeight()>>1) + closeY;

						g_pLast->BltSprite( &point, pSpriteDisconectedClosePushed );
					}
				}

				g_pLast->Unlock();

				gC_vs_ui.MouseControl(M_MOVING, g_x, g_y);
				gC_vs_ui.DrawMousePointer();

				//-----------------------------------------------------------------
				// Last를 Back으로 copy - 3D HAL이 아닌 경우만..
				//-----------------------------------------------------------------
				point.x = 0;
				point.y = 0;
				RECT rect = { 0, 0, g_GameRect.right, g_GameRect.bottom };

				g_pBack->BltNoColorkey( &point, g_pLast, &rect );	
			}

			CSDLGraphics::Flip();

			// Yield CPU to prevent 100% usage
			// On Windows, PeekMessage/GetMessage handle this
			// On macOS/Linux with SDL, we need to explicitly yield
			SDL_Delay(41);
		}
		
		pSpriteDisconected->Release();
		pSpriteDisconectedCloseFocused->Release();
		pSpriteDisconectedClosePushed->Release();

		// 2004, 07, 19 sobeit add start - 서버와 연결할수 없습니다. 확인 누르면 게임 종료하게 수정
		if(true == g_pUserInformation->IsAutoLogIn) // 웹 자동 로긴모드 일 때만
			SetMode(MODE_QUIT);
		// 2004, 07, 19 sobeit add end

	}
	else
	{
		DEBUG_ADD("UpdateDisconnected : Now ALT+TAB Mode");
		
		{
			POINT point;
			
			gC_vs_ui.Show();

			gC_vs_ui.MouseControl(M_MOVING, g_x, g_y);
			gC_vs_ui.DrawMousePointer();

			//-----------------------------------------------------------------
			// Last를 Back으로 copy - 3D HAL이 아닌 경우만..
			//-----------------------------------------------------------------
			point.x = 0;
			point.y = 0;
			RECT rect = { 0, 0, g_GameRect.right, g_GameRect.bottom };

			g_pBack->BltNoColorkey( &point, g_pLast, &rect );	
		}	
		
		CSDLGraphics::Flip();

		pSpriteDisconected->Release();
		pSpriteDisconectedCloseFocused->Release();
		pSpriteDisconectedClosePushed->Release();
	}

	DEBUG_ADD("UpdateDisconnected : OK");
}

//-----------------------------------------------------------------------------
// Select LastSelected Character
//-----------------------------------------------------------------------------
// 이전에 선택한 캐릭터를 선택하기
//-----------------------------------------------------------------------------
void
SelectLastSelectedCharacter()
{
	if (g_pWorldPlayerConfigTable==NULL)
	{
		g_pWorldPlayerConfigTable = new WorldPlayerConfigTable;
	}

	if (g_pWorldPlayerConfigTable!=NULL
		&& g_pUserInformation!=NULL
		&& g_pServerInformation!=NULL)
	{
		if (g_pWorldPlayerConfigTable->empty())
		{
			g_pWorldPlayerConfigTable->LoadFromFile( g_pFileDef->getProperty("FILE_INFO_PLAYER_INFO").c_str());
		}

		int worldID = g_pServerInformation->GetServerGroupID();

		PlayerConfigTable* pPCTable = g_pWorldPlayerConfigTable->GetPlayerConfigTable( worldID );

		if (pPCTable!=NULL)
		{
			const char* playerID = g_pUserInformation->UserID.GetString();		

			if( strlen( playerID ) >= 12 )
				return;
		
			PlayerConfig* pConfig = pPCTable->GetPlayerConfig( playerID );

			if (pConfig!=NULL)
			{
				gC_vs_ui.SelectCharacter( pConfig->GetLastSlot() );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Select LastSelected Character
//-----------------------------------------------------------------------------
// 현재 선택한 캐릭터를 저장하기
//-----------------------------------------------------------------------------
void
SaveLastSelectedCharacter(int slot)
{
	if (g_pWorldPlayerConfigTable==NULL)
	{
		g_pWorldPlayerConfigTable = new WorldPlayerConfigTable;
	}

	if (g_pWorldPlayerConfigTable!=NULL
		&& g_pServerInformation!=NULL
		&& g_pUserInformation!=NULL)
	{		
		int			worldID		= g_pServerInformation->GetServerGroupID();
		const char* playerID	= g_pUserInformation->UserID.GetString();
		
		PlayerConfigTable* pPCTable = g_pWorldPlayerConfigTable->GetPlayerConfigTable( worldID );

		if (pPCTable==NULL)
		{
			// worldID가 없으면 생성해서 추가
			pPCTable = new PlayerConfigTable;

			g_pWorldPlayerConfigTable->AddPlayerConfigTable( worldID, pPCTable );
		}
		
		PlayerConfig* pConfig = pPCTable->GetPlayerConfig( playerID );

		if (pConfig==NULL)
		{
			// playerID가 없으면 생성해서 추가
			pConfig = new PlayerConfig;
			if( playerID != NULL && strlen( playerID ) < 15 )
				pConfig->SetPlayerID( playerID );
			else
				pConfig->SetPlayerID( "DarkEden" );

			pPCTable->AddPlayerConfig( pConfig );
		}		

		// 항상 저장하게 하자..
		{
			pConfig->SetLastSlot( slot );		

			g_pWorldPlayerConfigTable->SaveToFile( g_pFileDef->getProperty("FILE_INFO_PLAYER_INFO").c_str());
		}
	}
}


// GetMakeItemFitPosition: ClientFunction.cpp (gamemodel, task 4.237).

// 2004, 03, 29 sobeit add start - 질드레 맵 고스트 추가
void
Add_GDR_Ghost(int ZoneID)
{
	if (g_pZone == NULL || (ZoneID != 1412 && ZoneID != 1413))
		return;
	CRarFile ghostFile;
	ghostFile.SetRAR("data\\ui\\txt\\TutorialEtc.rpk", "darkeden");
	if (!ghostFile.Open("ghostPos.xml") || ghostFile.GetFilePointer() == NULL)
		return;

	XMLTree tree;
	XMLParser parser;
	parser.parse(static_cast<char*>(ghostFile.GetFilePointer()), &tree);

	char name[64];
	snprintf(name, sizeof(name), "PositionList_%d", ZoneID);
	const XMLTree* map = tree.GetChild(name);
	if (map == NULL)
		return;

	for (size_t i = 0; i < map->GetChildCount(); ++i)
	{
		snprintf(name, sizeof(name), "Position%zu", i + 1);
		const XMLTree* position = map->GetChild(name);
		if (position == NULL)
			continue;
		const XMLAttribute* x = position->GetAttribute("x");
		const XMLAttribute* y = position->GetAttribute("y");
		if (x == NULL || y == NULL)
			continue;
		const int mapX = x->ToInt();
		const int mapY = y->ToInt();
		if (mapX < 0 || mapY < 0 || mapX >= g_pZone->GetWidth() || mapY >= g_pZone->GetHeight())
			continue;

		MFakeCreature* ghost = NewFakeCreature(CREATURETYPE_GHOST, mapX, mapY, rand() % 8);
		if (ghost == NULL)
			continue;
		if (!g_pZone->AddFakeCreature(ghost))
		{
			delete ghost;
			continue;
		}
		ghost->SetZone(g_pZone);
		ghost->SetFakeCreatureType(MFakeCreature::FAKE_CREATURE_GHOST);
		ghost->SetMoveType(MCreature::CREATURE_FAKE_UNDERGROUND);
		ghost->AddEffectStatus(static_cast<EFFECTSTATUS>(EFFECTSTATUS_GHOST_1 + rand() % 2), 0xFFFF);
	}
}
