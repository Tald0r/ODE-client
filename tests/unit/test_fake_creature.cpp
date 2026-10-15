#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "MFakeCreature.h"
#include "MSector.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MActionInfoTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "EffectSpriteTypeDef.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "SkillDef.h"
#include "MItem.h"
#include "MEffect.h"
#include "MAttachEffect.h"
#include "MEventManager.h"
#include "MEventQueue.h"
#include "FrameClock.h"
#include "MViewDef.h"
#include "ViewSectors.h"
#include "WorldTileGeometry.h"
#include "DirectionSelection.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

// MItemUse.cpp's, reached through MItemHost.
MItem*	NewItemOfClass(ITEM_CLASS itemClass);

// The fake creature (task 4.245, gamemodel's since then): a pet, an
// elemental, a turret, a rocket, a wanderer or a wild wolf the client moves
// on its own. A zone of cells the fakes block or free, the player, the view
// and the world are hosts that answer and record.
namespace {

std::vector<std::string> calls;
std::string N(long long value) { return std::to_string(value); }
using Calls = std::vector<std::string>;
using Cell = std::pair<int, int>;

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

// The creature's own zone: an address the fakes compare.
int zoneMarker;
// Two zones that answer differently, so a read of the wrong one shows. The
// fake's own zone (m_pZone, the ...In entries): a width and height, the
// cells nothing can stand on and its creatures. The player's zone
// (g_pZone): its own size and blocked cells, the cells a path search has
// visited, its creatures and the fake IDs taken.
bool zoneExists = true;
int zoneWidth = 60, zoneHeight = 60;
std::set<Cell> blocked;
std::map<TYPE_OBJECTID, MCreature*> ownZoneCreatures;
int playerZoneWidth = 200, playerZoneHeight = 200;
std::set<Cell> playerBlocked;
std::set<Cell> visited;
int unvisits = 0;
std::map<TYPE_OBJECTID, MCreature*> creatures;
std::set<TYPE_OBJECTID> fakeIDsTaken;
POINT firstSector = { 10, 10 };
MZone* OwnZone() { return reinterpret_cast<MZone*>(&zoneMarker); }
std::vector<MZone*> zonesAsked;
// The move type each free-cell question was asked with.
std::vector<int> moveTypesAsked;
bool Free(int x, int y) { return x >= 0 && y >= 0 && x < zoneWidth && y < zoneHeight && blocked.count({x, y}) == 0; }
bool PlayerZoneFree(int x, int y) { return x >= 0 && y >= 0 && x < playerZoneWidth && y < playerZoneHeight && playerBlocked.count({x, y}) == 0; }
// Every free-cell question so far was asked with this move type.
bool AskedAs(int moveType)
{
	if (moveTypesAsked.empty()) return false;
	for (int asked : moveTypesAsked)
		if (asked != moveType) return false;
	return true;
}

const ZoneState::Host zoneHost = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetWidth = []() { return playerZoneWidth; };
	// Placing a creature reads its sector (darkness): one plain sector.
	host.GetSector = [](TYPE_SECTORPOSITION, TYPE_SECTORPOSITION) -> const MSector* { static MSector sector; return &sector; };
	host.GetHeight = []() { return playerZoneHeight; };
	// GetFakeID only asks whether the ID is taken: a marker answers.
	host.GetFakeCreature = [](TYPE_OBJECTID id) -> MCreature* {
		return fakeIDsTaken.count(id) != 0 ? reinterpret_cast<MCreature*>(&zoneMarker) : nullptr;
	};
	host.GetCreature = [](TYPE_OBJECTID id) -> MCreature* {
		auto found = creatures.find(id);
		return found == creatures.end() ? nullptr : found->second;
	};
	host.SetVisitedFlag = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { visited.insert({x, y}); };
	host.UnSetVisitedFlag = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { visited.erase({x, y}); ++unvisits; };
	host.IsVisitedFlag = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { return visited.count({x, y}) != 0; };
	host.CanMove = [](int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { moveTypesAsked.push_back(moveType); return PlayerZoneFree(x, y); };
	host.CanMoveIn = [](MZone* pZone, int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		zonesAsked.push_back(pZone);
		moveTypesAsked.push_back(moveType);
		return Free(x, y);
	};
	host.GetWidthIn = [](MZone* pZone) { zonesAsked.push_back(pZone); return zoneWidth; };
	host.GetHeightIn = [](MZone* pZone) { zonesAsked.push_back(pZone); return zoneHeight; };
	host.GetCreatureIn = [](MZone* pZone, TYPE_OBJECTID id) -> MCreature* {
		zonesAsked.push_back(pZone);
		auto found = ownZoneCreatures.find(id);
		return found == ownZoneCreatures.end() ? nullptr : found->second;
	};
	host.MoveGroundCreatureIn = [](MZone*, MCreature*, TYPE_SECTORPOSITION, TYPE_SECTORPOSITION, TYPE_SECTORPOSITION, TYPE_SECTORPOSITION) { return true; };
	host.MoveFlyingCreatureIn = [](MZone*, MCreature*, TYPE_SECTORPOSITION, TYPE_SECTORPOSITION, TYPE_SECTORPOSITION, TYPE_SECTORPOSITION) { return true; };
	return host;
}();

MCreature* player = nullptr;
bool playerGhost = false;
const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.Creature = []() { return player; };
	host.GetID = []() { return player != nullptr ? player->GetID() : TYPE_OBJECTID(OBJECTID_NULL); };
	host.HasEffectStatus = [](EFFECTSTATUS status) { return status == EFFECTSTATUS_GHOST && playerGhost; };
	return host;
}();

const WorldEvents::Host worldHost = []() {
	WorldEvents::Host host{};
	host.ViewExists = []() { return true; };
	host.GetFirstSector = []() { return firstSector; };
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int startZ, int direction, TYPE_OBJECTID targetID,
		TYPE_SECTORPOSITION targetX, TYPE_SECTORPOSITION targetY, int targetZ, std::uint32_t delayFrame, MActionResult* pResult, bool bStartFromMainNode, int sX, int sY) {
		calls.push_back("skill|" + N(actionInfo) + "|" + N(startX) + "," + N(startY) + "," + N(startZ) + "|" + N(direction) + "|" + N(targetID)
			+ "|" + N(targetX) + "," + N(targetY) + "," + N(targetZ) + "|" + N(delayFrame) + "|" + (pResult != NULL ? "result" : "none")
			+ "|" + (bStartFromMainNode ? "main" : "first") + "|" + N(sX) + "," + N(sY));
	};
	host.PlaySoundAt = [](TYPE_SOUNDID soundID, bool repeat, int x, int y) { calls.push_back("sound at|" + N(soundID) + "|" + N(repeat) + "|" + N(x) + "," + N(y)); };
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

