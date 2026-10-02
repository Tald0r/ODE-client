#pragma once

#include "MTypeDef.h"
#include "MonotonicClock.h"
#include <functional>
#include <optional>

class ZONETABLE_INFO;

struct AmbientSoundPosition { int x; int y; };
struct AmbientSoundPlayback
{
	TYPE_SOUNDID id;
	bool loop;
	int x;
	int y;
};
struct AmbientSoundUpdate
{
	// Apply a stop before playback when both are present.
	std::optional<TYPE_SOUNDID> stop;
	std::optional<AmbientSoundPlayback> play;
};

// Shared across the large/small zone views, like the original frame deadline
// and propeller flag. Audio playback and the player remain executable-owned.
class AmbientSoundState
{
public:
	using Random = std::function<unsigned()>;
	void SetNextSoundTime(MonotonicClock::TimePoint time) { m_NextSound = time; }
	MonotonicClock::TimePoint GetNextSoundTime() const { return m_NextSound; }
	void OnSoundsStopped() { m_PropellerPlaying = false; }
	bool IsPropellerPlaying() const { return m_PropellerPlaying; }
	void DelayAfterZoneLoad(MonotonicClock::TimePoint now, unsigned randomValue);

	// A missing generator uses rand(). Missing metadata or position skips
	// random playback but still schedules the next attempt. Empty sound lists
	// retain the legacy SOUNDID_NULL request and its coordinate draws.
	AmbientSoundUpdate Update(MonotonicClock::TimePoint now, int zoneID,
		const ZONETABLE_INFO* zone, std::optional<AmbientSoundPosition> player,
		const Random& random = {});

private:
	static unsigned Draw(const Random& random);
	MonotonicClock::TimePoint m_NextSound{};
	bool m_PropellerPlaying = false;
};
