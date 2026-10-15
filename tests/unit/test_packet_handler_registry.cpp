#include "test_framework.h"
#include "PacketHandlerRegistry.h"
#include "PacketDispatcher.h"
#include "PacketFactory.h"

#define WIRE_INVENTORY_INCLUDES
#include "../generated/WireInventory.inc"
#undef WIRE_INVENTORY_INCLUDES

#include <set>
#include <string>
#include <vector>

// The packet-handler registry (task 4.218, gamemodel's since then): the
// table that binds each packet the client can receive to its handler. A
// packet the client accepts but nobody registered disconnects the client
// on arrival, so the table must cover every received direction (GC, GL,
// GU, LC, LG, UC, CR, CU, RC) of the wire inventory, except the three
// peer-file replies whose handlers task 5.1's eighth slice deleted with
// the outbound peer side. Of the packets the client only writes (CG, CL)
// it binds just CGConnectSetKey, which the login paths dispatch to
// themselves as a no-op.
namespace {

struct FactoryEntry
{
	FactoryEntry(const char* n, PacketFactory* f, bool) : name(n), factory(f) {}
	const char*		name;
	PacketFactory*	factory;
};

struct AllFactories
{
	std::vector<FactoryEntry> factories;
	AllFactories()
	{
#define WIRE_INVENTORY_ENTRIES
#include "../generated/WireInventory.inc"
#undef WIRE_INVENTORY_ENTRIES
	}
	~AllFactories() { for (auto& e : factories) delete e.factory; }
	AllFactories(const AllFactories&) = delete;
	AllFactories& operator=(const AllFactories&) = delete;
};

bool ClientWrites(const std::string& name)
{
	return name.compare(0, 2, "CG") == 0 || name.compare(0, 2, "CL") == 0;
}

// Clears the dispatcher's table around a test that fills it.
struct CleanTable
{
	CleanTable() { PacketDispatcher::clearForTests(); }
	~CleanTable() { PacketDispatcher::clearForTests(); }
};

} // namespace

TEST(PacketHandlerRegistry, EveryReceivedPacketHasAHandler)
{
	CleanTable clean;
	registerClientPacketHandlers();
	AllFactories all;
	CHECK(all.factories.size() > 400u);
	const std::set<std::string> deliberatelyUnbound = {"RCConnectVerify", "RCRequestVerify", "RCRequestedFile"};
	std::vector<std::string> missing, unexpected;
	for (const auto& entry : all.factories)
	{
		const std::string name = entry.name;
		const bool bound = PacketDispatcher::handlerFor(entry.factory->getPacketID()) != nullptr;
		const bool shouldBe = ClientWrites(name) ? name == "CGConnectSetKey" : deliberatelyUnbound.count(name) == 0;
		if (shouldBe && !bound) missing.push_back(name);
		if (!shouldBe && bound) unexpected.push_back(name);
	}
	CHECK(missing.empty());
	CHECK(unexpected.empty());
	for (const auto& name : missing) printf("      unbound: %s\n", name.c_str());
	for (const auto& name : unexpected) printf("      bound: %s\n", name.c_str());
}

TEST(PacketHandlerRegistry, RegisteringAgainChangesNothing)
{
	CleanTable clean;
	registerClientPacketHandlers();
	// The registry remembers it ran: a second call registers nothing,
	// and so meets no double registration.
	PacketDispatcher::clearForTests();
	registerClientPacketHandlers();
	AllFactories all;
	for (const auto& entry : all.factories)
		CHECK(PacketDispatcher::handlerFor(entry.factory->getPacketID()) == nullptr);
}

TEST(PacketDispatcher, HandlerForAnswersNullPastTheTableAndWhenUnbound)
{
	CleanTable clean;
	CHECK(PacketDispatcher::handlerFor(Packet::PACKET_MAX) == nullptr);
	CHECK(PacketDispatcher::handlerFor(0) == nullptr);
	struct Thunk { static void call(Packet*, Player*) {} };
	PacketDispatcher::registerHandler(0, &Thunk::call);
	CHECK(PacketDispatcher::handlerFor(0) == &Thunk::call);
	PacketDispatcher::clearForTests();
	CHECK(PacketDispatcher::handlerFor(0) == nullptr);
}
