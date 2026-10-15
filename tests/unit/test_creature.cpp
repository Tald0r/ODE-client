#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "MCreature.h"
#include "MCreatureTable.h"
#include "MCreatureSpriteTable.h"
#include "MActionInfoTable.h"
#include "SkillDef.h"
#include "MSector.h"
#include "MonsterNameTable.h"
#include "MLevelNameTable.h"
#include "MEffectSpriteTypeTable.h"
#include "MEffectStatusTable.h"
#include "EffectSpriteTypeDef.h"
#include "MEffectStatusDef.h"
#include "AdvancementAction.h"
#include "AddonDef.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "WorldEventsHost.h"
#include "StatusPresentationHost.h"
#include "DialogPresentationHost.h"
#include "VsUiPresentationHost.h"
#include "GameOptionsHost.h"
#include "MEventManager.h"
#include "MEventQueue.h"
#include "SoundDef.h"
#include "FrameClock.h"
#include "ZoneFlags.h"
#include "MAttachEffect.h"
#include "MViewDef.h"
#include "DirectionSelection.h"
#include "WorldTileGeometry.h"
#include "MEffect.h"
#include "MParty.h"
#include "MEffectGeneratorTable.h"
#include "MEffectGenerator.h"
#include "MEffectTarget.h"
#include "RankBonusTable.h"
#include "domain/Formulas.h"

#include <cstdio>
#include <cstring>
#include <new>
#include <map>
#include <memory>
#include <string>
#include <vector>

BYTE GetCreatureActionCountMax( const MCreature* pCreature, int action );
extern bool g_bInterpolateDraw;	// MObjectScreen.cpp's

// The creature (task 4.242, gamemodel's since then): what it asks of the
// zone, the player, the view, the windows and the options, through hosts
// that record each call in order.
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

bool zoneExists = true;
int zoneID = 1;
int zoneWidth = 100;
int zoneHeight = 100;
MSector sector;
// Sector reads are kept apart: placing a creature reads its sector.
std::vector<std::string> sectorReads;
std::map<TYPE_OBJECTID, MCreature*> zoneCreatures;
std::vector<const MEffect*> groundEffects;
bool playerGhost = false;
bool moveOK = true;
bool chatWhite = false;
bool bloodDrop = false;
std::uint32_t chattingColor = 0x00123456;
// The creature's own zone: an address the fakes only compare.
int zoneMarker;
MZone* OwnZone() { return reinterpret_cast<MZone*>(&zoneMarker); }
std::string Z(MZone* pZone) { return pZone == OwnZone() ? "own" : "other"; }
MCreature* player = nullptr;
MCreature* fakeInZone = nullptr;
bool viewExists = true;
int maxEffectFrame = 7;
ITEM_CLASS rightHand = ITEM_CLASS_NULL;
bool handHolds = false;

const ZoneState::Host zoneHost = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetID = []() { return zoneID; };
	host.GetWidth = []() { return zoneWidth; };
	host.GetHeight = []() { return zoneHeight; };
	host.GetSector = [](TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) -> const MSector* {
		sectorReads.push_back(N(x) + "," + N(y)); return &sector; };
	host.GetGroundEffects = [](std::vector<const MEffect*>& effects) { effects = groundEffects; };
	host.GetCreature = [](TYPE_OBJECTID id) -> MCreature* {
		calls.push_back("creature|" + N(id));
		auto found = zoneCreatures.find(id);
		return found == zoneCreatures.end() ? nullptr : found->second; };
	host.RemoveFakeCreature = [](TYPE_OBJECTID id) { calls.push_back("remove fake|" + N(id)); return true; };
	host.GetFakeCreature = [](TYPE_OBJECTID id) { calls.push_back("fake|" + N(id)); return fakeInZone; };
	host.SetServerBlockIn = [](MZone* pZone, int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("block|" + Z(pZone) + "|" + N(moveType) + "|" + N(x) + "," + N(y)); };
	host.UnSetServerBlockIn = [](MZone* pZone, int moveType, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("unblock|" + Z(pZone) + "|" + N(moveType) + "|" + N(x) + "," + N(y)); };
	host.MoveGroundCreatureIn = [](MZone* pZone, MCreature* pCreature, TYPE_SECTORPOSITION xo, TYPE_SECTORPOSITION yo, TYPE_SECTORPOSITION xn, TYPE_SECTORPOSITION yn) {
		calls.push_back("move ground|" + Z(pZone) + "|" + N(pCreature->GetID()) + "|" + N(xo) + "," + N(yo) + "|" + N(xn) + "," + N(yn)); return moveOK; };
	host.MoveFlyingCreatureIn = [](MZone* pZone, MCreature* pCreature, TYPE_SECTORPOSITION xo, TYPE_SECTORPOSITION yo, TYPE_SECTORPOSITION xn, TYPE_SECTORPOSITION yn) {
		calls.push_back("move flying|" + Z(pZone) + "|" + N(pCreature->GetID()) + "|" + N(xo) + "," + N(yo) + "|" + N(xn) + "," + N(yn)); return moveOK; };
	host.MoveUndergroundCreatureIn = [](MZone* pZone, MCreature* pCreature, TYPE_SECTORPOSITION xo, TYPE_SECTORPOSITION yo, TYPE_SECTORPOSITION xn, TYPE_SECTORPOSITION yn) {
		calls.push_back("move underground|" + Z(pZone) + "|" + N(pCreature->GetID()) + "|" + N(xo) + "," + N(yo) + "|" + N(xn) + "," + N(yn)); return moveOK; };
	return host;
}();
const GameOptions::Host optionsHost = []() {
	GameOptions::Host host{};
	host.ChatWhite = []() { return chatWhite; };
	host.BloodDrop = []() { return bloodDrop; };
	host.ChattingColor = []() { return chattingColor; };
	return host;
}();
TYPE_OBJECTID playerID = 0;
const PlayerState::Host playerHost = []() {
	PlayerState::Host host{};
	host.Creature = []() { return player; };
	host.GetID = []() { return playerID; };
	host.HasEffectStatus = [](EFFECTSTATUS status) { return status == EFFECTSTATUS_GHOST && playerGhost; };
	host.SetDelay = [](std::uint32_t milliseconds) { calls.push_back("delay|" + N(milliseconds)); };
	return host;
}();
const VsUiPresentation::Host uiHost = []() {
	VsUiPresentation::Host host{};
	host.LockGear = []() { calls.push_back("lock gear"); };
	return host;
}();
const DialogPresentation::Host dialogHost = []() {
	DialogPresentation::Host host{};
	host.UnlockGear = []() { calls.push_back("unlock gear"); };
	return host;
}();
const CreatureState::Host creatureHost = []() {
	CreatureState::Host host{};
	host.GetAddonItemClass = [](MCreature*, int addon, ITEM_CLASS* pItemClass) {
		calls.push_back("addon|" + N(addon));
		*pItemClass = rightHand;
		return handHolds;
	};
	host.SyncTurretDirection = [](MCreature* pCreature) { calls.push_back("turret|" + N(pCreature->GetID())); };
	return host;
}();
const WorldEvents::Host worldHost = []() {
	WorldEvents::Host host{};
	host.ViewExists = []() { return viewExists; };
	host.GetMaxEffectFrame = [](int bltType, std::uint16_t frameID) { calls.push_back("max frame|" + N(bltType) + "|" + N(frameID)); return maxEffectFrame; };
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int startZ, int direction, TYPE_OBJECTID targetID,
		TYPE_SECTORPOSITION targetX, TYPE_SECTORPOSITION targetY, int targetZ, std::uint32_t delayFrame, MActionResult* pResult, bool bStartFromMainNode, int sX, int sY) {
		calls.push_back("skill|" + N(actionInfo) + "|" + N(startX) + "," + N(startY) + "," + N(startZ) + "|" + N(direction) + "|" + N(targetID)
			+ "|" + N(targetX) + "," + N(targetY) + "," + N(targetZ) + "|" + N(delayFrame) + "|" + (pResult != NULL ? "result" : "none")
			+ "|" + (bStartFromMainNode ? "main" : "first") + "|" + N(sX) + "," + N(sY));
	};
	host.AddOustersElemental = [](MCreature* pOwner, int creatureType, int status) { calls.push_back("elemental|" + N(pOwner->GetID()) + "|" + N(creatureType) + "|" + N(status)); };
	host.PlaySound = [](TYPE_SOUNDID soundID) { calls.push_back("sound|" + N(soundID)); };
	host.PlaySoundAt = [](TYPE_SOUNDID soundID, bool repeat, int x, int y) { calls.push_back("sound at|" + N(soundID) + "|" + N(repeat) + "|" + N(x) + "," + N(y)); };
	return host;
}();
// Effects count their frames on the game's clock, as GameInit's host does.
const MEffectHost effectHost = []() {
	MEffectHost host{};
	host.CurrentFrame = []() -> DWORD { return g_CurrentFrame; };
	return host;
}();
// An attached effect finds its creature in the zone, as GameInit's host
// does: every creature here is.
const MAttachEffectHost attachHost = []() {
	MAttachEffectHost host{};
	host.LiveCreature = [](TYPE_OBJECTID, MAttachCreaturePosition&) { return true; };
	host.CreaturePosition = [](const MCreature*, MAttachCreaturePosition&) { return true; };
	return host;
}();
const StatusPresentation::Host statusHost = []() {
	StatusPresentation::Host host{};
	host.SetWeaponSpeed = [](int speed, int speedClass) { calls.push_back("weapon speed|" + N(speed) + "|" + N(speedClass)); };
	return host;
}();

// The effect generator every test action uses: it records where an effect
// starts and what it aims at.
struct FakeGenerator : MEffectGenerator
{
	TYPE_EFFECTGENERATORID GetID() override { return 7; }
	bool Generate(const EFFECTGENERATOR_INFO& info) override
	{
		calls.push_back("effect|" + N(info.nActionInfo) + "|" + N(info.x0) + "," + N(info.y0) + "|" + N(info.x1) + "," + N(info.y1)
			+ "|" + N(info.creatureID));
		return false;
	}
};
struct Generators : MEffectGeneratorTable
{
	void Add(MEffectGenerator* generator) { AddEffectGenerator(generator); }
};

