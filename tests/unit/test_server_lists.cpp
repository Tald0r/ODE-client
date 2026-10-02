#include "test_framework.h"
#include "packet_stream_access.h"
#include "ApplyServerList.h"
#include "CServerInformation.h"
#include "Lpackets/LCWorldList.h"
#include "Lpackets/LCServerList.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"

#include <memory>
#include <string>
#include <vector>

namespace {

struct Row
{
	BYTE id;
	std::string name;
	BYTE status;
};

// Both login lists carry [requested ID][count], then [ID][name length]
// [name bytes][status] per row. Build wire bytes independently of write().
template<class PacketType, class Factory>
std::unique_ptr<PacketType> ReadList(BYTE requested, const std::vector<Row>& rows)
{
	static_assert(sizeof(WorldID_t) == 1 && sizeof(ServerGroupID_t) == 1);
	CHECK(rows.size() <= 255);
	std::vector<unsigned char> bytes = {requested, static_cast<BYTE>(rows.size())};
	for (const auto& row : rows)
	{
		CHECK(row.name.size() <= 255);
		bytes.push_back(row.id);
		bytes.push_back(static_cast<BYTE>(row.name.size()));
		bytes.insert(bytes.end(), row.name.begin(), row.name.end());
		bytes.push_back(row.status);
	}

	Factory factory;
	std::unique_ptr<PacketType> packet(static_cast<PacketType*>(factory.createPacket()));
	Socket socket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&socket, 2048);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	packet->read(stream);
	CHECK(stream.isEmpty());
	CHECK_EQ(rows.size(), packet->getListNum());
	CHECK_EQ(bytes.size(), packet->getPacketSize());
	return packet;
}

std::unique_ptr<LCWorldList> Worlds(BYTE requested, const std::vector<Row>& rows)
{
	return ReadList<LCWorldList, LCWorldListFactory>(requested, rows);
}

std::unique_ptr<LCServerList> Servers(BYTE requested, const std::vector<Row>& rows)
{
	return ReadList<LCServerList, LCServerListFactory>(requested, rows);
}

void SeedSelection(CServerInformation& selection)
{
	auto worlds = Worlds(12, {{12, "Old world", 3}, {23, "Other world", 4}});
	ApplyWorldList(selection, *worlds);
	auto servers = Servers(7, {{7, "Old server", 5}, {8, "Retained server", 6}});
	CHECK(ApplyServerList(selection, *servers));
}

void CheckWorld(CServerInformation& selection, unsigned int id, const char* name, int status)
{
	const ServerGroup* world = selection.GetData(id);
	CHECK(world != nullptr);
	if (world == nullptr)
		return;
	CHECK(std::string(world->GetGroupName()) == name);
	CHECK_EQ(status, world->GetGroupStatus());
}

void CheckServer(ServerGroup& world, unsigned int id, const char* name, int status)
{
	const SERVER_INFO* server = world.GetData(id);
	CHECK(server != nullptr);
	if (server == nullptr)
		return;
	CHECK(std::string(server->ServerName.GetString()) == name);
	CHECK_EQ(status, server->ServerStatus);
}

} // namespace

TEST(ServerLists, WorldListReplacesOldWorldsAndServersAndSelectsTheRequestedWorld)
{
	CServerInformation selection;
	SeedSelection(selection);
	auto packet = Worlds(9, {{42, "First", 1}, {9, "Selected", 255}});
	ApplyWorldList(selection, *packet);
	CHECK_EQ(0, packet->getListNum());
	packet.reset();
	CHECK_EQ(2, selection.size());
	CHECK(selection.GetData(12) == nullptr);
	CHECK(selection.GetData(23) == nullptr);
	CheckWorld(selection, 42, "First", 1);
	CheckWorld(selection, 9, "Selected", 255);
	CHECK(selection.GetData(9)->empty());
	CHECK_EQ(9, selection.GetServerGroupID());
	CHECK_EQ(255, selection.GetServerGroupStatus());
	CHECK(std::string(selection.GetServerGroupName()) == "Selected");
	CHECK_EQ(0, selection.GetServerID());
	CHECK_EQ(0, selection.GetServerStatus());
	CHECK(selection.GetServerName() == nullptr);
}

TEST(ServerLists, WorldFallbackUsesPacketOrderAndZeroMeansNoPreference)
{
	for (BYTE requested : {0, 99})
	{
		CServerInformation selection;
		auto packet = Worlds(requested, {{42, "First", 1}, {0, "Zero", 2}, {9, "Last", 3}});
		ApplyWorldList(selection, *packet);
		CHECK_EQ(0, packet->getListNum());
		CHECK_EQ(42, selection.GetServerGroupID());
		CHECK_EQ(1, selection.GetServerGroupStatus());
		CHECK(std::string(selection.GetServerGroupName()) == "First");
	}
}

