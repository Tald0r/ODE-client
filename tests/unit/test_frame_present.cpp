#include "test_framework.h"
#include "DXLib/CDirectDraw.h"
#include "SpriteLibBackendSDL.h"
#include <cstdint>
#include <vector>

// Presenting a frame (task 4.211). Flip draws the back buffer the
// executable's source gives (BackBuffer: none without a source) through
// PresentFrame, which does nothing without a renderer and otherwise clears,
// draws a valid back buffer letterboxed, and presents.
namespace {

spritectl_surface_t s_given = SPRITECTL_INVALID_SURFACE;
spritectl_surface_t Given() { return s_given; }

// A 100x60 software target, filled with a colour PresentFrame must replace.
struct Target
{
	SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 100, 60, 32, SDL_PIXELFORMAT_ARGB8888);
	SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
	Target() { if (surface) SDL_FillRect(surface, nullptr, 0xFF123456); }
	~Target()
	{
		if (renderer) spritectl_release_present_resources(renderer);
		if (renderer) SDL_DestroyRenderer(renderer);
		if (surface) SDL_FreeSurface(surface);
	}
	// The surface itself, not SDL_RenderReadPixels, which would flush
	// queued drawing that only SDL_RenderPresent is meant to flush.
	std::vector<uint32_t> Pixels() const
	{
		std::vector<uint32_t> out;
		for (int y = 0; y < 60; ++y)
		{
			const auto* row = reinterpret_cast<const uint32_t*>(static_cast<const unsigned char*>(surface->pixels) + y * surface->pitch);
			for (int x = 0; x < 100; ++x) out.push_back(row[x] & 0xFFFFFF);
		}
		return out;
	}
};

} // namespace

TEST(FramePresent, TheBackBufferComesFromTheInstalledSource)
{
	CHECK(CSDLGraphics::BackBuffer() == SPRITECTL_INVALID_SURFACE);
	spritectl_surface_s surface{};
	s_given = &surface;
	CSDLGraphics::SetBackBufferSource(Given);
	CHECK(CSDLGraphics::BackBuffer() == &surface);
	s_given = SPRITECTL_INVALID_SURFACE;
	CHECK(CSDLGraphics::BackBuffer() == SPRITECTL_INVALID_SURFACE);
	CSDLGraphics::SetBackBufferSource(nullptr);
	CHECK(CSDLGraphics::BackBuffer() == SPRITECTL_INVALID_SURFACE);
}

TEST(FramePresent, WithoutARendererNothingHappens)
{
	CHECK_EQ(0, spritectl_init());
	spritectl_surface_t back = spritectl_create_surface(16, 12, SPRITECTL_FORMAT_RGB565);
	CHECK(back != SPRITECTL_INVALID_SURFACE);
	CSDLGraphics::PresentFrame(nullptr, back);
	CSDLGraphics::PresentFrame(nullptr, SPRITECTL_INVALID_SURFACE);
	spritectl_destroy_surface(back);
}

TEST(FramePresent, NoBackBufferPresentsAClearedFrame)
{
	CHECK_EQ(0, spritectl_init());
	Target target;
	CHECK(target.renderer != nullptr);
	if (!target.renderer) return;
	CSDLGraphics::PresentFrame(target.renderer, SPRITECTL_INVALID_SURFACE);
	const auto pixels = target.Pixels();
	CHECK(pixels.size() == 100u * 60u);
	bool allBlack = !pixels.empty();
	for (uint32_t p : pixels) allBlack = allBlack && p == 0;
	CHECK(allBlack);
}

TEST(FramePresent, TheBackBufferIsDrawnLetterboxed)
{
	CHECK_EQ(0, spritectl_init());
	const int xbrz = spritectl_get_xbrz_enabled();
	spritectl_set_xbrz_enabled(0);
	Target target;
	spritectl_surface_t back = spritectl_create_surface(16, 12, SPRITECTL_FORMAT_RGB565);
	CHECK(target.renderer != nullptr && back != SPRITECTL_INVALID_SURFACE);
	if (target.renderer && back)
	{
		SDL_FillRect(back->surface, nullptr, 0xFFFF);
		CSDLGraphics::PresentFrame(target.renderer, back);
		// 16x12 into 100x60 scales by 5 to 80x60, centred: white from x 10
		// to 89, the cleared bars black.
		const auto pixels = target.Pixels();
		CHECK(pixels.size() == 100u * 60u);
		int wrong = pixels.empty() ? 1 : 0;
		for (size_t i = 0; i < pixels.size(); ++i)
		{
			const int x = static_cast<int>(i % 100);
			const uint32_t want = (x >= 10 && x < 90) ? 0xFFFFFFu : 0u;
			wrong += pixels[i] != want;
		}
		CHECK_EQ(0, wrong);
	}
	if (back) spritectl_destroy_surface(back);
	spritectl_set_xbrz_enabled(xbrz);
}
