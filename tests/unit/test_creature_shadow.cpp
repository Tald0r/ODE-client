#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "CreatureShadowDraw.h"
#include "MCreatureWear.h"
#include "MPlayer.h"
#include "MFakeCreature.h"
#include "MZone.h"
#include "AdvancementAction.h"
#include "ZoneStateHost.h"
#include "MSector.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "EffectSpriteTypeDef.h"
#include "MEventManager.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "UserInformation.h"
#include "FrameClock.h"
#include "AddonDef.h"
#include "SpriteIDDef.h"
#include "MViewDef.h"
#include "Types/ModifyDef.h"
#include "CSpriteSurface.h"
#include "CShadowSprite.h"
#include "CSprite.h"

#include <algorithm>
#include <memory>
#include <cstdio>
#include <fstream>
#include <vector>

// A creature's shadow (task 4.249: MTopView's members until then,
// gameview's since). Shadows are black on a white surface, a blended one
// grey, and each test shadow sprite has a shape of its own, so a picture
// shows which sprite went where and how. Each test draws through the
// moved code onto one surface and lays the blits it should make onto
// another; the two pictures must match.
namespace {

const char* kPath = "creature_shadow_sprite.bin";
const int kSize = 96;

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

// A shadow nine pixels wide whose shape is its id's: pixel 0 and the
// pixels of id's bits (1 to 8) cast shadow.
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
// its offset.
void Place(CCreatureFramePack& pack, int id, int action, int direction, int frame, int sprite, int cx, int cy)
{
	FRAME_ARRAY& frames = pack[TYPE_FRAMEID(id)][BYTE(action)][BYTE(direction)];
	frames.Init(WORD(frame + 1));
	frames[WORD(frame)].Set(TYPE_SPRITEID(sprite), short(cx), short(cy));
}

// Frames 0 to `last` of (id, action, direction), every one the sprite.
void Fill(CCreatureFramePack& pack, int id, int action, int direction, int last, int sprite)
{
	FRAME_ARRAY& frames = pack[TYPE_FRAMEID(id)][BYTE(action)][BYTE(direction)];
	frames.Init(WORD(last + 1));
	for (int frame = 0; frame <= last; ++frame) frames[WORD(frame)].Set(TYPE_SPRITEID(sprite), 4, 5);
}

struct Canvas
{
	CSpriteSurface surface;
	Canvas() { CHECK_EQ(0, spritectl_init()); CHECK(surface.Init(kSize, kSize)); surface.FillSurface(0xFFFF); }
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
	bool Blank() { for (WORD pixel : Picture()) if (pixel != 0xFFFF) return false; return true; }
	// A shadow as the drawing makes it: black, or grey when blended.
	void Shadow(int x, int y, CShadowSprite& sprite, bool blended = false)
	{
		POINT point = { x, y };
		surface.Lock();
		if (blended) surface.BltShadowSpriteDarkness(&point, &sprite, 1);
		else surface.BltShadowSprite(&point, &sprite);
		surface.Unlock();
	}
	void Sprite(int x, int y, CSprite& sprite)
	{
		POINT point = { x, y };
		surface.Lock();
		surface.BltSprite(&point, &sprite);
		surface.Unlock();
	}
};

// A creature whose state a test sets: of the type, bare (the type puts
// its default hair on), alive, at rest on frame 0.
struct Creature : MCreatureWear
{
	using MCreature::m_Action;
	using MCreature::m_ActionCount;
	using MCreature::m_ActionCountMax;
	using MCreature::m_bAlive;
	using MCreature::m_bHasHead;
	using MCreature::m_bCutHeight;
	using MCreature::m_MoveType;
	using MCreature::m_DarknessCount;
	using MCreature::m_bEffectStatus;
	using MCreature::m_bHallu;
	using MCreature::m_HalluCreatureType;
	using MCreature::m_HalluCreatureFrameID;

	void Become(int type)
	{
		SetCreatureType(TYPE_CREATURETYPE(type));
		for (ADDON_INFO& addon : m_Addon) addon.bAddon = FALSE;
		m_ShoulderAddon.bAddon = FALSE;
		Pose(ACTION_STAND, 0);
	}
	void Wear(int slot, int frameID, ITEM_CLASS itemClass)
	{
		ADDON_INFO& addon = m_Addon[slot];
		addon.bAddon = TRUE;
		addon.FrameID = TYPE_FRAMEID(frameID);
		addon.ItemClass = itemClass;
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
	void Die() { m_bAlive = false; }
	void Set(EFFECTSTATUS status) { m_bEffectStatus[status] = true; }
};

// A pet whose pose a test sets.
struct Pet : MFakeCreature
{
	void Pose(int action, int frame)
	{
		m_Action = BYTE(action);
		m_ActionCountMax = 100;
		m_ActionCount = BYTE(frame);
	}
};

struct ZoneProbe : MZone
{
	using MZone::MZone;
	using MZone::m_Info;
	using MZone::m_mapCreature;
};

struct PlayerProbe : MPlayer
{
	using MCreature::m_bEffectStatus;
	using MCreature::m_DarknessCount;
	void Set(EFFECTSTATUS status, bool on = true) { m_bEffectStatus[status] = on; }
};

// Creature types: a slayer, a vampire and an ousters (players), and the
// sprite types: 0 a slayer's and 2 an ousters' (no body of their own), 3
// a body (frame set 1), 4 a player vampire's body (frame set 2).
const int kSlayer = 1, kVampire = 2, kOusters = 3, kPlayerVampire = 4;
const int kBody = 1;

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
	DWORD oldFrame = g_CurrentFrame;
	PlayerProbe* player = nullptr;
	std::unique_ptr<ZoneProbe> zone;

	// The view's shadow packs, each sprite of a pack shaped apart.
	CCreatureFramePack creatureFPK, addonFPK, oustersFPK;
	CShadowSpritePack creatureSSPK, addonSSPK, oustersSSPK;
	CCreatureFramePack slayerManFPK, slayerWomanFPK, vampireManFPK, vampireWomanFPK, advancedOustersFPK;
	CShadowSpritePack slayerManSSPK, slayerWomanSSPK, vampireManSSPK, vampireWomanSSPK, advancedOustersSSPK;
	CSpritePack etcSPK;
	Canvas drawn, expected;
	CreatureDrawPacks packs;
	POINT at = { 30, 20 };
	bool userBlending = false;

