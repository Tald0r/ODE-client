//----------------------------------------------------------------------
// test_item_table.cpp
//----------------------------------------------------------------------
//
// The item table (gamemodel, docs/reviews/RESTRUCTURING.md task 4.4, first
// slice): ITEMTABLE_INFO, one item definition, and ITEMTYPE_TABLE, the
// per-class table of them with the average price MPriceManager uses.
// The client loads the table from Item.inf and SaveToFile writes the
// same layout, so a round trip through both pins that the two agree
// field for field (a matching swap in both would still pass), and the
// hand-built head pins the string order against bytes. No shipped
// Item.inf is in the repository to pin the rest against.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "MItemTable.h"
#include "ItemClassDef.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

namespace {

const char* const	kTempFile = "item_table_test.bin";

void	RemoveScratch()
{
	std::remove(kTempFile);
}

bool	StrEq(const char* actual, const char* expected)
{
	return actual != NULL && std::strcmp(actual, expected) == 0;
}

// A definition with every field set to a distinct value.
void	Fill(ITEMTABLE_INFO& info)
{
	info.EName = "Long Sword";
	info.HName = "Sword";
	info.Description = "A plain blade.";
	info.SetFrameID(11, 12, 13);
	info.SetDropFrameID(14);
	info.SetAddonFrameID(15, 16);
	info.SetDescriptionFrameID(17);
	info.SetSoundID(21, 22, 23, 24);
	info.bMaleOnly = true;
	info.bFemaleOnly = false;
	info.SetGrid(2, 3);
	info.Price = 1500;
	info.Weight = 70;
	info.SetValue(1, 2, 3, 4, 5, 6, 7);
	info.SetRequireSTR(31);
	info.SetRequireDEX(32);
	info.SetRequireINT(33);
	info.SetRequireSUM(96);
	info.SetRequireLevel(34);
	info.SetRequireAdvancementLevel(35);
	info.UseActionInfo = 41;
	info.SilverMax = 42;
	info.ToHit = 43;
	info.MaxNumber = 44;
	info.CriticalHit = 45;
	info.DefaultOptionList.push_back(51);
	info.DefaultOptionList.push_back(52);
	info.ItemStyle = 61;
	info.ElementalType = ITEMTABLE_INFO::ELEMENTAL_TYPE_WATER;
	info.Elemental = 62;
	info.Race = 63;
}

} // namespace

// Every field the loader sets must also start defined: an InitClass'd
// slot that no file entry fills is read like any other (the average
// price, the shop's price display). Price, Race and DropFrameID were
// the three the constructor missed until task 4.4's review.
TEST(ItemTableInfo, ConstructsAsAnEmptyOneByOneItem)
{
	ITEMTABLE_INFO info;
	CHECK_EQ(FRAMEID_NULL, info.TileFrameID);
	CHECK_EQ(FRAMEID_NULL, info.InventoryFrameID);
	CHECK_EQ(FRAMEID_NULL, info.GearFrameID);
	CHECK_EQ(FRAMEID_NULL, info.DropFrameID);
	CHECK_EQ(FRAMEID_NULL, info.AddonMaleFrameID);
	CHECK_EQ(FRAMEID_NULL, info.AddonFemaleFrameID);
	CHECK_EQ(SOUNDID_NULL, info.UseSoundID);
	CHECK_EQ(SOUNDID_NULL, info.TileSoundID);
	CHECK_EQ(1, info.GridWidth);
	CHECK_EQ(1, info.GridHeight);
	CHECK_EQ(0, info.Weight);
	CHECK_EQ(0, info.Price);
	CHECK_EQ(0, info.Race);
	CHECK_EQ(0, info.SilverMax);
	CHECK_EQ(1, info.MaxNumber);
	CHECK_EQ(ITEMTABLE_INFO::ELEMENTAL_TYPE_ANY, info.ElementalType);
	CHECK(info.IsGenderForAll());
	CHECK(info.DefaultOptionList.empty());
}

TEST(ItemTypeTable, FreshTableHasNoEntriesAndZeroAveragePrice)
{
	ITEMTYPE_TABLE table;
	CHECK_EQ(0, table.GetSize());
	CHECK_EQ(0, table.GetAveragePrice());
}

