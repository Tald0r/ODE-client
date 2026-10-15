#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "MPlayer.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MActionInfoTable.h"
#include "MEffectStatusTable.h"
#include "MEffectSpriteTypeTable.h"
#include "EffectSpriteTypeDef.h"
#include "RankBonusTable.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "MSkillManager.h"
#include "ModifyStatusManager.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MMoneyManager.h"
#include "MGuildInfoMapper.h"
#include "AddonDef.h"
#include "ClientMode.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "PlayerPresentationHost.h"
#include "PacketFunction.h"
#include "PCSlayerInfo2.h"
#include "PCVampireInfo2.h"
#include "PCOustersInfo2.h"
#include "VS_UI_title.h"

#include <string>
#include <vector>

// PacketFunction.cpp's, which no header declares.
extern BOOL g_bSetHotKey;
extern DWORD g_PreviousCreatureType;
extern S_SLOT g_char_slot_ingame;
int UI_GetFaceStyle(bool bMale, int faceStyle);

// The player from the game server's word on it (task 4.253 moved these
// into gameclient): SetPCSlayerInfo, SetPCVampireInfo and SetPCOustersInfo
// give g_pPlayer its body, guild, status values and money, fill the
// character sheet (g_char_slot_ingame) and the user's record, name the
// guild for the mapper and set the quick slots' hot keys once.
namespace {

std::vector<std::string> calls;
std::string N(long long value) { return std::to_string(value); }

bool Same(const std::vector<std::string>& expected)
{
	if (calls == expected) return true;
	std::printf("      calls: ");
	for (const std::string& call : calls) std::printf("[%s] ", call.c_str());
	std::printf("\n      wanted:");
	for (const std::string& call : expected) std::printf("[%s] ", call.c_str());
	std::printf("\n");
	return false;
}

// Every status value the player is given, in order, as the manager that
// reacts to them sees it; what the reactions do is ModifyStatusManager's.
struct StatusLog : ModifyStatusManager
{
	mutable std::vector<std::string> set;
	bool Execute(int id, void* pVoid) const override
	{
		set.push_back(N(id) + "=" + N(static_cast<MODIFY_VALUE*>(pVoid)->newValue));
		return true;
	}
};

const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.Creature = []() -> MCreature* { return g_pPlayer; };
	host.Status = []() -> MStatus* { return g_pPlayer; };
	host.GetRace = []() { return g_pPlayer->GetRace(); };
	return host;
}();

// Zone 3001 has no skills, so the skill check only empties the set.
const ZoneState::Host zoneHost = []() {
	ZoneState::Host host{};
	host.GetID = []() { return 3001; };
	return host;
}();

const PlayerPresentation::Host presentationHost = []() {
	PlayerPresentation::Host host{};
	host.SetHotKey = [](int hotkey, int id) { calls.push_back("hotkey|" + N(hotkey) + "|" + N(id)); };
	return host;
}();

struct Scene
{
	GameModelWorld world;
	CREATURE_TABLE creatureTypes;
	CREATURESPRITE_TABLE sprites;
	MActionInfoTable actions;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	RankBonusTable rankBonuses;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	MSkillSet skills;
	MSkillInfoTable skillInfo;
	MSkillManager skillManager;
	StatusLog statusLog;
	MVampireGear vampireGear;
	MOustersGear oustersGear;
	MMoneyManager money;
	MGuildInfoMapper guilds;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	MSkillSet* oldSkills = g_pSkillAvailable;
	MSkillInfoTable* oldSkillInfo = g_pSkillInfoTable;
	MSkillManager* oldSkillManager = g_pSkillManager;
	ModifyStatusManager* oldModify = g_pModifyStatusManager;
	MVampireGear* oldVampireGear = g_pVampireGear;
	MOustersGear* oldOustersGear = g_pOustersGear;
	MMoneyManager* oldMoney = g_pMoneyManager;
	MGuildInfoMapper* oldGuilds = g_pGuildInfoMapper;
	MPlayer* oldPlayer = g_pPlayer;
	int oldMode = g_Mode;
	BOOL oldSetHotKey = g_bSetHotKey;
	DWORD oldPreviousType = g_PreviousCreatureType;
	S_SLOT oldSlot = g_char_slot_ingame;
	const PlayerState::Host* oldPlayerHost = PlayerState::SetHost(&playerHost);
	const ZoneState::Host* oldZoneHost = ZoneState::SetHost(&zoneHost);
	const PlayerPresentation::Host* oldPresentation = PlayerPresentation::SetHost(&presentationHost);
	MPlayer* me = nullptr;

