#include "test_framework.h"
#include "CShadowSprite.h"
#include "CIndexSprite565.h"
#include "CSprite565.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackendSDL.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <vector>

namespace {
constexpr const char* path = "shadow_sprite_loading_test.bin";
const std::vector<WORD> valid{4, 1, 5, 2, 1, 1, 1, 1};
struct Fixture { ~Fixture() { std::remove(path); } };
void Write(const std::vector<WORD>& words, size_t limit = SIZE_MAX)
{
	std::vector<char> bytes;
	for (WORD word : words) { bytes.push_back(char(word)); bytes.push_back(char(word >> 8)); }
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(bytes.data(), static_cast<std::streamsize>((std::min)(limit, bytes.size())));
}
void CheckEmpty(const CShadowSprite& sprite)
{
	CHECK(!sprite.IsInit());
	CHECK_EQ(0, sprite.GetWidth());
	CHECK_EQ(0, sprite.GetHeight());
}
}

TEST(ShadowSpriteLoading, IncompleteHeadersAreNotEmptySprites)
{
	Fixture fixture;
	for (size_t size = 0; size < 4; ++size) {
		Write({0, 0}, size);
		std::ifstream in(path, std::ios::binary);
		CShadowSprite sprite;
		CHECK(!sprite.LoadFromFile(in));
		CHECK(!in);
		CheckEmpty(sprite);
	}
}

TEST(ShadowSpriteLoading, RejectsIncompleteAndOutOfBoundsRows)
{
	Fixture fixture;
	for (const auto& words : {
		std::vector<WORD>{4, 1, 0},
		std::vector<WORD>{4, 1, 2, 1, 0},
		std::vector<WORD>{4, 1, 3, 1, 5, 0},
		std::vector<WORD>{4, 1, 3, 1, 1, 4},
		std::vector<WORD>{4, 1, 5, 2, 1, 1, 1, 2},
		std::vector<WORD>{65535, 1, 3, 1, 65535, 1},
		std::vector<WORD>{4, 1, 5, 2, 1, 1}}) {
		Write(words);
		std::ifstream in(path, std::ios::binary);
		CShadowSprite sprite;
		CHECK(!sprite.LoadFromFile(in));
		CheckEmpty(sprite);
	}
}

TEST(ShadowSpriteLoading, ReleaseClearsCompleteEmptySpriteState)
{
	Fixture fixture;
	for (const auto& words : {std::vector<WORD>{0, 3}, std::vector<WORD>{3, 0}}) {
		Write(words);
		std::ifstream in(path, std::ios::binary);
		CShadowSprite sprite;
		CHECK(sprite.LoadFromFile(in));
		CHECK(sprite.IsInit());
		sprite.Release();
		CheckEmpty(sprite);
		sprite.Release();
		CheckEmpty(sprite);
	}
}

TEST(ShadowSpriteLoading, EveryTruncatedPrefixClearsStateAndAllowsReload)
{
	Fixture fixture;
	const std::vector<WORD> twoRows{4, 2, 5, 2, 1, 1, 1, 1, 3, 1, 0, 4};
	CShadowSprite sprite;
	for (size_t size = 0; size < twoRows.size() * 2; ++size) {
		Write(twoRows, size);
		{
			std::ifstream in(path, std::ios::binary);
			CHECK(!sprite.LoadFromFile(in));
			CHECK(!in);
		}
		CheckEmpty(sprite);
		Write(valid);
		std::ifstream in(path, std::ios::binary);
		CHECK(sprite.LoadFromFile(in));
		CHECK_EQ(4, sprite.GetWidth());
	}
}

