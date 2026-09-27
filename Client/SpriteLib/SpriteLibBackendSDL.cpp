/*-----------------------------------------------------------------------------

	SpriteLibBackendSDL.cpp

	SDL2 backend implementation for SpriteLib.
	Implements the unified backend interface using SDL2.

	2025.01.14

-----------------------------------------------------------------------------*/

#include "SpriteLibBackendSDL.h"
#include "CrtCompat.h"
#include "FrameUpscaler.h"
#include "SpriteGpu.h"
#include "SpriteScanline.h"
#include "DataPath.h"
#include <climits>
#include <limits>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Global State
 * ============================================================================ */

static int g_spritectl_initialized = 0;
static SDL_Renderer* g_spritectl_default_renderer = NULL;
static FrameUpscaler g_frame_upscaler;
static bool g_xbrz_enabled = true;

/* Reused scratch surface for spritectl_blt_sprite()'s non-RLE fallback path
 * (see below) - grows to the largest sprite blitted so far instead of being
 * allocated/freed on every single call. */
static SDL_Surface* g_spritectl_blt_scratch = NULL;
static int g_spritectl_blt_scratch_w = 0;
static int g_spritectl_blt_scratch_h = 0;

/* ============================================================================
 * Initialization
 * ============================================================================ */

int spritectl_init(void) {
	if (g_spritectl_initialized) {
		return 0;  /* Already initialized */
	}

	/* Initialize SDL video subsystem */
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "SpriteLib Backend: SDL_Init failed: %s\n", SDL_GetError());
		return -1;
	}

	g_spritectl_initialized = 1;
	return 0;
}

void spritectl_shutdown(void) {
	if (!g_spritectl_initialized) {
		return;
	}

	/* Clean up default renderer */
	spritectl_release_present_resources(NULL);
	if (g_spritectl_default_renderer) {
		SDL_DestroyRenderer(g_spritectl_default_renderer);
		g_spritectl_default_renderer = NULL;
	}

	if (g_spritectl_blt_scratch) {
		SDL_FreeSurface(g_spritectl_blt_scratch);
		g_spritectl_blt_scratch = NULL;
		g_spritectl_blt_scratch_w = 0;
		g_spritectl_blt_scratch_h = 0;
	}

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	g_spritectl_initialized = 0;
}

/* ============================================================================
 * Surface Functions
 * ============================================================================ */

SDL_Surface* spritectl_sdl_create_surface(int width, int height, int format) {
	SDL_Surface* surf = NULL;

	/* Calculate depth and masks */
	switch (format) {
		case SPRITECTL_FORMAT_RGB565:
			/* RGB565: R at bits 11-15, G at bits 5-10, B at bits 0-4 */
			surf = SDL_CreateRGBSurface(0, width, height, 16,
			                            0xF800,  /* R mask */
			                            0x07E0,  /* G mask */
			                            0x001F,  /* B mask */
			                            0x0000); /* A mask */
			if (surf) {
				static int debug_565_count = 0;
				if (debug_565_count < 3) {
					fprintf(stderr, "Created RGB565 surface: requested format=RGB565, actual format=%s\n",
						SDL_GetPixelFormatName(surf->format->format));
					debug_565_count++;
				}
			}
			return surf;
		case SPRITECTL_FORMAT_RGB555:
			/* RGB555: R at bits 10-14, G at bits 5-9, B at bits 0-4 */
			surf = SDL_CreateRGBSurface(0, width, height, 16,
			                            0x7C00,  /* R mask */
			                            0x03E0,  /* G mask */
			                            0x001F,  /* B mask */
			                            0x0000); /* A mask */
			if (surf) {
				static int debug_555_count = 0;
				if (debug_555_count < 3) {
					fprintf(stderr, "Created RGB555 surface: requested format=RGB555, actual format=%s\n",
						SDL_GetPixelFormatName(surf->format->format));
					debug_555_count++;
				}
			}
			return surf;
		case SPRITECTL_FORMAT_RGBA32:
			/* RGBA32: standard byte order */
			return SDL_CreateRGBSurface(0, width, height, 32,
			                            0x000000FF,  /* R mask */
			                            0x0000FF00,  /* G mask */
			                            0x00FF0000,  /* B mask */
			                            0xFF000000); /* A mask */
		default:
			/* Default to RGB565 */
			surf = SDL_CreateRGBSurface(0, width, height, 16,
			                            0xF800,  /* R mask */
			                            0x07E0,  /* G mask */
			                            0x001F,  /* B mask */
			                            0x0000); /* A mask */
			if (surf) {
				fprintf(stderr, "Created DEFAULT surface: actual format=%s\n",
					SDL_GetPixelFormatName(surf->format->format));
			}
			return surf;
	}
}

uint32_t spritectl_sdl_get_pixelformat(int format) {
	switch (format) {
		case SPRITECTL_FORMAT_RGB565:
			return SDL_PIXELFORMAT_RGB565;
		case SPRITECTL_FORMAT_RGB555:
			return SDL_PIXELFORMAT_RGB555;
		case SPRITECTL_FORMAT_RGBA32:
			return SDL_PIXELFORMAT_RGBA32;
		default:
			return SDL_PIXELFORMAT_UNKNOWN;
	}
}

spritectl_surface_t spritectl_create_surface(int width, int height, int format) {
	spritectl_surface_t surface;

	if (!g_spritectl_initialized) {
		fprintf(stderr, "SpriteLib Backend: Not initialized\n");
		return SPRITECTL_INVALID_SURFACE;
	}

	/* Allocate surface structure */
	surface = (spritectl_surface_t)malloc(sizeof(struct spritectl_surface_s));
	if (!surface) {
		fprintf(stderr, "SpriteLib Backend: Failed to allocate surface\n");
		return SPRITECTL_INVALID_SURFACE;
	}

	/* Create SDL surface */
	surface->surface = spritectl_sdl_create_surface(width, height, format);
	if (!surface->surface) {
		free(surface);
		return SPRITECTL_INVALID_SURFACE;
	}

	/* Initialize fields */
	surface->texture = NULL;
	surface->renderer = NULL;
	surface->width = width;
	surface->height = height;
	surface->format = format;
	surface->locked = 0;
	surface->ref_count = 1;
	surface->cpu_borrowed = 0;
	surface->transient = 0;

	return surface;
}

void spritectl_destroy_surface(spritectl_surface_t surface) {
	if (!surface) {
		return;
	}

	surface->ref_count--;
	if (surface->ref_count > 0) {
		return;  /* Still referenced */
	}

	SpriteGpu::ForgetSurface(surface);
	/* Free SDL resources */
	if (surface->texture) {
		SDL_DestroyTexture(surface->texture);
	}
	if (surface->surface) {
		SDL_FreeSurface(surface->surface);
	}

	free(surface);
}

int spritectl_lock_surface(spritectl_surface_t surface, spritectl_surface_info_t* info) {
	if (!surface || !info) {
		return -1;
	}

	if (!SpriteGpu::CpuAccess(surface, true)) return -1;
	/* Lock SDL surface */
	if (SDL_LockSurface(surface->surface) != 0) {
		fprintf(stderr, "SpriteLib Backend: Failed to lock surface: %s\n", SDL_GetError());
		return -1;
	}

	surface->locked++;

	/* Fill info structure */
	info->width = surface->width;
	info->height = surface->height;
	info->pitch = surface->surface->pitch;
	info->pixels = surface->surface->pixels;
	info->format = surface->format;

	return 0;
}

int spritectl_unlock_surface(spritectl_surface_t surface) {
	if (!surface || surface->locked == 0) {
		return -1;
	}

	SDL_UnlockSurface(surface->surface);
	surface->locked--;

	return 0;
}

int spritectl_clear_surface(spritectl_surface_t surface, uint32_t color) {
	SDL_Rect rect;

	if (!surface) {
		return -1;
	}

	if (SpriteGpu::Fill(surface, nullptr, color)) return 0;
	if (!SpriteGpu::CpuAccess(surface, true)) return -1;
	/* Fill entire surface with color */
	rect.x = 0;
	rect.y = 0;
	rect.w = surface->width;
	rect.h = surface->height;

	SDL_FillRect(surface->surface, &rect, (Uint32)color);

	return 0;
}

