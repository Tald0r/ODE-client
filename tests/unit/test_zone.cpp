#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "MZone.h"
#include "MSector.h"
#include "MCreature.h"
#include "MFakeCreature.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MActionInfoTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "EffectSpriteTypeDef.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "MZoneTable.h"
#include "MParty.h"
#include "MHelicopterManager.h"
#include "MChaseEffect.h"
#include "MGuidanceEffect.h"
#include "MEffectTarget.h"
#include "SoundNode.h"
#include "SoundDef.h"
#include "AmbientSoundState.h"
#include "ZoneFlags.h"
#include "MImageObject.h"
#include "MAnimationObject.h"
#include "SkillDef.h"
#include "MItem.h"
#include "MEffect.h"
#include "MAttachEffect.h"
#include "MEventManager.h"
#include "MEventQueue.h"
#include "FrameClock.h"
#include "MViewDef.h"
#include "ViewSectors.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"
#include "GameOptionsHost.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// MItemUse.cpp's, reached through MItemHost.
MItem*	NewItemOfClass(ITEM_CLASS itemClass);
// MZone.cpp's: whether a corpse is one of the relic tables that block.
bool	IsRelicTable(MItem* pItem);

// The zone (task 4.246, gamemodel's since then): the map of sectors the
// game's creatures, items and effects stand on. A real zone stands behind
// ZoneState here, as g_pZone; the player, the view, the world and the
// options are hosts that answer and record.
namespace {

std::vector<std::string> calls;
std::string N(long long value) { return std::to_string(value); }
using Calls = std::vector<std::string>;

bool Same(const Calls& expected)
{
	if (calls == expected) return true;
	std::printf("      calls: ");
	for (const std::string& call : calls) std::printf("[%s] ", call.c_str());
	std::printf("\n      wanted:");
	for (const std::string& call : expected) std::printf("[%s] ", call.c_str());
	std::printf("\n");
	return false;
}

// The zone's own creatures report themselves through ZoneState, as in the
// game: these entries forward to g_pZone the way GameInit's do.
const ZoneState::Host zoneHost = []() {
	ZoneState::Host host{};
	host.Exists = []() { return g_pZone != NULL; };
	host.GetID = []() { return g_pZone != NULL ? int(g_pZone->GetID()) : int(ZONEID_NULL); };
	host.GetCreature = [](TYPE_OBJECTID id) { return g_pZone->GetCreature(id); };
	host.GetWidth = []() { return g_pZone != NULL ? int(g_pZone->GetWidth()) : 0; };
	host.GetHeight = []() { return g_pZone != NULL ? int(g_pZone->GetHeight()) : 0; };
	host.GetSector = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) -> const MSector* { return &g_pZone->GetSector(x, y); };
	host.GetFakeCreature = [](TYPE_OBJECTID id) { return g_pZone != NULL ? g_pZone->GetFakeCreature(id) : NULL; };
	host.RemoveFakeCreature = [](TYPE_OBJECTID id) { return g_pZone->RemoveFakeCreature(id); };
	host.SetServerBlockIn = [](MZone* pZone, int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { pZone->SetServerBlock(BYTE(moveType), x, y); };
	host.UnSetServerBlockIn = [](MZone* pZone, int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { pZone->UnSetServerBlock(BYTE(moveType), x, y); };
	host.MoveGroundCreatureIn = [](MZone* pZone, MCreature* pCreature, TYPE_SECTORPOSITION xo, TYPE_SECTORPOSITION yo, TYPE_SECTORPOSITION xn, TYPE_SECTORPOSITION yn) {
		return pZone->MoveGroundCreature(pCreature, xo, yo, xn, yn);
	};
	host.MoveFlyingCreatureIn = [](MZone* pZone, MCreature* pCreature, TYPE_SECTORPOSITION xo, TYPE_SECTORPOSITION yo, TYPE_SECTORPOSITION xn, TYPE_SECTORPOSITION yn) {
		return pZone->MoveFlyingCreature(pCreature, xo, yo, xn, yn);
	};
	host.MoveUndergroundCreatureIn = [](MZone* pZone, MCreature* pCreature, TYPE_SECTORPOSITION xo, TYPE_SECTORPOSITION yo, TYPE_SECTORPOSITION xn, TYPE_SECTORPOSITION yn) {
		return pZone->MoveUndergroundCreature(pCreature, xo, yo, xn, yn);
	};
	host.CanMove = [](int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { return g_pZone->CanMove(BYTE(moveType), x, y); };
	host.CanMoveIn = [](MZone* pZone, int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { return pZone->CanMove(BYTE(moveType), x, y); };
	host.GetWidthIn = [](MZone* pZone) { return int(pZone->GetWidth()); };
	host.GetHeightIn = [](MZone* pZone) { return int(pZone->GetHeight()); };
	host.GetCreatureIn = [](MZone* pZone, TYPE_OBJECTID id) { return pZone->GetCreature(id); };
	host.DeleteCreature = [](MCreature* pCreature) { delete pCreature; };
	return host;
}();

// The player: a creature that reports itself as the player, as MPlayer
// does, with the effect statuses a test gives it and its item check
// buffer.
struct Player : MCreature
{
	CLASS_TYPE GetClassType() const override { return CLASS_PLAYER; }
	MZone* Zone() const { return m_pZone; }
};
MCreature* player = nullptr;
std::set<int> playerStatuses;
MItem* checkBuffer = nullptr;
const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.Creature = []() { return player; };
	host.GetID = []() { return player != nullptr ? player->GetID() : TYPE_OBJECTID(OBJECTID_NULL); };
	host.HasEffectStatus = [](EFFECTSTATUS status) { return playerStatuses.count(int(status)) != 0; };
	host.GetItemCheckBuffer = []() { return checkBuffer; };
	host.ClearItemCheckBuffer = []() { calls.push_back("clear check buffer"); checkBuffer = nullptr; };
	host.SendPositionInfoToParty = []() { calls.push_back("party position"); };
	host.SendStatusInfoToParty = []() { calls.push_back("party status"); };
	return host;
}();

// The view: a random monster type and the sprites of its tile and
// image-object packs.
int randomMonsterType = 33;
int tileSprites = 0;
std::map<int, std::vector<TYPE_SPRITEID>> imageObjectFrames;
const WorldEvents::Host worldHost = []() {
	WorldEvents::Host host{};
	host.ViewExists = []() { return true; };
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int startZ, int direction, TYPE_OBJECTID targetID,
		TYPE_SECTORPOSITION targetX, TYPE_SECTORPOSITION targetY, int targetZ, std::uint32_t delayFrame, MActionResult* pResult, bool bStartFromMainNode, int sX, int sY) {
		calls.push_back("skill|" + N(actionInfo) + "|" + N(startX) + "," + N(startY) + "," + N(startZ) + "|" + N(direction) + "|" + N(targetID)
			+ "|" + N(targetX) + "," + N(targetY) + "," + N(targetZ) + "|" + N(delayFrame) + "|" + (pResult != NULL ? "result" : "none")
			+ "|" + (bStartFromMainNode ? "main" : "first") + "|" + N(sX) + "," + N(sY));
	};
	host.PlaySoundAt = [](TYPE_SOUNDID soundID, bool repeat, int x, int y) { calls.push_back("sound|" + N(soundID) + "|" + N(repeat) + "|" + N(x) + "," + N(y)); };
	host.StopSound = [](TYPE_SOUNDID soundID) { calls.push_back("stop sound|" + N(soundID)); };
	host.GetRandomMonsterTypeInZone = []() { return randomMonsterType; };
	host.SetFirstDraw = []() { calls.push_back("first draw"); };
	host.GetTileSpriteCount = []() { return tileSprites; };
	host.GetImageObjectFrameCount = [](int frameID) {
		auto found = imageObjectFrames.find(frameID);
		return found == imageObjectFrames.end() ? 0 : int(found->second.size());
	};
	host.GetImageObjectFrameSprite = [](int frameID, int frame) { return std::uint16_t(imageObjectFrames[frameID][frame]); };
	return host;
}();
int personalShopUpdateTime = 0;
const GameOptions::Host optionsHost = []() {
	GameOptions::Host host{};
	host.PersonalShopUpdateTime = []() { return personalShopUpdateTime; };
	return host;
}();
const CreatureState::Host creatureHost = []() {
	CreatureState::Host host{};
	host.GetAddonItemClass = [](MCreature*, int, ITEM_CLASS* pItemClass) { *pItemClass = ITEM_CLASS_NULL; return false; };
	// A sector keeps its creatures as objects, as GameInit converts them.
	host.ObjectOf = [](const MCreature* pCreature) -> MObject* { return const_cast<MCreature*>(pCreature); };
	host.CreatureOf = [](MObject* pObject) { return static_cast<MCreature*>(pObject); };
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
MonotonicClock::TimePoint itemTime;
std::vector<int> itemsDestroyed;
int dropFrames = 4;
const MItemHost itemHost = []() {
	MItemHost host{};
	host.pCurrentTime = &itemTime;
	host.DropFrameCount = [](TYPE_FRAMEID) { return dropFrames; };
	host.NewItem = NewItemOfClass;
	host.ItemDestroyed = [](MItem* pItem) noexcept { itemsDestroyed.push_back(int(pItem->GetID())); };
	return host;
}();

// The zone, with its sector lookup open to the tests.
struct ZoneProbe : MZone
{
	using MZone::MZone;
	using MZone::SectorAt;
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
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	MEventQueue* oldEvents = g_pEventManager;
	MParty* oldParty = g_pParty;
	MZone* oldZone = g_pZone;
	const ZoneState::Host* oldZoneHost = ZoneState::SetHost(&zoneHost);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&playerHost);
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&worldHost);
	const GameOptions::Host* oldOptions = GameOptions::SetHost(&optionsHost);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creatureHost);
	const MEffectHost* oldEffectHost = MEffect::SetHost(&effectHost);
	const MAttachEffectHost* oldAttachHost = MAttachEffect::SetHost(&attachHost);
	DWORD oldFrame = g_CurrentFrame;
	// The zone last: it frees what it holds before the tables go.
	std::unique_ptr<ZoneProbe> zone;
	Player* me = nullptr;

	explicit Scene(int width = 40, int height = 30)
	{
		calls.clear();
		playerStatuses.clear();
		checkBuffer = nullptr;
		randomMonsterType = 33;
		tileSprites = 0;
		imageObjectFrames.clear();
		personalShopUpdateTime = 0;
		itemTime = MonotonicClock::TimePoint();
		dropFrames = 4;
		itemsDestroyed.clear();
		g_CurrentFrame = 1000;
		MItem::SetHost(&itemHost);
		creatureTypes.Init(800);
		g_pCreatureTable = &creatureTypes;
		Tribe(5);
		sprites.Init(4);
		g_pCreatureSpriteTable = &sprites;
		actions.Init(MAX_ACTIONINFO);
		for (int info = 0; info < MAX_ACTIONINFO; ++info)
			actions.GetMutable(info)->Set("", ACTION_STAND, EFFECTSPRITETYPE_NULL, 0, 0);
		actions.SetMinResultActionInfo(MIN_RESULT_ACTIONINFO);
		g_pActionInfoTable = &actions;
		// Each sprite type shows the frame of its own number, unless a test
		// says otherwise.
		effectSprites.Init(MAX_EFFECTSPRITETYPE);
		for (int type = 0; type < MAX_EFFECTSPRITETYPE; ++type)
			testfw::MutableRow(effectSprites, type).FrameID = TYPE_FRAMEID(type);
		g_pEffectSpriteTypeTable = &effectSprites;
		effectStatuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &effectStatuses;
		g_pRankBonusTable = &rankBonuses;
		g_pMonsterNameTable = &monsterNames;
		g_pLevelNameTable = &levelNames;
		g_pEventManager = &events;
		g_pParty = NULL;
		zone.reset(new ZoneProbe(TYPE_SECTORPOSITION(width), TYPE_SECTORPOSITION(height)));
		g_pZone = zone.get();
		// The player stands at (20, 15); the zone does not own it.
		me = new Player;
		me->SetZone(g_pZone);
		me->SetCreatureType(5);
		me->SetID(77);
		me->SetPosition(20, 15);
		me->SetServerPosition(20, 15);
		player = me;
	}
	~Scene()
	{
		zone.reset();
		g_pZone = oldZone;
		player = nullptr;
		delete me;
		MItem::SetHost(NULL);
		MAttachEffect::SetHost(oldAttachHost);
		MEffect::SetHost(oldEffectHost);
		CreatureState::SetHost(oldCreature);
		GameOptions::SetHost(oldOptions);
		WorldEvents::SetHost(oldWorld);
		PlayerState::SetHost(oldPlayer);
		ZoneState::SetHost(oldZoneHost);
		g_pParty = oldParty;
		g_pEventManager = oldEvents;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pRankBonusTable = oldRankBonuses;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pActionInfoTable = oldActions;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatureTypes;
		g_CurrentFrame = oldFrame;
	}
	// A creature type of the tribe, with a count for each action.
	void Tribe(int type, CREATURETRIBE tribe = CREATURETRIBE_SLAYER)
	{
		testfw::MutableRow(creatureTypes, type).SetCreatureTribe(tribe, ACTION_MAX_SLAYER);
		testfw::MutableRow(creatureTypes, type).MoveTimes = 8;
		testfw::MutableRow(creatureTypes, type).MoveTimesMotor = 8;
		testfw::MutableRow(creatureTypes, type).MoveRatio = 1;
		for (int action = 0; action < ACTION_MAX_SLAYER; ++action)
			testfw::MutableRow(creatureTypes, type).SetActionCount(action, 10 + action);
	}
	MSector& Sector(int x, int y) { return *zone->SectorAt(x, y); }
};

