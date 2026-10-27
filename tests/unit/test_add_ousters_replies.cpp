#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "CommunicationPresentationHost.h"
#include "EffectInfo.h"
#include "MCreatureTable.h"
#include "MGameStringTable.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCAddOusters.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

// An ouster appearing. Its type is the ouster's (an operator's for
// competence 0). One the zone does not know yet is made as a wearing
// creature with the packet's advancement, name, type, ID, cell,
// direction, hair and coat colours, master's aura, health, alignment,
// rank, attack speed, guild and union, standing, and added to the zone
// (or dropped if the zone refuses it); one the zone knows takes the type,
// advancement, guild, union, cell, direction, standing, colours, aura,
// health, alignment, rank and speed in place, its name kept. A creature
// that is no NPC by its type shows whether its personal shop is open, and
// an open one shows the shop's sign (or the default message) to an ouster
// player in the chat colour. The ouster then takes the packet's effect
// statuses, is dressed as the packet describes (any creature it lands on:
// the host dresses only one that wears gear), and takes its pet and its
// nickname. Without a zone nothing happens.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool zoneTakes = true;
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
	bool ousters = false;
	RecordingStatus status;
	FakeCreature() { status.owner = this; }
	FakeCreature(const FakeCreature&) = delete;
	FakeCreature& operator=(const FakeCreature&) = delete;
};
std::map<TYPE_OBJECTID, FakeCreature*> zoneCreatures;
std::vector<std::unique_ptr<FakeCreature>> made;
FakeCreature me;

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
	host.Creature = []() { return hasPlayer ? Handle(&me) : nullptr; };
	return host;
}();

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
	host.IsOusters = [](MCreature* p) { return Fake(p).ousters; };
	host.SetCompetence = [](MCreature* p, int competence) { calls.push_back("competence|" + Who(p) + "|" + N(competence)); };
	host.SetCreatureType = [](MCreature* p, int type) { calls.push_back("type|" + Who(p) + "|" + N(type)); Fake(p).type = type; };
	host.GetCreatureType = [](MCreature* p) { return Fake(p).type; };
	host.SetName = [](MCreature* p, const char* name) { calls.push_back("name|" + Who(p) + "|" + name); };
	host.SetGuildNumber = [](MCreature* p, int guild) { calls.push_back("guild|" + Who(p) + "|" + N(guild)); };
	host.SetUnionGuildID = [](MCreature* p, int unionID) { calls.push_back("union|" + Who(p) + "|" + N(unionID)); };
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
	host.SetAction = [](MCreature* p, int action) { calls.push_back("acts|" + Who(p) + "|" + N(action)); };
	host.SetBodyColor1 = [](MCreature* p, std::uint16_t color) { calls.push_back("hair|" + Who(p) + "|" + N(color)); };
	host.SetMasterEffectType = [](MCreature* p, int type) { calls.push_back("aura|" + Who(p) + "|" + N(type)); };
	host.SetBodyColor2 = [](MCreature* p, std::uint16_t color) { calls.push_back("coat|" + Who(p) + "|" + N(color)); };
	host.SetWeaponSpeed = [](MCreature* p, int speed) { calls.push_back("speed|" + Who(p) + "|" + N(speed)); };
	host.SetAddonToOusters3 = [](MCreature* p, const PCOustersInfo3* pInfo) {
		calls.push_back("dressed|" + Who(p) + "|" + N(pInfo->getObjectID()));
	};
	host.SetPersnalShop = [](MCreature* p, int open) { calls.push_back("shop|" + Who(p) + "|" + N(open)); };
	host.SetPersnalString = [](MCreature* p, const char* sign, std::uint32_t color) {
		calls.push_back("sign|" + Who(p) + "|" + (sign ? sign : "(null)") + "|" + N(color));
	};
	host.SetNickName = [](MCreature* p, std::uint8_t type, char* szNickName) {
		calls.push_back("nickname|" + Who(p) + "|" + N(type) + "|" + (szNickName ? szNickName : "(null)"));
	};
	host.Status = [](MCreature* p) -> MStatus* { return &Fake(p).status; };
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	return host;
}();

