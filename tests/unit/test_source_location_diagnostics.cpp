//----------------------------------------------------------------------
// test_source_location_diagnostics.cpp
//----------------------------------------------------------------------
//
// The C++20 source_location entry points in basic/BasicException.h and
// basic/DebugLog.h (docs/history/cpp17-cpp20-compatibility-assessment-2026-09-04.md,
// modernization backlog priority 1).
//
// The whole value of the change is that a call site stops forwarding
// __FILE__ and __LINE__ by hand and records the same thing anyway, so
// "the same thing anyway" is what is under test. A defaulted
// std::source_location::current() is evaluated at the CALL site, and
// here it is nested one level deep - it is the default argument of the
// ExceptionSite/LogSite constructor, which is itself the default
// argument of the entry point. That nesting is the part a compiler
// could plausibly get wrong by reporting the header's own line, so
// every capture below is compared against the __LINE__ and __FILE__ of
// the line that made the call, not merely checked for being non-zero.
//
// Both facilities are fatal or global by nature - g_BasicException
// asserts in Debug and exits in Release, and log_write reaches a file
// and the console - so each header carries one small, documented test
// seam (a reporter, an observer) that is NULL in every shipped build.
// Those seams are what makes the capture observable; they are installed
// and put back inside the tests below.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "BasicException.h"
#include "DebugLog.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>


namespace {

//----------------------------------------------------------------------
// Comparison helpers. NULL tolerant so a regression fails the check
// instead of crashing the whole run.
//----------------------------------------------------------------------
bool
Is(const char* expected, const char* actual)
{
	return actual != NULL && std::strcmp(actual, expected) == 0;
}

bool
EndsWith(const char* text, const char* suffix)
{
	if (text == NULL)
		return false;

	const size_t n_text = std::strlen(text);
	const size_t n_suffix = std::strlen(suffix);

	return n_text >= n_suffix
		&& std::strcmp(text + (n_text - n_suffix), suffix) == 0;
}

bool
Contains(const char* text, const char* needle)
{
	return text != NULL && std::strstr(text, needle) != NULL;
}

//----------------------------------------------------------------------
// What the exception reporter seam was handed
//----------------------------------------------------------------------
struct ExceptionCapture
{
	bool			b_called;
	EXCEPTION_CODE		code;
	const char*		sz_error;
	const char*		file;
	unsigned long		line;
	const char*		function;
};

ExceptionCapture	g_exception_capture;

void
ResetExceptionCapture()
{
	g_exception_capture.b_called = false;
	g_exception_capture.code = INVALID_EXCEPTION_CODE;
	g_exception_capture.sz_error = NULL;
	g_exception_capture.file = NULL;
	g_exception_capture.line = 0;
	g_exception_capture.function = NULL;
}

void
RecordException(EXCEPTION_CODE code, const char* sz_error, const ExceptionSite& site)
{
	g_exception_capture.b_called = true;
	g_exception_capture.code = code;
	g_exception_capture.sz_error = sz_error;
	g_exception_capture.file = site.file;
	g_exception_capture.line = site.line;
	g_exception_capture.function = site.function;
}

//----------------------------------------------------------------------
// What the log site observer seam was handed
//----------------------------------------------------------------------
struct LogCapture
{
	bool		b_called;
	LogLevel	level;
	const char*	file;
	int		line;
	const char*	function;
};

LogCapture	g_log_capture;

void
ResetLogCapture()
{
	g_log_capture.b_called = false;
	g_log_capture.level = LOG_LEVEL_NONE;
	g_log_capture.file = NULL;
	g_log_capture.line = 0;
	g_log_capture.function = NULL;
}

void
RecordLogSite(const LogSite& site, LogLevel level)
{
	g_log_capture.b_called = true;
	g_log_capture.level = level;
	g_log_capture.file = site.file;
	g_log_capture.line = site.line;
	g_log_capture.function = site.function;
}

} // anonymous namespace