// A creature that counts its deletions.
int creaturesDeleted = 0;
struct Counted : MCreature
{
	~Counted() override { ++creaturesDeleted; }
	MZone* Zone() const { return m_pZone; }
	using MCreature::m_ActionCount;
	using MCreature::m_ActionCountMax;
};

// A creature of the type at the cell, made the way the game makes one
// before the zone takes it (GameInit's NewCreature gives it g_pZone).
template <class T = Counted>
T* Make(TYPE_OBJECTID id, int x, int y, int type = 5)
{
	T* pCreature = new T;
	pCreature->SetZone(g_pZone);
	pCreature->SetCreatureType(TYPE_CREATURETYPE(type));
	pCreature->SetID(id);
	pCreature->SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
	pCreature->SetServerPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
	return pCreature;
}

// An effect that counts its deletions, of a kind a test picks.
int effectsDeleted = 0;
struct CountedEffect : MEffect
{
	EFFECT_TYPE kind = EFFECT_SECTOR;
	explicit CountedEffect(EFFECT_TYPE type = EFFECT_SECTOR) : MEffect(BLT_EFFECT), kind(type) {}
	~CountedEffect() override { ++effectsDeleted; }
	EFFECT_TYPE GetEffectType() const override { return kind; }
};

// An effect of the sprite type (whose frame is the type's own number here)
// at the cell, lasting the frames.
CountedEffect* Effect(int type, int x, int y, DWORD frames = 100, MEffect::EFFECT_TYPE kind = MEffect::EFFECT_SECTOR)
{
	CountedEffect* pEffect = new CountedEffect(kind);
	pEffect->SetFrameID(TYPE_FRAMEID(type), 4);
	pEffect->SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
	pEffect->SetCount(frames);
	return pEffect;
}

// An item of the class and type at the cell.
MItem* Item(ITEM_CLASS itemClass, int type, TYPE_OBJECTID id, int x, int y)
{
	MItem* pItem = NewItemOfClass(itemClass);
	pItem->SetItemType(TYPE_ITEMTYPE(type));
	pItem->SetID(id);
	pItem->SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
	return pItem;
}

} // namespace

//----------------------------------------------------------------------
// The map
//----------------------------------------------------------------------
TEST(Zone, ItIsAGridOfSectorsOfItsSize)
{
	Scene scene(40, 30);
	ZoneProbe& zone = *scene.zone;
	CHECK_EQ(40, int(zone.GetWidth()));
	CHECK_EQ(30, int(zone.GetHeight()));
	CHECK(zone.IsPositionInZone(39, 29));
	CHECK(!zone.IsPositionInZone(40, 0));
	CHECK(!zone.IsPositionInZone(0, 30));
	CHECK(zone.SectorAt(39, 29) != NULL);
	CHECK(zone.SectorAt(40, 0) == NULL);
	CHECK(zone.SectorAt(0, -1) == NULL);
	CHECK(zone.SectorAt(-1, 0) == NULL);
	// A size of nothing leaves the grid as it was; a new one replaces it.
	zone.Init(0, 5);
	CHECK_EQ(40, int(zone.GetWidth()));
	zone.Init(7, 0);
	CHECK_EQ(30, int(zone.GetHeight()));
	zone.Init(12, 9);
	CHECK_EQ(12, int(zone.GetWidth()));
	CHECK_EQ(9, int(zone.GetHeight()));
	CHECK(zone.SectorAt(11, 8) != NULL);
	CHECK(zone.SectorAt(12, 8) == NULL);
}

TEST(Zone, ACreatureMayStandWhereItsLayerIsFree)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	CHECK(zone.CanMove(MCreature::CREATURE_GROUND, 3, 3));
	CHECK(!zone.CanMove(MCreature::CREATURE_GROUND, 40, 3));
	CHECK(!zone.CanMove(MCreature::CREATURE_FAKE_NO_BLOCK, 3, 30));
	// Each layer answers for its own kinds, fakes too.
	zone.SetBlockGroundSector(3, 3);
	CHECK(!zone.CanMove(MCreature::CREATURE_GROUND, 3, 3));
	CHECK(!zone.CanMove(MCreature::CREATURE_FAKE_GROUND, 3, 3));
	CHECK(zone.CanMove(MCreature::CREATURE_FLYING, 3, 3));
	CHECK(zone.CanMove(MCreature::CREATURE_UNDERGROUND, 3, 3));
	zone.SetBlockFlyingSector(4, 3);
	CHECK(!zone.CanMove(MCreature::CREATURE_FLYING, 4, 3));
	CHECK(!zone.CanMove(MCreature::CREATURE_FAKE_FLYING, 4, 3));
	CHECK(zone.CanMove(MCreature::CREATURE_GROUND, 4, 3));
	zone.SetBlockUndergroundSector(5, 3);
	CHECK(!zone.CanMove(MCreature::CREATURE_UNDERGROUND, 5, 3));
	CHECK(!zone.CanMove(MCreature::CREATURE_FAKE_UNDERGROUND, 5, 3));
	CHECK(zone.CanMove(MCreature::CREATURE_GROUND, 5, 3));
	// A fake nothing blocks goes anywhere in the zone.
	zone.SetBlockAllSector(6, 3);
	CHECK(zone.CanMove(MCreature::CREATURE_FAKE_NO_BLOCK, 6, 3));
	CHECK(!zone.CanMove(MCreature::CREATURE_GROUND, 6, 3));
	zone.UnSetBlockAllSector(6, 3);
	CHECK(zone.CanMove(MCreature::CREATURE_GROUND, 6, 3));
	// A sanctuary keeps out every real creature, never a fake.
	scene.Sector(7, 3).SetSanctuary();
	CHECK(!zone.CanMove(MCreature::CREATURE_GROUND, 7, 3));
	CHECK(!zone.CanMove(MCreature::CREATURE_FLYING, 7, 3));
	CHECK(!zone.CanMove(MCreature::CREATURE_UNDERGROUND, 7, 3));
	CHECK(zone.CanMove(MCreature::CREATURE_FAKE_GROUND, 7, 3));
	CHECK(zone.CanMove(MCreature::CREATURE_FAKE_FLYING, 7, 3));
	CHECK(zone.CanMove(MCreature::CREATURE_FAKE_UNDERGROUND, 7, 3));
	CHECK(zone.CanMove(MCreature::CREATURE_FAKE_NO_BLOCK, 7, 3));
}

TEST(Zone, AServerBlockIsSetForOneLayerAndClearedForAll)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	zone.SetServerBlock(MCreature::CREATURE_GROUND, 2, 2);
	CHECK(scene.Sector(2, 2).IsBlockServerGround());
	CHECK(!scene.Sector(2, 2).IsBlockServerFlying());
	CHECK(!scene.Sector(2, 2).IsBlockServerUnderground());
	CHECK(!zone.CanMove(MCreature::CREATURE_GROUND, 2, 2));
	zone.SetServerBlock(MCreature::CREATURE_FLYING, 2, 2);
	CHECK(scene.Sector(2, 2).IsBlockServerFlying());
	zone.SetServerBlock(MCreature::CREATURE_UNDERGROUND, 3, 2);
	CHECK(scene.Sector(3, 2).IsBlockServerUnderground());
	CHECK(!scene.Sector(3, 2).IsBlockServerGround());
	// A fake's type blocks nothing.
	zone.SetServerBlock(MCreature::CREATURE_FAKE_GROUND, 4, 2);
	CHECK(!scene.Sector(4, 2).IsBlockServerGround());
	// Off the zone: nothing, and no fault.
	zone.SetServerBlock(MCreature::CREATURE_GROUND, 40, 2);
	zone.UnSetServerBlock(MCreature::CREATURE_GROUND, 2, 30);
	// Clearing for any layer clears all three.
	scene.Sector(3, 2).SetBlockServerGround();
	scene.Sector(3, 2).SetBlockServerFlying();
	zone.UnSetServerBlock(MCreature::CREATURE_FLYING, 3, 2);
	CHECK(!scene.Sector(3, 2).IsBlockServerGround());
	CHECK(!scene.Sector(3, 2).IsBlockServerFlying());
	CHECK(!scene.Sector(3, 2).IsBlockServerUnderground());
}

//----------------------------------------------------------------------
// Creatures
//----------------------------------------------------------------------
TEST(Zone, ACreatureJoinsItsSectorOnItsLayer)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	testfw::MutableRow(*g_pCreatureTable, 6).bFlyingCreature = true;
	scene.Tribe(6);
	testfw::MutableRow(*g_pCreatureTable, 6).bFlyingCreature = true;
	Counted* walker = Make(1001, 3, 4);
	Counted* flier = Make(1002, 5, 4, 6);
	Counted* burrower = Make(1003, 7, 4);
	burrower->SetUndergroundCreature();
	CHECK(zone.AddCreature(walker));
	CHECK(zone.AddCreature(flier));
	CHECK(zone.AddCreature(burrower));
	CHECK_EQ(3, zone.GetCreatureNumber());
	CHECK(zone.GetCreature(1001) == walker);
	CHECK(scene.Sector(3, 4).GetGroundCreature() == walker);
	CHECK(scene.Sector(5, 4).GetFlyingCreature() == flier);
	CHECK(scene.Sector(7, 4).GetUndergroundCreature() == burrower);
	CHECK_EQ(int(MCreature::CREATURE_GROUND), int(walker->GetMoveType()));
	CHECK_EQ(int(MCreature::CREATURE_FLYING), int(flier->GetMoveType()));
	CHECK_EQ(int(MCreature::CREATURE_UNDERGROUND), int(burrower->GetMoveType()));
	// Each layer's ID by its cell; the ground's creature by its cell.
	CHECK_EQ(1001, int(zone.GetCreatureID(3, 4)));
	CHECK(zone.GetCreatureBySector(3, 4) == walker);
	CHECK_EQ(1002, int(zone.GetFlyingCreatureID(5, 4)));
	CHECK_EQ(1003, int(zone.GetUndergroundCreatureID(7, 4)));
	CHECK_EQ(int(OBJECTID_NULL), int(zone.GetCreatureID(5, 4)));
	CHECK_EQ(int(OBJECTID_NULL), int(zone.GetFlyingCreatureID(3, 4)));
	CHECK_EQ(int(OBJECTID_NULL), int(zone.GetUndergroundCreatureID(3, 4)));
	CHECK_EQ(int(OBJECTID_NULL), int(zone.GetCreatureID(40, 4)));
	CHECK(zone.GetCreatureBySector(3, 30) == NULL);
	CHECK_EQ(int(OBJECTID_NULL), int(zone.GetFlyingCreatureID(40, 4)));
	CHECK_EQ(int(OBJECTID_NULL), int(zone.GetUndergroundCreatureID(7, 30)));
	// It knows its zone now, whatever it had.
	CHECK(walker->Zone() == &zone);
	MZone elsewhere(5, 5);
	Counted* stranger = Make(1004, 9, 4);
	stranger->SetZone(&elsewhere);
	CHECK(zone.AddCreature(stranger));
	CHECK(stranger->Zone() == &zone);
}

TEST(Zone, ACreatureItCannotPlaceIsRefused)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	Counted* walker = Make(1001, 3, 4);
	CHECK(zone.AddCreature(walker));
	// The same ID again, a creature off the zone, or one nowhere: refused,
	// and still the caller's.
	std::unique_ptr<Counted> twin(Make(1001, 6, 4));
	CHECK(!zone.AddCreature(twin.get()));
	std::unique_ptr<Counted> outside(Make(1002, 40, 4));
	CHECK(!zone.AddCreature(outside.get()));
	std::unique_ptr<Counted> nowhere(new Counted);
	nowhere->SetCreatureType(5);
	nowhere->SetID(1003);
	CHECK(!zone.AddCreature(nowhere.get()));
	CHECK_EQ(1, zone.GetCreatureNumber());
	// A sector's layer holds a few: past them, refused.
	const int room = MSector::POSITION_GROUNDCREATURE_MAX - MSector::POSITION_GROUNDCREATURE + 1;
	for (int more = 1; more < room; ++more)
		CHECK(zone.AddCreature(Make(TYPE_OBJECTID(1100 + more), 3, 4)));
	CHECK_EQ(room, int(scene.Sector(3, 4).GetGroundCreatureSize()));
	std::unique_ptr<Counted> crowded(Make(1004, 3, 4));
	CHECK(!zone.AddCreature(crowded.get()));
	CHECK_EQ(room, zone.GetCreatureNumber());
	CHECK(zone.GetCreature(1004) == NULL);
}

TEST(Zone, ACreatureLeavingIsFreedAndItsServerBlockCleared)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	creaturesDeleted = 0;
	Counted* walker = Make(1001, 3, 4);
	CHECK(zone.AddCreature(walker));
	zone.SetServerBlock(MCreature::CREATURE_GROUND, 3, 4);
	CHECK(zone.RemoveCreature(1001));
	CHECK_EQ(1, creaturesDeleted);
	CHECK(zone.GetCreature(1001) == NULL);
	CHECK(scene.Sector(3, 4).GetGroundCreature() == NULL);
	CHECK(!scene.Sector(3, 4).IsBlockServerGround());
	CHECK(!zone.RemoveCreature(1001));
	// One found at its server cell instead of its own goes too.
	Counted* mover = Make(1002, 5, 4);
	CHECK(zone.AddCreature(mover));
	mover->SetX(6);
	CHECK(zone.RemoveCreature(1002));
	CHECK_EQ(2, creaturesDeleted);
	// One in neither cell stays, in the zone's list too.
	Counted* lost = Make(1003, 8, 4);
	CHECK(zone.AddCreature(lost));
	scene.Sector(8, 4).RemoveCreature(1003);
	CHECK(!zone.RemoveCreature(1003));
	CHECK_EQ(2, creaturesDeleted);
	CHECK(zone.GetCreature(1003) == lost);
}

