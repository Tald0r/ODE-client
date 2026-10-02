#include "test_framework.h"
#include "MusicSelection.h"
#include "MMusicTable.h"
#include "TextEncoding.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<unsigned char>;

struct Track { int zone; MUSIC_ID music; };
constexpr Track castleTracks[]{
	{1201, MUSIC_OCTAVUS}, {1211, MUSIC_OCTAVUS}, {1212, MUSIC_OCTAVUS},
	{1202, MUSIC_TERTIUS}, {1221, MUSIC_TERTIUS}, {1222, MUSIC_TERTIUS},
	{1203, MUSIC_SEPTIMUS}, {1231, MUSIC_SEPTIMUS}, {1232, MUSIC_SEPTIMUS},
	{1204, MUSIC_QUARTUS}, {1241, MUSIC_QUARTUS}, {1242, MUSIC_QUARTUS},
	{1205, MUSIC_OCTAVUS}, {1251, MUSIC_OCTAVUS}, {1252, MUSIC_OCTAVUS},
	{1206, MUSIC_TERTIUS}, {1261, MUSIC_TERTIUS}, {1262, MUSIC_TERTIUS},
};

struct Encoding
{
	TextEncoding::Encoding previous = TextEncoding::GetResourceEncoding();
	explicit Encoding(TextEncoding::Encoding value = TextEncoding::Encoding::Utf8)
	{
		TextEncoding::SetResourceEncoding(value);
	}
	~Encoding() { TextEncoding::SetResourceEncoding(previous); }
};

struct MusicFile
{
	std::filesystem::path path;
	explicit MusicFile(const Bytes& bytes)
	{
		static unsigned int sequence = 0;
		path = std::filesystem::temp_directory_path() / ("darkeden_music_"
			+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
			+ "_" + std::to_string(sequence++) + ".bin");
		std::ofstream output(path, std::ios::binary);
		if (!bytes.empty()) output.write(reinterpret_cast<const char*>(bytes.data()),
			static_cast<std::streamsize>(bytes.size()));
		output.close();
		if (!output.good()) throw std::runtime_error("Cannot write music fixture");
	}
	~MusicFile() { std::error_code error; std::filesystem::remove(path, error); }
};

void Number(Bytes& bytes, unsigned value)
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		bytes.push_back(static_cast<unsigned char>(value >> shift));
}

void Text(Bytes& bytes, const std::string& value)
{
	Number(bytes, static_cast<unsigned>(value.size()));
	bytes.insert(bytes.end(), value.begin(), value.end());
}

template<class T> bool Load(T& table, const Bytes& bytes)
{
	MusicFile fixture(bytes);
	std::ifstream input(fixture.path, std::ios::binary);
	table.LoadFromFile(input);
	if (input.good()) CHECK_EQ(bytes.size(), input.tellg());
	return input.good();
}

template<class T> Bytes Save(T& table)
{
	MusicFile fixture({});
	{
		std::ofstream output(fixture.path, std::ios::binary);
		table.SaveToFile(output);
		CHECK(output.good());
	}
	std::ifstream input(fixture.path, std::ios::binary);
	return {std::istreambuf_iterator<char>(input), {}};
}

bool Is(const MString& text, const char* expected)
{
	return text.GetString() != nullptr && std::strcmp(text.GetString(), expected) == 0;
}
}

TEST(MusicSelection, TwoHourRotationRepeatsAcrossEveryHourByte)
{
	constexpr MUSIC_ID expected[]{MUSIC_LIVE_OR_DEAD, MUSIC_WINDMILL, MUSIC_WINDMILL,
		MUSIC_TREASURE, MUSIC_MARCHING, MUSIC_WHISPER, MUSIC_HELL_KNIGHT,
		MUSIC_LUNATIC, MUSIC_SAY_AGAIN, MUSIC_HIDE_AWAY, MUSIC_HELL_KNIGHT, MUSIC_HOLLOWEEN};
	for (int hour = 0; hour <= 255; ++hour)
		CHECK_EQ(expected[(hour % 24) / 2], SelectZoneMusic(61, static_cast<std::uint8_t>(hour), false, false, false));
}

TEST(MusicSelection, EveryCastleAndItsTwoSubzonesKeepTheirAssignedTrack)
{
	for (const auto& track : castleTracks)
		for (bool war : {false, true})
			for (std::uint8_t hour : {0, 23, 255})
				CHECK_EQ(track.music, SelectZoneMusic(track.zone, hour, true, true, war));
}

