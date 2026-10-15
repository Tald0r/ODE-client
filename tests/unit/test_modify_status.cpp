#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "ModifyStatusManager.h"
#include "Packet/Types/ModifyDef.h"
#include "Alignment.h"
#include "MStatus.h"
#include "MItem.h"
#include "MInventory.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MSkillManager.h"
#include "MMoneyManager.h"
#include "MCreatureTable.h"
#include "MGameStringTable.h"
#include "ExperienceTable.h"
#include "UserInformation.h"
#include "ClientMode.h"
#include "GameMessages.h"
#include "MHelpDef.h"
#include "../basic/CMessageArray.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "StatusPresentationHost.h"
#include "GameInfoPresentationHost.h"
#include "RankBonusHandlerHost.h"

#include <string>
#include <vector>

// What a change of one of the player's status values does (task 4.235,
// gamemodel's since then): ModifyStatusManager runs a function per value
// after MPlayer::SetStatus stored it. The UI, the player and the zone are
// fakes that record each call.
namespace {

std::vector<std::string> calls;
std::string N(long long value) { return std::to_string(value); }

MStatus status;
Race race = RACE_SLAYER;
bool ousters = false;
bool noPlayer = false;
int sight = 13;
int lightSights = 0;
TYPE_OBJECTID playerID = 77;
MItemManager* gear = NULL;
MObject playerObject, petObject;
MCreature* Player() { return reinterpret_cast<MCreature*>(&playerObject); }
MCreature* Pet() { return reinterpret_cast<MCreature*>(&petObject); }
TYPE_OBJECTID petID = OBJECTID_NULL;
MPetItem* petItem = NULL;
bool petInZone = true;

const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.Status = []() -> MStatus* { return noPlayer ? NULL : &status; };
	host.GetRace = []() { return race; };
	host.Creature = []() { return Player(); };
	host.GetGear = []() { return gear; };
	host.CalculateStatus = []() { calls.push_back("calculate"); };
	host.SetGuildNumber = [](int number) { calls.push_back("guild|" + N(number)); };
	host.GetID = []() { return playerID; };
	host.SetSight = [](int value) { calls.push_back("sight|" + N(value)); };
	host.GetSight = []() { return sight; };
	host.HasEffectStatus = [](EFFECTSTATUS) { return false; };
	host.CalculateLightSight = []() { ++lightSights; };
	return host;
}();
const CreatureState::Host creatureHost = []() {
	CreatureState::Host host{};
	host.IsOusters = [](MCreature* pCreature) { CHECK(pCreature == Player()); return ousters; };
	host.GetServerX = [](MCreature* pCreature) { CHECK(pCreature == Player()); return TYPE_SECTORPOSITION(40); };
	host.GetServerY = [](MCreature* pCreature) { CHECK(pCreature == Player()); return TYPE_SECTORPOSITION(50); };
	host.SetWeaponSpeed = [](MCreature* pCreature, int speed) { CHECK(pCreature == Player()); calls.push_back("speed|" + N(speed)); };
	host.GetPetID = [](MCreature* pCreature) { CHECK(pCreature == Player()); return petID; };
	host.GetPetItem = [](MCreature* pCreature) { CHECK(pCreature == Pet()); return petItem; };
	host.SetDead = [](MCreature* pCreature) { CHECK(pCreature == Pet()); calls.push_back("dead"); };
	host.SetUnionGuildID = [](MCreature* pCreature, int unionID) { CHECK(pCreature == Player()); calls.push_back("union|" + N(unionID)); };
	host.GetCreatureType = [](MCreature*) { return int(race == RACE_VAMPIRE ? CREATURETYPE_VAMPIRE_MALE1 : CREATURETYPE_SLAYER_MALE); };
	return host;
}();
const ZoneState::Host zoneHost = []() {
	ZoneState::Host host{};
	host.GetFakeCreature = [](TYPE_OBJECTID id) { return petInZone && id == petID && id != OBJECTID_NULL ? Pet() : static_cast<MCreature*>(NULL); };
	host.KeepObjectInSight = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y, int value) { calls.push_back("keep|" + N(x) + "|" + N(y) + "|" + N(value)); };
	host.GetID = []() { return 1; };
	return host;
}();
const StatusPresentation::Host statusHost = []() {
	StatusPresentation::Host host{};
	host.SetHP = [](int current, int maximum) { calls.push_back("hp|" + N(current) + "|" + N(maximum)); };
	host.SetMP = [](int current, int maximum) { calls.push_back("mp|" + N(current) + "|" + N(maximum)); };
	host.SetCharInfoHP = [](int value) { calls.push_back("info hp|" + N(value)); };
	host.SetCharInfoMP = [](int value) { calls.push_back("info mp|" + N(value)); };
	host.SetCharInfoHP_max = [](int value) { calls.push_back("info hp max|" + N(value)); };
	host.SetCharInfoMP_max = [](int value) { calls.push_back("info mp max|" + N(value)); };
	host.SetCharInfoSTR = [](int value) { calls.push_back("info str|" + N(value)); };
	host.SetCharInfoDEX = [](int value) { calls.push_back("info dex|" + N(value)); };
	host.SetCharInfoINT = [](int value) { calls.push_back("info int|" + N(value)); };
	host.SetCharInfoFAME = [](int value) { calls.push_back("info fame|" + N(value)); };
	host.SetCharInfoTOHIT = [](int value) { calls.push_back("info tohit|" + N(value)); };
	host.SetCharInfoDefense = [](int value) { calls.push_back("info defense|" + N(value)); };
	host.SetCharInfoProtection = [](int value) { calls.push_back("info protection|" + N(value)); };
	host.SetCharInfoLevel = [](int value) { calls.push_back("info level|" + N(value)); };
	host.SetCharInfoSilverDamage = [](int value) { calls.push_back("info silver|" + N(value)); };
	host.LevelUp = []() { calls.push_back("level up"); };
	host.SetBonusPoint = [](int points) { calls.push_back("bonus|" + N(points)); };
	host.SetSkillPoint = [](int points) { calls.push_back("skill points|" + N(points)); };
	host.SetElementalValue = [](int element, int value) { calls.push_back("element|" + N(element) + "|" + N(value)); };
	host.SetSheet = [](StatusPresentation::SheetField field, int value) { calls.push_back("sheet|" + N(field) + "|" + N(value)); };
	return host;
}();
const GameInfoPresentation::Host infoHost = []() {
	GameInfoPresentation::Host host{};
	host.ExecuteHelpEvent = [](int helpEvent) { calls.push_back("help|" + N(helpEvent)); };
	return host;
}();
const RankBonusHandlers::Host rankHost = []() {
	RankBonusHandlers::Host host{};
	host.CheckRegen = []() { calls.push_back("regen"); };
	return host;
}();
const MItemHost itemHost = []() {
	MItemHost host{};
	host.PetName = [](MItem*) { return std::string("Rex"); };
	host.RefreshAffect = [](MItem* pItem) { calls.push_back("refresh|" + N(pItem->GetID())); };
	return host;
}();

