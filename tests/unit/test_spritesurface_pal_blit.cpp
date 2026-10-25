//----------------------------------------------------------------------
// test_spritesurface_pal_blit.cpp
//----------------------------------------------------------------------
//
// Tests for the SDL backend's palette sprite blits in
// Client/SpriteLib/CSpriteSurface_Adapter.cpp and the screen blend
// tables in Client/SpriteLib/CSpriteSurface_SDL.cpp.
//
// Two things went wrong on the effect path after the SDL port and both
// are pinned here:
//
//   - BltSpritePalEffect, the BLT_SCREEN effect blit, was an empty
//     stub, so every screen-blend skill effect drew nothing; and the
//     effect function table it selects from was all NULL, so even a
//     working blit would have plain-copied.
//   - BltAlphaSpritePal, the BLT_EFFECT blit, refused any sprite that
//     crossed a surface edge instead of clipping it.
//
// The member blits lock a backend surface, which a test binary does
// not have. They hand the locked pixels to the static *To functions,
// and those are what is driven here, against a small array with every
// pixel outside the expected footprint checked untouched.
//
// Three details of the fixtures matter, each because a first version
// of this file lacked it and passed anyway:
//
//   - The surface rows carry padding beyond the visible width, so a
//     blit that strode by the width rather than the pitch would fail.
//   - The blend tests draw over a grey, not black. Over black a screen
//     blend and a plain copy produce identical bytes, and so do an
//     opaque alpha and a copy.
//   - The clipping tests use a sprite with several segments and
//     transparent runs, so the clip variants' skip-into-a-run and
//     later-segment branches actually execute.
//
// Sprite container layout (CSpritePalBase::LoadFromFile):
//
//      DWORD  size      bytes of packed pixel data
//      WORD   width
//      WORD   height
//      BYTE   data[size]
//      WORD   index[height]   per-scanline byte lengths
//
// and each scanline is run length encoded as a segment count, then
// per segment a transparent run, a colour run, and the colour run's
// pixels: one palette index byte for CSpritePal, an (alpha, index)
// pair for CAlphaSpritePal.
//
//----------------------------------------------------------------------
#include "test_framework.h"
#include "CSpriteSurface.h"
#include "CSpritePal.h"
#include "CAlphaSpritePal.h"
#include "MPalette.h"
#include "ColorDraw.h"
#include <SDL_log.h>
#include <string>
#include <cstdio>
#include <fstream>
#include <vector>

namespace {

const char* const	kTempFile		= "spritesurface_pal_blit_test.bin";

// The plain fixture: every pixel opaque.
const int		kSpriteWidth	= 4;
const int		kSpriteHeight	= 2;

// The segmented fixture: three segments per row over seven columns,
// (2 transparent, 1 colour)(1 transparent, 2 colour)(0 transparent, 1 colour).
const int		kSegmentedWidth	= 7;
const bool		kSegmentedColumn[kSegmentedWidth] = { false, false, true, false, true, true, true };

// Alpha 32 makes the source replace the destination; 16 halves the way.
const BYTE		kOpaque		= 0x20;
const BYTE		kHalf		= 0x10;

// A colour whose three channels are distinct and mid-range, so a
// channel put in the wrong place or scaled wrongly shows up.
const BYTE		kRed	= 10;
const BYTE		kGreen	= 40;
const BYTE		kBlue	= 20;

void	PushWord(std::vector<unsigned char>& bytes, WORD value)
{
	bytes.push_back((unsigned char)(value & 0xFF));
	bytes.push_back((unsigned char)((value >> 8) & 0xFF));
}

void	PushDword(std::vector<unsigned char>& bytes, DWORD value)
{
	PushWord(bytes, (WORD)(value & 0xFFFF));
	PushWord(bytes, (WORD)((value >> 16) & 0xFFFF));
}

//----------------------------------------------------------------------
// Scanline builders. Every colour pixel is palette index 1.
//----------------------------------------------------------------------
void	PushSegment(std::vector<unsigned char>& scanline, int transparentRun, int colourRun, bool withAlpha, BYTE alpha)
{
	scanline.push_back((unsigned char)transparentRun);
	scanline.push_back((unsigned char)colourRun);
	for (int i = 0; i < colourRun; i++)
	{
		if (withAlpha)
			scanline.push_back(alpha);
		scanline.push_back(0x01);
	}
}

std::vector<unsigned char>	OpaqueScanline(bool withAlpha, BYTE alpha = kOpaque)
{
	std::vector<unsigned char>	scanline;
	scanline.push_back(1);
	PushSegment(scanline, 0, kSpriteWidth, withAlpha, alpha);
	return scanline;
}

std::vector<unsigned char>	SegmentedScanline(bool withAlpha)
{
	std::vector<unsigned char>	scanline;
	scanline.push_back(3);
	PushSegment(scanline, 2, 1, withAlpha, kOpaque);
	PushSegment(scanline, 1, 2, withAlpha, kOpaque);
	PushSegment(scanline, 0, 1, withAlpha, kOpaque);
	return scanline;
}

//----------------------------------------------------------------------
// Writes a sprite of kSpriteHeight rows, each the scanline given, in
// the container format, and loads it into the sprite handed in. The
// declared width is a parameter so a scanline wider than its sprite
// can be written to exercise the loader's rejection.
//----------------------------------------------------------------------
template <class Sprite>
bool	LoadSprite(Sprite& sprite, const std::vector<unsigned char>& scanline, int declaredWidth)
{
	std::vector<unsigned char>	bytes;

	PushDword(bytes, (DWORD)(scanline.size() * kSpriteHeight));
	PushWord(bytes, (WORD)declaredWidth);
	PushWord(bytes, (WORD)kSpriteHeight);

	for (int row = 0; row < kSpriteHeight; row++)
		bytes.insert(bytes.end(), scanline.begin(), scanline.end());

	for (int row = 0; row < kSpriteHeight; row++)
		PushWord(bytes, (WORD)scanline.size());

	{
		std::ofstream	out(kTempFile, std::ios::binary | std::ios::trunc);
		out.write((const char*)&bytes[0], (std::streamsize)bytes.size());
	}

	std::ifstream	in(kTempFile, std::ios::binary);
	bool		loaded = sprite.LoadFromFile(in);

	in.close();
	std::remove(kTempFile);

	return loaded;
}

template <class Sprite>
bool	LoadOpaqueSprite(Sprite& sprite, bool withAlpha, BYTE alpha = kOpaque)
{
	return LoadSprite(sprite, OpaqueScanline(withAlpha, alpha), kSpriteWidth);
}

template <class Sprite>
bool	LoadSegmentedSprite(Sprite& sprite, bool withAlpha)
{
	return LoadSprite(sprite, SegmentedScanline(withAlpha), kSegmentedWidth);
}

//----------------------------------------------------------------------
// A palette with the test colour at index 1.
//----------------------------------------------------------------------
void	FillPalette(MPalette& pal)
{
	pal.Init(2);
	pal[0] = 0;
	pal[1] = ColorDraw::Color(kRed, kGreen, kBlue);
}

WORD	TestColour()
{
	return ColorDraw::Color(kRed, kGreen, kBlue);
}

//----------------------------------------------------------------------
// A pixel buffer standing in for a locked surface. Each row is padded
// by kPadding words beyond the visible width, as an SDL surface's rows
// can be, so the pitch and the width differ; the padding is checked
// untouched like every other pixel outside a footprint.
//----------------------------------------------------------------------
const int	kPadding = 3;

class Surface
{
public:
	Surface(int width, int height)
		: m_Width(width), m_Height(height), m_Stride(width + kPadding),
		  m_Pixels((size_t)((width + kPadding) * height), 0) {}

