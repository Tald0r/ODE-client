#include "test_framework.h"
#include "CrashReport.h"
#include "MCrashReportManager.h"
#include "Properties.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

// The crash report (task 4.251: the executable's until then, gamemodel's
// since). Windows installs RecordExceptionInfo as the unhandled-exception
// filter; it gathers the crash (the executable's time, the client's
// version, the OS, the faulting address, the exception, the call stack)
// and logs it. What it writes is FillCrashReport's, on every platform; the
// gathering itself runs on Windows only.
namespace {

const char* kVersionPath = "crash_report_version.bin";
const char* kLogPath = "crash_report_test.log";

// g_pFileDef naming a version file of these bytes (none for an empty
// list), restored after.
struct VersionFile
{
	Properties properties;
	Properties* old = g_pFileDef;
	explicit VersionFile(const std::vector<unsigned char>& bytes)
	{
		std::remove(kVersionPath);
		if (!bytes.empty())
		{
			std::ofstream out(kVersionPath, std::ios::binary | std::ios::trunc);
			for (unsigned char byte : bytes) out.put(char(byte));
		}
		properties.setProperty("FILE_INFO_VERSION", kVersionPath);
		g_pFileDef = &properties;
	}
	~VersionFile()
	{
		g_pFileDef = old;
		std::remove(kVersionPath);
	}
};

// MString hands back an empty string as a null pointer.
bool Same(const char* a, const char* b)
{
	if (a == nullptr) a = "";
	const bool same = std::strcmp(a, b) == 0;
	if (!same) std::printf("      \"%s\", not \"%s\"\n", a, b);
	return same;
}

} // namespace

TEST(CrashReport, EveryExceptionCodeHasItsName)
{
	struct Case { DWORD code; const char* name; };
	for (const Case& c : {Case{0x40010005, "a Control-C"}, Case{0x40010008, "a Control-Break"},
		Case{0x80000002, "a Datatype Misalignment"}, Case{0x80000003, "a Breakpoint"},
		Case{0xc0000005, "an Access Violation"}, Case{0xc0000006, "an In Page Error"},
		Case{0xc0000017, "a No Memory"}, Case{0xc000001d, "an Illegal Instruction"},
		Case{0xc0000025, "a Noncontinuable Exception"}, Case{0xc0000026, "an Invalid Disposition"},
		Case{0xc000008c, "a Array Bounds Exceeded"}, Case{0xc000008d, "a Float Denormal Operand"},
		Case{0xc000008e, "a Float Divide by Zero"}, Case{0xc000008f, "a Float Inexact Result"},
		Case{0xc0000090, "a Float Invalid Operation"}, Case{0xc0000091, "a Float Overflow"},
		Case{0xc0000092, "a Float Stack Check"}, Case{0xc0000093, "a Float Underflow"},
		Case{0xc0000094, "an Integer Divide by Zero"}, Case{0xc0000095, "an Integer Overflow"},
		Case{0xc0000096, "a Privileged Instruction"}, Case{0xc00000fd, "a Stack Overflow"},
		Case{0xc0000142, "a DLL Initialization Failed"}, Case{0xe06d7363, "a Microsoft C++ Exception"}})
		CHECK(Same(GetExceptionDescription(c.code), c.name));
	// Any other, the first table's neighbours among them.
	for (DWORD code : {DWORD(0), DWORD(0x40010004), DWORD(0xc0000004), DWORD(0xe06d7364), DWORD(0xffffffff)})
		CHECK(Same(GetExceptionDescription(code), "an Unknown exception type"));
}

TEST(CrashReport, AReportHoldsWhatTheHandlerGathered)
{
	MCrashReport report;
	FillCrashReport(report, "2026-10-15 12:34:56", 0x12345678, "Windows 10/11", 0x00007ff6a1b2c3d4ULL, 0xc0000005,
		{ 0x00007ff6a1b2c3d4ULL, 0x1ULL, 0xfedcba9876543210ULL });
	CHECK(Same(report.GetExecutableTime(), "2026-10-15 12:34:56"));
	// The version is a WORD: the low two bytes.
	CHECK_EQ(0x5678, int(report.GetVersion()));
	CHECK(Same(report.GetOS(), "Windows 10/11"));
	CHECK(Same(report.GetAddress(), "0x00007ff6a1b2c3d4"));
	CHECK(Same(report.GetMessage(), "an Access Violation(0xc0000005)"));
	CHECK(Same(report.GetCallStack(), " 0x00007ff6a1b2c3d4 0x0000000000000001 0xfedcba9876543210"));
	// An unknown exception, a code under eight digits, no stack.
	FillCrashReport(report, "", 7, "", 0, 0x5, {});
	CHECK(Same(report.GetExecutableTime(), ""));
	CHECK_EQ(7, int(report.GetVersion()));
	CHECK(Same(report.GetAddress(), "0x0000000000000000"));
	CHECK(Same(report.GetMessage(), "an Unknown exception type(0x00000005)"));
	CHECK(Same(report.GetCallStack(), ""));
}

