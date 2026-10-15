#include "test_framework.h"
#include "MObjectSelector.h"
#include "MJusticeAttackManager.h"
#include "MWarManager.h"
#include "GuildWarInfo.h"
#include "RaceWarInfo.h"
#include "MZoneTable.h"
#include "MCreatureTable.h"
#include <set>

// Whom the player may select and attack (task 4.225). The creatures are
// fakes the host reads back; the justice and war managers are real.
namespace {

struct Fake
{
	const char* name = "someone";
	bool npc = false;
	int guild = 0;
	int unionGuild = 0;
	std::set<int> effects;
	bool available = true;			// its effect status can be read
	bool playerOnlySprite = false;
	bool tribeEnemy = false;		// the player may attack its race
	bool guildEnemy = false;		// the player may attack its guild
};

MCreature* C(Fake& fake) { return reinterpret_cast<MCreature*>(&fake); }
Fake& F(MCreature* pCreature) { return *reinterpret_cast<Fake*>(pCreature); }

struct World
{
	bool shift = false;
	std::set<int> playerEffects;
	int playerType = 0;
	int playerGuild = 0;
	int playerUnion = 0;
	bool nonPK = false;
	bool zone = true;
	int zoneID = 1201;
	int pkType = PK_TYPE_NULL;
	MJusticeAttackManager justice;
	MWarManager war;
};
World* world = nullptr;

const MObjectSelectorHost host{
	.ForceAttack = []() { return world->shift; },
	.PlayerHasEffectStatus = [](EFFECTSTATUS status) { return world->playerEffects.count(status) != 0; },
	.PlayerCanAttackTribe = [](MCreature* c) { return F(c).tribeEnemy; },
	.PlayerCanAttackGuild = [](MCreature* c) { return F(c).guildEnemy; },
	.PlayerCreatureType = []() { return world->playerType; },
	.PlayerGuildNumber = []() { return world->playerGuild; },
	.PlayerUnionGuildID = []() { return world->playerUnion; },
	.IsNonPK = []() { return world->nonPK; },
	.GetName = [](MCreature* c) { return F(c).name; },
	.IsNPC = [](MCreature* c) { return F(c).npc; },
	.GetGuildNumber = [](MCreature* c) { return F(c).guild; },
	.GetUnionGuildID = [](MCreature* c) { return F(c).unionGuild; },
	.HasEffectStatus = [](MCreature* c, EFFECTSTATUS status) { return F(c).effects.count(status) != 0; },
	.HasAvailableEffectStatus = [](MCreature* c) { return F(c).available; },
	.HasPlayerOnlySprite = [](MCreature* c) { return F(c).playerOnlySprite; },
	.HasZone = []() { return world->zone; },
	.GetZoneID = []() { return world->zoneID; },
	.GetZonePKType = []() { return world->pkType; },
	.JusticeAttackManager = []() { return &world->justice; },
	.WarManager = []() { return &world->war; },
};

struct Fixture
{
	World state;
	MObjectSelector selector{&host};
	Fixture() { world = &state; }
	~Fixture() { world = nullptr; }
	// The zone's castle at war between these guilds.
	void GuildWar(std::initializer_list<int> guilds)
	{
		auto* info = new GuildWarInfo;
		info->setCastleID(static_cast<ZoneID_t>(state.zoneID));
		for (int guild : guilds) info->addJoinGuild(static_cast<GuildID_t>(guild));
		state.war.SetWar(info);
	}
};

} // namespace

TEST(ObjectSelector, WithoutAHostNothing)
{
	MObjectSelector selector;
	Fake creature;
	CHECK(!selector.CanSelect(C(creature)));
	CHECK(!selector.CanAttack(C(creature)));
	CHECK(!selector.IsWarEnemy(C(creature)));
}

TEST(ObjectSelector, ByDefaultEveryoneCanBeSelected)
{
	Fixture f;
	Fake creature;
	CHECK(f.selector.CanSelect(C(creature)));
	f.selector.SelectEnemy();
	CHECK(!f.selector.CanSelect(C(creature)));
	f.selector.SelectAll();
	CHECK(f.selector.CanSelect(C(creature)));
}

