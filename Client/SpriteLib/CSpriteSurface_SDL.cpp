/*-----------------------------------------------------------------------------

	CSpriteSurface_SDL.cpp

	SDL2 backend implementation for CSpriteSurface.
	This file contains all CSpriteSurface methods implemented using SpriteLibBackend.
	Does not depend on CDirectDraw or any Windows-specific code.

	2025.01.14

-----------------------------------------------------------------------------*/

#ifdef SPRITELIB_BACKEND_SDL

#include "Client_PCH.h"
#include "CSprite.h"
#include "CAlphaSprite.h"
#include "CIndexSprite.h"
#include "CShadowSprite.h"
#include "CSpriteOutlineManager.h"
#include "CFilter.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackend.h"
#include "SpriteLibBackendSDL.h"
#include "SpriteGpu.h"
#include <algorithm>
#include <cstdint>
#include <vector>

namespace {
// Internal drawing holds SDL pixels until its last read/write. The public
// DirectDraw compatibility lock remains an idempotent game-state flag.
class SurfacePixelLock {
public:
	explicit SurfacePixelLock(spritectl_surface_t surface) : m_surface(surface)
	{
		if (spritectl_lock_surface(surface, &info) != 0) m_surface = nullptr;
	}
	~SurfacePixelLock() { if (m_surface) spritectl_unlock_surface(m_surface); }
	SurfacePixelLock(const SurfacePixelLock&) = delete;
	SurfacePixelLock& operator=(const SurfacePixelLock&) = delete;
	spritectl_surface_info_t info{};
private:
	spritectl_surface_t m_surface;
};
}

/* ============================================================================
 * Static Member Initialization
 * ============================================================================ */

int CSpriteSurface::s_Value1 = 1;
int CSpriteSurface::s_Value2 = 31;
int CSpriteSurface::s_Value3 = 1;

// Register the non-palette effects restored for skill and rank icons.
// Other legacy effects keep the existing plain-copy fallback until ported.
FUNCTION_MEMCPYEFFECT CSpriteSurface::s_pMemcpyEffectFunction = NULL;
FUNCTION_MEMCPYEFFECT CSpriteSurface::s_pMemcpyEffectFunctionTable[MAX_EFFECT] = {
    NULL, CSpriteSurface::memcpyEffectGrayScale, NULL, NULL, NULL, NULL, NULL, NULL,
    CSpriteSurface::memcpyEffectGradation
};

/*
 * The palette effect routines are compiled (CSpriteSurface_Effects.cpp),
 * but they were written for three 5-bit channels, and on this backend
 * ColorDraw::Green decodes the full 6-bit field of a 5:6:5 pixel. The
 * screen blend is ported below (its green table is 64 wide for that
 * reason) and is the only effect any call site selects - every
 * DRAW_NORMALSPRITEPAL_EFFECT in MTopView.cpp passes EFFECT_SCREEN.
 * The rest still assume a green of 0..31: ColorDodge, for one, divides
 * by (32 - green), which is zero or negative for half the range. They
 * stay unregistered until someone ports them, so selecting one gets
 * memcpyPalEffect's plain copy rather than a wrong or crashing blend.
 * The table is indexed by FUNCTION_EFFECT; the entries are positional.
 */
FUNCTION_MEMCPYPALEFFECT CSpriteSurface::s_pMemcpyPalEffectFunction = NULL;
FUNCTION_MEMCPYPALEFFECT CSpriteSurface::s_pMemcpyPalEffectFunctionTable[MAX_EFFECT] =
{
	NULL,									/* EFFECT_DARKER */
	NULL,									/* EFFECT_GRAY_SCALE */
	NULL,									/* EFFECT_LIGHTEN */
	NULL,									/* EFFECT_DARKEN */
	NULL,									/* EFFECT_COLOR_DODGE */
	CSpriteSurface::memcpyPalEffectScreen,	/* EFFECT_SCREEN */
	NULL,									/* EFFECT_DODGE_BURN */
	NULL,									/* EFFECT_DIFFERENT */
	NULL,									/* EFFECT_GRADATION */
	NULL,									/* EFFECT_SIMPLE_OUTLINE */
	NULL,									/* EFFECT_WIPE_OUT */
	NULL,									/* EFFECT_NET */
	NULL,									/* EFFECT_GRAY_SCALE_VARIOUS */
	NULL,									/* EFFECT_SCREEN_ALPHA (an empty body) */
};

WORD CSpriteSurface::s_EffectScreenTableR[32][32] = {{0}};
WORD CSpriteSurface::s_EffectScreenTableG[64][64] = {{0}};
WORD CSpriteSurface::s_EffectScreenTableB[32][32] = {{0}};

