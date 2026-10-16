#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "MTopView.h"
#include "MPlayer.h"
#include "MFakeCreature.h"
#include "MZone.h"
#include "MSector.h"
#include "MCreature.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "EffectSpriteTypeDef.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "MZoneTable.h"
#include "MSkillManager.h"
#include "MObjectSelector.h"
#include "SystemAvailabilities.h"
#include "MEffect.h"
#include "MEventManager.h"
#include "MEventQueue.h"
#include "MWeather.h"
#include "MPortal.h"
#include "UserOption.h"
#include "FrameClock.h"
#include "ViewSectors.h"
#include "MViewDef.h"
#include "SpriteIDDef.h"
#include "DirectionSelection.h"
#include "WorldTileGeometry.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "CSpriteSurface.h"
#include "CIndexSprite.h"
#include "CSprite.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <random>
#include <vector>

// The globals the view's picking reads and writes (MObjectScreen.cpp's
// and MTopView.cpp's), and the creature filter MTopViewDraw.cpp exports.
extern int		g_DrawCamGapX;
extern int		g_DrawCamGapY;
extern POINT	g_MouseSector;
extern bool		g_bMouseInPortal;
bool IsEscapeDrawCreatureFunction(MCreature* pCreature);

// The view's protected members this file drives (MTopView.h makes it a
// friend).
struct MTopViewTestAccess2
{
	static void Determine(MTopView& view) { view.DetermineImageObject(); }
	static void Update(MTopView& view, POINT first) { view.UpdateImageObject(first); }
	static void DrawTileSurface(MTopView& view) { view.DrawTileSurface(); }
	static void DrawZone(MTopView& view, int x, int y) { view.DrawZone(x, y); }
};
using Access = MTopViewTestAccess2;

// The view (task 4.260: the executable's until then, gameclient's since):
// where the screen is in the zone, what the mouse is over, which image
// objects are in sight, the light buffers and the tiles the frame draws.
// A real zone stands behind ZoneState as g_pZone and the view's own zone;
// the player is a real MPlayer; every object a test makes is its own and
// sits on the zone's sectors, never in the zone's maps unless the test
// says so, so the zone frees nothing of the test's.
namespace {

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
// Everyone may be picked: no forced attack, no hallucination, select all.
const MObjectSelectorHost selectorHost = []() {
	MObjectSelectorHost host{};
	host.ForceAttack = []() { return false; };
	host.PlayerHasEffectStatus = [](EFFECTSTATUS) { return false; };
	return host;
}();

struct ZoneProbe : MZone
{
	using MZone::MZone;
	using MZone::SectorAt;
	using MZone::m_Info;
	using MZone::m_mapCreature;
	using MZone::m_mapFakeCreature;
	using MZone::m_mapGroundEffect;
};

// A creature a test places and poses: alive, standing, facing down, on
// frame 0, with no sector offset.
struct Creature : MCreature
{
	using MCreature::m_sX;
	using MCreature::m_sY;
	using MCreature::m_Z;
	using MCreature::m_DarknessCount;
	using MCreature::m_bCutHeight;
	using MCreature::m_bAlive;
	using MCreature::m_Action;
	using MCreature::m_CurrentDirection;
	using MCreature::m_bEffectStatus;

	Creature(TYPE_OBJECTID id, int type, int x, int y)
	{
		SetCreatureType(TYPE_CREATURETYPE(type));
		SetID(id);
		MObject::SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
		m_Action = ACTION_STAND;
		m_CurrentDirection = 2;
	}
	void Rect(int left, int top, int right, int bottom)
	{
		RECT rect = { left, top, right, bottom };
		SetScreenRect(&rect);
	}
	void Set(EFFECTSTATUS status, bool on = true) { m_bEffectStatus[status] = on; }
};

struct Pet : MFakeCreature
{
	using MCreature::m_DarknessCount;
	void Rect(int left, int top, int right, int bottom)
	{
		RECT rect = { left, top, right, bottom };
		SetScreenRect(&rect);
	}
};

struct PlayerProbe : MPlayer
{
	using MCreature::m_sX;
	using MCreature::m_sY;
	using MCreature::m_Z;
	using MCreature::m_bEffectStatus;
	using MCreature::m_DarknessCount;
	using MCreature::m_MoveType;
	using MCreature::m_Action;
	using MCreature::m_CurrentDirection;
	using MPlayer::m_LightSight;
	using MPlayer::m_TimeLightSight;
	using MPlayer::m_Sight;
	void Set(EFFECTSTATUS status, bool on = true) { m_bEffectStatus[status] = on; }
	void At(int x, int y) { MObject::SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y)); }
};

// An effect the mouse may pick, and one it may not.
struct Pickable : MSelectableEffect
{
	Pickable() : MSelectableEffect(BLT_EFFECT) {}
	void Rect(int left, int top, int right, int bottom)
	{
		RECT rect = { left, top, right, bottom };
		SetScreenRect(&rect);
	}
};
struct Unpickable : MEffect
{
	Unpickable() : MEffect(BLT_EFFECT) {}
	void Rect(int left, int top, int right, int bottom)
	{
		RECT rect = { left, top, right, bottom };
		SetScreenRect(&rect);
	}
};

// Creature types: a slayer and an ousters (players, no body of their own),
// a vampire and an NPC, both with the body of frame set 1.
const int kSlayer = 1, kVampire = 2, kOusters = 3, kNPC = 4;
const TYPE_OBJECTID kPlayerID = 1000;

struct Scene
{
	GameModelWorld world;
	CREATURE_TABLE creatureTypes;
	CREATURESPRITE_TABLE sprites;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	RankBonusTable rankBonuses;
	MEventQueue events;
	MObjectSelector selector{ &selectorHost };
	CZoneTable zoneTable;
	SystemAvailabilitiesManager systems;
	UserOption option;
	MWeather weather;
	MSkillSet skills;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MEventQueue* oldEvents = g_pEventManager;
	MObjectSelector* oldSelector = g_pObjectSelector;
	CZoneTable* oldZoneTable = g_pZoneTable;
	SystemAvailabilitiesManager* oldSystems = g_pSystemAvailableManager;
	UserOption* oldOption = g_pUserOption;
	MWeather* oldWeather = g_pWeather;
	MSkillSet* oldSkills = g_pSkillAvailable;
	MPlayer* oldPlayer = g_pPlayer;
	MZone* oldZone = g_pZone;
	DWORD oldFrame = g_CurrentFrame;
	bool oldFrameChanged = g_bFrameChanged;
	int oldGapX = g_DrawCamGapX, oldGapY = g_DrawCamGapY;
	POINT oldMouseSector = g_MouseSector;
	bool oldInPortal = g_bMouseInPortal;
	float oldLightWidth = MTopView::s_LightWidth, oldLightHeight = MTopView::s_LightHeight;
	const ZoneState::Host* oldZoneHost = ZoneState::SetHost(&zoneHost);
	const PlayerState::Host* oldPlayerHost = PlayerState::SetHost(&playerHost);
	const CreatureState::Host* oldCreatureHost = CreatureState::SetHost(&creatureHost);
	const MEffectHost* oldEffectHost = MEffect::SetHost(&effectHost);
	std::unique_ptr<ZoneProbe> zone;
	PlayerProbe* player = nullptr;
	CSpriteSurface screen;
	CSpriteSurface* pScreen = &screen;
	std::unique_ptr<MTopView> view;

	explicit Scene(int width = 30, int height = 30)
	{
		CHECK_EQ(0, spritectl_init());
		g_CurrentFrame = 0;
		g_bFrameChanged = false;
		g_DrawCamGapX = g_DrawCamGapY = 0;
		creatureTypes.Init(20);
		g_pCreatureTable = &creatureTypes;
		Type(kSlayer, CREATURETRIBE_SLAYER, 0);
		Type(kVampire, CREATURETRIBE_VAMPIRE, 3);
		Type(kOusters, CREATURETRIBE_OUSTERS, 2);
		Type(kNPC, CREATURETRIBE_NPC, 3);
		sprites.Init(4);
		for (int type = 0; type < 4; ++type)
		{
			CREATURESPRITETABLE_INFO& row = testfw::MutableRow(sprites, type);
			row.FrameID = TYPE_FRAMEID(type == 3 ? 1 : FRAMEID_NULL);
			row.CreatureType = BYTE(type == 0 ? FLAG_CREATURESPRITE_PLAYER_SLAYER
				: type == 2 ? FLAG_CREATURESPRITE_PLAYER_OUSTERS : FLAG_CREATURESPRITE_MONSTER);
		}
		g_pCreatureSpriteTable = &sprites;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		g_pRankBonusTable = &rankBonuses;
		g_pEventManager = &events;
		g_pObjectSelector = &selector;
		g_pZoneTable = &zoneTable;
		g_pSystemAvailableManager = &systems;
		g_pUserOption = &option;
		g_pWeather = &weather;
		g_pSkillAvailable = NULL;	// the player's type change would refresh it
		zone.reset(new ZoneProbe(TYPE_SECTORPOSITION(width), TYPE_SECTORPOSITION(height)));
		g_pZone = zone.get();
		player = new PlayerProbe;
		g_pPlayer = player;
		player->SetCreatureType(kSlayer);
		player->SetID(kPlayerID);
		player->At(width - 1, height - 1);
		g_pSkillAvailable = &skills;
		CHECK(screen.Init(800, 600));
		screen.FillSurface(0);
		view.reset(new MTopView);
		view->SetSurface(pScreen);
		view->SetZone(zone.get());
		// Picking: the screen's corner is the zone's, the first draw done.
		view->m_bFirstTileDraw = false;
		view->m_FirstSector.x = view->m_FirstSector.y = 0;
		view->m_PlusPoint.x = view->m_PlusPoint.y = 0;
		view->m_FirstZonePixel.x = view->m_FirstZonePixel.y = 0;
	}
	~Scene()
	{
		view.reset();
		zone->m_mapCreature.clear();
		zone->m_mapFakeCreature.clear();
		zone->m_mapGroundEffect.clear();
		zone.reset();
		g_pZone = oldZone;
		delete player;
		g_pPlayer = oldPlayer;
		MEffect::SetHost(oldEffectHost);
		CreatureState::SetHost(oldCreatureHost);
		PlayerState::SetHost(oldPlayerHost);
		ZoneState::SetHost(oldZoneHost);
		MTopView::s_LightWidth = oldLightWidth;
		MTopView::s_LightHeight = oldLightHeight;
		g_bMouseInPortal = oldInPortal;
		g_MouseSector = oldMouseSector;
		g_DrawCamGapX = oldGapX;
		g_DrawCamGapY = oldGapY;
		g_bFrameChanged = oldFrameChanged;
		g_CurrentFrame = oldFrame;
		g_pSkillAvailable = oldSkills;
		g_pWeather = oldWeather;
		g_pUserOption = oldOption;
		g_pSystemAvailableManager = oldSystems;
		g_pZoneTable = oldZoneTable;
		g_pObjectSelector = oldSelector;
		g_pEventManager = oldEvents;
		g_pRankBonusTable = oldRankBonuses;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatureTypes;
	}
	void Type(int type, CREATURETRIBE tribe, int sprite)
	{
		CREATURETABLE_INFO& row = testfw::MutableRow(creatureTypes, type);
		row.SetCreatureTribe(tribe, ACTION_MAX_VAMPIRE);
		row.SpriteTypes.Init(1);
		*row.SpriteTypes.GetMutable(0) = sprite;
	}
	MSector& Sector(int x, int y) { return *zone->SectorAt(x, y); }
	MTopView& View() { return *view; }
	void SetZoneID(int id) { zone->m_Info.ZoneID = WORD(id); }
	// The player becomes the type (its skills left alone).
	void PlayerType(int type)
	{
		MSkillSet* pSkills = g_pSkillAvailable;
		g_pSkillAvailable = NULL;
		player->SetCreatureType(TYPE_CREATURETYPE(type));
		g_pSkillAvailable = pSkills;
	}
	// An event that has the flag.
	void Event(EVENT_ID id, DWORD flag)
	{
		MEvent event;
		event.eventID = id;
		event.eventFlag = flag;
		event.eventDelay = -1;
		events.AddEvent(event);
	}
};

// The point (px, py) of sector (sx, sy) on a screen whose corner is the
// zone's.
int PX(int sx, int px = 5) { return sx * TILE_X + px; }
int PY(int sy, int py = 5) { return sy * TILE_Y + py; }

//----------------------------------------------------------------------
// Sprites the picking reads
//----------------------------------------------------------------------
const char* kSpritePath = "top_view_2_sprite.bin";

template<class Sprite> void LoadWords(Sprite& sprite, const std::vector<WORD>& words)
{
	{
		std::ofstream out(kSpritePath, std::ios::binary | std::ios::trunc);
		for (WORD word : words) { out.put(char(word & 0xFF)); out.put(char(word >> 8)); }
	}
	std::ifstream in(kSpritePath, std::ios::binary);
	CHECK(sprite.LoadFromFile(in));
	in.close();
	std::remove(kSpritePath);
}