TEST(ObjectSelector, ByRaceEnemiesOrFriends)
{
	Fixture f;
	Fake enemy, friendly;
	enemy.tribeEnemy = true;
	enemy.guildEnemy = false;
	friendly.guildEnemy = true;		// by race, the guild does not count
	f.selector.SelectEnemy();
	CHECK(f.selector.CanSelect(C(enemy)));
	CHECK(!f.selector.CanSelect(C(friendly)));
	f.selector.SelectFriend();
	CHECK(!f.selector.CanSelect(C(enemy)));
	CHECK(f.selector.CanSelect(C(friendly)));
}

TEST(ObjectSelector, ByGuildEnemiesOrFriends)
{
	Fixture f;
	Fake enemy, friendly;
	enemy.guildEnemy = true;
	friendly.tribeEnemy = true;		// by guild, the race does not count
	f.selector.SelectByGuild();
	f.selector.SelectEnemy();
	CHECK(f.selector.CanSelect(C(enemy)));
	CHECK(!f.selector.CanSelect(C(friendly)));
	f.selector.SelectFriend();
	CHECK(!f.selector.CanSelect(C(enemy)));
	CHECK(f.selector.CanSelect(C(friendly)));
	f.selector.SelectByRace();
	CHECK(f.selector.CanSelect(C(enemy)));
	CHECK(!f.selector.CanSelect(C(friendly)));
}

TEST(ObjectSelector, TheExceptionsCanAlwaysBeSelected)
{
	Fixture f;
	f.selector.SelectEnemy();
	Fake creature;
	CHECK(!f.selector.CanSelect(C(creature)));

	f.state.shift = true;
	CHECK(f.selector.CanSelect(C(creature)));
	f.state.shift = false;

	f.state.playerEffects = {EFFECTSTATUS_HALLUCINATION};
	CHECK(f.selector.CanSelect(C(creature)));
	f.state.playerEffects.clear();

	f.state.justice.AddCreature("someone");		// a self-defense target
	CHECK(f.selector.CanSelect(C(creature)));
	f.state.justice.RemoveCreature("someone");

	creature.npc = true;
	CHECK(f.selector.CanSelect(C(creature)));
	creature.npc = false;

	f.state.pkType = PK_TYPE_ALL;				// a war enemy
	CHECK(f.selector.CanSelect(C(creature)));
	f.state.pkType = PK_TYPE_NULL;

	for (int type : {CREATURETYPE_SLAYER_OPERATOR, CREATURETYPE_VAMPIRE_OPERATOR, CREATURETYPE_OUSTERS_OPERATOR})
	{
		f.state.playerType = type;
		CHECK(f.selector.CanSelect(C(creature)));
	}
	f.state.playerType = CREATURETYPE_SLAYER_OPERATOR + 1;
	CHECK(!f.selector.CanSelect(C(creature)));
	// The non-PK option does not limit selection.
	f.state.nonPK = true;
	creature.playerOnlySprite = true;
	f.state.shift = true;
	CHECK(f.selector.CanSelect(C(creature)));
}

TEST(ObjectSelector, AttackByRaceOrGuildWhateverIsSelected)
{
	Fixture f;
	Fake enemy, friendly;
	enemy.tribeEnemy = true;
	friendly.guildEnemy = true;
	CHECK(!f.selector.CanAttack(nullptr));
	for (int target = 0; target < 3; ++target)
	{
		if (target == 0) f.selector.SelectAll();
		if (target == 1) f.selector.SelectFriend();
		if (target == 2) f.selector.SelectEnemy();
		f.selector.SelectByRace();
		CHECK(f.selector.CanAttack(C(enemy)));
		CHECK(!f.selector.CanAttack(C(friendly)));
		f.selector.SelectByGuild();
		CHECK(!f.selector.CanAttack(C(enemy)));
		CHECK(f.selector.CanAttack(C(friendly)));
	}
}

