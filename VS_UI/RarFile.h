// RarFile.h: interface for the CRarFile class.
// Loose resource overrides and bounded in-memory RAR/RPK reading.
//////////////////////////////////////////////////////////////////////

#ifndef _RAR_FILE_HEADER_
#define _RAR_FILE_HEADER_

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#else
#include "Platform.h"
#endif
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * CRarFile - Cross-platform loose and packed resource reader.
 *
 * Loose files beside the named archive override its members. Otherwise the
 * archive is read into bounded memory using its password, without extracting
 * anything to disk. Raw opens retain bytes; text opens decode once to UTF-8.
 */

class CRarFile
{
private:
	std::string m_rar_filename;
	std::string m_password;
	std::string m_base_dir;      // Extracted directory path

	char *m_data;
	char *m_file_pointer;
	int m_size;
	bool m_text = false;
	bool OpenLimited(const char* filename, bool text);

public:
	// Constructor
	CRarFile();
	CRarFile(const char *rar_filename, const char *pass);

	// Destructor
	~CRarFile();
	CRarFile(const CRarFile&) = delete;
	CRarFile& operator=(const CRarFile&) = delete;

	// Release resources
	void Release();

	// Set archive path and the adjacent directory for loose overrides.
	void SetRAR(const char *rar_filename, const char *pass);

	// Open raw bytes from the loose directory or archive. Failure closes old data.
	bool Open(const char *in_filename);
	// Decode once using the resource page or UTF-8 BOM, with a 16 MiB input cap.
	// GetString then clips only at UTF-8 scalar boundaries, consuming each line.
	bool OpenText(const char *in_filename);

	// Exact reads reject negative/oversized requests without changing the cursor
	// or destination. The caller supplies at least size writable destination bytes.
	char*	Read(char *buf, int size);
	char*	Read(int size);
	bool	GetString(char* buf, int size);

	// Check if file is ready
	bool	IsSet()	{ return (m_data != NULL); }

	// Check if EOF
	bool	IsEOF(int plus = 0);

	// Caller owns regular archive-member names, even when empty. Optional glob
	// filter uses '*' and '?', folded separators, and ASCII case-insensitivity.
	std::vector<std::string> *GetList(char *filter = NULL);

	char* GetFilePointer(){return m_file_pointer;};
	size_t GetRemainingSize() const {
		return m_file_pointer ? static_cast<size_t>(m_size - (m_file_pointer - m_data)) : 0;
	}
};

#endif
