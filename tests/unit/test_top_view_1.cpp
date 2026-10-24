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
#include "SpriteLib/SpriteLibBackendSDL.h"
#include "SpriteLib/CIndexSprite.h"
#include "SpriteLib/CSprite565.h"
#include "MCreatureWear.h"
#include "MFakeCreature.h"
#include "MSlayerGear.h"
#include "MVampireGear.h"
#include "MOustersGear.h"
#include "MZoneTable.h"
#include "MJusticeAttackManager.h"
#include "MGuildMarkManager.h"
#include "MEventManager.h"
#include "MGameStringTable.h"
#include "CMessageArray.h"
#include "NicknameInfo.h"
#include "MonotonicClock.h"
#include "VS_UI_GameCommon.h"
#include "SpriteIDDef.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <fstream>
#include <memory>
#include <set>
#include <string>
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
// the creature sprite bookkeeping, and the text it draws: the text queue,
// item names, chat, creature and player names with their HP bars and
// marks, HP changes, event text and the HUD's messages (task 4.260, which
// moved MTopView into gameclient). A view is constructed, given a surface
// and only what each path reads; nothing loads data files.

extern PrintInfo*	g_ClientPrintInfo[];	// MTopView.cpp's, ten fonts
extern bool			g_bInterpolateDraw;		// MObjectScreen.cpp's
extern RECT			g_GameRect;
extern BOOL			g_bDrawProgress;		// GameUI.cpp's
extern BOOL			g_MyFull;				// ClientMode.cpp's
extern int			g_x, g_y;				// InputState.cpp's: the mouse
extern CMessageArray*	g_pSystemMessage;	// GameMessages.cpp's
extern CMessageArray*	g_pPlayerMessage;
extern CMessageArray*	g_pNoticeMessage;
extern CMessageArray*	g_pGameMessage;

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
	static int	ChatString(MTopView& view, POINT* p, MCreature* c, COLORREF color, BYTE flag = 0)	{ return view.DrawChatString(p, c, color, flag); }
	static void	HPModify(MTopView& view, POINT* p, MCreature* c)	{ view.DrawCreatureHPModify(p, c); }
	static void	SiegeMark(MTopView& view, MCreature* c, int y)		{ view.DrawGuildMarkInSiegeWar(c, y); }
	static void	Name(MTopView& view, MCreature* c)					{ view.DrawCreatureName(c); }
	static void	MyName(MTopView& view)								{ view.DrawCreatureMyName(); }
	static void	EventString(MTopView& view, int& x, int& y)			{ view.DrawEventString(x, y); }
	static void	Information(MTopView& view)							{ view.DrawInformation(); }
};

namespace {

typedef MTopViewTestAccess1 Access;

const int kFonts = 10;		// MTopView.cpp's MAX_FONTID
const int kFontItem = 7;	// its FONTID_ITEM
const int kWidth = 160, kHeight = 120;
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
	int width, height;
	explicit Canvas(int w = kWidth, int h = kHeight) : width(w), height(h)
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
	bool Blank() { for (WORD pixel : Picture()) if (pixel != kPaper) return false; return true; }
};

