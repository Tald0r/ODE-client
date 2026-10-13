#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "DialogPresentationHost.h"
#include "MInventory.h"
#include "MStatus.h"
#include "PlayerStateHost.h"
#include "TempInformation.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCMakeItemOK.h"
#include <string>
#include <vector>

// An item made with a skill: the items locked for the skill are let go
// (only if the skill was waiting for it), the stacks the making used up
// are recounted, the made item goes into the inventory where the packet
// says (or is freed if it does not fit), and the player's status changes.
namespace {

std::vector<std::string> calls;
int alive = 0;
MStatus status;

struct Ware : MItem
{
	Ware() { alive++; }
	~Ware() { alive--; }
	ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_SWORD; }
};

MItemHost FactoryHost()
{
	MItemHost host{};
	host.NewItem = [](ITEM_CLASS itemClass) -> MItem* { return itemClass == ITEM_CLASS_SWORD ? new Ware : nullptr; };
	return host;
}
const MItemHost items = FactoryHost();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.UnlockItem = []() { calls.push_back("unlock"); };
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Status = []() { return &status; };
	return host;
}();

struct World : GameModelWorld
{
	MInventory inventory;
	TempInformation temp;
	MInventory* oldInventory = g_pInventory;
	TempInformation* oldTemp = g_pTempInformation;
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	Ware* stack = nullptr;
	World()
	{
		g_pItemTable->InitClass(ITEM_CLASS_SWORD, 3);
		for (int type = 0; type < 3; type++)
			testfw::MutableRow(*g_pItemTable, ITEM_CLASS_SWORD, type).SetGrid(1, 1);
		inventory.Init(4, 4);
		g_pInventory = &inventory;
		g_pTempInformation = &temp;
		MItem::SetHost(&items);
		calls.clear();
		alive = 0;
		status.SetStatus(MODIFY_CURRENT_HP, 10);
		// A stack the making uses up, at (0,0).
		stack = new Ware;
		stack->SetID(500);
		stack->SetNumber(9);
		CHECK(inventory.AddItem(stack, 0, 0));
	}
	~World()
	{
		inventory.Release();
		MItem::SetHost(nullptr);
		PlayerState::SetHost(oldPlayer);
		DialogPresentation::SetHost(oldDialog);
		g_pTempInformation = oldTemp;
		g_pInventory = oldInventory;
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

void Receive(BYTE itemClass, CoordInven_t x, CoordInven_t y)
{
	GCMakeItemOK original;
	original.addChangedItemListElement(500, 4);
	original.addChangedItemListElement(501, 3);	// nothing of that ID
	original.setObjectID(800);
	original.setItemClass(itemClass);
	original.setItemType(2);
	original.setOptionType({7});
	original.setDurability(60);
	original.setItemNum(3);
	original.setX(x);
	original.setY(y);
	original.addShortData(MODIFY_CURRENT_HP, 55);
	GCMakeItemOK packet;
	Decode(packet, original);
	GCMakeItemOKHandler::execute(&packet, nullptr);
}

} // namespace

TEST(MakeItemHandler, AMadeItemRecountsTheStacksGoesWhereThePacketSaysAndChangesTheStatus)
{
	World world;
	world.temp.SetMode(TempInformation::MODE_SKILL_MAKE_ITEM);
	Receive(ITEM_CLASS_SWORD, 2, 1);
	CHECK(calls == std::vector<std::string>{"unlock"});
	CHECK_EQ(static_cast<int>(TempInformation::MODE_NULL), static_cast<int>(world.temp.GetMode()));
	CHECK_EQ(4, static_cast<int>(world.stack->GetNumber()));
	const MItem* made = world.inventory.GetItem(2, 1);
	CHECK(made != nullptr);
	if (made != nullptr)
	{
		CHECK_EQ(800, static_cast<long long>(made->GetID()));
		CHECK_EQ(2, static_cast<long long>(made->GetItemType()));
		CHECK(made->GetItemOptionList() == std::list<TYPE_ITEM_OPTION>({7}));
		CHECK_EQ(60, static_cast<long long>(made->GetCurrentDurability()));
		CHECK_EQ(3, static_cast<long long>(made->GetNumber()));
	}
	CHECK_EQ(55, static_cast<int>(status.GetHP()));
	CHECK_EQ(2, alive);
}

TEST(MakeItemHandler, WithoutTheSkillWaitingNothingIsLetGoButThePacketIsApplied)
{
	World world;
	// Another request's mode stays.
	world.temp.SetMode(TempInformation::MODE_PARTY_REQUEST);
	Receive(ITEM_CLASS_SWORD, 2, 1);
	CHECK(calls.empty());
	CHECK_EQ(static_cast<int>(TempInformation::MODE_PARTY_REQUEST), static_cast<int>(world.temp.GetMode()));
	CHECK(world.inventory.GetItem(2, 1) != nullptr);
	CHECK_EQ(4, static_cast<int>(world.stack->GetNumber()));
	CHECK_EQ(55, static_cast<int>(status.GetHP()));
}

TEST(MakeItemHandler, AMadeItemThatDoesNotFitIsFreed)
{
	World world;
	// (0,0) holds the stack, and (9,9) is off the grid.
	Receive(ITEM_CLASS_SWORD, 0, 0);
	Receive(ITEM_CLASS_SWORD, 9, 9);
	CHECK_EQ(1, alive);
	CHECK_EQ(55, static_cast<int>(status.GetHP()));
}

TEST(MakeItemHandler, AnUnknownClassMakesNothingAndLeavesTheStatus)
{
	World world;
	Receive(ITEM_CLASS_HELM, 2, 1);
	CHECK(world.inventory.GetItem(2, 1) == nullptr);
	CHECK_EQ(4, static_cast<int>(world.stack->GetNumber()));
	// As before the move, the status change is skipped too (a follow-up).
	CHECK_EQ(10, static_cast<int>(status.GetHP()));
}

TEST(MakeItemHandler, MissingServicesSkipTheUnlockAndTheStatus)
{
	World world;
	world.temp.SetMode(TempInformation::MODE_SKILL_MAKE_ITEM);
	DialogPresentation::SetHost(nullptr);
	PlayerState::SetHost(nullptr);
	Receive(ITEM_CLASS_SWORD, 2, 1);
	CHECK(calls.empty());
	CHECK_EQ(static_cast<int>(TempInformation::MODE_NULL), static_cast<int>(world.temp.GetMode()));
	CHECK(world.inventory.GetItem(2, 1) != nullptr);
	CHECK_EQ(10, static_cast<int>(status.GetHP()));
}