	Scene()
	{
		calls.clear();
		creatureTypes.Init(700);
		g_pCreatureTable = &creatureTypes;
		Type(CREATURETYPE_SLAYER_MALE, CREATURETRIBE_SLAYER, true);
		Type(CREATURETYPE_SLAYER_FEMALE, CREATURETRIBE_SLAYER, false);
		Type(CREATURETYPE_SLAYER_OPERATOR, CREATURETRIBE_SLAYER, true);
		Type(CREATURETYPE_VAMPIRE_MALE1, CREATURETRIBE_VAMPIRE, true);
		Type(CREATURETYPE_VAMPIRE_FEMALE1, CREATURETRIBE_VAMPIRE, false);
		Type(CREATURETYPE_VAMPIRE_OPERATOR, CREATURETRIBE_VAMPIRE, true);
		Type(CREATURETYPE_BAT, CREATURETRIBE_VAMPIRE, true);
		Type(CREATURETYPE_WOLF, CREATURETRIBE_VAMPIRE, true);
		Type(CREATURETYPE_WER_WOLF, CREATURETRIBE_VAMPIRE, true);
		Type(CREATURETYPE_OUSTERS, CREATURETRIBE_OUSTERS, false);
		Type(CREATURETYPE_OUSTERS_OPERATOR, CREATURETRIBE_OUSTERS, false);
		testfw::MutableRow(creatureTypes, CREATURETYPE_BAT).FlyingHeight = 30;
		sprites.Init(1);
		g_pCreatureSpriteTable = &sprites;
		actions.Init(MAX_ACTIONINFO);
		g_pActionInfoTable = &actions;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		g_pRankBonusTable = &rankBonuses;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		g_pSkillAvailable = &skills;
		skillInfo.CTypeTable<SKILLINFO_NODE>::Init(MAX_ACTIONINFO);
		g_pSkillInfoTable = &skillInfo;
		skillManager.CTypeTable<MSkillDomain>::Init(MAX_SKILLDOMAIN);
		g_pSkillManager = &skillManager;
		g_pModifyStatusManager = &statusLog;
		vampireGear.Init();
		g_pVampireGear = &vampireGear;
		oustersGear.Init();
		g_pOustersGear = &oustersGear;
		g_pMoneyManager = &money;
		g_pGuildInfoMapper = &guilds;
		g_Mode = MODE_WAIT_UPDATEINFO;
		g_bSetHotKey = FALSE;
		g_PreviousCreatureType = DWORD(-1);
		g_char_slot_ingame = S_SLOT();
		g_pUserInformation->CharacterID = "Kim";
		me = new MPlayer;
		g_pPlayer = me;
		me->SetCreatureType(CREATURETYPE_SLAYER_MALE);
	}
	~Scene()
	{
		delete me;
		g_pPlayer = oldPlayer;
		PlayerPresentation::SetHost(oldPresentation);
		ZoneState::SetHost(oldZoneHost);
		PlayerState::SetHost(oldPlayerHost);
		g_char_slot_ingame = oldSlot;
		g_PreviousCreatureType = oldPreviousType;
		g_bSetHotKey = oldSetHotKey;
		g_Mode = decltype(g_Mode)(oldMode);
		g_pGuildInfoMapper = oldGuilds;
		g_pMoneyManager = oldMoney;
		g_pOustersGear = oldOustersGear;
		g_pVampireGear = oldVampireGear;
		oustersGear.Release();
		vampireGear.Release();
		g_pModifyStatusManager = oldModify;
		g_pSkillManager = oldSkillManager;
		g_pSkillInfoTable = oldSkillInfo;
		g_pSkillAvailable = oldSkills;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pRankBonusTable = oldRankBonuses;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pActionInfoTable = oldActions;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatureTypes;
	}
	// A creature type of the tribe, of the sex its table row says, with a
	// colour of its own the packet's must replace.
	void Type(int type, CREATURETRIBE tribe, bool bMale)
	{
		testfw::MutableRow(creatureTypes, type).SetCreatureTribe(tribe, 0);
		testfw::MutableRow(creatureTypes, type).bMale = bMale;
		testfw::MutableRow(creatureTypes, type).ColorSet = 11;
	}
	DWORD Status(int n) const { return me->GetStatus(DWORD(n)); }
	const MCreatureWear::ADDON_INFO& Addon(int addon) const { return me->GetAddonInfo(addon); }
};

PCSlayerInfo2 Slayer()
{
	PCSlayerInfo2 info{};
	info.setObjectID(4001);
	info.setName("Kim");
	info.setSex(MALE);
	info.setHairStyle(HAIR_STYLE2);
	info.setHairColor(120);
	info.setSkinColor(230);
	info.setMasterEffectColor(3);
	info.setAlignment(5000);
	info.setSTR(41, ATTR_CURRENT);
	info.setSTR(42, ATTR_MAX);
	info.setSTR(43, ATTR_BASIC);
	info.setDEX(51, ATTR_CURRENT);
	info.setDEX(52, ATTR_MAX);
	info.setDEX(53, ATTR_BASIC);
	info.setINT(61, ATTR_CURRENT);
	info.setINT(62, ATTR_MAX);
	info.setINT(63, ATTR_BASIC);
	info.setSTRExp(701);
	info.setDEXExp(702);
	info.setINTExp(703);
	info.setRank(7);
	info.setRankExp(8800);
	info.setHP(150, 300);
	info.setMP(90, 200);
	info.setFame(5555);
	info.setGold(123456);
	for (int d = 0; d < 5; ++d) info.setSkillDomain(SkillDomain(d), SkillLevel_t(10 + d), SkillExp_t(1000 + d));
	info.setSight(13);
	for (int k = 0; k < 4; ++k) info.setHotKey(BYTE(k), SkillType_t(300 + k));
	info.setCompetence(3);
	info.setGuildID(31);
	info.setGuildName("Moon");
	info.setGuildMemberRank(2);
	info.setAdvancementLevel(0);
	info.setAdvancementGoalExp(9999);
	info.setAttrBonus(17);
	return info;
}

