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
#include "FrameClock.h"
#include "InputState.h"
#include "ZoneFlags.h"
#include "MonotonicClock.h"
#include "MEventManager.h"
#include "MEventQueue.h"
#include "MPlayer.h"
#include "MZone.h"
#include "MSector.h"
#include "MCreature.h"
#include "MItem.h"
#include "MEffect.h"
#include "MTopView.h"
#include "MObjectSelector.h"
#include "MJusticeAttackManager.h"
#include "MWarManager.h"
#include "GuildWarInfo.h"
#include "MParty.h"
#include "MTradeManager.h"
#include "MSkillManager.h"
#include "MInventory.h"
#include "MSlayerGear.h"
#include "MOustersGear.h"
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
#include "MPortal.h"
#include "TempInformation.h"
#include "SystemAvailabilities.h"
#include "UIDialog.h"
#include "DrawItemNameNode.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"
#include "SkillDef.h"
#include "SoundDef.h"
#include "UiRuntime.h"
#include "UserOption.h"
#include "VS_UI.h"
#include "VS_UI_Base.h"
#include "VS_UI_mouse_pointer.h"
#include "VS_UI_UIMessage.h"
#include "pi_core.h"
#include "u_window.h"
#include "timer2.h"
#include "Fl2.h"
#include "Ci.h"
#include "CIndexSprite.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackend.h"
#include "CDirectInput.h"
#include "DXLibBackend.h"
#include "Cpackets/CGUnburrow.h"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// MItemUse.cpp's, reached through MItemHost.
MItem*	NewItemOfClass(ITEM_CLASS itemClass);

// The globals the view's picking writes (MObjectScreen.cpp's).
extern POINT	g_MouseSector;
extern bool		g_bMouseInPortal;
extern int		g_DrawCamGapX;
extern int		g_DrawCamGapY;
extern bool		gbl_mine_progress;
// CGameUpdate.cpp's: the last left press walked; Windows' speed check's
// error count.
extern bool		g_bPreviousMove;
extern int		g_CheckErrorTime;

// The game loop's input (task 4.261 moved Client/CGameUpdate.cpp into
// gameclient), its ProcessInput: what the mouse and the keys do in the game
// each tick. In order:
// - an event that denies the mouse or the keyboard stops it;
// - a held left button is let go by its release or a right press (lock
//   off; a repeat ends, else the next destination, or the destination, is
//   marked); a held right by its release or a left press (lock off, the
//   button-up skill check, a repeat ends); a held centre by its release;
// - the UI is shown the cursor (the host's point, mapped to the game's
//   frame) and then each press and release of this tick at g_x, g_y, in
//   the order left down, left up, right down, right up, centre up, centre
//   down; the first it takes ends the input;
// - Alt or right Ctrl without left Shift, with no trade going on, shows
//   the items' names and, unless watching, repeating, a bat or a wolf,
//   starts a request: left Alt a trade, right Alt a party (when the party
//   system is open and no party request is), right Ctrl the other's info;
//   without them the names and the request go;
// - waiting for the server, laying a mine or moving fast stops it; a
//   repeating action unmarks the sector and stops it; the UI holding the
//   input unselects and stops it; so does death, with the sector too;
// - whom the mouse may pick: everyone (left Shift, hallucination, a safe
//   zone or sector, a skill of the player's own race, Intimate Grail),
//   friends (left Ctrl or a request) or enemies; by race (slayers and
//   ousters) or by guild (vampires);
// - the object: an item name under the mouse first while the names show,
//   then the view's pick;
// - watching, the edges of the screen scroll the player and stop it;
// - Caps Lock (not in a casket or underground) locks on, with the attack
//   cursor, and the press of this tick holds its button; without it the
//   lock ends when neither left nor right is held;
// - not locked: the selection and the cursor (nothing: unselect, attack
//   cursor with Shift, the no-move cursor over a blocked sector, else the
//   default, all but the attack one only off a portal; a creature: select
//   it, the attack cursor if it may be attacked, the default for an NPC; an
//   item: select it unless darkness hides it, the pick-up cursor with its
//   name and its colour; an effect: select it); then, after the delay and
//   not paused: Shift with a left press force-attacks a creature, with a
//   right press uses the skill forced; a left press turns a turret, walks
//   to an empty sector (not with left Ctrl), traces a creature (asking a
//   personal shop for its stock), goes to an item or an effect; a right
//   press uses the skill; the left button held after a walk keeps walking;
// - the release of this tick lets its button go (left: a repeat ends, else
//   the destination is marked; right: a repeat ends), and standing still
//   unmarks the sector.
//
// The scene: a real zone (g_pZone, ZoneState), a real MPlayer standing in
// it, a real view (g_pTopView) whose corner is the zone's and whose picking
// reads the objects' screen rects, a real selector, the real UI with a
// window of the test's over the screen (the cursor over it, the UI takes
// what it is given), the real mouse pointer, and a connection that writes
// what is sent. The keys are held through dxlib's virtual keys; the
// presses and releases of the tick are the input's public pulse flags.
//
// Not reachable headless: everything that asks the UI's game windows
// (gC_vs_ui's C_VS_UI_GAME, which only StartGame builds, from the skins
// and sprite packs): the party window (so the right and centre buttons
// always end the input when the UI takes them, and a party member is
// never picked), the request-party window, the web browser, the elevator,
// the Christmas card, the way points and the personal shop window all
// answer "not running".
//
// Not run, recorded:
// - a right press with an item or an effect under the mouse hands it to
//   ProcessInputRButtonDown, whose war check casts it to MCreature
//   (CGameUpdate.cpp:327): undefined behaviour, so no test presses there;
// - a quest or unique item's shade is CIndexSprite::ColorSet[set][16], the
//   set the client config's plus 0 to 14, unchecked against the 495 sets
//   (CGameUpdate.cpp:1227): a configured set past 480 reads off the table.
// Dead (each changed in turn, no test notices): the interaction-object
// press (the view never picks one); the creature press's second unmark
// (the press unmarked already); the portal check off the zone (the pick
// clears the flag there); the held left's Shift, delay and repeat checks
// (settled before it). Dead by reading: OnAttacking() for the cursor over
// nothing (a repeat stopped the tick) and the no-move cursor's
// no-creature-selected (unselected just before); its !g_bUIInput, like the
// focused-party-member return, needs the party window.
namespace {

std::vector<std::string> calls;
std::string N(long long value) { return std::to_string(value); }
using Calls = std::vector<std::string>;

bool Same(const Calls& expected)
{
	if (calls == expected) return true;
	std::printf("      calls: ");
	for (const std::string& call : calls) std::printf("[%s] ", call.c_str());
	std::printf("\n      wanted:");
	for (const std::string& call : expected) std::printf("[%s] ", call.c_str());
	std::printf("\n");
	return false;
}

int Count(const std::string& call)
{
	int count = 0;
	for (const std::string& made : calls) count += made == call;
	return count;
}

//----------------------------------------------------------------------
// The mouse pointer keeps its cursor, its text and its colour to itself;
// an explicit instantiation may name a private member, so these read them.
//----------------------------------------------------------------------
template <class Tag, typename Tag::type Member>
struct Expose
{
	friend typename Tag::type Get(Tag) { return Member; }
};
struct CursorOf { using type = int C_VS_UI_MOUSE_POINTER::*; friend type Get(CursorOf); };
struct TextOf { using type = std::string C_VS_UI_MOUSE_POINTER::*; friend type Get(TextOf); };
struct ColorOf { using type = COLORREF C_VS_UI_MOUSE_POINTER::*; friend type Get(ColorOf); };
template struct Expose<CursorOf, &C_VS_UI_MOUSE_POINTER::m_cursor>;
template struct Expose<TextOf, &C_VS_UI_MOUSE_POINTER::m_string>;
template struct Expose<ColorOf, &C_VS_UI_MOUSE_POINTER::m_color>;

//----------------------------------------------------------------------
// The connection: each packet sent is recorded and written, so its bytes
// can be read back.
//----------------------------------------------------------------------
struct Wire : ClientPlayer
{
	Wire() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~Wire() { setPlayerStatus(CPS_END_SESSION); }
	void sendPacket(Packet* pPacket) override
	{
		calls.push_back("send|" + N(pPacket->getPacketID()));
		Player::sendPacket(pPacket);
	}
	std::vector<unsigned char> Sent() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};

// The packets as one stream writes them.
std::vector<unsigned char> Encoded(const std::vector<const Packet*>& packets)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	for (const Packet* pPacket : packets) encoded.write(pPacket);
	return SocketOutputStreamTestAccess::Bytes(encoded);
}

//----------------------------------------------------------------------
// Hosts
//----------------------------------------------------------------------
int cursorX = 0, cursorY = 0;	// where the host says the cursor is
const CGameUpdateHost host{
	.UpdateMouse = []() { calls.push_back("mouse"); },
	.UpdateInput = []() { calls.push_back("input"); },
	.UpdateSocketInput = []() { calls.push_back("in"); return true; },
	.UpdateSocketOutput = []() { calls.push_back("out"); return true; },
	.SetMode = [](CLIENT_MODE mode) { calls.push_back("mode|" + N(mode)); },
	.GetCursorPoint = [](POINT& pt) { pt.x = cursorX; pt.y = cursorY; },
};

const ZoneState::Host zoneHost = []() {
	ZoneState::Host host{};
	host.Exists = []() { return g_pZone != NULL; };
	host.GetID = []() { return g_pZone != NULL ? int(g_pZone->GetID()) : int(ZONEID_NULL); };
	host.GetCreature = [](TYPE_OBJECTID id) { return g_pZone->GetCreature(id); };
	host.GetWidth = []() { return g_pZone != NULL ? int(g_pZone->GetWidth()) : 0; };
	host.GetHeight = []() { return g_pZone != NULL ? int(g_pZone->GetHeight()) : 0; };
	host.GetSector = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) -> const MSector* { return &g_pZone->GetSector(x, y); };
	host.GetFakeCreature = [](TYPE_OBJECTID id) { return g_pZone != NULL ? g_pZone->GetFakeCreature(id) : NULL; };
	host.MoveGroundCreatureIn = [](MZone* pZone, MCreature* pCreature, TYPE_SECTORPOSITION xo, TYPE_SECTORPOSITION yo, TYPE_SECTORPOSITION xn, TYPE_SECTORPOSITION yn) {
		return pZone->MoveGroundCreature(pCreature, xo, yo, xn, yn);
	};
	host.MoveFlyingCreatureIn = [](MZone* pZone, MCreature* pCreature, TYPE_SECTORPOSITION xo, TYPE_SECTORPOSITION yo, TYPE_SECTORPOSITION xn, TYPE_SECTORPOSITION yn) {
		return pZone->MoveFlyingCreature(pCreature, xo, yo, xn, yn);
	};
	host.MoveUndergroundCreatureIn = [](MZone* pZone, MCreature* pCreature, TYPE_SECTORPOSITION xo, TYPE_SECTORPOSITION yo, TYPE_SECTORPOSITION xn, TYPE_SECTORPOSITION yn) {
		return pZone->MoveUndergroundCreature(pCreature, xo, yo, xn, yn);
	};
	host.CanMove = [](int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { return g_pZone->CanMove(BYTE(moveType), x, y); };
	host.CanMoveIn = [](MZone* pZone, int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { return pZone->CanMove(BYTE(moveType), x, y); };
	host.GetWidthIn = [](MZone* pZone) { return int(pZone->GetWidth()); };
	host.GetHeightIn = [](MZone* pZone) { return int(pZone->GetHeight()); };
	host.GetCreatureIn = [](MZone* pZone, TYPE_OBJECTID id) { return pZone->GetCreature(id); };
	host.DeleteCreature = [](MCreature* pCreature) { delete pCreature; };
	return host;
}();

const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.Creature = []() -> MCreature* { return g_pPlayer; };
	host.Status = []() -> MStatus* { return g_pPlayer; };
	host.GetID = []() { return g_pPlayer != NULL ? g_pPlayer->GetID() : TYPE_OBJECTID(OBJECTID_NULL); };
	host.HasEffectStatus = [](EFFECTSTATUS status) { return g_pPlayer != NULL && g_pPlayer->HasEffectStatus(status); };
	return host;
}();

const CreatureState::Host creatureHost = []() {
	CreatureState::Host host{};
	host.GetAddonItemClass = [](MCreature*, int, ITEM_CLASS* pItemClass) { *pItemClass = ITEM_CLASS_NULL; return false; };
	host.ObjectOf = [](const MCreature* pCreature) -> MObject* { return const_cast<MCreature*>(pCreature); };
	host.CreatureOf = [](MObject* pObject) { return static_cast<MCreature*>(pObject); };
	return host;
}();

const MEffectHost effectHost = []() {
	MEffectHost host{};
	host.CurrentFrame = []() -> DWORD { return g_CurrentFrame; };
	return host;
}();

const MItemHost itemHost = []() {
	MItemHost host{};
	host.pCurrentFrame = &g_CurrentFrame;
	host.NewItem = NewItemOfClass;
	host.DropFrameCount = [](TYPE_FRAMEID) { return 4; };
	return host;
}();