struct Ware : MItem
{
	ITEM_CLASS m_Class;
	MItem::ITEM_GEAR_SLOT m_Slot;
	Ware(ITEM_CLASS itemClass, TYPE_OBJECTID id, MItem::ITEM_GEAR_SLOT slot = MItem::SLOT_NULL) : m_Class(itemClass), m_Slot(slot) { SetID(id); }
	ITEM_CLASS GetItemClass() const override { return m_Class; }
	ITEM_GEAR_SLOT GetGearSlot() const override { return m_Slot; }
};

// Messages in a log, oldest first.
std::vector<std::string> Lines(CMessageArray& log)
{
	std::vector<std::string> lines;
	for (int i = 0; i < log.GetSize(); ++i)
		if (log[i][0] != '\0') lines.push_back(log[i]);
	return lines;
}

// A message as a log formats it.
template <class... Args>
std::string Formatted(int stringID, Args... args)
{
	CMessageArray one;
	one.Init(1, 256);
	one.AddSafeFormat(GetGameString(stringID), args...);
	return one[0];
}

struct Scene
{
	GameModelWorld world;
	ModifyStatusManager manager;
	CMessageArray gameLog, systemLog;
	MSkillInfoTable info;
	MSkillManager skills;
	MSkillSet available;
	MInventory inventory;
	MSlayerGear slayerGear;
	MVampireGear vampireGear;
	MOustersGear oustersGear;
	MMoneyManager money;
	CLIENT_MODE oldMode = g_Mode;
	CMessageArray* oldGame = g_pGameMessage;
	CMessageArray* oldSystem = g_pSystemMessage;
	MSkillInfoTable* oldInfo = g_pSkillInfoTable;
	MSkillManager* oldSkills = g_pSkillManager;
	MSkillSet* oldAvailable = g_pSkillAvailable;
	MInventory* oldInventory = g_pInventory;
	MSlayerGear* oldSlayerGear = g_pSlayerGear;
	MVampireGear* oldVampireGear = g_pVampireGear;
	MOustersGear* oldOustersGear = g_pOustersGear;
	MMoneyManager* oldMoney = g_pMoneyManager;
	MMagazine* oldMagazine = g_pCurrentMagazine;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&playerHost);
	const CreatureState::Host* oldCreatures = CreatureState::SetHost(&creatureHost);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zoneHost);
	const StatusPresentation::Host* oldStatus = StatusPresentation::SetHost(&statusHost);
	const GameInfoPresentation::Host* oldInfoHost = GameInfoPresentation::SetHost(&infoHost);
	const RankBonusHandlers::Host* oldRank = RankBonusHandlers::SetHost(&rankHost);

	explicit Scene(Race playerRace = RACE_SLAYER)
	{
		InitGameStringTable();
		calls.clear();
		status = MStatus();
		race = playerRace;
		ousters = playerRace == RACE_OUSTERS;
		noPlayer = false;
		sight = 13;
		lightSights = 0;
		playerID = 77;
		petID = OBJECTID_NULL;
		petItem = NULL;
		petInZone = true;
		g_Mode = MODE_GAME;
		gameLog.Init(8, 256);
		systemLog.Init(8, 256);
		g_pGameMessage = &gameLog;
		g_pSystemMessage = &systemLog;
		info.CTypeTable<SKILLINFO_NODE>::Init(MAX_ACTIONINFO);
		g_pSkillInfoTable = &info;
		skills.Init();
		g_pSkillManager = &skills;
		g_pSkillAvailable = &available;
		inventory.Init(4, 4);
		slayerGear.Init();
		vampireGear.Init();
		oustersGear.Init();
		g_pInventory = &inventory;
		g_pSlayerGear = &slayerGear;
		g_pVampireGear = &vampireGear;
		g_pOustersGear = &oustersGear;
		gear = playerRace == RACE_VAMPIRE ? static_cast<MItemManager*>(&vampireGear)
			: playerRace == RACE_OUSTERS ? static_cast<MItemManager*>(&oustersGear) : static_cast<MItemManager*>(&slayerGear);
		g_pMoneyManager = &money;
		g_pCurrentMagazine = NULL;
		g_pItemTable->InitClass(ITEM_CLASS_POTION, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_POTION, 0).SetGrid(1, 1);
		MItem::SetHost(&itemHost);
	}
	~Scene()
	{
		MItem::SetHost(NULL);
		RankBonusHandlers::SetHost(oldRank);
		GameInfoPresentation::SetHost(oldInfoHost);
		StatusPresentation::SetHost(oldStatus);
		ZoneState::SetHost(oldZone);
		CreatureState::SetHost(oldCreatures);
		PlayerState::SetHost(oldPlayer);
		g_pCurrentMagazine = oldMagazine;
		g_pMoneyManager = oldMoney;
		g_pOustersGear = oldOustersGear;
		g_pVampireGear = oldVampireGear;
		g_pSlayerGear = oldSlayerGear;
		g_pInventory = oldInventory;
		g_pSkillAvailable = oldAvailable;
		g_pSkillManager = oldSkills;
		g_pSkillInfoTable = oldInfo;
		g_pSystemMessage = oldSystem;
		g_pGameMessage = oldGame;
		g_Mode = oldMode;
		inventory.Release();
		gear = NULL;
	}

	// Runs the value's function as MPlayer::SetStatus does, after storing it.
	MODIFY_VALUE Modify(int part, DWORD value, DWORD oldValue = 0)
	{
		MODIFY_VALUE modify;
		modify.oldValue = oldValue;
		modify.newValue = value;
		CHECK(manager.Execute(part, &modify));
		return modify;
	}
	std::vector<std::string> Messages() { return Lines(gameLog); }
	void Clear() { calls.clear(); gameLog.Clear(); systemLog.Clear(); }
};

using Calls = std::vector<std::string>;

// The English table words some "_1" and "_2" strings alike (they differed
// in Korean by the number's last digit); the tests tell them apart.
void Distinct(int first, int second)
{
	testfw::MutableRow(*g_pGameStringTable, first) = (std::string("[1] ") + GetGameString(first)).c_str();
	testfw::MutableRow(*g_pGameStringTable, second) = (std::string("[2] ") + GetGameString(second)).c_str();
}

