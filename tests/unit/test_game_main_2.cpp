#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "packet_stream_access.h"
#include "GameMain.h"
#include "ClientMode.h"
#include "ClientPlayer.h"
#include "GameSocket.h"
#include "Exception.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketOutputStream.h"
#include "SocketEncryptOutputStream.h"
#include "WireHost.h"
#include "RequestServerPlayer.h"
#include "RequestServerPlayerManager.h"
#include "ClientCommunicationManager.h"
#include "CWaitUIUpdate.h"
#include "CWaitPacketUpdate.h"
#include "CGameUpdate.h"
#include "FrameClock.h"
#include "InputState.h"
#include "MorphState.h"
#include "MonotonicClock.h"
#include "MEventQueue.h"
#include "MEventManager.h"
#include "MParty.h"
#include "MWeather.h"
#include "MWarManager.h"
#include "MTradeManager.h"
#include "MStorage.h"
#include "MZoneSoundManager.h"
#include "MPlayer.h"
#include "MCreatureTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "EffectSpriteTypeDef.h"
#include "MCreatureSpriteTable.h"
#include "MActionInfoTable.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "SoundDef.h"
#include "SkillDef.h"
#include "MTopView.h"
#include "MJusticeAttackManager.h"
#include "TempInformation.h"
#include "UIDialog.h"
#include "DialogWindowHost.h"
#include "HelpEvent.h"
#include "MChatManager.h"
#include "GameMessages.h"
#include "../basic/CMessageArray.h"
#include "MGameStringTable.h"
#include "MMusic.h"
#include "MMusicTable.h"
#include "DXLib/CDirectSound.h"
#include "AmbientSoundState.h"
#include "PCConfigTable.h"
#include "CServerInformation.h"
#include "Properties.h"
#include "UserOption.h"
#include "KeyAccelerator.h"
#include "AcceleratorDef.h"
#include "BasicException.h"
#include "UiRuntime.h"
#include "VS_UI.h"
#include "VS_UI_Base.h"
#include "VS_UI_mouse_pointer.h"
#include "VS_UI_GlobalResource.h"
#include "VS_UI_GameCommon.h"
#include "VS_UI_Message.h"
#include "VS_UI_item.h"
#include "SkinManager.h"
#include "pi_core.h"
#include "u_window.h"
#include "Ci.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackend.h"
#include "SpriteLibBackendSDL.h"
#include "DXLib/CDirectDraw.h"
#include "CDirectInput.h"
#include "DXLibBackend.h"
#include "DXInputEvents.h"
#include "Cpackets/CGReady.h"
#include "Cpackets/CGLogout.h"
#include "Cpackets/CLLogout.h"
#include "Cpackets/CGSay.h"
#include <SDL.h>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// GameMain.cpp's own, which no header declares.
bool ContainsIgnoreCase(const char* haystack, const char* needle);
extern bool g_bCheckHack;
extern std::string g_strBadProcessList[20];
// The other files' that no header declares.
extern BOOL g_bSetHotKey;					// PacketFunction.cpp
extern bool g_bPartyRunning;				// UIDialog.cpp
extern bool g_bRunning;						// DXLibBackendSDL.cpp
extern bool g_bPreviousMove;				// CGameUpdate.cpp
extern C_VS_UI_DIALOG* g_msg_free_text;		// VS_UI_Message.cpp

// The game's main functions (task 4.262 moved Client/GameMain.cpp into
// gameclient), group 2: the mode machine, the socket, the input and the
// disconnect screen.
// - SetMode(mode) sets g_Mode, reads the input and clears it, lets go of
//   the three mouse buttons, does the mode's work and clears the input
//   again at the end. The waits on the UI (the new user, the world and
//   server choices, the character choice, a wrong login, which is the main
//   menu again) run CWaitUIUpdate every 20 ms; the waits on the server run
//   CWaitPacketUpdate every 50 ms, giving up MAX_WAIT_PACKET ms after the
//   stamp; the game runs CGameUpdate every 62 ms; the main menu starts the
//   title screen; the option change rebuilds the display and goes to the
//   main menu (or quits when the view cannot be rebuilt); quitting says
//   goodbye to the server and stops the loop.
// - UpdateSocketInput reads and runs the connection's packets, then, at
//   most every 330 ms, updates the peer managers; UpdateSocketOutput writes
//   it. A Throwable from the connection goes to the main menu, disconnected
//   (a protocol error is reported to the server first) and answers false.
// - UpdateDisconnected releases the connection, the music, the loading
//   thread, the game's objects and the dialogs, and shows the
//   "disconnected" box until a key or the box's close button.
// - UpdateInput reads the input; UpdateMouse puts the cursor, mapped into
//   the game frame and held inside it, in g_x, g_y.
// - CheckInvalidProcess (Windows: the cheat tools' window titles) and
//   ContainsIgnoreCase, the title match.
//
// The executable's services are a recording GameMainHost; the three
// updates are the real ones over recording hosts; the UI is the real one
// with a window of the test's in it, the dialogs record through
// DialogWindow's host, and the connection is a real ClientPlayer whose
// packets are compared byte for byte. The title screen the main menu
// starts is the real one too: its skins are the test's (every point and
// rect at the origin), the data files it cannot find are reported to the
// test's exception reporter instead of ending the process, and
// C_VS_UI::Release frees it with the UI's objects (on the heap for that)
// after each test. The disconnect box's three sprites come from a UI pack
// the test writes; its wait loop is driven by keys held through the input
// backend and by a frame script the test's window runs as it is drawn, and
// the eighth flip presses every closing key so that a loop that runs on
// ends, as a failure.
//
// Not run:
// - SetMode(MODE_WAIT_UPDATEINFO): UI_StartGame builds the whole game UI
//   (C_VS_UI_GAME), whose windows read the built skill tree
//   (g_pSkillManager's step lists, from the game's data) and much more; it
//   needs a seam first. Its first half (the player removed, the self-defense
//   list released, ZoneLoading::InitSkillTree) cannot run without it.
// - CheckInvalidProcess with the check on, on Windows: it reads the live
//   desktop's window titles (another process's only).
// - UpdateMouse's real cursor on Windows (GetCursorPos); off Windows the
//   cursor is SDL's, (0, 0) on the dummy driver, so a cursor left of or
//   above the frame (the `< 0` clamps) is not reachable either.
// - The wave music's stream (g_pOGG) in SetMode and UpdateDisconnected:
//   only with the audio up, which the tests leave down.
// - The disconnect box's close button by the mouse on Windows (the real
//   cursor again).
// - SetAcquire(true) and the graphics calls (RestoreGammaRamp, ReleaseAll,
//   RestoreAllSurfaces): no-ops on SDL, nothing to see.
namespace {

std::string calls;
std::string N(long long value) { return std::to_string(value); }
void Call(const std::string& call) { calls += call + ";"; }

// The calls equal, or both printed.
bool Same(const std::string& expected, const std::string& actual)
{
	if (expected != actual)
		std::fprintf(stderr, "    expected %s\n    actual   %s\n", expected.c_str(), actual.c_str());
	return expected == actual;
}
#define CHECK_SAME(expected, actual) CHECK(Same((expected), (actual)))

// The input left as a frame leaves it: the buttons pressed and released
// and the wheel turned. SetMode's last clear wipes it; a call recorded
// after the mode's first clear dirties it so that the last one shows.
void Dirty()
{
	if (g_pSDLInput == NULL) return;
	g_pSDLInput->m_lb_down = TRUE;
	g_pSDLInput->m_rb_up = TRUE;
	g_pSDLInput->m_mouse_z = 3;
}
bool Clean(const CSDLInput& input) { return !input.m_lb_down && !input.m_rb_up && input.m_mouse_z == 0; }

//----------------------------------------------------------------------
// The hosts
//----------------------------------------------------------------------
const GameMainHost mainHost{
	.InitDraw = []() -> BOOL { Call("draw"); return TRUE; },
	.InitSurface = []() -> BOOL { Call("surface"); return TRUE; },
	.InitSound = []() -> BOOL { Call("sound"); return TRUE; },
	.ReleaseSocket = []() { Call("release socket"); },
};

// The three updates over hosts that record any call; their Init is
// recorded too (and dirties the input), then runs.
const CWaitUIHost uiHost{
	.UpdateMouse = []() { Call("ui.mouse"); },
	.UpdateInput = []() { Call("ui.input"); },
	.CheckInvalidProcess = []() { Call("ui.check"); return true; },
	.UpdateSocketInput = []() { Call("ui.in"); return true; },
	.UpdateSocketOutput = []() { Call("ui.out"); return true; },
	.SetMode = [](CLIENT_MODE mode) { Call("ui.mode" + N(mode)); },
	.DrawTitleLoading = []() { Call("ui.title"); },
	.EndTitleLoading = [](bool) { Call("ui.end"); },
};
std::string initSnapshot;	// what WaitUI::Init saw of the input, "" when clean
struct WaitUI : CWaitUIUpdate
{
	WaitUI() : CWaitUIUpdate(&uiHost) {}
	void Init() override
	{
		if (g_pSDLInput != NULL)
			initSnapshot = std::string(g_pSDLInput->KeyDown(DIK_F5) ? "key" : "") + (Clean(*g_pSDLInput) ? "" : "pulse");
		Call("ui init");
		Dirty();
		CWaitUIUpdate::Init();
	}
};

const MonotonicClock::TimePoint kStamp = MonotonicClock::TimePoint() + MonotonicClock::Millis(7000000);
const CWaitPacketHost packetHost{
	.ClearInputReceivers = []() { Call("receivers off"); Dirty(); },
	.StampFrameClock = []() { Call("stamp"); g_FrameNow = kStamp; },
	.SetMode = [](CLIENT_MODE mode) { Call("packet.mode" + N(mode)); },
	.UpdateDisconnected = []() { Call("packet.disconnected"); },
	.UpdateSocketInput = []() { Call("packet.in"); return true; },
	.UpdateSocketOutput = []() { Call("packet.out"); return true; },
};
struct WaitPacket : CWaitPacketUpdate
{
	WaitPacket() : CWaitPacketUpdate(&packetHost) {}
	using CWaitPacketUpdate::m_DelayLimit;
};

const CGameUpdateHost gameHost{
	.UpdateMouse = []() { Call("game.mouse"); },
	.UpdateInput = []() { Call("game.input"); },
	.UpdateSocketInput = []() { Call("game.in"); return true; },
	.UpdateSocketOutput = []() { Call("game.out"); return true; },
	.SetMode = [](CLIENT_MODE mode) { Call("game.mode" + N(mode)); },
	.GetCursorPoint = [](POINT& pt) { pt.x = pt.y = 0; Call("game.cursor"); },
};
struct Game : CGameUpdate
{
	Game() : CGameUpdate(&gameHost) {}
	void Init() override { Call("game init"); Dirty(); CGameUpdate::Init(); }
};

// Some other loop, to see a mode leave g_pUpdate alone.
struct Other : CWinUpdate {};

// The dialogs: a message box is "message", the NPC talk box "talk".
DialogWindow::Window* const kTalk = reinterpret_cast<DialogWindow::Window*>(static_cast<std::uintptr_t>(0x7A1C));
DialogWindow::Window* const kMessage = reinterpret_cast<DialogWindow::Window*>(static_cast<std::uintptr_t>(0x3E55));
std::string pending;
const DialogWindow::Host windowHost = []() {
	DialogWindow::Host host{};
	host.NewMessage = [](int x, int y, int extraHeight, std::uint16_t buttons) {
		pending = "popup|" + N(x) + "|" + N(y) + "|" + N(extraHeight) + "|" + N(buttons);
		return kMessage;
	};
	host.SetMessage = [](DialogWindow::Window*, const char* const* pLines, int count, bool noFit) {
		for (int i = 0; i < count; ++i) pending += std::string("|") + pLines[i];
		if (!noFit) pending += "|fit";
	};
	host.Start = [](DialogWindow::Window*) { Call(pending); };
	host.Finish = [](DialogWindow::Window* pWindow) { Call(pWindow == kTalk ? "hide talk" : "hide message"); };
	host.Delete = [](DialogWindow::Window* pWindow) { Call(pWindow == kTalk ? "close talk" : "close message"); };
	return host;
}();
struct Dialogs : UIDialog
{
	void OpenTalk() { m_pPCTalkDlg = kTalk; SetLockInputPCTalk(); }
	void OpenMessage() { m_pMessageDlg = kMessage; SetLockInputMessage(); }
	bool TalkOpen() const { return m_pPCTalkDlg != NULL; }
	bool MessageOpen() const { return m_pMessageDlg != NULL; }
};

// The help mail, with whether the game was active when it came.
const MHelpHost helpHost{ .AddHelpMail = [](HELP_EVENT he) { Call("help" + N(he) + "@active" + N(g_bActiveGame)); } };

// The event queue's gamma, which each removal resets.
const MEventHost eventHost = []() {
	MEventHost h{};
	h.SetAddGammaRamp = [](WORD, WORD, WORD) { Call("gamma"); };
	return h;
}();

// The UI draws no cursor (the pointer needs the game's sprites); asked to,
// it is recorded.
bool EventFlag(std::uint32_t flag)
{
	if (flag == EVENTFLAG_NOT_DRAW_MOUSE_POINTER)
	{
		Call("pointer");
		return true;
	}
	return false;
}
const UiRuntime::Host uiRuntime{ .EventFlagActive = EventFlag };

// Each flip is counted. The disconnect box waits for a key: a loop that
// runs on is ended at the eighth flip, as a failure (the calls show
// "watchdog"), by every key that closes the box, read here too in case
// the loop no longer reads the input itself.
int flips = 0;
spritectl_surface_t Flipped()
{
	Call("flip");
	if (++flips == 8 && g_pSDLInput != NULL)
	{
		Call("watchdog");
		for (int key : {DIK_ESCAPE, DIK_NUMPADENTER, DIK_RETURN, DIK_LMENU, DIK_RMENU, DIK_TAB, DIK_SPACE})
			dxlib_input_virtual_key(key, 1);
		g_pSDLInput->UpdateInput();
	}
	return SPRITECTL_INVALID_SURFACE;
}

// The UI's start-up errors (the title's missing data files), counted.
int uiErrors = 0;
void Reporter(EXCEPTION_CODE, const char*, const ExceptionSite&) { ++uiErrors; }

//----------------------------------------------------------------------
// The connection and the peers
//----------------------------------------------------------------------
enum class Fault { None, Protocol, ProtocolDatagram, NamedInText, Other, Std };
void Raise(Fault fault)
{
	switch (fault)
	{
		case Fault::None: return;
		case Fault::Protocol: throw InvalidProtocolException("bad packet id");
		case Fault::ProtocolDatagram: throw InvalidProtocolException("bad packet id (datagram)");
		case Fault::NamedInText: throw IOException("an InvalidProtocolException upstream");
		case Fault::Other: throw IOException("connection reset");
		case Fault::Std: throw std::runtime_error("out of memory");
	}
}

// The game connection: what it is asked is recorded, what is sent kept,
// serialised, in its output stream.
struct Connection : ClientPlayer
{
	Fault inFault = Fault::None, commandFault = Fault::None, outFault = Fault::None;
	Connection() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~Connection() { setPlayerStatus(CPS_END_SESSION); }
	void sendPacket(Packet* pPacket) override
	{
		Call("send" + N(pPacket->getPacketID()));
		Player::sendPacket(pPacket);
	}
	void processInput() override { Call("in"); Raise(inFault); }
	void processCommand() override { Call("command"); Raise(commandFault); }
	void processOutput() override { Call("out"); Dirty(); Raise(outFault); }
	std::vector<unsigned char> Sent() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};
Connection* current = nullptr;

// The packets as the connection's stream (an encrypting one, its code 0)
// sends them.
std::vector<unsigned char> Encoded(const std::vector<const Packet*>& packets)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketEncryptOutputStream encoded(&out);
	for (const Packet* pPacket : packets) encoded.write(pPacket);
	return SocketOutputStreamTestAccess::Bytes(encoded);
}
std::string Send(const Packet& packet) { return "send" + N(packet.getPacketID()) + ";"; }