TEST(ServerLists, DuplicateWorldsKeepTheLastMetadataAndFirstFallbackID)
{
	CServerInformation selection;
	auto packet = Worlds(99, {{42, "Earlier", 1}, {9, "Other", 2}, {42, "Later", 3}});
	ApplyWorldList(selection, *packet);
	CHECK_EQ(0, packet->getListNum());
	CHECK_EQ(2, selection.size());
	CheckWorld(selection, 42, "Later", 3);
	CHECK_EQ(42, selection.GetServerGroupID());
	CHECK_EQ(3, selection.GetServerGroupStatus());
	CHECK(std::string(selection.GetServerGroupName()) == "Later");
}

TEST(ServerLists, EmptyWorldListClearsTheEntireSelection)
{
	CServerInformation selection;
	SeedSelection(selection);
	auto packet = Worlds(12, {});
	ApplyWorldList(selection, *packet);
	CHECK_EQ(0, packet->getListNum());
	CHECK(selection.empty());
	CHECK_EQ(0, selection.GetServerGroupID());
	CHECK_EQ(0, selection.GetServerGroupStatus());
	CHECK_EQ(0, selection.GetServerID());
	CHECK_EQ(0, selection.GetServerStatus());
	CHECK(selection.GetServerGroupName() == nullptr);
	CHECK(selection.GetServerName() == nullptr);
}

TEST(ServerLists, ServerListUpdatesSelectedWorldAndRetainsUnlistedServers)
{
	CServerInformation selection;
	SeedSelection(selection);
	ServerGroup* world = selection.GetData(12);
	SERVER_INFO* retained = world->GetData(8);
	SERVER_INFO* updated = world->GetData(7);
	auto packet = Servers(9, {{7, "Updated", 255}, {9, "Selected", 2}});
	CHECK(ApplyServerList(selection, *packet));
	CHECK_EQ(0, packet->getListNum());
	packet.reset();
	CHECK_EQ(3, world->size());
	CHECK(world->GetData(7) == updated);
	CHECK(world->GetData(8) == retained);
	CheckServer(*world, 7, "Updated", 255);
	CheckServer(*world, 8, "Retained server", 6);
	CheckServer(*world, 9, "Selected", 2);
	CHECK(selection.GetData(23)->empty());
	CHECK_EQ(12, selection.GetServerGroupID());
	CHECK_EQ(3, selection.GetServerGroupStatus());
	CHECK(std::string(selection.GetServerGroupName()) == "Old world");
	CHECK_EQ(9, selection.GetServerID());
	CHECK_EQ(2, selection.GetServerStatus());
	CHECK(std::string(selection.GetServerName()) == "Selected");
}

TEST(ServerLists, ServerFallbackRequiresTheRequestedIDInThisPacket)
{
	// Server 7 already exists, but must not be selected if absent from the list.
	for (BYTE requested : {0, 7, 99})
	{
		CServerInformation selection;
		SeedSelection(selection);
		auto packet = Servers(requested, {{42, "First", 1}, {0, "Zero", 2}, {9, "Last", 3}});
		CHECK(ApplyServerList(selection, *packet));
		CHECK_EQ(0, packet->getListNum());
		CHECK_EQ(42, selection.GetServerID());
		CHECK_EQ(1, selection.GetServerStatus());
		CHECK(std::string(selection.GetServerName()) == "First");
	}
}

TEST(ServerLists, DuplicateServersKeepTheLastMetadataAndFirstFallbackID)
{
	CServerInformation selection;
	SeedSelection(selection);
	auto packet = Servers(99, {{42, "Earlier", 1}, {9, "Other", 2}, {42, "Later", 3}});
	CHECK(ApplyServerList(selection, *packet));
	CHECK_EQ(0, packet->getListNum());
	CHECK_EQ(4, selection.GetData(12)->size());
	CheckServer(*selection.GetData(12), 42, "Later", 3);
	CHECK_EQ(42, selection.GetServerID());
	CHECK_EQ(3, selection.GetServerStatus());
	CHECK(std::string(selection.GetServerName()) == "Later");
}

TEST(ServerLists, EmptyServerListIsAcceptedAndRetainsThePreviousServer)
{
	CServerInformation selection;
	SeedSelection(selection);
	auto packet = Servers(99, {});
	CHECK(ApplyServerList(selection, *packet));
	CHECK_EQ(0, packet->getListNum());
	CHECK_EQ(2, selection.GetData(12)->size());
	CHECK_EQ(7, selection.GetServerID());
	CHECK_EQ(5, selection.GetServerStatus());
	CHECK(std::string(selection.GetServerName()) == "Old server");
}

