#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "MTopView.h"
#include "CreatureShadowDraw.h"
#include "CreatureEffectDraw.h"
#include "CreatureDivineGuidanceDraw.h"
#include "MCreatureWear.h"
#include "MFakeCreature.h"
#include "MPlayer.h"
#include "MZone.h"
#include "MSector.h"
#include "ZoneStateHost.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "MEventManager.h"
#include "MObjectSelector.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "UserOption.h"
#include "AdvancementAction.h"
#include "FrameClock.h"
#include "AddonDef.h"
#include "SpriteIDDef.h"
#include "EffectSpriteTypeDef.h"
#include "SkillDef.h"
#include "Types/ModifyDef.h"
#include "CSpriteSurface.h"
#include "CIndexSprite.h"
#include "CIndexSprite565.h"
#include "CShadowSprite.h"
#include "CSprite.h"
#include "CSprite565.h"
#include "MGuildMarkManager.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <vector>

// The view's creature drawing (Client/MTopViewDraw.cpp, task 4.260: the
// executable's until then). Each test draws through the view onto one
// surface and lays the blits it should make, with the colour sets named,
// onto another; the two pictures must match. SpriteLib's SDL backend draws
// a colour-set index sprite as a plain one in the colour sets last set
// (CSpriteSurface_Adapter.cpp) but darkens a darkened one, and the outline
// blits draw nothing, so a picture shows which sprite went where, in which
// colours and how dark.

bool IsEscapeDrawCreatureFunction(MCreature* pCreature);
extern int defaultCoatColor;

struct MTopViewTestAccess4
{
	static CreatureDrawPacks Packs(MTopView& view) { return view.GetCreatureDrawPacks(); }
	static CSpriteSurface* Surface(MTopView& view) { return view.m_pSurface; }
	static void Creature(MTopView& view, POINT* p, MCreature* c) { view.DrawCreature(p, c); }
	static void Shadow(MTopView& view, POINT* p, MCreature* c) { view.DrawCreatureShadow(p, c); }
	static void PartyHP(MTopView& view, POINT* p, MCreature* c) { view.DrawPartyHP(p, c); }
	static void Vampire(MTopView& view, POINT* p, MCreature* c, int a, int d, int f, int body, int index)
	{ view.DrawVampireCharacter(p, c, a, d, f, body, index); }
	static void AdvancedVampire(MTopView& view, POINT* p, MCreature* c, int a, int d, int f, int body, int index)
	{ view.DrawAdvancementClassVampireCharacter(p, c, a, d, f, body, index); }
	static void Slayer(MTopView& view, POINT* p, MCreature* c, int a, int d, int f, int index)
	{ view.DrawSlayerCharacter(p, c, a, d, f, index); }
	static void AdvancedSlayer(MTopView& view, POINT* p, MCreature* c, int a, int d, int f, int index)
	{ view.DrawAdvancementClassSlayerCharacter(p, c, a, d, f, index); }
	static void Ousters(MTopView& view, POINT* p, MCreature* c, int a, int d, int f)
	{ view.DrawOustersCharacter(p, c, a, d, f); }
	static void AdvancedOusters(MTopView& view, POINT* p, MCreature* c, int a, int d, int f)
	{ view.DrawAdvancementClassOustersCharacter(p, c, a, d, f); }
	static void SelectedVampire(MTopView& view, POINT* p, MCreature* c, int a, int d, int f, int body, int index)
	{ view.DrawSelectedVampireCreature(p, c, a, d, f, body, index); }
	static void SelectedAdvancedVampire(MTopView& view, POINT* p, MCreature* c, int a, int d, int f, int body, int index)
	{ view.DrawSelectedAdvancementVampireCreature(p, c, a, d, f, body, index); }
	static void SelectedSlayer(MTopView& view, POINT* p, MCreature* c, int a, int d, int f)
	{ view.DrawSelectedSlayerCreature(p, c, a, d, f); }
	static void SelectedAdvancedSlayer(MTopView& view, POINT* p, MCreature* c, int a, int d, int f)
	{ view.DrawSelectedAdvancementSlayerCreature(p, c, a, d, f); }
	static void SelectedOusters(MTopView& view, POINT* p, MCreature* c, int a, int d, int f)
	{ view.DrawSelectedOustersCreature(p, c, a, d, f); }
	static void SelectedAdvancedOusters(MTopView& view, POINT* p, MCreature* c, int a, int d, int f)
	{ view.DrawSelectedAdvancementOustersCreature(p, c, a, d, f); }
};

namespace {

using Access = MTopViewTestAccess4;

const char* kPath = "top_view_4_sprite.bin";
const int kSize = 128;
// The colour sets of channels 2 and 3 every scene starts with.
const int kC2 = 7, kC3 = 8;
// No attached effect colour (MCreature.cpp's ATTACHEFFECTCOLOR_NULL).
const WORD kNoEffectColor = 0xFFFF;

template<class Sprite> void Load(Sprite& sprite, const std::vector<WORD>& words)
{
	{
		std::ofstream out(kPath, std::ios::binary | std::ios::trunc);
		for (WORD word : words) { out.put(char(word & 0xFF)); out.put(char(word >> 8)); }
	}
	std::ifstream in(kPath, std::ios::binary);
	CHECK(sprite.LoadFromFile(in));
	in.close();
	std::remove(kPath);
}

// An index sprite of five pixels: colour-set channels 0 to 3 at gradation
// 2, then a fixed colour, its mark.
void LoadMark(CIndexSprite& sprite, WORD mark)
{
	Load(sprite, {5, 1, 9, 1, 0, 4, 0x0002, 0x0102, 0x0202, 0x0302, 1, mark});
}

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

// A shadow nine pixels wide whose shape is its id's.
void LoadShadow(CShadowSprite& sprite, int id)
{
	const unsigned mask = 1u | (unsigned(id) << 1);
	std::vector<WORD> line{0};
	int at = 0;
	for (int x = 0; x < 9; )
	{
		if (!((mask >> x) & 1)) { ++x; continue; }
		const int start = x;
		while (x < 9 && ((mask >> x) & 1)) ++x;
		line.push_back(WORD(start - at));
		line.push_back(WORD(x - start));
		at = x;
		++line[0];
	}
	std::vector<WORD> words{9, 1, WORD(line.size())};
	words.insert(words.end(), line.begin(), line.end());
	Load(sprite, words);
}

// Every frame ID, action and direction of the pack, without frames.
void ShapePack(CCreatureFramePack& pack, int ids, int actions)
{
	pack.Init(TYPE_FRAMEID(ids));
	for (int id = 0; id < ids; ++id)
	{
		pack[TYPE_FRAMEID(id)].Init(BYTE(actions));
		for (int action = 0; action < actions; ++action)
			pack[TYPE_FRAMEID(id)][BYTE(action)].Init(8);
	}
}

// Frame `frame` of (id, action, direction), the last, is the sprite at its
// offset; the frames before it are sprite 3 at (40 + before, 40).
void Place(CCreatureFramePack& pack, int id, int action, int direction, int frame, int sprite, int cx, int cy)
{
	FRAME_ARRAY& frames = pack[TYPE_FRAMEID(id)][BYTE(action)][BYTE(direction)];
	frames.Init(WORD(frame + 1));
	for (int before = 0; before < frame; ++before) frames[WORD(before)].Set(3, short(40 + before), 40);
	frames[WORD(frame)].Set(TYPE_SPRITEID(sprite), short(cx), short(cy));
}

struct Frame { int sprite, cx, cy; };

// The frames of (id, action, direction), as listed.
void Frames(CCreatureFramePack& pack, int id, int action, int direction, const std::vector<Frame>& list)
{
	FRAME_ARRAY& frames = pack[TYPE_FRAMEID(id)][BYTE(action)][BYTE(direction)];
	frames.Init(WORD(list.size()));
	for (size_t f = 0; f < list.size(); ++f) frames[WORD(f)].Set(TYPE_SPRITEID(list[f].sprite), short(list[f].cx), short(list[f].cy));
}

struct Canvas
{
	CSpriteSurface surface;
	WORD background = 0;
	Canvas() { CHECK_EQ(0, spritectl_init()); CHECK(surface.Init(kSize, kSize)); surface.FillSurface(0); }
	void Fill(WORD pixel) { background = pixel; surface.FillSurface(pixel); }
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
	bool Blank() { for (WORD pixel : Picture()) if (pixel != background) return false; return true; }
	// A plain blit, or one darkened by `darkBits`.
	void Blit(int x, int y, CIndexSprite& sprite, int darkBits = 0)
	{
		POINT point = { x, y };
		surface.Lock();
		if (darkBits == 0) surface.BltIndexSprite(&point, &sprite);
		else surface.BltIndexSpriteDarkness(&point, &sprite, BYTE(darkBits));
		surface.Unlock();
	}
	void Blit(int x, int y, CSprite& sprite)
	{
		POINT point = { x, y };
		surface.Lock();
		surface.BltSprite(&point, &sprite);
		surface.Unlock();
	}
};

const TYPE_OBJECTID kCreatureID = 100;

// A creature whose parts and state a test sets.
template<class Base> struct Probe : Base
{
	using Base::m_Addon;
	using Base::m_ShoulderAddon;
	using Base::m_Action;
	using Base::m_ActionCount;
	using Base::m_ActionCountMax;
	using Base::m_MoveCount;
	using Base::m_MoveCountMax;
	using Base::m_cX;
	using Base::m_cY;
	using Base::m_Z;
	using Base::m_InvisibleCount;
	using Base::m_bEffectStatus;
	using Base::m_AttachEffectColor;
	using Base::m_bAlive;
	using Base::m_bHasHead;
	using Base::m_bCutHeight;
	using Base::m_CutHeightCount;
	using Base::m_bInCasket;
	using Base::m_CasketCount;
	using Base::m_CasketType;
	using Base::m_bFakeDie;
	using Base::m_FakeDieCount;
	using Base::m_bFastMove;
	using Base::m_bBloodyZenith;
	using Base::m_BloodyZenithCount;
	using Base::m_ShadowCount;
	using Base::m_bHallu;
	using Base::m_HalluCreatureType;
	using Base::m_HalluCreatureFrameID;
	using Base::m_HalluAction;
	using Base::m_HalluFrame;
	using Base::m_DarknessCount;
	using Base::m_MoveType;
	using Base::m_MoveDevice;
	using Base::m_nSpecialActionInfo;
	using Base::m_nUsedActionInfo;
	using Base::m_bMale;
	using Base::m_NameSelection;

	// Of the type, bare, plain-coloured, alive, at rest on frame 0; not
	// selected (an object's ID starts as OBJECTID_NULL, the view's
	// selection when there is none).
	void Become(int type)
	{
		this->SetID(kCreatureID);
		this->SetCreatureType(TYPE_CREATURETYPE(type));
		for (auto& addon : m_Addon) addon.bAddon = FALSE;
		m_ShoulderAddon.bAddon = FALSE;
		m_AttachEffectColor = kNoEffectColor;
		this->SetBodyColor1(30);
		this->SetBodyColor2(60);
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
	// Frame `frame` of the action, which runs on.
	void Pose(int action, int frame)
	{
		m_Action = BYTE(action);
		m_ActionCountMax = 100;
		m_ActionCount = BYTE(frame);
	}
	// The action done: its frames counted out.
	void Finish(int action, int frames)
	{
		m_Action = BYTE(action);
		m_ActionCountMax = BYTE(frames);
		m_ActionCount = BYTE(frames);
	}
	void Set(EFFECTSTATUS status, bool on = true) { m_bEffectStatus[status] = on; }
	void Advance() { this->MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1); }
};
using Wearer = Probe<MCreatureWear>;
using Pet = Probe<MFakeCreature>;

struct PlayerProbe : MPlayer
{
	using MCreature::m_bEffectStatus;
	using MCreature::m_DarknessCount;
	using MCreature::m_TraceID;
	using MPlayer::m_fTrace;
	using MPlayer::m_LightSight;
	using MPlayer::m_Sight;
	void Set(EFFECTSTATUS status, bool on = true) { m_bEffectStatus[status] = on; }
};

struct ZoneProbe : MZone
{
	using MZone::MZone;
	using MZone::m_Info;
	using MZone::m_mapCreature;
};

// The sprite effects' global state, put back as it was.
struct IndexEffect : CIndexSprite565
{
	static decltype(s_pMemcpyEffectFunction) Get() { return s_pMemcpyEffectFunction; }
	static void Set(decltype(s_pMemcpyEffectFunction) effect) { s_pMemcpyEffectFunction = effect; }
};
struct EffectGuard
{
	decltype(IndexEffect::Get()) index = IndexEffect::Get();
	~EffectGuard() { IndexEffect::Set(index); }
};

// Creature types: a slayer, a player vampire and an ousters (players), a
// monster with a body and an NPC with one; a monster of two bodies.
const int kSlayer = 1, kVampire = 2, kOusters = 3, kMonster = 4, kNPC = 5, kTwoBodies = 6;
// The body frame sets: the monster's, the player vampire's, the second body's.
const int kBody = 1, kPlayerBody = 2, kSecondBody = 3;
const int kActions = 35;		// ACTION_MAX_SLAYER, the most of the plain packs
const int kAdvancedActions = 50;	// ACTION_NEW_SLAYER_MAX, the most of the advanced packs
const int kIds = 16;

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
	MEventQueue events;
	UserOption option;
	MObjectSelector selector;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MEventQueue* oldEvents = g_pEventManager;
	MPlayer* oldPlayer = g_pPlayer;
	MZone* oldZone = g_pZone;
	UserOption* oldOption = g_pUserOption;
	MObjectSelector* oldSelector = g_pObjectSelector;
	DWORD oldFrame = g_CurrentFrame;
	int savedSets[4];
	int savedValue = CSpriteSurface::s_Value1;
	decltype(CSpriteSurface::s_pMemcpyEffectFunction) savedSurfaceEffect = CSpriteSurface::s_pMemcpyEffectFunction;
	decltype(IndexEffect::Get()) savedIndexEffect = IndexEffect::Get();
	PlayerProbe* player = nullptr;
	std::unique_ptr<ZoneProbe> zone;
	Canvas drawn, expected;
	MTopView view;
	POINT at = { 40, 50 };
	POINT moved = { 0, 0 };

	Scene()
	{
		CHECK_EQ(0, spritectl_init());
		CIndexSprite::SetColorSet();
		for (int set = 0; set < 4; ++set) savedSets[set] = CIndexSprite::GetUsingColorSet(BYTE(set));
		CIndexSprite::SetUsingColorSet(1, 2);
		CIndexSprite::SetUsingColorSetOnly(2, kC2);
		CIndexSprite::SetUsingColorSetOnly(3, kC3);
		// Bit 2 clear: no kill-quest arrow; and a creature's draw gap is 0.
		g_CurrentFrame = 1;
		creatureTypes.Init(820);
		g_pCreatureTable = &creatureTypes;
		Type(kSlayer, CREATURETRIBE_SLAYER, {0});
		Type(kVampire, CREATURETRIBE_VAMPIRE, {4});
		Type(kOusters, CREATURETRIBE_OUSTERS, {2});
		Type(kMonster, CREATURETRIBE_VAMPIRE, {3});
		Type(kNPC, CREATURETRIBE_NPC, {3});
		Type(kTwoBodies, CREATURETRIBE_VAMPIRE, {3, 5});
		sprites.Init(6);
		Sprite(0, FRAMEID_NULL, FLAG_CREATURESPRITE_PLAYER_SLAYER);
		Sprite(1, FRAMEID_NULL, FLAG_CREATURESPRITE_PLAYER_SLAYER);
		Sprite(2, FRAMEID_NULL, FLAG_CREATURESPRITE_PLAYER_OUSTERS);
		Sprite(3, kBody, FLAG_CREATURESPRITE_MONSTER);
		Sprite(4, kPlayerBody, FLAG_CREATURESPRITE_PLAYER_VAMPIRE);
		Sprite(5, kSecondBody, FLAG_CREATURESPRITE_MONSTER);
		g_pCreatureSpriteTable = &sprites;
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		g_pRankBonusTable = &rankBonuses;
		g_pEventManager = &events;
		option.BlendingShadow = FALSE;
		option.DoNotShowPersnalShopMsg = FALSE;
		g_pUserOption = &option;
		g_pObjectSelector = &selector;
		g_pUserInformation->GoreLevel = true;
		zone.reset(new ZoneProbe(TYPE_SECTORPOSITION(16), TYPE_SECTORPOSITION(16)));
		g_pZone = zone.get();
		player = new PlayerProbe;
		g_pPlayer = player;
		player->SetCreatureType(kSlayer);
		player->SetID(1000);
		player->MObject::SetPosition(8, 8);
		// The view's packs, each sprite of a pack marked apart.
		for (CCreatureFramePack* pPack : {&view.m_CreatureFPK, &view.m_AddonFPK, &view.m_OustersFPK})
			ShapePack(*pPack, kIds, kActions);
		for (CCreatureFramePack* pPack : {&view.m_AdvancementSlayerManFPK, &view.m_AdvancementSlayerWomanFPK,
			&view.m_AdvancementVampireManFPK, &view.m_AdvancementVampireWomanFPK, &view.m_AdvancementOustersFPK})
			ShapePack(*pPack, kIds, kAdvancedActions);
		int n = 0;
		for (CIndexSpritePack* pPack : IndexPacks())
		{
			pPack->Init(4);
			for (int sprite = 0; sprite < 4; ++sprite, ++n) LoadMark((*pPack)[WORD(sprite)], WORD(0x1000 + 37 * n));
		}
		view.m_EtcSPK.Init(40);
		for (int sprite = 0; sprite < 40; ++sprite) LoadSprite(view.m_EtcSPK[WORD(sprite)], sprite);
		CSpriteSurface* pSurface = &drawn.surface;
		view.SetSurface(pSurface);
		view.SetZone(zone.get());
		view.m_ColorOutlineItem = 0x0101;
		view.m_ColorOutlineNPC = 0x0202;
		view.m_ColorOutlineAttackPossible = 0x0303;
		view.m_ColorOutlineAttackImpossible = 0x0404;
		view.m_SOMOutlineColor = 0;
		view.m_nLight = 0;
		view.m_pointChatString.x = -1;
		view.m_pointChatString.y = -1;
		view.m_pSelectedCreature = nullptr;
	}
	~Scene()
	{
		for (int set = 0; set < 4; ++set) CIndexSprite::SetUsingColorSetOnly(BYTE(set), savedSets[set]);
		CSpriteSurface::s_Value1 = savedValue;
		CSpriteSurface::s_pMemcpyEffectFunction = savedSurfaceEffect;
		IndexEffect::Set(savedIndexEffect);
		view.SetZone(nullptr);
		zone.reset();
		g_pZone = oldZone;
		delete player;
		g_pPlayer = oldPlayer;
		g_CurrentFrame = oldFrame;
		g_pObjectSelector = oldSelector;
		g_pUserOption = oldOption;
		g_pEventManager = oldEvents;
		g_pRankBonusTable = oldRankBonuses;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatureTypes;
	}
	std::vector<CIndexSpritePack*> IndexPacks()
	{
		return { &view.m_CreatureSPK, &view.m_AddonSPK, &view.m_OustersSPK, &view.m_AdvancementSlayerManSPK,
			&view.m_AdvancementSlayerWomanSPK, &view.m_AdvancementVampireManSPK, &view.m_AdvancementVampireWomanSPK,
			&view.m_AdvancementOustersSPK };
	}
	// A creature type of the tribe whose sprite types are listed.
	CREATURETABLE_INFO& Type(int type, CREATURETRIBE tribe, const std::vector<int>& spriteTypes)
	{
		CREATURETABLE_INFO& row = testfw::MutableRow(creatureTypes, type);
		row.SetCreatureTribe(tribe, kActions);
		row.SpriteTypes.Init(int(spriteTypes.size()));
		for (size_t i = 0; i < spriteTypes.size(); ++i) *row.SpriteTypes.GetMutable(int(i)) = spriteTypes[i];
		row.Height = 30;
		row.DeadHeight = 10;
		return row;
	}
	// A monster type with a body (frame set 1).
	CREATURETABLE_INFO& Monster(int type) { return Type(type, CREATURETRIBE_VAMPIRE, {3}); }
	void Sprite(int type, int frameID, int flags)
	{
		CREATURESPRITETABLE_INFO& row = testfw::MutableRow(sprites, type);
		row.FrameID = TYPE_FRAMEID(frameID);
		row.CreatureType = BYTE(flags);
	}
	// DrawCreature at `at`, on the locked surface; `moved` is where the
	// point it was given ends.
	void Draw(MCreature& creature)
	{
		moved = at;
		drawn.surface.Lock();
		Access::Creature(view, &moved, &creature);
		drawn.surface.Unlock();
	}
	template<class Draw> void Call(Draw draw)
	{
		moved = at;
		drawn.surface.Lock();
		draw(&moved);
		drawn.surface.Unlock();
	}
	// A blit at (dx, dy) from where the creature stands, in its colours.
	void Blit(int dx, int dy, CIndexSprite& sprite, int c0, int c1, int c2 = kC2, int c3 = kC3)
	{
		Darkened(0, dx, dy, sprite, c0, c1, c2, c3);
	}
	// The same darkened by `darkBits` (0 plain).
	void Darkened(int darkBits, int dx, int dy, CIndexSprite& sprite, int c0, int c1, int c2 = kC2, int c3 = kC3)
	{
		CIndexSprite::SetUsingColorSet(c0, c1);
		CIndexSprite::SetUsingColorSetOnly(2, c2);
		CIndexSprite::SetUsingColorSetOnly(3, c3);
		expected.Blit(at.x + dx, at.y + dy, sprite, darkBits);
	}
	void Blit(int dx, int dy, CSprite& sprite) { expected.Blit(at.x + dx, at.y + dy, sprite); }
	// A blit at (dx, dy) in the one colour set `colorSet`.
	void ColorSet(int dx, int dy, CIndexSprite& sprite, int colorSet)
	{
		POINT point = { at.x + dx, at.y + dy };
		expected.surface.Lock();
		expected.surface.BltIndexSpriteColorSet(&point, &sprite, WORD(colorSet));
		expected.surface.Unlock();
	}
	// The drawn picture is the expected one, and not blank.
	bool Same()
	{
		const std::vector<WORD> a = drawn.Picture(), b = expected.Picture();
		bool any = false;
		for (WORD pixel : a) any |= pixel != drawn.background;
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
		drawn.Fill(drawn.background);
		expected.Fill(expected.background);
	}
	CIndexSprite& Body(int sprite) { return view.m_CreatureSPK[WORD(sprite)]; }
	// What gameview's drawing makes of it, on the expected picture, from
	// the colour sets every scene starts with.
	typedef void (*Effect)(const CreatureDrawPacks&, POINT*, MCreature*, int, int, int);
	void Expect(Effect draw, MCreature& creature, int action, int direction, int frame)
	{
		ResetSets();
		CreatureDrawPacks packs = MTopViewTestAccess4::Packs(view);
		packs.pSurface = &expected.surface;
		POINT point = at;
		expected.surface.Lock();
		draw(packs, &point, &creature, action, direction, frame);
		expected.surface.Unlock();
	}
	// The colour sets every scene starts with.
	static void ResetSets()
	{
		CIndexSprite::SetUsingColorSet(1, 2);
		CIndexSprite::SetUsingColorSetOnly(2, kC2);
		CIndexSprite::SetUsingColorSetOnly(3, kC3);
	}
	// The sprite wiped out by `value` at (dx, dy), in the colours given.
	void Wipe(int dx, int dy, CIndexSprite& sprite, int c0, int c1, int value)
	{
		CIndexSprite::SetUsingColorSet(c0, c1);
		CIndexSprite::SetUsingColorSetOnly(2, kC2);
		CIndexSprite::SetUsingColorSetOnly(3, kC3);
		const int oldValue = CSpriteSurface::s_Value1;
		const auto oldEffect = IndexEffect::Get();
		CSpriteSurface::s_Value1 = value;
		CIndexSprite::SetEffect(CIndexSprite::EFFECT_WIPE_OUT);
		POINT point = { at.x + dx, at.y + dy };
		expected.surface.Lock();
		expected.surface.BltIndexSpriteEffect(&point, &sprite);
		expected.surface.Unlock();
		CSpriteSurface::s_Value1 = oldValue;
		IndexEffect::Set(oldEffect);
	}
	// A sprite through the surface's wipe-out effect at `value`.
	void WipeSprite(int dx, int dy, CSprite& sprite, int value)
	{
		const int oldValue = CSpriteSurface::s_Value1;
		const auto oldEffect = CSpriteSurface::s_pMemcpyEffectFunction;
		CSpriteSurface::s_Value1 = value;
		CSpriteSurface::SetEffect(CSpriteSurface::EFFECT_WIPE_OUT);
		POINT point = { at.x + dx, at.y + dy };
		expected.surface.Lock();
		expected.surface.BltSpriteEffect(&point, &sprite);
		expected.surface.Unlock();
		CSpriteSurface::s_Value1 = oldValue;
		CSpriteSurface::s_pMemcpyEffectFunction = oldEffect;
	}
};

