#include "test_framework.h"
#include "gamemodel_world.h"
#include "ConvertDuration.h"
#include "EffectInfo.h"
#include "MCreatureTable.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCAddMonster.h"
#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

// A monster appearing. One in a slayer's or an ouster's shape is the
// host's to make whole. Any other the zone does not know yet is made of
// the packet's type, walking, with its ID, cell, direction and health,
// standing, named, in guild 1, and added to the zone (or dropped if the
// zone refuses it); one the zone knows takes the type if it changed and
// the rest in place, its name kept. The monster then takes the packet's
// effect statuses, and its type may add more: Gilles de Rais's map
// effect, a trap's light, the Great Ruffian's body, a draining stance,
// a lasting effect status, a sacrifice's wounds. One that came through a
// portal comes up out of it over a bloody tunnel, unless its type shows
// itself otherwise. In a siege, a trap marked hidden is not shown to an
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
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
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
	host.AddEffectStatus = [](MCreature* p, EFFECTSTATUS status, std::uint32_t delayFrame) {
		calls.push_back("has|" + Who(p) + "|" + N(status) + "|" + N(delayFrame));
		return true;
	};
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
		CHECK(startZ == 0 && targetZ == 0 && pResult == nullptr && sX == 0 && sY == 0);
		calls.push_back("effect|" + N(actionInfo) + "|" + N(startX) + "," + N(startY) + "|dir " + N(direction) + "|"
			+ (targetID == OBJECTID_NULL ? std::string("nobody") : N(targetID)) + "@" + N(targetX) + "," + N(targetY) + "|" + N(delayFrame)
			+ (bStartFromMainNode ? "|main" : "|side"));
	};
	host.AddGDREffect = [](int effect, bool bAppearBossMonster) { calls.push_back("gilles|" + N(effect) + "|" + (bAppearBossMonster ? "boss" : "no boss")); };
	host.ComeFromPortal = [](MCreature* p) { calls.push_back("from portal|" + Who(p)); };
	host.AddRaceSlayerMonster = [](GCAddMonster* pPacket) { calls.push_back("slayer shape|" + N(pPacket->getObjectID())); };
	host.AddRaceOustersMonster = [](GCAddMonster* pPacket) { calls.push_back("ousters shape|" + N(pPacket->getObjectID())); };
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
		made.clear();
		zoneCreatures.clear();
		known.id = 1003;
		known.type = 50;
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

void Receive(ObjectID_t id, int type = 50, int fromFlag = 0, const Effects& effects = {{EFFECTSTATUS_CURSE_PARALYSIS, 20}})
{
	GCAddMonster packet;
	packet.setObjectID(id);
	packet.setMonsterType(static_cast<MonsterType_t>(type));
	packet.setMonsterName("Grub");
	packet.setX(30);
	packet.setY(31);
	packet.setDir(5);
	packet.setMaxHP(90);
	packet.setCurrentHP(60);
	packet.setFromFlag(static_cast<BYTE>(fromFlag));
	EffectInfo* pInfo = new EffectInfo;
	for (const auto& effect : effects)
		pInfo->addListElement(static_cast<EffectID_t>(effect.first), static_cast<WORD>(effect.second));
	packet.setEffectInfo(pInfo);
	GCAddMonsterHandler::execute(&packet, nullptr);
}

std::string Health(const std::string& who)
{
	return "status|" + who + "|" + N(MODIFY_MAX_HP) + "|90";
}
std::string Current(const std::string& who)
{
	return "status|" + who + "|" + N(MODIFY_CURRENT_HP) + "|60";
}
// One effect status (one pair of status and duration).
std::string Effects1(const std::string& who)
{
	return "effects|" + who + "|1|0";
}
std::string Light(int actionInfo, const std::string& id)
{
	return "effect|" + N(actionInfo) + "|30,31|dir 5|" + id + "@30,31|65535|side";
}
// The bloody tunnel under one come through a portal, 18 duration units on.
std::string Portal()
{
	return "effect|" + N(RESULT_MAGIC_BLOODY_TUNNEL) + "|30,31|dir " + N(DIRECTION_DOWN) + "|nobody@30,31|" + N(ConvertDurationToFrame(18)) + "|main";
}

std::vector<std::string> Made(const std::string& id, int type)
{
	return {"new creature", "type|new|" + N(type), "walks|new", "id|" + id, "at|" + id + "|30,31", "server at|" + id + "|30,31",
		"turns|" + id + "|5", "faces|" + id + "|5", "acts|" + id + "|" + N(ACTION_STAND), Health(id), Current(id),
		"name|" + id + "|Grub", "guild|" + id + "|1", "zone takes|" + id, Effects1(id)};
}

