#include "test_framework.h"
#include "gamemodel_world.h"
#include "MActionResult.h"
#include "MCreatureTable.h"
#include "MStatus.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCAddBat.h"
#include "Gpackets/GCAddWolf.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

// A vampire as a bat or a wolf. One the zone does not know yet is made as
// a wearing creature of the form's type with the packet's name, ID, cell,
// direction, guild and health, standing, and added to the zone (or
// dropped if the zone refuses it); a bat flies with the packet's colour
// (its own, 0xFFFF, when none), and a wolf walks. One the zone knows takes
// the form in place: it moves to the cell, takes the direction, guild and
// health, and the transformation's effect starts from it, its result
// changing the creature's type. A werewolf (a wolf of item type 39) takes
// the packet's colour as its body colour. Without a zone nothing happens.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool zoneTakes = true;

struct FakeCreature;
std::string Who(const FakeCreature* p);

// The creature's status array, recording each value set: SetStatus is
// virtual, as MCreature's own is in the game.
struct RecordingStatus : MStatus
{
	FakeCreature* owner = nullptr;
	void SetStatus(DWORD n, DWORD value) override;
};

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	TYPE_SECTORPOSITION x = 0, y = 0;
	int direction = 0;
	RecordingStatus status;
	FakeCreature() { status.owner = this; }
};
std::map<TYPE_OBJECTID, FakeCreature*> zoneCreatures;
std::vector<std::unique_ptr<FakeCreature>> made;

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature* creature) { return reinterpret_cast<MCreature*>(creature); }
FakeCreature& Fake(MCreature* p) { return *reinterpret_cast<FakeCreature*>(p); }
std::string Who(const FakeCreature* p) { return p->id == 0 ? std::string("new") : N(p->id); }
std::string Who(MCreature* p) { return Who(&Fake(p)); }

void RecordingStatus::SetStatus(DWORD n, DWORD value)
{
	calls.push_back("status|" + Who(owner) + "|" + N(n) + "|" + N(value));
	MStatus::SetStatus(n, value);
}

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) {
		auto it = zoneCreatures.find(id);
		return it == zoneCreatures.end() ? nullptr : Handle(it->second);
	};
	host.NewCreatureWear = []() -> MCreature* {
		made.push_back(std::make_unique<FakeCreature>());
		calls.push_back("new wearing creature");
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
	host.SetGuildNumber = [](MCreature* p, int guild) { calls.push_back("guild|" + Who(p) + "|" + N(guild)); };
	host.SetFlyingCreature = [](MCreature* p) { calls.push_back("flies|" + Who(p)); };
	host.SetGroundCreature = [](MCreature* p) { calls.push_back("walks|" + Who(p)); };
	host.SetID = [](MCreature* p, TYPE_OBJECTID id) { calls.push_back("id|" + N(id)); Fake(p).id = id; };
	host.SetPosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("at|" + Who(p) + "|" + N(x) + "," + N(y));
		Fake(p).x = x;
		Fake(p).y = y;
	};
	host.SetServerPosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("server at|" + Who(p) + "|" + N(x) + "," + N(y));
	};
	host.MovePosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("moves|" + Who(p) + "|" + N(x) + "," + N(y));
		Fake(p).x = x;
		Fake(p).y = y;
		return true;
	};
	host.SetDirection = [](MCreature* p, int direction) { calls.push_back("turns|" + Who(p) + "|" + N(direction)); Fake(p).direction = direction; };
	host.SetCurrentDirection = [](MCreature* p, int direction) { calls.push_back("faces|" + Who(p) + "|" + N(direction)); };
	host.SetAction = [](MCreature* p, int action) { calls.push_back("acts|" + Who(p) + "|" + N(action)); };
	host.SetBatColor = [](MCreature* p, std::uint16_t color) { calls.push_back("bat colour|" + Who(p) + "|" + N(color)); };
	host.SetBodyColor1 = [](MCreature* p, std::uint16_t color) { calls.push_back("body colour|" + Who(p) + "|" + N(color)); };
	host.Status = [](MCreature* p) -> MStatus* { return &Fake(p).status; };
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.GetDirection = [](MCreature* p) { return Fake(p).direction; };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int startZ, int direction,
		TYPE_OBJECTID targetID, TYPE_SECTORPOSITION targetX, TYPE_SECTORPOSITION targetY, int targetZ, std::uint32_t delayFrame,
		MActionResult* pResult, bool bStartFromMainNode, int sX, int sY) {
		CHECK(startZ == 0 && targetZ == 0 && delayFrame == 0 && !bStartFromMainNode && sX == 0 && sY == 0);
		CHECK(targetID == OBJECTID_NULL);
		std::string call = "effect|" + N(actionInfo) + "|" + N(startX) + "," + N(startY) + "|dir " + N(direction)
			+ "|@" + N(targetX) + "," + N(targetY);
		CHECK(pResult != nullptr && pResult->GetSize() == 1);
		if (pResult != nullptr && pResult->GetSize() == 1)
		{
			const MActionResultNode* pNode = *pResult->GetIterator();
			CHECK(pNode->GetType() == ACTIONRESULTNODE_CHANGE_CREATURE_TYPE);
			if (pNode->GetType() == ACTIONRESULTNODE_CHANGE_CREATURE_TYPE)
			{
				const auto* pChange = static_cast<const MActionResultNodeChangeCreatureType*>(pNode);
				call += "|becomes " + N(pChange->m_CreatureType) + " for " + N(pChange->m_TargetID);
			}
		}
		delete pResult;
		calls.push_back(call);
	};
	return host;
}();

