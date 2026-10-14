#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "CommunicationPresentationHost.h"
#include "EffectInfo.h"
#include "ItemClassDef.h"
#include "MCreatureTable.h"
#include "MGameStringTable.h"
#include "MItemTable.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCAddVampire.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

// A vampire appearing. Its creature type follows its shape and coat, or
// is the operator's for competence 0; a bat flies and any other walks. One
// the zone does not know yet is made as a wearing creature with the
// packet's advancement, name, type, sex, ID, cell, direction, colours (a
// werewolf's body in its coat colour twice), master's aura, health,
// alignment, rank, attack speed, guild and union, and bat colour (its own,
// 0xFFFF, for none), standing, and added to the zone (or dropped if the
// zone refuses it); one the zone knows takes all that in place, moving to
// the cell, its name kept. A creature that is no NPC by its type shows
// whether its personal shop is open, and an open one shows the shop's
// sign (or the default message) to a vampire player in the chat colour.
// The vampire then takes the packet's effect statuses, comes up out of a
// portal when it came through one, takes its pet, and takes its nickname:
// a custom one as sent, another by its index into the nickname table (the
// first past the table). Without a zone nothing happens.
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
	bool vampire = false;
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
	host.IsVampire = [](MCreature* p) { return Fake(p).vampire; };
	host.SetCompetence = [](MCreature* p, int competence) { calls.push_back("competence|" + Who(p) + "|" + N(competence)); };
	host.SetCreatureType = [](MCreature* p, int type) { calls.push_back("type|" + Who(p) + "|" + N(type)); Fake(p).type = type; };
	host.GetCreatureType = [](MCreature* p) { return Fake(p).type; };
	host.SetMale = [](MCreature* p, bool bMale) { calls.push_back(std::string(bMale ? "man|" : "woman|") + Who(p)); };
	host.SetFlyingCreature = [](MCreature* p) { calls.push_back("flies|" + Who(p)); };
	host.SetGroundCreature = [](MCreature* p) { calls.push_back("walks|" + Who(p)); };
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
	host.SetBodyColor1 = [](MCreature* p, std::uint16_t color) { calls.push_back("body 1|" + Who(p) + "|" + N(color)); };
	host.SetBodyColor2 = [](MCreature* p, std::uint16_t color) { calls.push_back("body 2|" + Who(p) + "|" + N(color)); };
	host.SetMasterEffectType = [](MCreature* p, int type) { calls.push_back("aura|" + Who(p) + "|" + N(type)); };
	host.SetWeaponSpeed = [](MCreature* p, int speed) { calls.push_back("speed|" + Who(p) + "|" + N(speed)); };
	host.SetBatColor = [](MCreature* p, std::uint16_t color) { calls.push_back("bat colour|" + Who(p) + "|" + N(color)); };
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
	host.ComeFromPortal = [](MCreature* p) { calls.push_back("from portal|" + Who(p)); };
	host.SetPetInfo = [](PetInfo* pPetInfo, TYPE_OBJECTID objectID) { calls.push_back(std::string(pPetInfo ? "pet|" : "no pet|") + N(objectID)); };
	return host;
}();

constexpr int kFrame = 61;

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
		me.vampire = true;
		made.clear();
		zoneCreatures.clear();
		known.id = 1003;
		zoneCreatures[known.id] = &known;
		// Coat 3 dresses a man as frame 61 and a woman as 62.
		g_pItemTable->InitClass(ITEM_CLASS_VAMPIRE_COAT, 4);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_VAMPIRE_COAT, 3).AddonMaleFrameID = kFrame;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_VAMPIRE_COAT, 3).AddonFemaleFrameID = kFrame + 1;
		creatureTypes.Init(CREATURETYPE_INSTALL_TURRET + 1);
		// Every type a vampire here takes is a vampire's, as the loaded
		// table has them, and so is type 0, which a creature has without
		// the creature host (an unloaded row would be a slayer's).
		for (int type : {0, kFrame, kFrame + 1, CREATURETYPE_BAT, CREATURETYPE_WER_WOLF, CREATURETYPE_VAMPIRE_OPERATOR})
			testfw::MutableRow(creatureTypes, type).SetCreatureTribe(CREATURETRIBE_VAMPIRE);
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

