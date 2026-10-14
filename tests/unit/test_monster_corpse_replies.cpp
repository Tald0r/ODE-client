#include "test_framework.h"
#include "gamemodel_world.h"
#include "GameInfoPresentationHost.h"
#include "MCreatureTable.h"
#include "MHelpDef.h"
#include "MItem.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCAddMonsterCorpse.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

// A monster's corpse appearing. A monster shaped as a slayer, a vampire or
// an ouster (735 to 737, 792 to 800) is ignored. When no creature and no
// item of the ID is in the zone, a dead creature is made of the packet's
// type, walking, with its ID, cell, direction, health (0 of 100), name,
// its killer, in guild 1 and headless when the packet says so, laid down
// as a corpse; a corpse item of the same ID at the cell holds it (owning
// it) and the packet's treasure count, and is laid in the zone. A
// Christmas tree (482) also takes its effect and shows its name over it,
// as a 650 does. An item already there takes the treasure count. A
// creature the zone knows dies where it is: it takes the type if it
// changed, moves to the cell, takes the direction, loses its health,
// stops and dies unless dead already, and takes the treasure count, guild
// 1, its killer and its head. The first time the player is the killer
// the kill's help shows (once a run). Without a zone nothing happens.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
int zoneTakesItems = 1;
TYPE_OBJECTID itemInTheCell = OBJECTID_NULL;
bool removesItem = true;
bool hasPlayer = true;

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
	std::string name;
	bool dead = false;
	// Set once the corpse that owns it is freed: any later use fails.
	bool freed = false;
	RecordingStatus status;
	FakeCreature() { status.owner = this; }
	FakeCreature(const FakeCreature&) = delete;
	FakeCreature& operator=(const FakeCreature&) = delete;
};
std::map<TYPE_OBJECTID, FakeCreature*> zoneCreatures;
std::vector<std::unique_ptr<FakeCreature>> made;

// A corpse item: it owns its creature, so freeing it frees the creature.
struct FakeCorpse : MItem
{
	FakeCreature* creature;
	explicit FakeCorpse(FakeCreature* pCreature) : creature(pCreature) {}
	~FakeCorpse() override
	{
		calls.push_back("corpse freed|" + Who(creature));
		creature->freed = true;
	}
	ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_CORPSE; }
};
struct Treasure : MItem
{
	ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_MONEY; }
};
std::map<TYPE_OBJECTID, MItem*> zoneItems;
std::vector<std::unique_ptr<MItem>> laid;

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature* creature) { return reinterpret_cast<MCreature*>(creature); }
// A creature the handler still uses: never one a freed corpse took.
FakeCreature& Fake(MCreature* p)
{
	FakeCreature& creature = *reinterpret_cast<FakeCreature*>(p);
	CHECK(!creature.freed);
	return creature;
}
std::string Who(const FakeCreature* p) { return p->id == 0 ? std::string("new") : N(p->id); }
std::string Who(MCreature* p) { return Who(&Fake(p)); }

void RecordingStatus::SetStatus(DWORD n, DWORD value)
{
	calls.push_back("status|" + Who(owner) + "|" + N(n) + "|" + N(value));
	MStatus::SetStatus(n, value);
}

FakeCreature me;

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? Handle(&me) : nullptr; };
	host.GetID = []() { return me.id; };
	return host;
}();

