#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "MTopView.h"
#include "MPlayer.h"
#include "MZone.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "EffectSpriteTypeDef.h"
#include "EffectSpriteTypeHelper.h"
#include "UserOption.h"
#include "FrameClock.h"
#include "ViewSectors.h"
#include "WorldEventsHost.h"
#include "VS_UI_Base.h"
#include "Fl2.h"
#include "FontHandleUtil.h"
#include "TextService.h"
#include "DXLib/CDirectDraw.h"
#include "SpriteLib/CSpriteSurface.h"
#include "SpriteLib/SpriteLibBackend.h"
#include "SpriteLib/CIndexSprite.h"

#include <climits>
#include <cstdio>
#include <fstream>
#include <memory>
#include <set>
#include <vector>

#if defined(__SANITIZE_ADDRESS__)
#define TOPVIEW1_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define TOPVIEW1_ASAN 1
#endif
#endif
#ifdef TOPVIEW1_ASAN
// Declared here rather than through <sanitizer/asan_interface.h>, which
// not every toolchain's ASan ships.
extern "C" int __asan_address_is_poisoned(void const volatile* addr);
#endif

// The view's own stand-up and teardown, its simple state, the effect
// queries, the item-name list, the text queue, the output-creature map,
// the creature sprite bookkeeping, and the text it draws (task 4.260,
// which moved MTopView into gameclient). A view is constructed, given a
// surface and only what each path reads; nothing loads data files.

extern PrintInfo*	g_ClientPrintInfo[];	// MTopView.cpp's, ten fonts
extern bool			g_bInterpolateDraw;		// MObjectScreen.cpp's
extern RECT			g_GameRect;
extern BOOL			g_bDrawProgress;		// GameUI.cpp's

// The view's protected and private members, for this file's tests.
struct MTopViewTestAccess1
{
	static CSpriteSurface*	Surface(MTopView& view)			{ return view.m_pSurface; }
	static bool&			Initialised(MTopView& view)		{ return view.m_bInit; }
	static bool				InitColors(MTopView& view)		{ return view.InitColors(); }
	static bool				InitSurfaces(MTopView& view)	{ return view.InitSurfaces(); }
	static bool				InitFonts(MTopView& view)		{ return view.InitFonts(); }
	static CreatureDrawPacks	Packs(MTopView& view)		{ return view.GetCreatureDrawPacks(); }
	static const std::map<DWORD, DrawCreatureExceptionProc>& Exceptions(MTopView& view) { return view.m_ExceptionProcMap; }
};

namespace {

typedef MTopViewTestAccess1 Access;

const int kFonts = 10;		// MTopView.cpp's MAX_FONTID
const int kFontItem = 7;	// its FONTID_ITEM
const int kWidth = 96, kHeight = 64;
const WORD kPaper = 0xFFFF;

// Whether the view freed a block the test allocated. ASan poisons a freed
// block, so the check is exact in a sanitized tree and passes elsewhere.
bool Freed(const void* p)
{
#ifdef TOPVIEW1_ASAN
	return __asan_address_is_poisoned(p) != 0;
#else
	(void)p;
	return true;
#endif
}

bool Live(const void* p)
{
#ifdef TOPVIEW1_ASAN
	return __asan_address_is_poisoned(p) == 0;
#else
	(void)p;
	return true;
#endif
}

const char* kPath = "top_view_1.bin";

void WriteWords(const std::vector<WORD>& words)
{
	std::ofstream out(kPath, std::ios::binary | std::ios::trunc);
	for (WORD word : words) { out.put(char(word & 0xFF)); out.put(char(word >> 8)); }
}

// Any loadable element of a pack: an index sprite, a shadow sprite.
template<class Sprite> void Load(Sprite& sprite, const std::vector<WORD>& words)
{
	WriteWords(words);
	std::ifstream in(kPath, std::ios::binary);
	CHECK(sprite.LoadFromFile(in));
	in.close();
	std::remove(kPath);
}

// A 4x1 index sprite: one transparent pixel, two palette pixels, a colour.
void LoadIndexSprite(CIndexSprite& sprite) { Load(sprite, {4, 1, 7, 1, 1, 2, 0x010a, 0xff1d, 1, 0x07e0}); }

// A 2x1 shadow sprite.
void LoadShadow(CShadowSprite& sprite) { Load(sprite, {2, 1, 3, 1, 0, 2}); }

// A one-entry file index table.
void LoadIndex(CFileIndexTable& table)
{
	{
		std::ofstream out(kPath, std::ios::binary | std::ios::trunc);
		const char bytes[] = {1, 0, 2, 0, 0, 0};
		out.write(bytes, sizeof bytes);
	}
	std::ifstream in(kPath, std::ios::binary);
	CHECK(table.LoadFromFile(in));
	in.close();
	std::remove(kPath);
}

struct Canvas
{
	CSpriteSurface surface;
	Canvas() { CHECK_EQ(0, spritectl_init()); CHECK(surface.Init(kWidth, kHeight)); surface.FillSurface(kPaper); }
	std::vector<WORD> Picture()
	{
		std::vector<WORD> pixels;
		DWORD pitch = 0;
		const unsigned char* base = static_cast<const unsigned char*>(surface.Lock(nullptr, &pitch));
		CHECK(base != nullptr);
		if (base == nullptr) return pixels;
		for (int y = 0; y < kHeight; ++y)
			for (int x = 0; x < kWidth; ++x)
				pixels.push_back(reinterpret_cast<const WORD*>(base + y * pitch)[x]);
		surface.Unlock();
		return pixels;
	}
	bool Blank() { for (WORD pixel : Picture()) if (pixel != kPaper) return false; return true; }
};

// The two pictures are the same; reports the first difference.
bool Same(Canvas& drawn, Canvas& expected)
{
	const std::vector<WORD> a = drawn.Picture(), b = expected.Picture();
	for (size_t i = 0; i < a.size() && i < b.size(); ++i)
		if (a[i] != b[i])
		{
			std::printf("      the picture differs first at (%d,%d): %04x, not %04x\n", int(i % kWidth), int(i / kWidth), a[i], b[i]);
			return false;
		}
	return a.size() == b.size();
}

// A view with a surface, as every test needs one: the constructor leaves
// m_pSurface uninitialised.
struct View
{
	Canvas canvas;
	std::unique_ptr<MTopView> view{new MTopView};
	View() { CSpriteSurface* p = &canvas.surface; view->SetSurface(p); }
	MTopView* operator->() { return view.get(); }
	MTopView& operator*() { return *view; }
};

struct ZoneProbe : MZone
{
	using MZone::MZone;
	using MZone::m_mapCreature;
	using MZone::m_mapFakeCreature;
	using MZone::m_mapItem;
};

struct CreatureProbe : MCreature
{
	using MCreature::m_sY;
	CreatureProbe(TYPE_OBJECTID id, int sectorY, int pixelY = 0)
	{
		SetID(id);
		MObject::SetPosition(1, TYPE_SECTORPOSITION(sectorY));
		m_sY = pixelY;
	}
};

struct PlayerProbe : MPlayer
{
	using MCreature::m_sY;
	using MCreature::m_bInCasket;
};

// The globals the item names and the creature map read; restored after.
struct Model
{
	GameModelWorld world;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	RankBonusTable rankBonuses;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MPlayer* oldPlayer = g_pPlayer;
	MZone* oldZone = g_pZone;
	DWORD oldFrame = g_CurrentFrame;
	bool oldInterpolate = g_bInterpolateDraw;
	ZoneProbe zone{TYPE_SECTORPOSITION(8), TYPE_SECTORPOSITION(8)};
	Model()
	{
		// What a creature's constructor reads.
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		g_pRankBonusTable = &rankBonuses;
		g_CurrentFrame = 1;
		g_bInterpolateDraw = false;
	}
	~Model()
	{
		zone.m_mapCreature.clear();
		zone.m_mapFakeCreature.clear();
		zone.m_mapItem.clear();
		g_bInterpolateDraw = oldInterpolate;
		g_CurrentFrame = oldFrame;
		g_pZone = oldZone;
		g_pPlayer = oldPlayer;
		g_pRankBonusTable = oldRankBonuses;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
	}
};

std::unique_ptr<MItem> Item(TYPE_OBJECTID id, const char* name = nullptr)
{
	std::unique_ptr<MItem> item(MRing::NewItem());
	item->SetID(id);
	if (name != nullptr) item->SetName(name);
	return item;
}

std::vector<DRAWITEMNAME_NODE*> Names(MTopView& view)
{
	return std::vector<DRAWITEMNAME_NODE*>(view.m_listDrawItemName.begin(), view.m_listDrawItemName.end());
}

// The text queue in drawing order, emptied.
std::vector<DRAWTEXT_NODE*> Drain(MTopView& view)
{
	std::vector<DRAWTEXT_NODE*> nodes;
	while (!view.m_pqDrawText.empty()) { nodes.push_back(view.m_pqDrawText.top()); view.m_pqDrawText.pop(); }
	return nodes;
}

std::vector<int> Contents(const MTopView::INT_ORDERED_LIST& list)
{
	return std::vector<int>(list.GetIterator(), list.GetEnd());
}

MTopView::INT_ORDERED_LIST List(std::initializer_list<int> values)
{
	MTopView::INT_ORDERED_LIST list;
	for (int value : values) list.Add(value);
	return list;
}

QWORD Key(TYPE_OBJECTID id, int y) { return (QWORD(DWORD(y)) << 32) | id; }

} // namespace

//----------------------------------------------------------------------
// Construction, surface and teardown
//----------------------------------------------------------------------
TEST(TopView1Life, ANewViewHoldsNothingAndSelectsNothing)
{
	View view;
	MTopView& v = *view;
	CHECK(!v.IsInit());
	CHECK(!Access::Initialised(v));
	CHECK(v.m_pZone == nullptr);
	CHECK_EQ(0, int(v.GetFirstSector().x));
	CHECK_EQ(0, int(v.GetFirstSector().y));
	CHECK_EQ(0, int(v.GetFirstZonePixel().x));
	CHECK_EQ(0, int(v.GetFirstZonePixel().y));
	CHECK_EQ(int(SECTORPOSITION_NULL), int(v.m_SelectSector.x));
	CHECK_EQ(int(SECTORPOSITION_NULL), int(v.m_SelectSector.y));
	CHECK_EQ(TYPE_OBJECTID(OBJECTID_NULL), v.GetSelectedCreature());
	CHECK_EQ(TYPE_OBJECTID(OBJECTID_NULL), v.GetSelectedItemID());
	CHECK_EQ(TYPE_OBJECTID(OBJECTID_NULL), v.m_SelectEffectID);
	CHECK(v.m_pTileSurface == nullptr);
	CHECK(v.m_bFirstTileDraw);
	CHECK_EQ(0, v.m_TileSurfaceFirstZonePixelX);
	CHECK_EQ(0, v.m_TileSurfaceFirstZonePixelY);
	CHECK(v.m_pTileRenderer == nullptr);
	CHECK_EQ(0, int(v.GetDarkBits()));
	CHECK(v.m_pLightBufferTexture == nullptr);
	CHECK(v.m_p2DLightPixelWidth == nullptr);
	CHECK(v.m_p2DLightPixelHeight == nullptr);
	CHECK(!v.IsDrawItemNameList());
	CHECK(v.m_pTileSFPArrayLargeZone == nullptr);
	CHECK(v.m_pImageObjectSFPArrayLargeZone == nullptr);
	CHECK(v.m_pTileSFPArraySmallZone == nullptr);
	CHECK(v.m_pImageObjectSFPArraySmallZone == nullptr);
	CHECK(!v.IsDrawRequest());
	CHECK(!v.m_bTileSearchForCreature);
	CHECK(!v.IsFade());
	CHECK(v.m_mapCreature.empty());
	CHECK(v.m_mapImageObject.empty());
	CHECK(v.m_pqDrawText.empty());
	CHECK(v.m_listDrawItemName.empty());
	CHECK_EQ(0, v.m_listLoadedCreatureSprite.GetSize());
	CHECK_EQ(0, v.m_listLoadedMonsterSprite.GetSize());
	CHECK_EQ(0u, unsigned(v.m_CreatureFPK.GetSize()));
	CHECK_EQ(0u, unsigned(v.m_CreatureSPK.GetSize()));
	// The creature draw exceptions are gamemodel's table.
	std::map<DWORD, DrawCreatureExceptionProc> exceptions;
	MapCreatureDrawExceptions(exceptions);
	CHECK(!exceptions.empty());
	CHECK(exceptions == Access::Exceptions(v));
}

