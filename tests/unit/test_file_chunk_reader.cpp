//----------------------------------------------------------------------
// test_file_chunk_reader.cpp
//----------------------------------------------------------------------
//
// basic/FileChunkReader - the file SendFileInfo sends a peer a chunk at a
// time. When the socket takes only part of a chunk, the send loop hands
// the rest back with Unread and the next Read must return those same
// bytes: mid-file, and after the short last chunk that ends the file.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "FileChunkReader.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

// A file of `size` bytes whose every byte says where it is.
struct PatternFile
{
	std::string path;
	std::vector<char> bytes;

	explicit PatternFile(size_t size)
	{
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		path = (std::filesystem::temp_directory_path()
			/ ("darkeden_file_chunk_reader_" + std::to_string(stamp) + ".bin")).string();
		for (size_t i = 0; i < size; ++i)
			bytes.push_back(static_cast<char>((i * 7 + 3) & 0xFF));
		std::ofstream out(path.c_str(), std::ios::out | std::ios::binary);
		out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	}

	~PatternFile()
	{
		std::remove(path.c_str());
	}

	bool Matches(const char* pRead, size_t offset, size_t count) const
	{
		return offset + count <= bytes.size()
			&& std::equal(pRead, pRead + count, bytes.begin() + static_cast<std::ptrdiff_t>(offset));
	}
};

}

TEST(FileChunkReader, OpenTakesTheFileSizeAsBytesLeft)
{
	PatternFile file(10000);
	Basic::FileChunkReader reader;
	reader.Open(file.path);
	CHECK(reader.IsOpen());
	CHECK_EQ(10000u, reader.GetBytesLeft());
	reader.Close();
}

TEST(FileChunkReader, AFileThatDoesNotOpenLeavesNothing)
{
	Basic::FileChunkReader reader;
	reader.Open((std::filesystem::temp_directory_path() / "darkeden_file_chunk_reader_missing.bin").string());
	CHECK(!reader.IsOpen());
	CHECK_EQ(0u, reader.GetBytesLeft());
}

TEST(FileChunkReader, ReadReturnsChunksInOrderAndCountsDown)
{
	PatternFile file(10000);
	Basic::FileChunkReader reader;
	reader.Open(file.path);
	std::vector<char> buffer(4096);

	CHECK_EQ(4096u, reader.Read(buffer.data(), 4096));
	CHECK(file.Matches(buffer.data(), 0, 4096));
	CHECK_EQ(10000u - 4096u, reader.GetBytesLeft());

	CHECK_EQ(4096u, reader.Read(buffer.data(), 4096));
	CHECK(file.Matches(buffer.data(), 4096, 4096));

	CHECK_EQ(10000u - 8192u, reader.Read(buffer.data(), 4096));
	CHECK(file.Matches(buffer.data(), 8192, 10000 - 8192));
	CHECK_EQ(0u, reader.GetBytesLeft());
	reader.Close();
}

TEST(FileChunkReader, UnreadMidFileReturnsTheSameBytesAgain)
{
	PatternFile file(10000);
	Basic::FileChunkReader reader;
	reader.Open(file.path);
	std::vector<char> buffer(4096);

	CHECK_EQ(4096u, reader.Read(buffer.data(), 4096));
	reader.Unread(100);
	CHECK_EQ(10000u - 4096u + 100u, reader.GetBytesLeft());

	CHECK_EQ(4096u, reader.Read(buffer.data(), 4096));
	CHECK(file.Matches(buffer.data(), 4096 - 100, 4096));
	reader.Close();
}

TEST(FileChunkReader, UnreadAfterTheShortLastChunkReturnsItsTail)
{
	PatternFile file(5000);
	Basic::FileChunkReader reader;
	reader.Open(file.path);
	std::vector<char> buffer(4096);

	CHECK_EQ(4096u, reader.Read(buffer.data(), 4096));
	CHECK_EQ(5000u - 4096u, reader.Read(buffer.data(), 4096));
	CHECK_EQ(0u, reader.GetBytesLeft());

	reader.Unread(300);
	CHECK_EQ(300u, reader.GetBytesLeft());

	CHECK_EQ(300u, reader.Read(buffer.data(), 4096));
	CHECK(file.Matches(buffer.data(), 5000 - 300, 300));
	CHECK_EQ(0u, reader.GetBytesLeft());
	reader.Close();
}