// The view's request mode, as GameInit forwards it; the sounds recorded.
const WorldEvents::Host worldHost = []() {
	WorldEvents::Host host{};
	host.ViewExists = []() { return g_pTopView != NULL; };
	host.GetViewRequestMode = []() { return int(g_pTopView->GetRequestMode()); };
	host.IsViewRequestMode = []() { return g_pTopView->IsRequestMode(); };
	host.PlaySound = [](TYPE_SOUNDID soundID) { calls.push_back("sound|" + N(soundID)); };
	host.PlaySoundAt = [](TYPE_SOUNDID soundID, bool, int, int) { calls.push_back("sound|" + N(soundID)); };
	return host;
}();

// Whom the picking may select and the cursor call attackable: the
// player's tribe rule unless a test says otherwise.
bool forceAttack = false;
const MObjectSelectorHost selectorHost = []() {
	MObjectSelectorHost host{};
	host.ForceAttack = []() { return forceAttack; };
	host.PlayerHasEffectStatus = [](EFFECTSTATUS status) { return g_pPlayer->HasEffectStatus(status); };
	host.PlayerCanAttackTribe = [](MCreature* pCreature) { return g_pPlayer->CanAttackTribe(pCreature); };
	host.PlayerCanAttackGuild = [](MCreature* pCreature) { return g_pPlayer->CanAttackGuild(pCreature); };
	host.PlayerCreatureType = []() { return int(g_pPlayer->GetCreatureType()); };
	host.IsNonPK = []() { return false; };
	host.IsNPC = [](MCreature* pCreature) { return pCreature->IsNPC(); };
	host.GetName = [](MCreature* pCreature) { return pCreature->GetName(); };
	host.HasPlayerOnlySprite = [](MCreature*) { return false; };
	host.HasZone = []() { return false; };
	host.JusticeAttackManager = []() { return g_pJusticeAttackManager; };
	return host;
}();

// Each time the UI is handed the mouse it asks whether the mouse is
// denied ("ui"); the cursor is never drawn here.
const UiRuntime::Host uiRuntime{ .EventFlagActive = [](std::uint32_t flag) {
	if (flag == EVENTFLAG_DENY_INPUT_MOUSE) calls.push_back("ui");
	return false;
} };

std::string Message(UINT message)
{
	switch (message)
	{
		case M_MOVING: return "move";
		case M_LEFTBUTTON_DOWN: return "ldown";
		case M_LEFTBUTTON_UP: return "lup";
		case M_RIGHTBUTTON_DOWN: return "rdown";
		case M_RIGHTBUTTON_UP: return "rup";
		case M_CENTERBUTTON_DOWN: return "cdown";
		case M_CENTERBUTTON_UP: return "cup";
	}
	return "mouse" + N(message);
}
std::string At(int x, int y) { return "@" + N(x) + "," + N(y); }

// A window over the whole screen. While the cursor is over it the UI
// hands it the mouse, and so takes what it is given.
struct UiWindow : Window
{
	bool over = false;
	UiWindow() : Window(0, 0, 800, 600) {}
	bool IsPixel(int, int) override { return over; }
	bool MouseControl(UINT message, int x, int y) override { calls.push_back(Message(message) + At(x, y)); return true; }
	void Show() override {}
	void Process() override {}
};

// The UI's results (gpC_base->SendMessage), when a test drains them.
void Received(DWORD message, intptr_t left, intptr_t right, void*)
{
	calls.push_back("ui message|" + N(message) + "|" + N(left) + "|" + N(right));
}
void NoTimer() {}

struct Probe : CGameUpdate
{
	Probe() : CGameUpdate(&host) {}
	using CGameUpdate::ProcessInput;
};

// The player, with what it keeps to itself open to the tests.
struct PlayerProbe : MPlayer
{
	using MPlayer::m_Action;
	using MPlayer::m_ActionCount;
	using MPlayer::m_ActionCountMax;
	using MPlayer::m_MoveAction;
	using MPlayer::m_bTraceCreatureToForceAttack;
	using MPlayer::m_bKeepTraceCreature;
	using MPlayer::m_DestX;
	using MPlayer::m_DestY;
	using MPlayer::m_NextDestX;
	using MPlayer::m_NextDestY;
	using MPlayer::m_fTrace;
	using MPlayer::m_bAlive;
	using MPlayer::m_bInCasket;
	using MPlayer::m_bFastMove;
	using MPlayer::m_bEffectStatus;
	using MPlayer::m_DarknessCount;
	using MPlayer::m_CurrentDirection;
	// The status on or off, without what adding one starts.
	void Set(EFFECTSTATUS status, bool on = true) { m_bEffectStatus[status] = on; }
	void Destinations(int nextX, int nextY, int x, int y) { m_NextDestX = nextX; m_NextDestY = nextY; m_DestX = x; m_DestY = y; }
};

struct ZoneProbe : MZone
{
	using MZone::MZone;
	using MZone::SectorAt;
	using MZone::m_Info;
	using MZone::m_mapGroundEffect;
};

// The input lock as it was, put back after the scene's dialog is
// destroyed: ~UIDialog's Release() clears the lock.
struct InputLockSaved
{
	DWORD old = UIDialog::IsLockInput();
	~InputLockSaved()
	{
		if (old & FLAG_UIDIALOG_LOCK_PCTALK) UIDialog::SetLockInputPCTalk(); else UIDialog::UnSetLockInputPCTalk();
		if (old & FLAG_UIDIALOG_LOCK_MESSAGE) UIDialog::SetLockInputMessage(); else UIDialog::UnSetLockInputMessage();
	}
};

// Which bit of the flags is the party system.
struct Systems : SystemAvailabilitiesManager
{
	using SystemAvailabilitiesManager::SYSTEM_PARTY;
};

struct Selector : MObjectSelector
{
	using MObjectSelector::MObjectSelector;
	using MObjectSelector::m_SelectTarget;
	using MObjectSelector::m_SelectBy;
};

// An effect the mouse may pick.
struct Pickable : MSelectableEffect
{
	Pickable() : MSelectableEffect(BLT_EFFECT) {}
};

// Creature types: a slayer (the player), a vampire, an ousters, an NPC, and
// a slayer that only players wear.
const int kSlayer = 5, kVampire = 6, kOusters = 7, kNPC = 8, kSlayerPlayer = 9;
const TYPE_OBJECTID kPlayerID = 77;

// The point (px, py) of sector (sx, sy) on a screen whose corner is the
// zone's; the mouse is at (5, 5) of sector (10, 10) unless a test moves it.
int PX(int sx, int px = 5) { return sx * TILE_X + px; }
int PY(int sy, int py = 5) { return sy * TILE_Y + py; }
const int kMouseX = PX(10), kMouseY = PY(10);
// A screen rect over the mouse.
RECT Over() { return { kMouseX - 2, kMouseY - 2, kMouseX + 3, kMouseY + 3 }; }

// What a test sets apart from the defaults, to see what is left alone.
const POINT kMarked = { 3, 4 };
const TYPE_OBJECTID kSelected = 4321;