TEST(ObjectSelector, TheAttackExceptions)
{
	Fixture f;
	Fake creature;
	CHECK(!f.selector.CanAttack(C(creature)));

	f.state.shift = true;
	CHECK(f.selector.CanAttack(C(creature)));
	f.state.shift = false;

	f.state.playerEffects = {EFFECTSTATUS_HALLUCINATION};
	CHECK(f.selector.CanAttack(C(creature)));
	f.state.playerEffects.clear();

	f.state.justice.AddCreature("someone");
	CHECK(f.selector.CanAttack(C(creature)));
	f.state.justice.RemoveCreature("someone");

	f.state.pkType = PK_TYPE_ALL;
	CHECK(f.selector.CanAttack(C(creature)));
	f.state.pkType = PK_TYPE_NULL;

	// An NPC or an operator may be selected, not attacked for that.
	creature.npc = true;
	f.state.playerType = CREATURETYPE_SLAYER_OPERATOR;
	CHECK(!f.selector.CanAttack(C(creature)));
}

TEST(ObjectSelector, UnderNonPKNoPlayerLookingCreatureIsAttacked)
{
	Fixture f;
	Fake creature;
	creature.tribeEnemy = true;
	creature.playerOnlySprite = true;
	f.state.shift = true;
	CHECK(f.selector.CanAttack(C(creature)));
	f.state.nonPK = true;
	CHECK(!f.selector.CanAttack(C(creature)));		// not even forced
	creature.npc = true;							// an NPC still is
	CHECK(f.selector.CanAttack(C(creature)));
	creature.npc = false;
	creature.playerOnlySprite = false;				// so is a monster
	CHECK(f.selector.CanAttack(C(creature)));
}

TEST(ObjectSelector, AWarEnemyNeedsACreatureAndAZone)
{
	Fixture f;
	f.state.pkType = PK_TYPE_ALL;
	Fake creature;
	CHECK(f.selector.IsWarEnemy(C(creature)));
	CHECK(!f.selector.IsWarEnemy(nullptr));
	f.state.zone = false;
	CHECK(!f.selector.IsWarEnemy(C(creature)));
	f.state.zone = true;
	for (int pk : {PK_TYPE_NULL, PK_TYPE_DISABLE, PK_TYPE_GUILD})
	{
		f.state.pkType = pk;
		CHECK(!f.selector.IsWarEnemy(C(creature)));
	}
}

TEST(ObjectSelector, InASiegeDefendersFightTheOthers)
{
	Fixture f;
	f.state.pkType = PK_TYPE_SIEGE;
	Fake other, defender, reinforcement;
	defender.effects = {EFFECTSTATUS_SIEGE_DEFENDERL};
	reinforcement.effects = {EFFECTSTATUS_SIEGE_REINFORCE};
	CHECK(!f.selector.IsWarEnemy(C(other)));		// the player is on no side
	for (int side : {EFFECTSTATUS_SIEGE_DEFENDERL, EFFECTSTATUS_SIEGE_REINFORCE})
	{
		f.state.playerEffects = {side};
		CHECK(f.selector.IsWarEnemy(C(other)));
		CHECK(!f.selector.IsWarEnemy(C(defender)));
		CHECK(!f.selector.IsWarEnemy(C(reinforcement)));
	}
	// Only a creature whose effect status can be read.
	other.available = false;
	CHECK(!f.selector.IsWarEnemy(C(other)));
}

