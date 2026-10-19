//-----------------------------------------------------------------------------
// CGameUpdate.cpp
//-----------------------------------------------------------------------------
// The part that runs the actual game
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
#include "MonotonicClock.h"
#include "PacketDispatcher.h"
#include "UserInformation.h"
#include "PacketDef.h"
#include "VS_UI.h"
#include "VS_UI_mouse_pointer.h"
#include "UIDialog.h"
#include "DebugLog.h"
#include "CGameUpdate.h"
#include "DXLib/DXLibBackend.h"  // For SDL text input functions
#include "MMoneyManager.h"
#include "MGameStringTable.h"
#include "SafeFormat.h"
#include "MObjectSelector.h"
#include "ClientFunction.h"
#include "MMusic.h"
#include "DXLib/CDirectInput.h"
#include "MZoneSoundManager.h"
#include "TempInformation.h"
#include "MFakeCreature.h"
#include "MParty.h"
#include "Profiler.h"
#include "MEventManager.h"
#include "UIFunction.h"
#include "SystemAvailabilities.h"
#include "MWarManager.h"
#include "MPlayer.h"
#include "MZone.h"
#include "MTopView.h"
#include "ZoneFlags.h"
#include "ClientMode.h"
#include "GameMessages.h"
#include "GameSocket.h"
#include "ClientConfig.h"
#include "UserOption.h"
#include "MGameTime.h"
#include "MWeather.h"
#include "MScreenEffectManager.h"
#include "MSkillManager.h"
#include "MInventory.h"
#include "MTradeManager.h"
#include "MActionInfoTable.h"
#include "DXLib/CDirectSound.h"
#include "MZoneTable.h"
#include "MInteractionObject.h"
#include "../basic/CMessageArray.h"
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

	// handle mouse events
	g_pSDLInput->SetMouseEventReceiver( DXMouseEvent );

	// handle keyboard events
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
				// stop installing a mine
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
		if(key == 0xcc) return;		// what is this? -_-;;

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
// may or may not have to do with pObject..
//-----------------------------------------------------------------------------
void
CGameUpdate::ProcessInputRButtonDown(MObject* pObject, bool bForceAttack)
{
	g_bRButtonDown = TRUE;
	
	// clear the selected sector.
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
	// riding a motorcycle
	//
	//---------------------------------------------------------------
	// get off the motorcycle.
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
	// when burrowed, send the packet that says the player comes up out of the ground.
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
	// using a skill on an object or a tile
	//
	//---------------------------------------------------------------
	else
	{

	
		//--------------------------------------------------
		// no object where the player clicked ..
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
			// use the special skill on oneself
			//--------------------------------------------------
			if (g_pPlayer->SelfSpecialAction())
			{
				g_pPlayer->SetRepeatAction();
			}
			//--------------------------------------------------
			// not a special skill used on oneself
			//--------------------------------------------------				
			else if (!g_pPlayer->IsSpecialActionInfoTargetSelf())
			{
				g_SelectSector = g_pTopView->GetSelectedSector(g_x, g_y);

				//--------------------------------------------------				
				// use the special skill on the zone.
				//--------------------------------------------------				
				if (g_pPlayer->TraceSectorToSpecialAction( static_cast<TYPE_SECTORPOSITION>(g_SelectSector.x), static_cast<TYPE_SECTORPOSITION>(g_SelectSector.y) ))
				{
					// mark it as the selected sector.
					g_pTopView->SetSelectedSector( g_SelectSector );
				
					g_pPlayer->SetRepeatAction();

					g_bPreviousMove = false;
				}
			}

			//----------------------------------
			// repeat the action on a tile or on oneself
			//----------------------------------						
			g_pTopView->SetSelectedSectorNULL();
			
		}		
		//--------------------------------------------------
		// on the selected object
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
					// the target creature
					MCreature* pCreature = ((MCreature*)pObject);
					
					//--------------------------------------------------
					// is it used on others?
					//--------------------------------------------------
					if (g_pPlayer->TraceCreatureToSpecialAction( pCreature->GetID(), bForceAttack ))
					{
						g_pPlayer->SetRepeatAction();								

						g_bPreviousMove = false;
					}
					else if (!g_pPlayer->IsSpecialActionInfoTargetOther())
					{
						//--------------------------------------------------
						// (!) when it is not used on another creature
						//--------------------------------------------------
						// use the special skill on oneself
						if (g_pPlayer->SelfSpecialAction())
						{
							g_pPlayer->SetRepeatAction();
						}
						else
						{	
							//--------------------------------------------------
							// when it is not used on oneself
							//--------------------------------------------------
							// at the coordinates of the selected creature ..
							// use the special skill on the zone?
							if (!g_pPlayer->IsSpecialActionInfoTargetSelf()
								&& g_pPlayer->TraceSectorToSpecialAction( pCreature->GetX(), pCreature->GetY()))
							{
								g_pPlayer->SetRepeatAction();

								g_bPreviousMove = false;
							}
						}									
					}
					
					//----------------------------------
					// clear the selected-sector mark
					//----------------------------------
					g_pTopView->SetSelectedSectorNULL();
				}
				break;

				//--------------------------------------------------
				// Item
				//--------------------------------------------------
				// an item: use it on oneself or on the zone.
				case MObject::TYPE_ITEM :		
				{
					//--------------------------------------------------
					// use the special skill on oneself
					//--------------------------------------------------
					if (g_pPlayer->SelfSpecialAction())
					{
						g_pPlayer->SetRepeatAction();
					}
					else
					{	
						//--------------------------------------------------
						// when it is not used on oneself..
						//--------------------------------------------------
						// the target creature
						if (!g_pPlayer->IsSpecialActionInfoTargetSelf())
						{
							MItem* pItem = ((MItem*)pObject);

							if (pItem!=NULL)
							{
								// the exception for Wild Wolf used on a corpse
								if(g_pPlayer->GetSpecialActionInfo() == SKILL_WILD_WOLF)
								{
									g_pPlayer->UseWildWolf_Corpse(pItem);
								}
								// at the coordinates of the selected creature ..
								// use the special skill on the zone.
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
		// L went up.. or R went down
		&& (g_pSDLInput->m_lb_up	|| g_pSDLInput->m_rb_down)
		)
	{

		g_bLButtonDown = FALSE;

		// leave the mode that keeps attacking whoever is around.
		g_pPlayer->UnSetLockMode();
	
		if (g_pPlayer->IsRepeatAction())
		{
			// the button is released, so stop repeating the action.
			g_pPlayer->UnSetRepeatAction();

		}
		else 
		{		
			//---------------------------------------------------	
			// not holding an item
			//---------------------------------------------------	
			// make it the selected sector.
			POINT point;
			g_pPlayer->GetNextDestination( point );		
			if (point.x==SECTORPOSITION_NULL || point.y==SECTORPOSITION_NULL)
			{
				// if the player is not heading anywhere now
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
		// R went up. or L went down
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
				// the button is released, so stop repeating the action.
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
	// show the item name list?
	//---------------------------------------------------	
	if ((g_pSDLInput->KeyDown(DIK_LMENU) || g_pSDLInput->KeyDown(DIK_RMENU) || g_pSDLInput->KeyDown(DIK_RCONTROL)
		)  
		&& !g_pSDLInput->KeyDown(DIK_LSHIFT)
		&& g_pTradeManager==NULL && g_pPlayer->GetWaitVerify()!=MPlayer::WAIT_VERIFY_TRADE)
	{

		g_pTopView->SetDrawItemNameList();

		if (!g_bWatchMode 
			// not repeating some action
			&& !g_pPlayer->IsRepeatAction()
			// not a bat or a wolf
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
	//	when input is restricted..
	//
	//---------------------------------------------------	
	// - a dialog that blocks the game is open
	//
	// - the player is waiting for the server's verification
	//
	// - during a repeated action no other input is needed (ignored)
	//
	// - the UI is taking the input
	//
	//---------------------------------------------------	
	if (g_pPlayer->IsWaitVerify() 
		|| gC_vs_ui.IsInstallMineProgress()	// installing a mine..
		|| g_pPlayer->IsFastMove()	// moving fast
		|| UI_IsRunning_WebBrowser()
		)	
	{

		return;
	}

	// return if there is an action on the currently selected character
	if (g_pPlayer->IsRepeatAction())
	{
		g_pTopView->SetSelectedSectorNULL();


		return;
	}


	//---------------------------------------------------	
	// the UI holds the mouse cursor input
	// the elevator is running..
	//---------------------------------------------------	
	if (((g_pUIDialog->IsLockInput() || g_bUIInput)
		
		// while the party window is open its characters can be selected.
		&& !bRunningParty)

		// so nothing else can be done ...
		|| gC_vs_ui.IsRunningElevator()
		|| gC_vs_ui.IsRunningXmasCardWindow()
		|| UI_IsRunningSelectWayPoint())
	{
		g_pTopView->SetSelectedNULL();
			

		return;
	}

	
	
	//---------------------------------------------------	
	//
	// the player is dead...
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
	// check the object under the mouse now
	//
	//---------------------------------------------------	
	// a forced attack, or in a small zone, or
	// under Hallucination: select everything
	//---------------------------------------------------	
	

	// when a forced attack is set
	bool bForceAttack = (g_pSDLInput->KeyDown(DIK_LSHIFT) 
						|| g_pPlayer->HasEffectStatus(EFFECTSTATUS_HALLUCINATION));
						
						

	if (bForceAttack
		|| g_bZoneSafe
		|| g_pPlayer->IsInSafeSector()
		|| (g_pPlayer->IsSlayer() && g_pPlayer->GetSpecialActionInfo() != ACTIONINFO_NULL && (*g_pActionInfoTable)[g_pPlayer->GetSpecialActionInfo()].GetUser() == FLAG_ACTIONINFO_USER_SLAYER)
		|| (g_pPlayer->IsVampire() && g_pPlayer->GetSpecialActionInfo() != ACTIONINFO_NULL && (*g_pActionInfoTable)[g_pPlayer->GetSpecialActionInfo()].GetUser() == FLAG_ACTIONINFO_USER_VAMPIRE)
		|| (g_pPlayer->IsOusters() && g_pPlayer->GetSpecialActionInfo() != ACTIONINFO_NULL && (*g_pActionInfoTable)[g_pPlayer->GetSpecialActionInfo()].GetUser() == FLAG_ACTIONINFO_USER_OUSTERS)
		// 2004, 11, 26, sobeit add start - the slayers' level-140 enchant skill - a blessing for slayers, a curse for the rest.. heh
		|| g_pPlayer->GetSpecialActionInfo() == SKILL_INTIMATE_GRAIL
		// 2004, 11, 26, sobeit add end
		)
	{
		g_pObjectSelector->SelectAll();				
	}
	//---------------------------------------------------	
	// with L-Control held, select only our side
	//---------------------------------------------------	
	else if (g_pSDLInput->KeyDown(DIK_LCONTROL)
		|| g_pTopView->IsRequestMode())
	{
		g_pObjectSelector->SelectFriend();		
	}
	//---------------------------------------------------	
	// otherwise.. select only enemies
	//---------------------------------------------------	
	else
	{
		// check the notice event ...
		// if a war for a castle is on!!
		// 1. the defending guild: everyone outside one's own guild is an enemy!
		// 2. a guild that applied to attack: everyone outside our guild is an enemy!
		// 3. anyone else, outside the guild war: everyone but the two guilds' members is on one team!
		
		g_pObjectSelector->SelectEnemy();		
	}

	//---------------------------------------------------	
	// besides this..
	//
	// whether g_pObjectSelector->SelectByRace()
	// or g_pObjectSelector->SelectByGuild() has to be chosen.
	//
	// an icon always on screen would be good.. Race/Guild
	//
	// also, depending on the attack mode, a Peace/Attack/Normal icon
	// is needed too.
	//---------------------------------------------------	
	// a slayer selects by race
	//---------------------------------------------------	
	if (g_pPlayer->IsSlayer() || g_pPlayer->IsOusters() )
	{
		g_pObjectSelector->SelectByRace();
	}
	//---------------------------------------------------	
	// a vampire selects by guild
	//---------------------------------------------------	
	else
	{
		g_pObjectSelector->SelectByGuild();
	}


	MObject*	pObject = NULL;
	
	

	int partyMember = (bRunningParty? gC_vs_ui.GetPartyManagerFocused() : -1) - 1;

	//---------------------------------------------------	
	// selecting in the party UI..
	//---------------------------------------------------	
	if (partyMember >= 0
		&& partyMember < g_pParty->GetSize())
	{

		PARTY_INFO* pInfo = g_pParty->GetMemberInfo( partyMember );

		if (pInfo!=NULL)
		{

			// ugh.. terrible.. - -;
			// MZone needs a map to look an id up by name.
			// and.. it should give Name --> MCreature* directly.. when? - -;
			pObject = g_pZone->GetCreature( g_pZone->GetCreatureID( pInfo->Name.GetString(), 1 ) );

		}
	}
	//---------------------------------------------------	
	// selecting in the zone..
	//---------------------------------------------------	
	else
	{

		//---------------------------------------------------
		// if another UI has the focus altogether, just return,
		// because using an item in the inventory could fire a skill.
		//---------------------------------------------------
		if (partyMember==-2		// the party window has no focus
			&& g_bUIInput)		// and the UI has input..
		{

			return;
		}

		//---------------------------------------------------	
		// when picking an item name..
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
			// no item name picked: pick an object
			//---------------------------------------------------	
			if (pObject==NULL)
			{

				pObject = g_pTopView->GetSelectedObject(g_x, g_y);

			}
		}
		//---------------------------------------------------	
		// just pick an object
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
	// check the lock mode
	//---------------------------------------------------	
	if (g_pSDLInput->KeyDown(DIK_CAPITAL) 
		&& !g_pPlayer->IsInCasket()
		&& !g_pPlayer->IsUndergroundCreature())
	{
		g_pPlayer->SetLockMode();

		// always switch to the attack cursor.
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

		// in lock mode clear the selected sector.
		g_pTopView->SetSelectedSectorNULL();
	}
	else
	{
		// with nothing pressed,
		// lock mode, if on, must be released.
		if (!g_bLButtonDown && !g_bRButtonDown
			)
		{
			g_pPlayer->UnSetLockMode();
		}
	}

	//---------------------------------------------------	
	// only outside lock mode
	// take the ordinary input and act on it.
	//---------------------------------------------------	
	if (!g_pPlayer->IsLockMode())
	{	
		
		//---------------------------------------------------	
		// no object under the mouse..
		//---------------------------------------------------	
		if (pObject==NULL)
		{
			
			// select nothing.
			g_pTopView->SetSelectedNULL();
			
			//---------------------------------------------------	
			// set the mouse pointer
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
						// a place the player cannot go
						//---------------------------------------------------
						if ((g_pPlayer->IsGroundCreature() && sector.IsBlockGround())
							|| (g_pPlayer->IsUndergroundCreature() && sector.IsBlockUnderground())
							|| (g_pPlayer->IsFlyingCreature() && sector.IsBlockFlying()))
						{	
							// not a portal
							if (!g_bMouseInPortal
								// the UI has no input --> the cursor is over the game
								&& !g_bUIInput
								// no character selected
								&& g_pTopView->GetSelectedCreature()==OBJECTID_NULL)
							{
								// show the cannot-move cursor
								gpC_mouse_pointer->SetCursorNotMove();					
							}
						}
						//---------------------------------------------------
						// a place the player can go..
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
		// an object under the mouse
		//
		//---------------------------------------------------	
		else
		{
			BYTE objectType = pObject->GetObjectType();
			
			
			//------------------------------------------------
			// the mouse over a creature
			//------------------------------------------------
			if (objectType==MObject::TYPE_CREATURE)
			{
				MCreature* pCreature = (MCreature*)pObject;

				g_pTopView->SetSelectedCreature( pCreature->GetID() );
					
				if (!g_bMouseInPortal 
						
					// attackable by race
					// or a forced attack..
					&& g_pObjectSelector->CanAttack( pCreature ) )
					// or, for a vampire, attackable by guild..
						
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
			// the mouse over an item
			//------------------------------------------------
			else if (pObject->GetObjectType()==MObject::TYPE_ITEM)
			{
				MItem* pItem = (MItem*)pObject;

				if(g_pZone != NULL && pItem != NULL )
				{
					// not for a slayer
					// for a vampire, find the creature, and only for a relic

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
						// the colour by option
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
						// add by Sonic 2006.10.28: show gear with three options in red
						else if(pItem->GetItemOptionListCount() > 2)
						{
							color = g_pClientConfig->COLOR_NAME_VAMPIRE; //Red
						}
						// end by Sonic 2006.10.28: show gear with three options in red
						else
						{
							color = g_pClientConfig->COLOR_NAME_ITEM_OPTION;
						}
						
						//------------------------------------------------
						// the pick-up cursor
						//------------------------------------------------
						if (!g_bMouseInPortal&&!g_pPlayer->IsInDarkness())
						{						
							// a corpse is not labelled 'corpse'.
							if (pItem->GetItemClass()==ITEM_CLASS_CORPSE)
							{
								gpC_mouse_pointer->SetCursorPickUp( "", color );
							}
							// an ordinary item shows its name.
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
			// the mouse over an effect
			//------------------------------------------------
			else if (pObject->GetObjectType()==MObject::TYPE_EFFECT)
			{
				g_pTopView->SetSelectedEffect( pObject->GetID() );
			}

		}


		//---------------------------------------------------	
		//
		// the player can attack only once the skill delay has passed.
		//
		//---------------------------------------------------	
		if (g_pPlayer->IsNotDelay() && !g_pPlayer->HasEffectStatus( EFFECTSTATUS_ETERNITY_PAUSE ) )
		{

			//-----------------------------------------------
			// Shift + L/R ButtonDown : forced tile attack
			//-----------------------------------------------
			if (g_pSDLInput->KeyDown(DIK_LSHIFT))
			{
				//-----------------------------------------------
				// Shift + LButtonDown : forced attack
				//-----------------------------------------------
				if (g_pSDLInput->m_lb_down
					// when not hiding
					&& !g_pPlayer->IsInCasket()
					&& !g_pPlayer->IsUndergroundCreature()
					&& !g_pPlayer->HasEffectStatus(EFFECTSTATUS_INSTALL_TURRET))
				{
					g_pPlayer->UnSetRequestMode();

					//-----------------------------------------------
					// force-attack the character..
					//-----------------------------------------------
					if (pObject!=NULL)
					{
						if (pObject->GetObjectType()==MObject::TYPE_CREATURE)
						{
							// when some object is selected,
							// holding LButton alone must not move the player.
							MCreature *pCreature = dynamic_cast<MCreature*>(pObject);
							g_bLButtonDown = TRUE;
							
							if (g_pPlayer->TraceCreatureToBasicAction( 
										pCreature->GetID(), 
										true))		// forced attack
							{
								//----------------------------------
								// repeat the basic action on another
								//----------------------------------
								g_pPlayer->SetRepeatAction();
								g_bPreviousMove = false;
							}
							// clear the destination mark.
							g_pTopView->SetSelectedSectorNULL();
						}
					}

				}		
				//-----------------------------------------------
				// Shift + RButtonDown : forced tile skill attack
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
					// must not be burrowed.
					&& !g_pPlayer->IsInCasket()
					&& !g_pPlayer->IsUndergroundCreature()
					&& g_pPlayer->CurPernalShop() != 2
					&& g_pPlayer->CurPernalShop() != 1
					&& gC_vs_ui.IsRunningPersnalShop() == false
					)
				{		

					g_pPlayer->UnSetRequestMode();

					g_bLButtonDown = TRUE;
					
					// clear the selected sector.
					g_pTopView->SetSelectedSectorNULL();

					// pick the sector
					g_SelectSector = g_pTopView->GetSelectedSector(g_x, g_y);

					// with Install Turret.. no other lbutton handling.. lbutton = always the turret's attack
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
					// no object where the player clicked: MOVE
					//--------------------------------------------------
					else if (pObject == NULL)
					{		
						// move only while neither l-shift nor l-control is held.
						if (!g_pSDLInput->KeyDown(DIK_LCONTROL))
						{		
							if (g_pPlayer->IsNotDelay() && !g_pPlayer->HasEffectStatus(EFFECTSTATUS_ETERNITY_PAUSE ) 
								)
							{
								if (g_pPlayer->SetMovePosition(static_cast<TYPE_SECTORPOSITION>(g_SelectSector.x), static_cast<TYPE_SECTORPOSITION>(g_SelectSector.y)))
								{
									{
										// set it as the next destination
										g_pPlayer->TraceNULL();							
										
										g_pPlayer->SetNextActionToMove();
										
										g_bPreviousMove = true;
									}
								}
							}
						}

					}
					//--------------------------------------------------
					// on the selected object
					//--------------------------------------------------
					else 
					{	

						switch (pObject->GetObjectType())
						{			
							case MObject::TYPE_CREATURE :	
								{
									// 2004, 12, 3, added by Seokmin
									MCreature* TempCreature = (MCreature*)pObject;
									if(TempCreature->CurPernalShop() == 1 )
									{
										gpC_base->SendMessage(UI_REQUEST_STORE_INFO, pObject->GetID(),0);
										gC_vs_ui.SetOtherObjectID(pObject->GetID());
									}
									// 2004, 12, 3, added by Seokmin

									if (g_pPlayer->TraceCreatureToBasicAction( pObject->GetID(), 
																				bForceAttack, true ))
									{
										//----------------------------------
										// repeat the basic action on another
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
				// no L/R button pressed,
				// but the LButton pressed before is still held..
				// But(!), L-Shift must not be held.
				//---------------------------------------------------------------
				else if (g_bLButtonDown 				
						&& !g_pSDLInput->KeyDown(DIK_LSHIFT)
						)
				{

					//--------------------------------------------------
					// no object where the player clicked,
					// and the player was moving just now: MOVE
					//--------------------------------------------------
					if (pObject == NULL && g_bPreviousMove)
					{	
						if (g_pPlayer->IsNotDelay() && !g_pPlayer->IsRepeatAction())
						{
							g_SelectSector = g_pTopView->GetSelectedSector(g_x, g_y);

							// set it as the next destination
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
			// the button is released, so stop repeating the action.
			g_pPlayer->UnSetRepeatAction();

			g_bPreviousMove = false;
		}
		else
		{	
			// make it the selected sector.
			POINT point;
			
			// if there is no next destination
			g_pPlayer->GetNextDestination( point );		
			if (point.x==SECTORPOSITION_NULL || point.y==SECTORPOSITION_NULL)
			{
				// if the player is not heading anywhere now
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
			// the button is released, so stop repeating the action.
			g_pPlayer->UnSetRepeatAction();
		}
	}

	if(g_pSDLInput->m_cb_up && g_bCButtonDown)
	{
		g_bCButtonDown = FALSE;
	}

	// standing still: clear the destination.
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
	bool& outputInfo = m_bOutputInfo;
	

	POINT	point;
	RECT	rect;
	
		char	str[128];

	// buffer : InitSurface must make it SYSTEMMEMORY before it is used.
	//
	

	//-----------------------------------------------------------------		
	// set the mouse coordinates again
	//-----------------------------------------------------------------
	m_pHost->GetCursorPoint(point);
	WindowToGamePoint(point);

	// give the UI the mouse coordinates
	gC_vs_ui.MouseControl(M_MOVING, point.x, point.y);

	//-----------------------------------------------------------------
	// draw the zone in the top view
	//-----------------------------------------------------------------

	{
		//-----------------------------------------------------------------
		// draw the game screen
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
				int& count = m_ShakeStep;
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
		// draw the UI
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
				// print the FPS
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
		// copy Last to Back - only without 3D HAL..
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
	// dead..
	//-----------------------------------------------------------------
	if (g_pPlayer->IsDead())
	{
		if (g_pPlayer->GetActionCount()==g_pPlayer->GetActionCountMax())		
		{	
			BYTE& a = m_DeadStep;
			BYTE& count = m_DeadStepFrames;

			int second = g_pPlayer->GetDeadDelayLast();

			// 30~20
			int blackValue = 20;
			if (second >= 3)
			{
				blackValue = min(31, second+20);
			}

			// once the dying action has finished, darken the screen...
			g_pTopView->SetFadeStart(blackValue, 0, 0, 5,5,5);
		

			// show it 6 frames at a time.
			if ((a & 0x01)==0)
			{
				{
				}
				{
				}
				
				// after 6 seconds, show the resurrect button.
				if (second < 4
					&& g_pTempInformation->GetMode()==TempInformation::MODE_NULL)
				{
					// FixMe Please oh shit by ssuk
					if( (g_pWarManager->IsExist( g_pZone->GetID() ) || g_pZone->GetID() == 71 || g_pZone->GetID() == 72 || g_pZone->GetID() == 73) && gC_vs_ui.IsRunningBloodBibleStatus() )
						gC_vs_ui.RequestShrineMinimap( g_pClientConfig->DELAY_PLAYER_RESURRECT_TIMEOUT );

					gC_vs_ui.RequestDie( g_pClientConfig->DELAY_PLAYER_RESURRECT_TIMEOUT );

					g_pTempInformation->SetMode(TempInformation::MODE_WAIT_RESURRECT);
					bool bResurrect = false, bElixir= false, bEternity= false, IsSiegeAttacker= false ;

					// 2005, 1, 18, sobeit add start - a death by a quest action skips the checks below.
					if(0 == g_pPlayer->GetResurrectZoneID())
					// 2005, 1, 18, sobeit add end
					{
						if( g_pInventory->FindItem( ITEM_CLASS_RESURRECT_ITEM, 0 ) != NULL )
							bResurrect = true;

						if(g_pInventory->FindItem( ITEM_CLASS_RESURRECT_ITEM, 1 ) != NULL )
							bElixir = true;

						bEternity = g_pSkillAvailable->IsEnableSkill( SKILL_ETERNITY ) &&
									(*g_pSkillInfoTable)[SKILL_ETERNITY].IsAvailableTime() 
									// 2004, 11, 11, sobeit add start - check the level too.. this one.. what is g_char_slot_ingame.DOMAIN_HEAL? -_-; bug fix.. -_-;
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

			if (!m_DeadStepTime) m_DeadStepTime = g_FrameNow;
			MonotonicClock::TimePoint& lastTime = *m_DeadStepTime;
			if (g_FrameNow - lastTime >= MonotonicClock::Millis(g_UpdateDelay))
			{
				if (++count==6) { count = 0; a++; }
				lastTime = g_FrameNow;
			}			
		}
	}

	//-----------------------------------------------------------------
	// draw the mouse
	//-----------------------------------------------------------------
	if (!g_pTopView->IsDrawRequest())
	{
		gC_vs_ui.DrawMousePointer();
	}

	//-----------------------------------------------------------------
	// print the debug information
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
// the game running...
//-----------------------------------------------------------------------------
void 
CGameUpdate::Update(void)
{

	__BEGIN_PROFILE("GameUpdate")

	if (!m_LastTickTime) m_LastTickTime = g_FrameNow;
	MonotonicClock::TimePoint& lastTime = *m_LastTickTime;

	if (g_Mode!=MODE_GAME || g_pClientConfig==NULL)
	{
		return;
	}
	
	//------------------------------------------
	// check whether it is time to log out.
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
	// sound
	//---------------------------------------------
	// does this really need doing every loop..

	g_SDLAudio.ReleaseTerminatedDuplicateBuffer();

	
	if (!m_NextSoundCheckTime) m_NextSoundCheckTime = g_FrameNow;
	MonotonicClock::TimePoint& nextSoundCheckTime = *m_NextSoundCheckTime;

	if (g_FrameNow > nextSoundCheckTime)
	{
		// sounds played this second
		g_SoundPerSecond = 0;

		// one second on
		nextSoundCheckTime = g_FrameNow + MonotonicClock::Millis(1000);
	}

	//---------------------------------------------------
	//
	// process once per fixed interval.
	//
	//---------------------------------------------------
	// [example]
	// g_UpdateDelay	: 100
	// lastTime			: 1000 
	// currentTime		: 1240 
	//
	// processes two ticks: 1200 and 1100.
	//---------------------------------------------------

	int k = g_pClientConfig->MAX_UPDATE_ONETIME;		// 12 frame * 1 Tile.. 


	m_pHost->UpdateMouse();

	//------------------------------------------
	// this also prevents an endless loop...
	// and since it draws at least once every k ticks,
	// it applies frame skipping.
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
		// 2006.11.07  removed the check for a second running client (Coffee)
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
		// mark that something changed
		g_bFrameChanged = true;

		do
		{

			// advance the game's frame count
			g_CurrentFrame++;
		

			//------------------------------------------
			//
			// socket input
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
			// input
			//
			//------------------------------------------
			if (g_bActiveGame)
			{
				// read the input.
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
			// socket output
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
			//	weather
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
			// update the zone's objects.
			//
			//------------------------------------------
				
			__BEGIN_PROFILE("ZoneUpdate")

			g_pZone->Update();			

			__END_PROFILE("ZoneUpdate")


		
			
			lastTime += MonotonicClock::Millis(g_UpdateDelay);
		
		} while (g_FrameNow - lastTime >= MonotonicClock::Millis(g_UpdateDelay)  
					&& --k);

/* add by sonic 2006.9.12 */
// added a time check
		//------------------------------------------------------------------
		// send a garbage packet once a minute.
		//------------------------------------------------------------------
#ifdef PLATFORM_WINDOWS
		// Windows-specific anti-cheat time verification - Windows only
		DWORD nextTimeValue = 60000;
		if (!m_NextVerifyTime) m_NextVerifyTime = MonotonicClock::Now() + MonotonicClock::Millis(60000);
		MonotonicClock::TimePoint& nextTime = *m_NextVerifyTime;

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
		if (!m_LastCheckTime) m_LastCheckTime = g_FrameNow;
		MonotonicClock::TimePoint& lastCheckTime = *m_LastCheckTime;
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
		// if after k updates..
		// there is still more to update.. ignore it..
		// ignore it??
		// like Diablo.. how about suddenly running? -_-;
		// detect cheat programs

#ifdef PLATFORM_WINDOWS
		// Windows-specific anti-cheat error check - Windows only
		if(g_CheckErrorTime>=5)
		{
			g_CheckErrorTime=0;
			m_pHost->SetMode(MODE_QUIT);
			return;
		}
#endif // PLATFORM_WINDOWS
		
		int& OnetimeUpdateCount = m_OnetimeUpdateCount;
		if (k==0)
		{
			OnetimeUpdateCount++;
	
			if (OnetimeUpdateCount > g_pClientConfig->MAX_UPDATE_ONETIME_COUNT)
			{
				// allow no more updates.
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
		if (!m_LastHour) m_LastHour = g_pGameTime->GetHour()+1;
		int& lastHour = *m_LastHour;

		int currentHour = g_pGameTime->GetHour();

		if (lastHour != currentHour)
		{
			//---------------------------------------------
			// a new hour changes the shadows too.
			//---------------------------------------------
			if (g_pTopView!=NULL)
			{
				g_pTopView->ClearShadowManager();
			}

			lastHour = g_pGameTime->GetHour();

			//---------------------------------------------
			// change the music too.
			//---------------------------------------------
			// play music only on even hours..
			if ((currentHour & 0x01)==0)
			{
				WorldEvents::PlayMusicCurrentZone();
			}

		}

		
		//---------------------------------------------
		// the time
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
		// the time
		//---------------------------------------------
		SafeFormat::Format(str, GetGameString(STRING_DRAW_GAME_DATE),
					g_pGameTime->GetYear(),
					g_pGameTime->GetMonth(),
					g_pGameTime->GetDay()
				);

		gC_vs_ui.SetDate( str );
	}
	
	
	//---------------------------------------------
	// play the zone's ambient sounds
	//---------------------------------------------
	if (g_pZoneSoundManager!=NULL)
	{
		g_pZoneSoundManager->UpdateSound();
	}

	//---------------------------------------------
	// give the UI the player's coordinates
	//---------------------------------------------
	gC_vs_ui.SetXY( g_pPlayer->GetX(), g_pPlayer->GetY() );

	//---------------------------------------------
	//
	// Draw - draw when something changed.
	//
	//---------------------------------------------
	{
		int& oldX = m_OldX;
		int& oldY = m_OldY;
		DWORD& oldFrame = m_OldFrame;

		//------------------------------------------------------
		// if it updated..
		// draw the new game screen.
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

		MonotonicClock::TimePoint& lastDrawTime = m_LastDrawTime;
		if (g_bFrameChanged || g_FrameNow - lastDrawTime >= MonotonicClock::Millis(15))
		{
			lastDrawTime = g_FrameNow;

			//------------------------------------------------------
			// draw the screen
			//------------------------------------------------------
			// draw when the mouse coordinates ... or the frame changed..
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
