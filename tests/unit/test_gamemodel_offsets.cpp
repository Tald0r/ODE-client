//----------------------------------------------------------------------
// test_gamemodel_offsets.cpp
//----------------------------------------------------------------------
//
// The item table's on-disk layout (gamemodel): an item definition
// stores its default option list as a one-byte count followed by the
// options, so the count and the options written must agree, or every
// field after the list, and every record after this one, is read from
// the wrong offset.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "MItemTable.h"

#include <cstdio>
#include <fstream>
#include <list>

namespace {

const char* const	kTempFile = "gamemodel_offsets_test.bin";

// A definition whose fields after the option list are distinctive,
// holding optionCount default options.
void	FillWithOptions(ITEMTABLE_INFO& info, int optionCount)
{
	info.EName = "Long Sword";
	info.Price = 1500;
	for (int i = 0; i < optionCount; i++)
	{
		info.DefaultOptionList.push_back(static_cast<TYPE_ITEM_OPTION>(i & 0xFF));
	}
	info.ItemStyle = 61;
	info.ElementalType = ITEMTABLE_INFO::ELEMENTAL_TYPE_WATER;
	info.Elemental = 62;
	info.Race = 63;
	info.SetDescriptionFrameID(17);
}

// Saves src and loads the file back into dst.
void	RoundTrip(ITEMTABLE_INFO& src, ITEMTABLE_INFO& dst)
{
	{
		std::ofstream out(kTempFile, std::ios::binary | std::ios::trunc);
		src.SaveToFile(out);
	}
	{
		std::ifstream in(kTempFile, std::ios::binary);
		dst.LoadFromFile(in);
	}
	std::remove(kTempFile);
}

bool	SameFieldsAfterTheOptions(const ITEMTABLE_INFO& a, const ITEMTABLE_INFO& b)
{
	return a.ItemStyle == b.ItemStyle
		&& a.ElementalType == b.ElementalType
		&& a.Elemental == b.Elemental
		&& a.Race == b.Race
		&& a.DescriptionFrameID == b.DescriptionFrameID;
}

} // namespace

// 255 options is the most the one-byte count holds: all of them are
// saved and read back.
TEST(ItemTableInfo, SaveKeepsAllOf255DefaultOptions)
{
	ITEMTABLE_INFO src;
	FillWithOptions(src, 255);
	ITEMTABLE_INFO dst;
	RoundTrip(src, dst);

	CHECK_EQ(255, (long long)dst.DefaultOptionList.size());
	CHECK(dst.DefaultOptionList == src.DefaultOptionList);
	CHECK(SameFieldsAfterTheOptions(src, dst));
}

// Past 255 the list is saved as its first 255, so the count still
// matches what follows it and the later fields keep their offsets.
TEST(ItemTableInfo, SaveCapsDefaultOptionsAt255AndKeepsFollowingFields)
{
	ITEMTABLE_INFO src;
	FillWithOptions(src, 256);
	ITEMTABLE_INFO dst;
	RoundTrip(src, dst);

	CHECK_EQ(255, (long long)dst.DefaultOptionList.size());
	std::list<TYPE_ITEM_OPTION> first255 = src.DefaultOptionList;
	first255.pop_back();
	CHECK(dst.DefaultOptionList == first255);
	CHECK_EQ(61, dst.ItemStyle);
	CHECK(SameFieldsAfterTheOptions(src, dst));
}
