#include "test_framework.h"
#include "gamemodel_world.h"
#include "DirectionSelection.h"
#include "MItem.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCFakeMove.h"
#include "Gpackets/GCFastMove.h"
#include <string>
#include <vector>

// A creature moving fast by a skill: a charging attack starts its effect
// from where it was to where it lands and leaves a short ghost trail; a
// teleport starts one effect where it was and another where it lands; a
// ducking wallop faces where it goes; a bike crash puts the creature on a
// motorcycle facing where it goes; any other skill only moves it. The
// player waiting on the skill's success is verified, and the player is
// held for as long as its own skill lasts. A fast-moving creature leaves
// an afterimage: a fake copy of it running the same move, which the zone
// takes or which is deleted.
namespace {

std::vector<std::string> calls;
bool hasPlayer = true;
PlayerState::WAIT_VERIFY waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
bool zoneTakesFakes = true;
bool makesFakes = true;
int bikesAlive = 0;

struct FakeCreature
{
	TYPE_OBJECTID id;
	TYPE_SECTORPOSITION x, y;
	int direction;
};
FakeCreature me{1001, 10, 11, 5};
FakeCreature other{1003, 50, 31, 3};
FakeCreature afterimage{9001, 0, 0, 0};

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* p) { return *reinterpret_cast<FakeCreature*>(p); }
std::string Id(MCreature* p) { return N(Fake(p).id); }

struct Bike : MItem
{
	Bike() { bikesAlive++; }
	~Bike() { bikesAlive--; }
	ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_MOTORCYCLE; }
};
MItemHost BikeHost()
{
	MItemHost host{};
	host.NewItem = [](ITEM_CLASS itemClass) -> MItem* { return itemClass == ITEM_CLASS_MOTORCYCLE ? new Bike : nullptr; };
	return host;
}
const MItemHost bikes = BikeHost();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? Handle(me) : nullptr; };
	host.GetID = []() { return me.id; };
	host.GetWaitVerify = []() { return waitVerify; };
	host.SetWaitVerifyNULL = []() { calls.push_back("verified"); };
	host.SetDelay = [](std::uint32_t milliseconds) { calls.push_back("held|" + N(milliseconds)); };
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.GetCreature = [](TYPE_OBJECTID id) {
		return id == other.id ? Handle(other) : id == me.id ? Handle(me) : nullptr;
	};
	host.NewFakeCreature = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) -> MCreature* {
		calls.push_back("afterimage of|" + Id(p) + "|" + N(x) + "," + N(y));
		return makesFakes ? Handle(afterimage) : nullptr;
	};
	host.AddFakeCreature = [](MCreature* p) { calls.push_back("zone takes|" + Id(p)); return zoneTakesFakes; };
	host.DeleteFakeCreature = [](MCreature* p) { calls.push_back("deleted|" + Id(p)); };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.GetDirection = [](MCreature* p) { return Fake(p).direction; };
	host.GetDirectionToPosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("faces|" + Id(p) + "|" + N(x) + "," + N(y));
		return 6;
	};
	host.FastMovePosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, bool server) {
		calls.push_back("fast move|" + Id(p) + "|" + N(x) + "," + N(y) + (server ? "|server" : ""));
	};
	host.SetGhost = [](MCreature* p, int flag, int delay) { calls.push_back("ghost|" + Id(p) + "|" + N(flag) + "|" + N(delay)); };
	host.AddEffectStatus = [](MCreature* p, EFFECTSTATUS status, std::uint32_t delayFrame) {
		calls.push_back("status|" + Id(p) + "|" + N(status) + "|" + N(delayFrame));
		return true;
	};
	host.SetAddonItem = [](MCreature* p, MItem* pItem) {
		calls.push_back("rides|" + Id(p) + "|" + N(pItem->GetItemClass()) + "|" + N(pItem->GetItemType()));
		return true;
	};
	host.SetInstallTurretDirect = [](MCreature* p, int direction) { calls.push_back("turret dir|" + Id(p) + "|" + N(direction)); };
	host.SetDirection = [](MCreature* p, int direction) { calls.push_back("turns|" + Id(p) + "|" + N(direction)); };
	host.SetCurrentDirection = [](MCreature* p, int direction) { calls.push_back("faces now|" + Id(p) + "|" + N(direction)); };
	host.SetInstallTurretCount = [](MCreature* p, int count) { calls.push_back("turret count|" + Id(p) + "|" + N(count)); };
	host.SetFakeFastMove = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("runs|" + Id(p) + "|" + N(x) + "," + N(y));
	};
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int startZ, int direction,
		TYPE_OBJECTID targetID, TYPE_SECTORPOSITION targetX, TYPE_SECTORPOSITION targetY, int targetZ, std::uint32_t delayFrame,
		MActionResult* pResult, bool bStartFromMainNode, int sX, int sY) {
		CHECK(startZ == 0 && targetZ == 0 && sX == 0 && sY == 0 && pResult == nullptr && !bStartFromMainNode);
		calls.push_back("effect|" + N(actionInfo) + "|" + N(startX) + "," + N(startY) + "|dir " + N(direction) + "|" + N(targetID)
			+ "@" + N(targetX) + "," + N(targetY) + "|" + N(delayFrame));
	};
	return host;
}();

