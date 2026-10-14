#include "test_framework.h"
#include "gamemodel_world.h"
#include "MCreatureTable.h"
#include "MItem.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCDropItemToZone.h"
#include "Gpackets/GCEnterVampirePortal.h"
#include <string>
#include <vector>

// A creature entering a vampire portal stops at the portal's cell, takes
// it as its server position, vanishes into the portal and leaves the
// zone. An item dropped to the zone lands there; when a creature dropped
// it for its pet (a pet "drains" the item), the pet acts drained and makes
// its sound: a dog growls when its pet item is 30 or more and barks
// otherwise, a slayer's pet (a tank) makes its creature type's drained
// sound, and a later pet makes none.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
int creatureLookups = 0;
int fakeLookups = 0;

struct FakeCreature
{
	TYPE_OBJECTID id;
	TYPE_OBJECTID petID;
	int creatureType;
	TYPE_SECTORPOSITION x, y;
	MPetItem* pPetItem;
};
FakeCreature owner{1003, 2001, 1, 10, 11, nullptr};
FakeCreature pet{2001, OBJECTID_NULL, 2, 12, 13, nullptr};

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* p) { return *reinterpret_cast<FakeCreature*>(p); }

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) { creatureLookups++; return id == owner.id ? Handle(owner) : nullptr; };
	host.GetFakeCreature = [](TYPE_OBJECTID id) { fakeLookups++; return id == pet.id ? Handle(pet) : nullptr; };
	host.RemoveCreature = [](TYPE_OBJECTID id) { calls.push_back("leaves the zone|" + N(id)); return true; };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.SetStop = [](MCreature* p) { calls.push_back("stops|" + N(Fake(p).id)); };
	host.MovePosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("moves|" + N(Fake(p).id) + "|" + N(x) + "," + N(y));
		return true;
	};
	host.SetServerPosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("server at|" + N(Fake(p).id) + "|" + N(x) + "," + N(y));
	};
	host.GetPetID = [](MCreature* p) { return Fake(p).petID; };
	host.SetAction = [](MCreature* p, int action) { calls.push_back("acts|" + N(Fake(p).id) + "|" + N(action)); };
	host.GetPetItem = [](MCreature* p) { return Fake(p).pPetItem; };
	host.GetCreatureType = [](MCreature* p) { return Fake(p).creatureType; };
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.MoveIntoPortal = [](MCreature* p) { calls.push_back("into the portal|" + N(Fake(p).id)); };
	host.AddItemToZone = [](GCAddItemToZone* pPacket, bool bDropping) -> MItem* {
		calls.push_back("lands|" + N(pPacket->getObjectID()) + (bDropping ? "|falling" : ""));
		return nullptr;
	};
	host.PlaySoundAt = [](TYPE_SOUNDID soundID, bool repeat, int x, int y) {
		calls.push_back("sound|" + N(soundID) + (repeat ? "|repeat" : "") + "|" + N(x) + "," + N(y));
	};
	return host;
}();

struct World : GameModelWorld
{
	MPetItem petItem;
	CREATURE_TABLE creatureTypes;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		calls.clear();
		zoneExists = true;
		creatureLookups = fakeLookups = 0;
		owner = FakeCreature{1003, 2001, 1, 10, 11, nullptr};
		pet = FakeCreature{2001, OBJECTID_NULL, 2, 12, 13, &petItem};
		creatureTypes.Init(3);
		if (auto* row = creatureTypes.GetMutable(2))
		{
			row->SetCreatureTribe(CREATURETRIBE_NPC, ACTION_DRAINED + 1);
			row->SetActionSound(ACTION_DRAINED, 1, 77);
		}
		g_pCreatureTable = &creatureTypes;
	}
	~World()
	{
		WorldEvents::SetHost(oldEvents);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		g_pCreatureTable = oldCreatureTypes;
	}
	void Pet(int type, int number)
	{
		petItem.SetItemType(static_cast<TYPE_ITEMTYPE>(type));
		petItem.SetNumber(static_cast<TYPE_ITEM_NUMBER>(number));
	}
};

