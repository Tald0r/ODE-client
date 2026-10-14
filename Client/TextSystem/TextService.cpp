#include "TextService.h"
#include "TextUtf8.h"
#include "TextEncoding.h"

#include <algorithm>
#include <cstring>
#include <list>
#include <new>
#include <string_view>
#include <unordered_map>

#ifdef USE_SDL_BACKEND
#include <SDL.h>
#endif

namespace TextSystem {

namespace {
struct NormalizedEntry {
	std::string input;
	std::string output;
	size_t bytes = 0;
};

struct NormalizationCache {
	NormalizationCacheLimits limits;
	NormalizationCacheStats stats;
	std::list<NormalizedEntry> recent;
	// Keys borrow immutable strings owned by recent, never caller buffers.
	std::unordered_map<std::string_view, std::list<NormalizedEntry>::iterator> positions;

	void Remember(const std::string& input, const std::string& output)
	{
		if (limits.entries == 0 || input.size() >= limits.storageBytes ||
			output.size() >= limits.storageBytes - input.size())
			return;
		try {
			recent.push_front({input, output});
		} catch (const std::bad_alloc&) {
			return; // Optional caching must not discard a successfully normalized line.
		}
		auto& entry = recent.front();
		// Count owned string capacity, including terminators. Entry limits bound
		// list/map metadata separately. Subtraction keeps even huge limits safe.
		if (entry.input.capacity() >= limits.storageBytes ||
			entry.output.capacity() >= limits.storageBytes - entry.input.capacity() - 1) {
			recent.pop_front();
			return;
		}
		entry.bytes = entry.input.capacity() + entry.output.capacity() + 2;
		while (positions.size() >= limits.entries ||
			entry.bytes > limits.storageBytes - stats.storageBytes) {
			const auto& cold = recent.back();
			positions.erase(std::string_view(cold.input));
			stats.storageBytes -= cold.bytes;
			recent.pop_back();
		}
		try {
			if (!positions.emplace(std::string_view(entry.input), recent.begin()).second) {
				recent.pop_front();
				return;
			}
		} catch (const std::bad_alloc&) {
			recent.pop_front();
			return;
		}
		stats.storageBytes += entry.bytes;
	}
};

thread_local NormalizationCache normalizationCache;
}

void TextService::ResetNormalizationCache(NormalizationCacheLimits limits)
{
	// Destroy borrowed keys before their owning strings.
	decltype(normalizationCache.positions){}.swap(normalizationCache.positions);
	normalizationCache.recent.clear();
	normalizationCache.stats = {};
	normalizationCache.limits = limits;
}

NormalizationCacheStats TextService::GetNormalizationCacheStats()
{
	auto stats = normalizationCache.stats;
	stats.entries = normalizationCache.positions.size();
	return stats;
}

// Resources decode at their declared boundary; display text is UTF-8.
// Repair malformed bytes without guessing a different character set.
static std::string NormalizeUncached(const std::string& text)
{
	if (text.empty())
		return text;

	if (IsValidUtf8(text.c_str(), text.size()))
		return text;

	using TextEncoding::Encoding;
	std::string converted;
	if (!TextEncoding::Convert(text, Encoding::Utf8, Encoding::Utf8, converted,
		TextEncoding::InvalidInput::Replace)) return {};
	return converted;
}

std::string TextService::NormalizeText(const std::string& text)
{
	auto& cache = normalizationCache;
	const auto found = cache.positions.find(std::string_view(text));
	if (found != cache.positions.end()) {
		cache.recent.splice(cache.recent.begin(), cache.recent, found->second);
		++cache.stats.hits;
		return found->second->output;
	}
	++cache.stats.computations;
	std::string normalized = NormalizeUncached(text);
	cache.Remember(text, normalized);
	return normalized;
}

TextService::TextService()
	: m_backend(NULL)
	, m_initialized(false)
	, m_defaultFont()
{
}

TextService::~TextService()
{
	delete m_backend;
	m_backend = NULL;
}

TextService& TextService::Get()
{
	static TextService s_instance;
	return s_instance;
}

bool TextService::EnsureInitialized()
{
	if (m_initialized)
		return true;

	m_backend = CreateSDLTextBackend();
	if (!m_backend)
		return false;

	if (!m_backend->Initialize())
		return false;

	FontDesc defaultDesc;
	defaultDesc.size = 16;
	m_defaultFont = m_backend->AcquireFont(defaultDesc);
	m_initialized = m_defaultFont.IsValid();
	return m_initialized;
}

FontHandle TextService::GetDefaultFont()
{
	EnsureInitialized();
	return m_defaultFont;
}

FontHandle TextService::GetFont(int size)
{
	if (!EnsureInitialized())
		return FontHandle();

	FontDesc desc;
	desc.size = size;
	FontHandle handle = m_backend->AcquireFont(desc);
	if (!handle.IsValid())
		return m_defaultFont;
	return handle;
}

TextStyle TextService::GetDefaultStyle()
{
	TextStyle style;
	style.font = GetDefaultFont();
	style.align = TextAlign::Left;
	style.lineSpacing = 0;
	style.color = ColorFromRGB(0xFFFFFF);
	return style;
}

int TextService::GetLineHeight(const TextStyle& style) const
{
	if (!m_initialized || !m_backend)
		return 16;
	return m_backend->GetLineHeight(style.font);
}

int TextService::MeasureLineWidth(const std::string& text, FontHandle font)
{
	if (!EnsureInitialized())
		return 0;

	int width = 0;
	const char* p = text.c_str();
	int remaining = static_cast<int>(text.size());
	while (*p && remaining > 0) {
		int len = 0;
		uint32_t codepoint = Utf8Decode(p, remaining, &len);
		if (len == 0) break; // Safety: no data left
		p += len;
		remaining -= len;

		if (codepoint == '\n')
			break;

		GlyphMetrics metrics;
		if (m_backend->GetGlyphMetrics(font, codepoint, metrics)) {
			width += metrics.advance;
		}
	}
	return width;
}

Metrics TextService::MeasureText(const std::string& text, const TextStyle& style, int maxWidth)
{
	Metrics out = {0, 0, GetLineHeight(style)};
	std::vector<std::string> lines = WrapText(text, style, maxWidth);

	int maxLineWidth = 0;
	for (size_t i = 0; i < lines.size(); ++i) {
		int w = MeasureLineWidth(lines[i], style.font);
		if (w > maxLineWidth)
			maxLineWidth = w;
	}

	out.width = maxLineWidth;
	if (!lines.empty()) {
		out.height = static_cast<int>(lines.size()) * (out.lineHeight + style.lineSpacing) - style.lineSpacing;
	}
	return out;
}

std::vector<std::string> TextService::WrapText(const std::string& text, const TextStyle& style, int maxWidth)
{
	std::string normalized = NormalizeText(text);

	std::vector<std::string> lines;
	if (!EnsureInitialized()) {
		lines.push_back(normalized);
		return lines;
	}

	std::string line;
	int lineWidth = 0;
	int lastBreakIndex = -1;
	int lastBreakSkip = 0;

	const char* p = normalized.c_str();
	int remaining = static_cast<int>(normalized.size());
	while (*p && remaining > 0) {
		int len = 0;
		uint32_t codepoint = Utf8Decode(p, remaining, &len);
		if (len == 0) break; // Safety: no data left

		if (codepoint == '\n') {
			lines.push_back(line);
			line.clear();
			lineWidth = 0;
			lastBreakIndex = -1;
			lastBreakSkip = 0;
			p += len;
			remaining -= len;
			continue;
		}

		GlyphMetrics metrics;
		if (!m_backend->GetGlyphMetrics(style.font, codepoint, metrics)) {
			p += len;
			remaining -= len;
			continue;
		}

		if (codepoint == ' ') {
			lastBreakIndex = static_cast<int>(line.size());
			lastBreakSkip = len;
		}

		if (maxWidth > 0 && lineWidth + metrics.advance > maxWidth && !line.empty()) {
			// lastBreakIndex may have just been set to line.size() on this very
			// iteration (codepoint == ' ' above, before the space is appended
			// below), so lastBreakIndex + lastBreakSkip can exceed line.size()
			// here - substr() would throw std::out_of_range in that case, so
			// treat it the same as "no break point recorded".
			if (lastBreakIndex >= 0 && lastBreakIndex + lastBreakSkip <= static_cast<int>(line.size())) {
				lines.push_back(line.substr(0, lastBreakIndex));
				line = line.substr(lastBreakIndex + lastBreakSkip);
				lineWidth = MeasureLineWidth(line, style.font);
				lastBreakIndex = -1;
				lastBreakSkip = 0;
			} else {
				lines.push_back(line);
				line.clear();
				lineWidth = 0;
				lastBreakIndex = -1;
				lastBreakSkip = 0;
			}
		}

		line.append(p, len);
		lineWidth += metrics.advance;
		p += len;
		remaining -= len;
	}

	if (!line.empty())
		lines.push_back(line);

	if (lines.empty())
		lines.push_back(std::string());

	return lines;
}

void TextService::DrawLine(RenderTarget& target, const std::string& text,
					 int x, int y, int maxWidth, const TextStyle& style)
{
	if (!EnsureInitialized())
		return;

	std::string normalized = NormalizeText(text);
	int lineWidth = MeasureLineWidth(normalized, style.font);
	int drawX = x;
	if (style.align == TextAlign::Center) {
		drawX = x + (maxWidth - lineWidth) / 2;
	} else if (style.align == TextAlign::Right) {
		drawX = x + (maxWidth - lineWidth);
	}

	const char* p = normalized.c_str();
	int remaining = static_cast<int>(normalized.size());
	int penX = drawX;

	while (*p && remaining > 0) {
		int len = 0;
		uint32_t codepoint = Utf8Decode(p, remaining, &len);
		if (len == 0) break; // Safety: no data left
		p += len;
		remaining -= len;

		GlyphMetrics metrics;
		if (!m_backend->GetGlyphMetrics(style.font, codepoint, metrics))
			continue;

		const Glyph* glyph = m_backend->GetGlyph(style.font, codepoint, style.color);
		if (glyph) {
			// SDL_ttf renders each UTF-8 glyph on a full line-height surface.
			// Its baseline is already positioned inside that surface. Applying
			// ink bearings again raises descenders (g, p, y) and shifts capitals.
			// Only compensate for padding SDL_ttf adds for a negative left bearing.
			m_backend->DrawGlyph(target, *glyph,
				penX + (std::min)(0, metrics.bearingX), y, style.color.a);
		}

		penX += metrics.advance;
	}
}

void TextService::DrawLines(RenderTarget& target, const std::vector<std::string>& lines,
					   int x, int y, int maxWidth, int maxLines, int startLine,
					   const TextStyle& style, int lineHeight)
{
	if (!EnsureInitialized())
		return;

	int count = static_cast<int>(lines.size());
	if (startLine < 0)
		startLine = 0;
	if (startLine >= count)
		return;

	int endLine = count;
	if (maxLines > 0)
		endLine = (std::min)(count, startLine + maxLines);

	int drawY = y;
	for (int i = startLine; i < endLine; ++i) {
		DrawLine(target, lines[i], x, drawY, maxWidth, style);
		drawY += lineHeight + style.lineSpacing;
	}
}

// RenderText is in RenderTargetSpriteSurface.cpp, beside the sprite-surface
// target it draws through. The caller passes the surface (task 4.208); it
// used to read the executable's back buffer, which kept it in the
// executable (task 5.3).

} // namespace TextSystem
