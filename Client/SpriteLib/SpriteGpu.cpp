#include "SpriteGpu.h"
#include "SpriteScanline.h"
#include "CSpritePalBase.h"
#include <algorithm>
#include <array>
#include <climits>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

namespace SpriteGpu {
namespace {
constexpr uint64_t CacheLimit = 128ull * 1024 * 1024;
enum class Owner { Cpu, Both, Gpu };
struct Surface {
	SDL_Texture* target = nullptr;
	SDL_Texture* upload = nullptr;
	SDL_Texture* snapshot = nullptr;
	Owner owner = Owner::Cpu;
	uint64_t bytes = 0;
	uint64_t revision = 0;
};
struct Sprite {
	SDL_Texture* texture = nullptr;
	uint64_t bytes = 0;
	uint64_t used = 0;
};
SDL_Renderer* device = nullptr;
SDL_RendererInfo deviceInfo{};
std::unordered_map<spritectl_surface_t, Surface> surfaces;
std::unordered_map<spritectl_sprite_t, Sprite> sprites;
std::unordered_map<const CSpritePalBase*, Sprite> paletteSprites;
SDL_Texture* paletteTexture = nullptr;
std::array<uint32_t, 256> paletteColors{};
Counters counters;
uint64_t sequence = 0;
struct Upscaler {
	SDL_Texture* corners = nullptr;
	SDL_Texture* output = nullptr;
	spritectl_surface_t source = nullptr;
	int width = 0, height = 0, factor = 0;
	uint64_t revision = 0;
} upscaler;

void MarkGpu(Surface& surface)
{
	surface.owner = Owner::Gpu;
	++surface.revision;
}

// Restoring target, clip and color is also necessary for the existing SDL
// presentation and xBRZ paths. Never leave a sprite surface bound at Flip().
class RenderState {
public:
	RenderState() : target(SDL_GetRenderTarget(device))
	{
		SDL_RenderGetViewport(device, &viewport);
		SDL_RenderGetClipRect(device, &clip);
		clipped = SDL_RenderIsClipEnabled(device);
		SDL_RenderGetScale(device, &scaleX, &scaleY);
		SDL_GetRenderDrawColor(device, &r, &g, &b, &a);
		SDL_GetRenderDrawBlendMode(device, &blend);
	}
	~RenderState()
	{
		SDL_SetRenderTarget(device, target);
		SDL_RenderSetScale(device, scaleX, scaleY);
		SDL_RenderSetViewport(device, &viewport);
		SDL_RenderSetClipRect(device, clipped ? &clip : nullptr);
		SDL_SetRenderDrawColor(device, r, g, b, a);
		SDL_SetRenderDrawBlendMode(device, blend);
	}
	bool Bind(SDL_Texture* texture, const SDL_Rect* rect = nullptr)
	{
		return SDL_SetRenderTarget(device, texture) == 0
			&& SDL_RenderSetScale(device, 1, 1) == 0
			&& SDL_RenderSetViewport(device, nullptr) == 0
			&& SDL_RenderSetClipRect(device, rect) == 0;
	}
private:
	SDL_Texture* target;
	SDL_Rect viewport{}, clip{};
	SDL_bool clipped;
	float scaleX = 1, scaleY = 1;
	Uint8 r = 0, g = 0, b = 0, a = 255;
	SDL_BlendMode blend = SDL_BLENDMODE_NONE;
};

std::unique_ptr<RenderState> batch;
SDL_Texture* batchTarget = nullptr;
SDL_Rect batchClip{};
bool batchClipped = false;
bool BindForDraw(SDL_Texture* target, const SDL_Rect* clip)
{
	const bool sameClip = bool(clip) == batchClipped && (!clip
		|| (clip->x == batchClip.x && clip->y == batchClip.y
			&& clip->w == batchClip.w && clip->h == batchClip.h));
	// SDL queues viewport/clip commands even when their values have not
	// changed. Avoid that work for every sprite and every letter of text.
	if (batch && target == batchTarget && sameClip) return true;
	if (!batch) batch = std::make_unique<RenderState>();
	if (!batch->Bind(target, clip)) { batch.reset(); return false; }
	batchTarget = target;
	batchClipped = clip != nullptr;
	if (clip) batchClip = *clip;
	return true;
}

bool Fits(int width, int height)
{
	return width > 0 && height > 0 && width <= 16384 && height <= 16384
		&& (!deviceInfo.max_texture_width || width <= deviceInfo.max_texture_width)
		&& (!deviceInfo.max_texture_height || height <= deviceInfo.max_texture_height)
		&& uint64_t(width) * height * 4 <= CacheLimit;
}

SDL_Texture* Texture(int width, int height, int access)
{
	SDL_Texture* result = SDL_CreateTexture(device, SDL_PIXELFORMAT_ARGB8888, access, width, height);
	if (result) {
		SDL_SetTextureScaleMode(result, SDL_ScaleModeNearest);
		SDL_SetTextureBlendMode(result, SDL_BLENDMODE_NONE);
	}
	return result;
}

bool Upload(spritectl_surface_t surface, Surface& state)
{
	if (state.owner != Owner::Cpu) return true;
	if (!state.upload) state.upload = Texture(surface->width, surface->height, SDL_TEXTUREACCESS_STREAMING);
	if (!state.upload) return false;
	void* pixels = nullptr;
	int pitch = 0;
	if (SDL_LockTexture(state.upload, nullptr, &pixels, &pitch) != 0) return false;
	const int converted = SDL_ConvertPixels(surface->width, surface->height,
		surface->surface->format->format, surface->surface->pixels, surface->surface->pitch,
		SDL_PIXELFORMAT_ARGB8888, pixels, pitch);
	SDL_UnlockTexture(state.upload);
	if (converted != 0) return false;
	RenderState restore;
	if (!restore.Bind(state.target) || SDL_RenderCopy(device, state.upload, nullptr, nullptr) != 0) return false;
	state.owner = surface->cpu_borrowed ? Owner::Cpu : Owner::Both;
	++state.revision;
	++counters.surfaceUploads;
	return true;
}

Surface* Target(spritectl_surface_t surface, bool writing)
{
	if (!device || !surface || !surface->surface || !Fits(surface->width, surface->height)
		|| surface->locked || SDL_MUSTLOCK(surface->surface) || surface->format != SPRITECTL_FORMAT_RGB565) return nullptr;
	auto& state = surfaces[surface];
	if (writing && surface->cpu_borrowed) return nullptr;
	if (!state.target) {
		const uint64_t bytes = uint64_t(surface->width) * surface->height * 8;
		if (bytes > CacheLimit || counters.surfaceBytes > CacheLimit - bytes) return nullptr;
		state.target = Texture(surface->width, surface->height, SDL_TEXTUREACCESS_TARGET);
		if (state.target) { state.bytes = bytes; counters.surfaceBytes += bytes; }
	}
	if (!state.target || !Upload(surface, state)) return nullptr;
	return &state;
}

bool Snapshot(spritectl_surface_t surface, Surface& state, const SDL_Rect* region = nullptr)
{
	if (!state.snapshot) {
		const uint64_t bytes = uint64_t(surface->width) * surface->height * 4;
		if (counters.surfaceBytes > CacheLimit - bytes) return false;
		state.snapshot = Texture(surface->width, surface->height, SDL_TEXTUREACCESS_TARGET);
		if (!state.snapshot) return false;
		state.bytes += bytes;
		counters.surfaceBytes += bytes;
	}
	RenderState restore;
	return restore.Bind(state.snapshot) && SDL_RenderCopy(device, state.target, region, region) == 0;
}

uint32_t Color(uint16_t pixel, int format)
{
	uint8_t r, g, b;
	if (format == SPRITECTL_FORMAT_RGB555) spritectl_555_to_rgb(pixel, &r, &g, &b);
	else spritectl_565_to_rgb(pixel, &r, &g, &b);
	return 0xff000000u | (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
}

// A full cache frees down to 7/8, least recently used first, in one sorted
// pass. Scanning every cached texture once per evicted one stalled frames
// whose new sprites each pushed out many small ones.
void Evict(uint64_t bytes)
{
	if (counters.textureBytes + bytes <= CacheLimit) return;
	struct Entry { uint64_t used; spritectl_sprite_t sprite; const CSpritePalBase* palette; };
	std::vector<Entry> order;
	order.reserve(sprites.size() + paletteSprites.size());
	for (const auto& entry : sprites) order.push_back({entry.second.used, entry.first, nullptr});
	for (const auto& entry : paletteSprites) order.push_back({entry.second.used, nullptr, entry.first});
	std::sort(order.begin(), order.end(), [](const Entry& a, const Entry& b) { return a.used < b.used; });
	auto release = [](auto& map, auto key) {
		auto found = map.find(key);
		SDL_DestroyTexture(found->second.texture);
		counters.textureBytes -= found->second.bytes;
		map.erase(found);
	};
	for (const Entry& entry : order) {
		if (counters.textureBytes + bytes <= CacheLimit - CacheLimit / 8) break;
		if (entry.sprite) release(sprites, entry.sprite);
		else release(paletteSprites, entry.palette);
	}
}

SDL_Texture* PaletteSpriteTexture(const CSpritePalBase* sprite, bool alpha)
{
	if (!sprite || !Fits(sprite->GetWidth(), sprite->GetHeight())) return nullptr;
	if (auto found = paletteSprites.find(sprite); found != paletteSprites.end()) {
		found->second.used = ++sequence;
		return found->second.texture;
	}
	const uint64_t bytes = uint64_t(sprite->GetWidth()) * sprite->GetHeight() * 4;
	std::vector<uint32_t> pixels(size_t(bytes / 4));
	if (!sprite->DecodeGpuPixels(pixels, alpha)) return nullptr;
	Evict(bytes);
	if (counters.textureBytes + bytes > CacheLimit) return nullptr;
	SDL_Texture* texture = Texture(sprite->GetWidth(), sprite->GetHeight(), SDL_TEXTUREACCESS_STATIC);
	if (!texture) return nullptr;
	if (SDL_UpdateTexture(texture, nullptr, pixels.data(), sprite->GetWidth() * 4) != 0) {
		SDL_DestroyTexture(texture);
		return nullptr;
	}
	try { paletteSprites.emplace(sprite, Sprite{texture, bytes, ++sequence}); }
	catch (...) { SDL_DestroyTexture(texture); throw; }
	counters.textureBytes += bytes;
	++counters.spriteUploads;
	return texture;
}

bool UpdatePalette(const MPalette& palette)
{
	std::array<uint32_t, 256> colors;
	for (unsigned i = 0; i < colors.size(); ++i) colors[i] = Color(palette[Uint8(i)], SPRITECTL_FORMAT_RGB565);
	if (paletteTexture && colors == paletteColors) return true;
	if (!paletteTexture) {
		Evict(sizeof(colors));
		paletteTexture = Texture(256, 1, SDL_TEXTUREACCESS_STATIC);
		if (!paletteTexture) return false;
		counters.textureBytes += sizeof(colors);
	}
	if (SDL_UpdateTexture(paletteTexture, nullptr, colors.data(), sizeof(colors)) != 0) return false;
	paletteColors = colors;
	++counters.paletteUploads;
	return true;
}

SDL_Texture* SpriteTexture(spritectl_sprite_t sprite)
{
	if (!Fits(sprite->width, sprite->height)) return nullptr;
	if (auto found = sprites.find(sprite); found != sprites.end()) {
		found->second.used = ++sequence;
		return found->second.texture;
	}
	const uint64_t bytes = uint64_t(sprite->width) * sprite->height * 4;
	std::vector<uint32_t> pixels(size_t(bytes / 4), 0);
	if (sprite->has_rle) {
		if (!sprite->scanline_rle || !sprite->scanline_lens) return nullptr;
		for (int y = 0; y < sprite->height; ++y) {
			const auto length = sprite->scanline_lens[y];
			if (!length) continue;
			const uint16_t* line = sprite->scanline_rle[y];
			if (!line || !ValidateSpriteScanline({line, length}, sprite->width)) return nullptr;
			const int runs = *line++;
			int x = 0;
			for (int run = 0; run < runs; ++run) {
				x += *line++;
				const int count = *line++;
				for (int i = 0; i < count; ++i)
					pixels[size_t(y) * sprite->width + x++] = Color(*line++, SPRITECTL_FORMAT_RGB565);
			}
		}
	} else {
		if (!sprite->pixels) return nullptr;
		if (sprite->format == SPRITECTL_FORMAT_RGBA32) {
			if (SDL_ConvertPixels(sprite->width, sprite->height, SDL_PIXELFORMAT_ABGR8888,
				sprite->pixels, sprite->width * 4, SDL_PIXELFORMAT_ARGB8888,
				pixels.data(), sprite->width * 4) != 0) return nullptr;
		} else {
			for (size_t i = 0; i < pixels.size(); ++i)
				if (sprite->pixels[i]) pixels[i] = Color(sprite->pixels[i], sprite->format);
		}
	}
	Evict(bytes);
	if (counters.textureBytes + bytes > CacheLimit) return nullptr;
	SDL_Texture* texture = Texture(sprite->width, sprite->height, SDL_TEXTUREACCESS_STATIC);
	if (!texture) return nullptr;
	if (SDL_UpdateTexture(texture, nullptr, pixels.data(), sprite->width * 4) != 0
		|| SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) != 0) {
		SDL_DestroyTexture(texture);
		return nullptr;
	}
	try { sprites.emplace(sprite, Sprite{texture, bytes, ++sequence}); }
	catch (...) { SDL_DestroyTexture(texture); throw; }
	counters.textureBytes += bytes;
	++counters.spriteUploads;
	return texture;
}
}

bool Attach(SDL_Renderer* renderer, bool allowSoftwareForTests)
{
	if (device == renderer) return device != nullptr;
	Detach();
	SDL_RendererInfo info{};
	if (!renderer || SDL_GetRendererInfo(renderer, &info) != 0
		|| !(info.flags & SDL_RENDERER_TARGETTEXTURE)
		|| (!allowSoftwareForTests && !(info.flags & SDL_RENDERER_ACCELERATED))) return false;
	device = renderer;
	deviceInfo = info;
	counters = {};
	SpriteGpuEffects::Attach(renderer);
	SDL_Log("Sprite composition: texture renderer (%s)", info.name);
	return true;
}

bool Active() { return device != nullptr; }
void Finish() { batch.reset(); }
Counters GetCounters() { return counters; }

bool CpuAccess(spritectl_surface_t surface, bool write, bool borrow)
{
	if (!surface || !surface->surface) return false;
	if (!device) { if (borrow) surface->cpu_borrowed = 1; return true; }
	auto found = surfaces.find(surface);
	if (found == surfaces.end()) {
		if (borrow) surface->cpu_borrowed = 1;
		return true;
	}
	auto& state = found->second;
	if (state.owner == Owner::Gpu) {
		Finish();
		RenderState restore;
		if (!restore.Bind(state.target)
			|| SDL_RenderReadPixels(device, nullptr, surface->surface->format->format,
				surface->surface->pixels, surface->surface->pitch) != 0) {
			SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Sprite readback failed: %s", SDL_GetError());
			return false;
		}
		++counters.readbacks;
		state.owner = Owner::Both;
	}
	if (write) { state.owner = Owner::Cpu; ++state.revision; }
	if (borrow) surface->cpu_borrowed = 1;
	return true;
}

void EndBorrow(spritectl_surface_t surface)
{
	if (surface) surface->cpu_borrowed = 0;
}

void ForgetSurface(spritectl_surface_t surface)
{
	if (upscaler.source == surface) upscaler.source = nullptr;
	if (auto found = surfaces.find(surface); found != surfaces.end()) {
		Finish();
		SDL_DestroyTexture(found->second.target);
		SDL_DestroyTexture(found->second.upload);
		SDL_DestroyTexture(found->second.snapshot);
		counters.surfaceBytes -= found->second.bytes;
		surfaces.erase(found);
	}
}

void ForgetSprite(spritectl_sprite_t sprite)
{
	if (auto found = sprites.find(sprite); found != sprites.end()) {
		SDL_DestroyTexture(found->second.texture);
		counters.textureBytes -= found->second.bytes;
		sprites.erase(found);
	}
}

void ForgetPaletteSprite(const CSpritePalBase* sprite)
{
	if (auto found = paletteSprites.find(sprite); found != paletteSprites.end()) {
		SDL_DestroyTexture(found->second.texture);
		counters.textureBytes -= found->second.bytes;
		paletteSprites.erase(found);
	}
}

void ClearPaletteTextures()
{
	while (!paletteSprites.empty()) ForgetPaletteSprite(paletteSprites.begin()->first);
	if (paletteTexture) {
		SDL_DestroyTexture(paletteTexture);
		paletteTexture = nullptr;
		counters.textureBytes -= sizeof(paletteColors);
	}
	paletteColors = {};
}

void Detach(SDL_Renderer* renderer)
{
	if (!device || (renderer && renderer != device)) return;
	Finish();
	// A device replacement preserves CPU contents before destroying textures.
	for (auto& entry : surfaces) {
		CpuAccess(entry.first, false);
		if (entry.first->renderer == device) {
			SDL_DestroyTexture(entry.first->texture);
			entry.first->texture = nullptr;
			entry.first->renderer = nullptr;
		}
	}
	while (!surfaces.empty()) ForgetSurface(surfaces.begin()->first);
	while (!sprites.empty()) ForgetSprite(sprites.begin()->first);
	ClearPaletteTextures();
	ReleaseUpscaler();
	SpriteGpuEffects::Detach();
	SDL_Log("Sprite composition: %llu draws, %llu sprite uploads, %llu surface uploads, %llu readbacks",
		static_cast<unsigned long long>(counters.draws), static_cast<unsigned long long>(counters.spriteUploads),
		static_cast<unsigned long long>(counters.surfaceUploads), static_cast<unsigned long long>(counters.readbacks));
	device = nullptr;
}

bool CheckpointOffscreen(spritectl_surface_t presented)
{
	// Persistent cursor/background surfaces must survive a render-target
	// reset. Only copy ones modified since their last checkpoint. The presented
	// frame is regenerated by the application after reset and needs no copy.
	for (auto& entry : surfaces)
		if (entry.first != presented && !entry.first->transient
			&& !CpuAccess(entry.first, false)) return false;
	return true;
}

void Reset()
{
	if (!device) return;
	Finish();
	for (auto& entry : surfaces) {
		entry.second.owner = Owner::Cpu;
		if (entry.first->renderer == device) {
			SDL_DestroyTexture(entry.first->texture);
			entry.first->texture = nullptr;
			entry.first->renderer = nullptr;
		}
	}
	while (!surfaces.empty()) ForgetSurface(surfaces.begin()->first);
	while (!sprites.empty()) ForgetSprite(sprites.begin()->first);
	ClearPaletteTextures();
	ReleaseUpscaler();
	SpriteGpuEffects::Attach(device);
}

bool DrawPalette(spritectl_surface_t dest, int x, int y, const CSpritePalBase* sprite,
	const MPalette& palette, bool alpha, bool screen)
{
	if (!SpriteGpuEffects::Active() || !sprite || !sprite->IsInit() || !dest || !dest->surface) return false;
	// Legacy palette blits clip against surface bounds, ignoring its clip rect.
	const SDL_Rect clip{0, 0, dest->width, dest->height};
	if (int64_t(x) + sprite->GetWidth() <= 0 || int64_t(y) + sprite->GetHeight() <= 0
		|| x >= dest->width || y >= dest->height || sprite->IsEmptySprite()) return true;
	try {
		Surface* target = Target(dest, true);
		if (!target || !UpdatePalette(palette)) return false;
		SDL_Texture* texture = PaletteSpriteTexture(sprite, alpha);
		if (!texture) return false;
		Finish();
		const SDL_Rect placement{x, y, sprite->GetWidth(), sprite->GetHeight()};
		SDL_Rect visible{};
		if (!SDL_IntersectRect(&placement, &clip, &visible)) return true;
		if ((screen || alpha) && !Snapshot(dest, *target, &visible)) return false;
		RenderState restore;
		using Effect = SpriteGpuEffects::Effect;
		const Effect effect = alpha ? Effect::PaletteAlpha : screen ? Effect::PaletteScreen : Effect::PaletteCopy;
		if (!restore.Bind(target->target) || !SpriteGpuEffects::Draw(texture, dest->width,
			dest->height, placement, clip, effect, 0, nullptr, target->snapshot, paletteTexture)) return false;
		MarkGpu(*target);
		++counters.draws;
		++counters.shaderDraws;
		return true;
	} catch (const std::bad_alloc&) { return false; }
}

bool DrawEffect(spritectl_surface_t dest, int x, int y, spritectl_sprite_t sprite,
	SpriteGpuEffects::Effect effect, int value, const Uint16* gradation)
{
	if (!SpriteGpuEffects::Active() || !sprite || !dest || !dest->surface) return false;
	const auto& clip = dest->surface->clip_rect;
	if (int64_t(x) + sprite->width <= clip.x || int64_t(y) + sprite->height <= clip.y
		|| int64_t(x) >= int64_t(clip.x) + clip.w || int64_t(y) >= int64_t(clip.y) + clip.h) return true;
	try {
		Surface* target = Target(dest, true);
		if (!target) return false;
		SDL_Texture* texture = SpriteTexture(sprite);
		if (!texture) return false;
		Finish();
		RenderState restore;
		const SDL_Rect placement{x, y, sprite->width, sprite->height};
		if (!restore.Bind(target->target) || !SpriteGpuEffects::Draw(texture, dest->width,
			dest->height, placement, clip, effect, value, gradation)) return false;
		MarkGpu(*target);
		++counters.draws;
		++counters.shaderDraws;
		return true;
	} catch (const std::bad_alloc&) { return false; }
}

static bool SurfaceEffect(spritectl_surface_t dest, const SDL_Rect& rect, SpriteGpuEffects::Effect effect, int value)
{
	if (!SpriteGpuEffects::Active()) return false;
	try {
		Surface* target = Target(dest, true);
		if (!target) return false;
		Finish();
		if (!Snapshot(dest, *target, &rect)) return false;
		RenderState restore;
		const SDL_Rect placement{0, 0, dest->width, dest->height};
		if (!restore.Bind(target->target) || !SpriteGpuEffects::Draw(target->snapshot, dest->width,
			dest->height, placement, rect, effect, value)) return false;
		MarkGpu(*target);
		++counters.draws;
		++counters.shaderDraws;
		return true;
	} catch (const std::bad_alloc&) { return false; }
}

bool Gamma(spritectl_surface_t dest, const SDL_Rect& rect, int value)
{
	return value >= 0 && SurfaceEffect(dest, rect, SpriteGpuEffects::Effect::Gamma, (std::min)(value, 2048));
}

bool Tint(spritectl_surface_t dest, const SDL_Rect& rect, int channel)
{
	return SurfaceEffect(dest, rect, SpriteGpuEffects::Effect::Color, channel);
}

bool LightGrid(spritectl_surface_t dest, std::span<const SpriteGpuEffects::LightCell> cells)
{
	if (!SpriteGpuEffects::Active()) return false;
	if (cells.empty()) return true;
	try {
		Surface* target = Target(dest, true);
		if (!target) return false;
		Finish();
		if (!Snapshot(dest, *target)) return false;
		RenderState restore;
		if (!restore.Bind(target->target)
			|| !SpriteGpuEffects::LightGrid(target->snapshot, dest->width, dest->height, cells)) return false;
		MarkGpu(*target);
		++counters.draws;
		++counters.shaderDraws;
		return true;
	} catch (const std::bad_alloc&) { return false; }
}

bool Draw(spritectl_surface_t dest, int x, int y, spritectl_sprite_t sprite, int flags, int alpha, int scale)
{
	if (!device || !sprite || !dest) return false;
	const int64_t width = int64_t(sprite->width) * scale / 256;
	const int64_t height = int64_t(sprite->height) * scale / 256;
	if (width <= 0 || height <= 0) return true;
	if (width > INT_MAX || height > INT_MAX) return false;
	// Preserve the current 565 RLE path: it ignores alpha and flip flags.
	const auto& clip = dest->surface->clip_rect;
	if (int64_t(x) + width <= clip.x || int64_t(y) + height <= clip.y
		|| int64_t(x) >= int64_t(clip.x) + clip.w || int64_t(y) >= int64_t(clip.y) + clip.h) return true;
	try {
		Surface* target = Target(dest, true);
		if (!target) return false;
		SDL_Texture* texture = SpriteTexture(sprite);
		if (!texture) return false;
		const int opacity = !sprite->has_rle && (flags & SPRITECTL_BLT_ALPHA) ? std::clamp(alpha, 0, 255) : 255;
		if (SDL_SetTextureAlphaMod(texture, Uint8(opacity)) != 0) return false;
		const SDL_Rect placement{x, y, int(width), int(height)};
		if (!BindForDraw(target->target, &clip) || SDL_RenderCopy(device, texture, nullptr, &placement) != 0) return false;
		MarkGpu(*target);
		++counters.draws;
		return true;
	} catch (const std::bad_alloc&) { return false; }
}

bool Fill(spritectl_surface_t dest, const SDL_Rect* rect, uint32_t color, bool ignoreClip)
{
	if (!device) return false;
	try {
		Surface* target = Target(dest, true);
		if (!target) return false;
		const uint32_t rgb = Color(uint16_t(color), dest->format);
		if (!BindForDraw(target->target, ignoreClip ? nullptr : &dest->surface->clip_rect)
			|| SDL_SetRenderDrawBlendMode(device, SDL_BLENDMODE_NONE) != 0
			|| SDL_SetRenderDrawColor(device, Uint8(rgb >> 16), Uint8(rgb >> 8), Uint8(rgb), 255) != 0
			|| SDL_RenderFillRect(device, rect) != 0) return false;
		MarkGpu(*target);
		++counters.draws;
		return true;
	} catch (const std::bad_alloc&) { return false; }
}

bool Copy(spritectl_surface_t dest, const SDL_Rect& to, spritectl_surface_t src, const SDL_Rect& from)
{
	if (!device) return false;
	try {
		Surface* source = Target(src, false);
		if (!source) return false;
		Surface* target = Target(dest, true);
		if (!target) return false;
		SDL_Texture* texture = source->target;
		if (dest == src) {
			if (!Snapshot(src, *source, &from)) return false;
			texture = source->snapshot;
		}
		if (!BindForDraw(target->target, &dest->surface->clip_rect)
			|| SDL_RenderCopy(device, texture, &from, &to) != 0) return false;
		MarkGpu(*target);
		++counters.draws;
		return true;
	} catch (const std::bad_alloc&) { return false; }
}

SDL_Texture* PresentationTexture(spritectl_surface_t surface, SDL_Renderer* renderer)
{
	if (!device || renderer != device) return nullptr;
	Finish();
	try {
		Surface* source = Target(surface, false);
		return source ? source->target : nullptr;
	} catch (const std::bad_alloc&) { return nullptr; }
}

void ReleaseUpscaler()
{
	if (device) Finish();
	SDL_DestroyTexture(upscaler.corners);
	SDL_DestroyTexture(upscaler.output);
	upscaler = {};
	counters.upscaleBytes = 0;
}

SDL_Texture* UpscaledTexture(spritectl_surface_t surface, SDL_Renderer* renderer, int factor)
{
	if (!device || renderer != device || !SpriteGpuEffects::XbrzAvailable() || !surface
		|| factor < 2 || factor > 4) return nullptr;
	const int64_t width = int64_t(surface->width) * factor;
	const int64_t height = int64_t(surface->height) * factor;
	const uint64_t bytes = uint64_t(surface->width) * surface->height * 4 * (1 + factor * factor);
	if (width <= 0 || height <= 0 || width > 16384 || height > 16384
		|| bytes > 64ull * 1024 * 1024 || !Fits(int(width), int(height))) return nullptr;
	Finish();
	try {
		Surface* state = Target(surface, false);
		if (!state) return nullptr;
		if (upscaler.width != surface->width || upscaler.height != surface->height || upscaler.factor != factor) {
			ReleaseUpscaler();
			upscaler.corners = Texture(surface->width, surface->height, SDL_TEXTUREACCESS_TARGET);
			upscaler.output = Texture(int(width), int(height), SDL_TEXTUREACCESS_TARGET);
			if (!upscaler.corners || !upscaler.output) { ReleaseUpscaler(); return nullptr; }
			upscaler.width = surface->width;
			upscaler.height = surface->height;
			upscaler.factor = factor;
			counters.upscaleBytes = bytes;
		}
		if (upscaler.source != surface || upscaler.revision != state->revision) {
			upscaler.source = nullptr;
			RenderState restore;
			SDL_SetTextureScaleMode(state->target, SDL_ScaleModeNearest);
			if (!restore.Bind(upscaler.corners)
				|| !SpriteGpuEffects::Xbrz(state->target, nullptr, surface->width, surface->height, factor)
				|| !restore.Bind(upscaler.output)
				|| !SpriteGpuEffects::Xbrz(state->target, upscaler.corners, surface->width, surface->height, factor)) return nullptr;
			upscaler.source = surface;
			upscaler.revision = state->revision;
			++counters.upscaledFrames;
		}
		return upscaler.output;
	} catch (const std::bad_alloc&) { return nullptr; }
}
}
