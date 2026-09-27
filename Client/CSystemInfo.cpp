/********************************************************************
	created:	2003/10/17
	created:	17:10:2003   13:48
	filename: 	E:\designed\project\client\CSystemInfo.cpp
	file path:	E:\designed\project\client
	file base:	CSystemInfo
	file ext:	cpp
	author:		sonee

	purpose:	시스템 정보를 알아낸다.
				2003-10-17		CPU Clock 얻어오기
								MMX,SSE2 테크놀러지 Enable 여부
								Hyper Thread Enable 여부
*********************************************************************/
#include "Client_PCH.h"
#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#include <intrin.h>
#else
#include "../basic/Platform.h"
#endif
#include "CSystemInfo.h"

#ifdef PLATFORM_WINDOWS

#pragma warning( disable : 4035 )		// disable 시켜버리자-_-;
#pragma warning( disable: 4800 ) //'int' : forcing value to bool 'true' or 'false' (performance warning)

// The original VC6/x86 implementations below used inline __asm (raw RDTSC/
// CPUID opcodes, __try/__except around them to catch illegal-instruction
// faults on pre-Pentium CPUs). MSVC's x64 compiler has no inline assembler
// at all (error C4235), and CPUID/RDTSC have been unconditionally present
// on every CPU capable of running x64 code since the architecture's
// inception, so this is rewritten with the equivalent __cpuid/__rdtsc
// compiler intrinsics (<intrin.h>) and the illegal-instruction handling
// dropped as moot on x64.
inline unsigned __int64 theCycleCount(void)
{
	return __rdtsc();
}

static bool cpuid(unsigned long function, unsigned long& out_eax, unsigned long& out_ebx, unsigned long& out_ecx, unsigned long& out_edx)
{
	// This whole function sits inside the file's PLATFORM_WINDOWS block, so
	// only the MSVC intrinsic is ever compiled. An inline-asm x86 cpuid used
	// to sit here behind _LINUX, a macro nothing defined, and was carried
	// through the port's macro collapse as PLATFORM_POSIX - still unreachable,
	// and x86-only, so it is gone rather than left to trap the arm64 build.
	int info[4];
	__cpuid(info, (int)function);
	out_eax = (unsigned long)info[0];
	out_ebx = (unsigned long)info[1];
	out_ecx = (unsigned long)info[2];
	out_edx = (unsigned long)info[3];
	return true;
}

long CSystemInfo::GetCpuClock()
{
	unsigned __int64			start;
	unsigned __int64			overhead;

	start = theCycleCount();
	overhead = theCycleCount()-start;
	start = theCycleCount();
	Sleep(100);

	unsigned cpuspeed100 = (unsigned)( (theCycleCount()-start-overhead) / 1000 );
	return cpuspeed100 /100;
}

// --------------------------------------------------------------------------
bool CSystemInfo::CheckMMXTechnology()
{
	// See the comment above cpuid()/theCycleCount(): CPUID and the MMX
	// register state (EMMS) are unconditionally present on x86-64, so the
	// original __try/__except probing for their absence is dropped.
	int info[4];
	__cpuid(info, 1);      // 0 = vendor string, 1 = version info, 2 = cache info
	DWORD RegEDX = (DWORD)info[3];

	return (RegEDX & 0x800000) != 0;   // bit 23 is set for MMX technology
}


// --------------------------------------------------------------------------
// x86-64 mandates SSE/SSE2 support in hardware (part of the base ABI), so
// these always return true on any CPU capable of running this x64 build -
// see the comment above cpuid()/theCycleCount() for why the original
// __asm/CPUID/__try probing code (VC6, 32-bit only) no longer applies.
bool CSystemInfo::CheckSSETechnology(void)
{
	return true;
}

bool CSystemInfo::CheckSSE2Technology()
{
	return true;
}

// --------------------------------------------------------------------------
bool CSystemInfo::Check3DNowTechnology()
{
	int info[4];
	__cpuid(info, (int)0x80000000);        // highest supported AMD extended function
	unsigned long RegEAX = (unsigned long)info[0];

	if (RegEAX <= 0x80000000UL)
	{
		return false;                       // no AMD extended CPUID functions
	}

	__cpuid(info, (int)0x80000001);
	return ((unsigned long)info[3] >> 31) != 0;    // bit 31 of edx: 3DNow support
}

// Returns non-zero if Hyper-Threading Technology is supported on the processors and zero if not.  This does not mean that
// Hyper-Threading Technology is necessarily enabled.
bool CSystemInfo::CheckHyperThreadTechnology()
{
	const unsigned int HT_BIT		 = 0x10000000;  // EDX[28] - Bit 28 set indicates Hyper-Threading Technology is supported in hardware.
	const unsigned int FAMILY_ID     = 0x0f00;      // EAX[11:8] - Bit 11 thru 8 contains family processor id
	const unsigned int EXT_FAMILY_ID = 0x0f00000;	// EAX[23:20] - Bit 23 thru 20 contains extended family  processor id
	const unsigned int PENTIUM4_ID   = 0x0f00;		// Pentium 4 family processor id

	unsigned long unused,
				  reg_eax = 0, 
				  reg_edx = 0,
				  vendor_id[3] = {0, 0, 0};

	// verify cpuid instruction is supported
	if( !cpuid(0,unused, vendor_id[0],vendor_id[2],vendor_id[1]) 
	 || !cpuid(1,reg_eax,unused,unused,reg_edx) )
	 return false;

	//  Check to see if this is a Pentium 4 or later processor
	if (((reg_eax & FAMILY_ID) ==  PENTIUM4_ID) || (reg_eax & EXT_FAMILY_ID))
		if (vendor_id[0] == 'uneG' && vendor_id[1] == 'Ieni' && vendor_id[2] == 'letn')
			return (reg_edx & HT_BIT) != 0;	// Genuine Intel Processor with Hyper-Threading Technology

	return false;  // This is not a genuine Intel processor.
}
#else
// Non-Windows platforms (macOS/Linux) - Stub implementations

inline uint64_t theCycleCount(void)
{
    // Stub implementation - return 0
    return 0;
}

[[maybe_unused]] static bool cpuid(unsigned long function, unsigned long& out_eax, unsigned long& out_ebx, unsigned long& out_ecx, unsigned long& out_edx)
{
    (void)function;
    // Stub implementation - assume no special CPU features
    out_eax = out_ebx = out_ecx = out_edx = 0;
    return false;
}

long CSystemInfo::GetCpuClock()
{
    // Stub implementation - return a reasonable default
    return 2000; // Assume 2 GHz
}

bool CSystemInfo::CheckMMXTechnology()
{
    // Stub implementation - assume MMX is available on modern systems
    return true;
}

bool CSystemInfo::CheckSSETechnology()
{
    // Stub implementation - assume SSE is available on modern systems
    return true;
}

bool CSystemInfo::CheckSSE2Technology()
{
    // Stub implementation - assume SSE2 is available on modern systems
    return true;
}

bool CSystemInfo::CheckHyperThreadTechnology()
{
    // Stub implementation - assume no hyperthreading
    return false;
}

#endif // PLATFORM_WINDOWS