	Scene()
	{
		CHECK_EQ(0, spritectl_init());
		g_CurrentFrame = 0;
		creatureTypes.Init(800);
		g_pCreatureTable = &creatureTypes;
		Type(kSlayer, CREATURETRIBE_SLAYER, 0);
		Type(kVampire, CREATURETRIBE_VAMPIRE, 3);
		Type(kOusters, CREATURETRIBE_OUSTERS, 2);
		Type(kPlayerVampire, CREATURETRIBE_VAMPIRE, 4);
		sprites.Init(5);
		for (int type = 0; type < 5; ++type)
		{
			CREATURESPRITETABLE_INFO& row = testfw::MutableRow(sprites, type);
			row.FrameID = TYPE_FRAMEID(type == 3 ? 1 : type == 4 ? 2 : FRAMEID_NULL);
			row.CreatureType = BYTE(type == 4 ? FLAG_CREATURESPRITE_PLAYER_VAMPIRE
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
		g_pUserInformation->GoreLevel = true;
		zone.reset(new ZoneProbe(TYPE_SECTORPOSITION(16), TYPE_SECTORPOSITION(16)));
		g_pZone = zone.get();
		player = new PlayerProbe;
		g_pPlayer = player;
		player->SetCreatureType(kSlayer);
		player->MObject::SetPosition(10, 10);
		const int actions = (std::max)({ACTION_MAX_SLAYER, ACTION_MAX_VAMPIRE, ACTION_MAX_OUSTERS});
		const int advancedActions = (std::max)({int(ACTION_ADVANCEMENT_SLAYER_MAX), int(ACTION_ADVANCEMENT_OUSTERS_MAX),
			int(ACTION_ADVANCEMENT_MAX)}) - ADVANCEMENT_ACTION_START;
		Shape(creatureFPK, 223, actions);
		Shape(addonFPK, 4, actions);
		Shape(oustersFPK, 4, actions);
		for (CCreatureFramePack* pPack : {&slayerManFPK, &slayerWomanFPK, &vampireManFPK, &vampireWomanFPK, &advancedOustersFPK})
			Shape(*pPack, AC_BIKE_2 + 1, advancedActions);
		int n = 1;
		for (CShadowSpritePack* pPack : {&creatureSSPK, &addonSSPK, &oustersSSPK, &slayerManSSPK, &slayerWomanSSPK,
			&vampireManSSPK, &vampireWomanSSPK, &advancedOustersSSPK})
		{
			pPack->Init(3);
			for (int sprite = 0; sprite < 3; ++sprite, ++n) LoadShadow((*pPack)[WORD(sprite)], n);
		}
		etcSPK.Init(SPRITEID_SLAYER_CROSS_SHADOW + 1);
		for (int sprite = 0; sprite <= SPRITEID_SLAYER_CROSS_SHADOW; ++sprite) LoadSprite(etcSPK[WORD(sprite)], sprite);
		packs.pSurface = &drawn.surface;
		packs.pCreatureShadowFPK = &creatureFPK;
		packs.pCreatureSSPK = &creatureSSPK;
		packs.pAddonShadowFPK = &addonFPK;
		packs.pAddonSSPK = &addonSSPK;
		packs.pOustersShadowFPK = &oustersFPK;
		packs.pOustersSSPK = &oustersSSPK;
		packs.pAdvancementSlayerManShadowFPK = &slayerManFPK;
		packs.pAdvancementSlayerManSSPK = &slayerManSSPK;
		packs.pAdvancementSlayerWomanShadowFPK = &slayerWomanFPK;
		packs.pAdvancementSlayerWomanSSPK = &slayerWomanSSPK;
		packs.pAdvancementVampireManShadowFPK = &vampireManFPK;
		packs.pAdvancementVampireManSSPK = &vampireManSSPK;
		packs.pAdvancementVampireWomanShadowFPK = &vampireWomanFPK;
		packs.pAdvancementVampireWomanSSPK = &vampireWomanSSPK;
		packs.pAdvancementOustersShadowFPK = &advancedOustersFPK;
		packs.pAdvancementOustersSSPK = &advancedOustersSSPK;
		packs.pEtcSPK = &etcSPK;
	}
	~Scene()
	{
		zone.reset();
		g_pZone = oldZone;
		delete player;
		g_pPlayer = oldPlayer;
		g_CurrentFrame = oldFrame;
		g_pEventManager = oldEvents;
		g_pRankBonusTable = oldRankBonuses;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatureTypes;
	}
	// A creature type of the tribe whose first sprite type is `sprite`.
	void Type(int type, CREATURETRIBE tribe, int sprite)
	{
		CREATURETABLE_INFO& row = testfw::MutableRow(creatureTypes, type);
		row.SetCreatureTribe(tribe, ACTION_MAX_SLAYER);
		row.SpriteTypes.Init(1);
		*row.SpriteTypes.GetMutable(0) = sprite;
	}
	// A monster type with a body (frame set 1).
	void Monster(int type) { Type(type, CREATURETRIBE_VAMPIRE, 3); }
	void Draw(MCreature& creature)
	{
		POINT point = at;
		drawn.surface.Lock();
		DrawCreatureShadow(packs, &point, &creature, userBlending);
		drawn.surface.Unlock();
	}
	// The body's shadow at (dx, dy) from where the creature stands.
	void Body(int sprite, int dx, int dy, bool blended = false)
	{
		expected.Shadow(at.x + dx, at.y + dy, creatureSSPK[WORD(sprite)], blended);
	}
	// The drawn picture is the expected one, and not blank.
	bool Same()
	{
		const std::vector<WORD> a = drawn.Picture(), b = expected.Picture();
		bool any = false;
		for (WORD pixel : a) any |= pixel != 0xFFFF;
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
		drawn.surface.FillSurface(0xFFFF);
		expected.surface.FillSurface(0xFFFF);
	}
};

} // namespace

//----------------------------------------------------------------------
// Whether there is a shadow
//----------------------------------------------------------------------
TEST(CreatureShadow, ABodyCastsTheShadowOfItsFrameAtItsOffset)
{
	Scene scene;
	Creature vampire;
	vampire.Become(kVampire);
	vampire.SetCurrentDirection(3);
	vampire.Pose(ACTION_ATTACK, 4);
	Place(scene.creatureFPK, kBody, ACTION_ATTACK, 3, 4, 1, 5, 6);
	scene.Draw(vampire);
	scene.Body(1, 5, 6);
	CHECK(scene.Same());
	// A frame its action has not got casts none.
	vampire.Pose(ACTION_ATTACK, 5);
	scene.Clear();
	scene.Draw(vampire);
	CHECK(scene.drawn.Blank());
}

TEST(CreatureShadow, NoneWhenAnEventHidesShadowsOrForAWholeGhost)
{
	Scene scene;
	Creature vampire;
	vampire.Become(kVampire);
	Place(scene.creatureFPK, kBody, ACTION_STAND, 2, 0, 1, 5, 6);
	vampire.SetCurrentDirection(2);
	MEvent hide;
	hide.eventID = EVENTID_RESURRECT;
	hide.eventFlag = EVENTFLAG_NOT_DRAW_CREATURE_SHADOW;
	hide.eventType = EVENTTYPE_ZONE;
	hide.eventDelay = -1;
	scene.events.AddEvent(hide);
	scene.Draw(vampire);
	CHECK(scene.drawn.Blank());
	scene.events.RemoveEvent(EVENTID_RESURRECT);
	// A ghost in all three parts casts none; in two, it does.
	vampire.SetGhost(1 | 2 | 4, 10);
	scene.Draw(vampire);
	CHECK(scene.drawn.Blank());
	vampire.SetGhost(1 | 2, 10);
	scene.Draw(vampire);
	scene.Body(1, 5, 6);
	CHECK(scene.Same());
	// Nor does one in the ghost status.
	vampire.SetGhost(0, 0);
	vampire.Set(EFFECTSTATUS_GHOST);
	scene.Clear();
	scene.Draw(vampire);
	CHECK(scene.drawn.Blank());
}

TEST(CreatureShadow, ABlindedPlayerSeesOnlyTheShadowsBesideIt)
{
	for (EFFECTSTATUS blind : {EFFECTSTATUS_YELLOW_POISON_TO_CREATURE, EFFECTSTATUS_FLARE, EFFECTSTATUS_BLINDNESS})
	{
		Scene scene;
		Creature vampire;
		vampire.Become(kVampire);
		vampire.SetCurrentDirection(2);
		Place(scene.creatureFPK, kBody, ACTION_STAND, 2, 0, 1, 5, 6);
		scene.player->Set(blind);
		// The player stands at (10, 10): the tiles around it, and no further.
		for (int x : {8, 9, 11, 12})
			for (int y : {8, 9, 11, 12})
			{
				vampire.MObject::SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
				scene.Clear();
				scene.Draw(vampire);
				if (x >= 9 && x <= 11 && y >= 9 && y <= 11)
				{
					scene.Body(1, 5, 6);
					CHECK(scene.Same());
				}
				else CHECK(scene.drawn.Blank());
			}
		// Its own shadow it sees.
		scene.Clear();
		scene.player->SetCreatureType(kVampire);
		scene.player->SetCurrentDirection(2);
		scene.player->MObject::SetPosition(0, 0);
		scene.Draw(*scene.player);
		scene.Body(1, 5, 6);
		CHECK(scene.Same());
	}
}

TEST(CreatureShadow, SomeTypesCastABlendedShadow)
{
	Scene scene;
	scene.Monster(674);
	Creature creature;
	creature.Become(674);
	creature.SetCurrentDirection(5);
	Place(scene.creatureFPK, kBody, ACTION_STAND, 5, 0, 2, 3, 4);
	scene.Draw(creature);
	scene.Body(2, 3, 4, true);
	CHECK(scene.Same());
}

//----------------------------------------------------------------------
// The slayer
//----------------------------------------------------------------------
TEST(CreatureShadow, ASlayersWornPartsCastShadowsAtTheirOffsets)
{
	Scene scene;
	Creature slayer;
	slayer.Become(kSlayer);
	slayer.SetCurrentDirection(2);
	slayer.Pose(ACTION_MOVE, 3);
	slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT);
	slayer.Wear(ADDON_RIGHTHAND, 2, ITEM_CLASS_SWORD);
	Place(scene.addonFPK, 1, ACTION_MOVE, 2, 3, 1, 4, 5);
	Place(scene.addonFPK, 2, ACTION_MOVE, 2, 3, 2, 6, 7);
	scene.Draw(slayer);
	scene.expected.Shadow(34, 25, scene.addonSSPK[1]);
	scene.expected.Shadow(36, 27, scene.addonSSPK[2]);
	CHECK(scene.Same());
	// Blended at the player's choice.
	scene.userBlending = true;
	scene.Clear();
	scene.Draw(slayer);
	scene.expected.Shadow(34, 25, scene.addonSSPK[1], true);
	scene.expected.Shadow(36, 27, scene.addonSSPK[2], true);
	CHECK(scene.Same());
	// A frame its action has not got casts none.
	slayer.Pose(ACTION_MOVE, 4);
	scene.Clear();
	scene.Draw(slayer);
	CHECK(scene.drawn.Blank());
}


TEST(CreatureShadow, AnInstallingSlayerCastsItsTurretsShadow)
{
	for (bool advanced : {false, true})
	{
		Scene scene;
		Creature slayer;
		slayer.Become(kSlayer);
		if (advanced) slayer.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
		slayer.Set(EFFECTSTATUS_INSTALL_TURRET);
		slayer.SetInstallTurretCount(6);
		slayer.SetCurrentDirection(5);
		slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT);
		// Standing, it faces 2 on the frame of its count, solid whatever
		// the option, and its parts cast none, though their frames are
		// there.
		scene.userBlending = true;
		Place(scene.creatureFPK, 222, ACTION_STAND, 2, 6, 1, 4, 5);
		for (int action : {ACTION_STAND, ACTION_ATTACK})
			for (int direction = 0; direction < 8; ++direction)
			{
				Fill(scene.addonFPK, 1, action, direction, 7, 2);
				const int advancedAction = ConvAdvancementSlayerActionFromSlayerAction(action, &slayer);
				if (advancedAction != -1)
					Fill(scene.slayerManFPK, AC_BODY, advancedAction - ADVANCEMENT_ACTION_START, direction, 7, 2);
			}
		scene.Draw(slayer);
		scene.Body(1, 4, 5);
		CHECK(scene.Same());
		// Otherwise its own action, direction and frame.
		slayer.Pose(ACTION_ATTACK, 3);
		Place(scene.creatureFPK, 222, ACTION_ATTACK, 5, 3, 2, 6, 7);
		scene.Clear();
		scene.Draw(slayer);
		scene.Body(2, 6, 7);
		CHECK(scene.Same());
	}
}

