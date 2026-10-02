#include "DelayedSoundQueue.h"

void DelayedSoundQueue::Add(const SOUND_NODE& sound)
{
	m_Sounds.push_back(sound);
	++m_Revision;
}

void DelayedSoundQueue::Clear()
{
	m_Sounds.clear();
	++m_Revision;
}

void DelayedSoundQueue::Update(MonotonicClock::TimePoint now, const Play& play)
{
	auto sound = m_Sounds.begin();
	while (sound != m_Sounds.end())
	{
		if (sound->GetPlayTime() < now)
		{
			const SOUND_NODE ready = *sound;
			sound = m_Sounds.erase(sound);
			const auto revision = ++m_Revision;
			if (play) play(ready);
			// Add, Clear and a nested Update may have changed the pending list.
			// Restart only after such a change; normal playback stays one pass.
			if (m_Revision != revision) sound = m_Sounds.begin();
		}
		else ++sound;
	}
}
