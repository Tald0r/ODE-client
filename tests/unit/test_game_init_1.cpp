#include "test_framework.h"
#include "GameInit.h"
#include "GameMain.h"
#include "GameInitInfoHost.h"
#include "GameInitInfo.h"
#include "ClientConfig.h"
#include "ClientMode.h"
#include "InputState.h"
#include "UserOption.h"
#include "UserInformation.h"
#include "Properties.h"
#include "MSoundTable.h"
#include "MGameStringTable.h"
#include "MTopView.h"
#include "UIFunction.h"
#include "CSoundPartManager.h"
#include "CDirectSound.h"
#include "COGGSTREAM.h"
#include "CDirectInput.h"
#include "DXLibBackend.h"
#include "DXLib/DXInputHost.h"
#include "DXLib/CDirectDraw.h"
#include "DXLib/CDirectDrawSurface.h"
#include "CSpriteSurface.h"
#include "CSprite565.h"
#include "CIndexSprite.h"
#include "SpriteLibBackend.h"
#include "SpriteLibBackendSDL.h"
#include "VS_UI.h"
#include "VS_UI_Base.h"
#include "VS_UI_UIMessage.h"
#include "VS_UI_GameCommon.h"
#include "VS_UI_GlobalResource.h"
#include "VS_UI_item.h"
#include "VS_UI_mouse_pointer.h"
#include "SkinManager.h"
#include "pi_core.h"
#include "u_window.h"
#include "U_edit.h"
#include "src/InputFocusManager.h"
#include "Ci.h"
#include "Fl2.h"
#include "timer2.h"
#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// The game's start-up (task 4.264 moved Client/GameInit.cpp into
// gameclient), group 1: the display, the input, the title picture and the
// sound, with the small helpers beside them.
// - InitDraw: g_bHAL is the option's Use3DHAL; one display through
//   CSDLGraphics::Init, 1024x768 for g_MyFull and 800x600 otherwise,
//   fullscreen as g_bFullScreen says; with the HAL g_bEnable3DHAL is TRUE,
//   without it g_bEnable3DHAL is left as it was (recorded defect); then
//   the screen-blend tables and the 5:6:5 gamma function. TRUE always.
// - InitInput: installs the input host (the cursor into g_x/g_y, the
//   activation into g_bActiveApp, the text focus, the editing keys, the
//   typed and composed text to the focused editor, a graphics reset to
//   the view's first draw), makes g_pSDLInput when there is none and
//   starts it. TRUE.
// - InitSurface: a new back buffer, black, and a new transient last frame
//   of CSDLGraphics::Color(30,30,30) (5:6:5 channel values: a pale
//   magenta, not a dark grey), both of g_GameRect's size; the title
//   picture; a view when there is none, given the last frame; the UI
//   released and started again on the last frame with UI_ResultReceiver;
//   the index sprites' colour sets. In MODE_CHANGE_OPTION a view that
//   will not initialise fails it. TRUE otherwise.
// - StartTitleLoading/DrawTitleLoading/EndTitleLoading: the picture is
//   UI sprite 4 (5 for g_MyFull) of FILE_SPRITE_UI through its index; at
//   alpha 32 it is copied onto the back buffer and flipped, below it is
//   drawn onto the last frame with BltSpriteAlpha at that alpha (which
//   blends nothing on a 5:6:5 surface: recorded defect) and not flipped;
//   the end deletes it and, asked to log in with the automatic login on,
//   sends UI_LOGIN with the ID and the key.
// - InitSound/UnInitSound: the audio once, its limit from VolumeSound,
//   the OGG streamer at (VolumeMusic-15)*250 within -10000..-1, 100 sound
//   parts for the sound table; released in reverse. InitVolume and the
//   two add-on loaders answer TRUE and do nothing else.
//
// The display, the frames and the UI run for real on SDL's dummy video
// driver; the audio is the real SDL_mixer on its dummy driver (without
// SDL_mixer InitSound fails, which is pinned instead). The UI needs no data
// files beyond a one-line Interface.inf: every sprite pack it asks for is
// missing and rejected without a word.
//
// Not observable here: the bUseHAL and bUseIME arguments InitDraw passes
// (CSDLGraphics::Init ignores both), so a HAL arm and the plain arm of the
// same size and mode differ only in g_bHAL and g_bEnable3DHAL; InitSurface's
// gamma (SetGammaRamp
// is an empty inline), its SetTransparency(0) calls (0 is a new surface's
// value already), its FL2 fallback source (a file static of
// FL2Rendering.cpp that only text drawing reads) and its deletes of the
// old frames (no hook in a surface's destructor); InitInput's
// SetAcquire(true) (a no-op that only answers). On Windows the dummy video
// driver cannot wrap a window (SDL_CreateWindowFrom), so the display's size
// and mode are pinned off Windows only.
//
// Not reachable:
// - InitDraw's "3D failed" block and its four arms (GameInit.cpp:721-
//   752): bUse3D is g_bHAL there, since the free video memory is a
//   constant 256 MB, so g_bHAL && !bUse3D never holds.
// - InitInput's failure (InitFailMessage("DirectInput Error!"), FALSE):
//   dxlib_input_init fails only when SDL_Init(0) does.
// - StartTitleLoading's CSprite555 arm: CSDLGraphics::Is565() is true.
//
// Needs the data tree: InitSurface in MODE_CHANGE_OPTION with a view not
// yet initialised (MTopView::Init loads the game's sprite packs; without
// them an exception escapes InitSurface), so its TRUE there.
//
// Not run (undefined behaviour):
// - DrawTitleLoading with a picture and no surface to draw it on:
//   g_pBack at alpha 32, g_pLast below it (GameInit.cpp:286-296 check
//   neither). InitSurface's first DrawTitleLoading (line 453) comes before
//   it makes g_pLast, so a first InitSurface with a title picture and
//   g_TitleSpriteAlpha below 32 would draw through a NULL g_pLast; the
//   start-up is safe only because g_TitleSpriteAlpha starts at 32.
// - Unchecked reads of a missing object: InitSound's option,
//   configuration, sound cache and sound table once the audio starts;
//   InitDraw's and InitSurface's option; StartTitleLoading's file table
//   without a picture; EndTitleLoading's user information when asked to
//   log in; InitSurface's IME, game strings and (in MODE_CHANGE_OPTION)
//   view.

extern CSpriteSurface*	g_pBack;
extern CSpriteSurface*	g_pLast;
extern RECT				g_GameRect;
extern BOOL				g_MyFull;
extern bool				g_bFullScreen;
extern HWND				g_hWnd;
extern BOOL				g_bEnable3DHAL;
extern BOOL				g_bActiveApp;
extern int				g_TitleSpriteAlpha;
extern timer_id_t		g_interface_blink_tid;	// Vs_ui.cpp's

