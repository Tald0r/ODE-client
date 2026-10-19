#include "test_framework.h"
#include "ClientMain.h"
#include "GameInit.h"
#include "GameMain.h"
#include "ClientMode.h"
#include "InputState.h"
#include "FrameClock.h"
#include "MonotonicClock.h"
#include "Exception.h"
#include "Properties.h"
#include "DebugLog.h"
#include "WinLib/CWinUpdate.h"
#include "CGameUpdate.h"
#include "CWaitPacketUpdate.h"
#include "CWaitUIUpdate.h"
#include "UIDialog.h"
#include "MWeather.h"
#include "MParty.h"
#include "MWarManager.h"
#include "MChatManager.h"
#include "MEventManager.h"
#include "MMusic.h"
#include "UserOption.h"
#include "UiRuntime.h"
#include "VS_UI.h"
#include "u_window.h"
#include "pi_core.h"
#include "Ci.h"
#include "CSprite.h"
#include "CSpriteSurface.h"
#include "DXLib/CDirectDraw.h"
#include "DXLib/CDirectSound.h"
#include "DXLib/COGGSTREAM.h"
#include "DXLib/CSoundPartManager.h"
#include "CDirectInput.h"
#include "DXLibBackend.h"
#include "ClientPlayer.h"
#include "PacketFactoryManager.h"
#include "PacketValidator.h"
#include "RequestServerPlayerManager.h"
#include "ClientCommunicationManager.h"
#include "RequestUserManager.h"
#include "RequestFileManager.h"
#include "MPlayer.h"
#include "MZone.h"
#include "MTopView.h"
#include "MScreenEffectManager.h"
#include "MZoneSoundManager.h"
#include "MNPCTable.h"
#include "TalkBox.h"
#include "MShopTemplateTable.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MInventory.h"
#include "MNPCScriptTable.h"
#include "CServerInformation.h"
#include "MMonsterKillQuestInfo.h"
#include "AcceleratorManager.h"
#include "ModifyStatusManager.h"
#include "MPriceManager.h"
#include "MItem.h"
// The hosts ReleaseAllObjects resets.
#include "MEffectTarget.h"
#include "MEffect.h"
#include "MHelicopterManager.h"
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
#include "UIFunction.h"
#include "UIMessageManager.h"
#include "PacketFunction.h"
#include "CUsingColorSetTable.h"
#include "ClientConfig.h"
#include "ClientDef.h"
#include "ExperienceTable.h"
#include "FameInfo.h"
#include "GameInitInfo.h"
#include "MActionInfoTable.h"
#include "MCreatureSpriteTable.h"
#include "MCreatureTable.h"
#include "MEffectGeneratorTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "MGameStringTable.h"
#include "MGameTime.h"
#include "MGuildInfoMapper.h"
#include "MGuildMarkManager.h"
#include "MHelpDisplayer.h"
#include "MHelpStringTable.h"
#include "MInteractionObjectTable.h"
#include "MItemOptionTable.h"
#include "MItemTable.h"
#include "MJusticeAttackManager.h"
#include "MLevelNameTable.h"
#include "MMoneyManager.h"
#include "MMusicTable.h"
#include "MObjectSelector.h"
#include "MSkillManager.h"
#include "MSoundTable.h"
#include "MStorage.h"
#include "MTimeItemManager.h"
#include "MTradeManager.h"
#include "MZoneTable.h"
#include "MonsterNameTable.h"
#include "PCConfigTable.h"
#include "ProfileManager.h"
#include "Profiler.h"
#include "RankBonusTable.h"
#include "ShrineInfoManager.h"
#include "SystemAvailabilities.h"
#include "TempInformation.h"
#include "UserInformation.h"
#include "KeyAccelerator.h"
#include "ZoneFlags.h"
#include "GameSocket.h"
#include "ServerInfoFileParser.h"
#include "GameMessages.h"
#include "../basic/CMessageArray.h"
#include <SDL.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

extern bool g_bRunning;			// DXLibBackendSDL.cpp's, which no header declares

// ClientMain.cpp (task 4.265 moved it, as Client.cpp, into gameclient),
// group 2: the window and the frame loop, the shutdown, and the host.
// - ClientHost: without a host, or with a host whose entry is empty,
//   Exit, RunUpdater, ShowMessageBox, DestroyGameWindow and WaitForMessage
//   do nothing, CreateGameWindow answers NULL, PumpMessage NoMessage and
//   ListDllNames false (the names untouched); with one, each passes its
//   arguments through and answers what the host answers. SetHost answers
//   the host it replaces; every call reads the current one.
// - RunClientFrame: off Windows it first drains SDL's events into the
//   input (an SDL_QUIT clears g_bRunning) and answers false, without
//   pumping, while g_bRunning is clear; on Windows it leaves both alone.
//   Then one turn of the OS pump: the quit message answers false; a
//   dispatched message is the whole frame; with none waiting, an active
//   application stamps the frame clock, runs g_pUpdate (when there is
//   one) and, once more than 1000 ms have passed since the frame-rate
//   window opened, sets g_FrameRate to the frames counted since then
//   (after this frame's update) per second, truncated, g_bGoodFPS to
//   whether that is at least 15, and opens the window again; an inactive
//   one waits for a message. Every answer but those two false is true.
// - FinishClient: ReleaseAllObjects (test_game_init_2.cpp pins it), then
//   the IME (gC_ci) is deleted, the mutex released (Windows), and with
//   g_bNeedUpdate set the working directory goes to g_CWD and the
//   updater runs with "UPDATE <command>", cut to 511 characters.
// - InitApp (Windows): clears the MAC address and reads it from NetBIOS,
//   asks for the game's window - a screen-sized popup in full screen, or
//   in a window the frame times the largest scale the work area holds
//   (g_WindowScale) plus the borders and menu - and answers FALSE without
//   one. Then, on every platform, it hides the cursor and answers
//   InitGame.
// - InitFailMessage stops the application, shows the cursor, logs the
//   message at debug level, destroys the game's window (Windows) and
//   answers 0.
// - WindowProc: MM_MCINOTIFY answers 0; the system menu's keys and the
//   task switches answer 1, and moving, sizing, maximising and the
//   monitor's power too in full screen; WM_ACTIVATEAPP sets the double
//   click time, runs CheckActivate and sets g_bActiveGame; WM_DESTROY
//   posts the quit message; WM_GETMINMAXINFO pins the window to the
//   scaled frame; WM_TIMER updates the music stream while the audio is up;
//   the keys and the IME's messages go to the UI first, Scroll Lock takes
//   a screenshot, and WM_KEYDOWN, WM_CHAR and WM_IME_STARTCOMPOSITION
//   answer 0; the rest go to the default procedure.
//
// The process and the OS are a recording ClientHost (SystemHost is never
// installed); the update is a recording CWinUpdate; the UI is the real one
// with a window of the test's taking the keyboard; the clock is a test
// source. Frees are seen by counting destructors.
//
// Windows only: InitApp's window (two tests), the single-instance mutex,
// the quit message WM_DESTROY posts, SDL left alone by the frame. Off
// Windows only: the frame's SDL pump, the shim's cursor count, the
// commands and messages that reach the default procedure.
//
// Not run:
// - InitGame answering TRUE, and so InitApp's TRUE: that needs the whole
//   data tree. InitApp is seen handing over to it by the exception
//   InitGame throws without Data/Info/FileDef.inf (test_game_init_3.cpp
//   pins InitGame).
// - On Windows, the window commands that reach DefWindowProc: the real one
//   would start a move loop or turn the monitor off (off Windows they run,
//   and the shim answers 0). No default answer is asserted on Windows.
// - WM_MOVE's CSDLGraphics::OnMove, which is empty: only the answer is seen.
// - Whether a message answers 0 itself or through DefWindowProc, off
//   Windows, where the shim's DefWindowProc answers 0 (MM_MCINOTIFY,
//   WM_KEYDOWN, WM_CHAR and WM_IME_STARTCOMPOSITION return before it);
//   WM_DESTROY's quit message, which the shim drops (seen on Windows).
// - On Windows the IME's composition messages, which the real default
//   procedure hands to the thread's IME window.
// - The frees of objects without a virtual destructor (g_pClientConfig):
//   only their reset is seen.
//
// Recorded defects:
// - ClientMain.cpp:432 switches on the whole wParam of WM_SYSCOMMAND, whose
//   low four bits the system uses (MSDN: mask with 0xFFF0): a move by the
//   title bar (SC_MOVE | HTCAPTION) or a size by a border is not
//   swallowed in full screen.
// - ClientMain.cpp:474 sets g_bActiveGame to the activation's wParam after
//   CheckActivate, which marks the game active only while the application
//   is (GameMain.cpp:1128-1145): an activation before ClientMain sets
//   g_bActiveApp (ClientMain.cpp:1636; InitApp's ShowWindow comes first)
//   marks the game active without CheckActivate's restore having run.
// - ClientMain.cpp:1070 deletes gC_ci without resetting it: the pointer is
//   left dangling (nothing reads it after FinishClient today).
// - ClientMain.cpp:1072-1076 frees g_pClientConfig, which ReleaseAllObjects
//   has already freed and reset (GameInit.cpp's SAFE_DELETE): dead code.
// - ClientMain.cpp:1088 ignores _chdir's answer: when g_CWD cannot be
//   entered the updater, a relative name, is run from wherever the
//   process is.
// - ClientMain.cpp:1087 cuts the updater's command line to 511 characters
//   without a word.
// - ClientMain.cpp:252 destroys the game's window and leaves g_hWnd naming
//   it (Windows).
// - basic/Platform.h:653: the shim's ShowCursor is a static inline with a
//   function-local count, so each translation unit counts on its own,
//   where Win32 keeps one count per thread: InitFailMessage shows the
//   cursor though another file's hide is still outstanding (the test
//   file's, here).
// - basic/Platform.h:759 and 770: off Windows SC_TASKLIST is 0xF140 (Win32:
//   0xF130; 0xF140 is SC_SCREENSAVE) and MM_MCINOTIFY 0x3D9 (Win32:
//   0x3B9). Harmless, as nothing sends them there.

