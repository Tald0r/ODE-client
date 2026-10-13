#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "MActionInfoTable.h"
#include "MActionResult.h"
#include "SkillDef.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCAttackArmsOK3.h"
#include "Gpackets/GCAttackArmsOK5.h"
#include "Gpackets/GCAttackMeleeOK3.h"
#include "Gpackets/GCSkillToObjectOK3.h"
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

// Attacks and skills between creatures in the zone that fall back on the
// attacker's basic action (its weapon's): a gun fired at a cell or at a
// target, a melee blow, and a skill aimed at an object. Six gun skills act
// as themselves instead. With only the target in the zone it is shown
// hit; with only the attacker, attacking nobody. The test host runs each
// handed-over result queue at once, so its nodes show in the record.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	std::string name;
	TYPE_SECTORPOSITION x = 0;
	TYPE_SECTORPOSITION y = 0;
	TYPE_ACTIONINFO basic = 0;
};
std::map<TYPE_OBJECTID, FakeCreature*> creatures;

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* pCreature)
{
	static FakeCreature none;
	CHECK(pCreature != nullptr);
	return pCreature == nullptr ? none : *reinterpret_cast<FakeCreature*>(pCreature);
}
void Record(MCreature* p, const std::string& what) { calls.push_back(Fake(p).name + "." + what); }

void Land(MActionResult* pResult)
{
	if (pResult == nullptr)
	{
		calls.push_back("(no result)");
		return;
	}
	pResult->Execute();
	delete pResult;
}

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) -> MCreature* {
		CHECK(zoneExists);
		const auto found = creatures.find(id);
		return found == creatures.end() ? nullptr : Handle(*found->second);
	};
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.GetBasicActionInfo = [](MCreature* p) { return Fake(p).basic; };
	host.SetDirectionToPosition = [](MCreature* p, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) { Record(p, "face|" + N(sX) + "|" + N(sY)); };
	host.SetEffectDelayFrame = [](MCreature* p, TYPE_ACTIONINFO ai, std::uint32_t frames) { Record(p, "delay|" + N(ai) + "|" + N(frames)); };
	host.PacketSpecialActionResult = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t) {
		Record(p, "result|" + N(ai) + "|" + N(id) + "|" + N(sX) + "|" + N(sY));
	};
	host.PacketSpecialActionToOther = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_OBJECTID id, MActionResult* pResult) {
		Record(p, "other|" + N(ai) + "|" + N(id));
		Land(pResult);
	};
	host.PacketSpecialActionToNobody = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		Record(p, "nobody|" + N(ai) + "|" + N(x) + "|" + N(y));
	};
	return host;
}();

constexpr TYPE_ACTIONINFO kMin = MIN_RESULT_ACTIONINFO;
// The attacker's basic action, apart from any constant the replies name.
constexpr TYPE_ACTIONINFO kGun = SKILL_ATTACK_GUN_SR;
const std::initializer_list<TYPE_ACTIONINFO> kOwnGunSkills = {SKILL_JABBING_VEIN, SKILL_MOLE_SHOT, SKILL_TRIDENT,
	SKILL_QUICK_FIRE, SKILL_ULTIMATE_BLOW, SKILL_HARPOON_BOMB};

struct World : GameModelWorld
{
	FakeCreature ogre, vanda;
	MActionInfoTable actions;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	World()
	{
		ogre = {1001, "Ogre", 10, 11, SKILL_ATTACK_MELEE};
		vanda = {1002, "Vanda", 20, 21, kGun};
		creatures = {{ogre.id, &ogre}, {vanda.id, &vanda}};
		zoneExists = true;
		calls.clear();
		actions.Init(MAX_ACTIONINFO);
		actions.SetMinResultActionInfo(kMin);
		g_pActionInfoTable = &actions;
	}
	~World()
	{
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		g_pActionInfoTable = oldActions;
		creatures.clear();
	}
};

template<class PacketType>
void Decode(PacketType& packet, const PacketType& original)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	packet.read(stream);
	CHECK(stream.isEmpty());
}

void ReceiveGunAtCell(ObjectID_t user, SkillType_t skill, Coord_t x, Coord_t y)
{
	GCAttackArmsOK3 original;
	original.setObjectID(user);
	original.setTargetXY(x, y);
	original.setSkillType(skill);
	GCAttackArmsOK3 packet;
	Decode(packet, original);
	GCAttackArmsOK3Handler::execute(&packet, nullptr);
}

void ReceiveGunAtTarget(ObjectID_t user, ObjectID_t target, SkillType_t skill, bool success = true)
{
	GCAttackArmsOK5 original;
	original.setObjectID(user);
	original.setTargetObjectID(target);
	original.setSkillSuccess(success);
	original.setSkillType(skill);
	GCAttackArmsOK5 packet;
	Decode(packet, original);
	GCAttackArmsOK5Handler::execute(&packet, nullptr);
}

