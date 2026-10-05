#include "test_framework.h"
#include "MZoneSound.h"
#include "SectorSoundInfo.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
struct SoundMetadataFile
{
	std::filesystem::path path;
	SoundMetadataFile()
	{
		static unsigned serial = 0;
		path = std::filesystem::temp_directory_path() / ("darkeden_sound_metadata_"
			+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
			+ "_" + std::to_string(++serial) + ".bin");
	}
	~SoundMetadataFile() { std::error_code error; std::filesystem::remove(path, error); }
	std::vector<unsigned char> Read() const
	{
		std::ifstream file(path, std::ios::binary);
		return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	}
	void Write(const std::vector<unsigned char>& bytes) const
	{
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		CHECK(file.good());
	}
};
}

TEST(ZoneSoundMetadata, DefaultsIncludeTheInheritedAllDaySchedule)
{
	ZONESOUND_INFO sound;
	CHECK_EQ(0, sound.ID);
	CHECK_EQ(0, sound.SoundID);
	CHECK(!sound.Loop);
	CHECK_EQ(60000, sound.MinDelay);
	CHECK_EQ(60000, sound.MaxDelay);
	CHECK_EQ(0, sound.StartHour);
	CHECK_EQ(24, sound.EndHour);
	CHECK(sound.NextPlayTime == MonotonicClock::TimePoint());
}

TEST(ZoneSoundMetadata, SavesIdentifiersBeforeTheSchedulingFields)
{
	SoundMetadataFile fixture;
	ZONESOUND_INFO sound;
	sound.ID = 0x1234;
	sound.SoundID = 0xabcd;
	sound.Loop = true;
	sound.MinDelay = 0x01020304;
	sound.MaxDelay = 0x05060708;
	sound.StartHour = 22;
	sound.EndHour = 3;
	sound.NextPlayTime = MonotonicClock::FromMillis(9999);
	{
		std::ofstream file(fixture.path, std::ios::binary);
		sound.SaveToFile(file);
		CHECK(file.good());
	}
	const std::vector<unsigned char> expected{
		0x34, 0x12, 0xcd, 0xab, 1, 4, 3, 2, 1, 8, 7, 6, 5, 22, 3};
	CHECK(fixture.Read() == expected);
}

TEST(ZoneSoundMetadata, LoadsAdjacentRecordsWithoutRestoringRuntimeDeadlines)
{
	SoundMetadataFile fixture;
	fixture.Write({0x34, 0x12, 0xcd, 0xab, 1, 4, 3, 2, 1, 8, 7, 6, 5, 22, 3,
		0xff, 0xff, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff, 0, 24});
	std::ifstream file(fixture.path, std::ios::binary);
	ZONESOUND_INFO first;
	first.NextPlayTime = MonotonicClock::FromMillis(1234);
	first.LoadFromFile(file);
	CHECK(file.good());
	CHECK_EQ(15, file.tellg());
	CHECK_EQ(0x1234, first.ID);
	CHECK_EQ(0xabcd, first.SoundID);
	CHECK(first.Loop);
	CHECK_EQ(0x01020304, first.MinDelay);
	CHECK_EQ(0x05060708, first.MaxDelay);
	CHECK_EQ(22, first.StartHour);
	CHECK_EQ(3, first.EndHour);
	CHECK(first.NextPlayTime == MonotonicClock::FromMillis(1234));
	CHECK(first.IsShowHour(23));
	CHECK(!first.IsShowHour(12));
	ZONESOUND_INFO second;
	second.LoadFromFile(file);
	CHECK(file.good());
	CHECK_EQ(30, file.tellg());
	CHECK_EQ(65535, second.ID);
	CHECK_EQ(0, second.SoundID);
	CHECK(!second.Loop);
	CHECK_EQ(0, second.MinDelay);
	CHECK_EQ(0xffffffffu, second.MaxDelay);
	CHECK_EQ(0, second.StartHour);
	CHECK_EQ(24, second.EndHour);
	CHECK(second.NextPlayTime == MonotonicClock::TimePoint());
}

TEST(ZoneSoundMetadata, TruncatedRecordsExposeStreamFailure)
{
	SoundMetadataFile fixture;
	fixture.Write({0x34, 0x12, 0xcd, 0xab, 1, 4, 3, 2, 1, 8, 7, 6, 5, 22});
	std::ifstream file(fixture.path, std::ios::binary);
	ZONESOUND_INFO sound;
	sound.LoadFromFile(file);
	CHECK(file.fail());
	CHECK_EQ(0x1234, sound.ID);
	CHECK_EQ(0xabcd, sound.SoundID);
}

TEST(SectorSoundMetadata, SavesAndLoadsPackedFourByteRecords)
{
	SoundMetadataFile fixture;
	const SECTORSOUND_INFO source(0x1234, 0x56, 0x78);
	const SECTORSOUND_INFO boundary(65535, 255, 0);
	{
		std::ofstream file(fixture.path, std::ios::binary);
		source.SaveToFile(file);
		boundary.SaveToFile(file);
		CHECK(file.good());
	}
	const std::vector<unsigned char> expected{0x34, 0x12, 0x56, 0x78, 0xff, 0xff, 0xff, 0};
	CHECK(fixture.Read() == expected);
	std::ifstream file(fixture.path, std::ios::binary);
	SECTORSOUND_INFO first, second;
	first.LoadFromFile(file);
	CHECK(file.good());
	CHECK_EQ(4, file.tellg());
	second.LoadFromFile(file);
	CHECK(file.good());
	CHECK_EQ(8, file.tellg());
	CHECK(first == source);
	CHECK(second == boundary);
	CHECK(first != second);
	CHECK_EQ(0x1234, first.ZoneSoundID);
	CHECK_EQ(0x56, first.X);
	CHECK_EQ(0x78, first.Y);
}

TEST(SectorSoundMetadata, LoadsTheOnDiskFieldOrderAndSignalsTruncation)
{
	SoundMetadataFile fixture;
	fixture.Write({0xcd, 0xab, 0, 255, 0x34, 0x12});
	std::ifstream file(fixture.path, std::ios::binary);
	SECTORSOUND_INFO sound;
	sound.LoadFromFile(file);
	CHECK(file.good());
	CHECK_EQ(0xabcd, sound.ZoneSoundID);
	CHECK_EQ(0, sound.X);
	CHECK_EQ(255, sound.Y);
	sound.LoadFromFile(file);
	CHECK(file.fail());
	CHECK_EQ(0x1234, sound.ZoneSoundID);
}
