//----------------------------------------------------------------------
// MSkillManager.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MSkillManager.h"
#include "MTypeDef.h"
#include "MItem.h"		// the item host carries the frame stamp the delays run on

#include <algorithm>

//----------------------------------------------------------------------
//
// Global
//
//----------------------------------------------------------------------
// SkillInfoTable
MSkillInfoTable	*					g_pSkillInfoTable = NULL;

// SkillManager
MSkillManager*						g_pSkillManager = NULL;

// SkillAvailable
MSkillSet*							g_pSkillAvailable = NULL;

// 
//----------------------------------------------------------------------
//
//							SKILLINFO_NODE
//
//----------------------------------------------------------------------
SKILLINFO_NODE::SKILLINFO_NODE()
{
	m_Level = 1;

	m_SpriteID = 0;

	m_MP = 0;
		
	m_bPassive = false;
	m_bActive = false;
		
	m_ExpLevel = 0;			// 스킬 레벨
	m_SkillExp = 0;			// 스킬 경험치

	m_LearnLevel = 100;
	m_eSkillRace = RACE_SLAYER;

	m_DelayTime = 0;		// the delay before it can be used again
	m_AvailableTime = MonotonicClock::TimePoint();	// when it can be used again

	m_bEnable = false;

	// Where it sits in its tree, and where it is drawn: the skill file
	// fills all three, and a domain branches on the step, so they are
	// defined here rather than left to whatever the memory held.
	m_SkillStep = SKILL_STEP_NULL;
	m_X = 0;
	m_Y = 0;

	DomainType = 0;		// 그 기술이 어느 도메인에 속하는가.
	minDamage = 0;		// 최소 데미지 또는 효과치.
	maxDamage = 0;		// 최대 데미지 또는 효과치.
	minDelay = 0;		// 최소 사용 딜레이.
	maxDelay = 0;		// 최대 사용 딜레이.
	minCastTime = 0;	// 최소 캐스팅 타임.
	maxCastTime = 0;	// 최대 캐스팅 타임.		
	minDuration = 0;	// 최소 지속 시간
	maxDuration = 0;	// 최대 지속 시간
	minRange = 1;		// 최소 사정거리
	maxRange = 1;		// 최대 사정거리
	maxExp = 0;			// 그 기술의 100% 경험치. 1 회당 + 1 씩 올라감
	SkillPoint = 0;
	LevelUpPoint = 0;
	Fire= 0;
	Water= 0;
	Earth= 0;
	Wind= 0;
	Sum= 0;
	Wristlet= 0;
	Stone1= 0;
	Stone2= 0;
	Stone3= 0;
	Stone4= 0;
	ElementalDomain= 0;
	CanDelete = 0;
}
//----------------------------------------------------------------------
// Save From File ServerSkillInfo
//----------------------------------------------------------------------
void 
SKILLINFO_NODE::SaveFromFileServerSkillInfo(ofstream &file)
{
	
	file.write((char*)&m_LearnLevel, 4);
	file.write((char*)&DomainType, 4);
	m_Name.SaveToFile( file );
	m_HName.SaveToFile( file );
	file.write((char*)&minDamage, 4);		// 최소 데미지 또는 효과치.
	file.write((char*)&maxDamage, 4);		// 최대 데미지 또는 효과치.
	file.write((char*)&minDelay, 4);			// 최소 사용 딜레이.
	file.write((char*)&maxDelay, 4);			// 최대 사용 딜레이.
	file.write((char*)&minDuration, 4);		// 최소 캐스팅 타임.
	file.write((char*)&maxDuration, 4);		// 최대 캐스팅 타임.
	file.write((char*)&m_MP, 4);					// 마나 소모량.(m_MP)
	file.write((char*)&minRange, 4);			// 최소 사정거리
	file.write((char*)&maxRange, 4);			// 최대 사정거리
	file.write((char*)&maxExp, 4);			// 그 기술의 100% 경험치. 1 회당 + 1 씩 올라감	
	
	if(DomainType == SKILLDOMAIN_OUSTERS)
	{
		file.write((char*)&SkillPoint,		sizeof(int));
		file.write((char*)&LevelUpPoint,		sizeof(int));
		int szSkill= SkillTypeList.size();
		file.write((char*)&szSkill,			sizeof(int));
		SKILLTYPE_LIST::const_iterator iSkillID = SkillTypeList.begin();
		while (iSkillID!=SkillTypeList.end())
 		{
 			int skillType = *iSkillID;
 
 			file.write((const char*)&skillType, sizeof(int));
 
 			iSkillID++;
 		}
// 		for(int i = 0; i < szSkill; i++)
// 		{
// 			int skillType;
// 			file.write((char*)&skillType,		sizeof(int));
// 			SkillTypeList.push_back(skillType);
// 		}
		file.write((char*)&Fire,				sizeof(int));
		file.write((char*)&Water,			sizeof(int));
		file.write((char*)&Earth,			sizeof(int));
		file.write((char*)&Wind,				sizeof(int));
		file.write((char*)&Sum,				sizeof(int));
		file.write((char*)&Wristlet,			sizeof(int));
		file.write((char*)&Stone1,			sizeof(int));
		file.write((char*)&Stone2,			sizeof(int));
		file.write((char*)&Stone3,			sizeof(int));
		file.write((char*)&Stone4,			sizeof(int));
		file.write((char*)&ElementalDomain,	sizeof(int));
		file.write((char*)&CanDelete,		sizeof(BYTE));
	} 
}
//----------------------------------------------------------------------
// Load From File ServerSkillInfo
//----------------------------------------------------------------------
void		
SKILLINFO_NODE::LoadFromFileServerSkillInfo(std::ifstream& file)
{
	int ll;
	MString name;
	MString hname;
	int mp;
	
	file.read((char*)&ll, 4);
	file.read((char*)&DomainType, 4);
	name.LoadFromFile( file );
	hname.LoadFromFile( file );
	file.read((char*)&minDamage, 4);		// 최소 데미지 또는 효과치.
	file.read((char*)&maxDamage, 4);		// 최대 데미지 또는 효과치.
	file.read((char*)&minDelay, 4);			// 최소 사용 딜레이.
	file.read((char*)&maxDelay, 4);			// 최대 사용 딜레이.
	file.read((char*)&minDuration, 4);		// 최소 캐스팅 타임.
	file.read((char*)&maxDuration, 4);		// 최대 캐스팅 타임.
	file.read((char*)&mp, 4);					// 마나 소모량.(m_MP)
	file.read((char*)&minRange, 4);			// 최소 사정거리
	file.read((char*)&maxRange, 4);			// 최대 사정거리
	file.read((char*)&maxExp, 4);			// 그 기술의 100% 경험치. 1 회당 + 1 씩 올라감	
	
	if(DomainType == SKILLDOMAIN_OUSTERS)
	{
		file.read((char*)&SkillPoint,		sizeof(int));
		file.read((char*)&LevelUpPoint,		sizeof(int));
		int szSkill;
		file.read((char*)&szSkill,			sizeof(int));
		SkillTypeList.clear();
		for(int i = 0; i < szSkill; i++)
		{
			int skillType;
			file.read((char*)&skillType,		sizeof(int));
			SkillTypeList.push_back(skillType);
		}
		file.read((char*)&Fire,				sizeof(int));
		file.read((char*)&Water,			sizeof(int));
		file.read((char*)&Earth,			sizeof(int));
		file.read((char*)&Wind,				sizeof(int));
		file.read((char*)&Sum,				sizeof(int));
		file.read((char*)&Wristlet,			sizeof(int));
		file.read((char*)&Stone1,			sizeof(int));
		file.read((char*)&Stone2,			sizeof(int));
		file.read((char*)&Stone3,			sizeof(int));
		file.read((char*)&Stone4,			sizeof(int));
		file.read((char*)&ElementalDomain,	sizeof(int));
		file.read((char*)&CanDelete,		sizeof(BYTE));
	} else
	{
		ElementalDomain = -1;
	}
	if (name != "Empty Skill")
	{
		m_LearnLevel = ll;
		m_Name = name;
		m_HName = hname;
		m_MP = mp;
	}
}

