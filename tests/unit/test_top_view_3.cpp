#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "MTopView.h"
#include "MPlayer.h"
#include "MZone.h"
#include "MCreatureWear.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "MEventManager.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "MObjectSelector.h"
#include "MAnimationObject.h"
#include "MShadowObject.h"
#include "MShadowAnimationObject.h"
#include "MEffect.h"
#include "MScreenEffect.h"
#include "MAttachEffect.h"
#include "MAttachOrbitEffect.h"
#include "EffectOrbit.h"
#include "MathTable.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MItemTable.h"
#include "MItemOptionTable.h"
#include "UserOption.h"
#include "CreatureShadowDraw.h"
#include "WorldEventsHost.h"
#include "EffectSpriteTypeDef.h"
#include "SpriteIDDef.h"
#include "SoundDef.h"
#include "FrameClock.h"
#include "MonotonicClock.h"
#include "Packet/Properties.h"
#include "CSpriteSurface.h"
#include "CSprite.h"
#include "CShadowSprite.h"
#include "CIndexSprite.h"
#include "CAlphaSpritePal.h"
#include "CSpritePal.h"
#include "MPalette.h"
#include "DXLib/CDirectDraw.h"
#include "Fl2.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>

extern RECT g_GameRect;
extern int g_FrameCount;
extern PrintInfo* g_ClientPrintInfo[];
// MItemUse.cpp's item factory, which the executable installs as MItem's host.
MItem*	NewItemOfClass(ITEM_CLASS itemClass);

// The view's world drawing (task 4.260 moved it into gameclient): the
// party HP bar, items, image objects, effects, the broken-gear picture,
// burrowed creatures, the alpha box, the screen fade, and the two
// endings. Each picture test draws through the view onto one surface and
// lays the blits it should make onto another; the two must match. The
// surfaces are grey, so a shadow, a half blend and a screen blend show.
//
// SpriteLib's SDL backend stubs several blits a test here meets (each
// pinned where it shows): BltColorAlpha draws nothing, so the screen
// fade, the alpha box and the endings' fades are invisible;
// BltSpriteOutline draws nothing, so a selected item or burrow is not
// drawn at all; SetClipRight is a no-op, so a party HP bar is never cut;
// BltSpriteHalf draws the sprite at half size, where DirectDraw drew it
// half blended; BltSpriteAlphaFilter and BltIndexSpriteColor draw plain.
struct MTopViewTestAccess3
{
	static void InitColors(MTopView& v) { v.InitColors(); }
	static void DrawPartyHP(MTopView& v, POINT* p, MCreature* c) { v.DrawPartyHP(p, c); }
	static void DrawAlphaBox(MTopView& v, RECT* r, BYTE red, BYTE green, BYTE blue, BYTE alpha) { v.DrawAlphaBox(r, red, green, blue, alpha); }
	static void DrawFade(MTopView& v) { v.DrawFade(); }
	static void DrawItem(MTopView& v, POINT* p, MItem* item) { v.DrawItem(p, item); }
	static void DrawItemShadow(MTopView& v, POINT* p, MItem* item) { v.DrawItemShadow(p, item); }
	static void DrawImageObject(MTopView& v, POINT* p, MImageObject* o) { v.DrawImageObject(p, o); }
	static void DrawEffect(MTopView& v, POINT* p, MEffect* e, bool selectable) { v.DrawEffect(p, e, selectable); }
	static void DrawEffects(MTopView& v, POINT* p, EFFECT_LIST::const_iterator i, BYTE n) { v.DrawEffect(p, i, n); }
	static void DrawGroundEffect(MTopView& v) { v.DrawGroundEffect(); }
	static void DrawAttachEffect(MTopView& v, POINT* p, ATTACHEFFECT_LIST::const_iterator i, BYTE n, MCreature* c, int type)
	{
		v.DrawAttachEffect(p, i, n, c, type);
	}
	static void DrawItemBroken(MTopView& v, int x, int y) { v.DrawItemBroken(x, y); }
	static void DrawUndergroundCreature(MTopView& v, POINT* p, MCreature* c) { v.DrawUndergroundCreature(p, c); }
	static bool DrawEvent(MTopView& v) { return v.DrawEvent(); }
	static bool AdvancementEnding(MTopView& v, MEvent* e) { return v.ExcuteAdvancementQuestEnding(e); }
	static bool OustersEnding(MTopView& v) { return v.ExcuteOustersFinEvent(); }
	static CreatureDrawPacks Packs(MTopView& v) { return v.GetCreatureDrawPacks(); }
	static MScreenFade& Fade(MTopView& v) { return v.m_Fade; }
	static WORD FadeColor(MTopView& v) { return v.m_FadeColor; }
};
using Access = MTopViewTestAccess3;

namespace {

const char* kPath = "top_view_3_sprite.bin";
const WORD kPaper = 0x4208;

template<class Loaded> bool LoadBytes(Loaded& loaded, const std::vector<unsigned char>& bytes)
{
	{
		std::ofstream out(kPath, std::ios::binary | std::ios::trunc);
		out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
	}
	std::ifstream in(kPath, std::ios::binary);
	const bool loaded_ok = loaded.LoadFromFile(in);
	in.close();
	std::remove(kPath);
	return loaded_ok;
}

void Push(std::vector<unsigned char>& bytes, WORD word)
{
	bytes.push_back(static_cast<unsigned char>(word & 0xFF));
	bytes.push_back(static_cast<unsigned char>(word >> 8));
}

std::vector<unsigned char> Bytes(const std::vector<WORD>& words)
{
	std::vector<unsigned char> bytes;
	for (WORD word : words) Push(bytes, word);
	return bytes;
}

// A w x h sprite of one colour, in CSprite's file layout.
std::vector<WORD> SolidWords(int w, int h, WORD colour)
{
	std::vector<WORD> words{WORD(w), WORD(h)};
	for (int y = 0; y < h; ++y)
	{
		words.insert(words.end(), {WORD(3 + w), 1, 0, WORD(w)});
		for (int x = 0; x < w; ++x) words.push_back(colour);
	}
	return words;
}

void Solid(CSprite& sprite, int w, int h, WORD colour) { CHECK(LoadBytes(sprite, Bytes(SolidWords(w, h, colour)))); }

// A w x h shadow, every pixel cast.
void Shade(CShadowSprite& sprite, int w, int h)
{
	std::vector<WORD> words{WORD(w), WORD(h)};
	for (int y = 0; y < h; ++y) words.insert(words.end(), {3, 1, 0, WORD(w)});
	CHECK(LoadBytes(sprite, Bytes(words)));
}

// An index sprite of five pixels: colour-set channels 0 to 3 at gradation
// 2, then a fixed colour, its mark.
void Mark(CIndexSprite& sprite, WORD mark)
{
	CHECK(LoadBytes(sprite, Bytes({5, 1, 9, 1, 0, 4, 0x0002, 0x0102, 0x0202, 0x0302, 1, mark})));
}

// A palette sprite (CSpritePalBase's layout) of w x h pixels of palette
// index 1, each with the alpha given when `alpha` is not 0.
template<class Sprite> void PalSprite(Sprite& sprite, int w, int h, BYTE alpha)
{
	std::vector<unsigned char> line{1, 0, static_cast<unsigned char>(w)};
	for (int x = 0; x < w; ++x)
	{
		if (alpha != 0) line.push_back(alpha);
		line.push_back(1);
	}
	std::vector<unsigned char> bytes;
	const DWORD size = DWORD(line.size() * h);
	Push(bytes, WORD(size & 0xFFFF));
	Push(bytes, WORD(size >> 16));
	Push(bytes, WORD(w));
	Push(bytes, WORD(h));
	for (int y = 0; y < h; ++y) bytes.insert(bytes.end(), line.begin(), line.end());
	for (int y = 0; y < h; ++y) Push(bytes, WORD(line.size()));
	CHECK(LoadBytes(sprite, bytes));
}

void Palette(MPalette& palette, WORD colour)
{
	palette.Init(2);
	palette[0] = 0;
	palette[1] = colour;
}

struct Canvas
{
	CSpriteSurface surface;
	int width, height;
	Canvas(int w, int h) : width(w), height(h)
	{
		CHECK_EQ(0, spritectl_init());
		CHECK(surface.Init(w, h));
		surface.FillSurface(kPaper);
	}
	std::vector<WORD> Picture()
	{
		std::vector<WORD> pixels;
		DWORD pitch = 0;
		const unsigned char* base = static_cast<const unsigned char*>(surface.Lock(nullptr, &pitch));
		CHECK(base != nullptr);
		if (base == nullptr) return pixels;
		for (int y = 0; y < height; ++y)
			for (int x = 0; x < width; ++x)
				pixels.push_back(reinterpret_cast<const WORD*>(base + y * pitch)[x]);
		surface.Unlock();
		return pixels;
	}
	WORD At(int x, int y) { return Picture()[size_t(y * width + x)]; }
	bool Blank() { for (WORD pixel : Picture()) if (pixel != kPaper) return false; return true; }
	void Sprite(int x, int y, CSprite& sprite) { POINT p = {x, y}; surface.BltSprite(&p, &sprite); }
	void Index(int x, int y, CIndexSprite& sprite) { POINT p = {x, y}; surface.BltIndexSprite(&p, &sprite); }
	void Shadow(int x, int y, CShadowSprite& sprite) { POINT p = {x, y}; surface.BltShadowSpriteDarkness(&p, &sprite, 1); }
	void Black(int x, int y, CShadowSprite& sprite) { POINT p = {x, y}; surface.BltShadowSprite(&p, &sprite); }
	void Half(int x, int y, CSprite& sprite) { POINT p = {x, y}; surface.BltSpriteHalf(&p, &sprite); }
	void Alpha(int x, int y, CAlphaSpritePal& sprite, MPalette& pal) { POINT p = {x, y}; surface.BltAlphaSpritePal(&p, &sprite, pal); }
	void Screen(int x, int y, CSpritePal& sprite, MPalette& pal)
	{
		POINT p = {x, y};
		CSpriteSurface::SetPalEffect(CSpriteSurface::EFFECT_SCREEN);
		surface.BltSpritePalEffect(&p, &sprite, pal);
	}
};

// A creature whose state a test sets.
struct Creature : MCreatureWear
{
	using MCreature::m_Action;
	using MCreature::m_ActionCount;
	using MCreature::m_ActionCountMax;
	using MCreature::m_MoveCount;
	using MCreature::m_bEffectStatus;
	using MCreature::m_NameSelection;
	using MCreature::m_OriChatColor;
	using MCreature::m_NextChatFadeTime;
	using MCreature::m_bAlive;
	void HP(int current, int max) { m_Status[MODIFY_CURRENT_HP] = DWORD(current); m_Status[MODIFY_MAX_HP] = DWORD(max); }
	void Set(EFFECTSTATUS status, bool on = true) { m_bEffectStatus[status] = on; }
};

struct ZoneProbe : MZone
{
	using MZone::MZone;
	using MZone::m_Info;
	using MZone::m_mapGroundEffect;
	using MZone::m_ppSector;
	// The tests own the ground effects they lend the zone.
	~ZoneProbe() { m_mapGroundEffect.clear(); }
	// SetSector assigns only a sector's look, so darkness is set in place.
	void Dark(int x, int y) { m_ppSector[y][x].SetDarkness(); }
};

struct PlayerProbe : MPlayer
{
	using MCreature::m_bEffectStatus;
	void Set(EFFECTSTATUS status, bool on = true) { m_bEffectStatus[status] = on; }
};

const int kSlayer = 1, kVampire = 2, kOusters = 3, kMonster = 4;

// The world the view reads, every global put back afterwards; a test
// fills the view's packs it draws from.
struct Scene
{
	GameModelWorld world;
	CREATURE_TABLE creatureTypes;
	CREATURESPRITE_TABLE sprites;
	EFFECTSPRITETYPE_TABLE effectSprites;
	MActionEffectSpriteTypeTable actionEffects;
	EFFECTSTATUS_TABLE effectStatuses;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	RankBonusTable rankBonuses;
	MEventQueue events;
	MObjectSelector selector;
	UserOption option;
	UserOption* oldOption = g_pUserOption;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	MActionEffectSpriteTypeTable* oldActionEffects = g_pActionEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MEventQueue* oldEvents = g_pEventManager;
	MObjectSelector* oldSelector = g_pObjectSelector;
	MPlayer* oldPlayer = g_pPlayer;
	MZone* oldZone = g_pZone;
	MSlayerGear* oldSlayerGear = g_pSlayerGear;
	MVampireGear* oldVampireGear = g_pVampireGear;
	MOustersGear* oldOustersGear = g_pOustersGear;
	DWORD oldFrame = g_CurrentFrame;
	bool oldFrameChanged = g_bFrameChanged;
	int oldFrameCount = g_FrameCount;
	MonotonicClock::TimePoint oldFrameNow = g_FrameNow;
	RECT oldGameRect = g_GameRect;
	CFilter* oldFilter = CSprite::GetFilter();
	decltype(CSpriteSurface::s_pMemcpyPalEffectFunction) oldPalEffect = CSpriteSurface::s_pMemcpyPalEffectFunction;
	int savedSets[4];
	PlayerProbe* player = nullptr;
	std::unique_ptr<ZoneProbe> zone;
	std::unique_ptr<MTopView> view;
	Canvas drawn, expected;

