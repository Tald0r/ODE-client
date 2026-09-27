//---------------------------------------------------------------------------------
// RankBonusTable.h
//---------------------------------------------------------------------------------
// 이미 화일이 존재하고.. 거기서 읽어온다고 가정했기 때문에.. 
// save함수는 없당..
//---------------------------------------------------------------------------------

#ifndef __RANKBONUSTABLE_H__
#define __RANKBONUSTABLE_H__

#include "CTypeTable.h"
#include "MString.h"
#include "RaceType.h"

#include <fstream>


//---------------------------------------------------------------------------------
// RankBonus Info
//---------------------------------------------------------------------------------
class RankBonusInfo {
	public:
		enum RANK_BONUS_STATUS
		{
			STATUS_NULL,
			STATUS_CANNOT_LEARN,
			STATUS_LEARNED,
		};
	
	public :
		unsigned short			GetType() const			{ return m_type; }
		const char*				GetName() const			{ return m_Name.GetString(); }
		char					GetLevel() const		{ return m_level; }
		bool					IsSlayerSkill() const	{ return (m_race == RACE_SLAYER); }
		bool					IsVampireSkill() const	{ return (m_race == RACE_VAMPIRE); }
		bool					IsOustersSkill() const	{ return (m_race == RACE_OUSTERS); }
		unsigned short			GetSkillIconID() const	{ return m_skillIconID; }
		RANK_BONUS_STATUS		GetStatus() const		{ return m_status; }
		void					SetStatus(RANK_BONUS_STATUS status)	{ m_status = status; }
		int						GetPoint() const		{ return m_point; }

	public :
		RankBonusInfo();
		void		LoadFromFile(std::ifstream& file);		

	private:
		unsigned short m_type;
		MString m_Name;
		char m_level;
		Race m_race;	// 0 : Slayer  1 : Vampire
		int m_point;
		unsigned short m_skillIconID;
		RANK_BONUS_STATUS m_status;
		
};

//---------------------------------------------------------------------------------
// RankBonus Table
//---------------------------------------------------------------------------------
class RankBonusTable:public CTypeTable<RankBonusInfo>
{
};

extern RankBonusTable *g_pRankBonusTable;
#endif