int spritectl_get_surface_size(spritectl_surface_t surface, int* width, int* height) {
	if (!surface) {
		return -1;
	}

	if (width) *width = surface->width;
	if (height) *height = surface->height;

	return 0;
}

/* ============================================================================
 * Sprite Functions
 * ============================================================================ */

spritectl_sprite_t spritectl_create_sprite(int width, int height, int format,
                                           const void* pixels, size_t data_size) {
	spritectl_sprite_t sprite;

	const size_t bytesPerPixel = format == SPRITECTL_FORMAT_RGBA32 ? 4 :
		(format == SPRITECTL_FORMAT_RGB565 || format == SPRITECTL_FORMAT_RGB555 ? 2 : 0);
	if (!pixels || width <= 0 || height <= 0 || bytesPerPixel == 0
		|| width > INT_MAX / height)
		return SPRITECTL_INVALID_SPRITE;
	const size_t pixelCount = size_t(width) * size_t(height);
	if (pixelCount > (std::numeric_limits<size_t>::max)() / bytesPerPixel
		|| data_size < pixelCount * bytesPerPixel)
		return SPRITECTL_INVALID_SPRITE;

	/* Allocate sprite structure */
	sprite = (spritectl_sprite_t)malloc(sizeof(struct spritectl_sprite_s));
	if (!sprite) {
		fprintf(stderr, "SpriteLib Backend: Failed to allocate sprite\n");
		return SPRITECTL_INVALID_SPRITE;
	}

	/* Initialize fields */
	sprite->width = width;
	sprite->height = height;
	sprite->format = format;
	sprite->data_size = data_size;
	sprite->rgba_pixels = NULL;
	sprite->scanline_rle = NULL;
	sprite->scanline_lens = NULL;
	sprite->has_rle = 0;
	sprite->ref_count = 1;

	/* Allocate and copy pixel data */
	sprite->pixels = (uint16_t*)malloc(data_size);
	if (!sprite->pixels) {
		free(sprite);
		return SPRITECTL_INVALID_SPRITE;
	}
	memcpy(sprite->pixels, pixels, data_size);

	return sprite;
}

void spritectl_destroy_sprite(spritectl_sprite_t sprite) {
	if (!sprite) {
		return;
	}

	sprite->ref_count--;
	if (sprite->ref_count > 0) {
		return;  /* Still referenced */
	}

	SpriteGpu::ForgetSprite(sprite);
	/* Free pixel data */
	if (sprite->pixels) {
		free(sprite->pixels);
	}
	if (sprite->rgba_pixels) {
		free(sprite->rgba_pixels);
	}

	/* Free RLE data */
	if (sprite->scanline_rle) {
		for (int y = 0; y < sprite->height; y++) {
			if (sprite->scanline_rle[y]) {
				free(sprite->scanline_rle[y]);
			}
		}
		free(sprite->scanline_rle);
	}
	if (sprite->scanline_lens) {
		free(sprite->scanline_lens);
	}

	free(sprite);
}

int spritectl_get_sprite_info(spritectl_sprite_t sprite, spritectl_sprite_info_t* info) {
	if (!sprite || !info) {
		return -1;
	}

	info->width = sprite->width;
	info->height = sprite->height;
	info->format = sprite->format;
	info->data_size = sprite->data_size;

	return 0;
}

size_t spritectl_get_sprite_data(spritectl_sprite_t sprite, void* buffer, size_t buffer_size) {
	if (!sprite) {
		return 0;
	}

	if (!buffer) {
		return sprite->data_size;  /* Return required size */
	}

	if (buffer_size < sprite->data_size) {
		return 0;  /* Buffer too small */
	}

	memcpy(buffer, sprite->pixels, sprite->data_size);
	return sprite->data_size;
}

/* ============================================================================
 * Blitting Functions
 * ============================================================================ */

/**
 * RLE-based sprite rendering - mimics original DirectX BltClipWidth
 *
 * This function directly processes RLE data and writes only non-transparent
 * pixels to the destination surface, exactly like the original DirectX implementation.
 *
 * Key difference from SDL blitting:
 * - Transparent pixels are SKIPPED, not blended
 * - This means the destination surface's original content shows through
 */
