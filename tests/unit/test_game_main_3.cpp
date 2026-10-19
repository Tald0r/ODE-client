#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "GameMain.h"
#include "GameInitInfoHost.h"
#include "MZone.h"
#include "MSector.h"
#include "MZoneDef.h"
#include "MZoneTable.h"
#include "MPortal.h"
#include "MPlayer.h"
#include "MNPC.h"
#include "MFakeCreature.h"
#include "MTopView.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MActionInfoTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "EffectSpriteTypeDef.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "MEventQueue.h"
#include "MEventManager.h"
#include "MParty.h"
#include "MWarManager.h"
#include "GuildWarInfo.h"
#include "MWeather.h"
#include "MStorage.h"
#include "MTradeManager.h"
#include "MZoneSoundManager.h"
#include "MTimeItemManager.h"
#include "MSkillManager.h"
#include "MGuildType.h"
#include "ZoneFlags.h"
#include "AmbientSoundState.h"
#include "FrameClock.h"
#include "ClientMode.h"
#include "InputState.h"
#include "GameSocket.h"
#include "WinLib/CWinUpdate.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"
#include "DialogPresentationHost.h"
#include "MEffect.h"
#include "UserOption.h"
#include "UserInformation.h"
#include "CServerInformation.h"
#include "Properties.h"
#include "VS_UI_Base.h"
#include "VS_UI_GameCommon.h"
#include "VS_UI_UIMessage.h"
#include "DXLib/CSoundPartManager.h"
#include "DXLib/COGGSTREAM.h"
#include "CDirectInput.h"
#include "MonotonicClock.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

// GameMain's zones, player and game objects (task 4.262 moved
// Client/GameMain.cpp into gameclient):
// - LoadZone(n) looks n up in the zone table (unknown: FALSE, nothing
//   touched); stops the zone's sounds and the music;
//   releases the current zone's objects; then in the zone of n's size
//   (small or large) either reuses the zone already loaded for n or
//   replaces it with a new one read from the map file (the teen file for a
//   player without gore), preloads the sprites near the player and reads
//   the view's tile and image-object sprite positions from the same file;
//   then the zone info (LoadZoneInfo), the Gilles de Rais ghosts, zone
//   4001's tiles and zone 74's cloud, the ambient sound delay, the view's
//   zone, the colour set, safety and holy-land flags, the war and the
//   timed items, the world name and the competence ghost command.
// - MoveZone takes the player out of the zone, loads n and puts the
//   player into g_pZone.
// - LoadZoneInfo publishes the info file's portals and safe areas to the
//   zone, plants the ousters' horns and turns zone 1311's weather on.
// - InitPlayer places the player in g_pZone; ReleaseGameObject drops the
//   world's objects; LoadCreature loads a creature type's sprites;
//   Add_GDR_Ghost spawns the ghosts of zones 1412 and 1413.
//
// The zone files are written to a temporary directory: a map (header,
// sectors, no image objects, then the tile and image-object sprite
// position arrays at the offsets the header names), an info file, the
// horn file every new MZone reads and an image-object pack.
//
// Not observable here: the minimap and progress calls (gC_vs_ui has no
// game window and g_bDrawProgress is FALSE, so each call ends early), the
// music stop (no audio device: g_SDLAudio is not initialised, so neither
// the OGG stream nor g_Music plays), the shadow manager (ClearShadowManager
// is empty) and the ousters' NPC info for the minimap. Nor are two calls
// that repeat what another already did: InitPlayer's ResetSendMove (its
// SetServerPosition clears the same state) and MoveZone's RemovePlayer
// (LoadZone releases the zone's creatures, the player's entry with them,
// and an unknown zone's SetPlayer finds the player still there).
//
// Not run (undefined behaviour):
// - LoadZone(4001) on a zone that is not square: the loop at
//   GameMain.cpp:1804-1810 passes (row, column) to GetSector(x, y), so it
//   reads and writes sectors off the grid.
// - MoveZone(n) with no zone and an unknown n: LoadZone returns FALSE
//   leaving g_pZone NULL and GameMain.cpp:1890 calls g_pZone->SetPlayer().
// - LoadZoneInfo with a valid info file and no player (GameMain.cpp:1994
//   g_pPlayer->GetRace()), and LoadZone without a view, a player (on a
//   load), a war manager, a timed-item register or server information:
//   each is read unchecked.

extern BOOL	g_bActiveApp;
extern bool	g_bRunning;
extern BOOL	g_bDrawProgress;	// GameUI.cpp's
extern CSoundPartManager*	g_pSoundManager;	// GameInitInfo.cpp's

namespace {

//----------------------------------------------------------------------
// Base keeps its UI result receiver to itself; an explicit instantiation
// may name a private member, so this reads it.
//----------------------------------------------------------------------
template <class Tag, typename Tag::type Member>
struct Expose
{
	friend typename Tag::type Get(Tag) { return Member; }
};
struct ReceiverOf { using type = C_VS_UI_UI_RESULT_RECEIVER Base::*; friend type Get(ReceiverOf); };
template struct Expose<ReceiverOf, &Base::m_C_ui_result_receiver>;

std::string calls;
std::string N(long long value) { return std::to_string(value); }
void Call(const std::string& call) { calls += call + ";"; }

bool Same(const std::string& expected, const std::string& actual)
{
	if (expected != actual)
		std::fprintf(stderr, "    expected %s\n    actual   %s\n", expected.c_str(), actual.c_str());
	return expected == actual;
}
#define CHECK_SAME(expected, actual) CHECK(Same((expected), (actual)))

//----------------------------------------------------------------------
// The hosts: GameMain's, the start-up's failure message, the world, the
// zone, the player and the dialogs, each recording what it is asked.
//----------------------------------------------------------------------
const GameMainHost gameMainHost{
	.InitDraw = []() -> BOOL { Call("draw"); return TRUE; },
	.InitSurface = []() -> BOOL { Call("surface"); return TRUE; },
	.InitSound = []() -> BOOL { Call("sound"); return TRUE; },
	.ReleaseSocket = []() { Call("socket"); },
};

const GameInitInfoHost::Host initInfoHost = []() {
	GameInitInfoHost::Host h{};
	h.InitFailMessage = [](const char* message) { Call(std::string("fail ") + message); };
	return h;
}();

// The tile pack's size as the view reports it to the zone.
int tileSprites = 0;
const WorldEvents::Host worldHost = []() {
	WorldEvents::Host h{};
	h.ViewExists = []() { return false; };
	h.GetTileSpriteCount = []() { return tileSprites; };
	return h;
}();

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
	h.DeleteCreature = [](MCreature* pCreature) { delete pCreature; };
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
	h.SendPositionInfoToParty = []() { Call("party position"); };
	h.SendStatusInfoToParty = []() { Call("party status"); };
	return h;
}();

const CreatureState::Host creatureHost = []() {
	CreatureState::Host h{};
	h.GetAddonItemClass = [](MCreature*, int, ITEM_CLASS* pItemClass) { *pItemClass = ITEM_CLASS_NULL; return false; };
	h.ObjectOf = [](const MCreature* pCreature) -> MObject* { return const_cast<MCreature*>(pCreature); };
	h.CreatureOf = [](MObject* pObject) { return static_cast<MCreature*>(pObject); };
	return h;
}();

const DialogPresentation::Host dialogHost = []() {
	DialogPresentation::Host h{};
	h.UnlockGear = []() { Call("unlock gear"); };
	h.UnlockItem = []() { Call("unlock item"); };
	h.UnlockItemTrade = []() { Call("unlock trade"); };
	return h;
}();

const MEffectHost effectHost = []() {
	MEffectHost h{};
	h.CurrentFrame = []() -> DWORD { return g_CurrentFrame; };
	return h;
}();

const MWeatherHost weatherHost = {
	.ReadOrigin = [](int& x, int& y) { x = 100; y = 200; return true; },
	.Width = []() { return 800; },
	.Height = []() { return 600; },
};

// The UI's result receiver: the text commands the game sends itself.
void Received(DWORD message, intptr_t left, intptr_t right, void* text)
{
	Call("ui" + N(message) + "," + N(left) + "," + N(right) + ":" + static_cast<const char*>(text));
}

//----------------------------------------------------------------------
// The files
//----------------------------------------------------------------------
using Bytes = std::vector<char>;

void Put(Bytes& bytes, std::uint32_t value, unsigned width)
{
	for (unsigned i = 0; i < width; ++i) bytes.push_back(char(value >> (8 * i)));
}

void PutString(Bytes& bytes, const char* value)
{
	Put(bytes, std::uint32_t(std::strlen(value)), 4);
	bytes.insert(bytes.end(), value, value + std::strlen(value));
}

void Write(const std::filesystem::path& path, const Bytes& bytes)
{
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(bytes.data(), std::streamsize(bytes.size()));
}

// A sprite position array as the view reads it: a count, then (sprite,
// file position) pairs.
struct SpritePositions { std::vector<std::pair<WORD, int>> entries; };

void PutPositions(Bytes& bytes, const SpritePositions& positions)
{
	Put(bytes, std::uint32_t(positions.entries.size()), 2);
	for (const auto& entry : positions.entries)
	{
		Put(bytes, entry.first, 2);
		Put(bytes, std::uint32_t(entry.second), 4);
	}
}

// A map of width x height sectors of the sprite, no image objects, the
// zone ID the file names, and the two sprite position arrays.
Bytes MapFile(unsigned fileZoneID, unsigned width, unsigned height,
	const SpritePositions& tiles, const SpritePositions& objects, unsigned sprite = SPRITEID_NULL)
{
	Bytes bytes;
	PutString(bytes, MAP_VERSION_2000_05_10);
	Put(bytes, fileZoneID, 2);
	Put(bytes, 7, 2);			// zone group
	PutString(bytes, "zone");
	Put(bytes, 1, 1);			// zone type
	Put(bytes, 2, 1);			// zone level
	PutString(bytes, "description");
	const std::size_t offsets = bytes.size();
	Put(bytes, 0, 4);
	Put(bytes, 0, 4);
	Put(bytes, width, 2);
	Put(bytes, height, 2);
	for (unsigned i = 0; i < width * height; ++i)
	{
		Put(bytes, sprite, 2);
		Put(bytes, 0, 1);		// flag
		Put(bytes, 0, 1);		// light
	}
	Put(bytes, 0, 4);			// no image objects
	const std::uint32_t tileOffset = std::uint32_t(bytes.size());
	PutPositions(bytes, tiles);
	const std::uint32_t imageOffset = std::uint32_t(bytes.size());
	PutPositions(bytes, objects);
	for (unsigned i = 0; i < 4; ++i)
	{
		bytes[offsets + i] = char(tileOffset >> (8 * i));
		bytes[offsets + 4 + i] = char(imageOffset >> (8 * i));
	}
	return bytes;
}