struct World : GameModelWorld
{
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	const MItemHost* oldItems = MItem::GetHost();
	World()
	{
		calls.clear();
		hasPlayer = true;
		waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
		zoneTakesFakes = true;
		makesFakes = true;
		MItem::SetHost(&bikes);
	}
	~World()
	{
		MItem::SetHost(oldItems);
		WorldEvents::SetHost(oldEvents);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		PlayerState::SetHost(oldPlayer);
	}
};

void ReceiveFast(ObjectID_t id, int skill)
{
	GCFastMove packet;
	packet.setObjectID(id);
	packet.setXY(40, 41, 44, 45);
	packet.setSkillType(static_cast<SkillType_t>(skill));
	GCFastMoveHandler::execute(&packet, nullptr);
}

void ReceiveFake(ObjectID_t id)
{
	GCFakeMove packet;
	packet.setObjectID(id);
	packet.setXY(44, 45);
	GCFakeMoveHandler::execute(&packet, nullptr);
}

std::string Effect(int action, const std::string& from, int direction, const std::string& to, int delay)
{
	return "effect|" + N(action) + "|" + from + "|dir " + N(direction) + "|" + to + "|" + N(delay);
}

} // namespace

TEST(FastMoveReplies, AChargingAttackRushesWithAGhostTrail)
{
	World world;
	ReceiveFast(1003, SKILL_CHARGING_ATTACK);
	CHECK(calls == std::vector<std::string>({Effect(SKILL_CLIENT_CHARGING_ATTACK, "40,41", 3, "1003@44,45", 17),
		"fast move|1003|44,45|server", "ghost|1003|3|16"}));
	// The player's own: verified and held a second.
	calls.clear();
	ReceiveFast(1001, SKILL_CHARGING_ATTACK);
	CHECK(calls == std::vector<std::string>({"verified", Effect(SKILL_CLIENT_CHARGING_ATTACK, "40,41", 5, "1001@44,45", 17),
		"held|1000", "fast move|1001|44,45|server", "ghost|1001|3|16"}));
}

TEST(FastMoveReplies, ATeleportStartsAnEffectAtBothEnds)
{
	World world;
	ReceiveFast(1001, SKILL_TELEPORT);
	CHECK(calls == std::vector<std::string>({"verified", Effect(SKILL_CLIENT_TELEPORT, "40,41", 5, "1001@40,41", 20),
		"fast move|1001|44,45|server", "held|1250", "ghost|1001|255|18",
		Effect(SKILL_CLIENT_TELEPORT2, "44,45", 5, "1001@44,45", 20)}));
	calls.clear();
	ReceiveFast(1003, SKILL_TELEPORT);
	CHECK(calls == std::vector<std::string>({Effect(SKILL_CLIENT_TELEPORT, "40,41", 3, "1003@40,41", 20),
		"fast move|1003|44,45|server", "ghost|1003|255|18", Effect(SKILL_CLIENT_TELEPORT2, "44,45", 3, "1003@44,45", 20)}));
}

TEST(FastMoveReplies, ADuckingWallopFacesWhereItGoes)
{
	World world;
	ReceiveFast(1003, SKILL_DUCKING_WALLOP);
	CHECK(calls == std::vector<std::string>({"faces|1003|44,45", Effect(SKILL_CLIENT_DUCKING_WALLOP, "40,41", 6, "1003@40,41", 10),
		"fast move|1003|44,45|server", "ghost|1003|255|9"}));
	calls.clear();
	ReceiveFast(1001, SKILL_DUCKING_WALLOP);
	CHECK(calls == std::vector<std::string>({"verified", "faces|1001|44,45", Effect(SKILL_CLIENT_DUCKING_WALLOP, "40,41", 6, "1001@40,41", 10),
		"fast move|1001|44,45|server", "ghost|1001|255|9", "held|565"}));
}