int spritectl_blt_sprite_rle(spritectl_surface_t dest, int x, int y,
                              spritectl_sprite_t sprite, int flags, int alpha) {
	if (!dest || !sprite || !sprite->has_rle || !sprite->scanline_rle
		|| !sprite->scanline_lens || sprite->width <= 0 || sprite->height <= 0) {
		return -1;
	}

	SDL_Surface* sdl_surface = dest->surface;
	if (!sdl_surface) {
		return -1;
	}

	/* Get destination surface info */
	int dest_width = sdl_surface->w;
	int dest_height = sdl_surface->h;
	int dest_pitch = sdl_surface->pitch;
	int dest_bytes_per_pixel = sdl_surface->format->BytesPerPixel;
	int dest_stride = dest_pitch / dest_bytes_per_pixel;  /* Number of pixels per row */

    // Direct RLE writes bypass SDL_BlitSurface, so apply the same destination
    // clip explicitly. SDL_SetClipRect already intersects it with the surface.
    const SDL_Rect& viewport = sdl_surface->clip_rect;
	const int64_t clip_left = SDL_max(int64_t(0), int64_t(viewport.x) - x);
	const int64_t clip_top = SDL_max(int64_t(0), int64_t(viewport.y) - y);
	const int64_t clip_right = SDL_min(int64_t(sprite->width), int64_t(viewport.x) + viewport.w - x);
	const int64_t clip_bottom = SDL_min(int64_t(sprite->height), int64_t(viewport.y) + viewport.h - y);
	if (clip_left >= clip_right || clip_top >= clip_bottom)
		return 0;

	// Validate all visible rows before the first write, including later segment
	// headers after earlier segments have consumed their color data.
	for (int sy = int(clip_top); sy < clip_bottom; ++sy) {
		const size_t length = sprite->scanline_lens[sy];
		if (length && (!sprite->scanline_rle[sy]
			|| !ValidateSpriteScanline({sprite->scanline_rle[sy], length}, sprite->width)))
			return -1;
	}
	if (SpriteGpu::Draw(dest, x, y, sprite, flags, alpha)) return 0;
	if (!SpriteGpu::CpuAccess(dest, true) || SDL_LockSurface(sdl_surface) != 0) return -1;

	/* Debug: print surface info on first call */
	static int debug_printed = 0;
	if (debug_printed < 3) {
		fprintf(stderr, "[RLE BLT] sprite=%dx%d, pos=(%d,%d), surface=%dx%d, bpp=%d, pitch=%d, stride=%d\n",
		        sprite->width, sprite->height, x, y, dest_width, dest_height, dest_bytes_per_pixel, dest_pitch, dest_stride);
		debug_printed++;
	}

	/* Process each scanline */
	for (int sy = int(clip_top); sy < clip_bottom; sy++) {
		if (!sprite->scanline_rle[sy] || sprite->scanline_lens[sy] == 0) {
			continue;  /* Empty scanline */
		}

		/* Get destination row pointer - use uint8_t* for format flexibility */
		uint8_t* dest_row_bytes = (uint8_t*)sdl_surface->pixels + (y + sy) * dest_pitch;

		/* Process RLE segments */
		uint16_t* rle_data = sprite->scanline_rle[sy];
		uint16_t rle_data_size = sprite->scanline_lens[sy];
		int rle_index = 0;

		/* Validate RLE data */
		if (rle_data_size < 1) {
			if (debug_printed < 3) {
				fprintf(stderr, "[RLE BLT] Invalid RLE data size: %d at scanline %d\n", rle_data_size, sy);
			}
			continue;
		}

		int seg_count = rle_data[rle_index++];
		if (rle_index + seg_count * 2 > rle_data_size) {
			if (debug_printed < 3) {
				fprintf(stderr, "[RLE BLT] RLE data corruption: seg_count=%d, size=%d at scanline %d\n",
				        seg_count, rle_data_size, sy);
			}
			continue;
		}

		int sx = 0;

		for (int seg = 0; seg < seg_count; seg++) {
			int trans_count = rle_data[rle_index++];
			int color_count = rle_data[rle_index++];

			/* Skip transparent pixels */
			sx += trans_count;

			/* Safety: if we've gone past sprite width, skip this entire segment */
			if (sx >= sprite->width) {
				rle_index += color_count;  /* Skip all color pixels in this segment */
				continue;  /* Move to next segment */
			}

			/* Copy color pixels */
			for (int c = 0; c < color_count && sx < sprite->width; c++) {
				/* Check if we're about to read past RLE data */
				if (rle_index >= rle_data_size) {
					if (debug_printed < 3) {
						fprintf(stderr, "[RLE BLT] RLE data overrun: rle_index=%d, size=%d, sx=%d\n",
						        rle_index, rle_data_size, sx);
					}
					break;
				}

				/* Check if this pixel is within clipping bounds */
				if (sx >= clip_left && sx < clip_right) {
					uint16_t pixel = rle_data[rle_index];

					/* Calculate actual destination position */
					int dest_x = x + sx;
					if (dest_x >= 0 && dest_x < dest_stride) {
						/* Write pixel based on destination format */
						if (dest_bytes_per_pixel == 2) {
							/* RGB565 destination - write directly */
							uint16_t* dest_row_16 = (uint16_t*)dest_row_bytes;
							dest_row_16[dest_x] = pixel;
						} else if (dest_bytes_per_pixel == 4) {
							/* RGBA32 destination - convert RGB565 to RGBA32 */
							uint8_t r, g, b;
							spritectl_565_to_rgb(pixel, &r, &g, &b);
							uint32_t* dest_row_32 = (uint32_t*)dest_row_bytes;

							/* Apply alpha blending if needed */
							if (flags & SPRITECTL_BLT_ALPHA) {
								/* Alpha blending with destination */
								uint32_t dest_pixel = dest_row_32[dest_x];
								uint8_t dest_r = dest_pixel & 0xFF;
								uint8_t dest_g = (dest_pixel >> 8) & 0xFF;
								uint8_t dest_b = (dest_pixel >> 16) & 0xFF;

								/* Blend: src * alpha/255 + dst * (1 - alpha/255) */
								uint8_t blend_r = (r * alpha + dest_r * (255 - alpha)) / 255;
								uint8_t blend_g = (g * alpha + dest_g * (255 - alpha)) / 255;
								uint8_t blend_b = (b * alpha + dest_b * (255 - alpha)) / 255;

								dest_row_32[dest_x] = (255 << 24) | (blend_b << 16) | (blend_g << 8) | blend_r;
							} else {
								/* Opaque write */
								dest_row_32[dest_x] = (255 << 24) | (b << 16) | (g << 8) | r;
							}
						} else if (debug_printed < 3) {
							fprintf(stderr, "[RLE BLT] Unsupported dest BPP: %d\n", dest_bytes_per_pixel);
						}
					} else if (debug_printed < 3) {
						fprintf(stderr, "[RLE BLT] OUT OF BOUNDS: dest_x=%d, stride=%d, sx=%d, x=%d\n",
						        dest_x, dest_stride, sx, x);
					}
				}

				rle_index++;  /* Consume pixel even if clipped */
				sx++;
			}
		}
	}

	SDL_UnlockSurface(sdl_surface);
	return 0;
}

spritectl_sprite_t spritectl_create_sprite_rle(int width, int height) {
	spritectl_sprite_t sprite;

	if (width <= 0 || height <= 0) {
		return SPRITECTL_INVALID_SPRITE;
	}

	/* Allocate sprite structure */
	sprite = (spritectl_sprite_t)malloc(sizeof(struct spritectl_sprite_s));
	if (!sprite) {
		fprintf(stderr, "SpriteLib Backend: Failed to allocate RLE sprite\n");
		return SPRITECTL_INVALID_SPRITE;
	}

	/* Initialize fields */
	sprite->width = width;
	sprite->height = height;
	sprite->format = SPRITECTL_FORMAT_RGB565;  /* Assume RGB565 for RLE sprites */
	sprite->data_size = 0;
	sprite->rgba_pixels = NULL;
	sprite->pixels = NULL;  /* RLE sprites don't have decoded pixels */
	sprite->ref_count = 1;
	sprite->has_rle = 1;  /* Mark as RLE sprite */

	/* Allocate RLE arrays */
	sprite->scanline_lens = (uint16_t*)calloc(height, sizeof(uint16_t));
	if (!sprite->scanline_lens) {
		free(sprite);
		return SPRITECTL_INVALID_SPRITE;
	}

	sprite->scanline_rle = (uint16_t**)calloc(height, sizeof(uint16_t*));
	if (!sprite->scanline_rle) {
		free(sprite->scanline_lens);
		free(sprite);
		return SPRITECTL_INVALID_SPRITE;
	}

	return sprite;
}

int spritectl_sprite_set_scanline_rle(spritectl_sprite_t sprite, int y,
                                       const uint16_t* rle_data, int rle_size) {
	if (!sprite || !sprite->has_rle || !sprite->scanline_rle || !sprite->scanline_lens
		|| y < 0 || y >= sprite->height || !rle_data || rle_size <= 0 || rle_size > UINT16_MAX
		|| !ValidateSpriteScanline({rle_data, size_t(rle_size)}, sprite->width))
		return -1;

	uint16_t* replacement = static_cast<uint16_t*>(malloc(size_t(rle_size) * sizeof(uint16_t)));
	if (!replacement) return -1;
	memcpy(replacement, rle_data, size_t(rle_size) * sizeof(uint16_t));
	SpriteGpu::ForgetSprite(sprite);
	free(sprite->scanline_rle[y]);
	sprite->scanline_rle[y] = replacement;
	sprite->scanline_lens[y] = uint16_t(rle_size);
	return 0;
}

