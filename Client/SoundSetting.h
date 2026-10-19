#ifndef __SOUND_SETTING__
#define __SOUND_SETTING__

// MP3 playback (__USE_MP3__) was never enabled and was retired with task
// 4.215; the music streams as OGG.


#include "COGGSTREAM.h"
// g_pOGG and g_oggfile: COGGSTREAM.h (dxlib's since tasks 4.256 and 4.258).
extern CDirectSoundBuffer*	g_pSoundBufferForOGG;

#endif	// __SOUND_SETTING