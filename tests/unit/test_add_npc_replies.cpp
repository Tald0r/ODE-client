#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "EffectSpriteTypeDef.h"
#include "MCreatureSpriteTable.h"
#include "MCreatureTable.h"
#include "MGuildType.h"
#include "MStatus.h"
#include "ZoneFlags.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCAddNPC.h"
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

// An NPC appearing. When the zone has no creature of the ID, an NPC is
// made: in the guild its sprite says (Eve for a slayer's, the ousters'
// for an ouster's, otherwise Tepez in zones 1007, 1114 and 1115 and
// Bathory elsewhere, the zone being the one the player is in), of the
// NPC ID's type, walking, with its ID, cell and direction, standing,
// named by the packet in zones 1500-1506 and by the creature table
// elsewhere, in the packet's colours, with its NPC ID and full health of
// 100 (Bathory, NPC 217, with 10 of it and an alignment of -10000). The
// zone takes it, and it is dressed as its NPC ID wears and, for types
// 638 and 657 or 639 and 654, takes Amata's or Ripaty's lasting effect;
// if the zone refuses it, it is deleted. An NPC the zone knows walks to
// the cell, faces the direction, stands and takes the colours; another
// creature, nothing. Without a zone nothing happens.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
bool zoneTakes = true;

struct FakeCreature;
std::string Who(const FakeCreature* p);

// The creature's status array, recording each value set: SetStatus is
// virtual, as MCreature's own is in the game.
struct RecordingStatus : MStatus
{
	FakeCreature* owner = nullptr;
	void SetStatus(DWORD n, DWORD value) override;
};

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	int type = 0;
	bool npc = true;
	RecordingStatus status;
	FakeCreature() { status.owner = this; }
	FakeCreature(const FakeCreature&) = delete;
	FakeCreature& operator=(const FakeCreature&) = delete;
};
std::map<TYPE_OBJECTID, FakeCreature*> zoneCreatures;
std::vector<std::unique_ptr<FakeCreature>> made;

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature* creature) { return reinterpret_cast<MCreature*>(creature); }
FakeCreature& Fake(MCreature* p) { return *reinterpret_cast<FakeCreature*>(p); }
std::string Who(const FakeCreature* p) { return p->id == 0 ? std::string("new") : N(p->id); }
std::string Who(MCreature* p) { return Who(&Fake(p)); }

void RecordingStatus::SetStatus(DWORD n, DWORD value)
{
	calls.push_back("status|" + Who(owner) + "|" + N(n) + "|" + N(value));
	MStatus::SetStatus(n, value);
}

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) {
		auto it = zoneCreatures.find(id);
		return it == zoneCreatures.end() ? nullptr : Handle(it->second);
	};
	host.NewNPC = []() -> MCreature* {
		made.push_back(std::make_unique<FakeCreature>());
		calls.push_back("new npc");
		return Handle(made.back().get());
	};
	host.AddCreature = [](MCreature* p) {
		calls.push_back("zone adds|" + Who(p));
		if (zoneTakes) zoneCreatures[Fake(p).id] = &Fake(p);
		return zoneTakes;
	};
	host.DeleteCreature = [](MCreature* p) { calls.push_back("deleted|" + Who(p)); };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.IsNPC = [](MCreature* p) { return Fake(p).npc; };
	host.SetGuildNumber = [](MCreature* p, int guild) { calls.push_back("guild|" + Who(p) + "|" + N(guild)); };
	host.SetCreatureType = [](MCreature* p, int type) { calls.push_back("type|" + Who(p) + "|" + N(type)); Fake(p).type = type; };
	host.GetCreatureType = [](MCreature* p) { return Fake(p).type; };
	host.SetGroundCreature = [](MCreature* p) { calls.push_back("walks|" + Who(p)); };
	host.SetID = [](MCreature* p, TYPE_OBJECTID id) { calls.push_back("id|" + N(id)); Fake(p).id = id; };
	host.SetPosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) { calls.push_back("at|" + Who(p) + "|" + N(x) + "," + N(y)); };
	host.MovePosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("moves|" + Who(p) + "|" + N(x) + "," + N(y));
		return true;
	};
	host.SetServerPosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back("server at|" + Who(p) + "|" + N(x) + "," + N(y));
	};
	host.SetDirection = [](MCreature* p, int direction) { calls.push_back("turns|" + Who(p) + "|" + N(direction)); };
	host.SetCurrentDirection = [](MCreature* p, int direction) { calls.push_back("faces|" + Who(p) + "|" + N(direction)); };
	host.SetAction = [](MCreature* p, int action) { calls.push_back("action|" + Who(p) + "|" + N(action)); };
	host.SetName = [](MCreature* p, const char* name) { calls.push_back("name|" + Who(p) + "|" + name); };
	host.SetBodyColor1 = [](MCreature* p, std::uint16_t color) { calls.push_back("main colour|" + Who(p) + "|" + N(color)); };
	host.SetBodyColor2 = [](MCreature* p, std::uint16_t color) { calls.push_back("sub colour|" + Who(p) + "|" + N(color)); };
	host.SetNPCID = [](MCreature* p, int npcID) { calls.push_back("npc id|" + Who(p) + "|" + N(npcID)); };
	host.Status = [](MCreature* p) -> MStatus* { return &Fake(p).status; };
	host.SetAddonToNPC = [](MCreature* p, int npcID) { calls.push_back("dressed|" + Who(p) + "|" + N(npcID)); };
	host.AttachLastingEffect = [](MCreature* p, int type) { calls.push_back("effect|" + Who(p) + "|" + N(type)); };
	return host;
}();

