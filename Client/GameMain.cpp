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

#include "Client.h"
#include "UIFunction.h"
//#include "MFileDef.h"
#include "Properties.h"
#include "GameObject.h"
#include "ServerInfo.h"
#include "DebugInfo.h"
#include "DebugLog.h"
#include "PacketDef.h"
#include "SoundNode.h"
#include "AddonDef.h"
#include "UIDialog.h"
#include "MusicDef.h"
#include "ZoneInfoData.h"
#include "CServerInformation.h"
#include "UserInformation.h"
#include "TempInformation.h"

#include "COpeningUpdate.h"
#include "CWaitUIUpdate.h"
#include "CWaitPacketUpdate.h"
#include "CGameUpdate.h"
#include "COrderedList.h"
#include "SpriteIDDef.h"
#include "MMusic.h"
#include "CDirectSoundStream.h"
#include "MChatManager.h"
#include "MGameStringTable.h"
#include "MHelpManager.h"
#include "CSoundPartManager.h"
#include "MParty.h"
#include "MZoneSound.h"
#include "MZoneSoundManager.h"
#include "SoundDef.h"
#include "RequestServerPlayerManager.h"
#include "ClientCommunicationManager.h"
#include "MJusticeAttackManager.h"
#include "WavePackFileManager.h"
#include "PCConfigTable.h"
#include "Profiler.h"
#include "CMP3.h"
#include "MEventManager.h"
#include "MNPC.h"
#include "UtilityFunction.h"

// 2002.6.28 [UDP수정]

#include "MWarManager.h"
#include "MTimeItemManager.h"

#include "VS_UI.h"

#include "MGuildType.h"
#include "MFakeCreature.h"

#include "MTestDef.h"
#include "SoundSetting.h"

#include "SXml.h"

//add by zdj 2005.5.17
extern void	ExecuteLogout();
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




#if defined(OUTPUT_DEBUG) && defined(__METROTECH_TEST__)
//	#define OUTPUT_DEBUG_UPDATE_LOOP
#endif

extern BOOL g_bSetHotKey;
extern CSoundPartManager*	g_pSoundManager;


extern bool g_bGoodFPS;


// 헬기장 프로펠러 소리.. - -;
extern BOOL g_bPlayPropeller;

#ifdef OUTPUT_DEBUG
	extern bool g_bSlideScreenShot;
	extern bool g_bSaveSlideScreenShot;
	extern RECT g_rectScreenShot;
#endif

//#define	new			DEBUG_NEW
//#define	delete		DEBUG_DELETE

//-----------------------------------------------------------------------------
// Global
//-----------------------------------------------------------------------------
// mp3
//CMySound*			g_pMP3 = NULL;

// Avi
CAVI				*g_pAvi = NULL;

int g_previousSoundID = SOUNDID_NULL;

	
// Sound PartManager
CSoundPartManager*	g_pSoundManager = NULL;

// Cursor Surface
CStorageSurface*	g_pCursorSurface = NULL;

// mouse cursor
POINT				g_SelectSector = { SECTORPOSITION_NULL, SECTORPOSITION_NULL };
BOOL				g_bLButtonDown = FALSE;
BOOL				g_bRButtonDown = FALSE;
BOOL				g_bCButtonDown = FALSE;
BOOL				g_bUIInput		= FALSE;
//Edit by sonic 2006.7.27  修改加速地址
//g_UpdateDelay		= DELAY_UPDATE_GAME

int					g_UpdateDelay		= 0;

// Zone
WORD				g_ZoneCreatureColorSet = 0xFFFF;
WORD				g_MyBatColorSet = 0xFFFF;
bool				g_bZoneSafe = false;
bool				g_bHolyLand = false;
int					g_nZoneLarge = ZONEID_NULL;
int					g_nZoneSmall = ZONEID_NULL;
bool				g_bZoneLargeLoadImage = false;
bool				g_bZoneSmallLoadImage = false;
MZone*				g_pZoneLarge = NULL;				
MZone*				g_pZoneSmall = NULL;
bool				g_bZonePlayerInLarge = true;
MonotonicClock::TimePoint	g_ZoneRandomSoundTime;
bool				g_bWatchMode = false;


// effectManager
MScreenEffectManager*	g_pInventoryEffectManager = NULL;

// DirectDraw
CSpriteSurface*			g_pBack		= NULL;
CSpriteSurface*			g_pLast		= NULL;
int						g_ScreenShotNumber = 0;

CSDLStream*		g_pSDLStream = NULL;
CMP3*					g_pMP3 = NULL;
int						g_SoundPerSecond = 0;

// Chat string
#ifdef	OUTPUT_DEBUG
	CMessageArray*		g_pDebugMessage = NULL;
#endif

bool				g_bPutMessage = false;		// 화면에 debug메세지를 출력할까?

bool				g_bNewDraw = false;			// 화면을 다시 그렸는가? (커서 제외)
bool				g_bSmoothCursor = false;	// 부드러운(?) cursor를 출력할 수 있나?
bool				g_bNetStatusGood = true;		// 네트웍 상태가 좋은가?

CMessageArray*		g_pSystemMessage = NULL;
CMessageArray*		g_pPlayerMessage = NULL;
CMessageArray*		g_pNoticeMessage = NULL;
CMessageArray*		g_pGameMessage = NULL;

CMessageArray*		g_pHelpMessage = NULL;

// socket
ClientPlayer*		g_pSocket = NULL;

// Thread
MWorkThread*		g_pLoadingThread = NULL;
//HANDLE				g_hFileThread = 0;
//HANDLE				g_hFileEvent;
//THREADJOB			g_ThreadJob;
	

// Server IP
//char				g_ServerIP[80];
bool				g_bFullScreen	= false;
int					g_MaxNPC		= MAX_NPC;
bool				g_bHAL			= true;
bool				g_bMusicSW		= true;
extern int			g_MorphCreatureType;

//--------------------------------------------
// 현재의 Client Mode에 대한 설정..
//--------------------------------------------
enum CLIENT_MODE	g_Mode = MODE_NULL;
enum CLIENT_MODE	g_ModeNext = MODE_NULL;

CWinUpdate*				g_pUpdate = NULL;