namespace {

//----------------------------------------------------------------------
// Private members the tests read: an explicit instantiation may name them.
//----------------------------------------------------------------------
template <class Tag, typename Tag::type Member>
struct Expose
{
	friend typename Tag::type Get(Tag) { return Member; }
};
struct ReceiverOf { using type = C_VS_UI_UI_RESULT_RECEIVER Base::*; friend type Get(ReceiverOf); };
template struct Expose<ReceiverOf, &Base::m_C_ui_result_receiver>;
struct ReceiverProcOf { using type = void (*C_VS_UI_UI_RESULT_RECEIVER::*)(DWORD, intptr_t, intptr_t, void*); friend type Get(ReceiverProcOf); };
template struct Expose<ReceiverProcOf, &C_VS_UI_UI_RESULT_RECEIVER::m_fp_result_receiver>;
struct OggVolumeOf { using type = long COGGSTREAM::*; friend type Get(OggVolumeOf); };
template struct Expose<OggVolumeOf, &COGGSTREAM::m_Volume>;
struct ViewInitOf { using type = bool MTopView::*; friend type Get(ViewInitOf); };
template struct Expose<ViewInitOf, &MTopView::m_bInit>;
struct ViewSurfaceOf { using type = CSpriteSurface* MTopView::*; friend type Get(ViewSurfaceOf); };
template struct Expose<ViewSurfaceOf, &MTopView::m_pSurface>;
struct ViewFirstDrawOf { using type = bool MTopView::*; friend type Get(ViewFirstDrawOf); };
template struct Expose<ViewFirstDrawOf, &MTopView::m_bFirstTileDraw>;
struct WindowOf { using type = SDL_Window**; friend type Get(WindowOf); };
template struct Expose<WindowOf, &CSDLGraphics::m_pSDLWindow>;
struct HwndOf { using type = HWND*; friend type Get(HwndOf); };
template struct Expose<HwndOf, &CSDLGraphics::m_hWnd>;
struct FullscreenOf { using type = bool*; friend type Get(FullscreenOf); };
template struct Expose<FullscreenOf, &CSDLGraphics::m_bFullscreen>;
struct ScreenWidthOf { using type = WORD*; friend type Get(ScreenWidthOf); };
template struct Expose<ScreenWidthOf, &CSDLGraphics::m_ScreenWidth>;
struct ScreenHeightOf { using type = WORD*; friend type Get(ScreenHeightOf); };
template struct Expose<ScreenHeightOf, &CSDLGraphics::m_ScreenHeight>;
using GammaFunction = void (*)(void*, int, int);
struct GammaOf { using type = GammaFunction*; friend type Get(GammaOf); };
template struct Expose<GammaOf, &CDirectDrawSurface::s_GammaFunction>;

std::string calls;
void Call(const std::string& call) { calls += call + ";"; }

bool Same(const std::string& expected, const std::string& actual)
{
	if (expected != actual)
		std::fprintf(stderr, "    expected %s\n    actual   %s\n", expected.c_str(), actual.c_str());
	return expected == actual;
}
#define CHECK_SAME(expected, actual) CHECK(Same((expected), (actual)))

int flips = 0;
spritectl_surface_t Flipped() { ++flips; return SPRITECTL_INVALID_SURFACE; }

const GameInitInfoHost::Host failHost{
	.InitFailMessage = [](const char* message) { Call(std::string("fail:") + message); },
};

using ScreenTables = std::vector<WORD>;
ScreenTables EffectTables()
{
	ScreenTables tables;
	auto add = [&tables](const WORD* table, std::size_t count) { tables.insert(tables.end(), table, table + count); };
	add(&CSpriteSurface::s_EffectScreenTableR[0][0], 32 * 32);
	add(&CSpriteSurface::s_EffectScreenTableG[0][0], 64 * 64);
	add(&CSpriteSurface::s_EffectScreenTableB[0][0], 32 * 32);
	return tables;
}
void SetEffectTables(const ScreenTables& tables)
{
	std::copy_n(tables.begin(), 32 * 32, &CSpriteSurface::s_EffectScreenTableR[0][0]);
	std::copy_n(tables.begin() + 32 * 32, 64 * 64, &CSpriteSurface::s_EffectScreenTableG[0][0]);
	std::copy_n(tables.begin() + 32 * 32 + 64 * 64, 32 * 32, &CSpriteSurface::s_EffectScreenTableB[0][0]);
}

std::vector<WORD> ColorSets()
{
	const WORD* first = &CIndexSprite::ColorSet[0][0];
	return std::vector<WORD>(first, first + MAX_COLORSET * MAX_COLORGRADATION);
}
void SetColorSets(const std::vector<WORD>& sets)
{
	std::copy(sets.begin(), sets.end(), &CIndexSprite::ColorSet[0][0]);
}

//----------------------------------------------------------------------
// Every global a test here changes, given back after.
//----------------------------------------------------------------------
struct Saved
{
	bool titleLoading = g_bTitleLoading;
	CSprite* titleSprite = g_pTitleLoadingSprite;
	int titleAlpha = g_TitleSpriteAlpha;
	Properties* files = g_pFileDef;
	BOOL myFull = g_MyFull;
	bool fullScreen = g_bFullScreen;
	HWND window = g_hWnd;
	bool hal = g_bHAL;
	BOOL enable3D = g_bEnable3DHAL;
	UserOption* option = g_pUserOption;
	ClientConfig* config = g_pClientConfig;
	CSoundPartManager* cache = g_pSoundManager;
	SOUND_TABLE* sounds = g_pSoundTable;
	COGGSTREAM* ogg = g_pOGG;
	CSDLInput* input = g_pSDLInput;
	int x = g_x, y = g_y;
	BOOL activeApp = g_bActiveApp;
	MTopView* view = g_pTopView;
	CSpriteSurface* back = g_pBack;
	CSpriteSurface* last = g_pLast;
	RECT gameRect = g_GameRect;
	CLIENT_MODE mode = g_Mode;
	UserInformation* user = g_pUserInformation;
	Base* base = gpC_base;
	CI* ime = gC_ci;
	MStringArray* strings = g_pGameStringTable;
	PI_ProcessRunner* runner = gpC_process_runner;
	WindowManager* windows = gpC_window_manager;
	C_VS_UI_WINDOW_MANAGER* uiWindows = gpC_vs_ui_window_manager;
	C_GLOBAL_RESOURCE* resource = gpC_global_resource;
	C_VS_UI_MOUSE_POINTER* pointer = gpC_mouse_pointer;
	C_VS_UI_ITEM* item = gpC_item;
	SkinManager* skin = g_pSkinManager;
	timer_id_t blink = g_interface_blink_tid;
	DXInput::Host inputHost = DXInput::GetHost();
	const GameInitInfoHost::Host* infoHost = GameInitInfoHost::SetHost(&failHost);
	HWND screenWindow = *Get(HwndOf());
	bool screenFull = *Get(FullscreenOf());
	WORD screenWidth = *Get(ScreenWidthOf());
	WORD screenHeight = *Get(ScreenHeightOf());
	GammaFunction gamma = *Get(GammaOf());
	ScreenTables effects = EffectTables();
	std::vector<WORD> colorSets = ColorSets();
	LONG volumeLimit = g_SDLAudio.GetVolumeLimit();
	bool mute = g_SDLAudio.IsMute();