// The player's own creature class, and a fake one (a pet, an elemental).
struct PlayerCreature : MCreature
{
	CLASS_TYPE GetClassType() const override { return CLASS_PLAYER; }
};
struct FakeCreature : MCreature
{
	CLASS_TYPE GetClassType() const override { return CLASS_FAKE; }
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
	RankBonusTable* oldRankBonuses = g_pRankBonusTable;
	MonsterNameTable monsterNames;
	MLevelNameTable levelNames;
	MonsterNameTable* oldMonsterNames = g_pMonsterNameTable;
	MLevelNameTable* oldLevelNames = g_pLevelNameTable;
	EFFECTSPRITETYPE_TABLE* oldEffectSprites = g_pEffectSpriteTypeTable;
	EFFECTSTATUS_TABLE* oldEffectStatuses = g_pEffectStatusTable;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zoneHost);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creatureHost);
	const WorldEvents::Host* oldWorld = WorldEvents::SetHost(&worldHost);
	const StatusPresentation::Host* oldStatus = StatusPresentation::SetHost(&statusHost);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&playerHost);
	const GameOptions::Host* oldOptions = GameOptions::SetHost(&optionsHost);
	const VsUiPresentation::Host* oldUi = VsUiPresentation::SetHost(&uiHost);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialogHost);
	MEventQueue events;
	MEventQueue* oldEvents = g_pEventManager;
	Generators generators;
	MEffectGeneratorTable* oldGenerators = g_pEffectGeneratorTable;
	const MEffectHost* oldEffectHost = MEffect::SetHost(&effectHost);
	const MAttachEffectHost* oldAttachHost = MAttachEffect::SetHost(&attachHost);

	Scene()
	{
		calls.clear();
		zoneExists = true;
		zoneID = 1;
		zoneWidth = 100;
		zoneHeight = 100;
		// MSector's assignment copies no flags: clear them one by one.
		sector.UnSetDarkness();
		sector.UnSetGroundElemental();
		sector.UnSetFuryOfGnome();
		sectorReads.clear();
		zoneCreatures.clear();
		groundEffects.clear();
		playerGhost = false;
		player = nullptr;
		fakeInZone = nullptr;
		moveOK = true;
		chatWhite = false;
		chattingColor = 0x00123456;
		bloodDrop = false;
		playerID = 0;
		g_pEventManager = &events;
		generators.Add(new FakeGenerator);
		g_pEffectGeneratorTable = &generators;
		viewExists = true;
		maxEffectFrame = 7;
		rightHand = ITEM_CLASS_NULL;
		handHolds = false;
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
		g_pMonsterNameTable = &monsterNames;
		g_pRankBonusTable = &rankBonuses;
		g_pLevelNameTable = &levelNames;
	}
	~Scene()
	{
		MAttachEffect::SetHost(oldAttachHost);
		MEffect::SetHost(oldEffectHost);
		g_pEffectGeneratorTable = oldGenerators;
		g_pEventManager = oldEvents;
		DialogPresentation::SetHost(oldDialog);
		VsUiPresentation::SetHost(oldUi);
		GameOptions::SetHost(oldOptions);
		PlayerState::SetHost(oldPlayer);
		StatusPresentation::SetHost(oldStatus);
		WorldEvents::SetHost(oldWorld);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		g_pRankBonusTable = oldRankBonuses;
		g_pLevelNameTable = oldLevelNames;
		g_pMonsterNameTable = oldMonsterNames;
		g_pEffectStatusTable = oldEffectStatuses;
		g_pEffectSpriteTypeTable = oldEffectSprites;
		g_pActionInfoTable = oldActions;
		g_pCreatureSpriteTable = oldSprites;
		g_pCreatureTable = oldCreatureTypes;
	}
	// A creature type of the tribe, with a count for each action.
	void Tribe(int type, CREATURETRIBE tribe)
	{
		testfw::MutableRow(creatureTypes, type).SetCreatureTribe(tribe, ACTION_MAX_SLAYER);
		for (int action = 0; action < ACTION_MAX_SLAYER; ++action)
			testfw::MutableRow(creatureTypes, type).SetActionCount(action, 10 + action);
	}
};

// Whether the first call is this one, printing the calls when not.
bool FirstIs(const std::string& expected)
{
	if (!calls.empty() && calls[0] == expected) return true;
	std::printf("      calls: ");
	for (const std::string& call : calls) std::printf("[%s] ", call.c_str());
	std::printf("\n      wanted first: [%s]\n", expected.c_str());
	return false;
}

} // namespace

//----------------------------------------------------------------------
// Leaving
//----------------------------------------------------------------------
TEST(Creature, ItsPetAndElementalLeaveTheZoneWithIt)
{
	Scene scene;
	{
		std::unique_ptr<MCreature> pCreature(new MCreature);
		pCreature->SetPetID(500);
		pCreature->SetElementalID(501);
	}
	CHECK(Same({"remove fake|500", "remove fake|501"}));
	// Without either, or without a zone, none.
	calls.clear();
	{
		std::unique_ptr<MCreature> pCreature(new MCreature);
	}
	zoneExists = false;
	{
		std::unique_ptr<MCreature> pCreature(new MCreature);
		pCreature->SetPetID(500);
		pCreature->SetElementalID(501);
	}
	CHECK(Same({}));
}

//----------------------------------------------------------------------
// An advanced creature's action counts
//----------------------------------------------------------------------
TEST(Creature, APlainCreatureCountsFromItsTable)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	creature.SetCreatureType(5);
	CHECK_EQ(13, GetCreatureActionCountMax(&creature, 3));
	// Advanced, but without a view: still the table.
	creature.SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 2);
	viewExists = false;
	CHECK_EQ(13, GetCreatureActionCountMax(&creature, 3));
	CHECK(Same({}));
}

TEST(Creature, AnAdvancedSlayerCountsByItsRightHand)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	creature.SetCreatureType(5);
	creature.SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 2);
	g_AdvanceSlayerActionMaxCount[ACTION_ADVANCEMENT_SLAYER_STOP_BLADE - ADVANCEMENT_ACTION_START] = 21;
	g_AdvanceSlayerActionMaxCount[ACTION_ADVANCEMENT_SLAYER_STOP_SWORD - ADVANCEMENT_ACTION_START] = 22;
	rightHand = ITEM_CLASS_BLADE;
	CHECK_EQ(21, GetCreatureActionCountMax(&creature, ACTION_STAND));
	rightHand = ITEM_CLASS_SWORD;
	CHECK_EQ(22, GetCreatureActionCountMax(&creature, ACTION_STAND));
	CHECK(Same({"addon|" + N(ADDON_RIGHTHAND), "addon|" + N(ADDON_RIGHTHAND)}));
	// An action with no advanced form counts none.
	CHECK_EQ(0, GetCreatureActionCountMax(&creature, 2));
	g_AdvanceSlayerActionMaxCount[ACTION_ADVANCEMENT_SLAYER_STOP_BLADE - ADVANCEMENT_ACTION_START] = 0;
	g_AdvanceSlayerActionMaxCount[ACTION_ADVANCEMENT_SLAYER_STOP_SWORD - ADVANCEMENT_ACTION_START] = 0;
}

TEST(Creature, AnAdvancedOustersCountsByAHeldChakram)
{
	Scene scene;
	scene.Tribe(6, CREATURETRIBE_OUSTERS);
	MCreature creature;
	creature.SetCreatureType(6);
	creature.SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 2);
	g_AdvanceOustersActionMaxCount[ACTION_ADVANCEMENT_OUSTERS_CHAKRAM_STOP - ADVANCEMENT_ACTION_START] = 31;
	g_AdvanceOustersActionMaxCount[ACTION_ADVANCEMENT_OUSTERS_WRISTLET_STOP - ADVANCEMENT_ACTION_START] = 32;
	rightHand = ITEM_CLASS_OUSTERS_CHAKRAM;
	handHolds = true;
	CHECK_EQ(31, GetCreatureActionCountMax(&creature, ACTION_STAND));
	// A chakram's class with nothing in the hand, or another item: no chakram.
	handHolds = false;
	CHECK_EQ(32, GetCreatureActionCountMax(&creature, ACTION_STAND));
	handHolds = true;
	rightHand = ITEM_CLASS_OUSTERS_WRISTLET;
	CHECK_EQ(32, GetCreatureActionCountMax(&creature, ACTION_STAND));
	g_AdvanceOustersActionMaxCount[ACTION_ADVANCEMENT_OUSTERS_CHAKRAM_STOP - ADVANCEMENT_ACTION_START] = 0;
	g_AdvanceOustersActionMaxCount[ACTION_ADVANCEMENT_OUSTERS_WRISTLET_STOP - ADVANCEMENT_ACTION_START] = 0;
}

TEST(Creature, AnAdvancedVampireCountsWithoutAHand)
{
	Scene scene;
	scene.Tribe(7, CREATURETRIBE_VAMPIRE);
	MCreature creature;
	creature.SetCreatureType(7);
	creature.SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 2);
	g_AdvanceVampireActionMaxCount[ACTION_ADVANCEMENT_MAGIC - ADVANCEMENT_ACTION_START] = 41;
	CHECK_EQ(41, GetCreatureActionCountMax(&creature, 3));
	CHECK(Same({}));
	g_AdvanceVampireActionMaxCount[ACTION_ADVANCEMENT_MAGIC - ADVANCEMENT_ACTION_START] = 0;
}

//----------------------------------------------------------------------
// Weapon speed
//----------------------------------------------------------------------
TEST(Creature, ThePlayersWeaponSpeedIsShown)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	PlayerCreature player;
	player.SetCreatureType(5);
	player.SetWeaponSpeed(70);
	CHECK(Same({"weapon speed|70|" + N(MCreature::WEAPON_SPEED_FAST)}));
	// Another creature's is not.
	calls.clear();
	MCreature creature;
	creature.SetCreatureType(5);
	creature.SetWeaponSpeed(70);
	CHECK(Same({}));
	CHECK_EQ(MCreature::WEAPON_SPEED_FAST, creature.GetWeaponSpeed());
}

//----------------------------------------------------------------------
// Effect statuses
//----------------------------------------------------------------------
namespace {
// A creature at (12, 34), ID 900, of a slayer type, turning to 3 and
// facing 5 now (GetDirection is the one it faces now).
void Place(MCreature& creature, int type = 5)
{
	creature.SetCreatureType(type);
	creature.SetID(900);
	creature.SetPosition(12, 34);
	creature.SetDirection(3);
	creature.SetCurrentDirection(5);
}
std::string Skill(int actionInfo, long long delay, TYPE_OBJECTID id = 900, int x = 12, int y = 34, int direction = 5)
{
	return "skill|" + N(actionInfo) + "|" + N(x) + "," + N(y) + ",0|" + N(direction) + "|" + N(id) + "|" + N(x) + "," + N(y) + ",0|"
		+ N(delay) + "|none|first|0,0";
}
} // namespace

