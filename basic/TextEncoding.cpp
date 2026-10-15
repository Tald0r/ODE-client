#include "TextEncoding.h"
#include "TextUtf8.h"

#include <atomic>
#include <cerrno>
#include <iconv.h>

namespace TextEncoding {
namespace {
std::atomic<Encoding> resourceEncoding{Encoding::Cp949};

class Converter {
public:
	Converter(const char* from, const char* to) : handle(iconv_open(to, from)) {}
	~Converter() { if (Valid()) iconv_close(handle); }
	bool Valid() const { return handle != reinterpret_cast<iconv_t>(-1); }
	Converter(const Converter&) = delete;
	Converter& operator=(const Converter&) = delete;
	iconv_t handle;
};
}

const char* Name(Encoding encoding)
{
	switch (encoding) {
	case Encoding::Utf8: return "UTF-8";
	case Encoding::Cp949: return "CP949";
	case Encoding::EucKr: return "EUC-KR";
	case Encoding::Gbk: return "GBK";
	case Encoding::Gb2312: return "GB2312";
	case Encoding::Big5: return "BIG5";
	}
	return nullptr;
}

bool Parse(std::string_view name, Encoding& encoding)
{
	for (const auto candidate : {Encoding::Utf8, Encoding::Cp949, Encoding::EucKr,
		Encoding::Gbk, Encoding::Gb2312, Encoding::Big5}) {
		const std::string_view canonical(Name(candidate));
		if (name.size() != canonical.size()) continue;
		bool matches = true;
		for (size_t i = 0; i < name.size(); ++i) {
			const char c = name[i] >= 'a' && name[i] <= 'z' ? char(name[i] - 'a' + 'A') : name[i];
			if (c != canonical[i]) { matches = false; break; }
		}
		if (matches) { encoding = candidate; return true; }
	}
	return false;
}

bool Convert(const char* input, size_t size, Encoding from, Encoding to,
	std::string& output, InvalidInput invalid)
{
	const auto* fromName = Name(from);
	const auto* toName = Name(to);
	// Check capacity before touching input, including the identity conversion.
	// Four output bytes per input byte covers all supported stateless codecs.
	if (!fromName || !toName || (!input && size != 0) ||
		size > (std::string().max_size() - 4) / 4 ||
		(invalid != InvalidInput::Reject && invalid != InvalidInput::Replace) ||
		(invalid == InvalidInput::Replace && to != Encoding::Utf8))
		return false;
	if (size == 0) { output.clear(); return true; }

	if (from == Encoding::Utf8) {
		if (invalid == InvalidInput::Reject && !TextSystem::IsValidUtf8(input, size))
			return false;
		if (to == Encoding::Utf8) {
			std::string converted;
			converted.reserve(size);
			for (size_t offset = 0; offset < size;) {
				const auto remaining = size - offset;
				int consumed = 0;
				bool valid = false;
				TextSystem::Utf8Decode(input + offset, int(remaining < 4 ? remaining : 4), &consumed, &valid);
				if (valid) converted.append(input + offset, consumed);
				else converted.append("\xEF\xBF\xBD");
				offset += consumed;
			}
			output.swap(converted);
			return true;
		}
	}

	std::string converted(size * 4 + 4, '\0');
	Converter converter(fromName, toName);
	if (!converter.Valid()) return false;
	char* in = const_cast<char*>(input);
	char* out = converted.data();
	size_t remaining = size;
	size_t capacity = converted.size();
	while (remaining != 0) {
		const size_t result = iconv(converter.handle, &in, &remaining, &out, &capacity);
		if (result == 0) break;
		if (result != static_cast<size_t>(-1) || invalid == InvalidInput::Reject ||
			(errno != EILSEQ && errno != EINVAL) || remaining == 0 || capacity < 3)
			return false;
		// All supported encodings are stateless: skipping a damaged byte cannot
		// leave an escape/shift mode active for the rest of the record.
		*out++ = '\xEF'; *out++ = '\xBF'; *out++ = '\xBD';
		capacity -= 3;
		++in;
		--remaining;
	}
	if (remaining != 0) return false;
	converted.resize(converted.size() - capacity);
	output.swap(converted);
	return true;
}

Encoding GetResourceEncoding() { return resourceEncoding.load(std::memory_order_relaxed); }

bool SetResourceEncoding(Encoding encoding)
{
	if (!Name(encoding)) return false;
	resourceEncoding.store(encoding, std::memory_order_relaxed);
	return true;
}

bool SetResourceEncoding(std::string_view name)
{
	Encoding encoding;
	return Parse(name, encoding) && SetResourceEncoding(encoding);
}

// Resources decode at their declared boundary; display text is UTF-8.
// Repair malformed bytes without guessing a different character set.
std::string NormalizeUtf8(const std::string& text)
{
	if (text.empty())
		return text;

	if (TextSystem::IsValidUtf8(text.c_str(), text.size()))
		return text;

	std::string converted;
	if (!Convert(text, Encoding::Utf8, Encoding::Utf8, converted, InvalidInput::Replace))
		return {};
	return converted;
}

} // namespace TextEncoding
