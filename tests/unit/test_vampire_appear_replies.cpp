#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "EffectInfo.h"
#include "ItemClassDef.h"
#include "MCreatureTable.h"
#include "MItemTable.h"
#include "MStatus.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCAddVampireFromBurrowing.h"
#include "Gpackets/GCAddVampireFromTransformation.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

// A vampire out of a burrow, or out of a transformation. Its creature type
// follows its shape and coat (an operator's, competence 0, is the
// operator's); a bat flies and any other walks. One the zone does not know
// yet is made as a wearing creature with the packet's advancement, type,
// sex, name, guild, ID, cell, direction, health, alignment, skin and coat
// colours and attack speed, standing, and added to the zone (or dropped
// if the zone refuses it). Out of a burrow, one the zone knows takes all
// that in place, moving to the cell; out of a transformation, a plain
// wearing creature it knows does the same, while any other is removed and
// made again (without the advancement, which went on the old one). The
// vampire then takes the packet's effect statuses and comes up out of the
// ground, or out of its transformation. Without a zone nothing happens.
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
	host.RemoveCreature = [](TYPE_OBJECTID id) { calls.push_back("removed|" + N(id)); zoneCreatures.erase(id); return true; };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.IsPlainCreatureWear = [](MCreature* p) { return Fake(p).plainWear; };
	host.SetCompetence = [](MCreature* p, int competence) { calls.push_back("competence|" + Who(p) + "|" + N(competence)); };
	host.SetCreatureType = [](MCreature* p, int type) { calls.push_back("type|" + Who(p) + "|" + N(type)); Fake(p).type = type; };
	host.GetCreatureType = [](MCreature* p) { return Fake(p).type; };
	host.SetMale = [](MCreature* p, bool bMale) { calls.push_back(std::string(bMale ? "man|" : "woman|") + Who(p)); };
	host.SetFlyingCreature = [](MCreature* p) { calls.push_back("flies|" + Who(p)); };
	host.SetGroundCreature = [](MCreature* p) { calls.push_back("walks|" + Who(p)); };
	host.SetName = [](MCreature* p, const char* name) { calls.push_back("name|" + Who(p) + "|" + name); };
	host.SetGuildNumber = [](MCreature* p, int guild) { calls.push_back("guild|" + Who(p) + "|" + N(guild)); };
	host.SetID = [](MCreature* p, TYPE_OBJECTID id) { calls.push_back("id|" + N(id)); Fake(p).id = id; };
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
	host.SetBodyColor1 = [](MCreature* p, std::uint16_t color) { calls.push_back("skin|" + Who(p) + "|" + N(color)); };
	host.SetBodyColor2 = [](MCreature* p, std::uint16_t color) { calls.push_back("coat|" + Who(p) + "|" + N(color)); };
	host.SetWeaponSpeed = [](MCreature* p, int speed) { calls.push_back("speed|" + Who(p) + "|" + N(speed)); };
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
		zoneCreatures[known.id] = &known;
		// Coat 3 dresses a man as frame 61 and a woman as 62.
		g_pItemTable->InitClass(ITEM_CLASS_VAMPIRE_COAT, 4);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_VAMPIRE_COAT, 3).AddonMaleFrameID = 61;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_VAMPIRE_COAT, 3).AddonFemaleFrameID = 62;
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

struct Vampire
{
	Sex sex = MALE;
	Shape_t shape = SHAPE_NORMAL;
	BYTE competence = 3;
};

// Every field set: the packet copies the vampire's details whole.
template<class PacketType>
void Fill(PacketType& packet, ObjectID_t id, const Vampire& vampire)
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
	info.setMasterEffectColor(0);
	info.setCurrentHP(60);
	info.setMaxHP(90);
	info.setAttackSpeed(10);
	info.setAlignment(100);
	info.setShape(vampire.shape);
	info.setCompetence(vampire.competence);
	info.setGuildID(7);
	info.setUnionID(0);
	info.setRank(1);
	info.setAdvancementLevel(2);
	packet.setVampireInfo(info);
	EffectInfo* pEffects = new EffectInfo;
	pEffects->addListElement(EFFECTSTATUS_CURSE_PARALYSIS, 20);
	packet.setEffectInfo(pEffects);
}

void ReceiveBurrowing(ObjectID_t id, const Vampire& vampire = {})
{
	GCAddVampireFromBurrowing packet;
	Fill(packet, id, vampire);
	GCAddVampireFromBurrowingHandler::execute(&packet, nullptr);
}

void ReceiveTransformation(ObjectID_t id, const Vampire& vampire = {})
{
	GCAddVampireFromTransformation packet;
	Fill(packet, id, vampire);
	GCAddVampireFromTransformationHandler::execute(&packet, nullptr);
}

