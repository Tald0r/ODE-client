#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "BonusSkillHost.h"
#include "DialogPresentationHost.h"
#include "ItemSkillIcons.h"
#include "MInventory.h"
#include "MMoneyManager.h"
#include "MPriceManager.h"
#include "MShop.h"
#include "MShopShelf.h"
#include "MStorage.h"
#include "PlayerStateHost.h"
#include "TempInformation.h"
#include "WorldEventsHost.h"
#include "ZoneStateHost.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCShopBuyOK.h"
#include "Gpackets/GCShopSellOK.h"
#include <list>
#include <string>
#include <vector>

// A purchase the player waited for: the item is made and put in the
// inventory cell the player chose, replacing a smaller stack of the same
// item there, or freed when it does not fit; a special shelf loses it and
// the shelf takes the shop's new version. A normal shop sets the money, a
// star shop takes the stars the item costs. A sale the player waited for:
// the item leaves the inventory (every skull, for a sale of all skulls),
// and the money is set; a sale from storage removes the item there too and
// adds the money. Either way the mode ends and trading is unlocked, and
// an item that changes a skill icon refreshes the usable skills.
namespace {

std::vector<std::string> calls;
bool hasPlayer = true;

struct FakeCreature
{
	bool slayer = true;
	bool vampire = false;
};
FakeCreature me;

std::string N(long long value) { return std::to_string(value); }
MCreature* Handle(FakeCreature& creature) { return reinterpret_cast<MCreature*>(&creature); }
FakeCreature& Fake(MCreature* pCreature) { return *reinterpret_cast<FakeCreature*>(pCreature); }

// Any item the factory makes; potions, stars and skulls stack.
struct Goods : MItem
{
	ITEM_CLASS m_Class;
	explicit Goods(ITEM_CLASS itemClass) : m_Class(itemClass) {}
	ITEM_CLASS GetItemClass() const override { return m_Class; }
	bool IsPileItem() const override
	{
		return m_Class == ITEM_CLASS_POTION || m_Class == ITEM_CLASS_EVENT_STAR || m_Class == ITEM_CLASS_SKULL;
	}
	TYPE_ITEM_NUMBER GetMaxNumber() const override { return IsPileItem() ? 20 : 1; }
};
MItem* Make(ITEM_CLASS itemClass, TYPE_OBJECTID id, int type = 0, int number = 1)
{
	MItem* const pItem = new Goods(itemClass);
	pItem->SetID(id);
	pItem->SetItemType(static_cast<TYPE_ITEMTYPE>(type));
	pItem->SetNumber(static_cast<TYPE_ITEM_NUMBER>(number));
	return pItem;
}

const MItemHost items = []() {
	MItemHost host{};
	host.NewItem = [](ITEM_CLASS itemClass) -> MItem* { return new Goods(itemClass); };
	host.ItemDestroyed = [](MItem* pItem) noexcept { calls.push_back("freed|" + N(pItem->GetID())); };
	return host;
}();

const PlayerState::Host player = []() {
	PlayerState::Host host{};
	host.Creature = []() { return hasPlayer ? Handle(me) : nullptr; };
	return host;
}();

const CreatureState::Host creature = []() {
	CreatureState::Host host{};
	host.IsSlayer = [](MCreature* p) { return Fake(p).slayer; };
	host.IsVampire = [](MCreature* p) { return Fake(p).vampire; };
	return host;
}();

const BonusSkills::Host bonus = []() {
	BonusSkills::Host host{};
	host.RefreshAvailableSkills = []() { calls.push_back("refresh"); };
	return host;
}();

const WorldEvents::Host events = []() {
	WorldEvents::Host host{};
	host.PlaySound = [](TYPE_SOUNDID soundID) { calls.push_back("sound|" + N(soundID)); };
	return host;
}();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.UnlockItemTrade = []() { calls.push_back("unlocked"); };
	return host;
}();

const ITEM_CLASS kClasses[] = {ITEM_CLASS_SWORD, ITEM_CLASS_POTION, ITEM_CLASS_EVENT_STAR, ITEM_CLASS_SKULL,
	ITEM_CLASS_HOLYWATER, ITEM_CLASS_VAMPIRE_PORTAL_ITEM};