	void	Fill(WORD value)	{ m_Pixels.assign(m_Pixels.size(), value); }

	WORD*	Pixels()		{ return &m_Pixels[0]; }
	int	Pitch() const		{ return m_Stride * (int)sizeof(WORD); }
	int	Width() const		{ return m_Width; }
	int	Height() const		{ return m_Height; }
	WORD	At(int x, int y) const	{ return m_Pixels[(size_t)(y * m_Stride + x)]; }

	//------------------------------------------------------------
	// Every pixel inside the rectangle holds `inside`, every pixel
	// outside it - padding included - holds `outside`.
	//------------------------------------------------------------
	bool	FootprintIs(int left, int top, int width, int height,
				WORD inside, WORD outside = 0) const
	{
		for (int y = 0; y < m_Height; y++)
		{
			for (int x = 0; x < m_Stride; x++)
			{
				bool	covered = (x >= left && x < left + width && y >= top && y < top + height);

				if (At(x, y) != (covered ? inside : outside))
					return false;
			}
		}
		return true;
	}

	bool	FootprintIs(int left, int top, int width, int height) const
	{
		return FootprintIs(left, top, width, height, TestColour(), 0);
	}

	bool	IsBlack() const	{ return FootprintIs(0, 0, 0, 0); }

	//------------------------------------------------------------
	// The segmented sprite placed with its origin at (originX,
	// originY): a pixel holds the colour exactly when it maps to one
	// of the sprite's coloured columns within its rows, and to a
	// column the surface can hold.
	//------------------------------------------------------------
	bool	SegmentedFootprintIs(int originX, int originY) const
	{
		for (int y = 0; y < m_Height; y++)
		{
			for (int x = 0; x < m_Stride; x++)
			{
				int	sx = x - originX;
				int	sy = y - originY;
				bool	covered = (x < m_Width)
						&& sx >= 0 && sx < kSegmentedWidth
						&& sy >= 0 && sy < kSpriteHeight
						&& kSegmentedColumn[sx];

				if (At(x, y) != (covered ? TestColour() : 0))
					return false;
			}
		}
		return true;
	}

private:
	int			m_Width;
	int			m_Height;
	int			m_Stride;
	std::vector<WORD>	m_Pixels;
};

void	ScreenBlit(Surface& surface, int x, int y, CSpritePal& sprite, MPalette& pal)
{
	POINT	point;
	point.x = x;
	point.y = y;
	CSpriteSurface::InitEffectTable();
	CSpriteSurface::SetPalEffect(CSpriteSurface::EFFECT_SCREEN);
	CSpriteSurface::BltSpritePalEffectTo(surface.Pixels(), surface.Pitch(),
										surface.Width(), surface.Height(),
										&point, &sprite, pal);
}

void	AlphaBlit(Surface& surface, int x, int y, CAlphaSpritePal& sprite, MPalette& pal)
{
	POINT	point;
	point.x = x;
	point.y = y;
	CSpriteSurface::BltAlphaSpritePalTo(surface.Pixels(), surface.Pitch(),
										surface.Width(), surface.Height(),
										&point, &sprite, pal);
}

} // namespace

//======================================================================
// ClipSpriteToSurface
//======================================================================

