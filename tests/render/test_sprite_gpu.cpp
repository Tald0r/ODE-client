#include "test_framework.h"
#include "xbrz.h"
#include "SpriteGpu.h"
#include "CSpriteSurface.h"
#include "CSpritePal.h"
#include "CAlphaSpritePal.h"
#include "CSprite565.h"
#include "CIndexSprite.h"
#include "CShadowSprite.h"
#include "FrameUpscaler.h"
#include "CFilter.h"
#include "MixedScene.h"
#include "../support/InvisibilitySprite.h"
#include <cstdio>
#include <climits>
#include <fstream>
#include <vector>

extern SDL_Renderer* testSpriteRenderer;

TEST(SpriteGpu, InvisibilityWipePreservesBackgroundWithoutReadbacks)
{
	CHECK(SpriteGpu::Attach(testSpriteRenderer, true));
	invisibility_test::CheckFade();
	SpriteGpu::Detach();
}

namespace {
struct Fixture {
	CSpriteSurface surface;
	spritectl_sprite_t sprite = nullptr;
	Fixture()
	{
		CHECK(SpriteGpu::Attach(testSpriteRenderer, true));
		CHECK(surface.Init(8, 8));
		surface.FillSurface(0x07e0);
		sprite = spritectl_create_sprite_rle(4, 2);
		CHECK(sprite != nullptr);
		// Transparent, opaque black, white, transparent. Black must survive
		// texture conversion: RLE holes are not the same as black pixels.
		const uint16_t line[]{1, 1, 2, 0, 0xffff};
		for (int y = 0; y < 2; ++y) CHECK_EQ(0, spritectl_sprite_set_scanline_rle(sprite, y, line, 5));
	}
	~Fixture()
	{
		spritectl_destroy_sprite(sprite);
		SpriteGpu::Detach();
	}
	void Draw(int x = 1, int y = 1)
	{
		CHECK_EQ(0, spritectl_blt_sprite(surface.GetBackendSurface(), x, y, sprite, 0, 255));
	}
	uint16_t Pixel(int x, int y)
	{
		auto backend = surface.GetBackendSurface();
		CHECK(SpriteGpu::CpuAccess(backend, false));
		return reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(backend->surface->pixels) + y * backend->surface->pitch)[x];
	}
};

template<class Base> struct PaletteFixture : Base {
	void BuildHoles(bool alpha)
	{
		this->Release();
		this->m_Width = 7; this->m_Height = 3;
		const int rowBytes = 7 + 3 * (alpha ? 2 : 1);
		this->m_Size = rowBytes * 3;
		this->AllocateDataAndScanlineTable(this->m_Size, 3);
		for (int y = 0; y < 3; ++y) {
			BYTE* p = this->m_pData + y * rowBytes;
			this->m_pPixels[y] = p;
			*p++ = 3;
			for (int x = 0; x < 3; ++x) {
				*p++ = 1; *p++ = 1;
				if (alpha) *p++ = BYTE(x == 1 ? 0 : 16 + 8 * x);
				*p++ = BYTE(x + y);
			}
		}
		this->m_bInit = true;
	}
	void Build(bool alpha, int seed = 0)
	{
		this->Release();
		this->m_Width = this->m_Height = 256;
		const int rowBytes = 5 + 256 * (alpha ? 2 : 1);
		this->m_Size = rowBytes * 256;
		this->AllocateDataAndScanlineTable(this->m_Size, 256);
		for (int y = 0; y < 256; ++y) {
			BYTE* p = this->m_pData + y * rowBytes;
			this->m_pPixels[y] = p;
			*p++ = 2;
			*p++ = 0; *p++ = 255;
			for (int x = 0; x < 256; ++x) {
				if (x == 255) { *p++ = 0; *p++ = 1; }
				if (alpha) *p++ = BYTE((x + y) % 33);
				*p++ = BYTE(x + y + seed);
			}
		}
		this->m_bInit = true;
	}
};
}

TEST(SpriteGpu, ReusesTextureAndPreservesRleHolesOpaqueBlackAndOrder)
{
	Fixture f;
	f.Draw(); f.Draw(2, 1);
	const auto drawn = SpriteGpu::GetCounters();
	CHECK_EQ(1u, drawn.spriteUploads);
	CHECK_EQ(0u, drawn.readbacks);
	CHECK_EQ(3u, drawn.draws);
	CHECK_EQ(0x07e0, f.Pixel(1, 1));
	CHECK_EQ(0, f.Pixel(2, 1));
	CHECK_EQ(0, f.Pixel(3, 1));
	CHECK_EQ(0xffff, f.Pixel(4, 1));
	CHECK_EQ(1u, SpriteGpu::GetCounters().readbacks);
}

TEST(SpriteGpu, ClipsNegativeAndExtremeCoordinatesWithoutWrapping)
{
	Fixture f;
	RECT clip{2, 2, 4, 4};
	f.surface.SetClip(&clip);
	f.Draw(1, 1);
	f.Draw(INT_MIN, 2); f.Draw(INT_MAX, 2);
	CHECK_EQ(0x07e0, f.Pixel(2, 1));
	CHECK_EQ(0, f.Pixel(2, 2));
	CHECK_EQ(0xffff, f.Pixel(3, 2));
	CHECK_EQ(0x07e0, f.Pixel(4, 2));
	f.surface.SetClipNULL();
	f.Draw(-2, 3);
	CHECK_EQ(0xffff, f.Pixel(0, 3));
}

TEST(SpriteGpu, PixelBorrowKeepsMixedCpuAndGpuDrawingInOrder)
{
	Fixture f;
	f.Draw();
	DWORD pitch = 0;
	auto* pixels = static_cast<uint8_t*>(f.surface.Lock(nullptr, &pitch));
	CHECK(pixels != nullptr);
	const auto draws = SpriteGpu::GetCounters().draws;
	f.Draw(2, 1); // Must write the same CPU image while pixels are borrowed.
	CHECK_EQ(draws, SpriteGpu::GetCounters().draws);
	reinterpret_cast<uint16_t*>(pixels + pitch)[3] = 0xf800;
	f.surface.Unlock();
	f.Draw(4, 1);
	CHECK_EQ(draws + 1, SpriteGpu::GetCounters().draws);
	CHECK_EQ(0xf800, f.Pixel(3, 1));
	CHECK_EQ(0, f.Pixel(5, 1));
	CHECK_EQ(0xffff, f.Pixel(6, 1));
}

TEST(SpriteGpu, DrawingLockAndPitchQueriesDoNotReadBack)
{
	Fixture f;
	f.Draw();
	CHECK(f.surface.Lock());
	CHECK(f.surface.Lock());
	CHECK_EQ(16, f.surface.GetSurfacePitch());
	f.Draw();
	f.surface.Unlock();
	CHECK_EQ(0u, SpriteGpu::GetCounters().readbacks);
	CHECK_EQ(3u, SpriteGpu::GetCounters().draws);
}

TEST(SpriteGpu, CpuEffectsSynchronizeAndLaterGpuDrawsPreserveThem)
{
	Fixture f;
	f.Draw();
	RECT rect{3, 1, 4, 2};
	f.surface.GammaBox565(&rect, 0);
	f.Draw(4, 1);
	CHECK_EQ(0, f.Pixel(3, 1));
	CHECK_EQ(0xffff, f.Pixel(6, 1));
}