// The wire layer's services: a bug report goes to the test's connection;
// the datagram manager's update asks how many datagrams to read
// ("datagrams", none), a peer's teardown whether it has a file request
// ("peer").
const WireHost wireHost = {
	.MaxProcessPacket = []() { Call("datagrams"); return 0; },
	.MaxRequestService = NULL,
	.ClientCommunicationUDPPort = []() -> uint { return 0; },
	.BugReportTarget = []() -> Player* { return current; },
	.EncryptZoneID = NULL,
	.EncryptServerID = NULL,
	.EncryptUsesEnglishSeed = NULL,
	.CurrentTime = NULL,
	.SendOtherRequest = NULL,
	.HasOtherRequest = [](const std::string&) { Call("peer"); return false; },
	.RemoveOtherRequest = NULL,
};

// The peer manager, with a peer on a socket that never connected: its
// update drops the peer, asking on the way whether it had a file request.
struct Peers : RequestServerPlayerManager
{
	void Add() { CHECK(AddRequestServerPlayer(new RequestServerPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))))); }
	~Peers()
	{
		for (RequestServerPlayer* pPlayer : m_listRequestServerPlayer)
			pPlayer->setPlayerStatus(CPS_END_SESSION);
	}
};

//----------------------------------------------------------------------
// The screen
//----------------------------------------------------------------------
WORD Pixel(CSpriteSurface& surface, int x, int y)
{
	spritectl_surface_info_t info{};
	if (spritectl_lock_surface(surface.GetBackendSurface(), &info) != 0 || !info.pixels) return 0;
	const WORD value = reinterpret_cast<WORD*>(static_cast<BYTE*>(info.pixels) + y * info.pitch)[x];
	spritectl_unlock_surface(surface.GetBackendSurface());
	return value;
}

// Every pixel a value of its own below 0x7000, so never the back
// buffer's 0x7777.
WORD Patterned(int x, int y) { return WORD(1 + (x * 31 + y * 17) % 0x6FFF); }
void Pattern(CSpriteSurface& surface)
{
	spritectl_surface_info_t info{};
	if (spritectl_lock_surface(surface.GetBackendSurface(), &info) != 0 || !info.pixels) return;
	for (int y = 0; y < info.height; ++y)
		for (int x = 0; x < info.width; ++x)
			reinterpret_cast<WORD*>(static_cast<BYTE*>(info.pixels) + y * info.pitch)[x] = Patterned(x, y);
	spritectl_unlock_surface(surface.GetBackendSurface());
}
const WORD kBack = 0x7777;

// The UI pack the disconnect box reads: three sprites (the box, its close
// button focused and pushed), one colour each, and their index.
const char* const kSpriteFile = "game_main_2_ui.spk";
const char* const kIndexFile = "game_main_2_ui.spki";
const WORD kBoxColour = 0x0841 * 3, kFocusedColour = 0x0841 * 5, kPushedColour = 0x0841 * 7;
const int kBoxWidth = 4, kBoxHeight = 2, kButtonWidth = 3;
void PutWord(std::ofstream& out, WORD word) { out.put(char(word & 0xFF)); out.put(char(word >> 8)); }
void PutLong(std::ofstream& out, std::uint32_t value) { PutWord(out, WORD(value & 0xFFFF)); PutWord(out, WORD(value >> 16)); }
// A sprite of one colour, each row one run.
std::vector<WORD> Sprite(int width, int height, WORD colour)
{
	std::vector<WORD> words{WORD(width), WORD(height)};
	for (int y = 0; y < height; ++y)
	{
		words.push_back(WORD(3 + width));
		words.push_back(1);
		words.push_back(0);
		words.push_back(WORD(width));
		for (int x = 0; x < width; ++x) words.push_back(colour);
	}
	return words;
}
void WriteUiPack()
{
	const std::vector<std::vector<WORD>> sprites{
		Sprite(kBoxWidth, kBoxHeight, kBoxColour), Sprite(kButtonWidth, 1, kFocusedColour), Sprite(kButtonWidth, 1, kPushedColour)};
	std::vector<std::uint32_t> offsets;
	{
		std::ofstream pack(kSpriteFile, std::ios::binary | std::ios::trunc);
		PutWord(pack, WORD(sprites.size()));
		std::uint32_t at = 2;
		for (const std::vector<WORD>& sprite : sprites)
		{
			offsets.push_back(at);
			for (WORD word : sprite) PutWord(pack, word);
			at += std::uint32_t(sprite.size() * 2);
		}
	}
	std::ofstream index(kIndexFile, std::ios::binary | std::ios::trunc);
	PutWord(index, WORD(offsets.size()));
	for (std::uint32_t offset : offsets) PutLong(index, offset);
}

// A window over the whole screen that takes the mouse: it records its
// drawing (and runs the frame's script), its process and the mouse it is
// given.
std::function<void(int)> onShow;	// the frame's script, by the frame's number
int frames = 0;
struct UiWindow : Window
{
	UiWindow() : Window(0, 0, 800, 600) {}
	bool IsPixel(int, int) override { return true; }
	bool MouseControl(UINT message, int x, int y) override
	{
		Call((message == M_MOVING ? std::string("move") : "mouse" + N(message)) + "@" + N(x) + "," + N(y));
		return true;
	}
	void KeyboardControl(UINT, UINT, long) override {}
	void Show() override
	{
		Call("show");
		++frames;
		if (onShow) onShow(frames);
	}
	void Process() override { Call("process"); }
};

// A key held down, through the input's backend.
void Hold(DWORD key) { dxlib_input_virtual_key(int(key), 1); }
// The left button pressed or released, as SDL delivers it.
void LeftButton(bool down)
{
	SDL_Event event{};
	event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	event.button.button = SDL_BUTTON_LEFT;
	DXInput::ProcessEvent(event);
}
#ifndef PLATFORM_WINDOWS
// The cursor, at window (0, 0), mapped into the game frame at (x, y): the
// frame as if presented from (-x, -y) at its own size. (Windows reads the
// real cursor.)
void CursorAt(int x, int y) { spritectl_set_present_geometry(0, 0, 0, 0, -x, -y, 800, 600, 800, 600); }
#endif
void NothingPresented() { spritectl_set_present_geometry(0, 0, 0, 0, 0, 0, 0, 0, 0, 0); }

// VS_UI's message boxes, which C_VS_UI::Release frees.
C_VS_UI_DIALOG** const kMessageBoxes[] = {
	&g_msg_not_available_menu, &g_msg_wrong_id_password, &g_msg_invalid_id_password, &g_msg_wrong_ssn, &g_msg_invalid_ssn,
	&g_msg_empty_name, &g_msg_left_bonus_point, &g_msg_server_connect_failed, &g_msg_not_select_char,
	&g_msg_char_delete_completed, &g_msg_all_slot_not_char, &g_msg_all_slot_created, &g_msg_character_create_failed,
	&g_msg_new_user_not_completed, &g_msg_id_aleady_exist, &g_msg_id_no_aleady_exist, &g_msg_password_mismatch, &g_msg_free_text};
