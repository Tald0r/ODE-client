#include "test_framework.h"
#include "TextEncoding.h"

#include <limits>
#include <string>

namespace {
using TextEncoding::Encoding;
using TextEncoding::InvalidInput;
using TextEncoding::Convert;
struct ResourceScope {
	Encoding saved = TextEncoding::GetResourceEncoding();
	~ResourceScope() { TextEncoding::SetResourceEncoding(saved); }
};
}

TEST(TextEncoding, ExplicitCodePagesRoundTripNonAsciiAndEmbeddedNul)
{
	struct Vector { Encoding encoding; const char* bytes; const char* utf8; };
	const Vector vectors[] = {
		{Encoding::Cp949, "\xC7\xD1", "\xED\x95\x9C"},
		{Encoding::EucKr, "\xC7\xD1", "\xED\x95\x9C"},
		{Encoding::Gbk, "\xC4\xE3\xBA\xC3", "\xE4\xBD\xA0\xE5\xA5\xBD"},
		{Encoding::Gb2312, "\xC4\xE3\xBA\xC3", "\xE4\xBD\xA0\xE5\xA5\xBD"},
		{Encoding::Big5, "\xA7\x41\xA6\x6E", "\xE4\xBD\xA0\xE5\xA5\xBD"},
		{Encoding::Utf8, "\xF0\x9F\x98\x80", "\xF0\x9F\x98\x80"}
	};
	for (const auto& v : vectors) {
		const std::string input = std::string(v.bytes) + '\0' + "tail";
		const std::string expected = std::string(v.utf8) + '\0' + "tail";
		std::string output("old");
		CHECK(Convert(input, v.encoding, Encoding::Utf8, output));
		CHECK(output == expected);
		CHECK(Convert(output, Encoding::Utf8, v.encoding, output));
		CHECK(output == input);
	}
}

TEST(TextEncoding, DeclaredLegacyBytesAreNotGuessedAsUtf8)
{
	const std::string bytes("\xC2\xA1");
	std::string utf8, korean, encoded;
	CHECK(Convert(bytes, Encoding::Utf8, Encoding::Utf8, utf8));
	CHECK(utf8 == bytes);
	CHECK(Convert(bytes, Encoding::Cp949, Encoding::Utf8, korean));
	CHECK(korean == "\xEC\xA7\x95");
	CHECK(Convert(korean, Encoding::Utf8, Encoding::Cp949, encoded));
	CHECK(encoded == bytes);
}

TEST(TextEncoding, RejectionNeverPublishesAPrefixOrTransliterates)
{
	std::string output("kept");
	for (const auto encoding : {Encoding::Cp949, Encoding::EucKr, Encoding::Gbk,
		Encoding::Gb2312, Encoding::Big5}) {
		CHECK(!Convert("prefix\xA1", encoding, Encoding::Utf8, output));
		CHECK(output == "kept");
	}
	for (const auto* bytes : {"\xC0\x80", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xF0\x9F"}) {
		CHECK(!Convert(bytes, Encoding::Utf8, Encoding::Utf8, output));
		CHECK(!Convert(bytes, Encoding::Utf8, Encoding::Cp949, output));
		CHECK(output == "kept");
	}
	CHECK(!Convert("\xF0\x9F\x98\x80", Encoding::Utf8, Encoding::Cp949, output));
	CHECK(output == "kept");
}

TEST(TextEncoding, InvalidResourceBytesBecomeReplacementCharactersWithoutGuessing)
{
	std::string output;
	CHECK(Convert("\xC7\xD1\xFF!\xA1", Encoding::Cp949, Encoding::Utf8,
		output, InvalidInput::Replace));
	CHECK(output == "\xED\x95\x9C\xEF\xBF\xBD!\xEF\xBF\xBD");
	CHECK(Convert("a\xF0\x9F!", Encoding::Utf8, Encoding::Utf8, output, InvalidInput::Replace));
	CHECK(output == "a\xEF\xBF\xBD\xEF\xBF\xBD!");
}

TEST(TextEncoding, ImpossibleSizesAndInvalidArgumentsAreRejectedBeforeReading)
{
	std::string output("kept");
	const auto huge = (std::numeric_limits<size_t>::max)();
	CHECK(!Convert("x", huge, Encoding::Cp949, Encoding::Utf8, output));
	CHECK(!Convert("x", huge / 4, Encoding::Utf8, Encoding::Utf8, output));
	CHECK(!Convert(nullptr, 1, Encoding::Utf8, Encoding::Utf8, output));
	CHECK(!Convert("x", 1, static_cast<Encoding>(255), Encoding::Utf8, output));
	CHECK(!Convert("x", 1, Encoding::Utf8, static_cast<Encoding>(255), output));
	CHECK(!Convert("x", 1, Encoding::Utf8, Encoding::Cp949, output, InvalidInput::Replace));
	CHECK(output == "kept");
	CHECK(Convert(nullptr, 0, Encoding::Utf8, Encoding::Utf8, output));
	CHECK(output.empty());
}

TEST(TextEncoding, ResourceSelectionRejectsUnknownNamesWithoutChangingPolicy)
{
	ResourceScope scope;
	CHECK(TextEncoding::SetResourceEncoding("GBK"));
	CHECK(TextEncoding::GetResourceEncoding() == Encoding::Gbk);
	CHECK(!TextEncoding::SetResourceEncoding("guess"));
	CHECK(!TextEncoding::SetResourceEncoding(static_cast<Encoding>(255)));
	CHECK(TextEncoding::GetResourceEncoding() == Encoding::Gbk);
	for (const auto encoding : {Encoding::Utf8, Encoding::Cp949, Encoding::EucKr,
		Encoding::Gbk, Encoding::Gb2312, Encoding::Big5}) {
		Encoding parsed = Encoding::Utf8;
		CHECK(TextEncoding::Parse(TextEncoding::Name(encoding), parsed));
		CHECK(parsed == encoding);
	}
	CHECK(TextEncoding::SetResourceEncoding("utf-8"));
	CHECK(TextEncoding::GetResourceEncoding() == Encoding::Utf8);
}

TEST(TextEncoding, NormalizeUtf8KeepsValidTextAndReplacesBadBytes)
{
	CHECK(TextEncoding::NormalizeUtf8("").empty());
	CHECK(TextEncoding::NormalizeUtf8("plain") == "plain");
	const std::string hangul = "\xED\x95\x9C";
	CHECK(TextEncoding::NormalizeUtf8(hangul + "!") == hangul + "!");
	// Each byte that is not UTF-8 becomes U+FFFD; the rest is kept.
	CHECK(TextEncoding::NormalizeUtf8("a\xFF" "b") == "a\xEF\xBF\xBD" "b");
	CHECK(TextEncoding::NormalizeUtf8("\xC0\xAF") == "\xEF\xBF\xBD\xEF\xBF\xBD");
}