/* ============================================================================
 * Constructor / Destructor
 * ============================================================================ */

CSpriteSurface::CSpriteSurface()
	: m_backend_surface(SPRITECTL_INVALID_SURFACE)
	, m_width(0)
	, m_height(0)
	, m_transparency(0)
	, m_lock_count(0)  // Initialize lock counter
{
}

CSpriteSurface::~CSpriteSurface()
{
	Release();
}

/* ============================================================================
 * Surface Initialization
 * ============================================================================ */

bool CSpriteSurface::Init(int width, int height)
{
	/* Cleanup existing surface */
	Release();

	/* Create backend surface */
	m_backend_surface = spritectl_create_surface(width, height, SPRITECTL_FORMAT_RGB565);
	if (m_backend_surface == SPRITECTL_INVALID_SURFACE) {
		return false;
	}

	m_width = width;
	m_height = height;
	return true;
}

bool CSpriteSurface::InitFromFile(const char* filename)
{
	(void)filename;
	/* TODO: Load from BMP file */
	return false;
}

void CSpriteSurface::Release()
{
	if (m_backend_surface != SPRITECTL_INVALID_SURFACE) {
		spritectl_destroy_surface(m_backend_surface);
		m_backend_surface = SPRITECTL_INVALID_SURFACE;
	}
	m_width = 0;
	m_height = 0;
	m_transparency = 0;
	m_lock_count = 0;
	m_ddsd = {};
}

/* ============================================================================
 * DirectX Compatibility Methods
 * ============================================================================ */

bool CSpriteSurface::InitOffsurface(int width, int height)
{
	/* Off-screen surface is the same as a regular surface in SDL2 */
	return Init(width, height);
}

void CSpriteSurface::SetTransparency(int value)
{
	m_transparency = value;
}

int CSpriteSurface::GetTransparency() const
{
	return m_transparency;
}

void CSpriteSurface::GDI_Text(int x, int y, const char* text, DWORD color)
{
	/* TODO: Implement text rendering using SDL2_ttf or similar
	 * For now, this is a stub - text rendering is not implemented in SDL backend
	 */
	(void)x; (void)y; (void)text; (void)color;
}

/* ============================================================================
 * Drawing Functions
 * ============================================================================ */

void CSpriteSurface::DrawRect(RECT* rect, WORD color)
{
	if (!rect || m_backend_surface == SPRITECTL_INVALID_SURFACE) {
		return;
	}

	spritectl_surface_s* backend = (spritectl_surface_s*)m_backend_surface;
	SDL_Surface* surf = backend->surface;
	if (!surf) {
		return;
	}

	/* Fill rect using backend */
	SDL_Rect sdl_rect;
	sdl_rect.x = rect->left;
	sdl_rect.y = rect->top;
	sdl_rect.w = rect->right - rect->left;
	sdl_rect.h = rect->bottom - rect->top;

	// Check surface format and handle accordingly
	Uint32 pixel = 0;

	// For 16-bit surfaces, try to use the color value directly if it matches the format
	if (SDL_ISPIXELFORMAT_INDEXED(surf->format->format) ||
	    surf->format->BytesPerPixel == 2) {
		// Check if this is RGB565 format
		if (surf->format->Rmask == 0xF800 &&
		    surf->format->Gmask == 0x07E0 &&
		    surf->format->Bmask == 0x001F) {
			// RGB565 format - use color directly
			pixel = (Uint32)color;

			// DEBUG: Check for HP bar colors (gray-ish colors)
			if ((color & 0xF800) != 0 && ((color & 0xF800) >> 11) < 25) {
				static int debug_count = 0;
				if (debug_count < 10) {
					uint8_t r = (color >> 11) & 0x1F;
					uint8_t g = (color >> 5) & 0x3F;
					uint8_t b = color & 0x1F;
					fprintf(stderr, "DrawRect HP color: input=0x%04X, RGB565=(%d,%d,%d), pixel=0x%08X, surface_format=%s\n",
						(uint16_t)color, r, g, b, pixel, SDL_GetPixelFormatName(surf->format->format));
					debug_count++;
				}
			}
		} else if (surf->format->Rmask == 0x7C00 &&
		           surf->format->Gmask == 0x03E0 &&
		           surf->format->Bmask == 0x001F) {
			// RGB555 format - convert from RGB565
			uint16_t rgb565 = (uint16_t)color;
			// Convert RGB565 to RGB555
			pixel = ((rgb565 & 0xF800) >> 1) |  // R (5 bits)
			        ((rgb565 & 0x07C0) >> 1) |  // G (5 bits)
			        (rgb565 & 0x001F);         // B (5 bits)
			static int rgb555_count = 0;
			if (rgb555_count < 3 && (color & 0xF800) != 0) {
				fprintf(stderr, "DrawRect: Converting RGB565 0x%04X to RGB555 0x%04X\n", rgb565, (uint16_t)pixel);
				rgb555_count++;
			}
		} else {
			// Unknown 16-bit format - fall back to SDL_MapRGB
			uint8_t r = 0, g = 0, b = 0;
			if (backend->format == SPRITECTL_FORMAT_RGB555) {
				spritectl_555_to_rgb((uint16_t)color, &r, &g, &b);
			} else {
				spritectl_565_to_rgb((uint16_t)color, &r, &g, &b);
			}
			pixel = SDL_MapRGB(surf->format, r, g, b);
			static int fallback_count = 0;
			if (fallback_count < 3 && r > 100) {
				fprintf(stderr, "DrawRect: Fallback to SDL_MapRGB: format=%s, Rmask=0x%08X, Gmask=0x%08X, Bmask=0x%08X, RGB=(%d,%d,%d), pixel=0x%08X\n",
					SDL_GetPixelFormatName(surf->format->format),
					surf->format->Rmask, surf->format->Gmask, surf->format->Bmask,
					r, g, b, pixel);
				fallback_count++;
			}
		}
	} else {
		// Non-16-bit surface, use SDL_MapRGB
		uint8_t r = 0, g = 0, b = 0;
		if (backend->format == SPRITECTL_FORMAT_RGB555) {
			spritectl_555_to_rgb((uint16_t)color, &r, &g, &b);
		} else {
			spritectl_565_to_rgb((uint16_t)color, &r, &g, &b);
		}
		pixel = SDL_MapRGB(surf->format, r, g, b);
	}

	spritectl_surface_info_t info;
	if (SpriteGpu::Fill(m_backend_surface, &sdl_rect, pixel)) return;
	if (spritectl_lock_surface(m_backend_surface, &info) == 0) {
		SDL_FillRect(surf, &sdl_rect, pixel);
		spritectl_unlock_surface(m_backend_surface);
	}
}