bool SameRect(const RECT& a, int left, int top, int right, int bottom)
{
	const bool same = a.left == left && a.top == top && a.right == right && a.bottom == bottom;
	if (!same) std::printf("      the rect is (%d,%d)-(%d,%d), not (%d,%d)-(%d,%d)\n",
		int(a.left), int(a.top), int(a.right), int(a.bottom), left, top, right, bottom);
	return same;
}

bool Cleared(const RECT& a) { return SameRect(a, 800, 600, 0, 0); }

int Special(int colorSet) { return MItem::GetSpecialColorItemColorset(colorSet); }

// The effect functions of the net, as the index sprites and the surface
// name them.
decltype(IndexEffect::Get()) IndexNet()
{
	EffectGuard guard;
	CIndexSprite::SetEffect(CIndexSprite::EFFECT_NET);
	return IndexEffect::Get();
}
decltype(CSpriteSurface::s_pMemcpyEffectFunction) SurfaceEffect(CSpriteSurface::FUNCTION_EFFECT effect)
{
	const auto old = CSpriteSurface::s_pMemcpyEffectFunction;
	CSpriteSurface::SetEffect(effect);
	const auto named = CSpriteSurface::s_pMemcpyEffectFunction;
	CSpriteSurface::s_pMemcpyEffectFunction = old;
	return named;
}

} // namespace

namespace {

struct Entry { int x, y; const void* sprite; int color; };

bool SameEntries(const CSpriteOutlineManager& outlines, const std::vector<Entry>& expected)
{
	const std::vector<OutlineEntry>& entries = outlines.GetOutlines();
	bool same = entries.size() == expected.size();
	for (size_t i = 0; same && i < entries.size(); ++i)
	{
		const void* sprite = entries[i].pIndexSprite ? static_cast<const void*>(entries[i].pIndexSprite) : static_cast<const void*>(entries[i].pSprite);
		same = entries[i].x == expected[i].x && entries[i].y == expected[i].y && sprite == expected[i].sprite
			&& int(entries[i].color) == expected[i].color;
	}
	if (!same)
	{
		std::printf("      %d entries:", int(entries.size()));
		for (const OutlineEntry& e : entries) std::printf(" (%d,%d c%d)", e.x, e.y, int(e.color));
		std::printf("\n");
	}
	return same;
}

bool SetsAre(int c0, int c1, int c2 = kC2, int c3 = kC3)
{
	const bool same = CIndexSprite::GetUsingColorSet(0) == c0 && CIndexSprite::GetUsingColorSet(1) == c1
		&& CIndexSprite::GetUsingColorSet(2) == c2 && CIndexSprite::GetUsingColorSet(3) == c3;
	if (!same) std::printf("      the sets are %d %d %d %d\n", CIndexSprite::GetUsingColorSet(0), CIndexSprite::GetUsingColorSet(1),
		CIndexSprite::GetUsingColorSet(2), CIndexSprite::GetUsingColorSet(3));
	return same;
}

} // namespace

//----------------------------------------------------------------------
// The packs the view hands gameview
//----------------------------------------------------------------------
TEST(TopView4Packs, EveryPackIsTheViewsOwn)
{
	Scene scene;
	scene.view.m_SelectCreatureID = 77;
	MTopView& v = scene.view;
	const CreatureDrawPacks packs = Access::Packs(v);
	CHECK(packs.pSurface == &scene.drawn.surface);
	CHECK(packs.pSurface == Access::Surface(v));
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
// The shadow
//----------------------------------------------------------------------
TEST(TopView4Shadow, TheShadowIsGameviewsWithTheUsersBlending)
{
	// A slayer's parts cast a blended shadow at the user's choice.
	Scene scene;
	ShapePack(scene.view.m_AddonShadowFPK, kIds, kActions);
	scene.view.m_AddonSSPK.Init(3);
	for (int sprite = 0; sprite < 3; ++sprite) LoadShadow(scene.view.m_AddonSSPK[WORD(sprite)], sprite + 5);
	Place(scene.view.m_AddonShadowFPK, 1, ACTION_ATTACK, 3, 2, 1, 5, 6);
	Wearer slayer;
	slayer.Become(kSlayer);
	slayer.SetCurrentDirection(3);
	slayer.Pose(ACTION_ATTACK, 2);
	slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
	std::vector<WORD> pictures[2];
	for (BOOL blending : {FALSE, TRUE})
	{
		scene.drawn.Fill(0xFFFF);
		scene.expected.Fill(0xFFFF);
		scene.option.BlendingShadow = blending;
		scene.Call([&](POINT* p) { Access::Shadow(scene.view, p, &slayer); });
		CreatureDrawPacks packs = Access::Packs(scene.view);
		packs.pSurface = &scene.expected.surface;
		POINT point = scene.at;
		scene.expected.surface.Lock();
		::DrawCreatureShadow(packs, &point, &slayer, blending != FALSE);
		scene.expected.surface.Unlock();
		CHECK(scene.Same());
		pictures[blending ? 1 : 0] = scene.drawn.Picture();
	}
	// The option shows: a blended shadow is not a plain one.
	CHECK(pictures[0] != pictures[1]);
}

//----------------------------------------------------------------------
// Whom the view does not draw
//----------------------------------------------------------------------
TEST(TopView4Escape, ACreatureIsDrawnUnlessAGhostOrAnEventHidesIt)
{
	Scene scene;
	Wearer monster;
	monster.Become(kMonster);
	CHECK(!IsEscapeDrawCreatureFunction(&monster));
	monster.Set(EFFECTSTATUS_GHOST);
	CHECK(IsEscapeDrawCreatureFunction(&monster));
	monster.Set(EFFECTSTATUS_GHOST, false);
	// A ghost in all three parts is hidden; in two of them it is not.
	for (BYTE parts : {BYTE(1 | 2), BYTE(1 | 4), BYTE(2 | 4)})
	{
		monster.SetGhost(parts, 10);
		CHECK(!IsEscapeDrawCreatureFunction(&monster));
	}
	monster.SetGhost(1 | 2 | 4, 10);
	CHECK(IsEscapeDrawCreatureFunction(&monster));
	monster.SetGhost(0, 0);
	MEvent hide;
	hide.eventID = EVENTID_RESURRECT;
	hide.eventFlag = EVENTFLAG_NOT_DRAW_CREATURE;
	hide.eventType = EVENTTYPE_ZONE;
	hide.eventDelay = -1;
	scene.events.AddEvent(hide);
	CHECK(IsEscapeDrawCreatureFunction(&monster));
	scene.events.RemoveEvent(EVENTID_RESURRECT);
	CHECK(!IsEscapeDrawCreatureFunction(&monster));
}

TEST(TopView4Escape, ABlindPlayerSeesOnlyTheNineSectorsAroundIt)
{
	for (EFFECTSTATUS blind : {EFFECTSTATUS_YELLOW_POISON_TO_CREATURE, EFFECTSTATUS_FLARE, EFFECTSTATUS_BLINDNESS})
	{
		Scene scene;
		Wearer monster;
		monster.Become(kMonster);
		monster.MObject::SetPosition(10, 10);
		CHECK(!IsEscapeDrawCreatureFunction(&monster));
		scene.player->Set(blind);
		CHECK(IsEscapeDrawCreatureFunction(&monster));
		// The player stands at (8, 8): 7 to 9 either way is seen.
		struct Spot { int x, y; bool seen; };
		for (const Spot& spot : {Spot{7, 7, true}, Spot{9, 9, true}, Spot{7, 9, true}, Spot{9, 7, true}, Spot{8, 8, true},
			Spot{6, 8, false}, Spot{10, 8, false}, Spot{8, 6, false}, Spot{8, 10, false}})
		{
			monster.MObject::SetPosition(TYPE_SECTORPOSITION(spot.x), TYPE_SECTORPOSITION(spot.y));
			const bool hidden = IsEscapeDrawCreatureFunction(&monster);
			if (hidden == spot.seen) std::printf("      (%d,%d) under %d\n", spot.x, spot.y, int(blind));
			CHECK_EQ(!spot.seen, hidden);
		}
		// The player itself always.
		scene.player->MObject::SetPosition(1, 1);
		CHECK(!IsEscapeDrawCreatureFunction(scene.player));
	}
}

TEST(TopView4Escape, APetIsHiddenWhileItsOwnerIsInDarkness)
{
	Scene scene;
	Pet pet;
	pet.Become(kMonster);
	pet.SetOwnerID(88);
	CHECK(pet.IsFakeCreature());
	CHECK(!IsEscapeDrawCreatureFunction(&pet));
	Wearer owner;
	owner.Become(kMonster);
	scene.zone->m_mapCreature[88] = &owner;
	CHECK(!IsEscapeDrawCreatureFunction(&pet));
	owner.m_DarknessCount = 0;
	CHECK(IsEscapeDrawCreatureFunction(&pet));
	// A creature that is not a pet is not hidden by another in darkness.
	Wearer other;
	other.Become(kMonster);
	CHECK(!IsEscapeDrawCreatureFunction(&other));
	scene.zone->m_mapCreature.erase(88);
	CHECK(!IsEscapeDrawCreatureFunction(&pet));
}

namespace {

MSector elementalSector;

const ZoneState::Host elementalZone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return true; };
	host.GetWidth = []() { return 16; };
	host.GetHeight = []() { return 16; };
	host.GetSector = [](TYPE_SECTORPOSITION, TYPE_SECTORPOSITION) -> const MSector* { return &elementalSector; };
	return host;
}();

} // namespace

TEST(TopView4Escape, AnOustersPlayerInAGroundElementalIsHiddenButFromOusters)
{
	Scene scene;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&elementalZone);
	elementalSector.SetGroundElemental();
	Wearer ousters;
	ousters.Become(kOusters);
	ousters.MObject::SetPosition(3, 3);
	CHECK(ousters.IsInGroundElemental());
	CHECK(IsEscapeDrawCreatureFunction(&ousters));
	scene.player->SetCreatureType(kOusters);
	CHECK(!IsEscapeDrawCreatureFunction(&ousters));
	scene.player->SetCreatureType(kSlayer);
	// Not a player's sprite: drawn.
	scene.Type(9, CREATURETRIBE_OUSTERS, {3});
	ousters.Become(9);
	CHECK(!IsEscapeDrawCreatureFunction(&ousters));
	ousters.Become(kOusters);
	elementalSector.UnSetGroundElemental();
	CHECK(!IsEscapeDrawCreatureFunction(&ousters));
	ZoneState::SetHost(oldZone);
}

//----------------------------------------------------------------------
// A body
//----------------------------------------------------------------------
TEST(TopView4Body, AMonsterIsDrawnFromItsBodysFrameInItsColours)
{
	Scene scene;
	Wearer monster;
	monster.Become(kMonster);
	monster.SetCurrentDirection(3);
	monster.Pose(ACTION_ATTACK, 2);
	Place(scene.view.m_CreatureFPK, kBody, ACTION_ATTACK, 3, 2, 1, 5, 6);
	monster.ClearScreenRect();
	scene.Draw(monster);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
	CHECK(SameRect(monster.GetScreenRect(), 45, 56, 50, 57));
	CHECK_EQ(scene.at.x, scene.moved.x);
	CHECK_EQ(scene.at.y, scene.moved.y);
	// Nothing for a frame the action has not got, and the rect is left.
	monster.Pose(ACTION_ATTACK, 3);
	monster.ClearScreenRect();
	scene.Clear();
	scene.Draw(monster);
	CHECK(scene.drawn.Blank());
	CHECK(Cleared(monster.GetScreenRect()));
}

TEST(TopView4Body, EveryBodyIsDrawnInTurnAndTheFirstSetsTheRect)
{
	Scene scene;
	Wearer monster;
	monster.Become(kTwoBodies);
	monster.SetCurrentDirection(1);
	monster.Pose(ACTION_MAGIC, 0);
	Place(scene.view.m_CreatureFPK, kBody, ACTION_MAGIC, 1, 0, 1, 5, 6);
	Place(scene.view.m_CreatureFPK, kSecondBody, ACTION_MAGIC, 1, 0, 2, 20, 9);
	monster.ClearScreenRect();
	scene.Draw(monster);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	scene.Blit(20, 9, scene.Body(2), 30, 60);
	CHECK(scene.Same());
	CHECK(SameRect(monster.GetScreenRect(), 45, 56, 50, 57));
	// On one spot the second lies over the first.
	Place(scene.view.m_CreatureFPK, kSecondBody, ACTION_MAGIC, 1, 0, 2, 5, 6);
	scene.Clear();
	scene.Draw(monster);
	scene.Blit(5, 6, scene.Body(2), 30, 60);
	CHECK(scene.Same());
}

TEST(TopView4Body, SpecialBodyColoursAreTheirItemColourSets)
{
	Scene scene;
	Wearer monster;
	monster.Become(kMonster);
	monster.SetCurrentDirection(3);
	monster.SetBodyColor1(UNIQUE_ITEM_COLOR);
	monster.SetBodyColor2(QUEST_ITEM_COLOR);
	Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
	scene.Draw(monster);
	scene.Blit(5, 6, scene.Body(1), Special(UNIQUE_ITEM_COLOR), Special(QUEST_ITEM_COLOR));
	CHECK(scene.Same());
	// The other way round.
	monster.SetBodyColor1(QUEST_ITEM_COLOR);
	monster.SetBodyColor2(UNIQUE_ITEM_COLOR);
	scene.Clear();
	scene.Draw(monster);
	scene.Blit(5, 6, scene.Body(1), Special(QUEST_ITEM_COLOR), Special(UNIQUE_ITEM_COLOR));
	CHECK(scene.Same());
}

TEST(TopView4Body, AnEffectColourOrABatsColourTakesTheColourSetBlit)
{
	// The body shows in the one effect colour set, whatever its own.
	Scene scene;
	Wearer monster;
	monster.Become(kMonster);
	monster.SetCurrentDirection(3);
	Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
	monster.m_AttachEffectColor = 5;
	scene.Draw(monster);
	scene.ColorSet(5, 6, scene.Body(1), 5);
	CHECK(scene.Same());
	// The last effect colour set below MAX_COLORSET is still one.
	monster.m_AttachEffectColor = MAX_COLORSET - 1;
	scene.Clear();
	scene.Draw(monster);
	scene.ColorSet(5, 6, scene.Body(1), MAX_COLORSET - 1);
	CHECK(scene.Same());
	monster.m_AttachEffectColor = MAX_COLORSET;
	scene.Clear();
	scene.Draw(monster);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
	// A bat in a colour of its own takes it; without one, its body's.
	scene.Monster(CREATURETYPE_BAT);
	Wearer bat;
	bat.Become(CREATURETYPE_BAT);
	bat.SetCurrentDirection(3);
	bat.SetBatColor(5);
	Scene::ResetSets();
	scene.Clear();
	scene.Draw(bat);
	scene.ColorSet(5, 6, scene.Body(1), 5);
	CHECK(scene.Same());
	bat.SetBatColor(0xFFFF);
	scene.Clear();
	scene.Draw(bat);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
	// Another creature's bat colour is not read.
	monster.m_AttachEffectColor = kNoEffectColor;
	monster.SetBatColor(5);
	scene.Clear();
	scene.Draw(monster);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
}

TEST(TopView4Body, AFadingBodyAndACutOneAreStillDrawn)
{
	Scene scene;
	Wearer monster;
	monster.Become(kMonster);
	monster.SetCurrentDirection(3);
	Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
	monster.SetFade();
	scene.Draw(monster);
	scene.Darkened(1, 5, 6, scene.Body(1), 30, 60);			// a bit darker
	CHECK(scene.Same());
	// Cut, it sinks by its count less 24; the rect stays where it stood.
	monster.UnSetFade();
	monster.m_bCutHeight = true;
	monster.m_CutHeightCount = 30;
	monster.ClearScreenRect();
	scene.Clear();
	scene.Draw(monster);
	scene.Blit(5, 12, scene.Body(1), 30, 60);
	CHECK(scene.Same());
	CHECK(SameRect(monster.GetScreenRect(), 45, 56, 50, 57));
}

TEST(TopView4Body, AFakeDyingBodyIsDrawnThroughTheNetOnItsFirstFrame)
{
	Scene scene;
	Wearer monster;
	monster.Become(kMonster);
	monster.SetCurrentDirection(3);
	monster.Pose(ACTION_ATTACK, 2);
	monster.m_bFakeDie = true;
	monster.m_FakeDieCount = 23;
	Place(scene.view.m_CreatureFPK, kBody, ACTION_ATTACK, 3, 2, 1, 5, 6);
	CIndexSprite::SetEffect(CIndexSprite::EFFECT_WIPE_OUT);
	CSpriteSurface::s_Value1 = 99;
	scene.Draw(monster);
	// Frame 0, the net at a fifth of its count (SDL draws it plainly).
	scene.Blit(40, 40, scene.Body(3), 30, 60);
	CHECK(scene.Same());
	CHECK_EQ(4, CSpriteSurface::s_Value1);
	CHECK(IndexEffect::Get() == IndexNet());
	CHECK(CSpriteSurface::s_pMemcpyEffectFunction == SurfaceEffect(CSpriteSurface::EFFECT_NET));
	// In an effect colour it takes the colour-set blit, which leaves the set
	// in s_Value1, and leaves the net.
	monster.m_AttachEffectColor = 5;
	CSpriteSurface::s_Value1 = 99;
	CIndexSprite::SetEffect(CIndexSprite::EFFECT_WIPE_OUT);
	Scene::ResetSets();
	scene.Clear();
	scene.Draw(monster);
	scene.ColorSet(40, 40, scene.Body(3), 5);
	CHECK(scene.Same());
	CHECK_EQ(5, CSpriteSurface::s_Value1);
	CHECK(CIndexSprite::IsWipeOutEffect());
	// The last set is an effect colour; the count of sets is not.
	monster.m_AttachEffectColor = MAX_COLORSET - 1;
	scene.Draw(monster);
	CHECK_EQ(MAX_COLORSET - 1, CSpriteSurface::s_Value1);
	monster.m_AttachEffectColor = MAX_COLORSET;
	scene.Draw(monster);
	CHECK_EQ(4, CSpriteSurface::s_Value1);
}

TEST(TopView4Body, AnInvisibleBodyWipesOutByItsCount)
{
	Scene scene;
	Wearer monster;
	monster.Become(kMonster);
	monster.SetCurrentDirection(3);
	Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
	for (int count : {1, 32, 63})
	{
		monster.m_InvisibleCount = count;
		CSpriteSurface::s_Value1 = 99;
		CIndexSprite::SetEffect(CIndexSprite::EFFECT_NET);
		scene.Clear();
		scene.Draw(monster);
		scene.Wipe(5, 6, scene.Body(1), 30, 60, count);
		CHECK(scene.Same());
		CHECK_EQ(count, CSpriteSurface::s_Value1);
		CHECK(CIndexSprite::IsWipeOutEffect());
	}
	// Wholly gone at 64: nothing, and the effect is left.
	monster.m_InvisibleCount = 64;
	CSpriteSurface::s_Value1 = 99;
	CIndexSprite::SetEffect(CIndexSprite::EFFECT_NET);
	scene.Clear();
	scene.Draw(monster);
	CHECK(scene.drawn.Blank());
	CHECK_EQ(99, CSpriteSurface::s_Value1);
	CHECK(!CIndexSprite::IsWipeOutEffect());
	// An effect colour tints both sets; special body colours are mapped.
	monster.m_InvisibleCount = 16;
	monster.m_AttachEffectColor = 5;
	scene.Clear();
	scene.Draw(monster);
	scene.Wipe(5, 6, scene.Body(1), 5, 5, 16);
	CHECK(scene.Same());
	monster.m_AttachEffectColor = kNoEffectColor;
	monster.SetBodyColor1(QUEST_ITEM_COLOR);
	monster.SetBodyColor2(UNIQUE_ITEM_COLOR);
	scene.Clear();
	scene.Draw(monster);
	scene.Wipe(5, 6, scene.Body(1), Special(QUEST_ITEM_COLOR), Special(UNIQUE_ITEM_COLOR), 16);
	CHECK(scene.Same());
	// A bat in its own colour takes it.
	scene.Monster(CREATURETYPE_BAT);
	Wearer bat;
	bat.Become(CREATURETYPE_BAT);
	bat.SetCurrentDirection(3);
	bat.SetBatColor(9);
	bat.m_InvisibleCount = 16;
	scene.Clear();
	scene.Draw(bat);
	scene.Wipe(5, 6, scene.Body(1), 9, 9, 16);
	CHECK(scene.Same());
	// Any bat colour but 0xFFFF overrides the effect colour, even one past
	// the sets (then the body's colours); the last set is a tint, the
	// count of sets is not.
	struct Case { WORD batColor; WORD effectColor; int c0, c1; };
	for (const Case& c : {Case{0xFFFE, 5, 30, 60}, Case{0xFFFF, 5, 5, 5}, Case{0xFFFF, MAX_COLORSET - 1, MAX_COLORSET - 1, MAX_COLORSET - 1},
		Case{0xFFFF, MAX_COLORSET, 30, 60}})
	{
		bat.SetBatColor(c.batColor);
		bat.m_AttachEffectColor = c.effectColor;
		scene.Clear();
		scene.Draw(bat);
		scene.Wipe(5, 6, scene.Body(1), c.c0, c.c1, 16);
		const bool same = scene.Same();
		if (!same) std::printf("      bat colour %04x, effect colour %d\n", unsigned(c.batColor), int(c.effectColor));
		CHECK(same);
	}
}

TEST(TopView4Body, AFastMoverLeavesDarkenedCopiesBehindIt)
{
	// Two copies, a wild wolf six, each half its step further back, the
	// farthest first, the nearest darkened by one bit and the rest by two;
	// then the body.
	for (int type : {kMonster, CREATURETYPE_WILD_WOLF})
	{
		Scene scene;
		scene.Monster(CREATURETYPE_WILD_WOLF);
		Wearer monster;
		monster.Become(type);
		monster.SetCurrentDirection(3);
		monster.m_bFastMove = true;
		monster.m_cX = 8;
		monster.m_cY = 4;
		Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
		scene.Draw(monster);
		const int copies = type == kMonster ? 2 : 6;
		for (int n = copies; n > 0; --n) scene.Darkened((std::min)(2, n), 5 - 4 * n, 6 - 2 * n, scene.Body(1), 30, 60);
		scene.Blit(5, 6, scene.Body(1), 30, 60);
		CHECK(scene.Same());
	}
}

TEST(TopView4Body, BloodyZenithRaisesItsCopiesAndItsBodyOnASine)
{
	Scene scene;
	Wearer monster;
	monster.Become(kMonster);
	monster.SetCurrentDirection(3);
	monster.m_bBloodyZenith = true;
	monster.m_BloodyZenithCount = 3;
	monster.m_MoveCount = 3;
	monster.m_MoveCountMax = 6;
	Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 60);
	scene.Draw(monster);
	// Copy n rises sin((3 - n) pi / 6) * 100, darkened by min(2, n) bits;
	// the body sin(3 pi / 6) * 100.
	scene.Darkened(2, 5, 60, scene.Body(1), 30, 60);
	scene.Darkened(2, 5, 10, scene.Body(1), 30, 60);
	scene.Darkened(1, 5, -27, scene.Body(1), 30, 60);
	scene.Blit(5, -40, scene.Body(1), 30, 60);
	CHECK(scene.Same());
	// Each draw counts it down.
	CHECK_EQ(2, monster.GetBloodyZenithCount());
	// Moving fast too, it keeps its count.
	monster.m_bFastMove = true;
	scene.Clear();
	scene.Draw(monster);
	CHECK_EQ(2, monster.GetBloodyZenithCount());
}

TEST(TopView4Body, AfterimagesTrailTheBodyFromItsFramesBefore)
{
	// Frame 1 is drawn: the afterimages are frames 0 and 3 (wrapped), the
	// second stepped back by the move, each darkened by a bit (the first
	// two of 0, 1, 1, 2, 2, 3). Facing 4 or less they go first.
	for (int direction : {3, 4, 5, 7})
	{
		Scene scene;
		Wearer monster;
		monster.Become(kMonster);
		monster.SetCurrentDirection(BYTE(direction));
		monster.Pose(ACTION_MOVE, 1);
		monster.m_ShadowCount = 2;
		monster.m_cX = 4;
		monster.m_cY = 2;
		Frames(scene.view.m_CreatureFPK, kBody, ACTION_MOVE, direction, {{2, 5, 6}, {1, 5, 6}, {0, 70, 0}, {3, 30, 20}});
		scene.Draw(monster);
		const bool first = direction <= 4;
		if (!first) scene.Blit(5, 6, scene.Body(1), 30, 60);
		scene.Darkened(1, 5, 6, scene.Body(2), 30, 60);
		scene.Darkened(1, 30 - 4, 20 - 2, scene.Body(3), 30, 60);
		if (first) scene.Blit(5, 6, scene.Body(1), 30, 60);
		const bool same = scene.Same();
		if (!same) std::printf("      facing %d\n", direction);
		CHECK(same);
	}
}

