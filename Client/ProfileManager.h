//----------------------------------------------------------------------
// ProfileManager.h
//----------------------------------------------------------------------
// 캐릭터들의 얼굴 정보를 관리한다.
//
// 실제로 관리되는건.. 캐릭얼굴 화일이 있다/없다/요청해라.. 정도이고,
// 있을 경우.. 화일 이름을 읽어서 외부에서 잘~ 사용하면 된다.
//
// 프로그램이 실행될때 기존에건 다 지울 예정이므로.. 관리는 별로 필요없다.
//
//
// [화일 관리 방법]
//
// 사용자가 *.bmp를 profile에 넣어둔다고 하고..
// 어떤 캐릭의 profile이 필요하면 '캐릭터.bmp'가 있을때
// '캐릭터.spr'을 생성해서 profile에 넣어두고.. 사용하면 된다.
//
// 지울때는 spr만 다 지우면 된다.
//
// Profile.Get(name)에서
//     내 client에 name.spr이 없다면..
//        다른 client에 요청을 해야한다.
//
//----------------------------------------------------------------------

#ifndef __PROFILE_MANAGER_H__
#define __PROFILE_MANAGER_H__


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

//----------------------------------------------------------------------
// ProfileManager
//----------------------------------------------------------------------
class ProfileManager {
	public :
		enum HAS_PROFILE
		{

		};

	public :
		typedef std::map<std::string, std::string>		PROFILE_MAP;

		
	public :
		ProfileManager();
		~ProfileManager();

		//-------------------------------------------------------------
		// Delete / Init Profiles - Profile 디렉토리 관리
		//-------------------------------------------------------------
		static void		DeleteProfiles();
		static void		InitProfiles();		

		//-------------------------------------------------------------
		// Release
		//-------------------------------------------------------------
		void				Release();		

		//-------------------------------------------------------------
		// Add / Remove Profile
		//-------------------------------------------------------------
		bool			HasProfile(const char* pName) const;
		void			AddProfile(const char* pName, const char* pFilename);		
		bool			RemoveProfile(const char* pName);


		//-------------------------------------------------------------
		// Get
		//-------------------------------------------------------------
		const char*		GetFilename(const char* pName) const;


		// The map used to be filled by peers too: RequestProfile queued a
		// name, Update dialled the peer for its profile file, and the file
		// manager added the result, or a "no profile" marker when the peer
		// had none. That outbound peer side was compiled out upstream and
		// is deleted (docs/RESTRUCTURING.md task 5.2, eighth slice), the
		// marker with it; the map now holds what InitProfiles found in the
		// profile directory.

	protected :
		//----------------------------------------------------------------------
		// Lock / Unlock
		//----------------------------------------------------------------------
		void		Lock()					{ EnterCriticalSection(&m_Lock); }
		void		Unlock()				{ LeaveCriticalSection(&m_Lock); }

	private :
		PROFILE_MAP		m_Profiles;


		CRITICAL_SECTION		m_Lock;
};

// Compile-time check to ensure CRITICAL_SECTION is fully defined
// Windows: the real <windows.h> struct is 24 bytes on x86, 40 bytes on x64
// (never 68 - an incomplete/forward-declared type would fail to compile
// sizeof() at all, not silently return a small value)
// POSIX/Emscripten: sizeof(CRITICAL_SECTION) = sizeof(pthread_mutex_t) + sizeof(int)
#ifdef PLATFORM_WINDOWS
	static_assert(sizeof(CRITICAL_SECTION) >= 24, "CRITICAL_SECTION is incomplete - Platform.h must be included before ProfileManager.h");
#else
	// For POSIX systems (including Emscripten), the size may vary
	// Just ensure it contains the mutex (basic sanity check)
	static_assert(sizeof(CRITICAL_SECTION) >= sizeof(int), "CRITICAL_SECTION is incomplete");
#endif

extern ProfileManager*		g_pProfileManager;


#endif