int spritectl_blt_sprite(spritectl_surface_t dest, int x, int y,
                         spritectl_sprite_t sprite, int flags, int alpha) {
	if (!dest || !sprite) {
		return -1;
	}

	/* If sprite has RLE data, use RLE-based rendering (like original DirectX) */
	if (sprite->has_rle && sprite->scanline_rle) {
		return spritectl_blt_sprite_rle(dest, x, y, sprite, flags, alpha);
	}
	if (SpriteGpu::Draw(dest, x, y, sprite, flags, alpha)) return 0;
	if (!SpriteGpu::CpuAccess(dest, true)) return -1;

	/* Fallback to old method for sprites without RLE data */
	static int fallback_count = 0;
	if (fallback_count < 3) {
		fprintf(stderr, "[SpriteLib] WARNING: Using fallback rendering for sprite without RLE data (sprite=%p, has_rle=%d)\n",
		        (void*)sprite, sprite->has_rle);
		fallback_count++;
	}

	/* Fallback to old method for sprites without RLE data */
	SDL_Rect dest_rect;
	SDL_Surface* src_surface = NULL;
	int result = -1;

	// CRITICAL: Destination surface must NOT be locked when blitting
	// Unlock before blit, then re-lock after to maintain expected state
	bool was_locked = (dest->locked > 0);
	int saved_lock_count = dest->locked;

	if (was_locked) {
		// Unlock the surface for blitting
		while (dest->locked > 0) {
			SDL_UnlockSurface(dest->surface);
			dest->locked--;
		}
	}

	/* NOTE: We do NOT lock the destination surface here.
	 * SDL_BlitSurface will handle any necessary locking internally.
	 * Locking the destination surface before blitting is incorrect and will cause errors.
	 */

	/* Reuse a persistent scratch surface sized to the largest sprite blitted
	 * so far, instead of creating/destroying a new SDL_Surface on every call.
	 * This fallback path runs for every non-RLE sprite - including all text
	 * glyphs (spritectl_create_sprite() always sets has_rle = 0) - so a
	 * text-heavy screen (e.g. the option dialog's Hotkey tab) was doing
	 * thousands of surface alloc/free cycles per second just from drawing
	 * its keybind list every frame. */
	if (!g_spritectl_blt_scratch ||
	    g_spritectl_blt_scratch_w < sprite->width ||
	    g_spritectl_blt_scratch_h < sprite->height) {
		if (g_spritectl_blt_scratch) {
			SDL_FreeSurface(g_spritectl_blt_scratch);
		}
		if (g_spritectl_blt_scratch_w < sprite->width) {
			g_spritectl_blt_scratch_w = sprite->width;
		}
		if (g_spritectl_blt_scratch_h < sprite->height) {
			g_spritectl_blt_scratch_h = sprite->height;
		}
		g_spritectl_blt_scratch = SDL_CreateRGBSurface(0, g_spritectl_blt_scratch_w, g_spritectl_blt_scratch_h, 32,
		                                              0xFF, 0xFF00, 0xFF0000, 0xFF000000);
		if (!g_spritectl_blt_scratch) {
			g_spritectl_blt_scratch_w = 0;
			g_spritectl_blt_scratch_h = 0;
			return -1;
		}
	}
	src_surface = g_spritectl_blt_scratch;

	/* Use cached RGBA pixels if available, otherwise convert and cache */
	if (sprite->rgba_pixels == NULL && sprite->format != SPRITECTL_FORMAT_RGBA32) {
		/* Convert and cache RGBA pixels for non-RGBA32 formats */
		sprite->rgba_pixels = (uint32_t*)malloc(sprite->width * sprite->height * sizeof(uint32_t));
		if (sprite->rgba_pixels) {
			if (sprite->format == SPRITECTL_FORMAT_RGB565) {
				spritectl_convert_565_to_rgba(sprite->pixels, sprite->rgba_pixels,
				                              sprite->width * sprite->height, 0x0000);
			} else if (sprite->format == SPRITECTL_FORMAT_RGB555) {
				spritectl_convert_555_to_rgba(sprite->pixels, sprite->rgba_pixels,
				                              sprite->width * sprite->height, 0x0000);
			} else {
				/* Unknown format, fill with opaque white */
				for (int i = 0; i < sprite->width * sprite->height; i++) {
					sprite->rgba_pixels[i] = 0xFFFFFFFF;
				}
			}
		}
	}

	/* Determine pixel source */
	const uint32_t* pixel_src;
	if (sprite->format == SPRITECTL_FORMAT_RGBA32) {
		pixel_src = (const uint32_t*)sprite->pixels;
	} else if (sprite->rgba_pixels) {
		pixel_src = sprite->rgba_pixels;
	} else {
		/* Fallback: fill with opaque white */
		static uint32_t* fallback_pixels = NULL;
		static int fallback_size = 0;
		if (!fallback_pixels || fallback_size < sprite->width * sprite->height) {
			free(fallback_pixels);
			fallback_pixels = (uint32_t*)malloc(sprite->width * sprite->height * sizeof(uint32_t));
			fallback_size = sprite->width * sprite->height;
			if (fallback_pixels) {
				for (int i = 0; i < sprite->width * sprite->height; i++) {
					fallback_pixels[i] = 0xFFFFFFFF;
				}
			}
		}
		pixel_src = fallback_pixels;
	}

	/* Copy into the top-left corner of the (possibly larger) scratch surface,
	 * row by row since its pitch may exceed sprite->width * 4. */
	SDL_LockSurface(src_surface);
	uint8_t* dst_row = (uint8_t*)src_surface->pixels;
	const uint8_t* src_row = (const uint8_t*)pixel_src;
	size_t row_bytes = (size_t)sprite->width * sizeof(uint32_t);
	for (int row = 0; row < sprite->height; row++) {
		memcpy(dst_row, src_row, row_bytes);
		dst_row += src_surface->pitch;
		src_row += row_bytes;
	}
	SDL_UnlockSurface(src_surface);

	/* Handle alpha blending. src_surface is now a reused scratch buffer (see
	 * above), so the alpha mod must be reset unconditionally every call -
	 * otherwise a non-alpha blit could inherit a stale mod value left over
	 * by an earlier alpha blit that shared the same buffer. */
	SDL_SetSurfaceAlphaMod(src_surface, (flags & SPRITECTL_BLT_ALPHA) ? (Uint8)alpha : 255);

	/* 关键: 始终启用混合模式以支持 alpha 通道 */
	/* 即使不是 alpha 模式，也需要 BLEND 模式来处理 colorkey 产生的透明像素 */
	if (SDL_SetSurfaceBlendMode(src_surface, SDL_BLENDMODE_BLEND) != 0) {
		fprintf(stderr, "[SpriteLib] SDL_SetSurfaceBlendMode failed: %s\n",
		        SDL_GetError());
	}

	/* Set up destination rectangle */
	dest_rect.x = x;
	dest_rect.y = y;
	dest_rect.w = sprite->width;
	dest_rect.h = sprite->height;

	/* Blit only the sprite-sized region of the scratch surface */
	SDL_Rect src_rect = {0, 0, sprite->width, sprite->height};
	if (SDL_BlitSurface(src_surface, &src_rect, dest->surface, &dest_rect) != 0) {
		fprintf(stderr, "SpriteLib Backend: SDL_BlitSurface failed: %s\n", SDL_GetError());
		result = -1;
	} else {
		result = 0;
	}

	/* NOTE: src_surface is the persistent scratch buffer (g_spritectl_blt_scratch) - do not free it here. */

	// Re-lock the surface if it was locked before (to maintain expected state)
	if (was_locked) {
		for (int i = 0; i < saved_lock_count; i++) {
			SDL_LockSurface(dest->surface);
			dest->locked++;
		}
	}

	return result;
}

