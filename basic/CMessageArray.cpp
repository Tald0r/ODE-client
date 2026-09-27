//----------------------------------------------------------------------
// CMessageArray.cpp
//----------------------------------------------------------------------
#include "Platform.h"
#include "CMessageArray.h"
#include "DataPath.h"
#include <sys/stat.h>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#ifdef OUTPUT_DEBUG
	//#define OUTPUT_FILE_LOG
#endif

#define __LOGGING__

// Platform-specific includes
#ifdef PLATFORM_WINDOWS
	#include <io.h>
	#include <share.h>
	#include <fcntl.h>
#else
	#include <unistd.h>
	#include <fcntl.h>
#endif

// Platform-specific I/O functions
#ifdef PLATFORM_WINDOWS
	#define PLATFORM_WRITE(fd, buf, len)	_write(fd, buf, static_cast<unsigned int>(len))
	// _open with its sharing mode, through the call that is not deprecated.
	static int PLATFORM_OPEN(const char* pName, int flags, int mode) { int fd = -1; _sopen_s(&fd, pName, flags, _SH_DENYNO, mode); return fd; }
	#define PLATFORM_CLOSE	_close
	#define PLATFORM_LSEEK	_lseek
	static constexpr int LogFileMode = _S_IREAD | _S_IWRITE;
#else
	#define PLATFORM_WRITE(fd, buf, len)	write(fd, buf, len)
	#define PLATFORM_OPEN	open
	#define PLATFORM_CLOSE	close
	#define PLATFORM_LSEEK	lseek
	static constexpr int LogFileMode = S_IRUSR | S_IWUSR;
#endif

// Platform-specific file flags
#ifndef PLATFORM_WINDOWS
	#define _O_WRONLY    O_WRONLY
	#define _O_TEXT      0
	#define _O_APPEND    O_APPEND
	#define _O_CREAT     O_CREAT
	#define _O_TRUNC     O_TRUNC
#endif

#ifdef OUTPUT_DEBUG
	CRITICAL_SECTION			g_Lock;

	#define __BEGIN_LOCK	EnterCriticalSection(&g_Lock);
	#define __END_LOCK		LeaveCriticalSection(&g_Lock);
#else
	#define __BEGIN_LOCK	((void)0);
	#define __END_LOCK		((void)0);
#endif


//----------------------------------------------------------------------
//
// constructor/destructor
//
//----------------------------------------------------------------------
CMessageArray::CMessageArray()
{
	m_Max		= 0;
	m_Length	= 0;
	m_Current	= 0;
	m_ppMessage = NULL;

	m_bLog		= false;
	m_LogFile	= -1;
	m_Filename	= NULL;
}

CMessageArray::~CMessageArray()
{
	Release();
}


//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Init
//----------------------------------------------------------------------
void
CMessageArray::Init(int max, int length, const char* filename)
{
	// Allocate under owners before replacing the previous ring. A failed
	// allocation leaves it intact, and a filename may alias GetFilename().
	if (max <= 0 || length < 0) {
		Release();
		return;
	}
	std::vector<std::unique_ptr<char[]>> rows(static_cast<size_t>(max));
	auto table = std::make_unique<char*[]>(static_cast<size_t>(max));
	for (int i = 0; i < max; ++i) {
		rows[i] = std::make_unique<char[]>(static_cast<size_t>(length) + 1);
		table[i] = rows[i].get();
	}
	std::unique_ptr<char[]> name;
	if (filename) {
		// The executable names its log "<cwd>\Log\Log<n>.txt" (Client.cpp).
		// Off Windows the name is resolved here, where the file is opened, or
		// the log would be one file beside the directory with backslashes in
		// its name; on Windows it is kept as given (basic/DataPath.h).
		const std::string resolved = Basic::NormalizeDataPath(filename);
		const size_t bytes = resolved.size() + 1;
		name = std::make_unique<char[]>(bytes);
		memcpy(name.get(), resolved.c_str(), bytes);
	}

	Release();
	m_Max = max;
	m_Length = length;
	m_ppMessage = table.release();
	for (auto& row : rows) row.release();
	m_Filename = name.release();
	if (m_Filename) {
		m_LogFile = PLATFORM_OPEN(m_Filename, _O_WRONLY | _O_TEXT | _O_CREAT | _O_TRUNC, LogFileMode);
		m_bLog = m_LogFile != -1;
	}
}

//----------------------------------------------------------------------
// Release
//----------------------------------------------------------------------
void
CMessageArray::Release()
{
	if (m_ppMessage) {
		for (int i = 0; i < m_Max; ++i) delete[] m_ppMessage[i];
		delete[] m_ppMessage;
	}
	m_ppMessage = nullptr;
	m_Max = m_Length = m_Current = 0;

	if (m_bLog) PLATFORM_CLOSE(m_LogFile);
	m_bLog = false;
	m_LogFile = -1;
	// A failed open still owns the copied filename.
	delete[] m_Filename;
	m_Filename = nullptr;
}