struct Scene : GameModelWorld
{
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
	TempInformation temp;
	MSkillInfoTable skillInfo;
	MInventory inventory;
	MSlayerGear slayerGear;
	MOustersGear oustersGear;
	Selector selector{ &selectorHost };
	SystemAvailabilitiesManager systems;
	MJusticeAttackManager justice;
	CZoneTable zoneTable;
	InputLockSaved oldLock;		// before dialog, so it is restored after ~UIDialog
	UIDialog dialog;
	Wire wire;
	PI_ProcessRunner runner;
	WindowManager windows;
	Base base;
	C_VS_UI_MOUSE_POINTER pointer;
	CI_KOREAN ime;
	CSDLInput input;
	CSpriteSurface screen;
	CSpriteSurface* pScreen = &screen;
	std::unique_ptr<UiWindow> window;

	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	MEventQueue* oldEvents = g_pEventManager;
	MWarManager* oldWars = g_pWarManager;
	MParty* oldParty = g_pParty;
	MTradeManager* oldTrade = g_pTradeManager;
	MSkillSet* oldSkills = g_pSkillAvailable;
	TempInformation* oldTemp = g_pTempInformation;
	MSkillInfoTable* oldSkillInfo = g_pSkillInfoTable;
	MInventory* oldInventory = g_pInventory;
	MSlayerGear* oldSlayerGear = g_pSlayerGear;
	MOustersGear* oldOustersGear = g_pOustersGear;
	MObjectSelector* oldSelector = g_pObjectSelector;
	MJusticeAttackManager* oldJustice = g_pJusticeAttackManager;
	SystemAvailabilitiesManager* oldSystems = g_pSystemAvailableManager;
	MMagazine* oldMagazine = g_pCurrentMagazine;
	CZoneTable* oldZoneTable = g_pZoneTable;
	UIDialog* oldDialog = g_pUIDialog;
	ClientPlayer* oldSocket = g_pSocket;
	MZone* oldZone = g_pZone;
	MPlayer* oldPlayer = g_pPlayer;
	MTopView* oldView = g_pTopView;
	CSDLInput* oldInput = g_pSDLInput;
	PI_ProcessRunner* oldRunner = gpC_process_runner;
	WindowManager* oldWindows = gpC_window_manager;
	Base* oldBase = gpC_base;
	C_VS_UI_MOUSE_POINTER* oldPointer = gpC_mouse_pointer;
	CI* oldIme = gC_ci;
	Race oldRace = g_eRaceInterface;
	int oldX = g_x, oldY = g_y;
	BOOL oldLeft = g_bLButtonDown, oldRight = g_bRButtonDown, oldCentre = g_bCButtonDown;
	BOOL oldUIInput = g_bUIInput;
	bool oldPreviousMove = g_bPreviousMove;
	int oldCheckErrorTime = g_CheckErrorTime;
	bool oldWatch = g_bWatchMode;
	bool oldZoneSafe = g_bZoneSafe;
	bool oldMine = gbl_mine_progress;
	POINT oldSelectSector = g_SelectSector;
	POINT oldMouseSector = g_MouseSector;
	bool oldInPortal = g_bMouseInPortal;
	int oldGapX = g_DrawCamGapX, oldGapY = g_DrawCamGapY;
	TYPE_OBJECTID oldOther = gC_vs_ui.GetOtherObjectID();
	DWORD oldFrame = g_CurrentFrame;
	MonotonicClock::TimePoint oldNow = g_FrameNow;
	const ZoneState::Host* oldZoneHost = ZoneState::SetHost(&zoneHost);
	const PlayerState::Host* oldPlayerHost = PlayerState::SetHost(&playerHost);
	const CreatureState::Host* oldCreatureHost = CreatureState::SetHost(&creatureHost);
	const MEffectHost* oldEffectHost = MEffect::SetHost(&effectHost);
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&worldHost);
	const UiRuntime::Host* oldRuntime = UiRuntime::SetHost(&uiRuntime);
	std::unique_ptr<ZoneProbe> zone;
	PlayerProbe* me = nullptr;
	std::unique_ptr<MTopView> view;
	Probe update;

	Scene()
	{
		static const bool moveTables = (MCreature::InitMoveTable(), true);
		(void)moveTables;
		calls.clear();
		g_CurrentFrame = 1000;
		g_FrameNow = MonotonicClock::TimePoint() + MonotonicClock::Millis(1000000);
		MItem::SetHost(&itemHost);
		creatureTypes.Init(800);
		g_pCreatureTable = &creatureTypes;
		sprites.Init(2);
		sprites.GetMutable(0)->CreatureType = FLAG_CREATURESPRITE_MONSTER;
		sprites.GetMutable(1)->CreatureType = FLAG_CREATURESPRITE_PLAYER_SLAYER;
		g_pCreatureSpriteTable = &sprites;
		Tribe(kSlayer, CREATURETRIBE_SLAYER);
		Tribe(kVampire, CREATURETRIBE_VAMPIRE);
		Tribe(kOusters, CREATURETRIBE_OUSTERS);
		Tribe(kNPC, CREATURETRIBE_NPC);
		Tribe(kSlayerPlayer, CREATURETRIBE_SLAYER, 1);
		Tribe(CREATURETYPE_BAT, CREATURETRIBE_VAMPIRE);
		Tribe(CREATURETYPE_WOLF, CREATURETRIBE_VAMPIRE);
		actions.Init(MAX_ACTIONINFO);
		for (int info = 0; info < MAX_ACTIONINFO; ++info)
		{
			actions.GetMutable(info)->Set("", ACTION_STAND, EFFECTSPRITETYPE_NULL, 0, 0);
			actions.GetMutable(info)->SetSoundID(SOUNDID_NULL);
		}
		actions.SetMinResultActionInfo(MIN_RESULT_ACTIONINFO);
		actions.GetMutable(SKILL_ATTACK_MELEE)->Set("", ACTION_ATTACK, EFFECTSPRITETYPE_NULL, 0, 0);
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
		g_pTradeManager = NULL;
		g_pSkillAvailable = &skills;
		g_pTempInformation = &temp;
		skillInfo.CTypeTable<SKILLINFO_NODE>::Init(MAX_ACTIONINFO);
		g_pSkillInfoTable = &skillInfo;
		inventory.Init(10, 6);
		g_pInventory = &inventory;
		slayerGear.Init();
		g_pSlayerGear = &slayerGear;
		oustersGear.Init();
		g_pOustersGear = &oustersGear;
		selector.SelectFriend();
		selector.SelectByGuild();
		g_pObjectSelector = &selector;
		g_pJusticeAttackManager = &justice;
		g_pSystemAvailableManager = &systems;
		g_pCurrentMagazine = NULL;
		for (int id : { 0, 60, 3001 })
		{
			ZONETABLE_INFO* pInfo = new ZONETABLE_INFO;
			pInfo->ID = TYPE_ZONEID(id);
			zoneTable.Add(pInfo);
		}
		g_pZoneTable = &zoneTable;
		g_pUIDialog = &dialog;
		UIDialog::UnSetLockInputPCTalk();
		UIDialog::UnSetLockInputMessage();
		g_pSocket = &wire;
		forceAttack = false;
		g_bZoneSafe = false;
		g_bWatchMode = false;
		g_bUIInput = FALSE;
		gbl_mine_progress = false;
		g_bLButtonDown = g_bRButtonDown = g_bCButtonDown = FALSE;
		g_bPreviousMove = false;
		g_CheckErrorTime = 0;
		g_SelectSector = kMarked;
		g_DrawCamGapX = g_DrawCamGapY = 0;
		g_MouseSector.x = g_MouseSector.y = -9;
		g_bMouseInPortal = false;
		g_eRaceInterface = RACE_SLAYER;
		gC_vs_ui.SetOtherObjectID(OBJECTID_NULL);
		// No letterbox: the window's points are the game's.
		spritectl_set_present_geometry(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

		zone.reset(new ZoneProbe(40, 30));
		g_pZone = zone.get();
		SetZoneID(60);
		me = new PlayerProbe;
		g_pPlayer = me;
		me->SetZone(g_pZone);
		PlayerType(kSlayer);
		me->SetID(kPlayerID);
		me->SetPosition(20, 15);
		me->SetServerPosition(20, 15);

		CHECK_EQ(0, spritectl_init());
		CHECK(screen.Init(800, 600));
		view.reset(new MTopView);
		view->SetSurface(pScreen);
		view->SetZone(zone.get());
		view->m_bFirstTileDraw = false;
		view->m_FirstSector.x = view->m_FirstSector.y = 0;
		view->m_PlusPoint.x = view->m_PlusPoint.y = 0;
		view->m_FirstZonePixel.x = view->m_FirstZonePixel.y = 0;
		g_pTopView = view.get();

		gpC_process_runner = &runner;
		gpC_window_manager = &windows;
		gpC_base = &base;
		gpC_mouse_pointer = &pointer;
		gC_ci = &ime;
		CHECK(input.Init(nullptr, nullptr));
		dxlib_input_virtual_reset();
		input.UpdateInput();
		g_pSDLInput = &input;
		window = std::make_unique<UiWindow>();
		window->Start();
		windows.AppearWindow(window.get());

		cursorX = 33;
		cursorY = 44;
		g_x = kMouseX;
		g_y = kMouseY;
		// What a test sees left alone or changed.
		Mark();
		SetCursor(C_VS_UI_MOUSE_POINTER::CURSOR_COIN);
	}
	~Scene()
	{
		windows.DisappearWindow(window.get());
		window.reset();
		dxlib_input_virtual_reset();
		g_pSDLInput = oldInput;
		g_pTopView = oldView;
		view.reset();
		delete me;
		g_pPlayer = oldPlayer;
		zone.reset();
		g_pZone = oldZone;
		spritectl_set_present_geometry(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		gC_vs_ui.SetOtherObjectID(oldOther);
		gC_ci = oldIme;
		gpC_mouse_pointer = oldPointer;
		gpC_base = oldBase;
		gpC_window_manager = oldWindows;
		gpC_process_runner = oldRunner;
		g_eRaceInterface = oldRace;
		g_DrawCamGapX = oldGapX;
		g_DrawCamGapY = oldGapY;
		g_bMouseInPortal = oldInPortal;
		g_MouseSector = oldMouseSector;
		g_SelectSector = oldSelectSector;
		gbl_mine_progress = oldMine;
		g_bZoneSafe = oldZoneSafe;
		g_bWatchMode = oldWatch;
		g_CheckErrorTime = oldCheckErrorTime;
		g_bPreviousMove = oldPreviousMove;
		g_bUIInput = oldUIInput;
		g_bCButtonDown = oldCentre;
		g_bRButtonDown = oldRight;
		g_bLButtonDown = oldLeft;
		g_y = oldY;
		g_x = oldX;
		UiRuntime::SetHost(oldRuntime);
		WorldEvents::SetHost(oldWorld);
		MEffect::SetHost(oldEffectHost);
		CreatureState::SetHost(oldCreatureHost);
		PlayerState::SetHost(oldPlayerHost);
		ZoneState::SetHost(oldZoneHost);
		MItem::SetHost(NULL);
		g_pSocket = oldSocket;
		g_pUIDialog = oldDialog;
		g_pZoneTable = oldZoneTable;
		g_pCurrentMagazine = oldMagazine;
		g_pSystemAvailableManager = oldSystems;
		g_pJusticeAttackManager = oldJustice;
		g_pObjectSelector = oldSelector;
		g_pOustersGear = oldOustersGear;
		g_pSlayerGear = oldSlayerGear;
		g_pInventory = oldInventory;
		oustersGear.Release();
		slayerGear.Release();
		inventory.Release();
		g_pSkillInfoTable = oldSkillInfo;
		g_pTempInformation = oldTemp;
		g_pSkillAvailable = oldSkills;
		g_pTradeManager = oldTrade;
		g_pParty = oldParty;
		g_pWarManager = oldWars;
		g_pEventManager = oldEvents;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pRankBonusTable = oldRankBonuses;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pActionInfoTable = oldActions;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatureTypes;
		g_CurrentFrame = oldFrame;
		g_FrameNow = oldNow;
	}

	// A creature type of the tribe, drawn with sprite type 0 (a monster's)
	// or 1 (one only players wear).
	void Tribe(int type, CREATURETRIBE tribe, int sprite = 0)
	{
		const int actionCount = tribe == CREATURETRIBE_VAMPIRE || tribe == CREATURETRIBE_NPC ? ACTION_MAX_VAMPIRE
			: (tribe == CREATURETRIBE_OUSTERS ? ACTION_MAX_OUSTERS : ACTION_MAX_SLAYER);
		CREATURETABLE_INFO& row = testfw::MutableRow(creatureTypes, type);
		row.SetCreatureTribe(tribe, actionCount);
		row.MoveTimes = 8;
		row.MoveTimesMotor = 8;
		row.MoveRatio = 1;
		for (int action = 0; action < actionCount; ++action)
			row.SetActionCount(action, 10 + action);
		row.SpriteTypes.Init(1);
		row.SpriteTypes.Set(0, TYPE_SPRITEID(sprite));
	}
	// The player becomes the type (its skills left alone).
	void PlayerType(int type)
	{
		MSkillSet* pSkills = g_pSkillAvailable;
		g_pSkillAvailable = NULL;
		me->SetCreatureType(TYPE_CREATURETYPE(type));
		g_pSkillAvailable = pSkills;
	}
	void SetZoneID(int id) { zone->m_Info.ZoneID = WORD(id); }
	MSector& Sector(int x, int y) { return *zone->SectorAt(x, y); }
	MTopView& View() { return *view; }

	// The marks a test reads back: the sector (3, 4), creature, item and
	// effect 4321, the names hidden, no request, the player's no request,
	// the selector on friends by guild.
	void Mark()
	{
		view->SetSelectedSector(kMarked);
		view->m_SelectCreatureID = kSelected;
		view->m_SelectItemID = kSelected;
		view->m_SelectEffectID = kSelected;
		view->UnSetDrawItemNameList();
		view->UnSetRequestMode();
		me->UnSetRequestMode();
		selector.SelectFriend();
		selector.SelectByGuild();
	}
	bool Marked() const
	{
		return view->m_SelectSector.x == kMarked.x && view->m_SelectSector.y == kMarked.y;
	}
	bool Selected(TYPE_OBJECTID creature, TYPE_OBJECTID item, TYPE_OBJECTID effect) const
	{
		const bool same = view->m_SelectCreatureID == creature && view->m_SelectItemID == item && view->m_SelectEffectID == effect;
		if (!same)
			std::printf("      selected creature %d item %d effect %d\n", int(view->m_SelectCreatureID), int(view->m_SelectItemID), int(view->m_SelectEffectID));
		return same;
	}
	bool SelectorUntouched() const
	{
		return selector.m_SelectTarget == MObjectSelector::SELECT_FRIEND && selector.m_SelectBy == MObjectSelector::SELECT_BY_GUILD;
	}

	int Cursor() const { return pointer.*Get(CursorOf()); }
	const std::string& CursorText() const { return pointer.*Get(TextOf()); }
	COLORREF CursorColor() const { return pointer.*Get(ColorOf()); }
	void SetCursor(int cursor) { pointer.SetCursor(cursor); }

	// The keys held this tick (dxlib's virtual keys, read by the input).
	void Keys(std::initializer_list<DWORD> held)
	{
		dxlib_input_virtual_reset();
		for (DWORD key : held) dxlib_input_virtual_key(int(key), 1);
		input.UpdateInput();
		for (DWORD key : held) CHECK(input.KeyDown(key));
	}
	// The presses and releases of this tick.
	void Buttons(bool lbDown, bool lbUp, bool rbDown, bool rbUp, bool cbDown, bool cbUp)
	{
		input.m_lb_down = lbDown;
		input.m_lb_up = lbUp;
		input.m_rb_down = rbDown;
		input.m_rb_up = rbUp;
		input.m_cb_down = cbDown;
		input.m_cb_up = cbUp;
	}
	void Run()
	{
		calls.clear();
		update.ProcessInput();
	}

	// A creature of the type standing in the zone (which owns it), under
	// the mouse.
	MCreature* Creature(TYPE_OBJECTID id, int type, int x = 10, int y = 10)
	{
		MCreature* pCreature = new MCreature;
		pCreature->SetZone(g_pZone);
		pCreature->SetCreatureType(TYPE_CREATURETYPE(type));
		pCreature->SetID(id);
		pCreature->SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
		pCreature->SetServerPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
		RECT rect = Over();
		pCreature->SetScreenRect(&rect);
		CHECK(zone->AddCreature(pCreature));
		return pCreature;
	}
	// An item lying in the zone (which owns it), under the mouse; a corpse
	// holds the body (the zone reads it as the corpse arrives).
	MItem* Item(TYPE_OBJECTID id, ITEM_CLASS itemClass = ITEM_CLASS_SWORD, int x = 10, int y = 10, MCreature* pBody = NULL)
	{
		if ((*g_pItemTable)[itemClass].GetSize() == 0)
		{
			g_pItemTable->InitClass(itemClass, 1);
			testfw::MutableRow(*g_pItemTable, itemClass, 0).SetGrid(1, 2);
			testfw::MutableRow(*g_pItemTable, itemClass, 0).HName = "Blade";
		}
		MItem* pItem = NewItemOfClass(itemClass);
		pItem->SetItemType(0);
		pItem->SetID(id);
		pItem->SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
		RECT rect = Over();
		pItem->SetScreenRect(&rect);
		if (pBody != NULL) static_cast<MCorpse*>(pItem)->SetCreature(pBody);
		CHECK(zone->AddItem(pItem, FALSE));
		return pItem;
	}
	// An event with the flag.
	void Event(EVENT_ID id, DWORD flag)
	{
		MEvent event;
		event.eventID = id;
		event.eventFlag = flag;
		event.eventDelay = -1;
		events.AddEvent(event);
	}
};

// A skill the player has, ready, aimed as the flag says, for the users
// the flag says.
void Skill(Scene& scene, int id, BYTE target, BYTE user)
{
	scene.actions.GetMutable(id)->Set("", ACTION_MAGIC, EFFECTSPRITETYPE_NULL, 3, target);
	scene.actions.GetMutable(id)->SetUser(user);
	scene.actions.GetMutable(id)->SetPacketType(target == FLAG_ACTIONINFO_TARGET_SELF ? ACTIONINFO_PACKET_SELF : ACTIONINFO_PACKET_OTHER);
	scene.skills.AddSkill(ACTIONINFO(id));
	scene.skillInfo.GetMutable(id)->SetEnable(true);
	scene.skillInfo.GetMutable(id)->minRange = 3;
	scene.skillInfo.GetMutable(id)->maxRange = 3;
	scene.me->SetSpecialActionInfo(TYPE_ACTIONINFO(id));
}

} // namespace

//======================================================================
// An event that denies the input
//======================================================================
TEST(GameUpdate2Deny, AnEventDenyingTheMouseOrTheKeysStopsEverything)
{
	for (DWORD flag : { DWORD(EVENTFLAG_DENY_INPUT_MOUSE), DWORD(EVENTFLAG_DENY_INPUT_KEYBOARD) })
	{
		Scene scene;
		scene.Event(EVENTID_METEOR, flag);
		g_bLButtonDown = g_bRButtonDown = g_bCButtonDown = TRUE;
		scene.me->SetLockMode();
		scene.Keys({ DIK_LMENU });
		scene.Buttons(true, true, true, true, true, true);
		scene.Run();
		CHECK(Same({}));
		CHECK(g_bLButtonDown && g_bRButtonDown && g_bCButtonDown);
		CHECK(scene.me->IsLockMode());
		CHECK(!scene.View().IsDrawItemNameList());
		CHECK(scene.SelectorUntouched());
		CHECK(scene.Marked());
		// Without it the same tick lets the buttons go.
		scene.events.RemoveEvent(EVENTID_METEOR);
		scene.Run();
		CHECK(!g_bLButtonDown && !g_bCButtonDown);
		CHECK(scene.View().IsDrawItemNameList());
	}
}