TEST(CreatureShadow, ASlayerWithoutAddonShadowsCastsNone)
{
	Scene scene;
	scene.addonFPK.Release();
	Creature slayer;
	slayer.Become(kSlayer);
	slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT);
	scene.Draw(slayer);
	CHECK(scene.drawn.Blank());
}

//----------------------------------------------------------------------
// The ousters
//----------------------------------------------------------------------
TEST(CreatureShadow, AnOustersCastsItsCoatAndChakram)
{
	Scene scene;
	Creature ousters;
	ousters.Become(kOusters);
	ousters.SetCurrentDirection(1);
	ousters.Pose(ACTION_ATTACK, 2);
	ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT);
	ousters.Wear(ADDON_RIGHTHAND, 3, ITEM_CLASS_OUSTERS_CHAKRAM);
	Place(scene.oustersFPK, 1, ACTION_ATTACK, 1, 2, 1, 4, 5);
	Place(scene.oustersFPK, 3, ACTION_ATTACK, 1, 2, 2, 9, 9);
	auto coat = [&](bool blended) { scene.expected.Shadow(34, 25, scene.oustersSSPK[1], blended); };
	auto chakram = [&](bool blended) { scene.expected.Shadow(39, 29, scene.oustersSSPK[2], blended); };
	scene.Draw(ousters);
	coat(false);
	chakram(false);
	CHECK(scene.Same());
	scene.userBlending = true;
	scene.Clear();
	scene.Draw(ousters);
	coat(true);
	chakram(true);
	CHECK(scene.Same());
	// A ghost's coat (1) and chakram (2) cast none, each by its flag.
	for (int ghost : {1, 2, 1 | 2})
	{
		ousters.SetGhost(BYTE(ghost), 10);
		scene.Clear();
		scene.Draw(ousters);
		if (!(ghost & 1)) coat(true);
		if (!(ghost & 2)) chakram(true);
		if (ghost == (1 | 2)) CHECK(scene.drawn.Blank());
		else CHECK(scene.Same());
	}
}

