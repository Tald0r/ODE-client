#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "packet_stream_access.h"
#include "CGameUpdate.h"
#include "ClientMode.h"
#include "ClientPlayer.h"
#include "GameSocket.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketOutputStream.h"
#include "SocketEncryptOutputStream.h"
#include "FrameClock.h"
#include "InputState.h"
#include "MonotonicClock.h"
#include "MEventQueue.h"
#include "MEventManager.h"
#include "MParty.h"
#include "MPlayer.h"
#include "MZone.h"
#include "MSector.h"
#include "MCreature.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MActionInfoTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "EffectSpriteTypeDef.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "MZoneTable.h"
#include "MWarManager.h"
#include "GuildWarInfo.h"
#include "MSkillManager.h"
#include "MGameStringTable.h"
#include "GameMessages.h"
#include "../basic/CMessageArray.h"
#include "MItem.h"
#include "MEffect.h"
#include "MObjectSelector.h"
#include "MJusticeAttackManager.h"
#include "MInventory.h"
#include "MSlayerGear.h"
#include "MOustersGear.h"
#include "TempInformation.h"
#include "ZoneFlags.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"
#include "MTopView.h"
#include "MRequestMode.h"
#include "WorldTileGeometry.h"
#include "SkillDef.h"
#include "SoundDef.h"
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
#include "SpriteLibBackend.h"
#include "CDirectInput.h"
#include "DXLibBackend.h"
#include "Cpackets/CGSkillToSelf.h"
#include "Cpackets/CGGetOffMotorCycle.h"
#include "Cpackets/CGUnburrow.h"
#include <SDL.h>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

// CGameUpdate.cpp's own (no header declares them) and VS_UI's.
extern bool	g_bPreviousMove;
extern int	g_CheckErrorTime;
extern bool	gbl_mine_progress;
extern bool	gbl_global_empty_move;
extern int	g_DrawCamGapX;
extern int	g_DrawCamGapY;
// MItemUse.cpp's, reached through MItemHost.
MItem*	NewItemOfClass(ITEM_CLASS itemClass);

// The game's frame loop (task 4.261 moved Client/CGameUpdate.cpp into
// gameclient), the input side:
// - Init clears the remembered walk (g_bPreviousMove), hands the mouse and
//   the keyboard to the loop's two receivers and starts SDL's text input.
// - The mouse receiver hands the wheel to the UI at the cursor (g_x, g_y,
//   not the event's point), ends the mine installing on a right release
//   and leaves the rest: a left press reaches the UI from ProcessInput's
//   pulse, never from here. An event that denies the mouse drops it all.
// - The keyboard receiver gives a key press to the UI as the wait screens'
//   does - the editing keys first as a WM_KEYDOWN of their virtual key,
//   then every key as its scan code - except scan code 0xCC; Ctrl+F (either
//   Ctrl) then flips the FPS display and ends the ousters' ending event.
//   A release, nothing; an event that denies the keyboard drops it all.
// - WindowToGamePoint maps the cursor the host gives from the window's
//   client area into the game frame through SpriteLib's letterbox mapping
//   (ProcessInput hands the result to the UI as the move).
// - ProcessInputRButtonDown (the right press in the world) holds the right
//   button and clears the view's picked cell; on Windows five speed-check
//   errors quit the game there; then (a war enemy is always forced) it
//   stops at Dragon Eyes, at a turret not standing still, takes a
//   standing turret out of its shape, fires a turret's first shot, gets
//   off a sylph (on unsafe ground) or a motorcycle (no flag corpse near),
//   surfaces from under the ground, or uses the chosen skill: on itself,
//   on the cell under the cursor, or on the creature clicked (on it, on
//   itself, or on its cell), repeating it.
//
// The UI is the real one with a window of the test's over it, which
// records what the UI hands it; an accelerator bound to the key under test
// records the scan code. The player stands in a real zone, its packets go
// out on a real connection and are compared byte for byte, and the view
// is a real MTopView.
//
// Not run (undefined behaviour):
// - ProcessInputRButtonDown on an item, an effect or any object that is
//   not a creature: CGameUpdate.cpp:327 casts every picked object to
//   MCreature* (C-style, so a static downcast to the wrong type) and hands
//   it to IsWarEnemy, which reads creature fields through it in a siege or
//   guild war; the item branch (530-568, Wild Wolf on a corpse, a zone
//   skill at the item's cell) is only reachable through that cast.
// - ProcessInputRButtonDown with no view (g_pTopView NULL) on any path
//   that reaches the skill block: 445, 453, 464 and 522 call the view
//   unchecked while 310 checks it.
// - The receivers with no event manager, Init with no input
//   (g_pSDLInput), ProcessInputRButtonDown with no player, no object
//   selector (an object), no socket (a packet) or no skill info table (a
//   sylph): each is read unchecked.
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

POINT cursor = { 0, 0 };	// where the host says the cursor is in the window
const CGameUpdateHost host{
	.UpdateMouse = []() { Call("mouse"); },
	.UpdateInput = []() { Call("input"); },
	.UpdateSocketInput = []() { Call("in"); return true; },
	.UpdateSocketOutput = []() { Call("out"); return true; },
	.SetMode = [](CLIENT_MODE mode) { Call("mode" + N(static_cast<int>(mode))); },
	.GetCursorPoint = [](POINT& pt) { pt = cursor; Call("cursor"); },
};

struct Probe : CGameUpdate
{
	Probe() : CGameUpdate(&host) {}
	using CGameUpdate::ProcessInput;
	using CGameUpdate::ProcessInputRButtonDown;
	using CGameUpdate::DXMouseEvent;
	using CGameUpdate::DXKeyboardEvent;
};

// The event queue's gamma, which every removal resets.
const MEventHost eventHost = []() {
	MEventHost h{};
	h.SetAddGammaRamp = [](WORD red, WORD green, WORD blue) { Call("gamma" + N(red) + "," + N(green) + "," + N(blue)); };
	return h;
}();

// An event of the id that carries the flag.
void AddEvent(MEventQueue& events, EVENT_ID id, DWORD flag)
{
	MEvent event;
	event.eventID = id;
	event.eventFlag = flag;
	events.AddEvent(event);
}

//----------------------------------------------------------------------
// The input side: the real UI with a recording window
//----------------------------------------------------------------------
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
	return "mouse" + N(message);
}

std::string At(int x, int y) { return "@" + N(x) + "," + N(y); }

