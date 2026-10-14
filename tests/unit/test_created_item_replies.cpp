#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "DialogPresentationHost.h"
#include "MInventory.h"
#include "MItem.h"
#include "MItemTable.h"
#include "PlayerStateHost.h"
#include "VsUiPresentationHost.h"
#include "Gpackets/GCCreateItem.h"
#include <string>
#include <vector>

// An item created in the player's inventory: it takes the packet's type,
// options, ID, durability, grade, enchantment and number (a gun is loaded
// with that many rounds instead) and goes to the inventory cell, or is
// dropped when the cell is taken. While the player picks up part of a
// stack, the part in hand leaves the inventory onto the mouse, the rest
// being the new item. A whole Christmas tree (types 12, 25 and 41) made
// while merging takes the place of its twelve parts. An operator's
// "*C2G on" puts each item it creates on at once.
namespace {

std::vector<std::string> calls;
bool hasPlayer = true;
PlayerState::ITEM_CHECK_BUFFER bufferStatus = PlayerState::ITEM_CHECK_BUFFER_NULL;
MItem* pBuffered = nullptr;
bool creatingToGear = false;
int alive = 0;

std::string N(long long value) { return std::to_string(value); }

struct Sword : MItem
{
	Sword() { alive++; }
	~Sword() { alive--; }
	ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_SWORD; }
};
struct Gun : MItem
{
	Gun() { alive++; }
	~Gun() { alive--; }
	ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_SR; }
	bool IsGunItem() const override { return true; }
};
struct Tree : MItem
{
	Tree() { alive++; }
	~Tree() { alive--; }
	ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_EVENT_TREE; }
};

MItemHost ItemHost()
{
	MItemHost host{};
	host.NewItem = [](ITEM_CLASS itemClass) -> MItem* {
		switch (itemClass)
		{
			case ITEM_CLASS_SWORD: return new Sword;
			case ITEM_CLASS_SR: return new Gun;
			case ITEM_CLASS_EVENT_TREE: return new Tree;
			default: return nullptr;
		}
	};
	host.LoadGun = [](MItem* pGun, TYPE_ITEM_NUMBER rounds) {
		calls.push_back("loads|" + N(pGun->GetID()) + "|" + N(rounds));
		return true;
	};
	return host;
}
const MItemHost items = ItemHost();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? reinterpret_cast<MCreature*>(&calls) : nullptr; };
	host.GetItemCheckBufferStatus = []() { return bufferStatus; };
	host.GetItemCheckBuffer = []() { return pBuffered; };
	host.ClearItemCheckBuffer = []() {
		calls.push_back("buffer cleared");
		bufferStatus = PlayerState::ITEM_CHECK_BUFFER_NULL;
		pBuffered = nullptr;
	};
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.PickUpItem = [](MItem* pItem) { calls.push_back("in hand|" + N(pItem->GetID()) + " x" + N(pItem->GetNumber())); };
	return host;
}();

const VsUiPresentation::Host ui = []() {
	VsUiPresentation::Host host{};
	host.IsCreatingToGear = []() { return creatingToGear; };
	host.MoveCreatedToGear = [](int x, int y) { calls.push_back("to gear|" + N(x) + "," + N(y)); };
	return host;
}();

struct World : GameModelWorld
{
	MInventory inventory;
	MInventory* oldInventory = g_pInventory;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	const VsUiPresentation::Host* oldUi = VsUiPresentation::SetHost(&ui);
	World()
	{
		const struct { ITEM_CLASS itemClass; int types; } rows[] = {
			{ITEM_CLASS_SWORD, 3}, {ITEM_CLASS_SR, 3}, {ITEM_CLASS_EVENT_TREE, 42}};
		for (const auto& row : rows)
		{
			g_pItemTable->InitClass(row.itemClass, row.types);
			for (int type = 0; type < row.types; type++)
				testfw::MutableRow(*g_pItemTable, row.itemClass, type).SetGrid(1, 1);
		}
		inventory.Init(10, 6);
		g_pInventory = &inventory;
		MItem::SetHost(&items);
		calls.clear();
		hasPlayer = true;
		bufferStatus = PlayerState::ITEM_CHECK_BUFFER_NULL;
		pBuffered = nullptr;
		creatingToGear = false;
		alive = 0;
	}
	~World()
	{
		inventory.Release();
		MItem::SetHost(nullptr);
		VsUiPresentation::SetHost(oldUi);
		DialogPresentation::SetHost(oldDialog);
		PlayerState::SetHost(oldPlayer);
		g_pInventory = oldInventory;
	}
	template<class ItemType>
	ItemType* Put(ObjectID_t id, int type, int x, int y, int number = 1)
	{
		ItemType* pItem = new ItemType;
		pItem->SetID(id);
		pItem->SetItemType(static_cast<TYPE_ITEMTYPE>(type));
		pItem->SetNumber(static_cast<TYPE_ITEM_NUMBER>(number));
		CHECK(inventory.AddItem(pItem, static_cast<BYTE>(x), static_cast<BYTE>(y)));
		return pItem;
	}
};

