//-----------------------------------------------------------------------------
// CGameUpdate.cpp
//-----------------------------------------------------------------------------
// 실제 게임을 진행하는 부분
//-----------------------------------------------------------------------------
#include "Client_PCH.h"
#include "CrtCompat.h"
#include "FrameClock.h"
#include "AmbientSoundState.h"
#include "InputState.h"
#include "WorldEventsHost.h"

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
#include "COGGSTREAM.h"
#include "SystemAvailabilities.h"
#include "MWarManager.h"
// EXECryptor and ThemidaSDK includes removed (SDL2) - Copy protection no longer needed
#include "Packet/Cpackets/CGVerifyTime.h"


//add by viva
#include "Packet/Gpackets/GCFriendChatting.h"


// Global
CGameUpdate*		g_pCGameUpdate = NULL;

bool g_bPreviousMove = false;
	
// g_pOGG, g_oggfile and g_pSoundBufferForOGG are COGGSTREAM.cpp's (dxlib)
// since tasks 4.256, 4.258 and 4.260.

extern bool g_bMouseInPortal;
		
extern POINT g_MouseSector;	

extern bool g_bZoneSafe;

extern bool g_bWatchMode;

int g_MyCheckTime=0;
int g_CheckErrorTime=0;
extern RECT g_GameRect;
int g_CheckTimeNum=0;

extern void	ExecuteLogout();

// g_bFrameChanged is FrameClock.cpp's (gamemodel) since task 4.260.

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
// g_DrawAlphaNum, how far the drawing is between frames, is FrameClock.cpp's.
extern bool	g_bInterpolateDraw;	// MObjectScreen.cpp's

// The camera-interpolation bias passed into the last MTopView::Draw;
// MObjectScreen.cpp holds it and says why picking adds it back.
extern int	g_DrawCamGapX;
extern int	g_DrawCamGapY;

// The game frame is letterbox-scaled into the window by
// spritectl_present_surface, so every Win32 cursor position must be mapped
// from window-client space back to game space before the UI consumes it.
extern "C" void spritectl_window_to_game_coords(int* x, int* y);
static void WindowToGamePoint(POINT& pt)
{
	int mouseX = (int)pt.x;
	int mouseY = (int)pt.y;
	spritectl_window_to_game_coords(&mouseX, &mouseY);
	pt.x = mouseX;
	pt.y = mouseY;
}


