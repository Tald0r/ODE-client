#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "packet_stream_access.h"
#include "UIFunction.h"
#include "GameUIBuilders.h"
#include "MPlayer.h"
#include "AddonDef.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MEffectStatusTable.h"
#include "MEffectSpriteTypeTable.h"
#include "EffectSpriteTypeDef.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "MInventory.h"
#include "MZoneTable.h"
#include "MGuildInfoMapper.h"
#include "MGameTime.h"
#include "MMonsterKillQuestInfo.h"
#include "TalkBox.h"
#include "TempInformation.h"
#include "UIDialog.h"
#include "DialogWindowHost.h"
#include "DialogPresentationHost.h"
#include "WorldEventsHost.h"
#include "UserOption.h"
#include "GameMessages.h"
#include "CMessageArray.h"
#include "ClientMode.h"
#include "Netmarble.h"
#include "SoundDef.h"
#include "VS_UI_Base.h"
#include "VS_UI_title.h"
#include "Ci.h"
#include "Fl2.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackendSDL.h"
#include "timer2.h"
#include "SocketInputStream.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "Gpackets/GCSelectQuestID.h"
#include "Gpackets/GCWarScheduleList.h"
#include "Gpackets/GCBloodBibleStatus.h"
#include "Gpackets/GCQuestStatus.h"
#include "Gpackets/GCGoodsList.h"
#include "Gpackets/GCMiniGameScores.h"
#include <SDL.h>
#ifndef PLATFORM_WINDOWS
#include <unistd.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define ASAN_SEES_FREES 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__) && !defined(ASAN_SEES_FREES)
#define ASAN_SEES_FREES 1
#endif
#ifdef ASAN_SEES_FREES
#include <sanitizer/asan_interface.h>
#endif

// Defined in GameUI.cpp and MItemUse.cpp, declared by no header.
int GetMyLevelWarStair();
void UI_RunPetInfo(struct PETINFO* pPetInfo);
MItem* NewItemOfClass(ITEM_CLASS itemClass);
extern S_SLOT g_char_slot_ingame;
extern bool g_bPartyRunning;

// The game's calls into the UI, third part (GameUI.cpp from the quick
// slots to the window cursor; gameclient's since task 4.256): what each
// does to the model - the character sheet (g_char_slot_ingame), the
// player's wait, the request mode, the NPC talk box and the message box,
// the quests, the inventory's items, the system messages, the guild
// names, the login sent to the UI's queue and the cursor - and to its
// hosts (the windows, the item factory, the sounds). gC_vs_ui is never
// built here, so its game and title windows do not exist: every one of
// its calls these functions make is a guarded no-op, and its IsRunning*
// answer false. What a function hands only to those windows is not
// observable here; the last test checks those calls are harmless.
namespace {

std::vector<std::string> calls;
std::string N(long long value) { return std::to_string(value); }

bool Same(const std::vector<std::string>& expected, const std::vector<std::string>& actual = calls)
{
	if (actual == expected) return true;
	std::printf("      calls: ");
	for (const std::string& call : actual) std::printf("[%s] ", call.c_str());
	std::printf("\n      wanted:");
	for (const std::string& call : expected) std::printf("[%s] ", call.c_str());
	std::printf("\n");
	return false;
}

bool SameText(const std::string& expected, const std::string& actual)
{
	if (expected == actual) return true;
	std::printf("      text:   [%s]\n      wanted: [%s]\n", actual.c_str(), expected.c_str());
	return false;
}

std::string Text(const char* text) { return text != NULL ? text : "(null)"; }

// The windows are numbered in the order they open.
int windowCount = 0;
DialogWindow::Window* NewWindow() { return reinterpret_cast<DialogWindow::Window*>(static_cast<std::uintptr_t>(++windowCount)); }
std::string W(DialogWindow::Window* pWindow) { return N(static_cast<long long>(reinterpret_cast<std::uintptr_t>(pWindow))); }

const DialogWindow::Host windowHost = []() {
	DialogWindow::Host host{};
	host.NewTalk = [](int creatureType, const char* name) {
		DialogWindow::Window* pWindow = NewWindow();
		calls.push_back("talk " + W(pWindow) + "|" + N(creatureType) + "|" + Text(name));
		return pWindow;
	};
	host.NewMessage = [](int x, int y, int extraHeight, std::uint16_t buttons) {
		DialogWindow::Window* pWindow = NewWindow();
		calls.push_back("box " + W(pWindow) + "|" + N(x) + "|" + N(y) + "|" + N(extraHeight) + "|" + N(buttons));
		return pWindow;
	};
	host.SetMenu = [](DialogWindow::Window* pWindow, const DialogWindow::MenuItem* pItems, int count) {
		std::string line = "menu " + W(pWindow);
		for (int i = 0; i < count; ++i) line += "|" + N(pItems[i].id) + ":" + pItems[i].text;
		calls.push_back(line);
	};
	host.SetMessage = [](DialogWindow::Window* pWindow, const char* const* pLines, int count, bool noFit) {
		std::string line = "text " + W(pWindow) + (noFit ? " nofit" : " fit");
		for (int i = 0; i < count; ++i) line += std::string("|") + pLines[i];
		calls.push_back(line);
	};
	host.Start = [](DialogWindow::Window* pWindow) { calls.push_back("start " + W(pWindow)); };
	host.Finish = [](DialogWindow::Window* pWindow) { calls.push_back("finish " + W(pWindow)); };
	host.Delete = [](DialogWindow::Window* pWindow) { calls.push_back("delete " + W(pWindow)); };
	return host;
}();

const DialogPresentation::Host dialogHost = []() {
	DialogPresentation::Host host{};
	host.CloseAllDialog = []() { calls.push_back("close all"); };
	return host;
}();

const WorldEvents::Host worldHost = []() {
	WorldEvents::Host host{};
	host.PlaySound = [](TYPE_SOUNDID soundID) { calls.push_back("sound|" + N(soundID)); };
	return host;
}();

std::string Options(const MItem* pItem)
{
	std::string text;
	for (TYPE_ITEM_OPTION option : pItem->GetItemOptionList()) text += (text.empty() ? "" : ",") + N(option);
	return text;
}

// The items the factory made, in order; the class it refuses (none
// when -1); and what each of them held when it was freed, in the order
// they were: "index made|id|type|grade|options|number|quest", MItem's
// own fields, which are all a destroyed item still has.
std::vector<MItem*> made;
int refusedClass = -1;
std::vector<std::string> freed;
const MItemHost itemHost = []() {
	MItemHost host{};
	host.NewItem = [](ITEM_CLASS itemClass) {
		calls.push_back("new|" + N(itemClass));
		MItem* pItem = int(itemClass) == refusedClass ? nullptr : NewItemOfClass(itemClass);
		if (pItem != nullptr) made.push_back(pItem);
		return pItem;
	};
	host.RefreshAffect = [](MItem* pItem) { calls.push_back("affect|" + N(pItem->GetID())); };
	host.ItemDestroyed = [](MItem* pItem) noexcept {
		calls.push_back("destroyed|" + N(pItem->GetID()));
		for (size_t i = 0; i < made.size(); ++i)
			if (made[i] == pItem)
				freed.push_back(N(i) + "|" + N(pItem->GetID()) + "|" + N(pItem->GetItemType()) + "|" + N(pItem->GetGrade())
					+ "|" + Options(pItem) + "|" + N(pItem->GetNumber()) + "|" + N(pItem->IsQuestItem()));
	};
	return host;
}();

// A creature type of a tribe that is no race (NPC).
const int NPC_TYPE = 700;

// The game strings: "s<id>" each, the formats these functions use as the
// English table words them.
void FillGameStrings()
{
	g_pGameStringTable->Init(MAX_GAME_STRING);
	for (int id = 0; id < MAX_GAME_STRING; ++id) g_pGameStringTable->GetInternalPointer()[id] = ("s" + N(id)).c_str();
	auto set = [](int id, const char* text) { g_pGameStringTable->GetInternalPointer()[id] = text; };
	set(UI_STRING_MESSAGE_HOUR, "%dh");
	set(UI_STRING_MESSAGE_MINUTE, "%dm");
	set(STRING_MESSAGE_MONSTER_KILL_QUEST_STRING_SET, "Bring %s, %d of them, within %s.");
	set(STRING_MESSAGE_MONSTER_KILL_QUEST_STRING_SET_VAMPIRE, "Drain %s, %d of them, within %s.");
	set(STRING_MESSAGE_OPEN_LAIR, "%s has opened.");
	set(STRING_MESSAGE_CLOSED_LAIR, "%s has closed.");
	set(STRING_MESSAGE_LEFT_TIME_LAIR, "%s stays open for %d minutes.");
	set(STRING_MESSAGE_YES_I_SEE, "Yes");
	set(UI_STRING_MESSAGE_CANCEL_SELECT_QUEST, "Later");
	set(UI_STRING_MESSAGE_NUMBER_OF_ANIMALS, " heads");
	set(UI_STRING_MESSAGE_TIME_LIMIT, "Limit:");
}

std::string S(int id) { return "s" + N(id); }

// One quest record, as the quest file holds it, loaded into the manager.
template <class T>
void Put(std::string& bytes, T value) { bytes.append(reinterpret_cast<const char*>(&value), sizeof(T)); }

void AddQuest(MQuestInfoManager& quests, DWORD id, QUEST_INFO_TYPE type, DWORD timeLimit, DWORD creatureType,
	DWORD spriteType, DWORD goal, const std::string& name, BYTE eventQuest = 0, BYTE level = 0)
{
	std::string bytes;
	Put<int>(bytes, 1);
	Put<BYTE>(bytes, BYTE(type));
	Put<DWORD>(bytes, id);
	Put<DWORD>(bytes, timeLimit);
	Put<DWORD>(bytes, creatureType);
	Put<DWORD>(bytes, spriteType);
	Put<char>(bytes, 0);
	Put<DWORD>(bytes, goal);
	Put<BYTE>(bytes, eventQuest);
	Put<BYTE>(bytes, level);
	const std::uint32_t length = std::uint32_t(name.size());
	for (int i = 0; i < 4; ++i) bytes.push_back(char(length >> (8 * i)));
	bytes += name;
	std::stringbuf buffer(bytes, std::ios::in);
	std::ifstream file;
	file.std::ios::rdbuf(&buffer);
	quests.LoadFromFile(file);
}

// The quest ids the NPC offers, read from the wire as the server sends
// them.
std::unique_ptr<GCSelectQuestID> QuestIDs(std::initializer_list<QuestID_t> ids)
{
	std::vector<unsigned char> bytes;
	bytes.push_back(BYTE(ids.size()));
	for (QuestID_t id : ids)
		for (size_t i = 0; i < sizeof(QuestID_t); ++i) bytes.push_back((unsigned char)(std::uint64_t(id) >> (8 * i)));
	std::unique_ptr<GCSelectQuestID> packet(new GCSelectQuestID);
	Socket socket((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&socket, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), (unsigned int)bytes.size());
	packet->read(stream);
	CHECK(stream.isEmpty());
	return packet;
}

// Leaves the stack below the caller full of '#', so a buffer a callee
// never writes reads as a run of them with no end.
void DirtyStack()
{
	volatile char buffer[16384];
	for (size_t i = 0; i < sizeof buffer; ++i) buffer[i] = '#';
}

std::vector<std::string> Lines(CMessageArray& log)
{
	std::vector<std::string> lines;
	for (int i = 0; i < log.GetSize(); i++)
		if (log[i] != nullptr && log[i][0] != '\0') lines.push_back(log[i]);
	return lines;
}

struct Scene
{
	GameModelWorld world;
	CREATURE_TABLE creatures;
	CREATURESPRITE_TABLE sprites;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	RankBonusTable rankBonuses;
	PCTalkBox talkBox;
	TempInformation temp;
	UIDialog dialog;
	MQuestInfoManager quests;
	MInventory inventory;
	UserOption option;
	CMessageArray systemMessages;
	CZoneTable zones;
	MGuildInfoMapper guilds;
	MGameTime gameTime;
	CREATURE_TABLE* oldCreatures = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	PCTalkBox* oldTalkBox = g_pPCTalkBox;
	TempInformation* oldTemp = g_pTempInformation;
	UIDialog* oldDialog = g_pUIDialog;
	MQuestInfoManager* oldQuests = g_pQuestInfoManager;
	MInventory* oldInventory = g_pInventory;
	UserOption* oldOption = g_pUserOption;
	CMessageArray* oldSystemMessages = g_pSystemMessage;
	CZoneTable* oldZones = g_pZoneTable;
	MGuildInfoMapper* oldGuilds = g_pGuildInfoMapper;
	MGameTime* oldGameTime = g_pGameTime;
	MPlayer* oldPlayer = g_pPlayer;
	S_SLOT oldSlot = g_char_slot_ingame;
	int oldActive = g_bActiveGame;
	bool oldPartyRunning = g_bPartyRunning;
	const DialogWindow::Host* oldWindows = DialogWindow::SetHost(&windowHost);
	const DialogPresentation::Host* oldPresentation = DialogPresentation::SetHost(&dialogHost);
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&worldHost);
	MPlayer* me = nullptr;