PCVampireInfo2 Vampire()
{
	PCVampireInfo2 info{};
	info.setObjectID(4002);
	info.setName("Kim");
	info.setLevel(45);
	info.setSex(MALE);
	info.setFame(6666);
	info.setBatColor(99);
	info.setSkinColor(230);
	info.setMasterEffectColor(4);
	info.setAlignment(-3000);
	info.setSTR(71, ATTR_CURRENT);
	info.setSTR(72, ATTR_MAX);
	info.setSTR(73, ATTR_BASIC);
	info.setDEX(81, ATTR_CURRENT);
	info.setDEX(82, ATTR_MAX);
	info.setDEX(83, ATTR_BASIC);
	info.setINT(91, ATTR_CURRENT);
	info.setINT(92, ATTR_MAX);
	info.setINT(93, ATTR_BASIC);
	info.setHP(250, 400);
	info.setRank(9);
	info.setRankExp(7700);
	info.setExp(3333);
	info.setGold(654321);
	info.setSight(12);
	info.setBonus(19);
	for (int k = 0; k < 8; ++k) info.setHotKey(BYTE(k), SkillType_t(400 + k));
	info.setSilverDamage(21);
	info.setCompetence(3);
	info.setGuildID(31);
	info.setGuildName("Moon");
	info.setGuildMemberRank(2);
	info.setAdvancementLevel(0);
	info.setAdvancementGoalExp(8888);
	return info;
}

PCOustersInfo2 Ousters()
{
	PCOustersInfo2 info{};
	info.setObjectID(4003);
	info.setName("Kim");
	info.setLevel(55);
	info.setSex(FEMALE);
	info.setFame(7777);
	info.setHairColor(140);
	info.setMasterEffectColor(5);
	info.setAlignment(-8000);
	info.setSTR(31, ATTR_CURRENT);
	info.setSTR(32, ATTR_MAX);
	info.setSTR(33, ATTR_BASIC);
	info.setDEX(34, ATTR_CURRENT);
	info.setDEX(35, ATTR_MAX);
	info.setDEX(36, ATTR_BASIC);
	info.setINT(37, ATTR_CURRENT);
	info.setINT(38, ATTR_MAX);
	info.setINT(39, ATTR_BASIC);
	info.setHP(260, 500);
	info.setMP(110, 220);
	info.setRank(11);
	info.setRankExp(6600);
	info.setExp(4444);
	info.setGold(111111);
	info.setSight(14);
	info.setBonus(23);
	info.setSkillBonus(29);
	info.setSilverDamage(27);
	info.setCompetence(3);
	info.setGuildID(32);
	info.setGuildName("Sun");
	info.setGuildMemberRank(4);
	info.setAdvancementLevel(0);
	info.setAdvancementGoalExp(7777);
	return info;
}

} // namespace

//----------------------------------------------------------------------
// The face the character sheet shows for a hair style: the man's or the
// woman's first, second or third, and none for another value.
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, FaceStyleFollowsTheHairStyleAndTheSex)
{
	CHECK_EQ(M_FACE1, UI_GetFaceStyle(true, HAIR_STYLE1));
	CHECK_EQ(M_FACE2, UI_GetFaceStyle(true, HAIR_STYLE2));
	CHECK_EQ(M_FACE3, UI_GetFaceStyle(true, HAIR_STYLE3));
	CHECK_EQ(W_FACE1, UI_GetFaceStyle(false, HAIR_STYLE1));
	CHECK_EQ(W_FACE2, UI_GetFaceStyle(false, HAIR_STYLE2));
	CHECK_EQ(W_FACE3, UI_GetFaceStyle(false, HAIR_STYLE3));
	CHECK_EQ(0, UI_GetFaceStyle(true, 3));
	CHECK_EQ(0, UI_GetFaceStyle(false, -1));
}

