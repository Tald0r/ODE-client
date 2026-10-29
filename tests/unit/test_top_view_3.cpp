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
#include "FontHandleUtil.h"
#include "MGameStringTable.h"
#include "ModifyStatusManager.h"
#include "MParty.h"
#include "DXLib/CDirectSound.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>

extern RECT g_GameRect;
extern int g_FrameCount;
extern PrintInfo* g_ClientPrintInfo[10];		// MAX_FONTID, MTopView.cpp's
// MItemUse.cpp's item factory, which the executable installs as MItem's host.
MItem*	NewItemOfClass(ITEM_CLASS itemClass);

// The view's world drawing (task 4.260 moved it into gameclient): the
// party HP bar, items, image objects, effects, the broken-gear picture,
// burrowed creatures, the alpha box, the screen fade, and the two
// endings. Each picture test draws through the view onto one surface and
// lays the blits it should make onto another; the two must match. The
// surfaces are grey, so a shadow, a half blend and a screen blend show.
//
// The screen fade, the alpha box and the endings' fades blend through
// BltColorAlpha, which Blended below restates. SpriteLib's SDL backend
// stubs several other blits a test here meets (each pinned where it
// shows): BltSpriteOutline draws nothing, so a selected item or burrow is
// not drawn at all; SetClipRight is a no-op, so a party HP bar is never
// cut; BltSpriteAlphaFilter draws plain. BltSpriteHalf
// is not reached at all: its CPU fallback reads a CSprite's (null) 565
// pixel buffer, so a half image object is undefined behaviour headless.
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
	static int DrawChatString(MTopView& v, POINT* p, MCreature* c, COLORREF color) { return v.DrawChatString(p, c, color); }
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

