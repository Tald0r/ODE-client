#include "test_framework.h"
#include "gamemodel_world.h"
#include "MCreatureTable.h"
#include "MItem.h"
#include "SkillDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCPetUseSkill.h"
#include <string>
#include <vector>

// A creature's pet uses its skill on a target: the target is hit. The pet
// makes its creature type's attack sound, and its skill follows its pet
// item: a tank (type 3) shows the target being shelled and fires its
// level's attack (3 below 40, 4 below 50, 5 from 50), a stirge (type 4)
// drains blood and a pixie (type 5) absorbs a soul at its level's grade
// (1 below 20, then one grade per ten levels up to 5 from 50). The pet's
// turret waits 3 frames and turns to the target; a Centauro turret
// (creature types 702-704) ends facing it, and type 702 fires from the
// muzzle's offset between the way it faced and the way it turns. The
// skill then starts from the pet towards the way it turns.
namespace {

std::vector<std::string> calls;

struct FakeCreature
{
	TYPE_OBJECTID id;
	TYPE_OBJECTID petID;
	int creatureType;
	TYPE_SECTORPOSITION x, y;
	int direction;
	MPetItem* pPetItem;
};
FakeCreature owner{1003, 2001, 1, 10, 11, 0, nullptr};
FakeCreature target{1005, OBJECTID_NULL, 1, 20, 21, 0, nullptr};
FakeCreature pet{2001, OBJECTID_NULL, 2, 12, 13, 1, nullptr};
int facesTarget = 6;

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* p) { return *reinterpret_cast<FakeCreature*>(p); }
std::string Id(MCreature* p) { return N(Fake(p).id); }

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.GetCreature = [](TYPE_OBJECTID id) {
		return id == owner.id ? Handle(owner) : id == target.id ? Handle(target) : nullptr;
	};
	host.GetFakeCreature = [](TYPE_OBJECTID id) { return id == pet.id ? Handle(pet) : nullptr; };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.GetDirection = [](MCreature* p) { return Fake(p).direction; };
	host.GetCreatureType = [](MCreature* p) { return Fake(p).creatureType; };
	host.GetPetID = [](MCreature* p) { return Fake(p).petID; };
	host.GetPetItem = [](MCreature* p) { return Fake(p).pPetItem; };
	host.SetAction = [](MCreature* p, int action) { calls.push_back("acts|" + Id(p) + "|" + N(action)); };
	host.GetDirectionToPosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("faces|" + Id(p) + "|" + N(x) + "," + N(y));
		return facesTarget;
	};
	host.SetTurretDelay = [](MCreature* p, int delay) { calls.push_back("turret waits|" + Id(p) + "|" + N(delay)); };
	host.SetTurretFinalDirection = [](MCreature* p, int direction) { calls.push_back("turret ends|" + Id(p) + "|" + N(direction)); };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int startZ, int direction,
		TYPE_OBJECTID targetID, TYPE_SECTORPOSITION targetX, TYPE_SECTORPOSITION targetY, int targetZ, std::uint32_t delayFrame,
		MActionResult* pResult, bool bStartFromMainNode, int sX, int sY) {
		CHECK(startZ == 0 && targetZ == 0 && delayFrame == 0 && pResult == nullptr);
		calls.push_back("effect|" + N(actionInfo) + "|" + N(startX) + "," + N(startY) + "|dir " + N(direction) + "|" + N(targetID)
			+ "@" + N(targetX) + "," + N(targetY) + (bStartFromMainNode ? "|main" : "") + "|" + N(sX) + "," + N(sY));
	};
	host.PlaySoundAt = [](TYPE_SOUNDID soundID, bool repeat, int x, int y) {
		calls.push_back("sound|" + N(soundID) + (repeat ? "|repeat" : "") + "|" + N(x) + "," + N(y));
	};
	return host;
}();

