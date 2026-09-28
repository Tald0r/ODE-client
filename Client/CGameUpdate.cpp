//-----------------------------------------------------------------------------
// CGameUpdate.cpp
//-----------------------------------------------------------------------------
// 실제 게임을 진행하는 부분
//-----------------------------------------------------------------------------
#include "Client_PCH.h"
#include "CrtCompat.h"

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif


//-----------------------------------------------------------------------------
// Include files
//-----------------------------------------------------------------------------
#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include "../basic/Platform.h"
#endif
#include <string>
#include "TextSystem/TextService.h"
#include "TextSystem/RenderTargetSpriteSurface.h"
#include "Client.h"
#include "MonotonicClock.h"
#include "PacketDispatcher.h"
#include "GameObject.h"
#include "UserInformation.h"
#include "ServerInfo.h"
#include "PacketDef.h"
#include "VS_UI.h"
#include "VS_UI_mouse_pointer.h"
#include "UIDialog.h"
#include "DebugInfo.h"
#include "CGameUpdate.h"
#include "DXLibBackend.h"  // For SDL text input functions
#include "MMoneyManager.h"
#include "MGameStringTable.h"
#include "SafeFormat.h"
#include "MObjectSelector.h"
#include "ClientFunction.h"
#include "MMusic.h"
#include "InputService.h"  // For DIK_* key code definitions
#include "DXLib/CDirectInput.h"
#include "MZoneSoundManager.h"
#include "TempInformation.h"
#include "MFakeCreature.h"
#include "MParty.h"
#include "Profiler.h"
#include "MTestDef.h"
#include "MEventManager.h"
#include "UIFunction.h"
#include "CMP3.h"
#include "COGGSTREAM.h"
#include "SystemAvailabilities.h"
#include "MWarManager.h"
// EXECryptor and ThemidaSDK includes removed (SDL2) - Copy protection no longer needed
#include "Packet/Cpackets/CGVerifyTime.h"

#ifdef OUTPUT_DEBUG
	#include "Packet/Gpackets/GCSkillFailed2.h"
	#include "Packet/Gpackets/GCSkillToObjectOK5.h"
	#include "Packet/Gpackets/GCSkillToSelfOK2.h"
	#include "Packet/Gpackets/GCSkillToTileOK2.h"
	#include "Packet/Cpackets/CGTypeStringList.h"
#endif

//add by viva
#include "Packet/Gpackets/GCFriendChatting.h"

#ifdef OUTPUT_DEBUG
	bool g_bSlideScreenShot = false;
	bool g_bSaveSlideScreenShot = false;
	bool g_bSlideRectSelect = false;
	RECT g_rectScreenShot = { 0, 0, 0, 0 };

	int gtx_op = 4;
	int gtx_src = 2;
	int gtx_dest = 2;
#endif

// Global
CGameUpdate*		g_pCGameUpdate = NULL;

bool g_bPreviousMove = false;
	
extern DWORD g_double_click_time;

COGGSTREAM*					g_pOGG = NULL;
CDirectSoundBuffer*			g_pSoundBufferForOGG = NULL;
FILE	*					g_oggfile = NULL;

extern int g_SoundPerSecond;

extern int	g_previousSoundID;
extern bool g_bMouseInPortal;
		
extern POINT g_MouseSector;	
extern bool	g_bTestMode;

extern bool g_bZoneSafe;

// 할할...
extern void	SetWatchMode( bool );
extern bool g_bWatchMode;

int g_MyCheckTime=0;
int g_CheckErrorTime=0;
extern BOOL g_MyFull;
extern RECT g_GameRect;
//add by sonic 2006.7.30
//检测系统登陆的用户
#ifdef PLATFORM_WINDOWS
	#include <tlhelp32.h>
	// ToolHelp32 functions (CreateToolhelp32Snapshot, etc.) live in kernel32.lib
	// on modern Windows SDKs; "th32.lib" doesn't exist and this pragma would
	// fail the link (LNK1104) on VS2019+.
	extern int GetCurrentUserNumber();
	int g_CheckTimeNum=0;
#else
	// Non-Windows platforms don't have tlhelp32.h
	// Forward declaration only - implementation is below
	extern int GetCurrentUserNumber();
	int g_CheckTimeNum=0;
#endif
//end
//extern bool CheckInvalidProcess();
extern void	KeepConnection();
extern BOOL	InitInfomation();
extern void	ExecuteLogout();

extern char	g_CWD[_MAX_PATH];

#define	MOUSE_POINTER_SKIP_LEFT		-16
#define	MOUSE_POINTER_SKIP_UP		-16

//#define __FIX_GUNFRAME__

#ifdef __FIX_GUNFRAME__
	extern int g_tempX;
	extern int g_tempY;
#endif

// update loop에 대한 debug message를 출력할까 말까?
#if defined(OUTPUT_DEBUG) //&& defined(_DEBUG)
	#ifdef __METROTECH_TEST__
//		#define	OUTPUT_DEBUG_UPDATE_LOOP	
//		#define OUTPUT_DEBUG_PROCESS_INPUT

		extern int  g_iAutoHealPotion;
		extern bool g_bAutoManaPotion;
		extern int  g_iAutoReload;
		extern int	g_iSpeed;
		extern bool g_bLight;
	#endif
#endif

#ifdef OUTPUT_DEBUG
	extern int g_HISTORY_LINE;
#endif

bool	g_bFrameChanged	 = false;

//----------------------------------------------------------------------
// Draw-phase interpolation (60 fps rendering between 62 ms logic ticks)
//----------------------------------------------------------------------
// The game simulation is hard-locked to one tick per 62 ms (g_UpdateDelay,
// server-synced and anti-cheat-guarded - do not touch the tick). To render
// more than 16 fps, UpdateDraw now runs between ticks as well and
// MCreature::GetPixelX/Y interpolate positions by g_DrawAlphaNum, the
// elapsed fraction of the current tick in 0..256 fixed point.
// g_bInterpolateDraw scopes that behaviour to the world draw only, so all
// game logic keeps reading exact tick positions.
DWORD	g_DrawAlphaNum = 256;
bool	g_bInterpolateDraw = false;

// The game frame is letterbox-scaled into the window by
// spritectl_present_surface, so every Win32 cursor position must be mapped
// from window-client space back to game space before the UI consumes it.
// The camera-interpolation bias that was passed into the last MTopView::Draw
// (the camGap values in UpdateDraw). Picking code (GetSelectedObject and
// friends, creature screen rects) adds it back so that everything the per-tick
// input logic hit-tests lives in exact tick space, matching pre-interpolation
// behaviour - only the *drawing* is interpolated. Without this, a tick whose
// pick lands between the interpolated and exact positions misses the target,
// and one missed pick drops the held-button attack repeat
// (ProcessInputRButtonDown: pObject == NULL -> UnSetRepeatAction).
int		g_DrawCamGapX = 0;
int		g_DrawCamGapY = 0;

extern "C" void spritectl_window_to_game_coords(int* x, int* y);
static void WindowToGamePoint(POINT& pt)
{
	int mouseX = (int)pt.x;
	int mouseY = (int)pt.y;
	spritectl_window_to_game_coords(&mouseX, &mouseY);
	pt.x = mouseX;
	pt.y = mouseY;
}
bool	g_bTestMusic = false;

#ifdef OUTPUT_DEBUG
std::string g_musicfilename[8]=
{
	"data\\music\\blood.mp3",
	"data\\music\\chaos.mp3",
	"data\\music\\dominator of darkness.mp3",
	"data\\music\\oblivion.mp3",
	"data\\music\\rest.mp3",
	"data\\music\\ruin.mp3",
	"data\\music\\silence of battlefield.mp3",
	"data\\music\\underworld.mp3"
};

int			g_CurrentMusicNum = 0;
#endif

extern MCreature* AddClientCreature();

void	PacketTest();
bool IsExistCorpseFromPlayer(MCreature* OriginCreature, int creature_type);
bool			HasEffectStatusSummonSylph( MCreature* pCreature );

extern void	SetFlagTo( bool bTae );

//-----------------------------------------------------------------------------
// Init
//-----------------------------------------------------------------------------
void
CGameUpdate::Init()
{
	g_bPreviousMove = false;

	// mouse event 처리
	g_pSDLInput->SetMouseEventReceiver( DXMouseEvent );

	// keyboard event 처리
	g_pSDLInput->SetKeyboardEventReceiver( DXKeyboardEvent );

	// Text input (SDL2 only). The SDL backend delivers SDL_TEXTINPUT to
	// InputFocusManager itself, so there is no receiver to register here.
	dxlib_input_start_text();  // Enable SDL text input
}

//-----------------------------------------------------------------------------
// DXMouseEvent
//-----------------------------------------------------------------------------
void		
CGameUpdate::DXMouseEvent(CSDLInput::E_MOUSE_EVENT event, int x, int y, int z)
{
	(void)x;
	(void)y;
	(void)z;

	if(g_pEventManager->GetEventByFlag(EVENTFLAG_DENY_INPUT_MOUSE))
	{
//		g_pEventManager->RemoveAllEvent();
		return;
	}

	static MonotonicClock::TimePoint	last_click_time;
	
	switch (event)
	{
			case CSDLInput::LEFTDOWN :
//				if ((DWORD)abs(GetTickCount() - last_click_time) <= g_double_click_time)
//				{
//					if (g_x>= double_click_x-1 && g_x <= double_click_x+1 &&
//						g_y>= double_click_y-1 && g_y <= double_click_y+1)
//					{
//						#ifdef OUTPUT_DEBUG_UPDATE_LOOP
//						DEBUG_ADD("MLD");
//						#endif
//						
//						gC_vs_ui.MouseControl(M_LB_DOUBLECLICK, g_x, g_y);
//						
//						g_pSDLInput->m_lb_down = false;
//						g_pSDLInput->m_lb_up = false;
//						
//						#ifdef OUTPUT_DEBUG_UPDATE_LOOP
//						DEBUG_ADD("MLD2");
//						#endif
//						
//						last_click_time = 0;
//						return;
//					}
//				}				
//				
				// Deliberately not delivered from here. ProcessInput() sends
				// M_LEFTBUTTON_DOWN from the m_lb_down pulse this same frame,
				// as the DirectInput original did (this line was commented
				// out there); sending it here too ran every click twice, so
				// an equip swap undid itself. The wait-UI screens have their
				// own receiver and are unaffected.
				//gC_vs_ui.MouseControl(M_LEFTBUTTON_DOWN, g_x, g_y);
				last_click_time = MonotonicClock::Now();
				
				#ifdef OUTPUT_DEBUG			
				if (g_pSDLInput->KeyDown(DIK_LSHIFT) && g_bSlideScreenShot)
				{
					g_rectScreenShot.left = g_x;
					g_rectScreenShot.right = g_x;
					g_rectScreenShot.top = g_y;
					g_rectScreenShot.bottom = g_y;
				}

				//---------------------------------------------------------
				// Minimap에 클릭하면 그쪽으로 이동한다.
				//---------------------------------------------------------
				#endif
			break;

			case CSDLInput::RIGHTUP :
				// 지뢰 만드는 거 중지
				gC_vs_ui.EndInstallMineProgress();
				//gC_vs_ui.EndCreateMineProgress();
				//gC_vs_ui.EndCreateBombProgress();
			break;

			//case CSDLInput::RIGHTDOWN :
				//---------------------------------------------------------
				// Chatting창에 extra input
				//---------------------------------------------------------
				// 2001.7.14 주석처리
				//gC_vs_ui.ChatMouseControlExtra( M_RIGHTBUTTON_DOWN, g_x, g_y );
			//break;


		case CSDLInput::WHEELDOWN:
			gC_vs_ui.MouseControl(M_WHEEL_DOWN, g_x, g_y);
			///gC_vs_ui.ChatMouseControlExtra( M_WHEEL_DOWN, g_x, g_y );
			break;

		case CSDLInput::WHEELUP:
			gC_vs_ui.MouseControl(M_WHEEL_UP, g_x, g_y);
//			gC_vs_ui.ChatMouseControlExtra( M_WHEEL_UP, g_x, g_y );
			break;

		default:
			break;
	}
}

//#include "PacketDef.h"
				
//-----------------------------------------------------------------------------
// DXKeyboardEvent
//-----------------------------------------------------------------------------
void	
CGameUpdate::DXKeyboardEvent(CSDLInput::E_KEYBOARD_EVENT event, DWORD key)
{
	if(g_pEventManager->GetEventByFlag(EVENTFLAG_DENY_INPUT_KEYBOARD))
	{
		return;
	}

	if (event==CSDLInput::KEYDOWN)
	{
		if(key == 0xcc) return;		// 이건 머지-_-?;;

		// Convert DIK scan codes to VK virtual key codes for control keys
		// These keys need WM_KEYDOWN messages for text editing to work
		UINT vk_key = 0;
		bool is_control_key = false;

		// DIK constants that might not be defined in all build configurations
		enum {
			DIK_CONST_BACK = 0x0E,
			DIK_CONST_DELETE = 0xD3,
			DIK_CONST_INSERT = 0xD2
		};

		switch (key) {
			case DIK_TAB:      vk_key = VK_TAB; is_control_key = true; break;
			case DIK_CONST_BACK:     vk_key = VK_BACK; is_control_key = true; break;
			case DIK_RETURN:   vk_key = VK_RETURN; is_control_key = true; break;
			case DIK_ESCAPE:   vk_key = VK_ESCAPE; is_control_key = true; break;
			case DIK_LEFT:     vk_key = VK_LEFT; is_control_key = true; break;
			case DIK_RIGHT:    vk_key = VK_RIGHT; is_control_key = true; break;
			case DIK_UP:       vk_key = VK_UP; is_control_key = true; break;
			case DIK_DOWN:     vk_key = VK_DOWN; is_control_key = true; break;
			case DIK_HOME:     vk_key = VK_HOME; is_control_key = true; break;
			case DIK_END:      vk_key = VK_END; is_control_key = true; break;
			case DIK_CONST_DELETE:   vk_key = VK_DELETE; is_control_key = true; break;
			case DIK_CONST_INSERT:   vk_key = VK_INSERT; is_control_key = true; break;
		}

		if (is_control_key) {
			static int keydown_debug_count = 0;
			if (keydown_debug_count < 10) {
				printf("  CGameUpdate DXKeyboardEvent: Sending WM_KEYDOWN, vk_key=%u (DIK=%lu)\n", vk_key, (unsigned long)key);
				keydown_debug_count++;
			}
			gC_vs_ui.KeyboardControl(WM_KEYDOWN, vk_key, 0);
		}

		gC_vs_ui.DIKeyboardControl(event, key);
	
		switch (key)
		{	
//			__BEGIN_HELP_EVENT
//				case DIK_LMENU :
//					// [도움말] alt누를때
//				
//					ExecuteHelpEvent( HE_PRESSED_ALT );					
//				break;
//			__END_HELP_EVENT				

// 머지 이건-_-?;;; 혹시 ? 아닌데-_-;
//			case DIK_F10 :
//				gC_vs_ui.HotKey_F10();
//			break;

			case DIK_SPACE :
				if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL) )
				{
				//	gC_vs_ui.HotKey_SummonPet(); 
//					gC_vs_ui.HotKey_WindowToggle();
				}
			break;

			//------------------------------------
			// L-CONTROL + S
			//------------------------------------
			//case DIK_S :				
			//	if (g_pSDLInput->KeyDown(DIK_LCONTROL))
			//	{	
					/*
					switch (rand()%3)
					{
						case 0 : (*g_pUIDialog).PopupMessageDlg(UIDialog::MESSAGE_CANNOT_BUY_NO_SPACE); break;
						case 1 : (*g_pUIDialog).PopupMessageDlg(UIDialog::MESSAGE_CANNOT_BUY_NO_MONEY); break;
						case 2 : (*g_pUIDialog).PopupMessageDlg(UIDialog::MESSAGE_CANNOT_BUY_NO_ITEM); break;
					}
					*/
					/*
					// 임시...
					g_TempInformation.Value2 = SKILL_DOUBLE_IMPACT;

					if (g_TempInformation.Mode==TempInformation::MODE_SKILL_LEARN)
					{
						#ifdef CONNECT_SERVER
							CGLearnSkill _CGLearnSkill;

							// temp information에 저장된 정보 읽기
							_CGLearnSkill.setSkillDomainType( g_TempInformation.Value1 );
							_CGLearnSkill.setSkillType( g_TempInformation.Value2 );
							
							
							#if defined(OUTPUT_DEBUG) && defined(__DEBUG_OUTPUT__)
									DEBUG_ADD_FORMAT( "[ Send ] %s", _CGLearnSkill.toString().c_str() );
							#endif

							g_pSocket->sendPacket( &_CGLearnSkill );
						#endif
					}
					*/


					// test
					/*
					ExecuteActionInfoFromMainNode(
							MAGIC_LIGHT,
						
							g_pPlayer->GetX(), g_pPlayer->GetY(), 0,
							g_pPlayer->GetDirection(),
							
							g_pPlayer->GetID(),
							g_pPlayer->GetX(), g_pPlayer->GetY(), 0,
							
							64,
							
							NULL
					);
					*/

					//---------------------------------------------------
					//
					// PC Talk Box Test
					//
					//---------------------------------------------------
					//---------------------------------------------------
					// UI에 알림...
					//---------------------------------------------------
					//
					// [ TEST CODE ]
					//
					// UI에서 선택된 대답
					/*
					g_PCTalkBox.SetAnswerID( rand()%g_PCTalkBox.size() );

					//---------------------------------------------------
					// server에 알림
					//---------------------------------------------------
					#ifdef CONNECT_SERVER
						CGNPCAskAnswer _CGNPCAskAnswer;
						_CGNPCAskAnswer.setObjectID( g_PCTalkBox.GetNPCID() );
						_CGNPCAskAnswer.setScriptID( g_PCTalkBox.GetScriptID() );
						_CGNPCAskAnswer.setAnswerID( g_PCTalkBox.GetAnswerID() );

						#if defined(OUTPUT_DEBUG) && defined(__DEBUG_OUTPUT__)
								DEBUG_ADD_FORMAT("[Send] %s", _CGNPCAskAnswer.toString().c_str());
						#endif

						g_pSocket->sendPacket( &_CGNPCAskAnswer );
					#endif
					*/
			//	}
			//break;

			//------------------------------------
			// FPS Toggle
			//------------------------------------
			case DIK_F : 
				if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
				{
					if(g_pEventManager->IsEvent(EVENTID_OUSTERS_FIN))
					{
						g_pEventManager->RemoveEvent(EVENTID_OUSTERS_FIN);
					}
//					else
//					{
//						MEvent event;
//						event.eventID = EVENTID_OUSTERS_FIN;
//						event.eventType = EVENTTYPE_ZONE;
//						event.eventFlag = EVENTFLAG_ONLY_EVENT_BACKGROUND | EVENTFLAG_NOT_DRAW_UI | EVENTFLAG_NOT_DRAW_CREATURE | EVENTFLAG_NOT_DRAW_INFORMATION | EVENTFLAG_NOT_DRAW_CREATURE_SHADOW | EVENTFLAG_NOT_DRAW_ITEM | EVENTFLAG_NOT_DRAW_MOUSE_POINTER | EVENTFLAG_NOT_DRAW_EFFECT | EVENTFLAG_DENY_INPUT | EVENTFLAG_NOT_FADE_SCREEN | EVENTFLAG_NOT_PLAY_SOUND;
//						event.parameter1 = 0;
//						event.parameter4 = EVENTBACKGROUNDID_CLOUD;
//						g_pEventManager->AddEvent(event);
//					}
					(*g_pUserOption).DrawFPS = !(*g_pUserOption).DrawFPS;
//							g_abHolyLandBonusSkills[4] = !g_abHolyLandBonusSkills[4];
//							g_pSkillAvailable->SetAvailableSkills();
				}

				#if defined(OUTPUT_DEBUG) && defined(_DEBUG)
					else if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
					{
						if (g_pPlayer->HasEffectStatus( EFFECTSTATUS_FADE_OUT ))
						{
							g_pPlayer->RemoveEffectStatus( EFFECTSTATUS_FADE_OUT );
						}
						else
						{
							// 5초 동안 좌우 잔상
							g_pPlayer->AddEffectStatus( EFFECTSTATUS_FADE_OUT, 5*1000 );
						}
					}
				#endif
			break;				

			//------------------------------------
			// Minimap Toggle
			//------------------------------------
			/*
			case DIK_M : 
				if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
				{
					(*g_pUserOption).DrawMinimap = !(*g_pUserOption).DrawMinimap;
				}
			break;

			//------------------------------------
			// DrawZoneName Toggle
			//------------------------------------
			case DIK_Z :
				if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
				{
					(*g_pUserOption).DrawZoneName = !(*g_pUserOption).DrawZoneName;
				}
			break;

			//------------------------------------
			// DrawGameTime Toggle
			//------------------------------------
			case DIK_T :
				if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
				{
					(*g_pUserOption).DrawGameTime = !(*g_pUserOption).DrawGameTime;
				}
			break;
			*/

			//------------------------------------
			// Blending Shadow
			//------------------------------------
			/*
			case DIK_V : 
				if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
				{
					(*g_pUserOption).BlendingShadow = !(*g_pUserOption).BlendingShadow;
					if (g_pTopView!=NULL)
					{
						g_pTopView->ClearShadowManager();
					}
				}
			break;
			*/

			//------------------------------------
			// 효과음 on/off
			//------------------------------------
			/*
			case DIK_E : 
				if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
				{
					//------------------------------------
					// 연주중이면.. 중단..
					//------------------------------------
					if (g_pUserOption->PlaySound)
					{
						g_pUserOption->PlaySound = FALSE;

						StopSound( g_previousSoundID );
						g_previousSoundID = SOUNDID_NULL;						
					}
					else
					{
						g_pUserOption->PlaySound = TRUE;
					}
				}
				//---------------------------------------------------------
				// Watch Mode
				//---------------------------------------------------------
				else if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
				{
					SetWatchMode( !g_bWatchMode );
				}
			break;
			*/

			//------------------------------------
			// 배경 음악 on/off
			//------------------------------------
			/*
			case DIK_B : 
				#ifdef OUTPUT_DEBUG
					//------------------------------------
					// 박쥐 변신 test
					//------------------------------------
					if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
					{
						MActionResult* pResult = new MActionResult;

						if (g_pPlayer->GetCreatureType()!=185)
						{									
							pResult->Add( new MActionResultNodeChangeCreatureType( g_pPlayer->GetID(), 185 ) );
							//g_pPlayer->SetCreatureType( 185 );
							//g_pPlayer->SetFlyingCreature();
						}
						else
						{
							pResult->Add( new MActionResultNodeChangeCreatureType( g_pPlayer->GetID(), CREATURETYPE_VAMPIRE_MALE ) );
							//g_pPlayer->SetCreatureType( CREATURETYPE_VAMPIRE_MALE );
							//g_pPlayer->SetGroundCreature();
						}

						//--------------------------------------------------
						// 박쥐 변신 test
						//--------------------------------------------------								
						ExecuteActionInfoFromMainNode(
							RESULT_MAGIC_TRANSFORM_TO_BAT,										// 사용 기술 번호
						
							g_pPlayer->GetX(), g_pPlayer->GetY(), 0,
							g_pPlayer->GetDirection(),														// 사용 방향
							
							OBJECTID_NULL,												// 목표에 대한 정보
							g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 
							
							0,													// 기술의 (남은) 지속 시간		
							
							pResult, //NULL,
							
							false);			// 기술 첨부터 시작한다.

						g_pPlayer->SetDelay( 1000 );
					}
					
				#endif
				if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
				{
					//if (g_SDLMusic.IsPlay())
					//{
					//	g_SDLMusic.Pause();
					//}
					//else
					//{						
					//	g_SDLMusic.Resume();
					//}
					
					//------------------------------------
					// 연주중이면.. 중단..
					//------------------------------------
					if (g_pUserOption->PlayWaveMusic)
					{
						if (g_pUserOption->PlayMusic)
						{
							g_pUserOption->PlayMusic = FALSE;

							if (g_pSDLStream->IsPlay())
							{
								g_pSDLStream->Stop();
							}
						}
						//------------------------------------
						// 아니면.. 다시 play
						//------------------------------------
						else 
						{
							g_pUserOption->PlayMusic = TRUE;
										
							PlayMusicCurrentZone();
						}
					}
					else
					{
						if (g_pUserOption->PlayMusic)
						{
							g_pUserOption->PlayMusic = FALSE;

							if (g_Music.IsPlay())
							{
								g_Music.Pause();
							}
						}
						//------------------------------------
						// 아니면.. 다시 play
						//------------------------------------
						else 
						{
							g_pUserOption->PlayMusic = TRUE;

							if (g_Music.IsPlay())
							{
								g_Music.Resume();
							}
							else
							{													
								PlayMusicCurrentZone();
							}
						}
					}
				}
			break;
			*/

			//------------------------------------
			// DrawInterface Toggle
			//------------------------------------
			/*
			case DIK_H :
				if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
				{
					//(*g_pUserOption).DrawInterface = !(*g_pUserOption).DrawInterface;

					// 도움말 띄운다.
					//(*g_pUIDialog).PopupHelpDlg();
				}
			break;
			*/
			
		
			#ifdef OUTPUT_DEBUG

		
				case DIK_SCROLL :
					if (g_bSlideScreenShot)
					{
						g_bSaveSlideScreenShot = !g_bSaveSlideScreenShot;
						g_bSlideRectSelect = false;
					}
				break;

				case DIK_S :
					if (g_pSDLInput->KeyDown(DIK_LMENU))
					{
						g_bSlideScreenShot = !g_bSlideScreenShot;
						g_bSlideRectSelect = false;
					}
				break;

				case DIK_P :
					if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU) )
					{
						bool en = true;
						if(g_abHolyLandBonusSkills[0] == true)
							en = false;
						else
							en = true;

						for(int i = 0; i < HOLYLAND_BONUS_MAX; i++)
						{
							g_abHolyLandBonusSkills[i] = en;
						}

						g_pSkillAvailable->SetAvailableSkills();

//						if (g_pProfiler!=NULL)
//						{
//							g_pProfiler->WriteToFile("profiler.txt", true);
//						}
					}
				break;

				#if defined(_DEBUG) || defined(OUTPUT_DEBUG)
					case DIK_F1 :
					{
						// info 다시 읽기 
						if (g_pSDLInput->KeyDown(DIK_LMENU) 
							|| g_pSDLInput->KeyDown(DIK_RMENU))
						{
							InitInfomation();
							InitSound();
						}					
					}
					break;

					//------------------------------------------------------------
					//
					// 기술 사용 - packet 테스트를 위해서
					//
					//------------------------------------------------------------
					case DIK_L : 
					{
						if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
						{
							MZone::CREATURE_MAP::const_iterator iCreature = g_pZone->GetCreatureBegin();					

							for (int i=0; i<g_pZone->GetCreatureNumber(); i++)
							{
								MCreature* pCreature = (*iCreature).second;

								if (pCreature!=g_pPlayer)
								{
									// 기술 사용
									// 기술 실패
									//int targetID;						
										
									//int nth;
									
									MZone::CREATURE_MAP::const_iterator iCreature2;

									// 기술 성공
									if (rand()%2==0)
									{
										GCSkillToTileOK2 gcSkillToTileOK2;

										gcSkillToTileOK2.setObjectID( pCreature->GetID() );
										gcSkillToTileOK2.setSkillType( ((rand()%2)? 34 : 37) );
										gcSkillToTileOK2.setX( pCreature->GetX() );
										gcSkillToTileOK2.setY( pCreature->GetY() );
										gcSkillToTileOK2.setRange( rand()%8 );
										gcSkillToTileOK2.setDuration( 0 );
										gcSkillToTileOK2.addCListElement( g_pPlayer->GetID() );
										gcSkillToTileOK2.addShortData( (ModifyType)12, 97 );
										gcSkillToTileOK2.addLongData( (ModifyType)48, 68002 );										

//										DEBUG_ADD_FORMAT("[Execute] %s", gcSkillToTileOK2.toString().c_str());

										// Local echo through the dispatcher: the packet's
										// virtual execute() is gone (RESTRUCTURING.md 2.2)
										// and the registered handler is the same code it
										// used to delegate to.
										PacketDispatcher::dispatch( &gcSkillToTileOK2, g_pSocket );
										
										DEBUG_ADD("TileOK2OKOK");
									}
									/*
									else
									if (rand()%2==0)
									{
										nth = rand()%g_pZone->GetCreatureNumber();

										iCreature2 = g_pZone->GetCreatureBegin();

										for (int i=0; i<nth; i++)
										{
											targetID = iCreature->second->GetID();
											iCreature2 ++;
										}

										GCSkillToObjectOK5 gcSkillToObjectOK5;

										gcSkillToObjectOK5.setObjectID( pCreature->GetID() );
										gcSkillToObjectOK5.setTargetObjectID( targetID );
										gcSkillToObjectOK5.setSkillType( rand()%60+10 );
										gcSkillToObjectOK5.setDuration( 50 );

										DEBUG_ADD_FORMAT("[Execute] %s", gcSkillToObjectOK5.toString().c_str());

										#ifdef CONNECT_SERVER
											PacketDispatcher::dispatch( &gcSkillToObjectOK5, g_pSocket );
										#else
											PacketDispatcher::dispatch( &gcSkillToObjectOK5, NULL );
										#endif
									}
									else if (rand()%2==0)
									{										
										int skillType = rand()%60+10;

										if (skillType != MAGIC_INVISIBILITY
											&& skillType != SKILL_SNIPPING)
										{
											GCSkillToSelfOK2 gcSkillToSelfOK2;

											gcSkillToSelfOK2.setObjectID( pCreature->GetID() );
											gcSkillToSelfOK2.setSkillType( skillType );
											gcSkillToSelfOK2.setDuration( 50 );

											DEBUG_ADD_FORMAT("[Execute] %s", gcSkillToSelfOK2.toString().c_str());

											#ifdef CONNECT_SERVER
												PacketDispatcher::dispatch( &gcSkillToSelfOK2, g_pSocket );
											#else
												PacketDispatcher::dispatch( &gcSkillToSelfOK2, NULL );
											#endif
										}
									}
									else
									{
										// 기술 실패
										nth = rand()%g_pZone->GetCreatureNumber();

										iCreature2 = g_pZone->GetCreatureBegin();

										for (int i=0; i<nth; i++)
										{
											targetID = iCreature->second->GetID();
											iCreature2 ++;
										}						

										GCSkillFailed2 gcSkillFailed2;
										gcSkillFailed2.setObjectID( pCreature->GetID() );
										gcSkillFailed2.setTargetObjectID( targetID );
										gcSkillFailed2.setSkillType( (rand()%2? 5:65) );

										DEBUG_ADD_FORMAT("[Execute] %s", gcSkillFailed2.toString().c_str());
										
										#ifdef CONNECT_SERVER
											PacketDispatcher::dispatch( &gcSkillFailed2, g_pSocket );
										#else
											PacketDispatcher::dispatch( &gcSkillFailed2, NULL );
										#endif
									}
									*/
								}

								iCreature++;
							}
						}
					}
					break;

					case DIK_1 :
