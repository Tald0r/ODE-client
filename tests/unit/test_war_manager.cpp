#include "test_framework.h"
#include "MWarManager.h"
#include "GuildWarInfo.h"
#include "RaceWarInfo.h"
#include "LevelWarInfo.h"
#include "MZoneTable.h"
#include "UserInformation.h"
#include "MonotonicClock.h"

#include <initializer_list>
#include <string>
#include <vector>

namespace {

struct Event { char kind; size_t rows; DWORD start; };
std::vector<Event> events;
unsigned long long now = 1234567;
ZoneID_t currentZone = 71;
bool hasZone = true;
int zoneReads = 0;

MonotonicClock::TimePoint Clock() { return MonotonicClock::FromMillis(now); }
size_t Rows() { return g_pUserInformation ? g_pUserInformation->WarInfo.size() : 0; }

const MWarHost host{
	.ReadZone = [](ZoneID_t& id) {
		++zoneReads;
		id = currentZone;
		return hasZone;
	},
	.RaceWarNotice = [](DWORD start) { events.push_back({'N', Rows(), start}); },
	.RaceWarStarted = []() { events.push_back({'S', Rows(), 0}); },
	.RaceWarEnded = []() { events.push_back({'E', Rows(), 0}); },
};

struct World
{
	UserInformation user;
	CZoneTable zones;
	UserInformation* previousUser = g_pUserInformation;
	CZoneTable* previousZones = g_pZoneTable;
	const MWarHost* previousHost;
	MonotonicClock::ScopedTestSource clock{Clock};

	explicit World(const MWarHost* installed = &host)
		: previousHost(MWarManager::SetHost(installed))
	{
		g_pUserInformation = &user;
		g_pZoneTable = &zones;
		events.clear();
		now = 1234567;
		currentZone = 71;
		hasZone = true;
		zoneReads = 0;
		for (int id : {71, 72, 73, 1201, 1202, 1203, 1204, 1205, 1206})
		{
			auto* zone = new ZONETABLE_INFO;
			zone->ID = static_cast<TYPE_ZONEID>(id);
			zone->Name = ("Zone " + std::to_string(id)).c_str();
			CHECK(zones.Add(zone));
		}
	}
	~World()
	{
		MWarManager::SetHost(previousHost);
		g_pUserInformation = previousUser;
		g_pZoneTable = previousZones;
	}
};

template<class Base>
struct TrackedWar : Base
{
	int& destroyed;
	explicit TrackedWar(int& count) : destroyed(count)
	{
		this->setStartTime(456);
		this->setRemainTime(120);
	}
	~TrackedWar() override { ++destroyed; }
};

GuildWarInfo* Guild(ZoneID_t zone, int& destroyed)
{
	auto* war = new TrackedWar<GuildWarInfo>(destroyed);
	war->setCastleID(zone);
	war->setAttackGuildName("Attackers");
	war->setDefenseGuildName("Defenders");
	war->addJoinGuild(17);
	war->addJoinGuild(29);
	return war;
}

RaceWarInfo* MakeRaceWar(std::initializer_list<ZoneID_t> ids, int& destroyed)
{
	auto* war = new TrackedWar<RaceWarInfo>(destroyed);
	for (auto id : ids) war->addCastleID(id);
	return war;
}

} // namespace

TEST(WarManager, FreshManagerHasNoWars)
{
	World world;
	MWarManager manager;
	CHECK_EQ(0, manager.getSize());
	CHECK(manager.getWarInfoList().empty());
	for (ZoneID_t id : {0, 71, 1201, 1211, 1212, 65535})
	{
		CHECK(!manager.IsExist(id));
		CHECK(manager.GetWarInfo(id) == nullptr);
	}
}

TEST(WarManager, GuildWarRetainsItsPacketDataAndPublishesADisplayRow)
{
	World world;
	int destroyed = 0;
	MWarManager manager;
	auto* war = Guild(1201, destroyed);
	manager.SetWar(war);
	CHECK(manager.GetWarInfo(1201) == war);
	CHECK_EQ(2, war->getJoinGuilds().getSize());
	CHECK_EQ(1, manager.getSize());
	CHECK_EQ(1, world.user.WarInfo.size());
	const auto& row = world.user.WarInfo.at(0);
	CHECK_EQ(1201, row.zone_id);
	CHECK_EQ(WAR_GUILD, row.war_type);
	CHECK(row.zone_name == "Zone 1201");
	CHECK(row.attack_guild_name == "Attackers");
	CHECK(row.defense_guild_name == "Defenders");
	CHECK_EQ(1354, row.left_time.time_since_epoch().count());
	CHECK(events.empty());
	CHECK_EQ(0, zoneReads);
}