TEST(SpriteGpu, ReplacingRleRowsInvalidatesTheUploadedTexture)
{
	Fixture f;
	f.Draw();
	const uint16_t replacement[]{1, 0, 1, 0xf800};
	CHECK_EQ(0, spritectl_sprite_set_scanline_rle(f.sprite, 0, replacement, 4));
	f.Draw(1, 4);
	CHECK_EQ(2u, SpriteGpu::GetCounters().spriteUploads);
	CHECK_EQ(0xf800, f.Pixel(1, 4));
	CHECK_EQ(0x07e0, f.Pixel(2, 4));
}

TEST(SpriteGpu, SurfaceCopiesAndOverlappingSelfCopiesRetainPixels)
{
	Fixture f;
	f.Draw();
	CSpriteSurface other;
	CHECK(other.Init(8, 8));
	POINT origin{0, 0};
	other.Blt(&origin, &f.surface, nullptr);
	CHECK_EQ(0u, SpriteGpu::GetCounters().readbacks);
	f.surface.FillSurface(0xf800);
	f.surface.Blt(&origin, &other, nullptr);
	CHECK_EQ(0, f.Pixel(2, 1));
	POINT shifted{1, 1};
	RECT from{0, 0, 7, 7};
	f.surface.Blt(&shifted, &f.surface, &from);
	CHECK_EQ(0, f.Pixel(3, 2));
	CHECK_EQ(0xffff, f.Pixel(4, 2));
}

TEST(SpriteGpu, ScaledSpritesUseTexturesAndNearestSampling)
{
	Fixture f;
	CHECK_EQ(0, spritectl_blt_sprite_scaled(f.surface.GetBackendSurface(), 0, 0, f.sprite, 512, 0));
	CHECK_EQ(1u, SpriteGpu::GetCounters().spriteUploads);
	CHECK_EQ(0, f.Pixel(2, 0));
	CHECK_EQ(0, f.Pixel(3, 0));
	CHECK_EQ(0xffff, f.Pixel(4, 0));
	CHECK_EQ(0xffff, f.Pixel(5, 0));
	CHECK_EQ(0x07e0, f.Pixel(6, 0));
}

TEST(SpriteGpu, DirectPresentationAvoidsReadbackAndRestoresWindowTarget)
{
	Fixture f;
	f.Draw();
	spritectl_set_xbrz_enabled(0);
	spritectl_finish_rendering();
	CHECK(SDL_GetRenderTarget(testSpriteRenderer) == nullptr);
	SDL_RenderClear(testSpriteRenderer);
	CHECK_EQ(0, spritectl_present_surface(f.surface.GetBackendSurface(), testSpriteRenderer));
	CHECK_EQ(0u, SpriteGpu::GetCounters().readbacks);
	std::vector<uint32_t> screen(64 * 64);
	CHECK_EQ(0, SDL_RenderReadPixels(testSpriteRenderer, nullptr, SDL_PIXELFORMAT_ARGB8888, screen.data(), 64 * 4));
	CHECK_EQ(0u, screen[16 * 64 + 16] & 0xffffff);
	CHECK_EQ(0xffffffu, screen[16 * 64 + 24] & 0xffffff);
}

TEST(SpriteGpu, DeviceDetachPreservesSurfacesAndReattachReuploadsSprites)
{
	Fixture f;
	f.Draw();
	SpriteGpu::Detach();
	CHECK(!SpriteGpu::Active());
	CHECK_EQ(0xffff, f.Pixel(3, 1));
	CHECK(SpriteGpu::Attach(testSpriteRenderer, true));
	f.Draw(1, 3);
	CHECK_EQ(1u, SpriteGpu::GetCounters().spriteUploads);
	CHECK_EQ(0xffff, f.Pixel(3, 3));
}

TEST(SpriteGpu, RgbaTextAlphaAndColorModulationDoNotLeakBetweenDraws)
{
	Fixture f;
	const uint32_t rgba[]{0x800000ff, 0x000000ff, 0xff0000ff};
	auto sprite = spritectl_create_sprite(3, 1, SPRITECTL_FORMAT_RGBA32, rgba, sizeof(rgba));
	CHECK(sprite != nullptr);
	CHECK_EQ(0, spritectl_blt_sprite(f.surface.GetBackendSurface(), 0, 0, sprite, SPRITECTL_BLT_ALPHA, 0));
	CHECK_EQ(0, spritectl_blt_sprite(f.surface.GetBackendSurface(), 0, 1, sprite, 0, 255));
	CHECK_EQ(0x07e0, f.Pixel(0, 0));
	CHECK_EQ(0x07e0, f.Pixel(1, 1));
	CHECK_EQ(0xf800, f.Pixel(2, 1));
	const auto blended = f.Pixel(0, 1);
	CHECK((blended >> 11) >= 15 && (blended >> 11) <= 16);
	CHECK(((blended >> 5) & 63) >= 30 && ((blended >> 5) & 63) <= 32);
	CHECK_EQ(0, blended & 31);
	spritectl_destroy_sprite(sprite);
}

TEST(SpriteGpu, ShadowSpritesDrawBlackAndDarkenWhatLiesUnderThem)
{
	// A row of four, its middle two pixels shadowed: drawn black, then
	// darkened by one bit (black at half), as the CPU path draws them.
	Fixture f;
	const char* path = "sprite_gpu_shadow.bin";
	{
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		for (int word : {4, 1, 3, 1, 1, 2}) { out.put(char(word & 0xFF)); out.put(char(word >> 8)); }
	}
	CShadowSprite shadow;
	{
		std::ifstream in(path, std::ios::binary);
		CHECK(shadow.LoadFromFile(in));
	}
	std::remove(path);
	POINT solid{1, 1}, dark{1, 3};
	const auto draws = SpriteGpu::GetCounters().draws;
	f.surface.BltShadowSprite(&solid, &shadow);
	f.surface.BltShadowSpriteDarkness(&dark, &shadow, 1);
	CHECK_EQ(draws + 2, SpriteGpu::GetCounters().draws);
	CHECK_EQ(0x07e0, f.Pixel(1, 1));
	CHECK_EQ(0, f.Pixel(2, 1));
	CHECK_EQ(0, f.Pixel(3, 1));
	CHECK_EQ(0x07e0, f.Pixel(4, 1));
	CHECK_EQ(0x07e0, f.Pixel(1, 3));
	for (int x : {2, 3})
	{
		const auto half = f.Pixel(x, 3);
		CHECK_EQ(0, half >> 11);
		CHECK(((half >> 5) & 63) >= 30 && ((half >> 5) & 63) <= 32);
		CHECK_EQ(0, half & 31);
	}
	CHECK_EQ(0x07e0, f.Pixel(4, 3));
	shadow.Release();
}

TEST(SpriteGpu, XbrzStillUsesTheCompositedFrame)
{
	Fixture f;
	f.Draw();
	spritectl_set_xbrz_enabled(1);
	spritectl_finish_rendering();
	SDL_RenderClear(testSpriteRenderer);
	CHECK_EQ(0, spritectl_present_surface(f.surface.GetBackendSurface(), testSpriteRenderer));
	CHECK_EQ(SpriteGpuEffects::XbrzAvailable() ? 0u : 1u, SpriteGpu::GetCounters().readbacks);
	if (SpriteGpuEffects::XbrzAvailable()) CHECK_EQ(1u, SpriteGpu::GetCounters().upscaledFrames);
	CHECK_EQ(0xffff, f.Pixel(3, 1));
	spritectl_set_xbrz_enabled(0);
}

