#include "test_framework.h"
#include "packet_stream_access.h"
#include "BonusSkillHost.h"
#include "MSkillManager.h"
#include "UserInformation.h"
#include "Gpackets/GCHolyLandBonusInfo.h"
#include "Gpackets/GCSweeperBonusInfo.h"
#include "Gpackets/GCSkillInfo.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"

#include <algorithm>
#include <array>
#include <memory>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
BonusSkills::PlayerState player;
bool playerAvailable;
int refreshes;
std::array<bool, HOLYLAND_BONUS_MAX> publishedHoly;
std::array<bool, SWEEPER_BONUS_MAX> publishedSweeper;
const BonusSkills::Host host{
	.ReadPlayer = [](BonusSkills::PlayerState& result) { result = player; return playerAvailable; },
	.RefreshAvailableSkills = [] {
		++refreshes;
		std::copy_n(g_abHolyLandBonusSkills, HOLYLAND_BONUS_MAX, publishedHoly.begin());
		std::copy_n(g_abSweeperBonusSkills, SWEEPER_BONUS_MAX, publishedSweeper.begin());
	},
};

struct World
{
	const BonusSkills::Host* previous = BonusSkills::SetHost(&host);
	std::array<bool, HOLYLAND_BONUS_MAX> oldHoly;
	std::array<bool, SWEEPER_BONUS_MAX> oldSweeper;
	World()
	{
		std::copy_n(g_abHolyLandBonusSkills, HOLYLAND_BONUS_MAX, oldHoly.begin());
		std::copy_n(g_abSweeperBonusSkills, SWEEPER_BONUS_MAX, oldSweeper.begin());
		std::fill_n(g_abHolyLandBonusSkills, HOLYLAND_BONUS_MAX, false);
		std::fill_n(g_abSweeperBonusSkills, SWEEPER_BONUS_MAX, false);
		player = {RACE_SLAYER, 20, 150};
		playerAvailable = true;
		refreshes = 0;
		publishedHoly.fill(false);
		publishedSweeper.fill(false);
	}
	~World()
	{
		std::copy(oldHoly.begin(), oldHoly.end(), g_abHolyLandBonusSkills);
		std::copy(oldSweeper.begin(), oldSweeper.end(), g_abSweeperBonusSkills);
		BonusSkills::SetHost(previous);
	}
};

template<class PacketType, class Factory>
std::unique_ptr<PacketType> Parse(const std::vector<BYTE>& bytes)
{
	Factory factory;
	// These regressions use packets admitted by the connection's factory cap.
	CHECK(bytes.size() <= factory.getPacketMaxSize());
	std::unique_ptr<PacketType> packet(static_cast<PacketType*>(factory.createPacket()));
	Socket socket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&socket, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned>(bytes.size()));
	packet->read(stream);
	CHECK(stream.isEmpty());
	CHECK_EQ(bytes.size(), packet->getPacketSize());
	return packet;
}

template<class PacketType, class Factory>
std::unique_ptr<PacketType> Read(const std::vector<BYTE>& races)
{
	std::vector<BYTE> bytes{static_cast<BYTE>(races.size())};
	bytes.insert(bytes.end(), races.begin(), races.end());
	return Parse<PacketType, Factory>(bytes);
}

void Holy(const std::vector<BYTE>& races)
{
	auto packet = Read<GCHolyLandBonusInfo, GCHolyLandBonusInfoFactory>(races);
	GCHolyLandBonusInfoHandler::execute(packet.get(), nullptr);
	CHECK_EQ(0, packet->getListNum());
}

void Sweeper(const std::vector<BYTE>& races)
{
	auto packet = Read<GCSweeperBonusInfo, GCSweeperBonusInfoFactory>(races);
	GCSweeperBonusInfoHandler::execute(packet.get(), nullptr);
	CHECK_EQ(0, packet->getListNum());
}
}

TEST(BonusSkillHandlers, SweeperLevelChangesRemoveThePreviousBracket)
{
	World world;
	player.race = RACE_VAMPIRE;
	player.level = 30;
	const std::vector<BYTE> races(12, RACE_VAMPIRE);
	Sweeper(races);
	CHECK(g_abSweeperBonusSkills[0]);
	player.level = 31;
	Sweeper(races);
	for (int i = 0; i < 12; ++i) CHECK_EQ(i >= 3 && i < 6, g_abSweeperBonusSkills[i]);
	CHECK_EQ(2, refreshes);
	CHECK(!publishedSweeper[0]);
	CHECK(publishedSweeper[3]);
}