// For each last digit, which string a value ending in it takes.
bool FirstForm(int value) { const int digit = value % 10; return digit == 2 || digit == 4 || digit == 5 || digit == 9; }

std::string Sheet(StatusPresentation::SheetField field, long long value) { return "sheet|" + N(field) + "|" + N(value); }

} // namespace

TEST(ModifyStatus, EveryValueHasAFunction)
{
	Scene scene;
	MODIFY_VALUE value{};
	for (int part : {MODIFY_CURRENT_HP, MODIFY_GOLD, MODIFY_SKILL_BONUS_POINT, MODIFY_ADVANCEMENT_CLASS_GOAL_EXP})
		CHECK(scene.manager.Execute(part, &value));
	// A value without one, or past the end, runs nothing.
	CHECK(!scene.manager.Execute(MODIFY_MAX, &value));
}

TEST(ModifyStatus, CurrentHPStopsBelowTheSilverDamage)
{
	Scene scene(RACE_SLAYER);
	status.SetStatus(MODIFY_MAX_HP, 100);
	status.SetStatus(MODIFY_SILVER_DAMAGE, 30);
	CHECK_EQ(70u, scene.Modify(MODIFY_CURRENT_HP, 90).newValue);
	CHECK((calls == Calls{"hp|70|100", "info hp|70"}));
	scene.Clear();
	CHECK_EQ(70u, scene.Modify(MODIFY_CURRENT_HP, 70).newValue);
	CHECK_EQ(69u, scene.Modify(MODIFY_CURRENT_HP, 69).newValue);
	CHECK_EQ(0, lightSights);
	// A vampire's skills follow its HP.
	race = RACE_VAMPIRE;
	scene.Modify(MODIFY_CURRENT_HP, 10);
	CHECK_EQ(1, lightSights);
}

TEST(ModifyStatus, CurrentMPIsShownAsIs)
{
	Scene scene(RACE_SLAYER);
	status.SetStatus(MODIFY_MAX_MP, 50);
	CHECK_EQ(80u, scene.Modify(MODIFY_CURRENT_MP, 80).newValue);
	CHECK((calls == Calls{"mp|80|50", "info mp|80"}));
	CHECK_EQ(1, lightSights);
	// Not a vampire's.
	race = RACE_VAMPIRE;
	scene.Modify(MODIFY_CURRENT_MP, 3);
	CHECK_EQ(1, lightSights);
}

TEST(ModifyStatus, MaxHPAndMPAreShownAndTold)
{
	Scene scene;
	status.SetStatus(MODIFY_CURRENT_HP, 40);
	status.SetStatus(MODIFY_CURRENT_MP, 30);
	scene.Modify(MODIFY_MAX_HP, 124);
	CHECK((calls == Calls{"hp|40|124", "info hp max|124"}));
	scene.Modify(MODIFY_MAX_HP, 120);
	CHECK((scene.Messages() == Calls{Formatted(STRING_STATUS_HP_MAX_1, 124), Formatted(STRING_STATUS_HP_MAX_2, 120)}));
	scene.Clear();
	scene.Modify(MODIFY_MAX_MP, 92);
	scene.Modify(MODIFY_MAX_MP, 93);
	CHECK((calls == Calls{"mp|30|92", "info mp max|92", "mp|30|93", "info mp max|93"}));
	CHECK((scene.Messages() == Calls{Formatted(STRING_STATUS_MP_MAX_1, 92), Formatted(STRING_STATUS_MP_MAX_2, 93)}));
	// An ouster's is EP.
	scene.Clear();
	ousters = true;
	scene.Modify(MODIFY_MAX_MP, 95);
	scene.Modify(MODIFY_MAX_MP, 96);
	CHECK((scene.Messages() == Calls{Formatted(STRING_STATUS_EP_MAX_1, 95), Formatted(STRING_STATUS_EP_MAX_2, 96)}));
	// Every last digit picks its form.
	Distinct(STRING_STATUS_HP_MAX_1, STRING_STATUS_HP_MAX_2);
	Distinct(STRING_STATUS_MP_MAX_1, STRING_STATUS_MP_MAX_2);
	Distinct(STRING_STATUS_EP_MAX_1, STRING_STATUS_EP_MAX_2);
	for (int value = 100; value < 110; ++value)
	{
		scene.Clear();
		scene.Modify(MODIFY_MAX_HP, value);
		ousters = false;
		scene.Modify(MODIFY_MAX_MP, value);
		ousters = true;
		scene.Modify(MODIFY_MAX_MP, value);
		CHECK((scene.Messages() == Calls{Formatted(FirstForm(value) ? STRING_STATUS_HP_MAX_1 : STRING_STATUS_HP_MAX_2, value),
			Formatted(FirstForm(value) ? STRING_STATUS_MP_MAX_1 : STRING_STATUS_MP_MAX_2, value),
			Formatted(FirstForm(value) ? STRING_STATUS_EP_MAX_1 : STRING_STATUS_EP_MAX_2, value)}));
	}
	// Out of the game, nothing is told.
	scene.Clear();
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Modify(MODIFY_MAX_HP, 124);
	scene.Modify(MODIFY_MAX_MP, 124);
	CHECK(scene.Messages().empty());
	CHECK_EQ(4u, calls.size());
}

TEST(ModifyStatus, DurabilityGoesToTheSlotOrTheRightHand)
{
	struct Case { Race race; int slot; int rightHand; MItem::ITEM_GEAR_SLOT twoHands; };
	const Case cases[] = {
		{RACE_SLAYER, MSlayerGear::GEAR_SLAYER_LEFTHAND, MSlayerGear::GEAR_SLAYER_RIGHTHAND, MItem::SLOT_TWOHAND},
		{RACE_VAMPIRE, MVampireGear::GEAR_VAMPIRE_LEFTHAND, MVampireGear::GEAR_VAMPIRE_RIGHTHAND, MItem::SLOT_VAMPIRE_TWOHAND},
		{RACE_OUSTERS, MOustersGear::GEAR_OUSTERS_LEFTHAND, MOustersGear::GEAR_OUSTERS_RIGHTHAND, MItem::SLOT_OUSTERS_TWOHAND},
	};
	for (const Case& c : cases)
	{
		Scene scene(c.race);
		MSlotItemManager& slots = *static_cast<MSlotItemManager*>(gear);
		Ware* shield = new Ware(ITEM_CLASS_POTION, 1);
		CHECK(slots.AddItem(shield, static_cast<BYTE>(c.slot)));
		// The slot in the top byte, the durability in the rest.
		scene.Modify(MODIFY_DURABILITY, (DWORD(c.slot) << 24) | 0x012345);
		CHECK_EQ(0x012345u, shield->GetCurrentDurability());
		delete slots.RemoveItem(static_cast<BYTE>(c.slot));
		// A two-handed weapon sent as the other hand's is the right hand's.
		Ware* weapon = new Ware(ITEM_CLASS_POTION, 2, c.twoHands);
		CHECK(slots.AddItem(weapon, static_cast<BYTE>(c.rightHand)));
		CHECK(slots.AddItem(new Ware(ITEM_CLASS_POTION, 3, c.twoHands), static_cast<BYTE>(c.slot)));
		scene.Modify(MODIFY_DURABILITY, (DWORD(c.slot) << 24) | 77);
		CHECK_EQ(77u, weapon->GetCurrentDurability());
		CHECK_EQ(0u, slots.GetItem(static_cast<BYTE>(c.slot))->GetCurrentDurability());
		// An empty slot changes nothing.
		delete slots.RemoveItem(static_cast<BYTE>(c.slot));
		scene.Modify(MODIFY_DURABILITY, (DWORD(c.slot) << 24) | 5);
		CHECK_EQ(77u, weapon->GetCurrentDurability());
	}
}