// One horn: on zone zoneID, its portal at (x, y).
struct Horn { int zoneID, x, y; };

Bytes HornFile(const std::vector<Horn>& horns)
{
	Bytes bytes;
	Put(bytes, horns.empty() ? 0 : 1, 4);	// zones
	if (!horns.empty())
	{
		Put(bytes, std::uint32_t(horns.size()), 4);
		for (const Horn& horn : horns)
		{
			Put(bytes, std::uint32_t(horn.zoneID), 4);
			Put(bytes, 1, 4);					// minimap x
			Put(bytes, 2, 4);					// minimap y
			Put(bytes, std::uint32_t(horn.x), 4);
			Put(bytes, std::uint32_t(horn.y), 4);
		}
	}
	return bytes;
}

// The zone info: width x height, then the portal and safe-area records as
// given (with their counts).
Bytes InfoFile(unsigned width, unsigned height, unsigned portals, const Bytes& portalBytes,
	unsigned safeAreas, const Bytes& safeBytes)
{
	Bytes bytes;
	Put(bytes, width, 2);
	Put(bytes, height, 2);
	Put(bytes, portals, 4);
	bytes.insert(bytes.end(), portalBytes.begin(), portalBytes.end());
	Put(bytes, safeAreas, 4);
	bytes.insert(bytes.end(), safeBytes.begin(), safeBytes.end());
	return bytes;
}

// A portal to one zone, or a multi-portal to several, over the rect.
void Portal(Bytes& bytes, int type, const std::vector<WORD>& zones, BYTE left, BYTE top, BYTE right, BYTE bottom)
{
	Put(bytes, std::uint32_t(type), 1);
	if (type == MPortal::TYPE_MULTI_PORTAL) Put(bytes, std::uint32_t(zones.size()), 1);
	for (WORD zone : zones) Put(bytes, zone, 2);
	for (BYTE edge : {left, top, right, bottom}) Put(bytes, edge, 1);
}

void Safe(Bytes& bytes, BYTE flag, BYTE left, BYTE top, BYTE right, BYTE bottom)
{
	for (BYTE value : {flag, left, top, right, bottom}) Put(bytes, value, 1);
}

//----------------------------------------------------------------------
// The player, the zone and the world
//----------------------------------------------------------------------
int playersDeleted = 0;

struct PlayerProbe : MPlayer
{
	~PlayerProbe() { ++playersDeleted; }
	using MPlayer::m_SendMove;
	using MPlayer::m_listSendDirection;
	using MPlayer::m_listDirection;
	using MPlayer::m_LightSight;
	using MCreature::m_bAlive;
	using MCreature::m_ChatString;
	using MCreature::m_ChatStringCurrent;
	using MCreature::m_ActionCount;
	using MCreature::m_ActionCountMax;
	MZone* GetZone() const { return m_pZone; }
};

int creaturesDeleted = 0;
struct Walker : MCreature
{
	~Walker() { ++creaturesDeleted; }
};

int itemsDeleted = 0;
struct Trinket : MItem
{
	explicit Trinket(TYPE_OBJECTID id) { SetID(id); }
	~Trinket() { ++itemsDeleted; }
	ITEM_CLASS GetItemClass() const { return ITEM_CLASS_SWORD; }
};

// The weather, with how many effects it was asked for.
struct WeatherProbe : MWeather { using MWeather::m_nMapEffect; };

int soundTablesDeleted = 0, soundManagersDeleted = 0;
struct SoundTable : MZoneSoundTable { ~SoundTable() { ++soundTablesDeleted; } };
struct SoundManager : MZoneSoundManager { ~SoundManager() { ++soundManagersDeleted; } };

// Creature types: the player's (a slayer), an ousters and an NPC; the
// horn (659) and the ghost (716) are NPCs.
const int kSlayer = 5, kOusters = 6, kNPC = 8;
const int kHorn = 659;
const TYPE_OBJECTID kPlayerID = 77;
// The small zone 21 and the large zone 31, as the table has them.
const int kSmall = 21, kLarge = 31;

