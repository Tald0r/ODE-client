#include "test_framework.h"
#include "RequestUserManager.h"

// Exercise the real address book used by the peer whisper and party handlers.
// Its production object owns g_pRequestUserManager; no executable stub is needed.
TEST(RequestUsers, MissingNamesDoNotCreateEntries)
{
	RequestUserManager users;
	CHECK(!users.HasRequestUser("absent"));
	CHECK(users.GetUserInfo("absent") == nullptr);
	users.AddRequestUser("Alice", "192.0.2.1");
	CHECK(users.HasRequestUser("Alice"));
	CHECK(!users.HasRequestUser("alice"));
	const auto* user = users.GetUserInfo("Alice");
	CHECK(user != nullptr);
	if (!user) return;
	CHECK(user->Name == "Alice");
	CHECK(user->IP == "192.0.2.1");
	CHECK_EQ(0, user->UDPPort);
}

TEST(RequestUsers, ChangedAddressesUpdateTheExistingEntryAndPort)
{
	RequestUserManager users;
	users.AddRequestUser("Alice", "192.0.2.1", 1234);
	users.AddRequestUser("Bob", "192.0.2.2", 4321);
	auto* alice = users.GetUserInfo("Alice");
	users.AddRequestUser("Alice", "198.51.100.1", 5678);
	CHECK(users.GetUserInfo("Alice") == alice);
	CHECK(alice->Name == "Alice");
	CHECK(alice->IP == "198.51.100.1");
	CHECK_EQ(5678, alice->UDPPort);
	CHECK(users.GetUserInfo("Bob")->IP == "192.0.2.2");
	CHECK_EQ(4321, users.GetUserInfo("Bob")->UDPPort);
}

TEST(RequestUsers, SameAddressRetainsThePortLearnedFromPartyDatagrams)
{
	RequestUserManager users;
	users.AddRequestUser("Alice", "192.0.2.1");
	auto* alice = users.GetUserInfo("Alice");
	// RCPositionInfo, RCCharacterInfo and RCStatusHP update this mutable row.
	alice->UDPPort = 1234;
	users.AddRequestUser("Alice", "192.0.2.1");
	CHECK(users.GetUserInfo("Alice") == alice);
	CHECK_EQ(1234, alice->UDPPort);
	users.AddRequestUser("Alice", "192.0.2.1", 5678);
	CHECK_EQ(1234, alice->UDPPort);
	users.AddRequestUser("Alice", "198.51.100.1");
	CHECK_EQ(0, alice->UDPPort);
}

TEST(RequestUsers, ReleaseIsRepeatableAndTheBookCanBeReused)
{
	RequestUserManager users;
	users.AddRequestUser("Alice", "192.0.2.1", 1234);
	users.AddRequestUser("Bob", "192.0.2.2", 4321);
	users.Release();
	CHECK(!users.HasRequestUser("Alice"));
	CHECK(users.GetUserInfo("Bob") == nullptr);
	users.Release();
	users.AddRequestUser("Alice", "198.51.100.1", 5678);
	CHECK(users.GetUserInfo("Alice")->IP == "198.51.100.1");
	CHECK_EQ(5678, users.GetUserInfo("Alice")->UDPPort);
}
