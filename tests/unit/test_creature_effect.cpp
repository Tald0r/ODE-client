#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "CreatureEffectDraw.h"
#include "MCreatureWear.h"
#include "MFakeCreature.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "EffectSpriteTypeDef.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "MItem.h"
#include "MItemOptionTable.h"
#include "AdvancementAction.h"
#include "FrameClock.h"
#include "AddonDef.h"
#include "Types/ModifyDef.h"
#include "CSpriteSurface.h"
#include "CIndexSprite.h"
#include "CIndexSprite565.h"
#include "CSpriteOutlineManager.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <vector>

// A creature drawn with an effect (task 4.250: MTopView's members until
// then, gameview's since). Each test draws through the moved code onto
// one surface and lays the blits it should make, with the colour sets
// named, onto another; the two pictures must match. SpriteLib's SDL
// backend darkens a darkened index sprite but draws a brightened or
// colour-set one as a plain one, so a picture shows where each part
// went, in which colours and how dark, not a brightening or an effect
// colour.
namespace {

const char* kSpritePath = "creature_effect_sprite.bin";
const int kSize = 112;

// A sprite of five pixels: colour-set channels 0 to 3 at gradation 2,
// then a fixed colour, its mark.
void LoadMark(CIndexSprite& sprite, WORD mark)
{
	const std::vector<WORD> words{5, 1, 9, 1, 0, 4, 0x0002, 0x0102, 0x0202, 0x0302, 1, mark};
	{
		std::ofstream out(kSpritePath, std::ios::binary | std::ios::trunc);
		for (WORD word : words) { out.put(char(word & 0xFF)); out.put(char(word >> 8)); }
	}
	std::ifstream in(kSpritePath, std::ios::binary);
	CHECK(sprite.LoadFromFile(in));
	in.close();
	std::remove(kSpritePath);
}

// Every frame ID, action and direction of the pack, without frames.
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

// Frame `frame` of (id, action, direction), the last, is the sprite at
// its offset; the frames before it are sprite 0 at (frame, 0).
void Place(CCreatureFramePack& pack, int id, int action, int direction, int frame, int sprite, int cx, int cy)
{
	FRAME_ARRAY& frames = pack[TYPE_FRAMEID(id)][BYTE(action)][BYTE(direction)];
	frames.Init(WORD(frame + 1));
	for (int before = 0; before < frame; ++before) frames[WORD(before)].Set(0, short(before), 0);
	frames[WORD(frame)].Set(TYPE_SPRITEID(sprite), short(cx), short(cy));
}

struct Canvas
{
	CSpriteSurface surface;
	Canvas() { CHECK_EQ(0, spritectl_init()); CHECK(surface.Init(kSize, kSize)); surface.FillSurface(0); }
	std::vector<WORD> Picture()
	{
		std::vector<WORD> pixels;
		DWORD pitch = 0;
		const unsigned char* base = static_cast<const unsigned char*>(surface.Lock(nullptr, &pitch));
		CHECK(base != nullptr);
		if (base == nullptr) return pixels;
		for (int y = 0; y < kSize; ++y)
			for (int x = 0; x < kSize; ++x)
				pixels.push_back(reinterpret_cast<const WORD*>(base + y * pitch)[x]);
		surface.Unlock();
		return pixels;
	}
	bool Blank() { for (WORD pixel : Picture()) if (pixel != 0) return false; return true; }
	// A plain blit, or one darkened by `darkBits`.
	void Blit(int x, int y, CIndexSprite& sprite, int darkBits = 0)
	{
		POINT point = { x, y };
		surface.Lock();
		if (darkBits == 0) surface.BltIndexSprite(&point, &sprite);
		else surface.BltIndexSpriteDarkness(&point, &sprite, BYTE(darkBits));
		surface.Unlock();
	}
};

// A creature whose parts and state a test sets: of the type and bare.
template<class Base> struct Probe : Base
{
	using Base::m_Addon;
	using Base::m_ShoulderAddon;
	using Base::m_Action;
	using Base::m_ActionCount;
	using Base::m_ActionCountMax;
	using Base::m_cX;
	using Base::m_cY;
	using Base::m_InvisibleCount;
	using Base::m_bEffectStatus;
	using Base::m_AttachEffectColor;

	void Become(int type)
	{
		this->SetCreatureType(TYPE_CREATURETYPE(type));
		for (auto& addon : m_Addon) addon.bAddon = FALSE;
		m_ShoulderAddon.bAddon = FALSE;
		Pose(ACTION_STAND, 0);
	}
	MCreatureWear::ADDON_INFO& Wear(int slot, int frameID, ITEM_CLASS itemClass, int colorSet1, int colorSet2, int itemType = 0)
	{
		MCreatureWear::ADDON_INFO& addon = slot == ADDON_MAX ? m_ShoulderAddon : m_Addon[slot];
		addon.bAddon = TRUE;
		addon.FrameID = TYPE_FRAMEID(frameID);
		addon.ItemClass = itemClass;
		addon.ItemType = TYPE_ITEMTYPE(itemType);
		addon.ColorSet1 = WORD(colorSet1);
		addon.ColorSet2 = WORD(colorSet2);
		addon.bEffectColor = FALSE;
		return addon;
	}
	void Pose(int action, int frame)
	{
		m_Action = BYTE(action);
		m_ActionCountMax = 100;
		m_ActionCount = BYTE(frame);
	}
	void Set(EFFECTSTATUS status) { m_bEffectStatus[status] = true; }
};
using Wearer = Probe<MCreatureWear>;
using Pet = Probe<MFakeCreature>;

const int kSlayer = 1, kVampire = 2;

struct Scene
{
	GameModelWorld world;
	CREATURE_TABLE creatureTypes;
	EFFECTSPRITETYPE_TABLE effectSprites;
	EFFECTSTATUS_TABLE effectStatuses;
	CREATURESPRITE_TABLE sprites;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	RankBonusTable rankBonuses;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	DWORD oldFrame = g_CurrentFrame;
	int savedSets[4];

	// The view's packs, each sprite pack's three sprites marked apart.
	CCreatureFramePack creatureFPK, addonFPK, slayerManFPK, slayerWomanFPK;
	CIndexSpritePack creatureSPK, addonSPK, slayerManSPK, slayerWomanSPK;
	CSpriteOutlineManager outlines;
	Canvas drawn, expected;
	CreatureDrawPacks packs;
	POINT at = { 40, 30 };