TEST(ShadowSpriteLoading, CompleteRejectedRecordsPreserveTheFollowingHeader)
{
	Fixture fixture;
	for (const auto& invalid : {
		std::vector<WORD>{4, 2, 3, 1, 1, 4, 1, 0},
		std::vector<WORD>{4, 2, 0, 1, 0}}) {
		auto words = invalid;
		const auto next = static_cast<std::streamoff>(words.size() * 2);
		words.insert(words.end(), valid.begin(), valid.end());
		Write(words);
		std::ifstream in(path, std::ios::binary);
		CShadowSprite sprite;
		CHECK(!sprite.LoadFromFile(in));
		CheckEmpty(sprite);
		CHECK_EQ(next, static_cast<std::streamoff>(in.tellg()));
		CHECK(sprite.LoadFromFile(in));
		CHECK_EQ(4, sprite.GetWidth());
		CHECK_EQ(1, sprite.GetHeight());
		std::ifstream throwing;
		throwing.exceptions(std::ios::badbit | std::ios::failbit);
		CHECK(!sprite.LoadFromFile(throwing));
		CheckEmpty(sprite);
	}
}

TEST(ShadowSpriteLoading, ValidRowsSupportDrawingPaddingAndMaximumLengths)
{
	Fixture fixture;
	CShadowSprite sprite;
	auto padded = valid;
	padded[2] += 2;
	padded.push_back(0x1234);
	padded.push_back(0xffff);
	Write(padded);
	{
		std::ifstream in(path, std::ios::binary);
		const bool loaded = sprite.LoadFromFile(in);
		CHECK(loaded);
		if (loaded) {
			CHECK_EQ(0xffff, sprite.GetPixelLine(0)[6]);
			CHECK(!sprite.IsColorPixel(0, 0));
			CHECK(sprite.IsColorPixel(1, 0));
			CHECK(!sprite.IsColorPixel(2, 0));
			CHECK(sprite.IsColorPixel(3, 0));
			std::vector<WORD> pixels(6, 0x1234);
			sprite.Blt(pixels.data() + 1, 4 * sizeof(WORD));
			CHECK(pixels == std::vector<WORD>({0x1234, 0x1234, 0, 0x1234, 0, 0x1234}));
		}
	}
	std::vector<WORD> maximum{65534, 1, 65535, 32767};
	for (size_t run = 0; run < 32767; ++run) { maximum.push_back(1); maximum.push_back(1); }
	Write(maximum);
	{
		std::ifstream in(path, std::ios::binary);
		const bool loaded = sprite.LoadFromFile(in);
		CHECK(loaded);
		if (loaded) CHECK_EQ(1, sprite.GetPixelLine(0)[65534]);
		CHECK_EQ(static_cast<std::streamoff>(maximum.size() * 2), static_cast<std::streamoff>(in.tellg()));
	}
	std::vector<WORD> tall{1, 65535};
	for (size_t y = 0; y < 65535; ++y) { tall.push_back(1); tall.push_back(0); }
	Write(tall);
	std::ifstream in(path, std::ios::binary);
	const bool loaded = sprite.LoadFromFile(in);
	CHECK(loaded);
	if (loaded) CHECK_EQ(0, sprite.GetPixelLine(65534)[0]);
	CHECK_EQ(65535, sprite.GetHeight());
}

TEST(ShadowSpriteOwnership, SelfAssignmentPreservesTheLoadedSprite)
{
	Fixture fixture;
	Write(valid);
	std::ifstream in(path, std::ios::binary);
	CShadowSprite sprite;
	CHECK(sprite.LoadFromFile(in));
	CShadowSprite& alias = sprite;
	sprite = alias;
	CHECK(sprite.IsInit());
	CHECK_EQ(4, sprite.GetWidth());
	CHECK_EQ(1, sprite.GetHeight());
	CHECK(sprite.IsColorPixel(3, 0));
}

TEST(ShadowSpriteOwnership, AssignmentRetainsAnInitializedEmptyRecord)
{
	Fixture fixture;
	Write({0, 3});
	std::ifstream in(path, std::ios::binary);
	CShadowSprite source, copy;
	CHECK(source.LoadFromFile(in));
	copy = source;
	CHECK(copy.IsInit());
	CHECK_EQ(0, copy.GetWidth());
	CHECK_EQ(3, copy.GetHeight());
}