//----------------------------------------------------------------------
// Add NextSkill
//----------------------------------------------------------------------
// 다음에 배울 수 있는 Skill들을 설정한다.
//----------------------------------------------------------------------
bool			
SKILLINFO_NODE::AddNextSkill(ACTIONINFO id)
{
	SKILLID_LIST::iterator iSkill = m_listNextSkill.begin();

	// sort해서 add한다.
	while (iSkill != m_listNextSkill.end())
	{
		// 이미 있으면 추가 불가
		if (*iSkill==id)
		{
			return false;
		}
		// 큰거 앞에..
		else if (*iSkill > id)
		{
			// 앞에 추가한다.
			m_listNextSkill.insert( iSkill, id );

			return true;
		}

		iSkill++;
	}

	m_listNextSkill.push_back( id );

	return true;
}

//----------------------------------------------------------------------
// Save To File
//----------------------------------------------------------------------
void		
SKILLINFO_NODE::SaveToFile(std::ofstream& file)
{

	m_Name.SaveToFile( file );							// 기술 이름
	m_HName.SaveToFile( file );
	file.write((const char*)&m_Level, 4);
	file.write((const char*)&m_X, 4);
	file.write((const char*)&m_Y, 4);					// 화면에서의 출력 시작 위치
	file.write((const char*)&m_SpriteID, SIZE_SPRITEID);	// 기술의 Icon Sprite
	file.write((const char*)&m_MP, 4);				// MP소비량
	file.write((const char*)&m_bPassive, 1);		// passive skill인가?
	file.write((const char*)&m_bActive, 1);			// 항상 사용 가능한 skill인가?

	BYTE skillStep = m_SkillStep;
	file.write((const char*)&skillStep, 1);

	// id list 저장
	int idNum = m_listNextSkill.size();
	file.write((const char*)&idNum, 4);
	SKILLID_LIST::const_iterator iSkillID = m_listNextSkill.begin();

	while (iSkillID!=m_listNextSkill.end())
	{
		TYPE_ACTIONINFO id = *iSkillID;

		file.write((const char*)&id, SIZE_ACTIONINFO);

		iSkillID++;
	}
}

//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
void		
SKILLINFO_NODE::LoadFromFile(std::ifstream& file)
{
	m_Name.LoadFromFile( file );					// 기술 이름
	m_HName.LoadFromFile( file );
	file.read((char*)&m_Level, 4);
	file.read((char*)&m_X, 4);
	file.read((char*)&m_Y, 4);						// 화면에서의 출력 시작 위치
	file.read((char*)&m_SpriteID, SIZE_SPRITEID);	// 기술의 Icon Sprite
	file.read((char*)&m_MP, 4);						// MP소비량
	file.read((char*)&m_bPassive, 1);				// passive 스킬?
	file.read((char*)&m_bActive, 1);				// 항상 사용 가능한 skill인가?

	BYTE skillStep;
	file.read((char*)&skillStep, 1);
	m_SkillStep = (SKILL_STEP)skillStep;
	// id list load
	int idNum;
	file.read((char*)&idNum, 4);

	m_listNextSkill.clear();
	for (int i=0; i<idNum; i++)
	{
		TYPE_ACTIONINFO id;

		file.read((char*)&id, SIZE_ACTIONINFO);

		m_listNextSkill.push_back( (ACTIONINFO)id );
	}	

	// 배운 level
	m_ExpLevel = 0;
}	

//----------------------------------------------------------------------
// Set DelayTime ( delay )
//----------------------------------------------------------------------
// 기술 사용후 다시 사용할 수 있는 delay시간 설정
//----------------------------------------------------------------------
void
SKILLINFO_NODE::SetDelayTime(DWORD delay)		
{
	// 3초 이하 기술은 delay가 없는 걸로 표시한다.
	if (delay < 1800)
	{
		delay = 0;
	}			
				
	m_DelayTime = delay;	
}

//----------------------------------------------------------------------
// Is AvailableTime ?
//----------------------------------------------------------------------
// 지금 사용 가능한가?
//----------------------------------------------------------------------
bool
SKILLINFO_NODE::IsAvailableTime() const
{
	// The delays run on the item host's frame stamp; without one
	// (a test binary) there is no delay.
	const MonotonicClock::TimePoint* pClock = MItem::Clock();

	return pClock==NULL || *pClock >= m_AvailableTime;
}

//----------------------------------------------------------------------
// Get AvailableTimeLeft
//----------------------------------------------------------------------
// 남은 사용 가능 시간
//----------------------------------------------------------------------
DWORD				
SKILLINFO_NODE::GetAvailableTimeLeft() const
{
	const MonotonicClock::TimePoint* pClock = MItem::Clock();

	if (pClock!=NULL)
	{
		const MonotonicClock::Duration left = m_AvailableTime - *pClock;

		if (left > MonotonicClock::Duration(0))
		{
			return (DWORD)left.count();
		}
	}

	return 0;
}

//----------------------------------------------------------------------
// Set AvailableTime
//----------------------------------------------------------------------
// 지금 바로 사용 가능하게 설정한다.
//----------------------------------------------------------------------
void
SKILLINFO_NODE::SetAvailableTime(int delay)
{
	// No delay at all reads as zero rather than as "now".
	const MonotonicClock::TimePoint* pClock = MItem::Clock();

	if(delay == 0)
		m_AvailableTime = MonotonicClock::TimePoint();
	else
		m_AvailableTime = (pClock!=NULL ? *pClock : MonotonicClock::TimePoint()) + MonotonicClock::Duration(delay);
}