// Three pixels wide, two high: row 0 is clear at x=0 and coloured at 1
// and 2; row 1 is coloured at 0 only.
void LoadPickIndexSprite(CIndexSprite& sprite)
{
	LoadWords(sprite, { 3, 2,
		6, 1, 1, 0, 2, 0x1234, 0x1234,
		5, 1, 0, 0, 1, 0x1234 });
}
void LoadPickSprite(CSprite& sprite)
{
	LoadWords(sprite, { 3, 2,
		5, 1, 1, 2, 0x4321, 0x4321,
		4, 1, 0, 1, 0x4321 });
}
// Whether (x, y) of the pick sprite is coloured.
bool PickPixel(int x, int y)
{
	return (y == 0 && (x == 1 || x == 2)) || (y == 1 && x == 0);
}

// Every frame ID and action of the pack, eight directions each, no frames.
void Shape(CCreatureFramePack& pack, int ids, int actions)
{
	pack.Init(TYPE_FRAMEID(ids));
	for (int id = 0; id < ids; ++id)
	{
		pack[TYPE_FRAMEID(id)].Init(BYTE(actions));
		for (int action = 0; action < actions; ++action)
			pack[TYPE_FRAMEID(id)][BYTE(action)].Init(8);
	}
}
// Frame 0 of (id, action, direction) is the sprite at its offset.
void Place(CCreatureFramePack& pack, int id, int action, int direction, int sprite, int cx, int cy)
{
	FRAME_ARRAY& frames = pack[TYPE_FRAMEID(id)][BYTE(action)][BYTE(direction)];
	frames.Init(1);
	frames[0].Set(TYPE_SPRITEID(sprite), short(cx), short(cy));
}

} // namespace

//======================================================================
// Coordinates
//======================================================================
TEST(TopView2Coordinates, AScreenPointIsTheZonePixelOfTheCornerPlusTheCameraGap)
{
	Scene scene;
	MTopView& view = scene.View();
	const int corners[][2] = { {0, 0}, {480, 240}, {-96, 31}, {1000, -7} };
	const int gaps[][2] = { {0, 0}, {3, -2}, {-17, 9} };
	const int points[][2] = { {0, 0}, {5, 7}, {799, 599}, {-4, -9} };
	for (const auto& corner : corners)
		for (const auto& gap : gaps)
			for (const auto& p : points)
			{
				view.m_FirstZonePixel.x = corner[0];
				view.m_FirstZonePixel.y = corner[1];
				g_DrawCamGapX = gap[0];
				g_DrawCamGapY = gap[1];
				const POINT pixel = view.ScreenToPixel(p[0], p[1]);
				CHECK_EQ(corner[0] + gap[0] + p[0], pixel.x);
				CHECK_EQ(corner[1] + gap[1] + p[1], pixel.y);
				// Back to the screen without the gap: the two are inverse
				// only while the camera rests (the gap is the picking's).
				const POINT back = view.PixelToScreen(pixel.x, pixel.y);
				CHECK_EQ(p[0] + gap[0], back.x);
				CHECK_EQ(p[1] + gap[1], back.y);
			}
}

TEST(TopView2Coordinates, AZonePixelOrSectorIsPlacedOnTheScreenFromTheCorner)
{
	Scene scene;
	MTopView& view = scene.View();
	g_DrawCamGapX = 11;		// never added on the way to the screen
	g_DrawCamGapY = -5;
	view.m_FirstZonePixel.x = 130;
	view.m_FirstZonePixel.y = -20;
	for (int x = -3; x <= 3; ++x)
		for (int y = -3; y <= 3; ++y)
		{
			const POINT pixel = view.PixelToScreen(x * 37, y * 19);
			CHECK_EQ(x * 37 - 130, pixel.x);
			CHECK_EQ(y * 19 + 20, pixel.y);
			const POINT sector = view.MapToScreen(x, y);
			CHECK_EQ(x * TILE_X - 130, sector.x);
			CHECK_EQ(y * TILE_Y + 20, sector.y);
		}
	CHECK_EQ(48, TILE_X);
	CHECK_EQ(24, TILE_Y);
}

TEST(TopView2Coordinates, TheSelectedSectorIsThePointsSectorInsideTheZone)
{
	Scene scene(20, 10);
	MTopView& view = scene.View();
	const int firsts[][2] = { {0, 0}, {3, 2}, {-2, -1} };
	const int pluses[][2] = { {0, 0}, {-30, 10}, {7, -23} };
	const int gaps[][2] = { {0, 0}, {-12, 5} };
	for (const auto& first : firsts)
		for (const auto& plus : pluses)
			for (const auto& gap : gaps)
			{
				view.m_FirstSector.x = first[0];
				view.m_FirstSector.y = first[1];
				view.m_PlusPoint.x = plus[0];
				view.m_PlusPoint.y = plus[1];
				g_DrawCamGapX = gap[0];
				g_DrawCamGapY = gap[1];
				for (int x = -150; x <= 1100; x += 7)
					for (int y = -80; y <= 300; y += 5)
					{
						const int px = first[0] * TILE_X + plus[0] + gap[0] + x;
						const int py = first[1] * TILE_Y + plus[1] + gap[1] + y;
						// Integer division: toward zero.
						const int sx = px / TILE_X, sy = py / TILE_Y;
						const bool in = sx >= 0 && sy >= 0 && sx < 20 && sy < 10;
						const POINT sector = view.GetSelectedSector(x, y);
						CHECK_EQ(in ? sx : SECTORPOSITION_NULL, sector.x);
						CHECK_EQ(in ? sy : SECTORPOSITION_NULL, sector.y);
					}
			}
}

TEST(TopView2Coordinates, APointJustLeftOfOrAboveTheZoneSelectsItsFirstSector)
{
	// Recorded defect: GetSelectedSector (MTopView.cpp:6294) divides
	// toward zero, so the 47 pixels left of the zone and the 23 above it
	// select column or row 0, not nothing.
	Scene scene(20, 10);
	MTopView& view = scene.View();
	for (int x = -47; x < 0; ++x)
	{
		const POINT sector = view.GetSelectedSector(x, 5);
		CHECK_EQ(0, sector.x);
		CHECK_EQ(0, sector.y);
	}
	for (int y = -23; y < 0; ++y)
	{
		const POINT sector = view.GetSelectedSector(5, y);
		CHECK_EQ(0, sector.x);
		CHECK_EQ(0, sector.y);
	}
	CHECK_EQ(SECTORPOSITION_NULL, view.GetSelectedSector(-48, 5).x);
	CHECK_EQ(SECTORPOSITION_NULL, view.GetSelectedSector(5, -24).y);
}

TEST(TopView2Coordinates, TheSelectedSectorIsBoundedByTheGlobalZoneNotTheViews)
{
	// Recorded defect (bug 30): GetSelectedSector bounds by g_pZone, the
	// rest of the view by its own zone.
	Scene scene(20, 10);
	ZoneProbe other(TYPE_SECTORPOSITION(5), TYPE_SECTORPOSITION(5));
	MTopView& view = scene.View();
	view.SetZone(&other);
	const POINT sector = view.GetSelectedSector(PX(12), PY(8));
	CHECK_EQ(12, sector.x);
	CHECK_EQ(8, sector.y);
	g_pZone = &other;
	CHECK_EQ(SECTORPOSITION_NULL, view.GetSelectedSector(PX(12), PY(8)).x);
	CHECK_EQ(4, view.GetSelectedSector(PX(4), PY(4)).x);
	g_pZone = scene.zone.get();
	view.SetZone(scene.zone.get());
}

TEST(TopView2Coordinates, TheDirectionToAPositionIsTheFacingRule)
{
	for (int ox = -2; ox <= 2; ++ox)
		for (int oy = -2; oy <= 2; ++oy)
			for (int dx = -4; dx <= 4; ++dx)
				for (int dy = -4; dy <= 4; ++dy)
					CHECK_EQ(SelectFacingDirection(ox, oy, ox + dx, oy + dy),
						MTopView::GetDirectionToPosition(ox, oy, ox + dx, oy + dy));
	CHECK_EQ(DIRECTION_RIGHT, MTopView::GetDirectionToPosition(0, 0, 5, 0));
	CHECK_EQ(DIRECTION_UP, MTopView::GetDirectionToPosition(0, 0, 0, -5));
}

//======================================================================
// Which creatures are not drawn
//======================================================================
TEST(TopView2Escape, APlainCreatureIsDrawn)
{
	Scene scene;
	Creature vampire(1, kVampire, 5, 5);
	CHECK(!IsEscapeDrawCreatureFunction(&vampire));
	CHECK(!IsEscapeDrawCreatureFunction(scene.player));
}

TEST(TopView2Escape, AGhostOrTheNoCreatureEventHidesIt)
{
	Scene scene;
	Creature vampire(1, kVampire, 5, 5);
	vampire.Set(EFFECTSTATUS_GHOST);
	CHECK(IsEscapeDrawCreatureFunction(&vampire));
	vampire.Set(EFFECTSTATUS_GHOST, false);
	CHECK(!IsEscapeDrawCreatureFunction(&vampire));
	scene.Event(EVENTID_HALLUCINATION, EVENTFLAG_NOT_DRAW_CREATURE_SHADOW);
	CHECK(!IsEscapeDrawCreatureFunction(&vampire));
	scene.Event(EVENTID_METEOR, EVENTFLAG_NOT_DRAW_CREATURE | EVENTFLAG_SHAKE_SCREEN);
	CHECK(IsEscapeDrawCreatureFunction(&vampire));
	CHECK(IsEscapeDrawCreatureFunction(scene.player));
}

TEST(TopView2Escape, OnlyAllThreeGhostFlagsHideIt)
{
	Scene scene;
	Creature vampire(1, kVampire, 5, 5);
	for (int flags = 0; flags < 16; ++flags)
	{
		vampire.SetGhost(BYTE(flags), 10);
		CHECK_EQ((flags & 7) == 7, IsEscapeDrawCreatureFunction(&vampire));
	}
	vampire.StopGhost();
	CHECK(!IsEscapeDrawCreatureFunction(&vampire));
}

TEST(TopView2Escape, AnOustersPlayerInItsGroundElementalIsHiddenFromOtherRaces)
{
	Scene scene;
	const int kOustersMonster = 5;
	scene.Type(kOustersMonster, CREATURETRIBE_OUSTERS, 3);
	scene.Sector(5, 5).SetGroundElemental();
	Creature ousters(1, kOusters, 5, 5);
	Creature monster(2, kOustersMonster, 5, 5);
	Creature elsewhere(3, kOusters, 6, 5);
	Creature vampire(4, kVampire, 5, 5);
	CHECK(IsEscapeDrawCreatureFunction(&ousters));
	CHECK(!IsEscapeDrawCreatureFunction(&monster));		// not a player's sprite
	CHECK(!IsEscapeDrawCreatureFunction(&elsewhere));		// no elemental there
	CHECK(!IsEscapeDrawCreatureFunction(&vampire));
	ousters.m_bAlive = false;
	CHECK(!IsEscapeDrawCreatureFunction(&ousters));
	ousters.m_bAlive = true;
	scene.PlayerType(kVampire);
	CHECK(IsEscapeDrawCreatureFunction(&ousters));
	scene.PlayerType(kOusters);
	CHECK(!IsEscapeDrawCreatureFunction(&ousters));
}

TEST(TopView2Escape, APetIsHiddenWhileItsOwnerIsInDarkness)
{
	Scene scene;
	Creature owner(7, kVampire, 5, 5);
	Pet pet;
	pet.SetCreatureType(kVampire);
	pet.SetOwnerID(7);
	CHECK(!IsEscapeDrawCreatureFunction(&pet));		// the owner is not in the zone
	owner.m_DarknessCount = 2;
	CHECK(!IsEscapeDrawCreatureFunction(&pet));
	scene.zone->m_mapCreature[7] = &owner;
	CHECK(IsEscapeDrawCreatureFunction(&pet));
	owner.m_DarknessCount = -1;
	CHECK(!IsEscapeDrawCreatureFunction(&pet));
	// A creature that is not a pet has no owner to ask.
	owner.m_DarknessCount = 0;
	Creature notAPet(8, kVampire, 5, 5);
	CHECK(!IsEscapeDrawCreatureFunction(&notAPet));
	CHECK(!IsEscapeDrawCreatureFunction(&owner));
	scene.zone->m_mapCreature.erase(7);
}

TEST(TopView2Escape, ABlindedPlayerSeesOnlyTheEightCellsRoundIt)
{
	Scene scene;
	scene.player->At(10, 10);
	const EFFECTSTATUS blinds[] = { EFFECTSTATUS_YELLOW_POISON_TO_CREATURE, EFFECTSTATUS_FLARE, EFFECTSTATUS_BLINDNESS };
	for (int blind = -1; blind < 3; ++blind)
	{
		if (blind >= 0) scene.player->Set(blinds[blind]);
		for (int dx = -3; dx <= 3; ++dx)
			for (int dy = -3; dy <= 3; ++dy)
			{
				Creature other(1, kVampire, 10 + dx, 10 + dy);
				const bool outside = dx < -1 || dx > 1 || dy < -1 || dy > 1;
				CHECK_EQ(blind >= 0 && outside, IsEscapeDrawCreatureFunction(&other));
			}
		// The player itself is always drawn.
		scene.player->At(3, 3);
		CHECK(!IsEscapeDrawCreatureFunction(scene.player));
		scene.player->At(10, 10);
		if (blind >= 0) scene.player->Set(blinds[blind], false);
	}
}

