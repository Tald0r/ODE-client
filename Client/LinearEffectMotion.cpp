#include "Client_PCH.h"
#include "LinearEffectMotion.h"
#include <cmath>

void LinearEffectMotion::SetLinearTarget(float pixelX, float pixelY, float pixelZ,
	int targetX, int targetY, int targetZ, WORD stepPixel)
{
	m_TargetX = targetX;
	m_TargetY = targetY;
	m_TargetZ = targetZ;
	float moveX = m_TargetX - pixelX;
	float moveY = m_TargetY - pixelY;
	float moveZ = m_TargetZ - pixelZ;
	m_Len = static_cast<float>(std::sqrt(moveX*moveX + moveY*moveY + moveZ*moveZ));
	if (m_Len == 0)
	{
		m_StepX = 0;
		m_StepY = 0;
		m_StepZ = 0;
	}
	else
	{
		m_StepX = moveX/m_Len * stepPixel;
		m_StepY = moveY/m_Len * stepPixel;
		m_StepZ = moveZ/m_Len * stepPixel;
	}
}

bool LinearEffectMotion::AdvanceLinear(float& pixelX, float& pixelY, float& pixelZ,
	WORD arrivalDistance)
{
	pixelX += m_StepX;
	pixelY += m_StepY;
	pixelZ += m_StepZ;
	if (std::fabs(pixelX - m_TargetX) < arrivalDistance &&
		std::fabs(pixelY - m_TargetY) < arrivalDistance &&
		std::fabs(pixelZ - m_TargetZ) < arrivalDistance)
	{
		pixelX = static_cast<float>(m_TargetX);
		pixelY = static_cast<float>(m_TargetY);
		pixelZ = static_cast<float>(m_TargetZ);
		m_StepX = 0;
		m_StepY = 0;
		m_StepZ = 0;
		return true;
	}
	return false;
}
