//----------------------------------------------------------------------
// MJusticeAttackManager.h
//----------------------------------------------------------------------
// Creatures the player may attack in self-defense.
// Membership is controlled by the server's add/remove packets.
//----------------------------------------------------------------------

#ifndef __MJUSTICE_ATTACK_MANAGER_H__
#define __MJUSTICE_ATTACK_MANAGER_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include <set>
#include <string>

class MJusticeAttackManager {
	public :
		MJusticeAttackManager();
		~MJusticeAttackManager();

		void		Release();
		
		// Null names are ignored and never match an entry.
		void		AddCreature(const char* pName);
		bool		RemoveCreature(const char* pName);
		bool		HasCreature(const char* pName) const;

	private :
		std::set<std::string>	m_Creatures;
};

extern MJusticeAttackManager* g_pJusticeAttackManager;


#endif