TEST(Zone, ThePlayerJoinsWithoutASectorAndLeavesUnfreed)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	zone.SetPlayer();
	CHECK(Same({"party position", "party status"}));
	CHECK(zone.GetCreature(77) == scene.me);
	// The player's cell is the view's business, not the zone's.
	CHECK(scene.Sector(20, 15).GetGroundCreature() == NULL);
	CHECK(scene.me->Zone() == &zone);
	zone.RemovePlayer();
	CHECK(zone.GetCreature(77) == NULL);
	// (Scene frees it: the zone did not.)
}

TEST(Zone, ACreatureIsFoundByNameButNeverThePlayer)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	scene.Tribe(9, CREATURETRIBE_NPC);
	Counted* walker = Make(1001, 3, 4);
	walker->SetName("Rain");
	Counted* npc = Make(1002, 5, 4, 9);
	npc->SetName("Rain");
	zone.SetPlayer();
	scene.me->SetName("Rain");
	CHECK(zone.AddCreature(npc));
	CHECK(zone.AddCreature(walker));
	// Any creature of the name (the lower ID first, as the map is ordered);
	// with the flag, not an NPC.
	CHECK_EQ(1001, int(zone.GetCreatureID("Rain")));
	CHECK_EQ(1001, int(zone.GetCreatureID("Rain", 1)));
	CHECK(zone.RemoveCreature(1001));
	CHECK_EQ(1002, int(zone.GetCreatureID("Rain")));
	CHECK_EQ(int(OBJECTID_NULL), int(zone.GetCreatureID("Rain", 1)));
	CHECK_EQ(int(OBJECTID_NULL), int(zone.GetCreatureID("Snow")));
	zone.RemovePlayer();
}

TEST(Zone, AMoveShiftsACreatureToItsNewSector)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	Counted* walker = Make(1001, 3, 4);
	CHECK(zone.AddCreature(walker));
	CHECK(zone.MoveGroundCreature(walker, 3, 4, 4, 4));
	CHECK(scene.Sector(3, 4).GetGroundCreature() == NULL);
	CHECK(scene.Sector(4, 4).GetGroundCreature() == walker);
	// Not where it was said to be: taken from its server cell.
	walker->SetServerPosition(4, 4);
	CHECK(zone.MoveFlyingCreature(walker, 9, 9, 5, 4));
	CHECK(scene.Sector(4, 4).GetGroundCreature() == NULL);
	CHECK(scene.Sector(5, 4).GetFlyingCreature() == walker);
	CHECK(zone.MoveUndergroundCreature(walker, 5, 4, 6, 4));
	CHECK(scene.Sector(5, 4).GetFlyingCreature() == NULL);
	CHECK(scene.Sector(6, 4).GetUndergroundCreature() == walker);
	// Off the zone: refused, nothing moved.
	CHECK(!zone.MoveGroundCreature(walker, 6, 4, 40, 4));
	CHECK(!zone.MoveFlyingCreature(walker, 6, 4, 6, 30));
	CHECK(!zone.MoveUndergroundCreature(walker, 6, 4, 40, 30));
	CHECK(scene.Sector(6, 4).GetUndergroundCreature() == walker);
	// The player and the fakes keep out of the sectors.
	CHECK(zone.MoveGroundCreature(scene.me, 20, 15, 21, 15));
	CHECK(scene.Sector(21, 15).GetGroundCreature() == NULL);
	MFakeCreature fake;
	fake.SetCreatureType(5);
	fake.SetPosition(10, 10);
	CHECK(zone.MoveGroundCreature(&fake, 10, 10, 11, 10));
	CHECK(zone.MoveFlyingCreature(&fake, 10, 10, 11, 10));
	CHECK(zone.MoveUndergroundCreature(&fake, 10, 10, 11, 10));
	CHECK(scene.Sector(11, 10).GetGroundCreature() == NULL);
	CHECK(scene.Sector(11, 10).GetFlyingCreature() == NULL);
	CHECK(scene.Sector(11, 10).GetUndergroundCreature() == NULL);
}

//----------------------------------------------------------------------
// Who the zone's newcomers are
//----------------------------------------------------------------------
TEST(Zone, APartyMemberComingIntoSightIsMarkedAndItsInfoFilled)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	MParty party;
	g_pParty = &party;
	PARTY_INFO* pInfo = new PARTY_INFO;
	pInfo->Name = "Rain";
	pInfo->bInSight = false;
	CHECK(party.AddMember(pInfo));
	Counted* friendly = Make(1001, 3, 4);
	friendly->SetName("Rain");
	friendly->SetStatus(MODIFY_MAX_HP, 90);
	friendly->SetStatus(MODIFY_CURRENT_HP, 40);
	friendly->SetGuildNumber(12);
	CHECK(zone.AddCreature(friendly));
	CHECK(friendly->IsPlayerParty());
	CHECK(pInfo->bInSight);
	CHECK_EQ(40, int(pInfo->HP));
	CHECK_EQ(90, int(pInfo->MaxHP));
	CHECK_EQ(12, int(pInfo->guildID));
	// An NPC of the name, or a stranger: not.
	scene.Tribe(9, CREATURETRIBE_NPC);
	Counted* npc = Make(1002, 5, 4, 9);
	npc->SetName("Rain");
	CHECK(zone.AddCreature(npc));
	CHECK(!npc->IsPlayerParty());
	Counted* stranger = Make(1003, 7, 4);
	stranger->SetName("Snow");
	CHECK(zone.AddCreature(stranger));
	CHECK(!stranger->IsPlayerParty());
	g_pParty = NULL;
}

TEST(Zone, AHallucinatingPlayerSeesEveryoneElseAsMonsters)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	scene.Tribe(44);
	scene.Tribe(55);
	playerStatuses = {EFFECTSTATUS_HALLUCINATION};
	randomMonsterType = 44;
	Counted* first = Make(1001, 3, 4);
	CHECK(zone.AddCreature(first));
	CHECK(first->IsHallu());
	CHECK_EQ(44, int(first->GetHalluCreatureType()));
	playerStatuses.clear();
	Counted* second = Make(1002, 5, 4);
	CHECK(zone.AddCreature(second));
	CHECK(!second->IsHallu());
	// All at once, but the player.
	zone.SetPlayer();
	randomMonsterType = 55;
	zone.ChangeToHalluCreature();
	CHECK(first->IsHallu());
	CHECK_EQ(55, int(first->GetHalluCreatureType()));
	CHECK(second->IsHallu());
	CHECK(!scene.me->IsHallu());
	scene.me->SetHalluCreature(44);
	zone.RemoveHalluCreature();
	CHECK(!first->IsHallu());
	CHECK(!second->IsHallu());
	CHECK(scene.me->IsHallu());
	zone.RemovePlayer();
}

//----------------------------------------------------------------------
// Corpses
//----------------------------------------------------------------------
TEST(Zone, ADeadCreatureBecomesACorpseAtItsServerCell)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	creaturesDeleted = 0;
	Counted* dead = Make(1001, 3, 4);
	CHECK(zone.AddCreature(dead));
	dead->SetItemCount(3);
	dead->SetServerPosition(5, 4);
	CHECK(scene.Sector(5, 4).IsBlockServerGround());
	CHECK(zone.AddCorpseFromCreature(1001));
	CHECK(zone.GetCreatureOnly(1001) == NULL);
	CHECK(scene.Sector(3, 4).GetGroundCreature() == NULL);
	CHECK(!scene.Sector(5, 4).IsBlockServerGround());
	MItem* corpse = zone.GetItem(1001);
	CHECK(corpse != NULL && corpse->GetItemClass() == ITEM_CLASS_CORPSE);
	CHECK_EQ(1001, int(zone.GetItemID(5, 4)));
	CHECK(static_cast<MCorpse*>(corpse)->GetCreature() == dead);
	CHECK_EQ(3, int(corpse->GetNumber()));
	CHECK_EQ(0, creaturesDeleted);
	// An item on that cell makes way, freed.
	g_pItemTable->InitClass(ITEM_CLASS_SWORD, 1);
	MItem* sword = NewItemOfClass(ITEM_CLASS_SWORD);
	sword->SetID(5000);
	sword->SetPosition(7, 4);
	CHECK(zone.AddItem(sword));
	Counted* other = Make(1002, 7, 4);
	CHECK(zone.AddCreature(other));
	CHECK(zone.AddCorpseFromCreature(1002));
	CHECK(zone.GetItem(5000) == NULL);
	CHECK_EQ(1002, int(zone.GetItemID(7, 4)));
	// The player, or one not in the zone: no corpse.
	zone.SetPlayer();
	CHECK(!zone.AddCorpseFromCreature(77));
	CHECK(!zone.AddCorpseFromCreature(4242));
	zone.RemovePlayer();
	// The zone frees the corpses, and their creatures with them.
	scene.zone.reset();
	CHECK_EQ(2, creaturesDeleted);
}

TEST(Zone, ADyingCorpseStillAnswersForItsCreature)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	Counted* dead = Make(1001, 3, 4);
	CHECK(zone.AddCreature(dead));
	CHECK(zone.AddCorpseFromCreature(1001));
	CHECK(zone.GetCreatureOnly(1001) == NULL);
	// Still dying (its action not done): the corpse's creature answers.
	dead->SetAction(ACTION_STAND);
	dead->m_ActionCount = 0;
	CHECK(dead->m_ActionCountMax > 0);
	CHECK(zone.GetCreature(1001) == dead);
	dead->m_ActionCount = dead->m_ActionCountMax;
	CHECK(zone.GetCreature(1001) == NULL);
	// Another item of an ID: nothing.
	g_pItemTable->InitClass(ITEM_CLASS_SWORD, 1);
	MItem* sword = NewItemOfClass(ITEM_CLASS_SWORD);
	sword->SetID(5000);
	sword->SetPosition(8, 4);
	CHECK(zone.AddItem(sword));
	CHECK(zone.GetCreature(5000) == NULL);
}

TEST(Zone, TheRelicTablesAreTheirCorpseTypes)
{
	Scene scene;
	const std::pair<int, bool> types[] = {
		{370, false}, {371, true}, {376, true}, {377, false},
		{525, false}, {526, true}, {549, true}, {550, false},
		{559, false}, {560, true}, {563, true}, {564, false},
	};
	for (const auto& type : types)
	{
		scene.Tribe(type.first);
		MCorpse corpse;
		MCreature creature;
		creature.SetCreatureType(TYPE_CREATURETYPE(type.first));
		corpse.SetCreature(&creature);
		CHECK_EQ(type.second, IsRelicTable(&corpse));
		corpse.SetCreature(NULL);
	}
	g_pItemTable->InitClass(ITEM_CLASS_SWORD, 1);
	std::unique_ptr<MItem> sword(NewItemOfClass(ITEM_CLASS_SWORD));
	CHECK(!IsRelicTable(sword.get()));
}

TEST(Zone, ARelicCorpseBlocksItsCellUntilItGoes)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	scene.Tribe(371);
	Counted* relic = Make(1001, 3, 4, 371);
	CHECK(zone.AddCreature(relic));
	CHECK(zone.AddCorpseFromCreature(1001));
	CHECK(scene.Sector(3, 4).IsBlockGround());
	CHECK(scene.Sector(3, 4).IsBlockFlying());
	CHECK(zone.RemoveItem(1001));
	CHECK(!scene.Sector(3, 4).IsBlockGround());
	CHECK(!scene.Sector(3, 4).IsBlockFlying());
}

TEST(Zone, ACorpseOfMyKillIsFoundIfItHoldsEnough)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	Counted* mine = Make(1001, 3, 4);
	mine->SetDrainCreatureID(77);
	mine->SetItemCount(2);
	Counted* theirs = Make(1002, 5, 4);
	theirs->SetDrainCreatureID(88);
	theirs->SetItemCount(5);
	CHECK(zone.AddCreature(mine));
	CHECK(zone.AddCreature(theirs));
	CHECK(zone.AddCorpseFromCreature(1001));
	CHECK(zone.AddCorpseFromCreature(1002));
	CHECK(zone.GetCorpseKilledByMe() == zone.GetItem(1001));
	CHECK(zone.GetCorpseKilledByMe(1) == zone.GetItem(1001));
	CHECK(zone.GetCorpseKilledByMe(2) == NULL);
}

