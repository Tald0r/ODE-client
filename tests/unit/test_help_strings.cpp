#include "test_framework.h"
#include "MHelpStringTable.h"
#include "TextEncoding.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace {
using Bytes = std::vector<unsigned char>;
static_assert(!std::is_copy_constructible_v<MHelpStringTable>);
static_assert(!std::is_copy_assignable_v<MHelpStringTable>);

struct Encoding
{
	TextEncoding::Encoding previous = TextEncoding::GetResourceEncoding();
	explicit Encoding(TextEncoding::Encoding value = TextEncoding::Encoding::Utf8)
	{
		TextEncoding::SetResourceEncoding(value);
	}
	~Encoding() { TextEncoding::SetResourceEncoding(previous); }
};

struct HelpFile
{
	std::filesystem::path path;
	explicit HelpFile(const Bytes& bytes)
	{
		static unsigned sequence = 0;
		path = std::filesystem::temp_directory_path() / ("darkeden_help_"
			+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
			+ "_" + std::to_string(sequence++) + ".bin");
		std::ofstream output(path, std::ios::binary);
		if (!bytes.empty()) output.write(reinterpret_cast<const char*>(bytes.data()),
			static_cast<std::streamsize>(bytes.size()));
		output.close();
		if (!output.good()) throw std::runtime_error("Cannot write help fixture");
	}
	~HelpFile() { std::error_code error; std::filesystem::remove(path, error); }
};

void Number(Bytes& bytes, unsigned value)
{
	for (unsigned shift = 0; shift < 32; shift += 8)
		bytes.push_back(static_cast<unsigned char>(value >> shift));
}

Bytes Strings(std::initializer_list<std::string> strings)
{
	Bytes bytes;
	Number(bytes, static_cast<unsigned>(strings.size()));
	for (const auto& text : strings)
	{
		Number(bytes, static_cast<unsigned>(text.size()));
		bytes.insert(bytes.end(), text.begin(), text.end());
	}
	return bytes;
}

bool Load(MHelpStringTable& table, const Bytes& bytes)
{
	HelpFile fixture(bytes);
	std::ifstream input(fixture.path, std::ios::binary);
	table.LoadFromFile(input);
	if (input.good()) CHECK_EQ(bytes.size(), input.tellg());
	return input.good();
}

Bytes Save(MHelpStringTable& table)
{
	HelpFile fixture({});
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
	return text.GetString() && std::strcmp(text.GetString(), expected) == 0;
}

bool Displayed(MHelpStringTable& table, int index)
{
	return table.IsDisplayed(static_cast<HELP_OUTPUT>(index));
}
}

TEST(HelpStrings, NewTablesHaveNoRowsOrDisplayedEntries)
{
	MHelpStringTable table;
	CHECK_EQ(0, table.GetSize());
	CHECK(!Displayed(table, 0));
	CHECK(!table.IsDisplayed(HELP_OUTPUT_NULL));
	CHECK(table.Get(0).GetString() == nullptr);
	table.ClearDisplayed();
}

TEST(HelpStrings, BothLookupMethodsMarkOnlyTheRequestedEntry)
{
	MHelpStringTable table;
	table.Init(3);
	table.Set(0, "first"); table.Set(1, "second"); table.Set(2, "third");
	for (int i = 0; i < 3; ++i) CHECK(!Displayed(table, i));
	CHECK(Is(table[1], "second"));
	CHECK(!Displayed(table, 0)); CHECK(Displayed(table, 1)); CHECK(!Displayed(table, 2));
	CHECK(Is(table.Get(2), "third"));
	CHECK(!Displayed(table, 0)); CHECK(Displayed(table, 1)); CHECK(Displayed(table, 2));
	CHECK(Is(table[1], "second"));
	CHECK(Displayed(table, 1));
}

TEST(HelpStrings, ClearingDisplayHistoryKeepsTheHelpText)
{
	MHelpStringTable table;
	table.Init(2);
	table.Set(0, "first"); table.Set(1, "second");
	table.Get(0); table[1];
	table.ClearDisplayed();
	CHECK(!Displayed(table, 0)); CHECK(!Displayed(table, 1));
	CHECK(Is(table.GetInternalPointer()[0], "first"));
	CHECK(Is(table.GetInternalPointer()[1], "second"));
}

TEST(HelpStrings, EmptyStringsStillCountAsDisplayedWhenLookedUp)
{
	MHelpStringTable table;
	table.Init(1);
	CHECK(table.Get(0).GetString() == nullptr);
	CHECK(Displayed(table, 0));
}