//----------------------------------------------------------------------
// A sprite that fits needs no clipping: the whole sprite rectangle,
// drawn at the point given. That includes one whose far edge lands
// exactly on the surface's, and one that covers the surface exactly.
//----------------------------------------------------------------------
TEST(ClipSpriteToSurface, WhollyVisibleSpriteIsNotClipped)
{
	RECT	rect;
	POINT	dest;

	CHECK_EQ((int)CSpriteSurface::SPRITE_CLIP_NONE,
		(int)CSpriteSurface::ClipSpriteToSurface(1, 1, 4, 2, 6, 4, &rect, &dest));
	CHECK_EQ(0, rect.left);
	CHECK_EQ(0, rect.top);
	CHECK_EQ(4, rect.right);
	CHECK_EQ(2, rect.bottom);
	CHECK_EQ(1, dest.x);
	CHECK_EQ(1, dest.y);

	CHECK_EQ((int)CSpriteSurface::SPRITE_CLIP_NONE,
		(int)CSpriteSurface::ClipSpriteToSurface(2, 2, 4, 2, 6, 4, &rect, &dest));
	CHECK_EQ(4, rect.right);
	CHECK_EQ(2, rect.bottom);

	CHECK_EQ((int)CSpriteSurface::SPRITE_CLIP_NONE,
		(int)CSpriteSurface::ClipSpriteToSurface(0, 0, 6, 4, 6, 4, &rect, &dest));
	CHECK_EQ(6, rect.right);
	CHECK_EQ(4, rect.bottom);
}

//----------------------------------------------------------------------
// Off the left edge: rect.left is how many source columns to skip and
// the destination is the surface's first column.
//----------------------------------------------------------------------
TEST(ClipSpriteToSurface, LeftOverhangSkipsSourceColumns)
{
	RECT	rect;
	POINT	dest;

	CHECK_EQ((int)CSpriteSurface::SPRITE_CLIP_LEFT,
		(int)CSpriteSurface::ClipSpriteToSurface(-3, 1, 4, 2, 6, 4, &rect, &dest));
	CHECK_EQ(3, rect.left);
	CHECK_EQ(4, rect.right);
	CHECK_EQ(0, dest.x);
	CHECK_EQ(1, dest.y);
}

//----------------------------------------------------------------------
// Off the right edge: rect.right is the visible width.
//----------------------------------------------------------------------
TEST(ClipSpriteToSurface, RightOverhangShortensTheRow)
{
	RECT	rect;
	POINT	dest;

	CHECK_EQ((int)CSpriteSurface::SPRITE_CLIP_RIGHT,
		(int)CSpriteSurface::ClipSpriteToSurface(4, 0, 4, 2, 6, 4, &rect, &dest));
	CHECK_EQ(0, rect.left);
	CHECK_EQ(2, rect.right);
	CHECK_EQ(4, dest.x);
}

//----------------------------------------------------------------------
// Wider than the surface: both edges cut.
//----------------------------------------------------------------------
TEST(ClipSpriteToSurface, SpriteWiderThanSurfaceIsCutOnBothSides)
{
	RECT	rect;
	POINT	dest;

	CHECK_EQ((int)CSpriteSurface::SPRITE_CLIP_WIDTH,
		(int)CSpriteSurface::ClipSpriteToSurface(-1, 0, 4, 2, 2, 4, &rect, &dest));
	CHECK_EQ(1, rect.left);
	CHECK_EQ(3, rect.right);
	CHECK_EQ(0, dest.x);
}

//----------------------------------------------------------------------
// Rows only: a sprite hanging off the top or the bottom keeps every
// column, so the row-only variant is chosen, and the destination row
// is the first visible one.
//----------------------------------------------------------------------
TEST(ClipSpriteToSurface, VerticalOverhangClipsRowsOnly)
{
	RECT	rect;
	POINT	dest;

	CHECK_EQ((int)CSpriteSurface::SPRITE_CLIP_HEIGHT,
		(int)CSpriteSurface::ClipSpriteToSurface(1, -1, 4, 2, 6, 4, &rect, &dest));
	CHECK_EQ(1, rect.top);
	CHECK_EQ(2, rect.bottom);
	CHECK_EQ(0, rect.left);
	CHECK_EQ(4, rect.right);
	CHECK_EQ(1, dest.x);
	CHECK_EQ(0, dest.y);

	CHECK_EQ((int)CSpriteSurface::SPRITE_CLIP_HEIGHT,
		(int)CSpriteSurface::ClipSpriteToSurface(1, 3, 4, 2, 6, 4, &rect, &dest));
	CHECK_EQ(0, rect.top);
	CHECK_EQ(1, rect.bottom);
	CHECK_EQ(3, dest.y);
}

//----------------------------------------------------------------------
// A corner: columns and rows both cut, reported as the column case,
// which walks only the visible rows anyway.
//----------------------------------------------------------------------
TEST(ClipSpriteToSurface, CornerOverhangClipsBothAxes)
{
	RECT	rect;
	POINT	dest;

	CHECK_EQ((int)CSpriteSurface::SPRITE_CLIP_LEFT,
		(int)CSpriteSurface::ClipSpriteToSurface(-1, -1, 4, 2, 6, 4, &rect, &dest));
	CHECK_EQ(1, rect.left);
	CHECK_EQ(1, rect.top);
	CHECK_EQ(4, rect.right);
	CHECK_EQ(2, rect.bottom);
	CHECK_EQ(0, dest.x);
	CHECK_EQ(0, dest.y);
}

//----------------------------------------------------------------------
// Nothing visible: past any edge, touching an edge from outside, or
// with no area at all.
//----------------------------------------------------------------------
TEST(ClipSpriteToSurface, InvisibleSpriteIsOutside)
{
	RECT	rect;
	POINT	dest;

	CHECK_EQ((int)CSpriteSurface::SPRITE_CLIP_OUTSIDE,
		(int)CSpriteSurface::ClipSpriteToSurface(6, 0, 4, 2, 6, 4, &rect, &dest));
	CHECK_EQ((int)CSpriteSurface::SPRITE_CLIP_OUTSIDE,
		(int)CSpriteSurface::ClipSpriteToSurface(0, 4, 4, 2, 6, 4, &rect, &dest));
	CHECK_EQ((int)CSpriteSurface::SPRITE_CLIP_OUTSIDE,
		(int)CSpriteSurface::ClipSpriteToSurface(-4, 0, 4, 2, 6, 4, &rect, &dest));
	CHECK_EQ((int)CSpriteSurface::SPRITE_CLIP_OUTSIDE,
		(int)CSpriteSurface::ClipSpriteToSurface(0, -2, 4, 2, 6, 4, &rect, &dest));
	CHECK_EQ((int)CSpriteSurface::SPRITE_CLIP_OUTSIDE,
		(int)CSpriteSurface::ClipSpriteToSurface(0, 0, 0, 2, 6, 4, &rect, &dest));
	CHECK_EQ((int)CSpriteSurface::SPRITE_CLIP_OUTSIDE,
		(int)CSpriteSurface::ClipSpriteToSurface(0, 0, 4, 2, 0, 0, &rect, &dest));
}

