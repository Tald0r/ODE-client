#include "test_framework.h"
#include "type_table_access.h"
#include "GameInit.h"
#include "GameMain.h"
#include "GameInitInfo.h"
#include "GameInitInfoHost.h"
#include "Properties.h"
#include "Exception.h"
#include "TextEncoding.h"
#include "DebugLog.h"
#include "ClientMode.h"
#include "FrameClock.h"
#include "GameMessages.h"
#include "../basic/CMessageArray.h"
#include "MGameStringTable.h"
#include "MStringArray.h"
#include "ClientConfig.h"
#include "UserOption.h"
#include "UserInformation.h"
#include "TempInformation.h"
#include "CServerInformation.h"
#include "SystemAvailabilities.h"
#include "HelpEvent.h"
#include "CGameUpdate.h"
#include "CWaitPacketUpdate.h"
#include "CWaitUIUpdate.h"
#include "CDirectInput.h"
#include "DXLib/DXInputHost.h"
// What InitGame and InitGameObject make.
#include "UIDialog.h"
#include "MChatManager.h"
#include "MHelpStringTable.h"
#include "MHelpDisplayer.h"
#include "MLevelNameTable.h"
#include "MonsterNameTable.h"
#include "ExperienceTable.h"
#include "MNPCScriptTable.h"
#include "ModifyStatusManager.h"
#include "KeyAccelerator.h"
#include "AcceleratorManager.h"
#include "ProfileManager.h"
#include "MGuildMarkManager.h"
#include "MGuildInfoMapper.h"
#include "MEventManager.h"
#include "MEventQueue.h"
#include "MJusticeAttackManager.h"
#include "RankBonusTable.h"
#include "MWarManager.h"
#include "SkillDef.h"
#include "FameInfo.h"
#include "MTimeItemManager.h"
#include "MMonsterKillQuestInfo.h"
#include "ShrineInfoManager.h"
#include "MEffectSpriteTypeTable.h"
#include "EffectSpriteTypeDef.h"
#include "MActionInfoTable.h"
#include "MCreatureSpriteTable.h"
#include "MCreatureTable.h"
#include "MEffectGeneratorTable.h"
#include "MEffectStatusTable.h"
#include "MInteractionObjectTable.h"
#include "MItemOptionTable.h"
#include "MItemTable.h"
#include "MMusicTable.h"
#include "MNPCTable.h"
#include "MShopTemplateTable.h"
#include "MSkillManager.h"
#include "MSoundTable.h"
#include "CUsingColorSetTable.h"
#include "MZoneTable.h"
#include "DXLib/CSoundPartManager.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MInventory.h"
#include "MParty.h"
#include "MGameTime.h"
#include "MPriceManager.h"
#include "MMoneyManager.h"
#include "TalkBox.h"
#include "UIMessageManager.h"
#include "MObjectSelector.h"
#include "MWeather.h"
#include "MScreenEffectManager.h"
#include "MPlayer.h"
#include "MZone.h"
// The hosts InitGameObject installs.
#include "MItem.h"
#include "Packet/WireHost.h"
#include "MEffectTarget.h"
#include "MEffect.h"
#include "MHelicopterManager.h"
#include "MZoneSoundManager.h"
#include "MGuidanceEffect.h"
#include "MParabolaEffect.h"
#include "MAttachEffect.h"
#include "MStopInventoryEffectGenerator.h"
#include "MStopZoneEffectGenerator.h"
#include "MStopZoneCrossEffectGenerator.h"
#include "MStopZoneXEffectGenerator.h"
#include "MStopZoneRhombusEffectGenerator.h"
#include "MStopZoneEmptyCrossEffectGenerator.h"
#include "MStopZoneEmptyRectEffectGenerator.h"
#include "MStopZoneRectEffectGenerator.h"
#include "MStopZoneWallEffectGenerator.h"
#include "MStopZoneMultipleEffectGenerator.h"
#include "MStopZoneRandomEffectGenerator.h"
#include "MStopZoneSelectableEffectGenerator.h"
#include "MSpreadOutEffectGenerator.h"
#include "MAroundZoneEffectGenerator.h"
#include "MFollowPathEffectGenerator.h"
#include "MMeteorDropEffectGenerator.h"
#include "MAttackCreatureParabolaEffectGenerator.h"
#include "MAttackCreatureEffectGenerator.h"
#include "MAttackCreatureHomingEffectGenerator.h"
#include "MRippleZoneWideEffectGenerator.h"
#include "MBloodyBreakerEffectGenerator.h"
#include "MBloodyWallEffectGenerator.h"
#include "MBloodyWaveEffectGenerator.h"
#include "MRippleZoneEffectGenerator.h"
#include "MRippleZonePixelEffectGenerator.h"
#include "MAttachZoneEffectGenerator.h"
#include "MAttachZoneAroundEffectGenerator.h"
#include "MAttachZoneSelectableEffectGenerator.h"
#include "MAttackZoneRectEffectGenerator.h"
#include "MSkipEffectGenerator.h"
#include "MStopZoneEmptyHorizontalWallEffectGenerator.h"
#include "MStopZoneEmptyVerticalEffectGenerator.h"
#include "MFallingEffectGenerator.h"
#include "MAttackZoneEffectGenerator.h"
#include "MFirePiecingEffectGenerator.h"
#include "MPlayingWithFireEffectGenerator.h"
#include "MAttackZoneBombEffectGenerator.h"
#include "MAttackZoneParabolaEffectGenerator.h"
#include "MMultipleFallingEffectGenerator.h"
#include "MRisingEffectGenerator.h"
#include "RankBonusHandlerHost.h"
#include "RegenZoneStatusHost.h"
#include "BonusSkillHost.h"
#include "SkillDowngradeHost.h"
#include "GuildPresentationHost.h"
#include "CommunicationPresentationHost.h"
#include "QuestWarPresentationHost.h"
#include "NoticePresentationHost.h"
#include "StatusPresentationHost.h"
#include "GameOptionsHost.h"
#include "PlayerPresentationHost.h"
#include "NPCPresentationHost.h"
#include "ZoneLoadingHost.h"
#include "TradePresentationHost.h"
#include "GameInfoPresentationHost.h"
#include "DialogPresentationHost.h"
#include "DialogWindowHost.h"
#include "LoginPresentationHost.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"
#include "VsUiPresentationHost.h"
#include "LoginListHost.h"
#include "UiRuntime.h"
#include "UIFunction.h"
#include "MonotonicClock.h"
#include "MathTable.h"
#include "EffectOrbit.h"
#include "MViewDef.h"

#include <algorithm>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

// The game's start-up (task 4.264 moved Client/GameInit.cpp into
// gameclient), group 3: InitGame, InitGameObject (the objects it makes and
// the hosts it installs), InitSkillTree, the system availabilities' file,
// the update loops' creation and release, and the small steps left.
//
// - InitGame starts the logging, makes a new g_pFileDef from
//   Data/Info/FileDef.inf in the working directory and takes its resource
//   encoding (an unknown one throws), makes the string tables and message
//   logs it lacks, reads the nickname table (missing: FALSE), the string
//   table (missing: FALSE unless the client is English, which applies its
//   built-in strings either way), sizes the message logs, then runs
//   InitClientConfig, InitInfomation, InitUserOption, InitVolume,
//   InitInput, InitDraw, InitGameObject, InitSurface and InitGameUpdate,
//   stopping at the first that fails.
// - InitGameObject loads the system availabilities once, makes each game
//   object it lacks, needs the version file, installs its 83 hosts, and
//   always makes a new weather, inventory effect manager and player.
// - InitSkillTree builds the skill tree and reads the server's domain
//   experience.
// - InitGameUpdate replaces the three update loops with ones on GameInit's
//   hosts; ReleaseGameUpdate deletes them.
//
// Not run here: InitGame past InitInfomation (the tests stop it there, at
// the missing regen tower file). Going on needs InitInfomation's 35 table
// files (test_game_init_info.cpp's fixture), then InitInput's SDL input
// backend and InitDraw's SDL window (CSDLGraphics::Init, which also sets
// the process-wide SDL_HINT_RENDER_DRIVER and on Windows wraps g_hWnd),
// InitGameObject's files (as below), and for InitSurface the UI:
// ClientMain's gC_ci (C_VS_UI::Init calls it unchecked), the skin file
// (Interface.inf; its absence asserts) and the UI's sprite packs and fonts.
// So InitGame's steps from InitUserOption on, and its tail
// (CheckInvalidProcess, UI_AffectUserOption, the profiles,
// SetMode(MODE_MAINMENU) and the two printf lines), are pinned one step at
// a time where they are this group's (InitVolume, InitGameObject,
// InitSkillTree, InitGameUpdate) and not end to end; InitClientConfig,
// InitInfomation and InitUserOption are test_game_init_info.cpp's.
// GetGameCursorPoint (the game loop's GetCursorPoint) reads the OS cursor:
// the desktop's on Windows, SDL's mouse state elsewhere; only its slot is
// pinned.
//
// Not run (undefined behaviour):
// - InitGameObject or InitSkillTree with g_pFileDef NULL, and InitSkillTree
//   with g_pSkillManager NULL (GameInit.cpp:4232): null dereferences.
// - InitGameObject without the tables the new player reads unchecked: a
//   creature table holding CREATURETYPE_SLAYER_MALE, the effect sprite and
//   status tables, the monster names (MCreature.cpp:826 binds a reference
//   through g_pMonsterNameTable) and the client configuration.
// - The wait loop's ClearInputReceivers with g_pSDLInput NULL
//   (GameInit.cpp:804), and the object selector's player entries with
//   g_pPlayer NULL or a zone entry with g_pZone NULL (GetZoneID,
//   GetZonePKType): unchecked dereferences.
// - InitGameObject with a skill tree already built (g_pSkillManager
//   initialised) and no UI: the new player's SetCreatureType reaches
//   MSkillSet::SetAvailableSkills, which asks GameInit's DialogPresentation
//   host for the mouse item, and UI_GetMouseItem calls through a null
//   gpC_mouse_pointer (GameUI.cpp:1711). The game runs InitGameObject once,
//   before the tree and the UI exist; a test that runs it again drops the
//   tree first.

