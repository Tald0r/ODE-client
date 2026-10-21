//----------------------------------------------------------------------
// test_present_geometry.cpp
//----------------------------------------------------------------------
//
// spritectl_window_to_game_coords (Client/SpriteLib/SpriteLibBackendSDL.cpp)
// on a high-DPI display. The letterbox rectangle a present records is in
// renderer output pixels; a mouse event is in window points; on a Retina
// Mac with SDL_WINDOW_ALLOW_HIGHDPI the output is twice the window, and
// a mapping that read the point as a pixel landed every click at half
// its distance from the top-left corner (docs/build/linux-macos-port-
// assessment-2026-09-07.md, area G). No machine that runs these tests
// has such a display and SDL's dummy driver cannot fake one, so the
// geometry is set by hand through spritectl_set_present_geometry, with
// the rectangle a present computes for that window - its 16.16
// fixed-point scale rounds the height down by one on the letterboxed
// fixtures, and the fixtures say so rather than rounding it back up.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "SpriteLibBackend.h"

namespace {

// (x, y) in window points through the mapping.
struct Point { int x; int y; };

Point Map(int x, int y)
{
	spritectl_window_to_game_coords(&x, &y);
	return { x, y };
}

} // namespace

TEST(PresentGeometry, RetinaWindowMapsPointsThroughThePixelRatio)
{
	// 1024x768 game frame in a 1024x768-point window at 2x: the output is
	// 2048x1536, the scale is exactly 2.0 and the frame fills it. A point
	// is two pixels, and a game pixel is two output pixels, so the mapping
	// is the identity - the old code, which skipped the first step, halved
	// everything.
	spritectl_set_present_geometry(1024, 768, 2048, 1536, 0, 0, 2048, 1536, 1024, 768);
	Point p = Map(512, 384);
	CHECK_EQ(512, p.x);
	CHECK_EQ(384, p.y);
	p = Map(1023, 767);
	CHECK_EQ(1023, p.x);
	CHECK_EQ(767, p.y);
	p = Map(0, 0);
	CHECK_EQ(0, p.x);
	CHECK_EQ(0, p.y);
}

TEST(PresentGeometry, RetinaLetterboxLandsInTheFrameNotTheBar)
{
	// 800x600 frame in a 1600x1000-point window at 2x: output 3200x2000,
	// scale (2000 << 16) / 600 = 218453, so the frame is 2666x1999 at
	// x = 267, y = 0 - the present's own numbers. The window's centre
	// (800, 500) is the frame's centre (400, 300); a point in the left
	// bar clamps to column 0; the window's far corner is the frame's
	// last pixel.
	spritectl_set_present_geometry(1600, 1000, 3200, 2000, 267, 0, 2666, 1999, 800, 600);
	Point p = Map(800, 500);
	CHECK_EQ(400, p.x);
	CHECK_EQ(300, p.y);
	p = Map(50, 500);
	CHECK_EQ(0, p.x);
	p = Map(1599, 999);
	CHECK_EQ(799, p.x);
	CHECK_EQ(599, p.y);
}

TEST(PresentGeometry, NonRetinaWindowIsUnchangedByTheRatio)
{
	// The same letterbox on an ordinary display: window and output agree,
	// scale (1000 << 16) / 600 = 109226, frame 1333x999 at x = 133, and
	// the mapping is what it was before the ratio step existed.
	spritectl_set_present_geometry(1600, 1000, 1600, 1000, 133, 0, 1333, 999, 800, 600);
	Point p = Map(133 + 666, 500);
	CHECK_EQ(399, p.x);
	CHECK_EQ(300, p.y);
	p = Map(0, 0);
	CHECK_EQ(0, p.x);
	CHECK_EQ(0, p.y);
}

TEST(PresentGeometry, NoWindowSizeMeansOutputSize)
{
	// A renderer without a window (the software renderer the frame
	// upscaler tests present to: a 16x16 frame into 80x64, which the
	// present letterboxes to 64x64 at x = 8) records no window size; the
	// ratio step is skipped rather than dividing by zero.
	spritectl_set_present_geometry(0, 0, 80, 64, 8, 0, 64, 64, 16, 16);
	Point p = Map(40, 32);
	CHECK_EQ(8, p.x);
	CHECK_EQ(8, p.y);
	p = Map(8, 0);
	CHECK_EQ(0, p.x);
	CHECK_EQ(0, p.y);
}

TEST(PresentGeometry, AWindowWithoutAnOutputSizeIsNotScaledToZero)
{
	// The present only records a window size when it has an output size,
	// but the mapping guards it as well: a window size beside an output
	// size of zero must not collapse every point onto the frame's origin.
	spritectl_set_present_geometry(1600, 1000, 0, 0, 133, 0, 1333, 999, 800, 600);
	Point p = Map(133 + 666, 500);
	CHECK_EQ(399, p.x);
	CHECK_EQ(300, p.y);
}