// NPC IDs by the sprite they are drawn with.
const int SLAYER_NPC = 300;			// a slayer NPC's sprite
const int SLAYER_SPRITE = 301;		// a slayer's sprite not marked an NPC's
const int OUSTERS_NPC = 302;
const int VAMPIRE_NPC = 303;
const int BATHORY = 217;

struct World : GameModelWorld
{
	FakeCreature known;
	FakeCreature monster;
	CREATURE_TABLE creatureTypes;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	CREATURESPRITE_TABLE sprites;
	CREATURESPRITE_TABLE* oldSprites = g_pCreatureSpriteTable;
	const int oldLarge = g_nZoneLarge;
	const int oldSmall = g_nZoneSmall;
	const bool oldInLarge = g_bZonePlayerInLarge;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	World()
	{
		calls.clear();
		zoneExists = true;
		zoneTakes = true;
		made.clear();
		zoneCreatures.clear();
		known.id = 1003;
		zoneCreatures[known.id] = &known;
		monster.id = 1004;
		monster.npc = false;
		zoneCreatures[monster.id] = &monster;
		g_nZoneLarge = 60;
		g_nZoneSmall = 1007;
		g_bZonePlayerInLarge = true;
		sprites.Init(5);
		testfw::MutableRow(sprites, 1).CreatureType = FLAG_CREATURESPRITE_SLAYER | FLAG_CREATURESPRITE_NPC;
		testfw::MutableRow(sprites, 2).CreatureType = FLAG_CREATURESPRITE_SLAYER;
		testfw::MutableRow(sprites, 3).CreatureType = FLAG_CREATURESPRITE_OUSTERS | FLAG_CREATURESPRITE_NPC;
		testfw::MutableRow(sprites, 4).CreatureType = FLAG_CREATURESPRITE_VAMPIRE | FLAG_CREATURESPRITE_NPC;
		g_pCreatureSpriteTable = &sprites;
		creatureTypes.Init(700);
		const std::pair<int, int> spriteOf[] = {{SLAYER_NPC, 1}, {SLAYER_SPRITE, 2}, {OUSTERS_NPC, 3}, {VAMPIRE_NPC, 4}, {BATHORY, 4},
			{638, 3}, {657, 3}, {639, 3}, {654, 3}};
		for (const auto& [npcID, sprite] : spriteOf)
		{
			CREATURETABLE_INFO& row = testfw::MutableRow(creatureTypes, npcID);
			row.SpriteTypes.Init(1);
			testfw::MutableRow(row.SpriteTypes, 0) = sprite;
			row.Name = ("NPC " + N(npcID)).c_str();
		}
		g_pCreatureTable = &creatureTypes;
	}
	~World()
	{
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		g_pCreatureTable = oldCreatureTypes;
		g_pCreatureSpriteTable = oldSprites;
		g_bZonePlayerInLarge = oldInLarge;
		g_nZoneSmall = oldSmall;
		g_nZoneLarge = oldLarge;
		zoneCreatures.clear();
		made.clear();
	}
};

