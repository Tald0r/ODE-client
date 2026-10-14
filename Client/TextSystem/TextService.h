#ifndef TEXTSYSTEM_TEXTSERVICE_H
#define TEXTSYSTEM_TEXTSERVICE_H

#include <string>
#include <vector>

#include "TextBackend.h"

class CSpriteSurface;

namespace TextSystem {

struct NormalizationCacheLimits {
	size_t entries = 1024;
	size_t storageBytes = 1024 * 1024;
};

struct NormalizationCacheStats {
	size_t entries = 0;
	size_t storageBytes = 0;
	size_t computations = 0;
	size_t hits = 0;
};

class TextService {
public:
	static TextService& Get();

	FontHandle GetDefaultFont();
	FontHandle GetFont(int size);
	TextStyle GetDefaultStyle();

	Metrics MeasureText(const std::string& text, const TextStyle& style, int maxWidth = 0);
	std::vector<std::string> WrapText(const std::string& text, const TextStyle& style, int maxWidth);

	void DrawLine(RenderTarget& target, const std::string& text,
				 int x, int y, int maxWidth, const TextStyle& style);

	void DrawLines(RenderTarget& target, const std::vector<std::string>& lines,
				  int x, int y, int maxWidth, int maxLines, int startLine,
				  const TextStyle& style, int lineHeight);

	int GetLineHeight(const TextStyle& style) const;

	// Simple text rendering API (compatibility layer for SDL_RenderText):
	// white text in the default style at (x, y) on surface; nothing when
	// surface is null or has no backend surface. The executable passes
	// g_pLast, its back buffer (task 4.208; defined with the sprite-surface
	// target in RenderTargetSpriteSurface.cpp).
	static void RenderText(CSpriteSurface* surface, int x, int y, const std::string& text);

	// Display text is UTF-8. Preserve valid scalars and replace each malformed
	// byte with U+FFFD; legacy resources must decode at their declared boundary.
	static std::string NormalizeText(const std::string& text);
	// The cache is local to the calling thread. Reset also clears counters and
	// does not depend on the resource pack's encoding selection.
	static void ResetNormalizationCache(NormalizationCacheLimits limits = {});
	static NormalizationCacheStats GetNormalizationCacheStats();

private:
	TextService();
	~TextService();
	TextService(const TextService&) = delete;
	TextService& operator=(const TextService&) = delete;

	bool EnsureInitialized();
	int MeasureLineWidth(const std::string& text, FontHandle font);

private:
	TextBackend* m_backend;
	bool m_initialized;
	FontHandle m_defaultFont;
};

} // namespace TextSystem

#endif // TEXTSYSTEM_TEXTSERVICE_H
