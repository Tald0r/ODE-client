//----------------------------------------------------------------------
// MObjectScreen.cpp - MObject's screen rectangles
//----------------------------------------------------------------------
// Split from MObject.cpp when the object base moved into gamemodel
// (docs/reviews/RESTRUCTURING.md task 4.4), because these two members read the
// draw interpolation state the game update kept. Since task 4.204 that
// state (g_bInterpolateDraw, the camera gaps) is defined here, in
// gamemodel, and the game update (CGameUpdate.cpp) sets it.
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MObject.h"

//----------------------------------------------------------------------
// Screen rects in exact tick space
//----------------------------------------------------------------------
// The rects are computed by the draw path, which since the 60 fps change
// renders positions interpolated between logic ticks against a camera
// carrying the player's interpolation bias (g_DrawCamGapX/Y). The per-tick
// input code hit-tests these rects (IsPointInScreenRect) against per-tick
// state, and a pick that misses because rect and logic disagree drops the
// held-button attack repeat. Shifting the rect by minus the object's own
// gap and minus the camera gap converts it back to exact tick space -
// identical to pre-interpolation behaviour. For the player both gaps cancel
// to zero, matching the pinned on-screen position. Outside the draw phase
// (g_bInterpolateDraw false) both gaps read as zero and this is a no-op.
//----------------------------------------------------------------------
// The world draw interpolates positions between ticks; the game loop
// (CGameUpdate.cpp) turns g_bInterpolateDraw on around it, so all game
// logic keeps reading exact tick positions.
bool	g_bInterpolateDraw = false;

// The camera-interpolation bias passed into the last MTopView::Draw (the
// camGap values in UpdateDraw). Picking code (GetSelectedObject and
// friends, creature screen rects) adds it back so that everything the
// per-tick input logic hit-tests lives in exact tick space, matching
// pre-interpolation behaviour - only the *drawing* is interpolated.
// Without this, a tick whose pick lands between the interpolated and
// exact positions misses the target, and one missed pick drops the
// held-button attack repeat (ProcessInputRButtonDown: pObject == NULL ->
// UnSetRepeatAction).
int		g_DrawCamGapX = 0;
int		g_DrawCamGapY = 0;

//----------------------------------------------------------------------
// Add ScreenRect
//----------------------------------------------------------------------
// Grows the object's on-screen hit rectangle by a sprite's rectangle.
//----------------------------------------------------------------------
void
MObject::AddScreenRect(RECT* pRect)
{
	RECT rect = *pRect;

	if (g_bInterpolateDraw)
	{
		int shiftX = -GetDrawGapX() - g_DrawCamGapX;
		int shiftY = -GetDrawGapY() - g_DrawCamGapY;
		rect.left += shiftX;	rect.right += shiftX;
		rect.top += shiftY;		rect.bottom += shiftY;
	}

	if (rect.left < m_ScreenRect.left)		m_ScreenRect.left	= rect.left;
	if (rect.top < m_ScreenRect.top)		m_ScreenRect.top	= rect.top;
	if (rect.right > m_ScreenRect.right)	m_ScreenRect.right	= rect.right;
	if (rect.bottom > m_ScreenRect.bottom)	m_ScreenRect.bottom	= rect.bottom;
}

//----------------------------------------------------------------------
// Set ScreenRect
//----------------------------------------------------------------------
void
MObject::SetScreenRect(RECT* pRect)
{
	m_ScreenRect = *pRect;

	if (g_bInterpolateDraw)
	{
		int shiftX = -GetDrawGapX() - g_DrawCamGapX;
		int shiftY = -GetDrawGapY() - g_DrawCamGapY;
		m_ScreenRect.left += shiftX;	m_ScreenRect.right += shiftX;
		m_ScreenRect.top += shiftY;		m_ScreenRect.bottom += shiftY;
	}
}