struct Vampire
{
	Sex sex = MALE;
	Shape_t shape = SHAPE_NORMAL;
	BYTE competence = 9;
	Color_t batColor = 11;
	int fromFlag = 0;
	BYTE shopOpen = 0;
	const char* sign = "";
	PetInfo* pet = nullptr;
	NicknameInfo* nickname = nullptr;
};

void Receive(ObjectID_t id, const Vampire& vampire = {})
{
	PCVampireInfo3 info;
	info.setObjectID(id);
	info.setName("Vamp");
	info.setX(30);
	info.setY(31);
	info.setDir(5);
	info.setSex(vampire.sex);
	info.setBatColor(vampire.batColor);
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
	GCAddVampire packet;
	packet.setVampireInfo(info);
	EffectInfo* pEffects = new EffectInfo;
	pEffects->addListElement(EFFECTSTATUS_CURSE_PARALYSIS, 20);
	packet.setEffectInfo(pEffects);
	packet.setFromFlag(static_cast<BYTE>(vampire.fromFlag));
	StoreInfo store;
	store.setOpen(vampire.shopOpen);
	store.setSign(vampire.sign);
	packet.setStoreInfo(&store);
	packet.setPetInfo(vampire.pet);
	packet.setNicknameInfo(vampire.nickname);
	GCAddVampireHandler::execute(&packet, nullptr);
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

std::vector<std::string> Made(const std::string& id)
{
	return Then(Then({"new wearing creature", "status|new|" + N(MODIFY_ADVANCEMENT_CLASS_LEVEL) + "|2", "name|new|Vamp", "type|new|61", "man|new",
		"walks|new", "id|" + id, "at|" + id + "|30,31", "server at|" + id + "|30,31", "turns|" + id + "|5", "faces|" + id + "|5",
		"acts|" + id + "|" + N(ACTION_STAND), "body 1|" + id + "|12", "body 2|" + id + "|14", "aura|" + id + "|2"}, Health(id)),
		{"speed|" + id + "|10", "guild|" + id + "|7", "union|" + id + "|8", "bat colour|" + id + "|11", "shop|" + id + "|0"});
}

std::vector<std::string> Known(const std::string& id)
{
	return Then(Then({"status|" + id + "|" + N(MODIFY_ADVANCEMENT_CLASS_LEVEL) + "|2", "type|" + id + "|61", "man|" + id, "walks|" + id,
		"guild|" + id + "|7", "union|" + id + "|8", "moves|" + id + "|30,31", "server at|" + id + "|30,31", "turns|" + id + "|5",
		"faces|" + id + "|5", "acts|" + id + "|" + N(ACTION_STAND), "body 1|" + id + "|12", "body 2|" + id + "|14", "aura|" + id + "|2"},
		Health(id)), {"speed|" + id + "|10", "bat colour|" + id + "|11", "shop|" + id + "|0"});
}

} // namespace

TEST(AddVampireReplies, ANewVampireIsMadeAndAdded)
{
	World world;
	Receive(1005);
	CHECK(calls == Then(Made("1005"), {"zone takes|1005", "effects|1005|1|0"}));
	CHECK(made.size() == 1 && zoneCreatures.count(1005) == 1);
	// Refused, it is dropped, with nothing after.
	calls.clear();
	zoneTakes = false;
	Receive(1006, {MALE, SHAPE_NORMAL, 9, 11, 1});
	CHECK(calls.size() >= 2 && calls[calls.size() - 2] == "zone takes|1006" && calls.back() == "deleted|1006");
}

TEST(AddVampireReplies, AKnownCreatureTakesItInPlace)
{
	World world;
	Receive(1003);
	CHECK(calls == Then(Known("1003"), {"effects|1003|1|0"}));
	CHECK(made.empty());
}