//======================================================================
// What the mouse is over: the screen rects the last frame drew
//======================================================================
namespace {

// The mouse at (5, 5) of sector (10, 10).
const int kMouseX = PX(10), kMouseY = PY(10);
// A rect over the mouse, and one beside it.
const int kOver[4] = { kMouseX - 2, kMouseY - 2, kMouseX + 3, kMouseY + 3 };
const int kBeside[4] = { kMouseX + 1, kMouseY - 2, kMouseX + 6, kMouseY + 3 };

enum Layer { FLYING, GROUND, UNDERGROUND };

void Stand(Scene& scene, MCreature& creature, Layer layer, int x, int y)
{
	creature.MObject::SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
	MSector& sector = scene.Sector(x, y);
	if (layer == FLYING) CHECK(sector.AddFlyingCreature(&creature));
	else if (layer == GROUND) CHECK(sector.AddGroundCreature(&creature));
	else CHECK(sector.AddUndergroundCreature(&creature));
}
void Leave(Scene& scene, MCreature& creature)
{
	CHECK(scene.Sector(creature.GetX(), creature.GetY()).RemoveCreature(creature.GetID()));
}

struct Item : MPotion
{
	Item(TYPE_OBJECTID id) { SetID(id); }
	void Rect(const int r[4]) { RECT rect = { r[0], r[1], r[2], r[3] }; SetScreenRect(&rect); }
};

MObject* Pick(Scene& scene) { return scene.View().GetSelectedObject(kMouseX, kMouseY); }

} // namespace

TEST(TopView2Select, NothingIsPickedBeforeTheFirstDraw)
{
	Scene scene;
	Creature vampire(1, kVampire, 10, 10);
	vampire.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	Stand(scene, vampire, GROUND, 10, 10);
	g_MouseSector.x = g_MouseSector.y = -7;
	g_bMouseInPortal = true;
	scene.View().m_bFirstTileDraw = true;
	CHECK(Pick(scene) == NULL);
	CHECK_EQ(-7, g_MouseSector.x);
	CHECK(g_bMouseInPortal);
	scene.View().m_bFirstTileDraw = false;
	CHECK(Pick(scene) == &vampire);
	CHECK_EQ(10, g_MouseSector.x);
	CHECK_EQ(10, g_MouseSector.y);
	CHECK(!g_bMouseInPortal);
	Leave(scene, vampire);
}

TEST(TopView2Select, TheMouseSectorIsThePointsSectorWithTheViewAndTheGap)
{
	Scene scene;
	MTopView& view = scene.View();
	view.m_FirstSector.x = 4;
	view.m_FirstSector.y = 3;
	view.m_PlusPoint.x = -20;
	view.m_PlusPoint.y = 9;
	g_DrawCamGapX = 7;
	g_DrawCamGapY = -4;
	for (int x = -300; x <= 900; x += 13)
		for (int y = -200; y <= 700; y += 11)
		{
			CHECK(view.GetSelectedObject(x, y) == NULL);
			CHECK_EQ((4 * TILE_X - 20 + 7 + x) / TILE_X, g_MouseSector.x);
			CHECK_EQ((3 * TILE_Y + 9 - 4 + y) / TILE_Y, g_MouseSector.y);
		}
}

TEST(TopView2Select, APortalUnderTheMouseIsFlaggedInsideTheZone)
{
	Scene scene;
	// No portal's zone is in the zone table, so no portal cursor is set.
	const int types[] = { MPortal::TYPE_NOMAL, MPortal::TYPE_SLAYER_ONLY, MPortal::TYPE_VAMPIRE_ONLY,
		MPortal::TYPE_MULTI_PORTAL, MPortal::TYPE_MULTI_PORTAL_HELIPORT, MPortal::TYPE_GUILD_PORTAL,
		MPortal::TYPE_BATTLE_PORTAL, MPortal::TYPE_OUSTERS_ONLY, MPortal::TYPE_CLIENT_SIEGE_POTAL_1,
		MPortal::TYPE_CLIENT_SIEGE_POTAL_4, MPortal::TYPE_CLIENT_ONLY };
	int zoneID = 70;
	for (int type : types)
	{
		g_bMouseInPortal = false;
		CHECK(scene.Sector(10, 10).AddPortal(type, zoneID++));
		CHECK(Pick(scene) == NULL);
		CHECK(g_bMouseInPortal);
		// A sector beside it is no portal.
		CHECK(scene.View().GetSelectedObject(kMouseX + TILE_X, kMouseY) == NULL);
		CHECK(!g_bMouseInPortal);
	}
	// Off the zone nothing is read, and the flag is cleared.
	g_bMouseInPortal = true;
	CHECK(scene.View().GetSelectedObject(-TILE_X, kMouseY) == NULL);
	CHECK(!g_bMouseInPortal);
	g_bMouseInPortal = true;
	CHECK(scene.View().GetSelectedObject(PX(30), kMouseY) == NULL);
	CHECK(!g_bMouseInPortal);
	g_bMouseInPortal = true;
	CHECK(scene.View().GetSelectedObject(kMouseX, PY(30)) == NULL);
	CHECK(!g_bMouseInPortal);
	CHECK_EQ(10, g_MouseSector.x);
	CHECK_EQ(30, g_MouseSector.y);
}

TEST(TopView2Select, DarknessHidesThePlayersViewAndTheCreaturesInIt)
{
	Scene scene;
	Creature monster(1, kVampire, 10, 10);
	Creature npc(2, kNPC, 10, 10);
	monster.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	npc.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	// Each layer's search has its own copy of the rule: flyers far below
	// (the first search), flyers near, ground and underground creatures.
	const struct { Layer layer; int y; } places[] = { { FLYING, 15 }, { FLYING, 10 }, { GROUND, 10 }, { UNDERGROUND, 10 } };
	for (const auto& place : places)
	for (int bits = 0; bits < 128; ++bits)
	{
		const bool vampire = bits & 1, dark = bits & 2, light = bits & 4, ghost = bits & 8,
			zone3001 = bits & 16, isNPC = bits & 32, creatureDark = bits & 64;
		scene.PlayerType(vampire ? kVampire : kSlayer);
		scene.player->m_DarknessCount = dark ? 3 : -1;
		scene.player->Set(EFFECTSTATUS_LIGHTNESS, light);
		scene.player->Set(EFFECTSTATUS_GHOST, ghost);
		scene.SetZoneID(zone3001 ? 3001 : 60);
		Creature& creature = isNPC ? npc : monster;
		creature.m_DarknessCount = creatureDark ? 0 : -1;
		Stand(scene, creature, place.layer, 10, place.y);
		g_MouseSector.x = -9;
		const bool blind = !vampire && dark && (!light || zone3001) && !ghost;
		const bool sees = (vampire && !zone3001) || (!vampire && !(!isNPC && creatureDark))
			|| (!vampire && light) || ghost;
		MObject* picked = Pick(scene);
		CHECK_EQ(!blind && sees, picked == &creature);
		CHECK_EQ(blind ? -9 : 10, g_MouseSector.x);
		CHECK(picked == NULL || picked == &creature);
		Leave(scene, creature);
	}
	scene.player->Set(EFFECTSTATUS_LIGHTNESS, false);
	scene.player->Set(EFFECTSTATUS_GHOST, false);
}

TEST(TopView2Select, EachKindIsSearchedInItsOwnWindowAroundTheMouse)
{
	Scene scene;
	Creature creature(1, kVampire, 0, 0);
	creature.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	Item item(2);
	item.Rect(kOver);
	Pickable effect;
	effect.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	// An effect is looked at only on a sector holding an object (see
	// below): one beside the mouse keeps it company.
	Item company(3);
	company.Rect(kBeside);
	for (int kind = 0; kind < 5; ++kind)
		for (int dx = -5; dx <= 5; ++dx)
			for (int dy = -6; dy <= 10; ++dy)
			{
				const int x = 10 + dx, y = 10 + dy;
				MObject* pObject = NULL;
				if (kind < 3) { Stand(scene, creature, Layer(kind), x, y); pObject = &creature; }
				else if (kind == 3) { CHECK(scene.Sector(x, y).AddItem(&item)); pObject = &item; }
				else { scene.Sector(x, y).AddEffect(&effect); CHECK(scene.Sector(x, y).AddItem(&company)); pObject = &effect; }
				const int lowest = kind == FLYING ? 8 : 4;
				const bool inWindow = dx >= -3 && dx <= 3 && dy >= -3 && dy <= lowest;
				CHECK_EQ(inWindow, Pick(scene) == pObject);
				if (kind < 3) Leave(scene, creature);
				else if (kind == 3) { MItem* pOut = NULL; CHECK(scene.Sector(x, y).RemoveItem(2, pOut)); }
				else
				{
					CHECK(scene.Sector(x, y).RemoveEffect(effect.GetID()));
					MItem* pOut = NULL;
					CHECK(scene.Sector(x, y).RemoveItem(3, pOut));
				}
			}
}

TEST(TopView2Select, TheWindowStopsAtTheZonesEdges)
{
	Scene scene(12, 12);
	Creature creature(1, kVampire, 0, 0);
	for (int mx : { 0, 1, 11 })
		for (int my : { 0, 2, 11 })
		{
			const int x = PX(mx), y = PY(my);
			creature.Rect(x - 1, y - 1, x + 1, y + 1);
			for (int cx = 0; cx < 12; ++cx)
				for (int cy = 0; cy < 12; ++cy)
					for (int layer = 0; layer < 2; ++layer)
					{
						Stand(scene, creature, Layer(layer), cx, cy);
						const int lowest = layer == FLYING ? 8 : 4;
						const bool inWindow = cx >= mx - 3 && cx <= mx + 3 && cy >= my - 3 && cy <= my + lowest;
						CHECK_EQ(inWindow, scene.View().GetSelectedObject(x, y) == &creature);
						Leave(scene, creature);
					}
		}
}

TEST(TopView2Select, LowerRowsComeFirstThenColumnsFromTheLeft)
{
	Scene scene;
	Creature a(1, kVampire, 0, 0), b(2, kVampire, 0, 0);
	a.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	b.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	// The lower row wins, whatever the column.
	Stand(scene, a, GROUND, 7, 11);
	Stand(scene, b, GROUND, 13, 12);
	CHECK(Pick(scene) == &b);
	Leave(scene, b);
	Stand(scene, b, GROUND, 8, 11);
	CHECK(Pick(scene) == &a);		// in a row, the left column wins
	Leave(scene, a);
	Leave(scene, b);
	// Two in a sector: the first to stand there.
	Stand(scene, b, GROUND, 10, 9);
	Stand(scene, a, GROUND, 10, 9);
	CHECK(Pick(scene) == &b);
	Leave(scene, b);
	CHECK(Pick(scene) == &a);
	Leave(scene, a);
}

TEST(TopView2Select, InASectorFlyingBeatsGroundBeatsEffectBeatsUndergroundBeatsItem)
{
	Scene scene;
	Creature flying(1, kVampire, 0, 0), ground(2, kVampire, 0, 0), under(3, kVampire, 0, 0);
	for (Creature* p : { &flying, &ground, &under }) p->Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	Item item(4);
	item.Rect(kOver);
	Pickable effect;
	effect.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	CHECK(scene.Sector(10, 11).AddItem(&item));
	CHECK(Pick(scene) == &item);
	Stand(scene, under, UNDERGROUND, 10, 11);
	CHECK(Pick(scene) == &under);
	scene.Sector(10, 11).AddEffect(&effect);
	CHECK(Pick(scene) == &effect);
	Stand(scene, ground, GROUND, 10, 11);
	CHECK(Pick(scene) == &ground);
	Stand(scene, flying, FLYING, 10, 11);
	CHECK(Pick(scene) == &flying);
	// Each one missing the mouse lets the next through.
	flying.Rect(kBeside[0], kBeside[1], kBeside[2], kBeside[3]);
	CHECK(Pick(scene) == &ground);
	ground.Rect(kBeside[0], kBeside[1], kBeside[2], kBeside[3]);
	CHECK(Pick(scene) == &effect);
	effect.Rect(kBeside[0], kBeside[1], kBeside[2], kBeside[3]);
	CHECK(Pick(scene) == &under);
	under.Rect(kBeside[0], kBeside[1], kBeside[2], kBeside[3]);
	CHECK(Pick(scene) == &item);
	item.Rect(kBeside);
	CHECK(Pick(scene) == NULL);
	CHECK(scene.Sector(10, 11).RemoveEffect(effect.GetID()));
	MItem* pOut = NULL;
	CHECK(scene.Sector(10, 11).RemoveItem(4, pOut));
	for (Creature* p : { &flying, &ground, &under }) Leave(scene, *p);
}

