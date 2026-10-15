#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "PacketFunction.h"
#include "MPlayer.h"
#include "MZone.h"
#include "MSector.h"
#include "MCreature.h"
#include "MCreatureWear.h"
#include "MFakeCreature.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "MEffectGeneratorTable.h"
#include "MEffectTarget.h"
#include "MEffect.h"
#include "MAttachEffect.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "RankBonusTable.h"
#include "MEventManager.h"
#include "MParty.h"
#include "MEventQueue.h"
#include "FrameClock.h"
#include "MonotonicClock.h"
#include "SkillDef.h"
#include "EffectSpriteTypeDef.h"
#include "WorldTileGeometry.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"
#include "Gpackets/GCAddMonster.h"

#include <memory>
#include <string>
#include <vector>

// MItemUse.cpp's, reached through MItemHost.
MItem*	NewItemOfClass(ITEM_CLASS itemClass);

// PacketFunction.cpp's, which PacketFunction.h does not declare.
void	SkillShadowDancing(MCreature* pUserCreature, MCreature* pTargetCreature, int skillID);
void	Add_GDR_Effect(int nEffect, bool bAppearBossMonster);
void	Add_GDR_Potal_Effect(int nMapID);
void	Add_Race_SlayerMonster(GCAddMonster* pPacket);
void	Add_Race_OustersMonster(GCAddMonster* pPacket);
void	Add_RocketRuncher(MCreature* UserCreature, MCreature* TargetCreature);
void	Add_Wild_Wolf(MCreature* UserCreature, MCreature* TargetCreature, bool IsEatCorpse);
void	SetDragonTorando(int Type, DWORD ObjectID, int TileX, int TileY);

// The packet handlers' helpers that show a skill (PacketFunction.cpp,
// gameclient's since task 4.252): an action's effect started from its
// main node or at a cell, the copies of the user that cross counter,
// shadow dance, Illendue and Blaze Walk send at the target, Gilles de
// Rais's map effects, the monsters in a slayer's and an ouster's shape,
// the rocket, the wild wolf, the dragon tornado and an ouster's elemental.
// A real zone stands behind ZoneState as g_pZone, a real MPlayer as
// g_pPlayer, and a generator that records each effect it is asked to start
// stands in the effect generator table.
namespace {

// What the generator was asked to start: the action, from where to where
// and whom, facing where, the phase (its step: the phase's number plus
// one), and what the target carried on (its delay and its result's nodes;
// -1 for no result).
struct Started
{
	TYPE_ACTIONINFO action = 0;
	int x0 = 0, y0 = 0, z0 = 0, x1 = 0, y1 = 0, z1 = 0;
	TYPE_OBJECTID creatureID = 0;
	int direction = 0, step = 0, power = 0;
	DWORD delay = 0;
	int results = -1;
};
std::vector<Started> started;

const TYPE_EFFECTGENERATORID kGenerator = 7;

struct Recorder : MEffectGenerator
{
	TYPE_EFFECTGENERATORID GetID() override { return kGenerator; }
	bool Generate(const EFFECTGENERATOR_INFO& info) override
	{
		Started s;
		s.action = info.nActionInfo;
		s.x0 = info.x0; s.y0 = info.y0; s.z0 = info.z0;
		s.x1 = info.x1; s.y1 = info.y1; s.z1 = info.z1;
		s.creatureID = info.creatureID;
		s.direction = info.direction;
		s.step = info.step;
		s.power = info.power;
		// Every action here has three phases and starts at most from the
		// second, so the target always goes on to a next phase.
		if (info.pEffectTarget != NULL)
		{
			s.delay = info.pEffectTarget->GetDelayFrame();
			if (info.pEffectTarget->IsExistResult())
				s.results = info.pEffectTarget->GetResult()->GetSize();
		}
		started.push_back(s);
		return true;
	}
};

struct Generators : MEffectGeneratorTable
{
	void Add(MEffectGenerator* generator) { AddEffectGenerator(generator); }
};

// A result node that counts its runs and its deletions.
int executed = 0;
int destroyed = 0;
struct CountingNode : MActionResultNode
{
	~CountingNode() override { ++destroyed; }
	void Execute() override { ++executed; }
};
MActionResult* CountedResult()
{
	MActionResult* pResult = new MActionResult;
	pResult->Add(new CountingNode);
	return pResult;
}

// What a creature keeps to itself, read through member pointers so the
// creature need not be of the reading type.
struct CreatureAccess : MCreature
{
	using MCreature::m_TraceID;
	using MCreature::m_TraceX;
	using MCreature::m_TraceY;
	using MCreature::m_pActionResult;
	using MCreature::m_nNextUsedActionInfo;
	using MCreature::m_MoveAction;
	using MCreature::m_NextX;
	using MCreature::m_NextY;
};
struct FakeAccess : MFakeCreature
{
	using MFakeCreature::m_Delay;
	using MFakeCreature::m_nextMoveTime;
	using MFakeCreature::m_IsEatCorps;
	using MFakeCreature::m_DestX;
	using MFakeCreature::m_DestY;
};
TYPE_OBJECTID TraceID(const MCreature* p) { return p->*(&CreatureAccess::m_TraceID); }
MActionResult* ResultOf(const MCreature* p) { return p->*(&CreatureAccess::m_pActionResult); }

// The zone's own creatures report themselves through ZoneState, as
// GameInit's entries forward them to g_pZone.
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
const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.Creature = []() -> MCreature* { return g_pPlayer; };
	host.GetID = []() { return g_pPlayer != NULL ? g_pPlayer->GetID() : TYPE_OBJECTID(OBJECTID_NULL); };
	host.HasEffectStatus = [](EFFECTSTATUS status) { return g_pPlayer != NULL && g_pPlayer->HasEffectStatus(status); };
	return host;
}();
// The world's own entry is not PacketFunction's: an effect started through
// it shows here, so a test can tell the two apart.
std::vector<TYPE_ACTIONINFO> worldStarted;
const WorldEvents::Host worldHost = []() {
	WorldEvents::Host host{};
	host.ViewExists = []() { return true; };
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION, TYPE_SECTORPOSITION, int, int, TYPE_OBJECTID,
		TYPE_SECTORPOSITION, TYPE_SECTORPOSITION, int, std::uint32_t, MActionResult* pResult, bool, int, int) {
		worldStarted.push_back(actionInfo);
		delete pResult;
	};
	return host;
}();
const CreatureState::Host creatureHost = []() {
	CreatureState::Host host{};
	host.GetAddonItemClass = [](MCreature*, int, ITEM_CLASS* pItemClass) { *pItemClass = ITEM_CLASS_NULL; return false; };
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
const MItemHost itemHost = []() {
	MItemHost host{};
	host.NewItem = NewItemOfClass;
	return host;
}();

// The real clock, held still: the rocket's flight is timed with it.
MonotonicClock::TimePoint clockNow = MonotonicClock::TimePoint() + MonotonicClock::Millis(5000000);
MonotonicClock::TimePoint ReadClock() { return clockNow; }

const int Slayer = 5, Vampire = 6, Ousters = 7;

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
	MParty party;
	Generators generators;
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
	MEffectGeneratorTable* oldGenerators = g_pEffectGeneratorTable;
	MParty* oldParty = g_pParty;
	MZone* oldZone = g_pZone;
	MPlayer* oldPlayer = g_pPlayer;
	DWORD oldFrame = g_CurrentFrame;
	const ZoneState::Host* oldZoneHost = ZoneState::SetHost(&zoneHost);
	const PlayerState::Host* oldPlayerHost = PlayerState::SetHost(&playerHost);
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&worldHost);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creatureHost);
	const MEffectHost* oldEffectHost = MEffect::SetHost(&effectHost);
	const MAttachEffectHost* oldAttachHost = MAttachEffect::SetHost(&attachHost);
	// The zone last but the player: it frees what it holds before the
	// tables go.
	std::unique_ptr<MZone> zone;
	MPlayer* me = nullptr;

	explicit Scene(int width = 40, int height = 30)
	{
		started.clear();
		worldStarted.clear();
		executed = destroyed = 0;
		clockNow = MonotonicClock::TimePoint() + MonotonicClock::Millis(5000000);
		g_CurrentFrame = 1000;
		MItem::SetHost(&itemHost);
		creatureTypes.Init(1000);
		g_pCreatureTable = &creatureTypes;
		Tribe(Slayer);
		Tribe(Vampire, CREATURETRIBE_VAMPIRE);
		Tribe(Ousters, CREATURETRIBE_OUSTERS);
		sprites.Init(4);
		g_pCreatureSpriteTable = &sprites;
		actions.Init(MAX_ACTIONINFO);
		// Every row stands still without phases unless a test gives it
		// some (the data file sets every row's sound; so does this).
		for (int info = 0; info < MAX_ACTIONINFO; ++info)
		{
			actions.GetMutable(info)->Set("", ACTION_STAND, EFFECTSPRITETYPE_NULL, 0, 0);
			actions.GetMutable(info)->SetSoundID(SOUNDID_NULL);
		}
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
		g_pParty = &party;
		generators.Add(new Recorder);
		g_pEffectGeneratorTable = &generators;
		zone.reset(new MZone(TYPE_SECTORPOSITION(width), TYPE_SECTORPOSITION(height)));
		g_pZone = zone.get();
		// The player stands at (20, 15) facing down-right; the zone does
		// not hold it.
		me = new MPlayer;
		g_pPlayer = me;
		me->SetZone(g_pZone);
		me->SetCreatureType(Slayer);
		me->SetID(77);
		me->SetPosition(20, 15);
		me->SetServerPosition(20, 15);
		me->SetDirection(DIRECTION_RIGHTDOWN);
		me->SetCurrentDirection(DIRECTION_RIGHTDOWN);
	}
	~Scene()
	{
		delete me;
		g_pPlayer = oldPlayer;
		zone.reset();
		g_pZone = oldZone;
		MItem::SetHost(NULL);
		MAttachEffect::SetHost(oldAttachHost);
		MEffect::SetHost(oldEffectHost);
		CreatureState::SetHost(oldCreature);
		WorldEvents::SetHost(oldWorld);
		PlayerState::SetHost(oldPlayerHost);
		ZoneState::SetHost(oldZoneHost);
		g_pParty = oldParty;
		g_pEffectGeneratorTable = oldGenerators;
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
		const int actionCount = tribe == CREATURETRIBE_VAMPIRE ? ACTION_MAX_VAMPIRE : (tribe == CREATURETRIBE_OUSTERS ? ACTION_MAX_OUSTERS : ACTION_MAX_SLAYER);
		testfw::MutableRow(creatureTypes, type).SetCreatureTribe(tribe, actionCount);
		testfw::MutableRow(creatureTypes, type).MoveTimes = 8;
		testfw::MutableRow(creatureTypes, type).MoveTimesMotor = 8;
		testfw::MutableRow(creatureTypes, type).MoveRatio = 1;
		for (int action = 0; action < actionCount; ++action)
			testfw::MutableRow(creatureTypes, type).SetActionCount(action, 10 + action);
	}
	// An action of three phases by the recording generator, the phase's
	// step its number plus one, starting as `start` says.
	void Show(TYPE_ACTIONINFO action, BYTE start = FLAG_ACTIONINFO_START_USER, int value = 0, int mainNode = MAIN_NODE_NULL, BYTE target = 0)
	{
		MActionInfo* info = actions.GetMutable(action);
		info->Set("", ACTION_MAGIC, EFFECTSPRITETYPE_NULL, 0, target, start, 0, value);
		info->Init(3);
		for (int phase = 0; phase < 3; ++phase)
		{
			info->GetMutable(phase)->EffectGeneratorID = kGenerator;
			info->GetMutable(phase)->Step = WORD(phase + 1);
			info->GetMutable(phase)->Count = 5;
		}
		info->SetMainNode(mainNode);
	}
	// A creature of the type in the zone at the cell.
	MCreature* Put(TYPE_OBJECTID id, int x, int y, int type = Slayer)
	{
		MCreature* pCreature = new MCreatureWear;
		pCreature->SetZone(g_pZone);
		pCreature->SetCreatureType(TYPE_CREATURETYPE(type));
		pCreature->SetID(id);
		pCreature->SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
		pCreature->SetServerPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
		CHECK(g_pZone->AddCreature(pCreature));
		return pCreature;
	}
	// The zone's fake creatures, oldest first (their IDs ascend).
	std::vector<MFakeCreature*> Fakes()
	{
		std::vector<MFakeCreature*> fakes;
		auto it = zone->GetFakeCreatureBegin();
		for (int i = 0; i < zone->GetFakeCreatureNumber(); ++i, ++it)
			fakes.push_back(static_cast<MFakeCreature*>(it->second));
		return fakes;
	}
};

