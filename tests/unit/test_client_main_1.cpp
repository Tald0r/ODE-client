#include "test_framework.h"
#include "ClientMain.h"
#include "AppendPatchInfo.h"
#include "ClientMode.h"
#include "Exception.h"
#include "GameInitInfoHost.h"
#include "InputState.h"
#include "Properties.h"
#include "ViewSectors.h"
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

extern BOOL	g_MyFull;		// ClientMode.cpp's
extern RECT	g_GameRect;

// The game's entry (task 4.265 moved Client/Client.cpp into gameclient as
// ClientMain.cpp), group 1: the decisions ClientMain takes on what it is
// given, which ask ClientHost for nothing.
// - FindUnlistedDll(names): the first name, compared without regard to
//   case, that is not one of the 52 DLLs of the whitelist, as a pointer
//   into the caller's vector; NULL for none. The names are judged alone,
//   so a directory named *.dll counts (the system host lists them).
// - CheckLaunchCommand: under eight characters the updater, eight that are
//   not "00000000" refused, else the game; LaunchDimension: the ninth
//   character when it is a digit, else -1 (the prefix is not checked).
// - ApplyDisplayMode(mode, fullscreen): '1' and '2' clear g_MyFull and
//   touch nothing else, '3' and '4' set it with the 1024x768 frame, cursor
//   and view; '1'/'3' answer windowed, '2'/'4' full screen; any other
//   character answers `fullscreen` and changes nothing.
// - ChooseWindowScale: the largest factor, at least 1, for which the
//   scaled game plus the unscaled frame fits on both axes.
// - ParsingRealServer: after the first space, up to four '|' fields
//   (empty ones skipped, each cut at 31 characters): the mode (any non-zero
//   atoi), the world (atoi), the ID and the key; false, the info untouched,
//   for NULL, under 16 characters, no space or no field.
// - GetFutecAddress: the first '(' ':' ')' after the first "Futec", in that
//   order, give g_FutecIP and g_FutecPort (atoi, unsigned); false and
//   nothing set otherwise.
// - CheckDarkEdenLanguage: the number after the first line starting
//   "LANGUAGE" in FILE_LANGUAGE_INFO (lines starting ';' skipped); Korean
//   without the file definitions, the file or a number.
// - GetPersnalShopUpdateTime: the number after the first line starting
//   "UPDATETIME" in FILE_UPDATE_PERSNALSHOPTIME, read beside the
//   FILE_INFO_DATA archive (a loose override) or from it; 60000 without it.
// - CheckTerriblePatch: false when FILE_INFO_APPENDPATCH cannot be opened
//   (FileOpenBinary reports it); else each patch in turn is executed and
//   checked, the first failure stopping the rest, and the answer is true.
// - The progress bar: off by g_bUseProgressBar; a window only on Windows.
//
// Recorded defects (pinned, not fixed):
// - GetFutecAddress (ClientMain.cpp:188-190) copies with CopyBounded, which
//   is strncpy: the count is the span, so no terminator is written. The
//   address keeps the tail of the last one, an empty address keeps the
//   last one whole, and the port's local str[20] is read by atoi past its
//   digits into uninitialised bytes.
// - ApplyDisplayMode (873-939) leaves g_MyFull as it finds it for a
//   command that does not end in 1-4 (DisplaySettings::LaunchCommand keeps
//   such an argument), and its default is TRUE (ClientMode.cpp:16): the
//   1024x768 layout over the 800x600 frame. The 800x600 modes do not set
//   the frame and the view either; they rely on those defaults.
// - CheckDarkEdenLanguage (299-311) has no range check: "LANGUAGE 5" is
//   DARKEDEN_LANGUAGE_MAX.
// - GetPersnalShopUpdateTime (315-340) answers 0, not its 60000 default,
//   for a file without a number on an UPDATETIME line; MZone.cpp:1785 then
//   refreshes the shops' signs at once.
// - CheckTerriblePatch (707-723) answers true after a patch fails to apply
//   or to check: the game runs on, the rest of the patches not applied.
//
// Not run (undefined behaviour or no end):
// - ParsingRealServer with fewer than four fields: 230-233 read arg2[]
//   entries the loop never wrote.
// - GetFutecAddress's clean form "Futec(ip:port)", or an empty port: atoi
//   reads str[] past what was copied (the tests end the port with a
//   non-digit). An address of 21 or more characters writes past
//   g_FutecIP[20]; a port of 20 or more past str[20].
// - ChooseWindowScale with a 0x0 or negative game (the frame fitting):
//   nothing bounds the loop and scale overflows; so does a game small
//   enough beside the work area for (scale + 1) * side to pass INT_MAX.
// - CheckDarkEdenLanguage with a number past 7 or below 0: the cast to
//   DARKEDEN_LANGUAGE leaves its range.
// - CheckLaunchCommand and LaunchDimension with NULL; GetPersnalShopUpdateTime
//   and CheckTerriblePatch without g_pFileDef: unchecked.
//
// Not observable here: the progress bar's window messages (the range, the
// step, the title), which off Windows reach the shim's no-ops; the
// shop time's archive member (a RAR with the password "darkeden").
namespace {

//----------------------------------------------------------------------
// A scratch directory of the test's own, removed after it
//----------------------------------------------------------------------
struct Scratch
{
	std::filesystem::path dir;
	Scratch()
	{
		static unsigned int sequence = 0;
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		dir = std::filesystem::temp_directory_path() / ("darkeden_client_main_1_"
			+ std::to_string(stamp) + "_" + std::to_string(sequence++));
		std::error_code error;
		std::filesystem::remove_all(dir, error);
		std::filesystem::create_directories(dir);
	}
	~Scratch()
	{
		std::error_code error;
		std::filesystem::remove_all(dir, error);
	}
	std::string Path(const std::string& name) const { return (dir / name).string(); }
	void Write(const std::string& name, const std::string& bytes) const
	{
		std::ofstream out(dir / name, std::ios::binary | std::ios::trunc);
		out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	}
	std::string Read(const std::string& name) const
	{
		std::ifstream in(dir / name, std::ios::binary);
		return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	}
	bool Exists(const std::string& name) const { return std::filesystem::exists(dir / name); }
};

// The client's file definitions replaced by the test's own.
struct FileDef
{
	Properties definitions;
	Properties* previous = g_pFileDef;
	FileDef() { g_pFileDef = &definitions; }
	~FileDef() { g_pFileDef = previous; }
};

//----------------------------------------------------------------------
// The DLL whitelist, as ClientMain.cpp's chain names it, in its order
//----------------------------------------------------------------------
const char* const kListedDlls[] = {
	"timer.dll", "msvcrtd.dll", "msvcrt.dll", "msvcp60.dll", "msvcirt.dll", "ifc22.dll", "gl.dll",
	"dsetup32.dll", "dsetup.dll", "deutil.dll", "basics.dll", "npx.dll", "npupdate0.dll", "nppsk.dll",
	"npgmup.dll", "npfgm9x.dll", "npcipher.dll", "mother.dll", "fl.dll", "dbghelp.dll", "darkedenweb.dll",
	"npchk.dll", "xerces-c_2_4_0.dll",
	"sdl2d.dll", "libssl-3-x64.dll", "libcrypto-3-x64.dll", "sdl2.dll", "sdl2_ttfd.dll", "sdl2_ttf.dll",
	"sdl2_imaged.dll", "sdl2_image.dll", "freetyped.dll", "freetype.dll", "libpng16d.dll", "libpng16.dll",
	"zlibd1.dll", "zlib1.dll",
	"zd.dll", "z.dll", "jpeg62.dll", "brotlicommon.dll", "brotlidec.dll", "bz2d.dll", "bz2.dll",
	"iconv-2.dll",
	"sdl2_mixerd.dll", "sdl2_mixer.dll", "ogg.dll", "vorbis.dll", "vorbisenc.dll", "vorbisfile.dll",
	"wavpackdll.dll",
};

std::string Upper(std::string text)
{
	for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	return text;
}

// Where FindUnlistedDll points in names, -1 for NULL, -2 for outside.
int UnlistedAt(const std::vector<std::string>& names)
{
	const std::string* found = FindUnlistedDll(names);
	if (found == nullptr) return -1;
	for (size_t i = 0; i < names.size(); ++i)
		if (found == &names[i]) return static_cast<int>(i);
	return -2;
}

// The working directory, back as it was.
struct CwdGuard
{
	std::filesystem::path saved = std::filesystem::current_path();
	~CwdGuard() { std::filesystem::current_path(saved); }
};

//----------------------------------------------------------------------
// The display globals ApplyDisplayMode may set
//----------------------------------------------------------------------
LONG* const kViews[] = {
	&g_SECTOR_WIDTH, &g_SECTOR_HEIGHT, &g_SECTOR_WIDTH_HALF, &g_SECTOR_HEIGHT_HALF,
	&g_SECTOR_SKIP_PLAYER_LEFT, &g_SECTOR_SKIP_PLAYER_UP,
	&g_TILESURFACE_SECTOR_WIDTH, &g_TILESURFACE_SECTOR_HEIGHT, &g_TILESURFACE_SECTOR_OUTLINE_RIGHT,
	&g_TILESURFACE_SECTOR_OUTLINE_DOWN, &g_TILESURFACE_WIDTH, &g_TILESURFACE_HEIGHT,
	&g_TILESURFACE_OUTLINE_RIGHT, &g_TILESURFACE_OUTLINE_DOWN,
};
const int kViewCount = static_cast<int>(std::size(kViews));

struct DisplayState
{
	BOOL myFull = FALSE;
	RECT rect = {0, 0, 0, 0};
	int x = 0;
	int y = 0;
	LONG views[std::size(kViews)] = {};