TEST(SpriteGpu, UnreleasedPixelBorrowRemainsCpuOwnedAcrossPresentation)
{
	Fixture f;
	S_SURFACEINFO info{};
	f.surface.GetSurfaceInfo(&info);
	CHECK(info.p_surface != nullptr);
	auto* row = static_cast<uint16_t*>(info.p_surface);
	row[0] = 0xf800;
	spritectl_set_xbrz_enabled(0);
	CHECK_EQ(0, spritectl_present_surface(f.surface.GetBackendSurface(), testSpriteRenderer));
	row[0] = 0x001f; // A previously returned pointer is still valid.
	f.Draw();
	CHECK_EQ(0x001f, f.Pixel(0, 0));
	f.surface.Unlock();
}

TEST(SpriteGpu, OffscreenCheckpointSurvivesTargetResetWithoutReadingLostTextures)
{
	Fixture f;
	f.Draw();
	CSpriteSurface frame;
	CHECK(frame.Init(8, 8));
	POINT origin{0, 0};
	frame.Blt(&origin, &f.surface, nullptr);
	spritectl_set_xbrz_enabled(0);
	CHECK_EQ(0, spritectl_present_surface(frame.GetBackendSurface(), testSpriteRenderer));
	const auto readbacks = SpriteGpu::GetCounters().readbacks;
	CHECK_EQ(1u, readbacks); // Only the changed persistent surface is saved.
	spritectl_render_device_reset();
	CHECK_EQ(readbacks, SpriteGpu::GetCounters().readbacks);
	CHECK_EQ(0u, SpriteGpu::GetCounters().surfaceBytes);
	CHECK_EQ(0u, SpriteGpu::GetCounters().textureBytes);
	frame.Blt(&origin, &f.surface, nullptr);
	CHECK(SpriteGpu::CpuAccess(frame.GetBackendSurface(), false));
	const auto* row = reinterpret_cast<const uint16_t*>(
		static_cast<const uint8_t*>(frame.GetBackendSurface()->surface->pixels) + frame.GetSurfacePitch());
	CHECK_EQ(0xffff, row[3]);
}

TEST(SpriteGpu, AFullTextureCacheFreesAnEighthOldestFirst)
{
	// Eight 16 MiB sprites fill the 128 MiB cache. One eviction per upload
	// scanned every cached texture each time; a full cache now frees down
	// to 7/8 in one pass, so the upload after it evicts nothing.
	Fixture f;
	constexpr int side = 2048;
	constexpr uint64_t bytes = uint64_t(side) * side * 4;
	std::vector<uint16_t> pixels(size_t(side) * side, 0);
	std::vector<spritectl_sprite_t> big;
	for (int i = 0; i < 10; ++i) {
		big.push_back(spritectl_create_sprite(side, side, SPRITECTL_FORMAT_RGB565,
			pixels.data(), pixels.size() * sizeof(uint16_t)));
		CHECK(big.back() != nullptr);
	}
	auto draw = [&](int i) {
		CHECK_EQ(0, spritectl_blt_sprite(f.surface.GetBackendSurface(), 0, 0, big[i], 0, 255));
	};
	CHECK_EQ(0u, SpriteGpu::GetCounters().textureBytes);
	for (int i = 0; i < 8; ++i) draw(i);
	CHECK_EQ(8 * bytes, SpriteGpu::GetCounters().textureBytes);
	draw(0);	// now the most recent; 1 and 2 are the oldest
	draw(8);
	CHECK_EQ(7 * bytes, SpriteGpu::GetCounters().textureBytes);
	draw(9);
	CHECK_EQ(8 * bytes, SpriteGpu::GetCounters().textureBytes);
	const auto uploads = SpriteGpu::GetCounters().spriteUploads;
	draw(0);
	draw(3);
	CHECK_EQ(uploads, SpriteGpu::GetCounters().spriteUploads);
	for (auto sprite : big) spritectl_destroy_sprite(sprite);
}

TEST(SpriteGpu, SelfCopyDoesNotReadBackAndHandlesBothOverlapDirections)
{
	Fixture f;
	f.Draw();
	const auto readbacks = SpriteGpu::GetCounters().readbacks;
	POINT shifted{1, 1};
	RECT from{0, 0, 7, 7};
	f.surface.Blt(&shifted, &f.surface, &from);
	CHECK_EQ(readbacks, SpriteGpu::GetCounters().readbacks);
	CHECK_EQ(0, f.Pixel(3, 2));
	CHECK_EQ(0xffff, f.Pixel(4, 2));
	POINT origin{0, 0};
	RECT back{1, 1, 8, 8};
	const auto nextReadbacks = SpriteGpu::GetCounters().readbacks;
	f.surface.Blt(&origin, &f.surface, &back);
	CHECK_EQ(nextReadbacks, SpriteGpu::GetCounters().readbacks);
	CHECK_EQ(0, f.Pixel(2, 1));
	CHECK_EQ(0xffff, f.Pixel(3, 1));
}

TEST(SpriteGpu, ShaderGammaMatchesEveryRgb565ColorWithoutCpuComposition)
{
	Fixture f;
	if (!SpriteGpuEffects::Active()) return; // --shaders requires this capability.
	CSpriteSurface surface;
	CHECK(surface.Init(256, 256));
	for (int gamma : {0, 1, 16, 31, 32, 33, 63, 255, 2048}) {
		DWORD pitch = 0;
		auto* bytes = static_cast<uint8_t*>(surface.Lock(nullptr, &pitch));
		CHECK(bytes != nullptr);
		for (int y = 0; y < 256; ++y)
			for (int x = 0; x < 256; ++x)
				reinterpret_cast<uint16_t*>(bytes + y * pitch)[x] = uint16_t(y * 256 + x);
		surface.Unlock();
		const auto before = SpriteGpu::GetCounters();
		RECT rect{0, 0, 256, 256};
		surface.GammaBox565(&rect, gamma);
		CHECK_EQ(before.shaderDraws + 1, SpriteGpu::GetCounters().shaderDraws);
		CHECK_EQ(before.readbacks, SpriteGpu::GetCounters().readbacks);
		CHECK(SpriteGpu::CpuAccess(surface.GetBackendSurface(), false));
		std::vector<uint16_t> expected(65536);
		for (int i = 0; i < 65536; ++i) expected[i] = uint16_t(i);
		CSpriteSurface::Gamma4Pixel565(expected.data(), int(expected.size()), gamma);
		for (int y = 0; y < 256; ++y)
			for (int x = 0; x < 256; ++x)
				CHECK_EQ(expected[y * 256 + x], reinterpret_cast<uint16_t*>(bytes + y * pitch)[x]);
	}
}