TEST(HelpStrings, InvalidLookupsReturnTheImmutableFallbackWithoutMarkingRows)
{
	MHelpStringTable table;
	table.Init(2);
	CHECK(table[-1].GetString() == nullptr);
	CHECK(table.Get(2).GetString() == nullptr);
	CHECK(table.Get(100000).GetString() == nullptr);
	CHECK(!Displayed(table, 0)); CHECK(!Displayed(table, 1));
	CHECK(!table.IsDisplayed(HELP_OUTPUT_NULL));
}

TEST(HelpStrings, PositiveInitializationReplacesRowsAndDisplayHistory)
{
	MHelpStringTable table;
	table.Init(2);
	table.Set(0, "old"); table.Get(0);
	table.Init(1);
	CHECK_EQ(1, table.GetSize());
	CHECK(!Displayed(table, 0));
	CHECK(table.GetInternalPointer()[0].GetString() == nullptr);
}

TEST(HelpStrings, NonpositiveInitializationKeepsRowsAndClearsDisplayHistory)
{
	MHelpStringTable table;
	table.Init(1);
	table.Set(0, "old");
	for (int size : {0, -1})
	{
		table.Get(0);
		table.Init(size);
		CHECK_EQ(1, table.GetSize());
		CHECK(!Displayed(table, 0));
		CHECK(Is(table.GetInternalPointer()[0], "old"));
	}
}

TEST(HelpStrings, FileRowsRoundTripWithoutSerializingDisplayHistory)
{
	Encoding encoding;
	const Bytes bytes{2, 0, 0, 0, 1, 0, 0, 0, 'a', 2, 0, 0, 0, 'b', 'c'};
	MHelpStringTable table;
	CHECK(Load(table, bytes));
	CHECK_EQ(2, table.GetSize());
	CHECK(!Displayed(table, 0)); CHECK(!Displayed(table, 1));
	CHECK(Is(table.Get(0), "a"));
	CHECK(Is(table[1], "bc"));
	CHECK(Save(table) == bytes);
}

TEST(HelpStrings, ValidReloadsWithTheSameOrDifferentSizeClearDisplayHistory)
{
	Encoding encoding;
	MHelpStringTable table;
	CHECK(Load(table, Strings({"old", "other"})));
	table.Get(0); table.Get(1);
	CHECK(Load(table, Strings({"new", "replacement"})));
	CHECK(!Displayed(table, 0)); CHECK(!Displayed(table, 1));
	CHECK(Is(table.Get(0), "new"));
	CHECK(Load(table, Strings({"last"})));
	CHECK_EQ(1, table.GetSize());
	CHECK(!Displayed(table, 0));
	CHECK(Is(table.Get(0), "last"));
}

TEST(HelpStrings, EmptyFileTablesClearAllRows)
{
	Encoding encoding;
	MHelpStringTable table;
	CHECK(Load(table, Strings({"old"})));
	table.Get(0);
	CHECK(Load(table, Strings({})));
	CHECK_EQ(0, table.GetSize());
	CHECK(!Displayed(table, 0));
	CHECK(Save(table) == Bytes({0, 0, 0, 0}));
}

TEST(HelpStrings, LoadingFromAnOffsetLeavesTheTrailerUnread)
{
	Encoding encoding;
	Bytes bytes{0xee};
	const auto rows = Strings({"help"});
	bytes.insert(bytes.end(), rows.begin(), rows.end());
	bytes.push_back(0xdd);
	HelpFile fixture(bytes);
	std::ifstream input(fixture.path, std::ios::binary);
	input.seekg(1);
	MHelpStringTable table;
	table.LoadFromFile(input);
	CHECK(input.good());
	CHECK_EQ(1 + rows.size(), input.tellg());
	CHECK_EQ(0xdd, input.get());
	CHECK(Is(table.Get(0), "help"));
}

TEST(HelpStrings, Utf8AndLegacyKoreanFilesUseTheResourceCodec)
{
	MHelpStringTable table;
	{
		Encoding encoding;
		const auto bytes = Strings({"도움말", "雪"});
		CHECK(Load(table, bytes));
		CHECK(Is(table[0], "도움말"));
		CHECK(Is(table[1], "雪"));
		CHECK(Save(table) == bytes);
	}
	{
		Encoding encoding(TextEncoding::Encoding::Cp949);
		const auto bytes = Strings({"\xb0\xa1"});
		CHECK(Load(table, bytes));
		CHECK(Is(table[0], "가"));
		CHECK(Save(table) == bytes);
	}
}

TEST(HelpStrings, EveryTruncatedReloadPreservesTextAndDisplayHistory)
{
	Encoding encoding;
	const auto replacement = Strings({"new first", "new second", "new third"});
	for (std::size_t size = 0; size < replacement.size(); ++size)
	{
		MHelpStringTable table;
		CHECK(Load(table, Strings({"old first", "old second"})));
		table.Get(1);
		CHECK(!Load(table, Bytes(replacement.begin(), replacement.begin() + size)));
		CHECK_EQ(2, table.GetSize());
		CHECK(!Displayed(table, 0)); CHECK(Displayed(table, 1));
		CHECK(Is(table.GetInternalPointer()[0], "old first"));
		if (table.GetSize() > 1) CHECK(Is(table.GetInternalPointer()[1], "old second"));
	}
}