struct World : GameModelWorld
{
	MPetItem petItem;
	CREATURE_TABLE creatureTypes;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		calls.clear();
		facesTarget = 6;
		owner = FakeCreature{1003, 2001, 1, 10, 11, 0, nullptr};
		target = FakeCreature{1005, OBJECTID_NULL, 1, 20, 21, 0, nullptr};
		pet = FakeCreature{2001, OBJECTID_NULL, 2, 12, 13, 1, &petItem};
		// Rows up to the Centauro turrets (702-704), whose attack sound is read.
		creatureTypes.Init(705);
		if (auto* row = creatureTypes.GetMutable(2))
		{
			row->SetCreatureTribe(CREATURETRIBE_NPC, ACTION_ATTACK + 1);
			row->SetActionSound(ACTION_ATTACK, 1, 88);
		}
		g_pCreatureTable = &creatureTypes;
		Pet(3, 30);
	}
	~World()
	{
		WorldEvents::SetHost(oldEvents);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		g_pCreatureTable = oldCreatureTypes;
	}
	void Pet(int type, int level)
	{
		petItem.SetItemType(static_cast<TYPE_ITEMTYPE>(type));
		petItem.SetNumber(static_cast<TYPE_ITEM_NUMBER>(level));
	}
};

void Receive(ObjectID_t attacker = 1003, ObjectID_t victim = 1005)
{
	GCPetUseSkill packet;
	packet.setAttacker(attacker);
	packet.setTarget(victim);
	GCPetUseSkillHandler::execute(&packet, nullptr);
}

// The skill from the pet (at 12,13) towards the way it turns.
std::string Fired(int skill, int direction = 6, int sx = 0, int sy = 0)
{
	return "effect|" + N(skill) + "|12,13|dir " + N(direction) + "|2001@12,13|" + N(sx) + "," + N(sy);
}

// What every pet skill does after the pet item's part.
std::vector<std::string> Turret(int skill)
{
	return {"turret waits|2001|3", "faces|2001|20,21", Fired(skill)};
}

std::vector<std::string> Then(std::vector<std::string> first, const std::vector<std::string>& rest)
{
	first.insert(first.end(), rest.begin(), rest.end());
	return first;
}

const std::string kHit = "acts|1005|" + std::to_string(ACTION_DAMAGED);
const std::string kSound = "sound|88|12,13";

} // namespace

TEST(PetSkillReplies, ATankShellsTheTargetAtItsLevelsAttack)
{
	World world;
	const std::string shelled = "effect|" + N(SKILL_CLIENT_TANK_ATTACKED) + "|0,0|dir " + N(DIRECTION_DOWN) + "|1005@0,0|0,0";
	const struct { int level; int skill; } levels[] = {
		{29, SKILL_CLIENT_TANK_ATTACK_3}, {30, SKILL_CLIENT_TANK_ATTACK_3}, {39, SKILL_CLIENT_TANK_ATTACK_3},
		{40, SKILL_CLIENT_TANK_ATTACK_4}, {49, SKILL_CLIENT_TANK_ATTACK_4}, {50, SKILL_CLIENT_TANK_ATTACK_5}};
	for (const auto& l : levels)
	{
		calls.clear();
		world.Pet(3, l.level);
		Receive();
		CHECK(calls == Then({kHit, kSound, shelled}, Turret(l.skill)));
	}
}

