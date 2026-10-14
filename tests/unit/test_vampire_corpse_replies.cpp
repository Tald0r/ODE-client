#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "ItemClassDef.h"
#include "MItemTable.h"
#include "MCreatureTable.h"
#include "MItem.h"
#include "MStatus.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCAddVampireCorpse.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

// A vampire's corpse appearing. When no creature and no item of the ID is
// in the zone, a dead creature that wears gear is made with the packet's
// advancement, type (by its shape and coat, or the operator's for
// competence 0) and sex, flying if a bat and walking otherwise, with its
// ID, cell, direction and health (0 of its most), laid down as a corpse,
// named, in its guild, with its attack speed, alignment and skin and coat
// colours; a corpse item of the same ID at the cell holds it (owning it)
// and the packet's treasure count, and is laid in the zone, after the
// item in its cell if the zone refuses it first, or freed with its
// creature if it still cannot be laid. A corpse already there takes the
// treasure count; another item, nothing. A creature the zone
// knows dies where it is: it takes the advancement, the type when it
// differs (walking), the cell, direction, health, speed, alignment and
// guild, stops and dies unless dead already, and takes the treasure
// count. Without a zone nothing happens.
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
	host.SetMale = [](MCreature* p, bool bMale) { calls.push_back(std::string(bMale ? "man|" : "woman|") + Who(p)); };
	host.SetFlyingCreature = [](MCreature* p) { calls.push_back("flies|" + Who(p)); };
	host.SetWeaponSpeed = [](MCreature* p, int speed) { calls.push_back("speed|" + Who(p) + "|" + N(speed)); };
	host.SetBodyColor1 = [](MCreature* p, std::uint16_t color) { calls.push_back("skin|" + Who(p) + "|" + N(color)); };
	host.SetBodyColor2 = [](MCreature* p, std::uint16_t color) { calls.push_back("coat|" + Who(p) + "|" + N(color)); };
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
		known.type = 61;
		zoneCreatures[known.id] = &known;
		treasure.SetID(2001);
		zoneItems[treasure.GetID()] = &treasure;
		// Coat 3 dresses a man as frame 61 and a woman as 62.
		g_pItemTable->InitClass(ITEM_CLASS_VAMPIRE_COAT, 4);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_VAMPIRE_COAT, 3).AddonMaleFrameID = 61;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_VAMPIRE_COAT, 3).AddonFemaleFrameID = 62;
	}
	~World()
	{
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		zoneCreatures.clear();
		zoneItems.clear();
		laid.clear();
		made.clear();
	}
};

struct Vampire
{
	Sex sex = MALE;
	Shape_t shape = SHAPE_NORMAL;
	BYTE competence = 9;
};

// Every field set: the packet copies the vampire's details whole.
void Receive(ObjectID_t id, const Vampire& vampire = {})
{
	PCVampireInfo3 info;
	info.setObjectID(id);
	info.setName("Vamp");
	info.setX(30);
	info.setY(31);
	info.setDir(5);
	info.setSex(vampire.sex);
	info.setBatColor(11);
	info.setSkinColor(12);
	info.setCoatType(3);
	info.setCoatColor(14, MAIN_COLOR);
	info.setCoatColor(15, SUB_COLOR);
	info.setMasterEffectColor(2);
	info.setCurrentHP(60);
	info.setMaxHP(90);
	info.setAttackSpeed(10);
	info.setAlignment(100);
	info.setShape(vampire.shape);
	info.setCompetence(vampire.competence);
	info.setGuildID(7);
	info.setUnionID(8);
	info.setRank(4);
	info.setAdvancementLevel(2);
	GCAddVampireCorpse packet;
	packet.setVampireInfo(info);
	packet.setTreasureCount(3);
	GCAddVampireCorpseHandler::execute(&packet, nullptr);
}

std::string Advancement(const std::string& who) { return "status|" + who + "|" + N(MODIFY_ADVANCEMENT_CLASS_LEVEL) + "|2"; }

std::vector<std::string> Made(const std::string& id, int type = 61)
{
	return {"new wearing creature", Advancement("new"), "type|new|" + N(type), "man|new", "walks|new", "id|" + id, "at|" + id + "|30,31",
		"server at|" + id + "|30,31", "turns|" + id + "|5", "faces|" + id + "|5", "status|" + id + "|" + N(MODIFY_MAX_HP) + "|90",
		"status|" + id + "|" + N(MODIFY_CURRENT_HP) + "|0", "corpse|" + id, "name|" + id + "|Vamp", "guild|" + id + "|7",
		"speed|" + id + "|10", "status|" + id + "|" + N(MODIFY_ALIGNMENT) + "|100", "skin|" + id + "|12", "coat|" + id + "|14",
		"corpse of|" + id};
}

std::vector<std::string> Then(std::vector<std::string> first, const std::vector<std::string>& rest)
{
	first.insert(first.end(), rest.begin(), rest.end());
	return first;
}

} // namespace

