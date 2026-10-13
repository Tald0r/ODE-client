#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "ZoneStateHost.h"
#include "PlayerStateHost.h"
#include "ConvertDuration.h"
#include "MActionInfoTable.h"
#include "MShop.h"
#include "MShopShelf.h"
#include "SkillDef.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCAttackArmsOK4.h"
#include "Gpackets/GCGetDamage.h"
#include "Gpackets/GCKnocksTargetBackOK4.h"
#include "Gpackets/GCShopBought.h"
#include "Gpackets/GCSkillToInventoryOK2.h"
#include "Gpackets/GCSkillToObjectOK4.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// Skill and damage results land on creatures in the zone, and on the
// player, through CreatureState: the result action (the skill offset into
// the result range), the effect it leaves and how long, a push to a new
// cell, and an NPC's shop shelf. Creatures are fakes behind opaque handles.
namespace {

std::vector<std::string> calls;

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	std::string name;
	TYPE_SECTORPOSITION x = 0;
	TYPE_SECTORPOSITION y = 0;
	MShop* shop = nullptr;
};

MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
// A handler that passes on a creature it never found fails here, not later.
FakeCreature& Fake(MCreature* pCreature)
{
	static FakeCreature none;
	CHECK(pCreature != nullptr);
	return pCreature == nullptr ? none : *reinterpret_cast<FakeCreature*>(pCreature);
}
bool IsNPC(MCreature* p) { return Fake(p).shop != nullptr || Fake(p).name == "Shopless"; }
std::string N(long long value) { return std::to_string(value); }
std::string Who(MCreature* pCreature) { return Fake(pCreature).name; }

bool zoneExists = true;
std::map<TYPE_OBJECTID, FakeCreature*> creatures;
FakeCreature* self = nullptr;
int alive = 0;

ZoneState::Host ZoneHost()
{
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) -> MCreature* {
		CHECK(zoneExists);
		auto found = creatures.find(id);
		return found == creatures.end() ? nullptr : Handle(*found->second);
	};
	return host;
}
const ZoneState::Host zone = ZoneHost();

CreatureState::Host CreatureHost()
{
	CreatureState::Host host{};
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	host.GetX = [](MCreature* p) { return Fake(p).x; };
	host.GetY = [](MCreature* p) { return Fake(p).y; };
	host.SetEffectDelayFrame = [](MCreature* p, TYPE_ACTIONINFO ai, std::uint32_t frames) {
		calls.push_back(Who(p) + ".delay|" + N(ai) + "|" + N(frames));
	};
	host.PacketSpecialActionResult = [](MCreature* p, TYPE_ACTIONINFO ai, TYPE_OBJECTID id, TYPE_SECTORPOSITION sX, TYPE_SECTORPOSITION sY, std::uint8_t grade) {
		calls.push_back(Who(p) + ".result|" + N(ai) + "|" + N(id) + "|" + N(sX) + "|" + N(sY) + (grade != 0 ? "|g" + N(grade) : ""));
	};
	host.PacketSpecialActionToInventoryItem = [](MCreature* p, TYPE_ACTIONINFO ai) {
		calls.push_back(Who(p) + ".inventory|" + N(ai));
	};
	host.AddEffectStatus = [](MCreature* p, EFFECTSTATUS status, std::uint32_t frames) {
		calls.push_back(Who(p) + ".effect|" + N(status) + "|" + N(frames));
		return true;
	};
	host.SetTurning = [](MCreature* p, std::uint16_t count) { calls.push_back(Who(p) + ".turning|" + N(count)); };
	host.MovePosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back(Who(p) + ".place|" + N(x) + "|" + N(y));
		Fake(p).x = x;
		Fake(p).y = y;
		return true;
	};
	host.SetServerPosition = [](MCreature* p, TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		calls.push_back(Who(p) + ".serverpos|" + N(x) + "|" + N(y));
	};
	host.SetStop = [](MCreature* p) { calls.push_back(Who(p) + ".stop"); };
	host.IsNPC = IsNPC;
	// The handler asks only an NPC for its shop.
	host.GetShop = [](MCreature* p) {
		CHECK(IsNPC(p));
		return Fake(p).shop;
	};
	return host;
}
const CreatureState::Host creature = CreatureHost();

