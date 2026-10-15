#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "UIFunction.h"
#include "MPlayer.h"
#include "MZone.h"
#include "ZoneFlags.h"
#include "FrameClock.h"
#include "MonotonicClock.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MActionInfoTable.h"
#include "MEffectStatusTable.h"
#include "MEffectSpriteTypeTable.h"
#include "EffectSpriteTypeDef.h"
#include "RankBonusTable.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "MInventory.h"
#include "MSlayerGear.h"
#include "MTradeManager.h"
#include "MStorage.h"
#include "MSkillManager.h"
#include "SkillDef.h"
#include "TempInformation.h"
#include "CServerInformation.h"
#include "Properties.h"
#include "UIDialog.h"
#include "DialogWindowHost.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "UserOption.h"
#include "KeyAccelerator.h"
#include "AcceleratorManager.h"
#include "Gpackets/GCFriendChatting.h"
#include "VS_UI.h"
#include "VS_UI_Base.h"
#include "VS_UI_GameCommon.h"
#include "VS_UI_GlobalResource.h"
#include "VS_UI_Dialog.h"
#include "VS_UI_Message.h"
#include "VS_UI_descriptor.h"
#include "VS_UI_item.h"
#include "VS_UI_mouse_pointer.h"
#include "VS_UI_UIMessage.h"
#include "pi_core.h"
#include "u_window.h"
#include "timer2.h"
#include "Fl2.h"
#include "Ci.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackendSDL.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

// Defined in GameUI.cpp, declared by no header (UIFunction.h names them
// ..._PUR, which nothing defines).
void UI_SetCharInfoSTR_PURE(int value);
void UI_SetCharInfoDEX_PURE(int value);
void UI_SetCharInfoINT_PURE(int value);
extern S_SLOT g_char_slot_ingame;
extern C_VS_UI_FRIEND_INFO* gpC_vs_ui_friend_info;

// The game's calls into the UI (Client/GameUI.cpp, gameclient's since task
// 4.256), its first part (to UI_UnlockItemTrade): what each does to the
// model - the character window's sheet (g_char_slot_ingame), its effect
// statuses, the trade and its request, the storage, the user's record and
// options, the descriptor, the friend window's lists - and what it asks of
// the GameUI host and the UI's base. gC_vs_ui is never initialised here,
// so every C_VS_UI method that checks for its title or game UI does
// nothing and the IsRunning* questions answer false: the one-line
// forwards are only shown to be harmless without a UI.
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

// The test's clock: Now() is wherever the test puts it.
const MonotonicClock::TimePoint kOrigin = MonotonicClock::TimePoint() + MonotonicClock::Millis(5000000);
MonotonicClock::TimePoint now = kOrigin;
MonotonicClock::TimePoint Now() { return now; }
long long MsAfter(MonotonicClock::TimePoint origin, MonotonicClock::TimePoint t)
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(t - origin).count();
}

// The character sheet, emptied for the test and given back after.
struct SlotScope
{
	S_SLOT old = g_char_slot_ingame;
	SlotScope() { g_char_slot_ingame = S_SLOT(); }
	~SlotScope() { g_char_slot_ingame = old; }
};

// The UI's base, initialised with this file's receiver: what the game
// queues on it (gpC_base->SendMessage) comes out of Drain in order.
void Received(DWORD message, intptr_t left, intptr_t right, void* pVoid)
{
	calls.push_back("ui " + N(message) + "|" + N(left) + "|" + N(right) + (pVoid != NULL ? "|ptr" : ""));
}
void NoTimer() {}

struct UiBase
{
	Base* oldBase = gpC_base;
	CI* oldIme = gC_ci;
	RECT oldRect = g_GameRect;
	CI_KOREAN ime;
	CSpriteSurface back;
	Base base;
	timer_id_t blink = INVALID_TID;
	UiBase()
	{
		spritectl_init();
		g_GameRect = {0, 0, 10, 8};
		back.Init(10, 8);
		gC_ci = &ime;
		// Base::Init starts the IME's cursor blinker, the timer after this
		// one; it is deleted again when the test ends.
		timer_id_t probe = gC_timer2.Add(1000, NoTimer);
		blink = probe + 1;
		gC_timer2.Delete(probe);
		base.Init(&back, Received);
		gpC_base = &base;
		calls.clear();
	}
	~UiBase()
	{
		const bool blinkerGone = gC_timer2.Delete(blink);
		CHECK(blinkerGone);
		g_SetFL2Surface(nullptr);
		gpC_base = oldBase;
		gC_ci = oldIme;
		g_GameRect = oldRect;
	}
	void Drain() { for (int i = 0; i < 8; ++i) base.DispatchMessage(); }
};

// The windows a VS_UI window registers with.
struct Windows
{
	PI_ProcessRunner* oldRunner = gpC_process_runner;
	WindowManager* oldWindows = gpC_window_manager;
	PI_ProcessRunner runner;
	WindowManager windows;
	Windows() { gpC_process_runner = &runner; gpC_window_manager = &windows; }
	~Windows() { gpC_window_manager = oldWindows; gpC_process_runner = oldRunner; }
};

} // namespace

//----------------------------------------------------------------------
// GameUI::SetHost
//----------------------------------------------------------------------
namespace {
std::vector<int> loadedZones;
const GameUI::Host zoneHost = []() {
	GameUI::Host host{};
	host.LoadZoneInfo = [](int zoneID) { loadedZones.push_back(zoneID); };
	return host;
}();
// A host without LoadZoneInfo.
const GameUI::Host messageHost = []() {
	GameUI::Host host{};
	host.ExecuteUIMessage = [](DWORD, intptr_t, intptr_t, void*) { loadedZones.push_back(-1); };
	return host;
}();
} // namespace

TEST(GameUI1Host, SetHostAnswersTheHostItReplaces)
{
	const GameUI::Host* old = GameUI::SetHost(&zoneHost);
	CHECK(GameUI::SetHost(&messageHost) == &zoneHost);
	CHECK(GameUI::SetHost(NULL) == &messageHost);
	CHECK(GameUI::SetHost(old) == NULL);
}

//----------------------------------------------------------------------
// UI_ChangeInterfaceRace: in a zone, the host loads the zone's
// information for the zone the player is in (the large one's id while he
// is in it, else the small one's).
//----------------------------------------------------------------------
namespace {
struct RaceScene
{
	MZone* oldZone = g_pZone;
	bool oldInLarge = g_bZonePlayerInLarge;
	int oldLarge = g_nZoneLarge;
	int oldSmall = g_nZoneSmall;
	Race oldInterface = g_eRaceInterface;
	const GameUI::Host* oldHost = GameUI::SetHost(&zoneHost);
	MZone zone{12, 9};
	RaceScene()
	{
		loadedZones.clear();
		g_pZone = &zone;
		g_nZoneLarge = 1001;
		g_nZoneSmall = 2002;
		g_bZonePlayerInLarge = true;
	}
	~RaceScene()
	{
		GameUI::SetHost(oldHost);
		g_eRaceInterface = oldInterface;
		g_nZoneSmall = oldSmall;
		g_nZoneLarge = oldLarge;
		g_bZonePlayerInLarge = oldInLarge;
		g_pZone = oldZone;
	}
};
} // namespace

TEST(GameUI1InterfaceRace, InAZoneTheHostLoadsThePlayersZone)
{
	RaceScene scene;
	UI_ChangeInterfaceRace(RACE_VAMPIRE);
	CHECK(std::vector<int>({1001}) == loadedZones);
	g_bZonePlayerInLarge = false;
	UI_ChangeInterfaceRace(RACE_SLAYER);
	CHECK(std::vector<int>({1001, 2002}) == loadedZones);
	// Every race, and none, asks the same: the race changes only the UI.
	UI_ChangeInterfaceRace(RACE_OUSTERS);
	UI_ChangeInterfaceRace(RACE_MAX);
	CHECK(std::vector<int>({1001, 2002, 2002, 2002}) == loadedZones);
	// GameUI leaves the interface's race to the UI.
	CHECK_EQ(int(scene.oldInterface), int(g_eRaceInterface));
}

