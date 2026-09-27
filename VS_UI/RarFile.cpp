// RarFile.cpp: implementation of the CRarFile class.
// Loose resource overrides and bounded in-memory RAR/RPK reading.
//////////////////////////////////////////////////////////////////////

#include "RarFile.h"
#include "CrtCompat.h"
#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif
#include <algorithm>
#include <limits>
#include <memory>
#include "ResourceText.h"
#include "TextUtf8.h"
#include "RarArchive.h"
#include "DataPath.h"

//////////////////////////////////////////////////////////////////////
// Error Reporting Macro (cross-platform)
//////////////////////////////////////////////////////////////////////
#ifdef PLATFORM_WINDOWS
	#define RARFILE_ERROR(msg) { \
		OutputDebugStringA("[RARFile ERROR] "); \
		OutputDebugStringA(msg); \
		OutputDebugStringA("\n"); \
	}
#else
	#define RARFILE_ERROR(msg) { \
		fprintf(stderr, "[RARFile ERROR] %s\n", msg); \
		fflush(stderr); \
	}
#endif

//////////////////////////////////////////////////////////////////////
// Construction
//////////////////////////////////////////////////////////////////////
CRarFile::CRarFile()
{
	m_file_pointer = NULL;
	m_data = NULL;
	m_size = 0;
	m_text = false;
}

//////////////////////////////////////////////////////////////////////
// Construction
//////////////////////////////////////////////////////////////////////
CRarFile::CRarFile(const char *rar_filename, const char *pass)
{
	m_file_pointer = NULL;
	m_data = NULL;
	m_size = 0;
	SetRAR(rar_filename, pass);
}

//////////////////////////////////////////////////////////////////////
// Destruction
//////////////////////////////////////////////////////////////////////
CRarFile::~CRarFile()
{
	Release();
}

//////////////////////////////////////////////////////////////////////
// Release
//////////////////////////////////////////////////////////////////////
void CRarFile::Release()
{
	if(m_data != NULL){
		free(m_data);
		m_data = NULL;
		m_file_pointer = NULL;
	}
	m_size = 0;
	m_text = false;
}

//////////////////////////////////////////////////////////////////////
// SetRAR
// Select an archive and its adjacent loose-resource directory.
//////////////////////////////////////////////////////////////////////
void CRarFile::SetRAR(const char *rar_filename, const char *pass)
{
	Release();
	if (rar_filename == NULL || rar_filename[0] == '\0')
	{
		m_rar_filename = "";
		m_base_dir = "";
		m_password = "";
		return;
	}

	m_rar_filename = rar_filename;
	m_password = pass ? pass : "";

	// Loose resource overrides live beside the .rpk/.rar, not in a subfolder
	// named after it. This also supports existing fully extracted data trees.
	// Example: "Data/Info/infodata.rpk" -> "Data/Info/"
	std::string path = rar_filename;
	size_t lastSlash = path.find_last_of("/\\");
	if (lastSlash != std::string::npos) {
		m_base_dir = path.substr(0, lastSlash + 1);
	} else {
		m_base_dir = "";
	}
}

//////////////////////////////////////////////////////////////////////
// Open
// Open a loose resource override, or its packed member when no override exists.
//////////////////////////////////////////////////////////////////////
bool CRarFile::Open(const char *in_filename)
{
	return OpenLimited(in_filename, false);
}

bool CRarFile::OpenText(const char* filename)
{
	return OpenLimited(filename, true);
}