TEST(TopView2Select, AFlyingCreatureFourToEightRowsDownBeatsEverythingNearer)
{
	Scene scene;
	Creature flying(1, kVampire, 0, 0), ground(2, kVampire, 0, 0);
	flying.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	ground.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	Stand(scene, ground, GROUND, 10, 14);
	for (int dy = 0; dy <= 8; ++dy)
	{
		Stand(scene, flying, FLYING, 10, 10 + dy);
		// Rows 4 to 8 below are searched for flyers first; above that the
		// lower ground creature comes first.
		CHECK(Pick(scene) == (dy >= 4 ? static_cast<MObject*>(&flying) : &ground));
		Leave(scene, flying);
	}
	Leave(scene, ground);
}

TEST(TopView2Select, ThePlayerAVanishingCreatureAndOneTheSelectorRefusesAreSkipped)
{
	Scene scene;
	Creature other(1, kVampire, 0, 0);
	other.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	scene.player->SetID(kPlayerID);
	RECT rect = { kOver[0], kOver[1], kOver[2], kOver[3] };
	scene.player->SetScreenRect(&rect);
	// Flyers far below (the first search), then each layer near.
	const struct { Layer layer; int y; } places[] = { { FLYING, 15 }, { FLYING, 9 }, { GROUND, 9 }, { UNDERGROUND, 9 } };
	for (const auto& place : places)
	{
		const int layer = place.layer;
		Stand(scene, *scene.player, place.layer, 10, place.y + 1);
		CHECK(Pick(scene) == NULL);
		Stand(scene, other, place.layer, 10, place.y);
		CHECK(Pick(scene) == &other);
		// A creature fading out by its cut height cannot be picked, unless
		// underground.
		other.m_bCutHeight = true;
		CHECK_EQ(layer == UNDERGROUND, Pick(scene) == &other);
		other.m_bCutHeight = false;
		MObjectSelector refusing(NULL);
		g_pObjectSelector = &refusing;
		CHECK(Pick(scene) == NULL);
		g_pObjectSelector = &scene.selector;
		Leave(scene, other);
		CHECK(scene.Sector(10, place.y + 1).RemoveCreature(kPlayerID));
	}
}

TEST(TopView2Select, AnOustersInItsGroundElementalIsHiddenFromOtherRaces)
{
	Scene scene;
	scene.Sector(10, 9).SetGroundElemental();
	scene.Sector(10, 15).SetGroundElemental();
	Creature ousters(1, kOusters, 0, 0);
	ousters.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	const struct { Layer layer; int y; } places[] = { { FLYING, 15 }, { FLYING, 9 }, { GROUND, 9 }, { UNDERGROUND, 9 } };
	for (const auto& place : places)
	{
		Stand(scene, ousters, place.layer, 10, place.y);
		CHECK(Pick(scene) == NULL);
		scene.PlayerType(kOusters);
		CHECK(Pick(scene) == &ousters);
		scene.PlayerType(kSlayer);
		Leave(scene, ousters);
		Stand(scene, ousters, place.layer, 10, place.y - 1);
		CHECK(Pick(scene) == &ousters);
		Leave(scene, ousters);
	}
}

TEST(TopView2Select, AnItemIsHiddenOnlyFromAVampireInZone3001)
{
	Scene scene;
	Item item(4);
	item.Rect(kOver);
	CHECK(scene.Sector(10, 10).AddItem(&item));
	for (int bits = 0; bits < 32; ++bits)
	{
		const bool vampire = bits & 1, dark = bits & 2, light = bits & 4, ghost = bits & 8, zone3001 = bits & 16;
		scene.PlayerType(vampire ? kVampire : kSlayer);
		scene.player->m_DarknessCount = dark ? 0 : -1;
		scene.player->Set(EFFECTSTATUS_LIGHTNESS, light);
		scene.player->Set(EFFECTSTATUS_GHOST, ghost);
		scene.SetZoneID(zone3001 ? 3001 : 60);
		const bool blind = !vampire && dark && (!light || zone3001) && !ghost;
		const bool hidden = vampire && zone3001 && !ghost;
		CHECK_EQ(!blind && !hidden, Pick(scene) == &item);
	}
	MItem* pOut = NULL;
	CHECK(scene.Sector(10, 10).RemoveItem(4, pOut));
}

TEST(TopView2Select, ACorpseIsPickedByItsCreaturesRect)
{
	Scene scene;
	MCorpse corpse;
	corpse.SetID(5);
	RECT away = { kBeside[0], kBeside[1], kBeside[2], kBeside[3] };
	RECT over = { kOver[0], kOver[1], kOver[2], kOver[3] };
	corpse.SetScreenRect(&over);
	CHECK(scene.Sector(10, 10).AddItem(&corpse));
	CHECK(Pick(scene) == NULL);			// no creature in it
	Creature body(6, kVampire, 10, 10);
	body.SetScreenRect(&away);
	corpse.SetCreature(&body);
	CHECK(Pick(scene) == NULL);			// its own rect does not count
	body.SetScreenRect(&over);
	corpse.SetScreenRect(&away);
	CHECK(Pick(scene) == static_cast<MItem*>(&corpse));
	corpse.SetCreature(NULL);
	MItem* pOut = NULL;
	CHECK(scene.Sector(10, 10).RemoveItem(5, pOut));
}

TEST(TopView2Select, AnEffectAloneOnItsSectorIsNeverPicked)
{
	// Recorded defect: GetSelectedObject looks at a sector's effects
	// (MTopView.cpp:7046) only inside its IsExistObject() branch (6867),
	// which counts creatures, items and image objects but not effects, so a
	// selectable effect with nothing else on its sector cannot be picked.
	Scene scene;
	Pickable effect;
	effect.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	scene.Sector(10, 10).AddEffect(&effect);
	CHECK(scene.Sector(10, 10).IsExistEffect());
	CHECK(Pick(scene) == NULL);
	Item company(3);
	company.Rect(kBeside);
	CHECK(scene.Sector(10, 10).AddItem(&company));
	CHECK(Pick(scene) == &effect);
	MItem* pOut = NULL;
	CHECK(scene.Sector(10, 10).RemoveItem(3, pOut));
	CHECK(scene.Sector(10, 10).RemoveEffect(effect.GetID()));
}

TEST(TopView2Select, OnlyASelectableEffectIsPicked)
{
	Scene scene;
	Item company(3);
	company.Rect(kBeside);
	CHECK(scene.Sector(10, 10).AddItem(&company));
	Unpickable plain;
	plain.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	Pickable pickable;
	pickable.Rect(kBeside[0], kBeside[1], kBeside[2], kBeside[3]);
	scene.Sector(10, 10).AddEffect(&plain);
	scene.Sector(10, 10).AddEffect(&pickable);
	CHECK(Pick(scene) == NULL);
	pickable.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	CHECK(Pick(scene) == &pickable);
	CHECK(scene.Sector(10, 10).RemoveEffect(plain.GetID()));
	CHECK(scene.Sector(10, 10).RemoveEffect(pickable.GetID()));
	MItem* pOut = NULL;
	CHECK(scene.Sector(10, 10).RemoveItem(3, pOut));
}

TEST(TopView2Select, GroundEffectsAnywhereComeAfterEverySector)
{
	Scene scene;
	Pickable ground, onSector;
	Unpickable plain;
	ground.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	plain.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	onSector.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	scene.zone->m_mapGroundEffect[plain.GetID()] = &plain;
	CHECK(Pick(scene) == NULL);
	scene.zone->m_mapGroundEffect[ground.GetID()] = &ground;
	CHECK(Pick(scene) == &ground);		// the zone's, wherever it lies
	Item company(3);
	company.Rect(kBeside);
	CHECK(scene.Sector(10, 10).AddItem(&company));
	scene.Sector(10, 10).AddEffect(&onSector);
	CHECK(Pick(scene) == &onSector);
	CHECK(scene.Sector(10, 10).RemoveEffect(onSector.GetID()));
	MItem* pOut = NULL;
	CHECK(scene.Sector(10, 10).RemoveItem(3, pOut));
	ground.Rect(kBeside[0], kBeside[1], kBeside[2], kBeside[3]);
	CHECK(Pick(scene) == NULL);
	scene.zone->m_mapGroundEffect.clear();
}

TEST(TopView2Select, APetIsPickedOnlyWhenTheViewAsksForInfo)
{
	Scene scene;
	Pet pet, stray;
	pet.SetCreatureType(kVampire);
	stray.SetCreatureType(kVampire);
	pet.SetOwnerID(7);
	pet.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	stray.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	scene.zone->m_mapFakeCreature[stray.GetID()] = &stray;
	scene.zone->m_mapFakeCreature[pet.GetID()] = &pet;
	CHECK(Pick(scene) == NULL);
	scene.View().SetRequestMode(MRequestMode::REQUEST_TRADE);
	CHECK(Pick(scene) == NULL);
	scene.View().SetRequestMode(MRequestMode::REQUEST_INFO);
	CHECK(Pick(scene) == &pet);			// the one without an owner is no pet
	pet.Rect(kBeside[0], kBeside[1], kBeside[2], kBeside[3]);
	CHECK(Pick(scene) == NULL);
	// A ground effect under the mouse comes first.
	pet.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	Pickable ground;
	ground.Rect(kOver[0], kOver[1], kOver[2], kOver[3]);
	scene.zone->m_mapGroundEffect[ground.GetID()] = &ground;
	CHECK(Pick(scene) == &ground);
	scene.zone->m_mapGroundEffect.clear();
	scene.zone->m_mapFakeCreature.clear();
	scene.View().UnSetRequestMode();
}

//======================================================================
// What the mouse is over: the sprites' coloured pixels
//======================================================================
namespace {

// The view's packs for the pixel picking: body frame set 1's standing
// frame facing down, the burrow sprite, and item tile frame 0, every one
// the pick sprite.
struct SpriteScene : Scene
{
	SpriteScene()
	{
		MTopView& view = View();
		Shape(view.m_CreatureFPK, 2, ACTION_MAX_VAMPIRE);
		Place(view.m_CreatureFPK, 1, ACTION_STAND, 2, 0, 0, 0);
		view.m_CreatureSPK.Init(1);
		LoadPickIndexSprite(view.m_CreatureSPK[0]);
		view.m_EtcSPK.Init(1);
		LoadPickSprite(view.m_EtcSPK[SPRITEID_CREATURE_BURROW]);
		view.m_ItemTileFPK.Init(1);
		view.m_ItemTileFPK[0].Set(0, 0, 0);
		view.m_ItemTileISPK.Init(1);
		LoadPickIndexSprite(view.m_ItemTileISPK[0]);
		g_pItemTable->InitClass(ITEM_CLASS_POTION, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_POTION, 0).TileFrameID = 0;
	}
	void BodyAt(int cx, int cy) { Place(View().m_CreatureFPK, 1, ACTION_STAND, 2, 0, cx, cy); }
	MObject* PickAt(int x, int y) { return View().GetSelectedObjectSprite(x, y); }
};

} // namespace

TEST(TopView2SelectSprite, ACreatureIsPickedOnlyOnAColouredPixelOfItsFrame)
{
	SpriteScene scene;
	Creature creature(1, kVampire, 0, 0);
	scene.BodyAt(5, 3);
	Stand(scene, creature, GROUND, 10, 10);
	for (int x = PX(10, 0); x <= PX(10, 12); ++x)
		for (int y = PY(10, -6); y <= PY(10, 8); ++y)
			CHECK_EQ(PickPixel(x - PX(10, 5), y - PY(10, 3)), scene.PickAt(x, y) == &creature);
	// The sector offset moves it, the height does not (on the ground).
	creature.m_sX = -7;
	creature.m_sY = 4;
	creature.m_Z = 9;
	for (int x = PX(10, -5); x <= PX(10, 8); ++x)
		for (int y = PY(10, 0); y <= PY(10, 12); ++y)
			CHECK_EQ(PickPixel(x - PX(10, -2), y - PY(10, 7)), scene.PickAt(x, y) == &creature);
	Leave(scene, creature);
}

TEST(TopView2SelectSprite, AFlyingCreatureIsLiftedByItsHeight)
{
	SpriteScene scene;
	Creature bat(1, kVampire, 0, 0);
	bat.m_Z = 30;
	scene.BodyAt(2, 1);
	Stand(scene, bat, FLYING, 10, 12);
	for (int x = PX(10, 0); x <= PX(10, 8); ++x)
		for (int y = PY(12, -40); y <= PY(12, 6); ++y)
			CHECK_EQ(PickPixel(x - PX(10, 2), y - PY(12, 1 - 30)), scene.PickAt(x, y) == &bat);
	Leave(scene, bat);
	// Five rows below the mouse: the first search, lifted the same.
	scene.BodyAt(2, -85);
	Stand(scene, bat, FLYING, 10, 15);
	const int oy = 15 * TILE_Y - 85 - 30;		// row 10 of the screen
	for (int x = PX(10, 0); x <= PX(10, 8); ++x)
		for (int y = oy - 5; y <= oy + 6; ++y)
			CHECK_EQ(PickPixel(x - PX(10, 2), y - oy), scene.PickAt(x, y) == &bat);
	Leave(scene, bat);
}