//						if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
//						{
//							if( g_pSoundBufferForOGG == NULL )
//								g_pSoundBufferForOGG = new CDirectSoundBuffer(g_hWnd, SOUND_STEREO, SOUND_44K, SOUND_16BIT);
//
//							if( g_pOGG == NULL )
//								g_pOGG = new COGGSTREAM(g_hWnd, g_pSoundBufferForOGG, 44100, 11025, 8800,1);
//							
//							if( g_oggfile != NULL)
//								fclose(g_oggfile );
//							g_oggfile = fopen("C:\\ogggggg\\3\\blood.ogg","rb");
//							g_pOGG->streamLoad(g_oggfile, NULL);
//							g_pOGG->streamPlay(SOUND_PLAY_REPEAT);
//							
////							gC_vs_ui.SetOustersSkillDown();
////							g_pMP3->Stop();
////							g_pMP3->Open("data\\music\\blood.mp3");
////							g_pMP3->Play( true );
////							g_bTestMusic = false;
////							g_pSystemMessage->Add("Blood.mp3 를 연주합니다");							
//						}
//						// ALT + 1  --> 공격 속도 Slow
						if(g_pSDLInput->KeyDown(DIK_LCONTROL))
						{ 
							
							//g_pPlayer->SetSpecialActionInfo(TEMP_SKILL_INFINITY_THUNDERBLOT);
//							if( g_pEventManager->GetEvent( EVENTID_WAR_EFFECT ) == NULL )
//							{
//								MEvent event;
//								event.eventID = EVENTID_WAR_EFFECT;
//								event.eventType = EVENTTYPE_NULL;
//								event.eventFlag = EVENTFLAG_SHAKE_SCREEN | EVENTFLAG_FADE_SCREEN;
//								event.parameter2 = RGB( -30, -30, 0 );
//								event.parameter3 = 1;
//								event.eventDelay = -1;
//								event.showTime = 2000;
//								event.totalTime = 30000;
//								g_pEventManager->AddEvent( event );
//							}
//							else
//							{
//								g_pEventManager->RemoveEvent( EVENTID_WAR_EFFECT );
//							}
//							ExecuteActionInfoFromMainNode(SKILL_DESTRUCTION_SPEAR_MASTERY,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//								g_pPlayer->GetX()+5, g_pPlayer->GetY(), 0, 100, NULL, false);			
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_SUMMON_GDR,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	0,	
//								g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 5000, NULL, false);			
//							MEvent event;
//
//							event.eventID = EVENTID_GDR_PRESENT;
//							event.eventType = EVENTTYPE_ZONE;
//							event.eventFlag = EVENTFLAG_SHAKE_SCREEN;
//							event.parameter3 = 2;
//							event.eventDelay =5000;
//							g_pEventManager->AddEvent(event);
//							//SetFadeStart(31, 0, 1, 0,0,0, 6);
							//gC_vs_ui.RunNameing();
							//g_pPlayer->SetAction(ACTION_DRAINED);
//							g_pPlayer->m_ActionCount = 0;
							//g_pPlayer->SetCauseCriticalWounds(100);
							//g_pPlayer->AddEffectStatus(EFFECTSTATUS_GLACIER, 100);
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_BLINDNESS,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//								g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 30, NULL, false);
						//	g_pPlayer->AddEffectStatus(EFFECTSTATUS_GRAND_MASTER_SLAYER_130, 0xffff);
							//g_pPlayer->AddEffectStatus(EFFECTSTATUS_BLINDNESS, 10);
//							g_pPlayer->AddEffectStatus(EFFECTSTATUS_CAN_ENTER_GDR_LAIR, 10);
//							g_pPlayer->AddEffectStatus(EFFECTSTATUS_ICE_STICK_SE, 10);
//							g_pPlayer->AddEffectStatus(EFFECTSTATUS_GDR_GLACIER_SLAYER, 100);
//							g_pPlayer->AddEffectStatus(EFFECTSTATUS_GRAND_MASTER_OUSTERS_130, 0xffff);
//							g_pPlayer->AddEffectStatus(EFFECTSTATUS_GRAND_MASTER_OUSTERS_150, 0xffff);
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_SLAYER_MASTER_130,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//								g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 5000, NULL, false);			
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_SLAYER_MASTER_150,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//								g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 5000, NULL, false);			
////
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_VAMPIRE_MASTER_130,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//								g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 5000, NULL, false);			
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_VAMPIRE_MASTER_150,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//								g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 5000, NULL, false);			
//
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_OUSTERS_MASTER_130,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//								g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 5000, NULL, false);			
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TEST_VIOLENT_PHANTOM,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//								g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 20, NULL, false);			
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TEST_SUMMON_GORE_GRAND_GROUND,g_pPlayer->GetX()+1, g_pPlayer->GetY()-3, 0,g_pPlayer->GetDirection(),	0,	
//								g_pPlayer->GetX()+1, g_pPlayer->GetY()-3, 0, 50, NULL, false);
//							ExecuteActionInfoFromMainNode(RESULT_CLIENT_TEST_SWORD_OF_THOR,g_pPlayer->GetX(), g_pPlayer->GetY()+3, 0,g_pPlayer->GetDirection(),	0,	
//								g_pPlayer->GetX(), g_pPlayer->GetY()+3, 0, 100, NULL, false);
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TEST_BURNING_SOL_1,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//								g_pPlayer->GetX()-5, g_pPlayer->GetY(), 0, 100, NULL, false);
//							g_pPlayer->SetAction(ACTION_SLAYER_BLADE);
//							ExecuteActionInfoFromMainNode(SKILL_PLEASURE_EXPLOSION,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//								g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 0, NULL, false);			
							
//							if(g_pEventManager->IsEvent(EVENTID_BG_CLOUD))
//							{
//								g_pEventManager->RemoveEvent(EVENTID_BG_CLOUD);
//							}
//							else
//							{
//								MEvent event;
//								event.eventID = EVENTID_BG_CLOUD;
//								event.eventType = EVENTTYPE_ZONE;
//								event.eventFlag = EVENTFLAG_CLOUD_BACKGROUND;
//								event.parameter3 = 2;
//								event.eventDelay =60000; // 5 초
//								g_pEventManager->AddEvent(event);
//							}
							
//							ExecuteActionInfoFromMainNode(SKILL_PLEASURE_EXPLOSION,g_pPlayer->GetX(), g_pPlayer->GetY()+3, 0,g_pPlayer->GetDirection(),	0,	
//								g_pPlayer->GetX(), g_pPlayer->GetY()+3, 0, 100, NULL, false);
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_LAND_MINE_LIGHT,g_pPlayer->GetX()+5, g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//								g_pPlayer->GetX()+5, g_pPlayer->GetY(), 0, 100, NULL, false);
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_PLEASUER_STATION_LIGHT,g_pPlayer->GetX()+3, g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//								g_pPlayer->GetX()+3, g_pPlayer->GetY(), 0, 100, NULL, false);

							//g_pPlayer->AddEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_1, 1000);
//
//							MActionResult* pResult = new MActionResult;
////
//							if (g_pPlayer->GetCreatureType()!=742)
//							{									
//								g_pPlayer->SetCreatureType( 742 );
//
//								g_pPlayer->SetGroundCreature();
//					
//								g_pPlayer->SetDirection( 2 );
//								g_pPlayer->SetCurrentDirection( 2 );
//								g_pPlayer->SetAction( ACTION_STAND );			
//
//								//--------------------------------------------------
//								// 늑대로 변신하는 결과
//								//--------------------------------------------------
//								MActionResult* pResult = new MActionResult;
//
//								pResult->Add( new MActionResultNodeChangeCreatureType( g_pPlayer->GetID(), 742 ) );
//
//								//--------------------------------------------------
//								// 늑대 변신 
//								//--------------------------------------------------								
//								ExecuteActionInfoFromMainNode(
//									RESULT_SKILL_INSTALL_TURRET,										// 사용 기술 번호
//								
//									g_pPlayer->GetX(), g_pPlayer->GetY(), 0,
//									g_pPlayer->GetDirection(),														// 사용 방향
//									
//									OBJECTID_NULL,												// 목표에 대한 정보
//									g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 
//									
//									0,													// 기술의 (남은) 지속 시간		
//									
//									NULL, //NULL,
//									
//									false);			// 기술 첨부터 시작한다.
//
//							}
//							else
//							{
//								pResult->Add( new MActionResultNodeChangeCreatureType( g_pPlayer->GetID(), CREATURETYPE_SLAYER_FEMALE ) );
//								//g_pPlayer->SetCreatureType( CREATURETYPE_VAMPIRE_MALE );
//								//g_pPlayer->SetGroundCreature();
//							}
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TEST_BURNING_SOL_4,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//								g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 20, pResult, false);	
//
//							g_pPlayer->SetDelay( 1000 );

							
//							UI_RunPetStorage();SKILL_CLIENT_TEST_SUMMON_GORE_GRAND_GROUND
//							UI_SetPetStorage(g_pStorage);
							// start 펫 소환 단축키 관련
//							for(int i = 0; i< 10; i++)
//								for(int j = 0; j < 6; j++)
//								{
//									const MItem * p_item = g_pInventory->GetItem(i, j);
//				
//									if (p_item) // Item이 있다.
//									{
//										if(p_item->GetItemClass() == ITEM_CLASS_PET_ITEM)
//										{
//											gpC_base->SendMessage(UI_ITEM_USE, p_item->GetID(), 0, (MItem *)p_item);
//											return;
//										}
//									}
//								}
							// end 펫 소환 단축키 관련

							// start npc 대화창 관련
//							g_pPCTalkBox->Release();
//							//---------------------------------------------------
//							// normal
//							//---------------------------------------------------
//							g_pPCTalkBox->SetType( PCTalkBox::SELECT_QUEST );
//						
//							int scriptID = 101;
//							
//							//---------------------------------------------------
//							// PC Talk Box의 정보 설정
//							//---------------------------------------------------
//							// SetContent라고 이름이 되어있지만.. Subject이다. - -;
//							g_pPCTalkBox->SetContent( "하하하" );
//							g_pPCTalkBox->SetNPCID( 297 );
//							g_pPCTalkBox->SetCreatureType( 297 );
//							g_pPCTalkBox->SetScriptID( scriptID );
//							
//							//---------------------------------------------------
//							// 각 string 추가
//							//---------------------------------------------------
//							int contentSize = 2;//g_pNPCScriptTable->GetContentSize( scriptID );
//							
//							//for (int i=0; i<contentSize; i++)
//							{
//								// g_PCTalkBox에 추가
//								g_pPCTalkBox->AddString( "우헤헤" );
//								g_pPCTalkBox->AddString( "크하하" );
//							}
//							
//							g_pUIDialog->PopupPCTalkDlg();
							// start npc 대화창 관련

//							gC_vs_ui.AddHelpMail(6); 
//							gC_vs_ui.AddHelpMail(0); 
//							gC_vs_ui.AddHelpMail(1); 
//							gC_vs_ui.AddHelpMail(2); 
//							gC_vs_ui.AddHelpMail(3); 
//							gC_vs_ui.AddHelpMail(4); 
//							gC_vs_ui.AddHelpMail(5); 
						
//							gC_vs_ui.RunSMSMessage();
//							extern void Add_GDR_Effect(int nEffect, bool bAppearBossMonster);
//							Add_GDR_Effect(1, false);
//							Add_GDR_Effect(2, false);
//
//							ExecuteActionInfoFromMainNode(TEMP_SKILL_SPIT_STREAM,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//								g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 0xffff, NULL, false);			
							//g_pPlayer->SetAction(ACTION_OUSTERS_CHAKRAM_SLOW);
	// create ghost
							{

//								extern void SetDragonTorando(int Type, DWORD ObjectID, int TileX, int TileY);
//								SetDragonTorando(EFFECTSTATUS_DRAGON_TORNADO, 11000, g_pPlayer->GetX()+4, g_pPlayer->GetY());

								UI_PopupMessage( UI_STRING_MESSAGE_CAMPAIGN_HELP_THANKS );
//
//								ExecuteActionInfoFromMainNode(SKILL_CLIENT_BIKE_CRASH, g_pPlayer->GetX(), g_pPlayer->GetY(), 0, g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//									g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 5000, NULL, false);	
//								MFakeCreature *pFakeCreature = NewFakeCreature(CREATURETYPE_ROCKET_LUNCHER, g_pPlayer->GetX()+5, g_pPlayer->GetY(), rand()%8);
//
//								if (!g_pZone->AddFakeCreature( pFakeCreature ))
//								{
//									delete pFakeCreature;
//								}
//								else
//								{
//									pFakeCreature->SetZone(g_pZone);
//									pFakeCreature->SetFakeCreatureType(MFakeCreature::FAKE_CREATURE_ROCKET);
//									pFakeCreature->SetMoveType(MCreature::CREATURE_FAKE_FLYING);
//									pFakeCreature->AddEffectStatus( (EFFECTSTATUS)(EFFECTSTATUS_ROCKET_LUNCHER), 0xFFFF );
//									pFakeCreature->SetTraceID(g_pPlayer->GetID());
//									pFakeCreature->SetTime(timeGetTime()+3000); 
//								//	pFakeCreature->SetAction( ACTION_MOVE );
//								}
							}
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TRAP_ICICLE_DROP_SIGN,g_pPlayer->GetX()+5, g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	0,	
//								g_pPlayer->GetX()+5, g_pPlayer->GetY(), 0, 10, NULL, false);			
//							
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TRAP_ICICLE_LARGE_AUGER,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	0,	
//								g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 5000, NULL, false);			
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TRAP_ICICLE_LARGE_AUGER_SIGN,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	0,	
//								g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 10, NULL, false);			
//							
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TRAP_ICICLE_AUGER,g_pPlayer->GetX(), g_pPlayer->GetY()-5, 0,g_pPlayer->GetDirection(),	0,	
//								g_pPlayer->GetX(), g_pPlayer->GetY()-5, 0, 5000, NULL, false);			
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TRAP_ICICLE_AUGER_SIGN,g_pPlayer->GetX(), g_pPlayer->GetY()-5, 0,g_pPlayer->GetDirection(),	0,	
//								g_pPlayer->GetX(), g_pPlayer->GetY()-5, 0, 10, NULL, false);	
//							
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TRAP_ICE_STICK_SW,g_pPlayer->GetX(), g_pPlayer->GetY()+5, 0,g_pPlayer->GetDirection(),	0,	
//								g_pPlayer->GetX(), g_pPlayer->GetY()+5, 0, 0, NULL, false);			
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TRAP_ICE_STICK_SW_SIGN,g_pPlayer->GetX(), g_pPlayer->GetY()+5, 0,g_pPlayer->GetDirection(),	0,	
//								g_pPlayer->GetX(), g_pPlayer->GetY()+5, 0, 0, NULL, false);			
//
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TRAP_ICE_STICK_SE,g_pPlayer->GetX()-5, g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	0,	
//								g_pPlayer->GetX()-5, g_pPlayer->GetY(), 0, 5000, NULL, false);			
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TRAP_ICE_STICK_SE_SIGN,g_pPlayer->GetX()-5, g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	0,	
//								g_pPlayer->GetX()-5, g_pPlayer->GetY(), 0, 10, NULL, false);	
//
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TRAP_ICE_STICK_NE,g_pPlayer->GetX()-5, g_pPlayer->GetY()-5, 0,g_pPlayer->GetDirection(),	0,	
//								g_pPlayer->GetX()-5, g_pPlayer->GetY()-5, 0, 5000, NULL, false);			
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TRAP_ICE_STICK_NE_SIGN,g_pPlayer->GetX()-5, g_pPlayer->GetY()-5, 0,g_pPlayer->GetDirection(),	0,	
//								g_pPlayer->GetX()-5, g_pPlayer->GetY()-5, 0, 10, NULL, false);
//
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TRAP_ICE_STICK_NW,g_pPlayer->GetX()+5, g_pPlayer->GetY()+5, 0,g_pPlayer->GetDirection(),	0,	
//								g_pPlayer->GetX()+5, g_pPlayer->GetY()+5, 0, 5000, NULL, false);			
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TRAP_ICE_STICK_NW_SIGN,g_pPlayer->GetX()+5, g_pPlayer->GetY()+5, 0,g_pPlayer->GetDirection(),	0,	
//								g_pPlayer->GetX()+5, g_pPlayer->GetY()+5, 0, 10, NULL, false);
//							
//								for(int i = 0; i< 100 ; i++)
//								{
//									int MapX = rand()% g_pZone->GetWidth();
//									int MapY = rand()% g_pZone->GetHeight();
//									//MFakeCreature *pFakeCreature = NewFakeCreature(716, g_pPlayer->GetX(), g_pPlayer->GetY(), 2);
//									MFakeCreature *pFakeCreature = NewFakeCreature(CREATURETYPE_GHOST, MapX, MapY, rand()%8);
//
//									if (!g_pZone->AddFakeCreature( pFakeCreature ))
//									{
//										delete pFakeCreature;
//									}
//									pFakeCreature->SetZone(g_pZone);
//									pFakeCreature->SetFakeCreatureType(MFakeCreature::FAKE_CREATURE_GHOST);
//									pFakeCreature->SetMoveType(MCreature::CREATURE_FAKE_UNDERGROUND);
//									pFakeCreature->AddEffectStatus( (EFFECTSTATUS)(EFFECTSTATUS_GHOST_1+rand()%2), 0xFFFF );
//								}

								
//							if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_FIRE_ELEMENTAL))
//								g_pPlayer->RemoveEffectStatus( EFFECTSTATUS_FIRE_ELEMENTAL );
//							else
//								g_pPlayer->AddEffectStatus( EFFECTSTATUS_FIRE_ELEMENTAL, 0xFFFF );
//							g_pPlayer->SetDelay(2000);
//							TYPE_OBJECTID petID = g_pPlayer->GetPetID();
//							if(petID != OBJECTID_NULL)
//							{
//								MFakeCreature *pCreature = (MFakeCreature *)g_pZone->GetFakeCreature(petID);
//								pCreature->SetAction( ACTION_ATTACK );
//								pCreature->SyncTurretDirection();
//
//
//								ExecuteActionInfoFromMainNode(
//									SKILL_CLIENT_TANK_ATTACK_3,										// 사용 기술 번호
//									
//									0, 0, 0,
//									pCreature->GetDirection(), // 사용 방향
//									
//									petID,												// 목표에 대한 정보
//									0, 0, 0, 
//									
//									0,													// 기술의 (남은) 지속 시간		
//									
//									NULL,
//									
//									false);	
//								pCreature->AddEffectStatus(EFFECTSTATUS_COMA, 0xFFFF);
//								pCreature->SetDead();
//							SetFlagTo( true );
//						}
//							g_pPlayer->AddEffectStatus(EFFECTSTATUS_HAS_PET, 0);
						}
						if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
						{
							//g_pPlayer->SetWeaponSpeed( MCreature::WEAPON_SPEED_SLOW );

							// 1 ~ 24
							/*
							if (++gtx_op > 24)
							{
								gtx_op = 1;
							}
							*/
							#ifdef __METROTECH_TEST__
								g_iAutoHealPotion = (g_iAutoHealPotion+1)%2;
							#endif
						}
					break;

					case DIK_2 :
						if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
						{  
//							extern void SetDragonTorando(int Type, DWORD ObjectID, int TileX, int TileY);
//							SetDragonTorando(EFFECTSTATUS_DRAGON_TORNADO, 11000, g_pPlayer->GetX()+3, g_pPlayer->GetY());
////
//							UI_RunModifyTax();  
//							if(g_pEventManager->IsEvent(EVENTID_ADVANCEMENT_QUEST_ENDING))
//							{
//								g_pEventManager->RemoveEvent(EVENTID_ADVANCEMENT_QUEST_ENDING);
//							}
//							else
//							{
//								MEvent event;
//								event.eventID = EVENTID_ADVANCEMENT_QUEST_ENDING;
//								event.eventType = EVENTTYPE_ZONE;
//								event.eventFlag = EVENTFLAG_ONLY_EVENT_BACKGROUND | EVENTFLAG_NOT_DRAW_UI | EVENTFLAG_NOT_DRAW_CREATURE | EVENTFLAG_NOT_DRAW_INFORMATION | EVENTFLAG_NOT_DRAW_CREATURE_SHADOW | EVENTFLAG_NOT_DRAW_ITEM | EVENTFLAG_NOT_DRAW_MOUSE_POINTER | EVENTFLAG_NOT_DRAW_EFFECT | EVENTFLAG_DENY_INPUT | EVENTFLAG_NOT_FADE_SCREEN | EVENTFLAG_NOT_PLAY_SOUND;
//								event.parameter1 = 0;
//								event.parameter4 = 3;
//								g_pEventManager->AddEvent(event);
//
////								MEvent event;
////								event.eventID = EVENTID_WAR_EFFECT;
////								event.eventType = EVENTTYPE_NULL;
////								event.eventFlag = EVENTFLAG_SHAKE_SCREEN | EVENTFLAG_FADE_SCREEN;
////								event.parameter2 = RGB( -30, -30, 0 );
////								event.parameter3 = 1;
////								event.eventDelay = -1;
////								event.showTime = 2000;
////								event.totalTime = 30000;
////								g_pEventManager->AddEvent( event );
//							}
//
							if(UI_IsRunning_WebBrowser())
								UI_Close_WebBrowser();
							else
							//add by zdj
					 			UI_Run_WebBrowser(/*"http://market.darkeden.com/"*/"");
//							if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_DIVINE_GUIDANCE))
//								g_pPlayer->RemoveEffectStatus(EFFECTSTATUS_DIVINE_GUIDANCE); 
//							else
//								g_pPlayer->AddEffectStatus(EFFECTSTATUS_DIVINE_GUIDANCE, 0xffff);
							//g_pPlayer->SetCreatureType(770); 
//							if(g_pPlayer->GetCreatureType() == CREATURETYPE_SLAYER_OPERATOR)
//								g_pPlayer->SetCreatureType(358);
//							else
//								g_pPlayer->SetCreatureType(CREATURETYPE_SLAYER_OPERATOR);
//							
						//	g_pPlayer->RemoveEffectStatus(EFFECTSTATUS_GLACIER);
						//	g_pPlayer->RemoveEffectStatus(EFFECTSTATUS_BLESS_FIRE);

//							ExecuteActionInfoFromMainNode(TEMP_SKILL_NOOSE_OF_WRAITH,g_SelectSector.x, g_SelectSector.y, 0,g_pPlayer->GetDirection(), g_pPlayer->GetID()	,	
//								g_SelectSector.x, g_SelectSector.y, 0, 0xffff, NULL, false);
//							g_pPlayer->SetAction(ACTION_DRAINED);
//							ExecuteActionInfoFromMainNode(RESULT_MAGIC_CAUSE_CRITICAL_WOUNDS,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//									g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 0xffff, NULL, false);

//							if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_1))
//								g_pPlayer->AddEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_2, 32);
//							else if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_2))
//								g_pPlayer->AddEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_3, 32);
//							else if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_3))
//								g_pPlayer->AddEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_4, 32);
//							else if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_4))
//							{
//								g_pPlayer->RemoveEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_4);
//								g_pPlayer->StopBurningSol();
//								ExecuteActionInfoFromMainNode(SKILL_CLIENT_BURNING_SOL_4,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//									g_pPlayer->GetX()-4, g_pPlayer->GetY(), 0, 100, NULL, false);	
//								ExecuteActionInfoFromMainNode(RESULT_CLIENT_BURNING_SOL_4,g_pPlayer->GetX()-4, g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//									g_pPlayer->GetX()-4, g_pPlayer->GetY(), 0, 100, NULL, false);	
//							}
//							else 
//								g_pPlayer->AddEffectStatus(EFFECTSTATUS_BURNING_SOL_CHARGE_1, 32);
//
//							ExecuteActionInfoFromMainNode(RESULT_SKILL_SWORD_OF_THOR,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	g_pPlayer->GetID(),	
//								g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 20, NULL, false);			
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_SWORD_OF_THOR,g_pPlayer->GetX()-4, g_pPlayer->GetY()-4, 0,g_pPlayer->GetDirection(),0	,	
//								g_pPlayer->GetX()-4, g_pPlayer->GetY()-4, 0, 50, NULL, false);
//
//							ExecuteActionInfoFromMainNode(RESULT_SKILL_WHITSUNTIDE,g_pPlayer->GetX()+4, g_pPlayer->GetY()+4, 0,g_pPlayer->GetDirection(),0	,	
//								g_pPlayer->GetX()+4, g_pPlayer->GetY()+4, 0, 50, NULL, false);
//
//							ExecuteActionInfoFromMainNode(SKILL_SWEEP_VICE_1,g_pPlayer->GetX(), g_pPlayer->GetY()+4, 0,g_pPlayer->GetDirection(),0	,	
//								g_pPlayer->GetX(), g_pPlayer->GetY()+4, 0, 1000, NULL, false);
//							ExecuteActionInfoFromMainNode(SKILL_SWEEP_VICE_5,g_pPlayer->GetX(), g_pPlayer->GetY()-4, 0,g_pPlayer->GetDirection(),0	,	
//								g_pPlayer->GetX(), g_pPlayer->GetY()-4, 0, 50, NULL, false);
//							g_pPlayer->SetAction(ACTION_MAGIC);

							
	//						EVENTFLAG_NOT_FADE_SCREEN
						//	g_pPlayer->AddEffectStatus(EFFECTSTATUS_ICICLE_LARGE_AUGER, 100);
						//	ExecuteActionInfoFromMainNode(SKILL_WIDE_ICE_FIELD,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,g_pPlayer->GetDirection(),	0,	
						//				g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 100, NULL, false);	
							
						//	ExecuteActionInfoFromMainNode(SKILL_CLIENT_TRAP_ICICLE_LARGE_AUGER,g_pPlayer->GetX()-5, g_pPlayer->GetY()+5, 0,g_pPlayer->GetDirection(),	0,	
						//		g_pPlayer->GetX()-5, g_pPlayer->GetY()+5, 0, 5, NULL, false);			
							//ExecuteActionInfoFromMainNode(SKILL_ICE_HAIL,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,0,	0,	
							//	g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 10000, NULL, false);			
						//	ExecuteActionInfoFromMainNode(SKILL_ICE_HAIL,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,0,	0,	
						//		g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 10000, NULL, false);			
//							ExecuteActionInfoFromMainNode(SKILL_ICE_WAVE,g_pPlayer->GetX()+5, g_pPlayer->GetY()+5, 0,g_pPlayer->GetDirection(),	0,	
//								g_pPlayer->GetX()+5, g_pPlayer->GetY()+5, 0, 100, NULL, false);			
							//ExecuteActionInfoFromMainNode(SKILL_CLIENT_TRAP_ICE_STICK_SW,g_pPlayer->GetX(), g_pPlayer->GetY()+5, 0,g_pPlayer->GetDirection(),	0,	
							//	g_pPlayer->GetX(), g_pPlayer->GetY()+5, 0, 0, NULL, false);			
						//	UI_RunNotice(7, 100000000);
						//	extern void Add_GDR_Effect(int nEffect, bool bAppearBossMonster);
//						//	Add_GDR_Effect(1,true);	
						//	Add_GDR_Effect(2,true);	
							
//							SetFlagTo( false );
//							TYPE_OBJECTID petID = g_pPlayer->GetPetID();
//							if(petID != OBJECTID_NULL)
//							{
//								MCreature *pCreature = g_pZone->GetFakeCreature(petID);
//								
//								g_pPlayer->SetAction( ACTION_DAMAGED );
//								ExecuteActionInfoFromMainNode(
//									SKILL_CLIENT_TANK_ATTACKED,										// 사용 기술 번호
//									
//									0, 0, 0,
//									DIRECTION_DOWN, // 사용 방향
//									
//									g_pPlayer->GetID(),												// 목표에 대한 정보
//									0, 0, 0, 
//									
//									0,													// 기술의 (남은) 지속 시간		
//									
//									NULL,
//									
//									false);	
//							}
//							g_pPlayer->AddEffectStatus( EFFECTSTATUS_FIRE_ELEMENTAL_ATTACK, 21 );
//							MEvent event;
//							event.eventID = EVENTID_OUSTERS_FIN;
//							event.eventType = EVENTTYPE_ZONE;
//							event.eventFlag = EVENTFLAG_ONLY_EVENT_BACKGROUND | EVENTFLAG_NOT_DRAW_UI | EVENTFLAG_NOT_DRAW_CREATURE | 
//								EVENTFLAG_NOT_DRAW_INFORMATION | EVENTFLAG_NOT_DRAW_CREATURE_SHADOW | EVENTFLAG_NOT_DRAW_ITEM | 
//								EVENTFLAG_NOT_DRAW_MOUSE_POINTER | EVENTFLAG_NOT_DRAW_EFFECT | EVENTFLAG_DENY_INPUT | 
//								EVENTFLAG_NOT_FADE_SCREEN | EVENTFLAG_NOT_PLAY_SOUND;
//							event.parameter1 = 0;
//							event.parameter4 = EVENTBACKGROUNDID_QUEST_2;
//							g_pEventManager->AddEvent(event);		
						}
						// ALT + 2  --> 공격 속도 normal
						if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
						{
							//g_pPlayer->SetWeaponSpeed( MCreature::WEAPON_SPEED_NORMAL);

							// 1 ~ 13
							/*
							if (++gtx_src > 13)
							{
								gtx_src = 1;
							}
							*/
							#ifdef __METROTECH_TEST__
								g_bAutoManaPotion = !g_bAutoManaPotion;
							#endif
						}
					break;

					case DIK_3 :
						if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
						{
//							g_pZone->RemoveTileEffect( g_pPlayer->GetX()+3, g_pPlayer->GetY(), EFFECTSTATUS_DRAGON_TORNADO, 11000 );

	#ifdef __TEST_SUB_INVENTORY__   // add by Coffee 2007-8-9 增加包中包
							MItem* pItem = MItem::NewItem( ITEM_CLASS_SUB_INVENTORY );
							pItem->SetID( rand()%10000 );
							pItem->SetItemType( 0 );
							gC_vs_ui.RunSubInventory( pItem );
	#endif


							//g_pPlayer->SetAction(ACTION_ADVANCEMENT_SLAYER_MAGIC_ATTACK);
							//gC_vs_ui.RunPopupMessage("ComeBackEventNotice.txt", C_VS_UI_POPUP_MESSAGE::POPUP_WELCOME );
//							UI_PopupMessage(UI_STRING_MESSAGE_GET_EVENT_ITEM_NOTICE);
							//g_pUIDialog->PopupFreeMessageDlg( (*g_pGameStringTable)[UI_STRING_MESSAGE_GET_EVENT_ITEM_NOTICE].GetString() );
							//g_pPlayer->SetInvisibleSoon();
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_QUEST_MAGIC_ZONE,16, 16, 0,0,	0,	
//								16, 16, 0, 0xffff, NULL, false);			
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_QUEST_MAGIC_ZONE_POTAL,16, 16, 0,0,	0,	
//								16, 16, 0, 0xffff, NULL, false);			
//							if(gC_vs_ui.IsRunningPowerjjang())
//							{
//								gC_vs_ui.PowerjjangGambleResult(rand()%7);
//							}
//							else
//							gC_vs_ui.RunPowerjjang();
//							if(!g_pPlayer->HasEffectStatus(EFFECTSTATUS_VIEW_HP))
//								g_pPlayer->AddEffectStatus(EFFECTSTATUS_VIEW_HP,0xffffffff);
//							else
//							{
////								ExecuteActionInfoFromMainNode(SKILL_TURRET_FIRE,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,2,	g_pPlayer->GetID(),	
////									g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 200, NULL, true);	
//
//								g_pPlayer->RemoveEffectStatus(EFFECTSTATUS_VIEW_HP);
//							}
//							ExecuteActionInfoFromMainNode(SKILL_VIOLENT_PHANTOM,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,DIRECTION_DOWN,	g_pPlayer->GetID(),	
//								g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 200, NULL, true);	
//							g_pPlayer->SetAction(ACTION_DAMAGED);
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TURRET_LASER_ATTACK,g_pPlayer->GetX(), g_pPlayer->GetY()-4, 0,g_pPlayer->GetDirection(),0	,	
//								g_pPlayer->GetX(), g_pPlayer->GetY()-4, 0, 1000, NULL, false);
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TURRET_LASER_ATTACK,g_pPlayer->GetX()+1, g_pPlayer->GetY()-3, 0,g_pPlayer->GetDirection(),0	,	
//								g_pPlayer->GetX()+1, g_pPlayer->GetY()-3, 0, 1000, NULL, false);
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TURRET_LASER_ATTACK,g_pPlayer->GetX()+2, g_pPlayer->GetY()-2, 0,g_pPlayer->GetDirection(),0	,	
//								g_pPlayer->GetX()+2, g_pPlayer->GetY()-2, 0, 1000, NULL, false);
//							ExecuteActionInfoFromMainNode(SKILL_CLIENT_TURRET_LASER_ATTACK,g_pPlayer->GetX()+3, g_pPlayer->GetY()-1, 0,g_pPlayer->GetDirection(),0	,	
//								g_pPlayer->GetX()+3, g_pPlayer->GetY()-1, 0, 1000, NULL, false);
//뷁      뷁   뷁 뷁
//뷁      뷁   뷁 뷁
//뷁뷁뷁뷁뷁   뷁 뷁 
//뷁      뷁   뷁 뷁
//뷁뷁뷁뷁뷁   뷁 뷁
//             뷁 뷁
//뷁뷁뷁뷁뷁   뷁 뷁
//    뷁   뷁뷁뷁 뷁
//    뷁       뷁 뷁
//뷁뷁뷁뷁뷁뷁뷁뷁뷁
//        뷁      뷁
//뷁뷁뷁뷁뷁      뷁
//뷁              뷁
//뷁뷁뷁뷁뷁      뷁



							//g_pPlayer->AddEffectStatus( EFFECTSTATUS_SIEGE_DEFENDERL, 100 );
//							ExecuteActionInfoFromMainNode(SKILL_WIDE_ICE_HAIL,g_pPlayer->GetX(), g_pPlayer->GetY(), 0,0,	0,	
//								g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 10000, NULL, false);	
//							g_pPlayer->RemoveEffectStatus(EFFECTSTATUS_BLINDNESS);
//							g_pZone->RemoveTileEffect(137, 80,EFFECTSTATUS_ELECTRIC_POST);
//							g_pZone->RemoveTileEffect(143, 86,EFFECTSTATUS_ELECTRIC_POST);
//							g_pZone->RemoveTileEffect(136, 93,EFFECTSTATUS_ELECTRIC_POST);
//							g_pZone->RemoveTileEffect(130, 87,EFFECTSTATUS_ELECTRIC_POST);

//							if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_WATER_ELEMENTAL))
//								g_pPlayer->RemoveEffectStatus( EFFECTSTATUS_WATER_ELEMENTAL );
//							else
//								g_pPlayer->AddEffectStatus( EFFECTSTATUS_WATER_ELEMENTAL, 0xFFFF );

//							g_pMP3->Stop();
//							g_pMP3->Open("data\\music\\dominator of darkness.mp3");
//							g_pMP3->Play( true );
//							g_pSystemMessage->Add("dominator of darkness.mp3 를 연주합니다");
//							g_bTestMusic = false;
						}
						// ALT + 3  --> 공격 속도 Fast
						if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
						{
							//g_pPlayer->SetWeaponSpeed( MCreature::WEAPON_SPEED_FAST );

							// 1 ~ 13
							/*
							if (++gtx_dest > 13)
							{
								gtx_dest = 1;
							}
							*/
							#ifdef __METROTECH_TEST__
								g_iAutoReload = (g_iAutoReload+1)%5;
							#endif
						}
					break;

					case DIK_4 :
						if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
						{
							int TempCount = ShowCursor(FALSE);
//							g_pUserInformation->IsNonPK = !g_pUserInformation->IsNonPK;
//							if(g_pUserInformation->IsNonPK)
//								g_pSystemMessage->Add("PK Not Available");
//							else
//								g_pSystemMessage->Add("PK Available");
							//g_pPlayer->AddEffectStatus( EFFECTSTATUS_WATER_ELEMENTAL_HEAL, 21 );
							//g_pPlayer->AddEffectStatus(EFFECTSTATUS_BLINDNESS, 300);
//							g_pMP3->Stop();
//							g_pMP3->Open("data\\music\\oblivion.mp3");
//							g_pMP3->Play( true );
//							g_pSystemMessage->Add("oblivion.mp3 를 연주합니다");
//							g_bTestMusic = false;
						}