void ReceiveMelee(ObjectID_t user, ObjectID_t target)
{
	GCAttackMeleeOK3 original;
	original.setObjectID(user);
	original.setTargetObjectID(target);
	GCAttackMeleeOK3 packet;
	Decode(packet, original);
	GCAttackMeleeOK3Handler::execute(&packet, nullptr);
}

void ReceiveObjectSkill(ObjectID_t user, SkillType_t skill, Coord_t x, Coord_t y, BYTE grade = 0)
{
	GCSkillToObjectOK3 original;
	original.setObjectID(user);
	original.setSkillType(skill);
	original.setTargetXY(x, y);
	original.setGrade(grade);
	GCSkillToObjectOK3 packet;
	Decode(packet, original);
	GCSkillToObjectOK3Handler::execute(&packet, nullptr);
}

// The hit an action-info node lands on the ogre, at its cell.
std::vector<std::string> Hit(TYPE_ACTIONINFO action)
{
	return {"Ogre.delay|" + N(action + kMin) + "|0", "Ogre.result|" + N(action + kMin) + "|1001|10|11"};
}

std::vector<std::string> Then(std::vector<std::string> first, const std::vector<std::string>& rest)
{
	first.insert(first.end(), rest.begin(), rest.end());
	return first;
}

} // namespace

TEST(BasicActionReplies, AGunAtACellFiresTheWeaponOrOneOfSixSkills)
{
	World world;
	ReceiveGunAtCell(1002, SKILL_DOUBLE_IMPACT, 30, 31);
	CHECK(calls == std::vector<std::string>({"Vanda.face|30|31", "Vanda.nobody|" + N(kGun) + "|30|31"}));
	for (TYPE_ACTIONINFO skill : kOwnGunSkills)
	{
		calls.clear();
		ReceiveGunAtCell(1002, skill, 30, 31);
		CHECK(calls == std::vector<std::string>({"Vanda.face|30|31", "Vanda.nobody|" + N(skill) + "|30|31"}));
	}
	calls.clear();
	ReceiveGunAtCell(4242, SKILL_DOUBLE_IMPACT, 30, 31);
	zoneExists = false;
	ReceiveGunAtCell(1002, SKILL_DOUBLE_IMPACT, 30, 31);
	CHECK(calls.empty());
}

TEST(BasicActionReplies, AGunAtATargetHitsItWithTheWeaponOrOneOfSixSkills)
{
	World world;
	ReceiveGunAtTarget(1002, 1001, SKILL_DOUBLE_IMPACT);
	CHECK(calls == Then({"Vanda.face|10|11", "Vanda.other|" + N(kGun) + "|1001"}, Hit(kGun)));
	for (TYPE_ACTIONINFO skill : kOwnGunSkills)
	{
		calls.clear();
		ReceiveGunAtTarget(1002, 1001, skill);
		CHECK(calls == Then({"Vanda.face|10|11", "Vanda.other|" + N(skill) + "|1001"}, Hit(skill)));
	}
	// A miss acts without a result.
	calls.clear();
	ReceiveGunAtTarget(1002, 1001, SKILL_DOUBLE_IMPACT, false);
	CHECK(calls == std::vector<std::string>({"Vanda.face|10|11", "Vanda.other|" + N(kGun) + "|1001", "(no result)"}));
}

TEST(BasicActionReplies, AGunAtATargetWithOnlyOneOfThemHereShowsThatOne)
{
	World world;
	// Every own-action skill on each side: only the target is hit by the
	// skill's own result; only the attacker fires four of them as
	// themselves and Quick Fire and Harpoon Bomb as its weapon.
	for (TYPE_ACTIONINFO skill : kOwnGunSkills)
	{
		calls.clear();
		ReceiveGunAtTarget(4242, 1001, skill);
		ReceiveGunAtTarget(1002, 4242, skill);
		const bool asWeapon = skill == SKILL_QUICK_FIRE || skill == SKILL_HARPOON_BOMB;
		CHECK(calls == std::vector<std::string>({"Ogre.result|" + N(skill + kMin) + "|1001|10|11",
			"Vanda.nobody|" + N(asWeapon ? kGun : skill) + "|20|21"}));
	}
	calls.clear();
	// Only the target: hit by the skill, or by a rifle.
	ReceiveGunAtTarget(4242, 1001, SKILL_TRIDENT);
	ReceiveGunAtTarget(4242, 1001, SKILL_DOUBLE_IMPACT);
	// Only the attacker: four of the six skills fire as themselves at its
	// own cell; Quick Fire and Harpoon Bomb fire the weapon.
	ReceiveGunAtTarget(1002, 4242, SKILL_TRIDENT);
	ReceiveGunAtTarget(1002, 4242, SKILL_QUICK_FIRE);
	ReceiveGunAtTarget(1002, 4242, SKILL_HARPOON_BOMB);
	CHECK(calls == std::vector<std::string>({"Ogre.result|" + N(SKILL_TRIDENT + kMin) + "|1001|10|11",
		"Ogre.result|" + N(RESULT_SKILL_ATTACK_GUN_AR) + "|1001|10|11",
		"Vanda.nobody|" + N(SKILL_TRIDENT) + "|20|21", "Vanda.nobody|" + N(kGun) + "|20|21",
		"Vanda.nobody|" + N(kGun) + "|20|21"}));
	calls.clear();
	ReceiveGunAtTarget(4242, 4243, SKILL_TRIDENT);
	zoneExists = false;
	ReceiveGunAtTarget(1002, 1001, SKILL_TRIDENT);
	CHECK(calls.empty());
}

