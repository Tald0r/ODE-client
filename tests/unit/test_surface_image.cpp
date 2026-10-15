#include "test_framework.h"
#include "SurfaceImage.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackendSDL.h"
#include <SDL.h>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>

// Loading a *.bmp into a sprite surface (task 4.232, SpriteLib's since
// then): converted to RGB565, as much as fits from the top left. Only a
// .bmp name (any case) of at least 8 characters, a file SDL can load, and
// a surface that can be locked.
namespace {

// A 3x2 BMP: red, green, blue / white, black, red (and black rows below,
// for a taller one).
std::string WriteBmp(const char* extension = ".bmp", int height = 2)
{
	static unsigned sequence = 0;
	const std::string path = (std::filesystem::temp_directory_path() / ("darkeden_surface_image_"
		+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
		+ "_" + std::to_string(sequence++) + extension)).string();
	SDL_Surface* image = SDL_CreateRGBSurfaceWithFormat(0, 3, height, 24, SDL_PIXELFORMAT_RGB24);
	SDL_FillRect(image, NULL, SDL_MapRGB(image->format, 0, 0, 0));
	const Uint32 colours[2][3] = {
		{SDL_MapRGB(image->format, 255, 0, 0), SDL_MapRGB(image->format, 0, 255, 0), SDL_MapRGB(image->format, 0, 0, 255)},
		{SDL_MapRGB(image->format, 255, 255, 255), SDL_MapRGB(image->format, 0, 0, 0), SDL_MapRGB(image->format, 255, 0, 0)}};
	for (int y = 0; y < 2; ++y)
		for (int x = 0; x < 3; ++x)
		{
			SDL_Rect cell{x, y, 1, 1};
			SDL_FillRect(image, &cell, colours[y][x]);
		}
	SDL_SaveBMP(image, path.c_str());
	SDL_FreeSurface(image);
	return path;
}

WORD Pixel(CSpriteSurface& surface, int x, int y)
{
	spritectl_surface_info_t info{};
	if (spritectl_lock_surface(surface.GetBackendSurface(), &info) != 0) return 0xDEAD;
	const WORD value = reinterpret_cast<const WORD*>(static_cast<const unsigned char*>(info.pixels) + y * info.pitch)[x];
	spritectl_unlock_surface(surface.GetBackendSurface());
	return value;
}

struct Surface
{
	CSpriteSurface surface;
	Surface(int width, int height) { spritectl_init(); surface.Init(width, height); surface.FillSurface(0x1234); }
};

} // namespace

TEST(SurfaceImage, ABmpIsCopiedInRGB565)
{
	// A surface wider than the image, so its rows lie further apart.
	const std::string path = WriteBmp();
	Surface target(6, 3);
	CHECK(LoadImageToSurface(path.c_str(), target.surface));
	CHECK_EQ(0xF800, Pixel(target.surface, 0, 0));
	CHECK_EQ(0x07E0, Pixel(target.surface, 1, 0));
	CHECK_EQ(0x001F, Pixel(target.surface, 2, 0));
	CHECK_EQ(0xFFFF, Pixel(target.surface, 0, 1));
	CHECK_EQ(0x0000, Pixel(target.surface, 1, 1));
	CHECK_EQ(0xF800, Pixel(target.surface, 2, 1));
	// Past the image the surface is untouched.
	CHECK_EQ(0x1234, Pixel(target.surface, 3, 0));
	CHECK_EQ(0x1234, Pixel(target.surface, 0, 2));
	std::remove(path.c_str());
}

TEST(SurfaceImage, OnlyWhatFitsIsCopied)
{
	// Narrower than the image: each row stops at the surface's edge, so
	// the row below the image stays untouched.
	const std::string path = WriteBmp();
	Surface target(2, 3);
	CHECK(LoadImageToSurface(path.c_str(), target.surface));
	CHECK_EQ(0xF800, Pixel(target.surface, 0, 0));
	CHECK_EQ(0x07E0, Pixel(target.surface, 1, 0));
	CHECK_EQ(0xFFFF, Pixel(target.surface, 0, 1));
	CHECK_EQ(0x0000, Pixel(target.surface, 1, 1));
	CHECK_EQ(0x1234, Pixel(target.surface, 0, 2));
	CHECK_EQ(0x1234, Pixel(target.surface, 1, 2));
	std::remove(path.c_str());
}