TEST(TopView2SelectSprite, TheBurrowAndTheItemTileArePickedByTheirSprites)
{
	SpriteScene scene;
	Creature mole(1, kVampire, 0, 0);
	mole.m_sX = 3;
	mole.m_sY = 2;
	scene.BodyAt(20, 20);		// an underground creature shows no body
	Stand(scene, mole, UNDERGROUND, 10, 10);
	for (int x = PX(10, 0); x <= PX(10, 8); ++x)
		for (int y = PY(10, 0); y <= PY(10, 6); ++y)
			CHECK_EQ(PickPixel(x - PX(10, 3), y - PY(10, 2)), scene.PickAt(x, y) == &mole);
	Leave(scene, mole);
	Item item(2);
	scene.View().m_ItemTileFPK[0].Set(0, 6, 4);
	item.SetPosition(10, 10);		// read from the item, not its sector
	CHECK(scene.Sector(10, 10).AddItem(&item));
	for (int x = PX(10, 0); x <= PX(10, 12); ++x)
		for (int y = PY(10, 0); y <= PY(10, 8); ++y)
			CHECK_EQ(PickPixel(x - PX(10, 6), y - PY(10, 4)), scene.PickAt(x, y) == &item);
	MItem* pOut = NULL;
	CHECK(scene.Sector(10, 10).RemoveItem(2, pOut));
}

TEST(TopView2SelectSprite, EachKindIsSearchedInItsOwnWindowAroundTheMouse)
{
	SpriteScene scene;
	Creature creature(1, kVampire, 0, 0);
	Item item(2);
	for (int kind = 0; kind < 4; ++kind)
		for (int dx = -3; dx <= 3; ++dx)
			for (int dy = -3; dy <= 7; ++dy)
			{
				const int x = 10 + dx, y = 10 + dy;
				// Pixel (1, 0) of the sprite under the mouse.
				const int sx = 4 - dx * TILE_X, sy = 5 - dy * TILE_Y;
				MObject* pObject = &creature;
				if (kind < 3)
				{
					creature.m_sX = sx;
					creature.m_sY = sy;
					Stand(scene, creature, Layer(kind), x, y);
				}
				else
				{
					scene.View().m_ItemTileFPK[0].Set(0, short(sx), short(sy));
					item.SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
					CHECK(scene.Sector(x, y).AddItem(&item));
					pObject = &item;
				}
				const int lowest = kind == FLYING ? 5 : 3;
				const bool inWindow = dx >= -1 && dx <= 1 && dy >= -1 && dy <= lowest;
				CHECK_EQ(inWindow, scene.PickAt(kMouseX, kMouseY) == pObject);
				if (kind < 3) Leave(scene, creature);
				else { MItem* pOut = NULL; CHECK(scene.Sector(x, y).RemoveItem(2, pOut)); }
			}
}

TEST(TopView2SelectSprite, FlyersTwoRowsDownComeFirstThenRowsUpwardsKindByKind)
{
	SpriteScene scene;
	Creature flying(1, kVampire, 0, 0), ground(2, kVampire, 0, 0), under(3, kVampire, 0, 0);
	Item item(4);
	// Each one's sprite under the mouse from wherever it stands.
	auto Over = [](Creature& c, int x, int y) { c.m_sX = kMouseX - 1 - x * TILE_X; c.m_sY = kMouseY - y * TILE_Y; };
	Over(under, 10, 11);
	Over(ground, 10, 11);
	scene.View().m_ItemTileFPK[0].Set(0, short(kMouseX - 1 - 10 * TILE_X), short(kMouseY - 11 * TILE_Y));
	item.SetPosition(10, 11);
	CHECK(scene.Sector(10, 11).AddItem(&item));
	CHECK(scene.PickAt(kMouseX, kMouseY) == &item);
	Stand(scene, under, UNDERGROUND, 10, 11);
	CHECK(scene.PickAt(kMouseX, kMouseY) == &under);
	Stand(scene, ground, GROUND, 10, 11);
	CHECK(scene.PickAt(kMouseX, kMouseY) == &ground);
	Over(flying, 10, 11);
	Stand(scene, flying, FLYING, 10, 11);
	CHECK(scene.PickAt(kMouseX, kMouseY) == &flying);
	Leave(scene, flying);
	// A lower row first; in a row, the left column.
	Over(flying, 9, 12);
	Stand(scene, flying, GROUND, 9, 12);
	CHECK(scene.PickAt(kMouseX, kMouseY) == &flying);
	Leave(scene, flying);
	Over(flying, 9, 11);
	Stand(scene, flying, UNDERGROUND, 9, 11);
	CHECK(scene.PickAt(kMouseX, kMouseY) == &flying);
	Leave(scene, flying);
	// A flyer four or five rows down beats them all.
	Over(flying, 11, 15);
	Stand(scene, flying, FLYING, 11, 15);
	CHECK(scene.PickAt(kMouseX, kMouseY) == &flying);
	Leave(scene, flying);
	Leave(scene, ground);
	Leave(scene, under);
	MItem* pOut = NULL;
	CHECK(scene.Sector(10, 11).RemoveItem(4, pOut));
}

TEST(TopView2SelectSprite, ThePlayerIsSkippedAndNeitherDarknessNorTheFirstDrawMatter)
{
	SpriteScene scene;
	scene.PlayerType(kVampire);
	scene.player->m_sX = kMouseX - 1 - 10 * TILE_X;
	scene.player->m_sY = kMouseY - 10 * TILE_Y;
	Stand(scene, *scene.player, GROUND, 10, 10);
	CHECK(scene.PickAt(kMouseX, kMouseY) == NULL);
	CHECK(scene.Sector(10, 10).RemoveCreature(kPlayerID));
	Creature other(1, kVampire, 0, 0);
	other.m_sX = scene.player->m_sX;
	other.m_sY = scene.player->m_sY;
	other.m_DarknessCount = 0;
	Stand(scene, other, GROUND, 10, 10);
	scene.PlayerType(kSlayer);
	scene.player->m_DarknessCount = 0;
	scene.View().m_bFirstTileDraw = true;
	MObjectSelector refusing(NULL);
	g_pObjectSelector = &refusing;
	g_MouseSector.x = -3;
	CHECK(scene.PickAt(kMouseX, kMouseY) == &other);
	CHECK_EQ(-3, g_MouseSector.x);		// not this one's to set
	g_pObjectSelector = &scene.selector;
	Leave(scene, other);
}

TEST(TopView2SelectSprite, TheViewAndTheCameraGapShiftWhereTheMouseLooks)
{
	SpriteScene scene;
	Creature creature(1, kVampire, 0, 0);
	scene.BodyAt(5, 3);
	Stand(scene, creature, GROUND, 10, 10);
	MTopView& view = scene.View();
	view.m_FirstSector.x = 6;
	view.m_FirstSector.y = 4;
	view.m_PlusPoint.x = -11;	// cancels out
	view.m_PlusPoint.y = 13;
	for (int gap : { 0, 9, -14 })
	{
		g_DrawCamGapX = gap;
		g_DrawCamGapY = -gap / 2;
		// The creature's frame on the screen of that view.
		const int ox = PX(10, 5) - 6 * TILE_X - gap, oy = PY(10, 3) - 4 * TILE_Y + gap / 2;
		for (int x = ox - 4; x <= ox + 6; ++x)
			for (int y = oy - 3; y <= oy + 4; ++y)
				CHECK_EQ(PickPixel(x - ox, y - oy), view.GetSelectedObjectSprite(x, y) == &creature);
	}
	Leave(scene, creature);
}

//======================================================================
// The image objects in sight
//======================================================================
namespace {

typedef std::map<QWORD, MImageObject*> ObjectMap;

QWORD GetOutputID(const MImageObject* o) { return (QWORD(o->GetViewpoint()) << 32) | o->GetID(); }

// A 24x20 zone seen through a view 6 sectors wide and 5 high, the player
// 3 from its left and 2 from its top; image objects the test places on
// the cells it names. The window of first sector F is columns F.x-1 to
// F.x+7 and rows F.y-1 to F.y+6, inside the zone.
struct ObjectScene : Scene
{
	static constexpr int W = 24, H = 20, SW = 6, SH = 5;
	LONG oldWidth = g_SECTOR_WIDTH, oldHeight = g_SECTOR_HEIGHT;
	LONG oldLeft = g_SECTOR_SKIP_PLAYER_LEFT, oldUp = g_SECTOR_SKIP_PLAYER_UP;
	std::vector<std::unique_ptr<MImageObject>> objects;
	std::vector<std::vector<POINT>> cells;

	ObjectScene() : Scene(W, H)
	{
		g_SECTOR_WIDTH = SW;
		g_SECTOR_HEIGHT = SH;
		g_SECTOR_SKIP_PLAYER_LEFT = -3;
		g_SECTOR_SKIP_PLAYER_UP = -2;
	}
	~ObjectScene()
	{
		g_SECTOR_SKIP_PLAYER_UP = oldUp;
		g_SECTOR_SKIP_PLAYER_LEFT = oldLeft;
		g_SECTOR_HEIGHT = oldHeight;
		g_SECTOR_WIDTH = oldWidth;
	}
	MImageObject* Add(TYPE_OBJECTID id, int viewpoint, const std::vector<POINT>& at)
	{
		objects.emplace_back(new MImageObject(id, id, 0, 0, 0, TYPE_SECTORPOSITION(viewpoint), 0));
		cells.push_back(at);
		for (const POINT& p : at) CHECK(Sector(p.x, p.y).AddImageObject(objects.back().get()));
		return objects.back().get();
	}
	// The objects with a cell in the window of first sector f.
	ObjectMap Expected(POINT f) const
	{
		ObjectMap expected;
		for (size_t i = 0; i < objects.size(); ++i)
			for (const POINT& p : cells[i])
				if (p.x >= f.x - 1 && p.x <= f.x + SW + 1 && p.y >= f.y - 1 && p.y <= f.y + SH + 1)
				{
					const MImageObject* o = objects[i].get();
					expected[(QWORD(o->GetViewpoint()) << 32) | o->GetID()] = objects[i].get();
				}
		return expected;
	}
	// The full search, from where the player stands.
	void Determine(int playerX, int playerY)
	{
		player->At(playerX, playerY);
		Access::Determine(View());
	}
	// The full search of first sector f (in range), and the view there.
	void Window(POINT f)
	{
		Determine(f.x + 3, f.y + 2);
		View().m_FirstSector = f;
	}
	bool Same(const ObjectMap& expected)
	{
		const ObjectMap& actual = View().m_mapImageObject;
		if (actual == expected) return true;
		std::printf("      %d objects, not %d\n", int(actual.size()), int(expected.size()));
		return false;
	}
	// Single-cell objects on most cells, some cells with two.
	void Scatter(unsigned seed)
	{
		std::mt19937 rng(seed);
		TYPE_OBJECTID id = 1;
		for (int y = 0; y < H; ++y)
			for (int x = 0; x < W; ++x)
			{
				const int n = int(rng() % 4);	// 0 to 3, one in four empty
				for (int i = 0; i < (n == 3 ? 2 : n); ++i)
					Add(id++, int(rng() % (H + 3)), { { x, y } });
			}
	}
};

} // namespace

TEST(TopView2ImageObjects, TheFullSearchTakesTheWindowRoundThePlayerKeyedByViewpointAndID)
{
	ObjectScene scene;
	scene.Scatter(7);
	// One object over a whole row, and two with one ID but other viewpoints.
	std::vector<POINT> row;
	for (int x = 0; x < ObjectScene::W; ++x) row.push_back({ x, 3 });
	scene.Add(5000, 9, row);
	scene.Add(5001, 2, { { 0, 0 } });
	scene.Add(5001, 3, { { 23, 19 } });
	for (int px = 0; px < ObjectScene::W; ++px)
		for (int py = 0; py < ObjectScene::H; ++py)
		{
			// A stale entry goes.
			scene.View().m_mapImageObject[QWORD(77) << 32] = NULL;
			scene.Determine(px, py);
			int fx = px - 3, fy = py - 2;
			if (fx <= 0) fx = 0;
			else if (fx + 6 + 1 >= ObjectScene::W) fx = ObjectScene::W - 6 - 1;
			if (fy <= 0) fy = 0;
			else if (fy + 5 + 1 >= ObjectScene::H) fy = ObjectScene::H - 5 - 1;
			CHECK(scene.Same(scene.Expected({ fx, fy })));
		}
	// Keys sort by viewpoint, then ID.
	scene.Determine(0, 0);
	QWORD last = 0;
	for (const auto& entry : scene.View().m_mapImageObject)
	{
		CHECK_EQ(scene.View().GetOutputImageObjectID(entry.second), entry.first);
		CHECK(entry.first >= last);
		last = entry.first;
	}
	CHECK(scene.View().m_mapImageObject.count((QWORD(2) << 32) | 5001) == 1);
	CHECK(scene.View().m_mapImageObject.count((QWORD(3) << 32) | 5001) == 0);
	CHECK(scene.View().m_mapImageObject.count((QWORD(9) << 32) | 5000) == 1);
}

