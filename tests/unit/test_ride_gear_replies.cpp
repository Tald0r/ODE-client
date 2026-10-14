#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "MOustersGear.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "PlayerStateHost.h"
#include "SoundDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCAddGearToZone.h"
#include "Gpackets/GCRideMotorCycle.h"
#include "Gpackets/GCRideMotorCycleOK.h"
#include <map>
#include <string>
#include <vector>

// A creature wearing gear rides off on a motorcycle lying in the zone:
// the motorcycle goes on as an addon and leaves the zone, and the player
// hears it from where the player stands; with no such creature the
// motorcycle just leaves the zone. The player's own ride does the same
// for the player and answers the wait. A piece of the player's gear falls
// into the zone, or, without a zone, is freed.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool hasPlayer = true;
Race race = RACE_SLAYER;
int freed = 0;

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	std::string name;
	TYPE_SECTORPOSITION x = 0;
	TYPE_SECTORPOSITION y = 0;
	bool wear = true;
};
std::map<TYPE_OBJECTID, FakeCreature*> creatures;
FakeCreature me;
std::map<TYPE_OBJECTID, MItem*> lying;	// the zone's items
std::vector<MItem*> dropped;			// items the zone took

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* pCreature)
{
	static FakeCreature none;
	CHECK(pCreature != nullptr);
	return pCreature == nullptr ? none : *reinterpret_cast<FakeCreature*>(pCreature);
}
void Record(MCreature* p, const std::string& what) { calls.push_back(Fake(p).name + "." + what); }

struct Piece : MItem
{
	ITEM_CLASS m_Class;
	Piece(ITEM_CLASS itemClass, TYPE_OBJECTID id) : m_Class(itemClass) { SetID(id); SetCurrentDurability(100); }
	~Piece() override { freed++; }
	ITEM_CLASS GetItemClass() const override { return m_Class; }
	bool IsGearItem() const override { return true; }
	int GetMaxDurability() const override { return 100; }
};
struct Helm : Piece
{
	explicit Helm(TYPE_OBJECTID id) : Piece(ITEM_CLASS_HELM, id) {}
	bool IsGearSlotHelm() const override { return true; }
};
struct VampireCoat : Piece
{
	explicit VampireCoat(TYPE_OBJECTID id) : Piece(ITEM_CLASS_VAMPIRE_COAT, id) {}
	bool IsGearSlotVampireCoat() const override { return true; }
};
struct Circlet : Piece
{
	explicit Circlet(TYPE_OBJECTID id) : Piece(ITEM_CLASS_OUSTERS_CIRCLET, id) {}
	bool IsGearSlotOustersCirclet() const override { return true; }
};

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) -> MCreature* {
		CHECK(zoneExists);
		const auto found = creatures.find(id);
		return found == creatures.end() ? nullptr : Handle(*found->second);
	};
	host.GetItem = [](TYPE_OBJECTID id) -> MItem* {
		CHECK(zoneExists);
		const auto found = lying.find(id);
		return found == lying.end() ? nullptr : found->second;
	};
	host.RemoveItem = [](TYPE_OBJECTID id) {
		calls.push_back("zone frees|" + N(id));
		const auto found = lying.find(id);
		if (found == lying.end()) return false;
		delete found->second;
		lying.erase(found);
		return true;
	};
	host.AddItem = [](MItem* pItem, bool bDropping) {
		CHECK(zoneExists);
		calls.push_back("zone takes|" + N(pItem->GetID()) + (bDropping ? "|falling" : "|lying"));
		dropped.push_back(pItem);
		return true;
	};
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.IsWear = [](MCreature* p) { return Fake(p).wear; };
	host.SetAddonItem = [](MCreature* p, MItem* pItem) {
		Record(p, "addon|" + N(pItem->GetItemClass()) + "|" + N(pItem->GetID()));
		return true;
	};
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? Handle(me) : nullptr; };
	host.GetRace = []() { return race; };
	host.SetWaitVerifyNULL = []() { calls.push_back("verified"); };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.PlaySound = [](TYPE_SOUNDID soundID) { calls.push_back("sound|" + N(soundID)); };
	host.PlaySoundAt = [](TYPE_SOUNDID soundID, bool repeat, int x, int y) {
		calls.push_back("sound|" + N(soundID) + (repeat ? "|repeat|" : "|once|") + N(x) + "," + N(y));
	};
	return host;
}();

