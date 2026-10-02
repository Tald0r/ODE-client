#include "test_framework.h"
#include "CServerInformation.h"

#include <cstring>
#include <new>
#include <string>

namespace {

ServerGroup* AddWorld(CServerInformation& selection, unsigned int id,
	const char* name, int status)
{
	auto* world = new ServerGroup;
	world->SetGroupName(name);
	world->SetGroupStatus(status);
	CHECK(selection.AddData(id, world));
	return world;
}

void AddServer(ServerGroup& world, unsigned int id, const char* name, int status)
{
	auto* server = new SERVER_INFO;
	server->ServerName = name;
	server->ServerStatus = status;
	CHECK(world.AddData(id, server));
}

} // namespace

TEST(ServerInformation, FreshSelectionStartsEmptyEvenOverNonzeroStorage)
{
	alignas(CServerInformation) unsigned char storage[sizeof(CServerInformation)];
	std::memset(storage, 0xcc, sizeof(storage));
	auto* selection = new (storage) CServerInformation;
	CHECK(selection->empty());
	CHECK_EQ(0, selection->GetServerGroupID());
	CHECK_EQ(0, selection->GetServerGroupStatus());
	CHECK_EQ(0, selection->GetServerID());
	CHECK_EQ(0, selection->GetServerStatus());
	CHECK(selection->GetServerGroupName() == nullptr);
	CHECK(selection->GetServerName() == nullptr);
	CHECK(!selection->SetServerID(7));
	CHECK(!selection->SetServerGroupID(7));
	selection->~CServerInformation();
}

TEST(ServerInformation, ReleaseAlsoClearsTheSelectedServerStatus)
{
	CServerInformation selection;
	auto* world = AddWorld(selection, 12, "World", 3);
	AddServer(*world, 7, "Server", 6);
	CHECK(selection.SetServerGroupID(12));
	CHECK(selection.SetServerID(7));
	CHECK_EQ(6, selection.GetServerStatus());
	selection.Release();
	CHECK_EQ(0, selection.GetServerStatus());
	selection.Release();
	CHECK_EQ(0, selection.GetServerStatus());
}

TEST(ServerInformation, EmptyRowsStartWithNoNameAndZeroStatus)
{
	SERVER_INFO server;
	CHECK(server.ServerName.GetString() == nullptr);
	CHECK_EQ(0, server.ServerStatus);
	ServerGroup world;
	CHECK(world.empty());
	CHECK(world.GetGroupName() == nullptr);
	CHECK_EQ(0, world.GetGroupStatus());
}

TEST(ServerInformation, SelectingWorldCopiesItsMetadata)
{
	CServerInformation selection;
	auto* world = AddWorld(selection, 17, "World seventeen", 3);
	CHECK(selection.SetServerGroupID(17));
	CHECK_EQ(17, selection.GetServerGroupID());
	CHECK_EQ(3, selection.GetServerGroupStatus());
	CHECK(std::string(selection.GetServerGroupName()) == "World seventeen");

	world->SetGroupName("Renamed world");
	world->SetGroupStatus(5);
	CHECK(std::string(selection.GetServerGroupName()) == "World seventeen");
	CHECK_EQ(3, selection.GetServerGroupStatus());
	CHECK(selection.SetServerGroupID(17));
	CHECK(std::string(selection.GetServerGroupName()) == "Renamed world");
	CHECK_EQ(5, selection.GetServerGroupStatus());
}

TEST(ServerInformation, ServerSelectionUsesTheSelectedWorld)
{
	CServerInformation selection;
	auto* first = AddWorld(selection, 12, "First world", 1);
	auto* second = AddWorld(selection, 23, "Second world", 2);
	AddServer(*first, 7, "First server", 4);
	AddServer(*second, 7, "Second server", 5);
	CHECK(selection.SetServerGroupID(12));
	CHECK(selection.SetServerID(7));
	CHECK_EQ(7, selection.GetServerID());
	CHECK(std::string(selection.GetServerName()) == "First server");
	CHECK_EQ(4, selection.GetServerStatus());
	CHECK(selection.SetServerGroupID(23));
	CHECK(selection.SetServerID(7));
	CHECK(std::string(selection.GetServerName()) == "Second server");
	CHECK_EQ(5, selection.GetServerStatus());

	second->GetData(7)->ServerName = "Updated server";
	second->GetData(7)->ServerStatus = 6;
	CHECK(std::string(selection.GetServerName()) == "Second server");
	CHECK_EQ(5, selection.GetServerStatus());
	CHECK(selection.SetServerID(7));
	CHECK(std::string(selection.GetServerName()) == "Updated server");
	CHECK_EQ(6, selection.GetServerStatus());
}

TEST(ServerInformation, MissingWorldClearsItsNameAndRetainsThePreviousSelection)
{
	CServerInformation selection;
	auto* world = AddWorld(selection, 12, "World", 3);
	AddServer(*world, 7, "Server", 4);
	CHECK(selection.SetServerGroupID(12));
	CHECK(selection.SetServerID(7));
	CHECK(!selection.SetServerGroupID(99));
	CHECK(selection.GetServerGroupName() == nullptr);
	CHECK_EQ(12, selection.GetServerGroupID());
	CHECK_EQ(3, selection.GetServerGroupStatus());
	CHECK_EQ(7, selection.GetServerID());
	CHECK_EQ(4, selection.GetServerStatus());
	CHECK(std::string(selection.GetServerName()) == "Server");
	CHECK_EQ(1, selection.size());
}

TEST(ServerInformation, MissingServerOrWorldLeavesTheSelectedServerAlone)
{
	CServerInformation selection;
	auto* world = AddWorld(selection, 12, "World", 3);
	AddServer(*world, 7, "Server", 4);
	CHECK(selection.SetServerGroupID(12));
	CHECK(selection.SetServerID(7));
	CHECK(!selection.SetServerID(99));
	CHECK_EQ(7, selection.GetServerID());
	CHECK_EQ(4, selection.GetServerStatus());
	CHECK(std::string(selection.GetServerName()) == "Server");
	CHECK_EQ(1, world->size());
	CHECK(selection.RemoveData(12));
	CHECK(!selection.SetServerID(7));
	CHECK_EQ(7, selection.GetServerID());
	CHECK_EQ(4, selection.GetServerStatus());
	CHECK(std::string(selection.GetServerName()) == "Server");
}

TEST(ServerInformation, ReleaseClearsOwnedWorldsServersAndNamesAndAllowsReuse)
{
	CServerInformation selection;
	auto* first = AddWorld(selection, 12, "First", 3);
	auto* second = AddWorld(selection, 23, "Second", 4);
	AddServer(*first, 7, "One", 1);
	AddServer(*second, 8, "Two", 2);
	CHECK(selection.SetServerGroupID(12));
	CHECK(selection.SetServerID(7));
	selection.Release();
	CHECK(selection.empty());
	CHECK_EQ(0, selection.GetServerGroupID());
	CHECK_EQ(0, selection.GetServerGroupStatus());
	CHECK_EQ(0, selection.GetServerID());
	CHECK(selection.GetServerGroupName() == nullptr);
	CHECK(selection.GetServerName() == nullptr);
	selection.Release();
	CHECK(selection.empty());
	auto* replacement = AddWorld(selection, 34, "Replacement", 5);
	AddServer(*replacement, 9, "New server", 6);
	CHECK(selection.SetServerGroupID(34));
	CHECK(selection.SetServerID(9));
	CHECK_EQ(34, selection.GetServerGroupID());
	CHECK_EQ(9, selection.GetServerID());
	CHECK_EQ(6, selection.GetServerStatus());
}