int PixelX(int x) { return WorldTileGeometry::TileToPixelX(x); }
int PixelY(int y) { return WorldTileGeometry::TileToPixelY(y); }

// A cell as "x,y".
std::string Cell(int x, int y) { return std::to_string(x) + "," + std::to_string(y); }

// Where each fake is headed (the next cell of its fast move; a fake keeps
// no server cell), in order.
std::vector<std::string> Destinations(const std::vector<MFakeCreature*>& fakes)
{
	std::vector<std::string> cells;
	for (const MFakeCreature* pFake : fakes) cells.push_back(Cell(pFake->*(&CreatureAccess::m_NextX), pFake->*(&CreatureAccess::m_NextY)));
	return cells;
}

} // namespace

//----------------------------------------------------------------------
// ExecuteActionInfoFromMainNode
//----------------------------------------------------------------------
TEST(PacketSkillShows, NoActionOrOnePastTheTableFreesTheResultUnrun)
{
	Scene scene;
	ExecuteActionInfoFromMainNode(ACTIONINFO_NULL, 1, 2, 0, 0, 5, 3, 4, 0, 0, CountedResult());
	CHECK_EQ(1, destroyed);
	ExecuteActionInfoFromMainNode(TYPE_ACTIONINFO(scene.actions.GetSize()), 1, 2, 0, 0, 5, 3, 4, 0, 0, CountedResult());
	CHECK_EQ(2, destroyed);
	// Without a table at all as well.
	g_pActionInfoTable = NULL;
	ExecuteActionInfoFromMainNode(SKILL_ATTACK_MELEE, 1, 2, 0, 0, 5, 3, 4, 0, 0, CountedResult());
	g_pActionInfoTable = &scene.actions;
	CHECK_EQ(3, destroyed);
	CHECK_EQ(0, executed);
	CHECK(started.empty());
	// No result is no harm.
	ExecuteActionInfoFromMainNode(ACTIONINFO_NULL, 1, 2, 0, 0, 5, 3, 4, 0, 0, NULL);
	CHECK(started.empty());
}

TEST(PacketSkillShows, AnActionWithoutPhasesRunsItsResultAtOnce)
{
	Scene scene;
	ExecuteActionInfoFromMainNode(SKILL_ATTACK_MELEE, 1, 2, 0, 0, 5, 3, 4, 0, 0, CountedResult());
	CHECK_EQ(1, executed);
	CHECK_EQ(1, destroyed);
	CHECK(started.empty());
	ExecuteActionInfoFromMainNode(SKILL_ATTACK_MELEE, 1, 2, 0, 0, 5, 3, 4, 0, 0, NULL);
	CHECK(started.empty());
	CHECK(worldStarted.empty());
}

TEST(PacketSkillShows, AnActionFromTheUserStartsAtTheUsersCellAimedAtTheTarget)
{
	Scene scene;
	scene.Show(MAGIC_BLOODY_NAIL, FLAG_ACTIONINFO_START_USER);
	ExecuteActionInfoFromMainNode(MAGIC_BLOODY_NAIL, 3, 4, 7, DIRECTION_LEFTUP, 501, 10, 12, 9, 40, CountedResult(), true, 2, -3);
	CHECK(started.size() == 1u);
	if (started.size() != 1u) return;
	const Started& s = started[0];
	CHECK_EQ(int(MAGIC_BLOODY_NAIL), int(s.action));
	CHECK_EQ(PixelX(3) + 2, s.x0);
	CHECK_EQ(PixelY(4) - 3, s.y0);
	CHECK_EQ(7, s.z0);
	CHECK_EQ(PixelX(10), s.x1);
	CHECK_EQ(PixelY(12), s.y1);
	CHECK_EQ(9, s.z1);
	CHECK_EQ(501, int(s.creatureID));
	CHECK_EQ(int(DIRECTION_LEFTUP), s.direction);
	CHECK_EQ(1, s.power);
	CHECK_EQ(1, s.step);		// no main node: the first phase
	CHECK_EQ(40, int(s.delay));
	CHECK_EQ(1, s.results);
	// The effect holds the result: it does not run yet, and goes with the
	// target the recorder did not keep.
	CHECK_EQ(0, executed);
	CHECK_EQ(1, destroyed);
	CHECK(worldStarted.empty());
}

TEST(PacketSkillShows, AnActionFromTheTargetOrFromNowhereStartsAtTheTarget)
{
	Scene scene;
	scene.Show(MAGIC_BLOODY_NAIL, FLAG_ACTIONINFO_START_TARGET);
	scene.Show(MAGIC_ACID_TOUCH, 0);
	for (TYPE_ACTIONINFO action : {TYPE_ACTIONINFO(MAGIC_BLOODY_NAIL), TYPE_ACTIONINFO(MAGIC_ACID_TOUCH)})
		ExecuteActionInfoFromMainNode(action, 3, 4, 7, DIRECTION_LEFTUP, 501, 10, 12, 9, 0, NULL, true, 2, -3);
	CHECK(started.size() == 2u);
	for (const Started& s : started)
	{
		CHECK_EQ(PixelX(10) + 2, s.x0);
		CHECK_EQ(PixelY(12) - 3, s.y0);
		CHECK_EQ(7, s.z0);
		CHECK_EQ(int(DIRECTION_LEFTUP), s.direction);
		CHECK_EQ(-1, s.results);
	}
}

TEST(PacketSkillShows, AnActionFromTheSkyFallsFromItsHeightFacingDown)
{
	Scene scene;
	scene.Show(MAGIC_BLOODY_NAIL, FLAG_ACTIONINFO_START_SKY, 300);
	scene.Show(MAGIC_ACID_TOUCH, FLAG_ACTIONINFO_START_SKY | FLAG_ACTIONINFO_START_USER, 300);
	for (TYPE_ACTIONINFO action : {TYPE_ACTIONINFO(MAGIC_BLOODY_NAIL), TYPE_ACTIONINFO(MAGIC_ACID_TOUCH)})
		ExecuteActionInfoFromMainNode(action, 3, 4, 7, DIRECTION_LEFTUP, 501, 10, 12, 9, 0, NULL, true, 2, -3);
	CHECK(started.size() == 2u);
	if (started.size() != 2u) return;
	// Sky alone: over the target.
	CHECK_EQ(PixelX(10) + 2, started[0].x0);
	CHECK_EQ(PixelY(12) - 3, started[0].y0);
	CHECK_EQ(307, started[0].z0);
	CHECK_EQ(int(DIRECTION_DOWN), started[0].direction);
	// Sky over the user.
	CHECK_EQ(PixelX(3) + 2, started[1].x0);
	CHECK_EQ(PixelY(4) - 3, started[1].y0);
	CHECK_EQ(307, started[1].z0);
	CHECK_EQ(int(DIRECTION_DOWN), started[1].direction);
}

