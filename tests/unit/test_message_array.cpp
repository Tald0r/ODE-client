#include "test_framework.h"
#include "CMessageArray.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#ifndef _WIN32
#include <sys/stat.h>
#endif

namespace {
template <typename... Args>
void AddV(CMessageArray& messages, const char* format, Args... values)
{
	const SafeFormat::Arg args[] = {SafeFormat::MakeArg(values)..., SafeFormat::Arg()};
	messages.AddSafeFormatV(format, args, sizeof...(values));
}

constexpr const char* path = "message_ring_test.log";
struct FileFixture {
	FileFixture() { std::remove(path); }
	~FileFixture() { std::remove(path); }
};
}

TEST(MessageArray, RealRingLinksFromBasicAndRetainsNewestRowsInOrder)
{
	CMessageArray messages;
	messages.Init(3, 12);
	messages.Add("first");
	messages.AddSafeFormat("row %d", 2);
	messages.AddSafeFormat("row %d", 3);
	messages.Add("last");
	CHECK_EQ(3, messages.GetSize());
	CHECK(std::string(messages[0]) == "row 2");
	CHECK(std::string(messages[1]) == "row 3");
	CHECK(std::string(messages[2]) == "last");
}

TEST(MessageArray, RingsAbove255RowsKeepTheirCompleteOrdering)
{
	CMessageArray messages;
	messages.Init(300, 16);
	for (int i = 0; i < 650; ++i) messages.AddSafeFormat("row %d", i);
	for (int i = 0; i < 300; ++i)
		CHECK(std::string(messages[i]) == "row " + std::to_string(350 + i));
}

TEST(MessageArray, EndIndexIsRejectedInsteadOfReturningTheOldestRow)
{
	CMessageArray messages;
	messages.Init(3, 12);
	messages.Add("first"); messages.Add("second"); messages.Add("third");
	CHECK(std::string(messages[3]).empty());
	CHECK(std::string(messages[-1]).empty());
	CHECK(std::string(messages[(std::numeric_limits<int>::min)()]).empty());
	CHECK(std::string(messages[(std::numeric_limits<int>::max)()]).empty());
}

TEST(MessageArray, EmptyAndReleasedRingsAcceptNoWritesOrInvalidReads)
{
	CMessageArray messages;
	for (int state = 0; state < 2; ++state) {
		CHECK_EQ(0, messages.GetSize());
		CHECK(messages.GetCurrent() == nullptr);
		CHECK(std::string(messages[0]).empty());
		messages.Add("ignored"); messages.Add(nullptr);
		messages.AddSafeFormat("row %d", 4); messages.AddSafeFormat(nullptr);
		AddV(messages, "row %d", 5); AddV(messages, nullptr);
		messages.AddSafeFormat("row %d", 6);
		messages.AddToFile("ignored"); messages.AddToFile(nullptr);
		messages.Clear(); messages.Next(); messages.Release(); messages.Release();
		messages.Init(2, 12); messages.Add("temporary"); messages.Release();
	}
}

TEST(MessageArray, InvalidDimensionsLeaveAnEmptyUsableObject)
{
	CMessageArray messages;
	for (const auto& dimensions : {std::pair{0, 4}, std::pair{-1, 4}, std::pair{3, -1}}) {
		messages.Init(2, 8); messages.Add("previous");
		messages.Init(dimensions.first, dimensions.second);
		CHECK_EQ(0, messages.GetSize());
		CHECK(messages.GetCurrent() == nullptr);
		messages.Add("ignored"); messages.Clear();
	}
	messages.Init(2, 0);
	messages.Add("zero length"); AddV(messages, "%d", 25);
	CHECK_EQ(2, messages.GetSize());
	CHECK(std::string(messages[0]).empty()); CHECK(std::string(messages[1]).empty());
}

