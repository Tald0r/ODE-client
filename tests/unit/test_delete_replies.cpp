#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "DialogPresentationHost.h"
#include "MInventory.h"
#include "PlayerStateHost.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCDeleteInventoryItem.h"
#include "Gpackets/GCDeleteObject.h"
#include <set>
#include <string>
#include <vector>

// An inventory item deletion frees the item, from the mouse when it is
// there (letting the mouse go) or else from the inventory; an item the
// player was dropping onto a relic table or a creature also ends that
// drop, letting the mouse go when the deleted item is the one dropped. An
// object deletion takes a creature, or else an item, out of the zone; an
// item the player was dropping onto a relic table ends that drop first.
namespace {

std::vector<std::string> calls;
MItem* mouseItem = nullptr;
MItem* checkItem = nullptr;
PlayerState::ITEM_CHECK_BUFFER checkStatus = PlayerState::ITEM_CHECK_BUFFER_NULL;
bool zoneExists = true;
std::set<TYPE_OBJECTID> creatures, items;

std::string N(long long value) { return std::to_string(value); }

struct Goods : MItem
{
	explicit Goods(TYPE_OBJECTID id) { SetID(id); }
	ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_SWORD; }
};
// What the check buffer holds when no drop is under way.
Goods idle(0);

const MItemHost itemHost = []() {
	MItemHost host{};
	host.ItemDestroyed = [](MItem* pItem) noexcept { calls.push_back("freed|" + N(pItem->GetID())); };
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.GetItemCheckBufferStatus = []() { return checkStatus; };
	host.GetItemCheckBuffer = []() { return checkItem; };
	host.ClearItemCheckBuffer = []() { calls.push_back("buffer cleared"); };
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.GetMouseItem = []() { return mouseItem; };
	host.DropItem = []() { calls.push_back("mouse drops"); mouseItem = nullptr; };
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.RemoveCreature = [](TYPE_OBJECTID id) { calls.push_back("creature|" + N(id)); return creatures.erase(id) > 0; };
	host.RemoveItem = [](TYPE_OBJECTID id) { calls.push_back("item|" + N(id)); return items.erase(id) > 0; };
	return host;
}();

struct World : GameModelWorld
{
	MInventory inventory;
	MInventory* oldInventory = g_pInventory;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	World()
	{
		calls.clear();
		mouseItem = nullptr;
		checkItem = &idle;
		checkStatus = PlayerState::ITEM_CHECK_BUFFER_NULL;
		zoneExists = true;
		creatures = {7};
		items = {8};
		g_pItemTable->InitClass(ITEM_CLASS_SWORD, 1);
		testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, 0).SetGrid(1, 1);
		inventory.Init(6, 4);
		g_pInventory = &inventory;
		MItem::SetHost(&itemHost);
	}
	~World()
	{
		MItem::SetHost(nullptr);
		ZoneState::SetHost(oldZone);
		DialogPresentation::SetHost(oldDialog);
		PlayerState::SetHost(oldPlayer);
		inventory.Release();
		g_pInventory = oldInventory;
	}
	// Two items in the inventory, 41 at (1,1) and 42 at (2,1), and 43 on
	// the mouse.
	void Stocked()
	{
		CHECK(inventory.AddItem(new Goods(41), 1, 1));
		CHECK(inventory.AddItem(new Goods(42), 2, 1));
		mouseItem = new Goods(43);
		calls.clear();
	}
};

void ReceiveInventoryDeletion(TYPE_OBJECTID id)
{
	GCDeleteInventoryItem packet;
	packet.setObjectID(id);
	GCDeleteInventoryItemHandler::execute(&packet, nullptr);
}

void ReceiveObjectDeletion(TYPE_OBJECTID id)
{
	GCDeleteObject packet;
	packet.setObjectID(id);
	GCDeleteObjectHandler::execute(&packet, nullptr);
}

} // namespace

TEST(DeleteReplies, AnItemOnTheMouseIsLetGoAndFreed)
{
	World world;
	world.Stocked();
	ReceiveInventoryDeletion(43);
	CHECK(calls == std::vector<std::string>({"mouse drops", "freed|43"}));
	CHECK(mouseItem == nullptr);
	CHECK(world.inventory.GetItem(1, 1) != nullptr);
	CHECK(world.inventory.GetItem(2, 1) != nullptr);
}

TEST(DeleteReplies, AnItemInTheInventoryIsTakenOutAndFreed)
{
	World world;
	world.Stocked();
	ReceiveInventoryDeletion(42);
	CHECK(calls == std::vector<std::string>{"freed|42"});
	CHECK(world.inventory.GetItem(2, 1) == nullptr);
	CHECK(world.inventory.GetItem(1, 1) != nullptr);
	CHECK(mouseItem != nullptr);
	// An item that is in neither place: nothing.
	calls.clear();
	ReceiveInventoryDeletion(99);
	CHECK(calls.empty());
	CHECK(world.inventory.GetItem(1, 1) != nullptr);
	CHECK(mouseItem != nullptr);
	delete mouseItem;
}

