//----------------------------------------------------------------------
// MParty.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MParty.h"

#include <cstdlib>

#include "ClientConfig.h"

#define	MAX_PARTY_MEMBER	6

//----------------------------------------------------------------------
// Global
//----------------------------------------------------------------------
MParty*	g_pParty = NULL;

const MPartyHost* MParty::s_Host = nullptr;

const MPartyHost* MParty::SetHost(const MPartyHost* host)
{
	const auto* previous = s_Host;
	s_Host = host;
	return previous;
}

bool MParty::JoinByID(TYPE_OBJECTID id, MString& name)
{
	return s_Host && s_Host->JoinByID && s_Host->JoinByID(id, name);
}

TYPE_OBJECTID MParty::JoinByName(const char* name)
{
	return s_Host && s_Host->JoinByName ? s_Host->JoinByName(name) : OBJECTID_NULL;
}

void MParty::LeaveByID(TYPE_OBJECTID id)
{
	if (s_Host && s_Host->LeaveByID) s_Host->LeaveByID(id);
}

void MParty::LeaveByName(const char* name)
{
	if (s_Host && s_Host->LeaveByName) s_Host->LeaveByName(name);
}

MonotonicClock::TimePoint MParty::Clock()
{
	return s_Host && s_Host->CurrentTime ? s_Host->CurrentTime() : MonotonicClock::TimePoint();
}


//----------------------------------------------------------------------
// PARTY_INFO
//----------------------------------------------------------------------
PARTY_INFO::PARTY_INFO()	
{ 
	ID = OBJECTID_NULL; 
	bMale = true;
	hairStyle = 0;
	guildID = 0;
	
	zoneID = 0;
	zoneX = 0;
	zoneY = 0;

	MaxHP = HP = 100;
	
	bInSight = false;
}

//----------------------------------------------------------------------
//
// MParty
//
//----------------------------------------------------------------------
MParty::MParty()
{
	m_pInfo.reserve( MAX_PARTY_MEMBER );

	m_bAccept = true;
	m_JoinTime = MonotonicClock::TimePoint();
}

MParty::~MParty()
{
	Release();
}

//----------------------------------------------------------------------
// Release
//----------------------------------------------------------------------
void
MParty::Release()
{
	PARTY_VECTOR::iterator iInfo = m_pInfo.begin();

	while (iInfo != m_pInfo.end())
	{
		PARTY_INFO* pInfo = *iInfo;

		if (pInfo!=NULL)
		{
			delete pInfo;
		}

		iInfo ++;
	}

	m_pInfo.clear();

	//m_JoinTime = 0xFFFFFFFF;
}

//----------------------------------------------------------------------
// UnSetPlayerParty
//----------------------------------------------------------------------
void
MParty::UnSetPlayerParty() const
{
	for (const auto* info : m_pInfo)
		if (info) LeaveByName(info->Name.GetString());
}

//----------------------------------------------------------------------
// Add Member
//----------------------------------------------------------------------
bool		
MParty::AddMember(PARTY_INFO* pInfo)
{
	if (pInfo == nullptr
		|| (pInfo->Name.GetString() == nullptr && pInfo->ID == OBJECTID_NULL))
		return false;
	if (static_cast<size_t>(GetSize()) >= m_pInfo.capacity())
		return false;

	if (pInfo->Name.GetString() == nullptr)
	{
		if (!JoinByID(pInfo->ID, pInfo->Name)) return false;
	}
	else if (pInfo->ID == OBJECTID_NULL)
	{
		pInfo->ID = JoinByName(pInfo->Name.GetString());
	}

	m_pInfo.push_back(pInfo);
	return true;
}

//----------------------------------------------------------------------
// Remove Member
//----------------------------------------------------------------------
bool		
MParty::RemoveMember(const char* pName)
{
	for (auto member = m_pInfo.begin(); member != m_pInfo.end(); ++member)
	{
		PARTY_INFO* info = *member;
		if (info && info->Name == pName)
		{
			LeaveByName(pName);
			delete info;
			m_pInfo.erase(member);
			return true;
		}
	}
	return false;
}