const int kMessageBoxCount = int(sizeof(kMessageBoxes) / sizeof(kMessageBoxes[0]));

const int kGameWidth = 640, kGameHeight = 300;
const char* const kOptionFile = "game_main_2_user_option.set";
const char* const kWait = "Please wait.";
const char* const kHelpKey = "F1 opens the help.";
const int kTheme = 2;
const DWORD kMaxWait = 4321;
const TYPE_OBJECTID kPlayerID = 77;

//----------------------------------------------------------------------
// Everything the functions read and write, saved and given back
//----------------------------------------------------------------------
struct Saved
{
	CLIENT_MODE mode = g_Mode, modeNext = g_ModeNext;
	int needUpdate = g_bNeedUpdate;
	bool running = g_bRunning;
	int activeGame = g_bActiveGame;
	BOOL activeApp = g_bActiveApp;
	ClientPlayer* socket = g_pSocket;
	CWinUpdate* update = g_pUpdate;
	int updateDelay = g_UpdateDelay;
	MonotonicClock::TimePoint frameNow = g_FrameNow;
	DWORD currentFrame = g_CurrentFrame;
	BOOL setHotKey = g_bSetHotKey;
	BOOL uiInput = g_bUIInput;
	int morph = g_MorphCreatureType;
	POINT selectSector = g_SelectSector;
	BOOL left = g_bLButtonDown, right = g_bRButtonDown, centre = g_bCButtonDown;
	bool previousMove = g_bPreviousMove;
	int x = g_x, y = g_y;
	RECT gameRect = g_GameRect;
	CSpriteSurface* back = g_pBack;
	CSpriteSurface* last = g_pLast;
	CSDLInput* input = g_pSDLInput;
	CWaitUIUpdate* waitUI = g_pCWaitUIUpdate;
	CWaitPacketUpdate* waitPacket = g_pCWaitPacketUpdate;
	CGameUpdate* game = g_pCGameUpdate;
	UserOption* option = g_pUserOption;
	KeyAccelerator* keys = g_pKeyAccelerator;
	UIDialog* dialog = g_pUIDialog;
	DWORD lock = UIDialog::IsLockInput();
	bool partyRunning = g_bPartyRunning;
	MChatManager* chat = g_pChatManager;
	CMessageArray* systemLog = g_pSystemMessage;
	CMessageArray* playerLog = g_pPlayerMessage;
	CMessageArray* gameLog = g_pGameMessage;
	CMessageArray* noticeLog = g_pNoticeMessage;
	MUSIC_TABLE* musicTable = g_pMusicTable;
	MMusic music = g_Music;
	bool mute = g_SDLAudio.IsMute();
	TempInformation* temp = g_pTempInformation;
	MJusticeAttackManager* justice = g_pJusticeAttackManager;
	MPlayer* player = g_pPlayer;
	MZone* zone = g_pZone;
	MTopView* view = g_pTopView;
	CREATURE_TABLE* creatures = g_pCreatureTable;
	Properties* files = g_pFileDef;
	MWeather* weather = g_pWeather;
	MParty* party = g_pParty;
	MEventQueue* events = g_pEventManager;
	MWarManager* wars = g_pWarManager;
	MTradeManager* trade = g_pTradeManager;
	MStorage* storage = g_pStorage;
	MStorage* storage2 = g_pStorage2;
	MZoneSoundTable* zoneSounds = g_pZoneSoundTable;
	MZoneSoundManager* zoneSoundManager = g_pZoneSoundManager;
	WorldPlayerConfigTable* playerConfigs = g_pWorldPlayerConfigTable;
	CServerInformation* servers = g_pServerInformation;
	RequestServerPlayerManager* peers = g_pRequestServerPlayerManager;
	ClientCommunicationManager* datagrams = g_pClientCommunicationManager;
	MonotonicClock::TimePoint nextSound = g_ZoneAmbientSounds.GetNextSoundTime();
	int previousSound = g_previousSoundID;
	bool checkHack = g_bCheckHack;
	// The UI's.
	PI_ProcessRunner* runner = gpC_process_runner;
	WindowManager* windows = gpC_window_manager;
	Base* base = gpC_base;
	C_VS_UI_MOUSE_POINTER* pointer = gpC_mouse_pointer;
	C_GLOBAL_RESOURCE* resource = gpC_global_resource;
	SkinManager* skins = g_pSkinManager;
	C_VS_UI_WINDOW_MANAGER* uiWindows = gpC_vs_ui_window_manager;
	C_VS_UI_ITEM* item = gpC_item;
	CI* ime = gC_ci;
	bool emptyMove = gbl_global_empty_move;
	bool woodSkin = gbl_wood_skin;
	Race raceInterface = g_eRaceInterface;
	C_VS_UI_DIALOG* messageBoxes[kMessageBoxCount];
	// The hosts.
	const GameMainHost* mainHost = SetGameMainHost(&::mainHost);
	const DialogWindow::Host* windowHost = DialogWindow::SetHost(&::windowHost);
	const UiRuntime::Host* runtime = UiRuntime::SetHost(&uiRuntime);
	BasicExceptionReporter reporter = g_SetBasicExceptionReporter(Reporter);
	Saved()
	{
		for (int i = 0; i < kMessageBoxCount; ++i)
		{
			messageBoxes[i] = *kMessageBoxes[i];
			*kMessageBoxes[i] = NULL;
		}
		Wire::SetHost(&wireHost);
		SetHelpHost(&helpHost);
		ResetGameMainState();
		NothingPresented();
	}
	~Saved()
	{
		NothingPresented();
		ResetGameMainState();
		SetHelpHost(NULL);
		Wire::SetHost(NULL);
		CSDLGraphics::SetBackBufferSource(nullptr);
		g_SetBasicExceptionReporter(reporter);
		UiRuntime::SetHost(runtime);
		DialogWindow::SetHost(windowHost);
		SetGameMainHost(mainHost);
		for (int i = 0; i < kMessageBoxCount; ++i) *kMessageBoxes[i] = messageBoxes[i];
		g_eRaceInterface = raceInterface;
		gbl_wood_skin = woodSkin;
		gbl_global_empty_move = emptyMove;
		gC_ci = ime;
		gpC_item = item;
		gpC_vs_ui_window_manager = uiWindows;
		g_pSkinManager = skins;
		gpC_global_resource = resource;
		gpC_mouse_pointer = pointer;
		gpC_base = base;
		gpC_window_manager = windows;
		gpC_process_runner = runner;
		g_bCheckHack = checkHack;
		g_previousSoundID = previousSound;
		g_ZoneAmbientSounds.SetNextSoundTime(nextSound);
		g_pClientCommunicationManager = datagrams;
		g_pRequestServerPlayerManager = peers;
		g_pServerInformation = servers;
		g_pWorldPlayerConfigTable = playerConfigs;
		g_pZoneSoundManager = zoneSoundManager;
		g_pZoneSoundTable = zoneSounds;
		g_pStorage2 = storage2;
		g_pStorage = storage;
		g_pTradeManager = trade;
		g_pWarManager = wars;
		g_pEventManager = events;
		g_pParty = party;
		g_pWeather = weather;
		g_pFileDef = files;
		g_pCreatureTable = creatures;
		g_pTopView = view;
		g_pZone = zone;
		g_pPlayer = player;
		g_pJusticeAttackManager = justice;
		g_pTempInformation = temp;
		if (mute) g_SDLAudio.SetMute(); else g_SDLAudio.UnSetMute();
		g_Music = music;
		g_pMusicTable = musicTable;
		g_pNoticeMessage = noticeLog;
		g_pGameMessage = gameLog;
		g_pPlayerMessage = playerLog;
		g_pSystemMessage = systemLog;
		g_pChatManager = chat;
		g_bPartyRunning = partyRunning;
		UIDialog::UnSetLockInputPCTalk();
		UIDialog::UnSetLockInputMessage();
		if (lock & FLAG_UIDIALOG_LOCK_PCTALK) UIDialog::SetLockInputPCTalk();
		if (lock & FLAG_UIDIALOG_LOCK_MESSAGE) UIDialog::SetLockInputMessage();
		g_pUIDialog = dialog;
		g_pKeyAccelerator = keys;
		g_pUserOption = option;
		g_pCGameUpdate = game;
		g_pCWaitPacketUpdate = waitPacket;
		g_pCWaitUIUpdate = waitUI;
		g_pSDLInput = input;
		g_pLast = last;
		g_pBack = back;
		g_GameRect = gameRect;
		g_y = y;
		g_x = x;
		g_bPreviousMove = previousMove;
		g_bCButtonDown = centre;
		g_bRButtonDown = right;
		g_bLButtonDown = left;
		g_SelectSector = selectSector;
		g_MorphCreatureType = morph;
		g_bUIInput = uiInput;
		g_bSetHotKey = setHotKey;
		g_CurrentFrame = currentFrame;
		g_FrameNow = frameNow;
		g_UpdateDelay = updateDelay;
		g_pUpdate = update;
		g_pSocket = socket;
		g_bActiveApp = activeApp;
		g_bActiveGame = activeGame;
		g_bRunning = running;
		g_bNeedUpdate = needUpdate;
		g_ModeNext = modeNext;
		g_Mode = mode;
	}
};

// The client on the title screen's side, settled: a connection, the three
// updates, the input (started, nothing held), the UI with the test's
// window over it, the screen (650x310 surfaces, the game 640x300 of them;
// the last frame patterned, the back buffer 0x7777), the dialogs (none
// open), the string, music and message tables, the chat filter (ignoring
// "Bob"), the user's options (MIDI music off, saved to a file of the
// test's), an event queued, and the game's objects (no player, no zone).
// The frame clock is far from the stamp; the main menu's theme is 2; the
// server is waited for 4321 ms.
const MonotonicClock::TimePoint kNow = MonotonicClock::TimePoint() + MonotonicClock::Millis(5000000);
struct Scene : GameModelWorld
{
	Saved saved;
	Connection connection;
	WaitUI waitUI;
	WaitPacket waitPacket;
	Game game;
	Other idle;
	CSDLInput input;
	Dialogs dialog;
	UserOption option;
	KeyAccelerator keys;
	MChatManager chat;
	CMessageArray systemLog, playerLog, gameLog, noticeLog;
	MUSIC_TABLE musicTable;
	TempInformation temp;
	MJusticeAttackManager justice;
	Properties files;
	MWeather weather;
	MParty party;
	MEventQueue events{ &eventHost };
	MWarManager wars;
	CI_KOREAN ime;
	CSpriteSurface back, last;
	std::unique_ptr<UiWindow> window;

