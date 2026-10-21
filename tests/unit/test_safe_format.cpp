//----------------------------------------------------------------------
// test_safe_format.cpp
//----------------------------------------------------------------------
//
// SafeFormat (basic/SafeFormat.h) - the checked formatter that stands
// between Data/Info/String.inf and sprintf (docs/reviews/RESTRUCTURING.md task
// 5.4, docs/reviews/code-health-review-2026-08-29.md finding C19).
//
// Two things are under test and they pull in opposite directions. The
// first is that the ordinary vocabulary of the string table - "%s",
// "%d", "%02d", "%s (%d,%d)" - still prints exactly what sprintf printed,
// because a formatter that changes the user interface is not a fix. The
// second is that a format the call site does not match can no longer make
// the program read or write anything: the specification comes out as
// text.
//
// The hostile cases are the reason the file exists, so they are written
// as the observable contract - the literal text that appears and the
// value that is left alone - rather than as "does not crash", which an
// out of bounds read in C++ would pass anyway.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "SafeFormat.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <limits>


namespace {

//----------------------------------------------------------------------
// Compared through a helper rather than by CHECK_EQ, which is integer
// only, and NULL tolerant so a regression fails the check instead of
// crashing the whole run.
//----------------------------------------------------------------------
bool
Is(const char* expected, const char* actual)
{
	return actual != NULL && std::strcmp(actual, expected) == 0;
}

} // anonymous namespace

namespace {
template <typename T>
void MatchesCrt(const std::string& format, T value)
{
	char expected[512];
	const int length = std::snprintf(expected, sizeof(expected), format.c_str(), value);
	CHECK(length >= 0 && static_cast<size_t>(length) < sizeof(expected));
	if (length < 0 || static_cast<size_t>(length) >= sizeof(expected)) return;
	for (const size_t capacity : {size_t(1), size_t(2), size_t(7), size_t(32), sizeof(expected)}) {
		char actual[528];
		std::memset(actual, 0xCD, sizeof(actual));
		const size_t stored = static_cast<size_t>(length) < capacity
			? static_cast<size_t>(length) : capacity - 1;
		const int written = SafeFormat::Format(actual, capacity, format.c_str(), value);
		if (std::memcmp(expected, actual, stored) != 0) {
			static int reported = 0;
			if (reported++ < 10) std::fprintf(stderr, "CRT mismatch: %s (capacity %zu): expected [%s], actual [%s]\n", format.c_str(), capacity, expected, actual);
		}
		CHECK_EQ(static_cast<int>(stored), written);
		CHECK_EQ(0, std::memcmp(expected, actual, stored));
		CHECK_EQ(0, actual[stored]);
		bool intact = true;
		for (size_t i = capacity; i < sizeof(actual); ++i)
			if (static_cast<unsigned char>(actual[i]) != 0xCD) intact = false;
		CHECK(intact);
	}
}
}

TEST(SafeFormat, IntegerFlagsPrecisionAndTruncationMatchTheCrt)
{
	for (const char* flags : {"", "-", "+", " ", "0", "#", "-+", "+#", " #", "0#", "-0"})
	for (const char* width : {"", "1", "8", "32"})
	for (const char* precision : {"", ".0", ".1", ".6", ".32"}) {
		const std::string prefix = std::string("%") + flags + width + precision;
		for (const char* conversion : {"d", "i"}) {
			for (const int value : {0, 42, -42, (std::numeric_limits<int>::min)(), (std::numeric_limits<int>::max)()})
				MatchesCrt(prefix + conversion, value);
			for (const long long value : {0LL, 5000000000LL, (std::numeric_limits<long long>::min)(), (std::numeric_limits<long long>::max)()})
				MatchesCrt(prefix + "ll" + conversion, value);
		}
		for (const char* conversion : {"u", "o", "x", "X"}) {
			for (const unsigned int value : {0U, 42U, (std::numeric_limits<unsigned int>::max)()})
				MatchesCrt(prefix + conversion, value);
			for (const unsigned long long value : {0ULL, 5000000000ULL, (std::numeric_limits<unsigned long long>::max)()})
				MatchesCrt(prefix + "ll" + conversion, value);
		}
	}
}