	Scene()
	{
		CHECK_EQ(0, spritectl_init());
		CIndexSprite::SetColorSet();
		for (int set = 0; set < 4; ++set) savedSets[set] = CIndexSprite::GetUsingColorSet(BYTE(set));
		// A creature's draw gap is 0 off the frame it snapped on (0).
		g_CurrentFrame = 1;
		creatureTypes.Init(800);
		for (int type : {kSlayer, kVampire, 702, 703})
		{
			testfw::MutableRow(creatureTypes, type).SetCreatureTribe(type == kSlayer ? CREATURETRIBE_SLAYER
				: CREATURETRIBE_VAMPIRE, ACTION_MAX_SLAYER);
		}
		g_pCreatureTable = &creatureTypes;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		sprites.Init(1);
		g_pCreatureSpriteTable = &sprites;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		g_pRankBonusTable = &rankBonuses;
		const int advancedActions = (std::max)({int(ACTION_ADVANCEMENT_SLAYER_MAX), int(ACTION_ADVANCEMENT_OUSTERS_MAX),
			int(ACTION_ADVANCEMENT_MAX)}) - ADVANCEMENT_ACTION_START;
		const int newActions = int(ACTION_NEW_SLAYER_MAX);
		Shape(creatureFPK, 223, ACTION_MAX_SLAYER);
		Shape(addonFPK, 4, ACTION_MAX_SLAYER);
		for (CCreatureFramePack* pPack : {&slayerManFPK, &slayerWomanFPK})
			Shape(*pPack, AC_PART_MAX + 4, (std::max)(advancedActions, newActions));
		int n = 0;
		for (CIndexSpritePack* pPack : {&creatureSPK, &addonSPK, &slayerManSPK, &slayerWomanSPK})
		{
			pPack->Init(3);
			for (int sprite = 0; sprite < 3; ++sprite, ++n) LoadMark((*pPack)[WORD(sprite)], WORD(0x8410 | ((n * 0x0821) & 0x7BEF)));
		}
		packs.pSurface = &drawn.surface;
		packs.pCreatureFPK = &creatureFPK;
		packs.pCreatureSPK = &creatureSPK;
		packs.pAddonFPK = &addonFPK;
		packs.pAddonSPK = &addonSPK;
		packs.pAdvancementSlayerManFPK = &slayerManFPK;
		packs.pAdvancementSlayerManSPK = &slayerManSPK;
		packs.pAdvancementSlayerWomanFPK = &slayerWomanFPK;
		packs.pAdvancementSlayerWomanSPK = &slayerWomanSPK;
		packs.pOutlines = &outlines;
	}
	~Scene()
	{
		for (int set = 0; set < 4; ++set) CIndexSprite::SetUsingColorSetOnly(BYTE(set), savedSets[set]);
		g_CurrentFrame = oldFrame;
		g_pRankBonusTable = oldRankBonuses;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pCreatureSpriteTable = oldSprites;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pCreatureTable = oldCreatureTypes;
	}
	// The drawn picture is the expected one, and not empty.
	bool Same()
	{
		const std::vector<WORD> a = drawn.Picture(), b = expected.Picture();
		bool any = false;
		for (WORD pixel : a) any |= pixel != 0;
		if (!any) std::printf("      nothing was drawn\n");
		for (size_t i = 0; i < a.size() && i < b.size(); ++i)
			if (a[i] != b[i])
			{
				std::printf("      the picture differs first at (%d,%d): %04x, not %04x\n", int(i % kSize), int(i / kSize), a[i], b[i]);
				break;
			}
		return any && a == b;
	}
	void Clear()
	{
		drawn.surface.FillSurface(0);
		expected.surface.FillSurface(0);
	}
	typedef void (*Effect)(const CreatureDrawPacks&, POINT*, MCreature*, int, int, int);
	void Draw(Effect draw, MCreature& creature, int action, int direction, int frame)
	{
		POINT point = at;
		creature.ClearScreenRect();
		drawn.surface.Lock();
		draw(packs, &point, &creature, action, direction, frame);
		drawn.surface.Unlock();
	}
	// A blit at (dx, dy) from where the creature stands, in its colours,
	// darkened by `darkBits`.
	void Blit(int dx, int dy, CIndexSprite& sprite, int colorSet1, int colorSet2, int darkBits = 0)
	{
		CIndexSprite::SetUsingColorSet(colorSet1, colorSet2);
		expected.Blit(at.x + dx, at.y + dy, sprite, darkBits);
	}
	// The wipe at `count` of a blit at (dx, dy), in the colours last set.
	void Wipe(int dx, int dy, CIndexSprite& sprite, int count)
	{
		POINT point = { at.x + dx, at.y + dy };
		CSpriteSurface::s_Value1 = count;
		CIndexSprite::SetEffect(CIndexSprite::EFFECT_WIPE_OUT);
		expected.surface.Lock();
		expected.surface.BltIndexSpriteEffect(&point, &sprite);
		expected.surface.Unlock();
	}
};

int Special(int colorSet) { return MItem::GetSpecialColorItemColorset(colorSet); }

// The sprite effects' global state (the index sprites' and the surface's
// effect, and the value they read), put back as it was.
struct IndexEffect : CIndexSprite565
{
	static decltype(s_pMemcpyEffectFunction) Get() { return s_pMemcpyEffectFunction; }
	static void Set(decltype(s_pMemcpyEffectFunction) effect) { s_pMemcpyEffectFunction = effect; }
};
struct EffectGuard
{
	decltype(IndexEffect::Get()) index = IndexEffect::Get();
	decltype(CSpriteSurface::s_pMemcpyEffectFunction) surface = CSpriteSurface::s_pMemcpyEffectFunction;
	int value = CSpriteSurface::s_Value1;
	~EffectGuard()
	{
		IndexEffect::Set(index);
		CSpriteSurface::s_pMemcpyEffectFunction = surface;
		CSpriteSurface::s_Value1 = value;
	}
};

bool SameRect(const RECT& a, int left, int top, int right, int bottom)
{
	const bool same = a.left == left && a.top == top && a.right == right && a.bottom == bottom;
	if (!same) std::printf("      the screen rect is (%d,%d)-(%d,%d), not (%d,%d)-(%d,%d)\n",
		int(a.left), int(a.top), int(a.right), int(a.bottom), left, top, right, bottom);
	return same;
}

// Where an effect lays a part drawn at (x, y) for a creature that moved
// (cx, cy) at once, and how dark (0 plain).
struct Lay { int dx, dy, darkBits; };
std::vector<Lay> FadeOutLays(int, int) { return { { -25, 0, 2 }, { 25, 0, 2 } }; }
std::vector<Lay> FastMoveLays(int cx, int cy) { return { { -cx, -cy, 2 }, { -(cx >> 1), -(cy >> 1), 1 }, { 0, 0, 0 } }; }
std::vector<Lay> InPlaceLays(int, int) { return { { 0, 0, 0 } }; }

// A weapon's afterimage f frames back is darkened by this.
const int kAfterimageDarkBits[6] = { 0, 1, 1, 2, 2, 3 };

struct Kind
{
	const char* name;
	Scene::Effect plain;
	Scene::Effect advanced;
	std::vector<Lay> (*lays)(int, int);
};

const Kind kKinds[] = {
	{ "fade out", DrawFadeOut, DrawFadeOutForACSlayer, FadeOutLays },
	{ "fast move", DrawFastMove, DrawFastMoveForACSlayer, FastMoveLays },
	{ "invisible", DrawInvisible, DrawInvisibleForACSlayer, InPlaceLays },
	{ "weapon fade out", DrawWeaponFadeOut, DrawWeaponFadeOutForACSlayer, InPlaceLays },
};

} // namespace

