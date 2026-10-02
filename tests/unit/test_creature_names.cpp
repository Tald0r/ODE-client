#include "test_framework.h"
#include "CreatureNameSelection.h"
#include "MonsterNameTable.h"
#include "TextEncoding.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Bytes
{
	std::vector<unsigned char> data;
	Bytes& U32(std::uint32_t value)
	{
		for (unsigned int shift = 0; shift < 32; shift += 8)
			data.push_back(static_cast<unsigned char>(value >> shift));
		return *this;
	}
	Bytes& Text(const std::string& value)
	{
		U32(static_cast<std::uint32_t>(value.size()));
		data.insert(data.end(), value.begin(), value.end());
		return *this;
	}
	Bytes& Group(std::initializer_list<std::string> names)
	{
		U32(static_cast<std::uint32_t>(names.size()));
		for (const auto& name : names) Text(name);
		return *this;
	}
};

struct NameFile
{
	std::filesystem::path path;
	explicit NameFile(const Bytes& bytes)
	{
		static unsigned int sequence = 0;
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		path = std::filesystem::temp_directory_path() / ("darkeden_creature_names_"
			+ std::to_string(stamp) + "_" + std::to_string(sequence++) + ".bin");
		std::ofstream output(path, std::ios::binary);
		if (!bytes.data.empty())
			output.write(reinterpret_cast<const char*>(bytes.data.data()),
				static_cast<std::streamsize>(bytes.data.size()));
		output.close();
		if (!output.good()) throw std::runtime_error("Cannot write creature name fixture");
	}
	~NameFile()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
	}
};

template<class Table>
bool Load(Table& table, const Bytes& bytes)
{
	NameFile fixture(bytes);
	std::ifstream input(fixture.path, std::ios::binary);
	table.LoadFromFile(input);
	if (input.good()) CHECK_EQ(bytes.data.size(), input.tellg());
	return input.good();
}

bool Is(const char* value, const char* expected)
{
	return value != nullptr && std::strcmp(value, expected) == 0;
}

struct NameTables
{
	MonsterNameTable monsters;
	MLevelNameTable levels;
	MonsterNameTable* previousMonsters = g_pMonsterNameTable;
	MLevelNameTable* previousLevels = g_pLevelNameTable;

	NameTables()
	{
		if (!Load(monsters, Bytes().Group({"Dark", "Elder"}).Group({"of"})
			.Group({"Shade", "Wraith", "Specter"}))
			|| !Load(levels, Bytes().Group({"", "Young", "Veteran"})))
			throw std::runtime_error("Cannot load creature name fixture");
		g_pMonsterNameTable = &monsters;
		g_pLevelNameTable = &levels;
	}
	~NameTables()
	{
		g_pMonsterNameTable = previousMonsters;
		g_pLevelNameTable = previousLevels;
	}
};

struct EncodingScope
{
	TextEncoding::Encoding previous = TextEncoding::GetResourceEncoding();
	~EncodingScope() { TextEncoding::SetResourceEncoding(previous); }
};

} // namespace

TEST(CreatureNames, FreshSelectionHasNoLevelTitle)
{
	NameTables tables;
	CreatureNameSelection selection;
	CHECK_EQ(0, selection.HasLevelName());
	CHECK(Is(selection.GetLevelName(tables.levels), ""));
}

TEST(CreatureNames, RealLibraryTablesKeepTheirThreeNameGroupsDistinct)
{
	NameTables tables;
	CHECK(g_pMonsterNameTable == &tables.monsters);
	CHECK(g_pLevelNameTable == &tables.levels);
	CHECK_EQ(2, g_pMonsterNameTable->GetFirstNameSize());
	CHECK_EQ(1, g_pMonsterNameTable->GetMiddleNameSize());
	CHECK_EQ(3, g_pMonsterNameTable->GetLastNameSize());
	CHECK(Is(g_pMonsterNameTable->GetFirstName(0), "Dark"));
	CHECK(Is(g_pMonsterNameTable->GetFirstName(1), "Elder"));
	CHECK(Is(g_pMonsterNameTable->GetMiddleName(0), "of"));
	CHECK(Is(g_pMonsterNameTable->GetLastName(2), "Specter"));
}

