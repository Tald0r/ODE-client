//-----------------------------------------------------------------------------
// CWinUpdate.h
//-----------------------------------------------------------------------------
// The base of the per-mode update classes.
//-----------------------------------------------------------------------------

#ifndef	__CWINUPDATE_H__
#define	__CWINUPDATE_H__

// Platform.h, not <MMSystem.h>: the real header conflicts with the SDL
// stand-ins in basic/AudioTypes.h and with the timeGetTime() macro it defines.
#include "../../basic/Platform.h"

class CWinUpdate {
	public :
		CWinUpdate();
		virtual ~CWinUpdate();

		//-------------------------------------------------------
		// Init
		//-------------------------------------------------------
		virtual void	Init() {}

		//-------------------------------------------------------
		// Update
		//-------------------------------------------------------
		virtual void	Update();
};

// The update the main loop runs now (one of the waits or the game).
extern CWinUpdate*	g_pUpdate;

#endif