//----------------------------------------------------------------------
// Items
//----------------------------------------------------------------------
TEST(Zone, AnItemLandsInItsSectorFallingIfItHasADropAnimation)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	g_pItemTable->InitClass(ITEM_CLASS_SWORD, 2);
	g_pItemTable->InitClass(ITEM_CLASS_MOTORCYCLE, 1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).SetDropFrameID(14);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).TileSoundID = 66;
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 1).SetDropFrameID(FRAMEID_NULL);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_MOTORCYCLE, 0).SetDropFrameID(15);
	auto item = [](ITEM_CLASS itemClass, int type, TYPE_OBJECTID id, int x, int y) {
		MItem* pItem = NewItemOfClass(itemClass);
		pItem->SetItemType(TYPE_ITEMTYPE(type));
		pItem->SetID(id);
		pItem->SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
		return pItem;
	};
	MItem* sword = item(ITEM_CLASS_SWORD, 0, 5001, 3, 4);
	CHECK(zone.AddItem(sword, TRUE));
	CHECK(sword->IsDropping());
	CHECK_EQ(5001, int(zone.GetItemID(3, 4)));
	CHECK(zone.GetItem(5001) == sword);
	CHECK_EQ(1, zone.GetItemNumber());
	CHECK_EQ(int(OBJECTID_NULL), int(zone.GetItemID(4, 4)));
	CHECK_EQ(int(OBJECTID_NULL), int(zone.GetItemID(40, 4)));
	// Put down, not dropped; a motorcycle; no fall to show: none falls.
	MItem* laid = item(ITEM_CLASS_SWORD, 0, 5002, 4, 4);
	CHECK(zone.AddItem(laid));
	CHECK(!laid->IsDropping());
	MItem* bike = item(ITEM_CLASS_MOTORCYCLE, 0, 5003, 5, 4);
	CHECK(zone.AddItem(bike, TRUE));
	CHECK(!bike->IsDropping());
	MItem* plain = item(ITEM_CLASS_SWORD, 1, 5004, 6, 4);
	CHECK(zone.AddItem(plain, TRUE));
	CHECK(!plain->IsDropping());
	// Refused, and still the caller's: its ID again, off the zone, a
	// sector with an item.
	std::unique_ptr<MItem> twin(item(ITEM_CLASS_SWORD, 0, 5001, 8, 4));
	CHECK(!zone.AddItem(twin.get()));
	std::unique_ptr<MItem> outside(item(ITEM_CLASS_SWORD, 0, 5005, 40, 4));
	CHECK(!zone.AddItem(outside.get()));
	std::unique_ptr<MItem> stacked(item(ITEM_CLASS_SWORD, 0, 5006, 3, 4));
	CHECK(!zone.AddItem(stacked.get()));
	CHECK_EQ(4, zone.GetItemNumber());
	// Each update a frame of its fall; landed, its sound at the player.
	calls.clear();
	for (int frame = 0; frame < MAX_DROP_COUNT - 1; ++frame)
		zone.UpdateItem();
	CHECK(sword->IsDropping());
	CHECK(Same({}));
	zone.UpdateItem();
	CHECK(!sword->IsDropping());
	CHECK(Same({"sound|66|0|20,15"}));
	zone.UpdateItem();
	CHECK(Same({"sound|66|0|20,15"}));
}

TEST(Zone, ABloodBibleOrACastleSymbolShowsItsTileEffect)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	g_pItemTable->InitClass(ITEM_CLASS_BLOOD_BIBLE, 6);
	g_pItemTable->InitClass(ITEM_CLASS_CASTLE_SYMBOL, 10);
	g_pItemTable->InitClass(ITEM_CLASS_WAR_ITEM, 2);
	g_pItemTable->InitClass(ITEM_CLASS_SWORD, 1);
	auto add = [&](ITEM_CLASS itemClass, int type, TYPE_OBJECTID id, int x) {
		MItem* pItem = NewItemOfClass(itemClass);
		pItem->SetItemType(TYPE_ITEMTYPE(type));
		pItem->SetID(id);
		pItem->SetPosition(TYPE_SECTORPOSITION(x), 6);
		CHECK(zone.AddItem(pItem));
	};
	auto at = [](int info, int x) {
		return "skill|" + N(info) + "|" + N(x) + ",6,0|" + N(DIRECTION_DOWN) + "|" + N(OBJECTID_NULL) + "|" + N(x) + ",6,0|65535|none|main|0,0";
	};
	add(ITEM_CLASS_BLOOD_BIBLE, 2, 5001, 5);
	CHECK(Same({at(ARMEGA_TILE + 2, 5), at(DROP_BLOOD_BIBLE, 5)}));
	calls.clear();
	add(ITEM_CLASS_CASTLE_SYMBOL, 3, 5002, 6);
	CHECK(Same({at(AMBER_OF_CURSE_TILE, 6), at(DROP_BLOOD_BIBLE, 6)}));
	calls.clear();
	add(ITEM_CLASS_CASTLE_SYMBOL, 5, 5003, 7);
	CHECK(Same({at(AMBER_OF_LIGHT_TILE, 7), at(DROP_BLOOD_BIBLE, 7)}));
	calls.clear();
	// A symbol type past the six: the first.
	add(ITEM_CLASS_CASTLE_SYMBOL, 9, 5004, 8);
	CHECK(Same({at(AMBER_OF_GUARD_TILE, 8), at(DROP_BLOOD_BIBLE, 8)}));
	calls.clear();
	add(ITEM_CLASS_CASTLE_SYMBOL, 6, 5008, 12);
	CHECK(Same({at(AMBER_OF_GUARD_TILE, 12), at(DROP_BLOOD_BIBLE, 12)}));
	calls.clear();
	// The dragon's eye (war item 0) alone, without the bible's drop.
	add(ITEM_CLASS_WAR_ITEM, 0, 5005, 9);
	CHECK(Same({at(SKILL_CLIENT_DRAGON_EYES, 9)}));
	calls.clear();
	add(ITEM_CLASS_WAR_ITEM, 1, 5006, 10);
	add(ITEM_CLASS_SWORD, 0, 5007, 11);
	CHECK(Same({}));
}

TEST(Zone, ASpecialItemsTileEffectsGoWithIt)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	g_pItemTable->InitClass(ITEM_CLASS_BLOOD_BIBLE, 6);
	g_pItemTable->InitClass(ITEM_CLASS_CASTLE_SYMBOL, 6);
	g_pItemTable->InitClass(ITEM_CLASS_WAR_ITEM, 1);
	CHECK(zone.AddItem(Item(ITEM_CLASS_BLOOD_BIBLE, 2, 5001, 5, 6)));
	CHECK(zone.AddEffect(Effect(EFFECTSPRITETYPE_ARMEGA_TILE + 2, 5, 6)));
	CHECK(zone.AddEffect(Effect(EFFECTSPRITETYPE_DROP_BLOOD_BIBLE, 5, 6)));
	MEffect* other = Effect(10, 5, 6);
	CHECK(zone.AddEffect(other));
	CHECK(zone.RemoveItem(5001));
	CHECK(zone.GetItem(5001) == NULL);
	CHECK_EQ(1, int(scene.Sector(5, 6).GetEffectSize()));
	CHECK_EQ(1, zone.GetEffectCount());
	CHECK(zone.GetEffect(other->GetID()) == other);
	// Picked up, it is the picker's, its effects gone all the same.
	MItem* symbol = Item(ITEM_CLASS_CASTLE_SYMBOL, 3, 5002, 6, 6);
	CHECK(zone.AddItem(symbol));
	CHECK(zone.AddEffect(Effect(EFFECTSPRITETYPE_AMBER_OF_CURSE_TILE, 6, 6)));
	CHECK(zone.AddEffect(Effect(EFFECTSPRITETYPE_DROP_BLOOD_BIBLE, 6, 6)));
	CHECK(zone.PickupItem(5002));
	std::unique_ptr<MItem> picked(symbol);
	CHECK(zone.GetItem(5002) == NULL);
	CHECK_EQ(int(OBJECTID_NULL), int(zone.GetItemID(6, 6)));
	CHECK_EQ(0, int(scene.Sector(6, 6).GetEffectSize()));
	// The dragon's eye takes its own.
	CHECK(zone.AddItem(Item(ITEM_CLASS_WAR_ITEM, 0, 5003, 7, 6)));
	CHECK(zone.AddEffect(Effect(EFFECTSPRITETYPE_DRAGON_EYE, 7, 6)));
	CHECK(zone.RemoveItem(5003));
	CHECK_EQ(0, int(scene.Sector(7, 6).GetEffectSize()));
	CHECK(!zone.RemoveItem(5003));
	CHECK(!zone.PickupItem(5003));
}

TEST(Zone, AnItemLeavingEmptiesThePlayersCheckBufferIfItWasThere)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	g_pItemTable->InitClass(ITEM_CLASS_SWORD, 1);
	MItem* first = Item(ITEM_CLASS_SWORD, 0, 5001, 3, 4);
	MItem* second = Item(ITEM_CLASS_SWORD, 0, 5002, 4, 4);
	MItem* third = Item(ITEM_CLASS_SWORD, 0, 5003, 5, 4);
	CHECK(zone.AddItem(first));
	CHECK(zone.AddItem(second));
	CHECK(zone.AddItem(third));
	checkBuffer = first;
	CHECK(zone.RemoveItem(5002));
	CHECK(Same({}));
	CHECK(zone.RemoveItem(5001));
	CHECK(Same({"clear check buffer"}));
	// Picked up, it stays there: the picker's to settle.
	calls.clear();
	checkBuffer = third;
	CHECK(zone.PickupItem(5003));
	std::unique_ptr<MItem> picked(third);
	CHECK(Same({}));
	checkBuffer = nullptr;
}

//----------------------------------------------------------------------
// Effects
//----------------------------------------------------------------------
TEST(Zone, AnEffectWithAWaitIsHeldUntilItsTime)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	CountedEffect* waiting = Effect(20, 3, 4);
	waiting->SetWaitFrame(5);
	CHECK(zone.AddEffect(waiting, 1));
	CHECK_EQ(0, zone.GetEffectCount());
	zone.UpdateWaitEffects();
	CHECK_EQ(0, zone.GetEffectCount());
	g_CurrentFrame += 10;
	zone.UpdateWaitEffects();
	CHECK(zone.GetEffect(waiting->GetID()) == waiting);
	CHECK_EQ(1, int(scene.Sector(3, 4).GetEffectSize()));
	// The zone frees the held ones too.
	effectsDeleted = 0;
	CountedEffect* held = Effect(21, 3, 4);
	held->SetWaitFrame(50);
	CHECK(zone.AddEffect(held, 1));
	scene.zone.reset();
	CHECK_EQ(2, effectsDeleted);
}

TEST(Zone, AnEffectJoinsItsSectorAndARefusedOneIsFreed)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	effectsDeleted = 0;
	CountedEffect* here = Effect(20, 3, 4);
	const TYPE_OBJECTID hereID = here->GetID();
	CHECK(zone.AddEffect(here));
	CHECK(zone.GetEffect(here->GetID()) == here);
	CHECK_EQ(1, int(scene.Sector(3, 4).GetEffectSize()));
	// Off the zone: refused and freed, unless it chases (it is kept,
	// in no sector).
	CHECK(!zone.AddEffect(Effect(20, 40, 4)));
	CHECK_EQ(1, effectsDeleted);
	CountedEffect* chaser = Effect(20, 40, 4, 100, MEffect::EFFECT_CHASE);
	CHECK(zone.AddEffect(chaser));
	CHECK(zone.GetEffect(chaser->GetID()) == chaser);
	// Its ID again: refused and freed.
	CountedEffect* twin = Effect(30, 5, 4);
	twin->SetID(here->GetID());
	CHECK(!zone.AddEffect(twin));
	CHECK_EQ(2, effectsDeleted);
	CHECK(!zone.AddEffect(NULL));
	// Without the sprite table: refused and freed.
	EFFECTSPRITETYPE_TABLE empty;
	g_pEffectSpriteTypeTable = &empty;
	CHECK(!zone.AddEffect(Effect(20, 6, 4)));
	CHECK_EQ(3, effectsDeleted);
	g_pEffectSpriteTypeTable = &scene.effectSprites;
	// Removed: freed, gone from its sector; again, nothing.
	CHECK(zone.RemoveEffect(hereID));
	CHECK_EQ(4, effectsDeleted);
	CHECK_EQ(0, int(scene.Sector(3, 4).GetEffectSize()));
	CHECK(zone.GetEffect(hereID) == NULL);
	CHECK(!zone.RemoveEffect(hereID));
	CHECK(zone.RemoveEffect(chaser->GetID()));
	CHECK_EQ(5, effectsDeleted);
}

TEST(Zone, ASecondEffectOfTheSameFrameLengthensTheFirst)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	effectsDeleted = 0;
	CountedEffect* first = Effect(20, 3, 4, 100);
	CHECK(zone.AddEffect(first));
	const DWORD firstEnd = first->GetEndFrame();
	// Shorter: refused, the first as it was.
	CHECK(!zone.AddEffect(Effect(20, 3, 4, 50)));
	CHECK_EQ(firstEnd, first->GetEndFrame());
	CHECK_EQ(1, effectsDeleted);
	// Longer: refused, the first lasts as long (recorded: one frame
	// short, SetCount counting the frame it is set in).
	CountedEffect* longer = Effect(20, 3, 4, 300);
	const DWORD longerEnd = longer->GetEndFrame();
	CHECK(!zone.AddEffect(longer));
	CHECK_EQ(longerEnd - 1, first->GetEndFrame());
	CHECK_EQ(1, zone.GetEffectCount());
	// Another frame, a multi effect, a moving one or a dragon fire cracker
	// stays apart.
	CHECK(zone.AddEffect(Effect(21, 3, 4)));
	CountedEffect* multi = Effect(20, 3, 4);
	multi->SetMulti(true);
	CHECK(zone.AddEffect(multi));
	CHECK(zone.AddEffect(Effect(20, 3, 4, 100, MEffect::EFFECT_MOVING)));
	CountedEffect* cracker = Effect(EFFECTSPRITETYPE_DRAGON_FIRE_CRACKER, 5, 4, 100);
	CHECK(zone.AddEffect(cracker));
	CountedEffect* cracker2 = Effect(EFFECTSPRITETYPE_DRAGON_FIRE_CRACKER, 5, 4, 300);
	CHECK(zone.AddEffect(cracker2));
	CHECK(cracker->GetEndFrame() < cracker2->GetEndFrame());
	CHECK_EQ(6, zone.GetEffectCount());
}

