#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "BonusSkillHost.h"
#include "CMessageArray.h"
#include "CServerInformation.h"
#include "ClientMode.h"
#include "ClientPlayer.h"
#include "FrameClock.h"
#include "GameInfoPresentationHost.h"
#include "GameMessages.h"
#include "MCreatureTable.h"
#include "MEffectStatusDef.h"
#include "MGameTime.h"
#include "MGuildInfoMapper.h"
#include "MQuickSlot.h"
#include "MSkillManager.h"
#include "MStorage.h"
#include "MWeather.h"
#include "MZoneTable.h"
#include "MorphState.h"
#include "PCOustersInfo2.h"
#include "PCSlayerInfo2.h"
#include "PCVampireInfo2.h"
#include "PlayerStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "TalkBox.h"
#include "WorldEventsHost.h"
#include "ZoneFlags.h"
#include "ZoneLoadingHost.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCUpdateInfo.h"
#include <algorithm>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

extern DWORD g_PreviousCreatureType;

// The game server's first word on a zone: the player's whole state. The
// zone's events are dropped, the mode waits for this update, the server's
// status is shown, every addon sprite loads and sound mutes. The player is
// made anew (the old one, if any, leaves the zone, and its special action
// is kept), its addons removed and its basic attack melee; the skill table
// and tree are reset, the previous creature type is the last transform
// the effects carry, and the guilds' names are forgotten. The PC info by
// race is stored, the interface changes race if it must, and the SMS
// charge, nickname, non-PK flag, union and Powerjjang points follow. The
// stash, the windows, the talk box and the quick slots are cleared. The
// progress bar runs while the view starts, the weather (cleared when rain
// or snow stops), darkness and light (a master's are fixed), the game time,
// gear, inventory, the item at the mouse, the motorcycle, the skills, the
// zone's creature sprites (only a large zone's moves may release sprites),
// the position and the zone (moved to from the game, loaded otherwise),
// the connection's encryption and the NPCs' names and places. The frame
// count catches up the time this took, the bat colour is set, the effects
// apply, sound returns, the HP is shown, the messages clear, and the
// player waits for its position.
namespace {

std::vector<std::string> calls;
int oldMe, newMe;
MCreature* pPlayerNow = nullptr;
bool zoneExists = true;
int zoneID = 21;
Race interfaceRace = RACE_SLAYER;
bool preloadsMonsters = false;
bool hasLargeZone = false;
const GCUpdateInfo* pReceiving = nullptr;
constexpr int kSpecialAction = 33;
constexpr std::uint32_t kFrames = 5;

std::string N(long long value) { return std::to_string(value); }
std::string Who(MCreature* p)
{
	return p == reinterpret_cast<MCreature*>(&oldMe) ? "old" : p == reinterpret_cast<MCreature*>(&newMe) ? "new" : "other";
}
std::string Same(const void* p, const void* expected) { return p == expected ? "packet's" : "other"; }
std::string Types(const COrderedList<int>& types)
{
	std::string text;
	auto it = types.GetIterator();
	for (int i = 0; i < types.GetSize(); i++, it++) text += (i ? "," : "") + N(*it);
	return text;
}

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return pPlayerNow; };
	host.GetID = []() { return TYPE_OBJECTID(7); };
	host.GetSpecialActionInfo = []() { return kSpecialAction; };
	host.Recreate = []() { calls.push_back("recreate"); pPlayerNow = reinterpret_cast<MCreature*>(&newMe); };
	host.SetBasicAttack = [](TYPE_ACTIONINFO actionInfo, int distance) { calls.push_back("basic attack|" + N(actionInfo) + "|" + N(distance)); };
	host.SetPCSlayerInfo = [](PCSlayerInfo2* p) { calls.push_back("slayer info|" + Same(p, pReceiving->getPCInfo())); };
	host.SetPCVampireInfo = [](PCVampireInfo2* p) { calls.push_back("vampire info|" + Same(p, pReceiving->getPCInfo())); };
	host.SetPCOustersInfo = [](PCOustersInfo2* p) { calls.push_back("ousters info|" + Same(p, pReceiving->getPCInfo())); };
	host.SetSpecialActionInfo = [](int actionInfo) { calls.push_back("special action|" + N(actionInfo)); };
	host.SetAttackModeNormal = []() { calls.push_back("attack mode normal"); };
	host.SetTimeLightSight = [](int sight) { calls.push_back("light|" + N(sight)); };
	host.SetGearInfo = [](GearInfo* pGear, BloodBibleSignInfo* pBible) {
		calls.push_back("gear|" + Same(pGear, pReceiving->getGearInfo()) + "|"
			+ Same(pBible, const_cast<GCUpdateInfo*>(pReceiving)->getBloodBibleSignInfo()));
	};
	host.CalculateStatus = []() { calls.push_back("calculate"); };
	host.SetInventoryInfo = [](InventoryInfo* p) { calls.push_back("inventory|" + Same(p, pReceiving->getInventoryInfo())); };
	host.SetExtraInfo = [](ExtraInfo* p) { calls.push_back("extra|" + Same(p, pReceiving->getExtraInfo())); };
	host.SetAddonItem = [](MItem* pItem) {
		std::string options;
		for (TYPE_ITEM_OPTION option : pItem->GetItemOptionList()) options += "," + N(option);
		calls.push_back("ride|" + N(pItem->GetItemClass()) + "|" + N(pItem->GetID()) + "|" + N(pItem->GetItemType()) + options);
		return true;
	};
	host.SetXY = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { calls.push_back("xy|" + N(x) + "|" + N(y)); };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetCreatureType = [](MCreature*) { return 2; };
	host.RemoveAddon = [](MCreature* p, int addon) { calls.push_back("addon|" + Who(p) + "|" + N(addon)); return true; };
	host.SetNickName = [](MCreature* p, std::uint8_t type, char* name) { calls.push_back("nick|" + Who(p) + "|" + N(type) + "|" + name); };
	host.SetUnionGuildID = [](MCreature* p, int unionID) { calls.push_back("union|" + Who(p) + "|" + N(unionID)); };
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetID = []() { return zoneID; };
	host.RemoveCreature = [](TYPE_OBJECTID id) { calls.push_back("zone remove|" + N(id)); return true; };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.MuteSound = [](bool bMute) { calls.push_back(std::string("mute|") + (bMute ? "on" : "off")); };
	host.SetWeather = [](int weather, int level) { calls.push_back("weather|" + N(weather) + "|" + N(level)); };
	host.SetDarkBits = [](int darkBits) { calls.push_back("dark|" + N(darkBits)); };
	host.SetEffectInfo = [](MCreature* p, EffectInfo* pInfo, int delayedFrame) {
		calls.push_back("effects|" + Who(p) + "|" + Same(pInfo, pReceiving->getEffectInfo()) + "|" + N(pInfo->getListNum()) + "|" + N(delayedFrame));
	};
	return host;
}();

