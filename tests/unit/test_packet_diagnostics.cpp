//----------------------------------------------------------------------
// test_packet_diagnostics.cpp
//----------------------------------------------------------------------
//
// PacketDiagnostics (docs/reviews/RESTRUCTURING.md task 2.4): the wire library's
// bug-report seam. It existed because SendBugReport was defined in the
// executable, which installed it here at its composition root so the
// library could report without linking against it.
//
// Task 5.1's second slice moved SendBugReport into the library, so the
// hook is an interception point now rather than the way out: with none
// installed a report goes to SendBugReport directly, and that drops it
// when the wire host names no connection. These tests install their own
// hook, so what they pin is unchanged - a hook, once installed, is
// where the text goes and the only place it goes.
//
//----------------------------------------------------------------------

#include "test_framework.h"

#include "PacketDiagnostics.h"

#include <string>
#include <cstring>

namespace {

std::string	s_LastReport;
int		s_Reports = 0;

void	Capture(const char* message)
{
	s_LastReport = message;
	s_Reports++;
}

struct HookGuard
{
	PacketDiagnostics::BugReportFn m_Previous;
	HookGuard() : m_Previous(PacketDiagnostics::getBugReportHook()) { s_Reports = 0; s_LastReport.clear(); }
	~HookGuard() { PacketDiagnostics::setBugReportHook(m_Previous); }
};

} // namespace

// Prove the counter moves with a hook, then that removing the hook
// stops it reaching the capture - otherwise "no report" would be true
// by construction. Since 5.1 the report still leaves, through
// SendBugReport; what this pins is that the hook is exclusive.
TEST(PacketDiagnostics, AHookIsTheOnlyPlaceTheTextGoes)
{
	HookGuard guard;
	PacketDiagnostics::setBugReportHook(&Capture);
	PacketDiagnostics::reportBug("armed %d", 1);
	CHECK_EQ(1, s_Reports);
	PacketDiagnostics::setBugReportHook(NULL);
	PacketDiagnostics::reportBug("too large PacketSize ID)%d %d/%d", 1, 2, 3);
	CHECK_EQ(1, s_Reports);
	CHECK(s_LastReport == "armed 1");
}

TEST(PacketDiagnostics, HookReceivesTheFormattedText)
{
	HookGuard guard;
	PacketDiagnostics::setBugReportHook(&Capture);
	PacketDiagnostics::reportBug("too large PacketSize ID)%d %d/%d", 284, 1000, 12);
	CHECK_EQ(1, s_Reports);
	CHECK(s_LastReport == "too large PacketSize ID)284 1000/12");
}

// Over-long reports are cut at the 255-character buffer, never overrun.
TEST(PacketDiagnostics, OverLongReportIsTruncatedNotOverrun)
{
	HookGuard guard;
	PacketDiagnostics::setBugReportHook(&Capture);
	const std::string longText(600, 'x');
	PacketDiagnostics::reportBug("%s", longText.c_str());
	CHECK_EQ(1, s_Reports);
	CHECK_EQ(255, s_LastReport.size());
}

TEST(PacketDiagnostics, NullFormatIsIgnored)
{
	HookGuard guard;
	PacketDiagnostics::setBugReportHook(&Capture);
	PacketDiagnostics::reportBug(NULL);
	CHECK_EQ(0, s_Reports);
}

TEST(PacketDiagnostics, TypedArgumentsRefuseMissingMismatchedAndWritingConversions)
{
	HookGuard guard;
	PacketDiagnostics::setBugReportHook(&Capture);
	PacketDiagnostics::reportBug("%s %d %s", 42);
	CHECK(s_LastReport == "%s 42 %s");
	int untouched = 17;
	PacketDiagnostics::reportBug("report%n", &untouched);
	CHECK(s_LastReport == "report%n");
	CHECK_EQ(17, untouched);
	PacketDiagnostics::reportBug("%s", "literal %s %n %%");
	CHECK(s_LastReport == "literal %s %n %%");
}
