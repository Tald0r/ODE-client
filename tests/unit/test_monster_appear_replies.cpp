#include "test_framework.h"
#include "gamemodel_world.h"
#include "EffectInfo.h"
#include "MCreatureTable.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCAddMonsterFromBurrowing.h"
#include "Gpackets/GCAddMonsterFromTransformation.h"
#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

// A monster out of a burrow, or out of a transformation. One the zone
// does not know yet is made of the packet's type, walking, with its ID,
// cell, direction, health and name, standing, in guild 1, and added to the
// zone (or dropped if the zone refuses it). Out of a burrow, one the zone
// knows takes all that in place; out of a transformation, a plain creature
// the zone knows moves to the cell and takes the type, direction, health
// and guild, while any other is removed and made again as a plain one. The
// monster then takes the packet's effect statuses and comes up out of the
// ground; a land mine or a pleasure station trap out of a burrow also
// shows its light. In a siege, a trap marked hidden is not shown to an
// attacker at all. Without a zone nothing happens.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool zoneTakes = true;
bool hasPlayer = true;
std::set<int> playerEffects;

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
	bool plain = true;
	int type = 0;
	TYPE_SECTORPOSITION x = 0, y = 0;
	int direction = 0;
	RecordingStatus status;
	FakeCreature() { status.owner = this; }
	FakeCreature(const FakeCreature&) = delete;
	FakeCreature& operator=(const FakeCreature&) = delete;
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

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? reinterpret_cast<MCreature*>(&calls) : nullptr; };
	// Asked only of a player there (the game's dereferences it).
	host.HasEffectStatus = [](EFFECTSTATUS status) {
		CHECK(hasPlayer);
		return playerEffects.count(status) != 0;
	};
	return host;
}();

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
	host.RemoveCreature = [](TYPE_OBJECTID id) { calls.push_back("removed|" + N(id)); zoneCreatures.erase(id); return true; };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.IsPlainCreature = [](MCreature* p) { return Fake(p).plain; };
	host.SetCreatureType = [](MCreature* p, int type) { calls.push_back("type|" + Who(p) + "|" + N(type)); Fake(p).type = type; };
	host.GetCreatureType = [](MCreature* p) { return Fake(p).type; };
	host.SetGroundCreature = [](MCreature* p) { calls.push_back("walks|" + Who(p)); };
	host.SetID = [](MCreature* p, TYPE_OBJECTID id) { calls.push_back("id|" + N(id)); Fake(p).id = id; };
	host.SetName = [](MCreature* p, const char* name) { calls.push_back("name|" + Who(p) + "|" + name); };
	host.SetPosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("at|" + Who(p) + "|" + N(x) + "," + N(y));
		Fake(p).x = x;
		Fake(p).y = y;
	};
	host.MovePosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("moves|" + Who(p) + "|" + N(x) + "," + N(y));
		Fake(p).x = x;
		Fake(p).y = y;
		return true;
	};
	host.SetServerPosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("server at|" + Who(p) + "|" + N(x) + "," + N(y));
	};
	host.SetDirection = [](MCreature* p, int direction) { calls.push_back("turns|" + Who(p) + "|" + N(direction)); Fake(p).direction = direction; };
	host.SetCurrentDirection = [](MCreature* p, int direction) { calls.push_back("faces|" + Who(p) + "|" + N(direction)); };
	host.SetAction = [](MCreature* p, int action) { calls.push_back("acts|" + Who(p) + "|" + N(action)); };
	host.SetGuildNumber = [](MCreature* p, int guild) { calls.push_back("guild|" + Who(p) + "|" + N(guild)); };
	host.Status = [](MCreature* p) -> MStatus* { return &Fake(p).status; };
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.GetDirection = [](MCreature* p) { return Fake(p).direction; };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.SetEffectInfo = [](MCreature* p, EffectInfo* pInfo, int delayedFrame) {
		calls.push_back("effects|" + Who(p) + "|" + N(pInfo == nullptr ? -1 : pInfo->getListNum()) + "|" + N(delayedFrame));
	};
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int startZ, int direction,
		TYPE_OBJECTID targetID, TYPE_SECTORPOSITION targetX, TYPE_SECTORPOSITION targetY, int targetZ, std::uint32_t delayFrame,
		MActionResult* pResult, bool bStartFromMainNode, int sX, int sY) {
		CHECK(startZ == 0 && targetZ == 0 && pResult == nullptr && !bStartFromMainNode && sX == 0 && sY == 0);
		calls.push_back("effect|" + N(actionInfo) + "|" + N(startX) + "," + N(startY) + "|dir " + N(direction) + "|"
			+ (targetID == OBJECTID_NULL ? std::string("nobody") : N(targetID)) + "@" + N(targetX) + "," + N(targetY) + "|" + N(delayFrame));
	};
	return host;
}();