namespace {

//----------------------------------------------------------------------
// Private and protected members, read through an explicit instantiation
// (which may name them).
//----------------------------------------------------------------------
template <class Tag, typename Tag::type Member>
struct Expose
{
	friend typename Tag::type Get(Tag) { return Member; }
};
struct FullscreenOf { using type = bool*; friend type Get(FullscreenOf); };
template struct Expose<FullscreenOf, &CSDLGraphics::m_bFullscreen>;
struct AudioUpOf { using type = bool CSDLAudio::*; friend type Get(AudioUpOf); };
template struct Expose<AudioUpOf, &CSDLAudio::m_bInit>;
struct StreamingOf { using type = bool COGGSTREAM::*; friend type Get(StreamingOf); };
template struct Expose<StreamingOf, &COGGSTREAM::m_bPlaying>;

std::string calls;
std::string N(long long value) { return std::to_string(value); }
std::string H(HWND hWnd) { return std::to_string(static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(hWnd))); }
void Call(const std::string& call) { calls += call + ";"; }

bool Same(const std::string& expected, const std::string& actual)
{
	if (expected != actual)
		std::fprintf(stderr, "    expected %s\n    actual   %s\n", expected.c_str(), actual.c_str());
	return expected == actual;
}
#define CHECK_SAME(expected, actual) CHECK(Same((expected), (actual)))

// Runs f; the Throwable it lets out, as "Name: message", or "" for none.
template <class F> std::string Thrown(F f)
{
	try { f(); }
	catch (Throwable& t) { return t.getName() + ": " + t.getMessage(); }
	return "";
}