TEST(ModifyStatus, TheSheetsExperienceValues)
{
	Scene scene;
	scene.Modify(MODIFY_STR_EXP_REMAIN, 11);
	scene.Modify(MODIFY_DEX_EXP_REMAIN, 12);
	scene.Modify(MODIFY_INT_EXP_REMAIN, 13);
	scene.Modify(MODIFY_VAMP_EXP_REMAIN, 14);
	scene.Modify(MODIFY_OUSTERS_EXP_REMAIN, 15);
	scene.Modify(MODIFY_RANK_EXP_REMAIN, 16);
	CHECK((calls == Calls{Sheet(StatusPresentation::SHEET_STR_EXP_REMAIN, 11), Sheet(StatusPresentation::SHEET_DEX_EXP_REMAIN, 12),
		Sheet(StatusPresentation::SHEET_INT_EXP_REMAIN, 13), Sheet(StatusPresentation::SHEET_EXP_REMAIN, 14),
		Sheet(StatusPresentation::SHEET_EXP_REMAIN, 15), Sheet(StatusPresentation::SHEET_GRADE_EXP_REMAIN, 16)}));
}

TEST(ModifyStatus, TheSheetsBasicAndMaximumStats)
{
	Scene scene;
	scene.Modify(MODIFY_BASIC_STR, 21);
	scene.Modify(MODIFY_MAX_STR, 22);
	scene.Modify(MODIFY_BASIC_DEX, 23);
	scene.Modify(MODIFY_MAX_DEX, 24);
	scene.Modify(MODIFY_BASIC_INT, 25);
	scene.Modify(MODIFY_MAX_INT, 26);
	CHECK((calls == Calls{Sheet(StatusPresentation::SHEET_STR_PURE, 21), Sheet(StatusPresentation::SHEET_STR_MAX, 22),
		Sheet(StatusPresentation::SHEET_DEX_PURE, 23), "regen", Sheet(StatusPresentation::SHEET_DEX_MAX, 24),
		Sheet(StatusPresentation::SHEET_INT_PURE, 25), Sheet(StatusPresentation::SHEET_INT_MAX, 26)}));
}

TEST(ModifyStatus, TheMagazinesBullets)
{
	Scene scene;
	scene.Modify(MODIFY_BULLET, 9);			// no magazine: nothing
	Ware magazine(ITEM_CLASS_MAGAZINE, 5);
	g_pCurrentMagazine = reinterpret_cast<MMagazine*>(static_cast<MItem*>(&magazine));
	scene.Modify(MODIFY_BULLET, 9);
	CHECK_EQ(9, magazine.GetNumber());
	g_pCurrentMagazine = NULL;
}

TEST(ModifyStatus, ASlayersWeaponTakesTheSilver)
{
	Scene scene(RACE_SLAYER);
	scene.Modify(MODIFY_SILVER_DURABILITY, 300);		// nothing in hand
	Ware* weapon = new Ware(ITEM_CLASS_SWORD, 8);
	CHECK(static_cast<MSlotItemManager&>(scene.slayerGear).AddItem(weapon, MSlayerGear::GEAR_SLAYER_RIGHTHAND));
	scene.Modify(MODIFY_SILVER_DURABILITY, 300);
	CHECK_EQ(300, weapon->GetSilver());
	race = RACE_VAMPIRE;
	scene.Modify(MODIFY_SILVER_DURABILITY, 100);
	CHECK_EQ(300, weapon->GetSilver());
}

TEST(ModifyStatus, VisionKeepsWhatThePlayerSees)
{
	Scene scene;
	sight = 9;
	scene.Modify(MODIFY_VISION, 9);
	CHECK((calls == Calls{"sight|9", "keep|40|50|9"}));
}

TEST(ModifyStatus, GoldIsTheMoney)
{
	Scene scene;
	scene.Modify(MODIFY_GOLD, 4321);
	CHECK_EQ(4321, scene.money.GetMoney());
}

TEST(ModifyStatus, CurrentStrengthAndDexterityRecheckTheItems)
{
	Scene scene(RACE_SLAYER);
	CHECK(scene.inventory.AddItem(new Ware(ITEM_CLASS_POTION, 31), 0, 0));
	CHECK(static_cast<MSlotItemManager&>(scene.slayerGear).AddItem(new Ware(ITEM_CLASS_POTION, 32), 0));
	scene.Clear();
	scene.Modify(MODIFY_CURRENT_STR, 32);
	CHECK((calls == Calls{"info str|32", "refresh|31", "refresh|32", "calculate"}));
	scene.Clear();
	scene.Modify(MODIFY_CURRENT_DEX, 30);
	CHECK((calls == Calls{"info dex|30", "refresh|31", "refresh|32", "calculate"}));
	scene.Modify(MODIFY_CURRENT_STR, 30);
	scene.Modify(MODIFY_CURRENT_DEX, 39);
	CHECK((scene.Messages() == Calls{Formatted(STRING_STATUS_DEX_2, 30), Formatted(STRING_STATUS_STR_2, 30), Formatted(STRING_STATUS_DEX_1, 39)}));
	scene.Clear();
	scene.Modify(MODIFY_CURRENT_STR, 34);
	CHECK((scene.Messages() == Calls{Formatted(STRING_STATUS_STR_1, 34)}));
	scene.Clear();
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Modify(MODIFY_CURRENT_STR, 34);
	scene.Modify(MODIFY_CURRENT_DEX, 34);
	CHECK(scene.Messages().empty());
}