TEST(Creature, StatusesStartTheirSkillOnTheCreature)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	const std::pair<EFFECTSTATUS, int> skills[] = {
		{EFFECTSTATUS_BLINDNESS, SKILL_CLIENT_BLINDNESS},
		{EFFECTSTATUS_FIERCE_FLAME, SKILL_CLIENT_FIERCE_FLAME},
		{EFFECTSTATUS_DUMMY_DRAKE, SKILL_CLIENT_DUMMY_DRAKE},
		{EFFECTSTATUS_HYDRO_CONVERGENCE, SKILL_CLIENT_HYDRO_CONVERGENCE},
		{EFFECTSTATUS_HETER_CHAKRAM, SKILL_CLIENT_SKILL_DUMMY_DRAKE},
		{EFFECTSTATUS_BLOOD_CURSE, SKILL_CLIENT_BLOOD_CURSE},
		{EFFECTSTATUS_DONATION_200501, SKILL_CLIENT_CANDLE},
	};
	for (const auto& row : skills)
	{
		calls.clear();
		MCreature creature;
		Place(creature);
		creature.AddEffectStatus(row.first, 40);
		CHECK(FirstIs(Skill(row.second, 40)));
	}
	// Reputo factum's hit runs eight frames, whatever it was given.
	calls.clear();
	MCreature creature;
	Place(creature);
	creature.AddEffectStatus(EFFECTSTATUS_REPUTO_FACTUM, 40);
	CHECK(FirstIs(Skill(SKILL_CLIENT_HIT_REPUTO_FACTUM, 8)));
}

TEST(Creature, AGlacierFreezesEachRaceItsOwnWay)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	scene.Tribe(7, CREATURETRIBE_OUSTERS);
	const std::pair<int, int> races[] = {{5, SKILL_CLIENT_GDR_GLACIER_SLAYER}, {6, SKILL_CLIENT_GDR_GLACIER_VAMPIRE}, {7, SKILL_CLIENT_GDR_GLACIER_OUSTERS}};
	for (const auto& race : races)
	{
		calls.clear();
		MCreature creature;
		Place(creature, race.first);
		creature.AddEffectStatus(EFFECTSTATUS_GLACIER, 50);
		// Twenty frames shorter, never below 0.
		CHECK(FirstIs(Skill(race.second, 30)));
		// Twenty frames or fewer: none left.
		for (DWORD frames : {DWORD(10), DWORD(20)})
		{
			calls.clear();
			MCreature short_;
			Place(short_, race.first);
			short_.AddEffectStatus(EFFECTSTATUS_GLACIER, frames);
			CHECK(FirstIs(Skill(race.second, 0)));
		}
		calls.clear();
		MCreature longer;
		Place(longer, race.first);
		longer.AddEffectStatus(EFFECTSTATUS_GLACIER, 21);
		CHECK(FirstIs(Skill(race.second, 1)));
	}
}

TEST(Creature, AnAdvancedSylphCallsItsSkill)
{
	Scene scene;
	scene.Tribe(7, CREATURETRIBE_OUSTERS);
	MCreature creature;
	Place(creature, 7);
	creature.AddEffectStatus(EFFECTSTATUS_SUMMON_SYLPH, 40);
	CHECK(calls.empty() || calls[0].rfind("skill|", 0) != 0);
	calls.clear();
	MCreature advanced;
	Place(advanced, 7);
	advanced.SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
	advanced.AddEffectStatus(EFFECTSTATUS_SUMMON_SYLPH, 40);
	// After the sylph's frame count, which asks for its hand.
	CHECK(calls.size() >= 3 && calls[2] == Skill(SKILL_CLIENT_ADVANCEMENT_SUMMON_SYLPH_START, 8));
	// A coloured sylph does not.
	calls.clear();
	MCreature coloured;
	Place(coloured, 7);
	coloured.SetStatus(MODIFY_ADVANCEMENT_CLASS_LEVEL, 1);
	coloured.AddEffectStatus(EFFECTSTATUS_SUMMON_SYLPH_RED, 40);
	CHECK(calls.empty() || calls[0].rfind("skill|", 0) != 0);
	// Its end has a skill too, for the advanced plain sylph only (after
	// the hand lookups of its walk's frame count).
	calls.clear();
	advanced.RemoveEffectStatus(EFFECTSTATUS_SUMMON_SYLPH);
	CHECK(!calls.empty() && calls.back() == Skill(SKILL_CLIENT_ADVANCEMENT_SUMMON_SYLPH_END, 8));
	calls.clear();
	creature.RemoveEffectStatus(EFFECTSTATUS_SUMMON_SYLPH);
	coloured.RemoveEffectStatus(EFFECTSTATUS_SUMMON_SYLPH_RED);
	for (const std::string& call : calls)
		CHECK(call.rfind("skill|", 0) != 0);
}

TEST(Creature, ElementalsComeFromTheOwnerOrActOnTheirOwn)
{
	Scene scene;
	scene.Tribe(7, CREATURETRIBE_OUSTERS);
	for (EFFECTSTATUS status : {EFFECTSTATUS_FIRE_ELEMENTAL, EFFECTSTATUS_WATER_ELEMENTAL})
	{
		calls.clear();
		MCreature owner;
		Place(owner, 7);
		owner.AddEffectStatus(status, 40);
		CHECK(FirstIs("elemental|900|699|" + N(status)));
		// The elemental itself shows it orbiting.
		calls.clear();
		FakeCreature elemental;
		Place(elemental, 7);
		elemental.AddEffectStatus(status, 40);
		CHECK(Same({}));
	}
}

TEST(Creature, AnElementalsActsArePassedToIt)
{
	Scene scene;
	scene.Tribe(7, CREATURETRIBE_OUSTERS);
	const std::pair<EFFECTSTATUS, int> acts[] = {
		{EFFECTSTATUS_FIRE_ELEMENTAL_ATTACK, SKILL_CLIENT_FIRE_ELEMENTAL_ATTACK},
		{EFFECTSTATUS_WATER_ELEMENTAL_HEAL, SKILL_CLIENT_WATER_ELEMENTAL_HEAL},
	};
	for (const auto& act : acts)
	{
		// The elemental casts it, from the main node, for good.
		calls.clear();
		FakeCreature elemental;
		Place(elemental, 7);
		elemental.SetID(901);
		elemental.AddEffectStatus(act.first, 40);
		CHECK(FirstIs("skill|" + N(act.second) + "|12,34,0|3|901|12,34,0|65535|none|first|0,0"));
		// Its owner hands it to the elemental in the zone and keeps none.
		calls.clear();
		MCreature owner;
		Place(owner, 7);
		owner.SetElementalID(901);
		fakeInZone = &elemental;
		CHECK(!owner.AddEffectStatus(act.first, 40));
		CHECK(calls.size() >= 2 && calls[0] == "fake|901" && calls[1] == "skill|" + N(act.second) + "|12,34,0|3|901|12,34,0|65535|none|first|0,0");
		CHECK(!owner.HasEffectStatus(act.first));
		CHECK(elemental.HasEffectStatus(act.first));
		// No elemental in the zone: nothing.
		calls.clear();
		fakeInZone = nullptr;
		MCreature alone;
		Place(alone, 7);
		alone.SetElementalID(902);
		CHECK(!alone.AddEffectStatus(act.first, 40));
		CHECK(Same({"fake|902"}));
		owner.SetElementalID(OBJECTID_NULL);
		alone.SetElementalID(OBJECTID_NULL);
	}
}

TEST(Creature, AnElementalsHitLastsItsSpritesFrames)
{
	Scene scene;
	scene.Tribe(7, CREATURETRIBE_OUSTERS);
	for (EFFECTSTATUS status : {EFFECTSTATUS_FIRE_ELEMENTAL_DAMAGED, EFFECTSTATUS_WATER_ELEMENTAL_HEALED})
	{
		calls.clear();
		scene.effectStatuses.GetMutable(status)->EffectSpriteType = 20;
		scene.effectSprites.GetMutable(20)->BltType = BLT_SCREEN;
		scene.effectSprites.GetMutable(20)->FrameID = 33;
		MCreature creature;
		Place(creature, 7);
		creature.AddEffectStatus(status, 40);
		CHECK(FirstIs("max frame|" + N(BLT_SCREEN) + "|33"));
		// A sprite type past the table: no frames asked.
		calls.clear();
		scene.effectStatuses.GetMutable(status)->EffectSpriteType = MAX_EFFECTSPRITETYPE;
		MCreature past;
		Place(past, 7);
		past.AddEffectStatus(status, 40);
		CHECK(calls.empty() || calls[0].rfind("max frame", 0) != 0);
	}
}

TEST(Creature, ALevelUpPlaysItsSound)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	for (EFFECTSTATUS status : {EFFECTSTATUS_LEVELUP_SLAYER, EFFECTSTATUS_LEVELUP_VAMPIRE})
	{
		calls.clear();
		MCreature creature;
		Place(creature);
		creature.AddEffectStatus(status, 40);
		CHECK(FirstIs("sound|" + N(SOUND_SLAYER_ENCHANT_PFA)));
	}
}

TEST(Creature, OnlyThePlayersHallucinationFadesTheScreen)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	g_pClientConfig->FPS = 16;
	MCreature creature;
	Place(creature);
	creature.AddEffectStatus(EFFECTSTATUS_HALLUCINATION, 160);
	CHECK(scene.events.GetEvent(EVENTID_HALLUCINATION) == nullptr);
	MCreature me;
	Place(me);
	player = &me;
	me.AddEffectStatus(EFFECTSTATUS_HALLUCINATION, 160);
	const MEvent* pEvent = scene.events.GetEvent(EVENTID_HALLUCINATION);
	CHECK(pEvent != nullptr);
	if (pEvent != nullptr)
	{
		CHECK_EQ(EVENTTYPE_EFFECT, pEvent->eventType);
		CHECK_EQ(EVENTFLAG_FADE_SCREEN, pEvent->eventFlag);
		CHECK_EQ(10000u, pEvent->eventDelay);
		CHECK_EQ(EFFECTSTATUS_HALLUCINATION, int(pEvent->parameter1));
		CHECK_EQ((30 << 16) | 30, int(pEvent->parameter2));
	}
	player = nullptr;
}

//----------------------------------------------------------------------
// Coming back, dying and lying dead
//----------------------------------------------------------------------
TEST(Creature, ComingToAgainResurrectsAVampireByItsBlood)
{
	Scene scene;
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature vampire;
	Place(vampire, 6);
	vampire.AddEffectStatus(EFFECTSTATUS_COMA, 40);
	calls.clear();
	vampire.RemoveEffectStatus(EFFECTSTATUS_COMA);
	// It turns the way it moves (3), as the old code did.
	CHECK(FirstIs("skill|" + N(BLOOD_RESURRECT) + "|12,34,0|3|900|12,34,0|32|none|first|0,0"));
	calls.clear();
	MCreature slayer;
	Place(slayer, 5);
	slayer.AddEffectStatus(EFFECTSTATUS_COMA, 40);
	calls.clear();
	slayer.RemoveEffectStatus(EFFECTSTATUS_COMA);
	CHECK(calls.empty() || calls[0].rfind("skill|", 0) != 0);
}

