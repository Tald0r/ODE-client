#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "ClientPlayer.h"
#include "DialogPresentationHost.h"
#include "GameInfoPresentationHost.h"
#include "MHelpDef.h"
#include "MPriceManager.h"
#include "MShop.h"
#include "MShopShelf.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "ZoneStateHost.h"
#include "Cpackets/CGShopRequestList.h"
#include "Gpackets/GCShopList.h"
#include "Gpackets/GCShopListMysterious.h"
#include "Gpackets/GCShopVersion.h"
#include <string>
#include <vector>

// An NPC's shop list fills one rack of its shop with the items the server
// lists: an NPC without a shop is given an empty one, filled with its
// fixed and mysterious racks; the shop's kind decides the rack shown
// first; the buying rate and the castle's tax ratio are set; the shop
// window opens on it; and the shop's help is shown. A mysterious list
// fills a rack with unidentified items of a class (the type it carries is
// not used, as upstream). A shop's
// versions ask the server for the special rack when the client's is out
// of date, and open the shop when nothing is.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;

struct FakeCreature
{
	TYPE_OBJECTID id;
	bool npc;
	MShop* shop = nullptr;
};
FakeCreature merchant{1003, true}, guard{1004, false};

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* p) { return *reinterpret_cast<FakeCreature*>(p); }

struct Goods : MItem
{
	ITEM_CLASS m_Class;
	explicit Goods(ITEM_CLASS itemClass) : m_Class(itemClass) {}
	ITEM_CLASS GetItemClass() const override { return m_Class; }
};

const MItemHost items = []() {
	MItemHost host{};
	host.NewItem = [](ITEM_CLASS itemClass) -> MItem* { return new Goods(itemClass); };
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.Exists = []() { return zoneExists; };
	host.GetCreature = [](TYPE_OBJECTID id) -> MCreature* {
		return id == 1003 ? Handle(merchant) : id == 1004 ? Handle(guard) : nullptr;
	};
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.IsNPC = [](MCreature* p) { return Fake(p).npc; };
	host.GetShop = [](MCreature* p) { return Fake(p).shop; };
	host.SetShop = [](MCreature* p, MShop* pShop) {
		calls.push_back("shop given");
		Fake(p).shop = pShop;
		return true;
	};
	host.CreateFixedShelf = [](MCreature*, bool mysterious) { calls.push_back(mysterious ? "mysterious rack" : "fixed rack"); };
	host.GetID = [](MCreature* p) { return Fake(p).id; };
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.SetShop = [](MShop* pShop) { calls.push_back(pShop == merchant.shop ? "shows the shop" : "shows another shop"); };
	host.RunShop = []() { calls.push_back("shop opens"); };
	return host;
}();

const GameInfoPresentation::Host info = []() {
	GameInfoPresentation::Host host{};
	host.ExecuteHelpEvent = [](int helpEvent) { calls.push_back("help|" + N(helpEvent)); };
	return host;
}();

// The game connection, its output kept unsent for inspection.
struct GamePlayer : ClientPlayer
{
	GamePlayer() : ClientPlayer(new Socket((EnsureSocketsInitialised(), new SocketImpl()))) {}
	~GamePlayer() { setPlayerStatus(CPS_END_SESSION); }
	std::vector<unsigned char> Sent() const { return SocketOutputStreamTestAccess::Bytes(*m_pOutputStream); }
};

struct World : GameModelWorld
{
	MPriceManager prices;
	GamePlayer connection;
	MPriceManager* oldPrices = g_pPriceManager;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	const GameInfoPresentation::Host* oldInfo = GameInfoPresentation::SetHost(&info);
	World()
	{
		calls.clear();
		zoneExists = true;
		merchant = FakeCreature{1003, true};
		guard = FakeCreature{1004, false};
		g_pPriceManager = &prices;
		MItem::SetHost(&items);
	}
	~World()
	{
		delete merchant.shop;
		merchant.shop = nullptr;
		MItem::SetHost(nullptr);
		GameInfoPresentation::SetHost(oldInfo);
		DialogPresentation::SetHost(oldDialog);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		g_pPriceManager = oldPrices;
	}
	// The NPC's shop, made beforehand, with a rack of the given type.
	MShop* Owned()
	{
		merchant.shop = new MShop;
		merchant.shop->Init(MShopShelf::MAX_SHELF);
		return merchant.shop;
	}
};

