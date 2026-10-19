#include "test_framework.h"
#include "ClientMain.h"
#include "ClientMode.h"
#include "GameInit.h"
#include "GameInitInfoHost.h"
#include "GameMain.h"
#include "GameMessages.h"
#include "MGameStringTable.h"
#include "InputState.h"
#include "MathTable.h"
#include "ViewSectors.h"
#include "AppendPatchInfo.h"
#include "HelpEvent.h"
#include "Netmarble.h"
#include "SpriteLib/MLoadingSPKWorkNode.h"
#include "DXLib/CDirectDraw.h"
#include "ClientConfig.h"
#include "Exception.h"
#include "Properties.h"
#include "Ci.h"
#include "DataPath.h"
#include "DebugLog.h"
#include "DisplaySettings.h"
#include "TextEncoding.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

// ClientMain, the game's entry point (task 4.265 moved it, as
// Client/ClientMain.cpp, into gameclient), end to end as far as it runs
// headless. In order it:
// - installs five hosts (help mail, the Netmarble refusal, the append
//   patches' progress, the sprite-pack opener, the start-up loading's
//   GameInitInfoHost) and the back-buffer source, on every call;
// - settles the working directory: Windows the executable's; elsewhere
//   the first of the working directory, the executable's directory and
//   the one three above it that holds Data/Info/FileDef.inf (none: a
//   message, and it stays where it is); g_CWD records it;
// - turns the argument into the launch command (DisplaySettings: an
//   empty or blank one becomes "000000000" and the display mode
//   UserSet/Display.ini names), applies the last character as the display
//   mode and cuts it, whatever it is;
// - Windows only: the single-instance mutex, Process Explorer (-1),
//   the launcher window, the Updater2.exe swap and the DLL whitelist
//   (ListDllNames, then FindUnlistedDll: -1 on a hit);
// - clears g_bNeedUpdate, loads the file definitions and the Netmarble
//   switch they name (a missing file or key throws out of ClientMain);
// - sorts the command: under eight characters the updater gets it
//   (after ReleaseAllObjects) and ClientMain returns 0; first eight not
//   "00000000": returns 0 at once; otherwise a game launch: the ninth
//   character's world (not for Netmarble), full screen, no NPC limit,
//   HAL, music, the Futec address, then the display mode's full screen;
// - applies the append patches (no patch file: ReleaseAllObjects and 0),
//   swaps in Updater2.exe, empties "<cwd>\Update", the Netmarble refusal
//   (-1 for Netmarble=1), the character input for the language, then
//   InitApp: the window (Windows), the cursor and InitGame; the frame
//   loop; FinishClient.
//
// Off Windows a game launch gets as far as InitGame's first data file,
// the nickname string table (FILE_INFO_NICKNAME_STRING): without it
// InitGame fails, InitApp with it, and FinishClient ends ClientMain with
// 0. The rest needs the real data tree: String.inf (unless English), the
// client configuration, the information tables, the user options, sound,
// input, the SDL window, the game objects, the UI's sprites and the
// update loop; then g_pTopView->Init() and RunClientFrame until SDL_QUIT,
// with the auto-login (ParsingRealServer's fields into
// g_pUserInformation) and the language's user information in between.
// FinishClient's "UPDATE <command>" hand-off needs g_bNeedUpdate, which
// ClientMain clears and only the running game sets.
//
// On Windows ClientMain works in the executable's directory, so a test
// cannot point it at a data tree of its own; there it is tested up to
// the file definitions, which the test binary's directory does not hold:
// the DLL whitelist's verdict. The window (ClientHost::CreateGameWindow,
// which InitApp asks for before InitGame) is not reached.
//
// Not run (undefined behaviour): a game launch, not Netmarble's, whose
// command has a space, is 16 characters or more and carries fewer than
// four '|' fields after the space: ParsingRealServer, called at
// ClientMain.cpp:1575, reads the fields it never wrote (230-233). Every
// game launch below either stops before it or has no space.

extern BOOL		g_MyFull;		// ClientMode.cpp's
extern RECT		g_GameRect;
extern int		g_Dimension;
extern bool		g_bFullScreen;
extern BOOL		g_bActiveApp;

