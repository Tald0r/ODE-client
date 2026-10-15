//----------------------------------------------------------------------
// MMusic.h
//----------------------------------------------------------------------
// The MIDI player's state: initialised, loaded, playing, paused, and the
// volume. The client plays no MIDI. The Win32 MCI implementation has not
// been built since 9e60d44e put it behind __USE_REAL_MIDI__, which nothing
// defines, and task 4.214 retired it; this keeps only the state the game
// asks about.
//----------------------------------------------------------------------
#ifndef __MMUSIC_H__
#define __MMUSIC_H__

#include "../basic/Platform.h"

class MMusic
{
public:
	bool		m_bInit;
	bool		m_bLoad;						// is a file loaded?
	bool		m_bPlay;						// is it playing?
	bool		m_bPause;						// is it paused?
	WORD		m_Volume;

public:
	MMusic();
	~MMusic();
	
	bool		Init(HWND);
	void		UnInit();

	bool		IsInit() const		{ return m_bInit; }

	bool		IsPlay() const		{ return m_bPlay; }
	bool		IsPause() const		{ return m_bLoad && m_bPause; }

	bool		Play(LPCSTR filename);
	bool		Stop();
	bool		Pause();
	bool		Resume();
	bool		RePlay();

	void		SetVolume(WORD volume);
	WORD		GetVolume()			{ return m_Volume; }

};

extern MMusic		g_Music;

#endif