TEST(HelpStrings, StreamExceptionsPreserveTextAndDisplayHistory)
{
	Encoding encoding;
	MHelpStringTable table;
	CHECK(Load(table, Strings({"old first", "old second"})));
	table.Get(1);
	auto bytes = Strings({"new first", "new second", "new third"});
	bytes.pop_back();
	HelpFile fixture(bytes);
	std::ifstream input(fixture.path, std::ios::binary);
	input.exceptions(std::ios::failbit | std::ios::badbit);
	bool threw = false;
	try { table.LoadFromFile(input); }
	catch (const std::ios_base::failure&) { threw = true; }
	CHECK(threw);
	CHECK_EQ(2, table.GetSize());
	CHECK(!Displayed(table, 0)); CHECK(Displayed(table, 1));
	CHECK(Is(table.GetInternalPointer()[0], "old first"));
	CHECK(Is(table.GetInternalPointer()[1], "old second"));
}

TEST(HelpStrings, EmptyReloadsCannotMarkEntriesThatNoLongerExist)
{
	Encoding encoding;
	MHelpStringTable table;
	CHECK(Load(table, Strings({"old"})));
	table.Get(0);
	CHECK(Load(table, Strings({})));
	CHECK(table.Get(0).GetString() == nullptr);
	CHECK(!Displayed(table, 0));
}

TEST(HelpStrings, ReleaseMakesAllDisplayedEntriesAndLookupsAbsent)
{
	MHelpStringTable table;
	table.Init(2);
	table.Get(0); table.Get(1);
	table.Release();
	CHECK_EQ(0, table.GetSize());
	CHECK(!Displayed(table, 0)); CHECK(!Displayed(table, 1));
	CHECK(table.Get(0).GetString() == nullptr);
	CHECK(table[1].GetString() == nullptr);
	CHECK(!Displayed(table, 0)); CHECK(!Displayed(table, 1));
}

TEST(HelpStrings, InvalidCountsPreserveTextAndDisplayHistory)
{
	Encoding encoding;
	for (unsigned count : {0xffffffffu, 0x7fffffffu, 3u})
	{
		MHelpStringTable table;
		CHECK(Load(table, Strings({"old"})));
		table.Get(0);
		Bytes bytes;
		Number(bytes, count);
		CHECK(!Load(table, bytes));
		CHECK_EQ(1, table.GetSize());
		CHECK(Displayed(table, 0));
		CHECK(Is(table.GetInternalPointer()[0], "old"));
	}
}

TEST(HelpStrings, PrefailedAndClosedStreamsDoNotResetDisplayHistory)
{
	Encoding encoding;
	MHelpStringTable table;
	CHECK(Load(table, Strings({"old"})));
	table.Get(0);
	std::ifstream closed;
	table.LoadFromFile(closed);
	CHECK(closed.fail());
	CHECK_EQ(1, table.GetSize());
	CHECK(Displayed(table, 0));
	HelpFile fixture(Strings({"replacement"}));
	std::ifstream failed(fixture.path, std::ios::binary);
	failed.setstate(std::ios::failbit);
	table.LoadFromFile(failed);
	CHECK(failed.fail());
	CHECK_EQ(1, table.GetSize());
	CHECK(Displayed(table, 0));
	CHECK(Is(table.GetInternalPointer()[0], "old"));
}

TEST(HelpStrings, AValidReloadAfterAFailurePublishesFreshTextAndHistory)
{
	Encoding encoding;
	MHelpStringTable table;
	CHECK(Load(table, Strings({"old"})));
	table.Get(0);
	CHECK(!Load(table, {1, 0, 0, 0, 8, 0, 0, 0, 'a'}));
	CHECK(Displayed(table, 0));
	CHECK(Load(table, Strings({"new first", "new second"})));
	CHECK_EQ(2, table.GetSize());
	CHECK(!Displayed(table, 0)); CHECK(!Displayed(table, 1));
	CHECK(Is(table.Get(0), "new first"));
	CHECK(Is(table.Get(1), "new second"));
}

TEST(HelpStrings, ReleasingThroughTheBaseCannotExposeStaleDisplayEntries)
{
	MHelpStringTable table;
	table.Init(1);
	table.Get(0);
	MStringArray& base = table;
	base.Release();
	CHECK_EQ(0, table.GetSize());
	CHECK(!Displayed(table, 0));
	CHECK(table.Get(0).GetString() == nullptr);
	CHECK(!Displayed(table, 0));
	table.Init(1);
	CHECK(!Displayed(table, 0));
}
