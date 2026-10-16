#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "CGameUpdate.h"
#include "ClientMode.h"
#include "ClientPlayer.h"
#include "GameSocket.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketOutputStream.h"
#include "Cpackets/CGVerifyTime.h"
#include "FrameClock.h"
#include "InputState.h"
#include "AmbientSoundState.h"
#include "MonotonicClock.h"
#include "MEventQueue.h"
#include "MEventManager.h"
#include "MParty.h"
#include "MPlayer.h"
#include "MTopView.h"
#include "MZone.h"
#include "MEffect.h"
#include "MScreenEffectManager.h"
#include "MWeather.h"
#include "MGameTime.h"
#include "MZoneSoundManager.h"
#include "MWarManager.h"
#include "MSkillManager.h"
#include "TempInformation.h"
#include "UserOption.h"
#include "WorldEventsHost.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "UiRuntime.h"
#include "VS_UI.h"
#include "VS_UI_Base.h"
#include "VS_UI_mouse_pointer.h"
#include "pi_core.h"
#include "u_window.h"
#include "Ci.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackend.h"
#include "SpriteLibBackendSDL.h"
#include "DXLib/CDirectDraw.h"
#include "CDirectInput.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "EffectSpriteTypeDef.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MActionInfoTable.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "MViewDef.h"
#include "WorldTileGeometry.h"
#include "TextSystem/TextService.h"
#include "TextSystem/RenderTargetSpriteSurface.h"
#include "../../basic/Platform.h"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// What CGameUpdate.cpp keeps of its own (task 4.261 moved it into
// gameclient), and the view's globals the frame publishes.
extern int		g_MyCheckTime;
extern int		g_CheckErrorTime;
extern int		g_CheckTimeNum;
extern bool		g_bInterpolateDraw;
extern int		g_DrawCamGapX;
extern int		g_DrawCamGapY;

// The game's frame loop, CGameUpdate::Update and UpdateDraw (task 4.261:
// the executable's until then, gameclient's since).
//
// Update, outside the game or without a configuration, does nothing but
// start its tick clock (the first call ever, in any mode). In the game:
// - a due logout with the player waiting for it logs out and ends the
//   frame (not run here: ExecuteLogout starts the title screen);
// - once a second (strictly after the next check) the count of sounds
//   played is reset;
// - it follows the cursor (UpdateMouse); a tick delay other than 62
//   sends 100 CGVerifyTime and puts it back to 62; a counter runs from
//   0 to 31 and round again;
// - 62 ms after the last tick it ticks, and keeps ticking while it is
//   still a whole delay behind, at most MAX_UPDATE_ONETIME times. Each
//   tick counts the frame, reads the socket (a failure, or a mode it
//   changed, ends the frame), reads and handles the input while the game
//   is active, runs the UI, stops when the application is inactive or
//   the mode changed, writes the socket (likewise), then moves the
//   weather, the inventory's effects, the events and the zone, and
//   counts the delay as done. A burst that used up its ticks and is
//   still behind counts itself; past MAX_UPDATE_ONETIME_COUNT such
//   bursts in a row the clock gives up on the lost time. Not a tick: the
//   frame did not change;
// - a game hour other than the last clears the view's shadows and, on an
//   even hour, plays the zone's music; the game clock is set from the
//   frame time (only with a string table) and the time and the date go
//   to the UI;
// - the zone's sounds are updated and the player's position goes to the
//   UI;
// - the fraction of the tick passed since the last tick (0..256) is
//   published, and a tick or 15 ms since the last draw draws, counting
//   the frame.
// UpdateDraw gives the UI the cursor, draws the world biased by the
// player's interpolation gap (shaken while a shake event shows, the step
// advancing once per tick), shows the UI, draws the pointer unless the
// view asked to draw its own, prints the frame rate when asked, copies
// the last frame to the back buffer, steps a dead player's fade and
// resurrection request, draws the pointer again and flips.
//
// The host records what it is asked, the UI is the real one with a
// window of the test's over it, the pointer is asked for but not drawn
// (the runtime says not to: its sprite needs the game's data), and the
// weather, the inventory's effects, the events, the zone and its sounds
// record their being moved. The zone is a real one with a view drawing
// it onto a screen of its own, so a tick can run. The clocks are the
// test's.
//
// Windows' speed check (Update's `#ifdef PLATFORM_WINDOWS` blocks) is
// tested under the same guard, so only the Windows CI runs it.
namespace {

std::string calls;
bool socketInputOk = true;
bool socketOutputOk = true;
// What happens right after a call, as the test says.
std::vector<std::pair<std::string, std::function<void()>>> triggers;
void When(const std::string& call, std::function<void()> action) { triggers.emplace_back(call, std::move(action)); }
int cursorX = 40, cursorY = 50;	// where UpdateMouse puts the cursor
POINT windowPoint = {3, 4};		// where GetCursorPoint finds it

void Call(const std::string& call)
{
	calls += call + ";";
	for (const auto& [name, action] : triggers)
		if (name == call) action();
}

const CGameUpdateHost host{
	.UpdateMouse = []() { g_x = cursorX; g_y = cursorY; Call("mouse"); },
	.UpdateInput = []() { Call("input"); },
	.UpdateSocketInput = []() { Call("in"); return socketInputOk; },
	.UpdateSocketOutput = []() { Call("out"); return socketOutputOk; },
	.SetMode = [](CLIENT_MODE mode) { Call("mode" + std::to_string(static_cast<int>(mode))); },
	.GetCursorPoint = [](POINT& point) { point = windowPoint; Call("cursor"); },
};

std::string Message(UINT message)
{
	switch (message)
	{
		case M_MOVING: return "move";
		case M_LEFTBUTTON_DOWN: return "ldown";
	}
	return "mouse" + std::to_string(message);
}
std::string At(int x, int y) { return "@" + std::to_string(x) + "," + std::to_string(y); }

bool Same(const std::string& expected, const std::string& actual)
{
	if (expected != actual)
		std::fprintf(stderr, "    expected %s\n    actual   %s\n", expected.c_str(), actual.c_str());
	return expected == actual;
}
#define CHECK_SAME(expected, actual) CHECK(Same((expected), (actual)))

// A window over the whole screen; the UI hands it the mouse, and it
// answers that it took it.
struct UiWindow : Window
{
	UiWindow() : Window(0, 0, 800, 600) {}
	bool IsPixel(int, int) override { return true; }
	bool MouseControl(UINT message, int x, int y) override { Call(Message(message) + At(x, y)); return true; }
	void Show() override { Call("show"); }
	void Process() override { Call("process"); }
};

WORD Pixel(CSpriteSurface& surface, int x, int y)
{
	spritectl_surface_info_t info{};
	if (spritectl_lock_surface(surface.GetBackendSurface(), &info) != 0 || !info.pixels) return 0;
	const WORD value = reinterpret_cast<WORD*>(static_cast<BYTE*>(info.pixels) + y * info.pitch)[x];
	spritectl_unlock_surface(surface.GetBackendSurface());
	return value;
}

std::vector<WORD> Picture(CSpriteSurface& surface)
{
	std::vector<WORD> pixels;
	spritectl_surface_info_t info{};
	if (spritectl_lock_surface(surface.GetBackendSurface(), &info) != 0 || !info.pixels) return pixels;
	for (int y = 0; y < info.height; ++y)
		for (int x = 0; x < info.width; ++x)
			pixels.push_back(reinterpret_cast<WORD*>(static_cast<BYTE*>(info.pixels) + y * info.pitch)[x]);
	spritectl_unlock_surface(surface.GetBackendSurface());
	return pixels;
}

void Pattern(CSpriteSurface& surface)
{
	spritectl_surface_info_t info{};
	if (spritectl_lock_surface(surface.GetBackendSurface(), &info) != 0 || !info.pixels) return;
	for (int y = 0; y < info.height; ++y)
		for (int x = 0; x < info.width; ++x)
			reinterpret_cast<WORD*>(static_cast<BYTE*>(info.pixels) + y * info.pitch)[x] = WORD(1 + ((y * info.width + x) % 0x7FFF));
	spritectl_unlock_surface(surface.GetBackendSurface());
}

// The pointer is asked for (and not drawn); "pointer/back" once the last
// frame has been copied to the back buffer.
bool EventFlag(std::uint32_t flag)
{
	if (flag != EVENTFLAG_NOT_DRAW_MOUSE_POINTER) return false;
	const bool copied = g_pBack != NULL && g_pLast != NULL && Pixel(*g_pBack, 0, 0) == Pixel(*g_pLast, 0, 0);
	Call(copied ? "pointer/back" : "pointer");
	return true;
}
const UiRuntime::Host uiRuntime{ .EventFlagActive = EventFlag };

spritectl_surface_t Flipped() { Call("flip"); return SPRITECTL_INVALID_SURFACE; }

const WorldEvents::Host worldHost = []() {
	WorldEvents::Host host{};
	host.PlayMusicCurrentZone = []() { Call("music"); };
	return host;
}();

// The weather asks the screen's size for each drop it makes.
const MWeatherHost weatherHost{
	.ReadOrigin = [](int& x, int& y) { x = y = 0; return true; },
	.Width = []() { if (calls.size() < 8 || calls.compare(calls.size() - 8, 8, "weather;") != 0) Call("weather"); return 80; },
	.Height = []() { return 60; },
};

// Rain whose next drops are due at its next move.
struct Rain : MWeather
{
	Rain() { SetRain(20); m_MoreEffectCount = 0; }
};

// An effect event asks whether the player still has its status.
const MEventHost eventHost{ .HasEffectStatus = [](DWORD) { Call("events"); return true; } };

const ZoneSoundHost zoneSoundHost = []() {
	ZoneSoundHost host{};
	host.PlayerPosition = [](int&, int&) { Call("zone sound"); return false; };
	return host;
}();

const ZoneState::Host zoneHost = []() {
	ZoneState::Host host{};
	host.Exists = []() { return g_pZone != NULL; };
	host.GetID = []() { return g_pZone != NULL ? int(g_pZone->GetID()) : int(ZONEID_NULL); };
	host.GetCreature = [](TYPE_OBJECTID id) { return g_pZone->GetCreature(id); };
	host.GetWidth = []() { return g_pZone != NULL ? int(g_pZone->GetWidth()) : 0; };
	host.GetHeight = []() { return g_pZone != NULL ? int(g_pZone->GetHeight()) : 0; };
	host.GetSector = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) -> const MSector* { return &g_pZone->GetSector(x, y); };
	host.GetFakeCreature = [](TYPE_OBJECTID id) { return g_pZone != NULL ? g_pZone->GetFakeCreature(id) : NULL; };
	return host;
}();
const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.Creature = []() -> MCreature* { return g_pPlayer; };
	host.GetID = []() { return g_pPlayer != NULL ? g_pPlayer->GetID() : TYPE_OBJECTID(OBJECTID_NULL); };
	host.HasEffectStatus = [](EFFECTSTATUS status) { return g_pPlayer != NULL && g_pPlayer->HasEffectStatus(status); };
	return host;
}();
const CreatureState::Host creatureHost = []() {
	CreatureState::Host host{};
	host.ObjectOf = [](const MCreature* pCreature) -> MObject* { return const_cast<MCreature*>(pCreature); };
	host.CreatureOf = [](MObject* pObject) { return static_cast<MCreature*>(pObject); };
	return host;
}();
const MEffectHost effectHost = []() {
	MEffectHost host{};
	host.CurrentFrame = []() -> DWORD { return g_CurrentFrame; };
	return host;
}();