// The real clock, held still: the fakes time their patrols with it.
MonotonicClock::TimePoint clockNow = MonotonicClock::TimePoint() + MonotonicClock::Millis(1000000);
MonotonicClock::TimePoint ReadClock() { return clockNow; }

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
	MonotonicClock::ScopedTestSource clock{ReadClock};
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	MEventQueue* oldEvents = g_pEventManager;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zoneHost);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&playerHost);
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&worldHost);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creatureHost);
	const MEffectHost* oldEffectHost = MEffect::SetHost(&effectHost);
	const MAttachEffectHost* oldAttachHost = MAttachEffect::SetHost(&attachHost);

	Scene()
	{
		calls.clear();
		zoneExists = true;
		zoneWidth = zoneHeight = 60;
		blocked.clear();
		ownZoneCreatures.clear();
		playerZoneWidth = playerZoneHeight = 200;
		playerBlocked.clear();
		moveTypesAsked.clear();
		visited.clear();
		unvisits = 0;
		creatures.clear();
		fakeIDsTaken.clear();
		firstSector = { 10, 10 };
		zonesAsked.clear();
		player = nullptr;
		playerGhost = false;
		clockNow = MonotonicClock::TimePoint() + MonotonicClock::Millis(1000000);
		MItem::SetHost(&itemHost);
		creatureTypes.Init(800);
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
	}
	~Scene()
	{
		player = nullptr;
		MItem::SetHost(NULL);
		MAttachEffect::SetHost(oldAttachHost);
		MEffect::SetHost(oldEffectHost);
		CreatureState::SetHost(oldCreature);
		WorldEvents::SetHost(oldWorld);
		PlayerState::SetHost(oldPlayer);
		ZoneState::SetHost(oldZone);
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
	// A creature type of the tribe, with a count for each action and eight
	// moves a step.
	void Tribe(int type, CREATURETRIBE tribe = CREATURETRIBE_SLAYER)
	{
		testfw::MutableRow(creatureTypes, type).SetCreatureTribe(tribe, ACTION_MAX_SLAYER);
		testfw::MutableRow(creatureTypes, type).MoveTimes = 8;
		testfw::MutableRow(creatureTypes, type).MoveTimesMotor = 8;
		testfw::MutableRow(creatureTypes, type).MoveRatio = 1;
		for (int action = 0; action < ACTION_MAX_SLAYER; ++action)
			testfw::MutableRow(creatureTypes, type).SetActionCount(action, 10 + action);
	}
};

// A fake of the type at the cell, in its own zone.
void Place(MFakeCreature& fake, int type, int x, int y)
{
	fake.SetCreatureType(type);
	fake.SetZone(OwnZone());
	fake.SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
	fake.SetServerPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
}

// A creature of the type at the cell, in the zone.
void Put(MCreature& creature, TYPE_OBJECTID id, int type, int x, int y)
{
	creature.SetCreatureType(type);
	creature.SetID(id);
	creature.SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
	creatures[id] = &creature;
}

// A pet item that counts its deletions.
int petsDeleted = 0;
struct CountedPet : MPetItem
{
	~CountedPet() override { ++petsDeleted; }
};

} // namespace

//----------------------------------------------------------------------
// Who it is
//----------------------------------------------------------------------
TEST(FakeCreature, ItTakesTheNextIDNoFakeHas)
{
	Scene scene;
	const DWORD first = MFakeCreature::GetFakeID();
	fakeIDsTaken = {TYPE_OBJECTID((first + 1) % 10000), TYPE_OBJECTID((first + 2) % 10000)};
	CHECK_EQ(int((first + 3) % 10000), int(MFakeCreature::GetFakeID()));
	// After 9999 comes 0.
	fakeIDsTaken.clear();
	DWORD id = MFakeCreature::GetFakeID();
	int guard = 0;
	while (id != 9999 && guard++ < 10001) id = MFakeCreature::GetFakeID();
	CHECK_EQ(9999, int(id));
	CHECK_EQ(0, int(MFakeCreature::GetFakeID()));
	// A new fake takes one.
	MFakeCreature fake;
	CHECK_EQ(1, int(fake.GetID()));
}

TEST(FakeCreature, AnotherPlayersPetItemGoesWithIt)
{
	Scene scene;
	scene.Tribe(5);
	MCreature me;
	Put(me, 77, 5, 1, 1);
	player = &me;
	petsDeleted = 0;
	{
		MFakeCreature pet;
		pet.SetOwnerID(88);
		pet.SetPetItem(new CountedPet);
	}
	CHECK_EQ(1, petsDeleted);
	// The player's own pet item is the inventory's: it stays.
	CountedPet mine;
	{
		MFakeCreature pet;
		pet.SetOwnerID(77);
		pet.SetPetItem(&mine);
	}
	CHECK_EQ(1, petsDeleted);
	// Without a player it stays too.
	player = nullptr;
	CountedPet kept;
	{
		MFakeCreature pet;
		pet.SetOwnerID(88);
		pet.SetPetItem(&kept);
	}
	CHECK_EQ(1, petsDeleted);
}

TEST(FakeCreature, ItsKindSetsHowItIsDrawn)
{
	Scene scene;
	scene.Tribe(5);
	// (The fade flag is set first by SetCreatureType, as the game does.)
	for (MFakeCreature::FAKE_CREATURE_TYPE kind : {MFakeCreature::FAKE_CREATURE_FAST_MOVE_ACTION, MFakeCreature::FAKE_CREATURE_FADE_ACTION,
		MFakeCreature::FAKE_CREATURE_BRIGHTNESS, MFakeCreature::FAKE_CREATURE_TURNING_MOVE})
	{
		MFakeCreature fake;
		Place(fake, 5, 20, 20);
		CHECK(!fake.IsFade());
		fake.SetFakeCreatureType(kind);
		CHECK(fake.IsFade());
		CHECK_EQ(int(kind), int(fake.GetFakeCreatureType()));
	}
	MFakeCreature toPortal;
	Place(toPortal, 5, 20, 20);
	toPortal.SetFakeCreatureType(MFakeCreature::FAKE_CREATURE_TO_PORTAL);
	CHECK(toPortal.IsCutHeight());
	CHECK(!toPortal.IsFade());
	MFakeCreature rocket;
	Place(rocket, 5, 20, 20);
	rocket.SetFakeCreatureType(MFakeCreature::FAKE_CREATURE_ROCKET);
	CHECK(!rocket.IsFade());
	CHECK(!rocket.IsCutHeight());
}

//----------------------------------------------------------------------
// When it is done
//----------------------------------------------------------------------
TEST(FakeCreature, AFadeEndsWithItsActionAndAPortalWithItsCut)
{
	Scene scene;
	scene.Tribe(5);
	MFakeCreature fade;
	Place(fade, 5, 20, 20);
	fade.SetFakeCreatureType(MFakeCreature::FAKE_CREATURE_FADE_ACTION);
	fade.SetAction(ACTION_ATTACK);
	CHECK(!fade.IsFakeEnd());
	fade.SetAction(ACTION_STAND);
	CHECK(fade.IsFakeEnd());
	MFakeCreature portal;
	Place(portal, 5, 20, 20);
	portal.SetFakeCreatureType(MFakeCreature::FAKE_CREATURE_TO_PORTAL);
	CHECK(!portal.IsFakeEnd());
	for (int frame = 0; frame < 40 && !portal.IsFakeEnd(); ++frame)
		portal.UpdateCutHeight();
	CHECK(portal.IsFakeEnd());
	// A plain fake never ends by itself.
	MFakeCreature plain;
	Place(plain, 5, 20, 20);
	CHECK(!plain.IsFakeEnd());
}