//----------------------------------------------------------------------
// The four effects, on a slayer's parts
//----------------------------------------------------------------------
TEST(CreatureEffect, EachLaysASlayersPartsWhereItShouldInTheirColours)
{
	for (const Kind& kind : kKinds)
	{
		Scene scene;
		Wearer slayer;
		slayer.Become(kSlayer);
		slayer.SetCurrentDirection(2);
		slayer.m_cX = 8;
		slayer.m_cY = 4;
		slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
		slayer.Wear(ADDON_HELM, 2, ITEM_CLASS_HELM, 40, 50);
		Place(scene.addonFPK, 1, ACTION_MOVE, 2, 2, 1, 2, 3);
		Place(scene.addonFPK, 2, ACTION_MOVE, 2, 2, 2, 14, 30);
		scene.Draw(kind.plain, slayer, ACTION_MOVE, 2, 2);
		for (const Lay& lay : kind.lays(8, 4))
		{
			scene.Blit(2 + lay.dx, 3 + lay.dy, scene.addonSPK[1], 30, 60, lay.darkBits);
			scene.Blit(14 + lay.dx, 30 + lay.dy, scene.addonSPK[2], 40, 50, lay.darkBits);
		}
		const bool same = scene.Same();
		if (!same) std::printf("      %s\n", kind.name);
		CHECK(same);
		// The screen rect covers the parts where they stand.
		CHECK(SameRect(slayer.GetScreenRect(), 42, 33, 59, 61));
	}
}

TEST(CreatureEffect, AUniqueItemsColourTakesBothSetsButACoatsKeepsItsFirst)
{
	for (const Kind& kind : kKinds)
		for (int special : {UNIQUE_ITEM_COLOR, QUEST_ITEM_COLOR})
		{
			Scene scene;
			Wearer slayer;
			slayer.Become(kSlayer);
			slayer.SetCurrentDirection(2);
			slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, special);
			slayer.Wear(ADDON_HELM, 2, ITEM_CLASS_HELM, 40, special);
			Place(scene.addonFPK, 1, ACTION_MOVE, 2, 2, 1, 2, 3);
			Place(scene.addonFPK, 2, ACTION_MOVE, 2, 2, 2, 14, 30);
			scene.Draw(kind.plain, slayer, ACTION_MOVE, 2, 2);
			for (const Lay& lay : kind.lays(0, 0))
			{
				scene.Blit(2 + lay.dx, 3 + lay.dy, scene.addonSPK[1], 30, Special(special), lay.darkBits);
				scene.Blit(14 + lay.dx, 30 + lay.dy, scene.addonSPK[2], Special(special), Special(special), lay.darkBits);
			}
			const bool same = scene.Same();
			if (!same) std::printf("      %s\n", kind.name);
			CHECK(same);
		}
}

TEST(CreatureEffect, NothingForAFrameTheActionHasNotGotOrForWhatIsNotWorn)
{
	for (const Kind& kind : kKinds)
	{
		Scene scene;
		Wearer slayer;
		slayer.Become(kSlayer);
		slayer.SetCurrentDirection(2);
		slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
		Place(scene.addonFPK, 1, ACTION_MOVE, 2, 2, 1, 2, 3);
		Place(scene.addonFPK, 2, ACTION_MOVE, 2, 2, 2, 14, 30);
		scene.Draw(kind.plain, slayer, ACTION_MOVE, 2, 3);
		CHECK(scene.drawn.Blank());
		CHECK(SameRect(slayer.GetScreenRect(), 800, 600, 0, 0));
		slayer.m_Addon[ADDON_COAT].bAddon = FALSE;
		scene.Draw(kind.plain, slayer, ACTION_MOVE, 2, 2);
		CHECK(scene.drawn.Blank());
	}
}

TEST(CreatureEffect, APetMovingFastIsDrawnToo)
{
	// A pet that brightens and one that darkens draw where a creature does;
	// where it stands the second is a bit darker (the SDL backend draws a
	// brightened blit as it is).
	for (MFakeCreature::FAKE_CREATURE_TYPE type : {MFakeCreature::FAKE_CREATURE_BRIGHTNESS, MFakeCreature::FAKE_CREATURE_STAND})
	{
		Scene scene;
		Pet pet;
		pet.Become(kSlayer);
		pet.SetFakeCreatureType(type);
		pet.SetCurrentDirection(2);
		pet.m_cX = 6;
		pet.m_cY = 2;
		pet.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
		Place(scene.addonFPK, 1, ACTION_MOVE, 2, 2, 1, 2, 3);
		scene.Draw(DrawFastMove, pet, ACTION_MOVE, 2, 2);
		const int darkBits = type == MFakeCreature::FAKE_CREATURE_BRIGHTNESS ? 0 : 1;
		for (const Lay& lay : FastMoveLays(6, 2))
			scene.Blit(2 + lay.dx, 3 + lay.dy, scene.addonSPK[1], 30, 60, lay.dx == 0 && lay.dy == 0 ? darkBits : lay.darkBits);
		CHECK(scene.Same());
	}
}

TEST(CreatureEffect, AnInvisibleSlayerWipesInBetweenItsEnds)
{
	// Wholly there (0) it draws plainly and wholly gone (64) darkened by the
	// snipping shift (2), which the darkened blit leaves in s_Value1; in
	// between it draws that, sets the wipe effect to its count and draws
	// through it over the darkened copy.
	for (int count : {0, 64, 32})
	{
		Scene scene;
		EffectGuard effects;
		Wearer slayer;
		slayer.Become(kSlayer);
		slayer.SetCurrentDirection(2);
		slayer.m_InvisibleCount = count;
		slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
		Place(scene.addonFPK, 1, ACTION_MOVE, 2, 2, 1, 2, 3);
		CSpriteSurface::s_Value1 = 99;
		CIndexSprite::SetEffect(CIndexSprite::EFFECT_NET);
		scene.Draw(DrawInvisible, slayer, ACTION_MOVE, 2, 2);
		// Before the expected blits, which set both again.
		CHECK_EQ(count == 32 ? 32 : count == 64 ? 2 : 99, CSpriteSurface::s_Value1);
		CHECK_EQ(count == 32, CIndexSprite::IsWipeOutEffect());
		scene.Blit(2, 3, scene.addonSPK[1], 30, 60, count == 0 ? 0 : 2);
		if (count == 32) scene.Wipe(2, 3, scene.addonSPK[1], 32);
		CHECK(scene.Same());
	}
}

