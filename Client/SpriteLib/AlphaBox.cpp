//-----------------------------------------------------------------------------
// AlphaBox.cpp
//-----------------------------------------------------------------------------
// The UI's translucent box (task 4.236: ClientFunction.cpp's until then,
// where it drew on the UI's back surface within g_GameRect).
//-----------------------------------------------------------------------------
#include "AlphaBox.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackend.h"

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

		// The SDL surface API has the same GPU gamma path on every platform.
		// Borrowing framebuffer pixels here forces a WebGL readback for each
		// translucent UI panel and delays the browser's audio callback.
		if (color == 0)
		{
			surface.GammaBox565(pRect, reverseAlpha);
			return;
		}

#ifdef PLATFORM_WINDOWS
		//------------------------------------------------
		// Lock it
		//------------------------------------------------
		BOOL bUnlock = !surface.IsLock();
		if (bUnlock)
		{
			surface.Lock();
		}

		// A coloured box (a black one returned above).
		surface.BltColorAlpha(pRect, color, reverseAlpha);

		//------------------------------------------------
		// Back to the lock state it had
		//------------------------------------------------
		if (bUnlock)
		{
			surface.Unlock();
		}
#else
		// SDL backend implementation (every platform but Windows)
		// Need to use spritectl API directly since Lock() is a stub in SDL backend

		// Get the SDL surface
		spritectl_surface_t sdl_surface = (spritectl_surface_t)surface.GetBackendSurface();
		if (!sdl_surface || sdl_surface == SPRITECTL_INVALID_SURFACE) {
			return;  // Invalid surface
		}

		// Lock the surface to get pixel access
		spritectl_surface_info_t surface_info;
		if (spritectl_lock_surface(sdl_surface, &surface_info) != 0) {
			return;  // Failed to lock
		}

		// Calculate clipping
		int startX = (pRect->left > 0) ? pRect->left : 0;
		int endX = (pRect->right < surface_info.width) ? pRect->right : surface_info.width;
		int startY = (pRect->top > 0) ? pRect->top : 0;
		int endY = (pRect->bottom < surface_info.height) ? pRect->bottom : surface_info.height;

		// A coloured box (a black one returned above): alpha blending
		BYTE r565 = (color >> 11) & 0x1F;  // 5-bit red
		BYTE g565 = (color >> 5) & 0x3F;   // 6-bit green
		BYTE b565 = color & 0x1F;          // 5-bit blue

		// Convert alpha from 5-bit (0-31) to 8-bit (0-255)
		BYTE alpha8 = (31 - reverseAlpha) * 255 / 31;

		// Draw alpha-blended rectangle
		for (int y = startY; y < endY; y++)
		{
			WORD* pDest = (WORD*)((BYTE*)surface_info.pixels + y * surface_info.pitch) + startX;
			for (int x = startX; x < endX; x++)
			{
				// Get destination pixel
				WORD destPixel = *pDest;
				BYTE destR = (destPixel >> 11) & 0x1F;
				BYTE destG = (destPixel >> 5) & 0x3F;
				BYTE destB = destPixel & 0x1F;

				// Alpha blend: src * alpha + dest * (1 - alpha)
				BYTE srcR = r565;
				BYTE srcG = g565;
				BYTE srcB = b565;

				BYTE resultR = (srcR * alpha8 + destR * (255 - alpha8)) / 255;
				BYTE resultG = (srcG * alpha8 + destG * (255 - alpha8)) / 255;
				BYTE resultB = (srcB * alpha8 + destB * (255 - alpha8)) / 255;

				// Clamp to valid range
				if (resultR > 31) resultR = 31;
				if (resultG > 63) resultG = 63;
				if (resultB > 31) resultB = 31;

				// Write back
				*pDest++ = (resultR << 11) | (resultG << 5) | resultB;
			}
		}

		// Unlock the surface
		spritectl_unlock_surface(sdl_surface);
#endif
}