TEST(Creature, ATurretPackedUpFiresItsLastResult)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	creature.AddEffectStatus(EFFECTSTATUS_INSTALL_TURRET, 40);
	calls.clear();
	creature.RemoveEffectStatus(EFFECTSTATUS_INSTALL_TURRET);
	CHECK(FirstIs("skill|" + N(RESULT_SKILL_INSTALL_TURRET) + "|12,34,0|0|900|12,34,0|0|none|first|0,0"));
	CHECK_EQ(2, creature.GetDirection());
}

TEST(Creature, DyingFreesItsServerBlockAndLandsWhereTheServerSays)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	creature.SetZone(OwnZone());
	creature.SetServerPosition(20, 21);
	calls.clear();
	creature.SetDead();
	// The death frees the block, and so does landing (it frees the block of
	// the move type it had).
	const std::string unblock = "unblock|own|" + N(MCreature::CREATURE_GROUND) + "|20,21";
	CHECK(Same({unblock, unblock, "move ground|own|900|12,34|20,21"}));
	CHECK(!creature.IsAlive());
	// A flier frees a flier's blocks and lands.
	calls.clear();
	MCreature flier;
	Place(flier);
	flier.SetZone(OwnZone());
	flier.SetMoveType(MCreature::CREATURE_FLYING);
	flier.SetServerPosition(20, 21);
	calls.clear();
	flier.SetDead();
	const std::string unblockFlier = "unblock|own|" + N(MCreature::CREATURE_FLYING) + "|20,21";
	CHECK(Same({unblockFlier, unblockFlier, "move ground|own|900|12,34|20,21"}));
	CHECK_EQ(MCreature::CREATURE_GROUND, flier.GetMoveType());
}

TEST(Creature, AWakenedGillesDeRaisDiesWithItsOwnSkill)
{
	Scene scene;
	scene.Tribe(723, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature, 723);
	creature.SetZone(OwnZone());
	creature.SetDead();
	CHECK(FirstIs(Skill(SKILL_CLIENT_GDR_DEAD, 20)));
}

TEST(Creature, ACorpseFreesItsServerBlock)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	creature.SetZone(OwnZone());
	creature.SetServerPosition(20, 21);
	calls.clear();
	creature.SetCorpse();
	// Its own, then landing's (SetGroundCreature).
	const std::string unblock = "unblock|own|" + N(MCreature::CREATURE_GROUND) + "|20,21";
	CHECK(Same({unblock, unblock}));
}

TEST(Creature, ChangingItsMoveTypeFreesTheOldBlock)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	creature.SetZone(OwnZone());
	creature.SetServerPosition(20, 21);
	calls.clear();
	creature.SetFlyingCreature();
	creature.SetUndergroundCreature();
	creature.SetGroundCreature();
	CHECK(Same({"unblock|own|" + N(MCreature::CREATURE_GROUND) + "|20,21",
		"unblock|own|" + N(MCreature::CREATURE_FLYING) + "|20,21",
		"unblock|own|" + N(MCreature::CREATURE_UNDERGROUND) + "|20,21"}));
	// Without a zone, none.
	MCreature lost;
	Place(lost);
	calls.clear();
	lost.SetFlyingCreature();
	lost.SetUndergroundCreature();
	lost.SetGroundCreature();
	CHECK(Same({}));
}

//----------------------------------------------------------------------
// Moving
//----------------------------------------------------------------------
TEST(Creature, AMoveGoesThroughItsZoneByItsMoveType)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	const std::pair<MCreature::MOVE_TYPE, std::string> types[] = {
		{MCreature::CREATURE_GROUND, "move ground"},
		{MCreature::CREATURE_FLYING, "move flying"},
		{MCreature::CREATURE_UNDERGROUND, "move underground"},
	};
	for (const auto& type : types)
	{
		calls.clear();
		MCreature creature;
		Place(creature);
		creature.SetZone(OwnZone());
		creature.SetMoveType(type.first);
		moveOK = true;
		CHECK(creature.MovePosition(13, 35));
		CHECK(FirstIs(type.second + "|own|900|12,34|13,35"));
		CHECK_EQ(13, creature.GetX());
		CHECK_EQ(35, creature.GetY());
		// Refused: it stays.
		calls.clear();
		moveOK = false;
		CHECK(!creature.MovePosition(14, 36));
		CHECK(FirstIs(type.second + "|own|900|13,35|14,36"));
		CHECK_EQ(13, creature.GetX());
	}
	// A fake moves without asking; without a zone nothing moves.
	calls.clear();
	MCreature fake;
	Place(fake);
	fake.SetZone(OwnZone());
	fake.SetMoveType(MCreature::CREATURE_FAKE_GROUND);
	CHECK(fake.MovePosition(13, 35));
	MCreature lost;
	Place(lost);
	CHECK(!lost.MovePosition(13, 35));
	CHECK(Same({}));
	CHECK_EQ(13, fake.GetX());
}

TEST(Creature, DrawingInterpolatesBetweenTicks)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	const DWORD oldFrame = g_CurrentFrame;
	const DWORD oldAlpha = g_DrawAlphaNum;
	const bool oldInterpolate = g_bInterpolateDraw;
	g_bInterpolateDraw = true;
	g_CurrentFrame = 50;
	creature.SnapshotDrawState();
	creature.SetZone(OwnZone());
	creature.SetPosition(13, 35);
	// A tile is TILE_X by TILE_Y; halfway (128 of 256) is half a tile on.
	g_DrawAlphaNum = 128;
	CHECK_EQ(12 * TILE_X + TILE_X / 2, creature.GetPixelX());
	CHECK_EQ(34 * TILE_Y + TILE_Y / 2, creature.GetPixelY());
	CHECK_EQ(TILE_X / 2 - TILE_X, creature.GetDrawGapX());
	CHECK_EQ(TILE_Y / 2 - TILE_Y, creature.GetDrawGapY());
	// At the start of the tick, exactly where it was.
	g_DrawAlphaNum = 0;
	CHECK_EQ(12 * TILE_X, creature.GetPixelX());
	CHECK_EQ(34 * TILE_Y, creature.GetPixelY());
	// Not interpolating: where it is.
	g_DrawAlphaNum = 128;
	g_bInterpolateDraw = false;
	CHECK_EQ(13 * TILE_X, creature.GetPixelX());
	CHECK_EQ(35 * TILE_Y, creature.GetPixelY());
	g_bInterpolateDraw = true;
	// Another frame: where it is, no gap.
	g_CurrentFrame = 51;
	CHECK_EQ(13 * TILE_X, creature.GetPixelX());
	CHECK_EQ(35 * TILE_Y, creature.GetPixelY());
	CHECK_EQ(0, creature.GetDrawGapX());
	CHECK_EQ(0, creature.GetDrawGapY());
	g_CurrentFrame = oldFrame;
	g_DrawAlphaNum = oldAlpha;
	g_bInterpolateDraw = oldInterpolate;
}

TEST(Creature, ATurretTurnsItsBarrelWithIt)
{
	Scene scene;
	for (int type : {701, 702, 703, 704, 705})
	{
		scene.Tribe(type, CREATURETRIBE_SLAYER);
		calls.clear();
		MCreature creature;
		Place(creature, type);
		// Each step of the cut turns it, the last one too.
		creature.SetCutHeightFrom(0, 2, 1);
		creature.UpdateCutHeight();
		creature.UpdateCutHeight();
		creature.UpdateCutHeight();
		if (type >= 702 && type <= 704) CHECK(Same({"turret|900", "turret|900"}));
		else CHECK(Same({}));
	}
}

//----------------------------------------------------------------------
// Chat
//----------------------------------------------------------------------
TEST(Creature, ChatIsRepairedAndWhiteWhenAsked)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	// The newest line is the last one shown (MAX_CHATSTRING - 1).
	const BYTE newest = BYTE(g_pClientConfig->MAX_CHATSTRING - 1);
	creature.SetChatString("hi\xFF", 0x00ABCDEF);
	CHECK(std::string(creature.GetChatString(newest)) == "hi\xEF\xBF\xBD");
	CHECK_EQ(0x00ABCDEFu, std::uint32_t(creature.GetOriChatColor() & 0x00FFFFFF));
	chatWhite = true;
	creature.SetChatString("again", 0x00ABCDEF);
	CHECK_EQ(std::uint32_t(RGB(255, 255, 255)), std::uint32_t(creature.GetOriChatColor() & 0x00FFFFFF));
}

TEST(Creature, AShopsSignIsRepairedAndTakesThePlayersColour)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	const BYTE newest = BYTE(g_pClientConfig->MAX_CHATSTRING - 1);
	creature.SetPersnalString("sale\xFF", 0x00ABCDEF);
	CHECK(std::string(creature.GetChatString(newest)) == "sale\xEF\xBF\xBD");
	CHECK_EQ(0x00ABCDEFu, std::uint32_t(creature.GetChatColor()));
	// A shop's own sign comes back in the player's chat colour when the
	// chat is cleared while typing.
	creature.SetPersnalShop(1);
	creature.SetPersnalShopMessage("cheap");
	creature.SetInputChatting(true);
	creature.ClearChatString();
	CHECK(std::string(creature.GetChatString(newest)) == "cheap");
	CHECK_EQ(chattingColor, std::uint32_t(creature.GetChatColor()));
}

//----------------------------------------------------------------------
// Bleeding, the coffin, turning vampire, facing
//----------------------------------------------------------------------
TEST(Creature, ALowCreatureBleedsWhenTheOptionIsOn)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	creature.SetStatus(MODIFY_MAX_HP, 100);
	creature.SetStatus(MODIFY_CURRENT_HP, 30);
	const MonotonicClock::TimePoint oldNow = g_FrameNow;
	g_FrameNow = MonotonicClock::TimePoint() + MonotonicClock::Millis(1000);
	// The option off: none.
	creature.CheckDropBlood();
	CHECK(Same({}));
	bloodDrop = true;
	creature.CheckDropBlood();
	CHECK(Same({"skill|" + N(BLOOD_DROP_GROUND) + "|12,34,0|3|900|12,34,0|80|none|first|0,0"}));
	// Not again before its next time.
	calls.clear();
	creature.CheckDropBlood();
	CHECK(Same({}));
	// Above the threshold, or in a coma: none.
	g_FrameNow += MonotonicClock::Millis(1000000);
	creature.SetStatus(MODIFY_CURRENT_HP, 31);
	creature.CheckDropBlood();
	CHECK(Same({}));
	g_FrameNow = oldNow;
}