//======================================================================
// The buttons held from earlier ticks
//======================================================================
namespace {
using Pointer = C_VS_UI_MOUSE_POINTER;
const TYPE_OBJECTID kNone = OBJECTID_NULL;
const std::string kMove = "move" + At(33, 44);
std::string Click(const char* message) { return message + At(kMouseX, kMouseY); }
} // namespace

TEST(GameUpdate2Held, AHeldLeftIsLetGoByItsReleaseOrARightPressAndMarksTheNextDestination)
{
	// The UI takes the release or the press (the cursor is over its
	// window), which ends the input there: what is seen is the letting go.
	for (int trigger = 0; trigger < 2; ++trigger)
	{
		Scene scene;
		scene.window->over = true;
		g_bLButtonDown = TRUE;
		scene.me->SetLockMode();
		scene.me->Destinations(12, 13, 14, 15);
		scene.Buttons(false, trigger == 0, trigger == 1, false, false, false);
		scene.Run();
		CHECK(Same({ "ui", kMove, "ui", Click(trigger == 0 ? "lup" : "rdown") }));
		CHECK(!g_bLButtonDown);
		CHECK(!scene.me->IsLockMode());
		CHECK_EQ(12, int(scene.View().m_SelectSector.x));
		CHECK_EQ(13, int(scene.View().m_SelectSector.y));
		CHECK(scene.SelectorUntouched());
	}
	// A right release or a left press does not let it go.
	for (int trigger = 0; trigger < 2; ++trigger)
	{
		Scene scene;
		scene.window->over = true;
		g_bLButtonDown = TRUE;
		scene.me->SetLockMode();
		scene.me->Destinations(12, 13, 14, 15);
		scene.Buttons(trigger == 1, false, false, trigger == 0, false, false);
		scene.Run();
		CHECK(Same({ "ui", kMove, "ui", Click(trigger == 0 ? "rup" : "ldown") }));
		CHECK(g_bLButtonDown);
		CHECK(scene.me->IsLockMode());
		CHECK(scene.Marked());
	}
}

TEST(GameUpdate2Held, TheLetGoLeftMarksTheDestinationWhenThereIsNoNextOne)
{
	const int N = SECTORPOSITION_NULL;
	const struct { int nextX, nextY, x, y, markX, markY; } cases[] = {
		{ 12, 13, 14, 15, 12, 13 },
		{ N, 13, 14, 15, 14, 15 },		// half a next destination is none
		{ 12, N, 14, 15, 14, 15 },
		{ N, N, 14, 15, 14, 15 },
		{ N, N, N, 15, kMarked.x, kMarked.y },	// nor half a destination
		{ N, N, 14, N, kMarked.x, kMarked.y },
		{ N, N, N, N, kMarked.x, kMarked.y },
		{ 0, 0, 14, 15, 0, 0 },
	};
	for (const auto& c : cases)
	{
		Scene scene;
		scene.window->over = true;
		g_bLButtonDown = TRUE;
		scene.me->Destinations(c.nextX, c.nextY, c.x, c.y);
		scene.Buttons(false, true, false, false, false, false);
		scene.Run();
		CHECK_EQ(c.markX, int(scene.View().m_SelectSector.x));
		CHECK_EQ(c.markY, int(scene.View().m_SelectSector.y));
	}
}

TEST(GameUpdate2Held, ALeftLetGoWhileRepeatingEndsTheRepeatAndMarksNothing)
{
	Scene scene;
	scene.window->over = true;
	g_bLButtonDown = TRUE;
	scene.me->SetRepeatAction();
	scene.me->Destinations(12, 13, 14, 15);
	scene.Buttons(false, true, false, false, false, false);
	scene.Run();
	CHECK(!g_bLButtonDown);
	CHECK(!scene.me->IsRepeatAction());
	CHECK(scene.Marked());
}

TEST(GameUpdate2Held, AHeldRightIsLetGoByItsReleaseOrALeftPressAndRunsTheButtonUpSkillCheck)
{
	for (int trigger = 0; trigger < 2; ++trigger)
	{
		Scene scene;
		scene.window->over = true;
		g_bRButtonDown = TRUE;
		scene.me->SetLockMode();
		scene.me->SetRepeatAction();
		// A turret mid-attack is stopped by the check: its attack winds
		// down (54 - 5 frames of 54) with the end-of-fire sound.
		scene.me->Set(EFFECTSTATUS_INSTALL_TURRET);
		scene.me->m_Action = ACTION_ATTACK;
		scene.me->m_ActionCount = 5;
		scene.me->m_ActionCountMax = 30;
		scene.Buttons(trigger == 1, false, false, trigger == 0, false, false);
		scene.Run();
		CHECK(Same({ "sound|" + N(SOUND_SLAYER_TURRET_FIRE_END), "ui", kMove, "ui", Click(trigger == 0 ? "rup" : "ldown") }));
		CHECK(!g_bRButtonDown);
		CHECK(!scene.me->IsLockMode());
		CHECK(!scene.me->IsRepeatAction());
		CHECK_EQ(49, int(scene.me->m_ActionCount));
		CHECK_EQ(54, int(scene.me->m_ActionCountMax));
		CHECK(scene.Marked());
	}
	// Without a turret the repeat ends all the same.
	for (int trigger = 0; trigger < 2; ++trigger)
	{
		Scene scene;
		scene.window->over = true;
		g_bRButtonDown = TRUE;
		scene.me->SetRepeatAction();
		scene.Buttons(trigger == 1, false, false, trigger == 0, false, false);
		scene.Run();
		CHECK(Same({ "ui", kMove, "ui", Click(trigger == 0 ? "rup" : "ldown") }));
		CHECK(!g_bRButtonDown);
		CHECK(!scene.me->IsRepeatAction());
	}
	// A right press or a left release does not let it go.
	for (int trigger = 0; trigger < 2; ++trigger)
	{
		Scene scene;
		scene.window->over = true;
		g_bRButtonDown = TRUE;
		scene.me->SetLockMode();
		scene.me->SetRepeatAction();
		scene.Buttons(false, trigger == 1, trigger == 0, false, false, false);
		scene.Run();
		CHECK(Same({ "ui", kMove, "ui", Click(trigger == 0 ? "rdown" : "lup") }));
		CHECK(g_bRButtonDown);
		CHECK(scene.me->IsLockMode());
		CHECK(scene.me->IsRepeatAction());
	}
}

TEST(GameUpdate2Held, AHeldCentreIsLetGoByItsReleaseAlone)
{
	for (int release = 0; release < 2; ++release)
	{
		Scene scene;
		scene.window->over = true;
		g_bCButtonDown = TRUE;
		scene.me->SetLockMode();
		scene.Buttons(false, false, false, false, release == 0, release == 1);
		scene.Run();
		CHECK(Same({ "ui", kMove, "ui", Click(release == 1 ? "cup" : "cdown") }));
		CHECK_EQ(release == 0, bool(g_bCButtonDown));
		CHECK(scene.me->IsLockMode());		// the centre has no lock to end
	}
}

//======================================================================
// The UI first
//======================================================================
TEST(GameUpdate2Ui, TheUIIsShownTheHostsCursorMappedIntoTheFrameAndTheMoveAloneTakesNothing)
{
	{
		Scene scene;
		scene.window->over = true;
		scene.Run();
		CHECK(Same({ "ui", kMove }));
		// Not taken: the game went on and chose whom to pick.
		CHECK(!scene.SelectorUntouched());
	}
	{
		// A 1600x1200 window showing the 800x600 frame: points halve.
		Scene scene;
		scene.window->over = true;
		spritectl_set_present_geometry(1600, 1200, 1600, 1200, 0, 0, 1600, 1200, 800, 600);
		scene.Run();
		CHECK(Same({ "ui", "move" + At(16, 22) }));
	}
}

TEST(GameUpdate2Ui, ThePressesAndReleasesGoToTheUIInTurnAndTheFirstItTakesEndsTheInput)
{
	// Left down, left up, right down, right up, centre up, centre down: the
	// first of them set this tick is the one the UI takes, at g_x, g_y.
	const char* const order[] = { "ldown", "lup", "rdown", "rup", "cup", "cdown" };
	for (int first = 0; first < 6; ++first)
	{
		Scene scene;
		scene.window->over = true;
		scene.Keys({ DIK_LMENU });
		scene.Buttons(first <= 0, first <= 1, first <= 2, first <= 3, first <= 5, first <= 4);
		scene.Run();
		CHECK(Same({ "ui", kMove, "ui", Click(order[first]) }));
		CHECK(scene.SelectorUntouched());
		CHECK(!scene.View().IsDrawItemNameList());
	}
}

TEST(GameUpdate2Ui, EachPressOrReleaseTheUILeavesIsOfferedAndTheGameGoesOn)
{
	// Over no window: the UI is handed the cursor and then each of them.
	const bool sets[][6] = {
		{ false, false, false, false, false, false },
		{ true, false, false, false, false, false },
		{ false, true, false, false, false, false },
		{ false, false, true, false, false, false },
		{ false, false, false, true, false, false },
		{ false, false, false, false, true, false },
		{ false, false, false, false, false, true },
		{ true, true, true, true, true, true },
	};
	for (const auto& set : sets)
	{
		Scene scene;
		scene.Buttons(set[0], set[1], set[2], set[3], set[4], set[5]);
		scene.Run();
		int pulses = 0;
		for (bool on : set) pulses += on;
		CHECK_EQ(1 + pulses, Count("ui"));
		CHECK(!scene.SelectorUntouched());
	}
}

//======================================================================
// The item names and the requests
//======================================================================
// Waiting for the server stops the input right after them, so what is
// seen is theirs.
TEST(GameUpdate2Names, AltOrRightCtrlShowsTheItemNamesUnlessShiftOrATradeGetsInTheWay)
{
	for (DWORD key : { DWORD(DIK_LMENU), DWORD(DIK_RMENU), DWORD(DIK_RCONTROL) })
	{
		Scene scene;
		scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_NPC_ASK);
		scene.Keys({ key });
		scene.Run();
		CHECK(scene.View().IsDrawItemNameList());
		// With left Shift: no names, and no request.
		scene.View().SetRequestMode(MRequestMode::REQUEST_INFO);
		scene.Keys({ key, DIK_LSHIFT });
		scene.Run();
		CHECK(!scene.View().IsDrawItemNameList());
		CHECK_EQ(int(MRequestMode::REQUEST_NULL), int(scene.View().GetRequestMode()));
		// Trading, or waiting for a trade.
		MTradeManager trade;
		g_pTradeManager = &trade;
		scene.Keys({ key });
		scene.Run();
		CHECK(!scene.View().IsDrawItemNameList());
		g_pTradeManager = NULL;
		scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
		scene.Run();
		CHECK(!scene.View().IsDrawItemNameList());
		scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_NPC_ASK);
		scene.Run();
		CHECK(scene.View().IsDrawItemNameList());
	}
	// Other keys show nothing.
	Scene scene;
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_NPC_ASK);
	scene.Keys({ DIK_LCONTROL, DIK_LSHIFT, DIK_CAPITAL });
	scene.Run();
	CHECK(!scene.View().IsDrawItemNameList());
}

TEST(GameUpdate2Names, LeftAltAsksForATradeRightAltAPartyRightCtrlTheOthersInfo)
{
	const auto Request = [](std::initializer_list<DWORD> keys, bool party) {
		Scene scene;
		scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_NPC_ASK);
		scene.View().SetRequestMode(MRequestMode::REQUEST_INFO);
		scene.systems.SetFlag(party ? 1u << Systems::SYSTEM_PARTY : 0u);
		scene.Keys(keys);
		scene.Run();
		CHECK(scene.View().IsDrawItemNameList());
		// The request block unmarks the sector.
		CHECK_EQ(int(SECTORPOSITION_NULL), int(scene.View().m_SelectSector.x));
		return int(scene.View().GetRequestMode());
	};
	CHECK_EQ(int(MRequestMode::REQUEST_TRADE), Request({ DIK_LMENU }, false));
	CHECK_EQ(int(MRequestMode::REQUEST_PARTY), Request({ DIK_RMENU }, true));
	CHECK_EQ(int(MRequestMode::REQUEST_NULL), Request({ DIK_RMENU }, false));
	CHECK_EQ(int(MRequestMode::REQUEST_INFO), Request({ DIK_RCONTROL }, false));
	// Left Alt first, then right Alt; right Alt without the party system
	// asks for nothing, even with right Ctrl.
	CHECK_EQ(int(MRequestMode::REQUEST_TRADE), Request({ DIK_LMENU, DIK_RMENU }, true));
	CHECK_EQ(int(MRequestMode::REQUEST_TRADE), Request({ DIK_LMENU, DIK_RCONTROL }, false));
	CHECK_EQ(int(MRequestMode::REQUEST_PARTY), Request({ DIK_RMENU, DIK_RCONTROL }, true));
	CHECK_EQ(int(MRequestMode::REQUEST_NULL), Request({ DIK_RMENU, DIK_RCONTROL }, false));
}