	static DisplayState Read()
	{
		DisplayState state;
		state.myFull = g_MyFull;
		state.rect = g_GameRect;
		state.x = g_x;
		state.y = g_y;
		for (int i = 0; i < kViewCount; ++i) state.views[i] = *kViews[i];
		return state;
	}
	void Write() const
	{
		g_MyFull = myFull;
		g_GameRect = rect;
		g_x = x;
		g_y = y;
		for (int i = 0; i < kViewCount; ++i) *kViews[i] = views[i];
	}
	bool operator==(const DisplayState& other) const
	{
		return (myFull != FALSE) == (other.myFull != FALSE)
			&& rect.left == other.rect.left && rect.top == other.rect.top
			&& rect.right == other.rect.right && rect.bottom == other.rect.bottom
			&& x == other.x && y == other.y
			&& std::memcmp(views, other.views, sizeof(views)) == 0;
	}
};

struct DisplayGuard
{
	DisplayState saved = DisplayState::Read();
	~DisplayGuard() { saved.Write(); }
};

// Every global at a value no mode writes.
DisplayState Sentinels(BOOL myFull)
{
	DisplayState state;
	state.myFull = myFull;
	state.rect = {1, 2, 3, 4};
	state.x = 5;
	state.y = 6;
	for (int i = 0; i < kViewCount; ++i) state.views[i] = 100 + i;
	return state;
}

// What '3' and '4' set.
DisplayState Large()
{
	DisplayState state;
	state.myFull = TRUE;
	state.rect = {1023, 767, 1024, 768};
	state.x = 512;
	state.y = 384;
	const LONG views[] = {21, 32, 11, 17, -10, -16, 27, 37, 24, 34, 1296, 888, 1152, 816};
	std::memcpy(state.views, views, sizeof(views));
	return state;
}

//----------------------------------------------------------------------
// The auto-login and the launcher's address
//----------------------------------------------------------------------
std::string Text(const MString& text) { return text.GetString() != nullptr ? text.GetString() : "<null>"; }

// The info as the refused parses must leave it.
void Preset(REALSERVER_INFO& info)
{
	info.bMode = true;
	info.WorldID = 77;
	info.ID = "keep";
	info.Key = "keep";
}
bool Untouched(const REALSERVER_INFO& info)
{
	return info.bMode && info.WorldID == 77 && Text(info.ID) == "keep" && Text(info.Key) == "keep";
}

struct FutecGuard
{
	char ip[sizeof(g_FutecIP)];
	unsigned int port = g_FutecPort;
	FutecGuard() { std::memcpy(ip, g_FutecIP, sizeof(ip)); }
	~FutecGuard()
	{
		std::memcpy(g_FutecIP, ip, sizeof(ip));
		g_FutecPort = port;
	}
};

void SetFutec(const char* ip, unsigned int port)
{
	std::memset(g_FutecIP, 0, sizeof(g_FutecIP));
	std::memcpy(g_FutecIP, ip, std::strlen(ip));
	g_FutecPort = port;
}

//----------------------------------------------------------------------
// The append patches
//----------------------------------------------------------------------
using Bytes = std::string;

// One patch: an original pack, one pack appended, the final size and count.
struct PatchSpec
{
	std::string original;
	long originalSize;
	WORD originalCount;
	std::string append;
	long appendSize;
	WORD appendCount;
	long finalSize;
	WORD finalCount;
};

void SavePatches(const std::string& path, const std::vector<PatchSpec>& specs)
{
	AppendPatchTable table;
	table.Init(static_cast<int>(specs.size()));
	for (size_t i = 0; i < specs.size(); ++i)
	{
		const PatchSpec& spec = specs[i];
		AppendPatch* patch = table.GetMutable(static_cast<int>(i));
		patch->SetOriginalInfo(spec.original.c_str(), spec.originalSize, spec.originalCount);
		patch->AddAppendInfo(spec.append.c_str(), spec.appendSize, spec.appendCount);
		patch->SetFinalInfo(spec.finalSize, spec.finalCount);
	}
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	table.SaveToFile(out);
}

std::vector<std::string> failures;

// The patch fixture: two packs and a pack to append to each, the patch
// information's path in the file definitions, failures recorded.
struct PatchFixture
{
	Scratch scratch;
	FileDef fileDef;
	GameInitInfoHost::Host host;
	const GameInitInfoHost::Host* previousHost = nullptr;

