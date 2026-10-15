#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "UIFunction.h"
#include "MPlayer.h"
#include "MZone.h"
#include "MParty.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "EffectSpriteTypeDef.h"
#include "MActionInfoTable.h"
#include "RankBonusTable.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "MSkillManager.h"
#include "SkillDef.h"
#include "TalkBox.h"
#include "UIDialog.h"
#include "DialogWindowHost.h"
#include "TempInformation.h"
#include "SystemAvailabilities.h"
#include "MStorage.h"
#include "MStatusManager.h"
#include "HelpEvent.h"
#include "ClientMode.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "CServerInformation.h"
#include "UserOption.h"
#include "UiRuntime.h"
#include "MEventQueue.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackendSDL.h"
#include "DXLib/CDirectDraw.h"
#include "VS_UI.h"
#include "VS_UI_Base.h"
#include "VS_UI_mouse_pointer.h"
#include "VS_UI_item.h"
#include "PacketFunction.h"
#include "CrtCompat.h"
#include "GuildInfo.h"
#include "GuildMemberInfo.h"
#include "Packet/PCSlayerInfo.h"
#include "Packet/PCVampireInfo.h"
#include "Packet/PCOustersInfo.h"
#include "Gpackets/GCWaitGuildList.h"
#include "Gpackets/GCActiveGuildList.h"
#include "Gpackets/GCGuildMemberList.h"
#include "Gpackets/GCShowWaitGuildInfo.h"
#include "Gpackets/GCShowGuildInfo.h"
#include "Gpackets/GCShowGuildMemberInfo.h"
#include "Gpackets/GCShowUnionInfo.h"

#include <climits>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

// GameUI.cpp's, not declared in UIFunction.h.
extern BOOL g_bDrawProgress;
extern bool UI_SetWaitGuildList();
// UIDialog.cpp's.
extern bool g_bPartyRunning;

// The game's calls into the UI from the skill-learning talk to the guild
// windows (GameUI.cpp's second part, gameclient's since task 4.256). The
// UI itself is never started here, so gC_vs_ui has no title, game or
// progress window and every call into it that checks for them does
// nothing: what is pinned is what each call does to the game's own state
// (the NPC talk box and its window, the player's wait, the storage, the
// temporary request register, the help mails, the sounds, the mouse's
// item, the back buffer) and to the hosts it calls.
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