TEST(ModifyStatus, EachStatsLastDigitPicksItsMessage)
{
	Scene scene;
	Distinct(STRING_STATUS_STR_1, STRING_STATUS_STR_2);
	Distinct(STRING_STATUS_DEX_1, STRING_STATUS_DEX_2);
	Distinct(STRING_STATUS_INT_1, STRING_STATUS_INT_2);
	for (int value = 20; value < 30; ++value)
	{
		scene.Clear();
		scene.Modify(MODIFY_CURRENT_STR, value);
		scene.Modify(MODIFY_CURRENT_DEX, value);
		scene.Modify(MODIFY_CURRENT_INT, value);
		CHECK((scene.Messages() == Calls{Formatted(FirstForm(value) ? STRING_STATUS_STR_1 : STRING_STATUS_STR_2, value),
			Formatted(FirstForm(value) ? STRING_STATUS_DEX_1 : STRING_STATUS_DEX_2, value),
			Formatted(FirstForm(value) ? STRING_STATUS_INT_1 : STRING_STATUS_INT_2, value)}));
	}
}

TEST(ModifyStatus, CurrentIntelligenceRechecksTheRacesGear)
{
	for (Race playerRace : {RACE_SLAYER, RACE_VAMPIRE, RACE_OUSTERS})
	{
		Scene scene(playerRace);
		CHECK(scene.inventory.AddItem(new Ware(ITEM_CLASS_POTION, 41), 0, 0));
		CHECK(static_cast<MSlotItemManager&>(scene.slayerGear).AddItem(new Ware(ITEM_CLASS_POTION, 42), 0));
		CHECK(static_cast<MSlotItemManager&>(scene.vampireGear).AddItem(new Ware(ITEM_CLASS_POTION, 43), 0));
		CHECK(static_cast<MSlotItemManager&>(scene.oustersGear).AddItem(new Ware(ITEM_CLASS_POTION, 44), 0));
		scene.Clear();
		scene.Modify(MODIFY_CURRENT_INT, 25);
		const std::string own = playerRace == RACE_SLAYER ? "refresh|42" : playerRace == RACE_VAMPIRE ? "refresh|43" : "refresh|44";
		CHECK((calls == Calls{"info int|25", "refresh|41", own, "calculate"}));
		CHECK((scene.Messages() == Calls{Formatted(STRING_STATUS_INT_1, 25)}));
		// A vampire's skill costs follow its INT.
		CHECK_EQ(playerRace == RACE_VAMPIRE ? 1 : 0, lightSights);
	}
	Scene scene;
	scene.Modify(MODIFY_CURRENT_INT, 26);
	CHECK((scene.Messages() == Calls{Formatted(STRING_STATUS_INT_2, 26)}));
}

TEST(ModifyStatus, TheCharacterWindowsOtherValues)
{
	Scene scene;
	scene.Modify(MODIFY_FAME, 1);
	scene.Modify(MODIFY_TOHIT, 2);
	scene.Modify(MODIFY_DEFENSE, 3);
	scene.Modify(MODIFY_PROTECTION, 4);
	scene.Modify(MODIFY_SKILL_BONUS_POINT, 5);
	CHECK((calls == Calls{"info fame|1", "info tohit|2", "info defense|3", "info protection|4", "skill points|5"}));
}

TEST(ModifyStatus, BonusPointsMarkALevelUp)
{
	Scene scene;
	scene.Modify(MODIFY_BONUS_POINT, 3);
	scene.Modify(MODIFY_BONUS_POINT, 0);
	CHECK((calls == Calls{"level up", "bonus|3", "bonus|0"}));
}

TEST(ModifyStatus, AttackSpeedIsThePlayers)
{
	Scene scene;
	scene.Modify(MODIFY_ATTACK_SPEED, 2);
	CHECK((calls == Calls{"speed|2"}));
}

TEST(ModifyStatus, ADomainsLevelIsTold)
{
	struct Case { int part; int domain; ACTIONINFO root; };
	const Case cases[] = {
		{MODIFY_SWORD_DOMAIN_LEVEL, SKILLDOMAIN_SWORD, SKILL_DOUBLE_IMPACT},
		{MODIFY_BLADE_DOMAIN_LEVEL, SKILLDOMAIN_BLADE, SKILL_SINGLE_BLOW},
		{MODIFY_ENCHANT_DOMAIN_LEVEL, SKILLDOMAIN_ENCHANT, MAGIC_CREATE_HOLY_WATER},
		{MODIFY_HEAL_DOMAIN_LEVEL, SKILLDOMAIN_HEAL, MAGIC_CURE_LIGHT_WOUNDS},
		{MODIFY_GUN_DOMAIN_LEVEL, SKILLDOMAIN_GUN, SKILL_FAST_RELOAD},
	};
	for (const Case& c : cases)
	{
		Scene scene;
		testfw::MutableRow(scene.info, c.root).SetLearnLevel(20);
		const char* name = (*g_pGameStringTable)[SKILLDOMAIN_NAME[c.domain]].GetString();
		scene.Modify(c.part, 12);
		CHECK_EQ(12, scene.skills.GetMutable(c.domain)->GetDomainLevel());
		CHECK((scene.Messages() == Calls{Formatted(STRING_MESSAGE_NEW_DOMAIN_LEVEL_1, name, 12)}));
		scene.Clear();
		// At the root skill's learning level, a new skill can be learned.
		scene.Modify(c.part, 20);
		CHECK((scene.Messages() == Calls{Formatted(STRING_MESSAGE_NEW_DOMAIN_LEVEL_2, name, 20),
			(*g_pGameStringTable)[STRING_MESSAGE_NEW_SKILL_AVAILABLE].GetString()}));
		CHECK(calls.empty());
	}
}

TEST(ModifyStatus, SomeDomainLevelsCallForHelp)
{
	Scene scene;
	testfw::MutableRow(scene.info, SKILL_DOUBLE_IMPACT).SetLearnLevel(99);
	const struct { int level; int help; } cases[] = {
		{4, -1}, {5, HELP_EVENT_LEARN_SKILL}, {6, -1}, {29, -1}, {30, HELP_EVENT_LEVEL_30},
		{31, HELP_EVENT_LEVEL_31}, {32, HELP_EVENT_LEVEL_32}, {33, -1},
	};
	for (const auto& c : cases)
	{
		scene.Clear();
		scene.Modify(MODIFY_SWORD_DOMAIN_LEVEL, c.level);
		CHECK((calls == (c.help < 0 ? Calls{} : Calls{"help|" + N(c.help)})));
	}
	// Every last digit picks its message.
	Distinct(STRING_MESSAGE_NEW_DOMAIN_LEVEL_1, STRING_MESSAGE_NEW_DOMAIN_LEVEL_2);
	const char* name = (*g_pGameStringTable)[SKILLDOMAIN_NAME[SKILLDOMAIN_SWORD]].GetString();
	for (int level = 40; level < 50; ++level)
	{
		scene.Clear();
		scene.Modify(MODIFY_SWORD_DOMAIN_LEVEL, level);
		CHECK((scene.Messages() == Calls{Formatted(FirstForm(level) ? STRING_MESSAGE_NEW_DOMAIN_LEVEL_1 : STRING_MESSAGE_NEW_DOMAIN_LEVEL_2, name, level)}));
	}
	// Out of the game, the level is kept and nothing told.
	scene.Clear();
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Modify(MODIFY_SWORD_DOMAIN_LEVEL, 30);
	CHECK_EQ(30, scene.skills.GetMutable(SKILLDOMAIN_SWORD)->GetDomainLevel());
	CHECK(calls.empty());
	CHECK(scene.Messages().empty());
}