// A window over the whole screen that takes the mouse and the keyboard.
struct UiWindow : Window
{
	UiWindow() : Window(0, 0, 800, 600) {}
	bool IsPixel(int, int) override { return true; }
	bool MouseControl(UINT message, int x, int y) override { Call(Message(message) + At(x, y)); return true; }
	void KeyboardControl(UINT message, UINT key, long extra) override
	{
		Call((message == WM_KEYDOWN ? std::string("keydown") : "key" + N(message) + ":") + N(key) + "," + N(extra));
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

// The UI's own event checks see nothing: only the loop's queue denies.
bool NoEventFlag(std::uint32_t) { return false; }
const UiRuntime::Host uiRuntime{ .EventFlagActive = NoEventFlag };

// Everything the receivers, the UI and the loop's input read, saved and
// given back after.
struct InputSaved
{
	int x = g_x, y = g_y;
	BOOL left = g_bLButtonDown, right = g_bRButtonDown, centre = g_bCButtonDown;
	bool previousMove = g_bPreviousMove;
	bool mineProgress = gbl_mine_progress;
	bool emptyMove = gbl_global_empty_move;
	CSDLInput* input = g_pSDLInput;
	WindowManager* windows = gpC_window_manager;
	PI_ProcessRunner* runner = gpC_process_runner;
	Base* base = gpC_base;
	C_VS_UI_MOUSE_POINTER* pointer = gpC_mouse_pointer;
	KeyAccelerator* keys = g_pKeyAccelerator;
	AcceleratorManager* accelerators = g_pAcceleratorManager;
	UserOption* option = g_pUserOption;
	MParty* party = g_pParty;
	CI* ime = gC_ci;
	MEventQueue* events = g_pEventManager;
	const UiRuntime::Host* runtime = UiRuntime::SetHost(&uiRuntime);
	~InputSaved()
	{
		UiRuntime::SetHost(runtime);
		g_pEventManager = events;
		gC_ci = ime;
		g_pParty = party;
		g_pUserOption = option;
		g_pAcceleratorManager = accelerators;
		g_pKeyAccelerator = keys;
		gpC_mouse_pointer = pointer;
		gpC_base = base;
		gpC_process_runner = runner;
		gpC_window_manager = windows;
		g_pSDLInput = input;
		gbl_global_empty_move = emptyMove;
		gbl_mine_progress = mineProgress;
		g_bPreviousMove = previousMove;
		g_bCButtonDown = centre;
		g_bRButtonDown = right;
		g_bLButtonDown = left;
		g_y = y;
		g_x = x;
	}
};

// The loop in the game with nothing held, the cursor at (7, 9), the FPS
// display off, no event queued and no mine being installed.
struct InputScene
{
	InputSaved saved;
	PI_ProcessRunner runner;
	WindowManager windows;
	Base base;
	C_VS_UI_MOUSE_POINTER pointer;
	CSDLInput input;
	KeyAccelerator keys;
	Accelerators accelerators;
	UserOption option;
	CI_KOREAN ime;	// a window taking the keyboard ends the IME's composition
	MEventQueue events{ &eventHost };
	std::unique_ptr<UiWindow> window;
	Probe update;

	InputScene()
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
		option.DrawFPS = FALSE;
		g_pUserOption = &option;
		g_pParty = NULL;
		gbl_global_empty_move = false;
		gC_ci = &ime;
		g_pEventManager = &events;
		window = std::make_unique<UiWindow>();
		window->AttrKeyboardControl(true);
		window->Start();
		windows.AppearWindow(window.get());
		g_x = 7;
		g_y = 9;
		g_bLButtonDown = g_bRButtonDown = g_bCButtonDown = FALSE;
		g_bPreviousMove = false;
		gbl_mine_progress = false;
		cursor.x = cursor.y = 0;
		calls.clear();
	}
	~InputScene()
	{
		windows.DisappearWindow(window.get());
		window.reset();
		dxlib_input_virtual_reset();
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

	// The keys held as the input reads them (the backend is started for
	// it); nothing receives their presses unless Init ran.
	void Hold(DWORD key)
	{
		if (!heldStarted) CHECK(input.Init(nullptr, nullptr));
		heldStarted = true;
		dxlib_input_virtual_key(int(key), 1);
		input.UpdateInput();
		CHECK(input.KeyDown(key));
	}
	void ReleaseAll()
	{
		dxlib_input_virtual_reset();
		input.UpdateInput();
	}
	bool heldStarted = false;
};

// The game frame letterboxed into the window, as a present records it,
// and back to no present (the identity) after.
struct Letterbox
{
	explicit Letterbox(bool on)
	{
		if (on)
			// An 800x600 frame in a 1600x1000 window: 1333x999 at x = 133.
			spritectl_set_present_geometry(1600, 1000, 1600, 1000, 133, 0, 1333, 999, 800, 600);
		else
			spritectl_set_present_geometry(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}
	~Letterbox() { spritectl_set_present_geometry(0, 0, 0, 0, 0, 0, 0, 0, 0, 0); }
};

//----------------------------------------------------------------------
// The world side: a player in a zone, a view and a connection
//----------------------------------------------------------------------
// The game connection: what is sent is recorded in the calls and kept,
// serialised, in its output stream.
struct Wire : ClientPlayer
{
	Wire() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~Wire() { setPlayerStatus(CPS_END_SESSION); }
	void sendPacket(Packet* pPacket) override
	{
		Call("send" + N(pPacket->getPacketID()));
		Player::sendPacket(pPacket);
	}
	std::vector<unsigned char> Sent() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};

// The packets as the connection's stream (an encrypting one, its code
// 0) sends them; each header carries the stream's running sequence number.
std::vector<unsigned char> Encoded(const std::vector<const Packet*>& packets)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketEncryptOutputStream encoded(&out);
	for (const Packet* pPacket : packets) encoded.write(pPacket);
	return SocketOutputStreamTestAccess::Bytes(encoded);
}

std::string Send(const Packet& packet) { return "send" + N(packet.getPacketID()) + ";"; }

const ZoneState::Host zoneHost = []() {
	ZoneState::Host h{};
	h.Exists = []() { return g_pZone != NULL; };
	h.GetID = []() { return g_pZone != NULL ? int(g_pZone->GetID()) : int(ZONEID_NULL); };
	h.GetCreature = [](TYPE_OBJECTID id) { return g_pZone->GetCreature(id); };
	h.GetWidth = []() { return g_pZone != NULL ? int(g_pZone->GetWidth()) : 0; };
	h.GetHeight = []() { return g_pZone != NULL ? int(g_pZone->GetHeight()) : 0; };
	h.GetSector = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) -> const MSector* { return &g_pZone->GetSector(x, y); };
	h.GetFakeCreature = [](TYPE_OBJECTID id) { return g_pZone != NULL ? g_pZone->GetFakeCreature(id) : NULL; };
	h.CanMove = [](int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { return g_pZone->CanMove(BYTE(moveType), x, y); };
	h.CanMoveIn = [](MZone* pZone, int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { return pZone->CanMove(BYTE(moveType), x, y); };
	h.GetWidthIn = [](MZone* pZone) { return int(pZone->GetWidth()); };
	h.GetHeightIn = [](MZone* pZone) { return int(pZone->GetHeight()); };
	h.GetCreatureIn = [](MZone* pZone, TYPE_OBJECTID id) { return pZone->GetCreature(id); };
	return h;
}();

const PlayerState::Host playerHost = []() {
	PlayerState::Host h{};
	h.Creature = []() -> MCreature* { return g_pPlayer; };
	h.Status = []() -> MStatus* { return g_pPlayer; };
	h.GetID = []() { return g_pPlayer != NULL ? g_pPlayer->GetID() : TYPE_OBJECTID(OBJECTID_NULL); };
	h.HasEffectStatus = [](EFFECTSTATUS status) { return g_pPlayer->HasEffectStatus(status); };
	h.GetItemCheckBuffer = []() { return g_pPlayer->GetItemCheckBuffer(); };
	h.ClearItemCheckBuffer = []() { g_pPlayer->ClearItemCheckBuffer(); };
	return h;
}();

// The world as the player reaches it: the sounds and the view's
// selection recorded.
const WorldEvents::Host worldHost = []() {
	WorldEvents::Host h{};
	h.ViewExists = []() { return true; };
	h.GetFirstSector = []() { POINT p = { 0, 0 }; return p; };
	h.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION, TYPE_SECTORPOSITION, int, int, TYPE_OBJECTID,
		TYPE_SECTORPOSITION, TYPE_SECTORPOSITION, int, std::uint32_t, MActionResult* pResult, bool, int, int) {
		Call("skill" + N(actionInfo));
		delete pResult;
	};
	h.PlaySound = [](TYPE_SOUNDID soundID) { Call("sound" + N(soundID)); };
	h.PlaySoundAt = [](TYPE_SOUNDID soundID, bool, int, int) { Call("sound at" + N(soundID)); };
	h.SetSelectedSectorNULL = []() { Call("unselect sector"); };
	h.SetSelectedNULL = []() { Call("unselect"); };
	h.SetSelectedCreature = [](TYPE_OBJECTID id) { Call("select creature" + N(id)); };
	h.GetViewRequestMode = []() { return int(MRequestMode::REQUEST_NULL); };
	h.IsViewRequestMode = []() { return false; };
	return h;
}();

const CreatureState::Host creatureHost = []() {
	CreatureState::Host h{};
	h.GetAddonItemClass = [](MCreature*, int, ITEM_CLASS* pItemClass) { *pItemClass = ITEM_CLASS_NULL; return false; };
	h.ObjectOf = [](const MCreature* pCreature) -> MObject* { return const_cast<MCreature*>(pCreature); };
	h.CreatureOf = [](MObject* pObject) { return static_cast<MCreature*>(pObject); };
	return h;
}();

const MEffectHost effectHost = []() {
	MEffectHost h{};
	h.CurrentFrame = []() -> DWORD { return g_CurrentFrame; };
	return h;
}();

// The skills' reuse times run on the frame clock.
const MItemHost itemHost = []() {
	MItemHost h{};
	h.NewItem = NewItemOfClass;
	h.DropFrameCount = [](TYPE_FRAMEID) { return 4; };
	h.pCurrentTime = &g_FrameNow;
	return h;
}();

// Whom the player may attack; a zone where all fight all makes every
// creature a war enemy. Each time the selector asks whether there is a
// zone (IsWarEnemy's first question) it is recorded.
bool warForAll = false;
const MObjectSelectorHost selectorHost = []() {
	MObjectSelectorHost h{};
	h.ForceAttack = []() { return false; };
	h.PlayerHasEffectStatus = [](EFFECTSTATUS status) { return g_pPlayer->HasEffectStatus(status); };
	h.PlayerCanAttackTribe = [](MCreature* pCreature) { return g_pPlayer->CanAttackTribe(pCreature); };
	h.PlayerCanAttackGuild = [](MCreature* pCreature) { return g_pPlayer->CanAttackGuild(pCreature); };
	h.IsNonPK = []() { return false; };
	h.IsNPC = [](MCreature* pCreature) { return pCreature->IsNPC(); };
	h.GetName = [](MCreature* pCreature) { return pCreature->GetName(); };
	h.HasPlayerOnlySprite = [](MCreature*) { return false; };
	h.HasZone = []() { Call("war?"); return true; };
	h.GetZoneID = []() { return 0; };
	h.GetZonePKType = []() { return warForAll ? PK_TYPE_ALL : PK_TYPE_NULL; };
	h.JusticeAttackManager = []() { return g_pJusticeAttackManager; };
	h.WarManager = []() { return g_pWarManager; };
	return h;
}();

// The player, with what it keeps to itself open to the tests.
struct PlayerProbe : MPlayer
{
	using MPlayer::m_fTrace;
	using MPlayer::m_TraceX;
	using MPlayer::m_TraceY;
	using MPlayer::m_nNextUsedActionInfo;
	using MPlayer::m_nUsedActionInfo;
	using MCreature::m_bEffectStatus;
	using MCreature::m_MoveType;
	using MCreature::m_Action;
	using MCreature::m_ActionCount;
	using MCreature::m_ActionCountMax;
	void Set(EFFECTSTATUS status, bool on = true) { m_bEffectStatus[status] = on; }
	// Mid-swing (on a motorcycle too): an attack's first frame of five.
	void Busy() { m_Action = ACTION_ATTACK; m_ActionCount = 0; m_ActionCountMax = 5; }
};

// Creature types: the player's (a slayer), a slayer only players are, a
// vampire monster and an NPC.
const int kSlayer = 5, kSlayerPlayer = 6, kVampire = 7, kNPC = 8;
const TYPE_OBJECTID kPlayerID = 77;

// What else the world's globals were, given back after.
struct WorldSaved
{
	CREATURE_TABLE* creatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE* sprites = g_pCreatureSpriteTable;
	MActionInfoTable* actions = g_pActionInfoTable;
	EFFECTSPRITETYPE_TABLE* effectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* effectStatuses = g_pEffectStatusTable;
	RankBonusTable* rankBonuses = g_pRankBonusTable;
	MonsterNameTable* monsterNames = g_pMonsterNameTable;
	MLevelNameTable* levelNames = g_pLevelNameTable;
	MEventQueue* events = g_pEventManager;
	MWarManager* wars = g_pWarManager;
	MParty* party = g_pParty;
	MSkillSet* skills = g_pSkillAvailable;
	CMessageArray* system = g_pSystemMessage;
	TempInformation* temp = g_pTempInformation;
	MSkillInfoTable* skillInfo = g_pSkillInfoTable;
	MInventory* inventory = g_pInventory;
	MSlayerGear* slayerGear = g_pSlayerGear;
	MOustersGear* oustersGear = g_pOustersGear;
	MObjectSelector* selector = g_pObjectSelector;
	MJusticeAttackManager* justice = g_pJusticeAttackManager;
	CZoneTable* zoneTable = g_pZoneTable;
	ClientPlayer* socket = g_pSocket;
	MZone* zone = g_pZone;
	MPlayer* player = g_pPlayer;
	MTopView* view = g_pTopView;
	bool zoneSafe = g_bZoneSafe;
	int x = g_x, y = g_y;
	BOOL left = g_bLButtonDown, right = g_bRButtonDown;
	bool previousMove = g_bPreviousMove;
	POINT selectSector = g_SelectSector;
	int checkErrorTime = g_CheckErrorTime;
	int gapX = g_DrawCamGapX, gapY = g_DrawCamGapY;
	DWORD frame = g_CurrentFrame;
	MonotonicClock::TimePoint now = g_FrameNow;
	const ZoneState::Host* zoneHost = ZoneState::SetHost(&::zoneHost);
	const PlayerState::Host* playerHost = PlayerState::SetHost(&::playerHost);
	const WorldEvents::Host* worldHost = WorldEvents::SetHost(&::worldHost);
	const CreatureState::Host* creatureHost = CreatureState::SetHost(&::creatureHost);
	const MEffectHost* effectHost = MEffect::SetHost(&::effectHost);
	~WorldSaved()
	{
		MItem::SetHost(NULL);
		MEffect::SetHost(effectHost);
		CreatureState::SetHost(creatureHost);
		WorldEvents::SetHost(worldHost);
		PlayerState::SetHost(playerHost);
		ZoneState::SetHost(zoneHost);
		g_FrameNow = now;
		g_CurrentFrame = frame;
		g_DrawCamGapY = gapY;
		g_DrawCamGapX = gapX;
		g_CheckErrorTime = checkErrorTime;
		g_SelectSector = selectSector;
		g_bPreviousMove = previousMove;
		g_bRButtonDown = right;
		g_bLButtonDown = left;
		g_y = y;
		g_x = x;
		g_bZoneSafe = zoneSafe;
		g_pTopView = view;
		g_pPlayer = player;
		g_pZone = zone;
		g_pSocket = socket;
		g_pZoneTable = zoneTable;
		g_pJusticeAttackManager = justice;
		g_pObjectSelector = selector;
		g_pOustersGear = oustersGear;
		g_pSlayerGear = slayerGear;
		g_pInventory = inventory;
		g_pSkillInfoTable = skillInfo;
		g_pTempInformation = temp;
		g_pSystemMessage = system;
		g_pSkillAvailable = skills;
		g_pParty = party;
		g_pWarManager = wars;
		g_pEventManager = events;
		g_pLevelNameTable = levelNames;
		g_pMonsterNameTable = monsterNames;
		g_pRankBonusTable = rankBonuses;
		g_pEffectStatusTable = effectStatuses;
		g_pEffectSpriteTypeTable = effectSprites;
		g_pActionInfoTable = actions;
		g_pCreatureSpriteTable = sprites;
		g_pCreatureTable = creatureTypes;
	}
};

// A slayer standing at (20, 15) in a 40x30 zone, no skill chosen, the
// cursor at (300, 130) over cell (6, 5) of a view whose screen starts at
// the zone's corner, the right button up, the last walk remembered
// (g_bPreviousMove) and a picked cell marked at (3, 4); g_SelectSector
// (1, 2).
struct World
{
	WorldSaved saved;
	GameModelWorld world;
	CREATURE_TABLE creatureTypes;
	CREATURESPRITE_TABLE sprites;
	MActionInfoTable actions;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	RankBonusTable rankBonuses;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	MEventQueue events;
	MWarManager wars;
	MParty party;
	MSkillSet skills;
	CMessageArray systemLog;
	TempInformation temp;
	MSkillInfoTable skillInfo;
	MInventory inventory;
	MSlayerGear slayerGear;
	MOustersGear oustersGear;
	MObjectSelector selector{ &selectorHost };
	MJusticeAttackManager justice;
	CZoneTable zoneTable;
	Wire wire;
	std::unique_ptr<MZone> zone;
	PlayerProbe* me = nullptr;
	MTopView view;
	Probe update;

	World()
	{
		static const bool moveTables = (MCreature::InitMoveTable(), true);
		(void)moveTables;
		CHECK_EQ(0, spritectl_init());
		calls.clear();
		warForAll = false;
		g_CurrentFrame = 1000;
		g_FrameNow = MonotonicClock::TimePoint() + MonotonicClock::Millis(1000000);
		MItem::SetHost(&itemHost);
		creatureTypes.Init(800);
		g_pCreatureTable = &creatureTypes;
		Tribe(kSlayer, CREATURETRIBE_SLAYER);
		// The flag's (670) and another monster, whose corpses lie about.
		Tribe(670, CREATURETRIBE_VAMPIRE);
		Tribe(671, CREATURETRIBE_VAMPIRE);
		Tribe(kSlayerPlayer, CREATURETRIBE_SLAYER);
		Tribe(kVampire, CREATURETRIBE_VAMPIRE);
		Tribe(kNPC, CREATURETRIBE_NPC);
		sprites.Init(4);
		g_pCreatureSpriteTable = &sprites;
		// Only players wear sprite 1.
		testfw::MutableRow(creatureTypes, kSlayerPlayer).SpriteTypes.Init(1);
		testfw::MutableRow(creatureTypes, kSlayerPlayer).SpriteTypes.Set(0, 1);
		sprites.GetMutable(1)->CreatureType = FLAG_CREATURESPRITE_PLAYER_SLAYER;
		actions.Init(MAX_ACTIONINFO);
		for (int info = 0; info < MAX_ACTIONINFO; ++info)
		{
			actions.GetMutable(info)->Set("", ACTION_STAND, EFFECTSPRITETYPE_NULL, 0, 0);
			actions.GetMutable(info)->SetSoundID(SOUNDID_NULL);
		}
		actions.SetMinResultActionInfo(MIN_RESULT_ACTIONINFO);
		g_pActionInfoTable = &actions;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		g_pRankBonusTable = &rankBonuses;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		g_pEventManager = &events;
		g_pWarManager = &wars;
		g_pParty = &party;
		g_pSkillAvailable = &skills;
		systemLog.Init(3, 512);
		g_pSystemMessage = &systemLog;
		g_pTempInformation = &temp;
		skillInfo.CTypeTable<SKILLINFO_NODE>::Init(MAX_ACTIONINFO);
		g_pSkillInfoTable = &skillInfo;
		inventory.Init(10, 6);
		g_pInventory = &inventory;
		slayerGear.Init();
		g_pSlayerGear = &slayerGear;
		oustersGear.Init();
		g_pOustersGear = &oustersGear;
		g_pObjectSelector = &selector;
		g_pJusticeAttackManager = &justice;
		ZONETABLE_INFO* pInfo = new ZONETABLE_INFO;
		pInfo->ID = 0;
		zoneTable.Add(pInfo);
		g_pZoneTable = &zoneTable;
		g_pSocket = &wire;
		g_bZoneSafe = false;
		zone.reset(new MZone(40, 30));
		g_pZone = zone.get();
		me = new PlayerProbe;
		g_pPlayer = me;
		me->SetZone(g_pZone);
		me->SetCreatureType(kSlayer);
		me->SetID(kPlayerID);
		me->SetPosition(20, 15);
		me->SetServerPosition(20, 15);
		view.m_FirstSector.x = view.m_FirstSector.y = 0;
		view.m_PlusPoint.x = view.m_PlusPoint.y = 0;
		view.m_SelectSector.x = 3;
		view.m_SelectSector.y = 4;
		g_pTopView = &view;
		g_DrawCamGapX = g_DrawCamGapY = 0;
		g_x = 300;
		g_y = 130;
		g_bLButtonDown = g_bRButtonDown = FALSE;
		g_bPreviousMove = true;
		g_SelectSector.x = 1;
		g_SelectSector.y = 2;
		g_CheckErrorTime = 0;
		calls.clear();
	}
	~World()
	{
		delete me;
		g_pPlayer = NULL;
		zone.reset();
		g_pZone = NULL;
		oustersGear.Release();
		slayerGear.Release();
		inventory.Release();
	}
	// A creature type of the tribe, with a count for each action.
	void Tribe(int type, CREATURETRIBE tribe)
	{
		const int actionCount = tribe == CREATURETRIBE_VAMPIRE ? ACTION_MAX_VAMPIRE : (tribe == CREATURETRIBE_OUSTERS ? ACTION_MAX_OUSTERS : ACTION_MAX_SLAYER);
		testfw::MutableRow(creatureTypes, type).SetCreatureTribe(tribe, actionCount);
		testfw::MutableRow(creatureTypes, type).MoveTimes = 8;
		testfw::MutableRow(creatureTypes, type).MoveTimesMotor = 8;
		testfw::MutableRow(creatureTypes, type).MoveRatio = 1;
		for (int action = 0; action < actionCount; ++action)
			testfw::MutableRow(creatureTypes, type).SetActionCount(action, 10 + action);
	}
	// A skill the player has, ready, aimed as the flags say, and chosen.
	void Skill(int id, BYTE target, BYTE range)
	{
		actions.GetMutable(id)->Set("", ACTION_MAGIC, EFFECTSPRITETYPE_NULL, range, target);
		skills.AddSkill(ACTIONINFO(id));
		skillInfo.GetMutable(id)->SetEnable(true);
		skillInfo.GetMutable(id)->minRange = range;
		skillInfo.GetMutable(id)->maxRange = range;
		me->SetSpecialActionInfo(TYPE_ACTIONINFO(id));
	}
	// A creature of the type at the cell, in the zone (which frees it).
	MCreature* Creature(TYPE_OBJECTID id, int type, int x, int y)
	{
		MCreature* pCreature = new MCreature;
		pCreature->SetZone(g_pZone);
		pCreature->SetCreatureType(TYPE_CREATURETYPE(type));
		pCreature->SetID(id);
		pCreature->SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
		pCreature->SetServerPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
		CHECK(zone->AddCreature(pCreature));
		return pCreature;
	}
	// The corpse of a creature of the type, on the cell.
	void Corpse(TYPE_OBJECTID id, int creatureType, int x, int y)
	{
		if ((*g_pItemTable)[ITEM_CLASS_CORPSE].GetSize() == 0)
			g_pItemTable->InitClass(ITEM_CLASS_CORPSE, 1);
		MCorpse* pCorpse = static_cast<MCorpse*>(NewItemOfClass(ITEM_CLASS_CORPSE));
		MCreature* pBody = new MCreature;
		pBody->SetCreatureType(TYPE_CREATURETYPE(creatureType));
		pBody->SetID(id + 10000);
		pCorpse->SetCreature(pBody);
		pCorpse->SetItemType(0);
		pCorpse->SetID(id);
		pCorpse->SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
		CHECK(zone->AddItem(pCorpse));
	}
	// The right press on the object (or nowhere), what it did recorded.
	std::string Press(MObject* pObject, bool force = false)
	{
		calls.clear();
		update.ProcessInputRButtonDown(pObject, force);
		return calls;
	}
	// The cell under the cursor, as the view picks it.
	POINT Cursor() { return view.GetSelectedSector(g_x, g_y); }
	// The zone's castle at war between guilds.
	void War()
	{
		GuildWarInfo* pInfo = new GuildWarInfo;
		pInfo->setCastleID(ZoneID_t(g_pZone->GetID()));
		wars.SetWar(pInfo);
		CHECK(wars.IsExist(ZoneID_t(g_pZone->GetID())));
	}
	// The system messages shown, oldest first, joined.
	std::string System()
	{
		std::string lines;
		for (int i = 0; i < systemLog.GetSize(); i++)
			if (systemLog[i] != nullptr && systemLog[i][0] != '\0') lines += std::string(systemLog[i]) + ";";
		return lines;
	}
};

bool Unpicked(const MTopView& view) { return view.m_SelectSector.x == SECTORPOSITION_NULL && view.m_SelectSector.y == SECTORPOSITION_NULL; }

} // namespace

//----------------------------------------------------------------------
// Init
//----------------------------------------------------------------------
TEST(GameUpdate1Init, ItForgetsTheWalkHandsTheInputToTheLoopAndStartsTextInput)
{
	SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
	CHECK_EQ(0, SDL_InitSubSystem(SDL_INIT_VIDEO));
	{
		InputScene scene;
		CHECK(scene.input.Init(nullptr, nullptr));
		scene.heldStarted = true;
		SDL_StopTextInput();
		CHECK(!SDL_IsTextInputActive());
		g_bPreviousMove = true;
		calls.clear();
		scene.update.Init();
		CHECK(!g_bPreviousMove);
		CHECK(SDL_IsTextInputActive());
		CHECK_SAME(std::string(""), calls);
		SDL_StopTextInput();

		// The mouse: the cursor moves to the wheel's point first (left);
		// the wheel goes to the UI at the cursor the game keeps.
		scene.input.DispatchMouseAt(CSDLInput::WHEELUP, 30, 20);
		CHECK_SAME("wheelup" + At(7, 9) + ";", calls);
		gbl_mine_progress = true;
		calls.clear();
		scene.input.DispatchMouseAt(CSDLInput::RIGHTUP, 30, 20);
		CHECK_SAME(std::string(""), calls);
		CHECK(!gbl_mine_progress);

		// The keyboard, pressed and released through the input.
		scene.keys.SetAcceleratorKey(1, DIK_F5);
		calls.clear();
		dxlib_input_virtual_key(DIK_F5, 1);
		scene.input.UpdateInput();
		CHECK_SAME(std::string("accel;"), calls);
		calls.clear();
		dxlib_input_virtual_key(DIK_F5, 0);
		scene.input.UpdateInput();
		CHECK_SAME(std::string(""), calls);
		scene.keys.SetAcceleratorKey(1, DIK_TAB);
		calls.clear();
		dxlib_input_virtual_key(DIK_TAB, 1);
		scene.input.UpdateInput();
		CHECK_SAME("keydown" + N(VK_TAB) + ",0;accel;", calls);
		dxlib_input_virtual_key(DIK_TAB, 0);
		scene.input.UpdateInput();
		// Ctrl+F flips the FPS display: the game's receiver, not the wait
		// screens'.
		dxlib_input_virtual_key(DIK_LCONTROL, 1);
		scene.input.UpdateInput();
		dxlib_input_virtual_key(DIK_F, 1);
		scene.input.UpdateInput();
		CHECK_EQ(TRUE, scene.option.DrawFPS);
		dxlib_input_virtual_reset();
		scene.input.UpdateInput();
		CHECK_EQ(TRUE, scene.option.DrawFPS);
	}
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

//----------------------------------------------------------------------
// DXMouseEvent
//----------------------------------------------------------------------
TEST(GameUpdate1Mouse, TheWheelGoesToTheUIAtTheCursorNotTheEventsPoint)
{
	InputScene scene;
	CHECK_SAME("wheeldown" + At(7, 9) + ";", scene.Mouse(CSDLInput::WHEELDOWN, 30, 20));
	CHECK_SAME("wheelup" + At(7, 9) + ";", scene.Mouse(CSDLInput::WHEELUP, 30, 20));
	g_x = 1;
	g_y = 2;
	CHECK_SAME("wheelup" + At(1, 2) + ";", scene.Mouse(CSDLInput::WHEELUP, 30, 20));
	CHECK(!gbl_mine_progress);
}

TEST(GameUpdate1Mouse, ARightReleaseEndsTheMineInstallingAndReachesNothing)
{
	InputScene scene;
	gbl_mine_progress = true;
	CHECK_SAME(std::string(""), scene.Mouse(CSDLInput::RIGHTUP, 30, 20));
	CHECK(!gbl_mine_progress);
	CHECK_SAME(std::string(""), scene.Mouse(CSDLInput::RIGHTUP, 30, 20));
	CHECK(!gbl_mine_progress);
}

TEST(GameUpdate1Mouse, ALeftPressAndTheOtherEventsReachNothing)
{
	InputScene scene;
	gbl_mine_progress = true;
	// The left press reaches the UI from ProcessInput's pulse alone: from
	// here as well, every click ran twice.
	for (CSDLInput::E_MOUSE_EVENT event : {CSDLInput::LEFTDOWN, CSDLInput::MOVE, CSDLInput::LEFTUP,
		CSDLInput::RIGHTDOWN, CSDLInput::CENTERDOWN, CSDLInput::CENTERUP})
		CHECK_SAME(std::string(""), scene.Mouse(event, 30, 20));
	CHECK(gbl_mine_progress);
	CHECK_EQ(7, g_x);
	CHECK_EQ(9, g_y);
}

TEST(GameUpdate1Mouse, AnEventDenyingTheMouseDropsItAll)
{
	for (DWORD flag : {DWORD(EVENTFLAG_DENY_INPUT_MOUSE), DWORD(EVENTFLAG_DENY_INPUT)})
	{
		InputScene scene;
		AddEvent(scene.events, EVENTID_METEOR, flag);
		gbl_mine_progress = true;
		CHECK_SAME(std::string(""), scene.Mouse(CSDLInput::WHEELDOWN, 30, 20));
		CHECK_SAME(std::string(""), scene.Mouse(CSDLInput::WHEELUP, 30, 20));
		CHECK_SAME(std::string(""), scene.Mouse(CSDLInput::RIGHTUP, 30, 20));
		CHECK(gbl_mine_progress);
		// Gone, the mouse is back.
		scene.events.RemoveEvent(EVENTID_METEOR);
		CHECK_SAME(std::string(""), scene.Mouse(CSDLInput::RIGHTUP, 30, 20));
		CHECK(!gbl_mine_progress);
		CHECK_SAME("wheelup" + At(7, 9) + ";", scene.Mouse(CSDLInput::WHEELUP, 30, 20));
	}
	// Denying the keyboard (or anything else) leaves the mouse.
	for (DWORD flag : {DWORD(EVENTFLAG_DENY_INPUT_KEYBOARD), DWORD(EVENTFLAG_NOT_DRAW_MOUSE_POINTER)})
	{
		InputScene scene;
		AddEvent(scene.events, EVENTID_METEOR, flag);
		gbl_mine_progress = true;
		CHECK_SAME("wheelup" + At(7, 9) + ";", scene.Mouse(CSDLInput::WHEELUP, 30, 20));
		CHECK_SAME(std::string(""), scene.Mouse(CSDLInput::RIGHTUP, 30, 20));
		CHECK(!gbl_mine_progress);
	}
}

//----------------------------------------------------------------------
// DXKeyboardEvent
//----------------------------------------------------------------------
TEST(GameUpdate1Keyboard, TheEditingKeysReachTheUIAsVirtualKeysFirst)
{
	InputScene scene;
	const struct { DWORD dik; UINT vk; } keys[] = {
		{DIK_TAB, VK_TAB}, {DIK_BACK, VK_BACK}, {DIK_RETURN, VK_RETURN},
		{DIK_ESCAPE, VK_ESCAPE}, {DIK_LEFT, VK_LEFT}, {DIK_RIGHT, VK_RIGHT},
		{DIK_UP, VK_UP}, {DIK_DOWN, VK_DOWN}, {DIK_HOME, VK_HOME},
		{DIK_END, VK_END}, {DIK_DELETE, VK_DELETE}, {DIK_INSERT, VK_INSERT},
	};
	for (const auto& key : keys)
		CHECK_SAME("keydown" + N(key.vk) + ",0;accel;", scene.Key(CSDLInput::KEYDOWN, key.dik));
	// (The scan codes the receiver spells out itself.)
	CHECK_EQ(0x0E, int(DIK_BACK));
	CHECK_EQ(0xD3, int(DIK_DELETE));
	CHECK_EQ(0xD2, int(DIK_INSERT));
}

TEST(GameUpdate1Keyboard, OtherKeysReachTheUIAsScanCodesOnly)
{
	InputScene scene;
	CHECK_SAME(std::string("accel;"), scene.Key(CSDLInput::KEYDOWN, DIK_F5));
	CHECK_SAME(std::string("accel;"), scene.Key(CSDLInput::KEYDOWN, DIK_NEXT));
	CHECK_SAME(std::string("accel;"), scene.Key(CSDLInput::KEYDOWN, DIK_NUMPADENTER));
	CHECK_EQ(FALSE, scene.option.DrawFPS);
}

TEST(GameUpdate1Keyboard, ScanCode0xCCAndAReleasedKeyAreLeft)
{
	InputScene scene;
	// 0xCC lies between the left (0xCB) and right (0xCD) arrows.
	CHECK_SAME(std::string(""), scene.Key(CSDLInput::KEYDOWN, 0xCC));
	CHECK_SAME("keydown" + N(VK_LEFT) + ",0;accel;", scene.Key(CSDLInput::KEYDOWN, 0xCB));
	CHECK_SAME("keydown" + N(VK_RIGHT) + ",0;accel;", scene.Key(CSDLInput::KEYDOWN, 0xCD));
	CHECK_SAME(std::string(""), scene.Key(CSDLInput::KEYUP, 0xCC));
	CHECK_SAME(std::string(""), scene.Key(CSDLInput::KEYUP, DIK_TAB));
	CHECK_SAME(std::string(""), scene.Key(CSDLInput::KEYUP, DIK_F5));
	// A released F with Ctrl held flips nothing.
	scene.Hold(DIK_LCONTROL);
	CHECK_SAME(std::string(""), scene.Key(CSDLInput::KEYUP, DIK_F));
	CHECK_EQ(FALSE, scene.option.DrawFPS);
	scene.ReleaseAll();
}

TEST(GameUpdate1Keyboard, AnEventDenyingTheKeyboardDropsItAll)
{
	for (DWORD flag : {DWORD(EVENTFLAG_DENY_INPUT_KEYBOARD), DWORD(EVENTFLAG_DENY_INPUT)})
	{
		InputScene scene;
		AddEvent(scene.events, EVENTID_METEOR, flag);
		AddEvent(scene.events, EVENTID_OUSTERS_FIN, 0);
		CHECK_SAME(std::string(""), scene.Key(CSDLInput::KEYDOWN, DIK_TAB));
		CHECK_SAME(std::string(""), scene.Key(CSDLInput::KEYDOWN, DIK_F5));
		scene.Hold(DIK_LCONTROL);
		CHECK_SAME(std::string(""), scene.Key(CSDLInput::KEYDOWN, DIK_F));
		CHECK_EQ(FALSE, scene.option.DrawFPS);
		CHECK(scene.events.IsEvent(EVENTID_OUSTERS_FIN));
		scene.ReleaseAll();
	}
	// Denying the mouse leaves the keyboard.
	InputScene scene;
	AddEvent(scene.events, EVENTID_METEOR, EVENTFLAG_DENY_INPUT_MOUSE);
	CHECK_SAME("keydown" + N(VK_TAB) + ",0;accel;", scene.Key(CSDLInput::KEYDOWN, DIK_TAB));
	CHECK_SAME(std::string("accel;"), scene.Key(CSDLInput::KEYDOWN, DIK_F5));
}

TEST(GameUpdate1Keyboard, CtrlFFlipsTheFPSDisplayAndEndsTheOustersEnding)
{
	for (DWORD ctrl : {DWORD(DIK_LCONTROL), DWORD(DIK_RCONTROL)})
	{
		InputScene scene;
		AddEvent(scene.events, EVENTID_OUSTERS_FIN, 0);
		AddEvent(scene.events, EVENTID_METEOR, 0);
		scene.Hold(ctrl);
		scene.keys.SetAcceleratorKey(1, DIK_F5);
		calls.clear();
		Probe::DXKeyboardEvent(CSDLInput::KEYDOWN, DIK_F);
		// The removal resets the gamma (no fade is queued).
		CHECK_SAME(std::string("gamma0,0,0;"), calls);
		CHECK_EQ(TRUE, scene.option.DrawFPS);
		CHECK(!scene.events.IsEvent(EVENTID_OUSTERS_FIN));
		CHECK(scene.events.IsEvent(EVENTID_METEOR));
		// Again: flipped back; no ending left to end.
		calls.clear();
		Probe::DXKeyboardEvent(CSDLInput::KEYDOWN, DIK_F);
		CHECK_SAME(std::string(""), calls);
		CHECK_EQ(FALSE, scene.option.DrawFPS);
		CHECK(scene.events.IsEvent(EVENTID_METEOR));
		scene.ReleaseAll();
	}
	InputScene scene;
	AddEvent(scene.events, EVENTID_OUSTERS_FIN, 0);
	// F alone, or Ctrl with another key (Space's case does nothing), or
	// Shift or Alt with F: nothing flips.
	scene.keys.SetAcceleratorKey(1, DIK_F5);
	calls.clear();
	Probe::DXKeyboardEvent(CSDLInput::KEYDOWN, DIK_F);
	CHECK_EQ(FALSE, scene.option.DrawFPS);
	for (DWORD other : {DWORD(DIK_LSHIFT), DWORD(DIK_LMENU)})
	{
		scene.Hold(other);
		Probe::DXKeyboardEvent(CSDLInput::KEYDOWN, DIK_F);
		CHECK_EQ(FALSE, scene.option.DrawFPS);
		scene.ReleaseAll();
	}
	scene.Hold(DIK_LCONTROL);
	for (DWORD key : {DWORD(DIK_SPACE), DWORD(DIK_G), DWORD(DIK_D)})
	{
		Probe::DXKeyboardEvent(CSDLInput::KEYDOWN, key);
		CHECK_EQ(FALSE, scene.option.DrawFPS);
	}
	CHECK_SAME(std::string(""), calls);
	CHECK(scene.events.IsEvent(EVENTID_OUSTERS_FIN));
	scene.ReleaseAll();
}

//----------------------------------------------------------------------
// WindowToGamePoint (through ProcessInput, whose cursor move it maps)
//----------------------------------------------------------------------
TEST(GameUpdate1Cursor, TheCursorIsMappedFromTheWindowIntoTheGameFrame)
{
	InputScene scene;
	// The UI takes the left press, which ends the frame's input there.
	scene.input.m_lb_down = TRUE;
	{
		// Nothing presented yet: window pixels are game pixels.
		Letterbox none(false);
		cursor.x = 799;
		cursor.y = 500;
		calls.clear();
		scene.update.ProcessInput();
		CHECK_SAME("cursor;move" + At(799, 500) + ";ldown" + At(7, 9) + ";", calls);
	}
	{
		Letterbox box(true);
		// (799 - 133) * 800 / 1333 = 399 and 500 * 600 / 999 = 300.
		calls.clear();
		scene.update.ProcessInput();
		CHECK_SAME("cursor;move" + At(399, 300) + ";ldown" + At(7, 9) + ";", calls);
		// In the bars: clamped to the frame's edges.
		cursor.x = 0;
		cursor.y = 1200;
		calls.clear();
		scene.update.ProcessInput();
		CHECK_SAME("cursor;move" + At(0, 599) + ";ldown" + At(7, 9) + ";", calls);
		cursor.x = 1599;
		cursor.y = 0;
		calls.clear();
		scene.update.ProcessInput();
		CHECK_SAME("cursor;move" + At(799, 0) + ";ldown" + At(7, 9) + ";", calls);
	}
}

//----------------------------------------------------------------------
// ProcessInputRButtonDown
//----------------------------------------------------------------------
TEST(GameUpdate1RButton, ItHoldsTheRightButtonAndClearsThePickedCellFirst)
{
	World scene;
	PlayerProbe& me = *scene.me;
	me.Set(EFFECTSTATUS_DRAGON_EYES);
	me.SetRepeatAction();
	scene.Skill(SKILL_SNAKE_COMBO, FLAG_ACTIONINFO_TARGET_ZONE, 2);
	// Dragon Eyes: nothing more, the skill's repeat and the remembered
	// walk as they were; no war question with no object.
	CHECK_SAME(std::string(""), scene.Press(NULL));
	CHECK_EQ(TRUE, g_bRButtonDown);
	CHECK(Unpicked(scene.view));
	CHECK(me.IsRepeatAction());
	CHECK(me.IsTraceNULL());
	CHECK(g_bPreviousMove);
	CHECK_EQ(1, int(g_SelectSector.x));
	CHECK_EQ(2, int(g_SelectSector.y));
	CHECK(scene.wire.Sent().empty());
	// On a creature the selector is asked whether it is a war enemy first.
	MCreature* pEnemy = scene.Creature(20000, kVampire, 22, 15);
	g_bRButtonDown = FALSE;
	CHECK_SAME(std::string("war?;"), scene.Press(pEnemy));
	CHECK_EQ(TRUE, g_bRButtonDown);
	CHECK(me.IsTraceNULL());
	// Without a view it only holds the button.
	g_pTopView = NULL;
	g_bRButtonDown = FALSE;
	scene.view.m_SelectSector.x = 3;
	CHECK_SAME(std::string(""), scene.Press(NULL));
	CHECK_EQ(TRUE, g_bRButtonDown);
	CHECK_EQ(3, int(scene.view.m_SelectSector.x));
}

TEST(GameUpdate1RButton, OnWindowsTheSpeedCheckQuitsTheGameAfterFiveErrors)
{
	World scene;
	PlayerProbe& me = *scene.me;
	scene.Skill(SKILL_DOUBLE_IMPACT, FLAG_ACTIONINFO_TARGET_OTHER, 3);
	MCreature* pEnemy = scene.Creature(20000, kVampire, 22, 15);
	g_CheckErrorTime = 5;
#ifdef PLATFORM_WINDOWS
	CHECK_SAME("mode" + N(MODE_QUIT) + ";", scene.Press(pEnemy));
	CHECK_EQ(0, g_CheckErrorTime);
	CHECK_EQ(TRUE, g_bRButtonDown);
	CHECK(Unpicked(scene.view));
	CHECK(me.IsTraceNULL());
	g_CheckErrorTime = 4;
	CHECK_SAME(std::string("war?;unselect sector;"), scene.Press(pEnemy));
	CHECK_EQ(4, g_CheckErrorTime);
	CHECK(me.IsTraceCreature());
#else
	// Elsewhere there is no speed check.
	CHECK_SAME(std::string("war?;unselect sector;"), scene.Press(pEnemy));
	CHECK_EQ(5, g_CheckErrorTime);
	CHECK(me.IsTraceCreature());
#endif
}

TEST(GameUpdate1RButton, AStandingTurretIsTakenOutOfItsShape)
{
	World scene;
	PlayerProbe& me = *scene.me;
	me.Set(EFFECTSTATUS_INSTALL_TURRET);
	me.SetSpecialActionInfo(MAGIC_UN_TRANSFORM);
	me.SetRepeatAction();
	CGSkillToSelf expected;
	expected.setSkillType(MAGIC_UN_TRANSFORM);
	CHECK_SAME(Send(expected), scene.Press(NULL));
	// (Its effect id stays 0.)
	CHECK(scene.wire.Sent() == Encoded({&expected}));
	CHECK(me.IsWaitVerifyNULL());
	CHECK(me.IsRepeatAction());
	CHECK(me.IsTraceNULL());
	CHECK(g_bPreviousMove);
	CHECK_EQ(1, int(g_SelectSector.x));
	CHECK(Unpicked(scene.view));
	CHECK_EQ(TRUE, g_bRButtonDown);
}

TEST(GameUpdate1RButton, ATurretThatIsNotStandingStillDoesNothing)
{
	World scene;
	PlayerProbe& me = *scene.me;
	me.Set(EFFECTSTATUS_INSTALL_TURRET);
	me.SetSpecialActionInfo(MAGIC_UN_TRANSFORM);
	me.SetAction(ACTION_ATTACK);
	CHECK(!me.IsStop());
	me.SetRepeatAction();
	CHECK_SAME(std::string(""), scene.Press(NULL));
	CHECK(scene.wire.Sent().empty());
	CHECK(me.IsRepeatAction());
	CHECK_EQ(1, int(g_SelectSector.x));
	CHECK(Unpicked(scene.view));
}

TEST(GameUpdate1RButton, ATurretsFirstShotSoundsThenTheSkillGoesOn)
{
	World scene;
	PlayerProbe& me = *scene.me;
	me.Set(EFFECTSTATUS_INSTALL_TURRET);
	me.SetRepeatAction();
	// Its fire, chosen: the shot's sound, then the skill as without a turret
	// (no cell the turret reaches is under the cursor).
	me.SetSpecialActionInfo(SKILL_TURRET_FIRE);
	const POINT cell = scene.Cursor();
	CHECK_SAME("sound" + N(SOUND_SLAYER_TURRET_FIRE_START) + ";", scene.Press(NULL));
	CHECK(!me.IsRepeatAction());
	CHECK_EQ(int(cell.x), int(g_SelectSector.x));
	CHECK_EQ(int(cell.y), int(g_SelectSector.y));
	CHECK(scene.wire.Sent().empty());
	// Another skill: no sound.
	scene.Skill(SKILL_SNAKE_COMBO, FLAG_ACTIONINFO_TARGET_ZONE, 2);
	me.SetRepeatAction();
	CHECK_SAME(std::string(""), scene.Press(NULL));
	CHECK(me.IsTraceSector());
	CHECK(me.IsRepeatAction());
	CHECK(!g_bPreviousMove);
}

TEST(GameUpdate1RButton, OffASylphOnUnsafeGround)
{
	const EFFECTSTATUS sylphs[] = {
		EFFECTSTATUS_SUMMON_SYLPH, EFFECTSTATUS_SUMMON_SYLPH_GREEN, EFFECTSTATUS_SUMMON_SYLPH_ORANGE,
		EFFECTSTATUS_SUMMON_SYLPH_RED, EFFECTSTATUS_SUMMON_SYLPH_YELLOW, EFFECTSTATUS_SUMMON_SYLPH_WHITE,
		EFFECTSTATUS_SUMMON_SYLPH_BROWN, EFFECTSTATUS_SUMMON_SYLPH_LILAC, EFFECTSTATUS_SUMMON_SYLPH_BLACK,
	};
	for (EFFECTSTATUS sylph : sylphs)
	{
		World scene;
		PlayerProbe& me = *scene.me;
		me.Set(sylph);
		scene.Skill(SKILL_SNAKE_COMBO, FLAG_ACTIONINFO_TARGET_ZONE, 2);
		me.SetRepeatAction();
		CGSkillToSelf expected;
		expected.setSkillType(MAGIC_UN_TRANSFORM);
		CHECK_SAME(Send(expected), scene.Press(NULL));
		CHECK(scene.wire.Sent() == Encoded({&expected}));
		CHECK(me.GetWaitVerify() == MPlayer::WAIT_VERIFY_SYLPH_SUMMON_GETOFF);
		// The sylph can be summoned again in four seconds.
		CHECK_EQ(4000, int(scene.skillInfo[SKILL_SUMMON_SYLPH].GetAvailableTimeLeft()));
		// Off it, nothing else: no skill, no motorcycle.
		CHECK(me.IsRepeatAction());
		CHECK(me.IsTraceNULL());
		CHECK_EQ(1, int(g_SelectSector.x));
		CHECK(g_bPreviousMove);
		CHECK(Unpicked(scene.view));
	}
	// Riding as well: still the sylph's.
	World scene;
	PlayerProbe& me = *scene.me;
	me.Set(EFFECTSTATUS_SUMMON_SYLPH);
	me.SetMoveDevice(MCreature::MOVE_DEVICE_RIDE);
	CHECK_SAME("send" + N(Packet::PACKET_CG_SKILL_TO_SELF) + ";", scene.Press(NULL));
}

TEST(GameUpdate1RButton, ASylphOnSafeGroundOrNotStandingStillStays)
{
	World scene;
	PlayerProbe& me = *scene.me;
	me.Set(EFFECTSTATUS_SUMMON_SYLPH);
	scene.Skill(SKILL_SNAKE_COMBO, FLAG_ACTIONINFO_TARGET_ZONE, 2);
	me.SetRepeatAction();
	MSector& here = const_cast<MSector&>(scene.zone->GetSector(20, 15));
	// A safe zone, a cell safe for slayers, a cell safe for all.
	g_bZoneSafe = true;
	CHECK_SAME(std::string(""), scene.Press(NULL));
	g_bZoneSafe = false;
	here.SetSafe(FLAG_SECTOR_SAFE_SLAYER);
	CHECK_SAME(std::string(""), scene.Press(NULL));
	here.SetSafe(FLAG_SECTOR_SAFE_COMMON);
	CHECK_SAME(std::string(""), scene.Press(NULL));
	// A cell safe for vampires is no safe ground for a slayer.
	here.SetSafe(FLAG_SECTOR_SAFE_VAMPIRE);
	me.SetAction(ACTION_ATTACK);
	CHECK(!me.IsStop());
	CHECK_SAME(std::string(""), scene.Press(NULL));
	CHECK(scene.wire.Sent().empty());
	CHECK(me.IsWaitVerifyNULL());
	CHECK_EQ(0, int(scene.skillInfo[SKILL_SUMMON_SYLPH].GetAvailableTimeLeft()));
	CHECK(me.IsRepeatAction());
	CHECK(me.IsTraceNULL());
	CHECK_EQ(1, int(g_SelectSector.x));
	me.SetStop();
	CHECK_SAME("send" + N(Packet::PACKET_CG_SKILL_TO_SELF) + ";", scene.Press(NULL));
}

TEST(GameUpdate1RButton, AtWarNoCellIsSafeGroundButASafeZoneIs)
{
	World scene;
	PlayerProbe& me = *scene.me;
	me.Set(EFFECTSTATUS_SUMMON_SYLPH);
	scene.War();
	MSector& here = const_cast<MSector&>(scene.zone->GetSector(20, 15));
	here.SetSafe(FLAG_SECTOR_SAFE_COMMON);
	g_bZoneSafe = true;
	CHECK_SAME(std::string(""), scene.Press(NULL));
	CHECK(me.IsWaitVerifyNULL());
	g_bZoneSafe = false;
	CHECK_SAME("send" + N(Packet::PACKET_CG_SKILL_TO_SELF) + ";", scene.Press(NULL));
	CHECK(me.GetWaitVerify() == MPlayer::WAIT_VERIFY_SYLPH_SUMMON_GETOFF);
}

TEST(GameUpdate1RButton, OffAMotorcycleUnlessAFlagCorpseIsNear)
{
	World scene;
	PlayerProbe& me = *scene.me;
	me.SetMoveDevice(MCreature::MOVE_DEVICE_RIDE);
	me.SetRepeatAction();
	scene.Corpse(700, 671, 21, 16);	// not the flag's
	scene.Corpse(701, 670, 22, 15);	// two cells away
	CGGetOffMotorCycle expected;
	expected.setObjectID(0);
	CHECK_SAME(Send(expected), scene.Press(NULL));
	CHECK(scene.wire.Sent() == Encoded({&expected}));
	CHECK(me.GetWaitVerify() == MPlayer::WAIT_VERIFY_MOTORCYCLE_GETOFF);
	CHECK(me.IsRepeatAction());
	CHECK(me.IsTraceNULL());
	CHECK_EQ(1, int(g_SelectSector.x));
	CHECK_SAME(std::string(""), scene.System());
	// The flag's corpse beside it: a message instead (none with no string).
	g_pGameStringTable->Init(MAX_GAME_STRING + 1);
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_ACTION_MOTORCYCLE_FLAG, "near the flag");
	scene.Corpse(702, 670, 19, 14);
	me.SetWaitVerifyNULL();
	CHECK_SAME(std::string(""), scene.Press(NULL));
	CHECK_SAME(std::string("near the flag;"), scene.System());
	CHECK(me.IsWaitVerifyNULL());
	CHECK(scene.wire.Sent() == Encoded({&expected}));
	g_pGameStringTable->Set(UI_STRING_MESSAGE_CANNOT_ACTION_MOTORCYCLE_FLAG, "");
	CHECK_SAME(std::string(""), scene.Press(NULL));
	CHECK_SAME(std::string("near the flag;"), scene.System());
}

TEST(GameUpdate1RButton, ARiderHeldOrNotStandingStillUsesItsSkillInstead)
{
	const EFFECTSTATUS holds[] = { EFFECTSTATUS_CURSE_PARALYSIS, EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS, EFFECTSTATUS_EXPLOSION_WATER };
	for (int i = 0; i <= 3; ++i)
	{
		World scene;
		PlayerProbe& me = *scene.me;
		scene.Skill(SKILL_SNAKE_COMBO, FLAG_ACTIONINFO_TARGET_ZONE, 2);
		me.SetMoveDevice(MCreature::MOVE_DEVICE_RIDE);
		if (i < 3)
			me.Set(holds[i]);
		else
		{
			me.Busy();
			CHECK(!me.IsStop());
		}
		me.SetRepeatAction();
		const POINT cell = scene.Cursor();
		// A rider uses no skill: the repeat is dropped, the cell taken.
		CHECK_SAME(std::string(""), scene.Press(NULL));
		CHECK(scene.wire.Sent().empty());
		CHECK(me.IsWaitVerifyNULL());
		CHECK(!me.IsRepeatAction());
		CHECK(me.IsTraceNULL());
		CHECK_EQ(int(cell.x), int(g_SelectSector.x));
		CHECK_EQ(int(cell.y), int(g_SelectSector.y));
	}
}

TEST(GameUpdate1RButton, UnderGroundItComesUpWhereItIs)
{
	for (MCreature::MOVE_TYPE type : {MCreature::CREATURE_UNDERGROUND, MCreature::CREATURE_FAKE_UNDERGROUND})
	{
		World scene;
		PlayerProbe& me = *scene.me;
		me.m_MoveType = type;
		me.SetCurrentDirection(3);
		me.SetRepeatAction();
		// Moving or not.
		me.SetAction(ACTION_ATTACK);
		CGUnburrow expected;
		expected.setX(20);
		expected.setY(15);
		expected.setDir(3);
		CHECK_SAME(Send(expected), scene.Press(NULL));
		CHECK(scene.wire.Sent() == Encoded({&expected}));
		CHECK(me.GetWaitVerify() == MPlayer::WAIT_VERIFY_SKILL_SUCCESS);
		CHECK(me.IsRepeatAction());
		CHECK_EQ(1, int(g_SelectSector.x));
		CHECK(Unpicked(scene.view));
	}
}

TEST(GameUpdate1RButton, NowhereASkillOnItselfIsUsedAndRepeated)
{
	World scene;
	PlayerProbe& me = *scene.me;
	scene.Skill(MAGIC_LIGHT, FLAG_ACTIONINFO_TARGET_SELF, 0);
	CHECK_SAME(std::string(""), scene.Press(NULL));
	CHECK_EQ(int(FLAG_TRACE_SELF), int(me.m_fTrace));
	CHECK_EQ(int(MAGIC_LIGHT), int(me.m_nUsedActionInfo));
	CHECK(me.IsRepeatAction());
	// Not a cell: the cursor is not read, the walk still remembered.
	CHECK_EQ(1, int(g_SelectSector.x));
	CHECK_EQ(2, int(g_SelectSector.y));
	CHECK(g_bPreviousMove);
	CHECK(Unpicked(scene.view));
	CHECK(scene.wire.Sent().empty());
	// One it cannot use now: the repeat dropped, still no cell read.
	scene.skillInfo.GetMutable(MAGIC_LIGHT)->SetEnable(false);
	me.TraceNULL();
	CHECK_SAME(std::string(""), scene.Press(NULL));
	CHECK(!me.IsRepeatAction());
	CHECK(me.IsTraceNULL());
	CHECK_EQ(1, int(g_SelectSector.x));
	CHECK_EQ(2, int(g_SelectSector.y));
}

TEST(GameUpdate1RButton, NowhereASkillOnTheZoneGoesToTheCellUnderTheCursor)
{
	World scene;
	PlayerProbe& me = *scene.me;
	scene.Skill(SKILL_SNAKE_COMBO, FLAG_ACTIONINFO_TARGET_ZONE, 2);
	const POINT cell = scene.Cursor();
	// (The cursor's x and y, not swapped.)
	CHECK_EQ(WorldTileGeometry::PixelToTileX(300), int(cell.x));
	CHECK_EQ(WorldTileGeometry::PixelToTileY(130), int(cell.y));
	CHECK(cell.x != scene.view.GetSelectedSector(130, 300).x);
	CHECK_SAME(std::string(""), scene.Press(NULL));
	CHECK_EQ(int(cell.x), int(g_SelectSector.x));
	CHECK_EQ(int(cell.y), int(g_SelectSector.y));
	CHECK(me.IsTraceSector());
	CHECK_EQ(int(cell.x), int(me.m_TraceX));
	CHECK_EQ(int(cell.y), int(me.m_TraceY));
	CHECK_EQ(int(SKILL_SNAKE_COMBO), int(me.m_nNextUsedActionInfo));
	CHECK(me.IsRepeatAction());
	CHECK(!g_bPreviousMove);
	// Recorded defect: the cell is marked picked (CGameUpdate.cpp:453) and
	// the mark cleared at once (464), so the press never shows it.
	CHECK(Unpicked(scene.view));
	CHECK(scene.wire.Sent().empty());
}

TEST(GameUpdate1RButton, NowhereASkillThatCannotGoThereDropsTheRepeat)
{
	World scene;
	PlayerProbe& me = *scene.me;
	// A skill on others only.
	scene.Skill(SKILL_DOUBLE_IMPACT, FLAG_ACTIONINFO_TARGET_OTHER, 3);
	me.SetRepeatAction();
	const POINT cell = scene.Cursor();
	CHECK_SAME(std::string(""), scene.Press(NULL));
	CHECK(!me.IsRepeatAction());
	CHECK(me.IsTraceNULL());
	CHECK_EQ(int(cell.x), int(g_SelectSector.x));
	CHECK_EQ(int(cell.y), int(g_SelectSector.y));
	CHECK(g_bPreviousMove);
	// A zone skill in a safe zone; a cursor off the zone.
	scene.Skill(SKILL_SNAKE_COMBO, FLAG_ACTIONINFO_TARGET_ZONE, 2);
	me.SetRepeatAction();
	g_bZoneSafe = true;
	CHECK_SAME(std::string(""), scene.Press(NULL));
	CHECK(!me.IsRepeatAction());
	CHECK(me.IsTraceNULL());
	CHECK(g_bPreviousMove);
	g_bZoneSafe = false;
	g_x = -500;
	CHECK_SAME(std::string(""), scene.Press(NULL));
	CHECK_EQ(int(SECTORPOSITION_NULL), int(g_SelectSector.x));
	CHECK(me.IsTraceNULL());
	CHECK(g_bPreviousMove);
	CHECK(Unpicked(scene.view));
}

TEST(GameUpdate1RButton, OnAnEnemyTheSkillTracesItAndRepeats)
{
	World scene;
	PlayerProbe& me = *scene.me;
	scene.Skill(SKILL_DOUBLE_IMPACT, FLAG_ACTIONINFO_TARGET_OTHER, 3);
	MCreature* pEnemy = scene.Creature(20000, kVampire, 26, 15);
	CHECK_SAME(std::string("war?;unselect sector;"), scene.Press(pEnemy));
	CHECK(me.IsTraceCreature());
	CHECK_EQ(20000, int(me.GetTraceID()));
	CHECK_EQ(int(SKILL_DOUBLE_IMPACT), int(me.m_nNextUsedActionInfo));
	CHECK(me.IsRepeatAction());
	CHECK(!g_bPreviousMove);
	CHECK(Unpicked(scene.view));
	// The cursor's cell is no business of a press on a creature.
	CHECK_EQ(1, int(g_SelectSector.x));
	CHECK(scene.wire.Sent().empty());
	CHECK_EQ(TRUE, g_bRButtonDown);
}

TEST(GameUpdate1RButton, OneOfItsOwnRaceIsTracedOnlyForcedOrAsAWarEnemy)
{
	for (int way = 0; way < 3; ++way)
	{
		World scene;
		PlayerProbe& me = *scene.me;
		// A skill slayers do not use, on a slayer player; refused there, it
		// is not used on the player's cell either.
		scene.Skill(SKILL_DOUBLE_IMPACT, FLAG_ACTIONINFO_TARGET_OTHER | FLAG_ACTIONINFO_TARGET_ZONE, 3);
		scene.actions.GetMutable(SKILL_DOUBLE_IMPACT)->SetUser(FLAG_ACTIONINFO_USER_VAMPIRE | FLAG_ACTIONINFO_USER_OUSTERS);
		MCreature* pOwn = scene.Creature(20001, kSlayerPlayer, 26, 15);
		CHECK(pOwn->IsPlayerOnly());
		if (way == 0)
		{
			CHECK_SAME(std::string("war?;"), scene.Press(pOwn));
			CHECK(me.IsTraceNULL());
			CHECK(!me.IsRepeatAction());
			CHECK(g_bPreviousMove);
			CHECK(Unpicked(scene.view));
			continue;
		}
		warForAll = way == 2;
		CHECK_SAME(std::string("war?;unselect sector;"), scene.Press(pOwn, way == 1));
		CHECK(me.IsTraceCreature());
		CHECK_EQ(20001, int(me.GetTraceID()));
		CHECK(me.IsRepeatAction());
		CHECK(!g_bPreviousMove);
	}
}

TEST(GameUpdate1RButton, OnACreatureASkillOnItselfOrOnTheZoneTakesItsTurn)
{
	{
		World scene;
		PlayerProbe& me = *scene.me;
		scene.Skill(MAGIC_LIGHT, FLAG_ACTIONINFO_TARGET_SELF, 0);
		MCreature* pEnemy = scene.Creature(20000, kVampire, 26, 15);
		CHECK_SAME(std::string("war?;"), scene.Press(pEnemy));
		CHECK_EQ(int(FLAG_TRACE_SELF), int(me.m_fTrace));
		CHECK(me.IsRepeatAction());
		CHECK(g_bPreviousMove);
		CHECK(Unpicked(scene.view));
	}
	{
		// On the creature's cell.
		World scene;
		PlayerProbe& me = *scene.me;
		scene.Skill(SKILL_SNAKE_COMBO, FLAG_ACTIONINFO_TARGET_ZONE, 2);
		MCreature* pEnemy = scene.Creature(20000, kVampire, 26, 17);
		CHECK_SAME(std::string("war?;"), scene.Press(pEnemy));
		CHECK(me.IsTraceSector());
		CHECK_EQ(26, int(me.m_TraceX));
		CHECK_EQ(17, int(me.m_TraceY));
		CHECK(me.IsRepeatAction());
		CHECK(!g_bPreviousMove);
		CHECK_EQ(1, int(g_SelectSector.x));
		CHECK(Unpicked(scene.view));
	}
	{
		// On itself and on the zone: itself first.
		World scene;
		PlayerProbe& me = *scene.me;
		scene.Skill(MAGIC_LIGHT, FLAG_ACTIONINFO_TARGET_SELF | FLAG_ACTIONINFO_TARGET_ZONE, 0);
		MCreature* pEnemy = scene.Creature(20000, kVampire, 26, 17);
		CHECK_SAME(std::string("war?;"), scene.Press(pEnemy));
		CHECK_EQ(int(FLAG_TRACE_SELF), int(me.m_fTrace));
		CHECK(g_bPreviousMove);
	}
	{
		// A skill on itself that refuses (a mine with none to lay), though
		// it could go on the zone: not on the zone either.
		World scene;
		PlayerProbe& me = *scene.me;
		scene.Skill(SKILL_INSTALL_MINE, FLAG_ACTIONINFO_TARGET_SELF | FLAG_ACTIONINFO_TARGET_ZONE, 2);
		MCreature* pEnemy = scene.Creature(20000, kVampire, 26, 17);
		CHECK_SAME(std::string("war?;"), scene.Press(pEnemy));
		CHECK(me.IsTraceNULL());
		CHECK(g_bPreviousMove);
	}
}

TEST(GameUpdate1RButton, AnNPCIsWalkedUpToAndTheSkillKept)
{
	World scene;
	PlayerProbe& me = *scene.me;
	scene.Skill(SKILL_DOUBLE_IMPACT, FLAG_ACTIONINFO_TARGET_OTHER, 3);
	MCreature* pNPC = scene.Creature(20002, kNPC, 30, 15);
	CHECK_SAME(std::string("war?;unselect sector;"), scene.Press(pNPC));
	CHECK(me.IsTraceCreature());
	CHECK_EQ(int(FLAG_TRACE_CREATURE_BASIC), int(me.m_fTrace & FLAG_TRACE_CREATURE_BASIC));
	CHECK_EQ(20002, int(me.GetTraceID()));
	CHECK(!me.IsRepeatAction());
	CHECK(g_bPreviousMove);
	CHECK(Unpicked(scene.view));
	CHECK(scene.wire.Sent().empty());
}