TEST(CreatureNames, TableLookupsRejectNegativeAndPastEndIndexes)
{
	NameTables tables;
	for (int index : {-1, (std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()})
	{
		CHECK(tables.monsters.GetFirstName(index) == nullptr);
		CHECK(tables.monsters.GetMiddleName(index) == nullptr);
		CHECK(tables.monsters.GetLastName(index) == nullptr);
		CHECK(tables.levels[index].GetString() == nullptr);
	}
	CHECK(tables.monsters.GetFirstName(2) == nullptr);
	CHECK(tables.monsters.GetMiddleName(1) == nullptr);
	CHECK(tables.monsters.GetLastName(3) == nullptr);
}

TEST(CreatureNames, LevelTitleFileRoundTripsWithItsOriginalLayout)
{
	NameTables tables;
	NameFile fixture({});
	{
		std::ofstream output(fixture.path, std::ios::binary);
		tables.levels.SaveToFile(output);
		output.close();
		CHECK(output.good());
	}
	std::ifstream input(fixture.path, std::ios::binary);
	const std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(input),
		std::istreambuf_iterator<char>()};
	CHECK(bytes == Bytes().Group({"", "Young", "Veteran"}).data);
	MLevelNameTable loaded;
	CHECK(Load(loaded, Bytes{bytes}));
	CHECK_EQ(3, loaded.GetSize());
	CHECK(Is(loaded[2].GetString(), "Veteran"));
}

TEST(CreatureNames, ReloadResizesEachNameGroupIncludingEmptyGroups)
{
	NameTables tables;
	CHECK(Load(tables.monsters, Bytes().Group({"New"}).Group({}).Group({"Ghost"})));
	CHECK_EQ(1, tables.monsters.GetFirstNameSize());
	CHECK_EQ(0, tables.monsters.GetMiddleNameSize());
	CHECK_EQ(1, tables.monsters.GetLastNameSize());
	CHECK(Is(tables.monsters.GetFirstName(0), "New"));
	CHECK(Is(tables.monsters.GetLastName(0), "Ghost"));
	CHECK(tables.monsters.GetMiddleName(0) == nullptr);
	CHECK(tables.monsters.GetFirstName(1) == nullptr);
	CHECK(tables.monsters.GetLastName(1) == nullptr);
}

TEST(CreatureNames, RandomTitleSelectionUpdatesTheLegacyIndexQuery)
{
	NameTables tables;
	CreatureNameSelection selection;
	struct Example { int draw; int index; const char* name; };
	for (const auto& example : {Example{0, 0, ""}, Example{1, 1, "Young"},
		Example{2, 2, "Veteran"}, Example{3, 0, ""}, Example{8, 2, "Veteran"},
		Example{(std::numeric_limits<int>::max)(), 1, "Young"}})
	{
		selection.SelectLevelName(tables.levels, example.draw);
		CHECK_EQ(example.index, selection.HasLevelName());
		CHECK(Is(selection.GetLevelName(tables.levels), example.name));
	}
}

TEST(CreatureNames, HallucinationSelectionUsesOnlyTheLastNameGroup)
{
	NameTables tables;
	CreatureNameSelection selection;
	const MString prefix("GM");
	struct Example { int draw; const char* name; };
	for (const auto& example : {Example{0, "Shade"}, Example{1, "Wraith"},
		Example{2, "Specter"}, Example{3, "Shade"}, Example{7, "Wraith"}})
	{
		selection.SelectHallucinationName(tables.monsters, example.draw);
		CHECK(Is(selection.GetHallucinationName("Slayer", prefix, &tables.monsters), example.name));
	}
	// Changing a title does not change the separately selected hallucination name.
	selection.SelectLevelName(tables.levels, 2);
	CHECK(Is(selection.GetHallucinationName("Slayer", prefix, &tables.monsters), "Wraith"));
}

