// Regression guards for complete renderer output. Display input is UTF-8;
// malformed bytes become replacement characters without dropping valid text.
// Resource codecs are covered in test_textservice_normalize.cpp.
#include "test_framework.h"

#include "TextService.h"
#include "RenderTarget.h"
#include "SpriteLib/SpriteLibBackend.h"
#include "SpriteLib/CSpriteSurface.h"
#include "RenderTargetSpriteSurface.h"
#include <vector>
#include <cstring>
#include <string>

namespace {
const char UTF8_TWO_SYLLABLES[] = "\xEA\xB7\xB8\xEB\xA3\xA8";
const char UTF8_FIRST_SYLLABLE[] = "\xEA\xB7\xB8";
const char REPLACEMENT[] = "\xEF\xBF\xBD";

std::string Normalize(const std::string& in)
{
	return TextSystem::TextService::NormalizeText(in);
}
} // namespace

TEST(TextServiceEncodingGuard, ValidUtf8IsPreserved)
{
	CHECK(std::string(UTF8_TWO_SYLLABLES) == Normalize(UTF8_TWO_SYLLABLES));
}

TEST(TextServiceEncodingGuard, TruncatedTailDoesNotDropTheValidPrefix)
{
	const std::string truncated = std::string(UTF8_FIRST_SYLLABLE) + "\xEB\xA3";
	CHECK(std::string(UTF8_FIRST_SYLLABLE) != Normalize(truncated));
	CHECK(std::string(UTF8_FIRST_SYLLABLE) + REPLACEMENT + REPLACEMENT == Normalize(truncated));
}

TEST(TextServiceEncodingGuard, InvalidByteDoesNotDropFollowingText)
{
	const std::string spliced =
		std::string(UTF8_TWO_SYLLABLES) + "\xFF" + UTF8_TWO_SYLLABLES;
	CHECK(std::string(UTF8_TWO_SYLLABLES) != Normalize(spliced));
	CHECK(std::string(UTF8_TWO_SYLLABLES) + REPLACEMENT + UTF8_TWO_SYLLABLES == Normalize(spliced));
}

TEST(TextServiceEncodingGuard, UnrecognisedTextStillProducesVisibleReplacements)
{
	const std::string garbage = "\xFF\xFE\xFF";
	CHECK(!Normalize(garbage).empty());
	CHECK(std::string(REPLACEMENT) + REPLACEMENT + REPLACEMENT == Normalize(garbage));
}

// Font rendering is shared by chat, options and character statistics. Descenders
// must extend below an ordinary lowercase letter, not be shifted upward until
// their bottoms line up. Check actual rasterized pixels at the UI font sizes.

namespace {
class TextTestSurface : public TextSystem::RenderTarget {
public:
    TextTestSurface() : surface(spritectl_create_surface(110, 60, SPRITECTL_FORMAT_RGB565)) {}
    ~TextTestSurface() { spritectl_destroy_surface(surface); }
    void* GetNative(TextSystem::NativeTargetType) const override { return surface; }
    int GetWidth() const override { return 110; }
    int GetHeight() const override { return 60; }
    spritectl_surface_t surface;
};
}

