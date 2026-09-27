//----------------------------------------------------------------------
// MSkillInfoTable.cpp
//----------------------------------------------------------------------
// Every value but the icon fields comes from the server through
// SkillInfo.inf (SKILLINFO_NODE::LoadFromFileServerSkillInfo). Code
// supplies only the per-session reset in Init(), the helicopter cast
// delay, and the range overrides and level-150 skills at the end of
// LoadFromFileServerSkillInfo.
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MSkillManager.h"

#ifndef __NEW_SKILL__
	#define __NEW_SKILL__
	//#undef  __NEW_SKILL__
#endif
//----------------------------------------------------------------------
//
//						MSkillInfoTable
//
//----------------------------------------------------------------------
MSkillInfoTable::MSkillInfoTable()
{
	// One slot per skill.
	CTypeTable<SKILLINFO_NODE>::Init( MIN_RESULT_ACTIONINFO );

	// The server's in-code skill definitions (2,300 lines under
	// #ifndef __GAME_CLIENT__, never compiled into the client) were
	// deleted in 2026-09 (docs/RESTRUCTURING.md task 5.2); git history
	// keeps them. The client fills the table from SkillInfo.inf via
	// LoadFromFileServerSkillInfo.

	//----------------------------------------------------------------------
	// Reset the per-session state (levels, experience, delays)
	//----------------------------------------------------------------------
	Init();
}