//======================================================================
// The screen blend
//======================================================================

//----------------------------------------------------------------------
// screen(d, s) = 1 - (1 - d)(1 - s): black leaves the source as it is,
// white stays white, and the operation is symmetric. Green is the
// 6-bit field, so its table has to answer for a value of 63.
//----------------------------------------------------------------------
TEST(ScreenBlend, TableFollowsTheScreenFormula)
{
	CSpriteSurface::InitEffectTable();

	CHECK_EQ((WORD)(17 << ColorDraw::s_bSHIFT_R), CSpriteSurface::s_EffectScreenTableR[0][17]);
	CHECK_EQ((WORD)(31 << ColorDraw::s_bSHIFT_R), CSpriteSurface::s_EffectScreenTableR[31][17]);
	CHECK_EQ(CSpriteSurface::s_EffectScreenTableR[9][23], CSpriteSurface::s_EffectScreenTableR[23][9]);

	CHECK_EQ((WORD)17, CSpriteSurface::s_EffectScreenTableB[0][17]);
	CHECK_EQ((WORD)31, CSpriteSurface::s_EffectScreenTableB[31][17]);

	CHECK_EQ((WORD)(63 << ColorDraw::s_bSHIFT_G), CSpriteSurface::s_EffectScreenTableG[0][63]);
	CHECK_EQ((WORD)(63 << ColorDraw::s_bSHIFT_G), CSpriteSurface::s_EffectScreenTableG[63][40]);
	CHECK_EQ(CSpriteSurface::s_EffectScreenTableG[12][50], CSpriteSurface::s_EffectScreenTableG[50][12]);

	// 16 over 16 with a maximum of 32: 16 + 16 * 16 / 32 = 24
	CHECK_EQ((WORD)(24 << ColorDraw::s_bSHIFT_R), CSpriteSurface::s_EffectScreenTableR[16][16]);
	// 32 over 32 with a maximum of 64: 32 + 32 * 32 / 64 = 48
	CHECK_EQ((WORD)(48 << ColorDraw::s_bSHIFT_G), CSpriteSurface::s_EffectScreenTableG[32][32]);
}

//----------------------------------------------------------------------
// EFFECT_SCREEN is registered, so selecting it blends rather than
// copying: over black the palette colour comes through exactly, over
// white the pixel stays white.
//----------------------------------------------------------------------
TEST(ScreenBlend, SelectingScreenBlendsThePixel)
{
	CSpriteSurface::InitEffectTable();
	CSpriteSurface::SetPalEffect(CSpriteSurface::EFFECT_SCREEN);

	MPalette	pal;
	FillPalette(pal);

	BYTE	source[2] = { 1, 1 };
	WORD	dest[2] = { 0, 0xFFFF };

	CSpriteSurface::memcpyPalEffect(dest, source, 2, pal);

	CHECK_EQ(TestColour(), dest[0]);
	CHECK_EQ((WORD)0xFFFF, dest[1]);
}

//======================================================================
// BltSpritePalEffectTo - the BLT_SCREEN effect path
//======================================================================

TEST(BltSpritePalEffectTo, DrawsTheWholeSpriteWhenItFits)
{
	CSpritePal	sprite;
	CHECK(LoadOpaqueSprite(sprite, false));

	MPalette	pal;
	FillPalette(pal);

	Surface		surface(6, 4);
	ScreenBlit(surface, 1, 1, sprite, pal);

	CHECK(surface.FootprintIs(1, 1, kSpriteWidth, kSpriteHeight));
}

//----------------------------------------------------------------------
// Over a black surface the screen blend and a plain copy produce the
// same bytes, so the footprint tests cannot tell them apart. This one
// draws over a grey and asserts the blended value: per channel,
// high + low * (max - high) / max, with red and blue out of 32 and
// green out of 64. A plain copy would leave the palette colour.
//----------------------------------------------------------------------
TEST(BltSpritePalEffectTo, BlendsOverANonBlackSurface)
{
	CSpritePal	sprite;
	CHECK(LoadOpaqueSprite(sprite, false));

	MPalette	pal;
	FillPalette(pal);

	const WORD	grey = ColorDraw::Color(16, 32, 16);

	// red:   16 + 10 * (32 - 16) / 32 = 21
	// green: 40 + 32 * (64 - 40) / 64 = 52
	// blue:  20 + 16 * (32 - 20) / 32 = 26
	const WORD	blended = ColorDraw::Color(21, 52, 26);

	Surface		surface(6, 4);
	surface.Fill(grey);
	ScreenBlit(surface, 1, 1, sprite, pal);

	CHECK(blended != TestColour());
	CHECK(surface.FootprintIs(1, 1, kSpriteWidth, kSpriteHeight, blended, grey));
}