TEST(GameUpdate2Names, InAnotherModeTheRequestEndsAndNoneStarts)
{
	Scene scene;
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_NPC_ASK);
	scene.temp.SetMode(TempInformation::MODE_SHOP_BUY);
	scene.View().SetRequestMode(MRequestMode::REQUEST_INFO);
	scene.Keys({ DIK_LMENU });
	scene.Run();
	CHECK(scene.View().IsDrawItemNameList());
	CHECK_EQ(int(MRequestMode::REQUEST_NULL), int(scene.View().GetRequestMode()));
	CHECK_EQ(int(SECTORPOSITION_NULL), int(scene.View().m_SelectSector.x));
	scene.temp.SetMode(TempInformation::MODE_NULL);
}

TEST(GameUpdate2Names, WatchingRepeatingOrAsABatOrAWolfTheNamesShowAndTheRequestStaysAsItWas)
{
	for (int why = 0; why < 4; ++why)
	{
		Scene scene;
		scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_NPC_ASK);
		if (why == 0) g_bWatchMode = true;
		if (why == 1) scene.me->SetRepeatAction();
		if (why == 2) scene.PlayerType(CREATURETYPE_BAT);
		if (why == 3) scene.PlayerType(CREATURETYPE_WOLF);
		scene.View().SetRequestMode(MRequestMode::REQUEST_INFO);
		scene.Keys({ DIK_LMENU });
		scene.Run();
		CHECK(scene.View().IsDrawItemNameList());
		CHECK_EQ(int(MRequestMode::REQUEST_INFO), int(scene.View().GetRequestMode()));
		CHECK(scene.Marked());
	}
}

TEST(GameUpdate2Names, WithoutTheKeysTheNamesAndTheRequestGo)
{
	Scene scene;
	scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_NPC_ASK);
	scene.View().SetDrawItemNameList();
	scene.View().SetRequestMode(MRequestMode::REQUEST_PARTY);
	scene.Run();
	CHECK(!scene.View().IsDrawItemNameList());
	CHECK_EQ(int(MRequestMode::REQUEST_NULL), int(scene.View().GetRequestMode()));
	CHECK(scene.Marked());
}

//======================================================================
// What stops the input
//======================================================================
TEST(GameUpdate2Stop, WaitingForTheServerLayingAMineOrMovingFastStopsIt)
{
	for (int why = 0; why < 3; ++why)
	{
		Scene scene;
		if (why == 0) scene.me->SetWaitVerify(MPlayer::WAIT_VERIFY_SKILL_SUCCESS);
		if (why == 1) gbl_mine_progress = true;
		if (why == 2) scene.me->m_bFastMove = true;
		scene.Keys({ DIK_CAPITAL });
		scene.Buttons(true, false, false, false, false, false);
		scene.Run();
		CHECK(Same({ "ui", "ui" }));
		CHECK(!g_bLButtonDown);
		CHECK(!scene.me->IsLockMode());
		CHECK(scene.Marked());
		CHECK(scene.Selected(kSelected, kSelected, kSelected));
		CHECK(scene.SelectorUntouched());
		CHECK_EQ(Pointer::CURSOR_COIN, scene.Cursor());
	}
}

TEST(GameUpdate2Stop, ARepeatingActionUnmarksTheSectorAndStopsIt)
{
	Scene scene;
	scene.me->SetRepeatAction();
	scene.Keys({ DIK_CAPITAL });
	scene.Buttons(true, false, false, false, false, false);
	scene.Run();
	CHECK_EQ(int(SECTORPOSITION_NULL), int(scene.View().m_SelectSector.x));
	CHECK_EQ(int(SECTORPOSITION_NULL), int(scene.View().m_SelectSector.y));
	CHECK(!g_bLButtonDown);
	CHECK(!scene.me->IsLockMode());
	CHECK(scene.Selected(kSelected, kSelected, kSelected));
	CHECK(scene.SelectorUntouched());
}

TEST(GameUpdate2Stop, TheUIHoldingTheInputUnselectsAndStopsIt)
{
	for (int why = 0; why < 3; ++why)
	{
		Scene scene;
		if (why == 0) g_bUIInput = TRUE;
		if (why == 1) UIDialog::SetLockInputPCTalk();
		if (why == 2) UIDialog::SetLockInputMessage();
		scene.Keys({ DIK_CAPITAL });
		scene.Buttons(true, false, false, false, false, false);
		scene.Run();
		CHECK(scene.Selected(kNone, kNone, kNone));
		CHECK(scene.Marked());
		CHECK(!g_bLButtonDown);
		CHECK(!scene.me->IsLockMode());
		CHECK(scene.SelectorUntouched());
	}
}

TEST(GameUpdate2Stop, DeadThePlayerSelectsAndMarksNothingAndStops)
{
	Scene scene;
	scene.me->m_bAlive = false;
	scene.Keys({ DIK_CAPITAL });
	scene.Buttons(true, false, false, false, false, false);
	scene.Run();
	CHECK(scene.Selected(kNone, kNone, kNone));
	CHECK_EQ(int(SECTORPOSITION_NULL), int(scene.View().m_SelectSector.x));
	CHECK(!g_bLButtonDown);
	CHECK(!scene.me->IsLockMode());
	CHECK(scene.SelectorUntouched());
	scene.me->m_bAlive = true;
}

//======================================================================
// Whom the mouse may pick
//======================================================================
namespace {
// The selector after one tick of the scene as the setup leaves it.
std::pair<int, int> Picks(const std::function<void(Scene&)>& setup)
{
	Scene scene;
	setup(scene);
	scene.Run();
	return { int(scene.selector.m_SelectTarget), int(scene.selector.m_SelectBy) };
}
const int kAll = MObjectSelector::SELECT_ALL, kFriend = MObjectSelector::SELECT_FRIEND, kEnemy = MObjectSelector::SELECT_ENEMY;
const int kByRace = MObjectSelector::SELECT_BY_RACE, kByGuild = MObjectSelector::SELECT_BY_GUILD;
// The skill the player has chosen, for the users the flag says.
void Chosen(Scene& scene, int skill, BYTE user)
{
	scene.actions.GetMutable(skill)->SetUser(user);
	scene.me->SetSpecialActionInfo(TYPE_ACTIONINFO(skill));
}
} // namespace

TEST(GameUpdate2Select, EveryoneOnAForcedAttackInASafePlaceOrWithASkillOfThePlayersRace)
{
	using P = std::pair<int, int>;
	CHECK(P(kEnemy, kByRace) == Picks([](Scene&) {}));
	CHECK(P(kAll, kByRace) == Picks([](Scene& s) { s.Keys({ DIK_LSHIFT }); }));
	CHECK(P(kAll, kByRace) == Picks([](Scene& s) { s.me->Set(EFFECTSTATUS_HALLUCINATION); }));
	CHECK(P(kAll, kByRace) == Picks([](Scene&) { g_bZoneSafe = true; }));
	// At war the sectors are not safe, the zone's flag still is.
	const auto War = [](Scene& s) { GuildWarInfo* pWar = new GuildWarInfo; pWar->setCastleID(60); s.wars.SetWar(pWar); };
	CHECK(P(kAll, kByRace) == Picks([&](Scene& s) { War(s); g_bZoneSafe = true; }));
	CHECK(P(kEnemy, kByRace) == Picks([&](Scene& s) { War(s); s.Sector(20, 15).SetSafe(FLAG_SECTOR_SAFE_SLAYER); }));
	CHECK(P(kAll, kByRace) == Picks([](Scene& s) { s.Sector(20, 15).SetSafe(FLAG_SECTOR_SAFE_SLAYER); }));
	// The slayers' safe sector is not a vampire's.
	CHECK(P(kEnemy, kByGuild) == Picks([](Scene& s) { s.PlayerType(kVampire); s.Sector(20, 15).SetSafe(FLAG_SECTOR_SAFE_SLAYER); }));
	// A skill for the player's race alone; one for more users is not.
	CHECK(P(kAll, kByRace) == Picks([](Scene& s) { Chosen(s, SKILL_DOUBLE_IMPACT, FLAG_ACTIONINFO_USER_SLAYER); }));
	CHECK(P(kEnemy, kByRace) == Picks([](Scene& s) { Chosen(s, SKILL_DOUBLE_IMPACT, FLAG_ACTIONINFO_USER_SLAYER | FLAG_ACTIONINFO_USER_VAMPIRE); }));
	CHECK(P(kEnemy, kByRace) == Picks([](Scene& s) { Chosen(s, SKILL_DOUBLE_IMPACT, FLAG_ACTIONINFO_USER_VAMPIRE); }));
	CHECK(P(kAll, kByGuild) == Picks([](Scene& s) { s.PlayerType(kVampire); Chosen(s, SKILL_DOUBLE_IMPACT, FLAG_ACTIONINFO_USER_VAMPIRE); }));
	CHECK(P(kEnemy, kByGuild) == Picks([](Scene& s) { s.PlayerType(kVampire); Chosen(s, SKILL_DOUBLE_IMPACT, FLAG_ACTIONINFO_USER_SLAYER); }));
	CHECK(P(kAll, kByRace) == Picks([](Scene& s) { s.PlayerType(kOusters); Chosen(s, SKILL_DOUBLE_IMPACT, FLAG_ACTIONINFO_USER_OUSTERS); }));
	CHECK(P(kEnemy, kByRace) == Picks([](Scene& s) { s.PlayerType(kOusters); Chosen(s, SKILL_DOUBLE_IMPACT, FLAG_ACTIONINFO_USER_SLAYER); }));
	// Intimate Grail, for any user and any race.
	CHECK(P(kAll, kByGuild) == Picks([](Scene& s) { s.PlayerType(kVampire); Chosen(s, SKILL_INTIMATE_GRAIL, 0); }));
	CHECK(P(kAll, kByRace) == Picks([](Scene& s) { Chosen(s, SKILL_INTIMATE_GRAIL, 0); }));
}

TEST(GameUpdate2Select, FriendsWithLeftCtrlOrARequestElseEnemies)
{
	using P = std::pair<int, int>;
	CHECK(P(kFriend, kByRace) == Picks([](Scene& s) { s.Keys({ DIK_LCONTROL }); }));
	CHECK(P(kFriend, kByRace) == Picks([](Scene& s) { s.Keys({ DIK_RCONTROL }); }));		// asking for info
	CHECK(P(kFriend, kByRace) == Picks([](Scene& s) { s.Keys({ DIK_LMENU }); }));		// asking for a trade
	CHECK(P(kFriend, kByRace) == Picks([](Scene& s) { s.Keys({ DIK_RMENU }); }));		// asking for a party
	// Right Alt with no party system starts no request.
	CHECK(P(kEnemy, kByRace) == Picks([](Scene& s) { s.systems.SetFlag(0); s.Keys({ DIK_RMENU }); }));
	CHECK(P(kAll, kByRace) == Picks([](Scene& s) { s.Keys({ DIK_LCONTROL, DIK_LSHIFT }); }));
	// A request a test left set does not last into the tick.
	CHECK(P(kEnemy, kByRace) == Picks([](Scene& s) { s.View().SetRequestMode(MRequestMode::REQUEST_INFO); }));
}

TEST(GameUpdate2Select, SlayersAndOustersByRaceVampiresByGuild)
{
	using P = std::pair<int, int>;
	CHECK(P(kEnemy, kByRace) == Picks([](Scene& s) { s.PlayerType(kOusters); }));
	CHECK(P(kEnemy, kByGuild) == Picks([](Scene& s) { s.PlayerType(kVampire); }));
	CHECK(P(kEnemy, kByGuild) == Picks([](Scene& s) { s.PlayerType(CREATURETYPE_BAT); }));
}

//======================================================================
// What is picked
//======================================================================
TEST(GameUpdate2Pick, WhileTheNamesShowAnItemsNameUnderTheMouseComesFirst)
{
	Scene scene;
	scene.Creature(300, kSlayer);		// a friend under the mouse
	MItem* pItem = scene.Item(600, ITEM_CLASS_SWORD, 12, 12);
	RECT away = { 0, 0, 1, 1 };
	pItem->SetScreenRect(&away);
	DRAWITEMNAME_NODE* pName = new DRAWITEMNAME_NODE(1, pItem);
	pName->SetRect(Over());
	scene.View().AddItemName(pName);
	// Right Ctrl shows the names (and asks for info, so friends are picked).
	scene.Keys({ DIK_RCONTROL });
	scene.Run();
	CHECK(scene.Selected(kNone, 600, kNone));
	// Its name away from the mouse: what is under it.
	pName->SetRect(away);
	scene.Mark();
	scene.Run();
	CHECK(scene.Selected(300, kNone, kNone));
	// The names hidden: the name is not looked at.
	pName->SetRect(Over());
	scene.Mark();
	scene.Keys({ DIK_LCONTROL });
	scene.Run();
	CHECK(scene.Selected(300, kNone, kNone));
	// The name of an item the zone no longer has: what is under the mouse.
	std::unique_ptr<MItem> pGone(NewItemOfClass(ITEM_CLASS_SWORD));
	pGone->SetID(601);
	DRAWITEMNAME_NODE* pGoneName = new DRAWITEMNAME_NODE(0, pGone.get());
	pGoneName->SetRect(Over());
	scene.View().AddItemName(pGoneName);
	scene.Mark();
	scene.Keys({ DIK_RCONTROL });
	scene.Run();
	CHECK(scene.Selected(300, kNone, kNone));
	scene.View().ClearItemNameList();
}