TEST(Zone, DarknessOnDarknessOfItsKindMergesAndTheMiddleKindDarkens)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	struct Case { int first, second; bool merged; bool darkens; };
	const Case cases[] = {
		{EFFECTSPRITETYPE_DARKNESS_1_1, EFFECTSPRITETYPE_DARKNESS_1_5, true, false},
		{EFFECTSPRITETYPE_DARKNESS_2_1, EFFECTSPRITETYPE_DARKNESS_2_5, true, true},
		{EFFECTSPRITETYPE_DARKNESS_3_1, EFFECTSPRITETYPE_DARKNESS_3_5, true, false},
		{EFFECTSPRITETYPE_DARKNESS_1_1, EFFECTSPRITETYPE_DARKNESS_3_5, false, false},
		{EFFECTSPRITETYPE_GRAY_DARKNESS_2_1, EFFECTSPRITETYPE_GRAY_DARKNESS_2_5, true, true},
		{EFFECTSPRITETYPE_GRAY_DARKNESS_1_1, EFFECTSPRITETYPE_GRAY_DARKNESS_1_5, true, false},
		// Unmerged, a middle kind darkens its sector by itself.
		{EFFECTSPRITETYPE_GRAY_DARKNESS_3_1, EFFECTSPRITETYPE_GRAY_DARKNESS_2_1, false, true},
		{EFFECTSPRITETYPE_DARKNESS_2_1, EFFECTSPRITETYPE_GRAY_DARKNESS_2_1, false, true},
		{EFFECTSPRITETYPE_DARKNESS_2_1, EFFECTSPRITETYPE_DARKNESS_1_1, false, false},
	};
	int x = 1;
	for (const Case& c : cases)
	{
		CountedEffect* first = Effect(c.first, x, 4, 100);
		CHECK(zone.AddEffect(first));
		scene.Sector(x, 4).UnSetDarkness();
		CountedEffect* second = Effect(c.second, x, 4, 300);
		const DWORD secondEnd = second->GetEndFrame();
		CHECK_EQ(!c.merged, zone.AddEffect(second));
		CHECK_EQ(c.merged ? 1 : 2, int(scene.Sector(x, 4).GetEffectSize()));
		if (c.merged) CHECK_EQ(secondEnd - 1, first->GetEndFrame());
		CHECK_EQ(c.darkens, scene.Sector(x, 4).HasDarkness());
		x += 2;
	}
}

TEST(Zone, DarknessKeepsOffAForbiddenCellAShadowCorpseAndMercyGround)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	effectsDeleted = 0;
	scene.Sector(3, 4).SetDarknessForbidden();
	CHECK(!zone.AddEffect(Effect(EFFECTSPRITETYPE_DARKNESS_1_1, 3, 4)));
	CHECK_EQ(1, effectsDeleted);
	// Other effects may.
	CHECK(zone.AddEffect(Effect(20, 3, 4)));
	// A corpse of the shadow (670) on the cell.
	scene.Tribe(670);
	Counted* shadow = Make(1001, 5, 4, 670);
	CHECK(zone.AddCreature(shadow));
	CHECK(zone.AddCorpseFromCreature(1001));
	CHECK(!zone.AddEffect(Effect(EFFECTSPRITETYPE_GRAY_DARKNESS_1_1, 5, 4)));
	CHECK_EQ(2, effectsDeleted);
	// Mercy ground: each of its nine frames covers one cell round its own.
	CountedEffect* mercy = Effect(EFFECTSPRITETYPE_MERCY_GROUND_1 + 5, 10, 10);
	CHECK(zone.AddGroundEffect(mercy));
	// frame 5: one right, one down... (5 / 3 - 1, 5 % 3 - 1) = (1, 0).
	CHECK(!zone.AddEffect(Effect(EFFECTSPRITETYPE_DARKNESS_1_1, 11, 10)));
	CHECK(!zone.AddEffect(Effect(EFFECTSPRITETYPE_ACID_SWAP_1, 11, 10)));
	CHECK(!zone.AddEffect(Effect(EFFECTSPRITETYPE_PROMINENCE_ING, 11, 10)));
	CHECK(zone.AddEffect(Effect(EFFECTSPRITETYPE_DARKNESS_1_1, 10, 10)));
	CHECK(zone.AddEffect(Effect(20, 11, 10)));
	// The regen tower's ground: its own cell.
	CHECK(zone.AddGroundEffect(Effect(EFFECTSPRITETYPE_REGEN_TOWER_GROUND, 15, 10)));
	CHECK(!zone.AddEffect(Effect(EFFECTSPRITETYPE_DARKNESS_2_1, 15, 10)));
	CHECK(zone.AddEffect(Effect(EFFECTSPRITETYPE_DARKNESS_2_1, 16, 10)));
}

TEST(Zone, ASanctuaryOnAnotherEffectMarksTheSquareRoundIt)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	CHECK(zone.AddEffect(Effect(20, 5, 5)));
	CHECK(zone.AddEffect(Effect(EFFECTSPRITETYPE_SANCTUARY_1, 5, 5)));
	for (int y = 4; y <= 6; ++y)
		for (int x = 4; x <= 6; ++x)
			CHECK(scene.Sector(x, y).HasSanctuary());
	CHECK(!scene.Sector(7, 5).HasSanctuary());
	CHECK(!scene.Sector(5, 3).HasSanctuary());
	// At the edge, only what is in the zone.
	CHECK(zone.AddEffect(Effect(20, 0, 0)));
	CHECK(zone.AddEffect(Effect(EFFECTSPRITETYPE_SANCTUARY_3, 0, 0)));
	CHECK(scene.Sector(1, 1).HasSanctuary());
	// On a sector with no other effect it marks only its own (the sector
	// does), not the square.
	CHECK(zone.AddEffect(Effect(EFFECTSPRITETYPE_SANCTUARY_1, 20, 20)));
	CHECK(scene.Sector(20, 20).HasSanctuary());
	CHECK(!scene.Sector(21, 20).HasSanctuary());
	CHECK(!scene.Sector(19, 19).HasSanctuary());
	// Recorded: the lasting sanctuary (_2) is numbered after the vanishing
	// one (_3), so the range from _1 to _3 leaves it out: it marks nothing.
	CHECK(EFFECTSPRITETYPE_SANCTUARY_2 > EFFECTSPRITETYPE_SANCTUARY_3);
	CHECK(zone.AddEffect(Effect(20, 25, 20)));
	CHECK(zone.AddEffect(Effect(EFFECTSPRITETYPE_SANCTUARY_2, 25, 20)));
	CHECK(!scene.Sector(25, 20).HasSanctuary());
	CHECK(!scene.Sector(26, 20).HasSanctuary());
}

//----------------------------------------------------------------------
// Removing a tile's effect
//----------------------------------------------------------------------
TEST(Zone, ATilesEffectIsRemovedByItsSpriteType)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	effectsDeleted = 0;
	CountedEffect* fire = Effect(20, 3, 4);
	const TYPE_OBJECTID fireID = fire->GetID();
	CHECK(zone.AddEffect(fire));
	CHECK(zone.AddEffect(Effect(21, 3, 4)));
	CHECK(zone.RemoveTileEffect(3, 4, TYPE_EFFECTSPRITETYPE(20)));
	CHECK_EQ(1, effectsDeleted);
	CHECK(zone.GetEffect(fireID) == NULL);
	CHECK_EQ(1, int(scene.Sector(3, 4).GetEffectSize()));
	// None of the type there, none at all, a type with no frame: nothing.
	CHECK(!zone.RemoveTileEffect(3, 4, TYPE_EFFECTSPRITETYPE(22)));
	CHECK(!zone.RemoveTileEffect(5, 4, TYPE_EFFECTSPRITETYPE(21)));
	CHECK(!zone.RemoveTileEffect(3, 4, TYPE_EFFECTSPRITETYPE(EFFECTSPRITETYPE_NULL)));
	testfw::MutableRow(scene.effectSprites, 21).FrameID = FRAMEID_NULL;
	CHECK(!zone.RemoveTileEffect(3, 4, TYPE_EFFECTSPRITETYPE(21)));
	// A moving effect is not the tile's.
	CHECK(zone.AddEffect(Effect(23, 6, 4, 100, MEffect::EFFECT_MOVING)));
	CHECK(!zone.RemoveTileEffect(6, 4, TYPE_EFFECTSPRITETYPE(23)));
	// Any darkness takes the first darkness there (recorded: the frames
	// are compared with sprite type numbers, alike in this table).
	CHECK(zone.AddEffect(Effect(EFFECTSPRITETYPE_DARKNESS_1_1, 8, 4)));
	CHECK(zone.RemoveTileEffect(8, 4, TYPE_EFFECTSPRITETYPE(EFFECTSPRITETYPE_DARKNESS_3_5)));
	CHECK_EQ(0, int(scene.Sector(8, 4).GetEffectSize()));
}

TEST(Zone, ATilesEffectIsRemovedByTheStatusItShows)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	effectsDeleted = 0;
	auto shows = [&](int status, int type) { testfw::MutableRow(scene.effectStatuses, status).EffectSpriteType = TYPE_EFFECTSPRITETYPE(type); };
	// A status past the table: nothing.
	CHECK(!zone.RemoveTileEffect(3, 4, int(EFFECTSTATUS_MAX)));
	// Most: the effect of the sprite type the status shows.
	shows(EFFECTSTATUS_POISON, 30);
	CHECK(zone.AddEffect(Effect(30, 3, 4)));
	CHECK(zone.RemoveTileEffect(3, 4, int(EFFECTSTATUS_POISON)));
	CHECK_EQ(0, int(scene.Sector(3, 4).GetEffectSize()));
	// A trap or a trying position: the ground effect of its frame on the
	// cell (recorded: answering false all the same).
	shows(EFFECTSTATUS_TRAP_INSTALLED, 31);
	CountedEffect* trapElsewhere = Effect(31, 6, 4);
	CountedEffect* trap = Effect(31, 5, 4);
	CHECK(zone.AddGroundEffect(trap));
	CHECK(zone.AddGroundEffect(trapElsewhere));
	const TYPE_OBJECTID trapID = trap->GetID();
	CHECK(!zone.RemoveTileEffect(5, 4, int(EFFECTSTATUS_TRAP_INSTALLED)));
	CHECK(zone.GetGroundEffect(trapID) == NULL);
	CHECK(zone.GetGroundEffect(trapElsewhere->GetID()) == trapElsewhere);
	shows(EFFECTSTATUS_TRYING_POSITION, 31);
	CHECK(!zone.RemoveTileEffect(6, 4, int(EFFECTSTATUS_TRYING_POSITION)));
	CHECK_EQ(0, zone.GetGroundEffectCount());
	CHECK_EQ(3, effectsDeleted);
	// A vampire portal: the ground effect of its server ID.
	CountedEffect* portal = Effect(32, 7, 4);
	MPortalEffectTarget* pTarget = new MPortalEffectTarget(1);
	pTarget->SetServerID(4321);
	portal->SetLink(ACTIONINFO_NULL, pTarget);
	CHECK(zone.AddGroundEffect(portal));
	CHECK(!zone.RemoveTileEffect(9, 9, int(EFFECTSTATUS_VAMPIRE_PORTAL), 1234));
	CHECK_EQ(1, zone.GetGroundEffectCount());
	CHECK(!zone.RemoveTileEffect(9, 9, int(EFFECTSTATUS_VAMPIRE_PORTAL), 4321));
	CHECK_EQ(0, zone.GetGroundEffectCount());
	// The ground elemental's aura, darkness forbidden, the gnome's fury:
	// the sector's mark and the effect both go.
	shows(EFFECTSTATUS_GROUND_ELEMENTAL_AURA, 33);
	scene.Sector(10, 4).SetGroundElemental();
	CHECK(zone.AddEffect(Effect(33, 10, 4)));
	CHECK(zone.RemoveTileEffect(10, 4, int(EFFECTSTATUS_GROUND_ELEMENTAL_AURA)));
	CHECK(!scene.Sector(10, 4).HasGroundElemental());
	CHECK_EQ(0, int(scene.Sector(10, 4).GetEffectSize()));
	shows(EFFECTSTATUS_DARKNESS_FORBIDDEN, 34);
	scene.Sector(11, 4).SetDarknessForbidden();
	CHECK(!zone.RemoveTileEffect(11, 4, int(EFFECTSTATUS_DARKNESS_FORBIDDEN)));
	CHECK(!scene.Sector(11, 4).HasDarknessForbidden());
	shows(EFFECTSTATUS_FURY_OF_GNOME, 35);
	scene.Sector(12, 4).SetFuryOfGnome();
	CHECK(!zone.RemoveTileEffect(12, 4, int(EFFECTSTATUS_FURY_OF_GNOME)));
	CHECK(!scene.Sector(12, 4).HasFuryOfGnome());
	// A tile's image deleted: shown again, the view told to redraw.
	shows(EFFECTSTATUS_DELETE_TILE, 36);
	scene.Sector(13, 4).SetDisableTileImage();
	calls.clear();
	CHECK(!zone.RemoveTileEffect(13, 4, int(EFFECTSTATUS_DELETE_TILE)));
	CHECK(!scene.Sector(13, 4).HasDisableTileImage());
	CHECK(Same({"first draw"}));
	// A dragon tornado: the fake of its server ID.
	MFakeCreature* tornado = new MFakeCreature;
	tornado->SetCreatureType(5);
	tornado->SetID(6001);
	CHECK(zone.AddFakeCreature(tornado));
	CHECK(zone.RemoveTileEffect(0, 0, int(EFFECTSTATUS_DRAGON_TORNADO), 6001));
	CHECK(zone.GetFakeCreature(6001) == NULL);
	CHECK(!zone.RemoveTileEffect(0, 0, int(EFFECTSTATUS_DRAGON_TORNADO_CHILD), 6001));
}