	Scene()
	{
		calls.clear();
		initSnapshot.clear();
		onShow = nullptr;
		frames = 0;
		flips = 0;
		uiErrors = 0;
		current = &connection;
		g_pSocket = &connection;
		g_pCWaitUIUpdate = &waitUI;
		g_pCWaitPacketUpdate = &waitPacket;
		g_pCGameUpdate = &game;
		// Another loop running: the modes replace it (quitting with none).
		g_pUpdate = &idle;
		CHECK(input.Init(nullptr, nullptr));
		dxlib_input_virtual_reset();
		input.Clear();
		g_pSDLInput = &input;
		dialog.Init();
		g_pUIDialog = &dialog;
		UIDialog::UnSetLockInputPCTalk();
		UIDialog::UnSetLockInputMessage();
		g_bPartyRunning = false;
		option.PlayWaveMusic = FALSE;
		option.PlayMusic = FALSE;
		option.VolumeMusic = 13;
		g_pUserOption = &option;
		keys.Init(MAX_ACCELERATOR);
		g_pKeyAccelerator = &keys;
		chat.SetIgnoreMode();
		chat.AddID("Bob");
		g_pChatManager = &chat;
		for (CMessageArray* log : {&systemLog, &playerLog, &gameLog, &noticeLog}) log->Init(3, 128);
		systemLog.Add("system");
		playerLog.Add("player");
		gameLog.Add("game");
		noticeLog.Add("notice");
		g_pSystemMessage = &systemLog;
		g_pPlayerMessage = &playerLog;
		g_pGameMessage = &gameLog;
		g_pNoticeMessage = &noticeLog;
		g_pGameStringTable->Init(MAX_GAME_STRING + 1);
		g_pGameStringTable->Set(STRING_MESSAGE_WAIT, kWait);
		g_pGameStringTable->Set(STRING_MESSAGE_HELP_KEY, kHelpKey);
		musicTable.Init(3);
		testfw::MutableRow(musicTable, kTheme).Filename = "theme.mid";
		g_pMusicTable = &musicTable;
		g_pClientConfig->MUSIC_THEME = kTheme;
		g_pClientConfig->MAX_WAIT_PACKET = int(kMaxWait);
		g_Music = MMusic();
		g_Music.Init(NULL);
		// Down: StopSound returns at once, the NULL g_pOGG is never touched.
		CHECK(!g_SDLAudio.IsInit());
		g_SDLAudio.UnSetMute();
		temp.SetMode(TempInformation::MODE_SKILL_LEARN);
		g_pTempInformation = &temp;
		g_pJusticeAttackManager = &justice;
		g_pPlayer = NULL;
		g_pZone = NULL;
		g_pTopView = NULL;
		g_pCreatureTable = NULL;
		WriteUiPack();
		files.setProperty("FILE_INFO_USEROPTION", kOptionFile);
		files.setProperty("FILE_SPRITEINDEX_UI", kIndexFile);
		files.setProperty("FILE_SPRITE_UI", kSpriteFile);
		g_pFileDef = &files;
		std::remove(kOptionFile);
		g_pWeather = &weather;
		g_pParty = &party;
		MEvent event;
		event.eventID = EVENTID_HALLUCINATION;
		event.eventFlag = 0;
		events.AddEvent(event);
		g_pEventManager = &events;
		g_pWarManager = &wars;
		g_pTradeManager = NULL;
		g_pStorage = g_pStorage2 = NULL;
		g_pZoneSoundTable = NULL;
		g_pZoneSoundManager = NULL;
		g_pWorldPlayerConfigTable = NULL;
		g_pServerInformation = NULL;
		g_pRequestServerPlayerManager = NULL;
		g_pClientCommunicationManager = NULL;
		g_previousSoundID = SOUNDID_NULL;
		// The UI, on the heap: C_VS_UI::Release frees it.
		gpC_process_runner = new PI_ProcessRunner;
		gpC_window_manager = new WindowManager;
		gpC_base = new Base;
		gpC_mouse_pointer = new C_VS_UI_MOUSE_POINTER;
		gpC_global_resource = new C_GLOBAL_RESOURCE;
		g_pSkinManager = new SkinManager;
		for (int i = 0; i < SkinManager::INTERFACE_MAX; ++i)
			for (int n = 0; n < 64; ++n)
			{
				testfw::MutableRow(*g_pSkinManager, i).LoadFromLinePointList("0 0");
				testfw::MutableRow(*g_pSkinManager, i).LoadFromLineRectList("0 0 1 1");
			}
		gpC_vs_ui_window_manager = NULL;
		gpC_item = NULL;
		gC_ci = &ime;
		gbl_global_empty_move = false;
		spritectl_init();
		back.Init(kGameWidth + 10, kGameHeight + 10);
		last.Init(kGameWidth + 10, kGameHeight + 10);
		back.FillSurface(kBack);
		Pattern(last);
		g_pBack = &back;
		g_pLast = &last;
		gpC_base->m_p_DDSurface_back = &back;
		g_GameRect = {0, 0, kGameWidth, kGameHeight};
		CSDLGraphics::SetBackBufferSource(Flipped);
		window = std::make_unique<UiWindow>();
		window->Start();
		gpC_window_manager->AppearWindow(window.get());
		g_FrameNow = kNow;
		g_CurrentFrame = 77;
		g_Mode = MODE_NULL;
		g_ModeNext = MODE_WAIT_PCLIST;
		g_bNeedUpdate = FALSE;
		g_bRunning = true;
		g_bActiveApp = TRUE;
		g_bActiveGame = FALSE;
		g_bSetHotKey = TRUE;
		g_bUIInput = TRUE;
		g_MorphCreatureType = 5;
		g_SelectSector.x = 1;
		g_SelectSector.y = 2;
		g_bLButtonDown = g_bRButtonDown = g_bCButtonDown = TRUE;
		g_UpdateDelay = 999;
		g_x = 7;
		g_y = 9;
		g_ZoneAmbientSounds.SetNextSoundTime(MonotonicClock::TimePoint());
		g_bCheckHack = true;
		g_pUserInformation->KeepConnection = TRUE;
		g_pUserInformation->IsAutoLogIn = false;
		g_pUserInformation->pLogInClientPlayer = NULL;
		g_pUserInformation->CharacterID = "Lantern";
		g_pUserInformation->WhisperID = "Moth";
		g_pUserInformation->LogoutTime = kNow;
		int mouseX = -1, mouseY = -1;
		SDL_GetMouseState(&mouseX, &mouseY);
		CHECK(mouseX == 0 && mouseY == 0);
		calls.clear();
	}
	~Scene()
	{
		SDL_StopTextInput();
		dxlib_input_virtual_reset();
		LeftButton(false);
		input.Clear();
		gpC_window_manager->DisappearWindow(window.get());
		window.reset();
		// The title, if started, and the UI's objects.
		gC_vs_ui.Release();
		dialog.Release();
		std::remove(kOptionFile);
		std::remove(kSpriteFile);
		std::remove(kIndexFile);
		current = nullptr;
		onShow = nullptr;
	}

	// The mode entered, from a frame's leftovers: the buttons held and the
	// input pulsed. What it called is answered.
	std::string Enter(CLIENT_MODE mode)
	{
		g_bLButtonDown = g_bRButtonDown = g_bCButtonDown = TRUE;
		Dirty();
		calls.clear();
		SetMode(mode);
		return calls;
	}
	// The buttons let go and the input clear.
	bool Settled() const { return !g_bLButtonDown && !g_bRButtonDown && !g_bCButtonDown && Clean(input); }

	// The music playing (MIDI), or stopped.
	void Playing() { g_Music.m_bLoad = true; g_Music.m_bPlay = true; g_Music.m_bPause = false; }
	static std::string Music() { return std::string(g_Music.m_bLoad ? "loaded" : "empty") + (g_Music.m_bPlay ? "|play" : ""); }

	// The four message logs, joined.
	std::string Logs()
	{
		std::string text;
		for (CMessageArray* log : {&systemLog, &playerLog, &gameLog, &noticeLog})
			for (int i = 0; i < log->GetSize(); ++i)
				if ((*log)[i] != nullptr && (*log)[i][0] != '\0') text += std::string((*log)[i]) + ";";
		return text;
	}
};

// A player standing by: a slayer (the only creature type) mid-swing.
struct PlayerProbe : MPlayer
{
	using MCreature::m_Action;
	using MCreature::m_ActionCount;
	using MCreature::m_ActionCountMax;
};
struct Body
{
	CREATURE_TABLE* oldCreatures = g_pCreatureTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	CREATURE_TABLE creatures;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	CREATURESPRITE_TABLE sprites;
	MActionInfoTable actions;
	RankBonusTable rankBonuses;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	PlayerProbe* me = nullptr;
	Body()
	{
		creatures.Init(1);
		testfw::MutableRow(creatures, 0).SetCreatureTribe(CREATURETRIBE_SLAYER, ACTION_MAX_SLAYER);
		for (int action = 0; action < ACTION_MAX_SLAYER; ++action)
			testfw::MutableRow(creatures, 0).SetActionCount(action, 10 + action);
		g_pCreatureTable = &creatures;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		sprites.Init(1);
		g_pCreatureSpriteTable = &sprites;
		actions.Init(MAX_ACTIONINFO);
		for (int info = 0; info < MAX_ACTIONINFO; ++info)
		{
			actions.GetMutable(info)->Set("", ACTION_STAND, EFFECTSPRITETYPE_NULL, 0, 0);
			actions.GetMutable(info)->SetSoundID(SOUNDID_NULL);
		}
		actions.SetMinResultActionInfo(MIN_RESULT_ACTIONINFO);
		g_pActionInfoTable = &actions;
		g_pRankBonusTable = &rankBonuses;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		me = new PlayerProbe;
		me->SetCreatureType(0);
		me->SetID(kPlayerID);
		me->m_Action = ACTION_ATTACK;
		me->m_ActionCount = 0;
		me->m_ActionCountMax = 5;
		g_pPlayer = me;
	}
	~Body()
	{
		// Unless UpdateDisconnected deleted it.
		if (g_pPlayer != NULL) delete me;
		g_pPlayer = NULL;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pRankBonusTable = oldRankBonuses;
		g_pActionInfoTable = oldActions;
		g_pCreatureSpriteTable = oldSprites;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pCreatureTable = oldCreatures;
	}
};

const std::string kUiWait = "ui init;";
// Starting the title hands the UI's cursor, at (0, 0), to the window under
// it.
const std::string kTitle = "move@0,0;";
const std::string kPacketWait = "stamp;receivers off;";

} // namespace

//======================================================================
// The host
//======================================================================
TEST(GameMain2Host, SetGameMainHostAnswersTheHostItReplaces)
{
	const GameMainHost other{};
	const GameMainHost* old = SetGameMainHost(&mainHost);
	CHECK(SetGameMainHost(&other) == &mainHost);
	CHECK(SetGameMainHost(NULL) == &other);
	CHECK(SetGameMainHost(old) == NULL);
}

//======================================================================
// SetMode: what every mode does
//======================================================================
TEST(GameMain2SetMode, TheInputIsReadThenClearedBeforeTheModeAndClearedAgainAfter)
{
	Scene scene;
	// The keys reach the receiver the input has before the mode.
	scene.input.SetKeyboardEventReceiver([](CSDLInput::E_KEYBOARD_EVENT event, DWORD key) {
		Call((event == CSDLInput::KEYDOWN ? "key" : "keyup") + N(key));
	});
	Hold(DIK_F5);
	scene.input.m_lb_down = TRUE;
	scene.input.m_mouse_z = 4;
	g_bLButtonDown = g_bRButtonDown = g_bCButtonDown = TRUE;
	SetMode(MODE_NEWUSER);
	// Read (the key comes in), then cleared before the loop's Init saw it.
	CHECK_SAME("key" + N(DIK_F5) + ";" + kUiWait, calls);
	CHECK_SAME(std::string(""), initSnapshot);
	// The pulse Init left is cleared at the end too.
	CHECK(scene.Settled());
	CHECK(!scene.input.KeyDown(DIK_F5));
}

