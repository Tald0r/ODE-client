#include "test_framework.h"
#include "CUsingColorSetTable.h"
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// The colour sets a sprite may use: one flag for each of the index
// sprites' MAX_COLORSET colour sets, all off at first. A set, or a range
// of them (the start below the end, both in range), is turned on or off;
// anything out of range is ignored. A node is saved as its count and one
// byte a set, and loads only a file of the same count (skipping another
// count's bytes).
namespace {

struct Probe : USINGCOLORSET_NODE
{
	bool Is(int set) const { return m_bEnable[set]; }
	std::vector<int> Enabled() const
	{
		std::vector<int> sets;
		for (int i = 0; i < MAX_COLORSET; i++)
			if (m_bEnable[i]) sets.push_back(i);
		return sets;
	}
};

struct TempFile
{
	std::filesystem::path path = std::filesystem::temp_directory_path() / ("darkeden_colorset_" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".inf");
	~TempFile() { std::error_code error; std::filesystem::remove(path, error); }
	std::vector<unsigned char> Bytes() const
	{
		std::ifstream in(path, std::ios::binary);
		return std::vector<unsigned char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	}
	void Write(const std::vector<unsigned char>& bytes) const
	{
		std::ofstream out(path, std::ios::binary | std::ios::trunc);
		out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}
};

std::vector<unsigned char> Count(int count)
{
	return {static_cast<unsigned char>(count & 0xFF), static_cast<unsigned char>((count >> 8) & 0xFF),
		static_cast<unsigned char>((count >> 16) & 0xFF), static_cast<unsigned char>((count >> 24) & 0xFF)};
}

} // namespace

TEST(UsingColorSetTable, ANodeStartsWithNoSetUsable)
{
	Probe node;
	CHECK(node.Enabled().empty());
}

TEST(UsingColorSetTable, OneSetTurnsOnAndOffInRangeOnly)
{
	Probe node;
	// Out of range: nothing, not even another set.
	node.Enable(-1);
	node.Enable(MAX_COLORSET);
	CHECK(node.Enabled().empty());
	node.Enable(0);
	node.Enable(MAX_COLORSET - 1);
	CHECK(node.Enabled() == (std::vector<int>{0, MAX_COLORSET - 1}));
	node.Disable(0);
	node.Disable(-1);
	node.Disable(MAX_COLORSET);
	CHECK(node.Enabled() == std::vector<int>{MAX_COLORSET - 1});
}

TEST(UsingColorSetTable, ARangeTurnsOnAndOffWhenItsStartIsBelowItsEnd)
{
	Probe node;
	node.Enable(3, 6);
	CHECK(node.Enabled() == (std::vector<int>{3, 4, 5, 6}));
	// A start not below its end, or either end out of range: nothing.
	node.Enable(10, 10);
	node.Enable(12, 11);
	node.Enable(-1, 2);
	node.Enable(MAX_COLORSET - 2, MAX_COLORSET);
	CHECK(node.Enabled() == (std::vector<int>{3, 4, 5, 6}));
	node.Enable(MAX_COLORSET - 2, MAX_COLORSET - 1);
	node.Disable(4, 5);
	node.Disable(6, 6);
	node.Disable(-1, 3);
	node.Disable(MAX_COLORSET - 2, MAX_COLORSET);
	CHECK(node.Enabled() == (std::vector<int>{3, 6, MAX_COLORSET - 2, MAX_COLORSET - 1}));
}

TEST(UsingColorSetTable, ANodeIsSavedAsItsCountAndOneByteASet)
{
	Probe node;
	node.Enable(0);
	node.Enable(7);
	TempFile file;
	{
		std::ofstream out(file.path, std::ios::binary);
		node.SaveToFile(out);
	}
	const std::vector<unsigned char> bytes = file.Bytes();
	CHECK_EQ(4u + MAX_COLORSET, bytes.size());
	CHECK(std::vector<unsigned char>(bytes.begin(), bytes.begin() + 4) == Count(MAX_COLORSET));
	// Each set's own flag.
	for (int i = 0; i < MAX_COLORSET; i++) CHECK_EQ(i == 0 || i == 7 ? 1 : 0, bytes[4 + i]);
	// And it loads back as it was.
	Probe loaded;
	{
		std::ifstream in(file.path, std::ios::binary);
		CHECK(loaded.LoadFromFile(in));
	}
	CHECK(loaded.Enabled() == (std::vector<int>{0, 7}));
}

TEST(UsingColorSetTable, ANodeLoadsOnlyItsOwnCount)
{
	TempFile file;
	std::vector<unsigned char> bytes = Count(MAX_COLORSET);
	for (int i = 0; i < MAX_COLORSET; i++) bytes.push_back(i % 3 == 0 ? 1 : 0);
	bytes.push_back(9);
	file.Write(bytes);
	Probe node;
	{
		std::ifstream in(file.path, std::ios::binary);
		CHECK(node.LoadFromFile(in));
		char next = 0;
		in.read(&next, 1);
		CHECK_EQ(9, next);
	}
	for (int i = 0; i < MAX_COLORSET; i++) CHECK(node.Is(i) == (i % 3 == 0));
	// Another count: its bytes are skipped, the node is left as it was.
	std::vector<unsigned char> other = Count(3);
	other.insert(other.end(), {0, 1, 1, 7});
	file.Write(other);
	{
		std::ifstream in(file.path, std::ios::binary);
		CHECK(!node.LoadFromFile(in));
		char next = 0;
		in.read(&next, 1);
		CHECK_EQ(7, next);
	}
	for (int i = 0; i < MAX_COLORSET; i++) CHECK(node.Is(i) == (i % 3 == 0));
}

TEST(UsingColorSetTable, AFileCutShortBeforeItsCountLoadsNothing)
{
	// Half of a count of 495: read without the check, it would be taken
	// for a whole one.
	TempFile file;
	file.Write({0xEF, 0x01});
	Probe node;
	node.Enable(5);
	std::ifstream in(file.path, std::ios::binary);
	CHECK(!node.LoadFromFile(in));
	CHECK(node.Enabled() == std::vector<int>{5});
}