	Saved()
	{
		calls.clear();
		flips = 0;
		CSDLGraphics::SetBackBufferSource(Flipped);
	}
	~Saved()
	{
		CHECK_SAME("", calls);
		CSDLGraphics::SetBackBufferSource(NULL);
		g_SDLAudio.SetVolumeLimit(volumeLimit);
		if (mute) g_SDLAudio.SetMute(); else g_SDLAudio.UnSetMute();
		SetColorSets(colorSets);
		SetEffectTables(effects);
		*Get(GammaOf()) = gamma;
		*Get(ScreenHeightOf()) = screenHeight;
		*Get(ScreenWidthOf()) = screenWidth;
		*Get(FullscreenOf()) = screenFull;
		*Get(HwndOf()) = screenWindow;
		GameInitInfoHost::SetHost(infoHost);
		DXInput::SetHost(inputHost);
		g_interface_blink_tid = blink;
		g_pSkinManager = skin;
		gpC_item = item;
		gpC_mouse_pointer = pointer;
		gpC_global_resource = resource;
		gpC_vs_ui_window_manager = uiWindows;
		gpC_window_manager = windows;
		gpC_process_runner = runner;
		g_pGameStringTable = strings;
		gC_ci = ime;
		gpC_base = base;
		g_pUserInformation = user;
		g_Mode = mode;
		g_GameRect = gameRect;
		g_pLast = last;
		g_pBack = back;
		g_pTopView = view;
		g_bActiveApp = activeApp;
		g_y = y;
		g_x = x;
		g_pSDLInput = input;
		g_pOGG = ogg;
		g_pSoundTable = sounds;
		g_pSoundManager = cache;
		g_pClientConfig = config;
		g_pUserOption = option;
		g_bEnable3DHAL = enable3D;
		g_bHAL = hal;
		g_hWnd = window;
		g_bFullScreen = fullScreen;
		g_MyFull = myFull;
		g_pFileDef = files;
		g_TitleSpriteAlpha = titleAlpha;
		g_pTitleLoadingSprite = titleSprite;
		g_bTitleLoading = titleLoading;
	}
};

// SDL's video on the dummy driver, and the sprite backend over it.
struct Video
{
	std::string previousDriver = SDL_getenv("SDL_VIDEODRIVER") ? SDL_getenv("SDL_VIDEODRIVER") : "";
	Video()
	{
		SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
		CHECK_EQ(0, SDL_InitSubSystem(SDL_INIT_VIDEO));
		CHECK_EQ(0, spritectl_init());
	}
	~Video()
	{
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		SDL_setenv("SDL_VIDEODRIVER", previousDriver.c_str(), 1);
	}
};

// A directory of the test's own, removed with what is in it.
struct TempDir
{
	std::filesystem::path path;
	TempDir()
	{
		static unsigned serial = 0;
		for (unsigned attempt = 0; attempt < 100; ++attempt)
		{
			auto candidate = std::filesystem::temp_directory_path() / ("darkeden_game_init_1_"
				+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
				+ "_" + std::to_string(++serial));
			if (std::filesystem::create_directory(candidate))
			{
				path = candidate;
				return;
			}
		}
		throw std::runtime_error("Cannot reserve a fixture directory");
	}
	~TempDir() { std::error_code error; std::filesystem::remove_all(path, error); }
	std::string File(const std::string& name) const { return (path / name).string(); }
};

// Little-endian bytes for the sprite pack and its index.
struct Bytes
{
	std::vector<char> data;
	Bytes& U16(unsigned value) { data.push_back(char(value)); data.push_back(char(value >> 8)); return *this; }
	Bytes& U32(unsigned value) { U16(value & 0xffff); return U16(value >> 16); }
	void Save(const std::string& path) const
	{
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file.write(data.data(), static_cast<std::streamsize>(data.size()));
		CHECK(file.good());
	}
};

// Two pictures in a UI pack: a 2x1 one at offset 2 and a 3x1 one after it.
const unsigned kNarrowAt = 2, kWideAt = 18;
void WriteTitlePack(const std::string& path)
{
	Bytes pack;
	pack.U16(2);
	for (unsigned word : {2u, 1u, 5u, 1u, 0u, 2u, 0xffffu, 0x07e0u}) pack.U16(word);
	CHECK_EQ(size_t(kWideAt), pack.data.size());
	for (unsigned word : {3u, 1u, 6u, 1u, 0u, 3u, 0xf800u, 0x001fu, 0x07e0u}) pack.U16(word);
	pack.Save(path);
}
// An index of the given offsets.
void WriteIndex(const std::string& path, const std::vector<unsigned>& offsets)
{
	Bytes index;
	index.U16(unsigned(offsets.size()));
	for (unsigned offset : offsets) index.U32(offset);
	index.Save(path);
}
// Ids 0 to 3 the narrow picture, 4 the narrow one, 5 the wide one.
const std::vector<unsigned> kSixPictures{kNarrowAt, kNarrowAt, kNarrowAt, kNarrowAt, kNarrowAt, kWideAt};

// The UI pack and its index in a directory, named in the file table.
struct TitleFiles
{
	TempDir dir;
	Properties files;
	TitleFiles()
	{
		files.setProperty("FILE_SPRITEINDEX_UI", dir.File("ui.spki"));
		files.setProperty("FILE_SPRITE_UI", dir.File("ui.spk"));
		g_pFileDef = &files;
	}
	void Write(const std::vector<unsigned>& offsets)
	{
		WriteTitlePack(dir.File("ui.spk"));
		WriteIndex(dir.File("ui.spki"), offsets);
	}
};

std::vector<WORD> Picture(CSpriteSurface& surface)
{
	std::vector<WORD> pixels;
	spritectl_surface_info_t info{};
	if (spritectl_lock_surface(surface.GetBackendSurface(), &info) != 0 || !info.pixels) return pixels;
	for (int row = 0; row < info.height; ++row)
		for (int column = 0; column < info.width; ++column)
			pixels.push_back(reinterpret_cast<WORD*>(static_cast<BYTE*>(info.pixels) + row * info.pitch)[column]);
	spritectl_unlock_surface(surface.GetBackendSurface());
	return pixels;
}

// A surface of the size, filled with the colour.
struct Canvas
{
	CSpriteSurface surface;
	Canvas(int width, int height, WORD color)
	{
		CHECK(surface.Init(width, height));
		surface.FillSurface(color);
	}
};

// A 2x1 picture: white, then green.
CSprite* NarrowSprite()
{
	TempDir dir;
	Bytes words;
	for (unsigned word : {2u, 1u, 5u, 1u, 0u, 2u, 0xffffu, 0x07e0u}) words.U16(word);
	words.Save(dir.File("narrow.bin"));
	std::ifstream file(dir.File("narrow.bin"), std::ios::binary);
	CSprite565* sprite = new CSprite565;
	CHECK(sprite->LoadFromFile(file));
	return sprite;
}

} // namespace

//----------------------------------------------------------------------
// The helpers
//----------------------------------------------------------------------
TEST(GameInit1Helpers, TheVolumeStepAndTheAddonLoadersAnswerTrueAndTouchNothing)
{
	Saved saved;
	// Nothing to read: a read of any of these would stop the test.
	g_pUserOption = NULL;
	g_pClientConfig = NULL;
	g_pFileDef = NULL;
	g_SDLAudio.SetVolumeLimit(-1234);
	CHECK_EQ(TRUE, InitVolume());
	CHECK(PrepareLoadingAddonSPK());
	CHECK(LoadingAddonSPK(false));
	CHECK(LoadingAddonSPK(true));
	CHECK_EQ(-1234L, long(g_SDLAudio.GetVolumeLimit()));
	CHECK(g_pTitleLoadingSprite == saved.titleSprite);
	CHECK_EQ(0, flips);
}