	PatchFixture()
	{
		failures.clear();
		SetAppendPatchHost(nullptr);
		host.InitFailMessage = [](const char* message) { failures.push_back(message); };
		previousHost = GameInitInfoHost::SetHost(&host);
		fileDef.definitions.setProperty("FILE_INFO_APPENDPATCH", scratch.Path("AppendPatch.inf"));
		scratch.Write("first.spk", Bytes("\x02\x00" "AB", 4));
		scratch.Write("first_add.spk", Bytes("\x01\x00" "C", 3));
		scratch.Write("second.spk", Bytes("\x01\x00" "X", 3));
		scratch.Write("second_add.spk", Bytes("\x02\x00" "YZ", 4));
	}
	~PatchFixture() { GameInitInfoHost::SetHost(previousHost); }

	PatchSpec First() const { return {scratch.Path("first.spk"), 4, 2, scratch.Path("first_add.spk"), 3, 1, 5, 3}; }
	PatchSpec Second() const { return {scratch.Path("second.spk"), 3, 1, scratch.Path("second_add.spk"), 4, 2, 5, 3}; }
	bool SecondUntouched() const
	{
		return scratch.Read("second.spk") == Bytes("\x01\x00" "X", 3) && scratch.Exists("second_add.spk");
	}
};

//----------------------------------------------------------------------
// The progress bar
//----------------------------------------------------------------------
struct ProgressGuard
{
	bool use = g_bUseProgressBar;
	HWND window = g_hWndProgress;
	~ProgressGuard()
	{
		g_bUseProgressBar = use;
		g_hWndProgress = window;
	}
};

// A handle no window has, off Windows only ever compared.
HWND FakeWindow()
{
	static int token = 0;
	return reinterpret_cast<HWND>(&token);
}

} // namespace

//======================================================================
// FindUnlistedDll
//======================================================================
TEST(ClientMain1Dll, NoNamesFindNothing)
{
	const std::vector<std::string> names;
	CHECK(FindUnlistedDll(names) == nullptr);
}

TEST(ClientMain1Dll, EveryListedNameIsAcceptedAloneAndTogether)
{
	CHECK_EQ(52, std::size(kListedDlls));
	std::vector<std::string> all;
	for (const char* name : kListedDlls)
	{
		const std::vector<std::string> alone = {name};
		CHECK_EQ(-1, UnlistedAt(alone));
		all.push_back(name);
	}
	CHECK_EQ(-1, UnlistedAt(all));
}

TEST(ClientMain1Dll, TheNamesAreComparedWithoutRegardToCase)
{
	for (const char* name : kListedDlls)
	{
		std::string capital = name;
		capital[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(capital[0])));
		const std::vector<std::string> names = {Upper(name), capital};
		CHECK_EQ(-1, UnlistedAt(names));
	}
	const std::vector<std::string> mixed = {"SDL2_Mixer.DLL", "LibCrypto-3-X64.Dll", "Xerces-C_2_4_0.dll"};
	CHECK_EQ(-1, UnlistedAt(mixed));
}