const BonusSkills::Host skills = []() {
	BonusSkills::Host host{};
	host.RefreshAvailableSkills = []() { calls.push_back("skills refreshed"); };
	return host;
}();

const GameInfoPresentation::Host info = []() {
	GameInfoPresentation::Host host{};
	host.SetNPCInfo = [](const char* pName, int npcID, int x, int y) {
		calls.push_back(std::string("npc|") + (pName ? pName : "(null)") + "|" + N(npcID) + "|" + N(x) + "|" + N(y));
	};
	return host;
}();

const ZoneLoading::Host loading = []() {
	ZoneLoading::Host host{};
	host.RemoveZoneEvents = []() { calls.push_back("zone events removed|mode " + N(g_Mode)); };
	host.LoadAllAddonSprites = []() { calls.push_back("addon sprites|mode " + N(g_Mode)); };
	host.InterfaceRace = []() { return interfaceRace; };
	host.ChangeInterfaceRace = [](Race race) { calls.push_back("interface|" + N(race)); };
	host.InitSkillTree = []() {
		calls.push_back("skill tree|previous " + N(static_cast<int>(g_PreviousCreatureType)) + "|morph " + N(g_MorphCreatureType));
	};
	host.SetSMSCharge = [](int charge) { calls.push_back("sms|" + N(charge)); };
	host.SetPowerjjangPoint = [](int point) { calls.push_back("powerjjang|" + N(point)); };
	host.CloseWindows = []() { calls.push_back("windows closed"); };
	host.StartProgress = [](int id) { calls.push_back("progress start|" + N(id)); };
	host.DrawProgress = [](int percent) { calls.push_back("progress|" + N(percent)); };
	host.EndProgress = []() { calls.push_back("progress end"); };
	host.InitTopView = []() { calls.push_back("top view"); };
	host.PreloadsMonsters = []() { return preloadsMonsters; };
	host.LoadCreature = [](int spriteType) { calls.push_back("load creature|" + N(spriteType)); };
	host.HasLargeZone = []() { return hasLargeZone; };
	host.AddMonsterSpriteTypes = [](const COrderedList<int>& types) { calls.push_back("sprite types|" + Types(types)); };
	host.ReleaseCreatureSpritesExcept = [](const COrderedList<int>& types) { calls.push_back("release sprites except|" + Types(types)); };
	host.MoveZone = [](int id) { calls.push_back("move zone|" + N(id)); };
	host.LoadZone = [](int id) { calls.push_back("load zone|" + N(id)); };
	host.SetEncryptCode = []() { calls.push_back("encrypt"); };
	host.StampFrameClock = []() { calls.push_back("stamp clock"); };
	host.MillisecondsToFrames = [](std::uint32_t) { calls.push_back("to frames"); return kFrames; };
	host.AdvanceFrame = [](int frames) { calls.push_back("advance|" + N(frames)); };
	host.SetMyBatColorSet = [](std::uint16_t colorSet) { calls.push_back("bat colour|" + N(colorSet)); };
	host.ShowPlayerHP = []() { calls.push_back("hp shown"); };
	host.EnterWaitSetPositionMode = []() { calls.push_back("wait set position"); };
	return host;
}();