//----------------------------------------------------------------------
// The title picture
//----------------------------------------------------------------------
TEST(GameInit1Title, StartingWithAPictureOnlyStartsTheLoading)
{
	Saved saved;
	std::unique_ptr<CSprite> sprite(NarrowSprite());
	g_pTitleLoadingSprite = sprite.get();
	g_bTitleLoading = false;
	g_pFileDef = NULL;	// no file is read
	StartTitleLoading();
	CHECK(g_bTitleLoading);
	CHECK(g_pTitleLoadingSprite == sprite.get());
	g_pTitleLoadingSprite = NULL;
}

TEST(GameInit1Title, TheWindowedPictureIsUISpriteFourAndTheLargeOneSpriteFive)
{
	for (BOOL large : {FALSE, TRUE})
	{
		Saved saved;
		TitleFiles title;
		title.Write(kSixPictures);
		g_pTitleLoadingSprite = NULL;
		g_bTitleLoading = false;
		g_MyFull = large;
		StartTitleLoading();
		CHECK(g_bTitleLoading);
		CHECK(g_pTitleLoadingSprite != NULL);
		if (g_pTitleLoadingSprite == NULL) continue;
		CHECK(dynamic_cast<CSprite565*>(g_pTitleLoadingSprite) != NULL);
		CHECK_EQ(large ? 3 : 2, int(g_pTitleLoadingSprite->GetWidth()));
		CHECK_EQ(1, int(g_pTitleLoadingSprite->GetHeight()));
		delete g_pTitleLoadingSprite;
		g_pTitleLoadingSprite = NULL;
	}
}

TEST(GameInit1Title, AnIndexWithoutThePicturesIdLeavesNoPicture)
{
	// Five entries reach id 4 (the windowed picture) and not id 5; four
	// reach neither.
	struct Case { std::size_t entries; BOOL large; bool loaded; };
	for (const Case& c : {Case{5, FALSE, true}, Case{5, TRUE, false}, Case{4, FALSE, false}, Case{0, FALSE, false}})
	{
		Saved saved;
		TitleFiles title;
		title.Write(std::vector<unsigned>(kSixPictures.begin(), kSixPictures.begin() + std::ptrdiff_t(c.entries)));
		g_pTitleLoadingSprite = NULL;
		g_bTitleLoading = false;
		g_MyFull = c.large;
		StartTitleLoading();
		CHECK(g_bTitleLoading);
		CHECK_EQ(c.loaded, g_pTitleLoadingSprite != NULL);
		delete g_pTitleLoadingSprite;
		g_pTitleLoadingSprite = NULL;
	}
}

TEST(GameInit1Title, AMissingOrRejectedFileLeavesNoPicture)
{
	enum What { NoIndex, BadIndex, NoPack, CutPack, PastEnd };
	for (What what : {NoIndex, BadIndex, NoPack, CutPack, PastEnd})
	{
		Saved saved;
		TitleFiles title;
		title.Write(kSixPictures);
		if (what == NoIndex) std::filesystem::remove(title.dir.File("ui.spki"));
		// An offset of 1 is inside the pack's count: the index is refused.
		if (what == BadIndex) WriteIndex(title.dir.File("ui.spki"), {2, 2, 2, 2, 1, 18});
		if (what == NoPack) std::filesystem::remove(title.dir.File("ui.spk"));
		if (what == CutPack) std::filesystem::resize_file(title.dir.File("ui.spk"), kNarrowAt + 10);
		// The index is not checked against the pack: an offset past its end
		// is accepted there and the read fails.
		if (what == PastEnd) WriteIndex(title.dir.File("ui.spki"), {2, 2, 2, 2, 1000, 18});
		g_pTitleLoadingSprite = NULL;
		g_bTitleLoading = false;
		g_MyFull = FALSE;
		StartTitleLoading();
		CHECK(g_bTitleLoading);
		CHECK(g_pTitleLoadingSprite == NULL);
		delete g_pTitleLoadingSprite;
		g_pTitleLoadingSprite = NULL;
	}
}

TEST(GameInit1Title, NotLoadingDrawsNothing)
{
	Saved saved;
	std::unique_ptr<CSprite> sprite(NarrowSprite());
	g_pTitleLoadingSprite = sprite.get();
	g_bTitleLoading = false;
	g_pBack = g_pLast = NULL;	// neither is touched
	for (int alpha : {32, 16})
	{
		g_TitleSpriteAlpha = alpha;
		DrawTitleLoading();
	}
	CHECK_EQ(0, flips);
	g_pTitleLoadingSprite = NULL;
}

TEST(GameInit1Title, WithoutAPictureOnlyFullAlphaFlips)
{
	Saved saved;
	g_pTitleLoadingSprite = NULL;
	g_bTitleLoading = true;
	g_pBack = g_pLast = NULL;	// neither is touched
	struct Case { int alpha; int flips; };
	for (const Case& c : {Case{32, 1}, Case{31, 0}, Case{33, 0}, Case{0, 0}})
	{
		flips = 0;
		g_TitleSpriteAlpha = c.alpha;
		DrawTitleLoading();
		CHECK_EQ(c.flips, flips);
	}
}

TEST(GameInit1Title, AtFullAlphaThePictureIsCopiedOntoTheBackBufferAndFlipped)
{
	Saved saved;
	Video video;
	std::unique_ptr<CSprite> sprite(NarrowSprite());
	Canvas back(4, 2, 0x1234);
	g_pTitleLoadingSprite = sprite.get();
	g_bTitleLoading = true;
	g_TitleSpriteAlpha = 32;
	g_pBack = &back.surface;
	g_pLast = NULL;	// not touched
	DrawTitleLoading();
	CHECK_EQ(1, flips);
	const std::vector<WORD> expected{0xffff, 0x07e0, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234, 0x1234};
	CHECK(expected == Picture(back.surface));
	CHECK(!back.surface.IsLocked());
	g_pTitleLoadingSprite = NULL;
}

TEST(GameInit1Title, BelowFullAlphaThePictureGoesOntoTheLastFrameWithoutAFlip)
{
	Saved saved;
	Video video;
	std::unique_ptr<CSprite> sprite(NarrowSprite());
	g_pTitleLoadingSprite = sprite.get();
	g_bTitleLoading = true;
	g_pBack = NULL;	// not touched
	for (int alpha : {31, 16, 1})
	{
		Canvas last(4, 2, 0x1234), blended(4, 2, 0x1234), copied(4, 2, 0x1234);
		POINT zero = {0, 0};
		blended.surface.BltSpriteAlpha(&zero, sprite.get(), BYTE(alpha));
		copied.surface.BltSprite(&zero, sprite.get());
		g_TitleSpriteAlpha = alpha;
		g_pLast = &last.surface;
		flips = 0;
		DrawTitleLoading();
		CHECK_EQ(0, flips);
		CHECK(Picture(blended.surface) == Picture(last.surface));
		// Recorded defect (SpriteLib): on a 5:6:5 surface BltSpriteAlpha
		// writes an RLE sprite's pixels unblended (SpriteLibBackendSDL.cpp
		// blends only into 32-bit surfaces; SpriteGpu.cpp's opacity is 255
		// for RLE), so the title picture does not fade on the SDL backend.
		CHECK(Picture(copied.surface) == Picture(last.surface));
		CHECK(!last.surface.IsLocked());
	}
	g_pTitleLoadingSprite = NULL;
}

