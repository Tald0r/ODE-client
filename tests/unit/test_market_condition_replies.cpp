#include "test_framework.h"
#include "gamemodel_world.h"
#include "DialogPresentationHost.h"
#include "MPriceManager.h"
#include "MShop.h"
#include "MShopShelf.h"
#include "ZoneStateHost.h"
#include "Gpackets/GCShopMarketCondition.h"
#include <string>
#include <vector>

// A shop's market condition comes before the player sells to its NPC: an
// NPC without a shop is given an empty one, the price manager takes the
// rates the NPC buys and sells at, and the inventory opens to sell. A
// creature that is not an NPC, or not in the zone, is ignored.
namespace {

std::vector<std::string> calls;
bool zoneExists = true;

struct FakeCreature
{
	bool npc;
	MShop* shop = nullptr;
};
FakeCreature merchant{true}, guard{false};

MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* p) { return *reinterpret_cast<FakeCreature*>(p); }

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
		if (!Fake(p).npc) return false;
		calls.push_back("shop given");
		Fake(p).shop = pShop;
		return true;
	};
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	// The rates are set by the time the inventory opens.
	host.OpenInventoryToSell = []() { calls.push_back("inventory to sell|" + std::to_string(g_pPriceManager->GetMarketCondBuy())); };
	return host;
}();

struct World : GameModelWorld
{
	MPriceManager prices;
	MPriceManager* oldPrices = g_pPriceManager;
	const ZoneState::Host* oldZone = ZoneState::SetHost(&zone);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	World()
	{
		calls.clear();
		zoneExists = true;
		merchant = FakeCreature{true};
		guard = FakeCreature{false};
		g_pPriceManager = &prices;
	}
	~World()
	{
		DialogPresentation::SetHost(oldDialog);
		CreatureState::SetHost(oldCreature);
		ZoneState::SetHost(oldZone);
		g_pPriceManager = oldPrices;
		delete merchant.shop;
		merchant.shop = nullptr;
	}
};

void Receive(ObjectID_t id, int buy, int sell)
{
	GCShopMarketCondition packet;
	packet.setObjectID(id);
	packet.setMarketCondBuy(static_cast<MarketCond_t>(buy));
	packet.setMarketCondSell(static_cast<MarketCond_t>(sell));
	GCShopMarketConditionHandler::execute(&packet, nullptr);
}

} // namespace

TEST(MarketConditionReplies, AnNPCWithoutAShopGetsOneAndTheRatesAreSet)
{
	World world;
	Receive(1003, 40, 90);
	CHECK(calls == std::vector<std::string>({"shop given", "inventory to sell|40"}));
	CHECK(merchant.shop != nullptr);
	if (merchant.shop) CHECK_EQ(static_cast<int>(MShopShelf::MAX_SHELF), static_cast<int>(merchant.shop->GetSize()));
	CHECK_EQ(40, world.prices.GetMarketCondBuy());
	CHECK_EQ(90, world.prices.GetMarketCondSell());
	// With a shop already, it is kept.
	calls.clear();
	MShop* pShop = merchant.shop;
	Receive(1003, 30, 80);
	CHECK(calls == std::vector<std::string>{"inventory to sell|30"});
	CHECK(merchant.shop == pShop);
	CHECK_EQ(30, world.prices.GetMarketCondBuy());
	CHECK_EQ(80, world.prices.GetMarketCondSell());
}

TEST(MarketConditionReplies, AnythingButAnNPCInTheZoneIsIgnored)
{
	World world;
	Receive(1004, 40, 90);
	Receive(4242, 40, 90);
	zoneExists = false;
	Receive(1003, 40, 90);
	CHECK(calls.empty());
	CHECK(merchant.shop == nullptr);
	CHECK_EQ(25, world.prices.GetMarketCondBuy());
	CHECK_EQ(100, world.prices.GetMarketCondSell());
}

TEST(MarketConditionReplies, AShopNobodyTakesIsNotKept)
{
	World world;
	// An NPC host that cannot hand the shop over: the reply frees it (ASan
	// would see the leak only with leak checks on; the shop is not kept).
	CreatureState::Host noShops = creature;
	noShops.SetShop = nullptr;
	CreatureState::SetHost(&noShops);
	Receive(1003, 40, 90);
	CHECK(calls == std::vector<std::string>{"inventory to sell|40"});
	CHECK(merchant.shop == nullptr);
	CHECK(!CreatureState::SetShop(Handle(merchant), nullptr));
	// Without a dialog host the inventory is not opened.
	calls.clear();
	CreatureState::SetHost(&creature);
	DialogPresentation::SetHost(nullptr);
	Receive(1003, 40, 90);
	CHECK(calls == std::vector<std::string>{"shop given"});
}