// The NPC talk windows, numbered in the order they open.
int windowCount = 0;
std::string W(DialogWindow::Window* pWindow) { return N(static_cast<long long>(reinterpret_cast<std::uintptr_t>(pWindow))); }
const DialogWindow::Host windowHost = []() {
	DialogWindow::Host host{};
	host.NewTalk = [](int creatureType, const char* name) {
		DialogWindow::Window* pWindow = reinterpret_cast<DialogWindow::Window*>(static_cast<std::uintptr_t>(++windowCount));
		calls.push_back("talk " + W(pWindow) + "|" + N(creatureType) + "|" + (name != NULL ? name : "(null)"));
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

// The skills of the tests' domains.
enum
{
	kRoot = 100,		// the tree's root, learned (learnable at level 1)
	kLearnZero = 101,	// learnable at level 0: never offered
	kFirst = 102,		// at level 1
	kAtDomain = 103,	// at the domain's level, 30
	kPastDomain = 104,	// one past it
	kOther = 105,		// not learnable yet (OTHER) and at level 20
	kTop = 107,			// at 149, the last level offered
	kBeyond = 108,		// at 150, never offered
};

// Everything the calls read, saved and given back after: the game's
// tables and settings, the player, the NPC talk box and its window, the
// request register, the storage and the system availabilities.
struct Scene
{
	GameModelWorld world;
	CREATURE_TABLE creatures;
	CREATURESPRITE_TABLE sprites;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	MActionInfoTable actions;
	RankBonusTable rankBonuses;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	MSkillInfoTable skillInfo;
	MSkillManager skillManager;
	SystemAvailabilitiesManager availabilities;
	PCTalkBox talkBox;
	UIDialog dialog;
	TempInformation temp;
	MStorage storage;
	CREATURE_TABLE* oldCreatures = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	MSkillInfoTable* oldSkillInfo = g_pSkillInfoTable;
	MSkillManager* oldSkillManager = g_pSkillManager;
	MSkillSet* oldSkillSet = g_pSkillAvailable;
	SystemAvailabilitiesManager* oldAvailabilities = g_pSystemAvailableManager;
	PCTalkBox* oldTalkBox = g_pPCTalkBox;
	UIDialog* oldDialog = g_pUIDialog;
	TempInformation* oldTemp = g_pTempInformation;
	MStorage* oldStorage = g_pStorage;
	MPlayer* oldPlayer = g_pPlayer;
	CLIENT_MODE oldMode = g_Mode;
	int oldActive = g_bActiveGame;
	bool oldPartyRunning = g_bPartyRunning;
	DWORD oldLock = UIDialog::IsLockInput();
	const DialogWindow::Host* oldWindows = DialogWindow::SetHost(&windowHost);
	MPlayer* me = nullptr;

	Scene()
	{
		calls.clear();
		windowCount = 0;
		creatures.Init(10);
		testfw::MutableRow(creatures, 0).SetCreatureTribe(CREATURETRIBE_SLAYER, 0);
		testfw::MutableRow(creatures, 7).Name = "Trainer";
		g_pCreatureTable = &creatures;
		sprites.Init(1);
		g_pCreatureSpriteTable = &sprites;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		actions.Init(MAX_ACTIONINFO);
		g_pActionInfoTable = &actions;
		g_pRankBonusTable = &rankBonuses;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		skillInfo.CTypeTable<SKILLINFO_NODE>::Init(MAX_ACTIONINFO);
		g_pSkillInfoTable = &skillInfo;
		skillManager.CTypeTable<MSkillDomain>::Init(MAX_SKILLDOMAIN);
		g_pSkillManager = &skillManager;
		g_pSkillAvailable = NULL;
		g_pSystemAvailableManager = &availabilities;
		g_pPCTalkBox = &talkBox;
		dialog.Init();
		g_pUIDialog = &dialog;
		g_pTempInformation = &temp;
		g_pStorage = &storage;
		g_Mode = MODE_GAME;
		g_bActiveGame = 1;
		g_bPartyRunning = false;
		UIDialog::UnSetLockInputPCTalk();
		UIDialog::UnSetLockInputMessage();
		g_pGameStringTable->Init(MAX_GAME_STRING + 1);
		g_pGameStringTable->Set(STRING_MESSAGE_WHAT_SKILL_TO_LEARN, "Learn which %s skill?");
		g_pGameStringTable->Set(STRING_MESSAGE_NO_SKILL_TO_LEARN, "Nothing to learn.");
		g_pGameStringTable->Set(UI_STRING_MESSAGE_CANCEL_LEARN_SKILL, "Later.");
		g_pGameStringTable->Set(UI_STRING_MESSAGE_HAN_DOMAIN_BLADE, "Blade");
		g_pGameStringTable->Set(UI_STRING_MESSAGE_HAN_DOMAIN_VAMPIRE, "Night");
		g_pGameStringTable->Set(UI_STRING_MESSAGE_NOT_LOGINED, "(Offline)");
		me = new MPlayer;
		g_pPlayer = me;
		me->SetCreatureType(0);	// a slayer
		talkBox.SetCreatureType(7);
	}
	~Scene()
	{
		delete me;	// clears g_pPlayer
		g_pPlayer = oldPlayer;
		dialog.Release();	// frees its window and the input lock
		if (oldLock & FLAG_UIDIALOG_LOCK_PCTALK) UIDialog::SetLockInputPCTalk();
		if (oldLock & FLAG_UIDIALOG_LOCK_MESSAGE) UIDialog::SetLockInputMessage();
		DialogWindow::SetHost(oldWindows);
		g_bPartyRunning = oldPartyRunning;
		g_bActiveGame = oldActive;
		g_Mode = oldMode;
		g_pStorage = oldStorage;
		g_pTempInformation = oldTemp;
		g_pUIDialog = oldDialog;
		g_pPCTalkBox = oldTalkBox;
		g_pSystemAvailableManager = oldAvailabilities;
		g_pSkillAvailable = oldSkillSet;
		g_pSkillManager = oldSkillManager;
		g_pSkillInfoTable = oldSkillInfo;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pRankBonusTable = oldRankBonuses;
		g_pActionInfoTable = oldActions;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatures;
		talkBox.Release();
	}

	// The player a vampire (the creature type it has is one).
	void Vampire() { testfw::MutableRow(creatures, me->GetCreatureType()).SetCreatureTribe(CREATURETRIBE_VAMPIRE, 0); }
	void Level(int level) { me->MStatus::SetStatus(MODIFY_LEVEL, level); }

	void Skill(int id, int level, int learnLevel, const char* name)
	{
		SKILLINFO_NODE& row = testfw::MutableRow(skillInfo, id);
		row.Set(level, name, 0, 0, 0, (std::string("H") + name).c_str());
		row.SetLearnLevel(learnLevel);
	}
	// The domain: kRoot learned (at learning level 1), under it kFirst and
	// the skills each test asks for (learnable next), and kOther under
	// kFirst (not yet).
	MSkillDomain& Domain(int domain, int domainLevel, std::initializer_list<int> offered)
	{
		Skill(kRoot, 0, 1, "root");
		for (int id : offered) testfw::MutableRow(skillInfo, kRoot).AddNextSkill(ACTIONINFO(id));
		testfw::MutableRow(skillInfo, kRoot).AddNextSkill(ACTIONINFO(kFirst));
		testfw::MutableRow(skillInfo, kFirst).AddNextSkill(ACTIONINFO(kOther));
		MSkillDomain& d = testfw::MutableRow(skillManager, domain);
		d.SetRootSkill(ACTIONINFO(kRoot));
		d.SetNewSkill();
		CHECK(d.LearnSkill(ACTIONINFO(kRoot)));
		d.SetDomainLevel(domainLevel);
		return d;
	}
	void StandardSkills()
	{
		Skill(kLearnZero, 1, 0, "zero");
		Skill(kFirst, 1, 1, "first");
		Skill(kAtDomain, 1, 30, "domain");
		Skill(kPastDomain, 1, 31, "past");
		Skill(kOther, 2, 20, "other");
		Skill(kTop, 1, 149, "top");
		Skill(kBeyond, 1, 150, "beyond");
	}

	// What an earlier talk left.
	void Stale()
	{
		talkBox.Release();
		talkBox.SetType(PCTalkBox::SELECT_QUEST);
		talkBox.AddString("stale");
		talkBox.SetContent("Earlier");
	}
	std::vector<std::string> Lines() const
	{
		std::vector<std::string> lines;
		for (const MString* pString : talkBox) lines.push_back(pString->GetString() != nullptr ? pString->GetString() : "<null>");
		return lines;
	}
	// The NPC's talk window open (window 1), the calls forgotten.
	void TalkOpen()
	{
		talkBox.Release();
		talkBox.SetType(PCTalkBox::NORMAL);
		talkBox.AddString("Buy");
		talkBox.SetContent("Welcome");
		dialog.PopupPCTalkDlg();
		CHECK(Same({"talk 1|7|Trainer", "menu 1|1:Buy", "text 1 fit|Welcome", "start 1"}));
		calls.clear();
	}
};

const std::string kLater = "999:Later.";

// The zone's sectors hold its creatures as objects, as GameInit's entries
// convert them.
const CreatureState::Host creatureHost = []() {
	CreatureState::Host host{};
	host.ObjectOf = [](const MCreature* pCreature) -> MObject* { return const_cast<MCreature*>(pCreature); };
	host.CreatureOf = [](MObject* pObject) { return static_cast<MCreature*>(pObject); };
	return host;
}();

// A zone of 40x30 tiles, no party.
struct ZoneScene : Scene
{
	MZone* oldZone = g_pZone;
	MParty* oldParty = g_pParty;
	const CreatureState::Host* oldCreatureHost = CreatureState::SetHost(&creatureHost);
	std::unique_ptr<MZone> zone;
	ZoneScene()
	{
		g_pParty = NULL;
		zone.reset(new MZone(40, 30));
		g_pZone = zone.get();
	}
	~ZoneScene()
	{
		zone.reset();
		g_pZone = oldZone;
		g_pParty = oldParty;
		CreatureState::SetHost(oldCreatureHost);
	}
	// A creature of the zone, which owns it.
	MCreature* Creature(TYPE_OBJECTID id, const char* name, int shop = 0)
	{
		MCreature* pCreature = new MCreature;
		pCreature->SetID(id);
		pCreature->SetName(name);
		pCreature->SetPosition(10, 12);
		pCreature->SetServerPosition(10, 12);
		pCreature->SetPersnalShop(shop);
		CHECK(zone->AddCreature(pCreature));
		return pCreature;
	}
};

// What a call prints on stderr. A crash inside it leaves its report in
// the capture file (game_ui_2_stderr_*.txt in the temporary directory).
template <class Call>
std::string Stderr(Call call)
{
	const std::filesystem::path path = std::filesystem::temp_directory_path() / ("game_ui_2_stderr_" + N(static_cast<long long>(reinterpret_cast<std::uintptr_t>(&call))) + ".txt");
	std::FILE* capture = Basic::OpenFile(path.string().c_str(), "w+");
	if (capture == NULL) return "<no capture>";
	std::fflush(stderr);
#ifdef _WIN32
	const int saved = _dup(_fileno(stderr));
	_dup2(_fileno(capture), _fileno(stderr));
	call();
	std::fflush(stderr);
	_dup2(saved, _fileno(stderr));
	_close(saved);
#else
	const int saved = dup(fileno(stderr));
	dup2(fileno(capture), fileno(stderr));
	call();
	std::fflush(stderr);
	dup2(saved, fileno(stderr));
	close(saved);
#endif
	std::string text;
	std::rewind(capture);
	char buffer[512];
	size_t count;
	while ((count = std::fread(buffer, 1, sizeof buffer, capture)) > 0) text.append(buffer, count);
	std::fclose(capture);
	std::error_code ignored;
	std::filesystem::remove(path, ignored);
	return text;
}

bool Has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

} // namespace

//----------------------------------------------------------------------
// The skill-learning talk
//----------------------------------------------------------------------
TEST(GameUI2, TheSkillTreeOffersTheSlayerDomainsSkillsUpToItsLevel)
{
	// A slayer's learnable skills are the domain's not-yet-learned ones
	// (learnable next or later) whose learning level is above 0, below
	// 150 and at most the domain's level, each as "<id><name> (<english
	// name>)", where the talk shows "<name> (<english name>)". The
	// talk box is cleared to a skill-learning one with a "999" line to
	// leave by, the wait on the NPC ends, and the talk pops up with the
	// NPC it had.
	Scene scene;
	scene.StandardSkills();
	const MSkillDomain& domain = scene.Domain(SKILLDOMAIN_BLADE, 30, {kLearnZero, kAtDomain, kPastDomain, kTop, kBeyond});
	CHECK(MSkillDomain::SKILLSTATUS_LEARNED == domain.GetSkillStatus(ACTIONINFO(kRoot)));
	CHECK(MSkillDomain::SKILLSTATUS_NEXT == domain.GetSkillStatus(ACTIONINFO(kFirst)));
	CHECK(MSkillDomain::SKILLSTATUS_NEXT == domain.GetSkillStatus(ACTIONINFO(kAtDomain)));
	CHECK(MSkillDomain::SKILLSTATUS_OTHER == domain.GetSkillStatus(ACTIONINFO(kOther)));
	scene.Level(1);	// a slayer's own level is not read
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_NPC_ASK);
	scene.Stale();

	UI_RunSkillTree(SKILLDOMAIN_BLADE, 99);

	CHECK(Same({"talk 1|7|Trainer", "menu 1|102:Hfirst (first)|103:Hdomain (domain)|105:Hother (other)|" + kLater,
		"text 1 fit|Learn which Blade skill?", "start 1"}));
	CHECK(scene.me->IsWaitVerifyNULL());
	CHECK(PCTalkBox::SKILL_LEARN == scene.talkBox.GetType());
	CHECK(std::string("Learn which Blade skill?") == scene.talkBox.GetContent());
	CHECK_EQ(7u, scene.talkBox.GetCreatureType());
	CHECK_EQ(int(FLAG_UIDIALOG_LOCK_PCTALK), int(UIDialog::IsLockInput()));
}