TEST(CreatureShadow, AnOustersWithoutAChakramStandsAndWalksItsOwnWay)
{
	// A wristlet is no chakram: it casts nothing, and the coat stands and
	// walks in the ousters' own actions.
	struct Case { int action, drawnAs; };
	for (const Case& c : {Case{ACTION_STAND, ACTION_OUSTERS_STAND}, Case{ACTION_MOVE, ACTION_OUSTERS_MOVE},
		Case{ACTION_ATTACK, ACTION_ATTACK}})
	{
		Scene scene;
		Creature ousters;
		ousters.Become(kOusters);
		ousters.SetCurrentDirection(1);
		ousters.Pose(c.action, 0);
		ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT);
		ousters.Wear(ADDON_RIGHTHAND, 2, ITEM_CLASS_OUSTERS_WRISTLET);
		Place(scene.oustersFPK, 1, c.drawnAs, 1, 0, 1, 4, 5);
		Place(scene.oustersFPK, 2, c.drawnAs, 1, 0, 2, 9, 9);
		scene.Draw(ousters);
		scene.expected.Shadow(34, 25, scene.oustersSSPK[1]);
		CHECK(scene.Same());
	}
}

//----------------------------------------------------------------------
// The advanced (second) classes
//----------------------------------------------------------------------
TEST(CreatureShadow, AnAdvancedSlayerCastsItsPartsFromItsGendersPack)
{
	for (bool male : {true, false})
	{
		Scene scene;
		Creature slayer;
		slayer.Become(kSlayer);
		slayer.SetMale(male);
		slayer.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
		slayer.SetCurrentDirection(2);
		slayer.Pose(ACTION_MAGIC, 2);
		slayer.Wear(ADDON_RIGHTHAND, 9, ITEM_CLASS_SWORD);
		slayer.Wear(ADDON_LEFTHAND, 8, ITEM_CLASS_SHIELD);
		const int action = ConvAdvancementSlayerActionFromSlayerAction(ACTION_MAGIC, &slayer);
		CHECK(action != -1);
		CCreatureFramePack& pack = male ? scene.slayerManFPK : scene.slayerWomanFPK;
		CShadowSpritePack& sprites = male ? scene.slayerManSSPK : scene.slayerWomanSSPK;
		// The parts of a sword and a shield, not their frame IDs.
		Place(pack, AC_SWORD, action - ADVANCEMENT_ACTION_START, 2, 2, 1, 4, 5);
		Place(pack, AC_SHIELD, action - ADVANCEMENT_ACTION_START, 2, 2, 2, 12, 9);
		scene.userBlending = !male;
		scene.Draw(slayer);
		scene.expected.Shadow(34, 25, sprites[1], !male);
		scene.expected.Shadow(42, 29, sprites[2], !male);
		CHECK(scene.Same());
	}
}

TEST(CreatureShadow, AnAdvancedSlayerWithoutAnAdvancedActionCastsNone)
{
	Scene scene;
	Creature slayer;
	slayer.Become(kSlayer);
	slayer.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
	// A plain attack has none (the table says it lacks its own rule).
	slayer.Pose(ACTION_ATTACK, 0);
	slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT);
	CHECK_EQ(-1, ConvAdvancementSlayerActionFromSlayerAction(ACTION_ATTACK, &slayer));
	scene.Draw(slayer);
	CHECK(scene.drawn.Blank());
}

TEST(CreatureShadow, AnAdvancedVampireCastsItsBodyFromItsGendersPack)
{
	for (bool male : {true, false})
	{
		Scene scene;
		Creature vampire;
		vampire.Become(kVampire);
		vampire.SetMale(male);
		vampire.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
		vampire.SetCurrentDirection(4);
		vampire.Pose(ACTION_ATTACK, 1);
		const int action = GetAdvancementVampireActionFromVampireAction(ACTION_ATTACK, &vampire);
		CHECK(action != -1);
		CCreatureFramePack& pack = male ? scene.vampireManFPK : scene.vampireWomanFPK;
		CShadowSpritePack& sprites = male ? scene.vampireManSSPK : scene.vampireWomanSSPK;
		// Frame set 0, whatever its body; set 1 (a weapon) never.
		Place(pack, 0, action - ADVANCEMENT_ACTION_START, 4, 1, 1, 4, 5);
		Place(pack, 1, action - ADVANCEMENT_ACTION_START, 4, 1, 2, 12, 9);
		scene.Draw(vampire);
		scene.expected.Shadow(34, 25, sprites[1]);
		CHECK(scene.Same());
	}
}

TEST(CreatureShadow, AnAdvancedVampireWithoutAnAdvancedActionCastsNone)
{
	Scene scene;
	int action = 0;
	while (action < ACTION_MAX_SLAYER && GetAdvancementVampireActionFromVampireAction(action, nullptr) != -1) ++action;
	CHECK(action < ACTION_MAX_SLAYER);
	Creature vampire;
	vampire.Become(kVampire);
	vampire.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
	vampire.Pose(action, 0);
	for (CCreatureFramePack* pPack : {&scene.vampireManFPK, &scene.vampireWomanFPK})
		for (int set = 0; set < 2; ++set)
			for (int direction = 0; direction < 8; ++direction) Fill(*pPack, set, 0, direction, 3, 1);
	scene.Draw(vampire);
	CHECK(scene.drawn.Blank());
}

TEST(CreatureShadow, AnAdvancedBatOrWolfCastsItsPlainBody)
{
	for (int type : {CREATURETYPE_BAT, CREATURETYPE_WOLF, CREATURETYPE_WER_WOLF})
	{
		Scene scene;
		scene.Monster(type);
		Creature beast;
		beast.Become(type);
		beast.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
		beast.SetCurrentDirection(4);
		beast.Pose(ACTION_ATTACK, 1);
		Place(scene.creatureFPK, kBody, ACTION_ATTACK, 4, 1, 1, 4, 5);
		scene.Draw(beast);
		scene.Body(1, 4, 5);
		CHECK(scene.Same());
	}
}