TEST(GameUI1InterfaceRace, NoZoneNoHostOrNoEntryLoadsNothing)
{
	RaceScene scene;
	g_pZone = NULL;
	UI_ChangeInterfaceRace(RACE_SLAYER);
	CHECK(loadedZones.empty());
	g_pZone = &scene.zone;
	GameUI::SetHost(NULL);
	UI_ChangeInterfaceRace(RACE_SLAYER);
	GameUI::SetHost(&messageHost);
	UI_ChangeInterfaceRace(RACE_SLAYER);
	CHECK(loadedZones.empty());
}

TEST(GameUI1InterfaceRace, TheInterfaceRaceIsOneOfTheThreeElseSlayer)
{
	Race old = g_eRaceInterface;
	g_eRaceInterface = RACE_VAMPIRE;
	CHECK_EQ(int(RACE_VAMPIRE), int(UI_GetInterfaceRace()));
	g_eRaceInterface = RACE_OUSTERS;
	CHECK_EQ(int(RACE_OUSTERS), int(UI_GetInterfaceRace()));
	g_eRaceInterface = RACE_SLAYER;
	CHECK_EQ(int(RACE_SLAYER), int(UI_GetInterfaceRace()));
	g_eRaceInterface = RACE_MAX;
	CHECK_EQ(int(RACE_SLAYER), int(UI_GetInterfaceRace()));
	g_eRaceInterface = old;
}

//----------------------------------------------------------------------
// The character window's sheet
//----------------------------------------------------------------------
TEST(GameUI1CharInfo, EachSetterWritesItsField)
{
	SlotScope slot;
	char name[] = "Kim";
	UI_SetCharInfoName(name);
	UI_SetCharInfoCreatureType(RACE_OUSTERS);
	UI_SetCharInfoSex(true);
	UI_SetCharInfoLevel(41);
	UI_SetCharInfoSTR(11);
	UI_SetCharInfoDEX(12);
	UI_SetCharInfoINT(13);
	UI_SetCharInfoSTR_MAX(21);
	UI_SetCharInfoDEX_MAX(22);
	UI_SetCharInfoINT_MAX(23);
	UI_SetCharInfoSTR_PURE(31);
	UI_SetCharInfoDEX_PURE(32);
	UI_SetCharInfoINT_PURE(33);
	UI_SetCharInfoDefense(51);
	UI_SetCharInfoProtection(52);
	UI_SetCharInfoTOHIT(53);
	UI_SetCharInfoSilverDamage(54);
	UI_SetCharInfoSilverDamage(7, 9);	// min, max
	UI_SetCharInfoHP(61);
	UI_SetCharInfoMP(62);
	UI_SetCharInfoHP_max(63);
	UI_SetCharInfoMP_max(64);
	UI_SetCharInfoFAME(71);
	UI_SetBonusPoint(72);
	UI_SetSkillPoint(73);
	const S_SLOT& s = g_char_slot_ingame;
	CHECK(s.sz_name == "Kim");
	// A copy: the caller's buffer is not kept.
	name[0] = 'J';
	CHECK(s.sz_name == "Kim");
	CHECK_EQ(int(RACE_OUSTERS), int(s.Race));
	CHECK(s.bl_female);
	CHECK_EQ(41, s.level);
	CHECK_EQ(41, UI_GetCharInfoLevel());
	CHECK_EQ(11, s.STR_CUR);
	CHECK_EQ(12, s.DEX_CUR);
	CHECK_EQ(13, s.INT_CUR);
	CHECK_EQ(21, s.STR_MAX);
	CHECK_EQ(22, s.DEX_MAX);
	CHECK_EQ(23, s.INT_MAXX);
	CHECK_EQ(31, s.STR_PURE);
	CHECK_EQ(32, s.DEX_PURE);
	CHECK_EQ(33, s.INT_PURE);
	CHECK_EQ(51, s.DEFENSE);
	CHECK_EQ(52, s.PROTECTION);
	CHECK_EQ(53, s.TOHIT);
	CHECK_EQ(54, s.SILVER_HP);
	CHECK_EQ(9, s.SILVER_DAM);
	CHECK_EQ(7, s.SILVER_DAM2);
	CHECK_EQ(61, s.HP);
	CHECK_EQ(62, s.MP);
	CHECK_EQ(63, s.HP_MAX);
	CHECK_EQ(64, s.MP_MAX);
	CHECK_EQ(71, s.FAME);
	CHECK_EQ(72, s.bonus_point);
	CHECK_EQ(73, s.skill_point);
	// Nothing else on the sheet moved.
	CHECK_EQ(0, s.DAM);
	CHECK_EQ(0, s.DAM2);
	CHECK(!s.bl_drained);
	CHECK(s.STATUS.empty());
	UI_SetCharInfoSex(false);
	CHECK(!g_char_slot_ingame.bl_female);
	UI_SetCharInfoLevel(-3);
	CHECK_EQ(-3, UI_GetCharInfoLevel());
}

TEST(GameUI1CharInfo, DamageTakesTheMaximumAsTheMinimumWhenNoneIsGiven)
{
	SlotScope slot;
	UI_SetCharInfoDAM(40, 25);	// max, min
	CHECK_EQ(40, g_char_slot_ingame.DAM);
	CHECK_EQ(25, g_char_slot_ingame.DAM2);
	UI_SetCharInfoDAM(30);
	CHECK_EQ(30, g_char_slot_ingame.DAM);
	CHECK_EQ(30, g_char_slot_ingame.DAM2);
	// -1 is the header's "none"; -2 is a minimum like any other.
	UI_SetCharInfoDAM(30, -1);
	CHECK_EQ(30, g_char_slot_ingame.DAM2);
	UI_SetCharInfoDAM(30, -2);
	CHECK_EQ(-2, g_char_slot_ingame.DAM2);
	UI_SetCharInfoDAM(30, 0);
	CHECK_EQ(0, g_char_slot_ingame.DAM2);
}

//----------------------------------------------------------------------
// UI_AddEffectStatus, UI_RemoveEffectStatus: the effect statuses on the
// character window, one per action info below the first result action
// info, each with its expiry (now + delayFrame frames of 1/16 s).
//----------------------------------------------------------------------
namespace {
struct EffectScene
{
	SlotScope slot;
	EFFECTSTATUS_TABLE statuses;
	MActionInfoTable actions;
	EFFECTSTATUS_TABLE* oldStatuses = g_pEffectStatusTable;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	MonotonicClock::ScopedTestSource clock{Now};
	// Effect status 1: action info 10; 2: blood drain; 3: the last action
	// info under the result ones (300); 4: the first result one. 0 is the
	// table's own default (ACTIONINFO_NULL).
	EffectScene()
	{
		now = kOrigin;
		statuses.Init(5);
		testfw::MutableRow(statuses, 1).OriginalActionInfo = 10;
		testfw::MutableRow(statuses, 2).OriginalActionInfo = SKILL_BLOOD_DRAIN;
		testfw::MutableRow(statuses, 3).OriginalActionInfo = 299;
		testfw::MutableRow(statuses, 4).OriginalActionInfo = 300;
		g_pEffectStatusTable = &statuses;
		actions.SetMinResultActionInfo(300);
		g_pActionInfoTable = &actions;
	}
	~EffectScene()
	{
		g_pActionInfoTable = oldActions;
		g_pEffectStatusTable = oldStatuses;
	}
	S_SLOT::UI_EFFECTSTATUS_TYPE& Status() { return g_char_slot_ingame.STATUS; }
	// "actionInfo@ms after kOrigin" for each status, in order.
	std::vector<std::string> Shown()
	{
		std::vector<std::string> shown;
		for (const S_SLOT::UI_EFFECTSTATUS_STRUCT& efs : Status())
			shown.push_back(N(efs.actionInfo) + "@" + N(MsAfter(kOrigin, efs.delayFrame)));
		return shown;
	}
	void Put(TYPE_ACTIONINFO ai)
	{
		S_SLOT::UI_EFFECTSTATUS_STRUCT efs;
		efs.actionInfo = ai;
		efs.delayFrame = kOrigin;
		Status().push_back(efs);
	}
};
using Shown = std::vector<std::string>;
} // namespace