TEST(FakeCreature, ARocketEndsWhenItsTargetGoesOrItsTimeIsUp)
{
	Scene scene;
	scene.Tribe(5);
	MCreature target;
	Put(target, 501, 5, 30, 31);
	target.SetCurrentDirection(2);
	MFakeCreature rocket;
	Place(rocket, 5, 20, 20);
	rocket.SetFakeCreatureType(MFakeCreature::FAKE_CREATURE_ROCKET);
	rocket.SetTraceID(501);
	rocket.SetTime(clockNow + MonotonicClock::Millis(1));
	CHECK(!rocket.IsFakeEnd());
	CHECK(Same({}));
	// Time up: it blows at its target.
	rocket.SetTime(clockNow - MonotonicClock::Millis(1));
	CHECK(rocket.IsFakeEnd());
	CHECK(Same({"skill|" + N(SKILL_CLIENT_PLASMA_ROCKET_LAUNCHER_BLOW) + "|30,31,0|2|0|30,31,0|10|none|first|0,0"}));
	// The target gone: it ends without a blow.
	calls.clear();
	creatures.erase(501);
	rocket.SetTime(clockNow + MonotonicClock::Millis(1));
	CHECK(rocket.IsFakeEnd());
	CHECK(Same({}));
}

TEST(FakeCreature, ADragonTornadoEndsOutOfThePlayersView)
{
	Scene scene;
	scene.Tribe(5);
	MCreature me;
	Put(me, 77, 5, 40, 40);
	player = &me;
	// The view around the player: from (40-8-1, 40-12-1) to 16+2 and 25+2
	// sectors on, both ends counted.
	const int x0 = 40 + g_SECTOR_SKIP_PLAYER_LEFT - 1, y0 = 40 + g_SECTOR_SKIP_PLAYER_UP - 1;
	const int x1 = x0 + g_SECTOR_WIDTH + 2, y1 = y0 + g_SECTOR_HEIGHT + 2;
	const std::pair<Cell, bool> cells[] = {
		{{x0, y0}, false}, {{x1, y1}, false}, {{40, 40}, false},
		{{x0 - 1, 40}, true}, {{x1 + 1, 40}, true}, {{40, y0 - 1}, true}, {{40, y1 + 1}, true},
	};
	for (const auto& cell : cells)
	{
		MFakeCreature tornado;
		Place(tornado, 5, cell.first.first, cell.first.second);
		tornado.SetFakeCreatureType(MFakeCreature::FAKE_CREATURE_DRAGON_TORNADO);
		CHECK_EQ(cell.second, tornado.IsFakeEnd());
	}
}

//----------------------------------------------------------------------
// The turning slide
//----------------------------------------------------------------------
TEST(FakeCreature, ATurningFakeSlidesToItsCellButNeverEnds)
{
	Scene scene;
	scene.Tribe(5);
	MFakeCreature fake;
	Place(fake, 5, 20, 20);
	fake.SetFakeCreatureType(MFakeCreature::FAKE_CREATURE_TURNING_MOVE);
	fake.SetFakePosition(23, 21);
	CHECK_EQ(23, int(fake.GetFakeX()));
	CHECK_EQ(21, int(fake.GetFakeY()));
	CHECK(!fake.IsFakeEnd());
	const int direction = fake.GetServerDirection();
	fake.UpdateFake();
	// It turns a step each frame.
	CHECK_EQ((direction + 1) & 7, int(fake.GetServerDirection()));
	// Over twice its stand's frames (2 x 10), it crosses the three cells
	// right and one down, a tile at a time.
	for (int frame = 1; frame < 2 * 10 + 2; ++frame)
		fake.UpdateFake();
	CHECK_EQ(23, int(fake.GetX()));
	CHECK_EQ(21, int(fake.GetY()));
	// Recorded: nothing in the game makes a turning fake (upstream's test
	// kind). Its end asks for its action's count at the end, which Action
	// resets in the same frame, so after any frame it has not ended.
	fake.SetAction(ACTION_STAND);
	bool ended = false;
	for (int frame = 0; frame < 40 && !ended; ++frame)
	{
		fake.Action();
		ended = fake.IsFakeEnd();
	}
	CHECK(!ended);
	CHECK_EQ(23, int(fake.GetX()));
}

//----------------------------------------------------------------------
// Where it goes next
//----------------------------------------------------------------------
TEST(FakeCreature, ItsNextDestinationStepsBackFromABlockedCell)
{
	Scene scene;
	scene.Tribe(5);
	MFakeCreature fake;
	Place(fake, 5, 20, 20);
	POINT next;
	// A free cell is taken as it is, asked of its own zone.
	CHECK(fake.SetNextDestination(25, 22));
	fake.GetNextDestination(next);
	CHECK_EQ(25, int(next.x));
	CHECK_EQ(22, int(next.y));
	CHECK(!zonesAsked.empty() && zonesAsked.back() == OwnZone());
	// A blocked one: the nearest free cell back towards it.
	blocked = {{25, 25}, {24, 24}};
	CHECK(fake.SetNextDestination(25, 25));
	fake.GetNextDestination(next);
	CHECK_EQ(23, int(next.x));
	CHECK_EQ(23, int(next.y));
	// Blocked all the way back: none.
	blocked = {{21, 20}, {22, 20}, {23, 20}};
	CHECK(!fake.SetNextDestination(23, 20));
	// Its own cell, nowhere, or a cell it found blocked: none.
	blocked.clear();
	CHECK(!fake.SetNextDestination(20, 20));
	CHECK(!fake.SetNextDestination(SECTORPOSITION_NULL, 20));
	// It asks as what it is: a fake nothing blocks.
	CHECK_EQ(int(MCreature::CREATURE_FAKE_NO_BLOCK), int(fake.GetMoveType()));
	CHECK(AskedAs(MCreature::CREATURE_FAKE_NO_BLOCK));
}

TEST(FakeCreature, AnOffZoneDestinationIsNotWalkedBack)
{
	Scene scene;
	scene.Tribe(5);
	MFakeCreature fake;
	Place(fake, 5, 20, 20);
	// The cell is off its own zone (narrower than the cell): the walk back
	// stops at once.
	zoneWidth = 30;
	blocked = {{35, 20}};
	CHECK(!fake.SetNextDestination(35, 20));
}