int spritectl_blt_sprite_scaled(spritectl_surface_t dest, int x, int y,
                                spritectl_sprite_t sprite, int scale, int flags) {
	SDL_Surface* src_surface = NULL;
	SDL_Surface* scaled_surface = NULL;
	int result = -1;
	int scaled_width, scaled_height;

	if (!dest || !sprite) {
		return -1;
	}
	/* Calculate scaled dimensions (scale is in 1/256 units) */
	const int64_t wideWidth = int64_t(sprite->width) * scale / 256;
	const int64_t wideHeight = int64_t(sprite->height) * scale / 256;
	if (wideWidth > INT_MAX || wideHeight > INT_MAX) return -1;
	if (wideWidth <= 0 || wideHeight <= 0) return 0;
	scaled_width = int(wideWidth);
	scaled_height = int(wideHeight);
	if (SpriteGpu::Draw(dest, x, y, sprite, flags, 255, scale)) return 0;
	if (!SpriteGpu::CpuAccess(dest, true)) return -1;

	if (scaled_width <= 0 || scaled_height <= 0) {
		return 0;  /* Too small to see */
	}

	/* Create temporary surface from sprite pixels */
	src_surface = SDL_CreateRGBSurface(0, sprite->width, sprite->height, 32,
	                                  0xFF, 0xFF00, 0xFF0000, 0xFF000000);
	if (!src_surface) {
		return -1;
	}

	/* Use cached RGBA pixels if available, otherwise convert and cache */
	if (sprite->rgba_pixels == NULL && sprite->format != SPRITECTL_FORMAT_RGBA32) {
		/* Convert and cache RGBA pixels for non-RGBA32 formats */
		sprite->rgba_pixels = (uint32_t*)malloc(sprite->width * sprite->height * sizeof(uint32_t));
		if (sprite->rgba_pixels) {
			if (sprite->format == SPRITECTL_FORMAT_RGB565) {
				spritectl_convert_565_to_rgba(sprite->pixels, sprite->rgba_pixels,
				                              sprite->width * sprite->height, 0x0000);
			} else if (sprite->format == SPRITECTL_FORMAT_RGB555) {
				spritectl_convert_555_to_rgba(sprite->pixels, sprite->rgba_pixels,
				                              sprite->width * sprite->height, 0x0000);
			} else {
				/* Unknown format, fill with opaque white */
				for (int i = 0; i < sprite->width * sprite->height; i++) {
					sprite->rgba_pixels[i] = 0xFFFFFFFF;
				}
			}
		}
	}

	/* Determine pixel source */
	const uint32_t* pixel_src;
	if (sprite->format == SPRITECTL_FORMAT_RGBA32) {
		pixel_src = (const uint32_t*)sprite->pixels;
	} else if (sprite->rgba_pixels) {
		pixel_src = sprite->rgba_pixels;
	} else {
		/* Fallback: fill with opaque white */
		static uint32_t* fallback_pixels = NULL;
		static int fallback_size = 0;
		if (!fallback_pixels || fallback_size < sprite->width * sprite->height) {
			free(fallback_pixels);
			fallback_pixels = (uint32_t*)malloc(sprite->width * sprite->height * sizeof(uint32_t));
			fallback_size = sprite->width * sprite->height;
			if (fallback_pixels) {
				for (int i = 0; i < sprite->width * sprite->height; i++) {
					fallback_pixels[i] = 0xFFFFFFFF;
				}
			}
		}
		pixel_src = fallback_pixels;
	}

	/* Copy to temporary surface */
	SDL_LockSurface(src_surface);
	uint32_t* src_pixels = (uint32_t*)src_surface->pixels;
	memcpy(src_pixels, pixel_src, sprite->width * sprite->height * sizeof(uint32_t));
	SDL_UnlockSurface(src_surface);

	/* Create scaled surface */
	scaled_surface = SDL_CreateRGBSurface(0, scaled_width, scaled_height, 32,
	                                    0xFF, 0xFF00, 0xFF0000, 0xFF000000);
	if (!scaled_surface) {
		SDL_FreeSurface(src_surface);
		return -1;
	}

	/* Scale the surface */
	SDL_SoftStretch(src_surface, NULL, scaled_surface, NULL);

	/* Blit scaled sprite to destination */
	SDL_Rect dest_rect;
	dest_rect.x = x;
	dest_rect.y = y;
	dest_rect.w = scaled_width;
	dest_rect.h = scaled_height;

	if (SDL_BlitSurface(scaled_surface, NULL, dest->surface, &dest_rect) != 0) {
		fprintf(stderr, "SpriteLib Backend: SDL_BlitSurface failed: %s\n", SDL_GetError());
		result = -1;
	} else {
		result = 0;
	}

	/* Cleanup */
	SDL_FreeSurface(scaled_surface);
	SDL_FreeSurface(src_surface);

	return result;
}

int spritectl_blt_surface(spritectl_surface_t dest,
                          const RECT* dest_rect,
                          spritectl_surface_t src,
                          const RECT* src_rect) {
	SDL_Rect sdl_dest_rect, sdl_src_rect;

	if (!dest || !src) {
		return -1;
	}

	/* Convert RECT to SDL_Rect */
	if (dest_rect) {
		sdl_dest_rect.x = dest_rect->left;
		sdl_dest_rect.y = dest_rect->top;
		sdl_dest_rect.w = dest_rect->right - dest_rect->left;
		sdl_dest_rect.h = dest_rect->bottom - dest_rect->top;
	} else {
		sdl_dest_rect.x = 0;
		sdl_dest_rect.y = 0;
		sdl_dest_rect.w = dest->width;
		sdl_dest_rect.h = dest->height;
	}

	if (src_rect) {
		sdl_src_rect.x = src_rect->left;
		sdl_src_rect.y = src_rect->top;
		sdl_src_rect.w = src_rect->right - src_rect->left;
		sdl_src_rect.h = src_rect->bottom - src_rect->top;
	} else {
		sdl_src_rect.x = 0;
		sdl_src_rect.y = 0;
		sdl_src_rect.w = src->width;
		sdl_src_rect.h = src->height;
	}

	/* Blit surface to surface */
	if (!SpriteGpu::CpuAccess(src, false) || !SpriteGpu::CpuAccess(dest, true)) return -1;
	if (SDL_BlitSurface(src->surface, &sdl_src_rect, dest->surface, &sdl_dest_rect) != 0) {
		fprintf(stderr, "SpriteLib Backend: SDL_BlitSurface failed: %s\n", SDL_GetError());
		return -1;
	}

	return 0;
}

/* ============================================================================
 * Sprite Pack Functions
 * ============================================================================ */

int spritectl_load_pack(const char* filename, spritectl_pack_t* pack) {
	FILE* file;
	uint16_t count;
	spritectl_pack_t pack_ptr = NULL;

	if (!filename || !pack) {
		return -1;
	}

	/* Open pack file: the game's spelling of the path, resolved to the
	   disk's (basic/DataPath.h) */
	file = Basic::OpenFile(Basic::NormalizeDataPath(filename).c_str(), "rb");
	if (!file) {
		fprintf(stderr, "SpriteLib Backend: Failed to open pack file: %s\n", filename);
		return -2;
	}

	/* Read sprite count (2 bytes, little-endian) */
	if (fread(&count, 2, 1, file) != 1) {
		fclose(file);
		return -4;
	}

	/* Allocate pack structure */
	pack_ptr = (spritectl_pack_t)malloc(sizeof(struct spritectl_pack_s));
	if (!pack_ptr) {
		fclose(file);
		return -3;
	}

	/* Initialize pack structure */
	pack_ptr->filename = Basic::DuplicateString(filename);
	pack_ptr->file = file;
	pack_ptr->sprite_count = count;
	pack_ptr->lazy_loading = 0;  /* Full load */
	pack_ptr->ref_count = 1;

	/* Allocate sprite array */
	if (count > 0) {
		pack_ptr->sprites = (spritectl_sprite_t*)calloc(count, sizeof(spritectl_sprite_t));
		if (!pack_ptr->sprites) {
			free(pack_ptr->filename);
			free(pack_ptr);
			fclose(file);
			return -3;
		}

		pack_ptr->loaded_flags = (int*)calloc(count, sizeof(int));
		if (!pack_ptr->loaded_flags) {
			free(pack_ptr->sprites);
			free(pack_ptr->filename);
			free(pack_ptr);
			fclose(file);
			return -3;
		}

		pack_ptr->file_offsets = NULL;  /* Not needed for full load */
	} else {
		pack_ptr->sprites = NULL;
		pack_ptr->loaded_flags = NULL;
		pack_ptr->file_offsets = NULL;
	}

	/* Load each sprite */
	uint16_t colorkey = 0xFFFF;  /* Default color key */
	for (uint16_t i = 0; i < count; i++) {
		int load_result = spritectl_load_sprite_from_file(file, &pack_ptr->sprites[i], colorkey);
		if (load_result != 0) {
			pack_ptr->sprites[i] = SPRITECTL_INVALID_SPRITE;
			pack_ptr->loaded_flags[i] = 0;
		} else {
			pack_ptr->loaded_flags[i] = 1;
		}
	}

	fclose(file);
	pack_ptr->file = NULL;

	*pack = pack_ptr;
	return 0;
}

void spritectl_free_pack(spritectl_pack_t pack) {
	if (!pack) {
		return;
	}

	/* Free all sprites */
	if (pack->sprites) {
		for (int i = 0; i < pack->sprite_count; i++) {
			if (pack->sprites[i] != SPRITECTL_INVALID_SPRITE) {
				spritectl_destroy_sprite(pack->sprites[i]);
			}
		}
		free(pack->sprites);
		pack->sprites = NULL;
	}

	/* Free other resources */
	if (pack->filename) {
		free(pack->filename);
		pack->filename = NULL;
	}
	if (pack->loaded_flags) {
		free(pack->loaded_flags);
		pack->loaded_flags = NULL;
	}
	if (pack->file_offsets) {
		free(pack->file_offsets);
		pack->file_offsets = NULL;
	}
	if (pack->file) {
		fclose(pack->file);
		pack->file = NULL;
	}

	free(pack);
}

