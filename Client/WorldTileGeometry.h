#pragma once

#include "Platform.h"
#include "MTypeDef.h"

namespace WorldTileGeometry {

int PixelToTileX(int pixel);
int PixelToTileY(int pixel);
// Tile origins outside the pixel-coordinate range saturate at its endpoints.
int TileToPixelX(int tile);
int TileToPixelY(int tile);

// One-tile delta in the selected direction; unknown integers yield (0, 0).
POINT DirectionOffset(int direction);

// The sector type retains its unsigned wrap at the map-coordinate boundary.
// An unknown direction leaves both coordinates unchanged.
void Step(TYPE_SECTORPOSITION& x, TYPE_SECTORPOSITION& y, BYTE direction);

} // namespace WorldTileGeometry
