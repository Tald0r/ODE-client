#include "test_framework.h"
#include "PCConfigTable.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Bytes
{
	std::vector<unsigned char> data;
	Bytes& Byte(unsigned char value) { data.push_back(value); return *this; }
	Bytes& U32(std::uint32_t value)
	{
		for (unsigned int shift = 0; shift < 32; shift += 8)
			Byte(static_cast<unsigned char>(value >> shift));
		return *this;
	}
	Bytes& Account(const std::string& name, unsigned char slot, std::uint32_t recent)
	{
		Byte(static_cast<unsigned char>(name.size()));
		data.insert(data.end(), name.begin(), name.end());
		return Byte(slot).U32(recent);
	}
};

struct ConfigFile
{
	std::filesystem::path path;
	explicit ConfigFile(const Bytes& bytes = {})
	{
		static unsigned int sequence = 0;
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		path = std::filesystem::temp_directory_path() / ("darkeden_player_config_"
			+ std::to_string(stamp) + "_" + std::to_string(sequence++) + ".bin");
		std::ofstream output(path, std::ios::binary);
		if (!bytes.data.empty())
			output.write(reinterpret_cast<const char*>(bytes.data.data()), bytes.data.size());
		output.close();
		if (!output.good()) throw std::runtime_error("Cannot write player config fixture");
	}
	~ConfigFile()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
	}
	std::vector<unsigned char> Read() const
	{
		std::ifstream input(path, std::ios::binary);
		return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
	}
};

template<class T>
bool Load(T& target, const Bytes& bytes)
{
	ConfigFile fixture(bytes);
	std::ifstream input(fixture.path, std::ios::binary);
	target.LoadFromFile(input);
	return input.good();
}

template<class T>
std::vector<unsigned char> Save(T& source)
{
	ConfigFile fixture;
	std::ofstream output(fixture.path, std::ios::binary);
	source.SaveToFile(output);
	output.close();
	CHECK(output.good());
	return fixture.Read();
}

PlayerConfig* Add(PlayerConfigTable& table, const std::string& name, int slot,
	std::uint32_t recent = 0)
{
	auto config = std::make_unique<PlayerConfig>();
	config->SetPlayerID(name);
	CHECK(Load(*config, Bytes().Byte(static_cast<unsigned char>(slot)).U32(recent)));
	auto* result = config.get();
	table.AddPlayerConfig(config.release());
	return result;
}

PlayerConfigTable& AddWorld(WorldPlayerConfigTable& worlds, int id)
{
	auto* table = new PlayerConfigTable;
	worlds.AddPlayerConfigTable(id, table);
	return *table;
}

void CheckAccount(const PlayerConfigTable& table, const char* name, int slot,
	std::uint32_t recent)
{
	const auto* config = table.GetPlayerConfig(name);
	CHECK(config != nullptr);
	if (config == nullptr) return;
	CHECK(config->GetPlayerID() == name);
	CHECK_EQ(slot, config->GetLastSlot());
	CHECK_EQ(recent, config->GetRecentCount());
}

std::string AccountName(int i)
{
	return "account" + std::string(i < 10 ? "0" : "") + std::to_string(i);
}

} // namespace

TEST(PlayerConfig, NewRecordAndAllThreeCharacterSlots)
{
	PlayerConfig config;
	CHECK(config.GetPlayerID().empty());
	CHECK_EQ(0, config.GetLastSlot());
	CHECK_EQ(0, config.GetRecentCount());
	for (int slot : {2, 1, 0})
	{
		config.SetLastSlot(slot);
		CHECK_EQ(slot, config.GetLastSlot());
	}
}

TEST(PlayerConfig, ReadsTheFiveByteRecordWithoutChangingAccountIdentity)
{
	PlayerConfig config;
	config.SetPlayerID("player");
	CHECK(Load(config, Bytes().Byte(2).U32(0x12345678)));
	CHECK_EQ(2, config.GetLastSlot());
	CHECK_EQ(0x12345678, config.GetRecentCount());
	CHECK(config.GetPlayerID() == "player");
}