TEST(GameInit1Title, TheEndDeletesThePictureAndStopsTheLoading)
{
	for (bool picture : {true, false})
	{
		Saved saved;
		g_pTitleLoadingSprite = picture ? NarrowSprite() : NULL;
		g_bTitleLoading = true;
		g_pUserInformation = NULL;	// not read without the login
		EndTitleLoading();
		CHECK(!g_bTitleLoading);
		CHECK(g_pTitleLoadingSprite == NULL);
		g_bTitleLoading = true;
		EndTitleLoading(false);
		CHECK(!g_bTitleLoading);
	}
}

namespace {
// What the UI's receiver was handed, LOGIN payloads copied out.
std::vector<std::string> logins;
const LOGIN* lastLogin = NULL;
void ReceivedLogin(DWORD message, intptr_t left, intptr_t right, void* pointer)
{
	const LOGIN* login = static_cast<const LOGIN*>(pointer);
	lastLogin = login;
	logins.push_back(std::to_string(message) + "," + std::to_string(left) + "," + std::to_string(right) + ":"
		+ (message == UI_LOGIN && login ? std::string(login->sz_id) + "/" + login->sz_password : std::string("?")));
}
std::string LoginOf(const char* id, const char* key)
{
	return std::to_string(UI_LOGIN) + ",0,0:" + id + "/" + key;
}
}

TEST(GameInit1Title, TheEndSendsTheAutomaticLoginOnlyWhenAskedAndOn)
{
	struct Case { bool send; bool automatic; bool sent; };
	for (const Case& c : {Case{true, true, true}, Case{true, false, false}, Case{false, true, false}})
	{
		Saved saved;
		UserInformation user;
		user.IsAutoLogIn = c.automatic;
		user.UserID = "tester";
		user.AutoLogInKeyValue = "key123";
		g_pUserInformation = &user;
		Base base;
		(base.*Get(ReceiverOf())).SetResultReceiver(ReceivedLogin);
		gpC_base = &base;
		logins.clear();
		g_pTitleLoadingSprite = NULL;
		EndTitleLoading(c.send);
		base.DispatchMessage();
		base.DispatchMessage();
		CHECK_EQ(size_t(c.sent ? 1 : 0), logins.size());
		if (c.sent && logins.size() == 1) CHECK_SAME(LoginOf("tester", "key123"), logins[0]);
	}
}

TEST(GameInit1Title, TheLoginIsOneStaticRecordRefilledEachTime)
{
	Saved saved;
	UserInformation user;
	user.IsAutoLogIn = true;
	g_pUserInformation = &user;
	Base base;
	(base.*Get(ReceiverOf())).SetResultReceiver(ReceivedLogin);
	gpC_base = &base;
	logins.clear();
	user.UserID = "first";
	user.AutoLogInKeyValue = "one";
	EndTitleLoading(true);
	base.DispatchMessage();
	const LOGIN* first = lastLogin;
	// An empty ID and key (GetString() NULL) are sent as empty strings.
	user.UserID = "";
	user.AutoLogInKeyValue = "";
	CHECK(user.UserID.GetString() == NULL);
	EndTitleLoading(true);
	base.DispatchMessage();
	user.UserID = "second-id";
	user.AutoLogInKeyValue = "two";
	EndTitleLoading(true);
	base.DispatchMessage();
	CHECK_EQ(size_t(3), logins.size());
	if (logins.size() == 3)
	{
		CHECK_SAME(LoginOf("first", "one"), logins[0]);
		CHECK_SAME(LoginOf("", ""), logins[1]);
		CHECK_SAME(LoginOf("second-id", "two"), logins[2]);
	}
	// Recorded defect: the record is a function static and the two arrays
	// each call allocates for it are never freed (GameInit.cpp:333-335).
	CHECK(lastLogin == first);
}

//----------------------------------------------------------------------
// The input
//----------------------------------------------------------------------
TEST(GameInit1Input, WithoutAnInputItMakesOneAndStartsIt)
{
	Saved saved;
	Video video;
	g_pSDLInput = NULL;
	CHECK_EQ(TRUE, InitInput());
	CHECK(g_pSDLInput != NULL);
	if (g_pSDLInput == NULL) return;
	CHECK_EQ(HRESULT(S_OK), g_pSDLInput->SetAcquire(true));
	// The backend takes keys now.
	dxlib_input_virtual_key(DIK_F5, 1);
	CHECK_EQ(1, dxlib_input_key_down(DIK_F5));
	dxlib_input_virtual_reset();
	delete g_pSDLInput;
	CHECK_EQ(0, dxlib_input_key_down(DIK_F5));
}

TEST(GameInit1Input, AnInputThereIsIsKeptAndStarted)
{
	Saved saved;
	Video video;
	CSDLInput input;
	CHECK_EQ(HRESULT(S_FALSE), input.SetAcquire(true));
	g_pSDLInput = &input;
	CHECK_EQ(TRUE, InitInput());
	CHECK(g_pSDLInput == &input);
	CHECK_EQ(HRESULT(S_OK), input.SetAcquire(true));
	dxlib_input_virtual_key(DIK_F5, 1);
	CHECK_EQ(1, dxlib_input_key_down(DIK_F5));
	dxlib_input_virtual_reset();
}

TEST(GameInit1Input, TheHostMovesTheCursorAndActivatesTheGame)
{
	Saved saved;
	Video video;
	CSDLInput input;
	g_pSDLInput = &input;
	DXInput::SetHost({});
	CHECK_EQ(TRUE, InitInput());
	const DXInput::Host& host = DXInput::GetHost();
	CHECK(host.mousePosition != NULL && host.activeApp != NULL && host.hasTextFocus != NULL
		&& host.keyDown != NULL && host.textInput != NULL && host.textEditing != NULL && host.graphicsReset != NULL);
	g_x = g_y = 0;
	DXInput::SetMousePosition(12, 34);
	CHECK_EQ(12, g_x);
	CHECK_EQ(34, g_y);
	host.mousePosition(-5, 700);
	CHECK_EQ(-5, g_x);
	CHECK_EQ(700, g_y);
	g_bActiveApp = TRUE;
	DXInput::SetActiveApp(false);
	CHECK_EQ(FALSE, g_bActiveApp);
	host.activeApp(true);
	CHECK_EQ(TRUE, g_bActiveApp);
}

TEST(GameInit1Input, TheHostHandsTheKeysAndTheTextToTheFocusedEditor)
{
	Saved saved;
	Video video;
	CSDLInput input;
	g_pSDLInput = &input;
	CHECK_EQ(TRUE, InitInput());
	const DXInput::Host host = DXInput::GetHost();
	InputFocusManager::GetInstance().SetFocusedEditor(nullptr);
	LineEditorVisual editor, other;
	// No focus: the text goes nowhere.
	CHECK(!host.hasTextFocus());
	host.textInput("lost");
	host.keyDown(0x08);
	CHECK_EQ(0, editor.Size());
	CHECK_EQ(0, other.Size());
	editor.Acquire();
	CHECK(host.hasTextFocus());
	host.textInput("ab");
	CHECK(std::strcmp(editor.GetString(), "ab") == 0);
	host.keyDown(0x08);	// VK_BACK
	CHECK(std::strcmp(editor.GetString(), "a") == 0);
	CHECK(!editor.m_Editor.IsComposing());
	host.textEditing("pre", 3, 0);
	CHECK(editor.m_Editor.IsComposing());
	host.textInput("c");
	CHECK(std::strcmp(editor.GetString(), "ac") == 0);
	CHECK_EQ(0, other.Size());
	editor.Unacquire();
	CHECK(!host.hasTextFocus());
}