TEST(Creature, ThePlayersCoffinLocksItsGear)
{
	Scene scene;
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	MCreature creature;
	Place(creature, 6);
	creature.AddCasketSoon(1);
	creature.RemoveCasketSoon();
	CHECK(Same({}));
	player = &creature;
	creature.AddCasketSoon(1);
	CHECK(Same({"lock gear"}));
	creature.RemoveCasketSoon();
	CHECK(Same({"lock gear", "unlock gear"}));
	player = nullptr;
}

TEST(Creature, TurningVampireSoundsByGender)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	scene.Tribe(CREATURETYPE_VAMPIRE_MALE1, CREATURETRIBE_VAMPIRE);
	scene.Tribe(CREATURETYPE_VAMPIRE_FEMALE1, CREATURETRIBE_VAMPIRE);
	MCreature man;
	Place(man);
	man.SetMale();
	man.ChangeToVampire();
	CHECK(FirstIs("sound at|" + N(SOUND_VAMPIRE_CHANGE_MALE) + "|0|12,34"));
	CHECK_EQ(CREATURETYPE_VAMPIRE_MALE1, int(man.GetCreatureType()));
	calls.clear();
	MCreature woman;
	Place(woman);
	woman.SetFemale();
	woman.ChangeToVampire();
	CHECK(FirstIs("sound at|" + N(SOUND_VAMPIRE_CHANGE_FEMALE) + "|0|12,34"));
	// A vampire already: nothing.
	calls.clear();
	CHECK(!woman.ChangeToVampire());
	CHECK(Same({}));
}

TEST(Creature, ItFacesACellOrKeepsItsFacing)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	for (const auto& cell : {std::pair<int, int>{20, 34}, {12, 40}, {5, 30}, {13, 33}})
	{
		CHECK_EQ(int(SelectFacingDirection(12, 34, cell.first, cell.second)), int(creature.GetDirectionToPosition(cell.first, cell.second)));
		creature.SetDirectionToPosition(cell.first, cell.second);
		CHECK_EQ(int(SelectFacingDirection(12, 34, cell.first, cell.second)), int(creature.GetServerDirection()));
	}
	// Its own cell: the way it faces now.
	CHECK_EQ(5, int(creature.GetDirectionToPosition(12, 34)));
}

//----------------------------------------------------------------------
// The turret and the bike
//----------------------------------------------------------------------
TEST(Creature, ATurretsLastShotSounds)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	creature.InstallTurretStopAttack();
	CHECK(Same({}));
	creature.SetAction(ACTION_ATTACK);
	creature.InstallTurretStopAttack();
	CHECK(Same({"sound|" + N(SOUND_SLAYER_TURRET_FIRE_END)}));
}

TEST(Creature, ACrashedBikeSpinsThreeTimesAndFreesThePlayer)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	creature.AddEffectStatus(EFFECTSTATUS_BIKE_CRASH, 0xFFFF);
	creature.SetMoveDevice(MCreature::MOVE_DEVICE_RIDE);
	creature.SetAction(ACTION_SLAYER_MOTOR_STAND);
	CHECK_EQ(ACTION_SLAYER_MOTOR_STAND, int(creature.GetAction()));
	creature.SetInstallTurretDirect(1);
	creature.SetInstallTurretCount(0);
	creature.SetCurrentDirection(0);
	playerID = 900;
	calls.clear();
	// One step of eight per update. The crash skill plays when it faces
	// three steps past its start (4); the third time it faces the start
	// (1) the crash ends, before the third skill.
	for (int step = 0; step < 24; ++step)
		creature.UpdateBikeCrash();
	int crashes = 0;
	for (const std::string& call : calls)
		if (call.rfind("skill|" + N(SKILL_CLIENT_BIKE_CRASH), 0) == 0) ++crashes;
	CHECK_EQ(2, crashes);
	CHECK(!calls.empty() && calls.back() == "delay|0");
	CHECK(!creature.HasEffectStatus(EFFECTSTATUS_BIKE_CRASH));
}

//----------------------------------------------------------------------
// Darkness and the ground under it
//----------------------------------------------------------------------
namespace {
const int MaxDarkness = 13;	// MCreature.cpp's MAX_DARKNESS_COUNT
}

TEST(Creature, ItSeesItsSectorsDarknessInsideTheZone)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	CHECK_EQ(-1, creature.GetDarknessCount());
	sector.SetDarkness();
	sectorReads.clear();
	creature.CheckInDarkness();
	CHECK(sectorReads == std::vector<std::string>{"12,34"});
	CHECK_EQ(MaxDarkness, creature.GetDarknessCount());
	sector.UnSetDarkness();
	creature.CheckInDarkness();
	CHECK_EQ(-1, creature.GetDarknessCount());
	// Past the zone's edge, or without a zone, it reads no sector.
	sector.SetDarkness();
	sectorReads.clear();
	for (const std::pair<int, int>& size : {std::pair<int, int>{12, 100}, {100, 34}})
	{
		zoneWidth = size.first;
		zoneHeight = size.second;
		creature.CheckInDarkness();
	}
	zoneWidth = zoneHeight = 100;
	zoneExists = false;
	creature.CheckInDarkness();
	CHECK(sectorReads.empty());
	CHECK_EQ(-1, creature.GetDarknessCount());
	// Dead, it is not in it.
	zoneExists = true;
	creature.SetDead();
	creature.CheckInDarkness();
	CHECK_EQ(-1, creature.GetDarknessCount());
}

TEST(Creature, ItStandsInAnElementalOrAGnomesFury)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	sectorReads.clear();
	CHECK(!creature.IsInGroundElemental());
	CHECK(!creature.IsInFuryOfGnome());
	CHECK(sectorReads == (std::vector<std::string>{"12,34", "12,34"}));
	sector.SetGroundElemental();
	CHECK(creature.IsInGroundElemental());
	CHECK(!creature.IsInFuryOfGnome());
	sector.SetFuryOfGnome();
	CHECK(creature.IsInFuryOfGnome());
	sectorReads.clear();
	zoneWidth = 12;
	CHECK(!creature.IsInGroundElemental());
	CHECK(!creature.IsInFuryOfGnome());
	zoneWidth = 100;
	zoneHeight = 34;
	CHECK(!creature.IsInGroundElemental());
	CHECK(!creature.IsInFuryOfGnome());
	zoneHeight = 100;
	zoneExists = false;
	CHECK(!creature.IsInGroundElemental());
	CHECK(!creature.IsInFuryOfGnome());
	CHECK(sectorReads.empty());
	zoneExists = true;
	creature.SetDead();
	CHECK(!creature.IsInGroundElemental());
	CHECK(!creature.IsInFuryOfGnome());
}

TEST(Creature, AVampireIsDarkOnlyInItsOwnZoneOrWhenBlind)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	// A slayer goes dark anywhere; a vampire only in zone 3001, or blind.
	MCreature slayer;
	Place(slayer);
	slayer.PlaceInDarkness();
	CHECK_EQ(MaxDarkness, slayer.GetDarknessCount());
	MCreature vampire;
	Place(vampire, 6);
	vampire.PlaceInDarkness();
	CHECK_EQ(-1, vampire.GetDarknessCount());
	vampire.PlaceInDarkness(true);
	CHECK_EQ(MaxDarkness, vampire.GetDarknessCount());
	MCreature home;
	Place(home, 6);
	zoneID = 3001;
	home.PlaceInDarkness();
	CHECK_EQ(MaxDarkness, home.GetDarknessCount());
	// The player's ghost sees no darkness at all.
	zoneID = 1;
	playerGhost = true;
	MCreature haunted;
	Place(haunted);
	haunted.PlaceInDarkness(true);
	CHECK_EQ(-1, haunted.GetDarknessCount());
	CHECK(Same({}));
}

TEST(Creature, DarknessFadesInAndOutByUpdates)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	MCreature creature;
	Place(creature);
	creature.PlaceInDarkness();
	creature.UpdateDarkness();
	creature.UpdateDarkness();
	CHECK_EQ(MaxDarkness - 2, creature.GetDarknessCount());
	creature.PlaceNotInDarkness();
	creature.UpdateDarkness();
	CHECK_EQ(MaxDarkness - 1, creature.GetDarknessCount());
	// Reaching the top, it leaves the darkness.
	creature.UpdateDarkness();
	CHECK_EQ(-1, creature.GetDarknessCount());
	// A vampire outside zone 3001 drops it at once, and the player's ghost
	// drops it for everyone; in zone 3001 a vampire keeps it.
	MCreature vampire;
	Place(vampire, 6);
	vampire.PlaceInDarkness(true);
	vampire.UpdateDarkness();
	CHECK_EQ(-1, vampire.GetDarknessCount());
	zoneID = 3001;
	vampire.PlaceInDarkness(true);
	vampire.UpdateDarkness();
	CHECK_EQ(MaxDarkness - 1, vampire.GetDarknessCount());
	// PlaceNotInDarkness turns it back only where it would update.
	zoneID = 1;
	vampire.PlaceNotInDarkness();
	zoneID = 3001;
	vampire.UpdateDarkness();
	CHECK_EQ(MaxDarkness - 2, vampire.GetDarknessCount());
	// The player's ghost stops PlaceNotInDarkness turning it back.
	playerGhost = true;
	vampire.PlaceNotInDarkness();
	playerGhost = false;
	vampire.UpdateDarkness();
	CHECK_EQ(MaxDarkness - 3, vampire.GetDarknessCount());
	playerGhost = true;
	vampire.UpdateDarkness();
	CHECK_EQ(-1, vampire.GetDarknessCount());
	CHECK(Same({}));
}

TEST(Creature, ItIsSeenInDarknessOnlyWithinItsCount)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	// Not in darkness: seen from anywhere.
	CHECK(creature.ShowInDarkness(12 + 50, 34));
	creature.PlaceInDarkness();
	CHECK(creature.ShowInDarkness(12 + MaxDarkness, 34));
	CHECK(creature.ShowInDarkness(12, 34 - MaxDarkness));
	CHECK(!creature.ShowInDarkness(12 + MaxDarkness + 1, 34));
	CHECK(!creature.ShowInDarkness(12, 34 + MaxDarkness + 1));
	// Lightness shows it, except in zone 3001; the player's ghost sees it.
	creature.AddEffectStatus(EFFECTSTATUS_LIGHTNESS, 0xFFFF);
	CHECK(creature.ShowInDarkness(12 + 50, 34));
	zoneID = 3001;
	CHECK(!creature.ShowInDarkness(12 + 50, 34));
	playerGhost = true;
	CHECK(creature.ShowInDarkness(12 + 50, 34));
}