int spritectl_get_sprite_from_pack(spritectl_pack_t pack, int index,
                                   spritectl_sprite_t* sprite) {
	if (!pack || !sprite) {
		return -1;
	}

	/* Check index bounds */
	if (index < 0 || index >= pack->sprite_count) {
		return -2;
	}

	/* Return sprite (or invalid handle if not loaded) */
	*sprite = pack->sprites[index];
	return 0;
}

int spritectl_get_pack_count(spritectl_pack_t pack) {
	if (!pack) {
		return 0;
	}
	return pack->sprite_count;
}

/* ============================================================================
 * Pixel Conversion Functions
 * ============================================================================ */

void spritectl_convert_565_to_555(const uint16_t* src, uint16_t* dest, int pixel_count) {
	int i;
	for (i = 0; i < pixel_count; i++) {
		uint16_t pixel = src[i];
		/* Convert 565 to 555: drop the low bit of green */
		dest[i] = ((pixel & 0xFFC0) >> 1) | (pixel & 0x001F);
	}
}

void spritectl_convert_555_to_565(const uint16_t* src, uint16_t* dest, int pixel_count) {
	int i;
	for (i = 0; i < pixel_count; i++) {
		uint16_t pixel = src[i];
		/* Convert 555 to 565: insert a zero bit for green's low bit */
		dest[i] = ((pixel & 0x7FE0) << 1) | (pixel & 0x001F);
	}
}

void spritectl_convert_565_to_rgba(const uint16_t* src, uint32_t* dest,
                                   int pixel_count, uint16_t colorkey) {
	int i;
	for (i = 0; i < pixel_count; i++) {
		uint16_t pixel = src[i];
		uint8_t r, g, b, a;

		if (pixel == colorkey) {
			/* Transparent pixel */
			a = 0;
		} else {
			a = 255;
		}

		spritectl_565_to_rgb(pixel, &r, &g, &b);
		dest[i] = spritectl_rgb_to_rgba(r, g, b, a);
	}
}

void spritectl_convert_555_to_rgba(const uint16_t* src, uint32_t* dest,
                                   int pixel_count, uint16_t colorkey) {
	int i;
	for (i = 0; i < pixel_count; i++) {
		uint16_t pixel = src[i];
		uint8_t r, g, b, a;

		if (pixel == colorkey) {
			/* Transparent pixel */
			a = 0;
		} else {
			a = 255;
		}

		spritectl_555_to_rgb(pixel, &r, &g, &b);
		dest[i] = spritectl_rgb_to_rgba(r, g, b, a);
	}
}

/* ============================================================================
 * RLE Sprite Decoding
 * ============================================================================ */

/**
 * Decode RLE-compressed sprite data to RGB565 pixels
 *
 * RLE Format (per scanline):
 *   - Length (2 bytes, uint16_t) - number of WORDs in RLE data
 *   - RLE Data (Length * 2 bytes):
 *     - Count (1 WORD) - number of segments
 *     - For each segment:
 *       - Trans Count (1 WORD) - transparent pixel count
 *       - Color Count (1 WORD) - color pixel count
 *       - Pixels (Color Count WORDs) - RGB565 pixel data
 */
int spritectl_decode_rle_data(const uint16_t* rle_data, int rle_len,
                               uint16_t* pixels_out, int width) {
	int rle_index = 0;
	int x = 0;

	/* Read segment count */
	if (rle_len == 0) {
		return 0;  /* Empty scanline */
	}

	int count = rle_data[rle_index++];
	rle_index = 1;

	/* Process each segment */
	for (int seg = 0; seg < count && x < width; seg++) {
		if (rle_index >= rle_len) break;

		/* Read transparent and color counts */
		int trans_count = rle_data[rle_index++];
		int color_count = rle_data[rle_index++];

		/* Skip transparent pixels (leave as 0) */
		x += trans_count;

		/* Copy color pixels */
		for (int c = 0; c < color_count && x < width; c++) {
			if (rle_index >= rle_len) break;
			pixels_out[x] = rle_data[rle_index++];
			x++;
		}
	}

	return 0;
}

int spritectl_decode_rle_sprite(const uint8_t* compressed, size_t compressed_size,
                                int width, int height,
                                uint16_t* pixels_out, size_t pixels_size) {
	(void)compressed;
	(void)compressed_size;
	(void)width;
	(void)height;
	(void)pixels_out;
	(void)pixels_size;

	/* For now, we'll load from FILE pointer instead of raw buffer */
	/* This is a simpler implementation that matches the original */
	fprintf(stderr, "SpriteLib Backend: Use spritectl_load_sprite_from_file instead\n");
	return -1;
}

/**
 * Load sprite from file (RLE encoded)
 * File format:
 *   - Width (2 bytes, uint16_t)
 *   - Height (2 bytes, uint16_t)
 *   - For each scanline:
 *     - Length (2 bytes, uint16_t)
 *     - RLE data (Length * 2 bytes)
 */
int spritectl_load_sprite_from_file(FILE* file, spritectl_sprite_t* sprite_out,
                                    uint16_t colorkey) {
	(void)colorkey;
	uint16_t width = 0, height = 0;
	uint16_t* scanline_lengths = NULL;
	uint16_t** scanline_rle = NULL;
	spritectl_sprite_t sprite = NULL;
	size_t pixel_count = 0;
	uint16_t* pixels = NULL;
	int result = -1;

	if (!file || !sprite_out) {
		return -1;
	}

	/* Read width and height */
	if (fread(&width, 2, 1, file) != 1) goto cleanup;
	if (fread(&height, 2, 1, file) != 1) goto cleanup;

	/* Handle empty sprite */
	if (width == 0 || height == 0) {
		*sprite_out = SPRITECTL_INVALID_SPRITE;
		return 0;
	}

	if (int(width) > INT_MAX / int(height)) goto cleanup;
	pixel_count = size_t(width) * size_t(height);

	/* Allocate scanline length array */
	scanline_lengths = (uint16_t*)calloc(height, sizeof(uint16_t));
	if (!scanline_lengths) goto cleanup;

	/* Allocate RLE data array */
	scanline_rle = (uint16_t**)calloc(height, sizeof(uint16_t*));
	if (!scanline_rle) goto cleanup;

	/* Read each scanline */
	for (int y = 0; y < height; y++) {
		/* Read scanline data length */
		if (fread(&scanline_lengths[y], 2, 1, file) != 1) goto cleanup;

		if (scanline_lengths[y] > 0) {
			/* Allocate and read RLE data */
			scanline_rle[y] = (uint16_t*)malloc(scanline_lengths[y] * sizeof(uint16_t));
			if (!scanline_rle[y]) goto cleanup;

			if (fread(scanline_rle[y], 2, scanline_lengths[y], file) != scanline_lengths[y]) {
				free(scanline_rle[y]);
				scanline_rle[y] = NULL;
				goto cleanup;
			}
		}
	}

	// Validate the complete file before decoding any scanline.
	for (int y = 0; y < height; ++y) {
		if (scanline_lengths[y] && !ValidateSpriteScanline(
			{scanline_rle[y], scanline_lengths[y]}, width))
			goto cleanup;
	}

	/* Decode RLE to raw pixels */
	pixels = (uint16_t*)calloc(pixel_count, sizeof(uint16_t));
	if (!pixels) goto cleanup;

	/* Decode each scanline */
	for (int y = 0; y < height; y++) {
		uint16_t* row = pixels + (y * width);

		if (scanline_rle[y] && scanline_lengths[y] > 0) {
			/* Decode RLE data */
			int rle_index = 0;
			int x = 0;

			/* Read segment count */
			int count = scanline_rle[y][rle_index++];

			/* Process each segment */
			for (int seg = 0; seg < count && x < width; seg++) {
				int trans_count = scanline_rle[y][rle_index++];
				int color_count = scanline_rle[y][rle_index++];

				/* Skip transparent pixels (keep as 0) */
				x += trans_count;

				/* Copy color pixels */
				for (int c = 0; c < color_count && x < width; c++) {
					row[x] = scanline_rle[y][rle_index++];
					x++;
				}
			}
		}
	}

	/* Create sprite structure */
	sprite = spritectl_create_sprite(width, height, SPRITECTL_FORMAT_RGB565,
	                                  pixels, pixel_count * sizeof(uint16_t));
	if (!sprite) goto cleanup;

	/* Preserve RLE data for correct transparency rendering */
	sprite->scanline_rle = scanline_rle;
	sprite->scanline_lens = scanline_lengths;
	sprite->has_rle = 1;
	scanline_rle = NULL;  /* Don't free in cleanup */
	scanline_lengths = NULL;

	*sprite_out = sprite;
	result = 0;

cleanup:
	free(pixels);
	if (scanline_rle) {
		for (int y = 0; y < height; y++) {
			if (scanline_rle[y]) {
				free(scanline_rle[y]);
			}
		}
		free(scanline_rle);
	}
	if (scanline_lengths) {
		free(scanline_lengths);
	}

	return result;
}

