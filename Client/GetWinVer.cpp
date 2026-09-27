#include "Client_PCH.h"

#ifdef PLATFORM_WINDOWS
// True when the Windows version this process is told it runs on is at least
// major.minor with a build of at least build. That is the version
// GetVersionEx reported (shaped by the executable's manifest), and
// VerifyVersionInfo reads the same one; it compares major and minor
// together and the build on its own.
static bool ReportedVersionAtLeast(DWORD major, DWORD minor, DWORD build)
{
   OSVERSIONINFOEXW info = {};
   info.dwOSVersionInfoSize = sizeof(info);
   info.dwMajorVersion = major;
   info.dwMinorVersion = minor;
   info.dwBuildNumber = build;
   DWORDLONG mask = 0;
   mask = VerSetConditionMask(mask, VER_MAJORVERSION, VER_GREATER_EQUAL);
   mask = VerSetConditionMask(mask, VER_MINORVERSION, VER_GREATER_EQUAL);
   mask = VerSetConditionMask(mask, VER_BUILDNUMBER, VER_GREATER_EQUAL);
   return VerifyVersionInfoW(&info, VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER, mask) != FALSE;
}

// The largest value in [0, limit] for which atLeast(value) holds; atLeast
// must hold for 0 and be true up to some value and false after it.
template <typename Test>
static DWORD HighestSatisfying(DWORD limit, Test atLeast)
{
   DWORD low = 0, high = limit;
   while (low < high)
   {
      const DWORD middle = low + (high - low + 1) / 2;
      if (atLeast(middle))
         low = middle;
      else
         high = middle - 1;
   }
   return low;
}

// The major, minor and build GetVersionEx (deprecated) reported. An x64
// build runs only on the NT platform.
static void QueryReportedVersion(OSVERSIONINFOEX& osvi)
{
   const DWORD major = HighestSatisfying(0xFF, [](DWORD m) { return ReportedVersionAtLeast(m, 0, 0); });
   const DWORD minor = HighestSatisfying(0xFFFF, [major](DWORD m) { return ReportedVersionAtLeast(major, m, 0); });
   const DWORD build = HighestSatisfying(0x7FFFFFFF, [major, minor](DWORD b) { return ReportedVersionAtLeast(major, minor, b); });
   osvi.dwMajorVersion = major;
   osvi.dwMinorVersion = minor;
   osvi.dwBuildNumber = build;
   osvi.dwPlatformId = VER_PLATFORM_WIN32_NT;
}
#endif

BOOL GetWinVersion(char *szVersion, size_t nSize)
{
   if (szVersion == NULL || nSize == 0)
      return FALSE;

#ifdef PLATFORM_WINDOWS
   OSVERSIONINFOEX osvi;
   ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
   QueryReportedVersion(osvi);

   switch (osvi.dwPlatformId)
   {
      // Test for the Windows NT product family.
      case VER_PLATFORM_WIN32_NT:
         if ( osvi.dwMajorVersion == 10 && osvi.dwMinorVersion == 0 )
            snprintf(szVersion, nSize, "%s", "Windows 10/11");
         else if ( osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 3 )
            snprintf(szVersion, nSize, "%s", "Windows 8.1");
         else if ( osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 2 )
            snprintf(szVersion, nSize, "%s", "Windows 8");
         else if ( osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 1 )
            snprintf(szVersion, nSize, "%s", "Windows 7");
         else if ( osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 0 )
            snprintf(szVersion, nSize, "%s", "Windows Vista");
         else if ( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1 )
            snprintf(szVersion, nSize, "%s", "Windows XP");
         else
            snprintf(szVersion, nSize, "Windows NT %d.%d", osvi.dwMajorVersion, osvi.dwMinorVersion);
         break;

      // Test for the Windows 95 product family.
      case VER_PLATFORM_WIN32_WINDOWS:
         if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0)
             snprintf(szVersion, nSize, "%s", "Windows 95");
         else if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 10)
             snprintf(szVersion, nSize, "%s", "Windows 98");
         else if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 90)
             snprintf(szVersion, nSize, "%s", "Windows ME");
         else
             snprintf(szVersion, nSize, "%s", "Windows 9x");
         break;

      default:
         snprintf(szVersion, nSize, "%s", "Unknown Windows");
         break;
   }

   // Add build number if available
   if (osvi.dwBuildNumber > 0)
   {
      const size_t used = strlen(szVersion);
      if (used < nSize)
         snprintf(szVersion + used, nSize - used, " (Build %d)", osvi.dwBuildNumber & 0xFFFF);
   }

   return TRUE;

#else
   // Non-Windows platforms
   snprintf(szVersion, nSize, "%s", "Non-Windows Platform");
   return TRUE;
#endif
}