// An effect that records its being moved, and is never drawn.
struct Recorder : MEffect
{
	std::string name;
	explicit Recorder(const std::string& what) : MEffect(BLT_EFFECT), name(what) { m_bDrawSkip = true; }
	bool Update() override { Call(name); return true; }
};

// The game connection: what is sent is kept, serialised, in its output
// stream.
struct Wire : ClientPlayer
{
	Wire() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~Wire() { setPlayerStatus(CPS_END_SESSION); }
	std::vector<unsigned char> Sent() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};

// The packets as one stream sends them.
std::vector<unsigned char> Encoded(const std::vector<const Packet*>& packets)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	for (const Packet* pPacket : packets) encoded.write(pPacket);
	return SocketOutputStreamTestAccess::Bytes(encoded);
}
std::vector<unsigned char> VerifyTimes(int count)
{
	CGVerifyTime packet;
	return Encoded(std::vector<const Packet*>(size_t(count), &packet));
}

struct ZoneProbe : MZone
{
	using MZone::MZone;
	using MZone::m_mapGroundEffect;
};

struct PlayerProbe : MPlayer
{
	using MCreature::m_sX;
	using MCreature::m_sY;
	using MCreature::m_bAlive;
	using MCreature::m_ActionCount;
	using MCreature::m_ActionCountMax;
	using MPlayer::m_DeadDelayTime;
	void At(int x, int y) { MObject::SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y)); }
};

struct Probe : CGameUpdate
{
	Probe() : CGameUpdate(&host) {}
	using CGameUpdate::m_bOutputInfo;
	using CGameUpdate::m_ShakeStep;
	using CGameUpdate::m_DeadStep;
	using CGameUpdate::m_DeadStepFrames;
	using CGameUpdate::m_DeadStepTime;
	using CGameUpdate::m_LastTickTime;
	using CGameUpdate::m_NextSoundCheckTime;
	using CGameUpdate::m_OnetimeUpdateCount;
	using CGameUpdate::m_LastHour;
	using CGameUpdate::m_NextVerifyTime;
	using CGameUpdate::m_LastDrawTime;
};

MonotonicClock::TimePoint now;
MonotonicClock::TimePoint Now() { return now; }
const MonotonicClock::TimePoint kOrigin = MonotonicClock::TimePoint() + MonotonicClock::Millis(5000000);
MonotonicClock::TimePoint After(MonotonicClock::TimePoint t, int ms) { return t + MonotonicClock::Duration(ms); }

// No frame presented yet: a window point is a game point. The geometry
// has no getter, so a scene leaves it so too, as the process starts.
void NothingPresented() { spritectl_set_present_geometry(0, 0, 0, 0, 0, 0, 0, 0, 0, 0); }

// Everything the loop, the view and the UI read, saved, and given back
// after.
struct Saved
{
	MonotonicClock::TimePoint frameNow = g_FrameNow;
	int updateDelay = g_UpdateDelay;
	BOOL activeApp = g_bActiveApp;
	BOOL activeGame = g_bActiveGame;
	CLIENT_MODE mode = g_Mode;
	int frameCount = g_FrameCount;
	DWORD currentFrame = g_CurrentFrame;
	bool frameChanged = g_bFrameChanged;
	DWORD alpha = g_DrawAlphaNum;
	int frameRate = g_FrameRate;
	int soundPerSecond = g_SoundPerSecond;
	int checkTime = g_MyCheckTime, checkError = g_CheckErrorTime, checkNum = g_CheckTimeNum;
	bool interpolate = g_bInterpolateDraw;
	int gapX = g_DrawCamGapX, gapY = g_DrawCamGapY;
	int x = g_x, y = g_y;
	BOOL lButton = g_bLButtonDown, rButton = g_bRButtonDown, cButton = g_bCButtonDown;
	CSpriteSurface* back = g_pBack;
	CSpriteSurface* last = g_pLast;
	RECT gameRect = g_GameRect;
	CSDLInput* input = g_pSDLInput;
	WindowManager* windows = gpC_window_manager;
	PI_ProcessRunner* runner = gpC_process_runner;
	Base* base = gpC_base;
	C_VS_UI_MOUSE_POINTER* pointer = gpC_mouse_pointer;
	UserOption* option = g_pUserOption;
	MParty* party = g_pParty;
	CI* ime = gC_ci;
	MEventQueue* events = g_pEventManager;
	MTopView* view = g_pTopView;
	MZone* zone = g_pZone;
	MPlayer* player = g_pPlayer;
	ClientPlayer* socket = g_pSocket;
	MGameTime* gameTime = g_pGameTime;
	MZoneSoundManager* zoneSounds = g_pZoneSoundManager;
	MWeather* weather = g_pWeather;
	MScreenEffectManager* inventoryEffects = g_pInventoryEffectManager;
	TempInformation* temp = g_pTempInformation;
	MWarManager* war = g_pWarManager;
	MSkillSet* skills = g_pSkillAvailable;
	CREATURE_TABLE* creatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE* sprites = g_pCreatureSpriteTable;
	EFFECTSPRITETYPE_TABLE* effectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* effectStatuses = g_pEffectStatusTable;
	MActionInfoTable* actions = g_pActionInfoTable;
	MonsterNameTable* monsterNames = g_pMonsterNameTable;
	MLevelNameTable* levelNames = g_pLevelNameTable;
	RankBonusTable* rankBonuses = g_pRankBonusTable;
	AmbientSoundState ambient = g_ZoneAmbientSounds;
	const UiRuntime::Host* runtime = UiRuntime::SetHost(&uiRuntime);
	const WorldEvents::Host* world = WorldEvents::SetHost(&worldHost);
	const MWeatherHost* weatherHostWas = MWeather::SetHost(&weatherHost);
	const ZoneSoundHost* zoneSoundHostWas = ZONESOUND_NODE::SetHost(&zoneSoundHost);
	const ZoneState::Host* zoneHostWas = ZoneState::SetHost(&zoneHost);
	const PlayerState::Host* playerHostWas = PlayerState::SetHost(&playerHost);
	const CreatureState::Host* creatureHostWas = CreatureState::SetHost(&creatureHost);
	const MEffectHost* effectHostWas = MEffect::SetHost(&effectHost);
	MonotonicClock::ScopedTestSource clock{Now};
	~Saved()
	{
		NothingPresented();
		MEffect::SetHost(effectHostWas);
		CreatureState::SetHost(creatureHostWas);
		PlayerState::SetHost(playerHostWas);
		ZoneState::SetHost(zoneHostWas);
		ZONESOUND_NODE::SetHost(zoneSoundHostWas);
		MWeather::SetHost(weatherHostWas);
		WorldEvents::SetHost(world);
		CSDLGraphics::SetBackBufferSource(nullptr);
		UiRuntime::SetHost(runtime);
		g_ZoneAmbientSounds = ambient;
		g_pRankBonusTable = rankBonuses;
		g_pLevelNameTable = levelNames;
		g_pMonsterNameTable = monsterNames;
		g_pActionInfoTable = actions;
		g_pEffectStatusTable = effectStatuses;
		g_pEffectSpriteTypeTable = effectSprites;
		g_pCreatureSpriteTable = sprites;
		g_pCreatureTable = creatureTypes;
		g_pSkillAvailable = skills;
		g_pWarManager = war;
		g_pTempInformation = temp;
		g_pInventoryEffectManager = inventoryEffects;
		g_pWeather = weather;
		g_pZoneSoundManager = zoneSounds;
		g_pGameTime = gameTime;
		g_pSocket = socket;
		g_pPlayer = player;
		g_pZone = zone;
		g_pTopView = view;
		g_pEventManager = events;
		gC_ci = ime;
		g_pParty = party;
		g_pUserOption = option;
		gpC_mouse_pointer = pointer;
		gpC_base = base;
		gpC_process_runner = runner;
		gpC_window_manager = windows;
		g_pSDLInput = input;
		g_GameRect = gameRect;
		g_pLast = last;
		g_pBack = back;
		g_bCButtonDown = cButton;
		g_bRButtonDown = rButton;
		g_bLButtonDown = lButton;
		g_y = y;
		g_x = x;
		g_DrawCamGapY = gapY;
		g_DrawCamGapX = gapX;
		g_bInterpolateDraw = interpolate;
		g_CheckTimeNum = checkNum;
		g_CheckErrorTime = checkError;
		g_MyCheckTime = checkTime;
		g_SoundPerSecond = soundPerSecond;
		g_FrameRate = frameRate;
		g_DrawAlphaNum = alpha;
		g_bFrameChanged = frameChanged;
		g_CurrentFrame = currentFrame;
		g_FrameCount = frameCount;
		g_Mode = mode;
		g_bActiveGame = activeGame;
		g_bActiveApp = activeApp;
		g_UpdateDelay = updateDelay;
		g_FrameNow = frameNow;
	}
};