// A scratch directory, removed with everything in it.
struct TempDir
{
	std::filesystem::path path;
	TempDir()
	{
		static unsigned sequence = 0;
		path = std::filesystem::temp_directory_path() / ("darkeden_client_main_2_"
			+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_" + std::to_string(sequence++));
		std::filesystem::create_directories(path);
	}
	~TempDir()
	{
		std::error_code error;
		std::filesystem::remove_all(path, error);
	}
};

bool InDirectory(const std::filesystem::path& dir)
{
	std::error_code error;
	return std::filesystem::equivalent(std::filesystem::current_path(), dir, error);
}

//----------------------------------------------------------------------
// The host: every call recorded, with the answers the test sets.
//----------------------------------------------------------------------
ClientHost::Pumped pumpAnswer = ClientHost::Pumped::NoMessage;
HWND windowAnswer = NULL;
bool dllAnswer = true;
std::filesystem::path updaterDir;

const ClientHost::Host recordingHost = {
	.Exit = [](int code) { Call("exit " + N(code)); },
	.RunUpdater = [](const char* arguments) {
		Call(std::string("updater ") + arguments);
		updaterDir = std::filesystem::current_path();
	},
	.ShowMessageBox = [](const char* text, const char* caption) { Call(std::string("box ") + text + "|" + caption); },
	.CreateGameWindow = [](DWORD exStyle, DWORD style, int width, int height) -> HWND {
		Call("create " + N(exStyle) + "," + N(style) + "," + N(width) + "," + N(height));
		return windowAnswer;
	},
	.DestroyGameWindow = [](HWND hWnd) { Call("destroy " + H(hWnd)); },
	.PumpMessage = []() { Call("pump"); return pumpAnswer; },
	.WaitForMessage = [] { Call("wait"); },
	.ListDllNames = [](std::vector<std::string>& names) {
		Call("dlls");
		names.push_back("fmod.dll");
		return dllAnswer;
	},
};

// The recording host installed, and the one before put back.
struct Hosted
{
	const ClientHost::Host* previous = ClientHost::SetHost(&recordingHost);
	Hosted()
	{
		calls.clear();
		pumpAnswer = ClientHost::Pumped::NoMessage;
		windowAnswer = NULL;
		dllAnswer = true;
		updaterDir.clear();
	}
	~Hosted() { ClientHost::SetHost(previous); }
};

//----------------------------------------------------------------------
// The frame loop's world: the recording host, a test clock, SDL's events
// (dummy video) drained into the input, a recording update, the
// application active and running, the frame-rate window opened at kStart
// at frame 10, the frame count 40, the last rate 77 and not good.
//----------------------------------------------------------------------
MonotonicClock::TimePoint now;
MonotonicClock::TimePoint Now() { return now; }
const MonotonicClock::TimePoint kStart = MonotonicClock::TimePoint() + MonotonicClock::Millis(5000000);
const MonotonicClock::TimePoint kStamped = kStart - MonotonicClock::Millis(7);
long long Ms(MonotonicClock::TimePoint t) { return static_cast<long long>(t.time_since_epoch().count()); }
MonotonicClock::TimePoint After(int ms) { return kStart + MonotonicClock::Millis(DWORD(ms)); }

// Records each update with the frame clock it sees, and counts frames.
struct RecordingLoop : CWinUpdate
{
	int frames = 0;
	void Update() override
	{
		Call("update@" + N(Ms(g_FrameNow)));
		g_FrameCount += frames;
	}
};

struct Frame
{
	Hosted hosted;
	MonotonicClock::ScopedTestSource clock{Now};
	bool running = g_bRunning;
	BOOL activeApp = g_bActiveApp;
	CWinUpdate* update = g_pUpdate;
	MonotonicClock::TimePoint frameNow = g_FrameNow, startTime = g_StartTime;
	int startFrameCount = g_StartFrameCount, frameCount = g_FrameCount, frameRate = g_FrameRate;
	bool goodFPS = g_bGoodFPS;
	RecordingLoop loop;
	Frame()
	{
		SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
		CHECK_EQ(0, SDL_InitSubSystem(SDL_INIT_VIDEO));
		CHECK_EQ(0, dxlib_input_init(nullptr));
		SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
		g_bRunning = true;
		g_bActiveApp = TRUE;
		g_pUpdate = &loop;
		now = kStart;
		g_FrameNow = kStamped;
		g_StartTime = kStart;
		g_StartFrameCount = 10;
		g_FrameCount = 40;
		g_FrameRate = 77;
		g_bGoodFPS = false;
	}
	~Frame()
	{
		SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
		dxlib_input_release();
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		g_bGoodFPS = goodFPS;
		g_FrameRate = frameRate;
		g_FrameCount = frameCount;
		g_StartFrameCount = startFrameCount;
		g_StartTime = startTime;
		g_FrameNow = frameNow;
		g_pUpdate = update;
		g_bActiveApp = activeApp;
		g_bRunning = running;
	}
	// The frame-rate window and the clock as they were set.
	bool Untouched() const
	{
		return g_FrameNow == kStamped && g_FrameRate == 77 && !g_bGoodFPS && g_StartTime == kStart
			&& g_StartFrameCount == 10 && g_FrameCount == 40;
	}
};

void PushQuit()
{
	SDL_Event event{};
	event.type = SDL_QUIT;
	CHECK_EQ(1, SDL_PushEvent(&event));
}

//----------------------------------------------------------------------
// The shutdown's world. Every global ReleaseAllObjects (with
// EndTitleLoading, ReleaseGameUpdate and ReleaseSocket) frees or resets is
// saved, then none, and every host it resets saved; all are given back
// after, with the game's activity, the title loading, the music, the move
// tables, the IME, the update flag, g_CWD and the working directory.
//----------------------------------------------------------------------
#define CM2_RELEASED(X) \
	X(g_pTitleLoadingSprite) X(g_pCGameUpdate) X(g_pCWaitPacketUpdate) X(g_pCWaitUIUpdate) \
	X(g_pOGG) X(g_oggfile) X(g_pSoundBufferForOGG) X(g_pUIDialog) X(g_pWeather) \
	X(g_pInventoryEffectManager) X(g_pPlayer) X(g_pZone) X(g_pZoneSmall) X(g_pZoneLarge) \
	X(g_pSDLInput) X(g_pZoneSoundTable) X(g_pZoneSoundManager) X(g_pTopView) X(g_pBack) X(g_pLast) \
	X(g_pSocket) X(g_pConfigKorean) X(g_pConfigForeign) X(g_pPacketFactoryManager) X(g_pPacketValidator) \
	X(g_pRequestServerPlayerManager) X(g_pClientCommunicationManager) X(g_pRequestFileManager) X(g_pRequestUserManager) \
	X(g_pGuildMarkManager) X(g_pNPCTable) X(g_pNPCTalkBox) X(g_pPCTalkBox) X(g_pShopTemplateTable) X(g_pSlayerGear) \
	X(g_pVampireGear) X(g_pOustersGear) X(g_pInventory) X(g_pSoundManager) X(g_pNPCScriptTable) X(g_pServerInformation) \
	X(g_pQuestInfoManager) X(g_pAcceleratorManager) X(g_pModifyStatusManager) \
	X(g_pActionInfoTable) X(g_pCreatureSpriteTable) X(g_pCreatureTable) X(g_pCreatureSpriteTypeMapper) \
	X(g_pEffectSpriteTypeTable) X(g_pActionEffectSpriteTypeTable) X(g_pEffectStatusTable) X(g_pGameMessage) \
	X(g_pGameStringTable) X(g_pInteractionObjectTable) X(g_pItemOptionTable) X(g_pSkillAvailable) \
	X(g_pSkillInfoTable) X(g_pSkillManager) X(g_pSoundTable) X(g_pSystemMessage) X(g_pPlayerMessage) \
	X(g_pUsingColorSetTable) X(g_pZoneTable) X(g_pNickNameStringTable) X(g_pRegenTowerInfoManager) \
	X(g_pSystemAvailableManager) X(g_pWorldPlayerConfigTable) X(g_pUserInformation) X(g_pTempInformation) \
	X(g_pHelpMessage) X(g_pNoticeMessage) X(g_pClientConfig) X(g_pUserOption) X(g_pProfileManager) \
	X(g_pGuildInfoMapper) X(g_pEventManager) X(g_pItemTable) X(g_pJusticeAttackManager) \
	X(g_pRankBonusTable) X(g_pExperienceTable) X(g_pMusicTable) X(g_pLevelNameTable) X(g_pParty) \
	X(g_pTradeManager) X(g_pStorage) X(g_pStorage2) X(g_pGameTime) X(g_pKeyAccelerator) \
	X(g_pEffectGeneratorTable) X(g_pPriceManager) X(g_pMoneyManager) X(g_pChatManager) X(g_pHelpStringTable) \
	X(g_pHelpDisplayer) X(g_pObjectSelector) X(g_pUIMessageManager) X(g_pMonsterNameTable) \
	X(g_pTimeItemManager) X(g_pFameInfoTable) X(g_pWarManager) X(g_pProfiler) X(g_pFileDef)

#define CM2_SAVE(g) decltype(g) saved_##g = g;
#define CM2_CLEAR(g) g = NULL;
#define CM2_RESTORE(g) g = saved_##g;

// One of the hosts ReleaseAllObjects resets: Take saves it and leaves
// none, Give puts it back.
struct HostSlot
{
	void (*take)();
	void (*give)();
};
template <typename T> struct HostOf;
template <typename T> struct HostOf<const T* (*)(const T*)> { using type = T; };
// The hosts whose SetHost answers the one it replaces.
template <auto Set>
struct Slot
{
	static inline const typename HostOf<decltype(Set)>::type* before = nullptr;
	static void Take() { before = Set(nullptr); }
	static void Give() { Set(before); }
};
#define CM2_SLOT(Owner) HostSlot{ &Slot<&Owner::SetHost>::Take, &Slot<&Owner::SetHost>::Give }

const MItemHost* itemBefore = nullptr;

const HostSlot kHostSlots[] = {
	{ [] { itemBefore = MItem::GetHost(); MItem::SetHost(nullptr); }, [] { MItem::SetHost(itemBefore); } },
	CM2_SLOT(MChatManager), CM2_SLOT(MWeather), CM2_SLOT(MWarManager), CM2_SLOT(MEffectTarget), CM2_SLOT(MEffect),
	CM2_SLOT(MHelicopter), CM2_SLOT(ZONESOUND_NODE), CM2_SLOT(MGuidanceEffect), CM2_SLOT(MParabolaEffect),
	CM2_SLOT(MAttachEffect), CM2_SLOT(MScreenEffectManager), CM2_SLOT(MStopInventoryEffectGenerator),
	CM2_SLOT(MStopZoneEffectGenerator), CM2_SLOT(MStopZoneCrossEffectGenerator), CM2_SLOT(MStopZoneXEffectGenerator),
	CM2_SLOT(MStopZoneRhombusEffectGenerator), CM2_SLOT(MStopZoneEmptyCrossEffectGenerator),
	CM2_SLOT(MStopZoneEmptyRectEffectGenerator), CM2_SLOT(MStopZoneRectEffectGenerator), CM2_SLOT(MStopZoneWallEffectGenerator),
	CM2_SLOT(MStopZoneMultipleEffectGenerator), CM2_SLOT(MStopZoneRandomEffectGenerator),
	CM2_SLOT(MStopZoneSelectableEffectGenerator), CM2_SLOT(MSpreadOutEffectGenerator), CM2_SLOT(MAroundZoneEffectGenerator),
	CM2_SLOT(MFollowPathEffectGenerator), CM2_SLOT(MMeteorDropEffectGenerator), CM2_SLOT(MAttackCreatureParabolaEffectGenerator),
	CM2_SLOT(MAttackCreatureEffectGenerator), CM2_SLOT(MAttackCreatureHomingEffectGenerator),
	CM2_SLOT(MRippleZoneWideEffectGenerator), CM2_SLOT(MBloodyBreakerEffectGenerator), CM2_SLOT(MBloodyWallEffectGenerator),
	CM2_SLOT(MBloodyWaveEffectGenerator), CM2_SLOT(MRippleZoneEffectGenerator), CM2_SLOT(MRippleZonePixelEffectGenerator),
	CM2_SLOT(MAttachZoneEffectGenerator), CM2_SLOT(MAttachZoneAroundEffectGenerator),
	CM2_SLOT(MAttachZoneSelectableEffectGenerator), CM2_SLOT(MAttackZoneRectEffectGenerator), CM2_SLOT(MSkipEffectGenerator),
	CM2_SLOT(RankBonusHandlers), CM2_SLOT(RegenZoneStatus), CM2_SLOT(BonusSkills), CM2_SLOT(SkillDowngrade),
	CM2_SLOT(GuildPresentation), CM2_SLOT(CommunicationPresentation), CM2_SLOT(QuestWarPresentation),
	CM2_SLOT(NoticePresentation), CM2_SLOT(StatusPresentation), CM2_SLOT(GameOptions), CM2_SLOT(PlayerPresentation),
	CM2_SLOT(NPCPresentation), CM2_SLOT(ZoneLoading), CM2_SLOT(TradePresentation), CM2_SLOT(GameInfoPresentation),
	CM2_SLOT(DialogPresentation), CM2_SLOT(DialogWindow), CM2_SLOT(LoginPresentation), CM2_SLOT(PlayerState),
	CM2_SLOT(ZoneState), CM2_SLOT(CreatureState), CM2_SLOT(WorldEvents), CM2_SLOT(VsUiPresentation), CM2_SLOT(LoginLists),
	CM2_SLOT(MStopZoneEmptyHorizontalWallEffectGenerator), CM2_SLOT(MStopZoneEmptyVerticalWallEffectGenerator),
	CM2_SLOT(MFallingEffectGenerator), CM2_SLOT(MAttackZoneEffectGenerator), CM2_SLOT(MFirePiecingEffectGenerator),
	CM2_SLOT(MPlayingWithFireEffectGenerator), CM2_SLOT(MAttackZoneBombEffectGenerator),
	CM2_SLOT(MAttackZoneParabolaEffectGenerator), CM2_SLOT(MMultipleFallingEffectGenerator), CM2_SLOT(MRisingEffectGenerator),
	CM2_SLOT(MParty), CM2_SLOT(UiRuntime), CM2_SLOT(GameUI), CM2_SLOT(UIMessages),
};

// The move tables MCreature keeps for every creature.
struct MoveTables : MCreature
{
	static bool Ready() { return m_cXTable[0][0] != NULL && m_cYTable[MAX_FRAME_MOVE - 1][MAX_DIRECTION - 1] != NULL; }
};

// The IME and the game's loop, which say when they go.
struct CountedIme : CI_KOREAN
{
	~CountedIme() override { Call("~ime"); }
};
const CGameUpdateHost noGameHost{};
struct CountedLoop : CGameUpdate
{
	CountedLoop() : CGameUpdate(&noGameHost) {}
	~CountedLoop() override { Call("~game loop"); }
};

// The game ended with its loop, the IME and the configuration made, no
// update pending, g_CWD naming dir, and the working directory elsewhere.
struct Shutdown
{
	CM2_RELEASED(CM2_SAVE)
	int activeGame = g_bActiveGame;
	bool titleLoading = g_bTitleLoading;
	MMusic music = g_Music;
	bool moveTables = MoveTables::Ready();
	CI* ime = gC_ci;
	int needUpdate = g_bNeedUpdate;
	char cwd[_MAX_PATH];
	std::filesystem::path workingDir = std::filesystem::current_path();
	Hosted hosted;
	TempDir dir;
	TempDir elsewhere;
	Shutdown()
	{
		std::memcpy(cwd, g_CWD, sizeof(cwd));
		CM2_RELEASED(CM2_CLEAR)
		for (const HostSlot& slot : kHostSlots) slot.take();
		CHECK(!g_SDLAudio.IsInit());
		g_bActiveGame = TRUE;
		g_bNeedUpdate = FALSE;
		g_pCGameUpdate = new CountedLoop;
		g_pClientConfig = new ClientConfig;
		gC_ci = new CountedIme;
		SetCwd(dir.path.string());
		std::filesystem::current_path(elsewhere.path);
	}
	~Shutdown()
	{
		std::error_code error;
		std::filesystem::current_path(workingDir, error);
		std::memcpy(g_CWD, cwd, sizeof(cwd));
		g_bNeedUpdate = needUpdate;
		gC_ci = ime;
		for (const HostSlot& slot : kHostSlots) slot.give();
		CM2_RELEASED(CM2_RESTORE)
		g_Music = music;
		g_bTitleLoading = titleLoading;
		g_bActiveGame = activeGame;
		if (moveTables) MCreature::InitMoveTable();
		else MCreature::ReleaseMoveTable();
	}
	static void SetCwd(const std::string& path) { std::snprintf(g_CWD, _MAX_PATH, "%s", path.c_str()); }
};

//----------------------------------------------------------------------
// The start-up's world: the recording host, an empty working directory
// (so InitGame throws for want of Data/Info/FileDef.inf), no file
// definitions, the logging and the window's globals given back after.
//----------------------------------------------------------------------
struct StartUp
{
	Hosted hosted;
	HWND hWnd = g_hWnd;
	bool fullScreen = g_bFullScreen;
	int windowScale = g_WindowScale;
	BYTE mac[6];
	RECT gameRect = g_GameRect;
	BOOL activeApp = g_bActiveApp;
	Properties* fileDef = g_pFileDef;
	bool logging = log_is_enabled(LOG_LEVEL_ERROR);
	bool debugLogging = log_is_enabled(LOG_LEVEL_DEBUG);
	std::filesystem::path workingDir = std::filesystem::current_path();
	int cursorShown = SDL_ENABLE;
	TempDir dir;
	StartUp()
	{
		std::memcpy(mac, g_macAddress, sizeof(mac));
		SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
		CHECK_EQ(0, SDL_InitSubSystem(SDL_INIT_VIDEO));
		cursorShown = SDL_ShowCursor(SDL_QUERY);
		SDL_ShowCursor(SDL_ENABLE);
		g_pFileDef = NULL;
		g_GameRect = {0, 0, 800, 600};
		g_bActiveApp = TRUE;
		std::filesystem::current_path(dir.path);
	}
	~StartUp()
	{
		// InitGame's log file, if a Debug build opened one, closed first.
		log_init();
		log_set_level(debugLogging ? LOG_LEVEL_DEBUG : LOG_LEVEL_INFO);
		log_set_file_output(nullptr);
		log_set_console_output(true);
		if (!logging) log_cleanup();
		std::error_code error;
		std::filesystem::current_path(workingDir, error);
		delete g_pFileDef;		// InitGame's
		g_pFileDef = fileDef;
		g_bActiveApp = activeApp;
		g_GameRect = gameRect;
		std::memcpy(g_macAddress, mac, sizeof(mac));
		g_WindowScale = windowScale;
		g_bFullScreen = fullScreen;
		g_hWnd = hWnd;
		SDL_ShowCursor(cursorShown);
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}
	// What f logs at debug level, through a file of the test's.
	template <class F> std::string Logged(F f)
	{
		const std::filesystem::path file = dir.path / "debug.log";
		log_init();
		log_set_console_output(false);
		log_set_file_output(file.string().c_str());
		log_set_level(LOG_LEVEL_DEBUG);
		f();
		log_set_file_output(nullptr);
		std::ifstream in(file);
		return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	}
};

#ifdef PLATFORM_WINDOWS
// The thread's cursor display count.
int CursorCount()
{
	ShowCursor(TRUE);
	return ShowCursor(FALSE);
}
#endif

//----------------------------------------------------------------------
// The window procedure's world: the application and game activity, the
// double click time, the full-screen flag, the frame and the window's
// scale given back after.
//----------------------------------------------------------------------
struct Proc
{
	BOOL activeApp = g_bActiveApp;
	int activeGame = g_bActiveGame;
	DWORD doubleClick = g_double_click_time;
	bool fullscreen = *Get(FullscreenOf());
	RECT gameRect = g_GameRect;
	int windowScale = g_WindowScale;
	~Proc()
	{
		g_WindowScale = windowScale;
		g_GameRect = gameRect;
		*Get(FullscreenOf()) = fullscreen;
		g_double_click_time = doubleClick;
		g_bActiveGame = activeGame;
		g_bActiveApp = activeApp;
	}
	static void FullScreen(bool on) { *Get(FullscreenOf()) = on; }
	static LRESULT Command(WPARAM command) { return WindowProc(NULL, WM_SYSCOMMAND, command, 0); }
};

// A window over the screen that takes the keyboard, recording each key.
struct KeyWindow : Window
{
	KeyWindow() : Window(0, 0, 800, 600) {}
	bool IsPixel(int, int) override { return true; }
	bool MouseControl(UINT, int, int) override { return true; }
	void KeyboardControl(UINT message, UINT key, long extra) override
	{
		Call("key " + N(message) + ":" + N(key) + "," + N(extra));
	}
	void Show() override {}
	void Process() override {}
};

// The real UI with the test's window taking the keyboard, an IME and no
// runtime host (so the keyboard is not denied).
struct Keys : Proc
{
	PI_ProcessRunner* runnerBefore = gpC_process_runner;
	WindowManager* windows = gpC_window_manager;
	CI* ime = gC_ci;
	const UiRuntime::Host* runtime = UiRuntime::SetHost(nullptr);
	PI_ProcessRunner runner;
	WindowManager manager;
	CI_KOREAN korean;
	std::unique_ptr<KeyWindow> window;
	Keys()
	{
		gpC_process_runner = &runner;
		gpC_window_manager = &manager;
		gC_ci = &korean;
		window = std::make_unique<KeyWindow>();
		window->AttrKeyboardControl(true);
		window->Start();
		manager.AppearWindow(window.get());
		calls.clear();
	}
	~Keys()
	{
		window.reset();
		UiRuntime::SetHost(runtime);
		gC_ci = ime;
		gpC_window_manager = windows;
		gpC_process_runner = runnerBefore;
	}
};

// Scan code 0x1E, one repeat: what a key's lParam carries.
const LPARAM kKeyBits = 0x001E0001;

} // namespace

