#include "test_framework.h"
#include "CWaitUIUpdate.h"
#include "ClientMode.h"
#include "FrameClock.h"
#include "InputState.h"
#include "MonotonicClock.h"
#include "MEventQueue.h"
#include "MParty.h"
#include "UserOption.h"
#include "KeyAccelerator.h"
#include "AcceleratorManager.h"
#include "UiRuntime.h"
#include "VS_UI.h"
#include "VS_UI_Base.h"
#include "VS_UI_mouse_pointer.h"
#include "pi_core.h"
#include "u_window.h"
#include "Ci.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackendSDL.h"
#include "DXLib/CDirectDraw.h"
#include "CDirectInput.h"
#include "DXLibBackend.h"
#include <SDL.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>

// The loop of the screens that wait for the player in the UI (task 4.255):
// the title, the login and the world, server and character choices.
// - Init hands the mouse and the keyboard to its two receivers and starts
//   SDL's text input.
// - The keyboard receiver gives a key press to the UI: the editing keys
//   (Tab, Backspace, Enter, Escape, the arrows, Home, End, Delete, Insert)
//   first as a WM_KEYDOWN of their virtual key, then every key as the scan
//   code it is; a release, nothing.
// - The mouse receiver turns a left press into a double click when it
//   comes within the double-click time of the last press and within a
//   pixel of it (the cursor's g_x, g_y, not the event's), and hands the
//   wheel over at the event's point; the rest it leaves.
// - Update follows the cursor every frame. 15 ms after the last draw it
//   reads the input and gives the cursor and the buttons to the UI,
//   stopping when the loop is no longer the current one or the
//   application inactive. Once a tick (g_UpdateDelay ms after the last
//   whole one) it checks the running programs (every 5 s), reads the
//   socket, runs the UI, writes the socket and, out of the five waiting
//   modes, stops (entering the option change again in that mode) before
//   the tick counts as done. A frame that read the input then draws and
//   counts itself. A failed socket, or an inactive application after the
//   UI ran, ends the frame there.
// - UpdateDraw shows the UI, fades the title out (alpha 31 down to 0 in
//   two seconds from the first draw, then ends the title loading), asks
//   for the cursor, copies the last frame to the back buffer and flips.
//
// The UI here is the real one with a window of the test's over it: the
// window records what the UI hands it, an accelerator bound to the key
// under test records the scan code, and the UI runtime records its being
// asked to draw the cursor (it is told not to: the pointer's sprite needs
// the game's data). The clocks are the test's: the frame clock moves an
// hour on for each scene, past every static deadline of the loop, and the
// first draw ever is at kOrigin, which the title's fade counts from.
namespace {

std::string calls;
std::string deactivateAt;	// after this call the application is inactive
std::string leaveAt;		// after this call another update runs
bool socketInputOk = true;
bool socketOutputOk = true;
bool traceMouse = false;	// record each mouse input the UI is given ("m;")
int cursorX = 0, cursorY = 0;	// where UpdateMouse puts the cursor

void Call(const std::string& call)
{
	calls += call + ";";
	if (call == deactivateAt) g_bActiveApp = FALSE;
	if (call == leaveAt) g_pUpdate = NULL;
}

const CWaitUIHost host{
	.UpdateMouse = []() { g_x = cursorX; g_y = cursorY; Call("mouse"); },
	.UpdateInput = []() { Call("input"); },
	// The answer is not read: a found program does not stop the tick.
	.CheckInvalidProcess = []() { Call("check"); return true; },
	.UpdateSocketInput = []() { Call("in"); return socketInputOk; },
	.UpdateSocketOutput = []() { Call("out"); return socketOutputOk; },
	.SetMode = [](CLIENT_MODE mode) { Call("mode" + std::to_string(static_cast<int>(mode))); },
	.DrawTitleLoading = []() { Call("title" + std::to_string(g_TitleSpriteAlpha)); },
	.EndTitleLoading = [](bool sendLogin) { Call(sendLogin ? "end1" : "end0"); },
};

std::string Message(UINT message)
{
	switch (message)
	{
		case M_WHEEL_UP: return "wheelup";
		case M_WHEEL_DOWN: return "wheeldown";
		case M_MOVING: return "move";
		case M_LEFTBUTTON_DOWN: return "ldown";
		case M_LEFTBUTTON_UP: return "lup";
		case M_LB_DOUBLECLICK: return "ldouble";
		case M_RIGHTBUTTON_DOWN: return "rdown";
		case M_RIGHTBUTTON_UP: return "rup";
		case M_CENTERBUTTON_DOWN: return "cdown";
		case M_CENTERBUTTON_UP: return "cup";
	}
	return "mouse" + std::to_string(message);
}

std::string At(int x, int y) { return "@" + std::to_string(x) + "," + std::to_string(y); }

// The calls equal, or both printed.
bool Same(const std::string& expected, const std::string& actual)
{
	if (expected != actual)
		std::fprintf(stderr, "    expected %s\n    actual   %s\n", expected.c_str(), actual.c_str());
	return expected == actual;
}
#define CHECK_SAME(expected, actual) CHECK(Same((expected), (actual)))

// A window over the whole screen that takes the keyboard. While the
// cursor is over it (over), the UI hands it the mouse and answers that the
// UI took it.
struct UiWindow : Window
{
	bool over = true;
	UiWindow() : Window(0, 0, 800, 600) {}
	bool IsPixel(int, int) override { return over; }
	bool MouseControl(UINT message, int x, int y) override { Call(Message(message) + At(x, y)); return true; }
	void KeyboardControl(UINT message, UINT key, long extra) override
	{
		Call((message == WM_KEYDOWN ? std::string("keydown") : "key" + std::to_string(message) + ":") + std::to_string(key) + "," + std::to_string(extra));
	}
	void Show() override { Call("show"); }
	void Process() override { Call("process"); }
};

// The accelerator the key under test is bound to.
void Accelerated(void*) { Call("accel"); }
struct Accelerators : AcceleratorManager
{
	Accelerators() { SetFunction(1, Accelerated); }
};

bool EventFlag(std::uint32_t flag)
{
	if (flag == EVENTFLAG_DENY_INPUT_MOUSE && traceMouse) calls += "m;";
	if (flag == EVENTFLAG_NOT_DRAW_MOUSE_POINTER)
	{
		Call("pointer");
		return true;
	}
	return false;
}
const UiRuntime::Host uiRuntime{ .EventFlagActive = EventFlag };

spritectl_surface_t Flipped() { Call("flip"); return SPRITECTL_INVALID_SURFACE; }

MonotonicClock::TimePoint now;
MonotonicClock::TimePoint Now() { return now; }
const MonotonicClock::TimePoint kOrigin = MonotonicClock::TimePoint() + MonotonicClock::Millis(5000000);
MonotonicClock::TimePoint later = kOrigin;
// An hour after the last time this gave.
MonotonicClock::TimePoint Later() { later += std::chrono::hours(1); return later; }

MonotonicClock::TimePoint After(MonotonicClock::TimePoint t, int ms) { return t + MonotonicClock::Duration(ms); }

WORD Pixel(CSpriteSurface& surface, int x, int y)
{
	spritectl_surface_info_t info{};
	if (spritectl_lock_surface(surface.GetBackendSurface(), &info) != 0 || !info.pixels) return 0;
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

struct Probe : CWaitUIUpdate
{
	Probe() : CWaitUIUpdate(&host) {}
	using CWaitUIUpdate::ProcessInput;
	using CWaitUIUpdate::UpdateDraw;
	using CWaitUIUpdate::DXMouseEvent;
	using CWaitUIUpdate::DXKeyboardEvent;
};

// Everything the loop and the UI read, saved, and given back after.
struct Saved
{
	MonotonicClock::TimePoint frameNow = g_FrameNow;
	int updateDelay = g_UpdateDelay;
	BOOL activeApp = g_bActiveApp;
	CWinUpdate* update = g_pUpdate;
	CLIENT_MODE mode = g_Mode;
	int frameCount = g_FrameCount;
	int titleAlpha = g_TitleSpriteAlpha;
	DWORD doubleClick = g_double_click_time;
	int x = g_x, y = g_y;
	CSpriteSurface* back = g_pBack;
	CSpriteSurface* last = g_pLast;
	RECT gameRect = g_GameRect;
	CSDLInput* input = g_pSDLInput;
	WindowManager* windows = gpC_window_manager;
	PI_ProcessRunner* runner = gpC_process_runner;
	Base* base = gpC_base;
	C_VS_UI_MOUSE_POINTER* pointer = gpC_mouse_pointer;
	KeyAccelerator* keys = g_pKeyAccelerator;
	AcceleratorManager* accelerators = g_pAcceleratorManager;
	UserOption* option = g_pUserOption;
	MParty* party = g_pParty;
	bool emptyMove = gbl_global_empty_move;
	CI* ime = gC_ci;
	const UiRuntime::Host* runtime = UiRuntime::SetHost(&uiRuntime);
	MonotonicClock::ScopedTestSource clock{Now};
	~Saved()
	{
		CSDLGraphics::SetBackBufferSource(nullptr);
		UiRuntime::SetHost(runtime);
		gC_ci = ime;
		gbl_global_empty_move = emptyMove;
		g_pParty = party;
		g_pUserOption = option;
		g_pAcceleratorManager = accelerators;
		g_pKeyAccelerator = keys;
		gpC_mouse_pointer = pointer;
		gpC_base = base;
		gpC_process_runner = runner;
		gpC_window_manager = windows;
		g_pSDLInput = input;
		g_GameRect = gameRect;
		g_pLast = last;
		g_pBack = back;
		g_y = y;
		g_x = x;
		g_double_click_time = doubleClick;
		g_TitleSpriteAlpha = titleAlpha;
		g_FrameCount = frameCount;
		g_Mode = mode;
		g_pUpdate = update;
		g_bActiveApp = activeApp;
		g_UpdateDelay = updateDelay;
		g_FrameNow = frameNow;
	}
};

// The loop on the main menu, settled at frame time t: it drew, ticked
// and checked the programs at t. Every 100 ms a tick, the cursor at
// (40, 50), the last frame a 10x8 pattern over the screen, the back
// buffer 0x7777, the double click 300 ms, the title faded out.
struct Scene
{
	Saved saved;
	PI_ProcessRunner runner;
	WindowManager windows;
	Base base;
	C_VS_UI_MOUSE_POINTER pointer;
	CSDLInput input;
	KeyAccelerator keys;
	Accelerators accelerators;
	UserOption option;
	CI_KOREAN ime;	// a window taking the keyboard ends the IME's composition
	CSpriteSurface back, last;
	std::unique_ptr<UiWindow> window;
	Probe wait;
	MonotonicClock::TimePoint t;

	Scene()
	{
		gpC_process_runner = &runner;
		gpC_window_manager = &windows;
		gpC_base = &base;
		gpC_mouse_pointer = &pointer;
		g_pSDLInput = &input;
		keys.Init(MAX_ACCELERATOR);
		g_pKeyAccelerator = &keys;
		g_pAcceleratorManager = &accelerators;
		option.UseEnterChat = FALSE;
		g_pUserOption = &option;
		g_pParty = NULL;
		gbl_global_empty_move = false;
		gC_ci = &ime;
		spritectl_init();
		back.Init(10, 8);
		last.Init(10, 8);
		back.FillSurface(0x7777);
		Pattern(last);
		g_pBack = &back;
		g_pLast = &last;
		base.m_p_DDSurface_back = &back;
		g_GameRect = {0, 0, 10, 8};
		CSDLGraphics::SetBackBufferSource(Flipped);
		window = std::make_unique<UiWindow>();
		window->AttrKeyboardControl(true);
		window->Start();
		windows.AppearWindow(window.get());

		deactivateAt.clear();
		leaveAt.clear();
		socketInputOk = socketOutputOk = true;
		traceMouse = false;
		cursorX = 40;
		cursorY = 50;
		g_double_click_time = 300;
		g_TitleSpriteAlpha = 0;
		now = kOrigin;
		g_UpdateDelay = 0;
		g_Mode = MODE_MAINMENU;
		g_bActiveApp = TRUE;
		g_pUpdate = &wait;
		g_FrameCount = 7;
		t = Later();
		g_FrameNow = t;
		wait.Update();
		CHECK_EQ(8, g_FrameCount);
		g_UpdateDelay = 100;
		calls.clear();
	}
	~Scene() { window.reset(); }

	// One frame, ms after the settled one.
	std::string Frame(int ms)
	{
		calls.clear();
		g_FrameNow = After(t, ms);
		wait.Update();
		return calls;
	}

	// The key pressed (or released), bound to the recording accelerator.
	std::string Key(CSDLInput::E_KEYBOARD_EVENT event, DWORD key)
	{
		keys.SetAcceleratorKey(1, WORD(key));
		calls.clear();
		Probe::DXKeyboardEvent(event, key);
		return calls;
	}

	std::string Mouse(CSDLInput::E_MOUSE_EVENT event, int x, int y)
	{
		calls.clear();
		Probe::DXMouseEvent(event, x, y, 0);
		return calls;
	}

	// The buttons pressed and released this frame, given to the UI.
	std::string Buttons(bool lbDown, bool lbUp, bool rbDown, bool rbUp, bool cbDown, bool cbUp)
	{
		input.m_lb_down = lbDown;
		input.m_lb_up = lbUp;
		input.m_rb_down = rbDown;
		input.m_rb_up = rbUp;
		input.m_cb_down = cbDown;
		input.m_cb_up = cbUp;
		calls.clear();
		wait.ProcessInput();
		return calls;
	}

	// One draw with the clock ms after the first draw ever.
	std::string Draw(int ms)
	{
		now = After(kOrigin, ms);
		calls.clear();
		wait.UpdateDraw();
		return calls;
	}
};

const std::string kMove = "move" + At(40, 50) + ";";
const std::string kDraw = "mouse;" + kMove + "show;pointer;flip;";
const std::string kInput = "mouse;input;" + kMove;
const std::string kTick = "in;process;out;";

} // namespace

TEST(WaitUIUpdate, InitHandsTheMouseAndTheKeysToTheLoopAndStartsTextInput)
{
	SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
	CHECK_EQ(0, SDL_InitSubSystem(SDL_INIT_VIDEO));
	{
		Scene scene;
		CHECK(scene.input.Init(nullptr, nullptr));
		SDL_StopTextInput();
		CHECK(!SDL_IsTextInputActive());
		scene.wait.Init();
		CHECK(SDL_IsTextInputActive());
		SDL_StopTextInput();

		// The cursor moves to the wheel's point first; the loop leaves the move.
		calls.clear();
		scene.input.DispatchMouseAt(CSDLInput::WHEELUP, 30, 20);
		CHECK_SAME("wheelup" + At(30, 20) + ";", calls);

		scene.keys.SetAcceleratorKey(1, DIK_F5);
		calls.clear();
		dxlib_input_virtual_key(DIK_F5, 1);
		scene.input.UpdateInput();
		CHECK_SAME(std::string("accel;"), calls);
		calls.clear();
		dxlib_input_virtual_key(DIK_F5, 0);
		scene.input.UpdateInput();
		CHECK_SAME(std::string(""), calls);
		dxlib_input_virtual_reset();
	}
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

TEST(WaitUIUpdate, TheEditingKeysReachTheUIAsVirtualKeysFirst)
{
	Scene scene;
	const struct { DWORD dik; UINT vk; } keys[] = {
		{DIK_TAB, VK_TAB}, {DIK_BACK, VK_BACK}, {DIK_RETURN, VK_RETURN},
		{DIK_ESCAPE, VK_ESCAPE}, {DIK_LEFT, VK_LEFT}, {DIK_RIGHT, VK_RIGHT},
		{DIK_UP, VK_UP}, {DIK_DOWN, VK_DOWN}, {DIK_HOME, VK_HOME},
		{DIK_END, VK_END}, {DIK_DELETE, VK_DELETE}, {DIK_INSERT, VK_INSERT},
	};
	for (const auto& key : keys)
		CHECK_SAME("keydown" + std::to_string(key.vk) + ",0;accel;", scene.Key(CSDLInput::KEYDOWN, key.dik));
}

TEST(WaitUIUpdate, OtherKeysReachTheUIAsScanCodesOnly)
{
	Scene scene;
	CHECK_SAME(std::string("accel;"), scene.Key(CSDLInput::KEYDOWN, DIK_F5));
	CHECK_SAME(std::string("accel;"), scene.Key(CSDLInput::KEYDOWN, DIK_NEXT));
	CHECK_SAME(std::string("accel;"), scene.Key(CSDLInput::KEYDOWN, DIK_NUMPADENTER));
}

TEST(WaitUIUpdate, AReleasedKeyIsLeft)
{
	Scene scene;
	CHECK_SAME(std::string(""), scene.Key(CSDLInput::KEYUP, DIK_TAB));
	CHECK_SAME(std::string(""), scene.Key(CSDLInput::KEYUP, DIK_F5));
}

TEST(WaitUIUpdate, TheWheelIsGivenAtTheEventsPoint)
{
	Scene scene;
	g_x = 1;
	g_y = 2;
	CHECK_SAME("wheeldown" + At(30, 20) + ";", scene.Mouse(CSDLInput::WHEELDOWN, 30, 20));
	CHECK_SAME("wheelup" + At(31, 21) + ";", scene.Mouse(CSDLInput::WHEELUP, 31, 21));
}

TEST(WaitUIUpdate, TheOtherMouseEventsAreLeft)
{
	Scene scene;
	now = Later();
	for (CSDLInput::E_MOUSE_EVENT event : {CSDLInput::MOVE, CSDLInput::LEFTUP, CSDLInput::RIGHTDOWN,
		CSDLInput::RIGHTUP, CSDLInput::CENTERDOWN, CSDLInput::CENTERUP})
		CHECK_SAME(std::string(""), scene.Mouse(event, 30, 20));
}

TEST(WaitUIUpdate, ASecondPressAtTheCursorWithinTheTimeIsADoubleClick)
{
	Scene scene;
	g_x = 100;
	g_y = 200;
	now = Later();
	// The cursor's point, not the event's.
	CHECK_SAME("ldown" + At(100, 200) + ";", scene.Mouse(CSDLInput::LEFTDOWN, 5, 6));
	now = After(now, 300);
	CHECK_SAME("ldouble" + At(100, 200) + ";", scene.Mouse(CSDLInput::LEFTDOWN, 5, 6));
	// The double click is the pair's end: the next press starts again.
	CHECK_SAME("ldown" + At(100, 200) + ";", scene.Mouse(CSDLInput::LEFTDOWN, 5, 6));
	now = After(now, 301);
	CHECK_SAME("ldown" + At(100, 200) + ";", scene.Mouse(CSDLInput::LEFTDOWN, 5, 6));
	now = After(now, 1);
	CHECK_SAME("ldouble" + At(100, 200) + ";", scene.Mouse(CSDLInput::LEFTDOWN, 5, 6));
	g_double_click_time = 0;
	CHECK_SAME("ldown" + At(100, 200) + ";", scene.Mouse(CSDLInput::LEFTDOWN, 5, 6));
	CHECK_SAME("ldouble" + At(100, 200) + ";", scene.Mouse(CSDLInput::LEFTDOWN, 5, 6));
}

TEST(WaitUIUpdate, ADoubleClickIsWithinAPixelOfTheFirstPress)
{
	Scene scene;
	const struct { int dx, dy; bool doubled; } moves[] = {
		{1, 0, true}, {-1, 0, true}, {0, 1, true}, {0, -1, true}, {1, 1, true}, {-1, -1, true},
		{2, 0, false}, {-2, 0, false}, {0, 2, false}, {0, -2, false},
	};
	for (const auto& move : moves)
	{
		now = Later();
		g_x = 100;
		g_y = 200;
		scene.Mouse(CSDLInput::LEFTDOWN, 0, 0);
		g_x = 100 + move.dx;
		g_y = 200 + move.dy;
		CHECK_SAME((move.doubled ? "ldouble" : "ldown") + At(g_x, g_y) + ";", scene.Mouse(CSDLInput::LEFTDOWN, 0, 0));
	}
	// A press that is not a double click is where the next is measured from.
	now = Later();
	g_x = 100;
	g_y = 200;
	scene.Mouse(CSDLInput::LEFTDOWN, 0, 0);
	g_x = 102;
	scene.Mouse(CSDLInput::LEFTDOWN, 0, 0);
	g_x = 103;
	CHECK_SAME("ldouble" + At(103, 200) + ";", scene.Mouse(CSDLInput::LEFTDOWN, 0, 0));
}

TEST(WaitUIUpdate, BetweenDrawsAndTicksOnlyTheCursorFollows)
{
	Scene scene;
	CHECK_SAME(std::string("mouse;"), scene.Frame(14));
	CHECK_EQ(8, g_FrameCount);
}

TEST(WaitUIUpdate, EveryFifteenMillisecondsTheInputIsReadAndTheFrameDrawn)
{
	Scene scene;
	CHECK_SAME(kInput + kDraw, scene.Frame(15));
	CHECK_EQ(9, g_FrameCount);
	// Fifteen from that draw, not from the tick.
	CHECK_SAME(std::string("mouse;"), scene.Frame(29));
	CHECK_SAME(kInput + kDraw, scene.Frame(30));
	CHECK_EQ(10, g_FrameCount);
}

TEST(WaitUIUpdate, EveryTickTheSocketAndTheUIRunInOrder)
{
	Scene scene;
	CHECK_SAME(std::string("mouse;"), scene.Frame(-1));
	// A tick between draws reads no input and draws nothing.
	g_UpdateDelay = 10;
	CHECK_SAME("mouse;" + kTick, scene.Frame(10));
	CHECK_EQ(8, g_FrameCount);
	// The next tick is g_UpdateDelay from that one.
	CHECK_SAME(std::string("mouse;"), scene.Frame(14));
	CHECK_SAME(kInput + kTick + kDraw, scene.Frame(20));
	CHECK_EQ(9, g_FrameCount);
	g_UpdateDelay = 100;
	CHECK_SAME(kInput + kDraw, scene.Frame(119));
	CHECK_SAME(kInput + kTick + kDraw, scene.Frame(135));
}

TEST(WaitUIUpdate, TheRunningProgramsAreCheckedEveryFiveSeconds)
{
	Scene scene;
	g_UpdateDelay = 1;
	CHECK_SAME(kInput + kTick + kDraw, scene.Frame(4999));
	CHECK_SAME("mouse;check;" + kTick, scene.Frame(5000));
	CHECK_SAME("mouse;" + kTick, scene.Frame(5001));
	CHECK_SAME(kInput + kTick + kDraw, scene.Frame(9999));
	CHECK_SAME("mouse;check;" + kTick, scene.Frame(10000));
}

TEST(WaitUIUpdate, AFailedSocketReadEndsTheFrameAndTheTickIsTriedAgain)
{
	Scene scene;
	socketInputOk = false;
	CHECK_SAME(kInput + "in;", scene.Frame(100));
	CHECK_EQ(8, g_FrameCount);
	socketInputOk = true;
	// Neither the tick nor the draw counted: the next frame does both.
	CHECK_SAME(kInput + kTick + kDraw, scene.Frame(101));
	CHECK_SAME(kInput + kDraw, scene.Frame(200));
	CHECK_SAME("mouse;" + kTick, scene.Frame(201));
}

TEST(WaitUIUpdate, AFailedSocketWriteEndsTheFrameAndTheTickIsTriedAgain)
{
	Scene scene;
	socketOutputOk = false;
	CHECK_SAME(kInput + kTick, scene.Frame(100));
	CHECK_EQ(8, g_FrameCount);
	socketOutputOk = true;
	CHECK_SAME(kInput + kTick + kDraw, scene.Frame(101));
}

TEST(WaitUIUpdate, AnInactiveApplicationAfterTheUIEndsTheFrame)
{
	Scene scene;
	deactivateAt = "process";
	CHECK_SAME(kInput + "in;process;", scene.Frame(100));
	CHECK_EQ(8, g_FrameCount);
}

TEST(WaitUIUpdate, OnlyTheWaitingModesCompleteATick)
{
	for (CLIENT_MODE mode : {MODE_MAINMENU, MODE_NEWUSER, MODE_WAIT_SELECTPC, MODE_WAIT_SELECT_WORLD, MODE_WAIT_SELECT_SERVER})
	{
		Scene scene;
		g_Mode = mode;
		CHECK_SAME(kInput + kTick + kDraw, scene.Frame(100));
		CHECK_SAME(kInput + kDraw, scene.Frame(199));
	}
	for (CLIENT_MODE mode : {MODE_NULL, MODE_OPENING, MODE_CREATECHARACTER, MODE_WAIT_SERVER_LIST, MODE_SELECTCHARACTER, MODE_GAME, MODE_QUIT})
	{
		Scene scene;
		g_Mode = mode;
		CHECK_SAME(kInput + kTick, scene.Frame(100));
		CHECK_EQ(8, g_FrameCount);
		// The tick did not count.
		CHECK_SAME(kInput + kTick, scene.Frame(101));
	}
	Scene scene;
	g_Mode = MODE_CHANGE_OPTION;
	CHECK_SAME(kInput + kTick + "mode" + std::to_string(static_cast<int>(MODE_CHANGE_OPTION)) + ";", scene.Frame(100));
	CHECK_EQ(8, g_FrameCount);
}

TEST(WaitUIUpdate, TheFrameStopsWhenTheLoopIsLeftOrTheApplicationInactive)
{
	for (const char* at : {"input", "move@40,50"})
	{
		{
			Scene scene;
			leaveAt = at;
			const std::string expected = std::string(at) == "input" ? "mouse;input;" : kInput;
			CHECK_SAME(expected, scene.Frame(100));
			CHECK_EQ(8, g_FrameCount);
		}
		{
			Scene scene;
			deactivateAt = at;
			const std::string expected = std::string(at) == "input" ? "mouse;input;" : kInput;
			CHECK_SAME(expected, scene.Frame(100));
			CHECK_EQ(8, g_FrameCount);
		}
	}
	// Left or inactive during the tick: the tick counts, nothing is drawn.
	for (const char* at : {"in", "out"})
	{
		{
			Scene scene;
			leaveAt = at;
			CHECK_SAME(kInput + kTick, scene.Frame(100));
			CHECK_EQ(8, g_FrameCount);
			g_pUpdate = &scene.wait;
			CHECK_SAME(kInput + kDraw, scene.Frame(101));
		}
		if (std::string(at) == "out")
		{
			Scene scene;
			deactivateAt = at;
			CHECK_SAME(kInput + kTick, scene.Frame(100));
			CHECK_EQ(8, g_FrameCount);
		}
	}
	// Inactive from the start, a frame due a draw only reads the input (and,
	// drawing nothing, every later frame is due one); a tick between draws
	// reads the socket and runs the UI, and stops there.
	Scene scene;
	g_bActiveApp = FALSE;
	g_UpdateDelay = 1;
	CHECK_SAME(std::string("mouse;in;process;"), scene.Frame(5));
	CHECK_SAME(std::string("mouse;input;"), scene.Frame(100));
	CHECK_SAME(std::string("mouse;input;"), scene.Frame(101));
}

TEST(WaitUIUpdate, TheButtonsOfTheFrameGoToTheUIAtTheCursor)
{
	Scene scene;
	g_x = 7;
	g_y = 9;
	const std::string move = "move" + At(7, 9) + ";";
	CHECK_SAME(move, scene.Buttons(false, false, false, false, false, false));
	CHECK_SAME(move + "ldown" + At(7, 9) + ";", scene.Buttons(true, false, false, false, false, false));
	CHECK_SAME(move + "lup" + At(7, 9) + ";", scene.Buttons(false, true, false, false, false, false));
	CHECK_SAME(move + "ldown" + At(7, 9) + ";lup" + At(7, 9) + ";", scene.Buttons(true, true, false, false, false, false));
	CHECK_SAME(move + "cdown" + At(7, 9) + ";", scene.Buttons(false, false, false, false, true, false));
	// Taken by the UI, a right press or release is the frame's last.
	CHECK_SAME(move + "ldown" + At(7, 9) + ";lup" + At(7, 9) + ";rdown" + At(7, 9) + ";", scene.Buttons(true, true, true, true, true, true));
	CHECK_SAME(move + "rup" + At(7, 9) + ";", scene.Buttons(false, false, false, true, true, true));
	CHECK_SAME(move + "cdown" + At(7, 9) + ";", scene.Buttons(false, false, false, false, true, true));
	// Recorded defect: the center button's release is read from the right
	// button's (m_rb_up), so a center release alone gives the UI nothing.
	CHECK_SAME(move, scene.Buttons(false, false, false, false, false, true));
}

TEST(WaitUIUpdate, ButtonsTheUILeavesAreFollowedByTheRest)
{
	Scene scene;
	scene.window->over = false;
	traceMouse = true;
	g_x = 7;
	g_y = 9;
	// Moving, the left press and release, the right press and release, the
	// center press, and (the defect above) a center release for the right's.
	CHECK_SAME(std::string("m;m;m;m;m;m;m;"), scene.Buttons(true, true, true, true, true, true));
	CHECK_EQ(7, scene.pointer.GetX());
	CHECK_EQ(9, scene.pointer.GetY());
	CHECK_SAME(std::string("m;m;m;"), scene.Buttons(false, false, false, true, false, false));
	CHECK_SAME(std::string("m;m;"), scene.Buttons(false, false, true, false, false, false));
	CHECK(scene.pointer.RightMousePushed());
	CHECK_SAME(std::string("m;"), scene.Buttons(false, false, false, false, false, true));
}

TEST(WaitUIUpdate, TheDrawShowsTheUIAndTheCursorAndCopiesTheLastFrame)
{
	Scene scene;
	cursorX = 3;
	cursorY = 4;
	CHECK_SAME("mouse;move" + At(3, 4) + ";show;pointer;flip;", scene.Draw(0));
	// The game's rectangle from the corner: its left and top are not read.
	g_GameRect = {3, 2, 6, 4};
	scene.back.FillSurface(0x7777);
	scene.Draw(0);
	CHECK_EQ(Pixel(scene.last, 0, 0), Pixel(scene.back, 0, 0));
	CHECK_EQ(Pixel(scene.last, 5, 3), Pixel(scene.back, 5, 3));
	CHECK_EQ(0x7777, Pixel(scene.back, 6, 3));
	CHECK_EQ(0x7777, Pixel(scene.back, 5, 4));
	CHECK(Pixel(scene.last, 5, 3) != 0x7777);
}

TEST(WaitUIUpdate, TheTitleFadesOutInTwoSecondsFromTheFirstDraw)
{
	Scene scene;
	const struct { int ms; int alpha; bool ends; } steps[] = {
		{0, 31, false}, {1000, 15, false}, {1937, 1, false}, {1938, 0, true}, {2000, 0, true}, {60000, 0, true},
	};
	for (const auto& step : steps)
	{
		g_TitleSpriteAlpha = 32;
		const std::string title = "title" + std::to_string(step.alpha) + ";" + (step.ends ? "end1;" : "");
		CHECK_SAME("mouse;" + kMove + "show;" + title + "pointer;flip;", scene.Draw(step.ms));
		CHECK_EQ(step.alpha, g_TitleSpriteAlpha);
	}
	// The alpha comes from the time alone, not from where the fade was.
	g_TitleSpriteAlpha = 1;
	scene.Draw(0);
	CHECK_EQ(31, g_TitleSpriteAlpha);
	// Faded out, or below, there is no title.
	for (int alpha : {0, -5})
	{
		g_TitleSpriteAlpha = alpha;
		CHECK_SAME(kDraw, scene.Draw(500));
		CHECK_EQ(alpha, g_TitleSpriteAlpha);
	}
}