const MWeatherHost weatherHost = {
	.ReadOrigin = [](int& x, int& y) { x = 0; y = 0; return true; },
	.Width = []() { return 800; },
	.Height = []() { return 600; },
};

// The game connection.
struct GamePlayer : ClientPlayer
{
	GamePlayer() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~GamePlayer() { setPlayerStatus(CPS_END_SESSION); }
};

struct NPC
{
	std::string name;
	NPCID_t id;
	ZoneCoord_t x, y;
};

// What an update carries, written as the server writes it.
struct Update
{
	char pcType = 'S';
	Color_t batColor = 0;
	std::vector<std::pair<EffectID_t, WORD>> effects;
	bool motorcycle = false;
	ObjectID_t motorID = 0;
	ItemType_t motorType = 0;
	std::list<OptionType_t> motorOptions;
	ZoneID_t zoneID = 21;
	Coord_t x = 30, y = 40;
	Weather weather = WEATHER_CLEAR;
	WeatherLevel_t weatherLevel = 2;
	DarkLevel_t dark = 3;
	LightLevel_t light = 7;
	std::vector<NPCType_t> npcTypes;
	std::vector<MonsterType_t> monsterTypes;
	std::vector<NPC> npcs;
	BYTE serverStat = 2;
	DWORD sms = 11;
	BYTE nickType = NicknameInfo::NICK_NONE;
	std::string nickname;
	WORD nickIndex = 0;
	BYTE nonPK = 1;
	uint unionID = 44;
	BYTE unionType = 3;
	uint powerjjang = 55;
};

std::unique_ptr<GCUpdateInfo> Build(const Update& u)
{
	Socket outSocket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream out(&outSocket);
	out.write(u.pcType);
	// A PC's record needs a name to be read, and its enums a value to be
	// written.
	if (u.pcType == 'S') { PCSlayerInfo2 pc; pc.setName("Kim"); pc.setSex(MALE); pc.setHairStyle(HAIR_STYLE1); pc.write(out); }
	else if (u.pcType == 'V') { PCVampireInfo2 pc; pc.setName("Kim"); pc.setSex(MALE); pc.setBatColor(u.batColor); pc.write(out); }
	else { PCOustersInfo2 pc; pc.setName("Kim"); pc.setSex(FEMALE); pc.write(out); }
	InventoryInfo inventory; inventory.write(out);
	GearInfo gear; gear.write(out);
	ExtraInfo extra; extra.write(out);
	EffectInfo effects;
	for (const auto& [id, value] : u.effects) effects.addListElement(id, value);
	effects.write(out);
	out.write(u.motorcycle);
	if (u.motorcycle)
	{
		RideMotorcycleInfo ride;
		ride.setObjectID(u.motorID);
		ride.setItemType(u.motorType);
		ride.setOptionType(u.motorOptions);
		ride.addListElement(new RideMotorcycleSlotInfo);
		ride.setListNum(1);
		ride.write(out);
	}
	out.write(u.zoneID);
	out.write(u.x);
	out.write(u.y);
	GameTime time;
	time.setYear(2026); time.setMonth(10); time.setDay(14); time.setHour(12); time.setMinute(30); time.setSecond(15);
	time.write(out);
	out.write(static_cast<BYTE>(u.weather));
	out.write(u.weatherLevel);
	out.write(u.dark);
	out.write(u.light);
	out.write(static_cast<BYTE>(u.npcTypes.size()));
	for (NPCType_t type : u.npcTypes) out.write(type);
	out.write(static_cast<BYTE>(u.monsterTypes.size()));
	for (MonsterType_t type : u.monsterTypes) out.write(type);
	out.write(static_cast<BYTE>(u.npcs.size()));
	for (const NPC& npc : u.npcs)
	{
		NPCInfo record;
		record.setName(npc.name);
		record.setNPCID(npc.id);
		record.setX(npc.x);
		record.setY(npc.y);
		record.write(out);
	}
	out.write(u.serverStat);
	out.write(static_cast<BYTE>(0));
	out.write(u.sms);
	NicknameInfo nick;
	nick.setNicknameType(u.nickType);
	nick.setNickname(u.nickname);
	nick.setNicknameIndex(u.nickIndex);
	nick.write(out);
	out.write(u.nonPK);
	out.write(u.unionID);
	out.write(u.unionType);
	// The Blood Bible's signs, none open: BloodBibleSignInfo's constructor
	// leaves its list size unset, so its write() cannot be used here.
	out.write(static_cast<uint>(0));
	out.write(static_cast<BYTE>(0));
	out.write(u.powerjjang);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(out);
	Socket inSocket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream in(&inSocket);
	CHECK(SocketInputStreamTestAccess::Preload(in, bytes.data(), static_cast<unsigned int>(bytes.size())));
	std::unique_ptr<GCUpdateInfo> pPacket(new GCUpdateInfo);
	pPacket->read(in);
	CHECK(in.isEmpty());
	return pPacket;
}