// What the tests change, given back after.
struct Saved
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
	MWeather* weather = g_pWeather;
	MSkillSet* skills = g_pSkillAvailable;
	MSkillInfoTable* skillInfo = g_pSkillInfoTable;
	MTradeManager* trade = g_pTradeManager;
	MStorage* storage = g_pStorage;
	MStorage* storage2 = g_pStorage2;
	MZoneSoundTable* zoneSounds = g_pZoneSoundTable;
	MZoneSoundManager* zoneSoundManager = g_pZoneSoundManager;
	CZoneTable* zoneTable = g_pZoneTable;
	MZone* zone = g_pZone;
	MZone* small = g_pZoneSmall;
	MZone* large = g_pZoneLarge;
	int smallID = g_nZoneSmall, largeID = g_nZoneLarge;
	bool inLarge = g_bZonePlayerInLarge;
	bool zoneSafe = g_bZoneSafe, holyLand = g_bHolyLand;
	unsigned short colorSet = g_ZoneCreatureColorSet;
	MPlayer* player = g_pPlayer;
	MTopView* view = g_pTopView;
	Properties* files = g_pFileDef;
	CSoundPartManager* soundManager = g_pSoundManager;
	UserOption* option = g_pUserOption;
	COGGSTREAM* ogg = g_pOGG;
	CServerInformation* servers = g_pServerInformation;
	Base* base = gpC_base;
	CSDLInput* input = g_pSDLInput;
	ClientPlayer* socket = g_pSocket;
	CLIENT_MODE mode = g_Mode;
	CWinUpdate* update = g_pUpdate;
	BOOL uiInput = g_bUIInput, activeApp = g_bActiveApp, drawProgress = g_bDrawProgress;
	BOOL left = g_bLButtonDown, right = g_bRButtonDown, centre = g_bCButtonDown;
	bool running = g_bRunning, netGood = g_bNetStatusGood;
	AmbientSoundState ambient = g_ZoneAmbientSounds;
	DWORD frame = g_CurrentFrame;
	MonotonicClock::TimePoint now = g_FrameNow;
	S_SLOT slot = g_char_slot_ingame;
	int previousSound = g_previousSoundID;
	std::filesystem::path directory = std::filesystem::current_path();
	const GameMainHost* gameMain = SetGameMainHost(&gameMainHost);
	const GameInitInfoHost::Host* initInfo = GameInitInfoHost::SetHost(&initInfoHost);
	const WorldEvents::Host* world = WorldEvents::SetHost(&worldHost);
	const ZoneState::Host* zoneState = ZoneState::SetHost(&zoneHost);
	const PlayerState::Host* playerState = PlayerState::SetHost(&playerHost);
	const CreatureState::Host* creatureState = CreatureState::SetHost(&creatureHost);
	const DialogPresentation::Host* dialogs = DialogPresentation::SetHost(&dialogHost);
	const MEffectHost* effects = MEffect::SetHost(&effectHost);
	const MWeatherHost* weatherOrigin = MWeather::SetHost(&weatherHost);
	~Saved()
	{
		ResetGameMainState();
		MWeather::SetHost(weatherOrigin);
		MEffect::SetHost(effects);
		DialogPresentation::SetHost(dialogs);
		CreatureState::SetHost(creatureState);
		PlayerState::SetHost(playerState);
		ZoneState::SetHost(zoneState);
		WorldEvents::SetHost(world);
		GameInitInfoHost::SetHost(initInfo);
		SetGameMainHost(gameMain);
		std::error_code error;
		std::filesystem::current_path(directory, error);
		g_previousSoundID = previousSound;
		g_char_slot_ingame = slot;
		g_FrameNow = now;
		g_CurrentFrame = frame;
		g_ZoneAmbientSounds = ambient;
		g_bNetStatusGood = netGood;
		g_bRunning = running;
		g_bCButtonDown = centre;
		g_bRButtonDown = right;
		g_bLButtonDown = left;
		g_bDrawProgress = drawProgress;
		g_bActiveApp = activeApp;
		g_bUIInput = uiInput;
		g_pUpdate = update;
		g_Mode = mode;
		g_pSocket = socket;
		g_pSDLInput = input;
		gpC_base = base;
		g_pServerInformation = servers;
		g_pOGG = ogg;
		g_pUserOption = option;
		g_pSoundManager = soundManager;
		g_pFileDef = files;
		g_pTopView = view;
		g_pPlayer = player;
		g_ZoneCreatureColorSet = colorSet;
		g_bHolyLand = holyLand;
		g_bZoneSafe = zoneSafe;
		g_bZonePlayerInLarge = inLarge;
		g_nZoneLarge = largeID;
		g_nZoneSmall = smallID;
		g_pZoneLarge = large;
		g_pZoneSmall = small;
		g_pZone = zone;
		g_pZoneTable = zoneTable;
		g_pZoneSoundManager = zoneSoundManager;
		g_pZoneSoundTable = zoneSounds;
		g_pStorage2 = storage2;
		g_pStorage = storage;
		g_pTradeManager = trade;
		g_pSkillInfoTable = skillInfo;
		g_pSkillAvailable = skills;
		g_pWeather = weather;
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

// A slayer (male, alive) standing at (3, 4) in no zone yet; the zone
// table holds the small zone 21 and the large zone 31 (their map files
// not yet written), every new zone reads an empty horn file, the image
// object pack holds one sprite, the view is a bare MTopView, the server
// group is "Bathory", the mode is the game's and GameMain's host
// records.
struct Scene
{
	Saved saved;
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
	WeatherProbe weather;
	MSkillSet skills;
	MSkillInfoTable skillInfo;
	CZoneTable zoneTable;
	Properties files;
	UserOption option;
	CServerInformation servers;
	CSDLInput input;
	MTopView view;
	std::filesystem::path dir;
	PlayerProbe* me = nullptr;

	Scene()
	{
		static const bool moveTables = (MCreature::InitMoveTable(), true);
		(void)moveTables;
		static unsigned sequence = 0;
		dir = std::filesystem::temp_directory_path() / ("darkeden_game_main_3_"
			+ std::to_string(MonotonicClock::Now().time_since_epoch().count()) + "_" + std::to_string(sequence++));
		std::filesystem::create_directories(dir);
		ResetGameMainState();
		calls.clear();
		tileSprites = 0;
		playersDeleted = creaturesDeleted = itemsDeleted = 0;
		soundTablesDeleted = soundManagersDeleted = 0;
		g_CurrentFrame = 1000;
		g_FrameNow = MonotonicClock::TimePoint() + MonotonicClock::Millis(1000000);
		creatureTypes.Init(800);
		g_pCreatureTable = &creatureTypes;
		Tribe(kSlayer, CREATURETRIBE_SLAYER);
		Tribe(kOusters, CREATURETRIBE_OUSTERS);
		Tribe(kNPC, CREATURETRIBE_NPC);
		Tribe(kHorn, CREATURETRIBE_NPC);
		Tribe(CREATURETYPE_GHOST, CREATURETRIBE_NPC);
		testfw::MutableRow(creatureTypes, kHorn).Name = "Horn";
		// The players stand in four frames and die in eight.
		for (int type : { kSlayer, kOusters })
		{
			testfw::MutableRow(creatureTypes, type).SetActionCount(ACTION_STAND, 4);
			testfw::MutableRow(creatureTypes, type).SetActionCount(ACTION_DIE, 8);
		}
		sprites.Init(4);
		g_pCreatureSpriteTable = &sprites;
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
		g_pWeather = &weather;
		g_pSkillAvailable = &skills;
		skillInfo.CTypeTable<SKILLINFO_NODE>::Init(MAX_ACTIONINFO);
		g_pSkillInfoTable = &skillInfo;
		g_pTradeManager = NULL;
		g_pStorage = g_pStorage2 = NULL;
		g_pZoneSoundTable = NULL;
		g_pZoneSoundManager = NULL;
		Zone(kSmall, true);
		Zone(kLarge, false);
		g_pZoneTable = &zoneTable;
		g_pZone = g_pZoneSmall = g_pZoneLarge = NULL;
		g_nZoneSmall = g_nZoneLarge = -1;
		g_bZonePlayerInLarge = false;
		g_bZoneSafe = g_bHolyLand = false;
		g_ZoneCreatureColorSet = 7;
		// Every new MZone reads the horn file; without one it would loop
		// over an uninitialised count (MZone.cpp:206-212).
		Write(dir / "horn.inf", HornFile({}));
		files.setProperty("FILE_INFO_HORN", (dir / "horn.inf").string());
		Bytes pack, index;
		Put(pack, 1, 2);
		Put(pack, 0, 4);
		Put(index, 1, 2);
		Put(index, 2, 4);
		Write(dir / "objects.spk", pack);
		Write(dir / "objects.spki", index);
		files.setProperty("FILE_SPRITE_IMAGEOBJECT", (dir / "objects.spk").string());
		g_pFileDef = &files;
		g_pSoundManager = NULL;
		option.PlayWaveMusic = FALSE;
		g_pUserOption = &option;
		g_pOGG = NULL;
		servers.SetServerGroupName("Bathory");
		g_pServerInformation = &servers;
		g_pUserInformation->GoreLevel = true;
		gpC_base = NULL;
		g_pSDLInput = &input;
		g_pSocket = NULL;
		g_Mode = MODE_GAME;
		g_pUpdate = NULL;
		g_bDrawProgress = FALSE;
		g_bActiveApp = TRUE;
		g_bNetStatusGood = false;
		g_pTopView = &view;
		me = new PlayerProbe;
		g_pPlayer = me;
		me->SetCreatureType(kSlayer);
		me->SetID(kPlayerID);
		me->SetPosition(3, 4);
		me->SetServerPosition(3, 4);
	}
	~Scene()
	{
		view.SetZone(NULL);
		// The zones LoadZone made first: a zone's release reads the player
		// it holds.
		delete g_pZoneSmall;
		delete g_pZoneLarge;
		g_pZoneSmall = g_pZoneLarge = g_pZone = NULL;
		delete g_pPlayer;
		g_pPlayer = NULL;
		delete g_pTradeManager;
		delete g_pStorage;
		delete g_pStorage2;
		delete g_pZoneSoundTable;
		delete g_pZoneSoundManager;
		g_pTradeManager = NULL;
		g_pStorage = g_pStorage2 = NULL;
		g_pZoneSoundTable = NULL;
		g_pZoneSoundManager = NULL;
		// The pack keeps objects.spk open, which Windows cannot delete.
		view.m_ImageObjectSPK.Release();
		std::error_code error;
		std::filesystem::current_path(saved.directory, error);
		std::filesystem::remove_all(dir, error);
	}
	// A creature type of the tribe, with a count for each action.
	void Tribe(int type, CREATURETRIBE tribe)
	{
		const int actionCount = tribe == CREATURETRIBE_VAMPIRE ? ACTION_MAX_VAMPIRE : (tribe == CREATURETRIBE_OUSTERS ? ACTION_MAX_OUSTERS : ACTION_MAX_SLAYER);
		testfw::MutableRow(creatureTypes, type).SetCreatureTribe(tribe, actionCount);
	}
	// Zone id's table row: small or large, its map and info files in the
	// temporary directory (map<id>.map, info<id>.inf).
	ZONETABLE_INFO* Zone(int id, bool small)
	{
		ZONETABLE_INFO* pInfo = new ZONETABLE_INFO;
		pInfo->ID = TYPE_ZONEID(id);
		pInfo->Property = small ? FLAG_ZONESIZE_SMALL : 0;
		pInfo->Filename = MapPath(id).c_str();
		pInfo->InfoFilename = InfoPath(id).c_str();
		pInfo->CreatureColorSet = WORD(id);
		zoneTable.Add(pInfo);
		return zoneTable.Get(TYPE_ZONEID(id));
	}
	std::string MapPath(int id) const { return (dir / ("map" + N(id) + ".map")).string(); }
	std::string InfoPath(int id) const { return (dir / ("info" + N(id) + ".inf")).string(); }
	// Zone id's map: width x height, the file naming zone fileID.
	void Map(int id, unsigned width, unsigned height, const SpritePositions& tiles = {{{1, 10}, {2, 20}}},
		const SpritePositions& objects = {{{3, 30}}}, int fileID = -1, unsigned sprite = SPRITEID_NULL)
	{
		Write(MapPath(id), MapFile(unsigned(fileID < 0 ? id : fileID), width, height, tiles, objects, sprite));
	}
	void Horns(const std::vector<Horn>& horns) { Write(dir / "horn.inf", HornFile(horns)); }
	// The zone's player, as the game has it after InitPlayer.
	void Enter(MZone* pZone)
	{
		me->SetZone(pZone);
		pZone->SetPlayer();
	}
	std::string Log() { std::string log = calls; calls.clear(); return log; }
};

std::vector<std::pair<int, int>> Entries(const CSpriteFilePositionArray* pArray)
{
	std::vector<std::pair<int, int>> entries;
	if (pArray == NULL) return entries;
	for (int i = 0; i < int(pArray->GetSize()); ++i)
		entries.push_back({ int((*pArray)[i].SpriteID), int((*pArray)[i].FilePosition) });
	return entries;
}
using Pairs = std::vector<std::pair<int, int>>;

// A zone of either size, and where LoadZone keeps it.
struct Kind
{
	int id;
	bool small;
	MZone*& Zone() const { return small ? g_pZoneSmall : g_pZoneLarge; }
	MZone*& Other() const { return small ? g_pZoneLarge : g_pZoneSmall; }
	int& Loaded() const { return small ? g_nZoneSmall : g_nZoneLarge; }
	int& OtherLoaded() const { return small ? g_nZoneLarge : g_nZoneSmall; }
	CSpriteFilePositionArray* Tiles(const MTopView& v) const { return small ? v.m_pTileSFPArraySmallZone : v.m_pTileSFPArrayLargeZone; }
	CSpriteFilePositionArray* Objects(const MTopView& v) const { return small ? v.m_pImageObjectSFPArraySmallZone : v.m_pImageObjectSFPArrayLargeZone; }
	CSpriteFilePositionArray* OtherTiles(const MTopView& v) const { return small ? v.m_pTileSFPArrayLargeZone : v.m_pTileSFPArraySmallZone; }
	CSpriteFilePositionArray* OtherObjects(const MTopView& v) const { return small ? v.m_pImageObjectSFPArrayLargeZone : v.m_pImageObjectSFPArraySmallZone; }
};
const Kind kKinds[] = { { kSmall, true }, { kLarge, false } };

// A creature of the NPC type standing at (x, y) in the zone; it counts
// its deletion.
Walker* AddWalker(MZone& zone, TYPE_OBJECTID id, int x = 1, int y = 1)
{
	Walker* walker = new Walker;
	walker->SetID(id);
	walker->SetCreatureType(kNPC);
	walker->SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
	walker->SetServerPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
	CHECK(zone.AddCreature(walker));
	return walker;
}

// The sector's portals as type:zone pairs.
Pairs Portals(const MSector& sector)
{
	Pairs portals;
	auto portal = sector.GetPortalBegin();
	for (int i = 0; i < sector.GetPortalSize(); ++i, ++portal)
		portals.push_back({ portal->Type, portal->ZoneID });
	return portals;
}

} // namespace