TEST(PacketSkillShows, AnActionStartsFromItsMainNodeWhenAsked)
{
	Scene scene;
	scene.Show(MAGIC_BLOODY_NAIL, FLAG_ACTIONINFO_START_USER, 0, 1);
	scene.Show(MAGIC_ACID_TOUCH, FLAG_ACTIONINFO_START_USER);
	ExecuteActionInfoFromMainNode(MAGIC_BLOODY_NAIL, 3, 4, 0, 0, 501, 10, 12, 0, 0, NULL, true);
	ExecuteActionInfoFromMainNode(MAGIC_BLOODY_NAIL, 3, 4, 0, 0, 501, 10, 12, 0, 0, NULL, false);
	ExecuteActionInfoFromMainNode(MAGIC_ACID_TOUCH, 3, 4, 0, 0, 501, 10, 12, 0, 0, NULL, true);
	CHECK(started.size() == 3u);
	if (started.size() != 3u) return;
	CHECK_EQ(2, started[0].step);		// its main node, the second phase
	CHECK_EQ(1, started[1].step);		// not asked: the first
	CHECK_EQ(1, started[2].step);		// asked, but it has none
}

//----------------------------------------------------------------------
// SkillToSector
//----------------------------------------------------------------------
TEST(PacketSkillShows, ASkillToASectorStartsAtTheCellWithNobodyCasting)
{
	Scene scene;
	scene.Show(MAGIC_BLOODY_NAIL, FLAG_ACTIONINFO_START_USER, 300, 1);
	SkillToSector(MAGIC_BLOODY_NAIL, 6, 9, DIRECTION_LEFT, 25, CountedResult());
	CHECK(started.size() == 1u);
	if (started.size() != 1u) return;
	const Started& s = started[0];
	CHECK_EQ(int(MAGIC_BLOODY_NAIL), int(s.action));
	CHECK_EQ(PixelX(6), s.x0);
	CHECK_EQ(PixelY(9), s.y0);
	CHECK_EQ(0, s.z0);
	CHECK_EQ(PixelX(6), s.x1);
	CHECK_EQ(PixelY(9), s.y1);
	CHECK_EQ(0, s.z1);
	CHECK_EQ(int(OBJECTID_NULL), int(s.creatureID));
	CHECK_EQ(int(DIRECTION_LEFT), s.direction);
	CHECK_EQ(1, s.power);
	CHECK_EQ(1, s.step);		// the first phase, main node or not
	CHECK_EQ(25, int(s.delay));
	CHECK_EQ(1, s.results);
	CHECK_EQ(0, executed);
	CHECK(worldStarted.empty());
}

TEST(PacketSkillShows, ASkillToASectorFromTheSkyStartsAtItsHeightFacingAsGiven)
{
	Scene scene;
	scene.Show(MAGIC_BLOODY_NAIL, FLAG_ACTIONINFO_START_SKY, 300);
	SkillToSector(MAGIC_BLOODY_NAIL, 6, 9, DIRECTION_LEFT, 0, NULL);
	CHECK(started.size() == 1u);
	if (started.size() != 1u) return;
	CHECK_EQ(300, started[0].z0);
	CHECK_EQ(int(DIRECTION_LEFT), started[0].direction);
	CHECK_EQ(-1, started[0].results);
}

TEST(PacketSkillShows, ASkillToASectorPastTheTableFreesTheResultUnrun)
{
	Scene scene;
	SkillToSector(TYPE_ACTIONINFO(scene.actions.GetSize()), 6, 9, DIRECTION_LEFT, 0, CountedResult());
	CHECK(started.empty());
	CHECK_EQ(0, executed);
	CHECK_EQ(1, destroyed);
}

//----------------------------------------------------------------------
// SkillCrossCounter
//----------------------------------------------------------------------
// The copies below are each freed when the zone refuses them and then used
// all the same (PacketFunction.cpp:4353/4398, 4470/4490, 4589/4610 and
// 4729/4748). Not exercised: a new fake takes an ID no fake in the zone
// has, so the zone never refuses one.
TEST(PacketSkillShows, ACrossCounterNeedsBothCreatures)
{
	Scene scene;
	MCreature* target = scene.Put(501, 14, 12);
	SkillCrossCounter(NULL, target, SKILL_ATTACK_MELEE);
	SkillCrossCounter(target, NULL, SKILL_ATTACK_MELEE);
	CHECK_EQ(0, scene.zone->GetFakeCreatureNumber());
}

TEST(PacketSkillShows, ACrossCounterCopyStrikesFromTheCellBesideTheTarget)
{
	Scene scene;
	scene.actions.GetMutable(SKILL_DOUBLE_IMPACT)->Set("", ACTION_ATTACK, EFFECTSPRITETYPE_NULL, 1, FLAG_ACTIONINFO_TARGET_OTHER);
	MCreature* user = scene.Put(500, 10, 10);
	MCreature* target = scene.Put(501, 14, 12);
	user->SetBasicActionInfo(SKILL_DOUBLE_IMPACT);
	user->SetWeaponSpeed(MCreature::WEAPON_SPEED_FAST);
	user->SetDirection(DIRECTION_LEFT);
	user->SetCurrentDirection(DIRECTION_LEFT);
	// A melee strike is the user's basic action.
	SkillCrossCounter(user, target, SKILL_ATTACK_MELEE);
	std::vector<MFakeCreature*> fakes = scene.Fakes();
	CHECK(fakes.size() == 1u);
	if (fakes.size() != 1u) return;
	MFakeCreature* copy = fakes[0];
	// One cell from the target toward the user, the user's look, facing
	// the target, faded once.
	CHECK(Cell(copy->GetX(), copy->GetY()) == "13,11");
	CHECK_EQ(int(Slayer), int(copy->GetCreatureType()));
	CHECK_EQ(int(MCreature::WEAPON_SPEED_FAST), int(copy->GetWeaponSpeed()));
	CHECK_EQ(int(DIRECTION_RIGHTDOWN), int(copy->GetDirection()));
	CHECK_EQ(int(DIRECTION_RIGHTDOWN), int(copy->GetServerDirection()));
	CHECK_EQ(int(MFakeCreature::FAKE_CREATURE_FADE_ACTION), int(copy->GetFakeCreatureType()));
	CHECK(copy->IsFade());
	CHECK_EQ(int(ACTION_ATTACK), int(copy->GetAction()));
	// It strikes the target with the skill, the result in hand.
	CHECK_EQ(int(SKILL_DOUBLE_IMPACT), int(copy->GetSpecialActionInfo()));
	CHECK_EQ(int(SKILL_DOUBLE_IMPACT), int(copy->*(&CreatureAccess::m_nNextUsedActionInfo)));
	CHECK_EQ(501, int(TraceID(copy)));
	CHECK_EQ(14, int(copy->*(&CreatureAccess::m_TraceX)));
	CHECK_EQ(12, int(copy->*(&CreatureAccess::m_TraceY)));
	MActionResult* pResult = ResultOf(copy);
	CHECK(pResult != NULL);
	if (pResult == NULL) return;
	CHECK_EQ(1, pResult->GetSize());
	const MActionResultNode* pNode = *pResult->GetIterator();
	CHECK(pNode->IsNodeActionInfo());
	if (!pNode->IsNodeActionInfo()) return;
	const MActionResultNodeActionInfo* pInfo = static_cast<const MActionResultNodeActionInfo*>(pNode);
	CHECK_EQ(int(SKILL_DOUBLE_IMPACT), int(pInfo->m_nActionInfo));
	CHECK_EQ(500, int(pInfo->m_UserID));
	CHECK_EQ(501, int(pInfo->m_TargetID));
	CHECK_EQ(13, int(pInfo->m_sX));
	CHECK_EQ(11, int(pInfo->m_sY));
	CHECK_EQ(0, int(pInfo->m_DelayFrame));
}

TEST(PacketSkillShows, ACrossCounterOnTheUsersCellFacesAsTheUserDoes)
{
	Scene scene;
	MCreature* user = scene.Put(500, 14, 12);
	MCreature* target = scene.Put(501, 14, 12, Vampire);
	user->SetDirection(DIRECTION_UP);
	user->SetCurrentDirection(DIRECTION_UP);
	SkillCrossCounter(user, target, SKILL_DOUBLE_IMPACT);
	std::vector<MFakeCreature*> fakes = scene.Fakes();
	CHECK(fakes.size() == 1u);
	if (fakes.size() != 1u) return;
	CHECK(Cell(fakes[0]->GetX(), fakes[0]->GetY()) == "14,12");
	CHECK_EQ(int(DIRECTION_UP), int(fakes[0]->GetDirection()));
}

TEST(PacketSkillShows, ACrossCounterWithAResultsActionShowsTheCopyOnly)
{
	Scene scene;
	MCreature* user = scene.Put(500, 10, 10);
	MCreature* target = scene.Put(501, 14, 12);
	user->SetSpecialActionInfo(SKILL_DOUBLE_IMPACT);
	SkillCrossCounter(user, target, MIN_RESULT_ACTIONINFO);
	std::vector<MFakeCreature*> fakes = scene.Fakes();
	CHECK(fakes.size() == 1u);
	if (fakes.size() != 1u) return;
	CHECK(ResultOf(fakes[0]) == NULL);
	CHECK_EQ(int(SKILL_DOUBLE_IMPACT), int(fakes[0]->GetSpecialActionInfo()));
	CHECK_EQ(int(MFakeCreature::FAKE_CREATURE_FADE_ACTION), int(fakes[0]->GetFakeCreatureType()));
	// One below the results is still a skill.
	scene.actions.GetMutable(MIN_RESULT_ACTIONINFO - 1)->Set("", ACTION_ATTACK, EFFECTSPRITETYPE_NULL, 1, FLAG_ACTIONINFO_TARGET_OTHER);
	SkillCrossCounter(user, target, MIN_RESULT_ACTIONINFO - 1);
	fakes = scene.Fakes();
	CHECK(fakes.size() == 2u);
	if (fakes.size() == 2u)
	{
		CHECK_EQ(int(MIN_RESULT_ACTIONINFO - 1), int(fakes[1]->GetSpecialActionInfo()));
		CHECK(ResultOf(fakes[1]) != NULL);
	}
}

