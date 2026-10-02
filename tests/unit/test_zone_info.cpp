#include "test_framework.h"
#include "MPortal.h"
#include "ZoneInfoData.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<unsigned char>;

struct ZoneInfoFile
{
	std::filesystem::path path;
	explicit ZoneInfoFile(const Bytes& bytes)
	{
		static unsigned int sequence = 0;
		path = std::filesystem::temp_directory_path() / ("darkeden_zone_info_"
			+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
			+ "_" + std::to_string(sequence++) + ".bin");
		std::ofstream output(path, std::ios::binary);
		if (!bytes.empty()) output.write(reinterpret_cast<const char*>(bytes.data()),
			static_cast<std::streamsize>(bytes.size()));
		output.close();
		if (!output.good()) throw std::runtime_error("Cannot write zone-info fixture");
	}
	~ZoneInfoFile() { std::error_code error; std::filesystem::remove(path, error); }
};

bool Load(MPortal& portal, const Bytes& bytes)
{
	ZoneInfoFile fixture(bytes);
	std::ifstream input(fixture.path, std::ios::binary);
	portal.LoadFromFile(input);
	if (input.good()) CHECK_EQ(bytes.size(), input.tellg());
	return input.good();
}

Bytes Save(MPortal& portal)
{
	ZoneInfoFile fixture({});
	{
		std::ofstream output(fixture.path, std::ios::binary);
		portal.SaveToFile(output);
		CHECK(output.good());
	}
	std::ifstream input(fixture.path, std::ios::binary);
	return {std::istreambuf_iterator<char>(input), {}};
}

void CheckRect(const MPortal& portal, int left, int top, int right, int bottom)
{
	CHECK_EQ(left, portal.GetLeft()); CHECK_EQ(top, portal.GetTop());
	CHECK_EQ(right, portal.GetRight()); CHECK_EQ(bottom, portal.GetBottom());
	const auto rect = portal.GetRect();
	CHECK_EQ(left, rect.left); CHECK_EQ(top, rect.top);
	CHECK_EQ(right, rect.right); CHECK_EQ(bottom, rect.bottom);
}

Bytes InfoBytes()
{
	return {
		0x23, 0x01, 0x34, 0x02, // 291 x 564
		2, 0, 0, 0, // two portals
		0, 61, 0, 1, 2, 3, 4,
		3, 2, 0xff, 0xff, 0, 0, 9, 8, 7, 6,
		2, 0, 0, 0, // two safety rectangles, including unknown flag bits
		0x15, 11, 12, 13, 14,
		0x80, 255, 254, 0, 1
	};
}

void AppendCount(Bytes& bytes, std::uint32_t count)
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		bytes.push_back(static_cast<unsigned char>(count >> shift));
}

bool LoadInfo(ZoneInfoData& info, const Bytes& bytes, int width = 291, int height = 564)
{
	ZoneInfoFile fixture(bytes);
	std::ifstream input(fixture.path, std::ios::binary);
	return info.LoadFromFile(input, width, height);
}

void CheckInfo(const ZoneInfoData& info)
{
	CHECK_EQ(291, info.width); CHECK_EQ(564, info.height);
	CHECK_EQ(2, info.portals.size()); CHECK_EQ(2, info.safetyZones.size());
	if (info.portals.size() != 2 || info.safetyZones.size() != 2) return;
	CHECK_EQ(0, info.portals[0].GetType());
	CHECK(info.portals[0].GetZoneID() == std::vector<WORD>({61}));
	CheckRect(info.portals[0], 1, 2, 3, 4);
	CHECK_EQ(3, info.portals[1].GetType());
	CHECK(info.portals[1].GetZoneID() == std::vector<WORD>({65535, 0}));
	CheckRect(info.portals[1], 9, 8, 7, 6);
	const auto& first = info.safetyZones[0];
	CHECK_EQ(0x15, first.flag);
	CHECK_EQ(11, first.left); CHECK_EQ(12, first.top);
	CHECK_EQ(13, first.right); CHECK_EQ(14, first.bottom);
	const auto& second = info.safetyZones[1];
	CHECK_EQ(0x80, second.flag);
	CHECK_EQ(255, second.left); CHECK_EQ(254, second.top);
	CHECK_EQ(0, second.right); CHECK_EQ(1, second.bottom);
}
}