bool CRarFile::OpenLimited(const char* in_filename, bool text)
{
	Release();
	if (in_filename == NULL || in_filename[0] == '\0')
	{
		RARFILE_ERROR("Open called with NULL or empty filename");
		return false;
	}

	// Build full path by combining base directory with filename
	std::string fullPath = Basic::NormalizeDataPath(m_base_dir + in_filename);
	const size_t limit = text ? ResourceText::MaxFileBytes :
		static_cast<size_t>((std::numeric_limits<int>::max)() - 1);
	std::unique_ptr<char, decltype(&free)> data(nullptr, &free);
	size_t bytesRead = 0;

	// Open the file
	const auto closeFile = [](FILE* value) { fclose(value); };
	std::unique_ptr<FILE, decltype(closeFile)> file(Basic::OpenFile(fullPath.c_str(), "rb"), closeFile);
	if (!file)
	{
		std::string packed;
		if (!RarArchive::Read(m_rar_filename, m_password, in_filename, limit, packed)) {
			char errorMsg[512];
			snprintf(errorMsg, sizeof(errorMsg), "Cannot read resource %s (archive=%s)",
				fullPath.c_str(), m_rar_filename.c_str());
			RARFILE_ERROR(errorMsg);
			return false;
		}
		data.reset(static_cast<char*>(malloc(packed.size() + 1)));
		if (!data) return false;
		bytesRead = packed.size();
		memcpy(data.get(), packed.data(), bytesRead);
	}
	else {
		if (fseek(file.get(), 0, SEEK_END) != 0) return false;
		const long fileSize = ftell(file.get());
		if (fileSize < 0 || static_cast<size_t>(fileSize) > limit ||
			fseek(file.get(), 0, SEEK_SET) != 0) return false;
		data.reset(static_cast<char*>(malloc(static_cast<size_t>(fileSize) + 1)));
		if (!data) return false;
		bytesRead = fread(data.get(), 1, static_cast<size_t>(fileSize), file.get());
		if (bytesRead != static_cast<size_t>(fileSize)) return false;
	}

	if (text) {
		std::string decoded;
		if (!ResourceText::Decode(std::string_view(data.get(), bytesRead), decoded) ||
			decoded.size() > static_cast<size_t>((std::numeric_limits<int>::max)() - 1)) return false;
		std::unique_ptr<char, decltype(&free)> utf8(
			static_cast<char*>(malloc(decoded.size() + 1)), &free);
		if (!utf8) return false;
		memcpy(utf8.get(), decoded.data(), decoded.size());
		bytesRead = decoded.size();
		data = std::move(utf8);
	}
	m_data = data.release();
	m_size = (int)bytesRead;
	m_data[m_size] = '\0';  // Null-terminate for string operations
	m_file_pointer = m_data;
	m_text = text;

	return true;
}

//////////////////////////////////////////////////////////////////////
// Read
// Copy data to buffer
//////////////////////////////////////////////////////////////////////
char* CRarFile::Read(char *buf, int size)
{
	if (!buf || size < 0 || m_file_pointer == NULL || IsEOF() ||
		size > m_size - (m_file_pointer - m_data))
		return NULL;

	memcpy(buf, m_file_pointer, size);
	char* re = m_data;
	m_file_pointer += size;
	return re;
}

//////////////////////////////////////////////////////////////////////
// Read
// Advance file pointer by size
//////////////////////////////////////////////////////////////////////
char* CRarFile::Read(int size)
{
	if (size < 0 || m_file_pointer == NULL || IsEOF() ||
		size > m_size - (m_file_pointer - m_data))
		return NULL;

	char* re = (char*)m_file_pointer;
	m_file_pointer += size;
	return re;
}

//////////////////////////////////////////////////////////////////////
// GetString
// Read one line from the current file pointer position
//////////////////////////////////////////////////////////////////////
bool CRarFile::GetString(char* buf, int size)
{
	if (buf == NULL || size <= 0)
		return false;

	if (m_file_pointer == NULL || IsEOF())
	{
		buf[0] = '\0';
		return false;
	}

	// Find current position in data
	const auto currentPos = m_file_pointer - m_data;
	if (currentPos >= m_size)
	{
		buf[0] = '\0';
		return false;
	}

	// Find newline character
	char* lineStart = m_file_pointer;
	char* newline = (char*)memchr(lineStart, '\n', m_size - currentPos);

	int lineLength;
	if (newline != NULL)
	{
		// Found newline - calculate line length
		lineLength = (int)(newline - lineStart);

		// Skip the newline for next call
		m_file_pointer = newline + 1;
	}
	else
	{
		// No newline found - read to end
		lineLength = (int)(m_size - currentPos);
		m_file_pointer = m_data + m_size;
	}

	// Copy line to buffer (respect buffer size)
	int copyLength = lineLength;
	if (copyLength >= size)
		copyLength = size - 1;
	if (m_text) {
		copyLength = static_cast<int>(TextSystem::Utf8PrefixBytes(
			std::string_view(lineStart, static_cast<size_t>(lineLength)),
			static_cast<size_t>(copyLength)));
	}

	memcpy(buf, lineStart, copyLength);
	buf[copyLength] = '\0';

	// Remove trailing \r if present (Windows CRLF files)
	if (copyLength > 0 && buf[copyLength - 1] == '\r')
	{
		buf[copyLength - 1] = '\0';
	}

	return true;
}

//////////////////////////////////////////////////////////////////////
// IsEOF
//////////////////////////////////////////////////////////////////////
bool CRarFile::IsEOF(int plus)
{
	if (m_file_pointer == NULL || plus < 0)
		return true;

	return plus >= m_size - (m_file_pointer - m_data);
}

//////////////////////////////////////////////////////////////////////
// GetList
// Enumerate regular members of this archive, preserving caller ownership.
//////////////////////////////////////////////////////////////////////
std::vector<std::string> *CRarFile::GetList(char *filter)
{
	auto list = std::make_unique<std::vector<std::string>>();
	RarArchive::List(m_rar_filename, m_password, filter ? filter : "", *list);
	return list.release();
}