extern MCreature*		AddClientCreature();
extern void Add_GDR_Effect(int nEffect, bool bAppearBossMonster);
extern void Add_GDR_Potal_Effect(int nMapID);
//---------------------------------------------------------------------------
// Update Socket Input
//---------------------------------------------------------------------------
bool
UpdateSocketInput()
{

	//DEBUG_ADD("-PS-");
	
	try {
		
		if (g_pSocket != NULL)
		{
			g_pSocket->processInput();
			g_pSocket->processCommand();
//			g_pSocket->processOutput();
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
		
		//InitFail("Server와의 접속이 끊어졌습니다.");
		SetMode( MODE_MAINMENU );
		UpdateDisconnected();


		return false;
	}


	//----------------------------------------------------------------
	// RequestServerPlayerManager도 처리한다.
	//----------------------------------------------------------------
	static MonotonicClock::TimePoint nextTime = g_FrameNow;

	if (nextTime <= g_FrameNow)
	{	
		#ifdef OUTPUT_DEBUG_UPDATE_LOOP
			DEBUG_ADD( "RequestUpdate" );
		#endif

		if (g_pRequestServerPlayerManager!=NULL)
		{
			//int numRequest = g_pRequestServerPlayerManager->GetSize();
			#ifdef OUTPUT_DEBUG_UPDATE_LOOP
				DEBUG_ADD( "RSPM" );
			#endif

			g_pRequestServerPlayerManager->Update();

			#ifdef OUTPUT_DEBUG_UPDATE_LOOP
				DEBUG_ADD( "RSPM ok" );
			#endif
		}

		if (g_pClientCommunicationManager!=NULL)
		{
			#ifdef OUTPUT_DEBUG_UPDATE_LOOP
				DEBUG_ADD( "CCMu" );
			#endif

			g_pClientCommunicationManager->Update();

			#ifdef OUTPUT_DEBUG_UPDATE_LOOP
				DEBUG_ADD( "CCMu ok" );
			#endif
		}


		// three updates a second
		nextTime = g_FrameNow + MonotonicClock::Millis(330);
	}

	//DEBUG_ADD("-OK-");
	

	return true;
}

//---------------------------------------------------------------------------
// Update Socket Output
//---------------------------------------------------------------------------
bool
UpdateSocketOutput()
{

	//DEBUG_ADD("-PS-");
	
	try {
		
		if (g_pSocket != NULL)
		{
			//g_pSocket->processInput();
			//g_pSocket->processCommand();
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
		DEBUG_ADD(t.toString().c_str());
		
		//InitFail("Server와의 접속이 끊어졌습니다.");
		SetMode( MODE_MAINMENU );
		UpdateDisconnected();


		return false;
	}

	// DEBUG_ADD("-OK-");
	
 
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

//检测非法进程 yckou
bool CheckInvalidProcess()
{
#ifdef PLATFORM_WINDOWS
	//以下这段检测非法进程 (Windows-specific anti-cheat check)
	if (g_bCheckHack)
	{
		HWND hCurrentWindow;
		char szText[255];
		hCurrentWindow = GetWindow(g_hWnd,GW_HWNDFIRST);
		while (hCurrentWindow != NULL)
		{
			if (GetWindowText(hCurrentWindow, szText, 255)>0)
			{
				int iLen = static_cast<int>(strlen(szText));
				for (int j=0;j<iLen;j++)
				{
					if(isupper((unsigned char)szText[j]) != 0)
						szText[j] = (char)tolower((unsigned char)szText[j]);
				}

// 				std::string strTemp = szText;

				for (int i=0;i<MAX_INVALID_PROCESS;i++)
				{
					if (ContainsIgnoreCase(szText, g_strBadProcessList[i].c_str()) &&
						!ContainsIgnoreCase(szText, "microsoft internet explorer") &&
						!ContainsIgnoreCase(szText, "myie") &&
						!ContainsIgnoreCase(szText, "dudu") &&
						!ContainsIgnoreCase(szText, "苟潼"))
					{
						g_bCheckHack = false;
//						MessageBox(g_hWnd,"您使用了不合适的外挂程序,将与服务器断开连接!",NULL,MB_OK);
//						ExecuteLogout();
//						abort();
						// client를 update해야한다.
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
		// 초기 화면
		//------------------------------------------------------
		case MODE_OPENING :
			// Debug Message
			DEBUG_ADD("[ SetMode ]  OPENING");
			
			UnInitSound();		// 잠시 Sound 중지

			g_pCOpeningUpdate->PlayMPG("test.mpg");

			//----------------------------------------------
			// update 함수 설정
			//----------------------------------------------
			g_pUpdate = g_pCOpeningUpdate;
			g_pCOpeningUpdate->Init();
		break;

		//------------------------------------------------------
		// 뭔가 option을 바꾼다. - -;
		//------------------------------------------------------
		case MODE_CHANGE_OPTION :
			//if (g_pTopView!=NULL)
			{
			//	delete g_pTopView;//->Release();
			//	g_pTopView = NULL;
			}
			//--------------------------------------------------
			// 음악 멈춘다.
			//--------------------------------------------------
			if (g_pUserOption->PlayWaveMusic)
			{
#ifdef __USE_MP3__
					DEBUG_ADD("MP3 STOP1");
					g_pMP3->Stop();
					DEBUG_ADD("MP3 STOP1 OK");
//					g_pSDLStream->Stop();
#else
					if( g_SDLAudio.IsInit() )
						g_pOGG->streamClose();
#endif
			}
			else
			{
				g_Music.Stop();
			}

			CSDLGraphics::RestoreGammaRamp();
			CSDLGraphics::ReleaseAll();

			InitDraw();
			InitSurface();

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
				ReleaseSocket();


			//------------------------------------------------------
			// [ TEST CODE ]
			// 여기서 sound를 초기화해도 되남??
			//------------------------------------------------------
			InitSound();			

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
			//gC_vs_ui.EndTitle();
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

			//UpdateDisconnected();

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

			//g_pSDLStream->Load( "music.wav" );//(*g_pMusicTable)[ g_pClientConfig->MUSIC_THEME ].Filename );
			//g_pSDLStream->Play(TRUE);

			//g_SDLMusic.SetOriginalTempo();
			//g_SDLMusic.Stop();			

			//g_SDLMusic.Play( (*g_pMusicTable)[ g_pClientConfig->MUSIC_THEME ].Filename );
		
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
			// Player의 정보 초기화
			//----------------------------------------------
			//g_pPlayer->SetCreatureType( 0 );			
			
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
			
			//------------------------------------------------------
			// Loading 중지
			//------------------------------------------------------
			//StopFileThread();

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
			//gC_vs_ui.InvalidIdPasswordMessage();

			/*
			//InitFail("Server가 응답하지 않습니다.");
			//InitFail("Server와의 접속이 끊어졌습니다.");
			// g_pBack->GDI_Text(101,201, "ID나 Password가 틀렸습니다.", RGB(0,0,0));
			TextSystem::TextService::RenderText(101, 201, "ID나 Password가 틀렸습니다.");
			// g_pBack->GDI_Text(100,200, "ID나 Password가 틀렸습니다.", RGB(220,220,220));
			TextSystem::TextService::RenderText(100, 200, "ID나 Password가 틀렸습니다.");

			// g_pBack->GDI_Text(101,221, "[ESC]를 누르세요.", RGB(0,0,0));
			TextSystem::TextService::RenderText(101, 221, "[ESC]를 누르세요.");
			// g_pBack->GDI_Text(100,220, "[ESC]를 누르세요.", RGB(220,220,220));
			TextSystem::TextService::RenderText(100, 220, "[ESC]를 누르세요.");

			CSDLGraphics::Flip();

			// return을 누를 때까지...
			while (1)
			{
				UpdateInput();
				
				if (g_pSDLInput->KeyDown(DIK_ESCAPE))
				{
					break;
				}
			}
			*/
			
			// 다시 mainmenu로..

			g_Mode = MODE_MAINMENU;

			//-----------------------
			// Socket 해제..
			//-----------------------
			ReleaseSocket();

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
			//UI_StartCharacterManager();				

			g_ZoneRandomSoundTime = g_FrameNow;

			//------------------------------------------------------------
			// server name 설정
			//------------------------------------------------------------
			/*
			int serverID = g_pServerInformation->GetServerGroupID();
			int status = g_pServerInformation->GetServerGroupStatus();

			char str[80];

			if (g_pServerInformation->GetServerGroupName()!=NULL)
			{
				snprintf(str, sizeof(str), "%s", g_pServerInformation->GetServerGroupName());

				//gC_vs_ui.SetServerName( str );
				DEBUG_ADD_FORMAT("[ SetMode ]  MODE_WAIT_PCLIST - ServerName[%d]=%s", serverID, str);
			}
			
//			gC_vs_ui.SetServerDefault( str, serverID, (C_VS_UI_CHAR_MANAGER::SERVER_STATUS)status );

//			gC_vs_ui.CharManagerDisable();
			*/

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
			//g_pPlayer->SetCreatureType( 0 );	
			g_pUserInformation->KeepConnection = FALSE;
			
//			gC_vs_ui.CharManagerEnable();

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
				
				delete g_pPlayer;//->SetSpecialActionInfoNULL();
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
			InitSkillTree();

			//------------------------------
			// 게임 UI 를 시작한다.
			//------------------------------
			UI_SetCharInfoName(g_pUserInformation->CharacterID);
			DEBUG_ADD("[ SetMode ] SET CHARINFO");
			UI_StartGame();

			DEBUG_ADD("[ SetMode ] UI FINISH");

			g_ZoneRandomSoundTime = g_FrameNow;

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
			InitSound();

			static int first = 1;
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

				// 2002.6.28 [UDP수정]
				// 서버에 UDP port를 알려주기 위해서
//				CGPortCheck cgPortCheck;
//				cgPortCheck.setPCName( g_pUserInformation->CharacterID.GetString() );
//				
//				string ServerAddress = g_pConfig->getProperty("LoginServerAddress");
//				uint ServerPort = g_pConfig->getPropertyInt("LoginServerCheckPort");
//
//				g_pClientCommunicationManager->sendPacket( ServerAddress,
//															ServerPort,
//															&cgPortCheck );

			//
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


//			//-----------------------------------------
//			// 자크 도움말..
//			//-----------------------------------------
//			if (g_pPlayer!=NULL
//				&& g_pPlayer->IsSlayer()
//				&& g_pInventory!=NULL
//				&& g_pSlayerGear!=NULL
//				&& g_pPlayer->GetSTR()+g_pPlayer->GetDEX()+g_pPlayer->GetINT()==30
//				&& g_pInventory->GetItemNum()==0
//				&& g_pSlayerGear->GetItemNum()==0)
//			{
//				gC_vs_ui.AddChatToHistory((*g_pGameStringTable)[STRING_MESSAGE_HOW_TO_GET_BASIC_WEAPON].GetString(), 
//											(*g_pGameStringTable)[UI_STRING_MESSAGE_HELP_MESSAGE].GetString(), 
//											CLD_INFO, RGB(130, 230, 230));
//			}
//Edit by sonic 2006.7.27  修改加速地址
			//g_UpdateDelay = DELAY_UPDATE_GAME;
			g_UpdateDelay = 100 ^ 90;
			
			DEBUG_ADD("SetUpdate");
g_pUpdate = g_pCGameUpdate;
		g_pCGameUpdate->Init();

		// Ensure input is enabled after start game
		CheckActivate(TRUE);

		ExecuteHelpEvent( HELP_EVENT_INTERFACE );
//			if(!g_pUserInformation->IsNetmarble)
//			{
//				ExecuteHelpEvent( HELP_EVENT_CAMPAIGN );
//				ExecuteHelpEvent( HELP_EVENT_EVENT );
//			}

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
							//g_pSocket->setPlayerStatus( CPS_END_SESSION );

					}
					//--------------------------------------------------
					// game중에 종료하는 경우.. CG Logout
					//--------------------------------------------------
					else
					{
						CGLogout cgLogout;
					
						g_pSocket->sendPacket( &cgLogout );

						#ifdef __DEBUG_OUTPUT__
							DEBUG_ADD(cgLogout.toString().c_str());					
						#endif
					}
				}

			//--------------------------------------------------
			// Thread Loading 종료..
			//--------------------------------------------------
			StopLoadingThread();

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
	//static BOOL musicPause = FALSE; //g_Music.IsPause();

	#ifdef OUTPUT_DEBUG
		if (g_pDebugMessage != NULL)
		{
			if (bActiveGame)
			{
				if (g_bActiveApp)
				{
					DEBUG_ADD("[CheckActivate] ActiveGame, ActiveApp");
				}
				else
				{
					DEBUG_ADD("[CheckActivate] ActiveGame, DeactiveApp");
				}
			}
			else
			{
				if (g_bActiveApp)
				{
					DEBUG_ADD("[CheckActivate] DeactiveGame, ActiveApp");
				}
				else
				{
					DEBUG_ADD("[CheckActivate] DeactiveGame, DeactiveApp");
				}
			}
		}
	#endif

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

	[[maybe_unused]] extern bool	g_bTestMode;
	if (g_bActiveApp
#ifdef OUTPUT_DEBUG
		|| g_bTestMode
#endif
		)
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

			#ifdef OUTPUT_DEBUG
				if (g_pDebugMessage != NULL)
					DEBUG_ADD("WM_ACTIVATEAPP : Stop Music");
			#endif
			//g_SDLMusic.Pause();
			//musicPause = g_Music.IsPause();

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
// File Thread Proc
//-----------------------------------------------------------------------------
/*
LONG
FileThreadProc(LPVOID lpParameter)
{
	std::ifstream file;

	while ( 1 )
	{		
		WaitForSingleObject(g_hFileEvent, INFINITE);

		#ifdef OUTPUT_DEBUG
			//		DEBUG_ADD("[Thread Job] Start");
		#endif
	
		bool bLoad = true;
			
		switch (g_ThreadJob)
		{
			//--------------------------------------------------------
			// Large Zone의 ImageObject를 Loading한다.
			//--------------------------------------------------------
			case THREADJOB_LOAD_IMAGEOBJECT_LARGEZONE :	
				g_bZoneLargeLoadImage = false;
				file.open((*g_pZoneTable).Get(g_nZoneLarge)->Filename, ios::binary);

				// Tile Loading
				if (g_ThreadJob==THREADJOB_LOAD_IMAGEOBJECT_LARGEZONE)
				{
					#ifdef OUTPUT_DEBUG
						//DEBUG_ADD("[Thread Job] Load Tile LargeZone");
					#endif
					file.seekg(g_pZone->GetTileFilePosition(), ios::beg);
					if (!g_pTopView->LoadFromFileTileSPKLargeZone( file ))	// tile만 load
					{
						#ifdef OUTPUT_DEBUG
						//		DEBUG_ADD("[Thread Job] Stop Loading Tile");
						#endif
						file.close();
						break;
					}
				}
			
				// ImageObject Loading
				if (bLoad && g_ThreadJob==THREADJOB_LOAD_IMAGEOBJECT_LARGEZONE)
				{
					#ifdef OUTPUT_DEBUG
						//	DEBUG_ADD("[Thread Job] Load Imageobject LargeZone");
					#endif
					file.seekg(g_pZone->GetImageObjectFilePosition(), ios::beg);				
					// 모두 load한 경우이면..
					if (g_pTopView->LoadFromFileImageObjectSPKLargeZone( file ))
					{
						#ifdef OUTPUT_DEBUG
							//	DEBUG_ADD("[Thread Job] Stop Loading ImageObject");
						#endif
						file.close();
						break;
					}
				}

				if (bLoad && g_ThreadJob==THREADJOB_LOAD_IMAGEOBJECT_LARGEZONE)
				{
					#ifdef OUTPUT_DEBUG
						//	DEBUG_ADD("[Thread Job] Complete Load");
					#endif
					g_bZoneLargeLoadImage = true;
				}

				file.close();
			break;

			//--------------------------------------------------------
			// Small Zone의 ImageObject를 Loading한다.
			//--------------------------------------------------------
			case THREADJOB_LOAD_IMAGEOBJECT_SMALLZONE :
				g_bZoneSmallLoadImage = false;
				file.open((*g_pZoneTable).Get(g_nZoneSmall)->Filename, ios::binary);

				// Tile Loading
				if (g_ThreadJob==THREADJOB_LOAD_IMAGEOBJECT_SMALLZONE)
				{
					#ifdef OUTPUT_DEBUG
					//		DEBUG_ADD("[Thread Job] Load Tile SmallZone");
					#endif

					file.seekg(g_pZone->GetTileFilePosition(), ios::beg);
					if (!g_pTopView->LoadFromFileTileSPKLargeZone( file ))	// tile만 load
					{
						#ifdef OUTPUT_DEBUG
								//DEBUG_ADD("[Thread Job] Stop Loading Tile");
						#endif
						file.close();
						break;
					}
				}
			

				// ImageObject Loading
				if (bLoad && g_ThreadJob==THREADJOB_LOAD_IMAGEOBJECT_SMALLZONE)
				{
					#ifdef OUTPUT_DEBUG
						//	DEBUG_ADD("[Thread Job] Load ImageObject SmallZone");
					#endif
					file.seekg(g_pZone->GetImageObjectFilePosition(), ios::beg);
					if (!g_pTopView->LoadFromFileImageObjectSPKSmallZone( file ))
					{
						#ifdef OUTPUT_DEBUG
							//	DEBUG_ADD("[Thread Job] Stop Loading ImageObject");
						#endif
						file.close();
						break;
					}
				}

				if (bLoad && g_ThreadJob==THREADJOB_LOAD_IMAGEOBJECT_SMALLZONE)
				{
					g_bZoneSmallLoadImage = true;
				}

				file.close();
			break;

			//--------------------------------------------------------
			// Creature를 Loading한다.
			//--------------------------------------------------------
			case THREADJOB_LOAD_CREATURE :
				g_pTopView->LoadFromFileCreatureSPK( 0 );
				g_pTopView->LoadFromFileCreatureSPK( 1 );
			break;
		}

		#ifdef OUTPUT_DEBUG
			//	DEBUG_ADD("[Thread Job] End");
		#endif

		ResetEvent(g_hFileEvent);
	}

	return 0L;
}
*/

//-----------------------------------------------------------------------------
// Stop File Thread
//-----------------------------------------------------------------------------
/*
void
StopFileThread()
{	
	#ifdef OUTPUT_DEBUG
	//		DEBUG_ADD("[Thread Job] StopFileThread");
	#endif

	switch (g_ThreadJob)
	{
		case THREADJOB_LOAD_IMAGEOBJECT_LARGEZONE :
			//g_nZoneLarge = ZONEID_NULL;

			// ImageObject Loading을 중단한다.
			g_pTopView->StopLoadTileSPK();
			g_pTopView->StopLoadImageObjectSPK();
			g_bZoneLargeLoadImage = false;
			
			// 우선 순위를 높게 잡고...
			SetThreadPriority(g_hFileThread, 
								THREAD_PRIORITY_HIGHEST);
		break;
		
		case THREADJOB_LOAD_IMAGEOBJECT_SMALLZONE :
			//g_nZoneSmall = ZONEID_NULL;

			// ImageObject Loading을 중단한다.
			g_pTopView->StopLoadTileSPK();
			g_pTopView->StopLoadImageObjectSPK();	
			g_bZoneSmallLoadImage = false;

			// 우선 순위를 높게 잡고...
			SetThreadPriority(g_hFileThread, 
								THREAD_PRIORITY_HIGHEST);
		break;
		
		case THREADJOB_LOAD_CREATURE :

			//g_pTopView->StopLoadCreature();

			// 캐릭터 그림 Load중인가?
			// 우선 순위를 높게 잡고...
			SetThreadPriority(g_hFileThread, 
								THREAD_PRIORITY_HIGHEST);
		break;
	}

	g_ThreadJob = THREADJOB_NONE;

	#ifdef OUTPUT_DEBUG
	//		DEBUG_ADD("[Thread Job] Wait for Stopping thread");
	#endif

	while (WaitForSingleObject(g_hFileEvent, 0) == WAIT_OBJECT_0);

		// Thread가 하는 Loading이 끝날때까지 기다린다.
	#ifdef OUTPUT_DEBUG
	//		DEBUG_ADD("[Thread Job] Stop File Thread");
	#endif
	

	// 순위를 좀 낮춘다.
	SetThreadPriority(g_hFileThread, 
						THREAD_PRIORITY_BELOW_NORMAL);
}
*/

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
// Load CreatureType
//-----------------------------------------------------------------------------
// creature type을 보고 sprite를 load한다.
//-----------------------------------------------------------------------------
BOOL
LoadCreatureType(int creatureType)
{
	// Debug Message
	DEBUG_ADD_FORMAT("LoadCreatureType : %d", creatureType);
	
	int spriteType = (*g_pCreatureTable)[creatureType].SpriteTypes[0];

	if (!(*g_pCreatureSpriteTable)[spriteType].bLoad)
	{
		//if (spriteType==9)
		//{
		//	int a = 9;
		//}

		LoadCreature( spriteType );

		return TRUE;
	}

	DEBUG_ADD_FORMAT("Don't need to LoadCreatureType : %d", creatureType);
	
	return FALSE;
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
	// Small Zone 제거
	//------------------------------------------------------
	/*
	if (g_pZoneSmall != NULL)
	{
		DEBUG_ADD("[ delete SmallZone ]");
		delete g_pZoneSmall;	
		g_pZoneSmall = NULL;
		g_nZoneSmall = ZONEID_NULL;	
		g_bZoneSmallLoadImage = false;
	}			
	*/

	//------------------------------------------------------
	// Large Zone 제거
	//------------------------------------------------------
	/*
	if (g_pZoneLarge != NULL)
	{
		DEBUG_ADD("[ delete LargeZone ]");
		delete g_pZoneLarge;	
		g_pZoneLarge = NULL;
		g_nZoneLarge = ZONEID_NULL;	
		g_bZoneLargeLoadImage = false;
	}
	*/

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
		DEBUG_ADD_FORMAT("[Error] Wrong Zone ID=%d", n);	
		
		return FALSE;
	}

	//------------------------------------------------
	// 모든 사운드를 정지해버린다.
	//------------------------------------------------
	if (g_pSoundManager!=NULL)
	{
		g_pSoundManager->Stop();		
		g_bPlayPropeller = FALSE;
	}
	g_SDLAudio.ReleaseDuplicateBuffer();


	// Debug Message
	DEBUG_ADD_FORMAT("LoadZone : ID=%d, Filename=%s", pZoneInfo->ID, pZoneInfo->Filename.GetString());		
	
	//------------------------------------------------
	// Music 정지...
	//------------------------------------------------
	//BOOL bMusicPause = g_SDLMusic.IsPause();
	//g_SDLMusic.Stop();

	//BOOL bMusicPause = g_Music.IsPause();

	if (g_pUserOption->PlayWaveMusic)
	{
#ifdef __USE_MP3__
		if (g_pMP3 != NULL)
		{
			DEBUG_ADD("MP3 STOP4");
			g_pMP3->Stop();
			DEBUG_ADD("MP3 STOP4 OK");
		}
#else
		if( g_pOGG != NULL )
		{
			if( g_SDLAudio.IsInit() )
				g_pOGG->streamClose();
		}
#endif
//		if (g_pSDLStream!=NULL)
//		{
//			g_pSDLStream->Stop();
//		}
//		g_pSDLStream->Update();
	}
	else
	{
		g_Music.Stop();
	}

	//------------------------------------------------
	// Fade Out 적용
	//------------------------------------------------
		/*
	if (g_pZone!=NULL && g_Mode==MODE_GAME )
	{
		g_pTopView->SetFadeOut(6);

		while(g_pTopView->IsFade())
		{
			g_CurrentTime = timeGetTime();
			g_pUpdate->Update();
		}

		// 음악 중지
		//g_Music.Stop();
	}
	*/

	//------------------------------------------------
	// Fade In 설정
	//------------------------------------------------
	//g_pTopView->SetFadeIn(10);


	//------------------------------------------------
	// 아직 다른 Zone을 Loading중이라면
	//------------------------------------------------
	/*
	if  (WaitForSingleObject(g_hFileEvent, 0) == WAIT_OBJECT_0)
	{
		StopFileThread();		
	}
	*/
	//g_pLoadingThread->Remove( 1 );

	//-------------------------------------------------------------
	// Loading 중이던것 모두 제거
	//-------------------------------------------------------------
	StopLoadingThread();

	// priority를 최대한 낮춘다.
	if (g_pLoadingThread!=NULL)
	{
		// Thread priority is platform-specific - currently implemented for Windows only
#ifdef PLATFORM_WINDOWS
		g_pLoadingThread->SetPriority( THREAD_PRIORITY_IDLE );
#endif // PLATFORM_WINDOWS
	}

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
		//ZONETABLE_INFO* pOldZoneInfo = (*g_pZoneTable).Get( g_nZoneSmall );

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
			
			// 그림이 Load되지 않은 상태면..
			/*
			if (!g_bZoneSmallLoadImage)
			{
				// Thread가 할일을 지정				
				g_ThreadJob = THREADJOB_LOAD_IMAGEOBJECT_SMALLZONE;				
				SetEvent(g_hFileEvent);
			}
			*/

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
				if (g_pLoadingThread!=NULL)
				{
					g_pLoadingThread->SetPriority( THREAD_PRIORITY_LOWEST );
				}

				return FALSE; 
			}

			DEBUG_ADD("[Load Zone] Load Zone Data");
			
			UI_DrawProgress( 80 );

			// Zone loading 실패?
			if (!g_pZone->LoadFromFile( file ))
			{
				SetMode( MODE_QUIT );

				// priority를 정상으로
				if (g_pLoadingThread!=NULL)
				{
					g_pLoadingThread->SetPriority( THREAD_PRIORITY_LOWEST );
				}
				return FALSE;
			}

			g_pZone->SetCurrentZoneID( n );
		
			//g_pTopView->LoadFromFileTileSPKLargeZone( file );	// tile만 load
			//g_pTopView->LoadFromFileImageObjectSPKLargeZone( file );
			file.close();			
			
			
			
			UI_DrawProgress( 90 );

			//-------------------------------------------------------
			// Player의 좌표 근처의 Sprite들을 Load한다.
			//-------------------------------------------------------
			DEBUG_ADD("[Load Zone] Find Tile&ImageObject ID");
			
			CSpriteSetManager TileSSM;
			CSpriteSetManager ImageObjectSSM;
			g_pZone->GetNearSpriteSet(TileSSM, ImageObjectSSM, g_pPlayer->GetX(), g_pPlayer->GetY());
			if (!g_pTopView->LoadFromFileTileAndImageObjectSet(TileSSM, ImageObjectSSM)) {
				LOG_ERROR("Rejected zone sprite preload: zone=%d", n);
				SetMode(MODE_QUIT);
				if (g_pLoadingThread) g_pLoadingThread->SetPriority(THREAD_PRIORITY_LOWEST);
				return FALSE;
			}

			DEBUG_ADD("[Load Zone] Find Tile&ImageObject ID.. OK");
			
			UI_DrawProgress( 95 );
		}
		
		//-------------------------------------------------------
		// LoadingThread가 할일을 지정			
		//-------------------------------------------------------
		//g_ThreadJob = THREADJOB_LOAD_IMAGEOBJECT_SMALLZONE;				
		//SetEvent(g_hFileEvent);
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
		
		//------------------------------------------
		// 음악 연주해야 되는 경우이면
		//------------------------------------------
		/*
		if (g_pUserOption->PlayMusic)
		{
			//------------------------------------------
			// 종족에 따라서 음악이 달라진다.
			//------------------------------------------
			TYPE_MUSICID	newMusicID = 0;

			if (g_pPlayer!=NULL && g_pPlayer->IsSlayer())
			{		
				newMusicID = pZoneInfo->MusicIDSlayer;
			}
			else
			{
				newMusicID = pZoneInfo->MusicIDVampire;
			}

			//g_SDLMusic.SetOriginalTempo();
			//g_SDLMusic.Play( (*g_pMusicTable)[ newMusicID ].Filename );			
			int musicID = newMusicID;

			if (musicID!=MUSICID_NULL)
			{
				if (g_pUserOption->PlayWaveMusic)
				{
					g_pSDLStream->Load( (*g_pMusicTable)[ musicID ].FilenameWav );
					g_pSDLStream->Play( TRUE );
				}
				else
				{
					g_Music.Play( (*g_pMusicTable)[ musicID ].Filename );
				}
			}
		}
		*/
	
		/*
		if (g_pMP3 != NULL)
		{
			g_pMP3->Pause();
			delete g_pMP3;
		}

		g_pMP3 = new CMySound( (*g_pMusicTable)[ (*g_pZoneTable)[g_nZoneSmall].MusicID ].Filename );
		g_pMP3->Play();
		*/
					
		g_bZonePlayerInLarge = false;
	}

	//----------------------------------------------------------------------
	//
	//					Large Zone
	//
	//----------------------------------------------------------------------
	else
	{
		//ZONETABLE_INFO* pOldZoneInfo = (*g_pZoneTable).Get( g_nZoneLarge );

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
			
			// 그림이 Load되지 않은 상태면..
			/*
			if (!g_bZoneLargeLoadImage)
			{
				// Thread가 할일을 지정				
				g_ThreadJob = THREADJOB_LOAD_IMAGEOBJECT_LARGEZONE;				
				SetEvent(g_hFileEvent);
			}
			*/

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
				if (g_pLoadingThread!=NULL)
				{
					g_pLoadingThread->SetPriority( THREAD_PRIORITY_LOWEST );
				}

				return FALSE; 
			}

			DEBUG_ADD("[Load Zone] Load Zone Data");
			
			UI_DrawProgress( 80 );

			// Zone loading 실패?
			if (!g_pZone->LoadFromFile( file ))
			{
				SetMode( MODE_QUIT );

				// priority를 정상으로
				if (g_pLoadingThread!=NULL)
				{
					g_pLoadingThread->SetPriority( THREAD_PRIORITY_LOWEST );
				}

				return FALSE;
			}

			//g_pTopView->LoadFromFileTileSPKLargeZone( file );	// Tile만 load
			//g_pTopView->LoadFromFileImageObjectSPKLargeZone( file );
			file.close();

			UI_DrawProgress( 90 );
			
			//-------------------------------------------------------
			// Player의 좌표 근처의 Sprite들을 Load한다.
			//-------------------------------------------------------
			DEBUG_ADD("[Load Zone] Find Tile&ImageObject ID");
			
			CSpriteSetManager TileSSM;
			CSpriteSetManager ImageObjectSSM;
			g_pZone->GetNearSpriteSet(TileSSM, ImageObjectSSM, g_pPlayer->GetX(), g_pPlayer->GetY());
			if (!g_pTopView->LoadFromFileTileAndImageObjectSet(TileSSM, ImageObjectSSM)) {
				LOG_ERROR("Rejected zone sprite preload: zone=%d", n);
				SetMode(MODE_QUIT);
				if (g_pLoadingThread) g_pLoadingThread->SetPriority(THREAD_PRIORITY_LOWEST);
				return FALSE;
			}
			
			DEBUG_ADD("[Load Zone] Find Tile&ImageObject ID.. OK");
				
			UI_DrawProgress( 95 );
		}

		//-------------------------------------------------------
		// LoadingThread가 할일을 지정			
		//-------------------------------------------------------
		//g_ThreadJob = THREADJOB_LOAD_IMAGEOBJECT_SMALLZONE;				
		//SetEvent(g_hFileEvent);
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
		
		//------------------------------------------
		// 음악 연주해야 되는 경우이면
		//------------------------------------------
		/*
		if (g_pUserOption->PlayMusic)
		{
			//------------------------------------------
			// 종족에 따라서 음악이 달라진다.
			//------------------------------------------
			//TYPE_MUSICID	oldMusicID = 0;
			TYPE_MUSICID	newMusicID = 0;

			DEBUG_ADD("[Load Zone] Before Music Check");
			
			if (g_pPlayer!=NULL && g_pPlayer->IsSlayer())
			{		
				newMusicID = pZoneInfo->MusicIDSlayer;
			}
			else
			{
				newMusicID = pZoneInfo->MusicIDVampire;
			}

			//g_SDLMusic.SetOriginalTempo();
			//g_SDLMusic.Play( (*g_pMusicTable)[ newMusicID ].Filename );			

			int musicID = newMusicID;

			if (musicID!=MUSICID_NULL)
			{
				if (g_pUserOption->PlayWaveMusic)
				{
					g_pSDLStream->Load( (*g_pMusicTable)[ musicID ].FilenameWav );
					g_pSDLStream->Play( TRUE );
				}
				else
				{
					g_Music.Play( (*g_pMusicTable)[ musicID ].Filename );
				}
			}
		}
		*/
	
		/*
		if (g_pMP3 != NULL)
		{
			g_pMP3->Pause();
			delete g_pMP3;
		}

		g_pMP3 = new CMySound;// (*g_pMusicTable)[ (*g_pZoneTable)[g_nZoneLarge].MusicID ].Filename );
		//g_pMP3->Play();
		g_pMP3->Load( (*g_pMusicTable)[ (*g_pZoneTable)[g_nZoneLarge].MusicID ].Filename );
		*/

		g_bZonePlayerInLarge = true;
	}

	DEBUG_ADD("[Load Zone] Before Minimap Loading");
	
	//------------------------------------------------
	// UI에 zone설정..
	//------------------------------------------------
	SIZE zoneSize = { g_pZone->GetWidth(), g_pZone->GetHeight() };

	gC_vs_ui.SetSize( zoneSize );
	gC_vs_ui.SetZone( n );
	
#ifdef OUTPUT_DEBUG
//	if(C_VS_UI_MINIMAP::GetZoneName(n))
//	{
//		extern int CLogFile;
//		
//		_write( CLogFile, "<<<", 3 );
//		_write( CLogFile, C_VS_UI_MINIMAP::GetZoneName(n), strlen(C_VS_UI_MINIMAP::GetZoneName(n)));
//		_write( CLogFile, ">>>", 3 );
//		_write( CLogFile, "\n", 1 );
//	}
#endif
	
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
//	BYTE PkType = g_pZone->GetPKType();
//	if(PkType != PK_TYPE_NULL)	// 그냥 일반 맵
//	{
//		if(PkType == PK_TYPE_DISABLE)		// 다 우리편..-_-; pk 금지
//			g_pPlayer->SetAttackModePeace();
//		else if(PkType == PK_TYPE_ALL)		// 나 빼고 다 적
//			g_pPlayer->SetAttackModeAggress();
//		else if(PkType == PK_TYPE_GUILD)	// 우리 길드 빼고 다 적..
//		{
//			int myGuildID = g_pPlayer->GetGuildNumber();	
//			int OtherGuildID = pCreature->GetGuildNumber();	
//			if( myGuildID		== GUILDID_VAMPIRE_DEFAULT ||
//				myGuildID		== GUILDID_SLAYER_DEFAULT ||
//				myGuildID		== GUILDID_OUSTERS_DEFAULT ||
//				OtherGuildID	== GUILDID_VAMPIRE_DEFAULT ||
//				OtherGuildID	== GUILDID_SLAYER_DEFAULT ||
//				OtherGuildID	== GUILDID_OUSTERS_DEFAULT ) // 길드가 없는 녀석 들은..
//				return TRUE; // 혼자 싸워라..-_-;
//
//			if(myGuildID == OtherGuildID)
//				return FALSE; // 우리 길드...
//			else
//				return TRUE;
//		}
//	}
//	else
//		g_pPlayer->SetAttackModeNormal();

	// 2004, 6, 22 sobeit add end
	//------------------------------------------------
	// minimap 그림 설정
	//------------------------------------------------
	//g_pTopView->LoadMinimap( pZoneInfo->MinimapFilename );	
	
	//------------------------------------------------
	// Random SoundID 시간 설정
	//------------------------------------------------
	g_ZoneRandomSoundTime = g_FrameNow + MonotonicClock::Millis(((rand()%5)+10)*1000);	// 10 to 15 s from now
	
	


	//---------------------------------------------------------------
	// 하나의 소리에 대한 정보
	//---------------------------------------------------------------
	/*
	ZONESOUND_INFO* pSoundInfo	= new ZONESOUND_INFO;
	pSoundInfo->ID				= 1;
	pSoundInfo->SoundID			= SOUND_WORLD_WATER_DROP;
	pSoundInfo->Loop			= true;
	pSoundInfo->MinDelay		= 0;
	pSoundInfo->MaxDelay		= 24;
	pSoundInfo->StartHour		= 0;
	pSoundInfo->EndHour			= 24;
	
	g_pZoneSoundTable->AddData( pSoundInfo->ID, pSoundInfo );
	*/

	//*/
	
	

	//------------------------------------------------
	// 이 Zone에서 등장하는 Creature에 대한 Sprite들
	//------------------------------------------------
	//g_pTopView->LoadFromFileCreatureSPK( 0 );
	//g_pTopView->LoadFromFileCreatureSPK( 1 );

	

	//------------------------------------------------
	// View에게 출력해야할 Zone을 지정한다.
	//------------------------------------------------
	// 이전에 출력된 creature들을 없앤다.
	g_pTopView->SetZone(g_pZone);	

	// Debug Message
	DEBUG_ADD_FORMAT("LoadZone OK : size=(%d, %d)", g_pZone->GetWidth(), g_pZone->GetHeight());			
		

	// priority를 정상으로
	if (g_pLoadingThread!=NULL)
	{
		g_pLoadingThread->SetPriority( THREAD_PRIORITY_LOWEST );
	}

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

//	if( g_pWarManager->IsExist( pZoneInfo->ID ) )
//	{
//		g_bZoneSafe = false;
//	}

	g_pWarManager->ClearWar();
	g_pTimeItemManager->clear();

	g_pUserInformation->WorldName = g_pServerInformation->GetServerGroupName();
#ifdef __DEBUG_OUTPUT__
	for(int w=0;w<g_pZone->GetWidth();w++)
	{
		for(int h=0;h<g_pZone->GetHeight();h++)
		{
			if( g_pZone->GetSector(w,h).IsBlockAny() && g_pZone->GetID() == 3001 )
			{
				gC_vs_ui.SetBlock( w,h );
			}
		}
	}
#endif

#ifndef __METROTECH_TEST__
	if(gpC_base != NULL && g_pUserInformation->bCompetence == true && g_pUserInformation->bCompetenceShape != true &&
		g_pZoneTable->Get( g_pZone->GetID() )->CompetenceZone == false
		)
	{
		gpC_base->SendTextMessage(UI_CHAT_RETURN, CLD_NORMAL, 0, "*command ghost on");
	}
#endif
	
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
//		g_pPlayer->UnSetTimeLightSight();
//		g_pPlayer->UnSetLightSightAttachEffect();
		g_pZone->RemovePlayer();
	}
	
	// n Zone을 load한다.
	LoadZone( n );

	g_pPlayer->SetZone(g_pZone);
	g_pZone->SetPlayer();//&g_pPlayer->;	
	g_pPlayer->SetTimeLightSight(g_pPlayer->GetTimeLightSight());
//	g_pPlayer->SetLightSightAttachEffect();
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
		DEBUG_ADD_FORMAT("[Error] Wrong Zone ID=%d", n);	
		
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
//	MZoneInfo zoneInfo;
	std::ifstream zoneInfoFile(Basic::NormalizeDataPath(pZoneInfo->InfoFilename.GetString()), ios::binary);

	//------------------------------------------------
	// File이 있는 경우만 loading한다.
	//------------------------------------------------
	if (zoneInfoFile.is_open())
	{
		DEBUG_ADD("[Load Zone] Load MinimapInfo");
		
		/*
		zoneInfo.LoadFromFile( zoneInfoFile );		

		//------------------------------------------------
		// minimap에 안전지대를 표시한다.
		//------------------------------------------------
		g_pTopView->LoadMinimap( pZoneInfo->MinimapFilename, &zoneInfo );	
		*/
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
			//BYTE bSlayer = (g_pPlayer==NULL || g_pPlayer->IsSlayer());
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
						
						//pNPC->SetCreatureType( 0 );
						pNPC->SetCreatureType( npcID );
						pNPC->SetGroundCreature();
						pNPC->SetID(MFakeCreature::GetFakeID());
						//pNPC->SetAction(ACTION_MOVE);
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
						
						//
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
/*
			//---------------------------------------------------------------
			//
			//  Zone의 환경 사운드 설정
			//
			//---------------------------------------------------------------
			long fpCurrent = zoneInfoFile.tellg();
			zoneInfoFile.seekg( 0, ios::end );
			long fpEof = zoneInfoFile.tellg();
				
			if (!zoneInfoFile.eof()	// 왜 이게 제대로 안되지- -;
				&& fpCurrent!=fpEof)
			{
				zoneInfoFile.seekg( fpCurrent, ios::beg );
			
				if (g_pZoneSoundTable)
				{
					DEBUG_ADD("Delete ZoneSoundTable");
					
					delete g_pZoneSoundTable;
				}

				if (g_pZoneSoundManager)
				{		
					DEBUG_ADD("Delete ZoneSoundManager");
					
					delete g_pZoneSoundManager;
				}

				DEBUG_ADD("New ZoneSoundTable");
				
				// Zone의 사운드를 어떻게 낼까?에 대한 정보
				g_pZoneSoundTable = new MZoneSoundTable;

				DEBUG_ADD("Delete ZoneSoundManager");
				
				// Zone에 있는 사운드 자체에 대한 정보
				g_pZoneSoundManager = new MZoneSoundManager;

				
				DEBUG_ADD("Load SectorSound");
				
				//---------------------------------------------------------------
				// Sector에 사운드 정보를 Loading한다.
				//---------------------------------------------------------------
				g_pZone->LoadFromFileSectorSound( zoneInfoFile );
				
				//---------------------------------------------------------------
				// ZoneSoundTable에 
				// 현재 Map에 대한 사운드 정보를 Loading한다.
				//---------------------------------------------------------------
				DEBUG_ADD("Load ZoneSoundTable");	
				
				g_pZoneSoundTable->LoadFromFile( zoneInfoFile );

				DEBUG_ADD("Load ZoneSoundTable OK");
			}
			else
			{
				DEBUG_ADD("No ZoneSound Info");
			}			
*/
		}
		else
		{			
			DEBUG_ADD_FORMAT("[Error] Invalid ZoneInfo for zone ID=%d", n);
			return FALSE;
		}

		
		
		DEBUG_ADD("[Load Zone] Load ZoneInfo OK");
	}
	else
	{		
		DEBUG_ADD("[Load Zone] ZoneInfo File OpenError");
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
	
	//------------------------------------------------
	// Creature의 종류
	//------------------------------------------------
	//g_pPlayer->SetCreatureType( 0 );

	//g_pPlayer->SetCreatureType( id%2 );
	//g_pPlayer->SetCreatureType( 1 );

	g_pPlayer->ClearChatString();

	//------------------------------------------------
	// 살아있게 한다. 확인용...
	//------------------------------------------------
	g_pTopView->SetFadeEnd();

	//------------------------------------------------
	// 위치 설정
	//------------------------------------------------
	//g_pPlayer->SetPosition(x, y);	
	g_pPlayer->SetStop();
	g_pPlayer->SetX( x );
	g_pPlayer->SetY( y );
	g_pPlayer->SetServerPosition(x, y);
	g_pPlayer->SetDirection(dir);
	g_pPlayer->SetCurrentDirection(dir);
	//g_pPlayer->SetAction( ACTION_MOVE );	// Default

	//------------------------------------------------
	//  Zone <--> Player
	//------------------------------------------------
	g_pZone->SetPlayer();//&g_pPlayer->;	
	g_pPlayer->SetZone(g_pZone);

	//------------------------------------------------
	// 시야 확인..
	//------------------------------------------------
	//#ifdef CONNECT_SERVER
	//	g_pZone->KeepObjectInSight(x, y, g_pPlayer->GetSight());
	//#endif

	//------------------------------------------------
	// 상태값
	//------------------------------------------------
	//g_pPlayer->SetStatus(MODIFY_HP, 100);
	//g_pPlayer->SetStatus(MODIFY_MP, 100);
	
	//------------------------------------------------
	// 옷 입히기	
	//------------------------------------------------
	///*
	//g_pPlayer->SetAddonEnable(ADDON_HAIR);		// 머리
	//g_pPlayer->SetAddonEnable(ADDON_COAT);		// 상의
	//g_pPlayer->SetAddonEnable(ADDON_TROUSER);	// 하의
	//g_pPlayer->SetAddonEnable(ADDON_HELM);		// 모자
	//g_pPlayer->SetAddonEnable(ADDON_LEFTHAND);	// 왼손 
	//g_pPlayer->SetAddonEnable(ADDON_RIGHTHAND);	// 오른손	
	//g_pPlayer->SetAddonEnable(ADDON_MOTOR);		// 오토바이
	
	//g_pPlayer->SetAddon(ADDON_COAT, ADDONID_COAT1_MALE);	
	//g_pPlayer->SetAddon(ADDON_TROUSER, ADDONID_TROUSER1_MALE);
	////g_pPlayer->SetAddon(ADDON_HELM, 9);
	////g_pPlayer->SetAddon(ADDON_RIGHTHAND, 7);
	//g_pPlayer->SetAddon(ADDON_HAIR, ADDONID_HAIR1_MALE);
	//g_pPlayer->RemoveAddon(ADDON_MOTOR);//, ADDONID_MOTORCYCLE_MALE);
	//*/

	//------------------------------------------------
	// 시야 설정
	//------------------------------------------------
	//g_pPlayer->SetLightSight( 12 );	// 빛의 크기
	//g_pPlayer->SetSight( 12 );		// 캐릭터의 시야

	//------------------------------------------------
	// 공격, 기술 관련
	//------------------------------------------------
	// 공격
	//g_pPlayer->SetBasicActionInfo( SKILL_ATTACK_MELEE );
	
	// 공격 가능거리 설정
	//g_pPlayer->SetBasicAttackDistance( 1 );

	// 특수 공격
	//g_pPlayer->SetSpecialActionInfo( MAGIC_BLESS );

	// 정지 동작
	if (g_pPlayer->IsAlive())
	{
		g_pPlayer->SetAction( ACTION_STAND );
	}
	else
	{
		g_pPlayer->SetAction( ACTION_DIE );
	}

	// 움직이는 방법
	//g_pPlayer->SetMoveDevice( MCreature::MOVE_DEVICE_WALK );

	//-------------------------------------------------------
	// UI에 상태 적용
	//-------------------------------------------------------
	//gC_vs_ui.SetHP(g_pPlayer->GetHP(), g_pPlayer->GetMAX_HP());
	//gC_vs_ui.SetMP(g_pPlayer->GetMP(), g_pPlayer->GetMAX_MP());

	//-------------------------------------------------------
	// Server로 보낸 move packet 개수를 지우고
	// 처음부터~~ 다시 move하게 한다.
	//-------------------------------------------------------
	g_pPlayer->ResetSendMove();

	//-------------------------------------------------------
	// Attack Mode
	//-------------------------------------------------------
	//g_pPlayer->SetAttackModeNormal();


	//-------------------------------------------------------
	//
	//				사용할 수 있는 기술
	//
	//-------------------------------------------------------
	/*
	(*g_pSkillManager).Init( MAX_SKILLDOMAIN );

	//---------------------------------------------------------------------
	// 기본 기술로부터 skill tree를 초기화한다.
	//---------------------------------------------------------------------
	(*g_pSkillManager)[SKILLDOMAIN_BLADE].SetRootSkill( SKILL_SINGLE_BLOW );
	(*g_pSkillManager)[SKILLDOMAIN_SWORD].SetRootSkill( SKILL_DOUBLE_IMPACT );
	(*g_pSkillManager)[SKILLDOMAIN_GUN].SetRootSkill( SKILL_SNIPPING );
	(*g_pSkillManager)[SKILLDOMAIN_GUN].SetRootSkill( SKILL_SHARP_SHOOTING );
	(*g_pSkillManager)[SKILLDOMAIN_ENCHANT].SetRootSkill( MAGIC_CREATE_HOLY_WATER );
	(*g_pSkillManager)[SKILLDOMAIN_HEAL].SetRootSkill( MAGIC_CURE_POISON );
	(*g_pSkillManager)[SKILLDOMAIN_VAMPIRE].SetRootSkill( MAGIC_HIDE );	

	MSkillDomain& bladeDomain = (*g_pSkillManager)[SKILLDOMAIN_BLADE];
	MSkillDomain& swordDomain = (*g_pSkillManager)[SKILLDOMAIN_SWORD];
	MSkillDomain& gunDomain = (*g_pSkillManager)[SKILLDOMAIN_GUN];
	MSkillDomain& enchantDomain = (*g_pSkillManager)[SKILLDOMAIN_ENCHANT];
	MSkillDomain& healDomain = (*g_pSkillManager)[SKILLDOMAIN_HEAL];
	*/

	//---------------------------------------------------------------------
	// 몇가지 skill을 배웠다고 표시한다.
	//---------------------------------------------------------------------
	/*
	swordDomain.LearnSkill( SKILL_DOUBLE_IMPACT );
	swordDomain.LearnSkill( SKILL_TRIPLE_SLASHER );
	swordDomain.LearnSkill( SKILL_SCREW_SLASHER );
	swordDomain.LearnSkill( SKILL_RAINBOW_SLASHER );
	swordDomain.LearnSkill( SKILL_CRUSH_COMBO );
	swordDomain.LearnSkill( SKILL_CROSS_COUNTER );
	
	bladeDomain.LearnSkill( SKILL_SINGLE_BLOW );
	bladeDomain.LearnSkill( SKILL_TWIN_BREAK );
	bladeDomain.LearnSkill( SKILL_TRIPLE_BREAK );
	bladeDomain.LearnSkill( SKILL_SPIRAL_SLAY );
	bladeDomain.LearnSkill( SKILL_WILD_SMASH );
	bladeDomain.LearnSkill( SKILL_ARMOR_CRASH );

	gunDomain.LearnSkill( SKILL_SNIPPING );
	gunDomain.LearnSkill( SKILL_SHARP_SHOOTING );
	gunDomain.LearnSkill( SKILL_FAST_RELOAD );
	gunDomain.LearnSkill( SKILL_KNOCKS_TARGET_BACK );

	enchantDomain.LearnSkill( MAGIC_LIGHT );
	enchantDomain.LearnSkill( MAGIC_DETECT_HIDDEN );
	enchantDomain.LearnSkill( MAGIC_CONTINUAL_LIGHT );
	enchantDomain.LearnSkill( MAGIC_DETECT_INVISIBILITY );
	enchantDomain.LearnSkill( MAGIC_VISIBLE );

	healDomain.LearnSkill( MAGIC_CURE_POISON );
	healDomain.LearnSkill( MAGIC_PROTECTION_FROM_CURSE );
	healDomain.LearnSkill( MAGIC_CURE_LIGHT_WOUNDS );
	healDomain.LearnSkill( MAGIC_CURE_POISON );
	healDomain.LearnSkill( MAGIC_PROTECTION_FROM_POISON );
	healDomain.LearnSkill( MAGIC_CURE_PARALYSIS );

	*/
	//g_SkillAvailable.AddSkill( MAGIC_CURE_POISON );
	//g_SkillAvailable.AddSkill( MAGIC_PROTECTION_FROM_POISON );
	//g_SkillAvailable.AddSkill( MAGIC_CURE_PARALYSIS );

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
	// Gear 초기화
	//------------------------------------------------
	/*
	if (g_pPlayer->IsSlayer())
	{
		g_SlayerGear.Init();
	}
	else
	{
		g_VampireGear.Init();
	}

	//------------------------------------------------
	//  Inventory 초기화
	//------------------------------------------------
	g_Inventory.Init(10,6);
	*/

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

	#ifdef OUTPUT_DEBUG
		maxScreenShot = 1000;
	#endif

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
			#ifdef OUTPUT_DEBUG
				if (g_bSaveSlideScreenShot
					&& g_rectScreenShot.left!=g_rectScreenShot.right
					&& g_rectScreenShot.top!=g_rectScreenShot.bottom)
				{
					if (g_rectScreenShot.left > g_rectScreenShot.right) { int temp=g_rectScreenShot.left; g_rectScreenShot.left=g_rectScreenShot.right; g_rectScreenShot.right=temp; }
					if (g_rectScreenShot.top > g_rectScreenShot.bottom) { int temp=g_rectScreenShot.top; g_rectScreenShot.top=g_rectScreenShot.bottom; g_rectScreenShot.bottom=temp; }

					int width = g_rectScreenShot.right - g_rectScreenShot.left + 8;
					int height = g_rectScreenShot.bottom - g_rectScreenShot.top + 8;

					width &= ~0x07;
					height &= ~0x07;

					CSpriteSurface surface;
					surface.InitOffsurface( width, height, DDSCAPS_SYSTEMMEMORY );

					POINT point0 = { 0, 0 };
					RECT rect = g_rectScreenShot;
					rect.left += 1;
					rect.top += 1;
					surface.BltNoColorkey(&point0, g_pBack, &rect);

					surface.SaveToBMP(str);
				}
				else
			#endif

//			g_pBack->SaveToBMP(str);
			// SaveSurfaceToImage() (UtilityFunction.cpp) takes a
			// CDirectDrawSurface&, but with SPRITELIB_BACKEND_SDL (the only
			// backend this project builds now, Windows included)
			// CSpriteSurface is a standalone class that no longer inherits
			// from CDirectDrawSurface. g_pBack is a CSpriteSurface*,
			// so this call never type-checked under that backend.
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


	#ifdef OUTPUT_DEBUG
		g_bSaveSlideScreenShot = false;
	#endif
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

	//static int reload	= 0;
	//static int reuse	= 0;

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
			//LPDIRECTSOUNDBUFFER	pBuffer = g_pWavePackFileManager->LoadFromFileData(soundID);

			//-----------------------------------------------------------
			// Loading 실패
			//-----------------------------------------------------------
			if (pBuffer==NULL)
			{
				DEBUG_ADD_FORMAT("[Error] Failed to Load WAV. id=%d, fn=%s", soundID, strFilename );
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
				//int sub = dist * g_pUserOption->VolumeSound;

				g_SDLAudio.SubVolumeFromMax(pBuffer, sub);			

				// Play
				g_SDLAudio.Play( pBuffer, repeat, g_bGoodFPS);
				g_SoundPerSecond++;

				// Force Feel
				//-------------------------------------------------
				//if (gpC_Imm!=NULL && gpC_Imm->IsDevice())
				{
				//	gpC_Imm->ForceSound( strFilename );
				}

				//reload++;
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
				//int sub = dist * g_pUserOption->VolumeSound;
				
				g_SDLAudio.SubVolumeFromMax(pBuffer, sub);

				g_SDLAudio.Play( pBuffer, repeat, g_bGoodFPS );
				g_SoundPerSecond++;

				// Force Feel
				//-------------------------------------------------				
				/*
				if (gpC_Imm!=NULL && gpC_Imm->IsDevice())
				{
					const char* pFilename = (*g_pSoundTable)[soundID].Filename;
				
					if (pFilename!=NULL)
					{					
						char strFilename[256];
						snprintf(strFilename, sizeof(strFilename), "%s", pFilename);

						gpC_Imm->ForceSound( strFilename );
					}
				}
				*/
			}
			
			//reuse ++;
		}
	}

	__END_PROFILE("PlaySound1")

	//#ifdef OUTPUT_DEBUG
	//	DEBUG_ADD_FORMAT("miss(%d) : match(%d)  / %d", reload, reuse, reload+reuse);		
	//#endif
}