namespace {

std::vector<std::string> messages;	// GameInitInfoHost::InitFailMessage
std::vector<std::string> calls;		// the host's other calls, in order

const GameInitInfoHost::Host initInfoHost = []() {
	GameInitInfoHost::Host h{};
	h.InitFailMessage = [](const char* message) { messages.push_back(message); };
	h.ShowError = [](const char* message) { calls.push_back(std::string("error ") + message); };
	h.PersonalShopUpdateTime = []() { calls.push_back("shop time"); return 1234; };
	h.InitPacketItemTable = []() { calls.push_back("packet items"); };
	h.SetFadeStart = [](signed char, signed char, signed char, BYTE, BYTE, BYTE, WORD) { calls.push_back("fade"); };
	return h;
}();

std::string FailOpen(const std::string& path) { return "[Error] File Open Error : [" + path + "]"; }

//----------------------------------------------------------------------
// The globals a test changes, put back in reverse order when it ends: a
// lent one points at the test's object; an owned one starts empty (or
// with what the test gives it) and whatever it holds at the end is
// deleted.
//----------------------------------------------------------------------
struct Globals
{
	std::vector<std::function<void()>> undo;
	template <class T> void Lend(T*& global, T* value)
	{
		T* saved = global;
		global = value;
		undo.push_back([&global, saved]() { global = saved; });
	}
	template <class T> void Own(T*& global, T* start = nullptr)
	{
		T* saved = global;
		global = start;
		undo.push_back([&global, saved]() { delete global; global = saved; });
	}
	template <class T> void Keep(T& value)
	{
		T saved = value;
		undo.push_back([&value, saved]() { value = saved; });
	}
	void Then(std::function<void()> step) { undo.push_back(std::move(step)); }
	~Globals()
	{
		for (auto step = undo.rbegin(); step != undo.rend(); ++step)
			(*step)();
	}
};

// A scratch directory, removed with everything in it.
struct TempDir
{
	std::filesystem::path path;
	TempDir()
	{
		static unsigned sequence = 0;
		path = std::filesystem::temp_directory_path() / ("darkeden_game_init_3_"
			+ std::to_string(MonotonicClock::Now().time_since_epoch().count()) + "_" + std::to_string(sequence++));
		std::filesystem::create_directories(path);
	}
	~TempDir()
	{
		std::error_code error;
		std::filesystem::remove_all(path, error);
	}
	std::string operator/(const std::string& name) const { return (path / name).string(); }
};

// Little-endian bytes, as the data files hold them.
struct Bytes
{
	std::string data;
	Bytes& Int(std::int32_t value)
	{
		for (int i = 0; i < 4; ++i) data += char((std::uint32_t(value) >> (8 * i)) & 0xFF);
		return *this;
	}
	Bytes& Word(std::uint16_t value)
	{
		data += char(value & 0xFF);
		data += char(value >> 8);
		return *this;
	}
	Bytes& String(const std::string& text) { Int(std::int32_t(text.size())); data += text; return *this; }
};

void WriteBytes(const std::string& path, const std::string& bytes)
{
	std::filesystem::create_directories(std::filesystem::path(path).parent_path());
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(bytes.data(), std::streamsize(bytes.size()));
}

bool Exists(const std::string& path) { return std::filesystem::exists(path); }

// Runs f; the Throwable it lets out, as "Name: message", or "" for none.
template <class F> std::string Thrown(F f)
{
	try { f(); }
	catch (Throwable& t) { return t.getName() + ": " + t.getMessage(); }
	return "";
}

//----------------------------------------------------------------------
// Protected and private members the start-up leaves its marks in.
//----------------------------------------------------------------------
template <class Tag, typename Tag::type Member>
struct Expose
{
	friend typename Tag::type Get(Tag) { return Member; }
};
struct WireHostOf { using type = const WireHost**; friend type Get(WireHostOf); };
template struct Expose<WireHostOf, &Wire::s_pHost>;
struct MouseReceiverOf { using type = void (*CSDLInput::*)(CSDLInput::E_MOUSE_EVENT, int, int, int); friend type Get(MouseReceiverOf); };
template struct Expose<MouseReceiverOf, &CSDLInput::m_fp_mouse_event_receiver>;
struct KeyboardReceiverOf { using type = void (*CSDLInput::*)(CSDLInput::E_KEYBOARD_EVENT, DWORD); friend type Get(KeyboardReceiverOf); };
template struct Expose<KeyboardReceiverOf, &CSDLInput::m_fp_keyboard_event_receiver>;
struct KeysOf { using type = BOOL (CSDLInput::*)[256]; friend type Get(KeysOf); };
template struct Expose<KeysOf, &CSDLInput::m_key>;

struct OrbitPositionsOf { using type = POINT (*)[EffectOrbit::TypeCount][EffectOrbit::StepCount]; friend type Get(OrbitPositionsOf); };
template struct Expose<OrbitPositionsOf, &EffectOrbit::s_Positions>;

struct PricePeek : MPriceManager { static const MPriceHost* Host() { return s_pHost; } };
struct GameLoopPeek : CGameUpdate { static const CGameUpdateHost* Host(const CGameUpdate& loop) { return loop.*(&GameLoopPeek::m_pHost); } };
struct WaitPacketPeek : CWaitPacketUpdate { static const CWaitPacketHost* Host(const CWaitPacketUpdate& loop) { return loop.*(&WaitPacketPeek::m_pHost); } };
struct WaitUIPeek : CWaitUIUpdate { static const CWaitUIHost* Host(const CWaitUIUpdate& loop) { return loop.*(&WaitUIPeek::m_pHost); } };
struct SelectorPeek : MObjectSelector { static const MObjectSelectorHost* Host(const MObjectSelector& selector) { return selector.*(&SelectorPeek::m_pHost); } };
struct MessagesPeek : CMessageArray { static int Width(const CMessageArray& log) { return log.*(&MessagesPeek::m_Length); } };
struct CreaturePeek : MCreature
{
	static bool*& EffectStatus(MCreature& creature) { return creature.*(&CreaturePeek::m_bEffectStatus); }
	static BYTE& ObjectType(MCreature& creature) { return creature.*(&CreaturePeek::m_ObjectType); }
};

// The deleted objects' count, by kind.
std::map<std::string, int> deleted;
struct GameLoopProbe : CGameUpdate { GameLoopProbe() : CGameUpdate(nullptr) {} ~GameLoopProbe() override { deleted["game loop"]++; } };
struct WaitPacketProbe : CWaitPacketUpdate { WaitPacketProbe() : CWaitPacketUpdate(nullptr) {} ~WaitPacketProbe() override { deleted["packet wait"]++; } };
struct WaitUIProbe : CWaitUIUpdate { WaitUIProbe() : CWaitUIUpdate(nullptr) {} ~WaitUIProbe() override { deleted["ui wait"]++; } };
struct PlayerProbe : MPlayer { ~PlayerProbe() override { deleted["player"]++; } };
struct EffectsProbe : MScreenEffectManager { ~EffectsProbe() override { deleted["inventory effects"]++; } };

} // namespace

//======================================================================
// The small steps
//======================================================================

TEST(GameInit3Steps, TheVolumeStepOnlyAnswersTrue)
{
	// The mixer is SDL_mixer's; InitVolume is a log line and TRUE.
	CHECK_EQ(TRUE, InitVolume());
	CHECK_EQ(TRUE, InitVolume());
}

TEST(GameInit3Steps, TheAddonSpriteLoadingInPartsAnswersTrueAndLoadsNothing)
{
	CHECK(PrepareLoadingAddonSPK());
	CHECK(LoadingAddonSPK(false));
	CHECK(LoadingAddonSPK(true));
}

//======================================================================
// InitGameUpdate / ReleaseGameUpdate
//======================================================================

namespace {

// The three loops' globals, empty for the test and put back after.
struct Loops
{
	Globals globals;
	Loops()
	{
		deleted.clear();
		globals.Own(g_pCGameUpdate);
		globals.Own(g_pCWaitPacketUpdate);
		globals.Own(g_pCWaitUIUpdate);
	}
};

} // namespace

TEST(GameInit3Loops, TheThreeLoopsAreMadeOnGameInitsHosts)
{
	Loops loops;
	CHECK_EQ(TRUE, InitGameUpdate());
	CHECK(g_pCGameUpdate != NULL && g_pCWaitPacketUpdate != NULL && g_pCWaitUIUpdate != NULL);
	if (g_pCGameUpdate == NULL || g_pCWaitPacketUpdate == NULL || g_pCWaitUIUpdate == NULL) return;

	// The packet wait's services are GameMain's and the frame clock's.
	const CWaitPacketHost* packet = WaitPacketPeek::Host(*g_pCWaitPacketUpdate);
	CHECK(packet != NULL);
	if (packet == NULL) return;
	CHECK(packet->ClearInputReceivers != NULL);
	CHECK(packet->StampFrameClock == &StampFrameClock);
	CHECK(packet->SetMode == &SetMode);
	CHECK(packet->UpdateDisconnected == &UpdateDisconnected);
	CHECK(packet->UpdateSocketInput == &UpdateSocketInput);
	CHECK(packet->UpdateSocketOutput == &UpdateSocketOutput);

	// The waiting screens' are GameMain's and the title's.
	const CWaitUIHost* ui = WaitUIPeek::Host(*g_pCWaitUIUpdate);
	CHECK(ui != NULL);
	if (ui == NULL) return;
	CHECK(ui->UpdateMouse == &UpdateMouse);
	CHECK(ui->UpdateInput == &UpdateInput);
	CHECK(ui->CheckInvalidProcess == &CheckInvalidProcess);
	CHECK(ui->UpdateSocketInput == &UpdateSocketInput);
	CHECK(ui->UpdateSocketOutput == &UpdateSocketOutput);
	CHECK(ui->SetMode == &SetMode);
	CHECK(ui->DrawTitleLoading == &DrawTitleLoading);
	CHECK(ui->EndTitleLoading == &EndTitleLoading);

	// The game loop's are GameMain's, and the cursor its own.
	const CGameUpdateHost* game = GameLoopPeek::Host(*g_pCGameUpdate);
	CHECK(game != NULL);
	if (game == NULL) return;
	CHECK(game->UpdateMouse == &UpdateMouse);
	CHECK(game->UpdateInput == &UpdateInput);
	CHECK(game->UpdateSocketInput == &UpdateSocketInput);
	CHECK(game->UpdateSocketOutput == &UpdateSocketOutput);
	CHECK(game->SetMode == &SetMode);
	CHECK(game->GetCursorPoint != NULL);
}

TEST(GameInit3Loops, ThePacketWaitTakesTheInputReceiversAway)
{
	Loops loops;
	CSDLInput input;
	Globals globals;
	globals.Lend(g_pSDLInput, &input);
	CHECK_EQ(TRUE, InitGameUpdate());
	const CWaitPacketHost* packet = g_pCWaitPacketUpdate != NULL ? WaitPacketPeek::Host(*g_pCWaitPacketUpdate) : NULL;
	CHECK(packet != NULL && packet->ClearInputReceivers != NULL);
	if (packet == NULL || packet->ClearInputReceivers == NULL) return;
	input.SetMouseEventReceiver([](CSDLInput::E_MOUSE_EVENT, int, int, int) {});
	input.SetKeyboardEventReceiver([](CSDLInput::E_KEYBOARD_EVENT, DWORD) {});
	CHECK(input.*Get(MouseReceiverOf()) != NULL);
	CHECK(input.*Get(KeyboardReceiverOf()) != NULL);
	packet->ClearInputReceivers();
	CHECK(input.*Get(MouseReceiverOf()) == NULL);
	CHECK(input.*Get(KeyboardReceiverOf()) == NULL);
}

TEST(GameInit3Loops, TheLoopsThereAreDeletedAndReplaced)
{
	Loops loops;
	g_pCGameUpdate = new GameLoopProbe;
	g_pCWaitPacketUpdate = new WaitPacketProbe;
	g_pCWaitUIUpdate = new WaitUIProbe;
	CHECK_EQ(TRUE, InitGameUpdate());
	CHECK_EQ(1, deleted["game loop"]);
	CHECK_EQ(1, deleted["packet wait"]);
	CHECK_EQ(1, deleted["ui wait"]);
	// The probes had no host; the new loops have GameInit's.
	CHECK(g_pCGameUpdate != NULL && GameLoopPeek::Host(*g_pCGameUpdate) != NULL);
	CHECK(g_pCWaitPacketUpdate != NULL && WaitPacketPeek::Host(*g_pCWaitPacketUpdate) != NULL);
	CHECK(g_pCWaitUIUpdate != NULL && WaitUIPeek::Host(*g_pCWaitUIUpdate) != NULL);
}

