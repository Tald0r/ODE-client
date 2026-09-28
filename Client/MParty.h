//----------------------------------------------------------------------
// MParty.h
//----------------------------------------------------------------------
// creatureID나 creatureName이 둘 다 key가 될 수 있다.
// 파티원은 최대수는 6.. 언젠가 늘어날 수도 있겠지만..
// key값 두개인거도 애매하고 max가 6이므로 map은 안쓰기로.. ㅋㅋ..
//
// 누가 party에 있다는거만 알면 된다.
// 그리고.. 근처에 있는 경우(g_pZone에 있는 경우) m_ID를 통해서 
// HP나 기타 정보들을 출력해주면 되겠지.
//
// GetMemberXXX( n )의 n은 array의 index이다.
//----------------------------------------------------------------------

#ifndef __MParty_H__
#define __MParty_H__

#ifdef _MSC_VER
#pragma warning(disable:4786)
#endif

#include <vector>

#include "MString.h"
#include "MTypeDef.h"
#include "MonotonicClock.h"

class PARTY_INFO
{
public :
	MString			Name;		// 이름
	TYPE_OBJECTID	ID;			// object ID
	bool			bMale;
	BYTE			hairStyle;

	// guildID
	MString			guildName;
	WORD			guildID;
	
	// 현재 있는 곳의 정보
	int				zoneID;
	BYTE			zoneX;
	BYTE			zoneY;

	// 에노지..
	WORD			MaxHP;
	WORD			HP;
	
	// IP
	std::string		IP;
	bool			bInSight;

public :
	PARTY_INFO();
};

// Borrowed callbacks installed by the executable. No host means no live
// creatures and no kick delay. Membership records remain owned by MParty.
struct MPartyHost {
	bool (*JoinByID)(TYPE_OBJECTID, MString& name) = nullptr;
	TYPE_OBJECTID (*JoinByName)(const char*) = nullptr;
	void (*LeaveByID)(TYPE_OBJECTID) = nullptr;
	void (*LeaveByName)(const char*) = nullptr;
	MonotonicClock::TimePoint (*CurrentTime)() = nullptr;
};

class MParty {
	public :
		typedef std::vector<PARTY_INFO*>	PARTY_VECTOR;

	public :
		static const MPartyHost* SetHost(const MPartyHost* host);
		static constexpr DWORD DefaultKickDelayMs = 60 * 60 * 1000;

		MParty();
		~MParty();

		void		Release();

		//--------------------------------------------------------
		// SetJoinTime
		//--------------------------------------------------------
		MonotonicClock::TimePoint	GetJoinTime() const	{ return m_JoinTime; }		
		void		SetJoinTime(MonotonicClock::TimePoint time)	{ m_JoinTime = time; }
		void		SetJoinTime();		
		bool		IsKickAvailableTime() const;

		//--------------------------------------------------------
		// Get Size - 현재 파티원의 수 (나를 제외한)
		//--------------------------------------------------------
		int			GetSize() const		{ return static_cast<int>(m_pInfo.size()); }

		//--------------------------------------------------------
		// Add / Remove
		//--------------------------------------------------------
		bool		AddMember(PARTY_INFO* pInfo);
		bool		RemoveMember(const char* pName);
		bool		RemoveMember(int creatureID);	
		void		UnSetPlayerParty() const;

		//--------------------------------------------------------
		// Accept/Refuse Invite
		//--------------------------------------------------------
		bool		IsAcceptInvite() const		{ return m_bAccept; }
		void		AcceptInvite()				{ m_bAccept = true; }
		void		RefuseInvite()				{ m_bAccept = false; }

		//--------------------------------------------------------
		// Get ID / Name
		//--------------------------------------------------------
		PARTY_INFO*	GetMemberInfo(int n) const;	
		PARTY_INFO*	GetMemberInfo(const char* pName) const;	
		PARTY_INFO* GetMemberInfoByIP(const char* pIP) const;

		bool		HasMember(const char* pName) const;

		//--------------------------------------------------------
		// Is Member In Sight - in the player's zone and within
		// 1.5 times the sight range (sum of the tile distances)
		// of the player's position
		//--------------------------------------------------------
		static bool	IsMemberInSight(int memberZone, int memberX, int memberY,
									int myZone, int myX, int myY, int sight);

	private:
		static const MPartyHost* s_Host;
		static bool JoinByID(TYPE_OBJECTID id, MString& name);
		static TYPE_OBJECTID JoinByName(const char* name);
		static void LeaveByID(TYPE_OBJECTID id);
		static void LeaveByName(const char* name);
		static MonotonicClock::TimePoint Clock();

	protected :
		int							m_Size;			// 파티원 수
		PARTY_VECTOR				m_pInfo;		// party 정보

		bool						m_bAccept;		// 초대를 받아들이는가?

		MonotonicClock::TimePoint	m_JoinTime;
};

extern MParty*	g_pParty;

#endif