//======================================================================
// ClientHost
//======================================================================
TEST(ClientMain2Host, WithoutAHostEveryEntryDoesNothing)
{
	const ClientHost::Host* previous = ClientHost::SetHost(nullptr);
	calls.clear();
	ClientHost::Exit(3);
	ClientHost::RunUpdater("UPDATE x");
	ClientHost::ShowMessageBox("text", "caption");
	CHECK(ClientHost::CreateGameWindow(1, 2, 3, 4) == NULL);
	int window = 0;
	ClientHost::DestroyGameWindow(reinterpret_cast<HWND>(&window));
	CHECK(ClientHost::PumpMessage() == ClientHost::Pumped::NoMessage);
	ClientHost::WaitForMessage();
	std::vector<std::string> names = { "kept.dll" };
	CHECK(!ClientHost::ListDllNames(names));
	CHECK(names == std::vector<std::string>({ "kept.dll" }));
	CHECK_SAME(std::string(""), calls);
	CHECK(ClientHost::SetHost(previous) == nullptr);
}

TEST(ClientMain2Host, AHostWithoutAnEntryIsTheSameAsNone)
{
	const ClientHost::Host empty{};
	const ClientHost::Host* previous = ClientHost::SetHost(&empty);
	ClientHost::Exit(3);
	ClientHost::RunUpdater("UPDATE x");
	ClientHost::ShowMessageBox("text", "caption");
	CHECK(ClientHost::CreateGameWindow(1, 2, 3, 4) == NULL);
	ClientHost::DestroyGameWindow(NULL);
	CHECK(ClientHost::PumpMessage() == ClientHost::Pumped::NoMessage);
	ClientHost::WaitForMessage();
	std::vector<std::string> names;
	CHECK(!ClientHost::ListDllNames(names));
	CHECK(names.empty());
	CHECK(ClientHost::SetHost(previous) == &empty);
}