TEST(GameUI2, TheSkillTreeOffersUpToLevel149)
{
	// A domain past 149 still offers nothing at 150 or above.
	Scene scene;
	scene.StandardSkills();
	scene.Domain(SKILLDOMAIN_BLADE, 200, {kTop, kBeyond});
	scene.Stale();

	UI_RunSkillTree(SKILLDOMAIN_BLADE, 0);

	CHECK(Same({"talk 1|7|Trainer", "menu 1|102:Hfirst (first)|105:Hother (other)|107:Htop (top)|" + kLater,
		"text 1 fit|Learn which Blade skill?", "start 1"}));
}

TEST(GameUI2, TheSkillTreeLeavesOutSkillsPastTheSystemsLimitButCountsThem)
{
	// The system's learning limit keeps a skill above it off the list,
	// but the skill still counts as one to learn: the talk asks which.
	Scene scene;
	scene.StandardSkills();
	scene.Domain(SKILLDOMAIN_BLADE, 30, {kAtDomain});
	scene.availabilities.SetLimitSkillLevel(20);
	scene.Stale();

	UI_RunSkillTree(SKILLDOMAIN_BLADE, 0);

	CHECK(Same({"talk 1|7|Trainer", "menu 1|102:Hfirst (first)|105:Hother (other)|" + kLater,
		"text 1 fit|Learn which Blade skill?", "start 1"}));

	// None within the limit: only the way out, still asking which.
	calls.clear();
	scene.availabilities.SetLimitSkillLevel(0);
	UI_RunSkillTree(SKILLDOMAIN_BLADE, 0);
	CHECK(Same({"delete 1", "talk 2|7|Trainer", "menu 2|" + kLater, "text 2 fit|Learn which Blade skill?", "start 2"}));
	CHECK((std::vector<std::string>{"Later."}) == scene.Lines());
}

TEST(GameUI2, NothingToLearnSaysSoAndOffersOnlyTheWayOut)
{
	Scene scene;
	scene.StandardSkills();
	scene.Domain(SKILLDOMAIN_BLADE, 0, {kAtDomain});
	scene.Stale();

	UI_RunSkillTree(SKILLDOMAIN_BLADE, 0);

	CHECK(Same({"talk 1|7|Trainer", "menu 1|" + kLater, "text 1 fit|Nothing to learn.", "start 1"}));
	CHECK(std::string("Nothing to learn.") == scene.talkBox.GetContent());
}

TEST(GameUI2, TheSkillTreeOfAVampireOrOustersReadsThePlayersLevel)
{
	// Not a slayer: the player's level, not the domain's, limits the list.
	Scene scene;
	scene.StandardSkills();
	scene.Domain(SKILLDOMAIN_VAMPIRE, 1, {kAtDomain, kPastDomain});
	scene.Vampire();
	CHECK(!scene.me->IsSlayer());
	scene.Level(30);
	scene.Stale();

	UI_RunSkillTree(SKILLDOMAIN_VAMPIRE, 0);

	CHECK(Same({"talk 1|7|Trainer", "menu 1|102:Hfirst (first)|103:Hdomain (domain)|105:Hother (other)|" + kLater,
		"text 1 fit|Learn which Night skill?", "start 1"}));
}

TEST(GameUI2, AnUnknownDomainOnlyResetsTheTalkBox)
{
	// Past the domains (or below them) the wait ends and the talk box is
	// emptied and made a skill-learning one, keeping its old subject; no
	// talk opens.
	for (int domain : {int(MAX_SKILLDOMAIN), -1})
	{
		Scene scene;
		scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_NPC_ASK);
		scene.Stale();

		UI_RunSkillTree(domain, 0);

		CHECK(Same({}));
		CHECK(scene.me->IsWaitVerifyNULL());
		CHECK_EQ(0, int(scene.talkBox.size()));
		CHECK(PCTalkBox::SKILL_LEARN == scene.talkBox.GetType());
		CHECK(std::string("Earlier") == scene.talkBox.GetContent());
		CHECK_EQ(0, int(UIDialog::IsLockInput()));
	}
}

//----------------------------------------------------------------------
// The NPC's windows: selling, repairing, the shop and the storage
//----------------------------------------------------------------------
TEST(GameUI2, TheShopAndStorageWindowsHideTheNPCsTalk)
{
	// Selling, repairing, silvering, buying a storage box and registering
	// a guild each hide the open talk window (it stays, to come back).
	void (*const hiders[])() = {
		UI_OpenInventoryToSell, UI_OpenInventoryToRepair, UI_OpenInventoryToSilvering,
		[]() { UI_RunStorageBuy(5000); }, []() { UI_ShowGuildRegist(300); },
	};
	for (void (*hide)() : hiders)
	{
		Scene scene;
		scene.TalkOpen();
		hide();
		CHECK(Same({"finish 1"}));
		CHECK_EQ(int(FLAG_UIDIALOG_LOCK_PCTALK), int(UIDialog::IsLockInput()));
		CHECK(!scene.storage.IsActive());
	}

	// No talk open: nothing.
	Scene scene;
	for (void (*hide)() : hiders) hide();
	CHECK(Same({}));
}

TEST(GameUI2, TheShopHidesTheTalkAndLocksTheGamesInput)
{
	{
		Scene scene;
		scene.TalkOpen();
		UIDialog::UnSetLockInputPCTalk();
		UI_RunShop();
		CHECK(Same({"finish 1"}));
		CHECK_EQ(int(FLAG_UIDIALOG_LOCK_PCTALK), int(UIDialog::IsLockInput()));
	}

	// Even with no talk to hide.
	Scene scene;
	UI_RunShop();
	CHECK(Same({}));
	CHECK_EQ(int(FLAG_UIDIALOG_LOCK_PCTALK), int(UIDialog::IsLockInput()));
}

