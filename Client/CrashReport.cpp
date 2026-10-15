/********************************************************************
	created:	2003/12/01
	created:	1:12:2003   12:15
	filename: 	D:\study\smodulelib\CrashReport.cpp
	file path:	D:\study\smodulelib
	file base:	CrashReport
	file ext:	cpp
	author:		Ssukgat
	
	purpose:	logs a fatal error when one happens.
*********************************************************************/
#include "Client_PCH.h"
#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#include <imagehlp.h>
#else
#include "../basic/Platform.h"
#endif
#include "CrashReport.h"
#include "Properties.h"
#include "MCrashReportManager.h"
#include "GameSocket.h"
#include "ClientPlayer.h"
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

///////////////////////////////////////////////////////////////////////////////
// GetExceptionDescription
//
// Translate the exception code into something human readable
const char *GetExceptionDescription(DWORD ExceptionCode)
{
	struct ExceptionNames
	{
		DWORD	ExceptionCode;
		const char *	ExceptionName;
	};

#if 0  // from winnt.h
#define STATUS_WAIT_0                    ((DWORD   )0x00000000L)    
#define STATUS_ABANDONED_WAIT_0          ((DWORD   )0x00000080L)    
#define STATUS_USER_APC                  ((DWORD   )0x000000C0L)    
#define STATUS_TIMEOUT                   ((DWORD   )0x00000102L)    
#define STATUS_PENDING                   ((DWORD   )0x00000103L)    
#define STATUS_SEGMENT_NOTIFICATION      ((DWORD   )0x40000005L)    
#define STATUS_GUARD_PAGE_VIOLATION      ((DWORD   )0x80000001L)    
#define STATUS_DATATYPE_MISALIGNMENT     ((DWORD   )0x80000002L)    
#define STATUS_BREAKPOINT                ((DWORD   )0x80000003L)    
#define STATUS_SINGLE_STEP               ((DWORD   )0x80000004L)    
#define STATUS_ACCESS_VIOLATION          ((DWORD   )0xC0000005L)    
#define STATUS_IN_PAGE_ERROR             ((DWORD   )0xC0000006L)    
#define STATUS_INVALID_HANDLE            ((DWORD   )0xC0000008L)    
#define STATUS_NO_MEMORY                 ((DWORD   )0xC0000017L)    
#define STATUS_ILLEGAL_INSTRUCTION       ((DWORD   )0xC000001DL)    
#define STATUS_NONCONTINUABLE_EXCEPTION  ((DWORD   )0xC0000025L)    
#define STATUS_INVALID_DISPOSITION       ((DWORD   )0xC0000026L)    
#define STATUS_ARRAY_BOUNDS_EXCEEDED     ((DWORD   )0xC000008CL)    
#define STATUS_FLOAT_DENORMAL_OPERAND    ((DWORD   )0xC000008DL)    
#define STATUS_FLOAT_DIVIDE_BY_ZERO      ((DWORD   )0xC000008EL)    
#define STATUS_FLOAT_INEXACT_RESULT      ((DWORD   )0xC000008FL)    
#define STATUS_FLOAT_INVALID_OPERATION   ((DWORD   )0xC0000090L)    
#define STATUS_FLOAT_OVERFLOW            ((DWORD   )0xC0000091L)    
#define STATUS_FLOAT_STACK_CHECK         ((DWORD   )0xC0000092L)    
#define STATUS_FLOAT_UNDERFLOW           ((DWORD   )0xC0000093L)    
#define STATUS_INTEGER_DIVIDE_BY_ZERO    ((DWORD   )0xC0000094L)    
#define STATUS_INTEGER_OVERFLOW          ((DWORD   )0xC0000095L)    
#define STATUS_PRIVILEGED_INSTRUCTION    ((DWORD   )0xC0000096L)    
#define STATUS_STACK_OVERFLOW            ((DWORD   )0xC00000FDL)    
#define STATUS_CONTROL_C_EXIT            ((DWORD   )0xC000013AL)    
#define STATUS_FLOAT_MULTIPLE_FAULTS     ((DWORD   )0xC00002B4L)    
#define STATUS_FLOAT_MULTIPLE_TRAPS      ((DWORD   )0xC00002B5L)    
#define STATUS_ILLEGAL_VLM_REFERENCE     ((DWORD   )0xC00002C0L)     
#endif

	ExceptionNames ExceptionMap[] =
	{
		{0x40010005, "a Control-C"},
		{0x40010008, "a Control-Break"},
		{0x80000002, "a Datatype Misalignment"},
		{0x80000003, "a Breakpoint"},
		{0xc0000005, "an Access Violation"},
		{0xc0000006, "an In Page Error"},
		{0xc0000017, "a No Memory"},
		{0xc000001d, "an Illegal Instruction"},
		{0xc0000025, "a Noncontinuable Exception"},
		{0xc0000026, "an Invalid Disposition"},
		{0xc000008c, "a Array Bounds Exceeded"},
		{0xc000008d, "a Float Denormal Operand"},
		{0xc000008e, "a Float Divide by Zero"},
		{0xc000008f, "a Float Inexact Result"},
		{0xc0000090, "a Float Invalid Operation"},
		{0xc0000091, "a Float Overflow"},
		{0xc0000092, "a Float Stack Check"},
		{0xc0000093, "a Float Underflow"},
		{0xc0000094, "an Integer Divide by Zero"},
		{0xc0000095, "an Integer Overflow"},
		{0xc0000096, "a Privileged Instruction"},
		{0xc00000fD, "a Stack Overflow"},
		{0xc0000142, "a DLL Initialization Failed"},
		{0xe06d7363, "a Microsoft C++ Exception"},
	};

	for (size_t i = 0; i < sizeof(ExceptionMap) / sizeof(ExceptionMap[0]); i++)
		if (ExceptionCode == ExceptionMap[i].ExceptionCode)
			return ExceptionMap[i].ExceptionName;

	return "an Unknown exception type";
}