struct World : GameModelWorld
{
	FakeCreature ogre, wolf;
	MSlayerGear slayerGear;
	MVampireGear vampireGear;
	MOustersGear oustersGear;
	MSlayerGear* oldSlayerGear = g_pSlayerGear;
	MVampireGear* oldVampireGear = g_pVampireGear;
	MOustersGear* oldOustersGear = g_pOustersGear;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		me = {7, "Self", 30, 31};
		ogre = {1001, "Ogre", 10, 11};
		wolf = {1002, "Wolf", 12, 13, false};
		creatures = {{ogre.id, &ogre}, {wolf.id, &wolf}, {me.id, &me}};
		zoneExists = true;
		hasPlayer = true;
		race = RACE_SLAYER;
		calls.clear();
		freed = 0;
		lying = {{501, new Piece(ITEM_CLASS_MOTORCYCLE, 501)}, {502, new Piece(ITEM_CLASS_SWORD, 502)}};
		const ITEM_CLASS classes[] = {ITEM_CLASS_HELM, ITEM_CLASS_VAMPIRE_COAT, ITEM_CLASS_OUSTERS_CIRCLET};
		for (ITEM_CLASS itemClass : classes)
		{
			g_pItemTable->InitClass(itemClass, 1);
			testfw::MutableRow(*g_pItemTable, itemClass, 0).SetGrid(1, 1);
			testfw::MutableRow(*g_pItemTable, itemClass, 0).Race = FLAG_RACE_SLAYER | FLAG_RACE_VAMPIRE | FLAG_RACE_OUSTERS;
		}
		slayerGear.Init();
		vampireGear.Init();
		oustersGear.Init();
		g_pSlayerGear = &slayerGear;
		g_pVampireGear = &vampireGear;
		g_pOustersGear = &oustersGear;
	}
	~World()
	{
		WorldEvents::SetHost(oldEvents);
		PlayerState::SetHost(oldPlayer);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		g_pOustersGear = oldOustersGear;
		g_pVampireGear = oldVampireGear;
		g_pSlayerGear = oldSlayerGear;
		for (auto& item : lying) delete item.second;
		for (MItem* item : dropped) delete item;
		lying.clear();
		dropped.clear();
		creatures.clear();
	}
};

template<class PacketType>
void Decode(PacketType& packet, const PacketType& original)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	packet.read(stream);
	CHECK(stream.isEmpty());
}

void ReceiveRide(ObjectID_t rider, ObjectID_t motorcycle)
{
	GCRideMotorCycle original;
	original.setObjectID(rider);
	original.setTargetObjectID(motorcycle);
	GCRideMotorCycle packet;
	Decode(packet, original);
	GCRideMotorCycleHandler::execute(&packet, nullptr);
}

void ReceiveRideOK(ObjectID_t motorcycle)
{
	GCRideMotorCycleOK original;
	original.setObjectID(motorcycle);
	GCRideMotorCycleOK packet;
	Decode(packet, original);
	GCRideMotorCycleOKHandler::execute(&packet, nullptr);
}

void ReceiveDropped(int slot)
{
	GCAddGearToZone original;
	original.setSlotID(static_cast<SlotID_t>(slot));
	GCAddGearToZone packet;
	Decode(packet, original);
	GCAddGearToZoneHandler::execute(&packet, nullptr);
}

const std::string kMotorcycle = N(ITEM_CLASS_MOTORCYCLE);

} // namespace

TEST(RideGearReplies, AWearingCreatureRidesOffOnTheMotorcycleWhereThePlayerHearsIt)
{
	World world;
	ReceiveRide(1001, 501);
	CHECK(calls == std::vector<std::string>({"Ogre.addon|" + kMotorcycle + "|501", "zone frees|501",
		"sound|" + N(SOUND_WORLD_BIKE_GO) + "|once|30,31"}));
	CHECK_EQ(1, freed);
	// Without a player there is nobody to hear it (the executable's zone
	// still needs the player to remove the motorcycle).
	calls.clear();
	hasPlayer = false;
	lying[503] = new Piece(ITEM_CLASS_MOTORCYCLE, 503);
	ReceiveRide(1001, 503);
	CHECK(calls == std::vector<std::string>({"Ogre.addon|" + kMotorcycle + "|503", "zone frees|503"}));
}

TEST(RideGearReplies, OnlyAMotorcycleIsRiddenAndOnlyByAWearingCreature)
{
	World world;
	ReceiveRide(1001, 502);
	ReceiveRide(1001, 4242);
	ReceiveRide(1002, 501);
	CHECK(calls.empty());
	CHECK_EQ(0, freed);
	// With no such creature, the motorcycle just leaves the zone.
	ReceiveRide(4242, 502);
	ReceiveRide(4242, 501);
	// Nor with neither.
	ReceiveRide(4242, 4242);
	CHECK(calls == std::vector<std::string>{"zone frees|501"});
	calls.clear();
	zoneExists = false;
	ReceiveRide(1001, 501);
	CHECK(calls.empty());
}