TEST(PlayerConfig, SavingAgesTheRecordAndPinsItsLayout)
{
	PlayerConfig config;
	CHECK(Load(config, Bytes().Byte(1).U32(8)));
	CHECK(Save(config) == Bytes().Byte(1).U32(9).data);
	CHECK_EQ(9, config.GetRecentCount());
	CHECK(Save(config) == Bytes().Byte(1).U32(10).data);
	CHECK_EQ(10, config.GetRecentCount());
}

TEST(PlayerConfig, SelectingACharacterResetsTheRecentCount)
{
	PlayerConfig config;
	CHECK(Load(config, Bytes().Byte(1).U32(1234)));
	config.SetLastSlot(2);
	CHECK_EQ(2, config.GetLastSlot());
	CHECK_EQ(0, config.GetRecentCount());
}

TEST(PlayerConfig, AccountLookupReplacementAndRejectedAdditions)
{
	PlayerConfigTable table;
	CHECK(table.GetPlayerConfig(nullptr) == nullptr);
	CHECK(table.GetPlayerConfig("missing") == nullptr);
	Add(table, "player", 1);
	Add(table, "Player", 2);
	Add(table, "player", 0, 3);
	CHECK_EQ(2, table.size());
	CheckAccount(table, "player", 0, 3);
	CheckAccount(table, "Player", 2, 0);
	CHECK(table.GetPlayerConfig("PLAYER") == nullptr);
	table.AddPlayerConfig(nullptr);
	// A rejected empty identity still belongs to its caller.
	auto unnamed = std::make_unique<PlayerConfig>();
	table.AddPlayerConfig(unnamed.get());
	CHECK_EQ(2, table.size());
	table.Release();
	table.Release();
	CHECK(table.empty());
	Add(table, "new", 1);
	CheckAccount(table, "new", 1, 0);
}

TEST(PlayerConfig, AccountTableRoundTripsAndPinsNameLengthsAndOrdering)
{
	PlayerConfigTable table;
	Add(table, "beta", 2, 7);
	Add(table, "alpha", 0, 3);
	const auto bytes = Save(table);
	CHECK(bytes == Bytes().U32(2).Account("alpha", 0, 4).Account("beta", 2, 8).data);
	PlayerConfigTable loaded;
	CHECK(Load(loaded, Bytes{bytes}));
	CHECK_EQ(2, loaded.size());
	CheckAccount(loaded, "alpha", 0, 4);
	CheckAccount(loaded, "beta", 2, 8);
}

TEST(PlayerConfig, OnlyTheTwentyMostRecentAccountsAreSaved)
{
	PlayerConfigTable table;
	for (int i = 0; i < 21; ++i)
		Add(table, AccountName(i), i % 3, 20 - i);
	PlayerConfigTable loaded;
	CHECK(Load(loaded, Bytes{Save(table)}));
	CHECK_EQ(20, loaded.size());
	CHECK_EQ(21, table.size());
	CHECK(loaded.GetPlayerConfig(AccountName(0).c_str()) == nullptr);
	for (int i = 1; i < 21; ++i)
		CheckAccount(loaded, AccountName(i).c_str(), i % 3, 21 - i);
	// An omitted account is kept in memory without being aged by this save.
	CheckAccount(table, AccountName(0).c_str(), 0, 20);
}

TEST(PlayerConfig, RecentCountTiesRetainLexicographicAccountOrder)
{
	PlayerConfigTable table;
	for (int i = 20; i >= 0; --i)
		Add(table, AccountName(i), 1, 5);
	PlayerConfigTable loaded;
	CHECK(Load(loaded, Bytes{Save(table)}));
	CHECK_EQ(20, loaded.size());
	for (int i = 0; i < 20; ++i)
		CheckAccount(loaded, AccountName(i).c_str(), 1, 6);
	CHECK(loaded.GetPlayerConfig(AccountName(20).c_str()) == nullptr);
}