TEST(SafeFormat, FloatingFlagsPrecisionAndTruncationMatchTheCrt)
{
	for (const char* flags : {"", "-", "+", " ", "0", "#", "-+", "+#", " #", "0#", "-0"})
	for (const char* width : {"", "1", "8", "32"})
	for (const char* precision : {"", ".0", ".1", ".6", ".32"})
	for (const char* conversion : {"e", "E", "f", "F", "g", "G", "a", "A"}) {
		const std::string format = std::string("%") + flags + width + precision + conversion;
		for (const double value : {0.0, -0.0, 1.5, -12.5, (std::numeric_limits<double>::max)(),
			(std::numeric_limits<double>::min)(), std::numeric_limits<double>::denorm_min(),
			std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
			std::numeric_limits<double>::quiet_NaN()})
			MatchesCrt(format, value);
	}
}

TEST(SafeFormat, TextCharacterAndPointerFieldsMatchTheCrt)
{
	int object = 7;
	for (unsigned mask = 0; mask < 32; ++mask) {
		std::string flags;
		for (unsigned bit = 0; bit < 5; ++bit)
			if (mask & (1U << bit)) flags += "-+ 0#"[bit];
		for (const char* width : {"", "1", "8", "32"})
		for (const char* precision : {"", ".0", ".1", ".6", ".32"}) {
			const std::string prefix = "%" + flags + width + precision;
			for (const int value : {0, 65, 255}) MatchesCrt(prefix + "c", value);
			for (const char* value : {"", "abc", "a string longer than the smallest buffers"})
				MatchesCrt(prefix + "s", value);
			MatchesCrt(prefix + "p", static_cast<void*>(&object));
			MatchesCrt(prefix + "p", static_cast<void*>(nullptr));
		}
	}
}

TEST(SafeFormat, StringPrecisionBoundsReadsFromAnUnterminatedArray)
{
	const char bytes[] = {'a', 'b', 'c'};
	char result[16];
	SafeFormat::Format(result, "[%.3s]", bytes);
	CHECK(Is("[abc]", result));
}


//----------------------------------------------------------------------
// What the string table really contains
//----------------------------------------------------------------------

TEST(SafeFormat, PrintsTheOrdinaryTableVocabularyExactlyAsSprintfDid)
{
	char buf[64];

	SafeFormat::Format(buf, "You can resurrect in %d seconds.", 12);
	CHECK(Is("You can resurrect in 12 seconds.", buf));

	SafeFormat::Format(buf, "%s (%d,%d)", "Adam", 132, 47);
	CHECK(Is("Adam (132,47)", buf));

	SafeFormat::Format(buf, "%02d:%02d:%02d", 7, 5, 9);
	CHECK(Is("07:05:09", buf));

	SafeFormat::Format(buf, "%s(%d)", "Gold", 250);
	CHECK(Is("Gold(250)", buf));

	SafeFormat::Format(buf, "%d/%d/%d", 2026, 9, 3);
	CHECK(Is("2026/9/3", buf));
}

TEST(SafeFormat, CopiesAFormatWithNoConversionsThrough)
{
	char buf[64];

	const int written = SafeFormat::Format(buf, "The connection is unstable.");

	CHECK(Is("The connection is unstable.", buf));
	CHECK_EQ(27, written);
}

TEST(SafeFormat, TreatsADoubledPercentAsOneLiteralPercent)
{
	char buf[32];

	SafeFormat::Format(buf, "100%% done");

	CHECK(Is("100% done", buf));
}

TEST(SafeFormat, AcceptsAMutableCharPointerArgument)
{
	char buf[64];
	char name[] = "Lorraine";
	char* pName = name;

	// This is the shape the converted call sites actually pass:
	// MString::GetString() returns char*, not const char*, and the
	// non-template overload has to win over the pointer template for it.
	SafeFormat::Format(buf, "[System]%s", pName);

	CHECK(Is("[System]Lorraine", buf));
}

TEST(SafeFormat, AcceptsAStdStringArgument)
{
	char buf[64];
	const std::string name = "Lorraine";

	SafeFormat::Format(buf, "[System]%s", name);

	CHECK(Is("[System]Lorraine", buf));
}

TEST(SafeFormat, ReturnsTheNumberOfBytesItWrote)
{
	char buf[64];

	CHECK_EQ(5, SafeFormat::Format(buf, "%d", 12345));
	CHECK_EQ(0, SafeFormat::Format(buf, ""));
	CHECK_EQ(8, SafeFormat::Format(buf, "%s", "12345678"));
}