const int kZoneWidth = 60, kZoneHeight = 80;
const int kPlayerX = 30, kPlayerY = 40;

// The game at frame time kOrigin, before the loop's first call: a 60x80
// zone with the player standing in its middle, a view drawing it onto a
// screen of its own, the last frame an 800x600 pattern, the back buffer
// 0x7777, the delay 62 ms, the cursor at (40, 50) (the window's point
// (3, 4)), the game inactive (no input read), the weather idle, no game
// time, inventory effects or zone sounds; the zone moves a recording
// ground effect, and an event keeps the view from writing its text (its
// fonts and sprites need the game's data).
struct Scene
{
	Saved saved;
	GameModelWorld world;
	PI_ProcessRunner runner;
	WindowManager windows;
	Base base;
	C_VS_UI_MOUSE_POINTER pointer;
	CSDLInput input;
	UserOption option;
	CI_KOREAN ime;
	CSpriteSurface back, last, screen;
	CSpriteSurface* pScreen = &screen;
	std::unique_ptr<UiWindow> window;
	CREATURE_TABLE creatureTypes;
	CREATURESPRITE_TABLE sprites;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	MActionInfoTable actions;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	RankBonusTable rankBonuses;
	MEventQueue events{&eventHost};
	TempInformation temp;
	MWarManager war;
	MWeather weather;		// idle
	MSkillSet skills;
	Wire wire;
	Recorder zoneEffect{"zone"};
	std::unique_ptr<ZoneProbe> zone;
	PlayerProbe* player = nullptr;
	std::unique_ptr<MTopView> view;
	Probe update;

	Scene()
	{
		calls.clear();
		socketInputOk = socketOutputOk = true;
		triggers.clear();
		cursorX = 40;
		cursorY = 50;
		windowPoint = {3, 4};
		NothingPresented();
		now = kOrigin;
		g_FrameNow = kOrigin;

		gpC_process_runner = &runner;
		gpC_window_manager = &windows;
		gpC_base = &base;
		gpC_mouse_pointer = &pointer;
		g_pSDLInput = &input;
		g_pUserOption = &option;
		g_pParty = NULL;
		gC_ci = &ime;
		CHECK_EQ(0, spritectl_init());
		CHECK(back.Init(800, 600));
		CHECK(last.Init(800, 600));
		CHECK(screen.Init(800, 600));
		back.FillSurface(0x7777);
		Pattern(last);
		screen.FillSurface(0);
		g_pBack = &back;
		g_pLast = &last;
		base.m_p_DDSurface_back = &back;
		g_GameRect = {0, 0, 800, 600};
		CSDLGraphics::SetBackBufferSource(Flipped);
		window = std::make_unique<UiWindow>();
		window->Start();
		windows.AppearWindow(window.get());

		creatureTypes.Init(4);
		CREATURETABLE_INFO& slayer = testfw::MutableRow(creatureTypes, 1);
		slayer.SetCreatureTribe(CREATURETRIBE_SLAYER, ACTION_MAX_VAMPIRE);
		slayer.SpriteTypes.Init(1);
		*slayer.SpriteTypes.GetMutable(0) = 0;
		g_pCreatureTable = &creatureTypes;
		sprites.Init(4);
		g_pCreatureSpriteTable = &sprites;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		actions.Init(MAX_ACTIONINFO);
		g_pActionInfoTable = &actions;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		g_pRankBonusTable = &rankBonuses;
		g_pEventManager = &events;
		Event(EVENTID_QUEST_FIN, EVENTFLAG_NOT_DRAW_INFORMATION);
		g_pTempInformation = &temp;
		g_pWarManager = &war;
		g_pSocket = &wire;
		g_pGameTime = NULL;
		g_pZoneSoundManager = NULL;
		g_pWeather = &weather;	// the view draws it
		g_pInventoryEffectManager = NULL;

		zone.reset(new ZoneProbe(TYPE_SECTORPOSITION(kZoneWidth), TYPE_SECTORPOSITION(kZoneHeight)));
		zone->m_mapGroundEffect[zoneEffect.GetID()] = &zoneEffect;
		g_pZone = zone.get();
		player = new PlayerProbe;
		g_pPlayer = player;
		g_pSkillAvailable = NULL;	// the player's type change would refresh it
		player->SetCreatureType(1);
		g_pSkillAvailable = &skills;
		player->SetID(1000);
		player->At(kPlayerX, kPlayerY);
		player->SnapshotDrawState();	// no interpolation gap
		view.reset(new MTopView);
		view->SetSurface(pScreen);
		view->SetZone(zone.get());
		view->m_pTileSurface = new CSpriteSurface;
		CHECK(view->m_pTileSurface->Init(g_TILESURFACE_WIDTH, g_TILESURFACE_HEIGHT));
		view->m_pTileRenderer = new TileRenderer;
		CHECK(view->m_pTileRenderer->Init(view->m_pTileSurface, &view->m_TileSPK));
		view->m_bFirstTileDraw = true;
		g_pTopView = view.get();

		g_Mode = MODE_GAME;
		g_UpdateDelay = 62;
		g_bActiveApp = TRUE;
		g_bActiveGame = FALSE;
		g_bLButtonDown = g_bRButtonDown = g_bCButtonDown = FALSE;
		g_FrameCount = 0;
		g_CurrentFrame = 0;
		g_SoundPerSecond = 0;
		g_MyCheckTime = g_CheckErrorTime = g_CheckTimeNum = 0;
		g_pClientConfig->MAX_UPDATE_ONETIME = 6;
		g_pClientConfig->MAX_UPDATE_ONETIME_COUNT = 10;
	}
	~Scene()
	{
		triggers.clear();
		windows.DisappearWindow(window.get());
		window.reset();
		view.reset();
		zone->m_mapGroundEffect.clear();
		zone.reset();
		delete player;
	}

	// An event that has the flag, forever, or as the test made it.
	void Event(EVENT_ID id, DWORD flag)
	{
		MEvent event;
		event.eventID = id;
		event.eventFlag = flag;
		events.AddEvent(event);
	}
	void Event(MEvent event) { events.AddEvent(event); }

	// The view draws nothing: MTopView::Draw returns at once without a
	// zone (no tick may run then: the zone's update is unchecked).
	void Zoneless() { g_pZone = NULL; }

	// One Update at ms after the origin, the back buffer 0x7777 before it.
	std::string Frame(int ms)
	{
		back.FillSurface(0x7777);
		calls.clear();
		now = After(kOrigin, ms);
		g_FrameNow = now;
		update.Update();
		return calls;
	}
	bool Sent(int verifyTimes) { return wire.Sent() == VerifyTimes(verifyTimes); }
};

// What a tick records with the game inactive and nothing but the zone to
// move, and what a draw records.
const std::string kTick = "in;process;out;zone;";
const std::string kDraw = "cursor;move@3,4;show;pointer;pointer/back;flip;";
std::string Ticks(int n)
{
	std::string ticks;
	for (int i = 0; i < n; ++i) ticks += kTick;
	return ticks;
}

} // namespace

