#include "test_framework.h"
#include "ZoneTileProvider.h"
#include <vector>

// The tile layer's view of a zone (task 4.223): its size, and each
// sector's sprite. Sprite 0 or SPRITEID_NULL, a cell off the zone, or no
// zone at all is the null tile (-1); no zone is 0 by 0.
namespace {

struct Sector
{
	WORD sprite;
	WORD GetSpriteID() const { return sprite; }
};

struct Zone
{
	unsigned short width, height;
	std::vector<Sector> sectors;
	unsigned short GetWidth() const { return width; }
	unsigned short GetHeight() const { return height; }
	// A cell off the zone has a sprite of its own, so asking for one shows.
	const Sector& GetSector(unsigned short x, unsigned short y) const
	{
		static const Sector off{0x4242};
		if (x >= width || y >= height) return off;
		return sectors[static_cast<size_t>(y) * width + x];
	}
};

// A 3x2 zone: 5, 0, 7 / SPRITEID_NULL, 9, 0xFFFE.
Zone MakeZone()
{
	return Zone{3, 2, {{5}, {0}, {7}, {SPRITEID_NULL}, {9}, {0xFFFE}}};
}

} // namespace

TEST(ZoneTileProvider, WithoutAZoneEveryTileIsNull)
{
	ZoneTileProvider<Zone> provider;
	CHECK_EQ(0, provider.GetWidth());
	CHECK_EQ(0, provider.GetHeight());
	CHECK_EQ(-1, provider.GetSpriteID(0, 0));

	Zone zone = MakeZone();
	provider.SetZone(&zone);
	provider.SetZone(nullptr);
	CHECK_EQ(0, provider.GetWidth());
	CHECK_EQ(-1, provider.GetSpriteID(0, 0));
}

TEST(ZoneTileProvider, ItGivesTheZonesSizeAndSprites)
{
	Zone zone = MakeZone();
	ZoneTileProvider<Zone> provider;
	provider.SetZone(&zone);
	CHECK_EQ(3, provider.GetWidth());
	CHECK_EQ(2, provider.GetHeight());
	CHECK_EQ(5, provider.GetSpriteID(0, 0));
	CHECK_EQ(7, provider.GetSpriteID(2, 0));
	CHECK_EQ(9, provider.GetSpriteID(1, 1));
	CHECK_EQ(0xFFFE, provider.GetSpriteID(2, 1));
	// Sprite 0 and SPRITEID_NULL are the null tile.
	CHECK_EQ(-1, provider.GetSpriteID(1, 0));
	CHECK_EQ(-1, provider.GetSpriteID(0, 1));
}

TEST(ZoneTileProvider, ACellOffTheZoneIsNull)
{
	Zone zone = MakeZone();
	ZoneTileProvider<Zone> provider;
	provider.SetZone(&zone);
	CHECK_EQ(-1, provider.GetSpriteID(-1, 0));
	CHECK_EQ(-1, provider.GetSpriteID(0, -1));
	CHECK_EQ(-1, provider.GetSpriteID(3, 0));
	CHECK_EQ(-1, provider.GetSpriteID(0, 2));
	CHECK_EQ(-1, provider.GetSpriteID(3, 2));
}

TEST(ZoneTileProvider, TheRendererDrawsThroughIt)
{
	// Through the interface TileRenderer reads.
	Zone zone = MakeZone();
	ZoneTileProvider<Zone> provider;
	provider.SetZone(&zone);
	ITileDataProvider& tiles = provider;
	CHECK_EQ(3, tiles.GetWidth());
	CHECK_EQ(2, tiles.GetHeight());
	CHECK_EQ(9, tiles.GetSpriteID(1, 1));
}