//----------------------------------------------------------------------
// A slayer: its id and sight, a male slayer's body with the packet's
// skin, master effect, hair (the second male style in its colour) and
// skin-coloured coat and trousers, the guild, the user's record, every
// status value in the order they are set, the sheet, the five domains,
// the money and the four hot keys.
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, SlayerInfoSetsThePlayerTheSheetAndTheHotKeys)
{
	Scene scene;
	g_pUserInformation->FaceStyle = HAIR_STYLE3;
	// What the sheet showed before, which the info replaces.
	g_char_slot_ingame.Race = RACE_VAMPIRE;
	g_char_slot_ingame.bl_female = true;
	g_char_slot_ingame.STATUS.resize(2);
	scene.me->SetStatus(MODIFY_TOHIT, 88);
	PCSlayerInfo2 info = Slayer();
	scene.statusLog.set.clear();

	SetPCSlayerInfo(&info);

	MPlayer* me = scene.me;
	CHECK_EQ(4001, me->GetID());
	CHECK_EQ(13, me->GetSight());
	CHECK_EQ(3, me->GetCompetence());
	CHECK_EQ(CREATURETYPE_SLAYER_MALE, me->GetCreatureType());
	CHECK(me->IsMale());
	CHECK_EQ(230, me->GetBodyColor1());
	CHECK_EQ(3, me->GetMasterEffectType());
	CHECK_EQ(ADDONID_HAIR2_MALE, scene.Addon(ADDON_HAIR).FrameID);
	CHECK_EQ(120, scene.Addon(ADDON_HAIR).ColorSet1);
	CHECK_EQ(230, scene.Addon(ADDON_COAT).ColorSet1);
	CHECK_EQ(230, scene.Addon(ADDON_TROUSER).ColorSet1);
	CHECK_EQ(31, me->GetGuildNumber());
	CHECK(std::string("Moon") == g_pUserInformation->GuildName.GetString());
	CHECK_EQ(2, g_pUserInformation->GuildGrade);
	CHECK_EQ(120, g_pUserInformation->HairColor);
	CHECK_EQ(230, g_pUserInformation->SkinColor);
	CHECK(std::string("Moon") == scene.guilds.GetGuildName(31));

	const std::vector<std::string> statuses = {
		N(MODIFY_MAX_HP) + "=300", N(MODIFY_MAX_MP) + "=200", N(MODIFY_CURRENT_HP) + "=150", N(MODIFY_CURRENT_MP) + "=90",
		N(MODIFY_CURRENT_STR) + "=41", N(MODIFY_CURRENT_DEX) + "=51", N(MODIFY_CURRENT_INT) + "=61",
		N(MODIFY_BASIC_STR) + "=43", N(MODIFY_BASIC_DEX) + "=53", N(MODIFY_BASIC_INT) + "=63",
		N(MODIFY_MAX_STR) + "=42", N(MODIFY_MAX_DEX) + "=52", N(MODIFY_MAX_INT) + "=62",
		N(MODIFY_STR_EXP_REMAIN) + "=701", N(MODIFY_DEX_EXP_REMAIN) + "=702", N(MODIFY_INT_EXP_REMAIN) + "=703",
		N(MODIFY_FAME) + "=5555", N(MODIFY_ALIGNMENT) + "=5000", N(MODIFY_RANK) + "=7", N(MODIFY_RANK_EXP_REMAIN) + "=8800",
		N(MODIFY_ADVANCEMENT_CLASS_LEVEL) + "=0", N(MODIFY_BONUS_POINT) + "=17",
	};
	CHECK(scene.statusLog.set == statuses);
	CHECK_EQ(300, scene.Status(MODIFY_MAX_HP));
	CHECK_EQ(17, scene.Status(MODIFY_BONUS_POINT));

	const S_SLOT& sheet = g_char_slot_ingame;
	CHECK(sheet.sz_name == "Kim");
	CHECK(sheet.sz_guild_name == "Moon");
	CHECK_EQ(31, sheet.GUILD_ID);
	CHECK_EQ(2, sheet.GUILD_GRADE);
	CHECK_EQ(7, sheet.GRADE);
	CHECK_EQ(8800, sheet.GRADE_EXP_REMAIN);
	CHECK_EQ(RACE_SLAYER, sheet.Race);
	CHECK(!sheet.bl_female);
	CHECK_EQ(120, sheet.hair_color);
	CHECK_EQ(230, sheet.skin_color);
	CHECK_EQ(M_FACE3, sheet.man_info.face);
	CHECK(sheet.STATUS.empty());
	CHECK_EQ(88, sheet.TOHIT);
	CHECK_EQ(1, sheet.level);
	CHECK_EQ(3, sheet.alignment);
	CHECK_EQ(41, sheet.STR_CUR);
	CHECK_EQ(51, sheet.DEX_CUR);
	CHECK_EQ(61, sheet.INT_CUR);
	CHECK_EQ(42, sheet.STR_MAX);
	CHECK_EQ(52, sheet.DEX_MAX);
	CHECK_EQ(62, sheet.INT_MAXX);
	CHECK_EQ(43, sheet.STR_PURE);
	CHECK_EQ(53, sheet.DEX_PURE);
	CHECK_EQ(63, sheet.INT_PURE);
	CHECK_EQ(150, sheet.HP);
	CHECK_EQ(90, sheet.MP);
	CHECK_EQ(300, sheet.HP_MAX);
	CHECK_EQ(200, sheet.MP_MAX);
	CHECK_EQ(5555, sheet.FAME);
	CHECK_EQ(701, sheet.STR_EXP_REMAIN);
	CHECK_EQ(702, sheet.DEX_EXP_REMAIN);
	CHECK_EQ(703, sheet.INT_EXP_REMAIN);
	// No advancement: the goal experience is not shown.
	CHECK_EQ(0, sheet.EXP_REMAIN);

	for (int d = 0; d < 5; ++d)
	{
		CHECK_EQ(10 + d, scene.skillManager[d].GetDomainLevel());
		CHECK_EQ(1000 + d, scene.skillManager[d].GetDomainExpRemain());
	}
	CHECK_EQ(0, scene.skillManager[SKILLDOMAIN_ETC].GetDomainLevel());
	CHECK_EQ(123456, scene.money.GetMoney());
	CHECK(Same({ "hotkey|0|300", "hotkey|1|301", "hotkey|2|302", "hotkey|3|303" }));
	CHECK_EQ(TRUE, g_bSetHotKey);
}