TEST(ClientMain2Host, EachEntryPassesItsArgumentsAndAnswersTheHosts)
{
	Hosted hosted;
	int window = 0;
	windowAnswer = reinterpret_cast<HWND>(&window);
	ClientHost::Exit(3);
	ClientHost::RunUpdater("UPDATE x");
	ClientHost::ShowMessageBox("text", "caption");
	CHECK(ClientHost::CreateGameWindow(1, 2, 3, 4) == windowAnswer);
	ClientHost::DestroyGameWindow(windowAnswer);
	for (ClientHost::Pumped answer : { ClientHost::Pumped::NoMessage, ClientHost::Pumped::Dispatched, ClientHost::Pumped::Quit })
	{
		pumpAnswer = answer;
		CHECK(ClientHost::PumpMessage() == answer);
	}
	ClientHost::WaitForMessage();
	std::vector<std::string> names = { "a.dll" };
	CHECK(ClientHost::ListDllNames(names));
	dllAnswer = false;
	CHECK(!ClientHost::ListDllNames(names));
	CHECK(names == std::vector<std::string>({ "a.dll", "fmod.dll", "fmod.dll" }));
	CHECK_SAME("exit 3;updater UPDATE x;box text|caption;create 1,2,3,4;destroy " + H(windowAnswer)
		+ ";pump;pump;pump;wait;dlls;dlls;", calls);
}

TEST(ClientMain2Host, EveryCallReadsTheHostInstalledThen)
{
	Hosted hosted;
	const ClientHost::Host other = { .WaitForMessage = [] { Call("other wait"); } };
	ClientHost::WaitForMessage();
	CHECK(ClientHost::SetHost(&other) == &recordingHost);
	ClientHost::WaitForMessage();
	CHECK(ClientHost::SetHost(&recordingHost) == &other);
	ClientHost::WaitForMessage();
	CHECK_SAME(std::string("wait;other wait;wait;"), calls);
}

TEST(ClientMain2Host, TheSystemHostFillsEveryEntryItsPlatformHas)
{
	// Read, never installed or called.
	const ClientHost::Host* host = ClientHost::SystemHost();
	CHECK(host != NULL && host == ClientHost::SystemHost());
	CHECK(host->Exit != NULL && host->RunUpdater != NULL && host->ShowMessageBox != NULL);
	CHECK(host->PumpMessage != NULL && host->WaitForMessage != NULL && host->ListDllNames != NULL);
#ifdef PLATFORM_WINDOWS
	CHECK(host->CreateGameWindow != NULL && host->DestroyGameWindow != NULL);
#else
	CHECK(host->CreateGameWindow == NULL && host->DestroyGameWindow == NULL);
#endif
}

//======================================================================
// RunClientFrame
//======================================================================
#ifndef PLATFORM_WINDOWS
TEST(ClientMain2Frame, OffWindowsAnSdlQuitStopsTheLoopBeforeTheOsPump)
{
	Frame frame;
	PushQuit();
	CHECK(!RunClientFrame());
	CHECK(!g_bRunning);
	CHECK_SAME(std::string(""), calls);
	CHECK(frame.Untouched());
	// The event was drained.
	SDL_PumpEvents();
	CHECK_EQ(0, SDL_PeepEvents(nullptr, 0, SDL_PEEKEVENT, SDL_QUIT, SDL_QUIT));
}

TEST(ClientMain2Frame, OffWindowsAStoppedLoopStaysStopped)
{
	Frame frame;
	g_bRunning = false;
	CHECK(!RunClientFrame());
	CHECK(!g_bRunning);
	CHECK_SAME(std::string(""), calls);
	CHECK(frame.Untouched());
}
#else
TEST(ClientMain2Frame, OnWindowsSdlsEventsAndTheRunningFlagAreLeftAlone)
{
	// Windows: the OS pump alone runs the loop.
	Frame frame;
	g_bRunning = false;
	PushQuit();
	CHECK(RunClientFrame());
	CHECK(!g_bRunning);
	CHECK_SAME("pump;update@" + N(Ms(kStart)) + ";", calls);
	CHECK_EQ(1, SDL_PeepEvents(nullptr, 0, SDL_PEEKEVENT, SDL_QUIT, SDL_QUIT));
}
#endif

TEST(ClientMain2Frame, TheQuitMessageStopsTheLoop)
{
	Frame frame;
	pumpAnswer = ClientHost::Pumped::Quit;
	CHECK(!RunClientFrame());
	CHECK_SAME(std::string("pump;"), calls);
	CHECK(frame.Untouched());
	CHECK(g_bRunning);
}

TEST(ClientMain2Frame, ADispatchedMessageIsTheWholeFrame)
{
	Frame frame;
	pumpAnswer = ClientHost::Pumped::Dispatched;
	now = After(5000);
	CHECK(RunClientFrame());
	CHECK_SAME(std::string("pump;"), calls);
	CHECK(frame.Untouched());
}