std::string Advancement(const std::string& who) { return "status|" + who + "|" + N(MODIFY_ADVANCEMENT_CLASS_LEVEL) + "|2"; }
std::vector<std::string> Health(const std::string& who)
{
	return {"status|" + who + "|" + N(MODIFY_MAX_HP) + "|90", "status|" + who + "|" + N(MODIFY_CURRENT_HP) + "|60",
		"status|" + who + "|" + N(MODIFY_ALIGNMENT) + "|100"};
}
std::vector<std::string> Colours(const std::string& who) { return {"skin|" + who + "|12", "coat|" + who + "|14", "speed|" + who + "|10"}; }
// The effect statuses, then the vampire coming up at the cell.
std::vector<std::string> Up(const std::string& who, TYPE_ACTIONINFO action)
{
	return {"effects|" + who + "|1|0", "effect|" + N(action) + "|30,31|dir 5|nobody@30,31|0"};
}

std::vector<std::string> Then(std::vector<std::string> first, const std::vector<std::string>& rest)
{
	first.insert(first.end(), rest.begin(), rest.end());
	return first;
}

} // namespace

TEST(VampireAppearReplies, ANewVampireComesOutOfItsBurrow)
{
	World world;
	ReceiveBurrowing(1005);
	CHECK(calls == Then(Then(Then(Then({"new wearing creature", Advancement("new"), "type|new|61", "man|new", "walks|new", "name|new|Vamp",
		"guild|new|7", "id|1005", "at|1005|30,31", "server at|1005|30,31", "turns|1005|5", "faces|1005|5", "acts|1005|" + N(ACTION_STAND)},
		Health("1005")), Colours("1005")), {"zone takes|1005"}), Up("1005", RESULT_MAGIC_HIDE)));
	CHECK(made.size() == 1 && zoneCreatures.count(1005) == 1);
	// Refused, it is dropped, with no effect.
	calls.clear();
	zoneTakes = false;
	ReceiveBurrowing(1006);
	CHECK(calls.size() >= 2 && calls[calls.size() - 2] == "zone takes|1006" && calls.back() == "deleted|1006");
}

TEST(VampireAppearReplies, AKnownCreatureComesOutOfTheBurrowInPlace)
{
	World world;
	ReceiveBurrowing(1003, {FEMALE});
	CHECK(calls == Then(Then(Then({Advancement("1003"), "name|1003|Vamp", "guild|1003|7", "type|1003|62", "woman|1003", "walks|1003", "id|1003",
		"moves|1003|30,31", "server at|1003|30,31", "turns|1003|5", "acts|1003|" + N(ACTION_STAND)}, Colours("1003")), Health("1003")),
		Up("1003", RESULT_MAGIC_HIDE)));
	CHECK(made.empty());
}

TEST(VampireAppearReplies, ABatFliesAndAnOperatorIsTheOperators)
{
	World world;
	ReceiveBurrowing(1005, {MALE, SHAPE_BAT});
	CHECK(calls.size() > 5 && calls[2] == "type|new|" + N(CREATURETYPE_BAT) && calls[4] == "flies|new");
	calls.clear();
	ReceiveTransformation(1006, {FEMALE, SHAPE_NORMAL, 0});
	CHECK(calls.size() > 6 && calls[2] == "competence|new|0" && calls[3] == "type|new|" + N(CREATURETYPE_VAMPIRE_OPERATOR)
		&& calls[4] == "woman|new" && calls[5] == "woman|new" && calls[6] == "walks|new");
	calls.clear();
	ReceiveBurrowing(1003, {MALE, SHAPE_BAT, 0});
	CHECK(calls.size() > 7 && calls[3] == "competence|1003|0" && calls[4] == "type|1003|" + N(CREATURETYPE_VAMPIRE_OPERATOR)
		&& calls[7] == "walks|1003");
	calls.clear();
	ReceiveTransformation(1003, {MALE, SHAPE_BAT});
	CHECK(calls.size() > 5 && calls[2] == "type|1003|" + N(CREATURETYPE_BAT) && calls[4] == "flies|1003");
	// Out of a burrow too: a new operator, and a known bat.
	calls.clear();
	ReceiveBurrowing(1007, {FEMALE, SHAPE_NORMAL, 0});
	CHECK(calls.size() > 6 && calls[2] == "competence|new|0" && calls[3] == "type|new|" + N(CREATURETYPE_VAMPIRE_OPERATOR)
		&& calls[6] == "walks|new");
	calls.clear();
	ReceiveBurrowing(1003, {MALE, SHAPE_BAT});
	CHECK(calls.size() > 6 && calls[3] == "type|1003|" + N(CREATURETYPE_BAT) && calls[5] == "flies|1003");
}

