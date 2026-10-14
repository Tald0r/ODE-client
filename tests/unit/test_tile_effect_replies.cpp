#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "ConvertDuration.h"
#include "DirectionSelection.h"
#include "MEffectStatusTable.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCAddEffectToTile.h"
#include <set>
#include <string>
#include <vector>

// An effect added to a tile starts its effect status's skill at the tile
// for the effect's duration. A trap shows only to a slayer, and a siege
// turret's laser not to the siege's attackers; a dragon tornado (or its
// child) is its own effect. Some effects do more: the regen tower's
// trying shows its light, luck and misfortune their auras, a ground
// elemental's aura, a forbidden darkness and the fury of gnome mark the
// zone's cell, and a halo strikes its creature from the tile.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool hasPlayer = true;
bool playerIsSlayer = true;
std::set<int> playerEffects;

struct FakeCreature
{
	TYPE_OBJECTID id;
	TYPE_SECTORPOSITION x, y;
};
FakeCreature me{1001, 5, 6};
FakeCreature haloed{1003, 50, 31};

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* p) { return *reinterpret_cast<FakeCreature*>(p); }

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? Handle(me) : nullptr; };
	host.HasEffectStatus = [](EFFECTSTATUS status) { return playerEffects.count(status) != 0; };
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) { return id == haloed.id ? Handle(haloed) : nullptr; };
	host.SetGroundElemental = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { calls.push_back("ground elemental|" + N(x) + "," + N(y)); };
	host.SetDarknessForbidden = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { calls.push_back("darkness forbidden|" + N(x) + "," + N(y)); };
	host.SetFuryOfGnome = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { calls.push_back("fury of gnome|" + N(x) + "," + N(y)); };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.IsSlayer = [](MCreature* p) { CHECK(p != nullptr); return playerIsSlayer; };
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int startZ, int direction,
		TYPE_OBJECTID targetID, TYPE_SECTORPOSITION targetX, TYPE_SECTORPOSITION targetY, int targetZ, std::uint32_t delayFrame,
		MActionResult* pResult, bool bStartFromMainNode, int sX, int sY) {
		CHECK(startZ == 0 && targetZ == 0 && pResult == nullptr && sX == 0 && sY == 0);
		calls.push_back("effect|" + N(actionInfo) + "|" + N(startX) + "," + N(startY) + "|dir " + N(direction) + "|"
			+ (targetID == OBJECTID_NULL ? std::string("nobody") : N(targetID)) + "@" + N(targetX) + "," + N(targetY)
			+ "|" + N(delayFrame) + (bStartFromMainNode ? "|main" : ""));
	};
	host.SetDragonTornado = [](int effectStatus, std::uint32_t objectID, int x, int y) {
		calls.push_back("tornado|" + N(effectStatus) + "|" + N(objectID) + "|" + N(x) + "," + N(y));
	};
	return host;
}();

struct World : GameModelWorld
{
	EFFECTSTATUS_TABLE statuses;
	EFFECTSTATUS_TABLE* oldStatuses = g_pEffectStatusTable;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		calls.clear();
		zoneExists = true;
		hasPlayer = true;
		playerIsSlayer = true;
		playerEffects.clear();
		statuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &statuses;
	}
	~World()
	{
		WorldEvents::SetHost(oldEvents);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		PlayerState::SetHost(oldPlayer);
		g_pEffectStatusTable = oldStatuses;
	}
	EFFECTSTATUS_NODE& Row(EFFECTSTATUS status) { return testfw::MutableRow(statuses, status); }
};

const int kDuration = 30;

void Receive(int effect, ObjectID_t objectID = 0)
{
	GCAddEffectToTile packet;
	packet.setEffectID(static_cast<EffectID_t>(effect));
	packet.setDuration(kDuration);
	packet.setObjectID(objectID);
	packet.setXY(40, 41);
	GCAddEffectToTileHandler::execute(&packet, nullptr);
}

// An effect at the tile for the effect's duration.
std::string AtTile(int action)
{
	return "effect|" + N(action) + "|40,41|dir " + N(DIRECTION_DOWN) + "|nobody@40,41|" + N(ConvertDurationToFrame(kDuration)) + "|main";
}

} // namespace

TEST(TileEffectReplies, AnEffectsSkillStartsAtTheTile)
{
	World world;
	world.Row(EFFECTSTATUS_TRAPPED).ActionInfo = SKILL_DOUBLE_IMPACT;
	Receive(EFFECTSTATUS_TRAPPED);
	CHECK(calls == std::vector<std::string>{AtTile(SKILL_DOUBLE_IMPACT)});
	// An effect without a skill shows nothing of its own.
	calls.clear();
	Receive(EFFECTSTATUS_POISON);
	CHECK(calls.empty());
}

TEST(TileEffectReplies, SomeEffectsShowMore)
{
	World world;
	Receive(EFFECTSTATUS_TRYING);
	Receive(EFFECTSTATUS_LUCKY);
	Receive(EFFECTSTATUS_MISFORTUNE);
	CHECK(calls == std::vector<std::string>({AtTile(REGEN_TOWER_TRING_POSITION_LIGHT), AtTile(RESULT_LUCKY_AURA), AtTile(RESULT_MISFORTUNE_AURA)}));
	// With a skill of their own, that comes first.
	calls.clear();
	world.Row(EFFECTSTATUS_LUCKY).ActionInfo = SKILL_DOUBLE_IMPACT;
	Receive(EFFECTSTATUS_LUCKY);
	CHECK(calls == std::vector<std::string>({AtTile(SKILL_DOUBLE_IMPACT), AtTile(RESULT_LUCKY_AURA)}));
}