//----------------------------------------------------------------------
// Competence 0 is an operator: competence 0, the operator's body, and the
// packet's sex over the operator row's; a woman's hair and face.
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, SlayerOperatorTakesTheOperatorBodyAndThePacketsSex)
{
	Scene scene;
	g_pUserInformation->FaceStyle = HAIR_STYLE1;
	PCSlayerInfo2 info = Slayer();
	info.setCompetence(0);
	info.setSex(FEMALE);

	SetPCSlayerInfo(&info);

	CHECK_EQ(0, scene.me->GetCompetence());
	CHECK_EQ(CREATURETYPE_SLAYER_OPERATOR, scene.me->GetCreatureType());
	CHECK(!scene.me->IsMale());
	CHECK_EQ(ADDONID_HAIR2_FEMALE, scene.Addon(ADDON_HAIR).FrameID);
	CHECK(g_char_slot_ingame.bl_female);
	CHECK_EQ(W_FACE1, g_char_slot_ingame.woman_info.face);
}

//----------------------------------------------------------------------
// Any other competence leaves the player's alone and takes the body of
// the packet's sex.
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, SlayerOfAnotherCompetenceTakesTheBodyOfItsSex)
{
	Scene scene;
	g_pUserInformation->FaceStyle = HAIR_STYLE2;
	scene.me->SetCompetence(5);
	PCSlayerInfo2 info = Slayer();
	info.setSex(FEMALE);

	SetPCSlayerInfo(&info);

	CHECK_EQ(5, scene.me->GetCompetence());
	CHECK_EQ(CREATURETYPE_SLAYER_FEMALE, scene.me->GetCreatureType());
	CHECK(!scene.me->IsMale());
	CHECK_EQ(ADDONID_HAIR2_FEMALE, scene.Addon(ADDON_HAIR).FrameID);
	CHECK(g_char_slot_ingame.bl_female);
	CHECK_EQ(W_FACE2, g_char_slot_ingame.woman_info.face);
}

//----------------------------------------------------------------------
// An advanced slayer's coat takes the hair colour, and the sheet shows the
// advancement's goal experience.
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, AdvancedSlayerWearsItsHairColourAndShowsTheGoal)
{
	Scene scene;
	PCSlayerInfo2 info = Slayer();
	info.setAdvancementLevel(1);

	SetPCSlayerInfo(&info);

	CHECK_EQ(120, scene.Addon(ADDON_COAT).ColorSet1);
	CHECK_EQ(230, scene.Addon(ADDON_TROUSER).ColorSet1);
	CHECK_EQ(1, scene.Status(MODIFY_ADVANCEMENT_CLASS_LEVEL));
	CHECK_EQ(9999, g_char_slot_ingame.EXP_REMAIN);
}

//----------------------------------------------------------------------
// The hot keys are set once a session: a second info sets none.
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, HotKeysAreSetOnlyOnce)
{
	Scene scene;
	g_bSetHotKey = TRUE;
	PCSlayerInfo2 slayer = Slayer();
	PCVampireInfo2 vampire = Vampire();

	SetPCSlayerInfo(&slayer);
	SetPCVampireInfo(&vampire);

	CHECK(Same({}));
	CHECK_EQ(TRUE, g_bSetHotKey);
}

//----------------------------------------------------------------------
// No guild name: the user's is empty and the sheet and the mapper show "".
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, SlayerWithoutAGuildNameShowsAnEmptyOne)
{
	Scene scene;
	g_char_slot_ingame.sz_guild_name = "Old";
	PCSlayerInfo2 info = Slayer();
	info.setGuildName("");

	SetPCSlayerInfo(&info);

	CHECK(g_pUserInformation->GuildName.GetString() == NULL);
	CHECK(g_char_slot_ingame.sz_guild_name.empty());
	CHECK(std::string() == scene.guilds.GetGuildName(31));
}

//----------------------------------------------------------------------
// A domain the skill manager does not hold is passed over.
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, SlayerDomainsPastTheManagerAreSkipped)
{
	Scene scene;
	scene.skillManager.CTypeTable<MSkillDomain>::Init(3);
	PCSlayerInfo2 info = Slayer();

	SetPCSlayerInfo(&info);

	CHECK_EQ(3, scene.skillManager.GetSize());
	CHECK_EQ(12, scene.skillManager[SKILLDOMAIN_GUN].GetDomainLevel());
	CHECK_EQ(1002, scene.skillManager[SKILLDOMAIN_GUN].GetDomainExpRemain());
	CHECK_EQ(123456, scene.money.GetMoney());
}