//======================================================================
// LoadZone
//======================================================================
TEST(GameMain3LoadZone, AnUnknownZoneIsRefusedBeforeAnythingIsTouched)
{
	Scene scene;
	scene.Map(kSmall, 4, 4);
	MZone zone(5, 5);
	g_pZone = &zone;
	AddWalker(zone, 500);
	CSoundPartManager sounds;
	g_pSoundManager = &sounds;
	g_ZoneAmbientSounds.Update(g_FrameNow, 2106, NULL, AmbientSoundPosition{ 1, 1 });
	CHECK(g_ZoneAmbientSounds.IsPropellerPlaying());
	CHECK_EQ(FALSE, LoadZone(22));
	CHECK_SAME("", scene.Log());
	CHECK(g_pZone == &zone);
	CHECK_EQ(1, zone.GetCreatureNumber());
	CHECK(g_pZoneSmall == NULL);
	CHECK(g_pZoneLarge == NULL);
	CHECK_EQ(-1, g_nZoneSmall);
	CHECK_EQ(-1, g_nZoneLarge);
	CHECK(g_ZoneAmbientSounds.IsPropellerPlaying());
	CHECK(scene.view.m_pZone == NULL);
	g_pZone = NULL;
}

TEST(GameMain3LoadZone, AZoneIsReadFromItsMapIntoTheZoneOfItsSize)
{
	for (const Kind& kind : kKinds)
	{
		Scene scene;
		// The file names zone 99; the table's ID wins.
		scene.Map(kind.id, 6, 5, {{{1, 10}, {2, 20}}}, {{{3, 30}}}, 99);
		Bytes portals;
		Portal(portals, MPortal::TYPE_NOMAL, {40}, 1, 1, 2, 1);
		Write(scene.InfoPath(kind.id), InfoFile(6, 5, 1, portals, 0, {}));
		ZONETABLE_INFO* pInfo = scene.zoneTable.Get(TYPE_ZONEID(kind.id));
		pInfo->Safety = true;
		pInfo->HolyLand = true;
		CSoundPartManager sounds;
		g_pSoundManager = &sounds;
		g_ZoneAmbientSounds.Update(g_FrameNow, 2106, NULL, AmbientSoundPosition{ 1, 1 });
		GuildWarInfo* war = new GuildWarInfo;
		war->setCastleID(ZoneID_t(1));
		scene.wars.SetWar(war);
		(*g_pTimeItemManager)[1];
		scene.view.m_mapCreature[decltype(scene.view.m_mapCreature)::key_type()] = scene.me;
		scene.view.m_bFirstTileDraw = false;
		g_bZonePlayerInLarge = kind.small;
		srand(5);
		const unsigned random = unsigned(rand());
		srand(5);
		CHECK_EQ(TRUE, LoadZone(kind.id));
		CHECK_SAME(std::string(""), scene.Log());
		MZone* pZone = kind.Zone();
		CHECK(pZone != NULL);
		CHECK(g_pZone == pZone);
		CHECK(kind.Other() == NULL);
		CHECK_EQ(kind.id, kind.Loaded());
		CHECK_EQ(-1, kind.OtherLoaded());
		CHECK_EQ(!kind.small, g_bZonePlayerInLarge);
		if (pZone == NULL) continue;
		CHECK_EQ(6, int(pZone->GetWidth()));
		CHECK_EQ(5, int(pZone->GetHeight()));
		CHECK_EQ(kind.id, int(pZone->GetID()));
		// The info file's portal (LoadZoneInfo).
		CHECK(Portals(pZone->GetSector(1, 1)) == Pairs({ { 0, 40 } }));
		CHECK(Portals(pZone->GetSector(2, 1)) == Pairs({ { 0, 40 } }));
		CHECK(Portals(pZone->GetSector(3, 1)).empty());
		// The view's sprite positions for the zone's size, from the file.
		CHECK(Entries(kind.Tiles(scene.view)) == Pairs({ { 1, 10 }, { 2, 20 } }));
		CHECK(Entries(kind.Objects(scene.view)) == Pairs({ { 3, 30 } }));
		CHECK(kind.OtherTiles(scene.view) == NULL);
		CHECK(kind.OtherObjects(scene.view) == NULL);
		CHECK_EQ(1, int(scene.view.m_ImageObjectSPK.GetSize()));
		// The view draws the zone, from scratch.
		CHECK(scene.view.m_pZone == pZone);
		CHECK(scene.view.m_bFirstTileDraw);
		CHECK(scene.view.m_mapCreature.empty());
		// The zone's flags, the war and the timed items, the world name.
		CHECK_EQ(kind.id, int(g_ZoneCreatureColorSet));
		CHECK(g_bZoneSafe);
		CHECK(g_bHolyLand);
		CHECK_EQ(0, scene.wars.getSize());
		CHECK(g_pTimeItemManager->empty());
		CHECK(g_pUserInformation->WorldName == "Bathory");
		// The sounds stopped, the next ambient sound 10 to 14 seconds on.
		CHECK(!g_ZoneAmbientSounds.IsPropellerPlaying());
		CHECK(g_ZoneAmbientSounds.GetNextSoundTime() == g_FrameNow + MonotonicClock::Millis(((random % 5) + 10) * 1000));
		CHECK_EQ(int(MODE_GAME), int(g_Mode));
	}
}

TEST(GameMain3LoadZone, WithoutASoundManagerTheAmbientSoundsAreNotTold)
{
	Scene scene;
	scene.Map(kSmall, 4, 4);
	g_ZoneAmbientSounds.Update(g_FrameNow, 2106, NULL, AmbientSoundPosition{ 1, 1 });
	CHECK_EQ(TRUE, LoadZone(kSmall));
	CHECK(g_ZoneAmbientSounds.IsPropellerPlaying());
}

TEST(GameMain3LoadZone, TheColourSetIsTheZonesBelowTheLimitAndNoneFromIt)
{
	Scene scene;
	scene.Map(kSmall, 4, 4);
	scene.Map(kLarge, 4, 4);
	scene.zoneTable.Get(kSmall)->CreatureColorSet = MAX_COLORSET - 1;
	scene.zoneTable.Get(kLarge)->CreatureColorSet = MAX_COLORSET;
	CHECK_EQ(TRUE, LoadZone(kSmall));
	CHECK_EQ(MAX_COLORSET - 1, int(g_ZoneCreatureColorSet));
	CHECK_EQ(TRUE, LoadZone(kLarge));
	CHECK_EQ(0xFFFF, int(g_ZoneCreatureColorSet));
	CHECK(!g_bZoneSafe);
	CHECK(!g_bHolyLand);
}

TEST(GameMain3LoadZone, TheZoneLoadedForTheSameIDIsKeptAndOnlyItsSpritePositionsReread)
{
	for (const Kind& kind : kKinds)
	{
		Scene scene;
		scene.Map(kind.id, 6, 5);
		CHECK_EQ(TRUE, LoadZone(kind.id));
		MZone* pZone = kind.Zone();
		if (pZone == NULL) continue;
		// The zone the player was in (elsewhere) and the kept zone hold a
		// creature each; the map file has changed since: other sectors and
		// other sprite positions, at the same offsets (the kept zone's
		// offsets are the ones read).
		AddWalker(*pZone, 501);
		MZone elsewhere(8, 8);
		AddWalker(elsewhere, 502);
		g_pZone = &elsewhere;
		scene.Map(kind.id, 6, 5, {{{4, 40}, {9, 90}}}, {{{5, 50}}}, -1, 2);
		scene.Log();
		CHECK_EQ(TRUE, LoadZone(kind.id));
		CHECK_SAME(std::string(""), scene.Log());
		CHECK(kind.Zone() == pZone);
		CHECK(g_pZone == pZone);
		CHECK_EQ(int(SPRITEID_NULL), int(pZone->GetSector(0, 0).GetSpriteID()));
		CHECK_EQ(0, pZone->GetCreatureNumber());
		CHECK_EQ(0, elsewhere.GetCreatureNumber());
		CHECK_EQ(2, creaturesDeleted);
		CHECK(Entries(kind.Tiles(scene.view)) == Pairs({ { 4, 40 }, { 9, 90 } }));
		CHECK(Entries(kind.Objects(scene.view)) == Pairs({ { 5, 50 } }));
		CHECK(scene.view.m_pZone == pZone);
	}
}

TEST(GameMain3LoadZone, APlayerWithoutGoreGetsTheTeenMapWhenTheZoneHasOne)
{
	for (const Kind& kind : kKinds)
	{
		Scene scene;
		scene.Map(kind.id, 6, 5, {{{1, 10}}});
		const std::string teen = (scene.dir / "teen.map").string();
		Write(teen, MapFile(unsigned(kind.id), 9, 7, {{{7, 70}}}, {{{8, 80}}}));
		scene.zoneTable.Get(TYPE_ZONEID(kind.id))->TeenFilename = teen.c_str();
		g_pUserInformation->GoreLevel = false;
		CHECK_EQ(TRUE, LoadZone(kind.id));
		CHECK_EQ(9, int(g_pZone->GetWidth()));
		CHECK(Entries(kind.Tiles(scene.view)) == Pairs({ { 7, 70 } }));
		// With gore, the map itself.
		g_pUserInformation->GoreLevel = true;
		kind.Loaded() = -1;
		CHECK_EQ(TRUE, LoadZone(kind.id));
		CHECK_EQ(6, int(g_pZone->GetWidth()));
		CHECK(Entries(kind.Tiles(scene.view)) == Pairs({ { 1, 10 } }));
		// Without gore and with no teen map, the map itself.
		g_pUserInformation->GoreLevel = false;
		scene.zoneTable.Get(TYPE_ZONEID(kind.id))->TeenFilename = "";
		kind.Loaded() = -1;
		CHECK_EQ(TRUE, LoadZone(kind.id));
		CHECK_EQ(6, int(g_pZone->GetWidth()));
		CHECK(Entries(kind.Tiles(scene.view)) == Pairs({ { 1, 10 } }));
	}
}