// The two pictures are the same; reports the first difference.
bool Same(Canvas& drawn, Canvas& expected)
{
	const std::vector<WORD> a = drawn.Picture(), b = expected.Picture();
	for (size_t i = 0; i < a.size() && i < b.size(); ++i)
		if (a[i] != b[i])
		{
			std::printf("      the picture differs first at (%d,%d): %04x, not %04x\n", int(i % drawn.width), int(i / drawn.width), a[i], b[i]);
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
	using MZone::m_Info;
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

MTopView::INT_ORDERED_LIST IntList(std::initializer_list<int> values)
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
			// A reset redraws the tile cache, so a scroll costs no readback.
			CHECK_EQ(1, v.m_pTileSurface->GetBackendSurface()->transient);
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
	v.AddMonsterSpriteTypes(IntList({0, 1, 2, 3, 4}));
	CHECK(Contents(v.m_listLoadedMonsterSprite) == std::vector<int>({1, 3, 4}));
	CHECK_EQ(0, v.m_listLoadedCreatureSprite.GetSize());
	v.AddMonsterSpriteTypes(IntList({1, 4}));
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
	v.ReleaseUselessCreatureSPKExcept(IntList({1, 2}));
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
	for (int type : {0, 1, 3}) v.LoadFromFileCreatureSPK(type);
	v.ReleaseUselessCreatureSPKExcept(IntList({0, 1, 3}));
	CHECK(Contents(v.m_listLoadedCreatureSprite) == std::vector<int>({0, 1, 3}));
	CHECK(Contents(v.m_listLoadedMonsterSprite) == std::vector<int>({1, 3}));
	book.Row(3).bLoad = FALSE;
	// Released, type 3 would release creature type 6 (its frame ID), whose sprite type 1 is in use.
	v.ReleaseUselessCreatureSPKExcept(IntList({1}));
	CHECK(book.Loaded(0));		// the player's is skipped
	CHECK(book.Loaded(1));
	CHECK(!book.Loaded(3));
	CHECK_EQ(kAllSprites, book.LiveSprites());
	CHECK(Contents(v.m_listLoadedCreatureSprite) == std::vector<int>({1}));
	CHECK(Contents(v.m_listLoadedMonsterSprite) == std::vector<int>({1}));
}

//----------------------------------------------------------------------
// The text the view draws
//----------------------------------------------------------------------
namespace {

// Where the view's text goes; reference prints elsewhere return here.
CSpriteSurface* s_textTarget = nullptr;
void Aim(CSpriteSurface* surface) { s_textTarget = surface; g_SetFL2Surface(surface); }

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
		Aim(&text.surface);
	}
	~Text()
	{
		Aim(nullptr);
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
	g_SetFL2Surface(s_textTarget);
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
	const RECT low = {50, 106, 90, 114};	// tail rows 115 to 122, past 120
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
	bool outline;		// the text's outline flag
	bool boxOutline;
	WORD boxOutlineColor;
	QWORD time;
};

std::vector<Queued> Queue(MTopView& v)
{
	std::vector<Queued> queued;
	for (DRAWTEXT_NODE* node : Drain(v))
	{
		const bool box = node->IsExistBox();
		const bool boxOutline = node->IsExistBoxOutline();
		queued.push_back({node->GetX(), node->GetY(), node->GetString(), node->GetColor(), node->GetFont(), box,
			box ? node->GetBox() : RECT{0, 0, 0, 0}, box ? node->GetBoxColor() : WORD(0), box && node->IsTransBox(),
			node->IsOutline() != 0, boxOutline, boxOutline ? node->GetBoxOutlineColor() : WORD(0), node->GetTextTime()});
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

//----------------------------------------------------------------------
// The text over creatures and the HUD
//----------------------------------------------------------------------
namespace {

const int kSlayer = 1, kVampire = 2, kOusters = 3, kNPC = 4, kMonster = 5, kBathory = 217;
const TYPE_ZONEID kZone = 61;
const int kNameSlayer = 1, kNameVampire = 2, kNameNPC = 3, kChatSlayer = 4, kChatVampire = 5,
	kChatLarge = 8, kUserID = 9;

// A sprite of its own size and colour.
void LoadSprite(CSprite& sprite, int n)
{
	const int width = 2 + n % 5, height = 1 + n % 3;
	const WORD colour = WORD(0x0841 * (1 + n % 29));
	std::vector<WORD> words{WORD(width), WORD(height)};
	for (int y = 0; y < height; ++y)
	{
		words.push_back(WORD(3 + width));
		words.push_back(1);
		words.push_back(0);
		words.push_back(WORD(width));
		for (int x = 0; x < width; ++x) words.push_back(colour);
	}
	Load(sprite, words);
}

// A creature whose drawn state a test sets.
template<class Kind> struct Figure : Kind
{
	using MCreature::m_CreatureType;
	using MCreature::m_ChatString;
	using MCreature::m_ChatStringCurrent;
	using MCreature::m_ChatTime;
	using MCreature::m_OriChatColor;
	using MCreature::m_bEffectStatus;
	using MCreature::m_NameSelection;
	using MCreature::m_MoveType;
	using MCreature::m_bAlive;
	using MCreature::m_bInCasket;
	using MCreature::m_HPModifyList;
	using MStatus::m_Status;

	// Of the type, at the sector, named, with no nickname (the constructor
	// leaves the nickname's type uninitialised) and nothing said.
	void Become(int type, TYPE_OBJECTID id, int x, int y, const char* name)
	{
		m_CreatureType = TYPE_CREATURETYPE(type);
		this->SetID(id);
		this->MObject::SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
		this->SetName(name);
		Nick(NicknameInfo::NICK_NONE, "");
		Say({});
		m_HPModifyList.clear();		// the constructor's status changes
	}
	void Nick(BYTE type, const char* nick)
	{
		char buffer[32];
		SafeFormat::Copy(buffer, nick);
		this->SetNickName(type, buffer);
	}
	// The newest lines last, as GetChatString gives them.
	void Say(std::initializer_list<const char*> lines)
	{
		const int rows = g_pClientConfig->MAX_CHATSTRING;
		for (int i = 0; i < rows; ++i) m_ChatString[i][0] = '\0';
		m_ChatStringCurrent = 0;
		int row = rows - int(lines.size());
		for (const char* line : lines) SafeFormat::Copy(m_ChatString[row++], size_t(g_pClientConfig->MAX_CHATSTRINGLENGTH_PLUS1), line);
	}
	void Set(int status, bool on = true) { m_bEffectStatus[status] = on; }
	void Hp(DWORD hp, DWORD maxHp) { m_Status[MODIFY_CURRENT_HP] = hp; m_Status[MODIFY_MAX_HP] = maxHp; }
	void Status(int n, DWORD value) { m_Status[n] = value; }
};

typedef Figure<MCreature> Creature;
typedef Figure<MPlayer> Me;

struct GuildMarks : MGuildMarkManager
{
	GuildMarks() : MGuildMarkManager(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr) {}
	using MGuildMarkManager::m_GuildMarks;
	using MGuildMarkManager::m_GradeMarks;
	using MGuildMarkManager::m_LevelMarks;
	CSprite* Small(WORD guild, int n)
	{
		CSprite* sprite = new CSprite565;
		LoadSprite(*sprite, n);
		m_GuildMarks[guild] = GUILD_SPRITES(nullptr, sprite);
		return sprite;
	}
	CSprite* Grade(Race race, int gradeID, int n)
	{
		CSpritePack& pack = m_GradeMarks[race];
		if (pack.GetSize() == 0) pack.Init(WORD((GRADE_MARK_MAX + 1) * 2));
		LoadSprite(pack[WORD(gradeID * 2 + 1)], n);
		return &pack[WORD(gradeID * 2 + 1)];
	}
	// A pet's level mark; every level up to 50 has a slot.
	CSprite* Level(int level, int n)
	{
		if (m_LevelMarks.GetSize() == 0) m_LevelMarks.Init(WORD(51 * 2));
		LoadSprite(m_LevelMarks[WORD(level * 2 + 1)], n);
		return &m_LevelMarks[WORD(level * 2 + 1)];
	}
};

struct Events : MEventQueue
{
	using MEventQueue::m_Events;
	void Add(EVENT_ID id, DWORD flags, int delay, std::vector<int> strings = {})
	{
		MEvent event;
		event.eventID = id;
		event.eventFlag = flags;
		event.eventDelay = delay;
		event.m_StringsID = strings;
		AddEvent(event);
	}
};

// The clock MonotonicClock::Now() reads while a scene lives.
MonotonicClock::TimePoint s_now;
MonotonicClock::TimePoint ReadClock() { return s_now; }

// The world the creature and HUD text reads: creature types (sprite
// types 0, 3 and 4 a slayer's, vampire's and ousters' player-only ones,
// 1 a monster's, 2 an NPC's), a zone in the zone table, the player (a
// slayer at (5,5) in peace), the guild marks, the event queue, the
// options, the UI base's chat font and the four message arrays.
struct Hud : Text
{
	CREATURE_TABLE creatures;
	CREATURESPRITE_TABLE sprites;
	CZoneTable zones;
	ZONETABLE_INFO* zoneInfo = new ZONETABLE_INFO;
	MJusticeAttackManager justice;
	GuildMarks marks;
	Events events;
	UserOption option;
	Base base;
	CMessageArray messages[4];
	MSlayerGear slayerGear;
	MVampireGear vampireGear;
	MOustersGear oustersGear;
	MonotonicClock::ScopedTestSource clock{&ReadClock};
	CREATURE_TABLE* oldCreatures = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	CZoneTable* oldZones = g_pZoneTable;
	MJusticeAttackManager* oldJustice = g_pJusticeAttackManager;
	MGuildMarkManager* oldMarks = g_pGuildMarkManager;
	MEventQueue* oldEvents = g_pEventManager;
	UserOption* oldOption = g_pUserOption;
	Base* oldBase = gpC_base;
	CMessageArray* oldMessages[4] = {g_pSystemMessage, g_pPlayerMessage, g_pNoticeMessage, g_pGameMessage};
	MSlayerGear* oldSlayerGear = g_pSlayerGear;
	MVampireGear* oldVampireGear = g_pVampireGear;
	MOustersGear* oldOustersGear = g_pOustersGear;
	MMagazine* oldMagazine = g_pCurrentMagazine;
	BOOL oldFull = g_MyFull;
	int oldX = g_x, oldY = g_y;
	int oldAlignment = int(g_char_slot_ingame.alignment);
	MonotonicClock::TimePoint oldFrameNow = g_FrameNow;
	Me* player = nullptr;

	Hud()
	{
		s_now = MonotonicClock::TimePoint(MonotonicClock::Duration(1000000));
		sprites.Init(5);
		const BYTE flags[5] = {FLAG_CREATURESPRITE_PLAYER_SLAYER, FLAG_CREATURESPRITE_MONSTER, FLAG_CREATURESPRITE_NPC,
			FLAG_CREATURESPRITE_PLAYER_VAMPIRE, FLAG_CREATURESPRITE_PLAYER_OUSTERS};
		for (int type = 0; type < 5; ++type) testfw::MutableRow(sprites, type).CreatureType = flags[type];
		g_pCreatureSpriteTable = &sprites;
		creatures.Init(800);
		Type(kSlayer, CREATURETRIBE_SLAYER, 0);
		Type(kVampire, CREATURETRIBE_VAMPIRE, 3);
		Type(kOusters, CREATURETRIBE_OUSTERS, 4);
		Type(kNPC, CREATURETRIBE_NPC, 2);
		Type(kMonster, CREATURETRIBE_VAMPIRE, 1);
		Type(kBathory, CREATURETRIBE_NPC, 2);
		Type(CREATURETYPE_SLAYER_OPERATOR, CREATURETRIBE_SLAYER, 0);
		Type(CREATURETYPE_VAMPIRE_OPERATOR, CREATURETRIBE_VAMPIRE, 3);
		Type(CREATURETYPE_OUSTERS_OPERATOR, CREATURETRIBE_OUSTERS, 4);
		for (int type : {672, 673}) Type(type, CREATURETRIBE_SLAYER, 0);
		for (int type = 725; type <= 730; ++type) Type(type, CREATURETRIBE_SLAYER, 1);
		g_pCreatureTable = &creatures;
		zoneInfo->ID = kZone;
		zoneInfo->ChatMaskByRace = false;
		zones.Add(zoneInfo);
		g_pZoneTable = &zones;
		model.zone.m_Info.ZoneID = kZone;
		g_pZone = &model.zone;
		g_pJusticeAttackManager = &justice;
		g_pGuildMarkManager = &marks;
		g_pEventManager = &events;
		option.DrawChatBoxOutline = FALSE;
		option.DoNotShowPersnalShopMsg = FALSE;
		g_pUserOption = &option;
		base.m_chatting_pi = PrintInfo{};
		base.m_chatting_pi.hfont = TextSystem::EncodeFontSizeHandle(21);
		gpC_base = &base;
		for (CMessageArray& array : messages) array.Init(3, 64);
		g_pSystemMessage = &messages[0];
		g_pPlayerMessage = &messages[1];
		g_pNoticeMessage = &messages[2];
		g_pGameMessage = &messages[3];
		g_pSlayerGear = &slayerGear;
		g_pVampireGear = &vampireGear;
		g_pOustersGear = &oustersGear;
		g_pCurrentMagazine = nullptr;
		g_pGameStringTable->Init(MAX_GAME_STRING);
		*g_pGameStringTable->GetMutable(UI_STRING_MESSAGE_MASTER_NAME) = "GM";
		model.levelNames.Init(3);
		*model.levelNames.GetMutable(1) = "Lord";
		ClientConfig& c = *g_pClientConfig;
		c.COLOR_NAME_NPC = RGB(1, 1, 1);
		c.COLOR_NAME_EVIL_MORE = RGB(10, 0, 0);
		c.COLOR_NAME_EVIL = RGB(11, 0, 0);
		c.COLOR_NAME_NEUTRAL = RGB(12, 0, 0);
		c.COLOR_NAME_GOOD = RGB(13, 0, 0);
		c.COLOR_NAME_GOOD_MORE = RGB(14, 0, 0);
		c.COLOR_HP_BAR_R = 1; c.COLOR_HP_BAR_G = 2; c.COLOR_HP_BAR_B = 3;
		c.COLOR_HP_BAR_BG_R = 4; c.COLOR_HP_BAR_BG_G = 5; c.COLOR_HP_BAR_BG_B = 6;
		Access::InitColors(*view);
		(*this)->m_pSelectedCreature = nullptr;	// the constructor leaves it unset (bug 11)
		(*this)->m_EtcSPK.Init(SPRITEID_INFO_REQUEST_LAST + 1);
		for (int id = 0; id <= SPRITEID_INFO_REQUEST_LAST; ++id) LoadSprite((*this)->m_EtcSPK[WORD(id)], id);
		player = new Me;
		g_pPlayer = player;
		player->Become(kSlayer, 1, 5, 5, "Me");
		player->SetAttackModePeace();
	}
	~Hud()
	{
		delete player;		// sets g_pPlayer NULL; the Model restores it
		g_FrameNow = oldFrameNow;
		g_char_slot_ingame.alignment = ALIGNMENT(oldAlignment);
		g_x = oldX;
		g_y = oldY;
		g_MyFull = oldFull;
		g_pCurrentMagazine = oldMagazine;
		g_pOustersGear = oldOustersGear;
		g_pVampireGear = oldVampireGear;
		g_pSlayerGear = oldSlayerGear;
		g_pSystemMessage = oldMessages[0];
		g_pPlayerMessage = oldMessages[1];
		g_pNoticeMessage = oldMessages[2];
		g_pGameMessage = oldMessages[3];
		gpC_base = oldBase;
		g_pUserOption = oldOption;
		g_pEventManager = oldEvents;
		g_pGuildMarkManager = oldMarks;
		g_pJusticeAttackManager = oldJustice;
		g_pZoneTable = oldZones;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatures;
	}
	void Type(int type, CREATURETRIBE tribe, int spriteType)
	{
		CREATURETABLE_INFO& row = testfw::MutableRow(creatures, type);
		row.SetCreatureTribe(tribe, ACTION_MAX_SLAYER);
		row.SpriteTypes.Init(1);
		*row.SpriteTypes.GetMutable(0) = spriteType;
		row.Height = 50;
	}
	void Later(int millis) { s_now += MonotonicClock::Duration(millis); }
};

// The node's text, or "" for none.
std::string TextOf(const Queued& q) { return q.text != nullptr ? q.text : ""; }

} // namespace

//----------------------------------------------------------------------
// Chat over a creature (DrawChatString)
//----------------------------------------------------------------------
namespace {

// The widest of the lines, or the box's least width.
int ChatWidth(std::initializer_list<const char*> lines, int font, int least)
{
	int width = 0;
	for (const char* line : lines) width = (std::max)(width, (std::max)(least, g_GetStringWidth(line, g_ClientPrintInfo[font]->hfont)));
	return width;
}

} // namespace

TEST(TopView1Chat, ChatIsQueuedAboveThePointInTheRaceFontWithOneBox)
{
	Hud scene;
	MTopView& v = *scene;
	for (int type : {kSlayer, kNPC, kVampire})
	{
		Creature c;
		c.Become(type, 9, 1, 1, "Kim");
		c.Say({"hi", "yo"});
		c.m_ChatTime = 7;
		const int font = type == kVampire ? kChatVampire : kChatSlayer;
		const int width = ChatWidth({"hi", "yo"}, font, 60);
		POINT point = {30, 100};
		CHECK_EQ(60, Access::ChatString(v, &point, &c, RGB(1, 2, 3), FLAG_DRAWTEXT_OUTLINE));
		CHECK_EQ(30, int(point.x));		// the point is not moved
		CHECK_EQ(100, int(point.y));
		const std::vector<Queued> q = Queue(v);
		CHECK_EQ(2, int(q.size()));
		if (q.size() != 2) continue;
		const QWORD time = (QWORD(7) << 41) | (QWORD(9) << 9);
		CHECK_EQ(30, q[0].x);
		CHECK_EQ(63, q[0].y);
		CHECK(TextOf(q[0]) == "hi");
		CHECK_EQ(COLORREF(RGB(1, 2, 3)), q[0].color);
		CHECK_EQ(font, q[0].font);
		CHECK(q[0].outline);
		CHECK(q[0].box);
		CHECK_EQ(26, int(q[0].rect.left));
		CHECK_EQ(58, int(q[0].rect.top));
		CHECK_EQ(30 + width + 4, int(q[0].rect.right));
		CHECK_EQ(102, int(q[0].rect.bottom));
		CHECK_EQ(v.m_ColorUIBoxBg, q[0].boxColor);
		CHECK(q[0].translucent);
		CHECK(!q[0].boxOutline);
		CHECK_EQ(time, q[0].time);
		CHECK_EQ(30, q[1].x);
		CHECK_EQ(83, q[1].y);
		CHECK(TextOf(q[1]) == "yo");
		CHECK_EQ(font, q[1].font);
		CHECK(!q[1].box);
		CHECK_EQ(time + 1, q[1].time);
	}
}

TEST(TopView1Chat, LoudChatIsLargerAndPushedBackInsideTheRightEdge)
{
	Hud scene;
	MTopView& v = *scene;
	Creature c;
	c.Become(kSlayer, 9, 1, 1, "Kim");
	c.Say({"a", "b"});
	c.Set(EFFECTSTATUS_LOUD);
	const int width = ChatWidth({"a", "b"}, kChatLarge, 90);
	POINT point = {100, 100};
	CHECK_EQ(50, Access::ChatString(v, &point, &c, 0));	// two rows of FONT_HEIGHT + 5
	const std::vector<Queued> q = Queue(v);
	CHECK_EQ(2, int(q.size()));
	if (q.size() != 2) return;
	const int x = kWidth - width;
	CHECK_EQ(x, q[0].x);
	CHECK_EQ(53, q[0].y);
	CHECK_EQ(kChatLarge, q[0].font);
	CHECK_EQ(x - 4, int(q[0].rect.left));
	CHECK_EQ(48, int(q[0].rect.top));
	CHECK_EQ(kWidth + 4, int(q[0].rect.right));
	CHECK_EQ(102, int(q[0].rect.bottom));
	CHECK_EQ(78, q[1].y);
	CHECK(!q[0].outline);
}

TEST(TopView1Chat, ChatAboveTheTopOrLeftOfTheScreenIsKeptOnIt)
{
	Hud scene;
	MTopView& v = *scene;
	Creature c;
	c.Become(kSlayer, 9, 1, 1, "Kim");
	c.Say({"a", "b"});
	const int width = ChatWidth({"a", "b"}, kChatSlayer, 60);
	POINT point = {-5, 30};
	CHECK_EQ(0, Access::ChatString(v, &point, &c, 0));
	std::vector<Queued> q = Queue(v);
	CHECK_EQ(2, int(q.size()));
	if (q.size() == 2)
	{
		CHECK_EQ(0, q[0].x);
		CHECK_EQ(3, q[0].y);
		CHECK_EQ(23, q[1].y);
		CHECK_EQ(-4, int(q[0].rect.left));
		CHECK_EQ(-2, int(q[0].rect.top));
		CHECK_EQ(width + 4, int(q[0].rect.right));
		CHECK_EQ(42, int(q[0].rect.bottom));
	}
	// Inside the top: where it was put.
	point = {10, 50};
	CHECK_EQ(10, Access::ChatString(v, &point, &c, 0));
	q = Queue(v);
	CHECK_EQ(2, int(q.size()));
	if (!q.empty()) CHECK_EQ(13, q[0].y);
}

TEST(TopView1Chat, TheSelectedCreaturesChatLeavesRoomForItsNameRows)
{
	Hud scene;
	MTopView& v = *scene;
	Creature c;
	c.Become(kSlayer, 9, 1, 1, "Kim");
	c.Say({"a", "b"});
	v.SetSelectedCreature(9);
	POINT point = {30, 30};
	CHECK_EQ(20, Access::ChatString(v, &point, &c, 0));	// one name row
	std::vector<Queued> q = Queue(v);
	CHECK_EQ(2, int(q.size()));
	if (q.size() == 2)
	{
		CHECK_EQ(23, q[0].y);
		CHECK_EQ(43, q[1].y);
		CHECK_EQ(18, int(q[0].rect.top));
		CHECK_EQ(62, int(q[0].rect.bottom));
	}
	// With a level name: two rows.
	c.m_NameSelection.SelectLevelName(scene.model.levelNames, 1);
	CHECK(c.HasLevelName());
	CHECK_EQ(40, Access::ChatString(v, &point, &c, 0));
	q = Queue(v);
	CHECK_EQ(2, int(q.size()));
	if (q.size() == 2)
	{
		CHECK_EQ(43, q[0].y);
		CHECK_EQ(38, int(q[0].rect.top));
		CHECK_EQ(82, int(q[0].rect.bottom));
	}
	point = {30, 39};	// first row at -1, inside the two rows
	CHECK_EQ(40, Access::ChatString(v, &point, &c, 0));
	Queue(v);
	point = {30, 80};	// below them: as put
	CHECK_EQ(40, Access::ChatString(v, &point, &c, 0));
	q = Queue(v);
	CHECK_EQ(2, int(q.size()));
	if (!q.empty()) CHECK_EQ(38, int(q[0].rect.top));
}

TEST(TopView1Chat, ChatIsTimedByTheSelectionTheShopOrTheChatAndCreature)
{
	Hud scene;
	MTopView& v = *scene;
	g_CurrentFrame = 30;
	Creature c;
	c.Become(kSlayer, 9, 1, 1, "Kim");
	c.Say({"a"});
	c.m_ChatTime = 7;
	POINT point = {30, 100};
	v.AddText(new DRAWTEXT_NODE(0, 0, "queued"));		// one in the queue already
	Access::ChatString(v, &point, &c, 0);
	std::vector<Queued> q = Queue(v);
	CHECK_EQ(2, int(q.size()));
	if (q.size() == 2) CHECK_EQ(((QWORD(7) << 41) | (QWORD(9) << 9)) + 1, q[0].time);
	c.SetPersnalShop(1);
	c.SetPersonalShopMsgTime(12);
	Access::ChatString(v, &point, &c, 0);
	q = Queue(v);
	CHECK_EQ(1, int(q.size()));
	if (q.size() == 1) CHECK_EQ(QWORD(42) << 41, q[0].time);
	v.m_pSelectedCreature = &c;
	Access::ChatString(v, &point, &c, 0);
	q = Queue(v);
	CHECK_EQ(1, int(q.size()));
	if (q.size() == 1) CHECK_EQ(QWORD(190) << 41, q[0].time);
	v.m_pSelectedCreature = nullptr;
}

TEST(TopView1Chat, TheBoxOutlineIsShadedByTheChatColourOrPinkForAShop)
{
	Hud scene;
	MTopView& v = *scene;
	scene.option.DrawChatBoxOutline = TRUE;
	Creature c;
	c.Become(kSlayer, 9, 1, 1, "Kim");
	c.Say({"a", "b"});
	POINT point = {30, 100};
	const struct { DWORD chatColor; WORD outline; } cases[] = {
		{0x0C000000u, WORD((8 << 11) | (4 << 5) | 4)},		// 12 - 8
		{0xFF000000u, WORD((24 << 11) | (20 << 5) | 20)},	// at most 20
		{0x03000000u, WORD(4 << 11)},						// at least 0
	};
	for (const auto& test : cases)
	{
		c.m_OriChatColor = test.chatColor;
		Access::ChatString(v, &point, &c, 0);
		const std::vector<Queued> q = Queue(v);
		CHECK_EQ(2, int(q.size()));
		if (q.size() != 2) continue;
		CHECK(q[0].boxOutline);
		CHECK_EQ(test.outline, q[0].boxOutlineColor);
		CHECK(!q[1].boxOutline);
	}
	// Recorded defect: Color(255, 0, 200) packs 8-bit values into 5:6:5,
	// so the shop's pink is (31, 0, 8): red.
	c.SetPersnalShop(1);
	Access::ChatString(v, &point, &c, 0);
	const std::vector<Queued> q = Queue(v);
	CHECK_EQ(2, int(q.size()));
	if (!q.empty()) CHECK_EQ(WORD((31 << 11) | 8), q[0].boxOutlineColor);
}

TEST(TopView1Chat, NoChatOrAHiddenShopMessageQueuesNothing)
{
	Hud scene;
	MTopView& v = *scene;
	Creature c;
	c.Become(kSlayer, 9, 1, 1, "Kim");
	POINT point = {30, 100};
	CHECK_EQ(100, Access::ChatString(v, &point, &c, 0));
	CHECK(v.m_pqDrawText.empty());
	c.Say({"sale"});
	c.SetPersnalShop(2);
	scene.option.DoNotShowPersnalShopMsg = TRUE;
	CHECK_EQ(100, Access::ChatString(v, &point, &c, 0));
	CHECK(v.m_pqDrawText.empty());
	c.SetPersnalShop(0);
	CHECK_EQ(80, Access::ChatString(v, &point, &c, 0));
	CHECK_EQ(1, int(Queue(v).size()));
}

TEST(TopView1Chat, AnEmptyLineEndsTheChatAndFiveLinesFill)
{
	Hud scene;
	MTopView& v = *scene;
	Creature c;
	c.Become(kSlayer, 9, 1, 1, "Kim");
	c.Say({"old", "", "b", "c"});
	POINT point = {30, 120};
	CHECK_EQ(80, Access::ChatString(v, &point, &c, 0));
	std::vector<Queued> q = Queue(v);
	CHECK_EQ(2, int(q.size()));
	if (q.size() == 2)
	{
		CHECK(TextOf(q[0]) == "b");
		CHECK(TextOf(q[1]) == "c");
	}
	c.Say({"1", "2", "3", "4", "5"});
	CHECK_EQ(20, Access::ChatString(v, &point, &c, 0));
	q = Queue(v);
	CHECK_EQ(5, int(q.size()));
	if (q.size() == 5)
	{
		CHECK(TextOf(q[0]) == "1");
		CHECK(TextOf(q[4]) == "5");
		CHECK_EQ(103, q[4].y);
	}
}

//----------------------------------------------------------------------
// HP changes over a creature (DrawCreatureHPModify)
//----------------------------------------------------------------------
namespace {

// A change's label as the view prints it, centred 24 pixels right of x.
void HPLabel(Canvas& canvas, Hud& scene, int x, int y, const char* text, COLORREF color)
{
	PrintInfo info = scene.base.m_chatting_pi;
	g_SetFL2Surface(&canvas.surface);
	g_PrintColorStrOut(x + 24 - g_GetStringWidth(text, info.hfont) / 2, y, text, info, color, RGB_BLACK);
	g_SetFL2Surface(s_textTarget);
}

const COLORREF kDamage = RGB(255, 150, 150), kHeal = RGB(150, 255, 150);

} // namespace

TEST(TopView1HPModify, ChangesShowOnlyToAPlayerWhoSeesHP)
{
	Hud scene;
	MTopView& v = *scene;
	Creature c;
	c.Become(kMonster, 9, 1, 1, "Kim");
	POINT point = {40, 60};
	c.AddHPModify(-7);
	Access::HPModify(v, &point, &c);
	CHECK(scene.text.Blank());
	CHECK_EQ(1, c.GetHPModifyListSize());
	scene.player->Set(EFFECTSTATUS_VIEW_HP);
	Creature none;
	none.Become(kMonster, 10, 1, 1, "Lee");
	Access::HPModify(v, &point, &none);
	CHECK(scene.text.Blank());
}

TEST(TopView1HPModify, ChangesArePrintedUpwardFromThePointAndTheOldestExpires)
{
	Hud scene;
	MTopView& v = *scene;
	scene.player->Set(EFFECTSTATUS_VIEW_HP);
	g_pClientConfig->HPModifyListTime = 10000;
	Creature c;
	c.Become(kMonster, 9, 1, 1, "Kim");
	c.AddHPModify(-7);
	scene.Later(5000);
	c.AddHPModify(12);
	scene.Later(5000);		// the first is exactly as old as its time: kept
	POINT point = {40, 60};
	Access::HPModify(v, &point, &c);
	Canvas expected;
	HPLabel(expected, scene, 40, 45, "-7", kDamage);
	HPLabel(expected, scene, 40, 60, "+12", kHeal);
	CHECK(Same(scene.text, expected));
	CHECK(!scene.text.Blank());
	CHECK_EQ(2, c.GetHPModifyListSize());
	CHECK(g_FL2_GetDC());		// the DC is released again
	g_FL2_ReleaseDC();
	scene.Later(1);			// now older: drawn once more, then dropped
	scene.text.surface.FillSurface(kPaper);
	Access::HPModify(v, &point, &c);
	CHECK(Same(scene.text, expected));
	CHECK_EQ(1, c.GetHPModifyListSize());
	CHECK_EQ(12, c.GetHPModifyList()->front().modify);
	scene.Later(5000);
	scene.text.surface.FillSurface(kPaper);
	Access::HPModify(v, &point, &c);
	Canvas last;
	HPLabel(last, scene, 40, 60, "+12", kHeal);
	CHECK(Same(scene.text, last));
	CHECK(c.IsEmptyHPModifyList());
}

TEST(TopView1HPModify, AnExpiredChangeBehindAFreshOneDropsTheFreshOne)
{
	Hud scene;
	MTopView& v = *scene;
	scene.player->Set(EFFECTSTATUS_VIEW_HP);
	g_pClientConfig->HPModifyListTime = 10000;
	Creature c;
	c.Become(kMonster, 9, 1, 1, "Kim");
	scene.Later(5000);
	c.AddHPModify(3);		// fresh, in front
	scene.Later(-5000);
	c.AddHPModify(-4);		// older, behind it
	scene.Later(10001);
	POINT point = {40, 60};
	Access::HPModify(v, &point, &c);
	// Recorded defect: an expired entry pops the list's front, not itself,
	// and the loop restarts there: the fresh +3 is dropped and the expired
	// -4 is printed twice.
	Canvas expected;
	HPLabel(expected, scene, 40, 45, "+3", kHeal);
	HPLabel(expected, scene, 40, 60, "-4", kDamage);
	HPLabel(expected, scene, 40, 75, "-4", kDamage);
	CHECK(Same(scene.text, expected));
	CHECK(c.IsEmptyHPModifyList());
}

//----------------------------------------------------------------------
// The guild mark in a siege war (DrawGuildMarkInSiegeWar)
//----------------------------------------------------------------------
TEST(TopView1SiegeMark, OnlySiegeWarriorsSeeEachOthersGuildMarks)
{
	Hud scene;
	MTopView& v = *scene;
	CSprite* mark = scene.marks.Small(7, 11);
	v.m_FirstZonePixel.x = 50;
	Creature c;
	c.Become(kSlayer, 9, 2, 3, "Kim");	// pixel x 96
	c.SetGuildNumber(7);
	c.SetSiegeWar(1);
	scene.player->SetSiegeWar(1);
	Access::SiegeMark(v, nullptr, 60);
	CHECK(scene.canvas.Blank());
	struct Gate { const char* what; void (*set)(Hud&, Creature&, bool); };
	const Gate gates[] = {
		{"ghost", [](Hud&, Creature& k, bool on) { k.Set(EFFECTSTATUS_GHOST, on); }},
		{"hallucinating player", [](Hud& h, Creature&, bool on) { h.player->Set(EFFECTSTATUS_HALLUCINATION, on); }},
		{"player out of the war", [](Hud& h, Creature&, bool on) { h.player->SetSiegeWar(on ? 0 : 1); }},
		{"creature out of the war", [](Hud&, Creature& k, bool on) { k.SetSiegeWar(on ? 0 : 1); }},
		{"no guild", [](Hud&, Creature& k, bool on) { k.SetGuildNumber(on ? 0 : 7); }},
		{"slayer operator", [](Hud&, Creature& k, bool on) { k.m_CreatureType = on ? CREATURETYPE_SLAYER_OPERATOR : kSlayer; }},
		{"vampire operator", [](Hud&, Creature& k, bool on) { k.m_CreatureType = on ? CREATURETYPE_VAMPIRE_OPERATOR : kSlayer; }},
		{"ousters operator", [](Hud&, Creature& k, bool on) { k.m_CreatureType = on ? CREATURETYPE_OUSTERS_OPERATOR : kSlayer; }},
	};
	for (const Gate& gate : gates)
	{
		gate.set(scene, c, true);
		Access::SiegeMark(v, &c, 60);
		if (!scene.canvas.Blank()) std::printf("      drawn for a %s\n", gate.what);
		CHECK(scene.canvas.Blank());
		gate.set(scene, c, false);
	}
	Access::SiegeMark(v, &c, 60);
	Canvas expected;
	POINT at = {96 - 50 + 15, 60 - 23};
	expected.surface.BltSprite(&at, mark);
	CHECK(Same(scene.canvas, expected));
	CHECK(!scene.canvas.Blank());
}

TEST(TopView1SiegeMark, AMissingMarkIsLookedUpOnceAndDrawsNothing)
{
	Hud scene;
	MTopView& v = *scene;
	Creature c;
	c.Become(kSlayer, 9, 2, 3, "Kim");
	c.SetGuildNumber(8);
	c.SetSiegeWar(2);
	scene.player->SetSiegeWar(2);
	CHECK(!scene.marks.HasGuildMark(8));
	Access::SiegeMark(v, &c, 60);
	CHECK(scene.marks.HasGuildMark(8));		// remembered as missing
	CHECK(scene.marks.GetGuildMarkSmall(8) == nullptr);
	CHECK(scene.canvas.Blank());
}

//----------------------------------------------------------------------
// A creature's name, HP bar, marks and request icon (DrawCreatureName)
//----------------------------------------------------------------------
namespace {

// Where a name's bar goes for the chat point (x, yPoint).
struct NameBar
{
	int left, right, maxHPBar, nameX, top;
	NameBar(int x, int yPoint, const char* name, int font)
	{
		const int namePixel = g_GetStringWidth(name, g_ClientPrintInfo[font]->hfont);
		maxHPBar = (std::max)(100, namePixel + 20);
		left = x + g_pClientConfig->POSITION_HP_BAR - 14;
		right = left + maxHPBar;
		nameX = left + ((maxHPBar - namePixel) >> 1);
		top = yPoint - 23;
	}
};

// q[i..i+2]: the bar's spent part under nothing, the name in black over
// the part left, and the name in its colour.
void CheckNameBar(MTopView& v, const std::vector<Queued>& q, size_t i, const NameBar& bar, const char* name,
	int font, COLORREF color, int currentPixels)
{
	CHECK(q.size() >= i + 3);
	if (q.size() < i + 3) return;
	const int bottom = bar.top + g_pClientConfig->FONT_HEIGHT;
	const Queued& base = q[i];
	CHECK_EQ(bar.nameX + 1, base.x);
	CHECK_EQ(bar.top + 5, base.y);
	CHECK(base.text == nullptr);
	CHECK_EQ(font, base.font);
	CHECK_EQ(currentPixels != bar.maxHPBar, base.box);
	if (base.box)
	{
		CHECK_EQ(bar.left + currentPixels, int(base.rect.left));
		CHECK_EQ(bar.top, int(base.rect.top));
		CHECK_EQ(bar.right, int(base.rect.right));
		CHECK_EQ(bottom, int(base.rect.bottom));
		CHECK_EQ(v.m_ColorHPBarBg, base.boxColor);
		CHECK(!base.translucent);
	}
	const Queued& black = q[i + 1];
	CHECK_EQ(bar.nameX + 1, black.x);
	CHECK_EQ(bar.top + 5, black.y);
	CHECK(TextOf(black) == name);
	CHECK_EQ(COLORREF(0), black.color);
	CHECK_EQ(font, black.font);
	CHECK_EQ(currentPixels != 0, black.box);
	if (black.box)
	{
		CHECK_EQ(bar.left, int(black.rect.left));
		CHECK_EQ(bar.top, int(black.rect.top));
		CHECK_EQ(bar.left + currentPixels, int(black.rect.right));
		CHECK_EQ(bottom, int(black.rect.bottom));
		CHECK_EQ(v.m_ColorHPBar, black.boxColor);
		CHECK(!black.translucent);
	}
	const Queued& named = q[i + 2];
	CHECK_EQ(bar.nameX, named.x);
	CHECK_EQ(bar.top + 4, named.y);
	CHECK(TextOf(named) == name);
	CHECK_EQ(color, named.color);
	CHECK_EQ(font, named.font);
	CHECK(!named.box);
}

const COLORREF kAlignment[5] = {RGB(10, 0, 0), RGB(11, 0, 0), RGB(12, 0, 0), RGB(13, 0, 0), RGB(14, 0, 0)};

} // namespace

TEST(TopView1Name, NoNameForGhostsGatesTheBuriedOrTheFarWhenBlind)
{
	Hud scene;
	MTopView& v = *scene;
	v.m_pointChatString = {60, 100};
	Access::Name(v, nullptr);
	CHECK(v.m_pqDrawText.empty());
	Creature c;
	c.Become(kMonster, 9, 5, 5, "Kim");
	auto drawn = [&]() { Access::Name(v, &c); return int(Queue(v).size()); };
	CHECK_EQ(3, drawn());
	c.Set(EFFECTSTATUS_GHOST);
	CHECK_EQ(0, drawn());
	c.Set(EFFECTSTATUS_GHOST, false);
	for (int gate = 725; gate <= 730; ++gate)
	{
		c.m_CreatureType = TYPE_CREATURETYPE(gate);
		CHECK_EQ(gate == 725 || gate == 730 ? 3 : 0, drawn());
	}
	c.m_CreatureType = kMonster;
	c.m_MoveType = MCreature::CREATURE_UNDERGROUND;
	CHECK_EQ(0, drawn());
	c.m_MoveType = MCreature::CREATURE_GROUND;
	for (int blind : {EFFECTSTATUS_YELLOW_POISON_TO_CREATURE, EFFECTSTATUS_FLARE, EFFECTSTATUS_BLINDNESS})
	{
		scene.player->Set(blind);
		c.MObject::SetPosition(6, 4);
		CHECK_EQ(3, drawn());		// one sector away: seen
		c.MObject::SetPosition(7, 5);
		CHECK_EQ(0, drawn());
		c.MObject::SetPosition(5, 3);
		CHECK_EQ(0, drawn());
		Access::Name(v, scene.player);	// the player's own name, though
		CHECK_EQ(3, int(Queue(v).size()));
		scene.player->Set(blind, false);
		CHECK_EQ(3, drawn());
	}
}

TEST(TopView1Name, TheHPBarIsQueuedUnderTheName)
{
	Hud scene;
	MTopView& v = *scene;
	v.m_pointChatString = {60, 100};
	Creature c;
	c.Become(kMonster, 9, 5, 5, "Kim");
	c.Status(MODIFY_ALIGNMENT, 2);
	const NameBar bar(60, 100, "Kim", kNameSlayer);
	const struct { DWORD hp, maxHp; int pixels; } cases[] = {
		{50, 100, bar.maxHPBar / 2},
		{100, 100, bar.maxHPBar},
		{0, 100, 0},
		{30, 0, bar.maxHPBar},		// no maximum: full
		{0, 0, 0},
		{150, 100, bar.maxHPBar},		// past the maximum: full
		{0xFFFFFFFFu, 100, 0},		// negative: empty
		{1, 300, bar.maxHPBar / 300},
	};
	for (const auto& test : cases)
	{
		c.Hp(test.hp, test.maxHp);
		v.m_bDrawRequest = true;
		Access::Name(v, &c);
		CHECK(!v.m_bDrawRequest);
		const std::vector<Queued> q = Queue(v);
		CHECK_EQ(3, int(q.size()));
		CheckNameBar(v, q, 0, bar, "Kim", kNameSlayer, kAlignment[2], test.pixels);
	}
	CHECK(scene.canvas.Blank());		// no marks
}

TEST(TopView1Name, TheNameIsColouredByRaceRelationAndRank)
{
	Hud scene;
	MTopView& v = *scene;
	v.m_pointChatString = {60, 100};
	Me& me = *scene.player;
	struct Case { const char* what; int type; int me; int mode; bool justice; int alignment; BYTE competence;
		bool hallu; COLORREF color; int font; };
	const COLORREF gold = COLORREF((31 << 11) | (23 << 5) | 3);
	const Case cases[] = {
		{"an NPC", kNPC, kSlayer, 0, false, 2, 3, false, RGB(1, 1, 1), kNameNPC},
		{"an NPC of rank 0", kNPC, kSlayer, 0, false, 2, 0, false, RGB(1, 1, 1), kNameNPC},
		{"Bathory", kBathory, kSlayer, 0, false, 2, 3, false, kAlignment[1], kNameVampire},
		// Recorded defect: the rank-0 (operator) gold is a 5:6:5 WORD used
		// as a COLORREF, so it prints as (0xE3, 0xFA, 0): green-yellow.
		{"rank 0", kMonster, kSlayer, 0, false, 2, 0, false, gold, kNameNPC},
		{"seen hallucinating", kMonster, kSlayer, 0, false, 2, 3, true, kAlignment[1], kNameVampire},
		{"a slayer's peer", kMonster, kSlayer, 0, false, 3, 3, false, kAlignment[3], kNameSlayer},
		{"a slayer's target", kMonster, kSlayer, 2, false, 3, 3, false, kAlignment[1], kNameVampire},
		{"a slayer's other tribe", kMonster, kSlayer, 1, false, 3, 3, false, kAlignment[1], kNameVampire},
		{"a slayer's own tribe", kSlayer, kSlayer, 1, false, 3, 3, false, kAlignment[3], kNameSlayer},
		{"a slayer's justice target", kMonster, kSlayer, 0, true, 3, 3, false, kAlignment[1], kNameVampire},
		{"an ousters' peer", kMonster, kOusters, 0, false, 4, 3, false, kAlignment[4], kNameVampire},
		{"an ousters' target", kMonster, kOusters, 2, false, 4, 3, false, kAlignment[1], kNameVampire},
		{"an ousters' justice target", kMonster, kOusters, 0, true, 4, 3, false, kAlignment[1], kNameVampire},
		{"a vampire's peer", kMonster, kVampire, 0, false, 0, 3, false, kAlignment[0], kNameVampire},
		{"a vampire's target", kMonster, kVampire, 2, false, 0, 3, false, kAlignment[1], kNameVampire},
		{"a vampire's justice target", kMonster, kVampire, 0, true, 0, 3, false, kAlignment[1], kNameVampire},
	};
	for (const Case& test : cases)
	{
		Creature c;
		c.Become(test.type, 9, 5, 5, "Kim");
		c.Hp(10, 10);
		c.Status(MODIFY_ALIGNMENT, DWORD(test.alignment));
		c.SetCompetence(test.competence);
		me.m_CreatureType = TYPE_CREATURETYPE(test.me);
		if (test.mode == 0) me.SetAttackModePeace();
		else if (test.mode == 1) me.SetAttackModeNormal();
		else me.SetAttackModeAggress();
		if (test.justice) scene.justice.AddCreature("Kim");
		me.Set(EFFECTSTATUS_HALLUCINATION, test.hallu);
		Access::Name(v, &c);
		const std::vector<Queued> q = Queue(v);
		CHECK_EQ(3, int(q.size()));
		if (q.size() == 3 && (q[2].color != test.color || q[2].font != test.font))
			std::printf("      %s: colour %06lx font %d\n", test.what, (unsigned long)q[2].color, q[2].font);
		if (q.size() == 3)
		{
			CHECK_EQ(test.color, q[2].color);
			CHECK_EQ(test.font, q[2].font);
			CHECK(TextOf(q[2]) == "Kim");
		}
		scene.justice.RemoveCreature("Kim");
	}
}

TEST(TopView1Name, TheLevelNameAndANicknameGoAboveTheName)
{
	Hud scene;
	MTopView& v = *scene;
	v.m_pointChatString = {60, 100};
	Creature c;
	c.Become(kMonster, 9, 5, 5, "Kim");
	c.Hp(10, 10);
	c.m_NameSelection.SelectLevelName(scene.model.levelNames, 1);
	const NameBar bar(60, 100, "Kim", kNameSlayer);
	Access::Name(v, &c);
	std::vector<Queued> q = Queue(v);
	CHECK_EQ(4, int(q.size()));
	if (q.size() == 4)
	{
		CHECK_EQ(61, q[0].x);
		CHECK_EQ(60, q[0].y);		// two font rows above, less 3, plus 3
		CHECK(TextOf(q[0]) == "Lord");
		CHECK_EQ(COLORREF(RGB(200, 250, 200)), q[0].color);
		CHECK_EQ(kNameSlayer, q[0].font);
		CHECK(!q[0].box);
	}
	CheckNameBar(v, q, 1, bar, "Kim", kNameSlayer, kAlignment[0], bar.maxHPBar);
	c.m_NameSelection.SelectLevelName(scene.model.levelNames, 0);
	const int nickX = bar.left + ((bar.maxHPBar - g_GetStringWidth("Nick", g_ClientPrintInfo[kUserID]->hfont)) >> 1);
	const struct { BYTE type; COLORREF color; WORD box; } nicks[] = {
		{NicknameInfo::NICK_BUILT_IN, RGB(255, 255, 0), WORD(31 << 11)},
		{NicknameInfo::NICK_QUEST, RGB(150, 150, 150), WORD(31)},
		{NicknameInfo::NICK_FORCED, RGB(255, 0, 0), 0},
		{NicknameInfo::NICK_CUSTOM_FORCED, RGB(255, 0, 0), 0},
		{NicknameInfo::NICK_CUSTOM, RGB(50, 170, 230), 0},
		{9, RGB(255, 255, 0), 0},
	};
	for (const auto& nick : nicks)
	{
		c.Nick(nick.type, "Nick");
		Access::Name(v, &c);
		q = Queue(v);
		CHECK_EQ(4, int(q.size()));
		if (q.size() != 4) continue;
		CHECK_EQ(nickX, q[3].x);
		CHECK_EQ(62, q[3].y);
		CHECK(TextOf(q[3]) == "Nick");
		CHECK_EQ(nick.color, q[3].color);
		CHECK_EQ(kUserID, q[3].font);
		CHECK_EQ(nick.box != 0, q[3].box);
		if (q[3].box)
		{
			CHECK_EQ(bar.left - 20, int(q[3].rect.left));
			CHECK_EQ(58, int(q[3].rect.top));
			CHECK_EQ(bar.right + 20, int(q[3].rect.right));
			CHECK_EQ(77, int(q[3].rect.bottom));
			CHECK_EQ(nick.box, q[3].boxColor);
			CHECK(!q[3].translucent);
		}
	}
	// An empty nickname, or no room above (row 41 - 42 < 0): none.
	c.Nick(NicknameInfo::NICK_CUSTOM, "");
	Access::Name(v, &c);
	CHECK_EQ(3, int(Queue(v).size()));
	c.Nick(NicknameInfo::NICK_CUSTOM, "Nick");
	v.m_pointChatString = {60, 41};
	Access::Name(v, &c);
	CHECK_EQ(3, int(Queue(v).size()));
	v.m_pointChatString = {60, 42};
	Access::Name(v, &c);
	CHECK_EQ(4, int(Queue(v).size()));
}

TEST(TopView1Name, GuildAndGradeMarksFlankTheBar)
{
	Hud scene;
	MTopView& v = *scene;
	v.m_pointChatString = {60, 100};
	CSprite* guild = scene.marks.Small(7, 11);
	CSprite* grade = scene.marks.Grade(RACE_SLAYER, 4, 13);
	CSprite* top = scene.marks.Grade(RACE_SLAYER, GRADE_MARK_MAX, 14);
	Creature c;
	c.Become(kSlayer, 9, 5, 5, "Kim");
	c.Hp(10, 10);
	c.SetGuildNumber(7);
	c.Status(MODIFY_RANK, 5);
	const NameBar bar(60, 100, "Kim", kNameSlayer);
	auto expect = [&](CSprite* left, CSprite* right)
	{
		Canvas expected;
		POINT at = {bar.left - 20, bar.top};
		expected.surface.Lock();
		if (left != nullptr) expected.surface.BltSprite(&at, left);
		expected.surface.Unlock();
		at = {bar.right, bar.top};
		expected.surface.Lock();
		if (right != nullptr) expected.surface.BltSprite(&at, right);
		expected.surface.Unlock();
		const bool same = Same(scene.canvas, expected);
		scene.canvas.surface.FillSurface(kPaper);
		Queue(v);
		return same;
	};
	Access::Name(v, &c);
	CHECK(!scene.canvas.surface.IsLock());		// each mark unlocks after its blit
	CHECK(!scene.canvas.Blank());
	CHECK(expect(guild, grade));
	c.Status(MODIFY_RANK, GRADE_MARK_MAX + 1);
	Access::Name(v, &c);
	CHECK(expect(guild, top));
	for (DWORD rank : {DWORD(0), DWORD(GRADE_MARK_MAX + 2)})
	{
		c.Status(MODIFY_RANK, rank);
		Access::Name(v, &c);
		CHECK(expect(guild, nullptr));
	}
	// Only a player's sprite gets a grade.
	c.Status(MODIFY_RANK, 5);
	c.m_CreatureType = kMonster;
	Access::Name(v, &c);
	CHECK(expect(guild, nullptr));
	c.m_CreatureType = kSlayer;
	// Neither while hallucinating, nor for operators and types 672 and 673.
	scene.player->Set(EFFECTSTATUS_HALLUCINATION);
	Access::Name(v, &c);
	CHECK(expect(nullptr, nullptr));
	scene.player->Set(EFFECTSTATUS_HALLUCINATION, false);
	for (int type : {CREATURETYPE_SLAYER_OPERATOR, CREATURETYPE_VAMPIRE_OPERATOR, CREATURETYPE_OUSTERS_OPERATOR, 672, 673})
	{
		c.m_CreatureType = TYPE_CREATURETYPE(type);
		Access::Name(v, &c);
		CHECK(expect(nullptr, nullptr));
	}
}

TEST(TopView1Name, AMissingGuildMarkIsLookedUpUnlessTheRaceIsMasked)
{
	Hud scene;
	MTopView& v = *scene;
	v.m_pointChatString = {60, 100};
	CSprite* grade = scene.marks.Grade(RACE_VAMPIRE, 4, 13);
	Creature c;
	c.Become(kSlayer, 9, 5, 5, "Kim");
	c.Hp(10, 10);
	c.SetGuildNumber(8);
	Access::Name(v, &c);
	CHECK(scene.marks.HasGuildMark(8));		// looked up, remembered missing
	CHECK(scene.canvas.Blank());
	Queue(v);
	// Another race's player where chat is masked by race: no guild at all.
	scene.zoneInfo->ChatMaskByRace = true;
	c.m_CreatureType = kVampire;
	c.SetGuildNumber(9);
	c.Status(MODIFY_RANK, 5);
	Access::Name(v, &c);
	CHECK(!scene.marks.HasGuildMark(9));
	const NameBar bar(60, 100, "Kim", kNameVampire);
	Canvas expected;
	POINT at = {bar.right, bar.top};
	expected.surface.Lock();
	expected.surface.BltSprite(&at, grade);
	expected.surface.Unlock();
	CHECK(Same(scene.canvas, expected));
	CHECK(!scene.canvas.Blank());
	Queue(v);
	// The same race keeps its guild.
	scene.player->m_CreatureType = kVampire;
	Access::Name(v, &c);
	CHECK(scene.marks.HasGuildMark(9));
	Queue(v);
}

TEST(TopView1Name, ARequestIconAnimatesOverAPlayerUnderTheMouse)
{
	Hud scene;
	MTopView& v = *scene;
	v.m_pointChatString = {60, 100};
	g_x = 100;
	g_y = 50;
	Creature c;
	c.Become(kSlayer, 9, 5, 5, "Kim");
	c.Hp(10, 10);
	const struct { MRequestMode::REQUEST_MODE mode; DWORD frame; int sprite; } cases[] = {
		{MRequestMode::REQUEST_TRADE, 0, SPRITEID_ITEM_TRADE},
		{MRequestMode::REQUEST_TRADE, 21, SPRITEID_ITEM_TRADE + 10},
		{MRequestMode::REQUEST_TRADE, 22, SPRITEID_ITEM_TRADE_LAST - 1},		// on the way back
		{MRequestMode::REQUEST_TRADE, 42, SPRITEID_ITEM_TRADE},				// back below the first
		{MRequestMode::REQUEST_PARTY, 0, SPRITEID_PARTY_REQUEST},
		{MRequestMode::REQUEST_PARTY, 12, SPRITEID_PARTY_REQUEST_LAST},
		{MRequestMode::REQUEST_PARTY, 21, SPRITEID_PARTY_REQUEST_LAST},		// held at the last
		{MRequestMode::REQUEST_INFO, 2, SPRITEID_INFO_REQUEST + 1},
		{MRequestMode::REQUEST_INFO, 24, SPRITEID_INFO_REQUEST_LAST},
	};
	for (const auto& test : cases)
	{
		v.SetRequestMode(test.mode);
		g_CurrentFrame = test.frame;
		v.m_bDrawRequest = false;
		Access::Name(v, &c);
		CHECK(!scene.canvas.surface.IsLock());
		CHECK(v.m_bDrawRequest);
		CHECK(!scene.canvas.Blank());
		CSprite& icon = v.m_EtcSPK[WORD(test.sprite)];
		Canvas expected;
		POINT at = {100 + 27 - icon.GetWidth(), 50 + 15 - icon.GetHeight()};
		expected.surface.Lock();
		expected.surface.BltSprite(&at, &icon);
		expected.surface.Unlock();
		CHECK(Same(scene.canvas, expected));
		scene.canvas.surface.FillSurface(kPaper);
		CHECK_EQ(3, int(Queue(v).size()));
	}
	// Not over an NPC, a monster, the dead, a casket or an operator.
	v.SetRequestMode(MRequestMode::REQUEST_TRADE);
	struct Who { const char* what; void (*set)(Creature&, bool); };
	const Who nobody[] = {
		{"an NPC", [](Creature& k, bool on) { k.m_CreatureType = on ? kNPC : kSlayer; }},
		{"a monster", [](Creature& k, bool on) { k.m_CreatureType = on ? kMonster : kSlayer; }},
		{"the dead", [](Creature& k, bool on) { k.m_bAlive = !on; }},
		{"a casket", [](Creature& k, bool on) { k.m_bInCasket = on; }},
		{"an operator", [](Creature& k, bool on) { k.m_CreatureType = on ? CREATURETYPE_SLAYER_OPERATOR : kSlayer; }},
		{"a vampire operator", [](Creature& k, bool on) { k.m_CreatureType = on ? CREATURETYPE_VAMPIRE_OPERATOR : kSlayer; }},
		{"an ousters operator", [](Creature& k, bool on) { k.m_CreatureType = on ? CREATURETYPE_OUSTERS_OPERATOR : kSlayer; }},
	};
	for (const Who& who : nobody)
	{
		who.set(c, true);
		v.m_bDrawRequest = true;
		Access::Name(v, &c);
		if (v.m_bDrawRequest || !scene.canvas.Blank()) std::printf("      an icon over %s\n", who.what);
		CHECK(!v.m_bDrawRequest);
		CHECK(scene.canvas.Blank());
		Queue(v);
		who.set(c, false);
	}
	v.SetRequestMode(MRequestMode::REQUEST_NULL);
	v.m_bDrawRequest = true;
	Access::Name(v, &c);
	CHECK(!v.m_bDrawRequest);
	CHECK(scene.canvas.Blank());
	Queue(v);
}

TEST(TopView1Name, AnOwnedPetShowsItsItemsLevelLeftOfTheBar)
{
	Hud scene;
	MTopView& v = *scene;
	v.m_pointChatString = {60, 100};
	CSprite* level = scene.marks.Level(3, 12);
	scene.marks.Level(4, 16);
	Figure<MFakeCreature> pet;
	pet.Become(kMonster, 9, 5, 5, "Pup");
	pet.Hp(10, 10);
	MPetItem item;
	item.SetNumber(3);
	pet.SetPetItem(&item);
	pet.SetOwnerID(scene.player->GetID());
	const NameBar bar(60, 100, "Pup", kNameSlayer);
	Access::Name(v, &pet);
	CHECK(!scene.canvas.surface.IsLock());
	Canvas expected;
	POINT at = {bar.left - 20, bar.top};
	expected.surface.BltSprite(&at, level);
	CHECK(Same(scene.canvas, expected));
	CHECK(!scene.canvas.Blank());
	Queue(v);
	// A pet without an owner, or without its item, shows none.
	scene.canvas.surface.FillSurface(kPaper);
	pet.SetOwnerID(OBJECTID_NULL);
	Access::Name(v, &pet);
	CHECK(scene.canvas.Blank());
	Queue(v);
	pet.SetOwnerID(scene.player->GetID());
	pet.SetPetItem(nullptr);
	Access::Name(v, &pet);
	CHECK(scene.canvas.Blank());
	Queue(v);
}

TEST(TopView1Name, ChatGoesAboveTheNameAndHPChangesAtItsTop)
{
	Hud scene;
	MTopView& v = *scene;
	v.m_pointChatString = {60, 100};
	scene.player->Set(EFFECTSTATUS_VIEW_HP);
	Creature c;
	c.Become(kMonster, 9, 5, 5, "Kim");
	c.Hp(10, 10);
	c.Say({"hey"});
	c.AddHPModify(-5);
	Access::Name(v, &c);
	const std::vector<Queued> q = Queue(v);
	CHECK_EQ(4, int(q.size()));
	if (!q.empty())
	{
		CHECK(TextOf(q[0]) == "hey");
		CHECK_EQ(COLORREF(RGB(255, 255, 255)), q[0].color);
		CHECK(q[0].outline);
		CHECK_EQ(83, q[0].y);
	}
	CheckNameBar(v, q, 1, NameBar(60, 80, "Kim", kNameSlayer), "Kim", kNameSlayer, kAlignment[0], NameBar(60, 80, "Kim", kNameSlayer).maxHPBar);
	Canvas expected;
	HPLabel(expected, scene, 60, 80, "-5", kDamage);
	CHECK(Same(scene.text, expected));
}

TEST(TopView1Name, WhereChatIsMaskedByRaceAnotherRacesPlayerHasNoName)
{
	Hud scene;
	MTopView& v = *scene;
	v.m_pointChatString = {60, 100};
	scene.zoneInfo->ChatMaskByRace = true;
	Figure<MCreatureWear> c;
	c.Become(kVampire, 9, 5, 5, "Kim");
	c.Hp(10, 10);
	auto names = [&]() { Access::Name(v, &c); return int(Queue(v).size()); };
	// The hallucination name stands in, and the monster name table is empty.
	CHECK_EQ(0, names());
	c.SetName("GMKim");				// an operator's name: shown
	CHECK_EQ(3, names());
	c.SetName("Kim");
	g_pUserInformation->IsMaster = TRUE;
	CHECK_EQ(3, names());
	g_pUserInformation->IsMaster = FALSE;
	scene.player->Set(EFFECTSTATUS_GHOST);
	CHECK_EQ(3, names());
	scene.player->Set(EFFECTSTATUS_GHOST, false);
	scene.player->m_CreatureType = CREATURETYPE_SLAYER_OPERATOR;
	CHECK_EQ(3, names());
	scene.player->m_CreatureType = kVampire;		// the same race
	CHECK_EQ(3, names());
	scene.player->m_CreatureType = kSlayer;
	scene.zoneInfo->ChatMaskByRace = false;
	CHECK_EQ(3, names());
	// A hallucinating player sees every creature's hallucination name.
	scene.zoneInfo->ChatMaskByRace = true;
	scene.player->m_CreatureType = kVampire;
	scene.player->Set(EFFECTSTATUS_HALLUCINATION);
	CHECK_EQ(0, names());
	scene.player->Set(EFFECTSTATUS_HALLUCINATION, false);
	// Recorded defect (not run: undefined behaviour): an empty operator
	// prefix is a NULL MString, so DrawCreatureName calls
	// strncmp(name, NULL, 0), which glibc declares nonnull. Were it to
	// compare equal, the empty prefix would match every name and nobody's
	// name would be masked.
}

//----------------------------------------------------------------------
// The player's own name (DrawCreatureMyName)
//----------------------------------------------------------------------
TEST(TopView1MyName, ThePlayersNameAndBarSitOverItsHead)
{
	Hud scene;
	MTopView& v = *scene;
	Me& me = *scene.player;		// at (5,5): pixel (240, 120), 50 tall
	v.m_FirstZonePixel = {180, 0};
	g_pUserInformation->CharacterID = "Hero";
	g_char_slot_ingame.alignment = UI_GOOD;
	const NameBar bar(60, 70, "Hero", kUserID);
	const struct { DWORD hp, maxHp; int pixels; } cases[] = {
		{30, 60, bar.maxHPBar / 2},
		{60, 60, bar.maxHPBar},
		{90, 60, bar.maxHPBar},
		{0, 60, 0},
		{30, 0, 0},		// no maximum: empty, where a creature's is full
	};
	for (const auto& test : cases)
	{
		me.Hp(test.hp, test.maxHp);
		Access::MyName(v);
		const std::vector<Queued> q = Queue(v);
		CHECK_EQ(3, int(q.size()));
		CheckNameBar(v, q, 0, bar, "Hero", kUserID, kAlignment[3], test.pixels);
	}
	me.Hp(60, 60);
	g_char_slot_ingame.alignment = UI_EVIL_PLUS;
	me.Nick(NicknameInfo::NICK_QUEST, "Champ");
	me.Say({"yo"});
	Access::MyName(v);
	const std::vector<Queued> q = Queue(v);
	CHECK_EQ(5, int(q.size()));
	if (q.size() == 5)
	{
		CHECK(TextOf(q[0]) == "yo");
		CHECK_EQ(me.GetChatColor(), q[0].color);
		CHECK(!q[0].outline);
		const NameBar raised(60, 50, "Hero", kUserID);
		CheckNameBar(v, q, 1, raised, "Hero", kUserID, kAlignment[0], raised.maxHPBar);
		CHECK_EQ(raised.left + ((raised.maxHPBar - g_GetStringWidth("Champ", g_ClientPrintInfo[kUserID]->hfont)) >> 1), q[4].x);
		CHECK_EQ(12, q[4].y);
		CHECK(TextOf(q[4]) == "Champ");
		CHECK_EQ(COLORREF(RGB(150, 150, 150)), q[4].color);
		CHECK(q[4].box);
		CHECK_EQ(raised.left - 20, int(q[4].rect.left));
		CHECK_EQ(8, int(q[4].rect.top));
		CHECK_EQ(raised.right + 20, int(q[4].rect.right));
		CHECK_EQ(27, int(q[4].rect.bottom));
		CHECK_EQ(WORD(31), q[4].boxColor);
	}
	// Unlike a creature's, the nickname is queued even above the screen.
	me.Say({});
	v.m_FirstZonePixel = {180, 40};
	Access::MyName(v);
	const std::vector<Queued> high = Queue(v);
	CHECK_EQ(4, int(high.size()));
	if (high.size() == 4) CHECK_EQ(-8, high[3].y);
}

TEST(TopView1MyName, NothingForAGhostOrWithoutACharacterName)
{
	Hud scene;
	MTopView& v = *scene;
	scene.player->Say({"yo"});
	g_pUserInformation->CharacterID = "Hero";
	scene.player->Set(EFFECTSTATUS_GHOST);
	Access::MyName(v);
	CHECK(v.m_pqDrawText.empty());
	scene.player->Set(EFFECTSTATUS_GHOST, false);
	g_pUserInformation->CharacterID = static_cast<const char*>(nullptr);
	Access::MyName(v);
	CHECK(v.m_pqDrawText.empty());
}

TEST(TopView1MyName, ThePlayersMarksUnlessHallucinating)
{
	Hud scene;
	MTopView& v = *scene;
	Me& me = *scene.player;
	v.m_FirstZonePixel = {180, 0};
	g_pUserInformation->CharacterID = "Hero";
	me.Hp(10, 10);
	CSprite* guild = scene.marks.Small(7, 11);
	CSprite* grade = scene.marks.Grade(RACE_SLAYER, 2, 13);
	me.SetGuildNumber(7);
	me.Status(MODIFY_RANK, 3);
	const NameBar bar(60, 70, "Hero", kUserID);
	Access::MyName(v);
	Canvas expected;
	POINT at = {bar.left - 20, bar.top};
	expected.surface.Lock();
	expected.surface.BltSprite(&at, guild);
	expected.surface.Unlock();
	at = {bar.right, bar.top};
	expected.surface.Lock();
	expected.surface.BltSprite(&at, grade);
	expected.surface.Unlock();
	CHECK(Same(scene.canvas, expected));
	CHECK(!scene.canvas.Blank());
	Queue(v);
	scene.canvas.surface.FillSurface(kPaper);
	me.SetGuildNumber(8);		// missing: looked up
	Access::MyName(v);
	CHECK(scene.marks.HasGuildMark(8));
	Queue(v);
	me.SetGuildNumber(0);		// no guild: not looked up
	Access::MyName(v);
	CHECK(!scene.marks.HasGuildMark(0));
	Queue(v);
	scene.canvas.surface.FillSurface(kPaper);
	me.Set(EFFECTSTATUS_HALLUCINATION);
	me.SetGuildNumber(7);
	Access::MyName(v);
	CHECK(scene.canvas.Blank());
	Queue(v);
}

TEST(TopView1MyName, ANicknameIsColouredAndBoxedByItsType)
{
	Hud scene;
	MTopView& v = *scene;
	Me& me = *scene.player;
	v.m_FirstZonePixel = {180, 0};
	g_pUserInformation->CharacterID = "Hero";
	g_char_slot_ingame.alignment = UI_GOOD;		// a name colour no nickname takes
	me.Hp(60, 60);
	const struct { BYTE type; COLORREF color; WORD box; } nicks[] = {
		{NicknameInfo::NICK_BUILT_IN, RGB(255, 255, 0), WORD(31 << 11)},
		{NicknameInfo::NICK_QUEST, RGB(150, 150, 150), WORD(31)},
		{NicknameInfo::NICK_FORCED, RGB(255, 0, 0), 0},
		{NicknameInfo::NICK_CUSTOM_FORCED, RGB(255, 0, 0), 0},
		{NicknameInfo::NICK_CUSTOM, RGB(50, 170, 230), 0},
		{9, RGB(255, 255, 0), 0},
	};
	for (const auto& nick : nicks)
	{
		me.Nick(nick.type, "Champ");
		Access::MyName(v);
		const std::vector<Queued> q = Queue(v);
		CHECK_EQ(4, int(q.size()));
		if (q.size() != 4) continue;
		CHECK(TextOf(q[3]) == "Champ");
		CHECK_EQ(nick.color, q[3].color);
		CHECK_EQ(nick.box != 0, q[3].box);
		if (q[3].box) CHECK_EQ(nick.box, q[3].boxColor);
	}
}

//----------------------------------------------------------------------
// The event's text (DrawEventString)
//----------------------------------------------------------------------
namespace {

const COLORREF kEventColor = RGB(29 << 3, 29 << 3, 12 << 3);

// Text as the HUD prints it: black one pixel down-right, then the colour.
void Shadowed(Canvas& canvas, int x, int y, const char* text, int font, COLORREF color)
{
	Print(canvas, x + 1, y + 1, text, font, 0);
	Print(canvas, x, y, text, font, color);
}

// An event line centred on x.
void EventLine(Canvas& canvas, int x, int y, const char* text)
{
	Shadowed(canvas, x - g_GetStringWidth(text, TextSystem::EncodeFontSizeHandle(10)) / 2, y, text, 0, kEventColor);
}

void Strings(std::initializer_list<std::pair<int, const char*>> strings)
{
	for (const auto& entry : strings) *g_pGameStringTable->GetMutable(entry.first) = entry.second;
}

} // namespace

TEST(TopView1Events, NothingShowsWithoutAnEventThatShowsText)
{
	Hud scene;
	MTopView& v = *scene;
	int x = 80, y = 30;
	Access::EventString(v, x, y);
	scene.events.Add(EVENTID_LOGOUT, EVENTFLAG_SHAKE_SCREEN, 1000, {100});
	Strings({{100, "Alpha"}});
	Access::EventString(v, x, y);
	CHECK_EQ(80, x);
	CHECK_EQ(30, y);
	CHECK(scene.text.Blank());
}

TEST(TopView1Events, EachStringIsCentredOnItsOwnRow)
{
	Hud scene;
	MTopView& v = *scene;
	Strings({{100, "Alpha"}, {101, "Beta"}});
	// Past the table, before it, and an entry the file left empty: skipped.
	scene.events.Add(EVENTID_LOGOUT, EVENTFLAG_SHOW_STRING, 1000, {100, 102, MAX_GAME_STRING, -1, 101});
	int x = 80, y = 30;
	Access::EventString(v, x, y);
	CHECK_EQ(80, x);
	CHECK_EQ(70, y);
	Canvas expected;
	EventLine(expected, 80, 30, "Alpha");
	EventLine(expected, 80, 50, "Beta");
	CHECK(Same(scene.text, expected));
	CHECK_EQ(kEventColor, scene.fonts[0].text_color);
	// No strings at all: one empty pass.
	scene.events.RemoveAllEvent();
	scene.events.Add(EVENTID_LOGOUT, EVENTFLAG_SHOW_STRING, 1000);
	y = 30;
	Access::EventString(v, x, y);
	CHECK_EQ(30, y);
}

TEST(TopView1Events, TheDelayCountsDownInWholeSecondsOncePerString)
{
	Hud scene;
	MTopView& v = *scene;
	scene.events.Add(EVENTID_LOGOUT, EVENTFLAG_SHOW_DELAY, 5500);
	const struct { int later; const char* shown; } steps[] = {
		{1000, "5"},
		{3500, "1"},
		{999, "1"},
		{1, "0"},
		// Recorded defect: past the delay the int - DWORD difference wraps,
		// so the count shows 4294967 until the event is removed.
		{1000, "4294967"},
	};
	for (const auto& step : steps)
	{
		scene.Later(step.later);
		scene.text.surface.FillSurface(kPaper);
		int x = 80, y = 30;
		Access::EventString(v, x, y);
		CHECK_EQ(50, y);
		Canvas expected;
		EventLine(expected, 80, 30, step.shown);
		const bool same = Same(scene.text, expected);
		if (!same) std::printf("      not \"%s\"\n", step.shown);
		CHECK(same);
	}
	scene.events.RemoveAllEvent();
	scene.events.Add(EVENTID_LOGOUT, EVENTFLAG_SHOW_DELAY, 2000, {7, 8});
	scene.text.surface.FillSurface(kPaper);
	int x = 80, y = 30;
	Access::EventString(v, x, y);
	CHECK_EQ(70, y);
	Canvas expected;
	EventLine(expected, 80, 30, "2");
	EventLine(expected, 80, 50, "2");
	CHECK(Same(scene.text, expected));
}

TEST(TopView1Events, ADelayStringFormatsTheSecondsAndTheFirstEventShows)
{
	Hud scene;
	MTopView& v = *scene;
	Strings({{103, "Wait %d s"}, {104, "Other %d"}});
	scene.events.Add(EVENTID_LOGOUT, EVENTFLAG_SHOW_DELAY_STRING, 3000, {103, MAX_GAME_STRING});
	scene.events.Add(EVENTID_RESURRECT, EVENTFLAG_SHOW_DELAY_STRING, 9000, {104});	// a later id
	int x = 60, y = 10;
	Access::EventString(v, x, y);
	CHECK_EQ(30, y);
	Canvas expected;
	EventLine(expected, 60, 10, "Wait 3 s");
	CHECK(Same(scene.text, expected));
}

//----------------------------------------------------------------------
// The HUD's messages (DrawInformation)
//----------------------------------------------------------------------
namespace {

const COLORREF kSystemColor = RGB(29 << 3, 8 << 3, 12 << 3);
const COLORREF kNoticeColor = RGB(20 << 3, 31 << 3, 12 << 3);
const COLORREF kGameColor = RGB(8 << 3, 28 << 3, 8 << 3);
const int kBoard = 640;

MonotonicClock::TimePoint s_infoTime(MonotonicClock::Duration(1LL << 50));

// The HUD drawn once an hour past every earlier draw: each of its four
// scroll timers restarts now. Returns the time.
MonotonicClock::TimePoint RestartTimers(Hud& scene, Canvas& board)
{
	s_infoTime += MonotonicClock::Duration(3600000);
	g_FrameNow = s_infoTime;
	Access::Information(*scene);
	board.surface.FillSurface(kPaper);
	return s_infoTime;
}

void At(MonotonicClock::TimePoint start, int millis) { g_FrameNow = start + MonotonicClock::Duration(millis); }

} // namespace

TEST(TopView1Information, MessagesArePrintedInTheirRowsAndColours)
{
	Hud scene;
	MTopView& v = *scene;
	Canvas board(kBoard, kBoard);
	Aim(&board.surface);
	g_MyFull = FALSE;
	RestartTimers(scene, board);
	g_pSystemMessage->Add("sys");
	g_pSystemMessage->Add("two");
	g_pPlayerMessage->Add("hi1");
	g_pPlayerMessage->Add("ok2");
	g_pPlayerMessage->Add("zz9");
	g_pNoticeMessage->Add("news");
	g_pNoticeMessage->Add("more");
	g_pGameMessage->Add("g1");
	g_pGameMessage->Add("g2");
	Access::Information(v);
	Canvas expected(kBoard, kBoard);
	Shadowed(expected, 10, 145, "sys", 0, kSystemColor);	// a slayer's: 45 + 10 + 70 + 20
	Shadowed(expected, 10, 165, "two", 0, kSystemColor);
	Shadowed(expected, 10, 40, "hi", 0, RGB(50, 50, 200));
	Shadowed(expected, 10, 60, "ok", 0, RGB(255, 255, 0));
	Shadowed(expected, 10, 80, "zz", 0, kNoticeColor);		// any other digit
	Shadowed(expected, 10, 100, "news", 8, kNoticeColor);
	Shadowed(expected, 10, 116, "more", 8, kNoticeColor);
	Shadowed(expected, 10, 410, "g2", 0, kGameColor);		// the newest lowest
	Shadowed(expected, 10, 390, "g1", 0, kGameColor);
	CHECK(Same(board, expected));
	CHECK(!board.Blank());
	CHECK(scene.canvas.Blank());	// no bullets
}

TEST(TopView1Information, APlayerMessagesLastCharacterIsItsColour)
{
	Hud scene;
	MTopView& v = *scene;
	Canvas board(kBoard, kBoard);
	Aim(&board.surface);
	RestartTimers(scene, board);
	g_pPlayerMessage->Add("ab0");
	g_pPlayerMessage->Add("cdx");
	Access::Information(v);
	Canvas expected(kBoard, kBoard);
	Shadowed(expected, 10, 40, "ab", 0, RGB(0, 255, 0));
	// Recorded defect: a message that does not end in a digit loses its
	// last character all the same and is green (atoi of a letter is 0).
	Shadowed(expected, 10, 60, "cd", 0, RGB(0, 255, 0));
	CHECK(Same(board, expected));
}

TEST(TopView1Information, GameMessagesMoveWithRaceBonusPointsAndFullScreen)
{
	Hud scene;
	MTopView& v = *scene;
	Canvas board(kBoard, kBoard);
	Aim(&board.surface);
	Me& me = *scene.player;
	const struct { int type; DWORD bonus; BOOL full; int system; int x; int y; } cases[] = {
		{kSlayer, 0, FALSE, 145, 10, 410},
		{kSlayer, 3, FALSE, 145, 85, 410},
		{kSlayer, 0, TRUE, 145, 10, 578},
		{kVampire, 0, FALSE, 148, 10, 440},
		{kVampire, 1, TRUE, 148, 85, 608},
		{kOusters, 0, FALSE, 148, 10, 410},		// not a slayer: 48
		{kOusters, 2, TRUE, 148, 85, 578},
	};
	for (const auto& test : cases)
	{
		me.m_CreatureType = TYPE_CREATURETYPE(test.type);
		me.Status(MODIFY_BONUS_POINT, test.bonus);
		g_MyFull = test.full;
		RestartTimers(scene, board);
		for (CMessageArray* array : {g_pSystemMessage, g_pGameMessage}) array->Clear();
		g_pSystemMessage->Add("s");
		g_pGameMessage->Add("g");
		Access::Information(v);
		Canvas expected(kBoard, kBoard);
		Shadowed(expected, 10, test.system, "s", 0, kSystemColor);
		Shadowed(expected, test.x, test.y, "g", 0, kGameColor);
		CHECK(Same(board, expected));
		board.surface.FillSurface(kPaper);
	}
}

TEST(TopView1Information, TheNoInformationEventHidesItAllAndStopsTheScrolling)
{
	Hud scene;
	MTopView& v = *scene;
	Canvas board(kBoard, kBoard);
	Aim(&board.surface);
	const MonotonicClock::TimePoint start = RestartTimers(scene, board);
	g_pSystemMessage->Add("sys");
	scene.events.Add(EVENTID_LOGOUT, EVENTFLAG_NOT_DRAW_INFORMATION | EVENTFLAG_SHOW_STRING, 1000, {100});
	Strings({{100, "Alpha"}});
	At(start, 60000);
	Access::Information(v);
	CHECK(board.Blank());
	CHECK(std::string((*g_pSystemMessage)[2]) == "sys");
	// Without it the event's string shows, centred on the game area.
	scene.events.RemoveAllEvent();
	scene.events.Add(EVENTID_LOGOUT, EVENTFLAG_SHOW_STRING, 1000, {100});
	g_GameRect = {0, 0, 400, 300};
	g_pSystemMessage->Clear();
	At(start, 0);
	Access::Information(v);
	Canvas expected(kBoard, kBoard);
	EventLine(expected, 200, 30, "Alpha");
	CHECK(Same(board, expected));
}

TEST(TopView1Information, EachMessageListScrollsOnItsOwnTimer)
{
	Hud scene;
	MTopView& v = *scene;
	Canvas board(kBoard, kBoard);
	Aim(&board.surface);
	g_pClientConfig->DELAY_SYSTEMMESSAGE = 5000;
	g_pClientConfig->DELAY_GAMEMESSAGE = 6000;
	const MonotonicClock::TimePoint start = RestartTimers(scene, board);
	CMessageArray* arrays[4] = {g_pSystemMessage, g_pNoticeMessage, g_pPlayerMessage, g_pGameMessage};
	for (CMessageArray* array : arrays) array->Add("x9");
	// Where each list's message is now: 2 is the newest row, -1 gone.
	auto row = [](CMessageArray* array)
	{
		for (int i = 0; i < 3; ++i) if (std::string((*array)[i]) == "x9") return i;
		return -1;
	};
	const struct { int millis; int system, notice, player, game; } steps[] = {
		{4999, 2, 2, 2, 2},
		{5000, 1, 2, 2, 2},		// the system list's 5 s
		{5999, 1, 2, 2, 2},
		{6000, 1, 2, 2, 1},		// the game list's own delay
		{6999, 1, 2, 2, 1},
		{7000, 1, 1, 2, 1},		// notices: 2 s longer
		{9999, 1, 1, 2, 1},
		{10000, 0, 1, 2, 1},
		{11999, 0, 1, 2, 1},
		{12000, 0, 1, 2, 0},
		{13999, 0, 1, 2, 0},
		{14000, 0, 0, 2, 0},
		{14999, 0, 0, 2, 0},
		{15000, -1, 0, 1, 0},		// player messages: 10 s longer
		{18000, -1, 0, 1, -1},
	};
	for (const auto& step : steps)
	{
		At(start, step.millis);
		Access::Information(v);
		if (row(arrays[0]) != step.system || row(arrays[1]) != step.notice || row(arrays[2]) != step.player || row(arrays[3]) != step.game)
			std::printf("      at %d ms: %d %d %d %d\n", step.millis, row(arrays[0]), row(arrays[1]), row(arrays[2]), row(arrays[3]));
		CHECK_EQ(step.system, row(arrays[0]));
		CHECK_EQ(step.notice, row(arrays[1]));
		CHECK_EQ(step.player, row(arrays[2]));
		CHECK_EQ(step.game, row(arrays[3]));
	}
}

TEST(TopView1Information, TheMagazinesBulletsAreCounted)
{
	Hud scene;
	MTopView& v = *scene;
	Canvas board(kBoard, kBoard), sheet(kBoard, 64);
	Aim(&board.surface);
	CSpriteSurface* p = &sheet.surface;
	v.SetSurface(p);
	RestartTimers(scene, board);
	sheet.surface.FillSurface(kPaper);
	std::unique_ptr<MItem> magazine(MMagazine::NewItem());
	magazine->SetNumber(37);
	g_pCurrentMagazine = static_cast<MMagazine*>(magazine.get());
	Access::Information(v);
	CHECK(!sheet.surface.IsLock());		// unlocked after the bullet's blit
	CSprite& bullet = v.m_EtcSPK[SPRITEID_BULLET];
	Canvas expectedSheet(kBoard, 64), expected(kBoard, kBoard);
	POINT at = {225, 9};
	expectedSheet.surface.Lock();
	expectedSheet.surface.BltSprite(&at, &bullet);
	expectedSheet.surface.Unlock();
	CHECK(Same(sheet, expectedSheet));
	CHECK(!sheet.Blank());
	Shadowed(expected, 225 + bullet.GetWidth() + 2, 9 + bullet.GetHeight() - 18, "37", 0, RGB(30 << 3, 28 << 3, 30 << 3));
	CHECK(Same(board, expected));
	g_pCurrentMagazine = nullptr;
	p = &scene.canvas.surface;
	v.SetSurface(p);
}