TEST(GameMain2SetMode, TheWaitsOnTheUIRunTheUILoopEvery20Ms)
{
	struct Case { CLIENT_MODE mode; bool dropsConnection; bool choosesCharacter; };
	for (const Case c : {Case{MODE_NEWUSER, false, false}, Case{MODE_WAIT_SELECT_WORLD, true, false},
		Case{MODE_WAIT_SELECT_SERVER, true, false}, Case{MODE_WAIT_SELECTPC, true, true}})
	{
		Scene scene;
		CHECK_SAME(kUiWait, scene.Enter(c.mode));
		CHECK_EQ(int(c.mode), int(g_Mode));
		CHECK_EQ(20, g_UpdateDelay);
		CHECK(g_pUpdate == &scene.waitUI);
		CHECK(scene.Settled());
		CHECK_EQ(c.dropsConnection ? FALSE : TRUE, g_pUserInformation->KeepConnection);
		// The character window: the last character's table is made (with no
		// server information nothing is looked up) and the morph is forgotten.
		CHECK_EQ(c.choosesCharacter, g_pWorldPlayerConfigTable != NULL);
		CHECK_EQ(c.choosesCharacter ? 0 : 5, g_MorphCreatureType);
		delete g_pWorldPlayerConfigTable;
		g_pWorldPlayerConfigTable = NULL;
		// Nothing else.
		CHECK_EQ(int(MODE_WAIT_PCLIST), int(g_ModeNext));
		CHECK_EQ(FALSE, g_bNeedUpdate);
		CHECK(std::string(g_pUserInformation->CharacterID.GetString()) == "Lantern");
		CHECK(!gC_vs_ui.IsRunningTitle());
		CHECK_SAME(std::string("system;player;game;notice;"), scene.Logs());
	}
}

TEST(GameMain2SetMode, AWrongLoginIsTheMainMenuWithoutTheTitleOrTheConnection)
{
	Scene scene;
	CHECK_SAME("release socket;" + kUiWait, scene.Enter(MODE_LOGIN_WRONG));
	CHECK_EQ(int(MODE_MAINMENU), int(g_Mode));
	CHECK_EQ(20, g_UpdateDelay);
	CHECK(g_pUpdate == &scene.waitUI);
	CHECK(scene.Settled());
	// Not the main menu's work: no title, the logs and the music kept.
	CHECK(!gC_vs_ui.IsRunningTitle());
	CHECK_SAME(std::string("system;player;game;notice;"), scene.Logs());
	CHECK_EQ(TRUE, g_pUserInformation->KeepConnection);
	CHECK_EQ(TRUE, g_bUIInput);
}

TEST(GameMain2SetMode, TheWaitsOnTheServerRunThePacketLoopEvery50MsAndGiveUpAfterTheConfiguredWait)
{
	for (CLIENT_MODE mode : {MODE_WAIT_LOGINOK, MODE_WAIT_WORLD_LIST, MODE_WAIT_SERVER_LIST, MODE_WAIT_REGISTERPLAYEROK,
		MODE_WAIT_PCLIST, MODE_WAIT_CREATEPCOK, MODE_WAIT_DELETEPCOK, MODE_WAIT_RECONNECT, MODE_WAIT_RECONNECT_LOGIN})
	{
		Scene scene;
		// The limit is set from the stamp, then the loop starts.
		CHECK_SAME(kPacketWait, scene.Enter(mode));
		CHECK_EQ(int(mode), int(g_Mode));
		CHECK_EQ(50, g_UpdateDelay);
		CHECK(g_pUpdate == &scene.waitPacket);
		CHECK(scene.waitPacket.m_DelayLimit == kStamp + MonotonicClock::Millis(kMaxWait));
		CHECK(scene.Settled());
		// The character list starts the ambient sounds' clock at the frame
		// (before the stamp moved it).
		CHECK(g_ZoneAmbientSounds.GetNextSoundTime() == (mode == MODE_WAIT_PCLIST ? kNow : MonotonicClock::TimePoint()));
		// Reconnecting to the login server forgets the character.
		if (mode == MODE_WAIT_RECONNECT_LOGIN)
			CHECK(g_pUserInformation->CharacterID.GetString() == NULL);
		else
			CHECK(std::string(g_pUserInformation->CharacterID.GetString()) == "Lantern");
		CHECK(std::string(g_pUserInformation->WhisperID.GetString()) == "Moth");
		CHECK_EQ(TRUE, g_pUserInformation->KeepConnection);
		CHECK_EQ(5, g_MorphCreatureType);
		CHECK_EQ(int(MODE_WAIT_PCLIST), int(g_ModeNext));
		CHECK(scene.connection.Sent().empty());
	}
}

TEST(GameMain2SetMode, ModesWithoutWorkOnlySetTheModeAndClearTheInput)
{
	for (CLIENT_MODE mode : {MODE_NULL, MODE_OPENING, MODE_CREATECHARACTER, MODE_SELECTCHARACTER})
	{
		Scene scene;
		CHECK_SAME(std::string(""), scene.Enter(mode));
		CHECK_EQ(int(mode), int(g_Mode));
		CHECK(g_pUpdate == &scene.idle);
		CHECK_EQ(999, g_UpdateDelay);
		CHECK(scene.Settled());
		CHECK_EQ(int(MODE_WAIT_PCLIST), int(g_ModeNext));
		CHECK_EQ(TRUE, g_bActiveApp);
		CHECK(g_bRunning);
	}
}

//======================================================================
// SetMode(MODE_QUIT)
//======================================================================
TEST(GameMain2Quit, QuittingToUpdateSaysGoodbyeToTheLoginServer)
{
	Scene scene;
	g_bNeedUpdate = TRUE;
	CLLogout logout;
	CHECK_SAME(Send(logout), scene.Enter(MODE_QUIT));
	CHECK(scene.connection.Sent() == Encoded({&logout}));
	CHECK_EQ(int(MODE_QUIT), int(g_Mode));
	CHECK(g_pUpdate == NULL);
	CHECK_EQ(FALSE, g_bUIInput);
	CHECK_EQ(FALSE, g_bActiveApp);
#ifndef PLATFORM_WINDOWS
	// Off Windows the main loop stops; Windows posts WM_CLOSE instead.
	CHECK(!g_bRunning);
#else
	CHECK(g_bRunning);
#endif
	CHECK(scene.Settled());
	CHECK_EQ(TRUE, g_bNeedUpdate);
	CHECK_EQ(int(MODE_WAIT_PCLIST), int(g_ModeNext));
}

TEST(GameMain2Quit, QuittingTheGameSaysGoodbyeToTheGameServer)
{
	Scene scene;
	CGLogout logout;
	CHECK_SAME(Send(logout), scene.Enter(MODE_QUIT));
	CHECK(scene.connection.Sent() == Encoded({&logout}));
	CHECK(g_pUpdate == NULL);
	CHECK_EQ(FALSE, g_bActiveApp);
	CHECK(scene.Settled());
}

TEST(GameMain2Quit, WithoutAConnectionNothingIsSent)
{
	Scene scene;
	g_pSocket = NULL;
	CHECK_SAME(std::string(""), scene.Enter(MODE_QUIT));
	CHECK(scene.connection.Sent().empty());
	CHECK(g_pUpdate == NULL);
	CHECK_EQ(FALSE, g_bUIInput);
	CHECK_EQ(FALSE, g_bActiveApp);
#ifndef PLATFORM_WINDOWS
	CHECK(!g_bRunning);
#endif
}

//======================================================================
// SetMode(MODE_GAME)
//======================================================================
TEST(GameMain2Game, TheGameStartsItsLoopEvery62MsActivatedAndHelped)
{
	Scene scene;
	g_bPreviousMove = true;
	g_SDLAudio.SetMute();
	UIDialog::SetLockInputPCTalk();
	UIDialog::SetLockInputMessage();
	// Activated (CheckActivate(TRUE)) before the interface's help mail.
	CHECK_SAME("game init;help" + N(HELP_EVENT_INTERFACE) + "@active1;", scene.Enter(MODE_GAME));
	CHECK_EQ(int(MODE_GAME), int(g_Mode));
	// "100 ^ 90": an exclusive or, 62 ms (the comment calls it the speed-up
	// address's edit).
	CHECK_EQ(62, g_UpdateDelay);
	CHECK(g_pUpdate == &scene.game);
	CHECK(!g_bPreviousMove);
	CHECK(scene.Settled());
	CHECK_EQ(int(TempInformation::MODE_NULL), int(scene.temp.GetMode()));
	CHECK(!g_pUserInformation->IsLogoutScheduled());
	CHECK_EQ(0u, unsigned(UIDialog::IsLockInput()));
	CHECK_EQ(TRUE, g_bActiveGame);
	CHECK(!g_SDLAudio.IsMute());
	CHECK(scene.connection.Sent().empty());
}

TEST(GameMain2Game, AnInactiveApplicationLeavesTheGameInactive)
{
	Scene scene;
	g_bActiveApp = FALSE;
	g_bActiveGame = TRUE;
	g_SDLAudio.SetMute();
	CHECK_SAME("game init;help" + N(HELP_EVENT_INTERFACE) + "@active0;", scene.Enter(MODE_GAME));
	CHECK(g_pUpdate == &scene.game);
	CHECK_EQ(FALSE, g_bActiveGame);
	CHECK(g_SDLAudio.IsMute());
	CHECK(scene.Settled());
}

//======================================================================
// SetMode(MODE_WAIT_SETPOSITION)
//======================================================================
TEST(GameMain2SetPosition, ThePlayerStopsTheOptionsAreSavedAndTheServerIsToldWeAreReady)
{
	Scene scene;
	Body body;
	MTopView view;
	view.SetSelectedCreature(42);
	g_pTopView = &view;
	CGReady ready;
	// The sound is started, the ready sent and written at once, then the
	// packet loop waits.
	CHECK_SAME("sound;" + Send(ready) + "out;" + kPacketWait, scene.Enter(MODE_WAIT_SETPOSITION));
	CHECK(scene.connection.Sent() == Encoded({&ready}));
	CHECK_EQ(int(CPS_WAITING_FOR_GC_SET_POSITION), int(scene.connection.getPlayerStatus()));
	CHECK_EQ(int(MODE_WAIT_SETPOSITION), int(g_Mode));
	CHECK_EQ(50, g_UpdateDelay);
	CHECK(g_pUpdate == &scene.waitPacket);
	CHECK(scene.waitPacket.m_DelayLimit == kStamp + MonotonicClock::Millis(kMaxWait));
	CHECK(scene.Settled());
	// The player stands.
	CHECK_EQ(int(ACTION_STAND), int(body.me->m_Action));
	// The options are saved to their file.
	{
		std::ifstream saved(kOptionFile, std::ios::binary);
		const std::string text((std::istreambuf_iterator<char>(saved)), std::istreambuf_iterator<char>());
		// Written in text mode: the line ends "\r\n" on Windows.
		CHECK(text.find("\n13\tVolumeMusic") != std::string::npos);
	}
	// The help key's message, the first time only.
	CHECK_SAME("system;" + std::string(kHelpKey) + ";player;game;notice;", scene.Logs());
	// Nothing is picked.
	CHECK(g_SelectSector.x == 0 && g_SelectSector.y == 0);
	CHECK(view.GetSelectedCreature() == OBJECTID_NULL);

	// Again: no second help message, until the state is forgotten.
	SetMode(MODE_WAIT_SETPOSITION);
	CHECK_SAME("system;" + std::string(kHelpKey) + ";player;game;notice;", scene.Logs());
	ResetGameMainState();
	SetMode(MODE_WAIT_SETPOSITION);
	CHECK_SAME("system;" + std::string(kHelpKey) + ";" + kHelpKey + ";player;game;notice;", scene.Logs());
	g_pTopView = NULL;
}

TEST(GameMain2SetPosition, WithoutOptionsOrViewTheRestIsDone)
{
	Scene scene;
	Body body;
	g_pUserOption = NULL;
	CGReady ready;
	CHECK_SAME("sound;" + Send(ready) + "out;" + kPacketWait, scene.Enter(MODE_WAIT_SETPOSITION));
	CHECK(!std::ifstream(kOptionFile, std::ios::binary).good());
	CHECK(g_SelectSector.x == 0 && g_SelectSector.y == 0);
	CHECK_EQ(int(ACTION_STAND), int(body.me->m_Action));
	CHECK(g_pUpdate == &scene.waitPacket);
}

