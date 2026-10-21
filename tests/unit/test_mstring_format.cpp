//----------------------------------------------------------------------
// test_mstring_format.cpp
//----------------------------------------------------------------------
//
// MString::FormatChecked (Client/MString.h) - the checked sibling of
// MString::Format, added by task 5.4's fifth slice
// (docs/reviews/code-health-review-2026-08-29.md finding C19).
//
// Both Format entry points now preserve argument types. FormatChecked also
// guarantees readable storage when the formatted result is empty.
//
// MString is in gamemodel, so unlike the twenty-four VS_UI sites the
// same slice converted, this half of the fix has a test path at all.
// That is the reason the checked entry point went here rather than into
// a local buffer at each call site.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "MString.h"

#include <cstring>


namespace {

bool
Is(const char* expected, const MString& actual)
{
	return actual.GetString() != NULL
		&& std::strcmp(actual.GetString(), expected) == 0;
}

} // anonymous namespace


TEST(MStringFormatChecked, PrintsWhatFormatPrintedForAnEntryThatMatchesItsCallSite)
{
	MString msg;

	// The three converted call sites, in the shape they actually have:
	// a zone name, and for the third a zone name and a count.
	msg.FormatChecked("The %s lair has opened.", "Silent");
	CHECK(Is("The Silent lair has opened.", msg));

	msg.FormatChecked("%d minutes left to enter %s.", 5, "the lair");
	CHECK(Is("5 minutes left to enter the lair.", msg));
}

TEST(MStringFormatChecked, PrintsAConversionWithNoArgumentAsText)
{
	MString msg;

	// The primitive of finding C19: an entry carrying one conversion
	// more than the call site passes. The old variadic path would have
	// read a stack word as a char*.
	msg.FormatChecked("The %s lair has opened. %s");

	CHECK(Is("The %s lair has opened. %s", msg));
}

TEST(MStringFormatChecked, LeavesTheStringUsableWhenTheEntryIsMissing)
{
	MString msg;

	// GetGameString answers "" for an id the table does not hold, and
	// the subscript answers a default MString whose GetString() is NULL.
	// Both reach this method at the converted call sites, so neither may
	// leave the result unreadable.
	msg.FormatChecked("");
	CHECK(Is("", msg));

	msg.FormatChecked(NULL);
	CHECK(Is("", msg));
}

TEST(MStringFormatChecked, RefusesAConversionThatWouldWriteThroughItsArgument)
{
	MString	msg;
	int		counter = 4242;

	msg.FormatChecked("opened%n", &counter);

	CHECK(Is("opened%n", msg));
	CHECK_EQ(4242, counter);
}

TEST(MStringFormatChecked, LeavesTheLengthAgreeingWithTheText)
{
	MString msg;

	// FormatChecked assigns through operator=, so the recorded length
	// has to come from the formatted text rather than from the format.
	msg.FormatChecked("%s", "abcdef");

	CHECK_EQ(6, (int)msg.GetLength());

	msg.FormatChecked("%s");

	CHECK_EQ(2, (int)msg.GetLength());
}

TEST(MStringFormat, TypedFormattingRetainsTheStorageAndTruncationContracts)
{
	MString text("old");
	text.Format("%s %d %s", 42);
	CHECK(Is("%s 42 %s", text));
	text.Format("[%s]", text.GetString());
	CHECK(Is("[%s 42 %s]", text));
	const std::string longText(2048, 'x');
	text.Format("%s", longText.c_str());
	CHECK_EQ(1023, text.GetLength());
	CHECK(std::string(text.GetString()) == longText.substr(0, 1023));
	text.Format("");
	CHECK(text.GetString() == NULL);
	CHECK_EQ(0, text.GetLength());
}