void CSpriteSurface::FillRect(RECT* rect, WORD color)
{
	DrawRect(rect, color);
}

void CSpriteSurface::HLine(int x, int y, int length, WORD color)
{
	RECT rect;
	rect.left = x;
	rect.top = y;
	rect.right = x + length;
	rect.bottom = y + 1;
	DrawRect(&rect, color);
}

void CSpriteSurface::VLine(int x, int y, int length, WORD color)
{
	RECT rect;
	rect.left = x;
	rect.top = y;
	rect.right = x + 1;
	rect.bottom = y + length;
	DrawRect(&rect, color);
}

void CSpriteSurface::Line(int x1, int y1, int x2, int y2, WORD color)
{
	/* TODO: Implement line drawing */
	/* For now, just draw horizontal/vertical lines */
	if (y1 == y2) {
		int x = (x1 < x2) ? x1 : x2;
		int len = (x2 - x1);
		if (len < 0) len = -len;
		HLine(x, y1, len, color);
	} else if (x1 == x2) {
		int y = (y1 < y2) ? y1 : y2;
		int len = (y2 - y1);
		if (len < 0) len = -len;
		VLine(x1, y, len, color);
	}
}

/* ============================================================================
 * Stub Implementations (TODO)
 * ============================================================================ */

void CSpriteSurface::BltHalf(POINT* pPoint, CSpriteSurface* SourceSurface, RECT* pRect)
{
	(void)pPoint;
	(void)SourceSurface;
	(void)pRect;
	/* TODO: Implement */
}

void CSpriteSurface::BltNoColorkey(POINT* pPoint, CSpriteSurface* SourceSurface, RECT* pRect)
{
	// These compatibility methods both copy raw pixels.
	Blt(pPoint, SourceSurface, pRect);
}

void CSpriteSurface::BltDarkness(POINT* pPoint, CSpriteSurface* SourceSurface, RECT* pRect, BYTE DarkBits)
{
	(void)pPoint;
	(void)SourceSurface;
	(void)pRect;
	(void)DarkBits;
	/* TODO: Implement */
}

void CSpriteSurface::BltBrightness(POINT* pPoint, CSpriteSurface* SourceSurface, RECT* pRect, BYTE BrightBits)
{
	(void)pPoint;
	(void)SourceSurface;
	(void)pRect;
	(void)BrightBits;
	/* TODO: Implement */
}