#ifdef __METROTECH_TEST__
						if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
							g_bLight = !g_bLight;
//							g_bHolyWater = !g_bHolyWater;
#endif
						break;
					case DIK_5 :
#ifdef __METROTECH_TEST__
						if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
						UI_RunHorn(g_pZone->GetID());
#endif
//						if (g_pSDLInput->KeyDown(DIK_RCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
//						{
//							g_pMP3->Stop();
//							g_pMP3->Open("data\\music\\rest.mp3");
//							g_pMP3->Play( true );
//							g_pSystemMessage->Add("rest.mp3 를 연주합니다");
//							g_bTestMusic = false;
//						}
						
						//						{
						//							g_pClientConfig->QuestItemColorSet += 15;
						//							
						//							if( g_pClientConfig->QuestItemColorSet >= 495 )
						//								g_pClientConfig->QuestItemColorSet = 0;
						//						} else
						//						{							
						//							g_pClientConfig->QuestItemColorSet += 15;
						//							
						//							if( g_pClientConfig->QuestItemColorSet >= 495 )
						//								g_pClientConfig->QuestItemColorSet = 0;
						//						}
						
						break;

					case DIK_6 :
//						{
//							if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
//							{
//								g_pMP3->Stop();
//								g_pMP3->Open("data\\music\\ruin.mp3");
//								g_pMP3->Play( true );
//								g_pSystemMessage->Add("ruin.mp3 를 연주합니다");
//								g_bTestMusic = false;
//								
//								(*g_pUserOption).BlendingShadow = !(*g_pUserOption).BlendingShadow;
//								if (g_pTopView!=NULL)
//								{
//									g_pTopView->ClearShadowManager();
//								}
//							}				
//						}
#ifdef __METROTECH_TEST__
						if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
						UI_RunMinigame(0);
#endif
						break;
						
					case DIK_7 :
//						{
//							if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
//							{
//								g_pMP3->Stop();
//								g_pMP3->Open("data\\music\\silence of battlefield.mp3");
//								g_pMP3->Play( true );
//								g_pSystemMessage->Add("silence of battlefield.mp3 를 연주합니다");
//								g_bTestMusic = false;
//							}				
//						}
						break;

					case DIK_8 :
#ifdef __METROTECH_TEST__
						if (g_pSDLInput->KeyDown(DIK_RCONTROL) && g_pSDLInput->KeyDown(DIK_RSHIFT))// &&
							//							g_pSDLInput->KeyDown(DIK_LCONTROL) && g_pSDLInput->KeyDown(DIK_RCONTROL))
						{
							g_iSpeed *= -1;
						}
#endif
//						if (g_pSDLInput->KeyDown(DIK_RCONTROL) || g_pSDLInput->KeyDown(DIK_LCONTROL))// &&
//							//							g_pSDLInput->KeyDown(DIK_LCONTROL) && g_pSDLInput->KeyDown(DIK_RCONTROL))
//						{
////							g_iSpeed *= -1;
//							g_pMP3->Stop();
//							g_pMP3->Open("data\\music\\underworld.mp3");
//							g_pMP3->Play( true );
//							g_pSystemMessage->Add("underworld.mp3 를 연주합니다");
//							g_bTestMusic = false;
//						}
						break;
						
					case DIK_9 :
//						if( g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
//						{
//							g_bTestMusic = !g_bTestMusic;
//
//							if(g_bTestMusic)
//							{
//								if (g_CurrentMusicNum >= 8 ) g_CurrentMusicNum = 0;
//								g_pMP3->Stop();
//								g_pMP3->Open(g_musicfilename[g_CurrentMusicNum].c_str());
//								g_pMP3->Play( false );
//							} else
//							{
//								g_pMP3->Stop();								
//							}
//						}

#ifdef __METROTECH_TEST__
						if (g_pSDLInput->KeyDown(DIK_RCONTROL) && g_pSDLInput->KeyDown(DIK_RSHIFT))// &&
//							g_pSDLInput->KeyDown(DIK_LCONTROL) && g_pSDLInput->KeyDown(DIK_RCONTROL))
						{
							int mi = 1;
							if(g_iSpeed < 0)
							{
								mi = -1;
								g_iSpeed *= -1;
							}
							g_iSpeed = (g_iSpeed%4)+1;
							g_iSpeed *= mi;
						}
#endif
						break;
					case DIK_0 :
						{
							if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
							{
								if( g_bTestMusic )
								{
									g_pMP3->Stop();									
								}
							}				
						}

					case DIK_Z :
						if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
						{
							CSDLGraphics::RestoreAllSurfaces();
// CDirect3D::Restore() removed (SDL2)

							if (g_pTopView!=NULL)
							{
								g_pTopView->RestoreSurface();
							}
						}
					break;
					
					case DIK_N :
						{
							if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
							{
								g_pUserInformation->IsNetmarble = !g_pUserInformation->IsNetmarble;
								if(g_pUserInformation->IsNetmarble)
									g_pSystemMessage->Add("Netmarble Set");
								else
									g_pSystemMessage->Add("Netmarble Unset");
							}
							else
								if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
								{
									g_pPCTalkBox->Release();
									
									//---------------------------------------------------
									// normal
									//---------------------------------------------------
									g_pPCTalkBox->SetType( PCTalkBox::NORMAL );
									
									
									int scriptID = 101;
									
									//---------------------------------------------------
									// PC Talk Box의 정보 설정
									//---------------------------------------------------
									// SetContent라고 이름이 되어있지만.. Subject이다. - -;
									g_pPCTalkBox->SetContent( "하하하" );
									g_pPCTalkBox->SetNPCID( 297 );
									g_pPCTalkBox->SetCreatureType( 297 );
									g_pPCTalkBox->SetScriptID( scriptID );
									
									//---------------------------------------------------
									// 각 string 추가
									//---------------------------------------------------
									int contentSize = 2;//g_pNPCScriptTable->GetContentSize( scriptID );
									
									//for (int i=0; i<contentSize; i++)
									{
										// g_PCTalkBox에 추가
										g_pPCTalkBox->AddString( "우헤헤" );
										g_pPCTalkBox->AddString( "크하하" );
									}
									
									g_pUIDialog->PopupPCTalkDlg();
								}
						}
						break;
						
					case DIK_M :
						if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
						{
							if(g_pEventManager->IsEvent(EVENTID_METEOR))
							{
								g_pEventManager->RemoveEvent(EVENTID_METEOR);
							}
							else
							{
								MEvent event;
								event.eventID = EVENTID_METEOR;
								event.eventType = EVENTTYPE_ZONE;
								event.eventFlag = EVENTFLAG_FADE_SCREEN;
								event.parameter2 = 30 << 16;
//								event.parameter4 = EVENTBACKGROUNDID_COSMOS;
								g_pEventManager->AddEvent(event);
							}
						}
						break;

					case DIK_G:
						if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
						{
//							MFakeCreature* pFakeCreature = NewFakeCreature( g_pPlayer, g_pPlayer->GetX(), g_pPlayer->GetY() );
//							
//							pFakeCreature->SetFakeCreatureType( MFakeCreature::FAKE_CREATURE_FAST_MOVE_ACTION );
//							
//							pFakeCreature->SetFakeCreatureFastMoveAction(g_pPlayer->GetX()+10, g_pPlayer->GetY()+10, 0, 0);
//							
//							//------------------------------------------------------
//							// Fake Creature를 Zone에 추가
//							//------------------------------------------------------
//							if (!g_pZone->AddFakeCreature( pFakeCreature ))
//							{
//								delete pFakeCreature;
//							}
						}
						break;

					case DIK_H :
					{
						if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
						{
							if (g_pPlayer->HasEffectStatus(EFFECTSTATUS_HALLUCINATION))
							{
								g_pPlayer->RemoveEffectStatus(EFFECTSTATUS_HALLUCINATION);
							}
							else
							{
								g_pPlayer->AddEffectStatus(EFFECTSTATUS_HALLUCINATION, 16*15);
							}
						}
					}
					break;
				#endif
				
			#endif


			#if defined(OUTPUT_DEBUG) && defined(_DEBUG)	//!defined(CONNECT_SERVER)
				/*
				case DIK_L :
					if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
					{
						if (g_pPlayer->IsVampire())
						{
							#ifdef OUTPUT_DEBUG
								#ifdef CONNECT_SERVER
									int value = g_pPlayer->GetBonusPoint();

									if (value > 0)
									{
										int strValue = value/4;
										int intValue = value - strValue;

										// NPC 처리 packet을 보낸다.
										CGUseBonusPoint _CGUseBonusPoint;

										// 1/4은 STR을 올린다.
										_CGUseBonusPoint.setWhich( INC_STR );

										for (int i=0; i<strValue; i++)
										{
											g_pSocket->sendPacket( &_CGUseBonusPoint );

											#if defined(OUTPUT_DEBUG) && defined(__DEBUG_OUTPUT__)
													DEBUG_ADD_FORMAT( "[Send] %s", _CGUseBonusPoint.toString().c_str() );							
											#endif
										}
										
										// 3/4는 INT를 올린다.
										_CGUseBonusPoint.setWhich( INC_INT );

										for (int i=0; i<strValue; i++)
										{
											g_pSocket->sendPacket( &_CGUseBonusPoint );

											#if defined(OUTPUT_DEBUG) && defined(__DEBUG_OUTPUT__)
													DEBUG_ADD_FORMAT( "[Send] %s", _CGUseBonusPoint.toString().c_str() );							
											#endif
										}	
									}

									
								#endif			
							#endif
						}
					}
				break;
				*/				

				//------------------------------------------------------------
				//
				// 음악 볼륨 조절
				//
				//------------------------------------------------------------				
				case DIK_SUBTRACT :
				{
					if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
					{
						int volume = g_Music.GetVolume();

						DEBUG_ADD_FORMAT("[MIDI] Volume = %x", volume);

						volume -= 0x1000;

						if (volume < 0)
						{
							volume = 0;
						}

						g_Music.Pause();
						g_Music.SetVolume( volume );
						g_Music.Resume();
						
					}
					
				}
				break;
				
				case DIK_ADD :
				{
					if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
					{
						int volume = g_Music.GetVolume();

						DEBUG_ADD_FORMAT("[MIDI] Volume = %x", volume);

						volume += 0x1000;

						if (volume > 0xFFFF)
						{
							volume = 0xFFFF;
						}

						g_Music.Pause();
						g_Music.SetVolume( volume );
						g_Music.Resume();
						
					}
					
				}
				break;
				
				//------------------------------------------------------------
				//
				// wide chatting
				//
				//------------------------------------------------------------
				/*
				case DIK_E : 
					if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
					{
						if (g_HISTORY_LINE > 4)//HISTORY_LINE) ㅋㅋ
						{
							g_HISTORY_LINE = 4;
						}
						else
						{
							g_HISTORY_LINE = 16;
						}
					}
					//---------------------------------------------------------
					// Wolf 변신 test
					//---------------------------------------------------------
					else if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
					{
						MActionResult* pResult = new MActionResult;

						if (g_pPlayer->GetCreatureType()!=186)
						{									
							pResult->Add( new MActionResultNodeChangeCreatureType( g_pPlayer->GetID(), 186 ) );
							//g_pPlayer->SetCreatureType( 185 );
							//g_pPlayer->SetFlyingCreature();
						}
						else
						{
							pResult->Add( new MActionResultNodeChangeCreatureType( g_pPlayer->GetID(), CREATURETYPE_VAMPIRE_MALE ) );
							//g_pPlayer->SetCreatureType( CREATURETYPE_VAMPIRE_MALE );
							//g_pPlayer->SetGroundCreature();
						}

						//--------------------------------------------------
						// 늑대 변신 test
						//--------------------------------------------------								
						ExecuteActionInfoFromMainNode(
							RESULT_MAGIC_TRANSFORM_TO_WOLF,										// 사용 기술 번호
						
							g_pPlayer->GetX(), g_pPlayer->GetY(), 0,
							g_pPlayer->GetDirection(),														// 사용 방향
							
							OBJECTID_NULL,												// 목표에 대한 정보
							g_pPlayer->GetX(), g_pPlayer->GetY(), 0, 
							
							0,													// 기술의 (남은) 지속 시간		
							
							pResult, //NULL,
							
							false);			// 기술 첨부터 시작한다.

						g_pPlayer->SetDelay( 1000 );
					}

				break;
				*/
				//------------------------------------------------------------
				// 헬기
				//------------------------------------------------------------
				/*
				case DIK_H :
				{
					// helicopter 테스트
					if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
					{
						TYPE_OBJECTID creatureID = g_pPlayer->GetID();

						if (g_pZone->GetHelicopter( creatureID )==NULL)
						{
							g_pZone->AddHelicopter( creatureID, 0, 0 );
						}
						else
						{
							g_pZone->RemoveHelicopter( creatureID );
						}
					}
				}
				break;
				*/

				//------------------------------------------------------------
				// Warp
				//------------------------------------------------------------
				case DIK_W :
				{
					if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
					{
						int x, y;

						while (1)
						{
							x = rand()%g_pZone->GetWidth();
							y = rand()%g_pZone->GetHeight();

							if (!g_pZone->GetSector(x,y).IsBlockGround())
							{
								break;
							}
						}

						g_pPlayer->SetStop();						
						g_pPlayer->MovePosition( x, y );
					}
				}
				break;

				//------------------------------------------------------------
				//
				//			Portal
				// 
				//------------------------------------------------------------
				/*
				case DIK_P : 
				{					
					//------------------------------------------------------------
					// 왼쪽 누르면 들어가기
					//------------------------------------------------------------
					if (g_pSDLInput->KeyDown(DIK_LMENU))
					{
						MZone::CREATURE_MAP::const_iterator iCreature = g_pZone->GetCreatureBegin();

						for (int i=0; i<g_pZone->GetCreatureNumber(); i++)
						{
							MCreature* pCreature = (*iCreature).second;

							//--------------------------------------------------
							// Player, NPC가 아닌 경우
							//--------------------------------------------------					
							if (pCreature->GetID()!=g_pPlayer->GetID()
								&& pCreature->IsVampire()
								&& !pCreature->IsNPC())
							{					
								AddVampirePortal(
										0,								// 이펙트의 OID
										pCreature->GetName(),								// 포탈 주인
										pCreature->GetX(), pCreature->GetY(),		// 포탈의 좌표
										16*10,							// 포탈의 지속 시간
										
										// 목표 정보
										rand()%4+11,							// 포탈의 목표 존 ID
										rand()%255,			// 포탈의 목표 좌표 x
										rand()%255,			// 포탈의 목표 좌표 y

										(rand()%2? true : false) );

								MoveIntoPortal( pCreature );

								g_pZone->RemoveCreature( pCreature->GetID() );
								
								break;
							}

							iCreature++;
						}
					}
					//------------------------------------------------------------
					// 오른쪽 누르면 나오기
					//------------------------------------------------------------
					else if (g_pSDLInput->KeyDown(DIK_RMENU))
					{
						MCreature* pCreature = AddClientCreature();

						if (pCreature!=NULL
							&& pCreature->IsVampire()
							&& !pCreature->IsNPC())
						{
							AddVampirePortal(
										0,								// 이펙트의 OID
										pCreature->GetName(),								// 포탈 주인
										pCreature->GetX(), pCreature->GetY(),		// 포탈의 좌표
										16*10,							// 포탈의 지속 시간
										
										// 목표 정보
										rand()%4+11,							// 포탈의 목표 존 ID
										rand()%255,			// 포탈의 목표 좌표 x
										rand()%255,			// 포탈의 목표 좌표 y

										(rand()%2? true : false) );

							ComeFromPortal( pCreature );
						}
					}					
				}
				break;				
				*/

				//------------------------------------------------------------
				//
				// casket
				//
				//------------------------------------------------------------
				case DIK_V : 
				{
					if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
					{
						MZone::CREATURE_MAP::const_iterator iCreature = g_pZone->GetCreatureBegin();

						for (int i=0; i<g_pZone->GetCreatureNumber(); i++)
						{
							MCreature* pCreature = (*iCreature).second;

							//--------------------------------------------------
							// Player, NPC가 아닌 경우
							//--------------------------------------------------					
							if (pCreature->GetID()!=g_pPlayer->GetID()
								&& pCreature->IsVampire()
								&& !pCreature->IsNPC())
							{							
								if (pCreature->IsInCasket())
								{
									pCreature->RemoveCasket();
								}
								else
								{
									pCreature->AddCasket( rand()%3 );
								}							
							}

							iCreature++;
						}
					}
				}
				break;

				case DIK_C : 
				{
					if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
					{
						if (g_pPlayer->IsInCasket())
						{
							g_pPlayer->RemoveCasket();
						}
						else
						{
							g_pPlayer->AddCasket( rand()%3 );
						}

						/*
						int x = g_pPlayer->GetX();
						int y = g_pPlayer->GetY();
						int dir = g_pPlayer->GetDirection();

						MFakeCreature* pCreature = new MFakeCreature;

						pCreature->SetID( 0 );
						pCreature->SetSameBody( g_pPlayer );
						pCreature->SetSameWear( g_pPlayer );
						
						pCreature->SetZone( g_pZone );

						pCreature->SetPosition( x, y );
						pCreature->SetServerPosition( x, y );
						pCreature->SetDirection( dir );
						pCreature->SetCurrentDirection( dir );
						pCreature->SetAction( ACTION_ATTACK );
						pCreature->SetWeaponSpeed( g_pPlayer->GetWeaponSpeed() );

						pCreature->SetFakeCreatureType( MFakeCreature::FAKE_CREATURE_FADE_ACTION );						

						if (!g_pZone->AddFakeCreature( pCreature ))
						{
							delete pCreature;
						}
						*/
					}
				}
				break;

				//------------------------------------------------------------
				//
				// 컴퓨터 띄우기
				//
				//------------------------------------------------------------
				case DIK_D : 
					if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
					{
						UI_RunComputer();
					}					
				break;

				//------------------------------------------------------------
				//
				// 캐릭터 추가 - packet 테스트를 위해서
				//
				//------------------------------------------------------------
				case DIK_K : 
				{
					if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
					{
						int objectID = MFakeCreature::GetFakeID();
						char name[80];
						SafeFormat::Format(name, "Fake%d", objectID);
						int x = g_pPlayer->GetX() + rand()%30-15;
						int y = g_pPlayer->GetY() + rand()%30-15;
						int dir = rand()%8;

						if (x >=0 && x < g_pZone->GetWidth()
							&& y >= 0 && y < g_pZone->GetHeight())
						{
							MCreature* pCreature = g_pZone->GetCreature( objectID );

							if (pCreature!=NULL)
							{
								break;
							}


							//------------------------------------------------------------
							// Vampire 추가
							//------------------------------------------------------------
							if (rand()%2)
							{
								pCreature = new MCreatureWear;

								pCreature->SetZone( g_pZone );
								
								//--------------------------------------------------
								// CreatureType 설정
								//--------------------------------------------------
								if (rand()%3)
								{
									pCreature->SetCreatureType( CREATURETYPE_BAT );
									pCreature->SetFlyingCreature();
								}
								else
								{
									pCreature->SetCreatureType( (rand()%2? CREATURETYPE_VAMPIRE_MALE1 : CREATURETYPE_VAMPIRE_FEMALE1) );
								}

								pCreature->SetMale( pCreature->GetCreatureType()==CREATURETYPE_VAMPIRE_MALE1 );						
								

								pCreature->SetName( name );

								// 임시로
								pCreature->SetGuildNumber( 2 );

								pCreature->SetGroundCreature();
								pCreature->SetID( objectID );
								//pCreature->SetAction(ACTION_MOVE);
								
								pCreature->SetPosition( x, y );
								pCreature->SetServerPosition( x, y );
								pCreature->SetDirection( dir );
								pCreature->SetCurrentDirection( dir );
								pCreature->SetAction( ACTION_STAND );

								// 색깔
								pCreature->SetBodyColor1( rand()%MAX_COLORSET );
								pCreature->SetBodyColor2( rand()%MAX_COLORSET );
								
								pCreature->SetStatus( MODIFY_MAX_HP, 80 );
								pCreature->SetStatus( MODIFY_CURRENT_HP, 100 );
								pCreature->SetStatus( MODIFY_ALIGNMENT, 7500 );

								pCreature->SetWeaponSpeed( 70 );							

								if (!g_pZone->AddCreature( pCreature ))
								{
									delete pCreature;
								}
							}
							//------------------------------------------------------------
							// Slayer 추가
							//------------------------------------------------------------
							else
							{
								MCreatureWear* pCreatureWear = new MCreatureWear;

								pCreatureWear->SetZone( g_pZone );

								pCreatureWear->SetCreatureType( (rand()%2)? CREATURETYPE_SLAYER_MALE : CREATURETYPE_SLAYER_FEMALE );
								pCreatureWear->SetGroundCreature();

								
								
								pCreatureWear->SetID( objectID );
								//pCreatureWear->SetAction(ACTION_MOVE);
								pCreatureWear->SetPosition( x, y );
								pCreatureWear->SetServerPosition( x, y );
								pCreatureWear->SetDirection( dir );
								pCreatureWear->SetCurrentDirection( dir );
								pCreatureWear->SetAction( ACTION_STAND );

								// 피부색
								pCreatureWear->SetBodyColor2( rand()%MAX_COLORSET );

								pCreatureWear->SetStatus( MODIFY_MAX_HP, 80 );
								pCreatureWear->SetStatus( MODIFY_CURRENT_HP, 100 );

								// 이름
								pCreatureWear->SetName( name );

								// 임시로 NPC복장을 입힌다.
								SetAddonToSlayer( pCreatureWear, 250 );

								
								
								if (!g_pZone->AddCreature( pCreatureWear ))
								{
									delete pCreatureWear;
								}
							}
						}
					}
				}
				break;
				

				//-------------------------------------------------------------
				// 방향 바꾸기
				//-------------------------------------------------------------
				case DIK_LEFT :
				case DIK_RIGHT :
				{
					MZone::CREATURE_MAP::const_iterator iCreature = g_pZone->GetCreatureBegin();

					for (int i=0; i<g_pZone->GetCreatureNumber(); i++)
					{
						MCreature* pCreature = (*iCreature).second;

						//--------------------------------------------------
						// Player, NPC가 아닌 경우
						//--------------------------------------------------					
						if (pCreature->GetID()!=g_pPlayer->GetID()
							&& !pCreature->IsNPC())
						{							
							int dir = pCreature->GetDirection();

							if (key==DIK_LEFT)
							{
								dir = (dir + 7) % 8;
							}
							else
							{
								dir = (dir + 1) % 8;
							}

							pCreature->SetDirection( dir );									
						}

						iCreature++;
					}
				}
				break;

				//------------------------------------
				// HP 회복 test
				//------------------------------------
				case DIK_R : 
					if (g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU))
					{
						g_pPlayer->SetStatus( MODIFY_CURRENT_HP, 0 );
						g_pPlayer->SetStatus( MODIFY_MAX_HP, 100 );
						g_pPlayer->SetRecoveryHP( 1, 100, 200 );
					}
				break;


				//------------------------------------------------------------
				//
				// zone 이동 자동..
				//
				//------------------------------------------------------------
				case DIK_LBRACKET : case DIK_RBRACKET :
				break;						

				/*
				case DIK_GRAVE :
					if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
					{
						(*g_pUIDialog).PopupDirectionSelectDlg(0, 0);
					}					
				break;
				*/

				

