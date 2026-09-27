//-----------------------------------------------------------------------------
// CWaitUIUpdate.h
//-----------------------------------------------------------------------------
// UI에서 메시지가 오기를 기다리는 Loop
//-----------------------------------------------------------------------------

#ifndef	__CWAITUIUPDATE_H__
#define	__CWAITUIUPDATE_H__

#include "WinLib/CWinUpdate.h"
#include "InputService.h"

class CWaitUIUpdate : public CWinUpdate {
	public :
		CWaitUIUpdate() {}
		~CWaitUIUpdate() {}

		void		Init();

		void		Update();		

	protected :
		void		ProcessInput();
		void		UpdateDraw();
		static void DXMouseEvent(CSDLInput::E_MOUSE_EVENT event, int x, int y, int z);
		static void	DXKeyboardEvent(CSDLInput::E_KEYBOARD_EVENT, DWORD);
};

extern CWaitUIUpdate*		g_pCWaitUIUpdate;

#endif