const GameInfoPresentation::Host info = []() {
	GameInfoPresentation::Host host{};
	host.ExecuteHelpEvent = [](int helpEvent) { calls.push_back("help|" + N(helpEvent)); };
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreatureOnly = [](TYPE_OBJECTID id) {
		auto it = zoneCreatures.find(id);
		return it == zoneCreatures.end() ? nullptr : Handle(it->second);
	};
	host.GetItem = [](TYPE_OBJECTID id) -> MItem* {
		auto it = zoneItems.find(id);
		return it == zoneItems.end() ? nullptr : it->second;
	};
	host.NewCreature = []() -> MCreature* {
		made.push_back(std::make_unique<FakeCreature>());
		calls.push_back("new creature");
		return Handle(made.back().get());
	};
	host.DeleteCreature = [](MCreature* p) { calls.push_back("deleted|" + Who(p)); };
	host.NewCorpse = [](MCreature* p) -> MItem* {
		calls.push_back("corpse of|" + Who(p));
		return new FakeCorpse(&Fake(p));
	};
	host.AddItem = [](MItem* pItem, bool bDropping) {
		calls.push_back("zone lays|" + N(pItem->GetID()) + (bDropping ? "|falling" : "|still"));
		if (zoneTakesItems <= 0) return false;
		--zoneTakesItems;
		zoneItems[pItem->GetID()] = pItem;
		laid.emplace_back(pItem);
		return true;
	};
	host.GetItemID = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("item at|" + N(x) + "," + N(y));
		return itemInTheCell;
	};
	host.RemoveItem = [](TYPE_OBJECTID id) {
		calls.push_back("removes item|" + N(id));
		if (removesItem) ++zoneTakesItems;
		return removesItem;
	};
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.SetCreatureType = [](MCreature* p, int type) { calls.push_back("type|" + Who(p) + "|" + N(type)); Fake(p).type = type; };
	host.GetCreatureType = [](MCreature* p) { return Fake(p).type; };
	host.SetGroundCreature = [](MCreature* p) { calls.push_back("walks|" + Who(p)); };
	host.SetID = [](MCreature* p, TYPE_OBJECTID id) { calls.push_back("id|" + N(id)); Fake(p).id = id; };
	host.SetPosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { calls.push_back("at|" + Who(p) + "|" + N(x) + "," + N(y)); };
	host.MovePosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("moves|" + Who(p) + "|" + N(x) + "," + N(y));
		return true;
	};
	host.SetServerPosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("server at|" + Who(p) + "|" + N(x) + "," + N(y));
	};
	host.SetDirection = [](MCreature* p, int direction) { calls.push_back("turns|" + Who(p) + "|" + N(direction)); };
	host.SetCurrentDirection = [](MCreature* p, int direction) { calls.push_back("faces|" + Who(p) + "|" + N(direction)); };
	host.SetCorpse = [](MCreature* p) { calls.push_back("corpse|" + Who(p)); Fake(p).dead = true; };
	host.SetName = [](MCreature* p, const char* name) { calls.push_back("name|" + Who(p) + "|" + name); Fake(p).name = name; };
	host.GetName = [](MCreature* p) { return Fake(p).name.c_str(); };
	host.SetDrainCreatureID = [](MCreature* p, TYPE_OBJECTID id) { calls.push_back("killed by|" + Who(p) + "|" + N(id)); };
	host.SetGuildNumber = [](MCreature* p, int guild) { calls.push_back("guild|" + Who(p) + "|" + N(guild)); };
	host.RemoveHead = [](MCreature* p) { calls.push_back("headless|" + Who(p)); };
	host.AddEffectStatus = [](MCreature* p, EFFECTSTATUS status, std::uint32_t delayFrame) {
		calls.push_back("has|" + Who(p) + "|" + N(status) + "|" + N(delayFrame));
		return true;
	};
	host.SetChatString = [](MCreature* p, const char* str, std::uint32_t color) {
		calls.push_back("says|" + Who(p) + "|" + (str ? str : "(null)") + "|" + N(color));
	};
	host.IsDead = [](MCreature* p) { return Fake(p).dead; };
	host.SetStop = [](MCreature* p) { calls.push_back("stops|" + Who(p)); };
	host.SetDead = [](MCreature* p) { calls.push_back("dies|" + Who(p)); Fake(p).dead = true; };
	host.SetItemCount = [](MCreature* p, int count) { calls.push_back("holds|" + Who(p) + "|" + N(count)); };
	host.Status = [](MCreature* p) -> MStatus* { return &Fake(p).status; };
	return host;
}();

struct World : GameModelWorld
{
	FakeCreature known;
	Treasure treasure;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const GameInfoPresentation::Host* oldInfo = GameInfoPresentation::SetHost(&info);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	World()
	{
		calls.clear();
		zoneExists = true;
		zoneTakesItems = 1;
		itemInTheCell = OBJECTID_NULL;
		removesItem = true;
		hasPlayer = true;
		me.id = 7;
		made.clear();
		zoneCreatures.clear();
		zoneItems.clear();
		laid.clear();
		known.id = 1003;
		known.type = 50;
		zoneCreatures[known.id] = &known;
		treasure.SetID(2001);
		zoneItems[treasure.GetID()] = &treasure;
	}
	~World()
	{
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		GameInfoPresentation::SetHost(oldInfo);
		PlayerState::SetHost(oldPlayer);
		zoneCreatures.clear();
		zoneItems.clear();
		laid.clear();
		made.clear();
	}
};