	explicit Scene(int w = 96, int h = 96) : drawn(w, h), expected(w, h)
	{
		for (int set = 0; set < 4; ++set) savedSets[set] = CIndexSprite::GetUsingColorSet(BYTE(set));
		// A creature's draw gap is 0 off the frame it snapped on (0).
		g_CurrentFrame = 1;
		g_bFrameChanged = false;
		g_FrameCount = 0;
		g_GameRect = {0, 0, w, h};
		creatureTypes.Init(800);
		// Every type has its actions; 2 and 4 are vampires, 3 an ousters, the rest slayers.
		for (int type = 0; type < 800; ++type)
		{
			CREATURETABLE_INFO& row = testfw::MutableRow(creatureTypes, type);
			row.SetCreatureTribe(type == kVampire || type == kMonster ? CREATURETRIBE_VAMPIRE
				: type == kOusters ? CREATURETRIBE_OUSTERS : CREATURETRIBE_SLAYER, ACTION_MAX_SLAYER);
		}
		g_pCreatureTable = &creatureTypes;
		sprites.Init(1);
		g_pCreatureSpriteTable = &sprites;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		// The row's constructor leaves the action frame unset.
		for (int type = 0; type < MAX_EFFECTSPRITETYPE; ++type)
			testfw::MutableRow(effectSprites, type).ActionEffectFrameID = FRAMEID_NULL;
		g_pEffectSpriteTypeTable = &effectSprites;
		g_pActionEffectSpriteTypeTable = &actionEffects;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		g_pRankBonusTable = &rankBonuses;
		g_pEventManager = &events;
		g_pObjectSelector = &selector;
		g_pUserOption = &option;
		g_pUserInformation->GoreLevel = true;
		zone.reset(new ZoneProbe(TYPE_SECTORPOSITION(16), TYPE_SECTORPOSITION(16)));
		g_pZone = zone.get();
		player = new PlayerProbe;
		g_pPlayer = player;
		player->SetCreatureType(kSlayer);
		player->MObject::SetPosition(8, 8);
		g_pClientConfig->COLOR_OUTLINE_ITEM = 0x1111;
		g_pClientConfig->COLOR_OUTLINE_NPC = 0x2222;
		g_pClientConfig->COLOR_OUTLINE_ATTACK_POSSIBLE = 0x3333;
		g_pClientConfig->COLOR_OUTLINE_ATTACK_IMPOSSIBLE = 0x4444;
		g_pClientConfig->COLOR_NAME_ITEM = RGB(1, 2, 3);
		g_pClientConfig->COLOR_NAME_ITEM_OPTION = RGB(4, 5, 6);
		g_pClientConfig->COLOR_NAME_ITEM_RARE_OPTION = RGB(7, 8, 9);
		g_pClientConfig->COLOR_NAME_VAMPIRE = RGB(10, 11, 12);
		g_pClientConfig->FONT_HEIGHT = 12;
		g_pClientConfig->FONT_ITEM_HEIGHT = 14;
		g_pClientConfig->MAX_DRAWITEMNAME = 4;
		view.reset(new MTopView);
		CSpriteSurface* surface = &drawn.surface;
		view->SetSurface(surface);
		view->SetZone(zone.get());
		Access::InitColors(*view);
		// The constructor leaves these unset.
		view->m_pSelectedCreature = nullptr;
		view->m_pointChatString = {-1, -1};
		view->m_FilterPosition = {0, 0};
		view->m_nLight = 0;
	}
	~Scene()
	{
		view.reset();
		zone.reset();
		g_pZone = oldZone;
		delete player;
		g_pPlayer = oldPlayer;
		for (int set = 0; set < 4; ++set) CIndexSprite::SetUsingColorSetOnly(BYTE(set), savedSets[set]);
		CSpriteSurface::s_pMemcpyPalEffectFunction = oldPalEffect;
		// Every user sets the filter's offset with it, so the pointer is all to put back.
		CSprite::SetFilter(oldFilter);
		g_GameRect = oldGameRect;
		g_FrameNow = oldFrameNow;
		g_FrameCount = oldFrameCount;
		g_bFrameChanged = oldFrameChanged;
		g_CurrentFrame = oldFrame;
		g_pOustersGear = oldOustersGear;
		g_pVampireGear = oldVampireGear;
		g_pSlayerGear = oldSlayerGear;
		g_pUserOption = oldOption;
		g_pObjectSelector = oldSelector;
		g_pEventManager = oldEvents;
		g_pRankBonusTable = oldRankBonuses;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pActionEffectSpriteTypeTable = oldActionEffects;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatureTypes;
	}
	MTopView& View() { return *view; }
	// The drawn picture is the expected one, and something was drawn.
	bool Same()
	{
		const std::vector<WORD> a = drawn.Picture(), b = expected.Picture();
		bool any = false;
		for (WORD pixel : a) any |= pixel != kPaper;
		if (!any) std::printf("      nothing was drawn\n");
		for (size_t i = 0; i < a.size() && i < b.size(); ++i)
			if (a[i] != b[i])
			{
				std::printf("      the picture differs first at (%d,%d): %04x, not %04x\n",
					int(i % size_t(drawn.width)), int(i / size_t(drawn.width)), a[i], b[i]);
				break;
			}
		return any && a == b;
	}
	void Clear()
	{
		drawn.surface.FillSurface(kPaper);
		expected.surface.FillSurface(kPaper);
	}
	void Event(EVENT_ID id, DWORD flag = 0)
	{
		MEvent event;
		event.eventID = id;
		event.eventType = EVENTTYPE_ZONE;
		event.eventDelay = -1;
		event.eventFlag = flag;
		events.AddEvent(event);
	}
};

bool SameRect(const RECT& a, int left, int top, int right, int bottom)
{
	const bool same = a.left == left && a.top == top && a.right == right && a.bottom == bottom;
	if (!same) std::printf("      the rect is (%d,%d)-(%d,%d), not (%d,%d)-(%d,%d)\n",
		int(a.left), int(a.top), int(a.right), int(a.bottom), left, top, right, bottom);
	return same;
}

} // namespace