TEST(TopView4Body, AfterimagesOfAnyOtherActionStandStill)
{
	Scene scene;
	Wearer monster;
	monster.Become(kMonster);
	monster.SetCurrentDirection(2);
	monster.Pose(ACTION_ATTACK, 0);
	monster.m_ShadowCount = 3;
	monster.m_cX = 4;
	monster.m_cY = 2;
	Frames(scene.view.m_CreatureFPK, kBody, ACTION_ATTACK, 2, {{1, 5, 6}, {0, 70, 0}, {3, 30, 20}, {2, 12, 30}});
	scene.Draw(monster);
	// The third afterimage darker, by two bits.
	scene.Darkened(1, 12, 30, scene.Body(2), 30, 60);
	scene.Darkened(1, 30, 20, scene.Body(3), 30, 60);
	scene.Darkened(2, 70, 0, scene.Body(0), 30, 60);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
}

TEST(TopView4Body, Types764And765DrawTheirAfterimagesFirstWhicheverWayTheyFace)
{
	for (int type : {764, 765, kMonster})
	{
		Scene scene;
		scene.Monster(764);
		scene.Monster(765);
		Wearer monster;
		monster.Become(type);
		monster.SetCurrentDirection(6);
		monster.m_ShadowCount = 1;
		Frames(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 6, {{2, 5, 6}, {1, 5, 6}});
		scene.Call([&](POINT* p) { Access::Vampire(scene.view, p, &monster, ACTION_STAND, 6, 1, kBody, 0); });
		// Facing 6 another type's afterimage, darkened, lies over its body.
		if (type == kMonster) scene.Darkened(1, 5, 6, scene.Body(2), 30, 60);
		else scene.Blit(5, 6, scene.Body(1), 30, 60);
		CHECK(scene.Same());
	}
}

TEST(TopView4Body, ACasketStandsOnTheTileAndHidesTheBodyWhileWhole)
{
	Scene scene;
	Wearer monster;
	monster.Become(kMonster);
	monster.SetCurrentDirection(3);
	Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
	monster.m_bInCasket = true;
	monster.m_CasketType = 1;
	monster.m_CasketCount = 0;
	// Casket 1 is sprite 21: three wide, one high; centred on the tile.
	CSprite& casket = scene.view.m_EtcSPK[SPRITEID_CASKET_1 + 2];
	CHECK_EQ(3, int(casket.GetWidth()));
	CHECK_EQ(1, int(casket.GetHeight()));
	monster.ClearScreenRect();
	scene.Draw(monster);
	scene.Blit(23, 23, casket);
	CHECK(scene.Same());
	CHECK(SameRect(monster.GetScreenRect(), 63, 73, 66, 74));
	// Wiping in, the body is drawn and the casket through the wipe.
	for (int count : {64, 20})
	{
		CSpriteSurface::s_Value1 = 7;
		monster.m_CasketCount = count;
		scene.Clear();
		scene.Draw(monster);
		scene.Blit(5, 6, scene.Body(1), 30, 60);
		scene.WipeSprite(23, 23, casket, count == 64 ? 7 : count);
		CHECK(scene.Same());
		CHECK_EQ(count == 64 ? 7 : count, CSpriteSurface::s_Value1);
		CHECK(CSpriteSurface::s_pMemcpyEffectFunction == SurfaceEffect(CSpriteSurface::EFFECT_WIPE_OUT));
		CHECK(SameRect(monster.GetScreenRect(), 63, 73, 66, 74));
	}
}

TEST(TopView4Body, ACasketPastTheSpritesIsNotDrawn)
{
	// Caskets are every other sprite from 19: of 41 sprites type 10 (39)
	// is the last, type 11 (41) one past; for each drawing of a casket.
	for (int drawing = 0; drawing < 4; ++drawing)
	{
		Scene scene;
		scene.view.m_EtcSPK.Init(41);
		for (int sprite = 0; sprite < 41; ++sprite) LoadSprite(scene.view.m_EtcSPK[WORD(sprite)], sprite);
		Wearer vampire;
		vampire.Become(drawing % 2 ? kVampire : kMonster);
		if (drawing % 2) vampire.Advance();
		vampire.m_bInCasket = true;
		vampire.m_CasketCount = 0;
		vampire.m_CasketType = 10;
		CSprite& last = scene.view.m_EtcSPK[39];
		auto draw = [&]() {
			scene.view.m_SOM.Clear();
			scene.Call([&](POINT* p) {
				switch (drawing)
				{
				case 0: Access::Vampire(scene.view, p, &vampire, ACTION_STAND, 2, 0, kBody, 0); break;
				case 1: Access::AdvancedVampire(scene.view, p, &vampire, ACTION_STAND, 2, 0, kPlayerBody, 0); break;
				case 2: Access::SelectedVampire(scene.view, p, &vampire, ACTION_STAND, 2, 0, kBody, 0); break;
				default: Access::SelectedAdvancedVampire(scene.view, p, &vampire, ACTION_STAND, 2, 0, 0, 0); break;
				}
			});
		};
		const int x = 24 - (last.GetWidth() >> 1), y = 24 - int(last.GetHeight());
		vampire.ClearScreenRect();
		draw();
		if (drawing < 2)
		{
			scene.Blit(x, y, last);
			CHECK(scene.Same());
		}
		else CHECK(SameEntries(scene.view.m_SOM, {{40 + x, 50 + y, &last, 0}}));
		CHECK(SameRect(vampire.GetScreenRect(), 40 + x, 50 + y, 40 + x + last.GetWidth(), 50 + y + last.GetHeight()));
		vampire.m_CasketType = 11;
		vampire.ClearScreenRect();
		scene.Clear();
		draw();
		CHECK(scene.drawn.Blank());
		CHECK(scene.view.m_SOM.GetOutlines().empty());
		CHECK(Cleared(vampire.GetScreenRect()));
	}
}

//----------------------------------------------------------------------
// What DrawCreature makes of the creature's state
//----------------------------------------------------------------------
TEST(TopView4Creature, NothingForACreatureItDoesNotDraw)
{
	Scene scene;
	Wearer monster;
	monster.Become(kMonster);
	monster.SetCurrentDirection(3);
	Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
	monster.Set(EFFECTSTATUS_GHOST);
	monster.ClearScreenRect();
	scene.Draw(monster);
	CHECK(scene.drawn.Blank());
	CHECK(Cleared(monster.GetScreenRect()));
	monster.Set(EFFECTSTATUS_GHOST, false);
	scene.Draw(monster);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
}

TEST(TopView4Creature, ACharmedCreatureIsDrainedOnTheRepeatingFrames)
{
	// Frame f is drawn as frame 6 + (f & 7) of being drained.
	for (int frame : {0, 9, 15})
	{
		Scene scene;
		Wearer monster;
		monster.Become(kMonster);
		monster.SetCurrentDirection(3);
		monster.Pose(ACTION_ATTACK, frame);
		monster.Set(EFFECTSTATUS_CHARM);
		Place(scene.view.m_CreatureFPK, kBody, ACTION_DRAINED, 3, 6 + (frame & 7), 1, 5, 6);
		scene.Draw(monster);
		scene.Blit(5, 6, scene.Body(1), 30, 60);
		CHECK(scene.Same());
	}
}

TEST(TopView4Creature, ACrashedBikeAndAFakeDeathShowFrame0)
{
	for (EFFECTSTATUS status : {EFFECTSTATUS_BIKE_CRASH, EFFECTSTATUS_MAX})
	{
		Scene scene;
		Wearer monster;
		monster.Become(kMonster);
		monster.SetCurrentDirection(3);
		monster.Pose(ACTION_ATTACK, 2);
		if (status == EFFECTSTATUS_MAX) monster.m_bFakeDie = true;
		else monster.Set(status);
		monster.m_AttachEffectColor = 5;
		Place(scene.view.m_CreatureFPK, kBody, ACTION_ATTACK, 3, 2, 1, 5, 6);
		scene.Draw(monster);
		scene.ColorSet(40, 40, scene.Body(3), 5);
		CHECK(scene.Same());
	}
}

TEST(TopView4Creature, AHallucinatingCreatureIsDrawnAsWhatItSeemsUnlessThePlayerIsAGhost)
{
	Scene scene;
	Wearer slayer;
	slayer.Become(kSlayer);
	slayer.SetCurrentDirection(2);
	slayer.Pose(ACTION_ATTACK, 0);
	slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
	Place(scene.view.m_AddonFPK, 1, ACTION_ATTACK, 2, 0, 1, 2, 3);
	slayer.m_bHallu = true;
	slayer.m_HalluCreatureType = kMonster;
	slayer.m_HalluCreatureFrameID = kSecondBody;
	slayer.m_HalluAction = ACTION_MAGIC;
	slayer.m_HalluFrame = 1;
	Place(scene.view.m_CreatureFPK, kSecondBody, ACTION_MAGIC, 2, 1, 2, 5, 6);
	scene.Draw(slayer);
	// Its body's frame of the seeming action; not its clothes.
	scene.Blit(5, 6, scene.Body(2), 30, 60);
	CHECK(scene.Same());
	// A ghost player sees through it: the slayer's coat, and no body.
	scene.player->Set(EFFECTSTATUS_GHOST);
	slayer.ClearScreenRect();
	scene.Clear();
	scene.Draw(slayer);
	scene.Blit(2, 3, scene.view.m_AddonSPK[1], 30, 60);
	CHECK(scene.Same());
	CHECK(SameRect(slayer.GetScreenRect(), 42, 53, 47, 54));
}

TEST(TopView4Creature, AnExceptionalTypeIsDrawnAsItsProcedureSays)
{
	// 740 (a claymore) stands facing 1 on frame 0, whatever it does.
	Scene scene;
	scene.Monster(740);
	Wearer mine;
	mine.Become(740);
	mine.SetCurrentDirection(5);
	mine.Pose(ACTION_ATTACK, 2);
	Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 1, 0, 1, 5, 6);
	Place(scene.view.m_CreatureFPK, kBody, ACTION_ATTACK, 5, 2, 2, 5, 6);
	scene.Draw(mine);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
}

TEST(TopView4Creature, DeadGildreTurnsToDustOnceItsDeathIsDone)
{
	for (int type : {717, 721})
	{
		Scene scene;
		scene.Monster(type);
		Wearer gildre;
		gildre.Become(type);
		gildre.SetCurrentDirection(2);
		Place(scene.view.m_CreatureFPK, kBody, ACTION_DIE, 2, 0, 1, 5, 6);
		gildre.m_bAlive = false;
		gildre.Finish(ACTION_DIE, 4);
		gildre.ClearScreenRect();
		scene.Draw(gildre);
		scene.Blit(-5, 0, scene.view.m_EtcSPK[SPRITEID_MONSTER_DUST]);
		CHECK(scene.Same());
		// The dust's rect is the tile's.
		CHECK(SameRect(gildre.GetScreenRect(), 40, 50, 40 + TILE_X, 50 + TILE_Y));
		// Still dying, its body.
		gildre.Pose(ACTION_DIE, 0);
		scene.Clear();
		scene.Draw(gildre);
		scene.Blit(5, 6, scene.Body(1), 30, 60);
		CHECK(scene.Same());
	}
}

TEST(TopView4Creature, AGoreGrandDiesFacingSouthAndIsGoneWhenDone)
{
	Scene scene;
	scene.Monster(753);
	Wearer summon;
	summon.Become(753);
	summon.SetCurrentDirection(5);
	summon.m_bAlive = false;
	summon.Pose(ACTION_STAND, 1);
	Place(scene.view.m_CreatureFPK, kBody, ACTION_DIE, 2, 1, 1, 5, 6);
	scene.Draw(summon);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
	summon.Finish(ACTION_STAND, 2);
	scene.Clear();
	scene.Draw(summon);
	CHECK(scene.drawn.Blank());
	// 769 dead is not drawn at all.
	scene.Monster(769);
	Wearer other;
	other.Become(769);
	other.SetCurrentDirection(3);
	Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
	scene.Draw(other);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
	other.m_bAlive = false;
	scene.Clear();
	scene.Draw(other);
	CHECK(scene.drawn.Blank());
}

TEST(TopView4Creature, ARocketLauncherBobsOnItsFrameAndMovesTheCallersPoint)
{
	// Facing 0 or 4 it bobs up, otherwise left, by 0, 2, 4, 2 on frames 0
	// to 3, drawn as frame 0 of moving.
	struct Case { int direction, frame, dx, dy; };
	for (const Case& c : {Case{0, 1, 0, -2}, Case{4, 2, 0, -4}, Case{2, 3, -2, 0}, Case{6, 0, 0, 0}, Case{1, 2, -4, 0}})
	{
		Scene scene;
		scene.Monster(CREATURETYPE_ROCKET_LUNCHER);
		Wearer rocket;
		rocket.Become(CREATURETYPE_ROCKET_LUNCHER);
		rocket.SetCurrentDirection(BYTE(c.direction));
		rocket.Pose(ACTION_ATTACK, c.frame);
		Place(scene.view.m_CreatureFPK, kBody, ACTION_MOVE, c.direction, 0, 1, 5, 6);
		scene.Draw(rocket);
		scene.Blit(5 + c.dx, 6 + c.dy, scene.Body(1), 30, 60);
		CHECK(scene.Same());
		// Recorded defect: the bob moves the caller's point (and, for a
		// creature of more bodies, again for each).
		CHECK_EQ(scene.at.x + c.dx, scene.moved.x);
		CHECK_EQ(scene.at.y + c.dy, scene.moved.y);
	}
	// Frames past 3 index past the four bobs: not drawn here (undefined).
}

TEST(TopView4Creature, ARocketLauncherOfTwoBodiesBobsTwice)
{
	// Recorded defect: the shift is made to *pPoint inside the per-body
	// loop, so a second body is shifted again.
	Scene scene;
	scene.Type(CREATURETYPE_ROCKET_LUNCHER, CREATURETRIBE_VAMPIRE, {3, 5});
	Wearer rocket;
	rocket.Become(CREATURETYPE_ROCKET_LUNCHER);
	rocket.SetCurrentDirection(2);
	rocket.Pose(ACTION_ATTACK, 1);
	Place(scene.view.m_CreatureFPK, kBody, ACTION_MOVE, 2, 0, 1, 5, 6);
	Place(scene.view.m_CreatureFPK, kSecondBody, ACTION_MOVE, 2, 0, 2, 5, 30);
	scene.Draw(rocket);
	scene.Blit(3, 6, scene.Body(1), 30, 60);
	scene.Blit(1, 30, scene.Body(2), 30, 60);
	CHECK(scene.Same());
	CHECK_EQ(scene.at.x - 4, scene.moved.x);
}

TEST(TopView4Creature, SettingAfireAnAttackLoopsItsFlameFrames)
{
	// Another creature by its special action, the player by its used one:
	// an attack becomes frame 3 + f % 4 of the attack.
	for (int action : {ACTION_ATTACK, ACTION_VAMPIRE_ATTACK_SLOW, ACTION_VAMPIRE_ATTACK_FAST, ACTION_MAGIC})
		for (bool isPlayer : {false, true})
		{
			Scene scene;
			Wearer monster;
			monster.Become(kMonster);
			monster.SetCurrentDirection(3);
			monster.Pose(action, 6);
			if (isPlayer)
			{
				monster.SetID(scene.player->GetID());
				monster.m_nUsedActionInfo = SKILL_SET_AFIRE;
			}
			else monster.m_nSpecialActionInfo = SKILL_SET_AFIRE;
			const bool afire = action != ACTION_MAGIC;
			Place(scene.view.m_CreatureFPK, kBody, afire ? ACTION_ATTACK : action, 3, afire ? 3 + 6 % 4 : 6, 1, 5, 6);
			scene.Draw(monster);
			scene.Blit(5, 6, scene.Body(1), 30, 60);
			const bool same = scene.Same();
			if (!same) std::printf("      action %d, %s\n", action, isPlayer ? "the player" : "another");
			CHECK(same);
			// The other action info is not read.
			if (isPlayer)
			{
				monster.m_nUsedActionInfo = ACTIONINFO_NULL;
				monster.m_nSpecialActionInfo = SKILL_SET_AFIRE;
			}
			else
			{
				monster.m_nSpecialActionInfo = ACTIONINFO_NULL;
				monster.m_nUsedActionInfo = SKILL_SET_AFIRE;
			}
			Place(scene.view.m_CreatureFPK, kBody, action, 3, 6, 2, 5, 6);
			if (afire) Place(scene.view.m_CreatureFPK, kBody, ACTION_ATTACK, 3, 6, 2, 5, 6);
			scene.Clear();
			scene.Draw(monster);
			scene.Blit(5, 6, scene.Body(2), 30, 60);
			CHECK(scene.Same());
		}
}

//----------------------------------------------------------------------
// The dead turned to dust, and the hidden
//----------------------------------------------------------------------
TEST(TopView4Dust, ADeadPlayerVampireIsDustOnceItsDeathIsDone)
{
	Scene scene;
	Wearer vampire;
	vampire.Become(kVampire);
	vampire.SetCurrentDirection(2);
	Place(scene.view.m_CreatureFPK, kPlayerBody, ACTION_DIE, 2, 3, 1, 5, 6);
	vampire.m_bAlive = false;
	vampire.Finish(ACTION_DIE, 4);
	vampire.ClearScreenRect();
	scene.Draw(vampire);
	scene.Blit(-5, 0, scene.view.m_EtcSPK[SPRITEID_VAMPIRE_DUST]);
	CHECK(scene.Same());
	CHECK(SameRect(vampire.GetScreenRect(), 40, 50, 40 + TILE_X, 50 + TILE_Y));
	// Still dying, or alive, its body.
	vampire.m_ActionCount = 3;
	scene.Clear();
	scene.Draw(vampire);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
	vampire.Finish(ACTION_DIE, 4);
	vampire.m_bAlive = true;
	scene.Clear();
	scene.Draw(vampire);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
}

TEST(TopView4Dust, WithoutGoreTheDeadPastHalfwayAreDustOrACross)
{
	// A slayer leaves a cross, a monster dust; a player vampire dust of its
	// own.
	struct Case { int type; int sprite; int dx, dy; };
	for (const Case& c : {Case{kSlayer, SPRITEID_SLAYER_CROSS, 15, -30}, Case{kMonster, SPRITEID_MONSTER_DUST, -5, 0},
		Case{kVampire, SPRITEID_VAMPIRE_DUST, -5, 0}})
	{
		Scene scene;
		g_pUserInformation->GoreLevel = false;
		Wearer dead;
		dead.Become(c.type);
		dead.m_bAlive = false;
		dead.m_Action = ACTION_DIE;
		dead.m_ActionCountMax = 4;
		dead.m_ActionCount = 2;
		scene.Draw(dead);
		scene.Blit(c.dx, c.dy, scene.view.m_EtcSPK[WORD(c.sprite)]);
		CHECK(scene.Same());
	}
}

TEST(TopView4Dust, WithoutGoreTheDeadBeforeHalfwayShowTheirFirstFrame)
{
	Scene scene;
	g_pUserInformation->GoreLevel = false;
	Wearer dead;
	dead.Become(kMonster);
	dead.SetCurrentDirection(3);
	dead.m_bAlive = false;
	dead.m_Action = ACTION_DIE;
	dead.m_ActionCountMax = 4;
	dead.m_ActionCount = 1;
	Place(scene.view.m_CreatureFPK, kBody, ACTION_DIE, 3, 1, 1, 5, 6);
	scene.Draw(dead);
	scene.Blit(40, 40, scene.Body(3), 30, 60);
	CHECK(scene.Same());
	// With gore, the frame it is on.
	g_pUserInformation->GoreLevel = true;
	scene.Clear();
	scene.Draw(dead);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
}

TEST(TopView4Dust, WhichTypesTurnToDust)
{
	// Without gore every type does but the holy relics, the trees and the
	// like; with it only 377 to 386 and 480; never 431 to 436.
	struct Case { int type; bool gore; bool dust; };
	for (const Case& c : {Case{377, true, true}, Case{386, true, true}, Case{480, true, true}, Case{376, true, false},
		Case{387, true, false}, Case{430, false, true}, Case{431, false, false}, Case{436, false, false}, Case{437, false, true},
		Case{526, false, false}, Case{549, false, false}, Case{550, false, true}, Case{371, false, false}, Case{560, false, false},
		Case{563, false, false}, Case{482, false, false}, Case{650, false, false}, Case{670, false, false}, Case{672, false, false},
		Case{673, false, false}, Case{730, false, false}, Case{731, false, false}, Case{732, false, false}, Case{636, false, false},
		Case{377, false, true}, Case{440, true, false}})
	{
		Scene scene;
		g_pUserInformation->GoreLevel = c.gore;
		scene.Monster(c.type);
		Wearer dead;
		dead.Become(c.type);
		dead.m_bAlive = false;
		dead.m_Action = ACTION_DIE;
		dead.m_ActionCountMax = 4;
		dead.m_ActionCount = 2;
		scene.Draw(dead);
		const std::vector<WORD> picture = scene.drawn.Picture();
		scene.Blit(-5, 0, scene.view.m_EtcSPK[SPRITEID_MONSTER_DUST]);
		const bool dust = picture == scene.expected.Picture();
		if (dust != c.dust) std::printf("      type %d, gore %d\n", c.type, int(c.gore));
		CHECK_EQ(c.dust, dust);
	}
}

TEST(TopView4Dust, SelectedDustIsOutlinedAndNamedButNotDrawn)
{
	// The outline blit draws nothing on SDL; the dust goes to the outlines,
	// which it never clears (recorded defect: the next selection adds to it).
	for (bool byItem : {false, true})
		for (bool levelName : {false, true})
		{
			Scene scene;
			scene.levelNames.Init(2);
			g_pUserInformation->GoreLevel = false;
			Wearer dead;
			dead.Become(kMonster);
			if (levelName) dead.m_NameSelection.SelectLevelName(scene.levelNames, 1);
			CHECK_EQ(levelName, dead.HasLevelName() != FALSE);
			dead.m_bAlive = false;
			dead.m_Action = ACTION_DIE;
			dead.m_ActionCountMax = 4;
			dead.m_ActionCount = 2;
			if (byItem) scene.view.m_SelectItemID = kCreatureID;
			else scene.view.m_SelectCreatureID = kCreatureID;
			scene.at.x = -3;
			scene.at.y = 45;
			dead.ClearScreenRect();
			scene.Draw(dead);
			CHECK(scene.drawn.Blank());
			CHECK_EQ(scene.view.m_ColorOutlineItem, scene.view.m_SOMOutlineColor);
			const std::vector<OutlineEntry>& outlines = scene.view.m_SOM.GetOutlines();
			CHECK_EQ(size_t(1), outlines.size());
			if (outlines.size() == 1)
			{
				CHECK_EQ(-8, outlines[0].x);
				CHECK_EQ(45, outlines[0].y);
				CHECK(outlines[0].pSprite == &scene.view.m_EtcSPK[SPRITEID_MONSTER_DUST]);
			}
			// The name above it: x at least 0, a font height up (two with a
			// title), and never above that.
			CHECK_EQ(0, int(scene.view.m_pointChatString.x));
			CHECK_EQ(levelName ? 40 : 25, int(scene.view.m_pointChatString.y));
			CHECK(scene.view.m_pSelectedCreature == &dead);
			CHECK(SameRect(dead.GetScreenRect(), -3, 45, -3 + TILE_X, 45 + TILE_Y));
			// Higher up the name is clamped.
			scene.at.y = 10;
			scene.at.x = 30;
			scene.Draw(dead);
			CHECK_EQ(30, int(scene.view.m_pointChatString.x));
			CHECK_EQ(levelName ? 40 : 20, int(scene.view.m_pointChatString.y));
			CHECK_EQ(size_t(2), scene.view.m_SOM.GetOutlines().size());
		}
}

TEST(TopView4Hidden, ASlayerDoesNotSeeIntoDarkness)
{
	Scene scene;
	scene.Type(10, CREATURETRIBE_SLAYER, {3});
	Wearer monster;
	monster.Become(10);
	monster.SetCurrentDirection(3);
	monster.m_DarknessCount = 0;
	Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
	scene.Draw(monster);
	CHECK(scene.drawn.Blank());
	// A vampire does, as do Lightness and a ghost; zone 3001 shows it.
	scene.player->SetCreatureType(kVampire);
	scene.Draw(monster);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
	scene.player->SetCreatureType(kSlayer);
	for (EFFECTSTATUS status : {EFFECTSTATUS_LIGHTNESS, EFFECTSTATUS_GHOST})
	{
		scene.player->Set(status);
		scene.Clear();
		scene.Draw(monster);
		scene.Blit(5, 6, scene.Body(1), 30, 60);
		CHECK(scene.Same());
		scene.player->Set(status, false);
	}
	scene.zone->m_Info.ZoneID = 3001;
	scene.Clear();
	scene.Draw(monster);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
	scene.zone->m_Info.ZoneID = 1;
	// Out of the darkness it is drawn; an NPC in it is too.
	monster.m_DarknessCount = -1;
	scene.Clear();
	scene.Draw(monster);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
	Wearer npc;
	npc.Become(kNPC);
	npc.SetCurrentDirection(3);
	npc.m_DarknessCount = 0;
	scene.Clear();
	scene.Draw(npc);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
}