TEST(GameInit1Input, AGraphicsResetRedrawsTheViewsTiles)
{
	Saved saved;
	Video video;
	CSDLInput input;
	g_pSDLInput = &input;
	CHECK_EQ(TRUE, InitInput());
	g_pTopView = NULL;
	DXInput::GetHost().graphicsReset();	// no view: nothing
	MTopView view;
	view.*Get(ViewFirstDrawOf()) = false;
	g_pTopView = &view;
	DXInput::GetHost().graphicsReset();
	CHECK(view.*Get(ViewFirstDrawOf()));
}

//----------------------------------------------------------------------
// The display
//----------------------------------------------------------------------
namespace {
// The display InitDraw makes, released after.
struct DisplayScene
{
	Saved saved;
	Video video;
	UserOption option;
	int anchor = 0;
	DisplayScene()
	{
		g_pUserOption = &option;
		g_hWnd = NULL;
#ifndef PLATFORM_WINDOWS
		// CSDLGraphics::Init only keeps it off Windows: any value tells.
		g_hWnd = reinterpret_cast<HWND>(&anchor);
#endif
		*Get(ScreenWidthOf()) = 0;
		*Get(ScreenHeightOf()) = 0;
	}
	~DisplayScene()
	{
		CSDLGraphics::ReleaseAll();
		// The windows' events (a focus lost among them, which drops the
		// virtual keys) are this test's: the next input pump must not see them.
		SDL_PumpEvents();
		SDL_FlushEvents(SDL_WINDOWEVENT, SDL_KEYDOWN - 1);
	}
};
}

TEST(GameInit1Draw, EachArmOpensOneDisplayOfItsSizeAndMode)
{
	struct Case { BOOL hal; bool fullScreen; BOOL large; WORD width, height; };
	for (const Case& c : {
		Case{TRUE, true, TRUE, 1024, 768}, Case{TRUE, true, FALSE, 800, 600},
		Case{TRUE, false, TRUE, 1024, 768}, Case{TRUE, false, FALSE, 800, 600},
		Case{FALSE, true, TRUE, 1024, 768}, Case{FALSE, true, FALSE, 800, 600},
		Case{FALSE, false, TRUE, 1024, 768}, Case{FALSE, false, FALSE, 800, 600},
		Case{7, false, FALSE, 800, 600}})
	{
		for (BOOL before : {FALSE, TRUE})
		{
			DisplayScene display;
			display.option.Use3DHAL = c.hal;
			g_bFullScreen = c.fullScreen;
			g_MyFull = c.large;
			g_bHAL = !c.hal;
			g_bEnable3DHAL = before;
			*Get(FullscreenOf()) = !c.fullScreen;
			CHECK_EQ(TRUE, InitDraw());
			CHECK_EQ(c.hal != FALSE, g_bHAL);
			// Recorded defect: without the HAL g_bEnable3DHAL keeps what it was
			// (GameInit.cpp:711 sets it only under g_bHAL), so turning 3D
			// off in the options leaves it TRUE for UIMessageManager and the
			// title UI.
			CHECK_EQ(c.hal ? TRUE : before, g_bEnable3DHAL);
#ifndef PLATFORM_WINDOWS
			CHECK(*Get(WindowOf()) != NULL);
			CHECK_EQ(c.width, CSDLGraphics::GetScreenWidth());
			CHECK_EQ(c.height, CSDLGraphics::GetScreenHeight());
			CHECK_EQ(c.fullScreen, CSDLGraphics::IsFullscreen());
			CHECK(CSDLGraphics::GetHwnd() == g_hWnd);
#endif
		}
	}
}

#ifndef PLATFORM_WINDOWS
TEST(GameInit1Draw, TheDisplayIsMadeOnce)
{
	// A window made and closed just before: the display's is the next one.
	for (BOOL hal : {TRUE, FALSE})
	{
		DisplayScene display;
		display.option.Use3DHAL = hal;
		g_bFullScreen = false;
		g_MyFull = FALSE;
		SDL_Window* probe = SDL_CreateWindow("probe", 0, 0, 8, 8, SDL_WINDOW_HIDDEN);
		CHECK(probe != NULL);
		if (probe == NULL) continue;
		const Uint32 id = SDL_GetWindowID(probe);
		SDL_DestroyWindow(probe);
		CHECK_EQ(TRUE, InitDraw());
		CHECK(*Get(WindowOf()) != NULL);
		if (*Get(WindowOf()) != NULL) CHECK_EQ(id + 1, SDL_GetWindowID(*Get(WindowOf())));
	}
}
#endif

TEST(GameInit1Draw, ItFillsTheScreenBlendTablesAndPicksThe565Gamma)
{
	for (BOOL hal : {TRUE, FALSE})
	{
		DisplayScene display;
		display.option.Use3DHAL = hal;
		g_bFullScreen = false;
		g_MyFull = FALSE;
		CSpriteSurface::InitEffectTable();
		const ScreenTables filled = EffectTables();
		SetEffectTables(ScreenTables(filled.size(), 0xBEEF));
		*Get(GammaOf()) = NULL;
		CHECK_EQ(TRUE, InitDraw());
		CHECK(filled == EffectTables());
		CHECK(*Get(GammaOf()) == &CDirectDrawSurface::Gamma4Pixel565);
	}
}

//----------------------------------------------------------------------
// The surfaces
//----------------------------------------------------------------------
namespace {
void NoTimer() {}

// The game's strings for the UI's messages, made once and kept: the UI
// keeps pointers into the table in function statics (g_InitMessage).
MStringArray* GameStrings()
{
	static MStringArray* strings = [] {
		MStringArray* table = new MStringArray;
		MStringArray* previous = g_pGameStringTable;
		g_pGameStringTable = table;
		InitGameStringTable();
		g_pGameStringTable = previous;
		return table;
	}();
	return strings;
}

const WORD kLastFill = CSDLGraphics::Color(30, 30, 30);
const int kWidth = 320, kHeight = 240;

// What InitSurface needs: the video, a working directory holding the UI's
// Interface.inf, the title's files, the option, the configuration, the
// IME and the game's strings; the UI torn down and the frames, the view
// and the title deleted after.
struct Surfaces
{
	Saved saved;
	Video video;
	TitleFiles title;
	std::filesystem::path previousDirectory = std::filesystem::current_path();
	UserOption option;
	ClientConfig config;
	CI_KOREAN ime;
	timer_id_t blinker = INVALID_TID;
	bool ownsView = true;
	Surfaces()
	{
		std::filesystem::create_directories(title.dir.path / "Data" / "Info");
		std::ofstream skin(title.dir.path / "Data" / "Info" / "Interface.inf", std::ios::binary);
		skin << "*INFO POINT_LIST\n10 20\n*END\n";
		skin.close();
		std::filesystem::current_path(title.dir.path);
		g_pUserOption = &option;
		g_pClientConfig = &config;
		gC_ci = &ime;
		g_pGameStringTable = GameStrings();
		g_GameRect = {0, 0, kWidth, kHeight};
		g_Mode = MODE_MAINMENU;
		g_TitleSpriteAlpha = 32;
		g_bTitleLoading = false;
		g_pTitleLoadingSprite = NULL;
		g_pBack = g_pLast = NULL;
		g_pTopView = NULL;
		gpC_base = NULL;
		gpC_process_runner = NULL;
		gpC_window_manager = NULL;
		gpC_vs_ui_window_manager = NULL;
		gpC_global_resource = NULL;
		gpC_mouse_pointer = NULL;
		gpC_item = NULL;
		g_pSkinManager = NULL;
		// The UI starts the IME's cursor blinker and then its own blinker:
		// the two timers after this one.
		timer_id_t probe = gC_timer2.Add(1000, NoTimer);
		blinker = probe + 1;
		gC_timer2.Delete(probe);
	}
	~Surfaces()
	{
		if (gpC_base != NULL)
		{
			CHECK_EQ(blinker + 1, g_interface_blink_tid);
			CHECK(gC_timer2.Delete(blinker));
			CHECK(gC_timer2.Delete(g_interface_blink_tid));
		}
		gC_vs_ui.Release();
		g_SetFL2Surface(nullptr);
		g_SetFL2FallbackSurface(nullptr);
		if (ownsView && g_pTopView != NULL)
		{
			g_pTopView->*Get(ViewInitOf()) = false;
			delete g_pTopView;
		}
		delete g_pLast;
		delete g_pBack;
		delete g_pTitleLoadingSprite;
		std::filesystem::current_path(previousDirectory);
	}
};
}

