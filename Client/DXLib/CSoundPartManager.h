//-----------------------------------------------------------------------------
// CSoundPartManager.h
//-----------------------------------------------------------------------------

#ifndef	__CSOUNDPARTMANAGER_H__
#define __CSOUNDPARTMANAGER_H__

#include "CPartManager.h"

/* This project no longer uses real DirectSound - including the real
   <DSound.h> here redefines _DSBPOSITIONNOTIFY (from basic/AudioTypes.h)
   with an incompatible duplicate, matching the same category of conflict
   already fixed for CDirectDraw/MMSystem elsewhere. basic/Platform.h only
   forward-declares LPDIRECTSOUNDBUFFER on non-Windows (it expects the real
   header to supply it on Windows), so declare the opaque pointer type here
   directly for all platforms instead. */
#include "../../basic/Platform.h"

#ifndef LPDIRECTSOUNDBUFFER
typedef struct IDirectSoundBuffer* LPDIRECTSOUNDBUFFER;
#endif

class CSoundPartManager : public CPartManager<WORD, BYTE, LPDIRECTSOUNDBUFFER> {
	public :
		CSoundPartManager()		{}
		// Release() here still dispatches to the derived OnReleaseData -
		// the base destructor's own Release() call no longer would.
		virtual ~CSoundPartManager()	{ Release(); }

		void			Stop();

	protected :
		// Frees one cached sound buffer; the base Release() calls this
		// for every slot, so a re-Init no longer leaks the cache.
		virtual void	OnReleaseData(LPDIRECTSOUNDBUFFER& buffer);

};

#endif
