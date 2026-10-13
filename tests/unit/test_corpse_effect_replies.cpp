#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "ConvertDuration.h"
#include "MEffectStatusTable.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCAddEffect.h"
#include "Gpackets/GCRemoveCorpseHead.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// An effect status lands on a creature standing in the zone or lying in its
// corpse: its sound plays, its action effect starts for a few statuses, and
// it is added for the converted duration (not doom, critical wounds or a
// third back stab). A siege laser is hidden from attacking players. A
// trying status flashes the regen tower's bead. A creature standing, or
// lying in its corpse, loses its head.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool hasPlayer = true;
std::vector<EFFECTSTATUS> playerEffects;

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	std::string name;
	TYPE_SECTORPOSITION x = 0;
	TYPE_SECTORPOSITION y = 0;
	int direction = 0;
};
std::map<TYPE_OBJECTID, FakeCreature*> creatures;
// Corpse item ID -> the creature lying in it.
std::map<TYPE_OBJECTID, FakeCreature*> corpses;
FakeCreature me{7, "Self"};

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
	host.GetCorpseCreature = [](TYPE_OBJECTID id) -> MCreature* {
		CHECK(zoneExists);
		const auto found = corpses.find(id);
		return found == corpses.end() ? nullptr : Handle(*found->second);
	};
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.GetDirection = [](MCreature* p) { return Fake(p).direction; };
	host.AddEffectStatus = [](MCreature* p, EFFECTSTATUS status, std::uint32_t delayFrame) {
		Record(p, "effect|" + N(status) + "|" + N(delayFrame));
		return true;
	};
	host.RemoveHead = [](MCreature* p) { Record(p, "headless"); };
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? Handle(me) : nullptr; };
	host.HasEffectStatus = [](EFFECTSTATUS status) {
		for (EFFECTSTATUS effect : playerEffects)
			if (effect == status) return true;
		return false;
	};
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.PlaySound = [](TYPE_SOUNDID soundID) { calls.push_back("sound|" + N(soundID)); };
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int, int direction,
		TYPE_OBJECTID targetID, TYPE_SECTORPOSITION, TYPE_SECTORPOSITION, int, std::uint32_t delayFrame,
		MActionResult* pResult, bool bStartFromMainNode, int, int) {
		CHECK(pResult == nullptr);
		calls.push_back("action|" + N(actionInfo) + "|" + N(startX) + "," + N(startY) + "|dir " + N(direction) + "|" + N(targetID)
			+ "|" + N(delayFrame) + (bStartFromMainNode ? "|main" : "|resume"));
	};
	return host;
}();

struct World : GameModelWorld
{
	FakeCreature ogre, lying;
	EFFECTSTATUS_TABLE statuses;
	EFFECTSTATUS_TABLE* oldStatuses = g_pEffectStatusTable;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		ogre = {1001, "Ogre", 10, 11, 3};
		lying = {5001, "Lying", 20, 21, 5};
		creatures = {{ogre.id, &ogre}};
		corpses = {{9001, &lying}};
		zoneExists = true;
		hasPlayer = true;
		playerEffects.clear();
		calls.clear();
		statuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &statuses;
	}
	~World()
	{
		WorldEvents::SetHost(oldEvents);
		PlayerState::SetHost(oldPlayer);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		g_pEffectStatusTable = oldStatuses;
		creatures.clear();
		corpses.clear();
	}
	EFFECTSTATUS_NODE& Row(EFFECTSTATUS status) { return testfw::MutableRow(statuses, status); }
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

void ReceiveEffect(ObjectID_t id, int effect, Duration_t duration = 50)
{
	GCAddEffect original;
	original.setObjectID(id);
	original.setEffectID(static_cast<EffectID_t>(effect));
	original.setDuration(duration);
	GCAddEffect packet;
	Decode(packet, original);
	GCAddEffectHandler::execute(&packet, nullptr);
}

void ReceiveRemoveHead(ObjectID_t id)
{
	GCRemoveCorpseHead original;
	original.setObjectID(id);
	GCRemoveCorpseHead packet;
	Decode(packet, original);
	GCRemoveCorpseHeadHandler::execute(&packet, nullptr);
}

std::string Effect(const char* who, EFFECTSTATUS status, Duration_t duration = 50)
{
	return std::string(who) + ".effect|" + N(status) + "|" + N(ConvertDurationToFrame(duration));
}

} // namespace

TEST(CorpseEffectReplies, AnEffectStatusLandsOnTheCreatureWithItsSound)
{
	World world;
	ReceiveEffect(1001, EFFECTSTATUS_CURSE_PARALYSIS);
	CHECK(calls == std::vector<std::string>{Effect("Ogre", EFFECTSTATUS_CURSE_PARALYSIS)});
	calls.clear();
	world.Row(EFFECTSTATUS_CURSE_PARALYSIS).SoundID = 42;
	ReceiveEffect(1001, EFFECTSTATUS_CURSE_PARALYSIS);
	CHECK(calls == std::vector<std::string>({"sound|42", Effect("Ogre", EFFECTSTATUS_CURSE_PARALYSIS)}));
}

