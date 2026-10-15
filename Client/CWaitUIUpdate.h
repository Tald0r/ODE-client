//-----------------------------------------------------------------------------
// CWaitUIUpdate.h
//-----------------------------------------------------------------------------
// The loop that waits for the player in the UI: the title, the login and the
// world, server and character choices. What it asks of the program around it
// goes through CWaitUIHost, which GameInit installs (task 4.255).
//-----------------------------------------------------------------------------

#ifndef	__CWAITUIUPDATE_H__
#define	__CWAITUIUPDATE_H__

#include "WinLib/CWinUpdate.h"
#include "InputService.h"
#include "ClientMode.h"

struct CWaitUIHost
{
	void (*UpdateMouse)();				// g_x, g_y from the cursor
	void (*UpdateInput)();				// the input devices' state and events
	bool (*CheckInvalidProcess)();		// the answer is not read
	bool (*UpdateSocketInput)();		// false: nothing more this tick
	bool (*UpdateSocketOutput)();		// false: nothing more this tick
	void (*SetMode)(CLIENT_MODE mode);
	void (*DrawTitleLoading)();			// the title picture, at g_TitleSpriteAlpha
	void (*EndTitleLoading)(bool SendLogin);
};

class CWaitUIUpdate : public CWinUpdate {
	public :
		explicit CWaitUIUpdate(const CWaitUIHost* pHost) : m_pHost(pHost) {}
		~CWaitUIUpdate() {}

		void		Init();

		void		Update();		

	protected :
		void		ProcessInput();
		void		UpdateDraw();
		static void DXMouseEvent(CSDLInput::E_MOUSE_EVENT event, int x, int y, int z);
		static void	DXKeyboardEvent(CSDLInput::E_KEYBOARD_EVENT, DWORD);

		const CWaitUIHost*	m_pHost;
};

extern CWaitUIUpdate*		g_pCWaitUIUpdate;

#endif
