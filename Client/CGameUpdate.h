//-----------------------------------------------------------------------------
// CGameUpdate.h
//-----------------------------------------------------------------------------
// Game Loop: the in-game frame - input, update and draw. What it asks of the
// program around it goes through CGameUpdateHost, which GameInit installs.
//-----------------------------------------------------------------------------

#ifndef	__CGAMEUPDATE_H__
#define	__CGAMEUPDATE_H__

#include "WinLib/CWinUpdate.h"
#include "InputService.h"
#include "ClientMode.h"

class MObject;

struct CGameUpdateHost
{
	void (*UpdateMouse)();				// g_x, g_y from the cursor
	void (*UpdateInput)();				// the input devices' state and events
	bool (*UpdateSocketInput)();		// false: nothing more this tick
	bool (*UpdateSocketOutput)();		// false: nothing more this tick
	void (*SetMode)(CLIENT_MODE mode);	// Windows' speed check quits through it
	void (*GetCursorPoint)(POINT& pt);	// the cursor in the window's client area
};

class CGameUpdate : public CWinUpdate {
	public :
		explicit CGameUpdate(const CGameUpdateHost* pHost) : m_pHost(pHost) {}
		~CGameUpdate() {}

		void		Init();

		void		Update();	

		void		UpdateDraw();

	protected :
		void		ProcessInput();
		void		ProcessInputRButtonDown(MObject* pObject, bool bForceAttack = false);

		static void	DXMouseEvent(CSDLInput::E_MOUSE_EVENT event, int x, int y, int z);
		static void	DXKeyboardEvent(CSDLInput::E_KEYBOARD_EVENT, DWORD);

		const CGameUpdateHost*	m_pHost;
};

extern CGameUpdate*		g_pCGameUpdate;

#endif
