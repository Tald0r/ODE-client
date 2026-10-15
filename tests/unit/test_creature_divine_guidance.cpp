#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "CreatureDivineGuidanceDraw.h"
#include "MCreatureWear.h"
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
#include "FrameClock.h"
#include "AddonDef.h"
#include "Types/ModifyDef.h"
#include "CSpriteSurface.h"
#include "CIndexSprite.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <vector>

extern int defaultCoatColor;

// A creature under divine guidance is drawn again as a darkened, turning
// silhouette (task 4.248: MTopView's members until then, gameview's
// since). Each test draws through the moved code onto one surface and
// lays the blits it should make, with the colour sets named, onto
// another; the two pictures must match.
namespace {

const char* kSpritePath = "divine_guidance_sprite.bin";

// A sprite of five pixels: colour-set channels 0 to 3 at gradation 2 (a
// light one: the last gradations are black), then a fixed colour, its
// mark.
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

// The frames of (id, action, direction): `frames` of them, the 31st (the
// one drawn) the sprite at its offset.
void Place(CCreatureFramePack& pack, int id, int action, int direction, int sprite, int cx, int cy, int frames = 31)
{
	FRAME_ARRAY& frameArray = pack[TYPE_FRAMEID(id)][BYTE(action)][BYTE(direction)];
	frameArray.Init(WORD(frames));
	if (frames > 30) frameArray[30].Set(TYPE_SPRITEID(sprite), short(cx), short(cy));
}

struct Canvas
{
	CSpriteSurface surface;
	Canvas() { CHECK_EQ(0, spritectl_init()); CHECK(surface.Init(64, 64)); surface.FillSurface(0); }
	std::vector<WORD> Picture()
	{
		std::vector<WORD> pixels;
		DWORD pitch = 0;
		const unsigned char* base = static_cast<const unsigned char*>(surface.Lock(nullptr, &pitch));
		CHECK(base != nullptr);
		if (base == nullptr) return pixels;
		for (int y = 0; y < 64; ++y)
			for (int x = 0; x < 64; ++x)
				pixels.push_back(reinterpret_cast<const WORD*>(base + y * pitch)[x]);
		surface.Unlock();
		return pixels;
	}
	bool Blank() { for (WORD pixel : Picture()) if (pixel != 0) return false; return true; }
	// A blit as the silhouettes make it.
	void Blit(int x, int y, CIndexSprite& sprite, int darkness)
	{
		POINT point = { x, y };
		surface.Lock();
		surface.BltIndexSpriteDarkness(&point, &sprite, BYTE(darkness));
		surface.Unlock();
	}
};

// A wearer whose parts a test sets: of the type and bare (the type puts
// its default hair on).
struct Wearer : MCreatureWear
{
	void Become(int type)
	{
		SetCreatureType(TYPE_CREATURETYPE(type));
		for (ADDON_INFO& addon : m_Addon) addon.bAddon = FALSE;
		m_ShoulderAddon.bAddon = FALSE;
	}
	void Wear(int slot, int frameID, ITEM_CLASS itemClass, int colorSet1, int colorSet2)
	{
		ADDON_INFO& addon = m_Addon[slot];
		addon.bAddon = TRUE;
		addon.FrameID = TYPE_FRAMEID(frameID);
		addon.ItemClass = itemClass;
		addon.ColorSet1 = WORD(colorSet1);
		addon.ColorSet2 = WORD(colorSet2);
	}
	void TakeOff(int slot) { m_Addon[slot].bAddon = FALSE; }
};

const int kSlayer = 1, kVampire = 2, kOusters = 3;
const int kHeight = 4;

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
	CCreatureFramePack creatureFPK, addonFPK, oustersFPK;
	CIndexSpritePack creatureSPK, addonSPK, oustersSPK;
	CCreatureFramePack slayerManFPK, slayerWomanFPK, vampireManFPK, vampireWomanFPK, advancedOustersFPK;
	CIndexSpritePack slayerManSPK, slayerWomanSPK, vampireManSPK, vampireWomanSPK, advancedOustersSPK;
	Canvas drawn, expected;
	CreatureDrawPacks packs;
	POINT at = { 10, 20 };

