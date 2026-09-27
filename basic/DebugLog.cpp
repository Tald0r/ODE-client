//-----------------------------------------------------------------------------
// DebugLog.cpp
//
// Lightweight, cross-platform logging system for Dark Eden client
// Implementation
//-----------------------------------------------------------------------------

#include "DebugLog.h"
#include "CrtCompat.h"
#include <string.h>
#include <time.h>
#include "Platform.h"

#ifdef PLATFORM_WINDOWS
	#include <sys/timeb.h>
#else
	#include <sys/time.h>
#endif

// Platform-specific includes
#ifdef PLATFORM_WINDOWS
	#define PLATFORM_LOCK_INITIALIZED 1
#endif

//-----------------------------------------------------------------------------
// Configuration
//-----------------------------------------------------------------------------
typedef struct {
	LogLevel level;
	bool output_to_console;
	bool output_to_file;
	bool output_to_array;
	char log_file[256];
	FILE *log_fp;
} LogConfig;

static LogConfig g_config = {
	LOG_LEVEL_INFO,		// Default level
	true,				// Console output enabled
	false,				// File output disabled
	false,				// Array output disabled
	"",					// Log file path
	NULL				// File handle
};

static bool g_initialized = false;

// Test seam (see DebugLog.h). NULL in every shipped build, and read without
// the lock for the same reason the level below is: it is only ever installed
// by a single-threaded test binary.
static LogSiteObserver g_site_observer = NULL;

//-----------------------------------------------------------------------------
// Thread safety
//-----------------------------------------------------------------------------
static CRITICAL_SECTION g_log_lock;

//-----------------------------------------------------------------------------
// Helper Functions
//-----------------------------------------------------------------------------

// Extract filename from full path (e.g., "/path/to/GameInit.cpp" -> "GameInit.cpp")
static const char* get_filename(const char *path) {
	if (path == NULL) return "unknown";

	const char *filename = strrchr(path, '/');
	if (filename == NULL) {
		filename = strrchr(path, '\\');	// Windows path separator
	}

	return (filename != NULL) ? (filename + 1) : path;
}

// Get level string
static const char* get_level_string(LogLevel level) {
	switch (level) {
		case LOG_LEVEL_DEBUG: return "DEBUG";
		case LOG_LEVEL_INFO:  return "INFO ";
		case LOG_LEVEL_WARN:  return "WARN ";
		case LOG_LEVEL_ERROR: return "ERROR";
		default:             return "UNKNOWN";
	}
}

// Get timestamp with milliseconds
// Format: "2024-01-20 23:45:12.123"
static void get_timestamp(char *buffer, size_t size) {
#ifdef PLATFORM_WINDOWS
	struct _timeb timebuf;
	_ftime_s(&timebuf);
	std::tm tm_value = {};
	Basic::LocalTime(&timebuf.time, &tm_value);
	const std::tm *tm_info = &tm_value;
	snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
			 tm_info->tm_year + 1900,
			 tm_info->tm_mon + 1,
			 tm_info->tm_mday,
			 tm_info->tm_hour,
			 tm_info->tm_min,
			 tm_info->tm_sec,
			 timebuf.millitm);
#else
	struct timeval tv;
	struct tm *tm_info;
	gettimeofday(&tv, NULL);
	tm_info = localtime(&tv.tv_sec);
	snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
			 tm_info->tm_year + 1900,
			 tm_info->tm_mon + 1,
			 tm_info->tm_mday,
			 tm_info->tm_hour,
			 tm_info->tm_min,
			 tm_info->tm_sec,
			 static_cast<long>(tv.tv_usec / 1000));
#endif
}

//-----------------------------------------------------------------------------
// Public Interface Implementation
//-----------------------------------------------------------------------------

void log_init(void) {
	if (g_initialized) {
		return;	// Already initialized
	}

	// Initialize lock FIRST
	InitializeCriticalSection(&g_log_lock);

	// Set default level based on build type
#ifdef _DEBUG
	g_config.level = LOG_LEVEL_INFO;
#else
	g_config.level = LOG_LEVEL_ERROR;	// Production: only errors
#endif

	g_initialized = true;

	// Log initialization message directly to stderr (avoiding lock)
	fprintf(stderr, "[DEBUG LOG] Logging system initialized (level: %s)\n",
			g_config.level == LOG_LEVEL_DEBUG ? "DEBUG" :
			g_config.level == LOG_LEVEL_INFO ? "INFO" :
			g_config.level == LOG_LEVEL_WARN ? "WARN" :
			g_config.level == LOG_LEVEL_ERROR ? "ERROR" : "NONE");
	fflush(stderr);
}