//----------------------------------------------------------------------
// A vampire in its own body: its id and sight, the first male body on the
// ground with the packet's skin, a fixed coat colour and the bat colour,
// the guild, the user's record, every status value in order, the sheet
// (the bat colour as its hair), the money, its gear emptied and eight hot
// keys.
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, VampireInfoSetsThePlayerTheSheetAndTheHotKeys)
{
	Scene scene;
	scene.vampireGear.SetBloodBibleOpenSlot(3);
	g_char_slot_ingame.sz_guild_name = "Old";
	g_char_slot_ingame.EXP_REMAIN = 1;
	g_char_slot_ingame.STATUS.resize(2);
	// Flying before, and with the mana and to-hit the sheet copies.
	scene.me->SetFlyingCreature();
	scene.me->SetStatus(MODIFY_TOHIT, 87);
	scene.me->SetStatus(MODIFY_MAX_MP, 6);
	scene.me->SetStatus(MODIFY_CURRENT_MP, 5);
	PCVampireInfo2 info = Vampire();
	scene.statusLog.set.clear();

	SetPCVampireInfo(&info);

	MPlayer* me = scene.me;
	CHECK_EQ(4002, me->GetID());
	CHECK_EQ(12, me->GetSight());
	CHECK_EQ(CREATURETYPE_VAMPIRE_MALE1, me->GetCreatureType());
	CHECK_EQ(3, me->GetCompetence());
	CHECK(me->IsMale());
	CHECK(me->IsGroundCreature());
	CHECK_EQ(230, me->GetBodyColor1());
	CHECK_EQ(377, me->GetBodyColor2());
	CHECK_EQ(4, me->GetMasterEffectType());
	CHECK_EQ(99, me->GetBatColor());
	CHECK_EQ(31, me->GetGuildNumber());
	CHECK(std::string("Moon") == g_pUserInformation->GuildName.GetString());
	CHECK_EQ(2, g_pUserInformation->GuildGrade);
	CHECK_EQ(99, g_pUserInformation->HairColor);
	CHECK_EQ(230, g_pUserInformation->SkinColor);
	CHECK_EQ(99, g_pUserInformation->BatColor);
	// Defect: the mapper is told the guild's name before the sheet takes the
	// new one (PacketFunction.cpp:3040, four lines above the slayer's and the
	// ousters' order), so it keeps the name the sheet showed before.
	CHECK(std::string("Old") == scene.guilds.GetGuildName(31));

	const std::vector<std::string> statuses = {
		N(MODIFY_MAX_HP) + "=400", N(MODIFY_CURRENT_HP) + "=250",
		N(MODIFY_CURRENT_STR) + "=71", N(MODIFY_CURRENT_DEX) + "=81", N(MODIFY_CURRENT_INT) + "=91",
		N(MODIFY_BASIC_STR) + "=73", N(MODIFY_BASIC_DEX) + "=83", N(MODIFY_BASIC_INT) + "=93",
		N(MODIFY_MAX_STR) + "=72", N(MODIFY_MAX_DEX) + "=82", N(MODIFY_MAX_INT) + "=92",
		N(MODIFY_BONUS_POINT) + "=19", N(MODIFY_LEVEL) + "=45", N(MODIFY_FAME) + "=6666", N(MODIFY_VAMP_EXP_REMAIN) + "=3333",
		N(MODIFY_SILVER_DAMAGE) + "=21", N(MODIFY_ALIGNMENT) + "=" + N(DWORD(-3000)), N(MODIFY_RANK) + "=9",
		N(MODIFY_RANK_EXP_REMAIN) + "=7700", N(MODIFY_ADVANCEMENT_CLASS_LEVEL) + "=0",
	};
	CHECK(scene.statusLog.set == statuses);

	const S_SLOT& sheet = g_char_slot_ingame;
	CHECK(sheet.sz_name == "Kim");
	CHECK(sheet.sz_guild_name == "Moon");
	CHECK_EQ(31, sheet.GUILD_ID);
	CHECK_EQ(2, sheet.GUILD_GRADE);
	CHECK_EQ(9, sheet.GRADE);
	CHECK_EQ(7700, sheet.GRADE_EXP_REMAIN);
	CHECK_EQ(99, sheet.hair_color);
	CHECK_EQ(230, sheet.skin_color);
	CHECK_EQ(M_FACE1, sheet.man_info.face);
	CHECK(sheet.STATUS.empty());
	CHECK_EQ(87, sheet.TOHIT);
	CHECK_EQ(5, sheet.MP);
	CHECK_EQ(6, sheet.MP_MAX);
	CHECK_EQ(45, sheet.level);
	CHECK_EQ(1, sheet.alignment);
	CHECK_EQ(3333, sheet.EXP_REMAIN);
	CHECK_EQ(71, sheet.STR_CUR);
	CHECK_EQ(81, sheet.DEX_CUR);
	CHECK_EQ(91, sheet.INT_CUR);
	CHECK_EQ(72, sheet.STR_MAX);
	CHECK_EQ(82, sheet.DEX_MAX);
	CHECK_EQ(92, sheet.INT_MAXX);
	CHECK_EQ(73, sheet.STR_PURE);
	CHECK_EQ(83, sheet.DEX_PURE);
	CHECK_EQ(93, sheet.INT_PURE);
	CHECK_EQ(250, sheet.HP);
	CHECK_EQ(400, sheet.HP_MAX);
	CHECK_EQ(6666, sheet.FAME);
	CHECK_EQ(654321, scene.money.GetMoney());
	CHECK_EQ(0, scene.vampireGear.GetBloodBibleOpenSlot());
	CHECK(Same({ "hotkey|0|400", "hotkey|1|401", "hotkey|2|402", "hotkey|3|403",
		"hotkey|4|404", "hotkey|5|405", "hotkey|6|406", "hotkey|7|407" }));
	CHECK_EQ(TRUE, g_bSetHotKey);
}

