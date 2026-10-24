#pragma once

#include "SpriteLibBackendSDL.h"
#include "SpriteGpuEffects.h"
#include <cstdint>

class CSpritePalBase;
class MPalette;

// All calls belong to the SDL render thread. CPU pixel access is explicit:
// scoped backend locks synchronize once; borrowed pointers pin the surface
// to software until the owner calls EndBorrow (CSpriteSurface::Unlock).
namespace SpriteGpu {
struct Counters {
	uint64_t draws = 0;
	uint64_t spriteUploads = 0;
	uint64_t surfaceUploads = 0;
	uint64_t readbacks = 0;
	uint64_t textureBytes = 0;
	uint64_t surfaceBytes = 0;
	uint64_t shaderDraws = 0;
	uint64_t paletteUploads = 0;
	uint64_t upscaledFrames = 0;
	uint64_t upscaleBytes = 0;
};
bool Attach(SDL_Renderer* renderer, bool allowSoftwareForTests = false);
void Detach(SDL_Renderer* renderer = nullptr);
bool Active();
void Finish();
bool CheckpointOffscreen(spritectl_surface_t presented);
void Reset();
Counters GetCounters();
bool CpuAccess(spritectl_surface_t surface, bool write, bool borrow = false);
void EndBorrow(spritectl_surface_t surface);
void ForgetSurface(spritectl_surface_t surface);
void ForgetSprite(spritectl_sprite_t sprite);
void ForgetPaletteSprite(const CSpritePalBase* sprite);
// False means the caller must execute the existing software path.
bool Draw(spritectl_surface_t dest, int x, int y, spritectl_sprite_t sprite,
	int flags, int alpha, int scale = 256);
bool Fill(spritectl_surface_t dest, const SDL_Rect* rect, uint32_t color, bool ignoreClip = false);
bool DrawEffect(spritectl_surface_t dest, int x, int y, spritectl_sprite_t sprite,
	SpriteGpuEffects::Effect effect, int value = 0, const Uint16* gradation = nullptr);
bool Gamma(spritectl_surface_t dest, const SDL_Rect& rect, int value);
bool Tint(spritectl_surface_t dest, const SDL_Rect& rect, int channel);
// alpha2 (0..31) is the share of what lies under that is kept, out of 32.
bool ColorAlpha(spritectl_surface_t dest, const SDL_Rect& rect, uint16_t color, int alpha2);
bool LightGrid(spritectl_surface_t dest, std::span<const SpriteGpuEffects::LightCell> cells);
bool DrawPalette(spritectl_surface_t dest, int x, int y, const CSpritePalBase* sprite,
	const MPalette& palette, bool alpha, bool screen);
bool Copy(spritectl_surface_t dest, const SDL_Rect& to, spritectl_surface_t src, const SDL_Rect& from);
SDL_Texture* PresentationTexture(spritectl_surface_t surface, SDL_Renderer* renderer);
SDL_Texture* UpscaledTexture(spritectl_surface_t surface, SDL_Renderer* renderer, int factor);
void ReleaseUpscaler();
}
