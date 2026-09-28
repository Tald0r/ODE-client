#include "test_framework.h"
#include "CrtCompat.h"
#include "SpriteLibBackendSDL.h"
#include <array>
#include <climits>
#include <cstdio>
#include <cstdint>
#include <vector>

namespace {
const std::vector<uint16_t> validLine{2, 1, 2, 0x1234, 0x5678, 1, 1, 0x9abc};

struct SpriteOwner {
	spritectl_sprite_t value = nullptr;
	~SpriteOwner() { spritectl_destroy_sprite(value); }
};

int Load(const std::vector<uint16_t>& words, spritectl_sprite_t* result, size_t byteLimit = SIZE_MAX)
{
	FILE* file = Basic::TemporaryFile();
	CHECK(file != nullptr);
	if (!file) return -1;
	size_t written = 0;
	for (uint16_t word : words) {
		if (written++ < byteLimit) std::fputc(word & 255, file);
		if (written++ < byteLimit) std::fputc(word >> 8, file);
	}
	std::rewind(file);
	int status = spritectl_load_sprite_from_file(file, result, 0);
	std::fclose(file);
	return status;
}
}

TEST(SpriteBackendBounds, RawDimensionsMustMatchTheirPixels)
{
	std::array<uint32_t, 4> pixels{};
	for (int format : {SPRITECTL_FORMAT_RGB565, SPRITECTL_FORMAT_RGB555, SPRITECTL_FORMAT_RGBA32}) {
		const size_t bytes = format == SPRITECTL_FORMAT_RGBA32 ? 16 : 8;
		SpriteOwner good{spritectl_create_sprite(2, 2, format, pixels.data(), bytes)};
		CHECK(good.value != nullptr);
		for (size_t size : {size_t(1), bytes - 1}) {
			SpriteOwner shortBuffer{spritectl_create_sprite(2, 2, format, pixels.data(), size)};
			CHECK(shortBuffer.value == nullptr);
		}
		for (int width : {-1, 0, INT_MAX}) {
			SpriteOwner bad{spritectl_create_sprite(width, 2, format, pixels.data(), bytes)};
			CHECK(bad.value == nullptr);
		}
	}
	SpriteOwner unknown{spritectl_create_sprite(2, 2, -1, pixels.data(), sizeof(pixels))};
	CHECK(unknown.value == nullptr);
}

TEST(SpriteBackendBounds, InvalidScanlineDoesNotReplaceThePreviousOne)
{
	SpriteOwner sprite{spritectl_create_sprite_rle(5, 1)};
	CHECK(sprite.value != nullptr);
	CHECK_EQ(0, spritectl_sprite_set_scanline_rle(sprite.value, 0, validLine.data(), int(validLine.size())));
	const auto* original = sprite.value->scanline_rle[0];
	for (const std::vector<uint16_t>& line : {
		std::vector<uint16_t>{1},
		std::vector<uint16_t>{2, 0, 2, 1, 2}, // later header is absent
		std::vector<uint16_t>{1, 0, 4, 1},
		std::vector<uint16_t>{1, 6, 0},
		std::vector<uint16_t>{1, 4, 2, 1, 2}}) {
		CHECK_EQ(-1, spritectl_sprite_set_scanline_rle(sprite.value, 0, line.data(), int(line.size())));
		CHECK(sprite.value->scanline_rle[0] == original);
		CHECK_EQ(validLine.size(), sprite.value->scanline_lens[0]);
	}
	// The stored length is uint16_t: a larger length must not be narrowed.
	std::vector<uint16_t> oversized(65536, 0);
	CHECK_EQ(-1, spritectl_sprite_set_scanline_rle(sprite.value, 0, oversized.data(), int(oversized.size())));
}

