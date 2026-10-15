#include "test_framework.h"
#include "Fl2.h"
#include "FontHandleUtil.h"
#include "SpriteLib/CSpriteSurface.h"
#include "SpriteLib/SpriteLibBackend.h"
#include <algorithm>
#include <cstring>
#include <string>

// The FL2 text layer (task 4.228, VS_UI's since then). It prints on the
// FL2 surface the UI sets or, before that, on the surface source the game
// installs; aligns by the text's measured width; prints a shadow 1 pixel
// down-right (by default a quarter of the colour) or an outline in the
// four directions; and converts the hangul editor's DBCS strings.
namespace {

struct Box { int left = 1 << 30, top = 1 << 30, right = -1, bottom = -1; bool Empty() const { return right < 0; } };

struct Canvas
{
	CSpriteSurface surface;
	Canvas() { spritectl_init(); surface.Init(160, 40); surface.FillSurface(0); }
	WORD At(int x, int y)
	{
		spritectl_surface_info_t info{};
		if (spritectl_lock_surface(surface.GetBackendSurface(), &info) != 0) return 0;
		const WORD value = reinterpret_cast<const WORD*>(static_cast<const unsigned char*>(info.pixels) + y * info.pitch)[x];
		spritectl_unlock_surface(surface.GetBackendSurface());
		return value;
	}
	// The box of the pixels lit, or lit only within these channels (the
	// text is anti-aliased, so a colour shows as its channels).
	Box Lit(WORD channels = 0xFFFF)
	{
		Box box;
		for (int y = 0; y < 40; ++y)
			for (int x = 0; x < 160; ++x)
			{
				const WORD value = At(x, y);
				if (value == 0 || (value & ~channels) != 0) continue;
				box.left = std::min(box.left, x); box.right = std::max(box.right, x);
				box.top = std::min(box.top, y); box.bottom = std::max(box.bottom, y);
			}
		return box;
	}
	void Clear() { surface.FillSurface(0); }
};

const WORD Red = 0xF800, Blue = 0x001F;

Canvas* fallback = nullptr;
CSpriteSurface* Fallback() { return fallback != nullptr ? &fallback->surface : nullptr; }

struct Scene
{
	Canvas canvas;
	PrintInfo info{};
	Scene() { info.text_color = RGB(255, 255, 255); g_SetFL2Surface(&canvas.surface); }
	~Scene() { g_SetFL2Surface(nullptr); g_SetFL2FallbackSurface(nullptr); fallback = nullptr; }
};

} // namespace

TEST(FL2Rendering, TextGoesOnTheFL2SurfaceOrElseTheGamesSurface)
{
	Scene scene;
	Canvas other;
	fallback = &other;
	g_SetFL2FallbackSurface(&Fallback);
	g_Print(10, 10, "Hi", &scene.info);
	CHECK(!scene.canvas.Lit().Empty());
	CHECK(other.Lit().Empty());
	scene.canvas.Clear();
	// Before the UI sets its surface: the game's.
	g_SetFL2Surface(nullptr);
	g_Print(10, 10, "Hi", &scene.info);
	CHECK(!other.Lit().Empty());
	CHECK(scene.canvas.Lit().Empty());
	// Neither: nothing.
	other.Clear();
	g_SetFL2FallbackSurface(nullptr);
	g_Print(10, 10, "Hi", &scene.info);
	CHECK(other.Lit().Empty());
	fallback = nullptr;
	g_SetFL2FallbackSurface(&Fallback);		// a source with no surface
	g_Print(10, 10, "Hi", &scene.info);
	CHECK(other.Lit().Empty());
}

TEST(FL2Rendering, NothingPrintsWithoutText)
{
	Scene scene;
	g_Print(10, 10, nullptr, &scene.info);
	g_Print(10, 10, "", &scene.info);
	CHECK(scene.canvas.Lit().Empty());
	// Without print info: the default style.
	g_Print(10, 10, "Hi");
	CHECK(!scene.canvas.Lit().Empty());
}