//				case DIK_5 :
//				{	
//					if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
//					{
//						g_pPlayer->SetMoveDevice( MCreature::MOVE_DEVICE_WALK );
//						g_pPlayer->SetBasicActionInfo( SKILL_ATTACK_MELEE );
//						g_pPlayer->SetStop();
//								
//						if (g_pPlayer->IsSlayer())
//						{
//							if (g_pPlayer->IsMale())
//							{
//								g_pPlayer->SetCreatureType( CREATURETYPE_SLAYER_FEMALE );
//							}
//							else
//							{
//								g_pPlayer->SetCreatureType( CREATURETYPE_SLAYER_MALE );
//							}
//						}
//						else
//						{
//							if (g_pPlayer->IsMale())
//							{
//								g_pPlayer->SetCreatureType( CREATURETYPE_VAMPIRE_FEMALE1 );
//							}
//							else
//							{
//								g_pPlayer->SetCreatureType( CREATURETYPE_VAMPIRE_MALE1 );
//							}
//						}
//						/*
//						if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
//							for (int i=0; i<8; i++)
//							{
//								const MItem* pItem = g_Inventory.GetItem(i, 0);
//
//								if (pItem!=NULL)
//								{
//									AddNewInventoryEffect( pItem->GetID(),
//														RESULT_MAGIC_CREATE_HOLY_WATER,
//														80	// 5초
//													);
//								}
//							}
//							*/
//					}
//				}
//				break;

				

//				case DIK_8 :
//				{
//					if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
//					{
//						if (g_pPlayer->HasEffectStatus(EFFECTSTATUS_SNIPPING_MODE))
//						{
//							g_pPlayer->RemoveEffectStatus(EFFECTSTATUS_SNIPPING_MODE);
//						}
//						else
//						{
//							g_pPlayer->AddEffectStatus(EFFECTSTATUS_SNIPPING_MODE, 16*10);
//							g_pPlayer->SetInvisible();
//						}
//					}				
//				}
//				break;

//				case DIK_9 :
//				{
//					if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
//					{
//						if (g_pPlayer->HasEffectStatus(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS))
//						{
//							g_pPlayer->RemoveEffectStatus(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS);
//						}
//						else
//						{
//							g_pPlayer->AddEffectStatus(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS, 16*10);
//						}
////						if (g_pPlayer->HasEffectStatus(EFFECTSTATUS_SHUT_DOWN))
////						{
////							g_pPlayer->RemoveEffectStatus(EFFECTSTATUS_SHUT_DOWN);
////						}
////						else
////						{
////							g_pPlayer->AddEffectStatus(EFFECTSTATUS_SHUT_DOWN, 16*10);
////						}
//					}				
//				}
//				break;

				

				/*
				case DIK_1 :
					if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
					{
						MItem*	pBlade = new MBlade;	pBlade->SetItemType( 0 ); pBlade->SetItemOption( 0 );
						g_pPlayer->RemoveAddon( ADDON_RIGHTHAND );
						g_pPlayer->SetAddonItem( pBlade );				
					}
				break;

				case DIK_2 :
					if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
					{
						MItem*	pSword = new MSword;	pSword->SetItemType( 0 );	pSword->SetItemOption( 0 );
						g_pPlayer->RemoveAddon( ADDON_RIGHTHAND );
						g_pPlayer->SetAddonItem( pSword );
					}				
				break;

				case DIK_7 :
				case DIK_8 :
				case DIK_9 :
					if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
					{
						static int itemType[] = { 0, 1, 5 };
						MItem*	pShield = new MShield;	pShield->SetItemType( itemType[key-DIK_7] ); pShield->SetItemOption( 0 );
						g_pPlayer->RemoveAddon( ADDON_LEFTHAND );
						g_pPlayer->SetAddonItem( pShield );				
					}
				break;

				case DIK_3 :			
				case DIK_4 :
				case DIK_5 :			
				case DIK_6 :
				{
					if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
					{
						MGunItem* pGunItem;

						#ifdef __FIX_GUNFRAME__
							int ac;
						#endif
						
						switch (key)
						{
							case DIK_3 :
								pGunItem = new MGunTR;
								#ifdef __FIX_GUNFRAME__
									ac = SKILL_ATTACK_GUN_TR;						
								#endif
							break;

							case DIK_4 :
								pGunItem = new MGunSMG;
								#ifdef __FIX_GUNFRAME__
									ac = SKILL_ATTACK_GUN_SMG;						
								#endif
							break;

							case DIK_5 :
								pGunItem = new MGunAR;
								#ifdef __FIX_GUNFRAME__
									ac = SKILL_ATTACK_GUN_AR;						
								#endif
							break;

							case DIK_6 :
								pGunItem = new MGunSG;
								#ifdef __FIX_GUNFRAME__
									ac = SKILL_ATTACK_GUN_SG;						
								#endif
							break;

							
						}

						#ifdef __FIX_GUNFRAME__
							// 임시 총 불꽃
							g_pPlayer->ClearAttachEffect();
							MAttachEffect* pEffect = g_pPlayer->CreateAttachEffect( 
																(*g_pActionInfoTable)[ac].GetActionEffectSpriteType(), 
																//(*g_pActionInfoTable)[m_nUsedActionInfo].GetDelay()
																0xFFFF
																);	// 지속 시간

							if (pEffect != NULL)
							{
								pEffect->SetDirection( g_pPlayer->GetDirection() );

								pEffect->SetLink( ac, NULL );

								// 붙어야 하는 캐릭터
								pEffect->SetAttachCreatureID( 1 );			
							}
						#endif

						pGunItem->SetItemType( 0 ); pGunItem->SetItemOption( 0 );				
					
						MMagazine* pMagazine = (MMagazine*)MItem::NewItem( (ITEM_CLASS)ITEM_CLASS_MAGAZINE );

						// 의미 없음 - -;
						pMagazine->SetID( 0 );

						// 이거는 총에 맞춰서 해줘야된다.
						for (int j=0; j<g_ItemTable[ITEM_CLASS_MAGAZINE].GetSize(); j++)			
						{
							pMagazine->SetItemType(	j );

							if (pMagazine->IsInsertToItem( pGunItem ))
							{
								break;
							}
						}

						// 의미 없음
						pMagazine->SetItemOption( 0 );

						// 탄창 개수
						pMagazine->SetNumber( 0xFFFF );

						//------------------------------------
						// 탄창 설정
						//------------------------------------
						pGunItem->SetMagazine( pMagazine );

						g_pPlayer->RemoveAddon( ADDON_RIGHTHAND );
						g_pPlayer->RemoveAddon( ADDON_LEFTHAND );
						g_pPlayer->SetAddonItem( pGunItem );
					}
				}
				break;
				*/

				case DIK_A :
					if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
					{
						//------------------------------------------------------
						// 사용자
						//------------------------------------------------------
						MCreature* pCreature = g_pZone->GetCreature( 1000 );

						if (pCreature!=NULL)
						{
							//------------------------------------------------------
							// TileOK2로 인한 결과 
							//------------------------------------------------------
							MActionResult* pResult = new MActionResult;
	
							pResult->Add( new MActionResultNodeActionInfo( 
														MAGIC_LIGHT,
														pCreature->GetID(),
														g_pPlayer->GetID(), 
														g_pPlayer->GetX(),
														g_pPlayer->GetY()
														) 
										);
								
							//------------------------------------------------------
							// TileOK2로 인한 결과 추가
							//------------------------------------------------------
							//Duration_t	m_Duration;
							pCreature->PacketSpecialActionToSector(
												MAGIC_LIGHT, 
												g_pPlayer->GetX(), g_pPlayer->GetY(),
												pResult						// 결과
							);

							// 방향을 바라보기
							pCreature->SetDirectionToPosition(g_pPlayer->GetX(), g_pPlayer->GetY());
					
						}
					}
				break;


				/*		
				case DIK_Q :								
					if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
					{
						if (g_pPlayer->IsDead())
						{						
							g_pPlayer->SetAlive();
							if (g_pTopView!=NULL)
							{
								g_pTopView->SetFadeEnd();
							}
						}
						else
						{
							// 죽은 경우..
							g_pPlayer->SetDead();						
						}
					}
				break;

				case DIK_W :								
					if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL))
					{
						for (int i=0; i<g_MaxNPC; i++)
						{
							MCreature* pCreature = g_pZone->GetCreature(1000+i);

							if (pCreature!=NULL)
							{
								if (pCreature->IsDead())
								{
									pCreature->SetAlive();
								}
								else
								{
									pCreature->SetDead();
								}
							}
						}
					}
				break;
				*/
			#endif			
		}
	}
	else // if (event==CSDLInput::KEYUP)
	{
	}
}



//-----------------------------------------------------------------------------
// ProcessInput RButton Down
//-----------------------------------------------------------------------------
// pObject와 관련..이 있을 수도 있고..
//-----------------------------------------------------------------------------
void
ProcessInputRButtonDown(MObject* pObject, bool bForceAttack = false)
{
	g_bRButtonDown = TRUE;
	
	// 선택된 sector가 없게 한다.
	if (g_pTopView!=NULL)
	{
		g_pTopView->SetSelectedSectorNULL();
	}
	// add by Sonic
		//START_START
		//CRYPT_START
#ifdef PLATFORM_WINDOWS
		// Windows-specific anti-cheat error check - Windows only
		if(g_CheckErrorTime>=5)
		{
			g_CheckErrorTime=0;
			SetMode(MODE_QUIT);
			return;
		}
#endif // PLATFORM_WINDOWS
		//VM_END
		//CRYPT_END
	// end 
//	UI_RunRaceWarNotice(0xFFFFFFFF);
	
	//bool bFreePKZone = g_pZoneTable->Get( g_pZone->GetID() )->FreePK;
	if(NULL != pObject)
		bForceAttack |= (g_pObjectSelector->IsWarEnemy( (MCreature*) pObject ) == TRUE);
	//bForceAttack |= ((g_pZone->GetID() == 1303)||(g_pZone->GetID() == 1020));
//	bForceAttack |= g_pZone->IsFreePKZone();

	if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_DRAGON_EYES))
		return;
	if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
	{
		if(!g_pPlayer->IsStop()) return;
		if(g_pPlayer->GetSpecialActionInfo() == MAGIC_UN_TRANSFORM)
		{
		//	if(!g_pPlayer->IsStop()) return;
			CGSkillToSelf _CGSkillToSelf;
			_CGSkillToSelf.setSkillType( MAGIC_UN_TRANSFORM );
			g_pSocket->sendPacket( &_CGSkillToSelf );
		//	g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_SYLPH_SUMMON_GETOFF );
			return ;
		}
		else if(g_pPlayer->GetSpecialActionInfo() == SKILL_TURRET_FIRE)
			PlaySound( SOUND_SLAYER_TURRET_FIRE_START );
	}
	if( HasEffectStatusSummonSylph( dynamic_cast<MCreature*>(g_pPlayer) )
//		&& //g_pPlayer->HasEffectStatus( EFFECTSTATUS_SUMMON_SYLPH ) && 
//		!g_bZoneSafe && !g_pPlayer->IsInSafeSector() 
		) 
	{ 
		if(!g_pPlayer->IsStop()) return;
		
		if(!g_bZoneSafe && !g_pPlayer->IsInSafeSector())
		{
			CGSkillToSelf _CGSkillToSelf;
			_CGSkillToSelf.setSkillType( MAGIC_UN_TRANSFORM );
	//		_CGSkillToSelf.setCEffectID( m_pEffectTarget->GetEffectID() );
			g_pSocket->sendPacket( &_CGSkillToSelf );
			g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_SYLPH_SUMMON_GETOFF );
			if (auto* entry = g_pSkillInfoTable->GetMutable(SKILL_SUMMON_SYLPH)) {
				entry->SetAvailableTime( 4000 );
			}
	//		gC_vs_ui.UnselectSkill();
		}
		else 
			return;
	} else
	//---------------------------------------------------------------
	// 
	// 오토바이를 타고 있는 경우 
	//
	//---------------------------------------------------------------
	// 오토바이에서 내린다.
	//---------------------------------------------------------------
	if (g_pPlayer->GetMoveDevice()==MCreature::MOVE_DEVICE_RIDE
		&& g_pPlayer->IsStop()
		&& !g_pPlayer->HasEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS)
		&& !g_pPlayer->HasEffectStatus(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS)
		&& !g_pPlayer->HasEffectStatus(EFFECTSTATUS_EXPLOSION_WATER)
		)
	{
#ifdef OUTPUT_DEBUG_PROCESS_INPUT
		DEBUG_ADD("MotorOff");
#endif
	
		if( ! IsExistCorpseFromPlayer(dynamic_cast<MCreature*>(g_pPlayer), 670) )
		{		
			CGGetOffMotorCycle _CGGetOffMotorCycle;
			
			_CGGetOffMotorCycle.setObjectID( 0 );
			
			g_pSocket->sendPacket(&_CGGetOffMotorCycle);
			
#ifdef __DEBUG_OUTPUT__
			DEBUG_ADD_FORMAT("[Send] %s", _CGGetOffMotorCycle.toString().c_str() );
#endif
			
			g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_MOTORCYCLE_GETOFF );
		} else
		{
			g_pSystemMessage->Add((*g_pGameStringTable)[UI_STRING_MESSAGE_CANNOT_ACTION_MOTORCYCLE_FLAG].GetString() );
		}
	}
	//---------------------------------------------------------------
	//
	// Burrow 된 경우 땅으로 솟아나온다는 packet을 보낸다.
	//
	//---------------------------------------------------------------
	else if (g_pPlayer->IsUndergroundCreature())
	{
		#ifdef OUTPUT_DEBUG_PROCESS_INPUT
			DEBUG_ADD("Unburrow");
		#endif

		CGUnburrow _CGUnburrow;

		_CGUnburrow.setX( static_cast<Coord_t>(g_pPlayer->GetX()) );
		_CGUnburrow.setY( static_cast<Coord_t>(g_pPlayer->GetY()) );
		_CGUnburrow.setDir( g_pPlayer->GetDirection() );

		g_pSocket->sendPacket(&_CGUnburrow);

		#ifdef __DEBUG_OUTPUT__
			DEBUG_ADD_FORMAT("[Send] %s", _CGUnburrow.toString().c_str() );
		#endif

		g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_SKILL_SUCCESS );
	}
	//---------------------------------------------------------------
	//
	// object/tile에 기술을 사용하는 경우 
	//
	//---------------------------------------------------------------
	else
	{
		#ifdef OUTPUT_DEBUG_PROCESS_INPUT
			DEBUG_ADD("TileSkill");
		#endif

		// sector 선택하기
		//g_SelectSector = g_pTopView->GetSelectedSector(g_x, g_y);
		//g_pTopView->SetSelectedSector(g_SelectSector.x, g_SelectSector.y);

		//
		//MObject*	pObject = g_pTopView->GetSelectedObject(g_x, g_y);
	
		//--------------------------------------------------
		// 선택한 곳에 Object가 없으면 ..
		//--------------------------------------------------
		if (pObject == NULL)
		{
			// [SKILLREPEAT DIAG] kept - a missed pick on the press edge.
			// This is the path the 60 fps picking regression came in through.
			if (g_pPlayer->IsRepeatAction())
				DEBUG_ADD_FORMAT("[SKILLREPEAT] why=RBDOWN_NOOBJ camGap=(%d,%d) cursor=(%d,%d)",
					g_DrawCamGapX, g_DrawCamGapY, g_x, g_y);

			g_pPlayer->UnSetRepeatAction();
			//g_pPlayer->TraceNextNULL();

			//--------------------------------------------------
			// 자신한테 특수 기술 사용
			//--------------------------------------------------
			if (g_pPlayer->SelfSpecialAction())
			{
				g_pPlayer->SetRepeatAction();
			}
			//--------------------------------------------------
			// 자신한테 사용하는 특수 기술이 아닌 경우
			//--------------------------------------------------				
			else if (!g_pPlayer->IsSpecialActionInfoTargetSelf())
			{
				g_SelectSector = g_pTopView->GetSelectedSector(g_x, g_y);

				//--------------------------------------------------				
				// Zone에 특수 기술을 사용한다.
				//--------------------------------------------------				
				if (g_pPlayer->TraceSectorToSpecialAction( static_cast<TYPE_SECTORPOSITION>(g_SelectSector.x), static_cast<TYPE_SECTORPOSITION>(g_SelectSector.y) ))
				{
					// 선택된 Sector로 표시한다.
					g_pTopView->SetSelectedSector( g_SelectSector );
				
					g_pPlayer->SetRepeatAction();

					g_bPreviousMove = false;
				}
			}

			//----------------------------------
			// Tile이나 자신에게 하는 행동 반복 설정
			//----------------------------------						
			g_pTopView->SetSelectedSectorNULL();
			
		}		
		//--------------------------------------------------
		// 선택된 Object에 대해서 
		//--------------------------------------------------
		else 
		{				
			#ifdef OUTPUT_DEBUG_PROCESS_INPUT
				DEBUG_ADD("Obj");
			#endif

			switch (pObject->GetObjectType())
			{	
				//--------------------------------------------------
				// Creature
				//--------------------------------------------------
				case MObject::TYPE_CREATURE :		
				{
					// 목표가 되는 Creature
					MCreature* pCreature = ((MCreature*)pObject);
					
					//--------------------------------------------------
					// 다른 애덜에게 사용하는건가?
					//--------------------------------------------------
					if (g_pPlayer->TraceCreatureToSpecialAction( pCreature->GetID(), bForceAttack ))
					{
						g_pPlayer->SetRepeatAction();								

						g_bPreviousMove = false;
					}
					else if (!g_pPlayer->IsSpecialActionInfoTargetOther())
					{
						//--------------------------------------------------
						// (!) 다른 Creature에 사용하는게 아닐 경우
						//--------------------------------------------------
						// 자신한테 특수 기술 사용
						if (g_pPlayer->SelfSpecialAction())
						{
							g_pPlayer->SetRepeatAction();
						}
						else
						{	
							//--------------------------------------------------
							// 자신에게 사용하는게 아닌 경우
							//--------------------------------------------------
							// 선택됐던 Creature의 좌표의 ..
							// Zone에 특수 기술을 사용할까?
							if (!g_pPlayer->IsSpecialActionInfoTargetSelf()
								&& g_pPlayer->TraceSectorToSpecialAction( pCreature->GetX(), pCreature->GetY()))
							{
								g_pPlayer->SetRepeatAction();

								g_bPreviousMove = false;
							}
						}									
					}
					
					//----------------------------------
					// 선택 sector표시 없애기
					//----------------------------------
					g_pTopView->SetSelectedSectorNULL();
				}
				break;

				//--------------------------------------------------
				// Item
				//--------------------------------------------------
				// Item일 경우는.. 자신한테 사용하거나 Zone에 사용한다.
				case MObject::TYPE_ITEM :		
				{
					//--------------------------------------------------
					// 자신한테 특수 기술 사용
					//--------------------------------------------------
					if (g_pPlayer->SelfSpecialAction())
					{
						g_pPlayer->SetRepeatAction();
					}
					else
					{	
						//--------------------------------------------------
						// 자신한테 사용하는게 아닌 경우..
						//--------------------------------------------------
						// 목표가 되는 Creature
						if (!g_pPlayer->IsSpecialActionInfoTargetSelf())
						{
							MItem* pItem = ((MItem*)pObject);

							if (pItem!=NULL)
							{
								// 와일드 울프를 시체한테 썼을때 예외 처리
								if(g_pPlayer->GetSpecialActionInfo() == SKILL_WILD_WOLF)
								{
									g_pPlayer->UseWildWolf_Corpse(pItem);
								}
								// 선택됐던 Creature의 좌표의 ..
								// Zone에 특수 기술을 사용한다.
								else if (g_pPlayer->TraceSectorToSpecialAction( pItem->GetX(), pItem->GetY()))
								{
									g_pPlayer->SetRepeatAction();
									
									g_bPreviousMove = false;
								}
							}
						}
					}
				}
				break;

			}
		}		
	}

	#ifdef OUTPUT_DEBUG_PROCESS_INPUT
		DEBUG_ADD("RbOK");
	#endif
}

