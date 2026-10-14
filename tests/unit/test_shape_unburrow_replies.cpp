#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "MEffectStatusTable.h"
#include "MSkillManager.h"
#include "PlayerStateHost.h"
#include "SkillDef.h"
#include "SoundDef.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCChangeShape.h"
#include "Gpackets/GCRemoveEffect.h"
#include "Gpackets/GCUnburrowOK.h"
#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

// The player comes up from a burrow at the cell the server names, with the
// burrowing effect. Effect statuses leave a creature standing in the zone or
// lying in its corpse: a sweeper's end sounds, a fade-out fades back in,
// and the player's aura shield is ready again at once. A creature's shape
// changes by an addon item made for the packet: a creature wearing gear
// stops, puts on a copy and takes the weapon speed; the item is freed.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool hasPlayer = true;
PlayerState::WAIT_VERIFY waitVerify = PlayerState::WAIT_VERIFY_NULL;

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	std::string name;
	TYPE_SECTORPOSITION x = 0;
	TYPE_SECTORPOSITION y = 0;
	int direction = 0;
	bool wear = true;
};
std::map<TYPE_OBJECTID, FakeCreature*> creatures;
// Corpse item ID -> the creature lying in it.
std::map<TYPE_OBJECTID, FakeCreature*> corpses;
FakeCreature me{7, "Self"};

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* pCreature)
{
	static FakeCreature none;
	CHECK(pCreature != nullptr);
	return pCreature == nullptr ? none : *reinterpret_cast<FakeCreature*>(pCreature);
}
void Record(MCreature* p, const std::string& what) { calls.push_back(Fake(p).name + "." + what); }

int alive = 0;
struct Sword : MItem
{
	Sword() { alive++; }
	~Sword() { alive--; }
	ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_SWORD; }
};
MonotonicClock::TimePoint now{MonotonicClock::Duration(1000)};
const MItemHost items = []() {
	MItemHost host{};
	host.pCurrentTime = &now;
	host.NewItem = [](ITEM_CLASS itemClass) -> MItem* {
		calls.push_back("new|" + N(itemClass));
		return itemClass == ITEM_CLASS_SWORD ? new Sword : nullptr;
	};
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) -> MCreature* {
		CHECK(zoneExists);
		const auto found = creatures.find(id);
		return found == creatures.end() ? nullptr : Handle(*found->second);
	};
	host.GetCorpseCreature = [](TYPE_OBJECTID id) -> MCreature* {
		CHECK(zoneExists);
		const auto found = corpses.find(id);
		return found == corpses.end() ? nullptr : Handle(*found->second);
	};
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.GetDirection = [](MCreature* p) { return Fake(p).direction; };
	host.SetServerPosition = [](MCreature* p, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY) { Record(p, "server|" + N(sX) + "," + N(sY)); };
	host.MovePosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		Record(p, "move|" + N(x) + "," + N(y));
		Fake(p).x = x;
		Fake(p).y = y;
		return true;
	};
	host.SetDirection = [](MCreature* p, int direction) { Record(p, "dir|" + N(direction)); };
	host.RemoveEffectStatus = [](MCreature* p, EFFECTSTATUS status) {
		Record(p, "remove|" + N(status));
		return true;
	};
	host.SetVisibleSoon = [](MCreature* p) { Record(p, "visible"); };
	host.IsWear = [](MCreature* p) { return Fake(p).wear; };
	host.SetStop = [](MCreature* p) { Record(p, "stop"); };
	host.SetAddonItem = [](MCreature* p, MItem* pItem) {
		std::string options;
		for (TYPE_ITEM_OPTION option : pItem->GetItemOptionList()) options += "+" + N(option);
		Record(p, "addon|" + N(pItem->GetItemClass()) + "|" + N(pItem->GetItemType()) + "|" + options
			+ (pItem->IsQuestItem() ? "|quest" : ""));
		return true;
	};
	host.SetWeaponSpeed = [](MCreature* p, int speed) { Record(p, "speed|" + N(speed)); };
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? Handle(me) : nullptr; };
	host.GetID = []() { return me.id; };
	host.GetWaitVerify = []() { return waitVerify; };
	host.SetWaitVerifyNULL = []() {
		calls.push_back("verified");
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
	};
	host.SetGroundCreature = []() { calls.push_back("ground"); };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.PlaySound = [](TYPE_SOUNDID soundID) { calls.push_back("sound|" + N(soundID)); };
	host.ExecuteActionInfoFromMainNode = [](TYPE_ACTIONINFO actionInfo, TYPE_SECTORPOSITION startX, TYPE_SECTORPOSITION startY, int startZ, int direction,
		TYPE_OBJECTID targetID, TYPE_SECTORPOSITION targetX, TYPE_SECTORPOSITION targetY, int targetZ, std::uint32_t delayFrame,
		MActionResult* pResult, bool bStartFromMainNode, int sX, int sY) {
		CHECK(pResult == nullptr);
		CHECK(startZ == 0 && targetZ == 0 && sX == 0 && sY == 0);
		calls.push_back("action|" + N(actionInfo) + "|" + N(startX) + "," + N(startY) + "|dir " + N(direction) + "|" + N(targetID)
			+ "@" + N(targetX) + "," + N(targetY) + "|" + N(delayFrame) + (bStartFromMainNode ? "|main" : "|resume"));
	};
	return host;
}();