//----------------------------------------------------------------------
// Add 
//----------------------------------------------------------------------
// String을 추가한다. 끝에~..
//----------------------------------------------------------------------
void		
CMessageArray::Add(const char *str)
{
	if (!str || !m_ppMessage) return;
	__BEGIN_LOCK
	StoreRow(str, strlen(str));
	__END_LOCK
}

//----------------------------------------------------------------------
// Add To File
//----------------------------------------------------------------------
// File에만 추가한다. 끝에~..
//----------------------------------------------------------------------
void		
CMessageArray::AddToFile(const char *str)
{
	if (!str) return;
	#ifndef __LOGGING__
		return;
	#endif

	__BEGIN_LOCK
	
	// file log
	if (m_bLog)
	{
		// [ TEST CODE ] 시간 출력
		//sprintf(g_MessageBuffer, "[%4d] ", timeGetTime() % 10000);
		//PLATFORM_WRITE( m_LogFile, g_MessageBuffer, strlen(g_MessageBuffer) );

		//m_LogFile << str << endl;
		PLATFORM_WRITE( m_LogFile, str, strlen( str ) );
		PLATFORM_WRITE( m_LogFile, "\n", 1 );

		// [ TEST CODE ] 화일 닫고 다시 열기
		#ifdef OUTPUT_FILE_LOG
			PLATFORM_CLOSE( m_LogFile );
			m_LogFile = PLATFORM_OPEN(m_Filename, _O_WRONLY | _O_TEXT | _O_APPEND | _O_CREAT, LogFileMode);
		#endif
	}	

	__END_LOCK
}

//--------------------------------------------------------------------------
// Store Row
//--------------------------------------------------------------------------
// The public entry points hold the lock across formatting and storage.
// Store the full message in the log, then bound the visible ring row.
//--------------------------------------------------------------------------
void
CMessageArray::StoreRow(const char* pBuffer, size_t nLength)
{
	if (!m_ppMessage || !pBuffer) return;
	if (m_bLog) {
		PLATFORM_WRITE(m_LogFile, pBuffer, nLength);
		PLATFORM_WRITE(m_LogFile, "\n", 1);
		#ifdef OUTPUT_FILE_LOG
			PLATFORM_CLOSE(m_LogFile);
			m_LogFile = PLATFORM_OPEN(m_Filename, _O_WRONLY | _O_TEXT | _O_APPEND | _O_CREAT, LogFileMode);
			m_bLog = m_LogFile != -1;
			m_bLog = m_LogFile != -1;
		#endif
	}
	const size_t count = (std::min)(nLength, static_cast<size_t>(m_Length));
	// memmove also permits Add(GetCurrent()) and other ring-backed text.
	memmove(m_ppMessage[m_Current], pBuffer, count);
	m_ppMessage[m_Current][count] = '\0';
	if (++m_Current == m_Max) m_Current = 0;
}

// Typed formatting shares the same row and log storage as Add.
void
CMessageArray::AddSafeFormatV(const char* format,
							  const SafeFormat::Arg* pArgs, size_t nCount)
{
	#ifndef __LOGGING__
		return;
	#endif

	__BEGIN_LOCK

	char	Buffer[4096];

	const int len = SafeFormat::FormatV(Buffer, sizeof(Buffer), format, pArgs, nCount);

	StoreRow(Buffer, len);

	__END_LOCK
}

//----------------------------------------------------------------------
// Next
//----------------------------------------------------------------------
// Current를 next로 바꾼다..
//----------------------------------------------------------------------
void
CMessageArray::Next()
{	
	if (!m_ppMessage) return;
	#ifndef __LOGGING__
		return;
	#endif

	__BEGIN_LOCK

	// file log
	if (m_bLog)
	{
		//m_LogFile << str << endl;
		PLATFORM_WRITE( m_LogFile, m_ppMessage[m_Current], strlen( m_ppMessage[m_Current] ) );
		PLATFORM_WRITE( m_LogFile, "\n", 1 );
	}	

	m_Current++; 
	if (m_Current==m_Max) m_Current=0;

	__END_LOCK
}

//----------------------------------------------------------------------
// operator []
//----------------------------------------------------------------------
// 0 ~ MAX-1
// 0이 가장 오래된 String이고 MAX-1이 가장 최근에 것으로
// return해야 한다.
//----------------------------------------------------------------------
const char*	
CMessageArray::operator [] (int i)
{
	if (!m_ppMessage || i < 0 || i >= m_Max) return "";
	__BEGIN_LOCK
	// Index zero is the oldest slot. Keep both branches below m_Max
	// without truncating to BYTE or overflowing m_Current + i.
	const int untilWrap = m_Max - m_Current;
	const int index = i < untilWrap ? m_Current + i : i - untilWrap;
	const char* message = m_ppMessage[index];
	__END_LOCK
	return message;
}

//----------------------------------------------------------------------
// Clear
//----------------------------------------------------------------------
void
CMessageArray::Clear()
{
	if (!m_ppMessage) return;
	for (int i=0; i<m_Max; i++)
	{
		m_ppMessage[i][0] = '\0';
	}
}