TEST(ClientMain1Dll, OnlyTheExactNamesAreListed)
{
	for (const char* listed : kListedDlls)
	{
		const std::string name = listed;
		const std::string variants[] = {
			name + "x", "x" + name, name.substr(0, name.size() - 1), name + " ", " " + name,
			"./" + name, name.substr(0, name.size() - 4), name + ".dll",
		};
		for (const std::string& variant : variants)
		{
			const std::vector<std::string> names = {variant};
			CHECK_EQ(0, UnlistedAt(names));
		}
	}
}

TEST(ClientMain1Dll, TheFirstUnlistedNameIsAnsweredWhereItStands)
{
	const std::vector<std::string> first = {"Evil.DLL", "sdl2.dll", "zlib1.dll"};
	CHECK_EQ(0, UnlistedAt(first));
	const std::vector<std::string> middle = {"sdl2.dll", "Evil.DLL", "zlib1.dll"};
	CHECK_EQ(1, UnlistedAt(middle));
	// Answered as the caller spelled it, not folded.
	CHECK(*FindUnlistedDll(middle) == "Evil.DLL");
	const std::vector<std::string> last = {"sdl2.dll", "zlib1.dll", "Evil.DLL"};
	CHECK_EQ(2, UnlistedAt(last));
	const std::vector<std::string> two = {"sdl2.dll", "one.dll", "two.dll"};
	CHECK_EQ(1, UnlistedAt(two));
}

TEST(ClientMain1Dll, TheSystemHostListsDirectoriesToo)
{
	Scratch scratch;
	scratch.Write("Timer.DLL", "x");
	scratch.Write("zlib1.dll", "x");
	scratch.Write("notes.txt", "x");
	std::filesystem::create_directory(scratch.dir / "plugins.dll");
	std::vector<std::string> names = {"sdl2.dll"};
	bool listed = false;
	{
		CwdGuard cwd;
		std::filesystem::current_path(scratch.dir);
		listed = ClientHost::SystemHost()->ListDllNames(names);
	}
	CHECK(listed);
	// Appended, in the listing's case-insensitive order.
	CHECK(names == std::vector<std::string>({"sdl2.dll", "plugins.dll", "Timer.DLL", "zlib1.dll"}));
	CHECK_EQ(1, UnlistedAt(names));
}

//======================================================================
// CheckLaunchCommand, LaunchDimension
//======================================================================
TEST(ClientMain1Launch, UnderEightCharactersGoToTheUpdater)
{
	for (const char* command : {"", "0", "0000000", "1234567", "       "})
		CHECK_EQ(LAUNCH_UPDATER, CheckLaunchCommand(command));
}

TEST(ClientMain1Launch, EightCharactersOtherThanZerosAreRefused)
{
	for (const char* command : {"00000001", "10000000", "0000000O", "abcdefgh", "0000000 ", " 00000000", "x00000000 1|2|a|b"})
		CHECK_EQ(LAUNCH_REFUSED, CheckLaunchCommand(command));
}

TEST(ClientMain1Launch, EightZerosRunTheGame)
{
	for (const char* command : {"00000000", "000000000", "000000003", "00000000xyz", "000000000 1|2|ab|cd"})
		CHECK_EQ(LAUNCH_GAME, CheckLaunchCommand(command));
}

TEST(ClientMain1Launch, TheNinthCharacterNamesTheWorld)
{
	for (int digit = 0; digit <= 9; ++digit)
	{
		const std::string command = "00000000" + std::to_string(digit);
		CHECK_EQ(digit, LaunchDimension(command.c_str()));
	}
	CHECK_EQ(1, LaunchDimension("0000000012"));
	CHECK_EQ(4, LaunchDimension("000000004 1|2|ab|cd"));
	// The first eight characters are not looked at.
	CHECK_EQ(7, LaunchDimension("abcdefgh7"));
}

TEST(ClientMain1Launch, NoDigitNoWorld)
{
	// '/' and ':' sit either side of the digits.
	for (const char* command : {"", "0000000", "00000000", "00000000/", "00000000:", "00000000a", "00000000 ", "00000000x5"})
		CHECK_EQ(-1, LaunchDimension(command));
}

//======================================================================
// ApplyDisplayMode
//======================================================================
TEST(ClientMain1Display, Mode1Is800x600InAWindowAndSetsNothingElse)
{
	DisplayGuard guard;
	for (bool fullscreen : {false, true})
		for (BOOL myFull : {FALSE, TRUE})
		{
			Sentinels(myFull).Write();
			CHECK(!ApplyDisplayMode('1', fullscreen));
			CHECK(DisplayState::Read() == Sentinels(FALSE));
		}
}

TEST(ClientMain1Display, Mode2Is800x600FullScreenAndSetsNothingElse)
{
	DisplayGuard guard;
	for (bool fullscreen : {false, true})
		for (BOOL myFull : {FALSE, TRUE})
		{
			Sentinels(myFull).Write();
			CHECK(ApplyDisplayMode('2', fullscreen));
			CHECK(DisplayState::Read() == Sentinels(FALSE));
		}
}

TEST(ClientMain1Display, Mode3Is1024x768InAWindow)
{
	DisplayGuard guard;
	for (bool fullscreen : {false, true})
		for (BOOL myFull : {FALSE, TRUE})
		{
			Sentinels(myFull).Write();
			CHECK(!ApplyDisplayMode('3', fullscreen));
			CHECK(DisplayState::Read() == Large());
		}
}

TEST(ClientMain1Display, Mode4Is1024x768FullScreen)
{
	DisplayGuard guard;
	for (bool fullscreen : {false, true})
		for (BOOL myFull : {FALSE, TRUE})
		{
			Sentinels(myFull).Write();
			CHECK(ApplyDisplayMode('4', fullscreen));
			CHECK(DisplayState::Read() == Large());
		}
}