TEST(CrashReport, AReportSurvivesItsLog)
{
	MCrashReportManager written;
	written.Init(1);
	FillCrashReport(*written.GetMutable(0), "2026-10-15 12:34:56", 42, "Windows 10/11", 0x1234ULL, 0xc00000fd, { 0x1234ULL, 0x5678ULL });
	written.SaveToFile(kLogPath);
	MCrashReportManager read;
	read.LoadFromFile(kLogPath);
	std::remove(kLogPath);
	CHECK_EQ(1, int(read.GetSize()));
	if (read.GetSize() != 1) return;
	const MCrashReport& report = read[0];
	CHECK(Same(report.GetExecutableTime(), "2026-10-15 12:34:56"));
	CHECK_EQ(42, int(report.GetVersion()));
	CHECK(Same(report.GetOS(), "Windows 10/11"));
	CHECK(Same(report.GetAddress(), "0x0000000000001234"));
	CHECK(Same(report.GetMessage(), "a Stack Overflow(0xc00000fd)"));
	CHECK(Same(report.GetCallStack(), " 0x0000000000001234 0x0000000000005678"));
}

TEST(CrashReport, TheVersionIsTheFirstFourBytesOfItsFile)
{
	{
		VersionFile file({ 0x78, 0x56, 0x34, 0x12, 0xff, 0xff });
		CHECK_EQ(0x12345678, ReadClientVersion());
	}
	{
		// A short file gives the bytes it has.
		VersionFile file({ 0x2a, 0x01 });
		CHECK_EQ(0x012a, ReadClientVersion());
	}
	{
		VersionFile none({});
		CHECK_EQ(0, ReadClientVersion());
	}
}

TEST(CrashReport, TheOperatingSystemIsNamed)
{
	char name[64] = "";
	CHECK(GetWinVersion(name, sizeof(name)));
#ifdef PLATFORM_WINDOWS
	CHECK(std::string(name).rfind("Windows", 0) == 0);
#else
	CHECK(Same(name, "Non-Windows Platform"));
#endif
	// Nowhere to write: refused.
	CHECK(!GetWinVersion(nullptr, sizeof(name)));
	char untouched[4] = "abc";
	CHECK(!GetWinVersion(untouched, 0));
	CHECK(Same(untouched, "abc"));
}

#ifdef PLATFORM_WINDOWS
TEST(CrashReport, TheHandlerLogsTheCrashItIsHanded)
{
	// A divide by zero at this thread's own context: logged, and the
	// process told to run its handler (end).
	VersionFile file({ 0x2a, 0x00, 0x00, 0x00 });
	CONTEXT context = {};
	RtlCaptureContext(&context);
	EXCEPTION_RECORD record = {};
	record.ExceptionCode = 0xc0000094;
	EXCEPTION_POINTERS pointers = { &record, &context };
	std::remove(CRASH_LOG_FILENAME);
	CHECK_EQ(long(EXCEPTION_EXECUTE_HANDLER), long(RecordExceptionInfo(&pointers)));
	MCrashReportManager log;
	log.LoadFromFile(CRASH_LOG_FILENAME);
	std::remove(CRASH_LOG_FILENAME);
	CHECK_EQ(1, int(log.GetSize()));
	if (log.GetSize() != 1) return;
	const MCrashReport& report = log[0];
	char address[32];
	std::snprintf(address, sizeof(address), "0x%016llx", static_cast<unsigned long long>(context.Rip));
	CHECK(Same(report.GetMessage(), "an Integer Divide by Zero(0xc0000094)"));
	CHECK(Same(report.GetAddress(), address));
	CHECK_EQ(42, int(report.GetVersion()));
	CHECK(std::string(report.GetOS()).rfind("Windows", 0) == 0);
	// The executable's time, "yyyy-mm-dd hh:mm:ss".
	CHECK(report.GetExecutableTime() != nullptr && std::strlen(report.GetExecutableTime()) == 19);
	// The walk starts at the faulting address and goes on to its callers.
	const std::string stack = report.GetCallStack() != nullptr ? report.GetCallStack() : "";
	std::printf("      call stack:%s\n", stack.c_str());
	CHECK(stack.rfind(std::string(" ") + address, 0) == 0);
	CHECK(stack.size() > std::string(" ").size() + std::strlen(address));
}
#else
TEST(CrashReport, OffWindowsTheHandlerDoesNothing)
{
	CHECK_EQ(0, int(RecordExceptionInfo(nullptr)));
}
#endif
