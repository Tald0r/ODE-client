#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "MShop.h"
#include "MShopShelf.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Types/ShopTypes.h"
#include "Gpackets/GCShopSold.h"
#include <string>
#include <vector>

// An item an NPC sold to someone: its special shelf loses the item (freed,
// and with it the item's UI descriptor), and the sold-from shelf takes the
// packet's version. Nothing changes without a zone, for a creature that is
// not there or is no NPC, for an NPC without a shop, or for a missing shelf.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;
int alive = 0;

struct FakeCreature
{
	TYPE_OBJECTID id = 0;
	bool npc = false;
	MShop* shop = nullptr;
};
std::vector<FakeCreature*> creatures;

MCreature* Handle(FakeCreature* creature) { return reinterpret_cast<MCreature*>(creature); }
FakeCreature& Fake(MCreature* pCreature) { return *reinterpret_cast<FakeCreature*>(pCreature); }

struct Ware : MItem
{
	Ware() { alive++; }
	~Ware() { alive--; }
	ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_SWORD; }
};

MItemHost ItemHost()
{
	MItemHost host{};
	host.ItemDestroyed = [](MItem* pItem) noexcept { calls.push_back("destroyed|" + std::to_string(pItem->GetID())); };
	return host;
}
const MItemHost items = ItemHost();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) -> MCreature* {
		CHECK(zoneExists);
		for (FakeCreature* creature : creatures)
			if (creature->id == id) return Handle(creature);
		return nullptr;
	};
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.IsNPC = [](MCreature* p) { return Fake(p).npc; };
	host.GetShop = [](MCreature* p) {
		CHECK(Fake(p).npc);
		return Fake(p).shop;
	};
	return host;
}();

struct World : GameModelWorld
{
	MShop shop;
	FakeCreature merchant{1003, true, &shop};
	FakeCreature shopless{1004, true, nullptr};
	FakeCreature ogre{1001, false, nullptr};
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	MShopShelf* special = nullptr;
	MShopShelf* fixed = nullptr;
	World()
	{
		creatures = {&merchant, &shopless, &ogre};
		shop.Init(MShopShelf::MAX_SHELF);
		fixed = MShopShelf::NewShelf(MShopShelf::SHELF_FIXED);
		special = MShopShelf::NewShelf(MShopShelf::SHELF_SPECIAL);
		CHECK(shop.SetShelf(MShopShelf::SHELF_FIXED, fixed));
		CHECK(shop.SetShelf(MShopShelf::SHELF_SPECIAL, special));
		for (MShopShelf* shelf : {fixed, special})
		{
			Ware* ware = new Ware;
			ware->SetID(shelf == special ? 801 : 802);
			CHECK(shelf->SetItem(3, ware));
			shelf->SetVersion(1);
		}
		MItem::SetHost(&items);
		calls.clear();
		zoneExists = true;
	}
	~World()
	{
		MItem::SetHost(nullptr);
		shop.Release();
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		creatures.clear();
	}
};

void Receive(ObjectID_t npc, ShopRackType_t shelf, BYTE index, ShopVersion_t version)
{
	GCShopSold original;
	original.setObjectID(npc);
	original.setShopType(shelf);
	original.setShopIndex(index);
	original.setShopVersion(version);
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	original.write(encoded);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 1024);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	GCShopSold packet;
	packet.read(stream);
	CHECK(stream.isEmpty());
	GCShopSoldHandler::execute(&packet, nullptr);
}

} // namespace

TEST(ShopSoldHandler, TheSpecialShelfLosesTheSoldItemAndTakesTheVersion)
{
	World world;
	Receive(1003, SHOP_RACK_SPECIAL, 3, 7);
	CHECK(world.special->GetItem(3) == nullptr);
	CHECK_EQ(7, world.special->GetVersion());
	// Its descriptor goes once, with the item.
	CHECK(calls == std::vector<std::string>{"destroyed|801"});
	CHECK_EQ(1, alive);
	// An empty slot sold from still takes the version.
	Receive(1003, SHOP_RACK_SPECIAL, 4, 8);
	CHECK_EQ(8, world.special->GetVersion());
	CHECK_EQ(1, alive);
}

TEST(ShopSoldHandler, AnotherShelfKeepsItsItemsAndTakesTheVersion)
{
	World world;
	Receive(1003, SHOP_RACK_NORMAL, 3, 9);
	CHECK(world.fixed->GetItem(3) != nullptr);
	CHECK_EQ(9, world.fixed->GetVersion());
	CHECK(calls.empty());
	CHECK_EQ(2, alive);
}

TEST(ShopSoldHandler, NothingChangesForNoZoneNoCreatureNoNPCNoShopOrNoShelf)
{
	World world;
	zoneExists = false;
	Receive(1003, SHOP_RACK_SPECIAL, 3, 7);
	zoneExists = true;
	Receive(4242, SHOP_RACK_SPECIAL, 3, 7);
	Receive(1001, SHOP_RACK_SPECIAL, 3, 7);
	Receive(1004, SHOP_RACK_SPECIAL, 3, 7);
	CHECK(calls.empty());
	CHECK_EQ(2, alive);
	CHECK_EQ(1, world.special->GetVersion());
	// A shop without its shelves: releasing it frees the two wares, and the
	// reply then finds no shelf to change.
	world.shop.Release();
	world.shop.Init(MShopShelf::MAX_SHELF);
	Receive(1003, SHOP_RACK_SPECIAL, 3, 7);
	CHECK(calls == std::vector<std::string>({"destroyed|802", "destroyed|801"}) || calls == std::vector<std::string>({"destroyed|801", "destroyed|802"}));
	CHECK_EQ(0, alive);
}

// A shelf type past the shop's shelves finds no shelf: MAX_SHELF and
// SHELF_BASE are enumerators, 8 and 0xff are past SHELF_TYPE's values.
// The handler used to cast the byte to SHELF_TYPE before the shop's range
// check, undefined for the last two and seen by no sanitizer, since the
// cast value is only passed on; a regression guard.
TEST(ShopSoldHandler, AShelfTypePastTheShelvesChangesNothing)
{
	World world;
	for (ShopRackType_t shelf : {(ShopRackType_t)MShopShelf::MAX_SHELF, (ShopRackType_t)MShopShelf::SHELF_BASE, (ShopRackType_t)8, (ShopRackType_t)0xff})
		Receive(1003, shelf, 3, 7);
	CHECK(calls.empty());
	CHECK_EQ(2, alive);
	CHECK_EQ(1, world.fixed->GetVersion());
	CHECK_EQ(1, world.special->GetVersion());
}