TEST(GameInit1Surface, AtStartUpItMakesTheFramesTheViewAndTheUI)
{
	Surfaces scene;
	CIndexSprite::SetColorSet();
	const std::vector<WORD> filled = ColorSets();
	SetColorSets(std::vector<WORD>(filled.size(), 0xBEEF));
	// The UI there is released first: this skin gives way to Interface.inf's.
	{
		std::ofstream other(scene.title.dir.path / "Data" / "Info" / "Other.inf", std::ios::binary);
		other << "*INFO POINT_LIST\n1 2\n*END\n";
	}
	g_pSkinManager = new SkinManager;
	CHECK(g_pSkinManager->LoadInformation("Other.inf"));
	CHECK_EQ(1, int(g_pSkinManager->Get(SkinManager::INFO).GetPoint(0).x));
	CHECK_EQ(TRUE, InitSurface());

	CHECK(g_pBack != NULL && g_pLast != NULL && g_pTopView != NULL && gpC_base != NULL);
	if (g_pBack == NULL || g_pLast == NULL || g_pTopView == NULL || gpC_base == NULL) return;
	CHECK_EQ(kWidth, g_pBack->GetWidth());
	CHECK_EQ(kHeight, g_pBack->GetHeight());
	CHECK(Picture(*g_pBack) == std::vector<WORD>(size_t(kWidth * kHeight), 0));
	CHECK_EQ(0, g_pBack->GetBackendSurface()->transient);
	CHECK_EQ(kWidth, g_pLast->GetWidth());
	CHECK_EQ(kHeight, g_pLast->GetHeight());
	CHECK(Picture(*g_pLast) == std::vector<WORD>(size_t(kWidth * kHeight), kLastFill));
	CHECK_EQ(1, g_pLast->GetBackendSurface()->transient);
	CHECK(g_pTopView->*Get(ViewSurfaceOf()) == g_pLast);
	CHECK(!(g_pTopView->*Get(ViewInitOf())));
	// The UI on the last frame, answering through UI_ResultReceiver.
	CHECK(gpC_base->m_p_DDSurface_back == g_pLast);
	CHECK((gpC_base->*Get(ReceiverOf())).*Get(ReceiverProcOf()) == &UI_ResultReceiver);
	CHECK(g_pSkinManager != NULL);
	if (g_pSkinManager != NULL) CHECK_EQ(10, int(g_pSkinManager->Get(SkinManager::INFO).GetPoint(0).x));
	CHECK(filled == ColorSets());
	// The title is started, has no picture here and flips five times.
	CHECK(g_bTitleLoading);
	CHECK(g_pTitleLoadingSprite == NULL);
	CHECK_EQ(5, flips);
}

TEST(GameInit1Surface, AViewThereIsIsKeptAndGivenTheNewLastFrame)
{
	Surfaces scene;
	MTopView view;
	g_pTopView = &view;
	scene.ownsView = false;
	CHECK_EQ(TRUE, InitSurface());
	CHECK(g_pTopView == &view);
	CHECK(view.*Get(ViewSurfaceOf()) == g_pLast);
	CHECK(g_pLast != NULL);
}

TEST(GameInit1Surface, TheFramesThereAreReplacedByNewOnesOfTheGameSize)
{
	Surfaces scene;
	g_pBack = new CSpriteSurface;
	CHECK(g_pBack->Init(8, 8));
	g_pBack->FillSurface(0x1234);
	g_pLast = new CSpriteSurface;
	CHECK(g_pLast->Init(8, 8));
	g_pLast->FillSurface(0x1234);
	CHECK_EQ(TRUE, InitSurface());
	CHECK(g_pBack != NULL && g_pLast != NULL);
	if (g_pBack == NULL || g_pLast == NULL) return;
	CHECK_EQ(kWidth, g_pBack->GetWidth());
	CHECK(Picture(*g_pBack) == std::vector<WORD>(size_t(kWidth * kHeight), 0));
	CHECK_EQ(kWidth, g_pLast->GetWidth());
	CHECK(Picture(*g_pLast) == std::vector<WORD>(size_t(kWidth * kHeight), kLastFill));
}

TEST(GameInit1Surface, AtFullAlphaTheTitlePictureEndsOnTheBlackBackBuffer)
{
	Surfaces scene;
	scene.title.Write(kSixPictures);
	g_MyFull = FALSE;
	CHECK_EQ(TRUE, InitSurface());
	CHECK(g_pTitleLoadingSprite != NULL);
	CHECK(g_pBack != NULL && g_pLast != NULL);
	if (g_pBack == NULL || g_pLast == NULL) return;
	std::vector<WORD> expected(size_t(kWidth * kHeight), 0);
	expected[0] = 0xffff;
	expected[1] = 0x07e0;
	CHECK(expected == Picture(*g_pBack));
	CHECK(Picture(*g_pLast) == std::vector<WORD>(size_t(kWidth * kHeight), kLastFill));
	CHECK_EQ(5, flips);
}

TEST(GameInit1Surface, BelowFullAlphaTheTitlePictureGoesOntoTheNewLastFrame)
{
	Surfaces scene;
	scene.title.Write(kSixPictures);
	g_MyFull = FALSE;
	g_TitleSpriteAlpha = 16;
	// The first draw comes before the new last frame: onto this one.
	g_pLast = new CSpriteSurface;
	CHECK(g_pLast->Init(8, 8));
	CHECK_EQ(TRUE, InitSurface());
	CHECK(g_pTitleLoadingSprite != NULL && g_pBack != NULL && g_pLast != NULL);
	if (g_pTitleLoadingSprite == NULL || g_pBack == NULL || g_pLast == NULL) return;
	Canvas expected(kWidth, kHeight, kLastFill);
	POINT zero = {0, 0};
	for (int draw = 0; draw < 4; ++draw) expected.surface.BltSpriteAlpha(&zero, g_pTitleLoadingSprite, 16);
	CHECK(Picture(expected.surface) == Picture(*g_pLast));
	CHECK(Picture(*g_pBack) == std::vector<WORD>(size_t(kWidth * kHeight), 0));
	CHECK_EQ(0, flips);
}