//----------------------------------------------------------------------
// The weapon's afterimages
//----------------------------------------------------------------------
namespace {

// A slayer swinging a sword on frame `frame` of an attack facing 2, its
// sword's frames 0 to `frame` at (10 + 4f, 20), sprite 1 the last, sprite
// 2 the others; a coat on its own frame.
struct Swing
{
	Scene scene;
	Wearer slayer;
	explicit Swing(int frame)
	{
		slayer.Become(kSlayer);
		slayer.SetCurrentDirection(2);
		slayer.Wear(ADDON_RIGHTHAND, 2, ITEM_CLASS_SWORD, 40, 50);
		slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
		FRAME_ARRAY& frames = scene.addonFPK[2][ACTION_ATTACK][2];
		frames.Init(WORD(frame + 1));
		for (int f = 0; f <= frame; ++f) frames[WORD(f)].Set(TYPE_SPRITEID(f == frame ? 1 : 2), short(10 + 4 * f), 20);
		Place(scene.addonFPK, 1, ACTION_ATTACK, 2, frame, 0, 2, 3);
	}
	// The afterimages of the frames before, at most five, darker the older
	// (but under Rediance, which colours them), then the sword; a fading
	// slayer's sword and coat are darkened by a bit.
	void Expect(int frame, int afterimages, bool rediance = false, bool fading = false)
	{
		for (int f = 1; f <= afterimages; ++f)
			scene.Blit(10 + 4 * (frame - f), 20, scene.addonSPK[2], 40, 50, rediance ? 0 : kAfterimageDarkBits[f]);
		scene.Blit(10 + 4 * frame, 20, scene.addonSPK[1], 40, 50, fading ? 1 : 0);
		scene.Blit(2, 3, scene.addonSPK[0], 30, 60, fading ? 1 : 0);
	}
};

} // namespace

TEST(CreatureEffect, ASwungWeaponLeavesUpToFiveAfterimages)
{
	for (int frame : {0, 1, 3, 5, 7})
	{
		Swing swing(frame);
		swing.scene.Draw(DrawWeaponFadeOut, swing.slayer, ACTION_ATTACK, 2, frame);
		swing.Expect(frame, (std::min)(frame, 5));
		const bool same = swing.scene.Same();
		if (!same) std::printf("      frame %d\n", frame);
		CHECK(same);
	}
}

TEST(CreatureEffect, UnderRedianceOrAnEffectColourOrFadingTheWeaponIsStillDrawn)
{
	// Under Rediance the afterimages take the effect colour set, and an
	// effect-coloured part keeps it (the SDL backend draws a colour set as
	// the sprite as it is: in the colours last set).
	{
		Swing swing(3);
		swing.slayer.Set(EFFECTSTATUS_REDIANCE);
		swing.slayer.m_Addon[ADDON_RIGHTHAND].bEffectColor = TRUE;
		swing.slayer.m_Addon[ADDON_RIGHTHAND].EffectColorSet = 77;
		swing.scene.Draw(DrawWeaponFadeOut, swing.slayer, ACTION_ATTACK, 2, 3);
		swing.Expect(3, 3, true);
		CHECK(swing.scene.Same());
	}
	// A fading slayer's parts draw too, darkened (one scene at a time:
	// each makes and frees the model's globals).
	Swing fading(2);
	fading.slayer.SetFade();
	fading.scene.Draw(DrawWeaponFadeOut, fading.slayer, ACTION_ATTACK, 2, 2);
	fading.Expect(2, 2, false, true);
	CHECK(fading.scene.Same());
}

//----------------------------------------------------------------------
// The advanced slayer
//----------------------------------------------------------------------
TEST(CreatureEffect, AnAdvancedSlayerLaysItsPartsFromItsGendersPack)
{
	for (const Kind& kind : kKinds)
		for (bool male : {true, false})
		{
			Scene scene;
			Wearer slayer;
			slayer.Become(kSlayer);
			slayer.SetMale(male);
			slayer.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
			slayer.m_cX = 8;
			slayer.m_cY = 4;
			slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
			slayer.Wear(ADDON_HELM, 2, ITEM_CLASS_HELM, 40, UNIQUE_ITEM_COLOR);
			const int action = ConvAdvancementSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer) - ADVANCEMENT_ACTION_START;
			CCreatureFramePack& pack = male ? scene.slayerManFPK : scene.slayerWomanFPK;
			CIndexSpritePack& sprites = male ? scene.slayerManSPK : scene.slayerWomanSPK;
			// The parts of a coat and a helm, not their frame IDs.
			Place(pack, AC_BODY, action, 2, 2, 1, 2, 3);
			Place(pack, AC_HELMET, action, 2, 2, 2, 14, 30);
			scene.Draw(kind.advanced, slayer, ACTION_MAGIC, 2, 2);
			for (const Lay& lay : kind.lays(8, 4))
			{
				scene.Blit(2 + lay.dx, 3 + lay.dy, sprites[1], 30, 60, lay.darkBits);
				scene.Blit(14 + lay.dx, 30 + lay.dy, sprites[2], Special(UNIQUE_ITEM_COLOR), Special(UNIQUE_ITEM_COLOR), lay.darkBits);
			}
			const bool same = scene.Same();
			if (!same) std::printf("      %s\n", kind.name);
			CHECK(same);
			CHECK(SameRect(slayer.GetScreenRect(), 42, 33, 59, 61));
		}
}

TEST(CreatureEffect, AnAdvancedSlayerWithoutAnAdvancedActionIsNotDrawn)
{
	for (const Kind& kind : kKinds)
	{
		Scene scene;
		Wearer slayer;
		slayer.Become(kSlayer);
		slayer.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
		slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
		CHECK_EQ(-1, ConvAdvancementSlayerActionFromSlayerAction(ACTION_ATTACK, &slayer));
		for (int action = 0; action < 8; ++action)
			for (int direction = 0; direction < 8; ++direction)
				Place(scene.slayerManFPK, AC_BODY, action, direction, 2, 1, 2, 3);
		scene.Draw(kind.advanced, slayer, ACTION_ATTACK, 2, 2);
		CHECK(scene.drawn.Blank());
	}
}

