#include "test_framework.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackendSDL.h"
#include <climits>
#include <cstring>

namespace {
WORD Pixel(CSpriteSurface& surface, int x, int y)
{
	spritectl_surface_info_t info{};
	CHECK_EQ(0, spritectl_lock_surface(surface.GetBackendSurface(), &info));
	if (!info.pixels) return 0;
	const WORD value = reinterpret_cast<WORD*>(static_cast<BYTE*>(info.pixels) + y * info.pitch)[x];
	spritectl_unlock_surface(surface.GetBackendSurface());
	return value;
}

void Pattern(CSpriteSurface& surface)
{
	spritectl_surface_info_t info{};
	CHECK_EQ(0, spritectl_lock_surface(surface.GetBackendSurface(), &info));
	if (!info.pixels) return;
	for (int y = 0; y < info.height; ++y)
		for (int x = 0; x < info.width; ++x)
			reinterpret_cast<WORD*>(static_cast<BYTE*>(info.pixels) + y * info.pitch)[x] = 1 + y * info.width + x;
	spritectl_unlock_surface(surface.GetBackendSurface());
}
}

TEST(SpriteSurfaceBounds, FillRespectsPitchAndPreservesPadding)
{
	CHECK_EQ(0, spritectl_init());
	CSpriteSurface surface;
	CHECK(surface.Init(3, 3));
	SDL_Surface* raw = surface.GetBackendSurface()->surface;
	CHECK(static_cast<size_t>(raw->pitch) > 3 * sizeof(WORD));
	std::memset(raw->pixels, 0x5a, raw->pitch * raw->h);
	surface.FillSurface(0x1234);
	for (int y = 0; y < 3; ++y) {
		for (int x = 0; x < 3; ++x) CHECK_EQ(0x1234, Pixel(surface, x, y));
		for (int x = 6; x < raw->pitch; ++x)
			CHECK_EQ(0x5a, static_cast<BYTE*>(raw->pixels)[y * raw->pitch + x]);
	}
}

TEST(SpriteSurfaceBounds, DescriptionsBelongToEachSurface)
{
	CHECK_EQ(0, spritectl_init());
	CSpriteSurface first, second;
	CHECK(first.Init(3, 2));
	CHECK(second.Init(7, 4));
	const S_SURFACEINFO* a = first.GetDDSD();
	const S_SURFACEINFO* b = second.GetDDSD();
	CHECK(a != b);
	CHECK_EQ(3, a->width);
	CHECK_EQ(2, a->height);
	CHECK_EQ(7, b->width);
	CHECK(a->p_surface != b->p_surface);
}

TEST(SpriteSurfaceBounds, BothBlitsClipEveryEdgeAndPreserveInputs)
{
	CHECK_EQ(0, spritectl_init());
	CSpriteSurface source, dest;
	CHECK(source.Init(3, 3));
	CHECK(dest.Init(5, 5));
	Pattern(source);
	// Exhaust small rectangles across all four edges, with independently
	// displaced destinations. A separate test covers empty/reversed bounds.
	for (bool noKey : {false, true})
	for (int sx = -2; sx <= 4; ++sx)
	for (int sy = -2; sy <= 4; ++sy)
	for (int dx = -2; dx <= 5; ++dx)
	for (int dy = -2; dy <= 5; ++dy) {
		dest.FillSurface(0x7777);
		POINT point{dx, dy};
		RECT rect{sx, sy, sx + 3, sy + 3};
		if (noKey) dest.BltNoColorkey(&point, &source, &rect);
		else dest.Blt(&point, &source, &rect);
		CHECK_EQ(dx, point.x);
		CHECK_EQ(sx, rect.left);
		for (int y = 0; y < 5; ++y)
		for (int x = 0; x < 5; ++x) {
			int rx = x - dx, ry = y - dy;
			int ox = sx + rx, oy = sy + ry;
			WORD expected = rx >= 0 && rx < 3 && ry >= 0 && ry < 3
				&& ox >= 0 && ox < 3 && oy >= 0 && oy < 3 ? 1 + oy * 3 + ox : 0x7777;
			CHECK_EQ(expected, Pixel(dest, x, y));
		}
	}
	CHECK_EQ(0, source.GetBackendSurface()->locked);
	CHECK_EQ(0, dest.GetBackendSurface()->locked);
}

TEST(SpriteSurfaceBounds, EmptyExtremeAndClippedBlits)
{
	CHECK_EQ(0, spritectl_init());
	CSpriteSurface source, dest;
	CHECK(source.Init(3, 3));
	CHECK(dest.Init(3, 3));
	Pattern(source);
	dest.FillSurface(0);
	RECT clip{1, 1, 2, 2};
	dest.SetClip(&clip);
	POINT origin{0, 0};
	dest.Blt(&origin, &source, nullptr);
	for (int y = 0; y < 3; ++y)
		for (int x = 0; x < 3; ++x)
			CHECK_EQ(x == 1 && y == 1 ? 5 : 0, Pixel(dest, x, y));
	dest.SetClipNULL();
	for (RECT rect : {RECT{2, 2, 1, 1}, RECT{0, 0, 0, 3}, RECT{INT_MAX, 0, INT_MIN, 1}})
		dest.Blt(&origin, &source, &rect);
	for (POINT point : {POINT{INT_MIN, 0}, POINT{INT_MAX, 0}, POINT{0, INT_MIN}, POINT{0, INT_MAX}})
		dest.Blt(&point, &source, nullptr);
	CHECK_EQ(5, Pixel(dest, 1, 1));
	CHECK_EQ(0, Pixel(dest, 0, 0));
}

