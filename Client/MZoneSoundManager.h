//-----------------------------------------------------------------------------
// MZoneSoundManager.h
//-----------------------------------------------------------------------------
// When the player moves while a zone's sounds play, their direction and
// volume change. Momentary sounds can be ignored, but looping or long ones
// have to follow.
//
// ZONESOUND_NODE is one sound playing now; MZoneSoundManager manages them
// (task 4.227, gamemodel's since then). What they ask of the game - the
// audio device, its sound cache, the options, the player's sector, the
// zone's sector sounds and the clocks - goes through ZoneSoundHost.
//-----------------------------------------------------------------------------

#ifndef	__MZONESOUNDMANAGER_H__
#define __MZONESOUNDMANAGER_H__

#include "MZoneSound.h"
#include "CTypeMap.h"

/* This project no longer uses real DirectSound - including the real
   <DSound.h> here redefines _DSBPOSITIONNOTIFY (from basic/AudioTypes.h)
   with an incompatible duplicate, the same conflict already fixed for
   CSoundPartManager.h/CDirectDraw/MMSystem elsewhere. basic/Platform.h only
   forward-declares LPDIRECTSOUNDBUFFER on non-Windows (it expects the real
   header to supply it on Windows), so declare the opaque pointer type here
   directly for all platforms instead. */
#include "../basic/Platform.h"
#include "MonotonicClock.h"

#ifndef LPDIRECTSOUNDBUFFER
typedef struct IDirectSoundBuffer* LPDIRECTSOUNDBUFFER;
#endif

#include <map>
#include <fstream>
#include <optional>
#include "SectorSoundInfo.h"

struct ZoneSoundHost
{
	// The audio device and its buffers.
	bool (*AudioReady)() = nullptr;
	LPDIRECTSOUNDBUFFER (*LoadWav)(const char* filename) = nullptr;
	LPDIRECTSOUNDBUFFER (*Duplicate)(LPDIRECTSOUNDBUFFER buffer) = nullptr;
	void (*Release)(LPDIRECTSOUNDBUFFER buffer) = nullptr;
	bool (*IsPlaying)(LPDIRECTSOUNDBUFFER buffer) = nullptr;
	void (*Play)(LPDIRECTSOUNDBUFFER buffer, bool loop) = nullptr;
	void (*Stop)(LPDIRECTSOUNDBUFFER buffer) = nullptr;
	void (*CenterToRightPan)(LPDIRECTSOUNDBUFFER buffer, int pan) = nullptr;
	void (*CenterToLeftPan)(LPDIRECTSOUNDBUFFER buffer, int pan) = nullptr;
	void (*CenterPan)(LPDIRECTSOUNDBUFFER buffer) = nullptr;
	void (*SubVolumeFromMax)(LPDIRECTSOUNDBUFFER buffer, int volume) = nullptr;
	// The sound cache: whether a sound is in it; storing one (false when it
	// was not stored, otherwise old is what it pushed out); reading one.
	bool (*IsCached)(TYPE_SOUNDID id) = nullptr;
	bool (*Cache)(TYPE_SOUNDID id, LPDIRECTSOUNDBUFFER buffer, LPDIRECTSOUNDBUFFER& old) = nullptr;
	bool (*GetCached)(TYPE_SOUNDID id, LPDIRECTSOUNDBUFFER& buffer) = nullptr;
	// The options.
	bool (*SoundOn)() = nullptr;
	int (*SoundVolume)() = nullptr;
	// The player's sector; false without a player.
	bool (*PlayerPosition)(int& x, int& y) = nullptr;
	// The sounds heard in the zone's sector; null without a zone.
	const SECTORSOUND_LIST* (*SectorSounds)(int x, int y) = nullptr;
	// The frame clock, and the game's hour (none before it is known).
	MonotonicClock::TimePoint (*Now)() = nullptr;
	std::optional<BYTE> (*GameHour)() = nullptr;
};

//-----------------------------------------------------------------------------
// ZONESOUND_NODE
//-----------------------------------------------------------------------------
// A sound playing now. m_pBuffer holds a duplicate of the cached buffer
// of its g_pSoundTable entry.
//-----------------------------------------------------------------------------
class ZONESOUND_NODE
{
	public :
		ZONESOUND_NODE(TYPE_SOUNDID id=SOUNDID_NULL);
		~ZONESOUND_NODE();

		// Without a host no sound plays.
		static const ZoneSoundHost*	SetHost(const ZoneSoundHost* pHost);

		//---------------------------------------------------------------
		// Get
		//---------------------------------------------------------------
		TYPE_SOUNDID			GetSoundID() const		{ return m_SoundID; }
		LPDIRECTSOUNDBUFFER		GetBuffer() const		{ return m_pBuffer; }
		int						GetX() const			{ return m_X; }
		int						GetY() const			{ return m_Y; }
		bool					IsLoop() const			{ return m_bLoop; }

		//---------------------------------------------------------------
		// Play
		//---------------------------------------------------------------
		void					Play(int x=0, int y=0, bool bLoop=false);
		void					Stop();
		
		//---------------------------------------------------------------
		// Loop
		//---------------------------------------------------------------
		void					StopLoop();			
		bool					IsContinueLoop() const	{ return m_bContinueLoop; }
		void					SetContinueLoop()		{ m_bContinueLoop = true; }
		void					UnSetContinueLoop()		{ m_bContinueLoop = false; }
		
		//---------------------------------------------------------------
		// File I/O
		//---------------------------------------------------------------
		void					SaveToFile(std::ofstream& file);
		void					LoadFromFile(std::ifstream& file);

	protected :
		TYPE_SOUNDID				m_SoundID;				// Sound ID
		LPDIRECTSOUNDBUFFER			m_pBuffer;				// buffer
		int							m_X, m_Y;				// where the sound is
		bool						m_bLoop;				// whether it loops
		bool						m_bContinueLoop;		// keep looping?
		int							m_playerX, m_playerY;	// the player's sector when it played
};


//-----------------------------------------------------------------------------
// MZoneSoundTable
//-----------------------------------------------------------------------------
// The sounds of the zones, by ZoneSoundID
//-----------------------------------------------------------------------------
class MZoneSoundTable : public CTypeMap<ZONESOUND_INFO>
{
	public :
		MZoneSoundTable();
		~MZoneSoundTable();
};

//-----------------------------------------------------------------------------
// MZoneSoundManager
//-----------------------------------------------------------------------------
// The sounds playing now, by ZoneSoundID
//-----------------------------------------------------------------------------
class MZoneSoundManager : public CTypeMap<ZONESOUND_NODE>
{
	public :
		MZoneSoundManager();
		~MZoneSoundManager();

		//---------------------------------------------------------------------
		// Update Sound
		//---------------------------------------------------------------------		
		void			UpdateSound();

	protected :
		TYPE_SECTORPOSITION		m_LastX, m_LastY;
		MonotonicClock::TimePoint	m_LastUpdateTime;
};

//-----------------------------------------------------------------------------
// global
//-----------------------------------------------------------------------------
extern MZoneSoundTable*		g_pZoneSoundTable;

extern MZoneSoundManager*	g_pZoneSoundManager;

#endif