TEST(GameUI2, TheStorageHidesTheTalkAndIsInUseThePetStorageOnlyInUse)
{
	Scene scene;
	scene.TalkOpen();
	UI_RunStorage();
	CHECK(Same({"finish 1"}));
	CHECK(scene.storage.IsActive());

	scene.storage.UnSetActive();
	calls.clear();
	UI_RunPetStorage();
	CHECK(Same({}));
	CHECK(scene.storage.IsActive());
}

//----------------------------------------------------------------------
// The calls that only reach the UI
//----------------------------------------------------------------------
TEST(GameUI2, TheUIOnlyCallsAreHarmlessWithoutTheUI)
{
	// With no game, title or progress window these reach nothing; the
	// talk window, the input lock, the storage, the player's wait and the
	// request register are as they were, and the questions answer no.
	// Three are not run: UI_DeleteCharacterFailed starts the UI's
	// g_msg_invalid_ssn box, NULL until the UI is set up; UI_StartGame
	// and UI_StartProgress build the game and loading windows, which load
	// the UI's sprite packs.
	Scene scene;
	scene.TalkOpen();
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_PARTY);
	scene.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
	scene.temp.PartyInviter = 44;
	g_pUserInformation->Slot = 2;

	GCShowGuildInfo info;
	info.setGuildID(9);
	info.setGuildName("Team");
	info.setGuildMaster("Kim");
	info.setGuildIntro("Hello");
	info.setGuildMemberCount(3);
	info.setJoinFee(100);
	GCShowGuildMemberInfo member;
	member.setGuildID(9);
	member.setName("Lee");
	member.setGuildMemberRank(2);
	member.setGuildMemberIntro("Hi");

	UI_CloseRepair();
	UI_CloseSilvering();
	UI_CloseSell();
	UI_SetShop(NULL);
	UI_SetStorage(&scene.storage);
	UI_SetPetStorage(&scene.storage);
	UI_StartCharacterManager();
	UI_NewCharacterCreateOk();
	UI_NewCharacterCreateFailed(3);
	UI_DeleteCharacterOK();
	UI_SetNPCInfo("Guard", 7, 10, 20);
	UI_LockItem();
	UI_UnlockItem();
	UI_LockGear();
	UI_UnlockGear();
	UI_RunParty();
	UI_RunPartyCancel("Kim");
	UI_ClosePartyRequest();
	UI_CloseParty();
	UI_ShowGuildStartJoin(100);
	UI_ShowGuildJoin(100, 2, "2026-10-15", "Team", 9);
	UI_ShowGuildQuit();
	UI_ShowGuildInfo(&info);
	UI_ShowGuildMemberInfo(&member);
	CHECK(!UI_IsRunningParty());
	CHECK(!UI_SetWaitGuildList());

	CHECK(Same({}));
	CHECK_EQ(int(FLAG_UIDIALOG_LOCK_PCTALK), int(UIDialog::IsLockInput()));
	CHECK(!scene.storage.IsActive());
	CHECK(MPlayer::WAIT_VERIFY_PARTY == scene.me->GetWaitVerify());
	CHECK(TempInformation::MODE_PARTY_REQUEST == scene.temp.GetMode());
	CHECK_EQ(44, scene.temp.PartyInviter);
	CHECK_EQ(2, g_pUserInformation->Slot);
	CHECK(std::string("Team") == info.getGuildName());
	CHECK(std::string("Hi") == member.getGuildMemberIntro());
}

//----------------------------------------------------------------------
// The character-select slots
//----------------------------------------------------------------------
namespace {

// The status manager as a slot of these stats left it.
struct Combat
{
	int toHit, minDam, maxDam, defense, protection;
	explicit Combat(MStatusManager& status)
		: toHit(status.GetTOHIT()), minDam(status.GetMinDAM()), maxDam(status.GetMaxDAM()),
		defense(status.GetDefense()), protection(status.GetProtection()) {}
	bool operator==(const Combat& o) const
	{
		return toHit == o.toHit && minDam == o.minDam && maxDam == o.maxDam && defense == o.defense && protection == o.protection;
	}
};
template <class Info>
Combat Expected(const Info& info)
{
	MStatusManager status;
	status.SetCharacterSelectSlot(info);
	return Combat(status);
}

// The status manager, the user's character names and the Ousters coats,
// given back after.
struct SlotScene
{
	GameModelWorld world{ 4 };
	CREATURE_TABLE creatures;
	CREATURESPRITE_TABLE sprites;
	MStatusManager oldStatus = g_StatusManager;
	CREATURE_TABLE* oldCreatures = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	MItem* oldCoats[OUSTERS_COAT_MAX];
	SlotScene()
	{
		for (int i = 0; i < OUSTERS_COAT_MAX; ++i) { oldCoats[i] = g_pPacketItemOustersCoat[i]; g_pPacketItemOustersCoat[i] = NULL; }
		g_pUserInformation->Character[0] = "Kim";
		g_pUserInformation->Character[1] = "Lee";
		g_pUserInformation->Character[2] = "Park";
		creatures.Init(10);
		testfw::MutableRow(creatures, 5).SpriteTypes.Init(1);
		testfw::MutableRow(creatures, 5).SpriteTypes.Set(0, 0);
		g_pCreatureTable = &creatures;
		sprites.Init(1);
		g_pCreatureSpriteTable = &sprites;
		g_pItemTable->InitClass(ITEM_CLASS_VAMPIRE_COAT, 2);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_VAMPIRE_COAT, 1).AddonMaleFrameID = 5;
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_VAMPIRE_COAT, 1).AddonFemaleFrameID = 5;
		g_pItemTable->InitClass(ITEM_CLASS_OUSTERS_COAT, 2);
		testfw::MutableRow(*g_pItemOptionTable, 3).ColorSet = 400;
	}
	~SlotScene()
	{
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatures;
		for (int i = 0; i < OUSTERS_COAT_MAX; ++i) g_pPacketItemOustersCoat[i] = oldCoats[i];
		g_StatusManager = oldStatus;
	}
	// A slot of other stats first, so the manager's state shows the call.
	void Unlike()
	{
		PCVampireInfo other{};
		other.setSTR(200);
		other.setDEX(200);
		other.setINT(200);
		other.setLevel(90);
		g_StatusManager.SetCharacterSelectSlot(other);
	}
};

PCSlayerInfo Slayer()
{
	PCSlayerInfo info{};
	info.setSTR(30);
	info.setDEX(57);
	info.setINT(10);
	info.setSkillDomainLevel(SKILL_DOMAIN_BLADE, 11);
	info.setSkillDomainLevel(SKILL_DOMAIN_SWORD, 33);
	info.setAlignment(10001);
	info.setHP(50, 80);
	info.setMP(20, 40);
	return info;
}

} // namespace