TEST(CreatureShadow, AnAdvancedOustersCastsCoatSet1AndChakramSet0)
{
	Scene scene;
	Creature ousters;
	ousters.Become(kOusters);
	ousters.MStatus::SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
	ousters.SetCurrentDirection(6);
	ousters.Pose(ACTION_STAND, 2);
	ousters.Wear(ADDON_COAT, 2, ITEM_CLASS_OUSTERS_COAT);
	ousters.Wear(ADDON_RIGHTHAND, 3, ITEM_CLASS_OUSTERS_CHAKRAM);
	const int action = ConvAdvancementOustersActionFromOusterAction(ACTION_STAND, true) - ADVANCEMENT_ACTION_START;
	Place(scene.advancedOustersFPK, 1, action, 6, 2, 1, 4, 5);
	Place(scene.advancedOustersFPK, 0, action, 6, 2, 2, 12, 9);
	auto coat = [&] { scene.expected.Shadow(34, 25, scene.advancedOustersSSPK[1], true); };
	auto chakram = [&] { scene.expected.Shadow(42, 29, scene.advancedOustersSSPK[2], true); };
	scene.userBlending = true;
	scene.Draw(ousters);
	coat();
	chakram();
	CHECK(scene.Same());
	for (int ghost : {1, 2, 1 | 2})
	{
		ousters.SetGhost(BYTE(ghost), 10);
		scene.Clear();
		scene.Draw(ousters);
		if (!(ghost & 1)) coat();
		if (!(ghost & 2)) chakram();
		if (ghost == (1 | 2)) CHECK(scene.drawn.Blank());
		else CHECK(scene.Same());
	}
	// Without a chakram it stands as one without.
	ousters.SetGhost(0, 0);
	ousters.Wear(ADDON_RIGHTHAND, 3, ITEM_CLASS_OUSTERS_WRISTLET);
	const int bare = ConvAdvancementOustersActionFromOusterAction(ACTION_STAND, false) - ADVANCEMENT_ACTION_START;
	CHECK(bare != action);
	Place(scene.advancedOustersFPK, 1, bare, 6, 2, 0, 4, 5);
	scene.Clear();
	scene.Draw(ousters);
	scene.expected.Shadow(34, 25, scene.advancedOustersSSPK[0], true);
	CHECK(scene.Same());
}

//----------------------------------------------------------------------
// The dead
//----------------------------------------------------------------------
TEST(CreatureShadow, APlayerVampireDeadAndDoneDyingIsDust)
{
	Scene scene;
	Creature vampire;
	vampire.Become(kPlayerVampire);
	vampire.SetCurrentDirection(2);
	Place(scene.creatureFPK, 2, ACTION_DIE, 2, 4, 1, 4, 5);
	vampire.Die();
	vampire.Finish(ACTION_DIE, 5);
	scene.Draw(vampire);
	CHECK(scene.drawn.Blank());
	// Still dying, or done and alive, its body casts its frame.
	vampire.Pose(ACTION_DIE, 4);
	scene.Draw(vampire);
	scene.Body(1, 4, 5);
	CHECK(scene.Same());
	vampire.m_bAlive = true;
	vampire.Finish(ACTION_DIE, 5);
	scene.Clear();
	scene.Draw(vampire);
	scene.Body(1, 4, 5);
	CHECK(scene.Same());
}

TEST(CreatureShadow, WithoutGoreTheDeadAreDustAndASlayerLeavesACross)
{
	Scene scene;
	g_pUserInformation->GoreLevel = false;
	Creature vampire;
	vampire.Become(kVampire);
	vampire.SetCurrentDirection(2);
	vampire.Die();
	// Half its dying done: dust.
	vampire.m_Action = ACTION_DIE;
	vampire.m_ActionCountMax = 10;
	vampire.m_ActionCount = 5;
	Place(scene.creatureFPK, kBody, ACTION_DIE, 2, 0, 1, 4, 5);
	scene.Draw(vampire);
	CHECK(scene.drawn.Blank());
	// Less: its first frame (no dying shown).
	vampire.m_ActionCount = 4;
	scene.Draw(vampire);
	scene.Body(1, 4, 5);
	CHECK(scene.Same());
	// A slayer leaves a cross, five to the left.
	Creature slayer;
	slayer.Become(kSlayer);
	slayer.Die();
	slayer.Finish(ACTION_DIE, 10);
	scene.Clear();
	scene.Draw(slayer);
	scene.expected.Sprite(scene.at.x - 5, scene.at.y, scene.etcSPK[SPRITEID_SLAYER_CROSS_SHADOW]);
	CHECK(scene.Same());
}

TEST(CreatureShadow, SomeAreDustWithGoreAndSomeNeverAre)
{
	// The bodies 377 to 386 and 480 are dust with gore too; Bathory and
	// Tepez (431 to 436), the relics, the trees and the fishmonger of the
	// list never (the waypoints on it cast nothing dead: see below).
	struct Case { int type; bool gore, dust; };
	for (const Case& c : {Case{377, true, true}, Case{386, true, true}, Case{480, true, true}, Case{376, true, false},
		Case{387, true, false}, Case{431, false, false}, Case{436, false, false}, Case{430, false, true}, Case{437, false, true},
		Case{526, false, false}, Case{560, false, false}, Case{482, false, false}, Case{650, false, false}, Case{670, false, false},
		Case{672, false, false}, Case{673, false, false}, Case{636, false, false}})
	{
		Scene scene;
		g_pUserInformation->GoreLevel = c.gore;
		scene.Monster(c.type);
		Creature creature;
		creature.Become(c.type);
		creature.Die();
		creature.Finish(ACTION_DIE, 10);
		for (int direction = 0; direction < 8; ++direction)
			for (int action : {ACTION_DIE, ACTION_STAND, 6})
				for (int frame : {0, 9, 14})
					Place(scene.creatureFPK, kBody, action, direction, frame, 1, 4, 5);
		scene.Draw(creature);
		CHECK_EQ(c.dust, scene.drawn.Blank());
	}
}

TEST(CreatureShadow, ACreatureShutInACasketCastsTheCasketsShadow)
{
	for (int type : {0, 1, 2})
	{
		Scene scene;
		Creature vampire;
		vampire.Become(kVampire);
		vampire.AddCasketSoon(type);
		scene.Draw(vampire);
		CSprite& image = scene.etcSPK[WORD(SPRITEID_CASKET_1 + type * 2)];
		const POINT offset[] = { { -27, 28 }, { -27, 23 }, { -31, 28 } };
		scene.expected.Sprite(scene.at.x + 24 - (image.GetWidth() >> 1) + offset[type].x,
			scene.at.y + TILE_Y - image.GetHeight() + offset[type].y, scene.etcSPK[WORD(SPRITEID_CASKET_1 + type * 2 + 1)]);
		CHECK(scene.Same());
	}
	// While the casket closes, its own shadow.
	Scene scene;
	Creature vampire;
	vampire.Become(kVampire);
	vampire.SetCurrentDirection(2);
	vampire.AddCasket(0);
	Place(scene.creatureFPK, kBody, ACTION_STAND, 2, 0, 1, 4, 5);
	scene.Draw(vampire);
	scene.Body(1, 4, 5);
	CHECK(scene.Same());
}