TEST(FakeCreature, APetStandsNearItsOwnerOnAFreeCell)
{
	Scene scene;
	// Without a zone: the owner's own cell.
	zoneExists = false;
	POINT p = MFakeCreature::CaculatePetPosition(20, 20, 2, 1, MCreature::CREATURE_GROUND, 25, 25);
	CHECK_EQ(20, int(p.x));
	CHECK_EQ(20, int(p.y));
	zoneExists = true;
	// The cells within two of the owner (18..21), off its row and column,
	// at least the minimum from the pet, free in the player's zone (the
	// owner's): block all but one.
	for (int y = 18; y < 22; ++y)
		for (int x = 18; x < 22; ++x)
			playerBlocked.insert({x, y});
	playerBlocked.erase({21, 18});
	p = MFakeCreature::CaculatePetPosition(20, 20, 2, 1, MCreature::CREATURE_FLYING, 25, 25);
	CHECK_EQ(21, int(p.x));
	CHECK_EQ(18, int(p.y));
	// Asked as the move type given.
	CHECK(AskedAs(MCreature::CREATURE_FLYING));
	moveTypesAsked.clear();
	// Too near the pet (nearer than the minimum): not that one.
	p = MFakeCreature::CaculatePetPosition(20, 20, 2, 3, MCreature::CREATURE_GROUND, 21, 19);
	CHECK_EQ(20, int(p.x));
	CHECK_EQ(20, int(p.y));
	// On the owner's row or column: never.
	playerBlocked.clear();
	for (int y = 18; y < 22; ++y)
		for (int x = 18; x < 22; ++x)
			if (x != 20 && y != 20) playerBlocked.insert({x, y});
	p = MFakeCreature::CaculatePetPosition(20, 20, 2, 1, MCreature::CREATURE_GROUND, 25, 25);
	CHECK_EQ(20, int(p.x));
	CHECK_EQ(20, int(p.y));
	CHECK(AskedAs(MCreature::CREATURE_GROUND));
}

//----------------------------------------------------------------------
// The turret
//----------------------------------------------------------------------
TEST(FakeCreature, ATurretTurnsToItsOwnersTarget)
{
	Scene scene;
	scene.Tribe(5);
	scene.Tribe(703);
	MCreature owner, target;
	Put(owner, 600, 5, 10, 10);
	Put(target, 601, 5, 26, 20);
	owner.SetStatus(MODIFY_LAST_TARGET, 601);
	MFakeCreature turret;
	Place(turret, 703, 20, 20);
	turret.SetOwnerID(600);
	turret.SetCurrentDirection(0);
	turret.SyncTurretDirection();
	const int wanted = ((SelectFacingDirection(20, 20, 26, 20) + 6) % 8) * 4;
	int turret0 = turret.GetTurretDirection();
	// One step of 32 a frame, the short way round, until it faces it.
	for (int frame = 0; frame < 32; ++frame)
	{
		turret.UpdateTurret();
		const int now = turret.GetTurretDirection();
		CHECK(now == wanted || (now - turret0 + 32) % 32 == 1 || (turret0 - now + 32) % 32 == 1);
		turret0 = now;
	}
	CHECK_EQ(wanted, int(turret.GetTurretDirection()));
	// No target: the final direction it was given, else its own facing.
	// From 8 to 2 is six steps back, not 26 on.
	owner.SetStatus(MODIFY_LAST_TARGET, 999);
	CHECK_EQ(8, wanted);
	turret.SetTurretFinalDirection(2);
	for (int frame = 0; frame < 6; ++frame)
		turret.UpdateTurret();
	CHECK_EQ(2, int(turret.GetTurretDirection()));
	for (int frame = 0; frame < 32; ++frame)
		turret.UpdateTurret();
	CHECK_EQ(2, int(turret.GetTurretDirection()));
	turret.SetTurretFinalDirection(0xFF);
	turret.SetCurrentDirection(4);
	for (int frame = 0; frame < 32; ++frame)
		turret.UpdateTurret();
	CHECK_EQ(((4 + 6) % 8) * 4, int(turret.GetTurretDirection()));
	// Not a turret: nothing turns.
	MFakeCreature other;
	Place(other, 5, 20, 20);
	other.SyncTurretDirection();
	const int before = other.GetTurretDirection();
	other.SetCurrentDirection(1);
	other.UpdateTurret();
	CHECK_EQ(before, int(other.GetTurretDirection()));
}

//----------------------------------------------------------------------
// The path search
//----------------------------------------------------------------------
namespace {
// A fake whose planned steps a test can read.
struct Walker : MFakeCreature
{
	using MFakeCreature::m_listDirection;
	using MFakeCreature::m_bTraceFlag;
	using MFakeCreature::m_bFastMove;
	using MFakeCreature::m_nUsedActionInfo;
	using MFakeCreature::m_nNextUsedActionInfo;
	using MFakeCreature::m_ActionCount;
	using MFakeCreature::m_ActionCountMax;
};
// Where the planned steps lead from (x, y), or {-1, -1} through a blocked
// cell.
Cell Follow(const Walker& walker, int x, int y)
{
	for (BYTE direction : walker.m_listDirection)
	{
		const POINT step = WorldTileGeometry::DirectionOffset(direction);
		x += step.x;
		y += step.y;
		if (!Free(x, y)) return {-1, -1};
	}
	return {x, y};
}
}

TEST(FakeCreature, AFakeFindsAWayToItsDestination)
{
	Scene scene;
	scene.Tribe(5);
	Walker walker;
	Place(walker, 5, 20, 20);
	CHECK(walker.SetDestination(24, 20));
	CHECK(Follow(walker, 20, 20) == Cell(24, 20));
	CHECK_EQ(4, int(walker.m_listDirection.size()));
	POINT dest;
	walker.GetDestination(dest);
	CHECK_EQ(24, int(dest.x));
	CHECK_EQ(20, int(dest.y));
	// Its search clears the visited flags of the view (from the sector
	// before the first to five past its end, 22 by 31 here) first.
	CHECK_EQ((10 + 16 + 5 - 9) * (10 + 25 + 5 - 9), unvisits);
	// The size and the cells are its own zone's, asked as a fake.
	for (MZone* pZone : zonesAsked) CHECK(pZone == OwnZone());
	CHECK(AskedAs(MCreature::CREATURE_FAKE_NO_BLOCK));
	// The view box stops at the player's zone's edge (25 by 30 here), not
	// its own.
	playerZoneWidth = 25;
	playerZoneHeight = 30;
	unvisits = 0;
	Walker clipped;
	Place(clipped, 5, 20, 20);
	CHECK(clipped.SetDestination(24, 20));
	CHECK_EQ((25 - 9) * (30 - 9), unvisits);
	playerZoneWidth = playerZoneHeight = 200;
	// Around a wall.
	for (int y = 12; y <= 30; ++y) blocked.insert({22, y});
	Walker around;
	Place(around, 5, 20, 20);
	CHECK(around.SetDestination(24, 20));
	CHECK(Follow(around, 20, 20) == Cell(24, 20));
	CHECK(around.m_listDirection.size() > 4);
}