TEST(ClientMain2Frame, WithNoMessageTheActiveGameStampsTheClockThenUpdates)
{
	Frame frame;
	now = After(400);
	CHECK(RunClientFrame());
	CHECK_SAME("pump;update@" + N(Ms(now)) + ";", calls);
	CHECK(g_FrameNow == now);
	// Within the second: the rate's window stays as it was.
	CHECK_EQ(77, g_FrameRate);
	CHECK(!g_bGoodFPS);
	CHECK(g_StartTime == kStart);
	CHECK_EQ(10, g_StartFrameCount);
}

TEST(ClientMain2Frame, WithoutAnUpdateTheFrameIsOnlyStamped)
{
	Frame frame;
	g_pUpdate = NULL;
	now = After(400);
	CHECK(RunClientFrame());
	CHECK_SAME(std::string("pump;"), calls);
	CHECK(g_FrameNow == now);
}

TEST(ClientMain2Frame, AnInactiveApplicationWaitsForAMessage)
{
	Frame frame;
	g_bActiveApp = FALSE;
	// Long past the rate's second: it is not measured either.
	now = After(5000);
	CHECK(RunClientFrame());
	CHECK_SAME(std::string("pump;wait;"), calls);
	CHECK(frame.Untouched());
}

TEST(ClientMain2Frame, TheRateIsMeasuredOnceMoreThanASecondHasPassed)
{
	Frame frame;
	frame.loop.frames = 1;
	// A second exactly: the window stays open.
	now = After(1000);
	CHECK(RunClientFrame());
	CHECK_EQ(41, g_FrameCount);
	CHECK_EQ(77, g_FrameRate);
	CHECK(!g_bGoodFPS);
	CHECK(g_StartTime == kStart);
	CHECK_EQ(10, g_StartFrameCount);
	// A millisecond more: the frames counted after this frame's update,
	// (42 - 10) * 1000 / 1001, truncated; the window opens again here.
	now = After(1001);
	CHECK(RunClientFrame());
	CHECK_EQ(42, g_FrameCount);
	CHECK_EQ(31, g_FrameRate);
	CHECK(g_bGoodFPS);
	CHECK(g_StartTime == now);
	CHECK_EQ(42, g_StartFrameCount);
	CHECK_SAME("pump;update@" + N(Ms(After(1000))) + ";pump;update@" + N(Ms(now)) + ";", calls);
	// The next second is measured from there.
	now = After(2001);
	CHECK(RunClientFrame());
	CHECK_EQ(31, g_FrameRate);
	now = After(2002);
	CHECK(RunClientFrame());
	// (44 - 42) * 1000 / 1001
	CHECK_EQ(1, g_FrameRate);
	CHECK(!g_bGoodFPS);
	CHECK_EQ(44, g_StartFrameCount);
}

TEST(ClientMain2Frame, FifteenFramesASecondIsAGoodRate)
{
	for (int frames : { 30, 29 })
	{
		Frame frame;
		g_FrameCount = 10 + frames;
		g_bGoodFPS = frames != 30;
		now = After(2000);
		CHECK(RunClientFrame());
		CHECK_EQ(frames / 2, g_FrameRate);
		CHECK_EQ(frames == 30, g_bGoodFPS);
		CHECK_EQ(10 + frames, g_StartFrameCount);
	}
}

//======================================================================
// FinishClient
//======================================================================
TEST(ClientMain2Finish, TheObjectsGoThenTheIme)
{
	Shutdown shutdown;
	FinishClient(NULL, "00000000");
	// ReleaseAllObjects first (the game ended, its loop freed, the
	// configuration reset), then the IME; no updater.
	CHECK_SAME(std::string("~game loop;~ime;"), calls);
	CHECK_EQ(FALSE, g_bActiveGame);
	CHECK(g_pCGameUpdate == NULL);
	CHECK(g_pClientConfig == NULL);
	CHECK(InDirectory(shutdown.elsewhere.path));
	CHECK(updaterDir.empty());
}

TEST(ClientMain2Finish, AnUpdatePendingRunsTheUpdaterFromTheClientsDirectory)
{
	Shutdown shutdown;
	g_bNeedUpdate = TRUE;
	FinishClient(NULL, "00000000 3");
	CHECK_SAME(std::string("~game loop;~ime;updater UPDATE 00000000 3;"), calls);
	std::error_code error;
	CHECK(std::filesystem::equivalent(updaterDir, shutdown.dir.path, error));
	// The directory is left there, and the flag set.
	CHECK(InDirectory(shutdown.dir.path));
	CHECK_EQ(TRUE, g_bNeedUpdate);
}

TEST(ClientMain2Finish, TheUpdatersCommandIsCutAt511Characters)
{
	// "UPDATE " and 504 characters fill the 512 bytes; one more is cut.
	for (size_t length : { size_t(504), size_t(505) })
	{
		Shutdown shutdown;
		g_bNeedUpdate = TRUE;
		std::string command(length, '7');
		command.back() = 'z';
		FinishClient(NULL, command.c_str());
		const std::string sent = "UPDATE " + command.substr(0, 504);
		CHECK_SAME("~game loop;~ime;updater " + sent + ";", calls);
	}
}

TEST(ClientMain2Finish, AnUnreachableClientDirectoryIsIgnored)
{
	Shutdown shutdown;
	g_bNeedUpdate = TRUE;
	Shutdown::SetCwd((shutdown.dir.path / "gone").string());
	FinishClient(NULL, "");
	CHECK_SAME(std::string("~game loop;~ime;updater UPDATE ;"), calls);
	std::error_code error;
	CHECK(std::filesystem::equivalent(updaterDir, shutdown.elsewhere.path, error));
}

#ifdef PLATFORM_WINDOWS
TEST(ClientMain2Finish, OnWindowsTheSingleInstanceMutexIsReleased)
{
	Shutdown shutdown;
	HANDLE mutex = CreateMutexA(NULL, TRUE, NULL);
	CHECK(mutex != NULL);
	FinishClient(mutex, "");
	// No longer this thread's to release.
	const BOOL released = ReleaseMutex(mutex);
	const DWORD error = GetLastError();
	CHECK(!released);
	CHECK_EQ(DWORD(ERROR_NOT_OWNER), error);
	CloseHandle(mutex);
}
#endif

//======================================================================
// InitApp and InitFailMessage
//======================================================================
#ifndef PLATFORM_WINDOWS
TEST(ClientMain2App, OffWindowsTheStartUpHidesTheCursorThenStartsTheGame)
{
	StartUp start;
	int sentinel = 0;
	g_hWnd = reinterpret_cast<HWND>(&sentinel);
	g_WindowScale = 3;
	std::memset(g_macAddress, 0xAA, sizeof(g_macAddress));
	// InitGame's throw without the file definitions says it ran.
	CHECK_SAME(std::string("FileNotExistException: Data/Info/FileDef.inf"), Thrown([] { InitApp(SW_HIDE); }));
	CHECK(g_pFileDef != NULL && g_pFileDef->getProperty("RESOURCE_TEXT_ENCODING") == "CP949");
	CHECK_EQ(SDL_DISABLE, SDL_ShowCursor(SDL_QUERY));
	// No window is asked for, and the window's globals stay.
	CHECK_SAME(std::string(""), calls);
	CHECK(g_hWnd == reinterpret_cast<HWND>(&sentinel));
	CHECK_EQ(3, g_WindowScale);
	CHECK_EQ(0xAA, int(g_macAddress[0]));
	CHECK_EQ(0xAA, int(g_macAddress[5]));

	// The failure message shows the cursor again, stops the application
	// and logs the message; no window to destroy here.
	HRESULT result = 1;
	const std::string logged = start.Logged([&result] { result = InitFailMessage("no data tree"); });
	CHECK_EQ(0, int(result));
	CHECK(logged.find("no data tree") != std::string::npos);
	CHECK_EQ(SDL_ENABLE, SDL_ShowCursor(SDL_QUERY));
	CHECK_EQ(FALSE, g_bActiveApp);
	CHECK_SAME(std::string(""), calls);
	CHECK(g_hWnd == reinterpret_cast<HWND>(&sentinel));
}

