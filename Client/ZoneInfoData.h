#pragma once

#include "MPortal.h"
#include <cstddef>
#include <cstdint>

// Zone-info records, independent of live sectors, minimap UI and horn NPCs.
// Failed loads preserve the previous data and set the stream failure state.
struct ZoneInfoData
{
	// Resource budgets, not file-field widths. A legacy trailer may follow
	// the records; it counts toward the byte budget but is left unread.
	static constexpr std::size_t MaxFileBytes = 64u * 1024u * 1024u;
	static constexpr std::uint32_t MaxRecords = 65536;

	WORD width = 0;
	WORD height = 0;
	std::vector<MPortal> portals;
	std::vector<B_RECT> safetyZones;

	bool LoadFromFile(std::ifstream& file, int expectedWidth, int expectedHeight);
};