TEST(GameUI2, ASlayerSlotGivesTheStatusManagerItsCombatStats)
{
	// The slot shows the combat stats the status manager works out for
	// this character; the slot itself goes to the title's character
	// manager, which is not running here (the next test has the outlook
	// past the slot's tables).
	SlotScene scene;
	for (Sex sex : {MALE, FEMALE})
		for (int kind = 0; kind < 3; ++kind)
		{
			// Each part worn and coloured unique, quest or from the option
			// table; and each worn part left off.
			const Color_t color = kind == 0 ? Color_t(UNIQUE_ITEM_COLOR) : kind == 1 ? Color_t(QUEST_ITEM_COLOR) : Color_t(3);
			PCSlayerInfo info = Slayer();
			info.setSex(sex);
			info.setHairStyle(HAIR_STYLE3);
			info.setHelmetType(kind == 2 ? HELMET_NONE : HELMET2);
			info.setJacketType(JACKET2);
			info.setPantsType(PANTS2);
			info.setWeaponType(kind == 2 ? WEAPON_NONE : WEAPON_MACE1);
			info.setShieldType(kind == 2 ? SHIELD_NONE : SHIELD2);
			info.setHelmetColor(color);
			info.setJacketColor(color);
			info.setPantsColor(color);
			info.setWeaponColor(color);
			info.setShieldColor(color);
			info.setHairColor(color);
			info.setSkinColor(1000);
			scene.Unlike();
			CHECK(!(Expected(info) == Combat(g_StatusManager)));
			UI_SetCharacter(2, &info);
			CHECK(Expected(info) == Combat(g_StatusManager));
		}
}

TEST(GameUI2, ASlayerSlotsOutlookPastTheSlotsTablesIsTheirFirstEntry)
{
	// The face, hair, helmet and shield tables hold 3 entries and the coat
	// and trouser ones 4, but the login packet's outlook fields are 2 and 3
	// bits wide: a hair style, helmet or shield of 3 or a jacket or pants
	// of 4 to 7 takes the table's first entry (the first face and hair,
	// nothing worn), as PacketFunction's PacketHair and PacketItem do. The
	// slot itself goes to the title's character manager, which is not
	// running here; the sanitizers see a read past a table.
	SlotScene scene;
	for (Sex sex : {MALE, FEMALE})
		for (int past = 4; past < 8; ++past)
		{
			PCSlayerInfo info = Slayer();
			info.setSex(sex);
			info.setHairStyle(HairStyle(3));
			info.setHelmetType(HelmetType(3));
			info.setJacketType(JacketType(past));
			info.setPantsType(PantsType(past));
			info.setShieldType(ShieldType(3));
			CHECK_EQ(3, int(info.getHairStyle()));
			CHECK_EQ(past, int(info.getJacketType()));
			CHECK_EQ(past, int(info.getPantsType()));
			scene.Unlike();
			UI_SetCharacter(2, &info);
			CHECK(Expected(info) == Combat(g_StatusManager));
		}
}

TEST(GameUI2, AVampireSlotGivesTheStatusManagerItsCombatStats)
{
	SlotScene scene;
	for (Sex sex : {MALE, FEMALE})
	{
		PCVampireInfo info{};
		info.setSex(sex);
		info.setSTR(20);
		info.setDEX(20);
		info.setINT(20);
		info.setLevel(7);
		info.setExp(1234);
		info.setAlignment(10001);
		info.setCoatType(1);
		info.setCoatColor(UNIQUE_ITEM_COLOR);
		info.setBatColor(5000);
		info.setSkinColor(QUEST_ITEM_COLOR);
		scene.Unlike();
		CHECK(!(Expected(info) == Combat(g_StatusManager)));
		UI_SetCharacter(0, &info);
		CHECK(Expected(info) == Combat(g_StatusManager));
	}
	// A coat type past the item table reads its empty fallback row: harmless.
	PCVampireInfo info{};
	info.setCoatType(9);
	scene.Unlike();
	UI_SetCharacter(1, &info);
	CHECK(Expected(info) == Combat(g_StatusManager));
}

TEST(GameUI2, AnOustersSlotSaysWhenItsCoatIsUnknown)
{
	// The coat's sprite comes from the packet's Ousters coat items; a
	// coat past them, or one the table left empty, is reported on stderr
	// and drawn as sprite 0.
	SlotScene scene;
	MPotion coat;
	coat.SetItemType(1);

	PCOustersInfo info{};
	info.setSex(FEMALE);
	info.setSTR(10);
	info.setDEX(57);
	info.setINT(10);
	info.setLevel(13);
	info.setAlignment(10001);
	info.setArmType(OustersArmType(1));
	info.setArmColor(UNIQUE_ITEM_COLOR);
	info.setBootsColor(QUEST_ITEM_COLOR);
	info.setCoatColor(5000);
	info.setHairColor(7);

	info.setCoatType(OUSTERS_COAT_MAX);
	scene.Unlike();
	std::string said = Stderr([&]() { UI_SetCharacter(1, &info); });
	CHECK(Has(said, "ERROR: UI_SetCharacter: coatType=5 is out of range [0, 5)"));
	CHECK(!Has(said, "is NULL"));
	CHECK(Expected(info) == Combat(g_StatusManager));

	info.setCoatType(OUSTERS_COAT4);
	scene.Unlike();
	said = Stderr([&]() { UI_SetCharacter(1, &info); });
	CHECK(Has(said, "ERROR: UI_SetCharacter: g_pPacketItemOustersCoat[4] is NULL!"));
	CHECK(!Has(said, "out of range"));
	CHECK(Expected(info) == Combat(g_StatusManager));

	g_pPacketItemOustersCoat[OUSTERS_COAT4] = &coat;
	scene.Unlike();
	said = Stderr([&]() { UI_SetCharacter(1, &info); });
	CHECK(!Has(said, "ERROR: UI_SetCharacter"));
	CHECK(Expected(info) == Combat(g_StatusManager));
	g_pPacketItemOustersCoat[OUSTERS_COAT4] = NULL;
}

TEST(GameUI2, ASlotPastTheUsersThreeCharacterNamesIsRefused)
{
	// The slot is the index of the character's name (the user's three);
	// LCPCList hands 0 to SLOT_MAX - 1. Any other slot does nothing: the
	// status manager keeps the stats it had.
	SlotScene scene;
	PCSlayerInfo slayer = Slayer();
	PCVampireInfo vampire{};
	vampire.setLevel(7);
	PCOustersInfo ousters{};
	ousters.setLevel(13);
	ousters.setCoatType(OUSTERS_COAT_MAX);
	scene.Unlike();
	const Combat before(g_StatusManager);
	for (int slot : {-1, 3, 4, INT_MIN, INT_MAX})
	{
		UI_SetCharacter(slot, &slayer);
		UI_SetCharacter(slot, &vampire);
		const std::string said = Stderr([&]() { UI_SetCharacter(slot, &ousters); });
		CHECK(said.empty());
		CHECK(before == Combat(g_StatusManager));
	}
}