//----------------------------------------------------------------------
// Remove Member
//----------------------------------------------------------------------
bool		
MParty::RemoveMember(int creatureID)
{
	for (auto member = m_pInfo.begin(); member != m_pInfo.end(); ++member)
	{
		PARTY_INFO* info = *member;
		if (info && info->ID == static_cast<TYPE_OBJECTID>(creatureID))
		{
			LeaveByID(creatureID);
			delete info;
			m_pInfo.erase(member);
			return true;
		}
	}
	return false;
}

//----------------------------------------------------------------------
// Get MemberInfo
//----------------------------------------------------------------------
PARTY_INFO*	
MParty::GetMemberInfo(int n) const
{
	if (n>=0 && static_cast<size_t>(n) < m_pInfo.size())
	{
		return m_pInfo[n];
	}

	return NULL;
}

//----------------------------------------------------------------------
// Get Member
//----------------------------------------------------------------------
PARTY_INFO*	
MParty::GetMemberInfo(const char* pName) const
{
	if (pName==NULL || pName[0]=='\0')
	{
		return NULL;
	}

	PARTY_VECTOR::const_iterator iInfo = m_pInfo.begin();

	while (iInfo != m_pInfo.end())
	{
		PARTY_INFO* pInfo = *iInfo;

		if (pInfo!=NULL && pInfo->Name==pName)
		{
			return pInfo;
		}

		iInfo ++;
	}

	return NULL;
}

//----------------------------------------------------------------------
// Get MemberInfoByIP
//----------------------------------------------------------------------
PARTY_INFO*	
MParty::GetMemberInfoByIP(const char* pIP) const
{
	if (pIP==NULL || pIP[0]=='\0')
	{
		return NULL;
	}

	PARTY_VECTOR::const_iterator iInfo = m_pInfo.begin();

	while (iInfo != m_pInfo.end())
	{
		PARTY_INFO* pInfo = *iInfo;

		if (pInfo!=NULL && pInfo->IP==pIP)
		{
			return pInfo;
		}

		iInfo ++;
	}

	return NULL;
}

//----------------------------------------------------------------------
// Has Member
//----------------------------------------------------------------------
bool		
MParty::HasMember(const char* pName) const
{
	if (pName==NULL || pName[0]=='\0')
	{
		return false;
	}

	PARTY_VECTOR::const_iterator iInfo = m_pInfo.begin();

	while (iInfo != m_pInfo.end())
	{
		PARTY_INFO* pInfo = *iInfo;

		if (pInfo!=NULL && pInfo->Name==pName)
		{
			return true;
		}

		iInfo ++;
	}

	return false;
}

//----------------------------------------------------------------------
// Is Member In Sight
//----------------------------------------------------------------------
bool
MParty::IsMemberInSight(int /*memberZone*/, int memberX, int memberY,
						int /*myZone*/, int myX, int myY, int sight)
{
	const int sight15 = sight + (sight>>1);

	return (std::abs(myX-memberX) + std::abs(myY-memberY)) <= sight15;
}

//----------------------------------------------------------------------
// Set JoinTime
//----------------------------------------------------------------------
void		
MParty::SetJoinTime()
{
	m_JoinTime = Clock();
}

//----------------------------------------------------------------------
// Is Kick AvailableTime
//----------------------------------------------------------------------
bool		
MParty::IsKickAvailableTime() const
{
	if (!s_Host || !s_Host->CurrentTime) return true;
	const DWORD delay = g_pClientConfig ? g_pClientConfig->AFTER_PARTY_KICK_DELAY : DefaultKickDelayMs;
	return Clock() - m_JoinTime > MonotonicClock::Millis(delay);
}