TEST(ClientMain2App, OffWindowsTheCursorCountIsClientMainsOwn)
{
	// Recorded defect (basic/Platform.h:653): this file's hide is still
	// outstanding when InitFailMessage's show makes the cursor visible.
	StartUp start;
	ShowCursor(FALSE);
	CHECK_EQ(SDL_DISABLE, SDL_ShowCursor(SDL_QUERY));
	Thrown([] { InitApp(SW_HIDE); });
	CHECK_EQ(SDL_DISABLE, SDL_ShowCursor(SDL_QUERY));
	InitFailMessage("no data tree");
	CHECK_EQ(SDL_ENABLE, SDL_ShowCursor(SDL_QUERY));
	ShowCursor(TRUE);
	CHECK_EQ(SDL_ENABLE, SDL_ShowCursor(SDL_QUERY));
}
#else
TEST(ClientMain2App, OnWindowsWithoutTheWindowItFailsBeforeTheGame)
{
	// Full screen: a popup the size of the screen. In a window: the frame
	// at the largest scale the work area holds, with its borders and menu.
	StartUp start;
	const BYTE cleared[6] = { 0, 0, 0, 0, 0, 0 };
	BYTE adapter[6] = { 0, 0, 0, 0, 0, 0 };
	GetMacAddressFromNetBIOS(adapter);
	const int cursor = CursorCount();
	for (bool fullScreen : { true, false })
	{
		calls.clear();
		g_bFullScreen = fullScreen;
		g_WindowScale = 9;
		int sentinel = 0;
		g_hWnd = reinterpret_cast<HWND>(&sentinel);
		std::memset(g_macAddress, 0xAA, sizeof(g_macAddress));
		CHECK_EQ(FALSE, InitApp(SW_HIDE));
		CHECK(g_hWnd == NULL);
		// Cleared, then the adapter's when NetBIOS answers (this runner's
		// NetBIOS is not the test's to choose).
		CHECK(std::memcmp(g_macAddress, cleared, sizeof(cleared)) == 0 || std::memcmp(g_macAddress, adapter, sizeof(adapter)) == 0);
		CHECK_EQ(cursor, CursorCount());
		CHECK(g_pFileDef == NULL);		// InitGame did not run
		if (fullScreen)
		{
			CHECK_SAME("create " + N(WS_EX_APPWINDOW) + "," + N(WS_POPUP | WS_CLIPCHILDREN) + ","
				+ N(GetSystemMetrics(SM_CXSCREEN)) + "," + N(GetSystemMetrics(SM_CYSCREEN)) + ";", calls);
			CHECK_EQ(9, g_WindowScale);
		}
		else
		{
			RECT work = { 0, 0, 0, 0 };
			SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
			const int frameW = GetSystemMetrics(SM_CXSIZEFRAME) * 2;
			const int frameH = GetSystemMetrics(SM_CYSIZEFRAME) * 2 + GetSystemMetrics(SM_CYMENU);
			const int scale = ChooseWindowScale(800, 600, frameW, frameH, work.right - work.left, work.bottom - work.top);
			CHECK_EQ(scale, g_WindowScale);
			CHECK_SAME("create " + N(WS_EX_APPWINDOW) + ","
				+ N(WS_CLIPCHILDREN | WS_OVERLAPPED | WS_THICKFRAME | WS_MINIMIZEBOX | WS_SYSMENU) + ","
				+ N(800 * scale + frameW) + "," + N(600 * scale + frameH) + ";", calls);
		}
	}
}

TEST(ClientMain2App, OnWindowsTheWindowIsShownWithoutTheCursorThenTheGameStarts)
{
	StartUp start;
	// A message-only window of the test's: nothing appears.
	HWND window = CreateWindowExA(0, "STATIC", "", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);
	CHECK(window != NULL);
	windowAnswer = window;
	g_bFullScreen = true;
	HCURSOR arrow = GetCursor();
	const int cursor = CursorCount();
	CHECK_SAME(std::string("FileNotExistException: Data/Info/FileDef.inf"), Thrown([] { InitApp(SW_HIDE); }));
	CHECK(g_hWnd == window);
	CHECK_EQ(cursor - 1, CursorCount());

	// The failure message: the cursor back, the application stopped, the
	// window destroyed (and still named: recorded defect).
	calls.clear();
	HRESULT result = 1;
	const std::string logged = start.Logged([&result] { result = InitFailMessage("no data tree"); });
	CHECK_EQ(0, int(result));
	CHECK(logged.find("no data tree") != std::string::npos);
	CHECK_EQ(cursor, CursorCount());
	CHECK_EQ(FALSE, g_bActiveApp);
	CHECK_SAME("destroy " + H(window) + ";", calls);
	CHECK(g_hWnd == window);
	SetCursor(arrow);
	DestroyWindow(window);
}
#endif

//======================================================================
// WindowProc
//======================================================================
TEST(ClientMain2WindowProc, TheMciNotificationIsAnswered)
{
	CHECK_EQ(0, int(WindowProc(NULL, MM_MCINOTIFY, 1, 0)));
}

TEST(ClientMain2WindowProc, TheSystemMenuKeysAndTaskSwitchesAreSwallowed)
{
	Proc proc;
	for (bool fullScreen : { true, false })
	{
		Proc::FullScreen(fullScreen);
		for (WPARAM command : { WPARAM(SC_HOTKEY), WPARAM(SC_KEYMENU), WPARAM(SC_TASKLIST), WPARAM(SC_PREVWINDOW), WPARAM(SC_NEXTWINDOW) })
			CHECK_EQ(1, int(Proc::Command(command)));
	}
}

TEST(ClientMain2WindowProc, InFullScreenMovingSizingMaximisingAndThePowerAreSwallowed)
{
	Proc proc;
	Proc::FullScreen(true);
	for (WPARAM command : { WPARAM(SC_MOVE), WPARAM(SC_SIZE), WPARAM(SC_MAXIMIZE), WPARAM(SC_MONITORPOWER) })
		CHECK_EQ(1, int(Proc::Command(command)));
}

#ifndef PLATFORM_WINDOWS
TEST(ClientMain2WindowProc, OffWindowsTheOtherCommandsGoToTheDefaultProcedure)
{
	Proc proc;
	// In a window: moving, sizing, maximising and the power.
	Proc::FullScreen(false);
	for (WPARAM command : { WPARAM(SC_MOVE), WPARAM(SC_SIZE), WPARAM(SC_MAXIMIZE), WPARAM(SC_MONITORPOWER) })
		CHECK_EQ(0, int(Proc::Command(command)));
	// Closing (Alt+F4) in either.
	CHECK_EQ(0, int(Proc::Command(SC_CLOSE)));
	Proc::FullScreen(true);
	CHECK_EQ(0, int(Proc::Command(SC_CLOSE)));
	// Recorded defect: the command is not masked with 0xFFF0, so a move by
	// the title bar (SC_MOVE | HTCAPTION, 2) or a size by the left border
	// (SC_SIZE | WMSZ_LEFT, 1) passes in full screen too.
	CHECK_EQ(0, int(Proc::Command(SC_MOVE | 2)));
	CHECK_EQ(0, int(Proc::Command(SC_SIZE | 1)));
}
#endif

TEST(ClientMain2WindowProc, AnActivationRunsCheckActivateThenSetsTheGamesActivity)
{
	Proc proc;
	CSDLInput* inputBefore = g_pSDLInput;
	MPlayer* playerBefore = g_pPlayer;
	const bool muted = g_SDLAudio.IsMute();
	const MMusic music = g_Music;
	CSDLInput input;
	g_pSDLInput = &input;
	g_pPlayer = NULL;

	// The application not active yet: CheckActivate flushes the input and
	// leaves the game inactive, then WindowProc marks it active - the
	// recorded defect.
	g_bActiveApp = FALSE;
	g_bActiveGame = FALSE;
	g_double_click_time = 1;
	input.m_mouse_z = 3;
	const LRESULT activated = WindowProc(NULL, WM_ACTIVATEAPP, TRUE, 0);
	CHECK_EQ(0, int(input.m_mouse_z));
	CHECK_EQ(TRUE, g_bActiveGame);
	CHECK_EQ(DWORD(GetDoubleClickTime()), g_double_click_time);

	// Deactivated while active: CheckActivate is told so (it mutes the
	// sound), and the game is inactive.
	g_bActiveApp = TRUE;
	g_SDLAudio.UnSetMute();
	input.m_mouse_z = 3;
	const LRESULT deactivated = WindowProc(NULL, WM_ACTIVATEAPP, FALSE, 0);
	CHECK_EQ(0, int(input.m_mouse_z));
	CHECK(g_SDLAudio.IsMute());
	CHECK_EQ(FALSE, g_bActiveGame);
#ifndef PLATFORM_WINDOWS
	CHECK_EQ(0, int(activated));
	CHECK_EQ(0, int(deactivated));
#else
	(void)activated;
	(void)deactivated;
#endif

	g_Music = music;
	if (muted) g_SDLAudio.SetMute();
	else g_SDLAudio.UnSetMute();
	g_pPlayer = playerBefore;
	g_pSDLInput = inputBefore;
}