TEST(CreatureEffect, AnAdvancedSlayersNewItemsTakeTheirOwnFramesAndActions)
{
	// Coats from item type 28, swords from 16 and the rest of the list are
	// the new items: drawn from their frame ID on the new slayer's action,
	// a coat from two frame sets (the frame ID's and the next).
	for (const Kind& kind : kKinds)
	{
		Scene scene;
		Wearer slayer;
		slayer.Become(kSlayer);
		slayer.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
		slayer.m_cX = 8;
		slayer.m_cY = 4;
		slayer.Wear(ADDON_COAT, 5, ITEM_CLASS_COAT, 30, 60, 28);
		slayer.Wear(ADDON_HELM, 8, ITEM_CLASS_HELM, 40, 50, 15);
		const int action = ConvNewSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer);
		CHECK(ConvAdvancementSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer) != -1);
		Place(scene.slayerManFPK, 5, action, 2, 2, 0, 2, 3);
		Place(scene.slayerManFPK, 6, action, 2, 2, 1, 26, 3);
		Place(scene.slayerManFPK, 8, action, 2, 2, 2, 14, 30);
		scene.Draw(kind.advanced, slayer, ACTION_MAGIC, 2, 2);
		for (const Lay& lay : kind.lays(8, 4))
		{
			scene.Blit(2 + lay.dx, 3 + lay.dy, scene.slayerManSPK[0], 30, 60, lay.darkBits);
			scene.Blit(26 + lay.dx, 3 + lay.dy, scene.slayerManSPK[1], 30, 60, lay.darkBits);
			scene.Blit(14 + lay.dx, 30 + lay.dy, scene.slayerManSPK[2], 40, 50, lay.darkBits);
		}
		const bool same = scene.Same();
		if (!same) std::printf("      %s\n", kind.name);
		CHECK(same);
		// One below the list is an old item: its part.
		slayer.Wear(ADDON_COAT, 5, ITEM_CLASS_COAT, 30, 60, 27);
		slayer.m_Addon[ADDON_HELM].bAddon = FALSE;
		Place(scene.slayerManFPK, AC_BODY, ConvAdvancementSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer) - ADVANCEMENT_ACTION_START,
			2, 2, 2, 7, 7);
		scene.Clear();
		scene.Draw(kind.advanced, slayer, ACTION_MAGIC, 2, 2);
		for (const Lay& lay : kind.lays(8, 4)) scene.Blit(7 + lay.dx, 7 + lay.dy, scene.slayerManSPK[2], 30, 60, lay.darkBits);
		CHECK(scene.Same());
	}
}

TEST(CreatureEffect, AnAdvancedSlayersNewItemOutsideItsPackIsSkipped)
{
	// A frame ID past the pack, or an action past the frame set's, draws
	// nothing.
	for (const Kind& kind : kKinds)
	{
		Scene scene;
		Wearer slayer;
		slayer.Become(kSlayer);
		slayer.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
		slayer.Wear(ADDON_HELM, AC_PART_MAX + 4, ITEM_CLASS_HELM, 40, 50, 15);
		scene.Draw(kind.advanced, slayer, ACTION_MAGIC, 2, 2);
		CHECK(scene.drawn.Blank());
		scene.slayerManFPK[TYPE_FRAMEID(8)].Init(1);
		slayer.Wear(ADDON_HELM, 8, ITEM_CLASS_HELM, 40, 50, 15);
		scene.Draw(kind.advanced, slayer, ACTION_MAGIC, 2, 2);
		CHECK(scene.drawn.Blank());
	}
}

TEST(CreatureEffect, AnAdvancedSlayersSwungWeaponLeavesItsAfterimages)
{
	// The afterimages are the right hand's: a sword's, not the shoulder
	// piece's; for a sword's part and for a new sword alike; up to five,
	// a unique sword's in its colour in both sets.
	struct Case { int frame; int colorSet2; };
	for (bool newItems : {false, true})
		for (const Case& c : {Case{3, 50}, Case{7, UNIQUE_ITEM_COLOR}})
		{
			Scene scene;
			Wearer slayer;
			slayer.Become(kSlayer);
			slayer.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
			slayer.Wear(ADDON_RIGHTHAND, 2, ITEM_CLASS_SWORD, 40, c.colorSet2, newItems ? 16 : 0);
			// The shoulder piece, which has no slot of the plain slayer's.
			slayer.Wear(ADDON_MAX, 3, ITEM_CLASS_SHOULDER_ARMOR, 30, 60);
			const int action = ConvAdvancementSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer) - ADVANCEMENT_ACTION_START;
			const int newAction = ConvNewSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer);
			for (int part : {int(AC_SWORD), int(AC_SHOULDER)})
			{
				const bool sword = part == AC_SWORD;
				FRAME_ARRAY& frames = sword && newItems ? scene.slayerManFPK[2][BYTE(newAction)][2]
					: scene.slayerManFPK[TYPE_FRAMEID(part)][BYTE(action)][2];
				frames.Init(WORD(c.frame + 1));
				for (int f = 0; f <= c.frame; ++f)
					frames[WORD(f)].Set(TYPE_SPRITEID(f == c.frame ? 1 : 2), short((sword ? 10 : 50) + 4 * f), short(sword ? 20 : 60));
			}
			scene.Draw(DrawWeaponFadeOutForACSlayer, slayer, ACTION_MAGIC, 2, c.frame);
			const int set1 = c.colorSet2 == UNIQUE_ITEM_COLOR ? Special(UNIQUE_ITEM_COLOR) : 40;
			const int set2 = c.colorSet2 == UNIQUE_ITEM_COLOR ? Special(UNIQUE_ITEM_COLOR) : c.colorSet2;
			for (int f = 1; f <= (std::min)(c.frame, 5); ++f) scene.Blit(10 + 4 * (c.frame - f), 20, scene.slayerManSPK[2], set1, set2, kAfterimageDarkBits[f]);
			scene.Blit(10 + 4 * c.frame, 20, scene.slayerManSPK[1], set1, set2);
			scene.Blit(50 + 4 * c.frame, 60, scene.slayerManSPK[1], 30, 60);
			const bool same = scene.Same();
			if (!same) std::printf("      %s, frame %d\n", newItems ? "a new sword" : "a sword's part", c.frame);
			CHECK(same);
		}
}