TEST(RideGearReplies, ThePlayerRidesOffAndTheWaitIsAnswered)
{
	World world;
	ReceiveRideOK(501);
	CHECK(calls == std::vector<std::string>({"Self.addon|" + kMotorcycle + "|501", "zone frees|501",
		"sound|" + N(SOUND_WORLD_BIKE_GO), "verified"}));
	// Anything else, nothing, or no zone: only the wait is answered.
	calls.clear();
	ReceiveRideOK(502);
	ReceiveRideOK(4242);
	zoneExists = false;
	ReceiveRideOK(501);
	CHECK(calls == std::vector<std::string>(3, "verified"));
	calls.clear();
	zoneExists = true;
	hasPlayer = false;
	ReceiveRideOK(501);
	CHECK(calls.empty());
}

TEST(RideGearReplies, APieceOfGearFallsIntoTheZone)
{
	World world;
	CHECK(world.slayerGear.AddItem(new Helm(21), MSlayerGear::GEAR_SLAYER_HELM));
	ReceiveDropped(MSlayerGear::GEAR_SLAYER_HELM);
	CHECK(calls == std::vector<std::string>{"zone takes|21|falling"});
	CHECK(world.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_HELM) == nullptr);
	CHECK_EQ(0, freed);
	calls.clear();
	race = RACE_VAMPIRE;
	CHECK(world.vampireGear.AddItem(new VampireCoat(22), MVampireGear::GEAR_VAMPIRE_COAT));
	ReceiveDropped(MVampireGear::GEAR_VAMPIRE_COAT);
	race = RACE_OUSTERS;
	CHECK(world.oustersGear.AddItem(new Circlet(23), MOustersGear::GEAR_OUSTERS_CIRCLET));
	ReceiveDropped(MOustersGear::GEAR_OUSTERS_CIRCLET);
	CHECK(calls == std::vector<std::string>({"zone takes|22|falling", "zone takes|23|falling"}));
	CHECK(world.vampireGear.GetItem(MVampireGear::GEAR_VAMPIRE_COAT) == nullptr);
	CHECK(world.oustersGear.GetItem(MOustersGear::GEAR_OUSTERS_CIRCLET) == nullptr);
}

TEST(RideGearReplies, EachRacesEmptySlotDropsNothingAndWithoutAZoneEachPieceIsFreed)
{
	World world;
	const struct { Race race; int slot; } slots[] = {{RACE_SLAYER, MSlayerGear::GEAR_SLAYER_HELM},
		{RACE_VAMPIRE, MVampireGear::GEAR_VAMPIRE_COAT}, {RACE_OUSTERS, MOustersGear::GEAR_OUSTERS_CIRCLET}};
	for (const auto& s : slots)
	{
		race = s.race;
		ReceiveDropped(s.slot);
	}
	CHECK(calls.empty());
	CHECK(world.slayerGear.AddItem(new Helm(21), MSlayerGear::GEAR_SLAYER_HELM));
	CHECK(world.vampireGear.AddItem(new VampireCoat(22), MVampireGear::GEAR_VAMPIRE_COAT));
	CHECK(world.oustersGear.AddItem(new Circlet(23), MOustersGear::GEAR_OUSTERS_CIRCLET));
	zoneExists = false;
	for (const auto& s : slots)
	{
		race = s.race;
		ReceiveDropped(s.slot);
	}
	CHECK(calls.empty());
	CHECK_EQ(3, freed);
	CHECK(world.vampireGear.GetItem(MVampireGear::GEAR_VAMPIRE_COAT) == nullptr);
	CHECK(world.oustersGear.GetItem(MOustersGear::GEAR_OUSTERS_CIRCLET) == nullptr);
}

TEST(RideGearReplies, WithoutAZoneAPieceIsFreedAndAnEmptySlotDropsNothing)
{
	World world;
	ReceiveDropped(MSlayerGear::GEAR_SLAYER_HELM);
	CHECK(calls.empty());
	CHECK(world.slayerGear.AddItem(new Helm(21), MSlayerGear::GEAR_SLAYER_HELM));
	zoneExists = false;
	ReceiveDropped(MSlayerGear::GEAR_SLAYER_HELM);
	CHECK(calls.empty());
	CHECK_EQ(1, freed);
	CHECK(world.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_HELM) == nullptr);
	// Without a player, nothing at all.
	zoneExists = true;
	CHECK(world.slayerGear.AddItem(new Helm(24), MSlayerGear::GEAR_SLAYER_HELM));
	hasPlayer = false;
	ReceiveDropped(MSlayerGear::GEAR_SLAYER_HELM);
	CHECK(calls.empty());
	CHECK(world.slayerGear.GetItem(MSlayerGear::GEAR_SLAYER_HELM) != nullptr);
}

TEST(RideGearReplies, WithoutAHostTheZoneHasNoItemsAndTakesNone)
{
	World world;
	ZoneState::SetHost(nullptr);
	CHECK(ZoneState::GetItem(501) == nullptr);
	CHECK(!ZoneState::RemoveItem(501));
	Piece piece(ITEM_CLASS_SWORD, 9);
	CHECK(!ZoneState::AddItem(&piece, true));
	CHECK(calls.empty());
}
