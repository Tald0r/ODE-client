//----------------------------------------------------------------------
// MGuidanceEffect.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "Client.h"
#include "MLinearEffect.h"
#include "MGuidanceEffect.h"
#include "MCreature.h"
#include "MZone.h"
#include "MTopView.h"

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
	MCreature* pCreature = g_pZone->GetCreature( m_CreatureID );

	// Creature가 사라졌을 경우..
	if (pCreature == NULL)
	{
		m_CreatureID = OBJECTID_NULL;
		m_EndFrame = 0;
		return false;
	}

	// 현재의 좌표를 읽어온다.
	POINT point;// = MTopView::MapToPixel(pCreature->GetX(), pCreature->GetY());
	//point.x += pCreature->GetSX();
	//point.y += pCreature->GetSY();	
	point.x = pCreature->GetPixelX();
	point.y = pCreature->GetPixelY();

	// 새로운 목적지 설정
	MLinearEffect::SetTarget( point.x, point.y, 
								pCreature->GetZ(),
								m_StepPixel);

	return true;
}

//----------------------------------------------------------------------
// Update
//----------------------------------------------------------------------
bool
MGuidanceEffect::Update()
{	
	if (g_CurrentFrame < m_EndFrame)
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
			m_Light = g_pTopView->m_EffectAlphaFPK[m_FrameID][m_Direction][m_CurrentFrame].GetLight();
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