struct World : GameModelWorld
{
	GamePlayer connection;
	CServerInformation server;
	MSkillInfoTable skillTable;
	MGuildInfoMapper guilds;
	MStringArray nicknames;
	PCTalkBox talkBox;
	MWeather weather;
	MGameTime gameTime;
	CZoneTable zones;
	CREATURE_TABLE creatureTypes;
	CMessageArray systemLog, gameLog, noticeLog;
	MBelt belt;
	CServerInformation* oldServer = g_pServerInformation;
	MSkillInfoTable* oldSkillTable = g_pSkillInfoTable;
	MGuildInfoMapper* oldGuilds = g_pGuildInfoMapper;
	MStringArray* oldNicknames = g_pNickNameStringTable;
	PCTalkBox* oldTalkBox = g_pPCTalkBox;
	MWeather* oldWeather = g_pWeather;
	MGameTime* oldGameTime = g_pGameTime;
	CZoneTable* oldZones = g_pZoneTable;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	CMessageArray* oldSystem = g_pSystemMessage;
	CMessageArray* oldGame = g_pGameMessage;
	CMessageArray* oldNotice = g_pNoticeMessage;
	MStorage* oldStorage = g_pStorage;
	MBelt* oldQuickSlot = g_pQuickSlot;
	CLIENT_MODE oldMode = g_Mode;
	int oldMorph = g_MorphCreatureType;
	bool oldInLarge = g_bZonePlayerInLarge;
	int oldLarge = g_nZoneLarge;
	const MWeatherHost* oldWeatherHost = MWeather::SetHost(&weatherHost);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	const BonusSkills::Host* oldSkills = BonusSkills::SetHost(&skills);
	const GameInfoPresentation::Host* oldInfo = GameInfoPresentation::SetHost(&info);
	const ZoneLoading::Host* oldLoading = ZoneLoading::SetHost(&loading);
	World()
	{
		calls.clear();
		pPlayerNow = reinterpret_cast<MCreature*>(&oldMe);
		zoneExists = true;
		zoneID = 21;
		interfaceRace = RACE_SLAYER;
		preloadsMonsters = false;
		hasLargeZone = false;
		g_Mode = MODE_NULL;
		g_MorphCreatureType = 0;
		g_bZonePlayerInLarge = false;
		g_nZoneLarge = ZONEID_NULL;
		g_pServerInformation = &server;
		g_pSkillInfoTable = &skillTable;
		g_pGuildInfoMapper = &guilds;
		nicknames.Init(3);
		nicknames.Set(1, "the Brave");
		g_pNickNameStringTable = &nicknames;
		g_pPCTalkBox = &talkBox;
		g_pWeather = &weather;
		g_pGameTime = &gameTime;
		ZONETABLE_INFO* pSmall = new ZONETABLE_INFO;
		pSmall->ID = 21;
		pSmall->Property = FLAG_ZONESIZE_SMALL;
		CHECK(zones.Add(pSmall));
		ZONETABLE_INFO* pLarge = new ZONETABLE_INFO;
		pLarge->ID = 22;
		pLarge->Property = 0;
		CHECK(zones.Add(pLarge));
		g_pZoneTable = &zones;
		creatureTypes.Init(10);
		testfw::MutableRow(creatureTypes, 3).Name = "Table Smith";
		g_pCreatureTable = &creatureTypes;
		for (CMessageArray* pLog : {&systemLog, &gameLog, &noticeLog})
		{
			pLog->Init(3, 64);
			pLog->Add("stale");
		}
		g_pSystemMessage = &systemLog;
		g_pGameMessage = &gameLog;
		g_pNoticeMessage = &noticeLog;
		g_pStorage = nullptr;
		g_pQuickSlot = &belt;
		g_pArmsBand1 = nullptr;
		g_pArmsBand2 = nullptr;
	}
	~World()
	{
		ZoneLoading::SetHost(oldLoading);
		GameInfoPresentation::SetHost(oldInfo);
		BonusSkills::SetHost(oldSkills);
		WorldEvents::SetHost(oldEvents);
		ZoneState::SetHost(oldZone);
		CreatureState::SetHost(oldCreature);
		PlayerState::SetHost(oldPlayer);
		MWeather::SetHost(oldWeatherHost);
		delete g_pStorage;
		g_pStorage = oldStorage;
		g_pQuickSlot = oldQuickSlot;
		g_nZoneLarge = oldLarge;
		g_bZonePlayerInLarge = oldInLarge;
		g_MorphCreatureType = oldMorph;
		g_Mode = oldMode;
		g_pNoticeMessage = oldNotice;
		g_pGameMessage = oldGame;
		g_pSystemMessage = oldSystem;
		g_pCreatureTable = oldCreatureTypes;
		g_pZoneTable = oldZones;
		g_pGameTime = oldGameTime;
		g_pWeather = oldWeather;
		g_pPCTalkBox = oldTalkBox;
		g_pNickNameStringTable = oldNicknames;
		g_pGuildInfoMapper = oldGuilds;
		g_pSkillInfoTable = oldSkillTable;
		g_pServerInformation = oldServer;
		pPlayerNow = nullptr;
	}
	void Receive(const Update& update)
	{
		std::unique_ptr<GCUpdateInfo> pPacket = Build(update);
		pReceiving = pPacket.get();
		GCUpdateInfoHandler::execute(pPacket.get(), &connection);
		pReceiving = nullptr;
	}
	static std::vector<std::string> Lines(CMessageArray& log)
	{
		std::vector<std::string> lines;
		for (int i = 0; i < log.GetSize(); i++)
			if (log[i] != nullptr && log[i][0] != '\0') lines.push_back(log[i]);
		return lines;
	}
};