MSkillInfoTable::~MSkillInfoTable()
{
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Init
//----------------------------------------------------------------------
void
MSkillInfoTable::Init()
{
	const int size = GetSize();
	for (int i = 0; i < size; i++)
	{
		m_pTypeInfo[i].SetExpLevel( 0 );
		m_pTypeInfo[i].SetSkillExp( 0 );
		m_pTypeInfo[i].SetDelayTime( 0 );
		m_pTypeInfo[i].SetAvailableTime();
		m_pTypeInfo[i].SetEnable();
	}

	
	// Cast delays
	if (SUMMON_HELICOPTER < size)
	{
		m_pTypeInfo[SUMMON_HELICOPTER].SetDelayTime( 3000 );
	}
//	m_pTypeInfo[MAGIC_BLOODY_TUNNEL].SetDelayTime( 3000 );
//	m_pTypeInfo[MAGIC_BLOODY_MARK].SetDelayTime( 3000 );
}
//----------------------------------------------------------------------
// Use English Names
//----------------------------------------------------------------------
// SkillInfo.inf carries two names per skill: the English one the skill's
// description file is named after (GetName) and the display name in the
// data's own language (GetHName), which the learn-skill message, the
// skill window and the status effect list show. The shipped data's
// display names are Chinese, so in English the display name is the
// English name.
//----------------------------------------------------------------------
void
MSkillInfoTable::UseEnglishNames()
{
	const int size = GetSize();
	for (int i = 0; i < size; i++)
	{
		const char* pName = m_pTypeInfo[i].GetName();
		if (pName != NULL && pName[0] != '\0')
		{
			m_pTypeInfo[i].SetHName( pName );
		}
	}
}

//----------------------------------------------------------------------
// Save From File  ServerSkillInfo
//----------------------------------------------------------------------
void			
MSkillInfoTable::SaveFromFileServerSkillInfo(std::ofstream& file)
{
	int num =MAGIC_BLOODY_TUNNEL_INTO-1;
	file.write((char*)&num, 4);
	for (int i=0;i<num;i++)
	{
		file.write((char*)&i, 4);
		m_pTypeInfo[i].SaveFromFileServerSkillInfo( file );
	}
}
//----------------------------------------------------------------------
// Load From File  ServerSkillInfo
//----------------------------------------------------------------------
void			
MSkillInfoTable::LoadFromFileServerSkillInfo(std::ifstream& file)
{
	int num, skillType;

	file.read((char*)&num, 4);

	for (int i=0; i<num; i++)
	{
		file.read((char*)&skillType, 4);

		// Load into the slot the file names.
		m_pTypeInfo[skillType].LoadFromFileServerSkillInfo( file );
	}

	m_pTypeInfo[MAGIC_THROW_HOLY_WATER].minRange = 8;		// range in tiles
	m_pTypeInfo[MAGIC_THROW_HOLY_WATER].maxRange = 8;		// range in tiles

	m_pTypeInfo[BOMB_SPLINTER].minRange = 6;		// range in tiles
	m_pTypeInfo[BOMB_SPLINTER].maxRange = 6;		// range in tiles

	m_pTypeInfo[BOMB_ACER].minRange = 6;		// range in tiles
	m_pTypeInfo[BOMB_ACER].maxRange = 6;		// range in tiles

	m_pTypeInfo[BOMB_BULLS].minRange = 6;		// range in tiles
	m_pTypeInfo[BOMB_BULLS].maxRange = 6;		// range in tiles

	m_pTypeInfo[BOMB_STUN].minRange = 6;		// range in tiles
	m_pTypeInfo[BOMB_STUN].maxRange = 6;		// range in tiles

	m_pTypeInfo[BOMB_CROSSBOW].minRange = 6;		// range in tiles
	m_pTypeInfo[BOMB_CROSSBOW].maxRange = 6;		// range in tiles

	m_pTypeInfo[BOMB_TWISTER].minRange = 6;		// range in tiles
	m_pTypeInfo[BOMB_TWISTER].maxRange = 6;		// range in tiles

	m_pTypeInfo[MAGIC_RAPID_GLIDING].minRange = 2;
	m_pTypeInfo[MAGIC_RAPID_GLIDING].maxRange = 6;
	
	m_pTypeInfo[SKILL_ULTIMATE_BLOW].minRange = 1;
	m_pTypeInfo[SKILL_ULTIMATE_BLOW].maxRange = 3;
	// add by Coffee 2007-2-25 (Bloody Scarify and the other level-150 skills)
// 	SKILLDOMAIN_BLADE,
// 	SKILLDOMAIN_SWORD,
// 	SKILLDOMAIN_GUN,
// 	SKILLDOMAIN_HEAL,
// 	SKILLDOMAIN_ENCHANT,	
// 	SKILLDOMAIN_ETC,
// 	SKILLDOMAIN_VAMPIRE,	
// 	SKILLDOMAIN_OUSTERS,

	//modify by viva : Notice
#ifdef __NEW_SKILL__
	m_pTypeInfo[SKILL_BLLODY_SCARIFY].Set(150, "Bloody Scarify", 0, 0, 505, "血之烙印");
	m_pTypeInfo[SKILL_BLLODY_SCARIFY].DomainType = SKILLDOMAIN_VAMPIRE;
	m_pTypeInfo[SKILL_BLLODY_SCARIFY].minDamage =60;
	m_pTypeInfo[SKILL_BLLODY_SCARIFY].maxDamage =200;
	m_pTypeInfo[SKILL_BLLODY_SCARIFY].minDelay =300;
	m_pTypeInfo[SKILL_BLLODY_SCARIFY].maxDelay =450;
	m_pTypeInfo[SKILL_BLLODY_SCARIFY].minDuration =700;
	m_pTypeInfo[SKILL_BLLODY_SCARIFY].maxDuration =700;
	m_pTypeInfo[SKILL_BLLODY_SCARIFY].SetMP(120);
	m_pTypeInfo[SKILL_BLLODY_SCARIFY].minRange=6;
	m_pTypeInfo[SKILL_BLLODY_SCARIFY].maxRange=6;
	m_pTypeInfo[SKILL_BLLODY_SCARIFY].SetLearnLevel(150);

	m_pTypeInfo[SKILL_BLOOD_CURSE].Set(150, "Blood Curse", 0, 0, 506, "血之诅咒");
	m_pTypeInfo[SKILL_BLOOD_CURSE].DomainType = SKILLDOMAIN_VAMPIRE;
	m_pTypeInfo[SKILL_BLOOD_CURSE].minDamage =60;
	m_pTypeInfo[SKILL_BLOOD_CURSE].maxDamage =220;
	m_pTypeInfo[SKILL_BLOOD_CURSE].minDelay =450;
	m_pTypeInfo[SKILL_BLOOD_CURSE].maxDelay =450;
	m_pTypeInfo[SKILL_BLOOD_CURSE].minDuration =200;
	m_pTypeInfo[SKILL_BLOOD_CURSE].maxDuration =200;
	m_pTypeInfo[SKILL_BLOOD_CURSE].SetMP(60);
	m_pTypeInfo[SKILL_BLOOD_CURSE].minRange=6;
	m_pTypeInfo[SKILL_BLOOD_CURSE].maxRange=6;
	m_pTypeInfo[SKILL_BLOOD_CURSE].SetLearnLevel(150);

	m_pTypeInfo[SKILL_SHINE_SWORD].Set(150, "Shine Sword", 700, 270,  497,"闪耀之剑");
	m_pTypeInfo[SKILL_SHINE_SWORD].DomainType = SKILLDOMAIN_SWORD;
	m_pTypeInfo[SKILL_SHINE_SWORD].minDamage =60;
	m_pTypeInfo[SKILL_SHINE_SWORD].maxDamage =165;
	m_pTypeInfo[SKILL_SHINE_SWORD].minDelay =100;
	m_pTypeInfo[SKILL_SHINE_SWORD].maxDelay =450;
	m_pTypeInfo[SKILL_SHINE_SWORD].minDuration =200;
	m_pTypeInfo[SKILL_SHINE_SWORD].maxDuration =200;
	m_pTypeInfo[SKILL_SHINE_SWORD].SetMP(50);
	m_pTypeInfo[SKILL_SHINE_SWORD].minRange=7;
	m_pTypeInfo[SKILL_SHINE_SWORD].maxRange=7;
	m_pTypeInfo[SKILL_SHINE_SWORD].SetLearnLevel(150);


	m_pTypeInfo[SKILL_BOMB_CRASH_WALK].Set(150, "Bomb Crash Walk", 700, 270,  499,"巨炮轰炸");
	m_pTypeInfo[SKILL_BOMB_CRASH_WALK].DomainType = SKILLDOMAIN_BLADE;
	m_pTypeInfo[SKILL_BOMB_CRASH_WALK].minDamage =60;
	m_pTypeInfo[SKILL_BOMB_CRASH_WALK].maxDamage =185;
	m_pTypeInfo[SKILL_BOMB_CRASH_WALK].minDelay =300;
	m_pTypeInfo[SKILL_BOMB_CRASH_WALK].maxDelay =300;
	m_pTypeInfo[SKILL_BOMB_CRASH_WALK].minDuration =200;
	m_pTypeInfo[SKILL_BOMB_CRASH_WALK].maxDuration =200;
	m_pTypeInfo[SKILL_BOMB_CRASH_WALK].SetMP(50);
	m_pTypeInfo[SKILL_BOMB_CRASH_WALK].minRange=7;
	m_pTypeInfo[SKILL_BOMB_CRASH_WALK].maxRange=7;
	m_pTypeInfo[SKILL_BOMB_CRASH_WALK].SetLearnLevel(150);

	m_pTypeInfo[SKILL_SATELLITE_BOMB].Set(150, "Satellite Bomb", 0, 0, 500,"卫星轰击");
	m_pTypeInfo[SKILL_SATELLITE_BOMB].DomainType = SKILLDOMAIN_GUN;
	m_pTypeInfo[SKILL_SATELLITE_BOMB].minDamage =60;
	m_pTypeInfo[SKILL_SATELLITE_BOMB].maxDamage =155;
	m_pTypeInfo[SKILL_SATELLITE_BOMB].minDelay =100;
	m_pTypeInfo[SKILL_SATELLITE_BOMB].maxDelay =450;
	m_pTypeInfo[SKILL_SATELLITE_BOMB].minDuration =200;
	m_pTypeInfo[SKILL_SATELLITE_BOMB].maxDuration =200;
	m_pTypeInfo[SKILL_SATELLITE_BOMB].SetMP(40);
	m_pTypeInfo[SKILL_SATELLITE_BOMB].minRange=7;
	m_pTypeInfo[SKILL_SATELLITE_BOMB].maxRange=7;
	m_pTypeInfo[SKILL_SATELLITE_BOMB].SetLearnLevel(150);

	m_pTypeInfo[SKILL_ILLUSION_INVERSION].Set(150, "Illusion Inversion", 700, 270,  501,"恐怖幻觉");
	m_pTypeInfo[SKILL_ILLUSION_INVERSION].DomainType = SKILLDOMAIN_HEAL;
	m_pTypeInfo[SKILL_ILLUSION_INVERSION].minDamage =60;
	m_pTypeInfo[SKILL_ILLUSION_INVERSION].maxDamage =220;
	m_pTypeInfo[SKILL_ILLUSION_INVERSION].minDelay =270;
	m_pTypeInfo[SKILL_ILLUSION_INVERSION].maxDelay =270;
	m_pTypeInfo[SKILL_ILLUSION_INVERSION].minDuration =200;
	m_pTypeInfo[SKILL_ILLUSION_INVERSION].maxDuration =200;
	m_pTypeInfo[SKILL_ILLUSION_INVERSION].SetMP(100);
	m_pTypeInfo[SKILL_ILLUSION_INVERSION].minRange=6;
	m_pTypeInfo[SKILL_ILLUSION_INVERSION].maxRange=6;
	m_pTypeInfo[SKILL_ILLUSION_INVERSION].SetLearnLevel(150);


	m_pTypeInfo[SKILL_HEAVEN_GROUND].Set(150, "Heaven Ground", 700, 270,  502,"天神降临");
	m_pTypeInfo[SKILL_HEAVEN_GROUND].DomainType = SKILLDOMAIN_ENCHANT;
	m_pTypeInfo[SKILL_HEAVEN_GROUND].minDamage =60;
	m_pTypeInfo[SKILL_HEAVEN_GROUND].maxDamage =220;
	m_pTypeInfo[SKILL_HEAVEN_GROUND].minDelay =400;
	m_pTypeInfo[SKILL_HEAVEN_GROUND].maxDelay =400;
	m_pTypeInfo[SKILL_HEAVEN_GROUND].minDuration =200;
	m_pTypeInfo[SKILL_HEAVEN_GROUND].maxDuration =200;
	m_pTypeInfo[SKILL_HEAVEN_GROUND].SetMP(100);
	m_pTypeInfo[SKILL_HEAVEN_GROUND].minRange=6;
	m_pTypeInfo[SKILL_HEAVEN_GROUND].maxRange=6;
	m_pTypeInfo[SKILL_HEAVEN_GROUND].SetLearnLevel(150);

	m_pTypeInfo[SKILL_DUMMY_DRAKE].Set(150, "Dummy Drake", 0, 0, 508,"德雷克傀儡"); 
	m_pTypeInfo[SKILL_DUMMY_DRAKE].DomainType = SKILLDOMAIN_OUSTERS;
	m_pTypeInfo[SKILL_DUMMY_DRAKE].minDamage =60;
	m_pTypeInfo[SKILL_DUMMY_DRAKE].maxDamage =220;
	m_pTypeInfo[SKILL_DUMMY_DRAKE].minDelay =400;
	m_pTypeInfo[SKILL_DUMMY_DRAKE].maxDelay =400;
	m_pTypeInfo[SKILL_DUMMY_DRAKE].minDuration =200;
	m_pTypeInfo[SKILL_DUMMY_DRAKE].maxDuration =200;
	m_pTypeInfo[SKILL_DUMMY_DRAKE].SetMP(100);
	m_pTypeInfo[SKILL_DUMMY_DRAKE].minRange=5;
	m_pTypeInfo[SKILL_DUMMY_DRAKE].maxRange=5;
	m_pTypeInfo[SKILL_DUMMY_DRAKE].SetLearnLevel(150);
	m_pTypeInfo[SKILL_DUMMY_DRAKE].Fire=25;
	m_pTypeInfo[SKILL_DUMMY_DRAKE].ElementalDomain= SKILLINFO_NODE::ELEMENTAL_DOMAIN_FIRE;
	m_pTypeInfo[SKILL_DUMMY_DRAKE].SkillTypeList.push_back(349);

	m_pTypeInfo[SKILL_HYDRO_CONVERGENCE].Set(150, "Hydro Convergence", 0, 0, 509,"复合水疗");
	m_pTypeInfo[SKILL_HYDRO_CONVERGENCE].DomainType = SKILLDOMAIN_OUSTERS;
	m_pTypeInfo[SKILL_HYDRO_CONVERGENCE].minDamage =60;
	m_pTypeInfo[SKILL_HYDRO_CONVERGENCE].maxDamage =150;
	m_pTypeInfo[SKILL_HYDRO_CONVERGENCE].minDelay =300;
	m_pTypeInfo[SKILL_HYDRO_CONVERGENCE].maxDelay =300;
	m_pTypeInfo[SKILL_HYDRO_CONVERGENCE].minDuration =300;
	m_pTypeInfo[SKILL_HYDRO_CONVERGENCE].maxDuration =300;
	m_pTypeInfo[SKILL_HYDRO_CONVERGENCE].SetMP(95);
	m_pTypeInfo[SKILL_HYDRO_CONVERGENCE].minRange=5;
	m_pTypeInfo[SKILL_HYDRO_CONVERGENCE].maxRange=5;
	m_pTypeInfo[SKILL_HYDRO_CONVERGENCE].SetLearnLevel(150);
	m_pTypeInfo[SKILL_HYDRO_CONVERGENCE].Water=25;
	m_pTypeInfo[SKILL_HYDRO_CONVERGENCE].ElementalDomain= SKILLINFO_NODE::ELEMENTAL_DOMAIN_WATER;
 	m_pTypeInfo[SKILL_HYDRO_CONVERGENCE].SkillTypeList.push_back(351);

	m_pTypeInfo[SKILL_SUMMON_CLAY].Set(150, "Summon Clay", 0, 0, 510,"粘土召唤");
	m_pTypeInfo[SKILL_SUMMON_CLAY].DomainType = SKILLDOMAIN_OUSTERS;
	m_pTypeInfo[SKILL_SUMMON_CLAY].minDamage =60;
	m_pTypeInfo[SKILL_SUMMON_CLAY].maxDamage =150;
	m_pTypeInfo[SKILL_SUMMON_CLAY].minDelay =400;
	m_pTypeInfo[SKILL_SUMMON_CLAY].maxDelay =400;
	m_pTypeInfo[SKILL_SUMMON_CLAY].minDuration =200;
	m_pTypeInfo[SKILL_SUMMON_CLAY].maxDuration =200;
	m_pTypeInfo[SKILL_SUMMON_CLAY].SetMP(100);
	m_pTypeInfo[SKILL_SUMMON_CLAY].minRange=5;
	m_pTypeInfo[SKILL_SUMMON_CLAY].maxRange=5;
	m_pTypeInfo[SKILL_SUMMON_CLAY].SetLearnLevel(150);
	m_pTypeInfo[SKILL_SUMMON_CLAY].Earth=25;
	m_pTypeInfo[SKILL_SUMMON_CLAY].ElementalDomain= SKILLINFO_NODE::ELEMENTAL_DOMAIN_EARTH;
 	m_pTypeInfo[SKILL_SUMMON_CLAY].SkillTypeList.push_back(352);

	m_pTypeInfo[SKILL_HETER_CHAKRAM].Set(150, "Heter Chakram", 0 ,0, 507,"夏布利基因");
	m_pTypeInfo[SKILL_HETER_CHAKRAM].DomainType = SKILLDOMAIN_OUSTERS;
	m_pTypeInfo[SKILL_HETER_CHAKRAM].minDamage =60;
	m_pTypeInfo[SKILL_HETER_CHAKRAM].maxDamage =150;
	m_pTypeInfo[SKILL_HETER_CHAKRAM].minDelay =400;
	m_pTypeInfo[SKILL_HETER_CHAKRAM].maxDelay =400;
	m_pTypeInfo[SKILL_HETER_CHAKRAM].minDuration =200;
	m_pTypeInfo[SKILL_HETER_CHAKRAM].maxDuration =200;
	m_pTypeInfo[SKILL_HETER_CHAKRAM].SetMP(75);
	m_pTypeInfo[SKILL_HETER_CHAKRAM].minRange=4;
	m_pTypeInfo[SKILL_HETER_CHAKRAM].maxRange=4;
	m_pTypeInfo[SKILL_HETER_CHAKRAM].SetLearnLevel(150);
	m_pTypeInfo[SKILL_HETER_CHAKRAM].ElementalDomain= SKILLINFO_NODE::ELEMENTAL_DOMAIN_NO_DOMAIN;
 	m_pTypeInfo[SKILL_HETER_CHAKRAM].SkillTypeList.push_back(304);

#endif
}
