#pragma once

#include <cstdint>
#include "MusicDef.h"

// Select a track from the current game state. Playback, event suppression,
// settings and file lookup stay with the caller. Hours repeat every 24 hours.
MUSIC_ID SelectZoneMusic(int zoneID, std::uint8_t hour, bool holyLand, bool waveMusic, bool war);