struct World : GameModelWorld
{
	MShop shop;
	MInventory inventory;
	MMoneyManager money;
	MPriceManager prices;
	TempInformation temp;
	MStorage storage;
	MInventory* oldInventory = g_pInventory;
	MMoneyManager* oldMoney = g_pMoneyManager;
	MPriceManager* oldPrices = g_pPriceManager;
	TempInformation* oldTemp = g_pTempInformation;
	MStorage* oldStorage = g_pStorage2;
	const PlayerState::Host* oldPlayer = PlayerState::SetHost(&player);
	const CreatureState::Host* oldCreature = CreatureState::SetHost(&creature);
	const BonusSkills::Host* oldBonus = BonusSkills::SetHost(&bonus);
	const WorldEvents::Host* oldEvents = WorldEvents::SetHost(&events);
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	World() : GameModelWorld(2)
	{
		calls.clear();
		hasPlayer = true;
		me = FakeCreature();
		for (ITEM_CLASS itemClass : kClasses)
		{
			g_pItemTable->InitClass(itemClass, 3);
			for (int type = 0; type < 3; type++)
			{
				testfw::MutableRow(*g_pItemTable, itemClass, type).SetGrid(1, 1);
				testfw::MutableRow(*g_pItemTable, itemClass, type).TileSoundID = static_cast<TYPE_SOUNDID>(100 + itemClass);
				testfw::MutableRow(*g_pItemTable, itemClass, type).InventorySoundID = static_cast<TYPE_SOUNDID>(200 + itemClass);
			}
		}
		testfw::MutableRow(*g_pItemOptionTable, 1).Part = ITEMOPTION_TABLE::PART_DAMAGE;
		shop.Init(MShopShelf::MAX_SHELF);
		CHECK(shop.SetShelf(MShopShelf::SHELF_FIXED, MShopShelf::NewShelf(MShopShelf::SHELF_FIXED)));
		CHECK(shop.SetShelf(MShopShelf::SHELF_SPECIAL, MShopShelf::NewShelf(MShopShelf::SHELF_SPECIAL)));
		inventory.Init(10, 6);
		money.SetMoney(1000);
		storage.Init(2);
		g_pInventory = &inventory;
		g_pMoneyManager = &money;
		g_pPriceManager = &prices;
		g_pTempInformation = &temp;
		g_pStorage2 = nullptr;
		MItem::SetHost(&items);
	}
	~World()
	{
		MItem::SetHost(nullptr);
		DialogPresentation::SetHost(oldDialog);
		WorldEvents::SetHost(oldEvents);
		BonusSkills::SetHost(oldBonus);
		CreatureState::SetHost(oldCreature);
		PlayerState::SetHost(oldPlayer);
		inventory.Release();
		storage.Release();
		shop.Release();
		g_pStorage2 = oldStorage;
		g_pTempInformation = oldTemp;
		g_pPriceManager = oldPrices;
		g_pMoneyManager = oldMoney;
		g_pInventory = oldInventory;
	}
	// The player waits on a purchase from this shelf into cell (x, y).
	void Buying(int shelf, int index, int x, int y)
	{
		temp.SetMode(TempInformation::MODE_SHOP_BUY);
		temp.pValue = &shop;
		temp.Value1 = shelf;
		temp.Value2 = index;
		temp.Value3 = x;
		temp.Value4 = y;
	}
	// The player waits on the sale of this item.
	void Selling(MItem* pItem)
	{
		temp.SetMode(TempInformation::MODE_SHOP_SELL);
		temp.pValue = pItem;
	}
	MItem* At(int x, int y) { return const_cast<MItem*>(inventory.GetItem(static_cast<BYTE>(x), static_cast<BYTE>(y))); }
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

void ReceiveBought(ITEM_CLASS itemClass, ObjectID_t id, int type = 1, int number = 1, std::list<OptionType_t> options = {})
{
	GCShopBuyOK original;
	original.setObjectID(1003);
	original.setShopVersion(77);
	original.setItemObjectID(id);
	original.setItemClass(static_cast<BYTE>(itemClass));
	original.setItemType(static_cast<ItemType_t>(type));
	original.setOptionType(options);
	original.setDurability(50);
	original.setItemNum(static_cast<ItemNum_t>(number));
	original.setSilver(3);
	original.setGrade(4);
	original.setEnchantLevel(5);
	original.setPrice(640);
	GCShopBuyOK packet;
	Decode(packet, original);
	GCShopBuyOKHandler::execute(&packet, nullptr);
}

void ReceiveSold(ObjectID_t id, int version = 0)
{
	GCShopSellOK original;
	original.setObjectID(1003);
	original.setShopVersion(static_cast<ShopVersion_t>(version));
	original.setItemObjectID(id);
	original.setPrice(1500);
	GCShopSellOK packet;
	Decode(packet, original);
	GCShopSellOKHandler::execute(&packet, nullptr);
}

std::string Tile(ITEM_CLASS itemClass) { return "sound|" + N(100 + itemClass); }
std::string Pocket(ITEM_CLASS itemClass) { return "sound|" + N(200 + itemClass); }

} // namespace

