#include "test_framework.h"
#include "gamemodel_world.h"
#include "MCreatureTable.h"
#include "MItem.h"
#include "MStatus.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCAddOustersCorpse.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

// An ouster's corpse appearing. When no creature and no item of the ID is
// in the zone, a dead creature that wears gear is made with the packet's
// advancement and type (an ouster's, the operator's for competence 0),
// with its ID, cell, direction and health (0 of its most), laid down as a
// corpse and named, in its guild, with its attack speed, alignment, hair
// and coat colours, and dressed as the packet describes; a corpse item of
// the same ID at the cell holds it (owning it) and the treasure count,
// and is laid in the zone, after the item in its cell if the zone refuses
// it first, or freed with its creature if it still cannot be laid. A
// corpse already there takes the treasure count; another item, nothing. A
// creature the zone knows dies where it is: it takes an ouster's type
// when its own differs (the operator's for competence 0; walking), the
// advancement, the cell, direction and health, attack speed, alignment
// and guild, stops and dies unless dead already, takes the treasure count
// and is dressed. Without a zone nothing happens.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
int zoneTakesItems = 1;
TYPE_OBJECTID itemInTheCell = OBJECTID_NULL;
bool removesItem = true;

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
	host.NewCreatureWear = []() -> MCreature* {
		made.push_back(std::make_unique<FakeCreature>());
		calls.push_back("new wearing creature");
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
	host.SetCompetence = [](MCreature* p, int competence) { calls.push_back("competence|" + Who(p) + "|" + N(competence)); };
	host.SetBodyColor1 = [](MCreature* p, std::uint16_t color) { calls.push_back("hair|" + Who(p) + "|" + N(color)); };
	host.SetBodyColor2 = [](MCreature* p, std::uint16_t color) { calls.push_back("coat|" + Who(p) + "|" + N(color)); };
	host.SetWeaponSpeed = [](MCreature* p, int speed) { calls.push_back("speed|" + Who(p) + "|" + N(speed)); };
	host.SetAddonToOusters3 = [](MCreature* p, const PCOustersInfo3* pInfo) {
		calls.push_back("dressed|" + Who(p) + "|" + N(pInfo->getObjectID()));
	};

	host.SetName = [](MCreature* p, const char* name) { calls.push_back("name|" + Who(p) + "|" + name); Fake(p).name = name; };
	host.SetGuildNumber = [](MCreature* p, int guild) { calls.push_back("guild|" + Who(p) + "|" + N(guild)); };
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
	FakeCreature fallen;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	World()
	{
		calls.clear();
		zoneExists = true;
		zoneTakesItems = 1;
		itemInTheCell = OBJECTID_NULL;
		removesItem = true;
		// The laid corpses read their creatures as they are freed.
		laid.clear();
		made.clear();
		zoneCreatures.clear();
		zoneItems.clear();
		known.id = 1003;
		known.type = CREATURETYPE_SLAYER_MALE;
		zoneCreatures[known.id] = &known;
		treasure.SetID(2001);
		zoneItems[treasure.GetID()] = &treasure;
	}
	~World()
	{
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		laid.clear();
		made.clear();
		zoneCreatures.clear();
		zoneItems.clear();
	}
};

// Every field set: the packet copies the ouster's details whole.
void Receive(ObjectID_t id, BYTE competence = 9)
{
	PCOustersInfo3 info;
	info.setObjectID(id);
	info.setName("Oust");
	info.setX(30);
	info.setY(31);
	info.setDir(5);
	info.setSex(FEMALE);
	info.setCoatColor(14);
	info.setHairColor(11);
	info.setArmColor(16);
	info.setBootsColor(15);
	info.setMasterEffectColor(2);
	info.setCoatType(OUSTERS_COAT_BASIC);
	info.setArmType(OUSTERS_ARM_CHAKRAM);
	info.setSylphType(OUSTERS_SYLPH_OFF);
	info.setCurrentHP(60);
	info.setMaxHP(90);
	info.setAttackSpeed(10);
	info.setAlignment(100);
	info.setCompetence(competence);
	info.setGuildID(7);
	info.setUnionID(8);
	info.setRank(4);
	info.setAdvancementLevel(2);
	GCAddOustersCorpse packet;
	packet.setOustersInfo(info);
	packet.setTreasureCount(3);
	GCAddOustersCorpseHandler::execute(&packet, nullptr);
}

std::string Status(const std::string& who, int index, int value) { return "status|" + who + "|" + N(index) + "|" + N(value); }

std::vector<std::string> Then(std::vector<std::string> first, const std::vector<std::string>& rest)
{
	first.insert(first.end(), rest.begin(), rest.end());
	return first;
}

std::vector<std::string> Made(const std::string& id, bool isOperator = false)
{
	std::vector<std::string> expected = {"new wearing creature", Status("new", MODIFY_ADVANCEMENT_CLASS_LEVEL, 2)};
	if (isOperator)
	{
		expected.push_back("competence|new|0");
	}
	expected.push_back("type|new|" + N(isOperator ? CREATURETYPE_OUSTERS_OPERATOR : CREATURETYPE_OUSTERS));
	return Then(expected, {"id|" + id, "at|" + id + "|30,31", "server at|" + id + "|30,31", "turns|" + id + "|5", "faces|" + id + "|5",
		Status(id, MODIFY_MAX_HP, 90), Status(id, MODIFY_CURRENT_HP, 0), "corpse|" + id, "name|" + id + "|Oust", "guild|" + id + "|7",
		"speed|" + id + "|10", Status(id, MODIFY_ALIGNMENT, 100), "hair|" + id + "|11", "coat|" + id + "|14", "dressed|" + id + "|" + id,
		"corpse of|" + id});
}