TEST(TopView2ImageObjects, AnUpdateToTheSameFirstSectorChangesNothing)
{
	ObjectScene scene;
	scene.Scatter(3);
	scene.Window({ 5, 5 });
	scene.View().m_mapImageObject[QWORD(77) << 32] = NULL;
	ObjectMap before = scene.View().m_mapImageObject;
	Access::Update(scene.View(), { 5, 5 });
	CHECK(scene.Same(before));
}

TEST(TopView2ImageObjects, AnUpdateOfUpToThreeSectorsEqualsTheFullSearch)
{
	// Single-cell objects and rectangles: what a step of the view's
	// first sector brings and takes is exactly the new window's.
	ObjectScene scene;
	scene.Scatter(11);
	TYPE_OBJECTID id = 9000;
	std::mt19937 rng(5);
	for (int i = 0; i < 25; ++i)
	{
		const int x = int(rng() % 21), y = int(rng() % 17);
		const int w = 1 + int(rng() % 3), h = 1 + int(rng() % 3);
		std::vector<POINT> rect;
		for (int dx = 0; dx < w; ++dx)
			for (int dy = 0; dy < h; ++dy) rect.push_back({ x + dx, y + dy });
		scene.Add(id++, int(rng() % 20), rect);
	}
	const int maxX = ObjectScene::W - ObjectScene::SW - 1, maxY = ObjectScene::H - ObjectScene::SH - 1;
	int moves = 0;
	for (int ax = 0; ax <= maxX; ++ax)
		for (int ay = 0; ay <= maxY; ++ay)
			for (int dx = -3; dx <= 3; ++dx)
				for (int dy = -3; dy <= 3; ++dy)
				{
					const POINT a = { ax, ay }, b = { ax + dx, ay + dy };
					if (b.x < 0 || b.y < 0 || b.x > maxX || b.y > maxY) continue;
					scene.Window(a);
					Access::Update(scene.View(), b);
					CHECK(scene.Same(scene.Expected(b)));
					++moves;
				}
	CHECK(moves > 1500);
}

TEST(TopView2ImageObjects, AWalkOfManyStepsKeepsTheWindowTheFullSearchWould)
{
	ObjectScene scene;
	scene.Scatter(23);
	std::mt19937 rng(17);
	POINT at = { 8, 6 };
	scene.Window(at);
	const int maxX = ObjectScene::W - ObjectScene::SW - 1, maxY = ObjectScene::H - ObjectScene::SH - 1;
	for (int step = 0; step < 400; ++step)
	{
		POINT next = { at.x + int(rng() % 3) - 1, at.y + int(rng() % 3) - 1 };
		next.x = (std::max)(0, (std::min)(maxX, int(next.x)));
		next.y = (std::max)(0, (std::min)(maxY, int(next.y)));
		Access::Update(scene.View(), next);
		scene.View().m_FirstSector = next;	// as DrawZone does after it
		CHECK(scene.Same(scene.Expected(next)));
		at = next;
	}
}

TEST(TopView2ImageObjects, AJumpOfMoreThanTheWindowKeepsTheRowsBetween)
{
	// Recorded defect: UpdateImageObject (MTopView.cpp:10410-10411 and
	// 10459-10460; the columns' 10687-10688 and 10735-10736) adds every row
	// from the new window's edge to the old one's, so a jump of more than the window's height (SH+3 rows) keeps
	// the objects of the rows between the two windows. The view jumps
	// that far only through a first draw, which searches afresh.
	ObjectScene scene;
	MImageObject* between = scene.Add(1, 0, { { 5, 9 } });
	MImageObject* above = scene.Add(2, 0, { { 5, 2 } });
	MImageObject* below = scene.Add(3, 0, { { 5, 18 } });
	scene.Window({ 3, 13 });		// rows 12 to 19
	CHECK(scene.Same(scene.Expected({ 3, 13 })));
	Access::Update(scene.View(), { 3, 0 });		// rows 0 to 6
	ObjectMap expected = scene.Expected({ 3, 0 });
	CHECK(expected.count(GetOutputID(above)) == 1);
	CHECK(expected.count(GetOutputID(between)) == 0);
	expected[GetOutputID(between)] = between;
	CHECK(scene.Same(expected));
	// And downwards.
	scene.Window({ 3, 0 });
	Access::Update(scene.View(), { 3, 13 });
	expected = scene.Expected({ 3, 13 });
	CHECK(expected.count(GetOutputID(below)) == 1);
	expected[GetOutputID(between)] = between;
	CHECK(scene.Same(expected));
}

TEST(TopView2ImageObjects, AnObjectOnCellsApartIsLostWhenTheCellsAcrossTheEdgeLeave)
{
	// Recorded defect: UpdateImageObject erases an object by its key when
	// any of its cells leaves the window (MTopView.cpp:10522), and gives it back only from the
	// one row (or column) at the new edge. An object whose cells are not
	// next to each other - one leaving, one staying away from the edge -
	// drops out until the next full search.
	ObjectScene scene;
	MImageObject* split = scene.Add(1, 0, { { 5, 4 }, { 5, 7 } });
	MImageObject* tall = scene.Add(2, 0, { { 8, 4 }, { 8, 5 }, { 8, 6 }, { 8, 7 } });
	scene.Window({ 3, 5 });		// rows 4 to 11
	CHECK(scene.Same(scene.Expected({ 3, 5 })));
	Access::Update(scene.View(), { 3, 6 });		// rows 5 to 12: row 4 leaves
	ObjectMap expected = scene.Expected({ 3, 6 });
	CHECK(expected.count(GetOutputID(split)) == 1);
	CHECK(expected.count(GetOutputID(tall)) == 1);
	expected.erase(GetOutputID(split));		// the contiguous one survives
	CHECK(scene.Same(expected));
}

//======================================================================
// The light buffer
//======================================================================
namespace {

// Every byte of the buffer is the value.
bool Filled(const CFilter& filter, int value)
{
	for (int y = 0; y < filter.GetHeight(); ++y)
		for (int x = 0; x < filter.GetWidth(); ++x)
			if (filter.GetFilter(WORD(y))[x] != value) return false;
	return true;
}
void Fill(CFilter& filter, int value)
{
	for (int y = 0; y < filter.GetHeight(); ++y)
		for (int x = 0; x < filter.GetWidth(); ++x) filter.SetFilter(WORD(x), WORD(y), BYTE(value));
}

// Lights of ranges 0 to 2, filter i (2i+1) square, cell (u, v) 1+u+v.
struct LightScene : Scene
{
	LightScene()
	{
		MTopView& view = View();
		view.m_LightBufferFilter.Init(SCREENLIGHT_WIDTH, SCREENLIGHT_HEIGHT);
		Fill(view.m_LightBufferFilter, 10);
		view.m_LightFTP.Init(3);
		for (int i = 0; i < 3; ++i)
		{
			view.m_LightFTP[TYPE_FILTERID(i)].Init(WORD(2 * i + 1), WORD(2 * i + 1));
			for (int u = 0; u <= 2 * i; ++u)
				for (int v = 0; v <= 2 * i; ++v) view.m_LightFTP[TYPE_FILTERID(i)].SetFilter(WORD(u), WORD(v), BYTE(1 + u + v));
		}
		MTopView::s_LightWidth = 10.0f;
		MTopView::s_LightHeight = 8.0f;
	}
	// The buffer is 10 everywhere but filter `id` added (or taken) at
	// (fx, fy).
	bool Lit(int id, int fx, int fy, bool add)
	{
		const CFilter& buffer = View().m_LightBufferFilter;
		const int size = 2 * id + 1;
		for (int y = 0; y < SCREENLIGHT_HEIGHT; ++y)
			for (int x = 0; x < SCREENLIGHT_WIDTH; ++x)
			{
				int expected = 10;
				if (id >= 0 && x >= fx && x < fx + size && y >= fy && y < fy + size)
				{
					const int f = 1 + (x - fx) + (y - fy);
					expected = add ? (std::min)(31, 10 + f) : (std::max)(0, 10 - f);
				}
				if (buffer.GetFilter(WORD(y))[x] != expected) return false;
			}
		return true;
	}
};

} // namespace

TEST(TopView2Light, TheDarkBitsAreKeptBelowSixteen)
{
	Scene scene;
	MTopView& view = scene.View();
	CHECK_EQ(0, view.GetDarkBits());
	for (int bits = 0; bits < 40; ++bits)
	{
		view.SetDarkBits(5);
		view.SetDarkBits(BYTE(bits));
		CHECK_EQ(bits < 16 ? bits : 5, view.GetDarkBits());
	}
}

TEST(TopView2Light, Clearing2DFillsTheBufferWithTheLightTheDarknessLeaves)
{
	LightScene scene;
	MTopView& view = scene.View();
	for (int bits = 0; bits < 16; ++bits)
		for (int flags = 0; flags < 8; ++flags)
			for (int sightGap : { -2, 0, 1, 5, 20 })
			{
				const bool dark = flags & 1, light = flags & 2, zone3001 = flags & 4;
				view.m_DarkBits = BYTE(bits);
				scene.player->m_DarknessCount = dark ? 1 : -1;
				scene.player->Set(EFFECTSTATUS_LIGHTNESS, light);
				scene.SetZoneID(zone3001 ? 3001 : 60);
				scene.player->m_TimeLightSight = 3;
				scene.player->m_LightSight = 3 + sightGap;
				view.m_nLight = 9;
				view.ClearLightBufferFilter2D();
				const int darkBits = dark && (!light || zone3001) ? 15 : bits;
				const int expected = (std::max)(0, 31 - 2 * darkBits - 2 * sightGap);
				CHECK(Filled(view.m_LightBufferFilter, expected));
				CHECK_EQ(0, view.m_nLight);
			}
}

TEST(TopView2Light, Clearing2DWithNoBufferOnlyResetsTheCount)
{
	Scene scene;
	MTopView& view = scene.View();
	view.m_nLight = 4;
	view.ClearLightBufferFilter2D();
	CHECK_EQ(0, view.m_nLight);
	CHECK(view.m_LightBufferFilter.IsNotInit());
}

TEST(TopView2Light, Clearing3DFillsTheBufferWithTheDarknessTheLightLeaves)
{
	// Recorded defect (bug 18): ClearLightBufferFilter3D (MTopView.cpp:8476)
	// has no IsNotInit guard and writes through the rows of a buffer never
	// made; only made buffers are cleared here. The 3D arm is dead
	// (s_bUse3DLight is false).
	LightScene scene;
	MTopView& view = scene.View();
	for (int bits = 0; bits < 16; ++bits)
		for (int flags = 0; flags < 8; ++flags)
			for (int sightGap : { -2, 0, 1, 5, 20 })
			{
				const bool dark = flags & 1, light = flags & 2, zone3001 = flags & 4;
				view.m_DarkBits = BYTE(bits);
				scene.player->m_DarknessCount = dark ? 1 : -1;
				scene.player->Set(EFFECTSTATUS_LIGHTNESS, light);
				scene.SetZoneID(zone3001 ? 3001 : 60);
				scene.player->m_TimeLightSight = 3;
				scene.player->m_LightSight = 3 + sightGap;
				view.m_nLight = 9;
				view.ClearLightBufferFilter3D();
				const int darkBits = dark && (!light || zone3001) ? 15 : bits;
				CHECK(Filled(view.m_LightBufferFilter, (std::max)(0, darkBits - sightGap)));
				CHECK_EQ(0, view.m_nLight);
			}
}

TEST(TopView2Light, ALightIsAddedOnlyInDarknessUpToTheCapUnlessForced)
{
	LightScene scene;
	MTopView& view = scene.View();
	const int cap = g_pClientConfig->MAX_LIGHT_DRAW;
	for (int threeD = 0; threeD < 2; ++threeD)
		for (int dark = 0; dark < 2; ++dark)
			for (int bits : { 0, 3 })
				for (int count : { 0, cap - 1, cap })
					for (int force = 0; force < 2; ++force)
						for (int sight : { 0, 3, 4, 5, 9 })
							for (int range : { 0, 1, 2, 5 })
							{
								Fill(view.m_LightBufferFilter, 10);
								scene.player->m_DarknessCount = dark ? 2 : -1;
								scene.player->m_Sight = char(sight);
								view.m_DarkBits = BYTE(bits);
								view.m_nLight = count;
								if (threeD) view.AddLightFilter3D(200, 160, BYTE(range), false, force != 0);
								else view.AddLightFilter2D(200, 160, BYTE(range), false, force != 0);
								const int used = force ? range : (std::min)((std::max)(sight - 3, 0), range);
								const bool lit = (!dark && used != 0 && (bits != 0 || dark) && count < cap) || force;
								const int id = (std::min)(used, 2);
								CHECK(scene.Lit(lit ? id : -1, 20 - id, 20 - id, !threeD));
								CHECK_EQ(count + (lit ? 1 : 0), view.m_nLight);
							}
}