TEST(BasicActionReplies, AMeleeBlowUsesTheAttackersBasicAction)
{
	World world;
	ReceiveMelee(1002, 1001);
	CHECK(calls == Then({"Vanda.face|10|11", "Vanda.other|" + N(kGun) + "|1001"}, Hit(kGun)));
	calls.clear();
	ReceiveMelee(4242, 1001);
	ReceiveMelee(1002, 4242);
	CHECK(calls == std::vector<std::string>({"Ogre.result|" + N(RESULT_SKILL_ATTACK_MELEE) + "|1001|10|11",
		"Vanda.nobody|" + N(kGun) + "|20|21"}));
	calls.clear();
	ReceiveMelee(4242, 4243);
	zoneExists = false;
	ReceiveMelee(1002, 1001);
	CHECK(calls.empty());
}

TEST(BasicActionReplies, AnObjectSkillIsShownTowardsItsCell)
{
	World world;
	ReceiveObjectSkill(1002, SKILL_DOUBLE_IMPACT, 40, 41);
	CHECK(calls == std::vector<std::string>({"Vanda.face|40|41", "Vanda.nobody|" + N(SKILL_DOUBLE_IMPACT) + "|40|41"}));
	// A graded skill acts as its step.
	calls.clear();
	auto& row = testfw::MutableRow(world.actions, SKILL_DOUBLE_IMPACT);
	row.SetUseActionStep();
	row.SetActionStep(1, SKILL_SINGLE_BLOW);
	ReceiveObjectSkill(1002, SKILL_DOUBLE_IMPACT, 40, 41, 2);
	// Grade 0 takes no step.
	ReceiveObjectSkill(1002, SKILL_DOUBLE_IMPACT, 40, 41, 0);
	CHECK(calls == std::vector<std::string>({"Vanda.face|40|41", "Vanda.nobody|" + N(SKILL_SINGLE_BLOW) + "|40|41",
		"Vanda.face|40|41", "Vanda.nobody|" + N(SKILL_DOUBLE_IMPACT) + "|40|41"}));
}

TEST(BasicActionReplies, AWeaponSkillUsesTheBasicActionUnlessItIsAResult)
{
	World world;
	testfw::MutableRow(world.actions, SKILL_DOUBLE_IMPACT).SetAffectCurrentWeaponAction();
	ReceiveObjectSkill(1002, SKILL_DOUBLE_IMPACT, 40, 41);
	CHECK(calls == std::vector<std::string>({"Vanda.face|40|41", "Vanda.nobody|" + N(kGun) + "|40|41"}));
	calls.clear();
	world.vanda.basic = kMin;
	ReceiveObjectSkill(1002, SKILL_DOUBLE_IMPACT, 40, 41);
	CHECK(calls.empty());
}

TEST(BasicActionReplies, WithoutTheBasicActionEntryAWeaponSkillDoesNothing)
{
	World world;
	testfw::MutableRow(world.actions, SKILL_DOUBLE_IMPACT).SetAffectCurrentWeaponAction();
	CreatureState::Host partial = creature;
	partial.GetBasicActionInfo = nullptr;
	CreatureState::SetHost(&partial);
	// The basic action reads as ACTIONINFO_NULL, which is past the results.
	ReceiveObjectSkill(1002, SKILL_DOUBLE_IMPACT, 40, 41);
	CHECK(calls.empty());
	CreatureState::SetHost(&creature);
}

TEST(BasicActionReplies, AnObjectSkillNeedsAZoneItsUserAndASkillInTheTable)
{
	World world;
	ReceiveObjectSkill(4242, SKILL_DOUBLE_IMPACT, 1, 1);
	ReceiveObjectSkill(1002, MAX_ACTIONINFO, 1, 1);
	zoneExists = false;
	ReceiveObjectSkill(1002, SKILL_DOUBLE_IMPACT, 1, 1);
	CHECK(calls.empty());
}