//----------------------------------------------------------------------
// The fast-moving copies: Shadow Dancing, Illendue, Blaze Walk
//----------------------------------------------------------------------
TEST(PacketSkillShows, ShadowDancingSendsThreeCopiesAroundTheTarget)
{
	Scene scene;
	MCreature* user = scene.Put(500, 10, 10);
	MCreature* target = scene.Put(501, 14, 12);
	user->SetBasicActionInfo(SKILL_DOUBLE_IMPACT);
	SkillShadowDancing(user, target, SKILL_ATTACK_MELEE);
	std::vector<MFakeCreature*> fakes = scene.Fakes();
	CHECK(fakes.size() == 3u);
	if (fakes.size() != 3u) return;
	// The target's cell toward the user, and the two mirrored about it.
	CHECK(Destinations(fakes) == std::vector<std::string>({"13,11", "13,13", "15,11"}));
	for (MFakeCreature* copy : fakes)
	{
		// From the user's cell, fast.
		CHECK(Cell(copy->GetX(), copy->GetY()) == "10,10");
		CHECK(copy->IsFastMove());
		CHECK_EQ(int(MFakeCreature::FAKE_CREATURE_FAST_MOVE_ACTION), int(copy->GetFakeCreatureType()));
		CHECK(copy->IsFade());
		CHECK_EQ(int(SKILL_DOUBLE_IMPACT), int(copy->GetBasicActionInfo()));
		CHECK_EQ(int(SKILL_DOUBLE_IMPACT), int(copy->GetSpecialActionInfo()));
		CHECK_EQ(501, int(TraceID(copy)));
		// Facing the target from where it started.
		CHECK_EQ(int(DIRECTION_RIGHTDOWN), int(copy->GetServerDirection()));
	}
}

TEST(PacketSkillShows, ShadowDancingWithoutAWayToTheTargetSendsThemAllToItsCell)
{
	Scene scene;
	MCreature* user = scene.Put(500, 14, 12);
	MCreature* target = scene.Put(501, 14, 12, Vampire);
	SkillShadowDancing(user, target, SKILL_DOUBLE_IMPACT);
	CHECK(Destinations(scene.Fakes()) == std::vector<std::string>({"14,12", "14,12", "14,12"}));
}

TEST(PacketSkillShows, ShadowDancingWithAResultsActionStopsAtTheFirstCopy)
{
	Scene scene;
	MCreature* user = scene.Put(500, 10, 10);
	MCreature* target = scene.Put(501, 14, 12);
	SkillShadowDancing(user, target, MIN_RESULT_ACTIONINFO);
	std::vector<MFakeCreature*> fakes = scene.Fakes();
	CHECK(fakes.size() == 1u);
	if (fakes.size() == 1u) CHECK(!fakes[0]->IsFastMove());
	SkillShadowDancing(NULL, target, SKILL_DOUBLE_IMPACT);
	SkillShadowDancing(user, NULL, SKILL_DOUBLE_IMPACT);
	CHECK_EQ(1, scene.zone->GetFakeCreatureNumber());
}

TEST(PacketSkillShows, IllendueSendsFourCopiesToTheTargetsCornersOrSides)
{
	Scene scene;
	MCreature* user = scene.Put(500, 10, 10);
	MCreature* target = scene.Put(501, 14, 12);
	// Facing straight: the corners two cells off.
	user->SetDirection(DIRECTION_RIGHT);
	user->SetCurrentDirection(DIRECTION_RIGHT);
	SkillIllendue(user, target, SKILL_DOUBLE_IMPACT);
	std::vector<MFakeCreature*> fakes = scene.Fakes();
	CHECK(Destinations(fakes) == std::vector<std::string>({"12,10", "12,14", "16,10", "16,14"}));
	for (MFakeCreature* copy : fakes)
	{
		CHECK(Cell(copy->GetX(), copy->GetY()) == "10,10");
		CHECK(copy->IsFastMove());
		CHECK_EQ(int(MFakeCreature::FAKE_CREATURE_FAST_MOVE_ACTION), int(copy->GetFakeCreatureType()));
		CHECK_EQ(int(SKILL_DOUBLE_IMPACT), int(copy->GetSpecialActionInfo()));
		CHECK_EQ(501, int(TraceID(copy)));
	}
	// Facing on a diagonal: the sides two cells off.
	for (MFakeCreature* copy : fakes) scene.zone->RemoveFakeCreature(copy->GetID());
	user->SetDirection(DIRECTION_RIGHTDOWN);
	user->SetCurrentDirection(DIRECTION_RIGHTDOWN);
	SkillIllendue(user, target, SKILL_DOUBLE_IMPACT);
	CHECK(Destinations(scene.Fakes()) == std::vector<std::string>({"14,14", "16,12", "12,12", "14,10"}));
	// Illusion Inversion takes the corners whichever way the user faces.
	for (MFakeCreature* copy : scene.Fakes()) scene.zone->RemoveFakeCreature(copy->GetID());
	SkillIllendue(user, target, TEMP_SKILL_ILLUSION_INVERSION);
	CHECK(Destinations(scene.Fakes()) == std::vector<std::string>({"12,10", "12,14", "16,10", "16,14"}));
}

TEST(PacketSkillShows, IllendueWithAMeleeStrikeUsesTheBasicActionAndAResultsActionStops)
{
	Scene scene;
	MCreature* user = scene.Put(500, 10, 10);
	MCreature* target = scene.Put(501, 14, 12);
	user->SetBasicActionInfo(SKILL_DOUBLE_IMPACT);
	SkillIllendue(user, target, SKILL_ATTACK_MELEE);
	std::vector<MFakeCreature*> fakes = scene.Fakes();
	CHECK(fakes.size() == 4u);
	for (MFakeCreature* copy : fakes) CHECK_EQ(int(SKILL_DOUBLE_IMPACT), int(copy->GetSpecialActionInfo()));
	for (MFakeCreature* copy : fakes) scene.zone->RemoveFakeCreature(copy->GetID());
	SkillIllendue(user, target, MIN_RESULT_ACTIONINFO);
	fakes = scene.Fakes();
	CHECK(fakes.size() == 1u);
	if (fakes.size() == 1u) CHECK(!fakes[0]->IsFastMove());
	SkillIllendue(NULL, target, SKILL_DOUBLE_IMPACT);
	SkillIllendue(user, NULL, SKILL_DOUBLE_IMPACT);
	CHECK_EQ(1, scene.zone->GetFakeCreatureNumber());
}

TEST(PacketSkillShows, BlazeWalkSendsOneCopyAGradeEachLaterThanTheLast)
{
	Scene scene;
	MCreature* user = scene.Put(500, 10, 10);
	MCreature* target = scene.Put(501, 14, 12);
	user->SetBasicActionInfo(SKILL_DOUBLE_IMPACT);
	// Grade 0: one copy, to the target's right.
	SkillBlazeWalk(user, target, SKILL_ATTACK_MELEE, 0);
	std::vector<MFakeCreature*> fakes = scene.Fakes();
	CHECK(Destinations(fakes) == std::vector<std::string>({"15,12"}));
	for (MFakeCreature* copy : fakes) scene.zone->RemoveFakeCreature(copy->GetID());
	// Grade 2: right, left and below, facing left, up and right... as the
	// directions 0, 4 and 6 are; 4, 8 and 12 frames late.
	SkillBlazeWalk(user, target, SKILL_DOUBLE_IMPACT, 2);
	fakes = scene.Fakes();
	CHECK(Destinations(fakes) == std::vector<std::string>({"15,12", "13,12", "14,13"}));
	if (fakes.size() != 3u) return;
	const int directions[3] = { 0, 4, 6 };
	for (int i = 0; i < 3; ++i)
	{
		MFakeCreature* copy = fakes[i];
		CHECK(Cell(copy->GetX(), copy->GetY()) == "10,10");
		CHECK(copy->IsFastMove());
		CHECK_EQ(int(MFakeCreature::FAKE_CREATURE_FAST_MOVE_ACTION), int(copy->GetFakeCreatureType()));
		CHECK_EQ(int(SKILL_DOUBLE_IMPACT), int(copy->GetSpecialActionInfo()));
		CHECK_EQ(501, int(TraceID(copy)));
		CHECK_EQ(directions[i], int(copy->GetServerDirection()));
		CHECK_EQ(4 * (i + 1), int(copy->*(&FakeAccess::m_Delay)));
	}
}

TEST(PacketSkillShows, BlazeWalkWithAResultsActionStopsAtTheFirstCopy)
{
	Scene scene;
	MCreature* user = scene.Put(500, 10, 10);
	MCreature* target = scene.Put(501, 14, 12);
	SkillBlazeWalk(user, target, MIN_RESULT_ACTIONINFO, 2);
	std::vector<MFakeCreature*> fakes = scene.Fakes();
	CHECK(fakes.size() == 1u);
	if (fakes.size() == 1u)
	{
		CHECK(!fakes[0]->IsFastMove());
		CHECK_EQ(0, int(fakes[0]->*(&FakeAccess::m_Delay)));
	}
	SkillBlazeWalk(NULL, target, SKILL_DOUBLE_IMPACT, 0);
	SkillBlazeWalk(user, NULL, SKILL_DOUBLE_IMPACT, 0);
	CHECK_EQ(1, scene.zone->GetFakeCreatureNumber());
	// (Grade 3 and up is a defect, not exercised: PacketFunction.cpp:4699
	// loops to the grade, reading past the three cells of TargetList and
	// leaving the direction unset.)
}