//----------------------------------------------------------------------
// BasicException
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// The point of the slice: a call that passes no location at all records
// the caller's line, and records exactly the string __FILE__ would have
// produced in this translation unit.
//----------------------------------------------------------------------
TEST(SourceLocationDiagnostics, ExceptionEntryPointRecordsTheCallersSite)
{
	BasicExceptionReporter previous = g_SetBasicExceptionReporter(&RecordException);

	ResetExceptionCapture();
	const unsigned long expected_line = (unsigned long)__LINE__ + 1;
	g_BasicException(FILE_OPEN, NULL);

	CHECK(g_exception_capture.b_called);
	CHECK_EQ(FILE_OPEN, g_exception_capture.code);
	CHECK_EQ(expected_line, g_exception_capture.line);
	CHECK(Is(__FILE__, g_exception_capture.file));
	CHECK(EndsWith(g_exception_capture.file, "test_source_location_diagnostics.cpp"));

	// The function name is captured even though nothing prints it. The
	// test framework names a test body <suite>_<name>_Body, so the suite
	// name appears in whatever spelling the compiler uses.
	CHECK(Contains(g_exception_capture.function, "SourceLocationDiagnostics"));

	g_SetBasicExceptionReporter(previous);
}

//----------------------------------------------------------------------
// The macros are the population that mattered: every _Error in VS_UI and
// Client goes through them, and their recorded file and line must be
// what they were before the macros stopped spelling __FILE__ and
// __LINE__ out.
//----------------------------------------------------------------------
TEST(SourceLocationDiagnostics, ErrorMacrosRecordTheLineOfTheMacroUse)
{
	BasicExceptionReporter previous = g_SetBasicExceptionReporter(&RecordException);

	ResetExceptionCapture();
	const unsigned long error_line = (unsigned long)__LINE__ + 1;
	_Error(NULL_REF);

	CHECK(g_exception_capture.b_called);
	CHECK_EQ(NULL_REF, g_exception_capture.code);
	CHECK(g_exception_capture.sz_error == NULL);
	CHECK_EQ(error_line, g_exception_capture.line);
	CHECK(Is(__FILE__, g_exception_capture.file));

	ResetExceptionCapture();
	const unsigned long error_str_line = (unsigned long)__LINE__ + 1;
	_ErrorStr("a message from the unit tests");

	CHECK(g_exception_capture.b_called);
	CHECK_EQ(INVALID_EXCEPTION_CODE, g_exception_capture.code);
	CHECK(Is("a message from the unit tests", g_exception_capture.sz_error));
	CHECK_EQ(error_str_line, g_exception_capture.line);
	CHECK(Is(__FILE__, g_exception_capture.file));

	void*	p_null = NULL;

	ResetExceptionCapture();
	const unsigned long check_mem_line = (unsigned long)__LINE__ + 1;
	CheckMemAlloc(p_null);

	CHECK(g_exception_capture.b_called);
	CHECK_EQ(MEM_ALLOC, g_exception_capture.code);
	CHECK_EQ(check_mem_line, g_exception_capture.line);
	CHECK(Is(__FILE__, g_exception_capture.file));

	// The same macro over a live pointer must still say nothing at all.
	int	n_live = 0;
	void*	p_live = &n_live;

	ResetExceptionCapture();
	CheckMemAlloc(p_live);
	CHECK_EQ(false, g_exception_capture.b_called);

	g_SetBasicExceptionReporter(previous);
}

//----------------------------------------------------------------------
// The (file, line) entry point is kept for the call sites that pass a
// location explicitly. It must record what it is handed and nothing
// else, and it must agree with the C++20 entry point when both are
// asked about the same line.
//----------------------------------------------------------------------
TEST(SourceLocationDiagnostics, ExceptionCompatibilityWrapperRecordsWhatItIsGiven)
{
	BasicExceptionReporter previous = g_SetBasicExceptionReporter(&RecordException);

	ResetExceptionCapture();
	g_BasicException(FAILED_JOB, NULL, "GameInit.cpp", 4242);

	CHECK(g_exception_capture.b_called);
	CHECK_EQ(FAILED_JOB, g_exception_capture.code);
	CHECK_EQ(4242, g_exception_capture.line);
	CHECK(Is("GameInit.cpp", g_exception_capture.file));

	// A file and a line carry no function name, so the wrapper reports
	// none rather than inventing one.
	CHECK(g_exception_capture.function == NULL);

	// Handed this translation unit's own __FILE__ and __LINE__, the
	// wrapper records exactly what the C++20 entry point records for
	// itself one line later.
	ResetExceptionCapture();
	const unsigned long ul_wrapper_expected = (unsigned long)__LINE__ + 1;
	g_BasicException(FILE_LOAD, NULL, __FILE__, __LINE__);
	const char* const	sz_wrapper_file = g_exception_capture.file;
	const unsigned long	ul_wrapper_line = g_exception_capture.line;

	ResetExceptionCapture();
	const unsigned long ul_located_expected = (unsigned long)__LINE__ + 1;
	g_BasicException(FILE_LOAD, NULL);
	const char* const	sz_located_file = g_exception_capture.file;
	const unsigned long	ul_located_line = g_exception_capture.line;

	CHECK_EQ(ul_wrapper_expected, ul_wrapper_line);
	CHECK_EQ(ul_located_expected, ul_located_line);
	CHECK(Is(sz_wrapper_file, sz_located_file));

	g_SetBasicExceptionReporter(previous);
}

