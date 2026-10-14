#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "CommunicationPresentationHost.h"
#include "MCreatureTable.h"
#include "MGameStringTable.h"
#include "MInventory.h"
#include "MItem.h"
#include "MItemTable.h"
#include "MStorage.h"
#include "PlayerStateHost.h"
#include "StoreInfo.h"
#include "SubItemInfo.h"
#include "VsUiPresentationHost.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCMyStoreInfo.h"
#include "Gpackets/GCOtherStoreInfo.h"
#include <string>
#include <vector>

// A personal shop's contents. The player's own: the player stops
// following anyone; unless the player's type is an NPC's, its shop is
// marked open or shut; an open shop shows its sign (or the default one) in
// the player's chat colour. Closing the shop (window mode 3) takes the
// sign away, closes the window and drops the shop's storage; opening it
// for sale (mode 2) closes the window. Otherwise the shop's items fill a
// new storage (each with its price, options, grade and enchantment, a
// gun loaded with an empty magazine of one round, a belt or arms band
// with its sub items), the player's own items for sale are marked
// personal, and the window shows the storage when the player is stocking
// it (mode 1), or when the server asks and neither window nor shop is up.
// Another creature's: its shop is marked open or shut and an open one
// shows its sign; the shop the player is looking at closing closes its
// window; a shop the player asked for fills a new storage and opens the
// window beside the inventory.
namespace {

std::vector<std::string> calls;
bool hasPlayer = true;
bool tracing = false;
int windowMode = 0;
bool windowRunning = false;
int alive = 0;

struct FakeCreature
{
	TYPE_OBJECTID id;
	int creatureType;
	int shopOpen;
};
FakeCreature me{1001, 2, 0};
FakeCreature seller{1003, 2, 0};
FakeCreature vendor{1005, 1, 0};
FakeCreature other{1007, 2, 0};

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* p) { return *reinterpret_cast<FakeCreature*>(p); }
std::string Id(MCreature* p) { return N(Fake(p).id); }

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
struct Gun : MItem
{
	Gun() { alive++; }
	~Gun() { alive--; }
	ITEM_CLASS GetItemClass() const override { return ITEM_CLASS_SR; }
	bool IsGunItem() const override { return true; }
};
struct Belt : MBelt
{
	Belt() { alive++; }
	~Belt() { alive--; }
};