TEST(SpriteGpu, ShaderSourceEffectsMatchEveryRgb565Color)
{
	Fixture f;
	if (!SpriteGpuEffects::Active()) return;
	CSpriteSurface surface;
	CHECK(surface.Init(256, 256));
	auto sprite = spritectl_create_sprite_rle(256, 256);
	CHECK(sprite != nullptr);
	for (int y = 0; y < 256; ++y) {
		uint16_t line[259]{1, 0, 256};
		for (int x = 0; x < 256; ++x) line[x + 3] = uint16_t(y * 256 + x);
		CHECK_EQ(0, spritectl_sprite_set_scanline_rle(sprite, y, line, 259));
	}
	using Effect = SpriteGpuEffects::Effect;
	const int previousValue = CSpriteSurface::s_Value1;
	for (auto effect : {Effect::Copy, Effect::Color, Effect::Darkness, Effect::GrayScale, Effect::Gradation}) {
		for (int value : {0, 1, 2, 6, 255}) {
			uint16_t gradation[94];
			for (int i = 0; i < 94; ++i) gradation[i] = uint16_t((i * 701) & 65535);
			const auto before = SpriteGpu::GetCounters();
			CHECK(SpriteGpu::DrawEffect(surface.GetBackendSurface(), 0, 0, sprite, effect, value, gradation));
			CHECK_EQ(before.shaderDraws + 1, SpriteGpu::GetCounters().shaderDraws);
			CHECK_EQ(before.readbacks, SpriteGpu::GetCounters().readbacks);
			std::vector<uint16_t> source(65536), expected(65536);
			for (int i = 0; i < 65536; ++i) source[i] = uint16_t(i);
			CSpriteSurface::s_Value1 = value;
			// The CPU routines take a WORD count, so compare one row at a time.
			for (int row = 0; row < 256; ++row) {
				auto* dest = expected.data() + row * 256;
				auto* src = source.data() + row * 256;
				if (effect == Effect::Color) CSpriteSurface::memcpyColor(dest, src, 256);
				else if (effect == Effect::Darkness) CSpriteSurface::memcpyDarkness(dest, src, 256);
				else if (effect == Effect::GrayScale) CSpriteSurface::memcpyEffectGrayScale(dest, src, 256);
				else for (int x = 0; x < 256; ++x)
					dest[x] = effect == Effect::Copy ? src[x]
						: gradation[(src[x] >> 11) + ((src[x] >> 6) & 31) + (src[x] & 31)];
			}
			CHECK(SpriteGpu::CpuAccess(surface.GetBackendSurface(), false));
			auto* bytes = static_cast<uint8_t*>(surface.GetBackendSurface()->surface->pixels);
			const int pitch = surface.GetSurfacePitch();
			for (int y = 0; y < 256; ++y)
				for (int x = 0; x < 256; ++x)
					CHECK_EQ(expected[y * 256 + x], reinterpret_cast<uint16_t*>(bytes + y * pitch)[x]);
		}
	}
	CSpriteSurface::s_Value1 = previousValue;
	spritectl_destroy_sprite(sprite);
}

TEST(SpriteGpu, PaletteShadersMatchCpuScreenAndAlphaIncludingPaletteMutation)
{
	Fixture f;
	if (!SpriteGpuEffects::Active()) return;
	CSpriteSurface surface;
	CHECK(surface.Init(256, 256));
	CSpriteSurface::InitEffectTable();
	const auto previousEffect = CSpriteSurface::s_pMemcpyPalEffectFunction;
	PaletteFixture<CSpritePal> plain;
	PaletteFixture<CAlphaSpritePal> alpha;
	plain.Build(false);
	alpha.Build(true);
	MPalette palette;
	palette.Init(255);
	for (int i = 0; i < 256; ++i) palette[BYTE(i)] = WORD(i * 257);
	for (int mode = 0; mode < 4; ++mode) {
		CSpriteSurface::s_pMemcpyPalEffectFunction = mode == 1 ? CSpriteSurface::memcpyPalEffectScreen : nullptr;
		DWORD pitch = 0;
		auto* pixels = static_cast<uint8_t*>(surface.Lock(nullptr, &pitch));
		CHECK(pixels != nullptr);
		std::vector<WORD> expected(65536);
		for (int i = 0; i < 65536; ++i) {
			expected[i] = WORD(i);
			reinterpret_cast<WORD*>(pixels + (i / 256) * pitch)[i % 256] = WORD(i);
		}
		surface.Unlock();
		if (mode == 3) palette[42] = 0xf800; // Mutation through a retained entry reference must work.
		POINT point{0, 0};
		const auto before = SpriteGpu::GetCounters();
		if (mode < 2) {
			CSpriteSurface::BltSpritePalEffectTo(expected.data(), 512, 256, 256, &point, &plain, palette);
			surface.BltSpritePalEffect(&point, &plain, palette);
		} else {
			CSpriteSurface::BltAlphaSpritePalTo(expected.data(), 512, 256, 256, &point, &alpha, palette);
			surface.BltAlphaSpritePal(&point, &alpha, palette);
		}
		CHECK_EQ(before.shaderDraws + 1, SpriteGpu::GetCounters().shaderDraws);
		CHECK_EQ(before.readbacks, SpriteGpu::GetCounters().readbacks);
		if (mode == 1 || mode == 3) CHECK_EQ(before.spriteUploads, SpriteGpu::GetCounters().spriteUploads);
		if (mode == 3) CHECK_EQ(before.paletteUploads + 1, SpriteGpu::GetCounters().paletteUploads);
		CHECK(SpriteGpu::CpuAccess(surface.GetBackendSurface(), false));
		for (int i = 0; i < 65536; ++i)
			CHECK_EQ(expected[i], reinterpret_cast<WORD*>(pixels + (i / 256) * pitch)[i % 256]);
	}
	const auto uploads = SpriteGpu::GetCounters().spriteUploads;
	alpha.Build(true, 73);
	POINT point{-17, -19};
	surface.BltAlphaSpritePal(&point, &alpha, palette);
	CHECK_EQ(uploads + 1, SpriteGpu::GetCounters().spriteUploads);
	CSpriteSurface::s_pMemcpyPalEffectFunction = previousEffect;
}