TEST(GameMain3LoadZone, AMissingMapFailsButLeavesAnEmptyZoneThatTheNextLoadReuses)
{
	for (const Kind& kind : kKinds)
	{
		Scene scene;
		scene.Map(kind.id, 6, 5);
		CHECK_EQ(TRUE, LoadZone(kind.id));
		if (kind.Zone() == NULL) continue;
		// The zone loaded before holds a creature; the player has left it.
		AddWalker(*kind.Zone(), 501);
		g_pZone = NULL;
		const int missing = kind.id + 1;
		scene.Zone(missing, kind.small);
		scene.Log();
		CHECK_EQ(FALSE, LoadZone(missing));
		CHECK_SAME("fail [Error] File Open Error : [" + scene.MapPath(missing) + "];", scene.Log());
		CHECK_EQ(int(MODE_GAME), int(g_Mode));
		// The zone before is deleted with its creature.
		CHECK_EQ(1, creaturesDeleted);
		// Recorded defect: the zone ID and a new, empty zone are kept before
		// the file is opened (GameMain.cpp:1526 and 1544, 1676 and 1694), so the
		// failed zone stays loaded as zone `missing`...
		CHECK_EQ(missing, kind.Loaded());
		CHECK(kind.Zone() != NULL);
		CHECK(g_pZone == kind.Zone());
		if (kind.Zone() == NULL) continue;
		CHECK_EQ(0, int(kind.Zone()->GetWidth()));
		CHECK_EQ(0, int(kind.Zone()->GetHeight()));
		// ...and the next load of it takes the empty zone as loaded: TRUE,
		// the view drawing a zone of no sectors (and no file is opened for
		// the zone, so no failure message either).
		MZone* pEmpty = kind.Zone();
		CHECK_EQ(TRUE, LoadZone(missing));
		CHECK_SAME(std::string(""), scene.Log());
		CHECK(kind.Zone() == pEmpty);
		CHECK(g_pZone == pEmpty);
		CHECK(scene.view.m_pZone == pEmpty);
		CHECK_EQ(0, int(pEmpty->GetWidth()));
	}
}

TEST(GameMain3LoadZone, AnUnreadableMapQuitsTheGame)
{
	for (const Kind& kind : kKinds)
	{
		Scene scene;
		Write(scene.MapPath(kind.id), Bytes{ 'x', 'y', 'z' });
		g_bUIInput = TRUE;
		CSDLInput* input = g_pSDLInput;
		CHECK_EQ(FALSE, LoadZone(kind.id));
		CHECK_SAME(std::string(""), scene.Log());
		CHECK_EQ(int(MODE_QUIT), int(g_Mode));
		CHECK(g_pUpdate == NULL);
		CHECK_EQ(FALSE, g_bUIInput);
		CHECK_EQ(FALSE, g_bActiveApp);
		CHECK(g_pSDLInput == input);
		// Recorded defect: the empty zone stays loaded as zone kind.id, as
		// for a missing map.
		CHECK_EQ(kind.id, kind.Loaded());
		CHECK(g_pZone == kind.Zone());
		if (g_pZone != NULL) CHECK_EQ(0, int(g_pZone->GetWidth()));
		CHECK(kind.Tiles(scene.view) == NULL);
		CHECK(scene.view.m_pZone == NULL);
	}
}

TEST(GameMain3LoadZone, ARejectedSpritePreloadQuitsTheGameWithTheZoneRead)
{
	for (const Kind& kind : kKinds)
	{
		// Tiles 0-3 are in the pack the view reports, sprite 1 lies under
		// every sector, and the view's tile pack is empty.
		Scene scene;
		scene.Map(kind.id, 6, 5, {{{1, 10}}}, {{{3, 30}}}, 99, 1);
		tileSprites = 4;
		CHECK_EQ(FALSE, LoadZone(kind.id));
		CHECK_SAME(std::string(""), scene.Log());
		CHECK_EQ(int(MODE_QUIT), int(g_Mode));
		CHECK_EQ(kind.id, kind.Loaded());
		CHECK(g_pZone == kind.Zone());
		if (g_pZone == NULL) continue;
		CHECK_EQ(6, int(g_pZone->GetWidth()));
		// The small zone takes the table's ID as soon as it is read; the
		// large one keeps the file's until the end of a load.
		CHECK_EQ(kind.small ? kind.id : 99, int(g_pZone->GetID()));
		CHECK(kind.Tiles(scene.view) == NULL);
		CHECK(scene.view.m_pZone == NULL);
	}
	for (const Kind& kind : kKinds)
	{
		// The tiles in the view's pack, but no image-object pack.
		Scene scene;
		scene.Map(kind.id, 6, 5, {{{1, 10}}}, {{{3, 30}}}, -1, 1);
		tileSprites = 4;
		scene.view.m_TileSPK.Init(4);
		scene.files.setProperty("FILE_SPRITE_IMAGEOBJECT", (scene.dir / "none.spk").string());
		CHECK_EQ(FALSE, LoadZone(kind.id));
		CHECK_SAME(std::string(""), scene.Log());
		CHECK_EQ(int(MODE_QUIT), int(g_Mode));
	}
	for (const Kind& kind : kKinds)
	{
		// Both packs: loaded.
		Scene scene;
		scene.Map(kind.id, 6, 5, {{{1, 10}}}, {{{3, 30}}}, -1, 1);
		tileSprites = 4;
		scene.view.m_TileSPK.Init(4);
		CHECK_EQ(TRUE, LoadZone(kind.id));
		CHECK_SAME(std::string(""), scene.Log());
		CHECK_EQ(int(MODE_GAME), int(g_Mode));
	}
}

TEST(GameMain3LoadZone, AKeptZoneWhoseMapIsGoneStillLoadsWithNoSpritePositions)
{
	for (const Kind& kind : kKinds)
	{
		Scene scene;
		scene.Map(kind.id, 6, 5);
		CHECK_EQ(TRUE, LoadZone(kind.id));
		std::filesystem::remove(scene.MapPath(kind.id));
		scene.Log();
		// Recorded defect: the map's second open (GameMain.cpp:1619, 1765),
		// a kept zone's only one, is not checked, so the view reads its
		// sprite positions from a stream that failed to open: TRUE, with
		// both arrays emptied.
		CHECK_EQ(TRUE, LoadZone(kind.id));
		CHECK_SAME(std::string(""), scene.Log());
		CHECK(kind.Tiles(scene.view) != NULL);
		CHECK(kind.Objects(scene.view) != NULL);
		CHECK(Entries(kind.Tiles(scene.view)).empty());
		CHECK(Entries(kind.Objects(scene.view)).empty());
	}
}

TEST(GameMain3LoadZone, TheObjectsOfTheZoneThePlayerWasInAreReleased)
{
	Scene scene;
	scene.Map(kSmall, 6, 5);
	scene.Map(kLarge, 7, 7);
	CHECK_EQ(TRUE, LoadZone(kLarge));
	MZone* pLarge = g_pZoneLarge;
	if (pLarge == NULL) return;
	AddWalker(*pLarge, 501);
	CHECK_EQ(TRUE, LoadZone(kSmall));
	// The large zone stays loaded, its objects released.
	CHECK(g_pZoneLarge == pLarge);
	CHECK_EQ(kLarge, g_nZoneLarge);
	CHECK_EQ(0, pLarge->GetCreatureNumber());
	CHECK_EQ(1, creaturesDeleted);
	CHECK(g_pZone == g_pZoneSmall);
	CHECK(!g_bZonePlayerInLarge);
}

TEST(GameMain3LoadZone, Zone4001TurnsEveryTileImageOff)
{
	// A square zone: the loop's swapped axes (see the top) stay on the grid.
	Scene scene;
	scene.Zone(4001, true);
	scene.Map(4001, 3, 3);
	scene.Map(kSmall, 3, 3);
	CHECK_EQ(TRUE, LoadZone(kSmall));
	CHECK(!g_pZone->GetSector(0, 0).HasDisableTileImage());
	CHECK_EQ(TRUE, LoadZone(4001));
	int disabled = 0;
	for (int y = 0; y < 3; ++y)
		for (int x = 0; x < 3; ++x)
			disabled += g_pZone->GetSector(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y)).HasDisableTileImage() ? 1 : 0;
	CHECK_EQ(9, disabled);
}

TEST(GameMain3LoadZone, Zone74ShowsAdamsCloudNorthOfRowSixty)
{
	Scene scene;
	scene.Zone(74, true);
	scene.Map(74, 4, 4);
	scene.me->MObject::SetPosition(3, 59);
	scene.me->SetShowAdamCloud(false);
	CHECK_EQ(TRUE, LoadZone(74));
	CHECK(scene.me->IsShowAdamCloud());
	scene.me->MObject::SetPosition(3, 60);
	CHECK_EQ(TRUE, LoadZone(74));
	CHECK(!scene.me->IsShowAdamCloud());
	// Another zone leaves it alone.
	scene.Map(kSmall, 4, 4);
	scene.me->SetShowAdamCloud(true);
	CHECK_EQ(TRUE, LoadZone(kSmall));
	CHECK(scene.me->IsShowAdamCloud());
}

TEST(GameMain3LoadZone, ACompetentPlayerOutsideACompetenceZoneTurnsTheGhostOn)
{
	struct Case { bool competence, shape, competenceZone, sent; };
	for (const Case& c : { Case{ true, false, false, true }, Case{ false, false, false, false },
		Case{ true, true, false, false }, Case{ true, false, true, false } })
	{
		Scene scene;
		scene.Map(kSmall, 4, 4);
		Base base;
		(base.*Get(ReceiverOf())).SetResultReceiver(Received);
		gpC_base = &base;
		g_pUserInformation->bCompetence = c.competence;
		g_pUserInformation->bCompetenceShape = c.shape;
		scene.zoneTable.Get(kSmall)->CompetenceZone = c.competenceZone;
		CHECK_EQ(TRUE, LoadZone(kSmall));
		scene.Log();
		base.DispatchMessage();
		CHECK_SAME(c.sent ? "ui" + N(UI_CHAT_RETURN) + "," + N(CLD_NORMAL) + ",0:*command ghost on;" : "", scene.Log());
	}
}

