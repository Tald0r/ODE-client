#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "MInventory.h"
#include "MMoneyManager.h"
#include "MTradeManager.h"
#include "SubItemInfo.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCTradeAddItem.h"
#include <string>
#include <vector>

// MItem::NewItem range-checks a class from the wire and creates the item
// through the factory the executable installs (MItemHost::NewItem). The trade
// box's new-item reply builds the other side's item that way and puts it in
// the other inventory, a belt or arms band with its pockets filled.
namespace {

std::vector<int> made;		// every class the factory was asked for
int alive = 0;

struct Sword : MItem
{
	Sword() { alive++; }
	~Sword() { alive--; }
	ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_SWORD; }
};
struct Potion : MItem
{
	Potion() { alive++; }
	~Potion() { alive--; }
	ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_POTION; }
	bool IsQuickItem() const override { return true; }
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
	made.push_back(itemClass);
	switch (itemClass)
	{
		case ITEM_CLASS_SWORD: return new Sword;
		case ITEM_CLASS_POTION: return new Potion;
		case ITEM_CLASS_BELT: return new Belt;
		case ITEM_CLASS_OUSTERS_ARMSBAND: return new ArmsBand;
		default: return nullptr;
	}
}

MItemHost FactoryHost()
{
	MItemHost host{};
	host.NewItem = Factory;
	return host;
}
const MItemHost withFactory = FactoryHost();

