#include "test_framework.h"
#include "ServerInfoFileParser.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>

namespace {

struct TempServerInfo
{
	std::filesystem::path path;

	explicit TempServerInfo(const std::string& text)
	{
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		path = std::filesystem::temp_directory_path()
			/ ("darkeden_server_info_" + std::to_string(stamp) + ".inf");
		Write(text);
	}

	~TempServerInfo()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
	}

	void Write(const std::string& text) const
	{
		std::ofstream output(path, std::ios::binary | std::ios::trunc);
		output << text;
		output.close();
		CHECK(output.good());
	}
};

} // namespace

TEST(ServerInfoParser, DimensionsAreNumberedByPairedAtMarkers)
{
	TempServerInfo file(
		"LoginServerAddress:outside\n"
		"@ first\nLoginServerAddress:first.example\n@\n"
		"LoginServerAddress:between\n"
		"@ second\nLoginServerAddress:second.example\n@\n"
		"@ third\nLoginServerAddress:third.example\n@\n"
		"LoginServerAddress:after\n");
	ServerInfoFileParser parser(file.path.string());
	CHECK(parser.getProperty(0, "LoginServerAddress") == "first.example");
	CHECK(parser.getProperty(1, "LoginServerAddress") == "second.example");
	CHECK(parser.getProperty(2, "LoginServerAddress") == "third.example");
	CHECK(parser.getProperty(3, "LoginServerAddress").empty());
	CHECK(parser.getProperty(-1, "LoginServerAddress").empty());
}

TEST(ServerInfoParser, RemovesSpacesAndSkipsBlankLinesAndWholeLineComments)
{
	TempServerInfo file(
		" # @ ignored marker\n \n @ dimension\n"
		" # LoginServerAddress:ignored\n\n   \n"
		" LoginServerAddress : local host . example \n"
		" LoginServerPort : 9 9 9 9 \n @\n");
	ServerInfoFileParser parser(file.path.string());
	CHECK(parser.getProperty(0, "LoginServerAddress") == "localhost.example");
	CHECK_EQ(9999, parser.getPropertyInt(0, "LoginServerPort"));
}

TEST(ServerInfoParser, KeysAreExactAndCaseSensitiveAndTheFirstMatchWins)
{
	TempServerInfo file(
		"@\nExtraKey:prefix\nKeyExtra:suffix\nkey:lower\n"
		"Key without separator\nKey:first\nKey:second\n@\n");
	ServerInfoFileParser parser(file.path.string());
	CHECK(parser.getProperty(0, "Key") == "first");
	CHECK(parser.getProperty(0, "key") == "lower");
	CHECK(parser.getProperty(0, "KEY").empty());
	CHECK(parser.getProperty(0, "Missing").empty());
}

TEST(ServerInfoParser, ValuesKeepColonsHashesAndTabs)
{
	TempServerInfo file(
		"@\nURL:http://example.test:8080/path#part\n"
		"Label:one\ttwo\n@\n");
	ServerInfoFileParser parser(file.path.string());
	CHECK(parser.getProperty(0, "URL") == "http://example.test:8080/path#part");
	CHECK(parser.getProperty(0, "Label") == "one\ttwo");
}

TEST(ServerInfoParser, IntegerPropertiesKeepDecimalPrefixAndMissingValueSemantics)
{
	TempServerInfo file(
		"@\nPort:12345\nNegative:-17\nPositive:+23\n"
		"Prefix:42suffix\nText:invalid\nZero:0\nEmpty:\n@\n");
	ServerInfoFileParser parser(file.path.string());
	CHECK_EQ(12345, parser.getPropertyInt(0, "Port"));
	CHECK_EQ(-17, parser.getPropertyInt(0, "Negative"));
	CHECK_EQ(23, parser.getPropertyInt(0, "Positive"));
	CHECK_EQ(42, parser.getPropertyInt(0, "Prefix"));
	CHECK_EQ(0, parser.getPropertyInt(0, "Text"));
	CHECK_EQ(0, parser.getPropertyInt(0, "Zero"));
	CHECK_EQ(-1, parser.getPropertyInt(0, "Empty"));
	CHECK_EQ(-1, parser.getPropertyInt(0, "Missing"));
	CHECK_EQ(-1, parser.getPropertyInt(1, "Port"));
}

TEST(ServerInfoParser, ReadsTheFinalRecordWithoutANewlineOrClosingMarker)
{
	TempServerInfo file("@\nLoginServerAddress:final.example");
	ServerInfoFileParser parser(file.path.string());
	CHECK(parser.getProperty(0, "LoginServerAddress") == "final.example");
	CHECK(parser.getProperty(0, "Missing").empty());
}