//----------------------------------------------------------------------
// Effects over time
//----------------------------------------------------------------------
namespace {
// An effect that walks one cell right each update.
struct Walking : CountedEffect
{
	Walking() : CountedEffect(EFFECT_MOVING) {}
	bool Update() override
	{
		SetPosition(TYPE_SECTORPOSITION(GetX() + 1), GetY());
		return MEffect::Update();
	}
};
}

TEST(Zone, AnUpdateMovesEffectsAndEndsTheSpent)
{
	Scene scene(10, 10);
	ZoneProbe& zone = *scene.zone;
	effectsDeleted = 0;
	Walking* walker = new Walking;
	walker->SetFrameID(20, 4);
	walker->SetPosition(7, 3);
	walker->SetCount(100);
	CHECK(zone.AddEffect(walker));
	CountedEffect* brief = Effect(21, 2, 2, 1);
	CHECK(zone.AddEffect(brief));
	const TYPE_OBJECTID briefID = brief->GetID();
	CountedEffect* paused = Effect(22, 4, 4, 1);
	paused->SetDelayFrame(1000);
	CHECK(zone.AddEffect(paused));
	// A chasing one off the zone carries on.
	CountedEffect* chaser = Effect(23, 20, 3, 100, MEffect::EFFECT_CHASE);
	CHECK(zone.AddEffect(chaser));
	g_CurrentFrame += 5;
	zone.UpdateEffects();
	// It moved a cell; the spent one is gone; the paused one waits.
	CHECK_EQ(1, int(scene.Sector(8, 3).GetEffectSize()));
	CHECK_EQ(0, int(scene.Sector(7, 3).GetEffectSize()));
	CHECK(zone.GetEffect(briefID) == NULL);
	CHECK_EQ(0, int(scene.Sector(2, 2).GetEffectSize()));
	CHECK(zone.GetEffect(paused->GetID()) == paused);
	CHECK_EQ(1, effectsDeleted);
	// Walking off the zone, it goes.
	zone.UpdateEffects();
	CHECK_EQ(1, int(scene.Sector(9, 3).GetEffectSize()));
	zone.UpdateEffects();
	CHECK_EQ(0, int(scene.Sector(9, 3).GetEffectSize()));
	CHECK_EQ(2, effectsDeleted);
	CHECK_EQ(2, zone.GetEffectCount());
	CHECK(zone.GetEffect(chaser->GetID()) == chaser);
	CHECK(zone.RemoveEffect(chaser->GetID()));
}

TEST(Zone, GroundEffectsOfAKindOnACellMergeOrGiveWay)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	effectsDeleted = 0;
	// The magic elusion on the magic elusion, mercy ground on its own
	// kind: refused (recorded: not freed, the caller's), the first made
	// to last as long.
	CountedEffect* elusion = Effect(EFFECTSPRITETYPE_MAGIC_ELUSION_ING, 3, 4, 100);
	CHECK(zone.AddGroundEffect(elusion));
	std::unique_ptr<CountedEffect> again(Effect(EFFECTSPRITETYPE_MAGIC_ELUSION_ING, 3, 4, 300));
	CHECK(zone.AddGroundEffect(again.get()));
	CHECK(zone.GetGroundEffect(again->GetID()) == NULL);
	CHECK_EQ(again->GetEndFrame() - 1, elusion->GetEndFrame());
	CountedEffect* mercy = Effect(EFFECTSPRITETYPE_MERCY_GROUND_4, 5, 4, 300);
	CHECK(zone.AddGroundEffect(mercy));
	std::unique_ptr<CountedEffect> shorter(Effect(EFFECTSPRITETYPE_MERCY_GROUND_4, 5, 4, 50));
	CHECK(zone.AddGroundEffect(shorter.get()));
	CHECK(zone.GetGroundEffect(shorter->GetID()) == NULL);
	CHECK(mercy->GetEndFrame() > shorter->GetEndFrame());
	CHECK(zone.AddGroundEffect(Effect(EFFECTSPRITETYPE_MERCY_GROUND_5, 5, 4)));
	// An ice field on the regen tower's ground: refused.
	CHECK(zone.AddGroundEffect(Effect(EFFECTSPRITETYPE_REGEN_TOWER_GROUND, 7, 4)));
	for (int ice : {EFFECTSPRITETYPE_ICE_FIELD_1, EFFECTSPRITETYPE_ICE_FIELD_2, EFFECTSPRITETYPE_ICE_FIELD_3})
	{
		std::unique_ptr<CountedEffect> field(Effect(ice, 7, 4));
		CHECK(zone.AddGroundEffect(field.get()));
		CHECK(zone.GetGroundEffect(field->GetID()) == NULL);
	}
	// The ground elemental's end on its summoning: the summoning ends now,
	// and the end joins.
	CountedEffect* summoning = Effect(EFFECTSPRITETYPE_SUMMON_GROUND_ELEMENTAL_ING, 9, 4, 300);
	const TYPE_OBJECTID summoningID = summoning->GetID();
	CHECK(zone.AddGroundEffect(summoning));
	CountedEffect* end = Effect(EFFECTSPRITETYPE_SUMMON_GROUND_ELEMENTAL_END, 9, 4);
	CHECK(zone.AddGroundEffect(end));
	CHECK(zone.GetGroundEffect(end->GetID()) == end);
	CHECK(summoning->GetEndFrame() <= g_CurrentFrame);
	// Elsewhere: each joins.
	CHECK(zone.AddGroundEffect(Effect(EFFECTSPRITETYPE_MAGIC_ELUSION_ING, 11, 4)));
	CHECK_EQ(7, zone.GetGroundEffectCount());
	// Removed: freed; spent: gone at the next update.
	effectsDeleted = 0;
	const TYPE_OBJECTID mercyID = mercy->GetID();
	CHECK(zone.RemoveGroundEffect(mercyID));
	CHECK_EQ(1, effectsDeleted);
	CHECK(!zone.RemoveGroundEffect(mercyID));
	g_CurrentFrame += 1;
	zone.UpdateGroundEffects();
	CHECK(zone.GetGroundEffect(summoningID) == NULL);
	CHECK(zone.GetGroundEffect(end->GetID()) == end);
	g_CurrentFrame += 1000;
	zone.UpdateGroundEffects();
	CHECK_EQ(0, zone.GetGroundEffectCount());
}

TEST(Zone, TheSweepingVicesPrecastingTurnsIntoItsBlowAndGoes)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	CountedEffect* other = Effect(40, 3, 4);
	CountedEffect* first = Effect(EFFECTSPRITETYPE_SWEEP_VICE_PRECASTING_2, 5, 4);
	CountedEffect* second = Effect(EFFECTSPRITETYPE_SWEEP_VICE_PRECASTING_3, 6, 4);
	CHECK(zone.AddGroundEffect(other));
	CHECK(zone.AddGroundEffect(first));
	CHECK(zone.AddGroundEffect(second));
	// One (the first in the zone's order) becomes the blow, after a delay.
	zone.ChangeSwapViceType(0, 0, TYPE_EFFECTSPRITETYPE(77), 9);
	const int changed = (first->GetFrameID() == 77) + (second->GetFrameID() == 77);
	CHECK_EQ(1, changed);
	CHECK(other->GetFrameID() == 40);
	CHECK(first->GetFrameID() == 77 ? first->IsDelayFrame() : second->IsDelayFrame());
	// The rest of the precasting goes; the blow and the others stay.
	zone.RemoveSwapViceType();
	CHECK_EQ(2, zone.GetGroundEffectCount());
	CHECK(zone.GetGroundEffect(other->GetID()) == other);
}

//----------------------------------------------------------------------
// Fakes, helicopters, the ground's marks
//----------------------------------------------------------------------
TEST(Zone, AFakeIsKeptApartAndItsOwnerForgetsItWhenItGoes)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	Counted* owner = Make(1001, 3, 4);
	CHECK(zone.AddCreature(owner));
	MFakeCreature* pet = new MFakeCreature;
	pet->SetCreatureType(5);
	pet->SetID(6001);
	pet->SetOwnerID(1001);
	owner->SetPetID(6001);
	MFakeCreature* elemental = new MFakeCreature;
	elemental->SetCreatureType(5);
	elemental->SetID(6002);
	elemental->SetOwnerID(1001);
	owner->SetElementalID(6002);
	CHECK(zone.AddFakeCreature(pet));
	CHECK(zone.AddFakeCreature(elemental));
	CHECK(!zone.AddFakeCreature(pet));
	CHECK_EQ(2, zone.GetFakeCreatureNumber());
	CHECK(zone.GetFakeCreature(6001) == pet);
	CHECK(zone.GetCreature(6001) == NULL);
	CHECK(zone.RemoveFakeCreature(6001));
	CHECK_EQ(int(OBJECTID_NULL), int(owner->GetPetID()));
	CHECK_EQ(6002, int(owner->GetElementalID()));
	CHECK(zone.RemoveFakeCreature(6002));
	CHECK_EQ(int(OBJECTID_NULL), int(owner->GetElementalID()));
	CHECK(!zone.RemoveFakeCreature(6002));
	CHECK(zone.GetFakeCreature(6002) == NULL);
}

TEST(Zone, AFakeThatIsDoneLeavesAtTheUpdate)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	// Dragon tornadoes end out of the player's view (at 20, 15: 11 to 29
	// across, 2 to 29 down).
	auto tornado = [&](TYPE_OBJECTID id, int x, int y) {
		MFakeCreature* pFake = new MFakeCreature;
		pFake->SetCreatureType(5);
		pFake->SetID(id);
		pFake->SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
		pFake->SetFakeCreatureType(MFakeCreature::FAKE_CREATURE_DRAGON_TORNADO);
		CHECK(zone.AddFakeCreature(pFake));
	};
	tornado(6001, 25, 15);
	tornado(6002, 35, 15);
	zone.UpdateFakeCreature();
	CHECK(zone.GetFakeCreature(6001) != NULL);
	CHECK(zone.GetFakeCreature(6002) == NULL);
	CHECK_EQ(1, zone.GetFakeCreatureNumber());
}

TEST(Zone, AHelicopterIsCalledForACreatureAndLeavesWithIt)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	const MHelicopterHost helicopterHost{
		.GetCreaturePosition = [](TYPE_OBJECTID id, int& x, int& y) {
			MCreature* pCreature = g_pZone->GetCreature(id);
			if (pCreature == NULL) return false;
			x = pCreature->GetX();
			y = pCreature->GetY();
			return true;
		},
		.AddEffect = [](MEffect* pEffect) { g_pZone->AddEffect(pEffect); },
		.GetEffect = [](TYPE_OBJECTID id) { return g_pZone->GetEffect(id); },
		.RemoveEffect = [](TYPE_OBJECTID id) { g_pZone->RemoveEffect(id); },
		.GetMaxEffectFrame = [](BLT_TYPE, TYPE_FRAMEID) { return 1; },
		.PlayerID = []() { return TYPE_OBJECTID(77); },
		.IsSelectingWayPoint = []() { return false; },
		.SelectWayPoint = []() {},
		.PlayLoop = [](TYPE_SOUNDID, int, int) -> ZONESOUND_NODE* { return NULL; },
		.MoveSound = [](ZONESOUND_NODE*, int, int) {},
		.DeleteSound = [](ZONESOUND_NODE*) {},
	};
	const MHelicopterHost* oldHelicopter = MHelicopter::SetHost(&helicopterHost);
	// Its shadow follows the creature it is called for.
	const MGuidanceEffectHost guidanceHost{
		.CreaturePosition = [](TYPE_OBJECTID id, int& x, int& y, int& z) {
			MCreature* pCreature = g_pZone->GetCreature(id);
			if (pCreature == NULL) return false;
			x = pCreature->GetPixelX();
			y = pCreature->GetPixelY();
			z = 0;
			return true;
		},
	};
	const MGuidanceEffectHost* oldGuidance = MGuidanceEffect::SetHost(&guidanceHost);
	Counted* slayer = Make(1001, 3, 4);
	CHECK(zone.AddCreature(slayer));
	// For a creature not here: none.
	CHECK(!zone.AddHelicopter(4242, 0, 0));
	CHECK(zone.AddHelicopter(1001, 0, 0));
	MHelicopter* pHelicopter = zone.GetHelicopter(1001);
	CHECK(pHelicopter != NULL);
	// Its shadow chases the creature; sent off, the shadow chases no more
	// (the manager drops it when it arrives).
	MChaseEffect* pShadow = static_cast<MChaseEffect*>(zone.GetEffect(pHelicopter->GetEffectID()));
	CHECK(pShadow != NULL && pShadow->GetEffectType() == MEffect::EFFECT_CHASE);
	CHECK_EQ(1001, int(pShadow->GetTraceCreatureID()));
	zone.RemoveHelicopter(1001);
	CHECK(zone.GetHelicopter(1001) == pHelicopter);
	CHECK_EQ(int(OBJECTID_NULL), int(pShadow->GetTraceCreatureID()));
	// Without its shadow it goes at once.
	CHECK(zone.RemoveEffect(pHelicopter->GetEffectID()));
	zone.RemoveHelicopter(1001);
	CHECK(zone.GetHelicopter(1001) == NULL);
	// A slayer leaving the zone sends its helicopter off.
	CHECK(zone.AddHelicopter(1001, 0, 0));
	pShadow = static_cast<MChaseEffect*>(zone.GetEffect(zone.GetHelicopter(1001)->GetEffectID()));
	CHECK(zone.RemoveCreature(1001));
	CHECK_EQ(int(OBJECTID_NULL), int(pShadow->GetTraceCreatureID()));
	// Another tribe's does not.
	scene.Tribe(8, CREATURETRIBE_VAMPIRE);
	Counted* vampire = Make(1002, 5, 4, 8);
	CHECK(zone.AddCreature(vampire));
	CHECK(zone.AddHelicopter(1002, 0, 0));
	pShadow = static_cast<MChaseEffect*>(zone.GetEffect(zone.GetHelicopter(1002)->GetEffectID()));
	CHECK(zone.RemoveCreature(1002));
	CHECK_EQ(1002, int(pShadow->GetTraceCreatureID()));
	scene.zone.reset();
	MGuidanceEffect::SetHost(oldGuidance);
	MHelicopter::SetHost(oldHelicopter);
}