TEST(CreatureShadow, NoneUndergroundOrHalfThroughAPortal)
{
	Scene scene;
	Creature vampire;
	vampire.Become(kVampire);
	vampire.SetCurrentDirection(2);
	Place(scene.creatureFPK, kBody, ACTION_STAND, 2, 0, 1, 4, 5);
	vampire.m_MoveType = MCreature::CREATURE_UNDERGROUND;
	scene.Draw(vampire);
	CHECK(scene.drawn.Blank());
	vampire.m_MoveType = MCreature::CREATURE_GROUND;
	vampire.m_bCutHeight = true;
	scene.Draw(vampire);
	CHECK(scene.drawn.Blank());
	vampire.m_bCutHeight = false;
	scene.Draw(vampire);
	scene.Body(1, 4, 5);
	CHECK(scene.Same());
}

TEST(CreatureShadow, AHeadlessCorpseLiesOnTheFrameAfterItsLast)
{
	Scene scene;
	testfw::MutableRow(scene.creatureTypes, kVampire).SetActionCount(ACTION_DIE, 7);
	Creature vampire;
	vampire.Become(kVampire);
	vampire.SetCurrentDirection(2);
	vampire.Finish(ACTION_DIE, 5);
	Place(scene.creatureFPK, kBody, ACTION_DIE, 2, 7, 1, 4, 5);
	vampire.m_bHasHead = false;
	scene.Draw(vampire);
	scene.Body(1, 4, 5);
	CHECK(scene.Same());
	// With its head, or another action, the last frame.
	Place(scene.creatureFPK, kBody, ACTION_DIE, 2, 4, 2, 6, 7);
	Place(scene.creatureFPK, kBody, ACTION_ATTACK, 2, 4, 2, 6, 7);
	for (int action : {ACTION_DIE, ACTION_ATTACK})
		for (bool head : {true, false})
		{
			if (action == ACTION_DIE && !head) continue;
			vampire.m_bHasHead = head;
			vampire.Finish(action, 5);
			scene.Clear();
			scene.Draw(vampire);
			scene.Body(2, 6, 7);
			CHECK(scene.Same());
		}
}

TEST(CreatureShadow, WithoutGoreADyingCreatureKeepsItsFirstFrame)
{
	struct Case { int type; bool first; };
	for (const Case& c : {Case{kVampire, true}, Case{430, true}, Case{431, false}, Case{436, false}, Case{437, true}})
	{
		Scene scene;
		g_pUserInformation->GoreLevel = false;
		if (c.type != kVampire) scene.Monster(c.type);
		Creature creature;
		creature.Become(c.type);
		creature.SetCurrentDirection(3);
		creature.Pose(ACTION_DIE, 3);
		Place(scene.creatureFPK, kBody, ACTION_DIE, 3, c.first ? 0 : 3, 1, 4, 5);
		scene.Draw(creature);
		scene.Body(1, 4, 5);
		CHECK(scene.Same());
	}
}

//----------------------------------------------------------------------
// Darkness and hallucination
//----------------------------------------------------------------------
TEST(CreatureShadow, InDarknessOnlyAVampireSeesAShadow)
{
	Scene scene;
	Creature vampire;
	vampire.Become(kVampire);
	vampire.SetCurrentDirection(2);
	vampire.m_DarknessCount = 0;
	Place(scene.creatureFPK, kBody, ACTION_STAND, 2, 0, 1, 4, 5);
	scene.Draw(vampire);
	CHECK(scene.drawn.Blank());
	// A vampire player sees it.
	scene.player->SetCreatureType(kVampire);
	scene.Draw(vampire);
	scene.Body(1, 4, 5);
	CHECK(scene.Same());
	// So does a slayer in the light of Lightness, but in zone 3001.
	scene.player->SetCreatureType(kSlayer);
	scene.player->Set(EFFECTSTATUS_LIGHTNESS);
	scene.Clear();
	scene.Draw(vampire);
	scene.Body(1, 4, 5);
	CHECK(scene.Same());
	scene.zone->m_Info.ZoneID = 3001;
	scene.Clear();
	scene.Draw(vampire);
	CHECK(scene.drawn.Blank());
	// An NPC's shadow shows in darkness.
	scene.Type(9, CREATURETRIBE_NPC, 3);
	vampire.Become(9);
	vampire.SetCurrentDirection(2);
	scene.Draw(vampire);
	scene.Body(1, 4, 5);
	CHECK(scene.Same());
}

TEST(CreatureShadow, ThePlayersOwnShadowShowsInDarkness)
{
	// A player who is no vampire, with a body of its own, in darkness.
	Scene scene;
	scene.Type(10, CREATURETRIBE_SLAYER, 3);
	scene.player->SetCreatureType(10);
	scene.player->SetCurrentDirection(2);
	scene.player->m_DarknessCount = 0;
	CHECK(!scene.player->IsVampire());
	Place(scene.creatureFPK, kBody, ACTION_STAND, 2, 0, 1, 4, 5);
	scene.Draw(*scene.player);
	scene.Body(1, 4, 5);
	CHECK(scene.Same());
}

TEST(CreatureShadow, AHallucinatedCreatureCastsWhatItLooksLike)
{
	Scene scene;
	scene.Monster(11);
	testfw::MutableRow(scene.creatureTypes, 11).SetActionCount(ACTION_STAND, 10);
	Creature slayer;
	slayer.Become(kSlayer);
	slayer.SetCurrentDirection(2);
	slayer.Wear(ADDON_COAT, 1, ITEM_CLASS_COAT);
	slayer.Pose(ACTION_STAND, 13);
	slayer.m_bHallu = true;
	slayer.m_HalluCreatureType = 11;
	slayer.m_HalluCreatureFrameID = 2;
	// The body it looks like, on its frame of that body's count, and no parts.
	Place(scene.creatureFPK, 2, ACTION_STAND, 2, 3, 1, 4, 5);
	Place(scene.addonFPK, 1, ACTION_STAND, 2, 13, 2, 6, 7);
	scene.Draw(slayer);
	scene.Body(1, 4, 5);
	CHECK(scene.Same());
}