namespace {

//----------------------------------------------------------------------
// The recording host
//----------------------------------------------------------------------
std::vector<std::string>	s_events;
std::filesystem::path		s_updaterDirectory;
std::vector<std::string>	s_dllNames;
bool						s_dllListed = true;

const ClientHost::Host kRecorder = {
	.Exit = [](int code) { s_events.push_back("exit " + std::to_string(code)); },
	.RunUpdater = [](const char* arguments) {
		s_events.push_back(std::string("updater ") + arguments);
		std::error_code error;
		s_updaterDirectory = std::filesystem::current_path(error);
	},
	.ShowMessageBox = [](const char* text, const char* caption) {
		s_events.push_back(std::string("box ") + caption + ": " + text);
	},
	.CreateGameWindow = [](DWORD, DWORD, int, int) -> HWND {
		s_events.push_back("window");
		return NULL;
	},
	.DestroyGameWindow = [](HWND) { s_events.push_back("destroy"); },
	.PumpMessage = []() -> ClientHost::Pumped {
		s_events.push_back("pump");
		return ClientHost::Pumped::Quit;
	},
	.WaitForMessage = [] { s_events.push_back("wait"); },
	.ListDllNames = [](std::vector<std::string>& names) -> bool {
		s_events.push_back("dlls");
		names.insert(names.end(), s_dllNames.begin(), s_dllNames.end());
		return s_dllListed;
	},
};

//----------------------------------------------------------------------
// Everything ClientMain changes, restored afterwards
//----------------------------------------------------------------------
struct Saved
{
	std::filesystem::path	directory = std::filesystem::current_path();
	std::string				cwd = g_CWD;
	DisplaySettings			display = GetDisplaySettings();
	BOOL					myFull = g_MyFull;
	RECT					gameRect = g_GameRect;
	int						cursorX = g_x, cursorY = g_y;
	LONG					sectors[6] = { g_SECTOR_WIDTH, g_SECTOR_HEIGHT, g_SECTOR_WIDTH_HALF,
								g_SECTOR_HEIGHT_HALF, g_SECTOR_SKIP_PLAYER_LEFT, g_SECTOR_SKIP_PLAYER_UP };
	LONG					tiles[8] = { g_TILESURFACE_SECTOR_WIDTH, g_TILESURFACE_SECTOR_HEIGHT,
								g_TILESURFACE_SECTOR_OUTLINE_RIGHT, g_TILESURFACE_SECTOR_OUTLINE_DOWN,
								g_TILESURFACE_WIDTH, g_TILESURFACE_HEIGHT,
								g_TILESURFACE_OUTLINE_RIGHT, g_TILESURFACE_OUTLINE_DOWN };
	int						needUpdate = g_bNeedUpdate;
	int						dimension = g_Dimension;
	bool					fullScreen = g_bFullScreen;
	int						maxNPC = g_MaxNPC;
	bool					hal = g_bHAL;
	bool					music = g_bMusicSW;
	std::string				futecIP = g_FutecIP;
	unsigned int			futecPort = g_FutecPort;
	BOOL					activeApp = g_bActiveApp;
	int						activeGame = g_bActiveGame;
	Properties*				fileDef = g_pFileDef;
	CI*						ci = gC_ci;
	HWND					progress = g_hWndProgress;
	TextEncoding::Encoding	encoding = TextEncoding::GetResourceEncoding();
	bool					logging = log_is_enabled(LOG_LEVEL_ERROR);
	// InitGame's FCreateSines scales the arctangent table in place, so
	// every call doubles it (MathTable.cpp:903-906).
	std::vector<int>		sines = Table(MathTable::FSinTab, MathTable::MAX_ANGLE);
	std::vector<int>		cosines = Table(MathTable::FCosTab, MathTable::MAX_ANGLE);
	std::vector<int>		arcTangents = Table(MathTable::FArcTanTab, MathTable::MAX_ANGLE + 1);
	const ClientHost::Host*	host = ClientHost::SetHost(&kRecorder);
	const GameInitInfoHost::Host* initInfo = GameInitInfoHost::SetHost(nullptr);

	Saved()
	{
		s_events.clear();
		s_updaterDirectory.clear();
		s_dllNames.clear();
		s_dllListed = true;
	}

	static std::vector<int> Table(const int* table, int size)
	{
		return std::vector<int>(table, table + size);
	}

	~Saved()
	{
		std::copy(arcTangents.begin(), arcTangents.end(), MathTable::FArcTanTab);
		std::copy(cosines.begin(), cosines.end(), MathTable::FCosTab);
		std::copy(sines.begin(), sines.end(), MathTable::FSinTab);
		// ClientMain's own hosts, installed on every call.
		SetHelpHost(nullptr);
		SetNetmarbleHost(nullptr);
		SetAppendPatchHost(nullptr);
		SetLoadingSPKHost(nullptr);
		CSDLGraphics::SetBackBufferSource(nullptr);
		GameInitInfoHost::SetHost(initInfo);
		ClientHost::SetHost(host);
		// A path that returns early leaves the file definitions allocated;
		// the others release them (NULL). gC_ci is never deleted here:
		// FinishClient deletes it and leaves the pointer.
		if (g_pFileDef != fileDef)
			delete g_pFileDef;
		g_pFileDef = fileDef;
		gC_ci = ci;
		g_hWndProgress = progress;
		TextEncoding::SetResourceEncoding(encoding);
		if (!logging && log_is_enabled(LOG_LEVEL_ERROR))
			log_cleanup();
		g_bActiveGame = activeGame;
		g_bActiveApp = activeApp;
		g_FutecPort = futecPort;
		std::snprintf(g_FutecIP, sizeof(g_FutecIP), "%s", futecIP.c_str());
		g_bMusicSW = music;
		g_bHAL = hal;
		g_MaxNPC = maxNPC;
		g_bFullScreen = fullScreen;
		g_Dimension = dimension;
		g_bNeedUpdate = needUpdate;
		g_TILESURFACE_SECTOR_WIDTH = tiles[0];
		g_TILESURFACE_SECTOR_HEIGHT = tiles[1];
		g_TILESURFACE_SECTOR_OUTLINE_RIGHT = tiles[2];
		g_TILESURFACE_SECTOR_OUTLINE_DOWN = tiles[3];
		g_TILESURFACE_WIDTH = tiles[4];
		g_TILESURFACE_HEIGHT = tiles[5];
		g_TILESURFACE_OUTLINE_RIGHT = tiles[6];
		g_TILESURFACE_OUTLINE_DOWN = tiles[7];
		g_SECTOR_WIDTH = sectors[0];
		g_SECTOR_HEIGHT = sectors[1];
		g_SECTOR_WIDTH_HALF = sectors[2];
		g_SECTOR_HEIGHT_HALF = sectors[3];
		g_SECTOR_SKIP_PLAYER_LEFT = sectors[4];
		g_SECTOR_SKIP_PLAYER_UP = sectors[5];
		g_x = cursorX;
		g_y = cursorY;
		g_GameRect = gameRect;
		g_MyFull = myFull;
		GetDisplaySettings() = display;
		std::snprintf(g_CWD, _MAX_PATH, "%s", cwd.c_str());
		std::error_code error;
		std::filesystem::current_path(directory, error);
	}
};

int Run(const char* argument)
{
	std::string command = argument;
	return ClientMain(command.data(), 1);
}

// What ClientMain threw: "<name>: <message>", or "returned <code>".
std::string RunThrowing(const char* argument)
{
	try
	{
		return "returned " + std::to_string(Run(argument));
	}
	catch (const Throwable& t)
	{
		return t.getName() + ": " + t.getMessage();
	}
}

#ifndef PLATFORM_WINDOWS

const char* const kNetmarbleRefusal =
	"box NetmarbleDarkEden: 넷마블 홈페이지를 통해서 다시 실행해주세요.";

// The errors ClientMain itself logs (LOG_ERROR in ClientMain.cpp).
int s_clientMainErrors = 0;
void CountClientMainErrors(const LogSite& site, LogLevel level)
{
	if (level == LOG_LEVEL_ERROR && site.file != NULL
		&& std::string_view(site.file).ends_with("ClientMain.cpp"))
		s_clientMainErrors++;
}

//----------------------------------------------------------------------
// A data tree in a temporary directory, made the working directory
//----------------------------------------------------------------------
const char* const kFileDef =
	"FILE_INFO_NETMARBLE : Data\\Info\\Netmarble.inf\n"
	"FILE_INFO_APPENDPATCH : Data\\Info\\AppendPatch.inf\n"
	"FILE_LANGUAGE_INFO : Data\\Info\\Language.inf\n"
	"FILE_INFO_NICKNAME_STRING : Data\\Info\\NickName.inf\n";

struct Launch : Saved
{
	std::filesystem::path	base;
	std::filesystem::path	root;