TEST(BonusSkillHandlers, SweeperStatChangesRemoveThePreviousBracket)
{
	World world;
	const std::vector<BYTE> races(12, RACE_SLAYER);
	Sweeper(races);
	CHECK(g_abSweeperBonusSkills[0]);
	player.statSum = 151;
	Sweeper(races);
	for (int i = 0; i < 12; ++i) CHECK_EQ(i >= 3 && i < 6, g_abSweeperBonusSkills[i]);
}

TEST(BonusSkillHandlers, HolyLandSnapshotsClearOmittedBonusesIncludingEmptyLists)
{
	World world;
	Holy(std::vector<BYTE>(12, RACE_SLAYER));
	CHECK(g_abHolyLandBonusSkills[11]);
	Holy({RACE_SLAYER});
	for (int i = 0; i < 12; ++i) CHECK_EQ(i == 0, g_abHolyLandBonusSkills[i]);
	Holy({});
	for (bool active : g_abHolyLandBonusSkills) CHECK(!active);
	CHECK_EQ(3, refreshes);
}

TEST(BonusSkillHandlers, SweeperSnapshotsClearOmittedBonusesIncludingEmptyLists)
{
	World world;
	Sweeper(std::vector<BYTE>(12, RACE_SLAYER));
	CHECK(g_abSweeperBonusSkills[2]);
	Sweeper({RACE_SLAYER});
	for (int i = 0; i < 12; ++i) CHECK_EQ(i == 0, g_abSweeperBonusSkills[i]);
	Sweeper({});
	for (bool active : g_abSweeperBonusSkills) CHECK(!active);
	CHECK_EQ(3, refreshes);
}

TEST(BonusSkillHandlers, HolyLandMatchesAllThreeRacesAndRejectsTheUnownedRace)
{
	World world;
	const std::vector<BYTE> owners{0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};
	for (int race : {RACE_SLAYER, RACE_VAMPIRE, RACE_OUSTERS})
	{
		player.race = race;
		Holy(owners);
		for (size_t i = 0; i < owners.size(); ++i)
		{
			CHECK_EQ(owners[i] == race, g_abHolyLandBonusSkills[i]);
			CHECK_EQ(owners[i] == race, publishedHoly[i]);
		}
	}
}

TEST(BonusSkillHandlers, EverySlayerStatBoundaryReplacesAllPreviousEligibility)
{
	World world;
	struct Case { std::int64_t value; int bracket; };
	const Case cases[] = {{-1, -1}, {0, -1}, {1, 0}, {150, 0}, {151, 1},
		{210, 1}, {211, 2}, {260, 2}, {261, 3}, {300, 3}, {301, -1},
		{(std::numeric_limits<std::int64_t>::min)(), -1}, {(std::numeric_limits<std::int64_t>::max)(), -1}};
	for (const auto& test : cases)
	{
		std::fill_n(g_abSweeperBonusSkills, SWEEPER_BONUS_MAX, true);
		player.statSum = test.value;
		Sweeper(std::vector<BYTE>(12, RACE_SLAYER));
		for (int i = 0; i < 12; ++i) CHECK_EQ(i / 3 == test.bracket, g_abSweeperBonusSkills[i]);
	}
}

TEST(BonusSkillHandlers, EveryVampireAndOustersLevelBoundaryReplacesAllPreviousEligibility)
{
	World world;
	struct Case { int value; int bracket; };
	const Case cases[] = {{-1, -1}, {0, -1}, {1, 0}, {30, 0}, {31, 1},
		{50, 1}, {51, 2}, {70, 2}, {71, 3}, {90, 3}, {91, -1},
		{(std::numeric_limits<int>::min)(), -1}, {(std::numeric_limits<int>::max)(), -1}};
	for (int race : {RACE_VAMPIRE, RACE_OUSTERS})
		for (const auto& test : cases)
		{
			std::fill_n(g_abSweeperBonusSkills, SWEEPER_BONUS_MAX, true);
			player.race = race;
			player.level = test.value;
			Sweeper(std::vector<BYTE>(12, static_cast<BYTE>(race)));
			for (int i = 0; i < 12; ++i) CHECK_EQ(i / 3 == test.bracket, g_abSweeperBonusSkills[i]);
		}
}