bool Has(const std::string& call) { return std::find(calls.begin(), calls.end(), call) != calls.end(); }
std::ptrdiff_t At(const std::string& call) { return std::find(calls.begin(), calls.end(), call) - calls.begin(); }
std::vector<std::string> Starting(const std::string& prefix)
{
	std::vector<std::string> found;
	for (const std::string& call : calls)
		if (call.rfind(prefix, 0) == 0) found.push_back(call);
	return found;
}

} // namespace

TEST(UpdateInfoReplies, ASlayersUpdateRunsInOrder)
{
	World world;
	world.talkBox.AddString("stale");
	world.Receive(Update{});
	std::vector<std::string> expected = {
		"zone events removed|mode " + N(MODE_NULL), "addon sprites|mode " + N(MODE_WAIT_UPDATEINFO), "mute|on",
		"zone remove|7", "recreate"};
	for (int addon = 0; addon < ADDON_MAX; addon++) expected.push_back("addon|new|" + N(addon));
	const std::vector<std::string> rest = {
		"basic attack|" + N(SKILL_ATTACK_MELEE) + "|1", "skill tree|previous 2|morph 0", "slayer info|packet's", "sms|11",
		"nick|new|" + N(NicknameInfo::NICK_NONE) + "|", "union|new|44", "powerjjang|55", "special action|" + N(kSpecialAction),
		"attack mode normal", "windows closed", "progress start|21", "progress|0", "top view", "progress|15", "weather|" + N(WEATHER_CLEAR) + "|2",
		"dark|3", "light|7", "gear|packet's|packet's", "calculate", "inventory|packet's", "extra|packet's", "progress|17", "skills refreshed",
		"sprite types|", "progress|70", "xy|30|40", "load zone|21", "encrypt", "stamp clock", "to frames", "advance|5", "bat colour|65535",
		"effects|new|packet's|0|5", "progress|100", "progress|100", "progress end", "mute|off", "hp shown", "wait set position"};
	expected.insert(expected.end(), rest.begin(), rest.end());
	CHECK(calls == expected);
	CHECK(world.connection.getPlayerStatus() == CPS_WAITING_FOR_LOADING);
	CHECK(g_Mode == MODE_WAIT_UPDATEINFO);
	CHECK(world.server.GetServerGroupStatus() == 2);
	CHECK(g_pUserInformation->IsNonPK && g_pUserInformation->dwUnionID == 44 && g_pUserInformation->bUnionGrade == 3);
	CHECK(g_PreviousCreatureType == static_cast<DWORD>(-1));
	// Quick slots, talk box and messages are cleared.
	CHECK(g_pQuickSlot == nullptr && g_pArmsBand1 == nullptr && g_pArmsBand2 == nullptr);
	CHECK(world.talkBox.empty());
	CHECK(World::Lines(world.systemLog).empty() && World::Lines(world.gameLog).empty() && World::Lines(world.noticeLog).empty());
}

