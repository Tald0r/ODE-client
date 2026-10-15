#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "MNPC.h"
#include "MNPCTable.h"
#include "MShop.h"
#include "MShopShelf.h"
#include "MShopTemplateTable.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MActionInfoTable.h"
#include "SkillDef.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "EffectSpriteTypeDef.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "MItem.h"
#include "MEffect.h"
#include "MAttachEffect.h"
#include "MEventManager.h"
#include "MEventQueue.h"
#include "FrameClock.h"
#include "DirectionSelection.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"

#include <cstdlib>
#include <memory>
#include <vector>

// MItemUse.cpp's, reached through MItemHost.
MItem*	NewItemOfClass(ITEM_CLASS itemClass);

// The NPC (task 4.244, gamemodel's since then): its info, its shop and the
// stock it builds for it, and its idle turns, through hosts that answer
// for the player and the world.
namespace {

MCreature* player = nullptr;

const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.Creature = []() { return player; };
	return host;
}();
const ZoneState::Host zoneHost = []() {
	ZoneState::Host host{};
	host.Exists = []() { return true; };
	return host;
}();
const WorldEvents::Host worldHost = []() {
	WorldEvents::Host host{};
	host.ViewExists = []() { return true; };
	return host;
}();
const CreatureState::Host creatureHost = []() {
	CreatureState::Host host{};
	host.GetAddonItemClass = [](MCreature*, int, ITEM_CLASS* pItemClass) { *pItemClass = ITEM_CLASS_NULL; return false; };
	return host;
}();
const MEffectHost effectHost = []() {
	MEffectHost host{};
	host.CurrentFrame = []() -> DWORD { return g_CurrentFrame; };
	return host;
}();
const MAttachEffectHost attachHost = []() {
	MAttachEffectHost host{};
	host.LiveCreature = [](TYPE_OBJECTID, MAttachCreaturePosition&) { return true; };
	host.CreaturePosition = [](const MCreature*, MAttachCreaturePosition&) { return true; };
	return host;
}();
const MItemHost itemHost = []() {
	MItemHost host{};
	host.NewItem = NewItemOfClass;
	return host;
}();

// A coat that counts its deletions.
int coatsDeleted = 0;
struct CountedCoat : MCoat
{
	~CountedCoat() override { ++coatsDeleted; }
};