TEST(ObjectSelector, InASiegeAttackersFightOtherSides)
{
	Fixture f;
	f.state.pkType = PK_TYPE_SIEGE;
	Fake side1, side3, none;
	side1.effects = {EFFECTSTATUS_SIEGE_ATTACKER_1};
	side3.effects = {EFFECTSTATUS_SIEGE_ATTACKER_3};
	const EFFECTSTATUS sides[] = {EFFECTSTATUS_SIEGE_ATTACKER_1, EFFECTSTATUS_SIEGE_ATTACKER_2,
		EFFECTSTATUS_SIEGE_ATTACKER_3, EFFECTSTATUS_SIEGE_ATTACKER_4, EFFECTSTATUS_SIEGE_ATTACKER_5};
	for (EFFECTSTATUS side : sides)
	{
		f.state.playerEffects = {side};
		CHECK(f.selector.IsWarEnemy(C(none)));
		CHECK_EQ(side != EFFECTSTATUS_SIEGE_ATTACKER_1, f.selector.IsWarEnemy(C(side1)) != FALSE);
		CHECK_EQ(side != EFFECTSTATUS_SIEGE_ATTACKER_3, f.selector.IsWarEnemy(C(side3)) != FALSE);
	}
	// Sharing one side does not end the check: another side of the
	// player's that the creature lacks still makes it an enemy.
	f.state.playerEffects = {EFFECTSTATUS_SIEGE_ATTACKER_1, EFFECTSTATUS_SIEGE_ATTACKER_4};
	CHECK(f.selector.IsWarEnemy(C(side1)));
	// A defender's ally check comes first.
	f.state.playerEffects = {EFFECTSTATUS_SIEGE_DEFENDERL, EFFECTSTATUS_SIEGE_ATTACKER_1};
	CHECK(f.selector.IsWarEnemy(C(side1)));
}

TEST(ObjectSelector, InASiegeTheSameUnionIsNoEnemy)
{
	Fixture f;
	f.state.pkType = PK_TYPE_SIEGE;
	f.state.playerEffects = {EFFECTSTATUS_SIEGE_ATTACKER_2};
	Fake ally;
	ally.unionGuild = 7;
	f.state.playerUnion = 7;
	CHECK(!f.selector.IsWarEnemy(C(ally)));
	f.state.playerUnion = 8;
	CHECK(f.selector.IsWarEnemy(C(ally)));
	// No union (0) is no alliance.
	ally.unionGuild = 0;
	f.state.playerUnion = 0;
	CHECK(f.selector.IsWarEnemy(C(ally)));
	// The defender's check comes before the union's.
	f.state.playerEffects = {EFFECTSTATUS_SIEGE_DEFENDERL};
	ally.unionGuild = f.state.playerUnion = 7;
	CHECK(f.selector.IsWarEnemy(C(ally)));
}

TEST(ObjectSelector, InAGuildWarAJoinedGuildFightsEveryOther)
{
	Fixture f;
	Fake mine, other, outsider;
	mine.guild = 10;
	other.guild = 20;
	outsider.guild = 30;
	f.state.playerGuild = 10;
	CHECK(!f.selector.IsWarEnemy(C(other)));		// no war yet
	f.GuildWar({20, 10});
	CHECK(!f.selector.IsWarEnemy(C(mine)));
	CHECK(f.selector.IsWarEnemy(C(other)));
	CHECK(f.selector.IsWarEnemy(C(outsider)));
	// A siege zone with no siege side falls through to the war.
	f.state.pkType = PK_TYPE_SIEGE;
	CHECK(f.selector.IsWarEnemy(C(outsider)));
}

TEST(ObjectSelector, OutsideTheWarOnlyItsGuildsAreEnemies)
{
	Fixture f;
	Fake joined, outsider, mine;
	joined.guild = 20;
	outsider.guild = 30;
	mine.guild = 40;
	f.state.playerGuild = 40;
	f.GuildWar({10, 20});
	CHECK(f.selector.IsWarEnemy(C(joined)));
	CHECK(!f.selector.IsWarEnemy(C(outsider)));
	CHECK(!f.selector.IsWarEnemy(C(mine)));
	// Another zone's war is not this zone's.
	f.state.zoneID = 1202;
	CHECK(!f.selector.IsWarEnemy(C(joined)));
}

TEST(ObjectSelector, ARaceWarIsNoGuildWar)
{
	Fixture f;
	auto* info = new RaceWarInfo;
	info->getCastleIDs().addValue(static_cast<ZoneID_t>(f.state.zoneID));
	f.state.war.SetWar(info);
	CHECK_EQ(1, f.state.war.getSize());
	Fake creature;
	creature.guild = 20;
	CHECK(!f.selector.IsWarEnemy(C(creature)));
}