void CSpriteSurface::BltDarknessFilter(POINT* pPoint, CSpriteSurface* SourceSurface, RECT* pRect, WORD TransColor)
{
	(void)pPoint;
	(void)SourceSurface;
	(void)pRect;
	(void)TransColor;
	/* TODO: Implement */
}

void CSpriteSurface::ChangeBrightnessBit(RECT* pRect, BYTE DarkBits)
{
	(void)pRect;
	(void)DarkBits;
	/* TODO: Implement */
}

void CSpriteSurface::BltColorAlpha(RECT* pRect, WORD color, BYTE alpha2)
{
	(void)pRect;
	(void)color;
	(void)alpha2;
	/* TODO: Implement */
}

void CSpriteSurface::SetTransient(bool transient)
{
	if (m_backend_surface) m_backend_surface->transient = transient;
}

/*
 * Screen blend, per channel with maximum M:
 *
 *     screen = max(d, s) + min(d, s) * (M - max(d, s)) / M
 *
 * which is the integer form of 1 - (1 - d)(1 - s). The table holds the
 * result already shifted into the channel's bit position, so the blend
 * routine ORs the three lookups together. Red and blue are 5-bit
 * (M = 32); green is the 6-bit field ColorDraw::Green returns (M = 64).
 * GameInit calls this once before anything draws.
 */
static WORD ScreenBlendChannel(int d, int s, int maximum)
{
	int	high = (d > s) ? d : s;
	int	low  = (d > s) ? s : d;

	return (WORD)((((maximum - high) * low) / maximum) + high);
}

void CSpriteSurface::InitEffectTable()
{
	int d, s;

	for (d = 0; d < 32; d++)
	{
		for (s = 0; s < 32; s++)
		{
			s_EffectScreenTableR[d][s] = ScreenBlendChannel(d, s, 32) << ColorDraw::s_bSHIFT_R;
			s_EffectScreenTableB[d][s] = ScreenBlendChannel(d, s, 32) << ColorDraw::s_bSHIFT_B;
		}
	}

	for (d = 0; d < 64; d++)
	{
		for (s = 0; s < 64; s++)
		{
			s_EffectScreenTableG[d][s] = ScreenBlendChannel(d, s, 64) << ColorDraw::s_bSHIFT_G;
		}
	}
}

void CSpriteSurface::memcpyHalf(WORD* pDest, WORD* pSource, WORD pixels)
{
	(void)pDest;
	(void)pSource;
	(void)pixels;
	/* TODO: Implement */
}

void CSpriteSurface::memcpyAlpha(WORD* pDest, WORD* pSource, WORD pixels)
{
	(void)pDest;
	(void)pSource;
	(void)pixels;
	/* TODO: Implement */
}

void CSpriteSurface::memcpyColor(WORD* pDest, WORD* pSource, WORD pixels)
{
    // rgb_RED / rgb_GREEN / rgb_BLUE select one original color channel.
    const WORD masks[] = {0xf800, 0x07e0, 0x001f};
    const WORD mask = s_Value1 >= 0 && s_Value1 < 3 ? masks[s_Value1] : 0xffff;
    for (int i = 0; i < pixels; ++i) pDest[i] = pSource[i] & mask;
}

void CSpriteSurface::memcpyScale(WORD* pDest, WORD destPitch, WORD* pSource, WORD pixels)
{
	(void)pDest;
	(void)destPitch;
	(void)pSource;
	(void)pixels;
	/* TODO: Implement */
}

void CSpriteSurface::memcpyDarkness(WORD* pDest, WORD* pSource, WORD pixels)
{
    const int shift = SDL_max(0, SDL_min(s_Value1, 6));
    for (int i = 0; i < pixels; ++i) {
        const WORD c = pSource[i];
        pDest[i] = (((c >> 11) >> shift) << 11)
            | ((((c >> 5) & 63) >> shift) << 5) | ((c & 31) >> shift);
    }
}

void CSpriteSurface::memcpyBrightness(WORD* pDest, WORD* pSource, WORD pixels)
{
	(void)pDest;
	(void)pSource;
	(void)pixels;
	/* TODO: Implement */
}

/* ============================================================================
 * Clipping Helper
 * ============================================================================ */

bool CSpriteSurface::ClippingRectToPoint(RECT*& pRect, POINT*& pPoint)
{
	(void)pRect;
	(void)pPoint;
	/* TODO: Implement clipping */
	return true;
}

/* ============================================================================
 * Include Adapter Implementation (BltSprite methods)
 * ============================================================================ */

/* Include the adapter code for BltSprite methods */
#include "CSpriteSurface_Adapter.cpp"

/* ============================================================================
 * Lock/Unlock Methods (Stub implementations for compatibility)
 * ============================================================================ */