TEST(ZoneInfo, PortalDefaultsInitializeTypeDestinationsAndRect)
{
	MPortal portal;
	CHECK_EQ(MPortal::TYPE_NOMAL, portal.GetType());
	CHECK(portal.GetZoneID().empty());
	CheckRect(portal, 0, 0, 0, 0);
}

TEST(ZoneInfo, PortalRectConstructorOwnsItsDestinations)
{
	std::vector<WORD> destinations{0, 61, 65535};
	MPortal portal(destinations, P_RECT{1, 2, 254, 255}, MPortal::TYPE_MULTI_PORTAL);
	destinations.clear();
	CHECK(portal.GetZoneID() == std::vector<WORD>({0, 61, 65535}));
	CHECK_EQ(MPortal::TYPE_MULTI_PORTAL, portal.GetType());
	CheckRect(portal, 1, 2, 254, 255);
}

TEST(ZoneInfo, PortalCornerConstructorPreservesReversedRectangles)
{
	MPortal portal({65535}, 255, 254, 2, 1, MPortal::TYPE_CLIENT_ONLY);
	CHECK(portal.GetZoneID() == std::vector<WORD>({65535}));
	CHECK_EQ(255, portal.GetType());
	CheckRect(portal, 255, 254, 2, 1);
}

TEST(ZoneInfo, PortalMutatorsAndCopiesHaveIndependentValueState)
{
	MPortal portal;
	portal.SetZoneID({61, 62});
	portal.SetType(MPortal::TYPE_MULTI_PORTAL);
	portal.SetRect(P_RECT{3, 4, 5, 6});
	MPortal copied = portal;
	MPortal assigned;
	assigned = portal;
	portal.SetLeft(7); portal.SetTop(8); portal.SetRight(9); portal.SetBottom(10);
	portal.SetZoneID({1003});
	CHECK(copied.GetZoneID() == std::vector<WORD>({61, 62}));
	CHECK(assigned.GetZoneID() == copied.GetZoneID());
	CHECK_EQ(MPortal::TYPE_MULTI_PORTAL, assigned.GetType());
	CheckRect(copied, 3, 4, 5, 6);
	CheckRect(assigned, 3, 4, 5, 6);
	CheckRect(portal, 7, 8, 9, 10);
	portal.SetRect(11, 12, 13, 14);
	CheckRect(portal, 11, 12, 13, 14);
}

TEST(ZoneInfo, EveryNonMultiTypeHasOneImplicitLittleEndianDestination)
{
	for (int type = 0; type <= 255; ++type)
	{
		if (type == MPortal::TYPE_MULTI_PORTAL) continue;
		const Bytes encoded{static_cast<BYTE>(type), 0x34, 0x12, 9, 8, 7, 6};
		MPortal portal;
		CHECK(Load(portal, encoded));
		CHECK_EQ(type, portal.GetType());
		CHECK(portal.GetZoneID() == std::vector<WORD>({0x1234}));
		CheckRect(portal, 9, 8, 7, 6);
		CHECK(Save(portal) == encoded);
	}
}

TEST(ZoneInfo, MultiPortalKeepsDestinationOrderAndDuplicates)
{
	const Bytes encoded{3, 4, 0x34, 0x12, 0, 0, 0xff, 0xff, 0x34, 0x12, 1, 2, 3, 4};
	MPortal portal;
	CHECK(Load(portal, encoded));
	CHECK(portal.GetZoneID() == std::vector<WORD>({0x1234, 0, 65535, 0x1234}));
	CheckRect(portal, 1, 2, 3, 4);
	CHECK(Save(portal) == encoded);
}

TEST(ZoneInfo, EmptyMultiPortalStillCarriesItsOwnRectangle)
{
	const Bytes encoded{3, 0, 9, 8, 7, 6};
	MPortal portal;
	CHECK(Load(portal, encoded));
	CHECK(portal.GetZoneID().empty());
	CheckRect(portal, 9, 8, 7, 6);
	CHECK(Save(portal) == encoded);
}