void Receive(ObjectID_t id, int npcID)
{
	GCAddNPC packet;
	packet.setObjectID(id);
	packet.setNPCID(static_cast<NPCID_t>(npcID));
	packet.setName("Packet name");
	packet.setSpriteType(9);
	packet.setMainColor(11);
	packet.setSubColor(12);
	packet.setX(30);
	packet.setY(31);
	packet.setDir(5);
	GCAddNPCHandler::execute(&packet, nullptr);
}

std::string Status(const std::string& who, int index, int value)
{
	return "status|" + who + "|" + N(index) + "|" + N(static_cast<DWORD>(value));
}

std::vector<std::string> Then(std::vector<std::string> first, const std::vector<std::string>& rest)
{
	first.insert(first.end(), rest.begin(), rest.end());
	return first;
}

// The NPC made, up to the zone's taking it.
std::vector<std::string> Made(const std::string& id, int npcID, int guild, const std::string& name)
{
	std::vector<std::string> made = {"new npc", "guild|new|" + N(guild), "type|new|" + N(npcID), "walks|new", "id|" + id,
		"at|" + id + "|30,31", "server at|" + id + "|30,31", "turns|" + id + "|5", "faces|" + id + "|5", "action|" + id + "|" + N(ACTION_STAND),
		"name|" + id + "|" + name, "main colour|" + id + "|11", "sub colour|" + id + "|12", "npc id|" + id + "|" + N(npcID),
		Status(id, MODIFY_MAX_HP, 100)};
	if (npcID == BATHORY)
		return Then(made, {Status(id, MODIFY_CURRENT_HP, 10), Status(id, MODIFY_ALIGNMENT, -10000), "zone adds|" + id});
	return Then(made, {Status(id, MODIFY_CURRENT_HP, 100), "zone adds|" + id});
}

} // namespace

TEST(AddNPCReplies, ANewNPCIsMadeAddedAndDressed)
{
	World world;
	Receive(1005, SLAYER_NPC);
	CHECK(calls == Then(Made("1005", SLAYER_NPC, GUILDID_EVE, "NPC 300"), {"dressed|1005|300"}));
	CHECK(zoneCreatures.count(1005) == 1);
}

TEST(AddNPCReplies, ItsGuildIsItsSpritesOrItsZones)
{
	World world;
	const std::pair<int, int> guildOf[] = {{SLAYER_NPC, GUILDID_EVE}, {SLAYER_SPRITE, GUILDID_EVE}, {OUSTERS_NPC, GUILDID_OUSTERS},
		{VAMPIRE_NPC, GUILDID_BATHORY}};
	TYPE_OBJECTID id = 1010;
	for (const auto& [npcID, guild] : guildOf)
	{
		calls.clear();
		Receive(++id, npcID);
		CHECK(calls.size() > 1 && calls[1] == "guild|new|" + N(guild));
	}
	// Tepez's zones are the player's, large or small.
	for (int zoneID : {1007, 1114, 1115, 1006, 1116})
	{
		g_nZoneLarge = zoneID;
		calls.clear();
		Receive(++id, VAMPIRE_NPC);
		const bool tepez = zoneID != 1006 && zoneID != 1116;
		CHECK(calls.size() > 1 && calls[1] == "guild|new|" + N(tepez ? GUILDID_TEPEZ : GUILDID_BATHORY));
	}
	g_nZoneLarge = 60;
	g_bZonePlayerInLarge = false;
	calls.clear();
	Receive(++id, VAMPIRE_NPC);
	CHECK(calls.size() > 1 && calls[1] == "guild|new|" + N(GUILDID_TEPEZ));
}