TEST(ShopReplies, APurchaseGoesIntoTheChosenCellAndSetsTheMoney)
{
	World world;
	world.Buying(MShopShelf::SHELF_FIXED, 4, 2, 3);
	ReceiveBought(ITEM_CLASS_SWORD, 501, 1, 1, {1});
	CHECK(calls == std::vector<std::string>({Tile(ITEM_CLASS_SWORD), "unlocked"}));
	MItem* const pItem = world.At(2, 3);
	CHECK(pItem != nullptr);
	if (pItem != nullptr)
	{
		CHECK_EQ(501, static_cast<int>(pItem->GetID()));
		CHECK_EQ(1, static_cast<int>(pItem->GetItemType()));
		CHECK_EQ(1, pItem->GetItemOptionListCount());
		CHECK_EQ(50, static_cast<int>(pItem->GetCurrentDurability()));
		CHECK_EQ(3, static_cast<int>(pItem->GetSilver()));
		CHECK_EQ(4, static_cast<int>(pItem->GetGrade()));
		CHECK_EQ(5, static_cast<int>(pItem->GetEnchantLevel()));
	}
	CHECK_EQ(640, world.money.GetMoney());
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	CHECK_EQ(77, world.shop.GetShelf(MShopShelf::SHELF_FIXED)->GetVersion());
}

TEST(ShopReplies, ASpecialShelfLosesWhatWasBought)
{
	World world;
	MShopShelf* const pSpecial = world.shop.GetShelf(MShopShelf::SHELF_SPECIAL);
	MShopShelf* const pFixed = world.shop.GetShelf(MShopShelf::SHELF_FIXED);
	CHECK(pSpecial->SetItem(4, Make(ITEM_CLASS_SWORD, 41)));
	CHECK(pFixed->SetItem(4, Make(ITEM_CLASS_SWORD, 42)));
	world.Buying(MShopShelf::SHELF_SPECIAL, 4, 0, 0);
	ReceiveBought(ITEM_CLASS_SWORD, 501);
	CHECK(pSpecial->GetItem(4) == nullptr);
	CHECK_EQ(77, pSpecial->GetVersion());
	world.Buying(MShopShelf::SHELF_FIXED, 4, 1, 0);
	ReceiveBought(ITEM_CLASS_SWORD, 502);
	CHECK(pFixed->GetItem(4) != nullptr);
	CHECK(std::find(calls.begin(), calls.end(), "freed|41") != calls.end());
	CHECK(std::find(calls.begin(), calls.end(), "freed|42") == calls.end());
}

TEST(ShopReplies, AnItemThatChangesASkillIconRefreshesTheSkills)
{
	World world;
	world.Buying(MShopShelf::SHELF_FIXED, 0, 0, 0);
	ReceiveBought(ITEM_CLASS_HOLYWATER, 501);
	me = {false, true};
	world.Buying(MShopShelf::SHELF_FIXED, 0, 1, 0);
	ReceiveBought(ITEM_CLASS_VAMPIRE_PORTAL_ITEM, 502);
	CHECK(calls == std::vector<std::string>({Tile(ITEM_CLASS_HOLYWATER), "refresh", "unlocked",
		Tile(ITEM_CLASS_VAMPIRE_PORTAL_ITEM), "refresh", "unlocked"}));
	// The other race's items, any race's other items, or no player: none.
	calls.clear();
	world.Buying(MShopShelf::SHELF_FIXED, 0, 2, 0);
	ReceiveBought(ITEM_CLASS_HOLYWATER, 503);
	me = {true, false};
	world.Buying(MShopShelf::SHELF_FIXED, 0, 3, 0);
	ReceiveBought(ITEM_CLASS_VAMPIRE_PORTAL_ITEM, 504);
	world.Buying(MShopShelf::SHELF_FIXED, 0, 4, 0);
	ReceiveBought(ITEM_CLASS_SWORD, 505);
	hasPlayer = false;
	world.Buying(MShopShelf::SHELF_FIXED, 0, 5, 0);
	ReceiveBought(ITEM_CLASS_HOLYWATER, 506);
	CHECK(calls == std::vector<std::string>({Tile(ITEM_CLASS_HOLYWATER), "unlocked", Tile(ITEM_CLASS_VAMPIRE_PORTAL_ITEM), "unlocked",
		Tile(ITEM_CLASS_SWORD), "unlocked", Tile(ITEM_CLASS_HOLYWATER), "unlocked"}));
}