//----------------------------------------------------------------------
// A woman vampire: the first female body, the same fixed coat colour, and
// the woman's first face.
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, VampireWomanTakesTheFemaleBody)
{
	Scene scene;
	PCVampireInfo2 info = Vampire();
	info.setSex(FEMALE);

	SetPCVampireInfo(&info);

	CHECK_EQ(CREATURETYPE_VAMPIRE_FEMALE1, scene.me->GetCreatureType());
	CHECK(!scene.me->IsMale());
	CHECK_EQ(377, scene.me->GetBodyColor2());
	CHECK_EQ(W_FACE1, g_char_slot_ingame.woman_info.face);
}

//----------------------------------------------------------------------
// A vampire operator: competence 0 and the operator's body, of the
// packet's sex.
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, VampireOperatorTakesTheOperatorBody)
{
	Scene scene;
	PCVampireInfo2 info = Vampire();
	info.setCompetence(0);
	info.setSex(FEMALE);

	SetPCVampireInfo(&info);

	CHECK_EQ(0, scene.me->GetCompetence());
	CHECK_EQ(CREATURETYPE_VAMPIRE_OPERATOR, scene.me->GetCreatureType());
	CHECK(!scene.me->IsMale());
}

//----------------------------------------------------------------------
// A vampire transformed when the info came keeps the bat's body (flying),
// the wolf's or the werewolf's (with its fixed colour); an operator's
// competence is still 0, another's is left alone.
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, TransformedVampireKeepsItsForm)
{
	{
		Scene scene;
		g_PreviousCreatureType = CREATURETYPE_BAT;
		PCVampireInfo2 info = Vampire();
		info.setCompetence(0);

		SetPCVampireInfo(&info);

		CHECK_EQ(CREATURETYPE_BAT, scene.me->GetCreatureType());
		CHECK_EQ(0, scene.me->GetCompetence());
		CHECK(scene.me->IsFlyingCreature());
		CHECK_EQ(230, scene.me->GetBodyColor1());
	}
	{
		Scene scene;
		g_PreviousCreatureType = CREATURETYPE_WOLF;
		scene.me->SetCompetence(5);
		scene.me->SetFlyingCreature();
		PCVampireInfo2 info = Vampire();
		info.setSex(FEMALE);

		SetPCVampireInfo(&info);

		CHECK_EQ(CREATURETYPE_WOLF, scene.me->GetCreatureType());
		CHECK_EQ(5, scene.me->GetCompetence());
		CHECK(scene.me->IsGroundCreature());
		CHECK(!scene.me->IsMale());
	}
	{
		Scene scene;
		g_PreviousCreatureType = CREATURETYPE_WER_WOLF;
		PCVampireInfo2 info = Vampire();

		SetPCVampireInfo(&info);

		CHECK_EQ(CREATURETYPE_WER_WOLF, scene.me->GetCreatureType());
		CHECK(scene.me->IsGroundCreature());
		CHECK_EQ(377, scene.me->GetBodyColor1());
	}
}

//----------------------------------------------------------------------
// Bat colour 0 means none: the player's is 0xFFFF, the record's 0.
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, VampireWithoutABatColourGetsTheDefault)
{
	Scene scene;
	PCVampireInfo2 info = Vampire();
	info.setBatColor(0);

	SetPCVampireInfo(&info);

	CHECK_EQ(0xFFFF, scene.me->GetBatColor());
	CHECK_EQ(0, g_pUserInformation->BatColor);
	CHECK_EQ(0, g_char_slot_ingame.hair_color);
}

//----------------------------------------------------------------------
// An advanced vampire's or ousters' sheet shows the advancement's goal.
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, AdvancedVampireAndOustersShowTheGoal)
{
	{
		Scene scene;
		PCVampireInfo2 info = Vampire();
		info.setAdvancementLevel(2);

		SetPCVampireInfo(&info);

		CHECK_EQ(2, scene.Status(MODIFY_ADVANCEMENT_CLASS_LEVEL));
		CHECK_EQ(8888, g_char_slot_ingame.EXP_REMAIN);
	}
	{
		Scene scene;
		PCOustersInfo2 info = Ousters();
		info.setAdvancementLevel(2);

		SetPCOustersInfo(&info);

		CHECK_EQ(2, scene.Status(MODIFY_ADVANCEMENT_CLASS_LEVEL));
		CHECK_EQ(7777, g_char_slot_ingame.EXP_REMAIN);
	}
}