TEST(TopView1Life, TheSurfaceIsBorrowedAndOutlivesTheView)
{
	Canvas canvas;
	{
		MTopView view;
		CSpriteSurface* p = &canvas.surface;
		view.SetSurface(p);
		CHECK(Access::Surface(view) == &canvas.surface);
		CSpriteSurface* none = nullptr;
		view.SetSurface(none);
		CHECK(Access::Surface(view) == nullptr);
		view.SetSurface(p);
	}
	// The view never deleted the canvas (ASan would stop a delete of it).
	CHECK(canvas.Blank());
	CHECK_EQ(kWidth, canvas.surface.GetWidth());
}

TEST(TopView1Life, ReleaseOnAnEmptyViewIsSafeTwice)
{
	View view;
	Access::Initialised(*view) = true;
	view->Release();
	CHECK(!view->IsInit());
	view->Release();
	CHECK(!view->IsInit());
	CHECK(view->m_pTileSurface == nullptr);
	CHECK(view->m_pqDrawText.empty());
}

namespace {

// Every pointer, list, map and pack Release empties, filled.
struct Owned
{
	CSpriteSurface* tileSurface = new CSpriteSurface;
	TileRenderer* renderer = new TileRenderer;
	CSpriteSurface* lightTexture = new CSpriteSurface;
	int* lightWidth = new int[4];
	int* lightHeight = new int[4];
	CSpriteFilePositionArray* sfpa[4] = {new CSpriteFilePositionArray, new CSpriteFilePositionArray,
		new CSpriteFilePositionArray, new CSpriteFilePositionArray};
	DRAWTEXT_NODE* text[2] = {new DRAWTEXT_NODE(0, 0, "a"), new DRAWTEXT_NODE(0, 0, "b")};
	std::unique_ptr<MItem> item = Item(41);
	DRAWITEMNAME_NODE* name[2] = {nullptr, nullptr};
	CreatureProbe creature{3, 2};

	void Give(MTopView& v)
	{
		name[0] = new DRAWITEMNAME_NODE(1, item.get());
		name[1] = new DRAWITEMNAME_NODE(2, item.get());
		v.m_pTileSurface = tileSurface;
		v.m_pTileRenderer = renderer;
		v.m_pLightBufferTexture = lightTexture;
		v.m_p2DLightPixelWidth = lightWidth;
		v.m_p2DLightPixelHeight = lightHeight;
		v.m_pTileSFPArrayLargeZone = sfpa[0];
		v.m_pImageObjectSFPArrayLargeZone = sfpa[1];
		v.m_pTileSFPArraySmallZone = sfpa[2];
		v.m_pImageObjectSFPArraySmallZone = sfpa[3];
		v.AddText(text[0]);
		v.AddText(text[1]);
		v.m_listDrawItemName.push_back(name[0]);
		v.m_listDrawItemName.push_back(name[1]);
		v.m_mapCreature[7] = &creature;
		v.m_mapImageObject[9] = nullptr;
		v.m_listLoadedCreatureSprite.Add(5);
		v.m_listLoadedMonsterSprite.Add(6);
		for (CCreatureFramePack* pack : CreaturePacks(v)) pack->Init(1);
		for (CEffectFramePack* pack : {&v.m_EffectAlphaFPK, &v.m_EffectNormalFPK, &v.m_EffectShadowFPK, &v.m_EffectScreenFPK})
			pack->Init(1);
		v.m_ItemTileFPK.Init(1);
		for (CAnimationFramePack* pack : {&v.m_ItemDropFPK, &v.m_ItemBrokenFPK, &v.m_ImageObjectFPK, &v.m_ImageObjectShadowFPK})
			pack->Init(1);
		for (CSpritePack* pack : SpritePacks(v)) pack->Init(1);
		for (CIndexSpritePack* pack : IndexPacks(v)) pack->Init(1);
		for (CShadowSpritePack* pack : ShadowPacks(v)) pack->Init(1);
		v.m_EffectAlphaSPK.Init(1);
		v.m_EffectScreenSPK.Init(1);
		v.m_EffectAlphaPPK.Init(1);
		v.m_EffectScreenPPK.Init(1);
		LoadIndex(v.m_TileSPKI);
		LoadIndex(v.m_ImageObjectSPKI);
		v.m_ImageObjectFilter.Init(2, 2);
		v.m_LightFTP.Init(1);
		v.m_LightBufferFilter.Init(2, 2);
		Access::Initialised(v) = true;
	}
	static std::vector<CCreatureFramePack*> CreaturePacks(MTopView& v)
	{
		return {&v.m_CreatureFPK, &v.m_AddonFPK, &v.m_OustersFPK, &v.m_AdvancementSlayerManFPK,
			&v.m_AdvancementSlayerWomanFPK, &v.m_AdvancementVampireManFPK, &v.m_AdvancementVampireWomanFPK,
			&v.m_AdvancementOustersFPK, &v.m_CreatureShadowFPK, &v.m_AddonShadowFPK, &v.m_OustersShadowFPK,
			&v.m_AdvancementSlayerManShadowFPK, &v.m_AdvancementSlayerWomanShadowFPK,
			&v.m_AdvancementVampireManShadowFPK, &v.m_AdvancementVampireWomanShadowFPK,
			&v.m_AdvancementOustersShadowFPK};
	}
	static std::vector<CSpritePack*> SpritePacks(MTopView& v)
	{
		return {&v.m_TileSPK, &v.m_ImageObjectSPK, &v.m_ItemBrokenSPK, &v.m_ItemRealSPK, &v.m_EffectNormalSPK,
			&v.m_WeatherSPK, &v.m_GuildSPK, &v.m_EtcSPK, &v.m_OustersFinSPK, &v.m_AdvacementQuestEnding};
	}
	static std::vector<CIndexSpritePack*> IndexPacks(MTopView& v)
	{
		return {&v.m_CreatureSPK, &v.m_AddonSPK, &v.m_OustersSPK, &v.m_AdvancementSlayerManSPK,
			&v.m_AdvancementSlayerWomanSPK, &v.m_AdvancementVampireManSPK, &v.m_AdvancementVampireWomanSPK,
			&v.m_AdvancementOustersSPK, &v.m_ItemTileISPK, &v.m_ItemDropISPK};
	}
	static std::vector<CShadowSpritePack*> ShadowPacks(MTopView& v)
	{
		return {&v.m_EffectShadowSPK, &v.m_AddonSSPK, &v.m_CreatureSSPK, &v.m_OustersSSPK, &v.m_ImageObjectSSPK,
			&v.m_AdvancementSlayerManSSPK, &v.m_AdvancementSlayerWomanSSPK, &v.m_AdvancementVampireManSSPK,
			&v.m_AdvancementVampireWomanSSPK, &v.m_AdvancementOustersSSPK};
	}
	// Everything the view owned was freed; what it borrowed was not.
	void CheckFreed()
	{
		CHECK(Freed(tileSurface));
		CHECK(Freed(renderer));
		CHECK(Freed(lightTexture));
		CHECK(Freed(lightWidth));
		CHECK(Freed(lightHeight));
		for (auto* p : sfpa) CHECK(Freed(p));
		for (auto* p : text) CHECK(Freed(p));
		for (auto* p : name) CHECK(Freed(p));
		CHECK(Live(item.get()));
	}
};

} // namespace

TEST(TopView1Life, ReleaseFreesWhatTheViewOwnsAndEmptiesEveryPack)
{
	Model model;
	View view;
	MTopView& v = *view;
	Owned owned;
	owned.Give(v);
	v.Release();
	owned.CheckFreed();
	CHECK(!v.IsInit());
	CHECK(v.m_pTileSurface == nullptr);
	CHECK(v.m_pTileRenderer == nullptr);
	CHECK(v.m_pLightBufferTexture == nullptr);
	CHECK(v.m_p2DLightPixelWidth == nullptr);
	CHECK(v.m_p2DLightPixelHeight == nullptr);
	CHECK(v.m_pTileSFPArrayLargeZone == nullptr);
	CHECK(v.m_pImageObjectSFPArrayLargeZone == nullptr);
	CHECK(v.m_pTileSFPArraySmallZone == nullptr);
	CHECK(v.m_pImageObjectSFPArraySmallZone == nullptr);
	CHECK(v.m_pqDrawText.empty());
	CHECK(v.m_listDrawItemName.empty());
	CHECK(v.m_mapCreature.empty());
	CHECK(v.m_mapImageObject.empty());
	CHECK_EQ(0, v.m_listLoadedCreatureSprite.GetSize());
	CHECK_EQ(0, v.m_listLoadedMonsterSprite.GetSize());
	for (CCreatureFramePack* pack : Owned::CreaturePacks(v)) CHECK_EQ(0, int(pack->GetSize()));
	for (CEffectFramePack* pack : {&v.m_EffectAlphaFPK, &v.m_EffectNormalFPK, &v.m_EffectShadowFPK, &v.m_EffectScreenFPK})
		CHECK_EQ(0, int(pack->GetSize()));
	CHECK_EQ(0, int(v.m_ItemTileFPK.GetSize()));
	CHECK_EQ(0, int(v.m_ItemDropFPK.GetSize()));
	CHECK_EQ(0, int(v.m_ImageObjectFPK.GetSize()));
	CHECK_EQ(0, int(v.m_ImageObjectShadowFPK.GetSize()));
	// Recorded defect: Release never releases m_ItemBrokenFPK (nor
	// m_EffectResources); only the destructors of the members free them.
	CHECK_EQ(1, int(v.m_ItemBrokenFPK.GetSize()));
	for (CSpritePack* pack : Owned::SpritePacks(v)) CHECK_EQ(0u, unsigned(pack->GetSize()));
	for (CIndexSpritePack* pack : Owned::IndexPacks(v)) CHECK_EQ(0u, unsigned(pack->GetSize()));
	for (CShadowSpritePack* pack : Owned::ShadowPacks(v)) CHECK_EQ(0u, unsigned(pack->GetSize()));
	CHECK_EQ(0u, unsigned(v.m_EffectAlphaSPK.GetSize()));
	CHECK_EQ(0u, unsigned(v.m_EffectScreenSPK.GetSize()));
	CHECK_EQ(0u, unsigned(v.m_EffectAlphaPPK.GetSize()));
	CHECK_EQ(0u, unsigned(v.m_EffectScreenPPK.GetSize()));
	CHECK_EQ(0, int(v.m_TileSPKI.GetSize()));
	CHECK_EQ(0, int(v.m_ImageObjectSPKI.GetSize()));
	CHECK(v.m_ImageObjectFilter.IsNotInit());
	CHECK_EQ(0, int(v.m_LightFTP.GetSize()));
	CHECK(v.m_LightBufferFilter.IsNotInit());
	// A second Release frees nothing twice.
	v.Release();
	CHECK(v.m_pTileSurface == nullptr);
}

TEST(TopView1Life, TheDestructorReleasesWhatTheViewOwns)
{
	Model model;
	Owned owned;
	{
		View view;
		owned.Give(*view);
	}
	owned.CheckFreed();
}

