/********************************************************************
	created:	2003/12/01
	created:	1:12:2003   12:16
	filename: 	D:\study\smodulelib\CrashReport.h
	file path:	D:\study\smodulelib
	file base:	CrashReport
	file ext:	h
	author:		Ssukgat
	
	purpose:	logs a fatal error when one happens.
*********************************************************************/

#pragma once

#include "../basic/Platform.h"

#ifdef PLATFORM_WINDOWS
extern LONG __stdcall RecordExceptionInfo( _EXCEPTION_POINTERS* pExp );
#else
// Declaration for non-Windows platforms (implementation in CrashReport.cpp)
// Note: Must match LPTOP_LEVEL_EXCEPTION_FILTER signature: long (*)(struct _EXCEPTION_POINTERS*)
LONG __stdcall RecordExceptionInfo( struct _EXCEPTION_POINTERS* pExp );
#endif

// To change the log file names, change them here
#include <vector>

class MCrashReport;

// The name of an exception code: "an Access Violation" for 0xc0000005,
// "an Unknown exception type" for one it does not know.
const char*	GetExceptionDescription(DWORD exceptionCode);

// The crash written into a report: the executable's time, the client's
// version, the OS, the faulting address, the exception's name and code,
// and the call stack's addresses.
void	FillCrashReport(MCrashReport& report, const char* executableTime, int version, const char* os,
			unsigned long long address, DWORD exceptionCode, const std::vector<unsigned long long>& callStack);

// The client's version: the first four bytes of the file
// FILE_INFO_VERSION names, 0 if it cannot be read.
int		ReadClientVersion();

// GetWinVer.cpp's: the Windows version, "Windows 10/11" and the like
// ("Non-Windows Platform" elsewhere).
BOOL	GetWinVersion(char *szVersion, size_t nSize);

#define OLD_CRASH_LOG_FILENAME	"CrashReport.log"
#define CRASH_LOG_FILENAME		"CrashReport2.log"

#define InitCrashReport()		SetUnhandledExceptionFilter( RecordExceptionInfo )
