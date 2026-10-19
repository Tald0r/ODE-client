/*-----------------------------------------------------------------------------

	AudioTypes.h

	Platform-independent audio type definitions and constants.
	Replaces DirectX DSBVOLUME_*, DSBCAPS_* constants.

	2025.02.04 - Created to eliminate DirectX audio dependencies

-----------------------------------------------------------------------------*/

#ifndef __AUDIOTYPES_H__
#define __AUDIOTYPES_H__

/* Include Platform.h for basic type definitions */
#include "Platform.h"

/* pow() for the dB -> linear volume conversion at the bottom of this file */
#include <math.h>

/*-----------------------------------------------------------------------------
	Volume Constants

	DirectX used logarithmic volume scale from -10000 to 0.
	SDL_mixer uses linear scale from 0 to MIX_MAX_VOLUME (128).

	For compatibility, we maintain the DirectX constants but map them
	to SDL volume values in the implementation.
-----------------------------------------------------------------------------*/

/* Volume range (DirectX-compatible values) */
#ifndef DSBVOLUME_MIN
#define DSBVOLUME_MIN            -10000    /* Silence */
#endif

#ifndef DSBVOLUME_MAX
#define DSBVOLUME_MAX            0         /* Full volume */
#endif

/* New platform-independent names */
#define AUDIO_VOLUME_MIN          DSBVOLUME_MIN
#define AUDIO_VOLUME_MAX          DSBVOLUME_MAX

/* The game's volume slider, 0..15: a position is value*SOUND_DEGREE +
   SOUND_MIN (Client.h's until task 4.258). */
const LONG SOUND_MIN	= DSBVOLUME_MIN / 4;
const LONG SOUND_RANGE	= DSBVOLUME_MAX - SOUND_MIN;
const LONG SOUND_DEGREE = SOUND_RANGE / 15;

/*-----------------------------------------------------------------------------
	Buffer Capabilities

	These flags control sound buffer properties.
	In SDL, these are handled differently but we maintain the
	interface for compatibility with existing code.
-----------------------------------------------------------------------------*/

#ifndef DSBCAPS_CTRLFREQUENCY
#define DSBCAPS_CTRLFREQUENCY     0x00000001  /* Frequency control */
#endif

#ifndef DSBCAPS_CTRLPAN
#define DSBCAPS_CTRLPAN           0x00000002  /* Pan control */
#endif

#ifndef DSBCAPS_CTRLVOLUME
#define DSBCAPS_CTRLVOLUME        0x00000004  /* Volume control */
#endif

#ifndef DSBCAPS_GLOBALFOCUS
#define DSBCAPS_GLOBALFOCUS       0x00000080  /* Global focus */
#endif

/* New platform-independent names */
#define AUDIO_BUFFER_CTRLFREQUENCY    DSBCAPS_CTRLFREQUENCY
#define AUDIO_BUFFER_CTRLPAN          DSBCAPS_CTRLPAN
#define AUDIO_BUFFER_CTRLVOLUME       DSBCAPS_CTRLVOLUME
#define AUDIO_BUFFER_GLOBALFOCUS      DSBCAPS_GLOBALFOCUS

/*-----------------------------------------------------------------------------
	Multimedia Structures

	Platform-independent definitions for multimedia structures.
	Replaces DirectX multimedia types.
-----------------------------------------------------------------------------*/

/* FOURCC/HMMIO: skip our compat typedefs if the real <mmsystem.h>/
   <mmiscapi.h> was already included earlier in this translation unit
   (WavePackFileManager.cpp includes <MMSystem.h> before CDirectSound.h on
   Windows; CMP3.cpp and MMusic.cpp's MCI branches did until tasks 4.214
   and 4.215 retired them) - defining both is an
   incompatible redefinition (error C2371/C2011). _INC_MMSYSTEM and
   _MMISCAPI_H_ are those headers' own include guards. */
#if !defined(_INC_MMSYSTEM) && !defined(_MMISCAPI_H_)
/* Forward declarations */
typedef DWORD FOURCC;
typedef void* HMMIO;
#endif /* !_INC_MMSYSTEM && !_MMISCAPI_H_ */

/* Multimedia RIFF chunk information for the dxlib stream stubs.
   Deliberately NOT named MMCKINFO: the real <mmsystem.h> struct has five
   members in a different order (ckid, cksize, fccType, dwDataOffset,
   dwFlags), so a translation unit mixing the two names across a call
   boundary would read every field at the wrong offset. Code that parses
   RIFF chunks with the real mmio API (WavePackFileManager.cpp) uses the
   real MMCKINFO from <mmsystem.h> instead. */
typedef struct _DXLIB_CKINFO {
    FOURCC  ckid;
    FOURCC  fccType;
    DWORD   dwDataOffset;
    DWORD   dwSize;
} DXLIB_CKINFO;

/* DSBPOSITIONNOTIFY: same idea, deferring to the real <dsound.h> if it was
   already included earlier in this translation unit. __DSOUND_INCLUDED__ is
   dsound.h's own include guard. */
#ifndef __DSOUND_INCLUDED__
typedef struct _DSBPOSITIONNOTIFY {
    DWORD   dwFlags;
    DWORD   dwOffset;
    DWORD   dwCallback;
} DSBPOSITIONNOTIFY;
#endif /* !__DSOUND_INCLUDED__ */

/* Multimedia constants */
#ifndef MMIO_READ
#define MMIO_READ      0
#endif

#ifndef MMIO_ALLOCBUF
#define MMIO_ALLOCBUF  0x10000
#endif

#ifndef FOURCC
#define FOURCC(a,b,c,d) (((DWORD)(a)<<0)|((DWORD)(b)<<8)|((DWORD)(c)<<16)|((DWORD)(d)<<24))
#endif

/*-----------------------------------------------------------------------------
	Volume / Pan Conversion

	Game code still speaks DirectSound units: a volume is an attenuation in
	hundredths of a decibel (DSBVOLUME_MIN..DSBVOLUME_MAX, see SOUND_MIN and
	SOUND_DEGREE above) and a pan runs DSBPAN_LEFT..DSBPAN_RIGHT. The
	SDL backend wants linear percentages. These two functions are the only
	place that mapping lives, so the adapters and the unit tests agree.
-----------------------------------------------------------------------------*/

#ifndef DSBPAN_LEFT
#define DSBPAN_LEFT              -10000    /* Fully left */
#endif

#ifndef DSBPAN_CENTER
#define DSBPAN_CENTER            0
#endif

#ifndef DSBPAN_RIGHT
#define DSBPAN_RIGHT             10000     /* Fully right */
#endif

/* DirectSound defines the volume as 20*log10(amplitude) in hundredths of a
   dB, so amplitude = 10^(v/2000). DSBVOLUME_MIN is silence by definition and
   anything at or above DSBVOLUME_MAX is full scale; the result is rounded to
   the nearest percent. */
static inline int AudioVolumeToPercent(long hundredthsOfDb)
{
	if (hundredthsOfDb >= DSBVOLUME_MAX) return 100;
	if (hundredthsOfDb <= DSBVOLUME_MIN) return 0;

	double amplitude = pow(10.0, (double)hundredthsOfDb / 2000.0);
	return (int)(amplitude * 100.0 + 0.5);
}

/* DSBPAN_LEFT..DSBPAN_RIGHT -> -100..100, clamped. */
static inline int AudioPanToPercent(long pan)
{
	if (pan <= DSBPAN_LEFT) return -100;
	if (pan >= DSBPAN_RIGHT) return 100;
	return (int)(pan / 100);
}

#endif // __AUDIOTYPES_H__