TEST(SurfaceImage, OnlyTheRowsThatFitAreCopied)
{
	// Far shorter than the image: nothing is written past the surface
	// (ASan sees a write past it).
	const std::string path = WriteBmp(".bmp", 256);
	Surface target(4, 1);
	CHECK(LoadImageToSurface(path.c_str(), target.surface));
	CHECK_EQ(0xF800, Pixel(target.surface, 0, 0));
	CHECK_EQ(0x001F, Pixel(target.surface, 2, 0));
	CHECK_EQ(0x1234, Pixel(target.surface, 3, 0));
	std::remove(path.c_str());
}

TEST(SurfaceImage, ANameUnderEightCharactersIsRefused)
{
	// Real BMPs under 7- and 8-character names, in a scratch working
	// directory.
	// Entered for the test's length; back out and removed whatever happens.
	struct Scratch
	{
		std::filesystem::path dir, previous;
		bool entered = false;
		explicit Scratch(std::filesystem::path path) : dir(std::move(path))
		{
			std::error_code error;
			std::filesystem::create_directory(dir, error);
			previous = std::filesystem::current_path(error);
			if (error) return;
			std::filesystem::current_path(dir, error);
			entered = !error;
		}
		~Scratch()
		{
			std::error_code error;
			if (entered) std::filesystem::current_path(previous, error);
			std::filesystem::remove_all(dir, error);
		}
	};
	const std::string path = WriteBmp();
	{
		const Scratch scratch(std::filesystem::path(path).replace_extension(".dir"));
		CHECK(scratch.entered);
		if (scratch.entered)
		{
			std::error_code error;
			std::filesystem::copy_file(path, "sxy.bmp", error);
			std::filesystem::copy_file(path, "sxyz.bmp", error);
			Surface target(4, 3);
			CHECK(!LoadImageToSurface("sxy.bmp", target.surface));
			CHECK_EQ(0x1234, Pixel(target.surface, 0, 0));
			CHECK(LoadImageToSurface("sxyz.bmp", target.surface));
			CHECK_EQ(0xF800, Pixel(target.surface, 0, 0));
		}
	}
	std::remove(path.c_str());
}

TEST(SurfaceImage, TheExtensionInAnyCase)
{
	const std::string upper = WriteBmp(".BMP");
	Surface target(4, 3);
	CHECK(LoadImageToSurface(upper.c_str(), target.surface));
	CHECK_EQ(0x001F, Pixel(target.surface, 2, 0));
	std::remove(upper.c_str());
}

TEST(SurfaceImage, NotABmpOrNotLoadableIsRefused)
{
	Surface target(4, 3);
	CHECK(!LoadImageToSurface(nullptr, target.surface));
	CHECK(!LoadImageToSurface("a.bmp", target.surface));		// under 8 characters
	const std::string jpeg = WriteBmp(".jpg");					// a BMP, misnamed
	CHECK(!LoadImageToSurface(jpeg.c_str(), target.surface));
	std::remove(jpeg.c_str());
	const std::string missing = (std::filesystem::temp_directory_path() / "darkeden_surface_image_missing.bmp").string();
	CHECK(!LoadImageToSurface(missing.c_str(), target.surface));
	CHECK_EQ(0x1234, Pixel(target.surface, 0, 0));
}

TEST(SurfaceImage, ASurfaceThatCannotBeLockedIsRefused)
{
	const std::string path = WriteBmp();
	CSpriteSurface uninitialised;
	CHECK(!LoadImageToSurface(path.c_str(), uninitialised));
	std::remove(path.c_str());
}
