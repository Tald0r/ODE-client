#pragma once

#include <SDL.h>
#include <span>

// Shader operations use integer RGB565 channel arithmetic. SDL continues to
// own the context, render targets and texture lifetime.
namespace SpriteGpuEffects {
enum class Effect { Copy, Gamma, Color, Darkness, GrayScale, Gradation, PaletteCopy, PaletteScreen, PaletteAlpha, LightGrid, ColorAlpha };
struct LightCell { SDL_Rect rect; Uint8 light; };
bool Attach(SDL_Renderer* renderer);
void Detach();
bool Active();
bool XbrzAvailable();
// A null corners texture classifies source pixels; otherwise scales by 2..4.
// The caller binds the appropriately sized SDL render target first.
bool Xbrz(SDL_Texture* source, SDL_Texture* corners, int width, int height, int factor);
bool LightGrid(SDL_Texture* source, int width, int height, std::span<const LightCell> cells);
bool Draw(SDL_Texture* source, int width, int height, const SDL_Rect& placement,
	const SDL_Rect& clip, Effect effect, int value, const Uint16* gradation = nullptr,
	SDL_Texture* background = nullptr, SDL_Texture* palette = nullptr);
}