TEST(GameMain3LoadZone, TheLargeZoneMeetsItsHornsUnderTheMapsZoneID)
{
	for (const Kind& kind : kKinds)
	{
		// The map names zone 99; a horn stands on 99 and one on the table's ID.
		Scene scene;
		scene.Horns({ { 99, 2, 2 }, { kind.id, 3, 3 } });
		scene.Map(kind.id, 6, 5, {{{1, 10}}}, {{{3, 30}}}, 99);
		Write(scene.InfoPath(kind.id), InfoFile(6, 5, 0, {}, 0, {}));
		CHECK_EQ(TRUE, LoadZone(kind.id));
		CHECK_EQ(1, g_pZone->GetCreatureNumber());
		const MCreature* pHorn = g_pZone->GetCreatureNumber() == 1 ? g_pZone->GetCreatureBegin()->second : NULL;
		if (pHorn == NULL) continue;
		// Recorded defect: the large zone takes the table's ID only after
		// LoadZoneInfo (GameMain.cpp:1793-1794; the small one at 1580), so its
		// horns, and zone 1311's weather, follow the map file's zone ID.
		CHECK_EQ(kind.small ? 3 : 2, int(pHorn->GetX()));
		CHECK_EQ(kind.id, int(g_pZone->GetID()));
	}
}

//======================================================================
// MoveZone
//======================================================================
TEST(GameMain3MoveZone, ThePlayerLeavesItsZoneAndStandsInTheZoneLoaded)
{
	Scene scene;
	scene.Map(kSmall, 6, 5);
	MZone before(8, 8);
	g_pZone = &before;
	scene.Enter(&before);
	scene.Log();
	// Walking, and its light sight out of date.
	scene.me->m_listDirection.push_back(2);
	scene.me->SetTimeLightSight(9);
	scene.me->m_LightSight = 0;
	MoveZone(kSmall);
	// (The stand unlocks the gear window.)
	CHECK_SAME(std::string("unlock gear;party position;party status;"), scene.Log());
	// Stopped and standing, out of the zone before (which is not deleted).
	CHECK(scene.me->m_listDirection.empty());
	CHECK_EQ(int(ACTION_STAND), int(scene.me->GetAction()));
	CHECK(before.GetCreature(kPlayerID) == NULL);
	CHECK_EQ(0, playersDeleted);
	// In the zone loaded, its light sight worked out again.
	CHECK(g_pZone == g_pZoneSmall);
	CHECK(g_pZone != NULL && g_pZone->GetCreature(kPlayerID) == scene.me);
	CHECK(scene.me->GetZone() == g_pZone);
	CHECK_EQ(9, scene.me->GetLightSight());
}

TEST(GameMain3MoveZone, WithNoZoneThePlayerIsOnlyPutIntoTheOneLoaded)
{
	Scene scene;
	scene.Map(kLarge, 6, 5);
	scene.me->m_listDirection.push_back(2);
	MoveZone(kLarge);
	CHECK_SAME(std::string("party position;party status;"), scene.Log());
	// Not stopped.
	CHECK_EQ(1, int(scene.me->m_listDirection.size()));
	CHECK(g_pZone == g_pZoneLarge);
	CHECK(g_pZone != NULL && g_pZone->GetCreature(kPlayerID) == scene.me);
	CHECK(scene.me->GetZone() == g_pZone);
}

TEST(GameMain3MoveZone, AnUnknownZonePutsThePlayerBackIntoTheZoneItLeft)
{
	Scene scene;
	MZone before(8, 8);
	g_pZone = &before;
	scene.Enter(&before);
	scene.Log();
	// Recorded defect: MoveZone ignores LoadZone's FALSE (GameMain.cpp:1887),
	// so the player is put back where it was, as if it had moved.
	MoveZone(22);
	CHECK_SAME("unlock gear;party position;party status;", scene.Log());
	CHECK(g_pZone == &before);
	CHECK(before.GetCreature(kPlayerID) == scene.me);
	CHECK(scene.me->GetZone() == &before);
	before.RemovePlayer();
	g_pZone = NULL;
}

//======================================================================
// LoadZoneInfo
//======================================================================
namespace {

// A 6x5 zone of the table's ID as g_pZone, the test's.
struct InfoZone
{
	MZone zone{ 6, 5 };
	explicit InfoZone(int id) { zone.SetCurrentZoneID(WORD(id)); g_pZone = &zone; }
	~InfoZone() { g_pZone = NULL; }
};

} // namespace

TEST(GameMain3LoadZoneInfo, AnUnknownZoneIsRefused)
{
	Scene scene;
	InfoZone info(kSmall);
	Bytes portals;
	Portal(portals, MPortal::TYPE_NOMAL, {40}, 1, 1, 1, 1);
	Write(scene.InfoPath(22), InfoFile(6, 5, 1, portals, 0, {}));
	CHECK_EQ(FALSE, LoadZoneInfo(22));
	CHECK(Portals(info.zone.GetSector(1, 1)).empty());
}

TEST(GameMain3LoadZoneInfo, AZoneWithoutAnInfoFileHasNothingToPublish)
{
	Scene scene;
	// Not even a zone is read.
	CHECK_EQ(TRUE, LoadZoneInfo(kSmall));
	InfoZone info(kSmall);
	CHECK_EQ(TRUE, LoadZoneInfo(kSmall));
	CHECK(Portals(info.zone.GetSector(0, 0)).empty());
	CHECK_EQ(0, int(info.zone.GetSector(0, 0).GetSafe()));
}

TEST(GameMain3LoadZoneInfo, PortalsLeadEverySectorOfTheirRectToTheirZones)
{
	Scene scene;
	InfoZone info(kSmall);
	Bytes portals;
	Portal(portals, MPortal::TYPE_NOMAL, {40}, 1, 1, 2, 1);
	Portal(portals, MPortal::TYPE_MULTI_PORTAL, {41, 42}, 4, 3, 4, 3);
	// Past the zone's edge: clipped; reversed: ordered.
	Portal(portals, MPortal::TYPE_SLAYER_ONLY, {43}, 5, 4, 9, 9);
	Portal(portals, MPortal::TYPE_VAMPIRE_ONLY, {44}, 3, 0, 2, 0);
	Write(scene.InfoPath(kSmall), InfoFile(6, 5, 4, portals, 0, {}));
	CHECK_EQ(TRUE, LoadZoneInfo(kSmall));
	const MZone& zone = info.zone;
	CHECK(Portals(zone.GetSector(1, 1)) == Pairs({ { MPortal::TYPE_NOMAL, 40 } }));
	CHECK(Portals(zone.GetSector(2, 1)) == Pairs({ { MPortal::TYPE_NOMAL, 40 } }));
	CHECK(Portals(zone.GetSector(0, 1)).empty());
	CHECK(Portals(zone.GetSector(3, 1)).empty());
	CHECK(Portals(zone.GetSector(4, 3)) == Pairs({ { MPortal::TYPE_MULTI_PORTAL, 41 }, { MPortal::TYPE_MULTI_PORTAL, 42 } }));
	CHECK(Portals(zone.GetSector(5, 4)) == Pairs({ { MPortal::TYPE_SLAYER_ONLY, 43 } }));
	CHECK(Portals(zone.GetSector(2, 0)) == Pairs({ { MPortal::TYPE_VAMPIRE_ONLY, 44 } }));
	CHECK(Portals(zone.GetSector(3, 0)) == Pairs({ { MPortal::TYPE_VAMPIRE_ONLY, 44 } }));
	int portalSectors = 0;
	for (int y = 0; y < 5; ++y)
		for (int x = 0; x < 6; ++x)
			portalSectors += zone.GetSector(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y)).GetPortalSize() > 0 ? 1 : 0;
	CHECK_EQ(6, portalSectors);
}

TEST(GameMain3LoadZoneInfo, SafeAreasGiveTheirSectorsTheSafetyOfTheirFlags)
{
	Scene scene;
	InfoZone info(kSmall);
	MZone& zone = info.zone;
	for (int x = 0; x < 6; ++x) zone.SetSafeSector(RECT{ x, 4, x, 4 }, FLAG_SECTOR_SAFE_NO_PK_ZONE);
	Bytes safe;
	Safe(safe, FLAG_MIP_SAFE_COMMON, 0, 0, 0, 0);
	Safe(safe, FLAG_MIP_SAFE_SLAYER, 1, 0, 1, 0);
	Safe(safe, FLAG_MIP_SAFE_VAMPIRE, 2, 0, 2, 0);
	Safe(safe, FLAG_MIP_SAFE_OUSTERS, 3, 0, 3, 0);
	Safe(safe, FLAG_MIP_SAFE_COMMON | FLAG_MIP_SAFE_SLAYER | FLAG_MIP_SAFE_VAMPIRE | FLAG_MIP_SAFE_OUSTERS, 4, 0, 5, 1);
	// No flag: skipped.
	Safe(safe, 0, 0, 4, 0, 4);
	// A later area over an earlier one replaces its safety.
	Safe(safe, FLAG_MIP_SAFE_SLAYER, 1, 4, 1, 4);
	Safe(safe, FLAG_MIP_SAFE_VAMPIRE, 1, 4, 1, 4);
	// Recorded defect: a flag of no known bit is not skipped like no flag
	// (GameMain.cpp:2011 tests flag != 0, not fSafe != 0), so it clears the
	// sectors' safety.
	Safe(safe, 0x80, 2, 4, 2, 4);
	Write(scene.InfoPath(kSmall), InfoFile(6, 5, 0, {}, 9, safe));
	CHECK_EQ(TRUE, LoadZoneInfo(kSmall));
	CHECK_EQ(FLAG_SECTOR_SAFE_COMMON, int(zone.GetSector(0, 0).GetSafe()));
	CHECK_EQ(FLAG_SECTOR_SAFE_SLAYER, int(zone.GetSector(1, 0).GetSafe()));
	CHECK_EQ(FLAG_SECTOR_SAFE_VAMPIRE, int(zone.GetSector(2, 0).GetSafe()));
	CHECK_EQ(FLAG_SECTOR_SAFE_OUSTERS, int(zone.GetSector(3, 0).GetSafe()));
	const int all = FLAG_SECTOR_SAFE_COMMON | FLAG_SECTOR_SAFE_SLAYER | FLAG_SECTOR_SAFE_VAMPIRE | FLAG_SECTOR_SAFE_OUSTERS;
	CHECK_EQ(all, int(zone.GetSector(4, 0).GetSafe()));
	CHECK_EQ(all, int(zone.GetSector(5, 1).GetSafe()));
	CHECK_EQ(0, int(zone.GetSector(3, 1).GetSafe()));
	CHECK_EQ(FLAG_SECTOR_SAFE_NO_PK_ZONE, int(zone.GetSector(0, 4).GetSafe()));
	CHECK_EQ(FLAG_SECTOR_SAFE_VAMPIRE, int(zone.GetSector(1, 4).GetSafe()));
	CHECK_EQ(0, int(zone.GetSector(2, 4).GetSafe()));
	CHECK_EQ(FLAG_SECTOR_SAFE_NO_PK_ZONE, int(zone.GetSector(3, 4).GetSafe()));
}