TEST(ClientMain1Display, AnyOtherCharacterKeepsTheSettingsChoiceAndChangesNothing)
{
	DisplayGuard guard;
	for (char mode : {'0', '5', '9', 'a', ' ', '\0', '\xff'})
		for (bool fullscreen : {false, true})
			for (BOOL myFull : {FALSE, TRUE})
			{
				Sentinels(myFull).Write();
				CHECK(ApplyDisplayMode(mode, fullscreen) == fullscreen);
				// Recorded defect: g_MyFull is left as found, TRUE by default.
				CHECK(DisplayState::Read() == Sentinels(myFull));
			}
}

//======================================================================
// ChooseWindowScale
//======================================================================
TEST(ClientMain1Scale, AtLeastOneEvenWhenTheGameDoesNotFit)
{
	CHECK_EQ(1, ChooseWindowScale(800, 600, 16, 58, 640, 480));
	CHECK_EQ(1, ChooseWindowScale(800, 600, 16, 58, 816, 658));
	CHECK_EQ(1, ChooseWindowScale(800, 600, 16, 58, 0, 0));
}

TEST(ClientMain1Scale, TheNextFactorMustFitBothAxesWhole)
{
	CHECK_EQ(2, ChooseWindowScale(800, 600, 16, 58, 1616, 1258));
	CHECK_EQ(1, ChooseWindowScale(800, 600, 16, 58, 1615, 1258));
	CHECK_EQ(1, ChooseWindowScale(800, 600, 16, 58, 1616, 1257));
}

TEST(ClientMain1Scale, TheShorterAxisDecides)
{
	CHECK_EQ(3, ChooseWindowScale(800, 600, 0, 0, 2400, 100000));
	CHECK_EQ(2, ChooseWindowScale(800, 600, 0, 0, 2399, 100000));
	CHECK_EQ(3, ChooseWindowScale(800, 600, 0, 0, 100000, 1800));
	CHECK_EQ(2, ChooseWindowScale(800, 600, 0, 0, 100000, 1799));
}

TEST(ClientMain1Scale, TheFrameIsAddedOnceNotScaled)
{
	CHECK_EQ(3, ChooseWindowScale(100, 100, 50, 50, 350, 350));
	CHECK_EQ(2, ChooseWindowScale(100, 100, 50, 50, 349, 350));
	CHECK_EQ(2, ChooseWindowScale(100, 100, 50, 50, 350, 349));
}

TEST(ClientMain1Scale, CommonDesktops)
{
	CHECK_EQ(1, ChooseWindowScale(800, 600, 16, 58, 1920, 1040));
	CHECK_EQ(2, ChooseWindowScale(800, 600, 16, 58, 2560, 1400));
	CHECK_EQ(3, ChooseWindowScale(800, 600, 16, 58, 3840, 2100));
	CHECK_EQ(1, ChooseWindowScale(1024, 768, 16, 58, 1920, 1040));
	CHECK_EQ(2, ChooseWindowScale(1024, 768, 16, 58, 3840, 2100));
}

TEST(ClientMain1Scale, AZeroSideIsBoundByTheOther)
{
	// Only a 0x0 game is unbounded (not run).
	CHECK_EQ(2, ChooseWindowScale(0, 600, 16, 58, 1000, 1258));
	CHECK_EQ(1, ChooseWindowScale(0, 600, 16, 58, 1000, 1257));
	CHECK_EQ(2, ChooseWindowScale(800, 0, 16, 58, 1616, 1000));
	CHECK_EQ(1, ChooseWindowScale(800, 0, 16, 58, 1615, 1000));
}

//======================================================================
// ParsingRealServer, REALSERVER_INFO
//======================================================================
TEST(ClientMain1RealServer, TheDefaultIsAManualLoginToWorldZero)
{
	const REALSERVER_INFO info;
	CHECK(!info.bMode);
	CHECK_EQ(0, info.WorldID);
	CHECK(info.ID.GetString() == nullptr);
	CHECK(info.Key.GetString() == nullptr);
}

TEST(ClientMain1RealServer, TheFourFieldsAfterTheFirstSpace)
{
	REALSERVER_INFO info;
	CHECK(ParsingRealServer("000000000 1|1|player|secret", 0, info));
	CHECK(info.bMode);
	CHECK_EQ(1, info.WorldID);
	CHECK(Text(info.ID) == "player");
	CHECK(Text(info.Key) == "secret");
}

TEST(ClientMain1RealServer, TheModeIsAnyNonZeroNumber)
{
	const struct { const char* mode; bool automatic; } cases[] = {
		{"0", false}, {"00", false}, {"x", false}, {"1", true}, {"2", true}, {"-1", true}, {"1x", true},
	};
	for (const auto& c : cases)
	{
		REALSERVER_INFO info;
		info.bMode = !c.automatic;
		const std::string command = std::string("000000000 ") + c.mode + "|0|ab|cd";
		CHECK(ParsingRealServer(command.c_str(), 0, info));
		CHECK(info.bMode == c.automatic);
	}
}

TEST(ClientMain1RealServer, TheWorldIsItsNumber)
{
	const struct { const char* world; int id; } cases[] = {{"0", 0}, {"1", 1}, {"12x", 12}, {"-3", -3}, {"x", 0}};
	for (const auto& c : cases)
	{
		REALSERVER_INFO info;
		info.WorldID = 99;
		const std::string command = std::string("000000000 1|") + c.world + "|ab|cd";
		CHECK(ParsingRealServer(command.c_str(), 0, info));
		CHECK_EQ(c.id, info.WorldID);
	}
}

