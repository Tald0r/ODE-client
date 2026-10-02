#pragma once

#include "SoundNode.h"
#include <functional>
#include <list>

// Pending sounds are values owned by this queue. Playback is supplied per
// update; an absent callback still discards sounds whose time has passed.
class DelayedSoundQueue
{
public:
	using Play = std::function<void(const SOUND_NODE&)>;
	DelayedSoundQueue() = default;
	DelayedSoundQueue(const DelayedSoundQueue&) = delete;
	DelayedSoundQueue& operator=(const DelayedSoundQueue&) = delete;

	void Add(const SOUND_NODE& sound);
	void Clear();
	std::size_t GetSize() const { return m_Sounds.size(); }
	// Preserve insertion order among ready sounds. An exact deadline waits
	// until a later frame, matching the zone's strict time comparison.
	void Update(MonotonicClock::TimePoint now, const Play& play);

private:
	std::list<SOUND_NODE> m_Sounds;
};