TEST(FL2Rendering, ItPrintsInItsColourAlignedByItsWidth)
{
	Scene scene;
	scene.info.text_color = RGB(248, 0, 0);
	g_Print(60, 10, "HH", &scene.info);
	const Box left = scene.canvas.Lit();
	CHECK_EQ(left.left, scene.canvas.Lit(Red).left);	// red only
	CHECK_EQ(left.right, scene.canvas.Lit(Red).right);
	const int width = g_GetStringWidth("HH", scene.info.hfont);
	CHECK(width > 0);
	// A font size of its own.
	scene.canvas.Clear();
	scene.info.hfont = TextSystem::EncodeFontSizeHandle(32);
	g_Print(60, 4, "HH", &scene.info);
	const Box big = scene.canvas.Lit();
	CHECK(big.bottom - big.top > left.bottom - left.top + 4);
	scene.info.hfont = nullptr;
	scene.canvas.Clear();
	scene.info.text_align = TA_CENTER;
	g_Print(60, 10, "HH", &scene.info);
	CHECK_EQ(left.left - width / 2, scene.canvas.Lit().left);
	// Right-aligned text ends at x. TA_CENTER (6) holds TA_RIGHT's bit
	// (2), so the centre test must not take right for centre.
	scene.canvas.Clear();
	scene.info.text_align = TA_RIGHT;
	g_Print(60, 10, "HH", &scene.info);
	CHECK_EQ(left.left - width, scene.canvas.Lit().left);
}

TEST(FL2Rendering, AColouredStringReturnsWhereItEnds)
{
	Scene scene;
	const int width = g_GetStringWidth("HH", scene.info.hfont);
	CHECK_EQ(10 + width, g_PrintColorStr(10, 10, "HH", scene.info, RGB(0, 0, 248)));
	const Box all = scene.canvas.Lit();
	CHECK(!all.Empty());
	CHECK_EQ(all.right, scene.canvas.Lit(Blue).right);		// blue, not the info's white
	CHECK_EQ(RGB(255, 255, 255), scene.info.text_color);	// left as it was
}

TEST(FL2Rendering, AShadowFallsOnePixelDownRight)
{
	Scene scene;
	g_Print(20, 10, "HH", &scene.info);
	const Box plain = scene.canvas.Lit();
	scene.canvas.Clear();
	const int width = g_GetStringWidth("HH", scene.info.hfont);
	CHECK_EQ(20 + width, g_PrintColorStrShadow(20, 10, "HH", scene.info, RGB(248, 0, 0), RGB(0, 0, 248)));
	const Box shadowed = scene.canvas.Lit();
	CHECK_EQ(plain.left, shadowed.left);
	CHECK_EQ(plain.top, shadowed.top);
	CHECK_EQ(plain.right + 1, shadowed.right);
	CHECK_EQ(plain.bottom + 1, shadowed.bottom);
	// The shadow in its colour below, the text in its own on top.
	const Box shadow = scene.canvas.Lit(Blue);
	CHECK_EQ(plain.bottom + 1, shadow.bottom);
	CHECK_EQ(plain.right + 1, shadow.right);
	CHECK_EQ(plain.left, scene.canvas.Lit(Red).left);
	CHECK_EQ(plain.top, scene.canvas.Lit(Red).top);
	// The default shadow: the colour at a quarter. The text's bottom row
	// and the shadow's (one lower) have the same coverage, so the
	// shadow's brightest is a quarter of the text's.
	auto brightestRed = [&](int row) {
		int brightest = 0;
		for (int x = 0; x < 160; ++x)
			brightest = std::max(brightest, scene.canvas.At(x, row) >> 11);
		return brightest;
	};
	// At 24 points the bottom row of "HH" is fully covered.
	scene.info.hfont = TextSystem::EncodeFontSizeHandle(24);
	scene.canvas.Clear();
	g_PrintColorStr(20, 4, "HH", scene.info, RGB(248, 248, 248));
	const int bottom = scene.canvas.Lit().bottom;
	const int text = brightestRed(bottom);
	scene.canvas.Clear();
	g_PrintColorStrShadow(20, 4, "HH", scene.info, RGB(248, 248, 248));
	const int quarter = brightestRed(bottom + 1);
	CHECK(text >= 28);
	CHECK(quarter * 4 >= text - 4);
	CHECK(quarter * 4 <= text + 4);
	// Of the colour's own channels: red text casts a red shadow.
	// (COLORREF keeps red in its low byte.)
	scene.canvas.Clear();
	g_PrintColorStrShadow(20, 4, "HH", scene.info, RGB(248, 0, 0));
	bool shaded = false;
	for (int x = 0; x < 160; ++x)
	{
		const WORD value = scene.canvas.At(x, bottom + 1);
		CHECK_EQ(0, value & ~Red);
		shaded = shaded || value != 0;
	}
	CHECK(shaded);
}