TEST(GameInit3Loops, EachLoopIsReplacedOnItsOwn)
{
	// Only the game loop there: it alone is deleted, all three are made.
	Loops loops;
	g_pCGameUpdate = new GameLoopProbe;
	CHECK_EQ(TRUE, InitGameUpdate());
	CHECK_EQ(1, deleted["game loop"]);
	CHECK_EQ(0, deleted["packet wait"] + deleted["ui wait"]);
	CHECK(g_pCGameUpdate != NULL && g_pCWaitPacketUpdate != NULL && g_pCWaitUIUpdate != NULL);
}

TEST(GameInit3Loops, TheReleaseDeletesEachLoopAndEmptiesItsPointer)
{
	Loops loops;
	g_pCGameUpdate = new GameLoopProbe;
	g_pCWaitPacketUpdate = new WaitPacketProbe;
	g_pCWaitUIUpdate = new WaitUIProbe;
	ReleaseGameUpdate();
	CHECK(g_pCGameUpdate == NULL && g_pCWaitPacketUpdate == NULL && g_pCWaitUIUpdate == NULL);
	CHECK_EQ(1, deleted["game loop"]);
	CHECK_EQ(1, deleted["packet wait"]);
	CHECK_EQ(1, deleted["ui wait"]);
	// Again, and with nothing there: nothing happens.
	ReleaseGameUpdate();
	CHECK(g_pCGameUpdate == NULL && g_pCWaitPacketUpdate == NULL && g_pCWaitUIUpdate == NULL);
	CHECK_EQ(3, deleted["game loop"] + deleted["packet wait"] + deleted["ui wait"]);
}

TEST(GameInit3Loops, TheReleaseTakesWhicheverLoopsThereAre)
{
	for (int present = 0; present < 3; ++present)
	{
		Loops loops;
		if (present == 0) g_pCGameUpdate = new GameLoopProbe;
		if (present == 1) g_pCWaitPacketUpdate = new WaitPacketProbe;
		if (present == 2) g_pCWaitUIUpdate = new WaitUIProbe;
		ReleaseGameUpdate();
		CHECK(g_pCGameUpdate == NULL && g_pCWaitPacketUpdate == NULL && g_pCWaitUIUpdate == NULL);
		CHECK_EQ(present == 0 ? 1 : 0, deleted["game loop"]);
		CHECK_EQ(present == 1 ? 1 : 0, deleted["packet wait"]);
		CHECK_EQ(present == 2 ? 1 : 0, deleted["ui wait"]);
	}
}

TEST(GameInit3Loops, MadeLoopsAreReleased)
{
	Loops loops;
	CHECK_EQ(TRUE, InitGameUpdate());
	ReleaseGameUpdate();
	CHECK(g_pCGameUpdate == NULL && g_pCWaitPacketUpdate == NULL && g_pCWaitUIUpdate == NULL);
}

//======================================================================
// InitSkillTree
//======================================================================

namespace {

// The skill manager InitSkillTree builds, over a blank skill table, with
// the domain experience file named (and written by the test) in a
// scratch directory; the start-up's failure messages recorded.
struct SkillTree
{
	TempDir dir;
	Properties files;
	MSkillInfoTable skillInfo;
	Globals globals;
	explicit SkillTree(bool named = true)
	{
		messages.clear();
		calls.clear();
		if (named) files.setProperty("FILE_INFO_SKILL_DOMAIN_EXP", dir / "domain.inf");
		globals.Lend(g_pFileDef, &files);
		globals.Lend(g_pSkillInfoTable, &skillInfo);
		globals.Own(g_pSkillManager, new MSkillManager);
		const GameInitInfoHost::Host* old = GameInitInfoHost::SetHost(&initInfoHost);
		globals.Then([old]() { GameInitInfoHost::SetHost(old); });
	}
	std::string Path() const { return files.getProperty("FILE_INFO_SKILL_DOMAIN_EXP"); }
	// The file: a count, then each record's domain, level, goal and total.
	void Domains(const std::vector<std::vector<int>>& records)
	{
		Bytes bytes;
		bytes.Int(std::int32_t(records.size()));
		for (const std::vector<int>& record : records)
			for (int value : record) bytes.Int(value);
		WriteBytes(Path(), bytes.data);
	}
};

// Each domain's root skill, which MSkillManager::Init plants in it.
struct Root { int domain; ACTIONINFO skill; };
const Root kRoots[] = {
	{ SKILLDOMAIN_BLADE, SKILL_SINGLE_BLOW }, { SKILLDOMAIN_SWORD, SKILL_DOUBLE_IMPACT },
	{ SKILLDOMAIN_GUN, SKILL_FAST_RELOAD }, { SKILLDOMAIN_ENCHANT, MAGIC_CREATE_HOLY_WATER },
	{ SKILLDOMAIN_HEAL, MAGIC_CURE_LIGHT_WOUNDS }, { SKILLDOMAIN_VAMPIRE, MAGIC_HIDE },
	{ SKILLDOMAIN_OUSTERS, SKILL_FLOURISH }, { SKILLDOMAIN_ETC, SKILL_SOUL_CHAIN },
};

bool TreeIsBuilt()
{
	if (g_pSkillManager == NULL || g_pSkillManager->GetSize() != MAX_SKILLDOMAIN) return false;
	for (const Root& root : kRoots)
		if ((*g_pSkillManager)[root.domain].GetSkillStatus(root.skill) == MSkillDomain::SKILLSTATUS_NULL) return false;
	return true;
}

} // namespace

TEST(GameInit3SkillTree, TheTreeGrowsFromEachDomainsRootSkill)
{
	SkillTree tree;
	tree.Domains({});
	CHECK_EQ(0, g_pSkillManager->GetSize());
	InitSkillTree();
	CHECK(TreeIsBuilt());
	CHECK(messages.empty());
}

TEST(GameInit3SkillTree, TheServersDomainExperienceIsReadFromItsFile)
{
	SkillTree tree;
	tree.Domains({ { SKILLDOMAIN_BLADE, 3, 30, 300 }, { SKILLDOMAIN_HEAL, 150, 7, 8 } });
	InitSkillTree();
	CHECK(TreeIsBuilt());
	CHECK(messages.empty());
	if (g_pSkillManager->GetSize() != MAX_SKILLDOMAIN) return;
	CHECK_EQ(30, (*g_pSkillManager)[SKILLDOMAIN_BLADE].GetExpInfo(3).GoalExp);
	CHECK_EQ(300, (*g_pSkillManager)[SKILLDOMAIN_BLADE].GetExpInfo(3).AccumExp);
	CHECK_EQ(7, (*g_pSkillManager)[SKILLDOMAIN_HEAL].GetExpInfo(150).GoalExp);
	CHECK_EQ(8, (*g_pSkillManager)[SKILLDOMAIN_HEAL].GetExpInfo(150).AccumExp);
}

TEST(GameInit3SkillTree, WithoutTheFileTheTreeStandsAndAStartUpFailureIsReported)
{
	// Recorded defect: InitSkillTree takes the file as optional
	// (GameInit.cpp:4236), but FileOpenBinary reports a missing file as a
	// start-up failure (GameInitInfo.cpp:166, GameInitInfoHost::
	// InitFailMessage), and the executable's answer marks the game
	// inactive, hides the game window and, on Windows, destroys it
	// (Client.cpp:378-395). Zone loading builds the tree again
	// (ZoneLoading::InitSkillTree), so a data tree without the file would
	// do that in play.
	SkillTree tree;
	InitSkillTree();
	CHECK(TreeIsBuilt());
	CHECK(messages == std::vector<std::string>({ FailOpen(tree.Path()) }));
}

TEST(GameInit3SkillTree, ATreeAlreadyGrownStartsAgain)
{
	SkillTree tree;
	tree.Domains({ { SKILLDOMAIN_GUN, 2, 20, 200 } });
	InitSkillTree();
	testfw::MutableRow(*g_pSkillManager, SKILLDOMAIN_SWORD).SetDomainLevel(9);
	tree.Domains({});
	InitSkillTree();
	CHECK(TreeIsBuilt());
	CHECK_EQ(0, (*g_pSkillManager)[SKILLDOMAIN_SWORD].GetDomainLevel());
	// The new domains' experience rows are their defaults again.
	CHECK_EQ(0, (*g_pSkillManager)[SKILLDOMAIN_GUN].GetExpInfo(2).GoalExp);
}

TEST(GameInit3SkillTree, WithoutTheFilesNameTheTreeIsBuiltAndTheLookupThrows)
{
	// FileDef.inf without FILE_INFO_SKILL_DOMAIN_EXP: Properties::getProperty
	// throws, out of InitSkillTree, once the tree is built.
	SkillTree tree(false);
	CHECK(Thrown([]() { InitSkillTree(); }) == "NoSuchElementException: FILE_INFO_SKILL_DOMAIN_EXP");
	CHECK(TreeIsBuilt());
	CHECK(messages.empty());
}

//======================================================================
// InitGameObject
//======================================================================

