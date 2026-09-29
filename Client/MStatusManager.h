//----------------------------------------------------------------------
// MStatusManager.h
//----------------------------------------------------------------------
//
// The client's own derivation of the combat stats from STR, DEX, INT and
// the weapon's skill domain. In game the server sends these values; the
// client computes them for the character-select slots, where it has
// only what the character list carries, and MPlayer::CalculateStatus
// takes the attack speed from it. A gamemodel member, so a unit test
// reaches it (tests/unit/test_status_manager.cpp).
//
//----------------------------------------------------------------------


#ifndef __MSTATUSMANAGER_H__
#define	__MSTATUSMANAGER_H__

class PCSlayerInfo;
class PCVampireInfo;
class PCOustersInfo;

class MStatusManager {
	public :
		MStatusManager();
		~MStatusManager();

		//--------------------------------------------------------------
		// The inputs of the computation
		//--------------------------------------------------------------
		void		Set(int str,int dex, int intel);

		//--------------------------------------------------------------
		// The skill domain of the weapon in hand and its level. For a
		// vampire or an ousters the domain is SKILLDOMAIN_VAMPIRE or
		// SKILLDOMAIN_OUSTERS and the level is the character's level.
		//--------------------------------------------------------------
		void		SetCurrentWeaponDomain(int domain, int level);

		//--------------------------------------------------------------
		// All the inputs at once, from a character-select slot.
		//--------------------------------------------------------------
		void		SetCharacterSelectSlot(const PCSlayerInfo& info);
		void		SetCharacterSelectSlot(const PCVampireInfo& info);
		void		SetCharacterSelectSlot(const PCOustersInfo& info);

		//--------------------------------------------------------------
		// The derived values
		//--------------------------------------------------------------
		int			GetTOHIT();
		int			GetMinDAM();		// min
		int			GetMaxDAM();		// max
		int			GetDefense();
		int			GetProtection();
		int			GetAttackSpeed();

	protected :
		int			m_STR;
		int			m_DEX;
		int			m_INT;

		int			m_Domain;
		int			m_DomainLevel;
};

extern MStatusManager		g_StatusManager;

#endif