bool CSpriteSurface::LockSDL()
{
	/* Stub: For SDL backend, we don't need explicit locking
	 * The spritectl_lock_surface is called internally when needed
	 */
	return true;
}

void CSpriteSurface::UnlockSDL()
{
	/* Stub: No explicit unlocking needed for SDL backend */
}

bool CSpriteSurface::IsLock()
{
	return m_lock_count > 0;
}

/* ============================================================================
 * Get Surface Info (for compatibility with Windows code)
 * ============================================================================ */

int CSpriteSurface::GetSurfacePitch() const
{
	/* Return the pitch (bytes per row) of the surface */
	if (m_backend_surface == SPRITECTL_INVALID_SURFACE) {
		return 0;
	}

	return m_backend_surface->surface->pitch;
}

int CSpriteSurface::GetWidth() const
{
	return m_width;
}

int CSpriteSurface::GetHeight() const
{
	return m_height;
}

void CSpriteSurface::GetSurfaceInfo(S_SURFACEINFO* info)
{
	if (!info) return;
	*info = {};
	if (!m_backend_surface || !m_backend_surface->surface) return;
	SDL_Surface* surface = m_backend_surface->surface;
	if (!SpriteGpu::CpuAccess(m_backend_surface, true, true)) return;
	info->width = surface->w;
	info->height = surface->h;
	info->pitch = surface->pitch;
	// Legacy callers borrow pixels without a release operation. This is only
	// supported for the uncompressed software surfaces Init() creates; never
	// return a pointer that SDL can invalidate on unlock.
	if (!SDL_MUSTLOCK(surface)) info->p_surface = surface->pixels;
}

/* ============================================================================
 * GetDDSD Compatibility Wrapper (for Windows API compatibility)
 * ============================================================================ */

S_SURFACEINFO* CSpriteSurface::GetDDSD()
{
	GetSurfaceInfo(&m_ddsd);
	return &m_ddsd;
}

/* ============================================================================
 * Clipping Methods (compatibility with CDirectDrawSurface)
 * ============================================================================ */

void CSpriteSurface::SetClip(RECT* rect)
{
    if (!m_backend_surface || !m_backend_surface->surface) return;
    if (!rect) {
        SetClipNULL();
        return;
    }
    SDL_Rect clip = {rect->left, rect->top,
                     SDL_max(0, rect->right - rect->left),
                     SDL_max(0, rect->bottom - rect->top)};
    SDL_SetClipRect(m_backend_surface->surface, &clip);
}

void CSpriteSurface::SetClipNULL()
{
    if (m_backend_surface && m_backend_surface->surface)
        SDL_SetClipRect(m_backend_surface->surface, nullptr);
}

/* ============================================================================
 * Blt Method (compatibility with CDirectDrawSurface)
 * ============================================================================ */

void CSpriteSurface::Blt(POINT* pPoint, CSpriteSurface* SourceSurface, RECT* pRect)
{
	if (!pPoint || !SourceSurface || !m_backend_surface || !SourceSurface->m_backend_surface)
		return;
	if (m_backend_surface->surface->format->BytesPerPixel != 2
		|| SourceSurface->m_backend_surface->surface->format->BytesPerPixel != 2)
		return;

	// Intersect both surfaces in offset coordinates. Widen before arithmetic:
	// RECT and POINT may contain extreme signed values.
	int64_t sx = pRect ? pRect->left : 0, sy = pRect ? pRect->top : 0;
	int64_t width = pRect ? int64_t(pRect->right) - sx : SourceSurface->m_width;
	int64_t height = pRect ? int64_t(pRect->bottom) - sy : SourceSurface->m_height;
	int64_t dx = pPoint->x, dy = pPoint->y;
	const SDL_Rect& clip = m_backend_surface->surface->clip_rect;
	const int64_t left = (std::max)({int64_t(0), -sx, int64_t(clip.x) - dx});
	const int64_t top = (std::max)({int64_t(0), -sy, int64_t(clip.y) - dy});
	const int64_t right = (std::min)({width, int64_t(SourceSurface->m_width) - sx,
		int64_t(clip.x) + clip.w - dx});
	const int64_t bottom = (std::min)({height, int64_t(SourceSurface->m_height) - sy,
		int64_t(clip.y) + clip.h - dy});
	if (left >= right || top >= bottom) return;
	sx += left; dx += left; sy += top; dy += top;
	width = right - left; height = bottom - top;
	const SDL_Rect from{int(sx), int(sy), int(width), int(height)};
	const SDL_Rect to{int(dx), int(dy), int(width), int(height)};
	if (SpriteGpu::Copy(m_backend_surface, to, SourceSurface->m_backend_surface, from)) return;

	SurfacePixelLock source(SourceSurface->m_backend_surface);
	SurfacePixelLock dest(m_backend_surface);
	if (!source.info.pixels || !dest.info.pixels) return;
	const bool sameSurface = source.info.pixels == dest.info.pixels;
	for (int64_t row = 0; row < height; ++row) {
		const int64_t y = sameSurface && dy > sy ? height - 1 - row : row;
		const BYTE* from = static_cast<BYTE*>(source.info.pixels) + (sy + y) * source.info.pitch + sx * 2;
		BYTE* to = static_cast<BYTE*>(dest.info.pixels) + (dy + y) * dest.info.pitch + dx * 2;
		memmove(to, from, static_cast<size_t>(width) * sizeof(WORD));
	}
}