TEST(ZoneInfo, MultiPortalSupportsTheFullByteDestinationCount)
{
	Bytes encoded{3, 255};
	std::vector<WORD> destinations;
	for (int i = 0; i < 255; ++i)
	{
		destinations.push_back(static_cast<WORD>(i * 257));
		encoded.push_back(static_cast<BYTE>(i));
		encoded.push_back(static_cast<BYTE>(i));
	}
	encoded.insert(encoded.end(), {0, 0, 255, 255});
	MPortal portal;
	CHECK(Load(portal, encoded));
	CHECK(portal.GetZoneID() == destinations);
	CHECK(Save(portal) == encoded);
}

TEST(ZoneInfo, LoadingAPortalReplacesItsPreviousRecord)
{
	MPortal portal({100, 200}, 1, 2, 3, 4, MPortal::TYPE_MULTI_PORTAL);
	CHECK(Load(portal, {0, 61, 0, 5, 6, 7, 8}));
	CHECK_EQ(MPortal::TYPE_NOMAL, portal.GetType());
	CHECK(portal.GetZoneID() == std::vector<WORD>({61}));
	CheckRect(portal, 5, 6, 7, 8);
}

TEST(ZoneInfo, PortalStreamsStartAtTheCurrentPositionAndLeaveTheTrailer)
{
	ZoneInfoFile fixture({0xee, 0, 61, 0, 1, 2, 3, 4, 3, 0, 9, 8, 7, 6, 0xdd});
	std::ifstream input(fixture.path, std::ios::binary);
	input.seekg(1);
	MPortal first, second;
	first.LoadFromFile(input);
	CHECK_EQ(8, input.tellg());
	CHECK(first.GetZoneID() == std::vector<WORD>({61}));
	second.LoadFromFile(input);
	CHECK_EQ(14, input.tellg());
	CHECK(second.GetZoneID().empty());
	CheckRect(second, 9, 8, 7, 6);
	CHECK_EQ(0xdd, input.get());
}

TEST(ZoneInfo, EveryTruncatedSinglePortalPreservesThePreviousRecord)
{
	const Bytes encoded{0, 61, 0, 9, 8, 7, 6};
	for (std::size_t size = 0; size < encoded.size(); ++size)
	{
		MPortal portal({100, 200}, 1, 2, 3, 4, MPortal::TYPE_MULTI_PORTAL);
		CHECK(!Load(portal, Bytes(encoded.begin(), encoded.begin() + size)));
		CHECK_EQ(MPortal::TYPE_MULTI_PORTAL, portal.GetType());
		CHECK(portal.GetZoneID() == std::vector<WORD>({100, 200}));
		CheckRect(portal, 1, 2, 3, 4);
	}
}

TEST(ZoneInfo, EveryTruncatedMultiPortalPreservesThePreviousRecord)
{
	const Bytes encoded{3, 2, 61, 0, 62, 0, 9, 8, 7, 6};
	for (std::size_t size = 0; size < encoded.size(); ++size)
	{
		MPortal portal({100}, 1, 2, 3, 4, MPortal::TYPE_GUILD_PORTAL);
		CHECK(!Load(portal, Bytes(encoded.begin(), encoded.begin() + size)));
		CHECK_EQ(MPortal::TYPE_GUILD_PORTAL, portal.GetType());
		CHECK(portal.GetZoneID() == std::vector<WORD>({100}));
		CheckRect(portal, 1, 2, 3, 4);
	}
}

TEST(ZoneInfo, ClosedPortalInputPreservesThePreviousRecord)
{
	MPortal portal({100}, 1, 2, 3, 4, MPortal::TYPE_GUILD_PORTAL);
	std::ifstream input;
	portal.LoadFromFile(input);
	CHECK(input.fail());
	CHECK_EQ(MPortal::TYPE_GUILD_PORTAL, portal.GetType());
	CHECK(portal.GetZoneID() == std::vector<WORD>({100}));
	CheckRect(portal, 1, 2, 3, 4);
}

