//----------------------------------------------------------------------
// MLinearEffect.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include "MTopView.h"
#include "MTypeDef.h"
#include "MMovingEffect.h"
#include "MLinearEffect.h"
#include "SkillDef.h"
//----------------------------------------------------------------------
//
// constructor/destructor
//
//----------------------------------------------------------------------

MLinearEffect::MLinearEffect(BYTE bltType)
: MMovingEffect(bltType)
{
	m_StepPixel		= 0;
}

MLinearEffect::~MLinearEffect()
{
}

//----------------------------------------------------------------------
//
// member functions
//
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Set Target
//----------------------------------------------------------------------
// 입력받은 목표 위치에 대해서
// 현재 위치로부터 목표 위치까지 이동하는데 대한 
// speed만큼 이동한다.
//----------------------------------------------------------------------
void
MLinearEffect::SetTarget(int x, int y, int z, WORD stepPixel)
{
	SetLinearTarget(m_PixelX, m_PixelY, m_PixelZ, x, y, z, stepPixel);
	// 이동하는 방향 설정
	m_Direction = MTopView::GetDirectionToPosition((int)m_PixelX, (int)m_PixelY, (int)m_TargetX, (int)m_TargetY);
	m_StepPixel = stepPixel;
}

//----------------------------------------------------------------------
// Update
//----------------------------------------------------------------------
// m_Count가 0일때까지 -1 해주면서 Frame을 바꾼다.
// Pixel좌표 이동도 해야한다. 
// 당연히 Sector좌표도 맞춰야 한다.
//
// 목표 Pixel위치까지 일정 Pixel씩 이동하게 된다.
//
// return 
//    true  - 계속 움직인다...
//    false - 이번이 마지막 움직임이라는 의미
//----------------------------------------------------------------------
bool
MLinearEffect::Update()
{
	// 계속 Update해도 되는가?
	if (g_CurrentFrame < m_EndFrame)
	{
		if (AdvanceLinear(m_PixelX, m_PixelY, m_PixelZ, m_StepPixel))
		{
			//------------------------------------------
			// 더 움직일 필요가 없는 경우이다.			
			//------------------------------------------
			// 2004, 12, 30, sobeit add start
			if(GetActionInfo() == SKILL_HALO) // 0.5초간 최종 위치에 대기
			{
				if(m_EndFrame>g_CurrentFrame+8)
					m_EndFrame = g_CurrentFrame+8;
			}
			else
			// 2004, 12, 30, sobeit add end
			{	
				m_EndFrame = 0;
				return false;
			}
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