TEST(VampireCorpseReplies, ANewCorpseIsMadeAndLaid)
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
	// A woman's frame; a bat flies.
	calls.clear();
	zoneTakesItems = 2;
	Receive(1006, {FEMALE});
	Receive(1007, {MALE, SHAPE_BAT});
	CHECK(std::count(calls.begin(), calls.end(), "type|new|62") == 1 && std::count(calls.begin(), calls.end(), "woman|new") == 1);
	CHECK(std::count(calls.begin(), calls.end(), "type|new|" + N(CREATURETYPE_BAT)) == 1 && std::count(calls.begin(), calls.end(), "flies|new") == 1);
}

TEST(VampireCorpseReplies, AnOperatorsCorpseIsTheOperators)
{
	World world;
	Receive(1005, {FEMALE, SHAPE_NORMAL, 0});
	CHECK(calls.size() > 6 && calls[2] == "competence|new|0" && calls[3] == "type|new|" + N(CREATURETYPE_VAMPIRE_OPERATOR)
		&& calls[4] == "woman|new" && calls[5] == "woman|new" && calls[6] == "walks|new");
}

TEST(VampireCorpseReplies, ACorpseTheZoneRefusesIsLaidAfterTheItemInItsCellOrFreed)
{
	World world;
	zoneTakesItems = 0;
	itemInTheCell = 2002;
	Receive(1005);
	CHECK(calls == Then(Made("1005"), {"zone lays|1005|still", "item at|30,31", "removes item|2002", "zone lays|1005|still"}));
	// It will not go: the corpse (and its creature) is freed.
	calls.clear();
	zoneTakesItems = 0;
	removesItem = false;
	Receive(1006);
	CHECK(calls == Then(Made("1006"), {"zone lays|1006|still", "item at|30,31", "removes item|2002", "corpse freed|1006"}));
	// Refused again after the item goes: the same.
	calls.clear();
	removesItem = true;
	zoneTakesItems = -1;
	Receive(1007);
	CHECK(calls == Then(Made("1007"), {"zone lays|1007|still", "item at|30,31", "removes item|2002", "zone lays|1007|still",
		"corpse freed|1007"}));
}

TEST(VampireCorpseReplies, ACorpseAlreadyThereTakesTheTreasure)
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
	// Another item keeps its number.
	CHECK_EQ(1, static_cast<int>(world.treasure.GetNumber()));
}

TEST(VampireCorpseReplies, AKnownCreatureDiesWhereItIs)
{
	World world;
	Receive(1003, {FEMALE});
	CHECK(calls == std::vector<std::string>({Advancement("1003"), "type|1003|62", "walks|1003", "moves|1003|30,31", "server at|1003|30,31",
		"turns|1003|5", "faces|1003|5", "status|1003|" + N(MODIFY_CURRENT_HP) + "|0", "speed|1003|10",
		"status|1003|" + N(MODIFY_ALIGNMENT) + "|100", "guild|1003|7", "stops|1003", "dies|1003", "holds|1003|3"}));
	// Of the same type and dead already, it is not stopped or killed again.
	calls.clear();
	Receive(1003, {FEMALE});
	CHECK(calls == std::vector<std::string>({Advancement("1003"), "moves|1003|30,31", "server at|1003|30,31", "turns|1003|5",
		"faces|1003|5", "status|1003|" + N(MODIFY_CURRENT_HP) + "|0", "speed|1003|10", "status|1003|" + N(MODIFY_ALIGNMENT) + "|100",
		"guild|1003|7", "holds|1003|3"}));
	// One now a bat takes the bat's type but walks, as the known branch
	// always has.
	calls.clear();
	Receive(1003, {MALE, SHAPE_BAT});
	CHECK(calls.size() > 2 && calls[1] == "type|1003|" + N(CREATURETYPE_BAT) && calls[2] == "walks|1003");
	// An operator of another type takes the operator's type.
	calls.clear();
	Receive(1003, {MALE, SHAPE_NORMAL, 0});
	CHECK(calls.size() > 4 && calls[1] == "competence|1003|0" && calls[2] == "type|1003|" + N(CREATURETYPE_VAMPIRE_OPERATOR)
		&& calls[3] == "man|1003" && calls[4] == "walks|1003");
}

TEST(VampireCorpseReplies, WithoutAZoneOrTheHostsNothingHappens)
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
	cannotMake.NewCreatureWear = nullptr;
	ZoneState::SetHost(&cannotMake);
	Receive(1005);
	CHECK(calls.empty());
	ZoneState::Host noCorpses = zone;
	noCorpses.NewCorpse = nullptr;
	ZoneState::SetHost(&noCorpses);
	Receive(1005);
	CHECK(calls.size() == 20 && calls[18] == "coat|1005|14" && calls.back() == "deleted|1005");
	// A creature host without the status array: no status is set, new or
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
	Receive(1005);
	CHECK(calls.empty());
}