//======================================================================
// SetMode(MODE_CHANGE_OPTION)
//======================================================================
TEST(GameMain2ChangeOption, AViewThatCannotBeRebuiltQuits)
{
	Scene scene;
	MTopView view;	// never initialised: InitChanges refuses
	g_pTopView = &view;
	scene.Playing();
	CGLogout logout;
	// The display and its surfaces again, "please wait" shown, the UI run
	// and drawn, the last frame copied and flipped; the box closed, quit.
	CHECK_SAME("draw;surface;popup|-1|-1|0|0|" + std::string(kWait) + ";process;show;flip;close message;" + Send(logout),
		scene.Enter(MODE_CHANGE_OPTION));
	CHECK_EQ(int(MODE_QUIT), int(g_Mode));
	CHECK(g_pUpdate == NULL);
	CHECK(scene.connection.Sent() == Encoded({&logout}));
	CHECK(scene.Settled());
	CHECK_EQ(0u, unsigned(UIDialog::IsLockInput()));
	// The MIDI music was stopped first.
	CHECK_SAME(std::string("empty"), Scene::Music());
	// The game's part of the last frame is on the back buffer.
	CHECK_EQ(Patterned(0, 0), Pixel(scene.back, 0, 0));
	CHECK_EQ(Patterned(kGameWidth - 1, kGameHeight - 1), Pixel(scene.back, kGameWidth - 1, kGameHeight - 1));
	CHECK_EQ(kBack, Pixel(scene.back, kGameWidth, 0));
	CHECK_EQ(kBack, Pixel(scene.back, 0, 300));
	g_pTopView = NULL;
}

TEST(GameMain2ChangeOption, WaveMusicWithTheAudioDownIsLeftAlone)
{
	Scene scene;
	MTopView view;
	g_pTopView = &view;
	CHECK(!g_SDLAudio.IsInit());
	scene.option.PlayWaveMusic = TRUE;
	scene.Playing();
	scene.Enter(MODE_CHANGE_OPTION);
	CHECK_SAME(std::string("loaded|play"), Scene::Music());
	CHECK_EQ(int(MODE_QUIT), int(g_Mode));
	g_pTopView = NULL;
}

TEST(GameMain2ChangeOption, WithoutAViewItGoesOnToTheMainMenu)
{
	Scene scene;
	CHECK_SAME("draw;surface;popup|-1|-1|0|0|" + std::string(kWait) + ";process;show;flip;close message;sound;" + kTitle + kUiWait,
		scene.Enter(MODE_CHANGE_OPTION));
	CHECK_EQ(int(MODE_MAINMENU), int(g_Mode));
	CHECK(gC_vs_ui.IsRunningTitle());
	CHECK(g_pUpdate == &scene.waitUI);
	CHECK(scene.Settled());
	CHECK(scene.connection.Sent().empty());
	CHECK(g_bRunning);
}

//======================================================================
// SetMode(MODE_MAINMENU)
//======================================================================
TEST(GameMain2MainMenu, TheMainMenuStartsTheTitleAndForgetsTheGame)
{
	Scene scene;
	scene.dialog.OpenTalk();
	CHECK_SAME("sound;hide talk;" + kTitle + kUiWait, scene.Enter(MODE_MAINMENU));
	CHECK_EQ(int(MODE_MAINMENU), int(g_Mode));
	CHECK_EQ(20, g_UpdateDelay);
	CHECK(g_pUpdate == &scene.waitUI);
	CHECK(scene.Settled());
	CHECK(gC_vs_ui.IsRunningTitle());
	CHECK_EQ(0u, unsigned(g_CurrentFrame));
	CHECK_EQ(FALSE, g_bSetHotKey);
	CHECK_EQ(FALSE, g_bUIInput);
	// The talk box is hidden, not closed.
	CHECK(scene.dialog.TalkOpen());
	// Chat from everyone: the list cleared, the mode accept.
	CHECK(scene.chat.IsAcceptMode());
	scene.chat.SetIgnoreMode();
	CHECK(!scene.chat.IsAcceptID("Bob"));
	// No whisper target, no character, no messages.
	CHECK(g_pUserInformation->WhisperID.GetString() == NULL);
	CHECK(g_pUserInformation->CharacterID.GetString() == NULL);
	CHECK_SAME(std::string(""), scene.Logs());
	// The connection is kept (not a web login), the music too (off).
	CHECK_SAME(std::string("empty"), Scene::Music());
	CHECK(scene.connection.Sent().empty());
	CHECK_EQ(int(MODE_WAIT_PCLIST), int(g_ModeNext));
}

TEST(GameMain2MainMenu, OnlyAWebLoginThatNeverLoggedInReleasesTheConnection)
{
	struct Case { bool autoLogIn; bool loggedIn; bool releases; };
	for (const Case c : {Case{true, false, true}, Case{true, true, false}, Case{false, false, false}, Case{false, true, false}})
	{
		Scene scene;
		int player = 0;
		g_pUserInformation->IsAutoLogIn = c.autoLogIn;
		g_pUserInformation->pLogInClientPlayer = c.loggedIn ? &player : NULL;
		CHECK_SAME(std::string(c.releases ? "release socket;" : "") + "sound;" + kTitle + kUiWait, scene.Enter(MODE_MAINMENU));
		g_pUserInformation->pLogInClientPlayer = NULL;
	}
}

TEST(GameMain2MainMenu, TheThemePlaysWhenMusicIsOn)
{
	Scene scene;
	scene.option.PlayMusic = TRUE;
	scene.Enter(MODE_MAINMENU);
	CHECK_SAME(std::string("loaded|play"), Scene::Music());
}

TEST(GameMain2MainMenu, NoThemeNoMusic)
{
	Scene scene;
	scene.option.PlayMusic = TRUE;
	g_pClientConfig->MUSIC_THEME = MUSICID_NULL;
	scene.Enter(MODE_MAINMENU);
	CHECK_SAME(std::string("empty"), Scene::Music());
}

TEST(GameMain2MainMenu, OnlyWaveMusicStopsThePlayingMusic)
{
	Scene scene;
	// Wave music, the music off: what played stops.
	scene.option.PlayWaveMusic = TRUE;
	scene.Playing();
	scene.Enter(MODE_MAINMENU);
	CHECK_SAME(std::string("empty"), Scene::Music());
	// Recorded defect (GameMain.cpp:560-562, inventory bug 4): the stop is
	// the body of `if (PlayWaveMusic)`, a comment between them, so the MIDI
	// music the player turned off plays on into the title; CHANGE_OPTION
	// and UpdateDisconnected stop it (and close the wave stream instead).
	scene.option.PlayWaveMusic = FALSE;
	scene.Playing();
	scene.Enter(MODE_MAINMENU);
	CHECK_SAME(std::string("loaded|play"), Scene::Music());
}

//======================================================================
// UpdateSocketInput / UpdateSocketOutput
//======================================================================
TEST(GameMain2SocketInput, WithoutAConnectionOrPeersThereIsNothingToDo)
{
	Scene scene;
	g_pSocket = NULL;
	CHECK(UpdateSocketInput());
	CHECK_SAME(std::string(""), calls);
	CHECK_EQ(int(MODE_NULL), int(g_Mode));
}

TEST(GameMain2SocketInput, ThePacketsAreReadAndRunThenThePeersAndTheDatagramsUpdated)
{
	Scene scene;
	Peers peers;
	peers.Add();
	ClientCommunicationManager datagrams;
	g_pRequestServerPlayerManager = &peers;
	g_pClientCommunicationManager = &datagrams;
	CHECK(UpdateSocketInput());
	// The peer's update dropped the peer that never connected.
	CHECK_SAME(std::string("in;command;peer;datagrams;"), calls);
	CHECK_EQ(0, peers.GetSize());
	CHECK_EQ(int(MODE_NULL), int(g_Mode));
}

TEST(GameMain2SocketInput, ThePeersAreUpdatedAtMostEvery330Ms)
{
	Scene scene;
	Peers peers;
	ClientCommunicationManager datagrams;
	const MonotonicClock::TimePoint t = kNow;
	// One peer waiting (one the update did not reach stays for the next).
	auto At = [&](int ms) {
		if (peers.GetSize() == 0) peers.Add();
		calls.clear();
		g_FrameNow = t + MonotonicClock::Millis(ms);
		CHECK(UpdateSocketInput());
		return calls;
	};
	// No peers yet: the first call still sets the next update.
	g_FrameNow = t;
	CHECK(UpdateSocketInput());
	CHECK_SAME(std::string("in;command;"), calls);
	g_pRequestServerPlayerManager = &peers;
	g_pClientCommunicationManager = &datagrams;
	const std::string updated = "in;command;peer;datagrams;";
	CHECK_SAME(std::string("in;command;"), At(329));
	CHECK_SAME(updated, At(330));
	CHECK_SAME(std::string("in;command;"), At(659));
	CHECK_SAME(updated, At(660));
	// Counted from the update, not from the deadline.
	CHECK_SAME(updated, At(1000));
	CHECK_SAME(std::string("in;command;"), At(1329));
	CHECK_SAME(updated, At(1330));
}

TEST(GameMain2SocketInput, AFreshStateUpdatesAtTheFirstCall)
{
	Scene scene;
	Peers peers;
	g_pRequestServerPlayerManager = &peers;
	g_FrameNow = kNow;
	peers.Add();
	CHECK(UpdateSocketInput());
	CHECK_SAME(std::string("in;command;peer;"), calls);
	// The next 330 ms after; forgotten, the next call updates again.
	peers.Add();
	calls.clear();
	CHECK(UpdateSocketInput());
	CHECK_SAME(std::string("in;command;"), calls);
	ResetGameMainState();
	calls.clear();
	CHECK(UpdateSocketInput());
	CHECK_SAME(std::string("in;command;peer;"), calls);
}

namespace {
// What the client does when the connection fails: back to the main menu
// (the title's sound, the UI loop), then disconnected (the connection,
// the game's objects with the queued event; the box drawn once, the game
// not active: the UI drawn over it).
const std::string kMenuAndDisconnected = "sound;" + kTitle + kUiWait + "release socket;gamma;flip;show;move@7,9;pointer;flip;";
// The bug report the client sends for a protocol error.
std::vector<unsigned char> Report(const Throwable& t)
{
	CGSay say;
	say.setMessage("*bug_report " + t.toString());
	say.setColor(0);
	return Encoded({&say});
}
std::string SayID() { return N(CGSay().getPacketID()); }
} // namespace

TEST(GameMain2SocketInput, AProtocolErrorIsReportedThenTheClientGoesToTheMainMenuDisconnected)
{
	Scene scene;
	Peers peers;
	g_pRequestServerPlayerManager = &peers;
	peers.Add();
	scene.connection.inFault = Fault::Protocol;
	CHECK(!UpdateSocketInput());
	// The command is not run, the peers not updated.
	CHECK_SAME("in;send" + SayID() + ";" + kMenuAndDisconnected, calls);
	CHECK(scene.connection.Sent() == Report(InvalidProtocolException("bad packet id")));
	CHECK_EQ(int(MODE_MAINMENU), int(g_Mode));
	CHECK(g_pUpdate == &scene.waitUI);
	CHECK(gC_vs_ui.IsRunningTitle());
	CHECK_EQ(1, peers.GetSize());
	// The game's objects are gone with the event.
	CHECK(scene.events.IsEmptyEvent());
}