namespace {

//----------------------------------------------------------------------
// A host slot InitGameObject fills: installs a host and answers the one
// it replaced.
//----------------------------------------------------------------------
struct HostSlot
{
	const char* name;
	const void* (*install)(const void* host);
	std::size_t size;		// the host's, for the entries check
};

template <class Host, const Host* (*Set)(const Host*)>
const void* Install(const void* host) { return Set(static_cast<const Host*>(host)); }

const void* InstallItemHost(const void* host)
{
	const MItemHost* old = MItem::GetHost();
	MItem::SetHost(static_cast<const MItemHost*>(host));
	return old;
}

const void* InstallPriceHost(const void* host)
{
	const MPriceHost* old = PricePeek::Host();
	MPriceManager::SetHost(static_cast<const MPriceHost*>(host));
	return old;
}

const void* InstallWireHost(const void* host)
{
	const WireHost* old = *Get(WireHostOf());
	Wire::SetHost(static_cast<const WireHost*>(host));
	return old;
}

#define HOST_SLOT(setter) \
	{ #setter, &Install<std::remove_cv_t<std::remove_pointer_t<decltype(setter(nullptr))>>, &setter>, \
		sizeof(*setter(nullptr)) }

// In InitGameObject's order.
const HostSlot kHostSlots[] = {
	{ "MItem::SetHost", &InstallItemHost, sizeof(MItemHost) },
	{ "Wire::SetHost", &InstallWireHost, sizeof(WireHost) },
	{ "MPriceManager::SetHost", &InstallPriceHost, sizeof(MPriceHost) },
	HOST_SLOT(MChatManager::SetHost),
	HOST_SLOT(MWeather::SetHost),
	HOST_SLOT(MWarManager::SetHost),
	HOST_SLOT(MEffectTarget::SetHost),
	HOST_SLOT(MEffect::SetHost),
	HOST_SLOT(MHelicopter::SetHost),
	HOST_SLOT(ZONESOUND_NODE::SetHost),
	HOST_SLOT(MGuidanceEffect::SetHost),
	HOST_SLOT(MParabolaEffect::SetHost),
	HOST_SLOT(MAttachEffect::SetHost),
	HOST_SLOT(MScreenEffectManager::SetHost),
	HOST_SLOT(MStopInventoryEffectGenerator::SetHost),
	HOST_SLOT(MStopZoneEffectGenerator::SetHost),
	HOST_SLOT(MStopZoneCrossEffectGenerator::SetHost),
	HOST_SLOT(MStopZoneXEffectGenerator::SetHost),
	HOST_SLOT(MStopZoneRhombusEffectGenerator::SetHost),
	HOST_SLOT(MStopZoneEmptyCrossEffectGenerator::SetHost),
	HOST_SLOT(MStopZoneEmptyRectEffectGenerator::SetHost),
	HOST_SLOT(MStopZoneRectEffectGenerator::SetHost),
	HOST_SLOT(MStopZoneWallEffectGenerator::SetHost),
	HOST_SLOT(MStopZoneMultipleEffectGenerator::SetHost),
	HOST_SLOT(MStopZoneRandomEffectGenerator::SetHost),
	HOST_SLOT(MStopZoneSelectableEffectGenerator::SetHost),
	HOST_SLOT(MSpreadOutEffectGenerator::SetHost),
	HOST_SLOT(MAroundZoneEffectGenerator::SetHost),
	HOST_SLOT(MFollowPathEffectGenerator::SetHost),
	HOST_SLOT(MMeteorDropEffectGenerator::SetHost),
	HOST_SLOT(MAttackCreatureParabolaEffectGenerator::SetHost),
	HOST_SLOT(MAttackCreatureEffectGenerator::SetHost),
	HOST_SLOT(MAttackCreatureHomingEffectGenerator::SetHost),
	HOST_SLOT(MRippleZoneWideEffectGenerator::SetHost),
	HOST_SLOT(MBloodyBreakerEffectGenerator::SetHost),
	HOST_SLOT(MBloodyWallEffectGenerator::SetHost),
	HOST_SLOT(MBloodyWaveEffectGenerator::SetHost),
	HOST_SLOT(MRippleZoneEffectGenerator::SetHost),
	HOST_SLOT(MRippleZonePixelEffectGenerator::SetHost),
	HOST_SLOT(MAttachZoneEffectGenerator::SetHost),
	HOST_SLOT(MAttachZoneAroundEffectGenerator::SetHost),
	HOST_SLOT(MAttachZoneSelectableEffectGenerator::SetHost),
	HOST_SLOT(MAttackZoneRectEffectGenerator::SetHost),
	HOST_SLOT(MSkipEffectGenerator::SetHost),
	HOST_SLOT(RankBonusHandlers::SetHost),
	HOST_SLOT(RegenZoneStatus::SetHost),
	HOST_SLOT(BonusSkills::SetHost),
	HOST_SLOT(SkillDowngrade::SetHost),
	HOST_SLOT(GuildPresentation::SetHost),
	HOST_SLOT(CommunicationPresentation::SetHost),
	HOST_SLOT(QuestWarPresentation::SetHost),
	HOST_SLOT(NoticePresentation::SetHost),
	HOST_SLOT(StatusPresentation::SetHost),
	HOST_SLOT(GameOptions::SetHost),
	HOST_SLOT(PlayerPresentation::SetHost),
	HOST_SLOT(NPCPresentation::SetHost),
	HOST_SLOT(ZoneLoading::SetHost),
	HOST_SLOT(TradePresentation::SetHost),
	HOST_SLOT(GameInfoPresentation::SetHost),
	HOST_SLOT(DialogPresentation::SetHost),
	HOST_SLOT(DialogWindow::SetHost),
	HOST_SLOT(LoginPresentation::SetHost),
	HOST_SLOT(PlayerState::SetHost),
	HOST_SLOT(ZoneState::SetHost),
	HOST_SLOT(CreatureState::SetHost),
	HOST_SLOT(WorldEvents::SetHost),
	HOST_SLOT(VsUiPresentation::SetHost),
	HOST_SLOT(LoginLists::SetHost),
	HOST_SLOT(MStopZoneEmptyHorizontalWallEffectGenerator::SetHost),
	HOST_SLOT(MStopZoneEmptyVerticalWallEffectGenerator::SetHost),
	HOST_SLOT(MFallingEffectGenerator::SetHost),
	HOST_SLOT(MAttackZoneEffectGenerator::SetHost),
	HOST_SLOT(MFirePiecingEffectGenerator::SetHost),
	HOST_SLOT(MPlayingWithFireEffectGenerator::SetHost),
	HOST_SLOT(MAttackZoneBombEffectGenerator::SetHost),
	HOST_SLOT(MAttackZoneParabolaEffectGenerator::SetHost),
	HOST_SLOT(MMultipleFallingEffectGenerator::SetHost),
	HOST_SLOT(MRisingEffectGenerator::SetHost),
	HOST_SLOT(MParty::SetHost),
	HOST_SLOT(UiRuntime::SetHost),
	HOST_SLOT(GameUI::SetHost),
	HOST_SLOT(UIMessages::SetHost),
	HOST_SLOT(SetGameMainHost),
};

// What each slot holds now (each read puts the same host back).
std::map<std::string, const void*> InstalledHosts()
{
	std::map<std::string, const void*> hosts;
	for (const HostSlot& slot : kHostSlots)
	{
		const void* host = slot.install(nullptr);
		slot.install(host);
		hosts[slot.name] = host;
	}
	return hosts;
}

// Creature types: the player's slayer (0), an NPC (1, its sprite an NPC's)
// and a vampire (2, its sprite one only players wear).
enum { kSlayer = 0, kNPC = 1, kVampire = 2, kCreatureTypes = 3 };

// The game objects' world: the tables a creature reads, a zone (not the
// current one) of the zone table's zone 61, the managers the object
// selector reads, an input device, and FileDef naming the files in a
// scratch directory: the version file, the domain experience file (no
// records), the data pack (absent) and the filter file beside it (absent
// unless written). Every object InitGameObject makes starts absent but
// the user information; every host slot it fills starts empty. All of it
// is deleted or put back after.
struct World
{
	TempDir dir;
	Properties files;
	ClientConfig config;
	CREATURE_TABLE creatureTypes;
	CREATURESPRITE_TABLE sprites;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	MSkillInfoTable skillInfo;
	CZoneTable zoneTable;
	MJusticeAttackManager justice;
	MWarManager wars;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	RankBonusTable rankBonuses;
	CSDLInput input;
	std::unique_ptr<MZone> zone;
	Globals globals;

	explicit World(const std::string& unnamed = "")
	{
		static const bool moveTables = (MCreature::InitMoveTable(), true);
		(void)moveTables;
		messages.clear();
		calls.clear();
		deleted.clear();
		creatureTypes.Init(kCreatureTypes);
		testfw::MutableRow(creatureTypes, kSlayer).SetCreatureTribe(CREATURETRIBE_SLAYER, ACTION_MAX_SLAYER);
		testfw::MutableRow(creatureTypes, kSlayer).ColorSet = 321;
		testfw::MutableRow(creatureTypes, kNPC).SetCreatureTribe(CREATURETRIBE_NPC, ACTION_MAX_SLAYER);
		testfw::MutableRow(creatureTypes, kVampire).SetCreatureTribe(CREATURETRIBE_VAMPIRE, ACTION_MAX_VAMPIRE);
		for (int type : { kNPC, kVampire })
		{
			testfw::MutableRow(creatureTypes, type).SpriteTypes.Init(1);
			testfw::MutableRow(creatureTypes, type).SpriteTypes.Set(0, type);
		}
		sprites.Init(kCreatureTypes);
		testfw::MutableRow(sprites, kNPC).CreatureType = FLAG_CREATURESPRITE_NPC_ALL;
		testfw::MutableRow(sprites, kVampire).CreatureType = FLAG_CREATURESPRITE_PLAYER_VAMPIRE;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		effectStatuses.Init(EFFECTSTATUS_MAX);
		ZONETABLE_INFO* pZoneInfo = new ZONETABLE_INFO;
		pZoneInfo->ID = 61;
		pZoneInfo->PKType = 2;
		zoneTable.Add(pZoneInfo);

		globals.Lend(g_pClientConfig, &config);
		globals.Lend(g_pCreatureTable, &creatureTypes);
		globals.Lend(g_pCreatureSpriteTable, &sprites);
		globals.Lend(g_pEffectSpriteTypeTable, &effectSprites);
		globals.Lend(g_pEffectStatusTable, &effectStatuses);
		globals.Lend(g_pSkillInfoTable, &skillInfo);
		globals.Lend(g_pZoneTable, &zoneTable);
		globals.Lend(g_pJusticeAttackManager, &justice);
		globals.Lend(g_pWarManager, &wars);
		globals.Lend(g_pMonsterNameTable, &monsterNames);
		globals.Lend(g_pLevelNameTable, &levelNames);
		globals.Lend(g_pRankBonusTable, &rankBonuses);
		globals.Lend(g_pSDLInput, &input);
		// A zone made without FileDef reads no horn file.
		globals.Lend(g_pFileDef, static_cast<Properties*>(NULL));
		zone.reset(new MZone(8, 8));
		zone->SetCurrentZoneID(61);
		g_pFileDef = &files;
		globals.Lend(g_pZone, static_cast<MZone*>(NULL));
		globals.Keep(g_Mode);
		globals.Then([]() { SetHelpHost(NULL); });

		const char* keys[][2] = {
			{ "FILE_INFO_DATA", "info.rpk" }, { "FILE_FILTER_INFO", "" }, { "FILE_INFO_VERSION", "version.inf" },
			{ "FILE_INFO_SKILL_DOMAIN_EXP", "domain.inf" },
		};
		for (const auto& key : keys)
			if (unnamed != key[0])
				files.setProperty(key[0], key[1][0] != '\0' ? dir / key[1] : std::string("filter.txt"));
		WriteBytes(dir / "version.inf", Bytes().Int(7).data);
		WriteBytes(dir / "domain.inf", Bytes().Int(0).data);

		globals.Own(g_pUserInformation, new UserInformation);
		globals.Own(g_pSystemAvailableManager);
		globals.Own(g_pServerInformation);
		globals.Own(g_pTempInformation);
		globals.Own(g_pSkillManager);
		globals.Own(g_pSkillAvailable);
		globals.Own(g_pSlayerGear);
		globals.Own(g_pVampireGear);
		globals.Own(g_pOustersGear);
		globals.Own(g_pInventory);
		globals.Own(g_pParty);
		globals.Own(g_pGameTime);
		globals.Own(g_pPriceManager);
		globals.Own(g_pMoneyManager);
		globals.Own(g_pPCTalkBox);
		globals.Own(g_pNPCTalkBox);
		globals.Own(g_pUIMessageManager);
		globals.Own(g_pObjectSelector);
		globals.Own(g_pWeather);
		globals.Own(g_pInventoryEffectManager);
		globals.Own(g_pPlayer);

		// Put back before anything is deleted.
		for (const HostSlot& slot : kHostSlots)
		{
			const void* old = slot.install(nullptr);
			globals.Then([&slot, old]() { slot.install(old); });
		}
		const GameInitInfoHost::Host* oldInfo = GameInitInfoHost::SetHost(&initInfoHost);
		globals.Then([oldInfo]() { GameInitInfoHost::SetHost(oldInfo); });
	}
	std::string Path(const char* key) const { return files.getProperty(key); }
};

// UIMessageManager's table of what each UI message runs.
struct UIMessagesPeek : UIMessageManager
{
	static bool Initialised(const UIMessageManager& manager)
	{
		return (manager.*(&UIMessagesPeek::m_UIMessageFunction))[UI_STORAGE_BUY] == &UIMessagesPeek::Execute_UI_STORAGE_BUY;
	}
};

std::vector<int> mails;
const MHelpHost helpHost{ .AddHelpMail = [](HELP_EVENT he) { mails.push_back(int(he)); } };

} // namespace

