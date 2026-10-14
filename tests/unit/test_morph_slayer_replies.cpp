#include "test_framework.h"
#include "gamemodel_world.h"
#include "MCreatureTable.h"
#include "MStatus.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCMorphSlayer2.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

// A creature in sight turning into a slayer. One the zone does not know
// yet is made as a wearing creature, a male or female slayer by the
// packet's sex, walking, with its ID, cell, direction, skin colour,
// health and name, standing, dressed as the packet describes, in no guild
// (-1), and added to the zone (or dropped if the zone refuses it). A plain
// wearing creature the zone knows takes the slayer's type, walks, leaves
// any guild, moves to the cell and takes the direction, skin colour,
// health, name and dress in place; any other creature it knows (the
// player, an NPC, a fake, a plain creature) is left alone. Without a zone
// nothing happens.
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
	bool plainWear = true;
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
	host.IsPlainCreatureWear = [](MCreature* p) { return Fake(p).plainWear; };
	host.SetCreatureType = [](MCreature* p, int type) { calls.push_back("type|" + Who(p) + "|" + N(type)); };
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
	host.SetName = [](MCreature* p, const char* name) { calls.push_back("name|" + Who(p) + "|" + name); };
	host.SetGuildNumber = [](MCreature* p, int guild) { calls.push_back("guild|" + Who(p) + "|" + N(guild)); };
	host.SetAddonToSlayer3 = [](MCreature* p, const PCSlayerInfo3* pInfo) {
		calls.push_back("dressed|" + Who(p) + "|" + N(pInfo->getObjectID()) + "|jacket " + N(pInfo->getJacketType()));
	};
	host.Status = [](MCreature* p) -> MStatus* { return &Fake(p).status; };
	host.GetID = [](MCreature* p) { return Fake(p).id; };
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
	}
	~World()
	{
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		zoneCreatures.clear();
		made.clear();
	}
};

// Every field set: the packet copies the slayer's details whole.
void Receive(ObjectID_t id, Sex sex = MALE)
{
	PCSlayerInfo3 info;
	info.setObjectID(id);
	info.setName("Slayer");
	info.setX(30);
	info.setY(31);
	info.setDir(5);
	info.setSex(sex);
	info.setHairStyle(HAIR_STYLE1);
	info.setHelmetType(HELMET_NONE);
	info.setJacketType(JACKET2);
	info.setPantsType(PANTS_BASIC);
	info.setWeaponType(WEAPON_NONE);
	info.setShieldType(SHIELD_NONE);
	info.setMotorcycleType(MOTORCYCLE_NONE);
	info.setShoulderType(0);
	info.setHairColor(11);
	info.setSkinColor(12);
	for (ColorType type : {MAIN_COLOR, SUB_COLOR})
	{
		info.setHelmetColor(13, type);
		info.setJacketColor(14, type);
		info.setPantsColor(15, type);
		info.setWeaponColor(16, type);
		info.setShieldColor(17, type);
		info.setMotorcycleColor(18, type);
	}
	info.setShoulderColor(19);
	info.setMasterEffectColor(0);
	info.setCurrentHP(60);
	info.setMaxHP(90);
	info.setAttackSpeed(10);
	info.setAlignment(0);
	info.setCompetence(3);
	info.setGuildID(0);
	info.setUnionID(0);
	info.setRank(1);
	info.setAdvancementLevel(0);
	GCMorphSlayer2 packet;
	packet.setSlayerInfo(info);
	GCMorphSlayer2Handler::execute(&packet, nullptr);
}

std::string Health(const std::string& who) { return "status|" + who + "|" + N(MODIFY_MAX_HP) + "|90"; }
std::string Current(const std::string& who) { return "status|" + who + "|" + N(MODIFY_CURRENT_HP) + "|60"; }
std::string Dressed(const std::string& who) { return "dressed|" + who + "|" + who + "|jacket " + N(JACKET2); }

} // namespace

TEST(MorphSlayerReplies, ANewSlayerIsMadeDressedAndAdded)
{
	World world;
	Receive(1005);
	CHECK(calls == std::vector<std::string>({"new wearing creature", "type|new|" + N(CREATURETYPE_SLAYER_MALE), "walks|new", "id|1005",
		"at|1005|30,31", "server at|1005|30,31", "turns|1005|5", "faces|1005|5", "acts|1005|" + N(ACTION_STAND), "skin|1005|12",
		Health("1005"), Current("1005"), "name|1005|Slayer", Dressed("1005"), "guild|1005|-1", "zone takes|1005"}));
	CHECK(made.size() == 1 && zoneCreatures.count(1005) == 1);
	// A female one, and one the zone refuses, which is dropped.
	calls.clear();
	zoneTakes = false;
	Receive(1006, FEMALE);
	CHECK(calls.size() == 17 && calls[1] == "type|new|" + N(CREATURETYPE_SLAYER_FEMALE) && calls[15] == "zone takes|1006"
		&& calls[16] == "deleted|1006");
}

TEST(MorphSlayerReplies, AKnownWearingCreatureTurnsIntoTheSlayerInPlace)
{
	World world;
	Receive(1003, FEMALE);
	CHECK(calls == std::vector<std::string>({"type|1003|" + N(CREATURETYPE_SLAYER_FEMALE), "walks|1003", "guild|1003|-1",
		"server at|1003|30,31", "moves|1003|30,31", "turns|1003|5", "acts|1003|" + N(ACTION_STAND), "skin|1003|12",
		Health("1003"), Current("1003"), "name|1003|Slayer", Dressed("1003")}));
	CHECK(made.empty());
	// Any other creature it knows is left alone.
	calls.clear();
	world.known.plainWear = false;
	Receive(1003);
	CHECK(calls.empty());
}

TEST(MorphSlayerReplies, WithoutAZoneOrTheHostsNothingHappens)
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
	CHECK(std::count(calls.begin(), calls.end(), "zone takes|1006") == 1 && std::count(calls.begin(), calls.end(), "name|1003|Slayer") == 1);
	// The known one, a man here, takes the male slayer's type.
	CHECK(std::count(calls.begin(), calls.end(), "type|1003|" + N(CREATURETYPE_SLAYER_MALE)) == 1);
	// Without the creature host, a made one is still added; a known one is
	// no plain wearing creature.
	calls.clear();
	CreatureState::SetHost(nullptr);
	Receive(1007);
	Receive(1003);
	CHECK(calls == std::vector<std::string>({"new wearing creature", "zone takes|new"}));
	CHECK(!CreatureState::IsPlainCreatureWear(Handle(&world.known)));
	CreatureState::SetAddonToSlayer3(Handle(&world.known), nullptr);
	calls.clear();
	ZoneState::SetHost(nullptr);
	Receive(1005);
	CHECK(calls.empty());
}