TEST(ServerLists, EmptyServerListStillSelectsAnExistingZeroID)
{
	CServerInformation selection;
	SeedSelection(selection);
	auto zero = Servers(0, {{0, "Zero server", 4}});
	CHECK(ApplyServerList(selection, *zero));
	CHECK(selection.SetServerID(7));
	auto packet = Servers(7, {});
	CHECK(ApplyServerList(selection, *packet));
	CHECK_EQ(0, selection.GetServerID());
	CHECK_EQ(4, selection.GetServerStatus());
	CHECK(std::string(selection.GetServerName()) == "Zero server");
}

TEST(ServerLists, MissingSelectedWorldLeavesPacketAndSelectionUntouched)
{
	CServerInformation selection;
	SeedSelection(selection);
	CHECK(selection.RemoveData(12));
	auto packet = Servers(9, {{9, "Incoming", 1}});
	CHECK(!ApplyServerList(selection, *packet));
	CHECK_EQ(1, packet->getListNum());
	CHECK_EQ(1, selection.size());
	CHECK_EQ(12, selection.GetServerGroupID());
	CHECK_EQ(7, selection.GetServerID());
	CHECK_EQ(5, selection.GetServerStatus());
	CHECK(std::string(selection.GetServerName()) == "Old server");
	CHECK(selection.GetData(23)->empty());

	// The same packet remains usable after a valid world is selected.
	CHECK(selection.SetServerGroupID(23));
	CHECK(ApplyServerList(selection, *packet));
	CHECK_EQ(0, packet->getListNum());
	CHECK_EQ(9, selection.GetServerID());
	CHECK(std::string(selection.GetServerName()) == "Incoming");
}

TEST(ServerLists, ZeroAndMaximumIDsCanBeSelected)
{
	for (BYTE id : {0, 255})
	{
		CServerInformation selection;
		auto worlds = Worlds(id, {{id, "World", 255}});
		ApplyWorldList(selection, *worlds);
		CHECK_EQ(id, selection.GetServerGroupID());
		CHECK_EQ(255, selection.GetServerGroupStatus());
		auto servers = Servers(id, {{id, "Server", 255}});
		CHECK(ApplyServerList(selection, *servers));
		CHECK_EQ(id, selection.GetServerID());
		CHECK_EQ(255, selection.GetServerStatus());
	}
}

TEST(ServerLists, MaximumAdvertisedListsAreConsumedInFull)
{
	std::vector<Row> rows;
	for (int id = 219; id <= 255; ++id)
		rows.push_back({static_cast<BYTE>(id), "ABCDEFGHIJKLMNOPQRST", static_cast<BYTE>(id)});
	CServerInformation selection;
	auto worlds = Worlds(255, rows);
	CHECK_EQ(LCWorldListFactory().getPacketMaxSize(), worlds->getPacketSize());
	ApplyWorldList(selection, *worlds);
	CHECK_EQ(0, worlds->getListNum());
	CHECK_EQ(37, selection.size());
	CHECK_EQ(255, selection.GetServerGroupID());
	auto servers = Servers(255, rows);
	CHECK_EQ(LCServerListFactory().getPacketMaxSize(), servers->getPacketSize());
	CHECK(ApplyServerList(selection, *servers));
	CHECK_EQ(0, servers->getListNum());
	CHECK_EQ(37, selection.GetData(255)->size());
	CHECK_EQ(255, selection.GetServerID());
	for (int id = 219; id <= 255; ++id)
	{
		CHECK_EQ(id, selection.GetData(id)->GetGroupStatus());
		CHECK_EQ(id, selection.GetData(255)->GetData(id)->ServerStatus);
	}
}

TEST(ServerLists, NullRowsAreConsumedAndOnlyTheFirstPositionSuppliesTheFallback)
{
	// Readers never construct null rows, but the public packet API permits them.
	CServerInformation selection;
	auto worlds = Worlds(0, {{42, "World", 1}});
	LCWorldList worldPacket;
	worldPacket.setCurrentWorldID(0);
	worldPacket.addListElement(nullptr);
	worldPacket.addListElement(worlds->popFrontListElement());
	ApplyWorldList(selection, worldPacket);
	CHECK_EQ(0, worldPacket.getListNum());
	CHECK_EQ(1, selection.size());
	CHECK_EQ(0, selection.GetServerGroupID());
	CHECK(selection.GetServerGroupName() == nullptr);
	CHECK(selection.SetServerGroupID(42));

	auto servers = Servers(0, {{9, "Server", 2}});
	LCServerList serverPacket;
	serverPacket.setCurrentServerGroupID(0);
	serverPacket.addListElement(nullptr);
	serverPacket.addListElement(servers->popFrontListElement());
	CHECK(ApplyServerList(selection, serverPacket));
	CHECK_EQ(0, serverPacket.getListNum());
	CHECK_EQ(1, selection.GetData(42)->size());
	CHECK_EQ(0, selection.GetServerID());
	CHECK(selection.GetServerName() == nullptr);
}
