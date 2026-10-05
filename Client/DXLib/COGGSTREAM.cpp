#include "COGGSTREAM.h"
#include "DXLibBackend.h"
#include "AudioTypes.h"

#include <stdlib.h>

COGGSTREAM::COGGSTREAM(void* pDirectSound, CDirectSoundBuffer* pBuffer,
                       unsigned long ulMin, unsigned long ulMax,
                       unsigned long ulFlags, unsigned long ulBufferSize)
{
	m_pBuffer = pBuffer;
	m_pData = NULL;
	m_DataSize = 0;
	m_Volume = DSBVOLUME_MAX;
	m_bPlaying = false;
	(void)pDirectSound; (void)ulMin; (void)ulMax; (void)ulFlags; (void)ulBufferSize;
}

COGGSTREAM::~COGGSTREAM()
{
	streamClose();
}

bool COGGSTREAM::streamLoad(FILE* file, tag_position_set* pos)
{
	(void)pos;

	streamClose();

	if (file == NULL)
		return false;

	// Idempotent; opens the mixer if InitSound() has not already.
	if (dxlib_music_init(NULL) != 0)
		return false;

	// Read from the current position to the end. The backend decodes
	// straight out of this buffer for as long as the music is loaded.
	long start = ftell(file);
	if (start < 0)
		start = 0;
	if (fseek(file, 0, SEEK_END) != 0)
		return false;
	long end = ftell(file);
	fseek(file, start, SEEK_SET);
	if (end <= start)
		return false;

	size_t size = (size_t)(end - start);
	m_pData = malloc(size);
	if (m_pData == NULL)
		return false;

	if (fread(m_pData, 1, size, file) != size)
	{
		free(m_pData);
		m_pData = NULL;
		return false;
	}
	m_DataSize = (int)size;

	if (dxlib_music_load_mem(m_pData, m_DataSize) != 0)
	{
		free(m_pData);
		m_pData = NULL;
		m_DataSize = 0;
		return false;
	}

	dxlib_music_set_volume(AudioVolumeToPercent(m_Volume));
	return true;
}

void COGGSTREAM::streamClose()
{
	if (m_pData != NULL)
	{
		// Stop and unload before the buffer the decoder reads from goes.
		dxlib_music_stop();
		dxlib_music_free();
		free(m_pData);
		m_pData = NULL;
		m_DataSize = 0;
	}
	m_bPlaying = false;
}

bool COGGSTREAM::streamPlay(unsigned long ulFlags)
{
	if (m_pData == NULL)
		return false;

	if (dxlib_music_play(ulFlags == SOUND_PLAY_REPEAT ? 1 : 0) != 0)
		return false;

	dxlib_music_set_volume(AudioVolumeToPercent(m_Volume));
	m_bPlaying = true;
	return true;
}

// Polled from WM_TIMER; the backend streams on its own, this only tracks
// whether a once-only track has run out.
bool COGGSTREAM::streamUpdate(void* pBuffer)
{
	(void)pBuffer;

	if (m_bPlaying && !dxlib_music_is_playing())
		m_bPlaying = false;

	return m_bPlaying;
}

long COGGSTREAM::streamVolume(long lVolume)
{
	if (lVolume < DSBVOLUME_MIN) lVolume = DSBVOLUME_MIN;
	if (lVolume > DSBVOLUME_MAX) lVolume = DSBVOLUME_MAX;
	m_Volume = lVolume;

	if (m_pData != NULL)
		dxlib_music_set_volume(AudioVolumeToPercent(m_Volume));

	return m_Volume;
}