	Scene()
	{
		calls.clear();
		made.clear();
		freed.clear();
		windowCount = 0;
		refusedClass = -1;
		MItem::SetHost(&itemHost);
		creatures.Init(800);
		Type(CREATURETYPE_SLAYER_MALE, CREATURETRIBE_SLAYER);
		Type(CREATURETYPE_VAMPIRE_MALE1, CREATURETRIBE_VAMPIRE);
		Type(CREATURETYPE_OUSTERS, CREATURETRIBE_OUSTERS);
		Type(NPC_TYPE, CREATURETRIBE_NPC);
		g_pCreatureTable = &creatures;
		sprites.Init(1);
		g_pCreatureSpriteTable = &sprites;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		g_pRankBonusTable = &rankBonuses;
		FillGameStrings();
		g_pPCTalkBox = &talkBox;
		g_pTempInformation = &temp;
		dialog.Init();
		g_pUIDialog = &dialog;
		g_pQuestInfoManager = &quests;
		inventory.Init(10, 6);
		g_pInventory = &inventory;
		g_pUserOption = &option;
		systemMessages.Init(4, 256);
		g_pSystemMessage = &systemMessages;
		g_pZoneTable = &zones;
		g_pGuildInfoMapper = &guilds;
		g_pGameTime = &gameTime;
		g_char_slot_ingame = S_SLOT();
		g_bActiveGame = 1;
		g_bPartyRunning = false;
		Items();
		me = new MPlayer;
		g_pPlayer = me;
		me->SetCreatureType(CREATURETYPE_SLAYER_MALE);
	}
	~Scene()
	{
		delete me;
		g_pPlayer = oldPlayer;
		inventory.Release();
		dialog.Release();
		talkBox.Release();
		MItem::SetHost(NULL);
		WorldEvents::SetHost(oldWorld);
		DialogPresentation::SetHost(oldPresentation);
		DialogWindow::SetHost(oldWindows);
		g_bPartyRunning = oldPartyRunning;
		g_bActiveGame = oldActive;
		g_char_slot_ingame = oldSlot;
		g_pGameTime = oldGameTime;
		g_pGuildInfoMapper = oldGuilds;
		g_pZoneTable = oldZones;
		g_pSystemMessage = oldSystemMessages;
		g_pUserOption = oldOption;
		g_pInventory = oldInventory;
		g_pQuestInfoManager = oldQuests;
		g_pUIDialog = oldDialog;
		g_pTempInformation = oldTemp;
		g_pPCTalkBox = oldTalkBox;
		g_pRankBonusTable = oldRankBonuses;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatures;
	}
	void Type(int type, CREATURETRIBE tribe)
	{
		testfw::MutableRow(creatures, type).SetCreatureTribe(tribe, 0);
		testfw::MutableRow(creatures, type).bMale = true;
	}
	// The item classes the tests make: a sword, a potion, two guns and
	// the magazines of the rifle, the shotgun and the sub-machine gun.
	void Items()
	{
		Class(ITEM_CLASS_SWORD, 3);
		Class(ITEM_CLASS_POTION, 1);
		Class(ITEM_CLASS_SG, 1);
		Class(ITEM_CLASS_SR, 1);
		Class(ITEM_CLASS_MAGAZINE, 3);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MAGAZINE, 0).Value1 = ITEM_CLASS_AR;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MAGAZINE, 1).Value1 = ITEM_CLASS_SG;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MAGAZINE, 2).Value1 = ITEM_CLASS_SMG;
	}
	void Class(int itemClass, int types)
	{
		g_pItemTable->InitClass(itemClass, types);
		for (int type = 0; type < types; ++type)
			testfw::MutableRow(*g_pItemTable, itemClass, type).SetGrid(1, 1);
	}
	// The player of that creature type.
	void Be(int creatureType) { me->SetCreatureType(TYPE_CREATURETYPE(creatureType)); }
	// A talk box with one line, open; the calls so far forgotten.
	void OpenTalk()
	{
		talkBox.Release();
		talkBox.SetType(PCTalkBox::NORMAL);
		talkBox.SetCreatureType(7);
		talkBox.AddString("Buy");
		dialog.PopupPCTalkDlg();
		CHECK_EQ(FLAG_UIDIALOG_LOCK_PCTALK, int(UIDialog::IsLockInput()));
		calls.clear();
	}
	// A sword with these options and grade, in the inventory at (x, 0)
	// unless x is negative.
	MItem* Sword(TYPE_OBJECTID id, std::initializer_list<TYPE_ITEM_OPTION> options, int grade, int x)
	{
		MItem* pItem = MItem::NewItem(ITEM_CLASS_SWORD);
		pItem->SetItemType(0);
		pItem->SetID(id);
		for (TYPE_ITEM_OPTION o : options) pItem->AddItemOption(o);
		pItem->SetGrade(grade);
		if (x >= 0) CHECK(inventory.AddItem(pItem, BYTE(x), 0));
		return pItem;
	}
	// The message box that is open: its box, text and start.
	std::vector<std::string> Popup(int window, const std::string& text)
	{
		const std::string w = N(window);
		return {"box " + w + "|-1|-1|0|1", "text " + w + " nofit|" + text, "start " + w};
	}
	// Closes the message box and forgets the calls.
	void Forget()
	{
		dialog.CloseMessageDlg();
		calls.clear();
	}
};

} // namespace

//----------------------------------------------------------------------
// The character sheet
//----------------------------------------------------------------------
TEST(GameUI3Sheet, TheWeaponSpeedIsTheSheets)
{
	Scene scene;
	g_char_slot_ingame.DEX_PURE = 77;
	UI_SetWeaponSpeed(40, 2);
	CHECK_EQ(2, int(g_char_slot_ingame.WS));
	CHECK_EQ(40, int(g_char_slot_ingame.WeaponSpeed));
	// The sheet keeps a byte of the speed.
	UI_SetWeaponSpeed(300, 255);
	CHECK_EQ(255, int(g_char_slot_ingame.WS));
	CHECK_EQ(44, int(g_char_slot_ingame.WeaponSpeed));
	CHECK_EQ(77, g_char_slot_ingame.DEX_PURE);
}

TEST(GameUI3Sheet, TheGuildIsTheSheets)
{
	Scene scene;
	UI_SetGuild(4321, 3, "Knights");
	CHECK_EQ(4321, int(g_char_slot_ingame.GUILD_ID));
	CHECK_EQ(3, int(g_char_slot_ingame.GUILD_GRADE));
	CHECK(g_char_slot_ingame.sz_guild_name == "Knights");
	UI_SetGuild(0, 0, "");
	CHECK_EQ(0, int(g_char_slot_ingame.GUILD_ID));
	CHECK_EQ(0, int(g_char_slot_ingame.GUILD_GRADE));
	CHECK(g_char_slot_ingame.sz_guild_name.empty());
	CHECK(calls.empty());
}

TEST(GameUI3Sheet, EachElementSetsItsOwnValue)
{
	Scene scene;
	auto values = []() {
		return N(g_char_slot_ingame.ElementalFire) + "|" + N(g_char_slot_ingame.ElementalWater) + "|"
			+ N(g_char_slot_ingame.ElementalEarth) + "|" + N(g_char_slot_ingame.ElementalWind);
	};
	g_char_slot_ingame.ElementalFire = g_char_slot_ingame.ElementalWater = 1;
	g_char_slot_ingame.ElementalEarth = g_char_slot_ingame.ElementalWind = 1;
	UI_SetElementalValue(ITEMTABLE_INFO::ELEMENTAL_TYPE_FIRE, 11);
	CHECK(values() == "11|1|1|1");
	UI_SetElementalValue(ITEMTABLE_INFO::ELEMENTAL_TYPE_WATER, -12);
	CHECK(values() == "11|-12|1|1");
	UI_SetElementalValue(ITEMTABLE_INFO::ELEMENTAL_TYPE_EARTH, 13);
	CHECK(values() == "11|-12|13|1");
	UI_SetElementalValue(ITEMTABLE_INFO::ELEMENTAL_TYPE_WIND, 14);
	CHECK(values() == "11|-12|13|14");
	// The sum, the end of the list and the "any" byte set nothing.
	UI_SetElementalValue(ITEMTABLE_INFO::ELEMENTAL_TYPE_SUM, 99);
	UI_SetElementalValue(ITEMTABLE_INFO::ELEMENTAL_TYPE_MAX, 99);
	UI_SetElementalValue(0xFF, 99);
	CHECK(values() == "11|-12|13|14");
}