TEST(TopView2Light, ALightIsCentredOnItsScreenOrZonePoint)
{
	LightScene scene;
	MTopView& view = scene.View();
	view.m_FirstZonePixel.x = 100;
	view.m_FirstZonePixel.y = 40;
	g_DrawCamGapX = 30;		// not the drawing's
	// Screen (209, 167) is filter cell (20, 20), truncated.
	view.AddLightFilter2D(209, 167, 2, false, true);
	CHECK(scene.Lit(2, 18, 18, true));
	Fill(view.m_LightBufferFilter, 10);
	view.AddLightFilter2D(309, 207, 2, true, true);
	CHECK(scene.Lit(2, 18, 18, true));
	Fill(view.m_LightBufferFilter, 10);
	view.AddLightFilter3D(309, 207, 1, true, true);
	CHECK(scene.Lit(1, 19, 19, false));
	Fill(view.m_LightBufferFilter, 10);
	// -1.5 truncates to -1, 0.375 to 0: the 5x5 light from (-3, -2).
	view.AddLightFilter2D(-15, 3, 2, false, true);
	CHECK(scene.Lit(2, -3, -2, true));
	CHECK_EQ(4, view.m_nLight);
}

TEST(TopView2Light, NoLightIsAddedWithoutLightFilters)
{
	Scene scene;
	MTopView& view = scene.View();
	view.m_LightBufferFilter.Init(SCREENLIGHT_WIDTH, SCREENLIGHT_HEIGHT);
	Fill(view.m_LightBufferFilter, 10);
	view.m_nLight = 3;
	view.AddLightFilter2D(200, 160, 2, false, true);
	view.AddLightFilter3D(200, 160, 2, false, true);
	CHECK(Filled(view.m_LightBufferFilter, 10));
	CHECK_EQ(3, view.m_nLight);
}

namespace {

std::vector<WORD> Picture(CSpriteSurface& surface, int width, int height)
{
	std::vector<WORD> pixels;
	DWORD pitch = 0;
	const unsigned char* base = static_cast<const unsigned char*>(surface.Lock(nullptr, &pitch));
	CHECK(base != nullptr);
	if (base == nullptr) return pixels;
	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width; ++x) pixels.push_back(reinterpret_cast<const WORD*>(base + y * pitch)[x]);
	surface.Unlock();
	return pixels;
}
WORD PixelAt(CSpriteSurface& surface, int x, int y)
{
	DWORD pitch = 0;
	const unsigned char* base = static_cast<const unsigned char*>(surface.Lock(nullptr, &pitch));
	const WORD pixel = base != nullptr ? reinterpret_cast<const WORD*>(base + y * pitch)[x] : 0;
	surface.Unlock();
	return pixel;
}
void Paint(CSpriteSurface& surface, int width, int height)
{
	DWORD pitch = 0;
	unsigned char* base = static_cast<unsigned char*>(surface.Lock(nullptr, &pitch));
	if (base != nullptr)
		for (int y = 0; y < height; ++y)
			for (int x = 0; x < width; ++x) reinterpret_cast<WORD*>(base + y * pitch)[x] = WORD(0x8410 | ((x * 7 + y * 13) & 0x7BEF));
	surface.Unlock();
}

} // namespace

TEST(TopView2Light, TheBufferDarkensTheScreenOnlyWhenItIsDark)
{
	LightScene scene;
	MTopView& view = scene.View();
	for (int y = 0; y < SCREENLIGHT_HEIGHT; ++y)
		for (int x = 0; x < SCREENLIGHT_WIDTH; ++x) view.m_LightBufferFilter.SetFilter(WORD(x), WORD(y), BYTE((x + y) % 32));
	view.m_p2DLightPixelWidth = new int[SCREENLIGHT_WIDTH];
	view.m_p2DLightPixelHeight = new int[SCREENLIGHT_HEIGHT];
	for (int i = 0; i < SCREENLIGHT_WIDTH; ++i) view.m_p2DLightPixelWidth[i] = 13;
	for (int i = 0; i < SCREENLIGHT_HEIGHT; ++i) view.m_p2DLightPixelHeight[i] = 10;
	CSpriteSurface reference;
	CHECK(reference.Init(800, 600));
	Paint(reference, 800, 600);
	const std::vector<WORD> plain = Picture(reference, 800, 600);
	reference.ApplyLightGrid(view.m_LightBufferFilter, view.m_p2DLightPixelWidth, view.m_p2DLightPixelHeight);
	const std::vector<WORD> lit = Picture(reference, 800, 600);
	CHECK(lit != plain);
	for (int bits = 0; bits < 2; ++bits)
		for (int dark = 0; dark < 2; ++dark)
		{
			Paint(scene.screen, 800, 600);
			view.m_DarkBits = BYTE(bits * 2);
			scene.player->m_DarknessCount = dark ? 0 : -1;
			view.DrawLightBuffer2D();
			CHECK(Picture(scene.screen, 800, 600) == (bits || dark ? lit : plain));
			Paint(scene.screen, 800, 600);
			view.DrawLightBuffer3D();		// the same
			CHECK(Picture(scene.screen, 800, 600) == (bits || dark ? lit : plain));
		}
}

//======================================================================
// The tiles and the frame
//======================================================================
namespace {

// A 60x80 zone whose every sector has a tile sprite of its own, a single
// pixel (the tile cell's corner) of a colour of its own, in two
// generations: a test switches the zone to generation 1 to see which
// cells a frame redraws. The view draws them through a real tile
// renderer onto a tile surface first filled with white.
//
// The player is a vampire-bodied creature of one frame - the pick
// sprite, 5 right of and 20 above where it stands - standing in the
// zone's creature map, so the frame draws it.
struct TileScene : Scene
{
	static constexpr int W = 60, H = 80;
	static constexpr WORD kWhite = 0xFFFF;
	int savedSets[4];
	bool quiet = false;		// a test expecting wrong cells

	TileScene() : Scene(W, H)
	{
		MTopView& view = View();
		view.m_TileSPK.Init(TYPE_SPRITEID(2 * W * H + 1));
		for (int id = 1; id <= 2 * W * H; ++id)
		{
			WORD colour = WORD(id);
			view.m_TileSPK[TYPE_SPRITEID(id)].SetPixelNoColorkey(&colour, 2, 1, 1);
		}
		Gen(0);
		view.m_pTileSurface = new CSpriteSurface;
		CHECK(view.m_pTileSurface->Init(g_TILESURFACE_WIDTH, g_TILESURFACE_HEIGHT));
		view.m_pTileSurface->FillSurface(kWhite);
		view.m_pTileRenderer = new TileRenderer;
		CHECK(view.m_pTileRenderer->Init(view.m_pTileSurface, &view.m_TileSPK));
		view.m_bFirstTileDraw = true;
		for (int set = 0; set < 4; ++set) savedSets[set] = CIndexSprite::GetUsingColorSet(BYTE(set));
		PlayerType(kVampire);
		player->m_Action = ACTION_STAND;
		player->m_CurrentDirection = 2;
		zone->m_mapCreature[kPlayerID] = player;
		Shape(view.m_CreatureFPK, 2, ACTION_MAX_VAMPIRE);
		Shape(view.m_CreatureShadowFPK, 2, ACTION_MAX_VAMPIRE);
		Place(view.m_CreatureFPK, 1, ACTION_STAND, 2, 0, 5, -20);
		view.m_CreatureSPK.Init(1);
		LoadPickIndexSprite(view.m_CreatureSPK[0]);
	}
	~TileScene()
	{
		for (int set = 0; set < 4; ++set) CIndexSprite::SetUsingColorSetOnly(BYTE(set), savedSets[set]);
	}
	static WORD Colour(int x, int y, int gen) { return WORD(1 + x + y * W + gen * W * H); }
	void Gen(int gen)
	{
		for (int y = 0; y < H; ++y)
			for (int x = 0; x < W; ++x) Sector(x, y).Set(Colour(x, y, gen));
	}
	CSpriteSurface& Tiles() { return *View().m_pTileSurface; }
	// The tile surface's corner, in sectors, from its pixel origin.
	POINT Origin() { return { View().m_TileSurfaceFirstZonePixelX / TILE_X, View().m_TileSurfaceFirstZonePixelY / TILE_Y }; }
	// Every cell of the tile surface shows its sector's tile of the
	// generation `gen` gives, black off the zone. Returns the cells that do
	// not.
	template<class GenOf> int WrongCells(GenOf gen)
	{
		const std::vector<WORD> pixels = Picture(Tiles(), g_TILESURFACE_WIDTH, g_TILESURFACE_HEIGHT);
		const POINT origin = Origin();
		CHECK_EQ(origin.x * TILE_X, View().m_TileSurfaceFirstZonePixelX);
		CHECK_EQ(origin.y * TILE_Y, View().m_TileSurfaceFirstZonePixelY);
		int wrong = 0;
		for (int j = 0; j * TILE_Y < g_TILESURFACE_HEIGHT; ++j)
			for (int i = 0; i * TILE_X < g_TILESURFACE_WIDTH; ++i)
			{
				const int sx = origin.x + i, sy = origin.y + j;
				const bool in = sx >= 0 && sy >= 0 && sx < W && sy < H;
				const WORD expected = in ? Colour(sx, sy, gen(sx, sy)) : 0;
				const WORD actual = pixels[size_t(j * TILE_Y) * g_TILESURFACE_WIDTH + i * TILE_X];
				if (actual != expected && wrong++ == 0 && !quiet)
					std::printf("      cell (%d,%d), sector (%d,%d): %04x, not %04x\n", i, j, sx, sy, actual, expected);
			}
		return wrong;
	}
	// The screen shows the tile surface's cells where the view says.
	int WrongScreenCorners()
	{
		MTopView& view = View();
		int wrong = 0;
		for (int sy = 0; sy < H; ++sy)
			for (int sx = 0; sx < W; ++sx)
			{
				const int x = sx * TILE_X - view.m_FirstZonePixel.x, y = sy * TILE_Y - view.m_FirstZonePixel.y;
				if (x < 0 || y < 0 || x >= g_GameRect.right || y >= g_GameRect.bottom) continue;
				const int tx = sx * TILE_X - view.m_TileSurfaceFirstZonePixelX, ty = sy * TILE_Y - view.m_TileSurfaceFirstZonePixelY;
				const WORD expected = PixelAt(Tiles(), tx, ty);
				if (PixelAt(screen, x, y) != expected && wrong++ == 0)
					std::printf("      sector (%d,%d) on the screen: %04x, not %04x\n", sx, sy, PixelAt(screen, x, y), expected);
			}
		return wrong;
	}
	void Stand(int x, int y, int sx = 0, int sy = 0)
	{
		player->At(x, y);
		player->m_sX = sx;
		player->m_sY = sy;
	}
	void Frame(int firstX = 0, int firstY = 0) { Access::DrawZone(View(), firstX, firstY); }
};

} // namespace

TEST(TopView2Frame, NoZoneNoDraw)
{
	TileScene scene;
	MTopView& view = scene.View();
	g_pZone = NULL;
	Creature stale(3, kVampire, 1, 1);
	view.m_mapCreature[7] = &stale;
	view.Draw(0, 0);
	g_pZone = scene.zone.get();
	CHECK(view.m_bFirstTileDraw);
	CHECK_EQ(1, int(view.m_mapCreature.size()));
	const std::vector<WORD> tiles = Picture(scene.Tiles(), g_TILESURFACE_WIDTH, g_TILESURFACE_HEIGHT);
	CHECK(std::all_of(tiles.begin(), tiles.end(), [](WORD pixel) { return pixel == TileScene::kWhite; }));
	const std::vector<WORD> screen = Picture(scene.screen, 800, 600);
	CHECK(std::all_of(screen.begin(), screen.end(), [](WORD pixel) { return pixel == 0; }));
	view.m_mapCreature.clear();
}

TEST(TopView2Frame, TheFullTileDrawCentresThePlayerAndFillsTheSurfaceAroundIt)
{
	// No renderer: only the geometry and the black outside the zone.
	TileScene scene;
	MTopView& view = scene.View();
	delete view.m_pTileRenderer;
	view.m_pTileRenderer = NULL;
	const int xs[] = { 0, 2, 8, 9, 11, 12, 30, 51, 52, 59 };
	const int ys[] = { 0, 3, 12, 14, 15, 40, 60, 67, 79 };
	const int offsets[] = { 0, -10, 30 };
	for (int px : xs)
		for (int py : ys)
			for (int sx : offsets)
			{
				const int sy = -sx / 2;
				scene.Tiles().FillSurface(TileScene::kWhite);
				scene.Stand(px, py, sx, sy);
				Access::DrawTileSurface(view);
				const int zx = (px - 8) * TILE_X + sx, zy = (py - 12) * TILE_Y + sy;
				CHECK_EQ(zx, view.m_FirstZonePixel.x);
				CHECK_EQ(zy, view.m_FirstZonePixel.y);
				const int fx = zx / TILE_X, fy = zy / TILE_Y;		// toward zero
				CHECK_EQ(fx, view.m_FirstSector.x);
				CHECK_EQ(fy, view.m_FirstSector.y);
				CHECK_EQ((fx - 3) * TILE_X, view.m_TileSurfaceFirstZonePixelX);
				CHECK_EQ((fy - 3) * TILE_Y, view.m_TileSurfaceFirstZonePixelY);
				// Recorded defect (bug 9): off the zone's left or top edge
				// the surface's first sector is moved onto the zone
				// (MTopView.cpp:13905 and 13935) but its first pixel is not.
				CHECK_EQ((std::max)(fx - 3, 0), view.m_TileSurfaceFirstSectorX);
				CHECK_EQ((std::max)(fy - 3, 0), view.m_TileSurfaceFirstSectorY);
				// Black where the surface is off the zone, white (not drawn)
				// on it - unless the surface passes the zone's bottom: that
				// fill (MTopView.cpp:13939-13948) starts at the zone's first
				// row on the surface, not past its last, so every tile cell
				// is blacked before the tiles are drawn over it (wasted, not
				// seen).
				const POINT origin = { fx - 3, fy - 3 };
				const bool pastBottom = origin.y + g_TILESURFACE_SECTOR_HEIGHT > TileScene::H;
				int wrong = 0;
				for (int j = 0; j * TILE_Y < g_TILESURFACE_HEIGHT; ++j)
					for (int i = 0; i * TILE_X < g_TILESURFACE_WIDTH; ++i)
					{
						const int cx = origin.x + i, cy = origin.y + j;
						const bool in = cx >= 0 && cy >= 0 && cx < TileScene::W && cy < TileScene::H;
						if (PixelAt(scene.Tiles(), i * TILE_X + 47, j * TILE_Y + 23) != (in && !pastBottom ? TileScene::kWhite : 0)) ++wrong;
					}
				CHECK_EQ(0, wrong);
			}
}