//----------------------------------------------------------------------
// The zone and the simple state
//----------------------------------------------------------------------
TEST(TopView1State, SetZoneRedrawsTheTilesAndForgetsTheZonesCreaturesAndItemNames)
{
	Model model;
	View view;
	MTopView& v = *view;
	CreatureProbe creature(4, 2);
	std::unique_ptr<MItem> item = Item(30);
	DRAWITEMNAME_NODE* name = new DRAWITEMNAME_NODE(1, item.get());
	DRAWTEXT_NODE* text = new DRAWTEXT_NODE(0, 0, "x");
	v.m_listDrawItemName.push_back(name);
	v.m_mapCreature[1] = &creature;
	v.m_mapImageObject[2] = nullptr;
	v.AddText(text);
	v.m_bFirstTileDraw = false;
	v.SetZone(&model.zone);
	CHECK(v.m_pZone == &model.zone);
	CHECK(v.m_bFirstTileDraw);
	CHECK(v.m_mapCreature.empty());
	CHECK(v.m_listDrawItemName.empty());
	CHECK(Freed(name));
	// The image objects and the queued text stay.
	CHECK_EQ(1, int(v.m_mapImageObject.size()));
	CHECK_EQ(1, int(v.m_pqDrawText.size()));
	CHECK(Live(text));
	v.m_bFirstTileDraw = false;
	v.SetZone(nullptr);
	CHECK(v.m_pZone == nullptr);
	CHECK(v.m_bFirstTileDraw);
}

TEST(TopView1State, SelectingOneKindOfObjectDropsTheOthers)
{
	View view;
	MTopView& v = *view;
	v.SetSelectedCreature(11);
	CHECK_EQ(TYPE_OBJECTID(11), v.GetSelectedCreature());
	v.m_SelectItemID = 12;
	v.m_SelectEffectID = 13;
	v.SetSelectedCreature(14);
	CHECK_EQ(TYPE_OBJECTID(14), v.GetSelectedCreature());
	CHECK_EQ(TYPE_OBJECTID(OBJECTID_NULL), v.GetSelectedItemID());
	CHECK_EQ(TYPE_OBJECTID(OBJECTID_NULL), v.m_SelectEffectID);
	v.m_SelectEffectID = 13;
	v.SetSelectedItem(15);
	CHECK_EQ(TYPE_OBJECTID(15), v.GetSelectedItemID());
	CHECK_EQ(TYPE_OBJECTID(OBJECTID_NULL), v.GetSelectedCreature());
	CHECK_EQ(TYPE_OBJECTID(OBJECTID_NULL), v.m_SelectEffectID);
	v.m_SelectCreatureID = 16;
	v.SetSelectedEffect(17);
	CHECK_EQ(TYPE_OBJECTID(17), v.m_SelectEffectID);
	CHECK_EQ(TYPE_OBJECTID(OBJECTID_NULL), v.GetSelectedCreature());
	CHECK_EQ(TYPE_OBJECTID(OBJECTID_NULL), v.GetSelectedItemID());
	v.m_SelectCreatureID = 18;
	v.m_SelectItemID = 19;
	v.SetSelectedNULL();
	CHECK_EQ(TYPE_OBJECTID(OBJECTID_NULL), v.GetSelectedCreature());
	CHECK_EQ(TYPE_OBJECTID(OBJECTID_NULL), v.GetSelectedItemID());
	CHECK_EQ(TYPE_OBJECTID(OBJECTID_NULL), v.m_SelectEffectID);
}

TEST(TopView1State, TheSelectedSectorIsSetByCoordinatesOrPointAndCleared)
{
	View view;
	MTopView& v = *view;
	v.SetSelectedSector(3, 4);
	CHECK_EQ(3, int(v.m_SelectSector.x));
	CHECK_EQ(4, int(v.m_SelectSector.y));
	POINT p = {5, 6};
	v.SetSelectedSector(p);
	CHECK_EQ(5, int(v.m_SelectSector.x));
	CHECK_EQ(6, int(v.m_SelectSector.y));
	v.SetSelectedSectorNULL();
	CHECK_EQ(int(SECTORPOSITION_NULL), int(v.m_SelectSector.x));
	CHECK_EQ(int(SECTORPOSITION_NULL), int(v.m_SelectSector.y));
}

TEST(TopView1State, DarkBitsTakeOnlyTheSixteenLevels)
{
	View view;
	view->SetDarkBits(15);
	CHECK_EQ(15, int(view->GetDarkBits()));
	view->SetDarkBits(16);
	CHECK_EQ(15, int(view->GetDarkBits()));
	view->SetDarkBits(255);
	CHECK_EQ(15, int(view->GetDarkBits()));
	view->SetDarkBits(0);
	CHECK_EQ(0, int(view->GetDarkBits()));
}

TEST(TopView1State, FlagsAndModes)
{
	View view;
	MTopView& v = *view;
	v.SetDrawItemNameList();
	CHECK(v.IsDrawItemNameList());
	v.UnSetDrawItemNameList();
	CHECK(!v.IsDrawItemNameList());
	v.m_bFirstTileDraw = false;
	v.SetFirstDraw();
	CHECK(v.m_bFirstTileDraw);
	v.m_bDrawRequest = true;
	CHECK(v.IsDrawRequest());
	Access::Initialised(v) = true;
	CHECK(v.IsInit());
	v.SetFadeStart(0, 31, 1, 3, 40, 17);
	CHECK(v.IsFade());
	CHECK_EQ(WORD((3 << 11) | (40 << 5) | 17), v.m_FadeColor);
	v.SetFadeEnd();
	CHECK(!v.IsFade());
}

TEST(TopView1State, OutputKeysPutTheRowAboveTheID)
{
	Model model;
	View view;
	CreatureProbe creature(0x1234, 3, 5);	// pixel y 3*24+5
	CHECK_EQ((QWORD(77) << 32) | 0x1234, view->GetOutputCreatureID(&creature));
	CHECK_EQ((QWORD(7) << 32) | 9, view->GetOutputCreatureID(9, 7));
	CHECK_EQ((QWORD(0xFFFFFFFFu) << 32) | 9, view->GetOutputCreatureID(9, -1));
	MImageObject object;
	object.SetID(0x55);
	object.SetViewpoint(6);
	CHECK_EQ((QWORD(6) << 32) | 0x55, view->GetOutputImageObjectID(&object));
}

TEST(TopView1State, TheCreatureFramesAndSpritesAreTheViewsPacks)
{
	View view;
	MTopView& v = *view;
	v.m_CreatureFPK.Init(2);
	v.m_CreatureFPK[1].Init(3);
	v.m_CreatureFPK[1][2].Init(4);
	v.m_CreatureFPK[1][2][3].Init(5);
	CHECK(&v.GetCreatureFrameArray(1, 2, 3) == &v.m_CreatureFPK[1][2][3]);
	CHECK_EQ(5, int(v.GetCreatureFrameArray(1, 2, 3).GetSize()));
	v.m_CreatureSPK.Init(3);
	CHECK(v.GetCreatureSprite(2) == &v.m_CreatureSPK[2]);
	// Past the pack: the pack's shared empty sprite.
	CHECK(v.GetCreatureSprite(3) == &v.m_CreatureSPK[3]);
	CHECK(v.GetCreatureSprite(3)->IsNotInit());
}

TEST(TopView1State, TheCreatureDrawPacksAreTheViewsOwn)
{
	View view;
	MTopView& v = *view;
	v.SetSelectedCreature(77);
	const CreatureDrawPacks packs = Access::Packs(v);
	CHECK(packs.pSurface == &view.canvas.surface);
	CHECK(packs.pCreatureFPK == &v.m_CreatureFPK);
	CHECK(packs.pCreatureSPK == &v.m_CreatureSPK);
	CHECK(packs.pAddonFPK == &v.m_AddonFPK);
	CHECK(packs.pAddonSPK == &v.m_AddonSPK);
	CHECK(packs.pOustersFPK == &v.m_OustersFPK);
	CHECK(packs.pOustersSPK == &v.m_OustersSPK);
	CHECK(packs.pAdvancementSlayerManFPK == &v.m_AdvancementSlayerManFPK);
	CHECK(packs.pAdvancementSlayerManSPK == &v.m_AdvancementSlayerManSPK);
	CHECK(packs.pAdvancementSlayerWomanFPK == &v.m_AdvancementSlayerWomanFPK);
	CHECK(packs.pAdvancementSlayerWomanSPK == &v.m_AdvancementSlayerWomanSPK);
	CHECK(packs.pAdvancementVampireManFPK == &v.m_AdvancementVampireManFPK);
	CHECK(packs.pAdvancementVampireManSPK == &v.m_AdvancementVampireManSPK);
	CHECK(packs.pAdvancementVampireWomanFPK == &v.m_AdvancementVampireWomanFPK);
	CHECK(packs.pAdvancementVampireWomanSPK == &v.m_AdvancementVampireWomanSPK);
	CHECK(packs.pAdvancementOustersFPK == &v.m_AdvancementOustersFPK);
	CHECK(packs.pAdvancementOustersSPK == &v.m_AdvancementOustersSPK);
	CHECK(packs.pCreatureShadowFPK == &v.m_CreatureShadowFPK);
	CHECK(packs.pCreatureSSPK == &v.m_CreatureSSPK);
	CHECK(packs.pAddonShadowFPK == &v.m_AddonShadowFPK);
	CHECK(packs.pAddonSSPK == &v.m_AddonSSPK);
	CHECK(packs.pOustersShadowFPK == &v.m_OustersShadowFPK);
	CHECK(packs.pOustersSSPK == &v.m_OustersSSPK);
	CHECK(packs.pAdvancementSlayerManShadowFPK == &v.m_AdvancementSlayerManShadowFPK);
	CHECK(packs.pAdvancementSlayerManSSPK == &v.m_AdvancementSlayerManSSPK);
	CHECK(packs.pAdvancementSlayerWomanShadowFPK == &v.m_AdvancementSlayerWomanShadowFPK);
	CHECK(packs.pAdvancementSlayerWomanSSPK == &v.m_AdvancementSlayerWomanSSPK);
	CHECK(packs.pAdvancementVampireManShadowFPK == &v.m_AdvancementVampireManShadowFPK);
	CHECK(packs.pAdvancementVampireManSSPK == &v.m_AdvancementVampireManSSPK);
	CHECK(packs.pAdvancementVampireWomanShadowFPK == &v.m_AdvancementVampireWomanShadowFPK);
	CHECK(packs.pAdvancementVampireWomanSSPK == &v.m_AdvancementVampireWomanSSPK);
	CHECK(packs.pAdvancementOustersShadowFPK == &v.m_AdvancementOustersShadowFPK);
	CHECK(packs.pAdvancementOustersSSPK == &v.m_AdvancementOustersSSPK);
	CHECK(packs.pEtcSPK == &v.m_EtcSPK);
	CHECK_EQ(TYPE_OBJECTID(77), packs.selectedCreatureID);
	CHECK(packs.pOutlines == &v.m_SOM);
}

