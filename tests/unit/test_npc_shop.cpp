#include "test_framework.h"
#include "gamemodel_world.h"
#include "type_table_access.h"
#include "NPCShopBuilder.h"
#include "MShop.h"
#include "MShopShelf.h"
#include "MShopTemplateTable.h"
#include "MNPCTable.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int alive = 0, destroyed = 0, genderReads = 0;
bool female = false, refuseItems = false;
std::vector<ITEM_CLASS> requestedClasses;
struct Destination { MItem* item; int type, zone, x, y, grade, number; };
std::vector<Destination> destinations;

template<class Base>
struct Tracked : Base
{
	Tracked() { ++alive; }
	~Tracked() override { --alive; ++destroyed; }
};

// Potion/portal use actions are executable-only. The callback supplies a
// charge-capable item built on the real library core; destination requests
// are observed separately without redefining any production symbol.
class ChargedItem : public Tracked<MItem>
{
public:
	explicit ChargedItem(ITEM_CLASS itemClass) : m_Class(itemClass) {}
	ITEM_CLASS GetItemClass() const override { return m_Class; }
	bool IsChargeItem() const override { return true; }
	TYPE_ITEM_NUMBER GetMaxNumber() const override
	{
		return (*g_pItemTable)[m_Class][GetItemType()].MaxNumber;
	}
private:
	ITEM_CLASS m_Class;
};

MItem* CreateItem(ITEM_CLASS itemClass)
{
	requestedClasses.push_back(itemClass);
	if (refuseItems) return nullptr;
	switch (itemClass)
	{
	case ITEM_CLASS_SWORD: return new Tracked<MSword>;
	case ITEM_CLASS_COAT: return new Tracked<MCoat>;
	case ITEM_CLASS_TROUSER: return new Tracked<MTrouser>;
	case ITEM_CLASS_VAMPIRE_COAT: return new Tracked<MVampireCoat>;
	case ITEM_CLASS_POTION:
	case ITEM_CLASS_VAMPIRE_PORTAL_ITEM: return new ChargedItem(itemClass);
	default: return nullptr;
	}
}

const NPCShopHost host{
	.CreateItem = CreateItem,
	.IsFemale = []() { ++genderReads; return female; },
	.SetPortalDestination = [](MItem& item, int zone,
		TYPE_SECTORPOSITION x, TYPE_SECTORPOSITION y) {
		destinations.push_back({&item, static_cast<int>(item.GetItemType()), zone, x, y,
			item.GetGrade(), static_cast<int>(item.GetNumber())});
	},
};

struct ShopWorld : GameModelWorld
{
	MShopTemplateTable templates;
	NPC_INFO npc;
	MShop shop;
	MShopTemplateTable* previous = g_pShopTemplateTable;

	ShopWorld()
	{
		g_pShopTemplateTable = &templates;
		alive = destroyed = genderReads = 0;
		female = refuseItems = false;
		requestedClasses.clear();
		destinations.clear();
		shop.Init(MShopShelf::MAX_SHELF);
		for (auto itemClass : {ITEM_CLASS_SWORD, ITEM_CLASS_COAT, ITEM_CLASS_TROUSER,
			ITEM_CLASS_VAMPIRE_COAT, ITEM_CLASS_POTION, ITEM_CLASS_VAMPIRE_PORTAL_ITEM})
		{
			g_pItemTable->InitClass(itemClass, 32);
			for (int type = 0; type < 32; ++type)
			{
				auto& row = testfw::MutableRow(*g_pItemTable, itemClass, type);
				row.Value1 = 3000;
				row.MaxNumber = 9;
			}
		}
	}
	~ShopWorld()
	{
		shop.Release();
		CHECK_EQ(0, alive);
		g_pShopTemplateTable = previous;
	}
	MShopTemplate& Add(unsigned int id, BYTE shelf, int itemClass, WORD first, WORD last)
	{
		auto* row = new MShopTemplate;
		row->Type = shelf;
		row->Class = itemClass;
		row->MinType = first;
		row->MaxType = last;
		row->MinOption = 0;
		row->MaxOption = 0;
		if (!templates.AddData(id, row))
		{
			delete row;
			throw std::runtime_error("Duplicate shop fixture id");
		}
		npc.ListShopTemplateID.push_back(id);
		return *row;
	}
	bool Build(bool mysterious = false, const NPCShopHost& services = host)
	{
		return BuildNPCShopShelf(shop, &npc, &templates, mysterious, services);
	}
	MShopShelf& Shelf(bool mysterious = false)
	{
		auto* shelf = shop.GetShelf(mysterious ? MShopShelf::SHELF_UNKNOWN : MShopShelf::SHELF_FIXED);
		if (!shelf) throw std::runtime_error("Missing test shelf");
		return *shelf;
	}
};

