//-----------------------------------------------------------------------------
// DebugLog.h
//
// Lightweight, cross-platform logging system for Dark Eden client
// Replaces the old DEBUG_ADD series of macros
//
// Features:
// - Automatic __FILE__ and __LINE__ inclusion
// - Log levels: DEBUG/INFO/WARN/ERROR/NONE
// - Configurable output: console/file/memory array
// - Thread-safe (using CRITICAL_SECTION)
// - Optional DEBUG logging with argument evaluation skipped when filtered
//-----------------------------------------------------------------------------

#ifndef __DEBUG_LOG_H__
#define __DEBUG_LOG_H__

#include <stdio.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// Log Level Enumeration
//-----------------------------------------------------------------------------
typedef enum {
	LOG_LEVEL_DEBUG = 0,	// Detailed diagnostic information
	LOG_LEVEL_INFO,		// General informational messages
	LOG_LEVEL_WARN,		// Warning messages
	LOG_LEVEL_ERROR,	// Error messages
	LOG_LEVEL_NONE		// Disable all logging
} LogLevel;

//-----------------------------------------------------------------------------
// Public Interface
//-----------------------------------------------------------------------------

// Initialization and cleanup require a quiescent lifecycle: initialize before
// starting logging threads, and join them before cleanup. Configuration and log
// calls may run concurrently while the logger is initialized.
void log_init(void);
void log_cleanup(void);

// Configuration
void log_set_level(LogLevel level);
bool log_is_enabled(LogLevel level);
void log_set_console_output(bool enable);
void log_set_file_output(const char *path);
void log_set_array_output(bool enable);

//-----------------------------------------------------------------------------
// Logging Macros
//-----------------------------------------------------------------------------


#define LOG_DEBUG(fmt, ...) \
	do { \
		if (log_is_enabled(LOG_LEVEL_DEBUG)) \
			log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
	} while (0)

#define LOG_INFO(fmt, ...) \
	log_write(LOG_LEVEL_INFO, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
	log_write(LOG_LEVEL_WARN, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
	log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

//-----------------------------------------------------------------------------
// Backward Compatibility - Map old DEBUG_ADD macros to new system
//-----------------------------------------------------------------------------

#define DEBUG_ADD(msg)			LOG_DEBUG("%s", msg)
#define DEBUG_ADD_ERR(msg)		LOG_ERROR("%s", msg)
#define DEBUG_ADD_WAR(msg)		LOG_WARN("%s", msg)
#define DEBUG_ADD_FORMAT(fmt, ...)	LOG_DEBUG(fmt, ##__VA_ARGS__)
#define DEBUG_ADD_FORMAT_ERR(fmt, ...)	LOG_ERROR(fmt, ##__VA_ARGS__)
#define DEBUG_ADD_FORMAT_WAR(fmt, ...)	LOG_WARN(fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <source_location>
#include "SafeFormat.h"

//-----------------------------------------------------------------------------
// C++20 diagnostics seam
//-----------------------------------------------------------------------------
//
// LogSite captures the caller before typed arguments reach the shared sink.
// The macros retain their __FILE__/__LINE__ sites; both interfaces produce
// the same line. Function names are captured for observers, not printed.
struct LogSite
{
	const char	*file;
	int		line;
	const char	*function;	// NULL when only a file and a line were supplied

	LogSite(const std::source_location &location = std::source_location::current())
		: file(location.file_name()),
		  line((int)location.line()),
		  function(location.function_name())
	{
	}

	LogSite(const char *site_file, int site_line)
		: file(site_file),
		  line(site_line),
		  function(NULL)
	{
	}
};

// Argument types and count remain available until the format is interpreted.
// Diagnostic call sites use the same bounded conversion vocabulary as the UI.
void log_write_args(const LogSite& site, LogLevel level, const char* format,
	const SafeFormat::Arg* args, size_t count);

template <typename... Args>
void log_write_at(const LogSite& site, LogLevel level, const char* format, Args... args)
{
	const SafeFormat::Arg packed[] = {SafeFormat::MakeArg(args)..., SafeFormat::Arg()};
	log_write_args(site, level, format, packed, sizeof...(args));
}

template <typename... Args>
void log_write(LogLevel level, const char* file, int line, const char* format, Args... args)
{
	log_write_at(LogSite(file, line), level, format, args...);
}

//-----------------------------------------------------------------------------
// Test seam
//-----------------------------------------------------------------------------
//
// Reports the site of each log_write call, from either entry point, BEFORE the
// sink level filter. Filtered LOG_DEBUG macros skip the call and its arguments.
// A test can observe direct calls without initialising
// the logging system, moving the level or redirecting the output. The
// observer is NULL in every shipped build and nothing else about logging
// changes; tests/unit/test_source_location_diagnostics.cpp is the only user.
//-----------------------------------------------------------------------------
typedef void (*LogSiteObserver)(const LogSite &site, LogLevel level);

// Installs an observer and returns the one it replaced.
LogSiteObserver log_set_site_observer(LogSiteObserver observer);

#endif // __cplusplus

#endif // __DEBUG_LOG_H__