TEST(MessageArray, EveryWritePathTruncatesAndClearKeepsTheRingUsable)
{
	CMessageArray messages;
	messages.Init(4, 4);
	messages.Add("abcdef");
	messages.AddSafeFormat("%s", "ghijkl");
	AddV(messages, "%s", "mnopqr");
	messages.AddSafeFormat("%s", "stuvwx");
	CHECK(std::string(messages[0]) == "abcd");
	CHECK(std::string(messages[1]) == "ghij");
	CHECK(std::string(messages[2]) == "mnop");
	CHECK(std::string(messages[3]) == "stuv");
	messages.Add(messages.GetCurrent());
	CHECK(std::string(messages[3]) == "abcd");
	messages.Clear();
	for (int i = 0; i < 4; ++i) CHECK(std::string(messages[i]).empty());
	messages.Add("again");
	CHECK(std::string(messages[3]) == "agai");
}

TEST(MessageArray, LogLifetimeSupportsReinitializationAndAnAliasedFilename)
{
	FileFixture fixture;
	CMessageArray messages;
	messages.Init(2, 4, path);
	messages.Add("old");
	messages.Init(2, 4, messages.GetFilename());
	CHECK(std::string(messages.GetFilename()) == path);
	messages.Add("abcdef"); messages.AddToFile("file only");
	CHECK(std::string(messages[1]) == "abcd");
#ifndef _WIN32
	struct stat info{};
	CHECK_EQ(0, stat(path, &info));
	CHECK_EQ(0, info.st_mode & (S_IRWXG | S_IRWXO | S_ISUID | S_ISGID));
#endif
	messages.Init(1, 8);
	CHECK(messages.GetFilename() == nullptr);
	messages.Add("no log");
	messages.Release(); messages.Release();
	CHECK(messages.GetFilename() == nullptr);
	std::ifstream in(path);
	std::string line;
	CHECK(bool(std::getline(in, line))); CHECK(line == "abcdef");
	CHECK(bool(std::getline(in, line))); CHECK(line == "file only");
	CHECK(!std::getline(in, line));
}

TEST(MessageArray, AFailedLogOpenStillReleasesItsFilename)
{
	FileFixture fixture;
	{ std::ofstream out(path); out << "parent is a file"; }
	const std::string impossible = std::string(path) + "/child";
	CMessageArray messages;
	messages.Init(2, 8, impossible.c_str());
	CHECK(messages.GetFilename() != nullptr);
	messages.Add("retained");
	CHECK(std::string(messages[1]) == "retained");
	messages.Release(); messages.Release();
	CHECK(messages.GetFilename() == nullptr);
	messages.Init(2, 8); messages.Add("new");
	CHECK(std::string(messages[1]) == "new");
}

TEST(MessageArray, TypedFrontEndAndPackedEntryRefuseUnprovidedArguments)
{
	CMessageArray messages;
	messages.Init(2, 64);
	messages.AddSafeFormat("%s %d %s", 42);
	AddV(messages, "%s", "literal %s %n %%");
	CHECK(std::string(messages[0]) == "%s 42 %s");
	CHECK(std::string(messages[1]) == "literal %s %n %%");
}

// The executable names its log "<cwd>\Log\Log<n>.txt" (Client.cpp). The log
// is opened through Basic::NormalizeDataPath, so off Windows it lands in the
// Log directory instead of beside it under a name with backslashes in it.
TEST(MessageArray, LogNamedWithBackslashesIsWrittenIntoItsDirectory)
{
	std::error_code error;
	const std::filesystem::path root = std::filesystem::temp_directory_path(error) /
		("message_log_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(root / "Log", error);
	{
		CMessageArray messages;
		messages.Init(2, 16, (root.string() + "\\Log\\Log1.txt").c_str());
		messages.Add("logged");
	}
	std::ifstream in(root / "Log" / "Log1.txt");
	std::string line;
	CHECK(bool(std::getline(in, line)));
	CHECK(line == "logged");
	in.close();
	std::filesystem::remove_all(root, error);
}
