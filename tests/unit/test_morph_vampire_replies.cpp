#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "CreatureTypeSelection.h"
#include "ItemClassDef.h"
#include "MCreatureTable.h"
#include "MItemTable.h"
#include "MStatus.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCMorphVampire2.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

// A creature in sight turning into a vampire. Its creature type follows
// its shape: in its own shape, the frame its coat gives a man or a woman;
// a bat's, a wolf's or a werewolf's otherwise. One the zone does not know
// yet is made as a wearing creature with the packet's name, that type and
// sex, walking, with its ID, cell, direction, skin and coat colours,
// attack speed, health and alignment, standing, in guild 2, and added to
// the zone (or dropped if the zone refuses it). One the zone knows takes
// all that in place, moving to the cell. Without a zone nothing happens.
// An ouster's creature type is the ouster's, whatever its coat.
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
	host.SetName = [](MCreature* p, const char* name) { calls.push_back("name|" + Who(p) + "|" + name); };
	host.SetCreatureType = [](MCreature* p, int type) { calls.push_back("type|" + Who(p) + "|" + N(type)); };
	host.SetMale = [](MCreature* p, bool bMale) { calls.push_back(std::string(bMale ? "man|" : "woman|") + Who(p)); };
	host.SetGroundCreature = [](MCreature* p) { calls.push_back("walks|" + Who(p)); };
	host.SetID = [](MCreature* p, TYPE_OBJECTID id) { calls.push_back("id|" + N(id)); Fake(p).id = id; };
	host.SetPosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { calls.push_back("at|" + Who(p) + "|" + N(x) + "," + N(y)); };
	host.SetServerPosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("server at|" + Who(p) + "|" + N(x) + "," + N(y));
	};
	host.MovePosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("moves|" + Who(p) + "|" + N(x) + "," + N(y));
		return true;
	};
	host.SetDirection = [](MCreature* p, int direction) { calls.push_back("turns|" + Who(p) + "|" + N(direction)); };
	host.SetCurrentDirection = [](MCreature* p, int direction) { calls.push_back("faces|" + Who(p) + "|" + N(direction)); };
	host.SetAction = [](MCreature* p, int action) { calls.push_back("acts|" + Who(p) + "|" + N(action)); };
	host.SetBodyColor1 = [](MCreature* p, std::uint16_t color) { calls.push_back("skin|" + Who(p) + "|" + N(color)); };
	host.SetBodyColor2 = [](MCreature* p, std::uint16_t color) { calls.push_back("coat|" + Who(p) + "|" + N(color)); };
	host.SetWeaponSpeed = [](MCreature* p, int speed) { calls.push_back("speed|" + Who(p) + "|" + N(speed)); };
	host.SetGuildNumber = [](MCreature* p, int guild) { calls.push_back("guild|" + Who(p) + "|" + N(guild)); };
	host.Status = [](MCreature* p) -> MStatus* { return &Fake(p).status; };
	return host;
}();

struct World : GameModelWorld
{
	FakeCreature known;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	World()
	{
		calls.clear();
		zoneExists = true;
		zoneTakes = true;
		made.clear();
		zoneCreatures.clear();
		known.id = 1003;
		zoneCreatures[known.id] = &known;
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
		made.clear();
	}
};

// Every field set: the packet copies the vampire's details whole.
void Receive(ObjectID_t id, Sex sex = MALE, Shape_t shape = SHAPE_NORMAL)
{
	PCVampireInfo3 info;
	info.setObjectID(id);
	info.setName("Vamp");
	info.setX(30);
	info.setY(31);
	info.setDir(5);
	info.setSex(sex);
	info.setBatColor(11);
	info.setSkinColor(12);
	info.setCoatType(3);
	info.setCoatColor(14, MAIN_COLOR);
	info.setCoatColor(15, SUB_COLOR);
	info.setMasterEffectColor(0);
	info.setCurrentHP(60);
	info.setMaxHP(90);
	info.setAttackSpeed(10);
	info.setAlignment(100);
	info.setShape(shape);
	info.setCompetence(9);
	info.setGuildID(0);
	info.setUnionID(0);
	info.setRank(1);
	info.setAdvancementLevel(0);
	GCMorphVampire2 packet;
	packet.setVampireInfo(info);
	GCMorphVampire2Handler::execute(&packet, nullptr);
}

std::vector<std::string> Health(const std::string& who)
{
	return {"status|" + who + "|" + N(MODIFY_MAX_HP) + "|90", "status|" + who + "|" + N(MODIFY_CURRENT_HP) + "|60",
		"status|" + who + "|" + N(MODIFY_ALIGNMENT) + "|100"};
}

std::vector<std::string> Then(std::vector<std::string> first, const std::vector<std::string>& rest)
{
	first.insert(first.end(), rest.begin(), rest.end());
	return first;
}

} // namespace