//---------------------------------------------------------------------------
// Process Game Input
//---------------------------------------------------------------------------
void
CGameUpdate::ProcessInput()
{
	if(g_pEventManager->GetEventByFlag(EVENTFLAG_DENY_INPUT))
		return;

	//-----------------------------------------------
	// Quit
	//-----------------------------------------------
	///*
	#ifdef OUTPUT_DEBUG
		if (g_pSDLInput->KeyDown(DIK_LMENU) && g_pSDLInput->KeyDown(DIK_X))
		{	
			//TerminateThread(g_hFileThread, 0);
			SetMode( MODE_QUIT );
			
			return;
		}	
	#endif
	//*/	


	//-----------------------------------------------
	// packet 이동 테스트
	//-----------------------------------------------
	#ifdef OUTPUT_DEBUG
		if (g_pSDLInput->KeyDown(DIK_M) 
			&& g_pSDLInput->KeyDown(DIK_LMENU))
		{			
			PacketTest();
		}
	#endif
	
	/*
	// [ TEST CODE ] DEBUG MESSAGE
	if (g_pSDLInput->m_lb_down || g_pSDLInput->m_lb_up)
			DEBUG_ADD("----------");

	if (g_pSDLInput->m_lb_down && g_pSDLInput->m_lb_up)
	{
			DEBUG_ADD("LButtonDown & Up");
	}
	else if (g_pSDLInput->m_lb_down)
	{
			DEBUG_ADD("LButtonDown");
	}
	else if (g_pSDLInput->m_lb_up)
	{
			DEBUG_ADD("LButtonUp");
	}
	*/

	
	//g_SelectSector = g_pTopView->GetSelectedSector(g_x, g_y);
	

	//---------------------------------------------------	
	// LButton Up
	//---------------------------------------------------	
	if (g_bLButtonDown
		// L이 Up이거나.. R이 Down인 경우
		&& (g_pSDLInput->m_lb_up	|| g_pSDLInput->m_rb_down)
#ifdef __METROTECH_TEST__
		&& !g_bCButtonDown
#endif
		)
	{
		#ifdef OUTPUT_DEBUG_PROCESS_INPUT
			DEBUG_ADD("LBUP");
		#endif

		g_bLButtonDown = FALSE;

		// 주위의 누군가를 계속 공격하는 mode를 해제한다.
		g_pPlayer->UnSetLockMode();
	
		if (g_pPlayer->IsRepeatAction())
		{
			// 버튼을 떼었으므로 행동 반복을 취소한다.
			g_pPlayer->UnSetRepeatAction();
			//g_pPlayer->TraceNextNULL();

			#ifdef OUTPUT_DEBUG_UPDATE_LOOP
					//DEBUG_ADD("plu");//[ProcessInput] Lbutton UP");
			#endif
		}
		else 
		{		
			//---------------------------------------------------	
			// item을 들고 있지 않은 경우
			//---------------------------------------------------	
			// 선택된 sector로 정한다.
			POINT point;
			g_pPlayer->GetNextDestination( point );		
			if (point.x==SECTORPOSITION_NULL || point.y==SECTORPOSITION_NULL)
			{
				// 현재 가고 있는 곳이 없으면
				g_pPlayer->GetDestination( point );
				if (point.x==SECTORPOSITION_NULL || point.y==SECTORPOSITION_NULL)
				{
					//-_-;;
				}
				else
				{
					g_pTopView->SetSelectedSector( point );
				}
			}
			else
			{
				g_pTopView->SetSelectedSector( point );
			}
		}
	}

	//---------------------------------------------------	
	// RButton Up
	//---------------------------------------------------	
	if (g_bRButtonDown
		// R이 Up이거나. L이 Down인 경우
		&& (g_pSDLInput->m_rb_up || g_pSDLInput->m_lb_down)
#ifdef __METROTECH_TEST__
		&& !g_bCButtonDown
#endif
		)
	{
		#ifdef OUTPUT_DEBUG_PROCESS_INPUT
			DEBUG_ADD("RBUP");
		#endif

		g_bRButtonDown = FALSE;

		g_pPlayer->UnSetLockMode();

		// 2004, 8, 30, sobeit add start
		g_pPlayer->CheckRbuttonUpSkill();
		// 2004, 8, 30, sobeit add end
		{
			if (g_pPlayer->IsRepeatAction())
			{
				// 버튼을 떼었으므로 행동 반복을 취소한다.
				g_pPlayer->UnSetRepeatAction();
				//g_pPlayer->TraceNextNULL();

				#ifdef OUTPUT_DEBUG_UPDATE_LOOP
						//DEBUG_ADD("pru");//[ProcessInput] Rbutton UP");
				#endif
			}
		}
	}

	if(g_bCButtonDown && g_pSDLInput->m_cb_up)
	{
		g_bCButtonDown = FALSE;
	}
	
	//---------------------------------------------------	
	// UI Input
	//---------------------------------------------------	
	POINT cursorPoint;
	GetCursorPos(&cursorPoint);

	ScreenToClient(g_hWnd, &cursorPoint);//by viva
	WindowToGamePoint(cursorPoint);

	#ifdef OUTPUT_DEBUG_PROCESS_INPUT
		DEBUG_ADD("UIMM");
	#endif
	gC_vs_ui.MouseControl(M_MOVING, cursorPoint.x, cursorPoint.y);
	//gC_vs_ui.MouseControl(M_MOVING, g_x, g_y);
	//if (gC_vs_ui.MouseControl(M_MOVING, g_x, g_y))
	//	return;

	#ifdef OUTPUT_DEBUG_PROCESS_INPUT
		DEBUG_ADD("IRP");
	#endif

	
	bool bRunningParty = UI_IsRunningParty();

	if (g_pSDLInput->m_lb_down)
	{
		#ifdef OUTPUT_DEBUG
			if (g_pSDLInput->KeyDown(DIK_LSHIFT) && g_bSlideScreenShot)
			{
				g_bSlideRectSelect = true;
			}
		#endif
		
		if (gC_vs_ui.MouseControl(M_LEFTBUTTON_DOWN, g_x, g_y) )
		{
			#ifdef OUTPUT_DEBUG_PROCESS_INPUT
					DEBUG_ADD("[ProcessInput] UI LButton Down return");
			#endif

			return;				
		}
	}

	if (g_pSDLInput->m_lb_up)
	{
		#ifdef OUTPUT_DEBUG
			g_bSlideRectSelect = false;			
		#endif

		if (gC_vs_ui.MouseControl(M_LEFTBUTTON_UP, g_x, g_y))
		{
			#ifdef OUTPUT_DEBUG_PROCESS_INPUT
					DEBUG_ADD("[ProcessInput] UI LButton Up return");
			#endif

			return;
		}
	}
	if (g_pSDLInput->m_rb_down)
	{
		if (gC_vs_ui.MouseControl(M_RIGHTBUTTON_DOWN, g_x, g_y)
			&& !bRunningParty)
		{
			#ifdef OUTPUT_DEBUG_PROCESS_INPUT
					DEBUG_ADD("[ProcessInput] UI RButton Down return");
			#endif

			return;
		}
	}

	if (g_pSDLInput->m_rb_up)
	{
		if (gC_vs_ui.MouseControl(M_RIGHTBUTTON_UP, g_x, g_y)
			&& !bRunningParty)
		{
			#ifdef OUTPUT_DEBUG_PROCESS_INPUT
					DEBUG_ADD("[ProcessInput] UI RButton Up return");
			#endif

			return;
		}
	}	

	if( g_pSDLInput->m_cb_up)
	{
		if( gC_vs_ui.MouseControl(M_CENTERBUTTON_UP, g_x,g_y)
			&& !bRunningParty)
		{
			return;
		}
	}

	if( g_pSDLInput->m_cb_down)
	{
		if( gC_vs_ui.MouseControl( M_CENTERBUTTON_DOWN, g_x,g_y)
			&& !bRunningParty)
		{
			return;
		}
	}
	
	//#ifdef OUTPUT_DEBUG
	// 채팅
//		if (gC_vs_ui.ChatMouseControlExtra( M_MOVING, g_x, g_y ))
//			return;
	//#endif

	// 2000.7.8. KJTINC
	/*
	static bool bl_ctrl;
	if (g_pSDLInput->KeyDown(DIK_LCONTROL) ||
		g_pSDLInput->KeyDown(DIK_RCONTROL))
	{
		bl_ctrl = true;
	}
	//if (g_pSDLInput->KeyUp(DIK_LCONTROL) &&
	//	 g_pSDLInput->KeyUp(DIK_RCONTROL))
	//{
	//	bl_ctrl = false;
	//}
	if (g_pSDLInput->KeyDown(DIK_SPACE) && bl_ctrl == true)
	{
		gC_vs_ui.PressCtrlSpace();
	}*/


	#ifdef OUTPUT_DEBUG_PROCESS_INPUT
		DEBUG_ADD("CINL");
	#endif

	//---------------------------------------------------	
	// ItemName List 출력?
	//---------------------------------------------------	
	if ((g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU) || g_pSDLInput->KeyDown(DIK_RCONTROL)
#ifdef __FRIEND_SYSTEM_VIVA__	// add by viva  g_pSDLInput->KeyDown(DIK_LCONTROL
		|| g_pSDLInput->KeyDown(DIK_LCONTROL)
#endif
		)  
		&& !g_pSDLInput->KeyDown(DIK_LSHIFT)
		&& g_pTradeManager==NULL && g_pPlayer->GetWaitVerify()!=MPlayer::WAIT_VERIFY_TRADE)
	{
		#ifdef OUTPUT_DEBUG_PROCESS_INPUT
			DEBUG_ADD("DINL");
		#endif

		g_pTopView->SetDrawItemNameList();

		if (!g_bWatchMode 
			// 뭔가 반복행동을 하고 있지 않은 경우
			&& !g_pPlayer->IsRepeatAction()
			// 박쥐나 늑대가 아닌 경우
			&& g_pPlayer->GetCreatureType()!=CREATURETYPE_BAT
			&& g_pPlayer->GetCreatureType()!=CREATURETYPE_WOLF
			)
		{
			g_pTopView->UnSetRequestMode();
			g_pTopView->SetSelectedSectorNULL();					

			if (g_pTempInformation->GetMode() == TempInformation::MODE_NULL)
			{					
				if (g_pSDLInput->KeyDown(DIK_LMENU))
				{
					g_pTopView->SetRequestMode( MTopView::REQUEST_TRADE );
				}
				else if (g_pSDLInput->KeyDown(DIK_RMENU))
				{
					if (!gC_vs_ui.IsRunningRequestParty()
						&& g_pSystemAvailableManager->IsAvailablePartySystem() )
					{
						g_pTopView->SetRequestMode( MTopView::REQUEST_PARTY );
					}
				}
				else if (g_pSDLInput->KeyDown(DIK_RCONTROL))
				{
					g_pTopView->SetRequestMode( MTopView::REQUEST_INFO );
				}
#ifdef __FRIEND_SYSTEM_VIVA__
				///add by viva
				else if(g_pSDLInput->KeyDown(DIK_LCONTROL))
				{
					gpC_mouse_pointer->SetCursorAddFriend();
				//	g_pPlayer->SetRepeatAction();
				}
#endif
			}
		}
	}
	else
	{
		#ifdef OUTPUT_DEBUG_PROCESS_INPUT
			DEBUG_ADD("NDINL");
		#endif

		g_pTopView->UnSetDrawItemNameList();
		g_pTopView->UnSetRequestMode();		
	}

	#ifdef OUTPUT_DEBUG_PROCESS_INPUT
		DEBUG_ADD("PMC");
	#endif

		
	//---------------------------------------------------	
	// 파티 없는데 파티창이 떠 있는 경우..
	// 파티 신청 모드.
	//---------------------------------------------------	
//	if (g_pParty!=NULL 
//		&& g_pParty->GetSize()==0
//		&& UI_IsRunningParty()
//		&& !gC_vs_ui.IsRunningRequestParty()
//		&& g_pTempInformation->Mode==TempInformation::MODE_NULL
//		&& !g_bWatchMode 
//		// 뭔가 반복행동을 하고 있지 않은 경우
//		&& !g_pPlayer->IsRepeatAction()
//		// 박쥐나 늑대가 아닌 경우
//		&& g_pPlayer->GetCreatureType()!=CREATURETYPE_BAT
//		&& g_pPlayer->GetCreatureType()!=CREATURETYPE_WOLF)
//	{
//		g_pTopView->SetSelectedSectorNULL();
//		g_pTopView->SetRequestMode( MTopView::REQUEST_PARTY );
//	}

	//---------------------------------------------------	
	//
	//		입력이 제한되는 경우..
	//
	//---------------------------------------------------	
	// - 게임 못하게 하는 dialog가 떠 있는 경우
	//
	// - Player가 server로부터 검증을 기다리는 상태
	//
	// - 반복 action중에는 딴 입력은 필요없다.(무시)
	//
	// - UI에서 입력을 받고 있는 경우
	//
	//---------------------------------------------------	
	if (g_pPlayer->IsWaitVerify() 
		|| gC_vs_ui.IsInstallMineProgress()	// 지뢰 만드는 중이면..
		//|| gC_vs_ui.IsCreateMineProgress()	// 지뢰 만드는 중이면..
		//|| gC_vs_ui.IsCreateBombProgress()	// 지뢰 만드는 중이면..
		|| g_pPlayer->IsFastMove()	// 빨리 움직이는 경우
		// 2004, 12, 3, 석민씨 추가
//		|| g_pPlayer->CurPernalShop() == 1  || gC_vs_ui.inventory_mode == 2
//		|| gC_vs_ui.IsRunningPersnalShop()
		// 2004, 12, 3, 석민씨 추가
		|| UI_IsRunning_WebBrowser()
		)	
	{
		#ifdef OUTPUT_DEBUG_PROCESS_INPUT
				DEBUG_ADD_FORMAT("[ProcessInput] Is WaitVerify Return. %d %d", g_pPlayer->IsWaitVerify(), g_pPlayer->IsFastMove());
		#endif

		return;
	}

	// 현재 셀렉트중인 캐릭터에 대한 동작이 있으면 리턴
	if (g_pPlayer->IsRepeatAction())
	{
		g_pTopView->SetSelectedSectorNULL();

		#ifdef OUTPUT_DEBUG_PROCESS_INPUT
				DEBUG_ADD("[ProcessInput] Is RepeatAction return");
		#endif

		return;
	}

	#ifdef OUTPUT_DEBUG_PROCESS_INPUT
		DEBUG_ADD("CUILock");
	#endif

	//---------------------------------------------------	
	// UI에서 mouse커서 입력을 잡고 있는 경우
	// elevator 작동중..
	//---------------------------------------------------	
	if (((g_pUIDialog->IsLockInput() || g_bUIInput)
		
		// 파티 떠있을때는 파티창의 캐릭을 선택할 수도 있다.
		&& !bRunningParty)

		// 다른 동작 못하도록 ...
		|| gC_vs_ui.IsRunningElevator()
		|| gC_vs_ui.IsRunningXmasCardWindow()
		|| UI_IsRunningSelectWayPoint())
	{
		g_pTopView->SetSelectedNULL();
			
		#ifdef OUTPUT_DEBUG_PROCESS_INPUT
				DEBUG_ADD_FORMAT("[ProcessInput] UI return. %d %d %d", g_pUIDialog->IsLockInput(), g_bUIInput, gC_vs_ui.IsRunningElevator());
		#endif

		return;
	}

	
	
	//---------------------------------------------------	
	//
	// Player가 죽은 경우... 
	//
	//---------------------------------------------------		
	if (g_pPlayer->IsDead())
	{
		g_pTopView->SetSelectedNULL();
		g_pTopView->SetSelectedSectorNULL();

		//---------------------------------------------------	
		// delay시간이 지나고 나서
		// 뭔가를 누르면 다시 살아나야 한다.
		//---------------------------------------------------	
		//if (g_pPlayer->IsNotDeadDelay() && g_pSDLInput->KeyDown( DIK_SPACE ))
		//{
		//	g_pPlayer->SetAlive();
		//	g_pTopView->SetFadeEnd();
		//}
		#ifdef OUTPUT_DEBUG_PROCESS_INPUT
				DEBUG_ADD("[ProcessInput] Player Dead return.");
		#endif

		return;
	}
 


	// Mouse를 향해서 바라보는 player...
	//POINT temp = g_pTopView->ScreenToPixel(g_x, g_y);
	//temp = MTopView::PixelToMap( temp.x, temp.y );
	//g_pPlayer->SetDirectionToPosition(temp.x, temp.y);

	#ifdef OUTPUT_DEBUG_PROCESS_INPUT
		DEBUG_ADD("FA");
	#endif

	//---------------------------------------------------	
	//
	// 현재 mouse의 위치에 있는 object에 대해서 check
	//
	//---------------------------------------------------	
	// 강제공격이거나 SmallZone에 있거나
	// Hallucination에 걸렸으면 다~ 선택
	//---------------------------------------------------	
	

	// 강제 공격이 설정되는 경우
	bool bForceAttack = (g_pSDLInput->KeyDown(DIK_LSHIFT) 
						|| g_pPlayer->HasEffectStatus(EFFECTSTATUS_HALLUCINATION));
						
						

	if (bForceAttack
		//|| !g_bZonePlayerInLarge
		|| g_bZoneSafe
		|| g_pPlayer->IsInSafeSector()
		|| (g_pPlayer->IsSlayer() && g_pPlayer->GetSpecialActionInfo() != ACTIONINFO_NULL && (*g_pActionInfoTable)[g_pPlayer->GetSpecialActionInfo()].GetUser() == FLAG_ACTIONINFO_USER_SLAYER)
		|| (g_pPlayer->IsVampire() && g_pPlayer->GetSpecialActionInfo() != ACTIONINFO_NULL && (*g_pActionInfoTable)[g_pPlayer->GetSpecialActionInfo()].GetUser() == FLAG_ACTIONINFO_USER_VAMPIRE)
		|| (g_pPlayer->IsOusters() && g_pPlayer->GetSpecialActionInfo() != ACTIONINFO_NULL && (*g_pActionInfoTable)[g_pPlayer->GetSpecialActionInfo()].GetUser() == FLAG_ACTIONINFO_USER_OUSTERS)
		// 2004, 11, 26, sobeit add start - 슬레 140 인챈 스킬 - 슬레에겐 축복, 나머진 저주..ㅋㅋ 
		|| g_pPlayer->GetSpecialActionInfo() == SKILL_INTIMATE_GRAIL
		// 2004, 11, 26, sobeit add end
		)
	{
		g_pObjectSelector->SelectAll();				
	}
	//---------------------------------------------------	
	// L-Control누르면 우리편만 선택
	//---------------------------------------------------	
	else if (g_pSDLInput->KeyDown(DIK_LCONTROL)
		|| g_pTopView->IsRequestMode())
	{
		g_pObjectSelector->SelectFriend();		
	}
	//---------------------------------------------------	
	// 아니면.. 적만 선택
	//---------------------------------------------------	
	else
	{
		// Notice Event 체크해서 ...
		// 성을 차지하기 위한 전쟁중이라면!!
		// 1. 기존에 수비하는 길드일 경우 자기 길드 아니면 다 적!
		// 2. 공격 신청을 한 길드일경우 우리 길드 아니면 다 적!
		// 3. 길드간 전쟁 신청 외에 다른사람일경우 그 두 길드원 빼고 다 같은 팀!
		
		g_pObjectSelector->SelectEnemy();		
	}

	//---------------------------------------------------	
	// 이것 외에도..
	//
	// g_pObjectSelector->SelectByRace()인지
	// g_pObjectSelector->SelectByGuild()인지를 선택해야 한다.
	//
	// 화면에 늘 Icon을 띄워놓는게 좋을 듯.. Race/Guild
	//
	// 또, 공격모드에 따라서 Peace/Attack/Normal Icon도
	// 있어야 한다.
	//---------------------------------------------------	
	// Slayer인 경우는 종족에 따라서 선택
	//---------------------------------------------------	
	if (g_pPlayer->IsSlayer() || g_pPlayer->IsOusters() )
	{
		g_pObjectSelector->SelectByRace();
	}
	//---------------------------------------------------	
	// Vampire인 경우는 Guild에 따라서 선택
	//---------------------------------------------------	
	else
	{
		g_pObjectSelector->SelectByGuild();
	}


	//MObject*	pObject = g_pTopView->GetSelectedObjectSprite(g_x, g_y);
	MObject*	pObject = NULL;
	
	
	#ifdef OUTPUT_DEBUG_PROCESS_INPUT
		DEBUG_ADD("PMF");
	#endif

	int partyMember = (bRunningParty? gC_vs_ui.GetPartyManagerFocused() : -1) - 1;

	//---------------------------------------------------	
	// 파티UI를 선택하는 경우..
	//---------------------------------------------------	
	if (partyMember >= 0
		&& partyMember < g_pParty->GetSize())
	{
		#ifdef OUTPUT_DEBUG_PROCESS_INPUT
			DEBUG_ADD("SelP");
		#endif

		PARTY_INFO* pInfo = g_pParty->GetMemberInfo( partyMember );

		if (pInfo!=NULL)
		{
			#ifdef OUTPUT_DEBUG_PROCESS_INPUT
				DEBUG_ADD("SPM");
			#endif

			// 우욱.. terrible.. - -;
			// MZone에 Name으로 id검색할 수 있게 map을 추가해야한다.
			// 그리고.. 바로 Name --> MCreature* 를 알 수 있게 해야한다.. 언제? - -;
			pObject = g_pZone->GetCreature( g_pZone->GetCreatureID( pInfo->Name.GetString(), 1 ) );

			#ifdef OUTPUT_DEBUG_PROCESS_INPUT
				DEBUG_ADD("SPMk");
			#endif
		}
	}
	//---------------------------------------------------	
	// 존에서 선택..
	//---------------------------------------------------	
	else
	{
		#ifdef OUTPUT_DEBUG_PROCESS_INPUT
			DEBUG_ADD("selZ");
		#endif

		//---------------------------------------------------
		// 아예 딴 UI에 focus 가 있으면 걍 return해야 한다.
		// inventory에서 아이템 사용했는데 기술 나갈 수가 있어서.
		//---------------------------------------------------
		if (partyMember==-2		// party창에 focus 된게 아니고
			&& g_bUIInput)		// UI에 입력이 있다면..
		{
			#ifdef OUTPUT_DEBUG_PROCESS_INPUT
				DEBUG_ADD("selZk1");
			#endif

			return;
		}

		//---------------------------------------------------	
		// ItemName을 선택하는 경우면..
		//---------------------------------------------------	
		if (g_pTopView->IsDrawItemNameList())
		{
			#ifdef OUTPUT_DEBUG_PROCESS_INPUT
				DEBUG_ADD("drawItemNL");
			#endif

			MItem* pItem = g_pTopView->SelectItemName(g_x, g_y);

			#ifdef OUTPUT_DEBUG_PROCESS_INPUT
				DEBUG_ADD("selIN");
			#endif

			if (pItem==NULL)
			{
				pObject = NULL;
			}
			else
			{
				pObject = (MObject*)pItem;
			}

			//---------------------------------------------------	
			// ItemName이 선택 안됐으면 Object선택
			//---------------------------------------------------	
			if (pObject==NULL)
			{
				#ifdef OUTPUT_DEBUG_PROCESS_INPUT
					DEBUG_ADD("selObj");
				#endif

				pObject = g_pTopView->GetSelectedObject(g_x, g_y);

				#ifdef OUTPUT_DEBUG_PROCESS_INPUT
					DEBUG_ADD("selObjK");
				#endif
			}
		}
		//---------------------------------------------------	
		// 그냥 Object 선택
		//---------------------------------------------------	
		else
		{
			#ifdef OUTPUT_DEBUG_PROCESS_INPUT
				DEBUG_ADD("selObj2");
			#endif

			pObject = g_pTopView->GetSelectedObject(g_x, g_y);

			#ifdef OUTPUT_DEBUG_PROCESS_INPUT
				DEBUG_ADD("selObj2K");
			#endif
		}

		#ifdef OUTPUT_DEBUG_PROCESS_INPUT
			DEBUG_ADD("selZk");
		#endif
	}