//----------------------------------------------------------------------
// The party HP bar
//----------------------------------------------------------------------
namespace {

// The bar's background (10 x 2) and a wider foreground (30 x 2), apart
// in colour: the HP share is of the background's width.
struct PartyBar : Scene
{
	Creature member;
	PartyBar()
	{
		View().m_EtcSPK.Init(SPRITEID_PARTY_HP + 1);
		Solid(View().m_EtcSPK[SPRITEID_PARTY_HP_BACKGROUND], 10, 2, 0x1111);
		Solid(View().m_EtcSPK[SPRITEID_PARTY_HP], 30, 2, 0xF800);
		member.SetCreatureType(kMonster);
	}
	// Draws the bar for a caller at (x, y); the bar starts 10 to the right.
	POINT Draw(int x, int y)
	{
		POINT point = {x, y};
		Access::DrawPartyHP(View(), &point, &member);
		return point;
	}
	void Background(int x, int y) { expected.Sprite(x, y, View().m_EtcSPK[SPRITEID_PARTY_HP_BACKGROUND]); }
	void Foreground(int x, int y) { expected.Sprite(x, y, View().m_EtcSPK[SPRITEID_PARTY_HP]); }
};

} // namespace

TEST(TopView3PartyHP, MovesTheCallersPointTenRightAndDrawsBothBarsThere)
{
	PartyBar scene;
	scene.member.HP(10, 10);
	const POINT point = scene.Draw(20, 30);
	// Recorded defect: the caller's point is moved, not a copy of it.
	CHECK_EQ(30, int(point.x));
	CHECK_EQ(30, int(point.y));
	scene.Background(30, 30);
	scene.Foreground(30, 30);
	CHECK(scene.Same());
}

TEST(TopView3PartyHP, TheForegroundIsNeverCutToTheHPOnSDL)
{
	PartyBar scene;
	scene.member.HP(1, 10);
	scene.Draw(20, 30);
	// Recorded defect: SetClipRight is a no-op on SDL, so one tenth of the
	// HP shows a full bar; DirectDraw cut it at one pixel.
	scene.Background(30, 30);
	scene.Foreground(30, 30);
	CHECK(scene.Same());
}

TEST(TopView3PartyHP, TheForegroundIsDrawnOnlyWhenItsHPEdgeIsRightOfTheSurfacesLeft)
{
	// The bar at x = -5: the edge is -5 plus the HP's share of 10 pixels.
	PartyBar scene;
	scene.member.HP(6, 10);			// edge 1: drawn
	scene.Draw(-15, 4);
	scene.Background(-5, 4);
	scene.Foreground(-5, 4);
	CHECK(scene.Same());

	scene.Clear();
	scene.member.HP(5, 10);			// edge 0: not drawn
	scene.Draw(-15, 4);
	scene.Background(-5, 4);
	CHECK(scene.Same());
}

TEST(TopView3PartyHP, HPOverTheMaximumCountsAsTheMaximum)
{
	// At x = -10 a full bar's edge is 0, so only HP past the maximum,
	// unclamped, would show the foreground; the background is off the surface.
	PartyBar scene;
	scene.member.HP(30, 10);
	scene.Draw(-20, 4);
	CHECK(scene.drawn.Blank());

	scene.member.HP(10, 10);		// at x = -9 the full bar's edge is 1
	scene.Draw(-19, 4);
	scene.Background(-9, 4);
	scene.Foreground(-9, 4);
	CHECK(scene.Same());
}

TEST(TopView3PartyHP, AMaximumOfZeroShowsNoHP)
{
	PartyBar scene;
	scene.member.HP(5, 0);
	scene.Draw(-14, 4);				// x = -4: any HP share would reach past 0
	scene.Background(-4, 4);
	CHECK(scene.Same());
}

TEST(TopView3PartyHP, ABurrowedCreatureShowsAFullBar)
{
	PartyBar scene;
	scene.member.HP(0, 10);
	scene.member.SetUndergroundCreature();
	scene.Draw(-14, 4);				// x = -4: a full bar's edge is 6
	scene.Background(-4, 4);
	scene.Foreground(-4, 4);
	CHECK(scene.Same());
}

TEST(TopView3PartyHP, ABarPastTheRightEdgeIsDrawnUncut)
{
	PartyBar scene;
	scene.member.HP(3, 10);
	scene.Draw(80, 50);				// x = 90: the edge, 93, is inside the 96 pixels: the cut branch
	scene.Background(90, 50);
	scene.Foreground(90, 50);
	CHECK(scene.Same());

	scene.Clear();
	scene.member.HP(10, 10);
	scene.Draw(80, 60);				// edge 100, past the surface: the uncut branch
	scene.Background(90, 60);
	scene.Foreground(90, 60);
	CHECK(scene.Same());
}

//----------------------------------------------------------------------
// The alpha box and the screen fade
//----------------------------------------------------------------------
TEST(TopView3AlphaBox, LocksAnUnlockedSurfaceOnlyWhileItDraws)
{
	Scene scene;
	RECT rect = {4, 4, 40, 40};
	CHECK(!scene.drawn.surface.IsLock());
	Access::DrawAlphaBox(scene.View(), &rect, 200, 100, 50, 16);
	CHECK(!scene.drawn.surface.IsLock());
	// Recorded defect: BltColorAlpha is an SDL stub, so a coloured box draws nothing.
	CHECK(scene.drawn.Blank());
}

TEST(TopView3AlphaBox, LeavesALockedSurfaceLocked)
{
	Scene scene;
	RECT rect = {4, 4, 40, 40};
	scene.drawn.surface.Lock();
	Access::DrawAlphaBox(scene.View(), &rect, 200, 100, 50, 16);
	CHECK(scene.drawn.surface.IsLock());
	scene.drawn.surface.Unlock();
	CHECK(scene.drawn.Blank());
}

TEST(TopView3AlphaBox, ABlackBoxDrawsNothing)
{
	// The gamma box that darkened black boxes is commented out.
	Scene scene;
	RECT rect = {4, 4, 40, 40};
	Access::DrawAlphaBox(scene.View(), &rect, 0, 0, 0, 16);
	CHECK(!scene.drawn.surface.IsLock());
	CHECK(scene.drawn.Blank());
}

TEST(TopView3Fade, StartSetsTheShadeTheEndTheStepAndTheColour)
{
	Scene scene;
	scene.View().SetFadeStart(4, 20, -3, 255, 0, 8);
	CHECK(scene.View().IsFade());
	CHECK_EQ(4, int(Access::Fade(scene.View()).Value()));
	CHECK_EQ(int(CSDLGraphics::Color(255, 0, 8)), int(Access::FadeColor(scene.View())));
	CHECK(Access::FadeColor(scene.View()) != 0);
	// The step's sign follows start and end, whatever the caller's.
	g_bFrameChanged = true;
	Access::DrawFade(scene.View());
	CHECK_EQ(7, int(Access::Fade(scene.View()).Value()));
}

TEST(TopView3Fade, DrawsTheShadeThenStepsOnAChangedFrame)
{
	Scene scene;
	scene.View().SetFadeStart(20, 2, 5, 255, 255, 255);
	g_bFrameChanged = false;
	Access::DrawFade(scene.View());
	CHECK_EQ(20, int(Access::Fade(scene.View()).Value()));
	g_bFrameChanged = true;
	Access::DrawFade(scene.View());
	CHECK_EQ(15, int(Access::Fade(scene.View()).Value()));
	CHECK(!scene.drawn.surface.IsLock());
	// Recorded defect: BltColorAlpha is an SDL stub, so the fade is invisible.
	CHECK(scene.drawn.Blank());
}

TEST(TopView3Fade, ADelayedFadeStepsByTheCurrentFrame)
{
	Scene scene;
	scene.View().SetFadeStart(20, 2, 5, 255, 255, 255, 3);
	g_bFrameChanged = true;
	g_CurrentFrame = 100;
	Access::DrawFade(scene.View());		// starts the fade's clock at 100
	CHECK_EQ(20, int(Access::Fade(scene.View()).Value()));
	g_CurrentFrame = 102;
	Access::DrawFade(scene.View());
	CHECK_EQ(20, int(Access::Fade(scene.View()).Value()));
	g_CurrentFrame = 103;
	Access::DrawFade(scene.View());
	CHECK_EQ(15, int(Access::Fade(scene.View()).Value()));
}

TEST(TopView3Fade, ABlackFadeStillSteps)
{
	Scene scene;
	scene.View().SetFadeStart(20, 2, 5);
	CHECK_EQ(0, int(Access::FadeColor(scene.View())));
	g_bFrameChanged = true;
	Access::DrawFade(scene.View());
	CHECK_EQ(15, int(Access::Fade(scene.View()).Value()));
	CHECK(scene.drawn.Blank());
}

TEST(TopView3Fade, TheNoFadeEventHoldsTheFade)
{
	Scene scene;
	scene.View().SetFadeStart(20, 2, 5, 255, 255, 255);
	scene.Event(EVENTID_METEOR, EVENTFLAG_NOT_FADE_SCREEN);
	g_bFrameChanged = true;
	Access::DrawFade(scene.View());
	CHECK_EQ(20, int(Access::Fade(scene.View()).Value()));
	CHECK(scene.View().IsFade());
}