//----------------------------------------------------------------------
// Knocked back
//----------------------------------------------------------------------
TEST(Creature, AKnockBackSlidesItBackPixelByPixel)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	creature.SetZone(OwnZone());
	creature.SetServerPosition(12, 34);
	calls.clear();
	// A push (not a hit): three steps, faded.
	CHECK(creature.KnockBackPosition(15, 34, 1));
	CHECK_EQ(-3 * TILE_X, creature.GetSX());
	CHECK_EQ(TILE_X, creature.GetCX());
	CHECK(creature.IsKnockBack());
	CHECK(creature.HasEffectStatus(EFFECTSTATUS_FADE_OUT));
	CHECK_EQ(int(SelectFacingDirection(15, 34, 12, 34)), int(creature.GetServerDirection()));
	const std::string ground = N(MCreature::CREATURE_GROUND);
	CHECK(Same({"move ground|own|900|12,34|15,34", "unblock|own|" + ground + "|12,34", "block|own|" + ground + "|15,34"}));
	CHECK_EQ(15, creature.GetServerX());
}

TEST(Creature, AHitKnockBackSoundsAndBleeds)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	testfw::MutableRow(*g_pCreatureTable, 5).SetActionSound(ACTION_DAMAGED, 77);
	MCreature creature;
	Place(creature);
	creature.SetZone(OwnZone());
	creature.SetServerPosition(12, 34);
	calls.clear();
	bloodDrop = true;
	CHECK(creature.KnockBackPosition(12, 36, 0));
	CHECK(FirstIs("sound at|77|0|12,34"));
	CHECK_EQ(ACTION_DAMAGED, int(creature.GetAction()));
	// One step per frame of the hit: 10 + ACTION_DAMAGED frames.
	CHECK_EQ(-2 * TILE_Y, creature.GetSY());
	CHECK_EQ(-2 * TILE_Y / (10 + ACTION_DAMAGED), creature.GetCY() * -1);
	int bleeds = 0;
	const std::string blood = "skill|" + N(BLOOD_CRITICAL_HIT) + "|12,34,0|" + N(SelectFacingDirection(12, 36, 12, 34)) + "|900|12,34,0|";
	for (const std::string& call : calls)
		if (call.rfind(blood, 0) == 0) ++bleeds;
	CHECK(bleeds >= 3 && bleeds <= 5);
	// No count for the hit: no knock-back at all.
	testfw::MutableRow(*g_pCreatureTable, 5).SetActionCount(ACTION_DAMAGED, 0);
	calls.clear();
	CHECK(!creature.KnockBackPosition(12, 38, 0));
	CHECK(Same({}));
	// The option off: no blood.
	testfw::MutableRow(*g_pCreatureTable, 5).SetActionCount(ACTION_DAMAGED, 10 + ACTION_DAMAGED);
	bloodDrop = false;
	calls.clear();
	CHECK(creature.KnockBackPosition(12, 38, 0));
	for (const std::string& call : calls)
		CHECK(call.rfind("skill|", 0) != 0);
}

//----------------------------------------------------------------------
// Drained
//----------------------------------------------------------------------
namespace {
struct Drinker : MCreature
{
	void StopBloodDrain() override { calls.push_back("stop drain|" + N(GetID())); }
};
}

TEST(Creature, ADrainedCreatureStopsItsDrinker)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature victim;
	Place(victim);
	victim.SetDrainCreatureID(501);
	// Not being drained: nobody is asked.
	victim.StopDrain();
	CHECK(Same({}));
	victim.SetAction(ACTION_DRAINED);
	victim.StopDrain();
	CHECK(Same({"creature|501"}));
	Drinker drinker;
	drinker.SetID(501);
	zoneCreatures[501] = &drinker;
	calls.clear();
	victim.StopDrain();
	CHECK(Same({"creature|501", "stop drain|501"}));
}

TEST(Creature, AnAbsorbedCreatureStopsItsAbsorber)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature victim;
	Place(victim);
	victim.SetAbsorbCreatureID(501);
	victim.StopAbsorb();
	CHECK(Same({}));
	victim.SetAction(ACTION_DRAINED);
	victim.StopAbsorb();
	CHECK(Same({"creature|501"}));
	// It asks the absorber to stop absorbing (a drained absorber passes it on).
	MCreature absorber;
	Place(absorber);
	absorber.SetID(501);
	absorber.SetAbsorbCreatureID(502);
	absorber.SetAction(ACTION_DRAINED);
	zoneCreatures[501] = &absorber;
	calls.clear();
	victim.StopAbsorb();
	CHECK(Same({"creature|501", "creature|502"}));
}

//----------------------------------------------------------------------
// Regeneration
//----------------------------------------------------------------------
TEST(Creature, ThePlayersVampireRegeneratesByItsDexterity)
{
	Scene scene;
	scene.Tribe(6, CREATURETRIBE_VAMPIRE);
	scene.Tribe(CREATURETYPE_BAT, CREATURETRIBE_VAMPIRE);
	const int bonus = decore::vampireDexHPRegenBonus(200);
	CHECK(bonus > 0);
	const MonotonicClock::TimePoint oldNow = g_FrameNow;
	g_FrameNow = MonotonicClock::TimePoint() + MonotonicClock::Millis(1000);
	const auto regenerated = [](MCreature& creature, int amount) {
		creature.SetStatus(MODIFY_MAX_HP, 1000);
		creature.SetStatus(MODIFY_CURRENT_HP, 100);
		creature.SetStatus(MODIFY_BASIC_DEX, 200);
		creature.SetRegen(amount, 1000);
		creature.UpdateStatus();
		return int(creature.GetStatus(MODIFY_CURRENT_HP)) - 100;
	};
	MCreature other;
	Place(other, 6);
	CHECK_EQ(5, regenerated(other, 5));
	MCreature self;
	Place(self, 6);
	player = &self;
	CHECK_EQ(5 + bonus, regenerated(self, 5));
	// As a bat, no bonus.
	self.SetCreatureType(CREATURETYPE_BAT);
	CHECK_EQ(5, regenerated(self, 5));
	player = nullptr;
	g_FrameNow = oldNow;
}

//----------------------------------------------------------------------
// Acting: each frame's update
//----------------------------------------------------------------------
namespace {
// The move tables GameInitInfo builds once.
void MoveTables()
{
	static const bool ready = (MCreature::InitMoveTable(), true);
	(void)ready;
}
// A creature whose move step a test can take.
struct Mover : MCreature
{
	using MCreature::ActionMove;
	using MCreature::AttachCastingEffect;
	using MCreature::AffectUsedActionInfo;
};
// The cell a step in the direction leads to.
std::string Next(int x, int y, BYTE direction)
{
	TYPE_SECTORPOSITION nx = TYPE_SECTORPOSITION(x), ny = TYPE_SECTORPOSITION(y);
	WorldTileGeometry::Step(nx, ny, direction);
	return N(nx) + "," + N(ny);
}
}

TEST(Creature, ItsElementalLeavesWithItsStatus)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	creature.SetElementalID(501);
	creature.AddEffectStatus(EFFECTSTATUS_FIRE_ELEMENTAL, 0xFFFF);
	calls.clear();
	creature.Action();
	CHECK(Same({}));
	creature.RemoveEffectStatus(EFFECTSTATUS_FIRE_ELEMENTAL);
	creature.AddEffectStatus(EFFECTSTATUS_WATER_ELEMENTAL, 0xFFFF);
	calls.clear();
	creature.Action();
	CHECK(Same({}));
	creature.RemoveEffectStatus(EFFECTSTATUS_WATER_ELEMENTAL);
	calls.clear();
	zoneExists = false;
	creature.Action();
	CHECK(Same({}));
	zoneExists = true;
	creature.Action();
	CHECK(Same({"remove fake|501"}));
	// Without one, none.
	calls.clear();
	creature.SetElementalID(OBJECTID_NULL);
	creature.Action();
	CHECK(Same({}));
}

TEST(Creature, AKeptSweeperWalksInStepWithTheFrames)
{
	Scene scene;
	scene.Tribe(672, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature, 672);
	creature.AddEffectStatus(EFFECTSTATUS_KEEP_SWEEPER, 0xFFFF);
	const DWORD oldFrame = g_CurrentFrame;
	// It switches to moving and keeps the count it had (a move's is none).
	creature.SetAction(ACTION_ATTACK);
	const int count = creature.GetActionCountMax();
	CHECK(count > 0);
	g_CurrentFrame = DWORD(count * 7 + 3);
	creature.Action();
	CHECK_EQ(ACTION_MOVE, int(creature.GetAction()));
	CHECK_EQ(3, int(creature.GetActionCount()));
	g_CurrentFrame = oldFrame;
}

TEST(Creature, ARelicsBearerActsOnEvenFramesOnly)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	creature.SetZone(OwnZone());
	creature.AddEffectStatus(EFFECTSTATUS_HAS_SLAYER_RELIC, 0xFFFF);
	CHECK(creature.KnockBackPosition(15, 34, 1));
	const DWORD oldFrame = g_CurrentFrame;
	g_CurrentFrame = 7;
	creature.Action();
	CHECK_EQ(-3 * TILE_X, creature.GetSX());
	g_CurrentFrame = 8;
	creature.Action();
	CHECK_EQ(-2 * TILE_X, creature.GetSX());
	g_CurrentFrame = oldFrame;
}

TEST(Creature, AnIdleTurretLooksAroundEveryHundredFrames)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	creature.AddEffectStatus(EFFECTSTATUS_INSTALL_TURRET, 0xFFFF);
	creature.SetAction(ACTION_STAND);
	creature.SetInstallTurretDirect(creature.GetServerDirection());
	const BYTE facing = creature.GetServerDirection();
	const DWORD oldFrame = g_CurrentFrame;
	g_CurrentFrame = 101;
	CHECK(creature.UpDateInstallTurret());
	CHECK_EQ(int(facing), int(creature.GetServerDirection()));
	g_CurrentFrame = 200;
	CHECK(creature.UpDateInstallTurret());
	CHECK(facing != creature.GetServerDirection());
	g_CurrentFrame = oldFrame;
}