//----------------------------------------------------------------------
// Off the top-left corner, only the overlapping cells are written,
// and the sprite's own top-left column and row are the ones skipped.
//----------------------------------------------------------------------
TEST(BltSpritePalEffectTo, ClipsAtTheTopLeftCorner)
{
	CSpritePal	sprite;
	CHECK(LoadOpaqueSprite(sprite, false));

	MPalette	pal;
	FillPalette(pal);

	Surface		surface(6, 4);
	ScreenBlit(surface, -1, -1, sprite, pal);

	CHECK(surface.FootprintIs(0, 0, kSpriteWidth - 1, kSpriteHeight - 1));
}

TEST(BltSpritePalEffectTo, ClipsAtTheBottomRightCorner)
{
	CSpritePal	sprite;
	CHECK(LoadOpaqueSprite(sprite, false));

	MPalette	pal;
	FillPalette(pal);

	Surface		surface(6, 4);
	ScreenBlit(surface, 4, 3, sprite, pal);

	CHECK(surface.FootprintIs(4, 3, 2, 1));
}

//----------------------------------------------------------------------
// Wider than the surface: the middle of the sprite is what shows.
//----------------------------------------------------------------------
TEST(BltSpritePalEffectTo, ClipsBothSidesOfAWideSprite)
{
	CSpritePal	sprite;
	CHECK(LoadOpaqueSprite(sprite, false));

	MPalette	pal;
	FillPalette(pal);

	Surface		surface(2, 4);
	ScreenBlit(surface, -1, 1, sprite, pal);

	CHECK(surface.FootprintIs(0, 1, 2, kSpriteHeight));
}

//----------------------------------------------------------------------
// Hanging off the top only: every column, the visible rows.
//----------------------------------------------------------------------
TEST(BltSpritePalEffectTo, ClipsRowsOnly)
{
	CSpritePal	sprite;
	CHECK(LoadOpaqueSprite(sprite, false));

	MPalette	pal;
	FillPalette(pal);

	Surface		surface(6, 4);
	ScreenBlit(surface, 1, -1, sprite, pal);

	CHECK(surface.FootprintIs(1, 0, kSpriteWidth, kSpriteHeight - 1));
}

//----------------------------------------------------------------------
// The segmented sprite under each kind of clipping. The placements
// are chosen so the cut lands inside a transparent run, inside a
// colour run, and at a segment boundary, and so that segments after
// the first are reached on both the left- and right-clipped walks.
//----------------------------------------------------------------------
TEST(BltSpritePalEffectTo, ClipsASegmentedSpriteCorrectly)
{
	CSpritePal	sprite;
	CHECK(LoadSegmentedSprite(sprite, false));

	MPalette	pal;
	FillPalette(pal);

	const int	placements[][3] =
	{
		//  x,  y, surface width
		{  0,  1, 9 },		// fits
		{ -1,  0, 9 },		// left cut inside the first transparent run
		{ -3,  0, 9 },		// left cut inside a colour run (column 2 gone)
		{ -5,  0, 9 },		// left cut at the last segment
		{  4,  1, 9 },		// right cut inside the middle colour run
		{  6,  1, 9 },		// right cut inside the first transparent run
		{ -1,  0, 3 },		// both sides cut
		{ -3, -1, 2 },		// both sides and the top cut
	};

	for (size_t i = 0; i < sizeof(placements) / sizeof(placements[0]); i++)
	{
		Surface	surface(placements[i][2], 4);
		ScreenBlit(surface, placements[i][0], placements[i][1], sprite, pal);
		CHECK(surface.SegmentedFootprintIs(placements[i][0], placements[i][1]));
	}
}

//----------------------------------------------------------------------
// Nothing drawn when the sprite is off the surface, never loaded, or
// refused by the loader. The refused case is the one worth having:
// sprite rejection is silent in this client and the return value is
// ignored, so a rejected sprite reaches the blit as an initialised,
// empty one.
//----------------------------------------------------------------------
TEST(BltSpritePalEffectTo, DrawsNothingOutsideUnloadedOrRejected)
{
	CSpritePal	sprite;
	CHECK(LoadOpaqueSprite(sprite, false));

	MPalette	pal;
	FillPalette(pal);

	Surface		surface(6, 4);
	ScreenBlit(surface, 6, 0, sprite, pal);
	ScreenBlit(surface, 0, 4, sprite, pal);
	ScreenBlit(surface, -4, 0, sprite, pal);
	CHECK(surface.IsBlack());

	CSpritePal	unloaded;
	ScreenBlit(surface, 1, 1, unloaded, pal);
	CHECK(surface.IsBlack());

	// A scanline wider than the sprite it claims to belong to.
	CSpritePal	rejected;
	CHECK_EQ(false, LoadSprite(rejected, OpaqueScanline(false), kSpriteWidth - 1));
	ScreenBlit(surface, 1, 1, rejected, pal);
	CHECK(surface.IsBlack());
}

//======================================================================
// BltAlphaSpritePalTo - the BLT_EFFECT path
//======================================================================

TEST(BltAlphaSpritePalTo, DrawsTheWholeSpriteWhenItFits)
{
	CAlphaSpritePal	sprite;
	CHECK(LoadOpaqueSprite(sprite, true));

	MPalette	pal;
	FillPalette(pal);

	Surface		surface(6, 4);
	AlphaBlit(surface, 1, 1, sprite, pal);

	CHECK(surface.FootprintIs(1, 1, kSpriteWidth, kSpriteHeight));
}

//----------------------------------------------------------------------
// An opaque alpha over black is indistinguishable from a copy. Half
// alpha over a darker grey is not: per channel, d + (s - d) * 16 / 32.
//----------------------------------------------------------------------
TEST(BltAlphaSpritePalTo, BlendsByAlphaOverANonBlackSurface)
{
	CAlphaSpritePal	sprite;
	CHECK(LoadOpaqueSprite(sprite, true, kHalf));

	MPalette	pal;
	FillPalette(pal);

	const WORD	grey = ColorDraw::Color(4, 20, 8);

	// red:   4  + (10 - 4)  * 16 / 32 = 7
	// green: 20 + (40 - 20) * 16 / 32 = 30
	// blue:  8  + (20 - 8)  * 16 / 32 = 14
	const WORD	blended = ColorDraw::Color(7, 30, 14);

	Surface		surface(6, 4);
	surface.Fill(grey);
	AlphaBlit(surface, 1, 1, sprite, pal);

	CHECK(blended != TestColour());
	CHECK(surface.FootprintIs(1, 1, kSpriteWidth, kSpriteHeight, blended, grey));
}

