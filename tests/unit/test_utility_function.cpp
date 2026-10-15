#include "test_framework.h"
#include "UtilityFunction.h"
#include <cstring>
#include <string>

// The account name and password input rules (task 4.232). An ID is 4 to
// 10 bytes with no space, does not start with a digit, and holds letters
// and digits (and the permitted characters), or hangul without letters;
// a 2-byte pair that is a lone jamo of the source's jamo list (at an even
// byte offset) is refused. A password may not hold a backslash or a quote.
namespace {

std::string Trimmed(const char* text)
{
	char buffer[64];
	std::snprintf(buffer, sizeof(buffer), "%s", text);
	char* str = buffer;
	RemoveStringSpace(str);
	return str;
}

} // namespace

TEST(UtilityFunction, RemoveStringSpaceTrimsBothEnds)
{
	CHECK(Trimmed("  ab c  ") == "ab c");
	CHECK(Trimmed("ab c") == "ab c");
	CHECK(Trimmed("   ab") == "ab");
	CHECK(Trimmed("ab   ") == "ab");
	CHECK(Trimmed("") == "");
	CHECK(Trimmed("    ") == "");
	CHECK(Trimmed(" a ") == "a");
}

TEST(UtilityFunction, AnIDIsFourToTenWithoutSpaces)
{
	CHECK_EQ(0, IsValidID("abc", nullptr));
	CHECK_EQ(1, IsValidID("abcd", nullptr));
	CHECK_EQ(1, IsValidID("abcdefghij", nullptr));
	CHECK_EQ(0, IsValidID("abcdefghijk", nullptr));
	CHECK_EQ(0, IsValidID("ab cd", nullptr));
	CHECK_EQ(0, IsValidID(" abcd", nullptr));
	CHECK_EQ(0, IsValidID("ab cd", " "));				// even a permitted space
	CHECK_EQ(0, IsValidID(std::string(200, 'a').c_str(), nullptr));
}

TEST(UtilityFunction, AnIDHoldsLettersDigitsAndThePermitted)
{
	CHECK_EQ(1, IsValidID("Abc12", nullptr));
	CHECK_EQ(1, IsValidID("zZ99", nullptr));
	CHECK_EQ(0, IsValidID("1abc", nullptr));			// not a digit first
	CHECK_EQ(0, IsValidID("9abc", nullptr));
	CHECK_EQ(0, IsValidID("0abc", nullptr));
	CHECK_EQ(0, IsValidID("1234", "1"));				// a permitted digit still may not lead
	CHECK_EQ(0, IsValidID("ab_c", nullptr));
	CHECK_EQ(1, IsValidID("ab_c", "_-"));
	CHECK_EQ(1, IsValidID("ab-c", "_-"));
	CHECK_EQ(0, IsValidID("ab!c", "_-"));
	CHECK_EQ(0, IsValidID("ab@c", nullptr));
	CHECK_EQ(1, IsValidID("_abc", "_"));				// a permitted one may lead
}

TEST(UtilityFunction, AnIDOfHangulTakesNoLettersAndNoLoneJamo)
{
	// Two-byte pairs read from a high byte.
	CHECK_EQ(1, IsValidID("\xEA\xB0\xEA\xB0", nullptr));
	CHECK_EQ(1, IsValidID("\xEA\xB0" "12", nullptr));		// with digits
	CHECK_EQ(0, IsValidID("ab\xEA\xB0", nullptr));			// with letters
	CHECK_EQ(0, IsValidID("\xEA\xB0" "ab", nullptr));
	CHECK_EQ(0, IsValidID("abc\xEA", nullptr));				// a high byte last
	// The jamo list is the source's UTF-8: a pair found at an even byte
	// offset of it is refused, at an odd one not.
	CHECK_EQ(0, IsValidID("\xE3\x85\xEA\xB0", nullptr));	// offset 0
	CHECK_EQ(1, IsValidID("\x85\x82\xEA\xB0", nullptr));	// offset 1
}

TEST(UtilityFunction, APasswordTakesNoBackslashOrQuote)
{
	CHECK_EQ(1, IsValidPassword("secret"));
	CHECK_EQ(1, IsValidPassword(""));
	CHECK_EQ(1, IsValidPassword("1234"));					// digits alone are fine
	CHECK_EQ(1, IsValidPassword("a\"b"));
	CHECK_EQ(0, IsValidPassword("a\\b"));
	CHECK_EQ(0, IsValidPassword("a'b"));
	CHECK_EQ(0, IsValidPassword("'"));
}