struct Corpse
{
	int type = 50;
	ObjectID_t killer = 1001;
	bool head = true;
};

void Receive(ObjectID_t id, const Corpse& corpse = {})
{
	GCAddMonsterCorpse packet;
	packet.setObjectID(id);
	packet.setMonsterType(static_cast<MonsterType_t>(corpse.type));
	packet.setMonsterName("Grub");
	packet.setX(30);
	packet.setY(31);
	packet.setDir(5);
	packet.sethasHead(corpse.head);
	packet.setTreasureCount(3);
	packet.setLastKiller(corpse.killer);
	GCAddMonsterCorpseHandler::execute(&packet, nullptr);
}

std::vector<std::string> Made(const std::string& id, int type = 50)
{
	return {"new creature", "type|new|" + N(type), "walks|new", "id|" + id, "at|" + id + "|30,31", "server at|" + id + "|30,31",
		"turns|" + id + "|5", "faces|" + id + "|5", "status|" + id + "|" + N(MODIFY_MAX_HP) + "|100",
		"status|" + id + "|" + N(MODIFY_CURRENT_HP) + "|0", "corpse|" + id, "name|" + id + "|Grub", "killed by|" + id + "|1001",
		"guild|" + id + "|1", "corpse of|" + id};
}

std::vector<std::string> Then(std::vector<std::string> first, const std::vector<std::string>& rest)
{
	first.insert(first.end(), rest.begin(), rest.end());
	return first;
}

} // namespace

TEST(MonsterCorpseReplies, AMonsterShapedAsAPlayerRaceIsIgnored)
{
	World world;
	for (int type : {735, 736, 737, 792, 796, 800})
	{
		Receive(1005, {type});
		Receive(1003, {type});
	}
	CHECK(calls.empty());
	// The types just outside those ranges are not.
	Receive(1005, {734});
	Receive(1006, {791});
	Receive(1003, {801});
	CHECK(made.size() == 2 && std::count(calls.begin(), calls.end(), "type|1003|801") == 1);
}

TEST(MonsterCorpseReplies, ANewCorpseIsMadeAndLaid)
{
	World world;
	Receive(1005);
	CHECK(calls == Then(Made("1005"), {"zone lays|1005|still"}));
	// The corpse item: the ID, type 0, the cell and the treasure count.
	CHECK(zoneItems.count(1005) == 1);
	if (zoneItems.count(1005) == 1)
	{
		const MItem* pCorpse = zoneItems[1005];
		CHECK(pCorpse->GetItemType() == 0 && pCorpse->GetX() == 30 && pCorpse->GetY() == 31 && pCorpse->GetNumber() == 3);
	}
	// Headless, it loses its head before the corpse holds it.
	calls.clear();
	zoneTakesItems = 1;
	Receive(1006, {50, 1001, false});
	CHECK(calls.size() == 17 && calls[14] == "headless|1006" && calls[15] == "corpse of|1006");
}

TEST(MonsterCorpseReplies, ACorpseTheZoneRefusesIsLaidAfterTheItemInItsCell)
{
	World world;
	zoneTakesItems = 0;
	itemInTheCell = 2002;
	Receive(1005);
	CHECK(calls == Then(Made("1005"), {"zone lays|1005|still", "item at|30,31", "removes item|2002", "zone lays|1005|still"}));
	CHECK(zoneItems.count(1005) == 1);
}

TEST(MonsterCorpseReplies, AChristmasTreeShowsItsName)
{
	World world;
	Receive(1005, {482});
	CHECK(calls == Then(Made("1005", 482), {"zone lays|1005|still", "has|1005|" + N(EFFECTSTATUS_CLIENTONLY_XMAS_TREE) + "|65535",
		"says|1005|Grub|16777215"}));
	calls.clear();
	zoneTakesItems = 1;
	Receive(1006, {650});
	CHECK(calls == Then(Made("1006", 650), {"zone lays|1006|still", "says|1006|Grub|16777215"}));
}

TEST(MonsterCorpseReplies, AnItemAlreadyThereTakesTheTreasure)
{
	World world;
	Receive(2001);
	CHECK(calls.empty() && made.empty());
	CHECK_EQ(3, static_cast<int>(world.treasure.GetNumber()));
}