TEST(ClientMain1RealServer, EmptyFieldsAreSkippedAndAFifthIgnored)
{
	REALSERVER_INFO info;
	CHECK(ParsingRealServer("000000000 |1||2|||ab|cd|", 0, info));
	CHECK(info.bMode);
	CHECK_EQ(2, info.WorldID);
	CHECK(Text(info.ID) == "ab");
	CHECK(Text(info.Key) == "cd");

	REALSERVER_INFO more;
	CHECK(ParsingRealServer("000000000 0|1|ab|cd|ef|gh", 0, more));
	CHECK(!more.bMode);
	CHECK(Text(more.Key) == "cd");
}

TEST(ClientMain1RealServer, OnlyTheFirstSpaceSplits)
{
	REALSERVER_INFO info;
	CHECK(ParsingRealServer("000000000 x y|2|a b|c d", 0, info));
	CHECK(!info.bMode);
	CHECK_EQ(2, info.WorldID);
	CHECK(Text(info.ID) == "a b");
	CHECK(Text(info.Key) == "c d");

	REALSERVER_INFO spaced;
	CHECK(ParsingRealServer("000000000  1|2|ab|cd", 0, spaced));
	CHECK(spaced.bMode);
}

TEST(ClientMain1RealServer, AFieldIsCutAt31Characters)
{
	const std::string id(40, 'i');
	const std::string key = std::string(31, 'k') + "K";
	const std::string command = "000000000 1|1|" + id + "|" + key;
	REALSERVER_INFO info;
	CHECK(ParsingRealServer(command.c_str(), 0, info));
	CHECK(Text(info.ID) == std::string(31, 'i'));
	CHECK(Text(info.Key) == std::string(31, 'k'));
}

TEST(ClientMain1RealServer, TheWorldArgumentIsNotUsed)
{
	for (int dimension : {0, 1, -5})
	{
		REALSERVER_INFO info;
		CHECK(ParsingRealServer("000000000 1|3|ab|cd", dimension, info));
		CHECK_EQ(3, info.WorldID);
	}
}

TEST(ClientMain1RealServer, RefusedCommandsLeaveTheInfoAlone)
{
	REALSERVER_INFO info;
	Preset(info);
	CHECK(!ParsingRealServer(nullptr, 0, info));
	CHECK(Untouched(info));
	// 15 characters, then 16.
	CHECK(!ParsingRealServer("00000 1|2|ab|cd", 0, info));
	CHECK(Untouched(info));
	CHECK(ParsingRealServer("000000 1|2|ab|cd", 0, info));
	CHECK_EQ(2, info.WorldID);

	for (const char* command : {"0000000000000001|2|ab|cd", "0000000000000000 ", "000000000 ||||||||"})
	{
		Preset(info);
		CHECK(!ParsingRealServer(command, 0, info));
		CHECK(Untouched(info));
	}
}

//======================================================================
// GetFutecAddress
// Every accepted port ends in a non-digit: atoi would otherwise read
// str[] past the copy (recorded defect, not run).
//======================================================================
TEST(ClientMain1Futec, TheAddressInParentheses)
{
	FutecGuard guard;
	SetFutec("", 0);
	CHECK(GetFutecAddress("00000000 Futec(10.0.0.7:9999 )"));
	CHECK(std::strcmp(g_FutecIP, "10.0.0.7") == 0);
	CHECK_EQ(9999, g_FutecPort);

	SetFutec("", 0);
	CHECK(GetFutecAddress("Futec(1.2.3.4: 2000x)"));
	CHECK(std::strcmp(g_FutecIP, "1.2.3.4") == 0);
	CHECK_EQ(2000, g_FutecPort);
}

TEST(ClientMain1Futec, ThePortIsAtoiUnsigned)
{
	FutecGuard guard;
	SetFutec("", 7);
	CHECK(GetFutecAddress("Futec(1.2.3.4:abc)"));
	CHECK_EQ(0, g_FutecPort);
	CHECK(GetFutecAddress("Futec(1.2.3.4:-5x)"));
	CHECK_EQ(4294967291LL, g_FutecPort);
}

TEST(ClientMain1Futec, TheFirstMarkersCountInOrder)
{
	FutecGuard guard;
	for (const char* command : {"", "Futec", "Futec 1.2.3.4:80)", "Futec(1.2.3.4 80)", "Futec(1.2.3.4:80",
		"futec(1.2.3.4:80x)", "(x) Futec(1.2.3.4:80x)", "a:b Futec(1.2.3.4:80x)", "Futec(1.2.3.4)80:",
		")Futec(1.2.3.4:80x)", "(Futec 1.2.3.4:80x)"})
	{
		SetFutec("keep", 4321);
		CHECK(!GetFutecAddress(command));
		CHECK(std::strcmp(g_FutecIP, "keep") == 0);
		CHECK_EQ(4321, g_FutecPort);
	}

	SetFutec("", 0);
	CHECK(GetFutecAddress("Futec xyz (1.2.3.4:80x)"));
	CHECK(std::strcmp(g_FutecIP, "1.2.3.4") == 0);
	CHECK_EQ(80, g_FutecPort);

	SetFutec("", 0);
	CHECK(GetFutecAddress("Futec(1.1.1.1:11x) Futec(2.2.2.2:22x)"));
	CHECK(std::strcmp(g_FutecIP, "1.1.1.1") == 0);
	CHECK_EQ(11, g_FutecPort);
}