/* ============================================================================
 * Present Surface to Renderer
 * ============================================================================ */

/* Where the last present placed the game frame inside the window, for the
 * window-to-game mouse mapping. Identity (empty) until the first present.
 * g_present_dest is in renderer output pixels; the window's size in points
 * is kept beside it because the two differ on a high-DPI display when the
 * window was created with SDL_WINDOW_ALLOW_HIGHDPI (a Retina Mac: 2x), and
 * mouse events arrive in points. The window is whichever one
 * spritectl_set_present_window was given (CSDLGraphics::Init, which
 * creates it); a present with none - the tests' software renderer over a
 * surface - records no window size and the mapping is the identity. It
 * used to be asked of the renderer through SDL_RenderGetWindow, which is
 * SDL 2.0.22: on an older SDL the ratio silently vanished while the
 * ALLOW_HIGHDPI flag that needs it stayed. */
static SDL_Rect g_present_dest = { 0, 0, 0, 0 };
static int g_present_game_w = 0;
static int g_present_game_h = 0;
static int g_present_output_w = 0;
static int g_present_output_h = 0;
static int g_present_window_w = 0;
static int g_present_window_h = 0;
static SDL_Window* g_present_window = NULL;

/* Intermediate render target for sharp-bilinear upscaling: the game frame is
 * first enlarged by an integer factor with nearest (crisp pixels), then that
 * is scaled to the final size with linear (no shimmer at fractional scales).
 * Direct nearest is used when the scale is an exact integer, direct linear
 * when shrinking. */
static SDL_Texture* g_scale_tex = NULL;
static SDL_Renderer* g_scale_tex_renderer = NULL;
static int g_scale_tex_w = 0;
static int g_scale_tex_h = 0;

void spritectl_set_xbrz_enabled(int enabled) {
	g_xbrz_enabled = enabled != 0;
	if (!g_xbrz_enabled) {
		g_frame_upscaler.Release();
		SpriteGpu::ReleaseUpscaler();
	}
}

int spritectl_get_xbrz_enabled(void) { return g_xbrz_enabled ? 1 : 0; }

void spritectl_release_present_resources(void* renderer_ptr) {
	SpriteGpu::Detach(static_cast<SDL_Renderer*>(renderer_ptr));
	SDL_Renderer* renderer = static_cast<SDL_Renderer*>(renderer_ptr);
	g_frame_upscaler.Release(renderer);
	if (!renderer || renderer == g_scale_tex_renderer) {
		if (g_scale_tex) SDL_DestroyTexture(g_scale_tex);
		g_scale_tex = NULL;
		g_scale_tex_renderer = NULL;
		g_scale_tex_w = g_scale_tex_h = 0;
	}
}

/* The two-pass sharp-bilinear is only worth it on a GPU. A software renderer
 * (SDL_RENDER_DRIVER=software) would run both scaling passes on the CPU at
 * output resolution every frame - there, a single linear pass is the right
 * trade. Cached per renderer. */
static int present_renderer_accelerated(SDL_Renderer* renderer) {
	static SDL_Renderer* cached_renderer = NULL;
	static int cached_accelerated = 0;

	if (renderer != cached_renderer) {
		SDL_RendererInfo info;
		cached_accelerated = (SDL_GetRendererInfo(renderer, &info) == 0) &&
			(info.flags & SDL_RENDERER_ACCELERATED) != 0;
		cached_renderer = renderer;
	}
	return cached_accelerated;
}

void spritectl_set_present_window(void* window) {
	g_present_window = static_cast<SDL_Window*>(window);
}

void spritectl_set_render_device(void* renderer) {
	const char* choice = SDL_getenv("DARKEDEN_SPRITE_RENDERER");
	if (!renderer || (choice && SDL_strcasecmp(choice, "software") == 0)) {
		SpriteGpu::Detach();
		return;
	}
	if (!SpriteGpu::Attach(static_cast<SDL_Renderer*>(renderer)))
		SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "GPU sprite composition unavailable; using software: %s", SDL_GetError());
}

void spritectl_finish_rendering(void) { SpriteGpu::Finish(); }

void spritectl_render_device_reset(void) {
	SpriteGpu::Reset();
	g_frame_upscaler.Release();
	if (g_scale_tex) SDL_DestroyTexture(g_scale_tex);
	g_scale_tex = nullptr;
	g_scale_tex_renderer = nullptr;
	g_scale_tex_w = g_scale_tex_h = 0;
}

void spritectl_set_present_geometry(int window_w, int window_h,
	int output_w, int output_h,
	int dest_x, int dest_y, int dest_w, int dest_h,
	int game_w, int game_h) {
	g_present_window_w = window_w;
	g_present_window_h = window_h;
	g_present_output_w = output_w;
	g_present_output_h = output_h;
	g_present_dest.x = dest_x;
	g_present_dest.y = dest_y;
	g_present_dest.w = dest_w;
	g_present_dest.h = dest_h;
	g_present_game_w = game_w;
	g_present_game_h = game_h;
}

void spritectl_window_to_game_coords(int* x, int* y) {
	if (!x || !y) {
		return;
	}
	if (g_present_dest.w <= 0 || g_present_dest.h <= 0 ||
		g_present_game_w <= 0 || g_present_game_h <= 0) {
		return;	/* nothing presented yet - coordinates are already game pixels */
	}

	/* Points to output pixels first, where the window is high-DPI. A
	 * window the same size as the output (every non-Retina display, and
	 * a renderer with no window) is the identity here. */
	long long px = *x;
	long long py = *y;
	if (g_present_window_w > 0 && g_present_window_h > 0 &&
		g_present_output_w > 0 && g_present_output_h > 0) {
		px = (px * g_present_output_w) / g_present_window_w;
		py = (py * g_present_output_h) / g_present_window_h;
	}

	int gx = (int)(((px - g_present_dest.x) * g_present_game_w) / g_present_dest.w);
	int gy = (int)(((py - g_present_dest.y) * g_present_game_h) / g_present_dest.h);

	if (gx < 0) gx = 0;
	if (gx > g_present_game_w - 1) gx = g_present_game_w - 1;
	if (gy < 0) gy = 0;
	if (gy > g_present_game_h - 1) gy = g_present_game_h - 1;

	*x = gx;
	*y = gy;
}