//----------------------------------------------------------------------
// The primitive finding C19 describes: a conversion with no argument
//----------------------------------------------------------------------

TEST(SafeFormat, PrintsAConversionWithNoArgumentAsText)
{
	char buf[64];

	// This is the whole exploit in one line. sprintf here reads a stack
	// word as a char* and copies it, unbounded, into buf.
	SafeFormat::Format(buf, "%s");

	CHECK(Is("%s", buf));
}

TEST(SafeFormat, StopsSubstitutingWhenTheArgumentsRunOut)
{
	char buf[64];

	SafeFormat::Format(buf, "%s%s%s%s", "one");

	CHECK(Is("one%s%s%s", buf));
}

TEST(SafeFormat, PrintsAConversionTheArgumentTypeCannotSatisfyAsText)
{
	char buf[64];

	// An int is not a char*, so the %s is not performed - and it does not
	// consume the argument either, so the %d that follows still gets it.
	SafeFormat::Format(buf, "%s %d", 5);

	CHECK(Is("%s 5", buf));
}

TEST(SafeFormat, RefusesAConversionThatWouldWriteThroughItsArgument)
{
	char buf[64];
	int counter = 4242;

	SafeFormat::Format(buf, "abc%n", &counter);

	CHECK(Is("abc%n", buf));
	CHECK_EQ(4242, counter);
}

TEST(SafeFormat, RefusesAWidthTakenFromAnArgument)
{
	char buf[64];

	// %*d consumes two arguments where the call site counted on one, so
	// every later conversion in a real entry would be reading the wrong
	// value. It is refused, and consumes nothing.
	SafeFormat::Format(buf, "[%*d]", 10, 5);

	CHECK(Is("[%*d]", buf));
}

TEST(SafeFormat, RefusesAPrecisionTakenFromAnArgument)
{
	char buf[64];

	// The header promises this of the precision as well as the width, and
	// for the same reason: %.*s consumes two arguments where the call site
	// counted on one.
	SafeFormat::Format(buf, "[%.*s]", 3, "abcdef");

	CHECK(Is("[%.*s]", buf));
}

TEST(SafeFormat, RefusesAWideConversionThatWouldRetypeACharPointer)
{
	char buf[64];

	// %S reads a char* as a wchar_t* and scans past the end of it. The
	// argument count looks right, which is what makes it dangerous.
	SafeFormat::Format(buf, "%S", "narrow");

	CHECK(Is("%S", buf));
}

TEST(SafeFormat, PrintsALengthPrefixedStringConversionNarrow)
{
	char buf[64];

	// %ls is the same trap spelled differently. Here the length modifier
	// is dropped rather than the conversion refused: the argument really
	// is a char*, so it prints as one.
	SafeFormat::Format(buf, "%ls", "narrow");

	CHECK(Is("narrow", buf));
}

TEST(SafeFormat, RefusesAFloatingConversionAgainstAnIntegerArgument)
{
	char buf[64];

	// The review measured this one against the real MSVC runtime: %f
	// needs no width and emitted 308 bytes from a register the call site
	// never set.
	SafeFormat::Format(buf, "%f", 3);

	CHECK(Is("%f", buf));
}

TEST(SafeFormat, PerformsAFloatingConversionWhenTheArgumentIsADouble)
{
	char buf[64];

	SafeFormat::Format(buf, "%.2f", 1.5);

	CHECK(Is("1.50", buf));
}

TEST(SafeFormat, RefusesAPointerConversionAgainstANonPointer)
{
	char buf[64];

	SafeFormat::Format(buf, "%p", 7);

	CHECK(Is("%p", buf));
}

TEST(SafeFormat, PerformsAPointerConversionWhenTheArgumentIsAPointer)
{
	char buf[64];
	char expected[64];
	int  anything = 0;
	void* pValue = &anything;

	SafeFormat::Format(buf, "%p", pValue);

	// The rendering of a pointer is the runtime's business, so the
	// assertion is that this agrees with it rather than a literal.
	std::snprintf(expected, sizeof(expected), "%p", pValue);

	CHECK(Is(expected, buf));
}

