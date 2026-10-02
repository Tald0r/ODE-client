#include "Client_PCH.h"
#include "ZoneInfoData.h"
#include <optional>
#include <type_traits>
#include <utility>

namespace {
bool Fail(std::ifstream& file)
{
	file.setstate(std::ios::failbit);
	return false;
}

std::optional<std::uint64_t> Remaining(std::ifstream& file)
{
	if (!file) return {};
	const auto start = file.tellg();
	if (start == std::streampos(-1)) return {};
	file.seekg(0, std::ios::end);
	const auto end = file.tellg();
	if (!file || end < start) return {};
	file.seekg(start);
	if (!file) return {};
	return static_cast<std::uint64_t>(end - start);
}

template<class T> bool Read(std::ifstream& file, T& value)
{
	static_assert(std::is_unsigned_v<T> && sizeof(T) <= 4);
	unsigned char bytes[sizeof(T)]{};
	if (!file.read(reinterpret_cast<char*>(bytes), sizeof(bytes))) return false;
	std::uint32_t decoded = 0;
	for (unsigned i = 0; i < sizeof(T); ++i) decoded |= std::uint32_t(bytes[i]) << (8 * i);
	value = static_cast<T>(decoded);
	return true;
}

bool CountFits(std::ifstream& file, std::uint32_t count, unsigned recordBytes, unsigned tailBytes)
{
	// Counts were signed four-byte integers. Negative encodings also exceed
	// the resource budget and fail before any record allocation.
	if (count > ZoneInfoData::MaxRecords) return Fail(file);
	const auto remaining = Remaining(file);
	if (!remaining || *remaining < tailBytes ||
		count > (*remaining - tailBytes) / recordBytes) return Fail(file);
	return true;
}
}

bool ZoneInfoData::LoadFromFile(std::ifstream& file, int expectedWidth, int expectedHeight)
{
	const auto fileBytes = Remaining(file);
	if (!fileBytes || *fileBytes > MaxFileBytes) return Fail(file);
	ZoneInfoData loaded;
	if (!Read(file, loaded.width) || !Read(file, loaded.height)) return false;
	if (loaded.width == 0 || loaded.height == 0 || loaded.width != expectedWidth ||
		loaded.height != expectedHeight) return Fail(file);

	std::uint32_t numPortal = 0;
	if (!Read(file, numPortal)) return false;
	// The smallest portal is an empty multi-portal (six bytes); a four-byte
	// safety count must remain even when the portal table is empty.
	if (!CountFits(file, numPortal, 6, 4)) return false;
	loaded.portals.reserve(numPortal);
	for (std::uint32_t i = 0; i < numPortal; ++i)
	{
		MPortal portal;
		portal.LoadFromFile(file);
		if (!file) return false;
		loaded.portals.push_back(portal);
	}

	std::uint32_t numSafe = 0;
	if (!Read(file, numSafe) || !CountFits(file, numSafe, 5, 0)) return false;
	loaded.safetyZones.resize(numSafe);
	static_assert(sizeof(B_RECT) == 5);
	for (auto& rect : loaded.safetyZones)
	{
		if (!file.read(reinterpret_cast<char*>(&rect), SIZE_B_RECT)) return false;
	}
	*this = std::move(loaded);
	return true;
}