//----------------------------------------------------------------------
// The file-free Init steps
//----------------------------------------------------------------------
TEST(TopView1Init, ColorsComeFromTheConfiguration)
{
	GameModelWorld world;
	View view;
	MTopView& v = *view;
	ClientConfig& c = *g_pClientConfig;
	c.COLOR_NAME_ITEM = 0x010101;
	c.COLOR_NAME_ITEM_OPTION = 0x020202;
	c.COLOR_NAME_VAMPIRE = 0x030303;
	c.COLOR_NAME_SLAYER = 0x040404;
	c.COLOR_NAME_NPC = 0x050505;
	c.COLOR_NAME_EVIL_MORE = 0x060606;
	c.COLOR_NAME_EVIL = 0x070707;
	c.COLOR_NAME_NEUTRAL = 0x080808;
	c.COLOR_NAME_GOOD = 0x090909;
	c.COLOR_NAME_GOOD_MORE = 0x0A0A0A;
	c.COLOR_OUTLINE_ITEM = 0x1111;
	c.COLOR_OUTLINE_NPC = 0x2222;
	c.COLOR_OUTLINE_ATTACK_POSSIBLE = 0x3333;
	c.COLOR_OUTLINE_ATTACK_IMPOSSIBLE = 0x4444;
	c.COLOR_HP_BAR_R = 1; c.COLOR_HP_BAR_G = 2; c.COLOR_HP_BAR_B = 3;
	c.COLOR_HP_BAR_BG_R = 4; c.COLOR_HP_BAR_BG_G = 5; c.COLOR_HP_BAR_BG_B = 6;
	v.m_ColorBlackHalf = 0x7777;
	CHECK(Access::InitColors(v));
	CHECK_EQ(COLORREF(0x010101), v.m_ColorNameItem);
	CHECK_EQ(COLORREF(0x020202), v.m_ColorNameItemOption);
	CHECK_EQ(COLORREF(0x030303), v.m_ColorNameVampire);
	CHECK_EQ(COLORREF(0x040404), v.m_ColorNameSlayer);
	CHECK_EQ(COLORREF(0x050505), v.m_ColorNameNPC);
	CHECK_EQ(COLORREF(0x060606), v.m_ColorNameAlignment[0]);
	CHECK_EQ(COLORREF(0x070707), v.m_ColorNameAlignment[1]);
	CHECK_EQ(COLORREF(0x080808), v.m_ColorNameAlignment[2]);
	CHECK_EQ(COLORREF(0x090909), v.m_ColorNameAlignment[3]);
	CHECK_EQ(COLORREF(0x0A0A0A), v.m_ColorNameAlignment[4]);
	// 5:6:5 (the only format): the outline colours as configured.
	CHECK_EQ(WORD(0x1111), v.m_ColorOutlineItem);
	CHECK_EQ(WORD(0x2222), v.m_ColorOutlineNPC);
	CHECK_EQ(WORD(0x3333), v.m_ColorOutlineAttackPossible);
	CHECK_EQ(WORD(0x4444), v.m_ColorOutlineAttackImpossible);
	CHECK_EQ(WORD((1 << 11) | (2 << 5) | 3), v.m_ColorHPBar);
	CHECK_EQ(WORD((4 << 11) | (5 << 5) | 6), v.m_ColorHPBarBg);
	CHECK_EQ(WORD((12 << 11) | (24 << 5) | 12), v.m_ColorUIBoxBg);
	CHECK_EQ(WORD(0), v.m_ColorBlackHalf);
}

namespace {
int s_titleLoadings = 0;
void CountTitleLoading() { ++s_titleLoadings; }
}

TEST(TopView1Init, SurfacesMakeANewTileSurfaceAndDropTheRenderer)
{
	const LONG oldWidth = g_TILESURFACE_WIDTH, oldHeight = g_TILESURFACE_HEIGHT;
	g_TILESURFACE_WIDTH = 24;
	g_TILESURFACE_HEIGHT = 16;
	WorldEvents::Host host{};
	host.DrawTitleLoading = &CountTitleLoading;
	const WorldEvents::Host* oldHost = WorldEvents::SetHost(&host);
	s_titleLoadings = 0;
	CHECK(!g_bDrawProgress);	// so UI_DrawProgress only logs
	{
		View view;
		MTopView& v = *view;
		CSpriteSurface* old = new CSpriteSurface;
		TileRenderer* renderer = new TileRenderer;
		v.m_pTileSurface = old;
		v.m_pTileRenderer = renderer;
		CHECK(Access::InitSurfaces(v));
		CHECK(Freed(old));
		CHECK(Freed(renderer));
		CHECK(v.m_pTileRenderer == nullptr);
		CHECK(v.m_pTileSurface != nullptr);
		if (v.m_pTileSurface != nullptr)
		{
			CHECK_EQ(24, v.m_pTileSurface->GetWidth());
			CHECK_EQ(16, v.m_pTileSurface->GetHeight());
		}
		CHECK_EQ(1, s_titleLoadings);
		// Again, with no renderer: the surface is replaced once more.
		CSpriteSurface* second = v.m_pTileSurface;
		CHECK(Access::InitSurfaces(v));
		CHECK(Freed(second));
		CHECK_EQ(2, s_titleLoadings);
	}
	WorldEvents::SetHost(oldHost);
	g_TILESURFACE_WIDTH = oldWidth;
	g_TILESURFACE_HEIGHT = oldHeight;
}

TEST(TopView1Init, FontsAreTheUIBasesPrintInfos)
{
	Base* oldBase = gpC_base;
	PrintInfo* old[kFonts];
	for (int i = 0; i < kFonts; ++i) old[i] = g_ClientPrintInfo[i];
	Base base;
	gpC_base = &base;
	{
		View view;
		CHECK(Access::InitFonts(*view));
	}
	CHECK(g_ClientPrintInfo[0] == &base.m_info_pi);
	CHECK(g_ClientPrintInfo[1] == &base.m_char_name_pi);
	CHECK(g_ClientPrintInfo[2] == &base.m_char_name_pi);
	CHECK(g_ClientPrintInfo[3] == &base.m_char_name_pi);
	CHECK(g_ClientPrintInfo[4] == &base.m_char_chat_pi);
	CHECK(g_ClientPrintInfo[5] == &base.m_char_chat_pi);
	CHECK(g_ClientPrintInfo[6] == &base.m_char_chat_pi);
	CHECK(g_ClientPrintInfo[7] == &base.m_item_pi);
	CHECK(g_ClientPrintInfo[8] == &base.m_char_chat_large_pi);
	CHECK(g_ClientPrintInfo[9] == &base.m_user_id_pi);
	for (int i = 0; i < kFonts; ++i) g_ClientPrintInfo[i] = old[i];
	gpC_base = oldBase;
}

//----------------------------------------------------------------------
// The effect queries
//----------------------------------------------------------------------
TEST(TopView1Effects, AnEffectsSpriteTypeIsItsFrameAfterTheTypesBefore)
{
	View view;
	for (int frameID : {0, 1, 57, 300})
	{
		const TYPE_FRAMEID id = TYPE_FRAMEID(frameID);
		CHECK_EQ(frameID, view->GetEffectSpriteType(BLT_EFFECT, id));
		CHECK_EQ(frameID + int(MAX_EFFECTSPRITETYPE_ALPHAEFFECT), view->GetEffectSpriteType(BLT_SCREEN, id));
		CHECK_EQ(frameID + int(MAX_EFFECTSPRITETYPE_SCREENEFFECT), view->GetEffectSpriteType(BLT_NORMAL, id));
		CHECK_EQ(frameID + int(MAX_EFFECTSPRITETYPE_NORMALEFFECT), view->GetEffectSpriteType(BLT_SHADOW, id));
		// The same as gamemodel's helper the sectors use.
		for (BLT_TYPE type : {BLT_NORMAL, BLT_EFFECT, BLT_SHADOW, BLT_SCREEN})
			CHECK_EQ(GetEffectSpriteType(type, id), view->GetEffectSpriteType(type, id));
	}
}

namespace {

// Two frame IDs; frame ID 1 has `frames` frames in direction 0 and one
// more in direction 1; each frame's light is base + direction*10 + frame.
void ShapeEffect(CEffectFramePack& pack, int frames, int base)
{
	pack.Init(2);
	pack[0].Init(1);
	pack[0][0].Init(1);
	pack[1].Init(2);
	for (int direction = 0; direction < 2; ++direction)
	{
		pack[1][BYTE(direction)].Init(WORD(frames + direction));
		for (int frame = 0; frame < frames + direction; ++frame)
			pack[1][BYTE(direction)][WORD(frame)].Set(0, 0, 0, char(base + direction * 10 + frame), false);
	}
}

struct Effects : View
{
	Effects()
	{
		ShapeEffect((*this)->m_EffectNormalFPK, 3, 20);
		ShapeEffect((*this)->m_EffectAlphaFPK, 4, 40);
		ShapeEffect((*this)->m_EffectShadowFPK, 5, 60);
		ShapeEffect((*this)->m_EffectScreenFPK, 6, 80);
	}
};

} // namespace

TEST(TopView1Effects, TheMostFramesAreDirectionZerosInTheTypesOwnPack)
{
	Effects view;
	CHECK_EQ(3, view->GetMaxEffectFrame(BLT_NORMAL, 1));
	CHECK_EQ(4, view->GetMaxEffectFrame(BLT_EFFECT, 1));
	CHECK_EQ(5, view->GetMaxEffectFrame(BLT_SHADOW, 1));
	CHECK_EQ(6, view->GetMaxEffectFrame(BLT_SCREEN, 1));
	CHECK_EQ(1, view->GetMaxEffectFrame(BLT_SCREEN, 0));
	// A frame ID past the pack has none.
	for (BLT_TYPE type : {BLT_NORMAL, BLT_EFFECT, BLT_SHADOW, BLT_SCREEN})
	{
		CHECK_EQ(0, view->GetMaxEffectFrame(type, 2));
		CHECK_EQ(0, view->GetMaxEffectFrame(type, 0xFFFF));
	}
	View empty;
	CHECK_EQ(0, empty->GetMaxEffectFrame(BLT_EFFECT, 0));
}

TEST(TopView1Effects, AFramesLightIsReadFromTheTypesOwnPackAndZeroPastIt)
{
	Effects view;
	CHECK_EQ(20, view->GetEffectLight(BLT_NORMAL, 1, 0, 0));
	CHECK_EQ(42, view->GetEffectLight(BLT_EFFECT, 1, 0, 2));
	CHECK_EQ(74, view->GetEffectLight(BLT_SHADOW, 1, 1, 4));
	CHECK_EQ(95, view->GetEffectLight(BLT_SCREEN, 1, 1, 5));
	// The last frame of each direction, and one past it.
	for (BLT_TYPE type : {BLT_NORMAL, BLT_EFFECT, BLT_SHADOW, BLT_SCREEN})
	{
		const int frames = view->GetMaxEffectFrame(type, 1);
		const int base = frames * 20 - 40;
		CHECK_EQ(base + frames - 1, view->GetEffectLight(type, 1, 0, frames - 1));
		CHECK_EQ(0, view->GetEffectLight(type, 1, 0, frames));
		CHECK_EQ(base + 10 + frames, view->GetEffectLight(type, 1, 1, frames));
		CHECK_EQ(0, view->GetEffectLight(type, 1, 1, frames + 1));
		CHECK_EQ(0, view->GetEffectLight(type, 1, 2, 0));		// direction past the frame ID's
		CHECK_EQ(0, view->GetEffectLight(type, 1, 300, 0));
		CHECK_EQ(0, view->GetEffectLight(type, 2, 0, 0));		// frame ID past the pack
	}
}

//----------------------------------------------------------------------
// The direction table and the random monster
//----------------------------------------------------------------------
TEST(TopView1Queries, ADirectionsStepIsItsTileOffset)
{
	const int expected[8][2] = {{-1, 0}, {-1, 1}, {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}};
	for (int direction = 0; direction < 8; ++direction)
	{
		const POINT step = MTopView::GetChangeValueToDirection(direction);
		CHECK_EQ(expected[direction][0], int(step.x));
		CHECK_EQ(expected[direction][1], int(step.y));
	}
	for (int direction : {-1, 8, 9, 100})
	{
		const POINT step = MTopView::GetChangeValueToDirection(direction);
		CHECK_EQ(0, int(step.x));
		CHECK_EQ(0, int(step.y));
	}
}