TEST(SpriteBackendBounds, FileLoaderRejectsEveryTruncatedPrefixAndBadRun)
{
	std::vector<uint16_t> file{5, 1, uint16_t(validLine.size())};
	file.insert(file.end(), validLine.begin(), validLine.end());
	for (size_t length = 0; length < file.size() * 2; ++length) {
		SpriteOwner sprite;
		CHECK_EQ(-1, Load(file, &sprite.value, length));
		CHECK(sprite.value == nullptr);
	}
	for (const std::vector<uint16_t>& line : {
		std::vector<uint16_t>{2, 0, 2, 1, 2},
		std::vector<uint16_t>{1, 0, 65535},
		std::vector<uint16_t>{1, 6, 0},
		std::vector<uint16_t>{1, 4, 2, 1, 2}}) {
		std::vector<uint16_t> bytes{5, 1, uint16_t(line.size())};
		bytes.insert(bytes.end(), line.begin(), line.end());
		SpriteOwner sprite;
		CHECK_EQ(-1, Load(bytes, &sprite.value));
		CHECK(sprite.value == nullptr);
	}
	SpriteOwner good;
	CHECK_EQ(0, Load(file, &good.value));
	CHECK(good.value != nullptr);
	if (good.value) {
		CHECK_EQ(0x1234, good.value->pixels[1]);
		CHECK_EQ(0x9abc, good.value->pixels[4]);
	}
}

TEST(SpriteBackendBounds, BlitRejectsMalformedLaterRowsBeforeWriting)
{
	CHECK_EQ(0, spritectl_init());
	auto surface = spritectl_create_surface(5, 2, SPRITECTL_FORMAT_RGB565);
	CHECK(surface != nullptr);
	if (!surface) return;
	SpriteOwner sprite{spritectl_create_sprite_rle(5, 2)};
	for (int y = 0; y < 2; ++y)
		CHECK_EQ(0, spritectl_sprite_set_scanline_rle(sprite.value, y, validLine.data(), int(validLine.size())));
	spritectl_clear_surface(surface, 0x7777);
	// Simulate legacy/internal construction bypassing the validating setter.
	sprite.value->scanline_lens[1] = 5;
	CHECK_EQ(-1, spritectl_blt_sprite_rle(surface, 0, 0, sprite.value, 0, 255));
	spritectl_surface_info_t info{};
	CHECK_EQ(0, spritectl_lock_surface(surface, &info));
	for (int y = 0; y < 2; ++y)
		for (int x = 0; x < 5; ++x)
			CHECK_EQ(0x7777, reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(info.pixels) + y * info.pitch)[x]);
	spritectl_unlock_surface(surface);
	sprite.value->scanline_lens[1] = uint16_t(validLine.size());
	CHECK_EQ(0, spritectl_blt_sprite_rle(surface, -1, 0, sprite.value, 0, 255));
	CHECK_EQ(0, spritectl_lock_surface(surface, &info));
	const uint16_t expected[] = {0x1234, 0x5678, 0x7777, 0x9abc, 0x7777};
	for (int y = 0; y < 2; ++y)
		for (int x = 0; x < 5; ++x)
			CHECK_EQ(expected[x], reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(info.pixels) + y * info.pitch)[x]);
	spritectl_unlock_surface(surface);
	for (int x : {INT_MIN, INT_MAX}) CHECK_EQ(0, spritectl_blt_sprite_rle(surface, x, 0, sprite.value, 0, 255));
	CHECK_EQ(0, surface->locked);
	spritectl_destroy_surface(surface);
}

TEST(SpriteBackendBounds, EmptyRowsRemainTransparent)
{
	for (const std::vector<uint16_t>& bytes : {
		std::vector<uint16_t>{3, 1, 0}, std::vector<uint16_t>{3, 1, 1, 0}}) {
		SpriteOwner sprite;
		CHECK_EQ(0, Load(bytes, &sprite.value));
		CHECK(sprite.value != nullptr);
		if (sprite.value)
			for (int x = 0; x < 3; ++x) CHECK_EQ(0, sprite.value->pixels[x]);
	}
}
