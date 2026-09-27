// Raw text transport for the optional external MinTrace window.
#ifndef ___MIN_TRACE___
#define ___MIN_TRACE___
#include "Platform.h"
#include <stdio.h>
#include <string.h>

#ifdef PLATFORM_WINDOWS
inline void _MinTraceA(LPCSTR p)
{
    COPYDATASTRUCT cd;
    HWND hWnd = ::FindWindowA("__MinTrace Window__", "MinTrace 2003");
    if (hWnd)
    {
        cd.dwData = 0;
        cd.cbData = static_cast<DWORD>((strlen(p)+1)*sizeof(char));
        cd.lpData = (void *)p;
        ::SendMessage (hWnd, WM_COPYDATA, 0, (LPARAM)&cd);
    }
}
#else
inline void _MinTraceA(LPCSTR p)
{
    // Non-Windows: output to stderr
    fprintf(stderr, "[MinTrace] %s\n", p ? p : "");
}
#endif

#endif
