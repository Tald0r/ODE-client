#include "Client_PCH.h"
#include "WorldTileGeometry.h"
#include "MViewDef.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace WorldTileGeometry {

namespace {

int TileToPixel(int tile, int scale)
{
	const auto pixel = static_cast<std::int64_t>(tile) * scale;
	return static_cast<int>(std::clamp<std::int64_t>(pixel,
		(std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()));
}

} // namespace

int PixelToTileX(int pixel) { return pixel / TILE_X; }
int PixelToTileY(int pixel) { return pixel / TILE_Y; }
int TileToPixelX(int tile) { return TileToPixel(tile, TILE_X); }
int TileToPixelY(int tile) { return TileToPixel(tile, TILE_Y); }

POINT DirectionOffset(int direction)
{
	POINT pt = { 0, 0 };

	switch (direction)
	{
		case DIRECTION_LEFTDOWN		: pt.x=-1;	pt.y=1;	break;
		case DIRECTION_RIGHTUP		: pt.x=1;	pt.y=-1;	break;
		case DIRECTION_LEFTUP		: pt.x=-1;	pt.y=-1;	break;
		case DIRECTION_RIGHTDOWN	: pt.x=1;	pt.y=1;	break;
		case DIRECTION_LEFT			: pt.x=-1;			break;
		case DIRECTION_DOWN			: pt.y=1;	break;
		case DIRECTION_UP			: pt.y=-1;	break;
		case DIRECTION_RIGHT		: pt.x=1;			break;
	}

	return pt;
}

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