const CommunicationPresentation::Host communication = []() {
	CommunicationPresentation::Host host{};
	host.GetChattingColor = []() -> std::uint32_t { return 0x1234; };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.SetEffectInfo = [](MCreature* p, EffectInfo* pInfo, int delayedFrame) {
		calls.push_back("effects|" + Who(p) + "|" + N(pInfo == nullptr ? -1 : pInfo->getListNum()) + "|" + N(delayedFrame));
	};
	host.SetPetInfo = [](PetInfo* pPetInfo, TYPE_OBJECTID objectID) { calls.push_back(std::string(pPetInfo ? "pet|" : "no pet|") + N(objectID)); };
	return host;
}();

struct World : GameModelWorld
{
	FakeCreature known;
	CREATURE_TABLE creatureTypes;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	MStringArray nicknames;
	MStringArray* oldNicknames = g_pNickNameStringTable;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const CommunicationPresentation::Host* oldCommunication = CommunicationPresentation::SetHost(&communication);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		calls.clear();
		zoneExists = true;
		zoneTakes = true;
		hasPlayer = true;
		me.id = 7;
		me.ousters = true;
		made.clear();
		zoneCreatures.clear();
		known.id = 1003;
		known.type = CREATURETYPE_OUSTERS;
		zoneCreatures[known.id] = &known;
		creatureTypes.Init(CREATURETYPE_INSTALL_TURRET + 1);
		for (int type : {CREATURETYPE_OUSTERS, CREATURETYPE_OUSTERS_OPERATOR})
			testfw::MutableRow(creatureTypes, type).SetCreatureTribe(CREATURETRIBE_OUSTERS);
		g_pCreatureTable = &creatureTypes;
		nicknames.Init(2);
		nicknames.Set(0, "Novice");
		nicknames.Set(1, "Night Walker");
		g_pNickNameStringTable = &nicknames;
		g_pGameStringTable->Init(MAX_GAME_STRING);
		g_pGameStringTable->Set(UI_STRING_MESSAGE_PERSNAL_DEFAULT_MESSGE, "Welcome");
	}
	~World()
	{
		WorldEvents::SetHost(oldEvents);
		CommunicationPresentation::SetHost(oldCommunication);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		PlayerState::SetHost(oldPlayer);
		g_pNickNameStringTable = oldNicknames;
		g_pCreatureTable = oldCreatureTypes;
		zoneCreatures.clear();
		made.clear();
	}
};

struct Ousters
{
	Sex sex = FEMALE;
	BYTE competence = 9;
	BYTE shopOpen = 0;
	const char* sign = "";
	PetInfo* pet = nullptr;
	NicknameInfo* nickname = nullptr;
};

// Every field set: the packet copies the ouster's details whole.
void Receive(ObjectID_t id, const Ousters& ousters = {})
{
	PCOustersInfo3 info;
	info.setObjectID(id);
	info.setName("Oust");
	info.setX(30);
	info.setY(31);
	info.setDir(5);
	info.setSex(ousters.sex);
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
	info.setCompetence(ousters.competence);
	info.setGuildID(7);
	info.setUnionID(8);
	info.setRank(4);
	info.setAdvancementLevel(2);
	GCAddOusters packet;
	packet.setOustersInfo(info);
	EffectInfo* pEffects = new EffectInfo;
	pEffects->addListElement(EFFECTSTATUS_CURSE_PARALYSIS, 20);
	packet.setEffectInfo(pEffects);
	StoreInfo store;
	store.setOpen(ousters.shopOpen);
	store.setSign(ousters.sign);
	packet.setStoreInfo(&store);
	packet.setPetInfo(ousters.pet);
	packet.setNicknameInfo(ousters.nickname);
	GCAddOustersHandler::execute(&packet, nullptr);
	// The pet and the nickname are the caller's, not the packet's to free.
	packet.setPetInfo(nullptr);
	packet.setNicknameInfo(nullptr);
}

std::vector<std::string> Health(const std::string& who)
{
	return {"status|" + who + "|" + N(MODIFY_MAX_HP) + "|90", "status|" + who + "|" + N(MODIFY_CURRENT_HP) + "|60",
		"status|" + who + "|" + N(MODIFY_ALIGNMENT) + "|100", "status|" + who + "|" + N(MODIFY_RANK) + "|4"};
}

std::vector<std::string> Then(std::vector<std::string> first, const std::vector<std::string>& rest)
{
	first.insert(first.end(), rest.begin(), rest.end());
	return first;
}

} // namespace

