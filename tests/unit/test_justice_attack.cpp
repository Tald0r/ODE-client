//----------------------------------------------------------------------
// Self-defense membership and its real packet handlers, linked from gamemodel.
//----------------------------------------------------------------------
#include "test_framework.h"
#include "packet_stream_access.h"

#include "MJusticeAttackManager.h"
#include "Gpackets/GCAddInjuriousCreature.h"
#include "Gpackets/GCRemoveInjuriousCreature.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"

#include <memory>
#include <string>
#include <vector>

namespace {

// Install the library's real global, restoring it even if a packet throws.
struct RosterFixture
{
	MJusticeAttackManager roster;
	MJusticeAttackManager* previous = g_pJusticeAttackManager;

	RosterFixture() { g_pJusticeAttackManager = &roster; }
	~RosterFixture() { g_pJusticeAttackManager = previous; }
};

template <class Factory, class PacketType>
std::unique_ptr<PacketType> ReadNamePacket(const std::string& name)
{
	Factory factory;
	std::unique_ptr<Packet> packet(factory.createPacket());
	auto* typed = dynamic_cast<PacketType*>(packet.get());
	CHECK(typed != nullptr);
	if (typed == nullptr)
		return {};

	// The server writes a byte count followed by that many name bytes.
	std::vector<unsigned char> bytes{static_cast<unsigned char>(name.size())};
	bytes.insert(bytes.end(), name.begin(), name.end());
	Socket socket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&socket, 64);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	typed->read(stream);
	CHECK(stream.isEmpty());
	CHECK_EQ(bytes.size(), typed->getPacketSize());
	CHECK(typed->getName() == name);
	packet.release();
	return std::unique_ptr<PacketType>(typed);
}

void AddPacket(const std::string& name)
{
	auto packet = ReadNamePacket<GCAddInjuriousCreatureFactory, GCAddInjuriousCreature>(name);
	if (packet)
		GCAddInjuriousCreatureHandler::execute(packet.get(), nullptr);
}

void RemovePacket(const std::string& name)
{
	auto packet = ReadNamePacket<GCRemoveInjuriousCreatureFactory, GCRemoveInjuriousCreature>(name);
	if (packet)
		GCRemoveInjuriousCreatureHandler::execute(packet.get(), nullptr);
}

} // namespace

TEST(JusticeAttack, NewRosterHasNoTargets)
{
	MJusticeAttackManager roster;
	CHECK(!roster.HasCreature("Slayer"));
	CHECK(!roster.RemoveCreature("Slayer"));
	roster.Release();
	CHECK(!roster.HasCreature("Slayer"));
}

TEST(JusticeAttack, AddOwnsTheNameAfterTheInputChanges)
{
	MJusticeAttackManager roster;
	{
		std::string name = "Vampire";
		roster.AddCreature(name.c_str());
		name.assign("Slayer");
		CHECK(!roster.HasCreature(name.c_str()));
	}
	CHECK(roster.HasCreature("Vampire"));
	CHECK(roster.RemoveCreature("Vampire"));
	CHECK(!roster.HasCreature("Vampire"));
}

TEST(JusticeAttack, DuplicateAddsNeedOnlyOneRemoval)
{
	MJusticeAttackManager roster;
	roster.AddCreature("Slayer");
	roster.AddCreature("Slayer");
	CHECK(roster.HasCreature("Slayer"));
	CHECK(roster.RemoveCreature("Slayer"));
	CHECK(!roster.HasCreature("Slayer"));
	CHECK(!roster.RemoveCreature("Slayer"));
}

TEST(JusticeAttack, NamesAreCaseSensitiveAndExact)
{
	MJusticeAttackManager roster;
	roster.AddCreature("Slayer");
	roster.AddCreature("slayer");
	roster.AddCreature("SlayerTwo");
	CHECK(!roster.HasCreature("SLAYER"));
	CHECK(!roster.HasCreature("Slay"));
	CHECK(roster.RemoveCreature("Slayer"));
	CHECK(!roster.HasCreature("Slayer"));
	CHECK(roster.HasCreature("slayer"));
	CHECK(roster.HasCreature("SlayerTwo"));
}

