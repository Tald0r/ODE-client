#include "MString.h"
#include "test_framework.h"
#include <climits>
#include <cstring>
#include <stdexcept>

namespace {

bool HasText(const MString& value, const char* expected)
{
	return value.GetString() != nullptr && std::strcmp(value.GetString(), expected) == 0 &&
		value.GetLength() == std::strlen(expected);
}

} // namespace

TEST(MStringAssignment, CopiesItsOwnTextBeforeReleasingStorage)
{
	MString value("prefix text");
	value = value.GetString();
	CHECK(HasText(value, "prefix text"));
	value = value.GetString() + 7;
	CHECK(HasText(value, "text"));
	value = value.GetString() + value.GetLength();
	CHECK_EQ(0, value.GetLength());
	CHECK(value.GetString() == nullptr);
}

TEST(MStringAssignment, SelfAssignmentPreservesTheObject)
{
	MString value("unchanged");
	MString& self = value;
	value = self;
	CHECK(HasText(value, "unchanged"));
	value.Init(0);
	value = self;
	CHECK(HasText(value, ""));
}

TEST(MStringAssignment, CopiesOwnTheirStorage)
{
	MString source("original");
	MString copy(source);
	MString assigned("old");
	assigned = source;
	source.GetString()[0] = 'O';
	CHECK(HasText(copy, "original"));
	CHECK(HasText(assigned, "original"));
	source.Release();
	CHECK(HasText(copy, "original"));
	CHECK(HasText(assigned, "original"));
}

TEST(MStringAssignment, EmptyAssignmentsKeepTheExistingNullStorageContract)
{
	MString value("text");
	value = "";
	CHECK_EQ(0, value.GetLength());
	CHECK(value.GetString() == nullptr);
	value = "text";
	value = static_cast<const char*>(nullptr);
	CHECK_EQ(0, value.GetLength());
	CHECK(value.GetString() == nullptr);
	MString empty;
	empty.Init(0);
	value = "text";
	value = empty;
	CHECK_EQ(0, value.GetLength());
	CHECK(value.GetString() == nullptr);
}

TEST(MStringAssignment, InitResetsToReadableEmptyStorage)
{
	MString value("text");
	value.Init(4);
	CHECK(HasText(value, ""));
	std::memcpy(value.GetString(), "four", 5);
	CHECK(std::strcmp(value.GetString(), "four") == 0);
	value.Init(0);
	CHECK(HasText(value, ""));
}

TEST(MStringAssignment, NegativeCapacityIsRejectedWithoutChangingTheString)
{
	MString value("preserved");
	for (int capacity : {-1, INT_MIN})
	{
		bool rejected = false;
		try { value.Init(capacity); }
		catch (const std::invalid_argument&) { rejected = true; }
		CHECK(rejected);
		CHECK(HasText(value, "preserved"));
	}
}