//	if (g_pPlayer->IsRepeatAction())
//	{
//		g_pTopView->SetSelectedSectorNULL();
//		
//#ifdef OUTPUT_DEBUG_PROCESS_INPUT
//		DEBUG_ADD("[ProcessInput] Is RepeatAction return");
//#endif
//		
//		return;
//	}
	
	//---------------------------------------------------		
	// Watch mode
	//---------------------------------------------------		
	if (g_bWatchMode)
	{
		int cx = 0, cy = 0;

		if (g_x < 10)		cx = -1;
		else if (g_x > 790)	cx = 1;
		if (g_y < 10)		cy = -1;
		else if (g_y > 590)	cy = 1;

		int x = g_pPlayer->GetX();
		int y = g_pPlayer->GetY();

		x += cx;
		y += cy;

		if (x < 0) x = 0;
		else if (x >= g_pZone->GetWidth()) x = g_pZone->GetWidth()-1;
		if (y < 0) y = 0;
		else if (y >= g_pZone->GetHeight()) y = g_pZone->GetHeight()-1;

		g_pPlayer->SetPosition( x, y );

		#ifdef OUTPUT_DEBUG_PROCESS_INPUT
				DEBUG_ADD("[ProcessInput] Watch Mode");
		#endif

		return;
	}

	#ifdef OUTPUT_DEBUG_PROCESS_INPUT
		DEBUG_ADD("ckL");
	#endif

	//---------------------------------------------------	
	// Lock Mode 체크
	//---------------------------------------------------	
	if (g_pSDLInput->KeyDown(DIK_CAPITAL) 
		&& !g_pPlayer->IsInCasket()
		&& !g_pPlayer->IsUndergroundCreature())
	{
		g_pPlayer->SetLockMode();

		// 무조건 공격 커서로 바뀐다.
		gpC_mouse_pointer->SetCursorAttack();

		if (g_pSDLInput->m_lb_down)
		{		
			g_bLButtonDown = TRUE;			
		}
		else if (g_pSDLInput->m_rb_down)
		{
			g_bRButtonDown = TRUE;
		}
		else if (g_pSDLInput->m_cb_down)
		{
			g_bCButtonDown = TRUE;
		}

		// Lock Mode에서는 선택된 sector를 없앤다.
		g_pTopView->SetSelectedSectorNULL();
	}
	else
	{
		// 아무것도 누르지 않은 상태에서 
		// LockMode이면 LockMode 해제해야 한다.
		if (!g_bLButtonDown && !g_bRButtonDown
#ifdef __METROTECH_TEST__
			&& !g_bCButtonDown
#endif
			)
		{
			g_pPlayer->UnSetLockMode();
		}
	}

	//---------------------------------------------------	
	// Lock Mode가 아닌 경우만 
	// 일반적인 입력을 받아들여 행동을 취한다.
	//---------------------------------------------------	
	if (!g_pPlayer->IsLockMode())
	{	
#ifdef OUTPUT_DEBUG_PROCESS_INPUT
		DEBUG_ADD("notLock");
#endif
		
		//---------------------------------------------------	
		// Mouse 위치에 Object가 없으면..
		//---------------------------------------------------	
		if (pObject==NULL)
		{
#ifdef OUTPUT_DEBUG_PROCESS_INPUT
			DEBUG_ADD("noObj");
#endif
			
			// 선택된 것 없게 한다.
			g_pTopView->SetSelectedNULL();
			
			//---------------------------------------------------	
			// mouse pointer 설정
			//---------------------------------------------------	
			//if (!g_bMouseInPortal)
			{
				if (g_pPlayer->OnAttacking() || g_pSDLInput->KeyDown(DIK_LSHIFT))
				{
					gpC_mouse_pointer->SetCursorAttack();
				}
				else
				{
					if (g_MouseSector.x >= 0 
						&& g_MouseSector.y >= 0
						&& g_MouseSector.x < g_pZone->GetWidth() 
						&& g_MouseSector.y < g_pZone->GetHeight())
					{
#ifdef OUTPUT_DEBUG_PROCESS_INPUT
						DEBUG_ADD_FORMAT("getSec(%d, %d)", g_MouseSector.x, g_MouseSector.y);
#endif
						
						const MSector& sector = g_pZone->GetSector(static_cast<TYPE_SECTORPOSITION>(g_MouseSector.x), static_cast<TYPE_SECTORPOSITION>(g_MouseSector.y));
						
						//---------------------------------------------------
						// 갈 수 없는 곳이면
						//---------------------------------------------------
						if ((g_pPlayer->IsGroundCreature() && sector.IsBlockGround())
							|| (g_pPlayer->IsUndergroundCreature() && sector.IsBlockUnderground())
							|| (g_pPlayer->IsFlyingCreature() && sector.IsBlockFlying()))
							//g_pZone->CanMove(g_pPlayer->GetMoveType(), g_MouseSector.x, g_MouseSector.y))
						{	
							// 포탈이 아니고
							if (!g_bMouseInPortal
								// UI에 입력이 없는 경우 --> 게임에 커서가 있는 경우
								&& !g_bUIInput
								// 선택된 캐릭터가 없는 경우
								&& g_pTopView->GetSelectedCreature()==OBJECTID_NULL)
							{
								// 갈 수 없는 커서로 표시
								gpC_mouse_pointer->SetCursorNotMove();					
							}
						}
						//---------------------------------------------------
						// 갈 수 있는 곳이면..
						//---------------------------------------------------
						else
						{
							if (!g_bMouseInPortal)
							{
								gpC_mouse_pointer->SetCursorDefault();
							}
						}
					}
					else
					{
						if (!g_bMouseInPortal)
						{
							gpC_mouse_pointer->SetCursorDefault();
						}
					}
				}
			}
			
#ifdef OUTPUT_DEBUG_PROCESS_INPUT
			DEBUG_ADD("noObjk");
#endif
		}
		//---------------------------------------------------	
		//
		// Mouse 위치에 Object가 있는 경우
		//
		//---------------------------------------------------	
		else
		{
			BYTE objectType = pObject->GetObjectType();
			
#ifdef OUTPUT_DEBUG_PROCESS_INPUT
			DEBUG_ADD_FORMAT("obj(%d)", objectType);
#endif
			
			//------------------------------------------------
			// Creature 위에 mouse가 있는 경우
			//------------------------------------------------
			if (objectType==MObject::TYPE_CREATURE)
			{
				MCreature* pCreature = (MCreature*)pObject;

				g_pTopView->SetSelectedCreature( pCreature->GetID() );
					
				if (!g_bMouseInPortal 
						
					// 종족에 따라서 공격할 수 있거나
					// 강제 공격이거나..
					//&& (g_pPlayer->CanAttackTribe( pCreature ) || bForceAttack)
					&& g_pObjectSelector->CanAttack( pCreature ) )
					// 뱀파이어인 경우에는 Guild에 따라서 공격할 수 있거나..	
						
				{
					if (pCreature->IsNPC())
					{
						gpC_mouse_pointer->SetCursorDefault();
					}
					else
					{
						gpC_mouse_pointer->SetCursorAttack();
					}
				}
#ifdef __FRIEND_SYSTEM_VIVA__//add by viva
				if(g_pSDLInput->KeyDown(DIK_LCONTROL) && g_pSDLInput->m_lb_down)	
				{
					gpC_base->SendMessage(UI_FRIEND_CHATTING_ADD_FRIEND, 0, 0, (void*)pCreature->GetName());
				}
#endif
					
			}
			
			//------------------------------------------------
			// Item 위에 mouse가 있는 경우
			//------------------------------------------------
			else if (pObject->GetObjectType()==MObject::TYPE_ITEM)
			{
				MItem* pItem = (MItem*)pObject;

				if(g_pZone != NULL && pItem != NULL )
				{
					// 슬레이어면 안되게
					// 뱀파이어면 creature 찾아서 성물인경우만

					if(!g_pZone->GetSector(pItem->GetX(), pItem->GetY()).HasDarkness() ||
						(g_pZone->GetSector(pItem->GetX(), pItem->GetY()).HasDarkness() && g_pPlayer->IsVampire() && g_pZone->GetID() != 3001)||
						(g_pZone->GetSector(pItem->GetX(), pItem->GetY()).HasDarkness() && !g_pPlayer->IsVampire() &&
						g_pPlayer->HasEffectStatus( EFFECTSTATUS_LIGHTNESS ) && g_pZone->GetID() != 3001)
						|| g_pPlayer->HasEffectStatus( EFFECTSTATUS_GHOST )
#ifdef __METROTECH_TEST__
						|| g_bLight
#endif
						)
					{
						g_pTopView->SetSelectedItem( pItem->GetID() );			
						
						COLORREF color;
						//------------------------------------------------
						// option에 따른 색깔
						//------------------------------------------------
						if (pItem->IsSpecialColorItem() )
						{
							WORD temp_color = CIndexSprite::ColorSet[pItem->GetSpecialColorItemColorset()][16];
							color = RGB(CSDLGraphics::Red(temp_color)<<3, CSDLGraphics::Green(temp_color)<<3, CSDLGraphics::Blue(temp_color)<<3);
						}
						else
						if (pItem->IsEmptyItemOptionList())
						{
							color = g_pClientConfig->COLOR_NAME_ITEM;	
						}
						else if(pItem->GetItemOptionListCount() == 2)
						{
							color = g_pClientConfig->COLOR_NAME_ITEM_RARE_OPTION;
						}
						// add by Sonic 2006.10.28 增加显示三属性装备为红色
						else if(pItem->GetItemOptionListCount() > 2)
						{
							color = g_pClientConfig->COLOR_NAME_VAMPIRE; //Red
						}
						// end by Sonic 2006.10.28 增加显示三属性装备为红色
						else
						{
							color = g_pClientConfig->COLOR_NAME_ITEM_OPTION;
						}
						
						//------------------------------------------------
						// item집는 커서
						//------------------------------------------------
						if (!g_bMouseInPortal&&!g_pPlayer->IsInDarkness())
						{						
							// 시체인 경우는 '시체'라고 표시 안한다.
							if (pItem->GetItemClass()==ITEM_CLASS_CORPSE)
							{
								gpC_mouse_pointer->SetCursorPickUp( "", color );
							}
							// 보통 아이템은 이름 표시한다.
							else
							{
								char str[80];
								SafeFormat::Copy(str, pItem->GetName());
								
								gpC_mouse_pointer->SetCursorPickUp( str, color );
							}
						}
					} 
				}
			}

			//------------------------------------------------
			// Effect 위에 mouse가 있는 경우
			//------------------------------------------------
			else if (pObject->GetObjectType()==MObject::TYPE_EFFECT)
			{
				g_pTopView->SetSelectedEffect( pObject->GetID() );
			}

			//------------------------------------------------
			// InteractionObject 위에 mouse가 있는 경우
			//------------------------------------------------
//			else if (pObject->GetObjectType()==MObject::TYPE_INTERACTIONOBJECT)
//			{
//				g_pTopView->SetSelectedInteractionObject( pObject->GetID() );
//			}
		}


		//---------------------------------------------------	
		//
		// player의 기술 사용 delay 시간이 지나야 공격 가능하다.
		//
		//---------------------------------------------------	
		if (g_pPlayer->IsNotDelay() && !g_pPlayer->HasEffectStatus( EFFECTSTATUS_ETERNITY_PAUSE ) )
		{
			#ifdef OUTPUT_DEBUG_PROCESS_INPUT
				DEBUG_ADD("noDel");
			#endif

			//-----------------------------------------------
			// Shift + L/R ButtonDown : 강제 Tile 공격
			//-----------------------------------------------
			if (g_pSDLInput->KeyDown(DIK_LSHIFT))
			{
				#ifdef OUTPUT_DEBUG_PROCESS_INPUT
					DEBUG_ADD("shiAt");
				#endif
				//-----------------------------------------------
				// Shift + LButtonDown : 강제 공격
				//-----------------------------------------------
				if (g_pSDLInput->m_lb_down
					// 숨어 있는 경우가 아닐때
					&& !g_pPlayer->IsInCasket()
					&& !g_pPlayer->IsUndergroundCreature()
					&& !g_pPlayer->HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
				{
					g_pPlayer->UnSetRequestMode();

					//-----------------------------------------------
					// 캐릭터를 강제 공격..
					//-----------------------------------------------
					if (pObject!=NULL)
					{
						if (pObject->GetObjectType()==MObject::TYPE_CREATURE)
						{
							// Object가 뭔가 선택됐을 경우에는 
							// LButton을 누르고 있는 것만으로 이동이 되지않게 한다.
							MCreature *pCreature = dynamic_cast<MCreature*>(pObject);
							g_bLButtonDown = TRUE;
							
							if (g_pPlayer->TraceCreatureToBasicAction( 
										pCreature->GetID(), 
										true))		// 강제 공격
							{
								//----------------------------------
								// 남에게 하는 기본 행동 반복 설정
								//----------------------------------
								g_pPlayer->SetRepeatAction();
								g_bPreviousMove = false;
							}
							// 목적지 표시를 없앤다.
							g_pTopView->SetSelectedSectorNULL();
						}
					}
					//-----------------------------------------------
					// 강제 Tile 공격
					//-----------------------------------------------
					//g_SelectSector = g_pTopView->GetSelectedSector(g_x, g_y);
					
					// 선택된 Sector로 표시한다.
					//g_pTopView->SetSelectedSector( g_SelectSector );

					//g_pPlayer->TraceSectorToBasicAction( g_SelectSector.x, g_SelectSector.y );			

					#ifdef OUTPUT_DEBUG_PROCESS_INPUT
						DEBUG_ADD("shiKK");
					#endif
				}		
				//-----------------------------------------------
				// Shift + RButtonDown : 강제 Tile 기술 공격
				//-----------------------------------------------
				else if (g_pSDLInput->m_rb_down)
				{
					g_pPlayer->UnSetRequestMode();

					ProcessInputRButtonDown(pObject, true);

					g_bPreviousMove = false;
					/*
					g_SelectSector = g_pTopView->GetSelectedSector(g_x, g_y);

					// 선택된 Sector로 표시한다.
					g_pTopView->SetSelectedSector( g_SelectSector );

					if (g_pPlayer->TraceSectorToSpecialAction( g_SelectSector.x, g_SelectSector.y ))
					{
						//g_pPlayer->SetRepeatAction();
					}


					// 목적지 표시를 없앤다.
					g_pTopView->SetSelectedSectorNULL();
					*/

					#ifdef OUTPUT_DEBUG_PROCESS_INPUT
						DEBUG_ADD("shiRK");
					#endif
				}
				
			}

			//---------------------------------------------------------------
			// L/R ButtonDown only
			//---------------------------------------------------------------
			else
			{
				
				#ifdef OUTPUT_DEBUG_PROCESS_INPUT
					DEBUG_ADD("LRonly");
				#endif
				//---------------------------------------------------------------
				//
				//				LButtonDown
				//
				//---------------------------------------------------------------
				if (g_pSDLInput->m_lb_down 
					// burrow 상태가 아니어야 한다.
					&& !g_pPlayer->IsInCasket()
					&& !g_pPlayer->IsUndergroundCreature()
					&& g_pPlayer->CurPernalShop() != 2
					&& g_pPlayer->CurPernalShop() != 1
					&& gC_vs_ui.IsRunningPersnalShop() == false
					)
				{		
					#ifdef OUTPUT_DEBUG_PROCESS_INPUT
						DEBUG_ADD("Lon");
					#endif

					g_pPlayer->UnSetRequestMode();

					g_bLButtonDown = TRUE;
					
					// 선택된 sector가 없게 한다.
					g_pTopView->SetSelectedSectorNULL();

					// sector 선택하기
					g_SelectSector = g_pTopView->GetSelectedSector(g_x, g_y);
					//g_pTopView->SetSelectedSector(g_SelectSector.x, g_SelectSector.y);

					//
					//MObject*	pObject = g_pTopView->GetSelectedObject(g_x, g_y);

					// 인스톨 터렛일때.. 잡다한 lbuttton 처리 않는다.. lbutton = 무조건 터렛 공격
					if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
					{		
						int TempDir = MTopView::GetDirectionToPosition(g_pPlayer->GetX(), g_pPlayer->GetY(),g_SelectSector.x, g_SelectSector.y);
						
//							if(g_pPlayer->GetDirection() != TempDir)
//							{
//								CGMove _CGMove;
//								_CGMove.setX( g_pPlayer->GetX() );
//								_CGMove.setY( g_pPlayer->GetY() );
//								_CGMove.setDir( TempDir );
//								g_pSocket->sendPacket( &_CGMove );
//							}
						if(g_pPlayer->GetDirection() == TempDir)
						{
//							g_pPlayer->UnSetRequestMode();
//							g_SelectSector = g_pTopView->GetSelectedSector(g_x, g_y);
//							g_pPlayer->SetSpecialActionInfo(SKILL_TURRET_FIRE);
//							if (g_pPlayer->TraceSectorToSpecialAction( g_SelectSector.x, g_SelectSector.y ))
//							{
//								// 선택된 Sector로 표시한다.
//								g_pTopView->SetSelectedSector( g_SelectSector );
//							
//								g_pPlayer->SetRepeatAction();
//							}
//
//							g_bPreviousMove = false;
//								char szTemp[128];
//								sprintf(szTemp, "방향이 같다: %d,방향 ", TempDir);
//								g_pSystemMessage->Add(szTemp);
						}
						else
						{
//							CGMove _CGMove;
//							_CGMove.setX( g_pPlayer->GetX() );
//							_CGMove.setY( g_pPlayer->GetY() );
//							_CGMove.setDir( TempDir );
//							g_pSocket->sendPacket( &_CGMove );
							if(g_pPlayer->IsStop())
								g_pPlayer->SetCurrentDirection(TempDir);
						}
					}
					//--------------------------------------------------
					// 선택한 곳에 Object가 없으면 MOVE
					//--------------------------------------------------
					else if (pObject == NULL)
					{		
						// l-shift나 l-control이 눌려있지 않은 상태에서만 이동.
						if (//!g_pSDLInput->KeyDown(DIK_LSHIFT) &&
							!g_pSDLInput->KeyDown(DIK_LCONTROL))
						{		
							if (g_pPlayer->IsNotDelay() && !g_pPlayer->HasEffectStatus(EFFECTSTATUS_ETERNITY_PAUSE ) 
								// 2004, 9, 14, sobeit add start - 총슬 130 skill 관련
								//&&  !g_pPlayer->HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET ) 
								// 2004, 9, 14, sobeit add end - 총슬 130 skill 관련
								)
							{
								if (g_pPlayer->SetMovePosition(static_cast<TYPE_SECTORPOSITION>(g_SelectSector.x), static_cast<TYPE_SECTORPOSITION>(g_SelectSector.y)))
								{
									//if (g_pPlayer->IsStop())
									{
										// 다음 목표위치로 설정한다
										g_pPlayer->TraceNULL();							
										
										g_pPlayer->SetNextActionToMove();
										
										g_bPreviousMove = true;
									}
								}
							}
						}

						/*
						if (g_pPlayer->IsStop())
						{
							// 다음 목표위치로 설정한다
							g_pPlayer->TraceNULL();
							
							if (g_pPlayer->SetMovePosition(g_SelectSector.x, g_SelectSector.y))
							{
								g_pPlayer->SetNextAction( ACTION_MOVE );
							}
						}
						*/
					}
					//--------------------------------------------------
					// 선택된 Object에 대해서 
					//--------------------------------------------------
					else 
					{	
						// Object가 뭔가 선택됐을 경우에는 
						// LButton을 누르고 있는 것만으로 이동이 되지않게 한다.
						//g_bLButtonDown = FALSE;

						switch (pObject->GetObjectType())
						{			
							case MObject::TYPE_CREATURE :	
								{
									// 2004, 12, 3, 석민씨 추가
									MCreature* TempCreature = (MCreature*)pObject;
									if(TempCreature->CurPernalShop() == 1 )
//										|| g_pPlayer->IsFlyingCreature() || g_pPlayer->IsUndergroundCreature() 
//										|| g_pPlayer->IsGroundCreature() || g_pPlayer->IsFakeCreature() 
//										|| g_pPlayer->GetMoveDevice()== 1|| g_pPlayer->GetMoveDevice()== 2)
									{
										gpC_base->SendMessage(UI_REQUEST_STORE_INFO, pObject->GetID(),0);
										gC_vs_ui.SetOtherObjectID(pObject->GetID());
									}
									// 2004, 12, 3, 석민씨 추가

									if (g_pPlayer->TraceCreatureToBasicAction( pObject->GetID(), 
																				bForceAttack, true ))
									{
										//----------------------------------
										// 남에게 하는 기본 행동 반복 설정
										//----------------------------------
										g_pPlayer->SetRepeatAction();
										g_bPreviousMove = false;
									}
									
									g_pTopView->SetSelectedSectorNULL();
								}
							break;

							case MObject::TYPE_ITEM :
								g_pPlayer->TraceItem( pObject->GetID() );

								g_bLButtonDown = FALSE;

								g_bPreviousMove = false;
							break;

							case MObject::TYPE_EFFECT :
								g_pPlayer->TraceEffect( pObject->GetID() );

								g_bLButtonDown = FALSE;

								g_bPreviousMove = false;								
							break;
							
							case MObject::TYPE_INTERACTIONOBJECT :
								g_pPlayer->TraceInteractionObject( pObject->GetID(), ((MInteractionObject*)pObject)->GetAction() );

								g_bLButtonDown = FALSE;

								g_bPreviousMove = false;
								//-------------------------
								// TEST CODE
								//-------------------------
								//((MInteractionObject*)pObject)->SetAction( !((MInteractionObject*)pObject)->GetAction() );
							break;
						}
					}
					#ifdef OUTPUT_DEBUG_PROCESS_INPUT
						DEBUG_ADD("LK");
					#endif
				}

				//---------------------------------------------------------------
				//
				//		 RButtonDown
				//
				//---------------------------------------------------------------
				else if (g_pSDLInput->m_rb_down)
				{
					#ifdef OUTPUT_DEBUG_PROCESS_INPUT
						DEBUG_ADD("Ro");
					#endif

					g_pPlayer->UnSetRequestMode();

					ProcessInputRButtonDown(pObject);

					g_bPreviousMove = false;

					#ifdef OUTPUT_DEBUG_PROCESS_INPUT
						DEBUG_ADD("RK");
					#endif
				}

				//---------------------------------------------------------------
				// L/R Button을 누르지는 않았지만,
				// 이전에 누른 LButton이 계속 눌려진 상태라면..
				// But(!), L-Shift가 눌러지지 않아야 한다.
				//---------------------------------------------------------------
				else if (g_bLButtonDown 				
						&& !g_pSDLInput->KeyDown(DIK_LSHIFT)
#ifdef __METROTECH_TEST__
						&& !g_bCButtonDown
#endif
						)
				{
					#ifdef OUTPUT_DEBUG_PROCESS_INPUT
						DEBUG_ADD("ConM");
					#endif

					//--------------------------------------------------
					// 선택한 곳에 Object가 없고
					// 방금전에 이동하고 있던 중이면 MOVE
					//--------------------------------------------------
					if (pObject == NULL && g_bPreviousMove)
					{	
						if (g_pPlayer->IsNotDelay() && !g_pPlayer->IsRepeatAction())
						{
							g_SelectSector = g_pTopView->GetSelectedSector(g_x, g_y);

							// 다음 목표위치로 설정한다
							g_pPlayer->TraceNULL();
							
							if (g_pPlayer->SetMovePosition(static_cast<TYPE_SECTORPOSITION>(g_SelectSector.x), static_cast<TYPE_SECTORPOSITION>(g_SelectSector.y)))
							{
								g_pPlayer->SetNextActionToMove();
							}
						}
					}

					#ifdef OUTPUT_DEBUG_PROCESS_INPUT
						DEBUG_ADD("ConMK");
					#endif
				}
			}
		}
	}	// lock mode

	//---------------------------------------------------	
	// LButton Up
	//---------------------------------------------------	
	if (g_pSDLInput->m_lb_up && g_bLButtonDown
#ifdef __METROTECH_TEST__
		&& !g_bCButtonDown
#endif
		)
	{
		#ifdef OUTPUT_DEBUG_PROCESS_INPUT
			DEBUG_ADD("LUP2");
		#endif

		g_bLButtonDown = FALSE;
	
		if (g_pPlayer->IsRepeatAction())
		{
			// 버튼을 떼었으므로 행동 반복을 취소한다.
			g_pPlayer->UnSetRepeatAction();
			//g_pPlayer->TraceNextNULL();

			g_bPreviousMove = false;
		}
		else
		{	
			// 선택된 sector로 정한다.
			POINT point;
			
			// 다음 갈 곳이 없으면
			g_pPlayer->GetNextDestination( point );		
			if (point.x==SECTORPOSITION_NULL || point.y==SECTORPOSITION_NULL)
			{
				// 현재 가고 있는 곳이 없으면
				g_pPlayer->GetDestination( point );
				if (point.x==SECTORPOSITION_NULL || point.y==SECTORPOSITION_NULL)
				{
					//-_-;;
				}
				else
				{
					g_pTopView->SetSelectedSector( point );
				}
			}
			else
			{
				g_pTopView->SetSelectedSector( point );
			}		
		}
	}

	//---------------------------------------------------	
	// RButton Up
	//---------------------------------------------------	
	if (g_pSDLInput->m_rb_up && g_bRButtonDown
#ifdef __METROTECH_TEST__
		&& !g_bCButtonDown
#endif
		)
	{
		#ifdef OUTPUT_DEBUG_PROCESS_INPUT
			DEBUG_ADD("RUP2");
		#endif

		g_bRButtonDown = FALSE;
	
		if (g_pPlayer->IsRepeatAction())
		{
			// 버튼을 떼었으므로 행동 반복을 취소한다.
			g_pPlayer->UnSetRepeatAction();
			//g_pPlayer->TraceNextNULL();
		}
	}

	if(g_pSDLInput->m_cb_up && g_bCButtonDown)
	{
		g_bCButtonDown = FALSE;
	}

	// 가만히 서 있는 상태이면 목표 위치를 없앤다.
	if (g_pPlayer->GetAction()==ACTION_STAND)
	{
		g_pTopView->SetSelectedSectorNULL();
	}

	#ifdef OUTPUT_DEBUG_PROCESS_INPUT
		DEBUG_ADD("IAEnd");
	#endif


	//---------------------------------------------------
	//
	// keyboard
	//
	//---------------------------------------------------
	/*
	// Missile 종류 바꾸기
	if (g_pSDLInput->KeyDown(DIK_1))
		g_pPlayer->SetActionInfo( ACTIONINFO_BOMB_TO_CREATURE );

	else if (g_pSDLInput->KeyDown(DIK_2))
			g_pPlayer->SetActionInfo( ACTIONINFO_FIRE_TO_CREATURE );

	else if (g_pSDLInput->KeyDown(DIK_3))
			g_pPlayer->SetActionInfo( ACTIONINFO_LIGHTNING_TO_CREATURE );

	else if (g_pSDLInput->KeyDown(DIK_4))
			g_pPlayer->SetActionInfo( ACTIONINFO_ATTACH_FIRE_TO_CREATURE );

	else if (g_pSDLInput->KeyDown(DIK_5))
			g_pPlayer->SetActionInfo( ACTIONINFO_ATTACH_AURA_TO_SELF );
	*/

	#if defined(OUTPUT_DEBUG) && defined(_DEBUG)
		//---------------------------------------------------
		// 비~~
		//---------------------------------------------------
		if (g_pSDLInput->KeyDown(DIK_8))
		{
			SetWeather( WEATHER_RAINY, rand()%10+10);
		}

		//---------------------------------------------------
		// 눈~~
		//---------------------------------------------------
		if (g_pSDLInput->KeyDown(DIK_9))
		{
			SetWeather( WEATHER_SNOWY, rand()%10+10);
		}

		//---------------------------------------------------
		// 날씨 멈춤
		//---------------------------------------------------
		if (g_pSDLInput->KeyDown(DIK_0))
		{
			SetWeather( WEATHER_CLEAR, 0 );
		}



		//---------------------------------------------------
		// 시야  + / -
		//---------------------------------------------------

		if (g_pSDLInput->KeyDown(DIK_SUBTRACT) 
			)//&& g_pPlayer->GetLightSight()!=0)
		{
//			g_pPlayer->SetLightSight( g_pPlayer->GetLightSight() - 1 );		
						g_pClientConfig->QuestItemColorSet -= 15;

					if( g_pClientConfig->QuestItemColorSet <= 0 )
						g_pClientConfig->QuestItemColorSet = 495-15;
		}
		
		if (g_pSDLInput->KeyDown(DIK_ADD) )// && g_pPlayer->GetLightSight()!=13)
		{
//			g_pPlayer->SetLightSight( g_pPlayer->GetLightSight() + 1 );		
			
			
		}

		//-----------------------------------------------
		// play Sound
		//-----------------------------------------------
		if (g_pSDLInput->KeyDown(DIK_F4))
		{
			//g_sb2 = g_Sound.LoadWav((*g_pSoundTable)[rand()%g_pSoundTable->GetSize()].Filename);
			//g_sb2 = g_Sound.LoadWav("Data\\Sound\\Sound1.wav");
			// [ TEST CODE ]
			/*
			static int pan = -10000;
			static int p = 200;
			pan += p;

			if (pan > 10000) 
			{
				pan = 10000;
				p = -p;
			}

			if (pan < -10000) 
			{
				pan = -10000;
				p = -p;
			}
			*/

			//g_Sound.ChangePan((*g_pSoundTable)[rand()%g_pSoundTable->GetSize()].pDSBuffer, pan);
			//g_Sound.Play((*g_pSoundTable)[rand()%g_pSoundTable->GetSize()].pDSBuffer);

			//PlaySound( rand()%g_pSoundTable->GetSize(), 
			//			false,
			//			g_pPlayer->GetX(), g_pPlayer->GetY() );
			PlaySound( SOUND_S_DIE, 
						false,
						g_pPlayer->GetX() + rand()%20-10, g_pPlayer->GetY() + rand()%20-10 );			

		}

		//---------------------------------------------------
		// 밝기 감소
		//---------------------------------------------------
		if (g_pSDLInput->KeyDown(DIK_F9))
		{
			BYTE DarkBits = g_pTopView->GetDarkBits()-1;
			g_pTopView->SetDarkBits(DarkBits);
		}
		
		//---------------------------------------------------
		// 밝기 증가
		//---------------------------------------------------
		if (g_pSDLInput->KeyDown(DIK_F10))
		{
			BYTE DarkBits = g_pTopView->GetDarkBits()+1;
			g_pTopView->SetDarkBits(DarkBits);
		}	
		
		// Gamma Test
		/*
		static BYTE gammaStep = 255;
		if (CSDLGraphics::IsSupportGammaControl())
		{
			if (g_pSDLInput->KeyDown(DIK_LBRACKET))
			{
				gammaStep -= 10;
				CSDLGraphics::SetGammaRamp( gammaStep );
			}
			if (g_pSDLInput->KeyDown(DIK_RBRACKET))
			{		
				gammaStep += 10;		
				CSDLGraphics::SetGammaRamp( gammaStep );
			}
		}
		*/
	#endif

	//-----------------------------------------------
	// File Loading Test with Thread
	//-----------------------------------------------		
	/*
	if (g_pSDLInput->KeyDown(DIK_Z))
	{	
		// test for state of event nonsignaled?
		if (WaitForSingleObject(g_hFileEvent, 0) != WAIT_OBJECT_0)
		{
			g_ThreadJob = THREADJOB_LOAD_CREATURE;
			SetEvent(g_hFileEvent);			
		}		
	} 

	if (g_pSDLInput->KeyDown(DIK_X) && WaitForSingleObject(g_hFileEvent, 0) != WAIT_OBJECT_0)
	{	
		g_pTopView->ReleaseCreatureSPK( 0 );
		g_pTopView->ReleaseCreatureSPK( 1 );
	}
	*/

	//-----------------------------------------------
	// Thread Priority Test
	//-----------------------------------------------	
	/*
	if (g_pSDLInput->KeyDown(DIK_9))
	{
		SetThreadPriority(g_hFileThread, 
							THREAD_PRIORITY_BELOW_NORMAL);
	}

	if (g_pSDLInput->KeyDown(DIK_0))
	{
		SetThreadPriority(g_hFileThread, 
							THREAD_PRIORITY_HIGHEST);
	}
	*/
		
	#ifdef OUTPUT_DEBUG
		// Zone이동 test
		/*
		if (g_pSDLInput->KeyDown(DIK_1))
		{	
			MoveZone( 1 );				
		}
		else if (g_pSDLInput->KeyDown(DIK_2))
		{				
			MoveZone( 2 );				
		}	
		else if (g_pSDLInput->KeyDown(DIK_3))
		{							
			MoveZone( 3 );			
		}
		else if (g_pSDLInput->KeyDown(DIK_4))
		{			
			MoveZone( 4 );				
		}
		*/
		

		//-----------------------------------------------
		// Test for Effect
		//-----------------------------------------------
		#ifdef _DEBUG
			static DWORD start=0, current = 0;

			

			current++;

			if (current-start > 10)
			{
				if (g_pSDLInput->KeyDown(DIK_LCONTROL) && g_pSDLInput->KeyDown(DIK_RCONTROL))
				{					
					
					POINT playerPoint = MTopView::MapToPixel(g_pPlayer->GetX(), g_pPlayer->GetY());

					for (int i=-1; i<=1; i++)
					{
						for (int j=-1; j<=1; j++)
						{
							if (i==0 && j==0)
								continue;
							/*
							if (i==0 && j==0)
							{
								MEffect*	pEffect;
								pEffect = new MEffect;

								pEffect->SetFrameID(0, 8);		// 0번 Effect, Max 8 Frame
								pEffect->SetPosition(g_pPlayer->GetX()+j, g_pPlayer->GetY()+i);	// Sector 좌표						
								pEffect->SetCount(125+rand()%8);			// 지속되는 Frame

								g_pZone->AddEffect( pEffect );
							}
							*/

							BYTE effectSpriteType = rand()%(*g_pEffectSpriteTypeTable).GetSize();
			
							//if (g_pSDLInput->KeyDown(DIK_1))	effectSpriteType = 0;
							//else if (g_pSDLInput->KeyDown(DIK_2))	effectSpriteType = 1;
							//else if (g_pSDLInput->KeyDown(DIK_3))	effectSpriteType = 2;
							//else if (g_pSDLInput->KeyDown(DIK_4))	effectSpriteType = 3;

							
							TYPE_FRAMEID	frameID	= (*g_pEffectSpriteTypeTable)[effectSpriteType].FrameID;
							BLT_TYPE		bltType = (*g_pEffectSpriteTypeTable)[effectSpriteType].BltType;
							
							// BLT_NORMAL
							int maxFrame = g_pTopView->GetMaxEffectFrame(bltType, frameID);
							
							// Sound ... Test *_*;;
							//g_Sound.Play(g_sb1);



							MLinearEffect*	pEffect;
							//pEffect = new MParabolaEffect(BLT_EFFECT);
							pEffect = new MLinearEffect(BLT_EFFECT);

							pEffect->SetFrameID(frameID, maxFrame);		// 0번 Effect, Max 8 Frame

							// 발사 위치 Pixel좌표
							pEffect->SetPixelPosition(playerPoint.x, playerPoint.y, 0);	
							
							// 목표 위치 Pixel좌표
							pEffect->SetTarget(playerPoint.x + -i*300, 
												playerPoint.y + -j*350,
												0,
												20);	// step

							// 지속되는 Frame (목표가 있다면 별로 관계 없음 - -;)
							pEffect->SetCount(1000);							

							//pEffect->SetLight(1);							

							g_pZone->AddEffect( pEffect );
							

							//*/	
							/*
							MLinearEffect*	pEffect;
							pEffect = new MLinearEffect;

							pEffect->SetFrameID(0, 8);		// 0번 Effect, Max 8 Frame

							// 발사 위치 Pixel좌표
							int xx=rand()%800-300;
							int yy=rand()%400-400;
							int last=rand()%500;
							pEffect->SetPixelPosition(playerPoint.x+xx, playerPoint.y+yy, 0);
							
							// 목표 위치 Pixel좌표
							pEffect->SetTarget(playerPoint.x+xx-rand()%50 , 
												playerPoint.y+yy+last,
												0,
												20);

							// 지속되는 Frame (목표가 있다면 별로 관계 없음 - -;)
							pEffect->SetCount(25);						

							g_pZone->AddEffect( pEffect );
							*/
						}					
					}
				}

				start = current;
			}
		#endif
	#endif
}

