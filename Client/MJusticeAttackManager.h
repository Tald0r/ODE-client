//----------------------------------------------------------------------
// MJusticeAttackManager.h
//----------------------------------------------------------------------
// 내가 정당방위로 공격할 수 있는 애덜이다.
//
// 서버에서 Add/Remove packet을 날려주기 때문에.. 걍... 냠냠..
//----------------------------------------------------------------------

#ifndef __MJUSTICE_ATTACK_MANAGER_H__
#define __MJUSTICE_ATTACK_MANAGER_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include <map>
#include <string>
#include "MonotonicClock.h"

class MJusticeAttackManager {
	public :
		// <이름, 최초의시간>		시간은 의미없지 싶은데.. 걍..
		typedef std::map<std::string, MonotonicClock::TimePoint>	TIME_MAP;

	public :
		MJusticeAttackManager();
		~MJusticeAttackManager();

		void		Release();
		
		void		AddCreature(const char* pName);
		bool		RemoveCreature(const char* pName);
		bool		HasCreature(const char* pName) const;

	private :
		TIME_MAP	m_Creatures;		// 나를 때린 애들이당.. 나쁜 애들~
};

extern MJusticeAttackManager* g_pJusticeAttackManager;


#endif