void Receive(int itemClass, ObjectID_t id, int type, int x, int y, int number = 5)
{
	GCCreateItem packet;
	packet.setObjectID(id);
	packet.setItemClass(static_cast<BYTE>(itemClass));
	packet.setItemType(static_cast<ItemType_t>(type));
	packet.addOptionType(7);
	packet.addOptionType(9);
	packet.setDurability(300);
	packet.setGrade(4);
	packet.setEnchantLevel(2);
	packet.setItemNum(static_cast<ItemNum_t>(number));
	packet.setInvenX(static_cast<CoordInven_t>(x));
	packet.setInvenY(static_cast<CoordInven_t>(y));
	GCCreateItemHandler::execute(&packet, nullptr);
}

std::string At(MInventory& inventory, int x, int y)
{
	MItem* p = inventory.GetItem(static_cast<BYTE>(x), static_cast<BYTE>(y));
	if (p == nullptr) return "(empty)";
	return N(p->GetItemClass()) + "#" + N(p->GetID()) + " type " + N(p->GetItemType()) + " x" + N(p->GetNumber())
		+ " dur " + N(p->GetCurrentDurability()) + " grade " + N(p->GetGrade()) + " +" + N(p->GetEnchantLevel())
		+ " opt " + N(p->GetItemOptionListCount());
}

} // namespace

TEST(CreatedItemReplies, AnItemIsCreatedInItsInventoryCell)
{
	World world;
	Receive(ITEM_CLASS_SWORD, 501, 2, 3, 4);
	CHECK(At(world.inventory, 3, 4) == N(ITEM_CLASS_SWORD) + "#501 type 2 x5 dur 300 grade 4 +2 opt 2");
	CHECK(calls.empty());
	// A gun is loaded with the rounds instead.
	Receive(ITEM_CLASS_SR, 502, 1, 5, 1, 30);
	CHECK(calls == std::vector<std::string>{"loads|502|30"});
	CHECK(At(world.inventory, 5, 1) == N(ITEM_CLASS_SR) + "#502 type 1 x1 dur 300 grade 4 +2 opt 2");
}

TEST(CreatedItemReplies, AnItemForATakenCellOrOfNoClassIsDropped)
{
	World world;
	world.Put<Sword>(400, 0, 3, 4);
	Receive(ITEM_CLASS_SWORD, 501, 2, 3, 4);
	CHECK(At(world.inventory, 3, 4) == N(ITEM_CLASS_SWORD) + "#400 type 0 x1 dur 0 grade -1 +0 opt 0");
	CHECK_EQ(1, alive);
	Receive(ITEM_CLASS_POTION, 503, 0, 0, 0);
	CHECK(At(world.inventory, 0, 0) == "(empty)");
	CHECK_EQ(1, alive);
}

TEST(CreatedItemReplies, PickingUpPartOfAStackPutsThatPartInHand)
{
	World world;
	Sword* pStack = world.Put<Sword>(400, 0, 2, 2, 10);
	bufferStatus = PlayerState::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY;
	pBuffered = pStack;
	// Three stay behind as the new item; seven are in hand.
	Receive(ITEM_CLASS_SWORD, 501, 0, 2, 2, 3);
	CHECK(calls == std::vector<std::string>({"buffer cleared", "in hand|400 x7"}));
	CHECK(At(world.inventory, 2, 2) == N(ITEM_CLASS_SWORD) + "#501 type 0 x3 dur 300 grade 4 +2 opt 2");
	delete pStack;
	// The part in hand is not where the inventory has it: not picked up.
	calls.clear();
	Sword* pElsewhere = new Sword;
	pElsewhere->SetID(402);
	pElsewhere->SetNumber(10);
	world.Put<Sword>(401, 0, 6, 0, 10);
	bufferStatus = PlayerState::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY;
	pBuffered = pElsewhere;
	Receive(ITEM_CLASS_SWORD, 503, 0, 7, 0, 3);
	CHECK(calls == std::vector<std::string>{"buffer cleared"});
	CHECK_EQ(10, pElsewhere->GetNumber());
	delete pElsewhere;
	// Nothing in hand, or another buffer: the item is only created.
	calls.clear();
	bufferStatus = PlayerState::ITEM_CHECK_BUFFER_PICKUP_SOME_FROM_INVENTORY;
	Receive(ITEM_CLASS_SWORD, 504, 0, 8, 0);
	bufferStatus = PlayerState::ITEM_CHECK_BUFFER_PICKUP_TO_INVENTORY;
	pBuffered = world.inventory.GetItem(8, 0);
	Receive(ITEM_CLASS_SWORD, 505, 0, 9, 0);
	CHECK(calls == std::vector<std::string>{"buffer cleared"});
	CHECK(At(world.inventory, 8, 0) != "(empty)" && At(world.inventory, 9, 0) != "(empty)");
}