TEST(TopView2Frame, TheFullTileDrawDrawsEveryTileOfTheSurface)
{
	TileScene scene;
	for (int px : { 30, 2, 57 })
		for (int py : { 40, 1, 78 })
		{
			scene.Tiles().FillSurface(TileScene::kWhite);
			scene.Stand(px, py);
			Access::DrawTileSurface(scene.View());
			CHECK_EQ(0, scene.WrongCells([](int, int) { return 0; }));
		}
}

TEST(TopView2Frame, AFrameScrollsTheTilesAndRedrawsOnlyTheThreeNewSectorsOfEachEdge)
{
	TileScene scene;
	MTopView& view = scene.View();
	const int dirs[][2] = { {-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {-1, 1}, {1, -1}, {1, 1} };
	for (const auto& d : dirs)
	{
		scene.Gen(0);
		scene.Tiles().FillSurface(TileScene::kWhite);
		scene.Stand(30, 40);
		Access::DrawTileSurface(view);
		scene.Frame();
		const POINT old = scene.Origin();
		CHECK_EQ(19, old.x);	// three sectors left of and above the screen
		CHECK_EQ(25, old.y);
		CHECK_EQ(0, scene.WrongCells([](int, int) { return 0; }));
		scene.Gen(1);
		for (int step = 1; step <= 3; ++step)
		{
			scene.Stand(30 + d[0] * step, 40 + d[1] * step);
			scene.Frame();
			CHECK_EQ(22 + d[0] * step, view.m_FirstSector.x);
			CHECK_EQ(28 + d[1] * step, view.m_FirstSector.y);
			const POINT origin = scene.Origin();
			// The surface moves only once the screen reaches its edge.
			CHECK_EQ(old.x + (step == 3 ? 3 * d[0] : 0), origin.x);
			CHECK_EQ(old.y + (step == 3 ? 3 * d[1] : 0), origin.y);
			CHECK_EQ(origin.x, view.m_TileSurfaceFirstSectorX);
			CHECK_EQ(origin.y, view.m_TileSurfaceFirstSectorY);
			// The kept cells are the old tiles, the new sectors new ones.
			CHECK_EQ(0, scene.WrongCells([old](int x, int y) {
				return x >= old.x && x < old.x + 22 && y >= old.y && y < old.y + 31 ? 0 : 1;
			}));
			CHECK_EQ(0, scene.WrongScreenCorners());
		}
	}
}

TEST(TopView2Frame, ScrollingAwayFromAZoneEdgeAfterAFullDrawDrawsTheWrongTiles)
{
	// Recorded defect (bug 9): the full draw moves the surface's first
	// sector onto the zone at its left or top edge (MTopView.cpp:13905,
	// 13935) but not its first pixel, so the first scroll right or down
	// draws its new column or row of tiles one sector further on than the
	// cells it fills - visible at the screen's right or bottom edge.
	TileScene scene;
	MTopView& view = scene.View();
	scene.Stand(10, 40);		// the surface starts a sector left of the zone
	Access::DrawTileSurface(view);
	CHECK_EQ(-1, scene.Origin().x);
	CHECK_EQ(0, view.m_TileSurfaceFirstSectorX);
	CHECK_EQ(0, scene.WrongCells([](int, int) { return 0; }));
	scene.quiet = true;
	scene.Gen(1);
	for (int step = 1; step <= 3; ++step)
	{
		scene.Stand(10 + step, 40);
		scene.Frame();
	}
	CHECK_EQ(2, scene.Origin().x);
	CHECK_EQ(3, view.m_TileSurfaceFirstSectorX);
	// Cells 19 to 21 (sectors 21 to 23) show sectors 22 to 24.
	for (int i = 19; i <= 21; ++i)
		for (int j = 0; j < 31; ++j)
			CHECK_EQ(TileScene::Colour(i + 3, scene.Origin().y + j, 1), PixelAt(scene.Tiles(), i * TILE_X, j * TILE_Y));
	CHECK_EQ(3 * 31, scene.WrongCells([](int x, int) { return x >= 21 ? 1 : 0; }));
	CHECK_EQ(0, scene.WrongScreenCorners());	// and the screen shows them

	// The same at the top edge, scrolling down.
	scene.Gen(0);
	scene.Stand(30, 14);
	Access::DrawTileSurface(view);
	CHECK_EQ(-1, scene.Origin().y);
	CHECK_EQ(0, view.m_TileSurfaceFirstSectorY);
	scene.Gen(1);
	for (int step = 1; step <= 3; ++step)
	{
		scene.Stand(30, 14 + step);
		scene.Frame();
	}
	CHECK_EQ(2, scene.Origin().y);
	for (int j = 28; j <= 30; ++j)
		CHECK_EQ(TileScene::Colour(scene.Origin().x, j + 3, 1), PixelAt(scene.Tiles(), 0, j * TILE_Y));
	CHECK_EQ(3 * 22, scene.WrongCells([](int, int y) { return y >= 30 ? 1 : 0; }));
}

TEST(TopView2Frame, TheScreenStopsAtTheZonesEdges)
{
	// DrawZone's first sector and sub-sector offset (MTopView.cpp:10969-
	// 11066): x keeps the window SW+1 sectors inside the zone, y only SH,
	// and a player half a step past an edge does not scroll past it.
	TileScene scene;
	MTopView& view = scene.View();
	view.m_ImageObjectFilter.Init(10, 6);
	struct Case { int px, sx, firstX, plusX; };
	const Case xs[] = {
		{ 30, 7, 22, 7 }, { 30, -7, 22, -7 },
		{ 8, -5, 0, 0 }, { 8, 5, 0, 5 }, { 3, 7, 0, 0 },
		{ 51, 5, 43, 0 }, { 51, -5, 43, -5 }, { 55, -5, 43, 0 },
	};
	for (const Case& c : xs)
	{
		scene.Stand(c.px, 40, c.sx, 0);
		scene.Frame();
		CHECK_EQ(c.firstX, view.m_FirstSector.x);
		CHECK_EQ(c.plusX, view.m_PlusPoint.x);
		CHECK_EQ(c.firstX * TILE_X + c.plusX, view.m_FirstZonePixel.x);
		CHECK_EQ(28, view.m_FirstSector.y);
		CHECK_EQ(c.px * TILE_X + c.sx - view.m_FirstZonePixel.x - 5 + 24, view.m_FilterPosition.x);
	}
	struct CaseY { int py, sy, firstY, plusY; };
	const CaseY ys[] = {
		{ 40, 3, 28, 3 },
		{ 12, -3, 0, 0 }, { 12, 3, 0, 3 }, { 5, 3, 0, 0 },
		{ 67, 3, 55, 0 }, { 67, -3, 55, -3 }, { 70, -3, 55, 0 },
	};
	for (const CaseY& c : ys)
	{
		scene.Stand(30, c.py, 0, c.sy);
		scene.Frame();
		CHECK_EQ(c.firstY, view.m_FirstSector.y);
		CHECK_EQ(c.plusY, view.m_PlusPoint.y);
		CHECK_EQ(c.firstY * TILE_Y + c.plusY, view.m_FirstZonePixel.y);
		CHECK_EQ(22, view.m_FirstSector.x);
		CHECK_EQ(c.py * TILE_Y + c.sy - view.m_FirstZonePixel.y - 3 - TILE_Y, view.m_FilterPosition.y);
	}
	// The camera's point is taken off the offset; a flyer's filter sits
	// three tiles up.
	scene.Stand(30, 40, 6, -4);
	scene.player->m_MoveType = MCreature::CREATURE_FLYING;
	scene.Frame(2, -5);
	CHECK_EQ(4, view.m_PlusPoint.x);
	CHECK_EQ(1, view.m_PlusPoint.y);
	CHECK_EQ(22 * TILE_X + 4, view.m_FirstZonePixel.x);
	CHECK_EQ(28 * TILE_Y + 1, view.m_FirstZonePixel.y);
	CHECK_EQ(40 * TILE_Y - 4 - view.m_FirstZonePixel.y - 3 - 3 * TILE_Y, view.m_FilterPosition.y);
	scene.player->m_MoveType = MCreature::CREATURE_GROUND;
}

TEST(TopView2Frame, AFrameDrawsTheTilesAndThePlayerAndResetsWhatItShows)
{
	TileScene scene;
	MTopView& view = scene.View();
	scene.Stand(30, 40, 0, 0);
	Access::DrawTileSurface(view);
	view.m_bDrawRequest = true;
	view.m_pSelectedCreature = scene.player;
	view.m_SelectSector.x = 30;		// the click marker, at the player: cleared
	view.m_SelectSector.y = 40;
	scene.screen.FillSurface(0);
	scene.Frame();
	CHECK(!view.m_bDrawRequest);
	CHECK(view.m_pSelectedCreature == NULL);
	CHECK_EQ(SECTORPOSITION_NULL, view.m_SelectSector.x);
	CHECK_EQ(SECTORPOSITION_NULL, view.m_SelectSector.y);
	CHECK_EQ(1, int(view.m_mapCreature.size()));
	CHECK(view.m_mapCreature.count(view.GetOutputCreatureID(scene.player)) == 1);
	CHECK_EQ(0, scene.WrongScreenCorners());
	// The player's frame, 5 right of and 20 above its corner of the
	// screen, and its rect.
	const int x = 30 * TILE_X - view.m_FirstZonePixel.x + 5, y = 40 * TILE_Y - view.m_FirstZonePixel.y - 20;
	const RECT& rect = scene.player->GetScreenRect();
	CHECK_EQ(x, rect.left);
	CHECK_EQ(y, rect.top);
	CHECK_EQ(x + 3, rect.right);
	CHECK_EQ(y + 2, rect.bottom);
	for (int dx = 0; dx < 3; ++dx)
		for (int dy = 0; dy < 2; ++dy)
			CHECK_EQ(PickPixel(dx, dy), PixelAt(scene.screen, x + dx, y + dy) == 0x1234);
	// Off the screen's corners the picture is the tile surface's.
	CHECK(PixelAt(scene.screen, x - 1, y) != 0x1234);
}

TEST(TopView2Frame, AScrollToTheZonesEdgeBlacksTheNewCellsOffIt)
{
	// The new strip's cells off the zone are black, its cells on it drawn,
	// and the surface's first sector and pixel agree (unlike the full
	// draw's, above).
	TileScene scene;
	MTopView& view = scene.View();
	const struct { int x0, y0, dx, dy; } cases[] = {
		{ 13, 40, -1, 0 },		// first sectors 5 to 2: the strip from column -1
		{ 30, 17, 0, -1 },		// row -1
		{ 48, 40, 1, 0 },		// 40 to 43: the strip to column 61
		{ 30, 64, 0, 1 },		// 52 to 55: the strip rows 80 to 82
		{ 13, 17, -1, -1 },
	};
	for (const auto& c : cases)
	{
		scene.Gen(0);
		scene.Tiles().FillSurface(TileScene::kWhite);
		scene.Stand(c.x0, c.y0);
		Access::DrawTileSurface(view);
		scene.Frame();
		const POINT old = scene.Origin();
		scene.Gen(1);
		for (int step = 1; step <= 3; ++step)
		{
			scene.Stand(c.x0 + c.dx * step, c.y0 + c.dy * step);
			scene.Frame();
		}
		const POINT origin = scene.Origin();
		CHECK_EQ(old.x + 3 * c.dx, origin.x);
		CHECK_EQ(old.y + 3 * c.dy, origin.y);
		CHECK_EQ(origin.x, view.m_TileSurfaceFirstSectorX);
		CHECK_EQ(origin.y, view.m_TileSurfaceFirstSectorY);
		CHECK_EQ(0, scene.WrongCells([old](int x, int y) {
			return x >= old.x && x < old.x + 22 && y >= old.y && y < old.y + 31 ? 0 : 1;
		}));
		CHECK_EQ(0, scene.WrongScreenCorners());
	}
}