//----------------------------------------------------------------------
// The reporter seam itself: installing one returns the previous one, so
// a test can put back whatever it found, and the shipped default is
// none.
//----------------------------------------------------------------------
TEST(SourceLocationDiagnostics, ExceptionReporterSeamRestoresThePreviousReporter)
{
	BasicExceptionReporter previous = g_SetBasicExceptionReporter(&RecordException);

	CHECK(previous == NULL);
	CHECK(g_SetBasicExceptionReporter(previous) == &RecordException);
	CHECK(g_SetBasicExceptionReporter(NULL) == NULL);
}


//----------------------------------------------------------------------
// DebugLog
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// LogSite is the seam that lets a location reach a C variadic function
// at all: it cannot follow the '...', so it is built at the call site
// and passed in front of the format. Both of its constructors are
// exercised here, because the (file, line) one is what the untouched
// LOG_* macros go through.
//----------------------------------------------------------------------
TEST(SourceLocationDiagnostics, LogSiteRoundTripsBothWaysOfNamingASite)
{
	const int	n_expected_line = __LINE__ + 1;
	LogSite		site;

	CHECK_EQ(n_expected_line, site.line);
	CHECK(Is(__FILE__, site.file));
	CHECK(EndsWith(site.file, "test_source_location_diagnostics.cpp"));
	CHECK(Contains(site.function, "SourceLocationDiagnostics"));

	LogSite		explicit_site("GameMain.cpp", 1234);

	CHECK_EQ(1234, explicit_site.line);
	CHECK(Is("GameMain.cpp", explicit_site.file));
	CHECK(explicit_site.function == NULL);
}

//----------------------------------------------------------------------
// The new entry point and the untouched LOG_* macros must arrive at the
// same core carrying the same file and line, because the whole point of
// keeping the macros is that a converted call site and an unconverted
// one produce the same log line.
//----------------------------------------------------------------------
TEST(SourceLocationDiagnostics, LogEntryPointsRecordTheSameSite)
{
	LogSiteObserver previous = log_set_site_observer(&RecordLogSite);

	ResetLogCapture();
	const int n_at_line = __LINE__ + 1;
	log_write_at(LogSite(), LOG_LEVEL_ERROR, "unit test %d", 1);

	CHECK(g_log_capture.b_called);
	CHECK_EQ(LOG_LEVEL_ERROR, g_log_capture.level);
	CHECK_EQ(n_at_line, g_log_capture.line);
	CHECK(Is(__FILE__, g_log_capture.file));
	CHECK(Contains(g_log_capture.function, "SourceLocationDiagnostics"));

	const char* const sz_at_file = g_log_capture.file;

	ResetLogCapture();
	const int n_macro_line = __LINE__ + 1;
	LOG_ERROR("unit test %d", 2);

	CHECK(g_log_capture.b_called);
	CHECK_EQ(LOG_LEVEL_ERROR, g_log_capture.level);
	CHECK_EQ(n_macro_line, g_log_capture.line);
	CHECK(Is(sz_at_file, g_log_capture.file));

	// The macro forwards __FILE__ and __LINE__ and nothing else, so the
	// function name is absent rather than wrong.
	CHECK(g_log_capture.function == NULL);

	// The legacy DEBUG_ADD wrappers still route through the same core.
	ResetLogCapture();
	const int n_debug_add_line = __LINE__ + 1;
	DEBUG_ADD_ERR("a message from the unit tests");

	CHECK(g_log_capture.b_called);
	CHECK_EQ(LOG_LEVEL_ERROR, g_log_capture.level);
	CHECK_EQ(n_debug_add_line, g_log_capture.line);

	log_set_site_observer(previous);
}