TEST(TopView3Fade, AnEndedFadeIsNotAdvanced)
{
	Scene scene;
	scene.View().SetFadeStart(20, 2, 5, 255, 255, 255);
	scene.View().SetFadeEnd();
	g_bFrameChanged = true;
	Access::DrawFade(scene.View());
	CHECK_EQ(20, int(Access::Fade(scene.View()).Value()));
	CHECK(!scene.drawn.surface.IsLock());
}

TEST(TopView3Fade, TheFadeEndsPastItsEnd)
{
	Scene scene;
	scene.View().SetFadeStart(6, 2, 5, 255, 255, 255);
	g_bFrameChanged = true;
	Access::DrawFade(scene.View());
	CHECK_EQ(1, int(Access::Fade(scene.View()).Value()));
	CHECK(!scene.View().IsFade());
}

//----------------------------------------------------------------------
// A burrowed creature
//----------------------------------------------------------------------
namespace {

struct Burrow : Scene
{
	Creature mole;
	Burrow()
	{
		View().m_EtcSPK.Init(1);
		Solid(View().m_EtcSPK[SPRITEID_CREATURE_BURROW], 6, 3, 0x07E0);
		mole.SetCreatureType(kVampire);
		mole.SetID(77);
		mole.MObject::SetPosition(3, 3);
	}
	POINT Draw(int x, int y)
	{
		POINT point = {x, y};
		Access::DrawUndergroundCreature(View(), &point, &mole);
		return point;
	}
	bool ChatAt(int x, int y)
	{
		const POINT& at = View().m_pointChatString;
		if (at.x != x || at.y != y) std::printf("      the name goes at (%d,%d), not (%d,%d)\n", int(at.x), int(at.y), x, y);
		return at.x == x && at.y == y;
	}
};

} // namespace

TEST(TopView3Burrow, AnUnselectedBurrowIsDrawnFourDownRightAndPickedThere)
{
	Burrow scene;
	const POINT point = scene.Draw(20, 30);
	CHECK_EQ(20, int(point.x));
	CHECK_EQ(30, int(point.y));
	scene.expected.Sprite(24, 34, scene.View().m_EtcSPK[SPRITEID_CREATURE_BURROW]);
	CHECK(scene.Same());
	CHECK(SameRect(scene.mole.GetScreenRect(), 24, 34, 30, 37));
	CHECK(scene.View().m_SOM.GetOutlines().empty());
	CHECK(scene.View().m_pSelectedCreature == nullptr);
	CHECK(scene.ChatAt(-1, -1));
}

TEST(TopView3Burrow, ASelectedBurrowIsOnlyOutlinedAndMarksWhereItsNameGoes)
{
	Burrow scene;
	scene.View().SetSelectedCreature(77);
	scene.Draw(20, 30);
	// Recorded defect: BltSpriteOutline is an SDL stub, so a selected burrow is not drawn at all.
	CHECK(scene.drawn.Blank());
	CHECK(SameRect(scene.mole.GetScreenRect(), 24, 34, 30, 37));
	const std::vector<OutlineEntry>& outlines = scene.View().m_SOM.GetOutlines();
	CHECK_EQ(size_t(1), outlines.size());
	if (outlines.size() == 1)
	{
		CHECK_EQ(24, outlines[0].x);
		CHECK_EQ(34, outlines[0].y);
		CHECK(outlines[0].pSprite == &scene.View().m_EtcSPK[SPRITEID_CREATURE_BURROW]);
	}
	// The selector has no host here, so nothing can be attacked.
	CHECK_EQ(0x4444, int(scene.View().m_SOMOutlineColor));
	CHECK(scene.ChatAt(20, 18));
	CHECK(scene.View().m_pSelectedCreature == &scene.mole);
}

TEST(TopView3Burrow, AnAttackableSelectedBurrowTakesTheAttackColour)
{
	Burrow scene;
	MObjectSelectorHost host;
	host.IsNonPK = +[]() { return false; };
	host.ForceAttack = +[]() { return true; };
	MObjectSelector selector(&host);
	g_pObjectSelector = &selector;
	scene.View().SetSelectedCreature(77);
	scene.Draw(20, 30);
	CHECK_EQ(0x3333, int(scene.View().m_SOMOutlineColor));
	g_pObjectSelector = &scene.selector;
}

TEST(TopView3Burrow, TheNameStaysOnTheSurface)
{
	Burrow scene;
	scene.View().SetSelectedCreature(77);
	scene.Draw(-5, 23);					// a font's height (12) above, but not above 12
	CHECK(scene.ChatAt(0, 12));
	scene.Draw(7, 25);
	CHECK(scene.ChatAt(7, 13));
	// A level name takes two lines.
	scene.levelNames.Init(2);
	scene.mole.m_NameSelection.SelectLevelName(scene.levelNames, 1);
	CHECK(scene.mole.HasLevelName());
	scene.Draw(7, 40);
	CHECK(scene.ChatAt(7, 24));
	scene.Draw(7, 60);
	CHECK(scene.ChatAt(7, 36));
}

//----------------------------------------------------------------------
// The blood bible in the gear window
//----------------------------------------------------------------------
namespace {

// Fourteen frames, frame f at (f, 2f), sprites alternating; frame 7 has none.
struct Bible : Scene
{
	static const int kID = EFFECTSPRITETYPE_BLOODY_BIBLE_IN_GEAR;
	Bible()
	{
		CEffectFramePack& frames = View().m_EffectAlphaFPK;
		frames.Init(TYPE_FRAMEID(kID + 1));
		frames[kID].Init(1);
		frames[kID][0].Init(14);
		for (int f = 0; f < 14; ++f)
			frames[kID][0][WORD(f)].Set(TYPE_SPRITEID(f == 7 ? SPRITEID_NULL : f % 2), short(f), short(2 * f), 0, false);
		View().m_EffectAlphaSPK.Init(2);
		PalSprite(View().m_EffectAlphaSPK[0], 3, 2, 0x10);
		PalSprite(View().m_EffectAlphaSPK[1], 2, 3, 0x20);
		View().m_EffectAlphaPPK.Init(WORD(kID + 1));
		Palette(View().m_EffectAlphaPPK[WORD(kID)], 0x07E0);
	}
	void Draw()
	{
		POINT point = {30, 20};
		View().DrawBloodBibleEffect_InGear(&point);
		CHECK_EQ(30, int(point.x));
		CHECK_EQ(20, int(point.y));
	}
	void Frame(int f) { expected.Alpha(30 + f, 20 + 2 * f, View().m_EffectAlphaSPK[WORD(f % 2)], View().m_EffectAlphaPPK[WORD(kID)]); }
};

} // namespace

TEST(TopView3BloodBible, ShowsFrameEightCountsInOfAHundredAndTwelve)
{
	for (int count : {43, 111, 120})
	{
		Bible scene;
		g_FrameCount = count;
		scene.Draw();
		scene.Frame((count % 112) >> 3);
		CHECK(scene.Same());
	}
}

TEST(TopView3BloodBible, AFrameWithoutASpriteShowsNothing)
{
	Bible scene;
	g_FrameCount = 63;
	scene.Draw();
	CHECK(scene.drawn.Blank());
}

//----------------------------------------------------------------------
// The broken-gear picture
//----------------------------------------------------------------------
namespace {

template<class Gear> struct GearProbe : Gear
{
	using MSlotItemManager::m_ItemSlot;
	using MPlayerGear::m_pItemStatus;
	using MPlayerGear::m_nBrokenItem;
};

// Each race's picture is sprite 0, 33, 48, 59 or 71; frame k of every
// frame type is at ((k % 16) * 3, 8 + (k / 16) * 3), its sprite of a
// colour of its own and its type's.
struct Broken : Scene
{
	GearProbe<MSlayerGear> slayer;
	GearProbe<MVampireGear> vampire;
	GearProbe<MOustersGear> ousters;
	std::vector<std::unique_ptr<MItem>> items;
	Broken()
	{
		CSpritePack& sprites = View().m_ItemBrokenSPK;
		sprites.Init(200);
		for (int body : {0, 33, 48, 59, 71}) Solid(sprites[WORD(body)], 4, 4, WORD(0x8000 | body));
		for (int n = 0; n < 100; ++n) Solid(sprites[WORD(100 + n)], 2, 2, WORD(0x0821 * (n + 1)));
		CAnimationFramePack& frames = View().m_ItemBrokenFPK;
		frames.Init(5);
		for (int type = 0; type < 5; ++type)
		{
			frames[WORD(type)].Init(60);
			for (int k = 0; k < 60; ++k)
				frames[WORD(type)][WORD(k)].Set(TYPE_SPRITEID(100 + (k + 9 * type) % 100), short((k % 16) * 3), short(8 + (k / 16) * 3));
		}
		slayer.MPlayerGear::Init(BYTE(MSlayerGear::MAX_GEAR_SLAYER));
		vampire.MPlayerGear::Init(BYTE(MVampireGear::MAX_GEAR_VAMPIRE));
		ousters.MPlayerGear::Init(BYTE(MOustersGear::MAX_GEAR_OUSTERS));
		g_pSlayerGear = &slayer;
		g_pVampireGear = &vampire;
		g_pOustersGear = &ousters;
	}
	template<class Gear> void Put(Gear& gear, int slot, ITEM_CLASS itemClass, MPlayerGear::ITEM_STATUS status)
	{
		items.emplace_back(NewItemOfClass(itemClass));
		gear.m_ItemSlot[slot] = items.back().get();
		gear.m_pItemStatus[slot] = status;
		if (status != MPlayerGear::ITEM_STATUS_OK) ++gear.m_nBrokenItem;
	}
	void Draw() { Access::DrawItemBroken(View(), 5, 4); }
	void Body(int sprite) { expected.Sprite(20, 4, View().m_ItemBrokenSPK[WORD(sprite)]); }
	void Frame(int type, int k) { expected.Sprite(20 + (k % 16) * 3, 12 + (k / 16) * 3, View().m_ItemBrokenSPK[WORD(100 + (k + 9 * type) % 100)]); }
	void Be(int type, bool male)
	{
		player->SetCreatureType(TYPE_CREATURETYPE(type));
		player->SetMale(male);
	}
};

const auto OK = MPlayerGear::ITEM_STATUS_OK;
const auto SOMEWHAT = MPlayerGear::ITEM_STATUS_SOMEWHAT_BROKEN;
const auto ALMOST = MPlayerGear::ITEM_STATUS_ALMOST_BROKEN;

} // namespace