MItemHost ItemHost()
{
	MItemHost host{};
	host.NewItem = [](ITEM_CLASS itemClass) -> MItem* {
		switch (itemClass)
		{
			case ITEM_CLASS_SWORD: return new Sword;
			case ITEM_CLASS_POTION: return new Potion;
			case ITEM_CLASS_SR: return new Gun;
			case ITEM_CLASS_BELT: return new Belt;
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
	host.Creature = []() { return hasPlayer ? Handle(me) : nullptr; };
	host.GetID = []() { return me.id; };
	host.IsTraceCreature = []() { return tracing; };
	host.TraceNULL = []() { calls.push_back("stops following"); };
	return host;
}();

const ZoneState::Host zone = []() {
	ZoneState::Host host{};
	host.GetCreature = [](TYPE_OBJECTID id) {
		return id == seller.id ? Handle(seller) : id == vendor.id ? Handle(vendor) : id == other.id ? Handle(other) : nullptr;
	};
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.GetCreatureType = [](MCreature* p) { return Fake(p).creatureType; };
	host.SetPersnalShop = [](MCreature* p, int open) { Fake(p).shopOpen = open; calls.push_back("shop|" + Id(p) + "|" + N(open)); };
	host.CurPernalShop = [](MCreature* p) { return Fake(p).shopOpen; };
	host.SetPersnalString = [](MCreature* p, const char* sign, std::uint32_t color) {
		calls.push_back("sign|" + Id(p) + "|" + sign + "|" + N(color));
	};
	host.ClearChatString = [](MCreature* p) { calls.push_back("sign gone|" + Id(p)); };
	host.SetPersnalShopMessage = [](MCreature* p, const char* message) { calls.push_back("message|" + Id(p) + "|" + message); };
	return host;
}();

const VsUiPresentation::Host ui = []() {
	VsUiPresentation::Host host{};
	host.GetPersnalShoptoUI = []() { return windowMode; };
	host.SetPersnalShoptoUI = [](int mode) { windowMode = mode; calls.push_back("window mode|" + N(mode)); };
	host.IsRunningPersnalShop = []() { return windowRunning; };
	host.RunPersnalShop = []() { calls.push_back("window opens"); };
	host.ClosePersnalShop = []() { calls.push_back("window closes"); };
	host.SetPersnalShop = [](MStorage* pStorage) {
		calls.push_back(pStorage == nullptr ? "window empty" : pStorage == g_pStorage2 ? "window shows the shop" : "window shows another storage");
	};
	host.SetInventoryMode = [](int mode) { calls.push_back("inventory mode|" + N(mode)); };
	return host;
}();

const CommunicationPresentation::Host communication = []() {
	CommunicationPresentation::Host host{};
	host.GetChattingColor = []() { return std::uint32_t{0x123456}; };
	return host;
}();

struct World : GameModelWorld
{
	MInventory inventory;
	CREATURE_TABLE creatureTypes;
	MInventory* oldInventory = g_pInventory;
	MStorage* oldStorage = g_pStorage2;
	CREATURE_TABLE* oldCreatureTypes = g_pCreatureTable;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const VsUiPresentation::Host* oldUi = VsUiPresentation::SetHost(&ui);
	const CommunicationPresentation::Host* oldCommunication = CommunicationPresentation::SetHost(&communication);
	World()
	{
		InitGameStringTable();
		const struct { ITEM_CLASS itemClass; int pockets; } rows[] = {
			{ITEM_CLASS_SWORD, 0}, {ITEM_CLASS_POTION, 0}, {ITEM_CLASS_BELT, 3}, {ITEM_CLASS_SR, 0}};
		for (const auto& row : rows)
		{
			g_pItemTable->InitClass(row.itemClass, 3);
			for (int type = 0; type < 3; type++)
			{
				testfw::MutableRow(*g_pItemTable, row.itemClass, type).SetGrid(1, 1);
				testfw::MutableRow(*g_pItemTable, row.itemClass, type).SetValue(0, 0, row.pockets);
			}
		}
		creatureTypes.Init(3);
		if (auto* row = creatureTypes.GetMutable(1)) row->SetCreatureTribe(CREATURETRIBE_NPC, 1);
		if (auto* row = creatureTypes.GetMutable(2)) row->SetCreatureTribe(CREATURETRIBE_SLAYER, 1);
		g_pCreatureTable = &creatureTypes;
		inventory.Init(4, 4);
		g_pInventory = &inventory;
		g_pStorage2 = nullptr;
		MItem::SetHost(&items);
		calls.clear();
		hasPlayer = true;
		tracing = false;
		windowMode = 0;
		windowRunning = false;
		me = FakeCreature{1001, 2, 0};
		seller = FakeCreature{1003, 2, 0};
		vendor = FakeCreature{1005, 1, 0};
		other = FakeCreature{1007, 2, 0};
	}
	~World()
	{
		delete g_pStorage2;
		g_pStorage2 = oldStorage;
		inventory.Release();
		MItem::SetHost(nullptr);
		CommunicationPresentation::SetHost(oldCommunication);
		VsUiPresentation::SetHost(oldUi);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		PlayerState::SetHost(oldPlayer);
		g_pCreatureTable = oldCreatureTypes;
		g_pInventory = oldInventory;
	}
};

// One slot of a shop's listing.
struct Ware
{
	int itemClass;
	ObjectID_t id;
	int price;
	std::vector<int> subPotions;	// slot IDs of potions in a belt
};

// StoreInfo has no setters, so the shop's bytes are written here.
void WriteShop(SocketOutputStream& out, bool open, const std::string& sign, const std::vector<Ware>& wares, bool toOther)
{
	out.write(static_cast<BYTE>(open));
	if (toOther && !open) return;
	out.write(static_cast<BYTE>(sign.size()));
	if (!sign.empty()) out.write(sign.data(), static_cast<unsigned int>(sign.size()));
	out.write(static_cast<BYTE>(wares.size()));
	for (const Ware& ware : wares)
	{
		StoreItemInfo info;
		if (ware.itemClass < 0)
		{
			info.write(out);
			continue;
		}
		info.setItemExist(1);
		info.setObjectID(ware.id);
		info.setItemClass(static_cast<BYTE>(ware.itemClass));
		info.setItemType(1);
		info.addOptionType(7);
		info.setDurability(300);
		info.setSilver(40);
		info.setGrade(5);
		info.setEnchantLevel(2);
		info.setItemNum(1);
		info.setPrice(ware.price);
		for (int slot : ware.subPotions)
		{
			SubItemInfo* pSub = new SubItemInfo;
			pSub->setObjectID(ware.id + 100 + slot);
			pSub->setItemClass(ITEM_CLASS_POTION);
			pSub->setItemType(0);
			pSub->setItemNum(2);
			pSub->setSlotID(static_cast<SlotID_t>(slot));
			info.addListElement(pSub);
		}
		info.setListNum(static_cast<BYTE>(ware.subPotions.size()));
		info.write(out);
	}
}

template<class PacketType>
void Read(PacketType& packet, const std::vector<unsigned char>& bytes)
{
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 8192);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	packet.read(stream);
	CHECK(stream.isEmpty());
}

void ReceiveMine(bool open, const std::string& sign, const std::vector<Ware>& wares, int openUI = 0)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	encoded.write(static_cast<BYTE>(openUI));
	WriteShop(encoded, open, sign, wares, false);
	GCMyStoreInfo packet;
	Read(packet, SocketOutputStreamTestAccess::Bytes(encoded));
	GCMyStoreInfoHandler::execute(&packet, nullptr);
}