struct Bytes
{
	std::vector<unsigned char> data;
	Bytes& U8(unsigned int value) { data.push_back(static_cast<unsigned char>(value)); return *this; }
	Bytes& U16(unsigned int value) { return U8(value).U8(value >> 8); }
	Bytes& U32(std::uint32_t value) { return U16(value).U16(value >> 16); }
	Bytes& Row(BYTE shelf, int itemClass, WORD first, WORD last, BYTE minOption, BYTE maxOption)
	{
		return U8(shelf).U32(static_cast<std::uint32_t>(itemClass)).U16(first).U16(last)
			.U8(minOption).U8(maxOption);
	}
};

struct ShopFile
{
	std::filesystem::path path;
	explicit ShopFile(const Bytes& bytes)
	{
		static unsigned int sequence = 0;
		path = std::filesystem::temp_directory_path() / ("darkeden_npc_shop_"
			+ std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
			+ "_" + std::to_string(sequence++) + ".bin");
		std::ofstream output(path, std::ios::binary);
		if (!bytes.data.empty())
			output.write(reinterpret_cast<const char*>(bytes.data.data()),
				static_cast<std::streamsize>(bytes.data.size()));
		output.close();
		if (!output.good()) throw std::runtime_error("Cannot write shop fixture");
	}
	~ShopFile() { std::error_code error; std::filesystem::remove(path, error); }
};

template<class Table>
bool Load(Table& table, const Bytes& bytes)
{
	ShopFile fixture(bytes);
	std::ifstream input(fixture.path, std::ios::binary);
	table.LoadFromFile(input);
	if (input.good()) CHECK_EQ(bytes.data.size(), input.tellg());
	return input.good();
}

void CheckTypes(const MShopShelf& shelf, std::initializer_list<int> types)
{
	unsigned int slot = 0;
	for (int type : types)
	{
		const auto* item = shelf.GetItem(slot++);
		CHECK(item != nullptr);
		if (item) CHECK_EQ(type, item->GetItemType());
	}
	CHECK(shelf.GetItem(slot) == nullptr);
}

} // namespace

TEST(NPCShop, TemplateRowsKeepTheElevenByteFileLayout)
{
	const auto bytes = Bytes().Row(2, ITEM_CLASS_SWORD, 256, 513, 7, 9);
	MShopTemplate row;
	CHECK(Load(row, bytes));
	CHECK_EQ(2, row.Type);
	CHECK_EQ(ITEM_CLASS_SWORD, row.Class);
	CHECK_EQ(256, row.MinType);
	CHECK_EQ(513, row.MaxType);
	CHECK_EQ(7, row.MinOption);
	CHECK_EQ(9, row.MaxOption);
	ShopFile saved({});
	{
		std::ofstream output(saved.path, std::ios::binary);
		row.SaveToFile(output);
		CHECK(output.good());
	}
	std::ifstream input(saved.path, std::ios::binary);
	const std::vector<unsigned char> actual{std::istreambuf_iterator<char>(input), {}};
	CHECK(actual == bytes.data);
}

TEST(NPCShop, RealTemplateTableLoadsKeysAndReplacesThemOnReload)
{
	ShopWorld world;
	CHECK(Load(world.templates, Bytes().U32(2)
		.U32(41).Row(0, ITEM_CLASS_SWORD, 1, 3, 0, 0)
		.U32(7).Row(2, ITEM_CLASS_COAT, 0, 5, 2, 8)));
	CHECK(g_pShopTemplateTable == &world.templates);
	CHECK_EQ(2, world.templates.size());
	CHECK_EQ(3, world.templates.GetData(41)->MaxType);
	CHECK_EQ(8, world.templates.GetData(7)->MaxOption);
	CHECK(world.templates.GetData(99) == nullptr);
	CHECK(Load(world.templates, Bytes().U32(1).U32(99).Row(0, ITEM_CLASS_SWORD, 5, 6, 0, 0)));
	CHECK_EQ(1, world.templates.size());
	CHECK(world.templates.GetData(41) == nullptr);
	CHECK_EQ(5, world.templates.GetData(99)->MinType);
}