//----------------------------------------------------------------------
// Gilles de Rais's lair
//----------------------------------------------------------------------
namespace {
struct Effects
{
	// The smoke statuses each show a sprite type of their own (its frame
	// its number); a sector effect of each lies on every cell it is lifted
	// from.
	static void Show(Scene& scene, int status, int type)
	{
		testfw::MutableRow(scene.effectStatuses, status).EffectSpriteType = TYPE_EFFECTSPRITETYPE(type);
		testfw::MutableRow(scene.effectSprites, type).FrameID = TYPE_FRAMEID(type);
	}
	static void Lay(Scene& scene, int type, int x, int y)
	{
		MEffect* pEffect = new MEffect(BLT_EFFECT);
		pEffect->SetFrameID(TYPE_FRAMEID(type), 4);
		pEffect->SetPosition(TYPE_SECTORPOSITION(x), TYPE_SECTORPOSITION(y));
		pEffect->SetCount(100);
		CHECK(scene.zone->AddEffect(pEffect));
	}
};

// The cells and smokes the upper (1) and lower (2) halves lay and lift:
// nine small smokes round the hall, then four large at the posts.
struct Smoke { int status; int action; int x, y; };
std::vector<Smoke> Smokes(int cx, int cy, const int posts[4][2])
{
	return {
		{ EFFECTSTATUS_BLACK_SMALL_SMOKE_1, MAP_BLACK_SMALL_SMOKE_1, cx, cy },
		{ EFFECTSTATUS_BLACK_SMALL_SMOKE_2, MAP_BLACK_SMALL_SMOKE_2, cx, cy - 1 },
		{ EFFECTSTATUS_BLACK_SMALL_SMOKE_3, MAP_BLACK_SMALL_SMOKE_3, cx - 1, cy - 1 },
		{ EFFECTSTATUS_BLACK_SMALL_SMOKE_1, MAP_BLACK_SMALL_SMOKE_1, cx + 1, cy - 1 },
		{ EFFECTSTATUS_BLACK_SMALL_SMOKE_2, MAP_BLACK_SMALL_SMOKE_2, cx - 1, cy },
		{ EFFECTSTATUS_BLACK_SMALL_SMOKE_3, MAP_BLACK_SMALL_SMOKE_3, cx + 1, cy },
		{ EFFECTSTATUS_BLACK_SMALL_SMOKE_1, MAP_BLACK_SMALL_SMOKE_1, cx - 1, cy + 1 },
		{ EFFECTSTATUS_BLACK_SMALL_SMOKE_2, MAP_BLACK_SMALL_SMOKE_2, cx + 1, cy + 1 },
		{ EFFECTSTATUS_BLACK_SMALL_SMOKE_3, MAP_BLACK_SMALL_SMOKE_3, cx, cy + 1 },
		{ EFFECTSTATUS_BLACK_LARGE_SMOKE, MAP_BLACK_LARGE_SMOKE, posts[0][0], posts[0][1] },
		{ EFFECTSTATUS_BLACK_LARGE_SMOKE, MAP_BLACK_LARGE_SMOKE, posts[1][0], posts[1][1] },
		{ EFFECTSTATUS_BLACK_LARGE_SMOKE, MAP_BLACK_LARGE_SMOKE, posts[2][0], posts[2][1] },
		{ EFFECTSTATUS_BLACK_LARGE_SMOKE, MAP_BLACK_LARGE_SMOKE, posts[3][0], posts[3][1] },
	};
}
const int kUpperPosts[4][2] = { { 137, 80 }, { 143, 86 }, { 136, 93 }, { 130, 87 } };
const int kLowerPosts[4][2] = { { 83, 134 }, { 89, 140 }, { 82, 147 }, { 76, 141 } };

// A scene of the hard lair (1413), every map action shown from the target.
struct Lair : Scene
{
	Lair() : Scene(150, 150)
	{
		zone->SetCurrentZoneID(1413);
		for (int action : { MAP_BLACK_SMALL_SMOKE_1, MAP_BLACK_SMALL_SMOKE_2, MAP_BLACK_SMALL_SMOKE_3, MAP_BLACK_LARGE_SMOKE,
			MAP_BLACK_HALL, MAP_ELECTRIC_POST, MAP_GDR_LAIR_POTAL })
			Show(TYPE_ACTIONINFO(action), FLAG_ACTIONINFO_START_TARGET, 0, 1);
		Effects::Show(*this, EFFECTSTATUS_BLACK_SMALL_SMOKE_1, 41);
		Effects::Show(*this, EFFECTSTATUS_BLACK_SMALL_SMOKE_2, 42);
		Effects::Show(*this, EFFECTSTATUS_BLACK_SMALL_SMOKE_3, 43);
		Effects::Show(*this, EFFECTSTATUS_BLACK_LARGE_SMOKE, 44);
	}
	int TypeOf(int status) { return int(effectStatuses[status].EffectSpriteType); }
};

// An effect started at the cell for the creature, as "action|x,y|id|dir|step".
std::string Start(int action, int x, int y, int id, int direction)
{
	return std::to_string(action) + "|" + Cell(PixelX(x), PixelY(y)) + "|" + std::to_string(id) + "|" + std::to_string(direction) + "|1";
}
std::vector<std::string> Starts()
{
	std::vector<std::string> made;
	for (const Started& s : started)
		made.push_back(std::to_string(s.action) + "|" + Cell(s.x1, s.y1) + "|" + std::to_string(s.creatureID) + "|" + std::to_string(s.direction) + "|" + std::to_string(s.step));
	return made;
}
}

TEST(PacketSkillShows, TheLairLaysItsSmokeOutsideTheBossesComing)
{
	for (int half : { 1, 2 })
	{
		Lair lair;
		const std::vector<Smoke> smokes = half == 1 ? Smokes(136, 86, kUpperPosts) : Smokes(83, 139, kLowerPosts);
		Add_GDR_Effect(half, false);
		// Each smoke at its cell from the start of its show, the player's
		// direction; the small ones for the player, the large for nobody.
		std::vector<std::string> wanted;
		for (size_t i = 0; i < smokes.size(); ++i)
			wanted.push_back(Start(smokes[i].action, smokes[i].x, smokes[i].y, i < 9 ? 77 : 0, DIRECTION_RIGHTDOWN));
		CHECK(Starts() == wanted);
		CHECK(worldStarted.empty());
	}
}

TEST(PacketSkillShows, TheBossesComingLiftsTheSmokeAndRaisesTheHallAndPosts)
{
	for (int half : { 1, 2 })
	{
		Lair lair;
		const std::vector<Smoke> smokes = half == 1 ? Smokes(136, 86, kUpperPosts) : Smokes(83, 139, kLowerPosts);
		const int (*posts)[2] = half == 1 ? kUpperPosts : kLowerPosts;
		for (const Smoke& smoke : smokes) Effects::Lay(lair, lair.TypeOf(smoke.status), smoke.x, smoke.y);
		// Another half's smoke stays, and so does a large smoke on a small
		// one's cell.
		Effects::Lay(lair, lair.TypeOf(EFFECTSTATUS_BLACK_LARGE_SMOKE), smokes[0].x, smokes[0].y);
		CHECK_EQ(14, lair.zone->GetEffectCount());
		Add_GDR_Effect(half, true);
		CHECK_EQ(1, lair.zone->GetEffectCount());
		CHECK_EQ(1, int(lair.zone->GetSector(TYPE_SECTORPOSITION(smokes[0].x), TYPE_SECTORPOSITION(smokes[0].y)).GetEffectSize()));
		const int cx = smokes[0].x, cy = smokes[0].y;
		CHECK(Starts() == std::vector<std::string>({
			Start(MAP_BLACK_HALL, cx, cy, 77, DIRECTION_RIGHTDOWN),
			Start(MAP_ELECTRIC_POST, posts[0][0], posts[0][1], 77, DIRECTION_RIGHTDOWN),
			Start(MAP_ELECTRIC_POST, posts[1][0], posts[1][1], 77, DIRECTION_RIGHTDOWN),
			Start(MAP_ELECTRIC_POST, posts[2][0], posts[2][1], 77, DIRECTION_RIGHTDOWN),
			Start(MAP_ELECTRIC_POST, posts[3][0], posts[3][1], 77, DIRECTION_RIGHTDOWN),
		}));
	}
}

TEST(PacketSkillShows, TheLairsEffectsAreTheHardLairsAndItsTwoHalvesOnly)
{
	Lair lair;
	Add_GDR_Effect(3, false);
	Add_GDR_Effect(0, true);
	CHECK(started.empty());
	for (int zoneID : { 1412, 1414 })
	{
		lair.zone->SetCurrentZoneID(WORD(zoneID));
		Add_GDR_Effect(1, false);
		Add_GDR_Effect(2, true);
	}
	CHECK(started.empty());
}