TEST(BonusSkillHandlers, EveryWireRaceByteHasExplicitSweeperEligibility)
{
	World world;
	for (int race : {RACE_SLAYER, RACE_VAMPIRE, RACE_OUSTERS})
		for (int owner = 0; owner <= 255; ++owner)
		{
			player.race = race;
			Sweeper(std::vector<BYTE>(12, static_cast<BYTE>(owner)));
			for (int i = 0; i < 12; ++i) CHECK_EQ(owner == race && i < 3, g_abSweeperBonusSkills[i]);
		}
	player.race = 3;
	Sweeper(std::vector<BYTE>(12, 3));
	for (bool active : g_abSweeperBonusSkills) CHECK(!active);
}

TEST(BonusSkillHandlers, MissingPlayerServicesLeaveStateAndPacketsUntouched)
{
	World world;
	const BonusSkills::Host empty{};
	const BonusSkills::Host noPlayer{.RefreshAvailableSkills = host.RefreshAvailableSkills};
	for (const auto* service : {static_cast<const BonusSkills::Host*>(nullptr), &empty, &noPlayer, &host})
	{
		std::fill_n(g_abHolyLandBonusSkills, HOLYLAND_BONUS_MAX, true);
		std::fill_n(g_abSweeperBonusSkills, SWEEPER_BONUS_MAX, true);
		BonusSkills::SetHost(service);
		playerAvailable = false;
		auto holy = Read<GCHolyLandBonusInfo, GCHolyLandBonusInfoFactory>({RACE_VAMPIRE});
		auto sweeper = Read<GCSweeperBonusInfo, GCSweeperBonusInfoFactory>({RACE_VAMPIRE});
		GCHolyLandBonusInfoHandler::execute(holy.get(), nullptr);
		GCSweeperBonusInfoHandler::execute(sweeper.get(), nullptr);
		CHECK_EQ(1, holy->getListNum());
		CHECK_EQ(1, sweeper->getListNum());
		for (bool active : g_abHolyLandBonusSkills) CHECK(active);
		for (bool active : g_abSweeperBonusSkills) CHECK(active);
		CHECK_EQ(0, refreshes);
	}
	BonusSkills::SetHost(nullptr);
}

TEST(BonusSkillHandlers, MissingRefreshStillAppliesAndConsumesBothPackets)
{
	World world;
	const BonusSkills::Host noRefresh{.ReadPlayer = host.ReadPlayer};
	CHECK(BonusSkills::SetHost(&noRefresh) == &host);
	Holy({RACE_SLAYER});
	Sweeper({RACE_SLAYER});
	CHECK(g_abHolyLandBonusSkills[0]);
	CHECK(g_abSweeperBonusSkills[0]);
	CHECK_EQ(0, refreshes);
	CHECK(BonusSkills::SetHost(nullptr) == &noRefresh);
}

TEST(BonusSkillHandlers, PlayerCallbackCanReplaceTheRefreshHost)
{
	World world;
	const BonusSkills::Host replacing{
		.ReadPlayer = [](BonusSkills::PlayerState& result) {
			BonusSkills::SetHost(&host);
			return host.ReadPlayer(result);
		},
	};
	BonusSkills::SetHost(&replacing);
	Holy({RACE_SLAYER});
	BonusSkills::SetHost(&replacing);
	Sweeper({RACE_SLAYER});
	CHECK_EQ(2, refreshes);
	CHECK(publishedHoly[0]);
	CHECK(publishedSweeper[0]);
}

TEST(BonusSkillHandlers, RefreshExceptionsLeaveCompleteStateAndConsumedPackets)
{
	World world;
	const BonusSkills::Host throwing{
		.ReadPlayer = host.ReadPlayer,
		.RefreshAvailableSkills = [] { throw std::runtime_error("refresh"); },
	};
	BonusSkills::SetHost(&throwing);
	auto holy = Read<GCHolyLandBonusInfo, GCHolyLandBonusInfoFactory>({RACE_SLAYER});
	auto sweeper = Read<GCSweeperBonusInfo, GCSweeperBonusInfoFactory>({RACE_SLAYER});
	bool threw = false;
	try { GCHolyLandBonusInfoHandler::execute(holy.get(), nullptr); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw);
	CHECK_EQ(0, holy->getListNum());
	CHECK(g_abHolyLandBonusSkills[0]);
	threw = false;
	try { GCSweeperBonusInfoHandler::execute(sweeper.get(), nullptr); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw);
	CHECK_EQ(0, sweeper->getListNum());
	CHECK(g_abSweeperBonusSkills[0]);
	BonusSkills::SetHost(nullptr);
}