TEST(CorpseEffectReplies, AnEffectStatusLandsOnTheCreatureLyingInACorpse)
{
	World world;
	ReceiveEffect(9001, EFFECTSTATUS_CURSE_PARALYSIS);
	CHECK(calls == std::vector<std::string>{Effect("Lying", EFFECTSTATUS_CURSE_PARALYSIS)});
	calls.clear();
	ReceiveEffect(4242, EFFECTSTATUS_CURSE_PARALYSIS);
	ReceiveEffect(1001, EFFECTSTATUS_MAX);
	zoneExists = false;
	ReceiveEffect(1001, EFFECTSTATUS_CURSE_PARALYSIS);
	CHECK(calls.empty());
}

TEST(CorpseEffectReplies, DoomCriticalWoundsAndTheThirdBackStabAreNotAdded)
{
	World world;
	ReceiveEffect(1001, EFFECTSTATUS_CURSE_DOOM);
	ReceiveEffect(1001, EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS);
	ReceiveEffect(1001, EFFECTSTATUS_BACK_STAB_3);
	CHECK(calls.empty());
}

TEST(CorpseEffectReplies, SixStatusesStartTheirActionFromTheCreature)
{
	World world;
	const struct { EFFECTSTATUS status; std::uint32_t delay; bool fromMain; bool added; } rows[] = {
		{EFFECTSTATUS_BACK_STAB_3, 18, false, false},
		{EFFECTSTATUS_ICE_OF_SOUL_STONE, 19, false, true},
		{EFFECTSTATUS_TRAP_TRIGGERED, ConvertDurationToFrame(50), true, true},
		{EFFECTSTATUS_TRAPPED, ConvertDurationToFrame(50), false, true},
		{EFFECTSTATUS_TRYING, 0xFFFF, false, true},
		{EFFECTSTATUS_GROUND_ELEMENTAL_CENTER, 0xFFFF, false, true},
	};
	for (const auto& row : rows)
	{
		calls.clear();
		world.Row(row.status).ActionInfo = SKILL_DOUBLE_IMPACT;
		ReceiveEffect(1001, row.status);
		std::vector<std::string> expected = {"action|" + N(SKILL_DOUBLE_IMPACT) + "|10,11|dir 3|1001|" + N(row.delay) + (row.fromMain ? "|main" : "|resume")};
		if (row.added) expected.push_back(Effect("Ogre", row.status));
		CHECK(calls == expected);
	}
	// Without an action in the table, only the status is added.
	calls.clear();
	world.Row(EFFECTSTATUS_TRAPPED).ActionInfo = ACTIONINFO_NULL;
	ReceiveEffect(1001, EFFECTSTATUS_TRAPPED);
	CHECK(calls == std::vector<std::string>{Effect("Ogre", EFFECTSTATUS_TRAPPED)});
}

TEST(CorpseEffectReplies, ASiegeLaserIsHiddenFromAttackingPlayers)
{
	World world;
	ReceiveEffect(1001, EFFECTSTATUS_TURRET_LASER);
	CHECK(calls == std::vector<std::string>{Effect("Ogre", EFFECTSTATUS_TURRET_LASER)});
	const EFFECTSTATUS attackers[] = {EFFECTSTATUS_SIEGE_ATTACKER_1, EFFECTSTATUS_SIEGE_ATTACKER_2, EFFECTSTATUS_SIEGE_ATTACKER_3,
		EFFECTSTATUS_SIEGE_ATTACKER_4, EFFECTSTATUS_SIEGE_ATTACKER_5};
	for (EFFECTSTATUS attacker : attackers)
	{
		calls.clear();
		playerEffects = {attacker};
		ReceiveEffect(1001, EFFECTSTATUS_TURRET_LASER);
		CHECK(calls.empty());
	}
	// Without a player it shows.
	calls.clear();
	hasPlayer = false;
	ReceiveEffect(1001, EFFECTSTATUS_TURRET_LASER);
	CHECK(calls == std::vector<std::string>{Effect("Ogre", EFFECTSTATUS_TURRET_LASER)});
}

TEST(CorpseEffectReplies, ATryingStatusFlashesTheRegenTowerBead)
{
	World world;
	const EFFECTSTATUS trying[] = {EFFECTSTATUS_SLAYER_TRYING_1, EFFECTSTATUS_SLAYER_TRYING_2, EFFECTSTATUS_SLAYER_TRYING_3,
		EFFECTSTATUS_VAMPIRE_TRYING_1, EFFECTSTATUS_VAMPIRE_TRYING_2, EFFECTSTATUS_VAMPIRE_TRYING_3,
		EFFECTSTATUS_OUSTERS_TRYING_1, EFFECTSTATUS_OUSTERS_TRYING_2, EFFECTSTATUS_OUSTERS_TRYING_3};
	for (EFFECTSTATUS status : trying)
	{
		calls.clear();
		ReceiveEffect(1001, status);
		CHECK(calls == std::vector<std::string>({Effect("Ogre", status),
			"action|" + N(REGEN_TOWER_FLASH_BEAD) + "|10,11|dir " + N(DIRECTION_DOWN) + "|1001|7|main"}));
	}
}

TEST(CorpseEffectReplies, AStandingOrLyingCreatureLosesItsHead)
{
	World world;
	ReceiveRemoveHead(1001);
	ReceiveRemoveHead(9001);
	ReceiveRemoveHead(4242);
	zoneExists = false;
	ReceiveRemoveHead(1001);
	CHECK(calls == std::vector<std::string>({"Ogre.headless", "Lying.headless"}));
}