TEST(ServerInfoParser, EmptyFilesAndEmptyDimensionsHaveNoProperties)
{
	TempServerInfo file("");
	ServerInfoFileParser parser(file.path.string());
	CHECK(parser.getProperty(0, "Key").empty());
	CHECK_EQ(-1, parser.getPropertyInt(0, "Key"));
	file.Write("# comment\n@\n\n@\n@\nKey:next\n@\n");
	CHECK(parser.getProperty(0, "Key").empty());
	CHECK(parser.getProperty(1, "Key") == "next");
}

TEST(ServerInfoParser, EachLookupReopensItsOwnFile)
{
	TempServerInfo first("@\nKey:first\n@\n"), second("@\nKey:second\n@\n");
	ServerInfoFileParser firstParser(first.path.string()), secondParser(second.path.string());
	CHECK(firstParser.getProperty(0, "Key") == "first");
	CHECK(secondParser.getProperty(0, "Key") == "second");
	first.Write("@\nKey:replacement\n@\n");
	CHECK(firstParser.getProperty(0, "Key") == "replacement");
	CHECK(secondParser.getProperty(0, "Key") == "second");
}

TEST(ServerInfoParser, ValuesAroundAndBeyondTheOldLineLimitAreComplete)
{
	TempServerInfo file("");
	ServerInfoFileParser parser(file.path.string());
	// "Key:" adds four bytes to each line; exercise both sides of the old
	// 2047-byte payload limit as well as substantially longer records.
	for (int size : {2042, 2043, 2044, 2048, 4096, 8192})
	{
		const std::string value(size, 'v');
		file.Write("@\nKey:" + value + "\n@\n");
		CHECK(parser.getProperty(0, "Key") == value);
	}
}

TEST(ServerInfoParser, LFAndCRLFHaveTheSamePropertyValues)
{
	TempServerInfo file("");
	ServerInfoFileParser parser(file.path.string());
	for (const std::string& newline : {std::string("\n"), std::string("\r\n")})
	{
		file.Write(" # comment" + newline + "@ first" + newline
			+ "LoginServerAddress:first.example" + newline
			+ "Empty:   " + newline + "@" + newline + "@ second" + newline
			+ "LoginServerAddress:second.example" + newline
			+ "LoginServerPort:9999" + newline
			+ "LoginServerCheckPort:9998");
		CHECK(parser.getProperty(0, "LoginServerAddress") == "first.example");
		CHECK(parser.getProperty(0, "Empty").empty());
		CHECK_EQ(-1, parser.getPropertyInt(0, "Empty"));
		CHECK(parser.getProperty(1, "LoginServerAddress") == "second.example");
		CHECK_EQ(9999, parser.getPropertyInt(1, "LoginServerPort"));
		CHECK_EQ(9998, parser.getPropertyInt(1, "LoginServerCheckPort"));
		CHECK(parser.getProperty(1, "Missing").empty());
	}
}

TEST(ServerInfoParser, ReadsPastLongCommentsAndUnrelatedRecords)
{
	TempServerInfo file("@\n#" + std::string(8192, 'c') + "\n"
		+ "Unrelated:" + std::string(8192, 'v') + "\n"
		+ std::string(8192, ' ') + "\nKey:found\n@\n@\nKey:next\n@\n");
	ServerInfoFileParser parser(file.path.string());
	CHECK(parser.getProperty(0, "Key") == "found");
	CHECK(parser.getProperty(1, "Key") == "next");
	CHECK(parser.getProperty(0, "Missing").empty());
}

TEST(ServerInfoParser, MissingAndRemovedFilesReturnNoValueAndCanBeRecreated)
{
	TempServerInfo file("");
	CHECK(std::filesystem::remove(file.path));
	ServerInfoFileParser parser(file.path.string());
	CHECK(parser.getProperty(0, "Key").empty());
	CHECK_EQ(-1, parser.getPropertyInt(0, "Key"));
	file.Write("@\nKey:42\n@\n");
	CHECK(parser.getProperty(0, "Key") == "42");
	CHECK_EQ(42, parser.getPropertyInt(0, "Key"));
	CHECK(std::filesystem::remove(file.path));
	CHECK(parser.getProperty(0, "Key").empty());
	CHECK_EQ(-1, parser.getPropertyInt(0, "Key"));
}

TEST(ServerInfoParser, LongKeysMatchWithoutTruncation)
{
	const std::string key(4096, 'k');
	TempServerInfo file("@\n" + key + ":value\n@\n");
	ServerInfoFileParser parser(file.path.string());
	CHECK(parser.getProperty(0, key) == "value");
	CHECK(parser.getProperty(0, key + "Extra").empty());
}

TEST(ServerInfoParser, LongFinalRecordsDoNotPreventMissingKeyLookups)
{
	TempServerInfo file("@\nUnrelated:" + std::string(8192, 'v'));
	ServerInfoFileParser parser(file.path.string());
	CHECK(parser.getProperty(0, "Missing").empty());
	CHECK_EQ(-1, parser.getPropertyInt(0, "Missing"));
}