TEST(TextServiceRaster, DescendersShareTheLowercaseBaseline)
{
    CHECK_EQ(0, spritectl_init());
    TextTestSurface target;
    CHECK(target.surface != nullptr);
    if (!target.surface) return;
    auto& text = TextSystem::TextService::Get();
    for (int size : {12, 14, 16}) {
        spritectl_surface_info_t pixels;
        CHECK_EQ(0, spritectl_lock_surface(target.surface, &pixels));
        std::memset(pixels.pixels, 0, pixels.pitch * pixels.height);
        spritectl_unlock_surface(target.surface);
        auto style = text.GetDefaultStyle();
        style.font = text.GetFont(size);
        CHECK(style.font.IsValid());
        text.DrawLine(target, "a", 10, 10, 0, style);
        text.DrawLine(target, "y", 40, 10, 0, style);
        text.DrawLine(target, "g", 70, 10, 0, style);
        int top[3] = {60, 60, 60};
        int bottom[3] = {-1, -1, -1};
        CHECK_EQ(0, spritectl_lock_surface(target.surface, &pixels));
        for (int y = 0; y < pixels.height; ++y) {
            const auto* row = reinterpret_cast<const uint16_t*>(
                static_cast<const unsigned char*>(pixels.pixels) + y * pixels.pitch);
            for (int letter = 0; letter < 3; ++letter) {
                for (int x = 10 + letter * 30; x < 35 + letter * 30; ++x) {
                    if (row[x]) {
                        if (top[letter] == 60) top[letter] = y;
                        bottom[letter] = y;
                    }
                }
            }
        }
        spritectl_unlock_surface(target.surface);
        CHECK(bottom[0] >= 10);
        for (int letter = 1; letter < 3; ++letter) {
            CHECK(bottom[letter] > bottom[0]);
            CHECK(top[letter] >= top[0] - 1);
            CHECK(top[letter] <= top[0] + 1);
        }
    }
}

// RenderText, the executable's quick white text: the default style in
// white at (x, y) on the surface it is given, exactly as DrawLine draws it
// (task 4.208); nothing on a null surface or one without a backend.
namespace {
std::vector<uint16_t> Pixels(CSpriteSurface& surface)
{
    std::vector<uint16_t> out;
    spritectl_surface_info_t info;
    if (spritectl_lock_surface(surface.GetBackendSurface(), &info) != 0) return out;
    for (int y = 0; y < info.height; ++y) {
        const auto* row = reinterpret_cast<const uint16_t*>(
            static_cast<const unsigned char*>(info.pixels) + y * info.pitch);
        out.insert(out.end(), row, row + info.width);
    }
    spritectl_unlock_surface(surface.GetBackendSurface());
    return out;
}
}

TEST(TextServiceRenderText, DrawsTheDefaultStyleInWhiteAtThePoint)
{
    CHECK_EQ(0, spritectl_init());
    CSpriteSurface drawn, expected;
    CHECK(drawn.Init(120, 60) && expected.Init(120, 60));
    if (!drawn.GetBackendSurface() || !expected.GetBackendSurface()) return;
    drawn.FillSurface(0);
    expected.FillSurface(0);

    TextSystem::TextService::RenderText(&drawn, 30, 20, "Hi");

    auto& service = TextSystem::TextService::Get();
    auto style = service.GetDefaultStyle();
    style.color.r = style.color.g = style.color.b = style.color.a = 255;
    TextSystem::SpriteSurfaceRenderTarget target(&expected);
    service.DrawLine(target, "Hi", 30, 20, 0, style);

    const auto got = Pixels(drawn), want = Pixels(expected);
    CHECK(got.size() == 120u * 60u);
    CHECK(got == want);
    // Something was drawn, in white, below and right of the point.
    bool white = false, beforePoint = false;
    for (size_t i = 0; i < got.size(); ++i) {
        if (!got[i]) continue;
        white = white || got[i] == 0xFFFF;
        beforePoint = beforePoint || static_cast<int>(i % 120) < 30 || static_cast<int>(i / 120) < 20;
    }
    CHECK(white);
    CHECK(!beforePoint);

    // Another text at another point, the same way.
    drawn.FillSurface(0);
    expected.FillSurface(0);
    TextSystem::TextService::RenderText(&drawn, 7, 31, "Ok go");
    service.DrawLine(target, "Ok go", 7, 31, 0, style);
    CHECK(Pixels(drawn) == Pixels(expected));
}

TEST(TextServiceRenderText, NeedsASurfaceWithABackend)
{
    CHECK_EQ(0, spritectl_init());
    TextSystem::TextService::RenderText(nullptr, 30, 20, "Hi");
    CSpriteSurface bare;
    CHECK(bare.GetBackendSurface() == nullptr);
    TextSystem::TextService::RenderText(&bare, 30, 20, "Hi");
}