namespace {

struct Mapper
{
	CreatureSpriteTypeMapper mapper;
	CreatureSpriteTypeMapper* old = g_pCreatureSpriteTypeMapper;
	Mapper()
	{
		mapper.Init(40);
		mapper.AddCreatureType(27, 501);
		mapper.AddCreatureType(5, 605);
		mapper.AddCreatureType(6, 606);
		mapper.AddCreatureType(9, 609);
		g_pCreatureSpriteTypeMapper = &mapper;
	}
	~Mapper() { g_pCreatureSpriteTypeMapper = old; mapper.Release(); }
};

} // namespace

TEST(TopView1Queries, WithNoMonsterLoadedTheRandomMonsterIsTheBloodWarlocks)
{
	Mapper mapper;
	View view;
	for (int i = 0; i < 10; ++i) CHECK_EQ(501, view->GetRandomMonsterTypeInZone());
	view->m_listLoadedMonsterSprite.Add(5);
	for (int i = 0; i < 10; ++i) CHECK_EQ(605, view->GetRandomMonsterTypeInZone());
}

TEST(TopView1Queries, TheRandomMonsterIsAnyOfTheLoadedMonsterSprites)
{
	Mapper mapper;
	View view;
	for (int sprite : {5, 6, 9}) view->m_listLoadedMonsterSprite.Add(sprite);
	std::set<int> seen;
	for (int i = 0; i < 300; ++i) seen.insert(view->GetRandomMonsterTypeInZone());
	CHECK(seen == std::set<int>({605, 606, 609}));
}

//----------------------------------------------------------------------
// The item-name list
//----------------------------------------------------------------------
TEST(TopView1ItemNames, NamesAreKeptNearestFirstUpToTheConfiguredMost)
{
	Model model;
	g_pClientConfig->MAX_DRAWITEMNAME = 3;
	View view;
	MTopView& v = *view;
	std::unique_ptr<MItem> item = Item(1);
	DRAWITEMNAME_NODE* n5 = new DRAWITEMNAME_NODE(5, item.get());
	DRAWITEMNAME_NODE* n2 = new DRAWITEMNAME_NODE(2, item.get());
	DRAWITEMNAME_NODE* n9 = new DRAWITEMNAME_NODE(9, item.get());
	v.AddItemName(n5);
	v.AddItemName(n2);
	v.AddItemName(n9);
	CHECK(Names(v) == std::vector<DRAWITEMNAME_NODE*>({n2, n5, n9}));
	// An equal distance goes after the one there; the farthest goes.
	DRAWITEMNAME_NODE* n5b = new DRAWITEMNAME_NODE(5, item.get());
	v.AddItemName(n5b);
	CHECK(Names(v) == std::vector<DRAWITEMNAME_NODE*>({n2, n5, n5b}));
	CHECK(Freed(n9));
	// The farthest can be the name just added: the caller's node is freed.
	DRAWITEMNAME_NODE* n7 = new DRAWITEMNAME_NODE(7, item.get());
	v.AddItemName(n7);
	CHECK(Names(v) == std::vector<DRAWITEMNAME_NODE*>({n2, n5, n5b}));
	CHECK(Freed(n7));
	DRAWITEMNAME_NODE* n0 = new DRAWITEMNAME_NODE(0, item.get());
	v.AddItemName(n0);
	CHECK(Names(v) == std::vector<DRAWITEMNAME_NODE*>({n0, n2, n5}));
	CHECK(Freed(n5b));
	v.ClearItemNameList();
	CHECK(v.m_listDrawItemName.empty());
	for (DRAWITEMNAME_NODE* node : {n0, n2, n5}) CHECK(Freed(node));
}

TEST(TopView1ItemNames, WithRoomForNoNameEveryNameIsFreedAtOnce)
{
	Model model;
	g_pClientConfig->MAX_DRAWITEMNAME = 0;
	View view;
	std::unique_ptr<MItem> item = Item(1);
	DRAWITEMNAME_NODE* node = new DRAWITEMNAME_NODE(1, item.get());
	view->AddItemName(node);
	CHECK(view->m_listDrawItemName.empty());
	CHECK(Freed(node));
}

TEST(TopView1ItemNames, ADistanceOfTwoToTheThirtyFirstSortsFirst)
{
	Model model;
	g_pClientConfig->MAX_DRAWITEMNAME = 5;
	View view;
	std::unique_ptr<MItem> item = Item(1);
	DRAWITEMNAME_NODE* nearName = new DRAWITEMNAME_NODE(2, item.get());
	DRAWITEMNAME_NODE* farName = new DRAWITEMNAME_NODE(0x80000000u, item.get());
	view->AddItemName(nearName);
	view->AddItemName(farName);
	// Recorded defect: GetDistance() narrows the DWORD distance to int, so
	// a distance of 2^31 or more counts as negative and sorts first.
	CHECK(Names(*view) == std::vector<DRAWITEMNAME_NODE*>({farName, nearName}));
	CHECK_EQ(INT_MIN, farName->GetDistance());
}

TEST(TopView1ItemNames, APointPicksTheFirstNameWhoseBoxHoldsIt)
{
	Model model;
	g_pClientConfig->MAX_DRAWITEMNAME = 5;
	View view;
	MTopView& v = *view;
	std::unique_ptr<MItem> a = Item(100), b = Item(200), gone = Item(300);
	model.zone.m_mapItem[100] = a.get();
	model.zone.m_mapItem[200] = b.get();
	v.SetZone(&model.zone);
	DRAWITEMNAME_NODE* na = new DRAWITEMNAME_NODE(1, a.get());
	DRAWITEMNAME_NODE* nb = new DRAWITEMNAME_NODE(2, b.get());
	DRAWITEMNAME_NODE* ngone = new DRAWITEMNAME_NODE(0, gone.get());
	na->SetRect(RECT{10, 10, 20, 15});
	nb->SetRect(RECT{15, 12, 30, 20});
	ngone->SetRect(RECT{25, 18, 40, 30});
	v.AddItemName(na);
	v.AddItemName(nb);
	v.AddItemName(ngone);
	CHECK(v.SelectItemName(10, 10) == a.get());		// left and top are in
	CHECK(v.SelectItemName(19, 14) == a.get());
	CHECK(v.SelectItemName(9, 10) == nullptr);
	CHECK(v.SelectItemName(10, 9) == nullptr);
	CHECK(v.SelectItemName(16, 13) == a.get());		// in both: the nearer
	CHECK(v.SelectItemName(20, 14) == b.get());		// right is out
	CHECK(v.SelectItemName(17, 15) == b.get());		// bottom is out
	CHECK(v.SelectItemName(24, 19) == b.get());
	CHECK(v.SelectItemName(30, 13) == nullptr);
	CHECK(v.SelectItemName(16, 20) == nullptr);
	// The nearest box holding the point decides, though its item left the
	// zone and a farther box holds the point too.
	CHECK(v.SelectItemName(27, 19) == nullptr);
	CHECK(v.SelectItemName(35, 25) == nullptr);
	CHECK(v.SelectItemName(50, 50) == nullptr);
}

//----------------------------------------------------------------------
// The text queue
//----------------------------------------------------------------------
TEST(TopView1Text, TextIsStampedAfterThisFrameInTheOrderAddedUnlessTimed)
{
	Model model;
	View view;
	MTopView& v = *view;
	g_CurrentFrame = 10;
	DRAWTEXT_NODE* a = new DRAWTEXT_NODE(0, 0, "a");
	DRAWTEXT_NODE* b = new DRAWTEXT_NODE(0, 0, "b");
	DRAWTEXT_NODE* timed = new DRAWTEXT_NODE(0, 0, "c");
	timed->SetTextTime(5);
	v.AddText(a);
	v.AddText(b);
	v.AddText(timed);
	CHECK_EQ(QWORD(170) << 41, a->GetTextTime());
	CHECK_EQ((QWORD(170) << 41) + 1, b->GetTextTime());
	CHECK_EQ(QWORD(5), timed->GetTextTime());
	// The oldest first.
	const std::vector<DRAWTEXT_NODE*> order = Drain(v);
	CHECK(order == std::vector<DRAWTEXT_NODE*>({timed, a, b}));
	for (DRAWTEXT_NODE* node : order) delete node;
}

TEST(TopView1Text, TheStampKeepsOnlyTheFramesLowTwentyThreeBits)
{
	Model model;
	View view;
	MTopView& v = *view;
	v.AddText(new DRAWTEXT_NODE(0, 0, "first"));
	// Recorded defect: (frame + 160) << 41 drops the frame's bits above the
	// 23rd, so after 2^23 frames text stamps start again from 0 and sort
	// before older text; DWORD addition also wraps the frame + 160.
	g_CurrentFrame = (1u << 23) - 160;
	DRAWTEXT_NODE* wrapped = new DRAWTEXT_NODE(0, 0, "w");
	v.AddText(wrapped);
	CHECK_EQ(QWORD(1), wrapped->GetTextTime());
	g_CurrentFrame = 0xFFFFFFFFu;
	DRAWTEXT_NODE* last = new DRAWTEXT_NODE(0, 0, "l");
	v.AddText(last);
	CHECK_EQ((QWORD(159) << 41) + 2, last->GetTextTime());
	v.ClearTextList();
}

TEST(TopView1Text, TheOlderTextComesFirstAndEqualTimesAreNotOrdered)
{
	DRAWTEXT_NODE older(0, 0, "o"), newer(0, 0, "n"), same(0, 0, "s");
	older.SetTextTime(1);
	newer.SetTextTime(2);
	same.SetTextTime(2);
	TextComparison compare;
	CHECK(!compare(&older, &newer));
	CHECK(compare(&newer, &older));
	// Recorded defect: >= is not a strict weak ordering - each of two
	// equal times is "less" than the other, which a priority queue may not
	// be given.
	CHECK(compare(&newer, &same));
	CHECK(compare(&same, &newer));
}

TEST(TopView1Text, ClearingTheQueueFreesEveryText)
{
	Model model;
	View view;
	DRAWTEXT_NODE* nodes[3] = {new DRAWTEXT_NODE(0, 0, "a"), new DRAWTEXT_NODE(0, 0, "b"), new DRAWTEXT_NODE(0, 0, "c")};
	for (DRAWTEXT_NODE* node : nodes) view->AddText(node);
	view->ClearTextList();
	CHECK(view->m_pqDrawText.empty());
	for (DRAWTEXT_NODE* node : nodes) CHECK(Freed(node));
	view->ClearTextList();
}

//----------------------------------------------------------------------
// The output-creature map
//----------------------------------------------------------------------
TEST(TopView1Creatures, ACreatureIsAddedOnceUnderItsRowAndID)
{
	Model model;
	View view;
	MTopView& v = *view;
	CreatureProbe a(7, 3), b(5, 1);
	CHECK(v.AddOutputCreature(&a));
	CHECK(v.AddOutputCreature(&b));
	CHECK(!v.AddOutputCreature(&a));
	CHECK_EQ(2, int(v.m_mapCreature.size()));
	CHECK(v.m_mapCreature.begin()->first == Key(5, 24));
	CHECK(v.m_mapCreature.begin()->second == &b);
	CHECK(v.m_mapCreature.rbegin()->first == Key(7, 72));
	CHECK(v.m_mapCreature.rbegin()->second == &a);
	v.ClearOutputCreature();
	CHECK(v.m_mapCreature.empty());
}

TEST(TopView1Creatures, ACreatureAboveTheZonesTopRowIsDrawnLast)
{
	Model model;
	View view;
	CreatureProbe low(1, 7), high(2, 0, -3);
	CHECK(view->AddOutputCreature(&low));
	CHECK(view->AddOutputCreature(&high));
	// Recorded defect: a negative pixel y becomes a key above every row's.
	CHECK(view->m_mapCreature.rbegin()->second == &high);
	CHECK(view->m_mapCreature.rbegin()->first == Key(2, -3));
}