TEST(SpriteGpu, PaletteShaderClippingHolesResetAndFollowingSdlDraws)
{
	Fixture f;
	if (!SpriteGpuEffects::Active()) return;
	PaletteFixture<CSpritePal> plain;
	PaletteFixture<CAlphaSpritePal> alpha;
	plain.BuildHoles(false); alpha.BuildHoles(true);
	MPalette palette;
	palette.Init(5);
	palette[0] = 0; palette[1] = 0xffff; palette[2] = 0xf800;
	palette[3] = 0x07e0; palette[4] = 0x001f;
	CSpriteSurface::InitEffectTable();
	const auto previousEffect = CSpriteSurface::s_pMemcpyPalEffectFunction;
	CSpriteSurface::s_pMemcpyPalEffectFunction = CSpriteSurface::memcpyPalEffectScreen;
	for (int kind = 0; kind < 2; ++kind) {
		for (POINT point : {POINT{-2, -1}, POINT{5, 6}, POINT{1, 2}, POINT{INT_MIN, 0}, POINT{INT_MAX, 0}}) {
			f.surface.FillSurface(0x5a69);
			std::vector<WORD> expected(64, 0x5a69);
			const auto before = SpriteGpu::GetCounters();
			// The CPU clipping API assumes game-sized coordinates.
			const bool visible = point.x != INT_MIN && point.x != INT_MAX;
			if (kind == 0) {
				if (visible) CSpriteSurface::BltSpritePalEffectTo(expected.data(), 16, 8, 8, &point, &plain, palette);
				f.surface.BltSpritePalEffect(&point, &plain, palette);
			} else {
				if (visible) CSpriteSurface::BltAlphaSpritePalTo(expected.data(), 16, 8, 8, &point, &alpha, palette);
				f.surface.BltAlphaSpritePal(&point, &alpha, palette);
			}
			CHECK_EQ(before.readbacks, SpriteGpu::GetCounters().readbacks);
			if (visible) CHECK_EQ(before.shaderDraws + 1, SpriteGpu::GetCounters().shaderDraws);
			f.Draw(0, 6); // SDL commands after a shader must retain normal texture state.
			for (int y = 6; y < 8; ++y) { expected[y * 8 + 1] = 0; expected[y * 8 + 2] = 0xffff; }
			for (int y = 0; y < 8; ++y)
				for (int x = 0; x < 8; ++x) CHECK_EQ(expected[y * 8 + x], f.Pixel(x, y));
		}
	}
	CHECK(SpriteGpu::CheckpointOffscreen(nullptr));
	spritectl_render_device_reset();
	CHECK(SpriteGpuEffects::Active());
	CHECK_EQ(0u, SpriteGpu::GetCounters().textureBytes);
	POINT origin{0, 0};
	const auto before = SpriteGpu::GetCounters();
	f.surface.BltAlphaSpritePal(&origin, &alpha, palette);
	CHECK_EQ(before.spriteUploads + 1, SpriteGpu::GetCounters().spriteUploads);
	CHECK_EQ(before.paletteUploads + 1, SpriteGpu::GetCounters().paletteUploads);
	CHECK_EQ(before.readbacks, SpriteGpu::GetCounters().readbacks);
	CSpriteSurface::s_pMemcpyPalEffectFunction = previousEffect;
}

TEST(SpriteGpu, LegacySpriteEffectEntryPointsUseShadersAndRestoreState)
{
	Fixture f;
	if (!SpriteGpuEffects::Active()) return;
	CSprite565 sprite;
	WORD colors[8]{0, 0xffff, 0x8410, 0xf800, 0x001f, 0x07e0, 0x5a69, 0};
	sprite.SetPixelNoColorkey(colors, 8, 4, 2);
	CIndexSprite::SetColorSet();
	const int previousValue = CSpriteSurface::s_Value1;
	const auto previousEffect = CSpriteSurface::s_pMemcpyEffectFunction;
	for (int mode = 0; mode < 4; ++mode) {
		f.surface.FillSurface(0x07e0);
		RECT viewport{2, 2, 4, 4};
		f.surface.SetClip(&viewport);
		POINT origin{1, 2};
		const auto before = SpriteGpu::GetCounters();
		WORD expected[8];
		if (mode == 0) {
			f.surface.BltSpriteColor(&origin, &sprite, 2);
			CSpriteSurface::memcpyColor(expected, colors, 8);
		} else if (mode == 1) {
			f.surface.BltSpriteDarkness(&origin, &sprite, 3);
			CSpriteSurface::memcpyDarkness(expected, colors, 8);
		} else if (mode == 2) {
			f.surface.BltSpriteColorSet(&origin, &sprite, 0);
			CSpriteSurface::memcpyEffectGradation(expected, colors, 8);
		} else {
			CSpriteSurface::s_pMemcpyEffectFunction = CSpriteSurface::memcpyEffectGrayScale;
			f.surface.BltSpriteEffect(&origin, &sprite);
			CSpriteSurface::memcpyEffectGrayScale(expected, colors, 8);
		}
		CHECK_EQ(before.shaderDraws + 1, SpriteGpu::GetCounters().shaderDraws);
		CHECK_EQ(before.readbacks, SpriteGpu::GetCounters().readbacks);
		for (int y = 0; y < 8; ++y)
			for (int x = 0; x < 8; ++x)
				CHECK_EQ((x >= 2 && x < 4 && y >= 2 && y < 4) ? expected[(y - 2) * 4 + x - 1] : 0x07e0, f.Pixel(x, y));
	}
	CSpriteSurface::s_Value1 = previousValue;
	CSpriteSurface::s_pMemcpyEffectFunction = previousEffect;
}

namespace {
void CheckXbrzImage(const std::vector<WORD>& pixels, int width, int height)
{
	CSpriteSurface surface;
	CHECK(surface.Init(width, height));
	DWORD pitch = 0;
	auto* bytes = static_cast<BYTE*>(surface.Lock(nullptr, &pitch));
	CHECK(bytes != nullptr);
	for (int y = 0; y < height; ++y)
		std::memcpy(bytes + y * pitch, pixels.data() + y * width, width * sizeof(WORD));
	surface.Unlock();
	std::vector<uint32_t> rgb(pixels.size());
	CHECK_EQ(0, SDL_ConvertPixels(width, height, SDL_PIXELFORMAT_RGB565, pixels.data(), width * 2,
		SDL_PIXELFORMAT_RGB888, rgb.data(), width * 4));
	for (int factor = 2; factor <= 4; ++factor) {
		const auto before = SpriteGpu::GetCounters();
		auto* texture = SpriteGpu::UpscaledTexture(surface.GetBackendSurface(), testSpriteRenderer, factor);
		CHECK(texture != nullptr);
		if (!texture) continue;
		CHECK_EQ(before.upscaledFrames + 1, SpriteGpu::GetCounters().upscaledFrames);
		CHECK_EQ(before.readbacks, SpriteGpu::GetCounters().readbacks);
		const int outWidth = width * factor, outHeight = height * factor;
		std::vector<uint32_t> expected(size_t(outWidth) * outHeight), actual(expected.size());
		xbrz::scale(factor, rgb.data(), expected.data(), width, height, xbrz::ColorFormat::RGB);
		CHECK_EQ(0, SDL_SetRenderTarget(testSpriteRenderer, texture));
		CHECK_EQ(0, SDL_RenderReadPixels(testSpriteRenderer, nullptr, SDL_PIXELFORMAT_ARGB8888, actual.data(), outWidth * 4));
		CHECK_EQ(0, SDL_SetRenderTarget(testSpriteRenderer, nullptr));
		size_t differences = 0;
		for (size_t i = 0; i < expected.size(); ++i) {
			if ((actual[i] & 0xffffff) != (expected[i] & 0xffffff)) {
				if (!differences) std::fprintf(stderr, "xBRZ %dx%d @%dx first mismatch (%zu,%zu): CPU=%06x GPU=%06x\n",
					width, height, factor, i % outWidth, i / outWidth, expected[i], actual[i] & 0xffffff);
				++differences;
			}
		}
		CHECK_EQ(size_t(0), differences);
		// Re-presenting an unchanged image must reuse the filtered GPU image.
		CHECK(texture == SpriteGpu::UpscaledTexture(surface.GetBackendSurface(), testSpriteRenderer, factor));
		CHECK_EQ(before.upscaledFrames + 1, SpriteGpu::GetCounters().upscaledFrames);
	}
}
}