TEST(GameUI1EffectStatus, AnEffectStatusIsAddedOnceWithItsExpiry)
{
	EffectScene scene;
	UI_AddEffectStatus(1, 32);	// 32 frames: 2 s
	CHECK(Shown({"10@2000"}) == scene.Shown());
	CHECK(!g_char_slot_ingame.bl_drained);
	// Again: the same one's expiry moves, from the time now.
	now = kOrigin + MonotonicClock::Millis(500);
	UI_AddEffectStatus(1, 48);
	CHECK(Shown({"10@3500"}) == scene.Shown());
	// No delay given: the expiry stays.
	now = kOrigin + MonotonicClock::Millis(900);
	UI_AddEffectStatus(1, 0);
	CHECK(Shown({"10@3500"}) == scene.Shown());
	// A new one goes at the end; a new one with no delay expires now.
	UI_AddEffectStatus(3, 0);
	UI_AddEffectStatus(1, 1);	// 62.5 ms, rounded down
	CHECK(Shown({"10@962", "299@900"}) == scene.Shown());
}

TEST(GameUI1EffectStatus, BloodDrainMarksTheCharacterDrained)
{
	EffectScene scene;
	UI_AddEffectStatus(2, 16);
	CHECK(Shown({N(SKILL_BLOOD_DRAIN) + "@1000"}) == scene.Shown());
	CHECK(g_char_slot_ingame.bl_drained);
	// One already there: only its expiry moves, the mark is not set again.
	g_char_slot_ingame.bl_drained = false;
	UI_AddEffectStatus(2, 32);
	CHECK(Shown({N(SKILL_BLOOD_DRAIN) + "@2000"}) == scene.Shown());
	CHECK(!g_char_slot_ingame.bl_drained);
	// Another status does not mark it.
	UI_AddEffectStatus(1, 16);
	CHECK(!g_char_slot_ingame.bl_drained);
}

TEST(GameUI1EffectStatus, OnlyActionInfosUnderTheResultOnesAndKnownStatusesShow)
{
	EffectScene scene;
	UI_AddEffectStatus(4, 16);	// action info 300: the first result one
	CHECK(scene.Status().empty());
	UI_AddEffectStatus(3, 16);	// 299
	CHECK(Shown({"299@1000"}) == scene.Shown());
	// The table's last status is known, one past it is not. With every
	// action info under the result ones (0x10000), the table's default
	// row for an unknown status (ACTIONINFO_NULL, 0xFFFF) would show.
	scene.actions.SetMinResultActionInfo(0x10000);
	UI_AddEffectStatus(5, 16);
	CHECK(Shown({"299@1000"}) == scene.Shown());
	UI_AddEffectStatus(0, 16);
	CHECK(Shown({"299@1000", "65535@1000"}) == scene.Shown());
}

TEST(GameUI1EffectStatus, ANegativeStatusReadsTheTablesDefaultRow)
{
	// Recorded defect: es is only checked against the table's size, so a
	// negative one is not rejected (GameUI.cpp:351, :409). The table's
	// range check answers its default row (ACTIONINFO_NULL), which shows
	// when every action info is under the result ones.
	EffectScene scene;
	UI_AddEffectStatus(-1, 16);
	CHECK(scene.Status().empty());
	scene.actions.SetMinResultActionInfo(0x10000);
	UI_AddEffectStatus(-1, 16);
	CHECK(Shown({"65535@1000"}) == scene.Shown());
	UI_RemoveEffectStatus(-1);
	CHECK(scene.Status().empty());
}

TEST(GameUI1EffectStatus, AHugeDelayWrapsTheMillisecondCount)
{
	// Recorded defect: delayFrame*1000 is a DWORD product (GameUI.cpp:376,
	// :389), so past 4294967 frames it wraps: 4294968 frames give 44 ms.
	EffectScene scene;
	UI_AddEffectStatus(1, 4294967);
	CHECK(Shown({"10@268435437"}) == scene.Shown());
	UI_AddEffectStatus(1, 4294968);
	CHECK(Shown({"10@44"}) == scene.Shown());
}

TEST(GameUI1EffectStatus, RemovingTakesTheFirstOfItsActionInfo)
{
	EffectScene scene;
	scene.Put(10);
	scene.Put(299);
	scene.Put(10);
	UI_RemoveEffectStatus(1);
	CHECK(Shown({"299@0", "10@0"}) == scene.Shown());
	UI_RemoveEffectStatus(1);
	CHECK(Shown({"299@0"}) == scene.Shown());
	// None left: nothing.
	UI_RemoveEffectStatus(1);
	CHECK(Shown({"299@0"}) == scene.Shown());
	UI_RemoveEffectStatus(3);
	CHECK(scene.Status().empty());
}

TEST(GameUI1EffectStatus, RemovingBloodDrainClearsTheMark)
{
	EffectScene scene;
	scene.Put(SKILL_BLOOD_DRAIN);
	scene.Put(10);
	g_char_slot_ingame.bl_drained = true;
	UI_RemoveEffectStatus(1);
	CHECK(g_char_slot_ingame.bl_drained);
	UI_RemoveEffectStatus(2);
	CHECK(scene.Status().empty());
	CHECK(!g_char_slot_ingame.bl_drained);
	// Not there: the mark stays.
	g_char_slot_ingame.bl_drained = true;
	UI_RemoveEffectStatus(2);
	CHECK(g_char_slot_ingame.bl_drained);
}

TEST(GameUI1EffectStatus, RemovingOnlyLooksUnderTheResultOnesAndInTheTable)
{
	EffectScene scene;
	scene.Put(300);
	UI_RemoveEffectStatus(4);
	CHECK_EQ(1, int(scene.Status().size()));
	scene.Status().clear();
	scene.Put(65535);
	scene.actions.SetMinResultActionInfo(0x10000);
	UI_RemoveEffectStatus(5);
	CHECK_EQ(1, int(scene.Status().size()));
	UI_RemoveEffectStatus(0);
	CHECK(scene.Status().empty());
}

//----------------------------------------------------------------------
// The friend window's lists (gpC_vs_ui_friend_info, dereferenced
// unchecked: a window is built for these tests).
//----------------------------------------------------------------------
namespace {
using FriendList = C_VS_UI_FRIEND_INFO::FRIEND_LIST;
struct FriendScene
{
	UiBase ui;	// the history measures its lines with the base's font
	Windows windows;
	C_VS_UI_WINDOW_MANAGER* oldUiWindows = gpC_vs_ui_window_manager;
	C_VS_UI_FRIEND_INFO* oldFriends = gpC_vs_ui_friend_info;
	C_VS_UI_WINDOW_MANAGER uiWindows;
	std::unique_ptr<C_VS_UI_FRIEND_INFO> info;
	std::vector<FriendList*> owned;	// the lists' entries; the window frees none
	FriendScene()
	{
		gpC_vs_ui_window_manager = &uiWindows;
		info.reset(new C_VS_UI_FRIEND_INFO);
		gpC_vs_ui_friend_info = info.get();
	}
	~FriendScene()
	{
		for (FriendList* pList : info->m_v_pFriendList) Own(pList);
		for (FriendList* pList : info->m_v_pBlackList) Own(pList);
		for (FriendList* pList : owned)
		{
			for (C_VS_UI_FRIEND_INFO::HISTORY_LIST* pLine : pList->m_v_pHistory_List) delete pLine;
			delete pList;
		}
		info.reset();
		gpC_vs_ui_friend_info = oldFriends;
		gpC_vs_ui_window_manager = oldUiWindows;
	}
	void Own(FriendList* pList)
	{
		for (FriendList* p : owned) if (p == pList) return;
		owned.push_back(pList);
	}
	FriendList* Add(std::vector<FriendList*>& list, const char* name, int online = 0, int show = 0)
	{
		FriendList* pList = new FriendList;
		pList->Name = name;
		pList->blIsOnline = online;
		pList->blIsShow = show;
		list.push_back(pList);
		Own(pList);
		return pList;
	}
	std::string Names(const std::vector<FriendList*>& list)
	{
		std::string names;
		for (FriendList* pList : list) names += pList->Name + (pList->blIsOnline ? "+" : "") + ";";
		return names;
	}
};
GCFriendChatting Chat(const char* name, int black = 0, int online = 0, const char* message = "")
{
	GCFriendChatting packet;
	packet.setPlayerName(name);
	packet.setIsBlack(BYTE(black));
	packet.setIsOnLine(BYTE(online));
	packet.setMessage(message);
	return packet;
}
} // namespace