int spritectl_present_surface(spritectl_surface_t surface, void* renderer_ptr) {
	SpriteGpu::Finish();
	if (!SpriteGpu::CheckpointOffscreen(surface)) return -1;
	if (!surface || !renderer_ptr) {
		return -1;
	}

	SDL_Renderer* renderer = (SDL_Renderer*)renderer_ptr;

	/* Get SDL surface from backend surface */
	SDL_Surface* sdl_surface = surface->surface;
	if (!sdl_surface) {
		fprintf(stderr, "SpriteLib Backend: Surface has no SDL surface\n");
		return -1;
	}

	/* Aspect-preserving letterbox: scale the game frame to the largest size
	 * that fits the window and center it. The bars are whatever Flip cleared
	 * the renderer to (black). Falls back to a 1:1 top-left blit if the
	 * output size cannot be queried. */
	SDL_Rect dest_rect;
	dest_rect.x = 0;
	dest_rect.y = 0;
	dest_rect.w = sdl_surface->w;
	dest_rect.h = sdl_surface->h;

	int out_w = 0;
	int out_h = 0;
	if (SDL_GetRendererOutputSize(renderer, &out_w, &out_h) == 0 &&
		out_w > 0 && out_h > 0) {
		/* largest scale preserving aspect, in 16.16 fixed point */
		long long scale_x = ((long long)out_w << 16) / sdl_surface->w;
		long long scale_y = ((long long)out_h << 16) / sdl_surface->h;
		long long scale = (scale_x < scale_y) ? scale_x : scale_y;
		if (scale < 1) scale = 1;

		dest_rect.w = (int)(((long long)sdl_surface->w * scale) >> 16);
		dest_rect.h = (int)(((long long)sdl_surface->h * scale) >> 16);
		if (dest_rect.w < 1) dest_rect.w = 1;
		if (dest_rect.h < 1) dest_rect.h = 1;
		dest_rect.x = (out_w - dest_rect.w) / 2;
		dest_rect.y = (out_h - dest_rect.h) / 2;
	}

	g_present_dest = dest_rect;
	g_present_game_w = sdl_surface->w;
	g_present_game_h = sdl_surface->h;

	/* The window's size in points, for the mouse mapping; none without a
	 * window (the tests' software renderer over a surface), and then the
	 * mapping is the identity. */
	g_present_output_w = out_w;
	g_present_output_h = out_h;
	g_present_window_w = 0;
	g_present_window_h = 0;
	if (g_present_window != NULL && out_w > 0 && out_h > 0) {
		SDL_GetWindowSize(g_present_window, &g_present_window_w, &g_present_window_h);
	}

	if (g_xbrz_enabled) {
		const int factor = FrameUpscaler::ScaleFactor(surface->width, surface->height, dest_rect);
		if (SDL_Texture* texture = SpriteGpu::UpscaledTexture(surface, renderer, factor)) {
			SDL_SetTextureScaleMode(texture, dest_rect.w == surface->width * factor && dest_rect.h == surface->height * factor
				? SDL_ScaleModeNearest : SDL_ScaleModeLinear);
			if (SDL_RenderCopy(renderer, texture, nullptr, &dest_rect) == 0) return 0;
		}
		if (!SpriteGpu::CpuAccess(surface, false)) return -1;
		if (g_frame_upscaler.Draw(sdl_surface, renderer, dest_rect)) return 0;
		// Restore the original path if filtering cannot allocate/convert/draw.
		// Disable it until explicitly toggled again instead of retrying every frame.
		spritectl_set_xbrz_enabled(0);
		SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "xBRZ unavailable; restored original presentation: %s", SDL_GetError());
	}

	/* This is called every frame (e.g. to present the fixed-size g_pBack
	 * backbuffer). Recreating and destroying a full-screen SDL_Texture on
	 * every call caused continuous GPU/driver-side memory churn even while
	 * completely idle. Reuse a persistent streaming texture instead, only
	 * (re)creating it if the renderer changes or it doesn't exist yet. */
	SDL_Texture* frameTexture = SpriteGpu::PresentationTexture(surface, renderer);
	if (!frameTexture) {
		if (!SpriteGpu::CpuAccess(surface, false)) return -1;
		if (surface->texture && surface->renderer != renderer) {
			SDL_DestroyTexture(surface->texture);
			surface->texture = nullptr;
		}
		if (!surface->texture) {
			surface->texture = SDL_CreateTexture(renderer, sdl_surface->format->format,
				SDL_TEXTUREACCESS_STREAMING, sdl_surface->w, sdl_surface->h);
			if (surface->texture) surface->renderer = renderer;
		}
		if (surface->texture && SDL_UpdateTexture(surface->texture, nullptr,
			sdl_surface->pixels, sdl_surface->pitch) == 0) frameTexture = surface->texture;
	}
	if (frameTexture) {

		int drawn = 0;

		if (dest_rect.w == sdl_surface->w || dest_rect.w % sdl_surface->w == 0) {
			/* 1:1 or exact integer upscale - nearest is pixel-perfect */
			SDL_SetTextureScaleMode(frameTexture, SDL_ScaleModeNearest);
			drawn = (SDL_RenderCopy(renderer, frameTexture, NULL, &dest_rect) == 0);
		} else if (dest_rect.w < sdl_surface->w) {
			/* shrinking - plain linear */
			SDL_SetTextureScaleMode(frameTexture, SDL_ScaleModeLinear);
			drawn = (SDL_RenderCopy(renderer, frameTexture, NULL, &dest_rect) == 0);
		} else {
			/* Fractional upscale - sharp bilinear: nearest to the next integer
			 * multiple on a render target, then linear down to the final size.
			 * Crisp pixels without the shimmer plain nearest gives at
			 * fractional scales, without the blur of plain linear. */
			int k = (dest_rect.w + sdl_surface->w - 1) / sdl_surface->w;
			if (k > 4) k = 4;

			int tex_w = sdl_surface->w * k;
			int tex_h = sdl_surface->h * k;

			if (present_renderer_accelerated(renderer) && SDL_RenderTargetSupported(renderer)) {
				if (g_scale_tex != NULL &&
					(g_scale_tex_renderer != renderer ||
					 g_scale_tex_w != tex_w || g_scale_tex_h != tex_h)) {
					SDL_DestroyTexture(g_scale_tex);
					g_scale_tex = NULL;
				}

				if (g_scale_tex == NULL) {
					g_scale_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
						SDL_TEXTUREACCESS_TARGET, tex_w, tex_h);
					if (g_scale_tex != NULL) {
						g_scale_tex_renderer = renderer;
						g_scale_tex_w = tex_w;
						g_scale_tex_h = tex_h;
						SDL_SetTextureScaleMode(g_scale_tex, SDL_ScaleModeLinear);
					}
				}

				if (g_scale_tex != NULL) {
					SDL_SetTextureScaleMode(frameTexture, SDL_ScaleModeNearest);
					if (SDL_SetRenderTarget(renderer, g_scale_tex) == 0) {
						SDL_RenderCopy(renderer, frameTexture, NULL, NULL);
						SDL_SetRenderTarget(renderer, NULL);
						drawn = (SDL_RenderCopy(renderer, g_scale_tex, NULL, &dest_rect) == 0);
					} else {
						SDL_SetRenderTarget(renderer, NULL);
					}
				}
			}

			if (!drawn) {
				/* no render-target support (or it failed) - plain linear */
				SDL_SetTextureScaleMode(frameTexture, SDL_ScaleModeLinear);
				drawn = (SDL_RenderCopy(renderer, frameTexture, NULL, &dest_rect) == 0);
			}
		}

		if (drawn) {
			return 0;
		}
	}

	/* Fallback: the renderer may not support this pixel format as a native
	 * texture format (or the fast path above failed for some other reason).
	 * SDL_CreateTextureFromSurface performs any necessary conversion, at the
	 * cost of allocating/freeing a texture every call - same as the old
	 * behavior, kept only as a safety net. */
	if (!SpriteGpu::CpuAccess(surface, false)) return -1;
	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, sdl_surface);
	if (!texture) {
		fprintf(stderr, "SpriteLib Backend: Failed to create texture: %s\n", SDL_GetError());
		return -1;
	}

	int ok = (SDL_RenderCopy(renderer, texture, NULL, &dest_rect) == 0);
	if (!ok) {
		fprintf(stderr, "SpriteLib Backend: Failed to render texture: %s\n", SDL_GetError());
	}
	SDL_DestroyTexture(texture);

	return ok ? 0 : -1;
}