/* ============================================================================
 * FillSurface Method (compatibility with CDirectDrawSurface)
 * ============================================================================ */

void CSpriteSurface::FillSurface(WORD color)
{
	if (!m_backend_surface || m_backend_surface->surface->format->BytesPerPixel != 2) return;
	if (SpriteGpu::Fill(m_backend_surface, nullptr, color, true)) return;
	SurfacePixelLock lock(m_backend_surface);
	if (!lock.info.pixels) return;
	for (int y = 0; y < lock.info.height; ++y) {
		WORD* row = reinterpret_cast<WORD*>(static_cast<BYTE*>(lock.info.pixels) + y * lock.info.pitch);
		std::fill_n(row, lock.info.width, color);
	}
}

/* ============================================================================
 * Gamma Correction
 * ============================================================================ */

void CSpriteSurface::Gamma4Pixel565(void *pDest, int len, int p)
{
	// TODO: [SDL_BACKEND] Implement optimized gamma correction algorithm
	// Current implementation uses basic RGB scaling
	// Original Windows implementation uses x86 assembly
	WORD* dest = (WORD*)pDest;
	int light = p;
	
	for (int i = 0; i < len; i++) {
		WORD pixel = dest[i];
		
		// Extract RGB565 components
		int r = (pixel >> 11) & 0x1F;
		int g = (pixel >> 5) & 0x3F;
		int b = pixel & 0x1F;
		
		// Apply gamma correction
		r = (r * light) >> 5;
		g = (g * light) >> 5;
		b = (b * light) >> 5;
		
		// Clamp values
		if (r > 31) r = 31;
		if (g > 63) g = 63;
		if (b > 31) b = 31;
		
		// Recombine to RGB565
		dest[i] = (r << 11) | (g << 5) | b;
	}
}

/* ============================================================================
 * Gamma4Pixel555 - Alias to Gamma4Pixel565 for compatibility
 * RGB555 and RGB565 have same structure (5-6-5 vs 5-5-5)
 * SDL backend uses RGB565 only, so Gamma4Pixel555 maps to Gamma4Pixel565
 * ============================================================================ */
void CSpriteSurface::Gamma4Pixel555(void *pDest, int len, int p)
{
	// RGB555 and RGB565 are structurally similar
	// In SDL backend, we always use RGB565, so just call Gamma4Pixel565
	Gamma4Pixel565(pDest, len, p);
}

/* ============================================================================
 * GammaBox565/555 - apply Gamma4Pixel565/555 to every row of pRect
 * (ported from the original CDirectDrawSurface::GammaBox565/555)
 * ============================================================================ */
void CSpriteSurface::GammaBox565(RECT* pRect, int p)
{
	if (!pRect)
	{
		return;
	}

	// SDL backend has no DirectDraw-style clip region tracking, so clip
	// against the surface's own bounds instead (matches GetClipRight()/
	// GetClipBottom() stubs above, which return m_width/m_height).
	if (pRect->bottom < 0 || pRect->top > m_height
		|| pRect->right < 0 || pRect->left > m_width)
	{
		return;
	}

	if (pRect->left < 0) pRect->left = 0;
	if (pRect->right > m_width) pRect->right = m_width;
	if (pRect->top < 0) pRect->top = 0;
	if (pRect->bottom > m_height) pRect->bottom = m_height;

	if (pRect->left >= pRect->right || pRect->top >= pRect->bottom)
	{
		return;
	}

	const SDL_Rect region{int(pRect->left), int(pRect->top),
		int(pRect->right - pRect->left), int(pRect->bottom - pRect->top)};
	if (SpriteGpu::Gamma(m_backend_surface, region, p)) return;
	SurfacePixelLock lock(m_backend_surface);
	const auto& info = lock.info;
	if (!info.pixels) return;
	WORD* pDest = (WORD*)((BYTE*)info.pixels + pRect->top * info.pitch + (pRect->left << 1));
	int dLen = pRect->right - pRect->left;
	int rows = pRect->bottom - pRect->top;

	for (int i = 0; i < rows; i++)
	{
		Gamma4Pixel565(pDest, dLen, p);
		pDest = (WORD*)((BYTE*)pDest + info.pitch);
	}
}