TEST(GameUI1Friends, AFriendsLineUpdatesTheFriendOrAddsOne)
{
	FriendScene scene;
	std::vector<FriendList*>& friends = scene.info->m_v_pFriendList;
	FriendList* pAnn = scene.Add(friends, "Ann");
	scene.Add(friends, "Bob", 0, 1);
	GCFriendChatting ann = Chat("Ann", 0, 1);
	UI_FriendChatting_UpdateFriendList(&ann);
	CHECK_EQ(1, pAnn->blIsOnline);
	// Sorted after: the one shown first.
	CHECK("Bob;Ann+;" == scene.Names(friends));
	GCFriendChatting cid = Chat("Cid", 0, 1);
	UI_FriendChatting_UpdateFriendList(&cid);
	CHECK_EQ(3, int(friends.size()));
	CHECK("Bob" == friends[0]->Name);
	CHECK(scene.Names(friends).find("Cid+;") != std::string::npos);
	CHECK(scene.info->m_v_pBlackList.empty());
	// Offline again.
	GCFriendChatting annOff = Chat("Ann", 0, 0);
	UI_FriendChatting_UpdateFriendList(&annOff);
	CHECK_EQ(0, pAnn->blIsOnline);
	CHECK_EQ(3, int(friends.size()));
}

TEST(GameUI1Friends, ABlackListedOnesLineGoesToTheBlackList)
{
	FriendScene scene;
	std::vector<FriendList*>& black = scene.info->m_v_pBlackList;
	scene.Add(scene.info->m_v_pFriendList, "Dan");
	scene.Add(black, "Eve");
	GCFriendChatting dan = Chat("Dan", 1, 1);
	UI_FriendChatting_UpdateFriendList(&dan);
	// A new black-list entry, even for a friend's name; online first.
	CHECK("Dan+;Eve;" == scene.Names(black));
	CHECK("Dan;" == scene.Names(scene.info->m_v_pFriendList));
	GCFriendChatting eve = Chat("Eve", 2, 1);
	UI_FriendChatting_UpdateFriendList(&eve);
	CHECK_EQ(2, int(black.size()));
	CHECK_EQ(1, black[0]->blIsOnline);
	CHECK_EQ(1, black[1]->blIsOnline);
	GCFriendChatting danOff = Chat("Dan", 1, 0);
	UI_FriendChatting_UpdateFriendList(&danOff);
	CHECK("Eve+;Dan;" == scene.Names(black));
}

TEST(GameUI1Friends, AFriendsMessageGoesToTheHistoryAndOpensTheWindow)
{
	FriendScene scene;
	std::vector<FriendList*>& friends = scene.info->m_v_pFriendList;
	scene.Add(friends, "Ann");
	FriendList* pBob = scene.Add(friends, "Bob");
	scene.Add(scene.info->m_v_pBlackList, "Eve");
	// A stranger's, or a black-listed one's: nothing.
	GCFriendChatting stranger = Chat("Zed", 0, 1, "hi");
	UI_FriendChatting_Message(&stranger);
	GCFriendChatting eve = Chat("Eve", 0, 1, "hi");
	UI_FriendChatting_Message(&eve);
	CHECK(!scene.info->Running());
	CHECK("Ann;Bob;" == scene.Names(friends));
	CHECK(pBob->m_v_pHistory_List.empty());
	GCFriendChatting bob = Chat("Bob", 0, 1, "hello");
	UI_FriendChatting_Message(&bob);
	CHECK(!pBob->m_v_pHistory_List.empty());
	std::string history;
	for (C_VS_UI_FRIEND_INFO::HISTORY_LIST* pLine : pBob->m_v_pHistory_List) history += pLine->strLine;
	CHECK(history.rfind("Bob ", 0) == 0);
	CHECK(history.size() >= 6 && history.compare(history.size() - 6, 6, ":hello") == 0);
	CHECK_EQ(1, pBob->blIsShow);
	CHECK(scene.info->Running());
	// Sorted after: the one shown first. Nothing else changed.
	CHECK("Bob;Ann;" == scene.Names(friends));
	CHECK_EQ(0, pBob->blIsOnline);
}

TEST(GameUI1Friends, DeletingTakesTheFriendElseTheBlackListedOne)
{
	FriendScene scene;
	std::vector<FriendList*>& friends = scene.info->m_v_pFriendList;
	std::vector<FriendList*>& black = scene.info->m_v_pBlackList;
	scene.Add(friends, "Ann");
	scene.Add(friends, "Bob");
	scene.Add(friends, "Ann");
	scene.Add(black, "Ann");
	scene.Add(black, "Eve");
	// The first of the name in the friend list only.
	// Recorded defect: the erased entry is never freed (GameUI.cpp:228,
	// :240); the scene frees it.
	GCFriendChatting ann = Chat("Ann");
	UI_RunFriendDelete(&ann);
	CHECK("Bob;Ann;" == scene.Names(friends));
	CHECK("Ann;Eve;" == scene.Names(black));
	UI_RunFriendDelete(&ann);
	CHECK("Bob;" == scene.Names(friends));
	CHECK("Ann;Eve;" == scene.Names(black));
	UI_RunFriendDelete(&ann);
	CHECK("Bob;" == scene.Names(friends));
	CHECK("Eve;" == scene.Names(black));
	GCFriendChatting zed = Chat("Zed");
	UI_RunFriendDelete(&zed);
	CHECK("Bob;" == scene.Names(friends));
	CHECK("Eve;" == scene.Names(black));
}

TEST(GameUI1Friends, AnAcceptedRequestAsksTheGameToUpdateTheList)
{
	UiBase ui;
	GCFriendChatting ok = Chat("Ann");
	UI_RunFriendOK(&ok);
	ui.Drain();
	CHECK(Same({"ui " + N(UI_FRIEND_CHATTING_UPDATE) + "|0|0"}));
}

//----------------------------------------------------------------------
// The world and server lists
//----------------------------------------------------------------------
namespace {
struct ServerScene
{
	GameModelWorld world;
	UiBase ui;
	CServerInformation* oldServers = g_pServerInformation;
	CServerInformation servers;
	ServerScene()
	{
		ServerGroup* pGroup = new ServerGroup;
		pGroup->SetGroupName("World");
		pGroup->SetGroupStatus(2);
		SERVER_INFO* pServer = new SERVER_INFO;
		pServer->ServerName = "Server";
		pServer->ServerStatus = 1;
		pGroup->AddData(3, pServer);
		servers.AddData(5, pGroup);
		servers.SetServerGroupID(5);
		servers.SetServerID(3);
		g_pServerInformation = &servers;
		g_pUserInformation->WorldID = 5;
		g_pUserInformation->ServerID = 3;
	}
	~ServerScene() { g_pServerInformation = oldServers; }
};
} // namespace

