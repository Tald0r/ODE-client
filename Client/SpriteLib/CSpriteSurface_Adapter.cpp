/*-----------------------------------------------------------------------------

	CSpriteSurface_Adapter.cpp

	SDL2 backend adapter implementation for CSpriteSurface.
	This file contains all BltSprite* methods implemented using SpriteLibBackend.

	NOTE: This file is included by CSpriteSurface_SDL.cpp
	      Do not compile separately.

	2025.01.14

-----------------------------------------------------------------------------*/

/* No #ifdef SPRITELIB_BACKEND_SDL here - included by SDL-only file */

#include "Client_PCH.h"
#include "CSprite.h"
#include "CAlphaSprite.h"
#include "CIndexSprite.h"
#include "CShadowSprite.h"
#include "CSpriteSurface.h"
#include "CFilter.h"

#include "SpriteLibBackend.h"
#include "SpriteScanline.h"
#include <climits>
#include <memory>
#include "DebugLog.h"

/* ============================================================================
 * Debug Configuration
 * ============================================================================ */

// Enable detailed debug logging for Sprite adapter
#ifndef SPRITE_ADAPTER_DEBUG
#define SPRITE_ADAPTER_DEBUG 0
#endif

// Enable tracking of backend sprite lifecycle
#ifndef SPRITE_ADAPTER_DEBUG_LIFECYCLE
#define SPRITE_ADAPTER_DEBUG_LIFECYCLE 0
#endif