TEST(CreatureEffect, AnAdvancedSlayersWeaponUnderRedianceOrAnEffectColourOrFadingIsStillDrawn)
{
	// Rediance's afterimages and an effect-coloured weapon go through the
	// colour-set blit (drawn plainly by SDL, in the colours last set); a
	// fading one's afterimages darken as ever and the weapon by a bit: all
	// of them drawn, for the sword's part and for a new sword.
	for (bool newItems : {false, true})
		for (int kind : {0, 1})
		{
			Scene scene;
			Wearer slayer;
			slayer.Become(kSlayer);
			slayer.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
			MCreatureWear::ADDON_INFO& sword = slayer.Wear(ADDON_RIGHTHAND, 2, ITEM_CLASS_SWORD, 40, 50, newItems ? 16 : 0);
			if (kind == 0)
			{
				slayer.Set(EFFECTSTATUS_REDIANCE);
				sword.bEffectColor = TRUE;
				sword.EffectColorSet = 77;
			}
			else slayer.SetFade();
			const int action = newItems ? ConvNewSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer)
				: ConvAdvancementSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer) - ADVANCEMENT_ACTION_START;
			FRAME_ARRAY& frames = scene.slayerManFPK[TYPE_FRAMEID(newItems ? 2 : int(AC_SWORD))][BYTE(action)][2];
			frames.Init(3);
			for (int f = 0; f <= 2; ++f) frames[WORD(f)].Set(TYPE_SPRITEID(f == 2 ? 1 : 2), short(10 + 4 * f), 20);
			scene.Draw(DrawWeaponFadeOutForACSlayer, slayer, ACTION_MAGIC, 2, 2);
			for (int f = 1; f <= 2; ++f) scene.Blit(10 + 4 * (2 - f), 20, scene.slayerManSPK[2], 40, 50, kind == 0 ? 0 : kAfterimageDarkBits[f]);
			scene.Blit(18, 20, scene.slayerManSPK[1], 40, 50, kind == 0 ? 0 : 1);
			const bool same = scene.Same();
			if (!same) std::printf("      %s, %s\n", newItems ? "a new sword" : "a sword's part", kind == 0 ? "Rediance" : "fading");
			CHECK(same);
		}
}

//----------------------------------------------------------------------
// The turrets
//----------------------------------------------------------------------
TEST(CreatureEffect, APetsTurretFacesItsTurretDirectionInTheBodysColours)
{
	for (int type : {702, 703})
	{
		Scene scene;
		Pet pet;
		pet.Become(type);
		pet.SetID(7);
		pet.SetCurrentDirection(3);
		pet.SetBodyColor1(UNIQUE_ITEM_COLOR);
		pet.SetBodyColor2(QUEST_ITEM_COLOR);
		pet.SetTurreDirection(5);
		FRAME_ARRAY& frames = scene.creatureFPK[4][ACTION_VAMPIRE_DRAIN][2];
		frames.Init(6);
		frames[5].Set(2, 20, 9);
		scene.Draw([](const CreatureDrawPacks& packs, POINT* p, MCreature* c, int a, int d, int f) {
			DrawCentauroTurret(packs, p, c, a, d, f, 4); }, pet, ACTION_STAND, 3, 0);
		// 702's turret sits off its centre by its facing.
		scene.Blit(20 + (type == 702 ? -7 : 0), 9 + (type == 702 ? 2 : 0), scene.creatureSPK[2], Special(UNIQUE_ITEM_COLOR), Special(QUEST_ITEM_COLOR));
		CHECK(scene.Same());
		// Selected, it goes to the outlines instead.
		scene.packs.selectedCreatureID = 7;
		scene.Clear();
		scene.Draw([](const CreatureDrawPacks& packs, POINT* p, MCreature* c, int a, int d, int f) {
			DrawCentauroTurret(packs, p, c, a, d, f, 4); }, pet, ACTION_STAND, 3, 0);
		CHECK(scene.drawn.Blank());
		CHECK_EQ(size_t(1), scene.outlines.GetOutlines().size());
		if (scene.outlines.GetOutlines().size() == 1)
		{
			const OutlineEntry& entry = scene.outlines.GetOutlines()[0];
			CHECK_EQ(scene.at.x + 20 + (type == 702 ? -7 : 0), entry.x);
			CHECK_EQ(scene.at.y + 9 + (type == 702 ? 2 : 0), entry.y);
			CHECK(entry.pIndexSprite == &scene.creatureSPK[2]);
		}
	}
}

TEST(CreatureEffect, AFiringTurretRecoils)
{
	// Delay d moves it back 2d along the facing (turret / 4 + 2) % 8.
	Scene scene;
	Pet pet;
	pet.Become(703);
	pet.SetTurreDirection(9);
	pet.SetTurretDelay(3);
	FRAME_ARRAY& frames = scene.creatureFPK[4][ACTION_VAMPIRE_DRAIN][2];
	frames.Init(10);
	frames[9].Set(2, 30, 20);
	scene.Draw([](const CreatureDrawPacks& packs, POINT* p, MCreature* c, int a, int d, int f) {
		DrawCentauroTurret(packs, p, c, a, d, f, 4); }, pet, ACTION_STAND, 3, 0);
	// Facing (9 / 4 + 2) % 8 = 4: (1, 0).
	CIndexSprite::SetUsingColorSet(pet.GetBodyColor1(), pet.GetBodyColor2());
	scene.expected.Blit(scene.at.x + 30 - 6, scene.at.y + 20, scene.creatureSPK[2]);
	CHECK(scene.Same());
}

TEST(CreatureEffect, AnInstallingTurretStandsFacing2OnItsCount)
{
	Scene scene;
	Wearer slayer;
	slayer.Become(kSlayer);
	slayer.SetID(9);
	slayer.SetCurrentDirection(5);
	slayer.SetInstallTurretCount(4);
	slayer.m_AttachEffectColor = 66;
	// One function pointer per frame index (a conditional between two
	// lambdas is not one MSVC is sure to take).
	const Scene::Effect firstIndex = [](const CreatureDrawPacks& packs, POINT* p, MCreature* c, int a, int d, int f) {
		DrawInstallTurret(packs, p, c, a, d, f, 0); };
	const Scene::Effect secondIndex = [](const CreatureDrawPacks& packs, POINT* p, MCreature* c, int a, int d, int f) {
		DrawInstallTurret(packs, p, c, a, d, f, 1); };
	auto install = [&](int frameIndex) { return frameIndex == 0 ? firstIndex : secondIndex; };
	Place(scene.creatureFPK, 222, ACTION_STAND, 2, 4, 1, 6, 7);
	Place(scene.creatureFPK, 222, ACTION_ATTACK, 5, 3, 2, 16, 17);
	// Standing: facing 2 on its count; the first frame index sets the
	// screen rect.
	scene.Draw(install(0), slayer, ACTION_STAND, 5, 0);
	scene.expected.Blit(scene.at.x + 6, scene.at.y + 7, scene.creatureSPK[1]);
	CHECK(scene.Same());
	CHECK(SameRect(slayer.GetScreenRect(), 46, 37, 51, 38));
	scene.Clear();
	scene.Draw(install(1), slayer, ACTION_STAND, 5, 0);
	scene.expected.Blit(scene.at.x + 6, scene.at.y + 7, scene.creatureSPK[1]);
	CHECK(scene.Same());
	CHECK(SameRect(slayer.GetScreenRect(), 800, 600, 0, 0));
	// Otherwise its own action, facing and frame; none past its frames.
	scene.Clear();
	scene.Draw(install(0), slayer, ACTION_ATTACK, 5, 3);
	scene.expected.Blit(scene.at.x + 16, scene.at.y + 17, scene.creatureSPK[2]);
	CHECK(scene.Same());
	scene.Clear();
	scene.Draw(install(0), slayer, ACTION_ATTACK, 5, 4);
	CHECK(scene.drawn.Blank());
	// Selected, it goes to the outlines in its attached effect's colour.
	scene.packs.selectedCreatureID = 9;
	scene.Draw(install(0), slayer, ACTION_STAND, 5, 0);
	CHECK(scene.drawn.Blank());
	CHECK_EQ(size_t(1), scene.outlines.GetOutlines().size());
	if (scene.outlines.GetOutlines().size() == 1)
	{
		const OutlineEntry& entry = scene.outlines.GetOutlines()[0];
		CHECK_EQ(46, entry.x);
		CHECK_EQ(37, entry.y);
		CHECK(entry.pIndexSprite == &scene.creatureSPK[1]);
		CHECK_EQ(66, int(entry.color));
	}
}