TEST(GameInit1Surface, ChangingTheOptionsFailsWhenTheViewWillNotInitialise)
{
	Surfaces scene;
	CIndexSprite::SetColorSet();
	const std::vector<WORD> filled = ColorSets();
	SetColorSets(std::vector<WORD>(filled.size(), 0xBEEF));
	MTopView view;
	view.*Get(ViewInitOf()) = true;	// Init answers false at once
	g_pTopView = &view;
	scene.ownsView = false;
	g_Mode = MODE_CHANGE_OPTION;
	CHECK_EQ(FALSE, InitSurface());
	// Everything before the check ran: three title draws and the colour sets.
	CHECK_EQ(3, flips);
	CHECK(filled == ColorSets());
	CHECK(gpC_base != NULL);
	view.*Get(ViewInitOf()) = false;
}

//----------------------------------------------------------------------
// The sound
//----------------------------------------------------------------------
namespace {
// The audio down on SDL's dummy driver, given back down after.
struct Audio
{
	std::string previousDriver = SDL_getenv("SDL_AUDIODRIVER") ? SDL_getenv("SDL_AUDIODRIVER") : "";
	explicit Audio(const char* driver = "dummy")
	{
		g_SDLAudio.Release();
		dxlib_music_release();
		SDL_setenv("SDL_AUDIODRIVER", driver, 1);
	}
	~Audio()
	{
		dxlib_music_release();
		g_SDLAudio.Release();
		SDL_setenv("SDL_AUDIODRIVER", previousDriver.c_str(), 1);
	}
};

// What InitSound reads: the option, the configuration, a sound cache and
// a sound table of 300 sounds; no OGG streamer yet.
struct Sound
{
	Saved saved;
	UserOption option;
	ClientConfig config;
	CSoundPartManager cache;
	SOUND_TABLE table;
	Audio audio;
	explicit Sound(const char* driver = "dummy") : audio(driver)
	{
		table.Init(300);
		g_pSoundTable = &table;
		g_pSoundManager = &cache;
		g_pUserOption = &option;
		g_pClientConfig = &config;
		config.MAX_SOUNDPART = 7;
		option.VolumeSound = 15;
		option.VolumeMusic = 15;
		g_pOGG = NULL;
		g_SDLAudio.SetVolumeLimit(-1234);
	}
	~Sound()
	{
		if (g_pOGG != saved.ogg) delete g_pOGG;
		g_pOGG = saved.ogg;
	}
};
}

TEST(GameInit1Sound, AFailedAudioFailsTheSoundAndTouchesNothing)
{
	Sound sound("no_such_audio_driver");
	CHECK_EQ(0u, unsigned(SDL_WasInit(SDL_INIT_AUDIO)));
	CHECK_EQ(FALSE, InitSound());
	CHECK(!g_SDLAudio.IsInit());
	CHECK_EQ(-1234L, long(g_SDLAudio.GetVolumeLimit()));
	CHECK(g_pOGG == NULL);
	CHECK_EQ(7, sound.config.MAX_SOUNDPART);
	CHECK_EQ(0, int(sound.cache.GetMaxIndex()));
}

#ifdef DARKEDEN_TEST_HAVE_SDL_MIXER
TEST(GameInit1Sound, AnAudioAlreadyUpIsLeftAsItIs)
{
	Sound sound;
	CHECK(g_SDLAudio.Init(NULL));
	g_SDLAudio.SetVolumeLimit(-1234);
	// Nothing is read: a read of any of these would stop the test.
	g_pUserOption = NULL;
	g_pClientConfig = NULL;
	g_pSoundManager = NULL;
	g_pSoundTable = NULL;
	CHECK_EQ(TRUE, InitSound());
	CHECK(g_SDLAudio.IsInit());
	CHECK_EQ(-1234L, long(g_SDLAudio.GetVolumeLimit()));
	CHECK(g_pOGG == NULL);
	CHECK_EQ(0, int(sound.cache.GetMaxIndex()));
}

TEST(GameInit1Sound, ItStartsTheAudioAtTheSoundVolumeAndCachesAHundredParts)
{
	// value*SOUND_DEGREE + SOUND_MIN, held within DSBVOLUME_MIN..DSBVOLUME_MAX.
	struct Case { int volume; long limit; };
	for (const Case& c : {Case{15, 15 * SOUND_DEGREE + SOUND_MIN}, Case{0, SOUND_MIN},
		Case{7, 7 * SOUND_DEGREE + SOUND_MIN}, Case{16, DSBVOLUME_MAX}, Case{-60, DSBVOLUME_MIN}})
	{
		Sound sound;
		sound.option.VolumeSound = c.volume;
		CHECK_EQ(TRUE, InitSound());
		CHECK(g_SDLAudio.IsInit());
		CHECK_EQ(c.limit, long(g_SDLAudio.GetVolumeLimit()));
		CHECK_EQ(100, sound.config.MAX_SOUNDPART);
		CHECK_EQ(300, int(sound.cache.GetMaxIndex()));
		CHECK_EQ(100, int(sound.cache.GetMaxPartIndex()));
		CHECK_EQ(0, int(sound.cache.GetUsed()));
	}
}

TEST(GameInit1Sound, ItMakesTheOggStreamerAtTheMusicVolumeOnce)
{
	// (VolumeMusic-15)*250, within -10000..-1: the loudest is -1, not 0.
	struct Case { int volume; long attenuation; };
	for (const Case& c : {Case{15, -1}, Case{16, -1}, Case{14, -250}, Case{0, -3750},
		Case{-25, -10000}, Case{-26, -10000}})
	{
		Sound sound;
		sound.option.VolumeMusic = c.volume;
		CHECK_EQ(TRUE, InitSound());
		CHECK(g_pOGG != NULL);
		if (g_pOGG != NULL) CHECK_EQ(c.attenuation, g_pOGG->*Get(OggVolumeOf()));
	}
	// A streamer there is kept as it is.
	Sound sound;
	COGGSTREAM stream(NULL, NULL, 44100, 11025, 8800, 1);
	stream.streamVolume(-4321);
	g_pOGG = &stream;
	sound.option.VolumeMusic = 0;
	CHECK_EQ(TRUE, InitSound());
	CHECK(g_pOGG == &stream);
	CHECK_EQ(-4321L, stream.*Get(OggVolumeOf()));
	g_pOGG = NULL;
}
#else
TEST(GameInit1Sound, WithoutTheMixerTheSoundFails)
{
	Sound sound;
	CHECK_EQ(FALSE, InitSound());
	CHECK(g_pOGG == NULL);
	CHECK_EQ(7, sound.config.MAX_SOUNDPART);
	CHECK_EQ(0, int(sound.cache.GetMaxIndex()));
}
#endif

TEST(GameInit1Sound, UnInitReleasesTheCacheAndTheAudio)
{
	for (bool withCache : {true, false})
	{
		Sound sound;
#ifdef DARKEDEN_TEST_HAVE_SDL_MIXER
		CHECK(g_SDLAudio.Init(NULL));
#endif
		sound.cache.Init(5, 2);
		COGGSTREAM stream(NULL, NULL, 44100, 11025, 8800, 1);
		g_pOGG = &stream;
		g_pSoundManager = withCache ? &sound.cache : NULL;
		UnInitSound();
		CHECK(!g_SDLAudio.IsInit());
		CHECK_EQ(withCache ? 0 : 5, int(sound.cache.GetMaxIndex()));
		// The streamer is not UnInitSound's.
		CHECK(g_pOGG == &stream);
		g_pOGG = NULL;
	}
}