TEST(WarManager, RaceWarSharesOneRecordAcrossZonesAndOrdersNotifications)
{
	World world;
	int destroyed = 0;
	MWarManager manager;
	auto* war = MakeRaceWar({71, 72}, destroyed);
	manager.SetWar(war);
	CHECK(manager.GetWarInfo(71) == war);
	CHECK(manager.GetWarInfo(72) == war);
	CHECK_EQ(2, manager.getSize());
	CHECK(war->getCastleIDs().IsEmpty());
	CHECK_EQ(2, world.user.WarInfo.size());
	for (const auto& row : world.user.WarInfo)
	{
		CHECK_EQ(WAR_RACE, row.war_type);
		CHECK(row.zone_name == "Zone " + std::to_string(row.zone_id));
		CHECK_EQ(1354, row.left_time.time_since_epoch().count());
		CHECK(row.attack_guild_name.empty());
	}
	CHECK_EQ(2, events.size());
	CHECK_EQ('N', events.at(0).kind);
	CHECK_EQ(0, events.at(0).rows);
	CHECK_EQ(456, events.at(0).start);
	CHECK_EQ('S', events.at(1).kind);
	CHECK_EQ(2, events.at(1).rows);
}

TEST(WarManager, CastleInteriorsResolveToAllSixCastleRecords)
{
	World world;
	int destroyed = 0;
	MWarManager manager;
	for (int castle = 1; castle <= 6; ++castle)
	{
		const auto id = static_cast<ZoneID_t>(1200 + castle);
		auto* war = Guild(id, destroyed);
		manager.SetWar(war);
		for (int floor : {1, 2})
		{
			const auto interior = static_cast<ZoneID_t>(1200 + castle * 10 + floor);
			CHECK(manager.IsExist(interior));
			CHECK(manager.GetWarInfo(interior) == war);
		}
	}
	CHECK_EQ(6, manager.getSize());
	CHECK(!manager.IsExist(1271));
	CHECK(manager.GetWarInfo(1213) == nullptr);
}

TEST(WarManager, ClearWarDeletesEachSharedRecordOnceAndClearsDisplayRows)
{
	World world;
	int destroyed = 0;
	MWarManager manager;
	manager.SetWar(MakeRaceWar({71, 72}, destroyed));
	manager.SetWar(Guild(1201, destroyed));
	manager.ClearWar();
	CHECK_EQ(2, destroyed);
	CHECK_EQ(0, manager.getSize());
	CHECK(world.user.WarInfo.empty());
	manager.ClearWar();
	CHECK_EQ(2, destroyed);
}

TEST(WarManager, DestructionClearsOwnedRecordsAndDisplayRows)
{
	World world;
	int destroyed = 0;
	{
		MWarManager manager;
		manager.SetWar(MakeRaceWar({71, 72, 73}, destroyed));
		manager.SetWar(Guild(1202, destroyed));
	}
	CHECK_EQ(2, destroyed);
	CHECK(world.user.WarInfo.empty());
}

TEST(WarManager, RemovingASingleZoneRaceWarClearsItsRow)
{
	World world;
	int destroyed = 0;
	MWarManager manager;
	manager.SetWar(MakeRaceWar({71}, destroyed));
	manager.RemoveWar(71);
	CHECK_EQ(1, destroyed);
	CHECK(!manager.IsExist(71));
	CHECK(world.user.WarInfo.empty());
	manager.RemoveWar(71);
	CHECK_EQ(1, destroyed);
}

TEST(WarManager, GuildRefreshKeepsOneRowAndUpdatesItsDeadline)
{
	World world;
	int destroyed = 0;
	MWarManager manager;
	auto* war = Guild(1201, destroyed);
	manager.SetWar(war);
	now = 2000123;
	war->setRemainTime(17);
	manager.SetWar(war);
	CHECK_EQ(1, manager.getSize());
	CHECK_EQ(1, world.user.WarInfo.size());
	CHECK_EQ(2017, world.user.WarInfo.at(0).left_time.time_since_epoch().count());
	CHECK_EQ(0, destroyed);
}

TEST(WarManager, ClearingRaceWarsPreservesGuildsAndNotifiesAfterRows)
{
	World world;
	int destroyed = 0;
	MWarManager manager;
	auto* war = Guild(1201, destroyed);
	manager.SetWar(war);
	manager.ClearRaceWar();
	CHECK(manager.GetWarInfo(1201) == war);
	CHECK_EQ(1, world.user.WarInfo.size());
	CHECK_EQ(0, destroyed);
	CHECK_EQ(1, events.size());
	CHECK_EQ('E', events.at(0).kind);
	CHECK_EQ(1, events.at(0).rows);
}

TEST(WarManager, MissingAndEmptyHostsStillMaintainRaceWarState)
{
	const MWarHost empty;
	for (const MWarHost* installed : {static_cast<const MWarHost*>(nullptr), &empty})
	{
		World world(installed);
		int destroyed = 0;
		MWarManager manager;
		manager.SetWar(MakeRaceWar({71, 72}, destroyed));
		CHECK_EQ(2, manager.getSize());
		CHECK_EQ(2, world.user.WarInfo.size());
		CHECK(events.empty());
		manager.ClearWar();
		CHECK_EQ(1, destroyed);
	}
}

TEST(WarManager, NullWarDoesNotPublishOrNotify)
{
	World world;
	MWarManager manager;
	manager.SetWar(nullptr);
	CHECK_EQ(0, manager.getSize());
	CHECK(world.user.WarInfo.empty());
	CHECK(events.empty());
}