//----------------------------------------------------------------------
// The level war's stair
//----------------------------------------------------------------------
TEST(GameUI3LevelWar, ASlayersStairIsItsPureStatSum)
{
	Scene scene;
	// The level is a vampire's or an ousters' measure, not a slayer's.
	scene.me->MStatus::SetStatus(MODIFY_LEVEL, 40);
	auto stair = [](int str, int dex, int inte) {
		g_char_slot_ingame.STR_PURE = str;
		g_char_slot_ingame.DEX_PURE = dex;
		g_char_slot_ingame.INT_PURE = inte;
		return GetMyLevelWarStair();
	};
	CHECK_EQ(1, stair(0, 0, 0));
	CHECK_EQ(1, stair(50, 50, 50));
	CHECK_EQ(2, stair(50, 50, 51));
	CHECK_EQ(2, stair(70, 70, 70));
	CHECK_EQ(3, stair(70, 70, 71));
	CHECK_EQ(3, stair(100, 100, 60));
	CHECK_EQ(4, stair(100, 100, 61));
	CHECK_EQ(4, stair(100, 100, 100));
	CHECK_EQ(0, stair(100, 100, 101));
	CHECK_EQ(0, stair(1000, 0, 0));
	// Recorded defect: a negative sum fails the first stair's floor and
	// lands on the second.
	CHECK_EQ(2, stair(-1, 0, 0));
}

TEST(GameUI3LevelWar, AVampireOrOustersStairIsItsLevel)
{
	Scene scene;
	// The pure stats are a slayer's measure.
	g_char_slot_ingame.STR_PURE = g_char_slot_ingame.DEX_PURE = g_char_slot_ingame.INT_PURE = 100;
	// A player of no race reads as an ousters.
	for (int type : { int(CREATURETYPE_VAMPIRE_MALE1), int(CREATURETYPE_OUSTERS), NPC_TYPE })
	{
		scene.Be(type);
		auto stair = [&](DWORD level) {
			scene.me->MStatus::SetStatus(MODIFY_LEVEL, level);
			return GetMyLevelWarStair();
		};
		CHECK_EQ(1, stair(1));
		CHECK_EQ(1, stair(30));
		CHECK_EQ(2, stair(31));
		CHECK_EQ(2, stair(50));
		CHECK_EQ(3, stair(51));
		CHECK_EQ(3, stair(70));
		CHECK_EQ(4, stair(71));
		CHECK_EQ(4, stair(90));
		CHECK_EQ(0, stair(91));
		CHECK_EQ(0, stair(150));
		// Recorded defect: level 0 (and a level past the int range, which
		// reads negative) fails the first stair's floor and lands on the
		// second.
		CHECK_EQ(2, stair(0));
		CHECK_EQ(2, stair(0xFFFFFFFFu));
	}
}

//----------------------------------------------------------------------
// The talk box and the windows that replace it
//----------------------------------------------------------------------
TEST(GameUI3Talk, TheWindowsThatOpenOverTheTalkHideIt)
{
	Scene scene;
	const std::pair<const char*, void (*)()> opens[] = {
		{"bring fee", []() { UI_OpenBringFee(5000); }},
		{"modify tax", UI_RunModifyTax},
		{"swap advancement", UI_Show_Swap_Advancement_Item},
		{"input name", []() { UI_RunInputNameWindow(1); }},
	};
	for (const auto& open : opens)
	{
		scene.OpenTalk();
		open.second();
		const bool hidden = Same({"finish " + N(windowCount)});
		if (!hidden) std::printf("      %s\n", open.first);
		CHECK(hidden);
		// Hidden, not closed: the game's input stays locked.
		CHECK_EQ(FLAG_UIDIALOG_LOCK_PCTALK, int(UIDialog::IsLockInput()));
		// With no talk box there is nothing to hide.
		scene.dialog.ClosePCTalkDlg();
		calls.clear();
		open.second();
		CHECK(calls.empty());
	}
}

TEST(GameUI3Talk, AnInputNameModeOutsideTheThreeIsRefused)
{
	// The window's modes are 0 to 2 (INPUT_NAME_MAX is their count); any
	// other leaves the talk as it is.
	Scene scene;
	for (int mode : {-1, 3, 4, 255})
	{
		scene.OpenTalk();
		UI_RunInputNameWindow(mode);
		CHECK(calls.empty());
		CHECK_EQ(FLAG_UIDIALOG_LOCK_PCTALK, int(UIDialog::IsLockInput()));
		scene.dialog.ClosePCTalkDlg();
	}
	// Each of the three it has opens it over the talk, which it hides.
	for (int mode : {0, 1, 2})
	{
		scene.OpenTalk();
		UI_RunInputNameWindow(mode);
		CHECK(Same({"finish " + N(windowCount)}));
		scene.dialog.ClosePCTalkDlg();
		calls.clear();
	}
}

TEST(GameUI3Talk, ClosingTheInputNameWindowEndsTheTalkAndTheWait)
{
	Scene scene;
	scene.OpenTalk();
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	UI_CloseInputNameWindow();
	CHECK(Same({"delete 1"}));
	CHECK_EQ(0, int(UIDialog::IsLockInput()));
	CHECK(scene.me->GetWaitVerify() == MPlayer::WAIT_VERIFY_NULL);
	CHECK(scene.temp.GetMode() == TempInformation::MODE_NULL);
	// With no talk box open, a talk lock left behind is still released.
	calls.clear();
	UIDialog::SetLockInputPCTalk();
	UIDialog::SetLockInputMessage();
	UI_CloseInputNameWindow();
	CHECK(calls.empty());
	CHECK_EQ(FLAG_UIDIALOG_LOCK_MESSAGE, int(UIDialog::IsLockInput()));
	UIDialog::UnSetLockInputMessage();
}

TEST(GameUI3Talk, TheWarListEndsTheTalkAndTakesEveryEntry)
{
	Scene scene;
	scene.OpenTalk();
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	GCWarScheduleList packet;
	for (BYTE warType : { BYTE(0), BYTE(1), BYTE(0) })
	{
		WarScheduleInfo* pInfo = new WarScheduleInfo();
		pInfo->warType = warType;
		pInfo->challengerGuildName[0] = "Red";
		packet.addWarScheduleInfo(pInfo);
	}
	UI_RunWarList(&packet);
	CHECK(Same({"delete 1"}));
	CHECK_EQ(0, int(UIDialog::IsLockInput()));
	CHECK(scene.me->GetWaitVerify() == MPlayer::WAIT_VERIFY_NULL);
	// The request mode is not the war list's.
	CHECK(scene.temp.GetMode() == TempInformation::MODE_TRADE_REQUEST);
	CHECK(packet.popWarScheduleInfo() == NULL);
	// No talk box: a talk lock left behind is released all the same.
	calls.clear();
	UIDialog::SetLockInputPCTalk();
	UI_RunWarList(&packet);
	CHECK(calls.empty());
	CHECK_EQ(0, int(UIDialog::IsLockInput()));
	// A missing entry ends the list there; the rest stays in the packet.
	WarScheduleInfo* pLast = new WarScheduleInfo();
	pLast->warType = 1;
	packet.addWarScheduleInfo(new WarScheduleInfo());
	packet.addWarScheduleInfo(NULL);
	packet.addWarScheduleInfo(pLast);
	UI_RunWarList(&packet);
	WarScheduleInfo* pLeft = packet.popWarScheduleInfo();
	CHECK(pLeft == pLast);
	delete pLeft;
	CHECK(packet.popWarScheduleInfo() == NULL);
}

//----------------------------------------------------------------------
// The quests an NPC offers
//----------------------------------------------------------------------
TEST(GameUI3Quest, NoQuestTableOrNoQuestLeavesTheTalkAlone)
{
	Scene scene;
	AddQuest(scene.quests, 501, QUEST_INFO_MONSTER_KILL, 0, 0, 0, 7, "Wolf");
	scene.talkBox.SetType(PCTalkBox::NORMAL);
	scene.talkBox.SetCreatureType(17);
	scene.talkBox.SetContent("before");
	scene.talkBox.AddString("old");
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	g_pQuestInfoManager = NULL;
	UI_RunQuestList(QuestIDs({501}).get());
	g_pQuestInfoManager = &scene.quests;
	UI_RunQuestList(QuestIDs({}).get());
	CHECK(calls.empty());
	CHECK(scene.talkBox.GetType() == PCTalkBox::NORMAL);
	CHECK_EQ((size_t)1, scene.talkBox.size());
	CHECK(SameText("before", scene.talkBox.GetContent()));
	CHECK(scene.me->GetWaitVerify() == MPlayer::WAIT_VERIFY_TRADE);
}