//----------------------------------------------------------------------
// The order, colours and edges of every effect, plain and advanced
//----------------------------------------------------------------------
TEST(CreatureEffect, ThePartsAreLaidInTheOrderOfTheirFacing)
{
	// Facing 2 the left hand comes before the right, facing 0 after it, in
	// both orders; a shield and a sword on one spot show which lies over
	// the other, and the last of the order is drawn.
	for (const Kind& kind : kKinds)
		for (bool advanced : {false, true})
			for (int direction : {0, 2})
			{
				Scene scene;
				Wearer slayer;
				slayer.Become(kSlayer);
				if (advanced) slayer.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
				slayer.SetCurrentDirection(BYTE(direction));
				slayer.Wear(ADDON_LEFTHAND, 1, ITEM_CLASS_SHIELD, 30, 60);
				slayer.Wear(ADDON_RIGHTHAND, 2, ITEM_CLASS_SWORD, 40, 50);
				CIndexSpritePack& sprites = advanced ? scene.slayerManSPK : scene.addonSPK;
				if (advanced)
				{
					const int action = ConvAdvancementSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer) - ADVANCEMENT_ACTION_START;
					Place(scene.slayerManFPK, AC_SHIELD, action, direction, 0, 1, 2, 3);
					Place(scene.slayerManFPK, AC_SWORD, action, direction, 0, 2, 2, 3);
				}
				else
				{
					Place(scene.addonFPK, 1, ACTION_MAGIC, direction, 0, 1, 2, 3);
					Place(scene.addonFPK, 2, ACTION_MAGIC, direction, 0, 2, 2, 3);
				}
				scene.Draw(advanced ? kind.advanced : kind.plain, slayer, ACTION_MAGIC, direction, 0);
				const bool shieldFirst = direction == 2;
				for (const Lay& lay : kind.lays(0, 0))
					for (int part : {0, 1})
					{
						const bool shield = (part == 0) == shieldFirst;
						scene.Blit(2 + lay.dx, 3 + lay.dy, sprites[shield ? 1 : 2], shield ? 30 : 40, shield ? 60 : 50, lay.darkBits);
					}
				const bool same = scene.Same();
				if (!same) std::printf("      %s%s facing %d\n", advanced ? "advanced " : "", kind.name, direction);
				CHECK(same);
			}
}

TEST(CreatureEffect, AnAdvancedSlayersSpecialColoursAndScreenRect)
{
	// A unique coat keeps its first set, a quest helm takes it in both;
	// for its items' parts and for the new items alike.
	for (const Kind& kind : kKinds)
		for (bool newItems : {false, true})
		{
			Scene scene;
			Wearer slayer;
			slayer.Become(kSlayer);
			slayer.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
			slayer.SetCurrentDirection(2);
			slayer.Wear(ADDON_COAT, 5, ITEM_CLASS_COAT, 30, UNIQUE_ITEM_COLOR, newItems ? 28 : 0);
			slayer.Wear(ADDON_HELM, 8, ITEM_CLASS_HELM, 40, QUEST_ITEM_COLOR, newItems ? 15 : 0);
			const int action = newItems ? ConvNewSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer)
				: ConvAdvancementSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer) - ADVANCEMENT_ACTION_START;
			Place(scene.slayerManFPK, newItems ? 5 : int(AC_BODY), action, 2, 2, 1, 2, 3);
			Place(scene.slayerManFPK, newItems ? 8 : int(AC_HELMET), action, 2, 2, 2, 14, 30);
			if (newItems) scene.slayerManFPK[6].Init(0);
			scene.Draw(kind.advanced, slayer, ACTION_MAGIC, 2, 2);
			for (const Lay& lay : kind.lays(0, 0))
			{
				scene.Blit(2 + lay.dx, 3 + lay.dy, scene.slayerManSPK[1], 30, Special(UNIQUE_ITEM_COLOR), lay.darkBits);
				scene.Blit(14 + lay.dx, 30 + lay.dy, scene.slayerManSPK[2], Special(QUEST_ITEM_COLOR), Special(QUEST_ITEM_COLOR), lay.darkBits);
			}
			const bool same = scene.Same();
			if (!same) std::printf("      %s%s\n", kind.name, newItems ? " (new items)" : "");
			CHECK(same);
			CHECK(SameRect(slayer.GetScreenRect(), 42, 33, 59, 61));
		}
}

TEST(CreatureEffect, AnAdvancedSlayerPastItsFramesIsNotDrawn)
{
	for (const Kind& kind : kKinds)
		for (bool newItems : {false, true})
		{
			Scene scene;
			Wearer slayer;
			slayer.Become(kSlayer);
			slayer.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
			slayer.Wear(ADDON_HELM, 8, ITEM_CLASS_HELM, 40, 50, newItems ? 15 : 0);
			const int action = newItems ? ConvNewSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer)
				: ConvAdvancementSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer) - ADVANCEMENT_ACTION_START;
			Place(scene.slayerManFPK, newItems ? 8 : int(AC_HELMET), action, 2, 2, 2, 14, 30);
			scene.Draw(kind.advanced, slayer, ACTION_MAGIC, 2, 3);
			CHECK(scene.drawn.Blank());
			CHECK(SameRect(slayer.GetScreenRect(), 800, 600, 0, 0));
		}
}

TEST(CreatureEffect, AnAdvancedSlayerWithoutAnAdvancedActionDrawsNoNewItemEither)
{
	for (const Kind& kind : kKinds)
	{
		Scene scene;
		Wearer slayer;
		slayer.Become(kSlayer);
		slayer.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
		slayer.Wear(ADDON_HELM, 8, ITEM_CLASS_HELM, 40, 50, 15);
		CHECK_EQ(-1, ConvAdvancementSlayerActionFromSlayerAction(ACTION_ATTACK, &slayer));
		for (int direction = 0; direction < 8; ++direction)
			Place(scene.slayerManFPK, 8, ConvNewSlayerActionFromSlayerAction(ACTION_ATTACK, &slayer), direction, 2, 2, 14, 30);
		scene.Draw(kind.advanced, slayer, ACTION_ATTACK, 2, 2);
		CHECK(scene.drawn.Blank());
	}
}

