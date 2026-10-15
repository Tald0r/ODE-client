//----------------------------------------------------------------------
// MMusic.cpp
//----------------------------------------------------------------------
// The state of a MIDI player that plays nothing (see MMusic.h). Init
// reports failure but marks the player initialised. Once initialised:
// - Play loads and plays (stopping a loaded file first), yet reports
//   failure, as nothing plays.
// - Stop, Pause, Resume and RePlay move between loaded, playing and
//   paused.
// Before Init, every call but SetVolume refuses.
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MMusic.h"

MMusic		g_Music;

MMusic::MMusic()
{
	m_bLoad = false;
	m_bPlay = false;
	m_bInit = false;
	m_bPause = false;
	m_Volume = 0;
}

MMusic::~MMusic()
{
}

bool MMusic::Init(HWND hwnd)
{
	(void)hwnd;
	m_bInit = true;
	m_bPause = false;
	return false;  // nothing plays
}

void MMusic::SetVolume(WORD volume)
{
	m_Volume = volume;
}

void MMusic::UnInit()
{
	if(m_bInit && m_bLoad)
	{
		Stop();
	}
}

bool MMusic::Play(LPCSTR filename)
{
	(void)filename;
	if (m_bInit)
	{
		if(m_bLoad)
		{
			Stop();
		}
		m_bLoad = true;
		m_bPlay = true;
		m_bPause = false;
		return false;  // nothing plays
	}
	return false;
}

bool MMusic::Stop()
{
	if (m_bInit)
	{
		if(!m_bPlay) return true;
		m_bLoad = false;
		m_bPlay = false;
		m_bPause = false;
		return true;
	}
	return false;
}

bool MMusic::Pause()
{
	if (m_bInit)
	{
		if(!m_bLoad || !m_bPlay) return true;
		m_bPlay = false;
		m_bPause = true;
		return true;
	}
	return false;
}

bool MMusic::Resume()
{
	if (m_bInit)
	{
		if(!m_bLoad || m_bPlay) return false;
		m_bPlay = true;
		m_bPause = false;
		return true;
	}
	return false;
}

bool MMusic::RePlay()
{
	if (m_bInit)
	{
		if(!m_bLoad) return false;
		m_bPlay = true;
		m_bPause = false;
		return true;
	}
	return false;
}
