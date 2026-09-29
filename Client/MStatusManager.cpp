//----------------------------------------------------------------------
// MStatusManager.cpp
//----------------------------------------------------------------------

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#else
#include "../basic/Platform.h"
#endif
#include <algorithm>
#include "MStatusManager.h"
#include "SkillDef.h"
#include "PCSlayerInfo.h"
#include "PCVampireInfo.h"
#include "PCOustersInfo.h"

#include "domain/Formulas.h"

//----------------------------------------------------------------------
// The client's own caps on the attack speed. The other stats take the
// server's rules and caps, from decore.
//----------------------------------------------------------------------
#define SLAYER_MAX_ATTACK_SPEED   35
#define VAMPIRE_MAX_ATTACK_SPEED  30
#define OUSTERS_MAX_ATTACK_SPEED  35

//----------------------------------------------------------------------
// global
//----------------------------------------------------------------------
MStatusManager		g_StatusManager;


//----------------------------------------------------------------------
// 
// constructor / destructor
//
//----------------------------------------------------------------------
MStatusManager::MStatusManager()
{
	m_STR = m_DEX = m_INT = 0;

	m_Domain = 0;
	m_DomainLevel = 0;

}

MStatusManager::~MStatusManager()
{
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
namespace {

enum StatRace { STAT_SLAYER, STAT_VAMPIRE, STAT_OUSTERS };

StatRace	RaceOf(int domain)
{
	switch (domain)
	{
		case SKILLDOMAIN_VAMPIRE :	return STAT_VAMPIRE;
		case SKILLDOMAIN_OUSTERS :	return STAT_OUSTERS;
		default :					return STAT_SLAYER;
	}
}

//----------------------------------------------------------------------
// The weapon family a slayer's skill domain stands for, as the server's
// AbilityBalance.cpp picks it from the item class: the cross is the heal
// domain's weapon, the mace the enchant domain's, and the gun domain's
// weapons are Arms. Bare hands (MAX_SKILLDOMAIN) are None; anything else
// is a weapon of no family the rules know, Other.
//----------------------------------------------------------------------
decore::WeaponFamily	WeaponFamilyOf(int domain)
{
	switch (domain)
	{
		case SKILLDOMAIN_SWORD :	return decore::WeaponFamily::Sword;
		case SKILLDOMAIN_BLADE :	return decore::WeaponFamily::Blade;
		case SKILLDOMAIN_HEAL :		return decore::WeaponFamily::Cross;
		case SKILLDOMAIN_ENCHANT :	return decore::WeaponFamily::Mace;
		case SKILLDOMAIN_GUN :		return decore::WeaponFamily::Arms;
		case MAX_SKILLDOMAIN :		return decore::WeaponFamily::None;
		default :					return decore::WeaponFamily::Other;
	}
}

//----------------------------------------------------------------------
// The inputs as the server's toStatAttr gathers them. A slayer's level
// is never read by its rules, and the character list does not carry
// one; a vampire's or an ousters' has no weapon domain.
//----------------------------------------------------------------------
decore::StatAttr	StatAttrOf(int str, int dex, int intel, int domain, int domainLevel)
{
	decore::StatAttr a = {};
	a.str = str;
	a.dex = dex;
	a.inte = intel;
	if (RaceOf(domain) == STAT_SLAYER)
	{
		a.level = 0;
		a.weapon = WeaponFamilyOf(domain);
		const bool hasDomain = a.weapon != decore::WeaponFamily::None
								&& a.weapon != decore::WeaponFamily::Other;
		a.weaponDomainLevel = hasDomain ? domainLevel : 0;
	}
	else
	{
		a.level = domainLevel;
		a.weapon = decore::WeaponFamily::None;
		a.weaponDomainLevel = 0;
	}
	return a;
}

// The server's combat damage bonus (VariableManager) is a server
// setting the client never sees; 0 is its default.
const int	kCombatDamageBonus = 0;

} // namespace

//----------------------------------------------------------------------
// To-hit, defense, protection and melee damage: the server's per-race
// rules (decore::slayerToHit and the rest, Formulas.cpp), capped at the
// server's 10000.
//----------------------------------------------------------------------
int 
MStatusManager::GetTOHIT()
{
	const decore::StatAttr a = StatAttrOf(m_STR, m_DEX, m_INT, m_Domain, m_DomainLevel);

	switch (RaceOf(m_Domain))
	{
		case STAT_VAMPIRE :	return decore::vampireToHit(a);
		case STAT_OUSTERS :	return decore::oustersToHit(a);
		default :			return decore::slayerToHit(a);
	}
}

int 
MStatusManager::GetDefense()
{
	const decore::StatAttr a = StatAttrOf(m_STR, m_DEX, m_INT, m_Domain, m_DomainLevel);

	switch (RaceOf(m_Domain))
	{
		case STAT_VAMPIRE :	return decore::vampireDefense(a);
		case STAT_OUSTERS :	return decore::oustersDefense(a);
		default :			return decore::slayerDefense(a);
	}
}

int 
MStatusManager::GetProtection()
{
	const decore::StatAttr a = StatAttrOf(m_STR, m_DEX, m_INT, m_Domain, m_DomainLevel);

	switch (RaceOf(m_Domain))
	{
		case STAT_VAMPIRE :	return decore::vampireProtection(a);
		case STAT_OUSTERS :	return decore::oustersProtection(a);
		default :			return decore::slayerProtection(a);
	}
}

int 
MStatusManager::GetMinDAM()
{
	const decore::StatAttr a = StatAttrOf(m_STR, m_DEX, m_INT, m_Domain, m_DomainLevel);

	switch (RaceOf(m_Domain))
	{
		case STAT_VAMPIRE :	return decore::vampireMinDamage(a, kCombatDamageBonus);
		case STAT_OUSTERS :	return decore::oustersMinDamage(a);
		default :			return decore::slayerMinDamage(a, kCombatDamageBonus);
	}
}

int 
MStatusManager::GetMaxDAM()
{
	const decore::StatAttr a = StatAttrOf(m_STR, m_DEX, m_INT, m_Domain, m_DomainLevel);

	switch (RaceOf(m_Domain))
	{
		case STAT_VAMPIRE :	return decore::vampireMaxDamage(a, kCombatDamageBonus);
		case STAT_OUSTERS :	return decore::oustersMaxDamage(a);
		default :			return decore::slayerMaxDamage(a, kCombatDamageBonus);
	}
}

//----------------------------------------------------------------------
// Attack Speed
//----------------------------------------------------------------------
//	Sword Slayer  : STR/10 + SwordDomainLevel/5
//	Blade Slayer  : STR/10 + BladeDomainLevel/5
//	Gun   Slayer  : DEX/10 + GunDomainLevel/5
//	Cleric Slayer : STR/10
//	Vampire       : DEX/10 + 10
//----------------------------------------------------------------------
int 
MStatusManager::GetAttackSpeed()
{
	int	value  = 0;

	switch (m_Domain)
	{
		//---------------------------------------------------
		// 맨손
		//---------------------------------------------------
		case MAX_SKILLDOMAIN :
//			value = m_STR/10;
			value = (std::min)(m_STR/10, SLAYER_MAX_ATTACK_SPEED);
		break;

		//---------------------------------------------------
		// 아우스터즈
		//---------------------------------------------------
		case SKILLDOMAIN_OUSTERS:
			value = (int)((m_DEX+m_DomainLevel)/10);
			value = (std::min)((int)value, OUSTERS_MAX_ATTACK_SPEED);
			break;

		//---------------------------------------------------
		// 뱀파이어
		//---------------------------------------------------
		case SKILLDOMAIN_VAMPIRE :
			value = (int)(m_DEX / 10 + 10);
			value = (std::min)((int)value, VAMPIRE_MAX_ATTACK_SPEED);
		break;

		//---------------------------------------------------
		// 메이스 / 십자가
		//---------------------------------------------------
		case SKILLDOMAIN_HEAL :
		case SKILLDOMAIN_ENCHANT :
//			value = (int)(m_STR / 10);
			value = (std::min)(m_STR/10, SLAYER_MAX_ATTACK_SPEED);
		break;

		//---------------------------------------------------
		// 총
		//---------------------------------------------------
		case SKILLDOMAIN_GUN :
//			value = (int)(m_DEX/10 + m_DomainLevel/5);
			value = (std::min)(m_DEX/10+m_DomainLevel/5, SLAYER_MAX_ATTACK_SPEED);
		break;

		//---------------------------------------------------
		// 검 / 도 
		//---------------------------------------------------
		default :
//			value = (int)(m_STR/10 + m_DomainLevel/5);
			value = (std::min)(m_STR/10+m_DomainLevel/5, SLAYER_MAX_ATTACK_SPEED);
		
	}

	return value;
}

/****************** Add by Sonic 2006.9.6 Start *****************/
void
MStatusManager::Set(int str, int dex, int intel)		
{

	m_STR = str;
	m_DEX = dex;
	m_INT = intel;
}


//--------------------------------------------------------------
// 현재 사용하는 무기의 domain level을 설정한다.
// Vampire는 domain level을 설정한다.
//--------------------------------------------------------------
void
MStatusManager::SetCurrentWeaponDomain(int domain, int level)
{
	m_Domain	 = domain;
	m_DomainLevel = level;
}
/****************** Add End by Sonic 2006.9.6 *******************/

//--------------------------------------------------------------
// The character-select slots (UI_SetCharacter). The character list
// carries the base STR, DEX and INT; a slayer's weapon shape and the
// six domain levels; a vampire's or ousters' level and experience.
//--------------------------------------------------------------
// The list's six domain levels are indexed by the wire's SkillDomain,
// which runs in SKILLDOMAIN's order.
static_assert((int)SKILL_DOMAIN_BLADE == (int)SKILLDOMAIN_BLADE
	&& (int)SKILL_DOMAIN_SWORD == (int)SKILLDOMAIN_SWORD
	&& (int)SKILL_DOMAIN_GUN == (int)SKILLDOMAIN_GUN
	&& (int)SKILL_DOMAIN_HEAL == (int)SKILLDOMAIN_HEAL
	&& (int)SKILL_DOMAIN_ENCHANT == (int)SKILLDOMAIN_ENCHANT
	&& (int)SKILL_DOMAIN_ETC == (int)SKILLDOMAIN_ETC,
	"PCSlayerInfo's domain levels are read by SKILLDOMAIN");

void
MStatusManager::SetCharacterSelectSlot(const PCSlayerInfo& info)
{
	// The skill domain of each weapon shape. The cross is the heal
	// domain's weapon and the mace the enchant domain's, as the server's
	// AbilityBalance.cpp (domainLevelOf) and MPlayer::CalculateStatus
	// have it.
	static const SKILLDOMAIN weaponDomain[WEAPON_MAX] =
	{
		MAX_SKILLDOMAIN,          // WEAPON_NONE (0)
		SKILLDOMAIN_SWORD,        // WEAPON_SWORD (1)
		SKILLDOMAIN_SWORD,        // WEAPON_SWORD1 (2)
		SKILLDOMAIN_BLADE,        // WEAPON_BLADE (3)
		SKILLDOMAIN_BLADE,        // WEAPON_BLADE1 (4)
		SKILLDOMAIN_GUN,          // WEAPON_SR (5)
		SKILLDOMAIN_GUN,          // WEAPON_SR1 (6)
		SKILLDOMAIN_GUN,          // WEAPON_SR2 (7)
		SKILLDOMAIN_GUN,          // WEAPON_SR3 (8)
		SKILLDOMAIN_GUN,          // WEAPON_AR (9)
		SKILLDOMAIN_GUN,          // WEAPON_AR1 (10)
		SKILLDOMAIN_GUN,          // WEAPON_AR2 (11)
		SKILLDOMAIN_GUN,          // WEAPON_AR3 (12)
		SKILLDOMAIN_GUN,          // WEAPON_SG (13)
		SKILLDOMAIN_GUN,          // WEAPON_SMG (14)
		SKILLDOMAIN_HEAL,         // WEAPON_CROSS (15)
		SKILLDOMAIN_HEAL,         // WEAPON_CROSS1 (16)
		SKILLDOMAIN_ENCHANT,      // WEAPON_MACE (17)
		SKILLDOMAIN_ENCHANT,      // WEAPON_MACE1 (18)
	};

	// The weapon's domain at the character's level in it; bare hands
	// have no domain level, as on the server.
	const WeaponType weaponType = info.getWeaponType();
	if (weaponType >= 0 && weaponType < WEAPON_MAX)
	{
		const SKILLDOMAIN domain = weaponDomain[weaponType];
		const int level = domain < SKILLDOMAIN_ETC ? info.getSkillDomainLevel((SkillDomain)domain) : 0;
		SetCurrentWeaponDomain( domain, level );
	}
	else
	{
		// Fallback to ETC domain if weapon type is invalid
		SetCurrentWeaponDomain( SKILLDOMAIN_ETC, 0 );
	}

	Set(info.getSTR(), info.getDEX(), info.getINT());
}

void
MStatusManager::SetCharacterSelectSlot(const PCVampireInfo& info)
{
	SetCurrentWeaponDomain( SKILLDOMAIN_VAMPIRE, info.getLevel() );
	Set(info.getSTR(), info.getDEX(), info.getINT());
}

void
MStatusManager::SetCharacterSelectSlot(const PCOustersInfo& info)
{
	SetCurrentWeaponDomain( SKILLDOMAIN_OUSTERS, info.getLevel() );
	Set(info.getSTR(), info.getDEX(), info.getINT());
}