TEST(MonsterCorpseReplies, AKnownCreatureDiesWhereItIs)
{
	World world;
	Receive(1003, {51, 1001, false});
	CHECK(calls == std::vector<std::string>({"type|1003|51", "walks|1003", "moves|1003|30,31", "server at|1003|30,31", "turns|1003|5",
		"faces|1003|5", "status|1003|" + N(MODIFY_CURRENT_HP) + "|0", "stops|1003", "dies|1003", "holds|1003|3", "guild|1003|1",
		"killed by|1003|1001", "headless|1003"}));
	// Of the same type and dead already, it is not stopped or killed again.
	calls.clear();
	Receive(1003, {51});
	CHECK(calls == std::vector<std::string>({"moves|1003|30,31", "server at|1003|30,31", "turns|1003|5", "faces|1003|5",
		"status|1003|" + N(MODIFY_CURRENT_HP) + "|0", "holds|1003|3", "guild|1003|1", "killed by|1003|1001"}));
}

TEST(MonsterCorpseReplies, ThePlayersFirstKillShowsTheHelpOnce)
{
	World world;
	// Without a player the killer is nobody's.
	hasPlayer = false;
	Receive(1005, {50, 7});
	CHECK(std::none_of(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("help|", 0) == 0; }));
	hasPlayer = true;
	calls.clear();
	Receive(1006, {50, 7});
	Receive(1007, {50, 7});
	Receive(1003, {50, 7});
	CHECK(std::count(calls.begin(), calls.end(), "help|" + N(HELP_EVENT_KILL)) == 1);
	CHECK(calls.size() > 13 && calls[12] == "killed by|1006|7" && calls[13] == "help|" + N(HELP_EVENT_KILL));
}

TEST(MonsterCorpseReplies, WithoutAZoneOrTheHostsNothingHappens)
{
	World world;
	zoneExists = false;
	Receive(1005);
	Receive(1003);
	CHECK(calls.empty());
	zoneExists = true;
	// A zone host that cannot make a creature: nothing; one that cannot
	// make a corpse: the creature is freed.
	ZoneState::Host cannotMake = zone;
	cannotMake.NewCreature = nullptr;
	ZoneState::SetHost(&cannotMake);
	Receive(1005);
	CHECK(calls.empty());
	ZoneState::Host noCorpses = zone;
	noCorpses.NewCorpse = nullptr;
	ZoneState::SetHost(&noCorpses);
	Receive(1005);
	CHECK(calls.size() == 15 && calls[13] == "guild|1005|1" && calls.back() == "deleted|1005");
	// A creature host without the status array: no health is set, new or
	// known.
	calls.clear();
	ZoneState::SetHost(&zone);
	CreatureState::Host noStatus = creature;
	noStatus.Status = nullptr;
	CreatureState::SetHost(&noStatus);
	Receive(1006);
	Receive(1003);
	CHECK(std::none_of(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("status|", 0) == 0; }));
	CHECK(std::count(calls.begin(), calls.end(), "zone lays|1006|still") == 1 && std::count(calls.begin(), calls.end(), "holds|1003|3") == 1);
	calls.clear();
	CreatureState::SetHost(nullptr);
	ZoneState::SetHost(nullptr);
	CHECK(ZoneState::GetCreatureOnly(1003) == nullptr && ZoneState::GetItemID(1, 2) == OBJECTID_NULL && ZoneState::NewCorpse(nullptr) == nullptr);
	CHECK(!CreatureState::IsDead(Handle(&world.known)));
	CreatureState::SetCorpse(Handle(&world.known));
	CreatureState::SetItemCount(Handle(&world.known), 1);
	Receive(1005);
	CHECK(calls.empty());
}

TEST(MonsterCorpseReplies, ACorpseTheZoneWillNotTakeIsFreedAndItsCreatureLeftAlone)
{
	World world;
	// The zone refuses it, and the item in the cell will not go: the
	// corpse (and its creature) is freed, and nothing touches the creature
	// after.
	zoneTakesItems = 0;
	itemInTheCell = 2002;
	removesItem = false;
	Receive(1005, {482});
	CHECK(calls == Then(Made("1005", 482), {"zone lays|1005|still", "item at|30,31", "removes item|2002", "corpse freed|1005"}));
	// The zone refuses it again after the item goes: the same.
	calls.clear();
	removesItem = true;
	zoneTakesItems = -1;
	Receive(1006, {650});
	CHECK(calls == Then(Made("1006", 650), {"zone lays|1006|still", "item at|30,31", "removes item|2002", "zone lays|1006|still",
		"corpse freed|1006"}));
}