TEST(PlayerConfig, AccountLoaderSkipsPlaceholdersAndKeepsTheLastDuplicate)
{
	PlayerConfigTable table;
	Add(table, "old", 2);
	CHECK(Load(table, Bytes().U32(4).Account("player", 0, 1).Byte(0)
		.Account("player", 2, 3).Account("other", 1, 4)));
	CHECK_EQ(2, table.size());
	CheckAccount(table, "player", 2, 3);
	CheckAccount(table, "other", 1, 4);
	CHECK(table.GetPlayerConfig("old") == nullptr);
}

TEST(PlayerConfig, WorldFilePinsVersionTwoAndKeepsAccountsIndependent)
{
	WorldPlayerConfigTable worlds;
	Add(AddWorld(worlds, 7), "player", 2, 5);
	Add(AddWorld(worlds, 1), "player", 0, 3);
	ConfigFile fixture;
	worlds.SaveToFile(fixture.path.string().c_str());
	CHECK(fixture.Read() == Bytes().U32(2).U32(2)
		.U32(1).U32(1).Account("player", 0, 4)
		.U32(7).U32(1).Account("player", 2, 6).data);
	WorldPlayerConfigTable loaded;
	loaded.LoadFromFile(fixture.path.string().c_str());
	CHECK_EQ(2, loaded.size());
	CHECK(loaded.GetPlayerConfigTable(99) == nullptr);
	for (int id : {1, 7})
	{
		const auto* table = loaded.GetPlayerConfigTable(id);
		CHECK(table != nullptr);
		if (table) CheckAccount(*table, "player", id == 1 ? 0 : 2, id == 1 ? 4 : 6);
	}
}

TEST(PlayerConfig, WorldReplacementAndReleaseOwnTheirTables)
{
	WorldPlayerConfigTable worlds;
	Add(AddWorld(worlds, 1), "old", 0);
	Add(AddWorld(worlds, 1), "new", 2);
	CHECK_EQ(1, worlds.size());
	CHECK(worlds.GetPlayerConfigTable(1)->GetPlayerConfig("old") == nullptr);
	CheckAccount(*worlds.GetPlayerConfigTable(1), "new", 2, 0);
	worlds.AddPlayerConfigTable(1, nullptr);
	CHECK_EQ(1, worlds.size());
	worlds.Release();
	worlds.Release();
	CHECK(worlds.empty());
	CHECK(worlds.GetPlayerConfigTable(1) == nullptr);
}

TEST(PlayerConfig, WorldLoaderSkipsPlaceholdersAndKeepsTheLastDuplicate)
{
	ConfigFile fixture(Bytes().U32(2).U32(3).U32(0xffffffff)
		.U32(7).U32(1).Account("old", 0, 1).U32(7).U32(1).Account("new", 2, 9));
	WorldPlayerConfigTable worlds;
	worlds.LoadFromFile(fixture.path.string().c_str());
	CHECK_EQ(1, worlds.size());
	const auto* table = worlds.GetPlayerConfigTable(7);
	CHECK(table != nullptr);
	if (table)
	{
		CHECK(table->GetPlayerConfig("old") == nullptr);
		CheckAccount(*table, "new", 2, 9);
	}
}

TEST(PlayerConfig, AbsentOrUnsupportedWorldFileClearsOldSettings)
{
	WorldPlayerConfigTable worlds;
	Add(AddWorld(worlds, 1), "old", 0);
	ConfigFile unsupported(Bytes().U32(1).U32(0));
	worlds.LoadFromFile(unsupported.path.string().c_str());
	CHECK(worlds.empty());
	Add(AddWorld(worlds, 1), "old", 0);
	worlds.LoadFromFile(nullptr);
	CHECK(worlds.empty());
	Add(AddWorld(worlds, 1), "old", 0);
	worlds.LoadFromFile((unsupported.path.string() + ".missing").c_str());
	CHECK(worlds.empty());
}