TEST(ModifyStatus, SomeSkillsNeverCountAsNew)
{
	// Learned skills, those past the level, and a few by name do not.
	Scene scene;
	MSkillDomain& sword = *scene.skills.GetMutable(SKILLDOMAIN_SWORD);
	testfw::MutableRow(scene.info, SKILL_DOUBLE_IMPACT).SetLearnLevel(99);
	for (ACTIONINFO excluded : {SKILL_MAGIC_ELUSION, SKILL_ILLUSION_OF_AVENGE, SKILL_WILL_OF_LIFE, SKILL_POISON_MESH, SKILL_DELEO_EFFICIO, SKILL_REPUTO_FACTUM})
	{
		testfw::MutableRow(scene.info, excluded).SetLearnLevel(10);
		sword.AddSkill(excluded);
	}
	testfw::MutableRow(scene.info, SKILL_TRIPLE_SLASHER).SetLearnLevel(10);
	sword.AddSkill(SKILL_TRIPLE_SLASHER);
	sword.SetNewSkill();
	CHECK(sword.LearnSkill(SKILL_TRIPLE_SLASHER));
	testfw::MutableRow(scene.info, SKILL_CROSS_COUNTER).SetLearnLevel(-1);
	sword.AddSkill(SKILL_CROSS_COUNTER);
	scene.Modify(MODIFY_SWORD_DOMAIN_LEVEL, 50);
	CHECK_EQ(1u, scene.Messages().size());
	testfw::MutableRow(scene.info, SKILL_DOUBLE_IMPACT).SetLearnLevel(50);
	scene.Modify(MODIFY_SWORD_DOMAIN_LEVEL, 50);
	CHECK_EQ(3u, scene.Messages().size());
}

TEST(ModifyStatus, TheLevelIsTheVampireDomains)
{
	Scene scene(RACE_VAMPIRE);
	testfw::MutableRow(scene.info, MAGIC_HIDE).SetLearnLevel(99);
	CHECK(scene.inventory.AddItem(new Ware(ITEM_CLASS_POTION, 51), 0, 0));
	CHECK(static_cast<MSlotItemManager&>(scene.vampireGear).AddItem(new Ware(ITEM_CLASS_POTION, 52), 0));
	scene.Clear();
	scene.Modify(MODIFY_LEVEL, 30);
	CHECK_EQ(30, scene.skills.GetMutable(SKILLDOMAIN_VAMPIRE)->GetDomainLevel());
	CHECK((calls == Calls{"help|" + N(HELP_EVENT_LEVEL_30), "info level|30", "refresh|51", "refresh|52"}));
	CHECK((scene.Messages() == Calls{Formatted(STRING_STATUS_LEVEL, 30)}));
	scene.Clear();
	testfw::MutableRow(scene.info, MAGIC_HIDE).SetLearnLevel(5);
	scene.Modify(MODIFY_LEVEL, 5);
	CHECK((calls == Calls{"help|" + N(HELP_EVENT_LEARN_SKILL), "info level|5", "refresh|51", "refresh|52"}));
	CHECK((scene.Messages() == Calls{Formatted(STRING_STATUS_LEVEL, 5), (*g_pGameStringTable)[STRING_MESSAGE_NEW_SKILL_AVAILABLE].GetString()}));
	for (int level : {31, 32, 33})
		scene.Modify(MODIFY_LEVEL, level);
	CHECK((calls == Calls{"help|" + N(HELP_EVENT_LEARN_SKILL), "info level|5", "refresh|51", "refresh|52",
		"help|" + N(HELP_EVENT_LEVEL_31), "info level|31", "refresh|51", "refresh|52",
		"help|" + N(HELP_EVENT_LEVEL_32), "info level|32", "refresh|51", "refresh|52",
		"info level|33", "refresh|51", "refresh|52"}));
	// Out of the game: the domain and the items only.
	scene.Clear();
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Modify(MODIFY_LEVEL, 30);
	CHECK((calls == Calls{"refresh|51", "refresh|52"}));
	CHECK(scene.Messages().empty());
}

TEST(ModifyStatus, TheAlignmentsStepAndValue)
{
	Scene scene;
	const auto good = std::string((*g_pGameStringTable)[STRING_CHANGE_TO_GOOD_ALIGNMENT].GetString());
	const auto bad = std::string((*g_pGameStringTable)[STRING_CHANGE_TO_BAD_ALIGNMENT].GetString());
	// Up within neutral: the value, and that it went up.
	CHECK_EQ(2000u, scene.Modify(MODIFY_ALIGNMENT, 2000, 1000).newValue);
	CHECK((calls == Calls{Sheet(StatusPresentation::SHEET_ALIGNMENT_NUM, 2000)}));
	CHECK((scene.Messages() == Calls{good}));
	// Down within neutral: that it went down, and the fame help.
	scene.Clear();
	scene.Modify(MODIFY_ALIGNMENT, 1000, 2000);
	CHECK((calls == Calls{Sheet(StatusPresentation::SHEET_ALIGNMENT_NUM, 1000), "help|" + N(HELP_EVENT_FAME)}));
	CHECK((scene.Messages() == Calls{bad}));
	// Down into evil (a negative value comes as 16 bits): the step changes.
	// Recorded, not changed: the two values are compared unsigned once
	// negative, so this move reads as one towards good.
	scene.Clear();
	const MODIFY_VALUE down = scene.Modify(MODIFY_ALIGNMENT, 65536 - 3000, 2000);
	CHECK_EQ(DWORD(-3000), down.newValue);
	char changed[256];
	SafeFormat::Format(changed, GetGameString(STRING_CHANGE_ALIGNMENT2), "Evil");
	CHECK((calls == Calls{Sheet(StatusPresentation::SHEET_ALIGNMENT_NUM, -3000), Sheet(StatusPresentation::SHEET_ALIGNMENT, 1)}));
	CHECK((scene.Messages() == Calls{good, changed}));
	// From evil+ (16 bits) to good+: the good string for the step, and
	// (recorded, not changed) a move towards evil.
	scene.Clear();
	scene.Modify(MODIFY_ALIGNMENT, 9000, 65536 - 9000);
	SafeFormat::Format(changed, GetGameString(STRING_CHANGE_ALIGNMENT), "Good+");
	CHECK((calls == Calls{Sheet(StatusPresentation::SHEET_ALIGNMENT_NUM, 9000), "help|" + N(HELP_EVENT_FAME), Sheet(StatusPresentation::SHEET_ALIGNMENT, 4)}));
	CHECK((scene.Messages() == Calls{bad, changed}));
	// From one negative to a lower one: told as a move towards evil.
	scene.Clear();
	scene.Modify(MODIFY_ALIGNMENT, 65536 - 10000, 65536 - 9000);
	CHECK((scene.Messages() == Calls{bad}));
	// The same: nothing said.
	scene.Clear();
	scene.Modify(MODIFY_ALIGNMENT, 9000, 9000);
	CHECK(scene.Messages().empty());
	CHECK_EQ(1u, calls.size());
	// Out of the game the step is not shown either.
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Modify(MODIFY_ALIGNMENT, 0, 9000);
	CHECK(scene.Messages().empty());
	CHECK_EQ(2u, calls.size());
}