TEST(ZoneInfo, PortalInputExceptionsPreserveThePreviousRecord)
{
	ZoneInfoFile fixture({3, 2, 61, 0, 62});
	std::ifstream input(fixture.path, std::ios::binary);
	input.exceptions(std::ios::failbit | std::ios::badbit);
	MPortal portal({100}, 1, 2, 3, 4, MPortal::TYPE_GUILD_PORTAL);
	bool rejected = false;
	try { portal.LoadFromFile(input); }
	catch (const std::ios_base::failure&) { rejected = true; }
	CHECK(rejected);
	CHECK_EQ(MPortal::TYPE_GUILD_PORTAL, portal.GetType());
	CHECK(portal.GetZoneID() == std::vector<WORD>({100}));
	CheckRect(portal, 1, 2, 3, 4);
}

TEST(ZoneInfo, SinglePortalSaveRejectsMissingOrExtraDestinationsBeforeWriting)
{
	for (int type : {0, 4, 255})
		for (const std::vector<WORD>& destinations : {std::vector<WORD>{}, {61, 62}})
		{
			MPortal portal(destinations, 1, 2, 3, 4, static_cast<BYTE>(type));
			ZoneInfoFile fixture({});
			std::ofstream output(fixture.path, std::ios::binary);
			portal.SaveToFile(output);
			CHECK(output.fail());
			output.close();
			CHECK_EQ(0, std::filesystem::file_size(fixture.path));
		}
}

TEST(ZoneInfo, MultiPortalSaveRejectsUnrepresentableCountsBeforeWriting)
{
	for (std::size_t count : {256, 257, 511})
	{
		MPortal portal(std::vector<WORD>(count, 61), 1, 2, 3, 4, MPortal::TYPE_MULTI_PORTAL);
		ZoneInfoFile fixture({});
		std::ofstream output(fixture.path, std::ios::binary);
		portal.SaveToFile(output);
		CHECK(output.fail());
		output.close();
		CHECK_EQ(0, std::filesystem::file_size(fixture.path));
	}
}

TEST(ZoneInfo, CompleteFileDefaultsToEmptyState)
{
	ZoneInfoData info;
	CHECK_EQ(0, info.width); CHECK_EQ(0, info.height);
	CHECK(info.portals.empty()); CHECK(info.safetyZones.empty());
}

TEST(ZoneInfo, CompleteFileReadsPortalAndSafetyRecordsInOrder)
{
	ZoneInfoData info;
	CHECK(LoadInfo(info, InfoBytes()));
	CheckInfo(info);
}

TEST(ZoneInfo, CompleteFileAcceptsEmptyTablesAndFullWordDimensions)
{
	ZoneInfoData info;
	CHECK(LoadInfo(info, {255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0}, 65535, 65535));
	CHECK_EQ(65535, info.width); CHECK_EQ(65535, info.height);
	CHECK(info.portals.empty()); CHECK(info.safetyZones.empty());
}

