//----------------------------------------------------------------------
// MHomingEffect.cpp
//----------------------------------------------------------------------
#include "Client_PCH.h"
#include <math.h>
#include "MHomingEffect.h"
#include "MathTable.h"
#include "SkillDef.h"
//----------------------------------------------------------------------
// 
// constructor/destructor
//
//----------------------------------------------------------------------

MHomingEffect::MHomingEffect(BYTE bltType, int currentAngle, int turnAngle)
: MGuidanceEffect(bltType), m_Steering(currentAngle, turnAngle)
{		
}

MHomingEffect::~MHomingEffect()
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
void		
MHomingEffect::SetTarget(int x, int y, int z, WORD speed)
{
	// 목표 설정..
	//MLinearEffect::SetTarget(x, y, z, speed);
	m_TargetX = x;
	m_TargetY = y;
	m_TargetZ = z;
	m_StepPixel = speed;

	// 임시로 - -;	
	m_StepZ = (m_TargetZ - m_PixelZ) / 16.0f;
	
	CalculateAngle();
}

//----------------------------------------------------------------------
// TraceCreature
//----------------------------------------------------------------------
bool
MHomingEffect::TraceCreature()
{
	int x, y, z;
	if (!TraceCreaturePosition(x, y, z)) return false;

	m_TargetX = x;
	m_TargetY = y;

	return true;
}

//----------------------------------------------------------------------
// CalculateAngle
//----------------------------------------------------------------------
void
MHomingEffect::CalculateAngle()
{
	m_Steering.TurnToward(MEffect::GetPixelX(), MEffect::GetPixelY(),
		m_TargetX, m_TargetY);
}

//----------------------------------------------------------------------
// SetDirectionByAngle
//----------------------------------------------------------------------
void
MHomingEffect::SetDirectionByAngle()
{
	const int angle = m_Steering.GetAngle();
	if (angle < MathTable::ANGLE_180)
	{
		if (angle < MathTable::ANGLE_90)
		{
			if (angle < MathTable::ANGLE_30)
			{
				m_Direction = DIRECTION_RIGHT;
			}
			else if (angle < MathTable::ANGLE_60)
			{
				m_Direction = DIRECTION_RIGHTUP;
			}
			else
			{
				m_Direction = DIRECTION_UP;
			}
		}
		else
		{
			if (angle < MathTable::ANGLE_120)
			{
				m_Direction = DIRECTION_UP;
			}
			else if (angle < MathTable::ANGLE_150)
			{
				m_Direction = DIRECTION_LEFTUP;
			}
			else
			{
				m_Direction = DIRECTION_LEFT;
			}
		}
	}
	else
	{
		if (angle < MathTable::ANGLE_270)
		{
			if (angle < MathTable::ANGLE_210)
			{
				m_Direction = DIRECTION_LEFT;
			}
			else if (angle < MathTable::ANGLE_240)
			{
				m_Direction = DIRECTION_LEFTDOWN;
			}
			else
			{
				m_Direction = DIRECTION_DOWN;
			}
		}
		else
		{
			if (angle < MathTable::ANGLE_300)
			{
				m_Direction = DIRECTION_DOWN;
			}
			else if (angle < MathTable::ANGLE_330)
			{
				m_Direction = DIRECTION_RIGHTDOWN;
			}
			else
			{
				m_Direction = DIRECTION_RIGHT;
			}
		}
	}
}

//----------------------------------------------------------------------
// Update
//----------------------------------------------------------------------
bool
MHomingEffect::Update()
{	
	if (!IsEnd())
	{
		if (GetActionInfo() != SKILL_CLIENT_HALO_ATTACK )
		{ 
			if(!TraceCreature())
				return false;

			CalculateAngle();
		}

		//--------------------------------
		// Pixel 좌표를 바꾼다.
		//--------------------------------
		// 각각의 방향에 대해서 Step만큼 이동해준다.
		const POINT step = m_Steering.Advance(m_StepPixel);
		m_PixelX += step.x;
		m_PixelY += step.y;
		m_PixelZ += m_StepZ;

		if (fabs(m_PixelZ-m_TargetZ) < fabs(m_StepZ))
		{
			m_PixelZ = static_cast<float>(m_TargetZ);
			m_StepZ = 0;
		}

		//------------------------------------------
		// 다 움직인 경우를 생각해봐야 한다.
		//------------------------------------------
		if (fabs(m_PixelX-m_TargetX)<m_StepPixel &&
			fabs(m_PixelY-m_TargetY)<m_StepPixel &&
			GetActionInfo() != SKILL_CLIENT_HALO_ATTACK 
			)		// z는 무시 - -;
		{
			m_PixelX = (float)m_TargetX;
			m_PixelY = (float)m_TargetY;
			m_PixelZ = (float)m_TargetZ;

			m_StepX = 0;
			m_StepY = 0;
			m_StepZ = 0;

			m_Steering.StopTurning();

			//------------------------------------------
			// 더 움직일 필요가 없는 경우이다.			
			//------------------------------------------
			m_EndFrame = 0;

			return false;
		}
		else
		{
			//--------------------------------
			// 방향을 다시 설정해준다.
			//--------------------------------
		//	SetDirectionByAngle();
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