struct World : GameModelWorld
{
	FakeCreature known;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		calls.clear();
		zoneExists = true;
		zoneTakes = true;
		hasPlayer = true;
		playerEffects.clear();
		known.type = 50;
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
		PlayerState::SetHost(oldPlayer);
		zoneCreatures.clear();
		made.clear();
	}
};

using Effects = std::vector<std::pair<int, int>>;

template<class PacketType>
void Fill(PacketType& packet, ObjectID_t id, int type, const Effects& effects)
{
	packet.setObjectID(id);
	packet.setMonsterType(static_cast<MonsterType_t>(type));
	packet.setMonsterName("Grub");
	packet.setX(30);
	packet.setY(31);
	packet.setDir(5);
	packet.setMaxHP(90);
	packet.setCurrentHP(60);
	EffectInfo* pInfo = new EffectInfo;
	for (const auto& effect : effects)
		pInfo->addListElement(static_cast<EffectID_t>(effect.first), static_cast<WORD>(effect.second));
	packet.setEffectInfo(pInfo);
}

void ReceiveBurrowing(ObjectID_t id, int type = 50, const Effects& effects = {{EFFECTSTATUS_CURSE_PARALYSIS, 20}})
{
	GCAddMonsterFromBurrowing packet;
	Fill(packet, id, type, effects);
	GCAddMonsterFromBurrowingHandler::execute(&packet, nullptr);
}

void ReceiveTransformation(ObjectID_t id, int type = 50)
{
	GCAddMonsterFromTransformation packet;
	Fill(packet, id, type, {{EFFECTSTATUS_CURSE_PARALYSIS, 20}});
	GCAddMonsterFromTransformationHandler::execute(&packet, nullptr);
}

std::string Health(const std::string& who)
{
	return "status|" + who + "|" + N(MODIFY_MAX_HP) + "|90";
}
std::string Current(const std::string& who)
{
	return "status|" + who + "|" + N(MODIFY_CURRENT_HP) + "|60";
}
std::string Up(const std::string& who)
{
	// One effect status (one pair of status and duration).
	return "effects|" + who + "|1|0";
}
const std::string kOut = "effect|" + std::to_string(RESULT_MAGIC_HIDE) + "|30,31|dir 5|nobody@30,31|0";

std::vector<std::string> Made(const std::string& id, int type)
{
	return {"new creature", "type|new|" + N(type), "walks|new", "id|" + id, "at|" + id + "|30,31", "server at|" + id + "|30,31",
		"turns|" + id + "|5", "faces|" + id + "|5", "acts|" + id + "|" + N(ACTION_STAND), Health(id), Current(id),
		"name|" + id + "|Grub", "guild|" + id + "|1", "zone takes|" + id};
}

std::vector<std::string> Then(std::vector<std::string> first, const std::vector<std::string>& rest)
{
	first.insert(first.end(), rest.begin(), rest.end());
	return first;
}

} // namespace

TEST(MonsterAppearReplies, ANewMonsterComesOutOfItsBurrow)
{
	World world;
	ReceiveBurrowing(1005);
	CHECK(calls == Then(Made("1005", 50), {Up("1005"), kOut}));
	CHECK(made.size() == 1);
	// Refused, it is dropped, with no effect.
	calls.clear();
	zoneTakes = false;
	ReceiveBurrowing(1006);
	CHECK(calls.size() >= 2 && calls[calls.size() - 2] == "zone takes|1006" && calls.back() == "deleted|1006");
}