struct Scene
{
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
	MNPCTable npcs;
	MShopTemplateTable templates;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	MEventQueue* oldEvents = g_pEventManager;
	MNPCTable* oldNpcs = g_pNPCTable;
	MShopTemplateTable* oldTemplates = g_pShopTemplateTable;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&playerHost);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zoneHost);
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&worldHost);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creatureHost);
	const MEffectHost* oldEffectHost = MEffect::SetHost(&effectHost);
	const MAttachEffectHost* oldAttachHost = MAttachEffect::SetHost(&attachHost);

	Scene()
	{
		player = nullptr;
		coatsDeleted = 0;
		MItem::SetHost(&itemHost);
		creatureTypes.Init(CREATURETYPE_INSTALL_TURRET + 1);
		g_pCreatureTable = &creatureTypes;
		sprites.Init(4);
		g_pCreatureSpriteTable = &sprites;
		actions.Init(MAX_ACTIONINFO);
		// The game loads every row; a row here stands still unless a test
		// sets it (the constructor leaves what Set fills).
		for (int info = 0; info < MAX_ACTIONINFO; ++info)
			actions.GetMutable(info)->Set("", ACTION_STAND, EFFECTSPRITETYPE_NULL, 0, 0);
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
		g_pNPCTable = &npcs;
		g_pShopTemplateTable = &templates;
		for (ITEM_CLASS itemClass : {ITEM_CLASS_SWORD, ITEM_CLASS_COAT, ITEM_CLASS_VAMPIRE_PORTAL_ITEM})
		{
			g_pItemTable->InitClass(itemClass, 16);
			for (int type = 0; type < 16; ++type)
			{
				testfw::MutableRow(*g_pItemTable, itemClass, type).Value1 = 3000;
				testfw::MutableRow(*g_pItemTable, itemClass, type).MaxNumber = 9;
			}
		}
	}
	~Scene()
	{
		player = nullptr;
		MItem::SetHost(NULL);
		MAttachEffect::SetHost(oldAttachHost);
		MEffect::SetHost(oldEffectHost);
		CreatureState::SetHost(oldCreature);
		WorldEvents::SetHost(oldWorld);
		ZoneState::SetHost(oldZone);
		PlayerState::SetHost(oldPlayer);
		g_pShopTemplateTable = oldTemplates;
		g_pNPCTable = oldNpcs;
		g_pEventManager = oldEvents;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pRankBonusTable = oldRankBonuses;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pActionInfoTable = oldActions;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatureTypes;
	}
	// A creature type of the tribe, with a count for each action.
	void Tribe(int type, CREATURETRIBE tribe, bool male = true)
	{
		testfw::MutableRow(creatureTypes, type).SetCreatureTribe(tribe, ACTION_MAX_SLAYER);
		testfw::MutableRow(creatureTypes, type).bMale = male;
		for (int action = 0; action < ACTION_MAX_SLAYER; ++action)
			testfw::MutableRow(creatureTypes, type).SetActionCount(action, 10 + action);
	}
	// An NPC (id 7) whose shop takes these templates.
	NPC_INFO& Npc(std::initializer_list<std::pair<unsigned int, MShopTemplate>> rows)
	{
		auto* pInfo = new NPC_INFO;
		for (const auto& row : rows)
		{
			auto* pTemplate = new MShopTemplate(row.second);
			templates.AddData(row.first, pTemplate);
			pInfo->ListShopTemplateID.push_back(row.first);
		}
		npcs.AddData(7, pInfo);
		return *pInfo;
	}
};

MShopTemplate Template(BYTE shelf, int itemClass, WORD first, WORD last)
{
	MShopTemplate row;
	row.Type = shelf;
	row.Class = itemClass;
	row.MinType = first;
	row.MaxType = last;
	row.MinOption = 0;
	row.MaxOption = 0;
	return row;
}

std::vector<int> Types(const MShopShelf* pShelf)
{
	std::vector<int> types;
	for (unsigned int slot = 0; slot < SHOP_SHELF_SLOT; ++slot)
	{
		const MItem* pItem = pShelf->GetItem(slot);
		if (pItem != NULL) types.push_back(int(pItem->GetItemType()));
	}
	return types;
}

// A shop whose fixed shelf holds one coat that counts its deletion.
MShop* ShopWithACoat()
{
	MShop* pShop = new MShop;
	pShop->Init(MShopShelf::MAX_SHELF);
	MShopShelf* pShelf = MShopShelf::NewShelf(MShopShelf::SHELF_FIXED);
	pShop->SetShelf(MShopShelf::SHELF_FIXED, pShelf);
	pShelf->AddItem(new CountedCoat);
	return pShop;
}

} // namespace

//----------------------------------------------------------------------
// Its info and its shop
//----------------------------------------------------------------------
TEST(NPC, ItsInfoIsTheNPCTables)
{
	Scene scene;
	NPC_INFO& info = scene.Npc({});
	MNPC npc;
	CHECK(npc.GetNPCInfo(7) == &info);
	CHECK(npc.GetNPCInfo(8) == NULL);
}

TEST(NPC, ANewShopReplacesTheOldOneAndItsStock)
{
	Scene scene;
	MNPC npc;
	CHECK(npc.GetShop() == NULL);
	MShop* pFirst = ShopWithACoat();
	npc.SetShop(pFirst);
	CHECK(npc.GetShop() == pFirst);
	MShop* pSecond = new MShop;
	npc.SetShop(pSecond);
	CHECK(npc.GetShop() == pSecond);
	CHECK_EQ(1, coatsDeleted);
	npc.SetShop(NULL);
	CHECK(npc.GetShop() == NULL);
}