struct World : GameModelWorld
{
	FakeCreature known;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		calls.clear();
		zoneExists = true;
		zoneTakes = true;
		made.clear();
		zoneCreatures.clear();
		known.id = 1003;
		known.x = 10;
		known.y = 11;
		known.direction = 3;
		zoneCreatures[known.id] = &known;
	}
	~World()
	{
		WorldEvents::SetHost(oldEvents);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		zoneCreatures.clear();
		made.clear();
	}
};

void ReceiveBat(ObjectID_t id, int color = 0)
{
	GCAddBat packet;
	packet.setObjectID(id);
	packet.setName("Flap");
	packet.setXYDir(30, 31, 5);
	packet.setMaxHP(90);
	packet.setCurrentHP(60);
	packet.setGuildID(7);
	packet.setBatColor(static_cast<WORD>(color));
	GCAddBatHandler::execute(&packet, nullptr);
}

void ReceiveWolf(ObjectID_t id, int itemType = 0, int color = 44)
{
	GCAddWolf packet;
	packet.setObjectID(id);
	packet.setName("Howl");
	packet.setXYDir(30, 31, 5);
	packet.setMaxHP(90);
	packet.setCurrentHP(60);
	packet.setItemType(static_cast<ItemType_t>(itemType));
	packet.setGuildID(7);
	packet.setColor(static_cast<Color_t>(color));
	GCAddWolfHandler::execute(&packet, nullptr);
}

std::string Health(const FakeCreature& creature)
{
	return N(creature.status.GetHP()) + "/" + N(creature.status.GetMAX_HP());
}

} // namespace

TEST(BatWolfReplies, ANewBatIsMadeFlyingAndAdded)
{
	World world;
	ReceiveBat(1005);
	CHECK(calls == std::vector<std::string>({"new wearing creature", "type|new|185", "name|new|Flap", "guild|new|7", "flies|new",
		"id|1005", "at|1005|30,31", "server at|1005|30,31", "turns|1005|5", "faces|1005|5", "acts|1005|" + N(ACTION_STAND),
		"status|1005|" + N(MODIFY_MAX_HP) + "|90", "status|1005|" + N(MODIFY_CURRENT_HP) + "|60", "bat colour|1005|65535", "zone takes|1005"}));
	CHECK(made.size() == 1 && Health(*made[0]) == "60/90");
	// A bat of a colour keeps it.
	calls.clear();
	ReceiveBat(1006, 12);
	CHECK(calls.size() == 15 && calls[13] == "bat colour|1006|12");
	// Refused, it is dropped.
	calls.clear();
	zoneTakes = false;
	ReceiveBat(1007);
	CHECK(calls.size() >= 2 && calls[calls.size() - 2] == "zone takes|1007" && calls.back() == "deleted|1007");
}