TEST(MonsterAppearReplies, AKnownMonsterComesOutWhereItIs)
{
	World world;
	ReceiveBurrowing(1003);
	CHECK(calls == std::vector<std::string>({"type|1003|50", "walks|1003", "id|1003", "moves|1003|30,31", "server at|1003|30,31",
		"turns|1003|5", "faces|1003|5", "acts|1003|" + N(ACTION_STAND), Health("1003"), Current("1003"), "name|1003|Grub",
		"guild|1003|1", Up("1003"), kOut}));
	CHECK(made.empty());
}

TEST(MonsterAppearReplies, ALandMineOrPleasureStationShowsItsLight)
{
	World world;
	ReceiveBurrowing(1005, 739);
	CHECK(calls.size() == 17 && calls[15] == "effect|" + N(SKILL_CLIENT_LAND_MINE_LIGHT) + "|30,31|dir 5|1005@30,31|65535"
		&& calls.back() == kOut);
	calls.clear();
	ReceiveBurrowing(1006, 738);
	CHECK(calls.size() == 17 && calls[15] == "effect|" + N(SKILL_CLIENT_PLEASUER_STATION_LIGHT) + "|30,31|dir 5|1006@30,31|65535");
	// The other traps show none.
	calls.clear();
	ReceiveBurrowing(1007, 740);
	CHECK(calls.size() == 16 && calls.back() == kOut);
}

TEST(MonsterAppearReplies, ATrapHiddenFromAttackersIsNotShownToOne)
{
	World world;
	const Effects hidden{{EFFECTSTATUS_CURSE_PARALYSIS, 20}, {EFFECTSTATUS_HIDE_TO_ATTACKER, 30}};
	for (EFFECTSTATUS attacker : {EFFECTSTATUS_SIEGE_ATTACKER_1, EFFECTSTATUS_SIEGE_ATTACKER_2, EFFECTSTATUS_SIEGE_ATTACKER_3,
		EFFECTSTATUS_SIEGE_ATTACKER_4, EFFECTSTATUS_SIEGE_ATTACKER_5})
	{
		playerEffects = {attacker};
		for (int trap : {738, 739, 740, 741})
			ReceiveBurrowing(1005, trap, hidden);
		// The mark alone hides it too.
		ReceiveBurrowing(1005, 741, {{EFFECTSTATUS_HIDE_TO_ATTACKER, 30}});
	}
	CHECK(calls.empty());
	// A defender sees it; so does an attacker when it is not a trap, or not
	// marked hidden (the mark's number as a duration does not count), or
	// when there is no player.
	playerEffects = {EFFECTSTATUS_SIEGE_DEFENDERL};
	ReceiveBurrowing(1005, 741, hidden);
	CHECK(made.size() == 1);
	playerEffects = {EFFECTSTATUS_SIEGE_ATTACKER_1};
	ReceiveBurrowing(1006, 742, hidden);
	ReceiveBurrowing(1009, 737, hidden);
	ReceiveBurrowing(1007, 741, {{EFFECTSTATUS_CURSE_PARALYSIS, EFFECTSTATUS_HIDE_TO_ATTACKER}});
	hasPlayer = false;
	ReceiveBurrowing(1008, 741, hidden);
	CHECK(made.size() == 5);
}

TEST(MonsterAppearReplies, ATransformedMonsterIsMadeOrTakesTheFormInPlace)
{
	World world;
	ReceiveTransformation(1005);
	CHECK(calls == Then(Made("1005", 50), {Up("1005"), kOut}));
	// A plain creature the zone knows moves and changes in place; its name
	// and ID stay.
	calls.clear();
	ReceiveTransformation(1003, 51);
	CHECK(calls == std::vector<std::string>({"moves|1003|30,31", "type|1003|51", "walks|1003", "turns|1003|5", "faces|1003|5",
		"acts|1003|" + N(ACTION_STAND), "server at|1003|30,31", Health("1003"), Current("1003"), "guild|1003|1", Up("1003"), kOut}));
	CHECK(made.size() == 1);
	// A new one refused is dropped, with no effect.
	calls.clear();
	zoneTakes = false;
	ReceiveTransformation(1006);
	CHECK(calls.size() >= 2 && calls[calls.size() - 2] == "zone takes|1006" && calls.back() == "deleted|1006");
}