TEST(GameUI1ServerList, NetmarbleConnectsToItsOwnWorldAndServer)
{
	ServerScene scene;
	g_pUserInformation->IsNetmarble = true;
	g_pUserInformation->WorldID = 7;
	g_pUserInformation->ServerID = 9;
	UI_SetWorldList();
	UI_SetServerList();
	scene.ui.Drain();
	CHECK(Same({"ui " + N(UI_CONNECT_SERVER) + "|1|7", "ui " + N(UI_CONNECT_SERVER) + "|0|9"}));
}

TEST(GameUI1ServerList, NetmarbleNeverReadsTheServerInformation)
{
	// A group the information does not hold would be read through NULL by
	// the server list's own path (GameUI.cpp:536-538).
	ServerScene scene;
	g_pUserInformation->IsNetmarble = true;
	scene.servers.Release();
	UI_SetServerList();
	scene.ui.Drain();
	CHECK(Same({"ui " + N(UI_CONNECT_SERVER) + "|0|3"}));
}

TEST(GameUI1ServerList, OtherwiseTheListsGoToTheUIOnly)
{
	// The lists go to the title UI (none here); nothing is queued and the
	// information is left as it was. Recorded defects, not run: an unknown
	// current group is read through NULL (GameUI.cpp:536-538), and a group
	// or server with an empty name (MString's NULL) reaches strlen
	// (GameUI.cpp:484, 556).
	ServerScene scene;
	g_pUserInformation->IsNetmarble = false;
	UI_SetWorldList();
	UI_SetServerList();
	g_pServerInformation = NULL;
	UI_SetWorldList();
	UI_SetServerList();
	g_pServerInformation = &scene.servers;
	scene.ui.Drain();
	CHECK(calls.empty());
	CHECK_EQ(5, int(scene.servers.GetServerGroupID()));
	CHECK_EQ(3, int(scene.servers.GetServerID()));
	CHECK(std::string("World") == scene.servers.GetServerGroupName());
}

//----------------------------------------------------------------------
// UI_SaveUserOption
//----------------------------------------------------------------------
namespace {
const char* kOptionPath = "game_ui_1_user_option.set";
struct OptionScene
{
	Properties* oldFiles = g_pFileDef;
	UserOption* oldOption = g_pUserOption;
	KeyAccelerator* oldKeys = g_pKeyAccelerator;
	Properties files;
	KeyAccelerator keys;
	UserOption option;
	OptionScene()
	{
		std::remove(kOptionPath);
		files.setProperty("FILE_INFO_USEROPTION", kOptionPath);
		g_pFileDef = &files;
		keys.Init(MAX_ACCELERATOR);
		g_pKeyAccelerator = &keys;
		g_pUserOption = &option;
	}
	~OptionScene()
	{
		std::remove(kOptionPath);
		g_pKeyAccelerator = oldKeys;
		g_pUserOption = oldOption;
		g_pFileDef = oldFiles;
	}
	std::string Saved()
	{
		std::ifstream in(kOptionPath, std::ios::binary);
		return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	}
};
} // namespace

TEST(GameUI1UserOption, TheOptionsAreSavedToTheFileTheDefinitionsName)
{
	OptionScene scene;
	scene.option.VolumeMusic = 37;
	scene.option.VolumeSound = 12;
	UI_SaveUserOption();
	const std::string saved = scene.Saved();
	CHECK(saved.find("37\tVolumeMusic\n") != std::string::npos);
	CHECK(saved.find("12\tVolumeSound\n") != std::string::npos);
	// Applying them changes nothing in them (its body is commented out).
	scene.option.DrawMinimap = TRUE;
	scene.option.OpenQuickSlot = FALSE;
	UI_AffectUserOption();
	CHECK_EQ(37, scene.option.VolumeMusic);
	CHECK_EQ(TRUE, scene.option.DrawMinimap);
	CHECK_EQ(FALSE, scene.option.OpenQuickSlot);
}

TEST(GameUI1UserOption, NoOptionsNoFile)
{
	OptionScene scene;
	g_pUserOption = NULL;
	UI_SaveUserOption();
	CHECK(scene.Saved().empty());
	// Nor are the definitions read.
	g_pFileDef = NULL;
	UI_SaveUserOption();
	UI_AffectUserOption();
	CHECK(scene.Saved().empty());
}

//----------------------------------------------------------------------
// UI_BackupLoginID: an id under 20 characters goes to the UI, which keeps
// one under 11 in the user's options (C_VS_UI::BackupPrevId).
//----------------------------------------------------------------------
TEST(GameUI1LoginID, AShortIdIsKeptInTheOptions)
{
	UserOption* oldOption = g_pUserOption;
	UserOption option;
	std::snprintf(option.BackupID, sizeof(option.BackupID), "old");
	g_pUserOption = &option;
	UI_BackupLoginID("kim");
	CHECK(std::string("kim") == option.BackupID);
	UI_BackupLoginID("abcdefghij");
	CHECK(std::string("abcdefghij") == option.BackupID);
	UI_BackupLoginID("abcdefghijk");	// 11: the UI's limit
	UI_BackupLoginID("abcdefghijklmnopqrs");	// 19
	UI_BackupLoginID(NULL);
	CHECK(std::string("abcdefghij") == option.BackupID);
	UI_BackupLoginID("");
	CHECK(std::string() == option.BackupID);
	// Recorded defect: C_VS_UI::BackupPrevId writes through g_pUserOption
	// unchecked, so a 0-10 character id with no options crashes; 20 and
	// more never reach it, nor does NULL.
	g_pUserOption = NULL;
	UI_BackupLoginID("abcdefghijklmnopqrst");
	UI_BackupLoginID(NULL);
	g_pUserOption = oldOption;
}

//----------------------------------------------------------------------
// UI_RemoveDescriptor: the descriptor showing goes when its item does.
//----------------------------------------------------------------------
namespace {
int descriptorShown = 0;
void ShowDescriptor(Rect, void*, long, long) { ++descriptorShown; }
} // namespace

TEST(GameUI1Descriptor, TheDescriptorOfTheRemovedItemGoes)
{
	UiBase ui;	// Show checks the base's back surface
	int item = 0, other = 0;
	descriptorShown = 0;
	g_descriptor_manager.RectCalculationFinished(ShowDescriptor, Rect(0, 0, 4, 4), &item, 0, 0);
	UI_RemoveDescriptor(&other);
	g_descriptor_manager.Show();
	CHECK_EQ(1, descriptorShown);
	UI_RemoveDescriptor(&item);
	g_descriptor_manager.Show();
	CHECK_EQ(1, descriptorShown);
	// NULL, the default, takes any.
	g_descriptor_manager.RectCalculationFinished(ShowDescriptor, Rect(0, 0, 4, 4), &item, 0, 0);
	UI_RemoveDescriptor();
	g_descriptor_manager.Show();
	CHECK_EQ(1, descriptorShown);
	g_descriptor_manager.Unset();
}

//----------------------------------------------------------------------
// The computer: the PC talk box is hidden while it runs, and the
// connection kept.
//----------------------------------------------------------------------
namespace {
DialogWindow::Window* const kTalk = reinterpret_cast<DialogWindow::Window*>(static_cast<std::uintptr_t>(7));
const DialogWindow::Host windowHost = []() {
	DialogWindow::Host host{};
	host.Start = [](DialogWindow::Window* pWindow) { calls.push_back("start " + N(static_cast<long long>(reinterpret_cast<std::uintptr_t>(pWindow)))); };
	host.Finish = [](DialogWindow::Window* pWindow) { calls.push_back("finish " + N(static_cast<long long>(reinterpret_cast<std::uintptr_t>(pWindow)))); };
	host.Delete = [](DialogWindow::Window* pWindow) { calls.push_back("delete " + N(static_cast<long long>(reinterpret_cast<std::uintptr_t>(pWindow)))); };
	return host;
}();
// The client's dialogs with a PC talk box open.
struct TalkDialog : UIDialog
{
	TalkDialog() { m_pPCTalkDlg = kTalk; }
	~TalkDialog() { m_pPCTalkDlg = NULL; }
};
struct ComputerScene
{
	GameModelWorld world;
	UIDialog* oldDialog = g_pUIDialog;
	const DialogWindow::Host* oldWindows = DialogWindow::SetHost(&windowHost);
	TalkDialog dialog;
	ComputerScene() { calls.clear(); g_pUIDialog = &dialog; }
	~ComputerScene()
	{
		g_pUIDialog = oldDialog;
		DialogWindow::SetHost(oldWindows);
	}
};
struct LockRestore
{
	DWORD old = UIDialog::IsLockInput();
	~LockRestore()
	{
		UIDialog::UnSetLockInputPCTalk();
		UIDialog::UnSetLockInputMessage();
		if (old & FLAG_UIDIALOG_LOCK_PCTALK) UIDialog::SetLockInputPCTalk();
		if (old & FLAG_UIDIALOG_LOCK_MESSAGE) UIDialog::SetLockInputMessage();
	}
};
} // namespace