TEST(GameUI3Quest, TheQuestNPCsTellEachQuestAndOfferIt)
{
	// Kaiser (17), Rebecca (255) and 653 tell each known quest as the
	// talk's text, with its time limit, and offer it under its id; the
	// last one told is the text that stays.
	Scene scene;
	AddQuest(scene.quests, 501, QUEST_INFO_MONSTER_KILL, 3 * 3600 + 5 * 60, 0, 0, 7, "Wolf");
	AddQuest(scene.quests, 502, QUEST_INFO_MEET_NPC, 0, 0, 0, 0, "Meet");
	AddQuest(scene.quests, 503, QUEST_INFO_MONSTER_KILL, 7200, 0, 0, 3, "Ghoul");
	AddQuest(scene.quests, 504, QUEST_INFO_MINI_GAME, 59, MINI_GAME_TYPE_MINE, 0, 0, "Mine");
	AddQuest(scene.quests, 505, QUEST_INFO_MINI_GAME, 0, MINI_GAME_TYPE_ARROW, 0, 0, "Arrow");
	AddQuest(scene.quests, 506, QUEST_INFO_MINI_GAME, 0, MINI_GAME_TYPE_NEMO, 0, 0, "Nemo");
	AddQuest(scene.quests, 507, QUEST_INFO_MONSTER_KILL, 40 * 60, 0, 0, 2, "Bat");
	AddQuest(scene.quests, 508, QUEST_INFO_MONSTER_KILL, 0, 0, 0, 4, "Imp");
	AddQuest(scene.quests, 509, QUEST_INFO_GATHER_ITEM, 60, 0, 0, 2, "Herb");
	AddQuest(scene.quests, 510, QUEST_INFO_TYPE_NULL, 0, 0, 0, 0, "Odd");
	struct Case { int race; QuestID_t id; std::string content; std::string line; };
	const std::string cancel = "|9999:Later";
	const Case cases[] = {
		{CREATURETYPE_SLAYER_MALE, 501, "Bring Wolf, 7 of them, within 3h 5m.", "|501:Yes"},
		{CREATURETYPE_VAMPIRE_MALE1, 501, "Drain Wolf, 7 of them, within 3h 5m.", "|501:Yes"},
		{CREATURETYPE_OUSTERS, 501, "Bring Wolf, 7 of them, within 3h 5m.", "|501:Yes"},
		// Minutes alone; whole hours keep their zero minutes; no limit, no time.
		{CREATURETYPE_SLAYER_MALE, 507, "Bring Bat, 2 of them, within 40m.", "|507:Yes"},
		{CREATURETYPE_SLAYER_MALE, 503, "Bring Ghoul, 3 of them, within 2h 0m.", "|503:Yes"},
		{CREATURETYPE_SLAYER_MALE, 508, "Bring Imp, 4 of them, within .", "|508:Yes"},
		{CREATURETYPE_SLAYER_MALE, 502, S(STRING_MESSAGE_MEET_NPC_SLAYER), "|502:Yes"},
		{CREATURETYPE_VAMPIRE_MALE1, 502, S(STRING_MESSAGE_MEET_NPC_VAMPIRE), "|502:Yes"},
		{CREATURETYPE_OUSTERS, 502, S(STRING_MESSAGE_MEET_NPC_SLAYER), "|502:Yes"},
		// A mini game is offered under its game's name; one with no name
		// here is told, not offered.
		{CREATURETYPE_SLAYER_MALE, 504, S(STRING_MESSAGE_SELECT_MINI_GAME), "|504:" + S(STRING_MESSAGE_SELECT_CRAZY_MINE)},
		{CREATURETYPE_SLAYER_MALE, 505, S(STRING_MESSAGE_SELECT_MINI_GAME), "|505:" + S(STRING_MESSAGE_SELECT_ARROW_TILES)},
		{CREATURETYPE_SLAYER_MALE, 506, S(STRING_MESSAGE_SELECT_MINI_GAME), ""},
		// A quest with nothing to tell (a gathering, or a type the client
		// does not know) is offered with no text.
		{CREATURETYPE_SLAYER_MALE, 509, "", "|509:Yes"},
		{CREATURETYPE_VAMPIRE_MALE1, 510, "", "|510:Yes"},
	};
	for (unsigned npc : { 17u, 255u, 653u })
		for (const Case& c : cases)
		{
			scene.Be(c.race);
			scene.talkBox.Release();
			scene.talkBox.SetType(PCTalkBox::NORMAL);
			scene.talkBox.SetCreatureType(npc);
			scene.talkBox.AddString("8888Old");
			scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
			scene.dialog.ClosePCTalkDlg();
			calls.clear();
			windowCount = 0;
			std::unique_ptr<GCSelectQuestID> packet = QuestIDs({c.id});
			DirtyStack();
			UI_RunQuestList(packet.get());
			const bool same = Same({"close all", "talk 1|" + N(npc) + "|(null)", "menu 1" + c.line + cancel,
				"text 1 fit|" + c.content, "start 1"});
			if (!same) std::printf("      npc %u quest %u\n", npc, unsigned(c.id));
			CHECK(same);
			CHECK(scene.talkBox.GetType() == PCTalkBox::SELECT_QUEST);
			CHECK(scene.me->GetWaitVerify() == MPlayer::WAIT_VERIFY_NULL);
			CHECK_EQ(FLAG_UIDIALOG_LOCK_PCTALK, int(UIDialog::IsLockInput()));
		}
}

TEST(GameUI3Quest, TheQuestNPCsOfferEveryKnownQuestInOrder)
{
	Scene scene;
	AddQuest(scene.quests, 501, QUEST_INFO_MONSTER_KILL, 3 * 3600 + 5 * 60, 0, 0, 7, "Wolf");
	AddQuest(scene.quests, 502, QUEST_INFO_MEET_NPC, 0, 0, 0, 0, "Meet");
	AddQuest(scene.quests, 503, QUEST_INFO_MONSTER_KILL, 7200, 0, 0, 3, "Ghoul");
	AddQuest(scene.quests, 504, QUEST_INFO_MINI_GAME, 0, MINI_GAME_TYPE_MINE, 0, 0, "Mine");
	scene.talkBox.SetCreatureType(255);
	// An id the table does not know is passed over.
	UI_RunQuestList(QuestIDs({501, 600, 502, 503, 504}).get());
	// The last quest told is the text, with its own time limit.
	CHECK(Same({"close all", "talk 1|255|(null)",
		"menu 1|501:Yes|502:Yes|503:Yes|504:" + S(STRING_MESSAGE_SELECT_CRAZY_MINE) + "|9999:Later",
		"text 1 fit|" + S(STRING_MESSAGE_SELECT_MINI_GAME), "start 1"}));
	calls.clear();
	UI_RunQuestList(QuestIDs({502, 503}).get());
	CHECK(Same({"delete 1", "close all", "talk 2|255|(null)", "menu 2|502:Yes|503:Yes|9999:Later",
		"text 2 fit|Bring Ghoul, 3 of them, within 2h 0m.", "start 2"}));
	// Only unknown ids: just the way out.
	calls.clear();
	UI_RunQuestList(QuestIDs({600}).get());
	CHECK(Same({"delete 2", "close all", "talk 3|255|(null)", "menu 3|9999:Later",
		"text 3 fit|Bring Ghoul, 3 of them, within 2h 0m.", "start 3"}));
}

TEST(GameUI3Quest, OtherNPCsListTheHuntsForTheRace)
{
	Scene scene;
	AddQuest(scene.quests, 501, QUEST_INFO_MONSTER_KILL, 3 * 3600 + 5 * 60, 0, 0, 7, "Wolf");
	AddQuest(scene.quests, 502, QUEST_INFO_MEET_NPC, 0, 0, 0, 0, "Meet");
	AddQuest(scene.quests, 503, QUEST_INFO_MONSTER_KILL, 40 * 60, 0, 0, 3, "Ghoul");
	AddQuest(scene.quests, 504, QUEST_INFO_MONSTER_KILL, 0, 0, 0, 4, "Imp");
	AddQuest(scene.quests, 505, QUEST_INFO_MONSTER_KILL, 7200, 0, 0, 5, "Bat");
	const std::pair<int, int> races[] = {
		{CREATURETYPE_SLAYER_MALE, UI_STRING_MESSAGE_SELECT_QUEST_SLAYER},
		{CREATURETYPE_VAMPIRE_MALE1, UI_STRING_MESSAGE_SELECT_QUEST_VAMPIRE},
		{CREATURETYPE_OUSTERS, UI_STRING_MESSAGE_SELECT_QUEST_OUSTERS},
	};
	for (unsigned npc : { 16u, 18u, 254u, 256u, 652u, 654u })
		for (const auto& race : races)
		{
			scene.Be(race.first);
			scene.talkBox.Release();
			scene.talkBox.SetCreatureType(npc);
			scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
			scene.dialog.ClosePCTalkDlg();
			calls.clear();
			windowCount = 0;
			UI_RunQuestList(QuestIDs({501, 600, 503, 504, 505}).get());
			const bool same = Same({"close all", "talk 1|" + N(npc) + "|(null)",
				"menu 1|501:Wolf 7 heads Limit:3h 5m|503:Ghoul 3 heads Limit:40m|504:Imp 4 heads Limit:"
				"|505:Bat 5 heads Limit:2h 0m|9999:Later",
				"text 1 fit|" + S(race.second), "start 1"});
			if (!same) std::printf("      npc %u race %d\n", npc, race.first);
			CHECK(same);
			CHECK(scene.talkBox.GetType() == PCTalkBox::SELECT_QUEST);
			CHECK(scene.me->GetWaitVerify() == MPlayer::WAIT_VERIFY_NULL);
		}
	// A quest that is not a hunt is not listed.
	scene.dialog.ClosePCTalkDlg();
	calls.clear();
	windowCount = 0;
	UI_RunQuestList(QuestIDs({501, 502}).get());
	CHECK(Same({"close all", "talk 1|654|(null)",
		"menu 1|501:Wolf 7 heads Limit:3h 5m|9999:Later",
		"text 1 fit|" + S(UI_STRING_MESSAGE_SELECT_QUEST_OUSTERS), "start 1"}));
	// Before any hunt, a quest that is not one lists nothing.
	scene.dialog.ClosePCTalkDlg();
	calls.clear();
	windowCount = 0;
	std::unique_ptr<GCSelectQuestID> packet = QuestIDs({502, 501});
	DirtyStack();
	UI_RunQuestList(packet.get());
	CHECK(Same({"close all", "talk 1|654|(null)", "menu 1|501:Wolf 7 heads Limit:3h 5m|9999:Later",
		"text 1 fit|" + S(UI_STRING_MESSAGE_SELECT_QUEST_OUSTERS), "start 1"}));
}

TEST(GameUI3Quest, APlayerOfNoRaceIsOfferedNothing)
{
	// The talk box is emptied and the wait ended, then nothing opens.
	Scene scene;
	AddQuest(scene.quests, 501, QUEST_INFO_MONSTER_KILL, 0, 0, 0, 7, "Wolf");
	scene.Be(NPC_TYPE);
	scene.talkBox.SetCreatureType(18);
	scene.talkBox.SetContent("before");
	scene.talkBox.AddString("old");
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	std::unique_ptr<GCSelectQuestID> packet = QuestIDs({501});
	UI_RunQuestList(packet.get());
	CHECK(calls.empty());
	CHECK(scene.talkBox.GetType() == PCTalkBox::SELECT_QUEST);
	CHECK_EQ((size_t)0, scene.talkBox.size());
	CHECK(SameText("before", scene.talkBox.GetContent()));
	CHECK(scene.me->GetWaitVerify() == MPlayer::WAIT_VERIFY_NULL);
	// The ids are left in the packet.
	CHECK(!packet->empty());
}

TEST(GameUI3Quest, TheHuntUnderWayIsAskedAboutItsMonster)
{
	// Without the quest window the quest under way is id 0xFFFFFFFF.
	Scene scene;
	CHECK(!UI_IsMonsterKillQuest_Monster(9));
	AddQuest(scene.quests, 0xFFFFFFFFu, QUEST_INFO_MONSTER_KILL, 0, 0, 9, 5, "Wolf");
	CHECK(UI_IsMonsterKillQuest_Monster(9));
	CHECK(!UI_IsMonsterKillQuest_Monster(8));
	CHECK(!UI_IsMonsterKillQuest_Monster(10));
	scene.quests.Release();
	AddQuest(scene.quests, 0xFFFFFFFFu, QUEST_INFO_MEET_NPC, 0, 0, 9, 5, "Wolf");
	CHECK(!UI_IsMonsterKillQuest_Monster(9));
	// Another quest's monster does not count.
	scene.quests.Release();
	AddQuest(scene.quests, 501, QUEST_INFO_MONSTER_KILL, 0, 0, 9, 5, "Wolf");
	CHECK(!UI_IsMonsterKillQuest_Monster(9));
}