TEST(TopView4Hidden, ThePlayerSeesItselfInDarkness)
{
	Scene scene;
	scene.Type(10, CREATURETRIBE_SLAYER, {3});
	scene.player->SetCreatureType(10);
	scene.player->SetBodyColor1(30);
	scene.player->SetBodyColor2(60);
	scene.player->SetCurrentDirection(3);
	scene.player->m_DarknessCount = 0;
	Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
	scene.Draw(*scene.player);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
}

TEST(TopView4Hidden, SomeTypesAreSeenInDarkness)
{
	// Their procedures turn them south (2).
	struct Case { int type; bool seen; };
	for (const Case& c : {Case{526, true}, Case{549, true}, Case{371, true}, Case{376, true}, Case{560, true}, Case{563, true},
		Case{670, true}, Case{673, true}, Case{669, false}, Case{650, false}, Case{482, false}})
	{
		Scene scene;
		scene.Type(c.type, CREATURETRIBE_SLAYER, {3});
		Wearer monster;
		monster.Become(c.type);
		monster.SetCurrentDirection(5);
		monster.m_DarknessCount = 0;
		Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 2, 0, 1, 5, 6);
		scene.Draw(monster);
		const bool seen = !scene.drawn.Blank();
		if (seen != c.seen) std::printf("      type %d\n", c.type);
		CHECK_EQ(c.seen, seen);
	}
}

TEST(TopView4Hidden, AnUndergroundCreatureIsItsBurrow)
{
	// A creature underground, or a pet (the fake move types are a pet's).
	for (bool pet : {false, true})
	{
		Scene scene;
		Pet fake;
		Wearer monster;
		MCreature* drawnCreature = pet ? static_cast<MCreature*>(&fake) : &monster;
		if (pet)
		{
			fake.Become(kMonster);
			fake.SetCurrentDirection(3);
			fake.m_MoveType = MCreature::CREATURE_FAKE_UNDERGROUND;
		}
		else
		{
			monster.Become(kMonster);
			monster.SetCurrentDirection(3);
			monster.m_MoveType = MCreature::CREATURE_UNDERGROUND;
		}
		Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
		drawnCreature->ClearScreenRect();
		scene.Draw(*drawnCreature);
		CSprite& burrow = scene.view.m_EtcSPK[SPRITEID_CREATURE_BURROW];
		scene.Blit(4, 4, burrow);
		CHECK(scene.Same());
		CHECK(SameRect(drawnCreature->GetScreenRect(), 44, 54, 44 + burrow.GetWidth(), 54 + burrow.GetHeight()));
	}
	// Gilles de Rais' lair ghost is underground but drawn as itself.
	Scene scene;
	scene.Monster(CREATURETYPE_GHOST);
	Wearer ghost;
	ghost.Become(CREATURETYPE_GHOST);
	ghost.SetCurrentDirection(3);
	ghost.m_MoveType = MCreature::CREATURE_UNDERGROUND;
	Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
	scene.Draw(ghost);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
}

//----------------------------------------------------------------------
// Frames, heights and silhouettes
//----------------------------------------------------------------------
TEST(TopView4Creature, AHeadlessCorpseShowsTheFrameAfterItsLast)
{
	Scene scene;
	testfw::MutableRow(scene.creatureTypes, kMonster).SetActionCount(ACTION_DIE, 3);
	Wearer dead;
	dead.Become(kMonster);
	dead.SetCurrentDirection(3);
	dead.m_bAlive = false;
	dead.Finish(ACTION_DIE, 5);
	Frames(scene.view.m_CreatureFPK, kBody, ACTION_DIE, 3, {{0, 70, 0}, {0, 70, 0}, {0, 70, 0}, {1, 5, 6}, {2, 5, 6}});
	// With its head, the last frame.
	scene.Draw(dead);
	scene.Blit(5, 6, scene.Body(2), 30, 60);
	CHECK(scene.Same());
	dead.m_bHasHead = false;
	scene.Clear();
	scene.Draw(dead);
	scene.Blit(5, 6, scene.Body(1), 30, 60);
	CHECK(scene.Same());
	// Only once the death is done.
	dead.m_ActionCount = 4;
	scene.Clear();
	scene.Draw(dead);
	scene.Blit(5, 6, scene.Body(2), 30, 60);
	CHECK(scene.Same());
}

TEST(TopView4Creature, ACreatureStandsItsHeightAboveTheCallersPoint)
{
	Scene scene;
	Wearer monster;
	monster.Become(kMonster);
	monster.SetCurrentDirection(3);
	monster.m_Z = 7;
	Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
	scene.Draw(monster);
	scene.Blit(5, -1, scene.Body(1), 30, 60);
	CHECK(scene.Same());
	CHECK_EQ(scene.at.y - 7, scene.moved.y);
}

TEST(TopView4Creature, DivineGuidanceDrawsGameviewsSilhouettesBeforeTheBody)
{
	for (int type : {kMonster, kSlayer, kNPC})
	{
		Scene scene;
		Wearer creature;
		creature.Become(type);
		creature.SetCurrentDirection(3);
		creature.Set(EFFECTSTATUS_DIVINE_GUIDANCE);
		creature.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
		for (int direction = 0; direction < 8; ++direction)
		{
			Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, direction, 0, direction % 4, 5 + direction, 6 + 3 * direction);
			Place(scene.view.m_AddonFPK, 1, ACTION_STAND, direction, 0, direction % 4, 20 + direction, 3 * direction);
			// The silhouettes are frame 30 of being drained, risen by the
			// type's height less a bob; facing 3 one lies on the body.
			const int sy = direction == 3 ? 36 : 50 + direction;
			Place(scene.view.m_CreatureFPK, kBody, ACTION_DRAINED, direction, 30, (direction + 1) % 4, direction == 3 ? 8 : 12, sy);
			Place(scene.view.m_AddonFPK, 1, ACTION_DRAINED, direction, 30, (direction + 1) % 4, direction == 3 ? 23 : 12, sy - 6);
		}
		scene.Draw(creature);
		if (type != kNPC)
		{
			CreatureDrawPacks packs = Access::Packs(scene.view);
			packs.pSurface = &scene.expected.surface;
			POINT point = scene.at;
			scene.expected.surface.Lock();
			DrawDivineGuidanceCharacter(packs, &point, &creature, 3, type == kSlayer ? FRAMEID_NULL : kBody, type == kSlayer, false);
			scene.expected.surface.Unlock();
		}
		if (type == kSlayer) scene.Blit(23, 9, scene.view.m_AddonSPK[3], 30, 60);
		else scene.Blit(8, 15, scene.Body(3), 30, 60);
		const bool same = scene.Same();
		if (!same) std::printf("      type %d\n", type);
		CHECK(same);
		// The silhouettes put more than the body's five pixels on it.
		int lit = 0;
		for (WORD pixel : scene.drawn.Picture()) lit += pixel != 0;
		CHECK_EQ(type == kNPC, lit == 5);
	}
}

//----------------------------------------------------------------------
// The creature under the cursor
//----------------------------------------------------------------------
TEST(TopView4Selected, TheSelectedCreatureIsDrawnFromItsOutlinesAndNamed)
{
	for (bool levelName : {false, true})
	{
		Scene scene;
		scene.levelNames.Init(2);
		Wearer monster;
		monster.Become(kMonster);
		monster.SetCurrentDirection(3);
		if (levelName) monster.m_NameSelection.SelectLevelName(scene.levelNames, 1);
		Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
		scene.view.m_SelectCreatureID = kCreatureID;
		monster.ClearScreenRect();
		scene.Draw(monster);
		scene.Blit(5, 6, scene.Body(1), 30, 60);
		CHECK(scene.Same());
		CHECK(SameRect(monster.GetScreenRect(), 45, 56, 50, 57));
		CHECK(scene.view.m_SOM.GetOutlines().empty());
		CHECK_EQ(scene.view.m_ColorOutlineAttackImpossible, scene.view.m_SOMOutlineColor);
		CHECK(scene.view.m_pSelectedCreature == &monster);
		// Its name goes its height above it: x at least 0, y at least a
		// font height (two with a title).
		CHECK_EQ(40, int(scene.view.m_pointChatString.x));
		CHECK_EQ(levelName ? 40 : 20, int(scene.view.m_pointChatString.y));
		scene.at.x = -3;
		scene.at.y = 45;
		scene.Draw(monster);
		CHECK_EQ(0, int(scene.view.m_pointChatString.x));
		CHECK_EQ(levelName ? 40 : 20, int(scene.view.m_pointChatString.y));
		scene.at.x = 30;
		scene.at.y = 75;
		scene.Draw(monster);
		CHECK_EQ(30, int(scene.view.m_pointChatString.x));
		CHECK_EQ(45, int(scene.view.m_pointChatString.y));
	}
}

TEST(TopView4Selected, AnUnselectedCreatureLeavesTheSelectionAlone)
{
	Scene scene;
	Wearer monster;
	monster.Become(kMonster);
	monster.SetCurrentDirection(3);
	Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
	scene.view.m_SelectCreatureID = kCreatureID + 1;
	scene.view.m_SOMOutlineColor = 0x0909;
	scene.Draw(monster);
	CHECK_EQ(0x0909, int(scene.view.m_SOMOutlineColor));
	CHECK_EQ(-1, int(scene.view.m_pointChatString.x));
	CHECK_EQ(-1, int(scene.view.m_pointChatString.y));
	CHECK(scene.view.m_pSelectedCreature == nullptr);
}

namespace {

const MObjectSelectorHost forcedAttack = []() {
	MObjectSelectorHost host{};
	host.ForceAttack = []() { return true; };
	host.IsNonPK = []() { return false; };
	return host;
}();

// The outline colour of the selected creature as drawn.
WORD OutlineOf(Scene& scene, MCreature& creature)
{
	scene.view.m_SelectCreatureID = creature.GetID();
	scene.view.m_SOMOutlineColor = 0;
	scene.Draw(creature);
	return scene.view.m_SOMOutlineColor;
}

} // namespace

TEST(TopView4Selected, TheOutlineSaysWhetherItCanBeAttacked)
{
	Scene scene;
	MTopView& v = scene.view;
	Wearer monster;
	monster.Become(kMonster);
	CHECK_EQ(v.m_ColorOutlineAttackImpossible, OutlineOf(scene, monster));
	MObjectSelector forced(&forcedAttack);
	g_pObjectSelector = &forced;
	CHECK_EQ(v.m_ColorOutlineAttackPossible, OutlineOf(scene, monster));
	g_pObjectSelector = &scene.selector;
	// An NPC is an NPC's colour, but Bathory (217), who can always be
	// attacked.
	Wearer npc;
	npc.Become(kNPC);
	CHECK_EQ(v.m_ColorOutlineNPC, OutlineOf(scene, npc));
	scene.Type(217, CREATURETRIBE_NPC, {3});
	npc.Become(217);
	CHECK_EQ(v.m_ColorOutlineAttackPossible, OutlineOf(scene, npc));
	scene.Monster(217);
	npc.Become(217);
	CHECK_EQ(v.m_ColorOutlineAttackPossible, OutlineOf(scene, npc));
}

TEST(TopView4Selected, InRequestModeAPlayerIsAnNPCsColour)
{
	Scene scene;
	MTopView& v = scene.view;
	Wearer vampire, monster;
	vampire.Become(kVampire);
	monster.Become(kMonster);
	v.SetRequestMode(MRequestMode::REQUEST_TRADE);
	CHECK_EQ(v.m_ColorOutlineNPC, OutlineOf(scene, vampire));
	CHECK_EQ(v.m_ColorOutlineAttackImpossible, OutlineOf(scene, monster));
	v.UnSetRequestMode();
	CHECK_EQ(v.m_ColorOutlineAttackImpossible, OutlineOf(scene, vampire));
	// The player asking the creature it follows.
	scene.player->SetRequestMode(MRequestMode::REQUEST_PARTY);
	scene.player->m_fTrace = FLAG_TRACE_CREATURE_BASIC;
	scene.player->m_TraceID = kCreatureID;
	CHECK_EQ(v.m_ColorOutlineNPC, OutlineOf(scene, monster));
	scene.player->m_TraceID = kCreatureID + 1;
	CHECK_EQ(v.m_ColorOutlineAttackImpossible, OutlineOf(scene, monster));
	scene.player->m_TraceID = kCreatureID;
	scene.player->m_fTrace = 0;
	CHECK_EQ(v.m_ColorOutlineAttackImpossible, OutlineOf(scene, monster));
	scene.player->m_fTrace = FLAG_TRACE_CREATURE_BASIC;
	scene.player->UnSetRequestMode();
	CHECK_EQ(v.m_ColorOutlineAttackImpossible, OutlineOf(scene, monster));
	// Not while the view is asking.
	scene.player->SetRequestMode(MRequestMode::REQUEST_PARTY);
	v.SetRequestMode(MRequestMode::REQUEST_TRADE);
	CHECK_EQ(v.m_ColorOutlineAttackImpossible, OutlineOf(scene, monster));
}

TEST(TopView4Selected, ASelectedSlayersPartsAreAllDrawnInTheLastColoursSet)
{
	// Recorded defect: the re-blit of the outlines (MTopViewDraw.cpp
	// 796-805) draws every part in the colour sets the last part set, as an
	// entry keeps none of its own; the coat shows in the helm's colours.
	Scene scene;
	Wearer slayer;
	slayer.Become(kSlayer);
	slayer.SetCurrentDirection(2);
	slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
	slayer.Wear(ADDON_HELM, 2, ITEM_CLASS_HELM, 40, 50);
	Place(scene.view.m_AddonFPK, 1, ACTION_STAND, 2, 0, 1, 2, 3);
	Place(scene.view.m_AddonFPK, 2, ACTION_STAND, 2, 0, 2, 14, 20);
	scene.view.m_SelectCreatureID = kCreatureID;
	RECT wide = { 0, 0, 100, 100 };
	slayer.SetScreenRect(&wide);
	scene.Draw(slayer);
	scene.Blit(2, 3, scene.view.m_AddonSPK[1], 40, 50);
	scene.Blit(14, 20, scene.view.m_AddonSPK[2], 40, 50);
	CHECK(scene.Same());
	// No body: the rect is the parts', cleared first.
	CHECK(SameRect(slayer.GetScreenRect(), 42, 53, 59, 71));
	CHECK(scene.view.m_SOM.GetOutlines().empty());
}

TEST(TopView4Selected, AnInvisibleOrFakeDyingSelectedCreatureIsDrawnWhole)
{
	// Recorded defect: the outline-only blit draws nothing on SDL, but the
	// re-blit before it draws the parts plainly, so selecting an invisible
	// creature shows it.
	for (int state : {0, 1})
	{
		Scene scene;
		Wearer monster;
		monster.Become(kMonster);
		monster.SetCurrentDirection(3);
		if (state == 0) monster.m_InvisibleCount = 32;
		else monster.m_bFakeDie = true;
		Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
		scene.view.m_SelectCreatureID = kCreatureID;
		scene.Draw(monster);
		scene.Blit(5, 6, scene.Body(1), 30, 60);
		CHECK(scene.Same());
	}
}

TEST(TopView4Selected, AGateIsOutlinedWhileAnotherGateIsSelected)
{
	// The four castle gates (726-729), each drawn standing facing 1 on
	// frame 4 by its procedure, take the outline path but are not named.
	for (int gate : {726, 727, 728, 729})
	{
		Scene scene;
		for (int type = 726; type <= 729; ++type) scene.Monster(type);
		Wearer selected;
		selected.Become(727);
		selected.SetID(300);
		scene.zone->m_mapCreature[300] = &selected;
		scene.view.m_SelectCreatureID = 300;
		Wearer other;
		other.Become(gate);
		Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 1, 4, 1, 5, 6);
		scene.Draw(other);
		scene.Blit(5, 6, scene.Body(1), 30, 60);
		CHECK(scene.Same());
		CHECK_EQ(scene.view.m_ColorOutlineAttackImpossible, scene.view.m_SOMOutlineColor);
		CHECK_EQ(-1, int(scene.view.m_pointChatString.y));
		// Another creature selected: not outlined.
		selected.Become(kMonster);
		selected.SetID(300);
		scene.view.m_SOMOutlineColor = 0;
		scene.Draw(other);
		CHECK_EQ(0, int(scene.view.m_SOMOutlineColor));
		scene.zone->m_mapCreature.erase(300);
		// Nothing selected: not outlined either.
		scene.Draw(other);
		CHECK_EQ(0, int(scene.view.m_SOMOutlineColor));
	}
}

//----------------------------------------------------------------------
// Which drawing a creature goes to
//----------------------------------------------------------------------
TEST(TopView4Dispatch, WithoutABodyTheRectIsClearedAndThePartsSetIt)
{
	for (bool selected : {false, true})
	{
		Scene scene;
		Wearer slayer;
		slayer.Become(kSlayer);
		slayer.SetCurrentDirection(2);
		slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
		Place(scene.view.m_AddonFPK, 1, ACTION_STAND, 2, 0, 1, 2, 3);
		if (selected) scene.view.m_SelectCreatureID = kCreatureID;
		RECT wide = { 0, 0, 100, 100 };
		slayer.SetScreenRect(&wide);
		scene.Draw(slayer);
		CHECK(SameRect(slayer.GetScreenRect(), 42, 53, 47, 54));
		// Wearing nothing drawable, it is left cleared.
		slayer.m_Addon[ADDON_COAT].bAddon = FALSE;
		slayer.SetScreenRect(&wide);
		scene.Draw(slayer);
		CHECK(Cleared(slayer.GetScreenRect()));
	}
}

TEST(TopView4Dispatch, AnAdvancedVampireTakesItsGendersPackButNotAsABatOrAWolf)
{
	for (bool male : {true, false})
		for (bool selected : {false, true})
		{
			Scene scene;
			Wearer vampire;
			vampire.Become(kVampire);
			vampire.Advance();
			vampire.m_bMale = male;
			vampire.SetCurrentDirection(3);
			CCreatureFramePack& pack = male ? scene.view.m_AdvancementVampireManFPK : scene.view.m_AdvancementVampireWomanFPK;
			CIndexSpritePack& sprites = male ? scene.view.m_AdvancementVampireManSPK : scene.view.m_AdvancementVampireWomanSPK;
			Place(pack, 0, ACTION_ADVANCEMENT_STOP - ADVANCEMENT_ACTION_START, 3, 0, 1, 5, 6);
			Place(pack, 1, ACTION_ADVANCEMENT_STOP - ADVANCEMENT_ACTION_START, 3, 0, 2, 25, 6);
			Place(scene.view.m_CreatureFPK, kPlayerBody, ACTION_STAND, 3, 0, 3, 50, 6);
			if (selected) scene.view.m_SelectCreatureID = kCreatureID;
			scene.Draw(vampire);
			scene.Blit(5, 6, sprites[1], 30, 60);
			scene.Blit(25, 6, sprites[2], 30, 60);
			const bool same = scene.Same();
			if (!same) std::printf("      %s, %s\n", male ? "male" : "female", selected ? "selected" : "not selected");
			CHECK(same);
		}
	// A bat, a wolf and a werwolf are drawn from the creature pack.
	for (int type : {CREATURETYPE_BAT, CREATURETYPE_WOLF, CREATURETYPE_WER_WOLF})
	{
		Scene scene;
		scene.Monster(type);
		Wearer beast;
		beast.Become(type);
		beast.Advance();
		CHECK(beast.IsAdvancementClass());
		beast.SetCurrentDirection(3);
		Place(scene.view.m_AdvancementVampireManFPK, 0, 0, 3, 0, 1, 5, 6);
		Place(scene.view.m_AdvancementVampireWomanFPK, 0, 0, 3, 0, 1, 5, 6);
		Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 3, 50, 6);
		scene.Draw(beast);
		scene.Blit(50, 6, scene.Body(3), 30, 60);
		CHECK(scene.Same());
	}
}

TEST(TopView4Dispatch, ASlayersClothesComeFromItsPacks)
{
	// Plain from the addon pack, advanced from its gender's; selected or
	// not the picture is the same.
	for (bool advanced : {false, true})
		for (bool selected : {false, true})
		{
			Scene scene;
			Wearer slayer;
			slayer.Become(kSlayer);
			if (advanced) slayer.Advance();
			slayer.SetCurrentDirection(2);
			slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
			const int stop = ConvAdvancementSlayerActionFromSlayerAction(ACTION_STAND, &slayer) - ADVANCEMENT_ACTION_START;
			Place(scene.view.m_AddonFPK, 1, ACTION_STAND, 2, 0, 1, 2, 3);
			Place(scene.view.m_AdvancementSlayerManFPK, AC_BODY, stop, 2, 0, 2, 12, 3);
			if (selected) scene.view.m_SelectCreatureID = kCreatureID;
			scene.Draw(slayer);
			if (advanced) scene.Blit(12, 3, scene.view.m_AdvancementSlayerManSPK[2], 30, 60);
			else scene.Blit(2, 3, scene.view.m_AddonSPK[1], 30, 60);
			CHECK(scene.Same());
		}
}

TEST(TopView4Dispatch, AnOusterssClothesComeFromItsPacks)
{
	for (bool advanced : {false, true})
		for (bool selected : {false, true})
		{
			Scene scene;
			Wearer ousters;
			ousters.Become(kOusters);
			if (advanced) ousters.Advance();
			ousters.SetCurrentDirection(2);
			ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT, 30, 60);
			ousters.Wear(ADDON_TROUSER, 2, ITEM_CLASS_OUSTERS_BOOTS, 30, 70);
			Place(scene.view.m_OustersFPK, 1, ACTION_OUSTERS_STAND, 2, 0, 1, 2, 3);
			Place(scene.view.m_AdvancementOustersFPK, 1, ACTION_ADVANCEMENT_OUSTERS_WRISTLET_STOP - ADVANCEMENT_ACTION_START, 2, 0, 2, 12, 3);
			if (selected) scene.view.m_SelectCreatureID = kCreatureID;
			scene.Draw(ousters);
			CIndexSprite& sprite = advanced ? scene.view.m_AdvancementOustersSPK[2] : scene.view.m_OustersSPK[1];
			scene.Blit(advanced ? 12 : 2, 3, sprite, 30, 60, defaultCoatColor, 70);
			const bool same = scene.Same();
			if (!same) std::printf("      %s, %s\n", advanced ? "advanced" : "plain", selected ? "selected" : "not selected");
			CHECK(same);
		}
}

TEST(TopView4Dispatch, AnOperatorAndGildreAreNoOusters)
{
	for (int type : {kOusters, CREATURETYPE_OUSTERS_OPERATOR, 717, 721, 723})
	{
		Scene scene;
		scene.Type(type, CREATURETRIBE_OUSTERS, {2});
		Wearer ousters;
		ousters.Become(type);
		ousters.SetCurrentDirection(2);
		ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT, 30, 60);
		Place(scene.view.m_OustersFPK, 1, ACTION_OUSTERS_STAND, 2, 0, 1, 2, 3);
		scene.Draw(ousters);
		CHECK_EQ(type != kOusters, scene.drawn.Blank());
	}
}

TEST(TopView4Dispatch, ASlayerPetsTurretStandsOnItWhileItStandsOrMoves)
{
	for (int type : {702, 703, 704})
		for (int action : {ACTION_STAND, ACTION_MOVE, ACTION_ATTACK})
			for (bool selected : {false, true})
			{
				Scene scene;
				scene.Monster(type);
				Pet pet;
				pet.Become(type);
				pet.SetCurrentDirection(2);
				pet.Pose(action, 0);
				pet.SetTurreDirection(1);
				Place(scene.view.m_CreatureFPK, kBody, action, 2, 0, 1, 5, 6);
				Frames(scene.view.m_CreatureFPK, kBody, ACTION_VAMPIRE_DRAIN, 2, {{0, 70, 0}, {2, 30, 9}});
				if (selected) scene.view.m_SelectCreatureID = kCreatureID;
				scene.Draw(pet);
				scene.Blit(5, 6, scene.Body(1), 30, 60);
				if (action != ACTION_ATTACK)
				{
					CreatureDrawPacks packs = Access::Packs(scene.view);
					packs.pSurface = &scene.expected.surface;
					packs.selectedCreatureID = OBJECTID_NULL;
					POINT point = scene.at;
					scene.expected.surface.Lock();
					DrawCentauroTurret(packs, &point, &pet, action, 2, 0, kBody);
					scene.expected.surface.Unlock();
				}
				const bool same = scene.Same();
				if (!same) std::printf("      type %d, action %d%s\n", type, action, selected ? ", selected" : "");
				CHECK(same);
			}
}

