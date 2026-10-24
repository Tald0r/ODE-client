//-----------------------------------------------------------------------------
// AlphaBox.cpp
//-----------------------------------------------------------------------------
// The UI's translucent box (task 4.236: ClientFunction.cpp's until then,
// where it drew on the UI's back surface within g_GameRect).
//-----------------------------------------------------------------------------
#include "AlphaBox.h"
#include "CSpriteSurface.h"

void
DrawAlphaBox(CSpriteSurface& surface, const RECT& screen, RECT box, WORD color, BYTE alpha)
{
	// Clip to the screen
	RECT* pRect = &box;

	if(pRect->left < 0)pRect->left = 0;
	if(pRect->right > screen.right-1)pRect->right = screen.right-1;
	if(pRect->top < 0)pRect->top = 0;
	if(pRect->bottom > screen.bottom -1)pRect->bottom = screen.bottom-1;

	if(pRect->left >= pRect->right || pRect->top >= pRect->bottom)return;

		int reverseAlpha = 31-alpha;

		// Black and coloured boxes take the SDL surface API's GPU path on every
		// platform. Borrowing framebuffer pixels here forces a WebGL readback
		// for each translucent UI panel and delays the browser's audio callback.
		if (color == 0)
		{
			surface.GammaBox565(pRect, reverseAlpha);
			return;
		}

		// A coloured box (a black one returned above).
		surface.BltColorAlpha(pRect, color, reverseAlpha);
}