TEST(SafeFormat, PrintsTheHostileNoticeEntryAsMostlyText)
{
	char buf[64];

	// The shape finding C19 and finding C22 both describe: one argument,
	// four string conversions and a write.
	SafeFormat::Format(buf, "[System]%s%s%s%s%n", "hello");

	CHECK(Is("[System]hello%s%s%s%n", buf));
}


//----------------------------------------------------------------------
// Bounds
//----------------------------------------------------------------------

TEST(SafeFormat, TruncatesAtTheDestinationSizeAndTerminates)
{
	char buf[8];
	std::memset(buf, 'Z', sizeof(buf));

	const int written = SafeFormat::Format(buf, "%s", "abcdefghijklmnop");

	CHECK(Is("abcdefg", buf));
	CHECK_EQ(7, written);
	CHECK_EQ('\0', buf[7]);
}

TEST(SafeFormat, TruncatesInTheMiddleOfALiteralRunAndTerminates)
{
	char buf[6];

	const int written = SafeFormat::Format(buf, "%d apples", 12);

	CHECK(Is("12 ap", buf));
	CHECK_EQ(5, written);
}

TEST(SafeFormat, WritesOnlyATerminatorIntoASingleByteDestination)
{
	char buf[2];
	buf[1] = 'Z';

	const int written = SafeFormat::Format(buf, 1, "abcdef");

	CHECK_EQ(0, written);
	CHECK_EQ('\0', buf[0]);

	// Nothing beyond the size it was given.
	CHECK_EQ('Z', buf[1]);
}

TEST(SafeFormat, ClampsAFieldWidthTheDataAsksFor)
{
	char buf[128];

	// A width of 9000 into a 128 byte row would be the whole row. The cap
	// is the one SanitizeGameStringTable rejects at, so a legitimate
	// entry - whose widest real field is 2 - is unaffected.
	const int written = SafeFormat::Format(buf, "%9000d|", 1);

	CHECK_EQ(33, written);
	CHECK_EQ('|', buf[32]);
	CHECK_EQ('1', buf[31]);
}

TEST(SafeFormat, DoesNotOverflowTheWidthAccumulatorOnALongDigitRun)
{
	char buf[64];

	SafeFormat::Format(buf, "%99999999999999999999d|", 7);

	// Whatever the digits said, the field is capped and the rest of the
	// entry still prints.
	CHECK_EQ(33, (int)std::strlen(buf));
	CHECK_EQ('|', buf[32]);
}

TEST(SafeFormat, TruncatesAStringConversionToItsPrecision)
{
	char buf[64];

	SafeFormat::Format(buf, "%.4s!", "abcdefgh");

	CHECK(Is("abcd!", buf));
}

TEST(SafeFormat, ClampsAPrecisionTheDataAsksFor)
{
	char buf[128];

	// The precision cap is the same 32 as the width cap, and had no test
	// of its own until the review round asked for one.
	SafeFormat::Format(buf, "%.9000s|", "0123456789012345678901234567890123456789");

	CHECK_EQ(33, (int)std::strlen(buf));
	CHECK_EQ('|', buf[32]);
}


//----------------------------------------------------------------------
// Malformed input
//----------------------------------------------------------------------

TEST(SafeFormat, PrintsATrailingPercentAsText)
{
	char buf[32];

	SafeFormat::Format(buf, "50%");

	CHECK(Is("50%", buf));
}

TEST(SafeFormat, PrintsAFormatThatEndsInsideASpecificationAsText)
{
	char buf[32];

	SafeFormat::Format(buf, "abc%-12", 5);

	CHECK(Is("abc%-12", buf));
}

TEST(SafeFormat, EmptiesTheDestinationForANullFormat)
{
	char buf[16];
	std::memset(buf, 'Z', sizeof(buf));

	const int written = SafeFormat::Format(buf, (const char*)NULL, 1);

	CHECK_EQ(0, written);
	CHECK(Is("", buf));
}

TEST(SafeFormat, SurvivesANullStringArgument)
{
	char buf[32];
	const char* pMissing = NULL;

	SafeFormat::Format(buf, "[%s]", pMissing);

	CHECK(Is("[]", buf));
}