TEST(JusticeAttack, RemovingAnUnknownNamePreservesTheRoster)
{
	MJusticeAttackManager roster;
	roster.AddCreature("Slayer");
	roster.AddCreature("Vampire");
	CHECK(!roster.RemoveCreature("Ousters"));
	CHECK(roster.HasCreature("Slayer"));
	CHECK(roster.HasCreature("Vampire"));
}

TEST(JusticeAttack, ReleaseClearsAllNamesAndAllowsReuse)
{
	MJusticeAttackManager roster;
	roster.AddCreature("Slayer");
	roster.AddCreature("Vampire");
	roster.Release();
	CHECK(!roster.HasCreature("Slayer"));
	CHECK(!roster.HasCreature("Vampire"));
	CHECK(!roster.RemoveCreature("Slayer"));
	roster.Release();
	roster.AddCreature("Ousters");
	CHECK(roster.HasCreature("Ousters"));
	CHECK(!roster.HasCreature("Slayer"));
}

TEST(JusticeAttack, RostersKeepSeparateMembership)
{
	MJusticeAttackManager first;
	MJusticeAttackManager second;
	first.AddCreature("Slayer");
	second.AddCreature("Vampire");
	CHECK(!first.HasCreature("Vampire"));
	CHECK(!second.HasCreature("Slayer"));
	first.Release();
	CHECK(second.HasCreature("Vampire"));
}

TEST(JusticeAttack, PacketSequenceChangesOnlyTheNamedTarget)
{
	RosterFixture fixture;
	AddPacket("Slayer");
	AddPacket("Vampire");
	AddPacket("Slayer");
	CHECK(fixture.roster.HasCreature("Slayer"));
	CHECK(fixture.roster.HasCreature("Vampire"));
	RemovePacket("Slayer");
	CHECK(!fixture.roster.HasCreature("Slayer"));
	CHECK(fixture.roster.HasCreature("Vampire"));
	RemovePacket("Slayer");
	CHECK(fixture.roster.HasCreature("Vampire"));
	RemovePacket("Vampire");
	CHECK(!fixture.roster.HasCreature("Vampire"));
}

TEST(JusticeAttack, PacketsAcceptTheShortestAndLongestWireNames)
{
	RosterFixture fixture;
	for (const auto* name : {"A", "abcdefghij"})
	{
		AddPacket(name);
		CHECK(fixture.roster.HasCreature(name));
		RemovePacket(name);
		CHECK(!fixture.roster.HasCreature(name));
	}
}

TEST(JusticeAttack, PacketsPreserveMultibyteNames)
{
	RosterFixture fixture;
	const char* name = "\xC7\xD1\xB1\xDB";
	AddPacket(name);
	CHECK(fixture.roster.HasCreature(name));
	CHECK(!fixture.roster.HasCreature("\xC7\xD1"));
	RemovePacket(name);
	CHECK(!fixture.roster.HasCreature(name));
}

TEST(JusticeAttack, PacketsIgnoreAMissingManager)
{
	RosterFixture fixture;
	fixture.roster.AddCreature("Vampire");
	g_pJusticeAttackManager = nullptr;
	AddPacket("Slayer");
	RemovePacket("Vampire");
	CHECK(g_pJusticeAttackManager == nullptr);
	CHECK(!fixture.roster.HasCreature("Slayer"));
	CHECK(fixture.roster.HasCreature("Vampire"));
}

TEST(JusticeAttack, PacketsCanRepopulateTheRosterAfterReset)
{
	RosterFixture fixture;
	AddPacket("Slayer");
	AddPacket("Vampire");
	fixture.roster.Release();
	CHECK(!fixture.roster.HasCreature("Slayer"));
	CHECK(!fixture.roster.HasCreature("Vampire"));
	RemovePacket("Slayer");
	AddPacket("Ousters");
	CHECK(fixture.roster.HasCreature("Ousters"));
	CHECK(!fixture.roster.HasCreature("Slayer"));
	CHECK(!fixture.roster.HasCreature("Vampire"));
}