TEST(TopView4Dispatch, AnInstallingTurretIsGameviewsSelectedOrNot)
{
	for (bool advanced : {false, true})
		for (bool selected : {false, true})
		{
			Scene scene;
			ShapePack(scene.view.m_CreatureFPK, 223, kActions);
			Wearer slayer;
			slayer.Become(kSlayer);
			if (advanced) slayer.Advance();
			slayer.SetCurrentDirection(5);
			slayer.Set(EFFECTSTATUS_INSTALL_TURRET);
			slayer.SetInstallTurretCount(2);
			slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
			Place(scene.view.m_AddonFPK, 1, ACTION_STAND, 5, 0, 1, 2, 3);
			Place(scene.view.m_CreatureFPK, 222, ACTION_STAND, 2, 2, 2, 6, 7);
			if (selected) scene.view.m_SelectCreatureID = kCreatureID;
			scene.Draw(slayer);
			// Its turret in the colours last set; no clothes.
			scene.Blit(6, 7, scene.Body(2), 1, 2);
			CHECK(scene.Same());
			CHECK(SameRect(slayer.GetScreenRect(), 46, 57, 51, 58));
		}
}

//----------------------------------------------------------------------
// A party member's bar and light, and a motorcycle's
//----------------------------------------------------------------------
namespace {

// Light filters 0 to 2, each one cell of its number plus one, and an
// empty 64 by 64 buffer.
void Lights(MTopView& view)
{
	view.m_LightFTP.Init(3);
	for (int id = 0; id < 3; ++id)
	{
		view.m_LightFTP[TYPE_FILTERID(id)].Init(1, 1);
		view.m_LightFTP[TYPE_FILTERID(id)].SetFilter(0, 0, BYTE(id + 1));
	}
	view.m_LightBufferFilter.Init(64, 64);
	for (int y = 0; y < 64; ++y)
		for (int x = 0; x < 64; ++x) view.m_LightBufferFilter.SetFilter(WORD(x), WORD(y), 0);
}

struct Lit { int x, y, value; };

// The lit cells of the buffer.
std::vector<Lit> LitCells(MTopView& view)
{
	std::vector<Lit> cells;
	for (int y = 0; y < 64; ++y)
		for (int x = 0; x < 64; ++x)
			if (int value = view.m_LightBufferFilter.GetFilter(WORD(y))[x]) cells.push_back({x, y, value});
	return cells;
}

bool SameLights(const std::vector<Lit>& lit, const std::vector<Lit>& expected)
{
	bool same = lit.size() == expected.size();
	for (size_t i = 0; same && i < lit.size(); ++i)
		same = lit[i].x == expected[i].x && lit[i].y == expected[i].y && lit[i].value == expected[i].value;
	if (!same)
	{
		std::printf("      lit:");
		for (const Lit& cell : lit) std::printf(" (%d,%d)=%d", cell.x, cell.y, cell.value);
		std::printf("\n");
	}
	return same;
}

// The light cell of a screen point.
Lit Cell(int x, int y, int value)
{
	return { int(float(x) / MTopView::s_LightWidth), int(float(y) / MTopView::s_LightHeight), value };
}

} // namespace

TEST(TopView4Party, APartyMemberShowsItsHPBarAndLightsItsSight)
{
	Scene scene;
	Lights(scene.view);
	// Placed so that a pixel more across, or the Z left out, moves the
	// light a cell (12.5 by 9.375 pixels).
	scene.view.m_FirstZonePixel.x = -10;
	scene.view.m_FirstZonePixel.y = 6;
	scene.player->m_LightSight = 1;
	Wearer member;
	member.Become(kMonster);
	member.SetCurrentDirection(3);
	member.MObject::SetPosition(5, 6);
	member.m_Z = 4;
	member.SetPlayerParty();
	Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
	scene.Draw(member);
	// The bar (DrawPartyHP's) its height (30) above the point less its Z.
	scene.Blit(5, 2, scene.Body(1), 30, 60);
	CSpriteSurface* pExpected = &scene.expected.surface;
	scene.view.SetSurface(pExpected);
	POINT bar = { scene.at.x, scene.at.y - 4 - 30 };
	scene.expected.surface.Lock();
	Access::PartyHP(scene.view, &bar, &member);
	scene.expected.surface.Unlock();
	CSpriteSurface* pDrawn = &scene.drawn.surface;
	scene.view.SetSurface(pDrawn);
	CHECK(scene.Same());
	// Its sight, as the player's, forced: from the tile's centre, raised.
	const int pX = member.GetPixelX() + 10 + 24, pY = member.GetPixelY() - 6 - TILE_Y;
	CHECK_EQ(21, Cell(pX, 0, 0).x);
	CHECK_EQ(22, Cell(pX + 1, 0, 0).x);
	CHECK_EQ(11, Cell(0, pY - 4, 0).y);
	CHECK_EQ(12, Cell(0, pY, 0).y);
	CHECK_EQ(1, scene.view.m_nLight);
	CHECK(SameLights(LitCells(scene.view), {Cell(pX, pY - 4, 2)}));
	// Riding, its headlight too, four tiles ahead.
	Lights(scene.view);
	scene.view.m_nLight = 0;
	member.m_MoveDevice = MCreature::MOVE_DEVICE_RIDE;
	scene.Clear();
	scene.Draw(member);
	const POINT ahead = MTopView::GetChangeValueToDirection(3);
	std::vector<Lit> lit = {Cell(pX, pY - 4, 2), Cell(pX + ahead.x * TILE_X * 4 + TILE_X_HALF, pY - 4 + ahead.y * TILE_Y * 4 + TILE_Y_HALF, 3)};
	std::sort(lit.begin(), lit.end(), [](const Lit& a, const Lit& b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });
	CHECK_EQ(2, scene.view.m_nLight);
	CHECK(SameLights(LitCells(scene.view), lit));
	// Not under a creature's poison.
	Lights(scene.view);
	scene.view.m_nLight = 0;
	member.Set(EFFECTSTATUS_YELLOW_POISON_TO_CREATURE);
	scene.Draw(member);
	CHECK_EQ(1, scene.view.m_nLight);
}

TEST(TopView4Party, AHallucinatingPlayerKnowsNoPartyAndThePlayerIsNoneOfIt)
{
	for (int kind : {0, 1})
	{
		Scene scene;
		Lights(scene.view);
		scene.player->m_LightSight = 1;
		Wearer member;
		member.Become(kMonster);
		member.SetCurrentDirection(3);
		member.SetPlayerParty();
		Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
		MCreature* drawn = &member;
		if (kind == 0) scene.player->Set(EFFECTSTATUS_HALLUCINATION);
		else
		{
			scene.Type(10, CREATURETRIBE_SLAYER, {3});
			scene.player->SetCreatureType(10);
			scene.player->SetCurrentDirection(3);
			scene.player->SetBodyColor1(30);
			scene.player->SetBodyColor2(60);
			scene.player->SetPlayerParty();
			drawn = scene.player;
		}
		scene.Draw(*drawn);
		scene.Blit(5, 6, scene.Body(1), 30, 60);
		CHECK(scene.Same());
		CHECK_EQ(0, scene.view.m_nLight);
		CHECK(LitCells(scene.view).empty());
	}
}

TEST(TopView4Party, AnotherRidersHeadlightShowsInTheDarkWithinThePlayersSight)
{
	struct Case { int sight; int darkBits; int lights; int filter; };
	for (const Case& c : {Case{4, 1, 1, 1}, Case{10, 2, 1, 2}, Case{3, 1, 0, 0}, Case{10, 0, 0, 0}})
	{
		Scene scene;
		Lights(scene.view);
		scene.view.m_DarkBits = BYTE(c.darkBits);
		scene.player->m_Sight = char(c.sight);
		Wearer rider;
		rider.Become(kMonster);
		rider.SetCurrentDirection(6);
		rider.MObject::SetPosition(5, 6);
		rider.m_MoveDevice = MCreature::MOVE_DEVICE_RIDE;
		scene.Draw(rider);
		CHECK_EQ(c.lights, scene.view.m_nLight);
		const POINT ahead = MTopView::GetChangeValueToDirection(6);
		const int pX = rider.GetPixelX() + TILE_X_HALF, pY = rider.GetPixelY() - TILE_Y;
		if (c.lights)
			CHECK(SameLights(LitCells(scene.view), {Cell(pX + ahead.x * TILE_X * 4 + TILE_X_HALF, pY + ahead.y * TILE_Y * 4 + TILE_Y_HALF, c.filter + 1)}));
		else
			CHECK(LitCells(scene.view).empty());
	}
	// Past the most lights drawn, none.
	Scene scene;
	Lights(scene.view);
	scene.view.m_DarkBits = 1;
	scene.player->m_Sight = 10;
	scene.view.m_nLight = g_pClientConfig->MAX_LIGHT_DRAW;
	Wearer rider;
	rider.Become(kMonster);
	rider.m_MoveDevice = MCreature::MOVE_DEVICE_RIDE;
	scene.Draw(rider);
	CHECK_EQ(g_pClientConfig->MAX_LIGHT_DRAW, scene.view.m_nLight);
	CHECK(LitCells(scene.view).empty());
	scene.view.m_nLight = g_pClientConfig->MAX_LIGHT_DRAW - 1;
	scene.Draw(rider);
	CHECK_EQ(g_pClientConfig->MAX_LIGHT_DRAW, scene.view.m_nLight);
}

//----------------------------------------------------------------------
// A slayer's clothes
//----------------------------------------------------------------------
namespace {

void DrawSlayer(Scene& scene, MCreature& slayer, int action, int direction, int frame, int index = 0)
{
	scene.Call([&](POINT* p) { Access::Slayer(scene.view, p, &slayer, action, direction, frame, index); });
}

} // namespace

TEST(TopView4Slayer, PartsAreLaidInTheOrderOfTheCreaturesFacingFromTheFramesOfTheOneGiven)
{
	// A shield and a sword on one spot: facing 2 the right hand is last,
	// facing 0 the left; the frames are the given direction's (4).
	for (int facing : {0, 2})
	{
		Scene scene;
		Wearer slayer;
		slayer.Become(kSlayer);
		slayer.SetCurrentDirection(BYTE(facing));
		slayer.Wear(ADDON_LEFTHAND, 1, ITEM_CLASS_SHIELD, 30, 60);
		slayer.Wear(ADDON_RIGHTHAND, 2, ITEM_CLASS_SWORD, 40, 50);
		Place(scene.view.m_AddonFPK, 1, ACTION_ATTACK, 4, 1, 1, 2, 3);
		Place(scene.view.m_AddonFPK, 2, ACTION_ATTACK, 4, 1, 2, 2, 3);
		DrawSlayer(scene, slayer, ACTION_ATTACK, 4, 1);
		if (facing == 2) scene.Blit(2, 3, scene.view.m_AddonSPK[2], 40, 50);
		else scene.Blit(2, 3, scene.view.m_AddonSPK[1], 30, 60);
		CHECK(scene.Same());
	}
}

TEST(TopView4Slayer, EveryWornPartIsDrawnInItsColoursAndAddsToTheRect)
{
	Scene scene;
	Wearer slayer;
	slayer.Become(kSlayer);
	slayer.SetCurrentDirection(2);
	slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, UNIQUE_ITEM_COLOR);
	slayer.Wear(ADDON_TROUSER, 3, ITEM_CLASS_TROUSER, 31, QUEST_ITEM_COLOR);
	slayer.Wear(ADDON_HELM, 2, ITEM_CLASS_HELM, 40, QUEST_ITEM_COLOR);
	slayer.Wear(ADDON_RIGHTHAND, 4, ITEM_CLASS_SWORD, 41, 51);
	Place(scene.view.m_AddonFPK, 1, ACTION_STAND, 2, 0, 1, 2, 3);
	Place(scene.view.m_AddonFPK, 3, ACTION_STAND, 2, 0, 3, 2, 10);
	Place(scene.view.m_AddonFPK, 2, ACTION_STAND, 2, 0, 2, 14, 20);
	Place(scene.view.m_AddonFPK, 4, ACTION_STAND, 2, 0, 0, 30, 25);
	RECT wide = { 0, 0, 100, 100 };
	slayer.SetScreenRect(&wide);
	slayer.ClearScreenRect();
	DrawSlayer(scene, slayer, ACTION_STAND, 2, 0);
	// A special coat or trouser keeps its first set; anything else takes
	// the special one in both.
	scene.Blit(2, 10, scene.view.m_AddonSPK[3], 31, Special(QUEST_ITEM_COLOR));
	scene.Blit(2, 3, scene.view.m_AddonSPK[1], 30, Special(UNIQUE_ITEM_COLOR));
	scene.Blit(14, 20, scene.view.m_AddonSPK[2], Special(QUEST_ITEM_COLOR), Special(QUEST_ITEM_COLOR));
	scene.Blit(30, 25, scene.view.m_AddonSPK[0], 41, 51);
	CHECK(scene.Same());
	CHECK(SameRect(slayer.GetScreenRect(), 42, 53, 75, 76));
	// A frame the action has not got: nothing, no rect.
	slayer.ClearScreenRect();
	scene.Clear();
	DrawSlayer(scene, slayer, ACTION_STAND, 2, 1);
	CHECK(scene.drawn.Blank());
	CHECK(Cleared(slayer.GetScreenRect()));
}

TEST(TopView4Slayer, AnEffectColouredPartTakesTheColoursOfThePartBefore)
{
	// An effect-coloured part is drawn in its effect colour set, the helm
	// after the coat or the coat first; the other part in its own colours.
	Scene scene;
	Wearer slayer;
	slayer.Become(kSlayer);
	slayer.SetCurrentDirection(2);
	slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
	MCreatureWear::ADDON_INFO& helm = slayer.Wear(ADDON_HELM, 2, ITEM_CLASS_HELM, 40, 50);
	helm.bEffectColor = TRUE;
	helm.EffectColorSet = 77;
	Place(scene.view.m_AddonFPK, 1, ACTION_STAND, 2, 0, 1, 2, 3);
	Place(scene.view.m_AddonFPK, 2, ACTION_STAND, 2, 0, 2, 14, 20);
	DrawSlayer(scene, slayer, ACTION_STAND, 2, 0);
	scene.Blit(2, 3, scene.view.m_AddonSPK[1], 30, 60);
	scene.ColorSet(14, 20, scene.view.m_AddonSPK[2], 77);
	CHECK(scene.Same());
	helm.bEffectColor = FALSE;
	slayer.m_Addon[ADDON_COAT].bEffectColor = TRUE;
	slayer.m_Addon[ADDON_COAT].EffectColorSet = 78;
	Scene::ResetSets();
	scene.Clear();
	DrawSlayer(scene, slayer, ACTION_STAND, 2, 0);
	scene.ColorSet(2, 3, scene.view.m_AddonSPK[1], 78);
	scene.Blit(14, 20, scene.view.m_AddonSPK[2], 40, 50);
	CHECK(scene.Same());
	// Fading, each part a bit darker.
	slayer.m_Addon[ADDON_COAT].bEffectColor = FALSE;
	slayer.SetFade();
	scene.Clear();
	DrawSlayer(scene, slayer, ACTION_STAND, 2, 0);
	scene.Darkened(1, 2, 3, scene.view.m_AddonSPK[1], 30, 60);
	scene.Darkened(1, 14, 20, scene.view.m_AddonSPK[2], 40, 50);
	CHECK(scene.Same());
}

TEST(TopView4Slayer, EachStateGoesToItsGameviewDrawing)
{
	// Fading out lays its copies and then the plain parts too; moving fast
	// goes before being invisible, which goes before a weapon's
	// afterimages.
	struct Case { std::vector<EFFECTSTATUS> statuses; bool fast; int invisible; int creatureAction; std::vector<Scene::Effect> draws; bool plain; };
	const std::vector<Case> cases = {
		{{EFFECTSTATUS_FADE_OUT}, false, 0, ACTION_ATTACK, {DrawFadeOut}, true},
		{{EFFECTSTATUS_FADE_OUT}, true, 0, ACTION_ATTACK, {DrawFadeOut, DrawFastMove}, false},
		{{}, true, 32, ACTION_ATTACK, {DrawFastMove}, false},
		{{EFFECTSTATUS_DANCING_SWORD}, false, 32, ACTION_ATTACK, {DrawInvisible}, false},
		{{EFFECTSTATUS_DANCING_SWORD}, false, 0, ACTION_ATTACK, {DrawWeaponFadeOut}, false},
		{{EFFECTSTATUS_GHOST_BLADE}, false, 0, ACTION_ATTACK, {DrawWeaponFadeOut}, false},
		{{EFFECTSTATUS_REDIANCE}, false, 0, ACTION_STAND, {DrawWeaponFadeOut}, false},
		{{EFFECTSTATUS_REDIANCE}, false, 0, ACTION_ATTACK, {}, true},
	};
	int n = 0;
	for (const Case& c : cases)
	{
		Scene scene;
		Wearer slayer;
		slayer.Become(kSlayer);
		slayer.SetCurrentDirection(2);
		slayer.m_Action = BYTE(c.creatureAction);
		slayer.m_cX = 8;
		slayer.m_cY = 4;
		for (EFFECTSTATUS status : c.statuses) slayer.Set(status);
		slayer.m_bFastMove = c.fast;
		slayer.m_InvisibleCount = c.invisible;
		slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
		slayer.Wear(ADDON_RIGHTHAND, 2, ITEM_CLASS_SWORD, 40, 50);
		Frames(scene.view.m_AddonFPK, 1, ACTION_ATTACK, 2, {{0, 60, 0}, {0, 60, 4}, {1, 2, 3}});
		Frames(scene.view.m_AddonFPK, 2, ACTION_ATTACK, 2, {{3, 20, 30}, {3, 24, 30}, {2, 28, 30}});
		const int value = CSpriteSurface::s_Value1;
		const auto effect = IndexEffect::Get();
		DrawSlayer(scene, slayer, ACTION_ATTACK, 2, 2);
		CSpriteSurface::s_Value1 = value;
		IndexEffect::Set(effect);
		bool first = true;
		for (Scene::Effect draw : c.draws)
		{
			if (first) scene.Expect(draw, slayer, ACTION_ATTACK, 2, 2);
			else
			{
				// Continue from the colours the first left.
				CreatureDrawPacks packs = Access::Packs(scene.view);
				packs.pSurface = &scene.expected.surface;
				POINT point = scene.at;
				scene.expected.surface.Lock();
				draw(packs, &point, &slayer, ACTION_ATTACK, 2, 2);
				scene.expected.surface.Unlock();
			}
			first = false;
		}
		if (c.plain)
		{
			scene.Blit(2, 3, scene.view.m_AddonSPK[1], 30, 60);
			scene.Blit(28, 30, scene.view.m_AddonSPK[2], 40, 50);
		}
		const bool same = scene.Same();
		if (!same) std::printf("      case %d\n", n);
		CHECK(same);
		++n;
	}
}

TEST(TopView4Slayer, AnInstallingTurretIsGameviewsAndNoClothes)
{
	for (int index : {0, 1})
	{
		Scene scene;
		ShapePack(scene.view.m_CreatureFPK, 223, kActions);
		Wearer slayer;
		slayer.Become(kSlayer);
		slayer.Set(EFFECTSTATUS_INSTALL_TURRET);
		slayer.SetInstallTurretCount(1);
		slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
		Place(scene.view.m_AddonFPK, 1, ACTION_STAND, 2, 0, 1, 2, 3);
		Place(scene.view.m_CreatureFPK, 222, ACTION_STAND, 2, 1, 2, 6, 7);
		slayer.ClearScreenRect();
		DrawSlayer(scene, slayer, ACTION_STAND, 2, 0, index);
		scene.Blit(6, 7, scene.Body(2), 1, 2);
		CHECK(scene.Same());
		// The frame index reaches it: only the first sets the rect.
		if (index == 0) CHECK(SameRect(slayer.GetScreenRect(), 46, 57, 51, 58));
		else CHECK(Cleared(slayer.GetScreenRect()));
	}
}

//----------------------------------------------------------------------
// An ousters' coat and chakram
//----------------------------------------------------------------------
namespace {

void DrawOusters(Scene& scene, MCreature& ousters, int action, int direction, int frame, bool advanced = false)
{
	scene.Call([&](POINT* p) {
		if (advanced) Access::AdvancedOusters(scene.view, p, &ousters, action, direction, frame);
		else Access::Ousters(scene.view, p, &ousters, action, direction, frame);
	});
}

} // namespace

TEST(TopView4Ousters, TheCoatInFourColoursThenTheChakramInItsTwo)
{
	// Channels: hair (body 1), coat, arm (the chakram's), boots.
	for (bool advanced : {false, true})
	{
		Scene scene;
		Wearer ousters;
		ousters.Become(kOusters);
		if (advanced) ousters.Advance();
		ousters.SetCurrentDirection(2);
		ousters.SetBodyColor1(11);
		ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT, 30, 60);
		ousters.Wear(ADDON_TROUSER, 2, ITEM_CLASS_OUSTERS_BOOTS, 33, 70);
		ousters.Wear(ADDON_RIGHTHAND, 3, ITEM_CLASS_OUSTERS_CHAKRAM, 44, 80);
		CCreatureFramePack& pack = advanced ? scene.view.m_AdvancementOustersFPK : scene.view.m_OustersFPK;
		CIndexSpritePack& sprites = advanced ? scene.view.m_AdvancementOustersSPK : scene.view.m_OustersSPK;
		// Advanced: the coat is part 1 and the chakram part 0, whatever
		// their frame IDs; on the chakram's attack.
		const int action = advanced ? ACTION_ADVANCEMENT_OUSTERS_ATTACK_NORMAL - ADVANCEMENT_ACTION_START : ACTION_ATTACK;
		Place(pack, advanced ? 1 : 1, action, 2, 1, 1, 2, 3);
		Place(pack, advanced ? 0 : 3, action, 2, 1, 2, 2, 3);
		RECT wide = { 0, 0, 100, 100 };
		ousters.SetScreenRect(&wide);
		ousters.ClearScreenRect();
		DrawOusters(scene, ousters, ACTION_ATTACK, 2, 1, advanced);
		scene.Blit(2, 3, sprites[1], 11, 60, 80, 70);
		scene.Blit(2, 3, sprites[2], 44, 80, 80, 70);
		CHECK(scene.Same());
		// Only the coat sets the rect.
		CHECK(SameRect(ousters.GetScreenRect(), 42, 53, 47, 54));
		// The chakram lies over the coat: apart, both show.
		Place(pack, advanced ? 0 : 3, action, 2, 1, 2, 22, 3);
		scene.Clear();
		DrawOusters(scene, ousters, ACTION_ATTACK, 2, 1, advanced);
		scene.Blit(2, 3, sprites[1], 11, 60, 80, 70);
		scene.Blit(22, 3, sprites[2], 44, 80, 80, 70);
		CHECK(scene.Same());
	}
}

TEST(TopView4Ousters, AWristletStandsAndWalksAsOustersAndColoursTheArm)
{
	// Without a chakram standing and walking are the ousters' own actions;
	// the arm takes the wristlet's second set, or the default coat colour
	// with nothing in the hand; the boots likewise.
	for (bool advanced : {false, true})
		for (bool wristlet : {true, false})
		{
			Scene scene;
			Wearer ousters;
			ousters.Become(kOusters);
			if (advanced) ousters.Advance();
			ousters.SetCurrentDirection(2);
			ousters.SetBodyColor1(11);
			ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT, 30, 60);
			if (wristlet) ousters.Wear(ADDON_RIGHTHAND, 3, ITEM_CLASS_OUSTERS_WRISTLET, 44, 80);
			CCreatureFramePack& pack = advanced ? scene.view.m_AdvancementOustersFPK : scene.view.m_OustersFPK;
			CIndexSpritePack& sprites = advanced ? scene.view.m_AdvancementOustersSPK : scene.view.m_OustersSPK;
			struct Pair { int action, drawn; };
			for (const Pair& pair : {Pair{ACTION_STAND, advanced ? ACTION_ADVANCEMENT_OUSTERS_WRISTLET_STOP - 100 : ACTION_OUSTERS_STAND},
				Pair{ACTION_MOVE, advanced ? ACTION_ADVANCEMENT_OUSTERS_WRISTLET_MOVE - 100 : ACTION_OUSTERS_MOVE},
				Pair{ACTION_MAGIC, advanced ? ACTION_ADVANCEMENT_OUSTERS_MAGIC - 100 : ACTION_MAGIC}})
			{
				Place(pack, 1, pair.drawn, 2, 0, 1, 2, 3);
				scene.Clear();
				DrawOusters(scene, ousters, pair.action, 2, 0, advanced);
				scene.Blit(2, 3, sprites[1], 11, 60, wristlet ? 80 : defaultCoatColor, defaultCoatColor);
				const bool same = scene.Same();
				if (!same) std::printf("      %s, %s, action %d\n", advanced ? "advanced" : "plain", wristlet ? "wristlet" : "bare", pair.action);
				CHECK(same);
			}
		}
}

