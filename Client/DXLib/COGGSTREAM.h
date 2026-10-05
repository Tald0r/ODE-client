#ifndef __COGGSTREAM_H__
#define __COGGSTREAM_H__

#include <stdio.h>

class CDirectSoundBuffer;

struct tag_position_set {
	int position;
	// Add other fields as needed
};

// Legacy sound-library-style constants kept for call-site compatibility.
// streamLoad()/streamPlay() return bool directly (true = success).
const bool SOUND_ERR_OK = true;
const unsigned long SOUND_PLAY_ONCE = 0;
const unsigned long SOUND_PLAY_REPEAT = 1;

//----------------------------------------------------------------------
// Streams one .ogg music file through the DXLib music backend
// (SDL_mixer). The original streamed into a CDirectSoundBuffer; that
// argument is accepted and ignored so the call sites need no change.
//
// Callers open the file themselves and hand over the FILE*; they also
// fclose() it whenever they like - sometimes *before* streamClose() - so
// the whole file is read into memory at streamLoad() and the FILE* is
// never touched again.
//
// Volume is a DirectSound attenuation in hundredths of a dB
// (DSBVOLUME_MIN..DSBVOLUME_MAX), as at every call site.
//----------------------------------------------------------------------
class COGGSTREAM
{
public:
	COGGSTREAM(void* pDirectSound, CDirectSoundBuffer* pBuffer,
	           unsigned long ulMin, unsigned long ulMax,
	           unsigned long ulFlags, unsigned long ulBufferSize);
	~COGGSTREAM();

	bool streamLoad(FILE* file, tag_position_set* pos);
	void streamClose();
	bool streamPlay(unsigned long ulFlags);
	bool streamUpdate(void* pBuffer);
	long streamVolume(long lVolume);

private:
	CDirectSoundBuffer*	m_pBuffer;		// unused on the SDL backend
	void*				m_pData;		// whole file image; owned until streamClose()
	int					m_DataSize;
	long				m_Volume;		// DSBVOLUME_MIN..DSBVOLUME_MAX
	bool				m_bPlaying;
};

#endif /* __COGGSTREAM_H__ */