TEST(PlayerConfig, InvalidSelectedSlotsPreserveTheSlotAndRecentCount)
{
	for (int slot : {-1, -256, 3, 255, (std::numeric_limits<int>::max)()})
	{
		PlayerConfig config;
		CHECK(Load(config, Bytes().Byte(1).U32(9)));
		config.SetLastSlot(slot);
		CHECK_EQ(1, config.GetLastSlot());
		CHECK_EQ(9, config.GetRecentCount());
	}
}

TEST(PlayerConfig, InvalidStoredSlotsDoNotPublishARecord)
{
	for (int slot : {3, 127, 255})
	{
		PlayerConfig config;
		CHECK(Load(config, Bytes().Byte(1).U32(9)));
		CHECK(!Load(config, Bytes().Byte(static_cast<unsigned char>(slot)).U32(77)));
		CHECK_EQ(1, config.GetLastSlot());
		CHECK_EQ(9, config.GetRecentCount());
	}
}

TEST(PlayerConfig, EveryTruncatedRecordPreservesPreviousValues)
{
	const auto complete = Bytes().Byte(2).U32(0xaabbccdd);
	for (size_t end = 0; end < complete.data.size(); ++end)
	{
		PlayerConfig config;
		CHECK(Load(config, Bytes().Byte(1).U32(9)));
		auto partial = complete;
		partial.data.resize(end);
		CHECK(!Load(config, partial));
		CHECK_EQ(1, config.GetLastSlot());
		CHECK_EQ(9, config.GetRecentCount());
	}
}

TEST(PlayerConfig, OldestRecentCountDoesNotWrapToMostRecent)
{
	PlayerConfig config;
	CHECK(Load(config, Bytes().Byte(2).U32(0xffffffff)));
	CHECK(Save(config) == Bytes().Byte(2).U32(0xffffffff).data);
	CHECK_EQ(0xffffffff, config.GetRecentCount());
}

TEST(PlayerConfig, FailedRecordWritesDoNotAgeTheAccount)
{
	PlayerConfig config;
	CHECK(Load(config, Bytes().Byte(1).U32(9)));
	std::ofstream unopened;
	config.SaveToFile(unopened);
	CHECK(unopened.fail());
	CHECK_EQ(9, config.GetRecentCount());
}

TEST(PlayerConfig, AccountReaderRejectsNegativeCounts)
{
	PlayerConfigTable table;
	Add(table, "old", 1);
	CHECK(!Load(table, Bytes().U32(0xffffffff)));
	CHECK(table.empty());
}

TEST(PlayerConfig, AccountNamesCannotAliasAnEmbeddedNullPrefix)
{
	PlayerConfigTable table;
	CHECK(!Load(table, Bytes().U32(1).Account(std::string("al\0ice", 6), 2, 3)));
	CHECK(table.empty());
}

TEST(PlayerConfig, UnrepresentableNamesFailBeforeWritingOrAging)
{
	for (const auto& name : {std::string(), std::string(256, 'a'), std::string("a\0b", 3)})
	{
		PlayerConfigTable table;
		auto* config = Add(table, "valid", 2, 5);
		config->SetPlayerID(name);
		ConfigFile fixture;
		std::ofstream output(fixture.path, std::ios::binary);
		table.SaveToFile(output);
		CHECK(output.fail());
		output.close();
		CHECK(fixture.Read().empty());
		CHECK_EQ(5, config->GetRecentCount());
	}
}