//-----------------------------------------------------------------------------
// FillCrashReport
//
// What the handler gathered, written into the report. Addresses are 0x and
// 16 hex digits (wsprintf doesn't reliably support 64-bit arguments, so
// these use snprintf); each call-stack address follows a space.
//-----------------------------------------------------------------------------
void FillCrashReport(MCrashReport& report, const char* executableTime, int version, const char* os,
	unsigned long long address, DWORD exceptionCode, const std::vector<unsigned long long>& callStack)
{
	char szTemp[1024];

	report.SetExcutableTime(executableTime);
	report.SetVersion(WORD(version));
	report.SetOS(os);

	snprintf(szTemp, sizeof(szTemp), "0x%016llx", address);
	report.SetAddress(szTemp);

	snprintf(szTemp, sizeof(szTemp), "%s(0x%08x)", GetExceptionDescription(exceptionCode), unsigned(exceptionCode));
	report.SetMessage(szTemp);

	std::string stack;
	for (unsigned long long frame : callStack)
	{
		snprintf(szTemp, sizeof(szTemp), "0x%016llx", frame);
		stack += ' ';
		stack += szTemp;
	}
	report.SetCallStack(stack.c_str());
}

//-----------------------------------------------------------------------------
// ReadClientVersion
//
// The first four bytes of the file FILE_INFO_VERSION names; 0 if it cannot
// be read.
//-----------------------------------------------------------------------------
int ReadClientVersion()
{
	int version = 0;
	std::ifstream versionFile(g_pFileDef->getProperty("FILE_INFO_VERSION").c_str(), std::ios::binary);

	if (versionFile.is_open())
	{
		versionFile.read((char*)&version, 4);
		versionFile.close();
	}
	return version;
}

// Everything down to RecordExceptionInfo is the dbghelp stack walk, Windows
// only; the non-Windows RecordExceptionInfo at the end is a stub that
// nothing installs (InitCrashReport is Windows-only in ClientMain).
#ifdef PLATFORM_WINDOWS

// ADDRESS64/KDHELP64/STACKFRAME64 and the PREAD_PROCESS_MEMORY_ROUTINE64/
// PFUNCTION_TABLE_ACCESS_ROUTINE64/PGET_MODULE_BASE_ROUTINE64/
// PTRANSLATE_ADDRESS_ROUTINE64 typedefs used to be hand-rolled here because
// the VC6-era Platform SDK didn't ship them. The Windows 10 SDK's
// <imagehlp.h> (pulled in via <Windows.h> above) already declares all of
// these identically, so redefining them is just a duplicate (error C2011).

// StackWalk64()
typedef BOOL (__stdcall *tSW)( 
  DWORD MachineType, 
  HANDLE hProcess,
  HANDLE hThread, 
  LPSTACKFRAME64 StackFrame, 
  PVOID ContextRecord,
  PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
  PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
  PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
  PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress );
tSW pSW = NULL;

// SymFunctionTableAccess64()
typedef PVOID (__stdcall *tSFTA)( HANDLE hProcess, DWORD64 AddrBase );
tSFTA pSFTA = NULL;

// SymGetModuleBase64()
typedef DWORD64 (__stdcall *tSGMB)( IN HANDLE hProcess, IN DWORD64 dwAddr );
tSGMB pSGMB = NULL;

