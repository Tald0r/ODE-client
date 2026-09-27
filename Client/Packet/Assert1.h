//--------------------------------------------------------------------------------
//
// Filename   : Assert.h
// Written By : Reiot
//
//--------------------------------------------------------------------------------

#ifndef __ASSERT_H__
#define __ASSERT_H__

// include files
#include "Types.h"
#include "Exception.h"

//--------------------------------------------------------------------------------
//
// When NDEBUG is defined in Config.h, every Assert is ignored. Otherwise a
// failed Assert logs to a file and then throws AssertionError.
//
// An unreferenced duplicate of PacketAssert.h, kept in step with it.
//
//--------------------------------------------------------------------------------

// Compatibility entry point, for call sites that name a file and a line.
void __assert__ ( const char * file , uint line , const char * func , const char * expr );

// C++20 entry point: the failing site arrives through the defaulted
// DiagnosticSite (Exception.h).
void __assert__ ( const char * func , const char * expr , const DiagnosticSite & site = DiagnosticSite() );

#undef Assert
#if defined(NDEBUG)
	#define Assert(expr) ((void)0)
#elif defined(__MFC__)
	#define Assert(expr) ASSERT(expr)
#else
	/* One spelling on every platform. The site comes from the defaulted
	   DiagnosticSite (std::source_location), so the function name is left
	   empty here; the POSIX branch used to pass __PRETTY_FUNCTION__ and
	   produced a different message from the Windows one for the same
	   assertion (test_packet_source_location.cpp pins the message). */
	#define Assert(expr) ((void)((expr)?0:(__assert__("",#expr),0)))
#endif

#endif