TEST(VampireAppearReplies, ATransformedVampireIsMadeOrTakesTheFormInPlace)
{
	World world;
	ReceiveTransformation(1005);
	CHECK(calls == Then(Then(Then(Then({"new wearing creature", Advancement("new"), "type|new|61", "man|new", "walks|new", "id|1005",
		"at|1005|30,31", "server at|1005|30,31", "turns|1005|5", "faces|1005|5", "acts|1005|" + N(ACTION_STAND)}, Colours("1005")),
		Then({"name|1005|Vamp", "guild|1005|7"}, Health("1005"))), {"zone takes|1005"}), Up("1005", RESULT_MAGIC_UN_TRANSFORM)));
	// A plain wearing creature the zone knows changes in place.
	calls.clear();
	ReceiveTransformation(1003);
	CHECK(calls == Then(Then(Then({Advancement("1003"), "moves|1003|30,31", "type|1003|61", "man|1003", "walks|1003", "server at|1003|30,31",
		"turns|1003|5", "faces|1003|5", "acts|1003|" + N(ACTION_STAND)}, Colours("1003")), Then({"guild|1003|7"}, Health("1003"))),
		Up("1003", RESULT_MAGIC_UN_TRANSFORM)));
	// A refused new one is dropped, with no effect.
	calls.clear();
	zoneTakes = false;
	ReceiveTransformation(1006);
	CHECK(calls.size() >= 2 && calls[calls.size() - 2] == "zone takes|1006" && calls.back() == "deleted|1006");
}

TEST(VampireAppearReplies, AnotherKnownCreatureIsMadeAgainAsAWearingOne)
{
	World world;
	world.known.plainWear = false;
	ReceiveTransformation(1003);
	CHECK(calls == Then(Then(Then(Then({Advancement("1003"), "removed|1003", "new wearing creature", "id|1003", "name|1003|Vamp",
		"at|1003|30,31", "type|1003|61", "man|1003", "walks|1003", "server at|1003|30,31", "turns|1003|5", "faces|1003|5",
		"acts|1003|" + N(ACTION_STAND)}, Colours("1003")), Then({"guild|1003|7"}, Health("1003"))), {"zone takes|1003"}),
		Up("1003", RESULT_MAGIC_UN_TRANSFORM)));
	// Refused, the new one is dropped, with no effect.
	calls.clear();
	made.back()->plainWear = false;
	zoneTakes = false;
	ReceiveTransformation(1003);
	CHECK(calls.size() >= 2 && calls[calls.size() - 2] == "zone takes|1003" && calls.back() == "deleted|1003");
}

TEST(VampireAppearReplies, WithoutAZoneOrTheHostsNothingHappens)
{
	World world;
	zoneExists = false;
	ReceiveBurrowing(1005);
	ReceiveTransformation(1003);
	CHECK(calls.empty());
	zoneExists = true;
	// A zone host that cannot make a wearing creature: nothing, but the
	// old creature a transformation removed is gone.
	ZoneState::Host cannotMake = zone;
	cannotMake.NewCreatureWear = nullptr;
	ZoneState::SetHost(&cannotMake);
	ReceiveBurrowing(1005);
	ReceiveTransformation(1005);
	CHECK(calls.empty());
	world.known.plainWear = false;
	ReceiveTransformation(1003);
	CHECK(calls == std::vector<std::string>({Advancement("1003"), "removed|1003"}));
	calls.clear();
	world.known.plainWear = true;
	zoneCreatures[world.known.id] = &world.known;
	// A creature host without the status array: no status is set, new or
	// known, in place or made again.
	ZoneState::SetHost(&zone);
	CreatureState::Host noStatus = creature;
	noStatus.Status = nullptr;
	CreatureState::SetHost(&noStatus);
	ReceiveBurrowing(1006);
	ReceiveBurrowing(1003);
	ReceiveTransformation(1007);
	ReceiveTransformation(1003);
	world.known.plainWear = false;
	ReceiveTransformation(1003);
	CHECK(std::none_of(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("status|", 0) == 0; }));
	CHECK(std::count_if(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("effects|", 0) == 0; }) == 5);
	// Without the creature host, a made one is still added and comes out.
	calls.clear();
	CreatureState::SetHost(nullptr);
	ReceiveBurrowing(1008);
	CHECK(calls == std::vector<std::string>({"new wearing creature", "zone takes|new", "effects|new|1|0",
		"effect|" + N(RESULT_MAGIC_HIDE) + "|0,0|dir 0|nobody@0,0|0"}));
	CHECK(!CreatureState::IsPlainCreatureWear(Handle(&world.known)));
	CreatureState::SetCompetence(Handle(&world.known), 0);
	calls.clear();
	ZoneState::SetHost(nullptr);
	ReceiveBurrowing(1005);
	CHECK(calls.empty());
}
