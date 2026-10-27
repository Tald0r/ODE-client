//----------------------------------------------------------------------
// test_text_wrap_bounds.cpp
//----------------------------------------------------------------------
//
// The text system's own wrapping, measuring and drawing of a server
// string, the route task 5 (a) of docs/reviews/NEXT.md follows: every
// server string ends there, the longest cut to the talk box's
// 2048-byte row. Each loop in TextService runs over the string's own
// bytes, Utf8Decode never reads past the bytes it is given and always
// moves on by one, and the glyph and metric caches refuse what is past
// their budget before they evict. These pin what a 2048-byte row, a
// width of zero and a width narrower than any glyph do.
//
//----------------------------------------------------------------------

#include "test_framework.h"
#include "CSpriteSurface.h"
#include "SpriteLibBackend.h"
#include "TextSystem/TextService.h"
#include "TextSystem/RenderTargetSpriteSurface.h"

#include <string>
#include <vector>

namespace {

// 2048 bytes: words of ASCII, two- and three-byte scalars, a word longer
// than any width here, and bytes that are no UTF-8.
std::string LongRow()
{
	const std::string pieces[] = {"Gold ", "\xC3\xA9t\xC3\xA9 ", "\xE4\xB8\xAD\xE6\x96\x87 ",
		"\xFF\xC2 ", "aLongWordWithoutAnyPlaceToBreakItAtAll "};
	std::string row;
	for (size_t i = 0; row.size() < 2048; ++i) row += pieces[i % 5];
	row.resize(2048);
	return row;
}

int Width(const std::string& line, const TextSystem::TextStyle& style)
{
	return TextSystem::TextService::Get().MeasureText(line, style, 0).width;
}

} // namespace

TEST(TextWrapBounds, ATwoKilobyteRowWrapsIntoLinesNoWiderThanTheWidth)
{
	auto& text = TextSystem::TextService::Get();
	const auto style = text.GetDefaultStyle();
	CHECK(style.font.IsValid());
	const std::string row = LongRow();
	const std::vector<std::string> lines = text.WrapText(row, style, 200);
	CHECK(lines.size() > 10);
	size_t bytes = 0;
	for (const std::string& line : lines)
	{
		CHECK(Width(line, style) <= 200);
		bytes += line.size();
	}
	// A break drops the space it breaks at and nothing else.
	CHECK(bytes <= text.NormalizeText(row).size());
	const auto metrics = text.MeasureText(row, style, 200);
	CHECK(metrics.width <= 200);
	CHECK_EQ(int(lines.size()) * metrics.lineHeight, metrics.height);
}

TEST(TextWrapBounds, AWidthOfZeroKeepsTheRowOnOneLine)
{
	auto& text = TextSystem::TextService::Get();
	const auto style = text.GetDefaultStyle();
	CHECK(style.font.IsValid());
	const std::string row = LongRow();
	for (int width : {0, -1})
	{
		const std::vector<std::string> lines = text.WrapText(row, style, width);
		CHECK_EQ(1u, lines.size());
		CHECK(!lines.empty() && lines[0] == text.NormalizeText(row));
	}
	CHECK(text.MeasureText(row, style, 0).width > 0);
}

TEST(TextWrapBounds, AWidthNarrowerThanAnyGlyphPutsOneGlyphOnALine)
{
	auto& text = TextSystem::TextService::Get();
	const auto style = text.GetDefaultStyle();
	CHECK(style.font.IsValid());
	const std::string row = "ab\xC3\xA9\xE4\xB8\xAD";
	const std::vector<std::string> lines = text.WrapText(row, style, 1);
	CHECK(lines == std::vector<std::string>({"a", "b", "\xC3\xA9", "\xE4\xB8\xAD"}));
}

TEST(TextWrapBounds, DrawingATwoKilobyteRowStaysOnTheSurface)
{
	CHECK_EQ(0, spritectl_init());
	CSpriteSurface surface;
	CHECK(surface.Init(64, 16));
	surface.FillSurface(0x0000);
	TextSystem::SpriteSurfaceRenderTarget target(&surface);
	auto& text = TextSystem::TextService::Get();
	auto style = text.GetDefaultStyle();
	style.color = TextSystem::ColorFromRGB(0xFFFFFF);
	const std::string row = LongRow();
	// Left, far right and far left of the surface: the pen runs thousands of
	// pixels past its edge, and the blits clip.
	for (int x : {0, 100000, -100000})
		text.DrawLine(target, row, x, 0, 0, style);
	spritectl_surface_info_t info{};
	CHECK_EQ(0, spritectl_lock_surface(surface.GetBackendSurface(), &info));
	int lit = 0;
	for (int y = 0; info.pixels && y < info.height; ++y)
		for (int x = 0; x < info.width; ++x)
			lit += reinterpret_cast<WORD*>(static_cast<BYTE*>(info.pixels) + y * info.pitch)[x] != 0;
	spritectl_unlock_surface(surface.GetBackendSurface());
	CHECK(lit > 0);
}