// What a new monster of the type receives after it is made and takes the
// packet's effect statuses.
std::vector<std::string> After(int type, int fromFlag = 0)
{
	Receive(1005, type, fromFlag);
	const std::vector<std::string> expected = Made("1005", type);
	if (calls.size() < expected.size() || !std::equal(expected.begin(), expected.end(), calls.begin()))
		return {"made otherwise"};
	std::vector<std::string> rest(calls.begin() + static_cast<long>(expected.size()), calls.end());
	calls.clear();
	zoneCreatures.erase(1005);
	return rest;
}

} // namespace

TEST(AddMonsterReplies, ANewMonsterIsMadeAndAdded)
{
	World world;
	Receive(1005);
	CHECK(calls == Made("1005", 50));
	CHECK(made.size() == 1 && zoneCreatures.count(1005) == 1);
	// Refused, it is dropped, with no effect.
	calls.clear();
	zoneTakes = false;
	Receive(1006, 50, 1);
	CHECK(calls.size() >= 2 && calls[calls.size() - 2] == "zone takes|1006" && calls.back() == "deleted|1006");
}

TEST(AddMonsterReplies, AKnownMonsterTakesItInPlace)
{
	World world;
	Receive(1003);
	CHECK(calls == std::vector<std::string>({"walks|1003", "moves|1003|30,31", "server at|1003|30,31", "turns|1003|5", "faces|1003|5",
		"acts|1003|" + N(ACTION_STAND), Health("1003"), Current("1003"), "guild|1003|1", Effects1("1003")}));
	// Of another type now, it takes the type first.
	calls.clear();
	Receive(1003, 51);
	CHECK(calls.size() == 11 && calls[0] == "type|1003|51" && calls[1] == "walks|1003");
	CHECK(made.empty());
}

TEST(AddMonsterReplies, AMonsterInAnotherRacesShapeIsTheHostsToMake)
{
	World world;
	for (int type : {735, 793, 796, 797})
		Receive(1005, type, 1);
	for (int type : {737, 795, 800})
		Receive(1003, type, 1);
	CHECK(calls == std::vector<std::string>({"slayer shape|1005", "slayer shape|1005", "slayer shape|1005", "slayer shape|1005",
		"ousters shape|1003", "ousters shape|1003", "ousters shape|1003"}));
}

TEST(AddMonsterReplies, ItsTypeAddsItsOwnEffects)
{
	World world;
	CHECK(After(724) == std::vector<std::string>({"gilles|1|boss"}));
	CHECK(After(725) == std::vector<std::string>({"gilles|2|boss"}));
	CHECK(After(739) == std::vector<std::string>({Light(SKILL_CLIENT_LAND_MINE_LIGHT, "1005")}));
	CHECK(After(738) == std::vector<std::string>({Light(SKILL_CLIENT_PLEASUER_STATION_LIGHT, "1005")}));
	CHECK(After(765) == std::vector<std::string>({Light(SKILL_CLIENT_GREAT_RUFFIAN_BODY, "1005")}));
	CHECK(After(753) == std::vector<std::string>({"acts|1005|" + N(ACTION_VAMPIRE_DRAIN)}));
	CHECK(After(768) == std::vector<std::string>({"has|1005|" + N(EFFECTSTATUS_SUMMON_MIGA_FIRE) + "|65535"}));
	CHECK(After(786) == std::vector<std::string>({"has|1005|" + N(EFFECTSTATUS_GRAND_MASTER_VAMPIRE) + "|65535"}));
	CHECK(After(788) == std::vector<std::string>({"has|1005|" + N(EFFECTSTATUS_DRAGON_TORNADO) + "|65535"}));
	CHECK(After(789) == std::vector<std::string>({"has|1005|" + N(EFFECTSTATUS_DRAGON_TORNADO_CHILD) + "|65535"}));
	// A sacrifice is wounded, its effect facing down (2).
	CHECK(After(794) == std::vector<std::string>({"has|1005|" + N(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS) + "|65535",
		"effect|" + N(RESULT_MAGIC_CAUSE_CRITICAL_WOUNDS) + "|30,31|dir 2|1005@30,31|65535|side"}));
	CHECK(After(740).empty() && After(50).empty());
}

TEST(AddMonsterReplies, OneThroughAPortalComesUpOutOfIt)
{
	World world;
	CHECK(After(50, 1) == std::vector<std::string>({"from portal|1005", Portal()}));
	CHECK(After(724, 1) == std::vector<std::string>({"gilles|1|boss", "from portal|1005", Portal()}));
	CHECK(After(786, 1) == std::vector<std::string>({"has|1005|" + N(EFFECTSTATUS_GRAND_MASTER_VAMPIRE) + "|65535", "from portal|1005", Portal()}));
	// These show themselves otherwise.
	CHECK(After(753, 1).size() == 1 && After(768, 1).size() == 1 && After(788, 1).size() == 1 && After(789, 1).size() == 1
		&& After(794, 1).size() == 2);
	// A flag other than 1 is no portal.
	CHECK(After(50, 2).empty());
	// A known monster comes up out of it too, from where it moved.
	Receive(1003, 50, 1);
	CHECK(calls.size() == 12 && calls[10] == "from portal|1003" && calls[11] == Portal());
}