std::vector<std::string> Dies(const std::string& id)
{
	return {Status(id, MODIFY_ADVANCEMENT_CLASS_LEVEL, 2), "moves|" + id + "|30,31", "server at|" + id + "|30,31", "turns|" + id + "|5",
		"faces|" + id + "|5", Status(id, MODIFY_CURRENT_HP, 0), "speed|" + id + "|10", Status(id, MODIFY_ALIGNMENT, 100),
		"guild|" + id + "|7"};
}

} // namespace

TEST(OustersCorpseReplies, ANewCorpseIsMadeDressedAndLaid)
{
	World world;
	Receive(1005);
	CHECK(calls == Then(Made("1005"), {"zone lays|1005|still"}));
	CHECK(zoneItems.count(1005) == 1);
	if (zoneItems.count(1005) == 1)
	{
		const MItem* pCorpse = zoneItems[1005];
		CHECK(pCorpse->GetItemType() == 0 && pCorpse->GetX() == 30 && pCorpse->GetY() == 31 && pCorpse->GetNumber() == 3);
	}
	calls.clear();
	zoneTakesItems = 1;
	Receive(1006, 0);
	CHECK(calls == Then(Made("1006", true), {"zone lays|1006|still"}));
}

TEST(OustersCorpseReplies, ACorpseTheZoneRefusesIsLaidAfterTheItemInItsCellOrFreed)
{
	World world;
	zoneTakesItems = 0;
	itemInTheCell = 2002;
	Receive(1005);
	CHECK(calls == Then(Made("1005"), {"zone lays|1005|still", "item at|30,31", "removes item|2002", "zone lays|1005|still"}));
	calls.clear();
	zoneTakesItems = 0;
	removesItem = false;
	Receive(1006);
	CHECK(calls == Then(Made("1006"), {"zone lays|1006|still", "item at|30,31", "removes item|2002", "corpse freed|1006"}));
	calls.clear();
	removesItem = true;
	zoneTakesItems = -1;
	Receive(1007);
	CHECK(calls == Then(Made("1007"), {"zone lays|1007|still", "item at|30,31", "removes item|2002", "zone lays|1007|still",
		"corpse freed|1007"}));
}

TEST(OustersCorpseReplies, ACorpseAlreadyThereTakesTheTreasure)
{
	World world;
	FakeCorpse* pOld = new FakeCorpse(&world.fallen);
	pOld->SetID(2003);
	zoneItems[2003] = pOld;
	laid.emplace_back(pOld);
	Receive(2003);
	Receive(2001);
	CHECK(made.empty());
	CHECK_EQ(3, static_cast<int>(pOld->GetNumber()));
	CHECK_EQ(1, static_cast<int>(world.treasure.GetNumber()));
}

TEST(OustersCorpseReplies, AKnownCreatureDiesWhereItIs)
{
	World world;
	Receive(1003);
	CHECK(calls == Then(Then({"type|1003|" + N(CREATURETYPE_OUSTERS), "walks|1003"}, Dies("1003")),
		{"stops|1003", "dies|1003", "holds|1003|3", "dressed|1003|1003"}));
	// An ouster already and dead already, it is not retyped, stopped or
	// killed again.
	calls.clear();
	Receive(1003);
	CHECK(calls == Then(Dies("1003"), {"holds|1003|3", "dressed|1003|1003"}));
	// An operator of another type takes the operator's type; one of an
	// ouster's type keeps it, as the old code compared only the ouster's.
	calls.clear();
	Receive(1003, 0);
	CHECK(calls == Then(Dies("1003"), {"holds|1003|3", "dressed|1003|1003"}));
	calls.clear();
	world.known.type = CREATURETYPE_SLAYER_MALE;
	Receive(1003, 0);
	CHECK(calls.size() > 3 && calls[0] == "competence|1003|0" && calls[1] == "type|1003|" + N(CREATURETYPE_OUSTERS_OPERATOR)
		&& calls[2] == "walks|1003" && calls[3] == Status("1003", MODIFY_ADVANCEMENT_CLASS_LEVEL, 2));
}

TEST(OustersCorpseReplies, WithoutAZoneOrTheHostsNothingHappens)
{
	World world;
	zoneExists = false;
	Receive(1005);
	Receive(1003);
	CHECK(calls.empty());
	zoneExists = true;
	ZoneState::Host cannotMake = zone;
	cannotMake.NewCreatureWear = nullptr;
	ZoneState::SetHost(&cannotMake);
	Receive(1005);
	CHECK(calls.empty());
	ZoneState::Host noCorpses = zone;
	noCorpses.NewCorpse = nullptr;
	ZoneState::SetHost(&noCorpses);
	Receive(1005);
	std::vector<std::string> unlaid = Made("1005");
	unlaid.back() = "deleted|1005";
	CHECK(calls == unlaid);
	calls.clear();
	ZoneState::SetHost(&zone);
	CreatureState::Host noStatus = creature;
	noStatus.Status = nullptr;
	CreatureState::SetHost(&noStatus);
	Receive(1006);
	Receive(1003);
	CHECK(std::none_of(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("status|", 0) == 0; }));
	CHECK(std::count(calls.begin(), calls.end(), "zone lays|1006|still") == 1 && std::count(calls.begin(), calls.end(), "holds|1003|3") == 1);
	// Without the creature host the corpse is still made and laid; the
	// known creature's reply runs without one.
	calls.clear();
	zoneTakesItems = 1;
	CreatureState::SetHost(nullptr);
	Receive(1007);
	Receive(1003);
	CHECK(calls == std::vector<std::string>({"new wearing creature", "corpse of|new", "zone lays|1007|still"}));
	calls.clear();
	ZoneState::SetHost(nullptr);
	Receive(1005);
	CHECK(calls.empty());
}