TEST(TopView3ItemBroken, NothingWithoutABrokenItem)
{
	Broken scene;
	scene.Be(kSlayer, true);
	scene.Put(scene.slayer, MSlayerGear::GEAR_SLAYER_HELM, ITEM_CLASS_HELM, OK);
	scene.Draw();
	CHECK(scene.drawn.Blank());
	CHECK(!scene.drawn.surface.IsLock());
}

TEST(TopView3ItemBroken, ASlayersGearShowsEachSlotByItsWear)
{
	Broken scene;
	scene.Be(kSlayer, true);
	scene.Put(scene.slayer, MSlayerGear::GEAR_SLAYER_HELM, ITEM_CLASS_HELM, OK);				// frame 0
	scene.Put(scene.slayer, MSlayerGear::GEAR_SLAYER_COAT, ITEM_CLASS_COAT, SOMEWHAT);			// 2 + 17
	scene.Put(scene.slayer, MSlayerGear::GEAR_SLAYER_GLOVE, ITEM_CLASS_GLOVE, ALMOST);			// 5 + 34
	scene.Put(scene.slayer, MSlayerGear::GEAR_SLAYER_BELT, ITEM_CLASS_COUPLE_RING, ALMOST);	// a couple ring shows whole: 6
	scene.Draw();
	scene.Body(SPRITEID_ITEM_BROKEN_SLAYER_MALE);
	for (int k : {0, 19, 39, 6}) scene.Frame(0, k);
	CHECK(scene.Same());
	CHECK(!scene.drawn.surface.IsLock());
}

TEST(TopView3ItemBroken, ASlayersHandsShowTheShieldAndTheWeaponKind)
{
	const struct { ITEM_CLASS left, right; int leftFrame, rightFrame; } hands[] = {
		{ ITEM_CLASS_SHIELD, ITEM_CLASS_AR, 3, MSlayerGear::MAX_GEAR_SLAYER + 1 - 12 },
		{ ITEM_CLASS_SWORD, ITEM_CLASS_CROSS, -1, MSlayerGear::MAX_GEAR_SLAYER - 12 },
		{ ITEM_CLASS_SWORD, ITEM_CLASS_MACE, -1, MSlayerGear::MAX_GEAR_SLAYER - 12 },
		{ ITEM_CLASS_HELM, ITEM_CLASS_SWORD, -1, MSlayerGear::GEAR_SLAYER_RIGHTHAND },
	};
	for (const auto& hand : hands)
	{
		Broken scene;
		scene.Be(kSlayer, false);
		scene.Put(scene.slayer, MSlayerGear::GEAR_SLAYER_LEFTHAND, hand.left, SOMEWHAT);
		scene.Put(scene.slayer, MSlayerGear::GEAR_SLAYER_RIGHTHAND, hand.right, OK);
		scene.Draw();
		scene.Body(SPRITEID_ITEM_BROKEN_SLAYER_FEMALE);
		if (hand.leftFrame >= 0) scene.Frame(1, hand.leftFrame + 17);
		scene.Frame(1, hand.rightFrame);
		CHECK(scene.Same());
	}
}

TEST(TopView3ItemBroken, OnlyTheSlotsBeforeTheLastTenAreShown)
{
	// Recorded defect: the bound "size - 4 - 6" predates the two
	// advancement slots, so a slayer's ZAP1 and ZAP2 are shown and ZAP3,
	// ZAP4 and the advancement slots are not.
	Broken scene;
	scene.Be(kSlayer, true);
	scene.Put(scene.slayer, MSlayerGear::GEAR_SLAYER_ZAP2, ITEM_CLASS_RING, SOMEWHAT);
	scene.Put(scene.slayer, MSlayerGear::GEAR_SLAYER_ZAP3, ITEM_CLASS_RING, SOMEWHAT);
	scene.Put(scene.slayer, MSlayerGear::GEAR_SLAYER_PDA, ITEM_CLASS_RING, SOMEWHAT);
	scene.Draw();
	scene.Body(SPRITEID_ITEM_BROKEN_SLAYER_MALE);
	scene.Frame(0, MSlayerGear::GEAR_SLAYER_ZAP2 + 17);
	CHECK(scene.Same());
}

TEST(TopView3ItemBroken, AFrameWithoutASpriteIsSkipped)
{
	Broken scene;
	scene.Be(kSlayer, true);
	scene.View().m_ItemBrokenFPK[0][WORD(MSlayerGear::GEAR_SLAYER_HELM)].Set(SPRITEID_NULL, 0, 0);
	scene.Put(scene.slayer, MSlayerGear::GEAR_SLAYER_HELM, ITEM_CLASS_HELM, OK);
	scene.Put(scene.slayer, MSlayerGear::GEAR_SLAYER_COAT, ITEM_CLASS_COAT, SOMEWHAT);
	scene.Draw();
	scene.Body(SPRITEID_ITEM_BROKEN_SLAYER_MALE);
	scene.Frame(0, MSlayerGear::GEAR_SLAYER_COAT + 17);
	CHECK(scene.Same());
}

TEST(TopView3ItemBroken, AVampiresGearUsesItsSexsPictureAndOffsets)
{
	for (bool male : {true, false})
	{
		Broken scene;
		scene.Be(kVampire, male);
		const int type = male ? 2 : 3;
		const int somewhat = MVampireGear::MAX_GEAR_VAMPIRE - 12;
		scene.Put(scene.vampire, MVampireGear::GEAR_VAMPIRE_COAT, ITEM_CLASS_COAT, SOMEWHAT);
		scene.Put(scene.vampire, MVampireGear::GEAR_VAMPIRE_RIGHTHAND, ITEM_CLASS_VAMPIRE_WEAPON, ALMOST);
		scene.Put(scene.vampire, MVampireGear::GEAR_VAMPIRE_RING1, ITEM_CLASS_VAMPIRE_COUPLE_RING, ALMOST);
		scene.Put(scene.vampire, MVampireGear::GEAR_VAMPIRE_AMULET4, ITEM_CLASS_VAMPIRE_AMULET, OK);
		scene.Put(scene.vampire, MVampireGear::GEAR_VAMPIRE_ZAP2, ITEM_CLASS_RING, OK);
		scene.Put(scene.vampire, MVampireGear::GEAR_VAMPIRE_ZAP3, ITEM_CLASS_RING, OK);
		scene.Draw();
		scene.Body(male ? SPRITEID_ITEM_BROKEN_VAMPIRE_MALE : SPRITEID_ITEM_BROKEN_VAMPIRE_FEMALE);
		scene.Frame(type, MVampireGear::GEAR_VAMPIRE_COAT + somewhat);
		scene.Frame(type, MVampireGear::GEAR_VAMPIRE_RIGHTHAND + 2 * somewhat);
		scene.Frame(type, MVampireGear::GEAR_VAMPIRE_RING1);
		scene.Frame(type, MVampireGear::GEAR_VAMPIRE_AMULET4);
		scene.Frame(type, MVampireGear::GEAR_VAMPIRE_ZAP2);
		CHECK(scene.Same());
	}
}

TEST(TopView3ItemBroken, AnOustersWristletHasAFrameOfItsOwn)
{
	const int somewhat = MOustersGear::MAX_GEAR_OUSTERS + 1 - 12;
	const int wristlet = MOustersGear::MAX_GEAR_OUSTERS - 12;
	for (MPlayerGear::ITEM_STATUS status : {OK, SOMEWHAT, ALMOST})
	{
		Broken scene;
		scene.Be(kOusters, false);
		scene.Put(scene.ousters, MOustersGear::GEAR_OUSTERS_RIGHTHAND, ITEM_CLASS_OUSTERS_WRISTLET, status);
		scene.Put(scene.ousters, MOustersGear::GEAR_OUSTERS_COAT, ITEM_CLASS_OUSTERS_COAT, ALMOST);
		scene.Put(scene.ousters, MOustersGear::GEAR_OUSTERS_ZAP2, ITEM_CLASS_RING, OK);
		scene.Put(scene.ousters, MOustersGear::GEAR_OUSTERS_ZAP3, ITEM_CLASS_RING, OK);
		scene.Draw();
		scene.Body(SPRITEID_ITEM_BROKEN_OUSTERS);
		scene.Frame(4, wristlet + (status == OK ? 0 : status == SOMEWHAT ? somewhat : 2 * somewhat));
		scene.Frame(4, MOustersGear::GEAR_OUSTERS_COAT + 2 * somewhat);
		scene.Frame(4, MOustersGear::GEAR_OUSTERS_ZAP2);
		CHECK(scene.Same());
	}
	Broken scene;
	scene.Be(kOusters, true);
	scene.Put(scene.ousters, MOustersGear::GEAR_OUSTERS_RIGHTHAND, ITEM_CLASS_OUSTERS_CHAKRAM, SOMEWHAT);
	scene.Draw();
	scene.Body(SPRITEID_ITEM_BROKEN_OUSTERS);
	scene.Frame(4, MOustersGear::GEAR_OUSTERS_RIGHTHAND + somewhat);
	CHECK(scene.Same());
}

