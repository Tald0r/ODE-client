#pragma once

#include "MPortal.h"

// Zone-info records, independent of live sectors, minimap UI and horn NPCs.
struct ZoneInfoData
{
	WORD width = 0;
	WORD height = 0;
	std::vector<MPortal> portals;
	std::vector<B_RECT> safetyZones;

	bool LoadFromFile(std::ifstream& file, int expectedWidth, int expectedHeight);
};
