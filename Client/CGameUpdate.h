//-----------------------------------------------------------------------------
// CGameUpdate.h
//-----------------------------------------------------------------------------
// Game Loop: the in-game frame - input, update and draw. What it asks of the
// program around it goes through CGameUpdateHost, which GameInit installs.
//-----------------------------------------------------------------------------

#ifndef	__CGAMEUPDATE_H__
#define	__CGAMEUPDATE_H__

#include "WinLib/CWinUpdate.h"
#include "DXLib/CDirectInput.h"
#include "ClientMode.h"
#include "MonotonicClock.h"
#include <optional>

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

		// What the frame loop remembers from one call to the next. The game
		// makes one CGameUpdate, so these behave as the function statics
		// they were; an empty optional is one the first call has not yet
		// set, from the clock or the game time as the static was.
		// UpdateDraw: the FPS text without the option (never set), the
		// screen-shake step, and the dead player's fade steps.
		bool									m_bOutputInfo = false;
		int										m_ShakeStep = 0;
		BYTE									m_DeadStep = 0;
		BYTE									m_DeadStepFrames = 0;
		std::optional<MonotonicClock::TimePoint>	m_DeadStepTime;
		// Update: the time of the last tick, the next reset of the sound
		// count, Windows' speed check, the catch-up bursts, the game hour,
		// and what the last draw saw.
		std::optional<MonotonicClock::TimePoint>	m_LastTickTime;
		std::optional<MonotonicClock::TimePoint>	m_NextSoundCheckTime;
		std::optional<MonotonicClock::TimePoint>	m_NextVerifyTime;
		std::optional<MonotonicClock::TimePoint>	m_LastCheckTime;
		int										m_OnetimeUpdateCount = 0;
		std::optional<int>						m_LastHour;
		int										m_OldX = -1;
		int										m_OldY = -1;
		DWORD									m_OldFrame = 0;
		MonotonicClock::TimePoint				m_LastDrawTime{};
};

extern CGameUpdate*		g_pCGameUpdate;

#endif
