#ifndef __DLL_INFO__
#define __DLL_INFO__

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// __EX marked the classes that were once built into a DLL of their own
// (dllexport there, dllimport in the game). They are compiled into the
// executable that uses them now, so it marks nothing: importing a symbol
// the same image defines is what MSVC's C4273 and LNK4217 report.
#define __EX

#endif