TEST(Creature, ATryKeepsOnlyOnTheRegenTowerUnderThePlayer)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature somebody;
	somebody.SetCreatureType(5);
	somebody.SetPosition(20, 30);
	player = &somebody;
	MEffect tower(BLT_NORMAL), elsewhere(BLT_NORMAL), beside(BLT_NORMAL), other(BLT_NORMAL);
	tower.SetFrameID(EFFECTSPRITETYPE_REGEN_TOWER_GROUND, 1);
	tower.SetPosition(20, 30);
	elsewhere.SetFrameID(EFFECTSPRITETYPE_REGEN_TOWER_GROUND, 1);
	elsewhere.SetPosition(20, 31);
	beside.SetFrameID(EFFECTSPRITETYPE_REGEN_TOWER_GROUND, 1);
	beside.SetPosition(21, 30);
	other.SetFrameID(EFFECTSPRITETYPE_REGEN_TOWER_GROUND + 1, 1);
	other.SetPosition(20, 30);
	const auto kept = [](std::vector<const MEffect*> effects) {
		Mover creature;
		Place(creature);
		creature.AddEffectStatus(EFFECTSTATUS_TRYING, 0xFFFF);
		groundEffects = effects;
		creature.ActionMove();
		return bool(creature.HasEffectStatus(EFFECTSTATUS_TRYING));
	};
	CHECK(!kept({}));
	CHECK(kept({&elsewhere, &beside, &other, &tower}));
	CHECK(!kept({&elsewhere, &beside, &other}));
	player = nullptr;
}

TEST(Creature, AStepSoundsAtTheCellItReaches)
{
	MoveTables();
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	testfw::MutableRow(*g_pCreatureTable, 5).MoveTimes = 8;
	testfw::MutableRow(*g_pCreatureTable, 5).SetActionSound(ACTION_MOVE, 88);
	Mover creature;
	Place(creature);
	creature.SetZone(OwnZone());
	creature.PacketMove(12, 34, DIRECTION_RIGHT);
	calls.clear();
	creature.ActionMove();
	const std::string next = Next(12, 34, DIRECTION_RIGHT);
	CHECK(Same({"move ground|own|900|12,34|" + next, "sound at|88|0|" + next}));
	// Refused by the zone: it stands, silent.
	Mover blocked;
	Place(blocked);
	blocked.SetZone(OwnZone());
	blocked.PacketMove(12, 34, DIRECTION_RIGHT);
	calls.clear();
	moveOK = false;
	blocked.ActionMove();
	CHECK(Same({"move ground|own|900|12,34|" + next}));
	CHECK_EQ(ACTION_STAND, int(blocked.GetAction()));
}

TEST(Creature, AMoveOffTheZoneIsDropped)
{
	MoveTables();
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MCreature creature;
	Place(creature);
	creature.SetZone(OwnZone());
	// Moving, it buffers the step instead of taking it.
	creature.SetAction(ACTION_MOVE);
	creature.PacketMove(12, 34, DIRECTION_RIGHT);
	zoneWidth = 13;
	CHECK(!creature.AffectMoveBuffer());
	creature.PacketMove(12, 34, DIRECTION_DOWN);
	zoneWidth = 100;
	zoneHeight = 35;
	CHECK(!creature.AffectMoveBuffer());
	zoneHeight = 100;
	creature.PacketMove(12, 34, DIRECTION_RIGHT);
	CHECK(creature.AffectMoveBuffer());
}

//----------------------------------------------------------------------
// Casting at another creature
//----------------------------------------------------------------------
namespace {
const TYPE_ACTIONINFO Cast = 200;
// A one-phase attack at another creature, by the fake generator, with a
// casting effect and a casting sound (66).
MActionInfo& CastRow(BYTE fStart = FLAG_ACTIONINFO_START_USER)
{
	MActionInfo& info = testfw::MutableRow(*g_pActionInfoTable, Cast);
	info.Set("cast", ACTION_ATTACK, EFFECTSPRITETYPE_BLOOD, 1, FLAG_ACTIONINFO_TARGET_OTHER, fStart);
	info.Init(1);
	info.GetMutable(0)->EffectGeneratorID = 7;
	info.GetMutable(0)->Count = 5;
	info.SetSoundID(66);
	info.SetCastingStartFrameAll(0);
	info.SetCastingFramesAll(4);
	info.SetCastingEffectToOther();
	return info;
}
}

TEST(Creature, ASkillAtAnotherAimsAtItsCell)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	CastRow();
	MCreature target;
	target.SetCreatureType(5);
	target.SetID(501);
	target.SetPosition(20, 30);
	zoneCreatures[501] = &target;
	Mover creature;
	Place(creature);
	creature.PacketSpecialActionToOther(Cast, 501, NULL);
	CHECK(FirstIs("creature|501"));
	// From its own cell, at the target's.
	calls.clear();
	creature.AffectUsedActionInfo(Cast);
	CHECK(Same({"effect|" + N(Cast) + "|" + N(12 * TILE_X) + "," + N(34 * TILE_Y) + "|" + N(20 * TILE_X) + "," + N(30 * TILE_Y) + "|501"}));
	// Started at the target, it starts at the target's cell too.
	CastRow(FLAG_ACTIONINFO_START_TARGET);
	calls.clear();
	creature.AffectUsedActionInfo(Cast);
	CHECK(Same({"effect|" + N(Cast) + "|" + N(20 * TILE_X) + "," + N(30 * TILE_Y) + "|" + N(20 * TILE_X) + "," + N(30 * TILE_Y) + "|501"}));
	// Nobody there: nothing is cast.
	calls.clear();
	Mover alone;
	Place(alone);
	alone.PacketSpecialActionToOther(Cast, 502, NULL);
	CHECK(Same({"creature|502"}));
	CHECK_EQ(int(ACTIONINFO_NULL), int(alone.GetUsedActionInfo()));
}

TEST(Creature, ACastingEffectGoesOnItsTargetWithItsSound)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	MActionInfo& info = CastRow();
	MCreature target;
	target.SetCreatureType(5);
	target.SetID(501);
	target.SetPosition(20, 30);
	zoneCreatures[501] = &target;
	Mover creature;
	Place(creature);
	creature.SetTraceID(501);
	calls.clear();
	creature.AttachCastingEffect(Cast, TRUE);
	CHECK(Same({"creature|501", "sound at|66|0|12,34"}));
	// On itself, it asks for nobody.
	info.SetCastingEffectToSelf();
	calls.clear();
	creature.AttachCastingEffect(Cast, TRUE);
	CHECK(Same({"sound at|66|0|12,34"}));
	// Without a casting sprite, no effect, but the sound.
	info.SetCastingEffectToOther();
	info.Set("cast", ACTION_ATTACK, EFFECTSPRITETYPE_NULL, 1, FLAG_ACTIONINFO_TARGET_OTHER);
	calls.clear();
	creature.AttachCastingEffect(Cast, TRUE);
	CHECK(Same({"sound at|66|0|12,34"}));
	// Not forced, and not its casting frame: nothing.
	info.SetCastingStartFrameAll(3);
	calls.clear();
	creature.AttachCastingEffect(Cast, FALSE);
	CHECK(Same({}));
}

TEST(Creature, AnAttacksSoundPlaysWhenItsActionStarts)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	testfw::MutableRow(*g_pCreatureTable, 5).SetActionSound(ACTION_ATTACK, 55);
	MActionInfo& info = CastRow();
	MCreature target;
	target.SetCreatureType(5);
	target.SetID(501);
	target.SetPosition(20, 30);
	zoneCreatures[501] = &target;
	// Cast at once (casting frame 0): the casting effect, its sound, the
	// effect, then the attack's sound.
	MCreature creature;
	Place(creature);
	creature.PacketSpecialActionToOther(Cast, 501, NULL);
	calls.clear();
	creature.Action();
	const std::string effect = "effect|" + N(Cast) + "|" + N(12 * TILE_X) + "," + N(34 * TILE_Y) + "|" + N(20 * TILE_X) + "," + N(30 * TILE_Y) + "|501";
	CHECK(Same({"creature|501", "sound at|66|0|12,34", effect, "sound at|55|0|12,34"}));
	// Cast later (frame 2): the attack's sound as it starts.
	info.SetCastingStartFrameAll(2);
	MCreature later;
	Place(later);
	later.PacketSpecialActionToOther(Cast, 501, NULL);
	calls.clear();
	later.Action();
	CHECK(FirstIs("sound at|55|0|12,34"));
}

//----------------------------------------------------------------------
// The frame clock and the zone's colour
//----------------------------------------------------------------------
namespace {
// The first attached effect's last frame, or 0 without one.
DWORD FirstEnd(const MCreature& creature, bool ground = false)
{
	if (ground ? creature.GetGroundAttachEffectSize() == 0 : creature.GetAttachEffectSize() == 0) return 0;
	return (*(ground ? creature.GetGroundAttachEffectIterator() : creature.GetAttachEffectIterator()))->GetEndFrame();
}
}

TEST(Creature, ARepeatedEffectLastsTheLongerOfItsTimes)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	scene.effectSprites.GetMutable(20)->FrameID = 33;
	const DWORD oldFrame = g_CurrentFrame;
	for (bool ground : {false, true})
	{
		MCreature creature;
		Place(creature);
		g_CurrentFrame = 100;
		CHECK(creature.CreateAttachEffect(20, 10, 10, ground ? TRUE : FALSE) != NULL);
		CHECK_EQ(109u, FirstEnd(creature, ground));
		creature.CreateAttachEffect(20, 30, 30, ground ? TRUE : FALSE);
		CHECK_EQ(129u, FirstEnd(creature, ground));
		creature.CreateAttachEffect(20, 5, 5, ground ? TRUE : FALSE);
		CHECK_EQ(129u, FirstEnd(creature, ground));
		g_CurrentFrame = 200;
		creature.CreateAttachEffect(20, 5, 5, ground ? TRUE : FALSE);
		CHECK_EQ(204u, FirstEnd(creature, ground));
	}
	g_CurrentFrame = oldFrame;
}