TEST(GameInit3GameObject, TheGameObjectsItLacksAreMade)
{
	World world;
	g_pUserInformation->GameVersion = 1;
	CHECK_EQ(TRUE, InitGameObject());
	CHECK(messages.empty());
	CHECK(calls.empty());
	CHECK(g_pSystemAvailableManager != NULL);
	CHECK(g_pTempInformation != NULL);
	CHECK(g_pSkillAvailable != NULL);
	CHECK(g_pSlayerGear != NULL);
	CHECK(g_pVampireGear != NULL);
	CHECK(g_pOustersGear != NULL);
	CHECK(g_pInventory != NULL);
	CHECK(g_pGameTime != NULL);
	CHECK(g_pPriceManager != NULL);
	CHECK(g_pPCTalkBox != NULL);
	CHECK(g_pNPCTalkBox != NULL);
	// The server names the login screens show until the lists come.
	CHECK(g_pServerInformation != NULL
		&& std::string(g_pServerInformation->GetServerGroupName()) == "ServerGroup"
		&& std::string(g_pServerInformation->GetServerName()) == "Server");
	// The version the client reports, whatever the version file says.
	CHECK_EQ(7457757, g_pUserInformation->GameVersion);
	// An empty party, a still sky, no inventory effects.
	CHECK(g_pParty != NULL && g_pParty->GetSize() == 0);
	CHECK(g_pWeather != NULL && !g_pWeather->IsActive());
	CHECK(g_pInventoryEffectManager != NULL && g_pInventoryEffectManager->GetSize() == 0);
	// The UI's messages are bound, and selection has its host.
	CHECK(g_pUIMessageManager != NULL && UIMessagesPeek::Initialised(*g_pUIMessageManager));
	CHECK(g_pObjectSelector != NULL && SelectorPeek::Host(*g_pObjectSelector) != NULL);
	// A slayer man, set from his creature type's row.
	CHECK(g_pPlayer != NULL && g_pPlayer->GetCreatureType() == kSlayer && g_pPlayer->GetBodyColor1() == 321);
	// The skill tree is built last.
	CHECK(TreeIsBuilt());
}

TEST(GameInit3GameObject, TheWalletSuggestsTheStorageBoxThroughTheHelp)
{
	World world;
	mails.clear();
	SetHelpHost(&helpHost);
	g_Mode = MODE_GAME;
	CHECK_EQ(TRUE, InitGameObject());
	if (g_pMoneyManager == NULL) { CHECK(g_pMoneyManager != NULL); return; }
	CHECK(g_pMoneyManager->SetMoney(100001));
	CHECK(mails == std::vector<int>({ HELP_EVENT_STORAGE_BUY }));
}

TEST(GameInit3GameObject, WhatIsThereIsKept)
{
	World world;
	mails.clear();
	SetHelpHost(&helpHost);
	g_Mode = MODE_GAME;
	// Each made by hand: the server names, the wallet without its hint,
	// the UI messages unbound, the selector without a host, the party
	// with a member, the availabilities with nothing open.
	SystemAvailabilitiesManager* systems = g_pSystemAvailableManager = new SystemAvailabilitiesManager;
	systems->SetFlag(0);
	CServerInformation* servers = g_pServerInformation = new CServerInformation;
	servers->SetServerGroupName("Bathory");
	servers->SetServerName("Elena");
	UserInformation* user = g_pUserInformation;
	TempInformation* temp = g_pTempInformation = new TempInformation;
	MSkillManager* skills = g_pSkillManager = new MSkillManager;
	MSkillSet* available = g_pSkillAvailable = new MSkillSet;
	MSlayerGear* slayerGear = g_pSlayerGear = new MSlayerGear;
	MVampireGear* vampireGear = g_pVampireGear = new MVampireGear;
	MOustersGear* oustersGear = g_pOustersGear = new MOustersGear;
	MInventory* inventory = g_pInventory = new MInventory;
	MParty* party = g_pParty = new MParty;
	PARTY_INFO* member = new PARTY_INFO;
	member->Name = "Ally";
	member->ID = 5;
	CHECK(party->AddMember(member));
	MGameTime* gameTime = g_pGameTime = new MGameTime;
	MPriceManager* prices = g_pPriceManager = new MPriceManager;
	MMoneyManager* wallet = g_pMoneyManager = new MMoneyManager;
	PCTalkBox* pcTalk = g_pPCTalkBox = new PCTalkBox;
	MStringList* npcTalk = g_pNPCTalkBox = new MStringList;
	UIMessageManager* uiMessages = g_pUIMessageManager = new UIMessageManager;
	MObjectSelector* selector = g_pObjectSelector = new MObjectSelector;
	WriteBytes(world.dir / "filter.txt", "*0 1\n7 3\n");

	CHECK_EQ(TRUE, InitGameObject());
	CHECK(g_pSystemAvailableManager == systems);
	CHECK(g_pServerInformation == servers);
	CHECK(g_pUserInformation == user);
	CHECK(g_pTempInformation == temp);
	CHECK(g_pSkillManager == skills);
	CHECK(g_pSkillAvailable == available);
	CHECK(g_pSlayerGear == slayerGear);
	CHECK(g_pVampireGear == vampireGear);
	CHECK(g_pOustersGear == oustersGear);
	CHECK(g_pInventory == inventory);
	CHECK(g_pParty == party);
	CHECK(g_pGameTime == gameTime);
	CHECK(g_pPriceManager == prices);
	CHECK(g_pMoneyManager == wallet);
	CHECK(g_pPCTalkBox == pcTalk);
	CHECK(g_pNPCTalkBox == npcTalk);
	CHECK(g_pUIMessageManager == uiMessages);
	CHECK(g_pObjectSelector == selector);
	// Untouched but for the party, which is emptied.
	CHECK_EQ(0, party->GetSize());
	CHECK(std::string(servers->GetServerGroupName()) == "Bathory" && std::string(servers->GetServerName()) == "Elena");
	CHECK(!UIMessagesPeek::Initialised(*uiMessages));
	CHECK(SelectorPeek::Host(*selector) == NULL);
	CHECK(wallet->SetMoney(100001));
	CHECK(mails.empty());
	// The filter file is not read for a manager already there.
	CHECK(systems->ScriptFiltering(7, 2));
	// The version is set and the tree built again all the same.
	CHECK_EQ(7457757, user->GameVersion);
	CHECK(TreeIsBuilt());
}

TEST(GameInit3GameObject, TheWeatherTheInventoryEffectsAndThePlayerAreAlwaysNew)
{
	World world;
	// A rainy sky (the rain needs its origin from a host).
	MWeatherHost sky{};
	sky.ReadOrigin = [](int& x, int& y) { x = y = 0; return true; };
	g_pWeather = new MWeather;
	const MWeatherHost* noSky = MWeather::SetHost(&sky);
	g_pWeather->SetRain(3);
	MWeather::SetHost(noSky);
	CHECK(g_pWeather->IsActive());
	g_pInventoryEffectManager = new EffectsProbe;
	g_pPlayer = new PlayerProbe;
	CHECK_EQ(TRUE, InitGameObject());
	CHECK_EQ(1, deleted["inventory effects"]);
	CHECK_EQ(1, deleted["player"]);
	// The new ones: a still sky, and a slayer man.
	CHECK(g_pWeather != NULL && !g_pWeather->IsActive());
	CHECK(g_pInventoryEffectManager != NULL);
	CHECK(g_pPlayer != NULL && g_pPlayer->GetBodyColor1() == 321);
}

TEST(GameInit3GameObject, ThePlayerLeavesItsZoneBeforeItIsDeleted)
{
	World world;
	// The player in the current zone, as MZone::SetPlayer adds it.
	PlayerProbe* old = new PlayerProbe;
	old->SetID(77);
	old->SetPosition(3, 4);
	g_pPlayer = old;
	g_pZone = world.zone.get();
	old->SetZone(g_pZone);
	CHECK(g_pZone->AddCreature(old));
	CHECK(g_pZone->GetCreature(77) != NULL);
	CHECK_EQ(TRUE, InitGameObject());
	CHECK_EQ(1, deleted["player"]);
	CHECK(world.zone->GetCreature(77) == NULL);
	// The new player is not put into the zone.
	CHECK_EQ(0, int(world.zone->GetCreatureNumber()));
}

TEST(GameInit3GameObject, WithoutTheVersionFileItFailsBeforeTheRestIsMade)
{
	World world;
	g_pUserInformation->GameVersion = 1;
	std::filesystem::remove(world.dir / "version.inf");
	CHECK_EQ(FALSE, InitGameObject());
	CHECK(messages == std::vector<std::string>({ FailOpen(world.Path("FILE_INFO_VERSION")) }));
	// What comes before the version is made.
	CHECK(g_pSystemAvailableManager != NULL);
	CHECK(g_pServerInformation != NULL);
	// Nothing after it: no game object, no host, no player, no tree.
	CHECK_EQ(1, int(g_pUserInformation->GameVersion));
	CHECK(g_pTempInformation == NULL && g_pSkillManager == NULL && g_pSkillAvailable == NULL);
	CHECK(g_pSlayerGear == NULL && g_pVampireGear == NULL && g_pOustersGear == NULL && g_pInventory == NULL);
	CHECK(g_pParty == NULL && g_pGameTime == NULL && g_pPriceManager == NULL && g_pMoneyManager == NULL);
	CHECK(g_pPCTalkBox == NULL && g_pNPCTalkBox == NULL && g_pUIMessageManager == NULL && g_pObjectSelector == NULL);
	CHECK(g_pWeather == NULL && g_pInventoryEffectManager == NULL && g_pPlayer == NULL);
	int installed = 0;
	for (const auto& host : InstalledHosts()) installed += host.second != NULL ? 1 : 0;
	CHECK_EQ(0, installed);
}

TEST(GameInit3GameObject, WithoutTheUserInformationItIsMade)
{
	World world;
	delete g_pUserInformation;
	g_pUserInformation = NULL;
	CHECK_EQ(TRUE, InitGameObject());
	CHECK(g_pUserInformation != NULL);
	if (g_pUserInformation != NULL) CHECK_EQ(7457757, g_pUserInformation->GameVersion);
}

TEST(GameInit3GameObject, TheVersionFilesContentIsNotRead)
{
	// Recorded defect: the version file must be there, but its four bytes
	// (an empty file: none) are read into a local nothing reads
	// (GameInit.cpp:3957-3963); the version reported is the literal
	// 7457757 (GameInit.cpp:3970).
	World world;
	WriteBytes(world.dir / "version.inf", "");
	CHECK_EQ(TRUE, InitGameObject());
	CHECK(messages.empty());
	CHECK_EQ(7457757, g_pUserInformation->GameVersion);
}