#if SPRITE_ADAPTER_DEBUG
#define SA_DEBUG(fmt, ...) \
	fprintf(stderr, "[SpriteAdapter] %s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define SA_DEBUG(fmt, ...) do {} while(0)
#endif

#if SPRITE_ADAPTER_DEBUG_LIFECYCLE
#define SA_DEBUG_LIFECYCLE(fmt, ...) \
	fprintf(stderr, "[SpriteAdapter LIFECYCLE] %s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define SA_DEBUG_LIFECYCLE(fmt, ...) do {} while(0)
#endif

/* Error logging - always enabled */
#define SA_DEBUG_ERROR(fmt, ...) \
	fprintf(stderr, "[SpriteAdapter ERROR] %s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Get or create backend sprite from CSprite
 * Handles lazy creation and synchronization
 */
static spritectl_sprite_t get_backend_sprite(CSprite* pSprite)
{
	if (!pSprite || !pSprite->IsInit()) {
		static int notInitCount = 0;
		if (notInitCount < 3) {
			printf("[get_backend_sprite] ERROR: pSprite=%p, IsInit=%d\n", pSprite, pSprite ? pSprite->IsInit() : 0);
			notInitCount++;
		}
		return SPRITECTL_INVALID_SPRITE;
	}

	/* Lazy creation: create backend sprite if doesn't exist */
	if (pSprite->GetBackendSprite() == SPRITECTL_INVALID_SPRITE) {
		WORD width = pSprite->GetWidth();
		WORD height = pSprite->GetHeight();

		/* Create backend sprite with RLE data for correct transparency */
		spritectl_sprite_t new_sprite = spritectl_create_sprite_rle(width, height);
		if (!new_sprite) {
			return SPRITECTL_INVALID_SPRITE;
		}

		/* Copy RLE data from CSprite to backend sprite */
		for (WORD y = 0; y < height; y++) {
			WORD* src_line = pSprite->GetPixelLine(y);
			if (!src_line) {
				SA_DEBUG_ERROR("get_backend_sprite: Invalid scanline at y=%d", y);
				spritectl_destroy_sprite(new_sprite);
				return SPRITECTL_INVALID_SPRITE;
			}

			/* Get RLE data size with bounds checking */
			WORD* pSrc = src_line;
			WORD* pSrcStart = src_line;  /* Remember start for validation */

			int count = *pSrc++;  /* Number of runs */

			/* Validate count to prevent infinite loops or memory corruption */
			if (count < 0 || count > 16384) {  /* Arbitrary reasonable limit */
				SA_DEBUG_ERROR("get_backend_sprite: Invalid RLE count=%d at y=%d", count, y);
				spritectl_destroy_sprite(new_sprite);
				return SPRITECTL_INVALID_SPRITE;
			}

			/* Calculate total RLE data size (count + runs data) with bounds checking */
			int rle_size = 1;  /* count byte */
			if (count > 0) {
				for (int j = 0; j < count; j++) {
					/* Check if we have enough data for transCount and colorCount */
					if ((pSrc - pSrcStart) > width * 2) {  /* Safety check */
						SA_DEBUG_ERROR("get_backend_sprite: RLE data exceeds bounds at segment %d, y=%d", j, y);
						spritectl_destroy_sprite(new_sprite);
						return SPRITECTL_INVALID_SPRITE;
					}

					pSrc++;  /* Skip transCount */
					int colorCount = *pSrc++;  /* Color pixels */

					/* Validate colorCount */
					if (colorCount < 0 || colorCount > width) {
						SA_DEBUG_ERROR("get_backend_sprite: Invalid colorCount=%d at segment %d, y=%d", colorCount, j, y);
						spritectl_destroy_sprite(new_sprite);
						return SPRITECTL_INVALID_SPRITE;
					}

					rle_size += 2 + colorCount;  /* trans + color count + pixel data */
					pSrc += colorCount;
				}
			}

			/* Validate final rle_size */
			if (rle_size <= 0 || rle_size > 65535) {
				SA_DEBUG_ERROR("get_backend_sprite: Invalid rle_size=%d at y=%d", rle_size, y);
				spritectl_destroy_sprite(new_sprite);
				return SPRITECTL_INVALID_SPRITE;
			}

			/* Set RLE data using helper function */
			if (rle_size > 1) {  /* Non-empty scanline */
				if (spritectl_sprite_set_scanline_rle(new_sprite, y, src_line, rle_size) != 0) {
					/* Cleanup and fall back */
					spritectl_destroy_sprite(new_sprite);
					return SPRITECTL_INVALID_SPRITE;
				}
			}
		}

		pSprite->SetBackendSprite(new_sprite);
		pSprite->SetBackendDirty(false);
	}
	/* Sync if dirty */
	else if (pSprite->IsBackendDirty()) {
		/* Destroy old sprite and recreate */
		spritectl_destroy_sprite(pSprite->GetBackendSprite());
		pSprite->SetBackendSprite(SPRITECTL_INVALID_SPRITE);

		/* Recreate (will be created on next call) */
		return get_backend_sprite(pSprite);
	}

	return pSprite->GetBackendSprite();
}

/**
 * Get or create backend sprite from CAlphaSprite
 * Handles lazy creation and synchronization
 * NOTE: CAlphaSprite uses RLE compression with alpha channel
 */
static spritectl_sprite_t get_backend_alpha_sprite(CAlphaSprite* pSprite)
{
	if (!pSprite || !pSprite->IsInit()) {
		return SPRITECTL_INVALID_SPRITE;
	}

	/* Lazy creation: create backend sprite if doesn't exist */
	if (pSprite->GetBackendSprite() == SPRITECTL_INVALID_SPRITE) {
		WORD width = pSprite->GetWidth();
		WORD height = pSprite->GetHeight();

		size_t pixel_count = size_t(width) * height;
		if (!pixel_count || pixel_count > INT_MAX) return SPRITECTL_INVALID_SPRITE;
		size_t data_size = pixel_count * sizeof(WORD);

		/* Allocate and decompress pixel data */
		WORD* pixels = (WORD*)malloc(data_size);
		if (!pixels) {
			return SPRITECTL_INVALID_SPRITE;
		}

		/* Decompress RLE format to raw pixels */
		/* Initialize with transparent color */
		WORD colorkey = CAlphaSprite::GetColorkey();
		for (size_t i = 0; i < pixel_count; i++) {
			pixels[i] = colorkey;
		}

		/* Decompress each line */
		for (WORD y = 0; y < height; y++) {
			const auto line = pSprite->GetPixelLineSpan(y);
			if (!ValidateSpriteScanline(line, width, 2)) {
				free(pixels);
				return SPRITECTL_INVALID_SPRITE;
			}
			const WORD* pPixels = line.data();
			WORD* dst_line = pixels + (size_t(y) * width);

			int count = *pPixels++;  // RLE run count
			int x = 0;

			if (count > 0) {
				for (int i = 0; i < count; i++) {
					int transCount = *pPixels++;   // transparent pixel count
					int colorCount = *pPixels++;   // colored pixel count

					x += transCount;  // skip transparent pixels

					/* Copy colored pixels with alpha */
					for (int j = 0; j < colorCount; j++) {
						pPixels++;  // alpha value
						WORD color = *pPixels++;   // color value

						if (x < width) {
							/* Store as pre-multiplied alpha or similar format */
							/* For now, just store the color directly */
							dst_line[x] = color;
						}
						x++;
					}
				}
			}
		}

		/* Create backend sprite */
		spritectl_sprite_t new_sprite = spritectl_create_sprite(
			width, height, SPRITECTL_FORMAT_RGB565,
			pixels, data_size);

		free(pixels);
		pSprite->SetBackendSprite(new_sprite);
		pSprite->SetBackendDirty(false);
	}
	/* Sync if dirty */
	else if (pSprite->IsBackendDirty()) {
		/* Destroy old sprite and recreate */
		spritectl_destroy_sprite(pSprite->GetBackendSprite());
		pSprite->SetBackendSprite(SPRITECTL_INVALID_SPRITE);

		/* Recreate (will be created on next call) */
		return get_backend_alpha_sprite(pSprite);
	}

	return pSprite->GetBackendSprite();
}

/**
 * Get or create backend sprite from CShadowSprite
 * Handles lazy creation and synchronization
 */
static spritectl_sprite_t get_backend_shadow_sprite(CShadowSprite* pSprite)
{
	SA_DEBUG_LIFECYCLE("get_backend_shadow_sprite: pSprite=%p, IsInit=%d",
	                   (void*)pSprite, pSprite ? pSprite->IsInit() : 0);

	if (!pSprite || !pSprite->IsInit()) {
		return SPRITECTL_INVALID_SPRITE;
	}

	/* Lazy creation: create backend sprite if doesn't exist */
	if (pSprite->GetBackendSprite() == SPRITECTL_INVALID_SPRITE) {
		WORD width = pSprite->GetWidth();
		WORD height = pSprite->GetHeight();

		const size_t pixel_count = size_t(width) * height;
		if (!pixel_count || pixel_count > INT_MAX) return SPRITECTL_INVALID_SPRITE;
		for (WORD y = 0; y < height; ++y) {
			if (!ValidateShadowScanline(pSprite->GetPixelLineSpan(y), width))
				return SPRITECTL_INVALID_SPRITE;
		}
		size_t data_size = pixel_count * sizeof(WORD);

		SA_DEBUG_LIFECYCLE("get_backend_shadow_sprite: Creating backend sprite, size=%dx%d (%zu pixels, %zu bytes)",
		                   width, height, pixel_count, data_size);

		/* Allocate and decode pixel data */
		WORD* pixels = (WORD*)malloc(data_size);
		if (!pixels) {
			SA_DEBUG_LIFECYCLE("get_backend_shadow_sprite: Failed to allocate pixel buffer");
			return SPRITECTL_INVALID_SPRITE;
		}

		SA_DEBUG_LIFECYCLE("get_backend_shadow_sprite: Allocated temp pixels=%p", (void*)pixels);

		memset(pixels, 0, data_size);
		// Decode with a size_t row stride; the legacy Blt API takes WORD pitch.
		for (WORD y = 0; y < height; ++y) {
			const auto line = pSprite->GetPixelLineSpan(y);
			WORD* dest = pixels + size_t(y) * width;
			size_t x = 0;
			for (size_t run = 0; run < line[0]; ++run) {
				x += line[1 + run * 2];
				const size_t colored = line[2 + run * 2];
				memset(dest + x, 0, colored * sizeof(WORD));
				x += colored;
			}
		}

		/* Create backend sprite */
		spritectl_sprite_t new_sprite = spritectl_create_sprite(
			width, height, SPRITECTL_FORMAT_RGB565,
			pixels, data_size);

		SA_DEBUG_LIFECYCLE("get_backend_shadow_sprite: Created backend sprite=%p from temp pixels=%p",
		                   (void*)new_sprite, (void*)pixels);

		// Free temp pixels AFTER creating the sprite (the sprite copies the data)
		free(pixels);
		pixels = NULL;  // Prevent dangling pointer

		pSprite->SetBackendSprite(new_sprite);
		pSprite->SetBackendDirty(false);

		SA_DEBUG_LIFECYCLE("get_backend_shadow_sprite: Set backend sprite=%p for CShadowSprite=%p",
		                   (void*)new_sprite, (void*)pSprite);
	}
	/* Sync if dirty */
	else if (pSprite->IsBackendDirty()) {
		SA_DEBUG_LIFECYCLE("get_backend_shadow_sprite: Backend dirty, destroying old sprite=%p",
		                   (void*)pSprite->GetBackendSprite());
		/* Destroy old sprite and recreate */
		spritectl_destroy_sprite(pSprite->GetBackendSprite());
		pSprite->SetBackendSprite(SPRITECTL_INVALID_SPRITE);

		/* Recreate (will be created on next call) */
		return get_backend_shadow_sprite(pSprite);
	}
	else {
		SA_DEBUG_LIFECYCLE("get_backend_shadow_sprite: Reusing existing backend sprite=%p",
		                   (void*)pSprite->GetBackendSprite());
	}

	return pSprite->GetBackendSprite();
}

/**
 * Get or create backend sprite from CIndexSprite
 * Handles lazy creation and synchronization
 */
static spritectl_sprite_t get_backend_index_sprite(CIndexSprite* pSprite)
{
	if (!pSprite || !pSprite->IsInit()) {
		return SPRITECTL_INVALID_SPRITE;
	}

	/* Lazy creation: create backend sprite if doesn't exist */
	if (pSprite->GetBackendSprite() == SPRITECTL_INVALID_SPRITE) {
		WORD width = pSprite->GetWidth();
		WORD height = pSprite->GetHeight();

		size_t pixel_count = width * height;
		size_t data_size = pixel_count * sizeof(WORD);

		/* Allocate and decode pixel data (index sprites are RLE-compressed) */
		WORD* pixels = (WORD*)malloc(data_size);
		if (!pixels) {
			return SPRITECTL_INVALID_SPRITE;
		}
		memset(pixels, 0, data_size);
		pSprite->Blt(pixels, width * sizeof(WORD));

		/* Create backend sprite */
		spritectl_sprite_t new_sprite = spritectl_create_sprite(
			width, height, SPRITECTL_FORMAT_RGB565,
			pixels, data_size);

		free(pixels);
		pSprite->SetBackendSprite(new_sprite);
		pSprite->CaptureBackendPalette();
		pSprite->SetBackendDirty(false);
	}
	/* A decoded RGB sprite also depends on the selected palette channels. */
	else if (pSprite->IsBackendDirty() || !pSprite->IsBackendPaletteCurrent()) {
		/* Destroy old sprite and recreate */
		spritectl_destroy_sprite(pSprite->GetBackendSprite());
		pSprite->SetBackendSprite(SPRITECTL_INVALID_SPRITE);

		/* Recreate (will be created on next call) */
		return get_backend_index_sprite(pSprite);
	}

	return pSprite->GetBackendSprite();
}

/* ============================================================================
 * BltSprite Methods
 * ============================================================================ */

void CSpriteSurface::BltSprite(POINT* pPoint, CSprite* pSprite) {
	if (!pPoint || !pSprite) {
		return;
	}

	/* Get backend sprite */
	spritectl_sprite_t backend_sprite = get_backend_sprite(pSprite);
	if (!backend_sprite) {
		LOG_WARN("[BltSprite] ERROR: get_backend_sprite returned invalid sprite! IsInit=%d\n", pSprite->IsInit());
		return;
	}

	/* Blit to backend surface */
	int flags = 0;
	int alpha = 255;
	spritectl_blt_sprite(m_backend_surface, pPoint->x, pPoint->y,
	                    backend_sprite, flags, alpha);
}

template<class Draw>
static void WithSpriteClip(spritectl_surface_t surface, const POINT* point,
	CSprite* sprite, const RECT* source, Draw draw)
{
	if (!surface || !surface->surface || !point || !sprite || !source) return;
	const SDL_Rect saved = surface->surface->clip_rect;
	const int64_t left = (std::max)(int64_t(saved.x), int64_t(point->x) + (std::max)(int64_t(0), int64_t(source->left)));
	const int64_t top = (std::max)(int64_t(saved.y), int64_t(point->y) + (std::max)(int64_t(0), int64_t(source->top)));
	const int64_t right = (std::min)(int64_t(saved.x) + saved.w,
		int64_t(point->x) + (std::min)(int64_t(sprite->GetWidth()), int64_t(source->right)));
	const int64_t bottom = (std::min)(int64_t(saved.y) + saved.h,
		int64_t(point->y) + (std::min)(int64_t(sprite->GetHeight()), int64_t(source->bottom)));
	if (left >= right || top >= bottom) return;
	const SDL_Rect clip{int(left), int(top), int(right - left), int(bottom - top)};
	struct RestoreClip {
		SDL_Surface* surface;
		SDL_Rect clip;
		~RestoreClip() { SDL_SetClipRect(surface, &clip); }
	} restore{surface->surface, saved};
	SDL_SetClipRect(surface->surface, &clip);
	draw();
}

void CSpriteSurface::BltSpriteClip(POINT* point, CSprite* sprite, const RECT* source)
{
	WithSpriteClip(m_backend_surface, point, sprite, source, [&] { BltSprite(point, sprite); });
}

void CSpriteSurface::BltSpriteColorClip(POINT* point, CSprite* sprite, const RECT* source, BYTE rgb)
{
	WithSpriteClip(m_backend_surface, point, sprite, source, [&] { BltSpriteColor(point, sprite, rgb); });
}

void CSpriteSurface::BltSpriteEffectClip(POINT* point, CSprite* sprite, const RECT* source)
{
	WithSpriteClip(m_backend_surface, point, sprite, source, [&] { BltSpriteEffect(point, sprite); });
}

void CSpriteSurface::BltSpriteNoClip(POINT* pPoint, CSprite* pSprite) {
	/* For now, same as BltSprite */
	BltSprite(pPoint, pSprite);
}

void CSpriteSurface::BltSpriteHalf(POINT* pPoint, CSprite* pSprite) {
	if (!pPoint || !pSprite) {
		return;
	}

	/* Get backend sprite */
	spritectl_sprite_t backend_sprite = get_backend_sprite(pSprite);
	if (!backend_sprite) {
		return;
	}

	/* Scale factor: 128 = 0.5x */
	int scale = 128;
	int flags = 0;
	spritectl_blt_sprite_scaled(m_backend_surface, pPoint->x, pPoint->y,
	                            backend_sprite, scale, flags);
}

void CSpriteSurface::BltSpriteAlpha(POINT* pPoint, CSprite* pSprite, BYTE alphaDepth) {
	if (!pPoint || !pSprite) {
		return;
	}

	/* Get backend sprite */
	spritectl_sprite_t backend_sprite = get_backend_sprite(pSprite);
	if (!backend_sprite) {
		return;
	}

	/* Blit with alpha */
	int flags = SPRITECTL_BLT_ALPHA;
	spritectl_blt_sprite(m_backend_surface, pPoint->x, pPoint->y,
	                    backend_sprite, flags, alphaDepth);
}

void CSpriteSurface::BltSpriteScale(POINT* pPoint, CSprite* pSprite, int scale) {
	if (!pPoint || !pSprite) {
		return;
	}

	/* Get backend sprite */
	spritectl_sprite_t backend_sprite = get_backend_sprite(pSprite);
	if (!backend_sprite) {
		return;
	}

	/* Scale parameter: 256 = 1x, 128 = 0.5x, 512 = 2x */
	int scale_factor = scale;
	int flags = 0;
	spritectl_blt_sprite_scaled(m_backend_surface, pPoint->x, pPoint->y,
	                            backend_sprite, scale_factor, flags);
}

/* ============================================================================
 * Sprite color and status effects
 * Preserve the original icon state rendering on SDL destinations
 * ============================================================================ */

// Reuse the original RLE effect blitters, with the SDL viewport translated
// to sprite coordinates. Their destination starts at the visible top-left.
template <typename Draw>
static void BltClippedSpriteEffect(spritectl_surface_t surface, POINT* point,
                                  CSprite* sprite, Draw draw)
{
    if (!surface || !surface->surface || !point || !sprite || !sprite->IsInit()) return;
    SDL_Surface* target = surface->surface;
    if (target->format->format != SDL_PIXELFORMAT_RGB565 || target->pitch <= 0 || target->pitch > 0xffff) return;
    const SDL_Rect& viewport = target->clip_rect;
    RECT clip = {
        SDL_max(0, viewport.x - point->x),
        SDL_max(0, viewport.y - point->y),
        SDL_min(sprite->GetWidth(), viewport.x + viewport.w - point->x),
        SDL_min(sprite->GetHeight(), viewport.y + viewport.h - point->y)
    };
    if (clip.left >= clip.right || clip.top >= clip.bottom) return;
    if (!SpriteGpu::CpuAccess(surface, true)) return;
    if (SDL_MUSTLOCK(target) && SDL_LockSurface(target) != 0) return;
    WORD* dest = reinterpret_cast<WORD*>(static_cast<BYTE*>(target->pixels)
        + (point->y + clip.top) * target->pitch) + point->x + clip.left;
    draw(dest, static_cast<WORD>(target->pitch), &clip);
    if (SDL_MUSTLOCK(target)) SDL_UnlockSurface(target);
}

static bool BltGpuSpriteEffect(spritectl_surface_t surface, POINT* point, CSprite* sprite,
	SpriteGpuEffects::Effect effect, int value = 0)
{
	if (!SpriteGpuEffects::Active() || !point || !sprite || !sprite->IsInit()) return false;
	Uint16 gradation[94]{};
	if (effect == SpriteGpuEffects::Effect::Gradation) {
		if (value < 0 || value >= MAX_COLORSET) effect = SpriteGpuEffects::Effect::Copy;
		else for (int i = 0; i < 94; ++i) {
			const int index = CIndexSprite::ColorToGradation[SDL_min(i, MAX_COLOR_TO_GRADATION - 1)];
			gradation[i] = CIndexSprite::ColorSet[value][SDL_min(index, MAX_COLORGRADATION - 1)];
		}
	}
	return SpriteGpu::DrawEffect(surface, point->x, point->y, get_backend_sprite(sprite), effect, value, gradation);
}

void CSpriteSurface::BltSpriteColor(POINT* pPoint, CSprite* pSprite, BYTE rgb) {
	if (BltGpuSpriteEffect(m_backend_surface, pPoint, pSprite, SpriteGpuEffects::Effect::Color, rgb)) {
		s_Value1 = rgb;
		return;
	}
    BltClippedSpriteEffect(m_backend_surface, pPoint, pSprite, [=](WORD* dest, WORD pitch, RECT* clip) {
        pSprite->BltColorClipWidth(dest, pitch, clip, rgb);
    });
}

void CSpriteSurface::BltSpriteDarkness(POINT* pPoint, CSprite* pSprite, BYTE DarkBits) {
	if (BltGpuSpriteEffect(m_backend_surface, pPoint, pSprite, SpriteGpuEffects::Effect::Darkness, DarkBits)) {
		s_Value1 = DarkBits;
		return;
	}
    BltClippedSpriteEffect(m_backend_surface, pPoint, pSprite, [=](WORD* dest, WORD pitch, RECT* clip) {
        pSprite->BltDarknessClipWidth(dest, pitch, clip, DarkBits);
    });
}

void CSpriteSurface::BltSpriteColorSet(POINT* pPoint, CSprite* pSprite, WORD colorSet) {
	if (BltGpuSpriteEffect(m_backend_surface, pPoint, pSprite, SpriteGpuEffects::Effect::Gradation, colorSet)) {
		s_Value1 = colorSet;
		return;
	}
    BltClippedSpriteEffect(m_backend_surface, pPoint, pSprite, [=](WORD* dest, WORD pitch, RECT* clip) {
        pSprite->BltColorSetClipWidth(dest, pitch, clip, colorSet);
    });
}

void CSpriteSurface::BltSpriteEffect(POINT* pPoint, CSprite* pSprite) {
	using Effect = SpriteGpuEffects::Effect;
	if (!s_pMemcpyEffectFunction || s_pMemcpyEffectFunction == memcpyEffectGrayScale
		|| s_pMemcpyEffectFunction == memcpyEffectGradation) {
		const Effect effect = !s_pMemcpyEffectFunction ? Effect::Copy
			: s_pMemcpyEffectFunction == memcpyEffectGrayScale ? Effect::GrayScale : Effect::Gradation;
		if (BltGpuSpriteEffect(m_backend_surface, pPoint, pSprite, effect, s_Value1)) return;
	}
    BltClippedSpriteEffect(m_backend_surface, pPoint, pSprite, [=](WORD* dest, WORD pitch, RECT* clip) {
        pSprite->BltEffectClipWidth(dest, pitch, clip);
    });
}

void CSpriteSurface::BltSpriteAlpha4444SmallNotTrans(POINT* pPoint, CSprite* pSprite, BYTE alpha, BYTE shift) {
	(void)shift;
	/* TODO: Implement */
	BltSpriteAlpha(pPoint, pSprite, alpha);
}

void CSpriteSurface::BltSpriteAlpha4444NotTrans(POINT* pPoint, CSprite* pSprite, BYTE alpha) {
	/* TODO: Implement */
	BltSpriteAlpha(pPoint, pSprite, alpha);
}

void CSpriteSurface::BltSprite1555SmallNotTrans(POINT* pPoint, CSprite* pSprite, BYTE shift) {
	(void)shift;
	/* TODO: Implement */
	BltSprite(pPoint, pSprite);
}

void CSpriteSurface::BltSprite1555NotTrans(POINT* pPoint, CSprite* pSprite) {
	/* TODO: Implement */
	BltSprite(pPoint, pSprite);
}

/* ============================================================================
 * Clipping a sprite against the surface
 *
 * See the declaration in CSpriteSurface.h. The visible rectangle is in
 * sprite coordinates, and pDest is the surface pixel its top-left maps
 * to, which is where every CSpritePal / CAlphaSpritePal clip variant
 * expects to start drawing.
 * ============================================================================ */
CSpriteSurface::SPRITE_CLIP
CSpriteSurface::ClipSpriteToSurface(int x, int y, int width, int height,
									int surfaceWidth, int surfaceHeight,
									RECT* pRect, POINT* pDest)
{
	if (pRect == NULL || pDest == NULL)
		return SPRITE_CLIP_OUTSIDE;

	if (width <= 0 || height <= 0 || surfaceWidth <= 0 || surfaceHeight <= 0)
		return SPRITE_CLIP_OUTSIDE;

	if (x >= surfaceWidth || y >= surfaceHeight || x + width <= 0 || y + height <= 0)
		return SPRITE_CLIP_OUTSIDE;

	int	left	= (x < 0) ? -x : 0;
	int	top		= (y < 0) ? -y : 0;
	int	right	= (x + width  > surfaceWidth)  ? surfaceWidth  - x : width;
	int	bottom	= (y + height > surfaceHeight) ? surfaceHeight - y : height;

	pRect->left		= left;
	pRect->top		= top;
	pRect->right	= right;
	pRect->bottom	= bottom;

	pDest->x		= x + left;
	pDest->y		= y + top;

	bool	leftClip	= (left > 0);
	bool	rightClip	= (right < width);

	if (leftClip && rightClip)
		return SPRITE_CLIP_WIDTH;
	if (leftClip)
		return SPRITE_CLIP_LEFT;
	if (rightClip)
		return SPRITE_CLIP_RIGHT;
	if (top > 0 || bottom < height)
		return SPRITE_CLIP_HEIGHT;

	return SPRITE_CLIP_NONE;
}

/*
 * The clip variants take their pitch as a WORD. A locked surface wider
 * than 32767 pixels would not fit; nothing here is, but the cast must
 * not silently wrap.
 */
static bool PitchFitsBlt(int pitch)
{
	return pitch > 0 && pitch <= 0xFFFF;
}

/* ============================================================================
 * BltSpritePalEffectTo
 *
 * Draws a palette sprite through the effect routine SetPalEffect
 * selected - the screen blend, for every effect in the effect sprite
 * table with BltType BLT_SCREEN - into a locked pixel buffer, clipped to
 * the buffer's edges.
 * ============================================================================ */
void CSpriteSurface::BltSpritePalEffectTo(WORD* pPixels, int pitch,
										int surfaceWidth, int surfaceHeight,
										const POINT* pPoint, CSpritePal* pSprite, MPalette &pal)
{
	if (pPixels == NULL || pPoint == NULL || pSprite == NULL || pSprite->IsNotInit())
		return;

	if (!PitchFitsBlt(pitch))
		return;

	RECT	rect;
	POINT	dest;

	SPRITE_CLIP	clip = ClipSpriteToSurface(pPoint->x, pPoint->y,
										pSprite->GetWidth(), pSprite->GetHeight(),
										surfaceWidth, surfaceHeight, &rect, &dest);

	if (clip == SPRITE_CLIP_OUTSIDE)
		return;

	WORD*	pDest	= (WORD*)((BYTE*)pPixels + dest.y * pitch + (dest.x << 1));
	WORD	wPitch	= (WORD)pitch;

	switch (clip)
	{
		case SPRITE_CLIP_NONE:		pSprite->BltEffect(pDest, wPitch, pal);						break;
		case SPRITE_CLIP_HEIGHT:	pSprite->BltEffectClipHeight(pDest, wPitch, &rect, pal);	break;
		case SPRITE_CLIP_LEFT:		pSprite->BltEffectClipLeft(pDest, wPitch, &rect, pal);		break;
		case SPRITE_CLIP_RIGHT:		pSprite->BltEffectClipRight(pDest, wPitch, &rect, pal);		break;
		case SPRITE_CLIP_WIDTH:		pSprite->BltEffectClipWidth(pDest, wPitch, &rect, pal);		break;
		default:																				break;
	}
}

/* ============================================================================
 * BltAlphaSpritePalTo
 *
 * The alpha-blended palette sprite, clipped the same way. Until this
 * existed the member blit refused any sprite that crossed a surface
 * edge, which is how a tall bolt from the sky vanished near the top of
 * the screen.
 * ============================================================================ */
void CSpriteSurface::BltAlphaSpritePalTo(WORD* pPixels, int pitch,
										int surfaceWidth, int surfaceHeight,
										const POINT* pPoint, CAlphaSpritePal* pSprite, MPalette &pal)
{
	if (pPixels == NULL || pPoint == NULL || pSprite == NULL || pSprite->IsNotInit())
		return;

	if (!PitchFitsBlt(pitch))
		return;

	RECT	rect;
	POINT	dest;

	SPRITE_CLIP	clip = ClipSpriteToSurface(pPoint->x, pPoint->y,
										pSprite->GetWidth(), pSprite->GetHeight(),
										surfaceWidth, surfaceHeight, &rect, &dest);

	if (clip == SPRITE_CLIP_OUTSIDE)
		return;

	WORD*	pDest	= (WORD*)((BYTE*)pPixels + dest.y * pitch + (dest.x << 1));
	WORD	wPitch	= (WORD)pitch;

	switch (clip)
	{
		case SPRITE_CLIP_NONE:		pSprite->Blt(pDest, wPitch, pal);					break;
		case SPRITE_CLIP_HEIGHT:	pSprite->BltClipHeight(pDest, wPitch, &rect, pal);	break;
		case SPRITE_CLIP_LEFT:		pSprite->BltClipLeft(pDest, wPitch, &rect, pal);	break;
		case SPRITE_CLIP_RIGHT:		pSprite->BltClipRight(pDest, wPitch, &rect, pal);	break;
		case SPRITE_CLIP_WIDTH:		pSprite->BltClipWidth(pDest, wPitch, &rect, pal);	break;
		default:																		break;
	}
}

/* ============================================================================
 * BltSpritePalEffect
 *
 * The BLT_SCREEN effect path: MTopView's DRAW_NORMALSPRITEPAL_EFFECT
 * selects the effect and calls this. It was an empty stub from the SDL
 * port until 2026-09-04, which is why every screen-blend skill effect
 * drew nothing.
 * ============================================================================ */
void CSpriteSurface::BltSpritePalEffect(POINT* pPoint, CSpritePal* pSprite, MPalette &pal) {
	if (!pPoint || !pSprite || pSprite->IsNotInit()) {
		return;
	}
	if ((!s_pMemcpyPalEffectFunction || s_pMemcpyPalEffectFunction == memcpyPalEffectScreen)
		&& SpriteGpu::DrawPalette(m_backend_surface, pPoint->x, pPoint->y, pSprite, pal,
			false, s_pMemcpyPalEffectFunction == memcpyPalEffectScreen)) return;

	spritectl_surface_info_t surface_info;
	if (spritectl_lock_surface(m_backend_surface, &surface_info) != 0) {
		static int lockFailCount = 0;
		if (lockFailCount < 3) {
			LOG_WARN("[BltSpritePalEffect] ERROR: Failed to lock surface\n");
			lockFailCount++;
		}
		return;
	}

	BltSpritePalEffectTo((WORD*)surface_info.pixels, surface_info.pitch,
						m_width, m_height, pPoint, pSprite, pal);

	spritectl_unlock_surface(m_backend_surface);
}

void CSpriteSurface::BltSpritePal1555SmallNotTrans(POINT* pPoint, CSpritePal* pSprite, BYTE shift, MPalette &pal) {
	(void)pPoint;
	(void)pSprite;
	(void)shift;
	(void)pal;
	/* TODO: Implement */
}

void CSpriteSurface::BltSpritePal1555NotTrans(POINT* pPoint, CSpritePal* pSprite, MPalette &pal) {
	(void)pPoint;
	(void)pSprite;
	(void)pal;
	/* TODO: Implement */
}

void CSpriteSurface::BltSpriteAlphaFilter(POINT* pPoint, CSprite* pSprite) {
	/* TODO: Implement alpha filter */
	BltSprite(pPoint, pSprite);
}

void CSpriteSurface::BltSpriteAlphaFilterDarkness(POINT* pPoint, CSprite* pSprite, BYTE DarkBits) {
	/* TODO: Implement */
	BltSpriteDarkness(pPoint, pSprite, DarkBits);
}

void CSpriteSurface::BltSpriteDarkerFilter(POINT* pPoint, CSprite* pSprite) {
	/* TODO: Implement darker filter */
	BltSprite(pPoint, pSprite);
}

/* ============================================================================
 * Alpha Sprite Methods
 * ============================================================================ */

void CSpriteSurface::BltAlphaSprite(POINT* pPoint, CAlphaSprite* pSprite) {
	if (!pPoint || !pSprite) {
		return;
	}

	/* Get backend sprite */
	spritectl_sprite_t backend_sprite = get_backend_alpha_sprite(pSprite);
	if (!backend_sprite) {
		return;
	}

	/* Blit to backend surface */
	int flags = SPRITECTL_BLT_ALPHA;
	int alpha = 255;
	spritectl_blt_sprite(m_backend_surface, pPoint->x, pPoint->y,
	                    backend_sprite, flags, alpha);
}

void CSpriteSurface::BltAlphaSpriteAlpha(POINT* pPoint, CAlphaSprite* pSprite, BYTE alpha) {
	if (!pPoint || !pSprite) {
		return;
	}

	/* Get backend sprite */
	spritectl_sprite_t backend_sprite = get_backend_alpha_sprite(pSprite);
	if (!backend_sprite) {
		return;
	}

	/* Blit to backend surface with alpha */
	int flags = SPRITECTL_BLT_ALPHA;
	spritectl_blt_sprite(m_backend_surface, pPoint->x, pPoint->y,
	                    backend_sprite, flags, alpha);
}

void CSpriteSurface::BltAlphaSprite4444(POINT* pPoint, CAlphaSprite* pSprite) {
	/* TODO: Implement 4444 format conversion */
	/* For now, use regular blit */
	BltAlphaSprite(pPoint, pSprite);
}

void CSpriteSurface::BltAlphaSprite4444NotTrans(POINT* pPoint, CAlphaSprite* pSprite) {
	/* TODO: Implement 4444 NotTrans */
	BltAlphaSprite(pPoint, pSprite);
}

void CSpriteSurface::BltAlphaSprite4444SmallNotTrans(POINT* pPoint, CAlphaSprite* pSprite, BYTE shift) {
	(void)shift;
	/* TODO: Implement scaling */
	BltAlphaSprite(pPoint, pSprite);
}

void CSpriteSurface::BltAlphaSpritePal(POINT* pPoint, CAlphaSpritePal* pSprite, MPalette &pal) {
	if (!pPoint || !pSprite) {
		LOG_ERROR("[BltAlphaSpritePal] ERROR: Invalid parameters (pPoint=%p, pSprite=%p)\n", pPoint, pSprite);
		return;
	}

	/* Check if sprite is initialized */
	if (pSprite->IsNotInit()) {
		LOG_ERROR("[BltAlphaSpritePal] ERROR: Sprite not initialized\n");
		return;
	}

	if (SpriteGpu::DrawPalette(m_backend_surface, pPoint->x, pPoint->y, pSprite, pal, true, false)) return;
	/* Lock backend surface for direct pixel access */
	spritectl_surface_info_t surface_info;
	if (spritectl_lock_surface(m_backend_surface, &surface_info) != 0) {
		static int lockFailCount = 0;
		if (lockFailCount < 3) {
			LOG_WARN("[BltAlphaSpritePal] ERROR: Failed to lock surface\n");
			lockFailCount++;
		}
		return;
	}

	/* Clipped to the surface; a sprite crossing an edge draws its visible part */
	BltAlphaSpritePalTo((WORD*)surface_info.pixels, surface_info.pitch,
						m_width, m_height, pPoint, pSprite, pal);

	/* Unlock surface */
	spritectl_unlock_surface(m_backend_surface);
}

void CSpriteSurface::BltAlphaSpritePalAlpha(POINT* pPoint, CAlphaSpritePal* pSprite, BYTE alpha, MPalette &pal) {
	(void)pPoint;
	(void)pSprite;
	(void)alpha;
	(void)pal;
	/* TODO: Implement */
}

void CSpriteSurface::BltAlphaSpritePal4444(POINT* pPoint, CAlphaSpritePal* pSprite, MPalette &pal) {
	(void)pPoint;
	(void)pSprite;
	(void)pal;
	/* TODO: Implement */
}

void CSpriteSurface::BltAlphaSpritePal4444NotTrans(POINT* pPoint, CAlphaSpritePal* pSprite, MPalette &pal) {
	(void)pPoint;
	(void)pSprite;
	(void)pal;
	/* TODO: Implement */
}

void CSpriteSurface::BltAlphaSpritePal4444SmallNotTrans(POINT* pPoint, CAlphaSpritePal* pSprite, BYTE shift, MPalette &pal) {
	(void)pPoint;
	(void)pSprite;
	(void)shift;
	(void)pal;
	/* TODO: Implement */
}

/* ============================================================================
 * Index Sprite Methods
 * ============================================================================ */

void CSpriteSurface::BltIndexSprite(POINT* pPoint, CIndexSprite* pSprite) {
	if (!pPoint || !pSprite) {
		return;
	}

	/* Get backend sprite */
	spritectl_sprite_t backend_sprite = get_backend_index_sprite(pSprite);
	if (!backend_sprite) {
		return;
	}

	/* Blit to backend surface */
	int flags = 0;
	int alpha = 255;
	spritectl_blt_sprite(m_backend_surface, pPoint->x, pPoint->y,
	                    backend_sprite, flags, alpha);
}

void CSpriteSurface::BltIndexSpriteDarkness(POINT* pPoint, CIndexSprite* pSprite, BYTE DarkBits) {
	(void)DarkBits;
	/* TODO: Implement darkness effect */
	BltIndexSprite(pPoint, pSprite);
}

void CSpriteSurface::BltIndexSpriteAlpha(POINT* pPoint, CIndexSprite* pSprite, BYTE alpha) {
	if (!pPoint || !pSprite) {
		return;
	}

	/* Get backend sprite */
	spritectl_sprite_t backend_sprite = get_backend_index_sprite(pSprite);
	if (!backend_sprite) {
		return;
	}

	/* Blit to backend surface with alpha */
	int flags = SPRITECTL_BLT_ALPHA;
	spritectl_blt_sprite(m_backend_surface, pPoint->x, pPoint->y,
	                    backend_sprite, flags, alpha);
}

void CSpriteSurface::BltIndexSpriteColor(POINT* pPoint, CIndexSprite* pSprite, BYTE rgb) {
	(void)rgb;
	/* TODO: Implement color tinting */
	BltIndexSprite(pPoint, pSprite);
}

void CSpriteSurface::BltIndexSpriteColorSet(POINT* pPoint, CIndexSprite* pSprite, WORD colorSet) {
	(void)colorSet;
	/* TODO: Implement color set */
	BltIndexSprite(pPoint, pSprite);
}

void CSpriteSurface::BltIndexSpriteEffect(POINT* pPoint, CIndexSprite* pSprite) {
	if (!CIndexSprite::IsWipeOutEffect()) {
		BltIndexSprite(pPoint, pSprite);
		return;
	}
	if (!pPoint || !pSprite || !pSprite->IsInit() || !m_backend_surface
		|| !m_backend_surface->surface || s_Value1 >= 64) return;
	const SDL_Rect& clip = m_backend_surface->surface->clip_rect;
	if (int64_t(pPoint->x) + pSprite->GetWidth() <= clip.x
		|| int64_t(pPoint->y) + pSprite->GetHeight() <= clip.y
		|| int64_t(pPoint->x) >= int64_t(clip.x) + clip.w
		|| int64_t(pPoint->y) >= int64_t(clip.y) + clip.h) return;

	// Keep the legacy center-out wipe for each indexed/fixed color run.
	// Encode the surviving pixels as ordinary RLE so both CPU and GPU
	// composition preserve holes and opaque black, without a GPU readback.
	std::unique_ptr<spritectl_sprite_s, decltype(&spritectl_destroy_sprite)> wiped(
		spritectl_create_sprite_rle(pSprite->GetWidth(), pSprite->GetHeight()), spritectl_destroy_sprite);
	if (!wiped) return;
	const int wipe = (std::max)(0, s_Value1);
	std::vector<WORD> row;
	for (int y = 0; y < pSprite->GetHeight(); ++y) {
		row.assign(1, 0);
		const WORD* source = pSprite->GetPixelLine(y);
		const int segments = *source++;
		int x = 0, written = 0;
		auto append = [&](int count, bool indexed) {
			const int skipped = count * wipe / 64;
			const int left = (count - skipped) / 2;
			for (int side = 0; side < 2; ++side) {
				const int begin = side == 0 ? 0 : left + skipped;
				const int end = side == 0 ? left : count;
				if (begin == end) continue;
				++row[0];
				row.push_back(WORD(x + begin - written));
				row.push_back(WORD(end - begin));
				for (int i = begin; i < end; ++i) {
					const WORD pixel = source[i];
					row.push_back(indexed ? CIndexSprite::ColorSet[
						CIndexSprite::GetUsingColorSet(BYTE(pixel >> 8))][pixel & 0xff] : pixel);
				}
				written = x + end;
			}
			source += count;
			x += count;
		};
		for (int segment = 0; segment < segments; ++segment) {
			x += *source++;
			const int indexed = *source++;
			append(indexed, true);
			const int fixed = *source++;
			append(fixed, false);
		}
		if (row.size() > 0xffff || spritectl_sprite_set_scanline_rle(
			wiped.get(), y, row.data(), static_cast<uint16_t>(row.size())) != 0) return;
	}
	spritectl_blt_sprite(m_backend_surface, pPoint->x, pPoint->y, wiped.get(), 0, 255);
}

void CSpriteSurface::BltIndexSpriteBrightness(POINT* pPoint, CIndexSprite* pSprite, BYTE BrightBits) {
	(void)BrightBits;
	/* TODO: Implement brightness */
	BltIndexSprite(pPoint, pSprite);
}

/* ============================================================================
 * Shadow Sprite Methods
 * ============================================================================ */

void CSpriteSurface::BltShadowSprite(POINT* pPoint, CShadowSprite* pSprite) {
	if (!pPoint || !pSprite) {
		return;
	}

	/* Get backend sprite */
	spritectl_sprite_t backend_sprite = get_backend_shadow_sprite(pSprite);
	if (!backend_sprite) {
		return;
	}

	/* Blit to backend surface */
	int flags = 0;
	int alpha = 255;
	spritectl_blt_sprite(m_backend_surface, pPoint->x, pPoint->y,
	                    backend_sprite, flags, alpha);
}

void CSpriteSurface::BltShadowSpriteSmall(POINT* pPoint, CShadowSprite* pSprite, BYTE shift) {
	(void)shift;
	/* TODO: Implement scaling */
	BltShadowSprite(pPoint, pSprite);
}

void CSpriteSurface::BltShadowSpriteDarkness(POINT* pPoint, CShadowSprite* pSprite, BYTE DarkBits) {
	(void)DarkBits;
	/* TODO: Implement darkness effect */
	BltShadowSprite(pPoint, pSprite);
}

void CSpriteSurface::BltShadowSprite4444(POINT* pPoint, CShadowSprite* pSprite, WORD pixel) {
	(void)pixel;
	/* TODO: Implement 4444 format */
	BltShadowSprite(pPoint, pSprite);
}

void CSpriteSurface::BltShadowSpriteSmall4444(POINT* pPoint, CShadowSprite* pSprite, WORD pixel, BYTE shift) {
	(void)pixel;
	(void)shift;
	/* TODO: Implement scaling + 4444 */
	BltShadowSprite(pPoint, pSprite);
}

/* ============================================================================
 * Sprite Outline Methods
 * ============================================================================ */

void CSpriteSurface::BltSpriteOutline(CSpriteOutlineManager *pSOM, WORD color) {
	(void)pSOM;
	(void)color;
	/* TODO: Implement sprite outline */
}

void CSpriteSurface::BltSpriteOutlineOnly(CSpriteOutlineManager* pSOM, WORD color) {
	(void)pSOM;
	(void)color;
	/* TODO: Implement */
}

void CSpriteSurface::BltSpriteOutlineDarkness(CSpriteOutlineManager* pSOM, WORD color, BYTE DarkBits) {
	(void)pSOM;
	(void)color;
	(void)DarkBits;
	/* TODO: Implement */
}