PlayerState::Host PlayerHost()
{
	PlayerState::Host host{};
	host.GetID = []() { return self->id; };
	host.Creature = []() { return Handle(*self); };
	return host;
}
const PlayerState::Host player = PlayerHost();

struct Ware : MItem
{
	Ware() { alive++; }
	~Ware() { alive--; }
	ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_SWORD; }
};
MItem* Factory(ITEM_CLASS itemClass) { return itemClass == ITEM_CLASS_SWORD ? new Ware : nullptr; }
MItemHost FactoryHost()
{
	MItemHost host{};
	host.NewItem = Factory;
	return host;
}
const MItemHost items = FactoryHost();

constexpr TYPE_ACTIONINFO kMinResult = MIN_RESULT_ACTIONINFO;

struct World : GameModelWorld
{
	FakeCreature ogre, vanda, me, merchant, shopless;
	MActionInfoTable actions;
	MActionInfoTable* oldActions = g_pActionInfoTable;
	MShop shop;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	World()
	{
		ogre = {1001, "Ogre", 10, 11};
		vanda = {1002, "Vanda", 20, 21};
		me = {7, "Self", 30, 31};
		merchant = {1003, "Merchant", 40, 41, &shop};
		shopless = {1004, "Shopless", 50, 51};
		creatures = {{ogre.id, &ogre}, {vanda.id, &vanda}, {me.id, &me}, {merchant.id, &merchant}, {shopless.id, &shopless}};
		self = &me;
		zoneExists = true;
		calls.clear();
		alive = 0;
		actions.Init(MAX_ACTIONINFO);
		actions.SetMinResultActionInfo(kMinResult);
		g_pActionInfoTable = &actions;
		shop.Init(MShopShelf::MAX_SHELF);
		CHECK(shop.SetShelf(MShopShelf::SHELF_FIXED, MShopShelf::NewShelf(MShopShelf::SHELF_FIXED)));
		g_pItemTable->InitClass(ITEM_CLASS_SWORD, 3);
		MItem::SetHost(&items);
	}
	~World()
	{
		MItem::SetHost(nullptr);
		shop.Release();
		PlayerState::SetHost(oldPlayer);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		g_pActionInfoTable = oldActions;
		creatures.clear();
		self = nullptr;
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

std::string Result(const char* who, TYPE_ACTIONINFO ai, const FakeCreature& c)
{
	return std::string(who) + ".result|" + N(ai) + "|" + N(c.id) + "|" + N(c.x) + "|" + N(c.y);
}

void ReceiveArmsOK4(ObjectID_t target, SkillType_t skill)
{
	GCAttackArmsOK4 original;
	original.setTargetObjectID(target);
	original.setSkillType(skill);
	GCAttackArmsOK4 packet;
	Decode(packet, original);
	GCAttackArmsOK4Handler::execute(&packet, nullptr);
}

void ReceiveToObjectOK4(ObjectID_t target, SkillType_t skill, BYTE grade)
{
	GCSkillToObjectOK4 original;
	original.setTargetObjectID(target);
	original.setSkillType(skill);
	original.setDuration(50);
	original.setGrade(grade);
	GCSkillToObjectOK4 packet;
	Decode(packet, original);
	GCSkillToObjectOK4Handler::execute(&packet, nullptr);
}

void ReceiveToInventoryOK2(ObjectID_t target, SkillType_t skill)
{
	GCSkillToInventoryOK2 original;
	original.setObjectID(target);
	original.setSkillType(skill);
	original.setDuration(30);
	GCSkillToInventoryOK2 packet;
	Decode(packet, original);
	GCSkillToInventoryOK2Handler::execute(&packet, nullptr);
}

void ReceiveKnockBack(ObjectID_t target, SkillType_t skill)
{
	GCKnocksTargetBackOK4 original;
	original.setTargetObjectID(target);
	original.setXYDir(60, 61, 2);
	original.setSkillType(skill);
	GCKnocksTargetBackOK4 packet;
	Decode(packet, original);
	GCKnocksTargetBackOK4Handler::execute(&packet, nullptr);
}

void ReceiveDamage(ObjectID_t target)
{
	GCGetDamage original;
	original.setObjectID(target);
	original.setDamage(9);
	GCGetDamage packet;
	Decode(packet, original);
	GCGetDamageHandler::execute(&packet, nullptr);
}

void ReceiveBought(ObjectID_t npc, ShopRackType_t shelf, BYTE index, int itemClass, ShopVersion_t version = 5)
{
	GCShopBought original;
	original.setObjectID(npc);
	original.setShopVersion(version);
	original.setShopType(shelf);
	original.setShopIndex(index);
	original.setItemObjectID(800);
	original.setItemClass(itemClass);
	original.setItemType(2);
	original.setDurability(70);
	original.setSilver(3);
	original.setGrade(4);
	original.setEnchantLevel(1);
	GCShopBought packet;
	Decode(packet, original);
	GCShopBoughtHandler::execute(&packet, nullptr);
}

} // namespace

TEST(CreatureActionResults, ArmsSkillsLandAsTheirOwnResultOrTheGunsDefault)
{
	World world;
	for (SkillType_t skill : {SKILL_JABBING_VEIN, SKILL_MOLE_SHOT, SKILL_TRIDENT, SKILL_QUICK_FIRE,
		SKILL_ULTIMATE_BLOW, SKILL_HARPOON_BOMB})
	{
		calls.clear();
		ReceiveArmsOK4(1001, skill);
		CHECK(calls == std::vector<std::string>{Result("Ogre", skill + kMinResult, world.ogre)});
	}
	calls.clear();
	ReceiveArmsOK4(1001, SKILL_ATTACK_MELEE);
	ReceiveArmsOK4(4242, SKILL_JABBING_VEIN);
	CHECK(calls == std::vector<std::string>{Result("Ogre", RESULT_SKILL_ATTACK_GUN_SR, world.ogre)});
	calls.clear();
	zoneExists = false;
	ReceiveArmsOK4(1001, SKILL_JABBING_VEIN);
	CHECK(calls.empty());
}

TEST(CreatureActionResults, ASkillOnAnObjectDelaysLandsAddsItsEffectAndMayTurnIt)
{
	World world;
	const std::uint32_t frames = ConvertDurationToFrame(50);
	// A skill without an effect or steps only lands, and only Dragon Tornado
	// turns its target.
	ReceiveToObjectOK4(1002, SKILL_JABBING_VEIN, 2);
	CHECK(calls == std::vector<std::string>({"Vanda.delay|" + N(SKILL_JABBING_VEIN) + "|" + N(frames),
		Result("Vanda", SKILL_JABBING_VEIN + kMinResult, world.vanda)}));
	calls.clear();
	world.actions.GetMutable(SKILL_DRAGON_TORNADO)->SetEffectStatus(EFFECTSTATUS_HAS_SLAYER_RELIC);
	ReceiveToObjectOK4(1002, SKILL_DRAGON_TORNADO, 0);
	CHECK(calls == std::vector<std::string>({"Vanda.delay|" + N(SKILL_DRAGON_TORNADO) + "|" + N(frames),
		Result("Vanda", SKILL_DRAGON_TORNADO + kMinResult, world.vanda),
		"Vanda.effect|" + N(EFFECTSTATUS_HAS_SLAYER_RELIC) + "|" + N(frames), "Vanda.turning|16"}));

	// A graded skill with steps lands as the step for its grade, with that
	// step's effect and turning, while the delay still names the skill sent.
	calls.clear();
	MActionInfo* stepped = world.actions.GetMutable(SKILL_JABBING_VEIN);
	stepped->SetUseActionStep();
	// A non-zero step: an unset one reads as 0, SKILL_ATTACK_MELEE.
	stepped->SetActionStep(1, SKILL_DRAGON_TORNADO);
	ReceiveToObjectOK4(1002, SKILL_JABBING_VEIN, 2);
	CHECK(calls == std::vector<std::string>({"Vanda.delay|" + N(SKILL_JABBING_VEIN) + "|" + N(frames),
		Result("Vanda", SKILL_DRAGON_TORNADO + kMinResult, world.vanda),
		"Vanda.effect|" + N(EFFECTSTATUS_HAS_SLAYER_RELIC) + "|" + N(frames), "Vanda.turning|16"}));
	// Grade 0 takes no step: the skill lands as itself.
	calls.clear();
	ReceiveToObjectOK4(1002, SKILL_JABBING_VEIN, 0);
	CHECK(calls == std::vector<std::string>({"Vanda.delay|" + N(SKILL_JABBING_VEIN) + "|" + N(frames),
		Result("Vanda", SKILL_JABBING_VEIN + kMinResult, world.vanda)}));

	// A skill past the table, an unknown target or no zone changes nothing.
	calls.clear();
	ReceiveToObjectOK4(1002, MAX_ACTIONINFO, 0);
	ReceiveToObjectOK4(4242, SKILL_JABBING_VEIN, 0);
	zoneExists = false;
	ReceiveToObjectOK4(1002, SKILL_JABBING_VEIN, 0);
	CHECK(calls.empty());
}

TEST(CreatureActionResults, ASkillOnAnInventoryItemDelaysThenActs)
{
	World world;
	ReceiveToInventoryOK2(1001, SKILL_JABBING_VEIN);
	ReceiveToInventoryOK2(4242, SKILL_JABBING_VEIN);
	CHECK(calls == std::vector<std::string>({"Ogre.delay|" + N(SKILL_JABBING_VEIN) + "|" + N(ConvertDurationToFrame(30)),
		"Ogre.inventory|" + N(SKILL_JABBING_VEIN)}));
}

TEST(CreatureActionResults, AKnockedBackTargetMovesStopsThenTakesTheResultWhereItLands)
{
	World world;
	ReceiveKnockBack(1001, SKILL_JABBING_VEIN);
	CHECK(calls == std::vector<std::string>({"Ogre.place|60|61", "Ogre.serverpos|60|61", "Ogre.stop",
		Result("Ogre", SKILL_JABBING_VEIN + kMinResult, world.ogre)}));
	CHECK_EQ(60, static_cast<int>(world.ogre.x));
}

TEST(CreatureActionResults, DamageLandsOnThePlayerOrTheNamedCreature)
{
	World world;
	ReceiveDamage(7);
	ReceiveDamage(1002);
	ReceiveDamage(4242);
	CHECK(calls == std::vector<std::string>({Result("Self", SKILL_ATTACK_MELEE + kMinResult, world.me),
		Result("Vanda", SKILL_ATTACK_MELEE + kMinResult, world.vanda)}));
	// The player is hit even without a zone.
	calls.clear();
	zoneExists = false;
	ReceiveDamage(7);
	CHECK(calls == std::vector<std::string>{Result("Self", SKILL_ATTACK_MELEE + kMinResult, world.me)});
}

TEST(CreatureActionResults, AnItemBoughtFromAnNPCFillsItsShelfSlot)
{
	World world;
	ReceiveBought(1003, MShopShelf::SHELF_FIXED, 4, ITEM_CLASS_SWORD);
	MShopShelf* shelf = world.shop.GetShelf(MShopShelf::SHELF_FIXED);
	MItem* item = shelf->GetItem(4);
	CHECK(item != nullptr);
	if (item != nullptr)
	{
		CHECK_EQ(800, static_cast<long long>(item->GetID()));
		CHECK_EQ(2, static_cast<long long>(item->GetItemType()));
		CHECK_EQ(70, static_cast<long long>(item->GetCurrentDurability()));
		CHECK_EQ(3, item->GetSilver());
		CHECK_EQ(4, item->GetGrade());
		CHECK_EQ(1, static_cast<long long>(item->GetEnchantLevel()));
	}
	CHECK_EQ(5, shelf->GetVersion());

	// A creature that is no NPC, an NPC without a shop, a missing shelf or a
	// class the factory cannot make changes neither the slot nor the
	// shelf's version, and keeps nothing alive.
	ReceiveBought(1001, MShopShelf::SHELF_FIXED, 5, ITEM_CLASS_SWORD, 9);
	ReceiveBought(1004, MShopShelf::SHELF_FIXED, 5, ITEM_CLASS_SWORD, 9);
	ReceiveBought(1003, MShopShelf::SHELF_SPECIAL, 5, ITEM_CLASS_SWORD, 9);
	ReceiveBought(1003, MShopShelf::SHELF_FIXED, 5, ITEM_CLASS_HELM, 9);
	CHECK(shelf->GetItem(5) == nullptr);
	CHECK_EQ(5, shelf->GetVersion());
	// A slot past the shelf keeps no item (it still takes the version: a
	// follow-up in the doc).
	ReceiveBought(1003, MShopShelf::SHELF_FIXED, 200, ITEM_CLASS_SWORD);
	CHECK_EQ(1, alive);
}

TEST(CreatureActionResults, WithoutAZoneOrATargetNoCreatureIsTouched)
{
	World world;
	// GetCreature fails its check if asked without a zone, and every
	// creature entry fails its check if handed no creature.
	zoneExists = false;
	ReceiveArmsOK4(1001, SKILL_JABBING_VEIN);
	ReceiveToObjectOK4(1002, SKILL_JABBING_VEIN, 0);
	ReceiveToInventoryOK2(1001, SKILL_JABBING_VEIN);
	ReceiveKnockBack(1001, SKILL_JABBING_VEIN);
	ReceiveDamage(1002);
	ReceiveBought(1003, MShopShelf::SHELF_FIXED, 4, ITEM_CLASS_SWORD);
	zoneExists = true;
	ReceiveKnockBack(4242, SKILL_JABBING_VEIN);
	ReceiveBought(4242, MShopShelf::SHELF_FIXED, 4, ITEM_CLASS_SWORD);
	CHECK(calls.empty());
	CHECK_EQ(0, alive);
}

TEST(CreatureActionResults, MissingEntriesSkipTheCreatureAndThePlayer)
{
	World world;
	const CreatureState::Host empty{};
	CreatureState::SetHost(&empty);
	MCreature* ogre = Handle(world.ogre);
	CHECK_EQ(0, static_cast<int>(CreatureState::GetX(ogre)));
	CHECK_EQ(0, static_cast<int>(CreatureState::GetY(ogre)));
	CHECK(!CreatureState::AddEffectStatus(ogre, EFFECTSTATUS_HAS_SLAYER_RELIC, 1));
	CHECK(!CreatureState::IsNPC(ogre));
	CHECK(CreatureState::GetShop(ogre) == nullptr);
	ReceiveArmsOK4(1001, SKILL_JABBING_VEIN);
	ReceiveToObjectOK4(1002, SKILL_JABBING_VEIN, 0);
	ReceiveToInventoryOK2(1001, SKILL_JABBING_VEIN);
	ReceiveKnockBack(1001, SKILL_JABBING_VEIN);
	ReceiveDamage(1002);
	ReceiveDamage(7);
	ReceiveBought(1003, MShopShelf::SHELF_FIXED, 4, ITEM_CLASS_SWORD);
	CHECK(calls.empty());
	CHECK_EQ(0, alive);
	// Without a player service the player is no creature.
	CreatureState::SetHost(&creature);
	PlayerState::SetHost(nullptr);
	CHECK(PlayerState::Creature() == nullptr);
	CHECK(PlayerState::SetHost(&player) == nullptr);
}
