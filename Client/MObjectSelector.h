//----------------------------------------------------------------------
// MObjectSelector.h
//----------------------------------------------------------------------
// Whom the player may select and attack: everyone, friends or enemies, by
// race or by guild, with forced attack, hallucination, self-defense
// (justice) targets, NPCs, war and siege enemies and operators as the
// exceptions (task 4.225, gamemodel's since then). What it asks of the
// game goes through MObjectSelectorHost; creatures stay the game's,
// passed back opaque.
//----------------------------------------------------------------------
#ifndef __MOBJECTSELECTOR__
#define __MOBJECTSELECTOR__

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include "../basic/Platform.h"
#endif
#include "MEffectStatusDef.h"

class MCreature;
class MJusticeAttackManager;
class MWarManager;

struct MObjectSelectorHost
{
	// The player.
	bool (*ForceAttack)() = nullptr;			// left shift held
	bool (*PlayerHasEffectStatus)(EFFECTSTATUS status) = nullptr;
	bool (*PlayerCanAttackTribe)(MCreature* pCreature) = nullptr;
	bool (*PlayerCanAttackGuild)(MCreature* pCreature) = nullptr;
	int (*PlayerCreatureType)() = nullptr;
	int (*PlayerGuildNumber)() = nullptr;
	int (*PlayerUnionGuildID)() = nullptr;
	bool (*IsNonPK)() = nullptr;				// the user's non-PK option

	// A creature.
	const char* (*GetName)(MCreature* pCreature) = nullptr;
	bool (*IsNPC)(MCreature* pCreature) = nullptr;
	int (*GetGuildNumber)(MCreature* pCreature) = nullptr;
	int (*GetUnionGuildID)(MCreature* pCreature) = nullptr;
	bool (*HasEffectStatus)(MCreature* pCreature, EFFECTSTATUS status) = nullptr;
	// A creature object whose effect status can be read (the siege check).
	bool (*HasAvailableEffectStatus)(MCreature* pCreature) = nullptr;
	// Its creature type's first sprite is one only players wear.
	bool (*HasPlayerOnlySprite)(MCreature* pCreature) = nullptr;

	// The world.
	bool (*HasZone)() = nullptr;
	int (*GetZoneID)() = nullptr;
	int (*GetZonePKType)() = nullptr;
	MJusticeAttackManager* (*JusticeAttackManager)() = nullptr;
	MWarManager* (*WarManager)() = nullptr;
};

class MObjectSelector {
	public :
		enum SELECT_TARGET
		{
			SELECT_ALL,
			SELECT_FRIEND,
			SELECT_ENEMY
		};

		enum SELECT_BY
		{
			SELECT_BY_RACE,
			SELECT_BY_GUILD,
		};

	public :
		// Without a host, nothing can be selected or attacked.
		explicit MObjectSelector(const MObjectSelectorHost* pHost = nullptr);
		~MObjectSelector();

		//------------------------------------------------------
		// Select
		//------------------------------------------------------
		void	SelectAll()			{ m_SelectTarget = SELECT_ALL; }		
		void	SelectFriend()		{ m_SelectTarget = SELECT_FRIEND; }
		void	SelectEnemy()		{ m_SelectTarget = SELECT_ENEMY; }

		void	SelectByRace()		{ m_SelectBy = SELECT_BY_RACE; }
		void	SelectByGuild()		{ m_SelectBy = SELECT_BY_GUILD; }

		//------------------------------------------------------
		// Can Select?
		//------------------------------------------------------
		BOOL	CanSelect(MCreature* pCreature);
		BOOL	CanAttack(MCreature* pCreature);
		BOOL	IsWarEnemy(MCreature* pCreature);

	protected :
		const MObjectSelectorHost*	m_pHost;
		SELECT_TARGET		m_SelectTarget;
		SELECT_BY			m_SelectBy;
};

extern MObjectSelector* g_pObjectSelector;

#endif