TEST(GameUI1Computer, RunningTheComputerHidesTheTalkAndKeepsTheConnection)
{
	LockRestore lock;
	ComputerScene scene;
	g_pUserInformation->KeepConnection = FALSE;
	UI_RunComputer();
	CHECK(Same({"finish 7"}));
	CHECK_EQ(TRUE, g_pUserInformation->KeepConnection);
	calls.clear();
	UI_CloseComputer();
	CHECK(Same({"start 7"}));
	CHECK_EQ(FALSE, g_pUserInformation->KeepConnection);
	// Without the dialogs, only the connection.
	g_pUIDialog = NULL;
	calls.clear();
	UI_RunComputer();
	CHECK_EQ(TRUE, g_pUserInformation->KeepConnection);
	UI_CloseComputer();
	CHECK_EQ(FALSE, g_pUserInformation->KeepConnection);
	CHECK(calls.empty());
}

TEST(GameUI1Computer, TheBriefingAndTutorialExitDoNothing)
{
	LockRestore lock;
	ComputerScene scene;
	g_pUserInformation->KeepConnection = FALSE;
	UI_RunBriefing();
	UI_CloseBriefing();
	UI_RunTutorialExit();
	UI_CloseTutorialExit();
	CHECK(calls.empty());
	CHECK_EQ(FALSE, g_pUserInformation->KeepConnection);
	CHECK_EQ(TRUE, UI_IsRunningBriefing());
	CHECK_EQ(TRUE, UI_IsRunningTutorialExit());
	g_pUIDialog = NULL;
	UI_RunBriefing();
	UI_CloseBriefing();
	CHECK(calls.empty());
}

//----------------------------------------------------------------------
// The trade
//----------------------------------------------------------------------
namespace {
struct ZoneProbe : MZone
{
	using MZone::MZone;
	using MZone::m_mapCreature;
};
const TYPE_OBJECTID kOther = 500;
struct TradeScene
{
	GameModelWorld world;
	CREATURE_TABLE creatureTypes;
	CREATURESPRITE_TABLE sprites;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	RankBonusTable rankBonuses;
	TempInformation temp;
	MInventory inventory;
	MSlayerGear slayerGear;
	C_VS_UI_ITEM uiItem;
	C_VS_UI_MOUSE_POINTER mouse;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	TempInformation* oldTemp = g_pTempInformation;
	MInventory* oldInventory = g_pInventory;
	MSlayerGear* oldSlayerGear = g_pSlayerGear;
	MTradeManager* oldTrade = g_pTradeManager;
	C_VS_UI_ITEM* oldUiItem = gpC_item;
	C_VS_UI_MOUSE_POINTER* oldMouse = gpC_mouse_pointer;
	MZone* oldZone = g_pZone;
	MPlayer* oldPlayer = g_pPlayer;
	MonotonicClock::TimePoint oldFrameNow = g_FrameNow;
	int oldRefPoints[ITEM_REF_POINT_COUNT][2];
	std::unique_ptr<ZoneProbe> zone;
	std::unique_ptr<MCreature> other;
	MPlayer* me = nullptr;
	MItem* pHeld = nullptr;	// on the mouse
	MItem* pPotion1 = nullptr;
	MItem* pPotion2 = nullptr;
	MItem* pHelm = nullptr;

	TradeScene()
	{
		// The pointer's pick-up sets the item's reference points.
		for (int i = 0; i < ITEM_REF_POINT_COUNT; ++i) { oldRefPoints[i][0] = g_item_ref_point[i].x; oldRefPoints[i][1] = g_item_ref_point[i].y; }
		creatureTypes.Init(1);
		testfw::MutableRow(creatureTypes, CREATURETYPE_SLAYER_MALE).SetCreatureTribe(CREATURETRIBE_SLAYER, 0);
		g_pCreatureTable = &creatureTypes;
		sprites.Init(1);
		g_pCreatureSpriteTable = &sprites;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		g_pRankBonusTable = &rankBonuses;
		Class(ITEM_CLASS_POTION, 1, 1, 0);
		Class(ITEM_CLASS_HELM, 2, 2, FLAG_RACE_SLAYER);
		g_pTempInformation = &temp;
		inventory.Init(10, 6);
		g_pInventory = &inventory;
		slayerGear.Init();
		g_pSlayerGear = &slayerGear;
		g_pTradeManager = NULL;
		gpC_item = &uiItem;
		gpC_mouse_pointer = &mouse;
		zone.reset(new ZoneProbe(TYPE_SECTORPOSITION(20), TYPE_SECTORPOSITION(20)));
		g_pZone = zone.get();
		other.reset(new MCreature);
		other->SetID(kOther);
		other->SetName("Trader");
		zone->m_mapCreature[kOther] = other.get();
		me = new MPlayer;
		g_pPlayer = me;
		me->SetCreatureType(CREATURETYPE_SLAYER_MALE);
	}
	~TradeScene()
	{
		delete g_pTradeManager;
		g_pTradeManager = oldTrade;
		zone->m_mapCreature.clear();
		zone.reset();
		g_pZone = oldZone;
		delete me;
		g_pPlayer = oldPlayer;
		other.reset();
		mouse.DropItem();
		delete pHeld;
		inventory.Release();
		slayerGear.Release();
		gpC_mouse_pointer = oldMouse;
		gpC_item = oldUiItem;
		g_pSlayerGear = oldSlayerGear;
		g_pInventory = oldInventory;
		g_pTempInformation = oldTemp;
		g_pRankBonusTable = oldRankBonuses;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatureTypes;
		g_FrameNow = oldFrameNow;
		for (int i = 0; i < ITEM_REF_POINT_COUNT; ++i) g_item_ref_point[i].Set(oldRefPoints[i][0], oldRefPoints[i][1]);
	}
	void Class(int itemClass, int width, int height, int race)
	{
		g_pItemTable->InitClass(itemClass, 1);
		testfw::MutableRow(*g_pItemTable, itemClass, 0).SetGrid(BYTE(width), BYTE(height));
		testfw::MutableRow(*g_pItemTable, itemClass, 0).Race = BYTE(race);
	}
	static MItem* Marked(MItem* pItem, TYPE_OBJECTID id)
	{
		pItem->SetItemType(0);
		pItem->SetID(id);
		pItem->SetTrade();
		return pItem;
	}
	// Two potions in the inventory, a helm worn and a potion on the mouse,
	// all marked for the trade.
	void Items()
	{
		pPotion1 = Marked(new MPotion, 601);
		pPotion2 = Marked(new MPotion, 602);
		CHECK(inventory.AddItem(pPotion1, 0, 0));
		CHECK(inventory.AddItem(pPotion2, 3, 2));
		pHelm = Marked(new MHelm, 603);
		CHECK(slayerGear.AddItem(pHelm, MSlayerGear::GEAR_SLAYER_HELM));
		pHeld = Marked(new MPotion, 604);
		mouse.PickUpItem(pHeld, 0, 0);
	}
	// A trade manager, as one left from before.
	MTradeManager* OldTrade()
	{
		g_pTradeManager = new MTradeManager;
		g_pTradeManager->Init();
		g_pTradeManager->SetOtherID(5);
		g_pTradeManager->SetOtherName("Before");
		g_pTradeManager->AcceptMyTrade();
		return g_pTradeManager;
	}
};
} // namespace