//======================================================================
// The gate: the game, with a configuration
//======================================================================
TEST(GameUpdate3Gate, OutsideTheGameAFrameDoesNothing)
{
	Scene scene;
	g_UpdateDelay = 1;
	g_SoundPerSecond = 5;
	g_CheckTimeNum = 7;
	g_FrameCount = 3;
	g_CurrentFrame = 9;
	g_bFrameChanged = true;
	g_DrawAlphaNum = 77;
	for (int mode = MODE_NULL; mode <= MODE_QUIT; ++mode)
	{
		if (mode == MODE_GAME) continue;
		g_Mode = CLIENT_MODE(mode);
		for (int ms : {0, 100, 5000})
			CHECK_SAME("", scene.Frame(ms));
	}
	CHECK(scene.Sent(0));
	CHECK_EQ(1, g_UpdateDelay);
	CHECK_EQ(5, g_SoundPerSecond);
	CHECK_EQ(7, g_CheckTimeNum);
	CHECK_EQ(3, g_FrameCount);
	CHECK_EQ(9, int(g_CurrentFrame));
	CHECK(g_bFrameChanged);
	CHECK_EQ(77, int(g_DrawAlphaNum));
	CHECK(!scene.update.m_NextSoundCheckTime);
}

TEST(GameUpdate3Gate, WithoutAConfigurationAFrameDoesNothing)
{
	Scene scene;
	ClientConfig* config = g_pClientConfig;
	g_pClientConfig = NULL;
	g_UpdateDelay = 1;
	CHECK_SAME("", scene.Frame(0));
	CHECK_SAME("", scene.Frame(1000));
	g_pClientConfig = config;
	CHECK(scene.Sent(0));
	CHECK_EQ(1, g_UpdateDelay);
	CHECK_EQ(0, g_FrameCount);
	CHECK_EQ(0, int(g_CurrentFrame));
}

TEST(GameUpdate3Gate, TheTickClockStartsAtTheFirstCallInAnyMode)
{
	{
		Scene scene;
		g_Mode = MODE_WAIT_UPDATEINFO;
		CHECK_SAME("", scene.Frame(0));
		CHECK(scene.update.m_LastTickTime == kOrigin);
		g_Mode = MODE_GAME;
		CHECK_SAME("mouse;" + kDraw, scene.Frame(61));
		CHECK_SAME("mouse;" + kTick + kDraw, scene.Frame(62));
	}
	{
		// The configuration is checked after it too.
		Scene scene;
		ClientConfig* config = g_pClientConfig;
		g_pClientConfig = NULL;
		CHECK_SAME("", scene.Frame(0));
		g_pClientConfig = config;
		CHECK_SAME("mouse;" + kTick + kDraw, scene.Frame(62));
	}
	{
		// Started in the game at 62 ms, the first tick is at 124.
		Scene scene;
		CHECK_SAME("mouse;" + kDraw, scene.Frame(62));
		CHECK(scene.update.m_LastTickTime == After(kOrigin, 62));
		CHECK_SAME("mouse;" + kDraw, scene.Frame(123));
		CHECK_SAME("mouse;" + kTick + kDraw, scene.Frame(124));
	}
}

//======================================================================
// The logout
//======================================================================
// Not run: the logout itself. Once it is due and the player waits for it,
// the frame calls ExecuteLogout, which in the game always starts the
// title screen (gC_vs_ui.StartTitle builds every title window from the
// skins and sprite packs), then cancels the logout and ends the frame.
TEST(GameUpdate3Logout, ALogoutIsDueOnlyAfterItsTime)
{
	Scene scene;
	scene.player->SetWaitVerify(MPlayer::WAIT_VERIFY_LOGOUT);
	g_pUserInformation->LogoutTime = After(kOrigin, 62);
	CHECK_SAME("mouse;" + kDraw, scene.Frame(0));
	CHECK_SAME("mouse;" + kTick + kDraw, scene.Frame(62));
	CHECK(g_pUserInformation->LogoutTime == After(kOrigin, 62));
	CHECK_EQ(int(MPlayer::WAIT_VERIFY_LOGOUT), int(scene.player->GetWaitVerify()));
}

TEST(GameUpdate3Logout, ADueLogoutThePlayerDoesNotWaitForIsLeftScheduled)
{
	Scene scene;
	g_pUserInformation->LogoutTime = kOrigin;
	CHECK_SAME("mouse;" + kDraw, scene.Frame(1));
	int ms = 1;
	for (MPlayer::WAIT_VERIFY wait : {MPlayer::WAIT_VERIFY_NULL, MPlayer::WAIT_VERIFY_RESURRECT, MPlayer::WAIT_VERIFY_SYLPH_SUMMON_GETOFF})
	{
		scene.player->SetWaitVerify(wait);
		CHECK_SAME("mouse;" + kDraw, scene.Frame(ms += 20));
	}
	CHECK_SAME("mouse;" + kTick + kDraw, scene.Frame(63));
	CHECK(g_pUserInformation->LogoutTime == kOrigin);
}

TEST(GameUpdate3Logout, WithoutUserInformationThereIsNoLogout)
{
	Scene scene;
	scene.player->SetWaitVerify(MPlayer::WAIT_VERIFY_LOGOUT);
	g_pUserInformation->LogoutTime = kOrigin;
	UserInformation* information = g_pUserInformation;
	g_pUserInformation = NULL;
	CHECK_SAME("mouse;" + kDraw, scene.Frame(1));
	CHECK_SAME("mouse;" + kTick + kDraw, scene.Frame(63));
	g_pUserInformation = information;
	CHECK(g_pUserInformation->LogoutTime == kOrigin);
}

//======================================================================
// Once a second, the count of sounds played
//======================================================================
TEST(GameUpdate3Sound, TheSoundCountIsResetOnceASecondStrictlyAfterTheCheck)
{
	Scene scene;
	g_SoundPerSecond = 5;
	scene.Frame(0);			// the first check is due now: not after it
	CHECK_EQ(5, g_SoundPerSecond);
	CHECK(scene.update.m_NextSoundCheckTime == kOrigin);
	scene.Frame(1);
	CHECK_EQ(0, g_SoundPerSecond);
	CHECK(scene.update.m_NextSoundCheckTime == After(kOrigin, 1001));
	g_SoundPerSecond = 5;
	scene.Frame(1001);
	CHECK_EQ(5, g_SoundPerSecond);
	scene.Frame(1002);
	CHECK_EQ(0, g_SoundPerSecond);
	CHECK(scene.update.m_NextSoundCheckTime == After(kOrigin, 2002));
	g_SoundPerSecond = 5;
	scene.Frame(2002);
	CHECK_EQ(5, g_SoundPerSecond);
}

//======================================================================
// The speed guard and the check counter
//======================================================================
TEST(GameUpdate3SpeedGuard, ADelayOtherThan62Sends100VerifyTimesAfterTheMouseAndPutsItBack)
{
	for (int delay : {1, 61, 63, 0, -62})
	{
		Scene scene;
		g_UpdateDelay = delay;
		When("mouse", [&scene]() { Call("sent" + std::to_string(scene.wire.Sent().size())); });
		CHECK_SAME("mouse;sent0;" + kDraw, scene.Frame(0));
		CHECK(scene.Sent(100));
		CHECK_EQ(62, g_UpdateDelay);
		// The tick counts the delay put back.
		CHECK_SAME("mouse;sent" + std::to_string(VerifyTimes(100).size()) + ";" + kTick + kDraw, scene.Frame(62));
		CHECK(scene.update.m_LastTickTime == After(kOrigin, 62));
		CHECK(scene.Sent(100));
	}
}

TEST(GameUpdate3SpeedGuard, The62DelaySendsNothing)
{
	Scene scene;
	scene.Frame(0);
	scene.Frame(62);
	scene.Frame(1000);
	CHECK(scene.Sent(0));
	CHECK_EQ(62, g_UpdateDelay);
}

TEST(GameUpdate3SpeedGuard, TheCheckCounterRunsFrom0To31EveryFrame)
{
	Scene scene;
	g_CheckTimeNum = 29;
	const int expected[] = {30, 31, 0, 1};
	for (int i = 0; i < 4; ++i)
	{
		scene.Frame(i);		// no tick, a draw at most
		CHECK_EQ(expected[i], g_CheckTimeNum);
	}
}

//======================================================================
// The tick
//======================================================================
TEST(GameUpdate3Tick, AFrameTicks62MsAfterTheLastTick)
{
	Scene scene;
	g_bFrameChanged = true;
	CHECK_SAME("mouse;" + kDraw, scene.Frame(0));
	CHECK(!g_bFrameChanged);
	CHECK_SAME("mouse;" + kDraw, scene.Frame(61));
	CHECK(!g_bFrameChanged);
	CHECK_EQ(0, int(g_CurrentFrame));
	CHECK_SAME("mouse;" + kTick + kDraw, scene.Frame(62));
	CHECK(g_bFrameChanged);
	CHECK_EQ(1, int(g_CurrentFrame));
	CHECK(scene.update.m_LastTickTime == After(kOrigin, 62));
	CHECK_SAME("mouse;", scene.Frame(63));
	CHECK(!g_bFrameChanged);
	CHECK_SAME("mouse;" + kDraw, scene.Frame(123));
	CHECK_SAME("mouse;" + kTick + kDraw, scene.Frame(124));
	CHECK_EQ(2, int(g_CurrentFrame));
	CHECK_EQ(5, g_FrameCount);
}

