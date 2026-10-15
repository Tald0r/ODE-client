#include "test_framework.h"
#include "CTypeMap.h"

#include <cstdio>
#include <fstream>
#include <initializer_list>

// CTypeMap's file: a count, then each record's id and the record. The NPC,
// shop template, quest and zone sound tables load through it.
namespace {

const char* kPath = "type_map_test.bin";

struct Record
{
	static int loads;
	int value = 0;
	void LoadFromFile(std::ifstream& file) { ++loads; file.read(reinterpret_cast<char*>(&value), 4); }
	void SaveToFile(std::ofstream& file) { file.write(reinterpret_cast<const char*>(&value), 4); }
};
int Record::loads = 0;

void WriteInts(std::initializer_list<int> ints)
{
	std::ofstream out(kPath, std::ios::binary | std::ios::trunc);
	for (int v : ints) out.write(reinterpret_cast<const char*>(&v), 4);
}

// Loads the file into the map; the records it read.
int Load(CTypeMap<Record>& map)
{
	Record::loads = 0;
	{
		std::ifstream in(kPath, std::ios::binary);
		map.LoadFromFile(in);
	}
	std::remove(kPath);
	return Record::loads;
}

Record* Make(int value)
{
	Record* record = new Record;
	record->value = value;
	return record;
}

} // namespace

TEST(TypeMap, TheRecordsSurviveTheirFile)
{
	CTypeMap<Record> written;
	written.AddData(7, Make(42));
	written.AddData(9, Make(-5));
	{
		std::ofstream out(kPath, std::ios::binary | std::ios::trunc);
		written.SaveToFile(out);
	}
	CTypeMap<Record> read;
	read.AddData(1, Make(1));
	CHECK_EQ(2, Load(read));
	CHECK_EQ(2, int(read.size()));
	CHECK(read.GetData(1) == nullptr);
	CHECK(read.GetData(7) != nullptr && read.GetData(7)->value == 42);
	CHECK(read.GetData(9) != nullptr && read.GetData(9)->value == -5);
}

TEST(TypeMap, ACountPastTheFileStopsAtItsEnd)
{
	// One record where the count says a thousand: the rest are not read
	// from a stream that has ended.
	WriteInts({1000, 7, 42});
	CTypeMap<Record> map;
	CHECK_EQ(1, Load(map));
	CHECK_EQ(1, int(map.size()));
	CHECK(map.GetData(7) != nullptr && map.GetData(7)->value == 42);
}

TEST(TypeMap, AnEmptyFileLoadsNothing)
{
	// No count at all: nothing is read, and what the map held is gone.
	WriteInts({});
	CTypeMap<Record> map;
	map.AddData(1, Make(1));
	CHECK_EQ(0, Load(map));
	CHECK_EQ(0, int(map.size()));
}