TEST(UpdateInfoReplies, WithoutAPlayerNoneLeavesTheZoneAndNoSpecialActionIsKept)
{
	World world;
	pPlayerNow = nullptr;
	world.Receive(Update{});
	CHECK(!Has("zone remove|7"));
	CHECK(Has("recreate") && Has("special action|" + N(ACTIONINFO_NULL)));
	CHECK(Has("skill tree|previous -1|morph 0"));
}

TEST(UpdateInfoReplies, AMorphsCreatureTypeIsForgotten)
{
	World world;
	g_MorphCreatureType = CREATURETYPE_WOLF;
	world.Receive(Update{});
	// The previous type is the morph's (not the player's) as the skill
	// tree is reset, and none after.
	CHECK(Has("skill tree|previous " + N(CREATURETYPE_WOLF) + "|morph 0"));
	CHECK(g_MorphCreatureType == 0 && g_PreviousCreatureType == static_cast<DWORD>(-1));
}

TEST(UpdateInfoReplies, TheLastTransformIsThePreviousCreatureType)
{
	World world;
	Update update;
	update.effects = {{EFFECTSTATUS_TRANSFORM_TO_WOLF, 1}, {EFFECTSTATUS_TRANSFORM_TO_BAT, 1}, {EFFECTSTATUS_TRANSFORM_TO_WERWOLF, 1}};
	world.Receive(update);
	CHECK(g_PreviousCreatureType == CREATURETYPE_WER_WOLF);
	// The packet's own effects reach the player whole.
	CHECK(Has("effects|new|packet's|3|5"));
	for (EffectID_t last : {EffectID_t(EFFECTSTATUS_TRANSFORM_TO_WOLF), EffectID_t(EFFECTSTATUS_TRANSFORM_TO_BAT)})
	{
		calls.clear();
		update.effects = {{EFFECTSTATUS_TRANSFORM_TO_WERWOLF, 1}, {last, 9}, {EFFECTSTATUS_COMA, 1}};
		world.Receive(update);
		CHECK(g_PreviousCreatureType == (last == EFFECTSTATUS_TRANSFORM_TO_WOLF ? DWORD(CREATURETYPE_WOLF) : DWORD(CREATURETYPE_BAT)));
	}
}

TEST(UpdateInfoReplies, TheSkillTableAndTheGuildNamesAreReset)
{
	World world;
	testfw::MutableRow(world.skillTable, SKILL_ATTACK_MELEE).SetSkillExp(500);
	std::string name = "Wolves";
	world.guilds.SetGuildName(5, name);
	world.Receive(Update{});
	CHECK(world.skillTable[SKILL_ATTACK_MELEE].GetSkillExp() == 0);
	CHECK(!world.guilds.IsExistGuildName(5));
}

TEST(UpdateInfoReplies, TheRaceSetsItsInfoAndTheInterface)
{
	World world;
	const std::pair<char, std::pair<std::string, Race>> races[] = {
		{'S', {"slayer info|packet's", RACE_SLAYER}}, {'V', {"vampire info|packet's", RACE_VAMPIRE}}, {'O', {"ousters info|packet's", RACE_OUSTERS}}};
	for (const auto& [type, expected] : races)
	{
		for (Race shown : {RACE_SLAYER, RACE_VAMPIRE, RACE_OUSTERS})
		{
			calls.clear();
			interfaceRace = shown;
			Update update;
			update.pcType = type;
			world.Receive(update);
			CHECK(Has(expected.first));
			CHECK(Starting("interface|") == (shown == expected.second ? std::vector<std::string>{} : std::vector<std::string>{"interface|" + N(expected.second)}));
			CHECK(At(expected.first) < At("sms|11"));
		}
	}
}

TEST(UpdateInfoReplies, AVampiresBatColourFollowsTheDefault)
{
	World world;
	Update update;
	update.pcType = 'V';
	update.batColor = 77;
	world.Receive(update);
	CHECK(Starting("bat colour|") == (std::vector<std::string>{"bat colour|65535", "bat colour|77"}));
	calls.clear();
	update.batColor = 0;
	world.Receive(update);
	CHECK(Starting("bat colour|") == std::vector<std::string>{"bat colour|65535"});
}