TEST(AddOustersReplies, ANewOusterIsMadeAddedAndDressed)
{
	World world;
	Receive(1005);
	CHECK(calls == Then(Then({"new wearing creature", "status|new|" + N(MODIFY_ADVANCEMENT_CLASS_LEVEL) + "|2", "name|new|Oust",
		"type|new|" + N(CREATURETYPE_OUSTERS), "id|1005", "at|1005|30,31", "server at|1005|30,31", "turns|1005|5", "faces|1005|5",
		"acts|1005|" + N(ACTION_STAND), "hair|1005|11", "coat|1005|14", "aura|1005|2"}, Health("1005")),
		{"speed|1005|10", "guild|1005|7", "union|1005|8", "shop|1005|0", "zone takes|1005", "effects|1005|1|0", "dressed|1005|1005"}));
	CHECK(made.size() == 1 && zoneCreatures.count(1005) == 1);
	// Refused, it is dropped, with nothing after.
	calls.clear();
	zoneTakes = false;
	Receive(1006);
	CHECK(calls.size() >= 2 && calls[calls.size() - 2] == "zone takes|1006" && calls.back() == "deleted|1006");
}

TEST(AddOustersReplies, AKnownCreatureTakesItInPlaceAndIsDressed)
{
	World world;
	Receive(1003);
	CHECK(calls == Then(Then({"type|1003|" + N(CREATURETYPE_OUSTERS), "status|1003|" + N(MODIFY_ADVANCEMENT_CLASS_LEVEL) + "|2", "guild|1003|7",
		"union|1003|8", "moves|1003|30,31", "server at|1003|30,31", "turns|1003|5", "faces|1003|5", "acts|1003|" + N(ACTION_STAND),
		"hair|1003|11", "coat|1003|14", "aura|1003|2"}, Health("1003")), {"speed|1003|10", "shop|1003|0", "effects|1003|1|0", "dressed|1003|1003"}));
	CHECK(made.empty());
}

TEST(AddOustersReplies, AnOperatorIsTheOperators)
{
	World world;
	Receive(1005, {FEMALE, 0});
	Receive(1003, {FEMALE, 0});
	CHECK(calls.size() > 4 && calls[3] == "competence|new|0" && calls[4] == "type|new|" + N(CREATURETYPE_OUSTERS_OPERATOR));
	CHECK(std::count(calls.begin(), calls.end(), "competence|1003|0") == 1
		&& std::count(calls.begin(), calls.end(), "type|1003|" + N(CREATURETYPE_OUSTERS_OPERATOR)) == 1);
}

TEST(AddOustersReplies, AnOpenShopShowsItsSignToAnOusterPlayer)
{
	World world;
	Receive(1005, {FEMALE, 9, 1, "Cheap chakrams"});
	Receive(1004, {FEMALE, 9, 1, ""});
	Receive(1003, {FEMALE, 9, 1, ""});
	CHECK(std::count(calls.begin(), calls.end(), "shop|1005|1") == 1 && std::count(calls.begin(), calls.end(), "sign|1005|Cheap chakrams|4660") == 1);
	CHECK(std::count(calls.begin(), calls.end(), "sign|1004|Welcome|4660") == 1);
	CHECK(std::count(calls.begin(), calls.end(), "shop|1003|1") == 1 && std::count(calls.begin(), calls.end(), "sign|1003|Welcome|4660") == 1);
	calls.clear();
	Receive(1003, {FEMALE, 9, 1, "Cheap chakrams"});
	CHECK(std::count(calls.begin(), calls.end(), "sign|1003|Cheap chakrams|4660") == 1);
	// The default message is passed as the table holds it, NULL when it is
	// empty (MCreature::SetPersnalString ignores that).
	calls.clear();
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PERSNAL_DEFAULT_MESSGE, "");
	Receive(1003, {FEMALE, 9, 1, ""});
	Receive(1010, {FEMALE, 9, 1, ""});
	CHECK(std::count(calls.begin(), calls.end(), "sign|1003|(null)|4660") == 1 && std::count(calls.begin(), calls.end(), "sign|1010|(null)|4660") == 1);
	// No sign for a player who is no ouster, or for no player, new or
	// known; nothing at all for an NPC's type.
	calls.clear();
	me.ousters = false;
	Receive(1003, {FEMALE, 9, 1, "Cheap chakrams"});
	Receive(1009, {FEMALE, 9, 1, "Cheap chakrams"});
	me.ousters = true;
	hasPlayer = false;
	Receive(1006, {FEMALE, 9, 1, "Cheap chakrams"});
	Receive(1003, {FEMALE, 9, 1, "Cheap chakrams"});
	CHECK(std::none_of(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("sign|", 0) == 0; }));
	CHECK(std::count(calls.begin(), calls.end(), "shop|1003|1") == 2 && std::count(calls.begin(), calls.end(), "shop|1006|1") == 1
		&& std::count(calls.begin(), calls.end(), "shop|1009|1") == 1);
	calls.clear();
	hasPlayer = true;
	testfw::MutableRow(world.creatureTypes, CREATURETYPE_OUSTERS).SetCreatureTribe(CREATURETRIBE_OUSTERS_NPC);
	Receive(1007, {FEMALE, 9, 1, "Cheap chakrams"});
	Receive(1003, {FEMALE, 9, 1, "Cheap chakrams"});
	CHECK(std::none_of(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("sign|", 0) == 0 || call.rfind("shop|", 0) == 0; }));
}