TEST(ClientMain1Futec, TheAddressIsNotTerminated)
{
	FutecGuard guard;
	// Recorded defect: the last address's tail stays.
	SetFutec("999.999.999.999", 0);
	CHECK(GetFutecAddress("Futec(1.2.3.4:80x)"));
	CHECK(std::strcmp(g_FutecIP, "1.2.3.4.999.999") == 0);

	// Recorded defect: an empty address keeps the last one.
	SetFutec("5.6.7.8", 0);
	CHECK(GetFutecAddress("Futec(:81x)"));
	CHECK(std::strcmp(g_FutecIP, "5.6.7.8") == 0);
	CHECK_EQ(81, g_FutecPort);

	// Twenty characters fill the array: nothing terminates it.
	SetFutec("", 0);
	CHECK(GetFutecAddress("Futec(123.123.123.123.1234:1x)"));
	CHECK(std::memcmp(g_FutecIP, "123.123.123.123.1234", sizeof(g_FutecIP)) == 0);
}

//======================================================================
// CheckDarkEdenLanguage
//======================================================================
TEST(ClientMain1Language, KoreanWithoutTheFileDefinitions)
{
	FileDef fileDef;
	g_pFileDef = nullptr;
	CHECK_EQ(DARKEDEN_KOREAN, CheckDarkEdenLanguage());
}

TEST(ClientMain1Language, KoreanWithoutTheFile)
{
	Scratch scratch;
	FileDef fileDef;
	fileDef.definitions.setProperty("FILE_LANGUAGE_INFO", scratch.Path("Language.inf"));
	CHECK_EQ(DARKEDEN_KOREAN, CheckDarkEdenLanguage());
}

TEST(ClientMain1Language, TheNumberNamesTheLanguage)
{
	Scratch scratch;
	FileDef fileDef;
	fileDef.definitions.setProperty("FILE_LANGUAGE_INFO", scratch.Path("Language.inf"));
	const DARKEDEN_LANGUAGE languages[] = {DARKEDEN_KOREAN, DARKEDEN_CHINESE, DARKEDEN_JAPANESE, DARKEDEN_ENGLISH, DARKEDEN_TAIWAN};
	for (int n = 0; n < 5; ++n)
	{
		scratch.Write("Language.inf", "LANGUAGE " + std::to_string(n) + "\r\n");
		CHECK_EQ(languages[n], CheckDarkEdenLanguage());
	}
	// Recorded defect: no range check.
	scratch.Write("Language.inf", "LANGUAGE 5\r\n");
	CHECK_EQ(DARKEDEN_LANGUAGE_MAX, CheckDarkEdenLanguage());
}

TEST(ClientMain1Language, TheFirstLanguageLineDecides)
{
	Scratch scratch;
	FileDef fileDef;
	fileDef.definitions.setProperty("FILE_LANGUAGE_INFO", scratch.Path("Language.inf"));
	const struct { const char* text; DARKEDEN_LANGUAGE language; } cases[] = {
		{";LANGUAGE 1\r\nVERSION 3\r\n\r\nLANGUAGE 2\r\nLANGUAGE 3\r\n", DARKEDEN_JAPANESE},
		{"LANGUAGE\r\nLANGUAGE 3\r\n", DARKEDEN_KOREAN},
		{"LANGUAGEX 3\r\nLANGUAGE 3\r\n", DARKEDEN_KOREAN},
		{"LANGUAGE3\r\n", DARKEDEN_ENGLISH},
		{"LANGUAGE 4 and more\r\n", DARKEDEN_TAIWAN},
		{"LANGUAGE 1", DARKEDEN_CHINESE},
		{" LANGUAGE 3\r\n", DARKEDEN_KOREAN},
		{"language 3\r\n", DARKEDEN_KOREAN},
		{"", DARKEDEN_KOREAN},
	};
	for (const auto& c : cases)
	{
		scratch.Write("Language.inf", c.text);
		CHECK_EQ(c.language, CheckDarkEdenLanguage());
	}
}

TEST(ClientMain1Language, AFileDefinitionsWithoutTheEntryThrows)
{
	FileDef fileDef;
	bool thrown = false;
	try { CheckDarkEdenLanguage(); } catch (NoSuchElementException&) { thrown = true; }
	CHECK(thrown);
}

//======================================================================
// GetPersnalShopUpdateTime
//======================================================================
TEST(ClientMain1ShopTime, SixtySecondsWithoutTheFile)
{
	Scratch scratch;
	FileDef fileDef;
	fileDef.definitions.setProperty("FILE_INFO_DATA", scratch.Path("absent.rpk"));
	fileDef.definitions.setProperty("FILE_UPDATE_PERSNALSHOPTIME", "PersnalShop.inf");
	CHECK_EQ(60000, GetPersnalShopUpdateTime());
}

TEST(ClientMain1ShopTime, TheFirstUpdateTimeLineBesideTheArchive)
{
	Scratch scratch;
	FileDef fileDef;
	fileDef.definitions.setProperty("FILE_INFO_DATA", scratch.Path("absent.rpk"));
	fileDef.definitions.setProperty("FILE_UPDATE_PERSNALSHOPTIME", "PersnalShop.inf");
	const struct { const char* text; int time; } cases[] = {
		{"UPDATETIME 30000\r\n", 30000},
		{"; refresh\r\nVERSION 2\r\nUPDATETIME 5\r\nUPDATETIME 9\r\n", 5},
		{"UPDATETIME7", 7},
		// Recorded defect: 0, not the 60000 default, without a number.
		{"", 0},
		{"VERSION 2\r\n", 0},
		{"UPDATETIME\r\nUPDATETIME 9\r\n", 0},
		{" UPDATETIME 9\r\n", 0},
	};
	for (const auto& c : cases)
	{
		scratch.Write("PersnalShop.inf", c.text);
		CHECK_EQ(c.time, GetPersnalShopUpdateTime());
	}

	// Beside the archive FILE_INFO_DATA names, not elsewhere.
	std::filesystem::create_directory(scratch.dir / "sub");
	scratch.Write("sub/PersnalShop.inf", "UPDATETIME 222\r\n");
	fileDef.definitions.setProperty("FILE_INFO_DATA", scratch.Path("sub/absent.rpk"));
	CHECK_EQ(222, GetPersnalShopUpdateTime());
}

