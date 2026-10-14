#include "test_framework.h"
#include "TileRenderer.h"
#include "SpriteLibBackendSDL.h"
#include <cstdio>
#include <fstream>
#include <vector>

// The tile layer's renderer (task 4.213, SpriteLib's since then). It draws
// a pack's sprite at a point on its surface: 0 draws nothing, the null
// tile (SPRITEID_NULL or -1) draws the null-tile pack's sprite when there
// is one, and an ID past the pack or not loaded fails. DrawTiles walks a
// provider's map within the viewport, clamped to the map, a tile's width
// and height apart.
namespace {

constexpr const char* path = "tile_renderer_test.bin";
const WORD white = 0xFFFF, green = 0x07E0;

// Loads entry id of the pack as a 2x1 sprite: white, then green.
void LoadSprite(CSpritePack& pack, WORD id)
{
	const WORD words[] = {2, 1, 5, 1, 0, 2, white, green};
	{
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		for (WORD w : words) { out.put(char(w & 0xFF)); out.put(char(w >> 8)); }
	}
	std::ifstream in(path, std::ios::binary);
	pack[id].LoadFromFile(in);
	in.close();
	std::remove(path);
}

WORD Pixel(CSpriteSurface& surface, int x, int y)
{
	spritectl_surface_info_t info{};
	if (spritectl_lock_surface(surface.GetBackendSurface(), &info) != 0) return 0xDEAD;
	const WORD value = reinterpret_cast<const WORD*>(static_cast<const unsigned char*>(info.pixels) + y * info.pitch)[x];
	spritectl_unlock_surface(surface.GetBackendSurface());
	return value;
}

int Lit(CSpriteSurface& surface)
{
	int lit = 0;
	for (int y = 0; y < surface.GetHeight(); ++y)
		for (int x = 0; x < surface.GetWidth(); ++x)
			lit += Pixel(surface, x, y) != 0;
	return lit;
}

struct Scene
{
	CSpriteSurface surface;
	CSpritePack pack;
	TileRenderer renderer;
	Scene()
	{
		spritectl_init();
		surface.Init(40, 30);
		surface.FillSurface(0);
		pack.Init(3);
		LoadSprite(pack, 1);
	}
	// Draws one tile with the surface locked, as MTopView does.
	bool Draw(int id, int x, int y)
	{
		surface.Lock();
		const bool drawn = renderer.DrawTile(id, x, y);
		surface.Unlock();
		return drawn;
	}
};

struct Map : ITileDataProvider
{
	int w, h;
	std::vector<int> ids;
	Map(int width, int height, std::vector<int> tiles) : w(width), h(height), ids(std::move(tiles)) {}
	int GetSpriteID(int x, int y) override { return ids[static_cast<size_t>(y * w + x)]; }
	int GetWidth() const override { return w; }
	int GetHeight() const override { return h; }
};

} // namespace

TEST(TileRenderer, ItNeedsASurfaceAndAPack)
{
	Scene scene;
	CHECK(!scene.renderer.IsInit());
	CHECK(!scene.renderer.Init(nullptr, &scene.pack));
	CHECK(!scene.renderer.Init(&scene.surface, nullptr));
	CHECK(!scene.renderer.IsInit());
	CHECK(!scene.Draw(1, 3, 2));
	scene.renderer.ClearSurface(0x1234);
	CHECK_EQ(0, Lit(scene.surface));
	CHECK_EQ(48, scene.renderer.GetTileX());
	CHECK_EQ(24, scene.renderer.GetTileY());

	CHECK(scene.renderer.Init(&scene.surface, &scene.pack));
	CHECK(scene.renderer.IsInit());
	CHECK(scene.renderer.GetSurface() == &scene.surface);
	CHECK(scene.renderer.GetSpritePack() == &scene.pack);
	scene.renderer.Release();
	CHECK(!scene.renderer.IsInit());
	CHECK(scene.renderer.GetSurface() == nullptr);
	CHECK(scene.renderer.GetSpritePack() == nullptr);
}

TEST(TileRenderer, ATileIsDrawnAtItsPoint)
{
	Scene scene;
	scene.renderer.Init(&scene.surface, &scene.pack);
	CHECK(scene.Draw(1, 3, 2));
	CHECK_EQ(white, Pixel(scene.surface, 3, 2));
	CHECK_EQ(green, Pixel(scene.surface, 4, 2));
	CHECK_EQ(2, Lit(scene.surface));

	POINT point = {10, 5};
	scene.surface.Lock();
	CHECK(scene.renderer.DrawTile(1, &point));
	CHECK(!scene.renderer.DrawTile(1, nullptr));
	scene.surface.Unlock();
	CHECK_EQ(white, Pixel(scene.surface, 10, 5));
	CHECK_EQ(4, Lit(scene.surface));
}