TEST(GameMain3LoadZoneInfo, AnInfoFileThatDoesNotFitTheZoneIsRefusedWhole)
{
	Scene scene;
	InfoZone info(kSmall);
	Bytes portals, safe;
	Portal(portals, MPortal::TYPE_NOMAL, {40}, 1, 1, 1, 1);
	Safe(safe, FLAG_MIP_SAFE_COMMON, 0, 0, 0, 0);
	// Another zone's size.
	Write(scene.InfoPath(kSmall), InfoFile(6, 6, 1, portals, 1, safe));
	CHECK_EQ(FALSE, LoadZoneInfo(kSmall));
	// Cut short in its last record.
	Bytes cut = InfoFile(6, 5, 1, portals, 1, safe);
	cut.pop_back();
	Write(scene.InfoPath(kSmall), cut);
	CHECK_EQ(FALSE, LoadZoneInfo(kSmall));
	CHECK(Portals(info.zone.GetSector(1, 1)).empty());
	CHECK_EQ(0, int(info.zone.GetSector(0, 0).GetSafe()));
	// Whole, it is published.
	Write(scene.InfoPath(kSmall), InfoFile(6, 5, 1, portals, 1, safe));
	CHECK_EQ(TRUE, LoadZoneInfo(kSmall));
	CHECK(Portals(info.zone.GetSector(1, 1)) == Pairs({ { MPortal::TYPE_NOMAL, 40 } }));
	CHECK_EQ(FLAG_SECTOR_SAFE_COMMON, int(info.zone.GetSector(0, 0).GetSafe()));
}

TEST(GameMain3LoadZoneInfo, TheOustersHornsStandOnTheirPortalsInTheirZone)
{
	Scene scene;
	MZone zone(10, 12);
	zone.SetCurrentZoneID(kSmall);
	g_pZone = &zone;
	// Horns on this zone at (2, 3) and off the map, and one on another zone.
	zone.GetHorn().push_back({ { kSmall, 1, 2, 2, 3 }, { kSmall + 1, 1, 2, 4, 4 } });
	zone.GetHorn().push_back({ { kSmall, 1, 2, 20, 20 } });
	Write(scene.InfoPath(kSmall), InfoFile(10, 12, 0, {}, 0, {}));
	CHECK_EQ(TRUE, LoadZoneInfo(kSmall));
	CHECK_EQ(1, zone.GetCreatureNumber());
	MNPC* pHorn = zone.GetCreatureNumber() == 1 ? dynamic_cast<MNPC*>(zone.GetCreatureBegin()->second) : NULL;
	CHECK(pHorn != NULL);
	if (pHorn != NULL)
	{
		CHECK_EQ(kHorn, int(pHorn->GetCreatureType()));
		CHECK_EQ(kHorn, int(pHorn->GetNPCID()));
		CHECK(std::string(pHorn->GetName()) == "Horn");
		CHECK_EQ(GUILDID_OUSTERS, pHorn->GetGuildNumber());
		CHECK(pHorn->IsGroundCreature());
		CHECK_EQ(2, int(pHorn->GetX()));
		CHECK_EQ(3, int(pHorn->GetY()));
		CHECK_EQ(2, int(pHorn->GetServerX()));
		CHECK_EQ(3, int(pHorn->GetServerY()));
		CHECK_EQ(1, int(pHorn->GetServerDirection()));
		CHECK_EQ(1, int(pHorn->GetDirection()));
		CHECK_EQ(int(ACTION_STAND), int(pHorn->GetAction()));
		CHECK_EQ(100, int(pHorn->GetHP()));
		CHECK_EQ(100, int(pHorn->GetMAX_HP()));
	}
	g_pZone = NULL;
}

TEST(GameMain3LoadZoneInfo, Zone62sHornStandsAtSevenElevenWhateverItsPortal)
{
	Scene scene;
	scene.Zone(62, true);
	MZone zone(10, 12);
	zone.SetCurrentZoneID(62);
	g_pZone = &zone;
	zone.GetHorn().push_back({ { 62, 1, 2, 2, 3 } });
	Write(scene.InfoPath(62), InfoFile(10, 12, 0, {}, 0, {}));
	CHECK_EQ(TRUE, LoadZoneInfo(62));
	const MCreature* pHorn = zone.GetCreatureNumber() == 1 ? zone.GetCreatureBegin()->second : NULL;
	CHECK(pHorn != NULL);
	if (pHorn != NULL)
	{
		CHECK_EQ(7, int(pHorn->GetX()));
		CHECK_EQ(11, int(pHorn->GetY()));
		CHECK_EQ(2, int(pHorn->GetServerX()));
		CHECK_EQ(3, int(pHorn->GetServerY()));
	}
	g_pZone = NULL;
}

TEST(GameMain3LoadZoneInfo, Zone1311TurnsTheSpotWeatherOn)
{
	Scene scene;
	scene.Zone(1311, true);
	Write(scene.InfoPath(1311), InfoFile(6, 5, 0, {}, 0, {}));
	Write(scene.InfoPath(kSmall), InfoFile(6, 5, 0, {}, 0, {}));
	{
		InfoZone info(kSmall);
		CHECK_EQ(TRUE, LoadZoneInfo(kSmall));
		CHECK(!scene.weather.IsActive());
	}
	InfoZone info(1311);
	CHECK_EQ(TRUE, LoadZoneInfo(1311));
	CHECK_EQ(int(MWeather::WEATHER_SPOT), int(scene.weather.GetWeatherType()));
	CHECK_EQ(2 << 3, int(scene.weather.m_nMapEffect));
}

//======================================================================
// InitPlayer
//======================================================================
TEST(GameMain3InitPlayer, ThePlayerStandsInTheZoneWithEverythingItWaitedForCleared)
{
	Scene scene;
	MZone zone(10, 10);
	g_pZone = &zone;
	PlayerProbe& me = *scene.me;
	me.m_listDirection.push_back(2);
	me.SetChatString("hello");
	me.m_SendMove = 3;
	me.m_listSendDirection.push_back(4);
	me.SetWaitVerify(MPlayer::WAIT_VERIFY_TRADE);
	Trinket waiting(9);
	me.SetItemCheckBuffer(&waiting, MPlayer::ITEM_CHECK_BUFFER_PICKUP_TO_MOUSE);
	scene.view.SetFadeStart(1, 31, 10, 31, 31, 31);
	CHECK(scene.view.IsFade());
	g_char_slot_ingame.Race = RACE_VAMPIRE;
	g_char_slot_ingame.bl_female = true;
	scene.Log();
	CHECK_EQ(TRUE, InitPlayer(5, 6, 3));
	// Into the zone, standing (which unlocks the gear), then the item
	// buffer cleared (which unlocks the gear, the items and the trade).
	CHECK_SAME("party position;party status;unlock gear;unlock gear;unlock item;unlock trade;", scene.Log());
	CHECK_EQ(5, int(me.GetX()));
	CHECK_EQ(6, int(me.GetY()));
	CHECK_EQ(5, int(me.GetServerX()));
	CHECK_EQ(6, int(me.GetServerY()));
	CHECK_EQ(3, int(me.GetServerDirection()));
	CHECK_EQ(3, int(me.GetDirection()));
	CHECK(me.m_listDirection.empty());
	CHECK(zone.GetCreature(kPlayerID) == &me);
	CHECK(me.GetZone() == &zone);
	CHECK_EQ(int(ACTION_STAND), int(me.GetAction()));
	CHECK_EQ(0, int(me.m_ActionCount));
	CHECK_EQ(4, int(me.m_ActionCountMax));
	CHECK_EQ(0, int(me.m_SendMove));
	CHECK(me.m_listSendDirection.empty());
	CHECK(g_bNetStatusGood);
	CHECK(me.IsWaitVerifyNULL());
	CHECK(me.GetItemCheckBuffer() == NULL);
	CHECK_EQ(int(MPlayer::ITEM_CHECK_BUFFER_NULL), int(me.GetItemCheckBufferStatus()));
	CHECK_EQ(0, int(me.m_ChatStringCurrent));
	CHECK_EQ('\0', me.m_ChatString[0][0]);
	CHECK(!scene.view.IsFade());
	CHECK_EQ(int(RACE_SLAYER), int(g_char_slot_ingame.Race));
	CHECK(!g_char_slot_ingame.bl_female);
	zone.RemovePlayer();
	g_pZone = NULL;
}

TEST(GameMain3InitPlayer, ADeadPlayerLiesDead)
{
	Scene scene;
	MZone zone(10, 10);
	g_pZone = &zone;
	PlayerProbe& me = *scene.me;
	me.SetCreatureType(kOusters);
	me.SetFemale();
	me.m_bAlive = false;
	CHECK_EQ(TRUE, InitPlayer(1, 2, 7));
	CHECK_EQ(int(ACTION_DIE), int(me.GetAction()));
	CHECK_EQ(8, int(me.m_ActionCountMax));
	CHECK(zone.GetCreature(kPlayerID) == &me);
	CHECK_EQ(int(RACE_OUSTERS), int(g_char_slot_ingame.Race));
	CHECK(g_char_slot_ingame.bl_female);
	zone.RemovePlayer();
	g_pZone = NULL;
}