TEST(PacketSkillShows, TheIllusionsWayShowsItsPortals)
{
	Lair lair;
	Add_GDR_Potal_Effect(1410);
	CHECK(Starts() == std::vector<std::string>({
		Start(MAP_GDR_LAIR_POTAL, 73, 80, 0, 0),
		Start(MAP_GDR_LAIR_POTAL, 118, 69, 0, 0),
	}));
	started.clear();
	Add_GDR_Potal_Effect(1411);
	CHECK(Starts() == std::vector<std::string>({
		Start(MAP_GDR_LAIR_POTAL, 125, 58, 0, 0),
		Start(MAP_GDR_LAIR_POTAL, 22, 85, 0, 0),
		Start(MAP_GDR_LAIR_POTAL, 9, 86, 0, 0),
		Start(MAP_GDR_LAIR_POTAL, 12, 102, 0, 0),
	}));
	started.clear();
	Add_GDR_Potal_Effect(1409);
	Add_GDR_Potal_Effect(1412);
	CHECK(started.empty());
}

//----------------------------------------------------------------------
// Monsters in a slayer's and an ouster's shape
//----------------------------------------------------------------------
namespace {
std::unique_ptr<GCAddMonster> Monster(TYPE_OBJECTID id, int type, int x, int y, int dir = DIRECTION_LEFT)
{
	std::unique_ptr<GCAddMonster> pPacket(new GCAddMonster);
	pPacket->setObjectID(id);
	pPacket->setMonsterType(MonsterType_t(type));
	pPacket->setMonsterName("Mimic");
	pPacket->setX(Coord_t(x));
	pPacket->setY(Coord_t(y));
	pPacket->setDir(Dir_t(dir));
	pPacket->setMaxHP(300);
	pPacket->setCurrentHP(120);
	pPacket->setMainColor(0);
	pPacket->setSubColor(0);
	return pPacket;
}

// The packet item tables the shapes are dressed from (InitPacketItemTable
// loads them in the game): an item of each slot read, its frame given, in
// place while the scene lasts.
struct Wardrobe
{
	struct Slot { MItem** slot; MItem* old; };
	std::vector<Slot> slots;
	std::vector<std::unique_ptr<MItem>> items;
	void Hang(MItem** slot, ITEM_CLASS itemClass, int type)
	{
		ITEMTYPE_TABLE& types = testfw::MutableRow(*g_pItemTable, itemClass);
		if (types.GetSize() <= type) types.Init(type + 1);
		testfw::MutableRow(types, type).AddonMaleFrameID = 11;
		testfw::MutableRow(types, type).AddonFemaleFrameID = 12;
		items.emplace_back(MItem::NewItem(itemClass));
		items.back()->SetItemType(TYPE_ITEMTYPE(type));
		slots.push_back({ slot, *slot });
		*slot = items.back().get();
	}
	~Wardrobe()
	{
		for (const Slot& s : slots) *s.slot = s.old;
	}
};

std::string Worn(const MCreatureWear* pCreature, int addon)
{
	const MCreatureWear::ADDON_INFO& info = pCreature->GetAddonInfo(addon);
	if (!info.bAddon) return "-";
	return std::to_string(int(info.ItemClass)) + "/" + std::to_string(int(info.ItemType));
}
std::string Colours(const MCreatureWear* pCreature, int addon)
{
	const MCreatureWear::ADDON_INFO& info = pCreature->GetAddonInfo(addon);
	return std::to_string(int(info.ColorSet1)) + "," + std::to_string(int(info.ColorSet2));
}
}

TEST(PacketSkillShows, ANewSlayerShapedMonsterIsMadeDressedAndAdded)
{
	Scene scene;
	Wardrobe wardrobe;
	wardrobe.Hang(&g_pPacketItemJacket[JACKET3], ITEM_CLASS_COAT, 8);
	wardrobe.Hang(&g_pPacketItemPants[PANTS3], ITEM_CLASS_TROUSER, 8);
	wardrobe.Hang(&g_pPacketItemHelm[HELMET2], ITEM_CLASS_HELM, 3);
	wardrobe.Hang(&g_pPacketItemWeapon[WEAPON_SR], ITEM_CLASS_SR, 0);
	std::unique_ptr<GCAddMonster> pPacket = Monster(600, Slayer, 12, 9, DIRECTION_LEFTUP);
	Add_Race_SlayerMonster(pPacket.get());
	MCreature* pCreature = scene.zone->GetCreature(600);
	CHECK(pCreature != NULL);
	if (pCreature == NULL) return;
	CHECK(pCreature->IsWear());
	const MCreatureWear* pWear = static_cast<const MCreatureWear*>(pCreature);
	CHECK_EQ(int(Slayer), int(pCreature->GetCreatureType()));
	CHECK(std::string(pCreature->GetName()) == "Mimic");
	CHECK(Cell(pCreature->GetX(), pCreature->GetY()) == "12,9");
	CHECK(Cell(pCreature->GetServerX(), pCreature->GetServerY()) == "12,9");
	// Turned down, though it faces the packet's way until it next turns.
	CHECK_EQ(int(DIRECTION_DOWN), int(pCreature->GetServerDirection()));
	CHECK_EQ(int(DIRECTION_LEFTUP), int(pCreature->GetDirection()));
	CHECK_EQ(int(ACTION_STAND), int(pCreature->GetAction()));
	CHECK_EQ(1, pCreature->GetGuildNumber());
	CHECK(pCreature->IsGroundCreature());
	CHECK_EQ(300, int(pCreature->GetMAX_HP()));
	CHECK_EQ(120, int(pCreature->GetHP()));
	// In the third coat and trousers, the second helmet and a sniper rifle.
	CHECK(Worn(pWear, ADDON_COAT) == std::to_string(int(ITEM_CLASS_COAT)) + "/8");
	CHECK(Worn(pWear, ADDON_TROUSER) == std::to_string(int(ITEM_CLASS_TROUSER)) + "/8");
	CHECK(Worn(pWear, ADDON_HELM) == std::to_string(int(ITEM_CLASS_HELM)) + "/3");
	CHECK(Worn(pWear, ADDON_RIGHTHAND) == std::to_string(int(ITEM_CLASS_SR)) + "/0");
	CHECK(!pCreature->HasEffectStatus(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS));
	CHECK(started.empty());
}

TEST(PacketSkillShows, TheCaptiveSlayersAreBasicallyDressed)
{
	Scene scene;
	scene.Tribe(796);
	scene.Tribe(CREATURETYPE_SLAYER_FEMALE);
	Wardrobe wardrobe;
	wardrobe.Hang(&g_pPacketItemJacket[JACKET3], ITEM_CLASS_COAT, 8);
	// 796 stays itself; 797 becomes a female slayer.
	std::unique_ptr<GCAddMonster> man = Monster(600, 796, 12, 9);
	std::unique_ptr<GCAddMonster> woman = Monster(601, 797, 13, 9);
	Add_Race_SlayerMonster(man.get());
	Add_Race_SlayerMonster(woman.get());
	MCreature* pMan = scene.zone->GetCreature(600);
	MCreature* pWoman = scene.zone->GetCreature(601);
	CHECK(pMan != NULL && pWoman != NULL);
	if (pMan == NULL || pWoman == NULL) return;
	CHECK_EQ(796, int(pMan->GetCreatureType()));
	CHECK_EQ(int(CREATURETYPE_SLAYER_FEMALE), int(pWoman->GetCreatureType()));
	for (MCreature* pCreature : { pMan, pWoman })
	{
		const MCreatureWear* pWear = static_cast<const MCreatureWear*>(pCreature);
		// The basic slots hold nothing (as the game's tables do): the third
		// coat is not put on, only the colour on what it wears.
		CHECK(Worn(pWear, ADDON_COAT) != std::to_string(int(ITEM_CLASS_COAT)) + "/8");
		CHECK(Colours(pWear, ADDON_COAT) == "393,393");
		CHECK(Colours(pWear, ADDON_TROUSER) == "393,393");
	}
}

TEST(PacketSkillShows, ASacrificedSlayerIsWounded)
{
	Scene scene;
	scene.Tribe(793);
	scene.Show(RESULT_MAGIC_CAUSE_CRITICAL_WOUNDS, FLAG_ACTIONINFO_START_USER);
	std::unique_ptr<GCAddMonster> pPacket = Monster(600, 793, 12, 9);
	Add_Race_SlayerMonster(pPacket.get());
	MCreature* pCreature = scene.zone->GetCreature(600);
	CHECK(pCreature != NULL);
	if (pCreature == NULL) return;
	CHECK(pCreature->HasEffectStatus(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS));
	CHECK(started.size() == 1u);
	if (started.size() != 1u) return;
	const Started& s = started[0];
	CHECK_EQ(int(RESULT_MAGIC_CAUSE_CRITICAL_WOUNDS), int(s.action));
	CHECK(Cell(s.x0, s.y0) == Cell(PixelX(12), PixelY(9)));
	CHECK(Cell(s.x1, s.y1) == Cell(PixelX(12), PixelY(9)));
	CHECK_EQ(600, int(s.creatureID));
	CHECK_EQ(int(DIRECTION_DOWN), s.direction);
	CHECK_EQ(0xffff, int(s.delay));
	CHECK_EQ(1, s.step);
}