TEST(DeleteReplies, AnInventoryDeletionEndsADropOntoARelicTableOrACreature)
{
	World world;
	world.Stocked();
	Goods other(50);
	// The deleted item is the one being dropped: the mouse lets it go too.
	MItem* pHeld = mouseItem;
	MItem* pDropped = world.inventory.GetItem(2, 1);
	checkItem = pDropped;
	checkStatus = PlayerState::ITEM_CHECK_BUFFER_DROP_TO_RELICTABLE;
	ReceiveInventoryDeletion(42);
	CHECK(calls == std::vector<std::string>({"mouse drops", "buffer cleared", "freed|42"}));
	delete pHeld;
	// Another item is being dropped: only the drop ends.
	calls.clear();
	mouseItem = new Goods(43);
	checkItem = &other;
	checkStatus = PlayerState::ITEM_CHECK_BUFFER_DROP_TO_CREATURE;
	ReceiveInventoryDeletion(41);
	CHECK(calls == std::vector<std::string>({"buffer cleared", "freed|41"}));
	CHECK(mouseItem != nullptr);
	// From the mouse, being dropped onto a creature or a relic table (the
	// dropped item is always the mouse's): the mouse lets it go twice.
	calls.clear();
	checkItem = mouseItem;
	ReceiveInventoryDeletion(43);
	CHECK(calls == std::vector<std::string>({"mouse drops", "mouse drops", "buffer cleared", "freed|43"}));
	calls.clear();
	mouseItem = checkItem = new Goods(44);
	checkStatus = PlayerState::ITEM_CHECK_BUFFER_DROP_TO_RELICTABLE;
	ReceiveInventoryDeletion(44);
	CHECK(calls == std::vector<std::string>({"mouse drops", "mouse drops", "buffer cleared", "freed|44"}));
}

TEST(DeleteReplies, WithNothingOnTheMouseTheInventoryItemIsFreed)
{
	World world;
	CHECK(world.inventory.AddItem(new Goods(41), 1, 1));
	calls.clear();
	ReceiveInventoryDeletion(41);
	CHECK(calls == std::vector<std::string>{"freed|41"});
	CHECK(world.inventory.GetItem(1, 1) == nullptr);
}

TEST(DeleteReplies, AnInventoryDeletionLeavesOtherChecksAndMissingItemsAlone)
{
	World world;
	world.Stocked();
	Goods other(50);
	checkItem = &other;
	for (PlayerState::ITEM_CHECK_BUFFER status : {PlayerState::ITEM_CHECK_BUFFER_NULL, PlayerState::ITEM_CHECK_BUFFER_DROP_TO_ZONE,
		PlayerState::ITEM_CHECK_BUFFER_DROP_TO_INVENTORY})
	{
		calls.clear();
		CHECK(world.inventory.AddItem(new Goods(44), 4, 2));
		checkStatus = status;
		ReceiveInventoryDeletion(44);
		CHECK(calls == std::vector<std::string>{"freed|44"});
	}
	// No item deleted: the drop goes on.
	calls.clear();
	checkStatus = PlayerState::ITEM_CHECK_BUFFER_DROP_TO_RELICTABLE;
	ReceiveInventoryDeletion(99);
	CHECK(calls.empty());
	delete mouseItem;
}

TEST(DeleteReplies, AnObjectDeletionTriesACreatureThenAnItem)
{
	World world;
	ReceiveObjectDeletion(7);
	CHECK(calls == std::vector<std::string>{"creature|7"});
	calls.clear();
	ReceiveObjectDeletion(8);
	CHECK(calls == std::vector<std::string>({"creature|8", "item|8"}));
	calls.clear();
	ReceiveObjectDeletion(9);
	CHECK(calls == std::vector<std::string>({"creature|9", "item|9"}));
}

TEST(DeleteReplies, AnObjectDeletionEndsADropOntoARelicTable)
{
	World world;
	Goods relic(8);
	mouseItem = &relic;
	checkItem = &relic;
	checkStatus = PlayerState::ITEM_CHECK_BUFFER_DROP_TO_RELICTABLE;
	ReceiveObjectDeletion(8);
	CHECK(calls == std::vector<std::string>({"mouse drops", "buffer cleared", "creature|8", "item|8"}));
	// Another object: only the drop ends.
	calls.clear();
	ReceiveObjectDeletion(7);
	CHECK(calls == std::vector<std::string>({"buffer cleared", "creature|7"}));
	// A drop onto a creature goes on.
	calls.clear();
	checkStatus = PlayerState::ITEM_CHECK_BUFFER_DROP_TO_CREATURE;
	ReceiveObjectDeletion(8);
	CHECK(calls == std::vector<std::string>({"creature|8", "item|8"}));
}

TEST(DeleteReplies, WithoutAZoneNoObjectIsDeleted)
{
	World world;
	Goods relic(8);
	checkItem = &relic;
	checkStatus = PlayerState::ITEM_CHECK_BUFFER_DROP_TO_RELICTABLE;
	zoneExists = false;
	ReceiveObjectDeletion(8);
	CHECK(calls.empty());
	// Without the zone's host there is no zone, and no creature to take.
	ZoneState::SetHost(nullptr);
	ReceiveObjectDeletion(7);
	CHECK(!ZoneState::RemoveCreature(7));
	const ZoneState::Host bare{};
	ZoneState::SetHost(&bare);
	CHECK(!ZoneState::RemoveCreature(7));
	CHECK(calls.empty());
}