TEST(TopView1Creatures, RemovingFindsTheCreatureOnlyWhereItWasAdded)
{
	Model model;
	View view;
	MTopView& v = *view;
	CreatureProbe a(7, 3), b(5, 1);
	v.AddOutputCreature(&a);
	v.AddOutputCreature(&b);
	CHECK(v.RemoveOutputCreature(&b));
	CHECK(!v.RemoveOutputCreature(&b));
	CHECK_EQ(1, int(v.m_mapCreature.size()));
	// Recorded defect: once the creature moved, Remove misses it and the
	// map keeps the old row's entry.
	a.m_sY = 4;
	CHECK(!v.RemoveOutputCreature(&a));
	CHECK_EQ(1, int(v.m_mapCreature.size()));
	CHECK(v.m_mapCreature.begin()->first == Key(7, 72));
}

TEST(TopView1Creatures, UpdatingMovesTheEntryToTheNewRow)
{
	Model model;
	View view;
	MTopView& v = *view;
	CreatureProbe a(7, 3), b(5, 1);
	v.AddOutputCreature(&a);
	v.AddOutputCreature(&b);
	CHECK(v.UpdateOutputCreature(7, 72, 10));
	CHECK_EQ(2, int(v.m_mapCreature.size()));
	CHECK(v.m_mapCreature.begin()->first == Key(7, 10));
	CHECK(v.m_mapCreature.begin()->second == &a);
	CHECK(!v.UpdateOutputCreature(7, 72, 11));		// not there any more
	CHECK(!v.UpdateOutputCreature(6, 24, 11));		// the row, another id
	CHECK_EQ(2, int(v.m_mapCreature.size()));
	CHECK(v.m_mapCreature.find(Key(5, 24))->second == &b);
}

TEST(TopView1Creatures, UpdatingOntoAnOccupiedKeyDropsTheMovedCreature)
{
	Model model;
	View view;
	MTopView& v = *view;
	CreatureProbe moved(7, 0), stale(7, 0);
	v.m_mapCreature[Key(7, 10)] = &moved;
	v.m_mapCreature[Key(7, 20)] = &stale;
	// Recorded defect: the insert fails on the occupied key, yet the
	// update reports success and the moved creature is gone from the map.
	CHECK(v.UpdateOutputCreature(7, 10, 20));
	CHECK_EQ(1, int(v.m_mapCreature.size()));
	CHECK(v.m_mapCreature.begin()->second == &stale);
}

TEST(TopView1Creatures, AllTheZonesCreaturesAndFakesReplaceTheMap)
{
	Model model;
	PlayerProbe* player = new PlayerProbe;
	g_pPlayer = player;
	View view;
	MTopView& v = *view;
	v.SetZone(&model.zone);
	g_pZone = &model.zone;
	CreatureProbe a(7, 3), b(9, 1), fake(4, 2), old(1, 1);
	model.zone.m_mapCreature[7] = &a;
	model.zone.m_mapCreature[9] = &b;
	model.zone.m_mapFakeCreature[4] = &fake;
	v.m_mapCreature[Key(1, 32)] = &old;
	v.m_bTileSearchForCreature = true;
	v.AddOutputCreatureAll();
	CHECK(!v.m_bTileSearchForCreature);
	CHECK_EQ(3, int(v.m_mapCreature.size()));
	std::vector<MCreature*> order;
	for (const auto& entry : v.m_mapCreature) order.push_back(entry.second);
	CHECK(order == std::vector<MCreature*>({&b, &fake, &a}));
	CHECK(v.m_mapCreature.count(Key(9, 24)) == 1);
	CHECK(v.m_mapCreature.count(Key(4, 48)) == 1);
	CHECK(v.m_mapCreature.count(Key(7, 72)) == 1);
	// Twenty creatures or more: still every one (the tile search is off).
	std::vector<std::unique_ptr<CreatureProbe>> crowd;
	for (int id = 100; id < 120; ++id)
	{
		crowd.emplace_back(new CreatureProbe(TYPE_OBJECTID(id), 2));
		model.zone.m_mapCreature[TYPE_OBJECTID(id)] = crowd.back().get();
	}
	v.m_bTileSearchForCreature = true;
	v.AddOutputCreatureAll();
	CHECK(!v.m_bTileSearchForCreature);
	CHECK_EQ(23, int(v.m_mapCreature.size()));
	v.SetZone(nullptr);
	delete player;
}

TEST(TopView1Creatures, InACasketThePlayerIsTheOnlyCreatureDrawn)
{
	Model model;
	PlayerProbe* player = new PlayerProbe;
	g_pPlayer = player;
	player->SetID(3);
	player->MObject::SetPosition(1, 5);
	player->m_sY = 0;
	player->m_bInCasket = true;
	View view;
	MTopView& v = *view;
	v.SetZone(&model.zone);
	CreatureProbe a(7, 3), fake(4, 2);
	model.zone.m_mapCreature[7] = &a;
	model.zone.m_mapFakeCreature[4] = &fake;
	v.m_mapCreature[Key(1, 32)] = &a;
	v.m_bTileSearchForCreature = true;
	v.AddOutputCreatureAll();
	CHECK(!v.m_bTileSearchForCreature);
	CHECK_EQ(1, int(v.m_mapCreature.size()));
	CHECK(v.m_mapCreature.begin()->first == Key(3, 120));
	CHECK(v.m_mapCreature.begin()->second == player);
	v.SetZone(nullptr);
	delete player;
}

//----------------------------------------------------------------------
// The creature sprite bookkeeping
//----------------------------------------------------------------------
namespace {

// Sprite types: 0 a slayer player's (sprites 0-1), 1 a monster's (2-3),
// 2 an NPC's (4-5), 3 a monster's whose frame ID is 6 (6-7), 4 a
// monster's with no frame ID (8-9). Creature type n has sprite type n;
// creature type 5 has 1, 0 and 3, and creature type 6 has 1.
struct Book
{
	GameModelWorld world;
	CREATURE_TABLE creatures;
	CREATURESPRITE_TABLE sprites;
	UserOption option;
	CREATURE_TABLE* oldCreatures = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	UserOption* oldOption = g_pUserOption;
	View view;
	MTopView& v = *view;

	Book()
	{
		sprites.Init(5);
		Sprite(0, FLAG_CREATURESPRITE_PLAYER_SLAYER, 0, 0);
		Sprite(1, FLAG_CREATURESPRITE_MONSTER, 1, 2);
		Sprite(2, FLAG_CREATURESPRITE_NPC, 2, 4);
		Sprite(3, FLAG_CREATURESPRITE_MONSTER, 6, 6);
		Sprite(4, FLAG_CREATURESPRITE_MONSTER, FRAMEID_NULL, 8);
		creatures.Init(7);
		for (int type = 0; type < 5; ++type) Creature(type, {type});
		Creature(5, {1, 0, 3});
		Creature(6, {1});
		g_pCreatureTable = &creatures;
		g_pCreatureSpriteTable = &sprites;
		option.IsPreLoadMonster = FALSE;
		option.UseHalfFrame = FALSE;
		g_pUserOption = &option;
	}
	~Book()
	{
		g_pUserOption = oldOption;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatures;
	}
	void Sprite(int type, BYTE flags, int frameID, int first)
	{
		CREATURESPRITETABLE_INFO& row = testfw::MutableRow(sprites, type);
		row.CreatureType = flags;
		row.FrameID = TYPE_FRAMEID(frameID);
		row.FirstSpriteID = TYPE_SPRITEID(first);
		row.LastSpriteID = TYPE_SPRITEID(first + 1);
		row.FirstShadowSpriteID = SPRITEID_NULL;
		row.LastShadowSpriteID = SPRITEID_NULL;
		row.bLoad = FALSE;
	}
	void Creature(int type, std::initializer_list<int> spriteTypes)
	{
		CREATURETABLE_INFO& row = testfw::MutableRow(creatures, type);
		row.SpriteTypes.Init(int(spriteTypes.size()));
		int i = 0;
		for (int spriteType : spriteTypes) *row.SpriteTypes.GetMutable(i++) = spriteType;
	}
	CREATURESPRITETABLE_INFO& Row(int type) { return testfw::MutableRow(sprites, type); }
	bool Loaded(int type) { return Row(type).bLoad != FALSE; }
	// Ten loaded sprites and ten loaded shadows.
	void FillSprites()
	{
		v.m_CreatureSPK.Init(10);
		v.m_CreatureSSPK.Init(10);
		for (WORD id = 0; id < 10; ++id)
		{
			LoadIndexSprite(v.m_CreatureSPK[id]);
			LoadShadow(v.m_CreatureSSPK[id]);
		}
	}
	// Which of the ten sprites are still loaded, as a bit set.
	int LiveSprites()
	{
		int bits = 0;
		for (WORD id = 0; id < 10; ++id) if (v.m_CreatureSPK[id].IsInit()) bits |= 1 << id;
		return bits;
	}
	int LiveShadows()
	{
		int bits = 0;
		for (WORD id = 0; id < 10; ++id) if (v.m_CreatureSSPK[id].IsInit()) bits |= 1 << id;
		return bits;
	}
};

const int kAllSprites = 0x3FF;

} // namespace

TEST(TopView1Sprites, LoadingMarksTheTypeAndListsMonstersAndNPCsButNotPlayers)
{
	Book book;
	MTopView& v = book.v;
	book.FillSprites();
	v.LoadFromFileCreatureSPK(1);
	CHECK(book.Loaded(1));
	CHECK(Contents(v.m_listLoadedCreatureSprite) == std::vector<int>({1}));
	CHECK(Contents(v.m_listLoadedMonsterSprite) == std::vector<int>({1}));
	v.LoadFromFileCreatureSPK(2);
	CHECK(book.Loaded(2));
	CHECK(Contents(v.m_listLoadedCreatureSprite) == std::vector<int>({1, 2}));
	CHECK(Contents(v.m_listLoadedMonsterSprite) == std::vector<int>({1}));
	v.LoadFromFileCreatureSPK(0);
	CHECK(book.Loaded(0));
	CHECK(Contents(v.m_listLoadedCreatureSprite) == std::vector<int>({1, 2}));
	// A loaded type is not listed again.
	v.m_listLoadedCreatureSprite.Remove(1);
	v.LoadFromFileCreatureSPK(1);
	CHECK(Contents(v.m_listLoadedCreatureSprite) == std::vector<int>({2}));
	// A type past the table changes nothing.
	v.LoadFromFileCreatureSPK(-1);
	v.LoadFromFileCreatureSPK(5);
	CHECK(!book.Loaded(3));
	CHECK(!book.Loaded(4));
	CHECK(Contents(v.m_listLoadedCreatureSprite) == std::vector<int>({2}));
	// Without preloading nothing is read or released.
	CHECK_EQ(kAllSprites, book.LiveSprites());
}

