#include "DelayedSoundQueue.h"

void DelayedSoundQueue::Add(const SOUND_NODE& sound)
{
	m_Sounds.push_back(sound);
}

void DelayedSoundQueue::Clear()
{
	m_Sounds.clear();
}

void DelayedSoundQueue::Update(MonotonicClock::TimePoint now, const Play& play)
{
	auto sound = m_Sounds.begin();
	while (sound != m_Sounds.end())
	{
		if (sound->GetPlayTime() < now)
		{
			if (play) play(*sound);
			sound = m_Sounds.erase(sound);
		}
		else ++sound;
	}
}
