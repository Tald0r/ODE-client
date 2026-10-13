#include "test_framework.h"
#include "gamemodel_world.h"
#include "packet_stream_access.h"
#include "type_table_access.h"
#include "DialogPresentationHost.h"
#include "MGameStringTable.h"
#include "MStorage.h"
#include "SubItemInfo.h"
#include "Socket.h"
#include "SocketImpl.h"
#include "SocketInputStream.h"
#include "SocketOutputStream.h"
#include "Gpackets/GCStashList.h"
#include <list>
#include <string>
#include <vector>

// The stash replaces the storage with one of the packet's racks, each item
// in its rack and slot built from the packet: a gun loaded with its count,
// anything else counted and silvered; a belt or arms band keeps the pockets
// that fit. The stashed money is set and the stash window opens on it.
// With no stash the player is told so and the storage stays.
namespace {

std::vector<std::string> calls;
int alive = 0;
bool loadable = true;

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

MItemHost FactoryHost()
{
	MItemHost host{};
	host.NewItem = [](ITEM_CLASS itemClass) -> MItem* {
		switch (itemClass)
		{
			case ITEM_CLASS_SWORD: case ITEM_CLASS_SR: case ITEM_CLASS_POTION: return new Ware(itemClass);
			case ITEM_CLASS_BELT: return new Belt;
			case ITEM_CLASS_OUSTERS_ARMSBAND: return new ArmsBand;
			default: return nullptr;
		}
	};
	host.LoadGun = [](MItem* pGun, TYPE_ITEM_NUMBER rounds) {
		calls.push_back("load|" + std::to_string(pGun->GetID()) + "|" + std::to_string(rounds));
		return loadable;
	};
	return host;
}
const MItemHost items = FactoryHost();

const DialogPresentation::Host dialog = []() {
	DialogPresentation::Host host{};
	host.RunStorage = []() { calls.push_back("run"); };
	host.SetStorage = [](MStorage* pStorage) { calls.push_back(pStorage == g_pStorage ? "show|stash" : "show|other"); };
	host.PopupFreeMessageDlg = [](const char* message) { calls.push_back("popup|" + std::string(message)); };
	return host;
}();

struct World : GameModelWorld
{
	MStorage* oldStorage = g_pStorage;
	const DialogPresentation::Host* oldDialog = DialogPresentation::SetHost(&dialog);
	World()
	{
		InitGameStringTable();
		const struct { ITEM_CLASS itemClass; int pockets; } rows[] = {
			{ITEM_CLASS_SWORD, 0}, {ITEM_CLASS_SR, 0}, {ITEM_CLASS_POTION, 0},
			{ITEM_CLASS_BELT, 2}, {ITEM_CLASS_OUSTERS_ARMSBAND, 2},
		};
		for (const auto& row : rows)
		{
			g_pItemTable->InitClass(row.itemClass, 3);
			for (int type = 0; type < 3; type++)
				testfw::MutableRow(*g_pItemTable, row.itemClass, type).SetValue(0, 0, row.pockets);
		}
		g_pStorage = new MStorage;
		g_pStorage->Init(1);
		MItem::SetHost(&items);
		calls.clear();
		alive = 0;
		loadable = true;
	}
	~World()
	{
		delete g_pStorage;
		g_pStorage = oldStorage;
		MItem::SetHost(nullptr);
		DialogPresentation::SetHost(oldDialog);
	}
	static const MItem* At(int rack, int slot)
	{
		g_pStorage->SetCurrent(rack);
		return g_pStorage->GetItem(slot);
	}
};

struct Pocket
{
	ObjectID_t id;
	BYTE itemClass;
	SlotID_t slot;
};

struct Stashed
{
	BYTE rack;
	BYTE index;
	ObjectID_t id;
	BYTE itemClass;
	ItemNum_t num;
	std::vector<Pocket> pockets;
};

// GCStashList has no item setters, so the stash is written here as the
// server sends it: the rack count, the item count, each item with its rack
// and slot, its fields and its pockets, then the money.
void Receive(BYTE racks, const std::vector<Stashed>& stash, Gold_t gold)
{
	Socket out((EnsureSocketsInitialised(), new SocketImpl()));
	SocketOutputStream encoded(&out);
	encoded.write(racks);
	encoded.write(static_cast<BYTE>(stash.size()));
	for (const Stashed& item : stash)
	{
		encoded.write(item.rack);
		encoded.write(item.index);
		encoded.write(item.id);
		encoded.write(item.itemClass);
		encoded.write(static_cast<ItemType_t>(1));
		encoded.write(static_cast<BYTE>(1));
		encoded.write(static_cast<OptionType_t>(6));
		encoded.write(static_cast<Durability_t>(40));
		encoded.write(item.num);
		encoded.write(static_cast<Silver_t>(12));
		encoded.write(static_cast<Grade_t>(2));
		encoded.write(static_cast<EnchantLevel_t>(3));
		encoded.write(static_cast<BYTE>(item.pockets.size()));
		for (const Pocket& pocket : item.pockets)
		{
			SubItemInfo info;
			info.setObjectID(pocket.id);
			info.setItemClass(pocket.itemClass);
			info.setItemType(1);
			info.setItemNum(5);
			info.setSlotID(pocket.slot);
			info.write(encoded);
		}
	}
	encoded.write(gold);
	const std::vector<unsigned char> bytes = SocketOutputStreamTestAccess::Bytes(encoded);
	Socket in((EnsureSocketsInitialised(), new SocketImpl()));
	SocketInputStream stream(&in, 4096);
	SocketInputStreamTestAccess::Preload(stream, bytes.data(), static_cast<unsigned int>(bytes.size()));
	GCStashList packet;
	packet.read(stream);
	CHECK(stream.isEmpty());
	GCStashListHandler::execute(&packet, nullptr);
}

} // namespace