TEST(CreatedItemReplies, AWholeTreeTakesThePlaceOfItsParts)
{
	World world;
	for (int whole : {12, 25, 41})
	{
		calls.clear();
		world.inventory.Release();
		world.inventory.Init(10, 6);
		alive = 0;
		// The twelve parts of this tree in a 3x4 block from (1,1); the first
		// tree's parts are types 0-11, the second's 13-24, the third's 29-40.
		const int first = whole == 12 ? 0 : whole == 25 ? 13 : 29;
		for (int y = 0; y < 4; y++)
			for (int x = 0; x < 3; x++)
				world.Put<Tree>(600 + y * 3 + x, first + y * 3 + x, 1 + x, 1 + y);
		// A part of another tree beside them stays.
		world.Put<Tree>(700, 5, 5, 1);
		bufferStatus = PlayerState::ITEM_CHECK_BUFFER_TREE_MERGE;
		Receive(ITEM_CLASS_EVENT_TREE, 800, whole, 1, 1, 1);
		CHECK(calls == std::vector<std::string>{"buffer cleared"});
		CHECK(At(world.inventory, 1, 1) == N(ITEM_CLASS_EVENT_TREE) + "#800 type " + N(whole) + " x1 dur 300 grade 4 +2 opt 2");
		CHECK(At(world.inventory, 3, 4) == "(empty)");
		CHECK(At(world.inventory, 5, 1) != "(empty)");
		CHECK_EQ(2, alive);
	}
}

TEST(CreatedItemReplies, ATreeOutsideAMergeOrAnotherTreeItemLeavesThePartsAlone)
{
	World world;
	for (int y = 0; y < 4; y++)
		for (int x = 0; x < 3; x++)
			world.Put<Tree>(600 + y * 3 + x, y * 3 + x, 1 + x, 1 + y);
	// Not merging: the whole tree has no place, the parts stay.
	Receive(ITEM_CLASS_EVENT_TREE, 800, 12, 1, 1, 1);
	CHECK(calls.empty());
	CHECK_EQ(12, alive);
	// Merging, but not a whole tree: the parts stay.
	bufferStatus = PlayerState::ITEM_CHECK_BUFFER_TREE_MERGE;
	Receive(ITEM_CLASS_EVENT_TREE, 801, 11, 6, 0, 1);
	CHECK(calls.empty());
	CHECK_EQ(13, alive);
	// Merging, a part is taken by its place in the block, of whichever
	// tree: the top-left takes types 0, 13 and 29, not 1; the next 1, 14
	// and 30.
	world.inventory.Release();
	world.inventory.Init(10, 6);
	alive = 0;
	world.Put<Tree>(600, 1, 3, 2);
	world.Put<Tree>(601, 14, 4, 2);
	world.Put<Tree>(602, 31, 5, 2);
	Receive(ITEM_CLASS_EVENT_TREE, 800, 25, 3, 2, 1);
	CHECK(At(world.inventory, 3, 2) == N(ITEM_CLASS_EVENT_TREE) + "#600 type 1 x1 dur 0 grade -1 +0 opt 0");
	CHECK(At(world.inventory, 4, 2) == "(empty)");
	CHECK(At(world.inventory, 5, 2) == "(empty)");
}

TEST(CreatedItemReplies, AnOperatorsCreatedItemGoesOnAtOnce)
{
	World world;
	creatingToGear = true;
	Receive(ITEM_CLASS_SWORD, 501, 2, 3, 4);
	CHECK(calls == std::vector<std::string>{"to gear|3,4"});
	// Not when the cell is taken.
	calls.clear();
	Receive(ITEM_CLASS_SWORD, 502, 2, 3, 4);
	CHECK(calls.empty());
}

TEST(CreatedItemReplies, WithoutThePlayerOrTheHostsNothingMoves)
{
	World world;
	hasPlayer = false;
	Receive(ITEM_CLASS_SWORD, 501, 2, 3, 4);
	CHECK(At(world.inventory, 3, 4) == "(empty)");
	CHECK_EQ(0, alive);
	VsUiPresentation::SetHost(nullptr);
	CHECK(!VsUiPresentation::IsCreatingToGear());
	VsUiPresentation::MoveCreatedToGear(3, 4);
	CHECK(calls.empty());
}