	Scene()
	{
		CHECK_EQ(0, spritectl_init());
		CIndexSprite::SetColorSet();
		for (int set = 0; set < 4; ++set) savedSets[set] = CIndexSprite::GetUsingColorSet(BYTE(set));
		g_CurrentFrame = 0;
		creatureTypes.Init(4);
		for (int type : {kSlayer, kVampire, kOusters})
		{
			testfw::MutableRow(creatureTypes, type).SetCreatureTribe(type == kSlayer ? CREATURETRIBE_SLAYER
				: type == kVampire ? CREATURETRIBE_VAMPIRE : CREATURETRIBE_OUSTERS, ACTION_MAX_SLAYER);
			testfw::MutableRow(creatureTypes, type).Height = kHeight;
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
		for (CCreatureFramePack* pPack : {&creatureFPK, &addonFPK, &oustersFPK})
			Shape(*pPack, 4, ACTION_MAX_SLAYER);
		for (CCreatureFramePack* pPack : {&slayerManFPK, &slayerWomanFPK, &vampireManFPK, &vampireWomanFPK, &advancedOustersFPK})
			Shape(*pPack, AC_BIKE_2 + 1, advancedActions);
		// Marks with each channel's top bit set stay lit when darkened.
		int n = 0;
		for (CIndexSpritePack* pPack : {&creatureSPK, &addonSPK, &oustersSPK, &slayerManSPK, &slayerWomanSPK,
			&vampireManSPK, &vampireWomanSPK, &advancedOustersSPK})
		{
			pPack->Init(3);
			for (int sprite = 0; sprite < 3; ++sprite, ++n) LoadMark((*pPack)[WORD(sprite)], WORD(0x8410 | ((n * 0x0821) & 0x7BEF)));
		}
		packs.pSurface = &drawn.surface;
		packs.pCreatureFPK = &creatureFPK;
		packs.pCreatureSPK = &creatureSPK;
		packs.pAddonFPK = &addonFPK;
		packs.pAddonSPK = &addonSPK;
		packs.pOustersFPK = &oustersFPK;
		packs.pOustersSPK = &oustersSPK;
		packs.pAdvancementSlayerManFPK = &slayerManFPK;
		packs.pAdvancementSlayerManSPK = &slayerManSPK;
		packs.pAdvancementSlayerWomanFPK = &slayerWomanFPK;
		packs.pAdvancementSlayerWomanSPK = &slayerWomanSPK;
		packs.pAdvancementVampireManFPK = &vampireManFPK;
		packs.pAdvancementVampireManSPK = &vampireManSPK;
		packs.pAdvancementVampireWomanFPK = &vampireWomanFPK;
		packs.pAdvancementVampireWomanSPK = &vampireWomanSPK;
		packs.pAdvancementOustersFPK = &advancedOustersFPK;
		packs.pAdvancementOustersSPK = &advancedOustersSPK;
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
				std::printf("      the picture differs first at (%d,%d): %04x, not %04x\n", int(i % 64), int(i / 64), a[i], b[i]);
				break;
			}
		return any && a == b;
	}
	void Draw(void (*draw)(const CreatureDrawPacks&, POINT*, MCreature*, int), MCreature& creature, int direction)
	{
		POINT point = at;
		drawn.surface.Lock();
		draw(packs, &point, &creature, direction);
		drawn.surface.Unlock();
	}
	void DrawBody(void (*draw)(const CreatureDrawPacks&, POINT*, MCreature*, int, int), MCreature& creature, int direction, int body)
	{
		POINT point = at;
		drawn.surface.Lock();
		draw(packs, &point, &creature, direction, body);
		drawn.surface.Unlock();
	}
};

int Special(int colorSet) { return MItem::GetSpecialColorItemColorset(colorSet); }

} // namespace

