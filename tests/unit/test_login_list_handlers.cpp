#include "test_framework.h"
#include "packet_stream_access.h"
#include "LoginListHost.h"
#include "CServerInformation.h"
#include "Lpackets/LCWorldList.h"
#include "Lpackets/LCServerList.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::vector<int> calls;
unsigned expectedWorld, expectedServer;
const LoginLists::Host host{
	.PublishWorlds = [] {
		calls.push_back(1);
		CHECK(g_pServerInformation != nullptr);
		if (g_pServerInformation) CHECK_EQ(expectedWorld, g_pServerInformation->GetServerGroupID());
	},
	.SelectWorldMode = [] { calls.push_back(2); },
	.PublishServers = [] {
		calls.push_back(3);
		CHECK(g_pServerInformation != nullptr);
		if (g_pServerInformation) CHECK_EQ(expectedServer, g_pServerInformation->GetServerID());
	},
	.SelectServerMode = [] { calls.push_back(4); },
};

struct World
{
	CServerInformation* previousSelection = g_pServerInformation;
	const LoginLists::Host* previousHost = LoginLists::SetHost(&host);
	World() { g_pServerInformation = nullptr; calls.clear(); expectedWorld = 5; expectedServer = 7; }
	~World()
	{
		delete g_pServerInformation;
		g_pServerInformation = previousSelection;
		LoginLists::SetHost(previousHost);
	}
};

struct Row { BYTE id; std::string name; BYTE status; };

template<class PacketType, class Factory>
std::unique_ptr<PacketType> Read(BYTE requested, const std::vector<Row>& rows)
{
	std::vector<BYTE> bytes{requested, static_cast<BYTE>(rows.size())};
	for (const auto& row : rows)
	{
		bytes.push_back(row.id);
		bytes.push_back(static_cast<BYTE>(row.name.size()));
		bytes.insert(bytes.end(), row.name.begin(), row.name.end());
		bytes.push_back(row.status);
	}
	Factory factory;
	std::unique_ptr<PacketType> packet(static_cast<PacketType*>(factory.createPacket()));
	Socket socket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&socket, 2048);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned>(bytes.size()));
	packet->read(stream);
	CHECK(stream.isEmpty());
	CHECK_EQ(bytes.size(), packet->getPacketSize());
	return packet;
}

void Worlds(BYTE requested, const std::vector<Row>& rows)
{
	auto packet = Read<LCWorldList, LCWorldListFactory>(requested, rows);
	LCWorldListHandler::execute(packet.get(), nullptr);
	CHECK_EQ(0, packet->getListNum());
}

void Servers(BYTE requested, const std::vector<Row>& rows)
{
	auto packet = Read<LCServerList, LCServerListFactory>(requested, rows);
	LCServerListHandler::execute(packet.get(), nullptr);
	CHECK_EQ(0, packet->getListNum());
}
}

TEST(LoginListHandlers, WorldListCreatesModelBeforePublishingAndSelectingTheMode)
{
	World world;
	Worlds(99, {{5, "First world", 1}, {9, "Other world", 2}});
	CHECK(calls == std::vector<int>({1, 2}));
	CHECK_EQ(2, g_pServerInformation->size());
	CHECK(std::string(g_pServerInformation->GetServerGroupName()) == "First world");
	CHECK_EQ(1, g_pServerInformation->GetServerGroupStatus());
}

TEST(LoginListHandlers, ServerListAppliesBeforePublishingAndSelectingTheMode)
{
	World world;
	Worlds(5, {{5, "World", 1}});
	calls.clear();
	Servers(7, {{2, "First server", 1}, {7, "Selected server", 4}});
	CHECK(calls == std::vector<int>({3, 4}));
	CHECK(std::string(g_pServerInformation->GetServerName()) == "Selected server");
	CHECK_EQ(4, g_pServerInformation->GetServerStatus());
}

TEST(LoginListHandlers, ReplacingWorldsDiscardsOldServersBeforePublication)
{
	World world;
	Worlds(5, {{5, "Old", 1}});
	Servers(7, {{7, "Old server", 1}});
	auto* original = g_pServerInformation;
	calls.clear();
	expectedWorld = 9;
	Worlds(9, {{9, "New", 3}});
	CHECK(g_pServerInformation == original);
	CHECK(g_pServerInformation->GetData(5) == nullptr);
	CHECK_EQ(0, g_pServerInformation->GetServerID());
	CHECK(calls == std::vector<int>({1, 2}));
}

TEST(LoginListHandlers, ServerSnapshotsRemoveOmittedServersAndClearAnEmptySelection)
{
	World world;
	Worlds(5, {{5, "World", 4}, {9, "Other world", 2}});
	auto* selectedWorld = g_pServerInformation->GetData(5);
	auto* otherWorld = g_pServerInformation->GetData(9);
	auto* otherServer = new SERVER_INFO;
	otherServer->ServerName = "Untouched";
	otherWorld->AddData(77, otherServer);
	Servers(7, {{7, "Removed later", 1}, {8, "Retained later", 2}});
	expectedServer = 8;
	Servers(8, {{8, "Current", 3}});
	CHECK(g_pServerInformation->GetData(5) == selectedWorld);
	CHECK(selectedWorld->GetData(7) == nullptr);
	CHECK_EQ(1, selectedWorld->size());
	CHECK_EQ(3, g_pServerInformation->GetServerStatus());
	expectedServer = 0;
	Servers(8, {});
	CHECK(selectedWorld->empty());
	CHECK_EQ(0, g_pServerInformation->GetServerID());
	CHECK_EQ(0, g_pServerInformation->GetServerStatus());
	CHECK(g_pServerInformation->GetServerName() == nullptr);
	CHECK_EQ(5, g_pServerInformation->GetServerGroupID());
	CHECK_EQ(4, g_pServerInformation->GetServerGroupStatus());
	CHECK(std::string(g_pServerInformation->GetServerGroupName()) == "World");
	CHECK(otherWorld->GetData(77) == otherServer);
	CHECK(std::string(otherServer->ServerName.GetString()) == "Untouched");
}

