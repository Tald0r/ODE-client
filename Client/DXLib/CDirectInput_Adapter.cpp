/*-----------------------------------------------------------------------------

	CDirectInput_Adapter.cpp

	DirectInput adapter using DXLibBackend.
	This file provides SDL2 backend support for CDirectInput class.

	2025.01.14

-----------------------------------------------------------------------------*/

#include "CDirectInput.h"
#include "DXLibBackend.h"
#include "DXInputHost.h"

#define MSB		0x80

/* Global instance */
CSDLInput*	g_pSDLInput = NULL;

/*=============================================================================
 * SDL Backend Implementation
 *=============================================================================*/

#ifdef DXLIB_BACKEND_SDL

/* Constructor */
CSDLInput::CSDLInput()
{
	m_pDI				= NULL;
	m_pMouse			= NULL;
	m_pKeyboard		= NULL;
	m_mouse_x		= 0;
	m_mouse_y		= 0;
	m_mouse_z		= 0;	
	m_limit_x		= 0;
	m_limit_y		= 0;
	m_mouse_info[0]=0;
	m_mouse_info[1]=0;
	m_mouse_info[2]=0;

	m_fp_mouse_event_receiver = NULL;
	m_fp_keyboard_event_receiver = NULL;
	
	Clear();
}

/* Destructor */
CSDLInput::~CSDLInput()
{
	FreeDirectInput();
}

/* Clear input state */
void CSDLInput::Clear()
{
	m_mouse_z = 0;
	(void)dxlib_input_get_mouse_wheel();
	for (int i=0; i<256; i++)
	{
		m_key[i] = FALSE;
	}
	
	// The down flags mirror the physical buttons, so take them from the
	// backend rather than zeroing them. Zeroing while a button was still
	// held made the next UpdateInput() report a second press for the same
	// click, and Clear() runs on mode changes and re-activation - exactly
	// when a click is often still in progress.
	int left = 0, right = 0, center = 0;
	dxlib_input_get_mouse_buttons(&left, &right, &center);
	m_lb_held = left ? TRUE : FALSE;
	m_rb_held = right ? TRUE : FALSE;
	m_cb_held = center ? TRUE : FALSE;
	m_lb_down = FALSE;
	m_rb_down = FALSE;
	m_cb_down = FALSE;
	m_lb_up = FALSE;
	m_rb_up = FALSE;
	m_cb_up = FALSE;

	// Transitions queued before the clear belong to the old mode; drop them.
	int button, down, x, y;
	while (dxlib_input_pop_mouse_button(&button, &down, &x, &y)) {
	}
}

/* Initialize using SDL backend */
BOOL CSDLInput::Init(HWND hWnd, HINSTANCE hInst, E_EXCLUSIVE ex)
{
	(void)hInst;
	(void)ex;
	// Initialize SDL backend
	if (dxlib_input_init(hWnd) != 0) {
		return FALSE;
	}

	// Mark as initialized (use non-null values as indicators)
	m_pDI = (IDirectInput*)0x01;
	m_pMouse = (IDirectInputDevice*)0x01;
	m_pKeyboard = (IDirectInputDevice*)0x01;

	return TRUE;
}

/* Release SDL backend */
void CSDLInput::FreeDirectInput()
{
	dxlib_input_release();

	m_pDI = NULL;
	m_pMouse = NULL;
	m_pKeyboard = NULL;
}