//----------------------------------------------------------------------
// Set Next AvailableTime
//----------------------------------------------------------------------
// 다음 사용 가능한 시간을 결정한다.
//----------------------------------------------------------------------
void
SKILLINFO_NODE::SetNextAvailableTime()
{
	// Available again once the delay has run from now.
	const MonotonicClock::TimePoint* pClock = MItem::Clock();

	m_AvailableTime = (pClock!=NULL ? *pClock : MonotonicClock::TimePoint()) + MonotonicClock::Millis(m_DelayTime);
}

//----------------------------------------------------------------------
// Set Enable
//----------------------------------------------------------------------
void				
SKILLINFO_NODE::SetEnable(bool enable)
{
	m_bEnable = enable;
}

//----------------------------------------------------------------------
// Set Disable
//----------------------------------------------------------------------
/*
void				
SKILLINFO_NODE::SetDisable()
{
	m_bEnable = false;
}
*/

//----------------------------------------------------------------------
//
//							MSkillSet
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Add Skill
//----------------------------------------------------------------------
bool			
MSkillSet::AddSkill(ACTIONINFO id, BYTE flag)
{
	SKILLID_MAP::iterator	iSkill;

	iSkill = find( id );
	
	//-----------------------------------------------
	// 아직 없는 Skill이면 추가	
	//-----------------------------------------------
	if (iSkill == end())
	{
		insert(SKILLID_MAP::value_type( id, SKILLID_NODE(id, flag) ));

		return true;
	}
	
	//-----------------------------------------------
	// 이미 있다면 flag만 바꾼다.
	//-----------------------------------------------
	SKILLID_NODE& node = (*iSkill).second;
	
	node.Flag = flag;

	return false;
}

//----------------------------------------------------------------------
// Remove Skill
//----------------------------------------------------------------------
// SkillID 제거
//----------------------------------------------------------------------
bool			
MSkillSet::RemoveSkill(ACTIONINFO id)
{
	SKILLID_MAP::iterator	iSkill;

	//--------------------------------------------------
	// ID가 id인 Skill를 찾는다.
	//--------------------------------------------------
	iSkill = find(id);
    
	//--------------------------------------------------
	// 그런 id를 가진 Skill이 없는 경우
	//--------------------------------------------------
	if (iSkill == end())
	{
		return false;
	}

	//--------------------------------------------------
	// 찾은 경우 --> 제거	
	//--------------------------------------------------
	// map에서 제거
	erase( iSkill );

	return true;
}

//----------------------------------------------------------------------
// Is Enable Skill?
//----------------------------------------------------------------------
// id의 skill이 사용 가능한가?
//----------------------------------------------------------------------
bool			
MSkillSet::IsEnableSkill(ACTIONINFO id) const
{
	SKILLID_MAP::const_iterator		iSkill;

	//--------------------------------------------------
	// ID가 id인 Skill를 찾는다.
	//--------------------------------------------------
	iSkill = find(id);
    
	//--------------------------------------------------
	// 그런 id를 가진 Skill이 없는 경우
	//--------------------------------------------------
	if (iSkill == end())
	{
		return false;
	}

	//--------------------------------------------------
	// 찾은 경우 --> Is enable?
	//--------------------------------------------------
			
	return ((*iSkill).second).IsEnable()!=0;	
}

//----------------------------------------------------------------------
// Enable Skill
//----------------------------------------------------------------------
bool			
MSkillSet::EnableSkill(ACTIONINFO id)
{
	SKILLID_MAP::iterator	iSkill;

	//--------------------------------------------------
	// ID가 id인 Skill를 찾는다.
	//--------------------------------------------------
	iSkill = find(id);
    
	//--------------------------------------------------
	// 그런 id를 가진 Skill이 없는 경우
	//--------------------------------------------------
	if (iSkill == end())
	{
		return false;
	}

	//--------------------------------------------------
	// 찾은 경우 --> enable
	//--------------------------------------------------
	// map에서 제거
	((*iSkill).second).SetEnable();

	return true;
}

//----------------------------------------------------------------------
// Disable Skill 
//----------------------------------------------------------------------
bool			
MSkillSet::DisableSkill(ACTIONINFO id)	
{
	SKILLID_MAP::iterator	iSkill;

	//--------------------------------------------------
	// ID가 id인 Skill를 찾는다.
	//--------------------------------------------------
	iSkill = find(id);
    
	//--------------------------------------------------
	// 그런 id를 가진 Skill이 없는 경우
	//--------------------------------------------------
	if (iSkill == end())
	{
		return false;
	}

	//--------------------------------------------------
	// 찾은 경우 --> enable
	//--------------------------------------------------
	// map에서 제거
	((*iSkill).second).SetDisable();

	return true;
}

//----------------------------------------------------------------------
//
//							MSkillDomain
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
//
// constructor / destructor
//
//----------------------------------------------------------------------
MSkillDomain::MSkillDomain()
{
	m_bNewSkill			= false;

	m_MaxLevel			= 0;
	m_MaxLearnedLevel	= -1;
	m_pLearnedSkillID	= NULL;
	m_DomainLevel		= 0;
	m_DomainExpRemain	= 0;

	m_DomainExpTable.Init( 151 );
}