TEST(ModifyStatus, AnAlignmentsSteps)
{
	const struct { int value; int step; } cases[] = {
		{-10000, 0}, {-7501, 0}, {-7500, 1}, {-2501, 1}, {-2500, 2}, {0, 2}, {2500, 2}, {2501, 3},
		{7500, 3}, {7501, 4}, {10000, 4},
		{65536 - 7501, 0}, {65536 - 7500, 1}, {65535, 2},		// negative values sent as 16 bits
		{20000, 0}, {32767, 0},								// above 10000, not a 16-bit negative: past the steps
	};
	for (const auto& c : cases)
		CHECK_EQ(c.step, ConvertAlignment(c.value));
}

TEST(ModifyStatus, ADomainsExperience)
{
	Scene scene;
	const struct { int part; int domain; } cases[] = {
		{MODIFY_SWORD_DOMAIN_EXP_REMAIN, SKILLDOMAIN_SWORD}, {MODIFY_BLADE_DOMAIN_EXP_REMAIN, SKILLDOMAIN_BLADE},
		{MODIFY_HEAL_DOMAIN_EXP_REMAIN, SKILLDOMAIN_HEAL}, {MODIFY_ENCHANT_DOMAIN_EXP_REMAIN, SKILLDOMAIN_ENCHANT},
		{MODIFY_GUN_DOMAIN_EXP_REMAIN, SKILLDOMAIN_GUN}, {MODIFY_ETC_DOMAIN_EXP_REMAIN, SKILLDOMAIN_ETC},
	};
	DWORD value = 100;
	for (const auto& c : cases)
	{
		scene.Modify(c.part, value);
		CHECK_EQ(value, scene.skills.GetMutable(c.domain)->GetDomainExpRemain());
		++value;
	}
	CHECK(calls.empty());
}

TEST(ModifyStatus, DamageRecomputesTheStatus)
{
	Scene scene;
	status.SetStatus(MODIFY_CURRENT_HP, 60);
	status.SetStatus(MODIFY_MAX_HP, 90);
	scene.Modify(MODIFY_MIN_DAMAGE, 1);
	scene.Modify(MODIFY_MAX_DAMAGE, 2);
	scene.Modify(MODIFY_SILVER_DAMAGE, 7);
	CHECK((calls == Calls{"calculate", "calculate", "info silver|7", "hp|60|90"}));
}

TEST(ModifyStatus, TheGuildAndTheRank)
{
	Scene scene;
	scene.Modify(MODIFY_GUILDID, 0x12345);
	CHECK((calls == Calls{"guild|" + N(0x12345), Sheet(StatusPresentation::SHEET_GUILD_ID, 0x12345)}));
	scene.Clear();
	scene.Modify(MODIFY_RANK, 4, 3);
	scene.Modify(MODIFY_RANK, 3, 4);
	scene.Modify(MODIFY_RANK, 4, 4);
	CHECK((calls == Calls{Sheet(StatusPresentation::SHEET_GRADE, 4), Sheet(StatusPresentation::SHEET_GRADE, 3), Sheet(StatusPresentation::SHEET_GRADE, 4)}));
	CHECK((scene.Messages() == Calls{(*g_pGameStringTable)[STRING_MESSAGE_UP_TO_GRADE].GetString()}));
}

TEST(ModifyStatus, AnOustersElements)
{
	Scene scene;
	scene.Modify(MODIFY_ELEMENTAL_FIRE, 1);
	scene.Modify(MODIFY_ELEMENTAL_WATER, 2);
	scene.Modify(MODIFY_ELEMENTAL_EARTH, 3);
	scene.Modify(MODIFY_ELEMENTAL_WIND, 4);
	CHECK((calls == Calls{"element|" + N(ITEMTABLE_INFO::ELEMENTAL_TYPE_FIRE) + "|1", "element|" + N(ITEMTABLE_INFO::ELEMENTAL_TYPE_WATER) + "|2",
		"element|" + N(ITEMTABLE_INFO::ELEMENTAL_TYPE_EARTH) + "|3", "element|" + N(ITEMTABLE_INFO::ELEMENTAL_TYPE_WIND) + "|4"}));
}