TEST(ZoneInfo, CompleteFileCanReplacePopulatedTablesWithEmptyOnes)
{
	ZoneInfoData info;
	CHECK(LoadInfo(info, InfoBytes()));
	CHECK(LoadInfo(info, {1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 1, 2));
	CHECK_EQ(1, info.width); CHECK_EQ(2, info.height);
	CHECK(info.portals.empty()); CHECK(info.safetyZones.empty());
}

TEST(ZoneInfo, CompleteFileDimensionsAreCheckedBeforeReadingTheTables)
{
	ZoneInfoFile fixture(InfoBytes());
	std::ifstream input(fixture.path, std::ios::binary);
	ZoneInfoData info;
	CHECK(!info.LoadFromFile(input, 123, 456));
	input.clear();
	CHECK_EQ(4, input.tellg());
}

TEST(ZoneInfo, CompleteFileReadsFromTheCurrentPositionAndLeavesLegacyTrailer)
{
	auto bytes = InfoBytes();
	bytes.insert(bytes.begin(), 0xee);
	bytes.push_back(0xdd);
	ZoneInfoFile fixture(bytes);
	std::ifstream input(fixture.path, std::ios::binary);
	input.seekg(1);
	ZoneInfoData info;
	CHECK(info.LoadFromFile(input, 291, 564));
	CHECK_EQ(bytes.size() - 1, input.tellg());
	CHECK_EQ(0xdd, input.get());
	CheckInfo(info);
}

TEST(ZoneInfo, CompleteFileCopiesOwnTheirPortalAndSafetyRecords)
{
	ZoneInfoData info;
	CHECK(LoadInfo(info, InfoBytes()));
	auto copied = info;
	info.portals.clear();
	info.safetyZones.clear();
	CheckInfo(copied);
}

TEST(ZoneInfo, EveryTruncatedCompleteFileIsRejectedAndPreservesPreviousState)
{
	const auto bytes = InfoBytes();
	for (std::size_t size = 0; size < bytes.size(); ++size)
	{
		ZoneInfoData info;
		CHECK(LoadInfo(info, bytes));
		CHECK(!LoadInfo(info, Bytes(bytes.begin(), bytes.begin() + size)));
		CheckInfo(info);
	}
}

TEST(ZoneInfo, MismatchedDimensionsPreservePreviousCompleteFile)
{
	ZoneInfoData info;
	CHECK(LoadInfo(info, InfoBytes()));
	CHECK(!LoadInfo(info, {1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0}));
	CheckInfo(info);
}

TEST(ZoneInfo, NegativePortalCountsAreRejectedAndPreservePreviousState)
{
	ZoneInfoData info;
	CHECK(LoadInfo(info, InfoBytes()));
	CHECK(!LoadInfo(info, {0x23, 1, 0x34, 2, 255, 255, 255, 255, 0, 0, 0, 0}));
	CheckInfo(info);
}

TEST(ZoneInfo, NegativeSafetyCountsAreRejectedAndPreservePreviousState)
{
	ZoneInfoData info;
	CHECK(LoadInfo(info, InfoBytes()));
	CHECK(!LoadInfo(info, {0x23, 1, 0x34, 2, 0, 0, 0, 0, 255, 255, 255, 255}));
	CheckInfo(info);
}

TEST(ZoneInfo, ClosedCompleteFileInputIsRejectedAndPreservesPreviousState)
{
	ZoneInfoData info;
	CHECK(LoadInfo(info, InfoBytes()));
	std::ifstream input;
	CHECK(!info.LoadFromFile(input, 291, 564));
	CHECK(input.fail());
	CheckInfo(info);
}

TEST(ZoneInfo, CompleteFileInputExceptionsPreservePreviousState)
{
	ZoneInfoData info;
	CHECK(LoadInfo(info, InfoBytes()));
	auto bytes = InfoBytes();
	bytes.resize(bytes.size() - 1);
	ZoneInfoFile fixture(bytes);
	std::ifstream input(fixture.path, std::ios::binary);
	input.exceptions(std::ios::failbit | std::ios::badbit);
	bool rejected = false;
	try { info.LoadFromFile(input, 291, 564); }
	catch (const std::ios_base::failure&) { rejected = true; }
	CHECK(rejected);
	CheckInfo(info);
}

TEST(ZoneInfo, ZeroDimensionsAreRejectedEvenWhenTheyMatchTheExpectedSize)
{
	ZoneInfoData info;
	CHECK(!LoadInfo(info, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 0, 0));
}

TEST(ZoneInfo, FailedCompleteFileSetsTheStreamFailureState)
{
	ZoneInfoFile fixture({1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0});
	std::ifstream input(fixture.path, std::ios::binary);
	ZoneInfoData info;
	CHECK(!info.LoadFromFile(input, 291, 564));
	CHECK(input.fail());
}

TEST(ZoneInfo, CompleteFileKeepsEachEmptyMultiPortalRectangle)
{
	ZoneInfoData info;
	CHECK(LoadInfo(info, {0x23, 1, 0x34, 2, 2, 0, 0, 0,
		3, 0, 1, 2, 3, 4, 3, 0, 9, 8, 7, 6, 0, 0, 0, 0}));
	CHECK_EQ(2, info.portals.size());
	if (info.portals.size() != 2) return;
	CHECK(info.portals[0].GetZoneID().empty());
	CHECK(info.portals[1].GetZoneID().empty());
	CheckRect(info.portals[0], 1, 2, 3, 4);
	CheckRect(info.portals[1], 9, 8, 7, 6);
}

TEST(ZoneInfo, CompleteFileRecordBudgetsAreInclusiveForBothTables)
{
	Bytes bytes{0x23, 1, 0x34, 2};
	AppendCount(bytes, ZoneInfoData::MaxRecords);
	for (std::uint32_t i = 0; i < ZoneInfoData::MaxRecords; ++i)
		bytes.insert(bytes.end(), {3, 0, 1, 2, 3, 4});
	AppendCount(bytes, ZoneInfoData::MaxRecords);
	for (std::uint32_t i = 0; i < ZoneInfoData::MaxRecords; ++i)
		bytes.insert(bytes.end(), {0x10, 9, 8, 7, 6});
	ZoneInfoData info;
	CHECK(LoadInfo(info, bytes));
	CHECK_EQ(ZoneInfoData::MaxRecords, info.portals.size());
	CHECK_EQ(ZoneInfoData::MaxRecords, info.safetyZones.size());
	if (info.portals.empty() || info.safetyZones.empty()) return;
	CheckRect(info.portals.back(), 1, 2, 3, 4);
	CHECK_EQ(0x10, info.safetyZones.back().flag);
	CHECK_EQ(6, info.safetyZones.back().bottom);
}

TEST(ZoneInfo, HugeCompleteFileCountsFailBeforeReadingAnyRecord)
{
	for (std::uint32_t count : {ZoneInfoData::MaxRecords + 1, 0x7fffffffu, 0x80000000u, 0xffffffffu})
		for (bool safety : {false, true})
		{
			Bytes bytes{0x23, 1, 0x34, 2};
			if (safety) AppendCount(bytes, 0);
			AppendCount(bytes, count);
			bytes.resize(bytes.size() + 10, 0);
			ZoneInfoFile fixture(bytes);
			std::ifstream input(fixture.path, std::ios::binary);
			ZoneInfoData info;
			CHECK(LoadInfo(info, InfoBytes()));
			CHECK(!info.LoadFromFile(input, 291, 564));
			CHECK(input.fail());
			input.clear();
			CHECK_EQ(safety ? 12 : 8, input.tellg());
			CheckInfo(info);
		}
}

TEST(ZoneInfo, InBudgetCountsMustFitTheBytesRemainingInTheFile)
{
	for (bool safety : {false, true})
	{
		Bytes bytes{0x23, 1, 0x34, 2};
		if (safety) AppendCount(bytes, 0);
		AppendCount(bytes, 2);
		if (safety) bytes.insert(bytes.end(), {1, 2, 3, 4, 5});
		else bytes.insert(bytes.end(), {3, 0, 1, 2, 3, 4, 0, 0, 0, 0});
		ZoneInfoFile fixture(bytes);
		std::ifstream input(fixture.path, std::ios::binary);
		ZoneInfoData info;
		CHECK(!info.LoadFromFile(input, 291, 564));
		CHECK(input.fail());
		input.clear();
		CHECK_EQ(safety ? 12 : 8, input.tellg());
		CHECK(info.portals.empty()); CHECK(info.safetyZones.empty());
	}
}

TEST(ZoneInfo, CompleteFileByteBudgetIncludesTheTrailerFromTheStartingPosition)
{
	for (std::size_t excess : {0, 1})
	{
		auto bytes = InfoBytes();
		bytes.insert(bytes.begin(), 0xee);
		ZoneInfoFile fixture(bytes);
		{
			std::ofstream output(fixture.path, std::ios::binary | std::ios::in);
			output.seekp(static_cast<std::streamoff>(ZoneInfoData::MaxFileBytes + excess));
			output.put(0);
			CHECK(output.good());
		}
		std::ifstream input(fixture.path, std::ios::binary);
		input.seekg(1);
		ZoneInfoData info;
		CHECK(LoadInfo(info, InfoBytes()));
		CHECK_EQ(excess == 0, info.LoadFromFile(input, 291, 564));
		CHECK_EQ(excess != 0, input.fail());
		input.clear();
		CHECK_EQ(excess == 0 ? bytes.size() : 1, input.tellg());
		CheckInfo(info);
	}
}

TEST(ZoneInfo, OutOfRangeExpectedDimensionsCannotMatchByNarrowing)
{
	for (int width : {-65245, 65827})
	{
		ZoneInfoData info;
		CHECK(!LoadInfo(info, InfoBytes(), width, 564));
		CHECK_EQ(0, info.width); CHECK_EQ(0, info.height);
	}
}