//----------------------------------------------------------------------
// The observer runs before the level filter, which is what lets a test
// see a site without initialising the logging system or moving the
// level. Nothing is emitted here either way: log_init has not been
// called in this binary, so the core returns before any output.
//----------------------------------------------------------------------
TEST(SourceLocationDiagnostics, LogObserverSeesFilteredCallsAndRestoresThePrevious)
{
	LogSiteObserver previous = log_set_site_observer(&RecordLogSite);

	CHECK(previous == NULL);

	// DEBUG is below the default INFO level and the system is not
	// initialised, so this call emits nothing - and is still observed.
	ResetLogCapture();
	const int n_line = __LINE__ + 1;
	log_write_at(LogSite(), LOG_LEVEL_DEBUG, "%s", "filtered out");

	CHECK(g_log_capture.b_called);
	CHECK_EQ(LOG_LEVEL_DEBUG, g_log_capture.level);
	CHECK_EQ(n_line, g_log_capture.line);

	CHECK(log_set_site_observer(NULL) == &RecordLogSite);

	// With no observer installed the seam is inert.
	ResetLogCapture();
	log_write_at(LogSite(), LOG_LEVEL_ERROR, "%s", "unobserved");
	CHECK_EQ(false, g_log_capture.b_called);

	log_set_site_observer(previous);
}

//----------------------------------------------------------------------
// The line that reaches the file is byte for byte what it was. The
// observer seam above sees a site before anything is formatted, so it
// cannot pin this; the file sink can. The logging system is initialised
// for the duration of this test only and put back afterwards, and the
// timestamp - the one part of a line that legitimately differs between
// two calls - is cut off before the comparison.
//----------------------------------------------------------------------
TEST(SourceLocationDiagnostics, LogLinesAreByteIdenticalBetweenEntryPoints)
{
	std::error_code			error;
	const std::filesystem::path	log_path =
		std::filesystem::temp_directory_path(error) / "source_location_diagnostics.log";

	log_init();
	log_set_console_output(false);
	log_set_file_output(log_path.string().c_str());
	log_set_level(LOG_LEVEL_ERROR);

	// The macro path and the two spellings of the new path, same message.
	log_write(LOG_LEVEL_ERROR, "C:\\src\\Some.cpp", 77, "unit %d", 3);
	log_write_at(LogSite("C:\\src\\Some.cpp", 77), LOG_LEVEL_ERROR, "unit %d", 3);

	const int n_line = __LINE__ + 1;
	log_write_at(LogSite(), LOG_LEVEL_ERROR, "unit %d", 3);

	log_write(LOG_LEVEL_ERROR, "typed.cpp", 5, "%s %d %s", 42);
	log_write_at(LogSite("typed.cpp", 6), LOG_LEVEL_ERROR, "%s", "literal %s %n %%");

	// Pointer diagnostics must use %p: a rejected %x would leave the
	// pointer unconsumed and lose the strings and status that follow it.
	int server = 0;
	char address[64];
	std::snprintf(address, sizeof(address), "%p", static_cast<void*>(&server));
	log_write_at(LogSite("typed.cpp", 7), LOG_LEVEL_ERROR,
		"[UI] SetCurrentServerName : %p %s %s %d",
		static_cast<const void*>(&server), "Group", "Server", 3);

	// Below the level: nothing may reach the file from either path.
	log_write(LOG_LEVEL_WARN, "C:\\src\\Some.cpp", 78, "%s", "filtered");
	log_write_at(LogSite(), LOG_LEVEL_WARN, "%s", "filtered");

	log_set_file_output(NULL);
	log_set_level(LOG_LEVEL_INFO);
	log_set_console_output(true);
	log_cleanup();

	std::vector<std::string>	lines;
	{
		std::ifstream	file(log_path);
		std::string	line;

		while (std::getline(file, line))
		{
			// Everything after the timestamp: "[ERROR] [file:line] message".
			const std::string::size_type n_cut = line.find("] ");
			lines.push_back(n_cut == std::string::npos ? line : line.substr(n_cut + 2));
		}
	}
	std::filesystem::remove(log_path, error);

	CHECK_EQ(6, lines.size());

	if (lines.size() == 6)
	{
		CHECK(lines[0] == "[ERROR] [Some.cpp:77] unit 3");
		CHECK(lines[1] == lines[0]);
		CHECK(lines[3] == "[ERROR] [typed.cpp:5] %s 42 %s");
		CHECK(lines[4] == "[ERROR] [typed.cpp:6] literal %s %n %%");
		CHECK(lines[5] == std::string("[ERROR] [typed.cpp:7] [UI] SetCurrentServerName : ")
			+ address + " Group Server 3");

		// The function name is captured but deliberately not printed.
		const std::string expected =
			"[ERROR] [test_source_location_diagnostics.cpp:" + std::to_string(n_line) + "] unit 3";
		CHECK(lines[2] == expected);
	}
}