//----------------------------------------------------------------------
// Types drawn their own way
//----------------------------------------------------------------------
namespace {

// A monster of the type, alive or dead, on (action, frame) facing 6; the
// shadow expected on (action, frame, direction), blended or not, or none.
struct Special
{
	int type;
	int action, frame;
	bool dead, finished;
	EFFECTSTATUS status;
	int drawnAction, drawnFrame, drawnDirection;
	bool blended, none;
};

void Check(const Special& c)
{
	Scene scene;
	scene.Monster(c.type);
	Creature creature;
	creature.Become(c.type);
	creature.SetCurrentDirection(6);
	if (c.finished) creature.Finish(c.action, c.frame + 1);
	else creature.Pose(c.action, c.frame);
	if (c.dead) creature.Die();
	if (c.status != EFFECTSTATUS_NULL) creature.Set(c.status);
	// None: whatever it might cast instead is there to be drawn.
	if (c.none)
		for (int action = 0; action < ACTION_MAX_SLAYER; ++action)
			for (int direction = 0; direction < 8; ++direction) Fill(scene.creatureFPK, kBody, action, direction, 47, 1);
	else Place(scene.creatureFPK, kBody, c.drawnAction, c.drawnDirection, c.drawnFrame, 1, 4, 5);
	scene.Draw(creature);
	if (c.none) CHECK(scene.drawn.Blank());
	else
	{
		scene.Body(1, 4, 5, c.blended);
		const bool same = scene.Same();
		if (!same) std::printf("      type %d\n", c.type);
		CHECK(same);
	}
}

const EFFECTSTATUS kNone = EFFECTSTATUS_NULL;

} // namespace

TEST(CreatureShadow, TheRelicsTreesAndPostsLieSouthAndBlended)
{
	for (int type : {371, 376, 560, 563, 482, 526, 549, 550, 557, 650, 660, 669, 673})
		Check({type, ACTION_STAND, 2, false, false, kNone, ACTION_STAND, 2, 2, true, false});
	// Their neighbours are drawn as they stand.
	for (int type : {370, 377, 559, 564, 481, 483, 525, 558, 649, 651, 659, 670})
		Check({type, ACTION_STAND, 2, false, false, kNone, ACTION_STAND, 2, 6, false, false});
}

TEST(CreatureShadow, GildreFloatsDrainingAndKneelsDead)
{
	for (int type : {717, 721, 723})
	{
		// Floating, it drains on the loop from frame 6.
		Check({type, ACTION_MOVE, 13, false, false, EFFECTSTATUS_GDR_FLOATING, ACTION_VAMPIRE_DRAIN, 6 + (13 & 7), 6, false, false});
		Check({type, ACTION_STAND, 2, false, false, kNone, ACTION_STAND, 2, 6, false, false});
	}
	// Awakened Gildre dead and done kneels on frame 46; the others lie.
	Check({723, ACTION_DIE, 9, true, true, kNone, ACTION_DIE, 46, 6, false, false});
	Check({721, ACTION_DIE, 9, true, true, kNone, ACTION_DIE, 9, 6, false, false});
	Check({723, ACTION_DIE, 9, true, false, kNone, ACTION_DIE, 9, 6, false, false});
	// Gildre's ice statue casts none.
	Check({722, ACTION_STAND, 0, false, false, kNone, ACTION_STAND, 0, 6, false, true});
}

TEST(CreatureShadow, StillOnesKeepTheirPose)
{
	// The fishmonger lies (action 6, frame 14).
	Check({636, ACTION_STAND, 3, false, false, kNone, 6, 14, 6, false, false});
	// The earth spirit and Tepez's coffin stand until coma or death,
	// facing 2 and 1.
	Check({698, ACTION_ATTACK, 3, false, false, kNone, ACTION_STAND, 3, 2, false, false});
	Check({698, ACTION_ATTACK, 3, false, false, EFFECTSTATUS_COMA, ACTION_DIE, 3, 2, false, false});
	Check({698, ACTION_ATTACK, 3, true, false, kNone, ACTION_DIE, 3, 2, false, false});
	Check({433, ACTION_ATTACK, 3, false, false, kNone, ACTION_STAND, 3, 1, false, false});
	Check({433, ACTION_ATTACK, 3, false, false, EFFECTSTATUS_COMA, ACTION_DIE, 3, 1, false, false});
	Check({433, ACTION_ATTACK, 3, true, false, kNone, ACTION_DIE, 3, 1, false, false});
	// 674 blends; 675 casts its magic's first frame, facing 3, blended.
	Check({674, ACTION_ATTACK, 3, false, false, kNone, ACTION_ATTACK, 3, 6, true, false});
	Check({675, ACTION_ATTACK, 3, false, false, kNone, ACTION_MAGIC, 0, 3, true, false});
	// The rocket launcher moves on its first frame.
	Check({767, ACTION_ATTACK, 3, false, false, kNone, ACTION_MOVE, 0, 6, false, false});
}

TEST(CreatureShadow, WaypointsObstaclesAndTowers)
{
	for (int type : {730, 731, 732})
	{
		Check({type, ACTION_ATTACK, 3, false, false, kNone, ACTION_STAND, 0, 2, true, false});
		Check({type, ACTION_DIE, 3, true, false, kNone, ACTION_DIE, 3, 6, false, true});
	}
	for (int type : {743, 748})
	{
		Check({type, ACTION_ATTACK, 3, false, false, kNone, ACTION_STAND, 0, 2, false, false});
		Check({type, ACTION_DIE, 3, true, false, kNone, ACTION_DIE, 3, 2, false, false});
		Check({type, ACTION_DIE, 3, true, true, kNone, ACTION_DIE, 3, 2, false, true});
	}
	// The pleasure station, land mine and auto turret stand, blended.
	for (int type : {738, 739, 741})
		Check({type, ACTION_ATTACK, 3, false, false, kNone, ACTION_STAND, 0, 2, true, false});
	// The claymore stands facing 1, and the guild tower faces 1, standing
	// while it lives.
	Check({740, ACTION_ATTACK, 3, false, false, kNone, ACTION_STAND, 0, 1, false, false});
	Check({740, ACTION_ATTACK, 3, true, false, kNone, ACTION_STAND, 0, 1, false, false});
	Check({734, ACTION_ATTACK, 3, false, false, kNone, ACTION_STAND, 0, 1, false, false});
	Check({734, ACTION_DIE, 3, true, false, kNone, ACTION_DIE, 3, 1, false, false});
	// The summoned Gore Grand faces 2, and casts none dead and done.
	Check({753, ACTION_ATTACK, 3, false, false, kNone, ACTION_ATTACK, 3, 2, false, false});
	Check({753, ACTION_DIE, 3, true, false, kNone, ACTION_DIE, 3, 2, false, false});
	Check({753, ACTION_DIE, 3, true, true, kNone, ACTION_DIE, 3, 2, false, true});
}