TEST(GameInit3GameObject, ANameMissingFromFileDefThrows)
{
	// Properties::getProperty throws for a key FileDef.inf lacks; each
	// escapes InitGameObject where it is first read.
	struct Case { const char* key; bool playerMade; };
	const Case cases[] = {
		{ "FILE_FILTER_INFO", false }, { "FILE_INFO_DATA", false }, { "FILE_INFO_VERSION", false },
		// The last: everything is made and installed, then InitSkillTree throws.
		{ "FILE_INFO_SKILL_DOMAIN_EXP", true },
	};
	for (const Case& c : cases)
	{
		World world(c.key);
		CHECK(Thrown([]() { InitGameObject(); }) == std::string("NoSuchElementException: ") + c.key);
		// The availabilities manager is made before its file is named.
		CHECK(g_pSystemAvailableManager != NULL);
		CHECK((g_pPlayer != NULL) == c.playerMade);
		CHECK((InstalledHosts()["SetGameMainHost"] != NULL) == c.playerMade);
		CHECK(messages.empty());
	}
}

TEST(GameInit3GameObject, AnAvailabilitiesManagerThereNeedsNoFilterOrData)
{
	// The two names are read only to load a new manager.
	for (const char* key : { "FILE_FILTER_INFO", "FILE_INFO_DATA" })
	{
		World world(key);
		g_pSystemAvailableManager = new SystemAvailabilitiesManager;
		CHECK_EQ(TRUE, InitGameObject());
	}
}

TEST(GameInit3GameObject, TheSystemAvailabilitiesAreReadFromTheFilterFileBesideTheDataPack)
{
	// The pack is absent: the loose file beside it is read, and its
	// lines go to SystemAvailabilitiesManager::LoadFromStream.
	World world;
	WriteBytes(world.dir / "filter.txt", "; the party's script 7, answer 3\r\n*0 1\r\n7 3\r\n");
	CHECK_EQ(TRUE, InitGameObject());
	if (g_pSystemAvailableManager == NULL) { CHECK(g_pSystemAvailableManager != NULL); return; }
	// Every system is open by default; with all closed the filter shows.
	CHECK(g_pSystemAvailableManager->IsAvailablePartySystem());
	g_pSystemAvailableManager->SetFlag(0);
	CHECK(!g_pSystemAvailableManager->ScriptFiltering(7, 2));
	CHECK(g_pSystemAvailableManager->ScriptFiltering(7, 3));
	CHECK(g_pSystemAvailableManager->ScriptFiltering(8, 2));
}

TEST(GameInit3GameObject, AFilterLinesFirst511BytesAreRead)
{
	// A filter of two scripts whose count ends the header at byte 511 is
	// read whole; one byte later the count is cut off, so the header keeps
	// the parser's -1 and the filter closes after its first script.
	for (int length : { 511, 512 })
	{
		World world;
		const std::string header = "*0" + std::string(std::size_t(length - 3), ' ') + "2";
		WriteBytes(world.dir / "filter.txt", header + "\n7 3\n8 4\n");
		CHECK_EQ(TRUE, InitGameObject());
		if (g_pSystemAvailableManager == NULL) { CHECK(g_pSystemAvailableManager != NULL); return; }
		g_pSystemAvailableManager->SetFlag(0);
		CHECK(!g_pSystemAvailableManager->ScriptFiltering(7, 2));
		CHECK_EQ(length == 512, g_pSystemAvailableManager->ScriptFiltering(8, 3));
	}
}

TEST(GameInit3GameObject, WithoutTheFilterFileTheSystemsKeepTheirDefaults)
{
	World world;
	CHECK_EQ(TRUE, InitGameObject());
	CHECK(messages.empty());
	if (g_pSystemAvailableManager == NULL) { CHECK(g_pSystemAvailableManager != NULL); return; }
	CHECK(g_pSystemAvailableManager->IsAvailablePartySystem() && g_pSystemAvailableManager->IsAvailableGuildSystem());
	g_pSystemAvailableManager->SetFlag(0);
	CHECK(g_pSystemAvailableManager->ScriptFiltering(7, 2));
}

namespace {

// InitGameObject again, with the skill tree dropped first: with a tree
// built, the new player's SetCreatureType reaches the UI (see the
// undefined behaviour at the top).
BOOL InitGameObjectAgain()
{
	delete g_pSkillManager;
	g_pSkillManager = NULL;
	return InitGameObject();
}

// The host's words that are empty (every host here is a table of
// pointers, so each word is one entry).
std::vector<int> EmptyEntries(const void* host, std::size_t size)
{
	std::vector<int> empty;
	for (std::size_t i = 0; i < size / sizeof(void*); ++i)
	{
		const void* entry = NULL;
		std::memcpy(&entry, static_cast<const unsigned char*>(host) + i * sizeof(void*), sizeof(entry));
		if (entry == NULL) empty.push_back(int(i));
	}
	return empty;
}

} // namespace

TEST(GameInit3GameObject, EveryHostSlotGetsAFullTableOfGameInits)
{
	World world;
	CHECK_EQ(TRUE, InitGameObject());
	std::map<std::string, const void*> hosts = InstalledHosts();
	CHECK_EQ(83, int(hosts.size()));
	for (const HostSlot& slot : kHostSlots)
	{
		const void* host = hosts[slot.name];
		const bool installed = host != NULL;
		if (!installed) std::fprintf(stderr, "    %s: no host\n", slot.name);
		CHECK(installed);
		if (!installed) continue;
		// Every entry is filled but one: ZoneLoading's SetMyBatColorSet
		// (word 23), whose installer went with the global it stored into,
		// which nothing read (commit 93ec63db); the call is a no-op.
		const bool zoneLoading = std::string(slot.name) == "ZoneLoading::SetHost";
		CHECK_EQ(0, int(slot.size % sizeof(void*)));
		const std::vector<int> empty = EmptyEntries(host, slot.size);
		const bool full = empty == (zoneLoading ? std::vector<int>({ 23 }) : std::vector<int>());
		if (!full) std::fprintf(stderr, "    %s: %zu empty entries\n", slot.name, empty.size());
		CHECK(full);
	}
	CHECK_EQ(26, int(sizeof(ZoneLoading::Host) / sizeof(void*)));
	CHECK(offsetof(ZoneLoading::Host, SetMyBatColorSet) == 23 * sizeof(void*));

	// The generators that place the same way share one table.
	const std::vector<std::vector<const char*>> shared = {
		{ "MStopZoneXEffectGenerator::SetHost", "MStopZoneRhombusEffectGenerator::SetHost",
			"MStopZoneEmptyCrossEffectGenerator::SetHost", "MStopZoneRandomEffectGenerator::SetHost",
			"MSpreadOutEffectGenerator::SetHost", "MFollowPathEffectGenerator::SetHost" },
		{ "MAttackCreatureParabolaEffectGenerator::SetHost", "MAttackCreatureEffectGenerator::SetHost",
			"MAttackCreatureHomingEffectGenerator::SetHost" },
		{ "MRippleZoneEffectGenerator::SetHost", "MRippleZonePixelEffectGenerator::SetHost" },
		{ "MAttachZoneEffectGenerator::SetHost", "MAttachZoneSelectableEffectGenerator::SetHost",
			"MAttackZoneRectEffectGenerator::SetHost", "MSkipEffectGenerator::SetHost" },
		{ "MStopZoneEmptyHorizontalWallEffectGenerator::SetHost", "MStopZoneEmptyVerticalWallEffectGenerator::SetHost" },
		{ "MAttackZoneEffectGenerator::SetHost", "MFirePiecingEffectGenerator::SetHost",
			"MPlayingWithFireEffectGenerator::SetHost" },
	};
	for (const std::vector<const char*>& group : shared)
		for (const char* name : group)
			CHECK(hosts[name] == hosts[group[0]]);

	// GameMain's are GameInit's own steps.
	const GameMainHost* gameMain = static_cast<const GameMainHost*>(hosts["SetGameMainHost"]);
	if (gameMain == NULL) return;
	CHECK(gameMain->InitDraw == &InitDraw);
	CHECK(gameMain->InitSurface == &InitSurface);
	CHECK(gameMain->InitSound == &InitSound);
	CHECK(gameMain->ReleaseSocket == &ReleaseSocket);

	// Again: the same tables (each is a static of GameInit.cpp's).
	CHECK_EQ(TRUE, InitGameObjectAgain());
	CHECK(InstalledHosts() == hosts);
}

TEST(GameInit3GameObject, TheObjectSelectorsHostIsMadeOnce)
{
	World world;
	CHECK_EQ(TRUE, InitGameObject());
	if (g_pObjectSelector == NULL) { CHECK(g_pObjectSelector != NULL); return; }
	const MObjectSelectorHost* first = SelectorPeek::Host(*g_pObjectSelector);
	delete g_pObjectSelector;
	g_pObjectSelector = NULL;
	CHECK_EQ(TRUE, InitGameObjectAgain());
	CHECK(g_pObjectSelector != NULL && SelectorPeek::Host(*g_pObjectSelector) == first);
}

TEST(GameInit3GameObject, TheObjectSelectorAsksThePlayerTheCreatureAndTheWorld)
{
	World world;
	CHECK_EQ(TRUE, InitGameObject());
	if (g_pObjectSelector == NULL || g_pPlayer == NULL) { CHECK(g_pObjectSelector != NULL && g_pPlayer != NULL); return; }
	const MObjectSelectorHost* host = SelectorPeek::Host(*g_pObjectSelector);
	if (host == NULL) { CHECK(host != NULL); return; }
	MPlayer& me = *g_pPlayer;
	MCreature npc;
	npc.SetCreatureType(kNPC);
	npc.SetName("Merchant");
	npc.SetGuildNumber(41);
	npc.SetUnionGuildID(42);
	MCreature vampire;
	vampire.SetCreatureType(kVampire);

	// Left shift held forces an attack.
	BOOL* keys = world.input.*Get(KeysOf());
	CHECK(!host->ForceAttack());
	keys[DIK_LSHIFT] = TRUE;
	CHECK(host->ForceAttack());
	keys[DIK_LSHIFT] = FALSE;

	// The player.
	CHECK(!host->PlayerHasEffectStatus(EFFECTSTATUS_COMA));
	CreaturePeek::EffectStatus(me)[EFFECTSTATUS_COMA] = true;
	CHECK(host->PlayerHasEffectStatus(EFFECTSTATUS_COMA));
	CHECK(!host->PlayerHasEffectStatus(EFFECTSTATUS_POISON));
	me.SetAttackModePeace();
	CHECK(!host->PlayerCanAttackTribe(&vampire));
	CHECK(!host->PlayerCanAttackGuild(&npc));
	me.SetAttackModeAggress();
	CHECK(host->PlayerCanAttackTribe(&vampire));
	CHECK(host->PlayerCanAttackGuild(&npc));
	CHECK_EQ(kSlayer, host->PlayerCreatureType());
	me.SetGuildNumber(77);
	me.SetUnionGuildID(88);
	CHECK_EQ(77, host->PlayerGuildNumber());
	CHECK_EQ(88, host->PlayerUnionGuildID());
	g_pUserInformation->IsNonPK = true;
	CHECK(host->IsNonPK());
	g_pUserInformation->IsNonPK = false;
	CHECK(!host->IsNonPK());

	// A creature.
	CHECK(std::string(host->GetName(&npc)) == "Merchant");
	CHECK(host->IsNPC(&npc));
	CHECK(!host->IsNPC(&vampire));
	CHECK_EQ(41, host->GetGuildNumber(&npc));
	CHECK_EQ(42, host->GetUnionGuildID(&npc));
	CHECK(!host->HasEffectStatus(&npc, EFFECTSTATUS_POISON));
	CreaturePeek::EffectStatus(npc)[EFFECTSTATUS_POISON] = true;
	CHECK(host->HasEffectStatus(&npc, EFFECTSTATUS_POISON));
	// Readable statuses: a creature object with its status array.
	CHECK(host->HasAvailableEffectStatus(&npc));
	BYTE& objectType = CreaturePeek::ObjectType(npc);
	objectType = MObject::TYPE_ITEM;
	CHECK(!host->HasAvailableEffectStatus(&npc));
	objectType = MObject::TYPE_CREATURE;
	bool* statuses = CreaturePeek::EffectStatus(npc);
	CreaturePeek::EffectStatus(npc) = NULL;
	CHECK(!host->HasAvailableEffectStatus(&npc));
	CreaturePeek::EffectStatus(npc) = statuses;
	// The type's first sprite: the vampire's only players wear.
	CHECK(host->HasPlayerOnlySprite(&vampire));
	CHECK(!host->HasPlayerOnlySprite(&npc));

	// The world.
	CHECK(!host->HasZone());
	g_pZone = world.zone.get();
	CHECK(host->HasZone());
	CHECK_EQ(61, host->GetZoneID());
	CHECK_EQ(2, host->GetZonePKType());
	CHECK(host->JusticeAttackManager() == &world.justice);
	CHECK(host->WarManager() == &world.wars);
}

