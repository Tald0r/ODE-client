//----------------------------------------------------------------------
// CrtCompat.h
//----------------------------------------------------------------------
//
// Portable spellings of the C runtime calls MSVC deprecates (C4996) in
// favour of its _s variants. Each helper does what the call it replaces
// does, except where its comment says otherwise (the scanners below on
// Windows, LocalTime on failure); on Windows it goes through the Windows
// CRT's equivalent that is not deprecated (MSVC, clang-cl and MinGW-w64
// all declare it), elsewhere through the standard or POSIX call itself.
//
// ScanString/ScanFile take the scanf formats the code already uses. The
// Windows _s scanners need the capacity of every %s, %c and %[ destination
// as an extra argument; pass such a buffer as CRT_BUFFER(buffer), which
// appends the array's length on Windows and nothing elsewhere. On valid
// input the result is the same; a word longer than the buffer overflowed
// it before and now fails that conversion on Windows instead.
//
// CRT_BUFFER takes a writable char array only: a pointer has no capacity
// to pass, and sizeof would give the pointer's size, so it fails to
// compile on every platform (static_asserts in tests/unit/test_crt_compat.cpp
// hold that). Nothing catches a %s buffer passed without CRT_BUFFER; on
// Windows that reads the next argument as the capacity.
//
//----------------------------------------------------------------------

#ifndef __CRTCOMPAT_H__
#define __CRTCOMPAT_H__

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <optional>
#include <string>

namespace Basic
{
	// CRT_BUFFER's halves. Both accept only a char array, so CRT_BUFFER
	// rejects a pointer at compile time on every platform. CrtArray
	// returns the array itself, which decays to the same pointer the bare
	// name did; CrtCapacity is its length, which for char is its sizeof.
	template<std::size_t N>
	constexpr char (&CrtArray(char (&buffer)[N]) noexcept)[N]
	{
		return buffer;
	}

	template<std::size_t N>
	constexpr unsigned CrtCapacity(char (&)[N]) noexcept
	{
		return static_cast<unsigned>(N);
	}
}

#ifdef _WIN32
	#include <share.h>
	#define CRT_BUFFER(buffer) (buffer), Basic::CrtCapacity(buffer)
	#define CRT_SCANF_FORMAT
#else
	#define CRT_BUFFER(buffer) (Basic::CrtArray(buffer))
	#define CRT_SCANF_FORMAT __attribute__((format(scanf, 2, 3)))
#endif

namespace Basic
{
	// fopen. On Windows fopen opens with _SH_DENYNO; _fsopen with that mode
	// is the same call without the deprecation.
	inline std::FILE* OpenFile(const char* pPath, const char* pMode)
	{
#ifdef _WIN32
		return _fsopen(pPath, pMode, _SH_DENYNO);
#else
		return std::fopen(pPath, pMode);
#endif
	}

	// sscanf.
	inline int ScanString(const char* pText, const char* pFormat, ...) CRT_SCANF_FORMAT;
	inline int ScanString(const char* pText, const char* pFormat, ...)
	{
		va_list args;
		va_start(args, pFormat);
#ifdef _WIN32
		const int result = vsscanf_s(pText, pFormat, args);
#else
		const int result = std::vsscanf(pText, pFormat, args);
#endif
		va_end(args);
		return result;
	}

	// fscanf.
	inline int ScanFile(std::FILE* pFile, const char* pFormat, ...) CRT_SCANF_FORMAT;
	inline int ScanFile(std::FILE* pFile, const char* pFormat, ...)
	{
		va_list args;
		va_start(args, pFormat);
#ifdef _WIN32
		const int result = vfscanf_s(pFile, pFormat, args);
#else
		const int result = std::vfscanf(pFile, pFormat, args);
#endif
		va_end(args);
		return result;
	}

	// localtime, into caller storage instead of the runtime's shared buffer.
	// False where localtime would have returned NULL, and *pResult is then
	// zeroed: localtime_s sets every field to -1 on failure and
	// localtime_r leaves them unspecified.
	inline bool LocalTime(const std::time_t* pTime, std::tm* pResult)
	{
#ifdef _WIN32
		const bool converted = localtime_s(pResult, pTime) == 0;
#else
		const bool converted = localtime_r(pTime, pResult) != nullptr;
#endif
		if (!converted)
			*pResult = std::tm{};
		return converted;
	}

	// strtok with the position kept in *ppContext rather than in the
	// runtime: pass the string on the first call and NULL after, with the
	// same context each time.
	inline char* Tokenize(char* pText, const char* pDelimiters, char** ppContext)
	{
#ifdef _WIN32
		return strtok_s(pText, pDelimiters, ppContext);
#else
		return strtok_r(pText, pDelimiters, ppContext);
#endif
	}

	// strdup; free the result with free().
	inline char* DuplicateString(const char* pText)
	{
#ifdef _WIN32
		return _strdup(pText);
#else
		return strdup(pText);
#endif
	}

	// strncpy, exactly: copies at most count characters, stops at the
	// source's terminator and fills the rest of the count with NULs, and -
	// like strncpy - leaves a copy that fills the count unterminated.
	inline char* CopyBounded(char* pDest, const char* pSource, std::size_t count)
	{
		std::size_t length = 0;
		while (length < count && pSource[length] != '\0')
			++length;
		std::memcpy(pDest, pSource, length);
		std::memset(pDest + length, 0, count - length);
		return pDest;
	}

	// ctime's text ("Www Mmm dd hh:mm:ss yyyy\n"), empty where ctime would
	// have returned NULL.
	inline std::string TimeText(std::time_t time)
	{
		char buffer[26] = {};
#ifdef _WIN32
		if (ctime_s(buffer, sizeof(buffer), &time) != 0)
			return std::string();
#else
		if (ctime_r(&time, buffer) == nullptr)
			return std::string();
#endif
		return std::string(buffer);
	}

	// getenv; nullopt where the variable is not set.
	inline std::optional<std::string> GetEnvironment(const char* pName)
	{
#ifdef _WIN32
		char* pValue = nullptr;
		std::size_t length = 0;
		if (_dupenv_s(&pValue, &length, pName) != 0 || pValue == nullptr)
			return std::nullopt;
		std::string value(pValue);
		std::free(pValue);
		return value;
#else
		const char* pValue = std::getenv(pName);
		return pValue ? std::optional<std::string>(pValue) : std::nullopt;
#endif
	}

	// tmpfile.
	inline std::FILE* TemporaryFile()
	{
#ifdef _WIN32
		std::FILE* pFile = nullptr;
		return tmpfile_s(&pFile) == 0 ? pFile : nullptr;
#else
		return std::tmpfile();
#endif
	}
}

#endif
