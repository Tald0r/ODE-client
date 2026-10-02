#pragma once

#include "SoundNode.h"
#include <cstdint>
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
	// Consume each record before playback, even if it throws. Callbacks may
	// append, clear or recursively update, but must keep the queue alive.
	// The supplied record remains valid only for the duration of the callback.
	void Update(MonotonicClock::TimePoint now, const Play& play);

private:
	std::list<SOUND_NODE> m_Sounds;
	std::uint64_t m_Revision = 0;
};