// What BltColorAlpha makes of `under`: per 5:6:5 channel colour * (31 -
// alpha2) / 32 + under * alpha2 / 32, each share rounded down, so alpha2
// is the 32nds of what lies under that are kept.
WORD Blended(WORD colour, WORD under, int alpha2)
{
	const auto channel = [&](int shift, int mask) {
		return (((colour >> shift) & mask) * (31 - alpha2) >> 5) + (((under >> shift) & mask) * alpha2 >> 5);
	};
	return WORD(channel(11, 0x1F) << 11 | channel(5, 0x3F) << 5 | channel(0, 0x1F));
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
	// The colour blended over `rect` as BltColorAlpha does.
	void ColorAlpha(const RECT& rect, WORD colour, int alpha2)
	{
		DWORD pitch = 0;
		unsigned char* base = static_cast<unsigned char*>(surface.Lock(nullptr, &pitch));
		CHECK(base != nullptr);
		if (base == nullptr) return;
		for (int y = int(rect.top); y < int(rect.bottom); ++y)
			for (int x = int(rect.left); x < int(rect.right); ++x)
			{
				WORD& pixel = reinterpret_cast<WORD*>(base + y * pitch)[x];
				pixel = Blended(colour, pixel, alpha2);
			}
		surface.Unlock();
	}
	bool Blank() { for (WORD pixel : Picture()) if (pixel != kPaper) return false; return true; }
	void Sprite(int x, int y, CSprite& sprite) { POINT p = {x, y}; surface.BltSprite(&p, &sprite); }
	void Index(int x, int y, CIndexSprite& sprite) { POINT p = {x, y}; surface.BltIndexSprite(&p, &sprite); }
	void Shadow(int x, int y, CShadowSprite& sprite) { POINT p = {x, y}; surface.BltShadowSpriteDarkness(&p, &sprite, 1); }
	void Black(int x, int y, CShadowSprite& sprite) { POINT p = {x, y}; surface.BltShadowSprite(&p, &sprite); }
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
	using MCreature::m_Action;
	using MCreature::m_ActionCount;
	using MCreatureWear::m_Addon;
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
		// The colour sets are fixed tables; filled here so an index sprite is not black when run alone.
		CIndexSprite::SetColorSet();
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
	// Darkness, a light filter of every range and a sighted player, so a
	// lit frame adds one light (m_nLight) to the light buffer.
	void Lit()
	{
		View().m_LightFTP.Init(4);
		for (int n = 0; n < 4; ++n) View().m_LightFTP[TYPE_FILTERID(n)].Init(2, 2);
		View().m_LightBufferFilter.Init(64, 64);
		View().SetDarkBits(1);
		player->SetSight(13);
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
	// Note: the caller's point is moved, not a copy of it (its one caller
	// sets it afresh).
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

TEST(TopView3PartyHP, AFullBarAtTheTopOfThirtyOneBitsIsFull)
{
	// The server's HP is 32 bits (GCOtherModifyInfo). At x = -15 a full bar's
	// edge is 5, drawn; the share overflowing an int put it at -5, not drawn.
	PartyBar scene;
	scene.member.HP(0x7FFFFFFF, 0x7FFFFFFF);
	scene.Draw(-15, 4);
	scene.Background(-5, 4);
	scene.Foreground(-5, 4);
	CHECK(scene.Same());

	// Past 2^31 both read negative as ints: a negative maximum is full, as
	// under a name, where the unclamped HP's share wrapped to 0.
	scene.Clear();
	scene.member.HP(INT_MIN, -1);
	scene.Draw(-15, 4);
	scene.Background(-5, 4);
	scene.Foreground(-5, 4);
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
	// The box keeps 16/32 of what is under it, and nothing outside changes.
	scene.expected.ColorAlpha(RECT{4, 4, 40, 40}, CSDLGraphics::Color(200, 100, 50), 16);
	CHECK(scene.Same());
}

TEST(TopView3AlphaBox, LeavesALockedSurfaceLocked)
{
	Scene scene;
	RECT rect = {4, 4, 40, 40};
	scene.drawn.surface.Lock();
	Access::DrawAlphaBox(scene.View(), &rect, 200, 100, 50, 16);
	CHECK(scene.drawn.surface.IsLock());
	scene.drawn.surface.Unlock();
	scene.expected.ColorAlpha(RECT{4, 4, 40, 40}, CSDLGraphics::Color(200, 100, 50), 16);
	CHECK(scene.Same());
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
	// Both draws laid the white over the screen at 20, keeping 20/32.
	scene.expected.ColorAlpha(g_GameRect, CSDLGraphics::Color(255, 255, 255), 20);
	scene.expected.ColorAlpha(g_GameRect, CSDLGraphics::Color(255, 255, 255), 20);
	CHECK(scene.Same());
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
	static constexpr int kID = EFFECTSPRITETYPE_BLOODY_BIBLE_IN_GEAR;
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
	using Gear::m_ItemSlot;
	using Gear::m_pItemStatus;
	using Gear::m_nBrokenItem;
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
	CHECK(!scene.drawn.surface.IsLock());		// before the picture is read, which locks
	scene.Body(SPRITEID_ITEM_BROKEN_SLAYER_MALE);
	for (int k : {0, 19, 39, 6}) scene.Frame(0, k);
	CHECK(scene.Same());
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
	using Item::m_bDropping;
	using Item::m_DropCount;
	using Item::m_CurrentFrame;
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
	// The tile tinted to one channel, as an installed mine is drawn (rgb 0, red).
	void TintedTile(int x, int y, BYTE rgb)
	{
		POINT p = {x, y};
		expected.surface.BltIndexSpriteColor(&p, &View().m_ItemTileISPK[2], rgb);
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

TEST(TopView3Item, AnInstalledMineIsDrawnRedAndAnotherPlain)
{
	std::vector<WORD> pictures[2];
	for (bool installed : {false, true})
	{
		Ground scene;
		ItemProbe<MMine>& mine = scene.Put<MMine>(0);
		if (installed) mine.SetInstalled();
		scene.Draw(mine);
		if (installed) scene.TintedTile(23, 32, 0);
		else scene.Tile(23, 32, 30);
		CHECK(scene.Same());
		pictures[installed] = scene.drawn.Picture();
	}
	// The tint shows: drawn plain, as the backend used to, the two would match.
	CHECK(pictures[0] != pictures[1]);
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
	CHECK(shade != 0);									// a colour, not the unfilled table's black
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

TEST(TopView3ItemShadow, AnUnselectedCorpseLeavesTheCreatureSelectedAlone)
{
	Remains scene;
	MCorpse& corpse = scene.Body(700, 3);
	scene.View().SetSelectedCreature(77);
	scene.Shadow(corpse);
	scene.Expect(corpse);
	CHECK(scene.Same());
	CHECK_EQ(77, int(scene.View().GetSelectedCreature()));
}

TEST(TopView3Item, ASelectedCorpsesBodyIsDrawnAsTheSelectedCreature)
{
	// The body is drawn as if it were the selected creature, which names it.
	// Type 702 is a monster's: its sprite type (2) is past a slayer's.
	Remains scene;
	scene.sprites.Init(3);
	for (int type = 0; type < 3; ++type)
	{
		CREATURESPRITETABLE_INFO& sprite = testfw::MutableRow(scene.sprites, type);
		sprite.FrameID = 1;
		sprite.CreatureType = BYTE(FLAG_CREATURESPRITE_MONSTER);
	}
	*testfw::MutableRow(scene.creatureTypes, 702).SpriteTypes.GetMutable(0) = 2;
	CCreatureFramePack& frames = scene.View().m_CreatureFPK;
	frames.Init(2);
	for (int id = 0; id < 2; ++id)
	{
		frames[WORD(id)].Init(BYTE(ACTION_MAX_SLAYER));
		for (int action = 0; action < ACTION_MAX_SLAYER; ++action) frames[WORD(id)][BYTE(action)].Init(8);
	}
	for (int direction = 0; direction < 8; ++direction)
	{
		FRAME_ARRAY& bodies = frames[1][BYTE(ACTION_DIE)][BYTE(direction)];
		bodies.Init(8);
		for (int f = 0; f < 8; ++f) bodies[WORD(f)].Set(0, 1, 1);
	}
	scene.View().m_CreatureSPK.Init(1);
	Mark(scene.View().m_CreatureSPK[0], 0x3333);
	MCorpse& corpse = scene.Body(702, 3);
	scene.Draw(corpse);
	CHECK(scene.View().m_pSelectedCreature == nullptr);
	scene.View().SetSelectedItem(corpse.GetID());
	scene.Draw(corpse);
	CHECK(scene.View().m_pSelectedCreature == corpse.GetCreature());
	CHECK_EQ(int(OBJECTID_NULL), int(scene.View().GetSelectedCreature()));
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

//----------------------------------------------------------------------
// Image objects
//----------------------------------------------------------------------
namespace {

// Still sprites 0 to 3 of their own sizes and colours; shadows 0 and 1.
// Animation 1 has frame 0 (sprite 1 at (10, 2)) and frame 1 (sprite 2
// at (12, 4)); animation 2 names a sprite past the pack. Shadow
// animation 1: frame 0 is shadow 0 at (-2, 5), frame 1 none. Alpha and
// screen effect 2: frame 0 at (7, 1) and (6, 2); the alpha frame 1 is
// sprite 1 at (9, 3), the screen frame 1 has none.
struct Scenery : Scene
{
	std::vector<std::unique_ptr<MImageObject>> objects;
	CFilter sentinel;										// a filter none of the view's
	Scenery()
	{
		CSpritePack& sprites = View().m_ImageObjectSPK;
		sprites.Init(4);
		Solid(sprites[0], 8, 6, 0xF800);
		Solid(sprites[1], 4, 4, 0x07E0);
		Solid(sprites[2], 6, 2, 0x001F);
		Solid(sprites[3], 3, 3, 0xFFE0);
		View().m_ImageObjectSSPK.Init(2);
		Shade(View().m_ImageObjectSSPK[0], 5, 3);
		Shade(View().m_ImageObjectSSPK[1], 3, 2);
		View().m_ImageObjectFilter.Init(4, 4);
		CAnimationFramePack& frames = View().m_ImageObjectFPK;
		frames.Init(3);
		frames[1].Init(2);
		frames[1][0].Set(1, 10, 2);
		frames[1][1].Set(2, 12, 4);
		frames[2].Init(1);
		frames[2][0].Set(7, 1, 1);
		CAnimationFramePack& shadows = View().m_ImageObjectShadowFPK;
		shadows.Init(2);
		shadows[1].Init(2);
		shadows[1][0].Set(0, -2, 5);
		shadows[1][1].Set(SPRITEID_NULL, 0, 0);
		CEffectFramePack& alpha = View().m_EffectAlphaFPK;
		alpha.Init(3);
		alpha[2].Init(1);
		alpha[2][0].Init(2);
		alpha[2][0][0].Set(0, 7, 1, 0, false);
		alpha[2][0][1].Set(1, 9, 3, 0, false);
		View().m_EffectAlphaSPK.Init(2);
		PalSprite(View().m_EffectAlphaSPK[0], 3, 2, 0x10);
		PalSprite(View().m_EffectAlphaSPK[1], 2, 2, 0x20);
		View().m_EffectAlphaPPK.Init(3);
		Palette(View().m_EffectAlphaPPK[2], 0x07FF);
		CEffectFramePack& screen = View().m_EffectScreenFPK;
		screen.Init(3);
		screen[2].Init(1);
		screen[2][0].Init(2);
		screen[2][0][0].Set(0, 6, 2, 0, false);
		screen[2][0][1].Set(SPRITEID_NULL, 0, 0, 0, false);
		View().m_EffectScreenSPK.Init(1);
		PalSprite(View().m_EffectScreenSPK[0], 3, 3, 0);
		View().m_EffectScreenPPK.Init(3);
		Palette(View().m_EffectScreenPPK[2], 0xF81F);
	}
	MImageObject& Still(int sprite, BYTE trans = 0, int viewpoint = 9, int pixelX = 0)
	{
		objects.emplace_back(new MImageObject(1, 1, TYPE_SPRITEID(sprite), pixelX, 0, TYPE_SECTORPOSITION(viewpoint), trans));
		return *objects.back();
	}
	MAnimationObject& Animated(int sprite, BYTE bltType, int frameID, int frameCount)
	{
		MAnimationObject* object = new MAnimationObject(2, 2, TYPE_SPRITEID(sprite), 0, 0, 9, false, bltType);
		object->SetFrameID(TYPE_FRAMEID(frameID), BYTE(frameCount));
		objects.emplace_back(object);
		return *object;
	}
	POINT Draw(MImageObject& object, int x = 20, int y = 30)
	{
		POINT point = {x, y};
		Access::DrawImageObject(View(), &point, &object);
		return point;
	}
	void Whole(int x, int y, int sprite) { expected.Sprite(x, y, View().m_ImageObjectSPK[WORD(sprite)]); }
	void Shadow(int x, int y, int sprite) { expected.Shadow(x, y, View().m_ImageObjectSSPK[WORD(sprite)]); }
};

bool At(POINT point, int x, int y)
{
	if (point.x != x || point.y != y) std::printf("      the point is (%d,%d), not (%d,%d)\n", int(point.x), int(point.y), x, y);
	return point.x == x && point.y == y;
}

} // namespace

TEST(TopView3ImageObject, AShadowObjectCastsItsShadowBlended)
{
	Scenery scene;
	MShadowObject shadow(3, 3, 1, 0, 0, 9, false);
	CHECK(At(scene.Draw(shadow), 20, 30));
	scene.Shadow(20, 30, 1);
	CHECK(scene.Same());
	scene.Clear();
	MShadowObject none(3, 3, SPRITEID_NULL, 0, 0, 9, false);
	scene.Draw(none);
	CHECK(scene.drawn.Blank());
}

TEST(TopView3ImageObject, AShadowAnimationObjectDrawsNothing)
{
	Scenery scene;
	MShadowAnimationObject shadow(3, 3, 0, 0, 0, 9, false);
	shadow.SetFrameID(1, 2);
	g_bFrameChanged = true;
	CHECK(At(scene.Draw(shadow), 20, 30));
	CHECK(scene.drawn.Blank());
	CHECK_EQ(0, int(shadow.GetFrame()));
}

TEST(TopView3ImageObject, AStillObjectIsDrawnWhole)
{
	Scenery scene;
	CHECK(At(scene.Draw(scene.Still(0)), 20, 30));
	scene.Whole(20, 30, 0);
	CHECK(scene.Same());
	scene.Clear();
	scene.Draw(scene.Still(SPRITEID_NULL));
	CHECK(scene.drawn.Blank());
}

TEST(TopView3ImageObject, AWallBeforeThePlayerIsSeenThroughItsFilter)
{
	// Translucent, at or below the player's row (8), and not a wall or
	// one the player is behind: the image object filter is set over it,
	// and the alpha filter blit (which SDL draws plain, a recorded defect)
	// or the plain one draws it whole.
	Scenery scene;
	scene.View().m_FilterPosition = {20, 30};
	CSprite::SetFilter(&scene.sentinel);
	scene.Draw(scene.Still(0, OBJECT_TRANS_FLAG, 8));
	scene.Whole(20, 30, 0);
	CHECK(scene.Same());
	CHECK(CSprite::GetFilter() == &scene.View().m_ImageObjectFilter);
	MImageObject& wall = scene.Still(0, OBJECT_TRANS_FLAG, 9, 0);
	wall.SetWall(MImageObject::WALL_RIGHTDOWN);			// the player (8 - 8) is past 0 - 9
	CSprite::SetFilter(&scene.sentinel);
	scene.Draw(wall);
	CHECK(CSprite::GetFilter() == &scene.View().m_ImageObjectFilter);
}

TEST(TopView3ImageObject, AnObjectThatHidesNothingIsDrawnWithoutTheFilter)
{
	Scenery scene;
	scene.View().m_FilterPosition = {20, 30};
	MImageObject& above = scene.Still(0, OBJECT_TRANS_FLAG, 7);	// above the player's row
	MImageObject& wall = scene.Still(0, OBJECT_TRANS_FLAG, 9, 10 * TILE_X);
	wall.SetWall(MImageObject::WALL_RIGHTDOWN);			// the player (8 - 8) is not past 10 - 9
	MImageObject& solid = scene.Still(0, 0, 8);
	for (MImageObject* object : {&above, &wall, &solid})
	{
		scene.Clear();
		CSprite::SetFilter(&scene.sentinel);
		scene.Draw(*object);
		CHECK(CSprite::GetFilter() == &scene.sentinel);
		scene.Whole(20, 30, 0);
		CHECK(scene.Same());
	}
}

TEST(TopView3ImageObject, AnAnimatedObjectDrawsItsStillSpriteWhereItsFrameGoes)
{
	Scenery scene;
	MAnimationObject& torch = scene.Animated(0, BLT_NORMAL, 1, 2);
	g_CurrentFrame = 3;									// animation frame 3 % 2
	g_bFrameChanged = true;
	CSprite::SetFilter(&scene.sentinel);
	const POINT point = scene.Draw(torch);
	CHECK(CSprite::GetFilter() == &scene.sentinel);		// drawn plain, without the filter
	// Note: the caller's point is moved to the frame's offset (every caller
	// sets it afresh).
	CHECK(At(point, 32, 34));
	scene.Whole(20, 30, 0);
	scene.Shadow(18, 35, 0);								// the shadow of the object's own frame, 0
	// Recorded defect: the still sprite is drawn again where frame 1's
	// sprite 2 belongs, so the object does not animate.
	scene.Whole(32, 34, 0);
	CHECK(scene.Same());
	CHECK_EQ(0, int(torch.GetFrame()));						// a normal animation is not stepped here
}

TEST(TopView3ImageObject, AnAnimationWithoutAStillSpriteShowsItsFrameThroughTheFilter)
{
	Scenery scene;
	MAnimationObject& torch = scene.Animated(SPRITEID_NULL, BLT_NORMAL, 1, 2);
	g_CurrentFrame = 2;
	CSprite::SetFilter(&scene.sentinel);
	CHECK(At(scene.Draw(torch), 30, 32));
	scene.Shadow(18, 35, 0);
	scene.Whole(30, 32, 1);
	CHECK(scene.Same());
	CHECK(CSprite::GetFilter() == &scene.View().m_ImageObjectFilter);
}

TEST(TopView3ImageObject, ATranslucentAnimationShowsItsFrame)
{
	Scenery scene;
	scene.View().m_FilterPosition = {20, 30};
	MAnimationObject& torch = scene.Animated(0, BLT_NORMAL, 1, 2);
	torch.SetTrans();
	g_CurrentFrame = 3;
	scene.Draw(torch);
	scene.Whole(20, 30, 0);
	scene.Shadow(18, 35, 0);
	scene.Whole(32, 34, 2);
	CHECK(scene.Same());
}

TEST(TopView3ImageObject, AFrameSpriteOffThePackShowsOnlyTheStillSprite)
{
	for (int sprite : {7, 4})							// far past the pack, and just past it
	{
		Scenery scene;
		scene.View().m_ImageObjectFPK[2][0].Set(TYPE_SPRITEID(sprite), 1, 1);
		MAnimationObject& torch = scene.Animated(0, BLT_NORMAL, 2, 1);
		CHECK(At(scene.Draw(torch), 20, 30));
		scene.Whole(20, 30, 0);							// shadow animation 2 is past its pack
		CHECK(scene.Same());
	}
}

TEST(TopView3ImageObject, TheShadowFollowsTheObjectsOwnFrame)
{
	Scenery scene;
	MAnimationObject& torch = scene.Animated(SPRITEID_NULL, BLT_NORMAL, 1, 3);
	torch.NextFrame();
	torch.NextFrame();									// frame 2: past the shadow's two frames
	g_CurrentFrame = 2;
	scene.Draw(torch);
	scene.Whole(30, 32, 1);
	CHECK(scene.Same());
	torch.SetFrameID(1, 2);
	torch.NextFrame();									// frame 1: no shadow sprite
	scene.Clear();
	scene.Draw(torch);
	scene.Whole(30, 32, 1);
	CHECK(scene.Same());
}

TEST(TopView3ImageObject, AnAlphaAnimationStepsOnAChangedFrame)
{
	Scenery scene;
	MAnimationObject& glow = scene.Animated(3, BLT_EFFECT, 2, 2);
	g_bFrameChanged = true;
	CHECK(At(scene.Draw(glow), 27, 31));
	scene.Whole(20, 30, 3);
	scene.expected.Alpha(27, 31, scene.View().m_EffectAlphaSPK[0], scene.View().m_EffectAlphaPPK[2]);
	CHECK(scene.Same());
	CHECK_EQ(1, int(glow.GetFrame()));
	scene.Clear();
	scene.Draw(glow);
	scene.Whole(20, 30, 3);
	scene.expected.Alpha(29, 33, scene.View().m_EffectAlphaSPK[1], scene.View().m_EffectAlphaPPK[2]);
	CHECK(scene.Same());
	CHECK_EQ(0, int(glow.GetFrame()));
	g_bFrameChanged = false;
	scene.Draw(glow);
	CHECK_EQ(0, int(glow.GetFrame()));
}

TEST(TopView3ImageObject, AnAlphaOrScreenAnimationAddsItsFramesLight)
{
	Scenery scene;
	scene.Lit();
	scene.View().m_EffectAlphaFPK[2][0][0].Set(0, 7, 1, 2, false);
	scene.View().m_EffectScreenFPK[2][0][0].Set(0, 6, 2, 2, false);
	scene.Draw(scene.Animated(SPRITEID_NULL, BLT_NORMAL, 1, 2));
	CHECK_EQ(0, scene.View().m_nLight);
	scene.Draw(scene.Animated(SPRITEID_NULL, BLT_EFFECT, 2, 2));
	CHECK_EQ(1, scene.View().m_nLight);
	scene.Draw(scene.Animated(SPRITEID_NULL, BLT_SCREEN, 2, 2));
	CHECK_EQ(2, scene.View().m_nLight);
}

TEST(TopView3ImageObject, AnAlphaAnimationOffThePackStillSteps)
{
	Scenery scene;
	MAnimationObject& glow = scene.Animated(SPRITEID_NULL, BLT_EFFECT, 5, 2);
	g_bFrameChanged = true;
	CHECK(At(scene.Draw(glow), 20, 30));
	CHECK(scene.drawn.Blank());
	CHECK_EQ(1, int(glow.GetFrame()));
}

TEST(TopView3ImageObject, AScreenAnimationStepsOnlyOnItsPack)
{
	Scenery scene;
	MAnimationObject& flare = scene.Animated(SPRITEID_NULL, BLT_SCREEN, 2, 2);
	g_bFrameChanged = true;
	CHECK(At(scene.Draw(flare), 26, 32));
	scene.expected.Screen(26, 32, scene.View().m_EffectScreenSPK[0], scene.View().m_EffectScreenPPK[2]);
	CHECK(scene.Same());
	CHECK_EQ(1, int(flare.GetFrame()));
	scene.Clear();
	CHECK(At(scene.Draw(flare), 20, 30));				// frame 1 has no sprite
	CHECK(scene.drawn.Blank());
	CHECK_EQ(0, int(flare.GetFrame()));
	MAnimationObject& off = scene.Animated(SPRITEID_NULL, BLT_SCREEN, 5, 2);
	scene.Draw(off);
	CHECK(scene.drawn.Blank());
	CHECK_EQ(0, int(off.GetFrame()));
}

//----------------------------------------------------------------------
// Effects
//----------------------------------------------------------------------
namespace {

// For each kind: frame ID 1's frame 0 is sprite 0 at an offset of its
// own, its frame 1 has none; frame ID 2's sprite is past the pack (the
// shadow's frame ID 2 names an unloaded sprite, 3 one past the pack);
// the alpha and screen frame ID 3 has no palette.
struct Sparks : Scene
{
	const MEffectHost* savedHost = MEffect::SetHost(nullptr);
	int savedBasisX = MScreenEffect::m_ScreenBasisX, savedBasisY = MScreenEffect::m_ScreenBasisY;
	std::vector<std::unique_ptr<MEffect>> effects;
	Sparks()
	{
		Kind(View().m_EffectNormalFPK, 4, 5);
		Kind(View().m_EffectAlphaFPK, 3, 6);
		Kind(View().m_EffectScreenFPK, 5, 2);
		Kind(View().m_EffectShadowFPK, 2, 3);
		View().m_EffectShadowFPK[2][0][0].Set(1, 0, 0, 0, false);
		View().m_EffectShadowFPK[3][0][0].Set(9, 0, 0, 0, false);
		View().m_EffectNormalSPK.Init(2);
		Solid(View().m_EffectNormalSPK[0], 5, 4, 0x7BEF);
		View().m_EffectAlphaSPK.Init(1);
		PalSprite(View().m_EffectAlphaSPK[0], 4, 3, 0x10);
		View().m_EffectScreenSPK.Init(1);
		PalSprite(View().m_EffectScreenSPK[0], 4, 3, 0);
		View().m_EffectShadowSPK.Init(2);
		Shade(View().m_EffectShadowSPK[0], 4, 2);
		View().m_EffectAlphaPPK.Init(3);
		View().m_EffectScreenPPK.Init(3);
		for (int id : {1, 2})
		{
			Palette(View().m_EffectAlphaPPK[WORD(id)], 0x07E0);
			Palette(View().m_EffectScreenPPK[WORD(id)], 0xF81F);
		}
		MScreenEffect::SetScreenBasis(0, 0);
	}
	~Sparks()
	{
		MScreenEffect::SetScreenBasis(savedBasisX, savedBasisY);
		MEffect::SetHost(savedHost);
	}
	static void Kind(CEffectFramePack& frames, int cx, int cy)
	{
		frames.Init(4);
		for (WORD id = 1; id < 4; ++id)
		{
			frames[id].Init(1);
			frames[id][0].Init(2);
			frames[id][0][0].Set(TYPE_SPRITEID(id == 2 ? 9 : 0), short(cx), short(cy), 0, false);
			frames[id][0][1].Set(SPRITEID_NULL, 0, 0, 0, false);
		}
	}
	template<class Effect = MSelectableEffect> Effect& Make(BYTE bltType, int frameID, int frame = 0)
	{
		Effect* effect = new Effect(bltType);
		effect->SetFrameID(TYPE_FRAMEID(frameID), 2);
		for (int f = 0; f < frame; ++f) effect->NextFrame();
		effects.emplace_back(effect);
		return *effect;
	}
	void Draw(MEffect* effect, bool selectable)
	{
		POINT point = {20, 30};
		Access::DrawEffect(View(), &point, effect, selectable);
		CHECK(At(point, 20, 30));
	}
	// What the kind's frame 0 at (x, y) draws on the expected canvas.
	void Expect(BYTE bltType, int x, int y)
	{
		switch (bltType)
		{
			case BLT_NORMAL: expected.Sprite(x, y, View().m_EffectNormalSPK[0]); break;
			case BLT_EFFECT: expected.Alpha(x, y, View().m_EffectAlphaSPK[0], View().m_EffectAlphaPPK[1]); break;
			case BLT_SCREEN: expected.Screen(x, y, View().m_EffectScreenSPK[0], View().m_EffectScreenPPK[1]); break;
			default: expected.Shadow(x, y, View().m_EffectShadowSPK[0]); break;
		}
	}
};

void Preset(MObject& object) { RECT rect = {1, 2, 3, 4}; object.SetScreenRect(&rect); }
bool Stale(const MObject& object) { return SameRect(object.GetScreenRect(), 1, 2, 3, 4); }

} // namespace

TEST(TopView3Effect, EachKindDrawsItsFrameAtItsOffset)
{
	Sparks scene;
	MEffect& normal = scene.Make(BLT_NORMAL, 1);
	scene.Draw(&normal, true);
	scene.Expect(BLT_NORMAL, 24, 35);
	CHECK(scene.Same());
	CHECK(SameRect(normal.GetScreenRect(), 24, 35, 29, 39));

	scene.Clear();
	MEffect& alpha = scene.Make(BLT_EFFECT, 1);
	scene.Draw(&alpha, true);
	scene.Expect(BLT_EFFECT, 23, 36);
	CHECK(scene.Same());
	CHECK(SameRect(alpha.GetScreenRect(), 23, 36, 27, 39));

	scene.Clear();
	MEffect& screen = scene.Make(BLT_SCREEN, 1);
	scene.Draw(&screen, true);
	scene.Expect(BLT_SCREEN, 25, 32);
	CHECK(scene.Same());
	CHECK(SameRect(screen.GetScreenRect(), 25, 32, 29, 35));

	scene.Clear();
	MEffect& shadow = scene.Make(BLT_SHADOW, 1);
	scene.Draw(&shadow, true);
	scene.Expect(BLT_SHADOW, 22, 33);
	CHECK(scene.Same());
	CHECK(Untouched(shadow));							// a shadow is never picked
}

TEST(TopView3Effect, AnUnselectableEffectKeepsItsRect)
{
	for (BLT_TYPE bltType : {BLT_NORMAL, BLT_EFFECT, BLT_SCREEN})
	{
		Sparks scene;
		MEffect& effect = scene.Make(BYTE(bltType), 1);
		Preset(effect);
		scene.Draw(&effect, false);
		CHECK(!scene.drawn.Blank());
		CHECK(Stale(effect));
	}
}

TEST(TopView3Effect, ANormalEffectWithoutAFrameOrSpriteIsUnpicked)
{
	const struct { int frameID, frame; } cases[] = { {5, 0}, {1, 1}, {2, 0} };	// past the pack, no sprite, sprite past its pack
	for (const auto& c : cases)
	{
		Sparks scene;
		MEffect& effect = scene.Make(BLT_NORMAL, c.frameID, c.frame);
		Preset(effect);
		scene.Draw(&effect, true);
		CHECK(scene.drawn.Blank());
		CHECK(Untouched(effect));
	}
}

TEST(TopView3Effect, AnAlphaOrScreenEffectWithoutAFrameKeepsAStaleRect)
{
	// Recorded defect: unlike a normal effect, these leave the last
	// frame's pick rect, so an effect that is not drawn can be picked.
	const struct { int frameID, frame; } cases[] = { {5, 0}, {1, 1}, {2, 0}, {3, 0} };	// ... and no palette
	for (BLT_TYPE bltType : {BLT_EFFECT, BLT_SCREEN})
		for (const auto& c : cases)
		{
			Sparks scene;
			MEffect& effect = scene.Make(BYTE(bltType), c.frameID, c.frame);
			Preset(effect);
			scene.Draw(&effect, true);
			CHECK(scene.drawn.Blank());
			CHECK(Stale(effect));
		}
}

TEST(TopView3Effect, AShadowEffectNeedsALoadedSprite)
{
	const struct { int frameID, frame; } cases[] = { {5, 0}, {1, 1}, {2, 0}, {3, 0} };	// ... and an unloaded sprite
	for (const auto& c : cases)
	{
		Sparks scene;
		MEffect& effect = scene.Make(BLT_SHADOW, c.frameID, c.frame);
		scene.Draw(&effect, true);
		CHECK(scene.drawn.Blank());
	}
}

TEST(TopView3Effect, TheNoEffectEventHidesEffects)
{
	Sparks scene;
	scene.Event(EVENTID_METEOR, EVENTFLAG_NOT_DRAW_EFFECT);
	MEffect& effect = scene.Make(BLT_NORMAL, 1);
	Preset(effect);
	scene.Draw(&effect, true);
	scene.Draw(nullptr, true);
	CHECK(scene.drawn.Blank());
	CHECK(Stale(effect));
}

TEST(TopView3Effect, NoEffectDrawsNothing)
{
	Sparks scene;
	scene.Draw(nullptr, true);
	CHECK(scene.drawn.Blank());
}

TEST(TopView3Effect, AnAlphaScreenOrShadowFrameAddsItsLight)
{
	Sparks scene;
	scene.Lit();
	scene.View().m_EffectNormalFPK[1][0][0].Set(0, 4, 5, 2, false);
	scene.View().m_EffectAlphaFPK[1][0][0].Set(0, 3, 6, 2, false);
	scene.View().m_EffectScreenFPK[1][0][0].Set(0, 5, 2, 2, false);
	scene.View().m_EffectShadowFPK[1][0][0].Set(0, 2, 3, 2, false);
	scene.Draw(&scene.Make(BLT_NORMAL, 1), true);
	CHECK_EQ(0, scene.View().m_nLight);					// a normal effect gives none
	scene.Draw(&scene.Make(BLT_EFFECT, 1), true);
	CHECK_EQ(1, scene.View().m_nLight);
	scene.Draw(&scene.Make(BLT_SCREEN, 1), true);
	CHECK_EQ(2, scene.View().m_nLight);
	scene.Draw(&scene.Make(BLT_SHADOW, 1), true);
	CHECK_EQ(3, scene.View().m_nLight);
	scene.View().m_EffectAlphaFPK[1][0][0].Set(0, 3, 6, 0, false);	// an unlit frame
	scene.Draw(&scene.Make(BLT_EFFECT, 1), true);
	CHECK_EQ(3, scene.View().m_nLight);
}

TEST(TopView3Effects, ZoneEffectsAreDrawnAtTheirPixelLessTheViewsOriginAndScreenEffectsWhereTheyAre)
{
	Sparks scene;
	scene.View().m_FirstZonePixel = {60, 40};
	MEffect& zoneEffect = scene.Make(BLT_NORMAL, 1);
	zoneEffect.SetPixelPosition(84, 75, 10);			// (24, 25) on the screen
	MScreenEffect& screenEffect = scene.Make<MScreenEffect>(BLT_EFFECT, 1);
	screenEffect.SetScreenPosition(10, 12);
	MEffect& third = scene.Make(BLT_SCREEN, 1);
	EFFECT_LIST list{&zoneEffect, &screenEffect, &third};
	POINT point = {1, 1};
	Access::DrawEffects(scene.View(), &point, list.begin(), 2);
	scene.Expect(BLT_NORMAL, 28, 30);
	scene.Expect(BLT_EFFECT, 13, 18);
	CHECK(scene.Same());
	CHECK(SameRect(zoneEffect.GetScreenRect(), 28, 30, 33, 34));	// selectable: it is picked
	CHECK(Untouched(screenEffect));								// not selectable
	CHECK(Untouched(third));									// past the count
}

TEST(TopView3Effects, AnEffectStillDelayedIsSkipped)
{
	Sparks scene;
	MEffectHost host;
	host.CurrentFrame = +[]() -> DWORD { return 10; };
	MEffect::SetHost(&host);
	MEffect& waiting = scene.Make(BLT_NORMAL, 1);
	waiting.SetDelayFrame(5);
	MEffect& shown = scene.Make(BLT_EFFECT, 1);
	shown.SetPixelPosition(30, 40, 0);
	EFFECT_LIST list{&waiting, &shown};
	POINT point = {0, 0};
	Access::DrawEffects(scene.View(), &point, list.begin(), 2);
	scene.Expect(BLT_EFFECT, 33, 46);
	CHECK(scene.Same());
	CHECK(Untouched(waiting));
	MEffect::SetHost(nullptr);
}

TEST(TopView3Effects, TheNoEffectEventHidesThemAll)
{
	Sparks scene;
	scene.Event(EVENTID_METEOR, EVENTFLAG_NOT_DRAW_EFFECT);
	MEffect& effect = scene.Make(BLT_NORMAL, 1);
	EFFECT_LIST list{&effect};
	POINT point = {0, 0};
	Access::DrawEffects(scene.View(), &point, list.begin(), 1);
	CHECK(scene.drawn.Blank());
}

TEST(TopView3GroundEffect, GroundEffectsAreDrawnAtTheirPixelUnlessSkipped)
{
	Sparks scene;
	scene.View().m_FirstZonePixel = {60, 40};
	MEffect& mark = scene.Make(BLT_NORMAL, 1);
	mark.SetPixelPosition(84, 75, 10);
	MEffect& skipped = scene.Make(BLT_EFFECT, 1);
	skipped.SetPixelPosition(70, 50, 0);
	skipped.SetDrawSkip(true);
	MEffect& pool = scene.Make<MEffect>(BLT_EFFECT, 1);
	pool.SetPixelPosition(100, 90, 4);					// (40, 46) on the screen
	for (MEffect* effect : {&mark, &skipped, &pool}) scene.zone->m_mapGroundEffect[effect->GetID()] = effect;
	scene.View().SetSelectedEffect(mark.GetID());			// selected, with no target to show
	Access::DrawGroundEffect(scene.View());
	scene.Expect(BLT_NORMAL, 28, 30);
	scene.Expect(BLT_EFFECT, 43, 52);
	CHECK(scene.Same());
	CHECK(SameRect(mark.GetScreenRect(), 28, 30, 33, 34));
	CHECK(Untouched(skipped));
	CHECK(Untouched(pool));
}

//----------------------------------------------------------------------
// Effects attached to a creature
//----------------------------------------------------------------------
namespace {

// A tested frame ID has eight directions of eight frames, empty until a
// test places one; frame ID f's palette is a colour of its own. Alpha
// sprite 0 is half transparent, 1 opaque; the screen and normal sprites
// are 3 x 2.
struct Aura : Scene
{
	static constexpr int kTurretBullet = EFFECTSPRITETYPE_INSTALL_TURRET_BULLET - MAX_EFFECTSPRITETYPE_ALPHAEFFECT;
	Creature wearer;
	std::vector<std::unique_ptr<MAttachEffect>> effects;
	ATTACHEFFECT_LIST list;
	Aura()
	{
		const int ids = (std::max)({int(EFFECTSPRITETYPE_INSTALL_TURRET_FIRE1), int(EFFECTSPRITETYPE_INSTALL_TURRET_FIRE2), kTurretBullet}) + 1;
		View().m_EffectAlphaFPK.Init(TYPE_FRAMEID(ids));
		View().m_EffectScreenFPK.Init(8);
		View().m_EffectNormalFPK.Init(8);
		for (int id : {1, 2, 3, 4, int(EFFECTSPRITETYPE_INSTALL_TURRET_FIRE1), int(EFFECTSPRITETYPE_INSTALL_TURRET_FIRE2), kTurretBullet})
			Shape(View().m_EffectAlphaFPK, id);
		for (int id = 1; id < 8; ++id)
		{
			Shape(View().m_EffectScreenFPK, id);
			Shape(View().m_EffectNormalFPK, id);
		}
		View().m_EffectAlphaSPK.Init(2);
		PalSprite(View().m_EffectAlphaSPK[0], 3, 2, 0x10);
		PalSprite(View().m_EffectAlphaSPK[1], 3, 2, 0x20);
		View().m_EffectScreenSPK.Init(1);
		PalSprite(View().m_EffectScreenSPK[0], 3, 2, 0);
		View().m_EffectNormalSPK.Init(1);
		Solid(View().m_EffectNormalSPK[0], 3, 2, 0x7BEF);
		View().m_EffectAlphaPPK.Init(WORD(ids));
		View().m_EffectScreenPPK.Init(8);
		for (int id = 0; id < ids; ++id) Palette(View().m_EffectAlphaPPK[WORD(id)], WORD(0x0841 * (1 + id % 7) | 0x8000));
		for (int id = 0; id < 8; ++id) Palette(View().m_EffectScreenPPK[WORD(id)], WORD(0x1082 * (1 + id)));
		actionEffects.Init(ACTIONEFFECTSPRITETYPE_EXTREME_FEMALE + 1);
		for (int aest : {1, 2, int(ACTIONEFFECTSPRITETYPE_EXTREME_MALE)})
		{
			ACTION_FRAMEID_ARRAY& actions = testfw::MutableRow(actionEffects, aest);
			actions.Init(256);
			for (int action = 0; action < 256; ++action) testfw::MutableRow(actions, action).FrameID = FRAMEID_NULL;
		}
		wearer.SetCreatureType(kSlayer);
		wearer.m_Action = ACTION_STAND;
		wearer.m_ActionCountMax = 100;
		wearer.SetCurrentDirection(0);
	}
	static void Shape(CEffectFramePack& frames, int id)
	{
		frames[WORD(id)].Init(8);
		for (int direction = 0; direction < 8; ++direction)
		{
			frames[WORD(id)][BYTE(direction)].Init(8);
			for (int f = 0; f < 8; ++f) frames[WORD(id)][BYTE(direction)][WORD(f)].Set(SPRITEID_NULL, 0, 0, 0, false);
		}
	}
	// Frame f of (id, direction) is the sprite at (cx, cy), behind or in front.
	static void Place(CEffectFramePack& frames, int id, int direction, int f, int sprite, int cx, int cy, bool back = false)
	{
		frames[WORD(id)][BYTE(direction)][WORD(f)].Set(TYPE_SPRITEID(sprite), short(cx), short(cy), 0, back);
	}
	void Alpha(int id, int direction, int f, int sprite, int cx, int cy, bool back = false) { Place(View().m_EffectAlphaFPK, id, direction, f, sprite, cx, cy, back); }
	EFFECTSPRITETYPETABLE_INFO& Type(int est) { return testfw::MutableRow(effectSprites, est); }
	void Act(int aest, int action, int frameID) { testfw::MutableRow(actionEffects, aest, action).FrameID = TYPE_FRAMEID(frameID); }
	template<class Effect = MAttachEffect> Effect& Attach(Effect* effect, BYTE bltType, int frameID, int maxFrame, int frame = 0, int direction = 0)
	{
		effect->SetBltType(bltType);
		effect->SetFrameID(TYPE_FRAMEID(frameID), BYTE(maxFrame));
		for (int f = 0; f < frame; ++f) effect->NextFrame();
		effect->SetDirection(BYTE(direction));
		effects.emplace_back(effect);
		list.push_back(effect);
		return *effect;
	}
	MAttachEffect& Make(int est, BYTE bltType, int frameID, int maxFrame = 8, int frame = 0, int direction = 0)
	{
		return Attach(new MAttachEffect(TYPE_EFFECTSPRITETYPE(est), 0), bltType, frameID, maxFrame, frame, direction);
	}
	void Draw(int type = 0, MCreature* creature = nullptr)
	{
		POINT point = {20, 20};
		Access::DrawAttachEffect(View(), &point, list.begin(), BYTE(list.size()), creature != nullptr ? creature : &wearer, type);
		CHECK(At(point, 20, 20));
	}
	void ExpectAlpha(int x, int y, int sprite, int id) { expected.Alpha(20 + x, 20 + y, View().m_EffectAlphaSPK[WORD(sprite)], View().m_EffectAlphaPPK[WORD(id)]); }
	// The attached alpha frame, which is drawn twice (recorded defect).
	void ExpectAttached(int x, int y, int sprite, int id) { ExpectAlpha(x, y, sprite, id); ExpectAlpha(x, y, sprite, id); }
	void ExpectScreen(int x, int y, int id) { expected.Screen(20 + x, 20 + y, View().m_EffectScreenSPK[0], View().m_EffectScreenPPK[WORD(id)]); }
	void ExpectNormal(int x, int y) { expected.Sprite(20 + x, 20 + y, View().m_EffectNormalSPK[0]); }
};

} // namespace

TEST(TopView3AttachEffect, NothingIsDrawnForNoCreature)
{
	Aura scene;
	scene.Alpha(1, 0, 0, 1, 4, 5);
	scene.Make(10, BLT_EFFECT, 1);
	POINT point = {20, 20};
	Access::DrawAttachEffect(scene.View(), &point, scene.list.begin(), 1, nullptr, 0);
	CHECK(scene.drawn.Blank());
}

TEST(TopView3AttachEffect, AnAlphaFrameIsDrawnTwice)
{
	Aura scene;
	scene.Alpha(1, 0, 0, 0, 4, 5);
	scene.Make(10, BLT_EFFECT, 1);
	scene.Draw();
	scene.ExpectAlpha(4, 5, 0, 1);
	// Recorded defect: once by the expanded blit, once by DRAW_ALPHASPRITEPAL
	// in the 2D branch, so a translucent effect shows darker than drawn once.
	CHECK(scene.drawn.Picture() != scene.expected.Picture());
	scene.ExpectAlpha(4, 5, 0, 1);
	CHECK(scene.Same());
}

TEST(TopView3AttachEffect, AColourEffectIsSkippedAndTheNextDrawn)
{
	Aura scene;
	scene.Alpha(1, 0, 0, 1, 4, 5);
	scene.Alpha(1, 2, 0, 1, 9, 1);
	scene.Make(10, BLT_EFFECT, 1).SetEffectColor(7);
	scene.Make(10, BLT_EFFECT, 1, 8, 0, 2);
	scene.Draw();
	scene.ExpectAttached(9, 1, 1, 1);
	CHECK(scene.Same());
}

TEST(TopView3AttachEffect, TheEffectsDirectionAndFrameChooseTheFrameUpToItsLast)
{
	Aura scene;
	scene.Alpha(1, 3, 2, 1, 4, 5);
	scene.Make(10, BLT_EFFECT, 1, 8, 2, 3);
	scene.Draw();
	scene.ExpectAttached(4, 5, 1, 1);
	CHECK(scene.Same());
	// A frame past the direction's last shows its last.
	scene.View().m_EffectAlphaFPK[1][3].Init(3);
	scene.Alpha(1, 3, 2, 1, 7, 8);
	scene.list.clear();
	scene.Make(10, BLT_EFFECT, 1, 8, 6, 3);
	scene.Clear();
	scene.Draw();
	scene.ExpectAttached(7, 8, 1, 1);
	CHECK(scene.Same());
}

TEST(TopView3AttachEffect, ADrawTypeDrawsTheFramesBehindOrInFront)
{
	for (bool back : {false, true})
		for (int type : {0, 1, 2})
		{
			Aura scene;
			scene.Alpha(1, 0, 0, 1, 4, 5, back);
			scene.Make(10, BLT_EFFECT, 1);
			scene.Draw(type);
			if (type == 0 || (type == 1) == !back)
			{
				scene.ExpectAttached(4, 5, 1, 1);
				CHECK(scene.Same());
			}
			else CHECK(scene.drawn.Blank());
		}
}

TEST(TopView3AttachEffect, ATypePastTheTableIsNotDrawn)
{
	Aura scene;
	scene.Alpha(1, 0, 0, 1, 4, 5);
	scene.Make(MAX_EFFECTSPRITETYPE, BLT_EFFECT, 1);
	scene.Draw();
	CHECK(scene.drawn.Blank());
}

TEST(TopView3AttachEffect, PairFramesAreDrawnAfterOrBeforeTheFrame)
{
	for (bool pairBack : {false, true})
	{
		Aura scene;
		scene.Alpha(1, 0, 3, 1, 4, 5);
		scene.View().m_EffectAlphaFPK[2][0].Init(2);			// the pair has two frames: 3 % 2 is 1
		scene.Alpha(2, 0, 1, 1, 4, 5);
		scene.Type(11).PairFrameIDList = {2};
		scene.Type(11).bPairFrameBack = pairBack;
		scene.Make(11, BLT_EFFECT, 1, 8, 3);
		scene.Draw();
		if (pairBack) scene.ExpectAlpha(4, 5, 1, 2);
		scene.ExpectAttached(4, 5, 1, 1);
		if (!pairBack) scene.ExpectAlpha(4, 5, 1, 2);
		CHECK(scene.Same());
	}
}

TEST(TopView3AttachEffect, APairFrameKeepsToItsOwnSide)
{
	for (bool pairBack : {false, true})
	{
		Aura scene;
		scene.Alpha(1, 0, 0, 1, 4, 5);
		scene.Alpha(2, 0, 0, 1, 9, 9, true);					// the pair's frame is behind
		scene.Type(11).PairFrameIDList = {2};
		scene.Type(11).bPairFrameBack = pairBack;
		scene.Make(11, BLT_EFFECT, 1);
		scene.Draw(1);
		scene.ExpectAttached(4, 5, 1, 1);
		CHECK(scene.Same());
		scene.Clear();
		scene.Draw(2);
		scene.ExpectAlpha(9, 9, 1, 2);
		CHECK(scene.Same());
	}
}

TEST(TopView3AttachEffect, AnActionsFrameReplacesTheEffectsInTheCreaturesDirection)
{
	Aura scene;
	scene.Type(12).ActionEffectFrameID = 1;
	scene.Act(1, ACTION_ATTACK, 3);
	scene.Act(1, ACTION_MAGIC, 3);
	scene.Act(1, ACTION_MOVE, 3);
	for (int f = 0; f < 8; ++f) scene.Alpha(3, 5, f, 1, 2 * f, 3);
	scene.Make(12, BLT_EFFECT, 1, 6);
	scene.wearer.SetCurrentDirection(5);
	const struct { int action, count, move, frame; } cases[] = {
		{ ACTION_ATTACK, 2, 0, 2 },
		{ ACTION_ATTACK, 9, 0, 5 },							// up to the effect's last frame
		{ ACTION_ADVANCEMENT_MAGIC_ATTACK, 4, 0, 4 },		// an advanced magic attack is magic
		{ ACTION_ADVANCEMENT_SLAYER_MAGIC_ATTACK, 1, 0, 1 },
		{ ACTION_ADVANCEMENT_OUSTERS_MAGIC_ATTACK, 3, 0, 3 },
		{ ACTION_MOVE, 4, 1, 1 },							// a move counts its moves
	};
	for (const auto& c : cases)
	{
		scene.wearer.m_Action = BYTE(c.action);
		scene.wearer.m_ActionCount = BYTE(c.count);
		scene.wearer.m_MoveCount = BYTE(c.move);
		scene.Clear();
		scene.Draw();
		scene.ExpectAttached(2 * c.frame, 3, 1, 3);
		CHECK(scene.Same());
	}
}

TEST(TopView3AttachEffect, AnActionWithoutAFrameStillTurnsTheEffect)
{
	Aura scene;
	scene.Type(12).ActionEffectFrameID = 1;
	scene.Alpha(1, 5, 2, 1, 6, 6);
	scene.Make(12, BLT_EFFECT, 1, 8, 2);
	scene.wearer.SetCurrentDirection(5);
	scene.Draw();
	scene.ExpectAttached(6, 6, 1, 1);
	CHECK(scene.Same());
}

TEST(TopView3AttachEffect, AnExtremeEffectNeedsAnActionFrameAndAPlainCreature)
{
	Aura scene;
	const int extreme = ACTIONEFFECTSPRITETYPE_EXTREME_MALE;
	scene.Type(13).ActionEffectFrameID = TYPE_FRAMEID(extreme);
	scene.Alpha(1, 0, 0, 1, 1, 1);
	scene.Alpha(3, 0, 0, 1, 6, 6);
	scene.Make(13, BLT_EFFECT, 1);
	scene.Draw();										// standing has no frame: skipped
	CHECK(scene.drawn.Blank());
	scene.Act(extreme, ACTION_STAND, 3);
	scene.Draw();
	scene.ExpectAttached(6, 6, 1, 3);
	CHECK(scene.Same());
	for (int type : {CREATURETYPE_BAT, CREATURETYPE_WOLF, CREATURETYPE_WER_WOLF})
	{
		Creature beast;
		beast.SetCreatureType(TYPE_CREATURETYPE(type));
		beast.m_Action = ACTION_STAND;
		beast.SetCurrentDirection(0);					// the direction with the frame
		scene.Clear();
		scene.Draw(0, &beast);
		CHECK(scene.drawn.Blank());
	}
}

TEST(TopView3AttachEffect, ASwordTrailShowsOnlyOnItsSwordActions)
{
	Aura scene;
	for (int f = 0; f < 8; ++f) scene.Alpha(1, 0, f, 1, 2 * f, 2);
	scene.Make(EFFECTSPRITETYPE_LAR_SLASH_MALE_FAST, BLT_EFFECT, 1, 6);
	const struct { int action; bool shown; } larSlash[] = {
		{ ACTION_SLAYER_SWORD_2, true }, { ACTION_SLAYER_SWORD_2_SLOW, true }, { ACTION_SLAYER_SWORD_2_FAST, true },
		{ ACTION_SLAYER_SWORD, false }, { ACTION_STAND, false },
	};
	for (const auto& c : larSlash)
	{
		scene.wearer.m_Action = BYTE(c.action);
		scene.wearer.m_ActionCount = 9;					// the trail's frame: up to the effect's last
		scene.Clear();
		scene.Draw();
		if (c.shown)
		{
			scene.ExpectAttached(10, 2, 1, 1);
			CHECK(scene.Same());
		}
		else CHECK(scene.drawn.Blank());
	}
	scene.list.clear();
	scene.Make(EFFECTSPRITETYPE_REDIANCE_MALE_FAST, BLT_EFFECT, 1, 6);
	for (int action : {int(ACTION_SLAYER_SWORD), int(ACTION_SLAYER_SWORD_FAST), int(ACTION_SLAYER_SWORD_SLOW), int(ACTION_SLAYER_SWORD_2)})
	{
		scene.wearer.m_Action = BYTE(action);
		scene.wearer.m_ActionCount = 3;
		scene.Clear();
		scene.Draw();
		scene.ExpectAttached(6, 2, 1, 1);
		CHECK(scene.Same());
	}
	scene.wearer.m_Action = ACTION_STAND;
	scene.Clear();
	scene.Draw();
	CHECK(scene.drawn.Blank());
}

TEST(TopView3AttachEffect, AnInstalledTurretShowsOnlyItsFireAndShells)
{
	Aura scene;
	scene.wearer.Set(EFFECTSTATUS_INSTALL_TURRET);
	const int ids[] = {1, int(EFFECTSPRITETYPE_INSTALL_TURRET_FIRE1), int(EFFECTSPRITETYPE_INSTALL_TURRET_FIRE2), Aura::kTurretBullet};
	for (int i = 0; i < 4; ++i) scene.Alpha(ids[i], 0, 0, 1, 2 + i * 7, 3);	// each at its own spot
	for (int id : ids) scene.Make(10, BLT_EFFECT, id);
	scene.Draw();
	for (int i = 1; i < 4; ++i) scene.ExpectAttached(2 + i * 7, 3, 1, ids[i]);
	CHECK(scene.Same());
}

TEST(TopView3AttachEffect, ANormalEffectIsDrawnOnceWithItsPairs)
{
	Aura scene;
	Aura::Place(scene.View().m_EffectNormalFPK, 5, 2, 1, 0, 3, 4);
	Aura::Place(scene.View().m_EffectNormalFPK, 6, 2, 1, 0, 12, 9);
	scene.Type(14).PairFrameIDList = {6};
	scene.Make(14, BLT_NORMAL, 5, 8, 1, 2);
	scene.Draw(2);										// a normal effect ignores the draw type
	scene.ExpectNormal(3, 4);
	scene.ExpectNormal(12, 9);
	CHECK(scene.Same());
	// A type past the table draws the frame but not its pairs.
	scene.list.clear();
	scene.Make(MAX_EFFECTSPRITETYPE, BLT_NORMAL, 5, 8, 1, 2);
	scene.Clear();
	scene.Draw();
	scene.ExpectNormal(3, 4);
	CHECK(scene.Same());
}

TEST(TopView3AttachEffect, AScreenEffectIsDrawnOnceThenItsPairs)
{
	Aura scene;
	Aura::Place(scene.View().m_EffectScreenFPK, 1, 0, 0, 0, 4, 5);
	Aura::Place(scene.View().m_EffectScreenFPK, 2, 0, 0, 0, 4, 5);
	Aura::Place(scene.View().m_EffectScreenFPK, 3, 0, 0, 0, 9, 9, true);
	scene.Type(15).PairFrameIDList = {2, 3};
	scene.Make(15, BLT_SCREEN, 1);
	scene.Draw();
	scene.ExpectScreen(4, 5, 1);
	scene.ExpectScreen(4, 5, 2);
	scene.ExpectScreen(9, 9, 3);
	CHECK(scene.Same());
	scene.Clear();
	scene.Draw(2);										// only the frame behind
	scene.ExpectScreen(9, 9, 3);
	CHECK(scene.Same());
}

TEST(TopView3AttachEffect, AScreenEffectsActionFrameCountsOnlyWhenItIsAnother)
{
	Aura scene;
	scene.Type(16).ActionEffectFrameID = 2;
	scene.Act(2, ACTION_STAND, 1);						// its own frame: the effect's frame stays
	scene.Act(2, ACTION_ATTACK, 3);
	for (int f = 0; f < 8; ++f)
	{
		Aura::Place(scene.View().m_EffectScreenFPK, 1, 4, f, 0, 2 * f, 1);
		Aura::Place(scene.View().m_EffectScreenFPK, 3, 4, f, 0, 2 * f, 7);
	}
	scene.Make(16, BLT_SCREEN, 1, 8, 2);
	scene.wearer.SetCurrentDirection(4);
	scene.wearer.m_ActionCount = 5;
	scene.Draw();
	scene.ExpectScreen(4, 1, 1);
	CHECK(scene.Same());
	scene.wearer.m_Action = ACTION_ATTACK;
	scene.Clear();
	scene.Draw();
	scene.ExpectScreen(10, 7, 3);
	CHECK(scene.Same());
}

TEST(TopView3AttachEffect, EachDrawnAlphaOrScreenFrameAddsItsLight)
{
	Aura scene;
	scene.Lit();
	scene.View().m_EffectAlphaFPK[1][0][0].Set(1, 4, 5, 2, false);
	scene.View().m_EffectAlphaFPK[2][0][0].Set(1, 9, 9, 2, true);
	scene.View().m_EffectScreenFPK[1][0][0].Set(0, 4, 5, 2, false);
	scene.View().m_EffectScreenFPK[2][0][0].Set(0, 9, 9, 2, false);
	scene.View().m_EffectNormalFPK[1][0][0].Set(0, 4, 5, 2, false);
	scene.Type(11).PairFrameIDList = {2};
	scene.Type(17).PairFrameIDList = {2};
	scene.Type(17).bPairFrameBack = true;
	scene.Make(10, BLT_NORMAL, 1);
	scene.Draw();
	CHECK_EQ(0, scene.View().m_nLight);
	const struct { int est; BYTE bltType; int type, lights; } cases[] = {
		{ 11, BLT_EFFECT, 0, 2 },		// the frame and its pair after
		{ 17, BLT_EFFECT, 0, 2 },		// the pair before
		{ 11, BLT_EFFECT, 1, 1 },		// the pair (behind) not drawn
		{ 11, BLT_SCREEN, 0, 2 },
	};
	for (const auto& c : cases)
	{
		scene.list.clear();
		scene.View().m_nLight = 0;
		scene.Make(c.est, c.bltType, 1);
		scene.Draw(c.type);
		CHECK_EQ(c.lights, scene.View().m_nLight);
	}
}

TEST(TopView3AttachEffect, APairFramesLightIsAddedLikeItsFrames)
{
	// Lights of 1 a cell, on a buffer of 10: the frame's and its pair's
	// both add to the same cells.
	Aura scene;
	scene.Lit();
	MTopView& view = scene.View();
	for (int n = 0; n < 4; ++n)
		for (int u = 0; u < 2; ++u)
			for (int v = 0; v < 2; ++v) view.m_LightFTP[TYPE_FILTERID(n)].SetFilter(WORD(u), WORD(v), 1);
	for (int y = 0; y < 64; ++y)
		for (int x = 0; x < 64; ++x) view.m_LightBufferFilter.SetFilter(WORD(x), WORD(y), 10);
	view.m_EffectAlphaFPK[1][0][0].Set(1, 4, 5, 2, false);
	view.m_EffectAlphaFPK[2][0][0].Set(1, 9, 9, 2, true);
	scene.Type(11).PairFrameIDList = {2};
	view.m_nLight = 0;
	scene.Make(11, BLT_EFFECT, 1);
	scene.Draw();
	CHECK_EQ(2, view.m_nLight);
	int twelve = 0, ten = 0;
	for (int y = 0; y < 64; ++y)
		for (int x = 0; x < 64; ++x)
		{
			const int cell = view.m_LightBufferFilter.GetFilter(WORD(y))[x];
			if (cell == 12) ++twelve;
			else if (cell == 10) ++ten;
		}
	CHECK_EQ(4, twelve);
	CHECK_EQ(64 * 64 - 4, ten);
}

TEST(TopView3AttachEffect, AScreenFramePastTheDirectionsLastShowsItsLast)
{
	Aura scene;
	scene.View().m_EffectScreenFPK[1][0].Init(2);
	Aura::Place(scene.View().m_EffectScreenFPK, 1, 0, 0, 0, 4, 5);
	Aura::Place(scene.View().m_EffectScreenFPK, 1, 0, 1, 0, 6, 7);
	scene.Make(15, BLT_SCREEN, 1, 8, 5);
	scene.Draw();
	scene.ExpectScreen(6, 7, 1);
	CHECK(scene.Same());
}

namespace {

// The orbit paths are built from the math tables; both are put back.
struct Orbits : Aura
{
	std::vector<int> sine, cosine, tangent;
	Orbits()
		: sine(MathTable::FSinTab, MathTable::FSinTab + MathTable::MAX_ANGLE),
		  cosine(MathTable::FCosTab, MathTable::FCosTab + MathTable::MAX_ANGLE),
		  tangent(MathTable::FArcTanTab, MathTable::FArcTanTab + MathTable::MAX_ANGLE + 1)
	{
		MathTable::FCreateSines();
		EffectOrbit::InitializePositions();
	}
	~Orbits()
	{
		std::copy(sine.begin(), sine.end(), MathTable::FSinTab);
		std::copy(cosine.begin(), cosine.end(), MathTable::FCosTab);
		std::copy(tangent.begin(), tangent.end(), MathTable::FArcTanTab);
		EffectOrbit::InitializePositions();
	}
	// The orbit step farthest above (y < 0) or below (y > 0).
	int Step(bool above)
	{
		MAttachOrbitEffect probe(10, 0, 1, 0);
		int best = 0, bestY = 0;
		for (int step = 0; step < EffectOrbit::StepCount; ++step)
		{
			probe.SetOrbitStep(step);
			const int y = probe.GetOrbitPosition().y;
			if (above ? y < bestY : y > bestY) { best = step; bestY = y; }
		}
		return best;
	}
	MAttachOrbitEffect& Orbit(BYTE bltType, int frameID, int step)
	{
		MAttachOrbitEffect& effect = Attach(new MAttachOrbitEffect(10, 0, 1, step), bltType, frameID, 8);
		return effect;
	}
};

} // namespace

TEST(TopView3AttachEffect, AnOrbitingEffectIsDrawnAtItsOrbitBehindWhenAbove)
{
	for (bool above : {false, true})
	{
		Orbits scene;
		scene.Alpha(1, 0, 0, 1, 30, 30);					// a frame in front
		MAttachOrbitEffect& orbit = scene.Orbit(BLT_EFFECT, 1, scene.Step(above));
		const POINT at = orbit.GetOrbitPosition();
		CHECK(above ? at.y < 0 : at.y > 0);
		CHECK(at.x != 0 || at.y != 0);
		scene.Draw(above ? 2 : 1);
		scene.ExpectAttached(30 + at.x, 30 + at.y, 1, 1);
		CHECK(scene.Same());
		scene.Clear();
		scene.Draw(above ? 1 : 2);
		CHECK(scene.drawn.Blank());
	}
}

TEST(TopView3AttachEffect, AnOrbitingScreenEffectIsBehindWhenAbove)
{
	for (bool above : {false, true})
	{
		Orbits scene;
		Aura::Place(scene.View().m_EffectScreenFPK, 1, 0, 0, 0, 30, 30);		// a frame in front
		scene.Orbit(BLT_SCREEN, 1, scene.Step(above));
		scene.Draw(above ? 2 : 1);
		CHECK(!scene.drawn.Blank());
		scene.Clear();
		scene.Draw(above ? 1 : 2);
		CHECK(scene.drawn.Blank());
	}
}

TEST(TopView3AttachEffect, AnOrbitingNormalEffectIgnoresItsOrbit)
{
	// Recorded defect: the orbit offset goes to a point the blit does not
	// use. (Drawn after an alpha effect, which sets that point; alone, the
	// offset would be added to an unset one.)
	Orbits scene;
	scene.Alpha(1, 0, 0, 1, 2, 2);
	scene.Make(10, BLT_EFFECT, 1);
	Aura::Place(scene.View().m_EffectNormalFPK, 3, 0, 0, 0, 30, 30);
	MAttachOrbitEffect& orbit = scene.Orbit(BLT_NORMAL, 3, scene.Step(true));
	CHECK(orbit.GetOrbitPosition().y < 0);
	scene.Draw();
	scene.ExpectAttached(2, 2, 1, 1);
	scene.ExpectNormal(30, 30);
	CHECK(scene.Same());
}

//----------------------------------------------------------------------
// The two endings and DrawEvent
//----------------------------------------------------------------------
namespace {

const char* kMissingPack = "top_view_3_no_such_pack.spk";
const char* kPackPath = "top_view_3_pack.spk";

MonotonicClock::TimePoint s_EndingNow;
MonotonicClock::TimePoint EndingNow() { return s_EndingNow; }

std::vector<int>& Sounds() { static std::vector<int> sounds; return sounds; }

// An 800 x 600 screen, a test clock, a sound-recording host, and the
// endings' pack files named. A pack's sprite 0 is the 40 x 30 scroll, 1 a
// 24 x 12 caption, and n > 1 a square of 4 + n.
struct Ending : Scene
{
	MonotonicClock::ScopedTestSource clock{&EndingNow};
	WorldEvents::Host host;
	const WorldEvents::Host* oldHost = nullptr;
	Properties files;
	Properties* oldFiles = g_pFileDef;
	Ending() : Scene(800, 600)
	{
		s_EndingNow = MonotonicClock::FromMillis(1000000);
		Sounds().clear();
		host.PlaySoundForce = +[](TYPE_SOUNDID sound) { Sounds().push_back(int(sound)); };
		oldHost = WorldEvents::SetHost(&host);
		files.setProperty("FILE_SPRITE_ADVANCEMENT_QUEST", kMissingPack);
		files.setProperty("FILE_SPRITE_OUSTERS_FIN", kMissingPack);
		g_pFileDef = &files;
		// The endings' music needs the audio device, which a test has not got.
		CHECK(!g_SDLAudio.IsInit());
	}
	~Ending()
	{
		g_pFileDef = oldFiles;
		WorldEvents::SetHost(oldHost);
		std::remove(kPackPath);
	}
	static std::vector<WORD> SpriteWords(int n)
	{
		return n == 0 ? SolidWords(40, 30, 0xF800) : n == 1 ? SolidWords(24, 12, 0x07E0) : SolidWords(4 + n, 4 + n, WORD(0x8000 | (n * 0x0421)));
	}
	static void Fill(CSpritePack& pack, int count)
	{
		pack.Init(WORD(count));
		for (int n = 0; n < count; ++n) CHECK(LoadBytes(pack[WORD(n)], Bytes(SpriteWords(n))));
	}
	void WritePack(const char* key, int count)
	{
		std::vector<WORD> words{WORD(count)};
		for (int n = 0; n < count; ++n)
		{
			const std::vector<WORD> sprite = SpriteWords(n);
			words.insert(words.end(), sprite.begin(), sprite.end());
		}
		const std::vector<unsigned char> bytes = Bytes(words);
		std::ofstream out(kPackPath, std::ios::binary | std::ios::trunc);
		out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
		files.setProperty(key, kPackPath);
	}
	MEvent* Live(EVENT_ID id) { return const_cast<MEvent*>(events.GetEvent(id)); }
	MEvent* Start(EVENT_ID id, DWORD spritePair = 0)
	{
		Event(id);
		MEvent* event = Live(id);
		event->parameter4 = spritePair;
		return event;
	}
	bool Advancement() { return Access::AdvancementEnding(View(), Live(EVENTID_ADVANCEMENT_QUEST_ENDING)); }
	void Pack(int x, int y, CSpritePack& pack, int n) { expected.Sprite(x, y, pack[WORD(n)]); }
	void Clip(int x, int y, CSpritePack& pack, int n, int top, int bottom)
	{
		POINT point = {x, y};
		RECT rows = {0, top, pack[WORD(n)].GetWidth(), bottom};
		expected.surface.BltSpriteClip(&point, &pack[WORD(n)], &rows);
	}
	// The scroll's progress p: -500 at the start, one step every 66 ms.
	void Progress(MEvent* event, int p) { s_EndingNow = event->eventStartTickCount + MonotonicClock::Millis(DWORD(66 * (p + 500) + 33)); }
	// The fade from white over the screen that ends each picture: it keeps
	// a 32nd of the picture each 10 ms of the event's clock, 31 at most.
	void Fade(const MEvent* event) { expected.ColorAlpha(g_GameRect, 0xFFFF, int((std::min)(DWORD(31), event->ElapsedMillis() / 10))); }
};

} // namespace

TEST(TopView3AdvancementEnding, AnUnloadablePackCancelsTheEnding)
{
	for (int count : {-1, 0, 6})						// no file, an empty pack, one sprite short
	{
		Ending scene;
		if (count >= 0) scene.WritePack("FILE_SPRITE_ADVANCEMENT_QUEST", count);
		scene.Start(EVENTID_ADVANCEMENT_QUEST_ENDING);
		CHECK(scene.Advancement());
		CHECK(scene.Live(EVENTID_ADVANCEMENT_QUEST_ENDING) == nullptr);
		CHECK_EQ(0, int(scene.View().m_AdvacementQuestEnding.GetSize()));
		CHECK(Sounds().empty());
		CHECK(scene.drawn.Blank());
	}
}

TEST(TopView3AdvancementEnding, ALoadedPackStartsTheEnding)
{
	Ending scene;
	scene.WritePack("FILE_SPRITE_ADVANCEMENT_QUEST", 7);
	MEvent* event = scene.Start(EVENTID_ADVANCEMENT_QUEST_ENDING, 1);
	s_EndingNow += MonotonicClock::Millis(500);
	CHECK(!scene.Advancement());
	CHECK_EQ(7, int(scene.View().m_AdvacementQuestEnding.GetSize()));
	CHECK_EQ(9998, int(event->parameter1));
	CHECK_EQ(0, int(event->parameter2));
	CHECK_EQ(1000, int(event->parameter3));
	CHECK(event->eventStartTickCount == s_EndingNow);
	CHECK_EQ(size_t(1), Sounds().size());
	if (!Sounds().empty()) CHECK_EQ(int(SOUND_SLAYER_ENCHANT_B2), Sounds()[0]);
	// Its first picture: the second of the pair, under the fade, which
	// keeps none of it yet...
	scene.Pack(0, 0, scene.View().m_AdvacementQuestEnding, 2);
	scene.Fade(event);
	CHECK(scene.Same());
	// ... and 6/32 of it 60 ms on, before the scroll's first step.
	s_EndingNow += MonotonicClock::Millis(60);
	CHECK(!scene.Advancement());
	CHECK_EQ(size_t(1), Sounds().size());				// started once
	scene.Pack(0, 0, scene.View().m_AdvacementQuestEnding, 2);
	scene.Fade(event);
	CHECK(scene.Same());
}

TEST(TopView3AdvancementEnding, ASpritePairPastThePackCancelsIt)
{
	for (DWORD pair : {DWORD(6), DWORD(0xFFFFFFFF), DWORD(5)})
	{
		Ending scene;
		Ending::Fill(scene.View().m_AdvacementQuestEnding, 7);
		scene.Start(EVENTID_ADVANCEMENT_QUEST_ENDING, pair);
		const bool cancelled = scene.Advancement();
		CHECK_EQ(pair != 5, cancelled);
		CHECK_EQ(pair == 5, scene.Live(EVENTID_ADVANCEMENT_QUEST_ENDING) != nullptr);
		CHECK_EQ(pair == 5 ? 7 : 0, int(scene.View().m_AdvacementQuestEnding.GetSize()));
	}
}

TEST(TopView3AdvancementEnding, ThePairCrossfadesByItsCountdown)
{
	const struct { DWORD countdown; bool first, second; } cases[] = {
		{ 33, false, true }, { 32, true, true }, { 1, true, true }, { 0, true, false },
	};
	for (const auto& c : cases)
	{
		Ending scene;
		CSpritePack& pack = scene.View().m_AdvacementQuestEnding;
		Ending::Fill(pack, 7);
		MEvent* event = scene.Start(EVENTID_ADVANCEMENT_QUEST_ENDING, 3);
		event->parameter1 = 9998;
		event->parameter3 = c.countdown;
		scene.Progress(event, 30);							// the fade over, the scroll gone by
		CHECK(!scene.Advancement());
		if (c.first) scene.Pack(0, 0, pack, 3);
		if (c.second && c.first)
		{
			POINT origin = {0, 0};
			scene.expected.surface.BltSpriteAlpha(&origin, &pack[4], BYTE(c.countdown));
		}
		else if (c.second) scene.Pack(0, 0, pack, 4);
		scene.Fade(event);
		CHECK(scene.Same());
	}
}

TEST(TopView3AdvancementEnding, TheCountdownStepsOnOddChangedFrames)
{
	const struct { bool changed; DWORD frame, from, to; } cases[] = {
		{ true, 3, 10, 9 }, { false, 3, 10, 10 }, { true, 4, 10, 10 }, { true, 3, 0, 0 },
	};
	for (const auto& c : cases)
	{
		Ending scene;
		Ending::Fill(scene.View().m_AdvacementQuestEnding, 7);
		MEvent* event = scene.Start(EVENTID_ADVANCEMENT_QUEST_ENDING);
		event->parameter1 = 9998;
		event->parameter3 = c.from;
		g_bFrameChanged = c.changed;
		g_CurrentFrame = c.frame;
		scene.Advancement();
		CHECK_EQ(int(c.to), int(event->parameter3));
	}
}

TEST(TopView3AdvancementEnding, TheScrollRisesWithTheClock)
{
	const struct { int progress, y, top, bottom; } cases[] = {
		{ -500, 0, 0, 0 },			// nothing yet
		{ -490, 540, 0, 10 },		// its first ten rows
		{ -100, 150, 0, 30 },		// whole
		{ 10, 40, 10, 30 },			// its top ten rows gone
		{ 30, 20, 0, 0 },			// gone, but not yet over
	};
	for (const auto& c : cases)
	{
		Ending scene;
		CSpritePack& pack = scene.View().m_AdvacementQuestEnding;
		Ending::Fill(pack, 7);
		MEvent* event = scene.Start(EVENTID_ADVANCEMENT_QUEST_ENDING);
		event->parameter1 = 9998;
		event->parameter3 = 0;
		scene.Progress(event, c.progress);
		CHECK(!scene.Advancement());
		CHECK_EQ(9998, int(event->parameter1));
		scene.Pack(0, 0, pack, 0);
		if (c.bottom > c.top) scene.Clip(380, c.y, pack, 0, c.top, c.bottom);
		scene.Fade(event);
		CHECK(scene.Same());
	}
}

TEST(TopView3AdvancementEnding, ItEndsOnceTheScrollHasPassed)
{
	Ending scene;
	Ending::Fill(scene.View().m_AdvacementQuestEnding, 7);
	MEvent* event = scene.Start(EVENTID_ADVANCEMENT_QUEST_ENDING);
	event->parameter1 = 9998;
	scene.Progress(event, 31);
	CHECK(!scene.Advancement());
	CHECK_EQ(9999, int(event->parameter1));
	CHECK_EQ(7, int(scene.View().m_AdvacementQuestEnding.GetSize()));
	CHECK(!scene.Advancement());
	CHECK(scene.Live(EVENTID_ADVANCEMENT_QUEST_ENDING) == nullptr);
	CHECK_EQ(0, int(scene.View().m_AdvacementQuestEnding.GetSize()));
}

TEST(TopView3OustersEnding, WithoutEventsTheBackgroundIsDrawn)
{
	Ending scene;
	CHECK(Access::OustersEnding(scene.View()));
	CHECK(scene.drawn.Blank());
}

TEST(TopView3OustersEnding, ABackgroundEventHidesTheBackground)
{
	const struct { DWORD flag; bool cleared; } cases[] = {
		{ EVENTFLAG_NOT_DRAW_BACKGROUND, true },
		{ EVENTFLAG_EVENT_BACKGROUND, false },
		{ EVENTFLAG_ONLY_EVENT_BACKGROUND, false },	// both bits: not cleared
	};
	for (const auto& c : cases)
	{
		Ending scene;
		scene.Event(EVENTID_BG_CLOUD, c.flag);
		CHECK(!Access::OustersEnding(scene.View()));
		if (c.cleared)
		{
			CHECK_EQ(0, int(scene.drawn.At(0, 0)));
			CHECK_EQ(0, int(scene.drawn.At(799, 599)));
		}
		else CHECK(scene.drawn.Blank());
	}
}

TEST(TopView3OustersEnding, AnUnloadablePackCancelsTheEnding)
{
	for (int count : {-1, 10})
	{
		Ending scene;
		if (count >= 0) scene.WritePack("FILE_SPRITE_OUSTERS_FIN", count);
		scene.Start(EVENTID_OUSTERS_FIN);
		CHECK(Access::OustersEnding(scene.View()));
		CHECK(scene.Live(EVENTID_OUSTERS_FIN) == nullptr);
		CHECK_EQ(0, int(scene.View().m_OustersFinSPK.GetSize()));
		CHECK(Sounds().empty());
	}
}

TEST(TopView3OustersEnding, ACancelledEndingShowsTheBackgroundAnEventHid)
{
	// Recorded defect: the cancel returns true whatever the background
	// event said, so the hidden background is drawn.
	Ending scene;
	scene.Event(EVENTID_BG_CLOUD, EVENTFLAG_EVENT_BACKGROUND);
	scene.Start(EVENTID_OUSTERS_FIN);
	CHECK(Access::OustersEnding(scene.View()));
}

TEST(TopView3OustersEnding, ALoadedPackStartsTheEndingOverTheBackground)
{
	Ending scene;
	scene.WritePack("FILE_SPRITE_OUSTERS_FIN", 11);
	MEvent* event = scene.Start(EVENTID_OUSTERS_FIN);
	s_EndingNow += MonotonicClock::Millis(700);
	CHECK(Access::OustersEnding(scene.View()));
	CHECK_EQ(11, int(scene.View().m_OustersFinSPK.GetSize()));
	CHECK_EQ(9998, int(event->parameter1));
	CHECK_EQ(0, int(event->parameter2));
	CHECK(event->eventStartTickCount == s_EndingNow);
	CHECK_EQ(size_t(1), Sounds().size());
	if (!Sounds().empty()) CHECK_EQ(int(SOUND_SLAYER_ENCHANT_B2), Sounds()[0]);
	// The scroll has not risen: only the fade, which keeps none of the
	// screen yet, and 6/32 of it 60 ms on.
	scene.Fade(event);
	CHECK(scene.Same());
	s_EndingNow += MonotonicClock::Millis(60);
	CHECK(Access::OustersEnding(scene.View()));
	CHECK_EQ(size_t(1), Sounds().size());
	scene.Fade(event);
	CHECK(scene.Same());
	// A background event still hides the background under it.
	scene.Event(EVENTID_BG_CLOUD, EVENTFLAG_EVENT_BACKGROUND);
	CHECK(!Access::OustersEnding(scene.View()));
}

TEST(TopView3OustersEnding, TheScrollRisesAndTheCaptionFollows)
{
	const struct { int progress, y, top, bottom, captionY, captionRows; } cases[] = {
		{ -490, 540, 0, 10, 250, 0 },		// the caption not yet
		{ -440, 490, 0, 30, 250, 0 },		// 450 - 440 - 30: none still
		{ -415, 465, 0, 30, 250, 5 },		// its first five rows
		{ -100, 150, 0, 30, 250, 12 },		// whole
		{ 10, 40, 10, 30, 250, 12 },
	};
	for (const auto& c : cases)
	{
		Ending scene;
		CSpritePack& pack = scene.View().m_OustersFinSPK;
		Ending::Fill(pack, 11);
		MEvent* event = scene.Start(EVENTID_OUSTERS_FIN);
		event->parameter1 = 9998;
		scene.Progress(event, c.progress);
		CHECK(Access::OustersEnding(scene.View()));
		CHECK_EQ(9998, int(event->parameter1));
		scene.Clip(380, c.y, pack, 0, c.top, c.bottom);
		if (c.captionRows > 0) scene.Clip(388, c.captionY, pack, 1, 0, c.captionRows);
		scene.Fade(event);
		CHECK(scene.Same());
	}
}

TEST(TopView3OustersEnding, ItEndsOnceTheScrollHasPassed)
{
	Ending scene;
	Ending::Fill(scene.View().m_OustersFinSPK, 11);
	MEvent* event = scene.Start(EVENTID_OUSTERS_FIN);
	event->parameter1 = 9998;
	scene.Progress(event, 31);
	CHECK(Access::OustersEnding(scene.View()));
	CHECK_EQ(9999, int(event->parameter1));
	CHECK(Access::OustersEnding(scene.View()));
	CHECK(scene.Live(EVENTID_OUSTERS_FIN) == nullptr);
	CHECK_EQ(0, int(scene.View().m_OustersFinSPK.GetSize()));
}

TEST(TopView3DrawEvent, TheAdvancementEndingComesFirstAndHidesTheBackground)
{
	Ending scene;
	CHECK(Access::DrawEvent(scene.View()));
	Ending::Fill(scene.View().m_AdvacementQuestEnding, 7);
	Ending::Fill(scene.View().m_OustersFinSPK, 11);
	MEvent* ousters = scene.Start(EVENTID_OUSTERS_FIN);
	MEvent* advancement = scene.Start(EVENTID_ADVANCEMENT_QUEST_ENDING);
	CHECK(!Access::DrawEvent(scene.View()));
	CHECK_EQ(9998, int(advancement->parameter1));
	CHECK_EQ(0, int(ousters->parameter1));				// not run alongside
	scene.events.RemoveEvent(EVENTID_ADVANCEMENT_QUEST_ENDING);
	CHECK(Access::DrawEvent(scene.View()));
	CHECK_EQ(9998, int(ousters->parameter1));
	scene.Event(EVENTID_BG_CLOUD, EVENTFLAG_EVENT_BACKGROUND);
	CHECK(!Access::DrawEvent(scene.View()));
}

//----------------------------------------------------------------------
// The ousters ending's two scenes
//----------------------------------------------------------------------
namespace {

// The queued text's text and place, oldest first; the queue emptied.
struct Line { std::string text; int x, y; };
std::vector<Line> Lines(MTopView& view)
{
	std::vector<Line> lines;
	while (!view.m_pqDrawText.empty())
	{
		DRAWTEXT_NODE* node = view.m_pqDrawText.top();
		view.m_pqDrawText.pop();
		lines.push_back({node->GetString() != nullptr ? node->GetString() : "", node->GetX(), node->GetY()});
		delete node;
	}
	return lines;
}
bool SameLines(const std::vector<Line>& a, const std::vector<Line>& b)
{
	bool same = a.size() == b.size();
	for (size_t i = 0; same && i < a.size(); ++i) same = a[i].text == b[i].text && a[i].x == b[i].x && a[i].y == b[i].y;
	if (!same) std::printf("      %d lines, not %d\n", int(a.size()), int(b.size()));
	return same;
}

// Status changes that run no reactions.
struct QuietStatus : ModifyStatusManager
{
	bool Execute(int, void*) const override { return false; }
};

// The slayer's (parameter1 1000) and the vampire's (2000) scenes, which
// only an event started there runs (an event from 0 goes straight to
// the scroll, 9998). Their nine script lines are strings of their own;
// the fonts measure; the pack's sprites are Ending's. The slayer wears
// one part (frame ID 1, its index sprite at (3, 4)); the vampire's body
// is frame ID 1 too (at (5, 6)); both stand on frame 0.
struct Fin : Ending
{
	PrintInfo fonts[10] = {};
	PrintInfo* oldFonts[10];
	QuietStatus status;
	MParty party;
	ModifyStatusManager* oldStatus = g_pModifyStatusManager;
	MParty* oldParty = g_pParty;
	Fin()
	{
		g_pModifyStatusManager = &status;		// the player's death sets its HP
		g_pParty = &party;						// and tells its party, of none
		for (int i = 0; i < 10; ++i)
		{
			oldFonts[i] = g_ClientPrintInfo[i];
			fonts[i].hfont = TextSystem::EncodeFontSizeHandle(10 + i);
			g_ClientPrintInfo[i] = &fonts[i];
		}
		g_pGameStringTable->Init(MAX_GAME_STRING);
		for (int i = 0; i < 10; ++i)
		{
			*g_pGameStringTable->GetMutable(STRING_MESSAGE_RIPATY_SCRIPT_1 + i) = Script('R', i).c_str();
			*g_pGameStringTable->GetMutable(STRING_MESSAGE_AMATA_SCRIPT_1 + i) = Script('A', i).c_str();
		}
		Ending::Fill(View().m_OustersFinSPK, 11);
		CREATURESPRITETABLE_INFO& body = testfw::MutableRow(sprites, 0);
		body.FrameID = 1;
		body.CreatureType = BYTE(FLAG_CREATURESPRITE_MONSTER);
		for (int type : {kSlayer, kVampire})
		{
			CREATURETABLE_INFO& row = testfw::MutableRow(creatureTypes, type);
			row.SpriteTypes.Init(1);
			*row.SpriteTypes.GetMutable(0) = 0;
			row.DeadActionInfo = ACTIONINFO_NULL;		// dying takes no action, so no action table
		}
		for (CCreatureFramePack* pack : {&View().m_AddonFPK, &View().m_CreatureFPK})
		{
			pack->Init(2);
			for (int id = 0; id < 2; ++id)
			{
				(*pack)[WORD(id)].Init(BYTE(ACTION_MAX_SLAYER));
				for (int action = 0; action < ACTION_MAX_SLAYER; ++action) (*pack)[WORD(id)][BYTE(action)].Init(8);
			}
		}
		View().m_AddonFPK[1][ACTION_STAND][6].Init(1);
		View().m_AddonFPK[1][ACTION_STAND][6][0].Set(0, 3, 4);
		View().m_CreatureFPK[1][ACTION_STAND][6].Init(1);
		View().m_CreatureFPK[1][ACTION_STAND][6][0].Set(0, 5, 6);
		View().m_AddonSPK.Init(1);
		Mark(View().m_AddonSPK[0], 0x1111);
		View().m_CreatureSPK.Init(1);
		Mark(View().m_CreatureSPK[0], 0x2222);
		for (auto& addon : player->m_Addon) addon.bAddon = FALSE;
		player->m_Action = ACTION_STAND;
		player->m_ActionCount = 0;
		player->SetBodyColor1(30);
		player->SetBodyColor2(60);
	}
	~Fin()
	{
		Lines(View());
		for (int i = 0; i < 10; ++i) g_ClientPrintInfo[i] = oldFonts[i];
		g_pParty = oldParty;
		g_pModifyStatusManager = oldStatus;
	}
	static std::string Script(char who, int n) { return std::string(1, who) + char('0' + n); }
	// A scene at its step, its clock started now. The ending is begun first,
	// which sets its fade back to white from what a scene before left.
	MEvent* Scene(DWORD scene, DWORD step)
	{
		MEvent* event = Start(EVENTID_OUSTERS_FIN);
		CHECK(Run());
		Sounds().clear();
		Clear();
		event->parameter1 = scene;
		event->parameter2 = step;
		return event;
	}
	void After(MEvent* event, int millis) { s_EndingNow = event->eventStartTickCount + MonotonicClock::Millis(DWORD(millis)); }
	bool Run() { return Access::OustersEnding(View()); }
	// A step starts the fade's clock again: the picture `lay` lays is hidden
	// at once and shows again 310 ms on, when the fade keeps 31/32 of it.
	template<class Lay> void Flash(MEvent* event, Lay lay)
	{
		lay();
		Fade(event);
		CHECK(Same());
		Clear();
		After(event, 310);
		CHECK(Run());
		Lines(View());
		lay();
		Fade(event);
		CHECK(Same());
	}
	// The scene's picture: its figure (sprite 3 or 2) and, past step 0,
	// sprites 4 to 10 round it.
	void Ring(int figure, bool round)
	{
		static const int centres[8][2] = { {400, 160}, {300, 210}, {240, 300}, {300, 390}, {400, 440}, {500, 390}, {560, 300}, {500, 210} };
		CSpritePack& pack = View().m_OustersFinSPK;
		for (int i = 0; i < (round ? 8 : 1); ++i)
		{
			const int n = i == 0 ? figure : 3 + i;
			Pack(centres[i][0] - pack[WORD(n)].GetWidth() / 2 + 10, centres[i][1] - pack[WORD(n)].GetHeight() + 24, pack, n);
		}
	}
	POINT Figure(int figure)
	{
		CSprite& sprite = View().m_OustersFinSPK[WORD(figure)];
		return { 400 - sprite.GetWidth() / 2 + 10, 160 - sprite.GetHeight() + 24 };
	}
	// The chat DrawChatString queues for the player at the point.
	std::vector<Line> Chat(POINT at)
	{
		Lines(View());
		Access::DrawChatString(View(), &at, player, RGB(255, 255, 255));
		return Lines(View());
	}
};

} // namespace

TEST(TopView3OustersEnding, TheSlayersSceneSpeaksALineEveryFourSecondsThenDiesAndRolls)
{
	Fin scene;
	MTopView& view = scene.View();
	MEvent* event = scene.Scene(1000, 0);
	PlayerProbe& me = *scene.player;
	const POINT talk = { scene.Figure(3).x - 15, scene.Figure(3).y - 15 };
	// Within four seconds: step 0, its figure alone and no line.
	scene.After(event, 3999);
	CHECK(scene.Run());
	CHECK_EQ(0, int(event->parameter2));
	CHECK(Sounds().empty());
	CHECK(Lines(view).empty());
	scene.Ring(3, false);
	scene.Fade(event);
	CHECK(scene.Same());
	// Each four seconds a step, a sound and a line: the round shows.
	for (int step = 1; step <= 8; ++step)
	{
		scene.Clear();
		scene.After(event, 4000);
		const MonotonicClock::TimePoint now = s_EndingNow;
		CHECK(scene.Run());
		CHECK_EQ(1000, int(event->parameter1));
		CHECK_EQ(step, int(event->parameter2));
		CHECK(event->eventStartTickCount == now);
		CHECK_EQ(size_t(step), Sounds().size());
		CHECK(std::string(me.GetChatString(BYTE(g_pClientConfig->MAX_CHATSTRING_MINUS_1))) == Fin::Script('R', step));
		const std::vector<Line> said = Lines(view);
		CHECK(!said.empty());
		CHECK(SameLines(said, scene.Chat(talk)));
		scene.Flash(event, [&] { scene.Ring(3, true); });
		CHECK(me.IsAlive());
	}
	for (int sound : Sounds()) CHECK_EQ(int(SOUND_SLAYER_ENCHANT_B2), sound);
	// Step 9: the line goes, the slayer dies.
	scene.After(event, 4000);
	CHECK(scene.Run());
	CHECK_EQ(9, int(event->parameter2));
	CHECK(!me.IsAlive());
	CHECK_EQ(size_t(9), Sounds().size());
	if (Sounds().size() == 9) CHECK_EQ(int(SOUND_SLAYER_DIE_MALE), Sounds()[8]);
	CHECK(std::string(me.GetChatString(BYTE(g_pClientConfig->MAX_CHATSTRING_MINUS_1))).empty());
	// Step 10 waits; step 11 rolls the scroll on the slayer's background.
	scene.After(event, 4000);
	CHECK(scene.Run());
	CHECK_EQ(10, int(event->parameter2));
	CHECK_EQ(1000, int(event->parameter1));
	scene.After(event, 4000);
	const MonotonicClock::TimePoint rolled = s_EndingNow;
	CHECK(scene.Run());
	CHECK_EQ(11, int(event->parameter2));
	CHECK_EQ(9998, int(event->parameter1));
	CHECK_EQ(int(EVENTBACKGROUNDID_OUSTERS_SLAYER), int(event->parameter4));
	CHECK(event->eventStartTickCount == rolled);
	CHECK_EQ(size_t(9), Sounds().size());
}

TEST(TopView3OustersEnding, TheSlayersSceneDressesItsFigureInWhatThePlayerWears)
{
	Fin scene;
	MEvent* event = scene.Scene(1000, 0);
	MCreatureWear::ADDON_INFO& coat = scene.player->m_Addon[ADDON_COAT];
	coat.bAddon = TRUE;
	coat.FrameID = 1;
	coat.ItemClass = ITEM_CLASS_COAT;
	coat.ColorSet1 = 30;
	coat.ColorSet2 = 60;
	coat.bEffectColor = FALSE;
	scene.After(event, 100);
	CHECK(scene.Run());
	scene.Ring(3, false);
	CIndexSprite::SetUsingColorSet(30, 60);
	scene.expected.Index(384 + 3, 312 + 4, scene.View().m_AddonSPK[0]);
	scene.Fade(event);
	CHECK(scene.Same());
	// A part without the action's frame is not drawn.
	scene.Clear();
	scene.player->m_Action = ACTION_MOVE;
	CHECK(scene.Run());
	scene.Ring(3, false);
	scene.Fade(event);
	CHECK(scene.Same());
}

TEST(TopView3OustersEnding, TheVampiresSceneShowsItsBodyInItsColoursAndDiesAtStepNine)
{
	Fin scene;
	MTopView& view = scene.View();
	PlayerProbe& me = *scene.player;
	me.SetCreatureType(kVampire);
	me.m_Action = ACTION_STAND;
	me.m_ActionCount = 0;
	MEvent* event = scene.Scene(2000, 0);
	const POINT talk = { scene.Figure(2).x - 15, scene.Figure(2).y - 15 };
	auto body = [&]()
	{
		CIndexSprite::SetUsingColorSet(30, 60);
		scene.expected.Index(384 + 5, 312 + 6, view.m_CreatureSPK[0]);
	};
	scene.After(event, 3999);
	CHECK(scene.Run());
	CHECK_EQ(0, int(event->parameter2));
	CHECK(Sounds().empty());
	scene.Ring(2, false);
	body();
	scene.Fade(event);
	CHECK(scene.Same());
	for (int step = 1; step <= 8; ++step)
	{
		scene.Clear();
		scene.After(event, 4000);
		CHECK(scene.Run());
		CHECK_EQ(step, int(event->parameter2));
		CHECK(std::string(me.GetChatString(BYTE(g_pClientConfig->MAX_CHATSTRING_MINUS_1))) == Fin::Script('A', step));
		const std::vector<Line> said = Lines(view);
		CHECK(!said.empty());
		CHECK(SameLines(said, scene.Chat(talk)));
		scene.Flash(event, [&] { scene.Ring(2, true); body(); });
		CHECK(me.IsAlive());
	}
	scene.After(event, 4000);
	CHECK(scene.Run());
	CHECK(!me.IsAlive());
	CHECK_EQ(size_t(9), Sounds().size());
	if (Sounds().size() == 9) CHECK_EQ(int(SOUND_VAMPIRE_DIE_MALE), Sounds()[8]);
	scene.After(event, 4000);
	CHECK(scene.Run());
	CHECK_EQ(10, int(event->parameter2));
	CHECK_EQ(2000, int(event->parameter1));
	scene.After(event, 4000);
	CHECK(scene.Run());
	CHECK_EQ(9998, int(event->parameter1));
	CHECK_EQ(int(EVENTBACKGROUNDID_OUSTERS_VAMPIRE), int(event->parameter4));
	CHECK_EQ(size_t(9), Sounds().size());
}