TEST(ShopReplies, AFullCellFreesThePurchaseAndASmallerStackIsReplaced)
{
	World world;
	CHECK(world.inventory.AddItem(Make(ITEM_CLASS_SWORD, 41), 2, 3));
	world.Buying(MShopShelf::SHELF_FIXED, 0, 2, 3);
	ReceiveBought(ITEM_CLASS_SWORD, 501);
	CHECK(calls == std::vector<std::string>({"freed|501", "unlocked"}));
	CHECK_EQ(41, static_cast<int>(world.At(2, 3)->GetID()));
	CHECK_EQ(640, world.money.GetMoney());
	// A stack of the same potion is replaced by the bought one, which
	// carries the new total.
	calls.clear();
	CHECK(world.inventory.AddItem(Make(ITEM_CLASS_POTION, 42, 1, 5), 4, 4));
	world.Buying(MShopShelf::SHELF_FIXED, 0, 4, 4);
	ReceiveBought(ITEM_CLASS_POTION, 502, 1, 8);
	CHECK(calls == std::vector<std::string>({"freed|42", Tile(ITEM_CLASS_POTION), "unlocked"}));
	CHECK_EQ(8, static_cast<int>(world.At(4, 4)->GetNumber()));
	// Past the stack's maximum the old stack stays and the purchase goes.
	calls.clear();
	world.Buying(MShopShelf::SHELF_FIXED, 0, 4, 4);
	ReceiveBought(ITEM_CLASS_POTION, 503, 1, 21);
	CHECK(calls == std::vector<std::string>({"freed|503", "unlocked"}));
	CHECK_EQ(502, static_cast<int>(world.At(4, 4)->GetID()));
}

TEST(ShopReplies, AStarShopTakesTheStarsTheItemCosts)
{
	World world;
	world.shop.SetShopType(MShop::SHOP_EVENT_STAR);
	// Two stacks of black stars (type 0): 15, then 10, in grid order.
	CHECK(world.inventory.AddItem(Make(ITEM_CLASS_EVENT_STAR, 61, 0, 15), 0, 0));
	CHECK(world.inventory.AddItem(Make(ITEM_CLASS_EVENT_STAR, 62, 0, 10), 1, 0));
	// A damage option asks black stars; a type 2 item costs (2 - 1) * 20.
	world.Buying(MShopShelf::SHELF_FIXED, 0, 5, 5);
	ReceiveBought(ITEM_CLASS_SWORD, 501, 2, 1, {1});
	CHECK(calls == std::vector<std::string>({Tile(ITEM_CLASS_SWORD), "freed|61", "unlocked"}));
	CHECK(world.At(0, 0) == nullptr);
	CHECK_EQ(5, static_cast<int>(world.At(1, 0)->GetNumber()));
	CHECK_EQ(1000, world.money.GetMoney());
}

TEST(ShopReplies, APurchaseNotWaitedForChangesNothing)
{
	World world;
	ReceiveBought(ITEM_CLASS_SWORD, 501);
	CHECK(calls.empty());
	CHECK(world.At(0, 0) == nullptr);
	CHECK_EQ(1000, world.money.GetMoney());
}