struct World : GameModelWorld
{
	MInventory inventory;
	MTradeManager trade;
	MMoneyManager money;
	MInventory* oldInventory = g_pInventory;
	MTradeManager* oldTrade = g_pTradeManager;
	MMoneyManager* oldMoney = g_pMoneyManager;
	World()
	{
		const struct { ITEM_CLASS itemClass; int pockets; } rows[] = {
			{ITEM_CLASS_SWORD, 0}, {ITEM_CLASS_POTION, 0}, {ITEM_CLASS_BELT, 3}, {ITEM_CLASS_OUSTERS_ARMSBAND, 2},
		};
		for (const auto& row : rows)
		{
			g_pItemTable->InitClass(row.itemClass, 3);
			for (int type = 0; type < 3; type++)
			{
				testfw::MutableRow(*g_pItemTable, row.itemClass, type).SetGrid(1, 1);
				testfw::MutableRow(*g_pItemTable, row.itemClass, type).SetValue(0, 0, row.pockets);
			}
		}
		inventory.Init(4, 4);
		g_pInventory = &inventory;
		g_pMoneyManager = &money;
		trade.Init();
		g_pTradeManager = &trade;
		MItem::SetHost(&withFactory);
		made.clear();
		alive = 0;
	}
	~World()
	{
		MItem::SetHost(nullptr);
		g_pTradeManager = oldTrade;
		g_pMoneyManager = oldMoney;
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

SubItemInfo* Pocket(ObjectID_t id, BYTE itemClass, ItemType_t type, ItemNum_t number, SlotID_t slot)
{
	SubItemInfo* info = new SubItemInfo;
	info->setObjectID(id);
	info->setItemClass(itemClass);
	info->setItemType(type);
	info->setItemNum(number);
	info->setSlotID(slot);
	return info;
}

GCTradeAddItem Offer(ObjectID_t id, BYTE itemClass, CoordInven_t x, CoordInven_t y)
{
	GCTradeAddItem packet;
	packet.setTargetObjectID(900);
	packet.setX(x);
	packet.setY(y);
	packet.setItemObjectID(id);
	packet.setItemClass(itemClass);
	packet.setItemType(2);
	packet.setOptionType({11, 12});
	packet.setDurability(345);
	packet.setItemNum(4);
	packet.setSilver(6);
	packet.setGrade(7);
	packet.setEnchantLevel(8);
	packet.setListNum(0);
	return packet;
}

void Receive(const GCTradeAddItem& original)
{
	GCTradeAddItem packet;
	Decode(packet, original);
	GCTradeAddItemHandler::execute(&packet, nullptr);
}

void Accept(MTradeManager& trade)
{
	trade.AcceptMyTrade();
	trade.AcceptOtherTrade();
}

} // namespace

TEST(ItemFactory, NewItemRangeChecksTheClassThenAsksTheInstalledFactory)
{
	World world;
	MItem* sword = MItem::NewItem(ITEM_CLASS_SWORD);
	CHECK(sword != nullptr && sword->GetItemClass() == ITEM_CLASS_SWORD);
	delete sword;
	// A class the factory cannot make is NULL from the factory.
	CHECK(MItem::NewItem(ITEM_CLASS_HELM) == nullptr);
	CHECK(made == std::vector<int>({ITEM_CLASS_SWORD, ITEM_CLASS_HELM}));
	// Out of range is refused before the factory sees it.
	made.clear();
	CHECK(MItem::NewItem(MAX_ITEM_CLASS) == nullptr);
	CHECK(MItem::NewItem(ITEM_CLASS_NULL) == nullptr);
	// Callers cast the wire's class byte to ITEM_CLASS before the check, so
	// every byte, and a negative int, has to be a value the enum can hold.
	CHECK(MItem::NewItem(static_cast<ITEM_CLASS>(200)) == nullptr);
	CHECK(MItem::NewItem(static_cast<ITEM_CLASS>(255)) == nullptr);
	CHECK(MItem::NewItem(static_cast<ITEM_CLASS>(-1)) == nullptr);
	CHECK(made.empty());
	// Without a factory nothing is made.
	MItem::SetHost(nullptr);
	CHECK(MItem::NewItem(ITEM_CLASS_SWORD) == nullptr);
	const MItemHost none{};
	MItem::SetHost(&none);
	CHECK(MItem::NewItem(ITEM_CLASS_SWORD) == nullptr);
	CHECK(made.empty());
	CHECK_EQ(0, alive);
}

TEST(ItemFactory, AnOfferedItemIsBuiltFromThePacketIntoTheOtherInventoryAndUnacceptsBothSides)
{
	World world;
	Accept(world.trade);
	Receive(Offer(501, ITEM_CLASS_SWORD, 2, 3));
	MItem* item = world.trade.GetOtherInventory()->GetItem(2, 3);
	CHECK(item != nullptr);
	if (item != nullptr)
	{
		CHECK(item->GetItemClass() == ITEM_CLASS_SWORD);
		CHECK_EQ(501, static_cast<long long>(item->GetID()));
		CHECK_EQ(2, static_cast<long long>(item->GetItemType()));
		CHECK(item->GetItemOptionList() == std::list<TYPE_ITEM_OPTION>({11, 12}));
		CHECK_EQ(345, static_cast<long long>(item->GetCurrentDurability()));
		CHECK_EQ(4, static_cast<long long>(item->GetNumber()));
		CHECK_EQ(6, item->GetSilver());
		CHECK_EQ(7, item->GetGrade());
		CHECK_EQ(8, static_cast<long long>(item->GetEnchantLevel()));
	}
	CHECK(!world.trade.IsAcceptMyTrade());
	CHECK(!world.trade.IsAcceptOtherTrade());

	// An item that does not fit is freed, and the offer still unaccepts.
	Accept(world.trade);
	Receive(Offer(502, ITEM_CLASS_SWORD, 2, 3));
	CHECK(world.trade.GetOtherInventory()->GetItem(2, 3) == item);
	CHECK_EQ(1, alive);
	CHECK(!world.trade.IsAcceptMyTrade());
	CHECK(!world.trade.IsAcceptOtherTrade());
}

TEST(ItemFactory, AnOfferOfAnUnknownClassOrWithoutATradeChangesNothing)
{
	World world;
	Accept(world.trade);
	Receive(Offer(503, ITEM_CLASS_HELM, 0, 0));
	CHECK(world.trade.GetOtherInventory()->GetItem(0, 0) == nullptr);
	CHECK(world.trade.IsAcceptMyTrade());
	CHECK(world.trade.IsAcceptOtherTrade());

	made.clear();
	g_pTradeManager = nullptr;
	Receive(Offer(504, ITEM_CLASS_SWORD, 0, 0));
	CHECK(made.empty());
	CHECK_EQ(0, alive);
}

TEST(ItemFactory, AnOfferedBeltOrArmsBandArrivesWithItsPocketsFilled)
{
	World world;
	GCTradeAddItem belt = Offer(600, ITEM_CLASS_BELT, 1, 1);
	belt.setListNum(3);
	belt.addListElement(Pocket(601, ITEM_CLASS_POTION, 1, 9, 0));
	belt.addListElement(Pocket(602, ITEM_CLASS_HELM, 0, 1, 1));	// no such class: skipped
	belt.addListElement(Pocket(603, ITEM_CLASS_POTION, 2, 5, 2));
	Receive(belt);
	MBelt* received = dynamic_cast<MBelt*>(world.trade.GetOtherInventory()->GetItem(1, 1));
	CHECK(received != nullptr);
	if (received != nullptr)
	{
		const MItem* first = received->GetItem(0);
		const MItem* third = received->GetItem(2);
		CHECK(first != nullptr && first->GetID() == 601 && first->GetItemType() == 1 && first->GetNumber() == 9);
		CHECK(received->GetItem(1) == nullptr);
		CHECK(third != nullptr && third->GetID() == 603 && third->GetItemType() == 2 && third->GetNumber() == 5);
	}

	GCTradeAddItem band = Offer(700, ITEM_CLASS_OUSTERS_ARMSBAND, 3, 3);
	band.setListNum(1);
	band.addListElement(Pocket(701, ITEM_CLASS_POTION, 0, 2, 1));
	Receive(band);
	MOustersArmsBand* armsBand = dynamic_cast<MOustersArmsBand*>(world.trade.GetOtherInventory()->GetItem(3, 3));
	CHECK(armsBand != nullptr && armsBand->GetItem(1) != nullptr && armsBand->GetItem(1)->GetID() == 701);

	// A sword cannot hold items: its listed pockets are not made.
	made.clear();
	GCTradeAddItem sword = Offer(800, ITEM_CLASS_SWORD, 0, 0);
	sword.setListNum(1);
	sword.addListElement(Pocket(801, ITEM_CLASS_POTION, 0, 1, 0));
	Receive(sword);
	CHECK(made == std::vector<int>{ITEM_CLASS_SWORD});
	CHECK(world.trade.GetOtherInventory()->GetItem(0, 0) != nullptr);
}
