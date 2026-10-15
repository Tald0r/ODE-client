//-----------------------------------------------------------------------------
// SurfaceImage.cpp
//-----------------------------------------------------------------------------
// Loads a *.bmp into a sprite surface (task 4.232: UtilityFunction.cpp's
// until then). USE_SDL_BACKEND is mandatory on all platforms, including
// PLATFORM_WINDOWS.
//-----------------------------------------------------------------------------
#include "SurfaceImage.h"
#include "CSpriteSurface.h"
#include "../../basic/DataPath.h"
#include <SDL.h>
#include <cstdio>
#include <cstring>
#include <string>

bool LoadImageToSurface(const char* pFilename, CSpriteSurface& surface)
{
	if (pFilename == NULL) {
		return false;
	}

	// Resolved for the disk (basic/DataPath.h): the identity on Windows;
	// off Windows the separators folded and each component matched
	// case-insensitively. SDL_LoadBMP() below opens it.
	const std::string sResolved = Basic::NormalizeDataPath(pFilename);
	pFilename = sResolved.c_str();

	int fileLen = static_cast<int>(strlen(pFilename));

	if (fileLen < 8) {
		return false;
	}

	// Check file extension
	char checkStr[10];
	snprintf(checkStr, sizeof(checkStr), "%s", pFilename + fileLen - 4);
	for (int i = 0; static_cast<size_t>(i) < strlen(checkStr); i++) {
		if (checkStr[i] >= 'A' && checkStr[i] <= 'Z') {
			checkStr[i] += 'a' - 'A';
		}
	}

	// Only support .bmp files for now
	if (strncmp(".bmp", checkStr, 4) != 0) {
		return false;
	}

	// Load image using SDL_image
	SDL_Surface* loaded = SDL_LoadBMP(pFilename);
	if (loaded == NULL) {
		return false;
	}

	// Convert to RGB565 format
	SDL_Surface* converted = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGB565, 0);
	if (converted == NULL) {
		SDL_FreeSurface(loaded);
		return false;
	}

	DWORD pitch = 0;
	auto* dest = static_cast<BYTE*>(surface.Lock(nullptr, &pitch));
	if (!dest) {
		SDL_FreeSurface(converted);
		SDL_FreeSurface(loaded);
		return false;
	}
	const int width = min(converted->w, surface.GetWidth());
	const int height = min(converted->h, surface.GetHeight());
	for (int y = 0; y < height; ++y)
		memcpy(dest + y * pitch, static_cast<BYTE*>(converted->pixels) + y * converted->pitch, width * sizeof(WORD));
	surface.Unlock();

	SDL_FreeSurface(converted);
	SDL_FreeSurface(loaded);

	return true;
}