TEST(ShadowSpriteOwnership, CopiesOwnRowsPaddingAndBackendCachesIndependently)
{
	Fixture fixture;
	CHECK_EQ(0, spritectl_init());
	auto words = valid;
	words[2] += 2;
	words.push_back(0x1234);
	words.push_back(0xffff);
	Write(words);
	std::ifstream in(path, std::ios::binary);
	CShadowSprite source, assigned;
	CHECK(source.LoadFromFile(in));
	CSpriteSurface surface;
	CHECK(surface.Init(4, 1));
	POINT origin{0, 0};
	surface.BltShadowSprite(&origin, &source);
	CHECK(source.GetBackendSprite() != nullptr);
	CShadowSprite constructed(source);
	assigned = source;
	for (auto* copy : {&constructed, &assigned}) {
		CHECK(copy->GetBackendSprite() == nullptr);
		CHECK(copy->GetPixelLine(0) != source.GetPixelLine(0));
		const auto line = copy->GetPixelLineSpan(0);
		CHECK_EQ(7, line.size());
		if (line.size() == 7) CHECK_EQ(0xffff, line[6]);
		surface.BltShadowSprite(&origin, copy);
		CHECK(copy->GetBackendSprite() != nullptr);
		CHECK(copy->GetBackendSprite() != source.GetBackendSprite());
	}
	source.Release();
	for (auto* copy : {&constructed, &assigned}) {
		CHECK(copy->IsColorPixel(3, 0));
		copy->Release();
		CHECK(copy->GetPixelLineSpan(0).empty());
		CHECK(copy->GetBackendSprite() == nullptr);
	}
}

TEST(ShadowSpriteOwnership, CorruptedSourceCountsAreRejectedBeforeCopying)
{
	Fixture fixture;
	for (WORD bad : {WORD(3), WORD(65535)}) {
		Write(valid);
		std::ifstream in(path, std::ios::binary);
		CShadowSprite source, assigned;
		CHECK(source.LoadFromFile(in));
		source.GetPixelLine(0)[0] = bad;
		assigned = source;
		CheckEmpty(assigned);
		CShadowSprite constructed(source);
		CheckEmpty(constructed);
	}
	Write(valid);
	std::ifstream in(path, std::ios::binary);
	CShadowSprite source, assigned;
	CHECK(source.LoadFromFile(in));
	source.GetPixelLine(0)[2] = 4;
	assigned = source;
	CheckEmpty(assigned);
}

TEST(ShadowSpriteOwnership, AllGeneratedFormsRetainCopyableRowBounds)
{
	Fixture fixture;
	CShadowSprite::SetColorkey(0);
	WORD pixels[]{0, 0xffff, 0, 0xffff};
	CShadowSprite fromPixels, fromSprite, fromIndexed;
	fromPixels.SetPixel(pixels, sizeof(pixels), 4, 1);
	CSprite565 sprite;
	Write({4, 1, 7, 2, 1, 1, 0xffff, 1, 1, 0xffff});
	{
		std::ifstream in(path, std::ios::binary);
		CHECK(sprite.LoadFromFile(in));
	}
	fromSprite.SetPixel(sprite);
	CIndexSprite565 indexed;
	Write({4, 1, 9, 2, 1, 1, 0x010a, 0, 1, 0, 1, 0xffff});
	{
		std::ifstream in(path, std::ios::binary);
		CHECK(indexed.LoadFromFile(in));
	}
	fromIndexed.SetPixel(indexed);
	for (auto* source : {&fromPixels, &fromSprite, &fromIndexed}) {
		CHECK_EQ(5, source->GetPixelLineSpan(0).size());
		CShadowSprite copied(*source);
		source->Release();
		CHECK(copied.IsInit());
		CHECK(!copied.IsColorPixel(0, 0));
		CHECK(copied.IsColorPixel(1, 0));
		CHECK(!copied.IsColorPixel(2, 0));
		CHECK(copied.IsColorPixel(3, 0));
	}
}