void ReceiveOthers(ObjectID_t id, bool requested, bool open, const std::string& sign, const std::vector<Ware>& wares)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	encoded.write(static_cast<ObjectID_t>(id));
	encoded.write(static_cast<BYTE>(requested));
	WriteShop(encoded, open, sign, wares, true);
	GCOtherStoreInfo packet;
	Read(packet, SocketOutputStreamTestAccess::Bytes(encoded));
	GCOtherStoreInfoHandler::execute(&packet, nullptr);
}

std::string Shown(const MItem* pItem)
{
	if (pItem == nullptr) return "(empty)";
	MItem* p = const_cast<MItem*>(pItem);
	std::string s = N(p->GetItemClass()) + "#" + N(p->GetID()) + " type " + N(p->GetItemType()) + " $" + N(p->GetPersnalPrice())
		+ " x" + N(p->GetNumber()) + " dur " + N(p->GetCurrentDurability()) + " silver " + N(p->GetSilver())
		+ " grade " + N(p->GetGrade()) + " +" + N(p->GetEnchantLevel()) + " opt " + N(p->GetItemOptionListCount());
	if (p->GetItemClass() == ITEM_CLASS_BELT)
	{
		MBelt* pBelt = static_cast<MBelt*>(p);
		for (int slot = 0; slot < 3; slot++)
		{
			const MItem* pSub = pBelt->GetItem(slot);
			s += pSub ? " [" + N(const_cast<MItem*>(pSub)->GetID()) + " x" + N(const_cast<MItem*>(pSub)->GetNumber()) + "]" : " []";
		}
	}
	return s;
}

} // namespace