TEST(TileEffectReplies, GroundEffectsMarkTheZonesCell)
{
	World world;
	Receive(EFFECTSTATUS_GROUND_ELEMENTAL_AURA);
	Receive(EFFECTSTATUS_DARKNESS_FORBIDDEN);
	Receive(EFFECTSTATUS_FURY_OF_GNOME);
	CHECK(calls == std::vector<std::string>({"ground elemental|40,41", "darkness forbidden|40,41", "fury of gnome|40,41"}));
	calls.clear();
	zoneExists = false;
	Receive(EFFECTSTATUS_GROUND_ELEMENTAL_AURA);
	Receive(EFFECTSTATUS_DARKNESS_FORBIDDEN);
	Receive(EFFECTSTATUS_FURY_OF_GNOME);
	CHECK(calls.empty());
}

TEST(TileEffectReplies, AHaloStrikesItsCreatureFromTheTile)
{
	World world;
	const int direction = SelectFacingDirection(40, 41, 50, 31);
	CHECK(direction != SelectFacingDirection(50, 31, 40, 41));
	Receive(EFFECTSTATUS_HALO, 1003);
	CHECK(calls == std::vector<std::string>{"effect|" + N(SKILL_CLIENT_HALO_ATTACK) + "|40,41|dir " + N(direction) + "|1003@50,31|12|main"});
	// Its creature gone, or no zone: nothing.
	calls.clear();
	Receive(EFFECTSTATUS_HALO, 4242);
	zoneExists = false;
	Receive(EFFECTSTATUS_HALO, 1003);
	CHECK(calls.empty());
}

TEST(TileEffectReplies, ATrapShowsOnlyToASlayer)
{
	World world;
	world.Row(EFFECTSTATUS_TRAP_INSTALLED).ActionInfo = SKILL_DOUBLE_IMPACT;
	Receive(EFFECTSTATUS_TRAP_INSTALLED);
	CHECK(calls == std::vector<std::string>{AtTile(SKILL_DOUBLE_IMPACT)});
	calls.clear();
	playerIsSlayer = false;
	Receive(EFFECTSTATUS_TRAP_INSTALLED);
	// No player at all, even with slayers about: hidden, never asked.
	playerIsSlayer = true;
	hasPlayer = false;
	Receive(EFFECTSTATUS_TRAP_INSTALLED);
	CHECK(calls.empty());
	// Another effect needs no player.
	world.Row(EFFECTSTATUS_TRAPPED).ActionInfo = SKILL_DOUBLE_IMPACT;
	Receive(EFFECTSTATUS_TRAPPED);
	CHECK(calls == std::vector<std::string>{AtTile(SKILL_DOUBLE_IMPACT)});
}

TEST(TileEffectReplies, ASiegeTurretsLaserIsHiddenFromTheAttackers)
{
	World world;
	world.Row(EFFECTSTATUS_TURRET_LASER).ActionInfo = SKILL_DOUBLE_IMPACT;
	Receive(EFFECTSTATUS_TURRET_LASER);
	CHECK(calls == std::vector<std::string>{AtTile(SKILL_DOUBLE_IMPACT)});
	for (EFFECTSTATUS attacker : {EFFECTSTATUS_SIEGE_ATTACKER_1, EFFECTSTATUS_SIEGE_ATTACKER_2, EFFECTSTATUS_SIEGE_ATTACKER_3,
		EFFECTSTATUS_SIEGE_ATTACKER_4, EFFECTSTATUS_SIEGE_ATTACKER_5})
	{
		calls.clear();
		playerEffects = {attacker};
		Receive(EFFECTSTATUS_TURRET_LASER);
		CHECK(calls.empty());
	}
	// A defender, or no player, sees it.
	calls.clear();
	playerEffects = {EFFECTSTATUS_SIEGE_DEFENDERL};
	Receive(EFFECTSTATUS_TURRET_LASER);
	hasPlayer = false;
	playerEffects = {EFFECTSTATUS_SIEGE_ATTACKER_1};
	Receive(EFFECTSTATUS_TURRET_LASER);
	CHECK(calls == std::vector<std::string>(2, AtTile(SKILL_DOUBLE_IMPACT)));
}

TEST(TileEffectReplies, ADragonTornadoIsItsOwnEffect)
{
	World world;
	world.Row(EFFECTSTATUS_DRAGON_TORNADO).ActionInfo = SKILL_DOUBLE_IMPACT;
	Receive(EFFECTSTATUS_DRAGON_TORNADO, 7001);
	Receive(EFFECTSTATUS_DRAGON_TORNADO_CHILD, 7002);
	CHECK(calls == std::vector<std::string>({"tornado|" + N(EFFECTSTATUS_DRAGON_TORNADO) + "|7001|40,41",
		"tornado|" + N(EFFECTSTATUS_DRAGON_TORNADO_CHILD) + "|7002|40,41"}));
}

TEST(TileEffectReplies, WithoutTheHostsNothingIsMarked)
{
	World world;
	ZoneState::SetHost(nullptr);
	WorldEvents::SetHost(nullptr);
	ZoneState::SetGroundElemental(1, 2);
	ZoneState::SetDarknessForbidden(1, 2);
	ZoneState::SetFuryOfGnome(1, 2);
	WorldEvents::SetDragonTornado(EFFECTSTATUS_DRAGON_TORNADO, 7001, 1, 2);
	Receive(EFFECTSTATUS_GROUND_ELEMENTAL_AURA);
	Receive(EFFECTSTATUS_DRAGON_TORNADO, 7001);
	CHECK(calls.empty());
}

TEST(TileEffectReplies, AnEffectPastTheEffectStatusesShowsNothing)
{
	World world;
	// The wire's effect ID is 16 bits; the effect statuses' enum holds 0 to
	// 511.
	Receive(600);
	Receive(65535);
	CHECK(calls.empty());
}