TEST(SpriteGpu, XbrzShadersMatchVendoredCpuAtEveryScaleAndImageBoundary)
{
	Fixture f;
	if (!SpriteGpuEffects::Active()) return;
	CHECK(SpriteGpuEffects::XbrzAvailable());
	if (!SpriteGpuEffects::XbrzAvailable()) return;
	CheckXbrzImage({0}, 1, 1);
	CheckXbrzImage({0xffff}, 1, 1);
	std::vector<WORD> edge{0, 0xffff, 0x07e0, 0x1234, 0x1234, 0, 0xf800, 0x001f, 0xffff, 0x001f, 0x001f};
	CheckXbrzImage(edge, int(edge.size()), 1);
	CheckXbrzImage(edge, 1, int(edge.size()));
	// Exhaust every binary 3x3 neighborhood; margins separate adjacent patterns.
	constexpr int width = 160, height = 80;
	std::vector<WORD> patterns(width * height);
	for (int pattern = 0; pattern < 512; ++pattern) {
		for (int bit = 0; bit < 9; ++bit) {
			const int x = (pattern % 32) * 5 + 1 + bit % 3;
			const int y = (pattern / 32) * 5 + 1 + bit / 3;
			patterns[y * width + x] = (pattern & (1 << bit)) ? 0xffff : 0;
		}
	}
	CheckXbrzImage(patterns, width, height);
	uint32_t random = 0x94871b32;
	const WORD palette[]{0, 0xffff, 0xf800, 0x07e0, 0x001f, 0x8410, 0x09bf, 0x23f7};
	for (int variant = 0; variant < 4; ++variant) {
		for (auto& pixel : patterns) {
			random = random * 1664525u + 1013904223u;
			pixel = variant < 2 ? palette[(random >> 16) % 8] : WORD(random >> 16);
		}
		CheckXbrzImage(patterns, width, height);
	}
	// Native-size input exercises millions of classifications and scaled pixels.
	patterns.resize(800 * 600);
	for (auto& pixel : patterns) {
		random = random * 1664525u + 1013904223u;
		pixel = palette[(random >> 16) % 8];
	}
	CheckXbrzImage(patterns, 800, 600);
}

TEST(SpriteGpu, XbrzCacheTracksGpuWritesCpuWritesToggleResetAndDetach)
{
	Fixture f;
	if (!SpriteGpuEffects::Active()) return;
	CHECK(SpriteGpuEffects::XbrzAvailable());
	f.Draw();
	auto filter = [&] { return SpriteGpu::UpscaledTexture(f.surface.GetBackendSurface(), testSpriteRenderer, 3); };
	CHECK(filter() != nullptr);
	const auto first = SpriteGpu::GetCounters();
	f.Draw(2, 4);
	CHECK(filter() != nullptr);
	CHECK_EQ(first.upscaledFrames + 1, SpriteGpu::GetCounters().upscaledFrames);
	CHECK_EQ(0u, SpriteGpu::GetCounters().readbacks);
	DWORD pitch = 0;
	auto* pixel = static_cast<WORD*>(f.surface.Lock(nullptr, &pitch));
	CHECK(pixel != nullptr);
	pixel[0] = 0xf800;
	CHECK(filter() != nullptr);
	pixel[0] = 0x001f; // An outstanding borrowed pointer remains writable.
	CHECK(filter() != nullptr);
	CHECK_EQ(first.upscaledFrames + 3, SpriteGpu::GetCounters().upscaledFrames);
	f.surface.Unlock();
	spritectl_set_xbrz_enabled(0);
	CHECK_EQ(0u, SpriteGpu::GetCounters().upscaleBytes);
	CHECK(filter() != nullptr);
	const auto beforeReset = SpriteGpu::GetCounters().upscaledFrames;
	CHECK(SpriteGpu::CheckpointOffscreen(nullptr));
	spritectl_render_device_reset();
	CHECK_EQ(0u, SpriteGpu::GetCounters().upscaleBytes);
	CHECK(filter() != nullptr);
	CHECK_EQ(beforeReset + 1, SpriteGpu::GetCounters().upscaledFrames);
	SpriteGpu::Detach();
	CHECK_EQ(0u, SpriteGpu::GetCounters().upscaleBytes);
}

TEST(SpriteGpu, XbrzPresentationMatchesCpuWithFractionalScalingAndLetterbox)
{
	Fixture f;
	if (!SpriteGpuEffects::Active()) return;
	CSpriteSurface surface;
	CHECK(surface.Init(17, 11));
	surface.FillSurface(0x8410);
	for (int y = 0; y < 11; ++y)
		CHECK_EQ(0, spritectl_blt_sprite(surface.GetBackendSurface(), y, y, f.sprite, 0, 255));
	// Save this unrelated offscreen fixture once; presentation need only filter
	// the frame surface, with no per-frame CPU pixels involved.
	CHECK(SpriteGpu::CheckpointOffscreen(surface.GetBackendSurface()));
	spritectl_set_xbrz_enabled(1);
	spritectl_finish_rendering();
	SDL_SetRenderDrawColor(testSpriteRenderer, 0, 0, 0, 255);
	SDL_RenderClear(testSpriteRenderer);
	const auto before = SpriteGpu::GetCounters();
	CHECK_EQ(0, spritectl_present_surface(surface.GetBackendSurface(), testSpriteRenderer));
	CHECK_EQ(before.upscaledFrames + 1, SpriteGpu::GetCounters().upscaledFrames);
	CHECK_EQ(before.readbacks, SpriteGpu::GetCounters().readbacks);
	CHECK(SDL_GetRenderTarget(testSpriteRenderer) == nullptr);
	std::vector<uint32_t> actual(64 * 64), expected(actual.size());
	CHECK_EQ(0, SDL_RenderReadPixels(testSpriteRenderer, nullptr, SDL_PIXELFORMAT_ARGB8888, actual.data(), 64 * 4));
	CHECK_EQ(0, spritectl_present_surface(surface.GetBackendSurface(), testSpriteRenderer));
	CHECK_EQ(before.upscaledFrames + 1, SpriteGpu::GetCounters().upscaledFrames);
	CHECK_EQ(before.readbacks, SpriteGpu::GetCounters().readbacks);
	CHECK(SpriteGpu::CpuAccess(surface.GetBackendSurface(), false));
	FrameUpscaler reference;
	SDL_RenderClear(testSpriteRenderer);
	// Presentation computes the aspect-preserving rectangle in 16.16 fixed point.
	const int scale = (64 << 16) / 17;
	const int w = (17 * scale) >> 16, h = (11 * scale) >> 16;
	const SDL_Rect destination{(64 - w) / 2, (64 - h) / 2, w, h};
	CHECK(reference.Draw(surface.GetBackendSurface()->surface, testSpriteRenderer, destination));
	CHECK_EQ(0, SDL_RenderReadPixels(testSpriteRenderer, nullptr, SDL_PIXELFORMAT_ARGB8888, expected.data(), 64 * 4));
	for (size_t i = 0; i < actual.size(); ++i) CHECK_EQ(expected[i], actual[i]);
	spritectl_set_xbrz_enabled(0);
	CHECK_EQ(0u, SpriteGpu::GetCounters().upscaleBytes);
}