/* Update input using SDL backend */
void CSDLInput::UpdateInput()
{
	// Update backend
	dxlib_input_update();

	// The button flags are per-frame pulses: "pressed this frame" and
	// "released this frame". The DirectInput original cleared all six at
	// the top of every update, and CGameUpdate::ProcessInput depends on
	// that - it calls MouseControl(M_LEFTBUTTON_DOWN) whenever m_lb_down is
	// set, so a flag that stayed up while the button was held delivered a
	// fresh click on every frame of the press (an equip that ran twice and
	// swapped the old item straight back). Physical state lives in m_*_held.
	m_lb_down = FALSE;
	m_rb_down = FALSE;
	m_cb_down = FALSE;
	m_lb_up = FALSE;
	m_rb_up = FALSE;
	m_cb_up = FALSE;

	// Update keyboard state
	for (int i = 0; i < 256; i++) {
		BOOL down = dxlib_input_key_down(i) ? TRUE : FALSE;
		
		// Check for state changes and trigger events
		if (down && !m_key[i]) {
			m_key[i] = TRUE;
			if (m_fp_keyboard_event_receiver) {
				m_fp_keyboard_event_receiver(KEYDOWN, i);
			}
		} else if (!down && m_key[i]) {
			m_key[i] = FALSE;
			if (m_fp_keyboard_event_receiver) {
				m_fp_keyboard_event_receiver(KEYUP, i);
			}
		}
	}

	dxlib_input_virtual_finish_frame();

	// Wheel: a delta since the last frame is all the game wants.
	m_mouse_z = dxlib_input_get_mouse_wheel();

	if (m_mouse_z != 0) {
		if (m_fp_mouse_event_receiver) {
			int cur_x, cur_y;
			dxlib_input_get_mouse_pos(&cur_x, &cur_y);
			m_fp_mouse_event_receiver(m_mouse_z > 0 ? WHEELUP : WHEELDOWN, cur_x, cur_y, m_mouse_z);
		}
	}

	// Buttons: consume every transition SDL delivered since the last frame,
	// in order and at the coordinates it happened at. The previous code
	// sampled the button state once per frame, which lost any press whose
	// release arrived in the same frame - a quick click on an inventory
	// slot did nothing - and reported a fresh press whenever the flags had
	// been cleared under a held button.
	int button, down, ex, ey;
	while (dxlib_input_pop_mouse_button(&button, &down, &ex, &ey)) {
		BOOL* pHeld = NULL;
		BOOL* pDown = NULL;
		BOOL* pUp = NULL;
		E_MOUSE_EVENT downEvent = MOVE;
		E_MOUSE_EVENT upEvent = MOVE;

		switch (button) {
			case 0:  pHeld = &m_lb_held; pDown = &m_lb_down; pUp = &m_lb_up; downEvent = LEFTDOWN;   upEvent = LEFTUP;   break;
			case 1:  pHeld = &m_rb_held; pDown = &m_rb_down; pUp = &m_rb_up; downEvent = RIGHTDOWN;  upEvent = RIGHTUP;  break;
			case 2:  pHeld = &m_cb_held; pDown = &m_cb_down; pUp = &m_cb_up; downEvent = CENTERDOWN; upEvent = CENTERUP; break;
			default: continue;
		}

		if (down && !*pHeld) {
			*pHeld = TRUE;
			*pDown = TRUE;
			DispatchMouseAt(downEvent, ex, ey);
		} else if (!down && *pHeld) {
			*pHeld = FALSE;
			*pUp = TRUE;
			DispatchMouseAt(upEvent, ex, ey);
		}
	}

	// Safety net: if a transition never reached the queue (it overflowed,
	// or SDL released the button silently on focus loss), the sampled state
	// still wins so a button can neither stick nor go unreported.
	int cur_x, cur_y;
	dxlib_input_get_mouse_pos(&cur_x, &cur_y);

	int left, right, center;
	dxlib_input_get_mouse_buttons(&left, &right, &center);

	if (left && !m_lb_held)        { m_lb_held = TRUE;  m_lb_down = TRUE; DispatchMouseAt(LEFTDOWN, cur_x, cur_y); }
	else if (!left && m_lb_held)   { m_lb_held = FALSE; m_lb_up = TRUE;   DispatchMouseAt(LEFTUP, cur_x, cur_y); }
	if (right && !m_rb_held)       { m_rb_held = TRUE;  m_rb_down = TRUE; DispatchMouseAt(RIGHTDOWN, cur_x, cur_y); }
	else if (!right && m_rb_held)  { m_rb_held = FALSE; m_rb_up = TRUE;   DispatchMouseAt(RIGHTUP, cur_x, cur_y); }
	if (center && !m_cb_held)      { m_cb_held = TRUE;  m_cb_down = TRUE; DispatchMouseAt(CENTERDOWN, cur_x, cur_y); }
	else if (!center && m_cb_held) { m_cb_held = FALSE; m_cb_up = TRUE;   DispatchMouseAt(CENTERUP, cur_x, cur_y); }

	// Finally bring the cursor to where it is now.
	if (cur_x != m_mouse_x || cur_y != m_mouse_y) {
		DispatchMouseAt(MOVE, cur_x, cur_y);
	}
}

/* Move the cursor to (x, y) if needed, then deliver the event there.
 * Publish the same position to the application before its event callback. */
void CSDLInput::DispatchMouseAt(E_MOUSE_EVENT event, int x, int y)
{
	if (x != m_mouse_x || y != m_mouse_y) {
		m_mouse_x = x;
		m_mouse_y = y;
		DXInput::SetMousePosition(x, y);
		if (event != MOVE && m_fp_mouse_event_receiver) {
			m_fp_mouse_event_receiver(MOVE, x, y, m_mouse_z);
		}
	}

	if (m_fp_mouse_event_receiver) {
		m_fp_mouse_event_receiver(event, x, y, m_mouse_z);
	}
}
/* Set acquire (SDL backend - no-op) */
HRESULT CSDLInput::SetAcquire(bool active_app)
{
	(void)active_app;
	if (!m_pMouse || !m_pKeyboard)
		return S_FALSE;
	return S_OK;
}

/* Set mouse position (SDL backend) */
void CSDLInput::SetMousePosition(int x, int y)
{
	m_mouse_x = x;
	m_mouse_y = y;
	dxlib_input_set_mouse_pos(x, y);
}

/* Set mouse speed (SDL backend - stub) */
void CSDLInput::SetMouseSpeed()
{
	// SDL backend uses system mouse settings
}

/* Get mouse acceleration (not applicable for SDL) */
int CSDLInput::GetMouseAcceleration(int value)
{
	return value;
}

/* Set mouse move limit */
void CSDLInput::SetMouseMoveLimit(int x, int y)
{
	(void)dxlib_input_get_mouse_wheel();
	m_mouse_x = 0;
	m_mouse_y = 0;
	m_mouse_z = 0;

	m_limit_x = x;
	m_limit_y = y;
}

/* Set event receivers */
void CSDLInput::SetMouseEventReceiver(void (*fp_receiver)(E_MOUSE_EVENT, int, int, int))
{
	m_fp_mouse_event_receiver = fp_receiver;
}

void CSDLInput::SetKeyboardEventReceiver(void (*fp_receiver)(E_KEYBOARD_EVENT, DWORD))
{
	m_fp_keyboard_event_receiver = fp_receiver;
}

/* Stub implementations for unused methods */
void CSDLInput::OnMouseInput() { /* Handled in UpdateInput */ }
void CSDLInput::OnKeyboardInput() { /* Handled in UpdateInput */ }
HRESULT CSDLInput::InitDI(HWND hWnd, HINSTANCE hInst, E_EXCLUSIVE ex) { 
	return Init(hWnd, hInst, ex) ? S_OK : S_FALSE; 
}

#endif /* DXLIB_BACKEND_SDL */