TEST(FastMoveReplies, ABikeCrashRidesAMotorcycleTowardsTheCell)
{
	World world;
	// Facing from where the creature stands, not from the packet's start.
	const int direction = SelectFacingDirection(50, 31, 44, 45);
	CHECK(direction != SelectFacingDirection(40, 41, 44, 45));
	ReceiveFast(1003, SKILL_BIKE_CRASH);
	const std::string bike = N(ITEM_CLASS_MOTORCYCLE) + "|6";
	CHECK(calls == std::vector<std::string>({"status|1003|" + N(EFFECTSTATUS_BIKE_CRASH) + "|65535", "rides|1003|" + bike,
		"turret dir|1003|" + N(direction), "turns|1003|" + N(direction), "faces now|1003|" + N(direction), "turret count|1003|0",
		"fast move|1003|44,45|server"}));
	CHECK_EQ(0, bikesAlive);
	// The player's own: held three seconds.
	calls.clear();
	ReceiveFast(1001, SKILL_BIKE_CRASH);
	const std::string mine = N(SelectFacingDirection(10, 11, 44, 45));
	CHECK(calls == std::vector<std::string>({"verified", "status|1001|" + N(EFFECTSTATUS_BIKE_CRASH) + "|65535", "rides|1001|" + bike,
		"turret dir|1001|" + mine, "turns|1001|" + mine, "faces now|1001|" + mine, "turret count|1001|0", "held|3000",
		"fast move|1001|44,45|server"}));
	// No motorcycle to ride: it only moves.
	calls.clear();
	MItem::SetHost(nullptr);
	ReceiveFast(1003, SKILL_BIKE_CRASH);
	CHECK(calls == std::vector<std::string>{"fast move|1003|44,45|server"});
}

TEST(FastMoveReplies, AnotherSkillOnlyMoves)
{
	World world;
	ReceiveFast(1003, SKILL_DOUBLE_IMPACT);
	CHECK(calls == std::vector<std::string>{"fast move|1003|44,45|server"});
	// The player not waiting on its skill is not verified.
	calls.clear();
	waitVerify = PlayerState::WAIT_VERIFY_NULL;
	ReceiveFast(1001, SKILL_DOUBLE_IMPACT);
	CHECK(calls == std::vector<std::string>{"fast move|1001|44,45|server"});
}

TEST(FastMoveReplies, NobodyOrNoPlayerMovesNothing)
{
	World world;
	ReceiveFast(4242, SKILL_CHARGING_ATTACK);
	hasPlayer = false;
	ReceiveFast(1003, SKILL_CHARGING_ATTACK);
	ReceiveFast(1001, SKILL_CHARGING_ATTACK);
	CHECK(calls.empty());
}

TEST(FastMoveReplies, AFastMoverLeavesAnAfterimage)
{
	World world;
	ReceiveFake(1003);
	CHECK(calls == std::vector<std::string>({"afterimage of|1003|50,31", "runs|9001|44,45", "zone takes|9001"}));
	// The zone has it already: the new one is deleted.
	calls.clear();
	zoneTakesFakes = false;
	ReceiveFake(1003);
	CHECK(calls == std::vector<std::string>({"afterimage of|1003|50,31", "runs|9001|44,45", "zone takes|9001", "deleted|9001"}));
	// Nobody, or no afterimage made.
	calls.clear();
	ReceiveFake(4242);
	makesFakes = false;
	ReceiveFake(1003);
	CHECK(calls == std::vector<std::string>{"afterimage of|1003|50,31"});
}

TEST(FastMoveReplies, WithoutTheHostsNothingMoves)
{
	World world;
	ZoneState::SetHost(nullptr);
	CreatureState::SetHost(nullptr);
	CHECK(ZoneState::NewFakeCreature(Handle(other), 1, 2) == nullptr);
	CHECK(!ZoneState::AddFakeCreature(Handle(afterimage)));
	ZoneState::DeleteFakeCreature(Handle(afterimage));
	CHECK_EQ(0, CreatureState::GetDirectionToPosition(Handle(other), 1, 2));
	CreatureState::FastMovePosition(Handle(other), 1, 2, true);
	CreatureState::SetGhost(Handle(other), 3, 16);
	CreatureState::SetInstallTurretDirect(Handle(other), 2);
	CreatureState::SetInstallTurretCount(Handle(other), 0);
	CreatureState::SetFakeFastMove(Handle(afterimage), 1, 2);
	CHECK(calls.empty());
}