TEST(GameUpdate3Tick, ATickReadsTheSocketAndInputRunsTheUiWritesTheSocketThenMovesTheWorld)
{
	Scene scene;
	scene.Frame(0);
	g_bActiveGame = TRUE;
	scene.input.m_lb_down = TRUE;	// the UI takes it: ProcessInput stops there
	Rain rain;
	g_pWeather = &rain;
	// The view would draw the rain's sprites.
	When("zone", [&scene]() { g_pWeather = &scene.weather; });
	MScreenEffectManager inventory;
	inventory.AddEffect(new Recorder("inventory"));
	g_pInventoryEffectManager = &inventory;
	MEvent effect;
	effect.eventID = EVENTID_LOVECHAIN;
	effect.eventType = EVENTTYPE_EFFECT;
	effect.parameter1 = 1;
	scene.Event(effect);
	MGameTime gameTime;
	gameTime.SetStartTime(kOrigin, 2024, 5, 6, 10, 0, 0);
	gameTime.SetCurrentTime(kOrigin);
	g_pGameTime = &gameTime;
	MZoneSoundManager zoneSounds;
	g_pZoneSoundManager = &zoneSounds;
	CHECK_SAME("mouse;in;input;cursor;move@3,4;ldown@40,50;process;out;weather;inventory;events;zone;music;zone sound;" + kDraw, scene.Frame(62));
	g_pInventoryEffectManager = NULL;
	g_pGameTime = NULL;
	g_pZoneSoundManager = NULL;
}

TEST(GameUpdate3Tick, TheInputIsReadAndHandledOnlyWhileTheGameIsActive)
{
	Scene scene;
	scene.input.m_lb_down = TRUE;
	scene.Frame(0);
	CHECK_SAME("mouse;" + kTick + kDraw, scene.Frame(62));
	g_bActiveGame = TRUE;
	CHECK_SAME("mouse;in;input;cursor;move@3,4;ldown@40,50;process;out;zone;" + kDraw, scene.Frame(124));
	// ProcessInput's own gate: an event denying the input, even the
	// keyboard's alone.
	scene.Event(EVENTID_KICK_OUT_FROM_ZONE, EVENTFLAG_DENY_INPUT_KEYBOARD);
	CHECK_SAME("mouse;in;input;process;out;zone;" + kDraw, scene.Frame(186));
}

TEST(GameUpdate3Tick, AFailedSocketReadEndsTheFrameAndTheTickIsRetried)
{
	Scene scene;
	scene.Frame(0);
	socketInputOk = false;
	CHECK_SAME("mouse;in;", scene.Frame(62));
	CHECK_EQ(1, int(g_CurrentFrame));
	CHECK(g_bFrameChanged);
	CHECK_EQ(1, g_FrameCount);
	CHECK(scene.update.m_LastTickTime == kOrigin);
	socketInputOk = true;
	CHECK_SAME("mouse;" + kTick + kDraw, scene.Frame(63));
	CHECK_EQ(2, int(g_CurrentFrame));
	CHECK(scene.update.m_LastTickTime == After(kOrigin, 62));
}

TEST(GameUpdate3Tick, AModeChangedByTheSocketReadEndsTheFrame)
{
	Scene scene;
	scene.Frame(0);
	When("in", []() { g_Mode = MODE_WAIT_RECONNECT_LOGIN; });
	CHECK_SAME("mouse;in;", scene.Frame(62));
	CHECK_EQ(1, g_FrameCount);
	CHECK(scene.update.m_LastTickTime == kOrigin);
}

TEST(GameUpdate3Tick, AnInactiveApplicationOrAModeChangeAfterTheUiEndsTheFrame)
{
	{
		Scene scene;
		scene.Frame(0);
		g_bActiveApp = FALSE;
		CHECK_SAME("mouse;in;process;", scene.Frame(62));
		CHECK_EQ(1, g_FrameCount);
		CHECK(scene.update.m_LastTickTime == kOrigin);
	}
	{
		Scene scene;
		scene.Frame(0);
		When("process", []() { g_bActiveApp = FALSE; });
		CHECK_SAME("mouse;in;process;", scene.Frame(62));
	}
	{
		Scene scene;
		scene.Frame(0);
		When("process", []() { g_Mode = MODE_QUIT; });
		CHECK_SAME("mouse;in;process;", scene.Frame(62));
		CHECK(scene.update.m_LastTickTime == kOrigin);
	}
}

TEST(GameUpdate3Tick, AFailedSocketWriteOrAModeChangeByItEndsTheFrame)
{
	{
		Scene scene;
		scene.Frame(0);
		socketOutputOk = false;
		CHECK_SAME("mouse;in;process;out;", scene.Frame(62));
		CHECK_EQ(1, g_FrameCount);
		CHECK(scene.update.m_LastTickTime == kOrigin);
	}
	{
		Scene scene;
		scene.Frame(0);
		When("out", []() { g_Mode = MODE_MAINMENU; });
		CHECK_SAME("mouse;in;process;out;", scene.Frame(62));
		CHECK(scene.update.m_LastTickTime == kOrigin);
	}
}

TEST(GameUpdate3Tick, AnEndInTheMiddleOfABurstKeepsTheTicksDone)
{
	Scene scene;
	scene.Frame(0);
	int writes = 0;
	When("out", [&writes]() { if (++writes == 2) g_Mode = MODE_MAINMENU; });
	CHECK_SAME("mouse;" + kTick + "in;process;out;", scene.Frame(186));
	CHECK_EQ(2, int(g_CurrentFrame));
	CHECK(scene.update.m_LastTickTime == After(kOrigin, 62));
}

TEST(GameUpdate3Tick, WithoutWeatherInventoryEffectsOrEventsTheZoneStillMoves)
{
	Scene scene;
	scene.Frame(0);
	MScreenEffectManager inventory;
	inventory.AddEffect(new Recorder("inventory"));
	Rain rain;
	MEvent effect;
	effect.eventID = EVENTID_LOVECHAIN;
	effect.eventType = EVENTTYPE_EFFECT;
	scene.Event(effect);
	g_pWeather = &rain;
	g_pInventoryEffectManager = &inventory;
	// They are gone while the tick moves the world; the view draws again
	// with the scene's.
	When("out", []() { g_pWeather = NULL; g_pInventoryEffectManager = NULL; g_pEventManager = NULL; });
	When("zone", [&scene]() { g_pWeather = &scene.weather; g_pEventManager = &scene.events; });
	CHECK_SAME("mouse;in;process;out;zone;" + kDraw, scene.Frame(62));
}

//======================================================================
// Catching up
//======================================================================
TEST(GameUpdate3CatchUp, AFrameBehindTicksOncePerWholeDelay)
{
	{
		Scene scene;
		scene.Frame(0);
		CHECK_SAME("mouse;" + Ticks(2) + kDraw, scene.Frame(185));
		CHECK(scene.update.m_LastTickTime == After(kOrigin, 124));
		CHECK_SAME("mouse;" + Ticks(1) + kDraw, scene.Frame(186));
		CHECK(scene.update.m_LastTickTime == After(kOrigin, 186));
		CHECK_EQ(3, int(g_CurrentFrame));
	}
	{
		Scene scene;
		scene.Frame(0);
		CHECK_SAME("mouse;" + Ticks(3) + kDraw, scene.Frame(186));
		CHECK(scene.update.m_LastTickTime == After(kOrigin, 186));
		CHECK_EQ(0, scene.update.m_OnetimeUpdateCount);
	}
}

TEST(GameUpdate3CatchUp, ABurstStopsAtMaxUpdateOnetimeAndOnlyOneEndingBehindCounts)
{
	Scene scene;
	scene.Frame(0);
	// Six behind: six ticks, caught up, not counted.
	CHECK_SAME("mouse;" + Ticks(6) + kDraw, scene.Frame(372));
	CHECK(scene.update.m_LastTickTime == After(kOrigin, 372));
	CHECK_EQ(0, scene.update.m_OnetimeUpdateCount);
	// Ten behind: six ticks, still behind, counted; the rest next frame.
	CHECK_SAME("mouse;" + Ticks(6) + kDraw, scene.Frame(992));
	CHECK(scene.update.m_LastTickTime == After(kOrigin, 744));
	CHECK_EQ(1, scene.update.m_OnetimeUpdateCount);
	CHECK_SAME("mouse;" + Ticks(4) + kDraw, scene.Frame(992));
	CHECK(scene.update.m_LastTickTime == After(kOrigin, 992));
	CHECK_EQ(0, scene.update.m_OnetimeUpdateCount);
	CHECK_EQ(16, int(g_CurrentFrame));
}

