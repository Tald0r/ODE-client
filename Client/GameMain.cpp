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
	// Process the RequestServerPlayerManager too.
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
// Sets the client's run mode.
//-----------------------------------------------------------------------------
// Handles the game state's change and
// the UI's mode change.
//
// (!) Nowhere~ else may change the game state or the UI mode.
//-----------------------------------------------------------------------------
void		
SetMode(enum CLIENT_MODE mode)
{
	g_Mode = mode;

	// Reset the input.
	if (g_pSDLInput!=NULL)
	{
		g_pSDLInput->UpdateInput();
		g_pSDLInput->Clear();
	
		// acquire
		g_pSDLInput->SetAcquire(true);			
		
		// Reset the input.
		g_pSDLInput->Clear();
	}

	g_bLButtonDown = FALSE;
	g_bRButtonDown = FALSE;
	g_bCButtonDown = FALSE;


	switch (g_Mode)
	{
		//------------------------------------------------------
		// Some option changes. - -;
		//------------------------------------------------------
		case MODE_CHANGE_OPTION :
			{
			}
			//--------------------------------------------------
			// Stop the music.
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
			// Show "please wait"..
			//-----------------------------------------------------------------
			g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[STRING_MESSAGE_WAIT].GetString(), -1, -1, 0 );

			gC_vs_ui.Process();
			gC_vs_ui.Show();

			//-----------------------------------------------------------------
			// Copy Last to Back - only without 3D HAL..
			//-----------------------------------------------------------------
			{
				POINT point = { 0, 0 };
				RECT rect = { 0, 0, g_GameRect.right, g_GameRect.bottom };

				g_pBack->BltNoColorkey( &point, g_pLast, &rect );
			}

			CSDLGraphics::Flip();

			//-----------------------------------------------------------------
			// g_pTopView : when switching 2D <--> 3D..
			//-----------------------------------------------------------------
			if (g_pTopView!=NULL && !g_pTopView->InitChanges())
			{
				LOG_ERROR("Game view restoration failed; stopping before another frame");
				g_pUIDialog->CloseMessageDlg();
				SetMode(MODE_QUIT);
				return;
			}

			g_pUIDialog->CloseMessageDlg();

			// Straight on to MAINMENU.. - -;
			SetMode( MODE_MAINMENU );
		break;

		//------------------------------------------------------
		// Set up the login state..
		//------------------------------------------------------
		case MODE_MAINMENU :
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MAINMENU");
			
			// acquire
			g_pSDLInput->SetAcquire(g_bActiveGame==TRUE);


			//------------------------------------------------------
			// Release the socket..
			//------------------------------------------------------
			if(NULL == g_pUserInformation->pLogInClientPlayer &&
				true == g_pUserInformation->IsAutoLogIn)// a web login that has never logged in
				s_pGameMainHost->ReleaseSocket();


			//------------------------------------------------------
			// [ TEST CODE ]
			// is it all right to init the sound here??
			//------------------------------------------------------
			s_pGameMainHost->InitSound();

			g_CurrentFrame		= 0;

			g_bSetHotKey	= FALSE;

			//------------------------------------------------------
			// ui dialog
			//------------------------------------------------------
			g_pUIDialog->HidePCTalkDlg();


			//------------------------------------------------------
			// Accept chat from everyone
			//------------------------------------------------------
			g_pChatManager->ClearID();
			g_pChatManager->SetAcceptMode();

			// Start the title screen UI
			gC_vs_ui.StartTitle();	
		
			gC_vs_ui.DropItem();

			g_bUIInput = FALSE;			

			// So as not to get whispers... nyam -- ;
			if (g_pUserInformation!=NULL)
			{
				// Clear the whisper target.
				g_pUserInformation->WhisperID.Release();

				g_pUserInformation->CharacterID.Release();
			}

			//----------------------------------------------
			// Clear the messages
			//----------------------------------------------
			g_pSystemMessage->Clear();
			g_pPlayerMessage->Clear();
			g_pGameMessage->Clear();
			g_pNoticeMessage->Clear();


			//------------------------------------------------------
			// Start the music
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
			// Set the update function
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_LOGIN;	
			g_pUpdate = g_pCWaitUIUpdate;
			g_pCWaitUIUpdate->Init();
		break;

		//------------------------------------------------------
		// Wait for the information to select a PC.
		//------------------------------------------------------
		case MODE_NEWUSER :
		{
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_NEWUSER");
			
			//----------------------------------------------
			// Set the update function
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_LOGIN;	
			g_pUpdate = g_pCWaitUIUpdate;
			g_pCWaitUIUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// Wait for the login to succeed.
		//------------------------------------------------------
		// Send (ID, Password)
		// and wait for the login to succeed.
		//------------------------------------------------------
		case MODE_WAIT_LOGINOK :
		{
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_LOGINOK");
			
			//----------------------------------------------
			// Set the update function
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
			// Set the update function
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
			// Set the update function
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
			// Set the update function
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
			// Set the update function
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_LOGIN;	
			g_pUpdate = g_pCWaitUIUpdate;
			g_pCWaitUIUpdate->Init();
		}
		break;

		
		//------------------------------------------------------
		// A wrong input.
		//------------------------------------------------------
		case MODE_LOGIN_WRONG :
		{
			// Back to the main menu..

			g_Mode = MODE_MAINMENU;

			//-----------------------
			// Release the socket..
			//-----------------------
			s_pGameMainHost->ReleaseSocket();

			//----------------------------------------------
			// Set the update function
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_LOGIN;	
			g_pUpdate = g_pCWaitUIUpdate;
			g_pCWaitUIUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// Wait for the registration.
		//------------------------------------------------------		
		case MODE_WAIT_REGISTERPLAYEROK :
		{
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_REGISTERPLAYEROK");
			
			//----------------------------------------------
			// Set the update function
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_WAITING;			
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );
			g_pCWaitPacketUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// Wait for the information to select a PC.
		//------------------------------------------------------
		case MODE_WAIT_PCLIST :
		{			
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_PCLIST");				
			
			//------------------------------------------------------------
			// The character selection window has to be shown.
			//------------------------------------------------------------
			// LCPCListHandler does that now.

			g_ZoneAmbientSounds.SetNextSoundTime(g_FrameNow);

			//----------------------------------------------
			// Set the update function
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_WAITING;		
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );
			g_pCWaitPacketUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// Wait for the information to select a PC.
		//------------------------------------------------------
		case MODE_WAIT_SELECTPC :
		{
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_SELECTPC");
			
			//----------------------------------------------
			// Reset the player's information
			//----------------------------------------------
			g_pUserInformation->KeepConnection = FALSE;
			

			SelectLastSelectedCharacter();

			//----------------------------------------------
			// Set the update function
			//----------------------------------------------
			g_MorphCreatureType = 0;
			g_UpdateDelay = DELAY_UPDATE_LOGIN;	
			g_pUpdate = g_pCWaitUIUpdate;
			g_pCWaitUIUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// Was the PC created properly?
		//------------------------------------------------------
		case MODE_WAIT_CREATEPCOK :
		{
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_CREATEPCOK");
			
			//----------------------------------------------
			// Set the update function
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_WAITING;		
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );
			g_pCWaitPacketUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// Was the PC created properly?
		//------------------------------------------------------
		case MODE_WAIT_DELETEPCOK :
		{
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_DELETEPCOK");
			
			//----------------------------------------------
			// Set the update function
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_WAITING;		
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );
			g_pCWaitPacketUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// To connect to the game server...
		//------------------------------------------------------
		case MODE_WAIT_RECONNECT :
		{				
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_RECONNECT");
			
			//----------------------------------------------
			// Set the update function
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_WAITING;		
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );
			g_pCWaitPacketUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// To reconnect from the game server to the login server...
		//------------------------------------------------------
		case MODE_WAIT_RECONNECT_LOGIN :
		{				
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_RECONNECT_LOGIN");

			// So as not to get whispers... nyam -- ;
			if (g_pUserInformation!=NULL)
			{
				g_pUserInformation->CharacterID.Release();
			}
			
			//----------------------------------------------
			// Set the update function
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_WAITING;		
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );
			g_pCWaitPacketUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// Receive UpdateInfo.
		//------------------------------------------------------
		case MODE_WAIT_UPDATEINFO :
		{	
			//--------------------------------------------------
			// Reset the player
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
			// Reset self-defense
			//--------------------------------------------------
			g_pJusticeAttackManager->Release();

			//--------------------------------------------------
			//
			// Init the skill tree - for now.. - -;;
			//
			//--------------------------------------------------
			ZoneLoading::InitSkillTree();

			//------------------------------
			// Start the game UI.
			//------------------------------
			UI_SetCharInfoName(g_pUserInformation->CharacterID);
			DEBUG_ADD("[ SetMode ] SET CHARINFO");
			UI_StartGame();

			DEBUG_ADD("[ SetMode ] UI FINISH");

			g_ZoneAmbientSounds.SetNextSoundTime(g_FrameNow);

			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_UPDATEINFO");
			
			//----------------------------------------------
			// Set the update function
			//----------------------------------------------
			g_UpdateDelay = DELAY_UPDATE_WAITING;		
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );
			g_pCWaitPacketUpdate->Init();			
		}
		break;

		//------------------------------------------------------
		// Wait to receive the player's position.
		//------------------------------------------------------
		case MODE_WAIT_SETPOSITION :
		{
			// Debug Message
			DEBUG_ADD("[ SetMode ]  MODE_WAIT_SETPOSITION");
			
			//--------------------------------------------------
			// Save the options
			//--------------------------------------------------
			if (g_pUserOption!=NULL)
			{
				g_pUserOption->SaveToFile( g_pFileDef->getProperty("FILE_INFO_USEROPTION").c_str());
			}
			
			// Stop..
			g_pPlayer->SetStop();
			g_pPlayer->SetAction( ACTION_STAND );

			// Drop the key input
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
			// Send the CGReady packet to the game server.
			//--------------------------------------------------
				CGReady cgReady;
				g_pSocket->sendPacket( &cgReady );
				g_pSocket->setPlayerStatus( CPS_WAITING_FOR_GC_SET_POSITION );

				// Send it right away.
				UpdateSocketOutput();

			g_SelectSector.x = 0;
			g_SelectSector.y = 0;


			if (g_pTopView!=NULL)
			{
				g_pTopView->ClearShadowManager();
						
				g_pTopView->SetSelectedNULL();
			}
		
			// The update function
			g_UpdateDelay = DELAY_UPDATE_WAITING;			
			g_pUpdate = g_pCWaitPacketUpdate;
			g_pCWaitPacketUpdate->SetDelay( g_pClientConfig->MAX_WAIT_PACKET );
			g_pCWaitPacketUpdate->Init();
		}
		break;

		//------------------------------------------------------
		// Start the game.
		//------------------------------------------------------
		case MODE_GAME :
			DEBUG_ADD("---------- Start Game ---------- ");

			DEBUG_ADD("CSDLGraphics::RestoreAllSurfaces()");
			CSDLGraphics::RestoreAllSurfaces();

			DEBUG_ADD("CDirect3D::Restore() - removed (SDL2)");

			DEBUG_ADD("TempInformation");

			//-----------------------------------------------------------
			// Clear the temp information
			//-----------------------------------------------------------
			g_pTempInformation->SetMode(TempInformation::MODE_NULL);

			DEBUG_ADD("UserInformation");
			//-----------------------------------------------------------
			// cancel the logout
			//-----------------------------------------------------------
			g_pUserInformation->CancelLogout();
		
			//-----------------------------------------------------------
			// Save the hot keys.
			//-----------------------------------------------------------
			DEBUG_ADD("SaveHotKey");
			UI_SaveHotKeyToServer();			

			//-----------------------------------------
			// Reset the buttons
			//-----------------------------------------
			g_bLButtonDown = FALSE;
			g_bRButtonDown = FALSE;
			g_bCButtonDown = FALSE;

			//-----------------------------------------
			// Unlock the input the UI dialogs locked..
			//-----------------------------------------
			DEBUG_ADD("UIDialog");
			UIDialog::UnSetLockInputPCTalk();
			UIDialog::UnSetLockInputMessage();

			// Remove the resurrect button
			// This belongs in GCUpdateInfoHandler.. for now. - -;
			DEBUG_ADD("FinishRequestDie");
			gC_vs_ui.FinishRequestDie();
			gC_vs_ui.FinishRequestResurrect();
			gC_vs_ui.CloseRequestShrineMinimap();


//Edit by sonic 2006.7.27  changed the speed-up address
			g_UpdateDelay = 100 ^ 90;
			
			DEBUG_ADD("SetUpdate");
g_pUpdate = g_pCGameUpdate;
		g_pCGameUpdate->Init();

		// Ensure input is enabled after start game
		CheckActivate(TRUE);

		ExecuteHelpEvent( HELP_EVENT_INTERFACE );

		break;

		
		//------------------------------------------------------
		// End the program.
		//------------------------------------------------------
		case MODE_QUIT :
			// Send the logout.
				if (g_pSocket!=NULL)
				{
					//--------------------------------------------------
					// Quitting to update sends CLLogout.
					//--------------------------------------------------
					if (g_bNeedUpdate)
					{
						//--------------------------------------------------
						// Send the CLLogout packet to the login server.
						//--------------------------------------------------
							CLLogout clLogout;
						
							g_pSocket->sendPacket( &clLogout );

					}
					//--------------------------------------------------
					// Quitting during the game.. CG Logout
					//--------------------------------------------------
					else
					{
						CGLogout cgLogout;
					
						g_pSocket->sendPacket( &cgLogout );

					}
				}

			//--------------------------------------------------
			// Stop the thread loading..
			//--------------------------------------------------
			s_pGameMainHost->StopLoadingThread();

			// Set the update function
			g_pUpdate = NULL;

			g_bUIInput = FALSE;

			// Close the window
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

	// Reset the input.
	g_pSDLInput->Clear();
}

//-----------------------------------------------------------------------------
// Check Activate
//-----------------------------------------------------------------------------
// Handles whether the program is active or not..
// Much to do with ALT + TAB. - -;
//-----------------------------------------------------------------------------
void
CheckActivate(BOOL bActiveGame)
{

	if (g_pSDLInput!=NULL)
	{
		// acquire
		g_pSDLInput->SetAcquire(bActiveGame==TRUE);			
		
		// Reset the input.
		g_pSDLInput->Clear();
	}
	
	//----------------------------------------------------
	// The program runs..
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
			// Set the gamma again
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
				// If playing.. stop..
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
				// When not playing
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
			// Clear the mouse buttons' pressed state
			//--------------------------------
			g_bLButtonDown = FALSE;
			g_bRButtonDown = FALSE;
			g_bCButtonDown = FALSE;

			if (g_pPlayer!=NULL)
			{
				g_pPlayer->UnSetLockMode();
			}

			
			// alt+tab in the UI
			DEBUG_ADD("UI_RestoreWhenActivateGame");

			gC_vs_ui.RestoreWhenActivateGame();

			DEBUG_ADD("UI_Restore_ok");
		}
		//----------------------------------------------------
		// Stopped..
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
			
			// Stop the repeated action
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
// Loads the sprites of the n-th creature type ID.
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
// Removes the game's objects.
//-----------------------------------------------------------------------------
void
ReleaseGameObject()
{
	//------------------------------------------------------
	// Remove the weather
	//------------------------------------------------------
	g_pWeather->Release();

	g_pParty->Release();

	g_pEventManager->RemoveAllEvent();
	g_pWarManager->ClearWar();
	g_pTimeItemManager->clear();

	//------------------------------------------------------
	// Remove the player from the zone / remove the zone
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
	// Remove the player
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
// Loads zone n.
//-----------------------------------------------------------------------------
BOOL
LoadZone(int n)
{	
	//------------------------------------------------
	// Handling by the zone's kind (size)
	//------------------------------------------------
	// 
	// It has to load into the zone of the same kind (size).
	//
	//------------------------------------------------
	ZONETABLE_INFO* pZoneInfo = g_pZoneTable->Get( n );

	if (pZoneInfo==NULL)
	{
		DEBUG_ADD_FORMAT_ERR("[Error] Wrong Zone ID=%d", n);
		
		return FALSE;
	}

	//------------------------------------------------
	// Stop every sound.
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
	// Stop the music...
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
	// Drop everything being loaded
	//-------------------------------------------------------------
	s_pGameMainHost->StopLoadingThread();

	// Lower the priority as far as it goes.
	// Thread priority is platform-specific - currently implemented for Windows only
#ifdef PLATFORM_WINDOWS
	s_pGameMainHost->SetLoadingThreadPriority( THREAD_PRIORITY_IDLE );
#endif // PLATFORM_WINDOWS

	//----------------------------------------------------------------------
	// Remove the current zone's objects
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
		// If we were in a large zone before..
		// make the shadows again
		//-------------------------------------------
		if (g_bZonePlayerInLarge)
		{
			g_pTopView->ClearShadowManager();
		}
		
		//-------------------------------------------
		// No need to load it again
		//-------------------------------------------
		if (g_nZoneSmall==n && g_pZoneSmall!=NULL)
		{
			DEBUG_ADD("ReleaseObject in SmallZone");
			
			// Remove the objects
			g_pZoneSmall->ReleaseObject();

			DEBUG_ADD("OK");
			

			// Take the small zone's pointer
			g_pZone	= g_pZoneSmall;

			UI_DrawProgress( 95 );
		}
		//-------------------------------------------
		// It has to be loaded again
		//-------------------------------------------
		else
		{
			// Remember the kind (size) of zone to load
			g_nZoneSmall = n;	

			DEBUG_ADD("[Load Zone] Release Old Tile&ImageObject SPK");
			
			// Delete the previous zone's sprites.
			g_pTopView->ReleaseTileSPKSmallZone();		
			g_pTopView->ReleaseImageObjectSPKSmallZone();	
			
			// Reset the small zone.
			if (g_pZoneSmall != NULL)
			{
				DEBUG_ADD( "Delete old ZoneSmall");
				
				delete g_pZoneSmall;
			}				

			DEBUG_ADD( "New ZoneSmall");
			
			g_pZoneSmall = new MZone;

			// Take the small zone's pointer
			g_pZone	= g_pZoneSmall;

			// For the teen version, load the teen map file
			MString filename = pZoneInfo->Filename;
			if(g_pUserInformation->GoreLevel == false)
			{
				if(pZoneInfo->TeenFilename.GetLength() > 0)
					filename = pZoneInfo->TeenFilename;
			}

			std::ifstream file;
			if (!FileOpenBinary(filename, file))
			{
				// Priority back to normal
				s_pGameMainHost->SetLoadingThreadPriority( THREAD_PRIORITY_LOWEST );

				return FALSE; 
			}

			DEBUG_ADD("[Load Zone] Load Zone Data");
			
			UI_DrawProgress( 80 );

			// Zone loading failed?
			if (!g_pZone->LoadFromFile( file ))
			{
				SetMode( MODE_QUIT );

				// Priority back to normal
				s_pGameMainHost->SetLoadingThreadPriority( THREAD_PRIORITY_LOWEST );
				return FALSE;
			}

			g_pZone->SetCurrentZoneID( n );
		
			file.close();			
			
			
			
			UI_DrawProgress( 90 );

			//-------------------------------------------------------
			// Load the sprites near the player's position.
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
		
		// For the teen version, load the teen map file
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
		// If we were in a small zone before..
		// make the shadows again
		//-------------------------------------------
		if (!g_bZonePlayerInLarge)
		{
			g_pTopView->ClearShadowManager();
		}

		//-----------------------------------------------
		// No need to load it again
		//-----------------------------------------------
		if (g_nZoneLarge==n && g_pZoneLarge!=NULL)
		{
			DEBUG_ADD("ReleaseObject in LargeZone");
			
			// Remove the objects
			g_pZoneLarge->ReleaseObject();
			
			DEBUG_ADD("OK");
			

			// Take the large zone's pointer
			g_pZone	= g_pZoneLarge;

			UI_DrawProgress( 95 );
		}
		//-----------------------------------------------
		// It has to be loaded again
		//-----------------------------------------------
		else
		{
			// Remember the kind (size) of zone to load
			g_nZoneLarge = n;	

			DEBUG_ADD("[Load Zone] Release Old Tile&ImageObject SPK");
			
			// Delete the previous zone's sprites.
			g_pTopView->ReleaseTileSPKLargeZone();		
			g_pTopView->ReleaseImageObjectSPKLargeZone();	
			
			// Reset the large zone.
			if (g_pZoneLarge != NULL)
			{
				DEBUG_ADD( "Delete old ZoneLarge");
				
				delete g_pZoneLarge;
			}			
			
			DEBUG_ADD( "New ZoneLarge");
			
			g_pZoneLarge = new MZone;

			// Take the large zone's pointer
			g_pZone	= g_pZoneLarge;			
			
			// For the teen version, load the teen map file
			MString filename = pZoneInfo->Filename;
			if(g_pUserInformation->GoreLevel == false)
			{
				if(pZoneInfo->TeenFilename.GetLength() > 0)
					filename = pZoneInfo->TeenFilename;
			}
			
			std::ifstream file;
			if (!FileOpenBinary(filename, file))
			{
				// Priority back to normal
				s_pGameMainHost->SetLoadingThreadPriority( THREAD_PRIORITY_LOWEST );

				return FALSE; 
			}

			DEBUG_ADD("[Load Zone] Load Zone Data");
			
			UI_DrawProgress( 80 );

			// Zone loading failed?
			if (!g_pZone->LoadFromFile( file ))
			{
				SetMode( MODE_QUIT );

				// Priority back to normal
				s_pGameMainHost->SetLoadingThreadPriority( THREAD_PRIORITY_LOWEST );

				return FALSE;
			}

			file.close();

			UI_DrawProgress( 90 );
			
			//-------------------------------------------------------
			// Load the sprites near the player's position.
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
	// Set the zone in the UI..
	//------------------------------------------------
	SIZE zoneSize = { g_pZone->GetWidth(), g_pZone->GetHeight() };

	gC_vs_ui.SetSize( zoneSize );
	gC_vs_ui.SetZone( n );


	LoadZoneInfo( n );
	g_pZone->SetCurrentZoneID( n );

	// 2004, 6, 22 sobeit add start - for the Gilles de Rais core
	Add_GDR_Ghost(n);
	Add_GDR_Effect(1,false);
	Add_GDR_Effect(2,false);
	Add_GDR_Potal_Effect(n);
	// 2005, 1, 18, sobeit add start - for the quest - since bDisableTileImage==false almost everywhere..
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
	// 2004, 9, 21, sobeit add start - the north Adam cloud
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
	// Set the random sound ID's time
	//------------------------------------------------
	g_ZoneAmbientSounds.DelayAfterZoneLoad(g_FrameNow, static_cast<unsigned>(rand()));
	
	//------------------------------------------------
	// Tell the view which zone to draw.
	//------------------------------------------------
	// This drops the creatures drawn before.
	g_pTopView->SetZone(g_pZone);	

	// Debug Message
	DEBUG_ADD_FORMAT("LoadZone OK : size=(%d, %d)", g_pZone->GetWidth(), g_pZone->GetHeight());			
		

	// Priority back to normal
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
// Moves to another zone.
//-----------------------------------------------------------------------------
void
MoveZone(int n)
{
	//------------------------------------------------
	// Take the player out of the zone.
	//------------------------------------------------
	if (g_pZone != NULL)
	{
		g_pPlayer->SetStop();
		g_pPlayer->SetAction( ACTION_STAND );
		g_pZone->RemovePlayer();
	}
	
	// Load zone n.
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
	// Load only when the file exists.
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
			// Load the portal information.
			// Better done inside... but for now..
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
				// Set it in the UI.
				//------------------------------------------------
				if (numZoneID==1)
				{					
					DEBUG_ADD("UI SetPortal");

					gC_vs_ui.SetPortal( rect, zoneID[0] );
				}
				else
				{
					DEBUG_ADD("UI SetPortal - 60001");

					// A multi-portal.. heh.. _-_;
					gC_vs_ui.SetPortal( rect, 60001 );
				}
			}

			DEBUG_ADD("PortalInfo OK");

			//------------------------------------------------
			// The safe zones
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

					// Am I safe here?

					if((MyRace == RACE_OUSTERS && ( rect2.flag & FLAG_MIP_SAFE_OUSTERS )) ||
						(MyRace == RACE_VAMPIRE && ( rect2.flag & FLAG_MIP_SAFE_VAMPIRE)) ||
						(MyRace == RACE_SLAYER && (rect2.flag & FLAG_MIP_SAFE_SLAYER)) ||
						(rect2.flag & FLAG_MIP_SAFE_COMMON) )
					{
						gC_vs_ui.SetSafetyZone( safeRect, true );						
					}
					// Is another race safe here?
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
			// Plant the ousters' horns on the map
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
						// Edit By Sonic 2006.10.1  fix the position of the magic spirit pillar
						//old position in 62  x=39 y=30, fixed position x=7 y=11
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
	// Keep it alive. To check...
	//------------------------------------------------
	g_pTopView->SetFadeEnd();

	//------------------------------------------------
	// Set the position
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

	// The standing action
	if (g_pPlayer->IsAlive())
	{
		g_pPlayer->SetAction( ACTION_STAND );
	}
	else
	{
		g_pPlayer->SetAction( ACTION_DIE );
	}

	//-------------------------------------------------------
	// Clear the count of move packets sent to the server
	// and move again from the start~~.
	//-------------------------------------------------------
	g_pPlayer->ResetSendMove();

	//-------------------------------------------------------
	// The skill verification state..
	//-------------------------------------------------------
	g_pPlayer->SetWaitVerifyNULL();

	//-------------------------------------------------------
	//
	//						Items
	//
	//-------------------------------------------------------
	//------------------------------------------------
	// The ItemCheckBuffer state
	//------------------------------------------------
	g_pPlayer->ClearItemCheckBuffer();
	
	//------------------------------------------------
	// What the info window shows..
	//------------------------------------------------
	g_char_slot_ingame.Race = g_pPlayer->GetRace();
	g_char_slot_ingame.bl_female = !g_pPlayer->IsMale();

	return TRUE;
}

//-----------------------------------------------------------------------------
// Make ScreenShot
//-----------------------------------------------------------------------------
// Saves a screenshot of the current screen.
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
	// Only MAX_SCREENSHOT screen captures are possible.
	for (; g_ScreenShotNumber<maxScreenShot; g_ScreenShotNumber++)
	{
		snprintf(str, sizeof(str), "%s%03d.jpg", g_pFileDef->getProperty("PATH_SCREENSHOT").c_str(), g_ScreenShotNumber);

#ifdef PLATFORM_WINDOWS
		int fd = -1;
		_sopen_s( &fd, str, _O_RDONLY, _SH_DENYNO, 0 );
#else
		int fd = open( str, O_RDONLY );
#endif // PLATFORM_WINDOWS

		// To save where there is no file yet..
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
// Play the filename (*g_pSoundTable) has for soundID.
// If (*g_pSoundManager) has it, just play it..
// if not, load the WAV and play it.
//-----------------------------------------------------------------------------
void	
PlaySound(TYPE_SOUNDID soundID, bool repeat, int x, int y)
{
	if(g_pEventManager->GetEventByFlag(EVENTFLAG_NOT_PLAY_SOUND))
		return;
	
	__BEGIN_PROFILE("PlaySound1")


	//-----------------------------------------------------------
	// An undefined sound ID..
	//-----------------------------------------------------------
	if (!g_SDLAudio.IsInit() || soundID >= (*g_pSoundTable).GetSize()
		|| !g_pUserOption->PlaySound)
	{
		__END_PROFILE("PlaySound1")
		return;
	}

	//-----------------------------------------------------------
	// Limit on the sounds per second
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


	// Beyond some distance there should be no sound.
	if (dist < 25)
	{			
		//-----------------------------------------------------------
		// Not there --> Load & Play
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
			// Loading failed
			//-----------------------------------------------------------
			if (pBuffer==NULL)
			{
				DEBUG_ADD_FORMAT_ERR("[Error] Failed to Load WAV. id=%d, fn=%s", soundID, strFilename );
			}
			else
			//-----------------------------------------------------------
			// If the load succeeded...
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

				
				// Hard-coded: the motorcycle a bit quieter. - -;
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
		// There --> Play
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

				// Hard-coded: the motorcycle a bit quieter. - -;
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
// The sound is always at the player's position..
// CenterPan & MaxVolume
//-----------------------------------------------------------------------------
void	
PlaySoundAtPlayer(TYPE_SOUNDID soundID)
{
	if(g_pEventManager->GetEventByFlag(EVENTFLAG_NOT_PLAY_SOUND))
		return;

	__BEGIN_PROFILE("PlaySound2")

	//-----------------------------------------------------------
	// An undefined sound ID..
	//-----------------------------------------------------------
	if (!g_SDLAudio.IsInit() || soundID >= (*g_pSoundTable).GetSize()
		|| !g_pUserOption->PlaySound)
	{
		__END_PROFILE("PlaySound2")
		return;
	}

	//-----------------------------------------------------------
	// Limit on the sounds per second
	//-----------------------------------------------------------
	// The player's sounds need no limit.

	//-----------------------------------------------------------
	// Not there --> Load & Play
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
		// Loading failed
		//-----------------------------------------------------------
		if (pBuffer==NULL)
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Failed to Load WAV. id=%d, fn=%s", soundID, strFilename );
		}
		//-----------------------------------------------------------
		// If the load succeeded...
		//-----------------------------------------------------------
		else
		{
			// If it replaced one, free the old one.
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
	// There --> Play
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
	// An undefined sound ID..
	//-----------------------------------------------------------
	if (!g_SDLAudio.IsInit() || soundID >= (*g_pSoundTable).GetSize()
		)
	{
		__END_PROFILE("PlaySound2")
		return;
	}

	//-----------------------------------------------------------
	// Limit on the sounds per second
	//-----------------------------------------------------------
	// The player's sounds need no limit.

	//-----------------------------------------------------------
	// Not there --> Load & Play
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
		// Loading failed
		//-----------------------------------------------------------
		if (pBuffer==NULL)
		{
			DEBUG_ADD_FORMAT_ERR("[Error] Failed to Load WAV. id=%d, fn=%s", soundID, strFilename );
		}
		//-----------------------------------------------------------
		// If the load succeeded...
		//-----------------------------------------------------------
		else
		{
			// If it replaced one, free the old one.
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
	// There --> Play
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
	// An undefined sound ID..
	//-----------------------------------------------------------
	if (!g_SDLAudio.IsInit() || soundID == SOUNDID_NULL)
		return;

	//-----------------------------------------------------------
	// An undefined sound ID..
	//-----------------------------------------------------------
	if (soundID == SOUNDID_NULL)
		return;

	//-----------------------------------------------------------
	// Not there --> just return
	//-----------------------------------------------------------
	if (soundID < g_pSoundManager->GetMaxIndex())
	{
		if ((*g_pSoundManager).IsDataNULL(soundID))
		{
			return;
		}
		//-----------------------------------------------------------
		// There --> Stop
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
// Changes the weather.
//---------------------------------------------------------------------------
void		
PlayMusicCurrentZone()
{
	// Play by the time
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
							// 2004, 11, 8, sobeit add start - just an error check
							if(SOUND_ERR_OK == g_pOGG->streamLoad( g_oggfile, NULL ))
							{
								if(SOUND_ERR_OK == g_pOGG->streamPlay( SOUND_PLAY_REPEAT ))
								{
									int volume = (g_pUserOption->VolumeMusic - 15) * 250;
									g_pOGG->streamVolume( max( -10000, min( -1, volume ) ) );
								}
							}
							// 2004, 11, 8, sobeit add end - just an error check
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
// Changes the weather.
//---------------------------------------------------------------------------
void		
SetWeather(int weather, int level)
{
	if (!s_PreviousWeather) s_PreviousWeather = weather;
	int& previousWeather = *s_PreviousWeather;

	//---------------------------------------------
	// Drop the previous weather's sound.
	//---------------------------------------------
	StopSound( g_previousSoundID );
	

	DEBUG_ADD_FORMAT("[Set Weather] %d, %d", weather, level);
	
	//---------------------------------------------
	// By the kind of weather...
	//---------------------------------------------
	if(previousWeather == WEATHER_SPOT)
		g_pWeather->Release();
	
	switch (weather)
	{
		//------------------------------
		// Clear weather
		//------------------------------
		case WEATHER_CLEAR :	
			// If it was raining before...
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
		// Rain
		//------------------------------
		case WEATHER_RAINY :
			// If it was clear before...
			if (previousWeather!=WEATHER_RAINY)
			{
				if (g_pTopView!=NULL && g_pTopView->IsInit())
				{
					g_pTopView->ClearShadowManager();
				}
			}

			g_pWeather->SetRain( level<<3 );

			// 2 is the sound of heavier rain
			g_previousSoundID = (level>=15)? SOUND_WORLD_WEATHER_RAIN_2 : SOUND_WORLD_WEATHER_RAIN_1; 
			
			// Play the rain sound repeatedly...
			PlaySound( g_previousSoundID , 
						true, 
						g_pPlayer->GetX(), g_pPlayer->GetY() );
		break;

		//------------------------------
		// Snow
		//------------------------------
		case WEATHER_SNOWY  :
			// If it was raining before...
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
			// If it was raining before...
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
	// Show the lightning..
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
	// Get the cursor's position.
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
	// Keep the mouse cursor inside.
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
	// Remove the socket
	//--------------------------------------------------
	s_pGameMainHost->ReleaseSocket();

	//--------------------------------------------------
	// Stop the sounds
	//--------------------------------------------------
	StopSound( g_previousSoundID );
	StopSound( SOUND_WORLD_PROPELLER );

	//--------------------------------------------------
	// Stop the music.
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
	// Stop the thread loading..
	//--------------------------------------------------
	s_pGameMainHost->StopLoadingThread();

	DEBUG_ADD("UpdateDisconnected : Stop Thread OK");
	
	//--------------------------------------------------
	// Remove the game objects
	//--------------------------------------------------
	ReleaseGameObject();

	DEBUG_ADD("UpdateDisconnected : Release GameObject OK");
	
	//------------------------------------------------------
	// Close all the dialogs.
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
	// The "disconnected" dialog
	//--------------------------------------------------
	// Desperate~ temporary code..
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
	// Show the disconnection once first.
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
		// Copy Last to Back - only without 3D HAL..
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
	// When not in ALT + TAB
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
			// The input to the UI should be blocked...
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
				// Copy Last to Back - only without 3D HAL..
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

		// 2004, 07, 19 sobeit add start - "cannot connect to the server": quit the game when OK is pressed
		if(true == g_pUserInformation->IsAutoLogIn) // only in web auto-login mode
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
			// Copy Last to Back - only without 3D HAL..
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
// Select the character selected last time
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
// Save the character selected now
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
			// No worldID: create and add it
			pPCTable = new PlayerConfigTable;

			g_pWorldPlayerConfigTable->AddPlayerConfigTable( worldID, pPCTable );
		}
		
		PlayerConfig* pConfig = pPCTable->GetPlayerConfig( playerID );

		if (pConfig==NULL)
		{
			// No playerID: create and add it
			pConfig = new PlayerConfig;
			if( playerID != NULL && strlen( playerID ) < 15 )
				pConfig->SetPlayerID( playerID );
			else
				pConfig->SetPlayerID( "DarkEden" );

			pPCTable->AddPlayerConfig( pConfig );
		}		

		// Always save..
		{
			pConfig->SetLastSlot( slot );		

			g_pWorldPlayerConfigTable->SaveToFile( g_pFileDef->getProperty("FILE_INFO_PLAYER_INFO").c_str());
		}
	}
}


// GetMakeItemFitPosition: ClientFunction.cpp (gamemodel, task 4.237).

// 2004, 03, 29 sobeit add start - add the ghosts on the Gilles de Rais map
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