void CSpriteSurface::GammaBox555(RECT* pRect, int p)
{
	// SDL backend always uses RGB565 surfaces (see Gamma4Pixel555 above)
	GammaBox565(pRect, p);
}

void CSpriteSurface::ColorBox(const RECT* rect, BYTE rgb)
{
	if (!m_backend_surface || !rect || rgb > 2) return;
	const int left = int((std::max)(int64_t(0), int64_t(rect->left)));
	const int top = int((std::max)(int64_t(0), int64_t(rect->top)));
	const int right = int((std::min)(int64_t(m_width), int64_t(rect->right)));
	const int bottom = int((std::min)(int64_t(m_height), int64_t(rect->bottom)));
	if (left >= right || top >= bottom) return;
	const SDL_Rect region{left, top, right - left, bottom - top};
	if (SpriteGpu::Tint(m_backend_surface, region, rgb)) return;
	SurfacePixelLock lock(m_backend_surface);
	if (!lock.info.pixels) return;
	const WORD masks[]{0xf800, 0x07e0, 0x001f};
	for (int y = top; y < bottom; ++y) {
		auto* row = reinterpret_cast<WORD*>(static_cast<BYTE*>(lock.info.pixels) + y * lock.info.pitch);
		for (int x = left; x < right; ++x) row[x] &= masks[rgb];
	}
}

void CSpriteSurface::ApplyLightGrid(const CFilter& filter, const int* widths, const int* heights)
{
	if (!m_backend_surface || !filter.IsInit() || !widths || !heights) return;
	std::vector<SpriteGpuEffects::LightCell> cells;
	cells.reserve(size_t(filter.GetWidth()) * filter.GetHeight());
	int64_t top = 0;
	for (int y = 0; y < filter.GetHeight() && top < m_height; ++y) {
		if (heights[y] <= 0) return;
		int64_t left = 0;
		const BYTE* light = filter.GetFilter(WORD(y));
		if (!light) return;
		for (int x = 0; x < filter.GetWidth() && left < m_width; ++x) {
			if (widths[x] <= 0) return;
			const int width = int((std::min)(int64_t(widths[x]), int64_t(m_width) - left));
			const int height = int((std::min)(int64_t(heights[y]), int64_t(m_height) - top));
			cells.push_back({{int(left), int(top), width, height}, light[x]});
			left += widths[x];
		}
		top += heights[y];
	}
	if (SpriteGpu::LightGrid(m_backend_surface, cells)) return;
	SurfacePixelLock lock(m_backend_surface);
	if (!lock.info.pixels) return;
	for (const auto& cell : cells) {
		for (int y = cell.rect.y; y < cell.rect.y + cell.rect.h; ++y) {
			auto* row = reinterpret_cast<WORD*>(static_cast<BYTE*>(lock.info.pixels) + y * lock.info.pitch);
			Gamma4Pixel565(row + cell.rect.x, cell.rect.w, cell.light);
		}
	}
}

/* ============================================================================
 * Missing Methods for Linker Compatibility
 * ============================================================================ */

/* ----------------------------------------------------------------------------
 * Lock state model
 *
 * The game code was written against DirectDraw, where Lock()/Unlock() flip a
 * single boolean flag: locking an already locked surface is a no-op, and
 * unlocking one that is not locked is harmless. MTopView and the UI layer rely
 * on that - they unlock defensively on several paths, re-lock from IsLock()
 * snapshots, and leave whole branches deliberately unbalanced. A counting model
 * therefore drifts upwards and never returns to zero, which breaks two things:
 * the "surface must not be locked" asserts in the UI layer fire every frame,
 * and SDL_BlitSurface refuses to blit a surface whose SDL lock is still held,
 * so the frame flip silently draws nothing.
 *
 * Backend surfaces are plain software SDL_Surfaces (SDL_CreateRGBSurface, no
 * RLE color key), so SDL_MUSTLOCK() is false for them and their pixel pointer
 * stays valid whether or not they are locked; the blit helpers take their own
 * short backend lock around direct pixel access. We therefore keep the
 * DirectDraw-style flag for the callers that branch on IsLock(), and never hold
 * an SDL lock across game code.
 * -------------------------------------------------------------------------- */