TEST(PersonalShopReplies, AnotherCreaturesShopTheyAskedForOpensBesideTheInventory)
{
	World world;
	ReceiveOthers(1003, true, true, "Cheap swords", {{ITEM_CLASS_SWORD, 501, 900, {}}, {-1, 0, 0, {}},
		{ITEM_CLASS_SR, 502, 700, {}}, {ITEM_CLASS_BELT, 503, 300, {0, 2}}});
	CHECK(calls == std::vector<std::string>({"shop|1003|1", "sign|1003|Cheap swords|" + N(0x123456), "message|1003|Cheap swords",
		"loads|502|1", "inventory mode|2", "window opens", "inventory mode|2", "window shows the shop"}));
	CHECK(g_pStorage2 != nullptr);
	if (g_pStorage2 == nullptr) return;
	CHECK_EQ(1003u, static_cast<unsigned>(g_pStorage2->GetCuropenid()));
	CHECK(Shown(g_pStorage2->GetItem(0)) == N(ITEM_CLASS_SWORD) + "#501 type 1 $900 x1 dur 300 silver 40 grade 5 +2 opt 1");
	CHECK(Shown(g_pStorage2->GetItem(1)) == "(empty)");
	// A gun takes no silver.
	CHECK(Shown(g_pStorage2->GetItem(2)) == N(ITEM_CLASS_SR) + "#502 type 1 $700 x1 dur 300 silver 0 grade 5 +2 opt 1");
	CHECK(Shown(g_pStorage2->GetItem(3)) == N(ITEM_CLASS_BELT) + "#503 type 1 $300 x1 dur 300 silver 40 grade 5 +2 opt 1 [603 x2] [] [605 x2]");
}

TEST(PersonalShopReplies, AnotherCreaturesShopNewsOnlyMarksIt)
{
	World world;
	ReceiveOthers(1003, false, true, "", {{ITEM_CLASS_SWORD, 501, 900, {}}});
	const std::string defaultSign = (*g_pGameStringTable)[UI_STRING_MESSAGE_PERSNAL_DEFAULT_MESSGE].GetString();
	CHECK(calls == std::vector<std::string>({"shop|1003|1", "sign|1003|" + defaultSign + "|" + N(0x123456)}));
	CHECK(g_pStorage2 == nullptr);
	// Shut: marked, no sign.
	calls.clear();
	ReceiveOthers(1003, false, false, "", {});
	CHECK(calls == std::vector<std::string>{"shop|1003|0"});
	// An NPC's type is left alone; nobody, nothing.
	calls.clear();
	ReceiveOthers(1005, false, true, "Wares", {});
	ReceiveOthers(4242, true, true, "Wares", {});
	CHECK(calls.empty());
}

TEST(PersonalShopReplies, TheShopThePlayerLooksAtClosingClosesItsWindow)
{
	World world;
	ReceiveOthers(1003, true, true, "Cheap swords", {{ITEM_CLASS_SWORD, 501, 900, {}}});
	MStorage* pShop = g_pStorage2;
	calls.clear();
	windowRunning = true;
	ReceiveOthers(1003, true, false, "", {});
	CHECK(calls == std::vector<std::string>({"shop|1003|0", "window closes"}));
	CHECK(g_pStorage2 == pShop);
	// Another seller's shop shut does not touch it; without the window up,
	// there is nothing to close.
	calls.clear();
	ReceiveOthers(1007, false, false, "", {});
	CHECK(calls == std::vector<std::string>{"shop|1007|0"});
	calls.clear();
	windowRunning = false;
	ReceiveOthers(1003, true, false, "", {});
	CHECK(calls == std::vector<std::string>{"shop|1003|0"});
}

TEST(PersonalShopReplies, ThePlayersShopIsStockedAndShownWhenStocking)
{
	World world;
	tracing = true;
	windowMode = 1;
	// The player's own sword for sale is in the inventory.
	Sword* pMine = new Sword;
	pMine->SetID(501);
	pMine->SetItemType(1);
	CHECK(world.inventory.AddItem(pMine));
	ReceiveMine(true, "Fine wares", {{ITEM_CLASS_SWORD, 501, 900, {}}});
	CHECK(calls == std::vector<std::string>({"stops following", "shop|1001|1", "sign|1001|Fine wares|" + N(0x123456),
		"message|1001|Fine wares", "window opens", "window shows the shop"}));
	CHECK(pMine->GetPersnal());
	CHECK(world.inventory.GetItem(pMine->GetGridX(), pMine->GetGridY()) == pMine);
	CHECK(g_pStorage2 != nullptr && g_pStorage2->GetCuropenid() == 1001);
	if (g_pStorage2 != nullptr)
		CHECK(Shown(g_pStorage2->GetItem(0)) == N(ITEM_CLASS_SWORD) + "#501 type 1 $900 x1 dur 300 silver 40 grade 5 +2 opt 1");
}

