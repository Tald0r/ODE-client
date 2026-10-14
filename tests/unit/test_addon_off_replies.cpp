#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "PlayerStateHost.h"
#include "SoundDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCGetOffMotorCycle.h"
#include "Gpackets/GCGetOffMotorCycleOK.h"
#include "Gpackets/GCTakeOff.h"
#include <map>
#include <string>
#include <vector>

// A creature wearing gear stops and takes an addon off. A creature gets
// off its motorcycle: the motorcycle is no longer drawn, it walks, and the
// player hears it from where the player stands. The player's own get-off
// answer does the same for the player, answers the wait, and plays the
// sound without a place.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool hasPlayer = true;

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

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* pCreature)
{
	static FakeCreature none;
	CHECK(pCreature != nullptr);
	return pCreature == nullptr ? none : *reinterpret_cast<FakeCreature*>(pCreature);
}
void Record(MCreature* p, const std::string& what) { calls.push_back(Fake(p).name + "." + what); }

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) -> MCreature* {
		CHECK(zoneExists);
		const auto found = creatures.find(id);
		return found == creatures.end() ? nullptr : Handle(*found->second);
	};
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.IsWear = [](MCreature* p) { return Fake(p).wear; };
	host.SetStop = [](MCreature* p) { Record(p, "stop"); };
	host.RemoveAddon = [](MCreature* p, int addon) {
		Record(p, "off|" + N(addon));
		return true;
	};
	host.SetMoveDevice = [](MCreature* p, CreatureState::MOVE_DEVICE device) { Record(p, "move|" + N(device)); };
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? Handle(me) : nullptr; };
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
		calls.clear();
	}
	~World()
	{
		WorldEvents::SetHost(oldEvents);
		PlayerState::SetHost(oldPlayer);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
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

void ReceiveTakeOff(ObjectID_t id, SlotID_t slot)
{
	GCTakeOff original;
	original.setObjectID(id);
	original.setSlotID(slot);
	GCTakeOff packet;
	Decode(packet, original);
	GCTakeOffHandler::execute(&packet, nullptr);
}

void ReceiveGetOff(ObjectID_t id)
{
	GCGetOffMotorCycle original;
	original.setObjectID(id);
	GCGetOffMotorCycle packet;
	Decode(packet, original);
	GCGetOffMotorCycleHandler::execute(&packet, nullptr);
}

void ReceiveGetOffOK()
{
	GCGetOffMotorCycleOK original;
	GCGetOffMotorCycleOK packet;
	Decode(packet, original);
	GCGetOffMotorCycleOKHandler::execute(&packet, nullptr);
}

} // namespace

TEST(AddonOffReplies, AWearingCreatureStopsAndTakesTheAddonOff)
{
	World world;
	ReceiveTakeOff(1001, 4);
	CHECK(calls == std::vector<std::string>({"Ogre.stop", "Ogre.off|4"}));
	// A creature not wearing gear, nobody by that ID, or no zone: nothing.
	calls.clear();
	ReceiveTakeOff(1002, 4);
	ReceiveTakeOff(4242, 4);
	zoneExists = false;
	ReceiveTakeOff(1001, 4);
	CHECK(calls.empty());
}

TEST(AddonOffReplies, ACreatureGetsOffItsMotorcycleWhereThePlayerHearsIt)
{
	World world;
	ReceiveGetOff(1001);
	CHECK(calls == std::vector<std::string>({"Ogre.off|" + N(ADDON_MOTOR), "Ogre.move|" + N(CreatureState::MOVE_DEVICE_WALK),
		"sound|" + N(SOUND_WORLD_BIKE_STOP) + "|once|30,31"}));
	// Without a player there is nobody to hear it.
	calls.clear();
	hasPlayer = false;
	ReceiveGetOff(1001);
	CHECK(calls == std::vector<std::string>({"Ogre.off|" + N(ADDON_MOTOR), "Ogre.move|" + N(CreatureState::MOVE_DEVICE_WALK)}));
	calls.clear();
	hasPlayer = true;
	ReceiveGetOff(1002);
	ReceiveGetOff(4242);
	zoneExists = false;
	ReceiveGetOff(1001);
	CHECK(calls.empty());
}

TEST(AddonOffReplies, ThePlayerGetsOffItsMotorcycle)
{
	World world;
	ReceiveGetOffOK();
	CHECK(calls == std::vector<std::string>({"Self.off|" + N(ADDON_MOTOR), "Self.move|" + N(CreatureState::MOVE_DEVICE_WALK),
		"verified", "sound|" + N(SOUND_WORLD_BIKE_STOP)}));
	calls.clear();
	hasPlayer = false;
	ReceiveGetOffOK();
	CHECK(calls.empty());
}

TEST(AddonOffReplies, TheEntriesPassTheirArgumentsAndAnswerOn)
{
	World world;
	CHECK(CreatureState::RemoveAddon(Handle(world.ogre), 4));
	CreatureState::SetMoveDevice(Handle(world.ogre), CreatureState::MOVE_DEVICE_RIDE);
	WorldEvents::PlaySoundAt(SOUND_WORLD_BIKE_STOP, true, 1, 2);
	CHECK(calls == std::vector<std::string>({"Ogre.off|4", "Ogre.move|" + N(CreatureState::MOVE_DEVICE_RIDE),
		"sound|" + N(SOUND_WORLD_BIKE_STOP) + "|repeat|1,2"}));
}

TEST(AddonOffReplies, WithoutHostsAnAddonStaysOn)
{
	World world;
	CreatureState::SetHost(nullptr);
	WorldEvents::SetHost(nullptr);
	CHECK(!CreatureState::RemoveAddon(Handle(world.ogre), 4));
	CreatureState::SetMoveDevice(Handle(world.ogre), CreatureState::MOVE_DEVICE_RIDE);
	WorldEvents::PlaySoundAt(SOUND_WORLD_BIKE_STOP, false, 1, 2);
	CHECK(calls.empty());
	// The devices keep MCreature's order.
	CHECK_EQ(0, static_cast<int>(CreatureState::MOVE_DEVICE_WALK));
	CHECK_EQ(1, static_cast<int>(CreatureState::MOVE_DEVICE_RIDE));
	CHECK_EQ(2, static_cast<int>(CreatureState::MOVE_DEVICE_SUMMON_SYLPH));
	CHECK_EQ(3, static_cast<int>(CreatureState::MOVE_DEVICE_MAX));
}