//----------------------------------------------------------------------
// The loading screen
//----------------------------------------------------------------------
namespace {

std::string drawCalls;
bool UiFlag(std::uint32_t flag)
{
	drawCalls += "flag" + N(flag) + ";";
	return flag == EVENTFLAG_NOT_DRAW_UI;	// the UI is not drawn
}
const UiRuntime::Host uiRuntime{ .EventFlagActive = UiFlag };
spritectl_surface_t Flipped() { drawCalls += "flip;"; return SPRITECTL_INVALID_SURFACE; }

WORD Pixel(CSpriteSurface& surface, int x, int y)
{
	spritectl_surface_info_t info{};
	if (spritectl_lock_surface(surface.GetBackendSurface(), &info) != 0 || !info.pixels) return 0xdead;
	const WORD value = reinterpret_cast<WORD*>(static_cast<BYTE*>(info.pixels) + y * info.pitch)[x];
	spritectl_unlock_surface(surface.GetBackendSurface());
	return value;
}

void Pattern(CSpriteSurface& surface)
{
	spritectl_surface_info_t info{};
	if (spritectl_lock_surface(surface.GetBackendSurface(), &info) != 0 || !info.pixels) return;
	for (int y = 0; y < info.height; ++y)
		for (int x = 0; x < info.width; ++x)
			reinterpret_cast<WORD*>(static_cast<BYTE*>(info.pixels) + y * info.pitch)[x] = WORD(1 + y * info.width + x);
	spritectl_unlock_surface(surface.GetBackendSurface());
}

// The back buffer 0x7777 and the last frame a pattern, both 10x8, the
// game's rectangle the top left 6x4 of them.
struct DrawScene
{
	CSpriteSurface* oldBack = g_pBack;
	CSpriteSurface* oldLast = g_pLast;
	RECT oldRect = g_GameRect;
	BOOL oldDraw = g_bDrawProgress;
	const UiRuntime::Host* oldRuntime = UiRuntime::SetHost(&uiRuntime);
	CSpriteSurface back, last;
	DrawScene()
	{
		drawCalls.clear();
		spritectl_init();
		back.Init(10, 8);
		last.Init(10, 8);
		back.FillSurface(0x7777);
		Pattern(last);
		g_pBack = &back;
		g_pLast = &last;
		g_GameRect = {0, 0, 6, 4};
		CSDLGraphics::SetBackBufferSource(Flipped);
	}
	~DrawScene()
	{
		CSDLGraphics::SetBackBufferSource(nullptr);
		UiRuntime::SetHost(oldRuntime);
		g_bDrawProgress = oldDraw;
		g_GameRect = oldRect;
		g_pLast = oldLast;
		g_pBack = oldBack;
	}
};

} // namespace

TEST(GameUI2, TheLoadingScreenDrawsOnlyWhileItRuns)
{
	DrawScene scene;
	g_bDrawProgress = FALSE;
	UI_DrawProgress(40);
	CHECK(drawCalls.empty());
	CHECK_EQ(0x7777, int(Pixel(scene.back, 0, 0)));
	CHECK_EQ(0x7777, int(Pixel(scene.back, 9, 7)));
}

TEST(GameUI2, TheLoadingScreenClearsTheBackShowsTheUICopiesTheLastFrameAndFlips)
{
	// The back buffer is filled black, the UI drawn (it is told not to
	// be here), the game's rectangle of the last frame copied over it at
	// the top left, and the frame shown.
	DrawScene scene;
	g_bDrawProgress = TRUE;
	UI_DrawProgress(40);
	CHECK(drawCalls == "flag" + N(EVENTFLAG_NOT_DRAW_UI) + ";flip;");
	for (int y = 0; y < 8; ++y)
		for (int x = 0; x < 10; ++x)
			CHECK_EQ(x < 6 && y < 4 ? int(Pixel(scene.last, x, y)) : 0, int(Pixel(scene.back, x, y)));
	CHECK_EQ(TRUE, g_bDrawProgress);
}

TEST(GameUI2, EndingTheLoadingScreenStopsItsDrawing)
{
	DrawScene scene;
	g_bDrawProgress = TRUE;
	UI_EndProgress();
	CHECK_EQ(FALSE, g_bDrawProgress);
	UI_DrawProgress(100);
	CHECK(drawCalls.empty());
	UI_EndProgress();
	CHECK_EQ(FALSE, g_bDrawProgress);
}

//----------------------------------------------------------------------
// The HP and MP bars' help
//----------------------------------------------------------------------
namespace {

std::vector<int> mails;
const MHelpHost helpHost{ .AddHelpMail = [](HELP_EVENT he) { mails.push_back(int(he)); } };

struct HelpScene
{
	CLIENT_MODE oldMode = g_Mode;
	HelpScene() { mails.clear(); SetHelpHost(&helpHost); g_Mode = MODE_GAME; }
	~HelpScene() { SetHelpHost(nullptr); g_Mode = oldMode; }
};

const std::vector<int> kPotion{HELP_EVENT_USE_POTION};

} // namespace

TEST(GameUI2, HPFallingTo30PercentAsksForAPotion)
{
	// Each time the HP goes from above 30% to 30% or less, the potion's
	// help mail; the last percent is kept whatever the mode.
	HelpScene scene;
	UI_SetHP(100, 100);	// the last percent a full bar (the static persists)
	mails.clear();
	UI_SetHP(31, 100);
	CHECK(mails.empty());
	UI_SetHP(30, 100);
	CHECK(kPotion == mails);
	mails.clear();
	UI_SetHP(29, 100);	// still low: no new mail
	UI_SetHP(0, 100);
	CHECK(mails.empty());
	UI_SetHP(31, 100);
	UI_SetHP(30, 100);	// from 31 down again
	CHECK(kPotion == mails);

	// The current HP is at least 0 and at most the maximum, the maximum
	// at least 1.
	mails.clear();
	UI_SetHP(100, 100);
	UI_SetHP(5, 0);		// 1 of 1
	UI_SetHP(5, -1);	// 1 of 1
	UI_SetHP(500, 100);	// 100 of 100
	CHECK(mails.empty());
	UI_SetHP(-5, 100);	// 0 of 100
	CHECK(kPotion == mails);
	mails.clear();
	UI_SetHP(100, 100);
	UI_SetHP(0, 0);		// 0 of 1
	CHECK(kPotion == mails);

	// The percentage holds past the 21,474,836 HP whose hundredfold is
	// past the int range.
	mails.clear();
	UI_SetHP(100, 100);
	UI_SetHP(25000000, 25000000);	// 100 of 100
	UI_SetHP(INT_MAX, INT_MAX);
	CHECK(mails.empty());
	UI_SetHP(7500000, 25000000);	// 30%
	CHECK(kPotion == mails);

	// Out of the game no mail is sent, but the drop is remembered.
	mails.clear();
	UI_SetHP(100, 100);
	g_Mode = MODE_MAINMENU;
	UI_SetHP(20, 100);
	g_Mode = MODE_GAME;
	UI_SetHP(10, 100);
	CHECK(mails.empty());
	UI_SetHP(100, 100);
}

TEST(GameUI2, MPFallingTo30PercentAsksForAPotionAndAbsorbingASoul)
{
	// Each time the MP goes from above 30% to 30% or less, the potion's
	// and the soul-absorbing help mails; the last percent is kept whatever
	// the mode, as the HP's is.
	HelpScene scene;
	const std::vector<int> lowMP{HELP_EVENT_USE_POTION, HELP_EVENT_ABSORB_SOUL};
	UI_SetMP(100, 100);	// the last percent a full bar (the static persists)
	mails.clear();
	UI_SetMP(31, 100);
	UI_SetMP(500, 100);	// 100 of 100
	// Past the 21,474,836 MP whose hundredfold is past the int range.
	UI_SetMP(25000000, 25000000);
	UI_SetMP(INT_MAX, INT_MAX);
	CHECK(mails.empty());
	UI_SetMP(30, 100);
	CHECK(lowMP == mails);
	mails.clear();
	UI_SetMP(29, 100);	// still low: no new mail
	UI_SetMP(-5, 100);	// 0 of 100
	CHECK(mails.empty());
	UI_SetMP(100, 100);
	UI_SetMP(30, 100);	// from 100 down again
	CHECK(lowMP == mails);

	// An MP with no maximum is 0%.
	mails.clear();
	UI_SetMP(31, 100);
	UI_SetMP(5, -1);
	CHECK(lowMP == mails);
	mails.clear();
	UI_SetMP(31, 100);
	UI_SetMP(0, 0);
	CHECK(lowMP == mails);

	// Out of the game no mail is sent, but the drop is remembered.
	mails.clear();
	UI_SetMP(100, 100);
	g_Mode = MODE_MAINMENU;
	UI_SetMP(20, 100);
	g_Mode = MODE_GAME;
	UI_SetMP(10, 100);
	CHECK(mails.empty());
	UI_SetMP(100, 100);
}