TEST(GameUpdate3CatchUp, PastMaxUpdateOnetimeCountBurstsTheLostTimeIsDropped)
{
	Scene scene;
	g_pClientConfig->MAX_UPDATE_ONETIME = 1;
	g_pClientConfig->MAX_UPDATE_ONETIME_COUNT = 2;
	scene.Frame(0);
	// One behind: one tick, caught up, not counted.
	CHECK_SAME("mouse;" + Ticks(1) + kDraw, scene.Frame(62));
	CHECK_EQ(0, scene.update.m_OnetimeUpdateCount);
	CHECK_SAME("mouse;" + Ticks(1) + kDraw, scene.Frame(620));
	CHECK_EQ(1, scene.update.m_OnetimeUpdateCount);
	CHECK(scene.update.m_LastTickTime == After(kOrigin, 124));
	CHECK_SAME("mouse;" + Ticks(1) + kDraw, scene.Frame(620));	// a tick draws
	CHECK_EQ(2, scene.update.m_OnetimeUpdateCount);
	CHECK(scene.update.m_LastTickTime == After(kOrigin, 186));
	// The third burst in a row gives the time up.
	CHECK_SAME("mouse;" + Ticks(1) + kDraw, scene.Frame(620));
	CHECK_EQ(0, scene.update.m_OnetimeUpdateCount);
	CHECK(scene.update.m_LastTickTime == After(kOrigin, 620));
	CHECK_SAME("mouse;", scene.Frame(620));
	CHECK_SAME("mouse;" + kDraw, scene.Frame(681));
	CHECK_SAME("mouse;" + Ticks(1) + kDraw, scene.Frame(682));
	CHECK_EQ(5, int(g_CurrentFrame));
}

TEST(GameUpdate3CatchUp, ANonPositiveMaxUpdateOnetimeRemovesTheCap)
{
	// Recorded defect: MAX_UPDATE_ONETIME comes from a configuration file
	// (ClientConfig.cpp); at 0 or below `--k` never reaches 0, so a frame
	// catches up every delay it is behind (here 20; after a stall of
	// minutes thousands, in one frame) and no burst ever counts. The loop
	// still ends when it has caught up.
	for (int max : {0, -1})
	{
		Scene scene;
		g_pClientConfig->MAX_UPDATE_ONETIME = max;
		scene.Frame(0);
		CHECK_SAME("mouse;" + Ticks(20) + kDraw, scene.Frame(1240));
		CHECK(scene.update.m_LastTickTime == After(kOrigin, 1240));
		CHECK_EQ(0, scene.update.m_OnetimeUpdateCount);
		CHECK_EQ(20, int(g_CurrentFrame));
	}
}

//======================================================================
// The game hour, the zone's sounds
//======================================================================
namespace {
int Count(const std::string& text, const std::string& what)
{
	int n = 0;
	for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1)) ++n;
	return n;
}
}

TEST(GameUpdate3GameTime, AnHourChangeIsSeenTheFrameAfterTheClockMovesAndAnEvenHourPlaysTheMusic)
{
	Scene scene;
	MGameTime gameTime;
	gameTime.SetTimeRatio(3600);	// a second an hour
	gameTime.SetStartTime(kOrigin, 2024, 5, 6, 10, 0, 0);
	gameTime.SetCurrentTime(kOrigin);
	g_pGameTime = &gameTime;
	// The first hour seen is a change.
	CHECK_SAME("mouse;music;" + kDraw, scene.Frame(0));
	CHECK(scene.update.m_LastHour == 10);
	CHECK_EQ(0, Count(scene.Frame(999), "music"));
	CHECK_EQ(10, int(gameTime.GetHour()));
	CHECK_EQ(0, Count(scene.Frame(1000), "music"));
	CHECK_EQ(11, int(gameTime.GetHour()));		// set by the frame
	CHECK(scene.update.m_LastHour == 10);
	CHECK_EQ(0, Count(scene.Frame(1001), "music"));	// 11: odd
	CHECK(scene.update.m_LastHour == 11);
	CHECK_EQ(0, Count(scene.Frame(2000), "music"));
	CHECK_EQ(12, int(gameTime.GetHour()));
	CHECK_EQ(1, Count(scene.Frame(2001), "music"));
	CHECK(scene.update.m_LastHour == 12);
	CHECK_EQ(0, Count(scene.Frame(2002), "music"));
	g_pGameTime = NULL;
}

TEST(GameUpdate3GameTime, WithoutAStringTableTheGameClockStands)
{
	// Recorded defect: SetCurrentTime is inside the string table's guard
	// with the time's text (CGameUpdate.cpp:2232-2243), so without a table
	// the game's clock is never moved; the date still goes to the UI.
	Scene scene;
	MGameTime gameTime;
	gameTime.SetTimeRatio(3600);
	gameTime.SetStartTime(kOrigin, 2024, 5, 6, 10, 0, 0);
	gameTime.SetCurrentTime(kOrigin);
	g_pGameTime = &gameTime;
	MStringArray* strings = g_pGameStringTable;
	g_pGameStringTable = NULL;
	CHECK_SAME("mouse;music;" + kDraw, scene.Frame(0));
	scene.Frame(5000);
	CHECK_EQ(10, int(gameTime.GetHour()));
	g_pGameStringTable = strings;
	scene.Frame(5100);
	CHECK_EQ(15, int(gameTime.GetHour()));
	g_pGameTime = NULL;
}

TEST(GameUpdate3ZoneSound, TheZoneSoundsAreUpdatedEveryFrameAfterTheTick)
{
	Scene scene;
	MZoneSoundManager zoneSounds;
	g_pZoneSoundManager = &zoneSounds;
	CHECK_SAME("mouse;zone sound;" + kDraw, scene.Frame(0));
	CHECK_SAME("mouse;zone sound;", scene.Frame(1));
	CHECK_SAME("mouse;" + kTick + "zone sound;" + kDraw, scene.Frame(62));
	g_pZoneSoundManager = NULL;
	CHECK_SAME("mouse;" + kDraw, scene.Frame(100));
}

//======================================================================
// The draw's fraction of the tick, the draw gate
//======================================================================
TEST(GameUpdate3Alpha, TheDrawFractionIsTheTimeSinceTheLastTickIn256ths)
{
	Scene scene;
	g_pClientConfig->MAX_UPDATE_ONETIME = 1;
	const struct { int ms; int alpha; } frames[] = {
		{ 0, 0 }, { 1, 4 }, { 31, 128 }, { 61, 251 },
		{ 62, 0 },			// a tick
		{ 70, 33 },
		{ 187, 256 },		// a tick, still 63 ms behind
		{ 100, 256 },		// before the last tick
	};
	for (const auto& frame : frames)
	{
		g_DrawAlphaNum = 999;
		scene.Frame(frame.ms);
		CHECK_EQ(frame.alpha, int(g_DrawAlphaNum));
	}
	CHECK(scene.update.m_LastTickTime == After(kOrigin, 124));
}

TEST(GameUpdate3DrawGate, AFrameDrawsOnATickOr15MsAfterTheLastDraw)
{
	Scene scene;
	const struct { int ms; bool drawn; } frames[] = {
		{ 0, true }, { 14, false }, { 15, true }, { 29, false }, { 30, true },
		{ 50, true }, { 62, true },	// a tick 12 ms after the last draw
		{ 76, false }, { 77, true },
	};
	int count = 0;
	for (const auto& frame : frames)
	{
		const std::string drawn = scene.Frame(frame.ms);
		CHECK_EQ(frame.drawn, drawn.find("flip") != std::string::npos);
		count += frame.drawn ? 1 : 0;
		CHECK_EQ(count, g_FrameCount);
		if (frame.drawn) CHECK(scene.update.m_LastDrawTime == After(kOrigin, frame.ms));
	}
}

//======================================================================
// UpdateDraw
//======================================================================
TEST(GameUpdate3Draw, AFrameFollowsTheCursorShowsTheUiAsksForThePointerTwiceAndFlips)
{
	// Recorded defect: the pointer is drawn twice a frame
	// (CGameUpdate.cpp:1701-1704 and 1827-1830), once onto the last frame
	// before it is copied to the back buffer and again onto the back
	// buffer: wasted work, and a translucent pointer twice blended.
	Scene scene;
	CHECK_SAME("mouse;cursor;move@3,4;show;pointer;pointer/back;flip;", scene.Frame(0));
	scene.Zoneless();
	CHECK_SAME("mouse;cursor;move@3,4;show;pointer;pointer/back;flip;", scene.Frame(20));
}

TEST(GameUpdate3Draw, TheCursorIsMappedIntoTheLetterboxedFrame)
{
	Scene scene;
	// An 800x600 frame letterboxed in a 1600x1000 window.
	spritectl_set_present_geometry(1600, 1000, 1600, 1000, 133, 0, 1333, 999, 800, 600);
	windowPoint = {133 + 666, 500};
	CHECK_SAME("mouse;cursor;move@399,300;show;pointer;pointer/back;flip;", scene.Frame(0));
	windowPoint = {0, 0};		// on the bar: the frame's corner
	CHECK_SAME("mouse;cursor;move@0,0;show;pointer;pointer/back;flip;", scene.Frame(20));
}

TEST(GameUpdate3Draw, AViewDrawingItsOwnRequestHidesThePointer)
{
	Scene scene;
	When("show", []() { g_pTopView->m_bDrawRequest = true; });
	CHECK_SAME("mouse;cursor;move@3,4;show;flip;", scene.Frame(0));
	triggers.clear();
	scene.view->m_bDrawRequest = false;
	CHECK_SAME("mouse;" + kDraw, scene.Frame(20));
}