//----------------------------------------------------------------------
// Items on the ground
//----------------------------------------------------------------------
namespace {

template<class Item> struct ItemProbe : Item
{
	using MItem::m_bDropping;
	using MItem::m_DropCount;
	using MItem::m_CurrentFrame;
};

// Ring type t lies on tile frame t and drops through drop frame t. Tile
// frame 0 names a sprite past the pack; 1 to 3 the mark at (3, 2),
// (-4, -3) and (45, 23); 4 is past the pack. Drop frame 1 has three
// frames, f at (f, 5 + f); 2 is past the pack. Option row n's colour
// set is 30 + 10n.
struct Ground : Scene
{
	std::vector<std::unique_ptr<MItem>> items;
	std::vector<std::unique_ptr<Creature>> bodies;
	Ground()
	{
		g_pItemTable->InitClass(ITEM_CLASS_RING, 5);
		g_pItemTable->InitClass(ITEM_CLASS_MINE, 1);
		for (int type = 0; type < 5; ++type) Row(ITEM_CLASS_RING, type);
		Row(ITEM_CLASS_MINE, 0);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MINE, 0).SetFrameID(1, 0, 0);
		g_pItemOptionTable->Init(4);
		for (int option = 0; option < 4; ++option) testfw::MutableRow(*g_pItemOptionTable, option).ColorSet = 30 + 10 * option;
		g_pClientConfig->QuestItemColorSet = 70;
		CImageFramePack& tiles = View().m_ItemTileFPK;
		tiles.Init(4);
		tiles[0].Set(9, 1, 1);
		tiles[1].Set(2, 3, 2);
		tiles[2].Set(2, -4, -3);
		tiles[3].Set(2, 45, 23);
		View().m_ItemTileISPK.Init(3);
		Mark(View().m_ItemTileISPK[2], 0x8410);
		CAnimationFramePack& drops = View().m_ItemDropFPK;
		drops.Init(2);
		drops[1].Init(3);
		for (int f = 0; f < 3; ++f) drops[1][WORD(f)].Set(1, short(f), short(5 + f));
		View().m_ItemDropISPK.Init(2);
		Mark(View().m_ItemDropISPK[1], 0x8C51);
	}
	static void Row(ITEM_CLASS itemClass, int type)
	{
		ITEMTABLE_INFO& row = testfw::MutableRow(*g_pItemTable, itemClass, type);
		row.SetFrameID(TYPE_FRAMEID(type), 0, 0);
		row.SetDropFrameID(TYPE_FRAMEID(type));
		row.ItemStyle = 0;
		row.EName = "Ring";
		row.HName = "Ring";
	}
	template<class Item = MRing> ItemProbe<Item>& Put(int type, int x = 5, int y = 12)
	{
		ItemProbe<Item>* item = new ItemProbe<Item>;
		items.emplace_back(item);
		item->SetItemType(TYPE_ITEMTYPE(type));
		item->SetID(TYPE_OBJECTID(500 + items.size()));
		item->MObject::SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
		return *item;
	}
	// A corpse of a creature of the type, which DrawCreature leaves undrawn (a ghost).
	MCorpse& Corpse(int creatureType, int x = 5, int y = 12)
	{
		MCorpse* corpse = new MCorpse;
		items.emplace_back(corpse);
		corpse->SetID(TYPE_OBJECTID(500 + items.size()));
		corpse->MObject::SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
		if (creatureType >= 0)
		{
			bodies.emplace_back(new Creature);
			Creature& body = *bodies.back();
			body.SetCreatureType(TYPE_CREATURETYPE(creatureType));
			body.SetID(TYPE_OBJECTID(900 + bodies.size()));
			body.Set(EFFECTSTATUS_GHOST);
			corpse->SetCreature(&body);
		}
		return *corpse;
	}
	~Ground()
	{
		// The bodies are the test's; a corpse would hand its own to the zone.
		for (auto& item : items)
			if (item->GetItemClass() == ITEM_CLASS_CORPSE) static_cast<MCorpse*>(item.get())->SetCreature(nullptr);
	}
	void Draw(MItem& item, int x = 20, int y = 30)
	{
		POINT point = {x, y};
		Access::DrawItem(View(), &point, &item);
		CHECK_EQ(x, int(point.x));
		CHECK_EQ(y, int(point.y));
	}
	void Tile(int x, int y, int colorSet)
	{
		CIndexSprite::SetUsingColorSet(colorSet, colorSet);
		expected.Index(x, y, View().m_ItemTileISPK[2]);
	}
	int Named() { return int(View().m_listDrawItemName.size()); }
	// The body's chat, due to fade: DrawItem fades it once it reaches the corpse.
	static void Chatting(Creature& body)
	{
		g_FrameNow = MonotonicClock::FromMillis(100000);
		body.m_OriChatColor = 0xFF000000 | RGB(200, 100, 40);
		body.m_NextChatFadeTime = g_FrameNow - MonotonicClock::Millis(1);
	}
	static bool Faded(Creature& body) { return (body.GetOriChatColor() >> 24) == DWORD(0xFF - g_pClientConfig->VALUE_CHATSTRING_FADE); }
};

bool Untouched(const MObject& object) { return SameRect(object.GetScreenRect(), 800, 600, 0, 0); }

} // namespace

TEST(TopView3Item, ARestingItemIsDrawnAtItsTileFrameInItsOptionColour)
{
	Ground scene;
	MItem& ring = scene.Put(1);
	scene.Draw(ring);
	scene.Tile(23, 32, 30);
	CHECK(scene.Same());
	CHECK(SameRect(ring.GetScreenRect(), 23, 32, 28, 33));

	scene.Clear();
	ring.AddItemOption(2);
	scene.Draw(ring);
	scene.Tile(23, 32, 50);
	CHECK(scene.Same());
	CHECK_EQ(50, CIndexSprite::GetUsingColorSet(0));
	CHECK_EQ(50, CIndexSprite::GetUsingColorSet(1));
}

TEST(TopView3Item, TheItemsPickRectIsKeptWithinItsTile)
{
	Ground scene;
	MItem& left = scene.Put(2);
	scene.Draw(left);
	CHECK(SameRect(left.GetScreenRect(), 20, 30, 21, 28));
	MItem& right = scene.Put(3);
	scene.Draw(right);
	CHECK(SameRect(right.GetScreenRect(), 65, 53, 20 + TILE_X, 30 + TILE_Y));
	scene.Tile(16, 27, 30);
	scene.Tile(65, 53, 30);
	CHECK(scene.Same());
}

TEST(TopView3Item, AQuestItemIsDrawnInItsSpecialColour)
{
	Ground scene;
	MItem& ring = scene.Put(1);
	ring.AddItemOption(2);
	ring.SetQuestFlag();
	CHECK_EQ(70, ring.GetSpecialColorItemColorset());
	scene.Draw(ring);
	scene.Tile(23, 32, 70);
	CHECK(scene.Same());
}

TEST(TopView3Item, ATileFrameOffThePackDrawsNothingAndIsNotNamed)
{
	Ground scene;
	scene.View().SetDrawItemNameList();
	MItem& ring = scene.Put(4);
	scene.Draw(ring);
	CHECK(scene.drawn.Blank());
	CHECK(Untouched(ring));
	CHECK_EQ(0, scene.Named());
}

TEST(TopView3Item, ASpriteOffThePackDrawsNothingButIsPickedAndNamed)
{
	Ground scene;
	scene.View().SetDrawItemNameList();
	MItem& ring = scene.Put(0);
	scene.Draw(ring);
	CHECK(scene.drawn.Blank());
	CHECK(SameRect(ring.GetScreenRect(), 21, 31, 21, 31));
	CHECK_EQ(1, scene.Named());
}

TEST(TopView3Item, ASelectedItemIsOnlyOutlined)
{
	Ground scene;
	MItem& ring = scene.Put(1);
	scene.View().SetSelectedItem(ring.GetID());
	scene.Draw(ring);
	// Recorded defect: BltSpriteOutline is an SDL stub, so a selected item is not drawn at all.
	CHECK(scene.drawn.Blank());
	CHECK(SameRect(ring.GetScreenRect(), 23, 32, 28, 33));
	const std::vector<OutlineEntry>& outlines = scene.View().m_SOM.GetOutlines();
	CHECK_EQ(size_t(1), outlines.size());
	if (outlines.size() == 1)
	{
		CHECK_EQ(23, outlines[0].x);
		CHECK_EQ(32, outlines[0].y);
		CHECK(outlines[0].pIndexSprite == &scene.View().m_ItemTileISPK[2]);
	}
	CHECK_EQ(0x1111, int(scene.View().m_SOMOutlineColor));
}

TEST(TopView3Item, AMineIsDrawnPlainInstalledOrNot)
{
	for (bool installed : {false, true})
	{
		Ground scene;
		ItemProbe<MMine>& mine = scene.Put<MMine>(0);
		if (installed) mine.SetInstalled();
		scene.Draw(mine);
		// Recorded defect: BltIndexSpriteColor draws plain on SDL, so an installed mine is not red.
		scene.Tile(23, 32, 30);
		CHECK(scene.Same());
	}
}

