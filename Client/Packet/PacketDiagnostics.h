//////////////////////////////////////////////////////////////////////
//
// Filename    : PacketDiagnostics.h
// Description : The wire layer's one outbound diagnostic seam.
//
//               Datagram::read used to call SendBugReport() directly,
//               which was then defined in the executable - a function
//               that formats the text and sends it to the game server
//               as a "*bug_report" chat line. A library may not reach
//               into the executable (rule W1, and the packet factory
//               test links packetwire on its own), so the call went
//               through a hook the executable installed at its
//               composition root.
//
//               SendBugReport moved into this library with the rest of
//               the wire layer's seams (docs/RESTRUCTURING.md task 5.1,
//               second slice), so the hook is no longer how a report
//               gets out: with none installed, reportBug calls it
//               directly, and it drops the report when the wire host
//               names no connection. The executable installs nothing;
//               it used to install a forwarder that called straight
//               back in here.
//
//               The hook stays because it is worth having - it is how
//               a test captures the formatted text - but it is now an
//               interception point rather than the delivery path.
//
//               Nothing in the wire layer calls it today. Its one
//               caller was Datagram::read's size check, which stopped
//               reporting on 2026-09-30: a datagram comes from whoever
//               can reach the client's UDP port, and a report per
//               datagram put one chat line on the player's own server
//               connection per hostile datagram.
//
//////////////////////////////////////////////////////////////////////

#ifndef __PACKET_DIAGNOSTICS_H__
#define __PACKET_DIAGNOSTICS_H__

#include "SafeFormat.h"

namespace PacketDiagnostics {

	// Receives one fully formatted report.
	typedef void (*BugReportFn)(const char* message);

	// Install (or, with NULL, remove) the executable's reporter. Written
	// only at startup, like the dispatcher table.
	void setBugReportHook(BugReportFn fn);
	BugReportFn getBugReportHook();

	// Formats a typed argument list into the same bounded 256-byte report.
	void reportBugArgs(const char* format, const SafeFormat::Arg* args, size_t count);
	template <typename... Args>
	void reportBug(const char* format, Args... args)
	{
		const SafeFormat::Arg packed[] = {SafeFormat::MakeArg(args)..., SafeFormat::Arg()};
		reportBugArgs(format, packed, sizeof...(args));
	}

}

#endif // __PACKET_DIAGNOSTICS_H__