	// files: with the file definitions and the Netmarble switch (off);
	// patches: with an empty append-patch table too.
	explicit Launch(bool files = true, bool patches = false)
	{
		static unsigned sequence = 0;
		base = std::filesystem::temp_directory_path() / ("darkeden_client_main_3_"
			+ std::to_string(MonotonicClock::Now().time_since_epoch().count())
			+ "_" + std::to_string(sequence++));
		root = base / "tree";
		std::filesystem::create_directories(root);
		std::filesystem::current_path(root);
		if (files)
		{
			Write("Data/Info/FileDef.inf", kFileDef);
			Write("Data/Info/Netmarble.inf", "Netmarble : 0\n");
		}
		if (patches)
			Write("Data/Info/AppendPatch.inf", std::string(4, '\0'));
	}

	~Launch()
	{
		std::error_code error;
		std::filesystem::current_path(directory, error);
		std::filesystem::remove_all(base, error);
	}

	void Write(const std::filesystem::path& path, const std::string& bytes) const
	{
		const std::filesystem::path file = root / path;
		std::filesystem::create_directories(file.parent_path());
		std::ofstream out(file, std::ios::binary | std::ios::trunc);
		out << bytes;
	}

	std::string Read(const std::filesystem::path& path) const
	{
		std::ifstream in(root / path, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	}

	bool Has(const std::filesystem::path& path) const
	{
		return std::filesystem::exists(root / path);
	}

	std::string Canonical() const
	{
		return std::filesystem::canonical(root).string();
	}
};

// The state a game launch sets, poisoned so each value is seen to change.
void Poison()
{
	g_Dimension = 5;
	g_bFullScreen = false;
	g_MaxNPC = 7;
	g_bHAL = false;
	g_bMusicSW = false;
	g_bNeedUpdate = TRUE;
	g_bActiveApp = TRUE;
	g_bActiveGame = TRUE;
	g_MyFull = TRUE;
	std::snprintf(g_FutecIP, sizeof(g_FutecIP), "%s", "9.9.9.9");
	g_FutecPort = 1;
}

//======================================================================
// The launch command
//======================================================================

// An empty or blank argument is a game launch for world 0 in the display
// mode UserSet/Display.ini names (none: '2', 800x600 full screen). Every
// game launch here stops at the append patches, which are missing: the
// failed open's InitFailMessage stops the application, ReleaseAllObjects
// runs (the game inactive, the file definitions released) and ClientMain
// returns 0 with nothing asked of the host.
TEST(ClientMain3Command, AnEmptyArgumentIsAGameLaunchForWorldZero)
{
	for (const char* argument : { "", " \t" })
	{
		Launch launch;
		Poison();

		CHECK_EQ(0, Run(argument));

		CHECK(s_events.empty());
		CHECK_EQ(0, g_Dimension);
		CHECK(g_bFullScreen);
		CHECK_EQ(FALSE, g_MyFull);
		CHECK_EQ(0, g_MaxNPC);
		CHECK(g_bHAL);
		CHECK(g_bMusicSW);
		CHECK_EQ(FALSE, g_bNeedUpdate);
		CHECK_EQ(FALSE, g_bActiveApp);
		CHECK_EQ(FALSE, g_bActiveGame);
		CHECK(g_pFileDef == NULL);
		CHECK(std::string(g_FutecIP) == "9.9.9.9");
		CHECK(GetDisplaySettings().fullscreen);
		CHECK(!GetDisplaySettings().highResolution);
		// The data tree is the working directory, and g_CWD says so.
		CHECK(std::filesystem::current_path().string() == launch.Canonical());
		CHECK(std::string(g_CWD) == launch.Canonical());
	}
}

TEST(ClientMain3Command, AnEmptyArgumentTakesTheDisplayModeOfDisplayIni)
{
	Launch launch;
	launch.Write("UserSet/Display.ini", "Fullscreen=0\nResolution=1024x768\n");
	Poison();
	g_bFullScreen = true;
	g_MyFull = FALSE;

	CHECK_EQ(0, Run(""));

	// "0000000003": world 0, 1024x768 in a window.
	CHECK_EQ(0, g_Dimension);
	CHECK(!g_bFullScreen);
	CHECK_EQ(TRUE, g_MyFull);
	CHECK_EQ(1024, g_GameRect.right);
	CHECK_EQ(768, g_GameRect.bottom);
	CHECK_EQ(512, g_x);
	CHECK_EQ(384, g_y);
	CHECK_EQ(21, g_SECTOR_WIDTH);
	CHECK_EQ(1296, g_TILESURFACE_WIDTH);
	CHECK(!GetDisplaySettings().fullscreen);
	CHECK(GetDisplaySettings().highResolution);
}

// Under eight characters (after the cut) the command goes to the updater
// as it is: the objects are released, the working directory is g_CWD, and
// ClientMain returns 0 without a game launch's settings.
TEST(ClientMain3Command, AShortCommandHandsOverToTheUpdater)
{
	Launch launch;
	Poison();
	g_bFullScreen = true;
	g_MyFull = TRUE;
	const LogSiteObserver previous = log_set_site_observer(CountClientMainErrors);
	s_clientMainErrors = 0;

	CHECK_EQ(0, Run("abc1"));

	log_set_site_observer(previous);
	CHECK(s_events == std::vector<std::string>{ "updater abc" });
	CHECK(s_updaterDirectory.string() == launch.Canonical());
	CHECK(std::string(g_CWD) == launch.Canonical());
	CHECK(g_pFileDef == NULL);
	CHECK_EQ(FALSE, g_bActiveGame);
	CHECK_EQ(FALSE, g_bNeedUpdate);
	// The mode digit is applied before the hand-off ('1': 800x600 in a
	// window, and the settings follow it); the game's settings are not.
	CHECK_EQ(FALSE, g_MyFull);
	CHECK(!GetDisplaySettings().fullscreen);
	CHECK(g_bFullScreen);
	CHECK_EQ(5, g_Dimension);
	CHECK_EQ(7, g_MaxNPC);
	CHECK(!g_bHAL);
	// A failed mutex release is logged for the empty command only.
	CHECK_EQ(0, s_clientMainErrors);
}

TEST(ClientMain3Command, TheUpdaterBoundaryIsEightCharacters)
{
	{
		// Seven after the cut: the updater.
		Launch launch;
		CHECK_EQ(0, Run("00000001"));
		CHECK(s_events == std::vector<std::string>{ "updater 0000000" });
	}
	{
		// Eight: a game launch (here stopped at the missing patches).
		Launch launch;
		Poison();
		CHECK_EQ(0, Run("000000001"));
		CHECK(s_events.empty());
		CHECK_EQ(0, g_MaxNPC);
		// No ninth character: the world stays.
		CHECK_EQ(5, g_Dimension);
	}
}

// A one-character argument is all display mode: the command left is empty,
// the updater gets "" and the failed release of the (off Windows absent)
// mutex is logged.
TEST(ClientMain3Command, AOneCharacterArgumentLeavesAnEmptyCommand)
{
	Launch launch;
	const LogSiteObserver previous = log_set_site_observer(CountClientMainErrors);
	s_clientMainErrors = 0;

	CHECK_EQ(0, Run("x"));

	log_set_site_observer(previous);
	CHECK(s_events == std::vector<std::string>{ "updater " });
	CHECK_EQ(1, s_clientMainErrors);
	CHECK(g_pFileDef == NULL);
}

// Eight characters or more whose first eight are not "00000000": refused,
// 0 at once - nothing released, nothing asked, no game settings.
// Recorded defect: this return (ClientMain.cpp:1409-1410) leaves the file
// definitions ClientMain just loaded allocated in g_pFileDef, and gives
// neither a message nor a log line.
TEST(ClientMain3Command, ACommandNotStartingWithEightZerosIsRefused)
{
	for (const char* argument : { "1000000001", "0000x00001", "0000000x01" })
	{
		Launch launch;
		Poison();

		CHECK_EQ(0, Run(argument));

		CHECK(s_events.empty());
		CHECK(g_pFileDef != NULL && g_pFileDef != launch.fileDef);
		if (g_pFileDef != NULL)
			CHECK(g_pFileDef->getProperty("FILE_INFO_NETMARBLE") == "Data/Info/Netmarble.inf");
		CHECK_EQ(TRUE, g_bActiveGame);
		CHECK_EQ(TRUE, g_bActiveApp);
		CHECK_EQ(FALSE, g_bNeedUpdate);
		CHECK_EQ(5, g_Dimension);
		CHECK(!g_bFullScreen);
		CHECK_EQ(7, g_MaxNPC);
		CHECK(!g_bHAL);
		CHECK(!g_bMusicSW);
	}
}

// A game launch's ninth character, when a digit, names the world.
TEST(ClientMain3Command, TheNinthCharacterNamesTheWorld)
{
	const struct { const char* argument; int world; } cases[] = {
		{ "0000000031", 3 },
		{ "0000000091", 9 },
		{ "0000000001", 0 },
		{ "00000000a1", 5 },
		{ "00000000/1", 5 },
		{ "00000000:1", 5 },
	};
	for (const auto& c : cases)
	{
		Launch launch;
		Poison();
		CHECK_EQ(0, Run(c.argument));
		CHECK_EQ(c.world, g_Dimension);
		CHECK_EQ(0, g_MaxNPC);
	}
}

// The last character is the display mode, and is cut whatever it is.
// '2' and '4' are full screen, '1' and '3' a window; anything else leaves
// the display settings' choice.
TEST(ClientMain3Command, TheLastCharacterIsTheDisplayMode)
{
	const struct { const char* argument; bool fullScreen; BOOL highResolution; } cases[] = {
		{ "0000000041", false, FALSE },
		{ "0000000042", true, FALSE },
		{ "0000000043", false, TRUE },
		{ "0000000044", true, TRUE },
	};
	for (const auto& c : cases)
	{
		Launch launch;
		Poison();
		g_bFullScreen = !c.fullScreen;
		g_MyFull = !c.highResolution;
		CHECK_EQ(0, Run(c.argument));
		CHECK_EQ(4, g_Dimension);
		CHECK_EQ(c.fullScreen, g_bFullScreen);
		CHECK_EQ(c.highResolution, g_MyFull);
		CHECK_EQ(c.fullScreen, GetDisplaySettings().fullscreen);
		CHECK_EQ(c.highResolution != FALSE, GetDisplaySettings().highResolution);
	}
	{
		// Not a mode: Display.ini's choice (here windowed) stands, the
		// resolution and the view stay, and the character is gone all the
		// same - the world is the 4 before it.
		Launch launch;
		launch.Write("UserSet/Display.ini", "Fullscreen=0\n");
		Poison();
		g_bFullScreen = true;
		CHECK_EQ(0, Run("000000004x"));
		CHECK_EQ(4, g_Dimension);
		CHECK(!g_bFullScreen);
		CHECK_EQ(TRUE, g_MyFull);
		CHECK(!GetDisplaySettings().fullscreen);
	}
}

// The launcher's login server, "Futec(ip:port)", anywhere in a game
// launch. Recorded defect: the cut takes the last character even when it
// is not a mode digit (ClientMain.cpp:1291), so a command ending in the
// address loses its ')' and the address is ignored.
TEST(ClientMain3Command, TheFutecAddressNeedsTheModeDigitAfterIt)
{
	{
		Launch launch;
		Poison();
		CHECK_EQ(0, Run("000000003Futec(1.2.3.4:9999)1"));
		CHECK(std::string(g_FutecIP) == "1.2.3.4");
		CHECK_EQ(9999, g_FutecPort);
		CHECK_EQ(3, g_Dimension);
	}
	{
		Launch launch;
		Poison();
		CHECK_EQ(0, Run("000000003Futec(1.2.3.4:9999)"));
		CHECK(std::string(g_FutecIP) == "9.9.9.9");
		CHECK_EQ(1, g_FutecPort);
		CHECK_EQ(3, g_Dimension);
	}
	{
		// Not for the updater: the address is read after the hand-off.
		Launch launch;
		Poison();
		CHECK_EQ(0, Run("F(1:2)1"));
		CHECK(s_events == std::vector<std::string>{ "updater F(1:2)" });
		CHECK(std::string(g_FutecIP) == "9.9.9.9");
	}
}

//======================================================================
// The data tree
//======================================================================

// No data tree under the working directory, the executable's or the one
// three above it: ClientMain stays where it is and the file definitions'
// load (ClientMain.cpp:1381) throws out of it.
// Recorded defect: neither ClientMain nor main/WinMain catches this, or
// the throws of a missing Netmarble file or key (1387-1388), append-patch
// key (701), language key (290) or InitGame's below: the game ends in
// std::terminate, with the message on stderr as the only clue.
TEST(ClientMain3Data, WithoutTheFileDefinitionsClientMainThrows)
{
	// The test binary's directory must not hold a data tree: ClientMain
	// would move there and patch it.
	char exeDirectory[PATH_MAX];
	if (platform_get_executable_dir(exeDirectory, sizeof(exeDirectory)) == 0
		&& !Basic::FindDataRoot({ exeDirectory, std::string(exeDirectory) + "../../../" },
			"Data\\Info\\FileDef.inf").empty())
	{
		std::fprintf(stderr, "ClientMain3Data: a data tree beside the test binary; not run\n");
		return;
	}
	Launch launch(false);
	Poison();

	CHECK(RunThrowing("abc1") == "FileNotExistException: Data\\Info\\FileDef.inf");

	CHECK(s_events.empty());
	CHECK(std::filesystem::current_path().string() == launch.Canonical());
	CHECK(std::string(g_CWD) == launch.Canonical());
	CHECK_EQ(FALSE, g_bNeedUpdate);
	CHECK(g_pFileDef != NULL && g_pFileDef != launch.fileDef);
	CHECK_EQ(FALSE, g_MyFull);
	// The hosts went in first.
	CHECK(!ParsingNetmarble("a b"));
	CHECK((s_events == std::vector<std::string>{ kNetmarbleRefusal, "exit 0" }));
}

// The Netmarble switch is read before the command is: a tree without it
// throws even for the updater.
TEST(ClientMain3Data, TheNetmarbleSwitchMustBeThere)
{
	{
		Launch launch;
		launch.Write("Data/Info/FileDef.inf", "FILE_INFO_APPENDPATCH : Data\\Info\\AppendPatch.inf\n");
		CHECK(RunThrowing("abc1") == "NoSuchElementException: FILE_INFO_NETMARBLE");
		CHECK(s_events.empty());
	}
	{
		Launch launch;
		std::filesystem::remove(launch.root / "Data/Info/Netmarble.inf");
		CHECK(RunThrowing("abc1") == "FileNotExistException: Data/Info/Netmarble.inf");
		CHECK(s_events.empty());
	}
	{
		Launch launch;
		launch.Write("Data/Info/Netmarble.inf", "Other : 1\n");
		CHECK(RunThrowing("abc1") == "NoSuchElementException: Netmarble");
		CHECK(s_events.empty());
	}
}

// Without the append-patch file the launch stops with 0 before the
// updater swap; without its key it throws.
TEST(ClientMain3Data, WithoutTheAppendPatchesTheLaunchStops)
{
	{
		Launch launch;
		launch.Write("Updater.exe", "old");
		launch.Write("Updater2.exe", "new");
		Poison();
		CHECK_EQ(0, Run("0000000031"));
		CHECK(s_events.empty());
		CHECK_EQ(FALSE, g_bActiveApp);
		CHECK(launch.Read("Updater.exe") == "old");
		CHECK(launch.Read("Updater2.exe") == "new");
	}
	{
		Launch launch;
		launch.Write("Data/Info/FileDef.inf", "FILE_INFO_NETMARBLE : Data\\Info\\Netmarble.inf\n");
		CHECK(RunThrowing("0000000031") == "NoSuchElementException: FILE_INFO_APPENDPATCH");
		// The game launch's settings came first.
		CHECK_EQ(3, g_Dimension);
	}
}

// Netmarble=1: a command with a space is refused through the host (the
// box, then Exit(0)); without a space silently; either way ClientMain
// returns -1, after the patches and the updater swap, and ignores the
// world the command names.
// Recorded defect: the -1 return (ClientMain.cpp:1568) leaves the
// file definitions allocated and nothing released.
TEST(ClientMain3Data, NetmarbleOneRefusesTheLaunch)
{
	{
		Launch launch(true, true);
		launch.Write("Data/Info/Netmarble.inf", "Netmarble : 1\n");
		launch.Write("Updater.exe", "old");
		launch.Write("Updater2.exe", "new");
		Poison();

		CHECK_EQ(-1, Run("000000003 x1"));

		CHECK((s_events == std::vector<std::string>{ kNetmarbleRefusal, "exit 0" }));
		CHECK_EQ(5, g_Dimension);
		CHECK_EQ(0, g_MaxNPC);
		CHECK(launch.Read("Updater.exe") == "new");
		CHECK(!launch.Has("Updater2.exe"));
		CHECK(g_pFileDef != NULL && g_pFileDef != launch.fileDef);
		CHECK_EQ(TRUE, g_bActiveGame);
		CHECK_EQ(TRUE, g_bActiveApp);
	}
	{
		Launch launch(true, true);
		launch.Write("Data/Info/Netmarble.inf", "Netmarble : 1\n");
		Poison();

		CHECK_EQ(-1, Run("0000000031"));

		CHECK(s_events.empty());
		CHECK_EQ(5, g_Dimension);
	}
}

// Another Netmarble value shows the refusal (the real host's Exit ends
// the process there) and, with this host, goes on as an ordinary launch
// up to InitGame.
TEST(ClientMain3Data, AnotherNetmarbleValueRefusesAndGoesOn)
{
	Launch launch(true, true);
	launch.Write("Data/Info/Netmarble.inf", "Netmarble : 2\n");
	Poison();
	TextEncoding::SetResourceEncoding(TextEncoding::Encoding::Utf8);

	CHECK_EQ(0, Run("000000003 x1"));

	CHECK((s_events == std::vector<std::string>{ kNetmarbleRefusal, "exit 0" }));
	CHECK_EQ(5, g_Dimension);
	CHECK(TextEncoding::GetResourceEncoding() == TextEncoding::Encoding::Cp949);
	CHECK(g_pFileDef == NULL);
	CHECK_EQ(FALSE, g_bActiveGame);
}

//======================================================================
// A game launch
//======================================================================

// Past the patches - which are applied - the new updater replaces the old
// one, then InitApp runs InitGame, which reads FileDef.inf again and
// stops at the missing nickname table; FinishClient releases everything
// (the tables InitGame made with it) and ClientMain returns 0.
// Recorded defect: InitGame news a second g_pFileDef over ClientMain's
// (GameInit.cpp:902), which leaks.
TEST(ClientMain3Game, AGameLaunchRunsUpToTheNicknameTable)
{
	Launch launch;
	launch.Write("org.spk", std::string("\x01\x00" "A", 3));
	launch.Write("add.spk", std::string("\x01\x00" "B", 3));
	{
		AppendPatchTable table;
		table.Init(1);
		AppendPatch* patch = table.GetMutable(0);
		CHECK(patch != NULL);
		if (patch == NULL)
			return;
		patch->SetOriginalInfo("org.spk", 3, 1);
		patch->AddAppendInfo("add.spk", 3, 1);
		patch->SetFinalInfo(4, 2);
		std::ofstream out(launch.root / "Data/Info/AppendPatch.inf", std::ios::binary);
		table.SaveToFile(out);
	}
	launch.Write("Updater.exe", "old");
	launch.Write("Updater2.exe", "new");
	launch.Write("Data/Info/Language.inf", "LANGUAGE 1\n");
	Poison();
	TextEncoding::SetResourceEncoding(TextEncoding::Encoding::Utf8);
	CHECK(g_pGameStringTable == NULL && g_pNickNameStringTable == NULL);
	CHECK(g_pSystemMessage == NULL && g_pHelpMessage == NULL);

	CHECK_EQ(0, Run("0000000041"));

	CHECK(s_events.empty());
	CHECK(launch.Read("org.spk") == std::string("\x02\x00" "AB", 4));
	CHECK(launch.Read("Updater.exe") == "new");
	CHECK(!launch.Has("Updater2.exe"));
	// InitGame ran: the resource encoding is FileDef.inf's default, and
	// the arctangent table is scaled once more (a second InitGame in one
	// process would leave it wrong; the fixture puts it back).
	CHECK(TextEncoding::GetResourceEncoding() == TextEncoding::Encoding::Cp949);
	CHECK(launch.arcTangents[MathTable::MAX_ANGLE] != 0);
	CHECK_EQ(2 * launch.arcTangents[MathTable::MAX_ANGLE], MathTable::FArcTanTab[MathTable::MAX_ANGLE]);
	CHECK(log_is_enabled(LOG_LEVEL_ERROR));
	// The nickname table's failed open stopped the application.
	CHECK_EQ(FALSE, g_bActiveApp);
	CHECK_EQ(FALSE, g_bActiveGame);
	CHECK(g_pFileDef == NULL);
	CHECK(g_pGameStringTable == NULL && g_pNickNameStringTable == NULL);
	CHECK(g_pSystemMessage == NULL && g_pPlayerMessage == NULL && g_pGameMessage == NULL);
	CHECK(g_pHelpMessage == NULL && g_pNoticeMessage == NULL);
	CHECK(g_pClientConfig == NULL);
	CHECK_EQ(4, g_Dimension);
	CHECK(!g_bFullScreen);
	CHECK_EQ(0, g_MaxNPC);
	// Recorded defect: FinishClient deletes gC_ci and leaves the pointer
	// (ClientMain.cpp:1070); the fixture puts the saved one back.
}

// What InitGame reads first, and the state the launch hands it: with no
// nickname key InitGame throws out of ClientMain, after the tables it
// makes, with the character input of the language still alive.
TEST(ClientMain3Game, InitGameStartsFromTheLaunchState)
{
	const struct { const char* language; bool chinese; } cases[] = {
		{ "LANGUAGE 1\n", true },
		{ "LANGUAGE 0\n", false },
		{ "LANGUAGE 3\n", false },
		{ nullptr, false },
	};
	for (const auto& c : cases)
	{
		Launch launch(true, true);
		launch.Write("Data/Info/FileDef.inf",
			"FILE_INFO_NETMARBLE : Data\\Info\\Netmarble.inf\n"
			"FILE_INFO_APPENDPATCH : Data\\Info\\AppendPatch.inf\n"
			"FILE_LANGUAGE_INFO : Data\\Info\\Language.inf\n"
			"RESOURCE_TEXT_ENCODING : GBK\n");
		if (c.language != nullptr)
			launch.Write("Data/Info/Language.inf", c.language);
		Poison();

		const std::string outcome = RunThrowing("0000000022");

		CHECK(outcome == "NoSuchElementException: FILE_INFO_NICKNAME_STRING");
		if (outcome.starts_with("returned"))
			continue;	// FinishClient ran: gC_ci is dangling
		CHECK(s_events.empty());
		CHECK(gC_ci != NULL && gC_ci != launch.ci);
		if (gC_ci != NULL && gC_ci != launch.ci)
		{
			CHECK_EQ(c.chinese, gC_ci->IsChinese());
			CHECK_EQ(!c.chinese, gC_ci->IsKorean());
			delete gC_ci;
		}
		// InitGame's own file definitions, read from the same file.
		CHECK(g_pFileDef != NULL && g_pFileDef->getProperty("RESOURCE_TEXT_ENCODING") == "GBK");
		CHECK(TextEncoding::GetResourceEncoding() == TextEncoding::Encoding::Gbk);
		CHECK(g_pGameStringTable != NULL && g_pNickNameStringTable != NULL);
		CHECK(g_pSystemMessage != NULL && g_pHelpMessage != NULL);
		CHECK_EQ(2, g_Dimension);
		CHECK(g_bFullScreen);
		CHECK_EQ(TRUE, g_bActiveApp);
		ReleaseAllObjects();
	}
	{
		// The language file's key comes first.
		Launch launch(true, true);
		launch.Write("Data/Info/FileDef.inf",
			"FILE_INFO_NETMARBLE : Data\\Info\\Netmarble.inf\n"
			"FILE_INFO_APPENDPATCH : Data\\Info\\AppendPatch.inf\n");
		CHECK(RunThrowing("0000000022") == "NoSuchElementException: FILE_LANGUAGE_INFO");
		CHECK(gC_ci == launch.ci);
	}
	{
		// An encoding InitGame does not know throws before the tables.
		Launch launch(true, true);
		launch.Write("Data/Info/FileDef.inf", std::string(kFileDef) + "RESOURCE_TEXT_ENCODING : KOI8-R\n");
		const std::string outcome = RunThrowing("0000000022");
		CHECK(outcome == "Error: Unsupported RESOURCE_TEXT_ENCODING in Data/Info/FileDef.inf");
		CHECK(g_pGameStringTable == NULL && g_pSystemMessage == NULL);
		if (!outcome.starts_with("returned") && gC_ci != launch.ci)
			delete gC_ci;
	}
}

// Off Windows the "Update" leftover the updater forgets is looked for as
// "<working directory>\Update": a sibling of the data tree whose name
// ends in "\Update", whose files (not its dotfiles) are deleted before it
// is removed. Recorded defect: the comment at ClientMain.cpp:1474-1476
// says this names nothing off Windows; it names that sibling.
TEST(ClientMain3Game, TheUpdateCleanupReachesASiblingOffWindows)
{
	Launch launch(true, true);
	const std::filesystem::path sibling = launch.base / "tree\\Update";
	std::filesystem::create_directories(sibling);
	std::ofstream(sibling / "a.txt") << "a";
	std::ofstream(sibling / ".keep") << "k";
	launch.Write("Update/b.txt", "b");

	CHECK_EQ(0, Run("0000000011"));

	CHECK(!std::filesystem::exists(sibling / "a.txt"));
	CHECK(std::filesystem::exists(sibling / ".keep"));
	CHECK(launch.Has("Update/b.txt"));
	CHECK(std::filesystem::current_path().string() == launch.Canonical());

	// Empty, it goes.
	std::filesystem::remove(sibling / ".keep");
	std::ofstream(sibling / "c.txt") << "c";
	CHECK_EQ(0, Run("0000000011"));
	CHECK(!std::filesystem::exists(sibling));
}

// Off Windows a new updater without an old one stays where it is: the
// swap removes the old one first and stops when that fails.
TEST(ClientMain3Game, ANewUpdaterNeedsAnOldOneOffWindows)
{
	Launch launch(true, true);
	launch.Write("Updater2.exe", "new");

	CHECK_EQ(0, Run("0000000011"));

	CHECK(launch.Read("Updater2.exe") == "new");
	CHECK(!launch.Has("Updater.exe"));
}

//======================================================================
// The hosts ClientMain installs, and the DLL whitelist
//======================================================================

// They stay after ClientMain returns, ReleaseAllObjects or not.
TEST(ClientMain3Hosts, TheInstalledHostsOutliveTheCall)
{
	Launch launch;
	CHECK_EQ(0, Run("abc1"));
	s_events.clear();

	CHECK(!ParsingNetmarble("a b"));
	GameInitInfoHost::ShowError("boom");
	g_bActiveApp = TRUE;
	GameInitInfoHost::InitFailMessage("stop");

	CHECK((s_events == std::vector<std::string>{ kNetmarbleRefusal, "exit 0", "box Error: boom" }));
	CHECK_EQ(FALSE, g_bActiveApp);
}

// Off Windows the DLL whitelist is not consulted: the loader decides.
TEST(ClientMain3Dlls, OffWindowsTheDllsAreNotListed)
{
	Launch launch;
	s_dllNames = { "evil.dll" };

	CHECK_EQ(0, Run("abc1"));

	CHECK(s_events == std::vector<std::string>{ "updater abc" });
}

#else // PLATFORM_WINDOWS

//======================================================================
// Windows: the DLL whitelist (each ClientMain call waits a second for the
// launcher window first)
//======================================================================

std::string ExecutableDirectory()
{
	char path[MAX_PATH] = { 0 };
	GetModuleFileNameA(NULL, path, MAX_PATH);
	char* cut = strrchr(path, '\\');
	if (cut != NULL)
		*cut = '\0';
	return path;
}

// An unlisted *.dll ends ClientMain with -1, silently, after the display
// mode and before g_bNeedUpdate is cleared and the file definitions load.
TEST(ClientMain3Dlls, AnUnlistedDllEndsTheStart)
{
	Saved saved;
	s_dllNames = { "SDL2.dll", "Evil.dll" };
	g_bNeedUpdate = TRUE;
	g_MyFull = TRUE;

	CHECK_EQ(-1, Run("0000000011"));

	CHECK(s_events == std::vector<std::string>{ "dlls" });
	CHECK(g_pFileDef == saved.fileDef);
	CHECK_EQ(TRUE, g_bNeedUpdate);
	CHECK(std::string(g_CWD) == ExecutableDirectory());
	CHECK_EQ(FALSE, g_MyFull);
}

// Listed names (in any case) pass, and so does a directory that cannot be
// listed, whatever it named: ClientMain goes on to the file definitions,
// which the test binary's directory does not hold.
TEST(ClientMain3Dlls, ListedDllsOrNoListingLetTheStartGoOn)
{
	if (std::filesystem::exists(std::filesystem::path(ExecutableDirectory()) / "Data" / "Info" / "FileDef.inf"))
	{
		std::fprintf(stderr, "ClientMain3Dlls: a data tree beside the test binary; not run\n");
		return;
	}
	{
		Saved saved;
		s_dllNames = { "SDL2.dll", "ZLIB1.DLL", "wavpackdll.dll" };
		g_bNeedUpdate = TRUE;
		CHECK(RunThrowing("0000000011") == "FileNotExistException: Data\\Info\\FileDef.inf");
		CHECK(s_events == std::vector<std::string>{ "dlls" });
		CHECK_EQ(FALSE, g_bNeedUpdate);
	}
	{
		Saved saved;
		s_dllNames = { "Evil.dll" };
		s_dllListed = false;
		g_bNeedUpdate = TRUE;
		CHECK(RunThrowing("0000000011") == "FileNotExistException: Data\\Info\\FileDef.inf");
		CHECK(s_events == std::vector<std::string>{ "dlls" });
		CHECK_EQ(FALSE, g_bNeedUpdate);
	}
}

#endif // PLATFORM_WINDOWS

}