TEST(Creature, ARepeatedStatusLastsTheLongerOfItsTimes)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	scene.effectSprites.GetMutable(20)->FrameID = 33;
	scene.effectStatuses.GetMutable(EFFECTSTATUS_STRIKING)->bUseEffectSprite = true;
	scene.effectStatuses.GetMutable(EFFECTSTATUS_STRIKING)->EffectSpriteType = 20;
	scene.effectStatuses.GetMutable(EFFECTSTATUS_STRIKING)->EffectColor = 0xFFFF;
	// The same sprite on the ground.
	scene.effectStatuses.GetMutable(EFFECTSTATUS_PROTECTION_FROM_ACID)->bUseEffectSprite = true;
	scene.effectStatuses.GetMutable(EFFECTSTATUS_PROTECTION_FROM_ACID)->EffectSpriteType = 20;
	scene.effectStatuses.GetMutable(EFFECTSTATUS_PROTECTION_FROM_ACID)->EffectColor = 0xFFFF;
	scene.effectStatuses.GetMutable(EFFECTSTATUS_PROTECTION_FROM_ACID)->bAttachGround = true;
	// A colour, not a sprite.
	scene.effectStatuses.GetMutable(EFFECTSTATUS_BLESS)->bUseEffectSprite = false;
	scene.effectStatuses.GetMutable(EFFECTSTATUS_BLESS)->EffectColor = 5;
	const DWORD oldFrame = g_CurrentFrame;
	for (EFFECTSTATUS status : {EFFECTSTATUS_STRIKING, EFFECTSTATUS_PROTECTION_FROM_ACID, EFFECTSTATUS_BLESS})
	{
		const bool ground = status == EFFECTSTATUS_PROTECTION_FROM_ACID;
		MCreature creature;
		Place(creature);
		g_CurrentFrame = 100;
		creature.AddEffectStatus(status, 10);
		CHECK_EQ(109u, FirstEnd(creature, ground));
		creature.AddEffectStatus(status, 30);
		CHECK_EQ(129u, FirstEnd(creature, ground));
		creature.AddEffectStatus(status, 5);
		CHECK_EQ(129u, FirstEnd(creature, ground));
		g_CurrentFrame = 200;
		creature.AddEffectStatus(status, 5);
		CHECK_EQ(204u, FirstEnd(creature, ground));
		CHECK_EQ(1, ground ? creature.GetGroundAttachEffectSize() : creature.GetAttachEffectSize());
	}
	g_CurrentFrame = oldFrame;
}

TEST(Creature, AMonsterTakesTheZonesColour)
{
	Scene scene;
	scene.Tribe(9, CREATURETRIBE_SLAYER);
	CREATURETABLE_INFO& row = testfw::MutableRow(*g_pCreatureTable, 9);
	row.SpriteTypes.Init(1);
	*row.SpriteTypes.GetMutable(0) = 2;
	row.ChangeColorSet = 0xFFFF;
	scene.sprites.GetMutable(2)->CreatureType = FLAG_CREATURESPRITE_MONSTER;
	const unsigned short oldColour = g_ZoneCreatureColorSet;
	g_ZoneCreatureColorSet = 77;
	MCreature monster;
	monster.SetCreatureType(9);
	CHECK_EQ(77, int(monster.GetChangeColorSet()));
	CHECK_EQ(77, int(monster.GetAttachEffectColor()));
	// Not a monster's sprite: not the zone's colour.
	scene.sprites.GetMutable(2)->CreatureType = FLAG_CREATURESPRITE_PLAYER;
	MCreature person;
	person.SetCreatureType(9);
	CHECK(person.GetChangeColorSet() != 77);
	CHECK(person.GetAttachEffectColor() != 77);
	g_ZoneCreatureColorSet = oldColour;
}

TEST(Creature, ChatIsStampedWithTheFrame)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	const DWORD oldFrame = g_CurrentFrame;
	MCreature creature;
	Place(creature);
	g_CurrentFrame = 1234;
	creature.SetChatString("hi", 0x00ABCDEF);
	CHECK_EQ(1234u, creature.GetChatTime());
	g_CurrentFrame = 2345;
	creature.SetPersnalString("sale", 0x00ABCDEF);
	CHECK_EQ(2345u, creature.GetChatTime());
	g_CurrentFrame = oldFrame;
}

TEST(Creature, AFastMoveEndsFacingWhatItTraces)
{
	MoveTables();
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	testfw::MutableRow(*g_pCreatureTable, 5).MoveTimes = 8;
	testfw::MutableRow(*g_pCreatureTable, 5).MoveRatio = 1;
	MCreature target;
	target.SetCreatureType(5);
	target.SetID(501);
	target.SetPosition(20, 30);
	zoneCreatures[501] = &target;
	Mover creature;
	Place(creature);
	creature.SetZone(OwnZone());
	creature.SetTraceID(501);
	creature.FastMovePosition(14, 34);
	calls.clear();
	for (int step = 0; step < 12; ++step)
		creature.ActionMove();
	int looks = 0;
	for (const std::string& call : calls)
		if (call == "creature|501") ++looks;
	CHECK(looks > 0);
	CHECK_EQ(int(SelectFacingDirection(14, 34, 20, 30)), int(creature.GetServerDirection()));
	// Nothing to trace: it keeps the way it moved.
	Mover alone;
	Place(alone);
	alone.SetZone(OwnZone());
	alone.SetTraceID(502);
	alone.FastMovePosition(14, 34);
	for (int step = 0; step < 12; ++step)
		alone.ActionMove();
	CHECK_EQ(int(SelectFacingDirection(12, 34, 14, 34)), int(alone.GetServerDirection()));
}

TEST(Creature, ItsColourShowsButEveryFewFrames)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	scene.effectStatuses.GetMutable(EFFECTSTATUS_BLESS)->bUseEffectSprite = false;
	scene.effectStatuses.GetMutable(EFFECTSTATUS_BLESS)->EffectColor = 5;
	const DWORD oldFrame = g_CurrentFrame;
	const int oldEvery = g_pClientConfig->FRAME_DRAW_ORIGINAL_SPRITE;
	g_pClientConfig->FRAME_DRAW_ORIGINAL_SPRITE = 4;
	MCreature creature;
	Place(creature);
	g_CurrentFrame = 100;
	creature.AddEffectStatus(EFFECTSTATUS_BLESS, 50);
	const WORD own = creature.GetChangeColorSet();
	CHECK(own != 5);
	g_CurrentFrame = 101;
	creature.UpdateAttachEffect();
	CHECK_EQ(5, int(creature.GetAttachEffectColor()));
	// Every fourth frame, its own colour.
	g_CurrentFrame = 104;
	creature.UpdateAttachEffect();
	CHECK_EQ(int(own), int(creature.GetAttachEffectColor()));
	// Paralysed, always the effect's.
	creature.AddEffectStatus(EFFECTSTATUS_CURSE_PARALYSIS, 50);
	g_CurrentFrame = 108;
	creature.UpdateAttachEffect();
	CHECK_EQ(5, int(creature.GetAttachEffectColor()));
	g_pClientConfig->FRAME_DRAW_ORIGINAL_SPRITE = oldEvery;
	g_CurrentFrame = oldFrame;
}

TEST(Creature, AnOustersFastMoveSpriteFollowsItsAction)
{
	Scene scene;
	scene.Tribe(7, CREATURETRIBE_OUSTERS);
	scene.effectSprites.GetMutable(EFFECTSPRITETYPE_FAST_MOVE_STOP)->BltType = BLT_SCREEN;
	scene.effectSprites.GetMutable(EFFECTSPRITETYPE_FAST_MOVE_STOP)->FrameID = 41;
	scene.effectSprites.GetMutable(EFFECTSPRITETYPE_FAST_MOVE_FLY)->FrameID = 42;
	MCreature creature;
	Place(creature, 7);
	creature.SetMoveDevice(MCreature::MOVE_DEVICE_SUMMON_SYLPH);
	creature.CreateAttachEffect(EFFECTSPRITETYPE_FAST_MOVE_FLY, 50, 50);
	// Standing, the flying sprite becomes the hovering one, at its frames.
	creature.SetAction(ACTION_STAND);
	CHECK_EQ(ACTION_OUSTERS_FAST_MOVE_STAND, int(creature.GetAction()));
	calls.clear();
	creature.UpdateAttachEffect();
	CHECK(Same({"max frame|" + N(BLT_SCREEN) + "|41"}));
	CHECK_EQ(int(EFFECTSPRITETYPE_FAST_MOVE_STOP), int((*creature.GetAttachEffectIterator())->GetEffectSpriteType()));
	// Moving, the hovering one flies (blitted as the hovering one is). On
	// foot under a sylph, a move becomes the flying move at once.
	MCreature mover;
	Place(mover, 7);
	mover.AddEffectStatus(EFFECTSTATUS_SUMMON_SYLPH_RED, 50);
	mover.SetMoveDevice(MCreature::MOVE_DEVICE_WALK);
	mover.SetAction(ACTION_MOVE);
	CHECK_EQ(ACTION_OUSTERS_FAST_MOVE_MOVE, int(mover.GetAction()));
	MAttachEffect* pHover = mover.CreateAttachEffect(EFFECTSPRITETYPE_FAST_MOVE_STOP, 50, 50);
	calls.clear();
	mover.UpdateAttachEffect();
	CHECK(Same({"max frame|" + N(BLT_SCREEN) + "|42"}));
	CHECK_EQ(int(EFFECTSPRITETYPE_FAST_MOVE_FLY), int(pHover->GetEffectSpriteType()));
}

TEST(Creature, AnAttachedEffectHandsOnAtItsLinkFrame)
{
	Scene scene;
	scene.Tribe(5, CREATURETRIBE_SLAYER);
	scene.effectSprites.GetMutable(20)->FrameID = 33;
	MActionInfo& info = CastRow();
	info.Init(2);
	for (int phase = 0; phase < 2; ++phase)
	{
		info.GetMutable(phase)->EffectGeneratorID = 7;
		info.GetMutable(phase)->Count = 5;
	}
	const DWORD oldFrame = g_CurrentFrame;
	for (bool ground : {false, true})
	{
		MCreature creature;
		Place(creature);
		g_CurrentFrame = 100;
		// Lasting ten frames, handing on after three.
		MAttachEffect* pEffect = creature.CreateAttachEffect(20, 10, 3, ground ? TRUE : FALSE);
		MEffectTarget* pTarget = new MEffectTarget(2);
		pTarget->Set(0, 0, 0, 900);
		pTarget->NextPhase();
		pEffect->SetLink(Cast, pTarget);
		calls.clear();
		g_CurrentFrame = 101;
		creature.UpdateAttachEffect();
		CHECK(Same({}));
		g_CurrentFrame = 102;
		creature.UpdateAttachEffect();
		CHECK(!calls.empty() && calls[0].rfind("effect|" + N(Cast) + "|", 0) == 0);
	}
	g_CurrentFrame = oldFrame;
}

//----------------------------------------------------------------------
// Construction
//----------------------------------------------------------------------
TEST(Creature, ANewCreatureIsNotInTheParty)
{
	Scene scene;
	MParty* oldParty = g_pParty;
	g_pParty = NULL;
	// The pool hands out used memory: build one over leftover bytes. The
	// constructor sets the status, which asks the party flag. Reading the
	// leftover flag is undefined (UBSan stops on it); with no name yet, the
	// party lookup it led to found nobody, so nothing else showed.
	alignas(MCreature) unsigned char storage[sizeof(MCreature)];
	std::memset(storage, 0xFF, sizeof storage);
	MCreature* pCreature = ::new (storage) MCreature;
	CHECK(!pCreature->IsPlayerParty());
	pCreature->~MCreature();
	g_pParty = oldParty;
}