//----------------------------------------------------------------------
// The case the log showed: a sprite hanging off the top of the screen
// used to be skipped whole. Its visible rows draw now.
//----------------------------------------------------------------------
TEST(BltAlphaSpritePalTo, SpriteHangingOffTheTopDrawsItsVisibleRows)
{
	CAlphaSpritePal	sprite;
	CHECK(LoadOpaqueSprite(sprite, true));

	MPalette	pal;
	FillPalette(pal);

	Surface		surface(6, 4);
	AlphaBlit(surface, 1, -1, sprite, pal);

	CHECK(surface.FootprintIs(1, 0, kSpriteWidth, kSpriteHeight - 1));
}

TEST(BltAlphaSpritePalTo, ClipsAtTheTopLeftCorner)
{
	CAlphaSpritePal	sprite;
	CHECK(LoadOpaqueSprite(sprite, true));

	MPalette	pal;
	FillPalette(pal);

	Surface		surface(6, 4);
	AlphaBlit(surface, -1, -1, sprite, pal);

	CHECK(surface.FootprintIs(0, 0, kSpriteWidth - 1, kSpriteHeight - 1));
}

TEST(BltAlphaSpritePalTo, ClipsAtTheBottomRightCorner)
{
	CAlphaSpritePal	sprite;
	CHECK(LoadOpaqueSprite(sprite, true));

	MPalette	pal;
	FillPalette(pal);

	Surface		surface(6, 4);
	AlphaBlit(surface, 4, 3, sprite, pal);

	CHECK(surface.FootprintIs(4, 3, 2, 1));
}

TEST(BltAlphaSpritePalTo, ClipsBothSidesOfAWideSprite)
{
	CAlphaSpritePal	sprite;
	CHECK(LoadOpaqueSprite(sprite, true));

	MPalette	pal;
	FillPalette(pal);

	Surface		surface(2, 4);
	AlphaBlit(surface, -1, 1, sprite, pal);

	CHECK(surface.FootprintIs(0, 1, 2, kSpriteHeight));
}

TEST(BltAlphaSpritePalTo, ClipsASegmentedSpriteCorrectly)
{
	CAlphaSpritePal	sprite;
	CHECK(LoadSegmentedSprite(sprite, true));

	MPalette	pal;
	FillPalette(pal);

	const int	placements[][3] =
	{
		{  0,  1, 9 },
		{ -1,  0, 9 },
		{ -3,  0, 9 },
		{ -5,  0, 9 },
		{  4,  1, 9 },
		{  6,  1, 9 },
		{ -1,  0, 3 },
		{ -3, -1, 2 },
	};

	for (size_t i = 0; i < sizeof(placements) / sizeof(placements[0]); i++)
	{
		Surface	surface(placements[i][2], 4);
		AlphaBlit(surface, placements[i][0], placements[i][1], sprite, pal);
		CHECK(surface.SegmentedFootprintIs(placements[i][0], placements[i][1]));
	}
}

TEST(BltAlphaSpritePalTo, DrawsNothingOutsideUnloadedOrRejected)
{
	CAlphaSpritePal	sprite;
	CHECK(LoadOpaqueSprite(sprite, true));

	MPalette	pal;
	FillPalette(pal);

	Surface		surface(6, 4);
	AlphaBlit(surface, 6, 0, sprite, pal);
	AlphaBlit(surface, 0, 4, sprite, pal);
	AlphaBlit(surface, 0, -2, sprite, pal);
	CHECK(surface.IsBlack());

	CAlphaSpritePal	unloaded;
	AlphaBlit(surface, 1, 1, unloaded, pal);
	CHECK(surface.IsBlack());

	CAlphaSpritePal	rejected;
	CHECK_EQ(false, LoadSprite(rejected, OpaqueScanline(true), kSpriteWidth - 1));
	AlphaBlit(surface, 1, 1, rejected, pal);
	CHECK(surface.IsBlack());
}

#include "CSprite565.h"

namespace {
class ScrollClipSprite : public CSprite565 {
public:
    ScrollClipSprite() {
        m_Width = 5;
        m_Height = 4;
        m_Pixels = new WORD*[m_Height];
        for (int y = 0; y < m_Height; ++y)
            m_Pixels[y] = new WORD[7]{1, 1, 4, 0xf800, 0xf800, 0xf800, 0xf800};
        m_bInit = true;
    }
};
}