struct Bytes
{
	Socket out{(EnsureSocketsInitialised(), new SocketImpl())};
	SocketOutputStream stream{&out};
	template<class T> Bytes& Put(T value) { stream.write(value); return *this; }
	template<class PacketType> void ReadInto(PacketType& packet)
	{
		const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(stream);
		Socket in((EnsureSocketsInitialised(), new SocketImpl()));
		SocketInputStream input(&in, 4096);
		SocketInputStreamTestAccess::Preload(input, bytes.data(), static_cast<unsigned int>(bytes.size()));
		packet.read(input);
		CHECK(input.isEmpty());
	}
};

struct Listed
{
	int index;
	TYPE_OBJECTID id;
	int itemClass;
	int type;
	int durability;
};

// A shop list for the NPC: its rack, version, items, rates and kind.
void ReceiveList(ObjectID_t npc, int rack, int version, std::vector<Listed> listed, int buy, int sell, int shopType = MShop::SHOP_NORMAL)
{
	using Item = SHOPLISTITEM;
	Bytes bytes;
	bytes.Put(npc).Put(static_cast<ShopVersion_t>(version)).Put(static_cast<ShopRackType_t>(rack)).Put(static_cast<BYTE>(listed.size()));
	for (const Listed& l : listed)
	{
		bytes.Put(static_cast<BYTE>(l.index)).Put(static_cast<decltype(Item::objectID)>(l.id)).Put(static_cast<decltype(Item::itemClass)>(l.itemClass))
			.Put(static_cast<decltype(Item::itemType)>(l.type)).Put(static_cast<BYTE>(0)).Put(static_cast<decltype(Item::durability)>(l.durability))
			.Put(static_cast<decltype(Item::silver)>(0)).Put(static_cast<decltype(Item::grade)>(0)).Put(static_cast<decltype(Item::enchantLevel)>(0));
	}
	bytes.Put(static_cast<MarketCond_t>(buy)).Put(static_cast<MarketCond_t>(sell)).Put(static_cast<BYTE>(shopType));
	GCShopList packet;
	bytes.ReadInto(packet);
	GCShopListHandler::execute(&packet, nullptr);
}

void ReceiveMysterious(ObjectID_t npc, int rack, int version, std::vector<Listed> listed, int buy, int sell)
{
	Bytes bytes;
	bytes.Put(npc).Put(static_cast<ShopVersion_t>(version)).Put(static_cast<ShopRackType_t>(rack)).Put(static_cast<BYTE>(listed.size()));
	for (const Listed& l : listed)
		bytes.Put(static_cast<BYTE>(l.index)).Put(static_cast<decltype(SHOPLISTITEM_MYSTERIOUS::itemClass)>(l.itemClass))
			.Put(static_cast<decltype(SHOPLISTITEM_MYSTERIOUS::itemType)>(l.type));
	bytes.Put(static_cast<MarketCond_t>(buy)).Put(static_cast<MarketCond_t>(sell));
	GCShopListMysterious packet;
	bytes.ReadInto(packet);
	GCShopListMysteriousHandler::execute(&packet, nullptr);
}

void ReceiveVersions(Player* pConnection, ObjectID_t npc, std::vector<int> versions, int sell)
{
	GCShopVersion packet;
	packet.setObjectID(npc);
	for (int rack = 0; rack < SHOP_RACK_TYPE_MAX; rack++)
		packet.setVersion(static_cast<ShopRackType_t>(rack), static_cast<ShopVersion_t>(versions[rack]));
	packet.setMarketCondSell(static_cast<MarketCond_t>(sell));
	GCShopVersionHandler::execute(&packet, pConnection);
}

const std::string kHelp = "help|" + std::to_string(HELP_EVENT_USE_SHOP);

} // namespace