TEST(GameUI1Trade, ARequestFromSomeoneHereWaitsForTheAnswer)
{
	TradeScene scene;
	scene.temp.Value1 = 3;
	// Nobody of that id: nothing.
	UI_RunExchangeAsk(kOther + 1);
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_NULL), int(scene.me->GetWaitVerify()));
	CHECK_EQ(int(TempInformation::MODE_NULL), int(scene.temp.GetMode()));
	CHECK_EQ(3, int(scene.temp.Value1));
	// Someone at a personal shop: nothing.
	scene.other->SetPersnalShop(1);
	UI_RunExchangeAsk(kOther);
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_NULL), int(scene.me->GetWaitVerify()));
	CHECK_EQ(int(TempInformation::MODE_NULL), int(scene.temp.GetMode()));
	CHECK_EQ(3, int(scene.temp.Value1));
	// Otherwise the player waits on the trade request from him.
	scene.other->SetPersnalShop(0);
	UI_RunExchangeAsk(kOther);
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_TRADE), int(scene.me->GetWaitVerify()));
	CHECK_EQ(int(TempInformation::MODE_TRADE_REQUEST), int(scene.temp.GetMode()));
	CHECK_EQ(int(kOther), int(scene.temp.Value1));
	// Only 1 means a personal shop.
	scene.me->SetWaitVerifyNULL();
	scene.temp.SetMode(TempInformation::MODE_NULL);
	scene.temp.Value1 = 3;
	scene.other->SetPersnalShop(2);
	UI_RunExchangeAsk(kOther);
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_TRADE), int(scene.me->GetWaitVerify()));
	CHECK_EQ(int(kOther), int(scene.temp.Value1));
}

TEST(GameUI1Trade, OpeningTheTradeStartsAFreshOneWithNothingMarked)
{
	TradeScene scene;
	scene.Items();
	scene.OldTrade();
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	UI_RunExchange(kOther);
	// The request's box is closed.
	CHECK_EQ(int(TempInformation::MODE_NULL), int(scene.temp.GetMode()));
	CHECK(g_pTradeManager != NULL);
	CHECK_EQ(int(kOther), int(g_pTradeManager->GetOtherID()));
	CHECK(std::string("Trader") == g_pTradeManager->GetOtherName());
	CHECK(!g_pTradeManager->IsAcceptMyTrade());
	CHECK(g_pTradeManager->GetMyInventory() == &scene.inventory);
	CHECK(g_pTradeManager->GetOtherInventory() != NULL);
	CHECK(!scene.pHeld->IsTrade());
	CHECK(!scene.pPotion1->IsTrade());
	CHECK(!scene.pPotion2->IsTrade());
	CHECK(!scene.pHelm->IsTrade());
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_TRADE), int(scene.me->GetWaitVerify()));
}

TEST(GameUI1Trade, OpeningTheTradeWithNothingHeldOrBefore)
{
	TradeScene scene;
	UI_RunExchange(kOther);
	CHECK(g_pTradeManager != NULL);
	CHECK_EQ(int(kOther), int(g_pTradeManager->GetOtherID()));
	CHECK(g_pTradeManager->GetMyInventory() == &scene.inventory);
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_TRADE), int(scene.me->GetWaitVerify()));
}

TEST(GameUI1Trade, OpeningATradeWithNobodyOnlyClosesTheRequest)
{
	TradeScene scene;
	scene.Items();
	MTradeManager* pOld = scene.OldTrade();
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	UI_RunExchange(kOther + 1);
	CHECK_EQ(int(TempInformation::MODE_NULL), int(scene.temp.GetMode()));
	CHECK(g_pTradeManager == pOld);
	CHECK_EQ(5, int(g_pTradeManager->GetOtherID()));
	CHECK(g_pTradeManager->IsAcceptMyTrade());
	CHECK(scene.pHeld->IsTrade());
	CHECK(scene.pPotion1->IsTrade());
	CHECK(scene.pHelm->IsTrade());
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_NULL), int(scene.me->GetWaitVerify()));
}

TEST(GameUI1Trade, ClosingTheTradeEndsItAndDelaysDroppingItems)
{
	TradeScene scene;
	g_FrameNow = kOrigin;
	g_pClientConfig->AFTER_TRADE_ITEM_DROP_DELAY = 1234;
	g_pUserInformation->ItemDropEnableTime = MonotonicClock::TimePoint();
	scene.OldTrade();
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	UI_CloseExchange();
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_NULL), int(scene.me->GetWaitVerify()));
	CHECK_EQ(int(TempInformation::MODE_NULL), int(scene.temp.GetMode()));
	CHECK(g_pTradeManager == NULL);
	CHECK_EQ(1234LL, MsAfter(kOrigin, g_pUserInformation->ItemDropEnableTime));
	// No trade: no delay. Another wait stays; any mode ends.
	g_FrameNow = kOrigin + MonotonicClock::Millis(10000);
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_PARTY);
	scene.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
	UI_CloseExchange();
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_PARTY), int(scene.me->GetWaitVerify()));
	CHECK_EQ(int(TempInformation::MODE_NULL), int(scene.temp.GetMode()));
	CHECK_EQ(1234LL, MsAfter(kOrigin, g_pUserInformation->ItemDropEnableTime));
}

TEST(GameUI1Trade, ClosingTheTradeNeedsThePlayerConfigTempAndUser)
{
	TradeScene scene;
	g_FrameNow = kOrigin;
	g_pUserInformation->ItemDropEnableTime = MonotonicClock::TimePoint();
	MTradeManager* pTrade = scene.OldTrade();
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	// Each missing in turn: nothing at all.
	auto unchanged = [&]() {
		return g_pTradeManager == pTrade
			&& scene.me->GetWaitVerify() == MPlayer::WAIT_VERIFY_TRADE
			&& scene.temp.GetMode() == TempInformation::MODE_TRADE_REQUEST
			&& g_pUserInformation->ItemDropEnableTime == MonotonicClock::TimePoint();
	};
	g_pPlayer = NULL;
	UI_CloseExchange();
	g_pPlayer = scene.me;
	CHECK(unchanged());
	ClientConfig* pConfig = g_pClientConfig;
	g_pClientConfig = NULL;
	UI_CloseExchange();
	g_pClientConfig = pConfig;
	CHECK(unchanged());
	g_pTempInformation = NULL;
	UI_CloseExchange();
	g_pTempInformation = &scene.temp;
	CHECK(unchanged());
	UserInformation* pUser = g_pUserInformation;
	g_pUserInformation = NULL;
	UI_CloseExchange();
	g_pUserInformation = pUser;
	CHECK(unchanged());
}

TEST(GameUI1Trade, ClosingTheAskEndsTheTradeAndItsRequest)
{
	TradeScene scene;
	g_pUserInformation->ItemDropEnableTime = MonotonicClock::TimePoint();
	scene.OldTrade();
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	UI_CloseExchangeAsk();
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_NULL), int(scene.me->GetWaitVerify()));
	CHECK_EQ(int(TempInformation::MODE_NULL), int(scene.temp.GetMode()));
	CHECK(g_pTradeManager == NULL);
	// No drop delay.
	CHECK(g_pUserInformation->ItemDropEnableTime == MonotonicClock::TimePoint());
	// Another wait and another request stay.
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_PARTY);
	scene.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
	UI_CloseExchangeAsk();
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_PARTY), int(scene.me->GetWaitVerify()));
	CHECK_EQ(int(TempInformation::MODE_PARTY_REQUEST), int(scene.temp.GetMode()));
	CHECK(g_pTradeManager == NULL);
}