TEST(ShopReplies, ASaleTakesTheItemAndSetsTheMoney)
{
	World world;
	MItem* const pHolyWater = Make(ITEM_CLASS_HOLYWATER, 41);
	CHECK(world.inventory.AddItem(pHolyWater, 2, 3));
	world.Selling(pHolyWater);
	ReceiveSold(41);
	CHECK(calls == std::vector<std::string>({Pocket(ITEM_CLASS_HOLYWATER), "refresh", "freed|41", "unlocked"}));
	CHECK(world.At(2, 3) == nullptr);
	CHECK_EQ(1500, world.money.GetMoney());
	CHECK(world.temp.GetMode() == TempInformation::MODE_NULL);
	// Another item in its cell is not sold.
	calls.clear();
	MItem* const pSword = Make(ITEM_CLASS_SWORD, 42);
	CHECK(world.inventory.AddItem(pSword, 2, 3));
	MItem* const pStale = Make(ITEM_CLASS_SWORD, 43);
	pStale->SetGridXY(2, 3);
	world.Selling(pStale);
	ReceiveSold(43);
	CHECK(calls == std::vector<std::string>{"unlocked"});
	CHECK(world.At(2, 3) == pSword);
	delete pStale;
}

TEST(ShopReplies, SellingAllSkullsTakesEverySkull)
{
	World world;
	CHECK(world.inventory.AddItem(Make(ITEM_CLASS_SKULL, 41), 0, 0));
	CHECK(world.inventory.AddItem(Make(ITEM_CLASS_SKULL, 42, 1), 3, 0));
	CHECK(world.inventory.AddItem(Make(ITEM_CLASS_SWORD, 43), 5, 0));
	world.temp.SetMode(TempInformation::MODE_SHOP_SELL_ALL_SKULL);
	ReceiveSold(0);
	CHECK(calls == std::vector<std::string>({Pocket(ITEM_CLASS_SKULL), "freed|41", "freed|42", "unlocked"}));
	CHECK(world.At(5, 0) != nullptr);
	CHECK_EQ(1500, world.money.GetMoney());
}

TEST(ShopReplies, ASaleFromStorageRemovesItThereAndAddsTheMoney)
{
	World world;
	CHECK(world.inventory.AddItem(Make(ITEM_CLASS_SWORD, 41), 0, 0));
	ReceiveSold(41, -1);
	CHECK(calls == std::vector<std::string>({"freed|41", "unlocked"}));
	CHECK_EQ(2500, world.money.GetMoney());
	// With storage, the item with the packet's object ID leaves its first
	// page too.
	calls.clear();
	g_pStorage2 = &world.storage;
	world.storage.SetCurrent(0);
	CHECK(world.storage.SetItem(3, Make(ITEM_CLASS_SWORD, 1003)));
	ReceiveSold(4242, -1);
	CHECK(calls == std::vector<std::string>({"freed|1003", "unlocked"}));
	CHECK(world.storage.GetItem(3) == nullptr);
	CHECK_EQ(4000, world.money.GetMoney());
	g_pStorage2 = nullptr;
}

TEST(ShopReplies, ASaleNotWaitedForChangesNothing)
{
	World world;
	CHECK(world.inventory.AddItem(Make(ITEM_CLASS_SWORD, 41), 0, 0));
	ReceiveSold(41);
	CHECK(calls.empty());
	CHECK(world.At(0, 0) != nullptr);
}

TEST(ShopReplies, EachListedItemChangesItsRacesSkillIconsAndNoOther)
{
	World world;
	const ITEM_CLASS slayers[] = {ITEM_CLASS_HOLYWATER, ITEM_CLASS_SLAYER_PORTAL_ITEM, ITEM_CLASS_BOMB, ITEM_CLASS_MINE,
		ITEM_CLASS_BOMB_MATERIAL};
	const ITEM_CLASS vampires[] = {ITEM_CLASS_VAMPIRE_PORTAL_ITEM, ITEM_CLASS_VAMPIRE_ETC};
	const auto refreshes = [](ITEM_CLASS itemClass) {
		calls.clear();
		Goods item(itemClass);
		CheckItemForSkillIcon(&item);
		return calls == std::vector<std::string>{"refresh"};
	};
	for (ITEM_CLASS itemClass : slayers) CHECK(refreshes(itemClass));
	for (ITEM_CLASS itemClass : vampires) CHECK(!refreshes(itemClass));
	me = {false, true};
	for (ITEM_CLASS itemClass : vampires) CHECK(refreshes(itemClass));
	for (ITEM_CLASS itemClass : slayers) CHECK(!refreshes(itemClass));
	// An ousters is neither.
	me = {false, false};
	for (ITEM_CLASS itemClass : slayers) CHECK(!refreshes(itemClass));
	for (ITEM_CLASS itemClass : vampires) CHECK(!refreshes(itemClass));
}