//======================================================================
// InitGame
//======================================================================

namespace {

// The keys InitGame and InitInfomation (as far as it gets) read, and the
// files they name, relative to the working directory.
const char* const kFileDef[][2] = {
	{ "FILE_INFO_NICKNAME_STRING", "Data/Info/NickName.inf" },
	{ "FILE_INFO_STRING", "Data/Info/String.inf" },
	{ "FILE_INFO_CLIENTCONFIG", "Data/Info/ClientConfig.inf" },
	{ "FILE_INFO_USEROPTION", "Data/Info/UserOption.inf" },
	{ "FILE_INFO_GUILD_INFO_MAPPER", "Data/Info/GuildInfo.inf" },
	{ "FILE_SPRITE_GUILD_MARK", "Data/Image/GuildMark.spk" },
	{ "FILE_SPRITE_GRADE_MARK_SLAYER", "Data/Image/GradeSlayer.spk" },
	{ "FILE_SPRITE_GRADE_MARK_VAMPIRE", "Data/Image/GradeVampire.spk" },
	{ "FILE_SPRITE_GRADE_MARK_OUSTERS", "Data/Image/GradeOusters.spk" },
	{ "FILE_SPRITE_LEVEL_MARK_PET", "Data/Image/LevelPet.spk" },
	{ "FILE_INFO_DATA", "Data/Info/infodata.rpk" },
	{ "FILE_REGEN_TOWER_INFO", "RegenTower.inf" },
};

// The start-up in a scratch working directory, with the file definitions
// ClientMain made already in g_pFileDef (naming a nickname table of their
// own), the resource encoding UTF-8, no input device and an empty input host,
// and every object InitGame and InitInfomation make absent. The logging
// InitGame starts is stopped again if it was off; everything else is
// deleted or put back.
struct StartUp
{
	TempDir dir;
	Properties previous;
	Globals globals;

	StartUp()
	{
		messages.clear();
		calls.clear();
		const bool logging = log_is_enabled(LOG_LEVEL_ERROR);
		globals.Then([logging]() {
			log_init();
			log_set_file_output(nullptr);
			log_set_console_output(true);
			if (!logging) log_cleanup();
		});
		const std::filesystem::path cwd = std::filesystem::current_path();
		std::filesystem::current_path(dir.path);
		globals.Then([cwd]() { std::error_code error; std::filesystem::current_path(cwd, error); });
		previous.setProperty("FILE_INFO_NICKNAME_STRING", "Data/Info/Previous.inf");
		Properties* const oldFileDef = g_pFileDef;
		g_pFileDef = &previous;
		globals.Then([this, oldFileDef]() {
			if (g_pFileDef != &previous) delete g_pFileDef;
			g_pFileDef = oldFileDef;
		});
		// The trigonometry and orbit tables InitGame builds; FCreateSines
		// also doubles the arctangent table in place, so all are put back.
		std::vector<int> tables(MathTable::FSinTab, MathTable::FSinTab + MathTable::MAX_ANGLE);
		tables.insert(tables.end(), MathTable::FCosTab, MathTable::FCosTab + MathTable::MAX_ANGLE);
		tables.insert(tables.end(), MathTable::FArcTanTab, MathTable::FArcTanTab + MathTable::MAX_ANGLE + 1);
		POINT (&orbits)[EffectOrbit::TypeCount][EffectOrbit::StepCount] = *Get(OrbitPositionsOf());
		const std::vector<POINT> positions(&orbits[0][0], &orbits[0][0] + EffectOrbit::TypeCount * EffectOrbit::StepCount);
		globals.Then([tables, positions]() {
			std::copy_n(tables.begin(), MathTable::MAX_ANGLE, MathTable::FSinTab);
			std::copy_n(tables.begin() + MathTable::MAX_ANGLE, MathTable::MAX_ANGLE, MathTable::FCosTab);
			std::copy_n(tables.begin() + 2 * MathTable::MAX_ANGLE, MathTable::MAX_ANGLE + 1, MathTable::FArcTanTab);
			std::copy(positions.begin(), positions.end(), &(*Get(OrbitPositionsOf()))[0][0]);
		});
		const TextEncoding::Encoding encoding = TextEncoding::GetResourceEncoding();
		TextEncoding::SetResourceEncoding(TextEncoding::Encoding::Utf8);
		globals.Then([encoding]() { TextEncoding::SetResourceEncoding(encoding); });

		globals.Own(g_pGameStringTable);
		globals.Own(g_pSystemMessage);
		globals.Own(g_pPlayerMessage);
		globals.Own(g_pGameMessage);
		globals.Own(g_pHelpMessage);
		globals.Own(g_pNoticeMessage);
		globals.Own(g_pNickNameStringTable);
		globals.Own(g_pClientConfig);
		globals.Own(g_pUserOption);
		globals.Own(g_pRegenTowerInfoManager);
		globals.Own(g_pQuestInfoManager);
		globals.Own(g_pTimeItemManager);
		globals.Own(g_pFameInfoTable);
		globals.Own(g_pWarManager);
		globals.Own(g_pKeyAccelerator);
		globals.Own(g_pAcceleratorManager);
		globals.Own(g_pModifyStatusManager);
		globals.Own(g_pEffectGeneratorTable);
		globals.Own(g_pUIDialog);
		globals.Own(g_pProfileManager);
		globals.Own(g_pGuildMarkManager);
		globals.Own(g_pGuildInfoMapper);
		globals.Own(g_pEventManager);
		globals.Own(g_pEffectSpriteTypeTable);
		globals.Own(g_pActionEffectSpriteTypeTable);
		globals.Own(g_pCreatureTable);
		globals.Own(g_pCreatureSpriteTypeMapper);
		globals.Own(g_pItemTable);
		globals.Own(g_pCreatureSpriteTable);
		globals.Own(g_pEffectStatusTable);
		globals.Own(g_pInteractionObjectTable);
		globals.Own(g_pItemOptionTable);
		globals.Own(g_pNPCTable);
		globals.Own(g_pJusticeAttackManager);
		globals.Own(g_pNPCScriptTable);
		globals.Own(g_pShopTemplateTable);
		globals.Own(g_pSkillInfoTable);
		globals.Own(g_pRankBonusTable);
		globals.Own(g_pExperienceTable);
		globals.Own(g_pSoundTable);
		globals.Own(g_pMusicTable);
		globals.Own(g_pZoneTable);
		globals.Own(g_pSoundManager);
		globals.Own(g_pUsingColorSetTable);
		globals.Own(g_pActionInfoTable);
		globals.Own(g_pChatManager);
		globals.Own(g_pHelpStringTable);
		globals.Own(g_pHelpDisplayer);
		globals.Own(g_pLevelNameTable);
		globals.Own(g_pMonsterNameTable);

		globals.Lend(g_pSDLInput, static_cast<CSDLInput*>(NULL));
		const DXInput::Host oldInput = DXInput::GetHost();
		DXInput::SetHost(DXInput::Host{});
		globals.Then([oldInput]() { DXInput::SetHost(oldInput); });
		const GameInitInfoHost::Host* oldInfo = GameInitInfoHost::SetHost(&initInfoHost);
		globals.Then([oldInfo]() { GameInitInfoHost::SetHost(oldInfo); });
		// A nickname table the definitions ClientMain read name, which
		// InitGame's own do not.
		WriteBytes(dir / "Data/Info/Previous.inf", Bytes().Int(1).Word(0).String("Old").data);
	}
	// Data/Info/FileDef.inf: kFileDef but the names left out, then the
	// extra lines.
	void FileDef(const std::vector<std::string>& unnamed = {}, const std::string& extra = "")
	{
		std::string text = "# FileDef\n";
		for (const auto& key : kFileDef)
			if (std::find(unnamed.begin(), unnamed.end(), key[0]) == unnamed.end())
				text += std::string(key[0]) + " : " + key[1] + "\n";
		WriteBytes(dir / "Data/Info/FileDef.inf", text + extra);
	}
	// The nickname table: a count, then each index and string.
	void Nicknames()
	{
		WriteBytes(dir / "Data/Info/NickName.inf", Bytes().Int(2).Word(1).String("Two").Word(0).String("One").data);
	}
	// The string table: a count, then each string.
	void Strings(const std::vector<std::string>& strings)
	{
		Bytes bytes;
		bytes.Int(std::int32_t(strings.size()));
		for (const std::string& text : strings) bytes.String(text);
		WriteBytes(dir / "Data/Info/String.inf", bytes.data);
	}
	// A client of another language than English (LANGUAGE 3).
	void Korean() { WriteBytes(dir / "Data/Info/Language.inf", "LANGUAGE 0\n"); }
	static std::string KoreanFileDef() { return "FILE_LANGUAGE_INFO : Data/Info/Language.inf\n"; }
};

// The message logs and their sizes, "lines x width" each, in the order
// system, player, game, help, notice.
std::string Logs()
{
	std::string text;
	for (CMessageArray* log : { g_pSystemMessage, g_pPlayerMessage, g_pGameMessage, g_pHelpMessage, g_pNoticeMessage })
		text += log == NULL ? std::string("none ") : std::to_string(log->GetSize()) + "x" + std::to_string(MessagesPeek::Width(*log)) + " ";
	return text;
}

std::string Entry(const MStringArray* table, int i)
{
	if (table == NULL || i >= table->GetSize() || (*table)[i].GetString() == NULL) return "(none)";
	return (*table)[i].GetString();
}

// Nothing of InitUserOption and the steps after it ran: no user options
// (and the host's shop refresh time not asked), the input host still
// empty, no input device.
bool StoppedBeforeTheUserOptions()
{
	const DXInput::Host& input = DXInput::GetHost();
	return g_pUserOption == NULL
		&& std::find(calls.begin(), calls.end(), "shop time") == calls.end()
		&& input.mousePosition == NULL && input.activeApp == NULL && input.graphicsReset == NULL
		&& g_pSDLInput == NULL;
}

} // namespace