TEST(ClientMain2WindowProc, TheDestroyedWindowQuits)
{
	CHECK_EQ(0, int(WindowProc(NULL, WM_DESTROY, 0, 0)));
#ifdef PLATFORM_WINDOWS
	// The quit message, code 0, waits in this thread's queue: taken out.
	MSG msg{};
	CHECK(PeekMessageA(&msg, NULL, WM_QUIT, WM_QUIT, PM_REMOVE));
	CHECK_EQ(WM_QUIT, int(msg.message));
	CHECK_EQ(0, int(msg.wParam));
#endif
}

TEST(ClientMain2WindowProc, TheWindowIsPinnedToTheScaledFrame)
{
	Proc proc;
	const int frameW = GetSystemMetrics(SM_CXSIZEFRAME) * 2;
	const int frameH = GetSystemMetrics(SM_CYSIZEFRAME) * 2 + GetSystemMetrics(SM_CYMENU);
	for (int scale : { 1, 2 })
	{
		g_GameRect = {0, 0, 1024, 768};
		g_WindowScale = scale;
		MINMAXINFO info{};
		info.ptMaxSize.x = 11;
		info.ptMaxPosition.y = 12;
		const LRESULT result = WindowProc(NULL, WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&info));
		CHECK_EQ(1024 * scale + frameW, int(info.ptMinTrackSize.x));
		CHECK_EQ(768 * scale + frameH, int(info.ptMinTrackSize.y));
		CHECK_EQ(1024 * scale + frameW, int(info.ptMaxTrackSize.x));
		CHECK_EQ(768 * scale + frameH, int(info.ptMaxTrackSize.y));
#ifndef PLATFORM_WINDOWS
		CHECK_EQ(0, int(result));
		CHECK_EQ(11, int(info.ptMaxSize.x));
		CHECK_EQ(12, int(info.ptMaxPosition.y));
		CHECK_EQ(0, int(info.ptReserved.x));
#else
		(void)result;
#endif
	}
}

#ifndef PLATFORM_WINDOWS
TEST(ClientMain2WindowProc, OffWindowsAMoveAndOtherMessagesGoToTheDefaultProcedure)
{
	Proc proc;
	Proc::FullScreen(false);
	g_bActiveApp = TRUE;
	CHECK_EQ(0, int(WindowProc(NULL, WM_MOVE, 0, MAKELPARAM(10, 20))));
	CHECK_EQ(0, int(WindowProc(NULL, WM_CLOSE, 0, 0)));
	CHECK_EQ(0, int(WindowProc(NULL, WM_USER, 5, 6)));
}
#endif

TEST(ClientMain2WindowProc, TheTimerUpdatesTheMusicStreamWhileTheAudioIsUp)
{
	COGGSTREAM* streamBefore = g_pOGG;
	const bool audioUp = g_SDLAudio.*Get(AudioUpOf());
	COGGSTREAM stream(NULL, NULL, 0, 0, 0, 0);
	// Nothing plays here, so an update finds the stream stopped.
	for (bool up : { false, true })
	{
		g_SDLAudio.*Get(AudioUpOf()) = up;
		g_pOGG = &stream;
		stream.*Get(StreamingOf()) = true;
		const LRESULT result = WindowProc(NULL, WM_TIMER, 1, 0);
		CHECK_EQ(!up, stream.*Get(StreamingOf()));
		// Without a stream nothing is updated.
		g_pOGG = NULL;
		WindowProc(NULL, WM_TIMER, 1, 0);
#ifndef PLATFORM_WINDOWS
		CHECK_EQ(0, int(result));
#else
		(void)result;
#endif
	}
	g_SDLAudio.*Get(AudioUpOf()) = audioUp;
	g_pOGG = streamBefore;
}

TEST(ClientMain2WindowProc, KeysAndCharactersGoToTheUiAndAreAnswered)
{
	Keys keys;
	CHECK_EQ(0, int(WindowProc(NULL, WM_KEYDOWN, 'A', kKeyBits)));
	CHECK_EQ(0, int(WindowProc(NULL, WM_CHAR, 'a', kKeyBits)));
	CHECK_EQ(0, int(WindowProc(NULL, WM_IME_STARTCOMPOSITION, 0, 0)));
	CHECK_SAME("key " + N(WM_KEYDOWN) + ":65," + N(kKeyBits) + ";key " + N(WM_CHAR) + ":97," + N(kKeyBits)
		+ ";key " + N(WM_IME_STARTCOMPOSITION) + ":0,0;", calls);
}

TEST(ClientMain2WindowProc, AKeyUpGoesToTheUiThenTheDefaultProcedure)
{
	Keys keys;
	const LRESULT up = WindowProc(NULL, WM_KEYUP, 'A', kKeyBits);
	CHECK_SAME("key " + N(WM_KEYUP) + ":65," + N(kKeyBits) + ";", calls);
#ifndef PLATFORM_WINDOWS
	CHECK_EQ(0, int(up));
#else
	(void)up;
#endif
}

#ifndef PLATFORM_WINDOWS
TEST(ClientMain2WindowProc, OffWindowsTheImesOtherMessagesGoToTheUiThenTheDefaultProcedure)
{
	// Not sent on Windows: the real default procedure hands them to the
	// thread's IME window.
	Keys keys;
	CHECK_EQ(0, int(WindowProc(NULL, WM_IME_COMPOSITION, 0xB0A1, 0x80)));
	CHECK_EQ(0, int(WindowProc(NULL, WM_IME_ENDCOMPOSITION, 0, 0)));
	CHECK_SAME("key " + N(WM_IME_COMPOSITION) + ":45217,128;key " + N(WM_IME_ENDCOMPOSITION) + ":0,0;", calls);
}
#endif

TEST(ClientMain2WindowProc, ScrollLockTakesAScreenshotAfterTheUiSeesIt)
{
	Keys keys;
	ClientConfig* configBefore = g_pClientConfig;
	Properties* fileDefBefore = g_pFileDef;
	const int numberBefore = g_ScreenShotNumber;
	const std::filesystem::path workingDir = std::filesystem::current_path();
	{
		TempDir dir;
		std::filesystem::current_path(dir.path);
		ClientConfig config;
		config.MAX_SCREENSHOT = 5;
		g_pClientConfig = &config;
		Properties files;
		files.setProperty("PATH_SCREENSHOT", "ScreenShot/");
		g_pFileDef = &files;
		g_ScreenShotNumber = 3;

		// Another key takes none.
		CHECK_EQ(0, int(WindowProc(NULL, WM_KEYDOWN, VK_RETURN, kKeyBits)));
		CHECK_EQ(3, g_ScreenShotNumber);
		CHECK(!std::filesystem::exists(dir.path / "ScreenShot"));

		CHECK_EQ(0, int(WindowProc(NULL, WM_KEYDOWN, VK_SCROLL, kKeyBits)));
		// MakeScreenShot's marks: its directory, the number taken.
		CHECK(std::filesystem::is_directory(dir.path / "ScreenShot"));
		CHECK_EQ(4, g_ScreenShotNumber);
		CHECK_SAME("key " + N(WM_KEYDOWN) + ":" + N(VK_RETURN) + "," + N(kKeyBits) + ";key " + N(WM_KEYDOWN) + ":"
			+ N(VK_SCROLL) + "," + N(kKeyBits) + ";", calls);

		std::error_code error;
		std::filesystem::current_path(workingDir, error);
	}
	g_ScreenShotNumber = numberBefore;
	g_pFileDef = fileDefBefore;
	g_pClientConfig = configBefore;
}