TEST(AddMonsterReplies, ATrapHiddenFromAttackersIsNotShownToOne)
{
	World world;
	const Effects hidden{{EFFECTSTATUS_CURSE_PARALYSIS, 20}, {EFFECTSTATUS_HIDE_TO_ATTACKER, 30}};
	for (EFFECTSTATUS attacker : {EFFECTSTATUS_SIEGE_ATTACKER_1, EFFECTSTATUS_SIEGE_ATTACKER_2, EFFECTSTATUS_SIEGE_ATTACKER_3,
		EFFECTSTATUS_SIEGE_ATTACKER_4, EFFECTSTATUS_SIEGE_ATTACKER_5})
	{
		playerEffects = {attacker};
		for (int trap : {738, 739, 740, 741})
			Receive(1005, trap, 0, hidden);
		// The mark alone hides it too.
		Receive(1005, 741, 0, {{EFFECTSTATUS_HIDE_TO_ATTACKER, 30}});
	}
	CHECK(calls.empty());
	// A defender sees it; so does an attacker when it is not a trap, or not
	// marked hidden (the mark's number as a duration does not count), or
	// when there is no player.
	playerEffects = {EFFECTSTATUS_SIEGE_DEFENDERL};
	Receive(1005, 741, 0, hidden);
	CHECK(made.size() == 1);
	playerEffects = {EFFECTSTATUS_SIEGE_ATTACKER_1};
	Receive(1006, 742, 0, hidden);
	Receive(1009, 736, 0, hidden);
	Receive(1007, 741, 0, {{EFFECTSTATUS_CURSE_PARALYSIS, EFFECTSTATUS_HIDE_TO_ATTACKER}});
	hasPlayer = false;
	Receive(1008, 741, 0, hidden);
	CHECK(made.size() == 5);
}

TEST(AddMonsterReplies, WithoutAZoneOrTheHostsNothingHappens)
{
	World world;
	zoneExists = false;
	Receive(1005);
	Receive(1005, 735);
	CHECK(calls.empty());
	zoneExists = true;
	// A zone host that cannot make a creature: nothing.
	ZoneState::Host cannotMake = zone;
	cannotMake.NewCreature = nullptr;
	ZoneState::SetHost(&cannotMake);
	Receive(1005, 50, 1);
	CHECK(calls.empty());
	// A creature host without the status array: no health is set, new or
	// known.
	ZoneState::SetHost(&zone);
	CreatureState::Host noStatus = creature;
	noStatus.Status = nullptr;
	CreatureState::SetHost(&noStatus);
	Receive(1003);
	Receive(1006);
	CHECK(std::none_of(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("status|", 0) == 0; }));
	CHECK(std::count(calls.begin(), calls.end(), "guild|1003|1") == 1 && std::count(calls.begin(), calls.end(), "zone takes|1006") == 1);
	calls.clear();
	// Without the creature's host a made creature is still added, its
	// health unset, and comes out of the portal at the cell it reports.
	CreatureState::SetHost(nullptr);
	Receive(1005, 50, 1);
	CHECK(calls == std::vector<std::string>({"new creature", "zone takes|new", "effects|new|1|0", "from portal|new",
		"effect|" + N(RESULT_MAGIC_BLOODY_TUNNEL) + "|0,0|dir " + N(DIRECTION_DOWN) + "|nobody@0,0|" + N(ConvertDurationToFrame(18)) + "|main"}));
	calls.clear();
	ZoneState::SetHost(nullptr);
	WorldEvents::SetHost(nullptr);
	WorldEvents::AddGDREffect(1, true);
	WorldEvents::ComeFromPortal(Handle(&world.known));
	WorldEvents::AddRaceSlayerMonster(nullptr);
	WorldEvents::AddRaceOustersMonster(nullptr);
	Receive(1005);
	CHECK(calls.empty());
}

TEST(AddMonsterReplies, AnEffectPastTheEffectStatusesIsNoHiddenMark)
{
	World world;
	playerEffects = {EFFECTSTATUS_SIEGE_ATTACKER_1};
	// The list's statuses are 16 bits on the wire; the effect statuses'
	// enum holds 0 to 511.
	Receive(1005, 741, 0, {{600, 20}, {65535, 20}});
	CHECK_EQ(1u, made.size());
}