TEST(TopView1Sprites, PreloadingAcceptsATypeOnlyWhenItsSpritesAndShadowsAreInThePacks)
{
	Book book;
	MTopView& v = book.v;
	book.option.IsPreLoadMonster = TRUE;
	v.m_CreatureSPK.Init(10);
	v.m_CreatureSSPK.Init(4);
	v.LoadFromFileCreatureSPK(1);		// shadows SPRITEID_NULL: none to read
	CHECK(book.Loaded(1));
	CHECK(Contents(v.m_listLoadedMonsterSprite) == std::vector<int>({1}));
	// Sprites past the pack: rejected, not marked, not listed.
	book.Row(3).LastSpriteID = 10;
	v.LoadFromFileCreatureSPK(3);
	CHECK(!book.Loaded(3));
	CHECK(Contents(v.m_listLoadedCreatureSprite) == std::vector<int>({1}));
	// Shadows past the pack: rejected as well.
	book.Row(2).FirstShadowSpriteID = 2;
	book.Row(2).LastShadowSpriteID = 4;
	v.LoadFromFileCreatureSPK(2);
	CHECK(!book.Loaded(2));
	CHECK(Contents(v.m_listLoadedCreatureSprite) == std::vector<int>({1}));
	// With half frames the shadows are not read at all.
	book.option.UseHalfFrame = TRUE;
	v.LoadFromFileCreatureSPK(2);
	CHECK(book.Loaded(2));
	CHECK(Contents(v.m_listLoadedCreatureSprite) == std::vector<int>({1, 2}));
	// An empty sprite pack rejects every type.
	book.v.m_CreatureSPK.Release();
	v.LoadFromFileCreatureSPK(4);
	CHECK(!book.Loaded(4));
}

TEST(TopView1Sprites, AddingMonsterTypesListsOnlyTheMonsters)
{
	Book book;
	MTopView& v = book.v;
	v.AddMonsterSpriteTypes(List({0, 1, 2, 3, 4}));
	CHECK(Contents(v.m_listLoadedMonsterSprite) == std::vector<int>({1, 3, 4}));
	CHECK_EQ(0, v.m_listLoadedCreatureSprite.GetSize());
	v.AddMonsterSpriteTypes(List({1, 4}));
	CHECK(Contents(v.m_listLoadedMonsterSprite) == std::vector<int>({1, 3, 4}));
}

TEST(TopView1Sprites, ReleasingACreatureReleasesItsLoadedSpriteTypes)
{
	Book book;
	MTopView& v = book.v;
	book.FillSprites();
	for (int type : {1, 2, 3}) v.LoadFromFileCreatureSPK(type);
	v.ReleaseCreatureSPK(1);
	CHECK(!book.Loaded(1));
	CHECK_EQ(kAllSprites & ~0x0C, book.LiveSprites());
	CHECK(Contents(v.m_listLoadedCreatureSprite) == std::vector<int>({2, 3}));
	CHECK(Contents(v.m_listLoadedMonsterSprite) == std::vector<int>({3}));
	// Recorded defect: the shadows are never released.
	CHECK_EQ(kAllSprites, book.LiveShadows());
	v.ReleaseCreatureSPK(2);
	CHECK(!book.Loaded(2));
	CHECK_EQ(kAllSprites & ~0x3C, book.LiveSprites());
	CHECK(Contents(v.m_listLoadedCreatureSprite) == std::vector<int>({3}));
	CHECK(Contents(v.m_listLoadedMonsterSprite) == std::vector<int>({3}));
	// A type not loaded keeps its sprites; a player's is never released.
	v.ReleaseCreatureSPK(4);
	v.LoadFromFileCreatureSPK(0);
	v.ReleaseCreatureSPK(0);
	CHECK(book.Loaded(0));
	CHECK_EQ(kAllSprites & ~0x3C, book.LiveSprites());
	// A creature type past the table has no sprite types.
	v.ReleaseCreatureSPK(7);
	v.ReleaseCreatureSPK(-1);
	CHECK(book.Loaded(3));
	CHECK_EQ(kAllSprites & ~0x3C, book.LiveSprites());
}

TEST(TopView1Sprites, ReleasingStopsAtTheFirstPlayerSpriteType)
{
	Book book;
	MTopView& v = book.v;
	book.FillSprites();
	for (int type : {0, 1, 3}) v.LoadFromFileCreatureSPK(type);
	v.ReleaseCreatureSPK(5);		// sprite types 1, 0, 3
	CHECK(!book.Loaded(1));
	// Recorded defect: it returns at the player's sprite type instead of
	// skipping it, so sprite type 3 after it stays loaded.
	CHECK(book.Loaded(3));
	CHECK(book.Loaded(0));
	CHECK_EQ(kAllSprites & ~0x0C, book.LiveSprites());
	CHECK(Contents(v.m_listLoadedCreatureSprite) == std::vector<int>({3}));
}

TEST(TopView1Sprites, ReleasingTheUselessTypesReleasesTheCreatureTypeNamedByTheFrameID)
{
	Book book;
	MTopView& v = book.v;
	book.FillSprites();
	for (int type : {0, 1, 2, 3, 4}) v.LoadFromFileCreatureSPK(type);
	CHECK(Contents(v.m_listLoadedCreatureSprite) == std::vector<int>({1, 2, 3, 4}));
	v.ReleaseUselessCreatureSPKExcept(List({1, 2}));
	CHECK(Contents(v.m_listLoadedCreatureSprite) == std::vector<int>({1, 2}));
	CHECK(Contents(v.m_listLoadedMonsterSprite) == std::vector<int>({1}));
	CHECK(book.Loaded(0));		// a player's: never in the list
	CHECK(book.Loaded(2));
	CHECK(!book.Loaded(3));
	CHECK(!book.Loaded(4));		// no frame ID: only unmarked
	// Recorded defect: sprite type 3's frame ID (6) is passed to
	// ReleaseCreatureSPK, which takes a creature type, so creature type 6's
	// sprite type 1 - one in use - is released and unmarked, and sprite
	// type 3's own sprites stay loaded.
	CHECK(!book.Loaded(1));
	CHECK_EQ(kAllSprites & ~0x0C, book.LiveSprites());
}

TEST(TopView1Sprites, ReleasingTheUselessTypesSkipsPlayersAndUnloadedTypes)
{
	Book book;
	MTopView& v = book.v;
	book.FillSprites();
	for (int type : {0, 3}) v.LoadFromFileCreatureSPK(type);
	v.ReleaseUselessCreatureSPKExcept(List({0, 3}));
	CHECK(Contents(v.m_listLoadedCreatureSprite) == std::vector<int>({0, 3}));
	CHECK(Contents(v.m_listLoadedMonsterSprite) == std::vector<int>({3}));
	book.Row(3).bLoad = FALSE;
	v.ReleaseUselessCreatureSPKExcept(List({}));
	CHECK(book.Loaded(0));		// the player's is skipped
	CHECK(!book.Loaded(3));
	CHECK_EQ(kAllSprites, book.LiveSprites());
	CHECK_EQ(0, v.m_listLoadedCreatureSprite.GetSize());
	CHECK_EQ(0, v.m_listLoadedMonsterSprite.GetSize());
}

//----------------------------------------------------------------------
// The text the view draws
//----------------------------------------------------------------------
namespace {

// The ten fonts, each its own size, printing on the text canvas; the
// view's surface is another, so boxes and text show apart.
struct Text : View
{
	Model model;
	PrintInfo fonts[kFonts] = {};
	PrintInfo* old[kFonts];
	RECT oldRect = g_GameRect;
	Canvas text;
	Text()
	{
		for (int i = 0; i < kFonts; ++i)
		{
			old[i] = g_ClientPrintInfo[i];
			fonts[i].hfont = TextSystem::EncodeFontSizeHandle(10 + i);
			g_ClientPrintInfo[i] = &fonts[i];
		}
		g_GameRect = {0, 0, kWidth, kHeight};
		g_SetFL2Surface(&text.surface);
	}
	~Text()
	{
		g_SetFL2Surface(nullptr);
		g_GameRect = oldRect;
		for (int i = 0; i < kFonts; ++i) g_ClientPrintInfo[i] = old[i];
	}
};

// Whether a font loads, so that text lights pixels at all.
bool FontsDraw() { return TextSystem::TextService::Get().GetDefaultFont().IsValid(); }

void Print(Canvas& canvas, int x, int y, const char* text, int font, COLORREF color)
{
	PrintInfo info = {};
	info.hfont = TextSystem::EncodeFontSizeHandle(10 + font);
	info.text_color = color;
	g_SetFL2Surface(&canvas.surface);
	g_Print(x, y, text, &info);
}

} // namespace

TEST(TopView1Draw, FontsMeasureAndPrintWithoutAWindow)
{
	Text scene;
	CHECK(FontsDraw());
	CHECK(g_GetStringWidth("HH", TextSystem::EncodeFontSizeHandle(20)) > g_GetStringWidth("HH", TextSystem::EncodeFontSizeHandle(10)));
	Print(scene.text, 4, 4, "HH", 2, RGB(0, 0, 128));
	CHECK(!scene.text.Blank());
}

TEST(TopView1Draw, TextIsPrintedInItsFontAndColourAndFreed)
{
	Text scene;
	MTopView& v = *scene;
	DRAWTEXT_NODE* node = new DRAWTEXT_NODE(5, 6, "Ab", RGB(250, 10, 10), 3);
	v.AddText(node);
	v.DrawTextList();
	CHECK(v.m_pqDrawText.empty());
	CHECK(Freed(node));
	Canvas expected;
	Print(expected, 5, 6, "Ab", 3, RGB(250, 10, 10));
	CHECK(Same(scene.text, expected));
	CHECK(FontsDraw() != scene.text.Blank());
	CHECK(scene.canvas.Blank());	// no box
	// The font's colour is left at the text's.
	CHECK_EQ(COLORREF(RGB(250, 10, 10)), scene.fonts[3].text_color);
}

TEST(TopView1Draw, OutlinedTextIsPrintedInTheInverseColourAboveLeftAndBelowRightFirst)
{
	Text scene;
	MTopView& v = *scene;
	v.AddText(new DRAWTEXT_NODE(20, 20, "Hi", RGB(0, 0, 255), 1, FLAG_DRAWTEXT_OUTLINE));
	v.DrawTextList();
	Canvas expected;
	Print(expected, 19, 19, "Hi", 1, RGB(255, 255, 0));
	Print(expected, 21, 21, "Hi", 1, RGB(255, 255, 0));
	Print(expected, 20, 20, "Hi", 1, RGB(0, 0, 255));
	CHECK(Same(scene.text, expected));
	CHECK(FontsDraw() != scene.text.Blank());
}

TEST(TopView1Draw, TextIsDrawnOldestFirstSoTheNewestIsOnTop)
{
	Text scene;
	MTopView& v = *scene;
	g_CurrentFrame = 3;
	DRAWTEXT_NODE* later = new DRAWTEXT_NODE(10, 10, "Mm", RGB(0, 255, 0), 4);
	DRAWTEXT_NODE* earlier = new DRAWTEXT_NODE(11, 11, "Mm", RGB(255, 0, 0), 4);
	earlier->SetTextTime(1);
	v.AddText(later);
	v.AddText(earlier);
	v.DrawTextList();
	Canvas expected;
	Print(expected, 11, 11, "Mm", 4, RGB(255, 0, 0));
	Print(expected, 10, 10, "Mm", 4, RGB(0, 255, 0));
	CHECK(Same(scene.text, expected));
}

TEST(TopView1Draw, AnOpaqueBoxIsFilledUnderTheTextAndAnEmptyTextPrintsNothing)
{
	Text scene;
	MTopView& v = *scene;
	DRAWTEXT_NODE* boxed = new DRAWTEXT_NODE(30, 30, "", 0, 0);
	boxed->SetBox(RECT{3, 4, 13, 9}, 0x1234, false);
	DRAWTEXT_NODE* none = new DRAWTEXT_NODE(30, 30, nullptr, 0, 0);
	none->SetBox(RECT{20, 4, 22, 6}, 0x4321, false);
	v.AddText(boxed);
	v.AddText(none);
	v.DrawTextList();
	CHECK(v.m_pqDrawText.empty());
	CHECK(Freed(boxed));
	CHECK(Freed(none));
	Canvas expected;
	RECT a = {3, 4, 13, 9}, b = {20, 4, 22, 6};
	expected.surface.FillRect(&a, 0x1234);
	expected.surface.FillRect(&b, 0x4321);
	CHECK(Same(scene.canvas, expected));
	CHECK(scene.text.Blank());
}

