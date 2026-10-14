#include "test_framework.h"
#include "MObject.h"

extern bool g_bInterpolateDraw;
extern int g_DrawCamGapX;
extern int g_DrawCamGapY;

// An object's screen rectangle: set as given, or grown to take in another
// rectangle. While the world draw interpolates, both are moved back by the
// object's own draw gap and the camera's, so picking works in exact tick
// space.
namespace {

struct Gapped : MObject
{
	int gapX = 0, gapY = 0;
	int GetDrawGapX() const override { return gapX; }
	int GetDrawGapY() const override { return gapY; }
};

struct Interpolation
{
	bool oldOn = g_bInterpolateDraw;
	int oldX = g_DrawCamGapX, oldY = g_DrawCamGapY;
	Interpolation(bool on, int camX, int camY)
	{
		g_bInterpolateDraw = on;
		g_DrawCamGapX = camX;
		g_DrawCamGapY = camY;
	}
	~Interpolation()
	{
		g_bInterpolateDraw = oldOn;
		g_DrawCamGapX = oldX;
		g_DrawCamGapY = oldY;
	}
};

bool Is(const RECT& rect, int left, int top, int right, int bottom)
{
	return rect.left == left && rect.top == top && rect.right == right && rect.bottom == bottom;
}

} // namespace

TEST(ObjectScreenRect, ARectIsSetAsGivenWhenNotInterpolating)
{
	Interpolation off(false, 7, 9);
	Gapped object;
	object.gapX = 3;
	object.gapY = 4;
	RECT rect = {10, 20, 30, 40};
	object.SetScreenRect(&rect);
	CHECK(Is(object.GetScreenRect(), 10, 20, 30, 40));
	CHECK(object.IsPointInScreenRect(15, 25));
}

TEST(ObjectScreenRect, ARectIsMovedBackByBothGapsWhileInterpolating)
{
	Interpolation on(true, 7, 9);
	Gapped object;
	object.gapX = 3;
	object.gapY = 4;
	RECT rect = {10, 20, 30, 40};
	object.SetScreenRect(&rect);
	// -3-7 across, -4-9 down.
	CHECK(Is(object.GetScreenRect(), 0, 7, 20, 27));
	// The caller's rect is left alone.
	CHECK(Is(rect, 10, 20, 30, 40));
}

TEST(ObjectScreenRect, AnAddedRectGrowsTheRectOnEachSide)
{
	// Gaps that would move it, were it interpolating.
	Interpolation off(false, 7, 9);
	Gapped object;
	object.gapX = 3;
	object.gapY = 4;
	RECT first = {10, 20, 30, 40};
	object.SetScreenRect(&first);
	RECT inside = {12, 22, 28, 38};
	object.AddScreenRect(&inside);
	CHECK(Is(object.GetScreenRect(), 10, 20, 30, 40));
	RECT left = {5, 25, 15, 35};
	object.AddScreenRect(&left);
	RECT up = {15, 15, 25, 25};
	object.AddScreenRect(&up);
	RECT right = {25, 25, 35, 35};
	object.AddScreenRect(&right);
	RECT down = {15, 35, 25, 45};
	object.AddScreenRect(&down);
	CHECK(Is(object.GetScreenRect(), 5, 15, 35, 45));
}

TEST(ObjectScreenRect, AnAddedRectIsMovedBackWhileInterpolating)
{
	Gapped object;
	object.gapX = 1;
	object.gapY = 2;
	{
		Interpolation off(false, 0, 0);
		RECT first = {10, 20, 30, 40};
		object.SetScreenRect(&first);
	}
	Interpolation on(true, 10, 20);
	// Moved back by 11 and 22: (39, 58)-(51, 72) grows the right and bottom.
	RECT added = {50, 80, 62, 94};
	object.AddScreenRect(&added);
	CHECK(Is(object.GetScreenRect(), 10, 20, 51, 72));
	CHECK(Is(added, 50, 80, 62, 94));
}