LONG __stdcall RecordExceptionInfo( _EXCEPTION_POINTERS* pExp )
{
	if( g_pSocket != NULL )
	{
		g_pSocket->disconnect();
	}

	PEXCEPTION_RECORD Exception = pExp->ExceptionRecord;
	PCONTEXT          Context   = pExp->ContextRecord;

//	ofstream logFile(CRASH_LOG_FILENAME, ios::app);

	MCrashReportManager crm;
	crm.Init(1);
	MCrashReport &cr = *crm.GetMutable(0); // Init(1) above guarantees this row.

	char szTemp[1024];
	char executableTime[1024] = "";
	GetModuleFileName(NULL, szTemp, 1024);
	SYSTEMTIME st;

	HANDLE ModuleFile = CreateFile(szTemp, GENERIC_READ,
				FILE_SHARE_READ, 0, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL, 0);

	if (ModuleFile != INVALID_HANDLE_VALUE)
	{
		FILETIME LastWriteTime;
		if (GetFileTime(ModuleFile, 0, 0, &LastWriteTime))
		{
			FileTimeToLocalFileTime(&LastWriteTime, &LastWriteTime);
			FileTimeToSystemTime(&LastWriteTime, &st);

			snprintf(executableTime, sizeof(executableTime), "%04d-%02d-%02d %02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
		}
		CloseHandle(ModuleFile);
	}

	// print out operating system
	char os[1024] = "";
	GetWinVersion(os, sizeof(os));

	// Eip/Ebp are the x86 CONTEXT register names; on x64 the equivalent
	// fields are Rip/Rbp (64-bit).
	std::vector<unsigned long long> callStack;

	int frameNum;
	// StackWalk64's frame-walking logic depends on the machine type
	// matching the actual CONTEXT layout (Rip/Rbp here, not Eip/Ebp).
	DWORD imageType = IMAGE_FILE_MACHINE_AMD64;

	HANDLE hThread;
	HANDLE hSWProcess = GetCurrentProcess();
	if(DuplicateHandle( hSWProcess, GetCurrentThread(), hSWProcess, &hThread, 0, false, DUPLICATE_SAME_ACCESS ) != 0)
	{

		STACKFRAME64 s; // in/out stackframe
		memset( &s, '\0', sizeof s );

		HINSTANCE hImagehlpDll = LoadLibrary( "dbghelp.dll" );
		if ( hImagehlpDll != NULL )
		{
			pSFTA = (tSFTA) GetProcAddress( hImagehlpDll, "SymFunctionTableAccess64" );
			pSGMB = (tSGMB) GetProcAddress( hImagehlpDll, "SymGetModuleBase64" );
			pSW = (tSW) GetProcAddress( hImagehlpDll, "StackWalk64" );

			if(pSFTA != NULL && pSGMB != NULL && pSW != NULL)
			{
				s.AddrPC.Offset = Context->Rip;
				s.AddrPC.Mode = AddrModeFlat;
				s.AddrFrame.Offset = Context->Rbp;
				s.AddrFrame.Mode = AddrModeFlat;

				for ( frameNum = 0; ; ++ frameNum )
				{
					if ( ! pSW( imageType, hSWProcess, hThread, &s, NULL, NULL, pSFTA, pSGMB, NULL ) )
						break;

					callStack.push_back(s.AddrPC.Offset);
				}
			}
			FreeLibrary( hImagehlpDll );
		}

		CloseHandle( hThread );
	}
	FillCrashReport(cr, executableTime, ReadClientVersion(), os, Context->Rip, Exception->ExceptionCode, callStack);

	crm.SaveToFile(CRASH_LOG_FILENAME);

	return EXCEPTION_EXECUTE_HANDLER;
}
#else
// Stub implementation for non-Windows platforms
LONG __stdcall RecordExceptionInfo( struct _EXCEPTION_POINTERS* pExp )
{
    // Crash reporting not implemented on non-Windows platforms
    // Just return to continue execution
    (void)pExp;  // Suppress unused parameter warning
    return 0;
}
#endif // PLATFORM_WINDOWS

//void RecordExceptionInfo( unsigned int u, _EXCEPTION_POINTERS* pExp )
//{
//	PEXCEPTION_RECORD Exception = pExp->ExceptionRecord;
//	PCONTEXT          Context   = pExp->ContextRecord;
//
//	ofstream logFile(CRASH_LOG_FILENAME, ios::app);
//
//	if(logFile.is_open())
//	{
//		char szTemp[1024];
//		WIN32_FILE_ATTRIBUTE_DATA fad;
//	 	GetModuleFileName(NULL, szTemp, 1024);
//		GetFileAttributesEx(szTemp, GetFileExInfoStandard, &fad);
//		SYSTEMTIME st;
//		FileTimeToSystemTime(&fad.ftLastWriteTime, &st);
//
//		int version = 0;
//		std::ifstream versionFile(g_pFileDef->getProperty("FILE_INFO_VERSION").c_str(), ios::binary | );
//
//		if (versionFile.is_open())
//		{
//			versionFile.read((char*)&version, 4);
//			versionFile.close();
//		}
//
//		// print out operating system
//		TCHAR szWinVer[512];
//		strcpy(szWinVer, "");
//		GetWinVersion(szWinVer);
//
//		// *CrashLog 2003-12-01 10:50:00 98 0xffffffff a memory access violation at 0xffffffff
//		wsprintf(szTemp, "%04d-%02d-%02d %02d:%02d:%02d %d 0x%08x %s (0x%08x), %s", 
//			st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond,
//			version,
//			Context->Eip,
//			GetExceptionDescription(Exception->ExceptionCode), Exception->ExceptionCode,
//			szWinVer);
////		logFile << GetExceptionDescription(Exception->ExceptionCode) << Exception->ExceptionCode << "at" << Context->Eip << endl;
//		logFile << szTemp << endl;
//
//		logFile.close();
//	}
//
//    throw SE_Exception( u );
//}