TEST(ShopListReplies, AListFillsARackOfANewShopAndOpensIt)
{
	World world;
	ReceiveList(1003, MShopShelf::SHELF_SPECIAL, 7, {{0, 501, ITEM_CLASS_SWORD, 2, 300}, {5, 502, ITEM_CLASS_POTION, 1, 10}}, 40, 15);
	CHECK(calls == std::vector<std::string>({"shop given", "fixed rack", "mysterious rack",
		"shows the shop", "shop opens", "shows the shop", kHelp}));
	MShop* pShop = merchant.shop;
	CHECK(pShop != nullptr);
	if (pShop == nullptr) return;
	CHECK_EQ(static_cast<int>(MShopShelf::MAX_SHELF), static_cast<int>(pShop->GetSize()));
	CHECK(pShop->GetShopType() == MShop::SHOP_NORMAL);
	CHECK_EQ(0, static_cast<int>(pShop->GetCurrent()));
	MShopShelf* pShelf = pShop->GetShelf(MShopShelf::SHELF_SPECIAL);
	CHECK(pShelf != nullptr);
	if (pShelf == nullptr) return;
	CHECK_EQ(7, pShelf->GetVersion());
	CHECK(pShelf->IsEnable());
	MItem* pSword = pShelf->GetItem(0);
	CHECK(pSword != nullptr && pSword->GetID() == 501 && pSword->GetItemClass() == ITEM_CLASS_SWORD
		&& pSword->GetItemType() == 2 && pSword->GetCurrentDurability() == 300);
	CHECK(pShelf->GetItem(5) != nullptr && pShelf->GetItem(5)->GetID() == 502);
	CHECK(pShelf->GetItem(1) == nullptr);
	CHECK_EQ(40, world.prices.GetMarketCondBuy());
	CHECK_EQ(15, world.prices.GetShopTaxRatio());
}

TEST(ShopListReplies, AListForAShopThatExistsReplacesItsRack)
{
	World world;
	MShop* pShop = world.Owned();
	MShopShelf* pOld = MShopShelf::NewShelf(MShopShelf::SHELF_SPECIAL);
	pShop->SetShelf(MShopShelf::SHELF_SPECIAL, pOld);
	ReceiveList(1003, MShopShelf::SHELF_SPECIAL, 9, {{2, 503, ITEM_CLASS_SWORD, 0, 50}}, 30, 10);
	CHECK(calls == std::vector<std::string>({"shows the shop", "shop opens", "shows the shop", kHelp}));
	CHECK(merchant.shop == pShop);
	CHECK(pShop->GetShelf(MShopShelf::SHELF_SPECIAL) != nullptr && pShop->GetShelf(MShopShelf::SHELF_SPECIAL)->GetVersion() == 9);
	// An event-star shop shows its second rack first.
	calls.clear();
	ReceiveList(1003, MShopShelf::SHELF_SPECIAL, 10, {}, 30, 10, MShop::SHOP_EVENT_STAR);
	CHECK(pShop->GetShopType() == MShop::SHOP_EVENT_STAR);
	CHECK_EQ(1, static_cast<int>(pShop->GetCurrent()));
}

TEST(ShopListReplies, AListThatCannotBeShelvedOrShownOnlyHelps)
{
	World world;
	// An item of a class the factory refuses is left out.
	ReceiveList(1003, MShopShelf::SHELF_FIXED, 1, {{0, 504, 250, 0, 0}, {1, 505, ITEM_CLASS_SWORD, 0, 0}}, 30, 10);
	CHECK(merchant.shop != nullptr && merchant.shop->GetShelf(MShopShelf::SHELF_FIXED) != nullptr);
	if (merchant.shop && merchant.shop->GetShelf(MShopShelf::SHELF_FIXED))
	{
		CHECK(merchant.shop->GetShelf(MShopShelf::SHELF_FIXED)->GetItem(0) == nullptr);
		CHECK(merchant.shop->GetShelf(MShopShelf::SHELF_FIXED)->GetItem(1) != nullptr);
	}
	// A rack past the shop's: nothing at all, not even the help.
	calls.clear();
	ReceiveList(1003, MShopShelf::MAX_SHELF, 1, {}, 30, 10);
	CHECK(calls.empty());
	// Not an NPC, nobody, no zone: only the help.
	ReceiveList(1004, MShopShelf::SHELF_FIXED, 1, {}, 30, 10);
	ReceiveList(4242, MShopShelf::SHELF_FIXED, 1, {}, 30, 10);
	zoneExists = false;
	ReceiveList(1003, MShopShelf::SHELF_FIXED, 1, {}, 30, 10);
	CHECK(calls == std::vector<std::string>(3, kHelp));
	CHECK(guard.shop == nullptr);
}