//======================================================================
// ReleaseGameObject
//======================================================================
TEST(GameMain3ReleaseGameObject, TheWorldsObjectsAreReleasedAndThePlayersDeleted)
{
	Scene scene;
	scene.weather.SetRain(1);
	CHECK(scene.weather.IsActive());
	PARTY_INFO* ally = new PARTY_INFO;
	ally->Name = "Ally";
	ally->ID = 5;
	CHECK(scene.party.AddMember(ally));
	MEvent event;
	event.eventID = EVENTID_RESURRECT;
	scene.events.AddEvent(event);
	GuildWarInfo* war = new GuildWarInfo;
	war->setCastleID(ZoneID_t(1));
	scene.wars.SetWar(war);
	(*g_pTimeItemManager)[1];
	MZone zone(6, 5);
	g_pZone = &zone;
	scene.Enter(&zone);
	AddWalker(zone, 501);
	g_pZoneSoundTable = new SoundTable;
	g_pZoneSoundManager = new SoundManager;
	g_pItemTable->InitClass(ITEM_CLASS_SWORD, 1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).SetGrid(1, 1);
	g_pTradeManager = new MTradeManager;
	g_pTradeManager->Init();
	CHECK(g_pTradeManager->GetOtherInventory()->AddItem(new Trinket(1)));
	g_pStorage = new MStorage;
	g_pStorage->Init(1);
	CHECK(g_pStorage->SetItem(0, new Trinket(2)));
	g_pStorage2 = new MStorage;
	g_pStorage2->Init(1);
	CHECK(g_pStorage2->SetItem(0, new Trinket(3)));
	scene.me = nullptr;
	ReleaseGameObject();
	CHECK(!scene.weather.IsActive());
	CHECK_EQ(0, int(scene.weather.GetSize()));
	CHECK_EQ(0, scene.party.GetSize());
	CHECK(scene.events.IsEmptyEvent());
	CHECK_EQ(0, scene.wars.getSize());
	CHECK(g_pTimeItemManager->empty());
	// The zone loses its objects and the player, but is not deleted.
	CHECK(g_pZone == NULL);
	CHECK_EQ(0, zone.GetCreatureNumber());
	CHECK_EQ(1, creaturesDeleted);
	CHECK(g_pZoneSoundTable == NULL);
	CHECK(g_pZoneSoundManager == NULL);
	CHECK_EQ(1, soundTablesDeleted);
	CHECK_EQ(1, soundManagersDeleted);
	CHECK(g_pPlayer == NULL);
	CHECK_EQ(1, playersDeleted);
	CHECK(g_pTradeManager == NULL);
	CHECK(g_pStorage == NULL);
	CHECK(g_pStorage2 == NULL);
	CHECK_EQ(3, itemsDeleted);
}

TEST(GameMain3ReleaseGameObject, WithNoZoneTheZoneSoundsSurvive)
{
	Scene scene;
	delete scene.me;
	scene.me = nullptr;
	CHECK(g_pPlayer == NULL);
	playersDeleted = 0;
	SoundTable* table = new SoundTable;
	SoundManager* manager = new SoundManager;
	g_pZoneSoundTable = table;
	g_pZoneSoundManager = manager;
	ReleaseGameObject();
	// Note: a state the game cannot reach, since it never creates the zone
	// sounds (their only assignments set NULL). They are deleted only inside
	// the zone's branch (GameMain.cpp:1352-1362), so with no zone they stay.
	CHECK(g_pZoneSoundTable == table);
	CHECK(g_pZoneSoundManager == manager);
	CHECK_EQ(0, soundTablesDeleted);
	CHECK_EQ(0, soundManagersDeleted);
	CHECK_EQ(0, playersDeleted);
}

//======================================================================
// LoadCreature, ReleaseUselessCreatureSPKExcept
//======================================================================
TEST(GameMain3LoadCreature, TheViewLoadsTheSpriteTypeAndReleasesTheUnusedOnes)
{
	Scene scene;
	for (int type : { 1, 2 })
	{
		CREATURESPRITETABLE_INFO& row = testfw::MutableRow(scene.sprites, type);
		row.CreatureType = FLAG_CREATURESPRITE_MONSTER;
		row.FrameID = FRAMEID_NULL;
		row.FirstSpriteID = row.LastSpriteID = SPRITEID_NULL;
		row.FirstShadowSpriteID = row.LastShadowSpriteID = SPRITEID_NULL;
		row.bLoad = FALSE;
	}
	scene.option.IsPreLoadMonster = FALSE;
	CHECK_EQ(TRUE, LoadCreature(1));
	CHECK_EQ(TRUE, LoadCreature(2));
	CHECK(testfw::MutableRow(scene.sprites, 1).bLoad != FALSE);
	CHECK(testfw::MutableRow(scene.sprites, 2).bLoad != FALSE);
	CHECK_EQ(2, scene.view.m_listLoadedCreatureSprite.GetSize());
	// Past the table: nothing, still TRUE.
	CHECK_EQ(TRUE, LoadCreature(4));
	CHECK_EQ(2, scene.view.m_listLoadedCreatureSprite.GetSize());
	MTopView::INT_ORDERED_LIST used;
	used.Add(2);
	ReleaseUselessCreatureSPKExcept(used);
	CHECK(testfw::MutableRow(scene.sprites, 1).bLoad == FALSE);
	CHECK(testfw::MutableRow(scene.sprites, 2).bLoad != FALSE);
	CHECK_EQ(1, scene.view.m_listLoadedCreatureSprite.GetSize());
}

//======================================================================
// Add_GDR_Ghost
//======================================================================
namespace {

const char* kGhosts =
	"<GhostPos>"
	"<PositionList_1412>"
	"<Position1 x='2' y='3'/>"
	"<Position2 x='-1' y='3'/>"
	"<Position3 x='6' y='3'/>"
	"<Position4 x='2' y='5'/>"
	"<Position5 x='5' y='4'/>"
	"<Position6 y='1'/>"
	"<Other x='1' y='1'/>"
	"</PositionList_1412>"
	"<PositionList_1413>"
	"<Position1 x='1' y='1'/>"
	"</PositionList_1413>"
	"<PositionList_1411><Position1 x='1' y='2'/></PositionList_1411>"
	"<PositionList_1414><Position1 x='1' y='2'/></PositionList_1414>"
	"<PositionList_0><Position1 x='1' y='2'/></PositionList_0>"
	"</GhostPos>";

// The ghosts' file beside the tutorial pack, the working directory the
// scene's temporary one.
void GhostFile(Scene& scene, const char* text)
{
	std::filesystem::create_directories(scene.dir / "data" / "ui" / "txt");
	Write(scene.dir / "data" / "ui" / "txt" / "ghostPos.xml", Bytes(text, text + std::strlen(text)));
	std::filesystem::current_path(scene.dir);
}

// The zone's fake creatures as (x, y) pairs, each checked to be a ghost
// moving underground with one of the two ghost looks.
Pairs Ghosts(MZone& zone)
{
	Pairs ghosts;
	auto ghost = zone.GetFakeCreatureBegin();
	for (int i = 0; i < zone.GetFakeCreatureNumber(); ++i, ++ghost)
	{
		const MFakeCreature* pGhost = dynamic_cast<const MFakeCreature*>(ghost->second);
		CHECK(pGhost != NULL);
		if (pGhost == NULL) continue;
		CHECK_EQ(CREATURETYPE_GHOST, int(pGhost->GetCreatureType()));
		CHECK_EQ(int(MFakeCreature::FAKE_CREATURE_GHOST), int(pGhost->GetFakeCreatureType()));
		CHECK_EQ(int(MCreature::CREATURE_FAKE_UNDERGROUND), int(pGhost->GetMoveType()));
		CHECK(pGhost->GetDirection() < 8);
		CHECK(pGhost->HasEffectStatus(EFFECTSTATUS_GHOST_1) != pGhost->HasEffectStatus(EFFECTSTATUS_GHOST_2));
		ghosts.push_back({ pGhost->GetX(), pGhost->GetY() });
	}
	return ghosts;
}

} // namespace

TEST(GameMain3Ghosts, ZoneFourteenTwelveSpawnsAGhostAtEachPositionOnTheMap)
{
	Scene scene;
	GhostFile(scene, kGhosts);
	MZone zone(6, 5);
	g_pZone = &zone;
	Add_GDR_Ghost(1412);
	// Off the map, with no x, or not named Position<n> for n up to the
	// count: none.
	CHECK(Ghosts(zone) == Pairs({ { 2, 3 }, { 5, 4 } }));
	g_pZone = NULL;
}

TEST(GameMain3Ghosts, ZoneFourteenThirteenReadsItsOwnList)
{
	Scene scene;
	GhostFile(scene, kGhosts);
	MZone zone(6, 5);
	g_pZone = &zone;
	Add_GDR_Ghost(1413);
	CHECK(Ghosts(zone) == Pairs({ { 1, 1 } }));
	g_pZone = NULL;
}

TEST(GameMain3Ghosts, OnlyZonesFourteenTwelveAndThirteenWithAZoneAndAListHaveGhosts)
{
	Scene scene;
	GhostFile(scene, kGhosts);
	MZone zone(6, 5);
	g_pZone = &zone;
	// Other zones, though the file has their lists.
	for (int other : { 1411, 1414, 0 })
		Add_GDR_Ghost(other);
	CHECK_EQ(0, zone.GetFakeCreatureNumber());
	// No zone: the list is not read (it would be placed on a NULL zone).
	g_pZone = NULL;
	Add_GDR_Ghost(1412);
	g_pZone = &zone;
	// No list for the zone.
	GhostFile(scene, "<GhostPos><PositionList_1412><Position1 x='1' y='1'/></PositionList_1412></GhostPos>");
	Add_GDR_Ghost(1413);
	CHECK_EQ(0, zone.GetFakeCreatureNumber());
	// No file.
	std::filesystem::remove(scene.dir / "data" / "ui" / "txt" / "ghostPos.xml");
	Add_GDR_Ghost(1412);
	CHECK_EQ(0, zone.GetFakeCreatureNumber());
	g_pZone = NULL;
}