TEST(GameInit3Start, TheFileDefinitionsComeFromDataInfoInTheWorkingDirectory)
{
	// Recorded defect: ClientMain has made and read g_pFileDef already
	// (Client.cpp:3703-3704, MFileDef.h's FILE_INFO_FILEDEF); InitGame makes
	// another from its own spelling of the same file, the literal
	// "Data/Info/FileDef.inf" (GameInit.cpp:902-904), and the first is
	// leaked. Without the file Properties::load throws, out of InitGame and
	// out of ClientMain, which catches nothing.
	StartUp start;
	CHECK(Thrown([]() { InitGame(); }) == "FileNotExistException: Data/Info/FileDef.inf");
	CHECK(g_pFileDef != &start.previous);
	CHECK(g_pFileDef != NULL && g_pFileDef->getProperty("RESOURCE_TEXT_ENCODING") == "CP949");
	CHECK(TextEncoding::GetResourceEncoding() == TextEncoding::Encoding::Utf8);
	CHECK(g_pGameStringTable == NULL && g_pNickNameStringTable == NULL && g_pSystemMessage == NULL);
	CHECK(messages.empty());
}

TEST(GameInit3Start, TheLoggingStartsFirst)
{
	StartUp start;
	log_cleanup();
	CHECK(!log_is_enabled(LOG_LEVEL_ERROR));
	Thrown([]() { InitGame(); });
	CHECK(log_is_enabled(LOG_LEVEL_ERROR));
	// A Debug build also writes the log beside the executable.
#ifdef _DEBUG
	CHECK(Exists(start.dir / "DarkEden-debug.log"));
#else
	CHECK(!Exists(start.dir / "DarkEden-debug.log"));
#endif
}

TEST(GameInit3Start, TheMathAndOrbitTablesAreBuiltOnceTheEncodingIsKnown)
{
	// Recorded defect: MathTable::FCreateSines doubles the arctangent
	// table in place (MathTable.cpp:903-906), so a second InitGame in one
	// process would double every angle the table gives again; the game
	// calls InitGame once.
	for (bool fileDef : { false, true })
	{
		StartUp start;
		if (fileDef) start.FileDef();
		std::fill_n(MathTable::FSinTab, MathTable::MAX_ANGLE, 0);
		std::fill_n(MathTable::FCosTab, MathTable::MAX_ANGLE, 0);
		std::fill_n(&(*Get(OrbitPositionsOf()))[0][0], EffectOrbit::TypeCount * EffectOrbit::StepCount, POINT{});
		const int tangent = MathTable::FArcTanTab[MathTable::MAX_ANGLE];
		Thrown([]() { InitGame(); });
		// Without FileDef.inf nothing is built; with it, sin and cos in
		// 16.16 over 2048 steps (the right angle's sine from a float pi, so
		// within one), the arctangent doubled, and the orbits' offsets from
		// them (the widest orbit: two tiles across, two down).
		CHECK_EQ(fileDef ? 0x10000 : 0, MathTable::FCosTab[0]);
		CHECK(std::abs(MathTable::FSinTab[MathTable::ANGLE_90] - (fileDef ? 0x10000 : 0)) <= 1);
		CHECK_EQ(fileDef ? 2 * tangent : tangent, MathTable::FArcTanTab[MathTable::MAX_ANGLE]);
		CHECK_EQ(fileDef ? 2 * TILE_X : 0, int(EffectOrbit(0, 0).GetPosition().x));
		CHECK(std::abs(int(EffectOrbit(0, 8).GetPosition().y) - (fileDef ? 2 * TILE_Y : 0)) <= 1);
	}
}

TEST(GameInit3Start, TheResourceEncodingIsFileDefsOrCP949)
{
	{
		StartUp start;
		start.FileDef();
		CHECK_EQ(FALSE, InitGame());
		CHECK(TextEncoding::GetResourceEncoding() == TextEncoding::Encoding::Cp949);
	}
	{
		StartUp start;
		start.FileDef({}, "RESOURCE_TEXT_ENCODING : gbk\n");
		CHECK_EQ(FALSE, InitGame());
		CHECK(TextEncoding::GetResourceEncoding() == TextEncoding::Encoding::Gbk);
	}
}

TEST(GameInit3Start, AnUnknownResourceEncodingThrows)
{
	// Recorded defect: the Error (GameInit.cpp:907) escapes InitGame,
	// InitApp and ClientMain, none of which catches (Client.cpp:1731, 4343):
	// std::terminate, with no message.
	StartUp start;
	start.FileDef({}, "RESOURCE_TEXT_ENCODING : KOI8-R\n");
	start.Nicknames();
	CHECK(Thrown([]() { InitGame(); }) == "Error: Unsupported RESOURCE_TEXT_ENCODING in Data/Info/FileDef.inf");
	CHECK(TextEncoding::GetResourceEncoding() == TextEncoding::Encoding::Utf8);
	// FileDef was read; nothing after the encoding was made.
	CHECK(g_pFileDef != &start.previous && g_pFileDef->getProperty("FILE_INFO_STRING") == "Data/Info/String.inf");
	CHECK(g_pGameStringTable == NULL && g_pNickNameStringTable == NULL && g_pSystemMessage == NULL);
}

TEST(GameInit3Start, WithoutTheNicknameTableItFailsWithItsLogsUnsized)
{
	// The table the previous definitions name is there; InitGame reads its
	// own.
	StartUp start;
	start.FileDef();
	CHECK_EQ(FALSE, InitGame());
	CHECK(messages == std::vector<std::string>({ FailOpen("Data/Info/NickName.inf") }));
	// The tables and logs are made, the logs not yet sized.
	CHECK(g_pGameStringTable != NULL && g_pGameStringTable->GetSize() == 0);
	CHECK(g_pNickNameStringTable != NULL && g_pNickNameStringTable->GetSize() == 0);
	CHECK(Logs() == "0x0 0x0 0x0 0x0 0x0 ");
	CHECK(g_pClientConfig == NULL);
	CHECK(StoppedBeforeTheUserOptions());
}

TEST(GameInit3Start, WithoutTheNicknameTablesNameItThrows)
{
	StartUp start;
	start.FileDef({ "FILE_INFO_NICKNAME_STRING" });
	CHECK(Thrown([]() { InitGame(); }) == "NoSuchElementException: FILE_INFO_NICKNAME_STRING");
	CHECK(g_pNickNameStringTable != NULL && g_pSystemMessage != NULL && g_pNoticeMessage != NULL);
	CHECK(messages.empty());
}

TEST(GameInit3Start, AnotherLanguageWithoutItsStringTableFails)
{
	StartUp start;
	start.FileDef({}, StartUp::KoreanFileDef());
	start.Korean();
	start.Nicknames();
	CHECK_EQ(FALSE, InitGame());
	CHECK(messages == std::vector<std::string>({ FailOpen("Data/Info/String.inf") }));
	CHECK(Entry(g_pNickNameStringTable, 0) == "One" && Entry(g_pNickNameStringTable, 1) == "Two");
	CHECK(g_pGameStringTable != NULL && g_pGameStringTable->GetSize() == 0);
	CHECK(Logs() == "0x0 0x0 0x0 0x0 0x0 ");
	CHECK(g_pClientConfig == NULL);
}

TEST(GameInit3Start, AnotherLanguageKeepsItsStringTableSanitisedAndGoesOn)
{
	// The steps run in order until InitInfomation fails (no regen tower
	// file): the client configuration is made and written back first,
	// nothing from the user options on runs.
	StartUp start;
	start.FileDef({}, StartUp::KoreanFileDef());
	start.Korean();
	start.Nicknames();
	start.Strings({ "Hello", "%n words" });
	CHECK_EQ(FALSE, InitGame());
	CHECK(messages.empty());
	CHECK(g_pGameStringTable != NULL && g_pGameStringTable->GetSize() == 2);
	CHECK(Entry(g_pGameStringTable, 0) == "Hello");
	CHECK(Entry(g_pGameStringTable, 1) == "(removed unsafe string)");
	CHECK(Logs() == "5x256 5x256 5x256 5x256 7x256 ");
	CHECK(g_pClientConfig != NULL && Exists(start.dir / "Data/Info/ClientConfig.inf"));
	CHECK(calls == std::vector<std::string>({ "error Cannot Open RTI File" }));
	CHECK(g_pKeyAccelerator != NULL && g_pRegenTowerInfoManager != NULL);
	CHECK(StoppedBeforeTheUserOptions());
}

TEST(GameInit3Start, TheEnglishClientTakesItsBuiltInStrings)
{
	StartUp start;
	start.FileDef();
	start.Nicknames();
	start.Strings({ "File zero", "%n words" });
	CHECK_EQ(FALSE, InitGame());
	CHECK(messages.empty());
	CHECK(g_pGameStringTable != NULL && g_pGameStringTable->GetSize() == MAX_GAME_STRING + 1);
	CHECK(Entry(g_pGameStringTable, STRING_NETWORK_CONDITION_BAD) == "The connection is unstable.");
	CHECK(Logs() == "5x256 5x256 5x256 5x256 7x256 ");
	CHECK(calls == std::vector<std::string>({ "error Cannot Open RTI File" }));
	CHECK(StoppedBeforeTheUserOptions());
}

TEST(GameInit3Start, TheEnglishClientNeedsNoStringTableButReportsItMissing)
{
	// Recorded defect: English takes String.inf as optional
	// (GameInit.cpp:978), but FileOpenBinary reports the missing file as a
	// start-up failure (GameInitInfo.cpp:166), whose executable answer marks
	// the game inactive and hides (on Windows destroys) the window InitApp
	// made before InitGame, which InitDraw then uses (Client.cpp:378-395).
	StartUp start;
	start.FileDef();
	start.Nicknames();
	CHECK_EQ(FALSE, InitGame());
	CHECK(messages == std::vector<std::string>({ FailOpen("Data/Info/String.inf") }));
	CHECK(Entry(g_pGameStringTable, STRING_NETWORK_CONDITION_BAD) == "The connection is unstable.");
	CHECK(g_pClientConfig != NULL);
	CHECK(calls == std::vector<std::string>({ "error Cannot Open RTI File" }));
}

TEST(GameInit3Start, TheTablesAndLogsThereAreKept)
{
	StartUp start;
	start.FileDef();
	MStringArray* strings = g_pGameStringTable = new MStringArray;
	MStringArray* nicknames = g_pNickNameStringTable = new MStringArray;
	CMessageArray* system = g_pSystemMessage = new CMessageArray;
	CMessageArray* player = g_pPlayerMessage = new CMessageArray;
	CMessageArray* game = g_pGameMessage = new CMessageArray;
	CMessageArray* help = g_pHelpMessage = new CMessageArray;
	CMessageArray* notice = g_pNoticeMessage = new CMessageArray;
	CHECK_EQ(FALSE, InitGame());
	CHECK(g_pGameStringTable == strings && g_pNickNameStringTable == nicknames);
	CHECK(g_pSystemMessage == system && g_pPlayerMessage == player && g_pGameMessage == game);
	CHECK(g_pHelpMessage == help && g_pNoticeMessage == notice);
}