TEST(TopView4Ousters, AChakramStandsAndWalksAsItself)
{
	for (bool advanced : {false, true})
	{
		Scene scene;
		Wearer ousters;
		ousters.Become(kOusters);
		if (advanced) ousters.Advance();
		ousters.SetCurrentDirection(2);
		ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT, 30, 60);
		ousters.Wear(ADDON_RIGHTHAND, 3, ITEM_CLASS_OUSTERS_CHAKRAM, 44, 80);
		CCreatureFramePack& pack = advanced ? scene.view.m_AdvancementOustersFPK : scene.view.m_OustersFPK;
		const int stand = advanced ? ACTION_ADVANCEMENT_OUSTERS_CHAKRAM_STOP - 100 : ACTION_STAND;
		Place(pack, 1, stand, 2, 0, 1, 2, 3);
		DrawOusters(scene, ousters, ACTION_STAND, 2, 0, advanced);
		CIndexSpritePack& sprites = advanced ? scene.view.m_AdvancementOustersSPK : scene.view.m_OustersSPK;
		scene.Blit(2, 3, sprites[1], 30, 60, 80, defaultCoatColor);
		CHECK(scene.Same());
	}
}

TEST(TopView4Ousters, SpecialColoursAreMappedOnEveryChannel)
{
	for (bool advanced : {false, true})
	{
		Scene scene;
		Wearer ousters;
		ousters.Become(kOusters);
		if (advanced) ousters.Advance();
		ousters.SetCurrentDirection(2);
		ousters.SetBodyColor1(11);
		ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT, 30, UNIQUE_ITEM_COLOR);
		ousters.Wear(ADDON_TROUSER, 2, ITEM_CLASS_OUSTERS_BOOTS, 33, QUEST_ITEM_COLOR);
		ousters.Wear(ADDON_RIGHTHAND, 3, ITEM_CLASS_OUSTERS_CHAKRAM, QUEST_ITEM_COLOR, UNIQUE_ITEM_COLOR);
		CCreatureFramePack& pack = advanced ? scene.view.m_AdvancementOustersFPK : scene.view.m_OustersFPK;
		CIndexSpritePack& sprites = advanced ? scene.view.m_AdvancementOustersSPK : scene.view.m_OustersSPK;
		const int action = advanced ? ACTION_ADVANCEMENT_OUSTERS_MAGIC - 100 : ACTION_MAGIC;
		Place(pack, 1, action, 2, 0, 1, 2, 3);
		Place(pack, advanced ? 0 : 3, action, 2, 0, 2, 22, 3);
		DrawOusters(scene, ousters, ACTION_MAGIC, 2, 0, advanced);
		const int unique = Special(UNIQUE_ITEM_COLOR), quest = Special(QUEST_ITEM_COLOR);
		scene.Blit(2, 3, sprites[1], 11, unique, unique, quest);
		scene.Blit(22, 3, sprites[2], quest, unique, unique, quest);
		CHECK(scene.Same());
		// A wristlet's and the default arm's are mapped too.
		ousters.Wear(ADDON_RIGHTHAND, 3, ITEM_CLASS_OUSTERS_WRISTLET, 44, QUEST_ITEM_COLOR);
		Place(pack, 1, advanced ? ACTION_ADVANCEMENT_OUSTERS_MAGIC - 100 : ACTION_MAGIC, 2, 0, 1, 2, 3);
		scene.Clear();
		DrawOusters(scene, ousters, ACTION_MAGIC, 2, 0, advanced);
		scene.Blit(2, 3, sprites[1], 11, unique, quest, quest);
		CHECK(scene.Same());
	}
}

TEST(TopView4Ousters, AGhostsPartsAndAPastFrameAreNotDrawn)
{
	for (bool advanced : {false, true})
	{
		Scene scene;
		Wearer ousters;
		ousters.Become(kOusters);
		if (advanced) ousters.Advance();
		ousters.SetCurrentDirection(2);
		ousters.SetBodyColor1(11);
		ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT, 30, 60);
		ousters.Wear(ADDON_TROUSER, 2, ITEM_CLASS_OUSTERS_BOOTS, 33, 70);
		ousters.Wear(ADDON_RIGHTHAND, 3, ITEM_CLASS_OUSTERS_CHAKRAM, 44, 80);
		CCreatureFramePack& pack = advanced ? scene.view.m_AdvancementOustersFPK : scene.view.m_OustersFPK;
		CIndexSpritePack& sprites = advanced ? scene.view.m_AdvancementOustersSPK : scene.view.m_OustersSPK;
		const int action = advanced ? ACTION_ADVANCEMENT_OUSTERS_MAGIC - 100 : ACTION_MAGIC;
		Place(pack, 1, action, 2, 0, 1, 2, 3);
		Place(pack, advanced ? 0 : 3, action, 2, 0, 2, 22, 3);
		// Ghost part 1 is the coat.
		ousters.SetGhost(1, 10);
		DrawOusters(scene, ousters, ACTION_MAGIC, 2, 0, advanced);
		scene.Blit(22, 3, sprites[2], 44, 80);
		CHECK(scene.Same());
		// Ghost part 2 the chakram: the coat in its colours.
		ousters.SetGhost(2, 10);
		Scene::ResetSets();
		scene.Clear();
		DrawOusters(scene, ousters, ACTION_MAGIC, 2, 0, advanced);
		scene.Blit(2, 3, sprites[1], 11, 60, 80, 70);
		CHECK(scene.Same());
		ousters.SetGhost(0, 0);
		ousters.ClearScreenRect();
		scene.Clear();
		DrawOusters(scene, ousters, ACTION_MAGIC, 2, 1, advanced);
		CHECK(scene.drawn.Blank());
		CHECK(Cleared(ousters.GetScreenRect()));
	}
}

TEST(TopView4Ousters, EffectColouredPartsTakeTheColourSetBlitAndFadingIsDrawn)
{
	for (bool advanced : {false, true})
	{
		Scene scene;
		Wearer ousters;
		ousters.Become(kOusters);
		if (advanced) ousters.Advance();
		ousters.SetCurrentDirection(2);
		ousters.SetBodyColor1(11);
		MCreatureWear::ADDON_INFO& coat = ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT, 30, 60);
		ousters.Wear(ADDON_TROUSER, 2, ITEM_CLASS_OUSTERS_BOOTS, 33, 70);
		MCreatureWear::ADDON_INFO& chakram = ousters.Wear(ADDON_RIGHTHAND, 3, ITEM_CLASS_OUSTERS_CHAKRAM, 44, 80);
		CCreatureFramePack& pack = advanced ? scene.view.m_AdvancementOustersFPK : scene.view.m_OustersFPK;
		CIndexSpritePack& sprites = advanced ? scene.view.m_AdvancementOustersSPK : scene.view.m_OustersSPK;
		const int action = advanced ? ACTION_ADVANCEMENT_OUSTERS_MAGIC - 100 : ACTION_MAGIC;
		Place(pack, 1, action, 2, 0, 1, 2, 3);
		Place(pack, advanced ? 0 : 3, action, 2, 0, 2, 22, 3);
		coat.bEffectColor = TRUE;
		coat.EffectColorSet = 77;
		DrawOusters(scene, ousters, ACTION_MAGIC, 2, 0, advanced);
		scene.ColorSet(2, 3, sprites[1], 77);
		scene.Blit(22, 3, sprites[2], 44, 80);
		CHECK(scene.Same());
		coat.bEffectColor = FALSE;
		chakram.bEffectColor = TRUE;
		chakram.EffectColorSet = 78;
		Scene::ResetSets();
		scene.Clear();
		DrawOusters(scene, ousters, ACTION_MAGIC, 2, 0, advanced);
		scene.Blit(2, 3, sprites[1], 11, 60, 80, 70);
		scene.ColorSet(22, 3, sprites[2], 78);
		CHECK(scene.Same());
		chakram.bEffectColor = FALSE;
		// Fading, each part a bit darker.
		ousters.SetFade();
		scene.Clear();
		DrawOusters(scene, ousters, ACTION_MAGIC, 2, 0, advanced);
		scene.Darkened(1, 2, 3, sprites[1], 11, 60, 80, 70);
		scene.Darkened(1, 22, 3, sprites[2], 44, 80, 80, 70);
		CHECK(scene.Same());
	}
}

TEST(TopView4Ousters, AnAdvancedOustersWithoutAnAdvancedActionIsNotDrawn)
{
	// Past the chakram's fast attack (17) there is none.
	Scene scene;
	Wearer ousters;
	ousters.Become(kOusters);
	ousters.Advance();
	ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT, 30, 60);
	ousters.Wear(ADDON_RIGHTHAND, 3, ITEM_CLASS_OUSTERS_CHAKRAM, 44, 80);
	CHECK_EQ(-1, ConvAdvancementOustersActionFromOusterAction(18, true));
	for (int action = 0; action < kAdvancedActions; ++action)
	{
		Place(scene.view.m_AdvancementOustersFPK, 0, action, 2, 0, 1, 2, 3);
		Place(scene.view.m_AdvancementOustersFPK, 1, action, 2, 0, 1, 2, 3);
	}
	DrawOusters(scene, ousters, 18, 2, 0, true);
	CHECK(scene.drawn.Blank());
	DrawOusters(scene, ousters, ACTION_OUSTERS_CHAKRAM_FAST, 2, 0, true);
	CHECK(!scene.drawn.Blank());
}

//----------------------------------------------------------------------
// The advanced slayer
//----------------------------------------------------------------------
namespace {

void DrawAdvancedSlayer(Scene& scene, MCreature& slayer, int action, int direction, int frame, int index = 0)
{
	scene.Call([&](POINT* p) { Access::AdvancedSlayer(scene.view, p, &slayer, action, direction, frame, index); });
}

int AdvancedMagic(MCreatureWear& slayer)
{
	return ConvAdvancementSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer) - ADVANCEMENT_ACTION_START;
}

} // namespace

TEST(TopView4AdvancedSlayer, WithoutAnAdvancedActionNothingIsDrawn)
{
	Scene scene;
	Wearer slayer;
	slayer.Become(kSlayer);
	slayer.Advance();
	slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
	CHECK_EQ(-1, ConvAdvancementSlayerActionFromSlayerAction(ACTION_ATTACK, &slayer));
	for (int action = 0; action < kAdvancedActions; ++action)
		Place(scene.view.m_AdvancementSlayerManFPK, AC_BODY, action, 2, 0, 1, 2, 3);
	DrawAdvancedSlayer(scene, slayer, ACTION_ATTACK, 2, 0);
	CHECK(scene.drawn.Blank());
	DrawAdvancedSlayer(scene, slayer, ACTION_MAGIC, 2, 0);
	CHECK(!scene.drawn.Blank());
}

TEST(TopView4AdvancedSlayer, PartsComeFromItsGendersPackInTheOrderOfTheFacingGiven)
{
	// A shield and a sword on one spot; the order is the given facing's
	// (unlike the plain slayer's, its own).
	for (bool male : {true, false})
		for (int direction : {0, 2})
		{
			Scene scene;
			Wearer slayer;
			slayer.Become(kSlayer);
			slayer.Advance();
			slayer.m_bMale = male;
			slayer.SetCurrentDirection(BYTE(2 - direction));
			slayer.Wear(ADDON_LEFTHAND, 1, ITEM_CLASS_SHIELD, 30, 60);
			slayer.Wear(ADDON_RIGHTHAND, 2, ITEM_CLASS_SWORD, 40, 50);
			CCreatureFramePack& pack = male ? scene.view.m_AdvancementSlayerManFPK : scene.view.m_AdvancementSlayerWomanFPK;
			CIndexSpritePack& sprites = male ? scene.view.m_AdvancementSlayerManSPK : scene.view.m_AdvancementSlayerWomanSPK;
			const int action = AdvancedMagic(slayer);
			Place(pack, AC_SHIELD, action, direction, 1, 1, 2, 3);
			Place(pack, AC_SWORD, action, direction, 1, 2, 2, 3);
			slayer.ClearScreenRect();
			DrawAdvancedSlayer(scene, slayer, ACTION_MAGIC, direction, 1);
			if (direction == 2) scene.Blit(2, 3, sprites[2], 40, 50);
			else scene.Blit(2, 3, sprites[1], 30, 60);
			const bool same = scene.Same();
			if (!same) std::printf("      %s, facing %d\n", male ? "male" : "female", direction);
			CHECK(same);
			CHECK(SameRect(slayer.GetScreenRect(), 42, 53, 47, 54));
			// A frame its action has not got: nothing.
			slayer.ClearScreenRect();
			scene.Clear();
			DrawAdvancedSlayer(scene, slayer, ACTION_MAGIC, direction, 2);
			CHECK(scene.drawn.Blank());
			CHECK(Cleared(slayer.GetScreenRect()));
		}
}

TEST(TopView4AdvancedSlayer, NewItemsTakeTheirFrameIDsOnTheNewAction)
{
	// A coat from type 28 is two frame sets (its own and the next), a helm
	// from 15 one; on the new action (3 for magic), not the advanced one.
	Scene scene;
	Wearer slayer;
	slayer.Become(kSlayer);
	slayer.Advance();
	slayer.SetCurrentDirection(2);
	slayer.Wear(ADDON_COAT, 5, ITEM_CLASS_COAT, 30, UNIQUE_ITEM_COLOR, 28);
	slayer.Wear(ADDON_HELM, 8, ITEM_CLASS_HELM, 40, QUEST_ITEM_COLOR, 15);
	const int action = ConvNewSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer);
	CHECK_EQ(int(ACTION_NEW_MAGIC), action);
	CHECK(action != AdvancedMagic(slayer));
	Place(scene.view.m_AdvancementSlayerManFPK, 5, action, 2, 0, 0, 2, 3);
	Place(scene.view.m_AdvancementSlayerManFPK, 6, action, 2, 0, 1, 26, 3);
	Place(scene.view.m_AdvancementSlayerManFPK, 8, action, 2, 0, 2, 14, 30);
	Place(scene.view.m_AdvancementSlayerManFPK, AC_BODY, AdvancedMagic(slayer), 2, 0, 3, 50, 50);
	slayer.ClearScreenRect();
	DrawAdvancedSlayer(scene, slayer, ACTION_MAGIC, 2, 0);
	scene.Blit(2, 3, scene.view.m_AdvancementSlayerManSPK[0], 30, Special(UNIQUE_ITEM_COLOR));
	scene.Blit(26, 3, scene.view.m_AdvancementSlayerManSPK[1], 30, Special(UNIQUE_ITEM_COLOR));
	scene.Blit(14, 30, scene.view.m_AdvancementSlayerManSPK[2], Special(QUEST_ITEM_COLOR), Special(QUEST_ITEM_COLOR));
	CHECK(scene.Same());
	CHECK(SameRect(slayer.GetScreenRect(), 42, 53, 71, 81));
	// One type below, the old item: its part on the advanced action.
	slayer.Wear(ADDON_COAT, 5, ITEM_CLASS_COAT, 30, 60, 27);
	slayer.m_Addon[ADDON_HELM].bAddon = FALSE;
	scene.Clear();
	DrawAdvancedSlayer(scene, slayer, ACTION_MAGIC, 2, 0);
	scene.Blit(50, 50, scene.view.m_AdvancementSlayerManSPK[3], 30, 60);
	CHECK(scene.Same());
}

TEST(TopView4AdvancedSlayer, ANewItemPastThePackOrItsActionsIsSkipped)
{
	Scene scene;
	Wearer slayer;
	slayer.Become(kSlayer);
	slayer.Advance();
	slayer.SetCurrentDirection(2);
	const int action = ConvNewSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer);
	// The coat's second frame set, past the pack, is skipped; its first drawn.
	slayer.Wear(ADDON_COAT, kIds - 1, ITEM_CLASS_COAT, 30, 60, 28);
	Place(scene.view.m_AdvancementSlayerManFPK, kIds - 1, action, 2, 0, 1, 2, 3);
	DrawAdvancedSlayer(scene, slayer, ACTION_MAGIC, 2, 0);
	scene.Blit(2, 3, scene.view.m_AdvancementSlayerManSPK[1], 30, 60);
	CHECK(scene.Same());
	// A frame set with the action as its last is drawn; one short, not.
	slayer.m_Addon[ADDON_COAT].bAddon = FALSE;
	slayer.Wear(ADDON_HELM, 8, ITEM_CLASS_HELM, 40, 50, 15);
	scene.view.m_AdvancementSlayerManFPK[8].Init(BYTE(action + 1));
	for (int a = 0; a <= action; ++a) scene.view.m_AdvancementSlayerManFPK[8][BYTE(a)].Init(8);
	Place(scene.view.m_AdvancementSlayerManFPK, 8, action, 2, 0, 2, 14, 30);
	scene.Clear();
	DrawAdvancedSlayer(scene, slayer, ACTION_MAGIC, 2, 0);
	scene.Blit(14, 30, scene.view.m_AdvancementSlayerManSPK[2], 40, 50);
	CHECK(scene.Same());
	scene.view.m_AdvancementSlayerManFPK[8].Init(BYTE(action));
	for (int a = 0; a < action; ++a) scene.view.m_AdvancementSlayerManFPK[8][BYTE(a)].Init(8);
	scene.Clear();
	DrawAdvancedSlayer(scene, slayer, ACTION_MAGIC, 2, 0);
	CHECK(scene.drawn.Blank());
}

TEST(TopView4AdvancedSlayer, AnEffectColouredPartTakesTheColoursBefore)
{
	for (bool newItems : {false, true})
	{
		Scene scene;
		Wearer slayer;
		slayer.Become(kSlayer);
		slayer.Advance();
		slayer.SetCurrentDirection(2);
		slayer.Wear(ADDON_COAT, 5, ITEM_CLASS_COAT, 30, 60, newItems ? 28 : 0);
		MCreatureWear::ADDON_INFO& helm = slayer.Wear(ADDON_HELM, 8, ITEM_CLASS_HELM, 40, 50, newItems ? 15 : 0);
		helm.bEffectColor = TRUE;
		helm.EffectColorSet = 77;
		const int action = newItems ? ConvNewSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer) : AdvancedMagic(slayer);
		Place(scene.view.m_AdvancementSlayerManFPK, newItems ? 5 : int(AC_BODY), action, 2, 0, 1, 2, 3);
		Place(scene.view.m_AdvancementSlayerManFPK, newItems ? 8 : int(AC_HELMET), action, 2, 0, 2, 14, 30);
		if (newItems) scene.view.m_AdvancementSlayerManFPK[6].Init(0);
		DrawAdvancedSlayer(scene, slayer, ACTION_MAGIC, 2, 0);
		scene.Blit(2, 3, scene.view.m_AdvancementSlayerManSPK[1], 30, 60);
		scene.ColorSet(14, 30, scene.view.m_AdvancementSlayerManSPK[2], 77);
		CHECK(scene.Same());
		// Fading, each part a bit darker.
		helm.bEffectColor = FALSE;
		slayer.SetFade();
		scene.Clear();
		DrawAdvancedSlayer(scene, slayer, ACTION_MAGIC, 2, 0);
		scene.Darkened(1, 2, 3, scene.view.m_AdvancementSlayerManSPK[1], 30, 60);
		scene.Darkened(1, 14, 30, scene.view.m_AdvancementSlayerManSPK[2], 40, 50);
		CHECK(scene.Same());
	}
}

TEST(TopView4AdvancedSlayer, EachStateGoesToItsGameviewDrawingWithThePlainAction)
{
	struct Case { EFFECTSTATUS status; bool fast; int invisible; int creatureAction; std::vector<Scene::Effect> draws; bool plain; };
	const std::vector<Case> cases = {
		{EFFECTSTATUS_FADE_OUT, false, 0, ACTION_MAGIC, {DrawFadeOutForACSlayer}, true},
		{EFFECTSTATUS_FADE_OUT, true, 0, ACTION_MAGIC, {DrawFadeOutForACSlayer, DrawFastMoveForACSlayer}, false},
		{EFFECTSTATUS_MAX, true, 32, ACTION_MAGIC, {DrawFastMoveForACSlayer}, false},
		{EFFECTSTATUS_DANCING_SWORD, false, 32, ACTION_MAGIC, {DrawInvisibleForACSlayer}, false},
		{EFFECTSTATUS_DANCING_SWORD, false, 0, ACTION_MAGIC, {DrawWeaponFadeOutForACSlayer}, false},
		{EFFECTSTATUS_GHOST_BLADE, false, 0, ACTION_MAGIC, {DrawWeaponFadeOutForACSlayer}, false},
		{EFFECTSTATUS_REDIANCE, false, 0, ACTION_STAND, {DrawWeaponFadeOutForACSlayer}, false},
		{EFFECTSTATUS_REDIANCE, false, 0, ACTION_MAGIC, {}, true},
	};
	int n = 0;
	for (const Case& c : cases)
	{
		Scene scene;
		Wearer slayer;
		slayer.Become(kSlayer);
		slayer.Advance();
		slayer.SetCurrentDirection(2);
		slayer.m_Action = BYTE(c.creatureAction);
		slayer.m_cX = 8;
		slayer.m_cY = 4;
		if (c.status != EFFECTSTATUS_MAX) slayer.Set(c.status);
		slayer.m_bFastMove = c.fast;
		slayer.m_InvisibleCount = c.invisible;
		slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, 60);
		slayer.Wear(ADDON_RIGHTHAND, 2, ITEM_CLASS_SWORD, 40, 50);
		const int action = AdvancedMagic(slayer);
		Frames(scene.view.m_AdvancementSlayerManFPK, AC_BODY, action, 2, {{0, 60, 0}, {0, 60, 4}, {1, 2, 3}});
		Frames(scene.view.m_AdvancementSlayerManFPK, AC_SWORD, action, 2, {{3, 20, 30}, {3, 24, 30}, {2, 28, 30}});
		const int value = CSpriteSurface::s_Value1;
		const auto effect = IndexEffect::Get();
		DrawAdvancedSlayer(scene, slayer, ACTION_MAGIC, 2, 2);
		CSpriteSurface::s_Value1 = value;
		IndexEffect::Set(effect);
		bool first = true;
		for (Scene::Effect draw : c.draws)
		{
			if (first) scene.Expect(draw, slayer, ACTION_MAGIC, 2, 2);
			else
			{
				CreatureDrawPacks packs = Access::Packs(scene.view);
				packs.pSurface = &scene.expected.surface;
				POINT point = scene.at;
				scene.expected.surface.Lock();
				draw(packs, &point, &slayer, ACTION_MAGIC, 2, 2);
				scene.expected.surface.Unlock();
			}
			first = false;
		}
		if (c.plain)
		{
			scene.Blit(2, 3, scene.view.m_AdvancementSlayerManSPK[1], 30, 60);
			scene.Blit(28, 30, scene.view.m_AdvancementSlayerManSPK[2], 40, 50);
		}
		const bool same = scene.Same();
		if (!same) std::printf("      case %d\n", n);
		CHECK(same);
		++n;
	}
}

//----------------------------------------------------------------------
// The advanced vampire
//----------------------------------------------------------------------
namespace {

void DrawAdvancedVampire(Scene& scene, MCreature& vampire, int action, int direction, int frame, int index = 0)
{
	scene.Call([&](POINT* p) { Access::AdvancedVampire(scene.view, p, &vampire, action, direction, frame, kPlayerBody, index); });
}

const int kStop = ACTION_ADVANCEMENT_STOP - ADVANCEMENT_ACTION_START;

} // namespace

TEST(TopView4AdvancedVampire, TheBodyThenItsWeaponTheBodySettingTheRect)
{
	for (int index : {0, 1})
	{
		Scene scene;
		Wearer vampire;
		vampire.Become(kVampire);
		vampire.Advance();
		Place(scene.view.m_AdvancementVampireManFPK, 0, kStop, 3, 1, 1, 5, 6);
		Place(scene.view.m_AdvancementVampireManFPK, 1, kStop, 3, 1, 2, 5, 6);
		vampire.ClearScreenRect();
		DrawAdvancedVampire(scene, vampire, ACTION_STAND, 3, 1, index);
		// The weapon over the body.
		scene.Blit(5, 6, scene.view.m_AdvancementVampireManSPK[2], 30, 60);
		CHECK(scene.Same());
		if (index == 0) CHECK(SameRect(vampire.GetScreenRect(), 45, 56, 50, 57));
		else CHECK(Cleared(vampire.GetScreenRect()));
		// Apart, both show; the weapon sets no rect.
		Place(scene.view.m_AdvancementVampireManFPK, 1, kStop, 3, 1, 2, 25, 9);
		vampire.ClearScreenRect();
		scene.Clear();
		DrawAdvancedVampire(scene, vampire, ACTION_STAND, 3, 1, index);
		scene.Blit(5, 6, scene.view.m_AdvancementVampireManSPK[1], 30, 60);
		scene.Blit(25, 9, scene.view.m_AdvancementVampireManSPK[2], 30, 60);
		CHECK(scene.Same());
		if (index == 0) CHECK(SameRect(vampire.GetScreenRect(), 45, 56, 50, 57));
	}
}