TEST(GameUI3Quest, AHuntTheServerAddsIsNoEventQuest)
{
	// GCMonsterKillQuestInfo adds a hunt the quest file lacks through
	// SetInfo, which gives it no event flag or level: both are 0.
	Scene scene;
	scene.quests.SetInfo(601, 5, 60, "Wolf");
	QUEST_INFO* pQuest = scene.quests.GetInfo(601);
	CHECK(pQuest != NULL);
	if (pQuest != NULL)
	{
		CHECK_EQ(0, int(pQuest->GetEventQuest()));
		CHECK_EQ(0, int(pQuest->GetQuestLevel()));
	}
	// Its status, and that of an event quest of a level past the five
	// event titles, take the quests' title (the window is not built here).
	AddQuest(scene.quests, 602, QUEST_INFO_MONSTER_KILL, 60, 0, 0, 7, "Event", 1, 5);
	AddQuest(scene.quests, 603, QUEST_INFO_MONSTER_KILL, 60, 0, 0, 7, "Event", 1, 255);
	for (WORD id : { WORD(601), WORD(602), WORD(603) })
	{
		GCQuestStatus status;
		status.setQuestID(id);
		status.setCurrentNul(1);
		status.setRemainTime(90);
		UI_RunQuestStatus(&status);
	}
	CHECK(calls.empty());
}

//----------------------------------------------------------------------
// The item shop
//----------------------------------------------------------------------
namespace {

GoodsInfo* Good(ObjectID_t id, int itemClass, int type, int grade, std::initializer_list<OptionType_t> options, int number, DWORD timeLimit)
{
	GoodsInfo* pInfo = new GoodsInfo();
	pInfo->objectID = id;
	pInfo->itemClass = BYTE(itemClass);
	pInfo->itemType = ItemType_t(type);
	pInfo->grade = Grade_t(grade);
	pInfo->optionType = options;
	pInfo->num = ItemNum_t(number);
	pInfo->timeLimit = timeLimit;
	return pInfo;
}

} // namespace

TEST(GameUI3ItemShop, EachGoodBecomesAnItem)
{
	Scene scene;
	scene.OpenTalk();
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	GCGoodsList packet;
	packet.addGoodsInfo(Good(11, ITEM_CLASS_SWORD, 2, 3, {5, 7}, 1, 0));
	packet.addGoodsInfo(Good(12, ITEM_CLASS_POTION, 0, 0, {}, 9, 60));
	packet.addGoodsInfo(Good(13, ITEM_CLASS_SG, 0, 4, {2}, 30, 1));
	packet.addGoodsInfo(Good(14, ITEM_CLASS_SR, 0, 0, {}, 12, 0));
	UI_RunItemShop(&packet);
	// Without the item list window, which would own them, each item is
	// freed as soon as it is made, a gun with its magazine.
	CHECK(Same({"finish 1", "new|" + N(ITEM_CLASS_SWORD), "destroyed|11", "new|" + N(ITEM_CLASS_POTION), "destroyed|12",
		"new|" + N(ITEM_CLASS_SG), "new|" + N(ITEM_CLASS_MAGAZINE), "destroyed|0", "destroyed|13",
		"new|" + N(ITEM_CLASS_SR), "new|" + N(ITEM_CLASS_MAGAZINE), "destroyed|0", "destroyed|14"}));
	CHECK(packet.isEmpty());
	CHECK(scene.me->GetWaitVerify() == MPlayer::WAIT_VERIFY_NULL);
	CHECK(scene.temp.GetMode() == TempInformation::MODE_NULL);
	CHECK(Same({"0|11|2|3|5,7|1|0",
		// A good with a time limit is a quest item.
		"1|12|0|0||9|1",
		// A gun's count is its magazine's: the first magazine that fits,
		// with no options and no id, freed by the gun before it.
		"3|0|1|-1||30|0", "2|13|0|4|2|1|1",
		// No magazine fits the rifle: it is given the last one tried.
		"5|0|2|-1||12|0", "4|14|0|0||1|0"}, freed));
}

TEST(GameUI3ItemShop, AGoodWithNoItemIsSkippedAndAMissingOneEndsTheList)
{
	Scene scene;
	GCGoodsList packet;
	// A class past the table, and one the factory cannot make.
	refusedClass = ITEM_CLASS_POTION;
	packet.addGoodsInfo(Good(11, 250, 0, 0, {}, 1, 0));
	packet.addGoodsInfo(Good(12, ITEM_CLASS_POTION, 0, 0, {}, 1, 0));
	packet.addGoodsInfo(Good(13, ITEM_CLASS_SWORD, 0, 0, {}, 1, 0));
	packet.addGoodsInfo(NULL);
	packet.addGoodsInfo(Good(15, ITEM_CLASS_SWORD, 0, 0, {}, 1, 0));
	UI_RunItemShop(&packet);
	CHECK(Same({"new|" + N(ITEM_CLASS_POTION), "new|" + N(ITEM_CLASS_SWORD), "destroyed|13"}));
	CHECK(Same({"0|13|0|0||1|0"}, freed));
	GoodsInfo* pLeft = packet.popGoodsInfo();
	CHECK(pLeft != NULL && pLeft->objectID == 15);
	delete pLeft;
	CHECK(packet.isEmpty());
}

TEST(GameUI3ItemShop, AGunTheFactoryMakesNoMagazineForIsListedEmpty)
{
	Scene scene;
	GCGoodsList packet;
	refusedClass = ITEM_CLASS_MAGAZINE;
	packet.addGoodsInfo(Good(13, ITEM_CLASS_SG, 0, 4, {2}, 30, 1));
	packet.addGoodsInfo(Good(14, ITEM_CLASS_SWORD, 0, 0, {}, 1, 0));
	UI_RunItemShop(&packet);
	// The gun is made and listed with no magazine (none freed with it),
	// and the good after it is made too; without the item list window
	// each is freed at once.
	CHECK(Same({"new|" + N(ITEM_CLASS_SG), "new|" + N(ITEM_CLASS_MAGAZINE), "destroyed|13",
		"new|" + N(ITEM_CLASS_SWORD), "destroyed|14"}));
	CHECK(packet.isEmpty());
	CHECK(Same({"0|13|0|4|2|1|1", "1|14|0|0||1|0"}, freed));
}

//----------------------------------------------------------------------
// The mixing forge and the option removal
//----------------------------------------------------------------------
TEST(GameUI3Forge, MixingPutsTheSecondOptionOnTheFirstItem)
{
	Scene scene;
	MItem* pFirst = scene.Sword(21, {5}, 2, 0);
	MItem* pSecond = scene.Sword(22, {9}, 6, 1);
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	calls.clear();
	UI_OkMixingForge((5u << 16) | 9u, pFirst, pSecond);
	// Taking the second out re-checks the items left; then it is deleted.
	std::vector<std::string> expected = {"affect|21", "destroyed|22", "sound|" + N(SOUND_EVENT_FANFARE)};
	for (const std::string& call : scene.Popup(1, S(UI_STRING_MESSAGE_COMPLETE_MERGE_ITEM))) expected.push_back(call);
	expected.push_back("affect|21");
	CHECK(Same(expected));
	CHECK(Options(pFirst) == "5,9");
	CHECK_EQ(6, pFirst->GetGrade());
	CHECK(scene.inventory.GetItem(TYPE_OBJECTID(22)) == NULL);
	CHECK(scene.inventory.GetItem(TYPE_OBJECTID(21)) == pFirst);
	// The forge's mode is the caller's to end.
	CHECK(scene.temp.GetMode() == TempInformation::MODE_TRADE_REQUEST);
	// The inventory's item of the first id is the one changed, to the
	// higher grade of the two handed in.
	scene.Forget();
	MItem* pHeld = scene.Sword(23, {3}, 1, 2);
	MItem* pLow = scene.Sword(24, {4}, 4, 3);
	MItem* pCopy = scene.Sword(23, {3}, 7, -1);
	UI_OkMixingForge((3u << 16) | 4u, pCopy, pLow);
	CHECK(Options(pHeld) == "3,4");
	CHECK_EQ(7, pHeld->GetGrade());
	CHECK(Options(pCopy) == "3");
	CHECK_EQ(7, pCopy->GetGrade());
	CHECK(scene.inventory.GetItem(TYPE_OBJECTID(24)) == NULL);
	delete pCopy;
	// A second item not in the inventory is not deleted.
	scene.Forget();
	MItem* pThird = scene.Sword(25, {6}, 1, 4);
	MItem* pLoose = scene.Sword(26, {8}, 1, -1);
	calls.clear();
	UI_OkMixingForge((6u << 16) | 8u, pThird, pLoose);
	CHECK(Options(pThird) == "6,8");
	expected = {"sound|" + N(SOUND_EVENT_FANFARE)};
	for (const std::string& call : scene.Popup(3, S(UI_STRING_MESSAGE_COMPLETE_MERGE_ITEM))) expected.push_back(call);
	expected.push_back("affect|25");
	CHECK(Same(expected));
	delete pLoose;
}

TEST(GameUI3Forge, MixingRefusesItemsThatDoNotMatch)
{
	Scene scene;
	MItem* pFirst = scene.Sword(21, {5}, 2, 0);
	MItem* pSecond = scene.Sword(22, {9}, 6, 1);
	MItem* pBare = scene.Sword(23, {}, 0, 2);
	MItem* pDouble = scene.Sword(24, {5, 9}, 0, 3);
	MItem* pOutside = scene.Sword(25, {5}, 0, -1);
	MItem* pTwin = scene.Sword(21, {9}, 0, -1);	// the first's id
	const std::vector<std::string> refused = scene.Popup(1, S(STRING_ERROR_ETC_ERROR));
	struct Case { const char* what; DWORD parameter; MItem* pItem; MItem* pItem2; };
	const Case cases[] = {
		{"no first", (5u << 16) | 9u, NULL, pSecond},
		{"no second", (5u << 16) | 9u, pFirst, NULL},
		{"first bare", (0u << 16) | 9u, pBare, pSecond},
		{"second bare", (5u << 16) | 0u, pFirst, pBare},
		{"first double", (5u << 16) | 9u, pDouble, pSecond},
		{"second double", (5u << 16) | 5u, pFirst, pDouble},
		{"first option", (6u << 16) | 9u, pFirst, pSecond},
		{"second option", (5u << 16) | 8u, pFirst, pSecond},
		{"swapped", (9u << 16) | 5u, pFirst, pSecond},
		// The high word is the first option whole, not its low byte.
		{"high word", (0x105u << 16) | 9u, pFirst, pSecond},
		{"not held", (5u << 16) | 9u, pOutside, pSecond},
		// One item twice: the second's removal would delete the first.
		{"same item", (5u << 16) | 5u, pFirst, pFirst},
		{"same id", (5u << 16) | 9u, pFirst, pTwin},
	};
	for (const Case& c : cases)
	{
		scene.Forget();
		windowCount = 0;
		UI_OkMixingForge(c.parameter, c.pItem, c.pItem2);
		const bool same = Same(refused);
		if (!same) std::printf("      %s\n", c.what);
		CHECK(same);
		CHECK(Options(pFirst) == "5");
		CHECK_EQ(2, pFirst->GetGrade());
		CHECK(scene.inventory.GetItem(TYPE_OBJECTID(22)) == pSecond);
		CHECK(scene.inventory.GetItem(TYPE_OBJECTID(21)) == pFirst);
	}
	delete pTwin;
	delete pOutside;
}