TEST(AddVampireReplies, ItsShapeAndCompetenceDecideItsType)
{
	World world;
	Receive(1005, {FEMALE});
	Receive(1003, {FEMALE});
	CHECK(std::count(calls.begin(), calls.end(), "type|new|62") == 1 && std::count(calls.begin(), calls.end(), "type|1003|62") == 1);
	CHECK(std::count(calls.begin(), calls.end(), "woman|new") == 1 && std::count(calls.begin(), calls.end(), "woman|1003") == 1);
	calls.clear();
	Receive(1006, {MALE, SHAPE_BAT});
	Receive(1003, {MALE, SHAPE_BAT});
	CHECK(std::count(calls.begin(), calls.end(), "flies|new") == 1 && std::count(calls.begin(), calls.end(), "flies|1003") == 1);
	calls.clear();
	Receive(1007, {MALE, SHAPE_NORMAL, 0});
	Receive(1003, {FEMALE, SHAPE_NORMAL, 0});
	CHECK(calls.size() > 30 && calls[3] == "competence|new|0" && calls[4] == "type|new|" + N(CREATURETYPE_VAMPIRE_OPERATOR)
		&& calls[5] == "man|new" && calls[6] == "man|new");
	CHECK(std::count(calls.begin(), calls.end(), "competence|1003|0") == 1
		&& std::count(calls.begin(), calls.end(), "type|1003|" + N(CREATURETYPE_VAMPIRE_OPERATOR)) == 1
		&& std::count(calls.begin(), calls.end(), "woman|1003") == 2);
}

TEST(AddVampireReplies, AWerewolfIsAllCoatAndNoBatColourIsItsOwn)
{
	World world;
	Receive(1005, {MALE, SHAPE_WERWOLF, 9, 0});
	Receive(1003, {MALE, SHAPE_WERWOLF, 9, 0});
	for (const char* id : {"1005", "1003"})
	{
		const std::string who = id;
		CHECK(std::count(calls.begin(), calls.end(), "body 1|" + who + "|14") == 1);
		CHECK(std::count(calls.begin(), calls.end(), "body 2|" + who + "|14") == 1);
		CHECK(std::count(calls.begin(), calls.end(), "bat colour|" + who + "|65535") == 1);
	}
}

TEST(AddVampireReplies, AnOpenShopShowsItsSignToAVampirePlayer)
{
	World world;
	Receive(1005, {MALE, SHAPE_NORMAL, 9, 11, 0, 1, "Cheap blood"});
	Receive(1003, {MALE, SHAPE_NORMAL, 9, 11, 0, 1, ""});
	CHECK(std::count(calls.begin(), calls.end(), "shop|1005|1") == 1 && std::count(calls.begin(), calls.end(), "sign|1005|Cheap blood|4660") == 1);
	CHECK(std::count(calls.begin(), calls.end(), "shop|1003|1") == 1 && std::count(calls.begin(), calls.end(), "sign|1003|Welcome|4660") == 1);
	// A new one without a sign shows the default message too.
	calls.clear();
	Receive(1004, {MALE, SHAPE_NORMAL, 9, 11, 0, 1, ""});
	CHECK(std::count(calls.begin(), calls.end(), "sign|1004|Welcome|4660") == 1);
	calls.clear();
	Receive(1003, {MALE, SHAPE_NORMAL, 9, 11, 0, 1, "Cheap blood"});
	CHECK(std::count(calls.begin(), calls.end(), "sign|1003|Cheap blood|4660") == 1);
	// The default message is passed as the table holds it, NULL when it is
	// empty (MCreature::SetPersnalString ignores that).
	calls.clear();
	g_pGameStringTable->Set(UI_STRING_MESSAGE_PERSNAL_DEFAULT_MESSGE, "");
	Receive(1003, {MALE, SHAPE_NORMAL, 9, 11, 0, 1, ""});
	Receive(1010, {MALE, SHAPE_NORMAL, 9, 11, 0, 1, ""});
	CHECK(std::count(calls.begin(), calls.end(), "sign|1003|(null)|4660") == 1 && std::count(calls.begin(), calls.end(), "sign|1010|(null)|4660") == 1);
	// No sign for a player who is no vampire, or for no player, new or
	// known; nothing at all for an NPC's type.
	calls.clear();
	me.vampire = false;
	Receive(1003, {MALE, SHAPE_NORMAL, 9, 11, 0, 1, "Cheap blood"});
	Receive(1009, {MALE, SHAPE_NORMAL, 9, 11, 0, 1, "Cheap blood"});
	me.vampire = true;
	hasPlayer = false;
	Receive(1006, {MALE, SHAPE_NORMAL, 9, 11, 0, 1, "Cheap blood"});
	Receive(1003, {MALE, SHAPE_NORMAL, 9, 11, 0, 1, "Cheap blood"});
	CHECK(std::none_of(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("sign|", 0) == 0; }));
	CHECK(std::count(calls.begin(), calls.end(), "shop|1003|1") == 2 && std::count(calls.begin(), calls.end(), "shop|1006|1") == 1
		&& std::count(calls.begin(), calls.end(), "shop|1009|1") == 1);
	calls.clear();
	hasPlayer = true;
	testfw::MutableRow(world.creatureTypes, kFrame).SetCreatureTribe(CREATURETRIBE_NPC);
	Receive(1007, {MALE, SHAPE_NORMAL, 9, 11, 0, 1, "Cheap blood"});
	Receive(1003, {MALE, SHAPE_NORMAL, 9, 11, 0, 1, "Cheap blood"});
	CHECK(std::none_of(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("sign|", 0) == 0 || call.rfind("shop|", 0) == 0; }));
}