TEST(ShadowSpriteAdapter, RejectsMutatedRowsBeforeCreatingOrRefreshingABackend)
{
	Fixture fixture;
	CHECK_EQ(0, spritectl_init());
	Write(valid);
	std::ifstream in(path, std::ios::binary);
	CShadowSprite sprite;
	CHECK(sprite.LoadFromFile(in));
	CSpriteSurface surface;
	CHECK(surface.Init(4, 1));
	POINT origin{0, 0};
	sprite.GetPixelLine(0)[2] = 5;
	surface.BltShadowSprite(&origin, &sprite);
	CHECK(sprite.GetBackendSprite() == nullptr);
	sprite.GetPixelLine(0)[2] = 1;
	surface.BltShadowSprite(&origin, &sprite);
	CHECK(sprite.GetBackendSprite() != nullptr);
	sprite.GetPixelLine(0)[0] = 65535;
	sprite.SetBackendDirty(true);
	surface.BltShadowSprite(&origin, &sprite);
	CHECK(sprite.GetBackendSprite() == nullptr);
}

TEST(ShadowSpriteAdapter, RejectsEmptyAndUnrepresentableRasterGeometry)
{
	Fixture fixture;
	CHECK_EQ(0, spritectl_init());
	CSpriteSurface surface;
	CHECK(surface.Init(4, 1));
	POINT origin{0, 0};
	for (const auto& words : {std::vector<WORD>{0, 1}, std::vector<WORD>{1, 0}, std::vector<WORD>{0, 0}}) {
		Write(words);
		std::ifstream in(path, std::ios::binary);
		CShadowSprite sprite;
		CHECK(sprite.LoadFromFile(in));
		surface.BltShadowSprite(&origin, &sprite);
		CHECK(sprite.GetBackendSprite() == nullptr);
	}
	std::vector<WORD> huge{65535, 65535};
	for (size_t y = 0; y < 65535; ++y) { huge.push_back(1); huge.push_back(0); }
	Write(huge);
	std::ifstream in(path, std::ios::binary);
	CShadowSprite sprite;
	CHECK(sprite.LoadFromFile(in));
	surface.BltShadowSprite(&origin, &sprite);
	CHECK(sprite.GetBackendSprite() == nullptr);
}

TEST(ShadowSpriteAdapter, WideValidRowsKeepTheirGeometryAndPixels)
{
	Fixture fixture;
	CHECK_EQ(0, spritectl_init());
	Write({40000, 2, 3, 1, 39999, 1, 3, 1, 39999, 1});
	std::ifstream in(path, std::ios::binary);
	CShadowSprite sprite;
	CHECK(sprite.LoadFromFile(in));
	CSpriteSurface surface;
	CHECK(surface.Init(4, 2));
	POINT origin{0, 0};
	surface.BltShadowSprite(&origin, &sprite);
	const auto backend = sprite.GetBackendSprite();
	CHECK(backend != nullptr);
	if (backend) {
		CHECK_EQ(40000, backend->width);
		CHECK_EQ(2, backend->height);
		// Opaque black where the sprite casts its shadow, clear elsewhere.
		CHECK_EQ(SPRITECTL_FORMAT_RGBA32, backend->format);
		CHECK_EQ(320000, backend->data_size);
		const auto* pixels = reinterpret_cast<const uint32_t*>(backend->pixels);
		CHECK_EQ(0u, pixels[39998]);
		CHECK_EQ(0xFF000000u, pixels[39999]);
		CHECK_EQ(0xFF000000u, pixels[79999]);
	}
}