TEST(BonusSkillHandlers, OversizedDirectCallsDrainExtraRowsWithoutIndexingPastTheModel)
{
	World world;
	// Constructed directly: normal connection framing refuses more than 12 rows.
	GCHolyLandBonusInfo holy;
	GCSweeperBonusInfo sweeper;
	for (int i = 0; i < 255; ++i)
	{
		auto holyRow = std::make_unique<BloodBibleBonusInfo>();
		holyRow->setRace(RACE_SLAYER);
		holy.addBloodBibleBonusInfo(holyRow.get());
		holyRow.release();
		auto sweeperRow = std::make_unique<SweeperBonusInfo>();
		sweeperRow->setRace(RACE_SLAYER);
		sweeper.addSweeperBonusInfo(sweeperRow.get());
		sweeperRow.release();
	}
	GCHolyLandBonusInfoHandler::execute(&holy, nullptr);
	GCSweeperBonusInfoHandler::execute(&sweeper, nullptr);
	CHECK_EQ(0, holy.getListNum());
	CHECK_EQ(0, sweeper.getListNum());
	for (int i = 0; i < 12; ++i)
	{
		CHECK(g_abHolyLandBonusSkills[i]);
		CHECK_EQ(i < 3, g_abSweeperBonusSkills[i]);
	}
}

TEST(BonusSkillHandlers, SkillSnapshotRebuildsTheModelAndClearsOnlySweeperBeforeRefresh)
{
	World world;
	UserInformation user;
	MSkillInfoTable info;
	MSkillManager manager;
	struct Restore
	{
		UserInformation* user = g_pUserInformation;
		MSkillInfoTable* info = g_pSkillInfoTable;
		MSkillManager* manager = g_pSkillManager;
		~Restore() { g_pUserInformation = user; g_pSkillInfoTable = info; g_pSkillManager = manager; }
	} restore;
	g_pUserInformation = &user;
	g_pSkillInfoTable = &info;
	g_pSkillManager = &manager;
	manager.Init();
	const BonusSkills::Host checking{
		.RefreshAvailableSkills = [] {
			CHECK(!g_pUserInformation->HasSkillRestore);
			CHECK(!g_pUserInformation->HasMagicGroundAttack);
			CHECK(!g_pUserInformation->HasMagicHallu);
			CHECK(!g_pUserInformation->HasMagicBloodyWarp);
			CHECK(!g_pUserInformation->HasMagicBloodySnake);
			host.RefreshAvailableSkills();
		},
	};
	for (BYTE race : {BYTE(PC_SLAYER), BYTE(PC_VAMPIRE), BYTE(PC_OUSTERS)})
	{
		std::fill_n(g_abHolyLandBonusSkills, HOLYLAND_BONUS_MAX, true);
		std::fill_n(g_abSweeperBonusSkills, SWEEPER_BONUS_MAX, true);
		user.HasSkillRestore = user.HasMagicGroundAttack = user.HasMagicHallu = true;
		user.HasMagicBloodyWarp = user.HasMagicBloodySnake = true;
		BonusSkills::SetHost(&checking);
		auto packet = Parse<GCSkillInfo, GCSkillInfoFactory>({race, 0});
		GCSkillInfoHandler::execute(packet.get(), nullptr);
		CHECK_EQ(0, packet->getListNum());
		for (bool active : publishedHoly) CHECK(active);
		for (bool active : publishedSweeper) CHECK(!active);
	}
	CHECK_EQ(3, refreshes);
	BonusSkills::SetHost(nullptr);
	std::fill_n(g_abSweeperBonusSkills, SWEEPER_BONUS_MAX, true);
	auto packet = Parse<GCSkillInfo, GCSkillInfoFactory>({PC_SLAYER, 0});
	GCSkillInfoHandler::execute(packet.get(), nullptr);
	for (bool active : g_abSweeperBonusSkills) CHECK(!active);
	CHECK_EQ(3, refreshes);
}
