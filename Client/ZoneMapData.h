#pragma once
#include "ZoneFileHeader.h"
#include "MImageObject.h"
#include "basic/CPositionList.h"
#include <cstdint>
#include <memory>
#include <vector>

// Complete map data, independent of live sectors, rendering and game globals.
// LoadFromFile publishes only a complete valid file; failure preserves *this.
struct ZoneMapData {
	struct Sector {
		std::uint16_t sprite = 0;
		std::uint8_t property = 0;
		std::uint8_t light = 0;
	};
	struct ImageObject {
		std::unique_ptr<MImageObject> object;
		CPositionList<TYPE_SECTORPOSITION> positions;
	};

	// Resource budgets, not on-disk field widths. These admit a 512x512 map
	// or long maps up to 1024 tiles per side, without unbounded live sectors.
	static constexpr std::size_t MaxFileBytes = 64u * 1024u * 1024u;
	static constexpr std::uint16_t MaxDimension = 1024;
	static constexpr std::size_t MaxSectors = 262144;
	static constexpr std::uint32_t MaxObjects = 262144;
	static constexpr std::size_t MaxPositions = 1048576;

	FILEINFO_ZONE_HEADER info;
	std::uint32_t tileOffset = 0;
	std::uint32_t imageOffset = 0;
	std::uint16_t width = 0;
	std::uint16_t height = 0;
	std::vector<Sector> sectors;
	std::vector<ImageObject> objects;

	bool LoadFromFile(std::ifstream& file, bool skipImageObjects = false);
};

// Fits an inclusive sector rect (portal or safe zone from the zone info
// file) to a width x height grid: orders its edges, then clamps them to
// the grid. Returns false when no sector of the rect is left to visit.
bool ClipSectorRect(int& left, int& top, int& right, int& bottom, int width, int height);