TEST(FL2Rendering, AnOutlineSurroundsTheText)
{
	Scene scene;
	g_Print(20, 10, "HH", &scene.info);
	const Box plain = scene.canvas.Lit();
	scene.canvas.Clear();
	const int width = g_GetStringWidth("HH", scene.info.hfont);
	CHECK_EQ(20 + width, g_PrintColorStrOut(20, 10, "HH", scene.info, RGB(248, 0, 0), RGB(0, 0, 248)));
	const Box outline = scene.canvas.Lit(Blue);
	CHECK_EQ(plain.left - 1, outline.left);
	CHECK_EQ(plain.right + 1, outline.right);
	CHECK_EQ(plain.top - 1, outline.top);
	CHECK_EQ(plain.bottom + 1, outline.bottom);
	CHECK(!scene.canvas.Lit(Red).Empty());					// the text on top
}

TEST(FL2Rendering, Measurements)
{
	Scene scene;
	CHECK_EQ(0, g_GetStringWidth(nullptr));
	CHECK_EQ(0, g_GetStringWidth(""));
	const int one = g_GetStringWidth("H");
	CHECK(one > 0);
	CHECK(g_GetStringWidth("HH") > one);
	// A bigger font is wider and taller.
	CHECK(g_GetStringWidth("H", TextSystem::EncodeFontSizeHandle(32)) > one);
	const int height = g_GetStringHeight("H");
	CHECK(height > 0);
	CHECK(g_GetStringHeight("H", TextSystem::EncodeFontSizeHandle(32)) > height);
	CHECK(g_GetStringHeight(nullptr) > 0);
}

TEST(FL2Rendering, OnlyTheOuterDCIsTaken)
{
	CHECK(g_FL2_GetDC());
	CHECK(!g_FL2_GetDC());
	CHECK(g_FL2_ReleaseDC());
	CHECK(!g_FL2_ReleaseDC());
	CHECK(g_FL2_GetDC());
	CHECK(g_FL2_ReleaseDC());
}

TEST(FL2Rendering, DBCSStrings)
{
	// A wide character first, so each one sits at an even byte offset.
	const char_t dbcs[] = {char_t(0xA1B0), char_t('b'), char_t(0xA3B2), 0};
	CHECK_EQ(0, g_DBCSLen(nullptr));
	CHECK_EQ(3, g_DBCSLen(dbcs));
	CHECK_EQ(0, g_GetByteLenth(nullptr, 3));
	CHECK_EQ(0, g_GetByteLenth(dbcs, 0));
	CHECK_EQ(5, g_GetByteLenth(dbcs, 3));		// a wide character is 2 bytes
	CHECK_EQ(3, g_GetByteLenth(dbcs, 2));
	CHECK_EQ(1, g_GetByteLenth(dbcs + 1, 1));

	char* single = nullptr;
	CHECK_EQ(0, g_Convert_DBCS_Ascii2SingleByte(nullptr, 3, single));
	CHECK_EQ(0, g_Convert_DBCS_Ascii2SingleByte(dbcs, 0, single));
	CHECK(single == nullptr);
	CHECK_EQ(2, g_Convert_DBCS_Ascii2SingleByte(dbcs, 1, single));
	CHECK(single != nullptr && std::memcmp(single, "\xB0\xA1", 3) == 0);
	delete[] single;
	single = nullptr;
	CHECK_EQ(3, g_Convert_DBCS_Ascii2SingleByte(dbcs, 2, single));
	CHECK(single != nullptr && std::memcmp(single, "\xB0\xA1" "b", 4) == 0);

	char_t* wide = nullptr;
	CHECK_EQ(0, g_ConvertAscii2DBCS(nullptr, 4, wide));
	CHECK_EQ(0, g_ConvertAscii2DBCS(single, 0, wide));
	CHECK(wide == nullptr);
	CHECK_EQ(2, g_ConvertAscii2DBCS(single, 3, wide));
	CHECK(wide != nullptr && wide[0] == char_t(0xA1B0) && wide[1] == 'b' && wide[2] == 0);
	delete[] single;
	delete[] wide;
}

TEST(FL2Rendering, NumbersAndMoney)
{
	CHECK(g_GetNumberString(1234) == "1234");
	CHECK(g_GetNumberString(-7) == "-7");
	CHECK(g_GetStringByMoney(0) == "0");
	CHECK(g_GetStringByMoney(999) == "999");
	CHECK(g_GetStringByMoney(1000) == "1,000");
	CHECK(g_GetStringByMoney(1234567) == "1,234,567");
	CHECK(g_GetStringByMoney(4294967295u) == "4,294,967,295");
}