TEST(FakeCreature, AnUnreachableDestinationIsRemembered)
{
	Scene scene;
	scene.Tribe(5);
	// The cell walled in on every side.
	for (int y = 19; y <= 21; ++y)
		for (int x = 29; x <= 31; ++x)
			if (x != 30 || y != 20) blocked.insert({x, y});
	Walker walker;
	Place(walker, 5, 20, 20);
	walker.SetDestination(30, 20);
	CHECK(walker.m_listDirection.empty());
	POINT dest;
	walker.GetDestination(dest);
	CHECK_EQ(int(SECTORPOSITION_NULL), int(dest.x));
	// It asks no more for that cell.
	CHECK(!walker.SetNextDestination(30, 20));
	// The search stays in the view: a cell reachable only round its edge
	// is not found.
	blocked.clear();
	for (int y = 0; y < 60; ++y)
		if (y != 50) blocked.insert({25, y});
	Walker boxed;
	Place(boxed, 5, 20, 20);
	boxed.SetDestination(30, 20);
	CHECK(boxed.m_listDirection.empty());
}

TEST(FakeCreature, ABlockedOrOffZoneOrSleepingFakeDoesNotSearch)
{
	Scene scene;
	scene.Tribe(5);
	Walker walker;
	Place(walker, 5, 20, 20);
	blocked = {{24, 20}};
	CHECK(!walker.SetDestination(24, 20));
	CHECK_EQ(0, unvisits);
	blocked.clear();
	CHECK(!walker.SetDestination(70, 20));
	CHECK(!walker.SetDestination(20, 70));
	// Even one that traces (and so would search for a blocked cell) does
	// not search off its zone.
	walker.m_bTraceFlag = true;
	CHECK(!walker.SetDestination(70, 20));
	CHECK(!walker.SetDestination(20, 70));
	CHECK_EQ(0, unvisits);
	POINT dest;
	walker.GetDestination(dest);
	CHECK_EQ(int(SECTORPOSITION_NULL), int(dest.x));
	walker.m_bTraceFlag = false;
	// Asleep: no, unless the player is a ghost.
	walker.AddEffectStatus(EFFECTSTATUS_SLEEP, 0xFFFF);
	CHECK(!walker.SetDestination(24, 20));
	playerGhost = true;
	CHECK(walker.SetDestination(24, 20));
	CHECK(!walker.m_listDirection.empty());
	// Dead: no.
	Walker dead;
	Place(dead, 5, 20, 20);
	dead.SetDead();
	CHECK(!dead.SetDestination(24, 20));
}

//----------------------------------------------------------------------
// The wanderer, the rocket, the wild wolf
//----------------------------------------------------------------------
TEST(FakeCreature, AGhostWandersInThePlayersView)
{
	Scene scene;
	scene.Tribe(5);
	scene.Tribe(CREATURETYPE_GHOST);
	MCreature me;
	Put(me, 77, 5, 40, 40);
	player = &me;
	std::srand(1);
	// In view, heading for a blocked cell: it turns three to five steps.
	blocked = {{41, 40}};
	moveTypesAsked.clear();
	std::set<int> turns;
	for (int ghosts = 0; ghosts < 100; ++ghosts)
	{
		MFakeCreature ghost;
		Place(ghost, CREATURETYPE_GHOST, 40, 40);
		ghost.SetDirection(DIRECTION_RIGHT);
		ghost.KeepWanderCreature();
		const int turn = (ghost.GetServerDirection() - DIRECTION_RIGHT + 8) % 8;
		CHECK(turn >= 3 && turn <= 5);
		turns.insert(turn);
		CHECK_EQ(ACTION_MOVE, int(ghost.GetAction()));
	}
	CHECK_EQ(3, int(turns.size()));
	// Asked of its own zone as a fake.
	CHECK(AskedAs(MCreature::CREATURE_FAKE_NO_BLOCK));
	blocked.clear();
	// Out of view while the player walks: it stands.
	MFakeCreature outOfView;
	Place(outOfView, CREATURETYPE_GHOST, 40 + 10, 40);
	me.SetAction(ACTION_MOVE);
	outOfView.SetAction(ACTION_MOVE);
	outOfView.KeepWanderCreature();
	CHECK_EQ(ACTION_STAND, int(outOfView.GetAction()));
	// Out of view, the player standing: from just past a side it walks
	// back in, by that side's way.
	me.SetAction(ACTION_STAND);
	const int x0 = 40 + g_SECTOR_SKIP_PLAYER_LEFT - 1, y0 = 40 + g_SECTOR_SKIP_PLAYER_UP - 1;
	const int x1 = x0 + g_SECTOR_WIDTH + 2, y1 = y0 + g_SECTOR_HEIGHT + 2;
	const std::pair<Cell, std::pair<int, int>> sides[] = {
		{{x0 - 1, 40}, {3, 5}}, {{x1 + 1, 40}, {7, 9}}, {{40, y0 - 1}, {1, 3}}, {{40, y1 + 1}, {5, 7}},
	};
	for (const auto& side : sides)
	{
		// One of three ways in, at random: all three, and no other.
		std::set<int> ways;
		for (int wanderers = 0; wanderers < 100; ++wanderers)
		{
			MFakeCreature wanderer;
			Place(wanderer, CREATURETYPE_GHOST, side.first.first, side.first.second);
			wanderer.KeepWanderCreature();
			CHECK_EQ(ACTION_MOVE, int(wanderer.GetAction()));
			int direction = wanderer.GetServerDirection();
			if (direction < side.second.first) direction += 8;
			CHECK(direction >= side.second.first && direction <= side.second.second);
			ways.insert(direction);
		}
		CHECK_EQ(3, int(ways.size()));
	}
	// Too far from the player (more than 10 across or 15 down): it stands.
	CHECK_EQ(10, x1 + 1 - 40);
	CHECK_EQ(15, y1 + 1 - 40);
	for (const Cell cell : {Cell(x1 + 2, 40), Cell(40, y1 + 2)})
	{
		MFakeCreature lost;
		Place(lost, CREATURETYPE_GHOST, cell.first, cell.second);
		lost.SetAction(ACTION_MOVE);
		lost.KeepWanderCreature();
		CHECK_EQ(ACTION_STAND, int(lost.GetAction()));
	}
	// At 1024x768 (Client.cpp's sizes) the view's sides are farther than
	// that, 12 or 13 across and 18 down: a ghost there stands.
	const LONG width = g_SECTOR_WIDTH, height = g_SECTOR_HEIGHT, left = g_SECTOR_SKIP_PLAYER_LEFT, up = g_SECTOR_SKIP_PLAYER_UP;
	g_SECTOR_WIDTH = 21;
	g_SECTOR_HEIGHT = 32;
	g_SECTOR_SKIP_PLAYER_LEFT = -10;
	g_SECTOR_SKIP_PLAYER_UP = -16;
	for (const Cell cell : {Cell(40 - 10 - 2, 40), Cell(40, 40 - 16 - 2)})
	{
		MFakeCreature wide;
		Place(wide, CREATURETYPE_GHOST, cell.first, cell.second);
		wide.SetAction(ACTION_MOVE);
		wide.KeepWanderCreature();
		CHECK_EQ(ACTION_STAND, int(wide.GetAction()));
	}
	g_SECTOR_WIDTH = width;
	g_SECTOR_HEIGHT = height;
	g_SECTOR_SKIP_PLAYER_LEFT = left;
	g_SECTOR_SKIP_PLAYER_UP = up;
	// Not a ghost: it does nothing.
	MFakeCreature plain;
	Place(plain, 5, x0 - 1, 40);
	plain.KeepWanderCreature();
	CHECK_EQ(ACTION_STAND, int(plain.GetAction()));
}

