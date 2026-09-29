//----------------------------------------------------------------------
// test_crt_compat.cpp
//----------------------------------------------------------------------
//
// basic/CrtCompat.h replaces C runtime calls MSVC deprecates with calls
// that do the same thing. These tests pin that sameness on every
// platform: the scanners parse like sscanf/fscanf (CRT_BUFFER supplies
// the capacity MSVC's _s scanners need for %s), OpenFile reads and
// writes like fopen, Tokenize splits like strtok, LocalTime agrees with
// localtime and DuplicateString copies like strdup.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "CrtCompat.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <limits>
#include <string>

TEST(CrtCompat, ScanStringParsesNumbersAndWords)
{
	int first = 0, second = 0;
	char word[16] = {};
	CHECK_EQ(3, Basic::ScanString("12 34 name", "%d %d %15s", &first, &second, CRT_BUFFER(word)));
	CHECK_EQ(12, first);
	CHECK_EQ(34, second);
	CHECK(std::string("name") == std::string(word));
	CHECK_EQ(0, Basic::ScanString("x", "%d", &first));
}

// CRT_BUFFER's halves take only a char array (a pointer does not
// compile): the array itself, and its length, which the Windows scanners
// receive as the capacity.
TEST(CrtCompat, CrtBufferPassesTheArrayAndItsLength)
{
	char word[16] = {};
	char ignore[256] = {};
	CHECK(&Basic::CrtArray(word) == &word);
	CHECK_EQ(16u, Basic::CrtCapacity(word));
	CHECK_EQ(256u, Basic::CrtCapacity(ignore));
	CHECK_EQ(static_cast<unsigned>(sizeof(ignore)), Basic::CrtCapacity(ignore));
}

TEST(CrtCompat, OpenFileWritesAndScanFileReadsBack)
{
	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	const auto path = (std::filesystem::temp_directory_path()
		/ ("darkeden_crt_compat_" + std::to_string(stamp) + ".txt")).string();

	std::FILE* out = Basic::OpenFile(path.c_str(), "w");
	CHECK(out != nullptr);
	if (out == nullptr)
		return;
	std::fputs("7 Volume\n", out);
	std::fclose(out);

	std::FILE* in = Basic::OpenFile(path.c_str(), "r");
	CHECK(in != nullptr);
	if (in != nullptr)
	{
		int value = 0;
		char name[32] = {};
		CHECK_EQ(2, Basic::ScanFile(in, "%d %s\n", &value, CRT_BUFFER(name)));
		CHECK_EQ(7, value);
		CHECK(std::string("Volume") == std::string(name));
		std::fclose(in);
	}
	std::remove(path.c_str());

	CHECK(Basic::OpenFile((path + ".missing").c_str(), "r") == nullptr);
}

TEST(CrtCompat, TokenizeSplitsLikeStrtok)
{
	char text[] = "a|bb||ccc";
	char* context = nullptr;
	std::string parts;
	for (char* token = Basic::Tokenize(text, "|", &context); token != nullptr;
		 token = Basic::Tokenize(nullptr, "|", &context))
		parts += std::string(token) + ";";
	CHECK(std::string("a;bb;ccc;") == parts);
}

TEST(CrtCompat, LocalTimeAgreesWithTheCalendar)
{
	const std::time_t now = std::time(nullptr);
	std::tm result = {};
	CHECK(Basic::LocalTime(&now, &result));
	CHECK(result.tm_mon >= 0 && result.tm_mon <= 11);
	CHECK(result.tm_mday >= 1 && result.tm_mday <= 31);
	CHECK(result.tm_year >= 100);
}

// A time the runtime cannot convert leaves a zeroed tm, not the -1s
// localtime_s writes or whatever localtime_r left.
TEST(CrtCompat, LocalTimeZeroesTheResultWhenItCannotConvert)
{
	const std::time_t never = std::numeric_limits<std::time_t>::max();
	std::tm result;
	std::memset(&result, 0x7f, sizeof(result));
	if (!Basic::LocalTime(&never, &result))
	{
		CHECK_EQ(0, result.tm_year);
		CHECK_EQ(0, result.tm_mon);
		CHECK_EQ(0, result.tm_mday);
		CHECK_EQ(0, result.tm_hour);
		CHECK_EQ(0, result.tm_min);
		CHECK_EQ(0, result.tm_sec);
		CHECK_EQ(0, result.tm_isdst);
	}
}

TEST(CrtCompat, DuplicateStringCopies)
{
	const char* original = "SpritePack.spk";
	char* copy = Basic::DuplicateString(original);
	CHECK(copy != nullptr);
	CHECK(copy != original);
	CHECK(std::string(original) == std::string(copy));
	std::free(copy);
}

TEST(CrtCompat, CopyBoundedMatchesStrncpy)
{
	// Shorter source: copied, then the rest of the count filled with NULs.
	char padded[8];
	std::memset(padded, 'x', sizeof(padded));
	Basic::CopyBounded(padded, "ab", 5);
	CHECK(std::memcmp(padded, "ab\0\0\0xxx", 8) == 0);

	// Longer source: exactly count characters, and no terminator added.
	char exact[6];
	std::memset(exact, 'x', sizeof(exact));
	Basic::CopyBounded(exact, "abcdef", 4);
	CHECK(std::memcmp(exact, "abcdxx", 6) == 0);

	// A zero count writes nothing.
	char untouched[2] = { 'q', 'r' };
	Basic::CopyBounded(untouched, "abc", 0);
	CHECK(untouched[0] == 'q' && untouched[1] == 'r');
}

TEST(CrtCompat, TimeTextHasCtimesShape)
{
	const std::string text = Basic::TimeText(std::time(nullptr));
	CHECK_EQ(25, static_cast<int>(text.size()));
	CHECK(!text.empty() && text.back() == '\n');
}

TEST(CrtCompat, GetEnvironmentReadsSetAndUnsetVariables)
{
	CHECK(!Basic::GetEnvironment("DARKEDEN_CRT_COMPAT_SURELY_UNSET_VARIABLE").has_value());
	const auto path = Basic::GetEnvironment("PATH");
#ifdef _WIN32
	(void)path;
#else
	CHECK(path.has_value() && !path->empty());
#endif
}

TEST(CrtCompat, TemporaryFileIsReadableAndWritable)
{
	std::FILE* file = Basic::TemporaryFile();
	CHECK(file != nullptr);
	if (file == nullptr)
		return;
	std::fputs("42", file);
	std::rewind(file);
	int value = 0;
	CHECK_EQ(1, Basic::ScanFile(file, "%d", &value));
	CHECK_EQ(42, value);
	std::fclose(file);
}