TEST(PlayerConfig, WorldSaveValidatesNestedNamesBeforeTruncatingTheFile)
{
	WorldPlayerConfigTable worlds;
	auto* valid = Add(AddWorld(worlds, 1), "valid", 0, 7);
	auto* invalid = Add(AddWorld(worlds, 2), std::string(256, 'a'), 2, 9);
	const auto original = Bytes().U32(2).U32(0);
	ConfigFile fixture(original);
	worlds.SaveToFile(fixture.path.string().c_str());
	CHECK(fixture.Read() == original.data);
	CHECK_EQ(7, valid->GetRecentCount());
	CHECK_EQ(9, invalid->GetRecentCount());
}

TEST(PlayerConfig, WorldSaveRejectsAReservedWorldIdBeforeTruncatingTheFile)
{
	WorldPlayerConfigTable worlds;
	Add(AddWorld(worlds, -1), "player", 0, 7);
	const auto original = Bytes().U32(2).U32(0);
	ConfigFile fixture(original);
	worlds.SaveToFile(fixture.path.string().c_str());
	CHECK(fixture.Read() == original.data);
	CheckAccount(*worlds.GetPlayerConfigTable(-1), "player", 0, 7);
}

TEST(PlayerConfig, AllByteLengthNamesCanBeLoadedAndSaved)
{
	for (size_t length : {1, 19, 20, 21, 255})
	{
		const std::string name(length, 'x');
		PlayerConfigTable table;
		CHECK(Load(table, Bytes().U32(1).Account(name, 2, 3)));
		CHECK_EQ(1, table.size());
		CheckAccount(table, name.c_str(), 2, 3);
		CHECK(Save(table) == Bytes().U32(1).Account(name, 2, 4).data);
	}
}

TEST(PlayerConfig, EveryTruncatedAccountTableLeavesNoPartialSettings)
{
	const auto complete = Bytes().U32(2).Account("first", 1, 4).Account("second", 2, 7);
	for (size_t end = 0; end < complete.data.size(); ++end)
	{
		PlayerConfigTable table;
		Add(table, "old", 0);
		auto partial = complete;
		partial.data.resize(end);
		CHECK(!Load(table, partial));
		CHECK(table.empty());
	}
}

TEST(PlayerConfig, AccountCountsMustFitTheRemainingBytes)
{
	for (std::uint32_t count : {2u, 0x7fffffffu, 0x80000000u})
	{
		PlayerConfigTable table;
		CHECK(!Load(table, Bytes().U32(count).Byte(0)));
		CHECK(table.empty());
	}
}

TEST(PlayerConfig, EveryTruncatedWorldFileLeavesNoPartialSettings)
{
	const auto complete = Bytes().U32(2).U32(2)
		.U32(1).U32(1).Account("first", 1, 4).U32(2).U32(1).Account("second", 2, 7);
	for (size_t end = 0; end < complete.data.size(); ++end)
	{
		WorldPlayerConfigTable worlds;
		Add(AddWorld(worlds, 9), "old", 0);
		auto partial = complete;
		partial.data.resize(end);
		ConfigFile fixture(partial);
		worlds.LoadFromFile(fixture.path.string().c_str());
		CHECK(worlds.empty());
	}
}

TEST(PlayerConfig, WorldCountsMustFitTheRemainingBytes)
{
	for (std::uint32_t count : {2u, 0x7fffffffu, 0x80000000u, 0xffffffffu})
	{
		WorldPlayerConfigTable worlds;
		ConfigFile fixture(Bytes().U32(2).U32(count).U32(0xffffffff));
		worlds.LoadFromFile(fixture.path.string().c_str());
		CHECK(worlds.empty());
	}
}

TEST(PlayerConfig, InvalidLaterWorldRecordsDiscardEarlierWorlds)
{
	for (const auto& bad : {Bytes().U32(1).Account("invalid", 3, 1),
		Bytes().U32(1).Account(std::string("a\0b", 3), 1, 1)})
	{
		auto bytes = Bytes().U32(2).U32(2).U32(1).U32(1).Account("valid", 1, 1).U32(2);
		bytes.data.insert(bytes.data.end(), bad.data.begin(), bad.data.end());
		ConfigFile fixture(bytes);
		WorldPlayerConfigTable worlds;
		worlds.LoadFromFile(fixture.path.string().c_str());
		CHECK(worlds.empty());
	}
}