TEST(ShopListReplies, AMysteriousListFillsARackWithUnidentifiedItems)
{
	World world;
	ReceiveMysterious(1003, MShopShelf::SHELF_UNKNOWN, 4, {{3, 0, ITEM_CLASS_SWORD, 1, 0}}, 35, 12);
	CHECK(calls == std::vector<std::string>({"shop given", "fixed rack", "mysterious rack", "shows the shop", "shop opens", "shows the shop"}));
	MShop* pShop = merchant.shop;
	CHECK(pShop != nullptr);
	if (pShop == nullptr) return;
	CHECK_EQ(0, static_cast<int>(pShop->GetCurrent()));
	MShopShelf* pShelf = pShop->GetShelf(MShopShelf::SHELF_UNKNOWN);
	CHECK(pShelf != nullptr && pShelf->GetVersion() == 4);
	if (pShelf == nullptr) return;
	CHECK(pShelf->GetItem(3) != nullptr && !pShelf->GetItem(3)->IsIdentified());
	CHECK(pShelf->GetItem(3) != nullptr && pShelf->GetItem(3)->GetItemClass() == ITEM_CLASS_SWORD);
	CHECK_EQ(35, world.prices.GetMarketCondBuy());
	CHECK_EQ(12, world.prices.GetShopTaxRatio());
	// An unknown rack type: nothing; not an NPC: nothing.
	calls.clear();
	ReceiveMysterious(1003, MShopShelf::MAX_SHELF, 4, {}, 35, 12);
	ReceiveMysterious(1004, MShopShelf::SHELF_UNKNOWN, 4, {}, 35, 12);
	zoneExists = false;
	ReceiveMysterious(1003, MShopShelf::SHELF_UNKNOWN, 4, {}, 35, 12);
	CHECK(calls.empty());
}

TEST(ShopListReplies, VersionsAskForTheSpecialRackOrOpenTheShop)
{
	World world;
	// No shop yet: the special rack is made at version 0, and the server
	// says 3, so it is asked for and the shop stays shut.
	ReceiveVersions(&world.connection, 1003, {1, 3, 2}, 20);
	CHECK(calls == std::vector<std::string>({"shop given", "fixed rack", "mysterious rack"}));
	CGShopRequestList request;
	request.setObjectID(1003);
	request.setRackType(SHOP_RACK_SPECIAL);
	Bytes expected;
	expected.Put(static_cast<const Packet*>(&request));
	CHECK(world.connection.Sent() == SocketOutputStreamTestAccess::Bytes(expected.stream));
	CHECK(merchant.shop != nullptr && merchant.shop->GetShelf(SHOP_RACK_SPECIAL) != nullptr);
	CHECK_EQ(20, world.prices.GetShopTaxRatio());
	// Up to date: the shop opens, and the other racks' versions are not
	// compared.
	calls.clear();
	GamePlayer quiet;
	merchant.shop->GetShelf(SHOP_RACK_SPECIAL)->SetVersion(3);
	ReceiveVersions(&quiet, 1003, {9, 3, 9}, 20);
	CHECK(calls == std::vector<std::string>({"fixed rack", "mysterious rack", "shop opens", "shows the shop"}));
	CHECK(quiet.Sent().empty());
	// Not an NPC, or no zone: nothing.
	calls.clear();
	ReceiveVersions(&quiet, 1004, {0, 0, 0}, 20);
	zoneExists = false;
	ReceiveVersions(&quiet, 1003, {0, 0, 0}, 20);
	CHECK(calls.empty());
}

TEST(ShopListReplies, WithoutHostsTheShopIsNeitherShownNorFilled)
{
	World world;
	DialogPresentation::SetHost(nullptr);
	GameInfoPresentation::SetHost(nullptr);
	DialogPresentation::RunShop();
	DialogPresentation::SetShop(nullptr);
	GameInfoPresentation::ExecuteHelpEvent(HELP_EVENT_USE_SHOP);
	CreatureState::SetHost(nullptr);
	CreatureState::CreateFixedShelf(Handle(merchant), true);
	CHECK(calls.empty());
	// A shop no creature takes is not kept, nor the rack built for it.
	CreatureState::Host noShops = creature;
	noShops.SetShop = nullptr;
	CreatureState::SetHost(&noShops);
	ReceiveList(1003, MShopShelf::SHELF_FIXED, 1, {{0, 501, ITEM_CLASS_SWORD, 0, 0}}, 30, 10);
	ReceiveMysterious(1003, MShopShelf::SHELF_UNKNOWN, 1, {}, 30, 10);
	ReceiveVersions(&world.connection, 1003, {0, 0, 0}, 20);
	CHECK(merchant.shop == nullptr);
	CHECK(calls.empty());
}
