#include "test_framework.h"
#include "MCreatureTable.h"
#include <cstring>
#include <new>

// A creature table row nothing has loaded starts as a slayer that does not
// move and stands no higher than the ground: a lookup of its tribe (IsNPC,
// IsVampire) or its movement reads a known value, not what the memory held.
// The row is built over bytes that are not zero, so a member the
// constructor leaves out reads them back.
TEST(CreatureTableRows, AnUnloadedRowStartsKnown)
{
	alignas(CREATURETABLE_INFO) unsigned char bytes[sizeof(CREATURETABLE_INFO)];
	std::memset(bytes, 0xAA, sizeof(bytes));
	CREATURETABLE_INFO* row = new (bytes) CREATURETABLE_INFO;
	CHECK_EQ(static_cast<int>(CREATURETRIBE_SLAYER), static_cast<int>(row->GetCreatureTribe()));
	CHECK(row->IsSlayer() && !row->IsVampire() && !row->IsNPC() && !row->IsOusters());
	CHECK_EQ(0, static_cast<int>(row->MoveTimes));
	CHECK_EQ(0, static_cast<int>(row->MoveRatio));
	CHECK_EQ(0, row->Height);
	CHECK_EQ(0, row->DeadHeight);
	CHECK_EQ(0, static_cast<int>(row->DeadActionInfo));
	row->~CREATURETABLE_INFO();
}