TEST(GameUpdate3Draw, TheLastFrameIsCopiedToTheBackBufferFromTheCornerToTheGameRectsEnd)
{
	Scene scene;
	scene.Zoneless();
	scene.Frame(0);
	CHECK(Picture(scene.back) == Picture(scene.last));
	int ms = 0;
	for (RECT rect : {RECT{0, 0, 100, 50}, RECT{7, 9, 100, 50}})
	{
		g_GameRect = rect;
		CHECK(scene.Frame(ms += 20).find("flip") != std::string::npos);
		const std::vector<WORD> back = Picture(scene.back), last = Picture(scene.last);
		int wrong = 0;
		for (int y = 0; y < 600; ++y)
			for (int x = 0; x < 800; ++x)
			{
				const size_t i = size_t(y) * 800 + x;
				const WORD expected = (x < 100 && y < 50) ? last[i] : WORD(0x7777);
				if (back[i] != expected) ++wrong;
			}
		CHECK_EQ(0, wrong);
	}
}

namespace {
// The surface with the frame rate's text as the frame prints it: 12
// point, a dark copy one pixel right of and below the white one.
std::vector<WORD> WithFrameRate(CSpriteSurface& surface, const char* text)
{
	TextSystem::SpriteSurfaceRenderTarget target(&surface);
	auto& service = TextSystem::TextService::Get();
	auto style = service.GetDefaultStyle();
	style.font = service.GetFont(12);
	style.color = TextSystem::ColorFromRGB(0x141414);
	service.DrawLine(target, text, 11, 41, 0, style);
	style.color = TextSystem::ColorFromRGB(0xFFFFFF);
	service.DrawLine(target, text, 10, 40, 0, style);
	return Picture(surface);
}
}

TEST(GameUpdate3Draw, TheFrameRateIsPrintedOnTheLastFrameWhenAsked)
{
	Scene scene;
	const std::vector<WORD> pattern = Picture(scene.last);
	g_FrameRate = 57;
	scene.Frame(0);
	CHECK(Picture(scene.last) == pattern);
	CSpriteSurface expected;
	CHECK(expected.Init(800, 600));
	Pattern(expected);
	const std::vector<WORD> text = WithFrameRate(expected, "57 FPS");
	CHECK(text != pattern);
	scene.option.DrawFPS = TRUE;
	scene.Frame(20);
	CHECK(Picture(scene.last) == text);
	CHECK(Picture(scene.back) == text);
	// The debug switch prints it too (nothing sets it in the game).
	Pattern(scene.last);
	scene.option.DrawFPS = FALSE;
	scene.update.m_bOutputInfo = true;
	scene.Frame(40);
	CHECK(Picture(scene.last) == text);
}

TEST(GameUpdate3Draw, TheWorldIsDrawnAtThePlayersInterpolatedPosition)
{
	Scene scene;
	scene.Frame(0);
	// The player moved 32 right and 24 down in the last tick.
	scene.player->SnapshotDrawState();
	scene.player->m_sX = 32;
	scene.player->m_sY = 24;
	g_bInterpolateDraw = true;
	scene.Frame(31);			// half the tick: 16 and 12 to go
	CHECK_EQ(128, int(g_DrawAlphaNum));
	CHECK_EQ(16, g_DrawCamGapX);
	CHECK_EQ(12, g_DrawCamGapY);
	// The camera is biased by the gap: DrawZone's correction is the
	// player's offset less the point it is given.
	CHECK_EQ(32 - 16, int(scene.view->m_PlusPoint.x));
	CHECK_EQ(24 - 12, int(scene.view->m_PlusPoint.y));
	// The view placed the player's filter at its interpolated position,
	// 16 short of the exact one: the world draws interpolated.
	const MTopView& view = *scene.view;
	CHECK_EQ(WorldTileGeometry::TileToPixelX(kPlayerX) + 16,
		int(view.m_FilterPosition.x) + int(view.m_FirstZonePixel.x) + (view.m_ImageObjectFilter.GetWidth() >> 1) - 24);
	CHECK(!g_bInterpolateDraw);
	// Later in the tick the gap shrinks; after the next tick the snapshot
	// is another frame's: none.
	scene.Frame(61);
	CHECK_EQ(251, int(g_DrawAlphaNum));
	CHECK_EQ(32 - ((32 * 251) >> 8), g_DrawCamGapX);
	scene.Frame(62);
	CHECK_EQ(0, g_DrawCamGapX);
	CHECK_EQ(0, g_DrawCamGapY);
	CHECK_EQ(32, int(scene.view->m_PlusPoint.x));
}

namespace {
// The point the world was drawn at, from DrawZone's correction (the
// player stands at offset 0, away from the zone's edges).
POINT DrawnAt(Scene& scene) { return { -scene.view->m_PlusPoint.x, -scene.view->m_PlusPoint.y }; }
const int kShake[13] = { 0, 1, 2, 2, 2, 1, 0, -2, -2, -2, -1, -1, 0 };
}

TEST(GameUpdate3Draw, AShowingShakeEventShakesTheWorldAStepATick)
{
	Scene scene;
	MEvent shake;
	shake.eventID = EVENTID_METEOR;
	shake.eventFlag = EVENTFLAG_SHAKE_SCREEN;
	shake.parameter3 = 2;
	scene.Event(shake);
	scene.Frame(0);
	for (int tick = 0; tick <= 14; ++tick)
	{
		const int step = tick % 13;
		CHECK_EQ(step, scene.update.m_ShakeStep);
		scene.Frame(62 * tick + 30);	// between ticks: the same step
		CHECK_EQ(kShake[step] * 2, int(DrawnAt(scene).x));
		CHECK_EQ((step % 3 - 1) * 3 * 2, int(DrawnAt(scene).y));
		CHECK_EQ(step, scene.update.m_ShakeStep);
		scene.Frame(62 * (tick + 1));	// the tick draws it, then steps
		CHECK_EQ(kShake[step] * 2, int(DrawnAt(scene).x));
		CHECK_EQ((step % 3 - 1) * 3 * 2, int(DrawnAt(scene).y));
	}
	// Shaken around the interpolated camera: the player 16 and 12 short.
	scene.player->SnapshotDrawState();
	scene.player->m_sX = 32;
	scene.player->m_sY = 24;
	scene.Frame(62 * 15 + 31);
	CHECK_EQ(2, scene.update.m_ShakeStep);
	CHECK_EQ(32 - (kShake[2] * 2 + 16), int(scene.view->m_PlusPoint.x));
	CHECK_EQ(24 - ((2 % 3 - 1) * 3 * 2 + 12), int(scene.view->m_PlusPoint.y));
}

TEST(GameUpdate3Draw, AMeteorShakeShakesUpAndDownInTwoSteps)
{
	Scene scene;
	MEvent shake;
	shake.eventID = EVENTID_COMBAT_MASTER;
	shake.eventFlag = EVENTFLAG_SHAKE_SCREEN;
	shake.parameter3 = 3;
	scene.Event(shake);
	scene.Event(EVENTID_METEOR_SHAKE, 0);
	scene.Frame(0);
	for (int tick = 0; tick < 4; ++tick)
	{
		scene.Frame(62 * (tick + 1));
		CHECK_EQ(kShake[tick] * 3, int(DrawnAt(scene).x));
		CHECK_EQ((tick % 2 * 3 - 2) * 3, int(DrawnAt(scene).y));
	}
}

TEST(GameUpdate3Draw, AHiddenShakeNeitherShakesNorSteps)
{
	Scene scene;
	MEvent shake;
	shake.eventID = EVENTID_METEOR;
	shake.eventFlag = EVENTFLAG_SHAKE_SCREEN;
	shake.parameter3 = 2;
	shake.showTime = 100;	// shown the first 100 ms of every 200
	shake.totalTime = 200;
	scene.Event(shake);
	scene.Frame(0);
	CHECK_EQ(-6, int(DrawnAt(scene).y));
	scene.Frame(62);		// shown: drawn at step 0, then step 1
	CHECK_EQ(-6, int(DrawnAt(scene).y));
	CHECK_EQ(1, scene.update.m_ShakeStep);
	for (int ms : {100, 124, 186})
	{
		scene.Frame(ms);
		CHECK_EQ(0, int(DrawnAt(scene).x));
		CHECK_EQ(0, int(DrawnAt(scene).y));
		CHECK_EQ(1, scene.update.m_ShakeStep);
	}
	scene.Frame(248);		// 48 ms into the next period
	CHECK_EQ(2, int(DrawnAt(scene).x));
	CHECK_EQ(0, int(DrawnAt(scene).y));
	CHECK_EQ(2, scene.update.m_ShakeStep);
}

//======================================================================
// The dead player
//======================================================================
// Not observable here: what the frame hands the game window (the
// resurrection request's four choices, the shrine minimap, the time and
// date strings and the player's position): gC_vs_ui has no game window
// in this binary, so each call ends at its `if (m_pC_game)`. The player
// below died in zone 1 by a quest (resurrect zone set), which skips the
// inventory and skill reads that only feed those choices.
namespace {
// The player fallen (its dying action played out) with ms to its
// resurrection.
void Fallen(Scene& scene, int ms)
{
	scene.player->SetResurrectZoneID(1);
	scene.player->m_bAlive = false;
	scene.player->m_ActionCountMax = 8;
	scene.player->m_ActionCount = 8;
	scene.player->m_DeadDelayTime = After(now, ms);
}
}