//----------------------------------------------------------------------
// The chat history
//----------------------------------------------------------------------
TEST(GameUI2, AChatLineReadsTheBaseOnlyForWhiteText)
{
	// The line goes to the game window's history (none here). With the
	// white-chat option the colour is the chat's text colour, read from
	// the UI's base; without it the base is not read at all.
	UserOption option;
	Base base;
	UserOption* oldOption = g_pUserOption;
	Base* oldBase = gpC_base;
	g_pUserOption = &option;

	option.ChatWhite = FALSE;
	gpC_base = NULL;
	UI_AddChatToHistory("hello", "Kim", CLD_NORMAL, 0x123456);
	UI_AddChatToHistory("hello", NULL, CLD_TOTAL - 1, 0x123456);

	option.ChatWhite = TRUE;
	gpC_base = &base;
	UI_AddChatToHistory("hello", "Kim", CLD_WHISPER, 0x123456);
	// A missing line, or a condition that is negative or from CLD_TOTAL
	// on, is dropped.
	UI_AddChatToHistory(NULL, "Kim", CLD_NORMAL, 0);
	UI_AddChatToHistory("hello", "Kim", CLD_TOTAL, 0);
	UI_AddChatToHistory("hello", "Kim", 100, 0);
	UI_AddChatToHistory("hello", "Kim", -1, 0);
	UI_AddChatToHistory("hello", "Kim", INT_MIN, 0);

	gpC_base = oldBase;
	g_pUserOption = oldOption;
}

//----------------------------------------------------------------------
// The mouse's item
//----------------------------------------------------------------------
namespace {

std::vector<std::string> sounds;
const WorldEvents::Host soundHost = []() {
	WorldEvents::Host host{};
	host.PlaySound = [](TYPE_SOUNDID id) {
		// Sounded before the mouse has the item.
		sounds.push_back(N(id) + (gpC_mouse_pointer->GetPickUpItem() == NULL ? "|empty" : "|holding"));
	};
	return host;
}();

} // namespace

TEST(GameUI2, PickingUpAnItemSoundsItThenGivesItToTheMouseDroppingFreesIt)
{
	GameModelWorld world;
	C_VS_UI_ITEM uiItem;
	C_VS_UI_MOUSE_POINTER mouse;
	C_VS_UI_ITEM* oldItem = gpC_item;
	C_VS_UI_MOUSE_POINTER* oldMouse = gpC_mouse_pointer;
	const WorldEvents::Host* oldHost = WorldEvents::SetHost(&soundHost);
	gpC_item = &uiItem;
	gpC_mouse_pointer = &mouse;
	sounds.clear();
	g_pItemTable->InitClass(ITEM_CLASS_POTION, 3);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_POTION, 2).InventorySoundID = 41;
	MPotion potion;
	potion.SetItemType(2);

	UI_PickUpItem(&potion);
	CHECK((std::vector<std::string>{"41|empty"}) == sounds);
	CHECK(mouse.GetPickUpItem() == &potion);

	UI_DropItem();
	CHECK(mouse.GetPickUpItem() == NULL);
	CHECK_EQ(1, int(sounds.size()));

	WorldEvents::SetHost(oldHost);
	gpC_mouse_pointer = oldMouse;
	gpC_item = oldItem;
}

//----------------------------------------------------------------------
// The UI's results
//----------------------------------------------------------------------
namespace {

std::vector<std::string> results;
int payload = 0;
const GameUI::Host resultHost = []() {
	GameUI::Host host{};
	host.ExecuteUIMessage = [](DWORD message, intptr_t left, intptr_t right, void* void_ptr) {
		results.push_back(N(message) + "|" + std::to_string(left) + "|" + std::to_string(right) + "|" + (void_ptr == &payload ? "payload" : "other"));
	};
	return host;
}();

struct HostScope
{
	const GameUI::Host* old;
	explicit HostScope(const GameUI::Host* host) : old(GameUI::SetHost(host)) { results.clear(); }
	~HostScope() { GameUI::SetHost(old); }
};

} // namespace

TEST(GameUI2, EveryResultOfTheUIGoesToTheHostAsItCame)
{
	{
		HostScope scope(&resultHost);
		UI_ResultReceiver(0xFFFFFFFF, INTPTR_MIN, INTPTR_MAX, &payload);
		UI_ResultReceiver(17, -1, 0, NULL);
		CHECK((std::vector<std::string>{"4294967295|" + std::to_string(INTPTR_MIN) + "|" + std::to_string(INTPTR_MAX) + "|payload",
			"17|-1|0|other"}) == results);
		// SetHost answers the host it replaces.
		CHECK(GameUI::SetHost(&resultHost) == &resultHost);
	}
	// No host, or one without the entry: dropped.
	{
		HostScope scope(NULL);
		UI_ResultReceiver(17, 1, 2, &payload);
		const GameUI::Host empty{};
		GameUI::SetHost(&empty);
		UI_ResultReceiver(17, 1, 2, &payload);
		GameUI::SetHost(NULL);
		CHECK(results.empty());
	}
}

//----------------------------------------------------------------------
// The party
//----------------------------------------------------------------------
TEST(GameUI2, APartyRequestRemembersWhoAsked)
{
	ZoneScene scene;
	scene.Creature(300, "Kim");
	scene.temp.PartyInviter = 7;

	UI_RunPartyRequest(301);	// nobody of the zone: nothing
	CHECK_EQ(7, scene.temp.PartyInviter);
	UI_RunPartyRequest(300);
	CHECK_EQ(300, scene.temp.PartyInviter);
	CHECK(TempInformation::MODE_NULL == scene.temp.GetMode());

	// No zone: nothing.
	scene.temp.PartyInviter = 7;
	g_pZone = NULL;
	UI_RunPartyRequest(300);
	CHECK_EQ(7, scene.temp.PartyInviter);
}

TEST(GameUI2, APartyInvitationRemembersWhoAskedUnlessTheyKeepAShopOrPartiesAreClosed)
{
	ZoneScene scene;
	scene.Creature(300, "Kim");
	scene.Creature(301, "Lee", 1);
	scene.Creature(302, "Park", 2);
	scene.temp.PartyInviter = 7;

	UI_RunPartyAsk(301);	// keeps a personal shop
	CHECK_EQ(7, scene.temp.PartyInviter);
	const DWORD party = 1u << 0;	// SystemAvailabilitiesManager::SYSTEM_PARTY (protected)
	scene.availabilities.SetFlag(~party);
	UI_RunPartyAsk(300);	// the party system is closed
	CHECK_EQ(7, scene.temp.PartyInviter);
	scene.availabilities.SetFlag(party);
	UI_RunPartyAsk(302);	// a shop flag other than 1 is no shop
	CHECK_EQ(302, scene.temp.PartyInviter);
	UI_RunPartyAsk(300);
	CHECK_EQ(300, scene.temp.PartyInviter);
	CHECK(TempInformation::MODE_NULL == scene.temp.GetMode());

	// Nobody of the zone, or no zone: nothing.
	scene.temp.PartyInviter = 7;
	UI_RunPartyAsk(399);
	CHECK_EQ(7, scene.temp.PartyInviter);
	g_pZone = NULL;
	UI_RunPartyAsk(300);
	CHECK_EQ(7, scene.temp.PartyInviter);
}

