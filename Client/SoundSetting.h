#ifndef __SOUND_SETTING__
#define __SOUND_SETTING__

// MP3 playback (__USE_MP3__) was never enabled and was retired with task
// 4.215; the music streams as OGG.


#include "COGGSTREAM.h"
// g_pOGG: COGGSTREAM.h (dxlib's since task 4.256).
extern CDirectSoundBuffer*	g_pSoundBufferForOGG;
extern FILE	*				g_oggfile;

#endif	// __SOUND_SETTING