//----------------------------------------------------------------------
// The slayer
//----------------------------------------------------------------------
TEST(DivineGuidance, TheColourSetsUsedHereCanBeToldApart)
{
	// Every colour set these tests name, lit and different at gradation 2,
	// so a picture shows which set a channel took.
	Scene scene;
	std::vector<int> sets{11, 30, 33, 40, 50, 60, 66, 70, 80, defaultCoatColor,
		Special(UNIQUE_ITEM_COLOR), Special(QUEST_ITEM_COLOR)};
	for (size_t i = 0; i < sets.size(); ++i)
	{
		CHECK(CIndexSprite::ColorSet[sets[i]][2] != 0);
		for (size_t j = 0; j < i; ++j)
			CHECK(CIndexSprite::ColorSet[sets[i]][2] != CIndexSprite::ColorSet[sets[j]][2]);
	}
}

TEST(DivineGuidance, ASlayersWornPartsTurnDarkenedFromFrame30OfBeingDrained)
{
	Scene scene;
	Wearer slayer;
	slayer.Become(kSlayer);
	slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
	// Direction 2 and the next one, as the clock stands at frame 0.
	Place(scene.addonFPK, 1, ACTION_DRAINED, 2, 1, 2, 3);
	Place(scene.addonFPK, 1, ACTION_DRAINED, 3, 2, 20, 10);
	scene.Draw(DrawDivineGuidanceSlayerCharacter, slayer, 2);
	CIndexSprite::SetUsingColorSet(30, 60);
	scene.expected.Blit(12, 23 - kHeight, scene.addonSPK[1], 2);
	scene.expected.Blit(30, 30 - kHeight, scene.addonSPK[2], 1);
	CHECK(scene.Same());
}

TEST(DivineGuidance, ItTurnsWithTheFrameClockAndNeedsAFrame30)
{
	Scene scene;
	Wearer slayer;
	slayer.Become(kSlayer);
	slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
	Place(scene.addonFPK, 1, ACTION_DRAINED, 3, 2, 20, 10);
	Place(scene.addonFPK, 1, ACTION_DRAINED, 4, 1, 2, 3, 30);
	// Frame 2 of the clock turns it on by one: direction 3 dark, then 4,
	// whose 30 frames hold no frame 30.
	g_CurrentFrame = 2;
	scene.Draw(DrawDivineGuidanceSlayerCharacter, slayer, 2);
	CIndexSprite::SetUsingColorSet(30, 60);
	scene.expected.Blit(30, 30 - kHeight, scene.addonSPK[2], 2);
	CHECK(scene.Same());
}

TEST(DivineGuidance, WhatItDoesNotWearIsNotDrawn)
{
	Scene scene;
	Wearer slayer;
	slayer.Become(kSlayer);
	slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
	slayer.TakeOff(ADDON_COAT);
	Place(scene.addonFPK, 1, ACTION_DRAINED, 2, 1, 2, 3);
	scene.Draw(DrawDivineGuidanceSlayerCharacter, slayer, 2);
	CHECK(scene.drawn.Blank());
}

TEST(DivineGuidance, AUniqueItemsColourTakesBothSetsButACoatsKeepsItsFirst)
{
	Scene scene;
	Wearer slayer;
	slayer.Become(kSlayer);
	slayer.Wear(ADDON_HELM, 2, ITEM_CLASS_HELM, 30, UNIQUE_ITEM_COLOR);
	Place(scene.addonFPK, 2, ACTION_DRAINED, 2, 1, 2, 3);
	scene.Draw(DrawDivineGuidanceSlayerCharacter, slayer, 2);
	CIndexSprite::SetUsingColorSet(Special(UNIQUE_ITEM_COLOR), Special(UNIQUE_ITEM_COLOR));
	scene.expected.Blit(12, 23 - kHeight, scene.addonSPK[1], 2);
	CHECK(scene.Same());
}

