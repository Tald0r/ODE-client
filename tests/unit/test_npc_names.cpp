#include "test_framework.h"
#include "Platform.h"
#include "MNPCTable.h"
#include "MNPCTableEnglish.h"

#include <string>

namespace {
struct NPCNamesTable
{
	MNPCTable table;
	MNPCTable* previous = g_pNPCTable;
	NPCNamesTable() { g_pNPCTable = &table; }
	~NPCNamesTable() { g_pNPCTable = previous; }
};
}

TEST(NPCNames, MissingTableAndRowsAreSafeAndDoNotPopulateTheTable)
{
	NPCNamesTable fixture;
	g_pNPCTable = nullptr;
	ApplyEnglishNPCTable();
	g_pNPCTable = &fixture.table;
	ApplyEnglishNPCTable();
	CHECK(fixture.table.empty());
	CHECK(fixture.table.AddData(11, nullptr));
	ApplyEnglishNPCTable();
	CHECK_EQ(1, fixture.table.size());
	CHECK(fixture.table.GetData(11) == nullptr);
}

TEST(NPCNames, OverlayChangesOnlyNamesAndDescriptionsOfKnownRows)
{
	NPCNamesTable fixture;
	const unsigned ids[]{11, 882, 999999};
	for (unsigned id : ids)
	{
		auto* row = new NPC_INFO;
		row->Name = "Original name";
		row->Description = "Original description";
		row->SpriteID = 42;
		row->ListShopTemplateID.push_back(17);
		row->ListShopTemplateID.push_back(23);
		CHECK(fixture.table.AddData(id, row));
	}
	for (int application = 0; application < 2; ++application)
	{
		ApplyEnglishNPCTable();
		CHECK_EQ(3, fixture.table.size());
		CHECK(std::string(fixture.table.GetData(11)->Name.GetString()) == "Drake");
		CHECK(std::string(fixture.table.GetData(11)->Description.GetString()) == "Sells ARs and SGs");
		CHECK(std::string(fixture.table.GetData(882)->Name.GetString()) == "Bernado");
		CHECK(std::string(fixture.table.GetData(882)->Description.GetString()) == "Dungeon entrance statue");
		CHECK(std::string(fixture.table.GetData(999999)->Name.GetString()) == "Original name");
		CHECK(std::string(fixture.table.GetData(999999)->Description.GetString()) == "Original description");
		for (unsigned id : ids)
		{
			const auto* row = fixture.table.GetData(id);
			CHECK_EQ(42, row->SpriteID);
			CHECK_EQ(2, row->ListShopTemplateID.size());
			CHECK_EQ(17, row->ListShopTemplateID.front());
			CHECK_EQ(23, row->ListShopTemplateID.back());
		}
	}
}