//======================================================================
// Watching
//======================================================================
TEST(GameUpdate2Watch, TheScreensEdgesMoveTheWatchedPlayerAndNothingElseHappens)
{
	// The edges are a fixed 800x600 screen's, 10 pixels in, not
	// g_GameRect's.
	const struct { int x, y, dx, dy; } cases[] = {
		{ 9, 300, -1, 0 }, { 10, 300, 0, 0 }, { 790, 300, 0, 0 }, { 791, 300, 1, 0 },
		{ 400, 9, 0, -1 }, { 400, 10, 0, 0 }, { 400, 590, 0, 0 }, { 400, 591, 0, 1 },
		{ 0, 0, -1, -1 }, { 799, 599, 1, 1 }, { 0, 599, -1, 1 },
	};
	for (const auto& c : cases)
	{
		Scene scene;
		g_bWatchMode = true;
		g_x = c.x;
		g_y = c.y;
		scene.Keys({ DIK_CAPITAL });
		scene.Buttons(true, false, false, false, false, false);
		scene.Run();
		CHECK_EQ(20 + c.dx, int(scene.me->GetX()));
		CHECK_EQ(15 + c.dy, int(scene.me->GetY()));
		CHECK(!g_bLButtonDown);
		CHECK(!scene.me->IsLockMode());
		CHECK(scene.Selected(kSelected, kSelected, kSelected));
		CHECK_EQ(Pointer::CURSOR_COIN, scene.Cursor());
		CHECK(scene.Marked());
	}
	// It stays in the zone.
	const struct { int fromX, fromY, x, y; } edges[] = {
		{ 0, 0, 0, 0 }, { 39, 29, 799, 599 }, { 39, 0, 799, 0 }, { 0, 29, 0, 599 },
	};
	for (const auto& e : edges)
	{
		Scene scene;
		g_bWatchMode = true;
		scene.me->SetPosition(TYPE_SECTORPOSITION(e.fromX), TYPE_SECTORPOSITION(e.fromY));
		g_x = e.x;
		g_y = e.y;
		scene.Run();
		CHECK_EQ(e.fromX, int(scene.me->GetX()));
		CHECK_EQ(e.fromY, int(scene.me->GetY()));
	}
}

//======================================================================
// The lock
//======================================================================
TEST(GameUpdate2Lock, CapsLockLocksOnWithTheAttackCursorAndTheTicksPressHoldsItsButton)
{
	// Left before right before centre; the input stops at the lock.
	const struct { bool lb, rb, cb, l, r, c; } cases[] = {
		{ true, false, false, true, false, false },
		{ false, true, false, false, true, false },
		{ false, false, true, false, false, true },
		{ true, true, false, true, false, false },
		{ true, false, true, true, false, false },
		{ false, true, true, false, true, false },
		{ false, false, false, false, false, false },
	};
	for (const auto& c : cases)
	{
		Scene scene;
		scene.me->m_Action = ACTION_MOVE;		// so standing still unmarks nothing
		Skill(scene, MAGIC_LIGHT, FLAG_ACTIONINFO_TARGET_SELF, FLAG_ACTIONINFO_USER_ALL);	// a right press would use it
		g_bPreviousMove = false;
		const int nextX = scene.me->m_NextDestX;
		scene.Keys({ DIK_CAPITAL });
		scene.Buttons(c.lb, false, c.rb, false, c.cb, false);
		scene.Run();
		CHECK(scene.me->IsLockMode());
		CHECK_EQ(Pointer::CURSOR_SLAYER_ATTACK, scene.Cursor());
		CHECK_EQ(c.l, bool(g_bLButtonDown));
		CHECK_EQ(c.r, bool(g_bRButtonDown));
		CHECK_EQ(c.c, bool(g_bCButtonDown));
		CHECK_EQ(int(SECTORPOSITION_NULL), int(scene.View().m_SelectSector.x));
		// Nothing more: no selection, no walk.
		CHECK(scene.Selected(kSelected, kSelected, kSelected));
		CHECK_EQ(nextX, int(scene.me->m_NextDestX));
		CHECK(!g_bPreviousMove);
		CHECK(!scene.me->IsRepeatAction());
	}
}

TEST(GameUpdate2Lock, InACasketOrUndergroundCapsLockDoesNotLock)
{
	for (int why = 0; why < 2; ++why)
	{
		Scene scene;
		if (why == 0) scene.me->m_bInCasket = true;
		else scene.me->SetMoveType(MCreature::CREATURE_UNDERGROUND);
		scene.me->SetLockMode();
		scene.Keys({ DIK_CAPITAL });
		scene.Buttons(true, false, false, false, false, false);
		scene.Run();
		// Nothing held: the lock it had ends.
		CHECK(!scene.me->IsLockMode());
		CHECK(!g_bLButtonDown);
		CHECK_EQ(Pointer::CURSOR_SLAYER_NORMAL, scene.Cursor());
		scene.me->m_bInCasket = false;
	}
}

TEST(GameUpdate2Lock, WithoutCapsLockTheLockLastsWhileLeftOrRightIsHeld)
{
	for (int held = 0; held < 4; ++held)
	{
		Scene scene;
		scene.me->SetLockMode();
		g_bLButtonDown = held == 0;
		g_bRButtonDown = held == 1;
		g_bCButtonDown = held == 2;
		scene.Run();
		const bool locked = held < 2;
		CHECK_EQ(locked, scene.me->IsLockMode());
		// Locked, the tick ends there; unlocked it goes on to the selection.
		if (locked) CHECK(scene.Selected(kSelected, kSelected, kSelected));
		else CHECK(scene.Selected(kNone, kNone, kNone));
	}
}

//======================================================================
// The selection and the cursor
//======================================================================
TEST(GameUpdate2Cursor, OverNothingTheSelectionGoesAndTheCursorIsTheDefaultOrWithShiftTheAttack)
{
	{
		Scene scene;
		scene.Run();
		CHECK(scene.Selected(kNone, kNone, kNone));
		CHECK_EQ(Pointer::CURSOR_SLAYER_NORMAL, scene.Cursor());
		// The default is the interface's race's.
		g_eRaceInterface = RACE_VAMPIRE;
		scene.Run();
		CHECK_EQ(Pointer::CURSOR_VAMPIRE_NORMAL, scene.Cursor());
	}
	{
		Scene scene;
		scene.Keys({ DIK_LSHIFT });
		scene.Run();
		CHECK(scene.Selected(kNone, kNone, kNone));
		CHECK_EQ(Pointer::CURSOR_SLAYER_ATTACK, scene.Cursor());
	}
}

TEST(GameUpdate2Cursor, OverASectorThePlayerCannotEnterTheCursorSaysSoUnlessOverAPortal)
{
	enum { GROUND, UNDER, FLYING };
	const struct { int player, block; bool notMove; } cases[] = {
		{ GROUND, GROUND, true }, { GROUND, UNDER, false }, { GROUND, FLYING, false },
		{ UNDER, UNDER, true }, { UNDER, GROUND, false },
		{ FLYING, FLYING, true }, { FLYING, GROUND, false },
	};
	for (const auto& c : cases)
		for (int portal = 0; portal < 2; ++portal)
		{
			Scene scene;
			scene.me->SetMoveType(c.player == GROUND ? MCreature::CREATURE_GROUND
				: c.player == UNDER ? MCreature::CREATURE_UNDERGROUND : MCreature::CREATURE_FLYING);
			MSector& sector = scene.Sector(10, 10);
			if (c.block == GROUND) sector.SetBlockGround();
			else if (c.block == UNDER) sector.SetBlockUnderground();
			else sector.SetBlockFlying();
			// A portal to a zone the zone table does not know: the view sets
			// no portal cursor of its own.
			if (portal) CHECK(sector.AddPortal(MPortal::TYPE_NOMAL, 70));
			scene.Run();
			CHECK_EQ(bool(portal), g_bMouseInPortal);
			CHECK(scene.Selected(kNone, kNone, kNone));
			CHECK_EQ(portal ? Pointer::CURSOR_COIN : c.notMove ? Pointer::CURSOR_NOT_MOVE : Pointer::CURSOR_SLAYER_NORMAL, scene.Cursor());
		}
	// Off the zone no sector is read: the default. (A missing bounds check
	// would read past the zone's rows, which are separate allocations no
	// in-zone sector aliases: only ASan sees that, so only its tree does.)
	const struct { int sx, sy; bool in; } edges[] = { { 39, 10, true }, { 40, 10, false }, { 10, 29, true }, { 10, 30, false } };
	for (const auto& e : edges)
	{
		Scene scene;
		scene.Sector(e.in ? e.sx : 39, e.in ? e.sy : 29).SetBlockGround();
		g_x = PX(e.sx);
		g_y = PY(e.sy);
		scene.Run();
		CHECK_EQ(e.sx, int(g_MouseSector.x));
		CHECK_EQ(e.sy, int(g_MouseSector.y));
		CHECK_EQ(e.in ? Pointer::CURSOR_NOT_MOVE : Pointer::CURSOR_SLAYER_NORMAL, scene.Cursor());
	}
}

TEST(GameUpdate2Cursor, OverACreatureItIsSelectedWithTheAttackCursorIfItMayBeAttacked)
{
	{
		Scene scene;
		scene.Creature(300, kVampire);		// an enemy
		scene.Run();
		CHECK(scene.Selected(300, kNone, kNone));
		CHECK_EQ(Pointer::CURSOR_SLAYER_ATTACK, scene.Cursor());
	}
	{
		// An NPC that may be attacked (a forced attack): the default.
		Scene scene;
		scene.Creature(301, kNPC);
		forceAttack = true;
		scene.Run();
		CHECK(scene.Selected(301, kNone, kNone));
		CHECK_EQ(Pointer::CURSOR_SLAYER_NORMAL, scene.Cursor());
		forceAttack = false;
	}
	{
		// A friend (picked with left Ctrl) may not be: the cursor stays.
		Scene scene;
		scene.Creature(302, kSlayer);
		scene.Keys({ DIK_LCONTROL });
		scene.Run();
		CHECK(scene.Selected(302, kNone, kNone));
		CHECK_EQ(Pointer::CURSOR_COIN, scene.Cursor());
	}
	{
		// Over a portal the enemy is selected and the cursor stays.
		Scene scene;
		scene.Creature(303, kVampire);
		CHECK(scene.Sector(10, 10).AddPortal(MPortal::TYPE_NOMAL, 70));
		scene.Run();
		CHECK(scene.Selected(303, kNone, kNone));
		CHECK_EQ(Pointer::CURSOR_COIN, scene.Cursor());
	}
}

namespace {
// The colours a test knows the item names by.
void NameColours()
{
	g_pClientConfig->COLOR_NAME_ITEM = RGB(1, 2, 3);
	g_pClientConfig->COLOR_NAME_ITEM_OPTION = RGB(4, 5, 6);
	g_pClientConfig->COLOR_NAME_ITEM_RARE_OPTION = RGB(7, 8, 9);
	g_pClientConfig->COLOR_NAME_VAMPIRE = RGB(10, 11, 12);
}
} // namespace

TEST(GameUpdate2Cursor, OverAnItemItIsSelectedWithThePickUpCursorItsNameAndAColourByItsOptions)
{
	const COLORREF colours[] = { RGB(1, 2, 3), RGB(4, 5, 6), RGB(7, 8, 9), RGB(10, 11, 12), RGB(10, 11, 12) };
	for (int options = 0; options < 5; ++options)
	{
		Scene scene;
		NameColours();
		MItem* pItem = scene.Item(600);
		std::list<TYPE_ITEM_OPTION> list;
		for (int i = 0; i < options; ++i) list.push_back(TYPE_ITEM_OPTION(i + 1));
		pItem->SetItemOptionList(list);
		scene.Run();
		CHECK(scene.Selected(kNone, 600, kNone));
		CHECK_EQ(Pointer::CURSOR_SLAYER_PICKUP, scene.Cursor());
		CHECK(std::string("Blade") == scene.CursorText());
		CHECK_EQ(colours[options], scene.CursorColor());
	}
	{
		// The name is cut to 79 characters.
		Scene scene;
		MItem* pItem = scene.Item(600);
		pItem->SetName(std::string(100, 'x').c_str());
		scene.Run();
		CHECK(std::string(79, 'x') == scene.CursorText());
	}
	{
		// A corpse (picked by its body) shows no name.
		Scene scene;
		NameColours();
		MCreature* pBody = new MCreature;
		pBody->SetCreatureType(kVampire);
		RECT over = Over();
		pBody->SetScreenRect(&over);
		MItem* pItem = scene.Item(602, ITEM_CLASS_CORPSE, 10, 10, pBody);
		RECT away = { 0, 0, 1, 1 };
		pItem->SetScreenRect(&away);
		scene.Run();
		CHECK(scene.Selected(kNone, 602, kNone));
		CHECK_EQ(Pointer::CURSOR_SLAYER_PICKUP, scene.Cursor());
		CHECK(std::string() == scene.CursorText());
		CHECK_EQ(RGB(1, 2, 3), scene.CursorColor());
	}
}