TEST(AddNPCReplies, ItIsNamedByThePacketInZones1500To1506)
{
	World world;
	TYPE_OBJECTID id = 1010;
	for (int zoneID : {1499, 1500, 1506, 1507})
	{
		g_nZoneLarge = zoneID;
		calls.clear();
		Receive(++id, SLAYER_NPC);
		const bool packetsName = zoneID >= 1500 && zoneID <= 1506;
		CHECK(std::count(calls.begin(), calls.end(), "name|" + N(id) + "|" + (packetsName ? "Packet name" : "NPC 300")) == 1);
	}
}

TEST(AddNPCReplies, BathoryIsWoundedAndTwoPairsTakeTheirEffects)
{
	World world;
	Receive(1005, BATHORY);
	CHECK(calls == Then(Made("1005", BATHORY, GUILDID_BATHORY, "NPC 217"), {"dressed|1005|217"}));
	const std::pair<int, int> effectOf[] = {{638, EFFECTSPRITETYPE_AMATA}, {657, EFFECTSPRITETYPE_AMATA}, {639, EFFECTSPRITETYPE_RIPATY},
		{654, EFFECTSPRITETYPE_RIPATY}};
	TYPE_OBJECTID id = 1010;
	for (const auto& [npcID, effect] : effectOf)
	{
		calls.clear();
		Receive(++id, npcID);
		CHECK(calls == Then(Made(N(id), npcID, GUILDID_OUSTERS, "NPC " + N(npcID)),
			{"dressed|" + N(id) + "|" + N(npcID), "effect|" + N(id) + "|" + N(effect)}));
	}
}

TEST(AddNPCReplies, AnNPCTheZoneRefusesIsDeleted)
{
	World world;
	zoneTakes = false;
	Receive(1005, SLAYER_NPC);
	CHECK(calls == Then(Made("1005", SLAYER_NPC, GUILDID_EVE, "NPC 300"), {"deleted|1005"}));
	calls.clear();
	Receive(1006, 638);
	CHECK(calls.back() == "deleted|1006");
}

TEST(AddNPCReplies, AKnownNPCWalksToTheCellAndStands)
{
	World world;
	Receive(1003, SLAYER_NPC);
	CHECK(calls == std::vector<std::string>({"walks|1003", "server at|1003|30,31", "moves|1003|30,31", "turns|1003|5", "faces|1003|5",
		"action|1003|" + N(ACTION_STAND), "main colour|1003|11", "sub colour|1003|12"}));
	calls.clear();
	Receive(1004, SLAYER_NPC);
	CHECK(calls.empty());
	CHECK(made.empty());
}

TEST(AddNPCReplies, WithoutAZoneOrTheHostsNothingHappens)
{
	World world;
	zoneExists = false;
	Receive(1005, SLAYER_NPC);
	Receive(1003, SLAYER_NPC);
	CHECK(calls.empty());
	zoneExists = true;
	ZoneState::Host cannotMake = zone;
	cannotMake.NewNPC = nullptr;
	ZoneState::SetHost(&cannotMake);
	Receive(1005, SLAYER_NPC);
	CHECK(calls.empty());
	ZoneState::SetHost(&zone);
	CreatureState::Host noStatus = creature;
	noStatus.Status = nullptr;
	CreatureState::SetHost(&noStatus);
	Receive(1006, BATHORY);
	CHECK(std::none_of(calls.begin(), calls.end(), [](const std::string& call) { return call.rfind("status|", 0) == 0; }));
	CHECK(calls.size() > 2 && calls[calls.size() - 2] == "zone adds|1006" && calls.back() == "dressed|1006|217");
	// Without the creature host the NPC is still made and added; one the
	// zone knows is no NPC to it.
	calls.clear();
	CreatureState::SetHost(nullptr);
	Receive(1007, SLAYER_NPC);
	Receive(1003, SLAYER_NPC);
	CHECK(calls == std::vector<std::string>({"new npc", "zone adds|new"}));
	calls.clear();
	ZoneState::SetHost(nullptr);
	Receive(1008, SLAYER_NPC);
	CHECK(calls.empty());
}