TEST(SafeFormat, IgnoresAnArgumentCountWithNoArgumentArray)
{
	char buf[32];

	// FormatV is the entry point the templates call; a caller that says
	// three arguments and hands over none must not read the pointer.
	const int written = SafeFormat::FormatV(buf, sizeof(buf), "%d%s%d", NULL, 3);

	CHECK(Is("%d%s%d", buf));
	CHECK_EQ(6, written);
}

TEST(SafeFormat, ReportsNothingWrittenForANullDestination)
{
	CHECK_EQ(0, SafeFormat::Format((char*)NULL, 16, "%d", 1));
	CHECK_EQ(0, SafeFormat::Format((char*)NULL, 0, "%d", 1));
}


//----------------------------------------------------------------------
// Argument widths and kinds
//----------------------------------------------------------------------

TEST(SafeFormat, PrintsAnIntegerAtTheWidthTheCallSiteDeclared)
{
	char buf[32];

	// printf("%x", -1) prints ffffffff for an int. Packing every integer
	// into a long long and issuing %llx would print sixteen f here and
	// silently change what these call sites already print.
	SafeFormat::Format(buf, "%x", -1);
	CHECK(Is("ffffffff", buf));

	SafeFormat::Format(buf, "%x", (long long)-1);
	CHECK(Is("ffffffffffffffff", buf));
}

TEST(SafeFormat, PrintsASixtyFourBitValueWithoutTruncatingIt)
{
	char buf[32];

	SafeFormat::Format(buf, "%d", (long long)5000000000LL);

	CHECK(Is("5000000000", buf));
}

TEST(SafeFormat, AcceptsEitherSignednessForAnIntegerConversion)
{
	char buf[32];

	SafeFormat::Format(buf, "%d", (unsigned short)7);
	CHECK(Is("7", buf));

	SafeFormat::Format(buf, "%u", 7);
	CHECK(Is("7", buf));
}

TEST(SafeFormat, AcceptsAnEnumerationAsAnInteger)
{
	enum Race { RACE_SLAYER = 0, RACE_VAMPIRE = 1, RACE_OUSTER = 2 };

	char buf[32];

	SafeFormat::Format(buf, "race %d", RACE_OUSTER);

	CHECK(Is("race 2", buf));
}

TEST(SafeFormat, PrintsACharacterConversion)
{
	char buf[32];

	SafeFormat::Format(buf, "[%c]", 'x');

	CHECK(Is("[x]", buf));
}

TEST(SafeFormat, HonoursFlagsAndWidthTogether)
{
	char buf[32];

	SafeFormat::Format(buf, "[%-6d][%+d]", 42, 42);

	CHECK(Is("[42    ][+42]", buf));
}

TEST(SafeFormat, TakesTheDestinationSizeFromARealArray)
{
	char small[4];

	// The array overload is the one call sites should use: the bound
	// comes from the declaration, so it cannot be stated wrongly.
	SafeFormat::Format(small, "%s", "abcdefgh");

	CHECK(Is("abc", small));
}