TEST(NPC, ItsShopGoesWithIt)
{
	Scene scene;
	{
		MNPC npc;
		npc.SetShop(ShopWithACoat());
	}
	CHECK_EQ(1, coatsDeleted);
}

TEST(NPC, AFixedShelfIsBuiltFromItsTemplatesInANewShop)
{
	Scene scene;
	scene.Npc({{30, Template(MShopShelf::SHELF_FIXED, ITEM_CLASS_SWORD, 1, 3)}});
	MNPC npc;
	npc.SetNPCID(7);
	CHECK(npc.CreateFixedShelf(false));
	CHECK(npc.GetShop() != NULL);
	CHECK(Types(npc.GetShop()->GetShelf(MShopShelf::SHELF_FIXED)) == (std::vector<int>{1, 2, 3}));
	// Built again into the same shop.
	MShop* pShop = npc.GetShop();
	CHECK(npc.CreateFixedShelf(false));
	CHECK(npc.GetShop() == pShop);
	CHECK(Types(pShop->GetShelf(MShopShelf::SHELF_FIXED)) == (std::vector<int>{1, 2, 3}));
	// An NPC the table lacks: no stock.
	MNPC stranger;
	stranger.SetNPCID(8);
	CHECK(!stranger.CreateFixedShelf(false));
	CHECK(stranger.GetShop() != NULL);
	CHECK(Types(stranger.GetShop()->GetShelf(MShopShelf::SHELF_FIXED)).empty());
}

TEST(NPC, AMysteriousShelfDressesAFemalePlayer)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER, true);
	scene.Tribe(8, CREATURETRIBE_SLAYER, false);
	scene.Npc({{40, Template(MShopShelf::SHELF_UNKNOWN, ITEM_CLASS_COAT, 0, 3)}});
	// A female player gets the paired female types (1 and 3).
	MCreature woman;
	woman.SetCreatureType(8);
	player = &woman;
	MNPC npc;
	npc.SetNPCID(7);
	CHECK(npc.CreateFixedShelf(true));
	CHECK(Types(npc.GetShop()->GetShelf(MShopShelf::SHELF_UNKNOWN)) == (std::vector<int>{1, 3}));
	// A male player, or none, the whole range.
	MCreature man;
	man.SetCreatureType(5);
	player = &man;
	MNPC other;
	other.SetNPCID(7);
	CHECK(other.CreateFixedShelf(true));
	CHECK(Types(other.GetShop()->GetShelf(MShopShelf::SHELF_UNKNOWN)) == (std::vector<int>{0, 1, 2, 3}));
	player = nullptr;
	MNPC alone;
	alone.SetNPCID(7);
	CHECK(alone.CreateFixedShelf(true));
	CHECK(Types(alone.GetShop()->GetShelf(MShopShelf::SHELF_UNKNOWN)) == (std::vector<int>{0, 1, 2, 3}));
}

TEST(NPC, APortalItemIsSetForItsZone)
{
	Scene scene;
	scene.Npc({{50, Template(MShopShelf::SHELF_FIXED, ITEM_CLASS_VAMPIRE_PORTAL_ITEM, 3, 3)}});
	MNPC npc;
	npc.SetNPCID(7);
	CHECK(npc.CreateFixedShelf(false));
	const MItem* pItem = npc.GetShop()->GetShelf(MShopShelf::SHELF_FIXED)->GetItem(0);
	CHECK(pItem != NULL);
	const MVampirePortalItem* pPortal = dynamic_cast<const MVampirePortalItem*>(pItem);
	CHECK(pPortal != NULL);
	if (pPortal != NULL)
	{
		CHECK_EQ(1003, pPortal->GetZoneID());
		CHECK_EQ(50, int(pPortal->GetZoneX()));
		CHECK_EQ(70, int(pPortal->GetZoneY()));
	}
}

