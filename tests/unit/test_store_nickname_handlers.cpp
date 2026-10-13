#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "VsUiPresentationHost.h"
#include "MStorage.h"
#include "NicknameInfo.h"
#include "SubItemInfo.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCAddNickname.h"
#include "Gpackets/GCAddStoreItem.h"
#include "Gpackets/GCNicknameList.h"
#include "Gpackets/GCRemoveStoreItem.h"
#include <string>
#include <vector>

// The personal store (g_pStorage2) and the player's nickname list: an item
// added to the open store lands in its first free slot, built from the
// packet; an item sold leaves it while the store view is open; nicknames go
// to the UI object's list.
namespace {

std::vector<std::string> calls;
int inventoryMode = 1;
int alive = 0;
bool loadable = true;

VsUiPresentation::Host UiHost()
{
	VsUiPresentation::Host host{};
	host.InventoryMode = []() { return inventoryMode; };
	host.AddNickNameList = [](NicknameInfo* p) { calls.push_back("addnick|" + p->getNickname()); };
	host.SetNickNameList = [](std::vector<NicknameInfo*>* p) { calls.push_back("setnicks|" + std::to_string(p->size())); };
	return host;
}
const VsUiPresentation::Host ui = UiHost();

struct Ware : MItem
{
	ITEM_CLASS m_Class;
	explicit Ware(ITEM_CLASS itemClass) : m_Class(itemClass) { alive++; }
	~Ware() { alive--; }
	ITEM_CLASS GetItemClass() const override { return m_Class; }
	bool IsGunItem() const override { return m_Class == ITEM_CLASS_SR; }
	bool IsQuickItem() const override { return m_Class == ITEM_CLASS_POTION; }
};
struct Belt : MBelt
{
	Belt() { alive++; }
	~Belt() { alive--; }
};
struct ArmsBand : MOustersArmsBand
{
	ArmsBand() { alive++; }
	~ArmsBand() { alive--; }
};

MItem* Factory(ITEM_CLASS itemClass)
{
	switch (itemClass)
	{
		case ITEM_CLASS_SWORD: case ITEM_CLASS_SR: case ITEM_CLASS_POTION: return new Ware(itemClass);
		case ITEM_CLASS_BELT: return new Belt;
		case ITEM_CLASS_OUSTERS_ARMSBAND: return new ArmsBand;
		default: return nullptr;
	}
}
MItemHost FactoryHost()
{
	MItemHost host{};
	host.NewItem = Factory;
	host.LoadGun = [](MItem* pGun, TYPE_ITEM_NUMBER rounds) {
		calls.push_back("load|" + std::to_string(pGun->GetID()) + "|" + std::to_string(rounds));
		return loadable;
	};
	return host;
}
const MItemHost items = FactoryHost();

struct World : GameModelWorld
{
	MStorage* oldStore = g_pStorage2;
	const VsUiPresentation::Host* oldUi = VsUiPresentation::SetHost(&ui);
	World()
	{
		const struct { ITEM_CLASS itemClass; int pockets; } rows[] = {
			{ITEM_CLASS_SWORD, 0}, {ITEM_CLASS_SR, 0}, {ITEM_CLASS_POTION, 0}, {ITEM_CLASS_BELT, 2},
			{ITEM_CLASS_OUSTERS_ARMSBAND, 2},
		};
		for (const auto& row : rows)
		{
			g_pItemTable->InitClass(row.itemClass, 3);
			for (int type = 0; type < 3; type++)
				testfw::MutableRow(*g_pItemTable, row.itemClass, type).SetValue(0, 0, row.pockets);
		}
		g_pStorage2 = new MStorage;
		g_pStorage2->Init(1);
		g_pStorage2->SetCuropenid(55);
		MItem::SetHost(&items);
		calls.clear();
		inventoryMode = 1;
		alive = 0;
		loadable = true;
	}
	~World()
	{
		delete g_pStorage2;
		g_pStorage2 = oldStore;
		MItem::SetHost(nullptr);
		VsUiPresentation::SetHost(oldUi);
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
	SocketInputStream stream(&in, 2048);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	packet.read(stream);
	CHECK(stream.isEmpty());
}

void ReceiveAdd(ObjectID_t owner, ObjectID_t id, BYTE itemClass, std::vector<SubItemInfo*> pockets = {})
{
	GCAddStoreItem original;
	original.setOwnerObjectID(owner);
	original.setIndex(0);
	StoreItemInfo& item = original.getItem();
	item.setItemExist(1);
	item.setObjectID(id);
	item.setItemClass(itemClass);
	item.setItemType(2);
	item.setOptionType({7});
	item.setDurability(90);
	item.setSilver(5);
	item.setGrade(3);
	item.setEnchantLevel(1);
	item.setItemNum(4);
	item.setPrice(1500);
	for (SubItemInfo* pocket : pockets) item.addListElement(pocket);
	GCAddStoreItem packet;
	Decode(packet, original);
	GCAddStoreItemHandler::execute(&packet, nullptr);
}

SubItemInfo* Pocket(ObjectID_t id, BYTE itemClass, SlotID_t slot)
{
	SubItemInfo* info = new SubItemInfo;
	info->setObjectID(id);
	info->setItemClass(itemClass);
	info->setItemType(1);
	info->setItemNum(3);
	info->setSlotID(slot);
	return info;
}

void ReceiveRemove(ObjectID_t owner, BYTE index)
{
	GCRemoveStoreItem packet;
	packet.setOwnerObjectID(owner);
	packet.setIndex(index);
	GCRemoveStoreItemHandler::execute(&packet, nullptr);
}

} // namespace

TEST(StoreNicknameHandlers, NicknamesGoToTheUIObjectsList)
{
	World world;
	GCAddNickname add;
	add.getNicknameInfo().setNickname("Lantern");
	GCAddNicknameHandler::execute(&add, nullptr);
	GCNicknameList list;
	list.getAddresses().push_back(new NicknameInfo);
	list.getAddresses().push_back(new NicknameInfo);
	GCNicknameListHandler::execute(&list, nullptr);
	CHECK(calls == std::vector<std::string>({"addnick|Lantern", "setnicks|2"}));
	// The packet does not free its entries (a follow-up in the doc).
	for (NicknameInfo* info : list.getAddresses()) delete info;
}

TEST(StoreNicknameHandlers, AnItemAddedToTheOpenStoreFillsTheFirstFreeSlotFromThePacket)
{
	World world;
	ReceiveAdd(55, 900, ITEM_CLASS_SWORD);
	const MItem* first = g_pStorage2->GetItem(0);
	CHECK(first != nullptr);
	if (first != nullptr)
	{
		CHECK_EQ(900, static_cast<long long>(first->GetID()));
		CHECK_EQ(2, static_cast<long long>(first->GetItemType()));
		CHECK(first->GetItemOptionList() == std::list<TYPE_ITEM_OPTION>({7}));
		CHECK_EQ(90, static_cast<long long>(first->GetCurrentDurability()));
		CHECK_EQ(1500, static_cast<long long>(const_cast<MItem*>(first)->GetPersnalPrice()));
		CHECK_EQ(5, first->GetSilver());
		CHECK_EQ(3, first->GetGrade());
		CHECK_EQ(1, static_cast<long long>(first->GetEnchantLevel()));
	}
	ReceiveAdd(55, 901, ITEM_CLASS_SWORD);
	CHECK(g_pStorage2->GetItem(1) != nullptr && g_pStorage2->GetItem(1)->GetID() == 901);

	// A gun is loaded with one round, as a new magazine starts, whatever
	// number the packet carries, and keeps no silver.
	ReceiveAdd(55, 902, ITEM_CLASS_SR);
	CHECK(calls == std::vector<std::string>{"load|902|1"});
	CHECK(g_pStorage2->GetItem(2) != nullptr && g_pStorage2->GetItem(2)->GetSilver() == 0);

	// Another owner's store, an unknown class, a closed store (even for an
	// owner of 0) or no store adds nothing.
	ReceiveAdd(56, 903, ITEM_CLASS_SWORD);
	ReceiveAdd(55, 904, ITEM_CLASS_HELM);
	g_pStorage2->SetCuropenid(0);
	ReceiveAdd(0, 905, ITEM_CLASS_SWORD);
	CHECK(g_pStorage2->GetItem(3) == nullptr);
	MStorage* store = g_pStorage2;
	g_pStorage2 = nullptr;
	ReceiveAdd(55, 906, ITEM_CLASS_SWORD);
	g_pStorage2 = store;
	CHECK_EQ(3, alive);
}

TEST(StoreNicknameHandlers, AGunWithoutAMagazineIsStoredUnloaded)
{
	World world;
	loadable = false;
	ReceiveAdd(55, 902, ITEM_CLASS_SR);
	CHECK(calls == std::vector<std::string>{"load|902|1"});
	CHECK(g_pStorage2->GetItem(0) != nullptr && g_pStorage2->GetItem(0)->GetID() == 902);
	CHECK_EQ(1, alive);
}

TEST(StoreNicknameHandlers, AStoredBeltKeepsThePocketsThatFit)
{
	World world;
	ReceiveAdd(55, 910, ITEM_CLASS_BELT, {Pocket(911, ITEM_CLASS_POTION, 1), Pocket(912, ITEM_CLASS_POTION, 5),
		Pocket(913, ITEM_CLASS_HELM, 0)});
	const MBelt* belt = dynamic_cast<const MBelt*>(g_pStorage2->GetItem(0));
	const MItem* pocket = belt != nullptr ? belt->GetItem(1) : nullptr;
	CHECK(pocket != nullptr && pocket->GetID() == 911 && pocket->GetItemType() == 1 && pocket->GetNumber() == 3);
	CHECK(belt != nullptr && belt->GetItem(0) == nullptr);
	// The belt and its one pocket; the pocket past the belt is freed.
	CHECK_EQ(2, alive);
}

TEST(StoreNicknameHandlers, AStoredArmsBandKeepsThePocketsThatFit)
{
	World world;
	ReceiveAdd(55, 920, ITEM_CLASS_OUSTERS_ARMSBAND, {Pocket(921, ITEM_CLASS_POTION, 0), Pocket(922, ITEM_CLASS_POTION, 9),
		Pocket(923, ITEM_CLASS_HELM, 1)});
	const MOustersArmsBand* band = dynamic_cast<const MOustersArmsBand*>(g_pStorage2->GetItem(0));
	const MItem* pocket = band != nullptr ? band->GetItem(0) : nullptr;
	CHECK(pocket != nullptr && pocket->GetID() == 921 && pocket->GetItemType() == 1 && pocket->GetNumber() == 3);
	CHECK(band != nullptr && band->GetItem(1) == nullptr);
	CHECK_EQ(2, alive);
}

TEST(StoreNicknameHandlers, ASoldItemLeavesTheStoreOnlyWhileTheStoreViewIsOpen)
{
	World world;
	ReceiveAdd(55, 900, ITEM_CLASS_SWORD);
	const MItem* item = g_pStorage2->GetItem(0);
	inventoryMode = 0;
	ReceiveRemove(55, 0);
	CHECK(g_pStorage2->GetItem(0) == item);
	inventoryMode = 2;
	ReceiveRemove(56, 0);
	CHECK(g_pStorage2->GetItem(0) == item);
	// The packet's slot, and only that one, is emptied.
	ReceiveAdd(55, 901, ITEM_CLASS_SWORD);
	const MItem* second = g_pStorage2->GetItem(1);
	ReceiveRemove(55, 1);
	CHECK(g_pStorage2->GetItem(0) == item);
	CHECK(g_pStorage2->GetItem(1) == nullptr);
	ReceiveRemove(55, 0);
	CHECK(g_pStorage2->GetItem(0) == nullptr);
	// The handler drops the removed items without freeing them (a
	// follow-up in the doc).
	delete item;
	delete second;
	// Mode 1 is the store view too; a closed store, even for an owner of
	// 0, or no store removes nothing.
	inventoryMode = 1;
	ReceiveAdd(55, 902, ITEM_CLASS_SWORD);
	const MItem* third = g_pStorage2->GetItem(0);
	g_pStorage2->SetCuropenid(0);
	ReceiveRemove(0, 0);
	MStorage* store = g_pStorage2;
	g_pStorage2 = nullptr;
	ReceiveRemove(55, 0);
	g_pStorage2 = store;
	CHECK(g_pStorage2->GetItem(0) == third);
	g_pStorage2->SetCuropenid(55);
	ReceiveRemove(55, 0);
	CHECK(g_pStorage2->GetItem(0) == nullptr);
	delete third;
	CHECK_EQ(0, alive);
}

TEST(StoreNicknameHandlers, MissingHostsAndEntriesSkipTheUIAndCloseTheStoreView)
{
	World world;
	ReceiveAdd(55, 900, ITEM_CLASS_SWORD);
	VsUiPresentation::SetHost(nullptr);
	CHECK_EQ(0, VsUiPresentation::InventoryMode());
	ReceiveRemove(55, 0);
	CHECK(g_pStorage2->GetItem(0) != nullptr);
	GCAddNickname add;
	GCAddNicknameHandler::execute(&add, nullptr);
	GCNicknameList list;
	GCNicknameListHandler::execute(&list, nullptr);
	const VsUiPresentation::Host empty{};
	CHECK(VsUiPresentation::SetHost(&empty) == nullptr);
	ReceiveRemove(55, 0);
	GCAddNicknameHandler::execute(&add, nullptr);
	GCNicknameListHandler::execute(&list, nullptr);
	CHECK(g_pStorage2->GetItem(0) != nullptr);
	CHECK(calls.empty());
	CHECK(VsUiPresentation::SetHost(&ui) == &empty);
}