MSkillDomain::~MSkillDomain()
{
	if (m_pLearnedSkillID!=NULL)
	{
		delete [] m_pLearnedSkillID;
	}


	ClearSkillStep();
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Set Max Level
//----------------------------------------------------------------------
// domain의 최고 level 기술
//----------------------------------------------------------------------
void		
MSkillDomain::SetMaxLevel()
{
	if (m_pLearnedSkillID!=NULL)
	{
		delete [] m_pLearnedSkillID;
	}

	m_pLearnedSkillID = new ACTIONINFO [m_MaxLevel+1];	// 0부터 시작한다.
	m_MaxLearnedLevel = -1;

	for (int i=0; i<=m_MaxLevel; i++)
	{
		m_pLearnedSkillID[i] = MAX_ACTIONINFO;//ACTIONINFO_NULL;
	}
}

//----------------------------------------------------------------------
// Clear
//----------------------------------------------------------------------
void			
MSkillDomain::Clear()
{
	ClearSkillList();

	//-----------------------------------------------
	// Forget the levels of what was learned.
	//-----------------------------------------------
	// ClearSkillList has already freed the array, so these are reset
	// whatever it found: a test for the pointer here could never fire,
	// and left the two counters naming a level of an array that is
	// gone - which the learn and unlearn paths then index.
	//-----------------------------------------------
	m_MaxLevel = 0;
	m_MaxLearnedLevel = -1;
}

void
MSkillDomain::ClearSkillList()
{
	SKILLID_MAP::iterator	iSkill = m_mapSkillID.begin();
	SKILLID_MAP::iterator	endItr = m_mapSkillID.end();

	while (iSkill!=endItr)
	{
		//-----------------------------------------------
		// Take it out of the skills usable now.
		//-----------------------------------------------
		if (g_pSkillAvailable!=NULL)
		{
			(*g_pSkillAvailable).RemoveSkill( (*iSkill).first );
		}

		iSkill++;
	}

	m_mapSkillID.clear();

	if(m_pLearnedSkillID != NULL)
	{
		delete [] m_pLearnedSkillID;
		m_pLearnedSkillID = NULL;
	}

	//-----------------------------------------------
	// The step lists hold the same skills, so they go with the list.
	// They used to be freed by the destructor alone, which left every
	// rebuild - the skill-info packet's, and a load - adding to lists
	// that still named the skills of the tree before it.
	//-----------------------------------------------
	ClearSkillStep();
}

//----------------------------------------------------------------------
// Clear SkillStep
//----------------------------------------------------------------------
void
MSkillDomain::ClearSkillStep()
{
	SKILL_STEP_MAP::iterator iList = m_mapSkillStep.begin();

	while (iList != m_mapSkillStep.end())
	{
		SKILL_STEP_LIST* pList = iList->second;

		if (pList != NULL)
		{
			delete pList;
		}

		iList ++;
	}

	m_mapSkillStep.clear();
}

//----------------------------------------------------------------------
// AddSkill
//----------------------------------------------------------------------
// id와 그의 하위에 있는 것들을 모두 추가한다.
// 단 id의 skill의 level은 0이어야 한다.
//----------------------------------------------------------------------
bool			
MSkillDomain::SetRootSkill(ACTIONINFO id, bool reset)
{
	//--------------------------------------------------
	// 다 지운다.
	//--------------------------------------------------
	//Clear();
	if(reset)
		m_DomainLevel		= 0;

	//--------------------------------------------------
	// level이 0인 skill이어야 한다.
	//--------------------------------------------------
//	if ((*g_pSkillInfoTable)[id].GetLevel()==0)
	{
		int oldMaxLevel = m_MaxLevel;

		if( reset )
			m_MaxLevel = 0;
		bool bOK = AddSkill( id );		
		
		if (oldMaxLevel > m_MaxLevel && reset)
		{
			m_MaxLevel = oldMaxLevel;
		}

		//--------------------------------------------------
		// 제대로 추가된 경우
		//--------------------------------------------------
		if (bOK)
		{
			//--------------------------------------------------
			// domain의 skill level을 설정해준다.
			//--------------------------------------------------
			SetMaxLevel();
		}
	}

	return false;
}

//----------------------------------------------------------------------
// AddSkill
//----------------------------------------------------------------------
// Add <id> and everything below it.
//----------------------------------------------------------------------
bool
MSkillDomain::AddSkill(ACTIONINFO id)
{
	SKILLID_MAP::iterator	iSkill;

	iSkill = m_mapSkillID.begin();

	while( iSkill != m_mapSkillID.end() )
	{
		if( iSkill->first == id )
		{
			break;
		}
		iSkill++;
	}
	int skillLevel = (*g_pSkillInfoTable)[id].GetLevel();

	//--------------------------------------------------
	// Find the domain's top skill level.
	//--------------------------------------------------
	if (m_MaxLevel < skillLevel)
	{
		m_MaxLevel = skillLevel;
	}

	//-----------------------------------------------
	// Add the skill if it is not in yet	
	//-----------------------------------------------
	if (iSkill == m_mapSkillID.end() )
	{		
		//-----------------------------------------------
		// A root-level skill is marked as learnable next.
		//-----------------------------------------------
		if (skillLevel==0)
		{
			m_mapSkillID.insert(SKILLID_MAP::value_type( id, SKILLSTATUS_NEXT ));
		}
		else
		{
			m_mapSkillID.insert(SKILLID_MAP::value_type( id, SKILLSTATUS_OTHER ));
		}	
		
		//-----------------------------------------------
		// Put it in the step list it belongs to here.
		//-----------------------------------------------
		AddSkillStep( GetSkillStepFor( id ), id );

		//--------------------------------------------------
		// Find what can be learned next and add it.
		//--------------------------------------------------
		const SKILLINFO_NODE::SKILLID_LIST& listNextSkill = (*g_pSkillInfoTable)[id].GetNextSkillList();

		SKILLINFO_NODE::SKILLID_LIST::const_iterator iNextSkill = listNextSkill.begin();

		while (iNextSkill != listNextSkill.end())
		{
			//--------------------------------------------------
			// Add the skill whose id is *iNextSkill when the
			// domain does not hold it yet.
			//--------------------------------------------------
			if (!m_mapSkillID.contains( *iNextSkill ))
			{
				AddSkill( *iNextSkill );
			}

			iNextSkill++;
		}

		return true;
	}

	//-----------------------------------------------
	// A skill the domain already holds gives false
	//-----------------------------------------------
	return false;
}


//----------------------------------------------------------------------
// Set SkillStatus
//----------------------------------------------------------------------
// skill의 상태를 변경한다.
//----------------------------------------------------------------------
/*
bool		
MSkillDomain::SetSkillStatus(ACTIONINFO id, SKILLSTATUS status)
{
	SKILLID_MAP::iterator	iSkill;

	iSkill = m_mapSkillID.find( id );
	
	//-----------------------------------------------
	// domain에 있는 Skill이면..
	//-----------------------------------------------
	if (iSkill != m_mapSkillID.end())
	{
		m_mapSkillID.insert(SKILLID_MAP::value_type( id, status ));

		switch (status)
		{
			case SKILLSTATUS_LEARNED :		// 배웠다.
				//-----------------------------------------------
				// 현재 사용 가능한 Skill에 추가한다.
				//-----------------------------------------------
				//(*g_pSkillAvailable).AddSkill( id );
				LearnSkill(id);
			break;

			case SKILLSTATUS_NEXT :				// 다음에 배울 수 있다.
			case SKILLSTATUS_OTHER :			// 아직은 배울 수 없다
				//-----------------------------------------------
				// 현재 사용 가능한 Skill에서 제거한다.
				//-----------------------------------------------
				(*g_pSkillAvailable).RemoveSkill( id );
			break;
		}

		return true;
	}

	//-----------------------------------------------
	// 이미 있는 Skill이면 false
	//-----------------------------------------------
	return false;
}
*/

//----------------------------------------------------------------------
// Get SkillStatus
//----------------------------------------------------------------------
// id의 상태는?
//----------------------------------------------------------------------
MSkillDomain::SKILLSTATUS		
MSkillDomain::GetSkillStatus(ACTIONINFO id) const
{
	SKILLID_MAP::const_iterator	iSkill;

	//--------------------------------------------------
	// ID가 id인 Skill를 찾는다.
	//--------------------------------------------------
	iSkill = m_mapSkillID.find(id);

	//--------------------------------------------------
	// 없을 경우 NULL을 return한다.
	//--------------------------------------------------
	if (iSkill == m_mapSkillID.end()) 
	{
		return SKILLSTATUS_NULL;
	}

	//--------------------------------------------------
	// 있으면 그 Skill를 return한다.
	//--------------------------------------------------
	return (*iSkill).second;
}

//----------------------------------------------------------------------
// Remove Skill
//----------------------------------------------------------------------
// mapSkill에서 제거하고 Skill의 pointer를 넘겨준다.
//----------------------------------------------------------------------
/*
bool
MSkillDomain::RemoveSkill(ACTIONINFO id)
{
	SKILLID_MAP::iterator	iSkill;

	//--------------------------------------------------
	// ID가 id인 Skill를 찾는다.
	//--------------------------------------------------
	iSkill = m_mapSkillID.find(id);
    
	//--------------------------------------------------
	// 그런 id를 가진 Skill이 없는 경우
	//--------------------------------------------------
	if (iSkill == m_mapSkillID.end())
	{
		return false;
	}

	//--------------------------------------------------
	// 찾은 경우 --> 제거	
	//--------------------------------------------------
	// map에서 제거
	m_mapSkillID.erase( iSkill );

	return true;
}
*/

//----------------------------------------------------------------------
// Add NextSkill
//----------------------------------------------------------------------
// 다음에 배울 수 있는 기술들을 체크한다.
//----------------------------------------------------------------------
void
MSkillDomain::AddNextSkill(ACTIONINFO id)
{
	// The empty slot of the learned-skill array, which UnLearnSkill
	// hands over when it walks down to a level holding no skill. Its
	// two siblings test for it; this one leaned on the info table
	// answering an index it does not hold with an empty entry.
	if (id==MAX_ACTIONINFO)
	{
		return;
	}

	const SKILLINFO_NODE::SKILLID_LIST& listNextSkill = (*g_pSkillInfoTable)[id].GetNextSkillList();

	SKILLINFO_NODE::SKILLID_LIST::const_iterator iNextSkill = listNextSkill.begin();

	while (iNextSkill != listNextSkill.end())
	{
		//--------------------------------------------------
		// ID가 *iNextSkil인 Skill를 찾는다.
		//--------------------------------------------------
		SKILLID_MAP::iterator iSkill = m_mapSkillID.find( *iNextSkill );

		//--------------------------------------------------
		// 있으면 그 Skill의 값을 바꾼다.
		//--------------------------------------------------
		// 물론, 현재 domain에 속해있는 경우에만 가능하고
		// 아직 배우지 않은 것일 경우에만 NEXT로 설정한다.
		//--------------------------------------------------
		if (iSkill != m_mapSkillID.end())
		{
			if ((*iSkill).second==SKILLSTATUS_OTHER)
			{						
				(*iSkill).second = SKILLSTATUS_NEXT;
			}
		}

		iNextSkill++;
	}
}
void
MSkillDomain::AddNextSkillForce(ACTIONINFO id)
{
	if (id==MAX_ACTIONINFO)
	{
		return;
	}
	bool HasChildSkill = false;
//	const SKILLINFO_NODE::SKILLID_LIST& listNextSkill = (*g_pSkillInfoTable)[id].GetNextSkillList();
//
//	SKILLINFO_NODE::SKILLID_LIST::const_iterator iNextSkill = listNextSkill.begin();
//
//	while (iNextSkill != listNextSkill.end())
//	{
//		//--------------------------------------------------
//		// ID가 *iNextSkil인 Skill를 찾는다.
//		//--------------------------------------------------
//		SKILLID_MAP::iterator iSkill = m_mapSkillID.find( *iNextSkill );
//
//		//--------------------------------------------------
//		// 있으면 그 Skill의 값을 바꾼다.
//		//--------------------------------------------------
//		// 물론, 현재 domain에 속해있는 경우에만 가능하고
//		// 아직 배우지 않은 것일 경우에만 NEXT로 설정한다.
//		//--------------------------------------------------
//		if (iSkill != m_mapSkillID.end())
//		{
//			if ((*iSkill).second==SKILLSTATUS_LEARNED)
//			{		
//				HasChildSkill = true; // 자기 밑에 딸린 스킬중에 배운스킬이 없어야 한다.
//			}
//		}
//
//		iNextSkill++;
//	}
	if(!HasChildSkill && (*g_pSkillInfoTable)[id].CanDelete)// 자기밑에 딸린 스킬이 없고 삭제 가능한 스킬이면
	{
		SKILLID_MAP::iterator	iSkill = m_mapSkillID.find( id );
		if (iSkill != m_mapSkillID.end())
		{
			if ((*iSkill).second==SKILLSTATUS_LEARNED)
			{						
				(*iSkill).second = SKILLSTATUS_NEXT;
			//	RemoveNextSkill(id);
			}

		}
	}
}

//----------------------------------------------------------------------
// Remove NextSkill
//----------------------------------------------------------------------
// Take the mark off the skills that could be learned next.
//----------------------------------------------------------------------
void
MSkillDomain::RemoveNextSkill(ACTIONINFO id)
{
	if (id==MAX_ACTIONINFO)
	{
		return;
	}

	//--------------------------------------------------
	//
	// Take the mark off what the skill just given back led to.
	//
	//--------------------------------------------------
	// <id> is the skill learned just before. It used to be read back
	// out of the map, which answers a lookup with the key that was
	// looked up when it holds it and with the end iterator when it
	// does not - so the lookup could only ever return <id>, and
	// dereferencing it was the one thing it could get wrong.
	//--------------------------------------------------
	const SKILLINFO_NODE::SKILLID_LIST& listNextSkill = (*g_pSkillInfoTable)[id].GetNextSkillList();

	SKILLINFO_NODE::SKILLID_LIST::const_iterator iNextSkill = listNextSkill.begin();

	while (iNextSkill != listNextSkill.end())
	{
		//--------------------------------------------------
		// Find the skill whose id is *iNextSkill.
		//--------------------------------------------------
		SKILLID_MAP::iterator	iSkill = m_mapSkillID.find( *iNextSkill );

		//--------------------------------------------------
		// If it is there, change its status.
		//--------------------------------------------------
		// Only for a skill in this domain, of course, and
		// NEXT becomes OTHER.
		//--------------------------------------------------
		if (iSkill != m_mapSkillID.end())
		{
			if ((*iSkill).second==SKILLSTATUS_NEXT)
			{						
				(*iSkill).second = SKILLSTATUS_OTHER;
			}
		}

		iNextSkill++;
	}
}

//----------------------------------------------------------------------
// Learn Skill
//----------------------------------------------------------------------
// Put the skill <id> into the learned state.
// What it leads to becomes learnable next.
//
// Only one skill can be learned at each level.
// So what can be learned now is the skill at the top level (m_MaxLevel).
//----------------------------------------------------------------------
bool
MSkillDomain::LearnSkill(ACTIONINFO id)
{
	//--------------------------------------------------
	// Nothing to spend on a new skill..
	//--------------------------------------------------
	if (!m_bNewSkill)
	{
		return false;
	}


	//--------------------------------------------------
	// Return unless it sits at the level being learned this time
	//--------------------------------------------------
	if ((*g_pSkillInfoTable)[id].GetLevel()!=m_MaxLearnedLevel+1)
	{
	//	return false;
	}

	SKILLID_MAP::iterator	iSkill;

	//--------------------------------------------------
	// Find the skill whose id is <id>.
	//--------------------------------------------------
	iSkill = m_mapSkillID.find(id);

	//--------------------------------------------------
	// Return NULL when it is not there.
	//--------------------------------------------------
	if (iSkill == m_mapSkillID.end()) 
	{
		return false;
	}

	//--------------------------------------------------
	// Already learned, so return false
	//--------------------------------------------------
	if ((*iSkill).second==SKILLSTATUS_LEARNED)
	{
		return false;
	}

	//-----------------------------------------------
	// Which level of the domain it sits at.
	//-----------------------------------------------
	int skillLevel = (*g_pSkillInfoTable)[id].GetLevel();

	//-----------------------------------------------
	// The levels come out of the skill file and the array is as long
	// as the deepest skill the tree walk counted, so a skill at a
	// level the walk never saw - or a domain whose array a clear or a
	// load took away - is refused rather than written past. Refused
	// before the skill is made usable, so a refusal leaves nothing
	// behind.
	//-----------------------------------------------
	if (m_pLearnedSkillID==NULL || skillLevel<0 || skillLevel>m_MaxLevel)
	{
		return false;
	}

	//-----------------------------------------------
	// Add it to the skills usable now.
	//-----------------------------------------------
	if (g_pSkillAvailable!=NULL)
	{
		(*g_pSkillAvailable).AddSkill( id );
	}

	// It is what is learned at that level.
	m_pLearnedSkillID[skillLevel] = id;

	if (skillLevel > m_MaxLearnedLevel)
	{
		m_MaxLearnedLevel = skillLevel;

		//--------------------------------------------------
		//
		// Clear every skill currently marked as learnable.
		//
		//--------------------------------------------------
		// The level below the one just learned
		//--------------------------------------------------
		//if (m_MaxLearnedLevel > 0)
		//{	
			///RemoveNextSkill( m_pLearnedSkillID[m_MaxLearnedLevel-1] );
		//}

		//--------------------------------------------------
		// Remove every "can be learned" mark
		//--------------------------------------------------
		SKILLID_MAP::iterator iSkill2 = m_mapSkillID.begin();

		while (iSkill2 != m_mapSkillID.end())
		{
			if ((*iSkill2).second==SKILLSTATUS_NEXT)
			{						
				(*iSkill2).second = SKILLSTATUS_OTHER;
			}

			iSkill2++;
		}

		//--------------------------------------------------
		// Mark it learned.
		//--------------------------------------------------
		(*iSkill).second = SKILLSTATUS_LEARNED;	

		//--------------------------------------------------
		//
		// Find what can be learned next and mark it.
		//
		//--------------------------------------------------
		AddNextSkill( id );
	}	
	else
	{
		//--------------------------------------------------
		// Mark it learned.
		//--------------------------------------------------
		(*iSkill).second = SKILLSTATUS_LEARNED;	
	}

	m_bNewSkill = false;

	return true;
}

//----------------------------------------------------------------------
// UnLearn Skill
//----------------------------------------------------------------------
// Put the skill <id> back into the unlearned state.
//
// What the highest-level skill among those removed
// What it leads to becomes learnable next.
//
//----------------------------------------------------------------------
bool
MSkillDomain::UnLearnSkill(ACTIONINFO id)
{
	//--------------------------------------------------
	// Only the skill at the current top level can be removed.
	//--------------------------------------------------
	if ((*g_pSkillInfoTable)[id].GetLevel()!=m_MaxLearnedLevel)
	{
		return false;
	}

	SKILLID_MAP::iterator	iSkill;

	//--------------------------------------------------
	// Find the skill whose id is <id>.
	//--------------------------------------------------
	iSkill = m_mapSkillID.find(id);

	//--------------------------------------------------
	// Return NULL when it is not there.
	//--------------------------------------------------
	if (iSkill == m_mapSkillID.end()) 
	{
		return false;
	}

	//--------------------------------------------------
	//
	// Change the skill's status to unlearned.
	//
	//--------------------------------------------------
	// Return false unless it was learned
	if ((*iSkill).second!=SKILLSTATUS_LEARNED)
	{
		return false;
	}
	
	//--------------------------------------------------
	// Nothing is learned unless the array that says what is learned
	// at each level is there, and unless the level that indexes it is
	// one the array holds.
	//--------------------------------------------------
	if (m_pLearnedSkillID==NULL || m_MaxLearnedLevel<0 || m_MaxLearnedLevel>m_MaxLevel)
	{
		return false;
	}

	//--------------------------------------------------
	// Mark what the skill being given back led to as out of reach.
	//--------------------------------------------------
	RemoveNextSkill( m_pLearnedSkillID[m_MaxLearnedLevel] );

	//--------------------------------------------------
	// Clear the level.
	//--------------------------------------------------
	m_pLearnedSkillID[m_MaxLearnedLevel] = MAX_ACTIONINFO;//ACTIONINFO_NULL;
	m_MaxLearnedLevel--;


	(*iSkill).second = SKILLSTATUS_OTHER;	// really NEXTSKILL, but never mind

	//-----------------------------------------------
	// Take it out of the skills usable now.
	//-----------------------------------------------
	if (g_pSkillAvailable!=NULL)
	{
		(*g_pSkillAvailable).RemoveSkill( id );
	}
	
	//--------------------------------------------------
	//
	// Find what can be learned next and mark it.
	//
	//--------------------------------------------------
	if (m_MaxLearnedLevel>=0)
	{
		AddNextSkill( m_pLearnedSkillID[m_MaxLearnedLevel] );
	}


	return true;
}


//----------------------------------------------------------------------
// Is Exist SkillStep
//----------------------------------------------------------------------
BOOL
MSkillDomain::IsExistSkillStep(SKILL_STEP ss) const
{
	return m_mapSkillStep.contains( ss ) ? TRUE : FALSE;
}

//----------------------------------------------------------------------
// Get SkillStep List
//----------------------------------------------------------------------
const MSkillDomain::SKILL_STEP_LIST*	
MSkillDomain::GetSkillStepList(SKILL_STEP ss) const
{
	SKILL_STEP_MAP::const_iterator iList = m_mapSkillStep.find( ss );

	if (iList == m_mapSkillStep.end())
	{
		return NULL;
	}

	return iList->second;
}

//----------------------------------------------------------------------
// Add SkillStep List
//----------------------------------------------------------------------
void
MSkillDomain::AddSkillStep(SKILL_STEP ss, ACTIONINFO ai)
{
	SKILL_STEP_LIST* pList;
	SKILL_STEP_MAP::const_iterator iList = m_mapSkillStep.find( ss );

	if (iList == m_mapSkillStep.end())
	{
		// Not there yet, so new one up.
		pList = new SKILL_STEP_LIST;
	}
	else
	{
		pList = iList->second;
	}

	// Is ai in the list already? The list itself is searched; it used
	// to be copied whole for this, once per skill added.
	if( std::ranges::find( *pList, ai ) == pList->end() )
//		pList->push_back( ai );
	{
		
		if( pList->empty() )
		{
			pList->push_back( ai );
		} else
		{
			bool	bAdded = false;
			SKILL_STEP_LIST::iterator itr = pList->begin();
			SKILL_STEP_LIST::iterator endItr = pList->end();	
			int		LearnLevel = (*g_pSkillInfoTable)[ai].GetLearnLevel();
			while( itr != endItr )
			{
				int currentPositionLevel = (*g_pSkillInfoTable)[*itr].GetLearnLevel();
				
				if( currentPositionLevel > LearnLevel )
				{
					pList->insert( itr, ai );
					bAdded = true;
					break;
				}
				
				itr++;
			}
			if(bAdded == false )
			{
				pList->push_back( ai );
			}
		}
	}

	// Store it (again).
	m_mapSkillStep[ss] = pList;
}

//----------------------------------------------------------------------
// Get SkillStep for a skill in this domain
//----------------------------------------------------------------------
// The step is the skill file's, except for the one skill the three
// races share: it sits in a step of its own for each of them, and the
// domain it is being added to is what says which.
//----------------------------------------------------------------------
SKILL_STEP
MSkillDomain::GetSkillStepFor(ACTIONINFO id) const
{
	const SKILL_STEP	skillStep = (*g_pSkillInfoTable)[id].GetSkillStep();

	if (skillStep!=SKILL_STEP_ETC || id!=SKILL_SOUL_CHAIN || g_pSkillManager==NULL)
	{
		return skillStep;
	}

	if (this == &(*g_pSkillManager)[SKILL_DOMAIN_VAMPIRE])
	{
		return SKILL_STEP_VAMPIRE_INNATE;
	}

	if (this == &(*g_pSkillManager)[SKILL_DOMAIN_OUSTERS])
	{
		return SKILL_STEP_OUSTERS_ETC;
	}

	return SKILL_STEP_APPRENTICE;
}

//----------------------------------------------------------------------
// Set State From SkillList
//----------------------------------------------------------------------
// The skill list carries one status per skill and nothing else, while
// the skill window reads the step lists and the learn and unlearn
// paths index an array of what is learned at each level. Both of those
// follow from the list and the skill file, so both are rebuilt here -
// otherwise a list that arrived any way other than a tree walk leaves
// the domain indexing an array it does not have.
//----------------------------------------------------------------------
void
MSkillDomain::SetStateFromSkillList()
{
	SKILLID_MAP::const_iterator	iSkill = m_mapSkillID.begin();

	//-----------------------------------------------
	// The domain reaches as deep as the deepest skill in it.
	//-----------------------------------------------
	m_MaxLevel = 0;

	while (iSkill != m_mapSkillID.end())
	{
		const int	skillLevel = (*g_pSkillInfoTable)[(*iSkill).first].GetLevel();

		if (m_MaxLevel < skillLevel)
		{
			m_MaxLevel = skillLevel;
		}

		AddSkillStep( GetSkillStepFor( (*iSkill).first ), (*iSkill).first );

		iSkill++;
	}

	//-----------------------------------------------
	// An empty array of that depth, then one entry per skill the list
	// says is learned - and those skills are usable again.
	//-----------------------------------------------
	SetMaxLevel();

	iSkill = m_mapSkillID.begin();

	while (iSkill != m_mapSkillID.end())
	{
		const ACTIONINFO	id = (*iSkill).first;
		const int			skillLevel = (*g_pSkillInfoTable)[id].GetLevel();

		if ((*iSkill).second==SKILLSTATUS_LEARNED
			&& skillLevel>=0 && skillLevel<=m_MaxLevel)
		{
			m_pLearnedSkillID[skillLevel] = id;

			if (skillLevel > m_MaxLearnedLevel)
			{
				m_MaxLearnedLevel = skillLevel;
			}

			if (g_pSkillAvailable!=NULL)
			{
				(*g_pSkillAvailable).AddSkill( id );
			}
		}

		iSkill++;
	}
}

//----------------------------------------------------------------------
// Save To File
//----------------------------------------------------------------------
// Skill ID를 File에 저장한다.
//----------------------------------------------------------------------
void		
MSkillDomain::SaveToFile(std::ofstream& file)
{
	SKILLID_MAP::iterator	iSkill = m_mapSkillID.begin();

	// size저장
	int size = m_mapSkillID.size();
	file.write((const char*)&size, 4);

	// 각 id저장
	while (iSkill != m_mapSkillID.end())
	{
		WORD id = (*iSkill).first;
		BYTE status = (*iSkill).second;

		file.write((const char*)&id, 2);
		file.write((const char*)&status, 1);

		iSkill++;
	}    
}

//----------------------------------------------------------------------
// Load From File
//----------------------------------------------------------------------
// Skill ID를 File에서 읽어온다.
//----------------------------------------------------------------------
void		
MSkillDomain::LoadFromFile(std::ifstream& file)
{
	Clear();

	// How many skills follow.
	int size = 0;

	if (file.read((char*)&size, 4) && size>0)
	{
		// Read them and store them.
		for (int i=0; i<size; i++)
		{
			WORD id = 0;
			BYTE status = 0;

			//-----------------------------------------------
			// Three bytes to a skill, so a count larger than what is
			// left of the file names skills that are not there.
			// Reading them used to leave the last id and status in
			// place and store that one skill over and over - and on
			// an empty file, whatever the two locals happened to
			// hold.
			//-----------------------------------------------
			if (!file.read((char*)&id, 2) || !file.read((char*)&status, 1))
			{
				break;
			}

			// Neither a status the enum does not have nor the one
			// that means the skill is not in the domain at all.
			if (status==SKILLSTATUS_NULL || status>SKILLSTATUS_OTHER)
			{
				continue;
			}

			m_mapSkillID.insert(SKILLID_MAP::value_type(
											(enum ACTIONINFO)id,
											(enum SKILLSTATUS)status ));
		}
	}

	// The file carries the statuses only; the rest follows from them.
	SetStateFromSkillList();
}

//----------------------------------------------------------------------
// LoadFromFileServerDomainInfo
//----------------------------------------------------------------------
void		
MSkillDomain::LoadFromFileServerDomainInfo(std::ifstream& file)
{
	int level = 0;

	if (!file.read((char*)&level, 4))
	{
		return;
	}

	//--------------------------------------------------
	// The row is read whether or not it can be stored, so that a
	// level this table cannot hold costs one row and not the rest of
	// the file: the manager's loop above reads the next domain from
	// wherever this leaves the stream.
	//--------------------------------------------------
	ExpInfo	info;

	info.GoalExp = 0;
	info.AccumExp = 0;
	info.LoadFromFile( file );
	if (!file.good())
		return;

	//--------------------------------------------------
	// The level names the row to fill. An invalid level consumes its
	// complete record but must not alter this table or a fallback value.
	//--------------------------------------------------
	if (level<0 || level>=m_DomainExpTable.GetSize())
	{
		return;
	}

	m_DomainExpTable.Set(level, info);
}

bool		
MSkillDomain::IsAvailableDeleteSkill(ACTIONINFO id)
{	
	const SKILLINFO_NODE::SKILLID_LIST& listNextSkill = (*g_pSkillInfoTable)[id].GetNextSkillList();

	SKILLINFO_NODE::SKILLID_LIST::const_iterator iNextSkill = listNextSkill.begin();

	while (iNextSkill != listNextSkill.end())
	{
		//--------------------------------------------------
		// ID가 *iNextSkil인 Skill를 찾는다.
		//--------------------------------------------------
		if(false == (*g_pSkillInfoTable)[*iNextSkill].CanDelete)
			return false;
		SKILLID_MAP::iterator iSkill = m_mapSkillID.find( *iNextSkill );

		if (iSkill != m_mapSkillID.end())
		{
			if ((*iSkill).second==SKILLSTATUS_LEARNED)
			{						
				return false;
			}
		}

		iNextSkill++;
	}
	return true;
}

//----------------------------------------------------------------------
// Get ExpInfo
//----------------------------------------------------------------------
const ExpInfo&	
MSkillDomain::GetExpInfo(int level) const
{
	return m_DomainExpTable[level];
}

//----------------------------------------------------------------------
//
// MSkillManager
//
//----------------------------------------------------------------------
MSkillManager::MSkillManager()
{
}

MSkillManager::~MSkillManager()
{
}

//----------------------------------------------------------------------
// Init
//----------------------------------------------------------------------
void
MSkillManager::Init()
{
	//--------------------------------------------------
	//
	// Skill Tree 초기화
	//
	//--------------------------------------------------
	CTypeTable<MSkillDomain>::Init( MAX_SKILLDOMAIN );
	//--------------------------------------------------
	// 기본 기술로부터 skill tree를 초기화한다.
	//--------------------------------------------------

	m_pTypeInfo[SKILLDOMAIN_BLADE].SetRootSkill( SKILL_SINGLE_BLOW );
	m_pTypeInfo[SKILLDOMAIN_SWORD].SetRootSkill( SKILL_DOUBLE_IMPACT );
	m_pTypeInfo[SKILLDOMAIN_GUN].SetRootSkill( SKILL_FAST_RELOAD );
	m_pTypeInfo[SKILLDOMAIN_ENCHANT].SetRootSkill( MAGIC_CREATE_HOLY_WATER );
	m_pTypeInfo[SKILLDOMAIN_HEAL].SetRootSkill( MAGIC_CURE_LIGHT_WOUNDS );
	m_pTypeInfo[SKILLDOMAIN_VAMPIRE].SetRootSkill( MAGIC_HIDE );
	m_pTypeInfo[SKILLDOMAIN_OUSTERS].SetRootSkill( SKILL_FLOURISH );
	m_pTypeInfo[SKILLDOMAIN_ETC].SetRootSkill( SKILL_SOUL_CHAIN );

	// The per-domain experience the server ships is read from a file
	// the executable opens, through LoadFromFileServerDomainInfo below
	// (InitSkillTree in GameInit.cpp).
}

void
MSkillManager::InitSkillList()
{
	for(int i = SKILLDOMAIN_BLADE; i < MAX_SKILLDOMAIN; i++ )
		m_pTypeInfo[i].ClearSkillList();

	m_pTypeInfo[SKILLDOMAIN_BLADE].SetRootSkill( SKILL_SINGLE_BLOW , false);
	m_pTypeInfo[SKILLDOMAIN_SWORD].SetRootSkill( SKILL_DOUBLE_IMPACT , false);
	m_pTypeInfo[SKILLDOMAIN_GUN].SetRootSkill( SKILL_FAST_RELOAD , false);
	m_pTypeInfo[SKILLDOMAIN_ENCHANT].SetRootSkill( MAGIC_CREATE_HOLY_WATER , false);
	m_pTypeInfo[SKILLDOMAIN_HEAL].SetRootSkill( MAGIC_CURE_LIGHT_WOUNDS , false);
	m_pTypeInfo[SKILLDOMAIN_VAMPIRE].SetRootSkill( MAGIC_HIDE , false);
	m_pTypeInfo[SKILLDOMAIN_OUSTERS].SetRootSkill( SKILL_FLOURISH , false);
	m_pTypeInfo[SKILLDOMAIN_ETC].SetRootSkill( SKILL_SOUL_CHAIN , false);
}



//----------------------------------------------------------------------
// LoadFromFileServerSkillInfo
//----------------------------------------------------------------------
void		
MSkillManager::LoadFromFileServerDomainInfo(std::ifstream& file)
{
	int num = 0;
	// A record contains domain, level, goal and accumulated experience.
	if (!file.read((char*)&num, 4) || !IsEntryCountSane(file, num, 16))
	{
		file.setstate(std::ios::failbit);
		return;
	}

	// The count and each domain are the file's. m_pTypeInfo is indexed
	// raw here, past even the typed table's own bound, so a domain the
	// file names outside the table would write through a wild pointer;
	// and once a row is refused the stream is no longer where the next
	// row begins, so reading stops.
	for (int i=0; i<num; i++)
	{
		int domain = 0;
		if (!file.read((char*)&domain, 4))
		{
			return;
		}

		if (domain < 0 || domain >= GetSize())
		{
			file.setstate(std::ios::failbit);
			return;
		}

		m_pTypeInfo[domain].LoadFromFileServerDomainInfo( file );
		if (!file.good())
			return;
	}
}