TEST(LoginListHandlers, ServerListWithoutASelectedWorldKeepsPacketAndSkipsPublication)
{
	World world;
	for (int attempt = 0; attempt < 2; ++attempt)
	{
		auto packet = Read<LCServerList, LCServerListFactory>(7, {{7, "Server", 1}});
		LCServerListHandler::execute(packet.get(), nullptr);
		CHECK_EQ(1, packet->getListNum());
		CHECK(calls.empty());
		if (attempt == 0) g_pServerInformation = new CServerInformation;
	}
}

TEST(LoginListHandlers, EmptyAcceptedListsStillPublishAndChangeMode)
{
	World world;
	expectedWorld = 0;
	Worlds(0, {});
	CHECK(calls == std::vector<int>({1, 2}));
	calls.clear();
	expectedWorld = 5;
	Worlds(5, {{5, "World", 1}});
	calls.clear();
	expectedServer = 0;
	Servers(0, {});
	CHECK(calls == std::vector<int>({3, 4}));
}

TEST(LoginListHandlers, MissingCallbacksDoNotPreventModelApplication)
{
	World world;
	const LoginLists::Host empty{};
	for (const auto* service : {static_cast<const LoginLists::Host*>(nullptr), &empty})
	{
		LoginLists::SetHost(service);
		Worlds(5, {{5, "World", 1}});
		Servers(7, {{7, "Server", 2}});
		CHECK_EQ(5, g_pServerInformation->GetServerGroupID());
		CHECK_EQ(7, g_pServerInformation->GetServerID());
		CHECK(calls.empty());
	}
	LoginLists::SetHost(nullptr);
}

TEST(LoginListHandlers, AReplacementHostSuppliesTheModeAfterPublication)
{
	World world;
	const LoginLists::Host replacing{
		.PublishWorlds = [] { calls.push_back(10); LoginLists::SetHost(&host); },
		.PublishServers = [] { calls.push_back(30); LoginLists::SetHost(&host); },
	};
	LoginLists::SetHost(&replacing);
	Worlds(5, {{5, "World", 1}});
	CHECK(calls == std::vector<int>({10, 2}));
	calls.clear();
	LoginLists::SetHost(&replacing);
	Servers(7, {{7, "Server", 1}});
	CHECK(calls == std::vector<int>({30, 4}));
}

TEST(LoginListHandlers, MissingPublicationAndModeCallbacksAreIndependent)
{
	World world;
	const LoginLists::Host modes{
		.SelectWorldMode = host.SelectWorldMode,
		.SelectServerMode = host.SelectServerMode,
	};
	LoginLists::SetHost(&modes);
	Worlds(5, {{5, "World", 1}});
	Servers(7, {{7, "Server", 1}});
	CHECK(calls == std::vector<int>({2, 4}));
	calls.clear();
	const LoginLists::Host publications{
		.PublishWorlds = host.PublishWorlds,
		.PublishServers = host.PublishServers,
	};
	LoginLists::SetHost(&publications);
	Worlds(5, {{5, "World", 1}});
	Servers(7, {{7, "Server", 1}});
	CHECK(calls == std::vector<int>({1, 3}));
	LoginLists::SetHost(nullptr);
}

TEST(LoginListHandlers, PublicationMayRemoveTheHostBeforeModeSelection)
{
	World world;
	const LoginLists::Host removing{
		.PublishWorlds = [] { calls.push_back(1); LoginLists::SetHost(nullptr); },
		.SelectWorldMode = host.SelectWorldMode,
		.PublishServers = [] { calls.push_back(3); LoginLists::SetHost(nullptr); },
		.SelectServerMode = host.SelectServerMode,
	};
	LoginLists::SetHost(&removing);
	Worlds(5, {{5, "World", 1}});
	CHECK(calls == std::vector<int>({1}));
	calls.clear();
	LoginLists::SetHost(&removing);
	Servers(7, {{7, "Server", 1}});
	CHECK(calls == std::vector<int>({3}));
}

TEST(LoginListHandlers, PublicationFailureDoesNotChangeTheMode)
{
	World world;
	const LoginLists::Host throwing{
		.PublishWorlds = [] { calls.push_back(1); throw std::runtime_error("world UI"); },
		.SelectWorldMode = host.SelectWorldMode,
		.PublishServers = [] { calls.push_back(3); throw std::runtime_error("server UI"); },
		.SelectServerMode = host.SelectServerMode,
	};
	LoginLists::SetHost(&throwing);
	bool threw = false;
	try { Worlds(5, {{5, "World", 1}}); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw);
	CHECK_EQ(5, g_pServerInformation->GetServerGroupID());
	CHECK(calls == std::vector<int>({1}));
	calls.clear();
	threw = false;
	try { Servers(7, {{7, "Server", 1}}); } catch (const std::runtime_error&) { threw = true; }
	CHECK(threw);
	CHECK_EQ(7, g_pServerInformation->GetServerID());
	CHECK(calls == std::vector<int>({3}));
	LoginLists::SetHost(nullptr);
}
