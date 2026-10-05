//----------------------------------------------------------------------
// Profiler.h
//----------------------------------------------------------------------
// 프로파일링... 냠..
//
//
// < 필요한 정보 >
//
// 이름						: 내용이기도 하고, ID이다.
// StartTime				: 이번 loop의 시작 시간
// TotalTime				: 전체 진행 시간
// Times					: 몇 번 반복을 했는가?
// 
//----------------------------------------------------------------------

#ifndef __PROFILER_H__
#define __PROFILER_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include "../basic/Platform.h"
#endif
#include <map>
#include <string>
#include <fstream>
#include "DebugLog.h"
#include "MonotonicClock.h"


//----------------------------------------------------------------------
// ProfilerInfo
//----------------------------------------------------------------------
class ProfilerInfo
{
	public :
		ProfilerInfo();
		~ProfilerInfo();

		void		Clear();

		//---------------------------------------------------------------
		// Begin / End
		//---------------------------------------------------------------
		void		Begin();
		void		End();

		//---------------------------------------------------------------
		// Get
		//---------------------------------------------------------------
		DWORD		GetTimes() const		{ return m_Times; }
		DWORD		GetTotalTime() const	{ return m_TotalTime; }
		float		GetAverageTime() const	{ return (float)m_TotalTime / m_Times; }

		//---------------------------------------------------------------
		// File Out
		//---------------------------------------------------------------
		void		WriteToFile(std::ofstream& file) const;

	private :
		MonotonicClock::TimePoint	m_StartTime;	// start of the open pass; the epoch when none is
		DWORD		m_TotalTime;		// milliseconds over every closed pass
		DWORD		m_Times;			// closed passes
};

//----------------------------------------------------------------------
// Profiler
//----------------------------------------------------------------------
class Profiler {
	public :
		typedef std::map< std::string, ProfilerInfo* >	PROFILE_MAP;

	public :
		Profiler();
		~Profiler();

		void			Release();

		//---------------------------------------------------------------
		// Begin / End
		//---------------------------------------------------------------
		void			Begin(const char* pName);
		void			End(const char* pName);

		//---------------------------------------------------------------
		// Get
		//---------------------------------------------------------------
		bool			HasProfileInfo(const char* pName) const;
		DWORD			GetTimes(const char* pName) const;
		DWORD			GetTotalTime(const char* pName) const;
		float			GetAverageTime(const char* pName) const;

		//---------------------------------------------------------------
		// Iterating
		//---------------------------------------------------------------
		int							GetNumber() const	{ return static_cast<int>(m_mapProfile.size()); }
		PROFILE_MAP::const_iterator GetBegin() const	{ return m_mapProfile.begin(); }

		//---------------------------------------------------------------
		// File Out
		//---------------------------------------------------------------
		void		WriteToFile(const char* pFilename, bool bAppend=false) const;

	protected :
		PROFILE_MAP		m_mapProfile;
};

extern Profiler* g_pProfiler;

#ifdef OUTPUT_DEBUG
	#define __BEGIN_PROFILE(name)	if (g_pProfiler!=NULL) g_pProfiler->Begin(name);
	#define __END_PROFILE(name)		if (g_pProfiler!=NULL) g_pProfiler->End(name);	
#else
	#define __BEGIN_PROFILE(name)	((void)0);
	#define __END_PROFILE(name)		((void)0);
#endif

#endif

