#pragma once

// The current zone's flags. The zone loading (GameMain) sets them; replies
// read them: whether the zone is a safe one, the large and the small zone
// the player is in or beside, and which of the two the player is in.
extern bool	g_bZoneSafe;
extern int	g_nZoneLarge;
extern int	g_nZoneSmall;
extern bool	g_bZonePlayerInLarge;