TEST(SpriteSurfaceClip, ScrolledSpritesRespectViewportAndReset)
{
    CHECK_EQ(0, spritectl_init());
    CSpriteSurface surface;
    CHECK(surface.Init(10, 8));
    if (!surface.GetBackendSurface()) return;
    ScrollClipSprite rle;
    WORD pixels[20];
    for (int i = 0; i < 20; ++i) pixels[i] = i % 5 == 0 ? 0 : 0xf800;
    auto decoded = spritectl_create_sprite(5, 4, SPRITECTL_FORMAT_RGB565, pixels, sizeof(pixels));
    CHECK(decoded != nullptr);
    if (!decoded) return;
    const POINT positions[] = {{0, 0}, {5, 4}, {9, 7}, {-2, 2}, {3, -2}, {3, 2}};
    const RECT clips[] = {{3, 2, 7, 6}, {-2, -2, 4, 4}, {30, 30, 35, 35}, {4, 4, 4, 4}};
    for (int mode = 0; mode < 3; ++mode) {
        for (RECT clip : clips) {
            for (POINT pos : positions) {
                surface.SetClipNULL();
                surface.FillSurface(0x1234);
                surface.SetClip(&clip);
                if (mode == 0)
                    surface.BltSprite(&pos, &rle);
                else if (mode == 1)
                    spritectl_blt_sprite(surface.GetBackendSurface(), pos.x, pos.y, decoded, 0, 255);
                else {
                    RECT fill = {pos.x + 1, pos.y, pos.x + 5, pos.y + 4};
                    surface.FillRect(&fill, 0xf800);
                }
                DWORD pitch = 0;
                const BYTE* data = static_cast<const BYTE*>(surface.Lock(nullptr, &pitch));
                CHECK(data != nullptr);
                if (data) {
                    for (int y = 0; y < 8; ++y) {
                        const WORD* row = reinterpret_cast<const WORD*>(data + y * pitch);
                        for (int x = 0; x < 10; ++x) {
                            const bool visible = x >= clip.left && x < clip.right && y >= clip.top && y < clip.bottom
                                && x >= pos.x + 1 && x < pos.x + 5 && y >= pos.y && y < pos.y + 4;
                            CHECK_EQ(visible ? 0xf800 : 0x1234, row[x]);
                        }
                    }
                    surface.Unlock();
                }
            }
        }
    }
    // Closing the skill viewport must restore drawing in the surrounding UI.
    surface.SetClipNULL();
    surface.FillSurface(0x1234);
    POINT origin = {0, 0};
    surface.BltSprite(&origin, &rle);
    const WORD* row = static_cast<const WORD*>(surface.Lock(nullptr));
    CHECK(row != nullptr);
    if (row) {
        CHECK_EQ(0x1234, row[0]);
        CHECK_EQ(0xf800, row[1]);
        surface.Unlock();
    }
    spritectl_destroy_sprite(decoded);
}

#include "CIndexSprite.h"

namespace {
class SkillStateSprite : public CSprite565 {
public:
    SkillStateSprite() {
        m_Width = 8;
        m_Height = 2;
        m_Pixels = new WORD*[m_Height];
        for (int y = 0; y < m_Height; ++y)
            m_Pixels[y] = new WORD[11]{2, 1, 3, 0xffff, 0xf800, 0x07e0, 1, 3, 0x001f, 0x8410, 0};
        m_bInit = true;
    }
};
}

TEST(SpriteSurfaceEffects, SkillAndRankStatesRespectTransparencyAndViewport)
{
    CHECK_EQ(0, spritectl_init());
    CIndexSprite::SetColorSet();
    CSpriteSurface surface;
    CHECK(surface.Init(11, 7)); // Odd width exercises padded SDL rows.
    if (!surface.GetBackendSurface()) return;
    RECT fullSurface = {0, 0, 11, 7};
    SkillStateSprite sprite;
    const auto savedEffect = CSpriteSurface::s_pMemcpyEffectFunction;
    const int savedValue = CSpriteSurface::s_Value1;
    CSpriteSurface::SetEffect(CSpriteSurface::EFFECT_GRAY_SCALE);
    const WORD expected[6][8] = {
        {0, 0xf800, 0xf800, 0, 0, 0, 0x8000, 0}, // red channel
        {0, 0x07e0, 0, 0x07e0, 0, 0, 0x0400, 0}, // green / learnable
        {0, 0x001f, 0, 0, 0, 0x001f, 0x0010, 0}, // blue channel
        {0, 0x7bef, 0x7800, 0x03e0, 0, 0x000f, 0x4208, 0}, // unavailable rank choice
        {0, 0xffff, 0x528a, 0x528a, 0, 0x528a, 0x8430, 0}, // locked skill
        {0, CIndexSprite::ColorSet[59][CIndexSprite::ColorToGradation[93]],
            CIndexSprite::ColorSet[59][CIndexSprite::ColorToGradation[31]],
            CIndexSprite::ColorSet[59][CIndexSprite::ColorToGradation[31]], 0,
            CIndexSprite::ColorSet[59][CIndexSprite::ColorToGradation[31]],
            CIndexSprite::ColorSet[59][CIndexSprite::ColorToGradation[48]],
            CIndexSprite::ColorSet[59][MAX_COLORGRADATION - 1]} // learned passive / rank
    };
    const POINT positions[] = {{1, 2}, {-2, 2}, {7, 2}, {1, -1}, {1, 6}, {30, 30}};
    const RECT clips[] = {{0, 0, 11, 7}, {3, 2, 7, 4}, {4, 3, 4, 3}, {-5, -5, 5, 5}};
    for (int mode = 0; mode < 6; ++mode) {
        for (RECT clip : clips) {
            for (POINT pos : positions) {
                surface.SetClipNULL();
                surface.FillRect(&fullSurface, 0x1234);
                surface.SetClip(&clip);
                if (mode < 3) surface.BltSpriteColor(&pos, &sprite, static_cast<BYTE>(mode));
                else if (mode == 3) surface.BltSpriteDarkness(&pos, &sprite, 1);
                else if (mode == 4) surface.BltSpriteEffect(&pos, &sprite);
                else surface.BltSpriteColorSet(&pos, &sprite, 59);
                DWORD pitch = 0;
                const BYTE* data = static_cast<const BYTE*>(surface.Lock(nullptr, &pitch));
                CHECK(data != nullptr);
                if (data) {
                    for (int y = 0; y < 7; ++y) {
                        const WORD* row = reinterpret_cast<const WORD*>(data + y * pitch);
                        for (int x = 0; x < 11; ++x) {
                            const int sx = x - pos.x;
                            const bool visible = x >= clip.left && x < clip.right && y >= clip.top && y < clip.bottom
                                && y >= pos.y && y < pos.y + 2 && sx >= 0 && sx < 8 && sx != 0 && sx != 4;
                            CHECK_EQ(visible ? expected[mode][sx] : 0x1234, row[x]);
                        }
                    }
                    surface.Unlock();
                }
            }
        }
    }
    // Drawing an effect must not recolor the cached sprite or later learned icons.
    surface.SetClipNULL();
    surface.FillRect(&fullSurface, 0x1234);
    POINT origin = {0, 0};
    surface.BltSprite(&origin, &sprite);
    const WORD* row = static_cast<const WORD*>(surface.Lock(nullptr));
    CHECK(row != nullptr);
    if (row) {
        CHECK_EQ(0xffff, row[1]);
        CHECK_EQ(0xf800, row[2]);
        CHECK_EQ(0x07e0, row[3]);
        CHECK_EQ(0x1234, row[4]);
        CHECK_EQ(0x001f, row[5]);
        surface.Unlock();
    }
    CSpriteSurface::s_pMemcpyEffectFunction = savedEffect;
    CSpriteSurface::s_Value1 = savedValue;
}