TEST(GameUI3Forge, ClosingTheForgeEndsItsMode)
{
	Scene scene;
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	UI_CloseMixingForge();
	CHECK(scene.temp.GetMode() == TempInformation::MODE_NULL);
	CHECK(calls.empty());
}

TEST(GameUI3Forge, RemovingOptionsLeavesTheOneChosen)
{
	Scene scene;
	MItem* pItem = scene.Sword(31, {3, 4}, 0, 0);
	calls.clear();
	// The option is the parameter's low byte.
	UI_OkRemoveOption(0x0107, pItem);
	std::vector<std::string> expected = {"sound|" + N(SOUND_XMAS_STAR), "affect|31"};
	for (const std::string& call : scene.Popup(1, S(STRING_MESSAGE_SUCCESS_REMOVE_OPTION))) expected.push_back(call);
	CHECK(Same(expected));
	CHECK(Options(pItem) == "7");
	// Option 0 is none: the item is left with no option.
	scene.Forget();
	UI_OkRemoveOption(0x0100, pItem);
	CHECK(pItem->IsEmptyItemOptionList());
	CHECK(calls.front() == "sound|" + N(SOUND_XMAS_STAR));
	// No item: the error.
	scene.Forget();
	windowCount = 0;
	UI_OkRemoveOption(5, NULL);
	CHECK(Same(scene.Popup(1, S(STRING_ERROR_ETC_ERROR))));
}

//----------------------------------------------------------------------
// Messages
//----------------------------------------------------------------------
TEST(GameUI3Messages, APopupShowsItsGameString)
{
	Scene scene;
	UI_PopupMessage(0);
	CHECK(Same(scene.Popup(1, S(0))));
	scene.Forget();
	UI_PopupMessage(MAX_GAME_STRING - 1);
	CHECK(Same(scene.Popup(2, S(MAX_GAME_STRING - 1))));
	scene.Forget();
	// Outside the table, or with no dialog or table, nothing.
	UI_PopupMessage(-1);
	UI_PopupMessage(MAX_GAME_STRING);
	g_pUIDialog = NULL;
	UI_PopupMessage(5);
	g_pUIDialog = &scene.dialog;
	MStringArray* pStrings = g_pGameStringTable;
	g_pGameStringTable = NULL;
	UI_PopupMessage(5);
	g_pGameStringTable = pStrings;
	CHECK(calls.empty());
}

TEST(GameUI3Messages, EachSMSErrorHasItsMessage)
{
	Scene scene;
	const std::pair<DWORD, int> errors[] = {
		{1, UI_STRING_MESSAGE_SMS_FAIL_MAX_NUM_EXCEEDED}, {2, UI_STRING_MESSAGE_SMS_FAIL_INVALID_DATA},
		{3, UI_STRING_MESSAGE_SMS_FAIL_NO_SUCH_EID}, {4, UI_STRING_MESSAGE_SMS_FAIL_NOT_ENOUGH_CHARGE},
		{0, STRING_ERROR_ETC_ERROR}, {5, STRING_ERROR_ETC_ERROR}, {0xFFFFFFFFu, STRING_ERROR_ETC_ERROR},
	};
	for (const auto& error : errors)
	{
		scene.Forget();
		windowCount = 0;
		UI_SMS_ERROR(error.first);
		CHECK(Same(scene.Popup(1, S(error.second))));
	}
}

TEST(GameUI3Messages, TheMasterLairSaysWhetherItIsOpen)
{
	Scene scene;
	ZONETABLE_INFO* pLair = new ZONETABLE_INFO;
	pLair->ID = 1101;
	pLair->Name = "Bathory's Lair";
	scene.zones.Add(pLair);
	UI_MasterLairMessage(0, 0, 1101);
	UI_MasterLairMessage(1, 0, 1101);
	UI_MasterLairMessage(2, 15, 1101);
	UI_MasterLairMessage(2, -3, 1101);
	CHECK(Lines(scene.systemMessages) == std::vector<std::string>({"Bathory's Lair has opened.", "Bathory's Lair has closed.",
		"Bathory's Lair stays open for 15 minutes.", "Bathory's Lair stays open for -3 minutes."}));
	// Another kind, a zone the table does not know, the option off, or no
	// log, zone table or strings: nothing.
	scene.systemMessages.Clear();
	UI_MasterLairMessage(3, 0, 1101);
	UI_MasterLairMessage(255, 0, 1101);
	UI_MasterLairMessage(0, 0, 1102);
	UI_MasterLairMessage(1, 0, 1102);
	UI_MasterLairMessage(2, 15, 1102);
	scene.option.DoNotShowLairMsg = TRUE;
	UI_MasterLairMessage(0, 0, 1101);
	scene.option.DoNotShowLairMsg = FALSE;
	g_pZoneTable = NULL;
	UI_MasterLairMessage(0, 0, 1101);
	g_pZoneTable = &scene.zones;
	MStringArray* pStrings = g_pGameStringTable;
	g_pGameStringTable = NULL;
	UI_MasterLairMessage(0, 0, 1101);
	g_pGameStringTable = pStrings;
	g_pSystemMessage = NULL;
	UI_MasterLairMessage(0, 0, 1101);
	g_pSystemMessage = &scene.systemMessages;
	CHECK(Lines(scene.systemMessages).empty());
	CHECK(calls.empty());
}

//----------------------------------------------------------------------
// Guild names and mini-game scores
//----------------------------------------------------------------------
TEST(GameUI3Guild, AGuildNameIsKeptUnlessItIsEmpty)
{
	Scene scene;
	std::string name = "Knights";
	UI_SetGuildName(7, name);
	CHECK(std::string(scene.guilds.GetGuildName(7)) == "Knights");
	std::string other = "Old";
	scene.guilds.SetGuildName(8, other);
	std::string empty;
	UI_SetGuildName(8, empty);
	CHECK(std::string(scene.guilds.GetGuildName(8)) == "Old");
	// An empty name does not need the mapper.
	g_pGuildInfoMapper = NULL;
	UI_SetGuildName(9, empty);
	g_pGuildInfoMapper = &scene.guilds;
}

TEST(GameUI3MiniGame, TheScoresTakenAreTheOnesShown)
{
	// None, the top one, or the top two; a longer list has only its top
	// one taken.
	Scene scene;
	for (int scores = 0; scores <= 4; ++scores)
		for (GameType type : { GAME_MINE, GAME_NEMO, GAME_ARROW })
			for (BYTE level : { BYTE(0), BYTE(1) })
			{
				GCMiniGameScores packet;
				packet.setGameType(type);
				packet.setLevel(level);
				for (int i = 0; i < scores; ++i) packet.addScore("p" + N(i), WORD(100 - i));
				UI_MiniGameScores(&packet);
				const size_t left = scores <= 2 ? 0 : size_t(scores - 1);
				CHECK_EQ(left, packet.getSize());
				if (scores > 2) CHECK(packet.popScore().first == "p1");
			}
	CHECK(calls.empty());
}

//----------------------------------------------------------------------
// The login handed to the UI's queue
//----------------------------------------------------------------------
namespace {

// The last login received. Unlike the game's UI_LOGIN handler, which
// frees its two strings and clears them, this receiver leaves them.
LOGIN* pTaken = NULL;

void Received(DWORD message, intptr_t left, intptr_t right, void* pVoid)
{
	std::string line = "message|" + N(message) + "|" + N(left) + "|" + N(right);
	if (message == UI_LOGIN && pVoid != NULL)
	{
		pTaken = static_cast<LOGIN*>(pVoid);
		line += "|" + Text(pTaken->sz_id) + "|" + Text(pTaken->sz_password);
	}
	calls.push_back(line);
}

// Whether a string was freed: ASan's poison on a freed block; without
// ASan this says yes, so the check is the sanitized trees' alone.
bool Freed(const void* p)
{
#ifdef ASAN_SEES_FREES
	return __asan_address_is_poisoned(p) != 0;
#else
	(void)p;
	return true;
#endif
}

void NoTimer() {}

// The UI's base with its queue, handing what it is sent to Received.
struct Queue
{
	GameModelWorld world;
	CI_KOREAN ime;
	CSpriteSurface back;
	Base base;
	Base* oldBase = gpC_base;
	CI* oldCI = gC_ci;
	RECT oldRect = g_GameRect;
	std::string oldCookie = g_CpCookie;

	Queue()
	{
		calls.clear();
		gC_ci = &ime;
		spritectl_init();
		back.Init(10, 8);
		g_GameRect = {0, 0, 10, 8};
		base.Init(&back, Received);
		// Init started the IME's cursor blinker; the next timer's id is one
		// past it.
		timer_id_t probe = gC_timer2.Add(1000, NoTimer);
		timer_id_t blinker = probe - 1;
		gC_timer2.Delete(probe);
		gC_timer2.Delete(blinker);
		gpC_base = &base;
	}
	~Queue()
	{
		// Init pointed the text renderer at the surface; it starts with none.
		g_SetFL2Surface(NULL);
		g_CpCookie = oldCookie;
		g_GameRect = oldRect;
		gC_ci = oldCI;
		gpC_base = oldBase;
	}
	// Every message queued, in order.
	void Dispatch()
	{
		for (int i = 0; i < 8; ++i) base.DispatchMessage();
	}
};

} // namespace

TEST(GameUI3Login, NetmarbleLogsInWithTheCookie)
{
	Queue queue;
	g_CpCookie = "cookie-123";
	g_pUserInformation->IsNetmarble = true;
	// Netmarble's login wins over the web login.
	g_pUserInformation->IsAutoLogIn = true;
	g_pUserInformation->UserID = "kim";
	UI_RunConnect();
	queue.Dispatch();
	CHECK(Same({"message|" + N(UI_LOGIN) + "|0|0|cookie-123|DarkEden"}));
	// Each connect is a login of its own.
	calls.clear();
	g_CpCookie.clear();
	UI_RunConnect();
	UI_RunConnect();
	queue.Dispatch();
	CHECK(Same({"message|" + N(UI_LOGIN) + "|0|0||DarkEden", "message|" + N(UI_LOGIN) + "|0|0||DarkEden"}));
}