TEST(TopView4AdvancedVampire, WithoutItsBodysFrameTheWeaponIsInTheColoursLastSet)
{
	// The weapon sets no colours of its own.
	Scene scene;
	Wearer vampire;
	vampire.Become(kVampire);
	vampire.Advance();
	Place(scene.view.m_AdvancementVampireManFPK, 1, kStop, 3, 0, 2, 25, 9);
	DrawAdvancedVampire(scene, vampire, ACTION_STAND, 3, 0);
	scene.Blit(25, 9, scene.view.m_AdvancementVampireManSPK[2], 1, 2);
	CHECK(scene.Same());
}

TEST(TopView4AdvancedVampire, ABatAWolfAndAWerwolfHaveNoWeapon)
{
	for (int type : {CREATURETYPE_BAT, CREATURETYPE_WOLF, CREATURETYPE_WER_WOLF, kMonster})
	{
		Scene scene;
		scene.Monster(type);
		Wearer beast;
		beast.Become(type);
		beast.Advance();
		beast.m_bMale = true;
		Place(scene.view.m_AdvancementVampireManFPK, 0, kStop, 3, 0, 1, 5, 6);
		Place(scene.view.m_AdvancementVampireManFPK, 1, kStop, 3, 0, 2, 25, 9);
		DrawAdvancedVampire(scene, beast, ACTION_STAND, 3, 0);
		scene.Blit(5, 6, scene.view.m_AdvancementVampireManSPK[1], 30, 60);
		if (type == kMonster) scene.Blit(25, 9, scene.view.m_AdvancementVampireManSPK[2], 30, 60);
		CHECK(scene.Same());
	}
}

TEST(TopView4AdvancedVampire, WithoutAnAdvancedActionOrPastThePacksNothingNotEvenTheCasket)
{
	Scene scene;
	Wearer vampire;
	vampire.Become(kVampire);
	vampire.Advance();
	vampire.m_bInCasket = true;
	vampire.m_CasketCount = 20;
	CHECK_EQ(-1, GetAdvancementVampireActionFromVampireAction(17, &vampire));
	for (int action = 0; action < kAdvancedActions; ++action)
		Place(scene.view.m_AdvancementVampireManFPK, 0, action, 3, 0, 1, 5, 6);
	DrawAdvancedVampire(scene, vampire, 17, 3, 0);
	CHECK(scene.drawn.Blank());
	// Its body's frame sets end before the action (moving is 1): nothing.
	scene.view.m_AdvancementVampireManFPK[0].Init(1);
	scene.view.m_AdvancementVampireManFPK[0][0].Init(8);
	DrawAdvancedVampire(scene, vampire, ACTION_MOVE, 3, 0);
	CHECK(scene.drawn.Blank());
	// Standing (0), the body and the casket.
	Place(scene.view.m_AdvancementVampireManFPK, 0, kStop, 3, 0, 1, 5, 6);
	DrawAdvancedVampire(scene, vampire, ACTION_STAND, 3, 0);
	CHECK(!scene.drawn.Blank());
}

TEST(TopView4AdvancedVampire, Types807To812AreMenOfTheirOwnBodiesAndActions)
{
	// Each its own frame set (2 to 7), from the men's pack; magic (12) is
	// drawn as 7 and magic attack (13) as 16.
	struct Case { int plain, drawn; };
	for (int type = 807; type <= 812; ++type)
		for (const Case& c : {Case{ACTION_STAND, kStop}, Case{ACTION_MAGIC, 7}, Case{13, 16}})
		{
			Scene scene;
			scene.Monster(type);
			Wearer vampire;
			vampire.Become(type);
			vampire.Advance();
			vampire.m_bMale = false;
			Place(scene.view.m_AdvancementVampireManFPK, type - 805, c.drawn, 3, 0, 1, 5, 6);
			Place(scene.view.m_AdvancementVampireManFPK, 1, c.drawn, 3, 0, 2, 25, 9);
			Place(scene.view.m_AdvancementVampireWomanFPK, 0, c.drawn, 3, 0, 3, 45, 9);
			DrawAdvancedVampire(scene, vampire, c.plain, 3, 0);
			scene.Blit(5, 6, scene.view.m_AdvancementVampireManSPK[1], 30, 60);
			scene.Blit(25, 9, scene.view.m_AdvancementVampireManSPK[2], 30, 60);
			const bool same = scene.Same();
			if (!same) std::printf("      type %d, action %d\n", type, c.plain);
			CHECK(same);
		}
	// Another type's magic is 12, its body 0, by its gender.
	Scene scene;
	Wearer vampire;
	vampire.Become(kVampire);
	vampire.Advance();
	vampire.m_bMale = false;
	Place(scene.view.m_AdvancementVampireWomanFPK, 0, ACTION_ADVANCEMENT_MAGIC - ADVANCEMENT_ACTION_START, 3, 0, 1, 5, 6);
	DrawAdvancedVampire(scene, vampire, ACTION_MAGIC, 3, 0);
	scene.Blit(5, 6, scene.view.m_AdvancementVampireWomanSPK[1], 30, 60);
	CHECK(scene.Same());
}

TEST(TopView4AdvancedVampire, ItsAfterimagesAreDrawnFromTheCreaturePack)
{
	// Recorded defects: the body's afterimage blits m_CreatureSPK with the
	// advanced pack's sprite ID (MTopViewDraw.cpp 2794), and the weapon's
	// reads the body's frames (FA, not FAWEAPON) and m_CreatureSPK too
	// (3027-3030). Facing 3, the afterimages, a bit darker, go first.
	Scene scene;
	Wearer vampire;
	vampire.Become(kVampire);
	vampire.Advance();
	vampire.m_ShadowCount = 1;
	Frames(scene.view.m_AdvancementVampireManFPK, 0, kStop, 3, {{2, 30, 20}, {1, 5, 6}});
	Frames(scene.view.m_AdvancementVampireManFPK, 1, kStop, 3, {{3, 60, 40}, {0, 25, 6}});
	DrawAdvancedVampire(scene, vampire, ACTION_STAND, 3, 1);
	scene.Darkened(1, 30, 20, scene.Body(2), 30, 60);
	scene.Blit(5, 6, scene.view.m_AdvancementVampireManSPK[1], 30, 60);
	scene.Darkened(1, 30, 20, scene.Body(2), 30, 60);
	scene.Blit(25, 6, scene.view.m_AdvancementVampireManSPK[0], 30, 60);
	CHECK(scene.Same());
	// Facing 5, the body and the weapon each before its afterimage.
	Frames(scene.view.m_AdvancementVampireManFPK, 0, kStop, 5, {{2, 5, 6}, {1, 5, 6}});
	Frames(scene.view.m_AdvancementVampireManFPK, 1, kStop, 5, {{3, 60, 40}, {0, 25, 6}});
	scene.Clear();
	DrawAdvancedVampire(scene, vampire, ACTION_STAND, 5, 1);
	scene.Darkened(1, 5, 6, scene.Body(2), 30, 60);
	scene.Blit(25, 6, scene.view.m_AdvancementVampireManSPK[0], 30, 60);
	CHECK(scene.Same());
}

TEST(TopView4AdvancedVampire, AdvancedMovingStepsItsAfterimagesBack)
{
	// Moving is 1 in the advanced actions too, so the steps apply.
	Scene scene;
	Wearer vampire;
	vampire.Become(kVampire);
	vampire.Advance();
	vampire.m_ShadowCount = 2;
	vampire.m_cX = 4;
	vampire.m_cY = 2;
	const int move = ACTION_ADVANCEMENT_MOVE - ADVANCEMENT_ACTION_START;
	CHECK_EQ(int(ACTION_MOVE), move);
	Frames(scene.view.m_AdvancementVampireManFPK, 0, move, 2, {{2, 30, 20}, {0, 60, 0}, {1, 5, 6}});
	DrawAdvancedVampire(scene, vampire, ACTION_MOVE, 2, 2);
	scene.Darkened(1, 60, 0, scene.Body(0), 30, 60);
	scene.Darkened(1, 30 - 4, 20 - 2, scene.Body(2), 30, 60);
	scene.Blit(5, 6, scene.view.m_AdvancementVampireManSPK[1], 30, 60);
	CHECK(scene.Same());
}

TEST(TopView4AdvancedVampire, FakeDyingBothLayersGoThroughTheNetInTheBodysColours)
{
	Scene scene;
	Wearer vampire;
	vampire.Become(kVampire);
	vampire.Advance();
	vampire.m_bFakeDie = true;
	vampire.m_FakeDieCount = 12;
	Place(scene.view.m_AdvancementVampireManFPK, 0, kStop, 3, 0, 1, 5, 6);
	Place(scene.view.m_AdvancementVampireManFPK, 1, kStop, 3, 0, 2, 25, 9);
	CSpriteSurface::s_Value1 = 99;
	DrawAdvancedVampire(scene, vampire, ACTION_STAND, 3, 0);
	scene.Blit(5, 6, scene.view.m_AdvancementVampireManSPK[1], 30, 60);
	scene.Blit(25, 9, scene.view.m_AdvancementVampireManSPK[2], 30, 60);
	CHECK(scene.Same());
	CHECK_EQ(2, CSpriteSurface::s_Value1);
	CHECK(IndexEffect::Get() == IndexNet());
	// In an effect colour both take the colour-set blit.
	vampire.m_AttachEffectColor = 5;
	CSpriteSurface::s_Value1 = 99;
	Scene::ResetSets();
	scene.Clear();
	DrawAdvancedVampire(scene, vampire, ACTION_STAND, 3, 0);
	scene.ColorSet(5, 6, scene.view.m_AdvancementVampireManSPK[1], 5);
	scene.ColorSet(25, 9, scene.view.m_AdvancementVampireManSPK[2], 5);
	CHECK(scene.Same());
	CHECK_EQ(5, CSpriteSurface::s_Value1);
}

TEST(TopView4AdvancedVampire, InvisibleBothLayersWipeOut)
{
	Scene scene;
	Wearer vampire;
	vampire.Become(kVampire);
	vampire.Advance();
	vampire.m_InvisibleCount = 20;
	Place(scene.view.m_AdvancementVampireManFPK, 0, kStop, 3, 0, 1, 5, 6);
	Place(scene.view.m_AdvancementVampireManFPK, 1, kStop, 3, 0, 2, 25, 9);
	DrawAdvancedVampire(scene, vampire, ACTION_STAND, 3, 0);
	scene.Wipe(5, 6, scene.view.m_AdvancementVampireManSPK[1], 30, 60, 20);
	scene.Wipe(25, 9, scene.view.m_AdvancementVampireManSPK[2], 30, 60, 20);
	CHECK(scene.Same());
	CHECK_EQ(20, CSpriteSurface::s_Value1);
}

TEST(TopView4AdvancedVampire, MovingFastBothLayersLeaveCopiesAndACutOneSinks)
{
	Scene scene;
	Wearer vampire;
	vampire.Become(kVampire);
	vampire.Advance();
	vampire.m_bFastMove = true;
	vampire.m_cX = 8;
	vampire.m_cY = 4;
	Place(scene.view.m_AdvancementVampireManFPK, 0, kStop, 3, 0, 1, 5, 6);
	Place(scene.view.m_AdvancementVampireManFPK, 1, kStop, 3, 0, 2, 45, 30);
	DrawAdvancedVampire(scene, vampire, ACTION_STAND, 3, 0);
	// Each layer's farther copy darkened by two bits, the nearer by one.
	for (int n = 2; n > 0; --n) scene.Darkened(n, 5 - 4 * n, 6 - 2 * n, scene.view.m_AdvancementVampireManSPK[1], 30, 60);
	scene.Blit(5, 6, scene.view.m_AdvancementVampireManSPK[1], 30, 60);
	for (int n = 2; n > 0; --n) scene.Darkened(n, 45 - 4 * n, 30 - 2 * n, scene.view.m_AdvancementVampireManSPK[2], 30, 60);
	scene.Blit(45, 30, scene.view.m_AdvancementVampireManSPK[2], 30, 60);
	CHECK(scene.Same());
	vampire.m_bFastMove = false;
	vampire.m_bCutHeight = true;
	vampire.m_CutHeightCount = 30;
	scene.Clear();
	DrawAdvancedVampire(scene, vampire, ACTION_STAND, 3, 0);
	scene.Blit(5, 12, scene.view.m_AdvancementVampireManSPK[1], 30, 60);
	scene.Blit(45, 36, scene.view.m_AdvancementVampireManSPK[2], 30, 60);
	CHECK(scene.Same());
}

TEST(TopView4AdvancedVampire, BloodyZenithCountsDownOnceForEachLayer)
{
	// Recorded defect: the body and the weapon each call
	// UpdateBloodyZenith, so an advanced vampire's count falls by two a draw
	// and its weapon has one copy fewer than its body.
	Scene scene;
	Wearer vampire;
	vampire.Become(kVampire);
	vampire.Advance();
	vampire.m_bBloodyZenith = true;
	vampire.m_BloodyZenithCount = 3;
	vampire.m_MoveCount = 0;
	vampire.m_MoveCountMax = 6;
	vampire.m_cX = 4;									// copy n is 2n left, so each shows
	Place(scene.view.m_AdvancementVampireManFPK, 0, kStop, 3, 0, 1, 5, 6);
	Place(scene.view.m_AdvancementVampireManFPK, 1, kStop, 3, 0, 2, 45, 30);
	DrawAdvancedVampire(scene, vampire, ACTION_STAND, 3, 0);
	// Move count 0: no rise. Copy n is darkened by min(2, n) bits; n 0 is
	// the layer itself.
	for (int n = 3; n >= 0; --n) scene.Darkened((std::min)(2, n), 5 - 2 * n, 6, scene.view.m_AdvancementVampireManSPK[1], 30, 60);
	for (int n = 2; n >= 0; --n) scene.Darkened((std::min)(2, n), 45 - 2 * n, 30, scene.view.m_AdvancementVampireManSPK[2], 30, 60);
	CHECK(scene.Same());
	CHECK_EQ(1, vampire.GetBloodyZenithCount());
}

TEST(TopView4AdvancedVampire, ACasketAsThePlainVampiresAndNoBodyWhileWhole)
{
	Scene scene;
	Wearer vampire;
	vampire.Become(kVampire);
	vampire.Advance();
	vampire.m_bInCasket = true;
	vampire.m_CasketType = 0;
	vampire.m_CasketCount = 0;
	Place(scene.view.m_AdvancementVampireManFPK, 0, kStop, 3, 0, 1, 5, 6);
	Place(scene.view.m_AdvancementVampireManFPK, 1, kStop, 3, 0, 2, 25, 9);
	CSprite& casket = scene.view.m_EtcSPK[SPRITEID_CASKET_1];
	vampire.ClearScreenRect();
	DrawAdvancedVampire(scene, vampire, ACTION_STAND, 3, 0);
	scene.Blit(24 - (casket.GetWidth() >> 1), 24 - casket.GetHeight(), casket);
	CHECK(scene.Same());
	CHECK(SameRect(vampire.GetScreenRect(), 64 - (casket.GetWidth() >> 1), 74 - casket.GetHeight(),
		64 - (casket.GetWidth() >> 1) + casket.GetWidth(), 74));
	for (int count : {64, 20})
	{
		CSpriteSurface::s_Value1 = 7;
		vampire.m_CasketCount = count;
		scene.Clear();
		DrawAdvancedVampire(scene, vampire, ACTION_STAND, 3, 0);
		scene.Blit(5, 6, scene.view.m_AdvancementVampireManSPK[1], 30, 60);
		scene.Blit(25, 9, scene.view.m_AdvancementVampireManSPK[2], 30, 60);
		scene.WipeSprite(24 - (casket.GetWidth() >> 1), 24 - casket.GetHeight(), casket, count == 64 ? 7 : count);
		CHECK(scene.Same());
		CHECK_EQ(count == 64 ? 7 : count, CSpriteSurface::s_Value1);
	}
}

//----------------------------------------------------------------------
// The selected drawings: parts to the outline manager
//----------------------------------------------------------------------

TEST(TopView4SelectedSlayer, PartsGoToTheOutlinesInTheCreaturesOrder)
{
	for (int facing : {0, 2})
	{
		Scene scene;
		Wearer slayer;
		slayer.Become(kSlayer);
		slayer.SetCurrentDirection(BYTE(facing));
		slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, UNIQUE_ITEM_COLOR);
		slayer.Wear(ADDON_LEFTHAND, 3, ITEM_CLASS_SHIELD, 40, QUEST_ITEM_COLOR);
		MCreatureWear::ADDON_INFO& sword = slayer.Wear(ADDON_RIGHTHAND, 2, ITEM_CLASS_SWORD, 41, 51);
		sword.bEffectColor = TRUE;
		sword.EffectColorSet = 77;
		Place(scene.view.m_AddonFPK, 1, ACTION_ATTACK, 4, 0, 1, 2, 3);
		Place(scene.view.m_AddonFPK, 3, ACTION_ATTACK, 4, 0, 3, 14, 20);
		Place(scene.view.m_AddonFPK, 2, ACTION_ATTACK, 4, 0, 2, 30, 25);
		slayer.ClearScreenRect();
		scene.Call([&](POINT* p) { Access::SelectedSlayer(scene.view, p, &slayer, ACTION_ATTACK, 4, 0); });
		CHECK(scene.drawn.Blank());
		const Entry coat = {42, 53, &scene.view.m_AddonSPK[1], 0};
		const Entry shield = {54, 70, &scene.view.m_AddonSPK[3], 0};
		const Entry swordEntry = {70, 75, &scene.view.m_AddonSPK[2], 77};
		if (facing == 2) CHECK(SameEntries(scene.view.m_SOM, {coat, shield, swordEntry}));
		else CHECK(SameEntries(scene.view.m_SOM, {coat, swordEntry, shield}));
		// The sets the last plain part set: the quest shield's, in both.
		CHECK(SetsAre(Special(QUEST_ITEM_COLOR), Special(QUEST_ITEM_COLOR)));
		CHECK(SameRect(slayer.GetScreenRect(), 42, 53, 75, 76));
	}
}

TEST(TopView4SelectedSlayer, ACoatKeepsItsFirstSetAndAPastFrameAddsNothing)
{
	Scene scene;
	Wearer slayer;
	slayer.Become(kSlayer);
	slayer.SetCurrentDirection(2);
	slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, QUEST_ITEM_COLOR);
	Place(scene.view.m_AddonFPK, 1, ACTION_ATTACK, 2, 0, 1, 2, 3);
	scene.Call([&](POINT* p) { Access::SelectedSlayer(scene.view, p, &slayer, ACTION_ATTACK, 2, 0); });
	CHECK(SetsAre(30, Special(QUEST_ITEM_COLOR)));
	CHECK(SameEntries(scene.view.m_SOM, {{42, 53, &scene.view.m_AddonSPK[1], 0}}));
	scene.view.m_SOM.Clear();
	slayer.ClearScreenRect();
	scene.Call([&](POINT* p) { Access::SelectedSlayer(scene.view, p, &slayer, ACTION_ATTACK, 2, 1); });
	CHECK(scene.view.m_SOM.GetOutlines().empty());
	CHECK(Cleared(slayer.GetScreenRect()));
}

TEST(TopView4SelectedSlayer, AnAdvancedSlayersPartsAndNewItemsGoToTheOutlines)
{
	for (bool male : {true, false})
		for (int direction : {0, 2})
		{
			Scene scene;
			Wearer slayer;
			slayer.Become(kSlayer);
			slayer.Advance();
			slayer.m_bMale = male;
			slayer.SetCurrentDirection(BYTE(2 - direction));
			slayer.Wear(ADDON_COAT, 5, ITEM_CLASS_COAT, 30, UNIQUE_ITEM_COLOR, 28);
			slayer.Wear(ADDON_LEFTHAND, 1, ITEM_CLASS_SHIELD, 40, 50);
			MCreatureWear::ADDON_INFO& sword = slayer.Wear(ADDON_RIGHTHAND, 2, ITEM_CLASS_SWORD, 41, 51);
			sword.bEffectColor = TRUE;
			sword.EffectColorSet = 77;
			CCreatureFramePack& pack = male ? scene.view.m_AdvancementSlayerManFPK : scene.view.m_AdvancementSlayerWomanFPK;
			CIndexSpritePack& sprites = male ? scene.view.m_AdvancementSlayerManSPK : scene.view.m_AdvancementSlayerWomanSPK;
			const int action = AdvancedMagic(slayer), newAction = ConvNewSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer);
			Place(pack, 5, newAction, direction, 0, 0, 2, 3);
			Place(pack, 6, newAction, direction, 0, 1, 26, 3);
			Place(pack, AC_SHIELD, action, direction, 0, 2, 14, 20);
			Place(pack, AC_SWORD, action, direction, 0, 3, 30, 25);
			slayer.ClearScreenRect();
			scene.Call([&](POINT* p) { Access::SelectedAdvancedSlayer(scene.view, p, &slayer, ACTION_MAGIC, direction, 0); });
			CHECK(scene.drawn.Blank());
			const Entry body1 = {42, 53, &sprites[0], 0}, body2 = {66, 53, &sprites[1], 0};
			const Entry shield = {54, 70, &sprites[2], 0}, swordEntry = {70, 75, &sprites[3], 77};
			if (direction == 2) CHECK(SameEntries(scene.view.m_SOM, {body1, body2, shield, swordEntry}));
			else CHECK(SameEntries(scene.view.m_SOM, {body1, body2, swordEntry, shield}));
			CHECK(SetsAre(40, 50));
			CHECK(SameRect(slayer.GetScreenRect(), 42, 53, 75, 76));
		}
}

TEST(TopView4SelectedSlayer, AnAdvancedSlayerSkipsWhatItHasNotGot)
{
	Scene scene;
	Wearer slayer;
	slayer.Become(kSlayer);
	slayer.Advance();
	slayer.SetCurrentDirection(2);
	slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT, 30, QUEST_ITEM_COLOR);
	// No advanced attack: nothing.
	for (int action = 0; action < kAdvancedActions; ++action)
		Place(scene.view.m_AdvancementSlayerManFPK, AC_BODY, action, 2, 0, 1, 2, 3);
	scene.Call([&](POINT* p) { Access::SelectedAdvancedSlayer(scene.view, p, &slayer, ACTION_ATTACK, 2, 0); });
	CHECK(scene.view.m_SOM.GetOutlines().empty());
	// A coat's part keeps its first set; a past frame adds nothing.
	scene.Call([&](POINT* p) { Access::SelectedAdvancedSlayer(scene.view, p, &slayer, ACTION_MAGIC, 2, 0); });
	CHECK(SameEntries(scene.view.m_SOM, {{42, 53, &scene.view.m_AdvancementSlayerManSPK[1], 0}}));
	CHECK(SetsAre(30, Special(QUEST_ITEM_COLOR)));
	scene.view.m_SOM.Clear();
	scene.Call([&](POINT* p) { Access::SelectedAdvancedSlayer(scene.view, p, &slayer, ACTION_MAGIC, 2, 1); });
	CHECK(scene.view.m_SOM.GetOutlines().empty());
	// A new coat whose second frame set is past the pack, or whose frame
	// set lacks the action: skipped.
	const int newAction = ConvNewSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer);
	slayer.Wear(ADDON_COAT, kIds - 1, ITEM_CLASS_COAT, 30, 60, 28);
	Place(scene.view.m_AdvancementSlayerManFPK, kIds - 1, newAction, 2, 0, 2, 2, 3);
	scene.Call([&](POINT* p) { Access::SelectedAdvancedSlayer(scene.view, p, &slayer, ACTION_MAGIC, 2, 0); });
	CHECK(SameEntries(scene.view.m_SOM, {{42, 53, &scene.view.m_AdvancementSlayerManSPK[2], 0}}));
	// The action the frame set's last: added; one past: not.
	scene.view.m_SOM.Clear();
	scene.view.m_AdvancementSlayerManFPK[kIds - 1].Init(BYTE(newAction + 1));
	for (int a = 0; a <= newAction; ++a) scene.view.m_AdvancementSlayerManFPK[kIds - 1][BYTE(a)].Init(8);
	Place(scene.view.m_AdvancementSlayerManFPK, kIds - 1, newAction, 2, 0, 2, 2, 3);
	scene.Call([&](POINT* p) { Access::SelectedAdvancedSlayer(scene.view, p, &slayer, ACTION_MAGIC, 2, 0); });
	CHECK(SameEntries(scene.view.m_SOM, {{42, 53, &scene.view.m_AdvancementSlayerManSPK[2], 0}}));
	scene.view.m_SOM.Clear();
	scene.view.m_AdvancementSlayerManFPK[kIds - 1].Init(BYTE(newAction));
	scene.Call([&](POINT* p) { Access::SelectedAdvancedSlayer(scene.view, p, &slayer, ACTION_MAGIC, 2, 0); });
	CHECK(scene.view.m_SOM.GetOutlines().empty());
}