TEST(NPCShop, FixedStockUsesNpcTemplateOrderAndInclusiveTypeRanges)
{
	ShopWorld world;
	world.Add(20, MShopShelf::SHELF_FIXED, ITEM_CLASS_SWORD, 8, 8);
	world.npc.ListShopTemplateID.push_back(404);
	world.Add(10, MShopShelf::SHELF_UNKNOWN, ITEM_CLASS_COAT, 0, 3);
	world.Add(30, MShopShelf::SHELF_FIXED, ITEM_CLASS_SWORD, 1, 3);
	CHECK(world.Build());
	CheckTypes(world.Shelf(), {8, 1, 2, 3});
	CHECK(world.Shelf().IsEnable());
	CHECK_EQ(4, requestedClasses.size());
	CHECK_EQ(0, genderReads);
	for (unsigned int slot = 0; slot < 4; ++slot)
	{
		const auto* item = world.Shelf().GetItem(slot);
		CHECK_EQ(ITEM_CLASS_SWORD, item->GetItemClass());
		CHECK_EQ(4, item->GetGrade());
		CHECK_EQ(3000, item->GetCurrentDurability());
		CHECK(item->IsIdentified());
	}
}

TEST(NPCShop, RebuildingReusesTheShelfAndDeletesItsPreviousStock)
{
	ShopWorld world;
	auto& row = world.Add(1, MShopShelf::SHELF_FIXED, ITEM_CLASS_SWORD, 0, 2);
	CHECK(world.Build());
	auto* shelf = &world.Shelf();
	shelf->SetVersion(17);
	shelf->SetName("Retained");
	row.MinType = row.MaxType = 5;
	CHECK(world.Build());
	CHECK(&world.Shelf() == shelf);
	CHECK_EQ(3, destroyed);
	CHECK_EQ(1, alive);
	CHECK_EQ(0, shelf->GetVersion()); // Release resets the server-version stamp.
	CHECK(std::string(shelf->GetName()) == "Retained");
	CheckTypes(*shelf, {5});
}

TEST(NPCShop, RebuildingOneShelfPreservesTheOtherShelfKinds)
{
	ShopWorld world;
	world.Add(1, MShopShelf::SHELF_FIXED, ITEM_CLASS_SWORD, 0, 0);
	world.Add(2, MShopShelf::SHELF_UNKNOWN, ITEM_CLASS_SWORD, 1, 1);
	CHECK(world.Build());
	CHECK(world.Build(true));
	auto* mystery = world.Shelf(true).GetItem(0);
	auto* special = new MShopSpecialShelf;
	CHECK(special->AddItem(new Tracked<MSword>));
	CHECK(world.shop.SetShelf(MShopShelf::SHELF_SPECIAL, special));
	CHECK(world.Build());
	CHECK(world.Shelf(true).GetItem(0) == mystery);
	CHECK(world.shop.GetShelf(MShopShelf::SHELF_SPECIAL) == special);
	CHECK_EQ(3, alive);
}

TEST(NPCShop, MissingNpcReplacesTheShelfWithEmptyStockAndReturnsFalse)
{
	ShopWorld world;
	world.Add(1, MShopShelf::SHELF_FIXED, ITEM_CLASS_SWORD, 0, 1);
	CHECK(world.Build());
	CHECK(!BuildNPCShopShelf(world.shop, nullptr, nullptr, false, host));
	CHECK_EQ(2, destroyed);
	CheckTypes(world.Shelf(), {});
}

TEST(NPCShop, EmptyOrMissingTemplatesDisableTheShelf)
{
	ShopWorld world;
	CHECK(BuildNPCShopShelf(world.shop, &world.npc, nullptr, false, host));
	CHECK(!world.Shelf().IsEnable());
	world.npc.ListShopTemplateID.push_back(123);
	CHECK(world.Build());
	CHECK(!world.Shelf().IsEnable());
	CheckTypes(world.Shelf(), {});
	CHECK(requestedClasses.empty());
}

TEST(NPCShop, ReversedTypeRangesDoNotCallTheFactory)
{
	ShopWorld world;
	world.Add(1, MShopShelf::SHELF_FIXED, ITEM_CLASS_SWORD, 5, 4);
	CHECK(world.Build());
	CHECK(!world.Shelf().IsEnable());
	CHECK(requestedClasses.empty());
}

TEST(NPCShop, AFactoryThatDeclinesEveryItemLeavesTheShelfDisabled)
{
	ShopWorld world;
	world.Add(1, MShopShelf::SHELF_UNKNOWN, ITEM_CLASS_SWORD, 0, 2);
	refuseItems = true;
	CHECK(world.Build(true));
	CHECK_EQ(3, requestedClasses.size());
	CHECK_EQ(0, genderReads);
	CHECK(!world.Shelf(true).IsEnable());
	CheckTypes(world.Shelf(true), {});
}