TEST(BatWolfReplies, AKnownCreatureTurnsIntoABatInPlace)
{
	World world;
	ReceiveBat(1003, 12);
	CHECK(calls == std::vector<std::string>({"flies|1003", "guild|1003|7", "moves|1003|30,31", "server at|1003|30,31", "turns|1003|5",
		"faces|1003|5", "status|1003|" + N(MODIFY_MAX_HP) + "|90", "status|1003|" + N(MODIFY_CURRENT_HP) + "|60", "bat colour|1003|12",
		"effect|" + N(RESULT_MAGIC_TRANSFORM_TO_BAT) + "|30,31|dir 5|@30,31|becomes 185 for 1003"}));
	CHECK(Health(world.known) == "60/90");
	CHECK(made.empty());
	calls.clear();
	ReceiveBat(1003);
	CHECK(calls[8] == "bat colour|1003|65535");
}

TEST(BatWolfReplies, ANewWolfIsMadeWalkingAndAWerewolfTakesItsColour)
{
	World world;
	ReceiveWolf(1005);
	CHECK(calls == std::vector<std::string>({"new wearing creature", "name|new|Howl", "type|new|186", "walks|new", "id|1005",
		"at|1005|30,31", "server at|1005|30,31", "turns|1005|5", "faces|1005|5", "acts|1005|" + N(ACTION_STAND), "status|1005|" + N(MODIFY_MAX_HP) + "|90", "status|1005|" + N(MODIFY_CURRENT_HP) + "|60",
		"guild|1005|7", "zone takes|1005"}));
	CHECK(made.size() == 1 && Health(*made[0]) == "60/90");
	calls.clear();
	ReceiveWolf(1006, 39, 44);
	CHECK(calls.size() == 15 && calls[2] == "type|new|" + N(CREATURETYPE_WER_WOLF) && calls.back() == "body colour|1006|44");
	// Refused, it is dropped and takes no colour.
	calls.clear();
	zoneTakes = false;
	ReceiveWolf(1007, 39, 44);
	CHECK(calls.size() >= 2 && calls[calls.size() - 2] == "zone takes|1007" && calls.back() == "deleted|1007");
}

TEST(BatWolfReplies, AKnownCreatureTurnsIntoAWolfInPlace)
{
	World world;
	ReceiveWolf(1003);
	CHECK(calls == std::vector<std::string>({"type|1003|186", "guild|1003|7", "walks|1003", "moves|1003|30,31", "server at|1003|30,31",
		"turns|1003|5", "faces|1003|5", "acts|1003|" + N(ACTION_STAND), "status|1003|" + N(MODIFY_MAX_HP) + "|90", "status|1003|" + N(MODIFY_CURRENT_HP) + "|60",
		"effect|" + N(RESULT_MAGIC_TRANSFORM_TO_WOLF) + "|30,31|dir 5|@30,31|becomes 186 for 1003"}));
	CHECK(Health(world.known) == "60/90");
	calls.clear();
	ReceiveWolf(1003, 39, 44);
	CHECK(calls.size() == 12 && calls[0] == "type|1003|" + N(CREATURETYPE_WER_WOLF) && calls.back() == "body colour|1003|44");
	CHECK(calls[10] == "effect|" + N(RESULT_MAGIC_TRANSFORM_TO_WOLF) + "|30,31|dir 5|@30,31|becomes " + N(CREATURETYPE_WER_WOLF) + " for 1003");
}

TEST(BatWolfReplies, WithoutAZoneOrTheHostsNothingHappens)
{
	World world;
	zoneExists = false;
	ReceiveBat(1005);
	ReceiveWolf(1005);
	CHECK(calls.empty());
	zoneExists = true;
	// A zone host that cannot make a wearing creature: nothing.
	ZoneState::Host cannotMake = zone;
	cannotMake.NewCreatureWear = nullptr;
	ZoneState::SetHost(&cannotMake);
	ReceiveBat(1005);
	ReceiveWolf(1005);
	CHECK(calls.empty());
	// Without the creature's host a made one is still added, its health
	// unset.
	ZoneState::SetHost(&zone);
	CreatureState::SetHost(nullptr);
	ReceiveBat(1005);
	CHECK(calls == std::vector<std::string>({"new wearing creature", "zone takes|new"}));
	calls.clear();
	ZoneState::SetHost(nullptr);
	CHECK(ZoneState::NewCreatureWear() == nullptr);
	CreatureState::SetBatColor(Handle(&world.known), 1);
	ReceiveBat(1005);
	ReceiveWolf(1005);
	CHECK(calls.empty());
}