TEST(PacketSkillShows, AKnownSlayerShapedMonsterIsSetInPlace)
{
	Scene scene;
	scene.Tribe(793);
	scene.Show(RESULT_MAGIC_CAUSE_CRITICAL_WOUNDS, FLAG_ACTIONINFO_START_USER);
	MCreature* pKnown = scene.Put(600, 5, 5, 793);
	pKnown->SetFlyingCreature();
	std::unique_ptr<GCAddMonster> pPacket = Monster(600, 793, 6, 7, DIRECTION_UP);
	Add_Race_SlayerMonster(pPacket.get());
	CHECK(scene.zone->GetCreature(600) == pKnown);
	CHECK(Cell(pKnown->GetX(), pKnown->GetY()) == "6,7");
	CHECK(Cell(pKnown->GetServerX(), pKnown->GetServerY()) == "6,7");
	// The known one turns the packet's way.
	CHECK_EQ(int(DIRECTION_UP), int(pKnown->GetServerDirection()));
	CHECK_EQ(int(DIRECTION_UP), int(pKnown->GetDirection()));
	CHECK_EQ(1, pKnown->GetGuildNumber());
	CHECK(pKnown->IsGroundCreature());
	CHECK_EQ(300, int(pKnown->GetMAX_HP()));
	CHECK_EQ(120, int(pKnown->GetHP()));
	// A sacrifice is wounded where it now stands.
	CHECK(pKnown->HasEffectStatus(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS));
	CHECK(started.size() == 1u);
	if (started.size() == 1u)
	{
		CHECK(Cell(started[0].x1, started[0].y1) == Cell(PixelX(6), PixelY(7)));
		CHECK_EQ(600, int(started[0].creatureID));
	}
	// Anything else stands, unwounded, and keeps its own type.
	MCreature* pOther = scene.Put(601, 8, 8);
	pOther->SetAction(ACTION_MOVE);
	std::unique_ptr<GCAddMonster> other = Monster(601, Vampire, 9, 9, DIRECTION_UP);
	Add_Race_SlayerMonster(other.get());
	CHECK_EQ(int(ACTION_STAND), int(pOther->GetAction()));
	CHECK_EQ(int(Slayer), int(pOther->GetCreatureType()));
	CHECK(Cell(pOther->GetX(), pOther->GetY()) == "9,9");
	CHECK(!pOther->HasEffectStatus(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS));
	CHECK_EQ(1u, started.size());
}

TEST(PacketSkillShows, ASlayerShapedMonsterOffTheZoneIsDropped)
{
	Scene scene;
	scene.Tribe(793);
	scene.Show(RESULT_MAGIC_CAUSE_CRITICAL_WOUNDS, FLAG_ACTIONINFO_START_USER);
	std::unique_ptr<GCAddMonster> pPacket = Monster(600, 793, 45, 9);
	Add_Race_SlayerMonster(pPacket.get());
	CHECK(scene.zone->GetCreature(600) == NULL);
	CHECK(started.empty());
}

TEST(PacketSkillShows, ANewOusterShapedMonsterIsMadeAndAdded)
{
	Scene scene;
	std::unique_ptr<GCAddMonster> pPacket = Monster(600, Ousters, 12, 9, DIRECTION_LEFTUP);
	Add_Race_OustersMonster(pPacket.get());
	MCreature* pCreature = scene.zone->GetCreature(600);
	CHECK(pCreature != NULL);
	if (pCreature == NULL) return;
	CHECK(pCreature->IsWear());
	CHECK_EQ(int(Ousters), int(pCreature->GetCreatureType()));
	CHECK(std::string(pCreature->GetName()) == "Mimic");
	CHECK(Cell(pCreature->GetX(), pCreature->GetY()) == "12,9");
	CHECK(Cell(pCreature->GetServerX(), pCreature->GetServerY()) == "12,9");
	CHECK_EQ(int(DIRECTION_DOWN), int(pCreature->GetServerDirection()));
	CHECK_EQ(int(DIRECTION_LEFTUP), int(pCreature->GetDirection()));
	CHECK_EQ(int(ACTION_STAND), int(pCreature->GetAction()));
	CHECK_EQ(1, pCreature->GetGuildNumber());
	CHECK_EQ(int(MCreature::WEAPON_SPEED_NORMAL), int(pCreature->GetWeaponSpeed()));
	CHECK_EQ(300, int(pCreature->GetMAX_HP()));
	CHECK_EQ(120, int(pCreature->GetHP()));
	CHECK(!pCreature->IsFade());
	CHECK(!pCreature->HasEffectStatus(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS));
	CHECK(started.empty());
}

TEST(PacketSkillShows, TheCaptiveOusterIsDressedFadedAndInItsColour)
{
	Scene scene;
	scene.Tribe(800, CREATURETRIBE_OUSTERS);
	Wardrobe wardrobe;
	wardrobe.Hang(&g_pPacketItemOustersCoat[OUSTERS_COAT_BASIC], ITEM_CLASS_OUSTERS_COAT, 2);
	wardrobe.Hang(&g_pPacketItemOustersArm[OUSTERS_ARM_CHAKRAM], ITEM_CLASS_OUSTERS_CHAKRAM, 1);
	std::unique_ptr<GCAddMonster> pPacket = Monster(600, 800, 12, 9);
	Add_Race_OustersMonster(pPacket.get());
	MCreature* pCreature = scene.zone->GetCreature(600);
	CHECK(pCreature != NULL);
	if (pCreature == NULL) return;
	const MCreatureWear* pWear = static_cast<const MCreatureWear*>(pCreature);
	CHECK(Worn(pWear, ADDON_COAT) == std::to_string(int(ITEM_CLASS_OUSTERS_COAT)) + "/2");
	CHECK(Worn(pWear, ADDON_RIGHTHAND) == std::to_string(int(ITEM_CLASS_OUSTERS_CHAKRAM)) + "/1");
	CHECK(Colours(pWear, ADDON_COAT) == "393,393");
	CHECK(Colours(pWear, ADDON_RIGHTHAND) == "393,393");
	CHECK_EQ(393, int(pCreature->GetBodyColor1()));
	CHECK_EQ(393, int(pCreature->GetBodyColor2()));
	CHECK(pCreature->IsFade());
}

TEST(PacketSkillShows, TheCaptiveOusterWithoutItsTablesIsOnlyColoured)
{
	Scene scene;
	scene.Tribe(800, CREATURETRIBE_OUSTERS);
	// Without the chakram (the tables not loaded) neither is worn.
	Wardrobe wardrobe;
	wardrobe.Hang(&g_pPacketItemOustersCoat[OUSTERS_COAT_BASIC], ITEM_CLASS_OUSTERS_COAT, 2);
	MItem* oldChakram = g_pPacketItemOustersArm[OUSTERS_ARM_CHAKRAM];
	g_pPacketItemOustersArm[OUSTERS_ARM_CHAKRAM] = NULL;
	std::unique_ptr<GCAddMonster> pPacket = Monster(600, 800, 12, 9);
	Add_Race_OustersMonster(pPacket.get());
	g_pPacketItemOustersArm[OUSTERS_ARM_CHAKRAM] = oldChakram;
	MCreature* pCreature = scene.zone->GetCreature(600);
	CHECK(pCreature != NULL);
	if (pCreature == NULL) return;
	const MCreatureWear* pWear = static_cast<const MCreatureWear*>(pCreature);
	CHECK(Worn(pWear, ADDON_COAT) != std::to_string(int(ITEM_CLASS_OUSTERS_COAT)) + "/2");
	CHECK(Colours(pWear, ADDON_COAT) == "393,393");
	CHECK(pCreature->IsFade());
}

TEST(PacketSkillShows, ASacrificedOusterIsWoundedNewOrKnown)
{
	Scene scene;
	scene.Tribe(795, CREATURETRIBE_OUSTERS);
	scene.Show(RESULT_MAGIC_CAUSE_CRITICAL_WOUNDS, FLAG_ACTIONINFO_START_USER);
	std::unique_ptr<GCAddMonster> fresh = Monster(600, 795, 12, 9);
	Add_Race_OustersMonster(fresh.get());
	MCreature* pFresh = scene.zone->GetCreature(600);
	CHECK(pFresh != NULL && pFresh->HasEffectStatus(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS));
	MCreature* pKnown = scene.Put(601, 5, 5, Ousters);
	std::unique_ptr<GCAddMonster> again = Monster(601, 795, 6, 7, DIRECTION_UP);
	Add_Race_OustersMonster(again.get());
	CHECK(scene.zone->GetCreature(601) == pKnown);
	CHECK(pKnown->HasEffectStatus(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS));
	CHECK(started.size() == 2u);
	if (started.size() != 2u) return;
	CHECK_EQ(600, int(started[0].creatureID));
	CHECK(Cell(started[0].x1, started[0].y1) == Cell(PixelX(12), PixelY(9)));
	CHECK_EQ(601, int(started[1].creatureID));
	CHECK(Cell(started[1].x0, started[1].y0) == Cell(PixelX(6), PixelY(7)));
	CHECK(Cell(started[1].x1, started[1].y1) == Cell(PixelX(6), PixelY(7)));
	CHECK_EQ(int(DIRECTION_DOWN), started[1].direction);
	CHECK_EQ(0xffff, int(started[1].delay));
}

TEST(PacketSkillShows, AKnownOusterShapedMonsterIsSetInPlaceTurnedDown)
{
	Scene scene;
	MCreature* pKnown = scene.Put(601, 5, 5, Ousters);
	pKnown->SetWeaponSpeed(80);
	pKnown->SetAction(ACTION_MOVE);
	std::unique_ptr<GCAddMonster> again = Monster(601, Vampire, 6, 7, DIRECTION_UP);
	Add_Race_OustersMonster(again.get());
	CHECK(scene.zone->GetCreature(601) == pKnown);
	CHECK_EQ(int(Ousters), int(pKnown->GetCreatureType()));
	CHECK(Cell(pKnown->GetX(), pKnown->GetY()) == "6,7");
	CHECK(Cell(pKnown->GetServerX(), pKnown->GetServerY()) == "6,7");
	// A known ouster is turned down, unlike a known slayer.
	CHECK_EQ(int(DIRECTION_DOWN), int(pKnown->GetServerDirection()));
	CHECK_EQ(int(DIRECTION_UP), int(pKnown->GetDirection()));
	CHECK_EQ(int(ACTION_STAND), int(pKnown->GetAction()));
	CHECK_EQ(1, pKnown->GetGuildNumber());
	// Speed 40 is the normal speed.
	CHECK_EQ(int(MCreature::WEAPON_SPEED_NORMAL), int(pKnown->GetWeaponSpeed()));
	CHECK_EQ(300, int(pKnown->GetMAX_HP()));
	CHECK_EQ(120, int(pKnown->GetHP()));
	CHECK(!pKnown->HasEffectStatus(EFFECTSTATUS_CAUSE_CRITICAL_WOUNDS));
	CHECK(started.empty());
}