struct World : GameModelWorld
{
	FakeCreature ogre, lying, wolf;
	EFFECTSTATUS_TABLE statuses;
	MSkillInfoTable skills;
	EFFECTSTATUS_TABLE* oldStatuses = g_pEffectStatusTable;
	MSkillInfoTable* oldSkills = g_pSkillInfoTable;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	World()
	{
		me = {7, "Self", 30, 31, 6};
		ogre = {1001, "Ogre", 10, 11, 3};
		lying = {5001, "Lying", 20, 21, 5};
		wolf = {1002, "Wolf", 12, 13, 1, false};
		creatures = {{ogre.id, &ogre}, {wolf.id, &wolf}, {me.id, &me}};
		corpses = {{9001, &lying}};
		zoneExists = true;
		hasPlayer = true;
		waitVerify = PlayerState::WAIT_VERIFY_NULL;
		calls.clear();
		alive = 0;
		statuses.Init(EFFECTSTATUS_MAX);
		g_pEffectStatusTable = &statuses;
		g_pSkillInfoTable = &skills;
		MItem::SetHost(&items);
	}
	~World()
	{
		MItem::SetHost(nullptr);
		WorldEvents::SetHost(oldEvents);
		PlayerState::SetHost(oldPlayer);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		g_pSkillInfoTable = oldSkills;
		g_pEffectStatusTable = oldStatuses;
		creatures.clear();
		corpses.clear();
	}
	EFFECTSTATUS_NODE& Row(EFFECTSTATUS status) { return testfw::MutableRow(statuses, status); }
	SKILLINFO_NODE& Skill(ACTIONINFO skill) { return *skills.GetMutable(skill); }
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

void ReceiveUnburrow()
{
	GCUnburrowOK original;
	original.setX(40);
	original.setY(41);
	original.setDir(2);
	GCUnburrowOK packet;
	Decode(packet, original);
	GCUnburrowOKHandler::execute(&packet, nullptr);
}

void ReceiveRemoval(ObjectID_t id, std::initializer_list<EFFECTSTATUS> removed)
{
	GCRemoveEffect original;
	original.setObjectID(id);
	for (EFFECTSTATUS status : removed) original.addEffectList(static_cast<EffectID_t>(status));
	GCRemoveEffect packet;
	Decode(packet, original);
	GCRemoveEffectHandler::execute(&packet, nullptr);
}

void ReceiveShape(ObjectID_t id, int itemClass, BYTE flag = 0, OptionType_t option = 3)
{
	GCChangeShape original;
	original.setObjectID(id);
	original.setItemClass(static_cast<BYTE>(itemClass));
	original.setItemType(4);
	original.setOptionType(option);
	original.setAttackSpeed(25);
	original.setFlag(flag);
	GCChangeShape packet;
	Decode(packet, original);
	GCChangeShapeHandler::execute(&packet, nullptr);
}

std::vector<std::string> Unburrowed(bool verified)
{
	std::vector<std::string> out;
	if (verified) out.push_back("verified");
	for (const char* step : {"Self.server|40,41", "Self.move|40,41", "Self.dir|2", "ground"}) out.push_back(step);
	out.push_back("action|" + N(RESULT_MAGIC_HIDE) + "|40,41|dir 6|" + N(OBJECTID_NULL) + "@40,41|0|resume");
	return out;
}

} // namespace

TEST(ShapeUnburrowReplies, ThePlayerComesUpAtTheServersCellWithTheBurrowingEffect)
{
	World world;
	waitVerify = PlayerState::WAIT_VERIFY_SKILL_SUCCESS;
	ReceiveUnburrow();
	CHECK(calls == Unburrowed(true));
	// Without a skill waiting, the same, unverified.
	calls.clear();
	me.x = 30;
	me.y = 31;
	waitVerify = PlayerState::WAIT_VERIFY_LOGOUT;
	ReceiveUnburrow();
	CHECK(calls == Unburrowed(false));
	CHECK(waitVerify == PlayerState::WAIT_VERIFY_LOGOUT);
	// Without a player, nothing.
	calls.clear();
	hasPlayer = false;
	ReceiveUnburrow();
	CHECK(calls.empty());
}