TEST(PersonalShopReplies, ThePlayersShopWindowOpensOnlyWhenAskedAndNotUp)
{
	World world;
	ReceiveMine(true, "", {}, 1);
	const std::string defaultSign = (*g_pGameStringTable)[UI_STRING_MESSAGE_PERSNAL_DEFAULT_MESSGE].GetString();
	CHECK(calls == std::vector<std::string>({"shop|1001|1", "sign|1001|" + defaultSign + "|" + N(0x123456)}));
	// The server asks, and neither the window nor the shop is up.
	calls.clear();
	me.shopOpen = 0;
	ReceiveMine(false, "", {}, 1);
	CHECK(calls == std::vector<std::string>({"shop|1001|0", "window opens", "window shows the shop"}));
	// Not asked, or the window up: no window.
	calls.clear();
	ReceiveMine(false, "", {}, 0);
	windowRunning = true;
	ReceiveMine(false, "", {}, 1);
	CHECK(calls == std::vector<std::string>(2, "shop|1001|0"));
	// A player whose type is an NPC's has its shop left alone.
	calls.clear();
	me.creatureType = 1;
	ReceiveMine(true, "", {}, 0);
	CHECK(calls == std::vector<std::string>{"sign|1001|" + defaultSign + "|" + N(0x123456)});
}

TEST(PersonalShopReplies, ClosingThePlayersShopDropsItsStorage)
{
	World world;
	ReceiveMine(true, "Fine wares", {{ITEM_CLASS_SWORD, 501, 900, {}}});
	CHECK(g_pStorage2 != nullptr);
	calls.clear();
	windowMode = 3;
	ReceiveMine(false, "", {});
	CHECK(calls == std::vector<std::string>({"shop|1001|0", "sign gone|1001", "window mode|0", "window closes", "window empty"}));
	CHECK(g_pStorage2 == nullptr);
	CHECK_EQ(0, alive);
	// Opening it for sale closes the window and keeps the storage.
	ReceiveMine(true, "Fine wares", {{ITEM_CLASS_SWORD, 501, 900, {}}});
	MStorage* pShop = g_pStorage2;
	calls.clear();
	windowMode = 2;
	ReceiveMine(true, "Fine wares", {});
	CHECK(calls == std::vector<std::string>({"shop|1001|1", "sign|1001|Fine wares|" + N(0x123456), "window mode|0", "window closes"}));
	CHECK(g_pStorage2 == pShop);
}

TEST(PersonalShopReplies, WithoutThePlayerOrTheHostsLittleHappens)
{
	World world;
	hasPlayer = false;
	ReceiveMine(true, "Fine wares", {{ITEM_CLASS_SWORD, 501, 900, {}}});
	CHECK(calls.empty());
	CHECK(g_pStorage2 == nullptr);
	VsUiPresentation::SetHost(nullptr);
	CreatureState::SetHost(nullptr);
	PlayerState::SetHost(nullptr);
	CommunicationPresentation::SetHost(nullptr);
	CHECK_EQ(0, VsUiPresentation::GetPersnalShoptoUI());
	CHECK(!VsUiPresentation::IsRunningPersnalShop());
	CHECK_EQ(0, CreatureState::CurPernalShop(Handle(seller)));
	CHECK(!PlayerState::IsTraceCreature());
	CHECK_EQ(0xFFFFFFu, CommunicationPresentation::GetChattingColor());
	VsUiPresentation::SetPersnalShoptoUI(1);
	VsUiPresentation::RunPersnalShop();
	VsUiPresentation::ClosePersnalShop();
	VsUiPresentation::SetPersnalShop(nullptr);
	VsUiPresentation::SetInventoryMode(2);
	CreatureState::SetPersnalShop(Handle(seller), 1);
	CreatureState::SetPersnalString(Handle(seller), "x", 1);
	CreatureState::ClearChatString(Handle(seller));
	CreatureState::SetPersnalShopMessage(Handle(seller), "x");
	PlayerState::TraceNULL();
	CHECK(calls.empty());
}
