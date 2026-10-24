#include "test_framework.h"
#include "AlphaBox.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackendSDL.h"

// The UI's translucent box (task 4.236, SpriteLib's since then): clipped
// to the screen one short of its right and bottom edges; a black box
// darkens what is under it, a coloured one blends over it.
namespace {

WORD Pixel(CSpriteSurface& surface, int x, int y)
{
	spritectl_surface_info_t info{};
	if (spritectl_lock_surface(surface.GetBackendSurface(), &info) != 0) return 0xDEAD;
	const WORD value = reinterpret_cast<const WORD*>(static_cast<const unsigned char*>(info.pixels) + y * info.pitch)[x];
	spritectl_unlock_surface(surface.GetBackendSurface());
	return value;
}

WORD RGB565(int r, int g, int b) { return WORD((r << 11) | (g << 5) | b); }

struct Surface
{
	CSpriteSurface surface;
	Surface(int width, int height, WORD fill) { spritectl_init(); surface.Init(width, height); surface.FillSurface(fill); }
};

const RECT kScreen = {0, 0, 8, 6};

} // namespace

TEST(AlphaBox, ABlackBoxDarkensByTheAlpha)
{
	Surface target(8, 6, RGB565(31, 63, 31));
	DrawAlphaBox(target.surface, kScreen, RECT{1, 1, 3, 3}, 0, 16);
	// Each channel times 31 - alpha, over 32.
	const WORD darkened = RGB565(31 * 15 >> 5, 63 * 15 >> 5, 31 * 15 >> 5);
	CHECK_EQ(darkened, Pixel(target.surface, 1, 1));
	CHECK_EQ(darkened, Pixel(target.surface, 2, 2));
	CHECK_EQ(RGB565(31, 63, 31), Pixel(target.surface, 3, 3));
	CHECK_EQ(RGB565(31, 63, 31), Pixel(target.surface, 0, 0));
	// Alpha 31 blacks it out.
	DrawAlphaBox(target.surface, kScreen, RECT{4, 4, 5, 5}, 0, 31);
	CHECK_EQ(0, Pixel(target.surface, 4, 4));
}

TEST(AlphaBox, TheBoxStaysInsideTheScreen)
{
	Surface target(8, 6, RGB565(31, 63, 31));
	// Past every edge: clipped to 0..6 across and 0..4 down (one short of
	// the screen's right and bottom).
	DrawAlphaBox(target.surface, kScreen, RECT{-3, -3, 20, 20}, 0, 31);
	CHECK_EQ(0, Pixel(target.surface, 0, 0));
	CHECK_EQ(0, Pixel(target.surface, 6, 4));
	CHECK_EQ(RGB565(31, 63, 31), Pixel(target.surface, 7, 0));
	CHECK_EQ(RGB565(31, 63, 31), Pixel(target.surface, 0, 5));
	// A box left with no width or height draws nothing.
	Surface other(8, 6, RGB565(31, 63, 31));
	DrawAlphaBox(other.surface, kScreen, RECT{7, 0, 9, 3}, 0, 31);
	DrawAlphaBox(other.surface, kScreen, RECT{0, 5, 3, 9}, 0, 31);
	DrawAlphaBox(other.surface, kScreen, RECT{3, 2, 3, 4}, 0, 31);
	for (int y = 0; y < 6; ++y)
		for (int x = 0; x < 8; ++x)
			CHECK_EQ(RGB565(31, 63, 31), Pixel(other.surface, x, y));
}

TEST(AlphaBox, AColouredBoxBlendsOverWhatIsUnder)
{
	Surface target(8, 6, RGB565(0, 0, 0));
	DrawAlphaBox(target.surface, kScreen, RECT{0, 0, 2, 2}, RGB565(31, 63, 31), 31);
	DrawAlphaBox(target.surface, kScreen, RECT{2, 0, 4, 2}, RGB565(31, 63, 31), 0);
	DrawAlphaBox(target.surface, kScreen, RECT{4, 0, 6, 2}, RGB565(30, 60, 20), 16);
	// Every platform blends through BltColorAlpha, the DirectDraw blend: per
	// channel colour * alpha/32 + under * (31 - alpha)/32. Windows drew
	// nothing (a stub) and the others an 8-bit approximation.
	CHECK_EQ(RGB565(30, 61, 30), Pixel(target.surface, 0, 0));
	CHECK_EQ(RGB565(30, 61, 30), Pixel(target.surface, 1, 1));
	CHECK_EQ(0, Pixel(target.surface, 2, 0));
	CHECK_EQ(RGB565(15, 30, 10), Pixel(target.surface, 4, 0));
	// Over a colour, both count.
	Surface over(8, 6, RGB565(10, 20, 30));
	DrawAlphaBox(over.surface, kScreen, RECT{0, 0, 1, 1}, RGB565(30, 60, 0), 16);
	CHECK_EQ(RGB565(15 + 4, 30 + 9, 0 + 14), Pixel(over.surface, 0, 0));
	CHECK_EQ(0, Pixel(target.surface, 6, 0));
	CHECK_EQ(0, Pixel(target.surface, 0, 2));
}

TEST(AlphaBox, ASurfaceWithoutPixelsIsLeftAlone)
{
	CSpriteSurface empty;
	DrawAlphaBox(empty, kScreen, RECT{0, 0, 4, 4}, RGB565(31, 0, 0), 16);
	DrawAlphaBox(empty, kScreen, RECT{0, 0, 4, 4}, 0, 16);
	CHECK(empty.GetBackendSurface() == nullptr || empty.GetBackendSurface() == SPRITECTL_INVALID_SURFACE);
}
