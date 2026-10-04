//----------------------------------------------------------------------
// test_packet_source_location.cpp
//----------------------------------------------------------------------
//
// DiagnosticSite, __END_CATCH, Assert() and ClientPlayer.cpp's
// packet-skip notice: each captures its site through a defaulted
// std::source_location instead of forwarding __FILE__ and __LINE__.
//
// The pins are exact strings built from this translation unit's own
// __FILE__ and __LINE__, because the failure mode worth catching is a
// capture that resolves to the header's line rather than the caller's.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "PacketAssert.h"
#include "Exception.h"
#include "DebugLog.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>


namespace {

// A failing Assert appends to assertion_failed.log; clean it up.
void	RemoveAssertionLog()
{
	std::error_code Error;
	std::filesystem::remove("assertion_failed.log", Error);
}

// The __END_CATCH probe: the line the macro occupies is recorded from
// inside the function rather than counted from outside it.
int	g_n_end_catch_line = 0;

void
ThrowThroughEndCatch()
{
	__BEGIN_TRY

	// __END_CATCH is two lines below the assignment.
	g_n_end_catch_line = __LINE__ + 2;
	throw Exception("a message from the unit tests");
	__END_CATCH
}

const std::string	NEWLINE("\n");

// The stack trace Throwable renders for a single frame: one space, the
// "file:line" string addStack pushed, and a streamed newline.
std::string
OneFrame(const std::string& file, int line)
{
	return " " + file + ":" + std::to_string(line) + NEWLINE;
}

// Everything a failed assertion writes before ctime()'s timestamp.
std::string
AssertionPrefix(const std::string& file, int line, const std::string& func, const std::string& expr)
{
	// func is streamed only when non-NULL, and "" is non-NULL, so its
	// separator is emitted either way and none appears before expr.
	return NEWLINE + "Assertion Failed : " + file + " : " + std::to_string(line)
		+ " : " + func + expr + " at ";
}

bool
StartsWith(const std::string& text, const std::string& prefix)
{
	return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

} // anonymous namespace


//----------------------------------------------------------------------
// DiagnosticSite
//----------------------------------------------------------------------

TEST(PacketSourceLocation, DiagnosticSiteCapturesTheCallersLine)
{
	const int		n_expected_line = __LINE__ + 1;
	const DiagnosticSite	site;

	CHECK_EQ(n_expected_line, site.line);
	CHECK(std::string(site.file) == __FILE__);
	CHECK(site.function != NULL);
	CHECK(std::string(site.function).find("PacketSourceLocation") != std::string::npos);

	const DiagnosticSite	explicit_site("GameMain.cpp", 1234);

	CHECK_EQ(1234, explicit_site.line);
	CHECK(std::string(explicit_site.file) == "GameMain.cpp");
	CHECK(explicit_site.function == NULL);
}


//----------------------------------------------------------------------
// Throwable::addStack and __END_CATCH
//----------------------------------------------------------------------

TEST(PacketSourceLocation, AddStackEntryPointRecordsTheCallersSite)
{
	Throwable	t("a message from the unit tests");

	const int	n_line = __LINE__ + 1;
	t.addStack();

	CHECK(t.getStackTrace() == OneFrame(__FILE__, n_line));
	CHECK(t.getMessage() == "a message from the unit tests");
}

TEST(PacketSourceLocation, AddStackCompatibilityOverloadRecordsWhatItIsGiven)
{
	Throwable	t_explicit;
	t_explicit.addStack("GameInit.cpp", 4242);
	CHECK(t_explicit.getStackTrace() == OneFrame("GameInit.cpp", 4242));

	Throwable		t_forwarded;
	const int		n_forwarded_line = __LINE__ + 1;
	t_forwarded.addStack(__FILE__, __LINE__);

	Throwable		t_captured;
	const int		n_captured_line = __LINE__ + 1;
	t_captured.addStack();

	CHECK(t_forwarded.getStackTrace() == OneFrame(__FILE__, n_forwarded_line));
	CHECK(t_captured.getStackTrace() == OneFrame(__FILE__, n_captured_line));

	// Same shape, different line: neither reports the header's location.
	CHECK(t_forwarded.getStackTrace() != t_captured.getStackTrace());
}

TEST(PacketSourceLocation, EndCatchRecordsTheLineOfTheMacroUse)
{
	bool	b_caught = false;

	try
	{
		ThrowThroughEndCatch();
	}
	catch (Throwable& t)
	{
		b_caught = true;

#ifdef NDEBUG
		// __BEGIN_TRY/__END_CATCH compile away under NDEBUG.
		CHECK(t.getStackTrace().empty());
#else
		CHECK(g_n_end_catch_line != 0);
		CHECK(t.getStackTrace() == OneFrame(__FILE__, g_n_end_catch_line));
		CHECK(t.getMessage() == "a message from the unit tests");
#endif
	}

	CHECK(b_caught);
}


//----------------------------------------------------------------------
// __assert__ and Assert()
//----------------------------------------------------------------------

TEST(PacketSourceLocation, AssertEntryPointRecordsTheCallersSite)
{
	bool	b_caught = false;

	const int	n_line = __LINE__ + 2;
	try {
		__assert__("", "n_value != 0");
	}
	catch (AssertionError& e)
	{
		b_caught = true;
		CHECK(StartsWith(e.getMessage(), AssertionPrefix(__FILE__, n_line, "", "n_value != 0")));
		CHECK(e.getName() == "AssertionError");
		RemoveAssertionLog();
	}

	CHECK(b_caught);
}

// On this platform the macro passes an empty function name.
TEST(PacketSourceLocation, AssertMacroRecordsTheLineOfTheMacroUse)
{
	const bool	b_false = false;
	bool		b_caught = false;

#ifdef NDEBUG
	// Assert() compiles away under NDEBUG (PacketAssert.h).
	Assert(b_false);
	CHECK_EQ(false, b_caught);
#else
	const int	n_line = __LINE__ + 2;
	try {
		Assert(b_false);
	}
	catch (AssertionError& e)
	{
		b_caught = true;
		CHECK(StartsWith(e.getMessage(), AssertionPrefix(__FILE__, n_line, "", "b_false")));
	}

	CHECK(b_caught);
#endif

	// A satisfied assertion still says nothing at all.
	const bool	b_true = true;
	bool		b_threw = false;

	try {
		Assert(b_true);
	}
	catch (Throwable&)
	{
		b_threw = true;
	}

	CHECK_EQ(false, b_threw);

	RemoveAssertionLog();
}

TEST(PacketSourceLocation, AssertCompatibilityOverloadRecordsWhatItIsGiven)
{
	bool	b_caught = false;

	try {
		__assert__("GameInit.cpp", 4242, "InitGame", "pZone != NULL");
	}
	catch (AssertionError& e)
	{
		b_caught = true;
		CHECK(StartsWith(e.getMessage(),
			NEWLINE + "Assertion Failed : GameInit.cpp : 4242 : InitGamepZone != NULL at "));
	}

	CHECK(b_caught);

	// A NULL function name drops its separator with it.
	b_caught = false;

	try {
		__assert__("GameInit.cpp", 4242, NULL, "pZone != NULL");
	}
	catch (AssertionError& e)
	{
		b_caught = true;
		CHECK(StartsWith(e.getMessage(),
			NEWLINE + "Assertion Failed : GameInit.cpp : 4242pZone != NULL at "));
	}

	RemoveAssertionLog();

	CHECK(b_caught);
}


//----------------------------------------------------------------------
// ClientPlayer.cpp's packet-skip notice
//----------------------------------------------------------------------

// The live site is unreachable from a test binary, so the warning macro
// and the explicit site are written side by side into the file sink.
// Warnings remain visible at the default INFO threshold; the two lines
// must match but for the number each names for itself.
TEST(PacketSourceLocation, PacketSkipWarningPreservesTheMacroCallSite)
{
	static const char* const	sz_format =
		"[PacketSkip] So many Packets. MaxProcessPacket:%d, CurrentPacket:%d, File:%s, Line:%d";

	std::error_code			error;
	const std::filesystem::path	log_path =
		std::filesystem::temp_directory_path(error) / "packet_source_location.log";

	log_init();
	log_set_console_output(false);
	log_set_file_output(log_path.string().c_str());
	log_set_level(LOG_LEVEL_INFO);

	// The equivalent warning macro captures the same caller information.
	const int n_macro_line = __LINE__ + 1;
	DEBUG_ADD_FORMAT_WAR(sz_format, 30, 31, __FILE__, __LINE__);

	// Exactly what it holds now.
	const int n_site_line = __LINE__ + 1;
	const LogSite site;
	log_write_at(site, LOG_LEVEL_WARN, sz_format, 30, 31, site.file, site.line);

	log_set_file_output(NULL);
	log_set_console_output(true);
	log_cleanup();

	std::vector<std::string>	lines;
	{
		std::ifstream	file(log_path);
		std::string	line;

		while (std::getline(file, line))
		{
			// Everything after the timestamp: "[WARN ] [file:line] message".
			const std::string::size_type n_cut = line.find("] ");
			lines.push_back(n_cut == std::string::npos ? line : line.substr(n_cut + 2));
		}
	}
	std::filesystem::remove(log_path, error);

	CHECK_EQ(2, lines.size());

	if (lines.size() == 2)
	{
		// DebugLog prints the basename in the header and whatever it was
		// handed in the body.
		const std::string	expected_macro =
			"[WARN ] [test_packet_source_location.cpp:" + std::to_string(n_macro_line)
			+ "] [PacketSkip] So many Packets. MaxProcessPacket:30, CurrentPacket:31, File:"
			+ __FILE__ + ", Line:" + std::to_string(n_macro_line);

		const std::string	expected_site =
			"[WARN ] [test_packet_source_location.cpp:" + std::to_string(n_site_line)
			+ "] [PacketSkip] So many Packets. MaxProcessPacket:30, CurrentPacket:31, File:"
			+ __FILE__ + ", Line:" + std::to_string(n_site_line);

		CHECK(lines[0] == expected_macro);
		CHECK(lines[1] == expected_site);

		// The header and the body of the converted line name one site.
		CHECK(lines[1].find(":" + std::to_string(n_site_line) + "]") != std::string::npos);
		CHECK(lines[1].find("Line:" + std::to_string(n_site_line)) != std::string::npos);
	}
}