TEST(Zone, SafeCellsAndPortalsAreMarkedWithinTheZone)
{
	Scene scene(10, 8);
	ZoneProbe& zone = *scene.zone;
	RECT safe = { -2, -3, 2, 1 };
	zone.SetSafeSector(safe, 4);
	for (int y = 0; y <= 1; ++y)
		for (int x = 0; x <= 2; ++x)
			CHECK_EQ(4, int(scene.Sector(x, y).GetSafe()));
	CHECK_EQ(0, int(scene.Sector(3, 0).GetSafe()));
	CHECK_EQ(0, int(scene.Sector(0, 2).GetSafe()));
	RECT portal = { 8, 6, 12, 20 };
	zone.AddPortal(1, 1003, portal);
	CHECK_EQ(1, scene.Sector(9, 7).GetPortalSize());
	CHECK_EQ(1, scene.Sector(8, 6).GetPortalSize());
	CHECK_EQ(0, scene.Sector(7, 6).GetPortalSize());
	// Wholly off the zone: nothing.
	RECT away = { 20, 20, 30, 30 };
	zone.AddPortal(1, 1004, away);
	zone.SetSafeSector(away, 2);
	CHECK_EQ(0, int(scene.Sector(9, 7).GetSafe()));
}

TEST(Zone, ItsPKTypeIsItsZoneTableRows)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	CZoneTable* oldTable = g_pZoneTable;
	g_pZoneTable = NULL;
	CHECK_EQ(0, int(zone.GetPKType()));
	CZoneTable table;
	g_pZoneTable = &table;
	zone.SetCurrentZoneID(61);
	CHECK_EQ(0, int(zone.GetPKType()));
	ZONETABLE_INFO* pInfo = new ZONETABLE_INFO;
	pInfo->ID = 61;
	pInfo->PKType = 3;
	CHECK(table.Add(pInfo));
	CHECK_EQ(3, int(zone.GetPKType()));
	zone.SetCurrentZoneID(62);
	CHECK_EQ(0, int(zone.GetPKType()));
	g_pZoneTable = oldTable;
}

//----------------------------------------------------------------------
// What the player can see
//----------------------------------------------------------------------
TEST(Zone, WhatLeavesThePlayersSightIsDropped)
{
	Scene scene(80, 80);
	ZoneProbe& zone = *scene.zone;
	creaturesDeleted = 0;
	g_pItemTable->InitClass(ITEM_CLASS_SWORD, 1);
	// Around (40, 40): 30 to 50 across, 26 to 54 down, by server cell.
	const std::pair<TYPE_OBJECTID, std::pair<int, int>> creatures[] = {
		{20001, {40, 40}}, {20002, {30, 26}}, {20003, {50, 54}},
		{20004, {29, 40}}, {20005, {51, 40}}, {20006, {40, 25}}, {20007, {40, 55}},
	};
	for (const auto& c : creatures)
		CHECK(zone.AddCreature(Make(c.first, c.second.first, c.second.second)));
	// A creature the client made (an ID under 10000) stays anywhere.
	CHECK(zone.AddCreature(Make(500, 70, 70)));
	CHECK(zone.AddItem(Item(ITEM_CLASS_SWORD, 0, 5001, 40, 41)));
	CHECK(zone.AddItem(Item(ITEM_CLASS_SWORD, 0, 5002, 60, 41)));
	CountedEffect* inSight = Effect(20, 41, 40);
	CHECK(zone.AddEffect(inSight));
	CHECK(zone.AddEffect(Effect(20, 60, 40)));
	// A chasing effect, the map's black smoke and the lair's portal stay.
	CHECK(zone.AddEffect(Effect(21, 60, 41, 100, MEffect::EFFECT_CHASE)));
	CHECK(zone.AddEffect(Effect(EFFECTSPRITETYPE_MAP_BLACK_LARGE_SMOKE, 60, 42)));
	CHECK(zone.AddEffect(Effect(EFFECTSPRITETYPE_MAP_BLACK_SMALL_SMOKE_3, 60, 43)));
	CHECK(zone.AddEffect(Effect(EFFECTSPRITETYPE_GDR_LAIR_POTAL, 60, 44)));
	effectsDeleted = 0;
	zone.KeepObjectInSight(40, 40, 13);
	CHECK(zone.GetCreature(20001) != NULL);
	CHECK(zone.GetCreature(20002) != NULL);
	CHECK(zone.GetCreature(20003) != NULL);
	for (TYPE_OBJECTID gone : {20004, 20005, 20006, 20007})
		CHECK(zone.GetCreature(gone) == NULL);
	CHECK_EQ(4, creaturesDeleted);
	CHECK(zone.GetCreature(500) != NULL);
	CHECK(scene.Sector(29, 40).GetGroundCreature() == NULL);
	CHECK(zone.GetItem(5001) != NULL);
	CHECK(zone.GetItem(5002) == NULL);
	CHECK(itemsDestroyed == std::vector<int>({5002}));
	CHECK_EQ(int(OBJECTID_NULL), int(zone.GetItemID(60, 41)));
	CHECK(zone.GetEffect(inSight->GetID()) == inSight);
	CHECK_EQ(1, effectsDeleted);
	CHECK_EQ(0, int(scene.Sector(60, 40).GetEffectSize()));
	CHECK_EQ(5, zone.GetEffectCount());
	// Near an edge, the sight stops at it.
	zone.KeepObjectInSight(2, 2, 13);
	CHECK(zone.GetCreature(500) != NULL);
	CHECK(zone.GetCreature(20001) == NULL);
}

//----------------------------------------------------------------------
// The creatures' turn
//----------------------------------------------------------------------
TEST(Zone, EachCreatureActsEachUpdateTheDragonEyedTwice)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	Counted* plain = Make(1001, 3, 4);
	Counted* eyed = Make(1002, 5, 4);
	CHECK(zone.AddCreature(plain));
	CHECK(zone.AddCreature(eyed));
	eyed->AddEffectStatus(EFFECTSTATUS_DRAGON_EYES, 0xFFFF);
	plain->SetAction(ACTION_ATTACK);
	eyed->SetAction(ACTION_ATTACK);
	CHECK_EQ(0, int(plain->m_ActionCount));
	zone.UpdateAllCreature();
	CHECK_EQ(1, int(plain->m_ActionCount));
	CHECK_EQ(2, int(eyed->m_ActionCount));
	// The player acts once, or twice as a ghost or with a dragon's eyes.
	zone.SetPlayer();
	scene.me->SetAction(ACTION_ATTACK);
	zone.UpdateAllCreature();
	CHECK_EQ(1, int(static_cast<Counted*>(static_cast<MCreature*>(scene.me))->m_ActionCount));
	for (int status : {int(EFFECTSTATUS_GHOST), int(EFFECTSTATUS_DRAGON_EYES)})
	{
		playerStatuses = {status};
		scene.me->SetAction(ACTION_ATTACK);
		zone.UpdateAllCreature();
		CHECK_EQ(2, int(static_cast<Counted*>(static_cast<MCreature*>(scene.me))->m_ActionCount));
	}
	playerStatuses.clear();
	zone.RemovePlayer();
}

TEST(Zone, PersonalShopSignsAreRedrawnOnTheOptionsTime)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	const MonotonicClock::TimePoint oldNow = g_FrameNow;
	personalShopUpdateTime = 1000;
	Counted* shop = Make(1001, 3, 4);
	shop->SetPersnalShop(1);
	Counted* passer = Make(1002, 5, 4);
	CHECK(zone.AddCreature(shop));
	CHECK(zone.AddCreature(passer));
	// (The next time is set on the first update the program makes; one
	// update and then one past the period redraw it either way.)
	g_FrameNow = MonotonicClock::TimePoint() + MonotonicClock::Millis(1000000000);
	zone.UpdateAllCreature();
	shop->SetPersonalShopMsgTime(200);
	passer->SetPersonalShopMsgTime(200);
	g_FrameNow += MonotonicClock::Millis(2000);
	zone.UpdateAllCreature();
	CHECK(shop->GetPersonalShopMsgTime() <= 127);
	CHECK_EQ(200, int(passer->GetPersonalShopMsgTime()));
	// Within the period: as it was.
	shop->SetPersonalShopMsgTime(200);
	g_FrameNow += MonotonicClock::Millis(500);
	zone.UpdateAllCreature();
	CHECK_EQ(200, int(shop->GetPersonalShopMsgTime()));
	g_FrameNow += MonotonicClock::Millis(600);
	zone.UpdateAllCreature();
	CHECK(shop->GetPersonalShopMsgTime() <= 127);
	g_FrameNow = oldNow;
}

TEST(Zone, AnInvisibleCreatureFadedOutLeavesUnlessThePlayerSeesIt)
{
	struct Case { std::set<int> statuses; int tribe; bool stays; };
	const Case cases[] = {
		{{}, CREATURETRIBE_SLAYER, false},
		{{}, CREATURETRIBE_VAMPIRE, true},
		{{int(EFFECTSTATUS_DETECT_INVISIBILITY)}, CREATURETRIBE_SLAYER, true},
		{{int(EFFECTSTATUS_OBSERVING_EYE)}, CREATURETRIBE_SLAYER, true},
		{{int(EFFECTSTATUS_GNOME_WHISPER)}, CREATURETRIBE_SLAYER, true},
	};
	for (const Case& c : cases)
	{
		Scene scene;
		ZoneProbe& zone = *scene.zone;
		scene.Tribe(7, CREATURETRIBE(c.tribe));
		scene.me->SetCreatureType(7);
		playerStatuses = c.statuses;
		Counted* hidden = Make(1001, 3, 4);
		CHECK(zone.AddCreature(hidden));
		hidden->AddEffectStatus(EFFECTSTATUS_INVISIBILITY, 0xFFFF);
		hidden->SetInvisible();
		// Faded but not invisible by its status: it stays.
		Counted* faded = Make(1002, 5, 4);
		CHECK(zone.AddCreature(faded));
		faded->SetInvisible();
		for (int frame = 0; frame < 100 && zone.GetCreatureOnly(1001) != NULL; ++frame)
			zone.UpdateAllCreature();
		CHECK_EQ(c.stays, zone.GetCreatureOnly(1001) != NULL);
		CHECK(zone.GetCreatureOnly(1002) != NULL);
		CHECK_EQ(64, faded->GetInvisibleCount());
		playerStatuses.clear();
	}
}

//----------------------------------------------------------------------
// Sounds
//----------------------------------------------------------------------
TEST(Zone, ASoundPlaysWhenItsDelayIsUp)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	const MonotonicClock::TimePoint oldNow = g_FrameNow;
	const AmbientSoundState oldAmbient = g_ZoneAmbientSounds;
	const bool oldInLarge = g_bZonePlayerInLarge;
	const int oldSmall = g_nZoneSmall;
	CZoneTable* oldTable = g_pZoneTable;
	g_pZoneTable = NULL;
	g_bZonePlayerInLarge = false;
	g_nZoneSmall = 1003;
	g_ZoneAmbientSounds = AmbientSoundState();
	g_FrameNow = MonotonicClock::TimePoint() + MonotonicClock::Millis(1000000);
	zone.AddSound(SOUND_NODE(41, 500, 7, 8, g_FrameNow));
	calls.clear();
	zone.UpdateSound();
	CHECK(Same({}));
	g_FrameNow += MonotonicClock::Millis(600);
	zone.UpdateSound();
	CHECK(Same({"sound|41|0|7,8"}));
	// The propeller zones play it looping at the player, once; leaving
	// one stops it.
	calls.clear();
	g_nZoneSmall = 2106;
	zone.UpdateSound();
	zone.UpdateSound();
	CHECK(Same({"sound|" + N(SOUND_WORLD_PROPELLER) + "|1|20,15"}));
	calls.clear();
	g_nZoneSmall = 1003;
	zone.UpdateSound();
	CHECK(Same({"stop sound|" + N(SOUND_WORLD_PROPELLER)}));
	g_pZoneTable = oldTable;
	g_nZoneSmall = oldSmall;
	g_bZonePlayerInLarge = oldInLarge;
	g_ZoneAmbientSounds = oldAmbient;
	g_FrameNow = oldNow;
}