//----------------------------------------------------------------------
// An ousters: its id and sight, the ousters body (of its row's sex) on the
// ground with the hair colour, the guild named for the mapper, the user's
// record, every status value in order, the sheet, the money, its gear
// emptied, and no hot keys.
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, OustersInfoSetsThePlayerAndTheSheet)
{
	Scene scene;
	scene.oustersGear.SetBloodBibleOpenSlot(2);
	g_pUserInformation->SkinColor = 66;
	g_char_slot_ingame.STATUS.resize(2);
	g_char_slot_ingame.alignment = ALIGNMENT(4);
	scene.me->SetFlyingCreature();
	scene.me->SetStatus(MODIFY_TOHIT, 86);
	PCOustersInfo2 info = Ousters();
	scene.statusLog.set.clear();

	SetPCOustersInfo(&info);

	MPlayer* me = scene.me;
	CHECK_EQ(4003, me->GetID());
	CHECK_EQ(14, me->GetSight());
	CHECK_EQ(CREATURETYPE_OUSTERS, me->GetCreatureType());
	CHECK_EQ(3, me->GetCompetence());
	CHECK(!me->IsMale());
	CHECK(me->IsGroundCreature());
	CHECK_EQ(140, me->GetBodyColor1());
	CHECK_EQ(5, me->GetMasterEffectType());
	CHECK_EQ(32, me->GetGuildNumber());
	CHECK(std::string("Sun") == g_pUserInformation->GuildName.GetString());
	CHECK_EQ(4, g_pUserInformation->GuildGrade);
	CHECK_EQ(140, g_pUserInformation->HairColor);
	CHECK_EQ(66, g_pUserInformation->SkinColor);
	CHECK(std::string("Sun") == scene.guilds.GetGuildName(32));

	const std::vector<std::string> statuses = {
		N(MODIFY_MAX_HP) + "=500", N(MODIFY_MAX_MP) + "=220", N(MODIFY_CURRENT_HP) + "=260", N(MODIFY_CURRENT_MP) + "=110",
		N(MODIFY_CURRENT_STR) + "=31", N(MODIFY_CURRENT_DEX) + "=34", N(MODIFY_CURRENT_INT) + "=37",
		N(MODIFY_BASIC_STR) + "=33", N(MODIFY_BASIC_DEX) + "=36", N(MODIFY_BASIC_INT) + "=39",
		N(MODIFY_MAX_STR) + "=32", N(MODIFY_MAX_DEX) + "=35", N(MODIFY_MAX_INT) + "=38",
		N(MODIFY_BONUS_POINT) + "=23", N(MODIFY_SKILL_BONUS_POINT) + "=29", N(MODIFY_LEVEL) + "=55", N(MODIFY_FAME) + "=7777",
		N(MODIFY_VAMP_EXP_REMAIN) + "=4444", N(MODIFY_SILVER_DAMAGE) + "=27", N(MODIFY_ALIGNMENT) + "=" + N(DWORD(-8000)),
		N(MODIFY_RANK) + "=11", N(MODIFY_RANK_EXP_REMAIN) + "=6600", N(MODIFY_ADVANCEMENT_CLASS_LEVEL) + "=0",
	};
	CHECK(scene.statusLog.set == statuses);

	const S_SLOT& sheet = g_char_slot_ingame;
	CHECK(sheet.sz_name == "Kim");
	CHECK(sheet.sz_guild_name == "Sun");
	CHECK_EQ(32, sheet.GUILD_ID);
	CHECK_EQ(4, sheet.GUILD_GRADE);
	CHECK_EQ(11, sheet.GRADE);
	CHECK_EQ(6600, sheet.GRADE_EXP_REMAIN);
	CHECK_EQ(140, sheet.hair_color);
	CHECK_EQ(W_FACE1, sheet.woman_info.face);
	CHECK(sheet.STATUS.empty());
	CHECK_EQ(86, sheet.TOHIT);
	CHECK_EQ(55, sheet.level);
	CHECK_EQ(0, sheet.alignment);
	CHECK_EQ(4444, sheet.EXP_REMAIN);
	CHECK_EQ(31, sheet.STR_CUR);
	CHECK_EQ(34, sheet.DEX_CUR);
	CHECK_EQ(37, sheet.INT_CUR);
	CHECK_EQ(32, sheet.STR_MAX);
	CHECK_EQ(35, sheet.DEX_MAX);
	CHECK_EQ(38, sheet.INT_MAXX);
	CHECK_EQ(33, sheet.STR_PURE);
	CHECK_EQ(36, sheet.DEX_PURE);
	CHECK_EQ(39, sheet.INT_PURE);
	CHECK_EQ(260, sheet.HP);
	CHECK_EQ(110, sheet.MP);
	CHECK_EQ(500, sheet.HP_MAX);
	CHECK_EQ(220, sheet.MP_MAX);
	CHECK_EQ(7777, sheet.FAME);
	CHECK_EQ(111111, scene.money.GetMoney());
	CHECK_EQ(0, scene.oustersGear.GetBloodBibleOpenSlot());
	CHECK(Same({}));
	CHECK_EQ(FALSE, g_bSetHotKey);
}

//----------------------------------------------------------------------
// An ousters operator: competence 0, the operator's body and the packet's
// sex, so a man's face.
//----------------------------------------------------------------------
TEST(PacketPlayerInfo, OustersOperatorTakesTheOperatorBodyAndThePacketsSex)
{
	Scene scene;
	PCOustersInfo2 info = Ousters();
	info.setCompetence(0);
	info.setSex(MALE);

	SetPCOustersInfo(&info);

	CHECK_EQ(0, scene.me->GetCompetence());
	CHECK_EQ(CREATURETYPE_OUSTERS_OPERATOR, scene.me->GetCreatureType());
	CHECK(scene.me->IsMale());
	CHECK_EQ(M_FACE1, g_char_slot_ingame.man_info.face);
}