TEST(CreatureEffect, EachNewItemClassFromItsOwnType)
{
	// At its threshold an item is new (its frame ID, the new action), one
	// type below it is not (its part, the advanced action).
	struct Case { int slot; ITEM_CLASS itemClass; int threshold; };
	for (const Case& c : {Case{ADDON_COAT, ITEM_CLASS_COAT, 28}, Case{ADDON_RIGHTHAND, ITEM_CLASS_SWORD, 16},
		Case{ADDON_RIGHTHAND, ITEM_CLASS_BLADE, 16}, Case{ADDON_LEFTHAND, ITEM_CLASS_SHIELD, 15},
		Case{ADDON_RIGHTHAND, ITEM_CLASS_CROSS, 14}, Case{ADDON_HELM, ITEM_CLASS_HELM, 15},
		Case{ADDON_RIGHTHAND, ITEM_CLASS_AR, 16}, Case{ADDON_RIGHTHAND, ITEM_CLASS_SR, 16},
		Case{ADDON_RIGHTHAND, ITEM_CLASS_MACE, 14}})
		for (const Kind& kind : kKinds)
			for (int type : {c.threshold, c.threshold - 1})
			{
				Scene scene;
				Wearer slayer;
				slayer.Become(kSlayer);
				slayer.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
				slayer.Wear(c.slot, 9, c.itemClass, 40, 50, type);
				const bool isNew = type == c.threshold;
				const int part = GetAdvancementPartFromItemClass(c.itemClass, 9);
				const int action = isNew ? ConvNewSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer)
					: ConvAdvancementSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer) - ADVANCEMENT_ACTION_START;
				Place(scene.slayerManFPK, isNew ? 9 : part, action, 2, 0, 1, 2, 3);
				if (isNew && c.slot == ADDON_COAT) Place(scene.slayerManFPK, 10, action, 2, 0, 2, 20, 3);
				scene.Draw(kind.advanced, slayer, ACTION_MAGIC, 2, 0);
				for (const Lay& lay : kind.lays(0, 0))
				{
					scene.Blit(2 + lay.dx, 3 + lay.dy, scene.slayerManSPK[1], 40, 50, lay.darkBits);
					if (isNew && c.slot == ADDON_COAT) scene.Blit(20 + lay.dx, 3 + lay.dy, scene.slayerManSPK[2], 40, 50, lay.darkBits);
				}
				const bool same = scene.Same();
				if (!same) std::printf("      %s, item class %d type %d\n", kind.name, int(c.itemClass), type);
				CHECK(same);
			}
}

TEST(CreatureEffect, AnAdvancedInvisibleSlayerWipesInBetweenItsEnds)
{
	for (bool newItems : {false, true})
		for (int count : {0, 64, 32})
		{
			Scene scene;
			EffectGuard effects;
			Wearer slayer;
			slayer.Become(kSlayer);
			slayer.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
			slayer.m_InvisibleCount = count;
			slayer.Wear(ADDON_HELM, 8, ITEM_CLASS_HELM, 40, 50, newItems ? 15 : 0);
			const int action = newItems ? ConvNewSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer)
				: ConvAdvancementSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer) - ADVANCEMENT_ACTION_START;
			Place(scene.slayerManFPK, newItems ? 8 : int(AC_HELMET), action, 2, 2, 1, 2, 3);
			CSpriteSurface::s_Value1 = 99;
			CIndexSprite::SetEffect(CIndexSprite::EFFECT_NET);
			scene.Draw(DrawInvisibleForACSlayer, slayer, ACTION_MAGIC, 2, 2);
			// As the plain slayer's: snipping-dark gone, wiped over it going.
			CHECK_EQ(count == 32 ? 32 : count == 64 ? 2 : 99, CSpriteSurface::s_Value1);
			CHECK_EQ(count == 32, CIndexSprite::IsWipeOutEffect());
			scene.Blit(2, 3, scene.slayerManSPK[1], 40, 50, count == 0 ? 0 : 2);
			if (count == 32) scene.Wipe(2, 3, scene.slayerManSPK[1], 32);
			CHECK(scene.Same());
		}
}

TEST(CreatureEffect, AnAdvancedPetMovingFastIsDrawnToo)
{
	for (bool newItems : {false, true})
		for (MFakeCreature::FAKE_CREATURE_TYPE type : {MFakeCreature::FAKE_CREATURE_BRIGHTNESS, MFakeCreature::FAKE_CREATURE_STAND})
		{
			Scene scene;
			Pet pet;
			pet.Become(kSlayer);
			pet.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
			pet.SetFakeCreatureType(type);
			pet.m_cX = 6;
			pet.m_cY = 2;
			pet.Wear(ADDON_HELM, 8, ITEM_CLASS_HELM, 40, 50, newItems ? 15 : 0);
			const int action = newItems ? ConvNewSlayerActionFromSlayerAction(ACTION_MAGIC, &pet)
				: ConvAdvancementSlayerActionFromSlayerAction(ACTION_MAGIC, &pet) - ADVANCEMENT_ACTION_START;
			Place(scene.slayerManFPK, newItems ? 8 : int(AC_HELMET), action, 2, 2, 1, 2, 3);
			scene.Draw(DrawFastMoveForACSlayer, pet, ACTION_MAGIC, 2, 2);
			const int darkBits = type == MFakeCreature::FAKE_CREATURE_BRIGHTNESS ? 0 : 1;
			for (const Lay& lay : FastMoveLays(6, 2))
				scene.Blit(2 + lay.dx, 3 + lay.dy, scene.slayerManSPK[1], 40, 50, lay.dx == 0 && lay.dy == 0 ? darkBits : lay.darkBits);
			CHECK(scene.Same());
		}
}

TEST(CreatureEffect, ASwungSpecialWeaponsAfterimagesTakeItsColours)
{
	// The afterimages of a unique sword in both sets; a quest coat keeps
	// its first.
	Swing swing(2);
	swing.slayer.Wear(ADDON_RIGHTHAND, 2, ITEM_CLASS_SWORD, 40, UNIQUE_ITEM_COLOR);
	swing.slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, QUEST_ITEM_COLOR);
	swing.scene.Draw(DrawWeaponFadeOut, swing.slayer, ACTION_ATTACK, 2, 2);
	const int unique = Special(UNIQUE_ITEM_COLOR);
	for (int f = 1; f <= 2; ++f) swing.scene.Blit(10 + 4 * (2 - f), 20, swing.scene.addonSPK[2], unique, unique, kAfterimageDarkBits[f]);
	swing.scene.Blit(18, 20, swing.scene.addonSPK[1], unique, unique);
	swing.scene.Blit(2, 3, swing.scene.addonSPK[0], 30, Special(QUEST_ITEM_COLOR));
	CHECK(swing.scene.Same());
}