TEST(MusicSelection, CastleTracksRequireBothHolyLandAndWaveMusic)
{
	for (bool holyLand : {false, true})
		for (bool waveMusic : {false, true})
			CHECK_EQ(holyLand && waveMusic ? MUSIC_OCTAVUS : MUSIC_TREASURE,
				SelectZoneMusic(1201, 7, holyLand, waveMusic, true));
}

TEST(MusicSelection, OtherHolyLandZonesUseWarStateRegardlessOfHour)
{
	for (int hour = 0; hour <= 255; ++hour)
	{
		CHECK_EQ(MUSIC_HOLYLAND, SelectZoneMusic(1200, static_cast<std::uint8_t>(hour), true, true, false));
		CHECK_EQ(MUSIC_HOLYLAND_WAR, SelectZoneMusic(1200, static_cast<std::uint8_t>(hour), true, true, true));
	}
}

TEST(MusicSelection, WarDoesNotChangeTheOrdinaryRotation)
{
	for (int hour = 0; hour <= 255; ++hour)
		for (bool waveMusic : {false, true})
			CHECK_EQ(SelectZoneMusic(61, static_cast<std::uint8_t>(hour), false, waveMusic, false),
				SelectZoneMusic(61, static_cast<std::uint8_t>(hour), false, waveMusic, true));
}

TEST(MusicSelection, WaveMusicDisabledUsesTimeEvenInHolyLandWar)
{
	for (int hour = 0; hour <= 255; ++hour)
		CHECK_EQ(SelectZoneMusic(1200, static_cast<std::uint8_t>(hour), false, false, false),
			SelectZoneMusic(1200, static_cast<std::uint8_t>(hour), true, false, true));
}

TEST(MusicSelection, LairTracksOverrideEveryTimeAndHolyLandCombination)
{
	for (int zone : {1410, 1411, 1412, 1413})
		for (int hour = 0; hour <= 255; ++hour)
			for (bool holyLand : {false, true})
				for (bool waveMusic : {false, true})
					for (bool war : {false, true})
						CHECK_EQ(zone <= 1411 ? MUSIC_ILLUSIONS_WAY : MUSIC_GDR_LAIR,
							SelectZoneMusic(zone, static_cast<std::uint8_t>(hour), holyLand, waveMusic, war));
}

TEST(MusicSelection, AdjacentAndOutOfRangeZoneIdsDoNotAliasSpecialTracks)
{
	for (int zone : {-1, 0, 1207, 1210, 1213, 1260, 1263, 1409, 1414, 65535, 66737, 66946})
	{
		CHECK_EQ(MUSIC_WHISPER, SelectZoneMusic(zone, 10, false, true, true));
		CHECK_EQ(MUSIC_HOLYLAND_WAR, SelectZoneMusic(zone, 10, true, true, true));
	}
}

TEST(MusicMetadata, DefaultRecordHasTwoEmptyPaths)
{
	MUSICTABLE_INFO info;
	CHECK_EQ(0, info.Filename.GetLength());
	CHECK_EQ(0, info.FilenameWav.GetLength());
	CHECK(info.Filename.GetString() == nullptr);
	CHECK(info.FilenameWav.GetString() == nullptr);
}

TEST(MusicMetadata, RecordReadsAndWritesTheTwoPathsInWireOrder)
{
	Encoding encoding;
	const Bytes bytes{5, 0, 0, 0, 'a', '.', 'm', 'i', 'd',
		5, 0, 0, 0, 'b', '.', 'w', 'a', 'v'};
	MUSICTABLE_INFO info;
	CHECK(Load(info, bytes));
	CHECK(Is(info.Filename, "a.mid"));
	CHECK(Is(info.FilenameWav, "b.wav"));
	CHECK(Save(info) == bytes);
}

TEST(MusicMetadata, RecordPathsOwnTheirCopies)
{
	MUSICTABLE_INFO original;
	original.Filename = "Music/first.mid";
	original.FilenameWav = "Sound/first.ogg";
	MUSICTABLE_INFO copied = original;
	MUSICTABLE_INFO assigned;
	assigned = original;
	original.Filename = "changed.mid";
	original.FilenameWav.Release();
	CHECK(Is(copied.Filename, "Music/first.mid"));
	CHECK(Is(copied.FilenameWav, "Sound/first.ogg"));
	CHECK(Is(assigned.Filename, "Music/first.mid"));
	CHECK(Is(assigned.FilenameWav, "Sound/first.ogg"));
}

