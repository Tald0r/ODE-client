#pragma once

// The creature type the player last morphed into with a skill, 0 when none.
// The morph skill's inventory reply sets it; the player's next info update,
// or a creature-type change on the player, clears it.
extern int g_MorphCreatureType;