TEST(SpriteGpu, UiSourceClippingRestoresViewportAndKeepsGpuComposition)
{
	Fixture f;
	CSprite565 sprite;
	WORD colors[8]{0, 0xffff, 0x8410, 0xf800, 0x001f, 0x07e0, 0x5a69, 0};
	sprite.SetPixelNoColorkey(colors, 8, 4, 2);
	const auto effect = CSpriteSurface::s_pMemcpyEffectFunction;
	const int savedValue = CSpriteSurface::s_Value1;
	CSpriteSurface::s_pMemcpyEffectFunction = CSpriteSurface::memcpyEffectGrayScale;
	for (int mode = 0; mode < 3; ++mode) {
		for (POINT point : {POINT{0, 2}, POINT{-1, -1}, POINT{INT_MIN, 0}, POINT{INT_MAX, 0}}) {
			f.surface.FillSurface(0x07e0);
			RECT viewport{1, 0, 6, 7};
			f.surface.SetClip(&viewport);
			RECT source{1, 0, 3, 2};
			const auto before = SpriteGpu::GetCounters();
			if (mode == 0) f.surface.BltSpriteClip(&point, &sprite, &source);
			else if (mode == 1) f.surface.BltSpriteColorClip(&point, &sprite, &source, 0);
			else f.surface.BltSpriteEffectClip(&point, &sprite, &source);
			if (mode == 0 || SpriteGpuEffects::Active()) CHECK_EQ(before.readbacks, SpriteGpu::GetCounters().readbacks);
			const SDL_Rect actualClip = f.surface.GetBackendSurface()->surface->clip_rect;
			CHECK_EQ(1, actualClip.x); CHECK_EQ(0, actualClip.y);
			CHECK_EQ(5, actualClip.w); CHECK_EQ(7, actualClip.h);
			WORD transformed[8];
			CSpriteSurface::s_Value1 = 0;
			if (mode == 0) std::memcpy(transformed, colors, sizeof(colors));
			else if (mode == 1) CSpriteSurface::memcpyColor(transformed, colors, 8);
			else CSpriteSurface::memcpyEffectGrayScale(transformed, colors, 8);
			for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) {
				const int64_t sx = int64_t(x) - point.x, sy = int64_t(y) - point.y;
				const bool drawn = x >= 1 && x < 6 && y < 7 && sx >= 1 && sx < 3 && sy >= 0 && sy < 2;
				CHECK_EQ(drawn ? transformed[sy * 4 + sx] : 0x07e0, f.Pixel(x, y));
			}
		}
	}
	CSpriteSurface::s_pMemcpyEffectFunction = effect;
	CSpriteSurface::s_Value1 = savedValue;
}

TEST(SpriteGpu, WorldLightingGridMatchesCpuIncludingUnevenCellsAndSurfaceEdges)
{
	Fixture f;
	for (int variant = 0; variant < 2; ++variant) {
		const int width = variant ? 800 : 17, height = variant ? 600 : 11;
		const int columns = variant ? 64 : 5, rows = variant ? 64 : 4;
		CSpriteSurface surface;
		CHECK(surface.Init(width, height));
		CFilter filter;
		filter.Init(WORD(columns), WORD(rows));
		std::vector<int> widths(columns), heights(rows);
		const int smallWidths[]{2, 3, 7, 1, 5}, smallHeights[]{1, 4, 2, 6};
		const BYTE light[]{0, 1, 16, 31, 32, 33, 63, 255};
		for (int x = 0; x < columns; ++x) widths[x] = variant ? 12 + x % 2 : smallWidths[x];
		for (int y = 0; y < rows; ++y) {
			heights[y] = variant ? (y % 8 == 0 || y % 8 == 3 || y % 8 == 6 ? 10 : 9) : smallHeights[y];
			for (int x = 0; x < columns; ++x) filter.SetFilter(WORD(x), WORD(y), light[(x + y) % 8]);
		}
		std::vector<WORD> expected(size_t(width) * height);
		DWORD pitch = 0;
		auto* bytes = static_cast<BYTE*>(surface.Lock(nullptr, &pitch));
		CHECK(bytes != nullptr);
		for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
			expected[y * width + x] = WORD((y * width + x) * 179);
			reinterpret_cast<WORD*>(bytes + y * pitch)[x] = expected[y * width + x];
		}
		surface.Unlock();
		int top = 0;
		for (int y = 0; y < rows && top < height; ++y) {
			int left = 0;
			for (int x = 0; x < columns && left < width; ++x) {
				const int span = (std::min)(widths[x], width - left);
				for (int row = top; row < (std::min)(top + heights[y], height); ++row)
					CSpriteSurface::Gamma4Pixel565(expected.data() + row * width + left, span, filter.GetFilter(WORD(y))[x]);
				left += widths[x];
			}
			top += heights[y];
		}
		RECT clip{2, 2, 4, 4};
		surface.SetClip(&clip); // Whole-frame lighting retains the original clip-independent behavior.
		const auto before = SpriteGpu::GetCounters();
		surface.ApplyLightGrid(filter, widths.data(), heights.data());
		if (SpriteGpuEffects::Active()) {
			CHECK_EQ(before.readbacks, SpriteGpu::GetCounters().readbacks);
			CHECK_EQ(before.shaderDraws + 1, SpriteGpu::GetCounters().shaderDraws);
		}
		CHECK(SpriteGpu::CpuAccess(surface.GetBackendSurface(), false));
		for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
			CHECK_EQ(expected[y * width + x], reinterpret_cast<WORD*>(bytes + y * pitch)[x]);
		CHECK_EQ(2, surface.GetBackendSurface()->surface->clip_rect.x);
		CHECK_EQ(2, surface.GetBackendSurface()->surface->clip_rect.w);
	}
}

TEST(SpriteGpu, MapRegionTintMatchesChannelMasksAndClipsToSurface)
{
	Fixture f;
	CSpriteSurface surface;
	CHECK(surface.Init(256, 256));
	const WORD masks[]{0xf800, 0x07e0, 0x001f};
	for (BYTE channel = 0; channel < 3; ++channel) {
		DWORD pitch = 0;
		auto* bytes = static_cast<BYTE*>(surface.Lock(nullptr, &pitch));
		CHECK(bytes != nullptr);
		for (int y = 0; y < 256; ++y) for (int x = 0; x < 256; ++x)
			reinterpret_cast<WORD*>(bytes + y * pitch)[x] = WORD(y * 256 + x);
		surface.Unlock();
		RECT area{-100, -100, 256, INT_MAX};
		const auto before = SpriteGpu::GetCounters();
		surface.ColorBox(&area, channel);
		if (SpriteGpuEffects::Active()) CHECK_EQ(before.readbacks, SpriteGpu::GetCounters().readbacks);
		CHECK(SpriteGpu::CpuAccess(surface.GetBackendSurface(), false));
		for (int y = 0; y < 256; ++y) for (int x = 0; x < 256; ++x)
			CHECK_EQ(WORD((y * 256 + x) & masks[channel]), reinterpret_cast<WORD*>(bytes + y * pitch)[x]);
	}
	f.surface.FillSurface(0xffff);
	RECT area{2, 3, 5, 6};
	f.surface.ColorBox(&area, 1);
	for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x)
		CHECK_EQ(x >= 2 && x < 5 && y >= 3 && y < 6 ? 0x07e0 : 0xffff, f.Pixel(x, y));
}

