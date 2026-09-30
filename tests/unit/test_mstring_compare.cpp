//----------------------------------------------------------------------
// test_mstring_compare.cpp
//----------------------------------------------------------------------
//
// MString's comparisons, and MStringPointerCompare over them: the order
// of MStringMap, which holds the chat filter's curse lists and the
// ignored IDs.
//
// An MString keeps no storage for an empty string. The default
// constructor, MString(""), MString(NULL) and an empty assignment leave
// GetString() NULL; Init(0) and a zero-length record read from a file
// leave an allocated "". Every comparison handed m_pString to strcmp
// as it was, so a NULL string on either side was read through. The
// Windows CI jobs crashed on that in MStringMap::Add, with an empty word
// from MChatManager::LoadFromFileCurse compared against the words
// already in the map, and the same Add crashes on every platform. A
// NULL string compares as "" here: equal to an allocated "", before
// every other string, while two non-NULL strings keep strcmp's order.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "MString.h"
#include "MStringMap.h"

#include <cstddef>

namespace {

// Each empty form an MString takes: three with no storage and one with
// an allocated "".
struct Empties
{
	MString	byDefault;
	MString	fromEmpty;
	MString	fromNull;
	MString	allocated;

	Empties()
		: fromEmpty(""), fromNull((const char*)NULL)
	{
		allocated.Init(0);
	}
};

} // namespace

TEST(MStringCompare, TheEmptyFormsAreNullOrAnAllocatedEmptyString)
{
	Empties e;

	CHECK(e.byDefault.GetString() == NULL);
	CHECK(e.fromEmpty.GetString() == NULL);
	CHECK(e.fromNull.GetString() == NULL);
	CHECK(e.allocated.GetString() != NULL);
	CHECK_EQ((size_t)0, e.allocated.GetLength());
}

TEST(MStringCompare, ANullStringComesBeforeAWordInBothOrders)
{
	MString empty;
	MString word("darn");

	CHECK(empty < word);
	CHECK(!(word < empty));
	CHECK(word > empty);
	CHECK(!(empty > word));
	CHECK(!(empty == word));
	CHECK(!(word == empty));
	CHECK(empty != word);
	CHECK(word != empty);
}

TEST(MStringCompare, EveryEmptyFormEqualsEveryOtherInBothOrders)
{
	Empties e;
	MString* const all[] = { &e.byDefault, &e.fromEmpty, &e.fromNull, &e.allocated };
	const int count = (int)(sizeof(all) / sizeof(all[0]));

	for (int i = 0; i < count; i++)
	{
		for (int j = 0; j < count; j++)
		{
			CHECK(*all[i] == *all[j]);
			CHECK(!(*all[i] != *all[j]));
			CHECK(!(*all[i] < *all[j]));
			CHECK(!(*all[i] > *all[j]));
		}
	}
}

TEST(MStringCompare, TheTextOverloadsReadANullOnEitherSideAsEmpty)
{
	Empties e;
	MString word("darn");
	const char* const none = NULL;

	// A NULL string against text.
	CHECK(e.byDefault == "");
	CHECK(!(e.byDefault != ""));
	CHECK(e.byDefault < "darn");
	CHECK(!(e.byDefault > "darn"));
	CHECK(e.byDefault != "darn");
	CHECK(!(e.byDefault == "darn"));

	// NULL text against a string.
	CHECK(e.byDefault == none);
	CHECK(e.allocated == none);
	CHECK(!(e.byDefault < none));
	CHECK(!(e.byDefault > none));
	CHECK(word > none);
	CHECK(!(word < none));
	CHECK(word != none);
	CHECK(!(word == none));

	// A word against text is strcmp's answer, as before.
	CHECK(word == "darn");
	CHECK(word < "heck");
	CHECK(word > "can");
	CHECK(word != "Darn");
}

TEST(MStringCompare, TwoWordsKeepStrcmpsOrder)
{
	MString a("a");
	MString ab("ab");
	MString b("b");
	MString upper("A");
	MString z("z");
	MString ga("\xB0\xA1");		// a CP949 syllable: both bytes have the high bit set

	CHECK(a < ab);				// a prefix comes first
	CHECK(ab < b);
	CHECK(upper < a);			// byte order, not case-folded
	CHECK(z < ga);				// bytes compare unsigned: 0xB0 after 'z'
	CHECK(!(ga < z));
	CHECK(ga > z);
	CHECK(a == MString("a"));
	CHECK(!(a < MString("a")));
}

TEST(MStringCompare, ThePointerCompareIsAStrictWeakOrderWithEmptyStrings)
{
	// MSVC's Debug map checks the comparator's answer in the other order
	// whenever it answers true ("invalid comparator"); these are the
	// properties every std::map comparator needs.
	Empties e;
	MString darn("darn");
	MString heck("heck");
	MString ga("\xB0\xA1");
	MString* const all[] = { &e.byDefault, &e.fromEmpty, &e.fromNull, &e.allocated,
		&darn, &heck, &ga };
	const int count = (int)(sizeof(all) / sizeof(all[0]));
	MStringPointerCompare before;

	// The cases the Windows jobs crashed on, both orders.
	CHECK(before(&e.byDefault, &darn));
	CHECK(!before(&darn, &e.byDefault));
	CHECK(!before(&e.byDefault, &e.allocated));
	CHECK(!before(&e.allocated, &e.byDefault));
	CHECK(before(&darn, &heck));
	CHECK(!before(&heck, &darn));

	for (int i = 0; i < count; i++)
	{
		CHECK(!before(all[i], all[i]));				// irreflexive

		for (int j = 0; j < count; j++)
		{
			CHECK(!(before(all[i], all[j]) && before(all[j], all[i])));	// asymmetric

			for (int k = 0; k < count; k++)
			{
				// transitive
				if (before(all[i], all[j]) && before(all[j], all[k]))
				{
					CHECK(before(all[i], all[k]));
				}

				// equivalence (neither before the other) is transitive
				const bool eqIJ = !before(all[i], all[j]) && !before(all[j], all[i]);
				const bool eqJK = !before(all[j], all[k]) && !before(all[k], all[j]);
				if (eqIJ && eqJK)
				{
					CHECK(!before(all[i], all[k]) && !before(all[k], all[i]));
				}
			}
		}
	}
}
