//----------------------------------------------------------------------
// test_indexsprite_colorset.cpp
//----------------------------------------------------------------------
//
// Regression coverage for the generated RGB 5:6:5 colour-set table.
// The shipped item-option table assigns colour set 377 to items without
// an option. That set is generated from the light-grey seed, so every
// gradation must remain neutral: red and blue match, while green carries
// twice their value because its RGB565 field has six bits instead of five.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "CIndexSprite.h"
#include "ColorDraw.h"

#include <cstdlib>

namespace {

struct ColorSeed
{
	int	red;
	int	green;
	int	blue;
};

const ColorSeed kRgb565Seeds[] =
{
	{ 16, 32, 16 },
	{ 24, 48, 24 },
	{ 8, 16, 8 },
	{ 30, 48, 18 },
	{ 25, 30, 11 },
	{ 21, 24, 11 },
	{ 19, 30, 13 },
	{ 21, 36, 11 },
	{ 22, 32, 9 }
};

} // namespace

TEST(CIndexSpriteColorSet, DefaultItemColorSetIsNeutralGrey)
{
	const int defaultItemColorSet = 377;

	CIndexSprite::SetColorSet();

	for (int gradation = 0; gradation < MAX_COLORGRADATION; gradation++)
	{
		const WORD color = CIndexSprite::ColorSet[defaultItemColorSet][gradation];
		const int red = ColorDraw::Red(color);
		const int green = ColorDraw::Green(color);
		const int blue = ColorDraw::Blue(color);

		CHECK_EQ(red, blue);
		CHECK(std::abs(green - red * 2) <= 1);
	}
}

//----------------------------------------------------------------------
// These seeds came from a 5:5:5 table. RGB565 doubles their green
// component; pinning the generated seed colour prevents a source-data
// restoration from silently copying the old numeric values back again.
//----------------------------------------------------------------------
TEST(CIndexSpriteColorSet, LegacySeedsAreScaledForRgb565)
{
	const int firstSeed = 24;

	CIndexSprite::SetColorSet();

	for (int index = 0; index < (int)(sizeof(kRgb565Seeds) / sizeof(kRgb565Seeds[0])); index++)
	{
		const int colorSet = (firstSeed + index) * MAX_COLORSET_SEED_MODIFY;
		const WORD seedColor = CIndexSprite::ColorSet[colorSet][MAX_COLORGRADATION_HALF];
		const ColorSeed& expected = kRgb565Seeds[index];

		CHECK_EQ(expected.red, ColorDraw::Red(seedColor));
		CHECK_EQ(expected.green, ColorDraw::Green(seedColor));
		CHECK_EQ(expected.blue, ColorDraw::Blue(seedColor));
	}
}


#include "CIndexSprite565.h"
#include "CSpriteSurface.h"

namespace {
class PaletteTestSprite : public CIndexSprite565 {
public:
    PaletteTestSprite() {
        // Transparent pixel, four independently recolorable pixels, fixed pixel.
        m_Width = 6;
        m_Height = 1;
        m_Pixels = new WORD*[1];
        m_Pixels[0] = new WORD[9]{1, 1, 4, 10, 0x10a, 0x20a, 0xff0a, 1, 0xf81f};
        m_bInit = true;
    }
};
}

TEST(CIndexSpriteColorSet, CachedBlitsFollowEachPaletteChannel)
{
    CHECK_EQ(0, spritectl_init());
    CIndexSprite::SetColorSet();
    CSpriteSurface surface;
    CHECK(surface.Init(6, 1));
    if (!surface.GetBackendSurface()) return;
    PaletteTestSprite sprite;
    POINT origin = {0, 0};
    const BYTE channels[] = {0, 1, 2, 255};
    int saved[4];
    int chosen[4] = {30, 60, 90, 120};
    for (int i = 0; i < 4; ++i) {
        saved[i] = CIndexSprite::GetUsingColorSet(channels[i]);
        CIndexSprite::SetUsingColorSetOnly(channels[i], chosen[i]);
    }
    // Reuse the same decoded sprite, changing skin/hair and higher channels,
    // then switch back as when two characters share one animation frame.
    for (int pass = 0; pass < 9; ++pass) {
        if (pass > 0) {
            const int i = (pass - 1) % 4;
            chosen[i] += pass <= 4 ? 150 : -150;
            CIndexSprite::SetUsingColorSetOnly(channels[i], chosen[i]);
        }
        surface.FillSurface(0x1234);
        surface.BltIndexSprite(&origin, &sprite);
        DWORD pitch = 0;
        const WORD* pixels = static_cast<const WORD*>(surface.Lock(nullptr, &pitch));
        CHECK(pixels != nullptr);
        if (pixels) {
            CHECK_EQ(0x1234, pixels[0]);
            for (int i = 0; i < 4; ++i)
                CHECK_EQ(CIndexSprite::ColorSet[chosen[i]][10], pixels[i + 1]);
            CHECK_EQ(0xf81f, pixels[5]);
            surface.Unlock();
        }
    }
    for (int i = 0; i < 4; ++i)
        CIndexSprite::SetUsingColorSetOnly(channels[i], saved[i]);
}

// A colour set is a row of ColorSet, which every indexed pixel reads, and
// the server sends some: a pet item's main colour, a character's skin and
// hair (the player's info, the character list). One the table lacks is
// drawn with the first row, not read past the table's end.
TEST(CIndexSpriteColorSet, ASetPastTheTableIsDrawnWithTheFirst)
{
	int saved[3];
	for (int i = 0; i < 3; ++i)
		saved[i] = CIndexSprite::GetUsingColorSet(BYTE(i));
	CIndexSprite::SetUsingColorSet(MAX_COLORSET - 1, MAX_COLORSET);
	CHECK_EQ(MAX_COLORSET - 1, CIndexSprite::GetUsingColorSet(0));
	CHECK_EQ(0, CIndexSprite::GetUsingColorSet(1));
	CIndexSprite::SetUsingColorSet(-1, 0xFFFF);
	CHECK_EQ(0, CIndexSprite::GetUsingColorSet(0));
	CHECK_EQ(0, CIndexSprite::GetUsingColorSet(1));
	CIndexSprite::SetUsingColorSetOnly(2, 0xFFFE);
	CHECK_EQ(0, CIndexSprite::GetUsingColorSet(2));
	CIndexSprite::SetUsingColorSetOnly(2, 377);
	CHECK_EQ(377, CIndexSprite::GetUsingColorSet(2));
	for (int i = 0; i < 3; ++i)
		CIndexSprite::SetUsingColorSetOnly(BYTE(i), saved[i]);
}