TEST(GameMain2SocketInput, ACommandsErrorIsHandledTheSame)
{
	Scene scene;
	scene.connection.commandFault = Fault::Protocol;
	CHECK(!UpdateSocketInput());
	CHECK_SAME("in;command;send" + SayID() + ";" + kMenuAndDisconnected, calls);
	CHECK(scene.connection.Sent() == Report(InvalidProtocolException("bad packet id")));
	CHECK_EQ(int(MODE_MAINMENU), int(g_Mode));
}

TEST(GameMain2SocketInput, OnlyAProtocolErrorOffTheDatagramsIsReported)
{
	struct Case { Fault fault; bool reported; };
	for (const Case c : {Case{Fault::ProtocolDatagram, false}, Case{Fault::Other, false}, Case{Fault::NamedInText, true}})
	{
		Scene scene;
		scene.connection.inFault = c.fault;
		CHECK(!UpdateSocketInput());
		// The text is matched, not the type: an error whose message names
		// the protocol exception is reported too.
		CHECK_SAME("in;" + (c.reported ? "send" + SayID() + ";" : std::string()) + kMenuAndDisconnected, calls);
		if (c.reported)
			CHECK(scene.connection.Sent() == Report(IOException("an InvalidProtocolException upstream")));
		else
			CHECK(scene.connection.Sent().empty());
		CHECK_EQ(int(MODE_MAINMENU), int(g_Mode));
	}
}

TEST(GameMain2SocketInput, AnythingButAThrowableEscapes)
{
	Scene scene;
	// Recorded defect (inventory bug 8): only Throwable is caught; a
	// std::exception from a reader leaves UpdateSocketInput (and the main
	// loop) with nothing cleaned up.
	scene.connection.inFault = Fault::Std;
	bool escaped = false;
	try { UpdateSocketInput(); } catch (const std::runtime_error&) { escaped = true; }
	CHECK(escaped);
	CHECK_SAME(std::string("in;"), calls);
	CHECK_EQ(int(MODE_NULL), int(g_Mode));
	CHECK(!gC_vs_ui.IsRunningTitle());
}

TEST(GameMain2SocketOutput, TheConnectionIsWrittenAndNothingElse)
{
	Scene scene;
	Peers peers;
	g_pRequestServerPlayerManager = &peers;
	peers.Add();
	g_FrameNow = kNow;
	CHECK(UpdateSocketOutput());
	// No peer update: that is the input's.
	CHECK_SAME(std::string("out;"), calls);
	CHECK_EQ(1, peers.GetSize());
	g_pSocket = NULL;
	calls.clear();
	CHECK(UpdateSocketOutput());
	CHECK_SAME(std::string(""), calls);
}

TEST(GameMain2SocketOutput, AFailedWriteGoesToTheMainMenuDisconnected)
{
	struct Case { Fault fault; bool reported; };
	for (const Case c : {Case{Fault::Protocol, true}, Case{Fault::ProtocolDatagram, false}, Case{Fault::Other, false}})
	{
		Scene scene;
		scene.connection.outFault = c.fault;
		CHECK(!UpdateSocketOutput());
		CHECK_SAME("out;" + (c.reported ? "send" + SayID() + ";" : std::string()) + kMenuAndDisconnected, calls);
		if (c.reported)
			CHECK(scene.connection.Sent() == Report(InvalidProtocolException("bad packet id")));
		else
			CHECK(scene.connection.Sent().empty());
		CHECK_EQ(int(MODE_MAINMENU), int(g_Mode));
		CHECK(gC_vs_ui.IsRunningTitle());
	}
}

TEST(GameMain2SocketOutput, AnythingButAThrowableEscapes)
{
	Scene scene;
	scene.connection.outFault = Fault::Std;
	bool escaped = false;
	try { UpdateSocketOutput(); } catch (const std::runtime_error&) { escaped = true; }
	CHECK(escaped);
	CHECK_SAME(std::string("out;"), calls);
	CHECK_EQ(int(MODE_NULL), int(g_Mode));
}

//======================================================================
// UpdateDisconnected
//======================================================================
TEST(GameMain2Disconnected, InactiveTheBoxIsDrawnOnceUnderTheUI)
{
	Scene scene;
	Body body;
	scene.Playing();
	scene.dialog.OpenTalk();
	scene.dialog.OpenMessage();
	UpdateDisconnected();
	// The connection, the game's objects (the event removed), the dialogs;
	// the box drawn and flipped, then the UI and the cursor over it, flipped.
	CHECK_SAME(std::string("release socket;gamma;close message;close talk;flip;show;move@7,9;pointer;flip;"), calls);
	CHECK_SAME(std::string("empty"), Scene::Music());
	CHECK(g_pPlayer == NULL);
	CHECK(!scene.dialog.TalkOpen());
	CHECK(!scene.dialog.MessageOpen());
	CHECK_EQ(0u, unsigned(UIDialog::IsLockInput()));
	// The box, centred on the game's 640x300, in the last frame, and the
	// last frame on the back buffer.
	const int boxX = kGameWidth / 2 - kBoxWidth / 2, boxY = kGameHeight / 2 - kBoxHeight / 2;
	CHECK_EQ(kBoxColour, Pixel(scene.last, boxX, boxY));
	CHECK_EQ(kBoxColour, Pixel(scene.last, boxX + kBoxWidth - 1, boxY + kBoxHeight - 1));
	CHECK_EQ(Patterned(boxX - 1, boxY), Pixel(scene.last, boxX - 1, boxY));
	CHECK_EQ(Patterned(boxX + kBoxWidth, boxY), Pixel(scene.last, boxX + kBoxWidth, boxY));
	CHECK_EQ(kBoxColour, Pixel(scene.back, boxX, boxY));
	CHECK_EQ(Patterned(kGameWidth - 1, kGameHeight - 1), Pixel(scene.back, kGameWidth - 1, kGameHeight - 1));
	CHECK_EQ(kBack, Pixel(scene.back, kGameWidth, kGameHeight - 1));
	// Not quitting, whatever the login.
	CHECK_EQ(int(MODE_NULL), int(g_Mode));
	CHECK(scene.connection.Sent().empty());
}

TEST(GameMain2Disconnected, WithoutTheBoxSpritesNothingIsDrawnButTheRestIsDone)
{
	Scene scene;
	std::remove(kIndexFile);
	UpdateDisconnected();
	CHECK_SAME(std::string("release socket;gamma;flip;show;move@7,9;pointer;flip;"), calls);
	CHECK_EQ(Patterned(kGameWidth / 2, kGameHeight / 2), Pixel(scene.last, kGameWidth / 2, kGameHeight / 2));
	CHECK_EQ(Patterned(kGameWidth / 2, kGameHeight / 2), Pixel(scene.back, kGameWidth / 2, kGameHeight / 2));
}

TEST(GameMain2Disconnected, WaveMusicWithTheAudioDownIsLeftAlone)
{
	Scene scene;
	scene.option.PlayWaveMusic = TRUE;
	scene.Playing();
	UpdateDisconnected();
	CHECK_SAME(std::string("loaded|play"), Scene::Music());
}

TEST(GameMain2Disconnected, ActiveTheBoxWaitsForOneOfSevenKeys)
{
	for (DWORD key : {DWORD(DIK_ESCAPE), DWORD(DIK_NUMPADENTER), DWORD(DIK_RETURN), DWORD(DIK_LMENU), DWORD(DIK_RMENU),
		DWORD(DIK_TAB), DWORD(DIK_SPACE)})
	{
		Scene scene;
		g_bActiveGame = TRUE;
		Hold(key);
		UpdateDisconnected();
		// The key is read before the first frame is drawn: the box is shown
		// once, on the back buffer.
		CHECK_SAME(std::string("release socket;gamma;flip;"), calls);
		CHECK_EQ(kBoxColour, Pixel(scene.back, kGameWidth / 2 - kBoxWidth / 2, kGameHeight / 2 - kBoxHeight / 2));
		CHECK_EQ(int(MODE_NULL), int(g_Mode));
		CHECK(scene.connection.Sent().empty());
	}
}

TEST(GameMain2Disconnected, AWebLoginQuitsWhenTheBoxIsClosed)
{
	Scene scene;
	g_bActiveGame = TRUE;
	g_pUserInformation->IsAutoLogIn = true;
	Hold(DIK_RETURN);
	CGLogout logout;
	UpdateDisconnected();
	CHECK_SAME("release socket;gamma;flip;" + Send(logout), calls);
	CHECK_EQ(int(MODE_QUIT), int(g_Mode));
	CHECK(g_pUpdate == NULL);
	CHECK(scene.connection.Sent() == Encoded({&logout}));
}

TEST(GameMain2Disconnected, InactiveAWebLoginDoesNotQuit)
{
	Scene scene;
	g_pUserInformation->IsAutoLogIn = true;
	UpdateDisconnected();
	CHECK_EQ(int(MODE_NULL), int(g_Mode));
	CHECK(scene.connection.Sent().empty());
}

#ifndef PLATFORM_WINDOWS
// The close button: 111, 69 from the box's corner, 198x20, the cursor
// strictly inside. Off Windows only: the cursor is SDL's (window (0, 0),
// mapped into the frame by the present geometry); Windows reads the real
// one.
namespace {
const int kBoxX = kGameWidth / 2 - kBoxWidth / 2, kBoxY = kGameHeight / 2 - kBoxHeight / 2;
const int kCloseX = kBoxX + 111, kCloseY = kBoxY + 69;
} // namespace

TEST(GameMain2Disconnected, AnyOtherKeyIsNotEnough)
{
	Scene scene;
	g_bActiveGame = TRUE;
	Hold(DIK_A);
	// The second frame presses Escape.
	onShow = [](int frame) { if (frame == 2) Hold(DIK_ESCAPE); };
	UpdateDisconnected();
	const std::string frame = "show;move@0,0;pointer;flip;";
	CHECK_SAME("release socket;gamma;flip;" + frame + frame, calls);
}

TEST(GameMain2Disconnected, AClickOnTheCloseButtonClosesTheBox)
{
	Scene scene;
	g_bActiveGame = TRUE;
	CursorAt(kCloseX + 40, kCloseY + 10);
	LeftButton(true);
	// The first frame draws the button pushed; the release comes after.
	onShow = [](int frame) { if (frame == 1) LeftButton(false); };
	UpdateDisconnected();
	CHECK_SAME("release socket;gamma;flip;show;move@" + N(kCloseX + 40) + "," + N(kCloseY + 10) + ";pointer;flip;", calls);
	// The pushed button on the last frame, and so on the back buffer.
	CHECK_EQ(kPushedColour, Pixel(scene.last, kCloseX, kCloseY));
	CHECK_EQ(kPushedColour, Pixel(scene.last, kCloseX + kButtonWidth - 1, kCloseY));
	CHECK_EQ(Patterned(kCloseX + kButtonWidth, kCloseY), Pixel(scene.last, kCloseX + kButtonWidth, kCloseY));
	CHECK_EQ(kPushedColour, Pixel(scene.back, kCloseX, kCloseY));
	CHECK_EQ(int(MODE_NULL), int(g_Mode));
}

