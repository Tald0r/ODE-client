//----------------------------------------------------------------------
// MGuidanceEffect.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MGuidanceEffect.h"

const MGuidanceEffectHost* MGuidanceEffect::s_pHost = nullptr;

const MGuidanceEffectHost* MGuidanceEffect::SetHost(const MGuidanceEffectHost* host)
{
	const auto* previous = s_pHost;
	s_pHost = host;
	return previous;
}

bool MGuidanceEffect::ReadCreaturePosition(TYPE_OBJECTID id, int& x, int& y, int& z)
{
	x = y = z = 0;
	return s_pHost && s_pHost->CreaturePosition && s_pHost->CreaturePosition(id, x, y, z);
}

//----------------------------------------------------------------------
// 
// constructor/destructor
//
//----------------------------------------------------------------------

MGuidanceEffect::MGuidanceEffect(BYTE bltType)
: MLinearEffect(bltType)
{
	//m_EffectType	= EFFECT_GUIDANCE;

	m_CreatureID	= OBJECTID_NULL;
}

MGuidanceEffect::~MGuidanceEffect()
{
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Move
//----------------------------------------------------------------------
// 매 순간마다 StepX~Z가 달라진다.
//----------------------------------------------------------------------
void				
MGuidanceEffect::SetTraceCreatureID(TYPE_OBJECTID id)
{ 
	m_CreatureID = id; 

	if (id!=OBJECTID_NULL)
	{
		TraceCreature();	
	}
}

//----------------------------------------------------------------------
// TraceCreature
//----------------------------------------------------------------------
bool
MGuidanceEffect::TraceCreature()
{
	int x, y, z;

	// Creature가 사라졌을 경우..
	if (!ReadCreaturePosition(m_CreatureID, x, y, z))
	{
		m_CreatureID = OBJECTID_NULL;
		m_EndFrame = 0;
		return false;
	}

	// 새로운 목적지 설정
	MLinearEffect::SetTarget(x, y, z, m_StepPixel);

	return true;
}

//----------------------------------------------------------------------
// Update
//----------------------------------------------------------------------
bool
MGuidanceEffect::Update()
{	
	if (!IsEnd())
	{
		if (!TraceCreature())
			return false;

		if (AdvanceLinear(m_PixelX, m_PixelY, m_PixelZ, m_StepPixel))
		{
			//------------------------------------------
			// 더 움직일 필요가 없는 경우이다.			
			//------------------------------------------
			m_EndFrame = 0;

			return false;
		}

		//--------------------------------
		// Sector 좌표를 맞춘다.
		//--------------------------------
		AffectPosition();

		//--------------------------------
		// Frame을 바꿔준다.
		//--------------------------------
		NextFrame();

		if (m_BltType == BLT_EFFECT)
		{
			RefreshLight();
		}

		//--------------------------------
		// Counter를 하나 줄인다.
		//--------------------------------
		//m_Count--;

		return true;
	}

	// 끝~

	return false;
}