TEST(ShapeUnburrowReplies, EffectStatusesLeaveACreatureStandingOrLyingInItsCorpse)
{
	World world;
	ReceiveRemoval(1001, {EFFECTSTATUS_KEEP_SWEEPER_3, EFFECTSTATUS_CURSE_PARALYSIS, EFFECTSTATUS_FADE_OUT});
	CHECK(calls == std::vector<std::string>({"sound|" + N(SOUND_SWEEPER_END), "Ogre.remove|" + N(EFFECTSTATUS_KEEP_SWEEPER_3),
		"Ogre.remove|" + N(EFFECTSTATUS_CURSE_PARALYSIS), "Ogre.remove|" + N(EFFECTSTATUS_FADE_OUT), "Ogre.visible"}));
	// Every sweeper status sounds its end; the statuses around them do not.
	for (int status = EFFECTSTATUS_KEEP_SWEEPER - 1; status <= EFFECTSTATUS_KEEP_SWEEPER_12 + 1; status++)
	{
		calls.clear();
		ReceiveRemoval(1001, {static_cast<EFFECTSTATUS>(status)});
		const bool sweeper = status >= EFFECTSTATUS_KEEP_SWEEPER && status <= EFFECTSTATUS_KEEP_SWEEPER_12;
		CHECK_EQ(sweeper ? 2u : 1u, calls.size());
	}
	calls.clear();
	ReceiveRemoval(9001, {EFFECTSTATUS_FADE_OUT});
	CHECK(calls == std::vector<std::string>({"Lying.remove|" + N(EFFECTSTATUS_FADE_OUT), "Lying.visible"}));
	// Nobody by that ID, or no zone: nothing.
	calls.clear();
	ReceiveRemoval(4242, {EFFECTSTATUS_FADE_OUT});
	zoneExists = false;
	ReceiveRemoval(1001, {EFFECTSTATUS_FADE_OUT});
	CHECK(calls.empty());
}

TEST(ShapeUnburrowReplies, ThePlayersAuraShieldIsReadyAgainWhenItsStatusLeaves)
{
	World world;
	world.Row(EFFECTSTATUS_CURSE_PARALYSIS).OriginalActionInfo = MAGIC_AURA_SHIELD;
	world.Row(EFFECTSTATUS_FADE_OUT).OriginalActionInfo = MAGIC_HIDE;
	world.Skill(MAGIC_AURA_SHIELD).SetAvailableTime(5000);
	world.Skill(MAGIC_HIDE).SetAvailableTime(5000);
	CHECK(!world.Skill(MAGIC_AURA_SHIELD).IsAvailableTime());
	// On another creature, the player's shield waits on.
	ReceiveRemoval(1001, {EFFECTSTATUS_CURSE_PARALYSIS});
	CHECK(!world.Skill(MAGIC_AURA_SHIELD).IsAvailableTime());
	// Only the shield's status frees it; another skill waits on.
	ReceiveRemoval(7, {EFFECTSTATUS_FADE_OUT});
	CHECK(!world.Skill(MAGIC_AURA_SHIELD).IsAvailableTime());
	CHECK(!world.Skill(MAGIC_HIDE).IsAvailableTime());
	ReceiveRemoval(7, {EFFECTSTATUS_CURSE_PARALYSIS});
	CHECK(world.Skill(MAGIC_AURA_SHIELD).IsAvailableTime());
	CHECK(!world.Skill(MAGIC_HIDE).IsAvailableTime());
	CHECK(calls.back() == "Self.remove|" + N(EFFECTSTATUS_CURSE_PARALYSIS));
}

TEST(ShapeUnburrowReplies, AWearingCreatureStopsAndPutsTheAddonOnWithTheWeaponSpeed)
{
	World world;
	ReceiveShape(1001, ITEM_CLASS_SWORD, SHAPE_FLAG_QUEST);
	CHECK(calls == std::vector<std::string>({"new|" + N(ITEM_CLASS_SWORD), "Ogre.stop",
		"Ogre.addon|" + N(ITEM_CLASS_SWORD) + "|4|+3|quest", "Ogre.speed|25"}));
	CHECK_EQ(0, alive);
	// No quest flag, no option.
	calls.clear();
	ReceiveShape(1001, ITEM_CLASS_SWORD, 0x02, 0);
	CHECK(calls == std::vector<std::string>({"new|" + N(ITEM_CLASS_SWORD), "Ogre.stop",
		"Ogre.addon|" + N(ITEM_CLASS_SWORD) + "|4|", "Ogre.speed|25"}));
	CHECK_EQ(0, alive);
}

TEST(ShapeUnburrowReplies, TheShapesItemIsFreedWhenNobodyWearsIt)
{
	World world;
	ReceiveShape(1002, ITEM_CLASS_SWORD);
	ReceiveShape(4242, ITEM_CLASS_SWORD);
	zoneExists = false;
	ReceiveShape(1001, ITEM_CLASS_SWORD);
	CHECK(calls == std::vector<std::string>(3, "new|" + N(ITEM_CLASS_SWORD)));
	CHECK_EQ(0, alive);
	// An item class nothing makes changes nothing.
	calls.clear();
	zoneExists = true;
	ReceiveShape(1001, ITEM_CLASS_HELM);
	CHECK(calls == std::vector<std::string>({"new|" + N(ITEM_CLASS_HELM)}));
}