namespace {

// A white surface with a shadow drawn on it: a row of four, its middle two
// pixels shadowed, at (2, 1).
struct ShadowOnWhite
{
	Fixture fixture;
	CSpriteSurface surface;
	CShadowSprite sprite;
	ShadowOnWhite()
	{
		CHECK_EQ(0, spritectl_init());
		CHECK(surface.Init(8, 3));
		surface.FillSurface(0xFFFF);
		Write({4, 1, 3, 1, 1, 2});
		std::ifstream in(path, std::ios::binary);
		CHECK(sprite.LoadFromFile(in));
	}
	WORD At(int x, int y)
	{
		DWORD pitch = 0;
		const auto* base = static_cast<const unsigned char*>(surface.Lock(nullptr, &pitch));
		const WORD pixel = base ? reinterpret_cast<const WORD*>(base + y * pitch)[x] : WORD(0xBAD);
		surface.Unlock();
		return pixel;
	}
	// The pixels the shadow does not cover are still white.
	bool RestWhite()
	{
		for (int y = 0; y < 3; ++y)
			for (int x = 0; x < 8; ++x)
				if (!(y == 1 && (x == 3 || x == 4)) && At(x, y) != 0xFFFF) return false;
		return true;
	}
};

// A 5:6:5 channel is within one of the expected.
bool Near(int value, int expected) { return value >= expected - 1 && value <= expected + 1; }

} // namespace

TEST(ShadowSpriteAdapter, AShadowIsDrawnBlackWhereTheSpriteCastsIt)
{
	// As the DirectDraw blit did: colour 0 over the shadowed pixels. The
	// SDL raster was black too, but black is the colour key, so it drew
	// nothing.
	ShadowOnWhite scene;
	POINT point{2, 1};
	scene.surface.BltShadowSprite(&point, &scene.sprite);
	CHECK_EQ(0, scene.At(3, 1));
	CHECK_EQ(0, scene.At(4, 1));
	CHECK(scene.RestWhite());
}

TEST(ShadowSpriteAdapter, ADarkenedShadowDarkensWhatLiesUnderIt)
{
	// The DirectDraw blit shifted each channel under the shadow right by
	// DarkBits; black blended at 1 - 2^-DarkBits comes within one of it.
	for (int bits : {1, 2})
	{
		ShadowOnWhite scene;
		POINT point{2, 1};
		scene.surface.BltShadowSpriteDarkness(&point, &scene.sprite, BYTE(bits));
		for (int x : {3, 4})
		{
			const WORD pixel = scene.At(x, 1);
			CHECK(Near(pixel >> 11, 31 >> bits));
			CHECK(Near((pixel >> 5) & 0x3F, 63 >> bits));
			CHECK(Near(pixel & 0x1F, 31 >> bits));
		}
		CHECK(scene.RestWhite());
	}
}

TEST(ShadowSpriteLoading, EagerPackReadsAllRecordsWithoutTheLegacyIndex)
{
	Fixture fixture;
	std::vector<WORD> words{3};
	for (unsigned id = 0; id < 3; ++id) words.insert(words.end(), valid.begin(), valid.end());
	Write(words);
	const std::string indexPath = std::string(path) + 'i';
	struct IndexCleanup {
		const std::string& path;
		~IndexCleanup() { std::remove(path.c_str()); }
	} cleanup{indexPath};
	{
		std::ofstream index(indexPath, std::ios::binary | std::ios::trunc);
		index.put(14);
		index.put(0);
	}
	CShadowSpritePack pack;
	CHECK(!pack.LoadFromFileRunning(path));
	CHECK(pack.LoadFromFile(path));
	CHECK_EQ(3, pack.GetSize());
	for (unsigned id = 0; id < pack.GetSize(); ++id) {
		CHECK(pack[id].IsInit());
		CHECK_EQ(4, pack[id].GetWidth());
		CHECK(pack[id].IsColorPixel(3, 0));
	}
}