TEST(PetSkillReplies, AStirgeDrainsAndAPixieAbsorbsAtTheirLevelsGrade)
{
	World world;
	const struct { int level; int stirge; int pixie; } levels[] = {
		{19, SKILL_CLIENT_STIRGE_BLOOD_DRAIN_1, SKILL_CLIENT_PIXIE_ABSORB_SOUL_1},
		{20, SKILL_CLIENT_STIRGE_BLOOD_DRAIN_2, SKILL_CLIENT_PIXIE_ABSORB_SOUL_2},
		{29, SKILL_CLIENT_STIRGE_BLOOD_DRAIN_2, SKILL_CLIENT_PIXIE_ABSORB_SOUL_2},
		{30, SKILL_CLIENT_STIRGE_BLOOD_DRAIN_3, SKILL_CLIENT_PIXIE_ABSORB_SOUL_3},
		{39, SKILL_CLIENT_STIRGE_BLOOD_DRAIN_3, SKILL_CLIENT_PIXIE_ABSORB_SOUL_3},
		{40, SKILL_CLIENT_STIRGE_BLOOD_DRAIN_4, SKILL_CLIENT_PIXIE_ABSORB_SOUL_4},
		{49, SKILL_CLIENT_STIRGE_BLOOD_DRAIN_4, SKILL_CLIENT_PIXIE_ABSORB_SOUL_4},
		{50, SKILL_CLIENT_STIRGE_BLOOD_DRAIN_5, SKILL_CLIENT_PIXIE_ABSORB_SOUL_5}};
	for (const auto& l : levels)
	{
		calls.clear();
		world.Pet(4, l.level);
		Receive();
		const std::string drained = "effect|" + N(l.stirge) + "|0,0|dir " + N(DIRECTION_DOWN) + "|1005@0,0|main|0,0";
		CHECK(calls == Then({kHit, kSound, drained}, Turret(l.stirge)));
		calls.clear();
		world.Pet(5, l.level);
		Receive();
		const std::string absorbed = "effect|" + N(l.pixie) + "|0,0|dir " + N(DIRECTION_DOWN) + "|1005@0,0|0,0";
		CHECK(calls == Then({kHit, kSound, absorbed}, Turret(l.pixie)));
	}
}

TEST(PetSkillReplies, AnotherPetOrNoPetItemFiresTheTanksFirstAttack)
{
	World world;
	world.Pet(2, 50);
	Receive();
	CHECK(calls == Then({kHit, kSound}, Turret(SKILL_CLIENT_TANK_ATTACK_3)));
	calls.clear();
	pet.pPetItem = nullptr;
	Receive();
	CHECK(calls == Then({kHit}, Turret(SKILL_CLIENT_TANK_ATTACK_3)));
	// A pet type without an attack sound makes none.
	calls.clear();
	pet.pPetItem = &world.petItem;
	pet.creatureType = 1;
	Receive();
	CHECK(calls == Then({kHit}, Turret(SKILL_CLIENT_TANK_ATTACK_3)));
}

TEST(PetSkillReplies, ACentauroTurretEndsFacingTheTarget)
{
	World world;
	world.Pet(2, 1);
	for (int type : {703, 704})
	{
		calls.clear();
		pet.creatureType = type;
		Receive();
		CHECK(calls == std::vector<std::string>({kHit, "turret waits|2001|3", "faces|2001|20,21", "turret ends|2001|6",
			Fired(SKILL_CLIENT_TANK_ATTACK_3)}));
	}
	// Type 702 fires from the muzzle's offset: from left-down (8,2) to up
	// (0,12), and none when it already faces that way.
	calls.clear();
	pet.creatureType = 702;
	Receive();
	CHECK(calls == std::vector<std::string>({kHit, "turret waits|2001|3", "faces|2001|20,21", "turret ends|2001|6",
		Fired(SKILL_CLIENT_TANK_ATTACK_3, 6, 8, -10)}));
	calls.clear();
	pet.direction = 4;
	facesTarget = 0;
	Receive();
	CHECK(calls.back() == Fired(SKILL_CLIENT_TANK_ATTACK_3, 0, -21, 0));
	calls.clear();
	pet.direction = 2;
	facesTarget = 2;
	Receive();
	CHECK(calls.back() == Fired(SKILL_CLIENT_TANK_ATTACK_3, 2, 0, 0));
}

TEST(PetSkillReplies, NoAttackerTargetOrPetDoesLess)
{
	World world;
	Receive(4242, 1005);
	CHECK(calls.empty());
	Receive(1003, 4242);
	CHECK(calls.empty());
	// The target is hit even when the pet is not there.
	owner.petID = 2009;
	Receive();
	CHECK(calls == std::vector<std::string>{kHit});
}

TEST(PetSkillReplies, WithoutTheHostsTheTurretIsLeftAlone)
{
	World world;
	CreatureState::SetHost(nullptr);
	CreatureState::SetTurretDelay(Handle(pet), 3);
	CreatureState::SetTurretFinalDirection(Handle(pet), 6);
	ZoneState::SetHost(nullptr);
	Receive();
	CHECK(calls.empty());
}