TEST(AddOustersReplies, ItTakesItsPetAndNickname)
{
	World world;
	PetInfo pet;
	NicknameInfo custom, forced, built, past;
	custom.setNicknameType(NicknameInfo::NICK_CUSTOM);
	custom.setNickname(std::string("Wanderer"));
	custom.setNicknameIndex(1);
	forced.setNicknameType(NicknameInfo::NICK_CUSTOM_FORCED);
	forced.setNickname(std::string("Lady"));
	built.setNicknameType(NicknameInfo::NICK_BUILT_IN);
	built.setNicknameIndex(1);
	past.setNicknameType(NicknameInfo::NICK_QUEST);
	past.setNicknameIndex(2);
	Receive(1005, {FEMALE, 9, 0, "", &pet, &custom});
	CHECK(calls.size() >= 4 && calls[calls.size() - 4] == "effects|1005|1|0" && calls[calls.size() - 3] == "dressed|1005|1005"
		&& calls[calls.size() - 2] == "pet|1005" && calls.back() == "nickname|1005|" + N(NicknameInfo::NICK_CUSTOM) + "|Wanderer");
	calls.clear();
	Receive(1003, {FEMALE, 9, 0, "", nullptr, &forced});
	Receive(1003, {FEMALE, 9, 0, "", nullptr, &built});
	Receive(1003, {FEMALE, 9, 0, "", nullptr, &past});
	CHECK(std::count(calls.begin(), calls.end(), "nickname|1003|" + N(NicknameInfo::NICK_CUSTOM_FORCED) + "|Lady") == 1);
	CHECK(std::count(calls.begin(), calls.end(), "nickname|1003|" + N(NicknameInfo::NICK_BUILT_IN) + "|Night Walker") == 1);
	CHECK(std::count(calls.begin(), calls.end(), "nickname|1003|" + N(NicknameInfo::NICK_QUEST) + "|Novice") == 1);
	CHECK(std::none_of(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("pet|", 0) == 0; }));
}

TEST(AddOustersReplies, WithoutAZoneOrTheHostsNothingHappens)
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
	// A creature host without the status array: no status is set, new or
	// known.
	ZoneState::SetHost(&zone);
	CreatureState::Host noStatus = creature;
	noStatus.Status = nullptr;
	CreatureState::SetHost(&noStatus);
	Receive(1006);
	Receive(1003);
	CHECK(std::none_of(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("status|", 0) == 0; }));
	CHECK(std::count_if(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("dressed|", 0) == 0; }) == 2);
	// Without the creature host, a made one is still added and takes its
	// effects.
	calls.clear();
	CreatureState::SetHost(nullptr);
	Receive(1008);
	CHECK(calls == std::vector<std::string>({"new wearing creature", "zone takes|new", "effects|new|1|0"}));
	CreatureState::SetAddonToOusters3(Handle(&world.known), nullptr);
	calls.clear();
	ZoneState::SetHost(nullptr);
	Receive(1005);
	CHECK(calls.empty());
}