TEST(FakeCreature, ARocketHeadsForItsTargetThenCirclesIt)
{
	Scene scene;
	scene.Tribe(5);
	scene.Tribe(CREATURETYPE_ROCKET_LUNCHER);
	MCreature target;
	Put(target, 501, 5, 30, 20);
	MFakeCreature rocket;
	Place(rocket, CREATURETYPE_ROCKET_LUNCHER, 20, 20);
	rocket.SetTraceID(501);
	// Far: it faces the target and makes for its cell, counting its circle
	// from the way it faces.
	rocket.KeepRocketLuncher();
	const BYTE toTarget = SelectFacingDirection(20, 20, 30, 20);
	CHECK_EQ(int(toTarget), int(rocket.GetServerDirection()));
	CHECK_EQ(int(toTarget), int(rocket.GetDirection()));
	POINT dest;
	rocket.GetDestination(dest);
	CHECK_EQ(30, int(dest.x));
	CHECK_EQ(20, int(dest.y));
	CHECK_EQ(ACTION_MOVE, int(rocket.GetAction()));
	// Next to it, at its mark: on to the next cell round the target, one
	// step of the circle on.
	rocket.SetPosition(31, 20);
	rocket.SetTargetTile(31, 20);
	rocket.KeepRocketLuncher();
	const POINT round = WorldTileGeometry::DirectionOffset((toTarget + 1) % 8);
	rocket.GetDestination(dest);
	CHECK_EQ(30 + int(round.x), int(dest.x));
	CHECK_EQ(20 + int(round.y), int(dest.y));
	CHECK_EQ(int(SelectFacingDirection(31, 20, dest.x, dest.y)), int(rocket.GetDirection()));
	// Next to it but not yet at its mark: it keeps its mark.
	rocket.SetTargetTile(29, 21);
	rocket.KeepRocketLuncher();
	rocket.GetDestination(dest);
	CHECK_EQ(29, int(dest.x));
	CHECK_EQ(21, int(dest.y));
	// A fake that is not a rocket: nothing.
	MFakeCreature plain;
	Place(plain, 5, 20, 20);
	plain.SetTraceID(501);
	plain.SetCurrentDirection(1);
	plain.KeepRocketLuncher();
	CHECK_EQ(1, int(plain.GetDirection()));
	CHECK_EQ(ACTION_STAND, int(plain.GetAction()));
}

namespace {
// Where the wolf is to go now.
Cell WolfDestination(MFakeCreature& wolf)
{
	POINT dest;
	wolf.GetDestination(dest);
	return {int(dest.x), int(dest.y)};
}
Cell Past(int x, int y, int towardX, int towardY)
{
	const POINT tile = GetNextTileByDirection(towardX, towardY, SelectFacingDirection(x, y, towardX, towardY));
	return {int(tile.x), int(tile.y)};
}
}

TEST(FakeCreature, AWildWolfLeapsPastItsPreyThreeTimes)
{
	Scene scene;
	scene.Tribe(5);
	scene.Tribe(CREATURETYPE_WILD_WOLF);
	MCreature prey;
	Put(prey, 501, 5, 30, 20);
	MFakeCreature wolf;
	Place(wolf, CREATURETYPE_WILD_WOLF, 20, 20);
	wolf.SetFakeCreatureType(MFakeCreature::FAKE_CREATURE_WILD_WOLF);
	wolf.SetTraceID(501);
	// It waits until its count (1 to 5 at random when made) reaches 4,
	// then leaps to the cell past the prey.
	int frames = 0;
	while (!wolf.IsFastMove() && frames < 10)
	{
		wolf.KeepWildWolfAttack();
		++frames;
	}
	CHECK(wolf.IsFastMove());
	CHECK(frames >= 1 && frames <= 4);
	const BYTE leap = SelectFacingDirection(20, 20, 30, 20);
	CHECK_EQ(int(leap), int(wolf.GetDirection()));
	Cell at = Past(20, 20, 30, 20);
	CHECK(WolfDestination(wolf) == at);
	// Landed, it waits four frames, then steps aside: one way round from
	// facing the prey. Then it leaps past again, steps aside again, and
	// leaps a third time.
	for (int round = 0; round < 4; ++round)
	{
		wolf.SetPosition(TYPE_SECTORPOSITION(at.first), TYPE_SECTORPOSITION(at.second));
		for (int frame = 0; frame < 4; ++frame) wolf.KeepWildWolfAttack();
		CHECK(WolfDestination(wolf) == at);
		wolf.KeepWildWolfAttack();
		Cell next;
		if (round % 2 == 0)
		{
			const BYTE aside = BYTE((SelectFacingDirection(at.first, at.second, 30, 20) + 7) % 8);
			const POINT step = GetNextTileByDirection(at.first, at.second, aside);
			next = {int(step.x), int(step.y)};
			CHECK_EQ(int(aside), int(wolf.GetDirection()));
		}
		else
			next = Past(at.first, at.second, 30, 20);
		CHECK(WolfDestination(wolf) == next);
		CHECK(!wolf.IsFakeEnd());
		at = next;
	}
	// After the third it is done on its 31st frame.
	wolf.SetPosition(TYPE_SECTORPOSITION(at.first), TYPE_SECTORPOSITION(at.second));
	for (int frame = 0; frame < 30; ++frame) wolf.KeepWildWolfAttack();
	CHECK(WolfDestination(wolf) == at);
	CHECK(!wolf.IsFakeEnd());
	wolf.KeepWildWolfAttack();
	CHECK(wolf.IsFakeEnd());
}

TEST(FakeCreature, AWildWolfWhosePreyIsGoneIsDone)
{
	Scene scene;
	scene.Tribe(5);
	scene.Tribe(CREATURETYPE_WILD_WOLF);
	MCreature prey;
	Put(prey, 501, 5, 30, 20);
	MFakeCreature wolf;
	Place(wolf, CREATURETYPE_WILD_WOLF, 20, 20);
	wolf.SetFakeCreatureType(MFakeCreature::FAKE_CREATURE_WILD_WOLF);
	wolf.SetTraceID(501);
	int frames = 0;
	while (!wolf.IsFastMove() && frames++ < 10) wolf.KeepWildWolfAttack();
	CHECK(wolf.IsFastMove());
	// Its prey gone: it is done on the 31st frame after the leap.
	creatures.erase(501);
	for (int frame = 0; frame < 30; ++frame) wolf.KeepWildWolfAttack();
	CHECK(!wolf.IsFakeEnd());
	wolf.KeepWildWolfAttack();
	CHECK(wolf.IsFakeEnd());
	// Without a zone it waits.
	zoneExists = false;
	MFakeCreature idle;
	Place(idle, CREATURETYPE_WILD_WOLF, 20, 20);
	idle.SetFakeCreatureType(MFakeCreature::FAKE_CREATURE_WILD_WOLF);
	creatures[501] = &prey;
	idle.SetTraceID(501);
	for (int frame = 0; frame < 10; ++frame) idle.KeepWildWolfAttack();
	CHECK(!idle.IsFastMove());
}