TEST(ClientMain1ShopTime, AFileDefinitionsWithoutAnEntryThrows)
{
	for (const char* present : {"FILE_INFO_DATA", "FILE_UPDATE_PERSNALSHOPTIME"})
	{
		FileDef fileDef;
		fileDef.definitions.setProperty(present, "x");
		bool thrown = false;
		try { GetPersnalShopUpdateTime(); } catch (NoSuchElementException&) { thrown = true; }
		CHECK(thrown);
	}
}

//======================================================================
// CheckTerriblePatch
//======================================================================
TEST(ClientMain1Patch, WithoutThePatchInformationItFails)
{
	PatchFixture fixture;
	CHECK(!CheckTerriblePatch());
	CHECK(failures == std::vector<std::string>({"[Error] File Open Error : [" + fixture.scratch.Path("AppendPatch.inf") + "]"}));
}

TEST(ClientMain1Patch, NoPatchesSucceed)
{
	PatchFixture fixture;
	SavePatches(fixture.scratch.Path("AppendPatch.inf"), {});
	CHECK(CheckTerriblePatch());
	fixture.scratch.Write("AppendPatch.inf", "");
	CHECK(CheckTerriblePatch());
	CHECK(failures.empty());
}

TEST(ClientMain1Patch, EveryPatchIsApplied)
{
	PatchFixture fixture;
	SavePatches(fixture.scratch.Path("AppendPatch.inf"), {fixture.First(), fixture.Second()});
	CHECK(CheckTerriblePatch());
	CHECK(fixture.scratch.Read("first.spk") == Bytes("\x03\x00" "ABC", 5));
	CHECK(fixture.scratch.Read("second.spk") == Bytes("\x03\x00" "XYZ", 5));
	CHECK(!fixture.scratch.Exists("first_add.spk"));
	CHECK(!fixture.scratch.Exists("second_add.spk"));
	CHECK(failures.empty());
}

TEST(ClientMain1Patch, APatchThatCannotApplyStopsTheRest)
{
	PatchFixture fixture;
	PatchSpec first = fixture.First();
	first.original = fixture.scratch.Path("missing.spk");
	SavePatches(fixture.scratch.Path("AppendPatch.inf"), {first, fixture.Second()});
	// Recorded defect: true all the same.
	CHECK(CheckTerriblePatch());
	CHECK(fixture.scratch.Exists("first_add.spk"));
	CHECK(fixture.SecondUntouched());
}

TEST(ClientMain1Patch, APatchThatDoesNotCheckStopsTheRest)
{
	PatchFixture fixture;
	PatchSpec first = fixture.First();
	first.finalSize = 6;
	SavePatches(fixture.scratch.Path("AppendPatch.inf"), {first, fixture.Second()});
	// Recorded defect: true all the same.
	CHECK(CheckTerriblePatch());
	CHECK(fixture.scratch.Read("first.spk") == Bytes("\x03\x00" "ABC", 5));
	CHECK(fixture.SecondUntouched());

	PatchFixture count;
	first = count.First();
	first.finalCount = 4;
	SavePatches(count.scratch.Path("AppendPatch.inf"), {first, count.Second()});
	CHECK(CheckTerriblePatch());
	CHECK(count.SecondUntouched());
}

TEST(ClientMain1Patch, AFileDefinitionsWithoutTheEntryThrows)
{
	FileDef fileDef;
	bool thrown = false;
	try { CheckTerriblePatch(); } catch (NoSuchElementException&) { thrown = true; }
	CHECK(thrown);
}

//======================================================================
// The progress bar
//======================================================================
TEST(ClientMain1Progress, RemovingNoWindowLeavesNone)
{
	ProgressGuard guard;
	g_hWndProgress = NULL;
	RemoveProgressBar();
	CHECK(g_hWndProgress == NULL);
}

TEST(ClientMain1Progress, SwitchedOffItLeavesTheWindowAlone)
{
	ProgressGuard guard;
	g_bUseProgressBar = false;
	g_hWndProgress = FakeWindow();
	CreateProgressBar("create");
	CHECK(g_hWndProgress == FakeWindow());
	SetProgressBarText("text");
	SetProgressBarCount(5);
	UpdateProgressBar();
	CHECK(g_hWndProgress == FakeWindow());

	g_hWndProgress = NULL;
	SetProgressBarText("text");
	CHECK(g_hWndProgress == NULL);
}

#ifndef PLATFORM_WINDOWS
// Off Windows the shim creates no window; on Windows these would make or
// address real ones.
TEST(ClientMain1Progress, RemovingForgetsTheWindow)
{
	ProgressGuard guard;
	g_hWndProgress = FakeWindow();
	RemoveProgressBar();
	CHECK(g_hWndProgress == NULL);
}

TEST(ClientMain1Progress, OffWindowsNoWindowIsMade)
{
	ProgressGuard guard;
	g_bUseProgressBar = true;
	// The last window removed, none made.
	g_hWndProgress = FakeWindow();
	CreateProgressBar("create");
	CHECK(g_hWndProgress == NULL);
	SetProgressBarText("first");
	CHECK(g_hWndProgress == NULL);

	// An existing window is retitled, counted and stepped, and kept.
	g_hWndProgress = FakeWindow();
	SetProgressBarText("next");
	SetProgressBarCount(3);
	UpdateProgressBar();
	CHECK(g_hWndProgress == FakeWindow());
}
#endif