TEST(GameUpdate2Cursor, AQuestItemsNameTakesItsColourSetsSeventeenthShade)
{
	// Recorded defect: the 5:6:5 shade's six green bits are shifted by 3
	// like the five-bit red and blue, and RGB() keeps the low byte, so a
	// green of 32 or more wraps (32 << 3 = 256 shows as 0); << 2 is the
	// six-bit scale.
	const struct { WORD shade; COLORREF colour; } cases[] = {
		{ WORD((16 << 11) | (31 << 5) | 8), RGB(128, 248, 64) },
		{ WORD((16 << 11) | (32 << 5) | 8), RGB(128, 0, 64) },
		{ WORD((31 << 11) | (63 << 5) | 31), RGB(248, 248, 248) },
	};
	for (const auto& c : cases)
	{
		Scene scene;
		NameColours();
		MItem* pItem = scene.Item(600);
		pItem->SetQuestFlag();
		// Frame 3 of the 28-frame cycle: shade set 100 + 3.
		g_CurrentFrame = 3;
		g_pClientConfig->QuestItemColorSet = 100;
		const WORD old = CIndexSprite::ColorSet[103][16];
		CIndexSprite::ColorSet[103][16] = c.shade;
		scene.Run();
		CIndexSprite::ColorSet[103][16] = old;
		CHECK(scene.Selected(kNone, 600, kNone));
		CHECK_EQ(c.colour, scene.CursorColor());
	}
}

TEST(GameUpdate2Cursor, AnItemIsSelectedWithoutThePickUpCursorOverAPortalOrForAPlayerInDarkness)
{
	for (int why = 0; why < 3; ++why)
	{
		Scene scene;
		scene.Item(600);
		if (why == 0) CHECK(scene.Sector(10, 10).AddPortal(MPortal::TYPE_NOMAL, 70));
		if (why == 1)
		{
			scene.PlayerType(kVampire);			// a vampire sees from darkness
			scene.me->m_DarknessCount = 3;
		}
		if (why == 2)
		{
			scene.me->Set(EFFECTSTATUS_LIGHTNESS);	// so does a lit slayer
			scene.me->m_DarknessCount = 3;
		}
		scene.Run();
		CHECK(scene.Selected(kNone, 600, kNone));
		CHECK_EQ(Pointer::CURSOR_COIN, scene.Cursor());
	}
}

TEST(GameUpdate2Cursor, AnItemOnADarkSectorIsSelectedOnlyByThoseWhoSeeInIt)
{
	// Recorded defect: an item darkness hides is not selected, but nothing
	// unselects either, so the last tick's selection stays on.
	const struct { int type; bool light, ghost; int zone; bool selected; } cases[] = {
		{ kSlayer, false, false, 60, false },
		{ kSlayer, true, false, 60, true },
		{ kSlayer, true, false, 3001, false },
		{ kSlayer, false, true, 3001, true },
		{ kOusters, false, false, 60, false },
		{ kOusters, true, false, 60, true },
		{ kVampire, false, false, 60, true },
	};
	for (const auto& c : cases)
	{
		Scene scene;
		scene.Item(600);
		scene.Sector(10, 10).SetDarkness();
		scene.PlayerType(c.type);
		scene.me->Set(EFFECTSTATUS_LIGHTNESS, c.light);
		scene.me->Set(EFFECTSTATUS_GHOST, c.ghost);
		scene.SetZoneID(c.zone);
		scene.Run();
		if (c.selected)
		{
			CHECK(scene.Selected(kNone, 600, kNone));
			CHECK_EQ(Pointer::CURSOR_SLAYER_PICKUP, scene.Cursor());
		}
		else
		{
			CHECK(scene.Selected(kSelected, kSelected, kSelected));
			CHECK_EQ(Pointer::CURSOR_COIN, scene.Cursor());
		}
	}
	// A vampire in zone 3001 is not even shown the item by the view.
	Scene scene;
	scene.Item(600);
	scene.Sector(10, 10).SetDarkness();
	scene.PlayerType(kVampire);
	scene.SetZoneID(3001);
	scene.Run();
	CHECK(scene.Selected(kNone, kNone, kNone));
}

TEST(GameUpdate2Cursor, OverAnEffectItIsSelectedAndTheCursorStays)
{
	Scene scene;
	Pickable effect;
	RECT over = Over();
	effect.SetScreenRect(&over);
	scene.zone->m_mapGroundEffect[effect.GetID()] = &effect;
	scene.Run();
	CHECK(scene.Selected(kNone, kNone, effect.GetID()));
	CHECK_EQ(Pointer::CURSOR_COIN, scene.Cursor());
	scene.zone->m_mapGroundEffect.clear();
}

//======================================================================
// Acting: Shift
//======================================================================
namespace {
// The unburrow the player sends from where it stands.
std::vector<unsigned char> Unburrow(const MPlayer& me)
{
	CGUnburrow expected;
	expected.setX(Coord_t(me.GetX()));
	expected.setY(Coord_t(me.GetY()));
	expected.setDir(me.GetDirection());
	return Encoded({ &expected });
}
const std::string kSendUnburrow = "send|" + N(CGUnburrow().getPacketID());
} // namespace

TEST(GameUpdate2Act, NothingIsDoneInTheDelayOrThePauseBeyondTheSelection)
{
	for (int why = 0; why < 2; ++why)
		for (int press = 0; press < 3; ++press)
		{
			Scene scene;
			if (why == 0) scene.me->SetDelay(500);
			else scene.me->Set(EFFECTSTATUS_ETERNITY_PAUSE);
			scene.me->SetMoveType(MCreature::CREATURE_UNDERGROUND);	// a right press would unburrow
			scene.me->SetRequestMode(MRequestMode::REQUEST_INFO);
			const int nextX = scene.me->m_NextDestX;
			// A left press, a right press, or the left held after a walk.
			if (press == 2) g_bLButtonDown = TRUE;
			g_bPreviousMove = press == 2;
			scene.Buttons(press == 0, false, press == 1, false, false, false);
			scene.Run();
			CHECK_EQ(press == 2, bool(g_bLButtonDown));
			CHECK(!g_bRButtonDown);
			CHECK_EQ(press == 2, g_bPreviousMove);
			CHECK_EQ(nextX, int(scene.me->m_NextDestX));
			CHECK(g_SelectSector.x == kMarked.x && g_SelectSector.y == kMarked.y);
			CHECK_EQ(int(MRequestMode::REQUEST_INFO), int(scene.me->GetRequestMode()));
			CHECK(scene.wire.Sent().empty());
			// The selection and the cursor came first.
			CHECK(scene.Selected(kNone, kNone, kNone));
			CHECK_EQ(Pointer::CURSOR_SLAYER_NORMAL, scene.Cursor());
		}
}

TEST(GameUpdate2Act, ShiftAndALeftPressForceAttackTheCreatureUnderTheMouse)
{
	{
		// A friend, struck: the forced attack repeats.
		Scene scene;
		scene.Creature(300, kSlayer);
		scene.me->SetRequestMode(MRequestMode::REQUEST_INFO);
		g_bPreviousMove = true;
		scene.Keys({ DIK_LSHIFT });
		scene.Buttons(true, false, false, false, false, false);
		scene.Run();
		CHECK(g_bLButtonDown);
		CHECK(scene.me->IsTraceCreature());
		CHECK_EQ(300, int(scene.me->GetTraceID()));
		CHECK(scene.me->m_bTraceCreatureToForceAttack);
		CHECK(!scene.me->m_bKeepTraceCreature);
		CHECK(scene.me->IsRepeatAction());
		CHECK(!g_bPreviousMove);
		CHECK_EQ(int(MRequestMode::REQUEST_NULL), int(scene.me->GetRequestMode()));
		CHECK_EQ(int(SECTORPOSITION_NULL), int(scene.View().m_SelectSector.x));
		CHECK(g_SelectSector.x == kMarked.x && g_SelectSector.y == kMarked.y);
	}
	// Nothing, or an item, under the mouse: the request ends, nothing more.
	for (int item = 0; item < 2; ++item)
	{
		Scene scene;
		if (item) scene.Item(600);
		scene.me->SetRequestMode(MRequestMode::REQUEST_INFO);
		g_bPreviousMove = true;
		scene.Keys({ DIK_LSHIFT });
		scene.Buttons(true, false, false, false, false, false);
		scene.Run();
		CHECK(!g_bLButtonDown);
		CHECK(scene.me->IsTraceNULL());
		CHECK(g_bPreviousMove);
		CHECK_EQ(int(MRequestMode::REQUEST_NULL), int(scene.me->GetRequestMode()));
	}
	// In a casket, underground or behind a turret: not even that.
	for (int why = 0; why < 3; ++why)
	{
		Scene scene;
		scene.Creature(300, kVampire);
		if (why == 0) scene.me->m_bInCasket = true;
		if (why == 1) scene.me->SetMoveType(MCreature::CREATURE_UNDERGROUND);
		if (why == 2) scene.me->Set(EFFECTSTATUS_INSTALL_TURRET);
		scene.me->SetRequestMode(MRequestMode::REQUEST_INFO);
		scene.Keys({ DIK_LSHIFT });
		scene.Buttons(true, false, false, false, false, false);
		scene.Run();
		CHECK(!g_bLButtonDown);
		CHECK(scene.me->IsTraceNULL());
		CHECK_EQ(int(MRequestMode::REQUEST_INFO), int(scene.me->GetRequestMode()));
		scene.me->m_bInCasket = false;
	}
}

TEST(GameUpdate2Act, ARightPressUsesTheSkillAndShiftForcesIt)
{
	// Burrowed, the skill is coming up out of the ground, whatever the key.
	for (int shift = 0; shift < 2; ++shift)
	{
		Scene scene;
		scene.me->SetMoveType(MCreature::CREATURE_UNDERGROUND);
		scene.me->SetRequestMode(MRequestMode::REQUEST_INFO);
		g_bPreviousMove = true;
		if (shift) scene.Keys({ DIK_LSHIFT });
		scene.Buttons(false, false, true, false, false, false);
		scene.Run();
		CHECK(Count(kSendUnburrow) == 1);
		CHECK(scene.wire.Sent() == Unburrow(*scene.me));
		CHECK(scene.me->GetWaitVerify() == MPlayer::WAIT_VERIFY_SKILL_SUCCESS);
		CHECK(g_bRButtonDown);
		CHECK(!g_bPreviousMove);
		CHECK_EQ(int(MRequestMode::REQUEST_NULL), int(scene.me->GetRequestMode()));
	}
	// A skill for other races, on one of the player's own (a player): only
	// Shift forces it on. (Left Ctrl, which picks friends, does not.)
	for (int shift = 0; shift < 2; ++shift)
	{
		Scene scene;
		scene.Creature(300, kSlayerPlayer);
		Skill(scene, SKILL_DOUBLE_IMPACT, FLAG_ACTIONINFO_TARGET_OTHER, FLAG_ACTIONINFO_USER_VAMPIRE | FLAG_ACTIONINFO_USER_OUSTERS);
		scene.Keys({ shift ? DWORD(DIK_LSHIFT) : DWORD(DIK_LCONTROL) });
		scene.Buttons(false, false, true, false, false, false);
		scene.Run();
		CHECK(scene.Selected(300, kNone, kNone));
		CHECK(g_bRButtonDown);
		CHECK_EQ(shift == 1, scene.me->m_fTrace == FLAG_TRACE_CREATURE_SPECIAL);
		CHECK_EQ(shift == 1 ? 300 : int(OBJECTID_NULL), int(scene.me->GetTraceID()));
		CHECK_EQ(shift == 1, bool(scene.me->IsRepeatAction()));
	}
}

//======================================================================
// Acting: a left press
//======================================================================
TEST(GameUpdate2Act, ALeftPressOnNothingWalksToTheSectorUnderTheMouse)
{
	{
		Scene scene;
		scene.me->SetRequestMode(MRequestMode::REQUEST_INFO);
		scene.Buttons(true, false, false, false, false, false);
		scene.Run();
		CHECK(g_bLButtonDown);
		CHECK_EQ(10, int(g_SelectSector.x));
		CHECK_EQ(10, int(g_SelectSector.y));
		CHECK_EQ(10, int(scene.me->m_NextDestX));
		CHECK_EQ(10, int(scene.me->m_NextDestY));
		CHECK_EQ(int(scene.me->m_MoveAction), int(scene.me->GetNextAction()));
		CHECK(scene.me->IsTraceNULL());
		CHECK(g_bPreviousMove);
		CHECK_EQ(int(MRequestMode::REQUEST_NULL), int(scene.me->GetRequestMode()));
		CHECK_EQ(int(SECTORPOSITION_NULL), int(scene.View().m_SelectSector.x));
	}
	{
		// Moving already, or mid-action: the press unmarks the sector
		// itself, and the walk ends the trace it had.
		Scene scene;
		scene.me->m_Action = ACTION_MOVE;
		scene.me->m_ActionCount = 0;
		scene.me->m_ActionCountMax = 10;
		scene.me->m_fTrace = FLAG_TRACE_ITEM;
		scene.Buttons(true, false, false, false, false, false);
		scene.Run();
		CHECK_EQ(10, int(scene.me->m_NextDestX));
		CHECK(scene.me->IsTraceNULL());
		CHECK_EQ(int(SECTORPOSITION_NULL), int(scene.View().m_SelectSector.x));
	}
	// With left Ctrl, or onto where it stands, the press goes nowhere.
	for (int why = 0; why < 2; ++why)
	{
		Scene scene;
		const int nextX = scene.me->m_NextDestX;
		const int nextAction = scene.me->GetNextAction();
		if (why == 0) scene.Keys({ DIK_LCONTROL });
		else { g_x = PX(20); g_y = PY(15); }
		scene.Buttons(true, false, false, false, false, false);
		scene.Run();
		CHECK(g_bLButtonDown);
		CHECK_EQ(why == 0 ? 10 : 20, int(g_SelectSector.x));
		CHECK_EQ(nextX, int(scene.me->m_NextDestX));
		CHECK_EQ(nextAction, int(scene.me->GetNextAction()));
		CHECK(!g_bPreviousMove);
	}
}