TEST(ModifyStatus, ASkillsLevelAndExperience)
{
	Scene scene;
	const SKILLINFO_NODE& row = scene.info[SKILL_DOUBLE_IMPACT];
	scene.Modify(MODIFY_SKILL_LEVEL, (DWORD(SKILL_DOUBLE_IMPACT) << 16) | 24);
	CHECK_EQ(24, row.GetExpLevel());
	scene.Modify(MODIFY_SKILL_LEVEL, (DWORD(SKILL_DOUBLE_IMPACT) << 16) | 25);
	CHECK((scene.Messages() == Calls{Formatted(STRING_MESSAGE_MODIFY_SKILL_LEVEL_2, row.GetHName(), 24),
		Formatted(STRING_MESSAGE_MODIFY_SKILL_LEVEL_2, row.GetHName(), 25)}));
	scene.Clear();
	scene.Modify(MODIFY_SKILL_LEVEL, (DWORD(SKILL_DOUBLE_IMPACT) << 16) | 26);
	CHECK((scene.Messages() == Calls{Formatted(STRING_MESSAGE_MODIFY_SKILL_LEVEL_1, row.GetHName(), 26)}));
	Distinct(STRING_MESSAGE_MODIFY_SKILL_LEVEL_1, STRING_MESSAGE_MODIFY_SKILL_LEVEL_2);
	for (int level = 10; level < 20; ++level)
	{
		scene.Clear();
		scene.Modify(MODIFY_SKILL_LEVEL, (DWORD(SKILL_DOUBLE_IMPACT) << 16) | DWORD(level));
		CHECK((scene.Messages() == Calls{Formatted(FirstForm(level) ? STRING_MESSAGE_MODIFY_SKILL_LEVEL_2 : STRING_MESSAGE_MODIFY_SKILL_LEVEL_1, row.GetHName(), level)}));
	}
	// The experience, in tens.
	scene.Modify(MODIFY_SKILL_EXP, (DWORD(SKILL_DOUBLE_IMPACT) << 16) | 7);
	CHECK_EQ(70, row.GetSkillExp());
	// Past the skills, or out of the game, nothing is told.
	scene.Clear();
	scene.Modify(MODIFY_SKILL_LEVEL, (DWORD(MAX_ACTIONINFO) << 16) | 3);
	scene.Modify(MODIFY_SKILL_EXP, (DWORD(MAX_ACTIONINFO) << 16) | 3);
	g_Mode = MODE_WAIT_UPDATEINFO;
	scene.Modify(MODIFY_SKILL_LEVEL, (DWORD(SKILL_DOUBLE_IMPACT) << 16) | 30);
	CHECK_EQ(30, row.GetExpLevel());
	CHECK(scene.Messages().empty());
}

TEST(ModifyStatus, APetsHealthWarnsAndKills)
{
	Scene scene;
	const std::string refill = (*g_pGameStringTable)[STRING_MESSAGE_PET_REQUEST_REFILL].GetString();
	// No player ID (even with a pet there), no pet, or a pet without its
	// item: nothing.
	MPetItem pet;
	pet.SetCurrentDurability(500);
	petID = 900;
	petItem = &pet;
	playerID = OBJECTID_NULL;
	scene.Modify(MODIFY_PET_HP, 0);
	scene.Modify(MODIFY_PET_EXP_REMAIN, 0);
	CHECK_EQ(500u, pet.GetCurrentDurability());
	playerID = 77;
	petInZone = false;
	scene.Modify(MODIFY_PET_HP, 0);
	petInZone = true;
	petItem = NULL;
	scene.Modify(MODIFY_PET_HP, 0);
	CHECK(calls.empty());
	petItem = &pet;
	scene.Modify(MODIFY_PET_HP, 61);
	CHECK_EQ(61u, pet.GetCurrentDurability());
	CHECK(Lines(scene.systemLog).empty());
	// An hour, ten minutes left: warnings; none left: the pet dies.
	char left[512], warning[512];
	scene.Modify(MODIFY_PET_HP, 60);
	SafeFormat::Format(left, GetGameString(UI_STRING_MESSAGE_HOUR), 1);
	SafeFormat::Format(warning, GetGameString(STRING_MESSAGE_PET_DIE_WARNING), "Rex", left);
	CHECK((Lines(scene.systemLog) == Calls{warning, refill}));
	scene.Clear();
	scene.Modify(MODIFY_PET_HP, 10);
	SafeFormat::Format(left, GetGameString(UI_STRING_MESSAGE_MINUTE), 10);
	SafeFormat::Format(warning, GetGameString(STRING_MESSAGE_PET_DIE_WARNING), "Rex", left);
	CHECK((Lines(scene.systemLog) == Calls{warning, refill}));
	CHECK(calls.empty());
	scene.Modify(MODIFY_PET_HP, 0);
	CHECK((calls == Calls{"dead"}));
	CHECK_EQ(0u, pet.GetCurrentDurability());
	petItem = NULL;
}

TEST(ModifyStatus, APetsExperienceIsWhatIsLeft)
{
	Scene scene;
	MPetItem pet;
	pet.SetNumber(3);
	petID = 900;
	petItem = &pet;
	const DWORD accum = g_pExperienceTable != NULL ? g_pExperienceTable->GetPetExp(3).AccumExp : 0;
	ExperienceTable table;
	ExperienceTable* oldTable = g_pExperienceTable;
	g_pExperienceTable = &table;
	scene.Modify(MODIFY_PET_EXP_REMAIN, 5);
	CHECK_EQ(DWORD(table.GetPetExp(3).AccumExp - 5), pet.GetPetExpRemain());
	(void)accum;
	// Without a pet in the zone, nothing.
	petInZone = false;
	scene.Modify(MODIFY_PET_EXP_REMAIN, 1);
	CHECK_EQ(DWORD(table.GetPetExp(3).AccumExp - 5), pet.GetPetExpRemain());
	g_pExperienceTable = oldTable;
	petItem = NULL;
}

TEST(ModifyStatus, TheUnionAndTheAdvancementNeedAPlayer)
{
	Scene scene;
	scene.Modify(MODIFY_UNIONID, 81);
	scene.Modify(MODIFY_UNIONGRADE, 2);
	scene.Modify(MODIFY_ADVANCEMENT_CLASS_LEVEL, 3);
	scene.Modify(MODIFY_ADVANCEMENT_CLASS_GOAL_EXP, 4);
	CHECK((calls == Calls{"union|81", Sheet(StatusPresentation::SHEET_ADVANCEMENT_LEVEL, 3), Sheet(StatusPresentation::SHEET_EXP_REMAIN, 4)}));
	CHECK_EQ(81u, g_pUserInformation->dwUnionID);
	CHECK_EQ(2, g_pUserInformation->bUnionGrade);
	scene.Clear();
	noPlayer = true;
	scene.Modify(MODIFY_UNIONID, 91);
	scene.Modify(MODIFY_UNIONGRADE, 9);
	scene.Modify(MODIFY_ADVANCEMENT_CLASS_LEVEL, 9);
	scene.Modify(MODIFY_ADVANCEMENT_CLASS_GOAL_EXP, 9);
	CHECK(calls.empty());
	CHECK_EQ(81u, g_pUserInformation->dwUnionID);
	CHECK_EQ(2, g_pUserInformation->bUnionGrade);
}

TEST(ModifyStatus, WithoutAPresentationHostNothingIsShown)
{
	Scene scene;
	StatusPresentation::SetHost(nullptr);
	status.SetStatus(MODIFY_MAX_HP, 100);
	scene.Modify(MODIFY_CURRENT_HP, 50);
	scene.Modify(MODIFY_BASIC_STR, 5);
	scene.Modify(MODIFY_BONUS_POINT, 2);
	CHECK(calls.empty());
}