TEST(DivineGuidance, ACoatOfAQuestItemsColourKeepsItsFirstSet)
{
	Scene scene;
	Wearer slayer;
	slayer.Become(kSlayer);
	slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, QUEST_ITEM_COLOR);
	Place(scene.addonFPK, 1, ACTION_DRAINED, 2, 1, 2, 3);
	scene.Draw(DrawDivineGuidanceSlayerCharacter, slayer, 2);
	CIndexSprite::SetUsingColorSet(30, Special(QUEST_ITEM_COLOR));
	scene.expected.Blit(12, 23 - kHeight, scene.addonSPK[1], 2);
	CHECK(scene.Same());
}

//----------------------------------------------------------------------
// The vampire
//----------------------------------------------------------------------
TEST(DivineGuidance, AVampiresBodyBobsInItsColours)
{
	Scene scene;
	MCreature vampire;
	vampire.SetCreatureType(kVampire);
	vampire.SetBodyColor1(33);
	vampire.SetBodyColor2(66);
	Place(scene.creatureFPK, 2, ACTION_DRAINED, 5, 1, 4, 6);
	Place(scene.creatureFPK, 2, ACTION_DRAINED, 6, 2, 25, 12);
	// Frame 4 of the clock: two directions on, and |10 - 4| lower.
	g_CurrentFrame = 4;
	scene.DrawBody(DrawDivineGuidanceVampireCharacter, vampire, 3, 2);
	CIndexSprite::SetUsingColorSet(33, 66);
	scene.expected.Blit(14, 26 - kHeight + 6, scene.creatureSPK[1], 2);
	scene.expected.Blit(35, 32 - kHeight + 6, scene.creatureSPK[2], 1);
	CHECK(scene.Same());
}

TEST(DivineGuidance, AVampiresBobFollowsTheClockOverTwentyFrames)
{
	Scene scene;
	MCreature vampire;
	vampire.SetCreatureType(kVampire);
	vampire.SetBodyColor1(33);
	vampire.SetBodyColor2(66);
	// Frame 14: seven directions on (14 / 2), and |10 - 14| lower.
	Place(scene.creatureFPK, 0, ACTION_DRAINED, 7, 1, 4, 6);
	g_CurrentFrame = 14;
	scene.DrawBody(DrawDivineGuidanceVampireCharacter, vampire, 0, 0);
	CIndexSprite::SetUsingColorSet(33, 66);
	scene.expected.Blit(14, 26 - kHeight + 4, scene.creatureSPK[1], 2);
	CHECK(scene.Same());
}

TEST(DivineGuidance, AVampiresSpecialColoursAreLookedUp)
{
	Scene scene;
	MCreature vampire;
	vampire.SetCreatureType(kVampire);
	vampire.SetBodyColor1(UNIQUE_ITEM_COLOR);
	vampire.SetBodyColor2(QUEST_ITEM_COLOR);
	Place(scene.creatureFPK, 0, ACTION_DRAINED, 0, 1, 4, 6);
	scene.DrawBody(DrawDivineGuidanceVampireCharacter, vampire, 0, 0);
	CIndexSprite::SetUsingColorSet(Special(UNIQUE_ITEM_COLOR), Special(QUEST_ITEM_COLOR));
	scene.expected.Blit(14, 26 - kHeight + 10, scene.creatureSPK[1], 2);
	CHECK(scene.Same());
}