void ReceivePortal(ObjectID_t id, int x, int y)
{
	GCEnterVampirePortal packet;
	packet.setObjectID(id);
	packet.setX(static_cast<Coord_t>(x));
	packet.setY(static_cast<Coord_t>(y));
	GCEnterVampirePortalHandler::execute(&packet, nullptr);
}

void ReceiveDrop(ObjectID_t dropper)
{
	GCDropItemToZone packet;
	packet.setObjectID(501);
	packet.setX(40);
	packet.setY(41);
	packet.setDropPetOID(dropper);
	GCDropItemToZoneHandler::execute(&packet, nullptr);
}

} // namespace

TEST(PortalDropReplies, ACreatureEntersAPortalAndLeaves)
{
	World world;
	ReceivePortal(1003, 30, 31);
	CHECK(calls == std::vector<std::string>({"stops|1003", "moves|1003|30,31", "server at|1003|30,31",
		"into the portal|1003", "leaves the zone|1003"}));
	// Nobody by that id, or no zone: nothing.
	calls.clear();
	ReceivePortal(4242, 30, 31);
	zoneExists = false;
	ReceivePortal(1003, 30, 31);
	CHECK(calls.empty());
}

TEST(PortalDropReplies, ADroppedItemLandsFalling)
{
	World world;
	ReceiveDrop(0);
	CHECK(calls == std::vector<std::string>{"lands|501|falling"});
	// Nobody dropped it: no creature is looked up.
	CHECK_EQ(0, creatureLookups);
	// A dropper not in the zone, or without a pet, or a pet not there.
	calls.clear();
	ReceiveDrop(4242);
	owner.petID = OBJECTID_NULL;
	ReceiveDrop(1003);
	// A dropper without a pet: no pet is looked up.
	CHECK_EQ(0, fakeLookups);
	owner.petID = 2009;
	ReceiveDrop(1003);
	CHECK(calls == std::vector<std::string>(3, "lands|501|falling"));
}

TEST(PortalDropReplies, ADogPetGrowlsOrBarks)
{
	World world;
	world.Pet(1, 30);
	ReceiveDrop(1003);
	CHECK(calls == std::vector<std::string>({"lands|501|falling", "acts|2001|" + N(ACTION_DRAINED), "sound|" + N(SOUND_WORLD_DOG_GRRR) + "|40,41"}));
	calls.clear();
	world.Pet(2, 29);
	ReceiveDrop(1003);
	CHECK(calls == std::vector<std::string>({"lands|501|falling", "acts|2001|" + N(ACTION_DRAINED), "sound|" + N(SOUND_WORLD_DOG_BARK) + "|40,41"}));
}

TEST(PortalDropReplies, ASlayersPetMakesItsDrainedSound)
{
	World world;
	world.Pet(3, 50);
	ReceiveDrop(1003);
	CHECK(calls == std::vector<std::string>({"lands|501|falling", "acts|2001|" + N(ACTION_DRAINED), "sound|77|12,13"}));
	// A pet type without a drained sound, a later pet, or no pet item: no
	// sound.
	calls.clear();
	pet.creatureType = 1;
	ReceiveDrop(1003);
	pet.creatureType = 2;
	world.Pet(4, 50);
	ReceiveDrop(1003);
	pet.pPetItem = nullptr;
	ReceiveDrop(1003);
	CHECK(calls == std::vector<std::string>({"lands|501|falling", "acts|2001|" + N(ACTION_DRAINED),
		"lands|501|falling", "acts|2001|" + N(ACTION_DRAINED), "lands|501|falling", "acts|2001|" + N(ACTION_DRAINED)}));
}

TEST(PortalDropReplies, WithoutTheHostsNothingMoves)
{
	World world;
	WorldEvents::SetHost(nullptr);
	WorldEvents::MoveIntoPortal(Handle(owner));
	ZoneState::SetHost(nullptr);
	CreatureState::SetHost(nullptr);
	CHECK(ZoneState::GetFakeCreature(2001) == nullptr);
	CHECK(CreatureState::GetPetID(Handle(owner)) == OBJECTID_NULL);
	CHECK(CreatureState::GetPetItem(Handle(pet)) == nullptr);
	CreatureState::SetAction(Handle(pet), ACTION_DRAINED);
	CHECK(calls.empty());
}
