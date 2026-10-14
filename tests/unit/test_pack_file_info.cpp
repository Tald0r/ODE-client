#include "test_framework.h"
#include "PackFileManager.h"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// A packed file's record: its ID, name and position in the data file.
// Saved as the ID (4 bytes), the name's length (1 byte) and bytes, and the
// position (4 bytes), and loaded back in that order, the name exactly
// its bytes. A record saves no data of its own.
namespace {

struct TempFile
{
	std::filesystem::path path = std::filesystem::temp_directory_path() / ("darkeden_packinfo_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".inf");
	~TempFile() { std::error_code error; std::filesystem::remove(path, error); }
	std::vector<unsigned char> Bytes() const
	{
		std::ifstream in(path, std::ios::binary);
		return std::vector<unsigned char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	}
};

} // namespace

TEST(PackFileInfo, ARecordHoldsItsIDNameAndPosition)
{
	PackFileInfo info(7, "sound.wav");
	CHECK_EQ(7u, info.GetID());
	CHECK(info.GetFilename() == "sound.wav");
	CHECK_EQ(0, info.GetFilePosition());
	PackFileInfo none;
	CHECK_EQ(0u, none.GetID());
	CHECK(none.GetFilename().empty());
	none.SetID(9);
	none.SetFilename("x.wav");
	none.SetFilePosition(1234);
	CHECK(none.GetID() == 9 && none.GetFilename() == "x.wav" && none.GetFilePosition() == 1234);
	TempFile file;
	std::ofstream out(file.path, std::ios::binary);
	CHECK(none.SaveToFileData(out));
}

TEST(PackFileInfo, ARecordIsSavedAsItsIDNameAndPosition)
{
	PackFileInfo info(0x01020304, "ab");
	info.SetFilePosition(0x0A0B0C0D);
	TempFile file;
	{
		std::ofstream out(file.path, std::ios::binary);
		info.SaveToFile(out);
	}
	CHECK(file.Bytes() == (std::vector<unsigned char>{0x04, 0x03, 0x02, 0x01, 2, 'a', 'b', 0x0D, 0x0C, 0x0B, 0x0A}));
	// No name: no name bytes.
	PackFileInfo unnamed(5);
	{
		std::ofstream out(file.path, std::ios::binary | std::ios::trunc);
		unnamed.SaveToFile(out);
	}
	CHECK(file.Bytes() == (std::vector<unsigned char>{5, 0, 0, 0, 0, 0, 0, 0, 0}));
}

TEST(PackFileInfo, ARecordLoadsItsIDAndPosition)
{
	PackFileInfo saved(77);
	saved.SetFilePosition(4321);
	TempFile file;
	{
		std::ofstream out(file.path, std::ios::binary);
		saved.SaveToFile(out);
		saved.SaveToFile(out);
	}
	std::ifstream in(file.path, std::ios::binary);
	PackFileInfo first, second;
	first.LoadFromFile(in);
	second.LoadFromFile(in);
	CHECK(first.GetID() == 77 && first.GetFilePosition() == 4321 && first.GetFilename().empty());
	CHECK(second.GetID() == 77 && second.GetFilePosition() == 4321);
}

TEST(PackFileInfo, ARecordLoadsExactlyItsNamesBytes)
{
	// A long name, then a shorter one: the second must not keep the
	// first's tail (both load through the same buffer on the stack).
	TempFile file;
	{
		std::ofstream out(file.path, std::ios::binary);
		PackFileInfo(1, "a_long_sound_name.wav").SaveToFile(out);
		PackFileInfo(2, "ab").SaveToFile(out);
		PackFileInfo(3).SaveToFile(out);
	}
	std::ifstream in(file.path, std::ios::binary);
	PackFileInfo first, second, third;
	first.LoadFromFile(in);
	second.LoadFromFile(in);
	third.LoadFromFile(in);
	CHECK(first.GetFilename() == "a_long_sound_name.wav");
	CHECK(second.GetFilename() == "ab");
	CHECK(second.GetID() == 2 && third.GetID() == 3);
	CHECK(third.GetFilename().empty());
}
