/*-----------------------------------------------------------------------------

	CDirectSoundStream_Adapter.cpp

	DirectSoundStream adapter using DXLibBackend.
	This file provides SDL2 backend support for the CSDLStream class declared
	in CDirectSoundStream.h. CDirectSoundStream.cpp beside it is the fallback
	used only when SDL2_mixer is absent; the two are never compiled together.

	Volume arrives in DirectSound hundredths of a dB and is converted with
	AudioVolumeToPercent at the backend boundary, as in CDirectSound_Adapter.

	2025.01.14

-----------------------------------------------------------------------------*/

#include "CDirectSoundStream.h"
#include "DXLibBackend.h"

/*=============================================================================
 * SDL Backend Implementation
 *=============================================================================*/

#ifdef DXLIB_BACKEND_SDL

/* Constructor */
CSDLStream::CSDLStream()
{
	m_bLoad = false;
	m_bPlay = false;
	m_bLoop = false;

	m_pDSBuffer = NULL;
	m_pDSNotify = NULL;

	m_dwBufferSize = 0;
	m_dwNotifySize = 0;
	m_dwNextWriteOffset = 0;
	m_dwProgress = 0;
	m_dwLastPos = 0;
	m_bFoundEnd = false;

	m_MaxVolume = DSBVOLUME_MAX;
}

/* Destructor */
CSDLStream::~CSDLStream()
{
	Release();
}

/* Release stream */
void CSDLStream::Release()
{
	// Stop if playing
	if (m_bPlay) {
		Stop();
	}

	// Free stream
	if (m_bLoad) {
		dxlib_stream_t stream = (dxlib_stream_t)m_pDSBuffer;
		dxlib_stream_free(stream);
	}

	m_pDSBuffer = NULL;
	m_bLoad = false;
	m_bPlay = false;
}

/* Load audio file for streaming */
void CSDLStream::Load(LPSTR filename)
{
	// Idempotent: opens the mixer if nothing else has yet.
	if (dxlib_stream_init(NULL) != 0) {
		return;
	}

	// Load stream
	dxlib_stream_t stream = dxlib_stream_load(filename);
	if (stream) {
		m_pDSBuffer = (LPDIRECTSOUNDBUFFER)stream;
		m_bLoad = true;
		m_bPlay = false;
	}
}

/* Play stream */
void CSDLStream::Play(BOOL bLooped)
{
	if (!m_bLoad) return;

	dxlib_stream_t stream = (dxlib_stream_t)m_pDSBuffer;
	if (dxlib_stream_play(stream, bLooped ? 1 : 0) == 0) {
		m_bPlay = true;
		m_bLoop = bLooped;
	}
}

/* Stop stream */
void CSDLStream::Stop()
{
	if (!m_bLoad) return;

	dxlib_stream_t stream = (dxlib_stream_t)m_pDSBuffer;
	dxlib_stream_stop(stream);
	m_bPlay = false;
}

/* Set volume limit */
void CSDLStream::SetVolumeLimit(LONG volume)
{
	m_MaxVolume = volume;
	if (m_MaxVolume > DSBVOLUME_MAX) m_MaxVolume = DSBVOLUME_MAX;
	if (m_MaxVolume < DSBVOLUME_MIN) m_MaxVolume = DSBVOLUME_MIN;

	if (m_bLoad) {
		dxlib_stream_t stream = (dxlib_stream_t)m_pDSBuffer;
		dxlib_stream_set_volume(stream, AudioVolumeToPercent(m_MaxVolume));
	}
}

/* Update stream (call regularly to refill buffers) */
void CSDLStream::Update()
{
	if (!m_bLoad || !m_bPlay) return;

	dxlib_stream_t stream = (dxlib_stream_t)m_pDSBuffer;
	dxlib_stream_update(stream);

	// Check if stream has finished
	if (!dxlib_stream_is_playing(stream)) {
		m_bPlay = false;
	}
}

/* Update progress (stub for SDL backend) */
BOOL CSDLStream::UpdateProgress()
{
	// Not applicable for SDL backend
	return TRUE;
}

/* Handle notification (stub for SDL backend) */
BOOL CSDLStream::HandleNotification(BOOL bLooped)
{
	(void)bLooped;
	// Not applicable for SDL backend
	return TRUE;
}

/* Fill buffer (stub for SDL backend) */
BOOL CSDLStream::FillBuffer(BOOL bLooped)
{
	(void)bLooped;
	// Not applicable for SDL backend
	return TRUE;
}

/* Read stream (stub for SDL backend) */
BOOL CSDLStream::ReadStream(BOOL bLooped, VOID* pbBuffer, DWORD dwBufferLength)
{
	(void)bLooped;
	(void)pbBuffer;
	(void)dwBufferLength;
	// Not applicable for SDL backend
	return TRUE;
}

/* Restore buffers (stub for SDL backend) */
BOOL CSDLStream::RestoreBuffers(BOOL bLooped)
{
	(void)bLooped;
	// Not applicable for SDL backend
	return TRUE;
}

/* Reset (stub for SDL backend) */
BOOL CSDLStream::Reset()
{
	// Not applicable for SDL backend
	return TRUE;
}

/* Wave read file (stub for SDL backend) */
HRESULT CSDLStream::WaveReadFile(HMMIO hmmioIn, UINT cbRead, BYTE* pbDest,
	DXLIB_CKINFO* pckIn, UINT* cbActualRead)
{
	(void)hmmioIn;
	(void)cbRead;
	(void)pbDest;
	(void)pckIn;
	(void)cbActualRead;
	// Not applicable for SDL backend
	return S_OK;
}

#endif /* DXLIB_BACKEND_SDL */
