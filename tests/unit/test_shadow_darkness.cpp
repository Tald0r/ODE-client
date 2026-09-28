//----------------------------------------------------------------------
// test_shadow_darkness.cpp
//----------------------------------------------------------------------
//
// CShadowSprite::BltDarkness darkens each shadow run in place through
// memcpyShadowDarkness, which splits a run into a leading pixel, a
// leading pixel pair and then groups of four. Every pixel of the run
// must be darkened exactly once, whatever the run length is mod 4.
//
// The masks come from ColorDraw's shift tables; the test sets the
// 5:6:5 halving masks for a shift of 1 and restores the tables after.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "CShadowSprite.h"
#include "ColorDraw.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

// Sets the shift-by-one masks and restores the previous ones on exit.
struct HalvingMasks
{
	WORD		w = ColorDraw::s_wMASK_SHIFT[1];
	DWORD		dw = ColorDraw::s_dwMASK_SHIFT[1];
	uint64_t	qw = ColorDraw::s_qwMASK_SHIFT[1];

	HalvingMasks()
	{
		ColorDraw::s_wMASK_SHIFT[1] = 0x7BEF;
		ColorDraw::s_dwMASK_SHIFT[1] = 0x7BEF7BEF;
		ColorDraw::s_qwMASK_SHIFT[1] = 0x7BEF7BEF7BEF7BEFULL;
	}

	~HalvingMasks()
	{
		ColorDraw::s_wMASK_SHIFT[1] = w;
		ColorDraw::s_dwMASK_SHIFT[1] = dw;
		ColorDraw::s_qwMASK_SHIFT[1] = qw;
	}
};

// A shadow sprite file written from little-endian WORDs, removed on exit.
struct ShadowFile
{
	std::string path;

	explicit ShadowFile(const std::vector<WORD>& words)
	{
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		path = (std::filesystem::temp_directory_path()
			/ ("darkeden_shadow_darkness_" + std::to_string(stamp) + ".bin")).string();
		std::vector<char> bytes;
		for (WORD word : words)
		{
			bytes.push_back(static_cast<char>(word & 0xFF));
			bytes.push_back(static_cast<char>(word >> 8));
		}
		std::ofstream out(path.c_str(), std::ios::out | std::ios::binary);
		out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	}

	~ShadowFile()
	{
		std::remove(path.c_str());
	}
};

}

// Width 7, height 1, one row of 3 WORDs: one segment, no transparent
// pixels, then a 7-pixel shadow run. 7 is 3 mod 4, so the run takes
// the single pixel, the pixel pair and one group of four.
TEST(ShadowDarkness, RunOfThreeModFourDarkensEveryPixelOnce)
{
	ShadowFile file({7, 1, 3, 1, 0, 7});
	CShadowSprite sprite;
	{
		std::ifstream in(file.path.c_str(), std::ios::in | std::ios::binary);
		CHECK(sprite.LoadFromFile(in));
	}
	CHECK(sprite.IsInit());
	if (!sprite.IsInit()) return;

	HalvingMasks masks;
	alignas(8) WORD buffer[8];
	for (WORD& pixel : buffer) pixel = 0xFFFF;

	sprite.BltDarkness(buffer, static_cast<WORD>(sizeof buffer), 1);

	for (int i = 0; i < 7; ++i)
		CHECK_EQ(0x7BEF, buffer[i]);
	CHECK_EQ(0xFFFF, buffer[7]);
}
