//////////////////////////////////////////////////////////////////////////////
/// \file LeakMemoryDumper.h
/// \author sonee
/// \date 2004.12.30
//////////////////////////////////////////////////////////////////////////////

#pragma once

// This legacy leak tracker installs a *replaceable global* operator delete.
// Even though only LeakMemoryDumper.cpp includes this header, that definition
// applies program-wide at link time, so every delete in the process - the CRT's
// and the STL's included - is routed through RemoveTrack().
//
// It used to be gated on _DEBUG, which looked inert: CMakeLists.txt explicitly
// avoids defining _DEBUG and relies on DebugLog.h instead. But MSVC's debug CRT
// switch (/MDd) defines _DEBUG implicitly, so Windows Debug builds silently
// enabled all of this while macOS and Linux never did. The result was an
// access violation before main(): a delete running during static
// initialization iterated allocList before its constructor had run.
//
// Enable it deliberately with ENABLE_LEAK_TRACKER if you want the leak dump;
// note it makes every delete a linear scan of every live allocation.
#ifdef ENABLE_LEAK_TRACKER

void AddTrack(uintptr_t addr, size_t asize, const char *fname, DWORD lnum);
void DumpUnfreed();
void RemoveTrack(uintptr_t addr);

__inline void * __cdecl operator new(size_t size,
								   const char *file, int line)
{
	void *ptr = (void *)malloc(size);
	AddTrack((uintptr_t)ptr, size, file, line);
	return(ptr);
}

__inline void __cdecl operator delete(void *p)
{
	RemoveTrack((uintptr_t)p);
	free(p);
}

#endif

// Must track the guard above: without the tracker there is no
// operator new(size_t, const char *, int) to route DEBUG_NEW through.
#ifdef ENABLE_LEAK_TRACKER
	#define DEBUG_NEW new(__FILE__, __LINE__)
#else
	#define DEBUG_NEW new
#endif

#define new DEBUG_NEW

#ifdef _MSC_VER
#pragma warning(disable:4291)
#endif