TEST(PlayerConfig, NullRowsDoNotCrashTheRecentAccountSort)
{
	PlayerConfigTable table;
	for (int i = 0; i < 20; ++i)
		Add(table, AccountName(i), 1, i);
	table["null-a"] = nullptr;
	table["null-b"] = nullptr;
	PlayerConfigTable loaded;
	CHECK(Load(loaded, Bytes{Save(table)}));
	CHECK_EQ(20, loaded.size());
	for (int i = 0; i < 20; ++i)
		CheckAccount(loaded, AccountName(i).c_str(), 1, i + 1);
}

TEST(PlayerConfig, AddingTheSameAccountPointerAgainKeepsItAlive)
{
	PlayerConfigTable table;
	auto* config = Add(table, "player", 2, 8);
	table.AddPlayerConfig(config);
	CHECK_EQ(1, table.size());
	CHECK(table.GetPlayerConfig("player") == config);
	CheckAccount(table, "player", 2, 8);
}

TEST(PlayerConfig, AddingTheSameWorldPointerAgainKeepsItAlive)
{
	WorldPlayerConfigTable worlds;
	auto& table = AddWorld(worlds, 7);
	Add(table, "player", 2, 8);
	worlds.AddPlayerConfigTable(7, &table);
	CHECK_EQ(1, worlds.size());
	CHECK(worlds.GetPlayerConfigTable(7) == &table);
	CheckAccount(*worlds.GetPlayerConfigTable(7), "player", 2, 8);
}

TEST(PlayerConfig, EmptyTablesRoundTripAndReplacePreviousSettings)
{
	PlayerConfigTable accounts;
	CHECK(Save(accounts) == Bytes().U32(0).data);
	Add(accounts, "old", 0);
	CHECK(Load(accounts, Bytes().U32(0)));
	CHECK(accounts.empty());
	WorldPlayerConfigTable worlds;
	ConfigFile fixture;
	worlds.SaveToFile(fixture.path.string().c_str());
	CHECK(fixture.Read() == Bytes().U32(2).U32(0).data);
	Add(AddWorld(worlds, 1), "old", 0);
	worlds.LoadFromFile(fixture.path.string().c_str());
	CHECK(worlds.empty());
}

TEST(PlayerConfig, NullPlaceholdersKeepTheirLegacyEncodingAtBothLevels)
{
	WorldPlayerConfigTable worlds;
	auto& accounts = AddWorld(worlds, 1);
	accounts["absent"] = nullptr;
	Add(accounts, "player", 2, 8);
	worlds[2] = nullptr;
	ConfigFile fixture;
	worlds.SaveToFile(fixture.path.string().c_str());
	CHECK(fixture.Read() == Bytes().U32(2).U32(2)
		.U32(1).U32(2).Byte(0).Account("player", 2, 9).U32(0xffffffff).data);
	WorldPlayerConfigTable loaded;
	loaded.LoadFromFile(fixture.path.string().c_str());
	CHECK_EQ(1, loaded.size());
	const auto* table = loaded.GetPlayerConfigTable(1);
	CHECK(table != nullptr);
	if (table)
	{
		CHECK_EQ(1, table->size());
		CheckAccount(*table, "player", 2, 9);
	}
}

TEST(PlayerConfig, MissingSaveFilenameLeavesTheModelAlone)
{
	WorldPlayerConfigTable worlds;
	auto& accounts = AddWorld(worlds, 1);
	Add(accounts, "player", 1, 7);
	worlds.SaveToFile(nullptr);
	CHECK_EQ(1, worlds.size());
	CheckAccount(accounts, "player", 1, 7);
}