//----------------------------------------------------------------------
// The allocation invariant
//----------------------------------------------------------------------
//
// C_VS_UI_ASK_DIALOG sizes each message row as strlen(entry) plus the
// room its own arguments need, and twelve of its call sites pass no
// arguments at all - so for those the whole allocation is
// strlen(entry)+1 (VS_UI/src/VS_UI_ExtraDialog.cpp, AllocAskMessage,
// task 5.4's fifth slice). That is safe only because a conversion this
// formatter refuses is copied out as the text it already was: refusing
// cannot make the output longer than the format it came from. If that
// ever stops holding, every one of those rows overflows by exactly the
// difference, so it is pinned here rather than left as a reading of the
// implementation.
//
// The guard bytes past the stated size are the point. An overrun in C++
// writes into whatever follows and carries on, so the assertion has to
// be that the bytes outside the destination were not touched.
//----------------------------------------------------------------------
TEST(SafeFormat, RefusingAConversionNeverWritesMoreThanTheFormatItCameFrom)
{
	static const char* const pFormats[] =
	{
		"%s",
		"%s%s%s",
		"%.*f",
		"%*d",
		"%n",
		"%hn",
		"%ls",
		"%p",
		"%08.3f",
		// A format that ends inside a specification. This one and "%"
		// take FormatV's end-of-format branch rather than its refusal
		// branch, so they pin less than the rest: that branch always
		// breaks out of the loop, so it is always the last write, and
		// growth there would be clipped by the bound instead of showing
		// up in the comparison below. Kept because the branch should
		// still round-trip, not because it would catch it growing.
		"%12.5",
		"%",
		"Store your pet? %s",
		"%s is asking to join your %s.",
		"Buy a storage box for $%d?",
	};

	const size_t nFormats = sizeof(pFormats) / sizeof(pFormats[0]);

	for (size_t i = 0; i < nFormats; ++i)
	{
		// Exactly the arithmetic AllocAskMessage performs when it has no
		// arguments to budget for.
		const size_t	nSize	= std::strlen(pFormats[i]) + 1;
		char			buf[128];

		std::memset(buf, '\xCD', sizeof(buf));

		SafeFormat::Format(buf, nSize, pFormats[i]);

		// The format back verbatim, and that is the whole assertion.
		// Checking only that the result fits would prove nothing at
		// all - FormatV bounds itself by nSize, so a refusal that grew
		// would still come back inside the bound, with the difference
		// quietly truncated away. Comparing against the format is what
		// makes the growth visible, because the truncated tail is
		// missing from the text. (%% is the one shape that legitimately
		// shrinks; it has its own test above.)
		CHECK(Is(pFormats[i], buf));

		// And nothing outside the stated size was touched. One check
		// for the whole guard region rather than one per byte, so a
		// failure names the format instead of drowning the suite's
		// check count in a hundred passes per case.
		char untouched[sizeof(buf)];
		std::memset(untouched, '\xCD', sizeof(untouched));

		CHECK_EQ(0, std::memcmp(buf + nSize, untouched, sizeof(buf) - nSize));
	}
}

TEST(SafeFormat, PlainCopyAndAppendPreserveTextAndBoundEveryWrite)
{
	for (size_t capacity = 0; capacity <= 12; ++capacity) {
		char destination[16];
		std::memset(destination, '!', sizeof(destination));
		const size_t copied = SafeFormat::Copy(destination, capacity, "abc%s");
		const std::string first = std::string("abc%s").substr(0, capacity ? capacity - 1 : 0);
		CHECK_EQ(first.size(), copied);
		if (capacity) CHECK(Is(first.c_str(), destination));
		const size_t appended = SafeFormat::Append(destination, capacity, "XYZ");
		const std::string expected = (first + "XYZ").substr(0, capacity ? capacity - 1 : 0);
		CHECK_EQ(expected.size(), appended);
		if (capacity) CHECK(Is(expected.c_str(), destination));
		for (size_t i = capacity; i < sizeof(destination); ++i) CHECK_EQ('!', destination[i]);
	}
}

TEST(SafeFormat, PlainTextOperationsSupportAliasedSources)
{
	char text[16] = "abcdef";
	CHECK_EQ(size_t(4), SafeFormat::Copy(text, text + 2));
	CHECK(Is("cdef", text));
	CHECK_EQ(size_t(4), SafeFormat::Copy(text + 2, sizeof(text) - 2, text));
	CHECK(Is("cdcdef", text));
	CHECK_EQ(size_t(6), SafeFormat::Copy(text, text));
	CHECK_EQ(size_t(12), SafeFormat::Append(text, text));
	CHECK(Is("cdcdefcdcdef", text));
	CHECK_EQ(size_t(15), SafeFormat::Append(text, text + 2));
	CHECK(Is("cdcdefcdcdefcde", text));
}

TEST(SafeFormat, PlainTextOperationsHandleMissingAndUnterminatedBuffers)
{
	CHECK_EQ(size_t(0), SafeFormat::Copy(nullptr, 8, "x"));
	CHECK_EQ(size_t(0), SafeFormat::Append(nullptr, 8, "x"));
	char text[5] = {'a', 'b', 'c', 'd', 'e'};
	CHECK_EQ(size_t(4), SafeFormat::Append(text, "ignored"));
	CHECK(Is("abcd", text));
	CHECK_EQ(size_t(4), SafeFormat::Append(text, nullptr));
	CHECK_EQ(size_t(0), SafeFormat::Copy(text, nullptr));
	CHECK(Is("", text));
	// Only the bytes that fit may be read, even without a source terminator.
	const char source[4] = {'w', 'x', 'y', 'z'};
	CHECK_EQ(size_t(4), SafeFormat::Copy(text, source));
	CHECK(Is("wxyz", text));
}