//----------------------------------------------------------------------
// Idle
//----------------------------------------------------------------------
namespace {
// The player stands straight below the NPC (12,34): swapping x and y would
// face another way.
const int PlayerX = 12, PlayerY = 40;
// Of 400 frames, how often the NPC of the type turned to the player, and
// how many stands it ended. Attacking, it never stands to the end.
std::pair<int, int> FacingThePlayer(Scene& scene, int type, bool attacking = false)
{
	scene.Tribe(type, CREATURETRIBE_NPC);
	MNPC npc;
	npc.SetCreatureType(type);
	npc.SetID(900);
	npc.SetPosition(12, 34);
	const BYTE toPlayer = SelectFacingDirection(12, 34, PlayerX, PlayerY);
	const BYTE away = BYTE((toPlayer + 4) % 8);
	int facing = 0, standEnds = 0;
	std::srand(1);
	for (int frame = 0; frame < 400; ++frame)
	{
		if (attacking && npc.GetAction() == ACTION_STAND) npc.SetAction(ACTION_ATTACK);
		if (npc.GetAction() == ACTION_STAND && npc.GetActionCount() + 1 >= npc.GetActionCountMax()) ++standEnds;
		npc.SetDirection(away);
		npc.Action();
		if (npc.GetServerDirection() == toPlayer) ++facing;
	}
	return {facing, standEnds};
}
}

TEST(NPC, BathoryAndTepezLookAtThePlayerNowAndThen)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature somebody;
	somebody.SetCreatureType(5);
	somebody.SetPosition(PlayerX, PlayerY);
	player = &somebody;
	// At the end of a stand, one time in five: about 8 of 40 stand ends
	// (binomial, standard deviation 2.5; the bounds are about 3 of them).
	for (int type : {217, 366})
	{
		const std::pair<int, int> seen = FacingThePlayer(scene, type);
		CHECK(seen.second >= 30);
		CHECK(seen.first >= 1);
		CHECK(seen.first <= seen.second / 5 + 8);
	}
	// Not while it does something else, nor another NPC.
	CHECK_EQ(0, FacingThePlayer(scene, 217, true).first);
	CHECK_EQ(0, FacingThePlayer(scene, 300).first);
	// Without a player, they keep their way.
	player = nullptr;
	CHECK_EQ(0, FacingThePlayer(scene, 217).first);
}

TEST(NPC, SomeNPCsRestartTheirEffectsWithEachStand)
{
	Scene scene;
	scene.effectSprites.GetMutable(20)->FrameID = 33;
	for (int type : {635, 636, 639, 640})
	{
		scene.Tribe(type, CREATURETRIBE_NPC);
		MNPC npc;
		npc.SetCreatureType(type);
		npc.SetID(900);
		npc.SetPosition(12, 34);
		MAttachEffect* pEffect = npc.CreateAttachEffect(20, 0xFFFF, 0xFFFF);
		CHECK(pEffect != NULL);
		pEffect->SetFrameID(33, 8);
		npc.SetAction(ACTION_STAND);
		// Each frame the effect is two frames on; on the frame the stand
		// starts again (its count back at 0), these NPCs restart it.
		int restarted = 0, wraps = 0, early = 0;
		for (int frame = 0; frame < 40; ++frame)
		{
			pEffect->SetFrameID(33, 8);
			pEffect->NextFrame();
			pEffect->NextFrame();
			npc.Action();
			if (npc.GetActionCount() == 0)
			{
				++wraps;
				if (pEffect->GetFrame() == 0) ++restarted;
			}
			else if (pEffect->GetFrame() == 0)
				++early;
		}
		CHECK(wraps > 0);
		const bool restarts = type >= 636 && type <= 639;
		CHECK_EQ(restarts ? wraps : 0, restarted);
		// Never in the middle of a stand.
		CHECK_EQ(0, early);
	}
}