TEST(GameMain2Disconnected, TheFocusedCloseButtonNeverShows)
{
	Scene scene;
	g_bActiveGame = TRUE;
	CursorAt(kCloseX + 1, kCloseY + 1);
	onShow = [](int frame) { if (frame == 1) Hold(DIK_ESCAPE); };
	UpdateDisconnected();
	CHECK_SAME("release socket;gamma;flip;show;move@" + N(kCloseX + 1) + "," + N(kCloseY + 1) + ";pointer;flip;", calls);
	// Recorded defect (GameMain.cpp:3195 and 3223): the focused button is
	// drawn on the back buffer, then the last frame is copied over the back
	// buffer, so the highlight is never seen.
	CHECK_EQ(Patterned(kCloseX, kCloseY), Pixel(scene.back, kCloseX, kCloseY));
	CHECK_EQ(Patterned(kCloseX, kCloseY), Pixel(scene.last, kCloseX, kCloseY));
}

TEST(GameMain2Disconnected, TheButtonsEdgesAreOutside)
{
	// A press and release on the edge (the cursor on the button's left or
	// top line, or one past its right or bottom) does not close the box;
	// Escape in the second frame does.
	struct Case { int x, y; };
	for (const Case c : {Case{kCloseX, kCloseY + 10}, Case{kCloseX + 40, kCloseY}, Case{kCloseX + 198, kCloseY + 10}, Case{kCloseX + 40, kCloseY + 20}})
	{
		Scene scene;
		g_bActiveGame = TRUE;
		CursorAt(c.x, c.y);
		LeftButton(true);
		onShow = [](int frame) { if (frame == 1) LeftButton(false); if (frame == 2) Hold(DIK_ESCAPE); };
		UpdateDisconnected();
		const std::string frame = "show;move@" + N(c.x) + "," + N(c.y) + ";pointer;flip;";
		CHECK_SAME("release socket;gamma;flip;" + frame + frame, calls);
		CHECK_EQ(Patterned(kCloseX, kCloseY), Pixel(scene.last, kCloseX, kCloseY));
	}
}

TEST(GameMain2Disconnected, AReleaseOffTheButtonDoesNotClose)
{
	Scene scene;
	g_bActiveGame = TRUE;
	CursorAt(kCloseX + 40, kCloseY + 10);
	LeftButton(true);
	// Pressed on the button; released off it; Escape.
	onShow = [](int frame) {
		if (frame == 1) { CursorAt(kCloseX - 10, kCloseY + 10); LeftButton(false); }
		if (frame == 2) Hold(DIK_ESCAPE);
	};
	UpdateDisconnected();
	CHECK_SAME("release socket;gamma;flip;show;move@" + N(kCloseX + 40) + "," + N(kCloseY + 10) + ";pointer;flip;show;move@"
		+ N(kCloseX - 10) + "," + N(kCloseY + 10) + ";pointer;flip;", calls);
}
TEST(GameMain2Disconnected, APressOffTheButtonReleasedOnItDoesNotClose)
{
	Scene scene;
	g_bActiveGame = TRUE;
	// Pressed on the button's left line (outside); released inside; Escape.
	CursorAt(kCloseX, kCloseY + 10);
	LeftButton(true);
	onShow = [](int frame) {
		if (frame == 1) { CursorAt(kCloseX + 40, kCloseY + 10); LeftButton(false); }
		if (frame == 2) Hold(DIK_ESCAPE);
	};
	UpdateDisconnected();
	CHECK_SAME("release socket;gamma;flip;show;move@" + N(kCloseX) + "," + N(kCloseY + 10) + ";pointer;flip;show;move@"
		+ N(kCloseX + 40) + "," + N(kCloseY + 10) + ";pointer;flip;", calls);
	// Never drawn pushed.
	CHECK_EQ(Patterned(kCloseX, kCloseY), Pixel(scene.last, kCloseX, kCloseY));
}

TEST(GameMain2Disconnected, AReleaseOffTheButtonForgetsThePress)
{
	Scene scene;
	g_bActiveGame = TRUE;
	// Pressed on the button, released off it; pressed off it, released on
	// it: the second release does not close the box either. Escape.
	CursorAt(kCloseX + 40, kCloseY + 10);
	LeftButton(true);
	onShow = [](int frame) {
		if (frame == 1) { CursorAt(kCloseX - 10, kCloseY + 10); LeftButton(false); }
		if (frame == 2) LeftButton(true);
		if (frame == 3) { CursorAt(kCloseX + 40, kCloseY + 10); LeftButton(false); }
		if (frame == 4) Hold(DIK_ESCAPE);
	};
	UpdateDisconnected();
	const std::string on = "show;move@" + N(kCloseX + 40) + "," + N(kCloseY + 10) + ";pointer;flip;";
	const std::string off = "show;move@" + N(kCloseX - 10) + "," + N(kCloseY + 10) + ";pointer;flip;";
	CHECK_SAME("release socket;gamma;flip;" + on + off + off + on, calls);
}
#endif

//======================================================================
// UpdateInput / UpdateMouse
//======================================================================
TEST(GameMain2Input, TheInputIsReadWhenThereIsOne)
{
	Scene scene;
	scene.input.SetKeyboardEventReceiver([](CSDLInput::E_KEYBOARD_EVENT event, DWORD key) {
		Call((event == CSDLInput::KEYDOWN ? "key" : "keyup") + N(key));
	});
	Hold(DIK_F5);
	UpdateInput();
	CHECK_SAME("key" + N(DIK_F5) + ";", calls);
	CHECK(scene.input.KeyDown(DIK_F5));
	g_pSDLInput = NULL;
	calls.clear();
	UpdateInput();
	CHECK_SAME(std::string(""), calls);
}

TEST(GameMain2Mouse, AOnePixelFrameHoldsTheCursorAtItsCorner)
{
	// Wherever the cursor is, on every platform.
	Scene scene;
	g_GameRect = {0, 0, 1, 1};
	UpdateMouse();
	CHECK_EQ(0, g_x);
	CHECK_EQ(0, g_y);
}

#ifndef PLATFORM_WINDOWS
TEST(GameMain2Mouse, TheCursorIsMappedIntoTheFrameAndHeldInsideIt)
{
	Scene scene;
	// Nothing presented: the window's (0, 0) as it is.
	UpdateMouse();
	CHECK_EQ(0, g_x);
	CHECK_EQ(0, g_y);
	// Mapped through the letterbox.
	CursorAt(200, 100);
	g_GameRect = {0, 0, 640, 480};
	UpdateMouse();
	CHECK_EQ(200, g_x);
	CHECK_EQ(100, g_y);
	// Inside up to the last column and row; at the edge, held on it.
	g_GameRect = {0, 0, 201, 101};
	UpdateMouse();
	CHECK_EQ(200, g_x);
	CHECK_EQ(100, g_y);
	g_GameRect = {0, 0, 200, 100};
	UpdateMouse();
	CHECK_EQ(199, g_x);
	CHECK_EQ(99, g_y);
	g_GameRect = {0, 0, 150, 480};
	UpdateMouse();
	CHECK_EQ(149, g_x);
	CHECK_EQ(100, g_y);
}

TEST(GameMain2Mouse, AnEmptyFramePutsTheCursorOutside)
{
	Scene scene;
	g_GameRect = {0, 0, 0, 0};
	UpdateMouse();
	// Recorded defect (inventory bug 9, GameMain.cpp:2940-2943): with no
	// frame the clamp yields -1, -1, outside every rect.
	CHECK_EQ(-1, g_x);
	CHECK_EQ(-1, g_y);
}
#endif

//======================================================================
// The cheat tools' window titles
//======================================================================
TEST(GameMain2InvalidProcess, ContainsIgnoreCaseFindsTheNeedleAnywhereInAnyCase)
{
	CHECK(!ContainsIgnoreCase(NULL, "wpe"));
	CHECK(!ContainsIgnoreCase("wpe", NULL));
	CHECK(!ContainsIgnoreCase(NULL, NULL));
	// An empty needle is everywhere, an empty title has nothing else.
	CHECK(ContainsIgnoreCase("", ""));
	CHECK(ContainsIgnoreCase("anything", ""));
	CHECK(!ContainsIgnoreCase("", "wpe"));
	CHECK(ContainsIgnoreCase("wpe", "wpe"));
	CHECK(ContainsIgnoreCase("WPE Pro", "wpe"));
	CHECK(ContainsIgnoreCase("my wpe pro", "WPE"));
	CHECK(ContainsIgnoreCase("Speed Accelerator", "accelerat"));
	CHECK(ContainsIgnoreCase("tool: WinSock Expert", "winsock expert"));
	CHECK(!ContainsIgnoreCase("gameice", "gamehack"));
	// A title shorter than the needle, and a near miss at the end.
	CHECK(!ContainsIgnoreCase("wp", "wpe"));
	CHECK(!ContainsIgnoreCase("tool wp", "wpe"));
	// Bytes past ASCII compare as they are.
	CHECK(ContainsIgnoreCase("x\xe5\x8f\x98\xe9\x80\x9fy", "\xe5\x8f\x98\xe9\x80\x9f"));
	CHECK(!ContainsIgnoreCase("x\xe5\x8f\x98y", "\xe5\x8f\x98\xe9\x80\x9f"));
}

TEST(GameMain2InvalidProcess, TheCheatToolsAreKnownByTheseTitles)
{
	const char* const titles[20] = {
		"\xe5\x8f\x98\xe9\x80\x9f",
		"\xe5\x8a\xa0\xe9\x80\x9f",
		"fpe",
		"\xe9\x87\x91\xe5\xb1\xb1\xe6\xb8\xb8\xe4\xbe\xa0",
		"game master",
		"gameice",
		"gamehack",
		"\xe6\xb8\xb8\xe6\x88\x8f\xe4\xbf\xae\xe6\x94\xb9\xe5\xa4\xa7\xe5\xb8\x88",
		"\xe4\xb8\x9c\xe6\x96\xb9\xe4\xb8\x8d\xe8\xb4\xa5",
		"accelerat",
		"wpe",
		"winsock expert",
		"a speeder",
		"\xe5\xbd\xa9\xe4\xba\x91",
		"\xe9\x9b\xb6\xe7\x82\xb9",
		"\xe8\x93\x9d\xe6\x9c\x88",
		"\xe6\x97\xb6\xe7\xa9\xba\xe6\xb8\xb8\xe4\xbe\xa0",
		"\xe6\x97\xb6\xe7\xa9\xba\xe6\xb8\xb8\xe4\xbe\xa0\xe6\xa0\x87\xe5\x87\x86\xe7\x89\x88",
		"\xe8\x93\x9d\xe2\x80\xbb\xe6\x9c\x88",
		"flyodbg",
	};
	for (int i = 0; i < 20; ++i)
		CHECK_SAME(std::string(titles[i]), g_strBadProcessList[i]);
}

TEST(GameMain2InvalidProcess, WithTheCheckOffNothingIsLookedFor)
{
	// Deterministic on every platform: with the check off even Windows
	// walks no window.
	Scene scene;
	g_bCheckHack = false;
	CHECK(CheckInvalidProcess());
	CHECK_EQ(int(MODE_NULL), int(g_Mode));
	CHECK_EQ(int(MODE_WAIT_PCLIST), int(g_ModeNext));
	CHECK_EQ(FALSE, g_bNeedUpdate);
	CHECK_SAME(std::string(""), calls);
}

#ifndef PLATFORM_WINDOWS
TEST(GameMain2InvalidProcess, OffWindowsNothingIsInvalid)
{
	Scene scene;
	CHECK(g_bCheckHack);
	CHECK(CheckInvalidProcess());
	CHECK_EQ(int(MODE_NULL), int(g_Mode));
	CHECK_EQ(int(MODE_WAIT_PCLIST), int(g_ModeNext));
	CHECK_EQ(FALSE, g_bNeedUpdate);
	CHECK(g_bRunning);
	CHECK_SAME(std::string(""), calls);
}
#endif
