#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace TextEncoding {

enum class Encoding : unsigned char { Utf8, Cp949, EucKr, Gbk, Gb2312, Big5 };
enum class InvalidInput { Reject, Replace };

const char* Name(Encoding encoding);
bool Parse(std::string_view name, Encoding& encoding);

// No encoding detection or transliteration. Failure preserves output, including
// when it aliases input. Replace is supported only when decoding to UTF-8 and
// substitutes U+FFFD for each invalid byte while preserving subsequent text.
bool Convert(const char* input, size_t size, Encoding from, Encoding to,
	std::string& output, InvalidInput invalid = InvalidInput::Reject);
inline bool Convert(std::string_view input, Encoding from, Encoding to,
	std::string& output, InvalidInput invalid = InvalidInput::Reject)
{
	return Convert(input.data(), input.size(), from, to, output, invalid);
}

// Display text as valid UTF-8: valid text (and empty text) as it is,
// malformed bytes each replaced by U+FFFD; empty when even that fails.
// TextService::NormalizeText caches it (task 4.241).
std::string NormalizeUtf8(const std::string& text);

// Set once before loading the resource pack. The shipped Korean data is CP949;
// converted or regional packs must declare their encoding explicitly.
Encoding GetResourceEncoding();
bool SetResourceEncoding(Encoding encoding);
bool SetResourceEncoding(std::string_view name);

} // namespace TextEncoding
