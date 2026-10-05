//----------------------------------------------------------------------
// MEffectTarget.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MEffectTarget.h"
#include "MZoneTable.h"
#include "DebugLog.h"

const MEffectTargetHost* MEffectTarget::s_pHost = nullptr;

const MEffectTargetHost* MEffectTarget::SetHost(const MEffectTargetHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

void MEffectTarget::RemoveFromPlayer(BYTE id)
{
	if (s_pHost && s_pHost->RemoveFromPlayer) s_pHost->RemoveFromPlayer(id);
}

void MEffectTarget::RemovePlayerRegistration()
{
	if (m_bRemovePlayerRegistration) RemoveFromPlayer(m_EffectID);
}

void MEffectTarget::ReleasePendingOwner() noexcept
{
	if (m_pPendingOwner != nullptr)
	{
		m_pPendingOwner->m_pTarget = nullptr;
		m_pPendingOwner = nullptr;
	}
}

MEffectTargetOwner::MEffectTargetOwner(MEffectTarget* target) noexcept
	: m_pTarget(nullptr)
{
	if (target != nullptr && !target->m_bDestroying)
	{
		target->ReleasePendingOwner();
		m_pTarget = target;
		target->m_pPendingOwner = this;
	}
}

MEffectTargetOwner::~MEffectTargetOwner()
{
	if (m_pTarget != nullptr)
	{
		auto* target = m_pTarget;
		// Disarm before invoking target/result destructors and host callbacks.
		target->ReleasePendingOwner();
		delete target;
	}
}

//----------------------------------------------------------------------
// Static member
//----------------------------------------------------------------------
BYTE		MEffectTarget::s_EffectID	= 0;

//----------------------------------------------------------------------
//
//						MEffectTarget
//
//----------------------------------------------------------------------
MEffectTarget::MEffectTarget(const MEffectTarget& target)			
{
	m_pResult = NULL; 

	*this = target;

	// Preserve the shared ID used to correlate this visual continuation.
	m_EffectID = target.m_EffectID;
	// Only the original is tracked by the player; visual branches share its ID.
	m_bRemovePlayerRegistration = false;
	
	m_ServerID = OBJECTID_NULL;

	m_bResultTime = false;
}	

MEffectTarget::MEffectTarget(BYTE max)
{			
	m_MaxPhase		= max;
	m_CurrentPhase	= 0;

	// 결과
	//m_nResultActionInfo = ACTIONINFO_NULL;
	m_pResult		= NULL;

	// 객체 ID를 할당한다.
	//m_InstanceID = s_InstanceID++;
	m_EffectID = 0;

	m_bResultTime = false;

	m_DelayFrame	= 0;
	m_X = m_Y = m_Z = 0;
	m_ID = OBJECTID_NULL;
	m_ServerID = OBJECTID_NULL;
}

MEffectTarget::~MEffectTarget() 
{ 
	m_bDestroying = true;
	ReleasePendingOwner();
	DEBUG_ADD_FORMAT("delete EffectTarget. id=%d", (int)m_EffectID);
	auto* result = m_pResult;
	m_pResult = nullptr;
	delete result;

	DEBUG_ADD("del res");

	// Visual copies must not remove the original target from the player roster.
	RemovePlayerRegistration();

	DEBUG_ADD("del ok");
}

//----------------------------------------------------------------------
// Set Result
//----------------------------------------------------------------------
void			
MEffectTarget::SetResult(MActionResult* pResult)
{ 
	if (m_pResult == pResult) return;
	if (m_bDestroying)
	{
		delete pResult;
		return;
	}
	auto* previous = m_pResult;
	m_pResult = pResult;
	delete previous;
}
		
//----------------------------------------------------------------------
// assign
//----------------------------------------------------------------------
void	
MEffectTarget::operator = (const MEffectTarget& target)
{
	m_MaxPhase		= target.m_MaxPhase;
	m_CurrentPhase	= target.m_CurrentPhase;
	m_DelayFrame	= target.m_DelayFrame;
	m_X				= target.m_X;
	m_Y				= target.m_Y;
	m_Z				= target.m_Z;
	m_ID			= target.m_ID;
	m_ServerID		= target.m_ServerID;
}



//----------------------------------------------------------------------
//
//					MPortalEffectTarget
//
//----------------------------------------------------------------------
MPortalEffectTarget::MPortalEffectTarget(const MEffectTarget& target)			
: MEffectTarget(target)
{
	m_ZoneID = OBJECTID_NULL;
	m_ZoneX = SECTORPOSITION_NULL;
	m_ZoneY = SECTORPOSITION_NULL;
}	

MPortalEffectTarget::MPortalEffectTarget(BYTE max)
: MEffectTarget(max)
{
	m_ZoneID = OBJECTID_NULL;
	m_ZoneX = SECTORPOSITION_NULL;
	m_ZoneY = SECTORPOSITION_NULL;
}

MPortalEffectTarget::~MPortalEffectTarget()
{
}

//------------------------------------------------------------------
// assign
//------------------------------------------------------------------
void	
MPortalEffectTarget::operator = (const MEffectTarget& target)
{
	MEffectTarget::operator=(target);

	if (target.GetEffectTargetType()==EFFECT_TARGET_PORTAL)
	{
		const MPortalEffectTarget& portalTarget = (const MPortalEffectTarget&)target;

		m_OwnerName = portalTarget.m_OwnerName;
		m_ZoneID = portalTarget.m_ZoneID;
		m_ZoneX = portalTarget.m_ZoneX;
		m_ZoneY = portalTarget.m_ZoneY;
	}
}

//------------------------------------------------------------------
// Get ZoneName
//------------------------------------------------------------------
const char*				
MPortalEffectTarget::GetZoneName() const
{
	if (g_pZoneTable!=NULL)
	{
		ZONETABLE_INFO* pZoneInfo = g_pZoneTable->Get( m_ZoneID );

		if (pZoneInfo!=NULL)
		{
			return pZoneInfo->Name.GetString();
		}
	}
	
	return NULL;
}