//-----------------------------------------------------------------------------
// PlaySound
//-----------------------------------------------------------------------------
// 소리나는 위치가 늘 player의 위치..
// CenterPan & MaxVolume
//-----------------------------------------------------------------------------
void	
PlaySound(TYPE_SOUNDID soundID)
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
	//if (g_SoundPerSecond > g_pClientConfig->MAX_SOUND_PER_SECOND)
	//{
	//	__END_PROFILE("PlaySound2")
		//return;
	//}

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
		//LPDIRECTSOUNDBUFFER	pBuffer = g_pWavePackFileManager->LoadFromFileData(soundID);

		//-----------------------------------------------------------
		// Loading 실패
		//-----------------------------------------------------------
		if (pBuffer==NULL)
		{
			DEBUG_ADD_FORMAT("[Error] Failed to Load WAV. id=%d, fn=%s", soundID, strFilename );
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
			//if (gpC_Imm!=NULL && gpC_Imm->IsDevice())
			{
			//	gpC_Imm->ForceSound( pFilename );
			}

			//reload++;
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


		// Force Feel
		//-------------------------------------------------
		/*
		const char* pFilename = (*g_pSoundTable)[soundID].Filename;

		if (pFilename!=NULL)
		{
			char strFilename[256];
			snprintf(strFilename, sizeof(strFilename), "%s", pFilename);
			if (gpC_Imm!=NULL && gpC_Imm->IsDevice())
			{
				gpC_Imm->ForceSound( strFilename );
			}
		}
		*/
		
		//reuse ++;
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
	//if (g_SoundPerSecond > g_pClientConfig->MAX_SOUND_PER_SECOND)
	//{
	//	__END_PROFILE("PlaySound2")
		//return;
	//}

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
		//LPDIRECTSOUNDBUFFER	pBuffer = g_pWavePackFileManager->LoadFromFileData(soundID);

		//-----------------------------------------------------------
		// Loading 실패
		//-----------------------------------------------------------
		if (pBuffer==NULL)
		{
			DEBUG_ADD_FORMAT("[Error] Failed to Load WAV. id=%d, fn=%s", soundID, strFilename );
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
			//if (gpC_Imm!=NULL && gpC_Imm->IsDevice())
			{
			//	gpC_Imm->ForceSound( pFilename );
			}

			//reload++;
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


		// Force Feel
		//-------------------------------------------------
		/*
		const char* pFilename = (*g_pSoundTable)[soundID].Filename;

		if (pFilename!=NULL)
		{
			char strFilename[256];
			snprintf(strFilename, sizeof(strFilename), "%s", pFilename);
			if (gpC_Imm!=NULL && gpC_Imm->IsDevice())
			{
				gpC_Imm->ForceSound( strFilename );
			}
		}
		*/
		
		//reuse ++;
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
	
	const MUSIC_ID musicByTime[] = 
	{
		MUSIC_LIVE_OR_DEAD,
		MUSIC_WINDMILL,
		MUSIC_WINDMILL,
		MUSIC_TREASURE,
		MUSIC_MARCHING,
		MUSIC_WHISPER,
		MUSIC_HELL_KNIGHT,
		MUSIC_LUNATIC,
		MUSIC_SAY_AGAIN,
		MUSIC_HIDE_AWAY,
		MUSIC_HELL_KNIGHT,
		MUSIC_HOLLOWEEN,		
	};


	
	const MUSIC_ID musicByHolyLand[] =
	{
		MUSIC_HOLYLAND,
		MUSIC_HOLYLAND_WAR,
		MUSIC_OCTAVUS,
		MUSIC_SEPTIMUS,
		MUSIC_TERTIUS,
		MUSIC_QUARTUS,
	};

	// 크리스마스 이벤트 날짜 체크
//	SYSTEMTIME st;
//	GetLocalTime( &st ); 
	
	// 크리스마스면 크리스마스 노래를 틀어준다.
	int newMusicID;

	// 안전지대인가?
	int zoneID	= (g_bZonePlayerInLarge?g_nZoneLarge : g_nZoneSmall);
	ZONETABLE_INFO* pZoneInfo = g_pZoneTable->Get( zoneID );	

//	if (bSafetyZone 
//		&& st.wMonth==12 && st.wDay>=17 && st.wDay<=25)
//	{
//		newMusicID = musicByTimeXmas[ (g_pGameTime->GetHour() / 2) % 12 ];
//	}
//	else
	if( pZoneInfo!=NULL && pZoneInfo->HolyLand && g_pUserOption->PlayWaveMusic)
	{
		switch ( zoneID )
		{
		case 1201 :
		case 1211 :
		case 1212 :
			newMusicID = musicByHolyLand[2];
			break;
		case 1202 :
		case 1221 :
		case 1222 :
			newMusicID = musicByHolyLand[4];
			break;
		case 1203 :
		case 1231 :
		case 1232 :
			newMusicID = musicByHolyLand[3];
			break;
		case 1204 :
		case 1241 :
		case 1242 :
			newMusicID = musicByHolyLand[5];
			break;
		case 1205 :
		case 1251 :
		case 1252 :
			newMusicID = musicByHolyLand[2];
			break;
		case 1206 :
		case 1261 :
		case 1262 :
			newMusicID = musicByHolyLand[4];
			break;
		default :
			if(g_pWarManager->IsExist( zoneID ) )
				newMusicID = musicByHolyLand[1];
			else
				newMusicID = musicByHolyLand[0];
			break;
		}
	} else 
	{
		newMusicID = musicByTime[ (g_pGameTime->GetHour() / 2) % 12 ];
	}	
	// 2004, 7, 6 sobeit add start - 질드레 레어 배경음 - bgm을 info에서 참조 하지 않는건지..
	if(zoneID == 1410 || zoneID == 1411)
		newMusicID = MUSIC_ILLUSIONS_WAY;
	else if(zoneID == 1412 || zoneID == 1413)
		newMusicID = MUSIC_GDR_LAIR;
	// 2004, 7, 6 sobeit add start

	if (g_Mode==MODE_GAME && g_pZone!=NULL && g_pUserOption->PlayMusic)
	{
		//int zondID = (g_bZonePlayerInLarge? g_nZoneLarge : g_nZoneSmall);

		//ZONETABLE_INFO* pZoneInfo = (*g_pZoneTable).Get( zondID );

		//if (pZoneInfo!=NULL)
		{
			//------------------------------------------
			// 종족에 따라서 음악이 달라진다.
			//------------------------------------------
			//TYPE_MUSICID	newMusicID = 0;

			//if (g_pPlayer!=NULL && g_pPlayer->IsSlayer())
			{		
			//	newMusicID = pZoneInfo->MusicIDSlayer;
			}
			//else
			{
			//	newMusicID = pZoneInfo->MusicIDVampire;
			}

			int musicID = newMusicID;

			if (musicID!=MUSICID_NULL)
			{
				if (g_pUserOption->PlayWaveMusic)
				{
#ifdef __USE_MP3__
					DEBUG_ADD("MP3 OPEN1");
					g_pMP3->Open( (*g_pMusicTable)[ musicID ].FilenameWav );
					DEBUG_ADD("MP3 OPEN1 OK");
					DEBUG_ADD("MP3 PLAY3");
					g_pMP3->Play( false );
					DEBUG_ADD("MP3 PLAY3 OK");
//					g_pSDLStream->Load( (*g_pMusicTable)[ musicID ].FilenameWav );
//					g_pSDLStream->Play( FALSE );
#else
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
#endif
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
	static int previousWeather = weather;

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

	// 1초 안의 거리에 있으면.. THUNDER1
	// 더 멀면 THUNDER2
	TYPE_SOUNDID sid = (delay<=1000)? SOUND_WORLD_WEATHER_THUNDER_1 : SOUND_WORLD_WEATHER_THUNDER_2;

	// sound를 기억해뒀다가 나중에 출력한다.
	SOUND_NODE* pNode = new SOUND_NODE( sid, delay, g_pPlayer->GetX(), g_pPlayer->GetY() );

	g_pZone->AddSound( pNode );
}

//---------------------------------------------------------------------------
// Open File
//---------------------------------------------------------------------------
bool
FileOpenBinary(const char* filename, std::ifstream& file)
{
	if (file.is_open())
	{
		file.close();
	}

	// The game's spelling of the path resolved to the disk's (basic/DataPath.h):
	// the identity on Windows; off Windows the separators folded and each
	// component matched case-insensitively, since the data tree's case does
	// not always match the tables. This used to fold the separators alone.
	const std::string convertedPath = Basic::NormalizeDataPath(filename);
	file.open(convertedPath.c_str(), ios::binary);

	if (!file.is_open())
	{
		/*
		DEBUG_ADD_FORMAT("[Error] File Open Error : [%s]", filename);
		*/
		InitFail("[Error] File Open Error : [%s]", filename);
		return false;
	}

	/*
	DEBUG_ADD_FORMAT("File Open OK : [%s]", filename);
	*/
	return true;
}

//---------------------------------------------------------------------------
// Update Input
//---------------------------------------------------------------------------
void
UpdateInput()
{
	// Input Event
	//g_pSDLInput->PollJoy();
	//g_pSDLInput->PollKey();
	//g_pSDLInput->PollMouse();    
	if (g_pSDLInput!=NULL)
	{
		g_pSDLInput->UpdateInput();
	}

	//---------------------------------------------------
	//
	// mouse
	//
	//---------------------------------------------------		
	/*
	static int xSign = 0;
	static int ySign = 0;
	static int mouseStep = 1;
	
	// 부호 결정
	int xSignNew = 0;
	int ySignNew = 0;

	if (g_pSDLInput->m_mouse_x)
	{
		xSignNew = (g_pSDLInput->m_mouse_xdata > 0)? 1: -1;
	}

	if (g_pSDLInput->m_mouse_y)
	{
		ySignNew = (g_pSDLInput->m_mouse_ydata > 0)? 1: -1;
	}

	//------------------------------------------------------
	// 정지된 상태
	//------------------------------------------------------
	if (xSignNew==0 && ySignNew==0)
	{		
		mouseStep = 6;
	}
	//------------------------------------------------------
	// 움직인 경우
	//------------------------------------------------------
	else
	{
		//------------------------------------------------------
		// 같은 방향으로 이동하면..
		//------------------------------------------------------
		if (xSignNew==xSign && ySignNew==ySign)
		{
			// 최대 mouse 이동 pixel설정
			if (abs(mouseStep) < 20)
			{
				mouseStep ++;
			}
		}
		//------------------------------------------------------
		// 다른 방향으로 이동하는 경우
		//------------------------------------------------------
		else
		{
			mouseStep = 6;			
		}

		xSign = xSignNew;
		ySign = ySignNew;

		// 커서를 움직여 준다.
		int step = mouseStep >> 1;
		if (g_pSDLInput->m_mouse_x) g_x += g_pSDLInput->m_mouse_xdata * (step? step : 1);
		if (g_pSDLInput->m_mouse_y) g_y += g_pSDLInput->m_mouse_ydata * (step? step : 1);
	}
	*/

	

	//sprintf(g_pDebugMessage->GetCurrent(), "(%d, %d)", g_pSDLInput->m_mouse_xdata, g_pSDLInput->m_mouse_ydata);
	//g_pDebugMessage->Next();
	
	
	//if (g_pSDLInput->m_mouse_x) g_x += g_pSDLInput->m_mouse_xdata*3;
	//if (g_pSDLInput->m_mouse_y) g_y += g_pSDLInput->m_mouse_ydata*3;	

	// Cursor의 position을 얻어낸다.
	/*
	POINT point;
	GetCursorPos(&point);
	g_x = point.x;
	g_y = point.y;


	//-----------------------------------------------
	// Mouse Cursor가 밖으로 나가지 않도록 한다.
	//-----------------------------------------------
	if (g_x<0) g_x=0;
	else if (g_x>=SURFACE_WIDTH) g_x=SURFACE_WIDTH-1;
	if (g_y<0) g_y=0;
	else if (g_y>=SURFACE_HEIGHT) g_y=SURFACE_HEIGHT-1;
	*/
	// - -;;
	//g_pSDLInput->SetMousePosition( g_x, g_y );

	//-----------------------------------------------
	// 새로 입력된 것을 CInputManager에 저장
	//-----------------------------------------------
	/*
	g_InputManager.SetPosition( g_x, g_y );

	if (g_pSDLInput->m_lbutton_down)	g_InputManager.SetLButtonDown();
	if (g_pSDLInput->m_lbutton_up)	g_InputManager.SetLButtonUp();
	if (g_pSDLInput->m_rbutton_down)	g_InputManager.SetRButtonDown();
	if (g_pSDLInput->m_rbutton_up)	g_InputManager.SetRButtonUp();
	*/}

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
//	RECT rc;
//	GetWindowRect(g_hWnd, &rc);

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
// keep Connection
//---------------------------------------------------------------------------
void
KeepConnection()
{
	/*
	// CGVerifyTime이 있으므로 이제 필요없다.
	#ifdef CONNECT_SERVER	
		if (
			#ifndef _DEBUG		// debug버전에서는 무조건 KeepConnection이다.
				(g_pUserInformation->KeepConnection || g_pUserInformation->IsMaster)
				&&				
			#endif
			g_pSocket!=NULL)
	
		{
			static MonotonicClock::TimePoint lastTime = g_FrameNow;

			//------------------------------------------------------------------
			// a keep-alive packet every three minutes
			//------------------------------------------------------------------
			if (g_FrameNow - lastTime > MonotonicClock::Millis(180000))		// 3 * 60 * 1000
			{
				#ifdef	CONNECT_SERVER			
					CGSay _CGSay;
					_CGSay.setMessage( "*" );
					g_pSocket->sendPacket( &_CGSay );			
				#endif

				lastTime = g_FrameNow;
			}
		}
	#endif
	*/
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
	ReleaseSocket();

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
//		g_pSDLStream->Stop();
		DEBUG_ADD("MP3 STOP5");
#ifdef __USE_MP3__
		g_pMP3->Stop();
#else
		if( g_SDLAudio.IsInit() )
			g_pOGG->streamClose();
#endif
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
	StopLoadingThread();

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
//	gC_vs_ui.CloseInfo();
//	gC_vs_ui.CloseShop();
	//UI_SetShop( NULL );
	//gC_vs_ui.CloseSkillTree();
	//gC_vs_ui.CloseGameMenu();
	//gC_vs_ui.CloseUserRegistrationWindow();
	
//	DEBUG_ADD("UpdateDisconnected : ServerDisc");
//	gC_vs_ui.ServerDisconnectMessage();
//	DEBUG_ADD("UpdateDisconnected : ServerDisc OK");

//	g_pUIDialog->CloseHelpDlg();
	g_pUIDialog->CloseMessageDlg();
	g_pUIDialog->ClosePCTalkDlg();

	DEBUG_ADD("UpdateDisconnected : Close All Dialog OK");
	

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
//	if (true)
//	{
//		POINT point;
//		if (false)  // CDirect3D::GetDevice()->BeginScene() removed (SDL2)
//		{
//			return;
//		}
//
//		//gC_vs_ui.Show();
//
//		//-----------------------------------------------------------------
//		// Disconnected dialog
//		//-----------------------------------------------------------------
//		g_pBack->Lock();
//		
//		point.x = 400 - (pSpriteDisconected->GetWidth()>>1);
//		point.y = 300 - (pSpriteDisconected->GetHeight()>>1);
//
//		g_pBack->BltSprite( &point, pSpriteDisconected );
//
////		const int maskX = 116;
////		const int maskY = 109;
////		point.x = 400 - (pSpriteDisconected->GetWidth()>>1) + maskX;
////		point.y = 300 - (pSpriteDisconected->GetHeight()>>1) + maskY;
////		g_pBack->BltSprite( &point, pSpriteDisconectedMask );
//
//		g_pBack->Unlock();
//
//		//gC_vs_ui.MouseControl(M_MOVING, g_x, g_y);
//		//gC_vs_ui.DrawMousePointer();
//	
//		// CDirect3D::GetDevice()->EndScene() removed (SDL2)

//	}
//	else
	{
		//gC_vs_ui.Show();
		POINT point;

		//-----------------------------------------------------------------
		// Disconnected dialog
		//-----------------------------------------------------------------
		g_pLast->Lock();
		
		point.x = (g_GameRect.right / 2)  - (pSpriteDisconected->GetWidth()>>1);
		point.y = (g_GameRect.bottom / 2) - (pSpriteDisconected->GetHeight()>>1);

		g_pLast->BltSprite( &point, pSpriteDisconected );

//		const int maskX = 116;
//		const int maskY = 109;
//		point.x = 400 - (pSpriteDisconected->GetWidth()>>1) + maskX;
//		point.y = 300 - (pSpriteDisconected->GetHeight()>>1) + maskY;
//		g_pBack->BltSprite( &point, pSpriteDisconectedMask );

		g_pLast->Unlock();

		//gC_vs_ui.MouseControl(M_MOVING, g_x, g_y);
		//gC_vs_ui.DrawMousePointer();

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
	
	
	//------------------------------------------------------
	// Main 화면..
	//------------------------------------------------------
//	gC_vs_ui.StartTitle();
	
	DEBUG_ADD("UpdateDisconnected : UI Start Title OK");
	
	//------------------------------------------------------
	// ALT + TAB 상태가 아닐 때
	//------------------------------------------------------
	[[maybe_unused]] extern bool	g_bTestMode;
	if (g_bActiveGame
#ifdef OUTPUT_DEBUG
		|| g_bTestMode
#endif
		)
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
					//return msg.wParam;

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
		

//			if (true)
//			{
//				if (false)  // CDirect3D::GetDevice()->BeginScene() removed (SDL2)
//				{
//					return;
//				}
//
//				gC_vs_ui.Show();
//
//				//-----------------------------------------------------------------
//				// Disconnected dialog
//				//-----------------------------------------------------------------
//				g_pBack->Lock();
//				
//				point.x = 400 - (pSpriteDisconected->GetWidth()>>1);
//				point.y = 300 - (pSpriteDisconected->GetHeight()>>1);
//
//				g_pBack->BltSprite( &point, pSpriteDisconected );
//
//				if (bClickFocused)
//				{
//					if (g_x > rectClose.left && g_x < rectClose.right
//						&& g_y > rectClose.top && g_y < rectClose.bottom)
//					{
//						point.x = 400 - (pSpriteDisconected->GetWidth()>>1) + closeX;
//						point.y = 300 - (pSpriteDisconected->GetHeight()>>1) + closeY;
//
//						g_pBack->BltSprite( &point, pSpriteDisconectedCloseFocused );
//					}
//				}
//
//				if (bClickClose)
//				{
//					if (g_x > rectClose.left && g_x < rectClose.right
//						&& g_y > rectClose.top && g_y < rectClose.bottom)
//					{
//						point.x = 400 - (pSpriteDisconected->GetWidth()>>1) + closeX;
//						point.y = 300 - (pSpriteDisconected->GetHeight()>>1) + closeY;
//
//						g_pBack->BltSprite( &point, pSpriteDisconectedClosePushed );
//					}
//				}
//
//				g_pBack->Unlock();
//
//				gC_vs_ui.MouseControl(M_MOVING, g_x, g_y);
//				gC_vs_ui.DrawMousePointer();
//			
//				// CDirect3D::GetDevice()->EndScene() removed (SDL2)

//			}
//			else
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
		
//		if (true)
//		{
//			if (false)  // CDirect3D::GetDevice()->BeginScene() removed (SDL2)
//			{
//				return;
//			}
//
//			gC_vs_ui.Show();	
//
//			gC_vs_ui.MouseControl(M_MOVING, g_x, g_y);
//			gC_vs_ui.DrawMousePointer();
//		
//			// CDirect3D::GetDevice()->EndScene() removed (SDL2)

//		}
//		else
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
	
	/*
	//InitFail("Server가 응답하지 않습니다.");
	//InitFail("Server와의 접속이 끊어졌습니다.");
	// g_pBack->GDI_Text(101,201, "Server가 응답하지 않습니다.", RGB(0,0,0));
	TextSystem::TextService::RenderText(101, 201, "Server가 응답하지 않습니다.");
	// g_pBack->GDI_Text(100,200, "Server가 응답하지 않습니다.", RGB(220,220,220));
	TextSystem::TextService::RenderText(100, 200, "Server가 응답하지 않습니다.");

	// g_pBack->GDI_Text(101,221, "[ESC]를 누르세요.", RGB(0,0,0));
	TextSystem::TextService::RenderText(101, 221, "[ESC]를 누르세요.");
	// g_pBack->GDI_Text(100,220, "[ESC]를 누르세요.", RGB(220,220,220));
	TextSystem::TextService::RenderText(100, 220, "[ESC]를 누르세요.");

	CSDLGraphics::Flip();

	// return을 누를 때까지...
	while (1)
	{
		UpdateInput();
		
		if (g_pSDLInput->KeyDown(DIK_ESCAPE))
		{
			break;
		}
	}

	// Title화면 UI시작
	//gC_vs_ui.StartTitle();

	//g_pBack->GDI_Text(101,201, "잠시 기다려 주세요.", RGB(0,0,0));
	//g_pBack->GDI_Text(100,200, "잠시 기다려 주세요.", RGB(220,220,220));
	*/
}

// ok를 누른 뒤 실행되는 것.
void RunAfterServerDisconnect()
{

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
		//if (oldSlot != slot || bNewPlayer)
		{
			pConfig->SetLastSlot( slot );		

			g_pWorldPlayerConfigTable->SaveToFile( g_pFileDef->getProperty("FILE_INFO_PLAYER_INFO").c_str());
		}
	}
}

//-----------------------------------------------------------------------------
// Set WatchMode ( true || false )
//-----------------------------------------------------------------------------
// 구경하는 mode..
//-----------------------------------------------------------------------------
void	
SetWatchMode(bool active)
{
	//-------------------------------------------------------------------
	// Player를 Zone에서 제거
	// UI 작동 못하게 설정?
	// 이동 방식 변경
	//-------------------------------------------------------------------
	if (active)
	{
		g_pPlayer->SetStop();
		g_pPlayer->SetAction( ACTION_STAND );
		g_pZone->RemovePlayer();

		g_bWatchMode = true;
	}
	//-------------------------------------------------------------------
	//
	//-------------------------------------------------------------------
	else
	{
		g_pPlayer->SetZone(g_pZone);
		g_pZone->SetPlayer();
		g_pPlayer->SetTimeLightSight(g_pPlayer->GetTimeLightSight());

		g_bWatchMode = false;
	}
}

//-----------------------------------------------------------------------------
// Add ClientCreature
//-----------------------------------------------------------------------------
MCreature*
AddClientCreature()
{
	//------------------------------------------------
	// NPC 
	//------------------------------------------------	
	const int hairMaleID[] = 
	{
		ADDONID_HAIR1_MALE,
		ADDONID_HAIR2_MALE,
		ADDONID_HAIR3_MALE
	};

	const int hairFemaleID[] = 
	{
		ADDONID_HAIR1_FEMALE,
		ADDONID_HAIR2_FEMALE,
		ADDONID_HAIR3_FEMALE
	};		

	static MCoat coat1;	coat1.SetItemType( 0 ); coat1.ClearItemOption();
	static MCoat coat2;	coat2.SetItemType( 1 );	coat2.ClearItemOption();
	static MItem* jacket[3] =
	{
		&coat1,		//JACKET_BASIC, 
		&coat1,		//JACKET1 , 
		&coat2		//JACKET2
	};

	static MTrouser	trouser1;	trouser1.SetItemType( 0 ); trouser1.ClearItemOption();
	static MTrouser	trouser2;	trouser2.SetItemType( 1 ); trouser2.ClearItemOption();
	static MItem* pants[3] =
	{
		&trouser1,	//PANTS_BASIC , 
		&trouser1,	//PANTS1 , 
		&trouser2	//PANTS2 ,
	};

	static MHelm helm1; helm1.SetItemType( 0 ); helm1.ClearItemOption(); 
	static MHelm helm2; helm2.SetItemType( 1 ); helm2.ClearItemOption();
	static MItem* helm[3] = 
	{
		NULL,		// NONE
		&helm1,		// HELMET1 ,
		&helm2,		// HELMET2
	};

	static MSword	sword;	sword.SetItemType( 0 );	sword.ClearItemOption();
	static MBlade	blade;	blade.SetItemType( 0 ); blade.ClearItemOption();
	static MGunAR	gunAR;	gunAR.SetItemType( 0 ); gunAR.ClearItemOption();
	static MGunTR	gunSR;	gunSR.SetItemType( 0 ); gunSR.ClearItemOption();
	static MGunSG	gunSG;	gunSG.SetItemType( 0 );	gunSG.ClearItemOption();
	static MGunSMG	gunSMG;	gunSMG.SetItemType( 0 );gunSMG.ClearItemOption();
	static MCross	cross;	cross.SetItemType( 0 );	cross.ClearItemOption();
	static MMace	mace;	mace.SetItemType( 0 ); mace.ClearItemOption();
	static MItem* weapon[11] =
	{
		NULL,		//WEAPON_NONE ,
		&sword,		//WEAPON_SWORD ,
		&blade,		//WEAPON_BLADE ,
		NULL,		//WEAPON_SHIELD ,
		&sword,		//WEAPON_SWORD_SHIELD ,
		&gunAR,		//WEAPON_AR ,
		&gunSR,		//WEAPON_SR ,
		&gunSG,		//WEAPON_SG ,
		&gunSMG,	//WEAPON_SMG ,
		&cross,		//WEAPON_CROSS ,			
		&mace
	};

	static MShield shield; shield.SetItemType( 0 ); shield.ClearItemOption();


		

	int creatureID;
	
	while (1)
	{
		creatureID = 1001 + rand()%50000;

		MCreature* pCreature = g_pZone->GetCreature( creatureID );

		if (pCreature==NULL)
		{
			break;
		}
	}

	int x, y;

	//-----------------------------------
	// 옷 입는 Creature
	//-----------------------------------
	if (0)//rand()%2)
	{
		MCreatureWear* pCreature = new MCreatureWear;

		pCreature->SetZone(g_pZone);

		// (*g_pCreatureTable)의 정보들 중의 하나..
		pCreature->SetCreatureType( rand()%2 );

		// Move 종류를 다르게 한다.
		switch (0)//rand()%2)//rand()%3)
		{
			case 0 :
				pCreature->SetGroundCreature();
			break;
		
			case 1 :
				pCreature->SetFlyingCreature();					
			break;
			
			case 2 :
				pCreature->SetUndergroundCreature();
			break;
		}						
		
		while (1)
		{
			x = rand()%(g_pZone->GetWidth()-1)+1;
			y = rand()%(g_pZone->GetHeight()-1)+1;
			//x = rand()%(20-1)+10;
			//y = rand()%(20-1)+20;	// 20~40
			/*
			int p = rand()%100;

			y = createPositionY[p];
			x = 12 - ((40-y)>>1) + (rand()%(41-y));

			if (rand()%2)
			{
				x += rand()%4;
			}
			else
			{
				x -= rand()%4;
			}
			*/

			if (x>=0 && x<g_pZone->GetWidth() 
				&& y>=0 && y<g_pZone->GetHeight())
			{					
				if (g_pZone->GetSector(x,y).IsEmpty())
				{
					pCreature->SetPosition( x, y );
					pCreature->SetServerPosition( x, y );
					break;
				}
			}
		}
		//pCreature->SetID(1000 + y*256 + x);
		
		pCreature->SetID( creatureID );

		// 이름 설정 - 일단은 ID로
		//char str[80];
		//sprintf(str, "ID=%d", pCreature->GetID());
		//pCreature->SetName( str );
		
		pCreature->SetDirection(DIRECTION_DOWN);
		pCreature->SetNextAction( ACTION_STAND );

		if (pCreature->IsMale())
		{
			pCreature->SetAddonHair( hairMaleID[rand()%3], 0);		// type, color				
		}
		else
		{
			pCreature->SetAddonHair( hairFemaleID[rand()%3], 0);		// type, color				
		}

		pCreature->SetAddonItem( jacket[rand()%3] );	
		pCreature->SetAddonItem( pants[rand()%3] );	
		pCreature->SetAddonItem( helm[rand()%3] );
		int w = rand()%10;
		pCreature->SetAddonItem( weapon[w] );
		if (w==3 || w==4)
		{
			pCreature->SetAddonItem( &shield );
		}

		

		if (!g_pZone->AddCreature(pCreature))
		{
			delete pCreature;
			return NULL;
		}

		return pCreature;
	}
	//-----------------------------------
	// 옷 없는 Creature
	//-----------------------------------
	else
	{				
		static int cType = 371;//199//11;
		
		LoadCreatureType( cType );

		MCreature* pCreature;

		if ((*g_pCreatureTable)[cType].pItemWearInfo!=NULL)
		{
			pCreature = new MCreatureWear;

			pCreature->SetCreatureType( cType );//11+rand()%(23-11+1) );

			SetAddonToSlayer( (MCreatureWear*)pCreature, cType );
		}
		else
		{
			pCreature = new MCreature;

			pCreature->SetCreatureType( cType );//11+rand()%(23-11+1) );
		}

		// 존재하는 Zone을 설정한다.
		pCreature->SetZone(g_pZone);

		// (*g_pCreatureTable)의 정보들 중의 하나..
		//pCreature->SetCreatureType( (rand()%((*g_pCreatureTable).GetSize()-4))+4 );				
		
		//------------------------------------------------------------
		// 바토리인 경우.. 하드코딩(-_-);
		//------------------------------------------------------------
		pCreature->SetStatus( MODIFY_MAX_HP, 100 );
		if (cType==217)
		{
			pCreature->SetStatus( MODIFY_CURRENT_HP, 10 );
			pCreature->SetStatus( MODIFY_ALIGNMENT, -10000 );
		}
		else
		{
			pCreature->SetStatus( MODIFY_CURRENT_HP, rand()%101 );
			pCreature->SetStatus( MODIFY_ALIGNMENT, -5000 + rand()%10000 );
		}
		
		cType++;

		if (cType==g_pCreatureTable->GetSize())
		{
			cType = 296;//104;//11;
		}
		else if (cType==111)
		{
			cType = 197;
		}
		else if (cType==200)
		{
			cType = 208;
		}
		else if (cType==210)
		{
			cType = 217;
		}
		else if (cType==220)
		{
			cType = 229;
		}
		else if (cType==231)
		{
			cType = 239;
		}
		else if (cType==241)
		{
			cType = 250;
		}
		else if (cType==251)
		{
			cType = 256;
		}
		else if (cType==257)
		{
			cType = 266;
		}
		else if (cType==267)
		{
			cType = 276;
		}
		else if (cType==277)
		{
			cType = 286;
		}
		else if (cType==288)
		{
			cType = 318;
		}
		else if (cType==320)
		{
			cType = 328;
		}
		else if (cType==330)
		{
			cType = 338;
		}
		else if (cType==340)
		{
			cType = 348;
		}
		

		pCreature->SetLevelName( rand()%100 );

		//int numGuildMark = g_pTopView->m_GuildSPK();
		//int guildMark = ((numGuildMark==0)? -1 : rand()%numGuildMark);
		//pCreature->SetGuildNumber( guildMark );
		
		if (pCreature->IsVampire())
		{
			pCreature->SetGuildNumber( 1 );
		}
		

		// Move 종류를 다르게 한다.
		switch (0)//rand()%2)//rand()%3)
		{
			case 0 :
				pCreature->SetGroundCreature();
			break;
		
			case 1 :
				pCreature->SetFlyingCreature();
			break;
			
			case 2 :
				pCreature->SetUndergroundCreature();
			break;
		}		

		while (1)
		{
			x = rand()%40 + 10;
			y = rand()%40 + 10;

			//x = rand()%(g_pZone->GetWidth()-1)+1;
			//y = rand()%(g_pZone->GetHeight()-1)+1;
			//x = rand()%(20-1)+10;
			//y = rand()%(20-1)+20;	// 20~40
			/*
			int p = rand()%100;

			y = 140 - createPositionY[p];
			x = 12 - ((40-createPositionY[p])>>1) + (rand()%(41-createPositionY[p]));
			
			if (rand()%2)
			{
				x += rand()%4;
			}
			else
			{
				x -= rand()%4;
			}
			*/

			if (x>=0 && x<g_pZone->GetWidth() 
				&& y>=0 && y<g_pZone->GetHeight())
			{					
				if (g_pZone->GetSector(x,y).IsEmpty())
				{
					pCreature->SetPosition( x, y );
					pCreature->SetServerPosition( x, y );
					break;
				}
			}
		}
		//pCreature->SetID(1000 + y*256 + x);
		pCreature->SetID( creatureID );

				
		// 이름 설정 - 일단은 ID로
		//if (pCreature->IsVampire())
		{
			//char str[80];
			//sprintf(str, "ID=%d, Type=%d", pCreature->GetID(), pCreature->GetCreatureType());
			pCreature->SetName( (*g_pCreatureTable)[pCreature->GetCreatureType()].Name.GetString() );

			// 함수자체가 클라이언트 전용-.-;;; 랜덤-_-;
			if ((pCreature->GetID() & 0x00000007)==7)
			{
				pCreature->SetPlayerParty();
			}
		}
		
		int direction = DIRECTION_LEFTDOWN;

		int creatureType = pCreature->GetCreatureType();
		int spriteType = (*g_pCreatureTable)[creatureType].SpriteTypes[0];
		int frameID = (*g_pCreatureSpriteTable)[spriteType].FrameID;

		if (frameID < g_pTopView->m_CreatureFPK.GetSize())
		{
			if (g_pTopView->m_CreatureFPK[frameID][0][1].GetSize()==0)
			{
				if (g_pTopView->m_CreatureFPK[frameID][0][3].GetSize()!=0)
				{
					direction = DIRECTION_RIGHTDOWN;
				}
				else if (g_pTopView->m_CreatureFPK[frameID][0][2].GetSize()!=0)
				{
					direction = DIRECTION_DOWN;
				}
			}
			else if (g_pTopView->m_CreatureFPK[frameID][0][3].GetSize()==0)
			{
				if (g_pTopView->m_CreatureFPK[frameID][0][3].GetSize()!=0)
				{
					direction = DIRECTION_RIGHTDOWN;
				}
				else if (g_pTopView->m_CreatureFPK[frameID][0][2].GetSize()!=0)
				{
					direction = DIRECTION_DOWN;
				}
			}
			else
			{
				// default
				direction = DIRECTION_LEFTDOWN;
			}
		}				

		pCreature->SetDirection(direction);//DIRECTION_UP);
		pCreature->SetCurrentDirection(direction);
		pCreature->SetNextAction( ACTION_STAND );

		if (!g_pZone->AddCreature(pCreature))
		{
			delete pCreature;
			return NULL;
		}

		// [ TEST CODE ]
		//pCreature->SetChatString( "1234567890abcdefgijklmnopqrstuvwxyz");
		
		// 임의의 frame으로 시작한다.
		//int end = rand()%20;
		//for (int a=0; a<end; a++)
		//{
		//	pCreature->Action();
		//}

		return pCreature;
	}

	return NULL;
}



//-----------------------------------------------------------------------------
// GetMakeItemFitPosition
//-----------------------------------------------------------------------------
//
// 성수만들기, 폭탄/지뢰 만들기.. 등에서 사용된다.
//
// pItem으로 인해서 생성되는 item(itemClass, itemType)이 들어갈 위치를
// fitPoint를 통해서 얻을 수 있다.
//
// return true인 경우만 fitPoint에 들어갈 수 있는 위치가 저장된다.
// return false이면 들어갈 위치가 없다는 의미이다.
//-----------------------------------------------------------------------------
bool
GetMakeItemFitPosition(MItem* pItem, ITEM_CLASS itemClass, int itemType, POINT& fitPoint)
{
	bool bFindPos = false;	// 자리가 있나?
	
	// create a temporary item to find the position it would fit into.
	MItem* pResultItem = MItem::NewItem( itemClass );

	if (pResultItem == NULL)
	{
		DEBUG_ADD_FORMAT("[Error] GetMakeItemFitPosition: invalid item class %d", itemClass);
		return false;
	}

	pResultItem->SetItemType( itemType );

	if (g_pInventory->GetFitPosition(pResultItem, fitPoint))
	{
		bFindPos = true;

		MItem* pOldItem = g_pInventory->GetItem(static_cast<BYTE>(fitPoint.x), static_cast<BYTE>(fitPoint.y));

		// 빈 곳에 추가할려고 하고..
		// 하나만 남아있다면.. 
		// 현재 위치에서 그대로 바꾼다.
		if (pOldItem==NULL
			&& pItem->GetNumber()==1)
		{
			fitPoint.x = pItem->GetGridX();
			fitPoint.y = pItem->GetGridY();

			bFindPos = true;
		}									
	}
	else
	{
		// 빈 자리가 없는 경우에
		// 하나만 남아있다면.. 그 자리에서 바꾸면 된다.
		if (pItem->GetNumber()==1)
		{
			fitPoint.x = pItem->GetGridX();
			fitPoint.y = pItem->GetGridY();

			bFindPos = true;
		}
	}

	// 임시로 만든것 제거
	delete pResultItem;

	return bFindPos;
}
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