//----------------------------------------------------------------------
// The ousters
//----------------------------------------------------------------------
TEST(DivineGuidance, AnOustersCoatTakesItsHairCoatArmsAndBootsColours)
{
	Scene scene;
	Wearer ousters;
	ousters.Become(kOusters);
	ousters.SetBodyColor1(11);
	ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT, 30, 60);
	ousters.Wear(ADDON_TROUSER, 2, ITEM_CLASS_OUSTERS_BOOTS, 40, 70);
	ousters.Wear(ADDON_RIGHTHAND, 3, ITEM_CLASS_OUSTERS_WRISTLET, 50, 80);
	Place(scene.oustersFPK, 1, ACTION_DRAINED, 0, 1, 2, 3);
	// A wristlet is not drawn; it colours the arms.
	Place(scene.oustersFPK, 3, ACTION_DRAINED, 0, 2, 30, 30);
	scene.Draw(DrawDivineGuidanceOustersCharacter, ousters, 0);
	CIndexSprite::SetUsingColorSetOnly(0, 11);
	CIndexSprite::SetUsingColorSetOnly(1, 60);
	CIndexSprite::SetUsingColorSetOnly(2, 80);
	CIndexSprite::SetUsingColorSetOnly(3, 70);
	scene.expected.Blit(12, 23 - kHeight, scene.oustersSPK[1], 2);
	CHECK(scene.Same());
}

TEST(DivineGuidance, BareArmsAndFeetTakeTheDefaultCoatColour)
{
	Scene scene;
	Wearer ousters;
	ousters.Become(kOusters);
	ousters.SetBodyColor1(11);
	ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT, 30, 60);
	Place(scene.oustersFPK, 1, ACTION_DRAINED, 0, 1, 2, 3);
	scene.Draw(DrawDivineGuidanceOustersCharacter, ousters, 0);
	CIndexSprite::SetUsingColorSetOnly(0, 11);
	CIndexSprite::SetUsingColorSetOnly(1, 60);
	CIndexSprite::SetUsingColorSetOnly(2, defaultCoatColor);
	CIndexSprite::SetUsingColorSetOnly(3, defaultCoatColor);
	scene.expected.Blit(12, 23 - kHeight, scene.oustersSPK[1], 2);
	CHECK(scene.Same());
}

TEST(DivineGuidance, AChakramIsDrawnInItsColoursAndAGhostsPartsAreNot)
{
	Scene scene;
	Wearer ousters;
	ousters.Become(kOusters);
	ousters.SetBodyColor1(11);
	ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT, 30, 60);
	ousters.Wear(ADDON_RIGHTHAND, 3, ITEM_CLASS_OUSTERS_CHAKRAM, 50, 80);
	Place(scene.oustersFPK, 1, ACTION_DRAINED, 0, 1, 2, 3);
	Place(scene.oustersFPK, 3, ACTION_DRAINED, 0, 2, 30, 30);
	scene.Draw(DrawDivineGuidanceOustersCharacter, ousters, 0);
	CIndexSprite::SetUsingColorSetOnly(0, 11);
	CIndexSprite::SetUsingColorSetOnly(1, 60);
	CIndexSprite::SetUsingColorSetOnly(2, 80);
	CIndexSprite::SetUsingColorSetOnly(3, defaultCoatColor);
	scene.expected.Blit(12, 23 - kHeight, scene.oustersSPK[1], 2);
	CIndexSprite::SetUsingColorSet(50, 80);
	scene.expected.Blit(40, 50 - kHeight, scene.oustersSPK[2], 2);
	CHECK(scene.Same());
	// A ghost's coat (1) and chakram (2) are not drawn.
	ousters.SetGhost(1 | 2, 10);
	scene.drawn.surface.FillSurface(0);
	scene.Draw(DrawDivineGuidanceOustersCharacter, ousters, 0);
	CHECK(scene.drawn.Blank());
}