TEST(SpriteSurfaceBounds, OverlapUsesTheOriginalPixels)
{
	CHECK_EQ(0, spritectl_init());
	CSpriteSurface surface;
	CHECK(surface.Init(5, 5));
	for (bool noKey : {false, true})
	for (int dx = -1; dx <= 1; ++dx)
	for (int dy = -1; dy <= 1; ++dy) {
		Pattern(surface);
		POINT point{dx, dy};
		if (noKey) surface.BltNoColorkey(&point, &surface, nullptr);
		else surface.Blt(&point, &surface, nullptr);
		for (int y = 0; y < 5; ++y)
		for (int x = 0; x < 5; ++x) {
			int sx = x - dx, sy = y - dy;
			WORD expected = sx >= 0 && sx < 5 && sy >= 0 && sy < 5
				? 1 + sy * 5 + sx : 1 + y * 5 + x;
			CHECK_EQ(expected, Pixel(surface, x, y));
		}
	}
}

TEST(SpriteSurfaceBounds, DrawRectConvertsGreenWithoutChangingBlue)
{
	CHECK_EQ(0, spritectl_init());
	CSpriteSurface surface;
	CHECK(surface.Init(1, 1));
	auto backend = surface.GetBackendSurface();
	SDL_FreeSurface(backend->surface);
	backend->surface = spritectl_sdl_create_surface(1, 1, SPRITECTL_FORMAT_RGB555);
	backend->format = SPRITECTL_FORMAT_RGB555;
	RECT rect{0, 0, 1, 1};
	for (WORD color : {WORD{0x0020}, WORD{0x07e0}, WORD{0xffff}, WORD{0xf800}, WORD{0x001f}}) {
		surface.DrawRect(&rect, color);
		CHECK_EQ(((color & 0xf800) >> 1) | ((color & 0x07c0) >> 1) | (color & 0x001f), Pixel(surface, 0, 0));
	}
}

TEST(SpriteSurfaceBounds, ShutdownPreservesOtherSDLSubsystemOwners)
{
	CHECK_EQ(0, SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_TIMER));
	CHECK_EQ(0, spritectl_init());
	spritectl_shutdown();
	CHECK((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) != 0);
	CHECK((SDL_WasInit(SDL_INIT_TIMER) & SDL_INIT_TIMER) != 0);
	SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_TIMER);
}

TEST(SpriteSurfaceBounds, CompressedPixelsStayLockedDuringDrawing)
{
	CHECK_EQ(0, spritectl_init());
	CSpriteSurface source, dest;
	CHECK(source.Init(3, 3));
	CHECK(dest.Init(3, 3));
	Pattern(source);
	SDL_Surface* raw = source.GetBackendSurface()->surface;
	CHECK_EQ(0, SDL_SetColorKey(raw, SDL_TRUE, 0));
	CHECK_EQ(0, SDL_SetSurfaceRLE(raw, 1));
	CHECK_EQ(0, SDL_BlitSurface(raw, nullptr, dest.GetBackendSurface()->surface, nullptr));
	CHECK(SDL_MUSTLOCK(raw));
	if (!SDL_MUSTLOCK(raw)) return;
	S_SURFACEINFO borrowed{};
	source.GetSurfaceInfo(&borrowed);
	CHECK(borrowed.p_surface == nullptr);
	CHECK(source.GetSurfacePointer() == nullptr);
	CHECK(!source.Lock());
	CHECK(!source.IsLock());
	POINT origin{0, 0};
	dest.Blt(&origin, &source, nullptr);
	for (int y = 0; y < 3; ++y)
		for (int x = 0; x < 3; ++x) CHECK_EQ(1 + y * 3 + x, Pixel(dest, x, y));
	RECT rect{0, 0, 3, 3};
	source.GammaBox565(&rect, 0);
	for (int y = 0; y < 3; ++y)
		for (int x = 0; x < 3; ++x) CHECK_EQ(0, Pixel(source, x, y));
	CHECK_EQ(0, raw->locked);
	CHECK_EQ(0, source.GetBackendSurface()->locked);
	CHECK_EQ(0, dest.GetBackendSurface()->locked);
}

TEST(SpriteSurfaceBounds, LegacyLockIsIdempotentAndReleaseClearsIt)
{
	CHECK_EQ(0, spritectl_init());
	CSpriteSurface surface;
	CHECK(surface.Init(3, 3));
	CHECK(surface.Lock());
	void* pixels = surface.Lock(nullptr);
	CHECK(pixels != nullptr);
	CHECK(surface.Lock(nullptr) == pixels);
	CHECK(surface.GetSurfacePointer() == pixels);
	CHECK(surface.IsLock());
	CHECK_EQ(0, surface.GetBackendSurface()->locked);
	surface.Unlock();
	CHECK(!surface.IsLock());
	surface.Lock();
	surface.Release();
	CHECK(!surface.IsLock());
	CHECK(surface.GetDDSD()->p_surface == nullptr);
}
