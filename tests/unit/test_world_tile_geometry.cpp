#include "test_framework.h"
#include "WorldTileGeometry.h"
#include "MViewDef.h"

#include <limits>
#include <initializer_list>

TEST(WorldTileGeometry, PixelConversionTruncatesTowardZeroAtTileEdges)
{
	struct Case { int pixel, x, y; };
	for (const Case c : {Case{0, 0, 0}, {1, 0, 0}, {-1, 0, 0}, {23, 0, 0}, {-23, 0, 0},
		{24, 0, 1}, {-24, 0, -1}, {47, 0, 1}, {-47, 0, -1}, {48, 1, 2}, {-48, -1, -2},
		{49, 1, 2}, {-49, -1, -2}})
	{
		CHECK_EQ(c.x, WorldTileGeometry::PixelToTileX(c.pixel));
		CHECK_EQ(c.y, WorldTileGeometry::PixelToTileY(c.pixel));
	}
}

TEST(WorldTileGeometry, PixelConversionAcceptsTheEntireIntegerRange)
{
	const int top = (std::numeric_limits<int>::max)(), bottom = (std::numeric_limits<int>::min)();
	CHECK_EQ(44739242, WorldTileGeometry::PixelToTileX(top));
	CHECK_EQ(-44739242, WorldTileGeometry::PixelToTileX(bottom));
	CHECK_EQ(89478485, WorldTileGeometry::PixelToTileY(top));
	CHECK_EQ(-89478485, WorldTileGeometry::PixelToTileY(bottom));
}

TEST(WorldTileGeometry, TileOriginsAndRepresentableRoundTripsKeepTheirScale)
{
	struct Case { int tile, x, y; };
	for (const Case c : {Case{0, 0, 0}, {1, 48, 24}, {-1, -48, -24},
		{2000, 96000, 48000}, {65535, 3145680, 1572840}, {-2000, -96000, -48000}})
	{
		CHECK_EQ(c.x, WorldTileGeometry::TileToPixelX(c.tile));
		CHECK_EQ(c.y, WorldTileGeometry::TileToPixelY(c.tile));
		CHECK_EQ(c.tile, WorldTileGeometry::PixelToTileX(c.x));
		CHECK_EQ(c.tile, WorldTileGeometry::PixelToTileY(c.y));
	}
}

TEST(WorldTileGeometry, AllEightDirectionsStepOneSector)
{
	struct Case { BYTE direction; int x, y; };
	for (const Case c : {Case{DIRECTION_LEFTDOWN, 9, 11}, {DIRECTION_RIGHTUP, 11, 9},
		{DIRECTION_LEFTUP, 9, 9}, {DIRECTION_RIGHTDOWN, 11, 11}, {DIRECTION_LEFT, 9, 10},
		{DIRECTION_DOWN, 10, 11}, {DIRECTION_UP, 10, 9}, {DIRECTION_RIGHT, 11, 10}})
	{
		TYPE_SECTORPOSITION x = 10, y = 10;
		WorldTileGeometry::Step(x, y, c.direction);
		CHECK_EQ(c.x, x); CHECK_EQ(c.y, y);
	}
}

TEST(WorldTileGeometry, SectorSteppingRetainsUnsignedBoundaryWrap)
{
	TYPE_SECTORPOSITION x = 0, y = 0;
	WorldTileGeometry::Step(x, y, DIRECTION_LEFTUP);
	CHECK_EQ(65535, x); CHECK_EQ(65535, y);
	WorldTileGeometry::Step(x, y, DIRECTION_RIGHTDOWN);
	CHECK_EQ(0, x); CHECK_EQ(0, y);
	x = 65535; y = 0;
	WorldTileGeometry::Step(x, y, DIRECTION_RIGHTUP);
	CHECK_EQ(0, x); CHECK_EQ(65535, y);
	WorldTileGeometry::Step(x, y, DIRECTION_LEFTDOWN);
	CHECK_EQ(65535, x); CHECK_EQ(0, y);
}

TEST(WorldTileGeometry, UnknownDirectionBytesLeaveBothCoordinatesUntouched)
{
	for (int direction = 8; direction <= 255; ++direction)
	{
		TYPE_SECTORPOSITION x = 123, y = 456;
		WorldTileGeometry::Step(x, y, static_cast<BYTE>(direction));
		CHECK_EQ(123, x); CHECK_EQ(456, y);
	}
}