//----------------------------------------------------------------------
// The advanced (second) classes
//----------------------------------------------------------------------
TEST(DivineGuidance, AnAdvancedSlayerDrawsItsPartsFromItsGendersPack)
{
	const int drained = ACTION_ADVANCEMENT_SLAYER_DRAINED - ADVANCEMENT_ACTION_START;
	for (bool male : {true, false})
	{
		Scene scene;
		Wearer slayer;
		slayer.Become(kSlayer);
		slayer.SetMale(male);
		// Facing direction 2, the left hand comes before the right, so the
		// sword lies over the shield.
		slayer.SetCurrentDirection(2);
		slayer.Wear(ADDON_RIGHTHAND, 9, ITEM_CLASS_SWORD, 30, 60);
		slayer.Wear(ADDON_LEFTHAND, 8, ITEM_CLASS_SHIELD, 40, 50);
		CCreatureFramePack& pack = male ? scene.slayerManFPK : scene.slayerWomanFPK;
		CIndexSpritePack& sprites = male ? scene.slayerManSPK : scene.slayerWomanSPK;
		// The parts of a sword and a shield, not their frame IDs.
		Place(pack, AC_SWORD, drained, 2, 1, 2, 3);
		Place(pack, AC_SHIELD, drained, 2, 2, 2, 3);
		scene.Draw(DrawDivineGuidanceAdvancementClassSlayerCharacter, slayer, 2);
		CIndexSprite::SetUsingColorSet(40, 50);
		scene.expected.Blit(12, 23 - kHeight, sprites[2], 2);
		CIndexSprite::SetUsingColorSet(30, 60);
		scene.expected.Blit(12, 23 - kHeight, sprites[1], 2);
		CHECK(scene.Same());
	}
}

TEST(DivineGuidance, AnAdvancedSlayerFacingTheLastDirectionDrawsItsPartsInThatOrder)
{
	// The parts' order is the row of the direction the slayer faces, of
	// AC_ADDON_MAX parts; facing direction 7, the last row, the body comes
	// second and the left hand last, so a shield lies over the coat.
	const int drained = ACTION_ADVANCEMENT_SLAYER_DRAINED - ADVANCEMENT_ACTION_START;
	Scene scene;
	Wearer slayer;
	slayer.Become(kSlayer);
	slayer.SetMale(true);
	slayer.SetCurrentDirection(7);
	slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
	slayer.Wear(ADDON_LEFTHAND, 1, ITEM_CLASS_SHIELD, 40, 50);
	// Frame 6 of the clock turns it on by three: 7 to 2.
	Place(scene.slayerManFPK, AC_BODY, drained, 2, 1, 2, 3);
	Place(scene.slayerManFPK, AC_SHIELD, drained, 2, 2, 2, 3);
	g_CurrentFrame = 6;
	scene.Draw(DrawDivineGuidanceAdvancementClassSlayerCharacter, slayer, 7);
	CIndexSprite::SetUsingColorSet(30, 60);
	scene.expected.Blit(12, 23 - kHeight, scene.slayerManSPK[1], 2);
	CIndexSprite::SetUsingColorSet(40, 50);
	scene.expected.Blit(12, 23 - kHeight, scene.slayerManSPK[2], 2);
	CHECK(scene.Same());
}

TEST(DivineGuidance, AnAdvancedVampireDrawsItsOneBodyFromItsGendersPack)
{
	const int drained = ACTION_ADVANCEMENT_DRAINED - ADVANCEMENT_ACTION_START;
	for (bool male : {true, false})
	{
		Scene scene;
		MCreature vampire;
		vampire.SetCreatureType(kVampire);
		vampire.SetMale(male);
		vampire.SetBodyColor1(33);
		vampire.SetBodyColor2(66);
		CCreatureFramePack& pack = male ? scene.vampireManFPK : scene.vampireWomanFPK;
		CIndexSpritePack& sprites = male ? scene.vampireManSPK : scene.vampireWomanSPK;
		// Frame set 0, whatever body it is given.
		Place(pack, 0, drained, 1, 1, 4, 6);
		scene.DrawBody(DrawDivineGuidanceAdvancementClassVampireCharacter, vampire, 1, 3);
		CIndexSprite::SetUsingColorSet(33, 66);
		scene.expected.Blit(14, 26 - kHeight + 10, sprites[1], 2);
		CHECK(scene.Same());
	}
}

