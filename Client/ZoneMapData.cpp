#include "Client_PCH.h"
#include "ZoneMapData.h"
#include "MShadowObject.h"
#include "MAnimationObject.h"
#include "MShadowAnimationObject.h"
#include "MInteractionObject.h"
#include <optional>
#include <type_traits>
#include <unordered_set>
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

std::unique_ptr<MImageObject> NewObject(std::uint8_t type)
{
	switch (type) {
	case MObject::TYPE_IMAGEOBJECT: return std::make_unique<MImageObject>();
	case MObject::TYPE_SHADOWOBJECT: return std::make_unique<MShadowObject>();
	case MObject::TYPE_ANIMATIONOBJECT: return std::make_unique<MAnimationObject>();
	case MObject::TYPE_SHADOWANIMATIONOBJECT: return std::make_unique<MShadowAnimationObject>();
	case MObject::TYPE_INTERACTIONOBJECT: return std::make_unique<MInteractionObject>();
	default: return {};
	}
}
}

bool ZoneMapData::LoadFromFile(std::ifstream& file, bool skipImageObjects)
{
	const auto fileBytes = Remaining(file);
	if (!fileBytes || *fileBytes > MaxFileBytes) return Fail(file);

	ZoneMapData loaded;
	loaded.info.LoadFromFile(file);
	if (!file || loaded.info.ZoneVersion != MAP_VERSION_2000_05_10) return Fail(file);
	if (!Read(file, loaded.tileOffset) || !Read(file, loaded.imageOffset) ||
		!Read(file, loaded.width) || !Read(file, loaded.height)) return false;
	if (loaded.width == 0 || loaded.height == 0 || loaded.width > MaxDimension ||
		loaded.height > MaxDimension) return Fail(file);
	const auto cells = std::size_t(loaded.width) * loaded.height;
	if (cells > MaxSectors) return Fail(file);
	const auto sectorBytes = Remaining(file);
	if (!sectorBytes || *sectorBytes < cells * 4 + 4) return Fail(file);
	loaded.sectors.resize(cells);
	for (auto& sector : loaded.sectors) {
		if (!Read(file, sector.sprite) || !Read(file, sector.property) ||
			!Read(file, sector.light)) return false;
	}

	std::uint32_t count = 0;
	if (!Read(file, count)) return false;
	// The historical count is a signed four-byte integer; negative encodings
	// also exceed this resource budget and are rejected before allocation.
	if (count > MaxObjects) return Fail(file);
	if (!skipImageObjects) {
		const auto objectBytes = Remaining(file);
		// Tag + the smallest MImageObject body + an empty WORD position list.
		constexpr std::uint32_t minObjectBytes = 30;
		if (!objectBytes || count > *objectBytes / minObjectBytes) return Fail(file);
		loaded.objects.reserve(count);
		std::unordered_set<TYPE_OBJECTID> ids;
		std::size_t positions = 0;
		for (std::uint32_t i = 0; i < count; ++i) {
			std::uint8_t type = 0;
			if (!Read(file, type)) return false;
			ImageObject entry;
			entry.object = NewObject(type);
			if (!entry.object) return Fail(file);
			entry.object->LoadFromFile(file);
			if (!file || entry.object->GetObjectType() != type ||
				(type <= MObject::TYPE_SHADOWOBJECT && entry.object->IsAnimation()) ||
				!ids.insert(entry.object->GetID()).second) return Fail(file);
			entry.positions.LoadFromFile(file);
			if (!file) return false;
			positions += static_cast<std::size_t>(entry.positions.GetSize());
			if (positions > MaxPositions) return Fail(file);
			for (auto p = entry.positions.GetIterator(); p != entry.positions.GetEnd(); ++p)
				if (p->X >= loaded.width || p->Y >= loaded.height) return Fail(file);
			loaded.objects.push_back(std::move(entry));
		}
	}
	*this = std::move(loaded);
	return true;
}

bool ClipSectorRect(int& left, int& top, int& right, int& bottom, int width, int height)
{
	// Order the edges first, so that clamping an edge that lies off the
	// grid leaves an empty range instead of one reaching past the grid.
	if (top > bottom) std::swap(top, bottom);
	if (left > right) std::swap(left, right);

	if (top < 0) top = 0;
	if (left < 0) left = 0;
	if (bottom >= height) bottom = height-1;
	if (right >= width) right = width-1;

	return top <= bottom && left <= right;
}