bool IsExistCorpseFromPlayer(MCreature* OriginCreature, int creature_type);
bool			HasEffectStatusSummonSylph( MCreature* pCreature );

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
		return;
	}

	static MonotonicClock::TimePoint	last_click_time;
	
	switch (event)
	{
			case CSDLInput::LEFTDOWN :
				// Deliberately not delivered from here. ProcessInput() sends
				// M_LEFTBUTTON_DOWN from the m_lb_down pulse this same frame,
				// as the DirectInput original did (this line was commented
				// out there); sending it here too ran every click twice, so
				// an equip swap undid itself. The wait-UI screens have their
				// own receiver and are unaffected.
				//gC_vs_ui.MouseControl(M_LEFTBUTTON_DOWN, g_x, g_y);
				last_click_time = MonotonicClock::Now();
				
			break;

			case CSDLInput::RIGHTUP :
				// 지뢰 만드는 거 중지
				gC_vs_ui.EndInstallMineProgress();
			break;



		case CSDLInput::WHEELDOWN:
			gC_vs_ui.MouseControl(M_WHEEL_DOWN, g_x, g_y);
			break;

		case CSDLInput::WHEELUP:
			gC_vs_ui.MouseControl(M_WHEEL_UP, g_x, g_y);
			break;

		default:
			break;
	}
}

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
				DEBUG_ADD_FORMAT("CGameUpdate DXKeyboardEvent: Sending WM_KEYDOWN, vk_key=%u (DIK=%lu)", vk_key, (unsigned long)key);
				keydown_debug_count++;
			}
			gC_vs_ui.KeyboardControl(WM_KEYDOWN, vk_key, 0);
		}

		gC_vs_ui.DIKeyboardControl(event, key);

		switch (key)
		{

			case DIK_SPACE :
				if (g_pSDLInput->KeyDown(DIK_LCONTROL) || g_pSDLInput->KeyDown(DIK_RCONTROL) )
				{
				}
			break;


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
					(*g_pUserOption).DrawFPS = !(*g_pUserOption).DrawFPS;
				}

			break;
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
CGameUpdate::ProcessInputRButtonDown(MObject* pObject, bool bForceAttack)
{
	g_bRButtonDown = TRUE;
	
	// 선택된 sector가 없게 한다.
	if (g_pTopView!=NULL)
	{
		g_pTopView->SetSelectedSectorNULL();
	}
	// add by Sonic
#ifdef PLATFORM_WINDOWS
		// Windows-specific anti-cheat error check - Windows only
		if(g_CheckErrorTime>=5)
		{
			g_CheckErrorTime=0;
			m_pHost->SetMode(MODE_QUIT);
			return;
		}
#endif // PLATFORM_WINDOWS
	// end 
	
	if(NULL != pObject)
		bForceAttack |= (g_pObjectSelector->IsWarEnemy( (MCreature*) pObject ) == TRUE);

	if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_DRAGON_EYES))
		return;
	if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
	{
		if(!g_pPlayer->IsStop()) return;
		if(g_pPlayer->GetSpecialActionInfo() == MAGIC_UN_TRANSFORM)
		{
			CGSkillToSelf _CGSkillToSelf;
			_CGSkillToSelf.setSkillType( MAGIC_UN_TRANSFORM );
			g_pSocket->sendPacket( &_CGSkillToSelf );
			return ;
		}
		else if(g_pPlayer->GetSpecialActionInfo() == SKILL_TURRET_FIRE)
			PlaySound( SOUND_SLAYER_TURRET_FIRE_START );
	}
	if( HasEffectStatusSummonSylph( dynamic_cast<MCreature*>(g_pPlayer) )
		) 
	{ 
		if(!g_pPlayer->IsStop()) return;
		
		if(!g_bZoneSafe && !g_pPlayer->IsInSafeSector())
		{
			CGSkillToSelf _CGSkillToSelf;
			_CGSkillToSelf.setSkillType( MAGIC_UN_TRANSFORM );
			g_pSocket->sendPacket( &_CGSkillToSelf );
			g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_SYLPH_SUMMON_GETOFF );
			if (auto* entry = g_pSkillInfoTable->GetMutable(SKILL_SUMMON_SYLPH)) {
				entry->SetAvailableTime( 4000 );
			}
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
	
		if( ! IsExistCorpseFromPlayer(dynamic_cast<MCreature*>(g_pPlayer), 670) )
		{		
			CGGetOffMotorCycle _CGGetOffMotorCycle;
			
			_CGGetOffMotorCycle.setObjectID( 0 );
			
			g_pSocket->sendPacket(&_CGGetOffMotorCycle);
			
			
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

		CGUnburrow _CGUnburrow;

		_CGUnburrow.setX( static_cast<Coord_t>(g_pPlayer->GetX()) );
		_CGUnburrow.setY( static_cast<Coord_t>(g_pPlayer->GetY()) );
		_CGUnburrow.setDir( g_pPlayer->GetDirection() );

		g_pSocket->sendPacket(&_CGUnburrow);


		g_pPlayer->SetWaitVerify( MPlayer::WAIT_VERIFY_SKILL_SUCCESS );
	}
	//---------------------------------------------------------------
	//
	// object/tile에 기술을 사용하는 경우 
	//
	//---------------------------------------------------------------
	else
	{

	
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

}

//---------------------------------------------------------------------------
// Process Game Input
//---------------------------------------------------------------------------
void
CGameUpdate::ProcessInput()
{
	if(g_pEventManager->GetEventByFlag(EVENTFLAG_DENY_INPUT))
		return;

	//---------------------------------------------------	
	// LButton Up
	//---------------------------------------------------	
	if (g_bLButtonDown
		// L이 Up이거나.. R이 Down인 경우
		&& (g_pSDLInput->m_lb_up	|| g_pSDLInput->m_rb_down)
		)
	{

		g_bLButtonDown = FALSE;

		// 주위의 누군가를 계속 공격하는 mode를 해제한다.
		g_pPlayer->UnSetLockMode();
	
		if (g_pPlayer->IsRepeatAction())
		{
			// 버튼을 떼었으므로 행동 반복을 취소한다.
			g_pPlayer->UnSetRepeatAction();

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
		)
	{

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
	m_pHost->GetCursorPoint(cursorPoint);
	WindowToGamePoint(cursorPoint);

	gC_vs_ui.MouseControl(M_MOVING, cursorPoint.x, cursorPoint.y);


	
	bool bRunningParty = UI_IsRunningParty();

	if (g_pSDLInput->m_lb_down)
	{
		
		if (gC_vs_ui.MouseControl(M_LEFTBUTTON_DOWN, g_x, g_y) )
		{

			return;				
		}
	}

	if (g_pSDLInput->m_lb_up)
	{

		if (gC_vs_ui.MouseControl(M_LEFTBUTTON_UP, g_x, g_y))
		{

			return;
		}
	}
	if (g_pSDLInput->m_rb_down)
	{
		if (gC_vs_ui.MouseControl(M_RIGHTBUTTON_DOWN, g_x, g_y)
			&& !bRunningParty)
		{

			return;
		}
	}

	if (g_pSDLInput->m_rb_up)
	{
		if (gC_vs_ui.MouseControl(M_RIGHTBUTTON_UP, g_x, g_y)
			&& !bRunningParty)
		{

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
	

	//---------------------------------------------------	
	// ItemName List 출력?
	//---------------------------------------------------	
	if ((g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU) || g_pSDLInput->KeyDown(DIK_RCONTROL)
		)  
		&& !g_pSDLInput->KeyDown(DIK_LSHIFT)
		&& g_pTradeManager==NULL && g_pPlayer->GetWaitVerify()!=MPlayer::WAIT_VERIFY_TRADE)
	{

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
			}
		}
	}
	else
	{

		g_pTopView->UnSetDrawItemNameList();
		g_pTopView->UnSetRequestMode();		
	}


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
		|| g_pPlayer->IsFastMove()	// 빨리 움직이는 경우
		|| UI_IsRunning_WebBrowser()
		)	
	{

		return;
	}

	// 현재 셀렉트중인 캐릭터에 대한 동작이 있으면 리턴
	if (g_pPlayer->IsRepeatAction())
	{
		g_pTopView->SetSelectedSectorNULL();


		return;
	}


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

		return;
	}
 



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


	MObject*	pObject = NULL;
	
	

	int partyMember = (bRunningParty? gC_vs_ui.GetPartyManagerFocused() : -1) - 1;

	//---------------------------------------------------	
	// 파티UI를 선택하는 경우..
	//---------------------------------------------------	
	if (partyMember >= 0
		&& partyMember < g_pParty->GetSize())
	{

		PARTY_INFO* pInfo = g_pParty->GetMemberInfo( partyMember );

		if (pInfo!=NULL)
		{

			// 우욱.. terrible.. - -;
			// MZone에 Name으로 id검색할 수 있게 map을 추가해야한다.
			// 그리고.. 바로 Name --> MCreature* 를 알 수 있게 해야한다.. 언제? - -;
			pObject = g_pZone->GetCreature( g_pZone->GetCreatureID( pInfo->Name.GetString(), 1 ) );

		}
	}
	//---------------------------------------------------	
	// 존에서 선택..
	//---------------------------------------------------	
	else
	{

		//---------------------------------------------------
		// 아예 딴 UI에 focus 가 있으면 걍 return해야 한다.
		// inventory에서 아이템 사용했는데 기술 나갈 수가 있어서.
		//---------------------------------------------------
		if (partyMember==-2		// party창에 focus 된게 아니고
			&& g_bUIInput)		// UI에 입력이 있다면..
		{

			return;
		}

		//---------------------------------------------------	
		// ItemName을 선택하는 경우면..
		//---------------------------------------------------	
		if (g_pTopView->IsDrawItemNameList())
		{

			MItem* pItem = g_pTopView->SelectItemName(g_x, g_y);


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

				pObject = g_pTopView->GetSelectedObject(g_x, g_y);

			}
		}
		//---------------------------------------------------	
		// 그냥 Object 선택
		//---------------------------------------------------	
		else
		{

			pObject = g_pTopView->GetSelectedObject(g_x, g_y);

		}

	}

	
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


		return;
	}


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
		
		//---------------------------------------------------	
		// Mouse 위치에 Object가 없으면..
		//---------------------------------------------------	
		if (pObject==NULL)
		{
			
			// 선택된 것 없게 한다.
			g_pTopView->SetSelectedNULL();
			
			//---------------------------------------------------	
			// mouse pointer 설정
			//---------------------------------------------------	
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
						
						const MSector& sector = g_pZone->GetSector(static_cast<TYPE_SECTORPOSITION>(g_MouseSector.x), static_cast<TYPE_SECTORPOSITION>(g_MouseSector.y));
						
						//---------------------------------------------------
						// 갈 수 없는 곳이면
						//---------------------------------------------------
						if ((g_pPlayer->IsGroundCreature() && sector.IsBlockGround())
							|| (g_pPlayer->IsUndergroundCreature() && sector.IsBlockUnderground())
							|| (g_pPlayer->IsFlyingCreature() && sector.IsBlockFlying()))
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
			
		}
		//---------------------------------------------------	
		//
		// Mouse 위치에 Object가 있는 경우
		//
		//---------------------------------------------------	
		else
		{
			BYTE objectType = pObject->GetObjectType();
			
			
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

		}


		//---------------------------------------------------	
		//
		// player의 기술 사용 delay 시간이 지나야 공격 가능하다.
		//
		//---------------------------------------------------	
		if (g_pPlayer->IsNotDelay() && !g_pPlayer->HasEffectStatus( EFFECTSTATUS_ETERNITY_PAUSE ) )
		{

			//-----------------------------------------------
			// Shift + L/R ButtonDown : 강제 Tile 공격
			//-----------------------------------------------
			if (g_pSDLInput->KeyDown(DIK_LSHIFT))
			{
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

				}		
				//-----------------------------------------------
				// Shift + RButtonDown : 강제 Tile 기술 공격
				//-----------------------------------------------
				else if (g_pSDLInput->m_rb_down)
				{
					g_pPlayer->UnSetRequestMode();

					ProcessInputRButtonDown(pObject, true);

					g_bPreviousMove = false;

				}
				
			}

			//---------------------------------------------------------------
			// L/R ButtonDown only
			//---------------------------------------------------------------
			else
			{
				
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

					g_pPlayer->UnSetRequestMode();

					g_bLButtonDown = TRUE;
					
					// 선택된 sector가 없게 한다.
					g_pTopView->SetSelectedSectorNULL();

					// sector 선택하기
					g_SelectSector = g_pTopView->GetSelectedSector(g_x, g_y);

					// 인스톨 터렛일때.. 잡다한 lbuttton 처리 않는다.. lbutton = 무조건 터렛 공격
					if(g_pPlayer->HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
					{		
						int TempDir = MTopView::GetDirectionToPosition(g_pPlayer->GetX(), g_pPlayer->GetY(),g_SelectSector.x, g_SelectSector.y);
						
						if(g_pPlayer->GetDirection() == TempDir)
						{
						}
						else
						{
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
						if (!g_pSDLInput->KeyDown(DIK_LCONTROL))
						{		
							if (g_pPlayer->IsNotDelay() && !g_pPlayer->HasEffectStatus(EFFECTSTATUS_ETERNITY_PAUSE ) 
								)
							{
								if (g_pPlayer->SetMovePosition(static_cast<TYPE_SECTORPOSITION>(g_SelectSector.x), static_cast<TYPE_SECTORPOSITION>(g_SelectSector.y)))
								{
									{
										// 다음 목표위치로 설정한다
										g_pPlayer->TraceNULL();							
										
										g_pPlayer->SetNextActionToMove();
										
										g_bPreviousMove = true;
									}
								}
							}
						}

					}
					//--------------------------------------------------
					// 선택된 Object에 대해서 
					//--------------------------------------------------
					else 
					{	

						switch (pObject->GetObjectType())
						{			
							case MObject::TYPE_CREATURE :	
								{
									// 2004, 12, 3, 석민씨 추가
									MCreature* TempCreature = (MCreature*)pObject;
									if(TempCreature->CurPernalShop() == 1 )
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
							break;
						}
					}
				}

				//---------------------------------------------------------------
				//
				//		 RButtonDown
				//
				//---------------------------------------------------------------
				else if (g_pSDLInput->m_rb_down)
				{

					g_pPlayer->UnSetRequestMode();

					ProcessInputRButtonDown(pObject);

					g_bPreviousMove = false;

				}

				//---------------------------------------------------------------
				// L/R Button을 누르지는 않았지만,
				// 이전에 누른 LButton이 계속 눌려진 상태라면..
				// But(!), L-Shift가 눌러지지 않아야 한다.
				//---------------------------------------------------------------
				else if (g_bLButtonDown 				
						&& !g_pSDLInput->KeyDown(DIK_LSHIFT)
						)
				{

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

				}
			}
		}
	}	// lock mode

	//---------------------------------------------------	
	// LButton Up
	//---------------------------------------------------	
	if (g_pSDLInput->m_lb_up && g_bLButtonDown
		)
	{

		g_bLButtonDown = FALSE;
	
		if (g_pPlayer->IsRepeatAction())
		{
			// 버튼을 떼었으므로 행동 반복을 취소한다.
			g_pPlayer->UnSetRepeatAction();

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
		)
	{

		g_bRButtonDown = FALSE;

		if (g_pPlayer->IsRepeatAction())
		{
			// 버튼을 떼었으므로 행동 반복을 취소한다.
			g_pPlayer->UnSetRepeatAction();
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

}

//---------------------------------------------------------------------------
// UpdateGame Draw
//---------------------------------------------------------------------------
void 
CGameUpdate::UpdateDraw()
{
	static bool outputInfo = false;		
	

	POINT	point;
	RECT	rect;
	
		char	str[128];

	// buffer : InitSurface에서 SYSTEMMEMORY로 해주고 써야된다.
	//
	

	//-----------------------------------------------------------------		
	// 마우스 좌표 다시 설정
	//-----------------------------------------------------------------
	m_pHost->GetCursorPoint(point);
	WindowToGamePoint(point);

	// ui에 mouse좌표 설정
	gC_vs_ui.MouseControl(M_MOVING, point.x, point.y);

	//-----------------------------------------------------------------
	// 탑뷰 Zone 출력
	//-----------------------------------------------------------------

	{
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

			const MEvent *event = g_pEventManager->GetEventByFlag(EVENTFLAG_SHAKE_SCREEN);
			if(event != NULL && event->IsShowTime() == true)
			{
				static int count = 0;
				const int maxStep = 13;

				// 2004, 5, 21 sobeit modify start
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
			g_pTopView->Draw(camGapX, camGapY);

			g_bInterpolateDraw = false;

		__END_PROFILE("GameDraw2D")



		//-----------------------------------------------------------------
		// UI 출력
		//-----------------------------------------------------------------		
		__BEGIN_PROFILE("UIDraw2D")

		gC_vs_ui.Show();

		__END_PROFILE("UIDraw2D")



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

			point.x = 0;
			point.y = 0;
			rect.left = 0;
			rect.top = 0;
			rect.right = g_GameRect.right;
			rect.bottom = g_GameRect.bottom;
			g_pBack->BltNoColorkey( &point, g_pLast, &rect );		

		__END_PROFILE("LastToBack")
	}
	

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

			// 30~20
			int blackValue = 20;
			if (second >= 3)
			{
				blackValue = min(31, second+20);
			}

			// 죽는 동작이 끝난 경우에 화면을 검게...
			g_pTopView->SetFadeStart(blackValue, 0, 0, 5,5,5);
		

			// 6 frame씩 보여준다.
			if ((a & 0x01)==0)
			{
				{
				}
				{
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



	__END_PROFILE("DrawDebugInfo")

	//-----------------------------------------------------------------
	// flip
	//-----------------------------------------------------------------
	CSDLGraphics::Flip();
}

//-----------------------------------------------------------------------------
// Update Game
//-----------------------------------------------------------------------------
// 게임 실행 중...
//-----------------------------------------------------------------------------
void 
CGameUpdate::Update(void)
{

	__BEGIN_PROFILE("GameUpdate")

	static MonotonicClock::TimePoint lastTime = g_FrameNow;

	if (g_Mode!=MODE_GAME || g_pClientConfig==NULL)
	{
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


		return;
	}


	//---------------------------------------------
	// Sound관련
	//---------------------------------------------
	// 매 loop 마다 안해줘도 되지 않을까..

	g_SDLAudio.ReleaseTerminatedDuplicateBuffer();

	
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


	m_pHost->UpdateMouse();

	//------------------------------------------
	// 무한루프 예방도 되고...
	// k값마다 한번씩은 Draw를 해주기 때문에
	// Frame Skipping을 적용시킨다.				
	//------------------------------------------

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

			// 게임의 frame수 증가
			g_CurrentFrame++;
		

			//------------------------------------------
			//
			// Socket 입력 처리
			//
			//------------------------------------------
			__BEGIN_PROFILE("GameSocketInput")
			if (!m_pHost->UpdateSocketInput())
			{
				DEBUG_ADD_WAR("[CGameUpdate] UpdateSocketInput Failed");

				__END_PROFILE("GameSocketInput")

				return;
			}
			__END_PROFILE("GameSocketInput")

			if (g_Mode!=MODE_GAME)
			{
				DEBUG_ADD("[CGameUpdate] Not MODE_GAME");
				
				return;
			}


			//------------------------------------------
			//
			// Input 처리
			//
			//------------------------------------------
			if (g_bActiveGame)
			{
				// Input값을 읽어온다.
				m_pHost->UpdateInput();
				
				__BEGIN_PROFILE("ProcessInput")


				ProcessInput();

				__END_PROFILE("ProcessInput")
			}

			__BEGIN_PROFILE("UIProcess")


			gC_vs_ui.Process();


			__END_PROFILE("UIProcess")

			if (!g_bActiveApp || g_Mode!=MODE_GAME)
			{
				DEBUG_ADD("[CGameUpdate] Not bActiveApp");

				return;
			}

			//------------------------------------------
			//
			// Socket 출력 처리
			//
			//------------------------------------------
			__BEGIN_PROFILE("GameSocketOutput")


			if (!m_pHost->UpdateSocketOutput() || g_Mode!=MODE_GAME)
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

				g_pWeather->Action();

			}

			//------------------------------------------
			//
			//	Effect Manager
			//
			//-------------------------------------------
			if (g_pInventoryEffectManager!=NULL)
			{

				g_pInventoryEffectManager->Update();

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
				
			__BEGIN_PROFILE("ZoneUpdate")

			g_pZone->Update();			

			__END_PROFILE("ZoneUpdate")


		
			
			lastTime += MonotonicClock::Millis(g_UpdateDelay);
		
		} while (g_FrameNow - lastTime >= MonotonicClock::Millis(g_UpdateDelay)  
					&& --k);

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
/* add end by sonic */
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
			m_pHost->SetMode(MODE_QUIT);
			return;
		}
#endif // PLATFORM_WINDOWS
		
		static int OnetimeUpdateCount = 0;
		if (k==0)
		{
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

	}
	else
	{
		g_bFrameChanged = false;
	}

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
				WorldEvents::PlayMusicCurrentZone();
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


			}

			

			
		}		
	}
	__END_PROFILE("GameUpdate")

}
