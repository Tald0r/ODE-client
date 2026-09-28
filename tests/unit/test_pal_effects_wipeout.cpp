//----------------------------------------------------------------------
// test_pal_effects_wipeout.cpp
//----------------------------------------------------------------------
//
// CSpriteSurface::memcpyPalEffectWipeOut draws a run of palette-index
// bytes with its centre left out: s_Value1/64 of the run is skipped and
// the rest is split between the two ends. The source holds one byte
// per pixel, so each drawn pixel is the palette colour of its index,
// and no more than `pixels` source bytes are read.
//
// The source is heap-allocated at exactly the run length so that a
// sanitizer build reports a read past its end.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "CSpriteSurface.h"
#include "MPalette.h"

#include <memory>

namespace {

constexpr WORD kSentinel = 0xABCD;

// Saves CSpriteSurface::s_Value1 and restores it on exit.
struct Value1Guard
{
	int saved = CSpriteSurface::s_Value1;
	explicit Value1Guard(int value)	{ CSpriteSurface::s_Value1 = value; }
	~Value1Guard()					{ CSpriteSurface::s_Value1 = saved; }
};

void InitPalette(MPalette& pal)
{
	pal.Init(4);
	pal[0] = 0x1111;
	pal[1] = 0x2222;
	pal[2] = 0x3333;
	pal[3] = 0x4444;
}

}

TEST(PalEffectWipeOut, NoSkipDrawsEveryPixelThroughThePalette)
{
	MPalette pal;
	InitPalette(pal);
	std::unique_ptr<BYTE[]> source(new BYTE[4]);
	for (BYTE i = 0; i < 4; ++i) source[i] = i;
	WORD dest[6];
	for (WORD& pixel : dest) pixel = kSentinel;

	Value1Guard value1(0);
	CSpriteSurface::memcpyPalEffectWipeOut(dest, source.get(), 4, pal);

	CHECK_EQ(0x1111, dest[0]);
	CHECK_EQ(0x2222, dest[1]);
	CHECK_EQ(0x3333, dest[2]);
	CHECK_EQ(0x4444, dest[3]);
	CHECK_EQ(kSentinel, dest[4]);
	CHECK_EQ(kSentinel, dest[5]);
}

// s_Value1 32 skips half of a 4-pixel run: one pixel drawn at each end.
TEST(PalEffectWipeOut, HalfSkipLeavesTheCentreUntouched)
{
	MPalette pal;
	InitPalette(pal);
	std::unique_ptr<BYTE[]> source(new BYTE[4]);
	for (BYTE i = 0; i < 4; ++i) source[i] = i;
	WORD dest[6];
	for (WORD& pixel : dest) pixel = kSentinel;

	Value1Guard value1(32);
	CSpriteSurface::memcpyPalEffectWipeOut(dest, source.get(), 4, pal);

	CHECK_EQ(0x1111, dest[0]);
	CHECK_EQ(kSentinel, dest[1]);
	CHECK_EQ(kSentinel, dest[2]);
	CHECK_EQ(0x4444, dest[3]);
	CHECK_EQ(kSentinel, dest[4]);
	CHECK_EQ(kSentinel, dest[5]);
}