TEST(GameUI2, ClosingThePartyQuestionEndsAPartyWaitAndRequest)
{
	Scene scene;
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_PARTY);
	scene.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
	scene.temp.PartyInviter = 44;
	UI_ClosePartyAsk();
	CHECK(scene.me->IsWaitVerifyNULL());
	CHECK(TempInformation::MODE_NULL == scene.temp.GetMode());
	CHECK_EQ(44, scene.temp.PartyInviter);

	// Another wait or request is left.
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	UI_ClosePartyAsk();
	CHECK(MPlayer::WAIT_VERIFY_TRADE == scene.me->GetWaitVerify());
	CHECK(TempInformation::MODE_TRADE_REQUEST == scene.temp.GetMode());
}

TEST(GameUI2, ClosingThePartyCancelEndsAPartyWaitAndRequest)
{
	Scene scene;
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_PARTY);
	scene.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
	UI_ClosePartyCancel();
	CHECK(scene.me->IsWaitVerifyNULL());
	CHECK(TempInformation::MODE_NULL == scene.temp.GetMode());

	// Another wait or request is left; the motorcycle's wait is 1, the
	// value of a yes.
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_MOTORCYCLE_GETON);
	scene.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
	UI_ClosePartyCancel();
	CHECK(MPlayer::WAIT_VERIFY_MOTORCYCLE_GETON == scene.me->GetWaitVerify());
	CHECK(TempInformation::MODE_NULL == scene.temp.GetMode());
	scene.temp.SetMode(TempInformation::MODE_TRADE_REQUEST);
	UI_ClosePartyCancel();
	CHECK(TempInformation::MODE_TRADE_REQUEST == scene.temp.GetMode());
}

//----------------------------------------------------------------------
// The guild windows
//----------------------------------------------------------------------
namespace {

GuildInfo* Guild(int id, const char* name)
{
	GuildInfo* pInfo = new GuildInfo;
	pInfo->setGuildID(GuildID_t(id));
	pInfo->setGuildName(name);
	pInfo->setGuildMaster("Kim");
	pInfo->setGuildExpireDate("2026-12-31");
	pInfo->setGuildMemberCount(3);
	return pInfo;
}

} // namespace

TEST(GameUI2, TheGuildListsHideTheTalkAndTakeEveryGuildOutOfThePacket)
{
	// Recorded defect: the guilds taken out are never freed (the packet
	// frees only what it still holds); the test frees them.
	{
		Scene scene;
		scene.TalkOpen();
		GCWaitGuildList packet;
		GuildInfo* guilds[] = {Guild(1, "One"), Guild(2, "Two")};
		for (GuildInfo* pInfo : guilds) packet.addGuildInfo(pInfo);
		UI_ShowWaitGuildList(&packet);
		CHECK(Same({"finish 1"}));
		CHECK_EQ(0, int(packet.getListNum()));
		CHECK(std::string("Two") == guilds[1]->getGuildName());	// still there
		for (GuildInfo* pInfo : guilds) delete pInfo;
	}
	{
		Scene scene;
		scene.TalkOpen();
		GCActiveGuildList packet;
		GuildInfo* guilds[] = {Guild(1, "One"), Guild(2, "Two"), Guild(3, "Three")};
		for (GuildInfo* pInfo : guilds) packet.addGuildInfo(pInfo);
		UI_ShowActiveGuildList(&packet);
		CHECK(Same({"finish 1"}));
		CHECK_EQ(0, int(packet.getListNum()));
		CHECK(std::string("Three") == guilds[2]->getGuildName());
		for (GuildInfo* pInfo : guilds) delete pInfo;
	}
	{
		// The union's guilds, after its master guild.
		Scene scene;
		scene.TalkOpen();
		GCShowUnionInfo packet;
		packet.getMasterGuildInfo().setGuildID(1);
		packet.getMasterGuildInfo().setGuildName("Master");
		SingleGuildInfo* guilds[] = {new SingleGuildInfo, new SingleGuildInfo};
		for (SingleGuildInfo* pInfo : guilds)
		{
			pInfo->setGuildID(2);
			pInfo->setGuildName("Member");
			pInfo->setGuildMemberCount(4);
			packet.addGuildInfoList(pInfo);
		}
		UI_ShowUnionGuildMemberInfo(&packet);
		CHECK(Same({}));	// the talk stays
		CHECK(packet.popFrontUnionGuildInfoList() == NULL);
		CHECK(std::string("Master") == packet.getMasterGuildInfo().getGuildName());
		CHECK(std::string("Member") == guilds[1]->getGuildName());
		for (SingleGuildInfo* pInfo : guilds) delete pInfo;
	}
}

TEST(GameUI2, TheGuildMemberListTakesEveryMemberOutOfThePacket)
{
	// Each member's server is looked up in the current world when it is
	// logged on; one on no server of it shows as not logged on. Only the
	// game window would see the lines; the members taken out are never
	// freed (recorded defect; the test frees them).
	Scene scene;
	CServerInformation servers;
	CServerInformation* oldServers = g_pServerInformation;
	g_pServerInformation = &servers;
	ServerGroup* pGroup = new ServerGroup;
	SERVER_INFO* pServer = new SERVER_INFO;
	pServer->ServerName = "Bathory";
	pGroup->AddData(1, pServer);
	servers.AddData(5, pGroup);
	CHECK(servers.SetServerGroupID(5));

	GuildMemberInfo* members[3];
	for (int i = 0; i < 3; ++i)
	{
		members[i] = new GuildMemberInfo;
		members[i]->setName("Member" + N(i));
		members[i]->setRank(GuildMemberRank_t(i));
		members[i]->setLogOn(i != 2);
		members[i]->setServerID(ServerID_t(i));	// server 0 is not in the world
	}
	// The second time with no world chosen.
	CServerInformation none;
	for (CServerInformation* pServers : {&servers, &none})
	{
		g_pServerInformation = pServers;
		GCGuildMemberList packet;
		packet.setType(BYTE(pServers == &none));
		for (GuildMemberInfo* pInfo : members) packet.addGuildMemberInfo(pInfo);
		UI_ShowGuildMemberList(&packet);
		CHECK_EQ(0, int(packet.getListNum()));
	}
	CHECK(std::string("Member2") == members[2]->getName());
	for (GuildMemberInfo* pInfo : members) delete pInfo;
	CHECK(Same({}));
	g_pServerInformation = oldServers;
}

TEST(GameUI2, AWaitingGuildsInfoTakesEveryMemberOutOfThePacket)
{
	GCShowWaitGuildInfo packet;
	packet.setGuildID(9);
	packet.setGuildName("Team");
	packet.setGuildMaster("Kim");
	packet.setGuildIntro("Hello");
	packet.setJoinFee(100);
	packet.addMember("One");
	packet.addMember("Two");
	packet.addMember("Three");
	UI_ShowWaitGuildInfo(&packet);
	CHECK_EQ(0, int(packet.getMemberNum()));
	CHECK(std::string("Team") == packet.getGuildName());
}
