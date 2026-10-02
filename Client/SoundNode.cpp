//----------------------------------------------------------------------
// SoundNode.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "SoundNode.h"
#include "SoundDef.h"

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Set
//----------------------------------------------------------------------
void			
SOUND_NODE::Set(TYPE_SOUNDID sid, DWORD delay, int x, int y, MonotonicClock::TimePoint now)
{
	m_PlayTime	= now + MonotonicClock::Millis(delay);
	m_SoundID	= sid;
	m_X			= x;
	m_Y			= y;
}

SOUND_NODE MakeThunderSound(DWORD delay, int x, int y, MonotonicClock::TimePoint now)
{
	const TYPE_SOUNDID sound = delay <= 1000 ? SOUND_WORLD_WEATHER_THUNDER_1 : SOUND_WORLD_WEATHER_THUNDER_2;
	return SOUND_NODE(sound, delay, x, y, now);
}