void log_cleanup(void) {
	if (!g_initialized) {
		return;
	}

	// Log shutdown message directly to stderr (avoiding potential lock issues)
	fprintf(stderr, "[DEBUG LOG] Logging system shutting down\n");
	fflush(stderr);

	// Close log file if open
	if (g_config.log_fp != NULL) {
		fclose(g_config.log_fp);
		g_config.log_fp = NULL;
	}

	// Cleanup lock
	DeleteCriticalSection(&g_log_lock);

	g_initialized = false;
}

void log_set_level(LogLevel level) {
	EnterCriticalSection(&g_log_lock);
	g_config.level = level;
	LeaveCriticalSection(&g_log_lock);
}

void log_set_console_output(bool enable) {
	EnterCriticalSection(&g_log_lock);
	g_config.output_to_console = enable;
	LeaveCriticalSection(&g_log_lock);
}

void log_set_file_output(const char *path) {
	EnterCriticalSection(&g_log_lock);

	// Close existing file if open
	if (g_config.log_fp != NULL) {
		fclose(g_config.log_fp);
		g_config.log_fp = NULL;
	}

	if (path != NULL && path[0] != '\0') {
		// Store path
		Basic::CopyBounded(g_config.log_file, path, sizeof(g_config.log_file) - 1);
		g_config.log_file[sizeof(g_config.log_file) - 1] = '\0';

		// Open file
		g_config.log_fp = Basic::OpenFile(g_config.log_file, "w");
		if (g_config.log_fp != NULL) {
			g_config.output_to_file = true;
		} else {
			// Failed to open file, fallback to console
			g_config.output_to_file = false;
		}
	} else {
		g_config.output_to_file = false;
		g_config.log_file[0] = '\0';
	}

	LeaveCriticalSection(&g_log_lock);
}

void log_set_array_output(bool enable) {
	EnterCriticalSection(&g_log_lock);
	g_config.output_to_array = enable;
	LeaveCriticalSection(&g_log_lock);
}

//-----------------------------------------------------------------------------
// Test Seam
//-----------------------------------------------------------------------------

LogSiteObserver log_set_site_observer(LogSiteObserver observer) {
	LogSiteObserver previous = g_site_observer;
	g_site_observer = observer;
	return previous;
}

//-----------------------------------------------------------------------------
// Core Logging Function
//-----------------------------------------------------------------------------

// The shared sink observes the call site before filtering and formatting.
void log_write_args(const LogSite& site, LogLevel level, const char* fmt,
	const SafeFormat::Arg* args, size_t count)
{
	// Test seam: reported before the level filter (see DebugLog.h).
	if (g_site_observer != NULL) {
		g_site_observer(site, level);
	}

	// Fast path: level filtering (no lock needed)
	if (level < g_config.level || !g_initialized) {
		return;
	}

	// Extract filename
	const char *filename = get_filename(site.file);
	const char *level_str = get_level_string(level);

	// Format message
	char message[2048];
	SafeFormat::FormatV(message, sizeof(message), fmt, args, count);

	// Build full log line
	char log_line[sizeof(message) + 256];
	char timestamp[64];
	get_timestamp(timestamp, sizeof(timestamp));

	snprintf(log_line, sizeof(log_line),
			 "[%s] [%s] [%s:%d] %s",
			 timestamp, level_str, filename, site.line, message);

	// Critical section for output
	EnterCriticalSection(&g_log_lock);

	// Output to console (stderr)
	if (g_config.output_to_console) {
		fprintf(stderr, "%s\n", log_line);
		fflush(stderr);
	}

	// Output to file
	if (g_config.output_to_file && g_config.log_fp != NULL) {
		fprintf(g_config.log_fp, "%s\n", log_line);
		fflush(g_config.log_fp);
	}


	LeaveCriticalSection(&g_log_lock);
}