TEST(DivineGuidance, AnAdvancedOustersDrawsCoatSet1AndChakramSet0)
{
	const int drained = ACTION_ADVANCEMENT_OUSTERS_DRAINED - ADVANCEMENT_ACTION_START;
	Scene scene;
	Wearer ousters;
	ousters.Become(kOusters);
	ousters.SetBodyColor1(11);
	ousters.Wear(ADDON_COAT, 2, ITEM_CLASS_OUSTERS_COAT, 30, 60);
	ousters.Wear(ADDON_TROUSER, 2, ITEM_CLASS_OUSTERS_BOOTS, 40, 70);
	ousters.Wear(ADDON_RIGHTHAND, 3, ITEM_CLASS_OUSTERS_CHAKRAM, 50, 80);
	Place(scene.advancedOustersFPK, 1, drained, 0, 1, 2, 3);
	Place(scene.advancedOustersFPK, 0, drained, 0, 2, 30, 30);
	scene.Draw(DrawDivineGuidanceAdvancementClassOustersCharacter, ousters, 0);
	CIndexSprite::SetUsingColorSetOnly(0, 11);
	CIndexSprite::SetUsingColorSetOnly(1, 60);
	CIndexSprite::SetUsingColorSetOnly(2, 80);
	CIndexSprite::SetUsingColorSetOnly(3, 70);
	scene.expected.Blit(12, 23 - kHeight, scene.advancedOustersSPK[1], 2);
	CIndexSprite::SetUsingColorSet(50, 80);
	scene.expected.Blit(40, 50 - kHeight, scene.advancedOustersSPK[2], 2);
	CHECK(scene.Same());
}

//----------------------------------------------------------------------
// The choice
//----------------------------------------------------------------------
TEST(DivineGuidance, TheSilhouetteFollowsTheRaceAndTheClass)
{
	// Each pack's frame 30 at its own place, so the picture names the pack.
	struct Case { bool slayer, ousters, advanced; };
	for (const Case& c : {Case{true, false, false}, Case{true, false, true}, Case{false, true, false},
		Case{false, true, true}, Case{false, false, false}, Case{false, false, true}})
	{
		Scene scene;
		Wearer creature;
		creature.Become(c.slayer ? kSlayer : c.ousters ? kOusters : kVampire);
		creature.Wear(ADDON_COAT, 1, c.ousters ? ITEM_CLASS_OUSTERS_COAT : ITEM_CLASS_COAT, 30, 60);
		creature.SetMale(true);
		if (c.advanced) creature.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
		Place(scene.addonFPK, 1, ACTION_DRAINED, 0, 1, 1, 1);
		Place(scene.slayerManFPK, AC_BODY, ACTION_ADVANCEMENT_SLAYER_DRAINED - ADVANCEMENT_ACTION_START, 0, 1, 9, 1);
		Place(scene.oustersFPK, 1, ACTION_DRAINED, 0, 1, 17, 1);
		Place(scene.advancedOustersFPK, 1, ACTION_ADVANCEMENT_OUSTERS_DRAINED - ADVANCEMENT_ACTION_START, 0, 1, 25, 1);
		Place(scene.creatureFPK, 2, ACTION_DRAINED, 0, 1, 33, 1);
		Place(scene.vampireManFPK, 0, ACTION_ADVANCEMENT_DRAINED - ADVANCEMENT_ACTION_START, 0, 1, 41, 1);
		POINT point = scene.at;
		scene.drawn.surface.Lock();
		DrawDivineGuidanceCharacter(scene.packs, &point, &creature, 0, 2, c.slayer, c.ousters);
		scene.drawn.surface.Unlock();
		// The mark (the sprite's fifth pixel) of each pack's place: only the
		// expected one is lit. A vampire's silhouette bobs 10 lower at frame 0.
		const int x = c.slayer ? (c.advanced ? 19 : 11) : c.ousters ? (c.advanced ? 35 : 27) : (c.advanced ? 51 : 43);
		const std::vector<WORD> picture = scene.drawn.Picture();
		for (int place : {11, 19, 27, 35, 43, 51})
		{
			const int y = 20 + 1 - kHeight + (place >= 43 ? 10 : 0);
			CHECK_EQ(place == x, picture[y * 64 + place + 4] != 0);
		}
	}
}
