#include "test_framework.h"
#include "CAlphaSprite565.h"
#include "CAlphaSprite555.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackendSDL.h"
#include "ColorDraw.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <vector>

namespace {
constexpr const char* path = "alpha_sprite_bounds_test.bin";
const std::vector<WORD> valid{3, 1, 7, 1, 1, 2, 0x1010, 0xf81f, 0x1010, 0x07e0};

std::vector<char> Bytes(const std::vector<WORD>& words)
{
	std::vector<char> bytes;
	for (WORD word : words) { bytes.push_back(char(word)); bytes.push_back(char(word >> 8)); }
	return bytes;
}

bool Load(CAlphaSprite& sprite, const std::vector<WORD>& words, size_t byteLimit = SIZE_MAX)
{
	const auto bytes = Bytes(words);
	{
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		out.write(bytes.data(), std::streamsize((std::min)(bytes.size(), byteLimit)));
	}
	std::ifstream in(path, std::ios::binary);
	bool result = sprite.LoadFromFile(in);
	in.close();
	std::remove(path);
	return result;
}

template<class Sprite> void RejectMalformedFiles()
{
	Sprite sprite;
	for (size_t bytes = 0; bytes < valid.size() * 2; ++bytes) {
		CHECK(!Load(sprite, valid, bytes));
		CHECK(!sprite.IsInit());
		CHECK_EQ(0, sprite.GetWidth());
		CHECK_EQ(0, sprite.GetHeight());
	}
	for (const auto& words : {
		std::vector<WORD>{3, 1, 3, 1, 0, 2}, // missing alpha/color pairs
		std::vector<WORD>{3, 1, 5, 1, 3, 1, 0x1010, 0xf81f}, // decoded width overflow
		std::vector<WORD>{3, 1, 5, 2, 0, 1, 0x1010, 0xf81f}, // later header absent
		std::vector<WORD>{3, 1, 0}}) {
		CHECK(!Load(sprite, words));
		CHECK(!sprite.IsInit());
	}
	CHECK(Load(sprite, valid));
	CHECK_EQ(3, sprite.GetWidth());
	CHECK_EQ(1, sprite.GetHeight());
}
}

TEST(AlphaSpriteBounds, Loader565RejectsTruncatedAndOverwideRuns) { RejectMalformedFiles<CAlphaSprite565>(); }
TEST(AlphaSpriteBounds, Loader555RejectsTruncatedAndOverwideRuns) { RejectMalformedFiles<CAlphaSprite555>(); }

TEST(AlphaSpriteBounds, RejectedRowsDoNotDesynchronizeTheNextPackedSprite)
{
	for (bool use555 : {false, true}) {
		CAlphaSprite565 sprite565;
		CAlphaSprite555 sprite555;
		CAlphaSprite& sprite = use555 ? static_cast<CAlphaSprite&>(sprite555) : static_cast<CAlphaSprite&>(sprite565);
		std::vector<WORD> bad{3, 2, 3, 1, 0, 6, 1, 0}; // invalid first row, empty second row
		bad.insert(bad.end(), valid.begin(), valid.end());
		const auto bytes = Bytes(bad);
		{
			std::ofstream out(path, std::ios::binary | std::ios::trunc);
			out.write(bytes.data(), std::streamsize(bytes.size()));
		}
		std::ifstream in(path, std::ios::binary);
		CHECK(!sprite.LoadFromFile(in));
		CHECK(sprite.LoadFromFile(in));
		CHECK_EQ(3, sprite.GetWidth());
		CHECK_EQ(use555 ? ColorDraw::Convert565to555(0xf81f) : 0xf81f, sprite.GetPixelLine(0)[4]);
		in.close();
		std::remove(path);
	}
}

TEST(AlphaSpriteBounds, AdapterRejectsMutatedRunsWithoutCreatingABackend)
{
	CHECK_EQ(0, spritectl_init());
	CAlphaSprite565 sprite;
	CHECK(Load(sprite, valid));
	CSpriteSurface surface;
	CHECK(surface.Init(3, 1));
	POINT origin{0, 0};
	sprite.GetPixelLine(0)[2] = 3; // runs past both encoded and decoded bounds
	surface.BltAlphaSprite(&origin, &sprite);
	CHECK(sprite.GetBackendSprite() == nullptr);
	sprite.GetPixelLine(0)[2] = 2;
	surface.BltAlphaSprite(&origin, &sprite);
	CHECK(sprite.GetBackendSprite() != nullptr);
	sprite.GetPixelLine(0)[2] = 3;
	sprite.SetBackendDirty(true);
	surface.BltAlphaSprite(&origin, &sprite);
	CHECK(sprite.GetBackendSprite() == nullptr);
}

TEST(AlphaSpriteBounds, CopyAndGeneratedPixelsRetainTheirScanlines)
{
	CHECK_EQ(0, spritectl_init());
	CAlphaSprite565 loaded, copied, generated;
	CHECK(Load(loaded, valid));
	copied = loaded;
	loaded.Release();
	WORD pixels[]{0, 0xf81f, 0x07e0}, alpha[]{0, 16, 16};
	generated.SetPixel(pixels, sizeof(pixels), alpha, sizeof(alpha), 3, 1);
	CSpriteSurface surface;
	CHECK(surface.Init(3, 1));
	POINT origin{0, 0};
	for (CAlphaSprite* sprite : {static_cast<CAlphaSprite*>(&copied), static_cast<CAlphaSprite*>(&generated)}) {
		surface.BltAlphaSprite(&origin, sprite);
		auto backend = sprite->GetBackendSprite();
		CHECK(backend != nullptr);
		if (backend) { CHECK_EQ(0xf81f, backend->pixels[1]); CHECK_EQ(0x07e0, backend->pixels[2]); }
	}
}

TEST(AlphaSpriteBounds, CopyPreservesPaddingAndSelfAssignment)
{
	CAlphaSprite565 source, copy;
	auto padded = valid;
	padded[2] += 2;
	padded.push_back(0x1111);
	padded.push_back(0x2222);
	CHECK(Load(source, padded));
	copy = source;
	CAlphaSprite565& self = copy;
	copy = self;
	source.Release();
	const auto line = copy.GetPixelLineSpan(0);
	CHECK_EQ(9, line.size());
	if (line.size() == 9) { CHECK_EQ(0x1111, line[7]); CHECK_EQ(0x2222, line[8]); }
	copy.Release();
	CHECK(copy.GetPixelLineSpan(0).empty());
	CHECK(Load(copy, {0, 1}));
	CHECK(copy.IsInit());
	copy.Release();
	CHECK(!copy.IsInit());
	CHECK_EQ(0, copy.GetHeight());
}
