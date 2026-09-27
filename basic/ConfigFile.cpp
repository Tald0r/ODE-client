#include "ConfigFile.h"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef PLATFORM_WINDOWS
#include <io.h>
#include <share.h>
#else
#include <unistd.h>
#endif

namespace ConfigFile {
namespace {
constexpr size_t MaxFileBytes = 1024 * 1024;

bool ValidName(const char* name)
{
	return name && std::strpbrk(name, "\r\n=") == nullptr;
}

bool ReadFile(const char* filename, std::string& contents, bool allowMissing)
{
	std::error_code error;
	const bool exists = std::filesystem::exists(filename, error);
	if (error) return false;
	if (!exists) return allowMissing;
	std::ifstream in(filename, std::ios::binary | std::ios::ate);
	if (!in) return false;
	const auto length = in.tellg();
	if (length < 0 || length > static_cast<std::streamoff>(MaxFileBytes)) return false;
	contents.resize(static_cast<size_t>(length));
	in.seekg(0);
	if (!in.read(contents.data(), static_cast<std::streamsize>(contents.size()))) return false;
	return contents.find('\0') == std::string::npos;
}

struct Line {
	std::string_view record;
	std::string_view ending;
	size_t next;
};

Line ReadLine(const std::string& contents, size_t offset)
{
	const auto newline = contents.find('\n', offset);
	const size_t end = newline == std::string::npos ? contents.size() : newline;
	size_t textEnd = end;
	if (textEnd > offset && contents[textEnd - 1] == '\r') --textEnd;
	const size_t next = newline == std::string::npos ? end : end + 1;
	return {std::string_view(contents).substr(offset, textEnd - offset),
		std::string_view(contents).substr(textEnd, next - textEnd), next};
}

struct PendingFile {
	std::filesystem::path path;
	FILE* file;
	~PendingFile()
	{
		if (file) std::fclose(file);
		std::error_code ignored;
		std::filesystem::remove(path, ignored);
	}
};

bool CommitFile(const char* filename, const std::string& contents)
{
	static std::atomic<unsigned long long> sequence{0};
	const std::filesystem::path target(filename);
#ifdef PLATFORM_WINDOWS
	const auto process = GetCurrentProcessId();
#else
	const auto process = getpid();
#endif
	for (int attempt = 0; attempt < 100; ++attempt) {
		auto temporary = target;
		temporary += ".tmp." + std::to_string(process) + "." + std::to_string(++sequence);
#ifdef PLATFORM_WINDOWS
		int descriptor = -1;
		_wsopen_s(&descriptor, temporary.c_str(), _O_WRONLY | _O_BINARY | _O_CREAT | _O_EXCL, _SH_DENYNO, _S_IREAD | _S_IWRITE);
#else
		const int descriptor = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
#endif
		if (descriptor < 0) {
			if (errno == EEXIST) continue;
			return false;
		}
		// Only an exclusively created path is owned by this cleanup guard.
		PendingFile pending{std::move(temporary), nullptr};
#ifdef PLATFORM_WINDOWS
		pending.file = _fdopen(descriptor, "wb");
		if (!pending.file) { _close(descriptor); return false; }
#else
		pending.file = fdopen(descriptor, "wb");
		if (!pending.file) { close(descriptor); return false; }
#endif
		if (std::fwrite(contents.data(), 1, contents.size(), pending.file) != contents.size()) return false;
		if (std::fclose(std::exchange(pending.file, nullptr)) != 0) return false;
#ifdef PLATFORM_WINDOWS
		return MoveFileExW(pending.path.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
		std::error_code error;
		std::filesystem::rename(pending.path, target, error);
		return !error;
#endif
	}
	return false;
}
} // namespace

int GetString(const char* filename, const char* key, const char* value, char* buffer, DWORD* size)
{
	if (!filename || !*filename || !size || !ValidName(key) || !ValidName(value)) return 1;
	std::string contents;
	if (!ReadFile(filename, contents, false)) return 1;
	const std::string prefix = std::string(key) + "." + value + "=";
	for (size_t offset = 0; offset < contents.size();) {
		const auto line = ReadLine(contents, offset);
		offset = line.next;
		if (!line.record.starts_with(prefix)) continue;
		const auto text = line.record.substr(prefix.size());
		const size_t capacity = *size;
		*size = static_cast<DWORD>(text.size() + 1);
		if (!buffer || text.size() + 1 > capacity) return 1;
		std::memcpy(buffer, text.data(), text.size());
		buffer[text.size()] = '\0';
		return 0;
	}
	return 1;
}

int SetString(const char* filename, const char* key, const char* value, const char* data)
{
	if (!filename || !*filename || !ValidName(key) || !ValidName(value) || !data || std::strpbrk(data, "\r\n")) return 1;
	std::string contents;
	if (!ReadFile(filename, contents, true)) return 1;
	const std::string prefix = std::string(key) + "." + value + "=";
	const size_t dataLength = std::strlen(data);
	if (prefix.size() > MaxFileBytes || dataLength > MaxFileBytes - prefix.size()) return 1;
	std::string updated;
	bool replaced = false;
	for (size_t offset = 0; offset < contents.size();) {
		const auto line = ReadLine(contents, offset);
		if (line.record.starts_with(prefix)) {
			if (!replaced) {
				updated += prefix;
				updated += data;
				updated += line.ending;
				replaced = true;
			}
		} else {
			updated.append(contents, offset, line.next - offset);
		}
		offset = line.next;
		if (updated.size() > MaxFileBytes) return 1;
	}
	if (!replaced) {
		if (!updated.empty() && updated.back() != '\n') updated += '\n';
		updated += prefix;
		updated += data;
		updated += '\n';
	}
	if (updated.size() > MaxFileBytes) return 1;
	return CommitFile(filename, updated) ? 0 : 1;
}
} // namespace ConfigFile