TEST(CreatureNames, OperatorPrefixMatchesOnlyAtTheStartAndIsCaseSensitive)
{
	NameTables tables;
	CreatureNameSelection selection;
	selection.SelectHallucinationName(tables.monsters, 2);
	const MString prefix("GM");
	for (const char* name : {"GM", "GMAlice", "GM Alice"})
		CHECK(selection.GetHallucinationName(name, prefix, &tables.monsters) == name);
	for (const char* name : {"AliceGM", "gmAlice", "G", ""})
		CHECK(Is(selection.GetHallucinationName(name, prefix, &tables.monsters), "Specter"));
}

TEST(CreatureNames, MultibyteNamesDecodeAndUseTheFullOperatorPrefix)
{
	EncodingScope encoding;
	CHECK(TextEncoding::SetResourceEncoding(TextEncoding::Encoding::Utf8));
	NameTables tables;
	const char* prefixText = "\xED\x95\x9C";
	const char* alias = "\xEB\xAC\xB4\xEC\x82\xAC";
	CHECK(Load(tables.monsters, Bytes().Group({}).Group({}).Group({alias})));
	CreatureNameSelection selection;
	selection.SelectHallucinationName(tables.monsters, 13);
	const MString prefix(prefixText);
	const std::string operatorName = std::string(prefixText) + "Alice";
	CHECK(selection.GetHallucinationName(operatorName.c_str(), prefix, &tables.monsters)
		== operatorName.c_str());
	CHECK(Is(selection.GetHallucinationName("Alice", prefix, &tables.monsters), alias));
}

TEST(CreatureNames, OperatorExceptionDoesNotRequireAHallucinationTable)
{
	CreatureNameSelection selection;
	const MString prefix("GM");
	const char* name = "GMAlice";
	CHECK(selection.GetHallucinationName(name, prefix, nullptr) == name);
}

TEST(CreatureNames, LookupsBorrowTheCurrentTableEntries)
{
	NameTables tables;
	CreatureNameSelection selection;
	selection.SelectLevelName(tables.levels, 2);
	selection.SelectHallucinationName(tables.monsters, 1);
	const MString prefix("GM");
	CHECK(selection.GetLevelName(tables.levels) == tables.levels[2].GetString());
	CHECK(selection.GetHallucinationName("Slayer", prefix, &tables.monsters)
		== tables.monsters.GetLastName(1));
	CHECK(tables.levels.Set(2, "Champion"));
	CHECK(tables.monsters.m_LastNames.Set(1, "Spirit"));
	CHECK(Is(selection.GetLevelName(tables.levels), "Champion"));
	CHECK(Is(selection.GetHallucinationName("Slayer", prefix, &tables.monsters), "Spirit"));
	tables.levels.Release();
	tables.monsters.m_LastNames.Release();
	CHECK(selection.GetLevelName(tables.levels) == nullptr);
	CHECK(selection.GetHallucinationName("Slayer", prefix, &tables.monsters) == nullptr);
}

TEST(CreatureNames, IncompleteCountPrefixesFailWithoutCreatingNames)
{
	for (size_t size = 0; size < 4; ++size)
	{
		auto bytes = Bytes().U32(1);
		bytes.data.resize(size);
		MonsterNameTable monsters;
		MLevelNameTable levels;
		CHECK(!Load(monsters, bytes));
		CHECK(!Load(levels, bytes));
		CHECK_EQ(0, monsters.GetFirstNameSize());
		CHECK_EQ(0, monsters.GetMiddleNameSize());
		CHECK_EQ(0, monsters.GetLastNameSize());
		CHECK_EQ(0, levels.GetSize());
	}
}

TEST(CreatureNames, ImpossibleCountsAndStringLengthsAreRejected)
{
	for (const auto& bytes : {Bytes().U32(0xffffffff), Bytes().U32(0x7fffffff),
		Bytes().U32(1).U32(65537), Bytes().U32(1).U32(5)})
	{
		MonsterNameTable monsters;
		MLevelNameTable levels;
		CHECK(!Load(monsters, bytes));
		CHECK(!Load(levels, bytes));
		CHECK(monsters.GetFirstName(0) == nullptr);
		CHECK(levels[0].GetString() == nullptr);
	}
}