TEST(UpdateInfoReplies, TheNicknameIsTheStringTheIndexedOneOrNone)
{
	World world;
	const std::pair<Update, std::string> cases[] = {
		{[] { Update u; u.nickType = NicknameInfo::NICK_CUSTOM; u.nickname = "Ace"; return u; }(), "nick|new|" + N(NicknameInfo::NICK_CUSTOM) + "|Ace"},
		{[] { Update u; u.nickType = NicknameInfo::NICK_CUSTOM_FORCED; u.nickname = "Forced"; return u; }(), "nick|new|" + N(NicknameInfo::NICK_CUSTOM_FORCED) + "|Forced"},
		{[] { Update u; u.nickType = NicknameInfo::NICK_BUILT_IN; u.nickIndex = 1; return u; }(), "nick|new|" + N(NicknameInfo::NICK_BUILT_IN) + "|the Brave"},
		// Past the table: the first entry; an empty entry: no text.
		{[] { Update u; u.nickType = NicknameInfo::NICK_QUEST; u.nickIndex = 9; return u; }(), "nick|new|" + N(NicknameInfo::NICK_QUEST) + "|"},
		{[] { Update u; u.nickType = NicknameInfo::NICK_FORCED; u.nickIndex = 2; return u; }(), "nick|new|" + N(NicknameInfo::NICK_FORCED) + "|"}};
	for (const auto& [update, call] : cases)
	{
		calls.clear();
		world.Receive(update);
		CHECK(Starting("nick|") == std::vector<std::string>{call});
	}
	world.nicknames.Set(0, "Rookie");
	calls.clear();
	Update past;
	past.nickType = NicknameInfo::NICK_QUEST;
	past.nickIndex = 3;
	world.Receive(past);
	CHECK(Starting("nick|") == std::vector<std::string>{"nick|new|" + N(NicknameInfo::NICK_QUEST) + "|Rookie"});
}

TEST(UpdateInfoReplies, TheStashIsDropped)
{
	World world;
	g_pStorage = new MStorage;
	world.Receive(Update{});
	CHECK(g_pStorage == nullptr);
}

TEST(UpdateInfoReplies, RainOrSnowStopsUnlessItGoesOn)
{
	World world;
	const struct { BYTE now; Weather next; bool cleared; } cases[] = {
		{MWeather::WEATHER_SNOW, WEATHER_CLEAR, true}, {MWeather::WEATHER_SNOW, WEATHER_RAINY, true}, {MWeather::WEATHER_SNOW, WEATHER_SNOWY, false},
		{MWeather::WEATHER_RAIN, WEATHER_CLEAR, true}, {MWeather::WEATHER_RAIN, WEATHER_SNOWY, true}, {MWeather::WEATHER_RAIN, WEATHER_RAINY, false}};
	for (const auto& c : cases)
	{
		if (c.now == MWeather::WEATHER_SNOW) world.weather.SetSnow(5);
		else world.weather.SetRain(5);
		calls.clear();
		Update update;
		update.weather = c.next;
		world.Receive(update);
		CHECK(world.weather.GetWeatherType() == (c.cleared ? static_cast<BYTE>(MWeather::WEATHER_NULL) : c.now));
		CHECK(Has("weather|" + N(c.next) + "|2"));
	}
}

TEST(UpdateInfoReplies, AMastersDarknessAndLightAreFixed)
{
	World world;
	g_pUserInformation->IsMaster = true;
	world.Receive(Update{});
	CHECK(Has("dark|0") && Has("light|13"));
}

TEST(UpdateInfoReplies, TheGameTimeStartsFromThePacketAtItsRatio)
{
	World world;
	g_FrameNow = MonotonicClock::TimePoint(MonotonicClock::Duration(1000000));
	world.Receive(Update{});
	world.gameTime.SetCurrentTime(g_FrameNow + MonotonicClock::Duration(1000));
	CHECK_EQ(2026, world.gameTime.GetYear());
	CHECK_EQ(10, world.gameTime.GetMonth());
	CHECK_EQ(14, world.gameTime.GetDay());
	CHECK_EQ(12, world.gameTime.GetHour());
	CHECK_EQ(30, world.gameTime.GetMinute());
	// a second later, at 24 to 1: 24 game seconds.
	CHECK_EQ(39, world.gameTime.GetSecond());
}

TEST(UpdateInfoReplies, TheMotorcycleIsRiddenWithItsRecord)
{
	World world;
	Update update;
	update.motorcycle = true;
	update.motorID = 901;
	update.motorType = 2;
	update.motorOptions = {4, 6};
	world.Receive(update);
	CHECK(Has("ride|" + N(ITEM_CLASS_MOTORCYCLE) + "|901|2,4,6"));
	CHECK(At("extra|packet's") < At("ride|" + N(ITEM_CLASS_MOTORCYCLE) + "|901|2,4,6")
		&& At("ride|" + N(ITEM_CLASS_MOTORCYCLE) + "|901|2,4,6") < At("progress|17"));
}

