//----------------------------------------------------------------------
// MObjectSelector.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MObjectSelector.h"
#include "MJusticeAttackManager.h"
#include "MWarManager.h"
#include "GuildWarInfo.h"
#include "MZoneTable.h"
#include "MCreatureTable.h"

//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
MObjectSelector* g_pObjectSelector = NULL;

//----------------------------------------------------------------------
//
// constructor / destructor
//
//----------------------------------------------------------------------
MObjectSelector::MObjectSelector(const MObjectSelectorHost* pHost)
	: m_pHost(pHost)
{
	m_SelectTarget	= SELECT_ALL;
	m_SelectBy		= SELECT_BY_RACE;
}

MObjectSelector::~MObjectSelector()
{
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
namespace {

bool IsOperator(int creatureType)
{
	return creatureType == CREATURETYPE_SLAYER_OPERATOR
		|| creatureType == CREATURETYPE_VAMPIRE_OPERATOR
		|| creatureType == CREATURETYPE_OUSTERS_OPERATOR;
}

} // namespace

//----------------------------------------------------------------------
// Can Select (MCreature*)
//----------------------------------------------------------------------
// Anyone, on a forced attack, under hallucination or when selecting all.
// By race (or guild): enemies selects those the player may attack by
// race (or guild), friends those it may not. Also anyone the player may
// attack in self-defense, any NPC, a war or siege enemy, and anyone at
// all for an operator. (A non-PK exception, added in 2004, was commented
// out.)
//----------------------------------------------------------------------
BOOL
MObjectSelector::CanSelect(MCreature* pCreature)
{
	if (m_pHost == NULL)
		return FALSE;

	const MObjectSelectorHost& host = *m_pHost;
	return host.ForceAttack()
			|| host.PlayerHasEffectStatus(EFFECTSTATUS_HALLUCINATION)
			|| m_SelectTarget==SELECT_ALL
			|| (m_SelectBy==SELECT_BY_RACE 
					&& ((m_SelectTarget==SELECT_ENEMY && host.PlayerCanAttackTribe( pCreature ))
						|| (m_SelectTarget==SELECT_FRIEND && !host.PlayerCanAttackTribe( pCreature ))
						)
				)
			|| (m_SelectBy==SELECT_BY_GUILD
					&& ((m_SelectTarget==SELECT_ENEMY && host.PlayerCanAttackGuild( pCreature ))
						|| (m_SelectTarget==SELECT_FRIEND && !host.PlayerCanAttackGuild( pCreature ))
						)
				)
			|| host.JusticeAttackManager()->HasCreature( host.GetName(pCreature) )
			|| host.IsNPC(pCreature)
			|| IsWarEnemy( pCreature )
			|| IsOperator(host.PlayerCreatureType());
}

//----------------------------------------------------------------------
// Can Attack (MCreature*)
//----------------------------------------------------------------------
// On a forced attack, under hallucination, whom the player may attack by
// race (or guild), a self-defense target or a war or siege enemy. Under
// the non-PK option, never a creature wearing a player-only sprite that
// is not an NPC (2004).
//----------------------------------------------------------------------
BOOL	
MObjectSelector::CanAttack(MCreature* pCreature)
{
	if (m_pHost == NULL || pCreature == NULL)
		return FALSE;

	const MObjectSelectorHost& host = *m_pHost;
	bool	IsAvailablePK = true;
	if (host.IsNonPK()
		&& host.HasPlayerOnlySprite(pCreature)
		&& !host.IsNPC(pCreature))
	{
		IsAvailablePK = false;
	}

	return (
				host.ForceAttack()
				|| host.PlayerHasEffectStatus(EFFECTSTATUS_HALLUCINATION)
				|| (m_SelectBy==SELECT_BY_RACE 
						&& host.PlayerCanAttackTribe(pCreature))
				|| (m_SelectBy==SELECT_BY_GUILD
						&& host.PlayerCanAttackGuild(pCreature))
				|| host.JusticeAttackManager()->HasCreature( host.GetName(pCreature) )
				|| IsWarEnemy( pCreature )
			)
			&& IsAvailablePK;
}

//--------------------------------------------------------------
// IsWarEnemy - an enemy in a war?
//--------------------------------------------------------------
// In a zone where all may attack all: anyone. In a siege zone, by the
// siege sides the effect statuses mark:
// - a defender (or reinforcement) fights whoever is neither;
// - the same union guild is never an enemy;
// - an attacker of side N fights whoever is not of side N.
// Otherwise, while the zone's castle is at war between guilds: if the
// player's guild has joined, every other guild; if not, only the guilds
// that have.
//--------------------------------------------------------------
BOOL	
MObjectSelector::IsWarEnemy(MCreature* pCreature)
{
	BOOL bWarSelect = FALSE;

	if (m_pHost == NULL || pCreature == NULL || !m_pHost->HasZone())
		return bWarSelect;

	const MObjectSelectorHost& host = *m_pHost;
	const WORD CurrentZoneID = static_cast<WORD>(host.GetZoneID());
	const int pkType = host.GetZonePKType();

	if (pkType == PK_TYPE_ALL)
		return TRUE;
	else if (pkType == PK_TYPE_SIEGE)
	{
		// 2004-11-08: the creature must really be one whose effect status
		// can be read.
		if (!host.HasAvailableEffectStatus(pCreature))
			return FALSE;

		// A defender's siege.
		if (host.PlayerHasEffectStatus(EFFECTSTATUS_SIEGE_DEFENDERL)
			|| host.PlayerHasEffectStatus(EFFECTSTATUS_SIEGE_REINFORCE))
		{
			if (!host.HasEffectStatus(pCreature, EFFECTSTATUS_SIEGE_DEFENDERL)
				&& !host.HasEffectStatus(pCreature, EFFECTSTATUS_SIEGE_REINFORCE))
				return TRUE;
		}

		if (host.PlayerUnionGuildID() > 0
			&& host.PlayerUnionGuildID() == host.GetUnionGuildID(pCreature))
		{
			return FALSE;
		}

		static const EFFECTSTATUS attackers[] = {
			EFFECTSTATUS_SIEGE_ATTACKER_1,
			EFFECTSTATUS_SIEGE_ATTACKER_2,
			EFFECTSTATUS_SIEGE_ATTACKER_3,
			EFFECTSTATUS_SIEGE_ATTACKER_4,
			EFFECTSTATUS_SIEGE_ATTACKER_5,
		};
		for (EFFECTSTATUS side : attackers)
		{
			if (host.PlayerHasEffectStatus(side) && !host.HasEffectStatus(pCreature, side))
				return TRUE;
		}
	}

	MWarManager* pWarManager = host.WarManager();
	if (pWarManager->IsExist( CurrentZoneID ) && pWarManager->getSize() > 0)
	{
		// Only the zone's own castle is searched.
		WarInfo *info = pWarManager->GetWarInfo( CurrentZoneID );
		if (info == NULL)
			return FALSE;

		if (info->getWarType() == WAR_GUILD)
		{
			GuildWarInfo *pInfo = static_cast<GuildWarInfo*>(info);
			int myGuildID = host.PlayerGuildNumber();
			GuildWarInfo::GuildIDList joinGuild = pInfo->getJoinGuilds();
			std::list<GuildID_t>::const_iterator	itr = joinGuild.Begin();
			// Whether the creature's guild has joined, when the player's
			// has not.
			bool	creatureGuildCheck = false;

			// The player's guild has joined: every other guild is an enemy.
			for (int i=0; i<joinGuild.getSize(); i++, itr++)
			{
				if (*itr == myGuildID)
				{
					if (host.GetGuildNumber(pCreature) != myGuildID)
						bWarSelect = TRUE;
					break;
				}
				if (*itr == host.GetGuildNumber(pCreature))
					creatureGuildCheck = true;
			}

			// It has not: only the guilds that have.
			if (bWarSelect == FALSE && creatureGuildCheck == true)
				bWarSelect = TRUE;
		}
	}

	return bWarSelect;
}