TEST(FakeCreature, AWildWolfAtACorpseLeapsThenFeeds)
{
	Scene scene;
	scene.Tribe(CREATURETYPE_WILD_WOLF);
	MFakeCreature wolf;
	Place(wolf, CREATURETYPE_WILD_WOLF, 20, 20);
	wolf.SetFakeCreatureType(MFakeCreature::FAKE_CREATURE_WILD_WOLF);
	wolf.SetIsEatCorpse(true);
	wolf.SetTargetTile(30, 20);
	int frames = 0;
	while (!wolf.IsFastMove() && frames++ < 10) wolf.KeepWildWolfAttack();
	CHECK(wolf.IsFastMove());
	// Past the corpse's cell.
	const BYTE leap = SelectFacingDirection(20, 20, 30, 20);
	const POINT past = GetNextTileByDirection(30, 20, leap);
	POINT dest;
	wolf.GetDestination(dest);
	CHECK_EQ(int(past.x), int(dest.x));
	CHECK_EQ(int(past.y), int(dest.y));
	// Landed there, in its moving turn: it turns about and feeds, and is
	// done after.
	// It waits four frames after each leap.
	wolf.SetPosition(TYPE_SECTORPOSITION(past.x), TYPE_SECTORPOSITION(past.y));
	const int direction = wolf.GetDirection();
	for (int frame = 0; frame < 4; ++frame) wolf.KeepWildWolfAttack();
	CHECK(ACTION_VAMPIRE_DRAIN != wolf.GetAction());
	wolf.KeepWildWolfAttack();
	CHECK_EQ(ACTION_VAMPIRE_DRAIN, int(wolf.GetAction()));
	// (The way it is to face, turned to by its frames.)
	CHECK_EQ((direction + 4) % 8, int(wolf.GetServerDirection()));
	for (int frame = 0; frame < 40; ++frame) wolf.KeepWildWolfAttack();
	CHECK(wolf.IsFakeEnd());
}

TEST(FakeCreature, AFastMoveEndsWhenItHasLandedAndActed)
{
	Scene scene;
	scene.Tribe(5);
	MFakeCreature fake;
	Place(fake, 5, 20, 20);
	fake.SetFakeCreatureType(MFakeCreature::FAKE_CREATURE_FAST_MOVE_ACTION);
	CHECK(fake.IsFakeEnd());
	fake.SetFakeCreatureFastMoveAction(24, 20, ACTIONINFO_NULL, 501);
	CHECK(fake.IsFastMove());
	CHECK(!fake.IsFakeEnd());
	// Landed, it ends once no skill is in hand or next and its action is a
	// stand or done.
	Walker landed;
	Place(landed, 5, 24, 20);
	landed.SetFakeCreatureType(MFakeCreature::FAKE_CREATURE_FAST_MOVE_ACTION);
	landed.SetAction(ACTION_STAND);
	CHECK(landed.IsFakeEnd());
	landed.m_nUsedActionInfo = 5;
	CHECK(!landed.IsFakeEnd());
	landed.m_nUsedActionInfo = ACTIONINFO_NULL;
	landed.m_nNextUsedActionInfo = 5;
	CHECK(!landed.IsFakeEnd());
	landed.m_nNextUsedActionInfo = ACTIONINFO_NULL;
	landed.SetAction(ACTION_ATTACK);
	landed.m_ActionCount = 0;
	CHECK(!landed.IsFakeEnd());
	landed.m_ActionCount = landed.m_ActionCountMax;
	CHECK(landed.IsFakeEnd());
	landed.m_bFastMove = true;
	CHECK(!landed.IsFakeEnd());
}

//----------------------------------------------------------------------
// A pet that follows
//----------------------------------------------------------------------
TEST(FakeCreature, APetChasesItsMasterFarOffAndWalksUpClose)
{
	Scene scene;
	scene.Tribe(5);
	MCreature master;
	Put(master, 600, 5, 30, 20);
	// More than six away (seven): it rushes there.
	MFakeCreature rusher;
	Place(rusher, 5, 23, 20);
	rusher.SetTraceID(600);
	rusher.KeepTraceCreature();
	CHECK(rusher.IsFastMove());
	// Four to six away: it walks up.
	for (int x : {24, 26})
	{
		MFakeCreature walker;
		Place(walker, 5, x, 20);
		walker.SetTraceID(600);
		walker.KeepTraceCreature();
		CHECK(!walker.IsFastMove());
		POINT next;
		walker.GetNextDestination(next);
		CHECK_EQ(30, int(next.x));
		CHECK_EQ(20, int(next.y));
		CHECK_EQ(ACTION_MOVE, int(walker.GetAction()));
	}
	// Three away or nearer: it stays.
	MFakeCreature adjacent;
	Place(adjacent, 5, 27, 20);
	adjacent.SetTraceID(600);
	adjacent.KeepTraceCreature();
	CHECK(!adjacent.IsFastMove());
	CHECK_EQ(ACTION_STAND, int(adjacent.GetAction()));
	// Its master gone: it stops.
	creatures.clear();
	MFakeCreature alone;
	Place(alone, 5, 25, 20);
	alone.SetTraceID(600);
	alone.SetAction(ACTION_MOVE);
	alone.KeepTraceCreature();
	CHECK_EQ(ACTION_STAND, int(alone.GetAction()));
}

TEST(FakeCreature, ATankPetSmokesWhenItsPatrolRunsOut)
{
	Scene scene;
	scene.Tribe(5);
	scene.Tribe(702);
	MCreature master;
	Put(master, 600, 5, 22, 20);
	// A turret-shaped pet (702) counts its patrol down without moving; a
	// tank (pet item type 3) then smokes.
	MFakeCreature tank;
	Place(tank, 702, 20, 20);
	tank.SetTraceID(600);
	std::unique_ptr<MPetItem> item(static_cast<MPetItem*>(MItem::NewItem(ITEM_CLASS_PET_ITEM)));
	item->SetItemType(3);
	tank.SetPetItem(item.get());
	tank.SetOwnerID(OBJECTID_NULL);
	int smokes = 0;
	for (int round = 0; round < 12 && smokes == 0; ++round)
	{
		clockNow += MonotonicClock::Millis(20000);
		calls.clear();
		tank.SetAction(ACTION_STAND);
		tank.KeepTraceCreature();
		for (const std::string& call : calls)
			if (call == "skill|" + N(SKILL_CLIENT_TANK_SMOKE) + "|0,0,0|" + N(DIRECTION_DOWN) + "|" + N(tank.GetID()) + "|0,0,0|0|none|first|0,0") ++smokes;
	}
	CHECK_EQ(1, smokes);
	// Before its time it waits: its patrol (two to four) does not run
	// down, so it does not smoke again.
	calls.clear();
	for (int frame = 0; frame < 10; ++frame)
	{
		tank.SetAction(ACTION_STAND);
		tank.KeepTraceCreature();
	}
	CHECK(Same({}));
	// A drinking pet (types 1 and 2) drinks instead.
	MFakeCreature bat;
	Place(bat, 702, 20, 20);
	bat.SetTraceID(600);
	std::unique_ptr<MPetItem> batItem(static_cast<MPetItem*>(MItem::NewItem(ITEM_CLASS_PET_ITEM)));
	batItem->SetItemType(1);
	bat.SetPetItem(batItem.get());
	bool drank = false;
	for (int round = 0; round < 12 && !drank; ++round)
	{
		clockNow += MonotonicClock::Millis(20000);
		bat.SetAction(ACTION_STAND);
		bat.KeepTraceCreature();
		drank = bat.GetAction() == ACTION_VAMPIRE_DRAIN;
	}
	CHECK(drank);
	// The items are the test's.
	tank.SetPetItem(NULL);
	bat.SetPetItem(NULL);
}

