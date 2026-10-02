//-----------------------------------------------------------------------------------------------
// MWarManager
//  Date : 2003.2
//  
//  by sonee
// 
//  전쟁 정보를 가지고 있는다.
//  전쟁 정보에 따라 UI 에서 확인 할 수 있도록 Push 해준다.
//-----------------------------------------------------------------------------------------------

#ifndef __MWARMANAGER_H__
#define __MWARMANAGER_H__

//#include "types.h"
#include "WarInfo.h"

#include <map>
#include <list>
#include <vector>

enum WAR_TYPE_ID // 안씀
{
	WAR_TYPE_HOLYLAND,					// 아담의 성지
	WAR_TYPE_GUILD,						// 길드전 
};

typedef			std::map<ZoneID_t, WarInfo*>			WarInfoMap;
typedef			WarInfoMap::iterator			WarInfoMapItr;

// Borrowed services installed by GameInit. Missing actions are skipped;
// without a current zone, level-war presentation is skipped.
struct MWarHost
{
	bool (*ReadZone)(ZoneID_t& id) = nullptr;
	void (*RaceWarNotice)(DWORD startTime) = nullptr;
	void (*RaceWarStarted)() = nullptr;
	void (*RaceWarEnded)() = nullptr;
};

class MWarManager
{
private :

// 	list<ZoneID_t>		HolyLandZone;							// 아담의 성지 필드 존 ID List	
	WarInfoMap			m_WarInfo;								// 전쟁 정보 Map<ZoneID>

public :	
	MWarManager();
	~MWarManager();	
	MWarManager(const MWarManager&) = delete;
	MWarManager& operator=(const MWarManager&) = delete;
	static const MWarHost* SetHost(const MWarHost* host);

	//-------------------------------------------------------------------
	// SetData
	//-------------------------------------------------------------------
	// Consumes a packet record. Shared zone entries keep it alive until the
	// last entry is removed; re-registering an already owned pointer is safe.
	void			SetWar(WarInfo *info);
	void			RemoveWar(ZoneID_t id);						// WarList Remove
	
	void			ClearWar();									// Clear
	void			ClearRaceWar();

	//-------------------------------------------------------------------
	// Get Data
	//-------------------------------------------------------------------
	WarInfo*						GetWarInfo(ZoneID_t id);
	const WarInfoMap&				getWarInfoList() { return m_WarInfo; }
	int								getSize() { return static_cast<int>(m_WarInfo.size()); }
	
	//-------------------------------------------------------------------
	// Check
	//-------------------------------------------------------------------
	bool			IsExist(ZoneID_t id);						// 존아이디가 전쟁에 포함되어있는가
// 	bool			IsHolyLand(ZoneID_t id);					// 성이 아니라 아담의 성지 필드인가
	
	void			Update();	

private:
	bool Owns(const WarInfo* info) const;
	void ReleaseUnreferenced(WarInfo* info);
	void Store(ZoneID_t id, WarInfo* info);
	static void UpdateRow(ZoneID_t id, const WarInfo& info);
	static bool ReadZone(ZoneID_t& id);
	static void RaceWarNotice(DWORD startTime);
	static void RaceWarStarted();
	static void RaceWarEnded();
	static const MWarHost* s_pHost;
};

extern MWarManager	*g_pWarManager;

#endif