TEST(GameUpdate2Act, NoLeftPressInACasketUndergroundOrKeepingAPersonalShop)
{
	for (int why = 0; why < 5; ++why)
	{
		Scene scene;
		if (why == 0) scene.me->m_bInCasket = true;
		if (why == 1) scene.me->SetMoveType(MCreature::CREATURE_UNDERGROUND);
		if (why == 2) scene.me->SetPersnalShop(1);
		if (why == 3) scene.me->SetPersnalShop(2);
		if (why == 4) scene.me->SetPersnalShop(3);		// no such shop state: it walks
		scene.me->SetRequestMode(MRequestMode::REQUEST_INFO);
		const int nextX = scene.me->m_NextDestX;
		scene.Buttons(true, false, false, false, false, false);
		scene.Run();
		const bool walks = why == 4;
		CHECK_EQ(walks, bool(g_bLButtonDown));
		CHECK_EQ(walks, g_bPreviousMove);
		CHECK_EQ(walks ? 10 : nextX, int(scene.me->m_NextDestX));
		CHECK_EQ(walks ? 10 : int(kMarked.x), int(g_SelectSector.x));
		CHECK_EQ(int(walks ? MRequestMode::REQUEST_NULL : MRequestMode::REQUEST_INFO), int(scene.me->GetRequestMode()));
		scene.me->m_bInCasket = false;
	}
}

TEST(GameUpdate2Act, ALeftPressBehindATurretTurnsThePlayerTowardTheMouseAndNothingMore)
{
	const struct { int direction; bool stopped; int expected; } cases[] = {
		{ DIRECTION_DOWN, true, DIRECTION_LEFT },
		{ DIRECTION_LEFT, true, DIRECTION_LEFT },
		{ DIRECTION_DOWN, false, DIRECTION_DOWN },		// busy: no turn
	};
	for (const auto& c : cases)
	{
		Scene scene;
		// Even an enemy there, under the mouse.
		RECT over = { PX(10) - 2, PY(15) - 2, PX(10) + 3, PY(15) + 3 };
		scene.Creature(300, kVampire, 10, 15)->SetScreenRect(&over);
		scene.me->Set(EFFECTSTATUS_INSTALL_TURRET);
		scene.me->m_CurrentDirection = BYTE(c.direction);
		if (!c.stopped)
		{
			scene.me->m_Action = ACTION_ATTACK;
			scene.me->m_ActionCount = 0;
			scene.me->m_ActionCountMax = 10;
		}
		const int nextX = scene.me->m_NextDestX;
		g_x = PX(10);
		g_y = PY(15);
		scene.Buttons(true, false, false, false, false, false);
		scene.Run();
		CHECK(scene.Selected(300, kNone, kNone));
		CHECK_EQ(c.expected, int(scene.me->GetDirection()));
		CHECK(g_bLButtonDown);
		CHECK_EQ(10, int(g_SelectSector.x));
		CHECK_EQ(15, int(g_SelectSector.y));
		CHECK_EQ(nextX, int(scene.me->m_NextDestX));
		CHECK(scene.me->IsTraceNULL());
		CHECK(!g_bPreviousMove);
	}
}

TEST(GameUpdate2Act, ALeftPressOnACreatureAttacksAnEnemyAndFollowsAFriendUnlessForced)
{
	{
		Scene scene;
		scene.Creature(300, kVampire);
		scene.me->SetRequestMode(MRequestMode::REQUEST_INFO);
		g_bPreviousMove = true;
		scene.Buttons(true, false, false, false, false, false);
		scene.Run();
		CHECK(g_bLButtonDown);
		CHECK(scene.me->IsTraceCreature());
		CHECK_EQ(300, int(scene.me->GetTraceID()));
		CHECK(scene.me->m_bTraceCreatureToForceAttack);
		CHECK(scene.me->IsRepeatAction());
		CHECK(!g_bPreviousMove);
		CHECK_EQ(int(SECTORPOSITION_NULL), int(scene.View().m_SelectSector.x));
		CHECK_EQ(10, int(g_SelectSector.x));
		CHECK_EQ(int(OBJECTID_NULL), int(gC_vs_ui.GetOtherObjectID()));
	}
	{
		// A friend (picked with left Ctrl) is followed: nothing repeats.
		Scene scene;
		scene.Creature(301, kSlayer);
		g_bPreviousMove = true;
		scene.Keys({ DIK_LCONTROL });
		scene.Buttons(true, false, false, false, false, false);
		scene.Run();
		CHECK_EQ(301, int(scene.me->GetTraceID()));
		CHECK(!scene.me->m_bTraceCreatureToForceAttack);
		CHECK(scene.me->m_bKeepTraceCreature);
		CHECK(!scene.me->IsRepeatAction());
		CHECK(g_bPreviousMove);
	}
	{
		// Hallucinating, everyone is struck.
		Scene scene;
		scene.Creature(302, kSlayer);
		scene.me->Set(EFFECTSTATUS_HALLUCINATION);
		scene.Buttons(true, false, false, false, false, false);
		scene.Run();
		CHECK_EQ(302, int(scene.me->GetTraceID()));
		CHECK(scene.me->m_bTraceCreatureToForceAttack);
		CHECK(scene.me->IsRepeatAction());
	}
}

TEST(GameUpdate2Act, ALeftPressOnAPersonalShopAsksTheUIForItsStock)
{
	for (int shop = 0; shop < 3; ++shop)
	{
		Scene scene;
		// The UI's results come out of its base, drained by the test. Its
		// Init starts the IME's cursor blinker, the timer after this one.
		const RECT oldRect = g_GameRect;
		g_GameRect = {0, 0, 10, 8};
		CSpriteSurface back;
		back.Init(10, 8);
		timer_id_t probe = gC_timer2.Add(1000, NoTimer);
		timer_id_t blink = probe + 1;
		gC_timer2.Delete(probe);
		scene.base.Init(&back, Received);
		MCreature* pShop = scene.Creature(300, kVampire);
		pShop->SetPersnalShop(shop);
		scene.Buttons(true, false, false, false, false, false);
		scene.Run();
		for (int i = 0; i < 4; ++i) scene.base.DispatchMessage();
		const std::string asked = "ui message|" + N(UI_REQUEST_STORE_INFO) + "|300|0";
		CHECK_EQ(shop == 1 ? 1 : 0, Count(asked));
		CHECK_EQ(shop == 1 ? 300 : int(OBJECTID_NULL), int(gC_vs_ui.GetOtherObjectID()));
		// It is attacked all the same.
		CHECK_EQ(300, int(scene.me->GetTraceID()));
		CHECK(gC_timer2.Delete(blink));
		g_SetFL2Surface(nullptr);
		g_GameRect = oldRect;
	}
}

TEST(GameUpdate2Act, ALeftPressOnAnItemOrAnEffectGoesToItAndLetsTheButtonGo)
{
	{
		Scene scene;
		scene.Item(600);
		scene.me->SetRequestMode(MRequestMode::REQUEST_INFO);
		g_bPreviousMove = true;
		scene.Buttons(true, false, false, false, false, false);
		scene.Run();
		CHECK(scene.me->IsTraceItem());
		CHECK_EQ(600, int(scene.me->GetTraceID()));
		CHECK(!g_bLButtonDown);
		CHECK(!g_bPreviousMove);
		CHECK_EQ(int(MRequestMode::REQUEST_NULL), int(scene.me->GetRequestMode()));
		CHECK_EQ(10, int(g_SelectSector.x));
	}
	{
		Scene scene;
		Pickable effect;
		RECT over = Over();
		effect.SetScreenRect(&over);
		scene.zone->m_mapGroundEffect[effect.GetID()] = &effect;
		g_bPreviousMove = true;
		scene.Buttons(true, false, false, false, false, false);
		scene.Run();
		CHECK(scene.me->IsTraceEffect());
		CHECK_EQ(int(effect.GetID()), int(scene.me->GetTraceID()));
		CHECK(!g_bLButtonDown);
		CHECK(!g_bPreviousMove);
		scene.zone->m_mapGroundEffect.clear();
	}
}

TEST(GameUpdate2Act, TheLeftHeldAfterAWalkKeepsWalkingToTheMouse)
{
	const struct { bool held, walked, shift, creature, walks; } cases[] = {
		{ true, true, false, false, true },
		{ true, false, false, false, false },		// the press did not walk
		{ false, true, false, false, false },		// not held
		{ true, true, true, false, false },			// with Shift
		{ true, true, false, true, false },			// over a creature
	};
	for (const auto& c : cases)
	{
		Scene scene;
		if (c.creature) scene.Creature(300, kVampire);
		g_bLButtonDown = c.held;
		g_bPreviousMove = c.walked;
		scene.me->m_fTrace = FLAG_TRACE_ITEM;		// what it was doing ends
		scene.me->m_ActionCount = 0;				// mid-action: the walk itself ends no trace
		scene.me->m_ActionCountMax = 10;
		if (c.shift) scene.Keys({ DIK_LSHIFT });
		const int nextX = scene.me->m_NextDestX;
		const int nextAction = scene.me->GetNextAction();
		scene.Run();
		CHECK_EQ(c.walks ? 10 : nextX, int(scene.me->m_NextDestX));
		CHECK_EQ(c.walks ? 10 : int(kMarked.x), int(g_SelectSector.x));
		CHECK_EQ(c.walks ? int(scene.me->m_MoveAction) : nextAction, int(scene.me->GetNextAction()));
		CHECK_EQ(c.walks, bool(scene.me->IsTraceNULL()));
		CHECK_EQ(c.held, bool(g_bLButtonDown));
		CHECK_EQ(c.walked, g_bPreviousMove);
	}
}

//======================================================================
// The releases of the tick
//======================================================================
TEST(GameUpdate2Release, ALeftClickWithinOneTickWalksThenMarksWhereItGoes)
{
	for (int standing = 0; standing < 2; ++standing)
	{
		Scene scene;
		if (!standing) scene.me->m_Action = ACTION_MOVE;
		scene.Buttons(true, true, false, false, false, false);
		scene.Run();
		CHECK(!g_bLButtonDown);
		CHECK(g_bPreviousMove);
		CHECK_EQ(10, int(scene.me->m_NextDestX));
		// The mark is the walk's; standing still takes it off again.
		CHECK_EQ(standing ? int(SECTORPOSITION_NULL) : 10, int(scene.View().m_SelectSector.x));
		CHECK_EQ(standing ? int(SECTORPOSITION_NULL) : 10, int(scene.View().m_SelectSector.y));
	}
}

TEST(GameUpdate2Release, AShiftClickWithinOneTickEndsTheRepeatItStarted)
{
	Scene scene;
	scene.Creature(300, kVampire);
	g_bPreviousMove = true;
	scene.Keys({ DIK_LSHIFT });
	scene.Buttons(true, true, false, false, false, false);
	scene.Run();
	CHECK(!g_bLButtonDown);
	CHECK_EQ(300, int(scene.me->GetTraceID()));
	CHECK(!scene.me->IsRepeatAction());
	CHECK(!g_bPreviousMove);
}

TEST(GameUpdate2Release, ARightClickWithinOneTickEndsTheSkillItRepeated)
{
	for (int released = 0; released < 2; ++released)
	{
		Scene scene;
		Skill(scene, MAGIC_LIGHT, FLAG_ACTIONINFO_TARGET_SELF, FLAG_ACTIONINFO_USER_ALL);
		scene.Buttons(false, false, true, released == 1, false, false);
		scene.Run();
		CHECK_EQ(released == 0, bool(g_bRButtonDown));
		CHECK_EQ(released == 0, bool(scene.me->IsRepeatAction()));
	}
}

TEST(GameUpdate2Release, AButtonTheLockHoldsIsLetGoByItsReleaseInTheSameTick)
{
	for (int button = 0; button < 3; ++button)
		for (int released = 0; released < 2; ++released)
		{
			Scene scene;
			scene.Keys({ DIK_CAPITAL });
			scene.Buttons(button == 0, button == 0 && released, button == 1, button == 1 && released, button == 2, button == 2 && released);
			scene.Run();
			CHECK(scene.me->IsLockMode());
			CHECK_EQ(button == 0 && !released, bool(g_bLButtonDown));
			CHECK_EQ(button == 1 && !released, bool(g_bRButtonDown));
			CHECK_EQ(button == 2 && !released, bool(g_bCButtonDown));
		}
}

TEST(GameUpdate2Release, StandingStillUnmarksTheSector)
{
	for (int standing = 0; standing < 2; ++standing)
	{
		Scene scene;
		scene.me->m_Action = standing ? ACTION_STAND : ACTION_MOVE;
		scene.Run();
		CHECK_EQ(standing ? int(SECTORPOSITION_NULL) : int(kMarked.x), int(scene.View().m_SelectSector.x));
		CHECK_EQ(standing ? int(SECTORPOSITION_NULL) : int(kMarked.y), int(scene.View().m_SelectSector.y));
	}
}
