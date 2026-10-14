#include "test_framework.h"
#include "gamemodel_world.h"
#include "MCreatureTable.h"
#include "MStatus.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCAddBurrowingCreature.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

// A creature burrowed underground. One the zone does not know yet is made
// as a male vampire with the packet's name, ID and cell, underground,
// facing down and standing, in no guild, at full health of 100, and added
// to the zone (or dropped if the zone refuses it). One the zone knows goes
// underground under the packet's name and moves to the cell, leaving any
// guild. Without a zone nothing happens.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool zoneTakes = true;

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	MStatus status;
};
std::map<TYPE_OBJECTID, FakeCreature*> zoneCreatures;
std::vector<std::unique_ptr<FakeCreature>> made;

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature* creature) { return reinterpret_cast<MCreature*>(creature); }
FakeCreature& Fake(MCreature* p) { return *reinterpret_cast<FakeCreature*>(p); }
std::string Who(MCreature* p) { return Fake(p).id == 0 ? std::string("new") : N(Fake(p).id); }

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) {
		auto it = zoneCreatures.find(id);
		return it == zoneCreatures.end() ? nullptr : Handle(it->second);
	};
	host.NewCreature = []() -> MCreature* {
		made.push_back(std::make_unique<FakeCreature>());
		calls.push_back("new creature");
		return Handle(made.back().get());
	};
	host.AddCreature = [](MCreature* p) {
		calls.push_back("zone takes|" + Who(p));
		if (zoneTakes) zoneCreatures[Fake(p).id] = &Fake(p);
		return zoneTakes;
	};
	host.DeleteCreature = [](MCreature* p) { calls.push_back("deleted|" + Who(p)); };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.SetCreatureType = [](MCreature* p, int type) { calls.push_back("type|" + Who(p) + "|" + N(type)); };
	host.SetName = [](MCreature* p, const char* name) { calls.push_back("name|" + Who(p) + "|" + name); };
	host.SetUndergroundCreature = [](MCreature* p) { calls.push_back("underground|" + Who(p)); };
	host.SetID = [](MCreature* p, TYPE_OBJECTID id) { calls.push_back("id|" + N(id)); Fake(p).id = id; };
	host.SetPosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("at|" + Who(p) + "|" + N(x) + "," + N(y));
	};
	host.SetServerPosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("server at|" + Who(p) + "|" + N(x) + "," + N(y));
	};
	host.MovePosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("moves|" + Who(p) + "|" + N(x) + "," + N(y));
		return true;
	};
	host.SetDirection = [](MCreature* p, int direction) { calls.push_back("turns|" + Who(p) + "|" + N(direction)); };
	host.SetCurrentDirection = [](MCreature* p, int direction) { calls.push_back("faces|" + Who(p) + "|" + N(direction)); };
	host.SetAction = [](MCreature* p, int action) { calls.push_back("acts|" + Who(p) + "|" + N(action)); };
	host.SetGuildNumber = [](MCreature* p, int guild) { calls.push_back("guild|" + Who(p) + "|" + N(guild)); };
	host.Status = [](MCreature* p) { return &Fake(p).status; };
	return host;
}();

struct World : GameModelWorld
{
	FakeCreature known;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	World()
	{
		calls.clear();
		zoneExists = true;
		zoneTakes = true;
		made.clear();
		zoneCreatures.clear();
		known.id = 1003;
		zoneCreatures[known.id] = &known;
	}
	~World()
	{
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		zoneCreatures.clear();
		made.clear();
	}
};

void Receive(ObjectID_t id, const char* name, int x, int y)
{
	GCAddBurrowingCreature packet;
	packet.setObjectID(id);
	packet.setName(name);
	packet.setX(static_cast<Coord_t>(x));
	packet.setY(static_cast<Coord_t>(y));
	GCAddBurrowingCreatureHandler::execute(&packet, nullptr);
}

} // namespace

TEST(BurrowingCreatureReplies, ANewCreatureIsMadeUndergroundAndAdded)
{
	World world;
	Receive(1005, "Mole", 30, 31);
	CHECK(calls == std::vector<std::string>({"new creature", "type|new|" + N(CREATURETYPE_VAMPIRE_MALE1), "name|new|Mole",
		"underground|new", "id|1005", "at|1005|30,31", "server at|1005|30,31", "turns|1005|" + N(DIRECTION_DOWN),
		"faces|1005|" + N(DIRECTION_DOWN), "acts|1005|" + N(ACTION_STAND), "guild|1005|-1", "zone takes|1005"}));
	CHECK(made.size() == 1);
	if (made.size() != 1) return;
	CHECK_EQ(100, static_cast<int>(made[0]->status.GetMAX_HP()));
	CHECK_EQ(100, static_cast<int>(made[0]->status.GetHP()));
	CHECK(zoneCreatures.count(1005) == 1);
}

TEST(BurrowingCreatureReplies, ACreatureTheZoneRefusesIsDropped)
{
	World world;
	zoneTakes = false;
	Receive(1005, "Mole", 30, 31);
	CHECK(calls.size() >= 2 && calls[calls.size() - 2] == "zone takes|1005" && calls.back() == "deleted|1005");
}

TEST(BurrowingCreatureReplies, AKnownCreatureGoesUndergroundWhereItIs)
{
	World world;
	Receive(1003, "Mole", 30, 31);
	CHECK(calls == std::vector<std::string>({"underground|1003", "name|1003|Mole", "moves|1003|30,31", "server at|1003|30,31",
		"guild|1003|-1"}));
	CHECK(made.empty());
}

TEST(BurrowingCreatureReplies, WithoutAZoneOrTheHostsNothingHappens)
{
	World world;
	zoneExists = false;
	Receive(1005, "Mole", 30, 31);
	CHECK(calls.empty());
	// Without the creature's host, a made creature is still added; its
	// health is left as it was made.
	zoneExists = true;
	CreatureState::SetHost(nullptr);
	Receive(1005, "Mole", 30, 31);
	CHECK(calls == std::vector<std::string>({"new creature", "zone takes|new"}));
	CHECK_EQ(1u, made.size());
	calls.clear();
	ZoneState::SetHost(nullptr);
	CHECK(ZoneState::NewCreature() == nullptr);
	CHECK(!ZoneState::AddCreature(Handle(&world.known)));
	ZoneState::DeleteCreature(Handle(&world.known));
	CreatureState::SetName(Handle(&world.known), "x");
	CreatureState::SetID(Handle(&world.known), 1);
	CreatureState::SetPosition(Handle(&world.known), 1, 2);
	CreatureState::SetGuildNumber(Handle(&world.known), 1);
	CHECK(calls.empty());
	CHECK_EQ(1003u, static_cast<unsigned>(world.known.id));
}