TEST(GameUI1Trade, ClosingTheCancelBoxEndsOnlyATradeRequest)
{
	TradeScene scene;
	scene.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
	UI_CloseExchangeCancel();
	CHECK_EQ(int(TempInformation::MODE_PARTY_REQUEST), int(scene.temp.GetMode()));
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	UI_CloseExchangeCancel();
	CHECK_EQ(int(TempInformation::MODE_NULL), int(scene.temp.GetMode()));
	g_pTempInformation = NULL;
	UI_CloseExchangeCancel();
	g_pTempInformation = &scene.temp;
}

TEST(GameUI1Trade, TheMouseItemIsThePointers)
{
	TradeScene scene;
	CHECK(UI_GetMouseItem() == NULL);
	scene.Items();
	CHECK(UI_GetMouseItem() == scene.pHeld);
	// Levelling up shows only in the game UI (none here): harmless.
	UI_LevelUp();
}

//----------------------------------------------------------------------
// UI_CloseStorage: the storage stops, and the skills the player can use
// are checked again (in zone 3001 that only empties the set).
//----------------------------------------------------------------------
namespace {
MStatus* pPlayerStatus = nullptr;
const PlayerState::Host statusHost = []() {
	PlayerState::Host host{};
	host.Status = []() -> MStatus* { return pPlayerStatus; };
	return host;
}();
const ZoneState::Host skillFreeZone = []() {
	ZoneState::Host host{};
	host.GetID = []() { return 3001; };
	return host;
}();
} // namespace

TEST(GameUI1Storage, ClosingTheStorageStopsItAndChecksTheSkills)
{
	MStorage* oldStorage = g_pStorage;
	MSkillSet* oldSkills = g_pSkillAvailable;
	MSkillManager* oldSkillManager = g_pSkillManager;
	MSkillInfoTable* oldSkillInfo = g_pSkillInfoTable;
	const PlayerState::Host* oldPlayerHost = PlayerState::SetHost(&statusHost);
	const ZoneState::Host* oldZoneHost = ZoneState::SetHost(&skillFreeZone);
	MStatus status;
	pPlayerStatus = &status;
	MStorage storage;
	MSkillSet skills;
	MSkillManager skillManager;
	MSkillInfoTable skillInfo;
	skillManager.CTypeTable<MSkillDomain>::Init(1);
	skillInfo.CTypeTable<SKILLINFO_NODE>::Init(1);
	g_pSkillManager = &skillManager;
	g_pSkillInfoTable = &skillInfo;

	storage.SetActive();
	skills.AddSkill(ACTIONINFO(5));
	g_pStorage = &storage;
	g_pSkillAvailable = &skills;
	UI_CloseStorage();
	CHECK(!storage.IsActive());
	CHECK(skills.empty());
	// Neither there: nothing to do.
	g_pStorage = NULL;
	g_pSkillAvailable = NULL;
	UI_CloseStorage();
	// Each alone.
	storage.SetActive();
	g_pStorage = &storage;
	UI_CloseStorage();
	CHECK(!storage.IsActive());
	g_pStorage = NULL;
	skills.AddSkill(ACTIONINFO(5));
	g_pSkillAvailable = &skills;
	UI_CloseStorage();
	CHECK(skills.empty());

	ZoneState::SetHost(oldZoneHost);
	PlayerState::SetHost(oldPlayerHost);
	g_pSkillInfoTable = oldSkillInfo;
	g_pSkillManager = oldSkillManager;
	g_pSkillAvailable = oldSkills;
	g_pStorage = oldStorage;
}

//----------------------------------------------------------------------
// UI_AlreadyExistIDMessage, UI_NoAlreadyExistIDMessage: each starts its
// own message box (made by g_InitMessage in the game).
//----------------------------------------------------------------------
TEST(GameUI1IDMessage, EachStartsItsOwnBox)
{
	Windows windows;
	UserOption* oldOption = g_pUserOption;
	C_GLOBAL_RESOURCE* oldResource = gpC_global_resource;
	C_VS_UI_DIALOG* oldExists = g_msg_id_aleady_exist;
	C_VS_UI_DIALOG* oldFree = g_msg_id_no_aleady_exist;
	UserOption option;
	g_pUserOption = &option;
	{
		C_GLOBAL_RESOURCE resource;
		gpC_global_resource = &resource;
		C_VS_UI_DIALOG exists(-1, -1, 1, 0, NULL, DIALOG_OK);
		C_VS_UI_DIALOG usable(-1, -1, 2, 0, NULL, DIALOG_OK);
		g_msg_id_aleady_exist = &exists;
		g_msg_id_no_aleady_exist = &usable;
		UI_AlreadyExistIDMessage();
		CHECK(exists.Running());
		CHECK(!usable.Running());
		exists.Finish();
		UI_NoAlreadyExistIDMessage();
		CHECK(!exists.Running());
		CHECK(usable.Running());
		usable.Finish();
	}
	g_msg_id_no_aleady_exist = oldFree;
	g_msg_id_aleady_exist = oldExists;
	gpC_global_resource = oldResource;
	g_pUserOption = oldOption;
}

//----------------------------------------------------------------------
// Without a UI. Not run: UI_CloseAllDialog, whose C_VS_UI::CloseAllDialog
// rebuilds every message box (g_FreeMessage, g_InitMessage: the global
// resource, the text service, and function statics that keep the first
// game string table's pointers for the process).
//----------------------------------------------------------------------
TEST(GameUI1WithoutUI, TheForwardsAreHarmlessAndAnswerNoUI)
{
	SlotScope slot;
	CHECK(!UI_IsRunningSelectWayPoint());
	UI_RunSelectWayPoint();
	UI_CloseSelectWayPoint();
	UI_SetCurrentServerName("server", 1);
	UI_SetCurrentServerName(NULL, -1);
	CHECK_EQ(FALSE, UI_IsRunningSelectExplosive());
	UI_RunSelectExplosive();
	UI_RunSelectBomb();
	UI_RunSelectMine();
	UI_CloseSelectExplosive();
	CHECK_EQ(FALSE, UI_IsRunningExchange());
	CHECK_EQ(FALSE, UI_IsRunningStorage());
	CHECK_EQ(FALSE, UI_IsRunningShop());
	CHECK_EQ(FALSE, UI_IsRunningBookcase());
	CHECK_EQ(FALSE, UI_IsRunningRepair());
	CHECK_EQ(FALSE, UI_IsRunningSilvering());
	CHECK_EQ(FALSE, UI_IsRunningSell());
	CHECK_EQ(FALSE, UI_IsRunningComputer());
	// The briefing and the tutorial's exit are stubs that answer TRUE.
	CHECK_EQ(TRUE, UI_IsRunningTutorialExit());
	CHECK_EQ(TRUE, UI_IsRunningBriefing());
	UI_RunExchangeCancel("Trader");
	UI_RunBookcase();
	UI_CloseBookcase();
	UI_CloseShop();
	UI_SaveHotKeyToServer();
	UI_SetHotKey(1, 2);
	CHECK_EQ(-1, UI_GetHotKey(0));
	CHECK_EQ(-1, UI_GetHotKey(3));
	UI_CloseUserRegistrationWindow();
	const POINT grid = UI_GetInventoryGridPosition(3, 4);
	CHECK_EQ(0, int(grid.x));
	CHECK_EQ(0, int(grid.y));
	const POINT at = UI_GetInventoryPosition();
	CHECK_EQ(0, int(at.x));
	CHECK_EQ(0, int(at.y));
	UI_LockItemTrade();
	UI_UnlockItemTrade();
	// Recorded defect: each copies the name into a new[] buffer the
	// dialog would own; without the game UI it leaks (GameUI.cpp:164,
	// 178, 185, 197, 203).
	GCFriendChatting packet;
	packet.setPlayerName("Ann");
	UI_RunFriendRequest(&packet);
	UI_RunFriendRefuse(&packet);
	UI_RunFriendWait(&packet);
	UI_RunFriendExist(&packet);
	UI_RunFriendBlack(&packet);
	// None of them touched the character sheet.
	CHECK(g_char_slot_ingame.sz_name.empty());
	CHECK(g_char_slot_ingame.STATUS.empty());
}