TEST(ItemTableInfo, SaveAndLoadRoundTripEveryField)
{
	ITEMTABLE_INFO src;
	Fill(src);
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		src.SaveToFile(out);
	}

	ITEMTABLE_INFO dst;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		dst.LoadFromFile(in);
	}
	RemoveScratch();

	CHECK(StrEq(dst.EName.GetString(), "Long Sword"));
	CHECK(StrEq(dst.HName.GetString(), "Sword"));
	CHECK(StrEq(dst.Description.GetString(), "A plain blade."));
	CHECK_EQ(11, dst.TileFrameID);
	CHECK_EQ(12, dst.InventoryFrameID);
	CHECK_EQ(13, dst.GearFrameID);
	CHECK_EQ(14, dst.DropFrameID);
	CHECK_EQ(15, dst.AddonMaleFrameID);
	CHECK_EQ(16, dst.AddonFemaleFrameID);
	CHECK_EQ(17, dst.DescriptionFrameID);
	CHECK_EQ(21, dst.TileSoundID);
	CHECK_EQ(22, dst.InventorySoundID);
	CHECK_EQ(23, dst.GearSoundID);
	CHECK_EQ(24, dst.UseSoundID);
	CHECK(dst.IsGenderForMale());
	CHECK(!dst.IsGenderForFemale());
	CHECK_EQ(2, dst.GridWidth);
	CHECK_EQ(3, dst.GridHeight);
	CHECK_EQ(1500, dst.Price);
	CHECK_EQ(70, dst.Weight);
	CHECK_EQ(1, dst.Value1);
	CHECK_EQ(7, dst.Value7);
	CHECK_EQ(31, dst.GetRequireSTR());
	CHECK_EQ(32, dst.GetRequireDEX());
	CHECK_EQ(33, dst.GetRequireINT());
	CHECK_EQ(96, dst.GetRequireSUM());
	CHECK_EQ(34, dst.GetRequireLevel());
	CHECK_EQ(35, dst.GetRequireAdvancementLevel());
	CHECK_EQ(41, dst.UseActionInfo);
	CHECK_EQ(42, dst.SilverMax);
	CHECK_EQ(43, dst.ToHit);
	CHECK_EQ(44, dst.MaxNumber);
	CHECK_EQ(45, dst.CriticalHit);
	CHECK_EQ(2, dst.DefaultOptionList.size());
	CHECK_EQ(51, dst.DefaultOptionList.front());
	CHECK_EQ(52, dst.DefaultOptionList.back());
	CHECK_EQ(61, dst.ItemStyle);
	CHECK_EQ(ITEMTABLE_INFO::ELEMENTAL_TYPE_WATER, dst.ElementalType);
	CHECK_EQ(62, dst.Elemental);
	CHECK_EQ(63, dst.Race);
}

// The file starts with the three strings, English name first: a
// hand-built head, so a swapped order in LoadFromFile cannot hide
// behind a matching swap in SaveToFile.
TEST(ItemTableInfo, FileStartsWithEnglishNameThenKoreanNameThenDescription)
{
	ITEMTABLE_INFO src;
	Fill(src);
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		src.SaveToFile(out);
	}
	std::vector<unsigned char> bytes;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
	}
	RemoveScratch();

	// [len:4]"Long Sword"[len:4]"Sword"[len:4]"A plain blade."
	CHECK(bytes.size() > 4 + 10 + 4 + 5 + 4 + 14);
	CHECK_EQ(10, bytes[0]);
	CHECK(std::memcmp(&bytes[4], "Long Sword", 10) == 0);
	CHECK_EQ(5, bytes[14]);
	CHECK(std::memcmp(&bytes[18], "Sword", 5) == 0);
	CHECK_EQ(14, bytes[23]);
	CHECK(std::memcmp(&bytes[27], "A plain blade.", 14) == 0);
}

//----------------------------------------------------------------------
// ITEMTYPE_TABLE: the CTypeTable body, then an average price over the
// items that carry no default option, in hundreds (the price manager
// divides by it), and 0 when every item has options.
//----------------------------------------------------------------------
namespace {

void	WriteTypeTable(const std::vector<ITEMTABLE_INFO>& items)
{
	std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
	const int count = (int)items.size();
	out.write((const char*)&count, 4);
	for (size_t i = 0; i < items.size(); i++)
		const_cast<ITEMTABLE_INFO&>(items[i]).SaveToFile(out);
}

} // namespace

TEST(ItemTypeTable, AveragePriceSkipsItemsWithDefaultOptionsAndRoundsToHundreds)
{
	std::vector<ITEMTABLE_INFO> items(3);
	items[0].Price = 1200;
	items[1].Price = 3400;
	items[2].Price = 999999;			// carries an option: not averaged
	items[2].DefaultOptionList.push_back(1);
	WriteTypeTable(items);

	ITEMTYPE_TABLE table;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		table.LoadFromFile(in);
	}
	RemoveScratch();

	CHECK_EQ(3, table.GetSize());
	CHECK_EQ(1200, table[0].Price);
	CHECK_EQ(1, table[2].DefaultOptionList.size());
	// (1200 + 3400) / 2 = 2300 -> /1000 = 2 -> *100 = 200
	CHECK_EQ(200, table.GetAveragePrice());
	CHECK_EQ(0, table[5].Price);			// past the end: a default-constructed entry
}

TEST(ItemTypeTable, AveragePriceIsZeroWhenEveryItemHasOptions)
{
	std::vector<ITEMTABLE_INFO> items(2);
	items[0].Price = 5000;
	items[0].DefaultOptionList.push_back(1);
	items[1].Price = 7000;
	items[1].DefaultOptionList.push_back(2);
	WriteTypeTable(items);

	ITEMTYPE_TABLE table;
	{
		std::ifstream in(kTempFile, std::ios::binary);
		table.LoadFromFile(in);
	}
	RemoveScratch();

	CHECK_EQ(2, table.GetSize());
	CHECK_EQ(0, table.GetAveragePrice());
}

//----------------------------------------------------------------------
// ITEMCLASS_TABLE: a table of type tables, one per item class. In the
// client build its constructor holds nothing (the in-code item
// definitions are server-only), so the shape comes from InitClass.
//----------------------------------------------------------------------
TEST(ItemClassTable, InitClassSizesOneClassAndLeavesTheRestEmpty)
{
	ITEMCLASS_TABLE table;
	table.Init(MAX_ITEM_CLASS);
	CHECK_EQ(MAX_ITEM_CLASS, table.GetSize());
	table.InitClass(ITEM_CLASS_MOTORCYCLE, 4);
	CHECK_EQ(4, table[ITEM_CLASS_MOTORCYCLE].GetSize());
	CHECK_EQ(0, table[ITEM_CLASS_MOTORCYCLE + 1].GetSize());
	CHECK_EQ(1, table[ITEM_CLASS_MOTORCYCLE][3].GridWidth);
}