TEST(TopView4SelectedOusters, TheCoatThenTheChakramGoToTheOutlines)
{
	for (bool advanced : {false, true})
	{
		Scene scene;
		Wearer ousters;
		ousters.Become(kOusters);
		if (advanced) ousters.Advance();
		ousters.SetCurrentDirection(2);
		ousters.SetBodyColor1(11);
		ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT, 30, UNIQUE_ITEM_COLOR);
		ousters.Wear(ADDON_TROUSER, 2, ITEM_CLASS_OUSTERS_BOOTS, 33, 70);
		ousters.Wear(ADDON_RIGHTHAND, 3, ITEM_CLASS_OUSTERS_CHAKRAM, QUEST_ITEM_COLOR, 80);
		CCreatureFramePack& pack = advanced ? scene.view.m_AdvancementOustersFPK : scene.view.m_OustersFPK;
		CIndexSpritePack& sprites = advanced ? scene.view.m_AdvancementOustersSPK : scene.view.m_OustersSPK;
		const int action = advanced ? ACTION_ADVANCEMENT_OUSTERS_CHAKRAM_STOP - 100 : ACTION_STAND;
		Place(pack, 1, action, 2, 0, 1, 2, 3);
		Place(pack, advanced ? 0 : 3, action, 2, 0, 2, 22, 13);
		ousters.ClearScreenRect();
		scene.Call([&](POINT* p) {
			if (advanced) Access::SelectedAdvancedOusters(scene.view, p, &ousters, ACTION_STAND, 2, 0);
			else Access::SelectedOusters(scene.view, p, &ousters, ACTION_STAND, 2, 0);
		});
		CHECK(scene.drawn.Blank());
		CHECK(SameEntries(scene.view.m_SOM, {{42, 53, &sprites[1], 0}, {62, 63, &sprites[2], 0}}));
		// The chakram's sets over the coat's arm and boots.
		CHECK(SetsAre(Special(QUEST_ITEM_COLOR), 80, 80, 70));
		CHECK(SameRect(ousters.GetScreenRect(), 42, 53, 47, 54));
		// Only the coat's sets when the chakram is a ghost.
		ousters.SetGhost(2, 10);
		scene.view.m_SOM.Clear();
		Scene::ResetSets();
		scene.Call([&](POINT* p) {
			if (advanced) Access::SelectedAdvancedOusters(scene.view, p, &ousters, ACTION_STAND, 2, 0);
			else Access::SelectedOusters(scene.view, p, &ousters, ACTION_STAND, 2, 0);
		});
		CHECK(SameEntries(scene.view.m_SOM, {{42, 53, &sprites[1], 0}}));
		CHECK(SetsAre(11, Special(UNIQUE_ITEM_COLOR), 80, 70));
		ousters.SetGhost(1, 10);
		scene.view.m_SOM.Clear();
		scene.Call([&](POINT* p) {
			if (advanced) Access::SelectedAdvancedOusters(scene.view, p, &ousters, ACTION_STAND, 2, 0);
			else Access::SelectedOusters(scene.view, p, &ousters, ACTION_STAND, 2, 0);
		});
		CHECK(SameEntries(scene.view.m_SOM, {{62, 63, &sprites[2], 0}}));
	}
}

TEST(TopView4SelectedOusters, AWristletOrBareHandStandsAsOustersWithTheArmsColour)
{
	for (bool advanced : {false, true})
		for (bool wristlet : {true, false})
		{
			Scene scene;
			Wearer ousters;
			ousters.Become(kOusters);
			if (advanced) ousters.Advance();
			ousters.SetCurrentDirection(2);
			ousters.SetBodyColor1(11);
			ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT, 30, 60);
			if (wristlet) ousters.Wear(ADDON_RIGHTHAND, 3, ITEM_CLASS_OUSTERS_WRISTLET, 44, QUEST_ITEM_COLOR);
			CCreatureFramePack& pack = advanced ? scene.view.m_AdvancementOustersFPK : scene.view.m_OustersFPK;
			CIndexSpritePack& sprites = advanced ? scene.view.m_AdvancementOustersSPK : scene.view.m_OustersSPK;
			Place(pack, 1, advanced ? ACTION_ADVANCEMENT_OUSTERS_WRISTLET_MOVE - 100 : ACTION_OUSTERS_MOVE, 2, 0, 1, 2, 3);
			scene.Call([&](POINT* p) {
				if (advanced) Access::SelectedAdvancedOusters(scene.view, p, &ousters, ACTION_MOVE, 2, 0);
				else Access::SelectedOusters(scene.view, p, &ousters, ACTION_MOVE, 2, 0);
			});
			CHECK(SameEntries(scene.view.m_SOM, {{42, 53, &sprites[1], 0}}));
			CHECK(SetsAre(11, 60, wristlet ? Special(QUEST_ITEM_COLOR) : defaultCoatColor, defaultCoatColor));
		}
}

TEST(TopView4SelectedOusters, EffectColouredPartsCarryTheCoatsEffectColour)
{
	// Recorded defect: the chakram in an effect colour is added with the
	// coat's EffectColorSet (MTopViewDraw.cpp 1344, 3838); the drawings
	// that are not selected use the chakram's own.
	for (bool advanced : {false, true})
	{
		Scene scene;
		Wearer ousters;
		ousters.Become(kOusters);
		if (advanced) ousters.Advance();
		ousters.SetCurrentDirection(2);
		MCreatureWear::ADDON_INFO& coat = ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT, 30, 60);
		MCreatureWear::ADDON_INFO& chakram = ousters.Wear(ADDON_RIGHTHAND, 3, ITEM_CLASS_OUSTERS_CHAKRAM, 44, 80);
		coat.EffectColorSet = 55;
		chakram.bEffectColor = TRUE;
		chakram.EffectColorSet = 78;
		CCreatureFramePack& pack = advanced ? scene.view.m_AdvancementOustersFPK : scene.view.m_OustersFPK;
		CIndexSpritePack& sprites = advanced ? scene.view.m_AdvancementOustersSPK : scene.view.m_OustersSPK;
		const int action = advanced ? ACTION_ADVANCEMENT_OUSTERS_CHAKRAM_STOP - 100 : ACTION_STAND;
		Place(pack, 1, action, 2, 0, 1, 2, 3);
		Place(pack, advanced ? 0 : 3, action, 2, 0, 2, 22, 13);
		scene.Call([&](POINT* p) {
			if (advanced) Access::SelectedAdvancedOusters(scene.view, p, &ousters, ACTION_STAND, 2, 0);
			else Access::SelectedOusters(scene.view, p, &ousters, ACTION_STAND, 2, 0);
		});
		CHECK(SameEntries(scene.view.m_SOM, {{42, 53, &sprites[1], 0}, {62, 63, &sprites[2], 55}}));
		// The coat in its own effect colour, with no sets changed.
		coat.bEffectColor = TRUE;
		chakram.bEffectColor = FALSE;
		scene.view.m_SOM.Clear();
		Scene::ResetSets();
		ousters.SetGhost(2, 10);
		scene.Call([&](POINT* p) {
			if (advanced) Access::SelectedAdvancedOusters(scene.view, p, &ousters, ACTION_STAND, 2, 0);
			else Access::SelectedOusters(scene.view, p, &ousters, ACTION_STAND, 2, 0);
		});
		CHECK(SameEntries(scene.view.m_SOM, {{42, 53, &sprites[1], 55}}));
		CHECK(SetsAre(1, 2));
	}
}

TEST(TopView4SelectedOusters, AnAdvancedOustersWithoutAnAdvancedActionAddsNothing)
{
	Scene scene;
	Wearer ousters;
	ousters.Become(kOusters);
	ousters.Advance();
	ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT, 30, 60);
	for (int action = 0; action < kAdvancedActions; ++action)
		Place(scene.view.m_AdvancementOustersFPK, 1, action, 2, 0, 1, 2, 3);
	scene.Call([&](POINT* p) { Access::SelectedAdvancedOusters(scene.view, p, &ousters, 18, 2, 0); });
	CHECK(scene.view.m_SOM.GetOutlines().empty());
	scene.Call([&](POINT* p) { Access::SelectedAdvancedOusters(scene.view, p, &ousters, ACTION_OUSTERS_CHAKRAM_FAST, 2, 0); });
	CHECK_EQ(size_t(1), scene.view.m_SOM.GetOutlines().size());
}

TEST(TopView4SelectedVampire, TheBodyGoesToTheOutlinesAndItsAfterimagesToTheSurface)
{
	Scene scene;
	Wearer monster;
	monster.Become(kMonster);
	monster.SetBodyColor1(UNIQUE_ITEM_COLOR);
	monster.m_ShadowCount = 2;
	monster.m_cX = 4;
	monster.m_cY = 2;
	Frames(scene.view.m_CreatureFPK, kBody, ACTION_MOVE, 3, {{2, 30, 20}, {1, 5, 6}, {0, 70, 0}, {3, 60, 40}});
	monster.ClearScreenRect();
	scene.Call([&](POINT* p) { Access::SelectedVampire(scene.view, p, &monster, ACTION_MOVE, 3, 1, kBody, 0); });
	CHECK(SameEntries(scene.view.m_SOM, {{45, 56, &scene.Body(1), 0}}));
	CHECK(SetsAre(Special(UNIQUE_ITEM_COLOR), 60));
	// The afterimages a bit darker.
	scene.Darkened(1, 30, 20, scene.Body(2), Special(UNIQUE_ITEM_COLOR), 60);
	scene.Darkened(1, 60 - 4, 40 - 2, scene.Body(3), Special(UNIQUE_ITEM_COLOR), 60);
	CHECK(scene.Same());
	CHECK(SameRect(monster.GetScreenRect(), 45, 56, 50, 57));
	// Frame index 1 leaves the rect; a past frame adds nothing.
	monster.ClearScreenRect();
	scene.view.m_SOM.Clear();
	scene.Call([&](POINT* p) { Access::SelectedVampire(scene.view, p, &monster, ACTION_MOVE, 3, 1, kBody, 1); });
	CHECK(Cleared(monster.GetScreenRect()));
	CHECK_EQ(size_t(1), scene.view.m_SOM.GetOutlines().size());
	scene.view.m_SOM.Clear();
	scene.Call([&](POINT* p) { Access::SelectedVampire(scene.view, p, &monster, ACTION_MOVE, 3, 4, kBody, 0); });
	CHECK(scene.view.m_SOM.GetOutlines().empty());
}

TEST(TopView4SelectedVampire, AnEffectOrBatColourIsTheEntrysOwn)
{
	Scene scene;
	scene.Monster(CREATURETYPE_BAT);
	Wearer bat;
	bat.Become(CREATURETYPE_BAT);
	bat.SetBatColor(9);
	Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
	scene.Call([&](POINT* p) { Access::SelectedVampire(scene.view, p, &bat, ACTION_STAND, 3, 0, kBody, 0); });
	CHECK(SameEntries(scene.view.m_SOM, {{45, 56, &scene.Body(1), 9}}));
	CHECK(SetsAre(1, 2));
	bat.SetBatColor(0xFFFF);
	bat.m_AttachEffectColor = 5;
	scene.view.m_SOM.Clear();
	scene.Call([&](POINT* p) { Access::SelectedVampire(scene.view, p, &bat, ACTION_STAND, 3, 0, kBody, 0); });
	CHECK(SameEntries(scene.view.m_SOM, {{45, 56, &scene.Body(1), 5}}));
	Wearer monster;
	monster.Become(kMonster);
	monster.SetBatColor(9);
	scene.view.m_SOM.Clear();
	scene.Call([&](POINT* p) { Access::SelectedVampire(scene.view, p, &monster, ACTION_STAND, 3, 0, kBody, 0); });
	CHECK(SameEntries(scene.view.m_SOM, {{45, 56, &scene.Body(1), 0}}));
	CHECK(SetsAre(30, 60));
}

TEST(TopView4SelectedVampire, ACasketIsAlwaysOutlinedAndHidesTheBodyWhileWhole)
{
	for (bool advanced : {false, true})
	{
		Scene scene;
		Wearer vampire;
		vampire.Become(advanced ? kVampire : kMonster);
		if (advanced) vampire.Advance();
		vampire.m_bInCasket = true;
		vampire.m_CasketType = 1;
		vampire.m_CasketCount = 0;
		Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
		Place(scene.view.m_AdvancementVampireManFPK, 0, kStop, 3, 0, 1, 5, 6);
		CSprite& casket = scene.view.m_EtcSPK[SPRITEID_CASKET_1 + 2];
		auto draw = [&](int index) {
			scene.view.m_SOM.Clear();
			scene.Call([&](POINT* p) {
				if (advanced) Access::SelectedAdvancedVampire(scene.view, p, &vampire, ACTION_STAND, 3, 0, 0, index);
				else Access::SelectedVampire(scene.view, p, &vampire, ACTION_STAND, 3, 0, kBody, index);
			});
		};
		vampire.ClearScreenRect();
		draw(0);
		CHECK(SameEntries(scene.view.m_SOM, {{63, 73, &casket, 0}}));
		CHECK(SameRect(vampire.GetScreenRect(), 63, 73, 66, 74));
		vampire.m_CasketCount = 20;
		draw(0);
		CIndexSprite& body = advanced ? scene.view.m_AdvancementVampireManSPK[1] : scene.Body(1);
		CHECK(SameEntries(scene.view.m_SOM, {{45, 56, &body, 0}, {63, 73, &casket, 0}}));
		vampire.ClearScreenRect();
		draw(1);
		CHECK(Cleared(vampire.GetScreenRect()));
		CHECK(scene.drawn.Blank());
	}
}

TEST(TopView4SelectedVampire, AnAdvancedVampiresBodyAndWeaponGoToTheOutlines)
{
	for (bool male : {true, false})
	{
		Scene scene;
		Wearer vampire;
		vampire.Become(kVampire);
		vampire.Advance();
		vampire.m_bMale = male;
		vampire.m_ShadowCount = 1;
		CCreatureFramePack& pack = male ? scene.view.m_AdvancementVampireManFPK : scene.view.m_AdvancementVampireWomanFPK;
		CIndexSpritePack& sprites = male ? scene.view.m_AdvancementVampireManSPK : scene.view.m_AdvancementVampireWomanSPK;
		Frames(pack, 0, kStop, 3, {{2, 30, 20}, {1, 5, 6}});
		Frames(pack, 1, kStop, 3, {{3, 60, 40}, {0, 25, 9}});
		vampire.ClearScreenRect();
		scene.Call([&](POINT* p) { Access::SelectedAdvancedVampire(scene.view, p, &vampire, ACTION_STAND, 3, 1, 0, 0); });
		CHECK(SameEntries(scene.view.m_SOM, {{45, 56, &sprites[1], 0}, {65, 59, &sprites[0], 0}}));
		CHECK(SameRect(vampire.GetScreenRect(), 45, 56, 50, 57));
		// The afterimages, from the advanced pack, straight to the surface,
		// a bit darker.
		scene.Darkened(1, 30, 20, sprites[2], 30, 60);
		scene.Darkened(1, 60, 40, sprites[3], 30, 60);
		CHECK(scene.Same());
		// From frame set 1 on: the weapon and set 2.
		Frames(pack, 2, kStop, 3, {{2, 30, 20}, {2, 7, 7}});
		vampire.m_ShadowCount = 0;
		scene.view.m_SOM.Clear();
		scene.Call([&](POINT* p) { Access::SelectedAdvancedVampire(scene.view, p, &vampire, ACTION_STAND, 3, 1, 1, 0); });
		CHECK(SameEntries(scene.view.m_SOM, {{65, 59, &sprites[0], 0}, {47, 57, &sprites[2], 0}}));
	}
}

TEST(TopView4SelectedVampire, AnAdvancedVampireWithoutAnAdvancedActionAddsNothing)
{
	Scene scene;
	Wearer vampire;
	vampire.Become(kVampire);
	vampire.Advance();
	for (int action = 0; action < kAdvancedActions; ++action)
		Place(scene.view.m_AdvancementVampireManFPK, 0, action, 3, 0, 1, 5, 6);
	scene.Call([&](POINT* p) { Access::SelectedAdvancedVampire(scene.view, p, &vampire, 17, 3, 0, 0, 0); });
	CHECK(scene.view.m_SOM.GetOutlines().empty());
}

TEST(TopView4SelectedVampire, Types807To812AddOneFrameSetTwiceOnTheUnconvertedAction)
{
	// Recorded defect: the selected drawing skips GetNewVampireAction
	// (magic stays 12, not 7) and fixes the frame set for both passes, so
	// the body is added twice and no weapon.
	Scene scene;
	scene.Monster(808);
	Wearer vampire;
	vampire.Become(808);
	vampire.Advance();
	vampire.m_bMale = false;
	const int magic = ACTION_ADVANCEMENT_MAGIC - ADVANCEMENT_ACTION_START;
	Place(scene.view.m_AdvancementVampireManFPK, 3, magic, 3, 0, 1, 5, 6);
	Place(scene.view.m_AdvancementVampireManFPK, 3, 7, 3, 0, 2, 5, 6);
	Place(scene.view.m_AdvancementVampireManFPK, 1, magic, 3, 0, 3, 25, 9);
	scene.Call([&](POINT* p) { Access::SelectedAdvancedVampire(scene.view, p, &vampire, ACTION_MAGIC, 3, 0, 0, 0); });
	CIndexSprite* body = &scene.view.m_AdvancementVampireManSPK[1];
	CHECK(SameEntries(scene.view.m_SOM, {{45, 56, body, 0}, {45, 56, body, 0}}));
}

TEST(TopView4SelectedVampire, ASelectedAdvancedBatIsDrawnFromTheAdvancedPack)
{
	// Recorded defect: DrawCreature sends an advanced bat, wolf or werwolf
	// to the plain body drawing (878-882), but when selected to the
	// advanced one (741-745), which draws it from the advanced packs.
	for (bool selected : {false, true})
	{
		Scene scene;
		scene.Monster(CREATURETYPE_BAT);
		Wearer bat;
		bat.Become(CREATURETYPE_BAT);
		bat.Advance();
		bat.m_bMale = true;
		bat.SetCurrentDirection(3);
		Place(scene.view.m_CreatureFPK, kBody, ACTION_STAND, 3, 0, 1, 5, 6);
		Place(scene.view.m_AdvancementVampireManFPK, 0, kStop, 3, 0, 2, 25, 9);
		if (selected) scene.view.m_SelectCreatureID = kCreatureID;
		scene.Draw(bat);
		if (selected) scene.Blit(25, 9, scene.view.m_AdvancementVampireManSPK[2], 30, 60);
		else scene.Blit(5, 6, scene.Body(1), 30, 60);
		CHECK(scene.Same());
	}
}

//----------------------------------------------------------------------
// Where the guild mark of a siege goes
//----------------------------------------------------------------------
namespace {

struct GuildMarks : MGuildMarkManager
{
	GuildMarks() : MGuildMarkManager("top_view_4_none", "top_view_4_none", "top_view_4_none", "top_view_4_none",
		"top_view_4_none", "top_view_4_none") {}
	// The small mark of the guild, owned by the manager.
	void Add(WORD guild, int n)
	{
		CSprite* pSprite = new CSprite565;
		LoadSprite(*pSprite, n);
		m_GuildMarks[guild] = GUILD_SPRITES(nullptr, pSprite);
	}
};

} // namespace

TEST(TopView4GuildMark, ASiegersMarkSitsAboveWhereItsChatWouldBe)
{
	// Its name's height above it, less a party member's bar; not for the
	// creature under the cursor (it is named elsewhere).
	for (int kind : {0, 1, 2})
	{
		Scene scene;
		GuildMarks marks;
		marks.Add(7, 3);
		MGuildMarkManager* oldMarks = g_pGuildMarkManager;
		g_pGuildMarkManager = &marks;
		scene.view.m_FirstZonePixel.x = 200;
		scene.at.y = 100;
		scene.player->SetSiegeWar(1);
		Wearer sieger;
		sieger.Become(kMonster);
		sieger.MObject::SetPosition(5, 6);
		sieger.SetSiegeWar(3);
		sieger.SetGuildNumber(7);
		if (kind == 1) sieger.SetPlayerParty();
		if (kind == 2) scene.view.m_SelectCreatureID = kCreatureID;
		scene.Draw(sieger);
		// The mark is sprite 3's double.
		CSprite& mark = scene.view.m_EtcSPK[3];
		const int markX = sieger.GetPixelX() - 200 + 15;
		const int markY = scene.at.y - 30 - (kind == 1 ? int(scene.view.m_EtcSPK[SPRITEID_PARTY_HP].GetHeight()) : 0) - 23;
		if (kind == 1)
		{
			CSpriteSurface* pExpected = &scene.expected.surface;
			scene.view.SetSurface(pExpected);
			POINT bar = { scene.at.x, scene.at.y - 30 };
			scene.expected.surface.Lock();
			Access::PartyHP(scene.view, &bar, &sieger);
			scene.expected.surface.Unlock();
			CSpriteSurface* pDrawn = &scene.drawn.surface;
			scene.view.SetSurface(pDrawn);
		}
		if (kind != 2) scene.expected.Blit(markX, markY, mark);
		if (kind == 2) CHECK(scene.drawn.Blank());
		else CHECK(scene.Same());
		g_pGuildMarkManager = oldMarks;
	}
}

//----------------------------------------------------------------------
// The advanced slayer's new items, class by class
//----------------------------------------------------------------------
TEST(TopView4AdvancedSlayer, EachNewItemClassFromItsOwnType)
{
	// At its threshold an item is new (its frame ID, the new action; a
	// coat two frame sets), one type below it is not (its part, the
	// advanced action); drawn or outlined. Frame ID 0 is a frame set too.
	struct Case { int slot; ITEM_CLASS itemClass; int threshold; };
	for (const Case& c : {Case{ADDON_COAT, ITEM_CLASS_COAT, 28}, Case{ADDON_RIGHTHAND, ITEM_CLASS_SWORD, 16},
		Case{ADDON_RIGHTHAND, ITEM_CLASS_BLADE, 16}, Case{ADDON_LEFTHAND, ITEM_CLASS_SHIELD, 15},
		Case{ADDON_RIGHTHAND, ITEM_CLASS_CROSS, 14}, Case{ADDON_HELM, ITEM_CLASS_HELM, 15},
		Case{ADDON_RIGHTHAND, ITEM_CLASS_AR, 16}, Case{ADDON_RIGHTHAND, ITEM_CLASS_SR, 16},
		Case{ADDON_RIGHTHAND, ITEM_CLASS_MACE, 14}})
		for (int type : {c.threshold, c.threshold - 1})
			for (bool selected : {false, true})
			{
				Scene scene;
				Wearer slayer;
				slayer.Become(kSlayer);
				slayer.Advance();
				const int frameID = c.slot == ADDON_COAT ? 0 : 9;
				MCreatureWear::ADDON_INFO& item = slayer.Wear(c.slot, frameID, c.itemClass, 40, 50, type);
				item.EffectColorSet = 66;
				// The helm in an effect colour shows the entry's own.
				if (c.itemClass == ITEM_CLASS_HELM) item.bEffectColor = TRUE;
				const bool isNew = type == c.threshold;
				const int part = GetAdvancementPartFromItemClass(c.itemClass, TYPE_FRAMEID(frameID));
				const int action = isNew ? ConvNewSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer) : AdvancedMagic(slayer);
				Place(scene.view.m_AdvancementSlayerManFPK, isNew ? frameID : part, action, 2, 0, 1, 2, 3);
				if (isNew && c.slot == ADDON_COAT) Place(scene.view.m_AdvancementSlayerManFPK, frameID + 1, action, 2, 0, 2, 20, 3);
				CIndexSpritePack& sprites = scene.view.m_AdvancementSlayerManSPK;
				const int color = item.bEffectColor ? 66 : 0;
				if (selected)
				{
					scene.Call([&](POINT* p) { Access::SelectedAdvancedSlayer(scene.view, p, &slayer, ACTION_MAGIC, 2, 0); });
					std::vector<Entry> entries = {{42, 53, &sprites[1], color}};
					if (isNew && c.slot == ADDON_COAT) entries.push_back({60, 53, &sprites[2], color});
					const bool same = SameEntries(scene.view.m_SOM, entries);
					if (!same) std::printf("      selected, item class %d type %d\n", int(c.itemClass), type);
					CHECK(same);
				}
				else
				{
					DrawAdvancedSlayer(scene, slayer, ACTION_MAGIC, 2, 0);
					if (item.bEffectColor) scene.ColorSet(2, 3, sprites[1], 66);
					else scene.Blit(2, 3, sprites[1], 40, 50);
					if (isNew && c.slot == ADDON_COAT) scene.Blit(20, 3, sprites[2], 40, 50);
					const bool same = scene.Same();
					if (!same) std::printf("      item class %d type %d\n", int(c.itemClass), type);
					CHECK(same);
				}
			}
}