TEST(MonsterAppearReplies, AnotherKnownCreatureIsMadeAgainAsAPlainOne)
{
	World world;
	world.known.plain = false;
	ReceiveTransformation(1003, 51);
	CHECK(calls == std::vector<std::string>({"removed|1003", "new creature", "id|1003", "name|1003|Grub", "at|1003|30,31",
		"type|1003|51", "walks|1003", "turns|1003|5", "faces|1003|5", "acts|1003|" + N(ACTION_STAND), "server at|1003|30,31",
		Health("1003"), Current("1003"), "guild|1003|1", "zone takes|1003", Up("1003"), kOut}));
	// Refused, the new one is dropped, with no effect.
	calls.clear();
	made.back()->plain = false;
	zoneTakes = false;
	ReceiveTransformation(1003, 52);
	CHECK(calls.size() >= 2 && calls[calls.size() - 2] == "zone takes|1003" && calls.back() == "deleted|1003");
}

TEST(MonsterAppearReplies, WithoutAZoneOrTheHostsNothingHappens)
{
	World world;
	zoneExists = false;
	ReceiveBurrowing(1005);
	ReceiveTransformation(1005);
	CHECK(calls.empty());
	zoneExists = true;
	// A zone host that cannot make a creature: nothing.
	ZoneState::Host cannotMake = zone;
	cannotMake.NewCreature = nullptr;
	ZoneState::SetHost(&cannotMake);
	ReceiveBurrowing(1005);
	ReceiveTransformation(1005);
	CHECK(calls.empty());
	// Nor can it make another creature in place of one it removed.
	world.known.plain = false;
	ReceiveTransformation(1003);
	CHECK(calls == std::vector<std::string>({"removed|1003"}));
	calls.clear();
	world.known.plain = true;
	zoneCreatures[world.known.id] = &world.known;
	// A creature host without the status array: no health is set, new or
	// known, made again or in place.
	ZoneState::SetHost(&zone);
	CreatureState::Host noStatus = creature;
	noStatus.Status = nullptr;
	CreatureState::SetHost(&noStatus);
	ReceiveBurrowing(1003);
	ReceiveBurrowing(1006);
	ReceiveTransformation(1003);
	ReceiveTransformation(1007);
	world.known.plain = false;
	ReceiveTransformation(1003);
	CHECK(std::none_of(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("status|", 0) == 0; }));
	CHECK(std::count(calls.begin(), calls.end(), kOut) == 5);
	calls.clear();
	// Without the creature's host a made creature is still added, its
	// health unset, and comes out.
	ZoneState::SetHost(&zone);
	CreatureState::SetHost(nullptr);
	ReceiveBurrowing(1005);
	CHECK(calls == std::vector<std::string>({"new creature", "zone takes|new", "effects|new|1|0",
		"effect|" + N(RESULT_MAGIC_HIDE) + "|0,0|dir 0|nobody@0,0|0"}));
	calls.clear();
	ZoneState::SetHost(nullptr);
	WorldEvents::SetHost(nullptr);
	CHECK(!CreatureState::IsPlainCreature(Handle(&world.known)));
	WorldEvents::SetEffectInfo(Handle(&world.known), nullptr, 0);
	ReceiveBurrowing(1005);
	ReceiveTransformation(1005);
	CHECK(calls.empty());
}

TEST(MonsterAppearReplies, AnEffectPastTheEffectStatusesIsNoHiddenMark)
{
	World world;
	playerEffects = {EFFECTSTATUS_SIEGE_ATTACKER_1};
	// The list's statuses are 16 bits on the wire; the effect statuses'
	// enum holds 0 to 511.
	ReceiveBurrowing(1005, 741, {{600, 20}, {65535, 20}});
	CHECK_EQ(1u, made.size());
}