//---------------------------------------------------------------------------
// UpdateGame Draw
//---------------------------------------------------------------------------
void 
CGameUpdate::UpdateDraw()
{
	//--------------------------------------------------------
	/*
	RECT rectClip = 
	{ 
		50,
		50, 
		750, 
		550 
	};
	g_pBack->SetClip(&rectClip);
	*/
	static bool outputInfo = false;		
	

	POINT	point;
	RECT	rect;
	
	//#ifdef	OUTPUT_DEBUG
		char	str[128];
	//#endif

	// buffer : InitSurface에서 SYSTEMMEMORY로 해주고 써야된다.
	//
	
	// 화면 밑에 Interface부분 지워주기...
	// [ TEST CODE ]
	/*
	rect.left = 0;
	rect.right = CLIPSURFACE_WIDTH;
	rect.top = CLIPSURFACE_HEIGHT;
	rect.bottom = SURFACE_HEIGHT;
	g_pBack->FillRect(&rect, CSDLGraphics::Color(5,7,7));
	*/

	//-----------------------------------------------------------------		
	// 마우스 좌표 다시 설정
	//-----------------------------------------------------------------
	GetCursorPos(&point);

	ScreenToClient(g_hWnd, &point);//by viva
	WindowToGamePoint(point);

	// ui에 mouse좌표 설정
	gC_vs_ui.MouseControl(M_MOVING, point.x, point.y);

	//-----------------------------------------------------------------
	// 탑뷰 Zone 출력
	//-----------------------------------------------------------------
	#ifdef OUTPUT_DEBUG_UPDATE_LOOP
			//DEBUG_ADD("dd");//[Update-Draw] Before Draw");
	#endif

//	if (true)
//	{
//		//-----------------------------------------------------------------
//		// Game 화면 출력
//		//-----------------------------------------------------------------
//		__BEGIN_PROFILE("GameDraw3D")
//			
//		const MEvent *event = g_pEventManager->GetEventByFlag(EVENTFLAG_SHAKE_SCREEN);
//		if(event != NULL)
//		{		
//			static int count = 0;
//			const int maxStep = 13;
//			const int step[maxStep] = { 0, 2, 3, 6, 5, 3, 0, -4, -6, -5, -3, -1, 0 };
//			
//			g_pTopView->Draw( step[count]*2, ((g_pEventManager->IsEvent(EVENTID_METEOR_SHAKE) == true)?((count%2)*20-10):(count%3-1)*3) );
//			
//			if(g_bFrameChanged)
//			{
//				count ++;
//				count %= maxStep;			
//			}
//		}
//		else
//			g_pTopView->Draw(0,0);
//
//		__END_PROFILE("GameDraw3D")
//
//		#ifdef OUTPUT_DEBUG_UPDATE_LOOP
//				DEBUG_ADD("us");//[Update-Draw] Before UI Show");
//		#endif
//
//		//-----------------------------------------------------------------
//		// UI 출력
//		//-----------------------------------------------------------------		
//		__BEGIN_PROFILE("UIDraw3D")
//
//		gC_vs_ui.Show();		
//
//		__END_PROFILE("UIDraw3D")
//
//
//		__BEGIN_PROFILE("DrawFPS")
//			if (outputInfo || g_pUserOption->DrawFPS)
//			{
//				//-----------------------------------------------------------------
//				// FPS 찍기	
//				//-----------------------------------------------------------------
//				sprintf(str, "%d FPS(HAL)", g_FrameRate);	
//			
//				g_pBack->GDI_Text(11,11, str, RGB(20,20,20));
//				g_pBack->GDI_Text(10,10, str, 0xFFFFFF);
//			}
//		__END_PROFILE("DrawFPS")
//		
//
//		#ifdef OUTPUT_DEBUG_UPDATE_LOOP
//				DEBUG_ADD("usk");//[Update-Draw] After UI Show");
//		#endif
//
//		__BEGIN_HELP_EVENT
//			UpdateDrawHelp();
//		__END_HELP_EVENT
//	}
//	else
	{
		//-----------------------------------------------------------------
		// Font 출력하는 Surface를 바꿔줘야 한다.
		//-----------------------------------------------------------------
		//g_SetFL2Surface( g_pLast->GetSurface() );

		//-----------------------------------------------------------------
		// Game 화면 출력
		//-----------------------------------------------------------------
		__BEGIN_PROFILE("GameDraw2D")

			// Camera interpolation: DrawZone recomputes the camera from the
			// player's exact tile+pixel position, so between ticks it would
			// still step 16 times a second. The Draw(x, y) offsets feed
			// straight into m_PlusPoint (m_PlusPoint = GetSX() - firstPoint),
			// so passing the negated interpolation gap biases the camera to
			// the player's interpolated position - which also keeps the
			// player pinned at the same screen spot, since the player sprite
			// itself is drawn at the interpolated position too.
			int camGapX = 0;
			int camGapY = 0;
			if (g_pPlayer != NULL)
			{
				camGapX = -g_pPlayer->GetDrawGapX();
				camGapY = -g_pPlayer->GetDrawGapY();
			}

			// Published for the picking code - see g_DrawCamGapX above.
			g_DrawCamGapX = camGapX;
			g_DrawCamGapY = camGapY;

			g_bInterpolateDraw = true;

//		#if defined(OUTPUT_DEBUG) && defined(_DEBUG)
//			if (g_pSDLInput->KeyDown(DIK_SPACE))
			const MEvent *event = g_pEventManager->GetEventByFlag(EVENTFLAG_SHAKE_SCREEN);
			if(event != NULL && event->IsShowTime() == true)
			{
				static int count = 0;
				const int maxStep = 13;

				// 2004, 5, 21 sobeit modify start
				//const int step[maxStep] = { 0, 2, 3, 6, 5, 3, 0, -4, -6, -5, -3, -1, 0 };
				//g_pTopView->Draw( step[count]*event->parameter3, ((g_pEventManager->IsEvent(EVENTID_METEOR_SHAKE) == true)?((count%2)*3-3):(count%3-1)*3)*event->parameter3 );
				const int step[maxStep] = { 0, 1, 2, 2, 2, 1, 0, -2, -2, -2, -1, -1, 0 };
				g_pTopView->Draw( step[count]*event->parameter3 + camGapX, ((g_pEventManager->IsEvent(EVENTID_METEOR_SHAKE) == true)?((count%2)*3-2):(count%3-1)*3)*event->parameter3 + camGapY );
				// 2004, 5, 21 sobeit modify end

				if(g_bFrameChanged)
				{
					count ++;
					count %= maxStep;
				}
			}
			else
//		#endif
			g_pTopView->Draw(camGapX, camGapY);

			g_bInterpolateDraw = false;

		__END_PROFILE("GameDraw2D")

		//g_SetFL2Surface( g_pBack->GetSurface() );

		#ifdef OUTPUT_DEBUG_UPDATE_LOOP
				//DEBUG_ADD("us");//[Update-Draw] Before UI Show");
		#endif

		//-----------------------------------------------------------------
		// UI 출력
		//-----------------------------------------------------------------		
		__BEGIN_PROFILE("UIDraw2D")

		gC_vs_ui.Show();

		__END_PROFILE("UIDraw2D")

		#ifdef OUTPUT_DEBUG_UPDATE_LOOP
				//DEBUG_ADD("usk");//[Update-Draw] After UI Show");
		#endif

//		__BEGIN_HELP_EVENT
//			UpdateDrawHelp();
//		__END_HELP_EVENT

		if (!g_pTopView->IsDrawRequest())
		{
			gC_vs_ui.DrawMousePointer();
		}

		
		__BEGIN_PROFILE("DrawFPS")
			if (outputInfo || g_pUserOption->DrawFPS)
			{
				//-----------------------------------------------------------------
				// FPS 찍기	
				//-----------------------------------------------------------------				
				SafeFormat::Format(str, "%d FPS", g_FrameRate);
				
				// Keep the counter below the HP frame, with a real dark shadow.
				TextSystem::SpriteSurfaceRenderTarget target(g_pLast);
				auto& text = TextSystem::TextService::Get();
				auto style = text.GetDefaultStyle();
				style.font = text.GetFont(12);
				style.color = TextSystem::ColorFromRGB(0x141414);
				text.DrawLine(target, str, 11, 41, 0, style);
				style.color = TextSystem::ColorFromRGB(0xFFFFFF);
				text.DrawLine(target, str, 10, 40, 0, style);
			}
		__END_PROFILE("DrawFPS")

		//-----------------------------------------------------------------
		// Last를 Back으로 copy - 3D HAL이 아닌 경우만..
		//-----------------------------------------------------------------
		__BEGIN_PROFILE("LastToBack")

//		if(true)
//		{
//			point.x = 0;
//			point.y = 0;
//			rect.left = 0;
//			rect.top = 0;
//			rect.right = SURFACE_WIDTH;
//			rect.bottom = SURFACE_HEIGHT;
//			g_pLast->SetTransparency(0x001F);
//			g_pBack->Blt( &point, g_pLast, &rect );		
//		}
//		else
//		{
			point.x = 0;
			point.y = 0;
			rect.left = 0;
			rect.top = 0;
			rect.right = g_GameRect.right;
			rect.bottom = g_GameRect.bottom;
			g_pBack->BltNoColorkey( &point, g_pLast, &rect );		
//		}

		__END_PROFILE("LastToBack")
	}
	
	#ifdef OUTPUT_DEBUG_UPDATE_LOOP
			//DEBUG_ADD("ddk");//[Update-Draw] After Draw");
	#endif


	//-------------------------------------------------------------------
	// Mouse로 선택한 좌표에 대한 debug용 code
	//-------------------------------------------------------------------


	//-----------------------------------------------------------------
	// tab누르면 MiniMap이 보여진다.
	//-----------------------------------------------------------------
	//if (g_bDrawMinimap)
	//{	
	//	g_pTopView->DrawMinimap(
	//			800-g_pZone->GetWidth()*2,	//10
	//			0, //-g_pZone->GetHeight(),	// 465
	//			1);
	//}

	//-----------------------------------------------------------------
	// 네트웍 상태가 좋은가?
	//-----------------------------------------------------------------
	/*
	if (!g_bNetStatusGood)
	{
		DEBUG_ADD("Network Condition is Bad.");

		// g_pBack->GDI_Text(351,11, (*g_pGameStringTable)[STRING_NETWORK_CONDITION_BAD].GetString(), RGB(10,10,10));
		TextSystem::TextService::RenderText(351, 11, (*g_pGameStringTable)[STRING_NETWORK_CONDITION_BAD].GetString());
		// g_pBack->GDI_Text(350,10, (*g_pGameStringTable)[STRING_NETWORK_CONDITION_BAD].GetString(), RGB(230,50,50));		
		TextSystem::TextService::RenderText(350, 10, (*g_pGameStringTable)[STRING_NETWORK_CONDITION_BAD].GetString());		
	}
	*/

	//-----------------------------------------------------------------
	// 죽은 상태..
	//-----------------------------------------------------------------
	if (g_pPlayer->IsDead())
	{
		if (g_pPlayer->GetActionCount()==g_pPlayer->GetActionCountMax())		
		{	
			static BYTE a = 0;
			static BYTE count = 0;

			int second = g_pPlayer->GetDeadDelayLast();
			//char str[128];

			// 30~20
			int blackValue = 20;
			if (second >= 3)
			{
				//blackValue = max(0, (g_pClientConfig->DELAY_PLAYER_DEAD/1000 - (second-5)*2)) * 2;
				blackValue = min(31, second+20);
			}

			// 죽는 동작이 끝난 경우에 화면을 검게...
			g_pTopView->SetFadeStart(blackValue, 0, 0, 5,5,5);
		

			// 6 frame씩 보여준다.
			if ((a & 0x01)==0)
			{
				//if (second==0)
				{
					//sprintf(str, "[SPACE]를 누르면 되살아날 수 있습니다.", second);
					//g_pBack->GDI_Text(270,400, str, RGB(230,230,230));
				}
				//else
				{
				//	sprintf(str, (*g_pGameStringTable)[STRING_RESURRECTION_AFTER_SECONDS].GetString(), second);
					//strcpy(str, "... Please Wait for Your Resurrection ...");
				//	g_pBack->GDI_Text(301,401, str, RGB(0,0,0)); 
				//	g_pBack->GDI_Text(300,400, str, RGB(230,230,230));
				}
				
				// 6초가 지나고나서 부활버튼을 띄운다.
				if (second < 4
					&& g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
				{
					// FixMe Please oh shit by ssuk
					if( (g_pWarManager->IsExist( g_pZone->GetID() ) || g_pZone->GetID() == 71 || g_pZone->GetID() == 72 || g_pZone->GetID() == 73) && gC_vs_ui.IsRunningBloodBibleStatus() )
						gC_vs_ui.RequestShrineMinimap( g_pClientConfig->DELAY_PLAYER_RESURRECT_TIMEOUT );

					gC_vs_ui.RequestDie( g_pClientConfig->DELAY_PLAYER_RESURRECT_TIMEOUT );

					g_pTempInformation->SetMode(TempInformation::MODE_WAIT_RESURRECT);
					bool bResurrect = false, bElixir= false, bEternity= false, IsSiegeAttacker= false ;

					// 2005, 1, 18, sobeit add start - 퀘스트 action으로 죽은 경우엔 아래를 체크 안한다.
					if(0 == g_pPlayer->GetResurrectZoneID())
					// 2005, 1, 18, sobeit add end
					{
						if( g_pInventory->FindItem( ITEM_CLASS_RESURRECT_ITEM, 0 ) != NULL )
							bResurrect = true;

						if(g_pInventory->FindItem( ITEM_CLASS_RESURRECT_ITEM, 1 ) != NULL )
							bElixir = true;

						//bEternity = (*g_pSkillInfoTable)[SKILL_ETERNITY].IsEnable();						
						bEternity = g_pSkillAvailable->IsEnableSkill( SKILL_ETERNITY ) &&
									(*g_pSkillInfoTable)[SKILL_ETERNITY].IsAvailableTime() 
									// 2004, 11, 11, sobeit add start - 레벨 검사도 한다.. 요거..g_char_slot_ingame.DOMAIN_HEAL는 뭐지? -_-; 버그 수정..ㅡㅡ;
									&& (*g_pSkillManager)[SKILLDOMAIN_HEAL].GetDomainLevel() >= (*g_pSkillInfoTable)[SKILL_ETERNITY].GetLearnLevel();
									// 2004, 11, 11, sobeit add end
						IsSiegeAttacker = g_pZone->GetPKType() == PK_TYPE_SIEGE &&
										 ( g_pPlayer->HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_1) ||
										 g_pPlayer->HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_2) ||
										 g_pPlayer->HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_3) ||
										 g_pPlayer->HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_4) ||
										 g_pPlayer->HasEffectStatus(EFFECTSTATUS_SIEGE_ATTACKER_5) );
					}
					gC_vs_ui.RequestResurrect( bResurrect, bElixir, bEternity , IsSiegeAttacker);
				}
			}

			static MonotonicClock::TimePoint lastTime = g_FrameNow;
			if (g_FrameNow - lastTime >= MonotonicClock::Millis(g_UpdateDelay))
			{
				if (++count==6) { count = 0; a++; }
				lastTime = g_FrameNow;
			}			
		}
	}

	//-----------------------------------------------------------------
	// Mouse Cursor 위치의 image를 기억시켜두기
	//-----------------------------------------------------------------
	//
	// FullScreen이고...
	// 바로 전의 FPS가 출력FPS 한계를 넘을 경우....
	// 커서 출력 위치를 기억한다.
	// 	
	// The smooth-cursor surface ping-pong (Store here, Restore/Flip in the
	// cursor-only path of Update) existed to move the mouse between the old
	// 16 fps world draws. Full frames now go out at the refresh rate with the
	// cursor drawn into each one, so the machinery is retired - its saved
	// background rectangles would smear over interpolated frames.
	g_bSmoothCursor = false;

	//-----------------------------------------------------------------
	// Mouse 그리기
	//-----------------------------------------------------------------
	if (!g_pTopView->IsDrawRequest())
	{
		gC_vs_ui.DrawMousePointer();
	}

	//-----------------------------------------------------------------
	// Debug Information출력
	//-----------------------------------------------------------------
	__BEGIN_PROFILE("DrawDebugInfo")

	[[maybe_unused]] PrintInfo* pPrintInfo = &gpC_base->m_chatting_pi;

	#ifdef	OUTPUT_DEBUG
		static DWORD lastCount = 0;	
		
		if (g_bSlideScreenShot)
		{
			if (g_bSlideRectSelect)
			{
				g_rectScreenShot.right = g_x;
				g_rectScreenShot.bottom = g_y;
			}

			RECT rect = g_rectScreenShot;

			g_pBack->DrawRect( &rect, CSDLGraphics::Color(3, 30, 10) );
		}
		
		if (g_pSDLInput->KeyDown(DIK_LSHIFT) && g_pSDLInput->KeyDown(DIK_RSHIFT) 
			&& g_CurrentFrame-lastCount > 7)
		{
			outputInfo = !outputInfo;
			lastCount = g_CurrentFrame;
		}

		//----------------------------------------------------------------
		// UDP packet test
		//----------------------------------------------------------------
		#ifdef __METROTECH_TEST__

			if (g_pPlayer!=NULL)
				
			{
				if (g_iAutoHealPotion && !g_pPlayer->IsVampire())
				{
					RECT rect = 
					{ 
						2,
						448,
						10,
						455
					};

					if(g_iAutoHealPotion == 1)
						g_pBack->FillRect(&rect, CSDLGraphics::Color(30, 10, 10));
					else if(g_iAutoHealPotion == 2)
						g_pBack->FillRect(&rect, CSDLGraphics::Color(30, 20, 10));
					else if(g_iAutoHealPotion == 3)
						g_pBack->FillRect(&rect, CSDLGraphics::Color(30, 10, 20));
				}

				if (g_bAutoManaPotion && !g_pPlayer->IsVampire())
				{
					RECT rect = 
					{ 
						12,
						448,
						20,
						455
					};

					g_pBack->FillRect(&rect, CSDLGraphics::Color(10, 10, 30));
				}

				if (g_iAutoReload && g_pPlayer->IsSlayer())
				{
//					char sz_temp[3];
//					wsprintf(sz_temp, "%d", g_iAutoReload-1);
//					g_PrintColorStr(22, 448, sz_temp, gpC_base->m_chatting_pi, RGB_GREEN);

					RECT rect = 
					{ 
						22,
						448,
						30,
						455
					};

					g_pBack->FillRect(&rect, CSDLGraphics::Color(10, 30, 10));

					rect.left++;
					rect.right--;
					rect.bottom++;

					for(int i = 0; i < g_iAutoReload-1; i++)
					{
						rect.bottom -= 2;
						rect.top = rect.bottom -1;
						g_pBack->FillRect(&rect, CSDLGraphics::Color(0, 0, 0));
					}
				}

				if (g_bLight)
				{
					RECT rect = 
					{ 
						32,
						448,
						40,
						455
					};
					
					g_pBack->FillRect(&rect, CSDLGraphics::Color(10, 30, 30));
				}

				if (g_iSpeed)
				{
					RECT rect = 
					{ 
						42,
						448,
						50,
						455
					};

					if(g_iSpeed < 0)
						g_pBack->FillRect(&rect, CSDLGraphics::Color(30, 0, 0));
					else
						g_pBack->FillRect(&rect, CSDLGraphics::Color(30, 30, 30));
					
					rect.left++;
					rect.right--;
					rect.bottom++;
					
					for(int i = 0; i < abs(g_iSpeed)-1; i++)
					{
						rect.bottom -= 2;
						rect.top = rect.bottom -1;
						g_pBack->FillRect(&rect, CSDLGraphics::Color(0, 0, 0));
					}
				}
			}

			if (outputInfo)
			{
				const int numValue = 5;
				int value[numValue] = 
				{
					g_UDPTest.UDPPacketAvailable,
					g_UDPTest.UDPPacketReceive,
					g_UDPTest.UDPPacketRead,
					g_UDPTest.UDPPacketExecute,
					g_UDPTest.UDPPacketSend
				};

				WORD color[numValue] =
				{
					CSDLGraphics::Color(10, 20, 20),
					CSDLGraphics::Color(20, 20, 20),
					CSDLGraphics::Color(30, 20, 20),
					CSDLGraphics::Color(20, 30, 20),
					CSDLGraphics::Color(20, 20, 30)
				};

				for (int v=0; v<numValue; v++)
				{
					RECT rect = 
					{ 
						210,
						455 + v*8,
						210 + (value[v]%100)*4,
						455 + v*8 + 8
					};

					g_pBack->DrawRect(&rect, color[v]);
				}

				if (outputInfo)
				{
					if (!true)
					{
						g_SetFL2Surface( g_pBack );
					}

					const COLORREF color = RGB(28<<3, 28<<3, 28<<3);				
					pPrintInfo->text_color	= color;
		
					char str[10];
					for (int i=0; i<numValue; i++)
					{
						SafeFormat::Format(str, "%d", value[i]);
						g_Print(200 + i*40, 455 + i*8, str, pPrintInfo);
					}				

					if (!true)
					{
						g_SetFL2Surface( g_pLast );
					}
				}
			}
		#endif


		if (outputInfo)//!g_pSDLInput->KeyDown(DIK_RSHIFT))
		{
			g_pTopView->DrawDebugInfo( g_pBack );

			if (!true)
			{
				g_SetFL2Surface( g_pBack );
			}

			if (g_bPutMessage && g_pDebugMessage!=NULL)
			{
				int strY = 565;
				
				for (int c=g_pDebugMessage->GetSize()-1; c>=0; c--)
				{
					if ((*g_pDebugMessage)[c][0] != NULL)
					{
						//g_pBack->GDI_Text(10,strY, g_DebugMessage[c], RGB(230,230,230));

						const COLORREF debugColor = RGB(28<<3, 28<<3, 28<<3);				
						pPrintInfo->text_color	= debugColor;
			
						g_Print(10, strY, (*g_pDebugMessage)[c], pPrintInfo);
						strY-=15;
					}
				}

				// Debug Log Filename 출력하기
				if (g_pDebugMessage->GetFilename()!=NULL)
				{
					SafeFormat::Format(str, "LogFile : %s", g_pDebugMessage->GetFilename());

					const COLORREF txColor = RGB(31<<3, 11<<3, 11<<3);										
					pPrintInfo->text_color	= txColor;
			
					g_Print(10, 580, str, pPrintInfo);
				}				
			}	
			//*/
			
			if (g_pLoadingThread!=NULL && g_pLoadingThread->IsWorking())
			{		
				SafeFormat::Format(str, "<<< Loading >>>");
				//g_pBack->GDI_Text(700,580, str, 0xFFFFFF);
				const COLORREF txColor = RGB(31<<3, 31<<3, 31<<3);				

				pPrintInfo->text_color	= txColor;
				
				g_Print(300, 5, str, pPrintInfo);
			}
			
			// Missile종류
			///*	 
			if (g_pPlayer->GetSpecialActionInfo() != ACTIONINFO_NULL)
			{
				SafeFormat::Format(str, "[%d] %s", g_pPlayer->GetSpecialActionInfo(), (*g_pActionInfoTable)[g_pPlayer->GetSpecialActionInfo()].GetName());
				
				//g_pBack->GDI_Text(550,10, str, RGB(20,20,20));
				//g_pBack->GDI_Text(551,11, str, RGB(120,255,120));


				const COLORREF bkColor = RGB(0, 0, 0);
				const COLORREF txColor = RGB(16<<3, 31<<3, 16<<3);				

				pPrintInfo->text_color	= bkColor;
				g_Print(500, 10, str, pPrintInfo);

				pPrintInfo->text_color	= txColor;
				g_Print(501, 11, str, pPrintInfo);				
			}	
			//*/

			// 가진 돈
			SafeFormat::Format(str, "Money : %d", (*g_pMoneyManager).GetMoney());
			
			//g_pBack->GDI_Text(550,10, str, RGB(20,20,20));
			//g_pBack->GDI_Text(551,11, str, RGB(120,255,120));
			
			const COLORREF bkMoneyColor = RGB(0, 0, 0);
			const COLORREF txMoneyColor = RGB(16<<3, 31<<3, 16<<3);				

			pPrintInfo->text_color	= bkMoneyColor;
			g_Print(500, 30, str, pPrintInfo);

			pPrintInfo->text_color	= txMoneyColor;
			g_Print(501, 31, str, pPrintInfo);

			SafeFormat::Format(str, "QuestColorset = %d", g_pClientConfig->QuestItemColorSet);
			pPrintInfo->text_color	= txMoneyColor;
			g_Print(501, 51, str, pPrintInfo);


			if (!true)
			{
				g_SetFL2Surface( g_pLast );
			}
		}
	#endif


	__END_PROFILE("DrawDebugInfo")

	//---------------------------------------------------------------------
	//
	// [ TEST CODE ] - Sword Domain의 Skill들을 가지고 작업한다.
	//
	//---------------------------------------------------------------------
	/*
	MSkillDomain& swordDomain = g_SkillManager[SKILLDOMAIN_SWORD];

	//---------------------------------------------------------------------
	// 몇가지 skill을 배웠다고 표시한다.
	//---------------------------------------------------------------------
	//swordDomain.LearnSkill( SKILL_DOUBLE_IMPACT );
	//swordDomain.LearnSkill( SKILL_TRIPLE_SLASHER );
	//swordDomain.LearnSkill( SKILL_RAINBOW_SLASHER );
	//swordDomain.LearnSkill( SKILL_HURRICANE_COMBO );

	//swordDomain.UnLearnSkill( SKILL_HURRICANE_COMBO );

	//---------------------------------------------------------------------
	// Sword Domain의 모든 기술들을 출력한다.
	//---------------------------------------------------------------------
	swordDomain.SetBegin();
	
	while (swordDomain.IsNotEnd())
	{
		// skill의 id와 status
		ACTIONINFO					id		= swordDomain.GetSkillID();
		MSkillDomain::SKILLSTATUS	status	= swordDomain.GetSkillStatus();

		//---------------------------------------
		// status는 다음과 같다. 
		//---------------------------------------
		//	MSkillDomain::SKILLSTATUS_LEARNED		// 배웠다.
		//	MSkillDomain::SKILLSTATUS_NEXT			// 다음에 배울 수 있다.
		//	MSkillDomain::SKILLSTATUS_OTHER			// 아직은 배울 수 없다.	
		//---------------------------------------
		
		//---------------------------------------
		// id를 알면 g_SkillInfoTable에서 
		// 그 id의 skill에 대한 정보를 얻을 수 있다.
		//---------------------------------------
		COLORREF color;
		switch (status)
		{
			case MSkillDomain::SKILLSTATUS_LEARNED :	
				color = RGB(250, 250, 250);
			break;

			case MSkillDomain::SKILLSTATUS_NEXT :	
				color = RGB(10, 10, 250);
			break;

			case MSkillDomain::SKILLSTATUS_OTHER :	
				color = RGB(120, 120, 120);
			break;
		}

		SKILLINFO_NODE& skillInfo = g_SkillInfoTable[id];
		
		int x = skillInfo.GetX()*2/3 - strlen(skillInfo.GetName())*8 + 100;
		int y = skillInfo.GetY();
		// g_pBack->GDI_Text(x+1, y+1, skillInfo.GetName(), 0);
		TextSystem::TextService::RenderText(x+1, y+1, skillInfo.GetName());
		// g_pBack->GDI_Text(x, y, skillInfo.GetName(), color);
		TextSystem::TextService::RenderText(x, y, skillInfo.GetName());

		// 다음
		swordDomain.Next();
	}
	//*/

	// 커서 출력
	//g_pBack->HLine(point.x-7, point.y, 7, color);
	//g_pBack->HLine(point.x+1, point.y, 7, color);
	//g_pBack->VLine(point.x, point.y-7, 7, color);
	//g_pBack->VLine(point.x, point.y+1, 7, color);	
	//*/
	

	//-----------------------------------------------------------------
	// flip
	//-----------------------------------------------------------------
	CSDLGraphics::Flip();
}