TEST(StashListHandler, EachStashedItemGoesToItsRackAndSlotBuiltFromThePacket)
{
	World world;
	CHECK(g_pStorage->SetItem(5, new Ware(ITEM_CLASS_SWORD)));
	Receive(2, {{0, 0, 900, ITEM_CLASS_SWORD, 4, {}}, {1, 3, 901, ITEM_CLASS_SR, 30, {}}}, 123456);
	// The old storage and its item went.
	CHECK(World::At(0, 5) == nullptr);
	const MItem* sword = World::At(0, 0);
	CHECK(sword != nullptr);
	if (sword != nullptr)
	{
		CHECK_EQ(900, static_cast<long long>(sword->GetID()));
		CHECK_EQ(1, static_cast<long long>(sword->GetItemType()));
		CHECK(sword->GetItemOptionList() == std::list<TYPE_ITEM_OPTION>({6}));
		CHECK_EQ(40, static_cast<long long>(sword->GetCurrentDurability()));
		CHECK_EQ(4, static_cast<long long>(sword->GetNumber()));
		CHECK_EQ(12, sword->GetSilver());
		CHECK_EQ(2, sword->GetGrade());
		CHECK_EQ(3, static_cast<long long>(sword->GetEnchantLevel()));
	}
	// The gun is loaded with its count, and its number and silver are left.
	const MItem* gun = World::At(1, 3);
	CHECK(gun != nullptr && gun->GetID() == 901 && gun->GetSilver() == 0 && gun->GetNumber() == 1);
	CHECK_EQ(123456, static_cast<long long>(g_pStorage->GetMoneyManager()->GetMoney()));
	CHECK(calls == std::vector<std::string>({"load|901|30", "run", "show|stash"}));
	CHECK_EQ(2, alive);
}

TEST(StashListHandler, ABeltOrArmsBandKeepsThePocketsThatFit)
{
	World world;
	Receive(1, {{0, 0, 910, ITEM_CLASS_BELT, 1, {{911, ITEM_CLASS_POTION, 1}, {912, ITEM_CLASS_POTION, 7}, {913, ITEM_CLASS_HELM, 0}}},
		{0, 1, 920, ITEM_CLASS_OUSTERS_ARMSBAND, 1, {{921, ITEM_CLASS_POTION, 0}, {922, ITEM_CLASS_POTION, 9}}},
		{0, 2, 930, ITEM_CLASS_SWORD, 1, {{931, ITEM_CLASS_POTION, 0}}}}, 0);
	const MBelt* belt = dynamic_cast<const MBelt*>(World::At(0, 0));
	const MItem* beltPocket = belt != nullptr ? belt->GetItem(1) : nullptr;
	CHECK(beltPocket != nullptr && beltPocket->GetID() == 911 && beltPocket->GetNumber() == 5);
	const MOustersArmsBand* band = dynamic_cast<const MOustersArmsBand*>(World::At(0, 1));
	const MItem* bandPocket = band != nullptr ? band->GetItem(0) : nullptr;
	CHECK(bandPocket != nullptr && bandPocket->GetID() == 921);
	// The belt, its pocket, the band, its pocket and the sword: the pockets
	// that did not fit, and the sword's, are not kept.
	CHECK_EQ(5, alive);
}

TEST(StashListHandler, AnUnknownClassIsSkippedAndAnUnloadableGunStashedUnloaded)
{
	World world;
	loadable = false;
	Receive(1, {{0, 0, 940, ITEM_CLASS_HELM, 1, {}}, {0, 1, 941, ITEM_CLASS_SR, 9, {}}}, 0);
	CHECK(World::At(0, 0) == nullptr);
	CHECK(World::At(0, 1) != nullptr);
	CHECK(calls == std::vector<std::string>({"load|941|9", "run", "show|stash"}));
	CHECK_EQ(1, alive);
}

TEST(StashListHandler, NoStashSaysSoAndKeepsTheStorage)
{
	World world;
	MStorage* old = g_pStorage;
	Receive(0, {}, 50);
	CHECK(g_pStorage == old);
	CHECK(calls == std::vector<std::string>{"popup|" + std::string((*g_pGameStringTable)[STRING_MESSAGE_NO_STORAGE].GetString())});
}

TEST(StashListHandler, MissingServicesStillBuildTheStash)
{
	World world;
	DialogPresentation::SetHost(nullptr);
	Receive(1, {{0, 0, 900, ITEM_CLASS_SWORD, 4, {}}}, 7);
	Receive(0, {}, 0);
	CHECK(calls.empty());
	CHECK(World::At(0, 0) != nullptr);
	CHECK_EQ(7, static_cast<long long>(g_pStorage->GetMoneyManager()->GetMoney()));
}
