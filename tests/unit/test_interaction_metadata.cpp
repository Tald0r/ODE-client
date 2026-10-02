#include "test_framework.h"
#include "MInteractionObjectTable.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<unsigned char>;
using Info = INTERACTIONOBJECTTABLE_INFO;

// One byte of type, two of frame, four of properties, and four for the
// legacy sound slot (only its low two bytes identify a sound).
const Bytes record{0xfe, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 0xcd, 0xab, 0, 0};

struct MetadataFile
{
	std::filesystem::path path;
	explicit MetadataFile(const Bytes& bytes)
	{
		static unsigned sequence = 0;
		path = std::filesystem::temp_directory_path() / ("darkeden_interaction_"
			+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
			+ "_" + std::to_string(sequence++) + ".bin");
		std::ofstream output(path, std::ios::binary);
		if (!bytes.empty()) output.write(reinterpret_cast<const char*>(bytes.data()),
			static_cast<std::streamsize>(bytes.size()));
		output.close();
		if (!output.good()) throw std::runtime_error("Cannot write interaction fixture");
	}
	~MetadataFile() { std::error_code error; std::filesystem::remove(path, error); }
};

template<class T> bool Load(T& value, const Bytes& bytes)
{
	MetadataFile fixture(bytes);
	std::ifstream input(fixture.path, std::ios::binary);
	value.LoadFromFile(input);
	if (input.good()) CHECK_EQ(bytes.size(), input.tellg());
	return input.good();
}

template<class T> Bytes Save(T& value)
{
	MetadataFile fixture({});
	{
		std::ofstream output(fixture.path, std::ios::binary);
		value.SaveToFile(output);
		CHECK(output.good());
	}
	std::ifstream input(fixture.path, std::ios::binary);
	return {std::istreambuf_iterator<char>(input), {}};
}

void CheckRecord(const Info& info, unsigned type, unsigned frame, int property, unsigned sound)
{
	CHECK_EQ(type, info.Type);
	CHECK_EQ(frame, info.FrameID);
	CHECK_EQ(property, info.Property);
	CHECK_EQ(sound, info.SoundID);
}
}

TEST(InteractionMetadata, ValueInitializedRecordsAndMissingRowsHaveZeroFields)
{
	const Info info{};
	CheckRecord(info, 0, 0, 0, 0);
	INTERACTIONOBJECT_TABLE table;
	CheckRecord(table[0], 0, 0, 0, 0);
	CHECK(table.GetMutable(-1) == nullptr);
	CHECK(table.GetMutable(0) == nullptr);
}

TEST(InteractionMetadata, RecordReadsAndWritesTheElevenByteLegacyLayout)
{
	Info info{};
	CHECK(Load(info, record));
	CheckRecord(info, 0xfe, 0x1234, 0x12345678, 0xabcd);
	CHECK(Save(info) == record);
}

TEST(InteractionMetadata, FullWidthIdsAndNegativePropertyBitsSurviveLoading)
{
	const Bytes bytes{0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x80, 0xff, 0xff, 0, 0};
	Info info{};
	CHECK(Load(info, bytes));
	CheckRecord(info, 255, 65535, (-2147483647 - 1), 65535);
	CHECK(Save(info) == bytes);
}

TEST(InteractionMetadata, SoundSlotHighBytesDoNotChangeTheSoundId)
{
	Bytes bytes = record;
	bytes[9] = 0x55;
	bytes[10] = 0xaa;
	Info info{};
	CHECK(Load(info, bytes));
	CheckRecord(info, 0xfe, 0x1234, 0x12345678, 0xabcd);
}

TEST(InteractionMetadata, ConsecutiveRecordsKeepTheirBoundariesAndTrailer)
{
	Bytes bytes{0xee};
	bytes.insert(bytes.end(), record.begin(), record.end());
	const Bytes second{2, 9, 0, 0xff, 0xff, 0xff, 0xff, 7, 0, 0, 0};
	bytes.insert(bytes.end(), second.begin(), second.end());
	bytes.push_back(0xdd);
	MetadataFile fixture(bytes);
	std::ifstream input(fixture.path, std::ios::binary);
	input.seekg(1);
	Info first{}, next{};
	first.LoadFromFile(input);
	CHECK_EQ(12, input.tellg());
	next.LoadFromFile(input);
	CHECK_EQ(23, input.tellg());
	CHECK_EQ(0xdd, input.get());
	CheckRecord(first, 0xfe, 0x1234, 0x12345678, 0xabcd);
	CheckRecord(next, 2, 9, -1, 7);
}

TEST(InteractionMetadata, RecordCopiesKeepIndependentValues)
{
	Info original{};
	CHECK(Load(original, record));
	Info copied = original;
	Info assigned{};
	assigned = original;
	original.Type = 0; original.FrameID = 0;
	original.Property = 0; original.SoundID = 0;
	CheckRecord(copied, 0xfe, 0x1234, 0x12345678, 0xabcd);
	CheckRecord(assigned, 0xfe, 0x1234, 0x12345678, 0xabcd);
}

TEST(InteractionMetadata, TableLoadsRowsAndSavesTheirOriginalWireLayout)
{
	Bytes bytes{2, 0, 0, 0};
	bytes.insert(bytes.end(), record.begin(), record.end());
	bytes.insert(bytes.end(), record.begin(), record.end());
	INTERACTIONOBJECT_TABLE table;
	CHECK(Load(table, bytes));
	CHECK_EQ(2, table.GetSize());
	CheckRecord(table[0], 0xfe, 0x1234, 0x12345678, 0xabcd);
	CheckRecord(table[1], 0xfe, 0x1234, 0x12345678, 0xabcd);
	CHECK(table.GetMutable(2) == nullptr);
	CheckRecord(table[2], 0, 0, 0, 0);
	CHECK(Save(table) == bytes);
}

TEST(InteractionMetadata, AZeroCountClearsAnExistingTable)
{
	INTERACTIONOBJECT_TABLE table;
	table.Init(1);
	table.GetMutable(0)->SoundID = 42;
	CHECK(Load(table, {0, 0, 0, 0}));
	CHECK_EQ(0, table.GetSize());
	CHECK(Save(table) == Bytes({0, 0, 0, 0}));
}
