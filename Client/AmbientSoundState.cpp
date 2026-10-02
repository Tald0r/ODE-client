#include "AmbientSoundState.h"
#include "MZoneTable.h"
#include "SoundDef.h"
#include <cstdlib>

unsigned AmbientSoundState::Draw(const Random& random)
{
	return random ? random() : static_cast<unsigned>(std::rand());
}

void AmbientSoundState::DelayAfterZoneLoad(MonotonicClock::TimePoint now, unsigned randomValue)
{
	m_NextSound = now + MonotonicClock::Millis(((randomValue % 5) + 10) * 1000);
}

AmbientSoundUpdate AmbientSoundState::Update(MonotonicClock::TimePoint now, int zoneID,
	const ZONETABLE_INFO* zone, std::optional<AmbientSoundPosition> player, const Random& random)
{
	AmbientSoundUpdate actions;
	// Commit decisions together after all random draws succeed. The caller
	// applies the returned actions afterward, without callbacks into this state.
	auto next = *this;
	if (zoneID == 2106 || zoneID == 2004 || zoneID == 2014 || zoneID == 2024)
	{
		if (!next.m_PropellerPlaying && player)
		{
			actions.play = {SOUND_WORLD_PROPELLER, true, player->x, player->y};
			next.m_PropellerPlaying = true;
		}
	}
	else
	{
		if (next.m_PropellerPlaying)
		{
			actions.stop = SOUND_WORLD_PROPELLER;
			next.m_PropellerPlaying = false;
		}
		if (now > next.m_NextSound)
		{
			if (zone && player)
			{
				const TYPE_SOUNDID id = zone->SoundIDList.empty() ? SOUNDID_NULL
					: zone->GetRandomSoundID(Draw(random));
				const int xSign = Draw(random) % 2 ? 1 : -1;
				const int xDistance = static_cast<int>(Draw(random) % 15) + 13;
				const int ySign = Draw(random) % 2 ? 1 : -1;
				const int yDistance = static_cast<int>(Draw(random) % 12) + 10;
				actions.play = {id, false, player->x + xSign * xDistance, player->y + ySign * yDistance};
			}
			next.m_NextSound = now + MonotonicClock::Millis(((Draw(random) % 10) + 6) * 1000);
		}
	}
	*this = next;
	return actions;
}