TEST(TopView3Item, AFallingItemIsDrawnAtItsDropFrameAboveTheGround)
{
	Ground scene;
	scene.View().SetDrawItemNameList();
	ItemProbe<MRing>& ring = scene.Put(1);
	ring.AddItemOption(1);
	ring.m_bDropping = TRUE;
	ring.m_CurrentFrame = 2;
	ring.m_DropCount = 3;
	CHECK(ring.GetDropHeight() != 0);
	scene.Draw(ring);
	CIndexSprite::SetUsingColorSet(40, 40);
	scene.expected.Index(22, 37 - ring.GetDropHeight(), scene.View().m_ItemDropISPK[1]);
	CHECK(scene.Same());
	CHECK(Untouched(ring));
	CHECK_EQ(1, scene.Named());
}

TEST(TopView3Item, AFallingItemOffItsDropFramesIsNeitherDrawnNorNamed)
{
	Ground scene;
	scene.View().SetDrawItemNameList();
	ItemProbe<MRing>& offPack = scene.Put(2);		// drop frame 2: past the pack
	offPack.m_bDropping = TRUE;
	scene.Draw(offPack);
	ItemProbe<MRing>& offFrames = scene.Put(1);		// frame 3 of 3
	offFrames.m_bDropping = TRUE;
	offFrames.m_CurrentFrame = 3;
	scene.Draw(offFrames);
	CHECK(scene.drawn.Blank());
	CHECK_EQ(0, scene.Named());
}

TEST(TopView3Item, AnItemIsNamedByItsDistanceFromThePlayer)
{
	Ground scene;
	MItem& ring = scene.Put(1, 5, 12);				// the player is at (8, 8)
	scene.Draw(ring);
	CHECK_EQ(0, scene.Named());
	scene.View().SetDrawItemNameList();
	scene.Draw(ring);
	CHECK_EQ(1, scene.Named());
	if (scene.Named() == 1)
	{
		const DRAWITEMNAME_NODE* node = scene.View().m_listDrawItemName.front();
		CHECK_EQ(4, node->GetDistance());
		CHECK(node->GetItem() == &ring);
	}
}

TEST(TopView3Item, TheNoItemEventHidesItems)
{
	Ground scene;
	scene.View().SetDrawItemNameList();
	scene.Event(EVENTID_METEOR, EVENTFLAG_NOT_DRAW_ITEM);
	MItem& ring = scene.Put(1);
	scene.Draw(ring);
	CHECK(scene.drawn.Blank());
	CHECK(Untouched(ring));
	CHECK_EQ(0, scene.Named());
}

TEST(TopView3Item, DarknessHidesAnItemFromAPlayerItBlinds)
{
	Ground scene;
	scene.View().SetDrawItemNameList();
	scene.zone->Dark(5, 12);
	MItem& ring = scene.Put(1);
	scene.Draw(ring);
	CHECK(scene.drawn.Blank());
	CHECK_EQ(0, scene.Named());
	// A vampire, a player in the lightness status, and zone 3001 see through it.
	scene.player->SetCreatureType(kVampire);
	scene.Draw(ring);
	scene.player->SetCreatureType(kSlayer);
	scene.player->Set(EFFECTSTATUS_LIGHTNESS);
	scene.Draw(ring);
	scene.player->Set(EFFECTSTATUS_LIGHTNESS, false);
	scene.zone->m_Info.ZoneID = 3001;
	scene.Draw(ring);
	CHECK_EQ(3, scene.Named());
	scene.Tile(23, 32, 30);
	CHECK(scene.Same());
}

TEST(TopView3Item, ACorpseShowsItsCreatureAndIsPickedByItsTile)
{
	Ground scene;
	scene.View().SetDrawItemNameList();
	MCorpse& corpse = scene.Corpse(kMonster);
	scene.Draw(corpse);
	CHECK(SameRect(corpse.GetCreature()->GetScreenRect(), 20, 30, 20 + TILE_X, 30 + TILE_Y));
	CHECK(Untouched(corpse));
	CHECK_EQ(0, scene.Named());
	// A Christmas tree and a notice board cannot be picked.
	for (int type : {482, 650})
	{
		MCorpse& fixed = scene.Corpse(type);
		scene.Draw(fixed);
		CHECK(SameRect(fixed.GetCreature()->GetScreenRect(), 20, 30, 20, 30));
	}
	// The relics and their kin keep the rect their creature drew.
	for (int type : {371, 376, 560, 563, 526, 549, 672, 673})
	{
		MCorpse& relic = scene.Corpse(type);
		scene.Draw(relic);
		CHECK(Untouched(*relic.GetCreature()));
	}
	for (int type : {370, 377, 559, 564, 525, 550, 671, 674})
	{
		MCorpse& other = scene.Corpse(type);
		scene.Draw(other);
		CHECK(SameRect(other.GetCreature()->GetScreenRect(), 20, 30, 20 + TILE_X, 30 + TILE_Y));
	}
	CHECK(scene.drawn.Blank());
}

TEST(TopView3Item, ACorpseWithoutACreatureShowsNothing)
{
	Ground scene;
	scene.View().SetDrawItemNameList();
	MCorpse& corpse = scene.Corpse(-1);
	scene.Draw(corpse);
	CHECK(scene.drawn.Blank());
	CHECK(Untouched(corpse));
	CHECK_EQ(0, scene.Named());
}

TEST(TopView3Item, ASelectedCorpseLeavesNoCreatureSelected)
{
	Ground scene;
	MCorpse& corpse = scene.Corpse(kMonster);
	scene.View().SetSelectedItem(corpse.GetID());
	scene.Draw(corpse);
	CHECK_EQ(int(OBJECTID_NULL), int(scene.View().GetSelectedCreature()));
	CHECK_EQ(int(corpse.GetID()), int(scene.View().GetSelectedItemID()));
	CHECK(SameRect(corpse.GetCreature()->GetScreenRect(), 20, 30, 20 + TILE_X, 30 + TILE_Y));
}

TEST(TopView3Item, ACorpsesChatFades)
{
	Ground scene;
	MCorpse& corpse = scene.Corpse(673);
	Creature& body = *scene.bodies.back();
	Ground::Chatting(body);
	scene.Draw(corpse);
	CHECK(Ground::Faded(body));
}

TEST(TopView3Item, DarknessHidesACorpseUnlessItIsARelicOrAnAltar)
{
	Ground scene;
	scene.zone->Dark(5, 12);
	for (int type : {526, 549, 371, 376, 560, 563, 670, 672, 673})
	{
		MCorpse& shown = scene.Corpse(type);
		Ground::Chatting(*scene.bodies.back());
		scene.Draw(shown);
		CHECK(Ground::Faded(*scene.bodies.back()));
	}
	CHECK(SameRect(scene.bodies[6]->GetScreenRect(), 20, 30, 20 + TILE_X, 30 + TILE_Y));	// 670
	for (int type : {525, 550, 370, 377, 559, 564, 669, 671, 674, 482})
	{
		MCorpse& hidden = scene.Corpse(type);
		Ground::Chatting(*scene.bodies.back());
		scene.Draw(hidden);
		CHECK(!Ground::Faded(*scene.bodies.back()));
		CHECK(Untouched(*scene.bodies.back()));
	}
}

//----------------------------------------------------------------------
// The item names
//----------------------------------------------------------------------
namespace {

struct Names : Ground
{
	PrintInfo font{};
	PrintInfo* oldFont = g_ClientPrintInfo[7];			// FONTID_ITEM, MTopView.cpp's
	Names() { g_ClientPrintInfo[7] = &font; }
	~Names() { g_ClientPrintInfo[7] = oldFont; }
	// The text nodes queued, oldest first.
	std::vector<const DRAWTEXT_NODE*> Queued()
	{
		std::vector<const DRAWTEXT_NODE*> nodes;
		MTopView::DRAWTEXT_PQ queue = View().m_pqDrawText;
		for (; !queue.empty(); queue.pop()) nodes.push_back(queue.top());
		return nodes;
	}
};

bool NodeIs(const DRAWTEXT_NODE* node, int x, int y, COLORREF color)
{
	const bool same = node->GetX() == x && node->GetY() == y && node->GetColor() == color && node->GetFont() == 7;
	if (!same) std::printf("      the text is at (%d,%d) in %08x, font %d, not (%d,%d) in %08x\n",
		node->GetX(), node->GetY(), unsigned(node->GetColor()), int(node->GetFont()), x, y, unsigned(color));
	return same;
}

} // namespace