namespace {
struct EffectLogCapture {
    SDL_LogOutputFunction previous = nullptr;
    void* context = nullptr;
    SDL_LogPriority priority = SDL_LogGetPriority(SDL_LOG_CATEGORY_RENDER);
    FUNCTION_MEMCPYEFFECT pixel = CSpriteSurface::s_pMemcpyEffectFunction;
    FUNCTION_MEMCPYPALEFFECT palette = CSpriteSurface::s_pMemcpyPalEffectFunction;
    std::vector<std::string> messages;

    static void SDLCALL Record(void* data, int category, SDL_LogPriority level, const char* text)
    {
        if (category == SDL_LOG_CATEGORY_RENDER && level == SDL_LOG_PRIORITY_WARN)
            static_cast<EffectLogCapture*>(data)->messages.emplace_back(text);
    }
    EffectLogCapture()
    {
        SDL_LogGetOutputFunction(&previous, &context);
        SDL_LogSetPriority(SDL_LOG_CATEGORY_RENDER, SDL_LOG_PRIORITY_WARN);
        SDL_LogSetOutputFunction(Record, this);
    }
    ~EffectLogCapture()
    {
        SDL_LogSetOutputFunction(previous, context);
        SDL_LogSetPriority(SDL_LOG_CATEGORY_RENDER, priority);
        CSpriteSurface::s_pMemcpyEffectFunction = pixel;
        CSpriteSurface::s_pMemcpyPalEffectFunction = palette;
    }
};
}

TEST(SpriteSurfaceEffects, UnsupportedSelectionsReportOnceAndKeepTheirCopyFallback)
{
    // An effect no code selects (each kind is reported once a process, and
    // the creature drawing selects the wipe-out).
    EffectLogCapture log;
    for (int repeat = 0; repeat < 100; ++repeat) {
        CSpriteSurface::SetEffect(CSpriteSurface::EFFECT_DIFFERENT);
        CSpriteSurface::SetPalEffect(CSpriteSurface::EFFECT_DIFFERENT);
    }
    CHECK_EQ(2u, log.messages.size());
    if (log.messages.size() == 2) {
        CHECK(log.messages[0].find("pixel") != std::string::npos);
        CHECK(log.messages[1].find("palette") != std::string::npos);
        for (const auto& message : log.messages) {
            CHECK(message.find("effect 7;") != std::string::npos);
            CHECK(message.find("plain copy") != std::string::npos);
        }
    }
    WORD source[] = {0xf800, 0x07e0, 0x001f};
    WORD dest[] = {0, 0, 0};
    CSpriteSurface::memcpyEffect(dest, source, 3);
    for (unsigned i = 0; i < 3; ++i) CHECK_EQ(source[i], dest[i]);
    MPalette pal;
    pal.Init(3);
    for (BYTE i = 0; i < 3; ++i) pal[i] = source[i];
    BYTE indices[] = {2, 0, 1};
    CSpriteSurface::memcpyPalEffect(dest, indices, 3, pal);
    for (unsigned i = 0; i < 3; ++i) CHECK_EQ(source[indices[i]], dest[i]);
    CSpriteSurface::SetEffect(CSpriteSurface::EFFECT_GRAY_SCALE);
    CHECK(CSpriteSurface::s_pMemcpyEffectFunction != nullptr);
    CSpriteSurface::SetEffect(CSpriteSurface::EFFECT_GRADATION);
    CHECK(CSpriteSurface::s_pMemcpyEffectFunction != nullptr);
    CSpriteSurface::SetPalEffect(CSpriteSurface::EFFECT_SCREEN);
    CHECK(CSpriteSurface::s_pMemcpyPalEffectFunction != nullptr);
    CHECK_EQ(2u, log.messages.size());
}

TEST(SpriteSurfaceEffects, SentinelSelectionCannotIndexPastEitherFunctionTable)
{
    EffectLogCapture log;
    for (int repeat = 0; repeat < 3; ++repeat) {
        CSpriteSurface::SetEffect(CSpriteSurface::EFFECT_GRAY_SCALE);
        CSpriteSurface::SetEffect(CSpriteSurface::MAX_EFFECT);
        CHECK(CSpriteSurface::s_pMemcpyEffectFunction == nullptr);
        CSpriteSurface::SetPalEffect(CSpriteSurface::EFFECT_SCREEN);
        CSpriteSurface::SetPalEffect(CSpriteSurface::MAX_EFFECT);
        CHECK(CSpriteSurface::s_pMemcpyPalEffectFunction == nullptr);
    }
    CHECK_EQ(2u, log.messages.size());
}