TEST(CreatureShadow, TheGreatRuffianTurnsWhenDrainedPastFrame4)
{
	// Dying, 765 faces 2 and 764 its own way.
	Check({765, ACTION_DIE, 3, false, false, kNone, ACTION_DIE, 3, 2, false, false});
	Check({764, ACTION_DIE, 3, false, false, kNone, ACTION_DIE, 3, 6, false, false});
	for (int type : {764, 765})
	{
		// Drained, it attacks; past frame 4 it holds 4, turning one
		// direction back on every odd frame of the clock.
		Check({type, ACTION_DRAINED, 3, false, false, kNone, ACTION_ATTACK, 3, 6, false, false});
		Check({type, ACTION_DRAINED, 5, false, false, kNone, ACTION_ATTACK, 4, 6, false, false});
		Check({type, ACTION_DRAINED, 4, false, false, kNone, ACTION_ATTACK, 4, 6, false, false});
		Scene scene;
		scene.Monster(type);
		Creature creature;
		creature.Become(type);
		creature.SetCurrentDirection(0);
		creature.Pose(ACTION_DRAINED, 6);
		g_CurrentFrame = 7;
		Place(scene.creatureFPK, kBody, ACTION_ATTACK, 7, 4, 1, 4, 5);
		scene.Draw(creature);
		scene.Body(1, 4, 5);
		CHECK(scene.Same());
		CHECK_EQ(7, int(creature.GetDirection()));
	}
}

TEST(CreatureShadow, TheSweeperKeeperWalksOnTheClock)
{
	// 672 moves on frame (clock + id) % 11, and lies south, blended (it is
	// on the relics' list).
	Scene scene;
	scene.Monster(672);
	Creature creature;
	creature.Become(672);
	creature.SetID(5);
	creature.SetCurrentDirection(6);
	creature.Pose(ACTION_MOVE, 7);
	g_CurrentFrame = 30;
	Place(scene.creatureFPK, kBody, ACTION_MOVE, 2, (30 + 5) % 11, 1, 4, 5);
	scene.Draw(creature);
	scene.Body(1, 4, 5, true);
	CHECK(scene.Same());
	// Standing, its own frame.
	creature.Pose(ACTION_STAND, 7);
	Place(scene.creatureFPK, kBody, ACTION_STAND, 2, 7, 2, 6, 7);
	scene.Clear();
	scene.Draw(creature);
	scene.Body(2, 6, 7, true);
	CHECK(scene.Same());
}

//----------------------------------------------------------------------
// Pets
//----------------------------------------------------------------------
TEST(CreatureShadow, APetInsideItsOwnersDarknessCastsNone)
{
	Scene scene;
	Pet pet;
	pet.SetCreatureType(kVampire);
	pet.SetCurrentDirection(2);
	pet.SetOwnerID(88);
	Place(scene.creatureFPK, kBody, ACTION_STAND, 2, 0, 1, 4, 5);
	// No owner in the zone: drawn.
	scene.Draw(pet);
	scene.Body(1, 4, 5);
	CHECK(scene.Same());
	// Its owner in the zone's creatures (the zone's lookup is all the
	// drawing asks of it): drawn, until the owner is in darkness.
	Creature owner;
	owner.Become(kVampire);
	owner.SetID(88);
	scene.zone->m_mapCreature[88] = &owner;
	scene.Clear();
	scene.Draw(pet);
	scene.Body(1, 4, 5);
	CHECK(scene.Same());
	owner.m_DarknessCount = 0;
	scene.Clear();
	scene.Draw(pet);
	CHECK(scene.drawn.Blank());
	scene.zone->m_mapCreature.erase(88);
}

TEST(CreatureShadow, ASlayersPetShowsItsTurret)
{
	// The pets 702 to 704 standing or moving show their turret: the frame
	// of the turret's direction of draining facing 2.
	for (int type : {702, 703, 704})
		for (int action : {ACTION_STAND, ACTION_MOVE, ACTION_ATTACK})
		{
			Scene scene;
			scene.Monster(type);
			Pet pet;
			pet.SetCreatureType(TYPE_CREATURETYPE(type));
			pet.SetCurrentDirection(3);
			pet.Pose(action, 0);
			pet.SetTurreDirection(5);
			Place(scene.creatureFPK, kBody, action, 3, 0, 1, 4, 5);
			Place(scene.creatureFPK, kBody, ACTION_VAMPIRE_DRAIN, 2, 5, 2, 20, 9);
			scene.Draw(pet);
			scene.Body(1, 4, 5);
			const bool turret = action != ACTION_ATTACK;
			// 702's turret sits off its centre by its facing.
			if (turret) scene.Body(2, 20 + (type == 702 ? -7 : 0), 9 + (type == 702 ? 2 : 0));
			CHECK(scene.Same());
		}
}

TEST(CreatureShadow, AFiringTurretRecoils)
{
	// Delay d moves the turret back 2d along the facing (turret / 4 + 2).
	Scene scene;
	scene.Monster(703);
	Pet pet;
	pet.SetCreatureType(703);
	pet.SetCurrentDirection(3);
	pet.Pose(ACTION_STAND, 0);
	pet.SetTurreDirection(9);
	pet.SetTurretDelay(3);
	Place(scene.creatureFPK, kBody, ACTION_STAND, 3, 0, 1, 4, 5);
	Place(scene.creatureFPK, kBody, ACTION_VAMPIRE_DRAIN, 2, 9, 2, 20, 20);
	scene.Draw(pet);
	scene.Body(1, 4, 5);
	// Facing (9 / 4 + 2) % 8 = 4: (1, 0).
	scene.Body(2, 20 - 1 * 3 * 2, 20 - 0);
	CHECK(scene.Same());
}

//----------------------------------------------------------------------
// An ousters in a ground elemental
//----------------------------------------------------------------------
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

TEST(CreatureShadow, AnOustersInAGroundElementalIsHiddenButFromOusters)
{
	Scene scene;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&elementalZone);
	elementalSector.SetGroundElemental();
	Creature ousters;
	ousters.Become(kOusters);
	ousters.MObject::SetPosition(3, 3);
	ousters.SetCurrentDirection(1);
	ousters.Wear(ADDON_COAT, 1, ITEM_CLASS_OUSTERS_COAT);
	ousters.Wear(ADDON_RIGHTHAND, 3, ITEM_CLASS_OUSTERS_CHAKRAM);
	Place(scene.oustersFPK, 1, ACTION_STAND, 1, 0, 1, 4, 5);
	CHECK(ousters.IsInGroundElemental());
	scene.Draw(ousters);
	CHECK(scene.drawn.Blank());
	// An ousters player sees it.
	scene.player->SetCreatureType(kOusters);
	scene.Draw(ousters);
	scene.expected.Shadow(34, 25, scene.oustersSSPK[1]);
	CHECK(scene.Same());
	// Out of the elemental, so does anyone.
	scene.player->SetCreatureType(kSlayer);
	elementalSector.UnSetGroundElemental();
	scene.Clear();
	scene.Draw(ousters);
	scene.expected.Shadow(34, 25, scene.oustersSSPK[1]);
	CHECK(scene.Same());
	ZoneState::SetHost(oldZone);
}