TEST(UpdateInfoReplies, TheZonesCreaturesFillTheProgressAndTheSprites)
{
	World world;
	Update update;
	update.npcTypes = {9, 3};
	update.monsterTypes = {5};
	world.Receive(update);
	// 30 shared by three: 50, 60, 70.
	CHECK(Starting("progress|") == (std::vector<std::string>{"progress|0", "progress|15", "progress|17", "progress|50", "progress|60",
		"progress|70", "progress|100", "progress|100"}));
	CHECK(Has("sprite types|3,5,9"));
	CHECK(Starting("load creature|").empty());
	// Monsters (not NPCs) load up front when the option says so; four
	// creatures: 47, 54, 61, 68, then 70.
	calls.clear();
	preloadsMonsters = true;
	update.monsterTypes = {5, 6};
	world.Receive(update);
	CHECK(Starting("load creature|") == (std::vector<std::string>{"load creature|5", "load creature|6"}));
	CHECK(Starting("progress|") == (std::vector<std::string>{"progress|0", "progress|15", "progress|17", "progress|47", "progress|54",
		"progress|61", "progress|68", "progress|70", "progress|100", "progress|100"}));
	// 29 creatures, one each: 69, then 70.
	calls.clear();
	preloadsMonsters = false;
	update.npcTypes.assign(20, 1);
	update.monsterTypes.assign(9, 2);
	world.Receive(update);
	const std::vector<std::string> drawn = Starting("progress|");
	CHECK(drawn.size() == 3 + 29 + 3 && drawn[3 + 28] == "progress|69" && drawn[3 + 29] == "progress|70");
}

TEST(UpdateInfoReplies, OnlyALargeZonesMoveToALargeZoneReleasesSprites)
{
	World world;
	Update update;
	update.monsterTypes = {5};
	update.zoneID = 22;
	// In a large zone, holding it, to a large zone: release the rest.
	g_bZonePlayerInLarge = true;
	g_nZoneLarge = 22;
	hasLargeZone = true;
	world.Receive(update);
	CHECK(Has("release sprites except|5") && Starting("sprite types|").empty());
	// To a small zone: add.
	calls.clear();
	update.zoneID = 21;
	world.Receive(update);
	CHECK(Has("sprite types|5") && Starting("release sprites except|").empty());
	// Any of the three missing: add.
	update.zoneID = 22;
	for (int missing = 0; missing < 3; missing++)
	{
		g_bZonePlayerInLarge = missing != 0;
		g_nZoneLarge = missing != 1 ? 22 : ZONEID_NULL;
		hasLargeZone = missing != 2;
		calls.clear();
		world.Receive(update);
		CHECK(Has("sprite types|5") && Starting("release sprites except|").empty());
	}
}

TEST(UpdateInfoReplies, FromTheGameTheZoneIsMovedToOtherwiseLoaded)
{
	World world;
	g_Mode = MODE_GAME;
	world.Receive(Update{});
	CHECK(Has("move zone|21") && !Has("load zone|21"));
	CHECK(At("xy|30|40") < At("move zone|21") && At("move zone|21") < At("encrypt"));
	calls.clear();
	g_Mode = MODE_WAIT_UPDATEINFO;
	world.Receive(Update{});
	CHECK(Has("load zone|21") && !Has("move zone|21"));
}

TEST(UpdateInfoReplies, NPCsAreNamedByTheTableOutsideTheEventZones)
{
	World world;
	Update update;
	update.npcs = {{"Packet Smith", 3, 10, 11}, {"Packet Ghost", 4, 12, 13}};
	world.Receive(update);
	CHECK(Starting("npc|") == (std::vector<std::string>{"npc|Table Smith|3|10|11", "npc|(null)|4|12|13"}));
	CHECK(At("encrypt") < At("npc|Table Smith|3|10|11") && At("npc|(null)|4|12|13") < At("stamp clock"));
	// In the event zones 1500-1506 the packet's names stand.
	for (int id : {1500, 1506})
	{
		calls.clear();
		zoneID = id;
		world.Receive(update);
		CHECK(Starting("npc|") == (std::vector<std::string>{"npc|Packet Smith|3|10|11", "npc|Packet Ghost|4|12|13"}));
	}
	for (int id : {1499, 1507})
	{
		calls.clear();
		zoneID = id;
		world.Receive(update);
		CHECK(Starting("npc|").front() == "npc|Table Smith|3|10|11");
	}
	// No zone: the table's.
	calls.clear();
	zoneID = 1500;
	zoneExists = false;
	world.Receive(update);
	CHECK(Starting("npc|").front() == "npc|Table Smith|3|10|11");
}