//----------------------------------------------------------------------
// The sprites to load, the image objects
//----------------------------------------------------------------------
TEST(Zone, TheSpritesNearThePlayerAreGathered)
{
	Scene scene(100, 100);
	ZoneProbe& zone = *scene.zone;
	tileSprites = 50;
	// Around (50, 50): 23 to 74 across, 11 to 89 down (the view's size
	// thrice, from three skips before it).
	scene.Sector(23, 11).Set(7, 0);
	scene.Sector(74, 89).Set(8, 0);
	scene.Sector(22, 50).Set(9, 0);
	scene.Sector(50, 90).Set(10, 0);
	scene.Sector(40, 40).Set(60, 0);		// past the tiles the view holds
	scene.Sector(41, 40).Set(50, 0);		// the first past them
	MImageObject* still = new MImageObject(1, 1, 30, 0, 0, 0, 0);
	MAnimationObject* moving = new MAnimationObject(2, 2, SPRITEID_NULL, 0, 0, 0, false);
	moving->SetFrameID(4, 3);
	imageObjectFrames[4] = {31, SPRITEID_NULL, 32};
	CHECK(zone.AddImageObject(still));
	CHECK(zone.AddImageObject(moving));
	zone.SetImageObjectSector(30, 30, 1);
	zone.SetImageObjectSector(31, 30, 2);
	std::set<TYPE_SPRITEID> tiles, objects;
	zone.GetNearSpriteSet(tiles, objects, 50, 50);
	CHECK(tiles == std::set<TYPE_SPRITEID>({0, 7, 8}));
	CHECK(objects == std::set<TYPE_SPRITEID>({30, 31, 32}));
	// Frames past the view's pack: none of them.
	imageObjectFrames.clear();
	objects.clear();
	zone.GetNearSpriteSet(tiles, objects, 50, 50);
	CHECK(objects == std::set<TYPE_SPRITEID>({30}));
}

TEST(Zone, AnImageObjectIsKeptAndPlacedByItsID)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	MImageObject* object = new MImageObject;
	object->SetID(7);
	CHECK(zone.AddImageObject(object));
	CHECK(!zone.AddImageObject(object));
	CHECK(!zone.AddImageObject(NULL));
	CHECK(zone.GetImageObject(7) == object);
	CHECK(zone.GetImageObject(8) == NULL);
	zone.SetImageObjectSector(3, 4, 7);
	zone.SetImageObjectSector(5, 4, 8);
	zone.SetImageObjectSector(40, 4, 7);
	CHECK_EQ(1, int(scene.Sector(3, 4).GetImageObjectSize()));
	CHECK_EQ(0, int(scene.Sector(5, 4).GetImageObjectSize()));
	zone.UnSetImageObjectSector(3, 4, 7);
	zone.UnSetImageObjectSector(3, 30, 7);
	CHECK_EQ(0, int(scene.Sector(3, 4).GetImageObjectSize()));
	CHECK(zone.RemoveImageObject(7));
	CHECK(!zone.RemoveImageObject(7));
	CHECK(zone.GetImageObject(7) == NULL);
}

//----------------------------------------------------------------------
// The zone's turn, and its end
//----------------------------------------------------------------------
TEST(Zone, AnUpdateRunsEachOfItsParts)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	const MonotonicClock::TimePoint oldNow = g_FrameNow;
	g_pItemTable->InitClass(ITEM_CLASS_SWORD, 1);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).SetDropFrameID(14);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).TileSoundID = 66;
	Counted* walker = Make(1001, 3, 4);
	CHECK(zone.AddCreature(walker));
	walker->SetAction(ACTION_ATTACK);
	MItem* sword = Item(ITEM_CLASS_SWORD, 0, 5001, 6, 4);
	CHECK(zone.AddItem(sword, TRUE));
	CountedEffect* waiting = Effect(20, 7, 4);
	waiting->SetWaitFrame(1);
	CHECK(zone.AddEffect(waiting, 1));
	const TYPE_OBJECTID briefID = [&]() { CountedEffect* brief = Effect(21, 8, 4, 1); CHECK(zone.AddEffect(brief)); return brief->GetID(); }();
	CountedEffect* ground = Effect(22, 9, 4, 1);
	CHECK(zone.AddGroundEffect(ground));
	const TYPE_OBJECTID groundID = ground->GetID();
	MFakeCreature* tornado = new MFakeCreature;
	tornado->SetCreatureType(5);
	tornado->SetID(6001);
	tornado->SetPosition(35, 15);
	tornado->SetFakeCreatureType(MFakeCreature::FAKE_CREATURE_DRAGON_TORNADO);
	CHECK(zone.AddFakeCreature(tornado));
	g_FrameNow = MonotonicClock::TimePoint() + MonotonicClock::Millis(1000000);
	zone.AddSound(SOUND_NODE(41, 0, 7, 8, g_FrameNow - MonotonicClock::Millis(1)));
	g_CurrentFrame += 5;
	calls.clear();
	zone.Update();
	CHECK_EQ(1, int(walker->m_ActionCount));
	CHECK(zone.GetFakeCreature(6001) == NULL);
	CHECK(sword->IsDropping());
	CHECK(zone.GetEffect(waiting->GetID()) == waiting);
	CHECK(zone.GetEffect(briefID) == NULL);
	CHECK(zone.GetGroundEffect(groundID) == NULL);
	CHECK(std::find(calls.begin(), calls.end(), "sound|41|0|7,8") != calls.end());
	g_FrameNow = oldNow;
}

TEST(Zone, ReleasingItsObjectsFreesThemAllButThePlayer)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	creaturesDeleted = 0;
	effectsDeleted = 0;
	g_pItemTable->InitClass(ITEM_CLASS_SWORD, 1);
	zone.SetPlayer();
	CHECK(zone.AddCreature(Make(1001, 3, 4)));
	scene.Tribe(371);
	CHECK(zone.AddCreature(Make(1002, 5, 4, 371)));
	CHECK(zone.AddCorpseFromCreature(1002));
	CHECK(scene.Sector(5, 4).IsBlockGround());
	CHECK(zone.AddItem(Item(ITEM_CLASS_SWORD, 0, 5001, 6, 4)));
	CHECK(zone.AddEffect(Effect(20, 7, 4)));
	CHECK(zone.AddGroundEffect(Effect(21, 8, 4)));
	CountedEffect* waiting = Effect(22, 9, 4);
	waiting->SetWaitFrame(100);
	CHECK(zone.AddEffect(waiting, 1));
	MFakeCreature* pet = new MFakeCreature;
	pet->SetCreatureType(5);
	pet->SetID(6001);
	CHECK(zone.AddFakeCreature(pet));
	MImageObject* object = new MImageObject;
	object->SetID(7);
	CHECK(zone.AddImageObject(object));
	zone.ReleaseObject();
	CHECK_EQ(0, zone.GetCreatureNumber());
	CHECK_EQ(2, creaturesDeleted);
	CHECK_EQ(0, zone.GetItemNumber());
	CHECK_EQ(0, zone.GetEffectCount());
	CHECK_EQ(0, zone.GetGroundEffectCount());
	CHECK_EQ(3, effectsDeleted);
	CHECK_EQ(0, zone.GetFakeCreatureNumber());
	CHECK(scene.Sector(3, 4).GetGroundCreature() == NULL);
	CHECK(!scene.Sector(5, 4).IsBlockGround());
	CHECK(!scene.Sector(5, 4).IsExistItem());
	CHECK_EQ(0, int(scene.Sector(7, 4).GetEffectSize()));
	// The image objects are the map's: they stay until it goes.
	CHECK(zone.GetImageObject(7) == object);
	// (The player is the Scene's to free.)
}

//----------------------------------------------------------------------
// A server's tile off the map
//----------------------------------------------------------------------
TEST(Zone, ATilesEffectOffTheMapIsNotLookedFor)
{
	Scene scene(10, 8);
	ZoneProbe& zone = *scene.zone;
	// GCDeleteEffectFromTile hands its packet's tile on unchecked: one
	// past an edge, or a sector number far off, finds nothing to remove
	// and reads no sector.
	testfw::MutableRow(scene.effectStatuses, EFFECTSTATUS_POISON).EffectSpriteType = 30;
	CHECK(zone.AddEffect(Effect(30, 9, 7)));
	for (const std::pair<int, int>& tile : {std::pair<int, int>{10, 0}, {0, 8}, {60000, 60000}})
	{
		const TYPE_SECTORPOSITION x = TYPE_SECTORPOSITION(tile.first), y = TYPE_SECTORPOSITION(tile.second);
		CHECK(!zone.RemoveTileEffect(x, y, TYPE_EFFECTSPRITETYPE(30)));
		CHECK(!zone.RemoveTileEffect(x, y, int(EFFECTSTATUS_POISON)));
		// The sector marks by status: none set, none read; no redraw.
		calls.clear();
		for (int status : {int(EFFECTSTATUS_GROUND_ELEMENTAL_AURA), int(EFFECTSTATUS_DARKNESS_FORBIDDEN),
			int(EFFECTSTATUS_FURY_OF_GNOME), int(EFFECTSTATUS_DELETE_TILE)})
			CHECK(!zone.RemoveTileEffect(x, y, status));
		CHECK(Same({}));
	}
	CHECK_EQ(1, int(scene.Sector(9, 7).GetEffectSize()));
	// A tornado or a trap is found by its ID or cell, not by indexing.
	MFakeCreature* tornado = new MFakeCreature;
	tornado->SetCreatureType(5);
	tornado->SetID(6001);
	CHECK(zone.AddFakeCreature(tornado));
	CHECK(zone.RemoveTileEffect(60000, 60000, int(EFFECTSTATUS_DRAGON_TORNADO), 6001));
}

TEST(Zone, AChasingEffectOffTheMapIsFreedWithTheZonesObjects)
{
	Scene scene(10, 10);
	ZoneProbe& zone = *scene.zone;
	effectsDeleted = 0;
	// A helicopter's shadow starts 500 pixels right and 300 up of its
	// creature: near an edge its sector lies off the map (above the top,
	// a sector number past 65000). The zone keeps such chasing effects.
	CHECK(zone.AddEffect(Effect(23, 20, 3, 100, MEffect::EFFECT_CHASE)));
	CHECK(zone.AddEffect(Effect(23, 3, 65000, 100, MEffect::EFFECT_CHASE)));
	CHECK(zone.AddEffect(Effect(24, 3, 3)));
	zone.ReleaseObject();
	CHECK_EQ(3, effectsDeleted);
	CHECK_EQ(0, zone.GetEffectCount());
	CHECK_EQ(0, int(scene.Sector(3, 3).GetEffectSize()));
}

TEST(Zone, ACorpseWhoseServerCellIsOffTheMapIsNotMade)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	creaturesDeleted = 0;
	// GCFastMove hands its packet's tile on unchecked: the creature keeps
	// a server cell off the map (the zone refused to block it). Then it
	// dies: no corpse can lie there, and the creature, out of the zone
	// already, is freed rather than lost.
	Counted* runner = Make(1001, 3, 4);
	CHECK(zone.AddCreature(runner));
	runner->SetServerPosition(255, 4);
	CHECK(!zone.AddCorpseFromCreature(1001));
	CHECK_EQ(1, creaturesDeleted);
	CHECK(zone.GetCreatureOnly(1001) == NULL);
	CHECK(zone.GetItem(1001) == NULL);
	CHECK(scene.Sector(3, 4).GetGroundCreature() == NULL);
	CHECK_EQ(0, zone.GetItemNumber());
	// Its own cell off the map, its server cell on it: taken from the
	// server cell, and the corpse lies there.
	Counted* stray = Make(1002, 5, 4);
	CHECK(zone.AddCreature(stray));
	stray->SetX(255);
	CHECK(zone.AddCorpseFromCreature(1002));
	CHECK(scene.Sector(5, 4).GetGroundCreature() == NULL);
	CHECK_EQ(1002, int(zone.GetItemID(5, 4)));
}

TEST(Zone, AMoveFromACellOffTheMapTakesTheServerCell)
{
	Scene scene;
	ZoneProbe& zone = *scene.zone;
	Counted* walker = Make(1001, 3, 4);
	CHECK(zone.AddCreature(walker));
	// Told it was off the map: taken from its server cell instead.
	CHECK(zone.MoveGroundCreature(walker, 255, 4, 5, 4));
	CHECK(scene.Sector(3, 4).GetGroundCreature() == NULL);
	CHECK(scene.Sector(5, 4).GetGroundCreature() == walker);
	walker->SetServerPosition(5, 4);
	CHECK(zone.MoveFlyingCreature(walker, 5, 65000, 6, 4));
	CHECK(scene.Sector(6, 4).GetFlyingCreature() == walker);
	walker->SetServerPosition(6, 4);
	CHECK(zone.MoveUndergroundCreature(walker, 300, 300, 7, 4));
	CHECK(scene.Sector(7, 4).GetUndergroundCreature() == walker);
	// Both cells off the map: nothing to take it from; it joins the new
	// cell all the same.
	walker->SetServerPosition(255, 255);
	CHECK(zone.MoveGroundCreature(walker, 255, 4, 8, 4));
	CHECK(scene.Sector(8, 4).GetGroundCreature() == walker);
}
