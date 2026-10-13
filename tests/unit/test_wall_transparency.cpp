#include "test_framework.h"
#include "MImageObject.h"
#include "WorldTileGeometry.h"

// A wall the player stands behind is drawn see-through. The wall's sector
// X is its pixel X in whole tiles; a wall running down to the right hides
// the cells with x - y past its sector X less its viewpoint, one running
// up to the right the cells with x + y short of its sector X plus its
// viewpoint. Anything else that is not a wall hides every cell.
namespace {

MImageObject Wall(MImageObject::WALL_DIRECTION direction, int tileX, int pixelInTile, TYPE_SECTORPOSITION viewpoint)
{
	MImageObject wall;
	wall.SetWall(direction);
	wall.SetPixelX(WorldTileGeometry::TileToPixelX(tileX) + pixelInTile);
	wall.SetViewpoint(viewpoint);
	return wall;
}

} // namespace

TEST(WallTransparency, AWallRunningDownToTheRightHidesTheCellsPastItsDiagonal)
{
	// Sector X 10, viewpoint 4: x - y must pass 6.
	const MImageObject wall = Wall(MImageObject::WALL_RIGHTDOWN, 10, 0, 4);
	CHECK(wall.IsWallTransPosition(20, 13));
	CHECK(!wall.IsWallTransPosition(20, 14));
	CHECK(!wall.IsWallTransPosition(0, 0));
}

TEST(WallTransparency, AWallRunningUpToTheRightHidesTheCellsShortOfItsDiagonal)
{
	// Sector X 10, viewpoint 4: x + y must stay under 14.
	const MImageObject wall = Wall(MImageObject::WALL_RIGHTUP, 10, 0, 4);
	CHECK(wall.IsWallTransPosition(6, 7));
	CHECK(!wall.IsWallTransPosition(7, 7));
	CHECK(!wall.IsWallTransPosition(20, 20));
}

TEST(WallTransparency, TheWallsSectorIsItsPixelXInWholeTiles)
{
	// The last pixel of tile 10 is still tile 10, so the diagonal is the same.
	const MImageObject edge = Wall(MImageObject::WALL_RIGHTDOWN, 10, WorldTileGeometry::TileToPixelX(1) - 1, 4);
	CHECK(edge.IsWallTransPosition(20, 13));
	CHECK(!edge.IsWallTransPosition(20, 14));
	const MImageObject next = Wall(MImageObject::WALL_RIGHTDOWN, 11, 0, 4);
	CHECK(!next.IsWallTransPosition(20, 13));
}

TEST(WallTransparency, AnObjectThatIsNoWallHidesEveryCell)
{
	const MImageObject plain = Wall(MImageObject::WALL_NULL, 10, 0, 4);
	CHECK(plain.IsWallTransPosition(0, 0));
	CHECK(plain.IsWallTransPosition(200, 0));
}