TEST(FakeCreature, APetsStepSoundsWhereItLands)
{
	MCreature::InitMoveTable();
	Scene scene;
	scene.Tribe(5);
	testfw::MutableRow(*g_pCreatureTable, 5).SetActionSound(ACTION_MOVE, 88);
	MFakeCreature pet;
	Place(pet, 5, 20, 20);
	pet.SetOwnerID(600);
	CHECK(pet.SetNextDestination(24, 20));
	// Its step is its own zone's to allow, asked as a fake: the player's
	// zone blocking the cell does not stop it.
	playerBlocked = {{21, 20}};
	zonesAsked.clear();
	moveTypesAsked.clear();
	pet.ActionMove();
	CHECK_EQ(21, int(pet.GetX()));
	CHECK_EQ(20, int(pet.GetY()));
	CHECK(Same({"sound at|88|0|21,20"}));
	for (MZone* pZone : zonesAsked) CHECK(pZone == OwnZone());
	CHECK(AskedAs(MCreature::CREATURE_FAKE_NO_BLOCK));
	playerBlocked.clear();
	// A blocked next cell: it waits and plans again.
	MFakeCreature stuck;
	Place(stuck, 5, 20, 20);
	stuck.SetOwnerID(600);
	CHECK(stuck.SetNextDestination(24, 20));
	blocked = {{21, 20}, {21, 19}, {21, 21}, {20, 19}, {20, 21}, {19, 19}, {19, 20}, {19, 21}};
	calls.clear();
	stuck.ActionMove();
	CHECK_EQ(20, int(stuck.GetX()));
	CHECK(Same({}));
}

TEST(FakeCreature, AWalkingPetPatrolsNearItsMaster)
{
	Scene scene;
	scene.Tribe(5);
	MCreature master;
	Put(master, 600, 5, 30, 20);
	// Round the master, two cells are free in the player's zone: one next
	// to the pet (at 31,19), one two away from it.
	for (int y = 18; y < 22; ++y)
		for (int x = 28; x < 32; ++x)
			playerBlocked.insert({x, y});
	playerBlocked.erase({31, 18});
	playerBlocked.erase({29, 21});
	// A pet of level 30 or more keeps two away from where it stands; a
	// lower one takes either.
	for (int level : {30, 29})
	{
		std::set<Cell> spots;
		for (int pets = 0; pets < 40; ++pets)
		{
			MFakeCreature pet;
			Place(pet, 5, 31, 19);
			pet.SetTraceID(600);
			std::unique_ptr<MPetItem> item(static_cast<MPetItem*>(MItem::NewItem(ITEM_CLASS_PET_ITEM)));
			item->SetNumber(level);
			pet.SetPetItem(item.get());
			pet.SetAction(ACTION_STAND);
			clockNow += MonotonicClock::Millis(20000);
			moveTypesAsked.clear();
			pet.KeepTraceCreature();
			CHECK_EQ(ACTION_MOVE, int(pet.GetAction()));
			POINT next;
			pet.GetNextDestination(next);
			spots.insert({int(next.x), int(next.y)});
			CHECK(AskedAs(pet.GetMoveType()));
			pet.SetPetItem(NULL);
		}
		if (level == 30)
			CHECK(spots == std::set<Cell>({{29, 21}}));
		else
			CHECK(spots == std::set<Cell>({{29, 21}, {31, 18}}));
	}
}

TEST(FakeCreature, ADrinkingPetCutsItsDrinkShortToFollow)
{
	Scene scene;
	scene.Tribe(5);
	MCreature master;
	Put(master, 600, 5, 30, 20);
	Walker pet;
	Place(pet, 5, 25, 20);
	pet.SetTraceID(600);
	pet.SetAction(ACTION_VAMPIRE_DRAIN);
	CHECK_EQ(ACTION_VAMPIRE_DRAIN, int(pet.GetAction()));
	const int drainMax = pet.m_ActionCountMax;
	CHECK(drainMax > 13);
	// Early in its drink and five away: it skips to the drink's last 13
	// frames and walks after them.
	pet.m_ActionCount = 0;
	pet.KeepTraceCreature();
	CHECK_EQ(drainMax - 13, int(pet.m_ActionCount));
	CHECK_EQ(ACTION_VAMPIRE_DRAIN, int(pet.GetAction()));
	CHECK_EQ(ACTION_MOVE, int(pet.GetNextAction()));
	// Later than that, it keeps its place.
	pet.m_ActionCount = BYTE(drainMax - 5);
	pet.KeepTraceCreature();
	CHECK_EQ(drainMax - 5, int(pet.m_ActionCount));
}

TEST(FakeCreature, ATracingPetFollowsItsMasterInItsOwnZone)
{
	MCreature::InitMoveTable();
	Scene scene;
	scene.Tribe(5);
	MCreature master;
	Put(master, 600, 5, 24, 20);
	// The master in the pet's own zone only.
	creatures.clear();
	ownZoneCreatures[600] = &master;
	Walker pet;
	Place(pet, 5, 20, 20);
	pet.SetOwnerID(700);
	pet.SetTraceID(600);
	// Bound elsewhere (22,22), it turns to where its master stands.
	CHECK(pet.SetNextDestination(22, 22));
	pet.m_bTraceFlag = true;
	zonesAsked.clear();
	pet.ActionMove();
	CHECK_EQ(21, int(pet.GetX()));
	CHECK_EQ(20, int(pet.GetY()));
	CHECK(pet.m_bTraceFlag);
	CHECK(!zonesAsked.empty());
	for (MZone* pZone : zonesAsked) CHECK(pZone == OwnZone());
	// Gone from its own zone (the player's still has it): it stops tracing
	// and stands.
	ownZoneCreatures.clear();
	creatures[600] = &master;
	Walker lost;
	Place(lost, 5, 20, 20);
	lost.SetOwnerID(700);
	lost.SetTraceID(600);
	CHECK(lost.SetNextDestination(22, 22));
	lost.m_bTraceFlag = true;
	lost.ActionMove();
	CHECK_EQ(20, int(lost.GetX()));
	CHECK(!lost.m_bTraceFlag);
	CHECK_EQ(ACTION_STAND, int(lost.GetAction()));
}