//-----------------------------------------------------------------------------
// Update Draw Help
//-----------------------------------------------------------------------------
void
CGameUpdate::UpdateDrawHelp()
{
	int strX = 350;
	int strY = (g_pPlayer->IsSlayer()? 430 : 460);

	PrintInfo* pPrintInfo = &gpC_base->m_info_pi;

	for (int c=g_pHelpMessage->GetSize()-1; c>=0; c--)
	{
		if ((*g_pHelpMessage)[c][0] != '\0')
		{
			const COLORREF color = RGB(28<<3, 28<<3, 8<<3);

			pPrintInfo->text_color = 0;
			g_Print(strX+1, strY+1, (*g_pHelpMessage)[c], pPrintInfo);

			pPrintInfo->text_color = color;
			g_Print(strX, strY, (*g_pHelpMessage)[c], pPrintInfo);

			strY-=20;
		}		
	}

	// scroll every 5 seconds
	static MonotonicClock::TimePoint HelplastTime = g_FrameNow;
	if (g_FrameNow - HelplastTime >= MonotonicClock::Millis(g_pClientConfig->DELAY_GAMEMESSAGE))
	{
		g_pHelpMessage->Add("\0");
		HelplastTime = g_FrameNow;
	}
}

//-----------------------------------------------------------------------------
// PacketTest
//-----------------------------------------------------------------------------
void
PacketTest()
{
	MZone::CREATURE_MAP::const_iterator iCreature = g_pZone->GetCreatureBegin();

	for (int i=0; i<g_pZone->GetCreatureNumber(); i++)
	{
		MCreature* pCreature = (*iCreature).second;

		if (pCreature->GetID()!=g_pPlayer->GetID()
			&& !pCreature->IsNPC()
			&& (rand()%8==0))
		{	
			int x = pCreature->GetX();
			int y = pCreature->GetY();
			int dir = rand()%8;
			pCreature->PacketMove(x, y, dir);
		}

		iCreature++;
	}
}

///以下是检测系统是否有登陆二个用户
int GetCurrentUserNumber()
{
	HANDLE			hSnapShot;
    PROCESSENTRY32	ProcessInfo;
	char*			pstrExeName;
    DWORD			ret = 0;
	DWORD			dwwinlogon = 0;
	DWORD			dwexplorer = 0;

	ProcessInfo.dwSize = sizeof(ProcessInfo);
    hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
	if (hSnapShot == INVALID_HANDLE_VALUE) return 0;
	if(!Process32First(hSnapShot,&ProcessInfo)) return 0;
	do
	{
		pstrExeName = strrchr(ProcessInfo.szExeFile,'\\');
        if(!pstrExeName)
            pstrExeName = ProcessInfo.szExeFile;
        else
            pstrExeName++; 
		if(!_stricmp(pstrExeName, "winlogon.exe"))
		{
			dwwinlogon++;
		}
		if(!_stricmp(pstrExeName, "explorer.exe"))
		{
			dwexplorer++;
		}
		/*
		if(!stricmp(pstrExeName, "csrss.exe"))
		{
			dwcsrss++;
		}
		*/
		if (dwwinlogon>1 && dwexplorer >1)
		{
			ret =2;
		}
	}while(Process32Next(hSnapShot,&ProcessInfo));
	if(hSnapShot)
	{
		CloseHandle(hSnapShot);	
	}
	
	return ret;
}
//-----------------------------------------------------------------------------
// Update Game
//-----------------------------------------------------------------------------
// 게임 실행 중...
//-----------------------------------------------------------------------------
void 
CGameUpdate::Update(void)
{
	#ifdef OUTPUT_DEBUG_UPDATE_LOOP
		//DEBUG_ADD("UB");
	#endif

	__BEGIN_PROFILE("GameUpdate")

	static MonotonicClock::TimePoint lastTime = g_FrameNow;
	//bool bChanged = false;

	if (g_Mode!=MODE_GAME || g_pClientConfig==NULL)
	{
		#ifdef OUTPUT_DEBUG_UPDATE_LOOP
			//DEBUG_ADD("UE1");
		#endif
		return;
	}
	
	//------------------------------------------
	// Logout 할 시간인치 체크한다.
	//------------------------------------------
	if (g_pUserInformation!=NULL
		&& g_pUserInformation->IsLogoutDue(g_FrameNow)
		&& g_pPlayer->GetWaitVerify() == MPlayer::WAIT_VERIFY_LOGOUT)
	{
		ExecuteLogout();

		g_pUserInformation->CancelLogout();

		#ifdef OUTPUT_DEBUG_UPDATE_LOOP
			//DEBUG_ADD("UE2");
		#endif

		return;
	}

	//------------------------------------------
	// Sound Stream
	//------------------------------------------
//	if (g_pSDLStream!=NULL)
//	{
//		#ifdef OUTPUT_DEBUG_UPDATE_LOOP
//			DEBUG_ADD("DSU");
//		#endif
//
//		g_pSDLStream->Update();
//
//		#ifdef OUTPUT_DEBUG_UPDATE_LOOP
//			DEBUG_ADD("DSUk");
//		#endif
//	}


	//---------------------------------------------
	// Sound관련
	//---------------------------------------------
	// 매 loop 마다 안해줘도 되지 않을까..
	#ifdef OUTPUT_DEBUG_UPDATE_LOOP
		//DEBUG_ADD("DXRTDB");
	#endif

	g_SDLAudio.ReleaseTerminatedDuplicateBuffer();

	#ifdef OUTPUT_DEBUG_UPDATE_LOOP
		//DEBUG_ADD("DXRTDBk");
	#endif
	
	static MonotonicClock::TimePoint nextSoundCheckTime = g_FrameNow;

	if (g_FrameNow > nextSoundCheckTime)
	{
		// sounds played this second
		g_SoundPerSecond = 0;

		// one second on
		nextSoundCheckTime = g_FrameNow + MonotonicClock::Millis(1000);
	}

	//---------------------------------------------------
	//
	// 일정 시간마다 한번씩 처리를 해준다.
	//
	//---------------------------------------------------
	// [예제]
	// g_UpdateDelay	: 100
	// lastTime			: 1000 
	// currentTime		: 1240 
	//
	// 1200, 1100의 두번을 처리하게 된다.
	//---------------------------------------------------

	int k = g_pClientConfig->MAX_UPDATE_ONETIME;		// 12 frame * 1 Tile.. 

	#ifdef OUTPUT_DEBUG
		if (g_bSlideScreenShot)
		{
			k = 1;
		}
	#endif

	UpdateMouse();

	//------------------------------------------
	// 무한루프 예방도 되고...
	// k값마다 한번씩은 Draw를 해주기 때문에
	// Frame Skipping을 적용시킨다.				
	//------------------------------------------
//CRYPT_START
//VM_START

	if(g_UpdateDelay!=62)
	{
		for(int i =0;i<100;i++)
		{
			CGVerifyTime _CGVerifyTime;
			g_pSocket->sendPacket( &_CGVerifyTime );
		}
		g_UpdateDelay=62;
	}
	if (g_CheckTimeNum>30)
	{
		g_CheckTimeNum=0;
		// 2006.11.07  去除程序双开检测 Coffee 
		/*
		if(GetCurrentUserNumber()>1)
		{
			//char* szInfo="程序已检测出你使用了双用户登陆系统，请退出一个Windows用户再登陆游戏。";
			//MessageBox(0,szInfo,"놔댄",MB_OK);
			g_bNeedUpdate = TRUE;
			SetMode(MODE_QUIT);
			g_ModeNext = MODE_QUIT;
		}
		*/
		// end Coffee
	}else
	{
		g_CheckTimeNum++;
	}
	int tmp =100;
	tmp=tmp-20;
	tmp=tmp+40;
	tmp=tmp-38;
	tmp=tmp-19;
	tmp--;
	FindWindow("a","b");
	if (g_FrameNow - lastTime >= MonotonicClock::Millis(tmp))
	{
		// 변화된것이 있다고 check
		g_bFrameChanged = true;

		do
		{
			#ifdef OUTPUT_DEBUG_UPDATE_LOOP
				//DEBUG_ADD("CGUP");
			#endif

			// 게임의 frame수 증가
			g_CurrentFrame++;
		
			//if (--k==0) break;

			//------------------------------------------
			//
			// Socket 입력 처리
			//
			//------------------------------------------
			__BEGIN_PROFILE("GameSocketInput")
			if (!UpdateSocketInput())
			{
				DEBUG_ADD("[CGameUpdate] UpdateSocketInput Failed");

				__END_PROFILE("GameSocketInput")

				return;
			}
			__END_PROFILE("GameSocketInput")

			if (g_Mode!=MODE_GAME)
			{
				DEBUG_ADD("[CGameUpdate] Not MODE_GAME");
				
				return;
			}

			#ifdef OUTPUT_DEBUG_UPDATE_LOOP
				//DEBUG_ADD( "USInOK" );
			#endif

			//------------------------------------------
			//
			// Input 처리
			//
			//------------------------------------------
			if (g_bActiveGame)
			{
				// Input값을 읽어온다.
				UpdateInput();
				
				__BEGIN_PROFILE("ProcessInput")

				#ifdef OUTPUT_DEBUG_UPDATE_LOOP
					//DEBUG_ADD( "PI" );
				#endif

				ProcessInput();

				__END_PROFILE("ProcessInput")
			}

			__BEGIN_PROFILE("UIProcess")

			#ifdef OUTPUT_DEBUG_UPDATE_LOOP
				//DEBUG_ADD( "UI_pro" );
			#endif

			gC_vs_ui.Process();

			#ifdef OUTPUT_DEBUG_UPDATE_LOOP
				//DEBUG_ADD( "UI_pro ok" );
			#endif

			__END_PROFILE("UIProcess")

			if (!g_bActiveApp || g_Mode!=MODE_GAME)
			{
				DEBUG_ADD("[CGameUpdate] Not bActiveApp");

				return;
			}

//			CheckInvalidProcess();
			//------------------------------------------
			// 접속을 유지해야하는 경우에는 접속 유지..
			//------------------------------------------			
			//KeepConnection();

			//------------------------------------------
			//
			// Socket 출력 처리
			//
			//------------------------------------------
			__BEGIN_PROFILE("GameSocketOutput")

			#ifdef OUTPUT_DEBUG_UPDATE_LOOP
				//DEBUG_ADD( "USOut" );
			#endif

			if (!UpdateSocketOutput() || g_Mode!=MODE_GAME)
			{			
				DEBUG_ADD("[CGameUpdate] UpdateSocketOutput Failed or Not MODE_GAME");

				__END_PROFILE("GameSocketOutput")
				
				return;
			}

			__END_PROFILE("GameSocketOutput")

			//------------------------------------------
			//
			//	날씨 처리
			//
			//------------------------------------------
			if (g_pWeather!=NULL)
			{
				#ifdef OUTPUT_DEBUG_UPDATE_LOOP
					//DEBUG_ADD( "Weather" );
				#endif

				g_pWeather->Action();

				#ifdef OUTPUT_DEBUG_UPDATE_LOOP
					//DEBUG_ADD( "WeaOK" );
				#endif
			}
			///*
			
			//*/
			

			//------------------------------------------
			//
			//	Effect Manager
			//
			//-------------------------------------------
			if (g_pInventoryEffectManager!=NULL)
			{
				#ifdef OUTPUT_DEBUG_UPDATE_LOOP
					//DEBUG_ADD( "IEM" );
				#endif

				g_pInventoryEffectManager->Update();

				#ifdef OUTPUT_DEBUG_UPDATE_LOOP
					//DEBUG_ADD( "IEM ok" );
				#endif
			}
			
			//------------------------------------------
			//
			//	Event Manager
			//
			//-------------------------------------------
			if (g_pEventManager != NULL)
			{
				g_pEventManager->ProcessEvent();
			}

			//------------------------------------------
			//
			// Zone의 object들을 update한다.
			//
			//------------------------------------------
			#ifdef OUTPUT_DEBUG_UPDATE_LOOP
				//DEBUG_ADD("z");//[Update] Before ZoneUpdate");
			#endif
				
			__BEGIN_PROFILE("ZoneUpdate")

			g_pZone->Update();			

			__END_PROFILE("ZoneUpdate")

			#ifdef OUTPUT_DEBUG_UPDATE_LOOP
				//DEBUG_ADD("zk");//[Update] After ZoneUpdate");
			#endif

		
			
			lastTime += MonotonicClock::Millis(g_UpdateDelay);
		
		} while (g_FrameNow - lastTime >= MonotonicClock::Millis(g_UpdateDelay)  
					&& --k);
//add by viva : friend system CG_UPDATE
#ifdef __FRIEND_SYSTEM_VIVA__
//		static DWORD friendUpdateTime = 0;
//		if(friendUpdateTime<g_CurrentTime)
//		{
//			GCFriendChatting gcFriend;
//			gcFriend.setCommand(CG_UPDATE);
//			g_pSocket->sendPacket(&gcFriend);
//			friendUpdateTime = g_CurrentTime + 10000;
//		}
#endif
//end

/* add by sonic 2006.9.12 */
//增加时间检测
		//------------------------------------------------------------------
		// 1분 마다 한번씩 garbarge packet을 보낸다.
		//------------------------------------------------------------------
#ifdef PLATFORM_WINDOWS
		// Windows-specific anti-cheat time verification - Windows only
		DWORD nextTimeValue = 60000;
		static MonotonicClock::TimePoint nextTime = MonotonicClock::Now() + MonotonicClock::Millis(60000);

		// g_MyCheckTime measures how much time has passed since the 60-second
		// reset below, and 97620 means the client believes it ran far ahead of
		// the wall clock - a speed hack. Two things made it accuse every honest
		// client instead:
		//
		//  - It added a fixed 72 per frame, so it counted frames, not
		//    milliseconds. Above ~23 fps it reached 97620 before the reset could
		//    run, and the client quit itself about 23 seconds into the game.
		//  - timeGetTime() is SDL_GetTicks() here (see basic/Platform.h), so it
		//    counts from SDL init rather than from boot, which made
		//    "g_CurrentTime < 60000" flag the first minute of every session.
		//
		// Accumulate the real elapsed milliseconds instead, clamped so a
		// debugger pause or a suspended window does not read as a time jump.
		static MonotonicClock::TimePoint lastCheckTime = g_FrameNow;
		DWORD checkTimeGap = (g_FrameNow > lastCheckTime)? (DWORD)(g_FrameNow - lastCheckTime).count() : 0;
		lastCheckTime = g_FrameNow;
		if (checkTimeGap > 1000) checkTimeGap = 1000;
		g_MyCheckTime += (int)checkTimeGap;

		// add by Coffee 2006.11.12
		if(g_MyCheckTime>=97620)
		{
			for(int i =0 ;i<10; i++)
			{
				CGVerifyTime _CGVerifyTime;
				g_CheckErrorTime++;
				g_pSocket->sendPacket( &_CGVerifyTime );
			}
		}
		// end by Coffee
		if (MonotonicClock::Now() > nextTime)		// 60 * 1000
		{
			CGVerifyTime _CGVerifyTime;
			if(g_MyCheckTime>=97620)
			{
					g_CheckErrorTime++;
					nextTimeValue=5390;
					g_pSocket->sendPacket( &_CGVerifyTime );
					nextTime = MonotonicClock::Now() + MonotonicClock::Millis(nextTimeValue);
			}else
			{
				nextTimeValue =60000;
				g_pSocket->sendPacket( &_CGVerifyTime );

				nextTime = MonotonicClock::Now() + MonotonicClock::Millis(nextTimeValue);
			}
			g_MyCheckTime=0;
		}
#endif // PLATFORM_WINDOWS
		/*
		g_MyCheckTime+=72;
		if(g_MyCheckTime>=67620)
		{
			CGVerifyTime _CGVerifyTime1;
			g_pSocket->sendPacket( &_CGVerifyTime1 );
			g_MyCheckTime=0;
		}
		*/
/* add end by sonic */
		//CRYPT_END
		// k번 만큼 update했는데도..
		// 더 update해야할 것이 있으면.. 무시한다..
		// 무시할까??
		// 디아처럼.. 갑자기 달리기는 어떨까? -_-;
		//检测外挂

#ifdef PLATFORM_WINDOWS
		// Windows-specific anti-cheat error check - Windows only
		if(g_CheckErrorTime>=5)
		{
			g_CheckErrorTime=0;
			SetMode(MODE_QUIT);
			return;
		}
#endif // PLATFORM_WINDOWS
		
		static int OnetimeUpdateCount = 0;
		if (k==0)
		{
			//lastTime = g_CurrentTime;  // 무시하는 경우..
			OnetimeUpdateCount++;
	
			if (OnetimeUpdateCount > g_pClientConfig->MAX_UPDATE_ONETIME_COUNT)
			{
				// 더 이상은 update못하게 한다.
				lastTime = g_FrameNow;
				OnetimeUpdateCount = 0;
			}
		}
		else
		{
			OnetimeUpdateCount = 0;
		}

		#ifdef OUTPUT_DEBUG_UPDATE_LOOP
			//DEBUG_ADD("CGUPOK");
		#endif
	}
	else
	{
		g_bFrameChanged = false;
	}
//CRYPT_END
	//---------------------------------------------
	// Update 회수 출력
	//---------------------------------------------
	/*
	#ifdef OUTPUT_DEBUG
		if (k!=g_pClientConfig->MAX_UPDATE_ONETIME)
		{
			if (g_bActiveGame && bChanged)
			{
					DEBUG_ADD_FORMAT("[ Update (%d) times & Draw ]", 12-k);
			}
			else
			{
					DEBUG_ADD_FORMAT("[ Update (%d) times ]", 12-k);
			}
		}
	#endif
	*/

	if (g_pGameTime!=NULL)
	{
		static int lastHour = g_pGameTime->GetHour()+1;

		int currentHour = g_pGameTime->GetHour();

		if (lastHour != currentHour)
		{
			//---------------------------------------------
			// 시간이 바꼈으면 그림자도 바꿔야 한다.
			//---------------------------------------------
			if (g_pTopView!=NULL)
			{
				g_pTopView->ClearShadowManager();
			}

			lastHour = g_pGameTime->GetHour();

			//---------------------------------------------
			// 음악도 바꾼다.
			//---------------------------------------------
			// 짝수 시간대에만 음악 연주..
			if ((currentHour & 0x01)==0)
			{
				PlayMusicCurrentZone();
			}

		}

		
		//---------------------------------------------
		// 시간
		//---------------------------------------------
		char str[80];

		if (g_pGameStringTable!=NULL)
		{
			g_pGameTime->SetCurrentTime( g_FrameNow );
		
			SafeFormat::Format(str, GetGameString(STRING_DRAW_GAME_TIME),
							g_pGameTime->GetHour(),
							g_pGameTime->GetMinute(),
							g_pGameTime->GetSecond()
					);
			
			gC_vs_ui.SetTime( str );
		}

		//---------------------------------------------
		// 시간
		//---------------------------------------------
		SafeFormat::Format(str, GetGameString(STRING_DRAW_GAME_DATE),
					g_pGameTime->GetYear(),
					g_pGameTime->GetMonth(),
					g_pGameTime->GetDay()
				);

		gC_vs_ui.SetDate( str );
	}
	
	
	//---------------------------------------------
	// Zone의 환경 사운드 출력
	//---------------------------------------------
	if (g_pZoneSoundManager!=NULL)
	{
		g_pZoneSoundManager->UpdateSound();
	}

	//---------------------------------------------
	// UI에 player 좌표 설정
	//---------------------------------------------
	gC_vs_ui.SetXY( g_pPlayer->GetX(), g_pPlayer->GetY() );

	//---------------------------------------------
	//
	// Draw - 변화된게 있으면 그려준다.
	//
	//---------------------------------------------
// 	if (g_bActiveGame
// #ifdef OUTPUT_DEBUG
// 		|| g_bTestMode
// #endif
// 		)
// 	{			
	{
		static int oldX = -1;
		static int oldY = -1;
		static DWORD oldFrame = 0;

		//------------------------------------------------------
		// Update됐으면..
		// 새로운 게임 화면을 그려준다.
		//------------------------------------------------------
		// Draw between ticks too - positions interpolate by g_DrawAlphaNum
		// and vsync paces the presents (CDirectDraw.cpp). lastTime is the
		// scheduled time of the last consumed tick, so the fraction runs
		// 0 -> 256 across each 62 ms window. The 15 ms fallback cap only
		// matters when the renderer ignores vsync (SDL_RENDER_DRIVER=
		// software) - it keeps the message loop from spin-presenting.
		g_DrawAlphaNum = 256;
		if (g_UpdateDelay > 0 && g_FrameNow >= lastTime)
		{
			DWORD sinceTick = (DWORD)(g_FrameNow - lastTime).count();
			if (sinceTick < (DWORD)g_UpdateDelay)
				g_DrawAlphaNum = (sinceTick << 8) / (DWORD)g_UpdateDelay;
		}

		static MonotonicClock::TimePoint s_lastDrawTime;
		if (g_bFrameChanged || g_FrameNow - s_lastDrawTime >= MonotonicClock::Millis(15))
		{
			s_lastDrawTime = g_FrameNow;
			g_bNewDraw = true;
			
			#ifdef OUTPUT_DEBUG_UPDATE_LOOP
					//DEBUG_ADD("d");//[Update] Before Draw");
			#endif
			

			//------------------------------------------------------
			// 화면 출력
			//------------------------------------------------------
			// 마우스 좌표나 ... frame이 바뀐 경우에 출력..
			if (g_CurrentFrame != oldFrame
				|| g_x != oldX
				|| g_y != oldY || 1)
			{
				__BEGIN_PROFILE("GameUpdateDraw")

				UpdateDraw();

				__END_PROFILE("GameUpdateDraw")

				//
				g_FrameCount++;


				oldX = g_x;
				oldY = g_y;
				oldFrame = g_CurrentFrame;


				#ifdef OUTPUT_DEBUG
					if (g_bSlideScreenShot && g_bSaveSlideScreenShot)
					{
						MakeScreenShot();
					}
				#endif
			}

			

			#ifdef OUTPUT_DEBUG_UPDATE_LOOP
					DEBUG_ADD("dk");//[Update] After Draw");
			#endif
			
			//g_FrameCount++;
		}		
		//------------------------------------------------------
		// 부드럽게 움직이는 커서를 출력할까??
		//------------------------------------------------------
		else if (g_bSmoothCursor)
		{
			///*
			static int surface = 0;
			POINT point;

			POINT cursorPoint;
			GetCursorPos(&cursorPoint);

			ScreenToClient(g_hWnd, &cursorPoint);//by viva
			WindowToGamePoint(cursorPoint);

			// UI에 마우스 좌표 설정
			gC_vs_ui.MouseControl(M_MOVING, cursorPoint.x, cursorPoint.y);


			//------------------------------------------------------
			// 방금 게임 화면이 그려진 상태라면..
			//------------------------------------------------------
			// 다음부터는 Flip해서 마우스만 그리기 위해서..
			// Primary와 Back을 똑같이 만든다.
			//------------------------------------------------------
			if (g_bNewDraw)
			{
				// 다시 false로..
				g_bNewDraw = false;
				
				//------------------------------------------------------
				// PrimarySurface --> BackSurface로 copy
				//------------------------------------------------------
				// 하드웨어 가속이 되는 경우..
				// Primary --> Back으로..
				// 그런데(!) 문제가 좀!! 있다.. 
				// 그래서 두번 Blt해줬다. - -;;
				//------------------------------------------------------
				// (방금 그린 화면과 이전의 화면이 다르기 때문에.. 똑같이 해준다)
//				if (true)
//				{
//					point.x = 0;
//					point.y = 0;
//					RECT rect = { 0, 0, 800, 600 };
//
//					g_pBack->BltPrimarySurface(&point, &rect);
//
//					// 기억했던 커서 부분을 지워준다. --> (0)
//					//g_pCursorSurface->Restore(0, g_pBack);
//					//g_FrameCount++;
//
//					g_pBack->BltPrimarySurface(&point, &rect);
//
//					// 기억했던 커서 부분을 지워준다. --> (0)
//					g_pCursorSurface->Restore(0, g_pBack);
//					
//					// 게임 화면의 Interface부분을 Update시켜줘야 한다.
//					//
//					// [ TEST CODE ]					
//					//gC_vs_ui.VS_UI_Loop();					
//
//					//---------------------------------------
//					// 글자 출력
//					//---------------------------------------
//					//UpdateDrawText();
//				}
//				//------------------------------------------------------
//				// 하드웨어 가속이 안되는 경우..
//				// Last --> Back으로 copy
//				//------------------------------------------------------
//				else
				{
					// Last --> Back으로 copy
					RECT rect;
					point.x = 0;
					point.y = 0;
					rect.left = 0;
					rect.top = 0;
					rect.right = g_GameRect.right;
					rect.bottom = g_GameRect.bottom;
					g_pBack->BltNoColorkey( &point, g_pLast, &rect );

					// 게임 화면의 Interface부분을 Update시켜줘야 한다.
					//
					// [ TEST CODE ]					
					gC_vs_ui.Show();					

					//---------------------------------------
					// 글자 출력
					//---------------------------------------
					//UpdateDrawText();

				}					

				
				MOUSEPOINTER_INFO mp_info;
				gC_vs_ui.GetCurrentMousePointerInfo(mp_info);

				// 커서 출력 위치를 기억시킨다. --> (1)
				point.x = mp_info.x;
				point.y = mp_info.y;
				g_pCursorSurface->Store(1, g_pBack, &point);

				// 다음에 처리할 surface번호
				surface = 0;
			}
			//------------------------------------------------------
			// Flip해서 마우스만 Update하는 상태..
			//------------------------------------------------------
			else
			{	
				// 기억했던 부분을 출력해서 커서를 지워준다. --> (surface)
				g_pCursorSurface->Restore(surface, g_pBack);

				// 마우스 출력 영역 설정
				MOUSEPOINTER_INFO mp_info;
				gC_vs_ui.GetCurrentMousePointerInfo(mp_info);

				// 커서 출력 위치를 기억시킨다. --> (surface)
				point.x = mp_info.x;
				point.y = mp_info.y;
				g_pCursorSurface->Store(surface, g_pBack, &point);
				
				//g_pCursorSurface->Restore(surface, g_pBack);

				// 다음에 처리할 surface번호
				surface = ((surface==0)? 1 : 0);				
			}
			
			//static POINT previousPoint;
			//if (previousPoint.x != cursorPoint.x
			//	|| previousPoint.y != cursorPoint.y)
			//{
			//	gC_vs_ui.VS_UI_Loop();
			//}
			//previousPoint = cursorPoint;
			
			if (!g_pTopView->IsDrawRequest())
			{
				gC_vs_ui.DrawMousePointer();
			}			
			

			// Flip		
			//g_FrameCount++;
			CSDLGraphics::Flip();
		}
	}
//#ifdef OUTPUT_DEBUG
//	static DWORD playTime = 0;
//	if( g_bTestMusic )			// 배경음악 루프중이면
//	{		
//		if( playTime == 0 )
//			playTime = timeGetTime();
//
//		if( g_pMP3->IsStop() )
//		{
//			playTime = timeGetTime();
//			g_CurrentMusicNum ++;
//			if(g_CurrentMusicNum >= 8 )
//				g_CurrentMusicNum = 0;
//
//			g_pMP3->Stop();
//			g_pMP3->Open( g_musicfilename[g_CurrentMusicNum].c_str() );
//			g_pMP3->Play( false );
//
//			g_pSystemMessage->Add( g_musicfilename[g_CurrentMusicNum].c_str() );
//		} else
//		{
//			g_pSystemMessage->Clear();
//			char szbuffer[512];
//			wsprintf(szbuffer,"CTRL+1~8을 누르시면 해당 곡만 반복해서 들을 수 있습니다.CTRL+0 은 다음곡으로 넘어갑니다.");
//			g_pSystemMessage->Add(szbuffer);
//			wsprintf(szbuffer,"%s(%d/8) %2d:%2d", g_musicfilename[g_CurrentMusicNum].c_str(), g_CurrentMusicNum+1,(timeGetTime() - playTime)/1000/60, ((timeGetTime() - playTime )/ 1000)%60 );
//			g_pSystemMessage->Add(szbuffer);
//		}
//	}
//#endif
//
	__END_PROFILE("GameUpdate")

	#ifdef OUTPUT_DEBUG_UPDATE_LOOP
		//DEBUG_ADD("UE0");
	#endif
}