TEST(PacketSkillShows, AnOusterShapedMonsterOffTheZoneIsDropped)
{
	Scene scene;
	scene.Tribe(795, CREATURETRIBE_OUSTERS);
	scene.Show(RESULT_MAGIC_CAUSE_CRITICAL_WOUNDS, FLAG_ACTIONINFO_START_USER);
	std::unique_ptr<GCAddMonster> pPacket = Monster(600, 795, 12, 31);
	Add_Race_OustersMonster(pPacket.get());
	CHECK(scene.zone->GetCreature(600) == NULL);
	CHECK(started.empty());
}

//----------------------------------------------------------------------
// The rocket, the wild wolf, the dragon tornado, the elemental
//----------------------------------------------------------------------
TEST(PacketSkillShows, TheRocketFliesFromBesideTheUserTimedByTheDistance)
{
	Scene scene;
	scene.Tribe(CREATURETYPE_ROCKET_LUNCHER);
	MCreature* user = scene.Put(500, 10, 10);
	MCreature* target = scene.Put(501, 14, 11);
	user->SetDirection(DIRECTION_UP);
	user->SetCurrentDirection(DIRECTION_UP);
	Add_RocketRuncher(user, target);
	std::vector<MFakeCreature*> fakes = scene.Fakes();
	CHECK(fakes.size() == 1u);
	if (fakes.size() != 1u) return;
	MFakeCreature* rocket = fakes[0];
	CHECK_EQ(int(CREATURETYPE_ROCKET_LUNCHER), int(rocket->GetCreatureType()));
	// The next cell toward the target, the user's way facing.
	CHECK(Cell(rocket->GetX(), rocket->GetY()) == "11,10");
	CHECK_EQ(int(DIRECTION_UP), int(rocket->GetDirection()));
	CHECK_EQ(int(MFakeCreature::FAKE_CREATURE_ROCKET), int(rocket->GetFakeCreatureType()));
	CHECK_EQ(int(MCreature::CREATURE_FAKE_FLYING), int(rocket->GetMoveType()));
	CHECK(rocket->HasEffectStatus(EFFECTSTATUS_ROCKET_LUNCHER));
	CHECK_EQ(501, int(TraceID(rocket)));
	// 250 ms a cell of the farther axis: three cells.
	CHECK(rocket->*(&FakeAccess::m_nextMoveTime) == clockNow + MonotonicClock::Millis(750));
	CHECK(rocket->IsGhost(0xff));
	CHECK_EQ(2, rocket->GetGhost());
	Add_RocketRuncher(NULL, target);
	Add_RocketRuncher(user, NULL);
	CHECK_EQ(1, scene.zone->GetFakeCreatureNumber());
}

TEST(PacketSkillShows, TheWildWolfRunsFromBesideTheUserAtTheTarget)
{
	Scene scene;
	scene.Tribe(CREATURETYPE_WILD_WOLF);
	MCreature* user = scene.Put(500, 10, 10);
	MCreature* target = scene.Put(501, 10, 6);
	user->SetDirection(DIRECTION_LEFT);
	for (bool eat : { true, false })
	{
		Add_Wild_Wolf(user, target, eat);
		std::vector<MFakeCreature*> fakes = scene.Fakes();
		CHECK(fakes.size() == 1u);
		if (fakes.size() != 1u) return;
		MFakeCreature* wolf = fakes[0];
		// The user turns to the target; the wolf starts a cell its way.
		CHECK_EQ(int(DIRECTION_UP), int(user->GetServerDirection()));
		CHECK_EQ(int(CREATURETYPE_WILD_WOLF), int(wolf->GetCreatureType()));
		CHECK(Cell(wolf->GetX(), wolf->GetY()) == "10,9");
		CHECK_EQ(int(DIRECTION_UP), int(wolf->GetDirection()));
		CHECK_EQ(int(MFakeCreature::FAKE_CREATURE_WILD_WOLF), int(wolf->GetFakeCreatureType()));
		CHECK_EQ(int(MCreature::CREATURE_FAKE_FLYING), int(wolf->GetMoveType()));
		CHECK_EQ(501, int(TraceID(wolf)));
		CHECK_EQ(eat, wolf->*(&FakeAccess::m_IsEatCorps));
		CHECK(Cell(wolf->*(&FakeAccess::m_DestX), wolf->*(&FakeAccess::m_DestY)) == "10,6");
		scene.zone->RemoveFakeCreature(wolf->GetID());
	}
	Add_Wild_Wolf(NULL, target, true);
	Add_Wild_Wolf(user, NULL, true);
	CHECK_EQ(0, scene.zone->GetFakeCreatureNumber());
}

TEST(PacketSkillShows, ADragonTornadoIsMadeOnceThenMovedACellAtATime)
{
	Scene scene;
	scene.Tribe(788);
	scene.Tribe(789);
	SetDragonTorando(EFFECTSTATUS_DRAGON_TORNADO, 7001, 8, 9);
	SetDragonTorando(EFFECTSTATUS_DRAGON_TORNADO_CHILD, 7002, 12, 9);
	MCreature* pTornado = scene.zone->GetFakeCreature(7001);
	MCreature* pChild = scene.zone->GetFakeCreature(7002);
	CHECK(pTornado != NULL && pChild != NULL);
	if (pTornado == NULL || pChild == NULL) return;
	CHECK_EQ(788, int(pTornado->GetCreatureType()));
	CHECK_EQ(789, int(pChild->GetCreatureType()));
	CHECK(Cell(pTornado->GetX(), pTornado->GetY()) == "8,9");
	CHECK_EQ(0, int(pTornado->GetDirection()));
	CHECK_EQ(int(MFakeCreature::FAKE_CREATURE_DRAGON_TORNADO), int(static_cast<MFakeCreature*>(pTornado)->GetFakeCreatureType()));
	CHECK_EQ(int(MCreature::CREATURE_FAKE_FLYING), int(pTornado->GetMoveType()));
	CHECK(pTornado->HasEffectStatus(EFFECTSTATUS_DRAGON_TORNADO));
	CHECK(pChild->HasEffectStatus(EFFECTSTATUS_DRAGON_TORNADO_CHILD));
	CHECK_EQ(int(ACTION_STAND), int(pTornado->GetAction()));
	// Told its cell again: nothing; another cell: one step that way.
	SetDragonTorando(EFFECTSTATUS_DRAGON_TORNADO, 7001, 8, 9);
	CHECK_EQ(int(ACTION_STAND), int(pTornado->GetAction()));
	CHECK_EQ(0, int(pTornado->GetServerDirection()));
	SetDragonTorando(EFFECTSTATUS_DRAGON_TORNADO, 7001, 8, 5);
	CHECK_EQ(2, scene.zone->GetFakeCreatureNumber());
	CHECK_EQ(int(DIRECTION_UP), int(pTornado->GetServerDirection()));
	CHECK_EQ(int(DIRECTION_UP), int(pTornado->GetDirection()));
	CHECK(pTornado->GetAction() == pTornado->*(&CreatureAccess::m_MoveAction));
	// Without a zone: nothing.
	g_pZone = NULL;
	SetDragonTorando(EFFECTSTATUS_DRAGON_TORNADO, 7003, 8, 9);
	g_pZone = scene.zone.get();
	CHECK_EQ(2, scene.zone->GetFakeCreatureNumber());
}

TEST(PacketSkillShows, AnElementalFliesBesideItsOwnerTracingIt)
{
	Scene scene;
	scene.Tribe(650, CREATURETRIBE_OUSTERS);
	// At the zone's corner only the cell diagonal to it is free of the
	// owner's row and column.
	MCreature* owner = scene.Put(500, 0, 0, Ousters);
	AddOustersElemental(owner, 650, EFFECTSTATUS_SUMMON_SYLPH_GREEN);
	std::vector<MFakeCreature*> fakes = scene.Fakes();
	CHECK(fakes.size() == 1u);
	if (fakes.size() != 1u) return;
	MFakeCreature* elemental = fakes[0];
	CHECK_EQ(650, int(elemental->GetCreatureType()));
	CHECK(Cell(elemental->GetX(), elemental->GetY()) == "1,1");
	CHECK(elemental->GetDirection() < 8);
	CHECK_EQ(int(elemental->GetID()), int(owner->GetElementalID()));
	CHECK_EQ(500, int(elemental->GetOwnerID()));
	CHECK_EQ(int(MFakeCreature::FAKE_CREATURE_TRACE), int(elemental->GetFakeCreatureType()));
	CHECK_EQ(int(MCreature::CREATURE_FAKE_FLYING), int(elemental->GetMoveType()));
	CHECK(Cell(elemental->GetFakeX(), elemental->GetFakeY()) == "0,0");
	CHECK_EQ(500, int(TraceID(elemental)));
	CHECK(elemental->HasEffectStatus(EFFECTSTATUS_SUMMON_SYLPH_GREEN));
	// One elemental an owner; none without an owner.
	AddOustersElemental(owner, 650, EFFECTSTATUS_SUMMON_SYLPH_GREEN);
	AddOustersElemental(NULL, 650, EFFECTSTATUS_SUMMON_SYLPH_GREEN);
	CHECK_EQ(1, scene.zone->GetFakeCreatureNumber());
}