TEST(TileRenderer, ZeroIsEmptyAndABadIdFails)
{
	Scene scene;
	scene.renderer.Init(&scene.surface, &scene.pack);
	CHECK(scene.Draw(0, 3, 2));
	CHECK(!scene.Draw(2, 3, 2));		// not loaded
	CHECK(!scene.Draw(3, 3, 2));		// past the pack
	CHECK(!scene.Draw(-2, 3, 2));
	CHECK_EQ(0, Lit(scene.surface));
}

TEST(TileRenderer, TheNullTileUsesItsOwnPack)
{
	Scene scene;
	scene.renderer.Init(&scene.surface, &scene.pack);
	CHECK(scene.Draw(SPRITEID_NULL, 3, 2));
	CHECK(scene.Draw(-1, 3, 2));
	CHECK_EQ(0, Lit(scene.surface));

	CSpritePack nullPack;
	nullPack.Init(3);
	LoadSprite(nullPack, 2);
	scene.renderer.SetNullTileSpritePack(&nullPack, 1);	// not loaded
	CHECK(scene.Draw(SPRITEID_NULL, 3, 2));
	CHECK_EQ(0, Lit(scene.surface));
	scene.renderer.SetNullTileSpritePack(&nullPack, 2);
	CHECK(scene.Draw(SPRITEID_NULL, 3, 2));
	CHECK(scene.Draw(-1, 20, 9));
	CHECK_EQ(white, Pixel(scene.surface, 3, 2));
	CHECK_EQ(white, Pixel(scene.surface, 20, 9));
	CHECK_EQ(4, Lit(scene.surface));
	scene.renderer.Release();
	CHECK(!scene.Draw(SPRITEID_NULL, 30, 20));
}

TEST(TileRenderer, TheViewportIsClampedToTheMapAndTilesAreSpaced)
{
	Scene scene;
	scene.renderer.Init(&scene.surface, &scene.pack);
	scene.renderer.SetTileDimensions(5, 3);
	CHECK_EQ(5, scene.renderer.GetTileX());
	CHECK_EQ(3, scene.renderer.GetTileY());
	// A 3x2 map; the viewport starts at column 1 and runs past both edges.
	Map map(3, 2, {1, 1, 1,
	               1, 0, 1});
	scene.renderer.DrawTiles(&map, 1, -4, 9, 9, 7, 4);
	// Column 1 at x 7, column 2 at x 12; row 0 at y 4, row 1 at y 7.
	CHECK_EQ(white, Pixel(scene.surface, 7, 4));
	CHECK_EQ(white, Pixel(scene.surface, 12, 4));
	CHECK_EQ(0, Pixel(scene.surface, 7, 7));		// tile 0
	CHECK_EQ(white, Pixel(scene.surface, 12, 7));
	CHECK_EQ(6, Lit(scene.surface));

	// The same without the renderer locking.
	scene.surface.FillSurface(0);
	scene.surface.Lock();
	scene.renderer.DrawTilesNoLock(&map, 1, -4, 9, 9, 7, 4);
	scene.surface.Unlock();
	CHECK_EQ(white, Pixel(scene.surface, 12, 7));
	CHECK_EQ(6, Lit(scene.surface));

	// A viewport left of the map starts at column 0.
	scene.surface.FillSurface(0);
	scene.renderer.DrawTiles(&map, -2, 0, 9, 1, 0, 0);
	CHECK_EQ(white, Pixel(scene.surface, 0, 0));
	CHECK_EQ(white, Pixel(scene.surface, 10, 0));
	CHECK_EQ(6, Lit(scene.surface));

	scene.renderer.DrawTiles(nullptr, 0, 0, 3, 2);
	scene.renderer.DrawTilesNoLock(nullptr, 0, 0, 3, 2);
	CHECK_EQ(6, Lit(scene.surface));
}

TEST(TileRenderer, ClearingFillsTheSurface)
{
	Scene scene;
	scene.renderer.Init(&scene.surface, &scene.pack);
	scene.renderer.ClearSurface(0x1234);
	CHECK_EQ(0x1234, Pixel(scene.surface, 0, 0));
	CHECK_EQ(0x1234, Pixel(scene.surface, 39, 29));
	scene.renderer.ClearSurface();
	CHECK_EQ(0, Lit(scene.surface));
}