TEST(TopView3ItemNames, NamesAreRightAlignedInRowsColouredByTheirOptions)
{
	Names scene;
	const char* name = scene.Put(1).GetName();
	CHECK(name != nullptr);
	// The text system measures headless (a fallback width without fonts).
	const int width = g_GetStringWidth(name, nullptr);
	CHECK(width > 0);
	const COLORREF colors[] = {RGB(1, 2, 3), RGB(4, 5, 6), RGB(7, 8, 9), RGB(10, 11, 12)};
	for (int options = 0; options < 4; ++options)
	{
		MItem& ring = scene.Put(1);
		for (int option = 0; option < options; ++option) ring.AddItemOption(TYPE_ITEM_OPTION(1 + option));
		scene.View().AddItemName(new DRAWITEMNAME_NODE(DWORD(options), &ring));
	}
	scene.View().DrawItemNameList();
	const std::vector<const DRAWTEXT_NODE*> nodes = scene.Queued();
	CHECK_EQ(size_t(4), nodes.size());
	int row = 0;
	for (const DRAWITEMNAME_NODE* item : scene.View().m_listDrawItemName)
	{
		const int x = 96 - width, y = 160 + 14 * row;
		if (size_t(row) < nodes.size())
		{
			CHECK(NodeIs(nodes[size_t(row)], x, y + 1, colors[row]));
			CHECK(nodes[size_t(row)]->IsExistBox());
			CHECK(SameRect(nodes[size_t(row)]->GetBox(), x, y, 96, y + 14));
			CHECK_EQ(int(scene.View().m_ColorUIBoxBg), int(nodes[size_t(row)]->GetBoxColor()));
		}
		CHECK(item->IsPointInRect(x, y));
		CHECK(item->IsPointInRect(95, y + 13));
		CHECK(!item->IsPointInRect(x - 1, y));
		CHECK(!item->IsPointInRect(96, y));
		CHECK(!item->IsPointInRect(x, y + 14));
		++row;
	}
	CHECK_EQ(4, row);
}

TEST(TopView3ItemNames, ASelectedNameIsShadowedWithoutABox)
{
	Names scene;
	MItem& ring = scene.Put(1);
	scene.View().AddItemName(new DRAWITEMNAME_NODE(1, &ring));
	scene.View().SetSelectedItem(ring.GetID());
	scene.View().DrawItemNameList();
	const int x = 96 - g_GetStringWidth(ring.GetName(), nullptr);
	const std::vector<const DRAWTEXT_NODE*> nodes = scene.Queued();
	CHECK_EQ(size_t(2), nodes.size());
	if (nodes.size() == 2)
	{
		CHECK(NodeIs(nodes[0], x, 161, 0));
		CHECK(NodeIs(nodes[1], x + 1, 162, RGB(1, 2, 3)));
		CHECK(!nodes[0]->IsExistBox());
		CHECK(!nodes[1]->IsExistBox());
	}
	CHECK(scene.View().m_listDrawItemName.front()->IsPointInRect(x, 160));
}

TEST(TopView3ItemNames, ASpecialItemsNameTakesItsColourSet)
{
	Names scene;
	MItem& ring = scene.Put(1);
	ring.AddItemOption(1);
	ring.SetQuestFlag();
	scene.View().AddItemName(new DRAWITEMNAME_NODE(1, &ring));
	scene.View().DrawItemNameList();
	const WORD shade = CIndexSprite::ColorSet[70][16];
	const COLORREF color = RGB(CSDLGraphics::Red(shade) << 3, CSDLGraphics::Green(shade) << 3, CSDLGraphics::Blue(shade) << 3);
	const std::vector<const DRAWTEXT_NODE*> nodes = scene.Queued();
	CHECK_EQ(size_t(1), nodes.size());
	if (!nodes.empty()) CHECK(NodeIs(nodes[0], 96 - g_GetStringWidth(ring.GetName(), nullptr), 161, color));
}

//----------------------------------------------------------------------
// The shadows of corpses
//----------------------------------------------------------------------
namespace {

// Creature types whose sprite type 0 is body frame 1; its stand and die
// frames cast shadow 0 at (5, 6) in every direction. 360 to 740 are
// vampires, 741 a slayer.
const int kSlayerBody = 741;

struct Remains : Ground
{
	Remains()
	{
		CREATURESPRITETABLE_INFO& sprite = testfw::MutableRow(sprites, 0);
		sprite.FrameID = 1;
		sprite.CreatureType = BYTE(FLAG_CREATURESPRITE_MONSTER);
		for (int type = 360; type <= kSlayerBody; ++type)
		{
			CREATURETABLE_INFO& row = testfw::MutableRow(creatureTypes, type);
			row.SetCreatureTribe(type == kSlayerBody ? CREATURETRIBE_SLAYER : CREATURETRIBE_VAMPIRE, ACTION_MAX_SLAYER);
			row.SpriteTypes.Init(1);
			*row.SpriteTypes.GetMutable(0) = 0;
		}
		CCreatureFramePack& frames = View().m_CreatureShadowFPK;
		frames.Init(2);
		for (int id = 0; id < 2; ++id)
		{
			frames[WORD(id)].Init(BYTE(ACTION_MAX_SLAYER));
			for (int action = 0; action < ACTION_MAX_SLAYER; ++action) frames[WORD(id)][BYTE(action)].Init(8);
		}
		for (int action : {int(ACTION_STAND), int(ACTION_DIE)})
			for (int direction = 0; direction < 8; ++direction)
			{
				FRAME_ARRAY& shadows = frames[1][BYTE(action)][BYTE(direction)];
				shadows.Init(8);
				for (int f = 0; f < 8; ++f) shadows[WORD(f)].Set(0, 5, 6);
			}
		View().m_CreatureSSPK.Init(1);
		Shade(View().m_CreatureSSPK[0], 4, 3);
		// The teen version's dust and cross shadows.
		View().m_EtcSPK.Init(SPRITEID_SLAYER_CROSS_SHADOW + 1);
		for (int sprite = 0; sprite <= SPRITEID_SLAYER_CROSS_SHADOW; ++sprite)
			Solid(View().m_EtcSPK[WORD(sprite)], 3, 2, WORD(0xF800 | sprite));
	}
	// A corpse whose creature lies at frame `count` of an 8-frame death.
	MCorpse& Body(int type, int count, bool dead = true)
	{
		MCorpse& corpse = Corpse(type);
		Creature& body = *bodies.back();
		body.Set(EFFECTSTATUS_GHOST, false);
		body.m_Action = ACTION_DIE;
		body.m_ActionCountMax = 8;
		body.m_ActionCount = BYTE(count);
		if (!dead) body.m_bAlive = true;
		else body.m_bAlive = false;
		return corpse;
	}
	void Shadow(MItem& item)
	{
		POINT point = {20, 30};
		Access::DrawItemShadow(View(), &point, &item);
		CHECK_EQ(20, int(point.x));
		CHECK_EQ(30, int(point.y));
	}
	// What gameview draws for the corpse's creature, on the expected canvas.
	void Expect(MCorpse& corpse)
	{
		CreatureDrawPacks packs = Access::Packs(View());
		packs.pSurface = &expected.surface;
		POINT point = {20, 30};
		DrawCreatureShadow(packs, &point, corpse.GetCreature(), g_pUserOption->BlendingShadow != FALSE);
	}
};

} // namespace

TEST(TopView3ItemShadow, AFallingCorpseCastsItsCreaturesShadow)
{
	Remains scene;
	MCorpse& corpse = scene.Body(700, 3);
	scene.Shadow(corpse);
	scene.Expect(corpse);
	CHECK(scene.Same());
}

TEST(TopView3ItemShadow, ALyingCorpseCastsNoneUnlessGoreIsOff)
{
	Remains scene;
	MCorpse& corpse = scene.Body(kSlayerBody, 8);
	scene.Shadow(corpse);
	CHECK(scene.drawn.Blank());
	scene.Expect(corpse);
	CHECK(!scene.expected.Blank());				// gameview would have drawn it
	// The teen version shows a dead slayer's cross.
	g_pUserInformation->GoreLevel = false;
	scene.Clear();
	scene.Shadow(corpse);
	scene.Expect(corpse);
	CHECK(scene.Same());
	MCorpse& living = scene.Body(kSlayerBody, 8, false);
	scene.Clear();
	scene.Shadow(living);
	CHECK(scene.drawn.Blank());
	scene.Expect(living);
	CHECK(!scene.expected.Blank());
}

TEST(TopView3ItemShadow, RelicsTreesBoardsAndWayPointsAlwaysCastOne)
{
	for (int type : {371, 376, 560, 563, 526, 549, 482, 650, 672, 673, 730, 731, 732})
	{
		Remains scene;
		// gameview draws a dead way point no shadow
		MCorpse& corpse = scene.Body(type, 8, type < 730);
		scene.Shadow(corpse);
		scene.Expect(corpse);
		CHECK(scene.Same());
	}
	for (int type : {370, 377, 559, 564, 525, 550, 481, 483, 649, 651, 671, 674, 729, 733})
	{
		Remains scene;
		MCorpse& corpse = scene.Body(type, 8);
		scene.Shadow(corpse);
		CHECK(scene.drawn.Blank());
	}
}

TEST(TopView3ItemShadow, ASelectedCorpseDecidesAlikeAndLeavesNoCreatureSelected)
{
	for (int count : {3, 8})
	{
		Remains scene;
		MCorpse& corpse = scene.Body(700, count);
		scene.View().SetSelectedItem(corpse.GetID());
		scene.Shadow(corpse);
		if (count == 3)
		{
			scene.Expect(corpse);
			CHECK(scene.Same());
		}
		else CHECK(scene.drawn.Blank());
		CHECK_EQ(int(OBJECTID_NULL), int(scene.View().GetSelectedCreature()));
	}
}

TEST(TopView3ItemShadow, OnlyCorpsesInTheLightCastOne)
{
	Remains scene;
	scene.Shadow(scene.Put(1));
	scene.Shadow(scene.Corpse(-1));
	CHECK(scene.drawn.Blank());
	scene.zone->Dark(5, 12);
	scene.Shadow(scene.Body(482, 3));
	CHECK(scene.drawn.Blank());
	scene.player->SetCreatureType(kVampire);
	MCorpse& seen = scene.Body(700, 3);
	scene.Shadow(seen);
	scene.Expect(seen);
	CHECK(scene.Same());
}
