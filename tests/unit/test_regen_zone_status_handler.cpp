#include "test_framework.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "RegenZoneStatusHost.h"
#include "ShrineInfoManager.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "Gpackets/GCRegenZoneStatus.h"
#include "RaceType.h"

#include <array>

namespace {
RegenTowerInfoManager* s_Table = nullptr;
int s_TableReads = 0;

RegenTowerInfoManager* ReadTable()
{
	++s_TableReads;
	return s_Table;
}

struct Fixture
{
	RegenTowerInfoManager table;
	RegenZoneStatus::Host host{.Table = ReadTable};
	const RegenZoneStatus::Host* previous = RegenZoneStatus::SetHost(&host);
	RegenTowerInfoManager* previousTable = s_Table;

	Fixture()
	{
		s_Table = &table;
		s_TableReads = 0;
	}

	~Fixture()
	{
		RegenZoneStatus::SetHost(previous);
		s_Table = previousTable;
	}
};

void Apply(const std::array<unsigned char, 8>& statuses)
{
	Socket socket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&socket, 1024);
	SocketInputStreamTestAccess::Preload(stream, statuses.data(),
		static_cast<unsigned int>(statuses.size()));
	GCRegenZoneStatus packet;
	packet.read(stream);
	CHECK(stream.isEmpty());
	CHECK_EQ(statuses.size(), static_cast<size_t>(packet.getPacketSize()));
	GCRegenZoneStatusHandler::execute(&packet, nullptr);
}
}

TEST(RegenZoneStatusHandler, MissingHostCallbackOrTableSkipsUpdates)
{
	Fixture f;
	f.table.Init(1);
	testfw::MutableRow(f.table, 0).owner = 73;
	CHECK(RegenZoneStatus::SetHost(nullptr) == &f.host);
	Apply({0, 1, 2, 3, 4, 5, 6, 7});
	const RegenZoneStatus::Host empty{};
	CHECK(RegenZoneStatus::SetHost(&empty) == nullptr);
	Apply({0, 1, 2, 3, 4, 5, 6, 7});
	CHECK_EQ(0, s_TableReads);
	CHECK(RegenZoneStatus::SetHost(&f.host) == &empty);
	s_Table = nullptr;
	Apply({0, 1, 2, 3, 4, 5, 6, 7});
	CHECK_EQ(1, s_TableReads);
	CHECK_EQ(73, f.table[0].owner);
}

TEST(RegenZoneStatusHandler, EmptyShortAndFullTablesApplyOnlyAllocatedRows)
{
	Fixture f;
	const std::array<unsigned char, 8> statuses{0, 1, 2, 3, 127, 128, 254, 255};
	for (int count : {0, 1, 7, 8, 9, 12, 13, RegenTowerInfo::MaxCount})
	{
		f.table.Init(count);
		for (int i = 0; i < count; ++i)
		{
			auto& row = testfw::MutableRow(f.table, i);
			row.num = i;
			row.zoneID = 71 + i % 3;
			row.x = i % 128;
			row.y = i;
			row.owner = 73;
		}
		Apply(statuses);
		CHECK_EQ(count, f.table.GetSize());
		for (int i = 0; i < count; ++i)
		{
			const auto& row = f.table[i];
			int expected = RACE_OUSTERS;
			if (i < 8) expected = statuses[i];
			else if (i < 12) expected = i % 2 ? RACE_VAMPIRE : RACE_SLAYER;
			CHECK_EQ(expected, row.owner);
			CHECK_EQ(i, row.num);
			CHECK_EQ(71 + i % 3, row.zoneID);
			CHECK_EQ(i % 128, row.x);
			CHECK_EQ(i, row.y);
		}
		CHECK_EQ(-1, f.table[-1].owner);
		CHECK_EQ(-1, f.table[count].owner);
	}
	CHECK_EQ(8, s_TableReads);
}

TEST(RegenZoneStatusHandler, RepeatedPacketsOverwriteStatusesAndRestoreFixedTail)
{
	Fixture f;
	f.table.Init(14);
	Apply({0, 1, 2, 3, 4, 5, 6, 7});
	for (int i = 8; i < 14; ++i) testfw::MutableRow(f.table, i).owner = 73;
	Apply({255, 254, 253, 252, 251, 250, 249, 248});
	for (int i = 0; i < 8; ++i) CHECK_EQ(255 - i, f.table[i].owner);
	CHECK_EQ(RACE_SLAYER, f.table[8].owner);
	CHECK_EQ(RACE_VAMPIRE, f.table[9].owner);
	CHECK_EQ(RACE_SLAYER, f.table[10].owner);
	CHECK_EQ(RACE_VAMPIRE, f.table[11].owner);
	CHECK_EQ(RACE_OUSTERS, f.table[12].owner);
	CHECK_EQ(RACE_OUSTERS, f.table[13].owner);
	CHECK_EQ(2, s_TableReads);
}

TEST(RegenZoneStatusHandler, HostAndBorrowedTableAreReadForEveryPacket)
{
	Fixture f;
	f.table.Init(1);
	RegenTowerInfoManager alternate;
	alternate.Init(1);
	Apply({1, 0, 0, 0, 0, 0, 0, 0});
	s_Table = &alternate;
	Apply({2, 0, 0, 0, 0, 0, 0, 0});
	CHECK_EQ(1, f.table[0].owner);
	CHECK_EQ(2, alternate[0].owner);
	const RegenZoneStatus::Host empty{};
	CHECK(RegenZoneStatus::SetHost(&empty) == &f.host);
	Apply({3, 0, 0, 0, 0, 0, 0, 0});
	CHECK_EQ(2, alternate[0].owner);
	CHECK(RegenZoneStatus::SetHost(&f.host) == &empty);
	Apply({4, 0, 0, 0, 0, 0, 0, 0});
	CHECK_EQ(4, alternate[0].owner);
	CHECK_EQ(3, s_TableReads);
}