TEST(CreatureNames, HallucinationSelectionRetainsTheFullTableIndex)
{
	MonsterNameTable names;
	names.m_LastNames.Init(65538);
	CHECK(names.m_LastNames.Set(1, "Wrapped"));
	CHECK(names.m_LastNames.Set(65537, "Selected"));
	CreatureNameSelection selection;
	selection.SelectHallucinationName(names, 65537);
	const MString prefix("GM");
	CHECK(Is(selection.GetHallucinationName("Slayer", prefix, &names), "Selected"));
}

TEST(CreatureNames, EmptyOperatorPrefixesDoNotExposeActualNames)
{
	NameTables tables;
	CreatureNameSelection selection;
	selection.SelectHallucinationName(tables.monsters, 1);
	MString prefix;
	prefix.Init(0); // Readable empty storage also occurs in an empty resource row.
	for (const char* name : {"Slayer", ""})
		CHECK(Is(selection.GetHallucinationName(name, prefix, &tables.monsters), "Wraith"));
	// A resource string can also carry a leading NUL in a nonzero byte count.
	CHECK(Load(prefix, Bytes().Text(std::string(1, '\0'))));
	CHECK(Is(selection.GetHallucinationName("", prefix, &tables.monsters), "Wraith"));
}

TEST(CreatureNames, NegativeRandomSamplesResetBothSelectionsToZero)
{
	NameTables tables;
	CreatureNameSelection selection;
	const MString prefix("GM");
	for (int draw : {-1, -7, (std::numeric_limits<int>::min)()})
	{
		selection.SelectLevelName(tables.levels, 2);
		selection.SelectHallucinationName(tables.monsters, 2);
		selection.SelectLevelName(tables.levels, draw);
		selection.SelectHallucinationName(tables.monsters, draw);
		CHECK_EQ(0, selection.HasLevelName());
		CHECK(Is(selection.GetLevelName(tables.levels), ""));
		CHECK(Is(selection.GetHallucinationName("Slayer", prefix, &tables.monsters), "Shade"));
	}
}

TEST(CreatureNames, EmptyLevelTableClearsThePreviousTitle)
{
	NameTables tables;
	CreatureNameSelection selection;
	selection.SelectLevelName(tables.levels, 2);
	tables.levels.Release();
	selection.SelectLevelName(tables.levels, 37);
	CHECK_EQ(0, selection.HasLevelName());
	CHECK(selection.GetLevelName(tables.levels) == nullptr);
}

TEST(CreatureNames, EmptyHallucinationTableUsesTheZeroSelectionOnReload)
{
	NameTables tables;
	CreatureNameSelection selection;
	selection.SelectHallucinationName(tables.monsters, 2);
	tables.monsters.m_LastNames.Release();
	selection.SelectHallucinationName(tables.monsters, 19);
	const MString prefix("GM");
	CHECK(selection.GetHallucinationName("Slayer", prefix, &tables.monsters) == nullptr);
	CHECK(Load(tables.monsters, Bytes().Group({}).Group({}).Group({"First", "Second"})));
	CHECK(Is(selection.GetHallucinationName("Slayer", prefix, &tables.monsters), "First"));
}

TEST(CreatureNames, MissingOperatorPrefixKeepsTheHallucinationName)
{
	NameTables tables;
	CreatureNameSelection selection;
	selection.SelectHallucinationName(tables.monsters, 2);
	const MString missingPrefix;
	CHECK(Is(selection.GetHallucinationName("Slayer", missingPrefix, &tables.monsters), "Specter"));
}

TEST(CreatureNames, AnUnnamedCreatureCanUseItsSelectedHallucinationName)
{
	NameTables tables;
	CreatureNameSelection selection;
	selection.SelectHallucinationName(tables.monsters, 1);
	const MString prefix("GM");
	CHECK(Is(selection.GetHallucinationName(nullptr, prefix, &tables.monsters), "Wraith"));
}

TEST(CreatureNames, MissingHallucinationTableDoesNotRevealTheActualName)
{
	CreatureNameSelection selection;
	const MString prefix("GM");
	CHECK(selection.GetHallucinationName("Slayer", prefix, nullptr) == nullptr);
	CHECK(selection.GetHallucinationName(nullptr, prefix, nullptr) == nullptr);
}
