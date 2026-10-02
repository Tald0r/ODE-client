//----------------------------------------------------------------------
// MChaseEffect.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MChaseEffect.h"

//----------------------------------------------------------------------
// 
// constructor/destructor
//
//----------------------------------------------------------------------

MChaseEffect::MChaseEffect(BYTE bltType)
: MGuidanceEffect(bltType)
{
	m_bChaseOver = false;
}

MChaseEffect::~MChaseEffect()
{
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Update
//----------------------------------------------------------------------
// ChaseEffect는 목표가 사라지기 전까지는 끝이 없다. - -;
//----------------------------------------------------------------------
bool
MChaseEffect::Update()
{	
	//if (g_CurrentFrame < m_EndFrame)
	{
		// CreatureID가 설정되어 있으면 Creature를 추적하고
		// 아니면.. 목표 좌표까지 이동하게 한다.
		if (m_CreatureID!=OBJECTID_NULL && !TraceCreature())
			return false;

		//--------------------------------
		// Frame을 바꿔준다.
		//--------------------------------
		NextFrame();

		if (AdvanceLinear(m_PixelX, m_PixelY, m_PixelZ, m_StepPixel))
		{
			m_bChaseOver = true;

			return true;
		}
		else
		{
			m_bChaseOver = false;
		}

		//--------------------------------
		// Sector 좌표를 맞춘다.
		//--------------------------------
		AffectPosition();

		
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
