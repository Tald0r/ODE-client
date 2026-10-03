#include "Client_PCH.h"
#include "WorldTileGeometry.h"
#include "MViewDef.h"

namespace WorldTileGeometry {

int PixelToTileX(int pixel) { return pixel / TILE_X; }
int PixelToTileY(int pixel) { return pixel / TILE_Y; }
int TileToPixelX(int tile) { return tile * TILE_X; }
int TileToPixelY(int tile) { return tile * TILE_Y; }

void Step(TYPE_SECTORPOSITION& x, TYPE_SECTORPOSITION& y, BYTE direction)
{
	switch (direction)
	{
		case DIRECTION_LEFTDOWN:  x--; y++; break;
		case DIRECTION_RIGHTUP:   x++; y--; break;
		case DIRECTION_LEFTUP:    x--; y--; break;
		case DIRECTION_RIGHTDOWN: x++; y++; break;
		case DIRECTION_LEFT:      x--;      break;
		case DIRECTION_DOWN:           y++; break;
		case DIRECTION_UP:             y--; break;
		case DIRECTION_RIGHT:     x++;      break;
	}
}

} // namespace WorldTileGeometry