TEST(AddVampireReplies, ItComesThroughAPortalWithItsPetAndNickname)
{
	World world;
	PetInfo pet;
	NicknameInfo custom;
	custom.setNicknameType(NicknameInfo::NICK_CUSTOM);
	custom.setNickname(std::string("Count"));
	custom.setNicknameIndex(1);
	Receive(1005, {MALE, SHAPE_NORMAL, 9, 11, 1, 0, "", &pet, &custom});
	CHECK(calls == Then(Made("1005"), {"zone takes|1005", "effects|1005|1|0", "from portal|1005", "pet|1005",
		"nickname|1005|" + N(NicknameInfo::NICK_CUSTOM) + "|Count"}));
	// A forced custom one is sent as it is; any other is read from the
	// table by its index, the first past the table.
	calls.clear();
	NicknameInfo forced, built, past;
	forced.setNicknameType(NicknameInfo::NICK_CUSTOM_FORCED);
	forced.setNickname(std::string("Lord"));
	built.setNicknameType(NicknameInfo::NICK_BUILT_IN);
	built.setNicknameIndex(1);
	past.setNicknameType(NicknameInfo::NICK_QUEST);
	past.setNicknameIndex(2);
	Receive(1003, {MALE, SHAPE_NORMAL, 9, 11, 0, 0, "", nullptr, &forced});
	Receive(1003, {MALE, SHAPE_NORMAL, 9, 11, 0, 0, "", nullptr, &built});
	Receive(1003, {MALE, SHAPE_NORMAL, 9, 11, 2, 0, "", nullptr, &past});
	CHECK(std::count(calls.begin(), calls.end(), "nickname|1003|" + N(NicknameInfo::NICK_CUSTOM_FORCED) + "|Lord") == 1);
	CHECK(std::count(calls.begin(), calls.end(), "nickname|1003|" + N(NicknameInfo::NICK_BUILT_IN) + "|Night Walker") == 1);
	CHECK(std::count(calls.begin(), calls.end(), "nickname|1003|" + N(NicknameInfo::NICK_QUEST) + "|Novice") == 1);
	// Only a flag of 1 is a portal.
	CHECK(std::none_of(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("from portal|", 0) == 0 || call.rfind("pet|", 0) == 0; }));
}

TEST(AddVampireReplies, WithoutAZoneOrTheHostsNothingHappens)
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
	CHECK(std::count_if(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("effects|", 0) == 0; }) == 2);
	// Without the creature host, a made one is still added and takes its
	// effects.
	calls.clear();
	CreatureState::SetHost(nullptr);
	Receive(1008);
	CHECK(calls == std::vector<std::string>({"new wearing creature", "zone takes|new", "effects|new|1|0"}));
	CreatureState::SetMasterEffectType(Handle(&world.known), 1);
	CreatureState::SetUnionGuildID(Handle(&world.known), 1);
	calls.clear();
	ZoneState::SetHost(nullptr);
	Receive(1005);
	CHECK(calls.empty());
}