TEST(TopView1Draw, ATranslucentBoxIsBlendedAndABlackOneIsNot)
{
	Text scene;
	MTopView& v = *scene;
	DRAWTEXT_NODE* grey = new DRAWTEXT_NODE(0, 0, "", 0, 0);
	grey->SetBox(RECT{2, 2, 12, 8}, 0x8410);
	DRAWTEXT_NODE* black = new DRAWTEXT_NODE(0, 0, "", 0, 0);
	black->SetBox(RECT{30, 2, 40, 8}, 0);
	v.AddText(grey);
	v.AddText(black);
	v.DrawTextList();
	Canvas expected;
	RECT a = {2, 2, 12, 8};
	expected.surface.Lock();
	expected.surface.BltColorAlpha(&a, 0x8410, 15);
#ifdef PLATFORM_WINDOWS
	RECT b = {30, 2, 40, 8};
	expected.surface.GammaBox565(&b, 15);
#endif
	expected.surface.Unlock();
	CHECK(Same(scene.canvas, expected));
	// BltColorAlpha is the SDL backend's stub (CSpriteSurface_SDL.cpp), so
	// both pictures stay blank today; they keep matching once it blends.
}

namespace {

// The chat box's outline and tail as the view draws them.
void ChatBoxLines(CSpriteSurface& s, RECT rect, WORD color)
{
	const int left = rect.left + 1, top = rect.top + 1;
	const int width = rect.right - rect.left, height = rect.bottom - rect.top;
	if (rect.top - 1 > 0) s.HLine(left - 1, rect.top - 1, width, color);
	if (rect.bottom < kHeight)
	{
		s.HLine(left - 1, rect.bottom, 29, color);
		s.HLine(left + 29 - 1 + 6, rect.bottom, width - 29 - 6, color);
	}
	if (rect.left >= 0) s.VLine(rect.left - 1, top - 1, height, color);
	if (rect.right < kWidth) s.VLine(rect.right, top - 1, height, color);
}

void ChatBoxTail(CSpriteSurface& s, RECT rect, WORD color)
{
	const int x = rect.left + 29, y = rect.bottom;
	const int pt[13][2] = {{x, y + 1}, {x + 5, y + 1}, {x, y + 2}, {x + 4, y + 2}, {x + 1, y + 3}, {x + 4, y + 3},
		{x + 1, y + 4}, {x + 3, y + 4}, {x + 1, y + 5}, {x + 3, y + 5}, {x + 2, y + 6}, {x + 2, y + 7}, {x + 2, y + 8}};
	for (const auto& p : pt)
		if (p[0] >= 0 && p[1] >= 0 && p[0] < kWidth && p[1] < kHeight) s.HLine(p[0], p[1], 1, color);
}

} // namespace

TEST(TopView1Draw, AnOutlinedChatBoxGetsItsLinesAndTail)
{
	Text scene;
	MTopView& v = *scene;
	const RECT grey = {4, 6, 54, 20}, black = {8, 30, 70, 44};
	DRAWTEXT_NODE* a = new DRAWTEXT_NODE(0, 0, "", 0, 0);
	a->SetBox(grey, 0x8410);
	a->SetBoxOutline(0x001F);
	a->SetTextTime(1);
	DRAWTEXT_NODE* b = new DRAWTEXT_NODE(0, 0, "", 0, 0);
	b->SetBox(black, 0);
	b->SetBoxOutline(0xF800);
	b->SetTextTime(2);
	v.AddText(a);
	v.AddText(b);
	v.DrawTextList();
	Canvas expected;
	CSpriteSurface& s = expected.surface;
	RECT r = grey;
	ChatBoxLines(s, grey, 0x001F);
	s.Lock();
	s.BltColorAlpha(&r, 0x8410, 15);
	// Recorded defect: a coloured outlined box is blended twice - once by
	// DRAW_ALPHA_BOX_2D and once more by the branch after it (unseen while
	// BltColorAlpha is the SDL backend's stub).
	s.BltColorAlpha(&r, 0x8410, 15);
	ChatBoxTail(s, grey, 0x001F);
	s.Unlock();
	r = black;
	ChatBoxLines(s, black, 0xF800);
	s.Lock();
#ifdef PLATFORM_WINDOWS
	s.GammaBox565(&r, 15);
#endif
	// Recorded defect: the black box's darkened tail is dead code (rect2).
	ChatBoxTail(s, black, 0xF800);
	s.Unlock();
	CHECK(Same(scene.canvas, expected));
	CHECK(!scene.canvas.Blank());
}

TEST(TopView1Draw, AChatTailOffTheGameRectIsClippedPointByPoint)
{
	Text scene;
	MTopView& v = *scene;
	const RECT low = {50, 50, 90, 58};	// tail rows 59 to 66, past 64
	DRAWTEXT_NODE* node = new DRAWTEXT_NODE(0, 0, "", 0, 0);
	node->SetBox(low, 0);
	node->SetBoxOutline(0x07E0);
	v.AddText(node);
	v.DrawTextList();
	Canvas expected;
	ChatBoxLines(expected.surface, low, 0x07E0);
	expected.surface.Lock();
#ifdef PLATFORM_WINDOWS
	RECT r = low;
	expected.surface.GammaBox565(&r, 15);
#endif
	ChatBoxTail(expected.surface, low, 0x07E0);
	expected.surface.Unlock();
	CHECK(Same(scene.canvas, expected));
}

namespace {

// The queued text of an item name: where, what, in which colour and box.
struct Queued
{
	int x, y;
	const char* text;
	COLORREF color;
	int font;
	bool box;
	RECT rect;
	WORD boxColor;
	bool translucent;
};

std::vector<Queued> Queue(MTopView& v)
{
	std::vector<Queued> queued;
	for (DRAWTEXT_NODE* node : Drain(v))
	{
		const bool box = node->IsExistBox();
		queued.push_back({node->GetX(), node->GetY(), node->GetString(), node->GetColor(), node->GetFont(), box,
			box ? node->GetBox() : RECT{0, 0, 0, 0}, box ? node->GetBoxColor() : WORD(0), box && node->IsTransBox()});
		delete node;
	}
	return queued;
}

} // namespace

TEST(TopView1Draw, ItemNamesAreQueuedRightAlignedDownFromRow160WithTheirBoxes)
{
	Text scene;
	MTopView& v = *scene;
	ClientConfig& c = *g_pClientConfig;
	c.MAX_DRAWITEMNAME = 10;
	c.FONT_ITEM_HEIGHT = 15;
	c.COLOR_NAME_ITEM = RGB(1, 2, 3);
	c.COLOR_NAME_ITEM_OPTION = RGB(4, 5, 6);
	c.COLOR_NAME_ITEM_RARE_OPTION = RGB(7, 8, 9);
	c.COLOR_NAME_VAMPIRE = RGB(10, 11, 12);
	Access::InitColors(v);
	std::unique_ptr<MItem> plain = Item(1, "Ring"), one = Item(2, "Blue"), two = Item(3, "Rare"),
		three = Item(4, "Red"), unnamed = Item(5), quest = Item(6, "Quest");
	one->AddItemOption(1);
	two->AddItemOption(1);
	two->AddItemOption(2);
	for (int option = 1; option <= 3; ++option) three->AddItemOption(TYPE_ITEM_OPTION(option));
	quest->SetQuestFlag();
	c.QuestItemColorSet = 3;
	const WORD oldShade = CIndexSprite::ColorSet[3][16];
	CIndexSprite::ColorSet[3][16] = WORD((9 << 11) | (20 << 5) | 30);
	MItem* items[] = {plain.get(), one.get(), two.get(), three.get(), unnamed.get(), quest.get()};
	DRAWITEMNAME_NODE* nodes[6];
	for (int i = 0; i < 6; ++i) v.AddItemName(nodes[i] = new DRAWITEMNAME_NODE(DWORD(i), items[i]));
	v.DrawItemNameList();
	CIndexSprite::ColorSet[3][16] = oldShade;
	const COLORREF colors[] = {RGB(1, 2, 3), RGB(4, 5, 6), RGB(7, 8, 9), RGB(10, 11, 12), 0,
		RGB(9 << 3, 20 << 3, 30 << 3)};
	std::vector<Queued> queued = Queue(v);
	CHECK_EQ(5, int(queued.size()));
	int row = 0;
	for (int i = 0; i < 6; ++i)
	{
		const int y = 160 + 15 * i;
		if (i == 4) continue;		// no name: no text, the row stays empty
		const char* name = items[i]->GetName();
		const int x = kWidth - g_GetStringWidth(name, scene.fonts[kFontItem].hfont);
		if (row < int(queued.size()))
		{
			const Queued& q = queued[size_t(row)];
			CHECK_EQ(x, q.x);
			CHECK_EQ(y + 1, q.y);
			CHECK(q.text == name);
			CHECK_EQ(colors[i], q.color);
			CHECK_EQ(kFontItem, q.font);
			CHECK(q.box);
			CHECK_EQ(x, int(q.rect.left));
			CHECK_EQ(y, int(q.rect.top));
			CHECK_EQ(kWidth, int(q.rect.right));
			CHECK_EQ(y + 15, int(q.rect.bottom));
			CHECK_EQ(v.m_ColorUIBoxBg, q.boxColor);
			CHECK(q.translucent);
		}
		++row;
		// The name's box is where a click selects it.
		CHECK(nodes[i]->IsPointInRect(x, y));
		CHECK(nodes[i]->IsPointInRect(kWidth - 1, y + 14));
		CHECK(!nodes[i]->IsPointInRect(x - 1, y));
		CHECK(!nodes[i]->IsPointInRect(kWidth, y));
		CHECK(!nodes[i]->IsPointInRect(x, y + 15));
		CHECK(!nodes[i]->IsPointInRect(x, y - 1));
	}
	// A name-less item's node never gets a box: its rectangle stays
	// uninitialised for SelectItemName to read (not run here).
}

TEST(TopView1Draw, TheSelectedItemsNameIsShadowedInBlackWithoutABox)
{
	Text scene;
	MTopView& v = *scene;
	g_pClientConfig->MAX_DRAWITEMNAME = 10;
	g_pClientConfig->FONT_ITEM_HEIGHT = 12;
	g_pClientConfig->COLOR_NAME_ITEM = RGB(1, 2, 3);
	Access::InitColors(v);
	std::unique_ptr<MItem> item = Item(8, "Axe");
	scene.model.zone.m_mapItem[8] = item.get();
	v.SetZone(&scene.model.zone);
	v.AddItemName(new DRAWITEMNAME_NODE(1, item.get()));
	v.SetSelectedItem(8);
	g_CurrentFrame = 2;
	v.DrawItemNameList();
	const int x = kWidth - g_GetStringWidth("Axe", scene.fonts[kFontItem].hfont);
	std::vector<Queued> queued = Queue(v);
	CHECK_EQ(2, int(queued.size()));
	if (queued.size() == 2)
	{
		CHECK_EQ(x, queued[0].x);
		CHECK_EQ(161, queued[0].y);
		CHECK_EQ(COLORREF(0), queued[0].color);
		CHECK(!queued[0].box);
		CHECK_EQ(x + 1, queued[1].x);
		CHECK_EQ(162, queued[1].y);
		CHECK_EQ(COLORREF(RGB(1, 2, 3)), queued[1].color);
		CHECK_EQ(kFontItem, queued[1].font);
		CHECK(!queued[1].box);
	}
	// The name's box is set all the same.
	CHECK(v.SelectItemName(x, 160) == item.get());
	CHECK(v.SelectItemName(kWidth - 1, 171) == item.get());
	CHECK(v.SelectItemName(x - 1, 160) == nullptr);
	CHECK(v.SelectItemName(x, 172) == nullptr);
	v.SetZone(nullptr);
}