TEST(NPCShop, MysteriousStockIsUnidentifiedAndFullyRepaired)
{
	ShopWorld world;
	world.Add(1, MShopShelf::SHELF_UNKNOWN, ITEM_CLASS_SWORD, 2, 3);
	CHECK(world.Build(true));
	CheckTypes(world.Shelf(true), {2, 3});
	for (unsigned int slot = 0; slot < 2; ++slot)
	{
		const auto* item = world.Shelf(true).GetItem(slot);
		CHECK(!item->IsIdentified());
		CHECK_EQ(4, item->GetGrade());
		CHECK_EQ(3000, item->GetCurrentDurability());
	}
}

TEST(NPCShop, FemaleMysteriousClothingUsesThePairedFemaleTypes)
{
	ShopWorld world;
	female = true;
	world.Add(1, MShopShelf::SHELF_UNKNOWN, ITEM_CLASS_COAT, 0, 3);
	world.Add(2, MShopShelf::SHELF_UNKNOWN, ITEM_CLASS_TROUSER, 0, 3);
	world.Add(3, MShopShelf::SHELF_UNKNOWN, ITEM_CLASS_VAMPIRE_COAT, 0, 3);
	CHECK(world.Build(true));
	CheckTypes(world.Shelf(true), {1, 3, 1, 3, 1, 3});
	CHECK_EQ(6, genderReads);
}

TEST(NPCShop, FixedClothingDoesNotReadGenderOrSkipTypes)
{
	ShopWorld world;
	female = true;
	world.Add(1, MShopShelf::SHELF_FIXED, ITEM_CLASS_COAT, 0, 3);
	CHECK(world.Build());
	CheckTypes(world.Shelf(), {0, 1, 2, 3});
	CHECK_EQ(0, genderReads);
}

TEST(NPCShop, MaleMysteriousClothingKeepsTheFullTemplateRange)
{
	ShopWorld world;
	world.Add(1, MShopShelf::SHELF_UNKNOWN, ITEM_CLASS_COAT, 0, 3);
	CHECK(world.Build(true));
	CheckTypes(world.Shelf(true), {0, 1, 2, 3});
}

TEST(NPCShop, ChargeItemsStartWithTheirMaximumNumber)
{
	ShopWorld world;
	world.Add(1, MShopShelf::SHELF_FIXED, ITEM_CLASS_POTION, 1, 2);
	testfw::MutableRow(*g_pItemTable, ITEM_CLASS_POTION, 2).MaxNumber = 17;
	CHECK(world.Build());
	CHECK_EQ(9, world.Shelf().GetItem(0)->GetNumber());
	CHECK_EQ(17, world.Shelf().GetItem(1)->GetNumber());
}

TEST(NPCShop, PortalDestinationsFollowTheThreeTypeGroupsAfterInitialization)
{
	ShopWorld world;
	world.Add(1, MShopShelf::SHELF_FIXED, ITEM_CLASS_VAMPIRE_PORTAL_ITEM, 0, 12);
	CHECK(world.Build());
	CHECK_EQ(9, destinations.size());
	for (const auto& destination : destinations)
	{
		CHECK(destination.item == world.Shelf().GetItem(destination.type));
		CHECK_EQ(4, destination.grade);
		CHECK_EQ(9, destination.number);
		const int group = (destination.type - 3) / 3;
		CHECK_EQ((group == 0 ? 1003 : group == 1 ? 1007 : 61), destination.zone);
		CHECK_EQ((group == 0 ? 50 : group == 1 ? 62 : 102), destination.x);
		CHECK_EQ((group == 0 ? 70 : group == 1 ? 65 : 220), destination.y);
	}
}

TEST(NPCShop, MissingFactoryProducesAnEmptyDisabledShelf)
{
	ShopWorld world;
	world.Add(1, MShopShelf::SHELF_FIXED, ITEM_CLASS_SWORD, 0, 1);
	const NPCShopHost empty;
	CHECK(world.Build(false, empty));
	CHECK(!world.Shelf().IsEnable());
	CHECK(requestedClasses.empty());
}

TEST(NPCShop, MissingGenderAndPortalCallbacksUseTheirDefaultBehavior)
{
	ShopWorld world;
	female = true;
	world.Add(1, MShopShelf::SHELF_UNKNOWN, ITEM_CLASS_COAT, 0, 3);
	world.Add(2, MShopShelf::SHELF_UNKNOWN, ITEM_CLASS_VAMPIRE_PORTAL_ITEM, 3, 3);
	const NPCShopHost factoryOnly{.CreateItem = CreateItem};
	CHECK(world.Build(true, factoryOnly));
	CheckTypes(world.Shelf(true), {0, 1, 2, 3, 3});
	CHECK_EQ(0, genderReads);
	CHECK(destinations.empty());
}