TEST(GameUI3Login, TheWebLoginLogsInWithItsKey)
{
	Queue queue;
	g_pUserInformation->IsNetmarble = false;
	g_pUserInformation->IsAutoLogIn = true;
	g_pUserInformation->UserID = "kim";
	g_pUserInformation->AutoLogInKeyValue = "key-42";
	UI_RunConnect();
	queue.Dispatch();
	CHECK(Same({"message|" + N(UI_LOGIN) + "|0|0|kim|key-42"}));
	// No ID or key: empty ones.
	calls.clear();
	g_pUserInformation->UserID = "";
	g_pUserInformation->AutoLogInKeyValue = "";
	UI_RunConnect();
	queue.Dispatch();
	CHECK(Same({"message|" + N(UI_LOGIN) + "|0|0||"}));
}

TEST(GameUI3Login, AConnectFreesTheLoginItReplaces)
{
	// Each login (Netmarble's, the web's) is a static whose strings the
	// UI_LOGIN handler frees and clears; a connect before the handler took
	// them (here: strings this receiver left) frees the ones it replaces.
	Queue queue;
	g_CpCookie = "cookie-123";
	g_pUserInformation->UserID = "kim";
	g_pUserInformation->AutoLogInKeyValue = "key-42";
	for (bool netmarble : {true, false})
	{
		g_pUserInformation->IsNetmarble = netmarble;
		g_pUserInformation->IsAutoLogIn = true;
		UI_RunConnect();
		queue.Dispatch();
		CHECK(pTaken != NULL);
		if (pTaken == NULL) return;
		const LOGIN first = *pTaken;
		UI_RunConnect();
		CHECK(Freed(first.sz_id));
		CHECK(Freed(first.sz_password));
		// Strings the handler freed and cleared are not freed again.
		queue.Dispatch();
		delete[] pTaken->sz_id;
		pTaken->sz_id = NULL;
		delete[] pTaken->sz_password;
		pTaken->sz_password = NULL;
		calls.clear();
		UI_RunConnect();
		queue.Dispatch();
		CHECK(Same({"message|" + N(UI_LOGIN) + "|0|0|" + (netmarble ? "cookie-123|DarkEden" : "kim|key-42")}));
	}
}

TEST(GameUI3Login, AManualLoginQueuesNothing)
{
	// The login window (the title's) asks for the ID.
	Queue queue;
	g_pUserInformation->IsNetmarble = false;
	g_pUserInformation->IsAutoLogIn = false;
	UI_RunConnect();
	queue.Dispatch();
	CHECK(calls.empty());
	// It does not need the UI's base.
	gpC_base = NULL;
	UI_RunConnect();
	gpC_base = &queue.base;
}

//----------------------------------------------------------------------
// The window cursor
//----------------------------------------------------------------------
namespace {

// Whether the system cursor shows: the display count GameUI's
// ShowCursor keeps is >= 0.
bool CursorShown()
{
#ifdef PLATFORM_WINDOWS
	ShowCursor(TRUE);
	return ShowCursor(FALSE) >= 0;
#else
	return SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE;
#endif
}

} // namespace

TEST(GameUI3Cursor, ShowingAndHidingSettleTheCount)
{
	// Shown is a count of 0, hidden -1, however many times either is
	// asked: a second hide still shows on one show, and the other way.
	const bool wasShown = CursorShown();
	UI_ShowWindowCursor();
	CHECK(CursorShown());
	UI_HiddenWindowCursor();
	CHECK(!CursorShown());
	UI_HiddenWindowCursor();
	CHECK(!CursorShown());
	UI_ShowWindowCursor();
	CHECK(CursorShown());
	UI_ShowWindowCursor();
	CHECK(CursorShown());
	UI_HiddenWindowCursor();
	CHECK(!CursorShown());
	UI_ShowWindowCursor();
	CHECK(CursorShown());
	if (wasShown) UI_ShowWindowCursor();
	else UI_HiddenWindowCursor();
}

//----------------------------------------------------------------------
// The notices
//----------------------------------------------------------------------
#ifndef PLATFORM_WINDOWS
namespace {

// The notice files a call looked for in the tutorial pack, as the pack's
// reader reports each it cannot find (on stderr off Windows); the test
// tree has no pack. The mail made of one goes to the game's mailbox,
// which does not exist here.
template <class F>
std::vector<std::string> NoticesLookedFor(F&& call)
{
	std::fflush(stderr);
	std::FILE* pCapture = std::tmpfile();
	CHECK(pCapture != NULL);
	if (pCapture == NULL) return {};
	const int saved = dup(2);
	dup2(fileno(pCapture), 2);
	call();
	std::fflush(stderr);
	dup2(saved, 2);
	close(saved);
	std::rewind(pCapture);
	std::string text;
	char buffer[512];
	size_t got;
	while ((got = std::fread(buffer, 1, sizeof(buffer), pCapture)) > 0) text.append(buffer, got);
	std::fclose(pCapture);
	std::vector<std::string> names;
	const std::string start = "Cannot read resource ", end = " (archive=";
	for (size_t at = text.find(start); at != std::string::npos; at = text.find(start, at + 1))
	{
		const size_t stop = text.find(end, at);
		if (stop == std::string::npos) break;
		const std::string path = text.substr(at + start.size(), stop - at - start.size());
		names.push_back(path.substr(path.find_last_of('/') + 1));
	}
	return names;
}

} // namespace

TEST(GameUI3Notice, EachNoticeIsItsRacesFile)
{
	Scene scene;
	using Names = std::vector<std::string>;
	auto notice = [](DWORD sendID, DWORD parameter) { return NoticesLookedFor([&]() { UI_RunNotice(sendID, parameter); }); };
	// A slayer of stair 1 (pure stats 30).
	g_char_slot_ingame.STR_PURE = g_char_slot_ingame.DEX_PURE = g_char_slot_ingame.INT_PURE = 10;
	CHECK(notice(1, 0) == Names({"BeginnerZoneSlayer.txt"}));
	CHECK(notice(2, 0) == Names({"StartCTF.txt"}));
	// The level war's are for the stair in the parameter's top digits.
	CHECK(notice(3, 100000005) == Names({"LevelWar1Slayer.txt"}));
	CHECK(notice(3, 199999999) == Names({"LevelWar1Slayer.txt"}));
	CHECK(notice(3, 200000005).empty());
	CHECK(notice(3, 99999999).empty());
	CHECK(notice(4, 100000000) == Names({"StartLevelWar1Slayer.txt"}));
	CHECK(notice(4, 300000000).empty());
	CHECK(notice(5, 0) == Names({"RaceWarSlayer.txt"}));
	CHECK(notice(6, 0) == Names({"StartRaceWarSlayer.txt"}));
	// The pet quest's date is the game's.
	scene.gameTime.SetStartTime(MonotonicClock::TimePoint(), 2026, 10, 15, 12, 0, 0);
	CHECK(notice(7, 0) == Names({"2ndPetQuestSlayer.txt"}));
	// The SMS notice opens its window; other ids are nothing.
	CHECK(notice(8, 0).empty());
	CHECK(notice(0, 0).empty());
	CHECK(notice(9, 0).empty());
	CHECK(notice(0xFFFFFFFFu, 0).empty());
	// A slayer past the last stair is stair 0, which a parameter under
	// 100000000 names.
	g_char_slot_ingame.INT_PURE = 281;
	CHECK(notice(3, 5) == Names({"LevelWar0Slayer.txt"}));
	CHECK(notice(3, 100000005).empty());
	// A vampire's and an ousters' stair is its level.
	scene.Be(CREATURETYPE_VAMPIRE_MALE1);
	scene.me->MStatus::SetStatus(MODIFY_LEVEL, 60);
	CHECK(notice(1, 0) == Names({"BeginnerZoneVampire.txt"}));
	CHECK(notice(2, 0) == Names({"StartCTF.txt"}));
	CHECK(notice(3, 300000000) == Names({"LevelWar3Vampire.txt"}));
	CHECK(notice(5, 0) == Names({"RaceWarVampire.txt"}));
	CHECK(notice(6, 0) == Names({"StartRaceWarVampire.txt"}));
	CHECK(notice(7, 0) == Names({"2ndPetQuestVampire.txt"}));
	scene.Be(CREATURETYPE_OUSTERS);
	scene.me->MStatus::SetStatus(MODIFY_LEVEL, 85);
	CHECK(notice(4, 499999999) == Names({"StartLevelWar4Ousters.txt"}));
	CHECK(notice(1, 0) == Names({"BeginnerZoneOusters.txt"}));
	CHECK(notice(5, 0) == Names({"RaceWarOusters.txt"}));
	// A player of no race reads as an ousters.
	scene.Be(NPC_TYPE);
	CHECK(notice(1, 0) == Names({"BeginnerZoneOusters.txt"}));
	CHECK(calls.empty());
}
#endif

//----------------------------------------------------------------------
// The other player's info
//----------------------------------------------------------------------
TEST(GameUI3OtherInfo, ASlayersInfoGoesOnlyToTheGameWindow)
{
	// The info, the slayer's face among it (its hair frame less its sex's
	// first hair frame), goes to the game window, which is not built here:
	// nothing changes.
	Scene scene;
	MCreatureWear slayer;
	slayer.SetCreatureType(CREATURETYPE_SLAYER_MALE);
	slayer.SetName("Kim");
	const std::pair<bool, TYPE_FRAMEID> faces[] = {
		{true, ADDONID_HAIR1_MALE}, {true, ADDONID_HAIR2_MALE}, {true, ADDONID_HAIR3_MALE},
		{false, ADDONID_HAIR1_FEMALE}, {false, ADDONID_HAIR2_FEMALE}, {false, ADDONID_HAIR3_FEMALE},
	};
	for (const auto& face : faces)
	{
		slayer.SetMale(face.first);
		slayer.SetAddonHair(face.second, 0);
		UI_RunOtherInfo(&slayer);
	}
	CHECK(slayer.IsSlayer());
	CHECK(UI_GetOtherInfoName() == NULL);
	CHECK(calls.empty());
}

TEST(GameUI3OtherInfo, ASlayersFaceOutsideItsSexsThreeHairsIsTheFirst)
{
	// The hair is set with the sex, but a corpse packet of the other sex
	// changes an existing slayer's sex and not its hair (GCAddSlayerCorpse
	// sets the creature type), and a slayer whose hair was never set has
	// the first male one: a face outside its sex's three is the first.
	Scene scene;
	MCreatureWear slayer;
	slayer.SetCreatureType(CREATURETYPE_SLAYER_MALE);
	slayer.SetName("Kim");
	const TYPE_FRAMEID hairs[] = {ADDONID_HAIR1_FEMALE, ADDONID_HAIR3_FEMALE, ADDONID_HAIR1_MALE, ADDONID_HAIR3_MALE,
		0, MAX_ADDONID_SLAYER, 0xFFFF};
	for (bool male : {true, false})
		for (TYPE_FRAMEID hair : hairs)
		{
			slayer.SetMale(male);
			slayer.SetAddonHair(hair, 0);
			UI_RunOtherInfo(&slayer);
		}
	CHECK(UI_GetOtherInfoName() == NULL);
	CHECK(calls.empty());
}