TEST(GameUpdate3Death, NoFadeWhileAliveOrStillFalling)
{
	Scene scene;
	scene.Frame(0);
	CHECK(!scene.view->m_Fade.IsActive());
	Fallen(scene, 2000);
	scene.player->m_ActionCount = 7;
	scene.Frame(20);
	CHECK(!scene.view->m_Fade.IsActive());
	CHECK_EQ(int(TempInformation::MODE_NULL), int(scene.temp.GetMode()));
	CHECK(!scene.update.m_DeadStepTime);
	scene.player->m_ActionCount = 8;
	scene.player->m_bAlive = true;
	scene.Frame(40);
	CHECK(!scene.view->m_Fade.IsActive());
	CHECK(!scene.update.m_DeadStepTime);
}

TEST(GameUpdate3Death, AFallenPlayerFadesToADarknessByTheSecondsLeft)
{
	Scene scene;
	scene.Frame(0);
	const struct { int ms; int black; } cases[] = {
		{ 0, 20 }, { 999, 20 }, { 2999, 20 }, { 3000, 23 }, { 10500, 30 },
		{ 11000, 31 }, { 12000, 31 }, { 40000, 31 },
	};
	int ms = 0;
	for (const auto& c : cases)
	{
		now = After(kOrigin, ms += 20);
		Fallen(scene, c.ms);
		scene.view->m_Fade.Stop();
		scene.Frame(ms);
		CHECK(scene.view->m_Fade.IsActive());
		CHECK_EQ(c.black, int(scene.view->m_Fade.Value()));
		CHECK_EQ(int(CSDLGraphics::Color(5, 5, 5)), int(scene.view->m_FadeColor));
	}
}

TEST(GameUpdate3Death, TheResurrectionIsAskedWithUnder4SecondsLeft)
{
	Scene scene;
	scene.Frame(0);
	const struct { int ms; bool asked; } cases[] = {
		{ 0, true }, { 3999, true }, { 4000, false }, { 9000, false },
	};
	int ms = 0;
	for (const auto& c : cases)
	{
		now = After(kOrigin, ms += 20);
		Fallen(scene, c.ms);
		scene.temp.SetMode(TempInformation::MODE_NULL);
		scene.Frame(ms);
		CHECK_EQ(int(c.asked ? TempInformation::MODE_WAIT_RESURRECT : TempInformation::MODE_NULL), int(scene.temp.GetMode()));
	}
	// Only while nothing else is asked.
	now = After(kOrigin, ms += 20);
	Fallen(scene, 0);
	scene.temp.SetMode(TempInformation::MODE_SHOP_BUY);
	scene.Frame(ms);
	CHECK_EQ(int(TempInformation::MODE_SHOP_BUY), int(scene.temp.GetMode()));
	scene.temp.SetMode(TempInformation::MODE_NULL);
}

TEST(GameUpdate3Death, TheRequestIsMadeInEveryOtherWindowOfSixStepsADelayApart)
{
	Scene scene;
	scene.Frame(0);
	Fallen(scene, 2000);
	// A step counts a draw g_UpdateDelay after the last step, the first
	// fallen draw's included (a frame that does not draw, 160, counts
	// nothing).
	const struct { int ms; int frames; } steps[] = {
		{ 20, 0 }, { 81, 0 }, { 100, 1 }, { 150, 1 }, { 160, 1 }, { 165, 2 },
	};
	for (const auto& step : steps)
	{
		scene.Frame(step.ms);
		CHECK_EQ(step.frames, int(scene.update.m_DeadStepFrames));
		CHECK_EQ(0, int(scene.update.m_DeadStep));
	}
	CHECK(scene.update.m_DeadStepTime == After(kOrigin, 165));
	// Every 62 ms from here: the request in windows 0 and 2, not 1 (the
	// window is read before the step).
	std::string asked;
	for (int i = 0; i < 18; ++i)
	{
		scene.temp.SetMode(TempInformation::MODE_NULL);
		scene.Frame(165 + 62 * (i + 1));
		asked += scene.temp.GetMode() == TempInformation::MODE_WAIT_RESURRECT ? "R" : ".";
	}
	CHECK_SAME("RRRR......RRRRRR..", asked);
	CHECK_EQ(3, int(scene.update.m_DeadStep));
	CHECK_EQ(2, int(scene.update.m_DeadStepFrames));
}

//======================================================================
// Windows' speed check
//======================================================================
// Compiled on Windows only (Update's `#ifdef PLATFORM_WINDOWS` blocks), so
// these run on the Windows CI alone. After the ticks of a frame that
// ticked: the time since the last such frame, at most a second, is added
// to g_MyCheckTime; at 97620 (the client ran far ahead of the clock) ten
// CGVerifyTime go out, each an error. A minute after the first check (5390
// ms when it was ahead) one more goes out, an error when ahead, and the
// sum starts again. Five errors quit the game and end the frame.
#ifdef PLATFORM_WINDOWS
TEST(GameUpdate3SpeedCheck, TheCheckTimeAddsEachTickingFramesGapUpToASecond)
{
	Scene scene;
	g_pClientConfig->MAX_UPDATE_ONETIME_COUNT = 0;	// a burst ending behind drops the rest
	scene.Frame(0);
	CHECK_EQ(0, g_MyCheckTime);
	scene.Frame(62);
	CHECK_EQ(0, g_MyCheckTime);
	scene.Frame(100);		// no tick
	CHECK_EQ(0, g_MyCheckTime);
	scene.Frame(124);
	CHECK_EQ(62, g_MyCheckTime);
	scene.Frame(1123);		// 999
	CHECK_EQ(1061, g_MyCheckTime);
	scene.Frame(3123);		// 2000: a second
	CHECK_EQ(2061, g_MyCheckTime);
	CHECK(scene.Sent(0));
	CHECK_EQ(0, g_CheckErrorTime);
}

TEST(GameUpdate3SpeedCheck, AMinuteAfterTheFirstCheckOneVerifyTimeGoesOut)
{
	Scene scene;
	g_pClientConfig->MAX_UPDATE_ONETIME_COUNT = 0;
	scene.Frame(0);
	scene.Frame(62);
	CHECK(scene.update.m_NextVerifyTime == After(kOrigin, 60062));
	scene.Frame(60062);		// due, not past
	CHECK(scene.Sent(0));
	CHECK_EQ(1000, g_MyCheckTime);
	CHECK_SAME("mouse;", scene.Frame(60063));	// past, but no tick
	CHECK(scene.Sent(0));
	CHECK_SAME("mouse;" + kTick + kDraw, scene.Frame(60124));
	CHECK(scene.Sent(1));
	CHECK_EQ(0, g_MyCheckTime);
	CHECK_EQ(0, g_CheckErrorTime);
	CHECK(scene.update.m_NextVerifyTime == After(kOrigin, 120124));
}

TEST(GameUpdate3SpeedCheck, AClientAheadOfTheClockSendsTenVerifyTimesAndQuits)
{
	{
		Scene scene;
		scene.Frame(0);
		scene.Frame(62);
		g_MyCheckTime = 97620 - 63;
		CHECK_SAME("mouse;" + kTick + kDraw, scene.Frame(124));
		CHECK(scene.Sent(0));
		CHECK_EQ(97619, g_MyCheckTime);
	}
	{
		Scene scene;
		scene.Frame(0);
		scene.Frame(62);
		g_MyCheckTime = 97620 - 62;
		scene.update.m_OnetimeUpdateCount = 3;
		CHECK_SAME("mouse;" + kTick + "mode" + std::to_string(int(MODE_QUIT)) + ";", scene.Frame(124));
		CHECK(scene.Sent(10));
		CHECK_EQ(0, g_CheckErrorTime);
		CHECK_EQ(97620, g_MyCheckTime);
		CHECK_EQ(3, scene.update.m_OnetimeUpdateCount);	// the frame ended first
	}
}

TEST(GameUpdate3SpeedCheck, FiveErrorsQuitAtTheNextTickingFrame)
{
	Scene scene;
	scene.Frame(0);
	g_CheckErrorTime = 4;
	CHECK_SAME("mouse;" + kTick + kDraw, scene.Frame(62));
	CHECK_EQ(4, g_CheckErrorTime);
	g_CheckErrorTime = 5;
	CHECK_SAME("mouse;", scene.Frame(70));
	CHECK_EQ(5, g_CheckErrorTime);
	CHECK_SAME("mouse;" + kTick + "mode" + std::to_string(int(MODE_QUIT)) + ";", scene.Frame(124));
	CHECK_EQ(0, g_CheckErrorTime);
	CHECK(scene.Sent(0));
}

TEST(GameUpdate3SpeedCheck, ADueCheckAheadOfTheClockSendsElevenAndChecksAgainIn5390Ms)
{
	Scene scene;
	g_pClientConfig->MAX_UPDATE_ONETIME_COUNT = 0;
	scene.Frame(0);
	scene.Frame(62);
	g_MyCheckTime = 97620 - 1000;
	CHECK_SAME("mouse;" + Ticks(6) + "mode" + std::to_string(int(MODE_QUIT)) + ";", scene.Frame(60063));
	CHECK(scene.Sent(11));
	CHECK_EQ(0, g_MyCheckTime);
	CHECK_EQ(0, g_CheckErrorTime);
	CHECK(scene.update.m_NextVerifyTime == After(kOrigin, 60063 + 5390));
}
#endif // PLATFORM_WINDOWS