TEST(SpriteGpu, TransientCompositionSkipsReadbackButRetainsPersistentSurfaces)
{
	Fixture f;
	CSpriteSurface frame, terrain;
	CHECK(frame.Init(8, 8));
	CHECK(terrain.Init(8, 8));
	frame.SetTransient(true);
	terrain.FillSurface(0x1234);
	POINT origin{0, 0};
	RECT full{0, 0, 8, 8};
	frame.BltNoColorkey(&origin, &terrain, &full);
	f.surface.BltNoColorkey(&origin, &frame, &full);
	auto before = SpriteGpu::GetCounters();
	CHECK(SpriteGpu::CheckpointOffscreen(f.surface.GetBackendSurface()));
	// Only the terrain needs a CPU checkpoint, despite copying both frame
	// buffers on the GPU. Presenting another frame needs no checkpoint.
	CHECK_EQ(before.readbacks + 1, SpriteGpu::GetCounters().readbacks);
	frame.FillSurface(0xabcd);
	f.surface.BltNoColorkey(&origin, &frame, &full);
	before = SpriteGpu::GetCounters();
	CHECK(SpriteGpu::CheckpointOffscreen(f.surface.GetBackendSurface()));
	CHECK_EQ(before.readbacks, SpriteGpu::GetCounters().readbacks);
	CHECK_EQ(0xabcd, f.Pixel(0, 0));
	SpriteGpu::Reset();
	// The persistent map survives reset; the transient frame is redrawn.
	frame.BltNoColorkey(&origin, &terrain, &full);
	f.surface.BltNoColorkey(&origin, &frame, &full);
	CHECK_EQ(0x1234, f.Pixel(7, 7));
	frame.SetTransient(false);
	frame.FillSurface(0x5678);
	before = SpriteGpu::GetCounters();
	CHECK(SpriteGpu::CheckpointOffscreen(f.surface.GetBackendSurface()));
	CHECK_EQ(before.readbacks + 1, SpriteGpu::GetCounters().readbacks);
}

TEST(SpriteGpu, SpriteBatchTracksTargetClipAndInterleavedPixelWrites)
{
	Fixture f;
	CSpriteSurface other;
	CHECK(other.Init(8, 8));
	other.FillSurface(0);
	auto backend = f.surface.GetBackendSurface();
	SDL_Rect clip{2, 1, 1, 1};
	SDL_SetClipRect(backend->surface, &clip);
	f.Draw();
	f.Draw(); // Same target/clip can reuse the batch state.
	other.FillSurface(0x001f); // Switch target.
	clip = {3, 2, 1, 1};
	SDL_SetClipRect(backend->surface, &clip);
	f.Draw();
	CHECK_EQ(0, f.Pixel(2, 1));
	CHECK_EQ(0xffff, f.Pixel(3, 2));
	CHECK_EQ(0x07e0, f.Pixel(3, 1));
	CHECK_EQ(0x07e0, f.Pixel(2, 2));
	DWORD pitch = 0;
	auto* pixels = static_cast<BYTE*>(f.surface.Lock(nullptr, &pitch));
	CHECK(pixels != nullptr);
	reinterpret_cast<WORD*>(pixels + 2 * pitch)[3] = 0xf800;
	f.surface.Unlock();
	SDL_SetClipRect(backend->surface, nullptr);
	f.Draw(1, 4);
	CHECK_EQ(0xf800, f.Pixel(3, 2));
	CHECK_EQ(0, f.Pixel(2, 4));
	CHECK_EQ(0xffff, f.Pixel(3, 4));
}

TEST(SpriteGpu, CompleteMixedFramesMatchCpuWithoutCompositionReadbacks)
{
	Fixture f;
	if (!SpriteGpuEffects::XbrzAvailable()) return;
	SpriteGpu::Detach();
	MixedScene scene;
	CHECK(scene.Init());
	std::vector<WORD> expected(800 * 600);
	std::vector<uint32_t> rgb(expected.size()), scaled(expected.size() * 4), actual(scaled.size());
	for (int number : {0, 1, 11}) {
		const int factor = number == 0 ? 2 : number == 1 ? 3 : 4;
		const int outputWidth = 800 * factor;
		scaled.resize(expected.size() * factor * factor);
		actual.resize(scaled.size());
		SpriteGpu::Detach();
		scene.Draw(number);
		auto* cpu = scene.frame.GetBackendSurface()->surface;
		for (int y = 0; y < 600; ++y)
			std::memcpy(expected.data() + y * 800, static_cast<BYTE*>(cpu->pixels) + y * cpu->pitch, 1600);
		CHECK_EQ(0, SDL_ConvertPixels(800, 600, SDL_PIXELFORMAT_RGB565, expected.data(), 1600,
			SDL_PIXELFORMAT_RGB888, rgb.data(), 3200));
		xbrz::scale(factor, rgb.data(), scaled.data(), 800, 600, xbrz::ColorFormat::RGB);
		CHECK(SpriteGpu::Attach(testSpriteRenderer));
		scene.Draw(number);
		const auto before = SpriteGpu::GetCounters();
		CHECK_EQ(0u, before.readbacks);
		auto* texture = SpriteGpu::UpscaledTexture(scene.frame.GetBackendSurface(), testSpriteRenderer, factor);
		CHECK(texture != nullptr);
		CHECK_EQ(0u, SpriteGpu::GetCounters().readbacks);
		CHECK_EQ(0, SDL_SetRenderTarget(testSpriteRenderer, texture));
		CHECK_EQ(0, SDL_RenderReadPixels(testSpriteRenderer, nullptr, SDL_PIXELFORMAT_ARGB8888, actual.data(), outputWidth * 4));
		CHECK_EQ(0, SDL_SetRenderTarget(testSpriteRenderer, nullptr));
		size_t mismatches = 0;
		for (size_t i = 0; i < actual.size(); ++i) if ((actual[i] & 0xffffff) != (scaled[i] & 0xffffff)) {
			if (mismatches < 4) std::fprintf(stderr, "Mixed xBRZ %dx (%zu,%zu): CPU=%06x GPU=%06x\n", factor, i % outputWidth, i / outputWidth, scaled[i] & 0xffffff, actual[i] & 0xffffff);
			++mismatches;
		}
		if (mismatches) {
			auto* composed = SpriteGpu::PresentationTexture(scene.frame.GetBackendSurface(), testSpriteRenderer);
			std::vector<WORD> pixels(expected.size());
			SDL_SetRenderTarget(testSpriteRenderer, composed);
			SDL_RenderReadPixels(testSpriteRenderer, nullptr, SDL_PIXELFORMAT_RGB565, pixels.data(), 1600);
			SDL_SetRenderTarget(testSpriteRenderer, nullptr);
			size_t differences = 0;
			for (size_t i = 0; i < pixels.size(); ++i) if (pixels[i] != expected[i]) {
				if (differences < 5) std::fprintf(stderr, "Mixed frame (%zu,%zu): CPU=%04x GPU=%04x\n", i % 800, i / 800, expected[i], pixels[i]);
				++differences;
			}
			std::fprintf(stderr, "Mixed frame differences=%zu; scaled differences=%zu\n", differences, mismatches);
		}
		CHECK_EQ(size_t(0), mismatches);
		CHECK_EQ(0, spritectl_present_surface(scene.frame.GetBackendSurface(), testSpriteRenderer));
		CHECK_EQ(0u, SpriteGpu::GetCounters().readbacks);
		scene.Draw(number + 1);
		CHECK_EQ(0, spritectl_present_surface(scene.frame.GetBackendSurface(), testSpriteRenderer));
		CHECK_EQ(0u, SpriteGpu::GetCounters().readbacks);
		CHECK_EQ(before.spriteUploads, SpriteGpu::GetCounters().spriteUploads);
		CHECK_EQ(before.surfaceUploads, SpriteGpu::GetCounters().surfaceUploads);
	}
}