//----------------------------------------------------------------------
// What the other player's info holds (FillOtherInfo), read straight.
//----------------------------------------------------------------------
namespace {

// A record whose every field is a mark the fill must replace.
C_VS_UI_OTHER_INFO::PLAYER_INFO Marked(bool female)
{
	C_VS_UI_OTHER_INFO::PLAYER_INFO info;
	info.PLAYER_NAME = "mark";
	for (int& level : info.DOMAINLEVEL)
		level = -1;
	info.GRADE = info.LEVEL = info.FAME = info.ALIGNMENT = -1;
	info.STR_CUR = info.DEX_CUR = info.INT_CUR = -1;
	info.STR_PURE = info.DEX_PURE = info.INT_PURE = -1;
	info.guild_id = info.face_type = -1;
	info.bl_female = female;
	return info;
}

// The stats the info shows, each its own.
void SetStats(MCreature& creature)
{
	creature.SetGuildNumber(77);
	creature.MStatus::SetStatus(MODIFY_BLADE_DOMAIN_LEVEL, 11);
	creature.MStatus::SetStatus(MODIFY_SWORD_DOMAIN_LEVEL, 12);
	creature.MStatus::SetStatus(MODIFY_GUN_DOMAIN_LEVEL, 13);
	creature.MStatus::SetStatus(MODIFY_HEAL_DOMAIN_LEVEL, 14);
	creature.MStatus::SetStatus(MODIFY_ENCHANT_DOMAIN_LEVEL, 15);
	creature.MStatus::SetStatus(MODIFY_RANK, 21);
	creature.MStatus::SetStatus(MODIFY_LEVEL, 22);
	creature.MStatus::SetStatus(MODIFY_FAME, 23);
	creature.MStatus::SetStatus(MODIFY_ALIGNMENT, DWORD(-500));
	creature.MStatus::SetStatus(MODIFY_CURRENT_STR, 31);
	creature.MStatus::SetStatus(MODIFY_CURRENT_DEX, 32);
	creature.MStatus::SetStatus(MODIFY_CURRENT_INT, 33);
	creature.MStatus::SetStatus(MODIFY_BASIC_STR, 41);
	creature.MStatus::SetStatus(MODIFY_BASIC_DEX, 42);
	creature.MStatus::SetStatus(MODIFY_BASIC_INT, 43);
}

void CheckStats(const C_VS_UI_OTHER_INFO::PLAYER_INFO& info)
{
	CHECK_EQ(11, info.DOMAINLEVEL[SKILLDOMAIN_BLADE]);
	CHECK_EQ(12, info.DOMAINLEVEL[SKILLDOMAIN_SWORD]);
	CHECK_EQ(13, info.DOMAINLEVEL[SKILLDOMAIN_GUN]);
	CHECK_EQ(14, info.DOMAINLEVEL[SKILLDOMAIN_HEAL]);
	CHECK_EQ(15, info.DOMAINLEVEL[SKILLDOMAIN_ENCHANT]);
	CHECK_EQ(21, info.GRADE);
	CHECK_EQ(22, info.LEVEL);
	CHECK_EQ(23, info.FAME);
	CHECK_EQ(-500, info.ALIGNMENT);
	CHECK_EQ(31, info.STR_CUR);
	CHECK_EQ(32, info.DEX_CUR);
	CHECK_EQ(33, info.INT_CUR);
	CHECK_EQ(41, info.STR_PURE);
	CHECK_EQ(42, info.DEX_PURE);
	CHECK_EQ(43, info.INT_PURE);
	CHECK_EQ(77, info.guild_id);
}

} // namespace

TEST(GameUI3OtherInfo, TheInfoHoldsASlayersNameStatsSexAndFace)
{
	// The face is the slayer's hair frame less its sex's first; one
	// outside the sex's three is the first face.
	Scene scene;
	MCreatureWear slayer;
	slayer.SetCreatureType(CREATURETYPE_SLAYER_MALE);
	slayer.SetName("Kim");
	SetStats(slayer);
	struct Face { bool male; int hair; int face; };
	const Face faces[] = {
		{true, ADDONID_HAIR1_MALE, M_FACE1}, {true, ADDONID_HAIR2_MALE, M_FACE2}, {true, ADDONID_HAIR3_MALE, M_FACE3},
		{true, ADDONID_HAIR1_MALE - 1, M_FACE1}, {true, ADDONID_HAIR3_MALE + 1, M_FACE1},
		{false, ADDONID_HAIR1_FEMALE, W_FACE1}, {false, ADDONID_HAIR2_FEMALE, W_FACE2}, {false, ADDONID_HAIR3_FEMALE, W_FACE3},
		{false, ADDONID_HAIR1_FEMALE - 1, W_FACE1}, {false, ADDONID_HAIR3_FEMALE + 1, W_FACE1},
	};
	for (const Face& face : faces)
	{
		slayer.SetMale(face.male);
		slayer.SetAddonHair(TYPE_FRAMEID(face.hair), 0);
		C_VS_UI_OTHER_INFO::PLAYER_INFO info = Marked(face.male);
		FillOtherInfo(info, &slayer);
		CHECK(std::string("Kim") == info.PLAYER_NAME);
		CHECK_EQ(!face.male, info.bl_female);
		CHECK_EQ(face.face, info.face_type);
		CheckStats(info);
	}
}

TEST(GameUI3OtherInfo, TheInfoLeavesANonSlayersFaceAndANamelessCreaturesName)
{
	// Only a slayer has a face here, and a creature without a name keeps
	// the record's.
	Scene scene;
	MCreatureWear vampire;
	vampire.SetCreatureType(CREATURETYPE_VAMPIRE_MALE1);
	vampire.SetMale(false);
	vampire.SetAddonHair(ADDONID_HAIR2_FEMALE, 0);
	SetStats(vampire);
	CHECK(vampire.GetName() == NULL);
	C_VS_UI_OTHER_INFO::PLAYER_INFO info = Marked(false);
	FillOtherInfo(info, &vampire);
	CHECK(std::string("mark") == info.PLAYER_NAME);
	CHECK_EQ(-1, info.face_type);
	CHECK(info.bl_female);
	CheckStats(info);
}

//----------------------------------------------------------------------
// The calls only the game's windows see
//----------------------------------------------------------------------
TEST(GameUI3NoUI, TheWindowsCallsAreHarmlessWithoutTheWindows)
{
	// Without the game and title windows these reach nothing and change
	// nothing of the model, and the questions answer "no".
	Scene scene;
	AddQuest(scene.quests, 501, QUEST_INFO_MONSTER_KILL, 60, 0, 0, 7, "Wolf");
	AddQuest(scene.quests, 502, QUEST_INFO_MONSTER_KILL, 60, 0, 0, 7, "Event", 1, 2);
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	g_char_slot_ingame.level = 33;
	const bool wasShown = CursorShown();
	char text[] = "text";
	CHECK(UI_GetOtherInfoName() == NULL);
	CHECK(!UI_IsRunningSwapAdvancementItem());
	CHECK(!UI_IsRunning_WebBrowser());
	CHECK_EQ(0, UI_GetMouseCursorInfo_WebBrowser());
	UI_RunOtherInfo(NULL);
	UI_CloseEnchant();
	UI_SetTotalFee(100);
	UI_ClearNPC();
	UI_CloseBloodBibleStatus();
	UI_RunBloodBibleStatus();
	UI_SetBloodBibleStatusTimer(30);
	for (Storage_t storage : { Storage_t(STORAGE_INVENTORY), Storage_t(STORAGE_ZONE), Storage_t(STORAGE_CORPSE) })
		for (Race_t race : { Race_t(RACE_SLAYER), Race_t(RACE_VAMPIRE), Race_t(RACE_OUSTERS) })
		{
			GCBloodBibleStatus bible;
			bible.setItemType(3);
			bible.setZoneID(1101);
			bible.setStorage(storage);
			bible.setOwnerName("Kim");
			bible.setRace(race);
			bible.setShrineRace(race);
			bible.setX(10);
			bible.setY(20);
			UI_SetBloodBibleStatus(&bible);
		}
	UI_RunAskGoBilingPage(0);
	UI_RunAskGoBilingPage(1);
	UI_RunAskGoBilingPage(2);
	UI_RunQuestStatus(NULL);
	for (WORD id : { WORD(501), WORD(502), WORD(600) })
	{
		GCQuestStatus status;
		status.setQuestID(id);
		status.setCurrentNul(3);
		status.setRemainTime(90);
		UI_RunQuestStatus(&status);
	}
	UI_InitQuestStatus();
	UI_CloseQuestStatus();
	UI_RunPopupMessage(1);
	UI_LotteryResult(true);
	UI_RunLottery(2);
	UI_RunImageNotice("notice", 4);
	UI_REQUEST_DIE_TIMER_RESET();
	UI_RemoveItemFromItemShop(11);
	UI_CloseTransItem();
	for (int game = 0; game <= 3; ++game) UI_RunMinigame(game);
	UI_SetCTFStatus(MonotonicClock::TimePoint(), 1, 2, 3);
	UI_CloseCTFStatusWindow();
	UI_SetDelayEternity();
	UI_CloseRequestResurrectWindow();
	UI_RefreshInfoImage();
	UI_DeleteNotice(3);
	UI_RunPetInfo(NULL);
	UI_SetSMSList(NULL);
	UI_AddSMSList(1, text, text, text);
	UI_DeleteSMSList(1);
	UI_SMS_Set_Charge(5);
	UI_RunRangerChat();
	UI_SetRangerChatString(text);
	UI_CloseRangerChat();
	UI_Run_Campaign_Help_Unfortunate_Neighbors(1000);
	UI_Run_Confirm_GetEventItem(2);
	UI_Run_WebBrowser(text);
	UI_Close_WebBrowser();
	for (int type : { int(CREATURETYPE_SLAYER_MALE), int(CREATURETYPE_VAMPIRE_MALE1), int(CREATURETYPE_OUSTERS), NPC_TYPE })
	{
		scene.Be(type);
		UI_ResetQuickItemSlot();
		UI_RunHorn(1101);
		UI_RunWelcomeMessage();
	}
	// The SMS notice opens its window (GameUI3Notice has the others).
	UI_RunNotice(8, 0);
	MPlayer* pPlayer = g_pPlayer;
	g_pPlayer = NULL;
	UI_RunWelcomeMessage();
	g_pPlayer = pPlayer;
	CHECK(calls.empty());
	CHECK(scene.me->GetWaitVerify() == MPlayer::WAIT_VERIFY_TRADE);
	CHECK(scene.temp.GetMode() == TempInformation::MODE_TRADE_REQUEST);
	CHECK_EQ(33, g_char_slot_ingame.level);
	CHECK(Lines(scene.systemMessages).empty());
	CHECK_EQ(wasShown, CursorShown());
	CHECK_EQ(0, int(UIDialog::IsLockInput()));
}