/* Pure accessor: returns the pixel pointer without changing the lock state. */
void* CSpriteSurface::GetSurfacePointer()
{
	S_SURFACEINFO info{};
	GetSurfaceInfo(&info);
	return info.p_surface;
}

bool CSpriteSurface::Lock()
{
	if (!m_backend_surface || !m_backend_surface->surface || SDL_MUSTLOCK(m_backend_surface->surface)) return false;
	m_lock_count = 1;
	return true;
}

void* CSpriteSurface::Lock(RECT* rect, DWORD* pitch)
{
	(void)rect;
	S_SURFACEINFO info{};
	GetSurfaceInfo(&info);
	if (pitch) *pitch = info.pitch;
	if (!info.p_surface) return nullptr;
	m_lock_count = 1;
	return info.p_surface;
}

void CSpriteSurface::Unlock()
{
	SpriteGpu::EndBorrow(m_backend_surface);
	m_lock_count = 0;
}

bool CSpriteSurface::InitTextureSurface(int width, int height, void* tex1, void* tex2)
{
	// OpenGL texture surface initialization
	// For SDL backend, just create a regular surface
	(void)tex1; (void)tex2; // Unused parameters
	return Init(width, height);
}

bool CSpriteSurface::Restore()
{
	// Restore surface after lost device (Windows-specific)
	// For SDL backend, this is a no-op
	return true;
}

namespace {
void ReportUnsupportedEffect(int effect, bool palette)
{
	// Selection happens while drawing. Report each unsupported kind once,
	// with one shared bucket for invalid IDs, rather than once per frame.
	static bool reported[2][CSpriteSurface::MAX_EFFECT + 1]{};
	const int slot = effect >= 0 && effect < CSpriteSurface::MAX_EFFECT
		? effect : CSpriteSurface::MAX_EFFECT;
	bool& seen = reported[palette ? 1 : 0][slot];
	if (seen) return;
	seen = true;
	SDL_LogWarn(SDL_LOG_CATEGORY_RENDER,
		"Unsupported %s sprite effect %d; using plain copy",
		palette ? "palette" : "pixel", effect);
}
}

void CSpriteSurface::SetEffect(FUNCTION_EFFECT effect)
{
	const int index = static_cast<int>(effect);
	s_pMemcpyEffectFunction = index >= 0 && index < MAX_EFFECT
		? s_pMemcpyEffectFunctionTable[index] : nullptr;
	if (!s_pMemcpyEffectFunction) ReportUnsupportedEffect(index, false);
}

void CSpriteSurface::SetPalEffect(FUNCTION_EFFECT effect)
{
	const int index = static_cast<int>(effect);
	s_pMemcpyPalEffectFunction = index >= 0 && index < MAX_EFFECT
		? s_pMemcpyPalEffectFunctionTable[index] : nullptr;
	if (!s_pMemcpyPalEffectFunction) ReportUnsupportedEffect(index, true);
}

// Static effect methods
void CSpriteSurface::memcpyEffect(unsigned short* dest, unsigned short* src, unsigned short pixels)
{
	if (s_pMemcpyEffectFunction != NULL) {
		s_pMemcpyEffectFunction(dest, src, pixels);
	} else {
		// Default: simple copy
		for (int i = 0; i < pixels; i++) {
			dest[i] = src[i];
		}
	}
}

void CSpriteSurface::memcpyEffectGrayScale(WORD* dest, WORD* src, WORD pixels)
{
    for (int i = 0; i < pixels; ++i) {
        const WORD c = src[i];
        // The legacy average uses three 5-bit channels; normalize RGB565 green.
        const int gray = ((c >> 11) + ((c >> 6) & 31) + (c & 31)) / 3;
        dest[i] = (gray << 11) | (((gray << 1) | (gray >> 4)) << 5) | gray;
    }
}

void CSpriteSurface::memcpyEffectGradation(WORD* dest, WORD* src, WORD pixels)
{
    if (s_Value1 < 0 || s_Value1 >= MAX_COLORSET) {
        memcpy(dest, src, pixels * sizeof(WORD));
        return;
    }
    for (int i = 0; i < pixels; ++i) {
        const WORD c = src[i];
        const int brightness = (c >> 11) + ((c >> 6) & 31) + (c & 31);
        // Pure white sums to 93; the legacy lookup has entries 0 through 92.
        const int gradation = CIndexSprite::ColorToGradation[SDL_min(brightness, MAX_COLOR_TO_GRADATION - 1)];
        dest[i] = CIndexSprite::ColorSet[s_Value1][SDL_min(gradation, MAX_COLORGRADATION - 1)];
    }
}

#endif /* SPRITELIB_BACKEND_SDL */