TEST(MusicMetadata, RecordStreamsLeaveTheNextRecordAndTrailer)
{
	Encoding encoding;
	MusicFile fixture({0xee, 1, 0, 0, 0, 'a', 0, 0, 0, 0,
		0, 0, 0, 0, 1, 0, 0, 0, 'b', 0xdd});
	std::ifstream input(fixture.path, std::ios::binary);
	input.seekg(1);
	MUSICTABLE_INFO first, second;
	first.LoadFromFile(input);
	CHECK_EQ(10, input.tellg());
	second.LoadFromFile(input);
	CHECK_EQ(19, input.tellg());
	CHECK(Is(first.Filename, "a"));
	CHECK_EQ(0, first.FilenameWav.GetLength());
	CHECK_EQ(0, second.Filename.GetLength());
	CHECK(Is(second.FilenameWav, "b"));
	CHECK_EQ(0xdd, input.get());
}

TEST(MusicMetadata, TableLoadsOrderedRowsAndProvidesImmutableMissingEntries)
{
	Encoding encoding;
	Bytes bytes;
	Number(bytes, 2);
	Text(bytes, "first.mid"); Text(bytes, "first.wav");
	Text(bytes, "second.mid"); Text(bytes, "");
	MUSIC_TABLE table;
	CHECK(Load(table, bytes));
	CHECK_EQ(2, table.GetSize());
	CHECK(Is(table[0].Filename, "first.mid"));
	CHECK(Is(table[0].FilenameWav, "first.wav"));
	CHECK(Is(table[1].Filename, "second.mid"));
	CHECK_EQ(0, table[1].FilenameWav.GetLength());
	CHECK(table.GetMutable(-1) == nullptr);
	CHECK(table.GetMutable(2) == nullptr);
	CHECK(table[-1].Filename.GetString() == nullptr);
	CHECK(table[2].FilenameWav.GetString() == nullptr);
	CHECK(Save(table) == bytes);
}

TEST(MusicMetadata, EverySelectedTrackCanIndexTheLoadedMusicTable)
{
	Encoding encoding;
	Bytes bytes;
	Number(bytes, MAX_MUSIC);
	for (int i = 0; i < MAX_MUSIC; ++i)
	{
		Text(bytes, "music-" + std::to_string(i) + ".mid");
		Text(bytes, "wave-" + std::to_string(i) + ".ogg");
	}
	MUSIC_TABLE table;
	CHECK(Load(table, bytes));
	for (int zone : {61, 1200, 1201, 1202, 1203, 1204, 1410, 1412})
		for (std::uint8_t hour : {0, 2, 6, 8, 10, 12, 14, 16, 18, 20, 22})
			for (bool war : {false, true})
			{
				const auto id = SelectZoneMusic(zone, hour, zone != 61, true, war);
				CHECK(Is(table[id].Filename, ("music-" + std::to_string(id) + ".mid").c_str()));
				CHECK(Is(table[id].FilenameWav, ("wave-" + std::to_string(id) + ".ogg").c_str()));
			}
}

TEST(MusicMetadata, EmptyFileTableClearsPreviousRows)
{
	Encoding encoding;
	MUSIC_TABLE table;
	table.Init(1);
	table.GetMutable(0)->Filename = "old.mid";
	CHECK(Load(table, {0, 0, 0, 0}));
	CHECK_EQ(0, table.GetSize());
	CHECK(Save(table) == Bytes({0, 0, 0, 0}));
}

TEST(MusicMetadata, Utf8PathsRoundTripThroughTheResourceCodec)
{
	Encoding encoding;
	Bytes bytes;
	Text(bytes, "Music/가.mid"); Text(bytes, "Sound/雪.ogg");
	MUSICTABLE_INFO info;
	CHECK(Load(info, bytes));
	CHECK(Is(info.Filename, "Music/가.mid"));
	CHECK(Is(info.FilenameWav, "Sound/雪.ogg"));
	CHECK(Save(info) == bytes);
}

TEST(MusicMetadata, LegacyKoreanPathsDecodeToUtf8AndSaveInTheirResourceEncoding)
{
	Encoding encoding(TextEncoding::Encoding::Cp949);
	Bytes bytes;
	Text(bytes, "Music/\xb0\xa1.mid"); Text(bytes, "Sound/\xb0\xa1.wav");
	MUSICTABLE_INFO info;
	CHECK(Load(info, bytes));
	CHECK(Is(info.Filename, "Music/가.mid"));
	CHECK(Is(info.FilenameWav, "Sound/가.wav"));
	CHECK(Save(info) == bytes);
}