TEST(MorphVampireReplies, AVampiresCreatureTypeFollowsItsShapeAndCoat)
{
	World world;
	CHECK_EQ(61, GetVampireCreatureType(SHAPE_NORMAL, true, 3));
	CHECK_EQ(62, GetVampireCreatureType(SHAPE_NORMAL, false, 3));
	CHECK_EQ(static_cast<int>(CREATURETYPE_BAT), GetVampireCreatureType(SHAPE_BAT, true, 3));
	CHECK_EQ(static_cast<int>(CREATURETYPE_WOLF), GetVampireCreatureType(SHAPE_WOLF, false, 3));
	CHECK_EQ(static_cast<int>(CREATURETYPE_WER_WOLF), GetVampireCreatureType(SHAPE_WERWOLF, true, 3));
	CHECK_EQ(static_cast<int>(SHAPE_MAX), GetVampireCreatureType(SHAPE_MAX, true, 3));
	// A coat past the table reads the table's default row.
	CHECK_EQ(static_cast<int>(FRAMEID_NULL), GetVampireCreatureType(SHAPE_NORMAL, true, 7));
	CHECK_EQ(static_cast<int>(CREATURETYPE_OUSTERS), GetOustersCreatureType(0));
	CHECK_EQ(static_cast<int>(CREATURETYPE_OUSTERS), GetOustersCreatureType(5));
}

TEST(MorphVampireReplies, ANewVampireIsMadeAndAdded)
{
	World world;
	Receive(1005);
	CHECK(calls == Then({"new wearing creature", "name|new|Vamp", "type|new|61", "man|new", "walks|new", "id|1005", "at|1005|30,31",
		"server at|1005|30,31", "turns|1005|5", "faces|1005|5", "acts|1005|" + N(ACTION_STAND), "skin|1005|12", "coat|1005|14",
		"speed|1005|10"}, Then(Health("1005"), {"guild|1005|2", "zone takes|1005"})));
	CHECK(made.size() == 1 && zoneCreatures.count(1005) == 1);
	// A woman as a bat, and one the zone refuses, which is dropped.
	calls.clear();
	zoneTakes = false;
	Receive(1006, FEMALE, SHAPE_BAT);
	CHECK(calls.size() == 20 && calls[2] == "type|new|" + N(CREATURETYPE_BAT) && calls[3] == "woman|new"
		&& calls[18] == "zone takes|1006" && calls[19] == "deleted|1006");
}

TEST(MorphVampireReplies, AKnownCreatureTurnsIntoTheVampireInPlace)
{
	World world;
	Receive(1003, FEMALE);
	CHECK(calls == Then({"name|1003|Vamp", "guild|1003|2", "type|1003|62", "woman|1003", "walks|1003", "moves|1003|30,31",
		"server at|1003|30,31", "turns|1003|5", "acts|1003|" + N(ACTION_STAND), "skin|1003|12", "coat|1003|14", "speed|1003|10"},
		Health("1003")));
	CHECK(made.empty());
	calls.clear();
	Receive(1003, MALE, SHAPE_WOLF);
	CHECK(calls.size() == 15 && calls[2] == "type|1003|" + N(CREATURETYPE_WOLF) && calls[3] == "man|1003");
}

TEST(MorphVampireReplies, WithoutAZoneOrTheHostsNothingHappens)
{
	World world;
	zoneExists = false;
	Receive(1005);
	Receive(1003);
	CHECK(calls.empty());
	zoneExists = true;
	// A zone host that cannot make a wearing creature: nothing.
	ZoneState::Host cannotMake = zone;
	cannotMake.NewCreatureWear = nullptr;
	ZoneState::SetHost(&cannotMake);
	Receive(1005);
	CHECK(calls.empty());
	// A creature host without the status array: no health is set, new or
	// known.
	ZoneState::SetHost(&zone);
	CreatureState::Host noStatus = creature;
	noStatus.Status = nullptr;
	CreatureState::SetHost(&noStatus);
	Receive(1006);
	Receive(1003);
	CHECK(std::none_of(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("status|", 0) == 0; }));
	CHECK(std::count(calls.begin(), calls.end(), "zone takes|1006") == 1 && std::count(calls.begin(), calls.end(), "speed|1003|10") == 1);
	// A new woman and a known man in their own shape take their sex's frame.
	calls.clear();
	Receive(1008, FEMALE);
	Receive(1003);
	CHECK(std::count(calls.begin(), calls.end(), "type|new|62") == 1 && std::count(calls.begin(), calls.end(), "type|1003|61") == 1);
	// Without the creature host, a made one is still added.
	calls.clear();
	CreatureState::SetHost(nullptr);
	Receive(1007);
	Receive(1003);
	CHECK(calls == std::vector<std::string>({"new wearing creature", "zone takes|new"}));
	calls.clear();
	ZoneState::SetHost(nullptr);
	Receive(1005);
	CHECK(calls.empty());
}
